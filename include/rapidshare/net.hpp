#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace rapidshare {

constexpr std::uint32_t protocol_magic = 0x52534831;
constexpr std::uint32_t max_chunk_size = 4 * 1024 * 1024;

bool send_all(int socket, const void* data, std::size_t size);
bool receive_all(int socket, void* data, std::size_t size);
bool send_u32(int socket, std::uint32_t value);
bool send_u64(int socket, std::uint64_t value);
bool receive_u32(int socket, std::uint32_t& value);
bool receive_u64(int socket, std::uint64_t& value);
int connect_to(const std::string& host, const std::string& port);
int listen_on(const std::string& port);
std::string safe_filename(const std::string& name);

}

