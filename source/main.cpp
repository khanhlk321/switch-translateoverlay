#define TESLA_INIT_IMPL
#include <tesla.hpp>

#include "capture.hpp"
#include "ocr.hpp"
#include "translate.hpp"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>

namespace {

std::mutex g_captionMutex;
std::string g_caption = "Point the game at the line you want, then press X.";
Thread g_thread = {};
bool g_hasThread = false;
std::atomic<int> g_state{0};
bool g_captureReady = false;
std::string g_captureError;

struct TranslateJob {
    CapturedFrame frame;
    bool cropLeftPanel;
};

void setCaption(const std::string& text) {
    std::lock_guard<std::mutex> lock(g_captionMutex);
    g_caption = text;
}

std::string getCaption() {
    std::lock_guard<std::mutex> lock(g_captionMutex);
    return g_caption;
}

size_t utf8Length(const std::string& text) {
    size_t count = 0;
    for (unsigned char c : text) {
        if ((c & 0xC0) != 0x80)
            count++;
    }
    return count;
}

std::string wrapText(const std::string& text, size_t columns) {
    std::string out;
    std::string word;
    size_t column = 0;

    auto flushWord = [&]() {
        if (word.empty())
            return;
        const size_t length = utf8Length(word);
        if (column > 0 && column + 1 + length > columns) {
            out.push_back('\n');
            column = 0;
        } else if (column > 0) {
            out.push_back(' ');
            column++;
        }
        out += word;
        column += length;
        word.clear();
    };

    for (char c : text) {
        if (c == '\n') {
            flushWord();
            out.push_back('\n');
            column = 0;
        } else if (c == ' ') {
            flushWord();
        } else {
            word.push_back(c);
        }
    }
    flushWord();
    return out;
}

void translateWorker(void* argument) {
    TranslateJob* job = static_cast<TranslateJob*>(argument);
    const std::string english = recognizeText(job->frame.rgba.data(), job->frame.width, job->frame.height, job->cropLeftPanel);
    delete job;
    if (english.empty())
        setCaption("No readable text on this frame.");
    else
        setCaption(translateText(english));
    g_state.store(2);
}

void collectFinishedJob() {
    if (!g_hasThread || g_state.load() != 2)
        return;
    threadWaitForExit(&g_thread);
    threadClose(&g_thread);
    g_hasThread = false;
    g_state.store(0);
}

void requestTranslation(bool cropLeftPanel) {
    if (g_state.load() == 1)
        return;
    collectFinishedJob();

    setCaption("Reading this frame...");
    if (!g_captureReady) {
        setCaption(g_captureError.empty() ? "Screenshot service is unavailable." : g_captureError);
        return;
    }

    TranslateJob* job = new TranslateJob();
    std::string error;
    if (!captureFrame(job->frame, error)) {
        delete job;
        setCaption(error);
        return;
    }
    job->cropLeftPanel = cropLeftPanel;

    g_state.store(1);
    const Result created = threadCreate(&g_thread, translateWorker, job, nullptr, 0x80000, 0x2B, -2);
    if (R_FAILED(created)) {
        delete job;
        g_state.store(0);
        setCaption("Could not start the translator.");
        return;
    }
    const Result started = threadStart(&g_thread);
    if (R_FAILED(started)) {
        threadClose(&g_thread);
        delete job;
        g_state.store(0);
        setCaption("Could not start the translator.");
        return;
    }
    g_hasThread = true;
}

void joinTranslator() {
    if (!g_hasThread)
        return;
    threadWaitForExit(&g_thread);
    threadClose(&g_thread);
    g_hasThread = false;
    g_state.store(0);
}

class CaptionElement : public tsl::elm::Element {
public:
    void draw(tsl::gfx::Renderer* renderer) override {
        const s32 width = tsl::cfg::FramebufferWidth;
        const s32 height = tsl::cfg::FramebufferHeight;
        const std::string body = wrapText(getCaption(), 28);
        const s32 barHeight = std::min(height - 70, 36 + 22 * (1 + static_cast<s32>(std::count(body.begin(), body.end(), '\n'))));
        const s32 y = height - barHeight;

        renderer->drawRect(0, 0, width, 58, tsl::Color{0x0, 0x0, 0x0, 0xC});
        renderer->drawString("X  this frame", false, 16, 28, 20, tsl::style::color::ColorText);
        renderer->drawString("B  close", false, 16, 50, 18, tsl::style::color::ColorDescription);

        renderer->drawRect(0, y, width, barHeight, tsl::Color{0x0, 0x0, 0x0, 0xD});
        renderer->drawString(body.c_str(), false, 16, y + 28, 20, tsl::Color{0xF, 0xF, 0xA, 0xF});
    }

    void layout(u16, u16, u16 parentWidth, u16 parentHeight) override {
        this->setBoundaries(0, 0, parentWidth, parentHeight);
    }
};

class CaptionGui : public tsl::Gui {
public:
    tsl::elm::Element* createUI() override {
        return new CaptionElement();
    }

    bool handleInput(u64 keysDown, u64, const HidTouchState&, HidAnalogStickState, HidAnalogStickState) override {
        if (keysDown & HidNpadButton_X) {
            requestTranslation(true);
            return true;
        }
        return false;
    }
};

class TranslateOverlay : public tsl::Overlay {
public:
    void initServices() override {
        loadDictionary();
        g_captureReady = captureServicesStart(g_captureError);
    }

    void exitServices() override {
        joinTranslator();
        if (g_captureReady)
            captureServicesStop();
    }

    void onShow() override {
        requestTranslation(false);
    }

    std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<CaptionGui>();
    }
};

}

int main(int argc, char** argv) {
    return tsl::loop<TranslateOverlay>(argc, argv);
}
