#include "OverviewPage.h"
#include "../Components/MaterialSnackbar.h"
#include "../../Core/ChineseLocale.h"   // 中文本地化：NR_STR / 日期 / Tooltip

namespace nerou::ui {

OverviewPage::OverviewPage()
{
    // 注册监听器
    app::GlobalContextStore::getInstance().addListener(this);
    DesignTokenStore::getInstance().addListener(this);
    
    // 设置滚动视图
    viewport = std::make_unique<juce::Viewport>();
    viewport->setViewedComponent(&content, false);
    addAndMakeVisible(viewport.get());
    
    // 创建UI组件
    rebuildUI();
    applyTheme();
}

OverviewPage::~OverviewPage()
{
    app::GlobalContextStore::getInstance().removeListener(this);
    DesignTokenStore::getInstance().removeListener(this);
}

void OverviewPage::paint(juce::Graphics& g)
{
    auto& tokens = DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();

    // 微信风格：浅灰全局背景
    g.fillAll(colors.background);
}

void OverviewPage::resized()
{
    auto bounds = getLocalBounds();
    viewport->setBounds(bounds);

    // 微信风格：两列卡片布局（主内容 + 侧边状态）
    const int margin = 16;
    const int gap = 12;
    const int totalWidth = bounds.getWidth() - margin * 2;
    const int rightColW = juce::jmin(280, totalWidth / 3);
    const int leftColW = totalWidth - rightColW - gap;

    // 动态计算内容高度
    int contentHeight = juce::jmax(bounds.getHeight(), 900);
    content.setSize(bounds.getWidth(), contentHeight);

    auto area = content.getLocalBounds().reduced(margin);

    // ── 左列（主内容）─────────────────────────────────────────────────────────
    auto leftCol = area.removeFromLeft(leftColW);
    area.removeFromLeft(gap);
    auto rightCol = area;

    // 智能建议区域
    nextActionTitle.setBounds(leftCol.removeFromTop(24));
    leftCol.removeFromTop(8);
    if (nextActionCard) {
        nextActionCard->setBounds(leftCol.removeFromTop(100));
        leftCol.removeFromTop(gap);
    }
    if (suggestionChips) {
        suggestionChips->setBounds(leftCol.removeFromTop(36));
        leftCol.removeFromTop(gap);
    }

    // 分隔空间
    leftCol.removeFromTop(8);

    // 统计卡片（2 列横排）
    statsTitle.setBounds(leftCol.removeFromTop(24));
    leftCol.removeFromTop(8);
    {
        const int cardW = (leftColW - gap) / 2;
        auto statsRow1 = leftCol.removeFromTop(88);
        auto statsRow2 = leftCol.removeFromTop(88);
        leftCol.removeFromTop(gap);

        if (statCards[0]) statCards[0]->setBounds(statsRow1.removeFromLeft(cardW));
        statsRow1.removeFromLeft(gap);
        if (statCards[1]) statCards[1]->setBounds(statsRow1);

        if (statCards[2]) statCards[2]->setBounds(statsRow2.removeFromLeft(cardW));
        statsRow2.removeFromLeft(gap);
        if (statCards[3]) statCards[3]->setBounds(statsRow2);
    }

    // 最近项目列表
    leftCol.removeFromTop(8);
    recentProjectsTitle.setBounds(leftCol.removeFromTop(24));
    leftCol.removeFromTop(8);
    for (int i = 0; i < projectCards.size(); ++i) {
        projectCards[i]->setBounds(leftCol.removeFromTop(60));
        leftCol.removeFromTop(6);
    }

    // ── 右列（状态 + 快捷操作）───────────────────────────────────────────────
    contextTitle.setBounds(rightCol.removeFromTop(24));
    rightCol.removeFromTop(8);
    if (contextChips) {
        contextChips->setBounds(rightCol.removeFromTop(120));
        rightCol.removeFromTop(gap);
    }

    quickActionsTitle.setBounds(rightCol.removeFromTop(24));
    rightCol.removeFromTop(8);

    // 快捷操作按钮（垂直排列）
    newProjectBtn.setBounds(rightCol.removeFromTop(36));
    rightCol.removeFromTop(6);
    importDataBtn.setBounds(rightCol.removeFromTop(36));
    rightCol.removeFromTop(6);
    connectDeviceBtn.setBounds(rightCol.removeFromTop(36));
    rightCol.removeFromTop(6);
    openSettingsBtn.setBounds(rightCol.removeFromTop(36));
}

void OverviewPage::rebuildUI()
{
    content.removeAllChildren();
    
    auto& tokens = DesignTokenStore::getInstance();
    
    // 标题 — 使用 NR_STR 宽字符字面量，与文件编码无关
    nextActionTitle.setText(NR_STR("智能建议"), juce::dontSendNotification);
    nextActionTitle.setJustificationType(juce::Justification::left);
    content.addAndMakeVisible(nextActionTitle);
    
    // 智能下一步卡片
    nextActionCard = std::make_unique<ActionCard>(NR_STR("开始新任务"), NR_STR("执行"));
    nextActionCard->onAction = [this]() {
        auto actions = app::Context().getSuggestedNextActions(1);
        if (actions.isEmpty()) return;
        
        // 执行第一个建议动作（V3 文件优先）
        auto& action = actions.getReference(0);
        if (action.actionId == "create_project")
        {
            if (onCreateNewProject) onCreateNewProject();
        }
        else if (action.actionId == "import_file")
        {
            if (onImportData) onImportData();
        }
        else if (action.actionId == "preprocess_data")
        {
            if (onNavigateToPage) onNavigateToPage("preparation");
        }
        else if (action.actionId == "train_model")
        {
            if (onNavigateToPage) onNavigateToPage("training");
        }
        else if (action.actionId == "validate_model")
        {
            if (onNavigateToPage) onNavigateToPage("validation");
        }
    };
    content.addAndMakeVisible(nextActionCard.get());
    
    // 建议Chips
    suggestionChips = std::make_unique<ChipGroup>(ChipGroup::SelectionMode::None);
    content.addAndMakeVisible(suggestionChips.get());
    
    // 统计标题
    statsTitle.setText(NR_STR("今日概览"), juce::dontSendNotification);
    statsTitle.setJustificationType(juce::Justification::left);
    content.addAndMakeVisible(statsTitle);
    
    // 统计卡片
    for (int i = 0; i < 4; ++i)
    {
        statCards[i] = std::make_unique<DataCard>(MaterialCard::Type::Elevated);
        statCards[i]->setElevation(1);
        content.addAndMakeVisible(statCards[i].get());
    }
    
    // 配置统计卡片占位（实际值由 updateStats() 在 rebuildUI 末尾立刻覆盖）
    // V3 文件优先：第 1 张从 "采集会话" 改为 "数据集" 占位
    statCards[0]->setIcon(juce::String(L"\u25a0"), tokens.getColors().statusInfo);
    statCards[0]->setTitle(NR_STR("数据集"));
    statCards[0]->setSubtitle(NR_STR("当前已就绪"));
    statCards[0]->setValue("--", "");

    statCards[1]->setIcon(juce::String(L"\u4eba"), tokens.getColors().statusInfo);
    statCards[1]->setTitle(NR_STR("受试者"));
    statCards[1]->setSubtitle(NR_STR("当前所属"));
    statCards[1]->setValue("--", "");

    statCards[2]->setIcon(juce::String(L"\u25b2"), tokens.getColors().statusInfo);
    statCards[2]->setTitle(NR_STR("当前模型"));
    statCards[2]->setSubtitle(NR_STR("等待训练"));
    statCards[2]->setValue("--", "");

    statCards[3]->setIcon(juce::String(L"\u2713"), tokens.getColors().statusSuccess);
    statCards[3]->setTitle(NR_STR("交付状态"));
    statCards[3]->setSubtitle(NR_STR("等待验证"));
    statCards[3]->setValue("--", "");
    
    // 最近项目标题
    recentProjectsTitle.setText(NR_STR("最近项目"), juce::dontSendNotification);
    recentProjectsTitle.setJustificationType(juce::Justification::left);
    content.addAndMakeVisible(recentProjectsTitle);
    
    // 项目卡片
    projectCards.clear();
    
    // 添加快捷操作按钮
    quickActionsTitle.setText(NR_STR("快捷操作"), juce::dontSendNotification);
    quickActionsTitle.setJustificationType(juce::Justification::left);
    content.addAndMakeVisible(quickActionsTitle);
    
    // V3 文件优先：主行动 = 导入文件（filled），新建项目降为次要
    importDataBtn.setButtonText(NR_STR("导入训练文件"));
    importDataBtn.setTooltip(NR_STR("从 NPZ / EDF / BDF / CSV 等格式导入数据集（训练入口）"));
    importDataBtn.getProperties().set("filled", true);
    importDataBtn.onClick = [this]() {
        if (onImportData) onImportData();
    };
    content.addAndMakeVisible(importDataBtn);

    newProjectBtn.setButtonText(NR_STR("新建项目"));
    newProjectBtn.setTooltip(NR_STR("创建一个新的训练项目"));
    newProjectBtn.setClickingTogglesState(false);
    newProjectBtn.getProperties().set("outlined", true);
    newProjectBtn.onClick = [this]() {
        if (onCreateNewProject) onCreateNewProject();
    };
    content.addAndMakeVisible(newProjectBtn);

    connectDeviceBtn.setButtonText(NR_STR("录制工具（侧路）"));
    connectDeviceBtn.setTooltip(NR_STR("打开录制工具：把 EEG 设备的实时信号录制为 NPZ 文件，再回到主流程导入"));
    connectDeviceBtn.getProperties().set("text", true);                    // 弱化样式（侧路）
    connectDeviceBtn.onClick = [this]() {
        if (onConnectDevice) onConnectDevice();
        if (onNavigateToPage) onNavigateToPage("acquisition");
    };
    content.addAndMakeVisible(connectDeviceBtn);
    
    openSettingsBtn.setButtonText(NR_STR("设置"));
    openSettingsBtn.setTooltip(NR_STR("打开系统设置"));
    openSettingsBtn.getProperties().set("text", true);
    openSettingsBtn.onClick = [this]() {
        if (onOpenSettings) onOpenSettings();
    };
    content.addAndMakeVisible(openSettingsBtn);
    
    // 上下文区域
    contextTitle.setText(NR_STR("当前状态"), juce::dontSendNotification);
    contextTitle.setJustificationType(juce::Justification::left);
    content.addAndMakeVisible(contextTitle);
    
    contextChips = std::make_unique<ChipGroup>(ChipGroup::SelectionMode::None);
    content.addAndMakeVisible(contextChips.get());
    
    // 更新所有数据
    updateNextActions();
    updateStats();
    updateRecentProjects();
    updateContextChips();
}

void OverviewPage::applyTheme()
{
    auto& tokens = DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();
    const auto& typo = tokens.getTypography();

    // 微信风格：section 标题用深灰色小标题字体
    auto sectionFont = typo.titleSmall;

    auto applySection = [&](juce::Label& l) {
        l.setFont(sectionFont);
        l.setColour(juce::Label::textColourId, colors.onSurfaceVariant);
        l.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    };

    applySection(nextActionTitle);
    applySection(statsTitle);
    applySection(recentProjectsTitle);
    applySection(contextTitle);
    applySection(quickActionsTitle);

    // 快捷操作按钮：微信绿 + 轮廓样式
    newProjectBtn.setColour(juce::TextButton::buttonColourId,  colors.primary);
    newProjectBtn.setColour(juce::TextButton::textColourOffId, colors.onPrimary);

    importDataBtn.setColour(juce::TextButton::buttonColourId,   colors.surface);
    importDataBtn.setColour(juce::TextButton::textColourOffId,  colors.primary);

    connectDeviceBtn.setColour(juce::TextButton::buttonColourId,  colors.surface);
    connectDeviceBtn.setColour(juce::TextButton::textColourOffId, colors.primary);

    openSettingsBtn.setColour(juce::TextButton::buttonColourId,  colors.surface);
    openSettingsBtn.setColour(juce::TextButton::textColourOffId, colors.onSurfaceVariant);
}

void OverviewPage::updateNextActions()
{
    auto actions = app::Context().getSuggestedNextActions(3);
    
    if (actions.isEmpty())
    {
        nextActionCard->setDescription(NR_STR("暂无待办任务。所有系统运行正常。"));
        nextActionCard->setTitle(NR_STR("一切就绪"));
        return;
    }
    
    // 更新主建议卡片
    auto& mainAction = actions.getReference(0);
    nextActionCard->setTitle(mainAction.title);
    nextActionCard->setDescription(mainAction.description);
    
    // 更新Chips
    suggestionChips->clearChips();
    for (int i = 1; i < actions.size(); ++i)
    {
        auto& action = actions.getReference(i);
        suggestionChips->addChip(action.title, MaterialChip::Type::Suggestion);
    }
    
    suggestionChips->resized();
}

void OverviewPage::updateStats()
{
    auto& context = app::Context();
    auto& device  = context.getDeviceState();

    // 卡片 0：设备状态
    if (statCards[0])
    {
        juce::Colour iconCol;
        juce::String statusStr;
        switch (device.status)
        {
        case app::DeviceState::Status::Acquiring:
            iconCol   = juce::Colour(0xFF07C160);
            statusStr = juce::String(L"\u91c7\u96c6\u4e2d");
            break;
        case app::DeviceState::Status::Connected:
            iconCol   = juce::Colour(0xFF1677FF);
            statusStr = juce::String(L"\u5df2\u8fde\u63a5");
            break;
        case app::DeviceState::Status::Connecting:
            iconCol   = juce::Colour(0xFFFA8C16);
            statusStr = juce::String(L"\u8fde\u63a5\u4e2d\u2026");
            break;
        default:
            iconCol   = juce::Colour(0xFF999999);
            statusStr = juce::String(L"\u672a\u8fde\u63a5");
            break;
        }
        statCards[0]->setIcon(juce::String(L"\u25cf"), iconCol);
        statCards[0]->setTitle(juce::String(L"\u91c7\u96c6\u8bbe\u5907"));
        if (device.status == app::DeviceState::Status::Connected ||
            device.status == app::DeviceState::Status::Acquiring)
        {
            statCards[0]->setValue(
                juce::String(device.channelCount) + "ch",
                juce::String(device.sampleRate, 0) + "Hz");
            statCards[0]->setSubtitle(statusStr + " | " + device.montageType);
        }
        else
        {
            statCards[0]->setValue("--", "");
            statCards[0]->setSubtitle(statusStr);
        }
    }

    // 卡片 1：受试者
    if (statCards[1])
    {
        if (context.hasCurrentSubject())
        {
            auto& subj = context.getCurrentSubject();
            statCards[1]->setIcon(juce::String(L"\u4eba"), juce::Colour(0xFF722ED1));
            statCards[1]->setTitle(juce::String(L"\u5f53\u524d\u53d7\u8bd5\u8005"));
            statCards[1]->setValue(subj.getDisplayName(), "");
            statCards[1]->setSubtitle(juce::String(L"\u5df2\u91c7\u96c6 ")
                + juce::String(subj.sessionCount) + juce::String(L" \u6b21"));
        }
        else
        {
            statCards[1]->setIcon(juce::String(L"\u4eba"), juce::Colour(0xFF999999));
            statCards[1]->setTitle(juce::String(L"\u5f53\u524d\u53d7\u8bd5\u8005"));
            statCards[1]->setValue(juce::String(L"\u672a\u9009\u62e9"), "");
            statCards[1]->setSubtitle("");
        }
    }

    // 卡片 2：数据集
    if (statCards[2])
    {
        if (context.hasCurrentDataset())
        {
            auto& ds = context.getCurrentDataset();
            statCards[2]->setIcon(juce::String(L"\u25a0"),
                ds.isProcessed ? juce::Colour(0xFF07C160) : juce::Colour(0xFFFA8C16));
            statCards[2]->setTitle(juce::String(L"\u5f53\u524d\u6570\u636e\u96c6"));
            statCards[2]->setValue(
                juce::String(ds.sampleCount),
                juce::String(L"\u6837\u672c"));
            statCards[2]->setSubtitle(
                ds.isProcessed
                    ? juce::String(L"\u5df2\u9884\u5904\u7406")
                    : juce::String(L"\u672a\u9884\u5904\u7406"));
        }
        else
        {
            statCards[2]->setIcon(juce::String(L"\u25a0"), juce::Colour(0xFF999999));
            statCards[2]->setTitle(juce::String(L"\u5f53\u524d\u6570\u636e\u96c6"));
            statCards[2]->setValue("--", "");
            statCards[2]->setSubtitle(juce::String(L"\u5c1a\u672a\u51c6\u5907"));
        }
    }

    // 卡片 3：模型
    if (statCards[3])
    {
        if (context.hasCurrentModel())
        {
            auto& model = context.getCurrentModel();
            statCards[3]->setIcon(juce::String(L"\u25b2"),
                model.isValidated ? juce::Colour(0xFF07C160) : juce::Colour(0xFF1677FF));
            statCards[3]->setTitle(juce::String(L"\u5f53\u524d\u6a21\u578b"));
            statCards[3]->setValue(model.name.isNotEmpty() ? model.name : model.id, "");
            statCards[3]->setSubtitle(
                model.isValidated
                    ? juce::String(L"\u5df2\u9a8c\u8bc1 \u2713")
                    : juce::String(L"\u672a\u9a8c\u8bc1"));
        }
        else
        {
            statCards[3]->setIcon(juce::String(L"\u25b2"), juce::Colour(0xFF999999));
            statCards[3]->setTitle(juce::String(L"\u5f53\u524d\u6a21\u578b"));
            statCards[3]->setValue("--", "");
            statCards[3]->setSubtitle(juce::String(L"\u5c1a\u672a\u8bad\u7ec3"));
        }
    }
}

void OverviewPage::updateRecentProjects()
{
    auto projects = app::Context().getRecentProjects(3);
    auto& tokens = DesignTokenStore::getInstance();
    const auto& colors = tokens.getColors();

    projectCards.clear();

    if (projects.isEmpty())
        return;

    for (const auto& project : projects)
    {
        // 使用 DataCard 子类以便显示图标 / 标题 / 副标题
        auto* card = new DataCard(MaterialCard::Type::Outlined);
        card->setInteractive(true);

        card->setIcon(juce::String(L"\U0001F4C1"), colors.primary);
        card->setTitle(project.name.isNotEmpty() ? project.name : juce::String(L"\u672a\u547d\u540d\u9879\u76ee"));

        // 副标题：优先显示统计摘要（受试者/数据集/模型计数），否则显示描述文案
        juce::String summary;
        if (project.subjectCount > 0 || project.datasetCount > 0 || project.modelCount > 0)
        {
            summary = juce::String(L"\u53d7\u8bd5\u8005 ") + juce::String(project.subjectCount)
                + juce::String(L" \u00b7 \u6570\u636e\u96c6 ") + juce::String(project.datasetCount)
                + juce::String(L" \u00b7 \u6a21\u578b ") + juce::String(project.modelCount);
        }
        else if (project.description.isNotEmpty())
        {
            summary = project.description;
        }
        else
        {
            summary = juce::String(L"\u70b9\u51fb\u6253\u5f00\u9879\u76ee");
        }
        card->setSubtitle(summary);

        // 按值捕获 project，避免悬挂引用
        app::ProjectInfo proj = project;
        card->onClick = [this, proj]() {
            app::Context().setCurrentProject(proj);
            if (onNavigateToPage) onNavigateToPage("overview");
        };

        projectCards.add(card);
        content.addAndMakeVisible(card);
    }

    // 添加完卡片后强制重新布局（projectCards 数量变化会影响 leftCol 高度）
    resized();
}

void OverviewPage::updateContextChips()
{
    auto& context = app::Context();
    
    contextChips->clearChips();
    
    // 项目Chip
    if (context.hasCurrentProject())
    {
        auto& project = context.getCurrentProject();
        auto* chip = new MaterialChip(juce::String(L"\U0001F4C1 ") + project.name, MaterialChip::Type::Assist);
        chip->setLeadingIcon(juce::String(L"\U0001F4C1"));
        contextChips->addChip(chip);
    }
    
    // 受试者Chip
    if (context.hasCurrentSubject())
    {
        auto& subject = context.getCurrentSubject();
        auto* chip = new MaterialChip(juce::String(L"\U0001F464 ") + subject.getDisplayName(), MaterialChip::Type::Assist);
        chip->setLeadingIcon(juce::String(L"\U0001F464"));
        contextChips->addChip(chip);
    }
    
    // 设备Chip
    if (context.isDeviceConnected())
    {
        auto& device = context.getDeviceState();
        auto* chip = new MaterialChip(juce::String(L"\U0001F50C ") + juce::String(device.channelCount) + "ch @ " +
                                      juce::String(device.sampleRate, 0) + "Hz",
                                      MaterialChip::Type::Assist);
        chip->setLeadingIcon(juce::String(L"\U0001F50C"));
        contextChips->addChip(chip);
    }
    
    // 模型Chip
    if (context.hasCurrentModel())
    {
        auto& model = context.getCurrentModel();
        auto* chip = new MaterialChip(juce::String(L"\U0001F9E0 ") + model.name, MaterialChip::Type::Assist);
        chip->setLeadingIcon(juce::String(L"\U0001F9E0"));
        contextChips->addChip(chip);
    }
    
    contextChips->resized();
}

// ============================================================================
// GlobalContextStore::Listener 实现
// ============================================================================

void OverviewPage::onProjectChanged(const app::ProjectInfo& newProject)
{
    juce::ignoreUnused(newProject);
    updateNextActions();
    updateStats();
    updateRecentProjects();
    updateContextChips();
    repaint();
}

void OverviewPage::onSubjectChanged(const app::SubjectInfo& newSubject)
{
    juce::ignoreUnused(newSubject);
    updateStats();
    updateContextChips();
    repaint();
}

void OverviewPage::onDeviceStateChanged(const app::DeviceState& newState)
{
    juce::ignoreUnused(newState);
    updateNextActions();
    updateStats();
    updateContextChips();
    repaint();
}

void OverviewPage::onModelChanged(const app::ModelInfo& newModel)
{
    juce::ignoreUnused(newModel);
    updateNextActions();
    updateStats();
    updateContextChips();
    repaint();
}

void OverviewPage::onDatasetChanged(const app::DatasetInfo& newDataset)
{
    juce::ignoreUnused(newDataset);
    updateNextActions();
    updateStats();
    repaint();
}

void OverviewPage::onTaskProgressChanged(const app::TaskProgress& progress)
{
    juce::ignoreUnused(progress);
    updateStats();
    repaint();
}

void OverviewPage::onContextChanged()
{
    updateNextActions();
    updateStats();
    updateContextChips();
    repaint();
}

// ============================================================================
// DesignTokenStore::Listener 实现
// ============================================================================

void OverviewPage::onDesignTokensChanged()
{
    applyTheme();
    repaint();
}

} // namespace nerou::ui
