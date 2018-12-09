cmake_minimum_required(VERSION 2.6)

set(CMAKE_CXX_STANDARD 14)

project(PCM)
add_library(PCM
        pcm/msr.cpp
        pcm/cpucounters.cpp
        pcm/pci.cpp
        pcm/client_bw.cpp
        pcm/utils.cpp)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} pcm)

project(dirigent)
add_executable(dirigent
        main.cpp
        monitor.cpp
        process.cpp
        timer.cpp)

target_link_libraries(dirigent LINK_PUBLIC pthread rt PCM)

project(dirigent_test)
add_executable(dirigent_test test.cpp)
