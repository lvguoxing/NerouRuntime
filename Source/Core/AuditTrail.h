#pragma once

#include <JuceHeader.h>

namespace nerou::core {

struct AuditEvent
{
    juce::String eventType;
    juce::String category;
    juce::String projectId;
    juce::String subjectId;
    juce::String datasetId;
    juce::String modelId;
    juce::String objectId;
    juce::String objectType;
    juce::String action;
    bool success = false;
    juce::String message;
    juce::File outputDir;
    juce::var details;
};

class AuditTrail
{
public:
    static bool appendEvent(const AuditEvent& event);
    static bool appendEvent(const juce::File& outputDir,
                            const juce::String& eventType,
                            const juce::String& category,
                            bool success,
                            const juce::String& message,
                            const juce::var& details = {});

private:
    static juce::File resolveAuditFile(const juce::File& outputDir);
    static juce::String isoNow();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuditTrail)
};

} // namespace nerou::core
