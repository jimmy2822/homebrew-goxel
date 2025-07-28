# CentOS Stream 9 Test Environment for Goxel v14.0 Daemon
FROM quay.io/centos/centos:stream9

# Install EPEL and development tools
RUN dnf -y update && \
    dnf -y install epel-release && \
    dnf -y groupinstall "Development Tools" && \
    dnf -y install \
    python3-scons \
    pkgconfig \
    glfw-devel \
    gtk3-devel \
    libpng-devel \
    mesa-libOSMesa-devel \
    mesa-libGL-devel \
    mesa-libGLU-devel \
    python3 \
    python3-pip \
    git \
    curl \
    netcat \
    && dnf clean all

# Create test user
RUN useradd -m -s /bin/bash goxel-test

# Create working directory
WORKDIR /build

# Create test directories
RUN mkdir -p /tmp/goxel-test /var/run/goxel
RUN chown goxel-test:goxel-test /tmp/goxel-test /var/run/goxel

# Switch to test user
USER goxel-test

# Default command
CMD ["/bin/bash"]