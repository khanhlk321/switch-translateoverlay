#include "ocr.hpp"

#include <switch.h>

#include <stb_truetype.h>

#include <algorithm>
#include <vector>

namespace {

constexpr int kNorm = 24;
constexpr char kAlphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.,!?'-:";

struct GlyphTemplate {
    uint32_t codepoint;
    std::vector<uint8_t> ink;
};

std::vector<GlyphTemplate> g_latin;
std::vector<GlyphTemplate> g_kana;
bool g_ready = false;
bool g_matchJapanese = false;

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

void addGlyph(stbtt_fontinfo& font, float scale, uint32_t codepoint, std::vector<GlyphTemplate>& dest) {
    int width = 0;
    int height = 0;
    unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, scale, scale, static_cast<int>(codepoint), &width, &height, nullptr, nullptr);
    if (bitmap == nullptr || width < 1 || height < 1) {
        if (bitmap != nullptr)
            stbtt_FreeBitmap(bitmap, nullptr);
        return;
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
        return;
    }

    std::vector<uint8_t> cropped(static_cast<size_t>((x1 - x0) * (y1 - y0)), 0);
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++)
            cropped[static_cast<size_t>((y - y0) * (x1 - x0) + (x - x0))] = bitmap[y * width + x] > 40 ? 1 : 0;
    }
    stbtt_FreeBitmap(bitmap, nullptr);

    GlyphTemplate glyph;
    glyph.codepoint = codepoint;
    normalize(cropped, x1 - x0, y1 - y0, glyph.ink);
    dest.push_back(std::move(glyph));
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
    for (const char* p = kAlphabet; *p != '\0'; p++)
        addGlyph(font, scale, static_cast<uint32_t>(*p), g_latin);
    for (uint32_t codepoint = '0'; codepoint <= '9'; codepoint++)
        addGlyph(font, scale, codepoint, g_kana);

    for (uint32_t codepoint = 0x3041; codepoint <= 0x3096; codepoint++)
        addGlyph(font, scale, codepoint, g_kana);
    for (uint32_t codepoint = 0x30A1; codepoint <= 0x30FA; codepoint++)
        addGlyph(font, scale, codepoint, g_kana);
    addGlyph(font, scale, 0x30FC, g_kana);
    const uint32_t kanji[] = {
        0x7D42, 0x4E86, 0x8A2D, 0x5B9A, 0x672C, 0x5F53, 0x5F85, 0x62BC, 0x7D9A, 0x65B0,
        0x8CB7, 0x58F2, 0x653B, 0x6483, 0x9632, 0x5FA1, 0x6575, 0x52DD, 0x5229, 0x6557,
        0x5317, 0x78BA, 0x8A8D, 0x524A, 0x9664, 0x9078, 0x629E, 0x97F3, 0x91CF, 0x8A00,
        0x8A9E, 0x8AAD, 0x8FBC, 0x4E2D, 0x8B66, 0x544A, 0x5B8C, 0x5931, 0x6210, 0x529F,
        0x9375, 0x8A71, 0x79C1, 0x76EE, 0x6A19, 0x505C, 0x6B62, 0x518D, 0x958B, 0x6B66,
        0x5668, 0x623B, 0x9589, 0x958B
    };
    for (uint32_t codepoint : kanji)
        addGlyph(font, scale, codepoint, g_kana);

    if (!g_latin.empty())
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

void appendUtf8(std::string& text, uint32_t codepoint) {
    if (codepoint < 0x80) {
        text.push_back(static_cast<char>(codepoint));
    } else if (codepoint < 0x800) {
        text.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        text.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        text.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        text.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        text.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

uint32_t classifyBox(const std::vector<uint8_t>& ink, int width, int height, float& score) {
    std::vector<uint8_t> sample;
    normalize(ink, width, height, sample);
    score = 0.0f;
    uint32_t best = '?';
    const std::vector<GlyphTemplate>& templates = g_matchJapanese ? g_kana : g_latin;
    for (const GlyphTemplate& glyph : templates) {
        const float iou = intersectionOverUnion(glyph.ink, sample);
        if (iou > score) {
            score = iou;
            best = glyph.codepoint;
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
            const uint32_t codepoint = classifyBox(ink, glyph.x1 - glyph.x0, glyph.y1 - glyph.y0, score);
            appendUtf8(text, codepoint);
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
    for (size_t i = 0; i < text.size();) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if ((c & 0x80) == 0) {
            if (c == '?')
                unknown++;
            else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
                letters++;
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            letters++;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            letters++;
            i += 3;
        } else {
            i++;
        }
    }
    return letters - unknown * 2;
}

}

std::string recognizeText(const uint8_t* rgba, int width, int height, bool cropLeftPanel, bool japaneseSource) {
    buildTemplates();
    g_matchJapanese = japaneseSource;
    const std::vector<GlyphTemplate>& templates = japaneseSource ? g_kana : g_latin;
    if (templates.empty())
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
