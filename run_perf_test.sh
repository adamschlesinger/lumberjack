#!/bin/bash
set -e

echo "Building performance test..."
mkdir -p build
cd build
cmake -Dlumberjack_BUILD_TESTS=ON ..
cmake --build . --target perf_branching_comparison

echo ""
echo "Running performance benchmark..."
echo ""
./tests/perf_branching_comparison
