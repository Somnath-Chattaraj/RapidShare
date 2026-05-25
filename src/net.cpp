#include "rapidshare/net.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <netdb.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace rapidshare {

namespace {

std::uint64_t swap_u64(std::uint64_t value) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return __builtin_bswap64(value);
#else
    return value;
#endif
}

}

bool send_all(int socket, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const char*>(data);
    while (size > 0) {
        const auto sent = ::send(socket, bytes, size, 0);
        if (sent < 0 && errno == EINTR) continue;
        if (sent <= 0) return false;
        bytes += sent;
        size -= static_cast<std::size_t>(sent);
    }
    return true;
}

bool receive_all(int socket, void* data, std::size_t size) {
    auto* bytes = static_cast<char*>(data);
    while (size > 0) {
        const auto received = ::recv(socket, bytes, size, 0);
        if (received < 0 && errno == EINTR) continue;
        if (received <= 0) return false;
        bytes += received;
        size -= static_cast<std::size_t>(received);
    }
    return true;
}

bool send_u32(int socket, std::uint32_t value) {
    value = htonl(value);
    return send_all(socket, &value, sizeof(value));
}

bool send_u64(int socket, std::uint64_t value) {
    value = swap_u64(value);
    return send_all(socket, &value, sizeof(value));
}

bool receive_u32(int socket, std::uint32_t& value) {
    if (!receive_all(socket, &value, sizeof(value))) return false;
    value = ntohl(value);
    return true;
}

bool receive_u64(int socket, std::uint64_t& value) {
    if (!receive_all(socket, &value, sizeof(value))) return false;
    value = swap_u64(value);
    return true;
}

int connect_to(const std::string& host, const std::string& port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* results = nullptr;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &results) != 0) return -1;

    int socket_fd = -1;
    for (auto* item = results; item; item = item->ai_next) {
        socket_fd = ::socket(item->ai_family, item->ai_socktype, item->ai_protocol);
        if (socket_fd < 0) continue;
        if (::connect(socket_fd, item->ai_addr, item->ai_addrlen) == 0) break;
        ::close(socket_fd);
        socket_fd = -1;
    }
    freeaddrinfo(results);
    return socket_fd;
}

int listen_on(const std::string& port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    addrinfo* results = nullptr;
    if (getaddrinfo(nullptr, port.c_str(), &hints, &results) != 0) return -1;

    int socket_fd = -1;
    for (auto* item = results; item; item = item->ai_next) {
        socket_fd = ::socket(item->ai_family, item->ai_socktype, item->ai_protocol);
        if (socket_fd < 0) continue;
        int enabled = 1;
        setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
        if (::bind(socket_fd, item->ai_addr, item->ai_addrlen) == 0 && ::listen(socket_fd, 32) == 0) break;
        ::close(socket_fd);
        socket_fd = -1;
    }
    freeaddrinfo(results);
    return socket_fd;
}

std::string safe_filename(const std::string& name) {
    return std::filesystem::path(name).filename().string();
}

}

