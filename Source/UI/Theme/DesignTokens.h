#pragma once

#include <JuceHeader.h>

/**
 * Material 3 设计令牌系统
 * 参考: https://m3.material.io/
 * 
 * 本系统提供完整的颜色、排版、间距、形状和动画令牌
 * 支持亮色/暗色主题、三种配色方案、可访问性适配
 */

namespace nerou::ui::tokens {

// ============================================================================
// 1. 颜色令牌 - Material 3 色彩系统
// ============================================================================

struct ColorScheme {
    // 主要颜色
    juce::Colour primary;
    juce::Colour onPrimary;
    juce::Colour primaryContainer;
    juce::Colour onPrimaryContainer;
    
    // 次要颜色
    juce::Colour secondary;
    juce::Colour onSecondary;
    juce::Colour secondaryContainer;
    juce::Colour onSecondaryContainer;
    
    // 第三颜色
    juce::Colour tertiary;
    juce::Colour onTertiary;
    juce::Colour tertiaryContainer;
    juce::Colour onTertiaryContainer;
    
    // 错误颜色
    juce::Colour error;
    juce::Colour onError;
    juce::Colour errorContainer;
    juce::Colour onErrorContainer;
    
    // 表面颜色
    juce::Colour surface;
    juce::Colour onSurface;
    juce::Colour surfaceVariant;
    juce::Colour onSurfaceVariant;
    juce::Colour surfaceContainer;
    juce::Colour surfaceContainerHigh;
    juce::Colour surfaceContainerHighest;
    juce::Colour surfaceContainerLow;
    juce::Colour surfaceContainerLowest;
    
    // 背景与轮廓
    juce::Colour background;
    juce::Colour onBackground;
    juce::Colour outline;
    juce::Colour outlineVariant;
    
    // 状态层透明度 (用于悬停、按下、拖拽、禁用)
    float hoverStateLayerOpacity = 0.08f;
    float pressedStateLayerOpacity = 0.12f;
    float draggedStateLayerOpacity = 0.16f;
    float focusStateLayerOpacity = 0.12f;
    float disabledContainerOpacity = 0.38f;
    float disabledContentOpacity = 0.38f;
    
    // 阴影颜色
    juce::Colour shadow;
    juce::Colour scrim;
    
    // 波形专用颜色
    juce::Colour waveformBackground;
    juce::Colour waveformGrid;
    juce::Colour waveformChannelColors[8]; // 8通道颜色循环
    juce::Colour waveformActiveChannel;
    juce::Colour waveformTriggerLine;
    juce::Colour waveformMeasurement;
    
