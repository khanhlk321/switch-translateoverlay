#include "capture.hpp"

#include <switch.h>

#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_SIMD
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cstdio>

namespace {

constexpr s64 kTimeoutNs = 500000000;
constexpr size_t kJpegBytes = 0x100000;

std::string code(Result rc) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "0x%08X", rc);
    return buf;
}

bool decodeJpeg(const uint8_t* jpeg, size_t jpegSize, CapturedFrame& frame, std::string& error) {
    int width = 0;
    int height = 0;
    int components = 0;
    uint8_t* pixels = stbi_load_from_memory(jpeg, static_cast<int>(jpegSize), &width, &height, &components, 4);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        error = "JPEG decode failed";
        return false;
    }
    frame.width = width;
    frame.height = height;
    frame.rgba.assign(pixels, pixels + static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    stbi_image_free(pixels);
    return true;
}

bool captureJpeg(CapturedFrame& frame, std::string& error) {
    std::vector<uint8_t> jpeg(kJpegBytes);
    u64 jpegSize = 0;
    Result rc = capsscCaptureJpegScreenShot(&jpegSize, jpeg.data(), jpeg.size(), ViLayerStack_Screenshot, kTimeoutNs);
    if (R_FAILED(rc) || jpegSize == 0 || jpegSize > jpeg.size()) {
        error = "JPEG capture failed " + code(rc);
        return false;
    }
    return decodeJpeg(jpeg.data(), static_cast<size_t>(jpegSize), frame, error);
}

bool captureRaw(CapturedFrame& frame, std::string& error) {
    u64 dataSize = 0;
    u64 width = 0;
    u64 height = 0;
    Result rc = capsscOpenRawScreenShotReadStream(&dataSize, &width, &height, ViLayerStack_Screenshot, kTimeoutNs);
    if (R_FAILED(rc)) {
        error = "raw capture failed " + code(rc);
        return false;
    }

    if (width == 0 || height == 0 || width > 1920 || height > 1080 || dataSize == 0 || dataSize > 16 * 1024 * 1024) {
        capsscCloseRawScreenShotReadStream();
        error = "raw capture had an unexpected size";
        return false;
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(dataSize));
    u64 offset = 0;
    while (offset < dataSize) {
        u64 got = 0;
        rc = capsscReadRawScreenShotReadStream(&got, buffer.data() + offset, static_cast<size_t>(dataSize - offset), offset);
        if (R_FAILED(rc) || got == 0) {
            capsscCloseRawScreenShotReadStream();
            error = "raw capture read failed " + code(rc);
            return false;
        }
        offset += got;
    }
    capsscCloseRawScreenShotReadStream();

    const size_t pixels = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    if (offset < pixels) {
        error = "raw capture was shorter than the image";
        return false;
    }
    frame.width = static_cast<int>(width);
    frame.height = static_cast<int>(height);
    frame.rgba.assign(buffer.begin(), buffer.begin() + pixels);
    return true;
}

}

bool captureServicesStart(std::string& error) {
    Result rc = capsscInitialize();
    if (R_FAILED(rc)) {
        error = "caps:sc init failed " + code(rc);
        return false;
    }
    return true;
}

void captureServicesStop() {
    capsscExit();
}

bool captureFrame(CapturedFrame& frame, std::string& error) {
    std::string jpegError;
    if (captureJpeg(frame, jpegError))
        return true;
    std::string rawError;
    if (captureRaw(frame, rawError))
        return true;
    error = jpegError + ". " + rawError + ". On firmware before 10.0.0, screenshot capture requires debug mode.";
    return false;
}
