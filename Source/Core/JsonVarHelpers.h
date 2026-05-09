#pragma once

#include <JuceHeader.h>

namespace nerou::core {

template <typename Collection, typename Mapper>
inline juce::var mapToVarArray(const Collection& values, Mapper mapper)
{
    juce::Array<juce::var> arrayValues;
    for (const auto& value : values)
        arrayValues.add(mapper(value));
    return juce::var(arrayValues);
}

inline juce::var stringArrayToVar(const juce::StringArray& values)
{
    return mapToVarArray(values, [](const auto& value) { return juce::var(value); });
}

} // namespace nerou::core
