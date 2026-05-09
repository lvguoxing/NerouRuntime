#include "AgentPanel.h"

namespace nerou::ui {

using namespace nerou::ai;

// ============================================================================
// ChatBubble
// ============================================================================

AgentPanel::ChatBubble::ChatBubble(Type t, const juce::String& msg)
    : type(t), text(msg)
{
    setInterceptsMouseClicks(false, false);
}

void AgentPanel::ChatBubble::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(4.0f, 2.0f);
    const bool isUser = (type == Type::User);

    juce::Colour bg, fg;
    float radius = 10.0f;

    if (isUser)
    {
        bg = juce::Colour(0xFF07C160);
        fg = juce::Colours::white;
    }
    else if (type == Type::Thinking)
    {
        bg = juce::Colour(0xFF2A2A3A);
        fg = juce::Colour(0xFF8888AA);
    }
    else if (type == Type::System)
    {
        bg = juce::Colour(0xFF1D3050);
        fg = juce::Colour(0xFF8FCDFF);
    }
    else
    {
        bg = juce::Colour(0xFF252535);
        fg = juce::Colour(0xFFE1E2E5);
    }

    g.setColour(bg);
    g.fillRoundedRectangle(bounds, radius);

    if (type == Type::Thinking)
    {
        // 三点动画
        juce::String dots = ".";
        for (int i = 0; i < thinkDots % 3; ++i) dots += ".";
        g.setColour(fg);
        g.setFont(juce::Font(14.0f));
        g.drawText("思考中" + dots, bounds.reduced(10.0f, 6.0f),
                   juce::Justification::centredLeft, false);
        return;
    }

    g.setColour(fg);
    g.setFont(juce::Font(13.5f));

    auto textBounds = bounds.reduced(10.0f, 8.0f);
    juce::AttributedString as = buildAttrStr(static_cast<int>(textBounds.getWidth()));
    juce::TextLayout layout;
    layout.createLayout(as, textBounds.getWidth());
    layout.draw(g, textBounds);
}

void AgentPanel::ChatBubble::resized() {}

int AgentPanel::ChatBubble::getPreferredHeight(int width) const
{
    if (type == Type::Thinking) return 34;

    int contentW = juce::jmax(40, width - 28);
    juce::AttributedString as = buildAttrStr(contentW);
    juce::TextLayout layout;
    layout.createLayout(as, static_cast<float>(contentW));
    return static_cast<int>(layout.getHeight()) + 24;
}

juce::AttributedString AgentPanel::ChatBubble::buildAttrStr(int /*width*/) const
{
    juce::AttributedString as;
    juce::Colour fg = (type == Type::User) ? juce::Colours::white
                    : (type == Type::System) ? juce::Colour(0xFF8FCDFF)
                    : juce::Colour(0xFFE1E2E5);
    as.setColour(fg);
    as.setFont(juce::Font(13.5f));
    as.setWordWrap(juce::AttributedString::byWord);
    as.append(text);
    return as;
}

// ============================================================================
// AgentPanel
// ============================================================================

