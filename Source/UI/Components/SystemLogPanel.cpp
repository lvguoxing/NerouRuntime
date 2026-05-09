#include "SystemLogPanel.h"
#include "../../Core/CjkFontFallback.h"
#include "../../Core/Utf8Literals.h"

namespace nerou::ui {

using Level = nerou::core::SystemLogger::Level;

namespace {

juce::Font pickFont(float h, juce::Font::FontStyleFlags style = juce::Font::plain)
{
    return NerouCjkFont::createUiFont(h, style);
}

juce::String formatClock(juce::int64 millis)
{
    juce::Time t(millis);
    auto pad2 = [](int n) { return n < 10 ? juce::String("0") + juce::String(n) : juce::String(n); };
    juce::String s;
    s << pad2(t.getHours()) << ":" << pad2(t.getMinutes()) << ":" << pad2(t.getSeconds());
    return s;
}

} // namespace

// ============================================================================
// LogTable — 内部日志列表组件
// ============================================================================
class SystemLogPanel::LogTable : public juce::Component,
                                 private juce::TableListBoxModel
{
public:
    LogTable()
    {
        table_.setModel(this);
        table_.getHeader().addColumn(juce::String::fromUTF8(u8"时间"),    1, 80);
        table_.getHeader().addColumn(juce::String::fromUTF8(u8"级别"),    2, 64);
        table_.getHeader().addColumn(juce::String::fromUTF8(u8"分类"),    3, 110);
        table_.getHeader().addColumn(juce::String::fromUTF8(u8"消息"),    4, 520);
        table_.setRowHeight(22);
        table_.setHeaderHeight(24);
        table_.setColour(juce::TableListBox::backgroundColourId, juce::Colour(0xff0f1419));
        table_.setColour(juce::TableListBox::outlineColourId,    juce::Colour(0xff2a3540));
        table_.setOutlineThickness(1);
        addAndMakeVisible(table_);
    }

    void setEntries(juce::Array<nerou::core::SystemLogger::Entry> entries, bool autoScroll)
    {
        entries_ = std::move(entries);
        table_.updateContent();

        if (autoScroll && entries_.size() > 0)
            table_.scrollToEnsureRowIsOnscreen(entries_.size() - 1);
    }

    void copyAllToClipboard() const
    {
        juce::StringArray lines;
        for (const auto& e : entries_)
            lines.add(e.formatted());
        juce::SystemClipboard::copyTextToClipboard(lines.joinIntoString("\n"));
    }

    void resized() override { table_.setBounds(getLocalBounds()); }

private:
    juce::TableListBox table_;
    juce::Array<nerou::core::SystemLogger::Entry> entries_;

    int getNumRows() override { return entries_.size(); }

    void paintRowBackground(juce::Graphics& g, int rowNumber, int, int, bool selected) override
    {
        if (rowNumber < 0 || rowNumber >= entries_.size()) return;

        juce::Colour base = (rowNumber % 2 == 0)
                                ? juce::Colour(0xff121820)
                                : juce::Colour(0xff0f1419);
        if (selected)
            base = base.brighter(0.3f);
        g.fillAll(base);
    }

    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool) override
    {
        if (rowNumber < 0 || rowNumber >= entries_.size()) return;
        const auto& e = entries_.getReference(rowNumber);

        g.setFont(pickFont(12.0f));
        const juce::Rectangle<int> area(6, 0, width - 12, height);

        switch (columnId)
        {
            case 1:
                g.setColour(juce::Colour(0xffb0bec5));
                g.drawText(formatClock(e.timestampMs), area, juce::Justification::centredLeft, true);
                break;
            case 2:
                g.setColour(nerou::core::SystemLogger::levelColour(e.level));
                g.setFont(pickFont(12.0f, juce::Font::bold));
                g.drawText(e.levelTag(), area, juce::Justification::centredLeft, true);
                break;
            case 3:
                g.setColour(juce::Colour(0xff90caf9));
                g.drawText(e.category, area, juce::Justification::centredLeft, true);
                break;
            case 4:
            default:
                g.setColour(e.level == Level::Error
                                ? juce::Colour(0xffff8a80)
                                : (e.level == Level::Warning
                                       ? juce::Colour(0xffffcc80)
                                       : juce::Colour(0xffeceff1)));
                g.drawText(e.message, area, juce::Justification::centredLeft, true);
                break;
        }
    }

