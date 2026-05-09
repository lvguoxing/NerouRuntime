#include "GlobalContextStore.h"
#include "../Core/JsonFileIO.h"
#include "../Core/JsonVarHelpers.h"
#include "../UI/Theme/DesignTokenStore.h"
#include <memory>

namespace nerou::app {

namespace {

juce::File getProjectMetadataFile(const juce::File& projectRoot)
{
    return projectRoot.getChildFile("project.json");
}

juce::File getProjectMetadataFile(const ProjectInfo& project)
{
    return getProjectMetadataFile(project.rootPath);
}

void loadStringArrayProperty(const juce::var& root, const juce::Identifier& key, juce::StringArray& target)
{
    target.clear();

    if (auto* arr = root.getProperty(key, juce::var()).getArray())
    {
        for (const auto& item : *arr)
        {
            const auto value = item.toString().trim();
            if (value.isNotEmpty() && !target.contains(value))
                target.add(value);
        }
    }
}

juce::var projectToVar(const ProjectInfo& project,
                       const juce::StringArray& datasetKeys,
                       const juce::StringArray& modelKeys)
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty("project_id", project.id);
    obj->setProperty("project_name", project.name);
    obj->setProperty("description", project.description);
    obj->setProperty("root_path", project.rootPath.getFullPathName());
    obj->setProperty("created_at_ms", (double) project.created.toMilliseconds());
    obj->setProperty("updated_at_ms", (double) project.lastModified.toMilliseconds());
    obj->setProperty("subject_count", project.subjectCount);
    obj->setProperty("dataset_count", juce::jmax(project.datasetCount, datasetKeys.size()));
    obj->setProperty("model_count", juce::jmax(project.modelCount, modelKeys.size()));
    obj->setProperty("dataset_keys", nerou::core::stringArrayToVar(datasetKeys));
    obj->setProperty("model_keys", nerou::core::stringArrayToVar(modelKeys));
    obj->setProperty("status", "active");
    return juce::var(obj.release());
}

bool loadProjectMetadata(const juce::File& rootPath, ProjectInfo& project)
{
    const auto metadataFile = getProjectMetadataFile(rootPath);
    if (!metadataFile.existsAsFile())
        return false;

    const auto root = juce::JSON::parse(metadataFile.loadFileAsString());
    if (root.isVoid())
        return false;

    project.rootPath = rootPath;
    project.id = root.getProperty("project_id", rootPath.getFileName()).toString();
    project.name = root.getProperty("project_name", rootPath.getFileName()).toString();
    project.description = root.getProperty("description", {}).toString();
    project.subjectCount = int(root.getProperty("subject_count", 0));
    project.datasetCount = int(root.getProperty("dataset_count", 0));
    project.modelCount = int(root.getProperty("model_count", 0));

    const auto createdAtMs = (juce::int64) root.getProperty("created_at_ms", 0.0);
    const auto updatedAtMs = (juce::int64) root.getProperty("updated_at_ms", 0.0);
    project.created = createdAtMs > 0 ? juce::Time(createdAtMs) : rootPath.getCreationTime();
    project.lastModified = updatedAtMs > 0 ? juce::Time(updatedAtMs) : rootPath.getLastModificationTime();
    return project.isValid();
}

void loadProjectArtifactKeys(const juce::File& rootPath,
                             juce::StringArray& datasetKeys,
                             juce::StringArray& modelKeys)
{
    datasetKeys.clear();
    modelKeys.clear();

    const auto metadataFile = getProjectMetadataFile(rootPath);
    if (!metadataFile.existsAsFile())
        return;

    const auto root = juce::JSON::parse(metadataFile.loadFileAsString());
    if (root.isVoid())
        return;

    loadStringArrayProperty(root, "dataset_keys", datasetKeys);
    loadStringArrayProperty(root, "model_keys", modelKeys);
}

void saveProjectMetadata(const ProjectInfo& project,
                         const juce::StringArray& datasetKeys = {},
                         const juce::StringArray& modelKeys = {})
{
    if (!project.isValid())
        return;

    if (!project.rootPath.exists() && !project.rootPath.createDirectory().wasOk())
        return;

    nerou::core::writeJsonFile(getProjectMetadataFile(project),
                               projectToVar(project, datasetKeys, modelKeys));
}

juce::File getSubjectsIndexFile(const ProjectInfo& project)
{
    return project.rootPath.getChildFile("subjects.json");
}

juce::var subjectToVar(const SubjectInfo& subject)
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty("subject_id", subject.id);
    obj->setProperty("subject_name", subject.name);
    obj->setProperty("notes", subject.notes);
    obj->setProperty("session_count", subject.sessionCount);
    obj->setProperty("last_session_ms", (double) subject.lastSession.toMilliseconds());
    return juce::var(obj.release());
}

