#include "rapidshare/checksum.hpp"
#include "rapidshare/net.hpp"

#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

std::mutex output_mutex;

void log_line(const std::string& text) {
    std::lock_guard<std::mutex> lock(output_mutex);
    std::cout << text << '\n';
}

void handle_client(int socket_fd, fs::path storage) {
    try {
        std::uint32_t magic = 0;
        std::uint32_t name_size = 0;
        std::uint64_t file_size = 0;
        std::uint64_t expected_checksum = 0;
        if (!rapidshare::receive_u32(socket_fd, magic)
            || !rapidshare::receive_u32(socket_fd, name_size)
            || !rapidshare::receive_u64(socket_fd, file_size)
            || !rapidshare::receive_u64(socket_fd, expected_checksum)) {
            throw std::runtime_error("incomplete transfer header");
        }
        if (magic != rapidshare::protocol_magic || name_size == 0 || name_size > 255) throw std::runtime_error("invalid transfer header");

        std::string raw_name(name_size, '\0');
        if (!rapidshare::receive_all(socket_fd, raw_name.data(), raw_name.size())) throw std::runtime_error("missing file name");
        const auto filename = rapidshare::safe_filename(raw_name);
        if (filename.empty() || filename == "." || filename == "..") throw std::runtime_error("invalid file name");

        const fs::path final_path = storage / filename;
        const fs::path partial_path = storage / (filename + ".part");
        std::uint64_t offset = fs::exists(partial_path) ? fs::file_size(partial_path) : 0;
        if (offset > file_size) {
            fs::resize_file(partial_path, 0);
            offset = 0;
        }
        if (!rapidshare::send_u64(socket_fd, offset)) throw std::runtime_error("could not send resume position");

        std::ofstream output(partial_path, std::ios::binary | std::ios::app);
        if (!output) throw std::runtime_error("could not create output file");
        std::uint64_t received_bytes = offset;
        while (true) {
            std::uint32_t size = 0;
            if (!rapidshare::receive_u32(socket_fd, size)) throw std::runtime_error("client disconnected");
            if (size == 0) break;
            if (size > rapidshare::max_chunk_size || received_bytes + size > file_size) throw std::runtime_error("invalid chunk");
            std::vector<char> data(size);
            if (!rapidshare::receive_all(socket_fd, data.data(), data.size())) throw std::runtime_error("incomplete chunk");
            output.write(data.data(), static_cast<std::streamsize>(data.size()));
            if (!output) throw std::runtime_error("failed to write file");
            received_bytes += size;
        }
        output.close();

        const bool valid = received_bytes == file_size && rapidshare::checksum_file(partial_path) == expected_checksum;
        if (valid) {
            if (fs::exists(final_path)) fs::remove(final_path);
            fs::rename(partial_path, final_path);
            log_line("Received " + filename + " (verified)");
        } else {
            log_line("Verification failed for " + filename);
        }
        rapidshare::send_u32(socket_fd, valid ? 1 : 0);
    } catch (const std::exception& error) {
        log_line(std::string("Client error: ") + error.what());
    }
    ::close(socket_fd);
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: rapidshare_server <port> [storage_directory]\n";
        return 1;
    }
    std::signal(SIGPIPE, SIG_IGN);
    const fs::path storage = argc == 3 ? argv[2] : "uploads";
    try {
        fs::create_directories(storage);
        const int server_fd = rapidshare::listen_on(argv[1]);
        if (server_fd < 0) throw std::runtime_error("could not listen on port");
        std::cout << "Listening on port " << argv[1] << "\nSaving files to " << fs::absolute(storage) << '\n';
        while (true) {
            const int client_fd = ::accept(server_fd, nullptr, nullptr);
            if (client_fd < 0) continue;
            std::thread(handle_client, client_fd, storage).detach();
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}

