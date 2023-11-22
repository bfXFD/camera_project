#include "sync/SyncController.h"
#include "storage/ImageSaver.h"
#include <fstream>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <thread>
#include <regex>
#include <cstdlib>

static const uint64_t SYNC_THRESHOLD_NS = 5e6; // 5 ms
static const size_t MAX_PENDING_FRAMES_PER_CAMERA = 4;
static const size_t MAX_PENDING_PAIRS_TO_WRITE = 2;

SyncController::SyncController(CameraBase* camA, CameraBase* camB,
                               CaptureSceneMode sceneMode, int startIndex, int maxPairsToSave)
    : cam1(camA), cam2(camB), maxPairsToSave(maxPairsToSave), sceneMode(sceneMode) {
    
    // 设置场景模式字符串
    sceneModeStr = (sceneMode == CaptureSceneMode::Light) ? "light" : "dark";
    
    // 创建同步图像保存目录（带错误处理）。可通过 CAPTURE_SAVE_DIR 指向外接硬盘。
    const char* captureSaveDir = std::getenv("CAPTURE_SAVE_DIR");
    saveDir = (captureSaveDir != nullptr && captureSaveDir[0] != '\0')
        ? captureSaveDir
        : "captured_images/sync_pairs";
    try {
        std::filesystem::create_directories(saveDir);
        std::cout << "[SyncController] 保存目录已创建/确认: " << saveDir << std::endl;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "[SyncController] 警告: 无法创建目录 " << saveDir << std::endl;
        std::cerr << "  错误: " << e.what() << std::endl;
        
        // 尝试使用用户主目录下的目录
        const char* home = std::getenv("HOME");
        if (home) {
            saveDir = std::string(home) + "/captured_images/sync_pairs";
            try {
                std::filesystem::create_directories(saveDir);
                std::cout << "[SyncController] 改用主目录: " << saveDir << std::endl;
            } catch (const std::filesystem::filesystem_error& e2) {
                std::cerr << "[SyncController] 错误: 也无法创建目录 " << saveDir << std::endl;
                std::cerr << "  错误: " << e2.what() << std::endl;
                std::cerr << "[SyncController] 请手动创建目录或使用sudo运行" << std::endl;
            }
        }
    }
    
    // 确定起始编号
    if (startIndex >= 0) {
        // 使用用户指定的起始编号
        syncPairCount = startIndex;
    } else {
        // 自动检测：扫描目录获取当前模式的最大编号
        int maxIndex = scanMaxIndex(saveDir, sceneModeStr);
        syncPairCount = maxIndex;  // 下一次保存时会先+1
    }
    
    std::cout << "[SyncController] 场景模式: " << sceneModeStr << std::endl;
    std::cout << "[SyncController] 起始编号: " << (syncPairCount + 1) << std::endl;
}

int SyncController::scanMaxIndex(const std::string& directory, const std::string& pattern) {
    int maxIndex = 0;
    
    if (!std::filesystem::exists(directory)) {
        return 0;
    }
    
    // 正则表达式匹配文件名中的编号
    // 格式: sync_pair_<number>_<pattern>_vis.png 或 sync_pair_<number>_<pattern>_ir.png
    std::regex filePattern("sync_pair_(\\d+)_" + pattern + "_(vis|ir)\\.png");
    
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            std::smatch match;
            if (std::regex_match(filename, match, filePattern)) {
                int index = std::stoi(match[1].str());
                maxIndex = std::max(maxIndex, index);
            }
        }
    }
    
    std::cout << "[SyncController] 扫描目录 " << directory << " 中 " << pattern
              << " 模式的最大编号: " << maxIndex << std::endl;
    
    return maxIndex;
}

void SyncController::start() {
    running = true;
    writerStopping = false;
    tWriter = std::thread(&SyncController::writerLoop, this);
    t1 = std::thread(&SyncController::grabLoop, this, cam1, std::ref(q1), std::ref(mtx1));
    t2 = std::thread(&SyncController::grabLoop, this, cam2, std::ref(q2), std::ref(mtx2));
    tSync = std::thread(&SyncController::syncLoop, this);
}