AgentPanel::AgentPanel()
{
    // 折叠/展开按钮
    toggleBtn.setButtonText(u8"\U0001F9E0"); // 🧠
    toggleBtn.onClick = [this] { setExpanded(!expanded_); };
    addAndMakeVisible(toggleBtn);

    // 标题
    titleLabel.setText(u8"\u667a\u80fd\u52a9\u624b", juce::dontSendNotification); // 智能助手
    titleLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFE1E2E5));
    addChildComponent(titleLabel);

    // 状态指示点
    statusDot.setText(u8"\u2022", juce::dontSendNotification); // •
    statusDot.setFont(juce::Font(18.0f));
    statusDot.setColour(juce::Label::textColourId, juce::Colour(0xFF555566));
    statusDot.setTooltip(u8"\u672a\u8fde\u63a5"); // 未连接
    addChildComponent(statusDot);

    // 清空按钮
    clearBtn.setButtonText(u8"\u6e05\u7a7a"); // 清空
    clearBtn.onClick = [this] { clearChat(); };
    addChildComponent(clearBtn);

    // 设置按钮
    settingsBtn.setButtonText(u8"\u2699"); // ⚙
    settingsBtn.onClick = [this] { showSettingsDialog(); };
    addChildComponent(settingsBtn);

    // 聊天视窗
    chatContainer.setInterceptsMouseClicks(false, true);
    chatViewport.setViewedComponent(&chatContainer, false);
    chatViewport.setScrollBarsShown(true, false);
    addChildComponent(chatViewport);

    // 输入框
    inputEditor.setMultiLine(false);
    inputEditor.setReturnKeyStartsNewLine(false);
    inputEditor.onReturnKey = [this] { sendCurrentInput(); };
    inputEditor.setTextToShowWhenEmpty(u8"\u8f93\u5165\u95ee\u9898\u2026", // 输入问题…
                                        juce::Colour(0xFF555566));
    inputEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF1A1A2A));
    inputEditor.setColour(juce::TextEditor::textColourId,        juce::Colour(0xFFE1E2E5));
    inputEditor.setColour(juce::TextEditor::outlineColourId,     juce::Colour(0xFF333345));
    addChildComponent(inputEditor);

    // 发送按钮
    sendBtn.setButtonText(u8"\u53d1\u9001"); // 发送
    sendBtn.onClick = [this] { sendCurrentInput(); };
    addChildComponent(sendBtn);

    // 检查连通性（延迟）
    startTimer(1500);

    // 欢迎消息（异步，等面板展开后添加）
}

AgentPanel::~AgentPanel()
{
    stopTimer();
}

// ============================================================================
// paint / resized
// ============================================================================

void AgentPanel::paint(juce::Graphics& g)
{
    auto bg = getBackgroundColor();
    g.fillAll(bg);

    // 左侧边线
    g.setColour(getBorderColor());
    g.drawLine(0.0f, 0.0f, 0.0f, static_cast<float>(getHeight()), 1.0f);

    if (!expanded_) return;

    // 标题栏底线
    g.setColour(getBorderColor());
    g.drawLine(0.0f, static_cast<float>(kHeaderHeight),
               static_cast<float>(getWidth()),
               static_cast<float>(kHeaderHeight), 1.0f);

    // 输入栏顶线
    int inputY = getHeight() - kInputHeight;
    g.drawLine(0.0f, static_cast<float>(inputY),
               static_cast<float>(getWidth()),
               static_cast<float>(inputY), 1.0f);
}

void AgentPanel::resized()
{
    auto bounds = getLocalBounds();

    // 折叠按钮始终在左上角
    toggleBtn.setBounds(2, 2, getCollapsedWidth() - 4, 40);

    if (!expanded_) return;

    // 标题栏
    int hdr = kHeaderHeight;
    auto headerArea = bounds.removeFromTop(hdr);
    headerArea.removeFromLeft(getCollapsedWidth()); // 留给 toggle 按钮
    titleLabel.setBounds(headerArea.removeFromLeft(120));
    statusDot.setBounds(headerArea.removeFromLeft(24));
    settingsBtn.setBounds(headerArea.removeFromRight(40));
    clearBtn.setBounds(headerArea.removeFromRight(50));

    // 输入区
    auto inputArea = bounds.removeFromBottom(kInputHeight);
    sendBtn.setBounds(inputArea.removeFromRight(56).reduced(4));
    inputEditor.setBounds(inputArea.reduced(4, 6));

    // 聊天区
    chatViewport.setBounds(bounds);
    relayoutChatContainer();
}

// ============================================================================
// 内部工具
// ============================================================================

void AgentPanel::relayoutChatContainer()
{
    int w = chatViewport.getWidth() - 8;
    if (w <= 0) return;

    int y = kBubblePad;
    for (auto* b : bubbles_)
    {
        int h = b->getPreferredHeight(w);
        int x = 4;

        // 用户消息右对齐
        if (b->type == ChatBubble::Type::User)
        {
            int bw = juce::jmin(w - 40, 260);
            x = w - bw;
            b->setBounds(x, y, bw, h);
        }
        else
        {
            int bw = juce::jmin(w - 20, 290);
            b->setBounds(x, y, bw, h);
        }

        y += h + kBubblePad;
    }

    chatContainer.setSize(w + 8, juce::jmax(y + 4, chatViewport.getHeight()));
}

void AgentPanel::scrollToBottom()
{
    juce::MessageManager::callAsync([this] {
        chatViewport.setViewPosition(0,
            juce::jmax(0, chatContainer.getHeight() - chatViewport.getHeight()));
    });
}

