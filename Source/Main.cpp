#include <JuceHeader.h>
#include "MainComponent.h"
#include "Core/SystemLogger.h"
// 单元测试（PipelineStore 状态机）
#include "Tests/PipelineStoreTest.h"

// 产品显示名（\u 宽字面量，与 .cpp 文件实际编码无关）
static const wchar_t* kAppTitleCn =
    L"\u8111\u673A\u63A5\u53E3\u4E0E\u795E\u7ECF\u53CD\u9988\u6A21\u578B\u8BAD\u7EC3\u7CFB\u7EDF";

class NerouRuntimeApplication  : public juce::JUCEApplication
{
public:
    NerouRuntimeApplication() = default;
    const juce::String getApplicationName() override       { return juce::String (kAppTitleCn); }
    const juce::String getApplicationVersion() override    { return "1.0"; }
    bool moreThanOneInstanceAllowed() override             { return false; }

    void initialise (const juce::String& commandLine) override
    {
        // ── 全局系统日志：最先初始化，保证后续 DBG / writeToLog 全部入库 ──────
        juce::Logger::setCurrentLogger(&nerou::core::SystemLogger::getInstance());
        NR_LOGI("App", juce::String::fromUTF8(u8"NeuroRuntime 启动 · v")
                           + getApplicationVersion()
                           + juce::String::fromUTF8(u8" · 日志目录 = ")
                           + nerou::core::SystemLogger::getInstance()
                                 .getLogDirectory().getFullPathName());

        if (commandLine.contains("--test"))
        {
            juce::UnitTestRunner runner;
            runner.setAssertOnFailure(false);
            runner.runTestsInCategory("PipelineStore");
            for (int i = 0; i < runner.getNumResults(); ++i)
            {
                auto* r = runner.getResult(i);
                juce::Logger::writeToLog(r->unitTestName + ": "
                    + juce::String(r->passes) + " passed, "
                    + juce::String(r->failures) + " failed");
            }
            quit();
            return;
        }

        const juce::String winTitle = juce::String (kAppTitleCn) + " v" + getApplicationVersion();
        mainWindow.reset (new MainWindow (winTitle));
    }

    void shutdown() override
    {
        NR_LOGI("App", juce::String::fromUTF8(u8"NeuroRuntime 退出"));
        mainWindow = nullptr;
        juce::Logger::setCurrentLogger(nullptr);
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
    }

    class MainWindow    : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name)
            : DocumentWindow (name,
                              juce::Colour (0xffEEF2F7),  // matches MainComponent C_BG
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen (true);
           #else
            setResizable (true, true);
            setResizeLimits (900, 620, 3840, 2160);
            setSize (1340, 880);
            centreWithSize (getWidth(), getHeight());
           #endif

            // ── Window icon (taskbar + alt-tab) ──────────────────────────────
            // Use logo.png (PNG is readable by JUCE ImageCache; .ico is not)
            juce::Image icon = juce::ImageCache::getFromMemory(
                BinaryData::logo_png,
                BinaryData::logo_pngSize);
            if (icon.isValid())
                setIcon(icon);

            setVisible (true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (NerouRuntimeApplication)
