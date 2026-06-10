#include "rapidshare/blocking_queue.hpp"
#include "rapidshare/checksum.hpp"
#include "rapidshare/net.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

struct Chunk {
    std::vector<char> data;
    bool last = false;
};

int main(int argc, char** argv) {
    if (argc < 4 || argc > 5) {
        std::cerr << "Usage: rapidshare_client <host> <port> <file> [chunk_kb]\n";
        return 1;
    }

    std::signal(SIGPIPE, SIG_IGN);
    try {
        const fs::path path = argv[3];
        if (!fs::is_regular_file(path)) throw std::runtime_error("file does not exist");
        const auto file_size = fs::file_size(path);
        const std::size_t chunk_size = argc == 5 ? std::stoul(argv[4]) * 1024 : 256 * 1024;
        if (chunk_size == 0 || chunk_size > rapidshare::max_chunk_size) throw std::runtime_error("chunk size must be between 1 KB and 4096 KB");

        std::cout << "Calculating checksum...\n";
        const auto checksum = rapidshare::checksum_file(path);
        const int socket_fd = rapidshare::connect_to(argv[1], argv[2]);
        if (socket_fd < 0) throw std::runtime_error("could not connect to server");

        const auto filename = path.filename().string();
        bool header_ok = rapidshare::send_u32(socket_fd, rapidshare::protocol_magic)
            && rapidshare::send_u32(socket_fd, static_cast<std::uint32_t>(filename.size()))
            && rapidshare::send_u64(socket_fd, file_size)
            && rapidshare::send_u64(socket_fd, checksum)
            && rapidshare::send_all(socket_fd, filename.data(), filename.size());
        if (!header_ok) throw std::runtime_error("failed to send transfer details");

        std::uint64_t offset = 0;
        if (!rapidshare::receive_u64(socket_fd, offset) || offset > file_size) throw std::runtime_error("invalid response from server");

        rapidshare::BlockingQueue<Chunk> queue(8);
        std::atomic<bool> read_failed{false};
        std::thread reader([&] {
            std::ifstream input(path, std::ios::binary);
            input.seekg(static_cast<std::streamoff>(offset));
            while (input) {
                Chunk chunk;
                chunk.data.resize(chunk_size);
                input.read(chunk.data.data(), static_cast<std::streamsize>(chunk.data.size()));
                chunk.data.resize(static_cast<std::size_t>(input.gcount()));
                if (chunk.data.empty()) break;
                queue.push(std::move(chunk));
            }
            if (input.bad()) read_failed = true;
            queue.push(Chunk{{}, true});
        });

        const auto started = std::chrono::steady_clock::now();
        std::uint64_t sent_bytes = offset;
        std::cout << (offset ? "Resuming" : "Uploading") << " " << filename << "\n";
        bool transfer_ok = true;
        while (true) {
            auto chunk = queue.pop();
            if (chunk.last) break;
            if (!rapidshare::send_u32(socket_fd, static_cast<std::uint32_t>(chunk.data.size()))
                || !rapidshare::send_all(socket_fd, chunk.data.data(), chunk.data.size())) {
                transfer_ok = false;
                break;
            }
            sent_bytes += chunk.data.size();
            const double progress = file_size == 0 ? 100.0 : 100.0 * sent_bytes / file_size;
            std::cout << "\rProgress: " << std::fixed << std::setprecision(1) << progress << "%" << std::flush;
        }
        reader.join();
        if (!transfer_ok || read_failed || !rapidshare::send_u32(socket_fd, 0)) throw std::runtime_error("transfer interrupted");

        std::uint32_t status = 0;
        if (!rapidshare::receive_u32(socket_fd, status)) throw std::runtime_error("server disconnected before verification");
        ::close(socket_fd);

        const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        const double transferred_mb = static_cast<double>(file_size - offset) / (1024.0 * 1024.0);
        std::cout << "\nSpeed: " << std::setprecision(2) << transferred_mb / std::max(elapsed, 0.001) << " MB/s\n";
        if (status != 1) throw std::runtime_error("server checksum verification failed");
        std::cout << "Transfer completed and verified\n";
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