    // 状态指示器颜色
    juce::Colour statusSuccess;
    juce::Colour statusWarning;
    juce::Colour statusError;
    juce::Colour statusInfo;
    juce::Colour statusRunning;
    juce::Colour statusIdle;
};

// 预设配色方案
namespace presets {
    // 医学蓝 - 默认专业配色
    inline ColorScheme getMedicalBlue(bool isDark = false) {
        ColorScheme cs;
        if (!isDark) {
            // 亮色主题
            cs.primary = juce::Colour(0xFF006495);           // #006495
            cs.onPrimary = juce::Colour(0xFFFFFFFF);         // white
            cs.primaryContainer = juce::Colour(0xFFCBE6FF); // #CBE6FF
            cs.onPrimaryContainer = juce::Colour(0xFF001E30); // #001E30
            
            cs.secondary = juce::Colour(0xFF50606F);         // #50606F
            cs.onSecondary = juce::Colour(0xFFFFFFFF);
            cs.secondaryContainer = juce::Colour(0xFFD3E4F5); // #D3E4F5
            cs.onSecondaryContainer = juce::Colour(0xFF0D1D2A);
            
            cs.tertiary = juce::Colour(0xFF65587B);        // #65587B
            cs.onTertiary = juce::Colour(0xFFFFFFFF);
            cs.tertiaryContainer = juce::Colour(0xFFEBDDFB); // #EBDDFB
            cs.onTertiaryContainer = juce::Colour(0xFF201634);
            
            cs.error = juce::Colour(0xFFBA1A1A);           // #BA1A1A
            cs.onError = juce::Colour(0xFFFFFFFF);
            cs.errorContainer = juce::Colour(0xFFFFDAD6);    // #FFDAD6
            cs.onErrorContainer = juce::Colour(0xFF410002);
            
            cs.surface = juce::Colour(0xFFFCFCFF);         // #FCFCFF
            cs.onSurface = juce::Colour(0xFF1A1C1E);
            cs.surfaceVariant = juce::Colour(0xFFDDE3EA);   // #DDE3EA
            cs.onSurfaceVariant = juce::Colour(0xFF41474D);
            cs.surfaceContainer = juce::Colour(0xFFF0F4F8); // #F0F4F8
            cs.surfaceContainerHigh = juce::Colour(0xFFE8ECF0);
            cs.surfaceContainerHighest = juce::Colour(0xFFE1E5E9);
            cs.surfaceContainerLow = juce::Colour(0xFFF5F9FD);
            cs.surfaceContainerLowest = juce::Colour(0xFFFFFFFF);
            
            cs.background = juce::Colour(0xFFFCFCFF);
            cs.onBackground = juce::Colour(0xFF1A1C1E);
            cs.outline = juce::Colour(0xFF71787E);
            cs.outlineVariant = juce::Colour(0xFFC1C7CE);
            
            cs.shadow = juce::Colour(0xFF000000).withAlpha(0.15f);
            cs.scrim = juce::Colour(0xFF000000).withAlpha(0.60f);
            
            cs.waveformBackground = juce::Colour(0xFFFAFAFA);
            cs.waveformGrid = juce::Colour(0xFFE0E0E0);
        } else {
            // 暗色主题
            cs.primary = juce::Colour(0xFF8FCDFF);          // #8FCDFF
            cs.onPrimary = juce::Colour(0xFF003450);        // #003450
            cs.primaryContainer = juce::Colour(0xFF004B71); // #004B71
            cs.onPrimaryContainer = juce::Colour(0xFFCBE6FF);
            
            cs.secondary = juce::Colour(0xFFB7C8D9);       // #B7C8D9
            cs.onSecondary = juce::Colour(0xFF22323F);
            cs.secondaryContainer = juce::Colour(0xFF384956);
            cs.onSecondaryContainer = juce::Colour(0xFFD3E4F5);
            
            cs.tertiary = juce::Colour(0xFFD0BFE8);        // #D0BFE8
            cs.onTertiary = juce::Colour(0xFF372A4D);
            cs.tertiaryContainer = juce::Colour(0xFF4D4063);
            cs.onTertiaryContainer = juce::Colour(0xFFEBDDFB);
            
            cs.error = juce::Colour(0xFFFFB4AB);
            cs.onError = juce::Colour(0xFF690005);
            cs.errorContainer = juce::Colour(0xFF93000A);
            cs.onErrorContainer = juce::Colour(0xFFFFB4AB);
            
            cs.surface = juce::Colour(0xFF0F1418);          // 深海军蓝背景
            cs.onSurface = juce::Colour(0xFFE1E2E5);
            cs.surfaceVariant = juce::Colour(0xFF41474D);
            cs.onSurfaceVariant = juce::Colour(0xFFC1C7CE);
            cs.surfaceContainer = juce::Colour(0xFF1A2026);
            cs.surfaceContainerHigh = juce::Colour(0xFF252B31);
            cs.surfaceContainerHighest = juce::Colour(0xFF30363C);
            cs.surfaceContainerLow = juce::Colour(0xFF151A1E);
            cs.surfaceContainerLowest = juce::Colour(0xFF0A0F12);
            
            cs.background = juce::Colour(0xFF0F1418);
            cs.onBackground = juce::Colour(0xFFE1E2E5);
            cs.outline = juce::Colour(0xFF8B9198);
            cs.outlineVariant = juce::Colour(0xFF41474D);
            
            cs.shadow = juce::Colour(0xFF000000).withAlpha(0.30f);
            cs.scrim = juce::Colour(0xFF000000).withAlpha(0.80f);
            
            cs.waveformBackground = juce::Colour(0xFF0D1117);
            cs.waveformGrid = juce::Colour(0xFF21262D);
        }
        
        // 波形通道颜色 (8色循环，色盲友好)
        cs.waveformChannelColors[0] = juce::Colour(0xFF0066CC); // 蓝
        cs.waveformChannelColors[1] = juce::Colour(0xFFCC6600); // 橙
        cs.waveformChannelColors[2] = juce::Colour(0xFF009900); // 绿
        cs.waveformChannelColors[3] = juce::Colour(0xFF990099); // 紫
        cs.waveformChannelColors[4] = juce::Colour(0xFF009999); // 青
        cs.waveformChannelColors[5] = juce::Colour(0xFFCC0000); // 红
        cs.waveformChannelColors[6] = juce::Colour(0xFF999900); // 橄榄
        cs.waveformChannelColors[7] = juce::Colour(0xFFFF6699); // 粉
        
        cs.waveformActiveChannel = cs.primary;
        cs.waveformTriggerLine = cs.tertiary;
        cs.waveformMeasurement = cs.secondary;
        
        // 状态指示器颜色 (独立于主题)
        cs.statusSuccess = juce::Colour(0xFF10B981);  // 翠绿
        cs.statusWarning = juce::Colour(0xFFF59E0B);  // 琥珀
        cs.statusError = juce::Colour(0xFFEF4444);    // 红
        cs.statusInfo = juce::Colour(0xFF3B82F6);     // 蓝
        cs.statusRunning = juce::Colour(0xFF06B6D4);  // 青
        cs.statusIdle = juce::Colour(0xFF9CA3AF);     // 灰
        
        return cs;
    }
    
    // 洁净白 - 适合演示和打印
    inline ColorScheme getCleanLab(bool isDark = false) {
        ColorScheme cs = getMedicalBlue(isDark);
        
        if (!isDark) {
            cs.primary = juce::Colour(0xFF10B981);           // 医疗绿
            cs.onPrimary = juce::Colour(0xFFFFFFFF);
            cs.primaryContainer = juce::Colour(0xFFD1FAE5);
            cs.onPrimaryContainer = juce::Colour(0xFF064E3B);
            
            cs.secondary = juce::Colour(0xFF6366F1);         // 靛蓝
            cs.onSecondary = juce::Colour(0xFFFFFFFF);
            cs.secondaryContainer = juce::Colour(0xFFE0E7FF);
            cs.onSecondaryContainer = juce::Colour(0xFF3730A3);
            
            cs.surface = juce::Colour(0xFFFFFFFF);
            cs.surfaceContainer = juce::Colour(0xFFF8FAFC);
            cs.background = juce::Colour(0xFFFFFFFF);
        } else {
            cs.primary = juce::Colour(0xFF34D399);
            cs.onPrimary = juce::Colour(0xFF064E3B);
            cs.primaryContainer = juce::Colour(0xFF059669);
            cs.onPrimaryContainer = juce::Colour(0xFFD1FAE5);
            
            cs.surface = juce::Colour(0xFF18181B);
            cs.background = juce::Colour(0xFF18181B);
        }
        
        return cs;
    }
    
