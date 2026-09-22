cmake_minimum_required(VERSION 3.16)

project(edge_vision
    VERSION 1.0.0
    LANGUAGES CXX
)

# =============================================================================
# C++ Settings
# =============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Build Type
# =============================================================================

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# =============================================================================
# Output Directory
# =============================================================================

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# =============================================================================
# Dependencies
# =============================================================================

find_package(OpenCV REQUIRED COMPONENTS
    core
    imgproc
    imgcodecs
    highgui
    videoio
    dnn
)

find_package(Threads REQUIRED)

# =============================================================================
# Paths
# =============================================================================

set(MODEL_PATH
    "${CMAKE_SOURCE_DIR}/models/yolo26n.onnx"
)

set(TEST_IMAGE_PATH
    "${CMAKE_SOURCE_DIR}/test/bus.jpg"
)

set(BOOT_ID_PATH
    "${CMAKE_SOURCE_DIR}/boot_id.dat"
)

# =============================================================================
# Sources
# =============================================================================

set(SOURCES
    src/main.cpp
    src/camera.cpp
    src/preprocessor.cpp
    src/detector.cpp
    src/postprocessor.cpp
    src/serializer.cpp
    src/tcp_client.cpp
    src/metrics.cpp
    src/ack.cpp
)

# =============================================================================
# Executable
# =============================================================================

add_executable(edge_vision
    ${SOURCES}
)

# =============================================================================
# Include Directories
# =============================================================================

target_include_directories(edge_vision
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
)

# =============================================================================
# Libraries
# =============================================================================

target_link_libraries(edge_vision
    PRIVATE
        ${OpenCV_LIBS}
        Threads::Threads
)

# =============================================================================
# Compile Definitions
# =============================================================================

target_compile_definitions(edge_vision
    PRIVATE
        MODEL_PATH="${MODEL_PATH}"
        TEST_IMAGE_PATH="${TEST_IMAGE_PATH}"
        BOOT_ID_FILE_PATH="${BOOT_ID_PATH}"
)

# =============================================================================
# Compiler Warnings
# =============================================================================

target_compile_options(edge_vision
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

# =============================================================================
# Release Optimization
# =============================================================================

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(edge_vision
        PRIVATE
            -O3
    )
endif()

# =============================================================================
# Sanitizer
# Usage:
# cmake -B build -DENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
# =============================================================================

option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(ENABLE_SANITIZERS)
    target_compile_options(edge_vision
        PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
    )

    target_link_options(edge_vision
        PRIVATE
            -fsanitize=address,undefined
    )
endif()