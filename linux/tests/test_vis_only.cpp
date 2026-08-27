#include "camera/VISCamera.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>

// VIS相机单独测试程序
// 用于在IR相机SDK不可用时测试MindVision相机

int main() {
    std::cout << "=== VIS Camera Test Program (Linux ARM64) ===" << std::endl;
    std::cout << "Testing MindVision VIS camera only..." << std::endl << std::endl;
    
    // Create camera object
    VISCamera visCamera;
    
    // Open VIS camera
    std::cout << "Opening VIS camera..." << std::endl;
    if (!visCamera.open()) {
        std::cerr << "ERROR: Failed to open VIS camera!" << std::endl;
        std::cerr << "Possible causes:" << std::endl;
        std::cerr << "  1. Camera not connected" << std::endl;
        std::cerr << "  2. USB permissions not set (run: sudo ./setup_usb_permissions.sh)" << std::endl;
        std::cerr << "  3. Driver not installed (run: sudo bash ../SDK/CameraSDK/install.sh)" << std::endl;
        return 1;
    }
    std::cout << "VIS camera opened successfully!" << std::endl;
    
    // Start camera (Callback mode)
    std::cout << "\nStarting camera (Callback mode)..." << std::endl;
    if (!visCamera.start(CaptureMode::Callback)) {
        std::cerr << "ERROR: Failed to start VIS camera!" << std::endl;
        visCamera.close();
        return 1;
    }
    std::cout << "VIS camera started" << std::endl;
    
    // Wait for frames
    std::cout << "\nWaiting for frames (5 seconds)..." << std::endl;
    std::cout << std::setw(10) << "Frame#" 
              << std::setw(15) << "Width" 
              << std::setw(15) << "Height"
              << std::setw(15) << "Channels" << std::endl;
    std::cout << std::string(55, '-') << std::endl;
    
    int frameCount = 0;
    auto startTime = std::chrono::steady_clock::now();
    
    while (true) {
        Frame frame;
        if (visCamera.grab(frame)) {
            frameCount++;
            std::cout << std::setw(10) << frameCount
                      << std::setw(15) << frame.width
                      << std::setw(15) << frame.height
                      << std::setw(15) << frame.channels << std::endl;
            
            // 采集10帧后停止
            if (frameCount >= 10) {
                break;
            }
        }
        
        // 超时检查
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count();
        if (elapsed >= 5) {
            std::cout << "\nTimeout reached (5 seconds)" << std::endl;
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Stop and close camera
    std::cout << "\nStopping camera..." << std::endl;
    visCamera.stop();
    
    std::cout << "Closing camera..." << std::endl;
    visCamera.close();
    
    // Summary
    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Total frames captured: " << frameCount << std::endl;
    
    if (frameCount > 0) {
        std::cout << "\nVIS camera test PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "\nVIS camera test FAILED - no frames captured" << std::endl;
        return 1;
    }
}
