#pragma once

#include <JuceHeader.h>

/**
 * 跨平台 UI 用中文字体：Windows 优先从 Fonts 目录加载微软雅黑，避免仅依赖「已安装字体名」在部分环境下匹配失败导致 □ 与拉丁乱码。
 */
namespace NerouCjkFont {

/** JUCE 7 Typeface 无 createSystemTypefaceFor(File)，需读入内存后交给 createSystemTypefaceFor(bytes, size)。 */
inline juce::Typeface::Ptr loadTypefaceFromFontFile(const juce::File& f)
{
    juce::MemoryBlock mb;
    if (!f.loadFileAsData(mb) || mb.getSize() == 0)
        return {};
    return juce::Typeface::createSystemTypefaceFor(mb.getData(), mb.getSize());
}

inline juce::Font createUiFont(float heightPx, int styleFlags = juce::Font::plain)
{
#if JUCE_WINDOWS
    {
        juce::File boldFace("C:/Windows/Fonts/msyhbd.ttc");
        if ((styleFlags & juce::Font::bold) != 0 && boldFace.existsAsFile())
            if (auto face = loadTypefaceFromFontFile(boldFace))
                return juce::Font(face).withHeight(heightPx).withStyle(styleFlags);
    }
    for (const char* rel : {"C:/Windows/Fonts/msyh.ttc", "C:/Windows/Fonts/msyh.ttf",
                           "C:/Windows/Fonts/simhei.ttf", "C:/Windows/Fonts/simsun.ttc"})
    {
        juce::File f(rel);
        if (f.existsAsFile())
            if (auto face = loadTypefaceFromFontFile(f))
                return juce::Font(face).withHeight(heightPx).withStyle(styleFlags);
    }
#elif JUCE_MAC
    for (const char* rel : {"/System/Library/Fonts/PingFang.ttc",
                            "/System/Library/Fonts/STHeiti Light.ttc",
                            "/Library/Fonts/Arial Unicode.ttf"})
    {
        juce::File f(rel);
        if (f.existsAsFile())
            if (auto face = loadTypefaceFromFontFile(f))
                return juce::Font(face).withHeight(heightPx).withStyle(styleFlags);
    }
#else
    for (const char* rel :
         {"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
          "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
          "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc"})
    {
        juce::File f(rel);
        if (f.existsAsFile())
            if (auto face = loadTypefaceFromFontFile(f))
                return juce::Font(face).withHeight(heightPx).withStyle(styleFlags);
    }
#endif

    const auto   allFaces = juce::Font::findAllTypefaceNames();
    const char*  cjk[]    = {"Microsoft YaHei UI", "Microsoft YaHei", "SimHei",
                             "PingFang SC", "WenQuanYi Micro Hei", "Noto Sans CJK SC", nullptr};
    for (int i = 0; cjk[i] != nullptr; ++i)
    {
        juce::String name(cjk[i]);
        if (allFaces.contains(name))
            return {name, heightPx, styleFlags};
    }

    return {heightPx, styleFlags};
}

} // namespace NerouCjkFont
