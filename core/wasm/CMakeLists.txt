cmake_minimum_required(VERSION 3.15)
project(morpheus_tracker_wasm)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# -pthread/-matomics/-mbulk-memory enable OpenCV threading (WITH_PTHREADS_PF)
# and shared linear memory. The same flags are set in profiles/emscripten-base so
# OpenCV is built against the same threading ABI - otherwise parallel_for_ ends up
# running on stub implementations.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -matomics -mbulk-memory")
set(CMAKE_CXX_FLAGS_RELEASE "-Wall -O3 -DNDEBUG")

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)
set(OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../js/src/wasm)

file(MAKE_DIRECTORY ${OUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR})

find_package(OpenCV REQUIRED)

include_directories(${SRC_DIR})

add_executable(img_tracker_wasm
    ${SRC_DIR}/img_tracker_wasm.cpp
    ${SRC_DIR}/esm.cpp
    ${SRC_DIR}/image_tracking/ORB.cpp
    ${SRC_DIR}/image_tracking/OBD.cpp
    ${SRC_DIR}/image_tracking/ShiTomasi.cpp
    ${SRC_DIR}/image_tracking/common/utils.cpp
    ${SRC_DIR}/debug/drawer.cpp
)

target_link_libraries(img_tracker_wasm opencv::opencv)

# ALLOW_MEMORY_GROWTH=0 - must stay 0: growing shared memory in wasm is expensive
#   and invalidates views already handed out over the SharedArrayBuffer.
# INITIAL_MEMORY=128MB - fixed budget, sized for tracking workloads.
# PTHREAD_POOL_SIZE=8 - the ceiling for cv::setNumThreads().
set_target_properties(img_tracker_wasm PROPERTIES LINK_FLAGS
    "-pthread \
     -s EXPORT_NAME='ImageTrackerWASM' \
     -s MODULARIZE=1 \
     -s EXPORT_ES6=1 \
     -s ALLOW_MEMORY_GROWTH=0 \
     -s PTHREAD_POOL_SIZE=8 \
     -s INITIAL_MEMORY=134217728 \
     -s EXPORTED_FUNCTIONS='[\"_malloc\", \"_free\"]' \
     -s EXPORTED_RUNTIME_METHODS='[\"cwrap\", \"getValue\", \"UTF8ToString\"]' \
     -s WASM=1 \
     -s MALLOC=dlmalloc"
)
