#include "ocr.hpp"

#include <switch.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <algorithm>
#include <vector>

namespace {

constexpr int kNorm = 24;
constexpr char kAlphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.,!?'-:";

struct GlyphTemplate {
    char ch;
    std::vector<uint8_t> ink;
};

std::vector<GlyphTemplate> g_templates;
bool g_ready = false;

void normalize(const std::vector<uint8_t>& src, int sw, int sh, std::vector<uint8_t>& dst) {
    dst.assign(kNorm * kNorm, 0);
    if (sw < 1 || sh < 1)
        return;
    int dw = 0;
    int dh = 0;
    if (sw >= sh) {
        dw = kNorm - 4;
        dh = std::max(1, (kNorm - 4) * sh / sw);
    } else {
        dh = kNorm - 4;
        dw = std::max(1, (kNorm - 4) * sw / sh);
    }
    const int ox = (kNorm - dw) / 2;
    const int oy = (kNorm - dh) / 2;
    for (int y = 0; y < dh; y++) {
        const int sy = y * sh / dh;
        for (int x = 0; x < dw; x++) {
            const int sx = x * sw / dw;
            if (src[static_cast<size_t>(sy * sw + sx)])
                dst[static_cast<size_t>((oy + y) * kNorm + (ox + x))] = 1;
        }
    }
}

void buildTemplates() {
    if (g_ready)
        return;

    PlFontData fontData = {};
    if (R_FAILED(plGetSharedFontByType(&fontData, PlSharedFontType_Standard)))
        return;

    stbtt_fontinfo font;
    const unsigned char* bytes = static_cast<const unsigned char*>(fontData.address);
    if (!stbtt_InitFont(&font, bytes, stbtt_GetFontOffsetForIndex(bytes, 0)))
        return;

    const float scale = stbtt_ScaleForPixelHeight(&font, 32.0f);
    for (const char* p = kAlphabet; *p != '\0'; p++) {
        int width = 0;
        int height = 0;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, scale, scale, static_cast<int>(*p), &width, &height, nullptr, nullptr);
        if (bitmap == nullptr || width < 1 || height < 1) {
            if (bitmap != nullptr)
                stbtt_FreeBitmap(bitmap, nullptr);
            continue;
        }

        int x0 = width;
        int y0 = height;
        int x1 = 0;
        int y1 = 0;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (bitmap[y * width + x] > 40) {
                    x0 = std::min(x0, x);
                    y0 = std::min(y0, y);
                    x1 = std::max(x1, x + 1);
                    y1 = std::max(y1, y + 1);
                }
            }
        }
        if (x1 <= x0 || y1 <= y0) {
            stbtt_FreeBitmap(bitmap, nullptr);
            continue;
        }

        std::vector<uint8_t> cropped(static_cast<size_t>((x1 - x0) * (y1 - y0)), 0);
        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++)
                cropped[static_cast<size_t>((y - y0) * (x1 - x0) + (x - x0))] = bitmap[y * width + x] > 40 ? 1 : 0;
        }
        stbtt_FreeBitmap(bitmap, nullptr);

        GlyphTemplate glyph;
        glyph.ch = *p;
        normalize(cropped, x1 - x0, y1 - y0, glyph.ink);
        g_templates.push_back(std::move(glyph));
    }
    if (!g_templates.empty())
        g_ready = true;
}

float intersectionOverUnion(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    int inter = 0;
    int uni = 0;
    const int n = kNorm * kNorm;
    for (int i = 0; i < n; i++) {
        const bool left = a[static_cast<size_t>(i)] != 0;
        const bool right = b[static_cast<size_t>(i)] != 0;
        if (left && right)
            inter++;
        if (left || right)
            uni++;
    }
    if (uni == 0)
        return 0.0f;
    return static_cast<float>(inter) / static_cast<float>(uni);
}

char classifyBox(const std::vector<uint8_t>& ink, int width, int height, float& score) {
    std::vector<uint8_t> sample;
    normalize(ink, width, height, sample);
    score = 0.0f;
    char best = '?';
    for (const GlyphTemplate& glyph : g_templates) {
        const float iou = intersectionOverUnion(glyph.ink, sample);
        if (iou > score) {
            score = iou;
            best = glyph.ch;
        }
    }
    if (score < 0.28f)
        return '?';
    return best;
}

int otsu(const std::vector<uint8_t>& gray) {
    int hist[256] = {};
    for (uint8_t value : gray)
        hist[value]++;
    const int total = static_cast<int>(gray.size());
    if (total == 0)
        return 128;

    float sum = 0.0f;
    for (int i = 0; i < 256; i++)
        sum += static_cast<float>(i * hist[i]);

    float sumBack = 0.0f;
    int weightBack = 0;
    float best = -1.0f;
    int threshold = 128;
    for (int i = 0; i < 256; i++) {
        weightBack += hist[i];
        if (weightBack == 0)
            continue;
        const int weightFore = total - weightBack;
        if (weightFore == 0)
            break;
        sumBack += static_cast<float>(i * hist[i]);
        const float meanBack = sumBack / static_cast<float>(weightBack);
        const float meanFore = (sum - sumBack) / static_cast<float>(weightFore);
        const float diff = meanBack - meanFore;
        const float variance = static_cast<float>(weightBack) * static_cast<float>(weightFore) * diff * diff;
        if (variance > best) {
            best = variance;
            threshold = i;
        }
    }
    return threshold;
}

struct Box {
    int x0, y0, x1, y1;
};

