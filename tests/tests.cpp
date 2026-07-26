#include "rapidshare/checksum.hpp"
#include "rapidshare/net.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
    if (rapidshare::safe_filename("../../notes.txt") != "notes.txt") return 1;
    if (rapidshare::safe_filename("folder/photo.png") != "photo.png") return 1;

    const auto path = std::filesystem::temp_directory_path() / "rapidshare_checksum_test.txt";
    {
        std::ofstream output(path, std::ios::binary);
        output << "rapidshare";
    }
    const auto first = rapidshare::checksum_file(path);
    const auto second = rapidshare::checksum_file(path);
    std::filesystem::remove(path);
    if (first == 0 || first != second) return 1;

    std::cout << "All tests passed\n";
}