    // -------------------------------------------------------------------------
    // 微信风格 - 仿微信桌面端设计
    // 主色: #07C160 绿 | 背景: #F5F5F5 | 导航: #2B2B2B | 卡片: #FFFFFF
    // -------------------------------------------------------------------------
    inline ColorScheme getWeChatStyle(bool isDark = false) {
        ColorScheme cs;

        if (!isDark) {
            // ── 主色系 ──────────────────────────────────────────────────────
            cs.primary              = juce::Colour(0xFF07C160);  // 微信绿
            cs.onPrimary            = juce::Colour(0xFFFFFFFF);
            cs.primaryContainer     = juce::Colour(0xFFD4F5E3);
            cs.onPrimaryContainer   = juce::Colour(0xFF00401E);

            cs.secondary            = juce::Colour(0xFF1AAD19);  // 深绿（次色）
            cs.onSecondary          = juce::Colour(0xFFFFFFFF);
            cs.secondaryContainer   = juce::Colour(0xFFE6F7ED);
            cs.onSecondaryContainer = juce::Colour(0xFF003010);

            cs.tertiary             = juce::Colour(0xFF576B95);  // 微信蓝灰
            cs.onTertiary           = juce::Colour(0xFFFFFFFF);
            cs.tertiaryContainer    = juce::Colour(0xFFDEE6F6);
            cs.onTertiaryContainer  = juce::Colour(0xFF1A2B4A);

            // ── 错误 ────────────────────────────────────────────────────────
            cs.error                = juce::Colour(0xFFFA5151);
            cs.onError              = juce::Colour(0xFFFFFFFF);
            cs.errorContainer       = juce::Colour(0xFFFFE0E0);
            cs.onErrorContainer     = juce::Colour(0xFF5C0000);

            // ── 表面层次 ────────────────────────────────────────────────────
            cs.surface              = juce::Colour(0xFFFFFFFF);  // 卡片白
            cs.onSurface            = juce::Colour(0xFF191919);  // 主文字
            cs.surfaceVariant       = juce::Colour(0xFFEDEDED);  // 分割线背景
            cs.onSurfaceVariant     = juce::Colour(0xFF818181);  // 次要文字
            cs.surfaceContainer     = juce::Colour(0xFFF5F5F5);  // 页面背景
            cs.surfaceContainerHigh = juce::Colour(0xFFEDEDED);  // 悬停背景
            cs.surfaceContainerHighest = juce::Colour(0xFFE6E6E6);
            cs.surfaceContainerLow  = juce::Colour(0xFFF9F9F9);
            cs.surfaceContainerLowest = juce::Colour(0xFFFFFFFF);

            cs.background           = juce::Colour(0xFFF5F5F5);  // 全局背景
            cs.onBackground         = juce::Colour(0xFF191919);
            cs.outline              = juce::Colour(0xFFE6E6E6);  // 边框线
            cs.outlineVariant       = juce::Colour(0xFFF0F0F0);

            cs.shadow               = juce::Colour(0xFF000000).withAlpha(0.08f);
            cs.scrim                = juce::Colour(0xFF000000).withAlpha(0.50f);

            // ── 波形显示 ─────────────────────────────────────────────────────
            cs.waveformBackground   = juce::Colour(0xFF0D1117);
            cs.waveformGrid         = juce::Colour(0xFF1E2730);

        } else {
            // ── 深色模式 ─────────────────────────────────────────────────────
            cs.primary              = juce::Colour(0xFF07C160);
            cs.onPrimary            = juce::Colour(0xFF003313);
            cs.primaryContainer     = juce::Colour(0xFF005C28);
            cs.onPrimaryContainer   = juce::Colour(0xFF9AE6BC);

            cs.secondary            = juce::Colour(0xFF62D68A);
            cs.onSecondary          = juce::Colour(0xFF00391A);
            cs.secondaryContainer   = juce::Colour(0xFF005226);
            cs.onSecondaryContainer = juce::Colour(0xFF8AF3B0);

            cs.tertiary             = juce::Colour(0xFFA8BFDF);
            cs.onTertiary           = juce::Colour(0xFF1A2E45);
            cs.tertiaryContainer    = juce::Colour(0xFF2D3E59);
            cs.onTertiaryContainer  = juce::Colour(0xFFD4E3F9);

            cs.error                = juce::Colour(0xFFFF6B6B);
            cs.onError              = juce::Colour(0xFF5C0000);
            cs.errorContainer       = juce::Colour(0xFF8B0000);
            cs.onErrorContainer     = juce::Colour(0xFFFFDAD6);

            cs.surface              = juce::Colour(0xFF1E1E1E);
            cs.onSurface            = juce::Colour(0xFFE8E8E8);
            cs.surfaceVariant       = juce::Colour(0xFF2C2C2C);
            cs.onSurfaceVariant     = juce::Colour(0xFFAAAAAA);
            cs.surfaceContainer     = juce::Colour(0xFF1A1A1A);
            cs.surfaceContainerHigh = juce::Colour(0xFF2A2A2A);
            cs.surfaceContainerHighest = juce::Colour(0xFF333333);
            cs.surfaceContainerLow  = juce::Colour(0xFF161616);
            cs.surfaceContainerLowest = juce::Colour(0xFF111111);

            cs.background           = juce::Colour(0xFF121212);
            cs.onBackground         = juce::Colour(0xFFE8E8E8);
            cs.outline              = juce::Colour(0xFF3D3D3D);
            cs.outlineVariant       = juce::Colour(0xFF2A2A2A);

            cs.shadow               = juce::Colour(0xFF000000).withAlpha(0.35f);
            cs.scrim                = juce::Colour(0xFF000000).withAlpha(0.80f);

            cs.waveformBackground   = juce::Colour(0xFF0A0E14);
            cs.waveformGrid         = juce::Colour(0xFF161C24);
        }

        // ── 波形通道色（色盲友好）──────────────────────────────────────────────
        cs.waveformChannelColors[0] = juce::Colour(0xFF07C160);  // 绿
        cs.waveformChannelColors[1] = juce::Colour(0xFF1D9BF0);  // 蓝
        cs.waveformChannelColors[2] = juce::Colour(0xFFF7A600);  // 橙
        cs.waveformChannelColors[3] = juce::Colour(0xFFEB4D3D);  // 红
        cs.waveformChannelColors[4] = juce::Colour(0xFF9B59B6);  // 紫
        cs.waveformChannelColors[5] = juce::Colour(0xFF00BCD4);  // 青
        cs.waveformChannelColors[6] = juce::Colour(0xFF8BC34A);  // 浅绿
        cs.waveformChannelColors[7] = juce::Colour(0xFFFF5722);  // 深橙

        cs.waveformActiveChannel    = cs.primary;
        cs.waveformTriggerLine      = cs.tertiary;
        cs.waveformMeasurement      = cs.secondary;

        // ── 状态色 ───────────────────────────────────────────────────────────
        cs.statusSuccess  = juce::Colour(0xFF07C160);  // 微信绿
        cs.statusWarning  = juce::Colour(0xFFF7A600);  // 琥珀
        cs.statusError    = juce::Colour(0xFFFA5151);  // 微信红
        cs.statusInfo     = juce::Colour(0xFF1D9BF0);  // 信息蓝
        cs.statusRunning  = juce::Colour(0xFF576B95);  // 运行蓝灰
        cs.statusIdle     = juce::Colour(0xFFBBBBBB);  // 空闲灰

        return cs;
    }

