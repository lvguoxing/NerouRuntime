#pragma once

#include <JuceHeader.h>

namespace nerou::core {

inline bool writeJsonFile(const juce::File& targetFile,
                          const juce::var& payload,
                          bool pretty = true)
{
    const auto parent = targetFile.getParentDirectory();
    if (!parent.exists() && !parent.createDirectory().wasOk())
        return false;

    return targetFile.replaceWithText(juce::JSON::toString(payload, pretty));
}

} // namespace nerou::core