juce::Array<SubjectInfo> loadSubjectsForProject(const ProjectInfo& project)
{
    juce::Array<SubjectInfo> subjects;
    if (!project.isValid())
        return subjects;

    const auto indexFile = getSubjectsIndexFile(project);
    if (!indexFile.existsAsFile())
        return subjects;

    const auto root = juce::JSON::parse(indexFile.loadFileAsString());
    if (root.isVoid())
        return subjects;

    if (auto* arr = root.getProperty("subjects", juce::var()).getArray())
    {
        for (const auto& item : *arr)
        {
            SubjectInfo subject;
            subject.id = item.getProperty("subject_id", {}).toString();
            subject.name = item.getProperty("subject_name", {}).toString();
            subject.notes = item.getProperty("notes", {}).toString();
            subject.sessionCount = int(item.getProperty("session_count", 0));
            const auto lastSessionMs = (juce::int64) item.getProperty("last_session_ms", 0.0);
            if (lastSessionMs > 0)
                subject.lastSession = juce::Time(lastSessionMs);

            if (subject.isValid())
                subjects.add(subject);
        }
    }

    return subjects;
}

void saveSubjectsForProject(const ProjectInfo& project, const juce::Array<SubjectInfo>& subjects)
{
    if (!project.isValid())
        return;

    if (!project.rootPath.exists() && !project.rootPath.createDirectory().wasOk())
        return;

    juce::Array<juce::var> subjectVars;
    for (const auto& subject : subjects)
        subjectVars.add(subjectToVar(subject));

    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("project_id", project.id);
    root->setProperty("project_name", project.name);
    root->setProperty("subject_count", subjects.size());
    root->setProperty("subjects", juce::var(subjectVars));
    nerou::core::writeJsonFile(getSubjectsIndexFile(project), juce::var(root.release()));
}

bool containsSubjectId(const juce::Array<SubjectInfo>& subjects, const juce::String& subjectId)
{
    for (const auto& subject : subjects)
        if (subject.id == subjectId)
            return true;
    return false;
}

} // namespace

// ============================================================================
// DeviceState 辅助方法
// ============================================================================

juce::String DeviceState::getStatusText() const
{
    switch (status)
    {
        case Status::Disconnected: return "未连接";
        case Status::Connecting: return "连接中...";
        case Status::Connected: return "已连接";
        case Status::Acquiring: return "采集中";
        case Status::Error: return "连接错误";
        default: return "未知";
    }
}

juce::Colour DeviceState::getStatusColor() const
{
    auto& tokens = ui::DesignTokenStore::getInstance();
    
    switch (status)
    {
        case Status::Disconnected: return tokens.getColors().statusIdle;
        case Status::Connecting: return tokens.getColors().statusInfo;
        case Status::Connected: return tokens.getColors().statusSuccess;
        case Status::Acquiring: return tokens.getColors().statusRunning;
        case Status::Error: return tokens.getColors().statusError;
        default: return tokens.getColors().statusIdle;
    }
}

// ============================================================================
// 导联名称表（按 10-20 / 10-10 / 10-5 系统和通道数）
// ============================================================================

juce::StringArray DeviceState::getChannelNames() const
{
    // 10-20 系统基础 19 通道
    static const char* names1020_19[] = {
        "Fp1","Fp2","F7","F3","Fz","F4","F8",
        "T3","C3","Cz","C4","T4",
        "T5","P3","Pz","P4","T6",
        "O1","O2"
    };
    // 10-10 系统 64 通道（常用子集）
    static const char* names1010_64[] = {
        "Fp1","Fpz","Fp2","AF7","AF3","AFz","AF4","AF8",
        "F7","F5","F3","F1","Fz","F2","F4","F6",
        "F8","FT7","FC5","FC3","FC1","FCz","FC2","FC4",
        "FC6","FT8","T7","C5","C3","C1","Cz","C2",
        "C4","C6","T8","TP7","CP5","CP3","CP1","CPz",
        "CP2","CP4","CP6","TP8","P7","P5","P3","P1",
        "Pz","P2","P4","P6","P8","PO7","PO3","POz",
        "PO4","PO8","O1","Oz","O2","Iz","A1","A2"
    };

    juce::StringArray result;
    bool is1010 = montageType.contains("10-10") || montageType.contains("10-5");

    int count = juce::jmin(channelCount, is1010 ? 64 : 19);
    for (int i = 0; i < count; ++i)
    {
        if (is1010)
            result.add(names1010_64[i]);
        else if (i < 19)
            result.add(names1020_19[i]);
        else
            result.add("Ch" + juce::String(i + 1));
    }
    // 补足
    while (result.size() < channelCount)
    {
        const int idx = result.size() + 1;
        if (is1010 && idx > 64)
            result.add("E" + juce::String(idx)); // 64+ 导联使用扩展电极命名
        else
            result.add("Ch" + juce::String(idx));
    }

    return result;
}

// ============================================================================
// TaskProgress 辅助方法
// ============================================================================

juce::String TaskProgress::getStateText() const
{
    return domain::toLocalizedDisplayString(state);
}

// ============================================================================
// GlobalContextStore 实现
// ============================================================================

