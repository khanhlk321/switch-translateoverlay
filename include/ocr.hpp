#pragma once

#include <stdint.h>

#include <string>

std::string recognizeText(const uint8_t* rgba, int width, int height, bool cropLeftPanel);
