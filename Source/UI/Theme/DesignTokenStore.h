#pragma once

#include "DesignTokens.h"

namespace nerou::ui {

/**
 * 设计令牌存储与管理
 * 单例模式，提供全局访问点
 */
class DesignTokenStore
{
public:
    static DesignTokenStore& getInstance() noexcept
    {
        static DesignTokenStore instance;
        return instance;
    }
    
    // 获取当前颜色方案
    const tokens::ColorScheme& getColors() const noexcept { return currentColors; }
    
    // 获取当前排版
    const tokens::Typography& getTypography() const noexcept { return currentTypography; }
    
    // 获取当前密度配置
    tokens::DensityConfig getDensity() const noexcept { 
        return tokens::DensityConfig::get(currentDensity); 
    }

    // 获取当前密度枚举（原始值）
    tokens::Density getDensityMode() const noexcept { return currentDensity; }
    
    // 设置主题
    void setTheme(tokens::ColorScheme scheme) noexcept
    {
        currentColors = scheme;
        notifyListeners();
    }
    
    // 设置暗色/亮色（只支持 Google Material 3 单一主题）
    void setDarkMode(bool dark) noexcept
    {
        isDark = dark;
        currentColors = tokens::presets::getClinicalWorkstation(dark);
        notifyListeners();
    }
    
    // 切换暗色/亮色
    void toggleDarkMode() noexcept
    {
        setDarkMode(!isDark);
    }
    
    // 预设主题（当前只保留 Google Material 3，其它样式已废弃）
    enum class PresetTheme {
        GoogleMaterial3 = 0
    };

    void setPresetTheme(PresetTheme /*preset*/) noexcept
    {
        currentPreset = PresetTheme::GoogleMaterial3;
        currentColors = tokens::presets::getClinicalWorkstation(isDark);
        notifyListeners();
    }
    
    // 设置密度
    void setDensity(tokens::Density density) noexcept
    {
        currentDensity = density;
        notifyListeners();
    }
    
    // 设置字体缩放
    void setFontScale(float scale) noexcept
    {
        fontScale = juce::jlimit(0.8f, 1.5f, scale);
        currentTypography = tokens::Typography::createDefault(14.0f * fontScale);
        notifyListeners();
    }
    
    // 监听器接口
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void onDesignTokensChanged() = 0;
    };
    
    void addListener(Listener* listener)
    {
        juce::ScopedLock lock(listenerLock);
        listeners.addIfNotAlreadyThere(listener);
    }
    
    void removeListener(Listener* listener)
    {
        juce::ScopedLock lock(listenerLock);
        listeners.removeFirstMatchingValue(listener);
    }
    
    // 获取波形通道颜色
    juce::Colour getWaveformChannelColor(int channelIndex) const noexcept
    {
        return currentColors.waveformChannelColors[channelIndex % 8];
    }
    
    // 获取状态颜色
    juce::Colour getStatusColor(bool success, bool warning = false, bool error = false) const noexcept
    {
        if (error) return currentColors.statusError;
        if (warning) return currentColors.statusWarning;
        if (success) return currentColors.statusSuccess;
        return currentColors.statusInfo;
    }
    
    // 获取表面颜色（带elevation）
    juce::Colour getSurfaceColor(int elevation = 0) const noexcept
    {
        switch (elevation) {
            case 0: return currentColors.surface;
            case 1: return currentColors.surfaceContainerLow;
            case 2: return currentColors.surfaceContainer;
            case 3: return currentColors.surfaceContainerHigh;
            case 4: return currentColors.surfaceContainerHighest;
            default: return currentColors.surface;
        }
    }
    
    // 获取状态层颜色
    juce::Colour getStateLayerColor(juce::Colour base, float opacity) const noexcept
    {
        return base.withAlpha(opacity);
    }
    
    // 获取禁用状态颜色
    juce::Colour getDisabledColor(bool isContainer = false) const noexcept
    {
        float opacity = isContainer ? 
            currentColors.disabledContainerOpacity : 
            currentColors.disabledContentOpacity;
        return currentColors.onSurface.withAlpha(opacity);
    }
    
    // 序列化/反序列化（用于保存用户偏好）
    void saveToProperties(juce::PropertiesFile& props) const
    {
        props.setValue("themePreset", static_cast<int>(currentPreset));
        props.setValue("isDark", isDark);
        props.setValue("density", static_cast<int>(currentDensity));
        props.setValue("fontScale", fontScale);
    }
    
    void loadFromProperties(const juce::PropertiesFile& props)
    {
        // 当前仅支持 Google Material 3，忽略历史 themePreset 取值
        currentPreset = PresetTheme::GoogleMaterial3;
        isDark = props.getBoolValue("isDark", false);
        currentDensity = static_cast<tokens::Density>(props.getIntValue("density", 1));
        fontScale = props.getDoubleValue("fontScale", 1.0);

        setPresetTheme(currentPreset);
        setFontScale(fontScale);
    }
    
    bool isDarkMode() const noexcept { return isDark; }
    PresetTheme getCurrentPreset() const noexcept { return currentPreset; }
    float getFontScale() const noexcept { return fontScale; }

private:
    DesignTokenStore()
        : currentColors(tokens::presets::getClinicalWorkstation(false))  // 默认临床工作站主题
        , currentTypography(tokens::Typography::createDefault())
        , currentDensity(tokens::Density::Comfortable)
        , isDark(false)
        , fontScale(1.0f)
        , currentPreset(PresetTheme::GoogleMaterial3)
    {
    }
    
    ~DesignTokenStore() = default;
    DesignTokenStore(const DesignTokenStore&) = delete;
    DesignTokenStore& operator=(const DesignTokenStore&) = delete;
    
    void notifyListeners()
    {
        juce::ScopedLock lock(listenerLock);
        for (auto* listener : listeners)
        {
            if (listener != nullptr)
                listener->onDesignTokensChanged();
        }
    }
    
    tokens::ColorScheme currentColors;
    tokens::Typography currentTypography;
    tokens::Density currentDensity;
    bool isDark;
    float fontScale;
    PresetTheme currentPreset;
    
    juce::Array<Listener*> listeners;
    juce::CriticalSection listenerLock;
};

} // namespace nerou::ui
