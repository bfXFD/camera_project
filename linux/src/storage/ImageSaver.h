#pragma once

#include "camera/CameraBase.h"
#include <string>

class ImageSaver {
public:
    // Save BGR image (3 channels) to file
    static bool saveBGR(const std::string& filename, const Frame& frame);

    // Save a 3-channel frame using selected source channels as output R, G and B.
    static bool saveChannelPermutation(const std::string& filename, const Frame& frame,
                                       int redIndex, int greenIndex, int blueIndex);
    
    // Save grayscale image (1 channel) to file
    static bool saveGray(const std::string& filename, const Frame& frame);
};
