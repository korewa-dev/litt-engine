#!/bin/bash
# Run Dither3D demo
cd "$(dirname "$0")"

if [ -f "bin/dither3d_demo" ]; then
    echo "Starting Dither3D Demo..."
    ./bin/dither3d_demo
else
    echo "Building first..."
    make dither3d_demo
    if [ -f "bin/dither3d_demo" ]; then
        echo "Starting Dither3D Demo..."
        ./bin/dither3d_demo
    else
        echo "Error: dither3d_demo not found"
        exit 1
    fi
fi
