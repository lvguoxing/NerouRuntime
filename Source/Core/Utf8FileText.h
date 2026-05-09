#pragma once

#include <JuceHeader.h>

namespace nerou::core {

inline juce::String loadTextFileAsUtf8(const juce::File& file)
{
    juce::MemoryBlock bytes;
    if (!file.loadFileAsData(bytes) || bytes.getSize() == 0)
        return file.loadFileAsString();

    const char* data = static_cast<const char*>(bytes.getData());
    int size = static_cast<int>(bytes.getSize());
    if (size >= 3
        && static_cast<unsigned char>(data[0]) == 0xef
        && static_cast<unsigned char>(data[1]) == 0xbb
        && static_cast<unsigned char>(data[2]) == 0xbf)
    {
        data += 3;
        size -= 3;
    }

    return juce::String::fromUTF8(data, size);
}

} // namespace nerou::core
