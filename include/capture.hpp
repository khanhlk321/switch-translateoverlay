#pragma once

#include <stdint.h>

#include <string>
#include <vector>

struct CapturedFrame {
    std::vector<uint8_t> rgba;
    int width = 0;
    int height = 0;
};

bool captureServicesStart(std::string& error);
void captureServicesStop();
bool captureFrame(CapturedFrame& frame, std::string& error);
