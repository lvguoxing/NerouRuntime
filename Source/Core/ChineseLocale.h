#pragma once

/*
 * ChineseLocale.h — 中文本地化工具库
 *
 * 提供：
 *   1. 中文日期时间格式化（YYYY年MM月DD日 HH:MM:SS）
 *   2. 微信风格相对时间（刚刚 / N分钟前 / 昨天 / 本周X）
 *   3. 中文数字/单位格式化（1,234 → 1.2千; 1,234,567 → 123.5万）
 *   4. JUCE TextEditor 中文输入法（IME）友好设置
 *   5. 中文界面常用字符串常量
 *
 * 所有字符串均使用 L"..." 宽字符字面量，与源文件编码无关。
 */

#include <JuceHeader.h>
#include "Utf8Literals.h"

namespace nerou::locale {

// ────────────────────────────────────────────────────────────────────────────
// 1. 日期时间格式化
// ────────────────────────────────────────────────────────────────────────────

/** 返回完整中文日期时间：2024年04月23日 14:30:05 */
inline juce::String formatChineseDatetime(juce::Time t = juce::Time::getCurrentTime())
{
    // 使用 \u 转义保证编码安全
    juce::String year  = juce::String(t.getYear());
    juce::String month = juce::String(t.getMonth() + 1).paddedLeft(L'0', 2);
    juce::String day   = juce::String(t.getDayOfMonth()).paddedLeft(L'0', 2);
    juce::String hour  = juce::String(t.getHours()).paddedLeft(L'0', 2);
    juce::String min   = juce::String(t.getMinutes()).paddedLeft(L'0', 2);
    juce::String sec   = juce::String(t.getSeconds()).paddedLeft(L'0', 2);

    return year  + juce::String(L"\u5e74") +   // 年
           month + juce::String(L"\u6708") +   // 月
           day   + juce::String(L"\u65e5 ") +  // 日
           hour  + ":" + min + ":" + sec;
}

/** 返回中文日期：2024年04月23日 */
inline juce::String formatChineseDate(juce::Time t = juce::Time::getCurrentTime())
{
    return juce::String(t.getYear())
         + juce::String(L"\u5e74")
         + juce::String(t.getMonth() + 1).paddedLeft(L'0', 2)
         + juce::String(L"\u6708")
         + juce::String(t.getDayOfMonth()).paddedLeft(L'0', 2)
         + juce::String(L"\u65e5");
}

/** 返回中文时间：14:30:05 */
inline juce::String formatChineseTime(juce::Time t = juce::Time::getCurrentTime())
{
    return juce::String(t.getHours()).paddedLeft(L'0', 2) + ":" +
           juce::String(t.getMinutes()).paddedLeft(L'0', 2) + ":" +
           juce::String(t.getSeconds()).paddedLeft(L'0', 2);
}

// ────────────────────────────────────────────────────────────────────────────
// 2. 微信风格相对时间（类似微信消息时间戳）
// ────────────────────────────────────────────────────────────────────────────

/**
 * 微信风格相对时间：
 *   < 1分钟  → "刚刚"
 *   < 1小时  → "N分钟前"
 *   < 24小时 → "今天 HH:MM"
 *   < 48小时 → "昨天 HH:MM"
 *   < 7天   → "周X HH:MM"
 *   否则    → "YYYY年MM月DD日"
 */
inline juce::String formatRelativeTime(juce::Time t)
{
    auto now = juce::Time::getCurrentTime();
    auto diffSec = static_cast<int64_t>((now - t).inSeconds());

    if (diffSec < 60)
        return juce::String(L"\u521a\u521a");  // 刚刚

    if (diffSec < 3600)
    {
        int mins = static_cast<int>(diffSec / 60);
        return juce::String(mins) + juce::String(L"\u5206\u949f\u524d");  // N分钟前
    }

    const juce::String timeStr = juce::String(t.getHours()).paddedLeft(L'0', 2) + ":" +
                                 juce::String(t.getMinutes()).paddedLeft(L'0', 2);

    if (diffSec < 86400 && now.getDayOfMonth() == t.getDayOfMonth())
        return juce::String(L"\u4eca\u5929 ") + timeStr;  // 今天 HH:MM

    if (diffSec < 172800)
        return juce::String(L"\u6628\u5929 ") + timeStr;  // 昨天 HH:MM

    if (diffSec < 604800)
    {
        // 周X
        static const wchar_t* weekdays[] = {
            L"\u5468\u65e5", L"\u5468\u4e00", L"\u5468\u4e8c",
            L"\u5468\u4e09", L"\u5468\u56db", L"\u5468\u4e94", L"\u5468\u516d"
        };
        int wday = t.getDayOfWeek(); // 0=Sunday
        return juce::String(weekdays[wday]) + L" " + timeStr;
    }

    return formatChineseDate(t);
}

// ────────────────────────────────────────────────────────────────────────────
// 3. 中文数字格式化
// ────────────────────────────────────────────────────────────────────────────

/** 大数简化：1234 → "1234"，12345 → "1.2万"，1234567 → "123.5万" */
inline juce::String formatChineseNumber(int64_t n)
{
    if (n < 10000)
        return juce::String(n);

    if (n < 100000000LL)
    {
        double wan = static_cast<double>(n) / 10000.0;
        juce::String s = juce::String(wan, 1);
        return s + juce::String(L"\u4e07");  // 万
    }

    double yi = static_cast<double>(n) / 100000000.0;
    juce::String s = juce::String(yi, 1);
    return s + juce::String(L"\u4ebf");  // 亿
}

/** 文件大小中文格式：1024 → "1.0 KB"，1048576 → "1.0 MB" */
inline juce::String formatFileSize(int64_t bytes)
{
    if (bytes < 1024)
        return juce::String(bytes) + " B";
    if (bytes < 1024 * 1024)
        return juce::String(static_cast<double>(bytes) / 1024.0, 1) + " KB";
    if (bytes < 1024LL * 1024 * 1024)
        return juce::String(static_cast<double>(bytes) / (1024.0 * 1024.0), 1) + " MB";
    return juce::String(static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0), 1) + " GB";
}

