cmake_minimum_required(VERSION 3.10)

project(MiniDB)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(minidb
    src/main.cpp
    src/disk_manager.cpp
    src/slotted_page.cpp
    src/free_space_map.cpp
    src/lru_k_replace.cpp
    src/buffer_pool_manager.cpp
)