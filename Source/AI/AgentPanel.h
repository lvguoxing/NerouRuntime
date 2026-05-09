#pragma once

#include <JuceHeader.h>
#include "AgentCore.h"

namespace nerou::ui {

/**
 * AgentPanel - Hermes Agent 对话侧边栏
 *
 * 作为浮动侧边栏嵌入 MainComponent 右侧，可折叠。
 * 支持：
 *   - 多轮对话
 *   - 智能建议气泡（来自 ExperienceEngine）
 *   - 后端状态指示
 *   - LLM 设置入口
 */

class AgentPanel : public juce::Component,
                   public ai::AgentCoreListener,
                   private juce::Timer
{
public:
    AgentPanel();
    ~AgentPanel() override;

    // juce::Component
    void paint(juce::Graphics& g) override;
    void resized() override;

    // AgentCoreListener
    void onAgentReply(const juce::String& content) override;
    void onAgentError(const juce::String& error)   override;
    void onAgentThinking()                          override;

    // 面板状态控制
    void setExpanded(bool expanded);
    bool isExpanded() const noexcept { return expanded_; }

    /** 获取推荐面板宽度（折叠/展开） */
    static int getCollapsedWidth() noexcept  { return 44; }
    static int getExpandedWidth()  noexcept  { return 340; }

    /** 外部调用：强制清空对话 */
    void clearChat();

private:
    void timerCallback() override;

    void sendCurrentInput();
    void appendBubble(const juce::String& text, bool isUser);
    void appendThinkingBubble();
    void removeThinkingBubble();
    void scrollToBottom();
    void relayoutChatContainer();
    void showSettingsDialog();
    void checkConnectivity();

    void buildSettingsContent(juce::Component& host,
                               juce::TextEditor& urlEditor,
                               juce::TextEditor& keyEditor,
                               juce::ComboBox&   backendCombo,
                               juce::TextEditor& modelEditor);

    // ── 气泡组件 ──────────────────────────────────────────────────────────────

    struct ChatBubble : public juce::Component
    {
        enum class Type { User, Assistant, Thinking, System };

        Type         type;
        juce::String text;
        bool         isThinking = false;
        int          thinkDots  = 0;

        ChatBubble(Type t, const juce::String& msg);
        void paint(juce::Graphics& g) override;
        void resized() override;
        int  getPreferredHeight(int width) const;

    private:
        juce::AttributedString buildAttrStr(int width) const;
    };

    // ── 内部组件 ──────────────────────────────────────────────────────────────

    juce::TextButton  toggleBtn;
    juce::Label       titleLabel;
    juce::Label       statusDot;
    juce::TextButton  clearBtn;
    juce::TextButton  settingsBtn;

    juce::Viewport    chatViewport;
    juce::Component   chatContainer;

    juce::TextEditor  inputEditor;
    juce::TextButton  sendBtn;

    juce::OwnedArray<ChatBubble> bubbles_;

    bool expanded_       = false;
    bool isThinking_     = false;
    int  thinkDotTicker_ = 0;

    juce::Colour getBackgroundColor()    const noexcept;
    juce::Colour getHeaderColor()        const noexcept;
    juce::Colour getBorderColor()        const noexcept;

    static constexpr int kHeaderHeight = 44;
    static constexpr int kInputHeight  = 44;
    static constexpr int kBubblePad    = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AgentPanel)
};

} // namespace nerou::ui
