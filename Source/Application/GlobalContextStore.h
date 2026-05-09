#pragma once

#include <JuceHeader.h>
#include <optional>
#include "../Core/ProjectPaths.h"
#include "../Domain/Entities.h"

namespace nerou::app {

/**
 * 全局上下文状态管理
 * 
 * 管理应用的核心业务对象状态:
 * - 当前项目
 * - 当前受试者
 * - 设备连接状态
 * - 当前模型
 * - 最近数据集
 * 
 * 支持持久化、状态监听、智能联动
 */

// ============================================================================
// 数据模型定义
// ============================================================================

struct SubjectInfo
{
    juce::String id;           // 受试者ID
    juce::String name;         // 姓名
    juce::String notes;        // 备注
    int sessionCount = 0;      // 已采集会话数
    juce::Time lastSession;    // 上次采集时间
    
    bool isValid() const { return !id.isEmpty(); }
    juce::String getDisplayName() const { return name.isEmpty() ? id : name; }
};

struct ProjectInfo
{
    juce::String id;           // 项目ID
    juce::String name;         // 项目名称
    juce::String description;  // 描述
    juce::File rootPath;       // 项目根目录
    juce::Time created;        // 创建时间
    juce::Time lastModified;   // 最后修改
    
    int subjectCount = 0;      // 受试者数量
    int datasetCount = 0;      // 数据集数量
    int modelCount = 0;        // 模型数量
    
    bool isValid() const { return !id.isEmpty(); }
    juce::String getDataPath() const { return rootPath.getChildFile("data").getFullPathName(); }
    juce::String getModelPath() const { return rootPath.getChildFile("models").getFullPathName(); }
    juce::String getExportPath() const { return rootPath.getChildFile("exports").getFullPathName(); }
};

// ============================================================================
// 每通道阻抗数据
// ============================================================================
struct ChannelImpedance
{
    enum class Quality { Unknown, Good, Acceptable, Poor, Disconnected };

    int    channelIndex = 0;       // 通道索引
    juce::String channelName;      // 通道名（Fp1, Fz 等）
    double impedanceKOhm = -1.0;   // 阻抗值（kΩ），-1 = 未测量
    Quality quality = Quality::Unknown;

    static Quality classifyImpedance(double kOhm)
    {
        if (kOhm < 0.0)   return Quality::Unknown;
        if (kOhm > 500.0) return Quality::Disconnected;
        if (kOhm > 100.0) return Quality::Poor;
        if (kOhm > 30.0)  return Quality::Acceptable; // ADS1299 常用目标阻抗 < 30kΩ
        return Quality::Good;
    }

    static juce::Colour qualityColor(Quality q)
    {
        switch (q)
        {
        case Quality::Good:         return juce::Colour(0xFF07C160); // 绿
        case Quality::Acceptable:   return juce::Colour(0xFFFA8C16); // 橙
        case Quality::Poor:         return juce::Colour(0xFFFF4D4F); // 红
        case Quality::Disconnected: return juce::Colour(0xFFBBBBBB); // 灰
        default:                    return juce::Colour(0xFF999999);
        }
    }

    juce::Colour getColor() const { return qualityColor(quality); }
    juce::String getLabel() const
    {
        if (impedanceKOhm < 0.0) return "--";
        return juce::String(impedanceKOhm, 1) + " k\u03a9";
    }
};

struct DeviceState
{
    enum class Status {
        Disconnected,   // 未连接
        Connecting,     // 连接中
        Connected,      // 已连接
        Acquiring,      // 采集中
        Error           // 错误
    };
    
    Status status = Status::Disconnected;
    juce::String deviceType;     // 设备类型
    juce::String connectionInfo; // 连接信息(IP/端口)
    int channelCount = 0;        // 通道数
    double sampleRate = 0.0;     // 采样率
    juce::String montageType;    // 导联系统

    // 实时采集统计
    double dataRateKBps = 0.0;     // 数据率 (KB/s)
    int    droppedPackets = 0;     // 丢包数
    double overallSignalQuality = 1.0; // 整体信号质量 0-1
    
    juce::Time connectedSince;   // 连接时间
    juce::Time acquiringSince;   // 采集开始时间

