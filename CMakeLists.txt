cmake_minimum_required(VERSION 3.15)
project(besu_native_evm VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -march=native -ffast-math)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    endif()
elseif(MSVC)
    add_compile_options(/W4)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /arch:AVX2)
    endif()
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files - Panama FFM architecture (single file EVM)
set(SOURCES
    src/evm_optimized.cpp
)

# Build shared library for Panama FFM
add_library(besu_native_evm SHARED ${SOURCES})

target_include_directories(besu_native_evm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Platform-specific settings
if(APPLE)
    set_target_properties(besu_native_evm PROPERTIES
        OUTPUT_NAME "besu_native_evm"
        SUFFIX ".dylib"
        INSTALL_RPATH "@loader_path"
    )
elseif(UNIX)
    set_target_properties(besu_native_evm PROPERTIES
        OUTPUT_NAME "besu_native_evm"
        SUFFIX ".so"
        INSTALL_RPATH "$ORIGIN"
    )
    target_link_libraries(besu_native_evm PRIVATE pthread)
elseif(WIN32)
    set_target_properties(besu_native_evm PROPERTIES
        OUTPUT_NAME "besu_native_evm"
        SUFFIX ".dll"
    )
endif()

# Position Independent Code for shared library
set_target_properties(besu_native_evm PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# Installation
install(TARGETS besu_native_evm
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Copy to Besu project for convenience
set(BESU_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../besu" CACHE PATH "Path to Besu project")
if(EXISTS "${BESU_PATH}")
    add_custom_command(TARGET besu_native_evm POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:besu_native_evm>
            ${BESU_PATH}/evm/src/test/resources/
        COMMENT "Copying library to Besu test resources"
    )
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "=== Besu Native EVM Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Architecture: Panama FFM (single-file EVM)")
message(STATUS "Source files:")
message(STATUS "  - src/evm_optimized.cpp")
message(STATUS "Headers:")
message(STATUS "  - include/message_frame_memory.h")
message(STATUS "  - include/storage_memory.h")
message(STATUS "  - include/account_witness.h")
message(STATUS "  - include/tracer_callback.h")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
if(EXISTS "${BESU_PATH}")
    message(STATUS "Besu path: ${BESU_PATH}")
    message(STATUS "Auto-copy to Besu: enabled")
else()
    message(STATUS "Besu path: not found (set -DBESU_PATH=<path> to enable auto-copy)")
endif()
message(STATUS "=====================================")
message(STATUS "")
