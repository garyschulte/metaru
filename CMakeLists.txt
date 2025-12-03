cmake_minimum_required(VERSION 3.15)
project(besu_native_evm VERSION 0.1.0 LANGUAGES CXX)

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
        add_compile_options(-O3 -march=native)
    endif()
elseif(MSVC)
    add_compile_options(/W4)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2)
    endif()
endif()

# Find JNI
find_package(JNI REQUIRED)
if(JNI_FOUND)
    message(STATUS "JNI found:")
    message(STATUS "  JNI_INCLUDE_DIRS: ${JNI_INCLUDE_DIRS}")
    message(STATUS "  JNI_LIBRARIES: ${JNI_LIBRARIES}")
else()
    message(FATAL_ERROR "JNI not found. Please install JDK.")
endif()

include_directories(${JNI_INCLUDE_DIRS})

# Find Java for header generation
find_package(Java REQUIRED)
message(STATUS "Java found: ${Java_JAVA_EXECUTABLE}")

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    # JNI bridge
    src/jni/jni_init.cpp
    src/jni/native_evm_jni.cpp
    src/jni/jni_helpers.cpp
    src/jni/message_frame_jni.cpp
    src/jni/operation_tracer_jni.cpp

    # EVM core
    src/evm/evm.cpp
    src/evm/operation_registry.cpp

    # Operations
    src/operations/arithmetic.cpp
    src/operations/comparison.cpp
    src/operations/bitwise.cpp
    src/operations/stack.cpp
    src/operations/memory.cpp
    src/operations/storage.cpp
    src/operations/control.cpp
    src/operations/calls.cpp
    src/operations/create.cpp
    src/operations/system.cpp

    # Types
    src/types/address.cpp
    src/types/wei.cpp
    src/types/bytes.cpp
    src/types/uint256.cpp
    src/types/hash.cpp

    # State
    src/state/world_state.cpp
    src/state/memory.cpp
    src/state/stack.cpp
)

# Build shared library for JNI
add_library(besu_native_evm SHARED ${SOURCES})

target_include_directories(besu_native_evm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(besu_native_evm
    PRIVATE
        ${JNI_LIBRARIES}
)

# Platform-specific settings
if(APPLE)
    set_target_properties(besu_native_evm PROPERTIES
        SUFFIX ".dylib"
        INSTALL_RPATH "@loader_path"
    )
elseif(UNIX)
    set_target_properties(besu_native_evm PROPERTIES
        SUFFIX ".so"
        INSTALL_RPATH "$ORIGIN"
    )
    target_link_libraries(besu_native_evm PRIVATE pthread)
elseif(WIN32)
    set_target_properties(besu_native_evm PROPERTIES
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

# Testing
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()

    # Try to find Google Test
    find_package(GTest QUIET)

    if(GTest_FOUND)
        message(STATUS "Google Test found, building tests")

        set(TEST_SOURCES
            test/cpp/test_types.cpp
            test/cpp/test_operations.cpp
            test/cpp/test_evm.cpp
        )

        add_executable(besu_native_evm_test ${TEST_SOURCES})

        target_link_libraries(besu_native_evm_test
            PRIVATE
                besu_native_evm
                GTest::GTest
                GTest::Main
        )

        target_include_directories(besu_native_evm_test
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/include
        )

        add_test(NAME besu_native_evm_test COMMAND besu_native_evm_test)
    else()
        message(STATUS "Google Test not found, tests will not be built")
        message(STATUS "To build tests, install Google Test:")
        message(STATUS "  Ubuntu/Debian: sudo apt-get install libgtest-dev")
        message(STATUS "  macOS: brew install googletest")
        message(STATUS "  Or build from source: https://github.com/google/googletest")
    endif()
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "=== Besu Native EVM Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "JNI include dirs: ${JNI_INCLUDE_DIRS}")
message(STATUS "Java executable: ${Java_JAVA_EXECUTABLE}")
message(STATUS "Build testing: ${BUILD_TESTING}")
if(BUILD_TESTING AND GTest_FOUND)
    message(STATUS "Google Test: found")
else()
    message(STATUS "Google Test: not found")
endif()
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "=====================================")
message(STATUS "")
