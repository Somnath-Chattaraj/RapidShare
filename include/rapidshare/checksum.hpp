#pragma once

#include <cstdint>
#include <filesystem>

namespace rapidshare {

std::uint64_t checksum_file(const std::filesystem::path& path);

}