    // 每通道阻抗（测量后填充）
    juce::Array<ChannelImpedance> channelImpedances;
    bool impedanceMeasured = false;  // 是否已完成阻抗测量
    juce::Time lastImpedanceMeasurement;

    bool isValid() const { return status != Status::Disconnected && 
                                  status != Status::Error; }
    juce::String getStatusText() const;
    juce::Colour getStatusColor() const;

    // 获取导联名称列表（按导联系统和通道数生成）
    juce::StringArray getChannelNames() const;
};

struct ModelInfo
{
    juce::String id;           // 模型ID
    juce::String name;         // 模型名称
    juce::String version;      // 版本
    juce::File onnxPath;       // ONNX文件路径
    juce::File manifestPath;   // manifest路径
    
    int inputChannels = 0;     // 输入通道数
    int inputSamples = 0;      // 输入采样点数
    int outputClasses = 0;     // 输出类别数
    juce::StringArray classNames; // 类别名称
    
    juce::Time created;        // 创建时间
    juce::Time lastUsed;       // 最后使用时间
    bool isValidated = false;  // 是否已验证
    
    bool isValid() const { return !id.isEmpty() && onnxPath.exists(); }
    juce::String getInputShape() const { 
        return "[" + juce::String(inputChannels) + ", " + juce::String(inputSamples) + "]"; 
    }
};

struct DatasetInfo
{
    juce::String id;           // 数据集ID
    juce::String name;         // 名称
    juce::File path;           // 路径
    int sampleCount = 0;       // 样本数
    int channelCount = 0;      // 通道数
    double duration = 0.0;     // 总时长(秒)
    
    juce::Time created;        // 创建时间
    juce::Time lastUsed;       // 最后使用时间
    bool isProcessed = false;  // 是否已预处理
    
    bool isValid() const { return !id.isEmpty() && path.exists(); }
};

struct TaskProgress
{
    domain::TaskState state = domain::TaskState::Idle;
    juce::String taskName;
    double progress = 0.0;     // 0-1
    juce::String statusMessage;
    juce::Time startTime;
    juce::Time estimatedEnd;   // 预计完成时间
    
    bool isActive() const { return domain::isActiveTaskState(state); }
    juce::String getStateText() const;
};

// ============================================================================
// 全局上下文存储
// ============================================================================

class GlobalContextStore : public juce::ChangeBroadcaster
{
public:
    static GlobalContextStore& getInstance() noexcept
    {
        static GlobalContextStore instance;
        return instance;
    }
    
    // ========== 项目操作 ==========
    void setCurrentProject(const ProjectInfo& project);
    const ProjectInfo& getCurrentProject() const noexcept { return currentProject; }
    bool hasCurrentProject() const noexcept { return currentProject.isValid(); }
    
    /**
     * 安全切换项目：如果有活跃任务（训练/预处理），返回 false 并拒绝切换。
     * UI 层应调用此方法而非 setCurrentProject 以防止数据丢失。
     */
    bool safeSetCurrentProject(const ProjectInfo& project);

    void createProject(const juce::String& name, const juce::String& description = "");
    void loadProject(const juce::File& path);
    void closeProject();
    
    // 最近项目列表
    juce::Array<ProjectInfo> getRecentProjects(int maxCount = 5) const;
    void addToRecentProjects(const ProjectInfo& project);
    
    // ========== 受试者操作 ==========
    void setCurrentSubject(const SubjectInfo& subject);
    const SubjectInfo& getCurrentSubject() const noexcept { return currentSubject; }
    bool hasCurrentSubject() const noexcept { return currentSubject.isValid(); }
    void clearCurrentSubject();
    
    juce::Array<SubjectInfo> getProjectSubjects() const;
    void addSubject(const SubjectInfo& subject);
    
    // ========== 设备状态 ==========
    void setDeviceState(const DeviceState& state);
    const DeviceState& getDeviceState() const noexcept { return deviceState; }
    bool isDeviceConnected() const noexcept { return deviceState.isValid(); }
    bool isAcquiring() const noexcept { return deviceState.status == DeviceState::Status::Acquiring; }
    
