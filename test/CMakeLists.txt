cmake_minimum_required(VERSION 3.15)
project(morpheus_tests CXX)

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

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs features2d video calib3d highgui)
find_package(nlohmann_json REQUIRED)
find_package(GTest REQUIRED)

# ============================================================================
# Include directories
# ============================================================================

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src
)

# ============================================================================
# Tracker library (CPU code only)
# ============================================================================

add_library(tracker_lib STATIC
    ../core/src/image_tracking/ORB.cpp
    ../core/src/image_tracking/OBD.cpp
    ../core/src/image_tracking/ShiTomasi.cpp
    ../core/src/image_tracking/common/utils.cpp
    ../core/src/debug/drawer.cpp
)

target_link_libraries(tracker_lib PUBLIC opencv::opencv)
target_include_directories(tracker_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src
)

# ============================================================================
# Test framework (dataset loading)
# ============================================================================

add_library(test_framework STATIC
    framework/dataset_format.cpp
    framework/tracker_config.cpp
)

target_link_libraries(test_framework PUBLIC
    tracker_lib
    opencv::opencv
    nlohmann_json::nlohmann_json
)

target_include_directories(test_framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
)

# ============================================================================
# Unit tests
# ============================================================================

include(GoogleTest)

add_executable(test_utils
    unit/test_utils.cpp
)
target_link_libraries(test_utils
    tracker_lib
    GTest::gtest_main
    opencv::opencv
)
gtest_discover_tests(test_utils WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/unit)

add_executable(test_dataset_loading
    unit/test_dataset_loading.cpp
)
target_link_libraries(test_dataset_loading
    test_framework
    GTest::gtest_main
)
gtest_discover_tests(test_dataset_loading WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/unit)

# ============================================================================
# Integration tests
# ============================================================================

add_executable(test_trackers
    integration/test_trackers.cpp
)
target_link_libraries(test_trackers
    test_framework
    tracker_lib
    GTest::gtest_main
    opencv::opencv
)
gtest_discover_tests(test_trackers WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/integration)

# ============================================================================
# Configuration summary
# ============================================================================

message(STATUS "")
message(STATUS "==============================================")
message(STATUS "Morpheus Tests Configuration")
message(STATUS "==============================================")
message(STATUS "Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler:     ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ Standard:     ${CMAKE_CXX_STANDARD}")
message(STATUS "OpenCV version:   ${OpenCV_VERSION}")
message(STATUS "==============================================")
message(STATUS "")
