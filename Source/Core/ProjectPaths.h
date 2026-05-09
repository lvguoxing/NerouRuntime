#pragma once

#include <JuceHeader.h>

/**
 * ProjectPaths
 *
 * 两级路径体系：
 *   1. 应用工程根（App Root）：可执行文件向上查找，含 onnx/models 的目录。
 *      用于 Python 脚本、ONNX 模型目录等全局资源。
 *
 *   2. 用户项目目录（Project Root）：由用户创建，固定子目录结构：
 *      <project_root>/
 *        project.json          项目元数据
 *        subjects.json         受试者注册表
 *        sessions/             每次采集会话目录（session.json + NPZ）
 *        recordings/           录制产物（recording.json + .npz）
 *        datasets/             预处理数据集（dataset_summary.json + .npz）
 *        models/               模型产物（manifest.json + .onnx + labels.json）
 *        validations/          验证结果（validation_result.json）
 *        logs/                 训练日志、运行日志
 */
namespace ProjectPaths {

// ============================================================================
// 应用工程根（App Root）
// ============================================================================

namespace detail {
/** 线程安全的 inline 缓存：C++17 保证 inline 变量在所有 TU 共享同一实例。 */
inline juce::CriticalSection& rootCacheLock()
{
    static juce::CriticalSection l;
    return l;
}
inline juce::File& rootCache()
{
    static juce::File cached;
    return cached;
}
inline bool& rootCacheValid()
{
    static bool valid = false;
    return valid;
}

inline juce::File computeProjectRoot()
{
    juce::File searchDir = juce::File::getSpecialLocation(
        juce::File::currentExecutableFile).getParentDirectory();
    for (int i = 0; i < 8; ++i)
    {
        if (searchDir.getChildFile("onnx").getChildFile("models").isDirectory())
            return searchDir;
        juce::File parent = searchDir.getParentDirectory();
        if (parent == searchDir)
            break;
        searchDir = parent;
    }
    juce::File cwdModels =
        juce::File::getCurrentWorkingDirectory().getChildFile("onnx").getChildFile("models");
    if (cwdModels.isDirectory())
        return juce::File::getCurrentWorkingDirectory();
    return juce::File::getCurrentWorkingDirectory();
}
} // namespace detail

/** 向上查找含 onnx/models 的应用工程根目录（结果缓存，避免重复磁盘探测）。 */
inline juce::File resolveProjectRootDirectory()
{
    juce::ScopedLock lock(detail::rootCacheLock());
    if (!detail::rootCacheValid())
    {
        detail::rootCache()      = detail::computeProjectRoot();
        detail::rootCacheValid() = true;
    }
    return detail::rootCache();
}

/** 若测试期间文件布局变化，可手动刷新根缓存。 */
inline void invalidateProjectRootCache()
{
    juce::ScopedLock lock(detail::rootCacheLock());
    detail::rootCacheValid() = false;
}

/** 返回全局 ONNX 模型目录（应用工程根下的 onnx/models） */
inline juce::File getOnnxModelsDirectory()
{
    return resolveProjectRootDirectory().getChildFile("onnx").getChildFile("models");
}

inline juce::File ensureDir(const juce::File& dir);

inline void writeTextIfMissing(const juce::File& file, const juce::String& text)
{
    if (file.existsAsFile())
        return;
    file.getParentDirectory().createDirectory();
    file.replaceWithText(text);
}

inline void ensureReadme(const juce::File& dir, const juce::String& text)
{
    ensureDir(dir);
    writeTextIfMissing(dir.getChildFile("README.md"), text);
}

inline juce::File getTrainingFilesDirectory()
{
    return ensureDir(resolveProjectRootDirectory().getChildFile("data").getChildFile("training_files"));
}

inline juce::File getProcessedNpzDirectory()
{
    return ensureDir(resolveProjectRootDirectory().getChildFile("data").getChildFile("processed_npz"));
}

inline juce::File getRuntimeDataDirectory()
{
    return ensureDir(resolveProjectRootDirectory().getChildFile("onnx").getChildFile("runtime_data"));
}

inline juce::File getRuntimePackagesDirectory()
{
    return ensureDir(resolveProjectRootDirectory().getChildFile("onnx").getChildFile("runtime_packages"));
}

inline juce::File getReportsDirectory()
{
    return ensureDir(resolveProjectRootDirectory().getChildFile("reports"));
}

inline void ensureApplicationResourceStructure(const juce::File& appRoot = resolveProjectRootDirectory())
{
    ensureReadme(appRoot.getChildFile("data").getChildFile("training_files"),
                 juce::String(L"# Training Files\n\nPlace EEG and neurophysiology training files here. Supported production input should be normalized to NPZ before training.\n"));
    ensureReadme(appRoot.getChildFile("data").getChildFile("raw_files"),
                 juce::String(L"# Raw Signal Files\n\nOptional staging area for original EEG/neurophysiology files before conversion.\n"));
    ensureReadme(appRoot.getChildFile("data").getChildFile("processed_npz"),
                 juce::String(L"# Processed NPZ\n\nPreprocessed classification datasets used by training. Expected shape: N x C x T plus labels.\n"));
    ensureReadme(appRoot.getChildFile("data").getChildFile("samples"),
                 juce::String(L"# Samples\n\nSmall demonstration or golden input files for validation.\n"));
    ensureReadme(appRoot.getChildFile("data").getChildFile("cache"),
                 juce::String(L"# Cache\n\nDownload and conversion cache. Files here are reproducible and not production artifacts.\n"));

    ensureReadme(appRoot.getChildFile("onnx").getChildFile("models"),
                 juce::String(L"# ONNX Models\n\nTrained classification model artifacts. Each model directory should contain model.onnx, manifest.json, labels.json and optional train_summary.json.\n"));
    ensureReadme(appRoot.getChildFile("onnx").getChildFile("deploy"),
                 juce::String(L"# Deploy\n\nStaged deployment outputs copied from validated model artifacts.\n"));
    ensureReadme(appRoot.getChildFile("onnx").getChildFile("runtime_data"),
                 juce::String(L"# ONNX Runtime DATA\n\nProduction inference packages generated from trained ONNX models, labels, manifest and golden samples.\n"));
    ensureReadme(appRoot.getChildFile("onnx").getChildFile("runtime_packages"),
                 juce::String(L"# Runtime Packages\n\nVersioned, validated production packages ready for ONNX Runtime deployment.\n"));
    ensureReadme(appRoot.getChildFile("onnx").getChildFile("templates"),
                 juce::String(L"# Model Templates\n\nReusable model template manifests and export profiles.\n"));

    ensureReadme(appRoot.getChildFile("reports").getChildFile("training"),
                 juce::String(L"# Training Reports\n\nTraining summaries, metrics and preflight reports.\n"));
    ensureReadme(appRoot.getChildFile("reports").getChildFile("validation"),
                 juce::String(L"# Validation Reports\n\nONNX Runtime validation and golden sample reports.\n"));
    ensureReadme(appRoot.getChildFile("logs").getChildFile("training"),
                 juce::String(L"# Training Logs\n\nClassifier training process logs.\n"));
    ensureReadme(appRoot.getChildFile("logs").getChildFile("inference"),
                 juce::String(L"# Inference Logs\n\nONNX Runtime validation and inference logs.\n"));
    ensureReadme(appRoot.getChildFile("logs").getChildFile("runtime_data"),
                 juce::String(L"# Runtime DATA Logs\n\nRuntime DATA export logs.\n"));

    ensureDir(appRoot.getChildFile("projects"));

    writeTextIfMissing(appRoot.getChildFile("resource_layout.json"),
        juce::String(L"{\n")
        + L"  \"version\": 1,\n"
        + L"  \"product\": \"NerouRuntime\",\n"
        + L"  \"positioning\": \"EEG and neurophysiology training-file classification platform\",\n"
        + L"  \"flow\": [\"training_files\", \"processed_npz\", \"training\", \"onnx\", \"runtime_data\", \"validation\"],\n"
        + L"  \"application_dirs\": {\n"
        + L"    \"training_files\": \"data/training_files\",\n"
        + L"    \"processed_npz\": \"data/processed_npz\",\n"
        + L"    \"onnx_models\": \"onnx/models\",\n"
        + L"    \"runtime_data\": \"onnx/runtime_data\",\n"
        + L"    \"reports\": \"reports\",\n"
        + L"    \"logs\": \"logs\"\n"
        + L"  }\n"
        + L"}\n");
}

/** 返回用户项目集根目录（应用工程根下的 projects/），不存在则创建 */
inline juce::File getProjectsBaseDir()
{
    auto dir = resolveProjectRootDirectory().getChildFile("projects");
    if (!dir.exists())
        dir.createDirectory();
    return dir;
}

/** @deprecated 请使用 getProjectsBaseDir() */
inline juce::File getProjectDir() { return getProjectsBaseDir(); }

/** 在工程根及 CWD 下查找相对路径文件 */
inline juce::File resolveFile(const juce::String& relativePath)
{
    juce::File searchDir = juce::File::getSpecialLocation(
        juce::File::currentExecutableFile).getParentDirectory();
    for (int i = 0; i < 8; ++i)
    {
        juce::File candidate = searchDir.getChildFile(relativePath);
        if (candidate.existsAsFile())
            return candidate;
        juce::File parent = searchDir.getParentDirectory();
        if (parent == searchDir)
            break;
        searchDir = parent;
    }
    return juce::File::getCurrentWorkingDirectory().getChildFile(relativePath);
}

// ============================================================================
// 用户项目目录（Project Root）— 标准子目录
// ============================================================================

/** 确保 dir 存在并返回，若创建失败则返回原值（不抛异常） */
inline juce::File ensureDir(const juce::File& dir)
{
    if (!dir.exists())
        dir.createDirectory();
    return dir;
}

/** sessions/ — 采集会话目录（每个 session 是子目录，含 session.json + NPZ） */
inline juce::File getSessionsDir(const juce::File& projectRoot)
{
    return ensureDir(projectRoot.getChildFile("sessions"));
}

/** 单次采集会话的子目录 */
inline juce::File getSessionDir(const juce::File& projectRoot, const juce::String& sessionId)
{
    return ensureDir(getSessionsDir(projectRoot).getChildFile(sessionId));
}

/** recordings/ — 录制产物目录（recording.json + .npz 原始文件） */
inline juce::File getRecordingsDir(const juce::File& projectRoot)
{
    return ensureDir(projectRoot.getChildFile("recordings"));
}

inline juce::File getTrainingFilesDir(const juce::File& projectRoot)
{
    return ensureDir(projectRoot.getChildFile("training_files"));
}

/** datasets/ — 预处理数据集目录（dataset_summary.json + processed .npz） */
inline juce::File getDatasetsDir(const juce::File& projectRoot)
{
    return ensureDir(projectRoot.getChildFile("datasets"));
}

inline juce::File getPreprocessingDir(const juce::File& projectRoot)
{
    return ensureDir(projectRoot.getChildFile("preprocessing"));
}

/** models/ — 模型产物目录（manifest.json + .onnx + labels.json） */
inline juce::File getModelsDir(const juce::File& projectRoot)
{
    return ensureDir(projectRoot.getChildFile("models"));
}

inline juce::File getExportsDir(const juce::File& projectRoot)
{
    return ensureDir(projectRoot.getChildFile("exports"));
}

inline juce::File getTrainingRunsDir(const juce::File& projectRoot)
{
    return ensureDir(projectRoot.getChildFile("training_runs"));
}

inline juce::File getProjectRuntimeDataDir(const juce::File& projectRoot)
{
    return ensureDir(projectRoot.getChildFile("runtime_data"));
}

inline juce::File getRuntimePackagesDir(const juce::File& projectRoot)
{
    return ensureDir(projectRoot.getChildFile("runtime_packages"));
}

inline juce::File getProjectReportsDir(const juce::File& projectRoot)
{
    return ensureDir(projectRoot.getChildFile("reports"));
}

/** validations/ — 验证结果目录（validation_result.json） */
inline juce::File getValidationsDir(const juce::File& projectRoot)
{
    return ensureDir(projectRoot.getChildFile("validations"));
}

/** logs/ — 训练日志及运行日志目录 */
inline juce::File getLogsDir(const juce::File& projectRoot)
{
    return ensureDir(projectRoot.getChildFile("logs"));
}

/**
 * 一键创建项目完整目录结构。
 * 应在 createProject() 时调用一次。
 */
inline void ensureProjectStructure(const juce::File& projectRoot)
{
    ensureReadme(projectRoot,
                 juce::String(L"# NerouRuntime Project\n\nProject-local resources for training-file classification, ONNX export, validation and Runtime DATA delivery.\n"));
    getTrainingFilesDir(projectRoot);
    getSessionsDir(projectRoot);
    getRecordingsDir(projectRoot);
    getDatasetsDir(projectRoot);
    getPreprocessingDir(projectRoot);
    getTrainingRunsDir(projectRoot);
    getModelsDir(projectRoot);
    getExportsDir(projectRoot);
    getValidationsDir(projectRoot);
    getProjectRuntimeDataDir(projectRoot);
    getRuntimePackagesDir(projectRoot);
    getProjectReportsDir(projectRoot);
    getLogsDir(projectRoot);

    ensureReadme(projectRoot.getChildFile("training_files"),
                 juce::String(L"# Training Files\n\nProject-specific EEG/neurophysiology training files.\n"));
    ensureReadme(projectRoot.getChildFile("datasets"),
                 juce::String(L"# Datasets\n\nPreprocessed NPZ datasets and dataset_summary.json.\n"));
    ensureReadme(projectRoot.getChildFile("preprocessing"),
                 juce::String(L"# Preprocessing\n\nMNE-Python preprocessing recipes, label maps and generated preprocessing.json files.\n"));
    ensureReadme(projectRoot.getChildFile("training_runs"),
                 juce::String(L"# Training Runs\n\nPer-run configuration, metrics and train_summary.json.\n"));
    ensureReadme(projectRoot.getChildFile("models"),
                 juce::String(L"# Models\n\nONNX model artifacts: model.onnx, manifest.json, labels.json.\n"));
    ensureReadme(projectRoot.getChildFile("exports"),
                 juce::String(L"# ONNX Exports\n\nPer-run ONNX export outputs with dummy input and dynamic batch manifest.\n"));
    ensureReadme(projectRoot.getChildFile("runtime_data"),
                 juce::String(L"# Runtime DATA\n\nValidated production inference packages for ONNX Runtime deployment.\n"));
    ensureReadme(projectRoot.getChildFile("runtime_packages"),
                 juce::String(L"# Runtime Packages\n\nFinal deployable Runtime DATA package versions.\n"));
    ensureReadme(projectRoot.getChildFile("validations"),
                 juce::String(L"# Validations\n\nONNX Runtime validation outputs and golden sample checks.\n"));
    ensureReadme(projectRoot.getChildFile("reports"),
                 juce::String(L"# Reports\n\nHuman-readable training, validation and delivery reports.\n"));
    ensureReadme(projectRoot.getChildFile("logs"),
                 juce::String(L"# Logs\n\nProject-local process logs.\n"));
}

} // namespace ProjectPaths
