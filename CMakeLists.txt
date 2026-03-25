cmake_minimum_required(VERSION 3.11)
project(MemoryDumper)

set(CMAKE_CXX_STANDARD 17)

add_executable(MemoryDumper
        main.cpp
        src/process_vm_reader.cpp
        src/proc_mem_reader.cpp
        src/memory_maps.cpp
)

target_include_directories(MemoryDumper PRIVATE include)

#add_subdirectory(test_binary)
