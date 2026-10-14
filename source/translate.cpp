#include "translate.hpp"

#include <switch.h>

#include <cstdio>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::unordered_map<std::string, std::string> g_dict;

std::string lowerAscii(std::string text) {
    for (char& c : text) {
        unsigned char ch = static_cast<unsigned char>(c);
        if (ch >= 'A' && ch <= 'Z')
            c = static_cast<char>(ch - 'A' + 'a');
    }
    return text;
}

void addPhrase(const char* english, const char* vietnamese) {
    g_dict[english] = vietnamese;
}

void applyGamePhrases() {
    const char* pairs[][2] = {
        {"new game", "trò chơi mới"},
        {"load game", "tải trò chơi"},
        {"save game", "lưu trò chơi"},
        {"continue game", "chơi tiếp"},
        {"game over", "thua cuộc"},
        {"you died", "bạn đã chết"},
        {"are you sure", "bạn có chắc không"},
        {"return to title", "về màn hình chính"},
        {"press any button", "nhấn nút bất kỳ"},
        {"press start", "nhấn start"},
        {"please wait", "vui lòng đợi"},
        {"options menu", "menu tùy chọn"},
        {"start", "bắt đầu"},
        {"press", "nhấn"},
        {"continue", "tiếp tục"},
        {"new", "mới"},
        {"game", "trò chơi"},
        {"load", "tải"},
        {"save", "lưu"},
        {"options", "tùy chọn"},
        {"settings", "cài đặt"},
        {"yes", "có"},
        {"no", "không"},
        {"ok", "đồng ý"},
        {"cancel", "hủy"},
        {"back", "quay lại"},
        {"exit", "thoát"},
        {"quit", "thoát"},
        {"menu", "menu"},
        {"pause", "tạm dừng"},
        {"resume", "tiếp tục"},
        {"inventory", "túi đồ"},
        {"item", "vật phẩm"},
        {"items", "vật phẩm"},
        {"equipment", "trang bị"},
        {"equip", "trang bị"},
        {"weapon", "vũ khí"},
        {"armor", "giáp"},
        {"potion", "thuốc"},
        {"health", "máu"},
        {"magic", "phép"},
        {"skill", "kỹ năng"},
        {"skills", "kỹ năng"},
        {"quest", "nhiệm vụ"},
        {"mission", "nhiệm vụ"},
        {"objective", "mục tiêu"},
        {"map", "bản đồ"},
        {"shop", "cửa hàng"},
        {"buy", "mua"},
        {"sell", "bán"},
        {"gold", "vàng"},
        {"level", "cấp"},
        {"attack", "tấn công"},
        {"defend", "phòng thủ"},
        {"defense", "phòng thủ"},
        {"enemy", "kẻ địch"},
        {"victory", "chiến thắng"},
        {"defeat", "thất bại"},
        {"retry", "thử lại"},
        {"confirm", "xác nhận"},
        {"delete", "xóa"},
        {"select", "chọn"},
        {"selected", "đã chọn"},
        {"next", "tiếp"},
        {"skip", "bỏ qua"},
        {"auto", "tự động"},
        {"on", "bật"},
        {"off", "tắt"},
        {"volume", "âm lượng"},
        {"language", "ngôn ngữ"},
        {"controller", "tay cầm"},
        {"tutorial", "hướng dẫn"},
        {"loading", "đang tải"},
        {"warning", "cảnh báo"},
        {"complete", "hoàn thành"},
        {"failed", "thất bại"},
        {"success", "thành công"},
        {"player", "người chơi"},
        {"boss", "trùm"},
        {"key", "chìa khóa"},
        {"door", "cửa"},
        {"locked", "đã khóa"},
        {"open", "mở"},
        {"close", "đóng"},
        {"talk", "nói chuyện"},
        {"status", "trạng thái"},
        {"party", "đội"},
        {"i", "tôi"},
    };
    for (const auto& pair : pairs)
        addPhrase(pair[0], pair[1]);
}

bool isWordChar(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '\'' || c == '-';
}

struct Token {
    bool word;
    std::string text;
};

std::vector<Token> tokenize(const std::string& line) {
    std::vector<Token> tokens;
    std::string current;
    bool currentWord = false;
    auto flush = [&]() {
        if (!current.empty())
            tokens.push_back({currentWord, current});
        current.clear();
    };
    for (unsigned char c : line) {
        if (c == ' ' || c == '\t') {
            flush();
            continue;
        }
        const bool word = isWordChar(c);
        if (!current.empty() && word != currentWord)
            flush();
        current.push_back(static_cast<char>(c));
        currentWord = word;
    }
    flush();
    return tokens;
}

std::string lookup(const std::string& phrase) {
    const auto it = g_dict.find(phrase);
    if (it == g_dict.end())
        return {};
    return it->second;
}

std::string translateLine(const std::string& line) {
    const std::vector<Token> tokens = tokenize(line);
    std::string out;
    for (size_t i = 0; i < tokens.size();) {
        if (!tokens[i].word) {
            out += tokens[i].text;
            i++;
            continue;
        }

        std::string chosen = tokens[i].text;
        size_t consumed = 1;
        for (size_t n = 4; n >= 1; n--) {
            if (i + n > tokens.size())
                continue;
            bool words = true;
            std::string phrase;
            for (size_t k = 0; k < n; k++) {
                if (!tokens[i + k].word) {
                    words = false;
                    break;
                }
                if (k > 0)
                    phrase.push_back(' ');
                phrase += lowerAscii(tokens[i + k].text);
            }
            if (!words)
                continue;
            const std::string hit = lookup(phrase);
            if (!hit.empty()) {
                chosen = hit;
                consumed = n;
                break;
            }
        }

        if (!out.empty() && out.back() != '\n')
            out.push_back(' ');
        out += chosen;
        i += consumed;
    }
    return out;
}

}

void loadDictionary() {
    g_dict.clear();
    if (R_SUCCEEDED(fsdevMountSdmc())) {
        FILE* file = std::fopen("sdmc:/switch/translate/en-vi.tsv", "rb");
        if (file != nullptr) {
            std::string data;
            std::fseek(file, 0, SEEK_END);
            const long length = std::ftell(file);
            if (length > 0 && length < 8 * 1024 * 1024) {
                data.resize(static_cast<size_t>(length));
                std::fseek(file, 0, SEEK_SET);
                const size_t got = std::fread(data.data(), 1, data.size(), file);
                data.resize(got);
            }
            std::fclose(file);

            std::string line;
            std::istringstream input(data);
            while (std::getline(input, line)) {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (line.empty() || line[0] == '#')
                    continue;
                const size_t tab = line.find('\t');
                if (tab == std::string::npos || tab == 0)
                    continue;
                g_dict[lowerAscii(line.substr(0, tab))] = line.substr(tab + 1);
            }
        }
        fsdevUnmountDevice("sdmc");
    }
    applyGamePhrases();
}

std::string translateText(const std::string& english) {
    std::string out;
    std::string line;
    std::istringstream input(english);
    bool first = true;
    while (std::getline(input, line)) {
        if (!first)
            out.push_back('\n');
        first = false;
        out += translateLine(line);
    }
    return out;
}
