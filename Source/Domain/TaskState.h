#pragma once

#include <JuceHeader.h>

namespace nerou::domain {

enum class TaskState
{
    Idle,
    Queued,
    Running,
    Paused,
    Completed,
    Failed,
    Cancelled
};

inline juce::String toDisplayString(TaskState state)
{
    switch (state)
    {
        case TaskState::Idle:      return "Idle";
        case TaskState::Queued:    return "Queued";
        case TaskState::Running:   return "Running";
        case TaskState::Paused:    return "Paused";
        case TaskState::Completed: return "Completed";
        case TaskState::Failed:    return "Failed";
        case TaskState::Cancelled: return "Cancelled";
    }

    return "Unknown";
}

inline juce::String toLocalizedDisplayString(TaskState state)
{
    switch (state)
    {
        case TaskState::Idle:      return juce::String::fromUTF8(u8"空闲");
        case TaskState::Queued:    return juce::String::fromUTF8(u8"排队中");
        case TaskState::Running:   return juce::String::fromUTF8(u8"运行中");
        case TaskState::Paused:    return juce::String::fromUTF8(u8"已暂停");
        case TaskState::Completed: return juce::String::fromUTF8(u8"已完成");
        case TaskState::Failed:    return juce::String::fromUTF8(u8"失败");
        case TaskState::Cancelled: return juce::String::fromUTF8(u8"已取消");
    }

    return juce::String::fromUTF8(u8"未知");
}

inline bool isActiveTaskState(TaskState state) noexcept
{
    return state == TaskState::Queued
        || state == TaskState::Running
        || state == TaskState::Paused;
}

inline bool isTerminalTaskState(TaskState state) noexcept
{
    return state == TaskState::Completed
        || state == TaskState::Failed
        || state == TaskState::Cancelled;
}

} // namespace nerou::domain
