#include "camera/IRCamera.h"
#include <cstring>
#include <cstdio>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>

// 搜索设备的回调函数
static int g_deviceCount = 0;
static int g_firstDeviceId = -1;
static std::string g_expectedCom;
static std::string g_expectedComName;

static bool containsText(const char* text, const std::string& value) {
    return text != nullptr && !value.empty() && std::string(text).find(value) != std::string::npos;
}

static void SearchCallback(IRC_USB_DEV_INFO* searchInfo, void* userData) {
    printf("Found device: ID=%d, Name=%s, SDK portName=%s, videoNode=%s\n",
           searchInfo->id,
           searchInfo->devName,
           searchInfo->portName,
           searchInfo->videoNode);

    // SDK can briefly report a stale placeholder while USB is re-enumerating.
    // It cannot be opened without a valid V4L2 node, so leave it for the next retry.
    if (searchInfo->videoNode == nullptr || searchInfo->videoNode[0] == '\0' ||
        !std::filesystem::exists(searchInfo->videoNode)) {
        printf("  -> Ignored incomplete SDK device record; waiting for a valid video node.\n");
        return;
    }

    // 优先选择脚本识别出的真实串口。部分SDK版本返回的portName不可靠，
    // 这里仅在SDK返回值恰好匹配时用于设备选择，不匹配时后续保留原选择逻辑。
    if (containsText(searchInfo->portName, g_expectedCom) || containsText(searchInfo->portName, g_expectedComName)) {
        if (g_firstDeviceId == -1) {
            g_firstDeviceId = searchInfo->id;
            printf("  -> Selected as IR camera (matched IR_CAMERA_COM=%s)\n", g_expectedCom.c_str());
        }
        g_deviceCount++;
        return;
    }

    // 优先查找包含"ThermaL"或"T1280"的设备
    if (strstr(searchInfo->devName, "ThermaL") != nullptr || 
        strstr(searchInfo->devName, "T1280") != nullptr) {
        if (g_firstDeviceId == -1) {
            g_firstDeviceId = searchInfo->id;
            printf("  -> Selected as IR camera (matched by name)\n");
        }
    }
    // 如果没有匹配的设备名，选择第一个具有有效视频节点的设备
    else if (g_firstDeviceId == -1) {
        g_firstDeviceId = searchInfo->id;
        printf("  -> Selected as IR camera (first device, name unknown)\n");
    }
    
    g_deviceCount++;
}

