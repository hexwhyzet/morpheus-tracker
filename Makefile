TARGET    ?= emscripten-release
BUILD_DIR  = core/build/$(TARGET)
JOBS      ?= $(shell nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)

CONAN_PROFILE = core/profiles/$(TARGET)

.PHONY: deps cmake build wasm test benchmark clean help

## Install dependencies (the first run builds OpenCV, which takes a while)
deps:
	conan install core -pr:h=$(CONAN_PROFILE) -pr:b=core/profiles/native \
		--output-folder=$(BUILD_DIR) --build=missing

BUILD_TYPE     = $(shell grep 'build_type=' $(CONAN_PROFILE) core/profiles/emscripten-base 2>/dev/null | tail -1 | cut -d= -f2)
BUILD_TYPE     := $(if $(BUILD_TYPE),$(BUILD_TYPE),Release)
# Must be absolute: cmake resolves a relative path against -S core, not the repo root.
TOOLCHAIN_FILE = $(CURDIR)/$(BUILD_DIR)/build/$(BUILD_TYPE)/generators/conan_toolchain.cmake

CMAKE_ARGS ?=

cmake:
	cmake -S core -B $(BUILD_DIR) \
		-DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN_FILE) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		$(CMAKE_ARGS)

build:
	cmake --build $(BUILD_DIR) -- -j$(JOBS)

## Full WASM build from scratch: dependencies + configure + build.
## Artifacts land in js/src/wasm/
wasm:
	$(MAKE) deps cmake build

## Native tests
test:
	$(MAKE) deps cmake build TARGET=native
	ctest --test-dir core/build/native -V --timeout 120

## Accuracy benchmark (POT-280)
benchmark:
	$(MAKE) deps TARGET=native
	$(MAKE) cmake TARGET=native CMAKE_ARGS=-DBUILD_BENCHMARK=ON
	$(MAKE) build TARGET=native

clean:
	rm -rf core/build

help:
	@echo ""
	@echo "Usage: make <target> [TARGET=<profile>] [JOBS=N]"
	@echo ""
	@echo "  wasm       Full WASM build → js/src/wasm/ (default)"
	@echo "  deps       Conan dependencies only"
	@echo "  cmake      Configure only"
	@echo "  build      Build only"
	@echo "  test       Native tests (gtest)"
	@echo "  benchmark  Build the accuracy benchmark"
	@echo "  clean      Remove core/build"
	@echo ""
	@echo "Profiles: emscripten-release (default), emscripten-debug, native"
	@echo ""
