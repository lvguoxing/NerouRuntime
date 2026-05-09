#include "ModelRegistryService.h"
#include "../Core/JsonFileIO.h"
#include "../Core/JsonVarHelpers.h"

namespace nerou::services {

// ─────────────────────────────────────────────────────────────────────────────
// 根目录
// ─────────────────────────────────────────────────────────────────────────────

void ModelRegistryService::setRootDirectory(const juce::File& directory)
{
    rootDirectory = directory;
}

juce::File ModelRegistryService::getRootDirectory() const
{
    return rootDirectory;
}

juce::File ModelRegistryService::defaultIndexFile() const
{
    return rootDirectory.getChildFile("models_index.json");
}

// ─────────────────────────────────────────────────────────────────────────────
// 扫描 / 加载
// ─────────────────────────────────────────────────────────────────────────────

int ModelRegistryService::scanDirectory(const juce::File& dir)
{
    juce::File searchDir = dir.exists() ? dir : rootDirectory;
    if (!searchDir.isDirectory())
        return 0;

    // 递归查找所有 manifest.json
    auto manifestFiles = searchDir.findChildFiles(
        juce::File::findFiles, true, "manifest.json");

    int found = 0;
    for (const auto& mf : manifestFiles)
    {
        domain::ModelArtifact artifact;
        if (domain::MetadataSerializer::loadManifestJson(mf, artifact))
        {
            // 更新或插入
            bool updated = false;
            for (int i = 0; i < cachedModels.size(); ++i)
            {
                if (cachedModels[i].id == artifact.id)
                {
                    cachedModels.set(i, artifact);
                    updated = true;
                    break;
                }
            }
            if (!updated)
                cachedModels.add(artifact);
            ++found;
        }
    }

    // 同时扫描根目录下的 .onnx 文件（无 manifest 情况下生成最小 artifact）
    auto orphanOnnx = searchDir.findChildFiles(
        juce::File::findFiles, true, "*.onnx");

    for (const auto& onnxFile : orphanOnnx)
    {
        // 若同目录已有 manifest 则跳过（已在上面处理）
        auto companion = onnxFile.getParentDirectory().getChildFile("manifest.json");
        if (companion.existsAsFile())
            continue;

        // 检查缓存中是否已有该 onnx
        bool alreadyKnown = false;
        for (const auto& m : cachedModels)
            if (m.onnxPath == onnxFile) { alreadyKnown = true; break; }
        if (alreadyKnown)
            continue;

        domain::ModelArtifact artifact;
        artifact.id        = juce::String::toHexString(onnxFile.getFullPathName().hashCode64());
        artifact.modelName = onnxFile.getFileNameWithoutExtension();
        artifact.onnxPath  = onnxFile;
        artifact.version   = "1.0";
        artifact.createdAt = onnxFile.getCreationTime();
        cachedModels.add(artifact);
        ++found;
    }

    if (found > 0)
        saveIndex();

    return found;
}

bool ModelRegistryService::loadIndex(const juce::File& indexFile)
{
    juce::File f = indexFile.existsAsFile() ? indexFile : defaultIndexFile();
    if (!f.existsAsFile())
        return false;

    auto root = juce::JSON::parse(f.loadFileAsString());
    if (root.isVoid())
        return false;

    auto* arr = root.getProperty("models", juce::var()).getArray();
    if (!arr)
        return false;

    cachedModels.clear();
    for (const auto& item : *arr)
    {
        domain::ModelArtifact artifact;
        artifact.id              = item.getProperty("model_id", {}).toString();
        artifact.modelName       = item.getProperty("model_name", {}).toString();
        artifact.version         = item.getProperty("version", "1.0").toString();
        artifact.taskType        = item.getProperty("task_type", {}).toString();
        artifact.onnxPath        = juce::File(item.getProperty("onnx_path", {}).toString());
        artifact.manifestPath    = juce::File(item.getProperty("manifest_path", {}).toString());
        artifact.inputChannels   = (int) item.getProperty("input_channels", 0);
        artifact.inputSamples    = (int) item.getProperty("input_samples", 0);
        artifact.inputSampleRate = (int) item.getProperty("input_sample_rate", 0);
        artifact.outputClasses   = (int) item.getProperty("output_classes", 0);
        artifact.validationStatus= item.getProperty("validation_status", "unknown").toString();
        artifact.isRecommended   = (bool)item.getProperty("is_recommended", false);
        artifact.createdFromJobId= item.getProperty("created_from_job_id", {}).toString();

        artifact.classNames.clear();
        if (auto* cnArr = item.getProperty("class_names", {}).getArray())
            for (const auto& v : *cnArr)
                artifact.classNames.add(v.toString());

        auto createdMs = (juce::int64) item.getProperty("created_at_ms", 0.0);
        if (createdMs > 0) artifact.createdAt = juce::Time(createdMs);

        if (artifact.isValid())
            cachedModels.add(artifact);
    }

    return true;
}

bool ModelRegistryService::saveIndex(const juce::File& indexFile) const
{
    juce::File f = indexFile.existsAsFile() ? indexFile : defaultIndexFile();

    auto modelArr = nerou::core::mapToVarArray(cachedModels, [](const auto& artifact) {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("model_id",          artifact.id);
        obj->setProperty("model_name",         artifact.modelName);
        obj->setProperty("version",            artifact.version);
        obj->setProperty("task_type",          artifact.taskType);
        obj->setProperty("onnx_path",          artifact.onnxPath.getFullPathName());
        obj->setProperty("manifest_path",      artifact.manifestPath.getFullPathName());
        obj->setProperty("input_channels",     artifact.inputChannels);
        obj->setProperty("input_samples",      artifact.inputSamples);
        obj->setProperty("input_sample_rate",  artifact.inputSampleRate);
        obj->setProperty("output_classes",     artifact.outputClasses);
        obj->setProperty("validation_status",  artifact.validationStatus);
        obj->setProperty("is_recommended",     artifact.isRecommended);
        obj->setProperty("created_from_job_id",artifact.createdFromJobId);
        obj->setProperty("created_at_ms",      (double) artifact.createdAt.toMilliseconds());

        obj->setProperty("class_names", nerou::core::stringArrayToVar(artifact.classNames));
        return juce::var(obj.release());
    });

    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("schema_version", "1.0");
    root->setProperty("model_count",    cachedModels.size());
    root->setProperty("models",         modelArr);

    return nerou::core::writeJsonFile(f, juce::var(root.release()));
}

// ─────────────────────────────────────────────────────────────────────────────
// 注册 / 更新
// ─────────────────────────────────────────────────────────────────────────────

void ModelRegistryService::registerModel(const domain::ModelArtifact& artifact)
{
    if (!artifact.isValid())
        return;

    for (int i = 0; i < cachedModels.size(); ++i)
    {
        if (cachedModels[i].id == artifact.id)
        {
            cachedModels.set(i, artifact);
            saveIndex();
            return;
        }
    }

    cachedModels.add(artifact);
    saveIndex();
}

void ModelRegistryService::updateValidationStatus(const juce::String& modelId,
                                                   const juce::String& status,
                                                   bool isRecommended)
{
    for (int i = 0; i < cachedModels.size(); ++i)
    {
        if (cachedModels[i].id == modelId)
        {
            auto copy            = cachedModels[i];
            copy.validationStatus = status;
            copy.isRecommended    = isRecommended;
            cachedModels.set(i, copy);
            saveIndex();
            return;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 查询
// ─────────────────────────────────────────────────────────────────────────────

const domain::ModelArtifact* ModelRegistryService::findById(const juce::String& id) const noexcept
{
    for (const auto& m : cachedModels)
        if (m.id == id)
            return &m;
    return nullptr;
}

juce::Array<domain::ModelArtifact>
ModelRegistryService::findCompatible(int channels, int samplePoints, int sampleRateHz) const
{
    juce::Array<domain::ModelArtifact> result;

    for (const auto& m : cachedModels)
    {
        if (!m.onnxPath.existsAsFile())
            continue;

        bool ok = true;
        if (channels > 0 && m.inputChannels > 0 && m.inputChannels != channels)
            ok = false;
        if (samplePoints > 0 && m.inputSamples > 0 && m.inputSamples != samplePoints)
            ok = false;
        if (sampleRateHz > 0 && m.inputSampleRate > 0 && m.inputSampleRate != sampleRateHz)
            ok = false;

        if (ok)
            result.add(m);
    }

    return result;
}

const juce::Array<domain::ModelArtifact>& ModelRegistryService::getCachedModels() const noexcept
{
    return cachedModels;
}

void ModelRegistryService::setCachedModels(const juce::Array<domain::ModelArtifact>& models)
{
    cachedModels = models;
}

void ModelRegistryService::clearCache()
{
    cachedModels.clear();
}

} // namespace nerou::services
