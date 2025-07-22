# Makefile for Goxel Headless C API Examples
# 
# This Makefile builds examples that use the Goxel Headless C API

# Paths
GOXEL_ROOT = ../..
INCLUDE_DIR = $(GOXEL_ROOT)/include
LIB_DIR = $(GOXEL_ROOT)/build/lib

# Compiler settings
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -I$(INCLUDE_DIR)
LDFLAGS = -L$(LIB_DIR) -lgoxel-headless -lpthread -lm

# Build settings
ifeq ($(MODE),debug)
    CFLAGS += -g -O0 -DDEBUG
else
    CFLAGS += -O2 -DNDEBUG
endif

# Targets
EXAMPLES = simple_example
BINARIES = $(EXAMPLES:%=build/%)

.PHONY: all clean install check

all: build $(BINARIES)

# Create build directory
build:
	mkdir -p build

# Build examples
build/%: %.c | build
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Clean build artifacts
clean:
	rm -rf build
	rm -f *.gox *.png *.jpg

# Install library (if not already built)
install:
	@echo "Building Goxel C API library..."
	cd $(GOXEL_ROOT) && scons c_api=1

# Check that library exists
check:
	@if [ ! -f "$(LIB_DIR)/libgoxel-headless.dylib" ] && [ ! -f "$(LIB_DIR)/libgoxel-headless.so" ]; then \
		echo "Error: Goxel C API library not found."; \
		echo "Run 'make install' first to build the library."; \
		exit 1; \
	fi

# Help target
help:
	@echo "Goxel Headless C API Examples"
	@echo "============================="
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build all examples (default)"
	@echo "  clean    - Remove build artifacts"
	@echo "  install  - Build the Goxel C API library"
	@echo "  check    - Check if library is available"
	@echo "  help     - Show this help"
	@echo ""
	@echo "Usage:"
	@echo "  1. make install    # Build the C API library"
	@echo "  2. make all        # Build examples"
	@echo "  3. ./build/simple_example  # Run example"
	@echo ""
	@echo "Environment variables:"
	@echo "  MODE=debug    - Build with debug information"