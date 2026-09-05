#!/bin/bash
# Build and run Litt Engine tests (Linux/macOS)
set -e

cd "$(dirname "$0")"

echo "Building Litt Engine tests..."
g++ -std=c++17 -O2 -Wall -I. tests.cpp -o tests

# Run tests
echo "Running tests..."
./tests
