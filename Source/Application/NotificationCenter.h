#pragma once

#include <JuceHeader.h>

namespace nerou::app {

/**
 * NotificationCenter — Services → UI 事件广播桥
 *
 * Services（训练/验证/数据准备/采集）在任务关键节点调用 post()，
 * MainComponent 等 UI 层订阅后转发给 SnackbarManager 显示。
 *
 * 设计原则：
 *   - 轻量单例，零依赖 UI 框架
 *   - 所有通知投递到消息线程（juce::MessageManager::callAsync）
 *   - Listener 的 onNotification 保证在 UI 线程调用
 */
class NotificationCenter
{
public:
    // ── 通知级别 ────────────────────────────────────────────────────────────
    enum class Level
    {
        Info,     // 普通信息（蓝）
        Success,  // 成功（绿）
        Warning,  // 警告（橙）
        Error     // 错误（红）
    };

    // ── 通知结构 ────────────────────────────────────────────────────────────
    struct Notification
    {
        Level        level   = Level::Info;
        juce::String title;        // 主标题（Snackbar 主文字）
        juce::String detail;       // 详情（可为空；用于 Snackbar 操作文字）
        juce::String actionId;     // 动作标识符（供监听者按需处理）
        juce::Time   timestamp;

        Notification() : timestamp(juce::Time::getCurrentTime()) {}
        Notification(Level l, const juce::String& t, const juce::String& d = {},
                     const juce::String& aid = {})
            : level(l), title(t), detail(d), actionId(aid)
            , timestamp(juce::Time::getCurrentTime()) {}
    };

    // ── 单例访问 ────────────────────────────────────────────────────────────
    static NotificationCenter& getInstance() noexcept
    {
        static NotificationCenter instance;
        return instance;
    }

    // ── 发布接口（任意线程可调用） ──────────────────────────────────────────
    void post(Level level,
              const juce::String& title,
              const juce::String& detail = {},
              const juce::String& actionId = {})
    {
        Notification n { level, title, detail, actionId };
        juce::MessageManager::callAsync([this, n]() {
            dispatchOnUIThread(n);
        });
    }

    void postSuccess(const juce::String& title, const juce::String& detail = {})
    {
        post(Level::Success, title, detail);
    }

    void postError(const juce::String& title, const juce::String& detail = {})
    {
        post(Level::Error, title, detail);
    }

    void postWarning(const juce::String& title, const juce::String& detail = {})
    {
        post(Level::Warning, title, detail);
    }

    void postInfo(const juce::String& title, const juce::String& detail = {})
    {
        post(Level::Info, title, detail);
    }

    // ── 监听器 ──────────────────────────────────────────────────────────────
    class Listener
    {
    public:
        virtual ~Listener() = default;
        /** 在 UI 线程调用。*/
        virtual void onNotification(const Notification& n) = 0;
    };

    void addListener(Listener* l)
    {
        juce::ScopedLock lock(listenersLock);
        listeners.add(l);
    }

    void removeListener(Listener* l)
    {
        juce::ScopedLock lock(listenersLock);
        listeners.remove(l);
    }

    // ── 历史记录（最近 50 条，UI 线程读取）────────────────────────────────
    const juce::Array<Notification>& getHistory() const noexcept { return history; }
    int getUnreadCount() const noexcept { return unreadCount; }
    void markAllRead() noexcept { unreadCount = 0; }

private:
    NotificationCenter() = default;
    ~NotificationCenter() = default;
    NotificationCenter(const NotificationCenter&) = delete;
    NotificationCenter& operator=(const NotificationCenter&) = delete;

    void dispatchOnUIThread(const Notification& n)
    {
        // 历史记录（最多保留 50 条）
        history.add(n);
        while (history.size() > 50)
            history.remove(0);
        ++unreadCount;

        // 广播给监听者
        juce::ScopedLock lock(listenersLock);
        listeners.call(&Listener::onNotification, n);
    }

    juce::ListenerList<Listener>   listeners;
    juce::CriticalSection          listenersLock;
    juce::Array<Notification>      history;
    int                            unreadCount = 0;
};

// ── 全局便捷访问 ────────────────────────────────────────────────────────────
inline NotificationCenter& Notify() { return NotificationCenter::getInstance(); }

} // namespace nerou::app
