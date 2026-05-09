#pragma once

#include <JuceHeader.h>

namespace nerou::ui::google {

/**
 * Google Material Design 3 令牌
 * 使用在 Google Workspace / Drive / Cloud Console 中常见的品牌配色。
 * 仅用于 Google Style 工作台壳层，不替换全局 DesignTokenStore。
 */
struct GoogleTheme
{
    // Production desktop palette: light, clinical, data-dense.
    static constexpr juce::uint32 kBlue500    = 0xFF2563EB;
    static constexpr juce::uint32 kBlue600    = 0xFF1D4ED8;
    static constexpr juce::uint32 kBlue50     = 0xFFEFF6FF;
    static constexpr juce::uint32 kGreen600   = 0xFF16A34A;
    static constexpr juce::uint32 kYellow700  = 0xFFD97706;
    static constexpr juce::uint32 kRed600     = 0xFFDC2626;
    static constexpr juce::uint32 kPurple600  = 0xFF0F766E;

    // Neutral palette (Material 3 inspired production light mode)
    static constexpr juce::uint32 kSurface       = 0xFFFFFFFF;
    static constexpr juce::uint32 kSurfaceDim    = 0xFFF7F9FB;
    static constexpr juce::uint32 kSurfaceHigh   = 0xFFEFF4F8;
    static constexpr juce::uint32 kOutline       = 0xFFB8C4D0;
    static constexpr juce::uint32 kOutlineSoft   = 0xFFD8E0E8;
    static constexpr juce::uint32 kOnSurface     = 0xFF0F172A;
    static constexpr juce::uint32 kOnSurfaceMuted= 0xFF475569;
    static constexpr juce::uint32 kOnSurfaceHint = 0xFF64748B;

    // Shadows (Material 3 elevation 2/3)
    static constexpr juce::uint32 kShadow        = 0x1F202124; // 12% black

    // Rounded corners (MD3: 4/8/12/16/28)
    static constexpr float cornerXS = 4.0f;
    static constexpr float cornerS  = 8.0f;
    static constexpr float cornerM  = 12.0f;
    static constexpr float cornerL  = 16.0f;
    static constexpr float cornerXL = 28.0f;

    static juce::Colour blue()       { return juce::Colour(kBlue500); }
    static juce::Colour bluePress()  { return juce::Colour(kBlue600); }
    static juce::Colour blueTint()   { return juce::Colour(kBlue50); }
    static juce::Colour surface()    { return juce::Colour(kSurface); }
    static juce::Colour surfaceDim() { return juce::Colour(kSurfaceDim); }
    static juce::Colour surfaceHigh(){ return juce::Colour(kSurfaceHigh); }
    static juce::Colour outline()    { return juce::Colour(kOutline); }
    static juce::Colour outlineSoft(){ return juce::Colour(kOutlineSoft); }
    static juce::Colour onSurface()  { return juce::Colour(kOnSurface); }
    static juce::Colour onMuted()    { return juce::Colour(kOnSurfaceMuted); }
    static juce::Colour onHint()     { return juce::Colour(kOnSurfaceHint); }
    static juce::Colour success()    { return juce::Colour(kGreen600); }
    static juce::Colour warning()    { return juce::Colour(kYellow700); }
    static juce::Colour danger()     { return juce::Colour(kRed600); }
    static juce::Colour info()       { return juce::Colour(kPurple600); }

    static juce::Font fontDisplay(float size = 22.0f)
    {
        juce::Font f(juce::Font::getDefaultSansSerifFontName(), size, juce::Font::plain);
        f.setExtraKerningFactor(-0.01f);
        return f;
    }
    static juce::Font fontTitle(float size = 16.0f)
    {
        return juce::Font(juce::Font::getDefaultSansSerifFontName(), size, juce::Font::bold);
    }
    static juce::Font fontLabel(float size = 13.0f)
    {
        return juce::Font(juce::Font::getDefaultSansSerifFontName(), size, juce::Font::plain);
    }
    static juce::Font fontCaption(float size = 11.0f)
    {
        return juce::Font(juce::Font::getDefaultSansSerifFontName(), size, juce::Font::plain);
    }

    /** 绘制 Material 3 elevation 2（软阴影 + 圆角底） */
    static void drawSurface(juce::Graphics& g,
                            juce::Rectangle<float> bounds,
                            float radius = cornerL,
                            juce::Colour fill = surface(),
                            bool withShadow = true)
    {
        if (withShadow)
        {
            juce::DropShadow ds(juce::Colour(kShadow), 18, { 0, 4 });
            juce::Path p;
            p.addRoundedRectangle(bounds, radius);
            ds.drawForPath(g, p);
        }
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, radius);
    }

    static void drawOutline(juce::Graphics& g,
                            juce::Rectangle<float> bounds,
                            float radius = cornerL,
                            juce::Colour color = outlineSoft(),
                            float thickness = 1.0f)
    {
        g.setColour(color);
        g.drawRoundedRectangle(bounds, radius, thickness);
    }
};

} // namespace nerou::ui::google