    // =========================================================================
    // Google Material 3 - 现代桌面工具风格（Gmail / Drive / Cloud Console 视觉语言）
    // =========================================================================
    inline ColorScheme getGoogleMaterial3(bool isDark = false)
    {
        ColorScheme cs;

        if (!isDark)
        {
            // ── 主色系（Google Blue）────────────────────────────────────────────
            cs.primary              = juce::Colour(0xFF1A73E8);  // Google Blue 500
            cs.onPrimary            = juce::Colour(0xFFFFFFFF);
            cs.primaryContainer     = juce::Colour(0xFFE8F0FE);  // Blue tint
            cs.onPrimaryContainer   = juce::Colour(0xFF0B2E59);

            cs.secondary            = juce::Colour(0xFF185ABC);  // Blue 700 press
            cs.onSecondary          = juce::Colour(0xFFFFFFFF);
            cs.secondaryContainer   = juce::Colour(0xFFD2E3FC);
            cs.onSecondaryContainer = juce::Colour(0xFF041E49);

            cs.tertiary             = juce::Colour(0xFF34A853);  // Google Green
            cs.onTertiary           = juce::Colour(0xFFFFFFFF);
            cs.tertiaryContainer    = juce::Colour(0xFFE6F4EA);
            cs.onTertiaryContainer  = juce::Colour(0xFF0D3B1C);

            cs.error                = juce::Colour(0xFFD93025);  // Google Red 600
            cs.onError              = juce::Colour(0xFFFFFFFF);
            cs.errorContainer       = juce::Colour(0xFFFCE8E6);
            cs.onErrorContainer     = juce::Colour(0xFF660001);

            // ── 表面层次（Google Neutral Palette）────────────────────────────────
            cs.surface                  = juce::Colour(0xFFFFFFFF);  // 纯白卡片
            cs.onSurface                = juce::Colour(0xFF202124);  // Google Grey 900
            cs.surfaceVariant           = juce::Colour(0xFFF1F3F4);
            cs.onSurfaceVariant         = juce::Colour(0xFF5F6368);  // Grey 700
            cs.surfaceContainer         = juce::Colour(0xFFF8F9FA);  // Grey 50
            cs.surfaceContainerHigh     = juce::Colour(0xFFF1F3F4);  // Grey 100
            cs.surfaceContainerHighest  = juce::Colour(0xFFE8EAED);  // Grey 200
            cs.surfaceContainerLow      = juce::Colour(0xFFFCFCFD);
            cs.surfaceContainerLowest   = juce::Colour(0xFFFFFFFF);

            cs.background       = juce::Colour(0xFFF8F9FA);
            cs.onBackground     = juce::Colour(0xFF202124);
            cs.outline          = juce::Colour(0xFFDADCE0);  // Grey 300 (分割/边框)
            cs.outlineVariant   = juce::Colour(0xFFE8EAED);  // Softer

            cs.shadow           = juce::Colour(0xFF000000).withAlpha(0.10f);
            cs.scrim            = juce::Colour(0xFF202124).withAlpha(0.50f);

            // ── 波形显示（保持深色数据区）────────────────────────────────────────
            cs.waveformBackground = juce::Colour(0xFF202124);
            cs.waveformGrid       = juce::Colour(0xFF3C4043);
        }
        else
        {
            // ── 暗色模式（Google Material 3 Dark）────────────────────────────────
            cs.primary              = juce::Colour(0xFF8AB4F8);  // Blue 200
            cs.onPrimary            = juce::Colour(0xFF002E5F);
            cs.primaryContainer     = juce::Colour(0xFF1F4980);
            cs.onPrimaryContainer   = juce::Colour(0xFFD2E3FC);

            cs.secondary            = juce::Colour(0xFFAECBFA);
            cs.onSecondary          = juce::Colour(0xFF0B2F55);
            cs.secondaryContainer   = juce::Colour(0xFF1F4A80);
            cs.onSecondaryContainer = juce::Colour(0xFFE8F0FE);

            cs.tertiary             = juce::Colour(0xFF81C995);
            cs.onTertiary           = juce::Colour(0xFF0F3C21);
            cs.tertiaryContainer    = juce::Colour(0xFF19603A);
            cs.onTertiaryContainer  = juce::Colour(0xFFC8E6CE);

            cs.error                = juce::Colour(0xFFF28B82);
            cs.onError              = juce::Colour(0xFF5C0000);
            cs.errorContainer       = juce::Colour(0xFF8C1D18);
            cs.onErrorContainer     = juce::Colour(0xFFFCE8E6);

            cs.surface                  = juce::Colour(0xFF202124);
            cs.onSurface                = juce::Colour(0xFFE8EAED);
            cs.surfaceVariant           = juce::Colour(0xFF2D2F31);
            cs.onSurfaceVariant         = juce::Colour(0xFF9AA0A6);
            cs.surfaceContainer         = juce::Colour(0xFF1F2023);
            cs.surfaceContainerHigh     = juce::Colour(0xFF2A2B2E);
            cs.surfaceContainerHighest  = juce::Colour(0xFF35363A);
            cs.surfaceContainerLow      = juce::Colour(0xFF1A1B1D);
            cs.surfaceContainerLowest   = juce::Colour(0xFF141518);

            cs.background       = juce::Colour(0xFF1F2023);
            cs.onBackground     = juce::Colour(0xFFE8EAED);
            cs.outline          = juce::Colour(0xFF3C4043);
            cs.outlineVariant   = juce::Colour(0xFF2D2F31);

            cs.shadow           = juce::Colour(0xFF000000).withAlpha(0.40f);
            cs.scrim            = juce::Colour(0xFF000000).withAlpha(0.80f);

            cs.waveformBackground = juce::Colour(0xFF141518);
            cs.waveformGrid       = juce::Colour(0xFF2A2B2E);
        }

        // ── 波形通道色（Google Chart Colors）───────────────────────────────────
        cs.waveformChannelColors[0] = juce::Colour(0xFF1A73E8);  // 蓝
        cs.waveformChannelColors[1] = juce::Colour(0xFF34A853);  // 绿
        cs.waveformChannelColors[2] = juce::Colour(0xFFFBBC04);  // 黄
        cs.waveformChannelColors[3] = juce::Colour(0xFFEA4335);  // 红
        cs.waveformChannelColors[4] = juce::Colour(0xFF9C27B0);  // 紫
        cs.waveformChannelColors[5] = juce::Colour(0xFF00ACC1);  // 青
        cs.waveformChannelColors[6] = juce::Colour(0xFF43A047);  // 深绿
        cs.waveformChannelColors[7] = juce::Colour(0xFFFB8C00);  // 橙

        cs.waveformActiveChannel    = cs.primary;
        cs.waveformTriggerLine      = cs.tertiary;
        cs.waveformMeasurement      = cs.secondary;

        // ── 状态色（Google Semantic Colors）────────────────────────────────────
        cs.statusSuccess  = juce::Colour(0xFF34A853);  // Green 500
        cs.statusWarning  = juce::Colour(0xFFFBBC04);  // Yellow 500
        cs.statusError    = juce::Colour(0xFFEA4335);  // Red 500
        cs.statusInfo     = juce::Colour(0xFF1A73E8);  // Blue 500
        cs.statusRunning  = juce::Colour(0xFF4285F4);  // Blue 400
        cs.statusIdle     = juce::Colour(0xFF9AA0A6);  // Grey 500

        return cs;
    }

