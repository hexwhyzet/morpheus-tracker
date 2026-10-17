# Morpheus Tracker Tests

Test harness for measuring the quality of the CPU trackers (ORB, OBD).

## Layout

```
test/
├── CMakeLists.txt              # Native build with clang
├── configs/
│   └── trackers/               # Tracker configs (JSON)
│       ├── orb_default.json
│       └── orb_fast.json
├── datasets/                   # Test datasets
│   ├── README.md              # Format documentation
│   └── example_dataset/
│       ├── dataset.json
│       ├── templates/
│       └── sequences/
├── framework/                  # Test framework
│   ├── dataset_format.hpp/cpp # Dataset handling
│   └── tracker_config.hpp     # Tracker configuration
├── unit/                       # Unit tests
│   ├── test_utils.cpp
│   └── test_dataset_loading.cpp
└── integration/                # Integration (quality) tests
    └── test_trackers.cpp
```

## Requirements

- **clang++** (compiler)
- **OpenCV** (system install)
- **CMake** >= 3.14
- **nlohmann/json** (fetched automatically)
- **Google Test** (fetched automatically)

## Installing dependencies

### macOS

```bash
brew install cmake opencv
```

### Linux (Ubuntu/Debian)

```bash
sudo apt-get install cmake clang libopencv-dev
```

## Building

```bash
cd test
mkdir build && cd build

# Configure
cmake ..

# Build
make -j$(nproc)
```

### Build options

```bash
# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release build (default)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Pick a different compiler
cmake -DCMAKE_CXX_COMPILER=clang++-15 ..
```

## Running the tests

### Unit tests

```bash
# Utility tests (validation, homography quality)
./test_utils

# Dataset loading tests
./test_dataset_loading
```

### Integration tests (tracker quality)

```bash
# Run all tracker tests
./test_trackers

# Run a single test
./test_trackers --gtest_filter=TrackerTest.ORBTracker_BasicTracking

# List all tests
./test_trackers --gtest_list_tests
```

## Dataset format

See `datasets/README.md` for the full documentation.

Short version:

```
datasets/<dataset_name>/
├── dataset.json          # Dataset description
├── templates/            # Tracking templates
│   └── t0.png
└── sequences/
    └── seq_000/
        ├── frames/       # Sequence frames
        │   ├── frame_0000.png
        │   └── ...
        └── gt.json       # Ground truth
```

## Adding your own datasets

1. Create the directory layout following `example_dataset`
2. Put the template images into `templates/`
3. Put the sequence frames into `sequences/seq_XXX/frames/`
4. Write a `dataset.json` describing the dataset
5. Write a `gt.json` with the ground truth for every sequence
6. The tests pick up the new dataset automatically

## Quality metrics

The harness measures:

- **Success Rate** - percentage of successfully tracked frames
- **Corner Error** - mean corner position error (in pixels) when ground truth is available
- **Tracking Stability** - length of the run of consecutive successful frames

## Sample output

```
[ RUN      ] TrackerTest.ORBTracker_BasicTracking
Frame 0 corner error: 2.35 px
Frame 1 corner error: 3.12 px
Frame 2 corner error: 4.56 px
Frame 3 corner error: 2.89 px
Frame 4 corner error: 3.45 px
Success rate: 100%
[       OK ] TrackerTest.ORBTracker_BasicTracking (245 ms)
```

## Comparing trackers and parameters

Implemented so far:
- ✅ Basic ORB and OBD testing
- ✅ Dataset and ground truth loading
- ✅ Corner error computation

TODO:
- [ ] Load tracker parameters from configs
- [ ] Automatic comparison of several configurations
- [ ] Report generation (CSV/Markdown tables)
- [ ] Quality plots

## Development

### Adding new tests

1. Create a `test/unit/test_*.cpp` or `test/integration/test_*.cpp` file
2. Use the Google Test macros (`TEST`, `EXPECT_*`, `ASSERT_*`)
3. Rebuild the project

### Debugging

```bash
# Debug build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Run under gdb
gdb ./test_trackers
```

## Troubleshooting

### "OpenCV not found"

Make sure OpenCV is installed:

```bash
# macOS
brew install opencv

# Linux
sudo apt-get install libopencv-dev
```

### "clang++ not found"

Install clang or switch the compiler:

```bash
cmake -DCMAKE_CXX_COMPILER=g++ ..
```

### "Dataset not found"

Check the dataset path. The tests resolve datasets relative to `build/`:

```bash
# From build/ the path must be:
../datasets/example_dataset/dataset.json
```
