# Test Makefile for Goxel Core API Tests

CC = gcc
CFLAGS = -std=gnu99 -Wall -g -O0 -I../src -I../ext_src
LIBS = -lm -ldl -pthread

# For headless testing, we don't need GUI dependencies
CORE_SOURCES = \
    ../src/core/goxel_core.c \
    ../src/core/project_mgmt.c \
    ../src/core/volume_ops.c \
    ../src/core/volume_utils.c \
    ../src/core/image.c \
    ../src/core/layer.c \
    ../src/core/material.c \
    ../src/core/shape.c \
    ../src/core/palette.c \
    ../src/core/file_format.c \
    ../src/core/utils/box.c \
    ../src/core/utils/cache.c \
    ../src/core/utils/color.c \
    ../src/core/utils/geometry.c \
    ../src/core/utils/img.c \
    ../src/core/utils/path.c \
    ../src/core/utils/vec.c \
    ../src/core/utils/json.c \
    ../src/xxhash.c

TEST_SOURCES = core/test_goxel_core.c

all: test_core

test_core: $(TEST_SOURCES) $(CORE_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(TEST_SOURCES) $(CORE_SOURCES) $(LIBS)

run-tests: test_core
	./test_core

clean:
	rm -f test_core

.PHONY: all run-tests clean