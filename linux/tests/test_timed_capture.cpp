#include "camera/VISCamera.h"
#include "camera/IRCamera.h"
#include "sync/SyncController.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <poll.h>
#include <string>
#include <termios.h>
#include <thread>
#include <unistd.h>

namespace {

volatile std::sig_atomic_t stopRequested = 0;

class TerminalRawMode {
public:
    TerminalRawMode() {
        enabled = isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &original) == 0;
        if (enabled) {
            termios raw = original;
            raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 0;
            tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        }
    }

    ~TerminalRawMode() {
        if (enabled) {
            tcsetattr(STDIN_FILENO, TCSANOW, &original);
        }
    }

private:
    bool enabled = false;
    termios original{};
};

void handleSignal(int) {
    stopRequested = 1;
}

CaptureSceneMode getUserSceneChoice() {
    std::cout << "\n=== 选择采集模式 ===" << std::endl;
    std::cout << "  1. 有光模式 (light)" << std::endl;
    std::cout << "  2. 无光模式 (dark)" << std::endl;
    std::cout << "请输入选择 (1 或 2): ";

    std::string input;
    while (std::getline(std::cin, input)) {
        if (input == "1") {
            std::cout << "已选择: 有光模式 (light)" << std::endl;
            return CaptureSceneMode::Light;
        }
        if (input == "2") {
            std::cout << "已选择: 无光模式 (dark)" << std::endl;
            return CaptureSceneMode::Dark;
        }
        std::cout << "无效输入，请输入 1 或 2: ";
    }

    std::cerr << "输入已结束，程序退出。" << std::endl;
    std::exit(1);
}

int getCaptureIntervalSeconds() {
    constexpr int defaultInterval = 3;
    const char* value = std::getenv("AUTO_CAPTURE_INTERVAL_SECONDS");
    if (value == nullptr || value[0] == '\0') {
        return defaultInterval;
    }

    try {
        const int interval = std::stoi(value);
        if (interval > 0) {
            return interval;
        }
    } catch (...) {
    }

    std::cerr << "警告: AUTO_CAPTURE_INTERVAL_SECONDS 必须是正整数，使用默认值 3 秒。" << std::endl;
    return defaultInterval;
}

bool quitKeyPressed() {
    pollfd input{};
    input.fd = STDIN_FILENO;
    input.events = POLLIN;

    const int result = poll(&input, 1, 0);
    if (result <= 0 || (input.revents & POLLIN) == 0) {
        return false;
    }

    char key = 0;
    const ssize_t bytesRead = read(STDIN_FILENO, &key, 1);
    return bytesRead == 1 && (key == 'q' || key == 'Q');
}

} // namespace

int main() {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    std::cout << "=== 双相机定时同步采集程序 (Linux) ===" << std::endl;

    const CaptureSceneMode sceneMode = getUserSceneChoice();
    const int intervalSeconds = getCaptureIntervalSeconds();
    const char* captureSaveDir = std::getenv("CAPTURE_SAVE_DIR");
    const std::string saveDir =
        (captureSaveDir != nullptr && captureSaveDir[0] != '\0')
            ? captureSaveDir
            : "captured_images/sync_pairs";

    VISCamera visCamera;
    IRCamera irCamera;
    bool irOpened = false;
    bool irStarted = false;
    bool visOpened = false;
    bool visStarted = false;

    auto closeCameras = [&]() {
        if (visStarted) {
            visCamera.stop();
        }
        if (irStarted) {
            irCamera.stop();
        }
        if (visOpened) {
            visCamera.close();
        }
        if (irOpened) {
            irCamera.close();
        }
    };

    std::cout << "\n[步骤1/4] 打开IR相机..." << std::endl;
    if (!irCamera.open()) {
        std::cerr << "错误: 无法打开IR相机!" << std::endl;
        return 1;
    }
    irOpened = true;

    std::cout << "\n[步骤2/4] 启动IR相机预览..." << std::endl;
    if (!irCamera.start(CaptureMode::Callback)) {
        std::cerr << "错误: 无法启动IR相机!" << std::endl;
        closeCameras();
        return 1;
    }
    irStarted = true;

    std::cout << "等待IR相机稳定 (1秒)..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "\n[步骤3/4] 打开VIS相机..." << std::endl;
    if (!visCamera.open()) {
        std::cerr << "错误: 无法打开VIS相机!" << std::endl;
        closeCameras();
        return 1;
    }
    visOpened = true;

    std::cout << "\n[步骤4/4] 启动VIS相机预览..." << std::endl;
    if (!visCamera.start(CaptureMode::Callback)) {
        std::cerr << "错误: 无法启动VIS相机!" << std::endl;
        closeCameras();
        return 1;
    }
    visStarted = true;

    SyncController controller(&visCamera, &irCamera, sceneMode);
    controller.start();
    TerminalRawMode terminalRawMode;

    std::cout << "\n定时采集已启动" << std::endl;
    std::cout << "采集周期: " << intervalSeconds << " 秒" << std::endl;
    std::cout << "保存目录: " << saveDir << std::endl;
    std::cout << "可以在两次采集之间调整相机位姿；按 q 或 Ctrl+C 安全退出。" << std::endl;

    const auto interval = std::chrono::seconds(intervalSeconds);
    auto nextCapture = std::chrono::steady_clock::now() + interval;
    int requestedPairs = 0;
    int reportedPairs = 0;

    while (!stopRequested) {
        if (quitKeyPressed()) {
            break;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= nextCapture) {
            controller.requestSavePairs(1);
            requestedPairs++;
            std::cout << "[定时采集] 已请求第 " << requestedPairs << " 组同步图像" << std::endl;

            do {
                nextCapture += interval;
            } while (nextCapture <= now);
        }

        const auto stats = controller.getStats();
        if (stats.matchedPairs > reportedPairs) {
            reportedPairs = stats.matchedPairs;
            std::cout << "[保存完成] 第 " << reportedPairs
                      << " 组，平均时间差 " << stats.avgTimeDiff << " ms" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cout << "\n正在停止同步控制器和相机..." << std::endl;
    controller.stop();
    closeCameras();

    const auto finalStats = controller.getStats();
    std::cout << "定时采集结束，共保存 " << finalStats.matchedPairs << " 组同步图像。" << std::endl;
    std::cout << "丢弃帧数: " << finalStats.droppedFrames << std::endl;
    if (finalStats.matchedPairs > 0) {
        std::cout << "平均同步时间差: " << finalStats.avgTimeDiff << " ms" << std::endl;
    }
    return 0;
}
