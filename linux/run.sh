#!/bin/bash

# 双相机同步采集 - Linux 运行脚本
# 支持 x86_64 和 aarch64 架构

# 检测系统架构
ARCH=$(uname -m)

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
        echo "警告: 未知架构: $ARCH, 默认使用 arm64"
        LIB_ARCH="arm64"
        IR_SDK_DIR="IRCamera_Linux_aarch64"
        ;;
esac

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# SDK库路径
MVSDK_LIB_DIR="${SCRIPT_DIR}/../SDK_Linux/CameraSDK/lib/${LIB_ARCH}"
IR_LIB_DIR="${SCRIPT_DIR}/../SDK_Linux/${IR_SDK_DIR}/SDK/libs"

# 设置动态库搜索路径
export LD_LIBRARY_PATH="${MVSDK_LIB_DIR}:${IR_LIB_DIR}:${LD_LIBRARY_PATH}"

check_ir_camera_usb_speed() {
    local device
    local vendor
    local product
    local speed

    for device in /sys/bus/usb/devices/*; do
        [ -f "${device}/idVendor" ] || continue
        [ -f "${device}/idProduct" ] || continue
        read -r vendor < "${device}/idVendor"
        read -r product < "${device}/idProduct"
        if [ "$vendor" = "04b4" ] && [ "$product" = "7510" ]; then
            read -r speed < "${device}/speed"
            echo "IR相机USB连接速度: ${speed}M (${device##*/})"
            if [ "$speed" -lt 5000 ]; then
                echo "错误: IR相机已降级到USB 2.0，1280x1024 YUYV 30fps需要USB 3.0。"
                echo "请重新插拔USB 3.0线缆并确认 lsusb -t 显示相机为5000M。"
                return 1
            fi
            return 0
        fi
    done

    echo "错误: USB总线上未枚举到IR相机 04b4:7510。"
    echo "请检查 lsusb 和内核日志中的 -71、invalid descriptor 或 no configurations。"
    return 1
}

check_ir_camera_usb_speed || exit 1

# 自动识别 IR 相机真实串口。SDK 返回的 portName 在部分环境中不可靠，
# 因此优先使用 Linux 的稳定 by-id 设备名解析到实际 /dev/ttyACM* 或 /dev/ttyUSB*。
detect_ir_camera_com() {
    local link
    local resolved

    if [ -d /dev/serial/by-id ]; then
        for link in /dev/serial/by-id/*ThermaL* /dev/serial/by-id/*Thermal* /dev/serial/by-id/*T1280* /dev/serial/by-id/*IRCamera* /dev/serial/by-id/*IR_Camera*; do
            if [ -e "$link" ]; then
                resolved="$(readlink -f "$link")"
                if [ -n "$resolved" ] && [ -e "$resolved" ]; then
                    printf '%s\n' "$resolved"
                    return 0
                fi
            fi
        done
    fi

    for resolved in /dev/ttyACM* /dev/ttyUSB*; do
        if [ -e "$resolved" ]; then
            printf '%s\n' "$resolved"
            return 0
        fi
    done

    return 1
}

if [ -z "${IR_CAMERA_COM:-}" ]; then
    if IR_CAMERA_COM="$(detect_ir_camera_com)"; then
        export IR_CAMERA_COM
        echo "IR相机串口: ${IR_CAMERA_COM}"
    else
        echo "警告: 未自动识别到IR相机串口，请检查 /dev/serial/by-id、/dev/ttyACM* 或 /dev/ttyUSB*"
    fi
else
    export IR_CAMERA_COM
    echo "IR相机串口: ${IR_CAMERA_COM} (来自环境变量)"
fi

# 图片保存目录。后续外接硬盘时可这样运行：
# CAPTURE_SAVE_DIR=/media/topeet/your_disk/sync_pairs ./run.sh
if [ -z "${CAPTURE_SAVE_DIR:-}" ]; then
    export CAPTURE_SAVE_DIR="captured_images/sync_pairs"
else
    export CAPTURE_SAVE_DIR
fi
echo "图片保存目录: ${CAPTURE_SAVE_DIR}"

# 检查build目录
if [ ! -d "${SCRIPT_DIR}/build" ]; then
    echo "构建目录不存在，请先运行 ./build.sh"
    exit 1
fi

cd "${SCRIPT_DIR}/build"

# 检查双相机同步程序
if [ ! -f "test_sync_1_sample" ]; then
    echo "未找到双相机同步程序，请先运行 ./build.sh"
    exit 1
fi

echo "=== 双相机同步采集程序 ==="
echo ""
echo "启动程序..."
echo "----------------------------------------"

# 直接运行双相机同步程序
./test_sync_1_sample

exit $?
