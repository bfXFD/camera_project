#include "storage/ImageSaver.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>

// stb_image_write - single header library for writing images
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace {

double applySoftGain(double value, double gain) {
    if (value <= 0.0) {
        return 0.0;
    }

    // 保持0和255端点不变，提升中低亮度时不会把大量高光直接截断到255。
    return 255.0 * gain * value / (255.0 + (gain - 1.0) * value);
}

double solveSoftGain(const std::array<size_t, 256>& histogram, size_t pixelCount, double targetMean) {
    double low = 0.1;
    double high = 32.0;

    for (int iteration = 0; iteration < 24; iteration++) {
        const double gain = (low + high) / 2.0;
        double sum = 0.0;
        for (size_t value = 0; value < histogram.size(); value++) {
            sum += applySoftGain(value, gain) * histogram[value];
        }

        const double correctedMean = pixelCount > 0 ? sum / pixelCount : 0.0;
        if (correctedMean < targetMean) {
            low = gain;
        } else {
            high = gain;
        }
    }

    return (low + high) / 2.0;
}

} // namespace

bool ImageSaver::saveBGR(const std::string& filename, const Frame& frame) {
    // Validate filename
    if (filename.empty()) {
        std::cerr << "ImageSaver::saveBGR - Empty filename" << std::endl;
        return false;
    }
    
    // Validate frame data integrity
    if (frame.channels != 3) {
        std::cerr << "ImageSaver::saveBGR - Expected 3 channels (BGR), got " 
                  << frame.channels << std::endl;
        return false;
    }
    
    int expectedSize = frame.width * frame.height * frame.channels;
    if (frame.data.size() != expectedSize) {
        std::cerr << "ImageSaver::saveBGR - Data size mismatch. Expected " 
                  << expectedSize << ", got " << frame.data.size() << std::endl;
        return false;
    }
    
    if (frame.width <= 0 || frame.height <= 0) {
        std::cerr << "ImageSaver::saveBGR - Invalid dimensions: " 
                  << frame.width << "x" << frame.height << std::endl;
        return false;
    }
    
    // 统计有效像素的RGB均值，避免纯黑和过曝区域干扰灰世界白平衡。
    double redSum = 0.0;
    double greenSum = 0.0;
    double blueSum = 0.0;
    std::array<size_t, 256> redHistogram{};
    std::array<size_t, 256> greenHistogram{};
    std::array<size_t, 256> blueHistogram{};
    size_t validPixels = 0;
    for (size_t i = 0; i < frame.data.size(); i += 3) {
        const uint8_t blue = frame.data[i];
        const uint8_t green = frame.data[i + 1];
        const uint8_t red = frame.data[i + 2];
        const int brightness = std::max({static_cast<int>(red), static_cast<int>(green), static_cast<int>(blue)});
        if (brightness > 8 && brightness < 245) {
            redSum += red;
            greenSum += green;
            blueSum += blue;
            redHistogram[red]++;
            greenHistogram[green]++;
            blueHistogram[blue]++;
            validPixels++;
        }
    }

    double redGain = 1.0;
    double greenGain = 1.0;
    double blueGain = 1.0;
    const char* autoCorrection = std::getenv("VIS_AUTO_COLOR_CORRECTION");
    // 默认使用相机ISP中的固定白平衡增益，不再进行保存端二次校正。
    // 仅在显式设置 VIS_AUTO_COLOR_CORRECTION=1 时启用灰世界诊断校正。
    const bool enableAutoCorrection = autoCorrection != nullptr && std::string(autoCorrection) == "1";

    if (enableAutoCorrection && validPixels > 0) {
        const double redMean = redSum / validPixels;
        const double greenMean = greenSum / validPixels;
        const double blueMean = blueSum / validPixels;
        const double targetMean = (redMean + greenMean + blueMean) / 3.0;

        // 使用保高光的非线性增益，并迭代求解各通道达到目标均值所需的增益。
        redGain = solveSoftGain(redHistogram, validPixels, targetMean);
        greenGain = solveSoftGain(greenHistogram, validPixels, targetMean);
        blueGain = solveSoftGain(blueHistogram, validPixels, targetMean);

        std::cout << "[VIS Color Correction] mean RGB="
                  << redMean << "," << greenMean << "," << blueMean
                  << " gain RGB=" << redGain << "," << greenGain << "," << blueGain
                  << std::endl;
    }

    // stb_image_write expects RGB, but the SDK output is BGR.
    std::vector<uint8_t> rgbData(frame.data.size());
    for (size_t i = 0; i < frame.data.size(); i += 3) {
        rgbData[i] = static_cast<uint8_t>(std::clamp(applySoftGain(frame.data[i + 2], redGain), 0.0, 255.0));
        rgbData[i + 1] = static_cast<uint8_t>(std::clamp(applySoftGain(frame.data[i + 1], greenGain), 0.0, 255.0));
        rgbData[i + 2] = static_cast<uint8_t>(std::clamp(applySoftGain(frame.data[i], blueGain), 0.0, 255.0));
    }
    
    // Determine file format from extension
    int result = 0;
    if (filename.find(".png") != std::string::npos) {
        result = stbi_write_png(filename.c_str(), frame.width, frame.height, 
                                frame.channels, rgbData.data(), 
                                frame.width * frame.channels);
    } else if (filename.find(".jpg") != std::string::npos || 
               filename.find(".jpeg") != std::string::npos) {
        result = stbi_write_jpg(filename.c_str(), frame.width, frame.height, 
                                frame.channels, rgbData.data(), 90);
    } else {
        // Default to PNG
        result = stbi_write_png(filename.c_str(), frame.width, frame.height, 
                                frame.channels, rgbData.data(), 
                                frame.width * frame.channels);
    }
    
    if (result == 0) {
        std::cerr << "ImageSaver::saveBGR - Failed to write file: " 
                  << filename << std::endl;
        return false;
    }
    
    return true;
}