    void cellClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent&) override
    {
        if (rowNumber >= 0 && rowNumber < entries_.size())
        {
            juce::SystemClipboard::copyTextToClipboard(entries_.getReference(rowNumber).formatted());
        }
    }
};

// ============================================================================
// SystemLogPanel
// ============================================================================
SystemLogPanel::SystemLogPanel()
{
    setInterceptsMouseClicks(true, true);
    setWantsKeyboardFocus(true);

    table_.reset(new LogTable());
    addAndMakeVisible(*table_);

    addAndMakeVisible(titleLabel_);
    titleLabel_.setText(juce::String::fromUTF8(u8"系统日志"), juce::dontSendNotification);
    titleLabel_.setFont(pickFont(20.0f, juce::Font::bold));
    titleLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffECEFF1));

    addAndMakeVisible(subtitleLabel_);
    subtitleLabel_.setText(juce::String::fromUTF8(u8"按级别 / 分类 / 关键字过滤运行期日志，支持导出与文件持久化。"),
                           juce::dontSendNotification);
    subtitleLabel_.setFont(pickFont(11.5f));
    subtitleLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff90A4AE));

    addAndMakeVisible(statsLabel_);
    statsLabel_.setFont(pickFont(11.0f));
    statsLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff90A4AE));
    statsLabel_.setJustificationType(juce::Justification::centredRight);

    // Level combo
    levelCombo_.addItem(juce::String::fromUTF8(u8"\u8c03\u8bd5+"),   1);
    levelCombo_.addItem(juce::String::fromUTF8(u8"\u4fe1\u606f+"),   2);
    levelCombo_.addItem(juce::String::fromUTF8(u8"\u8b66\u544a+"),   3);
    levelCombo_.addItem(juce::String::fromUTF8(u8"\u9519\u8bef"),    4);
    levelCombo_.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(levelCombo_);

    categoryInput_.setTextToShowWhenEmpty(juce::String::fromUTF8(u8"分类关键字…"),
                                           juce::Colour(0xff90A4AE));
    categoryInput_.setFont(pickFont(12.0f));
    categoryInput_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1b242d));
    categoryInput_.setColour(juce::TextEditor::textColourId, juce::Colour(0xffECEFF1));
    categoryInput_.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2a3540));
    addAndMakeVisible(categoryInput_);

    searchInput_.setTextToShowWhenEmpty(juce::String::fromUTF8(u8"正文关键字…"),
                                         juce::Colour(0xff90A4AE));
    searchInput_.setFont(pickFont(12.0f));
    searchInput_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1b242d));
    searchInput_.setColour(juce::TextEditor::textColourId, juce::Colour(0xffECEFF1));
    searchInput_.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2a3540));
    addAndMakeVisible(searchInput_);

    autoScrollToggle_.setButtonText(juce::String::fromUTF8(u8"自动滚动"));
    autoScrollToggle_.setToggleState(true, juce::dontSendNotification);
    autoScrollToggle_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffECEFF1));
    addAndMakeVisible(autoScrollToggle_);

    pauseToggle_.setButtonText(juce::String::fromUTF8(u8"暂停"));
    pauseToggle_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffECEFF1));
    addAndMakeVisible(pauseToggle_);

    auto styleBtn = [](juce::TextButton& b, juce::Colour bg)
    {
        b.setColour(juce::TextButton::buttonColourId, bg);
        b.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffECEFF1));
    };

    clearBtn_.setButtonText     (juce::String::fromUTF8(u8"清空"));
    exportBtn_.setButtonText    (juce::String::fromUTF8(u8"导出…"));
    openFolderBtn_.setButtonText(juce::String::fromUTF8(u8"打开日志目录"));
    copyBtn_.setButtonText      (juce::String::fromUTF8(u8"复制全部"));
    closeBtn_.setButtonText     (juce::String::fromUTF8(u8"关闭"));

    styleBtn(clearBtn_,      juce::Colour(0xff37474f));
    styleBtn(exportBtn_,     juce::Colour(0xff37474f));
    styleBtn(openFolderBtn_, juce::Colour(0xff37474f));
    styleBtn(copyBtn_,       juce::Colour(0xff37474f));
    styleBtn(closeBtn_,      juce::Colour(0xff1976d2));

    for (auto* b : { &clearBtn_, &exportBtn_, &openFolderBtn_, &copyBtn_, &closeBtn_ })
        addAndMakeVisible(*b);

    wireControls();

    nerou::core::SystemLogger::getInstance().addChangeListener(this);
    applyFilterAndRefresh();
}