    // ClinicalWorkstation - IEC 62366 medical-grade industrial theme
    inline ColorScheme getClinicalWorkstation(bool isDark = false) {
        ColorScheme cs;
        if (!isDark) {
            cs.primary = juce::Colour(0xFF1976D2); cs.onPrimary = juce::Colour(0xFFFFFFFF);
            cs.primaryContainer = juce::Colour(0xFFE3F2FD); cs.onPrimaryContainer = juce::Colour(0xFF0D47A1);
            cs.secondary = juce::Colour(0xFF546E7A); cs.onSecondary = juce::Colour(0xFFFFFFFF);
            cs.secondaryContainer = juce::Colour(0xFFECEFF1); cs.onSecondaryContainer = juce::Colour(0xFF263238);
            cs.tertiary = juce::Colour(0xFF5C6BC0); cs.onTertiary = juce::Colour(0xFFFFFFFF);
            cs.tertiaryContainer = juce::Colour(0xFFE8EAF6); cs.onTertiaryContainer = juce::Colour(0xFF1A237E);
            cs.error = juce::Colour(0xFFC62828); cs.onError = juce::Colour(0xFFFFFFFF);
            cs.errorContainer = juce::Colour(0xFFFFEBEE); cs.onErrorContainer = juce::Colour(0xFFB71C1C);
            cs.surface = juce::Colour(0xFFFFFFFF); cs.onSurface = juce::Colour(0xFF212121);
            cs.surfaceVariant = juce::Colour(0xFFF5F5F5); cs.onSurfaceVariant = juce::Colour(0xFF616161);
            cs.surfaceContainer = juce::Colour(0xFFF0F2F5);
            cs.surfaceContainerHigh = juce::Colour(0xFFE8EAED);
            cs.surfaceContainerHighest = juce::Colour(0xFFDFE1E5);
            cs.surfaceContainerLow = juce::Colour(0xFFF5F7FA);
            cs.surfaceContainerLowest = juce::Colour(0xFFFFFFFF);
            cs.background = juce::Colour(0xFFF0F2F5); cs.onBackground = juce::Colour(0xFF212121);
            cs.outline = juce::Colour(0xFFDADCE0); cs.outlineVariant = juce::Colour(0xFFEEEFF1);
            cs.shadow = juce::Colour(0xFF000000).withAlpha(0.08f);
            cs.scrim = juce::Colour(0xFF212121).withAlpha(0.50f);
            cs.waveformBackground = juce::Colour(0xFF1A1C20); cs.waveformGrid = juce::Colour(0xFF2A2E34);
        } else {
            cs.primary = juce::Colour(0xFF90CAF9); cs.onPrimary = juce::Colour(0xFF0D47A1);
            cs.primaryContainer = juce::Colour(0xFF1565C0); cs.onPrimaryContainer = juce::Colour(0xFFBBDEFB);
            cs.secondary = juce::Colour(0xFF90A4AE); cs.onSecondary = juce::Colour(0xFF263238);
            cs.secondaryContainer = juce::Colour(0xFF37474F); cs.onSecondaryContainer = juce::Colour(0xFFCFD8DC);
            cs.tertiary = juce::Colour(0xFF9FA8DA); cs.onTertiary = juce::Colour(0xFF1A237E);
            cs.tertiaryContainer = juce::Colour(0xFF3949AB); cs.onTertiaryContainer = juce::Colour(0xFFC5CAE9);
            cs.error = juce::Colour(0xFFEF9A9A); cs.onError = juce::Colour(0xFFB71C1C);
            cs.errorContainer = juce::Colour(0xFFC62828); cs.onErrorContainer = juce::Colour(0xFFFFCDD2);
            cs.surface = juce::Colour(0xFF1E2024); cs.onSurface = juce::Colour(0xFFE0E2E6);
            cs.surfaceVariant = juce::Colour(0xFF2A2D32); cs.onSurfaceVariant = juce::Colour(0xFF9EA2A8);
            cs.surfaceContainer = juce::Colour(0xFF1A1C20);
            cs.surfaceContainerHigh = juce::Colour(0xFF252830);
            cs.surfaceContainerHighest = juce::Colour(0xFF303338);
            cs.surfaceContainerLow = juce::Colour(0xFF16181C);
            cs.surfaceContainerLowest = juce::Colour(0xFF111316);
            cs.background = juce::Colour(0xFF1A1C20); cs.onBackground = juce::Colour(0xFFE0E2E6);
            cs.outline = juce::Colour(0xFF3C3F44); cs.outlineVariant = juce::Colour(0xFF2A2D32);
            cs.shadow = juce::Colour(0xFF000000).withAlpha(0.35f);
            cs.scrim = juce::Colour(0xFF000000).withAlpha(0.80f);
            cs.waveformBackground = juce::Colour(0xFF111316); cs.waveformGrid = juce::Colour(0xFF1E2228);
        }
        cs.waveformChannelColors[0] = juce::Colour(0xFF1976D2);
        cs.waveformChannelColors[1] = juce::Colour(0xFFE65100);
        cs.waveformChannelColors[2] = juce::Colour(0xFF2E7D32);
        cs.waveformChannelColors[3] = juce::Colour(0xFF7B1FA2);
        cs.waveformChannelColors[4] = juce::Colour(0xFF00838F);
        cs.waveformChannelColors[5] = juce::Colour(0xFFC62828);
        cs.waveformChannelColors[6] = juce::Colour(0xFF827717);
        cs.waveformChannelColors[7] = juce::Colour(0xFFAD1457);
        cs.waveformActiveChannel = cs.primary;
        cs.waveformTriggerLine = cs.tertiary;
        cs.waveformMeasurement = cs.secondary;
        cs.statusSuccess = juce::Colour(0xFF2E7D32);
        cs.statusWarning = juce::Colour(0xFFED6C02);
        cs.statusError = juce::Colour(0xFFC62828);
        cs.statusInfo = juce::Colour(0xFF1565C0);
        cs.statusRunning = juce::Colour(0xFF0277BD);
        cs.statusIdle = juce::Colour(0xFF9E9E9E);
        return cs;
    }