bool ImageSaver::saveChannelPermutation(const std::string& filename, const Frame& frame,
                                         int redIndex, int greenIndex, int blueIndex) {
    if (filename.empty() || frame.channels != 3 || frame.width <= 0 || frame.height <= 0) {
        std::cerr << "ImageSaver::saveChannelPermutation - Invalid image parameters" << std::endl;
        return false;
    }

    const int expectedSize = frame.width * frame.height * frame.channels;
    if (frame.data.size() != static_cast<size_t>(expectedSize)) {
        std::cerr << "ImageSaver::saveChannelPermutation - Data size mismatch" << std::endl;
        return false;
    }

    if (redIndex < 0 || redIndex > 2 || greenIndex < 0 || greenIndex > 2 ||
        blueIndex < 0 || blueIndex > 2 || redIndex == greenIndex ||
        redIndex == blueIndex || greenIndex == blueIndex) {
        std::cerr << "ImageSaver::saveChannelPermutation - Invalid channel permutation" << std::endl;
        return false;
    }

    std::vector<uint8_t> rgbData(frame.data.size());
    for (size_t i = 0; i < frame.data.size(); i += 3) {
        rgbData[i] = frame.data[i + redIndex];
        rgbData[i + 1] = frame.data[i + greenIndex];
        rgbData[i + 2] = frame.data[i + blueIndex];
    }

    return stbi_write_png(filename.c_str(), frame.width, frame.height, 3,
                          rgbData.data(), frame.width * 3) != 0;
}

bool ImageSaver::saveGray(const std::string& filename, const Frame& frame) {
    // Validate filename
    if (filename.empty()) {
        std::cerr << "ImageSaver::saveGray - Empty filename" << std::endl;
        return false;
    }
    
    // Validate frame data integrity
    if (frame.channels != 1) {
        std::cerr << "ImageSaver::saveGray - Expected 1 channel (grayscale), got " 
                  << frame.channels << std::endl;
        return false;
    }
    
    int expectedSize = frame.width * frame.height * frame.channels;
    if (frame.data.size() != expectedSize) {
        std::cerr << "ImageSaver::saveGray - Data size mismatch. Expected " 
                  << expectedSize << ", got " << frame.data.size() << std::endl;
        return false;
    }
    
    if (frame.width <= 0 || frame.height <= 0) {
        std::cerr << "ImageSaver::saveGray - Invalid dimensions: " 
                  << frame.width << "x" << frame.height << std::endl;
        return false;
    }
    
    // Determine file format from extension
    int result = 0;
    if (filename.find(".png") != std::string::npos) {
        result = stbi_write_png(filename.c_str(), frame.width, frame.height, 
                                frame.channels, frame.data.data(), 
                                frame.width * frame.channels);
    } else if (filename.find(".jpg") != std::string::npos || 
               filename.find(".jpeg") != std::string::npos) {
        result = stbi_write_jpg(filename.c_str(), frame.width, frame.height, 
                                frame.channels, frame.data.data(), 90);
    } else {
        // Default to PNG
        result = stbi_write_png(filename.c_str(), frame.width, frame.height, 
                                frame.channels, frame.data.data(), 
                                frame.width * frame.channels);
    }
    
    if (result == 0) {
        std::cerr << "ImageSaver::saveGray - Failed to write file: " 
                  << filename << std::endl;
        return false;
    }
    
    return true;
}
