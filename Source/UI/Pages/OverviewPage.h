#pragma once

#include <JuceHeader.h>
#include "../Components/MaterialCard.h"
#include "../Components/MaterialChip.h"
#include "../../Application/GlobalContextStore.h"

namespace nerou::ui {

/**
 * 总览页 - 智能仪表盘
 * 
 * 功能:
 * 1. 智能下一步行动（基于当前状态）
 * 2. 今日概览统计
 * 3. 最近项目列表
 * 4. 全局状态摘要
 * 5. 快捷操作入口
 */
class OverviewPage : public juce::Component,
                     public app::GlobalContextStore::Listener,
                     public DesignTokenStore::Listener
{
public:
    OverviewPage();
    ~OverviewPage() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // GlobalContextStore::Listener
    void onProjectChanged(const app::ProjectInfo& newProject) override;
    void onSubjectChanged(const app::SubjectInfo& newSubject) override;
    void onDeviceStateChanged(const app::DeviceState& newState) override;
    void onModelChanged(const app::ModelInfo& newModel) override;
    void onDatasetChanged(const app::DatasetInfo& newDataset) override;
    void onTaskProgressChanged(const app::TaskProgress& progress) override;
    void onContextChanged() override;
    
    // DesignTokenStore::Listener
    void onDesignTokensChanged() override;
    
    // 页面导航回调
    std::function<void(const juce::String& pageId)> onNavigateToPage;
    std::function<void()> onCreateNewProject;
    std::function<void()> onImportData;
    std::function<void()> onConnectDevice;
    std::function<void()> onOpenSettings;

private:
    void rebuildUI();
    void updateNextActions();
    void updateStats();
    void updateRecentProjects();
    void updateContextChips();
    
    // 主题更新
    void applyTheme();
    
    // 滚动视图
    std::unique_ptr<juce::Viewport> viewport;
    juce::Component content;
    
    // 智能下一步区域
    juce::Label nextActionTitle;
    std::unique_ptr<ActionCard> nextActionCard;
    std::unique_ptr<ChipGroup> suggestionChips;
    
    // 今日概览统计
    juce::Label statsTitle;
    std::array<std::unique_ptr<DataCard>, 4> statCards;
    
    // 最近项目
    juce::Label recentProjectsTitle;
    juce::OwnedArray<MaterialCard> projectCards;
    juce::TextButton viewAllProjectsBtn;
    
    // 全局上下文摘要（Chip形式）
    juce::Label contextTitle;
    std::unique_ptr<ChipGroup> contextChips;
    std::unique_ptr<StatusChip> deviceStatusChip;
    std::unique_ptr<StatusChip> taskStatusChip;
    
    // 快捷操作
    juce::Label quickActionsTitle;
    juce::TextButton newProjectBtn;
    juce::TextButton importDataBtn;
    juce::TextButton connectDeviceBtn;
    juce::TextButton openSettingsBtn;
    
    // 欢迎/空状态
    juce::Label welcomeTitle;
    juce::Label welcomeSubtitle;
    bool showWelcomeState = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OverviewPage)
};

} // namespace nerou::ui
