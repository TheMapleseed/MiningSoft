#!/bin/bash

# Build script for Monero Miner on Apple Silicon
# This script optimizes the build for Apple Silicon architecture

set -e

echo "Building Monero Miner for Apple Silicon..."

# Create build directory
mkdir -p build
cd build

# Configure CMake with Apple Silicon optimizations
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_COMPILER=clang

# Build with maximum optimization
make -j$(sysctl -n hw.ncpu)

echo "Build completed successfully!"
echo "Executable: ./monero-miner"
echo ""
echo "Usage examples:"
echo "  ./monero-miner -c ../config.json"
echo "  ./monero-miner -p stratum+tcp://pool.monero.hashvault.pro:4444 -u your_wallet -w x"
echo "  ./monero-miner --help"
