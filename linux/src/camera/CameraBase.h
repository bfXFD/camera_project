#pragma once
#include <vector>
#include <cstdint>

enum class CaptureMode {
    Polling,
    Callback,
    SoftwareTrigger
};

struct Frame {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<uint8_t> data;
    uint64_t timestamp = 0;
};

class CameraBase {
public:
    virtual ~CameraBase() = default;
    
    virtual bool open() = 0;
    virtual bool start(CaptureMode mode) = 0;
    virtual bool triggerOnce() = 0;
    virtual bool grab(Frame& frame) = 0;
    virtual void stop() = 0;
    virtual void close() = 0;
};
