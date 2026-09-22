#pragma once
#include <vector>
#include <cstdint>
#include <string>

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
    // Shared host monotonic clock used for RGB/IR pairing.
    uint64_t timestamp = 0;
    uint64_t hostCallbackEndTimestampNs = 0;
    uint64_t deviceTimestampUs = 0;
    uint64_t frameId = 0;
    double exposureTimeUs = 0.0;
    bool hasDeviceTimestamp = false;
    bool hasFrameId = false;
    bool hasExposureTime = false;
    std::string timestampSource = "host_callback_entry_monotonic";
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