    // 赛博霓虹 - 科技感展示
    inline ColorScheme getNeonCyber(bool isDark = true) {
        ColorScheme cs;
        
        // 强制暗色
        cs.primary = juce::Colour(0xFF00F5FF);          // 霓虹青
        cs.onPrimary = juce::Colour(0xFF000000);
        cs.primaryContainer = juce::Colour(0xFF00F5FF).withAlpha(0.15f);
        cs.onPrimaryContainer = juce::Colour(0xFF00F5FF);
        
        cs.secondary = juce::Colour(0xFFFF00FF);        // 霓虹洋红
        cs.onSecondary = juce::Colour(0xFF000000);
        cs.secondaryContainer = juce::Colour(0xFFFF00FF).withAlpha(0.15f);
        cs.onSecondaryContainer = juce::Colour(0xFFFF00FF);
        
        cs.tertiary = juce::Colour(0xFFBC13FE);         // 紫罗兰
        cs.onTertiary = juce::Colour(0xFFFFFFFF);
        cs.tertiaryContainer = juce::Colour(0xFFBC13FE).withAlpha(0.15f);
        cs.onTertiaryContainer = juce::Colour(0xFFBC13FE);
        
        cs.error = juce::Colour(0xFFFF0040);
        cs.onError = juce::Colour(0xFFFFFFFF);
        cs.errorContainer = juce::Colour(0xFFFF0040).withAlpha(0.15f);
        cs.onErrorContainer = juce::Colour(0xFFFF0040);
        
        cs.surface = juce::Colour(0xFF050508);
        cs.onSurface = juce::Colour(0xFFE0E0E0);
        cs.surfaceVariant = juce::Colour(0xFF151520);
        cs.onSurfaceVariant = juce::Colour(0xFFA0A0A0);
        cs.surfaceContainer = juce::Colour(0xFF0D0D14);
        cs.surfaceContainerHigh = juce::Colour(0xFF141420);
        cs.surfaceContainerHighest = juce::Colour(0xFF1C1C28);
        cs.surfaceContainerLow = juce::Colour(0xFF08080C);
        cs.surfaceContainerLowest = juce::Colour(0xFF030305);
        
        cs.background = juce::Colour(0xFF050508);
        cs.onBackground = juce::Colour(0xFFE0E0E0);
        cs.outline = juce::Colour(0xFF333340);
        cs.outlineVariant = juce::Colour(0xFF252530);
        
        cs.shadow = juce::Colour(0xFF000000).withAlpha(0.5f);
        cs.scrim = juce::Colour(0xFF000000).withAlpha(0.85f);
        
        cs.waveformBackground = juce::Colour(0xFF030305);
        cs.waveformGrid = juce::Colour(0xFF151520);
        
        // 霓虹通道颜色
        cs.waveformChannelColors[0] = juce::Colour(0xFF00F5FF); // 青
        cs.waveformChannelColors[1] = juce::Colour(0xFFFF00FF); // 洋红
        cs.waveformChannelColors[2] = juce::Colour(0xFF39FF14); // 酸橙绿
        cs.waveformChannelColors[3] = juce::Colour(0xFFFF3131); // 霓虹红
        cs.waveformChannelColors[4] = juce::Colour(0xFFFFD700); // 金
        cs.waveformChannelColors[5] = juce::Colour(0xFFBC13FE); // 紫
        cs.waveformChannelColors[6] = juce::Colour(0xFFFF5F1F); // 橙
        cs.waveformChannelColors[7] = juce::Colour(0xFF00FFFF); // 水鸭色
        
        cs.waveformActiveChannel = cs.primary;
        cs.waveformTriggerLine = cs.secondary;
        cs.waveformMeasurement = juce::Colour(0xFFFFD700);
        
        cs.statusSuccess = juce::Colour(0xFF39FF14);
        cs.statusWarning = juce::Colour(0xFFFFD700);
        cs.statusError = juce::Colour(0xFFFF0040);
        cs.statusInfo = juce::Colour(0xFF00F5FF);
        cs.statusRunning = juce::Colour(0xFFFF00FF);
        cs.statusIdle = juce::Colour(0xFF666666);
        
        return cs;
    }
}

// ============================================================================
// 2. 排版令牌
// ============================================================================

struct Typography {
    // 字体族
    juce::Font displayLarge;      // 大标题
    juce::Font displayMedium;
    juce::Font displaySmall;
    
