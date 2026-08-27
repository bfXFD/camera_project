#include "camera/VISCamera.h"
#include <cstring>
#include <chrono>
#include <iostream>

// Linux下需要全局初始化SDK（只初始化一次）
static bool g_sdkInitialized = false;

bool VISCamera::open() {
    // Linux版本：首次调用时初始化SDK
    if (!g_sdkInitialized) {
        std::cout << "Initializing MindVision SDK..." << std::endl;
        int ret = CameraSdkInit(1);  // 1 = 中文提示信息
        if (ret != CAMERA_STATUS_SUCCESS) {
            std::cerr << "CameraSdkInit failed with code: " << ret << std::endl;
            return false;
        }
        g_sdkInitialized = true;
        std::cout << "MindVision SDK initialized successfully" << std::endl;
    }
    
    // 枚举设备
    tSdkCameraDevInfo devList[8];
    int camCount = 8;
    int ret = CameraEnumerateDevice(devList, &camCount);
    
    std::cout << "Camera enumeration result: " << ret << ", count: " << camCount << std::endl;
    
    if (ret != CAMERA_STATUS_SUCCESS) {
        std::cerr << "CameraEnumerateDevice failed with code: " << ret << std::endl;
        return false;
    }
    
    if (camCount <= 0) {
        std::cerr << "No MindVision cameras found!" << std::endl;
        return false;
    }
    
    // 打印找到的相机信息
    for (int i = 0; i < camCount; i++) {
        std::cout << "Camera " << i << ": " 
                  << devList[i].acProductName << " - " 
                  << devList[i].acFriendlyName << std::endl;
    }
    
    // 初始化第一个相机
    ret = CameraInit(&devList[0], -1, -1, &hCamera);
    if (ret != CAMERA_STATUS_SUCCESS) {
        std::cerr << "CameraInit failed with code: " << ret << std::endl;
        return false;
    }
    
    std::cout << "Camera initialized successfully, handle: " << hCamera << std::endl;

    // 设置输出格式为BGR8
    ret = CameraSetIspOutFormat(hCamera, CAMERA_MEDIA_TYPE_BGR8);
    if (ret != CAMERA_STATUS_SUCCESS) {
        std::cerr << "CameraSetIspOutFormat failed with code: " << ret << std::endl;
        CameraUnInit(hCamera);
        hCamera = -1;
        return false;
    }

    // 使用官方采集软件一键白平衡得到的固定数字增益。
    // CameraSetGain的单位为1/100倍：R=1.37, G=1.00, B=2.07。
    ret = CameraSetWbMode(hCamera, FALSE);
    if (ret != CAMERA_STATUS_SUCCESS) {
        std::cerr << "Warning: CameraSetWbMode(manual) failed with code: " << ret << std::endl;
    }

    ret = CameraSetGain(hCamera, 137, 100, 207);
    if (ret != CAMERA_STATUS_SUCCESS) {
        std::cerr << "Warning: CameraSetGain(R=137,G=100,B=207) failed with code: " << ret << std::endl;
    } else {
        std::cout << "VIS fixed white balance gain enabled: R=1.37, G=1.00, B=2.07" << std::endl;
    }
    
    return true;
}

bool VISCamera::start(CaptureMode m) {
    currentMode = m;

    if (currentMode == CaptureMode::Callback || currentMode == CaptureMode::SoftwareTrigger) {
        CameraSetCallbackFunction(
            hCamera,
            FrameCallback,
            this,
            nullptr
        );
    }

    if (currentMode == CaptureMode::SoftwareTrigger) {
        CameraSetTriggerMode(hCamera, 1); // 软触发
    } else {
        CameraSetTriggerMode(hCamera, 0); // 连续
    }

    return CameraPlay(hCamera) == CAMERA_STATUS_SUCCESS;
}

bool VISCamera::triggerOnce() {
    if (currentMode != CaptureMode::SoftwareTrigger) {
        return false;
    }
    return CameraSoftTrigger(hCamera) == CAMERA_STATUS_SUCCESS;
}

bool VISCamera::grab(Frame& frame) {
    // 如果是Callback或SoftwareTrigger模式，从队列获取
    if (currentMode == CaptureMode::Callback || 
        currentMode == CaptureMode::SoftwareTrigger) {
        return getFrameFromQueue(frame);
    }
    
    // Polling模式的实现
    tSdkFrameHead frameInfo;
    BYTE* pRaw = nullptr;

    if (CameraGetImageBuffer(hCamera, &frameInfo, &pRaw, 1000)
        != CAMERA_STATUS_SUCCESS)
        return false;

    if (!frameBuffer) {
        frameBuffer = (unsigned char*)malloc(
            frameInfo.iWidth * frameInfo.iHeight * 3);
    }

    CameraImageProcess(hCamera, pRaw, frameBuffer, &frameInfo);
    CameraReleaseImageBuffer(hCamera, pRaw);

    frame.width = frameInfo.iWidth;
    frame.height = frameInfo.iHeight;
    frame.channels = 3;
    frame.data.assign(frameBuffer,
                      frameBuffer + frame.width * frame.height * 3);

    frame.timestamp =
        std::chrono::steady_clock::now().time_since_epoch().count();

    return true;
}

void VISCamera::stop() {
    CameraPause(hCamera);
}

void VISCamera::close() {
    // 检查frameBuffer是否为nullptr再释放
    if (frameBuffer != nullptr) {
        free(frameBuffer);
        frameBuffer = nullptr;
    }
    
    // 清空frameQueue中的所有帧
    std::lock_guard<std::mutex> lock(queueMutex);
    while (!frameQueue.empty()) {
        frameQueue.pop();
    }
    
    // 确保CameraUnInit正确调用
    if (hCamera != -1) {
        CameraUnInit(hCamera);
        hCamera = -1;
    }
}

// Linux版本：移除__stdcall调用约定
void VISCamera::FrameCallback(
    CameraHandle hCamera,
    BYTE* pFrameBuffer,
    tSdkFrameHead* pFrameHead,
    void* pContext)
{
    VISCamera* camera = static_cast<VISCamera*>(pContext);
    
    try {
        Frame frame;
        frame.width = pFrameHead->iWidth;
        frame.height = pFrameHead->iHeight;
        frame.channels = 3;
        
        // 分配临时缓冲区用于处理后的图像
        int dataSize = frame.width * frame.height * frame.channels;
        std::vector<uint8_t> processedBuffer(dataSize);
        
        // 使用CameraImageProcess处理图像（从RAW转换为BGR）
        int ret = CameraImageProcess(hCamera, pFrameBuffer, processedBuffer.data(), pFrameHead);
        if (ret != CAMERA_STATUS_SUCCESS) {
            std::cerr << "[ERROR] CameraImageProcess failed with code: " << ret << std::endl;
            return;
        }
        
        // 复制处理后的图像数据
        frame.data = std::move(processedBuffer);
        
        // 记录时间戳
        frame.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        
        // 线程安全地将帧加入队列
        std::lock_guard<std::mutex> lock(camera->queueMutex);
        camera->frameQueue.push(std::move(frame));
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception in VISCamera::FrameCallback: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[ERROR] Unknown exception in VISCamera::FrameCallback" << std::endl;
    }
}

bool VISCamera::getFrameFromQueue(Frame& frame) {
    std::lock_guard<std::mutex> lock(queueMutex);
    
    if (frameQueue.empty()) {
        return false;
    }
    
    frame = frameQueue.front();
    frameQueue.pop();
    
    return true;
}