    void updateDeviceStatus(DeviceState::Status newStatus);
    void setDeviceParameters(int channels, double sampleRate, const juce::String& montage);

    // 阻抗测量管理
    void beginImpedanceMeasurement();
    void updateChannelImpedance(int channelIndex, double kOhm);
    void finalizeImpedanceMeasurement();
    const juce::Array<ChannelImpedance>& getChannelImpedances() const noexcept { return deviceState.channelImpedances; }
    bool hasImpedanceData() const noexcept { return deviceState.impedanceMeasured; }

    // 实时信号质量更新
    void updateSignalQuality(double overallQuality, double dataRateKBps = 0.0, int droppedPackets = 0);
    
    // ========== 模型操作 ==========
    void setCurrentModel(const ModelInfo& model);
    const ModelInfo& getCurrentModel() const noexcept { return currentModel; }
    bool hasCurrentModel() const noexcept { return currentModel.isValid(); }
    void clearCurrentModel();
    
    juce::Array<ModelInfo> getProjectModels() const;
    void addModel(const ModelInfo& model);
    void updateModelValidation(const juce::String& modelId, bool validated);
    
    // 查找匹配当前设备参数的模型
    juce::Array<ModelInfo> getCompatibleModels() const;
    bool isModelCompatible(const ModelInfo& model) const;
    
    // ========== 数据集操作 ==========
    void setCurrentDataset(const DatasetInfo& dataset);
    const DatasetInfo& getCurrentDataset() const noexcept { return currentDataset; }
    bool hasCurrentDataset() const noexcept { return currentDataset.isValid(); }
    void clearCurrentDataset();
    
    juce::Array<DatasetInfo> getRecentDatasets(int maxCount = 5) const;
    void addToRecentDatasets(const DatasetInfo& dataset);

    // ========== 当前采集会话 ==========
    void setCurrentSession(const domain::Session& session);
    const domain::Session* getCurrentSession() const noexcept;
    bool hasCurrentSession() const noexcept;
    void clearCurrentSession();

    // ========== 最近录制历史 (最多保留 kMaxRecentRecordings 条) ==========
    static constexpr int kMaxRecentRecordings = 10;

    /** 服务层采集完成后调用此方法，将 Recording 推入历史并通知监听者 */
    void addRecentRecording(const domain::Recording& rec);
    juce::Array<domain::Recording> getRecentRecordings(int maxCount = 5) const;
    bool hasRecentRecordings() const noexcept { return !recentRecordings.isEmpty(); }

    // ========== 最近验证结果历史 (最多保留 kMaxRecentValidations 条) ==========
    static constexpr int kMaxRecentValidations = 10;

    /** 验证服务完成后调用此方法 */
    void addRecentValidationResult(const domain::ValidationResult& result);
    juce::Array<domain::ValidationResult> getRecentValidationResults(int maxCount = 5) const;
    bool hasRecentValidationResults() const noexcept { return !recentValidationResults.isEmpty(); }

    // ========== 服务层结果回填 (Service → Context 桥接) ==========
    /**
     * 供 TrainingService doneCb 调用：
     * 将 ModelArtifact 转换为 ModelInfo 并更新 currentModel。
     */
    void applyModelArtifact(const domain::ModelArtifact& artifact);

    /**
     * 供 DatasetPreparationService doneCb 调用：
     * 将 ProcessedDataset 转换为 DatasetInfo 并更新 currentDataset。
     */
    void applyProcessedDataset(const domain::ProcessedDataset& dataset);

    /**
     * 供 ValidationService doneCb 调用：
     * 更新 currentModel.isValidated 并追加验证历史。
     */
    void applyValidationResult(const domain::ValidationResult& result);

    // ========== 任务进度 ==========
    void startTask(const juce::String& taskName);
    void updateTaskProgress(double progress, const juce::String& status = "");
    void pauseTask();
    void resumeTask();
    void completeTask(const juce::String& message = "");
    void failTask(const juce::String& error);
    void cancelTask();
    
    const TaskProgress& getCurrentTask() const noexcept { return currentTask; }
    bool hasActiveTask() const noexcept { return currentTask.isActive(); }
    