void GlobalContextStore::setCurrentProject(const ProjectInfo& project)
{
    juce::ScopedLock lock(stateLock);
    
    currentProject = project;
    
    // 项目变更时重置某些状态
    if (!project.isValid())
    {
        currentSubject = SubjectInfo();
        currentDataset = DatasetInfo();
        projectSubjects.clear();
        projectModels.clear();
        projectDatasetKeys.clear();
        projectModelKeys.clear();
    }
    else
    {
        loadProjectArtifactKeys(currentProject.rootPath, projectDatasetKeys, projectModelKeys);
        projectSubjects = loadSubjectsForProject(currentProject);
        currentProject.subjectCount = projectSubjects.size();
        currentProject.datasetCount = juce::jmax(currentProject.datasetCount, projectDatasetKeys.size());
        currentProject.modelCount = juce::jmax(currentProject.modelCount, projectModelKeys.size());
        if (currentSubject.isValid() && !containsSubjectId(projectSubjects, currentSubject.id))
            currentSubject = SubjectInfo();
        if (currentProject.lastModified == juce::Time())
            currentProject.lastModified = juce::Time::getCurrentTime();
        saveProjectMetadata(currentProject, projectDatasetKeys, projectModelKeys);
        addToRecentProjects(currentProject);
    }
    
    notifyContextChanged();
    listeners.call([this](Listener& l) { l.onProjectChanged(currentProject); });
}

bool GlobalContextStore::safeSetCurrentProject(const ProjectInfo& project)
{
    if (hasActiveTask())
    {
        juce::Logger::writeToLog(
            juce::String::fromUTF8(u8"[GlobalContext] Refused project switch: active task running"));
        return false;
    }
    setCurrentProject(project);
    return true;
}

void GlobalContextStore::createProject(const juce::String& name, const juce::String& description)
{
    ProjectInfo project;
    project.id = juce::String(juce::Time::currentTimeMillis());
    project.name = name;
    project.description = description;
    project.rootPath = ProjectPaths::getProjectDir().getChildFile(name);
    project.created = juce::Time::getCurrentTime();
    project.lastModified = project.created;
    
    // 创建标准项目目录结构
    project.rootPath.createDirectory();
    ProjectPaths::ensureProjectStructure(project.rootPath);
    
    setCurrentProject(project);
}

void GlobalContextStore::loadProject(const juce::File& path)
{
    // 从项目文件加载项目信息
    ProjectInfo project;
    if (!loadProjectMetadata(path, project))
    {
        project.rootPath = path;
        project.id = path.getFileName();
        project.name = path.getFileNameWithoutExtension();
        project.created = path.getCreationTime();
        project.lastModified = path.getLastModificationTime();
    }
    
    setCurrentProject(project);
}

void GlobalContextStore::closeProject()
{
    setCurrentProject(ProjectInfo());
}

juce::Array<ProjectInfo> GlobalContextStore::getRecentProjects(int maxCount) const
{
    juce::ScopedLock lock(stateLock);
    
    juce::Array<ProjectInfo> result;
    int count = juce::jmin(maxCount, recentProjects.size());
    
    for (int i = 0; i < count; ++i)
    {
        result.add(recentProjects[i]);
    }
    
    return result;
}

void GlobalContextStore::addToRecentProjects(const ProjectInfo& project)
{
    if (!project.isValid())
        return;
        
    // 移除重复项
    recentProjects.removeIf([&project](const ProjectInfo& p) {
        return p.id == project.id;
    });
    
    // 添加到开头
    recentProjects.insert(0, project);
    
    // 限制数量
    while (recentProjects.size() > 10)
    {
        recentProjects.removeLast();
    }
}

void GlobalContextStore::setCurrentSubject(const SubjectInfo& subject)
{
    juce::ScopedLock lock(stateLock);
    
    currentSubject = subject;
    if (subject.isValid() && currentProject.isValid())
    {
        bool updated = false;
        for (int i = 0; i < projectSubjects.size(); ++i)
        {
            if (projectSubjects[i].id == subject.id)
            {
                projectSubjects.set(i, subject);
                updated = true;
                break;
            }
        }

        if (!updated)
            projectSubjects.add(subject);

        currentProject.subjectCount = projectSubjects.size();
        currentProject.lastModified = juce::Time::getCurrentTime();
        saveSubjectsForProject(currentProject, projectSubjects);
        saveProjectMetadata(currentProject, projectDatasetKeys, projectModelKeys);
    }
    
    notifyContextChanged();
    listeners.call([this](Listener& l) { l.onSubjectChanged(currentSubject); });
}

void GlobalContextStore::clearCurrentSubject()
{
    setCurrentSubject(SubjectInfo());
}

juce::Array<SubjectInfo> GlobalContextStore::getProjectSubjects() const
{
    juce::ScopedLock lock(stateLock);
    return projectSubjects;
}

