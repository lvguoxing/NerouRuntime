#pragma once

#include <JuceHeader.h>
#include <deque>
#include <atomic>

/**
 * SystemLogger — 全局系统日志基础设施
 *
 * 目标：
 *   1. 统一收集全进程日志（DBG / juce::Logger::writeToLog / NR_LOG*）
 *   2. 日志分级（Debug/Info/Warning/Error）与分类（Acquisition/Training/...）
 *   3. 内存环形缓冲（最近 N 条，供 UI 面板查看）
 *   4. 按天滚动的文件日志：<AppRoot>/logs/nerou_YYYYMMDD.log
 *   5. 提供 ChangeBroadcaster，UI 面板实时刷新
 *   6. 自动桥接 NotificationCenter：Warning/Error 级别自动提示用户
 *
 * 使用：
 *   NR_LOGI("训练", "开始训练任务：5 类 × 30 epoch");
 *   NR_LOGE("推理", "ONNX 加载失败：" + err);
 */

namespace nerou::core {

class SystemLogger : public juce::Logger,
                     public juce::ChangeBroadcaster
{
public:
    enum class Level { Debug = 0, Info = 1, Warning = 2, Error = 3 };

    struct Entry
    {
        juce::int64  timestampMs { 0 };
        Level        level { Level::Info };
        juce::String category;
        juce::String message;

        juce::String formatted() const;
        juce::String levelTag() const;
    };

    static SystemLogger& getInstance();

    // ── 发布接口（线程安全，任意线程调用） ──────────────────────────────────
    void log(Level level, const juce::String& category, const juce::String& message);
    void logDebug  (const juce::String& category, const juce::String& message) { log(Level::Debug,   category, message); }
    void logInfo   (const juce::String& category, const juce::String& message) { log(Level::Info,    category, message); }
    void logWarning(const juce::String& category, const juce::String& message) { log(Level::Warning, category, message); }
    void logError  (const juce::String& category, const juce::String& message) { log(Level::Error,   category, message); }

    /** juce::Logger 接口：接收 DBG()、juce::Logger::writeToLog(..) 的流量。 */
    void logMessage(const juce::String& message) override;

    // ── 查询接口（UI 线程）──────────────────────────────────────────────────
    /** 返回当前缓冲副本，供 UI 面板渲染 */
    juce::Array<Entry> snapshot() const;

    int getEntryCount() const noexcept;
    juce::Array<Entry> snapshotFiltered(Level minLevel,
                                        const juce::String& categoryContains = {},
                                        const juce::String& textContains = {}) const;

    /** 清空内存缓冲（不删除磁盘文件） */
    void clearBuffer();

    /** 暂停记录（UI 查看时防止刷新抖动；文件依旧写入）*/
    void setPaused(bool p) noexcept { paused.store(p); }
    bool isPaused() const noexcept  { return paused.load(); }

    /** 是否将 Warning/Error 级别联动到 NotificationCenter。默认开启。*/
    void setBridgeNotifications(bool b) noexcept { bridgeNotifications.store(b); }

    /** 当前日志目录 */
    juce::File getLogDirectory() const;
    /** 当天滚动文件 */
    juce::File getCurrentLogFile() const;

    /** 导出最近 N 条为单个 .log 文件 */
    bool exportTo(const juce::File& dest) const;

    static juce::String levelToString(Level l);
    static juce::Colour levelColour(Level l);

private:
    SystemLogger();
    ~SystemLogger() override = default;

    SystemLogger(const SystemLogger&) = delete;
    SystemLogger& operator=(const SystemLogger&) = delete;

    void pushEntry(Entry entry);
    void writeToDisk(const Entry& entry);
    juce::File resolveLogDirectory() const;

    static constexpr int kMaxInMemory = 4000;

    mutable juce::CriticalSection bufferLock;
    std::deque<Entry>             buffer;
    std::atomic<int>              entryCount { 0 };

    mutable juce::CriticalSection fileLock;
    juce::File                    logDirectory;
    juce::String                  currentDateStamp; // yyyyMMdd
    // 持久化文件流：避免每条日志反复 open/close，显著降低启动期磁盘写入开销
    std::unique_ptr<juce::FileOutputStream> diskStream;
    int                           pendingFlushBytes { 0 };

    std::atomic<bool>             paused { false };
    std::atomic<bool>             bridgeNotifications { true };
    // 防止 NotificationCenter → Logger → NotificationCenter 递归
    std::atomic<bool>             reentryGuard { false };
};

} // namespace nerou::core

// ── 公共宏 ──────────────────────────────────────────────────────────────────
#define NR_LOGD(cat, msg) ::nerou::core::SystemLogger::getInstance().logDebug  (cat, msg)
#define NR_LOGI(cat, msg) ::nerou::core::SystemLogger::getInstance().logInfo   (cat, msg)
#define NR_LOGW(cat, msg) ::nerou::core::SystemLogger::getInstance().logWarning(cat, msg)
#define NR_LOGE(cat, msg) ::nerou::core::SystemLogger::getInstance().logError  (cat, msg)