    juce::Font headlineLarge;     // 标题
    juce::Font headlineMedium;
    juce::Font headlineSmall;
    
    juce::Font titleLarge;        // 小标题
    juce::Font titleMedium;
    juce::Font titleSmall;
    
    juce::Font bodyLarge;         // 正文
    juce::Font bodyMedium;
    juce::Font bodySmall;
    
    juce::Font labelLarge;        // 标签
    juce::Font labelMedium;
    juce::Font labelSmall;
    
    // 等宽字体 (用于数值、时间、波形刻度)
    juce::Font monoLarge;
    juce::Font monoMedium;
    juce::Font monoSmall;
    
    // 返回支持 CJK 字符的最优 UI 字体名（按优先级）
    static juce::String resolveUiFont()
    {
        // 按优先级枚举候选字体
        static const char* const kCandidates[] = {
            "Microsoft YaHei UI",   // Windows 8+ 标准 UI 字体
            "Microsoft YaHei",      // 旧版 Windows
            "PingFang SC",          // macOS / iOS
            "Hiragino Sans GB",     // macOS 旧版
            "Noto Sans CJK SC",     // Linux / Android
            "WenQuanYi Micro Hei",  // Linux
            "SimHei",               // 黑体（最后手段）
            nullptr
        };

        const juce::StringArray installed = juce::Font::findAllTypefaceNames();
        for (int i = 0; kCandidates[i] != nullptr; ++i)
        {
            if (installed.contains(kCandidates[i], false))
                return kCandidates[i];
        }
        // 回退到平台 sans-serif（DirectWrite 会自动 fallback）
        return juce::Font::getDefaultSansSerifFontName();
    }

    // 返回支持等宽显示的最优字体名
    static juce::String resolveMonoFont()
    {
        static const char* const kCandidates[] = {
            "JetBrains Mono",      // 最佳选择
            "Cascadia Code",       // Windows 11
            "Cascadia Mono",       // Windows Terminal
            "Fira Code",           // 开源
            "Consolas",            // Windows 经典
            "SF Mono",             // macOS
            "Menlo",               // macOS 旧版
            nullptr
        };

        const juce::StringArray installed = juce::Font::findAllTypefaceNames();
        for (int i = 0; kCandidates[i] != nullptr; ++i)
        {
            if (installed.contains(kCandidates[i], false))
                return kCandidates[i];
        }
        return "Consolas"; // 回退
    }

    static Typography createDefault(float baseSize = 14.0f) {
        Typography t;
        float s = baseSize / 14.0f; // 缩放系数

        const juce::String uiFont = resolveUiFont();
        const juce::String monoFont = resolveMonoFont();
        
        t.displayLarge  = juce::Font(uiFont, 48.0f * s, juce::Font::plain);
        t.displayMedium = juce::Font(uiFont, 36.0f * s, juce::Font::plain);
        t.displaySmall  = juce::Font(uiFont, 28.0f * s, juce::Font::plain);
        
        t.headlineLarge  = juce::Font(uiFont, 26.0f * s, juce::Font::plain);
        t.headlineMedium = juce::Font(uiFont, 22.0f * s, juce::Font::plain);
        t.headlineSmall  = juce::Font(uiFont, 18.0f * s, juce::Font::bold);
        
        t.titleLarge  = juce::Font(uiFont, 18.0f * s, juce::Font::plain);
        t.titleMedium = juce::Font(uiFont, 14.0f * s, juce::Font::bold);
        t.titleSmall  = juce::Font(uiFont, 13.0f * s, juce::Font::bold);
        
        t.bodyLarge  = juce::Font(uiFont, 14.0f * s, juce::Font::plain);
        t.bodyMedium = juce::Font(uiFont, 13.0f * s, juce::Font::plain);
        t.bodySmall  = juce::Font(uiFont, 12.0f * s, juce::Font::plain);
        
        t.labelLarge  = juce::Font(uiFont, 13.0f * s, juce::Font::bold);
        t.labelMedium = juce::Font(uiFont, 12.0f * s, juce::Font::bold);
        t.labelSmall  = juce::Font(uiFont, 11.0f * s, juce::Font::bold);
        
        t.monoLarge  = juce::Font(monoFont, 22.0f * s, juce::Font::bold);
        t.monoMedium = juce::Font(monoFont, 13.0f * s, juce::Font::plain);
        t.monoSmall  = juce::Font(monoFont, 11.0f * s, juce::Font::plain);
        
        return t;
    }
};

// ============================================================================
// 3. 间距令牌 - 8dp 网格系统
// ============================================================================

namespace spacing {
    inline constexpr int dp0 = 0;
    inline constexpr int dp1 = 4;
    inline constexpr int dp2 = 8;
    inline constexpr int dp3 = 12;
    inline constexpr int dp4 = 16;
    inline constexpr int dp5 = 20;
    inline constexpr int dp6 = 24;
    inline constexpr int dp7 = 28;
    inline constexpr int dp8 = 32;
    inline constexpr int dp9 = 36;
    inline constexpr int dp10 = 40;
    inline constexpr int dp11 = 44;
    inline constexpr int dp12 = 48;
    inline constexpr int dp16 = 64;
    inline constexpr int dp20 = 80;
    inline constexpr int dp24 = 96;
    