void GlobalContextStore::addSubject(const SubjectInfo& subject)
{
    juce::ScopedLock lock(stateLock);
    
    // 检查是否已存在
    for (int i = 0; i < projectSubjects.size(); ++i)
    {
        if (projectSubjects[i].id == subject.id)
        {
            projectSubjects.set(i, subject);
            if (currentProject.isValid())
            {
                currentProject.subjectCount = projectSubjects.size();
                currentProject.lastModified = juce::Time::getCurrentTime();
                saveSubjectsForProject(currentProject, projectSubjects);
                saveProjectMetadata(currentProject, projectDatasetKeys, projectModelKeys);
            }
            notifyContextChanged();
            return;
        }
    }
    
    projectSubjects.add(subject);
    
    if (currentProject.isValid())
    {
        currentProject.subjectCount = projectSubjects.size();
        currentProject.lastModified = juce::Time::getCurrentTime();
        saveSubjectsForProject(currentProject, projectSubjects);
        saveProjectMetadata(currentProject, projectDatasetKeys, projectModelKeys);
    }

    notifyContextChanged();
}

void GlobalContextStore::setDeviceState(const DeviceState& state)
{
    juce::ScopedLock lock(stateLock);
    
    deviceState = state;
    
    notifyContextChanged();
    listeners.call([this](Listener& l) { l.onDeviceStateChanged(deviceState); });
}

void GlobalContextStore::updateDeviceStatus(DeviceState::Status newStatus)
{
    juce::ScopedLock lock(stateLock);
    
    deviceState.status = newStatus;
    
    if (newStatus == DeviceState::Status::Connected)
    {
        deviceState.connectedSince = juce::Time::getCurrentTime();
    }
    else if (newStatus == DeviceState::Status::Acquiring)
    {
        deviceState.acquiringSince = juce::Time::getCurrentTime();
    }
    
    notifyContextChanged();
    listeners.call([this](Listener& l) { l.onDeviceStateChanged(deviceState); });
}

void GlobalContextStore::setDeviceParameters(int channels, double sampleRate, 
                                           const juce::String& montage)
{
    juce::ScopedLock lock(stateLock);
    
    deviceState.channelCount = channels;
    deviceState.sampleRate = sampleRate;
    deviceState.montageType = montage;

    // 重置阻抗数据（参数变更时）
    deviceState.channelImpedances.clear();
    deviceState.impedanceMeasured = false;
    
    notifyContextChanged();
}

// ============================================================================
// 阻抗测量
// ============================================================================

void GlobalContextStore::beginImpedanceMeasurement()
{
    juce::ScopedLock lock(stateLock);
    deviceState.impedanceMeasured = false;
    deviceState.channelImpedances.clear();

    // 预填充通道列表
    auto names = deviceState.getChannelNames();
    for (int i = 0; i < deviceState.channelCount; ++i)
    {
        ChannelImpedance ch;
        ch.channelIndex   = i;
        ch.channelName    = (i < names.size()) ? names[i] : ("Ch" + juce::String(i + 1));
        ch.impedanceKOhm  = -1.0;
        ch.quality        = ChannelImpedance::Quality::Unknown;
        deviceState.channelImpedances.add(ch);
    }
    notifyContextChanged();
    listeners.call([this](Listener& l) { l.onDeviceStateChanged(deviceState); });
}

void GlobalContextStore::updateChannelImpedance(int channelIndex, double kOhm)
{
    juce::ScopedLock lock(stateLock);
    for (auto& ch : deviceState.channelImpedances)
    {
        if (ch.channelIndex == channelIndex)
        {
            ch.impedanceKOhm = kOhm;
            ch.quality = ChannelImpedance::classifyImpedance(kOhm);
            break;
        }
    }
    // 轻量通知：只 repaint，不发完整事件
    notifyContextChanged();
}

void GlobalContextStore::finalizeImpedanceMeasurement()
{
    juce::ScopedLock lock(stateLock);
    deviceState.impedanceMeasured = true;
    deviceState.lastImpedanceMeasurement = juce::Time::getCurrentTime();
    notifyContextChanged();
    listeners.call([this](Listener& l) { l.onDeviceStateChanged(deviceState); });
}

void GlobalContextStore::updateSignalQuality(double overallQuality, double dataRateKBps, int droppedPackets)
{
    juce::ScopedLock lock(stateLock);
    deviceState.overallSignalQuality = juce::jlimit(0.0, 1.0, overallQuality);
    deviceState.dataRateKBps         = dataRateKBps;
    deviceState.droppedPackets       = droppedPackets;
    // 不广播完整事件，避免过多刷新
}

void GlobalContextStore::setCurrentModel(const ModelInfo& model)
{
    juce::ScopedLock lock(stateLock);
    
    currentModel = model;
    currentModel.lastUsed = juce::Time::getCurrentTime();
    
    addModel(model);
    
    notifyContextChanged();
    listeners.call([this](Listener& l) { l.onModelChanged(currentModel); });
}

void GlobalContextStore::clearCurrentModel()
{
    setCurrentModel(ModelInfo());
}

juce::Array<ModelInfo> GlobalContextStore::getProjectModels() const
{
    juce::ScopedLock lock(stateLock);
    return projectModels;
}

