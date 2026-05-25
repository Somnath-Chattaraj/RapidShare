cmake_minimum_required(VERSION 3.16)
project(rapidshare VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(rapidshare_common
    src/net.cpp
    src/checksum.cpp
)
target_include_directories(rapidshare_common PUBLIC include)
target_link_libraries(rapidshare_common PUBLIC Threads::Threads)

add_executable(rapidshare_server src/server.cpp)
target_link_libraries(rapidshare_server PRIVATE rapidshare_common)

add_executable(rapidshare_client src/client.cpp)
target_link_libraries(rapidshare_client PRIVATE rapidshare_common)

enable_testing()
add_executable(rapidshare_tests tests/tests.cpp)
target_link_libraries(rapidshare_tests PRIVATE rapidshare_common)
add_test(NAME rapidshare_tests COMMAND rapidshare_tests)

