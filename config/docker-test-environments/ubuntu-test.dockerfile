# Ubuntu 22.04 Test Environment for Goxel v14.0 Daemon
FROM ubuntu:22.04

# Set non-interactive frontend to avoid prompts
ENV DEBIAN_FRONTEND=noninteractive

# Update package list and install dependencies
RUN apt-get update && apt-get install -y \
    scons \
    pkg-config \
    build-essential \
    libglfw3-dev \
    libgtk-3-dev \
    libpng-dev \
    libosmesa6-dev \
    mesa-utils \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    python3 \
    python3-pip \
    git \
    curl \
    netcat-openbsd \
    && rm -rf /var/lib/apt/lists/*

# Create test user
RUN useradd -m -s /bin/bash goxel-test

# Create working directory
WORKDIR /build

# Copy source code (will be mounted in testing)
# COPY src/ ./src/
# COPY SConstruct ./
# COPY Makefile ./

# Create test directories
RUN mkdir -p /tmp/goxel-test /var/run/goxel
RUN chown goxel-test:goxel-test /tmp/goxel-test /var/run/goxel

# Switch to test user
USER goxel-test

# Set environment variables
ENV PATH="/home/goxel-test/.local/bin:$PATH"

# Default command
CMD ["/bin/bash"]