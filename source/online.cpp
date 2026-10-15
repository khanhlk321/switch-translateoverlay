#include "online.hpp"

#include "translate.hpp"

#include <curl/curl.h>
#include <switch.h>

#include <string>

namespace {

constexpr size_t kChunkBytes = 400;
bool g_networkReady = false;

size_t writeBody(char* data, size_t size, size_t count, void* userdata) {
    auto* body = static_cast<std::string*>(userdata);
    const size_t bytes = size * count;
    if (body->size() + bytes > 256 * 1024)
        return 0;
    body->append(data, bytes);
    return bytes;
}

void appendCodepoint(std::string& out, unsigned int codepoint) {
    if (codepoint < 0x80) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

int hexValue(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

std::string decodeJsonString(const std::string& body) {
    const std::string key = "\"translatedText\":\"";
    const size_t start = body.find(key);
    if (start == std::string::npos)
        return {};

    std::string text;
    for (size_t i = start + key.size(); i < body.size(); i++) {
        const char c = body[i];
        if (c == '"')
            break;
        if (c != '\\' || i + 1 >= body.size()) {
            text.push_back(c);
            continue;
        }
        const char escaped = body[++i];
        if (escaped == 'n')
            text.push_back('\n');
        else if (escaped == 't')
            text.push_back('\t');
        else if (escaped == 'u' && i + 4 < body.size()) {
            unsigned int codepoint = 0;
            bool ok = true;
            for (int nibble = 0; nibble < 4; nibble++) {
                const int value = hexValue(body[i + 1 + nibble]);
                if (value < 0) {
                    ok = false;
                    break;
                }
                codepoint = (codepoint << 4) | static_cast<unsigned int>(value);
            }
            i += 4;
            if (ok)
                appendCodepoint(text, codepoint);
        } else {
            text.push_back(escaped);
        }
    }
    return text;
}

std::string urlEncode(const std::string& value) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size() * 3);
    for (unsigned char c : value) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
            out.push_back(static_cast<char>(c));
        else if (c == ' ')
            out.push_back('+');
        else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0x0F]);
        }
    }
    return out;
}

bool consoleIsOnline() {
    if (R_FAILED(nifmInitialize(NifmServiceType_User)))
        return false;
    NifmInternetConnectionType type = static_cast<NifmInternetConnectionType>(0);
    u32 strength = 0;
    NifmInternetConnectionStatus status = NifmInternetConnectionStatus_ConnectingUnknown1;
    const Result rc = nifmGetInternetConnectionStatus(&type, &strength, &status);
    nifmExit();
    return R_SUCCEEDED(rc) && status == NifmInternetConnectionStatus_Connected;
}

bool requestChunk(const std::string& text, SourceLanguage source, std::string& translated) {
    const char* from = source == SourceLanguage::Japanese ? "ja" : "en";
    const std::string url = std::string("https://api.mymemory.translated.net/get?langpair=") + from + "|vi&q=" + urlEncode(text);

    std::string body;
    CURL* curl = curl_easy_init();
    if (curl == nullptr)
        return false;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeBody);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "translate-overlay/0.1");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    const CURLcode result = curl_easy_perform(curl);
    long httpStatus = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK || httpStatus != 200)
        return false;

    translated = decodeJsonString(body);
    if (translated.empty() || translated.find("MYMEMORY WARNING") != std::string::npos)
        return false;
    return true;
}

}

bool networkStart() {
    if (R_FAILED(socketInitializeDefault()))
        return false;
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
        socketExit();
        return false;
    }
    g_networkReady = true;
    return true;
}

void networkStop() {
    if (!g_networkReady)
        return;
    curl_global_cleanup();
    socketExit();
    g_networkReady = false;
}

bool translateOnline(const std::string& english, SourceLanguage language, std::string& translated) {
    translated.clear();
    if (!g_networkReady || !consoleIsOnline())
        return false;

    std::string chunk;
    bool anyOnline = false;
    auto flush = [&]() {
        if (chunk.empty())
            return;
        std::string piece;
        if (requestChunk(chunk, language, piece)) {
            if (!translated.empty())
                translated.push_back('\n');
            translated += piece;
            anyOnline = true;
        } else {
            if (!translated.empty())
                translated.push_back('\n');
            translated += translateText(chunk, language);
        }
        chunk.clear();
    };

    std::string line;
    for (char c : english) {
        if (c == '\n') {
            if (chunk.size() + line.size() > kChunkBytes)
                flush();
            if (!chunk.empty())
                chunk.push_back('\n');
            chunk += line;
            line.clear();
            if (chunk.size() >= kChunkBytes)
                flush();
        } else {
            line.push_back(c);
        }
    }
    if (chunk.size() + line.size() > kChunkBytes)
        flush();
    if (!chunk.empty())
        chunk.push_back('\n');
    chunk += line;
    flush();
    return anyOnline;
}
