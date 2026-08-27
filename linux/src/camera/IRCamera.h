#pragma once
#include "camera/CameraBase.h"
#include <chrono>
#include <queue>
#include <mutex>
#include "IRCUSBSDK.h"   // RayThink T1280 SDK头文件

class IRCamera : public CameraBase {
public:
    bool open() override;
    bool start(CaptureMode mode) override;
    bool triggerOnce() override;
    bool grab(Frame& frame) override;
    void stop() override;
    void close() override;

private:
    IRC_USB_HANDLE handle = 0;
    std::queue<Frame> frameQueue;
    std::mutex queueMutex;
    CaptureMode currentMode;
    
    bool getFrameFromQueue(Frame& frame);
    static void FrameCallback(IRC_USB_HANDLE handle, IRC_USB_VIDEO_INFO_CB* videoInfo, void* userPtr);
};