void AgentPanel::appendBubble(const juce::String& text, bool isUser)
{
    auto type = isUser ? ChatBubble::Type::User : ChatBubble::Type::Assistant;
    auto* b   = bubbles_.add(new ChatBubble(type, text));
    chatContainer.addAndMakeVisible(b);
    relayoutChatContainer();
    scrollToBottom();
}

void AgentPanel::appendThinkingBubble()
{
    auto* b = bubbles_.add(new ChatBubble(ChatBubble::Type::Thinking, ""));
    chatContainer.addAndMakeVisible(b);
    isThinking_ = true;
    relayoutChatContainer();
    scrollToBottom();
}

void AgentPanel::removeThinkingBubble()
{
    for (int i = bubbles_.size() - 1; i >= 0; --i)
    {
        if (bubbles_[i]->type == ChatBubble::Type::Thinking)
        {
            chatContainer.removeChildComponent(bubbles_[i]);
            bubbles_.remove(i);
            break;
        }
    }
    isThinking_ = false;
    relayoutChatContainer();
}

// ============================================================================
// AgentCoreListener 回调
// ============================================================================

void AgentPanel::onAgentReply(const juce::String& content)
{
    removeThinkingBubble();
    appendBubble(content, false);
}

void AgentPanel::onAgentError(const juce::String& error)
{
    removeThinkingBubble();
    auto* b = bubbles_.add(
        new ChatBubble(ChatBubble::Type::System,
                       juce::String::fromUTF8("\xe2\x9a\xa0 ") + error)); // ⚠
    chatContainer.addAndMakeVisible(b);
    relayoutChatContainer();
    scrollToBottom();
}

void AgentPanel::onAgentThinking()
{
    appendThinkingBubble();
}

// ============================================================================
// 发送消息
// ============================================================================

void AgentPanel::sendCurrentInput()
{
    auto text = inputEditor.getText().trim();
    if (text.isEmpty()) return;
    if (!expanded_) setExpanded(true);

    inputEditor.clear();
    appendBubble(text, true);

    Agent().sendUserMessage(text, this);
}

// ============================================================================
// 展开/折叠
// ============================================================================

void AgentPanel::setExpanded(bool expanded)
{
    if (expanded_ == expanded) return;
    expanded_ = expanded;

    titleLabel.setVisible(expanded);
    statusDot.setVisible(expanded);
    clearBtn.setVisible(expanded);
    settingsBtn.setVisible(expanded);
    chatViewport.setVisible(expanded);
    inputEditor.setVisible(expanded);
    sendBtn.setVisible(expanded);

    if (expanded && bubbles_.isEmpty())
    {
        // 欢迎消息
        auto* b = bubbles_.add(new ChatBubble(
            ChatBubble::Type::System,
            u8"\u4f60\u597d\uff01\u6211\u662f NeuroRuntime \u667a\u80fd\u52a9\u624b\uff0c"
            u8"\u53ef\u4ee5\u56de\u7b54\u5173\u4e8e EEG \u91c7\u96c6\u3001\u8bad\u7ec3\u3001"
            u8"\u9a8c\u8bc1\u7684\u95ee\u9898\uff0c\u5e76\u6839\u636e\u5386\u53f2\u7ecf\u9a8c"
            u8"\u63d0\u4f9b\u53c2\u6570\u5efa\u8bae\u3002"));
        chatContainer.addAndMakeVisible(b);
        relayoutChatContainer();
    }

    resized();
    repaint();

    // 通知父组件调整布局
    if (auto* parent = getParentComponent())
        parent->resized();
}

// ============================================================================
// 清空对话
// ============================================================================

void AgentPanel::clearChat()
{
    chatContainer.removeAllChildren();
    bubbles_.clear();
    Agent().clearConversation();
    relayoutChatContainer();
}

// ============================================================================
// Timer（thinking 动画 + 连通性检查）
// ============================================================================

void AgentPanel::timerCallback()
{
    static int tickCount = 0;
    ++tickCount;

    // thinking 气泡动画
    if (isThinking_)
    {
        ++thinkDotTicker_;
        for (auto* b : bubbles_)
        {
            if (b->type == ChatBubble::Type::Thinking)
            {
                b->thinkDots = thinkDotTicker_;
                b->repaint();
            }
        }
    }

    // 每 30 秒检查一次连通性
    if (tickCount % 60 == 0)
        checkConnectivity();
}

