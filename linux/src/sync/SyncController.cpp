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
    t1 = std::thread(&SyncController::grabLoop, this, cam1, std::ref(q1), std::ref(mtx1));
    t2 = std::thread(&SyncController::grabLoop, this, cam2, std::ref(q2), std::ref(mtx2));
    tSync = std::thread(&SyncController::syncLoop, this);
}

void SyncController::stop() {
    running = false;
    t1.join();
    t2.join();
    tSync.join();
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
                
                // 文件名格式: sync_pair_<编号>_<场景模式>_<相机类型>.png
                // 例如: sync_pair_1_light_vis.png, sync_pair_1_light_ir.png
                
                // 保存VIS相机图像（假设cam1是VIS相机，3通道BGR）
                std::string visFilename = saveDir + "/sync_pair_" +
                                         std::to_string(syncPairCount) + "_" +
                                         sceneModeStr + "_vis.png";
                if (f1.channels == 3) {
                    ImageSaver::saveBGR(visFilename, f1);

                    const char* channelDiagnostic = std::getenv("VIS_CHANNEL_DIAGNOSTIC");
                    if (channelDiagnostic != nullptr && std::string(channelDiagnostic) == "1") {
                        struct ChannelPermutation {
                            int r;
                            int g;
                            int b;
                            const char* label;
                        };
                        const ChannelPermutation permutations[] = {
                            {0, 1, 2, "C0C1C2"},
                            {0, 2, 1, "C0C2C1"},
                            {1, 0, 2, "C1C0C2"},
                            {1, 2, 0, "C1C2C0"},
                            {2, 0, 1, "C2C0C1"},
                            {2, 1, 0, "C2C1C0"},
                        };

                        for (const auto& permutation : permutations) {
                            const std::string diagnosticFilename = saveDir + "/sync_pair_" +
                                std::to_string(syncPairCount) + "_" + sceneModeStr +
                                "_vis_perm_" + permutation.label + ".png";
                            ImageSaver::saveChannelPermutation(diagnosticFilename, f1,
                                                               permutation.r,
                                                               permutation.g,
                                                               permutation.b);
                        }
                        std::cout << "[Channel Diagnostic] 已保存同一VIS帧的6种通道排列" << std::endl;
                    }
                } else {
                    ImageSaver::saveGray(visFilename, f1);
                }
                
                // 保存IR相机图像（假设cam2是IR相机，1通道灰度）
                std::string irFilename = saveDir + "/sync_pair_" +
                                        std::to_string(syncPairCount) + "_" +
                                        sceneModeStr + "_ir.png";
                if (f2.channels == 1) {
                    ImageSaver::saveGray(irFilename, f2);
                } else {
                    ImageSaver::saveBGR(irFilename, f2);
                }
                
                // 记录日志：时间戳差异（转换为毫秒）
                std::ofstream logFile(saveDir + "/sync.log", std::ios::app);
                logFile << "Pair " << syncPairCount << " (" << sceneModeStr << "): "
                       << "VIS_ts=" << f1.timestamp << ", "
                       << "IR_ts=" << f2.timestamp << ", "
                       << "dt=" << dtMs << " ms\n";
                logFile.close();
                
                // 静默保存，不输出到控制台
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
