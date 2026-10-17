cmake_minimum_required(VERSION 3.15)
project(morpheus_benchmark CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# ============================================================================
# Dependencies (via Conan)
# ============================================================================

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs features2d video calib3d videoio)
find_package(nlohmann_json REQUIRED)

# ============================================================================
# Tracker library (CPU)
# ============================================================================

add_library(benchmark_tracker_lib STATIC
    ../core/src/image_tracking/ORB.cpp
    ../core/src/image_tracking/OBD.cpp
    ../core/src/image_tracking/ShiTomasi.cpp
    ../core/src/image_tracking/common/utils.cpp
    ../core/src/debug/drawer.cpp
)

target_link_libraries(benchmark_tracker_lib PUBLIC opencv::opencv)
target_include_directories(benchmark_tracker_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src
)

# ============================================================================
# Test framework (dataset loading, tracker config)
# ============================================================================

add_library(benchmark_test_framework STATIC
    ../test/framework/dataset_format.cpp
    ../test/framework/tracker_config.cpp
)

target_link_libraries(benchmark_test_framework PUBLIC
    benchmark_tracker_lib
    opencv::opencv
    nlohmann_json::nlohmann_json
)

target_include_directories(benchmark_test_framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../test
)

# ============================================================================
# POT-280 benchmark
# ============================================================================

add_executable(pot280_benchmark
    pot280/run.cpp
)

target_link_libraries(pot280_benchmark PRIVATE
    benchmark_tracker_lib
    benchmark_test_framework
    opencv::opencv
    nlohmann_json::nlohmann_json
)

target_include_directories(pot280_benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../test
)

# ============================================================================
# Frame renderer
# ============================================================================

add_executable(pot280_render
    pot280/render_frames.cpp
)

target_link_libraries(pot280_render PRIVATE
    benchmark_tracker_lib
    benchmark_test_framework
    opencv::opencv
    nlohmann_json::nlohmann_json
)

target_include_directories(pot280_render PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../test
)

# ============================================================================
# Configuration summary
# ============================================================================

message(STATUS "")
message(STATUS "==============================================")
message(STATUS "Morpheus Benchmark Configuration")
message(STATUS "==============================================")
message(STATUS "Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler:     ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ Standard:     ${CMAKE_CXX_STANDARD}")
message(STATUS "OpenCV version:   ${OpenCV_VERSION}")
message(STATUS "==============================================")
message(STATUS "")