void GlobalContextStore::addModel(const ModelInfo& model)
{
    if (!model.isValid())
        return;
        
    juce::ScopedLock lock(stateLock);
    
    // 更新或添加
    for (int i = 0; i < projectModels.size(); ++i)
    {
        if (projectModels[i].id == model.id)
        {
            projectModels.set(i, model);
            return;
        }
    }
    
    projectModels.add(model);

    const auto modelKey = model.id.isNotEmpty() ? model.id : model.onnxPath.getFullPathName();
    if (modelKey.isNotEmpty() && !projectModelKeys.contains(modelKey))
        projectModelKeys.add(modelKey);
    
    if (currentProject.isValid())
    {
        currentProject.modelCount = juce::jmax(currentProject.modelCount, projectModelKeys.size());
        currentProject.lastModified = juce::Time::getCurrentTime();
        saveProjectMetadata(currentProject, projectDatasetKeys, projectModelKeys);
    }
}

void GlobalContextStore::updateModelValidation(const juce::String& modelId, bool validated)
{
    juce::ScopedLock lock(stateLock);
    
    for (auto& model : projectModels)
    {
        if (model.id == modelId)
        {
            model.isValidated = validated;
            if (currentModel.id == modelId)
            {
                currentModel.isValidated = validated;
            }
            break;
        }
    }
}

juce::Array<ModelInfo> GlobalContextStore::getCompatibleModels() const
{
    juce::ScopedLock lock(stateLock);
    
    juce::Array<ModelInfo> compatible;
    
    for (const auto& model : projectModels)
    {
        if (isModelCompatible(model))
        {
            compatible.add(model);
        }
    }
    
    return compatible;
}

bool GlobalContextStore::isModelCompatible(const ModelInfo& model) const
{
    if (!model.isValid())
        return false;
        
    // 检查输入通道数匹配
    if (deviceState.channelCount > 0 && 
        model.inputChannels != deviceState.channelCount)
    {
        return false;
    }
    
    return true;
}

void GlobalContextStore::setCurrentDataset(const DatasetInfo& dataset)
{
    juce::ScopedLock lock(stateLock);
    
    currentDataset = dataset;
    currentDataset.lastUsed = juce::Time::getCurrentTime();
    
    addToRecentDatasets(dataset);

    const auto datasetKey = currentDataset.id.isNotEmpty() ? currentDataset.id : currentDataset.path.getFullPathName();
    if (datasetKey.isNotEmpty() && !projectDatasetKeys.contains(datasetKey))
        projectDatasetKeys.add(datasetKey);

    if (currentProject.isValid())
    {
        currentProject.datasetCount = juce::jmax(currentProject.datasetCount, projectDatasetKeys.size());
        currentProject.lastModified = juce::Time::getCurrentTime();
        saveProjectMetadata(currentProject, projectDatasetKeys, projectModelKeys);
    }
    
    notifyContextChanged();
    listeners.call([this](Listener& l) { l.onDatasetChanged(currentDataset); });
}

void GlobalContextStore::clearCurrentDataset()
{
    setCurrentDataset(DatasetInfo());
}

juce::Array<DatasetInfo> GlobalContextStore::getRecentDatasets(int maxCount) const
{
    juce::ScopedLock lock(stateLock);
    
    juce::Array<DatasetInfo> result;
    int count = juce::jmin(maxCount, recentDatasets.size());
    
    for (int i = 0; i < count; ++i)
    {
        result.add(recentDatasets[i]);
    }
    
    return result;
}

void GlobalContextStore::addToRecentDatasets(const DatasetInfo& dataset)
{
    if (!dataset.isValid())
        return;
        
    // 移除重复项
    recentDatasets.removeIf([&dataset](const DatasetInfo& d) {
        return d.id == dataset.id;
    });
    
    // 添加到开头
    recentDatasets.insert(0, dataset);
    
    // 限制数量
    while (recentDatasets.size() > 10)
    {
        recentDatasets.removeLast();
    }
}

void GlobalContextStore::startTask(const juce::String& taskName)
{
    juce::ScopedLock lock(stateLock);
    
    currentTask.state = domain::TaskState::Running;
    currentTask.taskName = taskName;
    currentTask.progress = 0.0;
    currentTask.startTime = juce::Time::getCurrentTime();
    currentTask.statusMessage = "开始执行...";
    
    notifyContextChanged();
    listeners.call([this](Listener& l) { l.onTaskProgressChanged(currentTask); });
}

void GlobalContextStore::updateTaskProgress(double progress, const juce::String& status)
{
    juce::ScopedLock lock(stateLock);
    
    currentTask.progress = juce::jlimit(0.0, 1.0, progress);
    if (!status.isEmpty())
    {
        currentTask.statusMessage = status;
    }
    
    // 计算预计完成时间
    if (progress > 0.01)
    {
        auto elapsed = juce::Time::getCurrentTime() - currentTask.startTime;
        double totalEstimated = elapsed.inSeconds() / progress;
        double remaining = totalEstimated * (1.0 - progress);
        currentTask.estimatedEnd = juce::Time::getCurrentTime() + 
                                   juce::RelativeTime(remaining);
    }
    
    notifyContextChanged();
    listeners.call([this](Listener& l) { l.onTaskProgressChanged(currentTask); });
}

