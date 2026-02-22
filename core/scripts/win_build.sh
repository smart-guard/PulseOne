#!/bin/bash
# PulseOne Windows Cross-Compile Build Script
# Executed inside pulseone-windows-builder Docker container
set -e

# Use POSIX thread model for C++11/14/17 features
export CC=x86_64-w64-mingw32-gcc-posix
export CXX=x86_64-w64-mingw32-g++-posix
export AR=x86_64-w64-mingw32-ar
export RANLIB=x86_64-w64-mingw32-ranlib

# 0. Build Shared Libraries (CRITICAL for linking)
cd /src/core/shared
echo 'Building Shared Libraries (Windows Cross-compile)...'
rm -rf build lib && mkdir lib
make -j4 CROSS_COMPILE_WINDOWS=1

# 1. Build Collector
cd /src/core/collector
echo 'Building Collector (Release) with ALL Drivers...'
rm -rf build-windows bin-windows/*.exe
make -f Makefile.windows -j2

if [ -f bin-windows/collector.exe ]; then
    x86_64-w64-mingw32-strip --strip-unneeded bin-windows/collector.exe
    cp bin-windows/collector.exe /output/pulseone-collector.exe
    echo 'Collector binary copied to output'

    if [ -d bin-windows/drivers ] && ls bin-windows/drivers/*.dll 1>/dev/null 2>&1; then
        mkdir -p /output/drivers
        cp bin-windows/drivers/*.dll /output/drivers/
        echo 'Driver DLLs copied from drivers/'
    else
        echo 'No driver DLLs found, skipping'
    fi
else
    echo 'Collector binary NOT found after build'
    exit 1
fi

# 2. Build Export Gateway
cd /src/core/export-gateway
echo 'Building Export Gateway (Release)...'
if [ -f Makefile ]; then
    rm -rf build bin/*.exe
    make -j4 CROSS_COMPILE_WINDOWS=1

    if [ -f bin/export-gateway.exe ]; then
        x86_64-w64-mingw32-strip --strip-unneeded bin/export-gateway.exe
        cp bin/export-gateway.exe /output/pulseone-export-gateway.exe
        echo 'Export Gateway built'
    else
        echo 'Export Gateway binary NOT found'
        exit 1
    fi
else
    echo 'Makefile not found for export-gateway'
    exit 1
fi
