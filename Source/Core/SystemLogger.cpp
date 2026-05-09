#include "SystemLogger.h"
#include "ProjectPaths.h"
#include "../Application/NotificationCenter.h"

namespace nerou::core {

namespace {

juce::String pad2(int n)
{
    return (n < 10 ? juce::String("0") + juce::String(n) : juce::String(n));
}

juce::String pad3(int n)
{
    if (n < 10)   return juce::String("00") + juce::String(n);
    if (n < 100)  return juce::String("0")  + juce::String(n);
    return juce::String(n);
}

juce::String formatTimestamp(juce::int64 millis, bool withDate)
{
    juce::Time t(millis);
    juce::String s;
    if (withDate)
    {
        s << t.getYear() << "-" << pad2(t.getMonth() + 1) << "-" << pad2(t.getDayOfMonth())
          << " ";
    }
    s << pad2(t.getHours()) << ":" << pad2(t.getMinutes()) << ":" << pad2(t.getSeconds())
      << "." << pad3(t.getMilliseconds());
    return s;
}

juce::String todayStamp()
{
    auto t = juce::Time::getCurrentTime();
    juce::String s;
    s << t.getYear() << pad2(t.getMonth() + 1) << pad2(t.getDayOfMonth());
    return s;
}

nerou::app::NotificationCenter::Level toNotifyLevel(SystemLogger::Level l)
{
    using NL = nerou::app::NotificationCenter::Level;
    switch (l)
    {
        case SystemLogger::Level::Warning: return NL::Warning;
        case SystemLogger::Level::Error:   return NL::Error;
        default:                           return NL::Info;
    }
}

} // namespace

// ── Entry helpers ───────────────────────────────────────────────────────────
juce::String SystemLogger::Entry::levelTag() const
{
    return SystemLogger::levelToString(level);
}

juce::String SystemLogger::Entry::formatted() const
{
    juce::String line;
    line << "[" << formatTimestamp(timestampMs, true) << "] "
         << "[" << levelTag() << "] ";
    if (category.isNotEmpty())
        line << "[" << category << "] ";
    line << message;
    return line;
}

// ── 单例 ────────────────────────────────────────────────────────────────────
SystemLogger& SystemLogger::getInstance()
{
    static SystemLogger instance;
    return instance;
}

SystemLogger::SystemLogger()
{
    logDirectory     = resolveLogDirectory();
    currentDateStamp = todayStamp();
}

// ── 核心发布路径 ────────────────────────────────────────────────────────────
void SystemLogger::log(Level level, const juce::String& category, const juce::String& message)
{
    if (paused.load())
        return;

    Entry e;
    e.timestampMs = juce::Time::currentTimeMillis();
    e.level       = level;
    e.category    = category.isEmpty() ? juce::String("General") : category;
    e.message     = message;

    pushEntry(e);
    writeToDisk(e);

    // 桥接到 NotificationCenter（仅 Warning / Error）
    if (bridgeNotifications.load()
        && (level == Level::Warning || level == Level::Error)
        && !reentryGuard.exchange(true))
    {
        nerou::app::Notify().post(toNotifyLevel(level),
                                  juce::String("[") + e.category + "] " + e.message);
        reentryGuard.store(false);
    }

    // 通知 UI 刷新（总是在任意线程调用，JUCE 的 ChangeBroadcaster 自带 async 派发）
    const_cast<SystemLogger*>(this)->sendChangeMessage();
}

void SystemLogger::logMessage(const juce::String& message)
{
    // 兼容 [分类] 前缀（OnnxRunner / DatasetPreparationService 已有 [推理引擎] 等前缀）
    juce::String cat = "System";
    juce::String msg = message;
    if (msg.startsWithChar('['))
    {
        const int close = msg.indexOfChar(']');
        if (close > 1 && close < 48)
        {
            cat = msg.substring(1, close).trim();
            msg = msg.substring(close + 1).trimStart();
        }
    }
    log(Level::Info, cat, msg);
}

void SystemLogger::pushEntry(Entry entry)
{
    juce::ScopedLock lock(bufferLock);
    buffer.push_back(std::move(entry));
    while ((int)buffer.size() > kMaxInMemory)
        buffer.pop_front();
    entryCount.store((int)buffer.size());
}

// ── 磁盘写入 ────────────────────────────────────────────────────────────────
void SystemLogger::writeToDisk(const Entry& entry)
{
    juce::ScopedLock lock(fileLock);

    const auto stamp = todayStamp();
    const bool dayRolled = (stamp != currentDateStamp) || !logDirectory.isDirectory();
    if (dayRolled)
    {
        currentDateStamp = stamp;
        logDirectory     = resolveLogDirectory();
        // 关闭旧流以便切换到新一天的文件
        diskStream.reset();
    }

    // 首次或滚日后打开持久化追加流，避免每条日志都 open/close
    if (diskStream == nullptr)
    {
        auto file = logDirectory.getChildFile("nerou_" + currentDateStamp + ".log");
        file.getParentDirectory().createDirectory();
        diskStream = std::make_unique<juce::FileOutputStream>(file);
        if (!diskStream->openedOk())
        {
            diskStream.reset();
            return;
        }
    }

    const auto line = entry.formatted() + juce::newLine;
    diskStream->writeText(line, false, false, nullptr);

    // 分批 flush：累计 ~4 KB 或 WARN/ERROR 级别立即落盘，降低 IO 频率
    pendingFlushBytes += line.getNumBytesAsUTF8();
    if (pendingFlushBytes >= 4096
        || entry.level == Level::Warning
        || entry.level == Level::Error)
    {
        diskStream->flush();
        pendingFlushBytes = 0;
    }
}

juce::File SystemLogger::resolveLogDirectory() const
{
    auto dir = ProjectPaths::resolveProjectRootDirectory().getChildFile("logs");
    if (!dir.exists())
        dir.createDirectory();
    return dir;
}

juce::File SystemLogger::getLogDirectory() const
{
    juce::ScopedLock lock(fileLock);
    return logDirectory;
}

juce::File SystemLogger::getCurrentLogFile() const
{
    juce::ScopedLock lock(fileLock);
    return logDirectory.getChildFile("nerou_" + currentDateStamp + ".log");
}

// ── 快照 / 过滤 ─────────────────────────────────────────────────────────────
juce::Array<SystemLogger::Entry> SystemLogger::snapshot() const
{
    juce::ScopedLock lock(bufferLock);
    juce::Array<Entry> out;
    out.ensureStorageAllocated((int)buffer.size());
    for (const auto& e : buffer)
        out.add(e);
    return out;
}

int SystemLogger::getEntryCount() const noexcept
{
    return entryCount.load();
}

juce::Array<SystemLogger::Entry> SystemLogger::snapshotFiltered(Level minLevel,
                                                                const juce::String& categoryContains,
                                                                const juce::String& textContains) const
{
    juce::ScopedLock lock(bufferLock);
    juce::Array<Entry> out;
    for (const auto& e : buffer)
    {
        if ((int)e.level < (int)minLevel) continue;
        if (categoryContains.isNotEmpty()
            && !e.category.containsIgnoreCase(categoryContains)) continue;
        if (textContains.isNotEmpty()
            && !e.message.containsIgnoreCase(textContains)
            && !e.category.containsIgnoreCase(textContains)) continue;
        out.add(e);
    }
    return out;
}

void SystemLogger::clearBuffer()
{
    {
        juce::ScopedLock lock(bufferLock);
        buffer.clear();
        entryCount.store(0);
    }
    sendChangeMessage();
}

bool SystemLogger::exportTo(const juce::File& dest) const
{
    auto entries = snapshot();

    juce::FileOutputStream out(dest);
    if (!out.openedOk())
        return false;
    out.setPosition(0);
    out.truncate();

    out.writeText(juce::String("# NeuroRuntime System Log export — ")
                      + formatTimestamp(juce::Time::currentTimeMillis(), true)
                      + juce::newLine,
                  false, false, nullptr);
    out.writeText(juce::String("# Total entries: ") + juce::String(entries.size()) + juce::newLine,
                  false, false, nullptr);

    for (const auto& e : entries)
        out.writeText(e.formatted() + juce::newLine, false, false, nullptr);
    out.flush();
    return true;
}

// ── 级别辅助 ────────────────────────────────────────────────────────────────
juce::String SystemLogger::levelToString(Level l)
{
    switch (l)
    {
        case Level::Debug:   return "DEBUG";
        case Level::Info:    return "INFO";
        case Level::Warning: return "WARN";
        case Level::Error:   return "ERROR";
    }
    return "INFO";
}

juce::Colour SystemLogger::levelColour(Level l)
{
    switch (l)
    {
        case Level::Debug:   return juce::Colour(0xff8b9cb3);   // 灰蓝
        case Level::Info:    return juce::Colour(0xff2e7d32);   // 绿
        case Level::Warning: return juce::Colour(0xffef6c00);   // 橙
        case Level::Error:   return juce::Colour(0xffc62828);   // 红
    }
    return juce::Colour(0xff2e7d32);
}

} // namespace nerou::core
