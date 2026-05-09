#pragma once

#include <JuceHeader.h>

#include "../Domain/Entities.h"
#include "../Domain/MetadataSerializer.h"

namespace nerou::services {

/**
 * ModelRegistryService
 *
 * 模型资产管理中心，职责：
 *   1. 扫描目录，识别所有 manifest.json + .onnx 对
 *   2. 注册新模型，写出 models_index.json
 *   3. 按 ID / 兼容参数查找模型
 *   4. 更新验证状态
 */
class ModelRegistryService
{
public:
    ModelRegistryService() = default;
    ~ModelRegistryService() = default;

    // ── 根目录 ────────────────────────────────────────────────────────────────
    void setRootDirectory(const juce::File& directory);
    juce::File getRootDirectory() const;

    // ── 扫描 / 加载 ───────────────────────────────────────────────────────────

    /**
     * 递归扫描 rootDirectory（或指定目录）下的所有 manifest.json 文件，
     * 解析为 ModelArtifact 并更新内部缓存。
     * 扫描完成后写出 models_index.json。
     * 返回找到的模型数量。
     */
    int scanDirectory(const juce::File& dir = {});

    /**
     * 从指定目录加载 models_index.json，反序列化为缓存。
     * 适合启动时恢复上次扫描结果。
     */
    bool loadIndex(const juce::File& indexFile = {});

    /**
     * 将当前缓存持久化为 models_index.json。
     */
    bool saveIndex(const juce::File& indexFile = {}) const;

    // ── 注册 / 更新 ───────────────────────────────────────────────────────────

    /**
     * 注册一个新的 ModelArtifact（通常由 TrainingService 在训练完成后调用）。
     * 若已有相同 id，则更新。写出 models_index.json。
     */
    void registerModel(const domain::ModelArtifact& artifact);

    /** 更新已注册模型的验证状态 */
    void updateValidationStatus(const juce::String& modelId,
                                 const juce::String& status,
                                 bool isRecommended = false);

    // ── 查询 ─────────────────────────────────────────────────────────────────

    /** 按 ID 查找，返回 nullptr 表示未找到 */
    const domain::ModelArtifact* findById(const juce::String& id) const noexcept;

    /**
     * 查找兼容指定输入参数的模型。
     * 传入 0 表示不限制该维度。
     */
    juce::Array<domain::ModelArtifact> findCompatible(int channels,
                                                       int samplePoints  = 0,
                                                       int sampleRateHz  = 0) const;

    /** 返回当前全部缓存模型 */
    const juce::Array<domain::ModelArtifact>& getCachedModels() const noexcept;

    /** 手动替换缓存（供迁移兼容） */
    void setCachedModels(const juce::Array<domain::ModelArtifact>& models);
    void clearCache();

private:
    juce::File defaultIndexFile() const;

    juce::File rootDirectory;
    juce::Array<domain::ModelArtifact> cachedModels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModelRegistryService)
};

} // namespace nerou::services
