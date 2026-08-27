#include "camera/VISCamera.h"
#include "camera/IRCamera.h"
#include "sync/SyncController.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <thread>
#include <termios.h>
#include <unistd.h>

class TerminalRawMode {
public:
    TerminalRawMode() {
        enabled = isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &oldTermios) == 0;
        if (enabled) {
            termios raw = oldTermios;
            raw.c_lflag &= static_cast<unsigned>(~(ICANON | ECHO));
            raw.c_cc[VMIN] = 1;
            raw.c_cc[VTIME] = 0;
            tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        }
    }

    ~TerminalRawMode() {
        if (enabled) {
            tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
        }
    }

private:
    bool enabled = false;
    termios oldTermios{};
};

static CaptureSceneMode getUserSceneChoice(std::string& sceneName) {
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
            sceneName = "light";
            std::cout << "已选择: 有光模式 (light)" << std::endl;
            return CaptureSceneMode::Light;
        }
        if (input == "2") {
            sceneName = "dark";
            std::cout << "已选择: 无光模式 (dark)" << std::endl;
            return CaptureSceneMode::Dark;
        }
        std::cout << "无效输入，请输入 1 或 2: ";
    }
}

static std::string getSaveDir() {
    const char* captureSaveDir = std::getenv("CAPTURE_SAVE_DIR");
    if (captureSaveDir != nullptr && captureSaveDir[0] != '\0') {
        return captureSaveDir;
    }
    return "captured_images/sync_pairs";
}

static int latestPairIndex(const std::string& saveDir, const std::string& sceneName) {
    int maxIndex = 0;
    if (!std::filesystem::exists(saveDir)) {
        return 0;
    }

    std::regex pattern("sync_pair_(\\d+)_" + sceneName + "_(vis|ir)\\.png");
    for (const auto& entry : std::filesystem::directory_iterator(saveDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const std::string filename = entry.path().filename().string();
        std::smatch match;
        if (std::regex_match(filename, match, pattern)) {
            maxIndex = std::max(maxIndex, std::stoi(match[1].str()));
        }
    }
    return maxIndex;
}

static void deleteLatestPair(const std::string& saveDir, const std::string& sceneName) {
    const int index = latestPairIndex(saveDir, sceneName);
    if (index <= 0) {
        std::cout << "没有找到可删除的 " << sceneName << " 采集数据" << std::endl;
        return;
    }

    const std::filesystem::path visFile = std::filesystem::path(saveDir) /
        ("sync_pair_" + std::to_string(index) + "_" + sceneName + "_vis.png");
    const std::filesystem::path irFile = std::filesystem::path(saveDir) /
        ("sync_pair_" + std::to_string(index) + "_" + sceneName + "_ir.png");

    std::error_code ec;
    std::filesystem::remove(visFile, ec);
    std::filesystem::remove(irFile, ec);

    const std::filesystem::path logPath = std::filesystem::path(saveDir) / "sync.log";
    if (std::filesystem::exists(logPath)) {
        const std::filesystem::path tmpPath = std::filesystem::path(saveDir) / "sync.log.tmp";
        std::ifstream in(logPath);
        std::ofstream out(tmpPath);
        const std::string prefix = "Pair " + std::to_string(index) + " (" + sceneName + "):";
        std::string line;
        while (std::getline(in, line)) {
            if (line.rfind(prefix, 0) != 0) {
                out << line << '\n';
            }
        }
        in.close();
        out.close();
        std::filesystem::rename(tmpPath, logPath, ec);
    }

    std::cout << "已删除上一组数据: sync_pair_" << index << "_" << sceneName << "_{vis,ir}.png" << std::endl;
}

int main() {
    std::cout << "=== 双相机长期键盘控制采集程序 (Linux) ===" << std::endl;

    std::string sceneName;
    CaptureSceneMode sceneMode = getUserSceneChoice(sceneName);
    const std::string saveDir = getSaveDir();

    VISCamera visCamera;
    IRCamera irCamera;

    std::cout << "\n[步骤1/4] 打开IR相机..." << std::endl;
    if (!irCamera.open()) {
        std::cerr << "错误: 无法打开IR相机!" << std::endl;
        return 1;
    }
    std::cout << "IR相机已打开" << std::endl;

    std::cout << "\n[步骤2/4] 启动IR相机预览..." << std::endl;
    if (!irCamera.start(CaptureMode::Callback)) {
        std::cerr << "错误: 无法启动IR相机!" << std::endl;
        irCamera.close();
        return 1;
    }
    std::cout << "IR相机已启动" << std::endl;

    std::cout << "等待IR相机稳定 (1秒)..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "\n[步骤3/4] 打开VIS相机..." << std::endl;
    if (!visCamera.open()) {
        std::cerr << "错误: 无法打开VIS相机!" << std::endl;
        irCamera.stop();
        irCamera.close();
        return 1;
    }
    std::cout << "VIS相机已打开" << std::endl;

    std::cout << "\n[步骤4/4] 启动VIS相机预览..." << std::endl;
    if (!visCamera.start(CaptureMode::Callback)) {
        std::cerr << "错误: 无法启动VIS相机!" << std::endl;
        visCamera.close();
        irCamera.stop();
        irCamera.close();
        return 1;
    }
    std::cout << "VIS相机已启动" << std::endl;

    SyncController controller(&visCamera, &irCamera, sceneMode);
    controller.start();

    std::cout << "\n同步控制器已启动" << std::endl;
    std::cout << "同步图像对将保存到: " << saveDir << "/" << std::endl;
    std::cout << "\n按键说明: c=采集一组, d=删除上一组, q=退出" << std::endl;
    std::cout << "采集键有2秒冷却，冷却期间重复按 c 不会生效" << std::endl;

    TerminalRawMode rawMode;
    auto lastCaptureTime = std::chrono::steady_clock::time_point::min();
    bool running = true;

    while (running) {
        const char key = static_cast<char>(getchar());
        switch (key) {
            case 'c':
            case 'C': {
                const auto now = std::chrono::steady_clock::now();
                if (lastCaptureTime != std::chrono::steady_clock::time_point::min() &&
                    now - lastCaptureTime < std::chrono::seconds(2)) {
                    std::cout << "采集按键冷却中，2秒内重复按键不会生效" << std::endl;
                    break;
                }

                const int before = controller.getStats().matchedPairs;
                lastCaptureTime = now;
                controller.requestSavePairs(1);
                std::cout << "开始采集一组..." << std::endl;

                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                while (std::chrono::steady_clock::now() < deadline) {
                    const auto stats = controller.getStats();
                    if (stats.matchedPairs > before) {
                        std::cout << "采集完成: 第 " << stats.matchedPairs
                                  << " 组, 平均时间差 " << stats.avgTimeDiff << " ms" << std::endl;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                if (controller.getStats().matchedPairs <= before) {
                    std::cout << "采集超时: 5秒内没有保存到同步图像对" << std::endl;
                }
                break;
            }
            case 'd':
            case 'D':
                deleteLatestPair(saveDir, sceneName);
                break;
            case 'q':
            case 'Q':
                running = false;
                break;
            default:
                std::cout << "未知按键: " << key << "，可用按键 c/d/q" << std::endl;
                break;
        }
    }

    std::cout << "\nStopping sync controller..." << std::endl;
    controller.stop();

    std::cout << "Stopping cameras..." << std::endl;
    visCamera.stop();
    irCamera.stop();

    std::cout << "Closing cameras..." << std::endl;
    visCamera.close();
    irCamera.close();

    std::cout << "退出长期键盘控制采集" << std::endl;
    return 0;
}
