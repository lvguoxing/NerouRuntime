#pragma once

#include <JuceHeader.h>

namespace nerou::app {

struct BootstrapSettings
{
    juce::String modelName;
    juce::String dataPath;
    juce::String epochs;
    juce::String trainClassCount;
    juce::String savePath;
    juce::String lastOnnxPath;
    juce::String lastInferNpzPath;
    bool         expertUi = false;
};

struct TrainingSettings
{
    juce::String modelName;
    juce::String dataPath;
    juce::String epochs;
    juce::String trainClassCount;
    juce::String savePath;
    int          batchId = 0;
    int          lrId = 0;
};

struct PrepSettings
{
    juce::String rawPath;
    juce::String outPath;
    juce::String lowHz;
    juce::String highHz;
    int          sampleRateHz = 0;
    int          channelCount = 0;
};

struct RealtimeSettings
{
    int          sourceId = 1;
    int          channelsId = 8;
    int          rateId = 256;
    juce::String playbackPath;
    juce::String liveConnection;
    int          uvRangeId = 100;
    int          filterId = 1;
    int          montageId = 1;
    int          winLenId = 256;
    int          numWinId = 1;
};

struct PerformanceSettings
{
    /** "auto" / "cpu" / "directml" / "cuda"；默认 "auto" */
    juce::String accelerationMode { "auto" };
};

class RuntimeSettingsStore
{
public:
    static BootstrapSettings loadBootstrap(const juce::PropertiesFile* p);
    static void saveTraining(juce::PropertiesFile* p, const TrainingSettings& settings);
    static void savePrep(juce::PropertiesFile* p, const PrepSettings& settings);
    static void saveRealtime(juce::PropertiesFile* p, const RealtimeSettings& settings);
    static RealtimeSettings loadRealtime(const juce::PropertiesFile* p);

    static PerformanceSettings loadPerformance(const juce::PropertiesFile* p);
    static void savePerformance(juce::PropertiesFile* p, const PerformanceSettings& settings);
};

} // namespace nerou::app

