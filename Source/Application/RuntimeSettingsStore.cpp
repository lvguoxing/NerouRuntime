#include "RuntimeSettingsStore.h"
#include "../Core/ProjectPaths.h"

namespace nerou::app {

BootstrapSettings RuntimeSettingsStore::loadBootstrap(const juce::PropertiesFile* p)
{
    BootstrapSettings out;
    if (p == nullptr)
        return out;

    // ── 基本持久化字段 ─────────────────────────────────────────────────
    out.modelName        = p->getValue("modelName", "");
    out.dataPath         = p->getValue("dataPath", "");
    out.epochs           = p->getValue("epochs", "30");
    out.trainClassCount  = p->getValue("trainClassCount", "4");
    out.savePath         = p->getValue("savePath", "");
    out.lastOnnxPath     = p->getValue("lastOnnxPath", "");
    out.lastInferNpzPath = p->getValue("lastInferNpzPath", "");
    out.expertUi         = p->getBoolValue("expertUi", false);

    // ── 智能发现项目根目录（含 data/npz 的祖先目录）────────────────────
    // 优先级: exe向上回溯 > CWD向上回溯 > exe目录本身
    auto findProjectRoot = []() -> juce::File {
        // 策略1: 从 exe 所在目录向上遍历（最多5层，覆盖 build/artefacts/Release/）
        auto dir = juce::File::getSpecialLocation(
            juce::File::currentExecutableFile).getParentDirectory();
        for (int i = 0; i < 5; ++i)
        {
            if (dir.getChildFile("data").getChildFile("npz").isDirectory())
                return dir;
            auto parent = dir.getParentDirectory();
            if (parent == dir) break;  // 到达根目录
            dir = parent;
        }

        // 策略2: CWD 向上遍历（3层）
        auto cwd = juce::File::getCurrentWorkingDirectory();
        for (int i = 0; i < 3; ++i)
        {
            if (cwd.getChildFile("data").getChildFile("npz").isDirectory())
                return cwd;
            auto parent = cwd.getParentDirectory();
            if (parent == cwd) break;
            cwd = parent;
        }

        // 策略3: 回退到 exe 目录
        return juce::File::getSpecialLocation(
            juce::File::currentExecutableFile).getParentDirectory();
    };

    const auto appRoot = findProjectRoot();
    ProjectPaths::ensureApplicationResourceStructure(appRoot);

    if (out.dataPath.isEmpty())
    {
        auto defaultTrainingFiles = appRoot.getChildFile("data").getChildFile("training_files");
        defaultTrainingFiles.createDirectory();
        juce::Array<juce::File> npzFiles;
        defaultTrainingFiles.findChildFiles(npzFiles, juce::File::findFiles, false, "*.npz");
        if (npzFiles.isEmpty() && appRoot.getChildFile("data").getChildFile("npz").isDirectory())
            out.dataPath = appRoot.getChildFile("data").getChildFile("npz").getFullPathName();
        else
            out.dataPath = defaultTrainingFiles.getFullPathName();
    }

    if (out.savePath.isEmpty())
    {
        auto defaultSave = appRoot.getChildFile("onnx").getChildFile("models");
        defaultSave.createDirectory();
        out.savePath = defaultSave.getFullPathName();
    }

    if (out.modelName.isEmpty())
        out.modelName = "eegnet";

    return out;
}

void RuntimeSettingsStore::saveTraining(juce::PropertiesFile* p, const TrainingSettings& settings)
{
    if (p == nullptr)
        return;

    p->setValue("modelName", settings.modelName.trim());
    p->setValue("dataPath", settings.dataPath);
    p->setValue("epochs", settings.epochs.trim());
    p->setValue("trainClassCount", settings.trainClassCount.trim());
    p->setValue("savePath", settings.savePath);
    p->setValue("trainBatchId", juce::String(settings.batchId));
    p->setValue("trainLrId", juce::String(settings.lrId));
    p->saveIfNeeded();
}

void RuntimeSettingsStore::savePrep(juce::PropertiesFile* p, const PrepSettings& settings)
{
    if (p == nullptr)
        return;

    p->setValue("prepRawPath", settings.rawPath);
    p->setValue("prepOutPath", settings.outPath);
    p->setValue("prepLowHz", settings.lowHz.trim());
    p->setValue("prepHighHz", settings.highHz.trim());
    p->setValue("prepSrHz", juce::String(settings.sampleRateHz));
    p->setValue("prepCh", juce::String(settings.channelCount));
    p->saveIfNeeded();
}

void RuntimeSettingsStore::saveRealtime(juce::PropertiesFile* p, const RealtimeSettings& settings)
{
    if (p == nullptr)
        return;

    p->setValue("realtimeSourceId", juce::String(settings.sourceId));
    p->setValue("realtimeChannelsId", juce::String(settings.channelsId));
    p->setValue("realtimeRateId", juce::String(settings.rateId));
    p->setValue("realtimePlaybackPath", settings.playbackPath);
    p->setValue("realtimeLiveConnectionString", settings.liveConnection);
    p->setValue("realtimeUvRangeId", juce::String(settings.uvRangeId));
    p->setValue("realtimeFilterId", juce::String(settings.filterId));
    p->setValue("realtimeMontageId", juce::String(settings.montageId));
    p->setValue("realtimeWinLenId", juce::String(settings.winLenId));
    p->setValue("realtimeNumWinId", juce::String(settings.numWinId));
    p->saveIfNeeded();
}

PerformanceSettings RuntimeSettingsStore::loadPerformance(const juce::PropertiesFile* p)
{
    PerformanceSettings out;
    if (p == nullptr)
        return out;
    out.accelerationMode = p->getValue("accelerationMode", "auto").trim().toLowerCase();
    if (out.accelerationMode != "auto" && out.accelerationMode != "cpu"
        && out.accelerationMode != "directml" && out.accelerationMode != "cuda")
        out.accelerationMode = "auto";
    return out;
}

void RuntimeSettingsStore::savePerformance(juce::PropertiesFile* p, const PerformanceSettings& settings)
{
    if (p == nullptr)
        return;
    p->setValue("accelerationMode", settings.accelerationMode);
    p->saveIfNeeded();
}

RealtimeSettings RuntimeSettingsStore::loadRealtime(const juce::PropertiesFile* p)
{
    RealtimeSettings out;
    if (p == nullptr)
        return out;

    out.sourceId = p->getIntValue("realtimeSourceId", 1);
    out.channelsId = p->getIntValue("realtimeChannelsId", 8);
    out.rateId = p->getIntValue("realtimeRateId", 256);
    out.playbackPath = p->getValue("realtimePlaybackPath", "");
    out.liveConnection = p->getValue("realtimeLiveConnectionString", "");
    out.uvRangeId = p->getIntValue("realtimeUvRangeId", 100);
    out.filterId = p->getIntValue("realtimeFilterId", 1);
    out.montageId = p->getIntValue("realtimeMontageId", 1);
    out.winLenId = p->getIntValue("realtimeWinLenId", 256);
    out.numWinId = p->getIntValue("realtimeNumWinId", 1);
    return out;
}

} // namespace nerou::app
