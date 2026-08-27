#!/bin/bash

# Dual Camera Sync - Linux Build Script
# 支持 x86_64 和 aarch64 架构

set -e  # 遇到错误立即退出

echo "=== Building Dual Camera Sync (Linux) ==="

# 检测系统架构
ARCH=$(uname -m)
echo "Detected architecture: $ARCH"

# 根据架构选择库目录
case "$ARCH" in
    x86_64)
        LIB_ARCH="x64"
        IR_SDK_DIR="IRCamera_Linux"
        ;;
    aarch64)
        LIB_ARCH="arm64"
        IR_SDK_DIR="IRCamera_Linux_aarch64"
        ;;
    armv7l|armhf|arm*)
        LIB_ARCH="arm"
        IR_SDK_DIR="IRCamera_Linux"
        ;;
    i686|i386)
        LIB_ARCH="x86"
        IR_SDK_DIR="IRCamera_Linux"
        ;;
    *)
        echo "Warning: Unknown architecture: $ARCH, defaulting to arm64"
        LIB_ARCH="arm64"
        IR_SDK_DIR="IRCamera_Linux_aarch64"
        ;;
esac

echo "Using SDK library for: $LIB_ARCH"

# SDK路径
MVSDK_LIB="../SDK_Linux/CameraSDK/lib/${LIB_ARCH}/libMVSDK.so"
IR_LIB="../SDK_Linux/${IR_SDK_DIR}/SDK/libs/libIRCUSBSDK.so"

# 检查MindVision SDK库文件
if [ ! -f "$MVSDK_LIB" ]; then
    echo "ERROR: MindVision SDK library not found at: $MVSDK_LIB"
    echo ""
    echo "Please ensure SDK_Linux is in the same parent directory as linux/"
    echo "Expected structure:"
    echo "  Camera_Project/"
    echo "  ├── linux/"
    echo "  └── SDK_Linux/"
    echo "      └── CameraSDK/lib/${LIB_ARCH}/libMVSDK.so"
    exit 1
fi

# 检查IR相机SDK库文件
if [ ! -f "$IR_LIB" ]; then
    echo ""
    echo "WARNING: IR Camera SDK library not found at: $IR_LIB"
    echo "         Only VIS camera test will be built."
    echo ""
fi

# 清理之前的build
echo "Cleaning previous build..."
rm -rf build

# 创建build目录
mkdir -p build
cd build

# 运行CMake
echo "Running CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译
echo "Building..."
make -j$(nproc)

echo ""
echo "=== Build Successful ==="
echo ""

# 显示编译结果
if [ -f "test_vis_only" ]; then
    echo "Built: test_vis_only (VIS camera only)"
fi

if [ -f "test_sync_1_sample" ]; then
    echo "Built: test_sync_1_sample (dual camera sync)"
fi

echo ""
echo "To run the tests, use: ./run.sh"
