#!/bin/bash

# 双相机定时同步采集。默认每2秒保存一组，q或Ctrl+C退出。

ARCH=$(uname -m)

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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MVSDK_LIB_DIR="${SCRIPT_DIR}/../SDK_Linux/CameraSDK/lib/${LIB_ARCH}"
IR_LIB_DIR="${SCRIPT_DIR}/../SDK_Linux/${IR_SDK_DIR}/SDK/libs"
export LD_LIBRARY_PATH="${MVSDK_LIB_DIR}:${IR_LIB_DIR}:${LD_LIBRARY_PATH}"

check_ir_camera_usb_speed() {
    local device vendor product speed
    for device in /sys/bus/usb/devices/*; do
        [ -f "${device}/idVendor" ] || continue
        [ -f "${device}/idProduct" ] || continue
        read -r vendor < "${device}/idVendor"
        read -r product < "${device}/idProduct"
        if [ "$vendor" = "04b4" ] && [ "$product" = "7510" ]; then
            read -r speed < "${device}/speed"
            echo "IR相机USB连接速度: ${speed}M (${device##*/})"
            if [ "$speed" -lt 5000 ]; then
                echo "错误: IR相机已降级到USB 2.0，必须以5000M连接。"
                return 1
            fi
            return 0
        fi
    done
    echo "错误: USB总线上未枚举到IR相机 04b4:7510。"
    return 1
}

detect_ir_camera_com() {
    local link resolved
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

check_ir_camera_usb_speed || exit 1

if [ -z "${IR_CAMERA_COM:-}" ]; then
    if IR_CAMERA_COM="$(detect_ir_camera_com)"; then
        export IR_CAMERA_COM
        echo "IR相机串口: ${IR_CAMERA_COM}"
    else
        echo "警告: 未自动识别到IR相机串口。"
    fi
fi

export AUTO_CAPTURE_INTERVAL_SECONDS="${AUTO_CAPTURE_INTERVAL_SECONDS:-2}"
SESSION_ID="${CAPTURE_SESSION_ID:-session_$(date +%Y%m%d_%H%M%S)}"
export CAPTURE_SAVE_DIR="${CAPTURE_SAVE_DIR:-/home/topeet/Camera_project/captures/sessions/${SESSION_ID}/sync_pairs}"

if [ ! -x "${SCRIPT_DIR}/build/test_timed_capture" ]; then
    echo "未找到定时采集程序，请先运行 ${SCRIPT_DIR}/build.sh"
    exit 1
fi

echo "=== 双相机定时同步采集 ==="
echo "采集周期: ${AUTO_CAPTURE_INTERVAL_SECONDS} 秒"
echo "保存目录: ${CAPTURE_SAVE_DIR}"
echo "按 q 或 Ctrl+C 退出"
echo "----------------------------------------"

cd "${SCRIPT_DIR}/build"
exec ./test_timed_capture