void GlobalContextStore::pauseTask()
{
    juce::ScopedLock lock(stateLock);
    
    if (currentTask.state == domain::TaskState::Running)
    {
        currentTask.state = domain::TaskState::Paused;
        currentTask.statusMessage = "已暂停";
        
        notifyContextChanged();
        listeners.call([this](Listener& l) { l.onTaskProgressChanged(currentTask); });
    }
}

void GlobalContextStore::resumeTask()
{
    juce::ScopedLock lock(stateLock);
    
    if (currentTask.state == domain::TaskState::Paused)
    {
        currentTask.state = domain::TaskState::Running;
        currentTask.statusMessage = "继续执行...";
        
        notifyContextChanged();
        listeners.call([this](Listener& l) { l.onTaskProgressChanged(currentTask); });
    }
}

void GlobalContextStore::completeTask(const juce::String& message)
{
    juce::ScopedLock lock(stateLock);
    
    currentTask.state = domain::TaskState::Completed;
    currentTask.progress = 1.0;
    currentTask.statusMessage = message.isEmpty() ? "已完成" : message;
    currentTask.estimatedEnd = juce::Time::getCurrentTime();
    
    notifyContextChanged();
    listeners.call([this](Listener& l) { l.onTaskProgressChanged(currentTask); });
}

void GlobalContextStore::failTask(const juce::String& error)
{
    juce::ScopedLock lock(stateLock);
    
    currentTask.state = domain::TaskState::Failed;
    currentTask.statusMessage = error;
    
    notifyContextChanged();
    listeners.call([this](Listener& l) { l.onTaskProgressChanged(currentTask); });
}

void GlobalContextStore::cancelTask()
{
    juce::ScopedLock lock(stateLock);
    
    currentTask.state = domain::TaskState::Cancelled;
    currentTask.progress = 0.0;
    currentTask.statusMessage = "已取消";
    
    notifyContextChanged();
    listeners.call([this](Listener& l) { l.onTaskProgressChanged(currentTask); });
}

juce::Array<GlobalContextStore::NextAction> GlobalContextStore::getSuggestedNextActions(int maxCount) const
{
    juce::Array<NextAction> actions;
    
    // 分析当前状态，生成智能建议
    
    // 1. 如果没有项目，建议创建项目
    if (!hasCurrentProject())
    {
        NextAction action;
        action.actionId = "create_project";
        action.title = juce::String(L"\u521b\u5efa\u65b0\u9879\u76ee");
        action.description = juce::String(L"\u5f00\u59cb\u4e00\u4e2a\u65b0\u7684\u8111\u7535\u5206\u6790\u9879\u76ee");
        action.buttonText = juce::String(L"\u521b\u5efa\u9879\u76ee");
        action.priority = 100;
        action.isHighlight = true;
        actions.add(action);
    }
    // 2. 如果有项目但还没有数据集，建议从文件导入（V3 文件优先方案）
    //    NPZ / NPY / CSV / TSV / EDF / BDF / MAT / SET / VHDR / FIF
    else if (!hasCurrentDataset())
    {
        NextAction action;
        action.actionId = "import_file";
        action.title = juce::String(L"\u5bfc\u5165\u8bad\u7ec3\u6587\u4ef6");
        action.description = juce::String(L"\u4ece NPZ / EDF / BDF / CSV \u7b49\u683c\u5f0f\u52a0\u8f7d\u5df2\u6709\u6570\u636e\u96c6");
        action.buttonText = juce::String(L"\u9009\u62e9\u6587\u4ef6");
        action.priority = 90;
        action.isHighlight = true;
        actions.add(action);
    }
    // 3. 如果有数据但未预处理，建议预处理
    else if (hasCurrentDataset() && !currentDataset.isProcessed)
    {
        NextAction action;
        action.actionId = "preprocess_data";
        action.title = juce::String(L"\u9884\u5904\u7406\u6570\u636e");
        action.description = juce::String(L"\u51c6\u5907\u8bad\u7ec3\u6570\u636e\u683c\u5f0f");
        action.buttonText = juce::String(L"\u9884\u5904\u7406");
        action.priority = 80;
        actions.add(action);
    }
    // 5. 如果有预处理数据但没有模型，建议训练
    else if (hasCurrentDataset() && currentDataset.isProcessed && !hasCurrentModel())
    {
        NextAction action;
        action.actionId = "train_model";
        action.title = juce::String(L"\u8bad\u7ec3\u6a21\u578b");
        action.description = juce::String(L"\u4f7f\u7528\u5f53\u524d\u6570\u636e\u96c6\u8bad\u7ec3 AI \u6a21\u578b");
        action.buttonText = juce::String(L"\u5f00\u59cb\u8bad\u7ec3");
        action.priority = 75;
        actions.add(action);
    }
    // 6. 如果有模型但未验证，建议验证
    else if (hasCurrentModel() && !currentModel.isValidated)
    {
        NextAction action;
        action.actionId = "validate_model";
        action.title = juce::String(L"\u9a8c\u8bc1\u6a21\u578b");
        action.description = juce::String(L"\u9a8c\u8bc1\u6a21\u578b\u6027\u80fd\u548c\u517c\u5bb9\u6027");
        action.buttonText = juce::String(L"\u9a8c\u8bc1");
        action.priority = 70;
        actions.add(action);
    }
    
    // 限制数量
    while (actions.size() > maxCount)
    {
        // 移除低优先级的
        int minPriorityIndex = 0;
        for (int i = 1; i < actions.size(); ++i)
        {
            if (actions[i].priority < actions[minPriorityIndex].priority)
            {
                minPriorityIndex = i;
            }
        }
        actions.remove(minPriorityIndex);
    }
    
    // 按优先级排序
    std::sort(actions.begin(), actions.end(), [](const NextAction& a, const NextAction& b) {
        return a.priority > b.priority;
    });
    
    return actions;
}