void SyncController::stop() {
    running = false;
    t1.join();
    t2.join();
    tSync.join();
    {
        std::lock_guard<std::mutex> lock(writerMutex);
        writerStopping = true;
    }
    writerCv.notify_all();
    tWriter.join();
}

bool SyncController::triggerBoth() {
    // 同时触发两个相机，确保时间间隔最小化
    bool result1 = cam1->triggerOnce();
    bool result2 = cam2->triggerOnce();
    
    return result1 && result2;
}

SyncController::SyncStats SyncController::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex);
    return stats;
}

void SyncController::requestSavePairs(int count) {
    if (count > 0) {
        pendingSavePairs.fetch_add(count);
    }
}

void SyncController::grabLoop(CameraBase* cam, std::queue<Frame>& q, std::mutex& mtx) {
    while (running) {
        try {
            Frame f;
            if (cam->grab(f)) {
                std::lock_guard<std::mutex> lk(mtx);
                q.push(std::move(f));
                while (q.size() > MAX_PENDING_FRAMES_PER_CAMERA) {
                    q.pop();
                    std::lock_guard<std::mutex> statsLock(statsMutex);
                    stats.droppedFrames++;
                }
            } else {
                // 如果grab失败，稍微等待一下
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Exception in grabLoop: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[ERROR] Unknown exception in grabLoop" << std::endl;
        }
    }
}

void SyncController::syncLoop() {
    while (running) {
        try {
            // 检查队列是否有数据
            bool hasFrames = false;
            {
                std::lock_guard<std::mutex> lk1(mtx1);
                std::lock_guard<std::mutex> lk2(mtx2);
                hasFrames = (!q1.empty() && !q2.empty());
            }
            
            if (!hasFrames) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            // 获取队列前端的时间戳进行比较（不复制整个帧）
            uint64_t ts1, ts2;
            {
                std::lock_guard<std::mutex> lk1(mtx1);
                std::lock_guard<std::mutex> lk2(mtx2);
                
                if (q1.empty() || q2.empty()) {
                    continue;
                }
                
                ts1 = q1.front().timestamp;
                ts2 = q2.front().timestamp;
            }

            int64_t dt = std::llabs((int64_t)ts1 - (int64_t)ts2);
            double dtMs = dt / 1e6;  // 转换为毫秒

            if (dt < SYNC_THRESHOLD_NS) {
                bool shouldSave = false;
                if (maxPairsToSave > 0) {
                    std::lock_guard<std::mutex> statsLock(statsMutex);
                    shouldSave = stats.matchedPairs < maxPairsToSave;
                } else if (pendingSavePairs.load() > 0) {
                    shouldSave = true;
                }

                // 匹配成功，从队列中取出帧。常驻模式下没有保存请求时只丢弃匹配帧，保持队列最新。
                Frame f1, f2;
                {
                    std::lock_guard<std::mutex> lk1(mtx1);
                    std::lock_guard<std::mutex> lk2(mtx2);
                    
                    if (q1.empty() || q2.empty()) {
                        continue;
                    }
                    
                    // 移动语义，避免复制
                    f1 = std::move(q1.front());
                    f2 = std::move(q2.front());
                    q1.pop();
                    q2.pop();
                }

                if (!shouldSave) {
                    continue;
                }

                if (maxPairsToSave <= 0) {
                    int expected = pendingSavePairs.load();
                    while (expected > 0 && !pendingSavePairs.compare_exchange_weak(expected, expected - 1)) {
                    }
                    if (expected <= 0) {
                        continue;
                    }
                }
                
                syncPairCount++;
                
                // 更新统计信息
                {
                    std::lock_guard<std::mutex> statsLock(statsMutex);
                    stats.matchedPairs++;
                    totalTimeDiff += dtMs;
                    stats.avgTimeDiff = totalTimeDiff / stats.matchedPairs;
                    stats.maxTimeDiff = std::max(stats.maxTimeDiff, dtMs);
                    stats.minTimeDiff = std::min(stats.minTimeDiff, dtMs);
                }
                
                PendingPair pair;
                pair.index = syncPairCount;
                pair.vis = std::move(f1);
                pair.ir = std::move(f2);
                pair.pairDeltaMs = dtMs;
                {
                    std::lock_guard<std::mutex> writerLock(writerMutex);
                    if (writerQueue.size() >= MAX_PENDING_PAIRS_TO_WRITE) {
                        writerQueue.pop();
                        std::lock_guard<std::mutex> statsLock(statsMutex);
                        stats.writerDroppedPairs++;
                    }
                    writerQueue.push(std::move(pair));
                }
                writerCv.notify_one();
            } else {
                // 时间戳差异过大，丢弃较旧的帧
                {
                    std::lock_guard<std::mutex> statsLock(statsMutex);
                    stats.droppedFrames++;
                }
                
                {
                    std::lock_guard<std::mutex> lk1(mtx1);
                    std::lock_guard<std::mutex> lk2(mtx2);
                    
                    if (q1.empty() || q2.empty()) {
                        continue;
                    }
                    
                    if (ts1 < ts2) {
                        q1.pop();
                    } else {
                        q2.pop();
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Exception in syncLoop: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[ERROR] Unknown exception in syncLoop" << std::endl;
        }
    }
}

void SyncController::writerLoop() {
    while (true) {
        PendingPair pair;
        {
            std::unique_lock<std::mutex> lock(writerMutex);
            writerCv.wait(lock, [this] { return writerStopping || !writerQueue.empty(); });
            if (writerQueue.empty()) {
                if (writerStopping) {
                    break;
                }
                continue;
            }
            pair = std::move(writerQueue.front());
            writerQueue.pop();
        }

        const std::string prefix = saveDir + "/sync_pair_" +
            std::to_string(pair.index) + "_" + sceneModeStr;
        const std::string visFilename = prefix + "_vis.png";
        const std::string irFilename = prefix + "_ir.png";
        const bool visSaved = pair.vis.channels == 3
            ? ImageSaver::saveBGR(visFilename, pair.vis)
            : ImageSaver::saveGray(visFilename, pair.vis);
        const bool irSaved = pair.ir.channels == 1
            ? ImageSaver::saveGray(irFilename, pair.ir)
            : ImageSaver::saveBGR(irFilename, pair.ir);

        if (!visSaved || !irSaved) {
            std::cerr << "[Writer] 保存同步帧对失败: " << pair.index << std::endl;
            continue;
        }

        std::ofstream logFile(saveDir + "/sync.log", std::ios::app);
        logFile << "Pair " << pair.index << " (" << sceneModeStr << "): "
                << "VIS_host_entry_ns=" << pair.vis.timestamp << ", "
                << "IR_host_entry_ns=" << pair.ir.timestamp << ", "
                << "host_pair_delta_ms=" << pair.pairDeltaMs << ", "
                << "VIS_host_processing_ms="
                << (pair.vis.hostCallbackEndTimestampNs >= pair.vis.timestamp
                    ? (pair.vis.hostCallbackEndTimestampNs - pair.vis.timestamp) / 1e6
                    : 0.0) << ", "
                << "IR_host_processing_ms="
                << (pair.ir.hostCallbackEndTimestampNs >= pair.ir.timestamp
                    ? (pair.ir.hostCallbackEndTimestampNs - pair.ir.timestamp) / 1e6
                    : 0.0) << ", "
                << "VIS_device_timestamp_us=" << pair.vis.deviceTimestampUs << ", "
                << "VIS_exposure_us=" << pair.vis.exposureTimeUs << "\n";
        logFile.close();

        std::lock_guard<std::mutex> statsLock(statsMutex);
        stats.savedPairs++;
    }
}
