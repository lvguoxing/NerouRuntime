#include "AuditTrail.h"
#include "SystemLogger.h"

namespace nerou::core {

juce::String AuditTrail::isoNow()
{
    return juce::Time::getCurrentTime().toISO8601(true);
}

juce::File AuditTrail::resolveAuditFile(const juce::File& outputDir)
{
    if (outputDir.getFullPathName().isEmpty())
        return {};

    outputDir.createDirectory();
    return outputDir.getChildFile("audit_events.jsonl");
}

bool AuditTrail::appendEvent(const AuditEvent& event)
{
    const auto auditFile = resolveAuditFile(event.outputDir);
    if (auditFile.getFullPathName().isEmpty())
        return false;

    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty("eventVersion", "1.0.0");
    obj->setProperty("timestamp", isoNow());
    obj->setProperty("eventType", event.eventType);
    obj->setProperty("category", event.category);
    obj->setProperty("projectId", event.projectId);
    obj->setProperty("subjectId", event.subjectId);
    obj->setProperty("datasetId", event.datasetId);
    obj->setProperty("modelId", event.modelId);
    obj->setProperty("objectId", event.objectId);
    obj->setProperty("objectType", event.objectType);
    obj->setProperty("action", event.action);
    obj->setProperty("success", event.success);
    obj->setProperty("message", event.message);
    obj->setProperty("outputDir", event.outputDir.getFullPathName());
    if (!event.details.isVoid())
        obj->setProperty("details", event.details);

    const auto line = juce::JSON::toString(juce::var(obj.release()), false);
    if (auto stream = auditFile.createOutputStream(1024))
    {
        stream->setPosition(auditFile.getSize());
        stream->writeText(line + "\n", false, false, nullptr);
        stream->flush();

        NR_LOGI("Audit", event.eventType + " | " + event.message);
        return true;
    }

    NR_LOGW("Audit", "Failed to append audit event: " + event.eventType);
    return false;
}

bool AuditTrail::appendEvent(const juce::File& outputDir,
                             const juce::String& eventType,
                             const juce::String& category,
                             bool success,
                             const juce::String& message,
                             const juce::var& details)
{
    AuditEvent event;
    event.outputDir = outputDir;
    event.eventType = eventType;
    event.category = category;
    event.success = success;
    event.message = message;
    event.details = details;
    return appendEvent(event);
}

} // namespace nerou::core