GlobalContextStore::AlignmentCheck GlobalContextStore::checkDeviceModelAlignment() const
{
    AlignmentCheck check;
    
    if (!isDeviceConnected() || !hasCurrentModel())
    {
        check.isAligned = true; // 没有对比对象时视为对齐
        return check;
    }
    
    if (deviceState.channelCount != currentModel.inputChannels)
    {
        check.isAligned = false;
        check.mismatchDescription = "设备通道数(" + juce::String(deviceState.channelCount) + 
                                   ")与模型输入通道数(" + juce::String(currentModel.inputChannels) + 
                                   ")不匹配";
        check.suggestions.add("更换为" + juce::String(deviceState.channelCount) + "通道的模型");
        check.suggestions.add("重新训练适配当前设备的模型");
    }
    
    return check;
}

GlobalContextStore::AlignmentCheck GlobalContextStore::checkDatasetModelAlignment() const
{
    AlignmentCheck check;
    
    if (!hasCurrentDataset() || !hasCurrentModel())
    {
        check.isAligned = true;
        return check;
    }
    
    if (currentDataset.channelCount != currentModel.inputChannels)
    {
        check.isAligned = false;
        check.mismatchDescription = "数据集通道数与模型输入通道数不匹配";
        check.suggestions.add("选择与模型通道数匹配的数据集");
        check.suggestions.add("重新预处理数据以匹配模型输入");
    }
    
    return check;
}

void GlobalContextStore::saveToProperties(juce::PropertiesFile& props)
{
    // 保存当前项目
    props.setValue("currentProjectId", currentProject.id);
    props.setValue("currentProjectPath", currentProject.rootPath.getFullPathName());
    props.setValue("currentSubjectId", currentSubject.id);
    props.setValue("currentSubjectName", currentSubject.name);
    props.setValue("currentSubjectNotes", currentSubject.notes);
    
    // 保存设备参数
    props.setValue("deviceChannels", deviceState.channelCount);
    props.setValue("deviceSampleRate", deviceState.sampleRate);
    props.setValue("deviceMontage", deviceState.montageType);
    
    // 保存最近项目
    for (int i = 0; i < juce::jmin(5, recentProjects.size()); ++i)
    {
        auto key = "recentProject_" + juce::String(i);
        props.setValue(key + "_id", recentProjects[i].id);
        props.setValue(key + "_name", recentProjects[i].name);
        props.setValue(key + "_path", recentProjects[i].rootPath.getFullPathName());
    }
    props.setValue("recentProjectCount", juce::jmin(5, recentProjects.size()));
}

void GlobalContextStore::loadFromProperties(const juce::PropertiesFile& props)
{
    // 加载当前项目
    auto projectId = props.getValue("currentProjectId");
    auto projectPath = props.getValue("currentProjectPath");
    
    if (!projectId.isEmpty() && !projectPath.isEmpty())
    {
        ProjectInfo project;
        if (!loadProjectMetadata(projectPath, project))
        {
            project.id = projectId;
            project.name = juce::File(projectPath).getFileNameWithoutExtension();
            project.rootPath = projectPath;
        }
        setCurrentProject(project);
    }

    auto subjectId = props.getValue("currentSubjectId");
    if (!subjectId.isEmpty())
    {
        SubjectInfo subject;
        subject.id = subjectId;
        subject.name = props.getValue("currentSubjectName");
        subject.notes = props.getValue("currentSubjectNotes");
        setCurrentSubject(subject);
    }
    
    // 加载设备参数
    deviceState.channelCount = props.getIntValue("deviceChannels", 32);
    deviceState.sampleRate = props.getDoubleValue("deviceSampleRate", 500.0);
    deviceState.montageType = props.getValue("deviceMontage", "10-20");
    
    // 加载最近项目
    int recentCount = props.getIntValue("recentProjectCount", 0);
    for (int i = 0; i < recentCount; ++i)
    {
        auto key = "recentProject_" + juce::String(i);
        ProjectInfo project;
        const auto projectPath = juce::File(props.getValue(key + "_path"));
        if (!loadProjectMetadata(projectPath, project))
        {
            project.id = props.getValue(key + "_id");
            project.name = props.getValue(key + "_name");
            project.rootPath = projectPath;
        }
        
        if (project.isValid())
        {
            recentProjects.add(project);
        }
    }
}