std::string readMask(const std::vector<uint8_t>& mask, int width, int height, float& quality) {
    std::vector<int> rowInk(static_cast<size_t>(height), 0);
    for (int y = 0; y < height; y++) {
        int count = 0;
        const uint8_t* row = mask.data() + static_cast<size_t>(y * width);
        for (int x = 0; x < width; x++)
            count += row[x] ? 1 : 0;
        rowInk[static_cast<size_t>(y)] = count;
    }

    std::string text;
    float scoreSum = 0.0f;
    int scoreCount = 0;
    int y = 0;
    while (y < height) {
        while (y < height && rowInk[static_cast<size_t>(y)] < std::max(2, width / 250))
            y++;
        if (y >= height)
            break;
        const int y0 = y;
        while (y < height && rowInk[static_cast<size_t>(y)] >= std::max(2, width / 250))
            y++;
        const int y1 = y;
        const int lineH = y1 - y0;
        if (lineH < 6 || lineH > 72)
            continue;

        std::vector<int> colInk(static_cast<size_t>(width), 0);
        for (int yy = y0; yy < y1; yy++) {
            const uint8_t* row = mask.data() + static_cast<size_t>(yy * width);
            for (int x = 0; x < width; x++)
                colInk[static_cast<size_t>(x)] += row[x] ? 1 : 0;
        }

        std::vector<Box> glyphs;
        int x = 0;
        while (x < width) {
            while (x < width && colInk[static_cast<size_t>(x)] == 0)
                x++;
            if (x >= width)
                break;
            const int x0 = x;
            while (x < width && colInk[static_cast<size_t>(x)] != 0)
                x++;
            const int x1 = x;
            if (x1 - x0 >= 2 && x1 - x0 < lineH * 2)
                glyphs.push_back({x0, y0, x1, y1});
        }
        if (glyphs.size() < 2)
            continue;

        std::vector<int> widths;
        widths.reserve(glyphs.size());
        for (const Box& glyph : glyphs)
            widths.push_back(glyph.x1 - glyph.x0);
        std::sort(widths.begin(), widths.end());
        const int median = std::max(1, widths[widths.size() / 2]);

        if (!text.empty())
            text.push_back('\n');
        int previousRight = glyphs.front().x0;
        for (const Box& glyph : glyphs) {
            const int gap = glyph.x0 - previousRight;
            if (gap > median / 2 && !text.empty() && text.back() != '\n')
                text.push_back(' ');
            previousRight = glyph.x1;

            std::vector<uint8_t> ink(static_cast<size_t>((glyph.x1 - glyph.x0) * (glyph.y1 - glyph.y0)), 0);
            for (int yy = glyph.y0; yy < glyph.y1; yy++) {
                for (int xx = glyph.x0; xx < glyph.x1; xx++) {
                    if (mask[static_cast<size_t>(yy * width + xx)])
                        ink[static_cast<size_t>((yy - glyph.y0) * (glyph.x1 - glyph.x0) + (xx - glyph.x0))] = 1;
                }
            }
            float score = 0.0f;
            const char ch = classifyBox(ink, glyph.x1 - glyph.x0, glyph.y1 - glyph.y0, score);
            text.push_back(ch);
            scoreSum += score;
            scoreCount++;
        }
    }
    quality = scoreCount == 0 ? 0.0f : scoreSum / static_cast<float>(scoreCount);
    return text;
}

int scoreText(const std::string& text) {
    int letters = 0;
    int unknown = 0;
    for (unsigned char c : text) {
        if (c == '?')
            unknown++;
        else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            letters++;
    }
    return letters - unknown * 2;
}

}

std::string recognizeText(const uint8_t* rgba, int width, int height, bool cropLeftPanel) {
    buildTemplates();
    if (g_templates.empty())
        return {};
    if (rgba == nullptr || width < 16 || height < 16)
        return {};

    int xBegin = 0;
    if (cropLeftPanel)
        xBegin = width * 36 / 100;
    const int cropW = width - xBegin;
    if (cropW < 16)
        return {};

    int step = 1;
    while (cropW / step > 480)
        step++;

    const int smallW = cropW / step;
    const int smallH = height / step;
    std::vector<uint8_t> gray(static_cast<size_t>(smallW * smallH), 0);
    for (int y = 0; y < smallH; y++) {
        for (int x = 0; x < smallW; x++) {
            const int sx = xBegin + x * step;
            const int sy = y * step;
            const uint8_t* p = rgba + (static_cast<size_t>(sy * width + sx) * 4);
            gray[static_cast<size_t>(y * smallW + x)] = static_cast<uint8_t>((p[0] * 30 + p[1] * 59 + p[2] * 11) / 100);
        }
    }

    const int threshold = otsu(gray);
    std::vector<uint8_t> dark(gray.size(), 0);
    std::vector<uint8_t> light(gray.size(), 0);
    for (size_t i = 0; i < gray.size(); i++) {
        dark[i] = gray[i] < threshold ? 1 : 0;
        light[i] = gray[i] > threshold ? 1 : 0;
    }

    float darkQuality = 0.0f;
    float lightQuality = 0.0f;
    const std::string darkText = readMask(dark, smallW, smallH, darkQuality);
    const std::string lightText = readMask(light, smallW, smallH, lightQuality);
    const int darkScore = scoreText(darkText);
    const int lightScore = scoreText(lightText);
    if (lightScore != darkScore)
        return lightScore > darkScore ? lightText : darkText;
    return lightQuality > darkQuality ? lightText : darkText;
}
