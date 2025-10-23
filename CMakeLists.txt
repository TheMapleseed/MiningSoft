cmake_minimum_required(VERSION 3.20)
project(MoneroMinerAppleSilicon VERSION 1.0.0 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set C standard to C23
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Apple Silicon specific optimizations for all generations
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    # Optimize for all Apple Silicon generations (M1, M2, M3, M4, M5)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=apple-a14 -mtune=apple-a14")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=apple-a14 -mtune=apple-a14")
    add_definitions(-DAPPLE_SILICON_OPTIMIZED)
    add_definitions(-DAPPLE_SILICON_UNIVERSAL)
endif()

# Compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -flto -fvectorize")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -flto -fvectorize")
endif()

# Find required packages
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# Metal framework for GPU acceleration
if(APPLE)
    find_library(METAL_FRAMEWORK Metal)
    find_library(METALPERFORMANCE_FRAMEWORK MetalPerformanceShaders)
    find_library(FOUNDATION_FRAMEWORK Foundation)
    find_library(IOKIT_FRAMEWORK IOKit)
endif()

# Include directories
include_directories(include)
include_directories(src)

# Source files
set(SOURCES
    src/main.cpp
    src/miner.cpp
    src/randomx_native.cpp
    src/metal_gpu.cpp
    src/cpu_throttle_manager.cpp
    src/pool_connection.cpp
    src/config_manager.cpp
    src/logger.cpp
    src/performance_monitor.cpp
    src/simple_json.cpp
)

# Header files
set(HEADERS
    include/miner.h
    include/randomx_native.h
    include/metal_gpu.h
    include/cpu_throttle_manager.h
    include/pool_connection.h
    include/config_manager.h
    include/logger.h
    include/performance_monitor.h
    include/simple_json.h
)

# Create executable
add_executable(monero-miner ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(monero-miner 
    Threads::Threads
    ${CMAKE_THREAD_LIBS_INIT}
)

# Apple-specific linking
if(APPLE)
    target_link_libraries(monero-miner
        ${METAL_FRAMEWORK}
        ${METALPERFORMANCE_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
    )
endif()

# Compiler definitions
target_compile_definitions(monero-miner PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# Installation
install(TARGETS monero-miner DESTINATION bin)

# Create build script
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/build.sh"
    @ONLY
)