void AgentPanel::checkConnectivity()
{
    if (!Agent().isReady())
    {
        statusDot.setColour(juce::Label::textColourId, juce::Colour(0xFF555566));
        statusDot.setTooltip(u8"\u672a\u914d\u7f6e LLM \u540e\u7aef"); // 未配置 LLM 后端
        return;
    }

    Agent().checkBackendConnectivity([this](bool ok, juce::String info) {
        juce::Colour color = ok ? juce::Colour(0xFF07C160)  // 绿
                                : juce::Colour(0xFFFA5151); // 红
        statusDot.setColour(juce::Label::textColourId, color);
        statusDot.setTooltip(info);
        statusDot.repaint();
    });
}

// ============================================================================
// 颜色
// ============================================================================

juce::Colour AgentPanel::getBackgroundColor() const noexcept
{
    return juce::Colour(0xFF141420);
}

juce::Colour AgentPanel::getHeaderColor() const noexcept
{
    return juce::Colour(0xFF1A1A2A);
}

juce::Colour AgentPanel::getBorderColor() const noexcept
{
    return juce::Colour(0xFF2A2A3A);
}

// ============================================================================
// 设置弹窗
// ============================================================================

void AgentPanel::showSettingsDialog()
{
    auto dialog = std::make_unique<juce::AlertWindow>(
        u8"\u667a\u80fd\u52a9\u624b\u8bbe\u7f6e", // 智能助手设置
        u8"\u914d\u7f6e LLM \u540e\u7aef\u8fde\u63a5", // 配置 LLM 后端连接
        juce::MessageBoxIconType::NoIcon);

    const auto& cfg = Agent().getLLMConfig();

    // 后端选择
    dialog->addComboBox("backend", { "Ollama (本地)", "OpenAI 兼容 API" }, "后端类型");
    auto* backendCombo = dialog->getComboBoxComponent("backend");
    backendCombo->setSelectedItemIndex(
        cfg.backend == ai::LLMConfig::Backend::OpenAICompatible ? 1 : 0);

    // URL
    dialog->addTextEditor("url", cfg.baseUrl, "服务地址 (URL)");

    // API Key
    dialog->addTextEditor("key", cfg.apiKey, "API Key (Ollama 可留空)");
    dialog->getTextEditor("key")->setPasswordCharacter(u8"\u2022"[0]);

    // 模型
    dialog->addTextEditor("model", cfg.model, "模型名称");

    dialog->addButton(u8"\u4fdd\u5b58", 1); // 保存
    dialog->addButton(u8"\u6d4b\u8bd5\u8fde\u63a5", 2); // 测试连接
    dialog->addButton(u8"\u53d6\u6d88", 0); // 取消

    dialog->enterModalState(true,
        juce::ModalCallbackFunction::create([this, d = dialog.get()](int result) {
            if (result == 0) return;

            ai::LLMConfig newCfg;
            auto* backEnd = d->getComboBoxComponent("backend");
            newCfg.backend = (backEnd && backEnd->getSelectedItemIndex() == 1)
                ? ai::LLMConfig::Backend::OpenAICompatible
                : ai::LLMConfig::Backend::Ollama;

            if (auto* e = d->getTextEditor("url"))
                newCfg.baseUrl = e->getText().trim();
            if (auto* e = d->getTextEditor("key"))
                newCfg.apiKey = e->getText().trim();
            if (auto* e = d->getTextEditor("model"))
                newCfg.model = e->getText().trim();

            Agent().setLLMConfig(newCfg);

            if (result == 2) // 测试连接
            {
                checkConnectivity();
                auto* b = bubbles_.add(new ChatBubble(
                    ChatBubble::Type::System,
                    u8"\u6b63\u5728\u68c0\u6d4b\u540e\u7aef\u8fde\u901a\u6027\u2026")); // 正在检测后端连通性…
                chatContainer.addAndMakeVisible(b);
                relayoutChatContainer();
                scrollToBottom();
            }
        }));

    dialog.release(); // AlertWindow 自己管理生命周期
}

} // namespace nerou::ui