bool IRCamera::open() {
    const char* expectedCom = std::getenv("IR_CAMERA_COM");
    g_expectedCom = expectedCom != nullptr ? expectedCom : "";
    g_expectedComName.clear();
    if (!g_expectedCom.empty()) {
        const size_t slash = g_expectedCom.find_last_of('/');
        g_expectedComName = slash == std::string::npos ? g_expectedCom : g_expectedCom.substr(slash + 1);
    }

    if (!g_expectedCom.empty()) {
        printf("Using detected IR camera COM from IR_CAMERA_COM: %s\n", g_expectedCom.c_str());
    } else {
        printf("IR_CAMERA_COM is not set; falling back to SDK device search only.\n");
    }

    // 1. 初始化SDK
    int ret = IRC_USB_Init();
    if (ret != IRC_USB_ERROR_OK) {
        printf("IRC_USB_Init failed with error code: %d\n", ret);
        return false;
    }

    // 2. 搜索设备。USB链路重新枚举后，节点可能已出现但SDK缓存尚未就绪。
    for (int attempt = 1; attempt <= 5; ++attempt) {
        g_deviceCount = 0;
        g_firstDeviceId = -1;
        ret = IRC_USB_SearchDev(SearchCallback, nullptr);
        if (ret != IRC_USB_ERROR_OK) {
            printf("IRC_USB_SearchDev attempt %d failed with error code: %d\n", attempt, ret);
        } else if (g_deviceCount > 0 && g_firstDeviceId != -1) {
            break;
        } else {
            printf("IRC_USB_SearchDev attempt %d found no devices; waiting for USB nodes to settle...\n", attempt);
        }

        if (attempt < 5) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    if (g_deviceCount <= 0 || g_firstDeviceId == -1) {
        printf("No IR camera devices found. Device count: %d, Selected ID: %d\n", g_deviceCount, g_firstDeviceId);
        if (std::filesystem::exists("/dev/video21") && std::filesystem::exists("/dev/ttyACM0")) {
            printf("Linux USB/UVC nodes exist, but the vendor SDK did not enumerate the camera.\n");
        } else {
            printf("The camera USB/UVC nodes disappeared during SDK discovery.\n");
        }
        IRC_USB_Deinit();
        return false;
    }
    
    printf("Found %d IR camera device(s), first device ID: %d\n", g_deviceCount, g_firstDeviceId);
    if (!g_expectedCom.empty()) {
        printf("Expected IR camera COM: %s\n", g_expectedCom.c_str());
        printf("Note: SDK portName may be incorrect; /dev COM detection is performed in run.sh.\n");
    }

    // 3. 打开设备
    ret = IRC_USB_OpenDev(g_firstDeviceId, &handle);
    if (ret != IRC_USB_ERROR_OK) {
        printf("IRC_USB_OpenDev failed with error code: %d\n", ret);
        IRC_USB_Deinit();
        return false;
    }
    
    printf("IR camera opened successfully, handle: %p\n", (void*)handle);
    
    // 4. 等待设备初始化稳定。快门校正在确认视频流出帧后再执行，
    // 避免设备刚打开时串口控制打断 UVC 出流。
    printf("Waiting for device to stabilize (500ms)...\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    return true;
}

bool IRCamera::start(CaptureMode mode) {
    currentMode = mode;
    
    std::cout << "Starting IR camera preview..." << std::endl;
    
    // 启动预览（使用回调函数）
    int ret = IRC_USB_StartPreview(handle, FrameCallback, this);
    
    std::cout << "IRC_USB_StartPreview returned: " << ret << std::endl;
    
    if (ret != IRC_USB_ERROR_OK) {
        std::cerr << "IRC_USB_StartPreview failed with code: " << ret << std::endl;
        return false;
    }
    
    std::cout << "IR camera preview started successfully" << std::endl;
    
    // 等待并验证回调是否开始工作。没有首帧就返回失败，避免后续同步流程空跑。
    std::cout << "Waiting for first frame (5 seconds)..." << std::endl;
    for (int i = 0; i < 50; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        bool hasFrame = false;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            hasFrame = !frameQueue.empty();
        }

        if (hasFrame) {
            std::cout << "IR camera receiving frames!" << std::endl;

            std::cout << "Performing shutter correction..." << std::endl;
            ret = IRC_USB_CorrectShutter(handle);
            if (ret != IRC_USB_ERROR_OK) {
                std::cout << "Warning: IRC_USB_CorrectShutter failed with code: " << ret << " (continuing anyway)" << std::endl;
            } else {
                std::cout << "Shutter correction completed" << std::endl;
            }

            std::cout << "Waiting after shutter correction (300ms)..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            return true;
        }
    }
    
    std::cerr << "[ERROR] IR camera: No frames received in 5 seconds!" << std::endl;
    if (!std::filesystem::exists("/dev/video21") || !std::filesystem::exists("/dev/ttyACM0")) {
        std::cerr << "  The camera disconnected or re-enumerated after preview started." << std::endl;
        std::cerr << "  Check lsusb -t and dmesg for USB -71/EPROTO errors." << std::endl;
    } else {
        std::cerr << "  The USB nodes still exist, but no valid UVC frame was received." << std::endl;
        std::cerr << "  Check dmesg for video endpoint -71/EPROTO errors." << std::endl;
    }
    return false;
}

bool IRCamera::triggerOnce() {
    // IR相机SDK可能不支持软件触发，返回false
    return false;
}

bool IRCamera::grab(Frame& frame) {
    // IR相机使用回调模式，从队列获取帧
    return getFrameFromQueue(frame);
}

void IRCamera::FrameCallback(IRC_USB_HANDLE handle, IRC_USB_VIDEO_INFO_CB* videoInfo, void* userPtr) {
    // 将userPtr转换为IRCamera*指针
    IRCamera* camera = static_cast<IRCamera*>(userPtr);
    
    // 添加调试信息（首次调用时打印）
    static bool firstCall = true;
    if (firstCall) {
        std::cout << "[IR Camera] First frame callback received!" << std::endl;
        std::cout << "  Width: " << videoInfo->width << ", Height: " << videoInfo->height << std::endl;
        std::cout << "  Frame Format: " << videoInfo->frameFmt << " (0=YUYV, 1=UYVY, 2=RAW)" << std::endl;
        firstCall = false;
    }
    
    // 创建Frame对象
    Frame frame;
    frame.width = videoInfo->width;
    frame.height = videoInfo->height;
    frame.channels = 1;  // 输出灰度图像
    
    // 根据帧格式计算数据大小并提取亮度数据
    int pixelCount = videoInfo->width * videoInfo->height;
    
    if (videoInfo->frameFmt == IRC_NET_FRAME_FMT_YUYV || videoInfo->frameFmt == IRC_NET_FRAME_FMT_UYVY) {
        // YUYV/UYVY 格式：每2个像素占4字节 (Y0 U Y1 V 或 U Y0 V Y1)
        // 只提取Y分量作为灰度图像
        frame.data.resize(pixelCount);
        uint8_t* src = (uint8_t*)videoInfo->dataBuf;
        
        if (videoInfo->frameFmt == IRC_NET_FRAME_FMT_YUYV) {
            // YUYV: Y在位置0和2
            for (int i = 0; i < pixelCount; i++) {
                // 每4字节包含2个Y值，位置0和2
                int byteIndex = (i / 2) * 4 + (i % 2) * 2;
                frame.data[i] = src[byteIndex];
            }
        } else {
            // UYVY: Y在位置1和3
            for (int i = 0; i < pixelCount; i++) {
                // 每4字节包含2个Y值，位置1和3
                int byteIndex = (i / 2) * 4 + (i % 2) * 2 + 1;
                frame.data[i] = src[byteIndex];
            }
        }
    } else {
        // RAW格式或其他：假设每像素1字节
        frame.data.assign((uint8_t*)videoInfo->dataBuf, (uint8_t*)videoInfo->dataBuf + pixelCount);
    }
    
    // 记录时间戳
    frame.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    
    // 使用互斥锁保护队列操作
    std::lock_guard<std::mutex> lock(camera->queueMutex);
    camera->frameQueue.push(std::move(frame));
}

bool IRCamera::getFrameFromQueue(Frame& frame) {
    // 使用lock_guard锁定互斥锁
    std::lock_guard<std::mutex> lock(queueMutex);
    
    // 检查队列是否为空
    if (frameQueue.empty()) {
        return false;
    }
    
    // 从队列取出front()帧并赋值给参数frame
    frame = frameQueue.front();
    
    // 调用pop()移除队列头部
    frameQueue.pop();
    
    return true;
}

void IRCamera::stop() {
    IRC_USB_StopPreview(handle);
}

void IRCamera::close() {
    // 清空frameQueue中的所有帧
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        while (!frameQueue.empty()) {
            frameQueue.pop();
        }
    }
    
    // 关闭设备
    IRC_USB_CloseDev(handle);
    
    // 反初始化SDK
    IRC_USB_Deinit();
}
