#pragma once

#include <JuceHeader.h>
#include "NpzEEGLoader.h"
#include "Utf8FileText.h"
#include <algorithm>
#include <map>
#include <utility>
#include <vector>

struct TrainPreflightReport {
    enum class Code { Ok, BadDir, NoNpz, NpzReadErr, ManifestMismatch };

    Code         code        = Code::BadDir;
    bool         canStart    = false;
    /** 与所选主模态 C×T 一致、将参与训练的 NPZ 个数 */
    int          npzCount    = 0;
    /** 目录内 *.npz 总数（可能包含其它形状，训练时会自动跳过） */
    int          npzTotalInDir = 0;
    int          sampleCount = 0;
    int          classCount = 0;
    int          shapeC      = 0;
    int          shapeT      = 0;
    int          manifestC   = 0;
    int          manifestT   = 0;
    int          manifestSampleRate = 0;
    int          manifestClassCount = 0;
    juce::File   manifestPath;
    juce::String npzError;
    bool         hasManifest = false;
};

namespace TrainPreflight {

inline int getJsonInt(juce::DynamicObject* obj,
                      const juce::Identifier& camelName,
                      const juce::Identifier& snakeName,
                      int fallback = 0)
{
    if (obj == nullptr)
        return fallback;
    if (obj->hasProperty(camelName))
        return (int)obj->getProperty(camelName).operator int();
    if (obj->hasProperty(snakeName))
        return (int)obj->getProperty(snakeName).operator int();
    return fallback;
}

inline void enrichFromDatasetSummary(TrainPreflightReport& report, const juce::File& dataDir)
{
    const auto summaryFile = dataDir.getChildFile("dataset_summary.json");
    if (summaryFile.existsAsFile())
    {
        auto parsed = juce::JSON::parse(nerou::core::loadTextFileAsUtf8(summaryFile));
        if (parsed.isObject())
        {
            auto* obj = parsed.getDynamicObject();
            report.sampleCount = getJsonInt(obj, "sampleCount", "sample_count", report.sampleCount);
            report.classCount = getJsonInt(obj, "classCount", "label_count", report.classCount);
            const int c = getJsonInt(obj, "channelCount", "channel_count");
            const int t = getJsonInt(obj, "windowSizeSamples", "window_size");
            if (c > 0)
                report.shapeC = c;
            if (t > 0)
                report.shapeT = t;
        }
    }

    const auto labelsFile = dataDir.getChildFile("labels.json");
    if (report.classCount <= 0 && labelsFile.existsAsFile())
    {
        auto parsed = juce::JSON::parse(nerou::core::loadTextFileAsUtf8(labelsFile));
        if (auto* arr = parsed.getArray())
            report.classCount = arr->size();
        else if (parsed.isObject())
            report.classCount = parsed.getDynamicObject()->getProperties().size();
    }
}

inline juce::File resolveDatasetManifestFile(const juce::File& dataDir, const juce::File& explicitManifestFile)
{
    if (explicitManifestFile.existsAsFile())
        return explicitManifestFile;

    const auto datasetManifest = dataDir.getChildFile("dataset_manifest.json");
    if (datasetManifest.existsAsFile())
        return datasetManifest;

    const auto legacySummary = dataDir.getChildFile("dataset_summary.json");
    if (legacySummary.existsAsFile())
        return legacySummary;

    return {};
}

inline TrainPreflightReport evaluate(const juce::File& dataDir, const juce::File& manifestFile)
{
    TrainPreflightReport r;

    if (!dataDir.isDirectory())
    {
        r.code = TrainPreflightReport::Code::BadDir;
        return r;
    }

    juce::Array<juce::File> npzs;
    dataDir.findChildFiles(npzs, juce::File::findFiles, false, "*.npz");
    npzs.sort();
    if (npzs.isEmpty())
    {
        r.code = TrainPreflightReport::Code::NoNpz;
        return r;
    }

    r.npzTotalInDir = npzs.size();

    std::map<std::pair<int, int>, std::vector<juce::File>> groups;
    juce::String                                           firstErr;
    for (const auto& f : npzs)
    {
        auto win = NpzEEGLoader::loadWindow(f, 0);
        if (win.error.isNotEmpty())
        {
            if (firstErr.isEmpty())
                firstErr = f.getFileName() + ": " + win.error;
            continue;
        }
        groups[{win.channels, win.timePoints}].push_back(f);
    }

    if (groups.empty())
    {
        r.code     = TrainPreflightReport::Code::NpzReadErr;
        r.npzError = firstErr.isEmpty() ? juce::String("无法读取目录内 NPZ") : firstErr;
        return r;
    }

    auto bestIt = std::max_element(
        groups.begin(),
        groups.end(),
        [](const auto& a, const auto& b) {
            const int na = (int)a.second.size();
            const int nb = (int)b.second.size();
            if (na != nb)
                return na < nb;
            if (a.first.first != b.first.first)
                return a.first.first < b.first.first;
            return a.first.second < b.first.second;
        });

    const auto& bestCt   = bestIt->first;
    const auto& bestFiles = bestIt->second;

    r.npzCount = (int)bestFiles.size();
    r.shapeC   = bestCt.first;
    r.shapeT   = bestCt.second;
    r.code     = TrainPreflightReport::Code::Ok;
    r.canStart = true;

    enrichFromDatasetSummary(r, dataDir);

    const auto resolvedManifest = resolveDatasetManifestFile(dataDir, manifestFile);
    if (resolvedManifest.existsAsFile())
    {
        r.hasManifest = true;
        r.manifestPath = resolvedManifest;
        auto parsed   = juce::JSON::parse(nerou::core::loadTextFileAsUtf8(resolvedManifest));
        if (parsed.isObject())
        {
            auto* o = parsed.getDynamicObject();
            r.manifestC = (int)o->getProperty("channelCount");
            r.manifestT = (int)o->getProperty("windowSizeSamples");
            if (r.manifestC <= 0)
                r.manifestC = (int)o->getProperty("channel_count");
            if (r.manifestT <= 0)
                r.manifestT = (int)o->getProperty("window_size");
            r.manifestSampleRate = (int)o->getProperty("sampleRateHz");
            if (r.manifestSampleRate <= 0)
                r.manifestSampleRate = (int)o->getProperty("sample_rate");
            r.manifestClassCount = (int)o->getProperty("classCount");
            if (r.manifestClassCount <= 0)
                r.manifestClassCount = (int)o->getProperty("label_count");
            if (r.classCount <= 0 && r.manifestClassCount > 0)
                r.classCount = r.manifestClassCount;

            const bool okC = (r.manifestC <= 0 || r.manifestC == r.shapeC);
            const bool okT = (r.manifestT <= 0 || r.manifestT == r.shapeT);
            if (!okC || !okT)
            {
                r.code     = TrainPreflightReport::Code::ManifestMismatch;
                r.canStart = false;
            }
        }
    }

    return r;
}

} // namespace TrainPreflight