/** 时长格式化：125 秒 → "2分05秒"，3661 → "1小时0分01秒" */
inline juce::String formatDurationChinese(int totalSeconds)
{
    if (totalSeconds < 0) totalSeconds = 0;
    int h = totalSeconds / 3600;
    int m = (totalSeconds % 3600) / 60;
    int s = totalSeconds % 60;

    juce::String result;
    if (h > 0)
        result += juce::String(h) + juce::String(L"\u5c0f\u65f6");  // 小时
    if (h > 0 || m > 0)
        result += juce::String(m) + juce::String(L"\u5206");         // 分
    result += juce::String(s).paddedLeft(L'0', 2) + juce::String(L"\u79d2");  // 秒
    return result;
}

// ────────────────────────────────────────────────────────────────────────────
// 4. JUCE TextEditor — 中文输入法（IME）友好设置
// ────────────────────────────────────────────────────────────────────────────

/**
 * 为 TextEditor 应用微信风格中文输入友好设置：
 *  - 允许键盘焦点
 *  - 设置中文占位提示文字（若提供）
 *  - Enter 键行为由外部回调决定
 *
 * 用法：
 *   applyChineseInputStyle(myEditor, NR_STR("请输入受试者姓名"));
 */
inline void applyChineseInputStyle(juce::TextEditor& editor,
                                   const juce::String& placeholderChinese = {})
{
    editor.setWantsKeyboardFocus(true);
    editor.setEscapeAndReturnKeysConsumed(false);  // 允许 Enter 键传递给父组件

    if (placeholderChinese.isNotEmpty())
    {
        // JUCE 8 TextEditor::setTextToShowWhenEmpty 支持提示文字
        juce::Colour hintColour = juce::Colour(0xFFAAAAAA);
        editor.setTextToShowWhenEmpty(placeholderChinese, hintColour);
    }
}

// ────────────────────────────────────────────────────────────────────────────
// 5. 中文界面常用字符串（NR_STR 等价的编译期常量辅助）
// ────────────────────────────────────────────────────────────────────────────

namespace strings {
    // 通用操作
    inline juce::String confirm()     { return juce::String(L"\u786e\u5b9a"); }     // 确定
    inline juce::String cancel()      { return juce::String(L"\u53d6\u6d88"); }     // 取消
    inline juce::String save()        { return juce::String(L"\u4fdd\u5b58"); }     // 保存
    inline juce::String close()       { return juce::String(L"\u5173\u95ed"); }     // 关闭
    inline juce::String browse()      { return juce::String(L"\u6d4f\u89c8"); }     // 浏览
    inline juce::String refresh()     { return juce::String(L"\u5237\u65b0"); }     // 刷新
    inline juce::String start()       { return juce::String(L"\u5f00\u59cb"); }     // 开始
    inline juce::String stop()        { return juce::String(L"\u505c\u6b62"); }     // 停止
    inline juce::String pause()       { return juce::String(L"\u6682\u505c"); }     // 暂停
    inline juce::String resume()      { return juce::String(L"\u7ee7\u7eed"); }     // 继续
    inline juce::String delete_()     { return juce::String(L"\u5220\u9664"); }     // 删除
    inline juce::String edit()        { return juce::String(L"\u7f16\u8f91"); }     // 编辑
    inline juce::String copy()        { return juce::String(L"\u590d\u5236"); }     // 复制
    inline juce::String paste()       { return juce::String(L"\u7c98\u8d34"); }     // 粘贴

    // 状态
    inline juce::String idle()        { return juce::String(L"\u5c31\u7eea"); }     // 就绪
    inline juce::String running()     { return juce::String(L"\u8fd0\u884c\u4e2d"); } // 运行中
    inline juce::String completed()   { return juce::String(L"\u5df2\u5b8c\u6210"); } // 已完成
    inline juce::String failed()      { return juce::String(L"\u5931\u8d25"); }      // 失败
    inline juce::String connecting()  { return juce::String(L"\u8fde\u63a5\u4e2d"); } // 连接中
    inline juce::String connected()   { return juce::String(L"\u5df2\u8fde\u63a5"); } // 已连接
    inline juce::String disconnected(){ return juce::String(L"\u5df2\u65ad\u5f00"); } // 已断开

    // 提示
    inline juce::String loading()     { return juce::String(L"\u52a0\u8f7d\u4e2d\u2026"); } // 加载中…
    inline juce::String processing()  { return juce::String(L"\u5904\u7406\u4e2d\u2026"); } // 处理中…
    inline juce::String noData()      { return juce::String(L"\u6682\u65e0\u6570\u636e"); } // 暂无数据
    inline juce::String selectFile()  { return juce::String(L"\u8bf7\u9009\u62e9\u6587\u4ef6"); } // 请选择文件
    inline juce::String selectDir()   { return juce::String(L"\u8bf7\u9009\u62e9\u76ee\u5f55"); } // 请选择目录
}

// ────────────────────────────────────────────────────────────────────────────
// 6. 微信风格快捷键提示字符串
// ────────────────────────────────────────────────────────────────────────────

/** 生成带快捷键的 Tooltip，如：formatTooltip("开始采集", "F5") → "开始采集 (F5)" */
inline juce::String formatTooltip(const juce::String& action, const juce::String& shortcut = {})
{
    if (shortcut.isEmpty()) return action;
    return action + " (" + shortcut + ")";
}

} // namespace nerou::locale
