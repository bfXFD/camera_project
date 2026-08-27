#pragma once
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <string>
#include "camera/CameraBase.h"

// 采集模式枚举
enum class CaptureSceneMode {
    Light,  // 有光模式
    Dark    // 无光模式
};

class SyncController {
public:
    struct SyncStats {
        int matchedPairs = 0;      // 匹配成功的帧对数
        int droppedFrames = 0;     // 丢弃的帧数
        double avgTimeDiff = 0.0;  // 平均时间戳差异（毫秒）
        double maxTimeDiff = 0.0;  // 最大时间戳差异（毫秒）
        double minTimeDiff = 1e9;  // 最小时间戳差异（毫秒）
    };
    
    SyncController(CameraBase* camA, CameraBase* camB,
                   CaptureSceneMode sceneMode = CaptureSceneMode::Light,
                   int startIndex = -1,
                   int maxPairsToSave = 0);  // startIndex=-1表示自动检测，maxPairsToSave<=0表示不限
    void start();
    void stop();
    
    // 软件触发同步：同时触发两个相机
    bool triggerBoth();
    
    // 获取同步统计信息
    SyncStats getStats() const;
    void requestSavePairs(int count = 1);

private:
    void grabLoop(CameraBase* cam, std::queue<Frame>& q, std::mutex& mtx);
    void syncLoop();

    CameraBase* cam1;
    CameraBase* cam2;

    std::queue<Frame> q1, q2;
    std::mutex mtx1, mtx2;

    std::atomic<bool> running{false};
    std::thread t1, t2, tSync;
    
    // 同步帧对计数器
    int syncPairCount = 0;
    int maxPairsToSave = 0;
    std::atomic<int> pendingSavePairs{0};
    
    // 统计信息
    mutable std::mutex statsMutex;
    SyncStats stats;
    double totalTimeDiff = 0.0;  // 用于计算平均值
    
    // 采集模式和命名相关
    CaptureSceneMode sceneMode;
    std::string sceneModeStr;  // "light" 或 "dark"
    std::string saveDir;       // 保存目录路径
    
    // 扫描目录获取最大编号
    int scanMaxIndex(const std::string& directory, const std::string& pattern);
};
