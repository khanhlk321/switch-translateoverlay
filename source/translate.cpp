#include "translate.hpp"

#include <switch.h>

#include <cstdio>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::unordered_map<std::string, std::string> g_vietnamese;
std::unordered_map<std::string, std::string> g_japanese;
std::unordered_map<std::string, std::string>* g_active = &g_vietnamese;

std::string lowerAscii(std::string text) {
    for (char& c : text) {
        unsigned char ch = static_cast<unsigned char>(c);
        if (ch >= 'A' && ch <= 'Z')
            c = static_cast<char>(ch - 'A' + 'a');
    }
    return text;
}

void addPhrase(const char* english, const char* translation) {
    (*g_active)[english] = translation;
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

void applyJapaneseGamePhrases() {
    const char* pairs[][2] = {
        {"ニューゲーム", "trò chơi mới"},
        {"つづきから", "chơi tiếp"},
        {"ゲームオーバー", "thua cuộc"},
        {"しんでしまった", "bạn đã chết"},
        {"本当にいいですか", "bạn có chắc không"},
        {"タイトルにもどる", "về màn hình chính"},
        {"ボタンを押してください", "nhấn nút bất kỳ"},
        {"スタートを押してください", "nhấn start"},
        {"お待ちください", "vui lòng đợi"},
        {"スタート", "bắt đầu"},
        {"押す", "nhấn"},
        {"つづける", "tiếp tục"},
        {"新規", "mới"},
        {"ゲーム", "trò chơi"},
        {"ロード", "tải"},
        {"セーブ", "lưu"},
        {"オプション", "tùy chọn"},
        {"設定", "cài đặt"},
        {"はい", "có"},
        {"いいえ", "không"},
        {"決定", "đồng ý"},
        {"キャンセル", "hủy"},
        {"戻る", "quay lại"},
        {"終了", "thoát"},
        {"メニュー", "menu"},
        {"一時停止", "tạm dừng"},
        {"再開", "tiếp tục"},
        {"もちもの", "túi đồ"},
        {"アイテム", "vật phẩm"},
        {"そうび", "trang bị"},
        {"武器", "vũ khí"},
        {"よろい", "giáp"},
        {"ポーション", "thuốc"},
        {"たいりょく", "máu"},
        {"まほう", "phép"},
        {"スキル", "kỹ năng"},
        {"クエスト", "nhiệm vụ"},
        {"ミッション", "nhiệm vụ"},
        {"目標", "mục tiêu"},
        {"マップ", "bản đồ"},
        {"ショップ", "cửa hàng"},
        {"買う", "mua"},
        {"売る", "bán"},
        {"ゴールド", "vàng"},
        {"レベル", "cấp"},
        {"攻撃", "tấn công"},
        {"防御", "phòng thủ"},
        {"敵", "kẻ địch"},
        {"勝利", "chiến thắng"},
        {"敗北", "thất bại"},
        {"リトライ", "thử lại"},
        {"確認", "xác nhận"},
        {"削除", "xóa"},
        {"選択中", "đã chọn"},
        {"選択", "chọn"},
        {"次へ", "tiếp"},
        {"スキップ", "bỏ qua"},
        {"オート", "tự động"},
        {"オン", "bật"},
        {"オフ", "tắt"},
        {"音量", "âm lượng"},
        {"言語", "ngôn ngữ"},
        {"コントローラー", "tay cầm"},
        {"チュートリアル", "hướng dẫn"},
        {"読み込み中", "đang tải"},
        {"警告", "cảnh báo"},
        {"完了", "hoàn thành"},
        {"失敗", "thất bại"},
        {"成功", "thành công"},
        {"プレイヤー", "người chơi"},
        {"ボス", "trùm"},
        {"鍵", "chìa khóa"},
        {"ドア", "cửa"},
        {"ロック中", "đã khóa"},
        {"開く", "mở"},
        {"閉じる", "đóng"},
        {"話す", "nói chuyện"},
        {"ステータス", "trạng thái"},
        {"パーティ", "đội"},
        {"私", "tôi"},
    };
    for (const auto& pair : pairs)
        addPhrase(pair[0], pair[1]);
}

bool isWordCodepoint(uint32_t codepoint) {
    if ((codepoint >= 'A' && codepoint <= 'Z') || (codepoint >= 'a' && codepoint <= 'z') || (codepoint >= '0' && codepoint <= '9') || codepoint == '\'' || codepoint == '-')
        return true;
    if (codepoint >= 0x3040 && codepoint <= 0x30FF)
        return true;
    if (codepoint >= 0x4E00 && codepoint <= 0x9FFF)
        return true;
    return codepoint == 0x30FC || codepoint == 0x3005;
}

uint32_t decodeUtf8(const std::string& text, size_t& index) {
    const unsigned char lead = static_cast<unsigned char>(text[index]);
    if (lead < 0x80) {
        index++;
        return lead;
    }
    if ((lead & 0xE0) == 0xC0 && index + 1 < text.size()) {
        const uint32_t codepoint = (static_cast<uint32_t>(lead & 0x1F) << 6) | (static_cast<unsigned char>(text[index + 1]) & 0x3F);
        index += 2;
        return codepoint;
    }
    if ((lead & 0xF0) == 0xE0 && index + 2 < text.size()) {
        const uint32_t codepoint = (static_cast<uint32_t>(lead & 0x0F) << 12)
            | (static_cast<uint32_t>(static_cast<unsigned char>(text[index + 1]) & 0x3F) << 6)
            | (static_cast<unsigned char>(text[index + 2]) & 0x3F);
        index += 3;
        return codepoint;
    }
    index++;
    return lead;
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
    for (size_t index = 0; index < line.size();) {
        const size_t start = index;
        const uint32_t codepoint = decodeUtf8(line, index);
        if (codepoint == ' ' || codepoint == '\t') {
            flush();
            continue;
        }
        const bool word = isWordCodepoint(codepoint);
        if (!current.empty() && word != currentWord)
            flush();
        current.append(line, start, index - start);
        currentWord = word;
    }
    flush();
    return tokens;
}

std::string lookup(const std::unordered_map<std::string, std::string>& dict, const std::string& phrase) {
    const auto it = dict.find(phrase);
    if (it == dict.end())
        return {};
    return it->second;
}

std::string translateLine(const std::unordered_map<std::string, std::string>& dict, const std::string& line) {
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
            const std::string hit = lookup(dict, phrase);
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

void loadWordList(const char* path, std::unordered_map<std::string, std::string>& dict) {
    FILE* file = std::fopen(path, "rb");
    if (file == nullptr)
        return;

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
        dict[lowerAscii(line.substr(0, tab))] = line.substr(tab + 1);
    }
}

}

void loadDictionary() {
    g_vietnamese.clear();
    g_japanese.clear();
    if (R_SUCCEEDED(fsdevMountSdmc())) {
        loadWordList("sdmc:/switch/translate/en-vi.tsv", g_vietnamese);
        loadWordList("sdmc:/switch/translate/ja-vi.tsv", g_japanese);
        fsdevUnmountDevice("sdmc");
    }
    g_active = &g_vietnamese;
    applyGamePhrases();
    g_active = &g_japanese;
    applyJapaneseGamePhrases();
}

std::string translateText(const std::string& english, SourceLanguage language) {
    const std::unordered_map<std::string, std::string>& dict =
        language == SourceLanguage::Japanese ? g_japanese : g_vietnamese;
    std::string out;
    std::string line;
    std::istringstream input(english);
    bool first = true;
    while (std::getline(input, line)) {
        if (!first)
            out.push_back('\n');
        first = false;
        out += translateLine(dict, line);
    }
    return out;
}