// ============================================================================
// 当前采集会话
// ============================================================================

void GlobalContextStore::setCurrentSession(const domain::Session& session)
{
    juce::ScopedLock lock(stateLock);
    currentSession = session;
    notifyContextChanged();
}

const domain::Session* GlobalContextStore::getCurrentSession() const noexcept
{
    return currentSession.has_value() ? &*currentSession : nullptr;
}

bool GlobalContextStore::hasCurrentSession() const noexcept
{
    return currentSession.has_value() && currentSession->isValid();
}

void GlobalContextStore::clearCurrentSession()
{
    juce::ScopedLock lock(stateLock);
    currentSession.reset();
    notifyContextChanged();
}

// ============================================================================
// 最近录制历史
// ============================================================================

void GlobalContextStore::addRecentRecording(const domain::Recording& rec)
{
    if (!rec.isValid())
        return;

    {
        juce::ScopedLock lock(stateLock);
        recentRecordings.removeIf([&rec](const domain::Recording& r) { return r.id == rec.id; });
        recentRecordings.insert(0, rec);
        while (recentRecordings.size() > kMaxRecentRecordings)
            recentRecordings.removeLast();
    }

    notifyContextChanged();
    listeners.call([&rec](Listener& l) { l.onRecordingAdded(rec); });
}

juce::Array<domain::Recording> GlobalContextStore::getRecentRecordings(int maxCount) const
{
    juce::ScopedLock lock(stateLock);
    juce::Array<domain::Recording> result;
    int count = juce::jmin(maxCount, recentRecordings.size());
    for (int i = 0; i < count; ++i)
        result.add(recentRecordings[i]);
    return result;
}

// ============================================================================
// 最近验证结果历史
// ============================================================================

void GlobalContextStore::addRecentValidationResult(const domain::ValidationResult& result)
{
    if (!result.isValid())
        return;

    {
        juce::ScopedLock lock(stateLock);
        recentValidationResults.removeIf(
            [&result](const domain::ValidationResult& r) { return r.id == result.id; });
        recentValidationResults.insert(0, result);
        while (recentValidationResults.size() > kMaxRecentValidations)
            recentValidationResults.removeLast();
    }

    notifyContextChanged();
    listeners.call([&result](Listener& l) { l.onValidationResultAdded(result); });
}

juce::Array<domain::ValidationResult> GlobalContextStore::getRecentValidationResults(int maxCount) const
{
    juce::ScopedLock lock(stateLock);
    juce::Array<domain::ValidationResult> out;
    int count = juce::jmin(maxCount, recentValidationResults.size());
    for (int i = 0; i < count; ++i)
        out.add(recentValidationResults[i]);
    return out;
}

// ============================================================================
// 服务层结果回填 (Service → Context 桥接)
// ============================================================================

void GlobalContextStore::applyModelArtifact(const domain::ModelArtifact& artifact)
{
    if (!artifact.isValid())
        return;

    ModelInfo info;
    info.id             = artifact.id;
    info.name           = artifact.modelName;
    info.version        = artifact.version;
    info.onnxPath       = artifact.onnxPath;
    info.manifestPath   = artifact.manifestPath;
    info.inputChannels  = artifact.inputChannels;
    info.inputSamples   = artifact.inputSamples;
    info.outputClasses  = artifact.outputClasses;
    info.classNames     = artifact.classNames;
    info.created        = artifact.createdAt;
    info.isValidated    = (artifact.validationStatus == "passed");

    setCurrentModel(info);
}

void GlobalContextStore::applyProcessedDataset(const domain::ProcessedDataset& dataset)
{
    if (!dataset.isValid())
        return;

    DatasetInfo info;
    info.id           = dataset.id;
    info.name         = dataset.outputPath.getFileNameWithoutExtension();
    info.path         = dataset.outputPath;
    info.sampleCount  = dataset.sampleCount;
    info.channelCount = dataset.channelCount;
    if (dataset.sampleRate > 0 && dataset.windowSize > 0 && dataset.sampleCount > 0)
        info.duration = double(dataset.sampleCount * dataset.windowSize) / double(dataset.sampleRate);
    info.isProcessed  = (dataset.state == domain::TaskState::Completed);
    info.created      = dataset.createdAt;

    setCurrentDataset(info);
}

void GlobalContextStore::applyValidationResult(const domain::ValidationResult& result)
{
    if (!result.isValid())
        return;

    // 更新对应模型的验证状态
    updateModelValidation(result.modelId, result.passed);

    // 追加验证历史
    addRecentValidationResult(result);
}

// ============================================================================

void GlobalContextStore::addListener(Listener* listener)
{
    listeners.add(listener);
}

void GlobalContextStore::removeListener(Listener* listener)
{
    listeners.remove(listener);
}

void GlobalContextStore::notifyContextChanged()
{
    listeners.call([](Listener& l) { l.onContextChanged(); });
    sendChangeMessage();
}

} // namespace nerou::app