    // 常用组合
    inline constexpr int cardPadding = dp3;      // 12px — 紧凑卡片
    inline constexpr int cardGap = dp2;           // 8px  — 卡片间距
    inline constexpr int sectionGap = dp4;        // 16px — 区块间距
    inline constexpr int sidebarWidth = 260;      // 侧边栏宽度
    inline constexpr int topBarHeight = 56;       // 顶栏高度
    inline constexpr int bottomBarHeight = 28;    // 底栏高度
    inline constexpr int panelDivider = 1;        // 分割线粗细
}

// ============================================================================
// 4. 形状令牌 - 圆角系统
// ============================================================================

namespace shapes {
    inline constexpr float cornerNone = 0.0f;
    inline constexpr float cornerExtraSmall = 4.0f;
    inline constexpr float cornerSmall = 8.0f;
    inline constexpr float cornerMedium = 12.0f;
    inline constexpr float cornerLarge = 16.0f;
    inline constexpr float cornerExtraLarge = 28.0f;
    inline constexpr float cornerFull = 1000.0f; // 胶囊形状
    
    // 常用组件形状（工业级精密圆角）
    inline constexpr float buttonRadius = 3.0f;       // 按钮：精密小圆角
    inline constexpr float cardRadius = cornerSmall;   // 卡片：8px
    inline constexpr float cardRadiusCompact = cornerExtraSmall; // 紧凑卡片：4px
    inline constexpr float chipRadius = cornerExtraSmall;  // Chip：4px
    inline constexpr float inputRadius = 3.0f;         // 输入框：3px
    inline constexpr float dialogRadius = cornerLarge;  // 对话框：16px
    inline constexpr float tooltipRadius = 4.0f;       // 提示框：4px
}

// ============================================================================
// 5. 动画令牌
// ============================================================================

namespace motion {
    // 缓动曲线
    struct Easing {
        float x1, y1, x2, y2; // 贝塞尔控制点
    };
    
    inline constexpr Easing standard = {0.4f, 0.0f, 0.2f, 1.0f};      // 标准
    inline constexpr Easing decelerate = {0.0f, 0.0f, 0.2f, 1.0f};  // 减速
    inline constexpr Easing accelerate = {0.4f, 0.0f, 1.0f, 1.0f};  // 加速
    inline constexpr Easing sharp = {0.4f, 0.0f, 0.6f, 1.0f};       // 锐利
    
    // 持续时间 (毫秒)
    inline constexpr int durationInstant = 0;
    inline constexpr int durationFast = 150;
    inline constexpr int durationNormal = 300;
    inline constexpr int durationSlow = 500;
    inline constexpr int durationEmphasis = 700;
    
    // 页面切换动画
    inline constexpr int pageTransitionDuration = 300;
    inline constexpr int panelSlideDuration = 250;
    inline constexpr int rippleDuration = 400;
    inline constexpr int snackbarDuration = 400;
    inline constexpr int dialogDuration = 350;
}

// ============================================================================
// 6. 阴影令牌 - Elevation
// ============================================================================

struct Elevation {
    int level;           // 0-5
    float blurRadius;
    juce::Point<float> offset;
    float spread;
    float opacity;
    
    static Elevation get(int level) {
        switch (level) {
            case 0: return {0, 0.0f, {0, 0}, 0.0f, 0.0f};
            case 1: return {1, 2.0f, {0, 1}, 0.0f, 0.06f};   // 更轻的阴影
            case 2: return {2, 4.0f, {0, 1}, 0.0f, 0.08f};
            case 3: return {3, 8.0f, {0, 2}, 0.0f, 0.10f};
            case 4: return {4, 16.0f, {0, 4}, 0.0f, 0.12f};
            case 5: return {5, 32.0f, {0, 8}, 0.0f, 0.15f};
            default: return get(0);
        }
    }
};

// ============================================================================
// 7. 组件尺寸令牌
// ============================================================================

namespace component {
    // 按钮
    inline constexpr int buttonHeightSmall = 32;
    inline constexpr int buttonHeightMedium = 40;
    inline constexpr int buttonHeightLarge = 48;
    inline constexpr int buttonMinWidth = 64;
    
    // 输入框
    inline constexpr int inputHeightSmall = 32;
    inline constexpr int inputHeightMedium = 40;
    inline constexpr int inputHeightLarge = 56;
    
    // 开关
    inline constexpr int switchWidth = 52;
    inline constexpr int switchHeight = 32;
    
    // Chip
    inline constexpr int chipHeight = 32;
    
    // 滑块
    inline constexpr int sliderTrackHeight = 4;
    inline constexpr int sliderThumbSize = 20;
    
    // 进度条
    inline constexpr int progressHeight = 4;
    
    // 列表项
    inline constexpr int listItemHeightOneLine = 48;
    inline constexpr int listItemHeightTwoLine = 64;
    inline constexpr int listItemHeightThreeLine = 88;
}

// ============================================================================
// 8. 密度模式
// ============================================================================

enum class Density {
    Compact,      // 紧凑 - 更多信息
    Comfortable,  // 舒适 - 默认
    Spacious      // 宽敞 - 更易读
};

struct DensityConfig {
    float scale;              // 间距缩放
    int rowHeight;            // 列表行高
    int sectionGap;           // 区块间距
    int cardPadding;          // 卡片内边距
    float fontScale;          // 字体缩放
    
    static DensityConfig get(Density d) {
        switch (d) {
            case Density::Compact:
                return {0.85f, 32, 12, 10, 0.92f};
            case Density::Comfortable:
                return {1.0f, 40, 16, 12, 1.0f};
            case Density::Spacious:
                return {1.15f, 48, 24, 16, 1.05f};
        }
        return get(Density::Comfortable);
    }
};

} // namespace nerou::ui::tokens

/**
 * 设计令牌使用示例:
 * 
 * auto& tokens = DesignTokenStore::getInstance();
 * auto color = tokens.colors.primary;
 * auto font = tokens.typography.titleMedium;
 * int padding = tokens.spacing.dp4;
 */
