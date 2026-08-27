cmake_minimum_required(VERSION 3.10)
project(DualCameraSync_Linux CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===== 自动检测系统架构 =====
execute_process(COMMAND uname -m OUTPUT_VARIABLE ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
message(STATUS "Detected architecture: ${ARCH}")

# 根据架构选择库目录
if(ARCH STREQUAL "x86_64")
    set(MVSDK_LIB_ARCH "x64")
elseif(ARCH STREQUAL "aarch64")
    set(MVSDK_LIB_ARCH "arm64")
elseif(ARCH MATCHES "arm.*")
    set(MVSDK_LIB_ARCH "arm")
elseif(ARCH STREQUAL "i686" OR ARCH STREQUAL "i386")
    set(MVSDK_LIB_ARCH "x86")
else()
    message(WARNING "Unknown architecture: ${ARCH}, defaulting to arm64")
    set(MVSDK_LIB_ARCH "arm64")
endif()

message(STATUS "Using MindVision SDK library for: ${MVSDK_LIB_ARCH}")

# ===== SDK路径配置 =====
set(MVSDK_ROOT "${PROJECT_SOURCE_DIR}/../SDK_Linux/CameraSDK")

# 根据架构选择IR相机SDK路径
if(ARCH STREQUAL "aarch64")
    set(T1280_ROOT "${PROJECT_SOURCE_DIR}/../SDK_Linux/IRCamera_Linux_aarch64/SDK")
    message(STATUS "Using IR Camera SDK for aarch64 architecture")
else()
    set(T1280_ROOT "${PROJECT_SOURCE_DIR}/../SDK_Linux/IRCamera_Linux/SDK")
    message(STATUS "Using IR Camera SDK for x86_64/other architecture")
endif()

# 检查库文件是否存在
set(MVSDK_LIB_PATH "${MVSDK_ROOT}/lib/${MVSDK_LIB_ARCH}/libMVSDK.so")
set(T1280_LIB_PATH "${T1280_ROOT}/libs/libIRCUSBSDK.so")

if(NOT EXISTS ${MVSDK_LIB_PATH})
    message(FATAL_ERROR "MindVision SDK library not found at: ${MVSDK_LIB_PATH}\n"
                        "Please check if the SDK is properly installed for ${ARCH} architecture.\n"
                        "Available architectures in SDK: arm, arm64, x64, x86")
endif()

if(NOT EXISTS ${T1280_LIB_PATH})
    message(FATAL_ERROR "IR Camera SDK library not found at: ${T1280_LIB_PATH}\n"
                        "Please check if the SDK is properly installed.")
endif()

message(STATUS "MindVision SDK lib: ${MVSDK_LIB_PATH}")
message(STATUS "T1280 SDK lib: ${T1280_LIB_PATH}")

# ===== 源文件 =====
set(CAMERA_SRC
    src/camera/VISCamera.cpp
    src/camera/IRCamera.cpp
)

set(SYNC_SRC
    src/sync/SyncController.cpp
)

set(STORAGE_SRC
    src/storage/ImageSaver.cpp
)

# 链接线程库
find_package(Threads REQUIRED)

# ===== 双相机同步程序 =====
add_executable(test_sync_1_sample
    tests/test_sync_1_sample.cpp
    ${CAMERA_SRC}
    ${SYNC_SRC}
    ${STORAGE_SRC}
)

target_include_directories(test_sync_1_sample PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${MVSDK_ROOT}/include
    ${T1280_ROOT}/include
)

target_link_libraries(test_sync_1_sample PRIVATE
    ${MVSDK_LIB_PATH}
    ${T1280_LIB_PATH}
    Threads::Threads
)

set_target_properties(test_sync_1_sample PROPERTIES
    BUILD_RPATH "${MVSDK_ROOT}/lib/${MVSDK_LIB_ARCH}:${T1280_ROOT}/libs"
    INSTALL_RPATH "${MVSDK_ROOT}/lib/${MVSDK_LIB_ARCH}:${T1280_ROOT}/libs"
)

# ===== 长期键盘控制采集程序 =====
add_executable(test_keyboard_capture
    tests/test_keyboard_capture.cpp
    ${CAMERA_SRC}
    ${SYNC_SRC}
    ${STORAGE_SRC}
)

target_include_directories(test_keyboard_capture PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${MVSDK_ROOT}/include
    ${T1280_ROOT}/include
)

target_link_libraries(test_keyboard_capture PRIVATE
    ${MVSDK_LIB_PATH}
    ${T1280_LIB_PATH}
    Threads::Threads
)

set_target_properties(test_keyboard_capture PROPERTIES
    BUILD_RPATH "${MVSDK_ROOT}/lib/${MVSDK_LIB_ARCH}:${T1280_ROOT}/libs"
    INSTALL_RPATH "${MVSDK_ROOT}/lib/${MVSDK_LIB_ARCH}:${T1280_ROOT}/libs"
)

message(STATUS "=== Linux Build Configuration ===")
message(STATUS "Architecture: ${ARCH}")
message(STATUS "MindVision SDK: ${MVSDK_ROOT}")
message(STATUS "MindVision lib arch: ${MVSDK_LIB_ARCH}")
message(STATUS "T1280 SDK: ${T1280_ROOT}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Building: test_sync_1_sample (dual camera sync)")
message(STATUS "Building: test_keyboard_capture (persistent keyboard capture)")
