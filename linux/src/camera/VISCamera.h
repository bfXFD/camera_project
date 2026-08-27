#pragma once
#include "camera/CameraBase.h"
#include <queue>
#include <mutex>
#include "CameraApi.h"   // MindVision SDK头文件

class VISCamera : public CameraBase {
public:
    bool open() override;
    bool start(CaptureMode mode) override;
    bool triggerOnce() override;
    bool grab(Frame& frame) override;
    void stop() override;
    void close() override;

    bool getFrameFromQueue(Frame& frame);

private:
    // Linux下不需要__stdcall，使用普通函数指针
    static void FrameCallback(
        CameraHandle hCamera,
        BYTE* pFrameBuffer,
        tSdkFrameHead* pFrameHead,
        void* pContext);

private:
    CameraHandle hCamera = -1;
    unsigned char* frameBuffer = nullptr;
    CaptureMode currentMode;

    std::queue<Frame> frameQueue;
    std::mutex queueMutex;
};