SystemLogPanel::~SystemLogPanel()
{
    nerou::core::SystemLogger::getInstance().removeChangeListener(this);
    if (getParentComponent())
        getParentComponent()->removeKeyListener(this);
}

void SystemLogPanel::wireControls()
{
    levelCombo_.onChange   = [this] { applyFilterAndRefresh(); };
    categoryInput_.onTextChange = [this] { applyFilterAndRefresh(); };
    searchInput_.onTextChange   = [this] { applyFilterAndRefresh(); };

    pauseToggle_.onClick = [this]
    {
        nerou::core::SystemLogger::getInstance().setPaused(pauseToggle_.getToggleState());
    };

    clearBtn_.onClick = [this]
    {
        nerou::core::SystemLogger::getInstance().clearBuffer();
        applyFilterAndRefresh();
    };

    copyBtn_.onClick = [this]
    {
        if (table_) table_->copyAllToClipboard();
    };

    openFolderBtn_.onClick = [this]
    {
        auto dir = nerou::core::SystemLogger::getInstance().getLogDirectory();
        if (dir.isDirectory())
            dir.revealToUser();
    };

    exportBtn_.onClick = [this]
    {
        auto defaultFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                               .getChildFile("NeuroRuntime_log_"
                                             + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S")
                                             + ".log");

        fileChooser_.reset(new juce::FileChooser(
            juce::String::fromUTF8(u8"导出系统日志"),
            defaultFile,
            "*.log;*.txt"));

        fileChooser_->launchAsync(
            juce::FileBrowserComponent::saveMode
                | juce::FileBrowserComponent::canSelectFiles
                | juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f == juce::File()) return;
                const bool ok = nerou::core::SystemLogger::getInstance().exportTo(f);
                if (ok)
                    nerou::core::SystemLogger::getInstance().logInfo(
                        "Logger", juce::String::fromUTF8(u8"日志已导出：") + f.getFullPathName());
                else
                    nerou::core::SystemLogger::getInstance().logError(
                        "Logger", juce::String::fromUTF8(u8"日志导出失败：") + f.getFullPathName());
            });
    };

    closeBtn_.onClick = [this] { hideOverlay(); };
}

// ── Overlay show/hide（与 SettingsDialog 风格一致） ─────────────────────────
void SystemLogPanel::showOverlay(juce::Component* parent)
{
    if (!parent) return;
    visible = true;
    setBounds(parent->getLocalBounds());
    parent->addAndMakeVisible(this);
    parent->addKeyListener(this);
    toFront(true);
    grabKeyboardFocus();

    setAlpha(0.0f);
    animator.animateComponent(this, getBounds(), 1.0f, 200, false, 0.0, 0.4);
    applyFilterAndRefresh();
    resized();
}

void SystemLogPanel::hideOverlay()
{
    visible = false;
    if (getParentComponent())
        getParentComponent()->removeKeyListener(this);

    animator.animateComponent(this, getBounds(), 0.0f, 150, true, 0.4, 0.0);
    juce::Timer::callAfterDelay(180, [this]() {
        if (getParentComponent())
            getParentComponent()->removeChildComponent(this);
        setAlpha(1.0f);
    });
}

// ── 事件 ────────────────────────────────────────────────────────────────────
void SystemLogPanel::changeListenerCallback(juce::ChangeBroadcaster*)
{
    applyFilterAndRefresh();
}

