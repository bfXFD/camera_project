#include "camera/VISCamera.h"
#include "camera/IRCamera.h"
#include "sync/SyncController.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <string>
#include <cstdlib>

// 获取用户选择的采集模式
CaptureSceneMode getUserSceneChoice() {
    std::cout << "\n=== 选择采集模式 ===" << std::endl;
    std::cout << "  1. 有光模式 (light)" << std::endl;
    std::cout << "  2. 无光模式 (dark)" << std::endl;
    std::cout << "请输入选择 (1 或 2): ";
    
    std::string input;
    while (true) {
        if (!std::getline(std::cin, input)) {
            std::cerr << "输入已结束，程序退出。" << std::endl;
            std::exit(1);
        }
        if (input == "1") {
            std::cout << "已选择: 有光模式 (light)" << std::endl;
            return CaptureSceneMode::Light;
        } else if (input == "2") {
            std::cout << "已选择: 无光模式 (dark)" << std::endl;
            return CaptureSceneMode::Dark;
        } else {
            std::cout << "无效输入，请输入 1 或 2: ";
        }
    }
}

int main() {
    std::cout << "=== 双相机同步采集程序 (Linux) ===" << std::endl;
    
    // 让用户选择采集模式
    CaptureSceneMode sceneMode = getUserSceneChoice();
    
    // Create camera objects
    VISCamera visCamera;
    IRCamera irCamera;
    
    // 先打开IR相机（USB带宽优先级）
    // 注意：IR相机需要较大带宽(1280x1024 YUYV @30fps ≈ 75MB/s)
    std::cout << "\n[步骤1/4] 打开IR相机..." << std::endl;
    if (!irCamera.open()) {
        std::cerr << "错误: 无法打开IR相机!" << std::endl;
        return 1;
    }
    std::cout << "IR相机已打开" << std::endl;
    
    // 启动IR相机预览
    std::cout << "\n[步骤2/4] 启动IR相机预览..." << std::endl;
    if (!irCamera.start(CaptureMode::Callback)) {
        std::cerr << "错误: 无法启动IR相机!" << std::endl;
        irCamera.close();
        return 1;
    }
    std::cout << "IR相机已启动" << std::endl;
    
    // 等待IR相机稳定后再打开VIS相机
    std::cout << "等待IR相机稳定 (1秒)..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 再打开VIS相机
    std::cout << "\n[步骤3/4] 打开VIS相机..." << std::endl;
    if (!visCamera.open()) {
        std::cerr << "错误: 无法打开VIS相机!" << std::endl;
        irCamera.stop();
        irCamera.close();
        return 1;
    }
    std::cout << "VIS相机已打开" << std::endl;
    
    // 启动VIS相机
    std::cout << "\n[步骤4/4] 启动VIS相机预览..." << std::endl;
    if (!visCamera.start(CaptureMode::Callback)) {
        std::cerr << "错误: 无法启动VIS相机!" << std::endl;
        visCamera.close();
        irCamera.stop();
        irCamera.close();
        return 1;
    }
    std::cout << "VIS相机已启动" << std::endl;
    
    // Create and start SyncController (传入场景模式，自动检测编号)
    std::cout << "\n启动同步控制器..." << std::endl;
    // 同步线程先常驻并清理旧帧，正式采样开始后再请求保存一组。
    SyncController controller(&visCamera, &irCamera, sceneMode);
    controller.start();
    const char* captureSaveDir = std::getenv("CAPTURE_SAVE_DIR");
    std::cout << "同步控制器已启动" << std::endl;
    std::cout << "同步图像对将保存到: "
              << ((captureSaveDir != nullptr && captureSaveDir[0] != '\0') ? captureSaveDir : "captured_images/sync_pairs")
              << "/" << std::endl;
    
    // Wait 1 second
    std::cout << "\nWaiting 1 second before sampling..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "\nStart sampling!\n" << std::endl;
    
    // Record initial statistics
    auto initialStats = controller.getStats();
    int initialPairs = initialStats.matchedPairs;
    controller.requestSavePairs(1);
    
    // Wait for 1 sample in 5 seconds
    std::cout << "Target: Complete 1 sync sample in 5 seconds\n" << std::endl;
    std::cout << std::setw(10) << "Sample#" 
              << std::setw(20) << "Time Diff(ms)" 
              << std::setw(15) << "Status" << std::endl;
    std::cout << std::string(45, '-') << std::endl;
    
    auto startTime = std::chrono::steady_clock::now();
    int targetSamples = 1;
    int lastReportedPairs = initialPairs;
    
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto currentStats = controller.getStats();
        int currentPairs = currentStats.matchedPairs;
        
        // Check for new sync pairs
        if (currentPairs > lastReportedPairs) {
            // Calculate latest time difference
            double latestTimeDiff = currentStats.minTimeDiff;
            if (currentPairs > initialPairs + 1) {
                latestTimeDiff = currentStats.avgTimeDiff;
            }
            
            std::cout << std::setw(10) << (currentPairs - initialPairs)
                      << std::setw(20) << std::fixed << std::setprecision(4) << latestTimeDiff
                      << std::setw(15) << "OK" << std::endl;
            
            lastReportedPairs = currentPairs;
        }
        
        // Check if target samples completed
        if (currentPairs >= initialPairs + targetSamples) {
            std::cout << "\nCompleted " << targetSamples << " sync samples!" << std::endl;
            break;
        }
        
        // Check timeout (5 seconds)
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count();
        if (elapsed >= 5) {
            std::cout << "\nReached 5 second time limit" << std::endl;
            std::cout << "Actual samples completed: " << (currentPairs - initialPairs) << " / " << targetSamples << std::endl;
            break;
        }
    }
    
    // Stop SyncController
    std::cout << "\nStopping sync controller..." << std::endl;
    controller.stop();
    
    // Stop and close cameras
    std::cout << "Stopping cameras..." << std::endl;
    visCamera.stop();
    irCamera.stop();
    
    std::cout << "Closing cameras..." << std::endl;
    visCamera.close();
    irCamera.close();
    
    // Output final statistics
    auto finalStats = controller.getStats();
    std::cout << "\n=== Final Statistics ===" << std::endl;
    std::cout << "Total sync pairs: " << (finalStats.matchedPairs - initialPairs) << std::endl;
    std::cout << "Dropped frames: " << (finalStats.droppedFrames - initialStats.droppedFrames) << std::endl;
    std::cout << "Average time diff: " << std::fixed << std::setprecision(4) << finalStats.avgTimeDiff << " ms" << std::endl;
    std::cout << "Min time diff: " << finalStats.minTimeDiff << " ms" << std::endl;
    std::cout << "Max time diff: " << finalStats.maxTimeDiff << " ms" << std::endl;
    
    if (finalStats.matchedPairs - initialPairs >= targetSamples) {
        std::cout << "\nSampling task completed!" << std::endl;
        return 0;
    } else {
        std::cout << "\nSampling task incomplete" << std::endl;
        return 1;
    }
}
