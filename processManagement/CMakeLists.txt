cmake_minimum_required(VERSION 3.16)

project(process_management LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "process_management is a Linux-only example.")
endif()

find_package(Threads REQUIRED)

add_executable(process_management
    src/main.cpp
    src/resource_pool.cpp
    src/server_app.cpp
    src/session_transport.cpp
    src/service.cpp
)

target_include_directories(process_management
    PRIVATE
        include
)

target_compile_features(process_management PRIVATE cxx_std_17)
target_link_libraries(process_management PRIVATE Threads::Threads)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(process_management PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()
