#!/bin/bash

# Alternative: Install OSMesa using Conda/Mamba
# This is often easier than building from source

echo "Installing OSMesa using Conda..."

# Check if conda is installed
if ! command -v conda &> /dev/null; then
    echo "Conda not found. Installing Miniforge..."
    curl -L -O "https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-MacOSX-arm64.sh"
    bash Miniforge3-MacOSX-arm64.sh -b -p "$HOME/miniforge3"
    eval "$($HOME/miniforge3/bin/conda shell.bash hook)"
fi

# Create environment for OSMesa
conda create -n osmesa -y
conda activate osmesa

# Install OSMesa from conda-forge
conda install -c conda-forge mesa-libgl-cos7-x86_64 -y
# or try:
# conda install -c conda-forge mesalib -y

echo "OSMesa installed via Conda"
echo "To use it, activate the environment: conda activate osmesa"