#pragma once

/*
 * Utf8Literals.h — 中文字符串编码安全层
 *
 * 问题根源：Windows 默认 ANSI 代码页（GBK/CP936）导致 char 字面量乱码。
 * 解决策略：
 *   1. 所有中文字符串使用 L"..." 宽字符字面量 → juce::String(wchar_t*)
 *      wchar_t 在 MSVC/Windows 上始终是 UTF-16，与源文件编码无关。
 *   2. NR_STR(x) 宏：对外统一接口，内部用 L 前缀宽字面量。
 *   3. NR_U8(x) 宏：C++17 u8"" 字面量 → JUCE CharPointer_UTF8 转换（备选）。
 *
 * 使用方式：
 *   label.setText(NR_STR("采集中心"), juce::dontSendNotification);
 *   button.setButtonText(NR_STR("开始采集"));
 *   button.setTooltip(NR_STR("点击开始实时脑电采集"));
 */

#include <JuceHeader.h>
#include <vector>

#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// ── MSVC 编码警告抑制（需配合 /source-charset:utf-8 编译选项）────────────────
#if defined(_MSC_VER)
#pragma warning(disable: 4819)  // 源文件包含无法用当前代码页表示的字符
#endif

namespace nerou::core {

// ── 乱码检测（已禁用）────────────────────────────────────────────────────────
// 注意：原始实现使用的模式字符（证、台、题等）在正常中文UI文案中极为常见，
// 导致大量误报（如"验证"、"数据"、"问题"等被错误修复）。
// 由于所有中文文本现已统一使用 NR_STR / L"\uXXXX" 安全编码，
// 不再需要运行时乱码检测。
inline bool looksMojibake(const juce::String& /*s*/)
{
    return false;
}

// ── Windows GBK→UTF-8 乱码修复 ──────────────────────────────────────────────
#if JUCE_WINDOWS
inline juce::String tryRepairMojibake(const juce::String& s)
{
    if (!looksMojibake(s))
        return s;

    const std::wstring ws = s.toWideCharPointer();
    if (ws.empty())
        return s;

    // 将当前 UTF-16 重新解释为 GBK 多字节序列
    const int gbkLen = WideCharToMultiByte(936, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (gbkLen <= 1)
        return s;

    std::vector<char> gbk((size_t)gbkLen);
    if (WideCharToMultiByte(936, 0, ws.c_str(), -1, gbk.data(), gbkLen, nullptr, nullptr) <= 0)
        return s;

    // 将 GBK 字节按 UTF-8 重新解码
    const int utf16Len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, gbk.data(), -1, nullptr, 0);
    if (utf16Len <= 1)
        return s;

    std::vector<wchar_t> repaired((size_t)utf16Len);
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, gbk.data(), -1, repaired.data(), utf16Len) <= 0)
        return s;

    juce::String fixed(repaired.data());
    if (fixed.isEmpty() || fixed.containsChar((juce::juce_wchar)0xfffd))
        return s;

    return fixed;
}
#else
inline juce::String tryRepairMojibake(const juce::String& s) { return s; }
#endif

// ── 宽字符字面量 → juce::String（核心转换，与文件编码无关）──────────────────
inline juce::String makeLocalizedString(const wchar_t* literal)
{
    // wchar_t* 在 Windows 是 UTF-16，在 macOS/Linux 是 UTF-32
    // juce::String(wchar_t*) 正确处理两种情况
    return tryRepairMojibake(juce::String(literal));
}

// ── u8 字面量备选接口（C++17，文件必须以 UTF-8 保存）──────────────────────
inline juce::String makeUtf8String(const char* utf8bytes)
{
    return juce::String(juce::CharPointer_UTF8(utf8bytes));
}

} // namespace nerou::core

// ────────────────────────────────────────────────────────────────────────────
// 公共宏接口
// ────────────────────────────────────────────────────────────────────────────

/**
 * NR_STR("中文文字") — 生成编码安全的 juce::String
 *
 * 原理：L"..." 宽字符字面量由编译器直接按 Unicode 码位存储，
 * 与源文件编码（UTF-8/GBK/...）完全无关，是最可靠的跨平台方案。
 *
 * 使用示例：
 *   label.setText(NR_STR("采集中心"), juce::dontSendNotification);
 *   button.setTooltip(NR_STR("点击开始脑电实时采集"));
 */
#define NR_STR(x) nerou::core::makeLocalizedString(L##x)

/**
 * NR_U8("中文") — 通过 u8 字面量创建 juce::String（要求文件以 UTF-8 保存）
 * 作为 NR_STR 的备选，适合 macOS/Linux 系统。
 */
#define NR_U8(x)  nerou::core::makeUtf8String(u8##x)