    // ========== 智能联动 ==========
    
    /**
     * 获取"智能下一步"建议
     * 根据当前状态推荐用户下一步操作
     */
    struct NextAction
    {
        juce::String actionId;       // 动作ID
        juce::String title;          // 标题
        juce::String description;    // 描述
        juce::String buttonText;     // 按钮文字
        int priority = 0;            // 优先级(高优先显示)
        bool isHighlight = false;    // 是否高亮
    };
    
    juce::Array<NextAction> getSuggestedNextActions(int maxCount = 3) const;
    
    /**
     * 检查配置一致性
     * 例如：模型输入形状与设备通道/采样率是否匹配
     */
    struct AlignmentCheck
    {
        bool isAligned = true;
        juce::String mismatchDescription;
        juce::Array<juce::String> suggestions;
    };
    
    AlignmentCheck checkDeviceModelAlignment() const;
    AlignmentCheck checkDatasetModelAlignment() const;
    
    // ========== 持久化 ==========
    void saveToProperties(juce::PropertiesFile& props);
    void loadFromProperties(const juce::PropertiesFile& props);
    
    // ========== 事件监听 ==========
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void onProjectChanged(const ProjectInfo& newProject) {}
        virtual void onSubjectChanged(const SubjectInfo& newSubject) {}
        virtual void onDeviceStateChanged(const DeviceState& newState) {}
        virtual void onModelChanged(const ModelInfo& newModel) {}
        virtual void onDatasetChanged(const DatasetInfo& newDataset) {}
        virtual void onTaskProgressChanged(const TaskProgress& progress) {}
        /** 新录制完成时触发（来自 AcquisitionService doneCb） */
        virtual void onRecordingAdded(const domain::Recording& rec) {}
        /** 新验证结果完成时触发（来自 ValidationService doneCb） */
        virtual void onValidationResultAdded(const domain::ValidationResult& result) {}
        virtual void onContextChanged() {} // 通用变更通知
    };
    
    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    GlobalContextStore() = default;
    ~GlobalContextStore() override = default;
    GlobalContextStore(const GlobalContextStore&) = delete;
    GlobalContextStore& operator=(const GlobalContextStore&) = delete;
    
    void notifyListeners(void (juce::ChangeBroadcaster::*notification)());
    void notifyContextChanged();
    
    // 核心状态
    ProjectInfo currentProject;
    SubjectInfo currentSubject;
    DeviceState deviceState;
    ModelInfo currentModel;
    DatasetInfo currentDataset;
    TaskProgress currentTask;

    // 当前采集会话（领域对象）
    std::optional<domain::Session> currentSession;

    // 历史记录
    juce::Array<ProjectInfo> recentProjects;
    juce::Array<DatasetInfo> recentDatasets;
    juce::Array<SubjectInfo> projectSubjects;
    juce::Array<ModelInfo> projectModels;
    juce::StringArray       projectDatasetKeys;
    juce::StringArray       projectModelKeys;

    // 最近录制 / 验证历史（领域对象）
    juce::Array<domain::Recording>         recentRecordings;
    juce::Array<domain::ValidationResult>  recentValidationResults;
    
    juce::ListenerList<Listener> listeners;
    juce::CriticalSection stateLock;
};

// ============================================================================
// 便捷访问函数
// ============================================================================

inline GlobalContextStore& Context() {
    return GlobalContextStore::getInstance();
}

} // namespace nerou::app

/**
 * 使用示例:
 * 
 * // 设置当前项目
 * Context().setCurrentProject(project);
 * 
 * // 获取设备状态
 * if (Context().isDeviceConnected()) {
 *     auto& device = Context().getDeviceState();
 *     int channels = device.channelCount;
 * }
 * 
 * // 获取智能建议
 * auto actions = Context().getSuggestedNextActions();
 * for (auto& action : actions) {
 *     // 显示建议按钮
 * }
 * 
 * // 监听变更
 * class MyComponent : public GlobalContextStore::Listener {
 *     void onDeviceStateChanged(const DeviceState& state) override {
 *         // 更新UI
 *     }
 * };
 */