void SystemLogPanel::applyFilterAndRefresh()
{
    Level minLevel = Level::Debug;
    switch (levelCombo_.getSelectedId())
    {
        case 1: minLevel = Level::Debug;   break;
        case 2: minLevel = Level::Info;    break;
        case 3: minLevel = Level::Warning; break;
        case 4: minLevel = Level::Error;   break;
        default: break;
    }

    auto& logger = nerou::core::SystemLogger::getInstance();
    auto entries = logger.snapshotFiltered(minLevel,
                                           categoryInput_.getText().trim(),
                                           searchInput_.getText().trim());
    const int totalCount = logger.getEntryCount();
    const int shown      = entries.size();

    if (table_)
        table_->setEntries(std::move(entries), autoScrollToggle_.getToggleState());

    statsLabel_.setText(juce::String::fromUTF8(u8"显示 ")
                            + juce::String(shown)
                            + juce::String::fromUTF8(u8" / 共 ")
                            + juce::String(totalCount)
                            + juce::String::fromUTF8(u8" 条"),
                        juce::dontSendNotification);
}

// ── 绘制 ────────────────────────────────────────────────────────────────────
juce::Rectangle<int> SystemLogPanel::computeCardBounds() const
{
    const int w = juce::jmin(1040, getWidth()  - 64);
    const int h = juce::jmin(680,  getHeight() - 64);
    return {
        (getWidth()  - w) / 2,
        (getHeight() - h) / 2,
        w, h
    };
}

void SystemLogPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0x99000000));
    g.fillAll();

    const auto card = computeCardBounds();
    juce::Path cardPath;
    cardPath.addRoundedRectangle(card.toFloat(), 16.0f);

    juce::DropShadow ds(juce::Colour(0xbb000000), 28, { 0, 10 });
    ds.drawForPath(g, cardPath);

    g.setColour(juce::Colour(0xff162029));
    g.fillRoundedRectangle(card.toFloat(), 16.0f);
    g.setColour(juce::Colour(0xff2a3540));
    g.drawRoundedRectangle(card.toFloat().reduced(0.5f), 16.0f, 1.0f);
}

void SystemLogPanel::resized()
{
    const auto card = computeCardBounds();
    auto inner = card.reduced(20, 16);

    // 标题
    {
        auto titleRow = inner.removeFromTop(28);
        statsLabel_.setBounds(titleRow.removeFromRight(220));
        titleLabel_.setBounds(titleRow);
    }
    subtitleLabel_.setBounds(inner.removeFromTop(18));
    inner.removeFromTop(8);

    // 过滤行
    {
        auto filterRow = inner.removeFromTop(32);
        levelCombo_.setBounds(filterRow.removeFromLeft(130));
        filterRow.removeFromLeft(8);
        categoryInput_.setBounds(filterRow.removeFromLeft(180));
        filterRow.removeFromLeft(8);
        searchInput_.setBounds(filterRow.removeFromLeft(260));
        filterRow.removeFromLeft(16);
        autoScrollToggle_.setBounds(filterRow.removeFromLeft(110));
        pauseToggle_.setBounds(filterRow.removeFromLeft(90));
    }
    inner.removeFromTop(10);

    // 底部按钮行
    auto footer = inner.removeFromBottom(36);
    {
        closeBtn_.setBounds(footer.removeFromRight(96));
        footer.removeFromRight(8);
        exportBtn_.setBounds(footer.removeFromRight(96));
        footer.removeFromRight(8);
        copyBtn_.setBounds(footer.removeFromRight(96));
        footer.removeFromRight(8);
        openFolderBtn_.setBounds(footer.removeFromRight(128));
        footer.removeFromRight(8);
        clearBtn_.setBounds(footer.removeFromRight(96));
    }
    inner.removeFromBottom(8);

    if (table_)
        table_->setBounds(inner);
}

bool SystemLogPanel::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    if (!visible) return false;
    if (key == juce::KeyPress::escapeKey)
    {
        hideOverlay();
        return true;
    }
    return false;
}

void SystemLogPanel::mouseDown(const juce::MouseEvent& e)
{
    if (!computeCardBounds().contains(e.getPosition()))
        hideOverlay();
}

} // namespace nerou::ui
