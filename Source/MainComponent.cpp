#include "MainComponent.h"
#include "Acquisition/BoardManager.h"
#include "Application/RuntimeSettingsStore.h"
#include "Application/GlobalContextStore.h"
#include "Core/JsonFileIO.h"
#include "Core/Utf8Literals.h"
#include "Core/SignalNorm.h"
#include "Core/Utf8FileText.h"
#include "Core/JsonVarHelpers.h"
#include <BinaryData.h>
#include <algorithm>

/** UI 文案别名：与 `NR_STR` 相同，宽字面量避免中文乱码 */
#define W NR_STR

namespace {
/** Windows JUCE String::formatted + 含中文的格式串经 _vsnwprintf 不稳定；整数左补零仅ASCII */
juce::String intZeroPadded(int n, int minDigits)
{
    juce::String s(n);
    while (s.length() < minDigits)
        s = juce::String::charToString((juce::juce_wchar)'0') + s;
    return s;
}

juce::String makeEntityId(const juce::String& prefix)
{
    return prefix + "_" + juce::Uuid().toDashedString();
}

/** 侧栏 Tab 基础说明（含快捷键），供 setupTooltips 与 updateTabGuardTooltips 共用 */
juce::String sidebarTabBaseTooltip(nerou::app::WorkflowTab t)
{
    using Tab = nerou::app::WorkflowTab;
    switch (t)
    {
        case Tab::Overview:    return W("\u5207\u6362\u5230\u300c\u603b\u89c8\u300d\u6807\u7b7e\u9875 (Ctrl+0)");
        case Tab::Acquisition: return W("\u5207\u6362\u5230\u300c\u8bad\u7ec3\u6587\u4ef6\u300d\u6807\u7b7e\u9875 (Ctrl+1)");
        case Tab::DataPrep:    return W("\u5207\u6362\u5230\u300c\u6570\u636e\u9884\u5904\u7406\u300d\u6807\u7b7e\u9875 (Ctrl+2)");
        case Tab::Training:    return W("\u5207\u6362\u5230\u300c\u8bad\u7ec3\u4e2d\u5fc3\u300d\u6807\u7b7e\u9875 (Ctrl+3)");
        case Tab::Inference:   return W("\u5207\u6362\u5230\u300c\u6a21\u578b\u9a8c\u8bc1\u300d\u6807\u7b7e\u9875 (Ctrl+4)");
    }
    return {};
}

juce::String resolveCurrentProjectId()
{
    auto& context = nerou::app::Context();
    if (context.hasCurrentProject())
        return context.getCurrentProject().id;

    return "default";
}

juce::String resolveCurrentSubjectId()
{
    auto& context = nerou::app::Context();
    if (context.hasCurrentSubject())
        return context.getCurrentSubject().id;

    return "default_subject";
}

juce::String resolveCurrentProjectDisplayName()
{
    auto& context = nerou::app::Context();
    if (context.hasCurrentProject())
    {
        const auto& project = context.getCurrentProject();
        return project.name.isNotEmpty() ? project.name : project.id;
    }

    return W("\u672a\u9009\u62e9\u9879\u76ee");
}

juce::String resolveCurrentSubjectDisplayName()
{
    auto& context = nerou::app::Context();
    if (context.hasCurrentSubject())
        return context.getCurrentSubject().getDisplayName();

    return W("\u672a\u9009\u62e9\u53d7\u8bd5\u8005");
}

juce::String composeBusinessContextPill()
{
    return W("\u9879\u76ee\uff1a") + resolveCurrentProjectDisplayName()
        + W(" | \u53d7\u8bd5\u8005\uff1a") + resolveCurrentSubjectDisplayName();
}

juce::String composeStatusBarContextText()
{
    return composeBusinessContextPill()
        + W("  |  NerouRuntime v1.0.0  |  JUCE 8");
}

bool directoryContainsNpz(const juce::File& dir)
{
    if (!dir.isDirectory())
        return false;

    juce::DirectoryIterator iter(dir, false, "*.npz", juce::File::findFiles);
    return iter.next();
}

juce::File chooseUsableTrainingDataDirectory(const juce::File& appRoot, const juce::String& persistedPath)
{
    const juce::File persisted(persistedPath.trim());
    if (directoryContainsNpz(persisted))
        return persisted;

    const juce::File candidates[] = {
        appRoot.getChildFile("data").getChildFile("training_files"),
        appRoot.getChildFile("data").getChildFile("npz"),
        juce::File::getCurrentWorkingDirectory().getChildFile("data").getChildFile("training_files"),
        juce::File::getCurrentWorkingDirectory().getChildFile("data").getChildFile("npz")
    };

    for (const auto& candidate : candidates)
        if (directoryContainsNpz(candidate))
            return candidate;

    return persistedPath.trim().isNotEmpty() ? persisted : appRoot.getChildFile("data").getChildFile("training_files");
}

juce::String nodeCommandId(nerou::domain::NodeType type)
{
    switch (type)
    {
    case nerou::domain::NodeType::Acquisition:   return "node.acquisition.command";
    case nerou::domain::NodeType::Preprocessing: return "node.preprocess.command";
    case nerou::domain::NodeType::Training:      return "node.training.command";
    case nerou::domain::NodeType::Validation:    return "node.validation.command";
    case nerou::domain::NodeType::Archive:       return "node.archive.command";
    }

    return "node.unknown.command";
}

juce::String acquisitionModeToKey(AcquisitionMode mode)
{
    switch (mode)
    {
    case AcquisitionMode::LiveBoard: return "live_board";
    case AcquisitionMode::Playback:  return "playback";
    default:                         return "synthetic";
    }

    return "synthetic";
}

juce::StringArray buildChannelNames(int channelCount, int montageId)
{
    static const char* const k1020[] = {
        "Fp1","Fp2","F7","F3","Fz","F4","F8","FT7","T7","C5","C3","C1","Cz","C2","C4","C6","T8","FT8",
        "TP7","CP5","CP3","CP1","CPz","CP2","CP4","CP6","TP8",
        "P7","P5","P3","P1","Pz","P2","P4","P6","P8","O1","Oz","O2","A1","A2"
    };
    static const char* const k1010[] = {
        "Fp1","Fpz","Fp2","AF7","AF3","AFz","AF4","AF8",
        "F7","F5","F3","F1","Fz","F2","F4","F6","F8",
        "FT7","FC5","FC3","FC1","FCz","FC2","FC4","FC6","FT8",
        "T7","C5","C3","C1","Cz","C2","C4","C6","T8",
        "TP7","CP5","CP3","CP1","CPz","CP2","CP4","CP6","TP8",
        "P7","P5","P3","P1","Pz","P2","P4","P6","P8",
        "PO7","PO5","PO3","POz","PO4","PO6","PO8","O1","Oz","O2"
    };
    static const char* const k105[] = {
        "Fp1","Fpz","Fp2","AF7","AF3","AFz","AF4","AF8",
        "F7","F5","F3","F1","Fz","F2","F4","F6","F8",
        "FT7","FC5","FC3","FC1","FCz","FC2","FC4","FC6","FT8",
        "T7","C5","C3","C1","Cz","C2","C4","C6","T8",
        "TP7","CP5","CP3","CP1","CPz","CP2","CP4","CP6","TP8",
        "P7","P5","P3","P1","Pz","P2","P4","P6","P8",
        "PO7","PO5","PO3","POz","PO4","PO6","PO8","O1","Oz","O2",
        "T9","T10","PPO9h","PPO10h","PO9h","PO10h","OI1h","OI2h"
    };

    const char* const* labels = nullptr;
    int labelCount = 0;
    switch (montageId)
    {
    case 2:
        labels = k1020;
        labelCount = (int) (sizeof(k1020) / sizeof(k1020[0]));
        break;
    case 3:
        labels = k1010;
        labelCount = (int) (sizeof(k1010) / sizeof(k1010[0]));
        break;
    case 4:
        labels = k105;
        labelCount = (int) (sizeof(k105) / sizeof(k105[0]));
        break;
    default:
        break;
    }

    juce::StringArray names;
    for (int i = 0; i < channelCount; ++i)
    {
        if (labels != nullptr && i < labelCount)
            names.add(labels[i]);
        else
            names.add("CH_" + juce::String(i + 1));
    }
    return names;
}

int countFilesWithExtension(const juce::File& directory, const juce::String& wildcard)
{
    if (!directory.isDirectory())
        return 0;

    int count = 0;
    juce::DirectoryIterator iter(directory, false, wildcard, juce::File::findFiles);
    while (iter.next())
        ++count;
    return count;
}

bool directoryContainsRawSignalFiles(const juce::File& directory)
{
    if (!directory.isDirectory())
        return false;

    static const char* const patterns[] = {
        "*.edf", "*.bdf", "*.gdf", "*.fif", "*.set", "*.vhdr"
    };

    for (auto* pattern : patterns)
    {
        juce::DirectoryIterator iter(directory, false, pattern, juce::File::findFiles);
        if (iter.next())
            return true;
    }
    return false;
}

juce::String makeFilesystemSafeName(juce::String text)
{
    text = text.trim();
    if (text.isEmpty())
        text = "model";
    for (auto ch : juce::String("\\/:*?\"<>|"))
        text = text.replaceCharacter(ch, '_');
    return text;
}

juce::File findFirstOnnxFile(const juce::File& dir)
{
    if (!dir.isDirectory())
        return {};

    juce::Array<juce::File> files;
    dir.findChildFiles(files, juce::File::findFiles, false, "*.onnx");
    return files.isEmpty() ? juce::File{} : files.getFirst();
}

juce::String timeToIsoString(const juce::Time& t)
{
    if (t == juce::Time())
        return {};

    return t.formatted("%Y-%m-%dT%H:%M:%S");
}

juce::String formatSubjectMenuLabel(const nerou::app::SubjectInfo& subject)
{
    juce::String label = subject.getDisplayName();
    juce::StringArray meta;
    meta.add(W("会话 ") + juce::String(juce::jmax(0, subject.sessionCount)));
    if (subject.lastSession != juce::Time())
        meta.add(W("最近 ") + subject.lastSession.formatted("%Y-%m-%d %H:%M"));
    if (!meta.isEmpty())
        label += W("  |  ") + meta.joinIntoString(W("  |  "));
    return label;
}

juce::String formatProjectMenuLabel(const nerou::app::ProjectInfo& project)
{
    juce::String label = project.name.isNotEmpty() ? project.name : project.id;
    juce::StringArray meta;
    meta.add(W("受试者 ") + juce::String(juce::jmax(0, project.subjectCount)));
    meta.add(W("数据集 ") + juce::String(juce::jmax(0, project.datasetCount)));
    meta.add(W("模型 ") + juce::String(juce::jmax(0, project.modelCount)));
    if (project.lastModified != juce::Time())
        meta.add(W("最近 ") + project.lastModified.formatted("%Y-%m-%d %H:%M"));
    if (!meta.isEmpty())
        label += W("  |  ") + meta.joinIntoString(W("  |  "));
    return label;
}

juce::var toJson(const nerou::domain::ProcessedDataset& dataset)
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty("dataset_id", dataset.id);
    obj->setProperty("project_id", dataset.projectId);
    obj->setProperty("source_recording_ids", nerou::core::stringArrayToVar(dataset.sourceRecordingIds));
    obj->setProperty("input_path", dataset.inputPath.getFullPathName());
    obj->setProperty("output_path", dataset.outputPath.getFullPathName());
    obj->setProperty("sample_count", dataset.sampleCount);
    obj->setProperty("channel_count", dataset.channelCount);
    obj->setProperty("sample_rate", dataset.sampleRate);
    obj->setProperty("window_size", dataset.windowSize);
    obj->setProperty("label_count", dataset.labelCount);
    obj->setProperty("preprocess_config", dataset.preprocessConfig);
    obj->setProperty("summary_path", dataset.summaryPath.getFullPathName());
    obj->setProperty("failed_files", nerou::core::stringArrayToVar(dataset.failedFiles));
    obj->setProperty("created_at", timeToIsoString(dataset.createdAt));
    obj->setProperty("status", nerou::domain::toDisplayString(dataset.state));
    return juce::var(obj.release());
}

juce::var toJson(const nerou::domain::TrainingJob& job)
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty("training_job_id", job.id);
    obj->setProperty("project_id", job.projectId);
    obj->setProperty("dataset_id", job.datasetId);
    obj->setProperty("task_type", job.taskType);
    obj->setProperty("model_template", job.modelTemplate);
    obj->setProperty("class_count", job.classCount);
    obj->setProperty("epochs", job.epochs);
    obj->setProperty("batch_size", job.batchSize);
    obj->setProperty("learning_rate", job.learningRate);
    obj->setProperty("train_config", job.trainConfig);
    obj->setProperty("preflight_result", job.preflightResult);
    obj->setProperty("log_path", job.logPath.getFullPathName());
    obj->setProperty("metrics_path", job.metricsPath.getFullPathName());
    obj->setProperty("started_at", timeToIsoString(job.startedAt));
    obj->setProperty("ended_at", timeToIsoString(job.endedAt));
    obj->setProperty("status", nerou::domain::toDisplayString(job.state));
    obj->setProperty("result_model_id", job.resultModelId);
    return juce::var(obj.release());
}

juce::var toJson(const nerou::domain::ValidationResult& validation)
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty("validation_id", validation.id);
    obj->setProperty("project_id", validation.projectId);
    obj->setProperty("model_id", validation.modelId);
    obj->setProperty("dataset_id", validation.datasetId);
    obj->setProperty("inference_job_id", validation.inferenceJobId);
    obj->setProperty("validation_type", validation.validationType);
    obj->setProperty("passed", validation.passed);
    obj->setProperty("conclusion", validation.conclusion);
    obj->setProperty("risk_level", validation.riskLevel);
    obj->setProperty("metrics", validation.metrics);
    obj->setProperty("issues", nerou::core::stringArrayToVar(validation.issues));
    obj->setProperty("suggestions", nerou::core::stringArrayToVar(validation.suggestions));
    obj->setProperty("report_path", validation.reportPath.getFullPathName());
    obj->setProperty("validated_at", timeToIsoString(validation.validatedAt));
    obj->setProperty("validated_by", validation.validatedBy);
    return juce::var(obj.release());
}

juce::var toJson(const nerou::domain::Recording& recording)
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty("recording_id", recording.id);
    obj->setProperty("session_id", recording.sessionId);
    obj->setProperty("project_id", recording.projectId);
    obj->setProperty("subject_id", recording.subjectId);
    obj->setProperty("file_path", recording.filePath.getFullPathName());
    obj->setProperty("file_format", recording.fileFormat);
    obj->setProperty("sample_rate", recording.sampleRate);
    obj->setProperty("channel_count", recording.channelCount);
    obj->setProperty("channel_names", nerou::core::stringArrayToVar(recording.channelNames));
    obj->setProperty("duration_sec", recording.durationSec);
    obj->setProperty("sample_count", recording.sampleCount);
    obj->setProperty("impedance_summary", recording.impedanceSummary);
    obj->setProperty("quality_score", recording.qualityScore);
    obj->setProperty("event_count", recording.eventCount);
    obj->setProperty("created_at", timeToIsoString(recording.createdAt));
    obj->setProperty("created_by", recording.createdBy);
    obj->setProperty("status", recording.status);
    return juce::var(obj.release());
}
} // namespace

//==============================================================================
// 布局与中文排版：汉字视认性依赖略大字号与行长；侧栏宽度由 resized() 按窗口比例写layoutSidebarPx// 正文字号参常见中UI（约 10.52pt 映射135px 屏幕坐标，按 96dpi 量级）
static constexpr int PAD      = 16;
static constexpr int FIELD_H  = 34;
static constexpr int LABEL_H  = 22;
static constexpr int BTN_H    = 40;
static constexpr int BTN_GAP  = 8;
static constexpr float FONT_LOG      = 13.0f;  // 日志多行正文
static constexpr float FONT_FIELD    = 14.0f; // 输入
static constexpr float FONT_UI       = 12.5f; // 表单标签、侧栏说
static constexpr float FONT_UI_COMPACT = 12.0f; // 次要标签、状态栏
static constexpr int   kPreflightFailTitleFlashTicks = 36; // ~1.2 s @30Hz
static constexpr int   kPreflightOkTitleFlashTicks   = 24;

// ── Colour palette ────────────────────────────────────────────────────────────
// ── 微信风格颜色系统 ─────────────────────────────────────
// 导航栏（深色侧边栏）
static const juce::Colour C_NAV_BG        (0xff2B2B2B);  // 导航栏背景
static const juce::Colour C_NAV_SELECTED  (0xff07C160);  // 选中项绿色
static const juce::Colour C_NAV_HOVER     (0xff3A3A3A);  // 悬停深灰
static const juce::Colour C_NAV_TEXT      (0xffCCCCCC);  // 导航文字
static const juce::Colour C_NAV_TEXT_ACT  (0xffFFFFFF);  // 激活文字白
// 内容区（浅色）
static const juce::Colour C_BG        (0xffF5F5F5);  // 全局背景浅灰
static const juce::Colour C_SURFACE   (0xffFFFFFF);  // 卡片白
static const juce::Colour C_BORDER    (0xffE6E6E6);  // 分割线
static const juce::Colour C_PRIMARY   (0xff07C160);  // 微信绿
static const juce::Colour C_PRIMARY_HV(0xff06AD56);  // 深绿悬停
static const juce::Colour C_DANGER    (0xffFA5151);  // 微信红
static const juce::Colour C_SUCCESS   (0xff07C160);  // 成功绿
static const juce::Colour C_WARNING   (0xffF7A600);  // 警告橙
static const juce::Colour C_TEXT_H    (0xff191919);  // 标题黑
static const juce::Colour C_TEXT_B    (0xff3D3D3D);  // 正文深灰
static const juce::Colour C_TEXT_S    (0xff818181);  // 次要灰
static const juce::Colour C_CHIP_BG   (0xffE8F5EE);  // 绿色浅底
static const juce::Colour C_LOG_BG    (0xff1E1E1E);  // 日志背景
static const juce::Colour C_LOG_FG    (0xffD0D0D0);  // 日志文字
static const juce::Colour C_LOG_OUTLINE(0xff333333); // 日志边框

//==============================================================================
// Style helpers
static void styleLabel(juce::Label& l, const ModernLookAndFeel& th,
                        juce::Colour col, float sz, bool bold = false,
                        juce::Justification j = juce::Justification::centredLeft)
{
    l.setColour(juce::Label::textColourId, col);
    l.setFont(th.cjkFont(sz, bold));
    l.setJustificationType(j);
}

static void styleField(juce::TextEditor& e, const ModernLookAndFeel& th)
{
    e.setColour(juce::TextEditor::backgroundColourId,     C_SURFACE);
    e.setColour(juce::TextEditor::outlineColourId,        C_BORDER);
    e.setColour(juce::TextEditor::focusedOutlineColourId, C_PRIMARY);
    e.setColour(juce::TextEditor::textColourId,           C_TEXT_B);
    e.setColour(juce::TextEditor::highlightColourId,      C_PRIMARY.withAlpha(0.18f));
    e.setFont(th.cjkFont(FONT_FIELD));
}

static void styleCombo(juce::ComboBox& c)
{
    c.setColour(juce::ComboBox::backgroundColourId, C_SURFACE);
    c.setColour(juce::ComboBox::outlineColourId,    C_BORDER);
    c.setColour(juce::ComboBox::textColourId,       C_TEXT_B);
    c.setColour(juce::ComboBox::arrowColourId,      C_TEXT_S);
}

static void stylePrimaryBtn(juce::TextButton& b)
{
    b.setColour(juce::TextButton::buttonColourId,   C_PRIMARY);
    b.setColour(juce::TextButton::buttonOnColourId, C_PRIMARY_HV);
    b.setColour(juce::TextButton::textColourOffId,  juce::Colours::white);
    b.setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
}

// 微信风格：轮廓按钮（白底+边框）
static void styleOutlineBtn(juce::TextButton& b, juce::Colour fg = C_TEXT_B)
{
    b.setColour(juce::TextButton::buttonColourId,   C_SURFACE);
    b.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffF0F0F0));
    b.setColour(juce::TextButton::textColourOffId,  fg);
    b.setColour(juce::TextButton::textColourOnId,   fg);
    b.getProperties().set("outlined", true);
}

// 微信风格：导航标签按钮（侧边深色导航栏中使用）
static void styleNavTabBtn(juce::TextButton& b, bool isActive = false)
{
    if (isActive)
    {
        b.setColour(juce::TextButton::buttonColourId,  C_NAV_SELECTED.withAlpha(0.15f));
        b.setColour(juce::TextButton::textColourOffId, C_NAV_TEXT_ACT);
        b.setColour(juce::TextButton::textColourOnId,  C_NAV_TEXT_ACT);
    }
    else
    {
        b.setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
        b.setColour(juce::TextButton::textColourOffId, C_NAV_TEXT);
        b.setColour(juce::TextButton::textColourOnId,  C_NAV_TEXT_ACT);
    }
    b.getProperties().set("nav_tab", true);
}

// Generic browse button launcher
static void browse(std::unique_ptr<juce::FileChooser>& fc,
                   const juce::String& title,
                   juce::TextEditor& target,
                   bool dirMode = true)
{
    int flags = juce::FileBrowserComponent::openMode |
                (dirMode ? juce::FileBrowserComponent::canSelectDirectories
                         : juce::FileBrowserComponent::canSelectFiles);
    fc = std::make_unique<juce::FileChooser>(title, juce::File::getCurrentWorkingDirectory(), "*");
    fc->launchAsync(flags, [&target](const juce::FileChooser& c) {
        auto r = c.getResult();
        if (r.exists()) target.setText(r.getFullPathName());
    });
}

//==============================================================================
MainComponent::MainComponent()
{
    setLookAndFeel(&theme);

    // ── FIRST: reset all Chinese text via L"..." wide literals ─────────────────
    // On MSVC/Windows, wchar_t literals are always UTF-16 regardless of
    // source/execution charset settings — this is the definitive encoding fix.
setupComponentText();

    // ─── Settings persistence ─────────────────────────────────────────────────
juce::PropertiesFile::Options propOpts;
    propOpts.applicationName     = "NerouRuntime";
    propOpts.filenameSuffix      = "settings";
    propOpts.osxLibrarySubFolder = "Application Support";
    appProperties.setStorageParameters(propOpts);
    if (auto* props = appProperties.getUserSettings())
        nerou::app::Context().loadFromProperties(*props);
    const auto bootstrap =
        nerou::app::RuntimeSettingsStore::loadBootstrap(appProperties.getUserSettings());

    // 硬件加速偏好：默认 Auto（会优先 GPU → CPU 回退）
    {
        const auto perf = nerou::app::RuntimeSettingsStore::loadPerformance(appProperties.getUserSettings());
        const auto mode = accel::fromKey(perf.accelerationMode);
        onnxRunner.setAccelerationMode(mode);
        validationService.setPreferredAccelerationMode(mode);
        NR_LOGI("System", juce::String::fromUTF8(u8"\u63a8\u7406\u52a0\u901f\u504f\u597d\uff1a") + accel::toDisplayName(mode));
    }

    nerou::app::Context().addListener(this);
    nerou::app::Notify().addListener(this);
    nerou::ui::SnackbarManager::getInstance().setHostWindow(this);

    // ── App Shell 初始化 ─────────────────────────────────────────────────
    addAndMakeVisible(appShell_);
    appShell_.getNavStrip().onNavChanged = [this](const juce::String& id) {
        if (id == "validation") switchTab(Tab::Inference);
        else                    switchTab(Tab::Training);
    };
    appShell_.getNavStrip().getSettingsButton().onClick = [this] {
        settingsDialog.showOverlay(this);
    };
    addChildComponent(commandPalette_);

    modelRegistryService.setRootDirectory(juce::File::getCurrentWorkingDirectory().getChildFile("onnx"));
    nameInput.setText(bootstrap.modelName, false);
    dataPathInput.setText(bootstrap.dataPath, false);
    epochInput.setText(bootstrap.epochs, false);
    classCountInput.setText(bootstrap.trainClassCount, false);
    savePathInput.setText(bootstrap.savePath, false);
    inferModelInput.setText(bootstrap.lastOnnxPath, false);
    inferDataInput.setText(bootstrap.lastInferNpzPath, false);
    addAndMakeVisible(topPrimaryActionBtn);
    stylePrimaryBtn(topPrimaryActionBtn);
    topPrimaryActionBtn.onClick = [this] { handleTopPrimaryAction(); };
    addAndMakeVisible(topSecondaryActionBtn);
    styleOutlineBtn(topSecondaryActionBtn, C_PRIMARY);
    topSecondaryActionBtn.onClick = [this] { handleTopSecondaryAction(); };

    // 系统日志按钮（顶栏右侧，Ctrl+L 亦可呼出）
    addAndMakeVisible(topLogBtn);
    styleOutlineBtn(topLogBtn, C_PRIMARY);
    topLogBtn.setButtonText(W("\u65e5\u5fd7"));
    topLogBtn.setTooltip(W("\u67e5\u770b\u7cfb\u7edf\u65e5\u5fd7\uff08Ctrl+L\uff09"));
    topLogBtn.onClick = [this] { showSystemLogPanel(); };

    addAndMakeVisible(statusBar);
    styleLabel(statusBar, theme, C_TEXT_S, 12.0f);
    statusBar.setColour(juce::Label::backgroundColourId, C_SURFACE);
    setStatusMsg(W("\u5c31\u7eea | Ctrl+1 \u6a21\u578b\u8bad\u7ec3 | Ctrl+2 \u63a8\u7406\u6a21\u578b | Ctrl+Enter \u8bad\u7ec3 | Esc \u505c\u6b62"));

    addAndMakeVisible(statusBarRight);
    styleLabel(statusBarRight, theme, C_TEXT_S, 11.5f, false, juce::Justification::centredRight);
    statusBarRight.setColour(juce::Label::backgroundColourId, C_SURFACE);

    // 导航标题（微信风格：白色文字显示在深色导航栏）
    addAndMakeVisible(appTitleLabel);
    appTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFFFFFFF));
    appTitleLabel.setFont(theme.cjkFont(16.0f, true));
    appTitleLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(appSubLabel);
    appSubLabel.setVisible(false);
    appSubLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF8A8A8A));
    appSubLabel.setFont(theme.cjkFont(10.0f));

    // Tab selector (5 tabs per PRD)
    for (auto* btn : { &tabOverviewBtn, &tabAcquisitionBtn, &tabDataPrepBtn, &tabTrainBtn, &tabInferBtn }) {
        addAndMakeVisible(*btn);
        styleOutlineBtn(*btn, C_PRIMARY);
        btn->setName("NR::Tab");
        btn->getProperties().set("nav_tab", true);
    }
    tabOverviewBtn.setVisible(false);
    tabAcquisitionBtn.setVisible(false);
    tabDataPrepBtn.setVisible(false);
    tabTrainBtn.setVisible(false);
    tabInferBtn.setVisible(false);

    styleNavTabBtn(tabOverviewBtn,    false);
    styleNavTabBtn(tabAcquisitionBtn, false);
    styleNavTabBtn(tabDataPrepBtn,    false);
    styleNavTabBtn(tabTrainBtn,       true);
    styleNavTabBtn(tabInferBtn,       false);

    tabOverviewBtn.onClick    = [this] { switchTab(Tab::Overview); };
    tabAcquisitionBtn.onClick = [this] { switchTab(Tab::Acquisition); };
    tabDataPrepBtn.onClick    = [this] { switchTab(Tab::DataPrep); };
    tabTrainBtn.onClick       = [this] { switchTab(Tab::Training); };
    tabInferBtn.onClick       = [this] { switchTab(Tab::Inference); };

    // Register page components
    addAndMakeVisible(overviewPage);
    addAndMakeVisible(acquisitionPage);
    addAndMakeVisible(preparationPage);
    addAndMakeVisible(trainingPage);
    addAndMakeVisible(validationPage);

    // Wire cross-page navigation callbacks
    overviewPage.onNavigateToPage = [this](const juce::String& pageId) {
        if (pageId == "validation") switchTab(Tab::Inference);
        else                        switchTab(Tab::Training);
    };
    overviewPage.onCreateNewProject = [this] { showCreateProjectDialog(); };
    overviewPage.onImportData = [this] { switchTab(Tab::Training); };
    overviewPage.onConnectDevice = [this] { switchTab(Tab::Training); };
    overviewPage.onOpenSettings = [this] { settingsDialog.showOverlay(this); };

    acquisitionPage.onRequestSelectSubject = [this] { showSubjectQuickSwitchMenu(); };
    acquisitionPage.onRequestCreateSubject = [this] { showCreateSubjectDialog(); };
    acquisitionPage.onInferenceToggled = [this](bool enabled) {
        realtimeOnnxInferToggle.setToggleState(enabled, juce::dontSendNotification);
    };

    trainingPage.onRequestSelectDataset   = [this] { switchTab(Tab::Training); };
    trainingPage.onRequestValidateModel   = [this] { switchTab(Tab::Inference); };
    trainingPage.onModelExported = [this](const nerou::app::ModelInfo& model) {
        nerou::app::Notify().postSuccess(
            juce::String::fromUTF8(u8"\u65b0\u6a21\u578b\u5df2\u5c31\u7eea"),
            juce::String::fromUTF8(u8"\u53ef\u5728 ONNX \u9a8c\u8bc1\u9875\u8fdb\u884c\u63a8\u7406\u9a8c\u8bc1"));
    };
    validationPage.onRequestRetrainModel = [this] { switchTab(Tab::Training); };
    validationPage.onRequestDeployModel = [this] { exportRuntimeDataPackage(); };

    expertUi = bootstrap.expertUi;

    // ── 首次启动工作区自动初始化 ───────────────────────────────────────
    {
        auto& ctx = nerou::app::Context();
        const auto appRoot = ProjectPaths::resolveProjectRootDirectory();
        ProjectPaths::ensureApplicationResourceStructure(appRoot);
        const auto usableTrainingDir = chooseUsableTrainingDataDirectory(appRoot, dataPathInput.getText());
        if (usableTrainingDir.isDirectory()
            && usableTrainingDir.getFullPathName() != dataPathInput.getText().trim())
        {
            dataPathInput.setText(usableTrainingDir.getFullPathName(), false);
            NR_LOGI(W("\u8bad\u7ec3"), W("\u5df2\u81ea\u52a8\u4fee\u6b63\u8bad\u7ec3\u6570\u636e\u76ee\u5f55: ")
                                    + usableTrainingDir.getFullPathName());
        }
        if (!ctx.hasCurrentProject())
        {
            const auto defaultProjectRoot = ProjectPaths::getProjectsBaseDir().getChildFile("default_project");
            ProjectPaths::ensureProjectStructure(defaultProjectRoot);
            nerou::app::ProjectInfo defaultProject;
            defaultProject.id          = "default";
            defaultProject.name        = "NerouRuntime";
            defaultProject.description = juce::String::fromUTF8(u8"\u5185\u7f6e\u9ed8\u8ba4\u9879\u76ee");
            defaultProject.rootPath    = defaultProjectRoot;
            defaultProject.created     = juce::Time::getCurrentTime();
            defaultProject.lastModified= juce::Time::getCurrentTime();
            ctx.setCurrentProject(defaultProject);
            NR_LOGI("Workspace", juce::String::fromUTF8(u8"\u5df2\u521d\u59cb\u5316\u9ed8\u8ba4\u9879\u76ee: ") + defaultProjectRoot.getFullPathName());
        }

        if (!ctx.hasCurrentDataset())
        {
            auto npzDir = appRoot.getChildFile("data").getChildFile("training_files");
            juce::Array<juce::File> trainingFiles;
            npzDir.findChildFiles(trainingFiles, juce::File::findFiles, false, "*.npz");
            if (trainingFiles.isEmpty())
                npzDir = appRoot.getChildFile("data").getChildFile("npz");
            if (npzDir.isDirectory())
            {
                int npzCount = 0;
                juce::DirectoryIterator iter(npzDir, false, "*.npz", juce::File::findFiles);
                while (iter.next()) ++npzCount;

                if (npzCount > 0)
                {
                    nerou::app::DatasetInfo defaultDs;
                    defaultDs.id          = "builtin_demo";
                    defaultDs.name        = juce::String::fromUTF8(u8"\u5185\u7f6e\u6f14\u793a\u6570\u636e\u96c6");
                    defaultDs.path        = npzDir;
                    defaultDs.sampleCount = npzCount;
                    defaultDs.channelCount= 8;
                    defaultDs.isProcessed = true;
                    defaultDs.created     = juce::Time::getCurrentTime();
                    ctx.setCurrentDataset(defaultDs);
                    NR_LOGI("Workspace",
                        juce::String::fromUTF8(u8"\u5df2\u52a0\u8f7d\u5185\u7f6e\u6570\u636e\u96c6: ")
                        + juce::String(npzCount)
                        + juce::String::fromUTF8(u8" \u4e2a NPZ \u6587\u4ef6"));
                }
            }
        }
    }

    // Set default page: the production workflow opens directly on model training.
    currentTab = Tab::Training;
    workflowOrchestrator.setActiveTab(Tab::Training);
    appShell_.setContentPage(&trainingPage);
    appShell_.getNavStrip().setActiveById("training");
    wirePipelineStoreCallbacks();
    wireSettingsDialog();

    // setWantsKeyboardFocus AFTER setLookAndFeel (already called at line 93)
setWantsKeyboardFocus(true);

    // ─── Tooltip window ─────────────────────────────────────────────────────
addAndMakeVisible(tooltipWindow);

    addAndMakeVisible(topBarTitle);
    styleLabel(topBarTitle, theme, C_TEXT_H, 16.0f, true);
    addAndMakeVisible(topBarSubtitle);
    styleLabel(topBarSubtitle, theme, C_TEXT_S, 12.0f, false);
    addAndMakeVisible(topBarPillPrimary);
    styleLabel(topBarPillPrimary, theme, C_PRIMARY, 11.0f, false);
    topBarPillPrimary.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(topBarPillSecondary);
    styleLabel(topBarPillSecondary, theme, C_TEXT_S, 11.0f, false);
    topBarPillSecondary.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(topProjectContextBtn);
    styleOutlineBtn(topProjectContextBtn, C_PRIMARY);
    topProjectContextBtn.onClick = [this] { showProjectQuickSwitchMenu(); };
    addAndMakeVisible(topSubjectContextBtn);
    styleOutlineBtn(topSubjectContextBtn, C_PRIMARY);
    topSubjectContextBtn.onClick = [this] { showSubjectQuickSwitchMenu(); };


    addAndMakeVisible(uiExpertToggle);
    uiExpertToggle.setClickingTogglesState(true);
    uiExpertToggle.setToggleState(expertUi, juce::dontSendNotification);
    uiExpertToggle.onClick = [this] {
        expertUi = uiExpertToggle.getToggleState();
        if (auto* p = appProperties.getUserSettings()) {
            p->setValue("expertUi", expertUi);
            p->saveIfNeeded();
        }
        applyExpertUiVisibility();
        resized();
    };

    addAndMakeVisible(sidebarFieldsViewport);
    sidebarFieldsViewport.setScrollBarsShown(true, false);
    sidebarFieldsViewport.setScrollBarThickness(9);
    sidebarFieldsViewport.setViewedComponent(&sidebarFieldsHost, false);
    sidebarFieldsHost.setFillColour(C_BG);
    {
        auto& vsb = sidebarFieldsViewport.getVerticalScrollBar();
        vsb.setColour(juce::ScrollBar::trackColourId, C_BG);
        vsb.setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xffc5d0e2));
    }

    addAndMakeVisible(trainPreflightSidebarBadge);
    styleLabel(trainPreflightSidebarBadge, theme, C_TEXT_S, 11.0f, true);
    trainPreflightSidebarBadge.setJustificationType(juce::Justification::centredLeft);
    trainPreflightSidebarBadge.setVisible(false);

    // ─────────────────────────────────────────────────────────────────────────
    // TAB 0 鈥?Realtime setup
    // ─────────────────────────────────────────────────────────────────────────
addAndMakeVisible(realtimeTitleLabel); styleLabel(realtimeTitleLabel, theme, C_TEXT_H, 20.0f, true);
    addAndMakeVisible(realtimeStatusLabel); styleLabel(realtimeStatusLabel, theme, C_TEXT_S, FONT_UI);
    addAndMakeVisible(startBoardBtn); stylePrimaryBtn(startBoardBtn);
    addAndMakeVisible(stopBoardBtn); styleOutlineBtn(stopBoardBtn, C_DANGER);
    addAndMakeVisible(realtimeSourceCombo); styleCombo(realtimeSourceCombo);
    realtimeSourceCombo.addItem(W("\u8bad\u7ec3\u6587\u4ef6\u6570\u636e"), 3);
    realtimeSourceCombo.setSelectedId(3);
    realtimeSourceCombo.onChange = [this] {
        persistRealtimeSettings();
        updateRealtimeSourceDependentUi();
        resized();
    };
    addAndMakeVisible(realtimePlaybackLabel); styleLabel(realtimePlaybackLabel, theme, C_TEXT_S, FONT_UI);
    addAndMakeVisible(realtimePlaybackInput);
    styleField(realtimePlaybackInput, theme);
    realtimePlaybackInput.setTextToShowWhenEmpty(W("\u9009\u62e9\u9884\u5904\u7406\u8f93\u51fa\u7684 .npz \u8bad\u7ec3\u6570\u636e"), C_TEXT_S);
    realtimePlaybackInput.onFocusLost = [this] { persistRealtimeSettings(); };
    addAndMakeVisible(realtimePlaybackBrowseBtn); styleOutlineBtn(realtimePlaybackBrowseBtn, C_PRIMARY);
    realtimePlaybackBrowseBtn.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser>(
            W("\u9009\u62e9 NPZ \u8bad\u7ec3\u6570\u636e"), juce::File::getCurrentWorkingDirectory(), "*.npz");
        const int flags = juce::FileBrowserComponent::openMode
                          | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync(flags, [this](const juce::FileChooser& c) {
            if (auto r = c.getResult(); r.existsAsFile()) {
                realtimePlaybackInput.setText(r.getFullPathName());
                persistRealtimeSettings();
            }
        });
    };
    addAndMakeVisible(realtimeLiveConnLabel);
    styleLabel(realtimeLiveConnLabel, theme, C_TEXT_S, FONT_UI);
    addAndMakeVisible(realtimeLiveConnInput);
    styleField(realtimeLiveConnInput, theme);
    realtimeLiveConnInput.setTextToShowWhenEmpty(W("\u4f8b\u5982\uff1a127.0.0.1:8765 / COM3 / \u8bbe\u5907\u5e8f\u5217\u53f7"), C_TEXT_S);
    realtimeLiveConnInput.onFocusLost = [this] { persistRealtimeSettings(); };
    addAndMakeVisible(realtimeLiveConnCopyBtn);
    styleOutlineBtn(realtimeLiveConnCopyBtn, C_PRIMARY);
    realtimeLiveConnCopyBtn.onClick = [this] {
        juce::SystemClipboard::copyTextToClipboard(realtimeLiveConnInput.getText());
        setStatusMsg(W("\u5df2\u590d\u5236\u8bbe\u5907\u8fde\u63a5\u4e32\u5230\u526a\u8d34\u677f"));
    };
    addAndMakeVisible(realtimeAlignLabel);
    styleLabel(realtimeAlignLabel, theme, C_TEXT_B, 12.0f, false, juce::Justification::centredLeft);

    addAndMakeVisible(realtimeChannelsCombo); styleCombo(realtimeChannelsCombo);
    for (int ch : { 8, 16, 32, 64, 128 })
        realtimeChannelsCombo.addItem(juce::String(ch) + W(" \u901a\u9053"), ch);
    realtimeChannelsCombo.setSelectedId(8);
    realtimeChannelsCombo.onChange = [this] {
        applyRealtimeChannelPaging(true);
        persistRealtimeSettings();
        updateRealtimeOnnxAlignmentUi();
    };
    addAndMakeVisible(realtimeRateCombo); styleCombo(realtimeRateCombo);
    for (int hz : { 256, 250, 500, 1000, 2000 })
        realtimeRateCombo.addItem(juce::String(hz) + W(" Hz"), hz);
    realtimeRateCombo.setSelectedId(256);
    realtimeRateCombo.onChange = [this] {
        applyRealtimeTimeWindowFromControls();
        persistRealtimeSettings();
        updateRealtimeOnnxAlignmentUi();
    };
    addAndMakeVisible(realtimeUvRangeCombo); styleCombo(realtimeUvRangeCombo);
    // 绾靛悜婊″箙 卤half锛埪礦锛夛紱1 mV = 1000 碌V
realtimeUvRangeCombo.addItem(W("\u00b150\u03bcV"), 50);
    realtimeUvRangeCombo.addItem(W("\u00b1100\u03bcV"), 100);
    realtimeUvRangeCombo.addItem(W("\u00b11 mV"), 1000);
    realtimeUvRangeCombo.addItem(W("\u00b110 mV"), 10000);
    realtimeUvRangeCombo.addItem(W("\u00b1100 mV"), 100000);
    realtimeUvRangeCombo.setSelectedId(100);
    realtimeUvRangeCombo.onChange = [this] {
        const int id = realtimeUvRangeCombo.getSelectedId();
        if (id > 0) waveformCanvas.setVerticalHalfRangeUv((float)id);
        persistRealtimeSettings();
    };
    waveformCanvas.setVerticalHalfRangeUv(100.0f);
    addAndMakeVisible(realtimeFilterRowLabel);
    styleLabel(realtimeFilterRowLabel, theme, C_TEXT_S, FONT_UI);
    addAndMakeVisible(realtimeFilterCombo); styleCombo(realtimeFilterCombo);
    realtimeFilterCombo.addItem(W("\u539f\u59cb"), 1);
    realtimeFilterCombo.addItem(W("1-15 Hz"), 2);
    realtimeFilterCombo.addItem(W("5-35 Hz"), 3);
    realtimeFilterCombo.addItem(W("8-15 Hz"), 4);
    realtimeFilterCombo.addItem(W("\u53bb\u5e73\u5747"), 5);
    realtimeFilterCombo.setSelectedId(1);
    realtimeFilterCombo.onChange = [this] {
        applyRealtimeDisplayFilterFromCombo();
        persistRealtimeSettings();
    };
    addAndMakeVisible(realtimeMontageRowLabel);
    styleLabel(realtimeMontageRowLabel, theme, C_TEXT_S, FONT_UI);
    addAndMakeVisible(realtimeMontageCombo); styleCombo(realtimeMontageCombo);
    realtimeMontageCombo.addItem(W("\u5e8f\u53f7"), 1);
    realtimeMontageCombo.addItem(W("10-20"), 2);
    realtimeMontageCombo.addItem(W("10-10"), 3);
    realtimeMontageCombo.addItem(W("10-5"), 4);
    realtimeMontageCombo.setSelectedId(2);
    realtimeMontageCombo.onChange = [this] {
        applyRealtimeMontageFromCombo();
        persistRealtimeSettings();
    };
    addAndMakeVisible(realtimeWinLenCombo); styleCombo(realtimeWinLenCombo);
    for (auto& [t, i] : std::initializer_list<std::pair<juce::String, int>>{
             {W("\u7a97\u957f 128"), 128}, {W("\u7a97\u957f 256"), 256}, {W("\u7a97\u957f 512"), 512}})
        realtimeWinLenCombo.addItem(t, i);
    realtimeWinLenCombo.setSelectedId(256);
    realtimeWinLenCombo.onChange = [this] {
        applyRealtimeTimeWindowFromControls();
        persistRealtimeSettings();
        updateRealtimeOnnxAlignmentUi();
    };
    addAndMakeVisible(realtimeNumWinCombo); styleCombo(realtimeNumWinCombo);
    for (auto& [t, i] : std::initializer_list<std::pair<juce::String, int>>{
             {W("1 \u7a97"), 1}, {W("2 \u7a97"), 2}, {W("4 \u7a97"), 4}, {W("8 \u7a97"), 8}, {W("16 \u7a97"), 16}})
        realtimeNumWinCombo.addItem(t, i);
    realtimeNumWinCombo.setSelectedId(1);
    realtimeNumWinCombo.onChange = [this] {
        applyRealtimeTimeWindowFromControls();
        persistRealtimeSettings();
    };
    addAndMakeVisible(realtimeOnnxInferToggle);
    realtimeOnnxInferToggle.setClickingTogglesState(true);
    realtimeOnnxInferToggle.setColour(juce::ToggleButton::textColourId, C_TEXT_B);
    addAndMakeVisible(realtimeRecordBtn); stylePrimaryBtn(realtimeRecordBtn);
    addAndMakeVisible(waveformCanvas);
    applyRealtimeTimeWindowFromControls();
    applyRealtimeChannelPaging(true);

    startBoardBtn.onClick = [this] {
        realtimeSourceCombo.setSelectedId(3, juce::dontSendNotification);
        AcquisitionMode mode = AcquisitionMode::Playback;

        const int nCh = realtimeChannelsCombo.getSelectedId() > 0 ? realtimeChannelsCombo.getSelectedId() : 8;
        const int sr  = realtimeRateCombo.getSelectedId() > 0 ? realtimeRateCombo.getSelectedId() : 256;
        persistRealtimeSettings();
        const juce::String liveHint;
        if (!BoardManager::getInstance().configure(
                mode, nCh, sr, juce::String())) {
            setStatusMsg(W("\u65e0\u6cd5\u52a0\u8f7d\u8bad\u7ec3\u6587\u4ef6\uff0c\u8bf7\u5148\u505c\u6b62\u5f53\u524d\u6587\u4ef6\u9884\u89c8"), true);
            return;
        }
        {
            juce::File     pf(realtimePlaybackInput.getText().trim());
            juce::String   perr;
            if (!pf.existsAsFile())
            {
                setStatusMsg(W("\u8bf7\u5148\u9009\u62e9\u4e00\u4e2a\u6709\u6548\u7684\u8bad\u7ec3\u6587\u4ef6"), true);
                return;
            }
            if (!BoardManager::getInstance().loadPlaybackNpz(pf, nCh, perr))
            {
                setStatusMsg(W("\u8bad\u7ec3\u6587\u4ef6\u52a0\u8f7d\u5931\u8d25\uff1a") + perr, true);
                return;
            }
        }
        if (BoardManager::getInstance().startStream()) {
            NR_LOGI(W("\u8bad\u7ec3\u6587\u4ef6"),
                    W("\u52a0\u8f7d\u8bad\u7ec3\u6587\u4ef6 mode=") + acquisitionModeToKey(mode)
                        + W(" ch=") + juce::String(nCh)
                        + W(" sr=") + juce::String(sr) + W("Hz"));
            nerou::domain::Session session;
            session.id = makeEntityId("session");
            session.projectId = resolveCurrentProjectId();
            session.subjectId = resolveCurrentSubjectId();
            session.deviceType = acquisitionModeToKey(mode);
            session.deviceSerial = mode == AcquisitionMode::LiveBoard ? liveHint : juce::String();
            session.channelCount = nCh;
            session.sampleRate = sr;
            session.montageType = realtimeMontageCombo.getText();
            session.displayFilter = realtimeFilterCombo.getText();
            session.recordFilterMode = "raw";
            session.connectionInfo = mode == AcquisitionMode::Playback
                ? realtimePlaybackInput.getText().trim()
                : liveHint;
            session.startedAt = juce::Time::getCurrentTime();
            session.status = "running";
            acquisitionService.setActiveSession(session);
            acquisitionService.startAcquisition();

            realtimeSourceCombo.setEnabled(false);
            realtimeChannelsCombo.setEnabled(false);
            realtimeRateCombo.setEnabled(false);
            realtimePlaybackInput.setEnabled(false);
            realtimePlaybackBrowseBtn.setEnabled(false);
            realtimeLiveConnInput.setEnabled(false);
            applyRealtimeChannelPaging(true);
            waveformCanvas.setRunning(true);
            waveformCanvas.setLoadSheddingLevel(0);
            realtimeLoadSheddingLevel = 0;
            realtimeInferCooldownBase = 8;
            realtimeInferCooldown = 0;
            healthLastEvalMs = 0;
            healthStableWindows = 0;
            realtimeRecordBtn.setButtonText(W("\u5bfc\u51fa\u9884\u89c8 NPZ"));
            realtimeStatusLabel.setText(composeRealtimeIdleStatus(), juce::dontSendNotification);
            realtimeStatusLabel.setColour(juce::Label::textColourId, C_SUCCESS);
            setStatusMsg(W("\u8bad\u7ec3\u6587\u4ef6\u5df2\u52a0\u8f7d\uff0c\u53ef\u8fdb\u5165\u9884\u5904\u7406\u6216\u5206\u7c7b\u8bad\u7ec3"));
        }
        else
        {
            setStatusMsg(W("\u542f\u52a8\u91c7\u96c6\u5931\u8d25"), true);
            NR_LOGE(W("\u91c7\u96c6"), W("BoardManager::startStream() \u8fd4\u56de\u5931\u8d25"));
        }
    };
    stopBoardBtn.onClick = [this] {
        BoardManager::getInstance().stopStream();
        acquisitionService.stopAcquisition();
        NR_LOGI(W("\u8bad\u7ec3\u6587\u4ef6"), W("\u7528\u6237\u505c\u6b62\u8bad\u7ec3\u6587\u4ef6\u9884\u89c8"));
        if (auto* activeSession = acquisitionService.getActiveSession())
        {
            auto completedSession = *activeSession;
            completedSession.endedAt = juce::Time::getCurrentTime();
            completedSession.durationSec =
                juce::jmax(0.0, (double) (completedSession.endedAt.toMilliseconds()
                    - completedSession.startedAt.toMilliseconds()) / 1000.0);
            completedSession.status = "stopped";
            acquisitionService.setActiveSession(completedSession);
        }
        persistRealtimeSettings();
        realtimeSourceCombo.setEnabled(true);
        realtimeChannelsCombo.setEnabled(true);
        realtimeRateCombo.setEnabled(true);
        realtimePlaybackInput.setEnabled(true);
        realtimePlaybackBrowseBtn.setEnabled(true);
        realtimeLiveConnInput.setEnabled(true);
        applyRealtimeChannelPaging(true);
        waveformCanvas.setRunning(false);
        waveformCanvas.setLoadSheddingLevel(0);
        realtimeLoadSheddingLevel = 0;
        realtimeInferCooldownBase = 8;
        realtimeInferCooldown = 0;
        realtimeRecordBtn.setButtonText(W("\u5bfc\u51fa\u9884\u89c8 NPZ"));
        realtimeStatusLabel.setText(W("\u72b6\u6001\uff1a\u7b49\u5f85\u8bad\u7ec3\u6587\u4ef6"), juce::dontSendNotification);
        realtimeStatusLabel.setColour(juce::Label::textColourId, C_TEXT_S);
    };

    realtimeRecordBtn.onClick = [this] {
        if (!BoardManager::getInstance().isStreaming()) {
            setStatusMsg(W("\u8bf7\u5148\u300c\u542f\u52a8\u8bbe\u5907\u300d\u518d\u5f00\u59cb\u91c7\u96c6"), true);
            return;
        }
        if (BoardManager::getInstance().isRecordingArmed()) {
            BoardManager::getInstance().cancelRecording();
            realtimeRecordBtn.setButtonText(W("\u5bfc\u51fa\u9884\u89c8 NPZ"));
            realtimeStatusLabel.setText(composeRealtimeIdleStatus(), juce::dontSendNotification);
            realtimeStatusLabel.setColour(juce::Label::textColourId, C_SUCCESS);
            return;
        }
        const int winLen = realtimeWinLenCombo.getSelectedId() > 0 ? realtimeWinLenCombo.getSelectedId() : 256;
        const int numWin = realtimeNumWinCombo.getSelectedId() > 0 ? realtimeNumWinCombo.getSelectedId() : 1;
        BoardManager::getInstance().armRecording(winLen, numWin);
        realtimeRecordBtn.setButtonText(W("\u53d6\u6d88\u91c7\u96c6"));
        setStatusMsg(W("\u6b63\u5728\u7f13\u51b2 ") + juce::String(numWin) + W("\u00d7") + juce::String(winLen) + W(" \u91c7\u6837\u70b9"));
    };

    // ─────────────────────────────────────────────────────────────────────────
    // TAB 1 鈥?Training setup
    // ─────────────────────────────────────────────────────────────────────────
auto addParamRow = [&](juce::Label& lbl, juce::Component& fld, juce::TextButton* btn = nullptr)
    {
        sidebarFieldsHost.addAndMakeVisible(lbl);
        styleLabel(lbl, theme, C_TEXT_S, FONT_UI);
        sidebarFieldsHost.addAndMakeVisible(fld);
        if (btn)
            sidebarFieldsHost.addAndMakeVisible(*btn);
    };
    auto addFieldRow = [&](juce::Label& lbl, juce::TextEditor& ed,
                            const juce::String& ph, juce::TextButton* btn = nullptr)
    {
        addParamRow(lbl, ed, btn);
        styleField(ed, theme);
        ed.setTextToShowWhenEmpty(ph, C_TEXT_S);
    };

    // Section header labels 鈥?styled as small uppercase captions
auto addSectionHeader = [&](juce::Label& lbl) {
        sidebarFieldsHost.addAndMakeVisible(lbl);
        lbl.setFont(theme.cjkFont(10.5f).withStyle(juce::Font::bold));
        lbl.setColour(juce::Label::textColourId, C_TEXT_S.withAlpha(0.75f));
        lbl.setJustificationType(juce::Justification::centredLeft);
    };
    addSectionHeader(trainSec_Identity);
    addSectionHeader(trainSec_Data);
    addSectionHeader(trainSec_Params);
    addSectionHeader(trainSec_Output);

    sidebarFieldsHost.addAndMakeVisible(trainDataCountLabel);
    styleLabel(trainDataCountLabel, theme, C_TEXT_S, 11.0f, false, juce::Justification::centredLeft);

    addParamRow(trainTemplateSecLabel, trainTemplateCombo, nullptr);
    styleCombo(trainTemplateCombo);
    trainTemplateCombo.onChange = [this] { applyTrainTemplateFromCombo(); };

    addFieldRow(nameSecLabel,  nameInput,     juce::String::fromUTF8(u8"\u4f8b\u5982\uff1a") + "eegnet_session_01");
    addFieldRow(dataSecLabel,  dataPathInput, juce::String::fromUTF8(u8"\u4f8b\u5982\uff1a") + "data/npz/",  &browseDirBtn);
    addFieldRow(epochSecLabel, epochInput,    juce::String::fromUTF8(u8"\u4f8b\u5982\uff1a") + "30");
    addFieldRow(classCountSecLabel, classCountInput, juce::String::fromUTF8(u8"\u4f8b\u5982\uff1a") + "4");
    addParamRow(batchSecLabel, batchCombo);
    addParamRow(lrSecLabel,    lrCombo);
    addFieldRow(saveSecLabel,  savePathInput, W("\u4f8b\u5982\uff1aonnx/deploy/"), &browseSaveBtn);


    styleOutlineBtn(browseDirBtn, C_PRIMARY);
    styleOutlineBtn(browseSaveBtn, C_PRIMARY);

    browseDirBtn.onClick = [this] {
        int flags = juce::FileBrowserComponent::openMode
                    | juce::FileBrowserComponent::canSelectDirectories;
        fileChooser = std::make_unique<juce::FileChooser>(W("\u9009\u62e9\u6570\u636e\u96c6\u76ee\u5f55"),
                                                          juce::File::getCurrentWorkingDirectory(), "*");
        fileChooser->launchAsync(flags, [this](const juce::FileChooser& c) {
            if (auto r = c.getResult(); r.exists())
            {
                dataPathInput.setText(r.getFullPathName());
                persistTrainingFields();
                refreshTrainPreflight();
            }
        });
    };
    dataPathInput.onFocusLost = [this] {
        if (currentTab == Tab::Training)
        {
            persistTrainingFields();
            refreshTrainPreflight();
        }
    };
    dataPathInput.onReturnKey = [this] {
        if (currentTab == Tab::Training)
        {
            persistTrainingFields();
            refreshTrainPreflight();
        }
    };
    browseSaveBtn.onClick = [this] {
        int flags = juce::FileBrowserComponent::openMode
                    | juce::FileBrowserComponent::canSelectDirectories;
        fileChooser = std::make_unique<juce::FileChooser>(W("\u9009\u62e9\u4fdd\u5b58\u76ee\u5f55"),
                                                          juce::File::getCurrentWorkingDirectory(), "*");
        fileChooser->launchAsync(flags, [this](const juce::FileChooser& c) {
            if (auto r = c.getResult(); r.exists())
            {
                savePathInput.setText(r.getFullPathName());
                persistTrainingFields();
                rebuildInferOnnxCombo();
                refreshTrainPreflight();
            }
        });
    };
    savePathInput.onFocusLost = [this] {
        persistTrainingFields();
        rebuildInferOnnxCombo();
        refreshTrainPreflight();
    };

    styleCombo(batchCombo);
    for (auto v : { 16, 32, 64, 128, 256 }) batchCombo.addItem(juce::String(v), v);
    batchCombo.setSelectedId(64);
    batchCombo.onChange = [this] { persistTrainingFields(); };

    styleCombo(lrCombo);
    for (auto& [txt, id] : std::initializer_list<std::pair<const char*, int>>{
        {"0.0001",1},{"0.0005",2},{"0.001",3},{"0.005",4},{"0.01",5}})
        lrCombo.addItem(txt, id);
    lrCombo.setSelectedId(2);
    lrCombo.onChange = [this] { persistTrainingFields(); };

    if (auto* p = appProperties.getUserSettings())
    {
        const int tb = (int) p->getIntValue("trainBatchId", 64);
        bool batchOk = false;
        for (int v : { 16, 32, 64, 128, 256 })
            if (v == tb)
            {
                batchCombo.setSelectedId(tb, juce::dontSendNotification);
                batchOk = true;
                break;
            }
        if (!batchOk)
            batchCombo.setSelectedId(64, juce::dontSendNotification);

        const int tl = (int) p->getIntValue("trainLrId", 2);
        if (tl >= 1 && tl <= 5)
            lrCombo.setSelectedId(tl, juce::dontSendNotification);
    }

    nameInput.onFocusLost = [this] {
        if (currentTab == Tab::Training)
        {
            persistTrainingFields();
            rebuildTrainTemplateCombo();
        }
    };
    epochInput.onFocusLost = [this] {
        if (currentTab == Tab::Training)
        {
            // P2 Validation: Clamp to valid range immediately on focus lost
int val = epochInput.getText().getIntValue();
            if (val < 1) val = 1;
            if (val > 10000) val = 10000;
            epochInput.setText(juce::String(val), false);

            persistTrainingFields();
            refreshTrainPreflight();
        }
    };
    classCountInput.onFocusLost = [this] {
        if (currentTab == Tab::Training)
        {
            // P2 Validation: Clamp to valid range immediately on focus lost
int val = classCountInput.getText().getIntValue();
            if (val < 2) val = 2;
            if (val > 100) val = 100;
            classCountInput.setText(juce::String(val), false);

            persistTrainingFields();
            refreshTrainPreflight();
        }
    };

    // Center - training
addAndMakeVisible(pageTitleLabel);  styleLabel(pageTitleLabel, theme, C_TEXT_H, 20.0f, true);
    addAndMakeVisible(statusChipLabel); styleLabel(statusChipLabel, theme, C_SUCCESS, FONT_UI, true, juce::Justification::centred);
    addAndMakeVisible(elapsedLabel);    styleLabel(elapsedLabel, theme, C_TEXT_S, FONT_UI_COMPACT, false, juce::Justification::centred);
    addAndMakeVisible(etaLabel);        styleLabel(etaLabel, theme, C_TEXT_S, FONT_UI_COMPACT, false, juce::Justification::centred);

    // 须先于进度条/图加入，否则 z-order 压在整页之上会出现大块白蒙层」遮
addAndMakeVisible(trainPreflightTitle);
    styleLabel(trainPreflightTitle, theme, C_TEXT_S, 12.0f, true);
    trainPreflightTitle.setOpaque(false);
    trainPreflightTitle.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(trainPreflightDetail);
    trainPreflightDetail.setFont(theme.cjkFont(11.5f));
    trainPreflightDetail.setJustificationType(juce::Justification::topLeft);
    trainPreflightDetail.setOpaque(false);
    trainPreflightDetail.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(trainPreflightRefreshBtn);
    styleOutlineBtn(trainPreflightRefreshBtn, C_PRIMARY);
    trainPreflightRefreshBtn.onClick = [this] { refreshTrainPreflight(); };

    addAndMakeVisible(progressSectionLabel); styleLabel(progressSectionLabel, theme, C_TEXT_H, 14.0f, true);
    addAndMakeVisible(epochBarLabel);        styleLabel(epochBarLabel, theme, C_TEXT_S, FONT_UI_COMPACT);
    addAndMakeVisible(epochProgressBar);

    for (auto* l : { &epochBarVal, &batchBarVal, &sampleBarVal }) {
        addAndMakeVisible(*l);
        styleLabel(*l, theme, C_TEXT_B, FONT_UI_COMPACT, true, juce::Justification::centredRight);
    }

    addAndMakeVisible(lossCanvas);
    addAndMakeVisible(accCanvas);

    addAndMakeVisible(trainLogTitle); styleLabel(trainLogTitle, theme, C_TEXT_H, 14.0f, true);
    addAndMakeVisible(trainLog);
    trainLog.setMultiLine(true); trainLog.setReadOnly(true);
    trainLog.setScrollbarsShown(true); trainLog.setCaretVisible(false);
    trainLog.setColour(juce::TextEditor::backgroundColourId, C_LOG_BG);
    trainLog.setColour(juce::TextEditor::textColourId,       C_LOG_FG);
    trainLog.setColour(juce::TextEditor::outlineColourId,    C_LOG_OUTLINE);
    trainLog.setFont(theme.cjkFont(FONT_LOG));

    addAndMakeVisible(startBtn);     stylePrimaryBtn(startBtn);
    addAndMakeVisible(pauseBtn);     stylePrimaryBtn(pauseBtn);
    addAndMakeVisible(stopBtn);      styleOutlineBtn(stopBtn, C_DANGER);
    addAndMakeVisible(saveLogBtn);   styleOutlineBtn(saveLogBtn, C_PRIMARY);
    addAndMakeVisible(saveChartBtn); styleOutlineBtn(saveChartBtn, C_PRIMARY);
    addAndMakeVisible(clearLogBtn);  styleOutlineBtn(clearLogBtn, C_DANGER);

    startBtn.onClick     = [this] { startTraining(); };
    pauseBtn.onClick     = [this] { pauseTraining(); };
    stopBtn.onClick      = [this] { stopTraining(); };
    clearLogBtn.onClick  = [this] {
        if (trainLog.getText().isEmpty()) return;
        juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::QuestionIcon,
            W("\u786e\u8ba4\u6e05\u7a7a\u65e5\u5fd7"),
            W("\u6e05\u7a7a\u540e\u65e5\u5fd7\u65e0\u6cd5\u6062\u590d\u3002\u5efa\u8bae\u5148\u5bfc\u51fa\u5230\u684c\u9762\u4fdd\u5b58\u3002\n\u786e\u8ba4\u6e05\u7a7a\uff1f"),
            W("\u6e05\u7a7a"),
            W("\u53d6\u6d88"),
            nullptr,
            juce::ModalCallbackFunction::create([this](int result) {
                if (result == 1) {
                    trainLog.setText({});
                    nerou::ui::SnackbarManager::getInstance().show(
                        W("\u65e5\u5fd7\u5df2\u6e05\u7a7a"),
                        nerou::ui::MaterialSnackbar::Duration::Short,
                        nerou::ui::MaterialSnackbar::Type::Default);
                }
            }));
    };
    saveLogBtn.onClick   = [this] {
        juce::File f = juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                           .getChildFile(W("\u795e\u7ecf\u8fd0\u884c\u65f6_\u8bad\u7ec3\u65e5\u5fd7.txt"));
        if (f.replaceWithText(trainLog.getText())) {
            updateLog(W("[\u4fe1\u606f] \u65e5\u5fd7\u5df2\u5bfc\u51fa\u5230\u684c\u9762"), trainLog);
            nerou::ui::SnackbarManager::getInstance().show(
                W("\u65e5\u5fd7\u5df2\u5bfc\u51fa: ") + f.getFullPathName(),
                nerou::ui::MaterialSnackbar::Duration::Long,
                nerou::ui::MaterialSnackbar::Type::Success);
        } else {
            nerou::ui::SnackbarManager::getInstance().show(
                W("\u65e5\u5fd7\u5bfc\u51fa\u5931\u8d25\uff0c\u8bf7\u68c0\u67e5\u684c\u9762\u5199\u5165\u6743\u9650"),
                nerou::ui::MaterialSnackbar::Duration::Long,
                nerou::ui::MaterialSnackbar::Type::Error);
        }
    };
    saveChartBtn.onClick = [this] {
        juce::File f = juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                           .getChildFile(W("\u795e\u7ecf\u8fd0\u884c\u65f6_\u8bad\u7ec3\u56fe\u8868.png"));
        
        int w = lossCanvas.getWidth() + accCanvas.getWidth() + 20;
        int h = juce::jmax(lossCanvas.getHeight(), accCanvas.getHeight());
        if (w <= 20 || h <= 0) {
            updateLog(W("[\u9519\u8bef] \u56fe\u8868\u5c1a\u672a\u521d\u59cb\u5316"), trainLog);
            return;
        }

        juce::Image combined(juce::Image::ARGB, w, h, true);
        juce::Graphics g(combined);
        g.fillAll(juce::Colours::white);
        g.drawImageAt(lossCanvas.createComponentSnapshot(lossCanvas.getLocalBounds(), false), 0, 0);
        g.drawImageAt(accCanvas.createComponentSnapshot(accCanvas.getLocalBounds(), false), lossCanvas.getWidth() + 20, 0);

        juce::FileOutputStream stream(f);
        if (stream.openedOk()) {
            stream.setPosition(0);
            stream.truncate();
            juce::PNGImageFormat().writeImageToStream(combined, stream);
            updateLog(W("[\u4fe1\u606f] \u56fe\u8868\u5feb\u7167\u5df2\u4fdd\u5b58\u5230\u684c\u9762"), trainLog);
            nerou::ui::SnackbarManager::getInstance().show(
                W("\u56fe\u8868\u5df2\u4fdd\u5b58: ") + f.getFullPathName(),
                nerou::ui::MaterialSnackbar::Duration::Long,
                nerou::ui::MaterialSnackbar::Type::Success);
        } else {
            updateLog(W("[\u9519\u8bef] \u56fe\u8868\u4fdd\u5b58\u5931\u8d25"), trainLog);
            nerou::ui::SnackbarManager::getInstance().show(
                W("\u56fe\u8868\u4fdd\u5b58\u5931\u8d25\uff0c\u8bf7\u68c0\u67e5\u684c\u9762\u5199\u5165\u6743\u9650"),
                nerou::ui::MaterialSnackbar::Duration::Long,
                nerou::ui::MaterialSnackbar::Type::Error);
        }
    };

    stopBtn.setEnabled(false);
    pauseBtn.setEnabled(false);

    // Training bridge callback（含异步环境[ENV] 行，不再覆盖回调
trainBridge.setLogCallback([safeThis = juce::Component::SafePointer<MainComponent>(this)](const TrainingLogEvent& ev) {
        auto* self = safeThis.getComponent();
        if (self == nullptr) return;  // MainComponent already destroyed

        if (ev.kind == TrainingLogEvent::Kind::RawText && ev.rawJson.startsWith("[ENV]"))
        {
            juce::MessageManager::callAsync([safeThis, line = ev.rawJson] {
                auto* s = safeThis.getComponent();
                if (s == nullptr) return;
                const bool isMissing = line.contains("MISSING") || line.contains("INCOMPLETE");
                const bool isOk      = line.contains("ALL OK");
                s->updateLog(W("[\u73af\u5883\u68c0\u67e5] ") + line.fromFirstOccurrenceOf("[ENV] ", false, false), s->trainLog);
                if (isOk)
                    s->setStatusMsg(W("Python \u73af\u5883\u5c31\u7eea\uff0c\u53ef\u4ee5\u5f00\u59cb\u8bad\u7ec3"));
                if (isMissing)
                    s->setStatusMsg(W("\u7f3a\u5c11\u4f9d\u8d56\u5305\uff0c\u8bf7\u8fd0\u884c pip install -r requirements.txt"), true);
            });
            return;
        }

        if (ev.kind == TrainingLogEvent::Kind::RawText)
        {
            const juce::ScopedLock sl(self->trainLogLock);
            self->trainPendingLogs.add(ev.rawJson);
            return;
        }

        if (ev.kind == TrainingLogEvent::Kind::PrepProgress)
            return;

        if (ev.kind == TrainingLogEvent::Kind::TrainingMetrics && ev.epoch > 0)
        {
            const int ep = ev.epoch;
            const float ls = ev.loss;
            const float ac = ev.acc;
            const float vl = ev.valLoss;
            const float va = ev.valAcc;
            const float lrate = ev.lr;
            juce::MessageManager::callAsync([safeThis, ep, ls, ac]() {
                if (auto* s = safeThis.getComponent())
                {
                    s->lossCanvas.addDataPoint(ep, ls, ac);
                    s->accCanvas.addDataPoint(ep, ls, ac);
                }
            });
            self->currentEpoch.store(ep);
            self->currentLoss.store(ls);
            self->currentAcc.store(ac);
            int total = self->totalEpochs.load();
            // ── PipelineStore 训练进度 ──────────────────────────
            nerou::app::PipelineStore::getInstance()
                .notifyTrainEpoch(ep, total, ac, va, ls);
            if (total > 0)
                self->epochProgressAtomic.store((double)ep / (double)total, std::memory_order_relaxed);
            const juce::ScopedLock sl(self->trainLogLock);
            // 显示训练 + 验证双指+ 当前学习
juce::String logLine = W("[\u7b2c ") + intZeroPadded(ep, 3) + W(" / ")
                                 + intZeroPadded(total, 3) + W(" \u8f6e]  Loss\uff1a") + juce::String(ls, 4)
                                 + W("  |  Acc\uff1a") + juce::String(ac, 4);
            if (vl >= 0.f)
                logLine += W("  |  Val Loss\uff1a") + juce::String(vl, 4)
                         + W("  |  Val Acc\uff1a") + juce::String(va, 4);
            if (lrate >= 0.f)
                logLine += W("  |  LR: ") + juce::String(lrate, 6);
            self->trainPendingLogs.add(logLine);
            return;
        }

        if (ev.kind == TrainingLogEvent::Kind::TrainingMetrics)
        {
            const juce::ScopedLock sl(self->trainLogLock);
            self->trainPendingLogs.add(ev.rawJson);
        }
    });


    // ─────────────────────────────────────────────────────────────────────────
    // TAB 2 鈥?Preprocessing
    // ─────────────────────────────────────────────────────────────────────────
addAndMakeVisible(prepTitleLabel); styleLabel(prepTitleLabel, theme, C_TEXT_H, 20.0f, true);

    auto addPrepField = [&](juce::Label& lbl, juce::TextEditor& ed,
                             const juce::String& ph, juce::TextButton* btn = nullptr)
    {
        sidebarFieldsHost.addAndMakeVisible(lbl);
        styleLabel(lbl, theme, C_TEXT_S, FONT_UI);
        sidebarFieldsHost.addAndMakeVisible(ed);
        styleField(ed, theme);
        ed.setTextToShowWhenEmpty(ph, C_TEXT_S);
        if (btn)
        {
            sidebarFieldsHost.addAndMakeVisible(*btn);
            styleOutlineBtn(*btn, C_PRIMARY);
        }
    };

    addPrepField(prepRawLabel,  prepRawInput,  W("\u4f8b\u5982\uff1adata/raw/"),      &prepBrowseRaw);
    addPrepField(prepOutLabel,  prepOutInput,  W("\u4f8b\u5982\uff1adata/npz/"),      &prepBrowseOut);
    addPrepField(prepLowLabel,  prepLowInput,  W("\u4f8b\u5982\uff1a1"));
    addPrepField(prepHighLabel, prepHighInput, W("\u4f8b\u5982\uff1a40"));

    sidebarFieldsHost.addAndMakeVisible(prepSrLabel);
    styleLabel(prepSrLabel, theme, C_TEXT_S, FONT_UI);
    sidebarFieldsHost.addAndMakeVisible(prepSrCombo);
    styleCombo(prepSrCombo);
    for (auto& [t, i] : std::initializer_list<std::pair<juce::String, int>>{
        {W("128 Hz"),128},{W("256 Hz"),256},{W("512 Hz"),512},{W("1000 Hz"),1000}})
        prepSrCombo.addItem(t, i);
    prepSrCombo.setSelectedId(256);

    sidebarFieldsHost.addAndMakeVisible(prepChLabel);
    styleLabel(prepChLabel, theme, C_TEXT_S, FONT_UI);
    sidebarFieldsHost.addAndMakeVisible(prepChCombo);
    styleCombo(prepChCombo);
    for (auto& [t, i] : std::initializer_list<std::pair<juce::String, int>>{
        {W("8 \u901a\u9053"),8},{W("16 \u901a\u9053"),16},{W("32 \u901a\u9053"),32},{W("64 \u901a\u9053"),64},{W("128 \u901a\u9053"),128}})
        prepChCombo.addItem(t, i);
    prepChCombo.setSelectedId(64);

    if (auto* p = appProperties.getUserSettings())
    {
        prepRawInput.setText(p->getValue("prepRawPath", ""), false);
        prepOutInput.setText(p->getValue("prepOutPath", ""), false);
        prepLowInput.setText(p->getValue("prepLowHz", "1"), false);
        prepHighInput.setText(p->getValue("prepHighHz", "40"), false);
        const int psr = (int) p->getIntValue("prepSrHz", 256);
        if (psr == 128 || psr == 256 || psr == 512 || psr == 1000)
            prepSrCombo.setSelectedId(psr, juce::dontSendNotification);
        const int pch = (int) p->getIntValue("prepCh", 64);
        if (pch == 8 || pch == 16 || pch == 32 || pch == 64 || pch == 128)
            prepChCombo.setSelectedId(pch, juce::dontSendNotification);
    }

    addAndMakeVisible(prepStartBtn);
    stylePrimaryBtn(prepStartBtn);
    addAndMakeVisible(prepClearBtn);
    styleOutlineBtn(prepClearBtn, C_DANGER);
    sidebarFieldsHost.addAndMakeVisible(prepSyncTrainBtn);
    styleOutlineBtn(prepSyncTrainBtn, C_PRIMARY);

    addAndMakeVisible(prepStatusLabel); styleLabel(prepStatusLabel, theme, C_TEXT_S, 14.0f, true, juce::Justification::centred);
    addAndMakeVisible(prepLogTitle);    styleLabel(prepLogTitle, theme, C_TEXT_H, 14.0f, true);
    addAndMakeVisible(prepProgressBar);
    addAndMakeVisible(prepLog);
    prepLog.setMultiLine(true); prepLog.setReadOnly(true);
    prepLog.setScrollbarsShown(true); prepLog.setCaretVisible(false);
    prepLog.setColour(juce::TextEditor::backgroundColourId, C_LOG_BG);
    prepLog.setColour(juce::TextEditor::textColourId,       C_LOG_FG);
    prepLog.setColour(juce::TextEditor::outlineColourId,    C_LOG_OUTLINE);
    prepLog.setFont(theme.cjkFont(FONT_LOG));

    prepBrowseRaw.onClick = [this] {
        int flags = juce::FileBrowserComponent::openMode
                    | juce::FileBrowserComponent::canSelectDirectories;
        fileChooser = std::make_unique<juce::FileChooser>(W("\u9009\u62e9\u539f\u59cb\u6570\u636e\u76ee\u5f55"),
                                                          juce::File::getCurrentWorkingDirectory(), "*");
        fileChooser->launchAsync(flags, [this](const juce::FileChooser& c) {
            if (auto r = c.getResult(); r.exists())
            {
                prepRawInput.setText(r.getFullPathName());
                persistPrepSettings();
            }
        });
    };
    prepBrowseOut.onClick = [this] {
        int flags = juce::FileBrowserComponent::openMode
                    | juce::FileBrowserComponent::canSelectDirectories;
        fileChooser = std::make_unique<juce::FileChooser>(W("\u9009\u62e9 NPZ \u8f93\u51fa\u76ee\u5f55"),
                                                          juce::File::getCurrentWorkingDirectory(), "*");
        fileChooser->launchAsync(flags, [this](const juce::FileChooser& c) {
            if (auto r = c.getResult(); r.exists())
            {
                prepOutInput.setText(r.getFullPathName());
                persistPrepSettings();
            }
        });
    };
    prepStartBtn.onClick  = [this] { startPreprocess(); };
    prepClearBtn.onClick  = [this] { prepLog.setText({}); };
    prepSyncTrainBtn.onClick = [this] {
        persistPrepSettings();
        const juce::String out = prepOutInput.getText().trim();
        dataPathInput.setText(out);
        persistTrainingFields();
        updateLog(W("[\u9884\u5904\u7406] \u5df2\u5c06\u300c\u8f93\u51fa\u76ee\u5f55\u300d\u540c\u6b65\u5230\u300c\u8bad\u7ec3\u914d\u7f6e -> \u6570\u636e\u96c6\u8def\u5f84\u300d"), prepLog);
        switchTab(Tab::Training);
    };

    prepRawInput.onFocusLost  = [this] { persistPrepSettings(); };
    prepOutInput.onFocusLost  = [this] { persistPrepSettings(); };
    prepLowInput.onFocusLost  = [this] { persistPrepSettings(); };
    prepHighInput.onFocusLost = [this] { persistPrepSettings(); };
    prepSrCombo.onChange      = [this] { persistPrepSettings(); };
    prepChCombo.onChange      = [this] { persistPrepSettings(); };

    prepBridge.setLogCallback([safeThis = juce::Component::SafePointer<MainComponent>(this)](const TrainingLogEvent& ev) {
        auto* self = safeThis.getComponent();
        if (self == nullptr) return;

        if (ev.kind == TrainingLogEvent::Kind::PrepProgress)
        {
            if (ev.prepTotal > 0)
                self->prepProgressAtomic.store((double)ev.prepDone / (double)ev.prepTotal,
                                         std::memory_order_relaxed);
            const juce::ScopedLock sl(self->prepLogLock);
            self->prepPendingLogs.add(ev.rawJson.isNotEmpty()
                                    ? ev.rawJson
                                    : (W("[\u8fdb\u5ea6] ") + juce::String(ev.prepDone) + W(" / ")
                                       + juce::String(ev.prepTotal)));
            return;
        }

        const juce::ScopedLock sl(self->prepLogLock);
        self->prepPendingLogs.add(ev.rawJson.isNotEmpty() ? ev.rawJson : W("[\u9884\u5904\u7406]"));
    });

    // ─────────────────────────────────────────────────────────────────────────
    // TAB 3 鈥?Inference
    // ─────────────────────────────────────────────────────────────────────────
addAndMakeVisible(inferTitleLabel); styleLabel(inferTitleLabel, theme, C_TEXT_H, 20.0f, true);

    addAndMakeVisible(inferOnnxPickLabel); styleLabel(inferOnnxPickLabel, theme, C_TEXT_S, FONT_UI);
    addAndMakeVisible(inferOnnxCombo);     styleCombo(inferOnnxCombo);
    inferOnnxCombo.onChange = [this] { applyInferOnnxFromCombo(); };

    auto addInferField = [&](juce::Label& lbl, juce::TextEditor& ed,
                              const juce::String& ph, juce::TextButton* btn = nullptr)
    {
        addAndMakeVisible(lbl); styleLabel(lbl, theme, C_TEXT_S, FONT_UI);
        addAndMakeVisible(ed);  styleField(ed, theme);
        ed.setTextToShowWhenEmpty(ph, C_TEXT_S);
        if (btn) { addAndMakeVisible(*btn); styleOutlineBtn(*btn, C_PRIMARY); }
    };
    addInferField(inferModelLabel, inferModelInput, W("\u4f8b\u5982\uff1aonnx/deploy/eegnet.onnx"), &inferBrowseModel);
    addInferField(inferDataLabel,  inferDataInput,  W("\u4f8b\u5982\uff1adata/npz/test_001.npz \u6216 golden_samples/input_normal.npy"),   &inferBrowseData);
    // 统一使用 styleField 设定C_TEXT_B，不再单独覆盖为 C_TEXT_H
addAndMakeVisible(inferLoadBtn); stylePrimaryBtn(inferLoadBtn);
    addAndMakeVisible(inferUseLastExportBtn); styleOutlineBtn(inferUseLastExportBtn, C_PRIMARY);
    addAndMakeVisible(inferRunBtn);  stylePrimaryBtn(inferRunBtn);
    addAndMakeVisible(inferClearBtn); styleOutlineBtn(inferClearBtn, C_DANGER);

    addAndMakeVisible(inferResultTitle); styleLabel(inferResultTitle, theme, C_TEXT_H, 14.0f, true);
    addAndMakeVisible(inferStatusLabel); styleLabel(inferStatusLabel, theme, C_TEXT_S, FONT_UI, false, juce::Justification::centredLeft);

    applyDefaultInferLabels();
    for (int i = 0; i < kMaxInferClasses; ++i) {
        addAndMakeVisible(inferClassLabels[i]);
        styleLabel(inferClassLabels[i], theme, C_TEXT_B, 13.0f);
        inferClassLabels[i].setText(inferLabelNames[i], juce::dontSendNotification);

        inferConfBars[i] = std::make_unique<juce::ProgressBar>(inferConfValues[i]);
        addAndMakeVisible(*inferConfBars[i]);
    }

    addAndMakeVisible(inferLogTitle); styleLabel(inferLogTitle, theme, C_TEXT_H, 14.0f, true);
    addAndMakeVisible(inferLog);
    inferLog.setMultiLine(true); inferLog.setReadOnly(true);
    inferLog.setScrollbarsShown(true); inferLog.setCaretVisible(false);
    inferLog.setColour(juce::TextEditor::backgroundColourId, C_LOG_BG);
    inferLog.setColour(juce::TextEditor::textColourId,       C_LOG_FG);
    inferLog.setColour(juce::TextEditor::outlineColourId,    C_LOG_OUTLINE);
    inferLog.setFont(theme.cjkFont(FONT_LOG));

    inferBrowseModel.onClick = [this] {
        int flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        fileChooser = std::make_unique<juce::FileChooser>(
            W("\u9009\u62e9 ONNX \u6a21\u578b"), juce::File::getCurrentWorkingDirectory(), "*.onnx");
        fileChooser->launchAsync(flags, [this](const juce::FileChooser& c) {
            auto r = c.getResult();
            if (r.existsAsFile()) {
                inferModelInput.setText(r.getFullPathName());
                rebuildInferOnnxCombo();
            }
        });
    };
    inferBrowseData.onClick  = [this] {
        fileChooser = std::make_unique<juce::FileChooser>(
            W("\u9009\u62e9 NPZ / NPY \u6d4b\u8bd5\u6587\u4ef6"), juce::File::getCurrentWorkingDirectory(), "*.npz;*.npy");
        const int flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync(flags, [this](const juce::FileChooser& c) {
            auto r = c.getResult();
            if (r.existsAsFile())
                inferDataInput.setText(r.getFullPathName());
        });
    };
    inferLoadBtn.onClick     = [this] { loadOnnxModel(); };
    inferUseLastExportBtn.onClick = [this] {
        juce::String path;
        if (auto* p = appProperties.getUserSettings())
            path = p->getValue("lastOnnxPath", {});
        inferModelInput.setText(path);
        updateLog(W("[\u4fe1\u606f] \u5df2\u586b\u5165\u4e0a\u6b21 ONNX \u8def\u5f84\u5e76\u5c1d\u8bd5\u52a0\u8f7d"), inferLog);
        rebuildInferOnnxCombo();
        if (path.isNotEmpty())
            loadOnnxModel();
    };
    inferRunBtn.onClick      = [this] { runInference(); };
    inferClearBtn.onClick    = [this] {
        inferLog.setText({});
        for (int i = 0; i < kMaxInferClasses; ++i)
            inferConfValues[i] = 0.0;
        // P0 修复：清空时恢复 inferStatusLabel 到空闲
if (onnxRunner.isModelLoaded())
            inferStatusLabel.setText(W("\u6a21\u578b\u5df2\u52a0\u8f7d\uff0c\u7b49\u5f85\u6d4b\u8bd5"), juce::dontSendNotification);
        else
            inferStatusLabel.setText(W("\u7b49\u5f85\u52a0\u8f7d\u6a21\u578b..."), juce::dontSendNotification);
        inferStatusLabel.setColour(juce::Label::textColourId, C_TEXT_S);
        repaint();
    };
    inferRunBtn.setEnabled(false);

    loadRealtimeSettings();

    // Boot logs
    updateLog(W("\u795e\u7ecf\u8fd0\u884c\u65f6 [C++ \u539f\u751f\u7248 / JUCE \u6846\u67b6] \u7cfb\u7edf\u542f\u52a8\u5b8c\u6210"), trainLog);
    updateLog(W("ONNX \u63a8\u7406\u5f15\u64ce\u5df2\u5c31\u7eea\uff0c\u7b49\u5f85\u8bad\u7ec3\u4efb\u52a1..."), trainLog);
    updateLog(W("[\u9884\u5904\u7406] \u7cfb\u7edf\u5c31\u7eea\uff0c\u8bf7\u914d\u7f6e\u53c2\u6570\u540e\u70b9\u51fb\u300c\u5f00\u59cb\u9884\u5904\u7406\u300d"), prepLog);
    updateLog(W("[\u63a8\u7406] \u8bf7\u5148\u52a0\u8f7d ONNX \u6a21\u578b\uff0c\u7136\u540e\u9009\u62e9\u6d4b\u8bd5\u6570\u636e\u8fd0\u884c\u63a8\u7406"), inferLog);

    // 立即完成最小必要初始化（让窗口尽快显示）
setupTooltips();
    initializeTabPanels();
    switchTab(Tab::Training);
    applyExpertUiVisibility();
    updateTopBarChrome();
    updateTabGuardTooltips();

    // Agent 初始化：推迟到首帧渲染之后，避免构造期磁盘 IO（内存文件加载）阻塞启动
    // initializeAgent() 在后面的 callAsync 中执行。

    addMouseListener(this, true);
    attachShortcutKeyListeners();

    openGLContext.attachTo(*this);

    setSize(1340, 880);
    startTimerHz(10);

    NR_LOGI(W("\u4e3b\u754c\u9762"), W("MainComponent \u521d\u59cb\u5316\u5b8c\u6210"));

    // 延迟执行：等首帧渲染后再做耗时的文件系统扫描 / Agent 初始化 / 环境探测
    juce::MessageManager::callAsync([this] {
        initializeAgent();
        rebuildTrainTemplateCombo(false);
        rebuildInferOnnxCombo();
        checkPythonEnvironment();
        startTimerHz(30);
        resized();
        repaint();
        NR_LOGD(W("\u4e3b\u754c\u9762"), W("\u6a21\u578b/\u6a21\u677f\u5217\u8868\u5237\u65b0\u5b8c\u6bd5\uff0c\u7cfb\u7edf\u5c31\u7eea"));
    });
}

MainComponent::~MainComponent()
{
    persistPrepSettings();
    persistTrainingFields();
    persistRealtimeSettings();
    if (auto* props = appProperties.getUserSettings())
    {
        nerou::app::Context().saveToProperties(*props);
        props->saveIfNeeded();
    }
    nerou::app::Context().removeListener(this);
    nerou::app::Notify().removeListener(this);
    detachShortcutKeyListeners();
    removeMouseListener(this);
    stopTimer();
    // CRITICAL: clear callbacks BEFORE stopping bridges to prevent
    // final flush from enqueueing callAsync lambdas referencing destroyed UI
    trainBridge.setLogCallback(nullptr);
    prepBridge.setLogCallback(nullptr);
    trainBridge.stopTask();
    prepBridge.stopTask();
    
    openGLContext.detach();
    // inferConfBars unique_ptr array auto-deletes on destruction
    setLookAndFeel(nullptr);
}

//==============================================================================
void MainComponent::initializeAgent()
{
    // 记忆目录：AppData/Roaming/NerouRuntime/memory
    juce::File memDir = juce::File::getSpecialLocation(
                            juce::File::userApplicationDataDirectory)
                        .getChildFile("NerouRuntime")
                        .getChildFile("memory");

    // 默认使用本地 Ollama（Hermes 3 模型）
    nerou::ai::LLMConfig cfg;
    cfg.backend  = nerou::ai::LLMConfig::Backend::Ollama;
    cfg.baseUrl  = "http://localhost:11434";
    cfg.model    = "hermes3:latest";
    cfg.temperature = 0.7;

    // 如果已保存过配置，则从 Properties 恢复
    if (auto* props = appProperties.getUserSettings())
    {
        juce::String savedUrl   = props->getValue("agent_llm_url");
        juce::String savedKey   = props->getValue("agent_llm_key");
        juce::String savedModel = props->getValue("agent_llm_model");
        int          backend    = props->getIntValue("agent_llm_backend", 0);

        if (!savedUrl.isEmpty())   cfg.baseUrl = savedUrl;
        if (!savedKey.isEmpty())   cfg.apiKey  = savedKey;
        if (!savedModel.isEmpty()) cfg.model   = savedModel;
        cfg.backend = (backend == 1)
            ? nerou::ai::LLMConfig::Backend::OpenAICompatible
            : nerou::ai::LLMConfig::Backend::Ollama;
    }

    nerou::ai::Agent().initialize(memDir, cfg);

    // 若已有项目上下文，立即加载记忆
    if (nerou::app::Context().hasCurrentProject())
        nerou::ai::Agent().switchProject(
            nerou::app::Context().getCurrentProject().id);

    addAndMakeVisible(agentPanel);
}

void MainComponent::layoutAgentPanel(juce::Rectangle<int>& contentArea)
{
    int agentW = agentPanel.isExpanded()
        ? nerou::ui::AgentPanel::getExpandedWidth()
        : nerou::ui::AgentPanel::getCollapsedWidth();
    agentPanel.setBounds(contentArea.removeFromRight(agentW));
}

//==============================================================================
void MainComponent::initializeTabPanels()
{
    realtimePanelView.bind({
        &realtimeTitleLabel, &realtimeStatusLabel, &realtimeAlignLabel, &startBoardBtn, &stopBoardBtn,
        &realtimeSourceCombo, &realtimeChannelsCombo, &realtimeRateCombo, &realtimeUvRangeCombo,
        &realtimeFilterRowLabel, &realtimeFilterCombo, &realtimeMontageRowLabel, &realtimeMontageCombo,
        &realtimeWinLenCombo, &realtimeNumWinCombo, &realtimeOnnxInferToggle, &realtimeRecordBtn,
        &realtimeLiveConnLabel, &realtimeLiveConnInput, &realtimeLiveConnCopyBtn, &waveformCanvas
    });

    trainingPanelView.bind({
        &trainSec_Identity, &trainSec_Data, &trainSec_Params, &trainSec_Output, &trainTemplateSecLabel,
        &trainTemplateCombo, &nameSecLabel, &nameInput, &dataSecLabel, &dataPathInput, &browseDirBtn,
        &trainDataCountLabel, &epochSecLabel, &epochInput, &classCountSecLabel, &classCountInput,
        &batchSecLabel, &batchCombo, &lrSecLabel, &lrCombo, &saveSecLabel, &savePathInput, &browseSaveBtn,
        &pageTitleLabel, &statusChipLabel, &elapsedLabel, &etaLabel, &progressSectionLabel, &epochBarLabel,
        &epochProgressBar, &epochBarVal, &batchBarVal, &sampleBarVal, &trainPreflightTitle,
        &trainPreflightDetail, &trainPreflightRefreshBtn, &lossCanvas, &accCanvas, &trainLogTitle,
        &trainLog, &startBtn, &pauseBtn, &stopBtn, &saveLogBtn, &saveChartBtn, &clearLogBtn
    });

    preprocessPanelView.bind({
        &prepTitleLabel, &prepRawLabel, &prepRawInput, &prepBrowseRaw, &prepOutLabel, &prepOutInput,
        &prepBrowseOut, &prepSrLabel, &prepSrCombo, &prepLowLabel, &prepLowInput, &prepHighLabel,
        &prepHighInput, &prepChLabel, &prepChCombo, &prepSyncTrainBtn, &prepStartBtn, &prepClearBtn,
        &prepStatusLabel, &prepLogTitle, &prepProgressBar, &prepLog
    });

    inferencePanelView.bind({
        &inferTitleLabel, &inferOnnxPickLabel, &inferOnnxCombo, &inferModelLabel, &inferModelInput,
        &inferBrowseModel, &inferLoadBtn, &inferUseLastExportBtn, &inferDataLabel, &inferDataInput,
        &inferBrowseData, &inferRunBtn, &inferClearBtn, &inferResultTitle, &inferStatusLabel,
        &inferLogTitle, &inferLog
    });

    std::vector<juce::Label*> inferLabels;
    std::vector<juce::ProgressBar*> inferBars;
    inferLabels.reserve(kMaxInferClasses);
    inferBars.reserve(kMaxInferClasses);
    for (int i = 0; i < kMaxInferClasses; ++i)
    {
        inferLabels.push_back(&inferClassLabels[(size_t) i]);
        inferBars.push_back(inferConfBars[(size_t) i].get());
    }
    inferencePanelView.bindClassIndicators(std::move(inferLabels), std::move(inferBars));
}

//==============================================================================
void MainComponent::switchTab(Tab t)
{
    // Production workflow is intentionally reduced to two operator pages:
    // Model Training and Runtime Model Generation. Legacy routes land on Training.
    if (t == Tab::Overview || t == Tab::Acquisition || t == Tab::DataPrep)
        t = Tab::Training;

    // 流程守卫只提示不拦截：生产操作员需要随时查看任一页面的空状态与下一步。
    if (t != Tab::Overview && t != Tab::Acquisition)
    {
        auto guard = workflowOrchestrator.checkCanNavigate(t);
        if (!guard.canEnter)
        {
            const juce::String msg = guard.blockerReason + W(" \u2014 ") + guard.actionHint;
            setStatusMsg(msg, true);
            nerou::ui::SnackbarManager::getInstance().show(
                msg,
                nerou::ui::MaterialSnackbar::Duration::Long,
                nerou::ui::MaterialSnackbar::Type::Warning);
        }
    }

    currentTab = t;
    workflowOrchestrator.setActiveTab(t);
    NR_LOGD(W("\u754c\u9762"),
            W("\u5207\u6362\u9875\u7b7e \u2192 ")
                + nerou::app::WorkflowOrchestrator::tabDisplayName(t));

    // ── 页面切换 (注入到 AppShell 内容区) ─────────────────────────────────
    {
        juce::Component* newPage = nullptr;
        juce::String navId;
        switch (t)
        {
            case Tab::Overview:
            case Tab::Acquisition:
            case Tab::DataPrep:     newPage = &trainingPage;    navId = "training";    break;
            case Tab::Training:     newPage = &trainingPage;    navId = "training";    break;
            case Tab::Inference:    newPage = &validationPage;  navId = "validation";  break;
        }

        appShell_.setContentPage(newPage);
        if (navId.isNotEmpty())
            appShell_.getNavStrip().setActiveById(navId);
    }

    updateTabGuardTooltips();
    updateStatusBarForCurrentTab();
    updateTopBarChrome();

    resized();
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    // 背景由 AppShell 接管
    g.fillAll(C_BG);
}

//==============================================================================
void MainComponent::resized()
{
    auto full = getLocalBounds();
    contentSurfaceBounds = full;
    appShell_.setBounds(full);

    const int paletteW = juce::jmin(640, juce::jmax(360, full.getWidth() - 96));
    const int paletteH = juce::jmin(420, juce::jmax(260, full.getHeight() - 120));
    commandPalette_.setBounds(full.withSizeKeepingCentre(paletteW, paletteH));
}

//==============================================================================
void MainComponent::layoutRealtimePanel(juce::Rectangle<int> area)
{
    auto header = area.removeFromTop(52).withTrimmedTop(14);
    realtimeTitleLabel.setBounds(header.removeFromLeft(200));

    header.removeFromRight(12);
    stopBoardBtn.setBounds(header.removeFromRight(96).reduced(0, 4));
    header.removeFromRight(8);
    startBoardBtn.setBounds(header.removeFromRight(96).reduced(0, 4));
    header.removeFromRight(12);
    realtimeStatusLabel.setBounds(header);

    auto alignRow = area.removeFromTop(28);
    realtimeAlignLabel.setBounds(alignRow);
    area.removeFromTop(4);

    auto row2 = area.removeFromTop(40);
    realtimeSourceCombo.setBounds(row2.removeFromLeft(128).reduced(0, 4));
    row2.removeFromLeft(6);
    realtimeWinLenCombo.setBounds(row2.removeFromLeft(104).reduced(0, 4));
    row2.removeFromLeft(6);
    realtimeNumWinCombo.setBounds(row2.removeFromLeft(76).reduced(0, 4));
    row2.removeFromLeft(6);
    realtimeOnnxInferToggle.setBounds(row2.removeFromLeft(104).reduced(0, 2));
    row2.removeFromLeft(6);
    realtimeRecordBtn.setBounds(row2.removeFromLeft(130).reduced(0, 4));
    area.removeFromTop(6);

    auto rowDev = area.removeFromTop(34);
    realtimeChannelsCombo.setBounds(rowDev.removeFromLeft(88).reduced(0, 4));
    rowDev.removeFromLeft(6);
    realtimeRateCombo.setBounds(rowDev.removeFromLeft(88).reduced(0, 4));
    rowDev.removeFromLeft(6);
    realtimeUvRangeCombo.setBounds(rowDev.removeFromLeft(112).reduced(0, 4));
    area.removeFromTop(6);

    if (realtimeSourceCombo.getSelectedId() == 2)
    {
        auto rowLive = area.removeFromTop(34);
        realtimeLiveConnLabel.setBounds(rowLive.removeFromLeft(88).reduced(0, 6));
        rowLive.removeFromLeft(4);
        realtimeLiveConnCopyBtn.setBounds(rowLive.removeFromRight(52).reduced(0, 4));
        rowLive.removeFromRight(4);
        realtimeLiveConnInput.setBounds(rowLive);
        area.removeFromTop(6);
    }

    auto rowF = area.removeFromTop(38);
    const int lblFilterW  = 76;
    const int lblMontageW = 64;
    const int g           = 8;
    const int comboW =
        juce::jmax(90, (rowF.getWidth() - lblFilterW - lblMontageW - 3 * g) / 2);
    realtimeFilterRowLabel.setBounds(rowF.removeFromLeft(lblFilterW).reduced(0, 4));
    rowF.removeFromLeft(g);
    realtimeFilterCombo.setBounds(rowF.removeFromLeft(comboW).reduced(0, 4));
    rowF.removeFromLeft(g);
    realtimeMontageRowLabel.setBounds(rowF.removeFromLeft(lblMontageW).reduced(0, 4));
    rowF.removeFromLeft(g);
    realtimeMontageCombo.setBounds(rowF.reduced(0, 4));
    area.removeFromTop(6);

    if (realtimeSourceCombo.getSelectedId() == 3)
    {
        auto rowPb = area.removeFromTop(32);
        realtimePlaybackLabel.setBounds(rowPb.removeFromLeft(76).reduced(0, 6));
        rowPb.removeFromLeft(4);
        realtimePlaybackBrowseBtn.setBounds(rowPb.removeFromRight(52).reduced(0, 4));
        rowPb.removeFromRight(4);
        realtimePlaybackInput.setBounds(rowPb);
        area.removeFromTop(6);
    }

    waveformCanvas.setBounds(area.reduced(0, 12));
}

//==============================================================================
void MainComponent::layoutTrainingPanel(juce::Rectangle<int> area)
{
    // Title bar（窄窗口下右对齐控件优先缩小宽度，避remove 后出现负宽与白块错位
auto           titleRow = area.removeFromTop(52);
    const int      padY     = 14;
    const int      gap      = 8;
    juce::Rectangle tr      = titleRow;
    const int      W        = tr.getWidth();
    int            wStat    = juce::jmin(90, juce::jmax(64, W / 8));
    int            wTime    = juce::jmin(125, juce::jmax(72, W / 5));
    int            needRhs  = wStat + wTime * 2 + gap * 3;
    const int      wTitleMin = 100;
    if (needRhs + wTitleMin > W && W > 0)
    {
        const int over = needRhs + wTitleMin - W;
        int reduc = juce::jmax(0, (wStat - 60) + (wTime - 72) * 2);
        if (reduc > 0)
        {
            const float s = juce::jmin(1.0f, (float)over / (float)reduc);
            wStat -= juce::roundToInt((float)(wStat - 60) * s);
            wTime -= juce::roundToInt((float)(wTime - 72) * s);
        }
        wStat = juce::jmax(56, wStat);
        wTime = juce::jmax(64, wTime);
        needRhs = wStat + wTime * 2 + gap * 3;
        const int slack = W - needRhs;
        if (slack < wTitleMin)
        {
            const int extra = wTitleMin - slack;
            wTime -= extra / 2;
            wStat = juce::jmax(56, wStat - (extra - extra / 2));
            wTime = juce::jmax(56, wTime);
        }
    }

    statusChipLabel.setBounds(tr.removeFromRight(wStat).reduced(0, padY));
    tr.removeFromRight(gap);
    etaLabel.setBounds(tr.removeFromRight(wTime).reduced(0, padY));
    tr.removeFromRight(gap);
    elapsedLabel.setBounds(tr.removeFromRight(wTime).reduced(0, padY));
    tr.removeFromRight(gap);
    if (tr.getWidth() > 2 && tr.getHeight() > padY * 2)
        pageTitleLabel.setBounds(tr.withTrimmedTop(padY));
    else
        pageTitleLabel.setBounds(titleRow.withTrimmedTop(padY));
    area.removeFromTop(8); // Space before top card

    // Top Card (Progress and Charts) 鈥?keep min log height on small windows
const int hAvail   = area.getHeight();
    const int gapCards = 16;
    const int minLogH  = 100;
    const int minTopH  = 260;
    int topH = juce::roundToInt(0.65f * (float) hAvail);
    const int maxTop   = juce::jmax(0, hAvail - gapCards - minLogH);
    topH = juce::jmin(topH, maxTop);
    topH = juce::jmax(juce::jmin(minTopH, maxTop), topH);

    trainTopCard = area.removeFromTop(topH);
    auto topInner = trainTopCard.reduced(24);

    {
        auto pfRow = topInner.removeFromTop(52);
        // P2 fix: increase title slot from 90 to 124px 「训练前查needs more room
trainPreflightTitle.setBounds(pfRow.removeFromLeft(124));
        trainPreflightRefreshBtn.setBounds(pfRow.removeFromRight(72).reduced(0, 9));
        pfRow.removeFromLeft(4);
        trainPreflightDetail.setBounds(pfRow);
        topInner.removeFromTop(6);
    }

    progressSectionLabel.setBounds(topInner.removeFromTop(24));
    topInner.removeFromTop(12);

    // 进度行：先保留右侧三个数值标签宽度，再分配进度条，避免窄窗口下出现负宽或错位
auto progRow = topInner.removeFromTop(34);
    epochBarLabel.setBounds(progRow.removeFromLeft(72));
    progRow.removeFromLeft(8);
    const int g      = 8;
    int       rwRem  = juce::jmax(0, progRow.getWidth());
    const int minMw  = 44;
    const int minBar = 32;
    // jlimit(low, high) 必须 low<=high；窄行时jlimit(96, rwRem-24) 会颠倒导致布屢崩溃与白
const int reserveThree = 3 * minMw + 2 * g;
    int       maxBar         = rwRem - reserveThree;
    maxBar                   = juce::jmax(0, maxBar);
    int barW                 = juce::roundToInt((float)rwRem * 0.42f);
    if (maxBar >= minBar)
        barW = juce::jlimit(minBar, maxBar, barW);
    else
        barW = juce::jmin(juce::jmax(0, rwRem / 3), rwRem);
    epochProgressBar.setBounds(progRow.removeFromLeft(barW));
    progRow.removeFromLeft(g);
    int mw = progRow.getWidth() > 2 * g ? (progRow.getWidth() - g * 2) / 3 : minMw;
    mw       = juce::jlimit(40, 96, mw);
    epochBarVal.setBounds(progRow.removeFromLeft(mw));
    progRow.removeFromLeft(g);
    batchBarVal.setBounds(progRow.removeFromLeft(mw));
    progRow.removeFromLeft(g);
    sampleBarVal.setBounds(progRow);
    topInner.removeFromTop(16);

    auto      chartRow = topInner;
    const int gapCh    = 20;
    int       cw       = (chartRow.getWidth() - gapCh) / 2;
    cw                 = juce::jmax(0, cw);
    lossCanvas.setBounds(chartRow.removeFromLeft(cw));
    chartRow.removeFromLeft(gapCh);
    accCanvas.setBounds(chartRow);

    area.removeFromTop(16); // Gap between cards

    // Bottom block: Realtime Log (Left) + Buttons (Right)
    // P2: btn panel 转单列垂直，控制工具组分
const int btnPanelW = juce::jlimit(140, 200, juce::roundToInt((float) area.getWidth() * 0.16f));
    auto rightBlock = area.removeFromRight(btnPanelW);
    area.removeFromRight(14);

    trainBottomCard = area;
    auto botInner = trainBottomCard.reduced(24);
    trainLogTitle.setBounds(botInner.removeFromTop(24));
    botInner.removeFromTop(12);
    trainLog.setBounds(botInner);

    // Single-column vertical stack
    {
        auto bp      = rightBlock.reduced(0, 12);
        const int bh = 36;
        const int bg = 8;  // gap between buttons
const int sg = 14; // extra separator gap between control group and tool group
auto placeBtn = [&](juce::TextButton& b) {
            b.setBounds(bp.removeFromTop(bh));
            bp.removeFromTop(bg);
        };
        placeBtn(startBtn);
        placeBtn(pauseBtn);
        placeBtn(stopBtn);
        bp.removeFromTop(sg); // visual separator gap
placeBtn(saveLogBtn);
        placeBtn(saveChartBtn);
        placeBtn(clearLogBtn);
    }
}

void MainComponent::layoutPrepPanel(juce::Rectangle<int> area)
{
    // Title
prepTitleLabel.setBounds(area.removeFromTop(52).withTrimmedTop(14));
    area.removeFromTop(4);

    // Status + progress
prepStatusLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(4);
    prepProgressBar.setBounds(area.removeFromTop(18));
    area.removeFromTop(10);

    // Log
prepLogTitle.setBounds(area.removeFromTop(22));
    area.removeFromTop(4);
    auto logArea = area;
    const int prepBtnW = juce::jlimit(96, 140, juce::roundToInt((float) logArea.getWidth() * 0.14f));
    auto btnStrip = logArea.removeFromRight(prepBtnW);
    logArea.removeFromRight(8);
    prepLog.setBounds(logArea);

    prepStartBtn.setBounds(btnStrip.removeFromTop(BTN_H));
    btnStrip.removeFromTop(BTN_GAP);
    prepClearBtn.setBounds(btnStrip.removeFromTop(BTN_H));
}

void MainComponent::layoutInferPanel(juce::Rectangle<int> area)
{
    inferTitleLabel.setBounds(area.removeFromTop(52).withTrimmedTop(14));
    area.removeFromTop(4);

    inferOnnxPickLabel.setBounds(area.removeFromTop(LABEL_H));
    area.removeFromTop(3);
    inferOnnxCombo.setBounds(area.removeFromTop(FIELD_H).reduced(0, 2));
    area.removeFromTop(10);

    auto addRow = [&](juce::Label& lbl, juce::TextEditor& ed,
                       juce::TextButton& btn, juce::TextButton* extra = nullptr) {
        lbl.setBounds(area.removeFromTop(LABEL_H));
        area.removeFromTop(3);
        auto row = area.removeFromTop(FIELD_H);
        btn.setBounds(row.removeFromRight(48)); row.removeFromRight(4);
        if (extra) { extra->setBounds(row.removeFromRight(80)); row.removeFromRight(4); }
        ed.setBounds(row);
        area.removeFromTop(10);
    };

    {
        inferModelLabel.setBounds(area.removeFromTop(LABEL_H));
        area.removeFromTop(3);
        auto row = area.removeFromTop(FIELD_H);
        inferUseLastExportBtn.setBounds(row.removeFromRight(102).reduced(0, 2)); row.removeFromRight(4);
        inferLoadBtn.setBounds(row.removeFromRight(76).reduced(0, 2));              row.removeFromRight(4);
        inferBrowseModel.setBounds(row.removeFromRight(48));                       row.removeFromRight(4);
        inferModelInput.setBounds(row);
        area.removeFromTop(10);
    }
    addRow(inferDataLabel, inferDataInput, inferBrowseData, &inferRunBtn);
    area.removeFromTop(4);

    // Results panel (left half) + log (right half)
int resultW = (area.getWidth() - 12) / 2;
    auto resultArea = area.removeFromLeft(resultW);
    area.removeFromLeft(12);

    inferResultTitle.setBounds(resultArea.removeFromTop(22));
    resultArea.removeFromTop(4);
    inferStatusLabel.setBounds(resultArea.removeFromTop(30));
    resultArea.removeFromTop(8);

    const int rowGap = 4;
    int       rowH   = 50;
    if (inferActiveClassCount > 0)
    {
        const int inferSlot = resultArea.getHeight() - BTN_H - rowGap
                              - (inferActiveClassCount - 1) * rowGap;
        if (inferSlot > 0)
            rowH = juce::jlimit(24, 54, inferSlot / inferActiveClassCount);
        else
            rowH = 24;
    }
    for (int i = 0; i < inferActiveClassCount; ++i) {
        auto cell = resultArea.removeFromTop(rowH);
        inferClassLabels[i].setBounds(cell.removeFromTop(18));
        cell.removeFromTop(3);
        if (inferConfBars[i]) inferConfBars[i]->setBounds(cell.removeFromTop(18));
        resultArea.removeFromTop(4);
    }
    inferClearBtn.setBounds(resultArea.removeFromTop(BTN_H));

    // Log
inferLogTitle.setBounds(area.removeFromTop(22));
    area.removeFromTop(4);
    inferLog.setBounds(area);
}

//==============================================================================
void MainComponent::applyRealtimeMontageFromCombo()
{
    switch (realtimeMontageCombo.getSelectedId())
    {
    case 2:
        waveformCanvas.setMontage(WaveformMontage::IEC1020);
        break;
    case 3:
        waveformCanvas.setMontage(WaveformMontage::IEC1010);
        break;
    case 4:
        waveformCanvas.setMontage(WaveformMontage::IEC105);
        break;
    default:
        waveformCanvas.setMontage(WaveformMontage::Numeric);
        break;
    }
}

void MainComponent::applyRealtimeDisplayFilterFromCombo()
{
    switch (realtimeFilterCombo.getSelectedId())
    {
    case 2:
        waveformCanvas.setDisplayFilter(WaveformDisplayFilter::Band_1_45Hz);
        break;
    case 3:
        waveformCanvas.setDisplayFilter(WaveformDisplayFilter::Band_5_35Hz);
        break;
    case 4:
        waveformCanvas.setDisplayFilter(WaveformDisplayFilter::Band_8_25Hz);
        break;
    case 5:
        waveformCanvas.setDisplayFilter(WaveformDisplayFilter::RemoveMean);
        break;
    default:
        waveformCanvas.setDisplayFilter(WaveformDisplayFilter::Raw);
        break;
    }
}

void MainComponent::applyRealtimeTimeWindowFromControls()
{
    const int winLen = realtimeWinLenCombo.getSelectedId() > 0 ? realtimeWinLenCombo.getSelectedId() : 256;
    const int numWin = realtimeNumWinCombo.getSelectedId() > 0 ? realtimeNumWinCombo.getSelectedId() : 1;
    const int sr = realtimeRateCombo.getSelectedId() > 0 ? realtimeRateCombo.getSelectedId() : 256;
    const float sec = (float) (winLen * numWin) / (float) juce::jmax(1, sr);

    waveformCanvas.setFilterSampleRate((double) sr);
    waveformCanvas.setTimeWindowSeconds(sec);
}

void MainComponent::applyRealtimeChannelPaging(bool resetToFirstPage)
{
    const int selectedChannels =
        realtimeChannelsCombo.getSelectedId() > 0 ? realtimeChannelsCombo.getSelectedId() : 8;
    const int totalChannels = juce::jmax(1, selectedChannels);
    realtimeChannelPageSize = (totalChannels > 32 ? 32 : 0);

    if (resetToFirstPage)
        realtimeChannelPageIndex = 0;

    if (realtimeChannelPageSize <= 0)
    {
        realtimeChannelPageIndex = 0;
        waveformCanvas.setVisibleChannelWindow(0, 0);
        return;
    }

    const int maxPage = juce::jmax(0, (totalChannels - 1) / realtimeChannelPageSize);
    realtimeChannelPageIndex = juce::jlimit(0, maxPage, realtimeChannelPageIndex);
    waveformCanvas.setVisibleChannelWindow(realtimeChannelPageIndex * realtimeChannelPageSize,
                                           realtimeChannelPageSize);
}

void MainComponent::updateRealtimeSourceDependentUi()
{
    const bool onRt = (currentTab == Tab::Acquisition);
    realtimeSourceCombo.setSelectedId(3, juce::dontSendNotification);
    const bool showPb = onRt;
    realtimePlaybackLabel.setVisible(showPb);
    realtimePlaybackInput.setVisible(showPb);
    realtimePlaybackBrowseBtn.setVisible(showPb);
    const bool showLive = false;
    realtimeLiveConnLabel.setVisible(showLive);
    realtimeLiveConnInput.setVisible(showLive);
    realtimeLiveConnCopyBtn.setVisible(showLive);
}

//==============================================================================
juce::String MainComponent::composeRealtimeIdleStatus() const
{
    const int hz = BoardManager::getInstance().getSampleRate();
    return W("\u72b6\u6001\uff1a\u8bad\u7ec3\u6587\u4ef6\u6570\u636e\u5df2\u52a0\u8f7d (")
         + juce::String(hz) + W("Hz)");
}

void MainComponent::onProjectChanged(const nerou::app::ProjectInfo& newProject)
{
    // 通知 Agent 切换项目，加载对应记忆
    nerou::ai::Agent().switchProject(newProject.id);
    onContextChanged();
}

void MainComponent::onSubjectChanged(const nerou::app::SubjectInfo& newSubject)
{
    juce::ignoreUnused(newSubject);
    onContextChanged();
}

void MainComponent::onContextChanged()
{
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis] {
        if (auto* self = safeThis.getComponent())
        {
            self->updateTopBarChrome();
            self->updateStatusBarForCurrentTab();
            self->updateTabGuardTooltips(); // 更新流程守卫徽章
            self->repaint();
        }
    });
}

//==============================================================================
void MainComponent::onNotification(const nerou::app::NotificationCenter::Notification& n)
{
    using Level = nerou::app::NotificationCenter::Level;
    using SBType = nerou::ui::MaterialSnackbar::Type;
    using SBDur  = nerou::ui::MaterialSnackbar::Duration;

    SBType type = SBType::Default;
    switch (n.level)
    {
        case Level::Success: type = SBType::Success; break;
        case Level::Warning: type = SBType::Warning; break;
        case Level::Error:   type = SBType::Error;   break;
        case Level::Info:    type = SBType::Info;     break;
    }

    const SBDur dur = (n.level == Level::Error || n.level == Level::Warning)
                    ? SBDur::Long : SBDur::Short;

    if (n.detail.isNotEmpty())
    {
        nerou::ui::SnackbarManager::getInstance().show(
            n.title + "  " + n.detail, dur, type);
    }
    else
    {
        nerou::ui::SnackbarManager::getInstance().show(n.title, dur, type);
    }

    // 状态栏同步显示（备份可见性）
    const bool isErr = (n.level == Level::Error);
    setStatusMsg(n.title + (n.detail.isNotEmpty() ? " — " + n.detail : ""), isErr);

    // 仅 Info / Success 主动落日志（Warning/Error 已由 SystemLogger 自动桥接生成）
    if (n.level == Level::Info || n.level == Level::Success)
    {
        NR_LOGI(W("\u901a\u77e5"),
                n.title + (n.detail.isNotEmpty() ? W(" | ") + n.detail : juce::String()));
    }
}

//==============================================================================
void MainComponent::timerCallback()
{
    if (++alignUiTicker >= 5)
    {
        alignUiTicker = 0;
        updateRealtimeOnnxAlignmentUi();
    }

    if (currentTab != Tab::Training)
    {
        trainPreflightFailTitleFlashTicks = 0;
        trainPreflightOkTitleFlashTicks   = 0;
    }
    else if (trainPreflightFailTitleFlashTicks > 0)
    {
        --trainPreflightFailTitleFlashTicks;
        if (trainPreflightFailTitleFlashTicks == 0)
            syncTrainPreflightTitleVisualState();
        else
        {
            const bool hi = (trainPreflightFailTitleFlashTicks % 10) < 5;
            trainPreflightTitle.setColour(juce::Label::textColourId, hi ? C_WARNING : C_DANGER);
        }
    }
    else if (trainPreflightOkTitleFlashTicks > 0)
    {
        --trainPreflightOkTitleFlashTicks;
        if (trainPreflightOkTitleFlashTicks == 0)
            syncTrainPreflightTitleVisualState();
        else
        {
            const bool hi = (trainPreflightOkTitleFlashTicks % 10) < 5;
            trainPreflightTitle.setColour(juce::Label::textColourId, hi ? C_SUCCESS : C_TEXT_S);
        }
    }

    epochProgress = epochProgressAtomic.load(std::memory_order_relaxed);
    prepProgress  = prepProgressAtomic.load(std::memory_order_relaxed);

    // Drain training log
    {
        juce::StringArray newLogs;
        { const juce::ScopedLock sl(trainLogLock); newLogs = trainPendingLogs; trainPendingLogs.clear(); }
        if (!newLogs.isEmpty()) {
            juce::String t = trainLog.getText();
            if (t.length() > 80000) t = t.substring(t.length() - 40000);
            for (auto& s : newLogs) t += s + "\n";
            trainLog.setText(t);
            trainLog.moveCaretToEnd();
        }
    }

    // Drain prep log
    {
        juce::StringArray newLogs;
        { const juce::ScopedLock sl(prepLogLock); newLogs = prepPendingLogs; prepPendingLogs.clear(); }
        if (!newLogs.isEmpty()) {
            juce::String t = prepLog.getText();
            if (t.length() > 80000) t = t.substring(t.length() - 40000);
            for (auto& s : newLogs) t += s + "\n";
            prepLog.setText(t);
            prepLog.moveCaretToEnd();
        }
    }

    if (currentTab == Tab::Acquisition && BoardManager::getInstance().isStreaming())
    {
        const auto health = BoardManager::getInstance().getStreamHealthStats();
        if (BoardManager::getInstance().isRecordingArmed()) {
            const int p = BoardManager::getInstance().getRecordingProgressFrames();
            const int t = BoardManager::getInstance().getRecordingTargetFrames();
            realtimeStatusLabel.setText(W("采集中：") + juce::String(p) + W(" / ") + juce::String(t)
                                            + W(" | 队列 ") + juce::String(health.livePendingDepth)
                                            + W(" | 丢帧 ") + juce::String((int64)health.droppedLiveFrames + (int64)health.droppedRingFrames),
                                        juce::dontSendNotification);
            realtimeStatusLabel.setColour(juce::Label::textColourId, C_WARNING);
        } else if (!BoardManager::getInstance().isRecordingReady()) {
            realtimeStatusLabel.setText(composeRealtimeIdleStatus()
                                            + W(" | 队列 ") + juce::String(health.livePendingDepth)
                                            + W(" | 丢帧 ") + juce::String((int64)health.droppedLiveFrames + (int64)health.droppedRingFrames),
                                        juce::dontSendNotification);
            realtimeStatusLabel.setColour(juce::Label::textColourId, C_SUCCESS);
        }
    }
    updateRealtimeAdaptiveControl();

    if (BoardManager::getInstance().isRecordingReady())
    {
        bool expected = false;
        if (npzExportBusy.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            int winLen = 0, numWin = 1;
            if (!BoardManager::getInstance().peekRecordingLayout(winLen, numWin) || winLen <= 0) {
                winLen = realtimeWinLenCombo.getSelectedId() > 0 ? realtimeWinLenCombo.getSelectedId() : 256;
                numWin = realtimeNumWinCombo.getSelectedId() > 0 ? realtimeNumWinCombo.getSelectedId() : 1;
            }

            juce::File outDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                     .getChildFile("NerouRuntime")
                                     .getChildFile("captures");
            outDir.createDirectory();
            const juce::String npzBase = juce::String::formatted(
                "rt_n%d_t%d_%s.npz", numWin, winLen,
                juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S"));
            juce::File outF = outDir.getChildFile(npzBase);

            juce::Thread::launch([this, outF, winLen, numWin]() {
                juce::String err;
                const bool ok = BoardManager::getInstance().flushRecordingToNpz(outF, err);
                juce::MessageManager::callAsync([this, ok, err, outF, winLen, numWin]() {
                    npzExportBusy.store(false, std::memory_order_release);
                    if (ok) {
                        inferDataInput.setText(outF.getFullPathName());
                        if (auto* p = appProperties.getUserSettings()) {
                            p->setValue("lastInferNpzPath", outF.getFullPathName());
                            p->saveIfNeeded();
                        }

                        nerou::domain::Recording recording;
                        recording.id = makeEntityId("recording");
                        recording.filePath = outF;
                        recording.fileFormat = "npz";
                        recording.sampleRate = BoardManager::getInstance().getSampleRate();
                        recording.channelCount = BoardManager::getInstance().getNumChannels();
                        recording.channelNames =
                            buildChannelNames(recording.channelCount, realtimeMontageCombo.getSelectedId());
                        recording.sampleCount = juce::jmax(0, winLen * numWin);
                        recording.durationSec = recording.sampleRate > 0
                            ? (double) recording.sampleCount / (double) recording.sampleRate
                            : 0.0;
                        recording.createdAt = juce::Time::getCurrentTime();
                        recording.createdBy = "realtime_monitor";
                        recording.status = "valid";
                        recording.projectId = resolveCurrentProjectId();
                        recording.subjectId = resolveCurrentSubjectId();
                        {
                            auto& context = nerou::app::Context();
                            auto impedance = std::make_unique<juce::DynamicObject>();
                            if (context.hasImpedanceData())
                            {
                                int good = 0, acceptable = 0, poor = 0, disconnected = 0;
                                double sum = 0.0;
                                int measured = 0;
                                for (const auto& ch : context.getChannelImpedances())
                                {
                                    if (ch.impedanceKOhm >= 0.0)
                                    {
                                        sum += ch.impedanceKOhm;
                                        ++measured;
                                    }
                                    using Q = nerou::app::ChannelImpedance::Quality;
                                    switch (ch.quality)
                                    {
                                    case Q::Good: ++good; break;
                                    case Q::Acceptable: ++acceptable; break;
                                    case Q::Poor: ++poor; break;
                                    case Q::Disconnected: ++disconnected; break;
                                    default: break;
                                    }
                                }
                                impedance->setProperty("status", "measured");
                                impedance->setProperty("measured_channels", measured);
                                impedance->setProperty("avg_kohm", measured > 0 ? (sum / (double)measured) : -1.0);
                                impedance->setProperty("good", good);
                                impedance->setProperty("acceptable", acceptable);
                                impedance->setProperty("poor", poor);
                                impedance->setProperty("disconnected", disconnected);
                            }
                            else
                            {
                                impedance->setProperty("status", "not_measured");
                            }
                            recording.impedanceSummary = juce::var(impedance.release());
                        }

                        if (auto* activeSession = acquisitionService.getActiveSession())
                        {
                            recording.sessionId = activeSession->id;
                            recording.projectId = activeSession->projectId;
                            recording.subjectId = activeSession->subjectId;
                        }

                        nerou::core::writeJsonFile(outF.getParentDirectory().getChildFile("recording.json"), toJson(recording));

                        if (recording.subjectId.isNotEmpty())
                        {
                            auto& context = nerou::app::Context();
                            auto subjects = context.getProjectSubjects();
                            for (const auto& subject : subjects)
                            {
                                if (subject.id == recording.subjectId)
                                {
                                    auto updatedSubject = subject;
                                    updatedSubject.sessionCount = juce::jmax(0, subject.sessionCount) + 1;
                                    updatedSubject.lastSession = recording.createdAt;
                                    context.addSubject(updatedSubject);
                                    if (context.hasCurrentSubject() && context.getCurrentSubject().id == updatedSubject.id)
                                        context.setCurrentSubject(updatedSubject);
                                    break;
                                }
                            }
                        }

                        realtimeRecordBtn.setButtonText(W("\u91c7\u96c6NPZ\u5bfc\u51fa"));
                        juce::String exportMsg;
                        if (numWin <= 1)
                            exportMsg = W("\u5df2\u4fdd\u5b58 NPZ (1,C,T)\uff0c\u5df2\u586b\u5165\u63a8\u7406\u6570\u636e\u8def\u5f84");
                        else
                            exportMsg = W("\u5df2\u4fdd\u5b58 NPZ (N=") + juce::String(numWin)
                                         + W(",C,T=") + juce::String(winLen) + W(")\uff0c\u5df2\u586b\u5165\u63a8\u7406\u6570\u636e\u8def\u5f84");
                        setStatusMsg(exportMsg);
                        // 复用训练页日志输出（原来错误地写inferLog
updateLog(W("[\u76d1\u63a7] NPZ \u5df2\u5bfc\u51fa\u5230\u63a8\u7406\u6570\u636e\u8def\u5f84\uff1a") + outF.getFullPathName(), trainLog);
                        if (BoardManager::getInstance().isStreaming()) {
                            realtimeStatusLabel.setText(composeRealtimeIdleStatus(), juce::dontSendNotification);
                            realtimeStatusLabel.setColour(juce::Label::textColourId, C_SUCCESS);
                        }
                    } else {
                        setStatusMsg(W("NPZ \u5bfc\u51fa\u5931\u8d25\uff1a") + err, true);
                        realtimeRecordBtn.setButtonText(W("\u91c7\u96c6NPZ\u5bfc\u51fa"));
                    }
                });
            });
        }
    }

    dispatchRealtimeInferenceIfNeeded();
    consumeRealtimeInferenceResult();

    // Training stat update + status bar
if (isTrainingActive) {
        updateTrainStatCards();
        int ep    = currentEpoch.load();
        int total = totalEpochs.load();
        float acc = currentAcc.load();
        setStatusMsg(W("\u8bad\u7ec3\u4e2d | \u7b2c ") + juce::String(ep) + W(" / ") + juce::String(total)
                     + W(" \u8f6e | \u51c6\u786e\u7387 ") + juce::String(acc * 100.0f, 1) + W("% | Esc \u505c\u6b62"));
    }

    // Auto-unlock training（依据子进程逢出码，避免崩早被误判为成功
if (isTrainingActive)
    {
        juce::uint32 trainExit = 0;
        bool trainUserCancel = false;
        if (trainBridge.tryConsumeRunResult(trainExit, trainUserCancel))
        {
            if (auto* activeJob = trainingService.getActiveJob())
            {
                auto updatedJob = *activeJob;
                updatedJob.endedAt = juce::Time::getCurrentTime();
                updatedJob.state = trainingService.getState();
                trainingService.setActiveJob(updatedJob);
            }

            if (trainUserCancel)
            {
                trainingService.setState(nerou::domain::TaskState::Cancelled);
                nerou::app::PipelineStore::getInstance().syncNodeTaskState(
                    nerou::domain::NodeType::Training,
                    nerou::domain::TaskState::Cancelled,
                    W("\u8bad\u7ec3\u5df2\u53d6\u6d88"));
                if (auto* activeJob = trainingService.getActiveJob())
                {
                    auto cancelledJob = *activeJob;
                    cancelledJob.state = nerou::domain::TaskState::Cancelled;
                    cancelledJob.endedAt = juce::Time::getCurrentTime();
                    trainingService.setActiveJob(cancelledJob);
                    nerou::core::writeJsonFile(juce::File(pendingTrainSaveDir).getChildFile("train_summary.json"), toJson(cancelledJob));
                }
                touchTrainPauseFile(false);
                setTrainUiLocked(false);
                updateLog(W("[\u4fe1\u606f] \u8bad\u7ec3\u5df2\u505c\u6b62\uff08\u7528\u6237\u4e2d\u65ad\u6216\u8fdb\u7a0b\u88ab\u7ec8\u6b62\uff09"), trainLog);
                setStatusMsg(W("\u8bad\u7ec3\u5df2\u4e2d\u6b62"));
            }
            else if (trainExit != 0)
            {
                trainingService.setState(nerou::domain::TaskState::Failed);
                nerou::app::PipelineStore::getInstance().syncNodeTaskState(
                    nerou::domain::NodeType::Training,
                    nerou::domain::TaskState::Failed,
                    W("\u8bad\u7ec3\u5931\u8d25"));
                if (auto* activeJob = trainingService.getActiveJob())
                {
                    auto failedJob = *activeJob;
                    failedJob.state = nerou::domain::TaskState::Failed;
                    failedJob.endedAt = juce::Time::getCurrentTime();
                    trainingService.setActiveJob(failedJob);
                    nerou::core::writeJsonFile(juce::File(pendingTrainSaveDir).getChildFile("train_summary.json"), toJson(failedJob));
                }
                touchTrainPauseFile(false);
                setTrainUiLocked(false);
                statusChipLabel.setText(W("\u8bad\u7ec3\u5931\u8d25"), juce::dontSendNotification);
                statusChipLabel.setColour(juce::Label::textColourId, C_DANGER);
                updateLog(W("[\u9519\u8bef] \u8bad\u7ec3\u8fdb\u7a0b\u5f02\u5e38\u9000\u51fa\uff08\u9000\u51fa\u7801 ") + juce::String((int) trainExit)
                              + W("\uff09\u3002\u8bf7\u67e5\u770b\u4e0a\u65b9\u65e5\u5fd7\u4e2d\u7684 Python \u62a5\u9519"),
                          trainLog);
                setStatusMsg(W("\u8bad\u7ec3\u5931\u8d25"), true);
                nerou::ui::SnackbarManager::getInstance().show(
                    W("\u8bad\u7ec3\u8fdb\u7a0b\u5df2\u9000\u51fa\uff0c\u8bf7\u67e5\u770b\u65e5\u5fd7\u4e2d\u7684 Python \u62a5\u9519"),
                    nerou::ui::MaterialSnackbar::Duration::Long,
                    nerou::ui::MaterialSnackbar::Type::Error);
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    W("\u8bad\u7ec3\u5931\u8d25"),
                    W("\u8bad\u7ec3\u8fdb\u7a0b\u5df2\u9000\u51fa\uff08\u9000\u51fa\u7801 ")
                        + juce::String((int) trainExit)
                        + W("\uff09\u3002\n\u8bf7\u68c0\u67e5\u6570\u636e\u5f62\u72b6\u3001\u6807\u7b7e\u7c7b\u522b\u6570\u3001Python \u4f9d\u8d56\u548c\u65e5\u5fd7\u8f93\u51fa\u3002"));
                isAutoPipelineActive = false;
            }
            else
            {
                trainingService.setState(nerou::domain::TaskState::Completed);
                epochProgress = 1.0;
                epochProgressAtomic.store(1.0, std::memory_order_relaxed);
                setTrainUiLocked(false);
                statusChipLabel.setText(W("\u5df2\u5b8c\u6210"), juce::dontSendNotification);
                statusChipLabel.setColour(juce::Label::textColourId, C_SUCCESS);
                updateLog(W("[\u5b8c\u6210] \u5168\u90e8\u8bad\u7ec3\u8f6e\u6b21\u5df2\u7ed3\u675f\uff0c\u6a21\u578b\u5df2\u4fdd\u5b58"), trainLog);
                setStatusMsg(W("\u8bad\u7ec3\u5b8c\u6210\uff0c\u6a21\u578b\u5df2\u5bfc\u51fa\u5230\u6307\u5b9a\u76ee\u5f55"));

                juce::File onnxF =
                    juce::File(pendingTrainSaveDir).getChildFile("nerou_" + pendingTrainBaseName + ".onnx");
                if (onnxF.existsAsFile() && onnxF.getSize() > 0)
                {
                    if (auto* activeJob = trainingService.getActiveJob())
                    {
                        auto completedJob = *activeJob;
                        completedJob.resultModelId = onnxF.getFullPathName();
                        completedJob.state = nerou::domain::TaskState::Completed;
                        completedJob.endedAt = juce::Time::getCurrentTime();
                        trainingService.setActiveJob(completedJob);
                        nerou::core::writeJsonFile(juce::File(pendingTrainSaveDir).getChildFile("train_summary.json"), toJson(completedJob));
                    }

                    nerou::domain::ModelArtifact model;
                    model.id = makeEntityId("model");
                    model.projectId = resolveCurrentProjectId();
                    model.modelName = pendingTrainBaseName;
                    model.version = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
                    model.taskType = "classification";
                    model.onnxPath = onnxF;
                    model.manifestPath = onnxF.getSiblingFile("manifest.json");
                    model.labelsPath = onnxF.getSiblingFile("labels.json");
                    model.inputChannels = lastTrainPreflight.manifestC > 0 ? lastTrainPreflight.manifestC : lastTrainPreflight.shapeC;
                    model.inputSamples = lastTrainPreflight.manifestT > 0 ? lastTrainPreflight.manifestT : lastTrainPreflight.shapeT;
                    model.inputSampleRate = prepSrCombo.getSelectedId();
                    model.outputClasses = expertUi ? classCountInput.getText().getIntValue() : inferSimpleModeNumClasses();
                    model.createdAt = juce::Time::getCurrentTime();
                    model.createdFromJobId = trainingService.getActiveJob() ? trainingService.getActiveJob()->id : juce::String();
                    trainingService.setLastExportedModel(model);

                    nerou::app::ModelInfo currentModelInfo;
                    currentModelInfo.id = onnxF.getFullPathName();
                    currentModelInfo.name = pendingTrainBaseName;
                    currentModelInfo.version = model.version;
                    currentModelInfo.onnxPath = onnxF;
                    currentModelInfo.manifestPath = model.manifestPath;
                    currentModelInfo.inputChannels = model.inputChannels;
                    currentModelInfo.inputSamples = model.inputSamples;
                    currentModelInfo.outputClasses = model.outputClasses;
                    currentModelInfo.created = model.createdAt;
                    nerou::app::Context().setCurrentModel(currentModelInfo);

                    if (auto* p = appProperties.getUserSettings())
                    {
                        p->setValue("lastOnnxPath", onnxF.getFullPathName());
                        p->saveIfNeeded();
                    }
                    updateLog(W("[\u4fe1\u606f] \u5df2\u8bb0\u5f55\u672c\u6b21 ONNX \u8def\u5f84\uff0c\u53ef\u5728\u300c\u63a8\u7406\u300d\u9875\u4f7f\u7528\u300c\u4f7f\u7528\u4e0a\u6b21\u5bfc\u51fa\u300d"), trainLog);
                    juce::File manifestF = onnxF.getSiblingFile("manifest.json");
                    if (manifestF.existsAsFile())
                        updateLog(W("[\u4fe1\u606f] manifest\uff08\u6807\u7b7e\u4e0e\u5143\u6570\u636e\uff09\uff1a") + manifestF.getFullPathName(), trainLog);
                    updateLog(W("[\u63d0\u793a] \u8bad\u7ec3\u7ed3\u675f\uff1a\u53ef\u5207\u6362\u81f3\u300c\u63a8\u7406\u6d4b\u8bd5\u300d\uff0c\u52a0\u8f7d ONNX \u4e0e\u540c\u5206\u5e03 NPZ \u505a\u9a8c\u8bc1"), trainLog);
                    rebuildInferOnnxCombo();

                    if (isAutoPipelineActive) {
                        isAutoPipelineActive = false;
                        juce::MessageManager::callAsync([this] {
                            switchTab(Tab::Inference);
                            loadLatestOnnxIntoInference(true);
                        });
                    }

                    // ── PipelineStore 训练完成通知 ──────────────────
                    nerou::app::PipelineStore::getInstance().notifyTrainDone(
                        true, onnxF,
                        juce::String(currentAcc.load() * 100.0f, 1) + "% acc");
                }
                else
                {
                    updateLog(W("[\u8b66\u544a] \u672a\u5728\u4fdd\u5b58\u76ee\u5f55\u627e\u5230\u6709\u6548 ONNX \u6587\u4ef6\uff0c\u8bf7\u786e\u8ba4\u8bad\u7ec3\u662f\u5426\u771f\u6b63\u5bfc\u51fa\u6210\u529f"), trainLog);
                    nerou::app::PipelineStore::getInstance().notifyTrainDone(
                        false, juce::File{}, "");
                }
                touchTrainPauseFile(false);
            }
        }
    }

    // Auto-unlock preprocessing
if (isPrepRunning)
    {
        juce::uint32 prepExit = 0;
        bool prepUserCancel = false;
        if (prepBridge.tryConsumeRunResult(prepExit, prepUserCancel))
        {
            if (auto* lastDataset = datasetPreparationService.getLastResult())
            {
                auto updatedDataset = *lastDataset;
                updatedDataset.state = nerou::domain::TaskState::Running;
                datasetPreparationService.setLastResult(updatedDataset);
            }

            if (prepUserCancel)
            {
                datasetPreparationService.setState(nerou::domain::TaskState::Cancelled);
                nerou::app::PipelineStore::getInstance().syncNodeTaskState(
                    nerou::domain::NodeType::Preprocessing,
                    nerou::domain::TaskState::Cancelled,
                    W("\u9884\u5904\u7406\u5df2\u53d6\u6d88"));
                if (auto* lastDataset = datasetPreparationService.getLastResult())
                {
                    auto cancelledDataset = *lastDataset;
                    cancelledDataset.state = nerou::domain::TaskState::Cancelled;
                    cancelledDataset.summaryPath = juce::File(prepOutInput.getText().trim()).getChildFile("dataset_summary.json");
                    datasetPreparationService.setLastResult(cancelledDataset);
                    nerou::core::writeJsonFile(cancelledDataset.summaryPath, toJson(cancelledDataset));
                }
                setPrepLocked(false);
                prepProgress = 0.0;
                prepProgressAtomic.store(0.0, std::memory_order_relaxed);
                updateLog(W("[\u4fe1\u606f] \u9884\u5904\u7406\u5df2\u505c\u6b62\uff08\u7528\u6237\u4e2d\u65ad\u6216\u8fdb\u7a0b\u88ab\u7ec8\u6b62\uff09"), prepLog);
                prepStatusLabel.setText(W("\u5df2\u4e2d\u6b62"), juce::dontSendNotification);
                prepStatusLabel.setColour(juce::Label::textColourId, C_WARNING);
            }
            else if (prepExit != 0)
            {
                datasetPreparationService.setState(nerou::domain::TaskState::Failed);
                nerou::app::PipelineStore::getInstance().syncNodeTaskState(
                    nerou::domain::NodeType::Preprocessing,
                    nerou::domain::TaskState::Failed,
                    W("\u9884\u5904\u7406\u5931\u8d25"));
                if (auto* lastDataset = datasetPreparationService.getLastResult())
                {
                    auto failedDataset = *lastDataset;
                    failedDataset.state = nerou::domain::TaskState::Failed;
                    failedDataset.summaryPath = juce::File(prepOutInput.getText().trim()).getChildFile("dataset_summary.json");
                    datasetPreparationService.setLastResult(failedDataset);
                    nerou::core::writeJsonFile(failedDataset.summaryPath, toJson(failedDataset));
                }
                setPrepLocked(false);
                prepProgress = 0.0;
                prepProgressAtomic.store(0.0, std::memory_order_relaxed);
                updateLog(W("[\u9519\u8bef] \u9884\u5904\u7406\u8fdb\u7a0b\u5f02\u5e38\u9000\u51fa\uff08\u9000\u51fa\u7801 ") + juce::String((int) prepExit)
                              + W("\uff09\uff0c\u8bf7\u68c0\u67e5\u76ee\u5f55\u6743\u9650\u4e0e Python \u65e5\u5fd7"),
                          prepLog);
                prepStatusLabel.setText(W("\u9884\u5904\u7406\u5931\u8d25"), juce::dontSendNotification);
                prepStatusLabel.setColour(juce::Label::textColourId, C_DANGER);
                isAutoPipelineActive = false;
            }
            else
            {
                datasetPreparationService.setState(nerou::domain::TaskState::Completed);
                prepProgress = 1.0;
                prepProgressAtomic.store(1.0, std::memory_order_relaxed);
                setPrepLocked(false);
                updateLog(W("[\u5b8c\u6210] \u6570\u636e\u9884\u5904\u7406\u5168\u90e8\u5b8c\u6210"), prepLog);
                prepStatusLabel.setText(W("\u9884\u5904\u7406\u5b8c\u6210"), juce::dontSendNotification);
                prepStatusLabel.setColour(juce::Label::textColourId, C_SUCCESS);
                if (auto* lastDataset = datasetPreparationService.getLastResult())
                {
                    auto completedDataset = *lastDataset;
                    const juce::File prepOutDir(prepOutInput.getText().trim());
                    completedDataset.sampleCount = countFilesWithExtension(prepOutDir, "*.npz");
                    completedDataset.summaryPath = prepOutDir.getChildFile("dataset_summary.json");
                    if (completedDataset.summaryPath.existsAsFile())
                    {
                        auto summary = juce::JSON::parse(completedDataset.summaryPath.loadFileAsString());
                        if (summary.isObject())
                        {
                            auto* obj = summary.getDynamicObject();
                            if (obj->hasProperty("sampleCount"))
                                completedDataset.sampleCount = (int) obj->getProperty("sampleCount");
                            else if (obj->hasProperty("sample_count"))
                                completedDataset.sampleCount = (int) obj->getProperty("sample_count");
                            if (obj->hasProperty("channelCount"))
                                completedDataset.channelCount = (int) obj->getProperty("channelCount");
                            else if (obj->hasProperty("channel_count"))
                                completedDataset.channelCount = (int) obj->getProperty("channel_count");
                        }
                    }
                    completedDataset.state = nerou::domain::TaskState::Completed;
                    datasetPreparationService.setLastResult(completedDataset);
                    if (!completedDataset.summaryPath.existsAsFile())
                        nerou::core::writeJsonFile(completedDataset.summaryPath, toJson(completedDataset));

                    nerou::app::DatasetInfo currentDatasetInfo;
                    currentDatasetInfo.id = completedDataset.outputPath.getFullPathName();
                    currentDatasetInfo.name = completedDataset.outputPath.getFileName();
                    currentDatasetInfo.path = completedDataset.outputPath;
                    currentDatasetInfo.sampleCount = completedDataset.sampleCount;
                    currentDatasetInfo.channelCount = completedDataset.channelCount;
                    currentDatasetInfo.created = completedDataset.createdAt;
                    currentDatasetInfo.isProcessed = true;
                    nerou::app::Context().setCurrentDataset(currentDatasetInfo);

                    // ── PipelineStore 更新 ───────────────────────────
                    // 1. 采集节点（若尚未标记完成）
                    {
                        auto& ps = nerou::app::PipelineStore::getInstance();
                        auto acqNode = ps.getNode(nerou::domain::NodeType::Acquisition);
                        if (!acqNode.isComplete())
                        {
                            ps.notifyAcquisitionDone(
                                completedDataset.inputPath,
                                juce::String(completedDataset.channelCount) + "ch");
                        }
                    }
                    // 2. 预处理节点完成
                    nerou::app::PipelineStore::getInstance().notifyPrepDone(
                        true,
                        completedDataset.outputPath,
                        juce::String(completedDataset.sampleCount) + " samples");
                }
                juce::MessageManager::callAsync([this] { 
                    refreshTrainPreflight(); 
                    if (isAutoPipelineActive) {
                        switchTab(Tab::Training);
                        startTraining();
                    }
                });
            }
        }
    }

    // 非训练场景下完成trainBridge 任务（如启动时环境探测）：消费结果以免积
if (!isTrainingActive)
    {
        juce::uint32 ex = 0;
        bool uc = false;
        if (trainBridge.tryConsumeRunResult(ex, uc) && !uc && ex != 0)
            updateLog(W("[\u73af\u5883\u68c0\u67e5] Python \u5b50\u8fdb\u7a0b\u975e\u96f6\u9000\u51fa\uff08") + juce::String((int) ex) + W("\uff09"), trainLog);
    }

    if (!isPrepRunning)
    {
        [[maybe_unused]] juce::uint32 ex = 0;
        [[maybe_unused]] bool uc         = false;
        prepBridge.tryConsumeRunResult(ex, uc);
    }

    updateTopBarChrome();
}

//==============================================================================
// ── Training logic ─────────────────────────────────────────────────────────
void MainComponent::startTraining()
{
    // P0 修复：防止训练重复启动
    if (isTrainingActive) {
        updateLog(W("[\u8b66\u544a] \u8bad\u7ec3\u5df2\u5728\u8fd0\u884c\u4e2d\uff0c\u8bf7\u5148\u505c\u6b62\u5f53\u524d\u8bad\u7ec3"), trainLog);
        return;
    }

    if (!ensureProjectContextReady(W("\u5f00\u59cb\u8bad\u7ec3")))
        return;

    refreshTrainPreflight();
    if (!trainPreflightPassed)
    {
        setStatusMsg(W("\u8bad\u7ec3\u524d\u68c0\u67e5\u672a\u901a\u8fc7\uff1a") + trainPreflightDetail.getText(), true);
        statusChipLabel.setText(W("\u5f85\u9884\u68c0"), juce::dontSendNotification);
        statusChipLabel.setColour(juce::Label::textColourId, C_WARNING);
        nerou::ui::SnackbarManager::getInstance().show(
            W("\u8bad\u7ec3\u524d\u68c0\u67e5\u672a\u901a\u8fc7\uff0c\u8bf7\u5148\u5904\u7406\u6570\u636e\u76ee\u5f55\u6216 manifest \u5339\u914d"),
            nerou::ui::MaterialSnackbar::Duration::Long,
            nerou::ui::MaterialSnackbar::Type::Warning);
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, W("\u8bad\u7ec3\u524d\u68c0\u67e5\u672a\u901a\u8fc7"),
                                                trainPreflightDetail.getText());
        return;
    }

    int ep    = epochInput.getText().getIntValue();
    int batch = batchCombo.getSelectedId();
    float lr  = expertUi ? lrCombo.getText().getFloatValue() : 0.0005f;
    juce::String name = nameInput.getText().trim();

    int numClasses = expertUi ? classCountInput.getText().getIntValue() : inferSimpleModeNumClasses();
    if (ep <= 0 || batch <= 0 || name.isEmpty()) {
        updateLog(W("[\u8b66\u544a] \u53c2\u6570\u6821\u9a8c\u5931\u8d25\uff1a\u8bf7\u68c0\u67e5\u6a21\u578b\u540d\u79f0\u3001\u8f6e\u6570\u548c\u6279\u91cf\u5927\u5c0f"), trainLog);
        return;
    }
    if (expertUi && (lr <= 0.0f || numClasses < 1)) {
        updateLog(W("[\u8b66\u544a] \u4e13\u5bb6\u6a21\u5f0f\uff1a\u8bf7\u68c0\u67e5\u5b66\u4e60\u7387\u4e0e\u7c7b\u522b\u6570\u3002"),
                  trainLog);
        return;
    }
    if (!expertUi && numClasses < 2)
        numClasses = 2;

    if (!expertUi)
        classCountInput.setText(juce::String(numClasses), juce::dontSendNotification);

    const juce::String dataPath = dataPathInput.getText().trim();
    const juce::String savePath = savePathInput.getText().trim();
    if (dataPath.isEmpty())
    {
        updateLog(W("[\u8b66\u544a] \u8bf7\u586b\u5199\u6570\u636e\u96c6\u76ee\u5f55"), trainLog);
        return;
    }
    if (!juce::File(dataPath).isDirectory())
    {
        updateLog(W("[\u8b66\u544a] \u6570\u636e\u96c6\u8def\u5f84\u4e0d\u5b58\u5728\u6216\u4e0d\u662f\u76ee\u5f55\uff1a") + dataPath, trainLog);
        return;
    }
    if (savePath.isEmpty())
    {
        updateLog(W("[\u8b66\u544a] \u8bf7\u586b\u5199\u6a21\u578b\u4fdd\u5b58\u76ee\u5f55\uff08\u5c06\u5bfc\u51fa ONNX \u4e0e manifest\uff09"), trainLog);
        return;
    }
    {
        juce::File saveDir(savePath);
        if (!saveDir.isDirectory() && !saveDir.createDirectory().wasOk())
        {
            updateLog(W("[\u8b66\u544a] \u65e0\u6cd5\u521b\u5efa\u6216\u8bbf\u95ee\u4fdd\u5b58\u76ee\u5f55\uff1a") + savePath, trainLog);
            return;
        }
    }

    persistTrainingFields();

    const float manifestSr = (float) juce::jmax(1, prepSrCombo.getSelectedId());

    {
        nerou::domain::TrainingJob job;
        job.id = makeEntityId("train");
        job.projectId = resolveCurrentProjectId();
        job.datasetId = dataPath;
        job.taskType = "classification";
        job.modelTemplate = trainTemplateCombo.getText().trim();
        job.classCount = numClasses;
        job.epochs = ep;
        job.batchSize = batch;
        job.learningRate = lr;
        job.startedAt = juce::Time::getCurrentTime();
        job.state = nerou::domain::TaskState::Queued;

        auto config = std::make_unique<juce::DynamicObject>();
        config->setProperty("model_name", name);
        config->setProperty("data_path", dataPath);
        config->setProperty("save_path", savePath);
        config->setProperty("sample_rate_hz", manifestSr);
        job.trainConfig = juce::var(config.release());
        job.logPath = juce::File(savePath).getChildFile("train.log");
        job.metricsPath = juce::File(savePath).getChildFile("train_metrics.json");
        job.preflightResult = juce::var(trainPreflightPassed);

        trainingService.setActiveJob(job);
        trainingService.setState(nerou::domain::TaskState::Queued);
        nerou::app::PipelineStore::getInstance().syncNodeTaskState(
            nerou::domain::NodeType::Training,
            nerou::domain::TaskState::Queued,
            W("\u7b49\u5f85\u542f\u52a8"));
    }

    totalEpochs.store(ep);
    epochProgress = 0.0;
    epochProgressAtomic.store(0.0, std::memory_order_relaxed);
    currentEpoch.store(0); currentLoss.store(0.0f); currentAcc.store(0.0f);
    lossCanvas.reset();    accCanvas.reset();
    trainingStartTime = juce::Time::getCurrentTime();
    setTrainUiLocked(true);

    pendingTrainSaveDir  = savePath;
    pendingTrainBaseName = name;
    touchTrainPauseFile(false);
    isPaused = false;
    pauseBtn.setButtonText(W("\u6682\u505c"));

    // 明确打印实际采样率来源（从预处理页读取），让用户知晓此隐式依
updateLog(W("\u2192 \u542f\u52a8\u8bad\u7ec3\u4efb\u52a1\uff1a\u300c") + name + W("\u300d\uff0c\u8f6e\u6570\uff1a") + juce::String(ep)
                  + W("\uff0c\u6279\u91cf\uff1a") + juce::String(batch) + W("\uff0c\u5b66\u4e60\u7387 ")
                  + juce::String(lr, 6)
                  + W("\uff0c\u91c7\u6837\u7387 ") + juce::String((int)manifestSr) + W(" Hz\uff0c\u7c7b\u522b\u6570 ") + juce::String(numClasses),
              trainLog);

    NR_LOGI(W("\u8bad\u7ec3"),
            W("\u5f00\u59cb\u8bad\u7ec3\u4efb\u52a1 name=") + name
                + W(" epochs=") + juce::String(ep)
                + W(" batch=") + juce::String(batch)
                + W(" lr=") + juce::String(lr, 6)
                + W(" classes=") + juce::String(numClasses)
                + W(" sr=") + juce::String((int)manifestSr) + W("Hz"));

    if (!trainBridge.launchTrainingTask(ep, batch, lr, name,
                                        dataPath,
                                        savePath,
                                        numClasses,
                                        manifestSr)) {
        updateLog(W("[\u9519\u8bef] \u65e0\u6cd5\u542f\u52a8\u8bad\u7ec3\u811a\u672c\uff0c\u8bf7\u68c0\u67e5 Python \u73af\u5883\u914d\u7f6e"), trainLog);
        nerou::ui::SnackbarManager::getInstance().show(
            W("\u8bad\u7ec3\u542f\u52a8\u5931\u8d25\uff1a\u672a\u80fd\u542f\u52a8 Python \u811a\u672c"),
            nerou::ui::MaterialSnackbar::Duration::Long,
            nerou::ui::MaterialSnackbar::Type::Error);
        NR_LOGE(W("\u8bad\u7ec3"), W("\u65e0\u6cd5\u542f\u52a8 Python \u8bad\u7ec3\u5b50\u8fdb\u7a0b"));
        trainingService.setState(nerou::domain::TaskState::Failed);
        nerou::app::PipelineStore::getInstance().syncNodeTaskState(
            nerou::domain::NodeType::Training,
            nerou::domain::TaskState::Failed,
            W("\u542f\u52a8\u8bad\u7ec3\u5931\u8d25"));
        setTrainUiLocked(false);
        statusChipLabel.setText(W("\u542f\u52a8\u5931\u8d25"), juce::dontSendNotification);
        statusChipLabel.setColour(juce::Label::textColourId, C_DANGER);
        setStatusMsg(W("\u8bad\u7ec3\u542f\u52a8\u5931\u8d25\uff0c\u8bf7\u67e5\u770b\u8bad\u7ec3\u65e5\u5fd7\u4e0e Python \u73af\u5883"), true);
    } else {
        trainingService.setState(nerou::domain::TaskState::Running);
        nerou::app::PipelineStore::getInstance().syncNodeTaskState(
            nerou::domain::NodeType::Training,
            nerou::domain::TaskState::Running,
            W("\u8bad\u7ec3\u8fdb\u884c\u4e2d"),
            0.0f);
    }

}

void MainComponent::pauseTraining()
{
    if (!isTrainingActive)
        return;
    isPaused = !isPaused;
    pauseBtn.setButtonText(isPaused ? W("\u7ee7\u7eed") : W("\u6682\u505c"));
    statusChipLabel.setText(isPaused ? W("\u5df2\u6682\u505c") : W("\u8bad\u7ec3\u4e2d"), juce::dontSendNotification);
    statusChipLabel.setColour(juce::Label::textColourId, isPaused ? C_WARNING : C_PRIMARY);
    touchTrainPauseFile(isPaused);
    updateLog(isPaused ? W("[\u4fe1\u606f] \u5df2\u8bf7\u6c42\u6682\u505c\uff08train.py \u5c06\u5728\u4e0b\u4e00\u4e2a batch \u8fb9\u754c\u505c\u4e0b\uff09")
                       : W("[\u4fe1\u606f] \u5df2\u6062\u590d\u8bad\u7ec3"), trainLog);
}

void MainComponent::stopTraining()
{
    if (!isTrainingActive) return;
    // P1 修复：加确认对话确保用户不人误操
const bool confirmed = juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::WarningIcon,
        W("\u505c\u6b62\u8bad\u7ec3"),
        W("\u786e\u5b9a\u8981\u7ec8\u6b62\u8bad\u7ec3\u8fdb\u7a0b\u5417\uff1f\n\u672a\u5bfc\u51fa\u7684\u6a21\u578b\u6587\u4ef6\u5c06\u4e22\u5931"),
        W("\u505c\u6b62"), W("\u7ee7\u7eed"));
    if (!confirmed)
        return;
    updateLog(W("[\u8b66\u544a] \u6025\u505c\u6307\u4ee4\u5df2\u53d1\u51fa\uff0c\u6b63\u5728\u7ec8\u6b62 Python \u5b50\u8fdb\u7a0b..."), trainLog);
    NR_LOGW(W("\u8bad\u7ec3"), W("\u7528\u6237\u8bf7\u6c42\u505c\u6b62\u8bad\u7ec3\uff0c\u6b63\u5728\u7ec8\u6b62\u5b50\u8fdb\u7a0b"));
    touchTrainPauseFile(false);
    trainBridge.stopTask();
    {
        [[maybe_unused]] juce::uint32 ex = 0;
        [[maybe_unused]] bool uc         = false;
        trainBridge.tryConsumeRunResult(ex, uc);
    }
    setTrainUiLocked(false);
    updateLog(W("[\u4fe1\u606f] \u8bad\u7ec3\u5df2\u4e2d\u6b62"), trainLog);
    setStatusMsg(W("\u8bad\u7ec3\u5df2\u4e2d\u6b62"));
}

void MainComponent::setTrainUiLocked(bool locked)
{
    isTrainingActive = locked;
    for (auto* e : { &nameInput, &dataPathInput, &epochInput, &classCountInput, &savePathInput })
        e->setReadOnly(locked);
    trainTemplateCombo.setEnabled(!locked);
    browseDirBtn.setEnabled(!locked);
    browseSaveBtn.setEnabled(!locked);
    batchCombo.setEnabled(!locked);
    lrCombo.setEnabled(!locked && expertUi);
    trainPreflightRefreshBtn.setEnabled(!locked);
    const bool canStart = !locked
                          && trainPreflightPassed
                          && nameInput.getText().trim().isNotEmpty()
                          && dataPathInput.getText().trim().isNotEmpty()
                          && savePathInput.getText().trim().isNotEmpty();
    startBtn.setEnabled(canStart);
    pauseBtn.setEnabled(locked);
    stopBtn.setEnabled(locked);

    // 根据锁定状态更新按钮 Tooltip（解释禁用原因）
    if (locked) {
        startBtn.setTooltip(W("\u8bad\u7ec3\u8fdb\u884c\u4e2d\uff0c\u53c2\u6570\u5df2\u9501\u5b9a\u3002\u9700\u505c\u6b62\u5f53\u524d\u8bad\u7ec3\u540e\u624d\u53ef\u91cd\u65b0\u914d\u7f6e\u3002"));
        pauseBtn.setTooltip(W("\u6682\u505c / \u7ee7\u7eed\u5f53\u524d\u8bad\u7ec3\uff08\u7531 train.py \u68c0\u6d4b .nerou_train_pause \u6587\u4ef6\uff09"));
        stopBtn.setTooltip(W("\u5f3a\u5236\u7ec8\u6b62\u5f53\u524d\u8bad\u7ec3 (Esc)\uff0c\u4e22\u5931\u672a\u4fdd\u5b58\u8fdb\u5ea6\u3002"));
    } else {
        if (canStart) {
            startBtn.setTooltip(W("\u5f00\u59cb\u8bad\u7ec3\u4efb\u52a1 (Ctrl+Enter)\u3002\u5f53\u524d\u914d\u7f6e\u5df2\u5c31\u7eea\u3002"));
        } else {
            juce::String tip = W("\u8bf7\u5148\u5b8c\u6210\u4ee5\u4e0b\u51c6\u5907\uff1a");
            if (!trainPreflightPassed) tip += W("\u8bad\u7ec3\u524d\u68c0\u67e5\u672a\u901a\u8fc7\uff1b");
            if (nameInput.getText().trim().isEmpty()) tip += W("\u6a21\u578b\u540d\u79f0\u672a\u586b\u5199\uff1b");
            if (dataPathInput.getText().trim().isEmpty()) tip += W("\u6570\u636e\u96c6\u76ee\u5f55\u672a\u8bbe\u7f6e\uff1b");
            if (savePathInput.getText().trim().isEmpty()) tip += W("\u4fdd\u5b58\u8def\u5f84\u672a\u8bbe\u7f6e\uff1b");
            startBtn.setTooltip(tip);
        }
        pauseBtn.setTooltip(W("\u6682\u672a\u5f00\u59cb\u8bad\u7ec3\uff0c\u65e0\u6cd5\u6682\u505c\u3002\u8bf7\u5148\u70b9\u51fb\u300c\u5f00\u59cb\u8bad\u7ec3\u300d\u3002"));
        stopBtn.setTooltip(W("\u6682\u672a\u5f00\u59cb\u8bad\u7ec3\uff0c\u65e0\u6cd5\u505c\u6b62\u3002\u8bf7\u5148\u70b9\u51fb\u300c\u5f00\u59cb\u8bad\u7ec3\u300d\u3002"));
    }

    if (locked) {
        startBtn.setButtonText(W("\u8bad\u7ec3\u8fdb\u884c\u4e2d..."));
        statusChipLabel.setText(W("\u8bad\u7ec3\u4e2d"), juce::dontSendNotification);
        statusChipLabel.setColour(juce::Label::textColourId, C_PRIMARY);
    } else {
        isPaused = false;
        startBtn.setButtonText(canStart ? W("\u5f00\u59cb\u8bad\u7ec3") : W("\u5148\u5b8c\u6210\u9884\u68c0"));
        pauseBtn.setButtonText(W("\u6682\u505c"));
        statusChipLabel.setText(canStart ? W("\u5c31\u7eea") : W("\u5f85\u9884\u68c0"), juce::dontSendNotification);
        statusChipLabel.setColour(juce::Label::textColourId, canStart ? C_SUCCESS : C_WARNING);
    }

    repaint();
}

void MainComponent::updateTrainStatCards()
{
    int ep = currentEpoch.load(), total = totalEpochs.load();
    epochBarVal.setText(juce::String(ep) + " / " + juce::String(total), juce::dontSendNotification);
    // P1 修复：提示语义明确的批量大小，去掉语义错误的「样N」计
    batchBarVal.setText(W("\u6279\u91cf: ") + juce::String(batchCombo.getSelectedId()), juce::dontSendNotification);
    // 展示训练数据集路径（箢题）代替语义不准ep*batch
const juce::String dsPath = dataPathInput.getText().trim();
    juce::String dsName = dsPath.isEmpty() ? W("\u672a\u8bbe\u7f6e") : juce::File(dsPath).getFileName();
    if (dsName.length() > 15)
        dsName = dsName.substring(0, 15) + "...";
    sampleBarVal.setText(dsName, juce::dontSendNotification);

    int secs = (int)(juce::Time::getCurrentTime() - trainingStartTime).inSeconds();
    elapsedLabel.setText(W("\u5df2\u7528\u65f6 ") + formatDuration(secs), juce::dontSendNotification);
    if (ep > 0 && total > 0) {
        int remain = (int)((double)secs / ep * (total - ep));
        etaLabel.setText(W("\u9884\u8ba1\u5269\u4f59 ") + formatDuration(remain), juce::dontSendNotification);
    }

}

//==============================================================================
// ── Preprocessing logic ────────────────────────────────────────────────────
void MainComponent::startPreprocess()
{
    // P1 修复：防止预处理重复启动
    if (isPrepRunning) {
        updateLog(W("[\u8b66\u544a] \u9884\u5904\u7406\u5df2\u5728\u8fd0\u884c\u4e2d\uff0c\u8bf7\u5148\u505c\u6b62\u5f53\u524d\u4efb\u52a1"), prepLog);
        return;
    }

    if (!ensureProjectContextReady(W("\u5f00\u59cb\u9884\u5904\u7406")))
        return;

    persistPrepSettings();
    auto rawDir = prepRawInput.getText().trim();
    auto outDir = prepOutInput.getText().trim();
    if (rawDir.isEmpty() || outDir.isEmpty()) {
        updateLog(W("[\u8b66\u544a] \u8bf7\u5148\u8bbe\u7f6e\u539f\u59cb\u6570\u636e\u76ee\u5f55\u4e0e\u8f93\u51fa\u76ee\u5f55"), prepLog);
        return;
    }

    if (!juce::File(rawDir).isDirectory())
    {
        updateLog(W("[\u8b66\u544a] \u539f\u59cb\u6570\u636e\u76ee\u5f55\u4e0d\u5b58\u5728\uff1a") + rawDir, prepLog);
        return;
    }
    juce::File(outDir).createDirectory();
    const bool useMnePipeline = directoryContainsRawSignalFiles(juce::File(rawDir));

    juce::String scriptPath = useMnePipeline
        ? "python_core/preprocess/mne_pipeline.py"
        : "python_core/preprocess.py";

    juce::StringArray args;
    if (useMnePipeline)
    {
        args.add("--input-dir=" + rawDir);
        args.add("--output-dir=" + outDir);
        args.add("--resample=" + juce::String(prepSrCombo.getSelectedId()));
        args.add("--l-freq=" + prepLowInput.getText().trim());
        args.add("--h-freq=" + prepHighInput.getText().trim());
        args.add("--notch=50");
        args.add("--reference=average");
    }
    else
    {
        args = {
            "--input="   + rawDir,
            "--output="  + outDir,
            "--sr="      + juce::String(prepSrCombo.getSelectedId()),
            "--low="     + prepLowInput.getText().trim(),
            "--high="    + prepHighInput.getText().trim(),
            "--channels="+ juce::String(prepChCombo.getSelectedId())
        };
    }

    {
        nerou::domain::ProcessedDataset dataset;
        dataset.id = makeEntityId("dataset");
        dataset.projectId = resolveCurrentProjectId();
        dataset.inputPath = juce::File(rawDir);
        dataset.outputPath = juce::File(outDir);
        dataset.sampleRate = prepSrCombo.getSelectedId();
        dataset.channelCount = prepChCombo.getSelectedId();
        dataset.createdAt = juce::Time::getCurrentTime();
        dataset.state = nerou::domain::TaskState::Queued;

        auto config = std::make_unique<juce::DynamicObject>();
        config->setProperty("low_hz", prepLowInput.getText().trim());
        config->setProperty("high_hz", prepHighInput.getText().trim());
        config->setProperty("sample_rate_hz", prepSrCombo.getSelectedId());
        config->setProperty("channels", prepChCombo.getSelectedId());
        dataset.preprocessConfig = juce::var(config.release());

        datasetPreparationService.setLastResult(dataset);
        datasetPreparationService.setState(nerou::domain::TaskState::Queued);
        nerou::app::PipelineStore::getInstance().syncNodeTaskState(
            nerou::domain::NodeType::Preprocessing,
            nerou::domain::TaskState::Queued,
            W("\u7b49\u5f85\u542f\u52a8"));
    }

    setPrepLocked(true);
    prepProgress = 0.0;
    prepProgressAtomic.store(0.0, std::memory_order_relaxed);
    updateLog(W("\u2192 \u542f\u52a8\u9884\u5904\u7406\uff1a") + rawDir + " -> " + outDir, prepLog);
    updateLog(useMnePipeline
                  ? W("[\u9884\u5904\u7406] \u68c0\u6d4b\u5230 EDF/GDF/FIF/BDF/SET/VHDR\uff0c\u4f7f\u7528 MNE-Python \u7ba1\u7ebf\u751f\u6210\u6807\u51c6 [N,C,T] NPZ")
                  : W("[\u9884\u5904\u7406] \u672a\u68c0\u6d4b\u5230\u539f\u59cb\u4fe1\u53f7\u6587\u4ef6\uff0c\u4f7f\u7528\u65e7 NPZ \u6570\u636e\u7ba1\u7ebf"),
              prepLog);
    NR_LOGI(W("\u9884\u5904\u7406"),
            W("\u5f00\u59cb\u9884\u5904\u7406 raw=") + rawDir
                + W(" out=") + outDir
                + W(" sr=") + juce::String(prepSrCombo.getSelectedId())
                + W("Hz ch=") + juce::String(prepChCombo.getSelectedId()));

    if (!prepBridge.launchScript(scriptPath, args)) {
        updateLog(W("[\u9519\u8bef] \u65e0\u6cd5\u542f\u52a8\u9884\u5904\u7406\u811a\u672c\uff1a") + scriptPath + W("\uff0c\u8bf7\u68c0\u67e5 Python \u73af\u5883"), prepLog);
        NR_LOGE(W("\u9884\u5904\u7406"), W("\u65e0\u6cd5\u542f\u52a8\u9884\u5904\u7406\u811a\u672c\uff1a") + scriptPath);
        datasetPreparationService.setState(nerou::domain::TaskState::Failed);
        nerou::app::PipelineStore::getInstance().syncNodeTaskState(
            nerou::domain::NodeType::Preprocessing,
            nerou::domain::TaskState::Failed,
            W("\u542f\u52a8\u9884\u5904\u7406\u5931\u8d25"));
        setPrepLocked(false);
    } else {
        datasetPreparationService.setState(nerou::domain::TaskState::Running);
        nerou::app::PipelineStore::getInstance().syncNodeTaskState(
            nerou::domain::NodeType::Preprocessing,
            nerou::domain::TaskState::Running,
            W("\u9884\u5904\u7406\u8fdb\u884c\u4e2d"),
            0.0f);
    }
}

void MainComponent::setPrepLocked(bool locked)
{
    isPrepRunning = locked;
    prepStartBtn.setEnabled(!locked);
    prepStartBtn.setButtonText(locked ? W("\u9884\u5904\u7406\u4e2d...") : W("\u5f00\u59cb\u9884\u5904\u7406"));
    prepStatusLabel.setText(locked ? W("\u9884\u5904\u7406\u8fdb\u884c\u4e2d") : W("\u7b49\u5f85\u4e2d"), juce::dontSendNotification);
    prepStatusLabel.setColour(juce::Label::textColourId, locked ? C_PRIMARY : C_TEXT_S);

    for (auto* e : { &prepRawInput, &prepOutInput, &prepLowInput, &prepHighInput })
        e->setReadOnly(locked);
    prepSrCombo.setEnabled(!locked);
    prepChCombo.setEnabled(!locked);
    prepBrowseRaw.setEnabled(!locked);
    prepBrowseOut.setEnabled(!locked);
    prepSyncTrainBtn.setEnabled(!locked);
}

//==============================================================================
// ── Inference logic ────────────────────────────────────────────────────────
void MainComponent::updateInferProbabilityBars(const std::vector<float>& results)
{
    if (results.empty())
        return;
    const int showN = juce::jmin(inferActiveClassCount, (int)results.size(), kMaxInferClasses);
    for (int i = 0; i < kMaxInferClasses; ++i)
        inferConfValues[i] = 0.0;
    for (int i = 0; i < showN; ++i) {
        inferConfValues[i] = (double)results[(size_t)i];
        if (inferConfBars[i])
            inferConfBars[i]->repaint();
    }
}

void MainComponent::loadOnnxModel()
{
    auto path = inferModelInput.getText().trim();
    if (path.isEmpty()) {
        validationService.setState(nerou::domain::TaskState::Failed);
        updateLog(W("[\u8b66\u544a] \u8bf7\u5148\u9009\u62e9 ONNX \u6a21\u578b\u6587\u4ef6\u8def\u5f84"), inferLog);
        return;
    }
    updateLog(W("\u2192 \u52a0\u8f7d\u6a21\u578b\uff1a") + path, inferLog);
    NR_LOGI(W("\u63a8\u7406"), W("\u52a0\u8f7d ONNX \u6a21\u578b\uff1a") + path);
    bool loaded = false;
    {
        std::lock_guard<std::mutex> lock(onnxRunnerMutex);
        loaded = onnxRunner.loadModel(path);
    }
    if (loaded) {
        juce::File      modelFile(path);
        juce::File      manifest = modelFile.getParentDirectory().getChildFile("manifest.json");
        if (manifest.existsAsFile())
            applyManifestLabels(manifest);
        else
            applyDefaultInferLabels();

        {
            int k = onnxRunner.getOutputClassCount();
            if (k <= 0)
                k = 4;
            inferActiveClassCount = juce::jmax(1, juce::jmin(k, kMaxInferClasses));
        }
        for (int i = 0; i < inferActiveClassCount; ++i)
            inferClassLabels[i].setText(inferLabelNames[(size_t)i], juce::dontSendNotification);

        if (auto* p = appProperties.getUserSettings()) {
            p->setValue("lastOnnxPath", path);
            p->saveIfNeeded();
        }

        nerou::domain::ModelArtifact model;
        model.id = makeEntityId("model");
        model.projectId = resolveCurrentProjectId();
        model.modelName = juce::File(path).getFileNameWithoutExtension();
        model.version = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
        model.taskType = "classification";
        model.onnxPath = juce::File(path);
        model.manifestPath = manifest;
        model.labelsPath = model.onnxPath.getSiblingFile("labels.json");
        model.inputChannels = onnxRunner.getInputChannels();
        model.inputSamples = onnxRunner.getInputTimePoints();
        model.inputSampleRate = prepSrCombo.getSelectedId();
        model.outputClasses = onnxRunner.getOutputClassCount();
        model.createdAt = juce::Time::getCurrentTime();
        trainingService.setLastExportedModel(model);

        nerou::app::ModelInfo currentModelInfo;
        currentModelInfo.id = model.onnxPath.getFullPathName();
        currentModelInfo.name = model.onnxPath.getFileNameWithoutExtension();
        currentModelInfo.version = model.version;
        currentModelInfo.onnxPath = model.onnxPath;
        currentModelInfo.manifestPath = model.manifestPath;
        currentModelInfo.inputChannels = model.inputChannels;
        currentModelInfo.inputSamples = model.inputSamples;
        currentModelInfo.outputClasses = model.outputClasses;
        currentModelInfo.created = model.createdAt;
        nerou::app::Context().setCurrentModel(currentModelInfo);

        juce::Array<nerou::domain::ModelArtifact> cachedModels;
        cachedModels.add(model);
        modelRegistryService.setCachedModels(cachedModels);

        validationService.setState(nerou::domain::TaskState::Idle);

        juce::String shapeHint =
            W("\u671f\u671b\u8f93\u5165\u5143\u7d20\u6570\uff1a") + juce::String(onnxRunner.getExpectedInputElements())
            + W("\uff08\u82e5\u5df2\u77e5\uff0cC=") + juce::String(onnxRunner.getInputChannels()) + W(" T=")
            + juce::String(onnxRunner.getInputTimePoints()) + W("\uff09");
        inferStatusLabel.setText(W("\u6a21\u578b\u5df2\u52a0\u8f7d | ") + shapeHint, juce::dontSendNotification);
        inferStatusLabel.setColour(juce::Label::textColourId, C_SUCCESS);
        inferRunBtn.setEnabled(true);
        updateLog(W("[\u6210\u529f] ONNX \u6a21\u578b\u52a0\u8f7d\u5b8c\u6210 ") + shapeHint, inferLog);
        NR_LOGI(W("\u63a8\u7406"), W("\u6a21\u578b\u5c31\u7eea ") + shapeHint);
        rebuildInferOnnxCombo();
        resized();
    } else {
        validationService.setState(nerou::domain::TaskState::Failed);
        inferStatusLabel.setText(W("\u6a21\u578b\u52a0\u8f7d\u5931\u8d25"), juce::dontSendNotification);
        inferStatusLabel.setColour(juce::Label::textColourId, C_DANGER);
        inferRunBtn.setEnabled(false);
        updateLog(W("[\u9519\u8bef] \u6a21\u578b\u52a0\u8f7d\u5931\u8d25\uff0c\u8bf7\u786e\u8ba4\u6587\u4ef6\u8def\u5f84\u6b63\u786e\u4e14\u4f9d\u8d56\u5b8c\u6574"), inferLog);
        NR_LOGE(W("\u63a8\u7406"), W("ONNX \u6a21\u578b\u52a0\u8f7d\u5931\u8d25\uff1a") + path);
    }
}

void MainComponent::runInference()
{
    if (!ensureProjectContextReady(W("\u8fd0\u884c\u9a8c\u8bc1")))
        return;

    auto& context = nerou::app::Context();
    const juce::File modelFile(inferModelInput.getText().trim());
    const juce::File npzFile(inferDataInput.getText().trim());

    const auto syncRegistryValidationStatus = [this, &modelFile](bool passed) {
        auto cachedModels = modelRegistryService.getCachedModels();
        bool touched = false;
        for (int i = 0; i < cachedModels.size(); ++i)
        {
            auto& artifact = cachedModels.getReference(i);
            if (artifact.id == inferModelInput.getText().trim() || artifact.onnxPath == modelFile)
            {
                artifact.validationStatus = passed ? "passed" : "failed";
                artifact.isRecommended = passed;
                touched = true;
            }
        }

        if (touched)
            modelRegistryService.setCachedModels(cachedModels);
    };

    const auto publishValidationResult = [this, &context, &syncRegistryValidationStatus](nerou::domain::ValidationResult validation) {
        validationService.setLastResult(validation);
        validationService.setState(validation.passed ? nerou::domain::TaskState::Completed
                                                     : nerou::domain::TaskState::Failed);
        context.applyValidationResult(validation);
        syncRegistryValidationStatus(validation.passed);
        nerou::app::PipelineStore::getInstance().syncNodeTaskState(
            nerou::domain::NodeType::Validation,
            validation.passed ? nerou::domain::TaskState::Completed : nerou::domain::TaskState::Failed,
            validation.passed ? W("\u9a8c\u8bc1\u901a\u8fc7") : validation.conclusion,
            validation.passed ? 1.0f : 0.0f);
    };

    const auto writeValidationFailure = [this, &context, &publishValidationResult](const juce::String& conclusion,
                                                                                    const juce::String& riskLevel = "high") {
        nerou::domain::ValidationResult validation;
        validation.id = makeEntityId("validation");
        validation.projectId = resolveCurrentProjectId();
        validation.modelId = context.hasCurrentModel() ? context.getCurrentModel().id : inferModelInput.getText().trim();
        validation.datasetId = inferDataInput.getText().trim();
        validation.validationType = "offline";
        validation.passed = false;
        validation.conclusion = conclusion;
        validation.riskLevel = riskLevel;
        validation.validatedAt = juce::Time::getCurrentTime();
        validation.validatedBy = "desktop_gui";

        if (context.hasCurrentProject())
        {
            auto reportDir = ProjectPaths::getValidationsDir(context.getCurrentProject().rootPath)
                                 .getChildFile(validation.id);
            reportDir.createDirectory();
            validation.reportPath = reportDir.getChildFile("validation_result.json");
            nerou::domain::MetadataSerializer::writeValidationResultJson(validation, reportDir);
        }

        publishValidationResult(validation);
    };

    {
        std::lock_guard<std::mutex> lock(onnxRunnerMutex);
        if (!onnxRunner.isModelLoaded()) {
            writeValidationFailure(W("\u9a8c\u8bc1\u5931\u8d25\uff1a\u672a\u52a0\u8f7d ONNX \u6a21\u578b"));
            updateLog(W("[\u8b66\u544a] \u8bf7\u5148\u52a0\u8f7d\u6a21\u578b"), inferLog);
            return;
        }
    }

    if (!npzFile.existsAsFile()) {
        writeValidationFailure(W("\u9a8c\u8bc1\u5931\u8d25\uff1a\u6d4b\u8bd5\u6570\u636e\u6587\u4ef6\u4e0d\u5b58\u5728"));
        updateLog(W("[\u8b66\u544a] \u8bf7\u9009\u62e9\u6709\u6548\u7684 NPZ \u6216 NPY \u6587\u4ef6\uff08\u5f62\u72b6 N\u00d7C\u00d7T \u6216 C\u00d7T\uff09"), inferLog);
        return;
    }

    auto window = NpzEEGLoader::loadWindow(npzFile, 0);
    if (window.error.isNotEmpty()) {
        writeValidationFailure(W("\u9a8c\u8bc1\u5931\u8d25\uff1a") + window.error);
        updateLog(W("[\u9519\u8bef] \u6d4b\u8bd5\u6837\u672c\uff1a") + window.error, inferLog);
        return;
    }

    int expected = 0;
    {
        std::lock_guard<std::mutex> lock(onnxRunnerMutex);
        expected = onnxRunner.getExpectedInputElements();
    }
    if (expected > 0 && window.flat.size() != (size_t)expected) {
        writeValidationFailure(W("\u9a8c\u8bc1\u5931\u8d25\uff1a\u8f93\u5165\u5f62\u72b6\u4e0e\u6a21\u578b\u4e0d\u5339\u914d"));
        updateLog(W("[\u9519\u8bef] \u7b2c 1 \u6761\u6837\u672c\u5143\u7d20\u6570 ") + juce::String((int)window.flat.size())
                      + W(" \u2260 \u6a21\u578b\u671f\u671b ") + juce::String(expected) + W("\uff08\u672c\u6587\u4ef6 C=")
                      + juce::String(window.channels) + W(" T=") + juce::String(window.timePoints)
                      + W("\uff09\u3002\u8bf7\u4f7f\u7528\u4e0e\u8bad\u7ec3\u4e00\u81f4\u7684\u9884\u5904\u7406\u6570\u636e"),
                  inferLog);
        return;
    }

    SignalNorm::normalizeChannelZScoreClipped(window.flat, window.channels, window.timePoints, 10.0f);
    updateLog(W("\u2192 \u4f7f\u7528 ") + npzFile.getFileName() + W(" \u6267\u884c\u63a8\u7406\uff08\u5df2 z-score \u5f52\u4e00\u5316\uff09..."), inferLog);
    std::vector<float> previewResults;
    {
        std::lock_guard<std::mutex> lock(onnxRunnerMutex);
        previewResults = onnxRunner.runInference(window.flat);
    }

    if (previewResults.empty()) {
        writeValidationFailure(W("\u9a8c\u8bc1\u5931\u8d25\uff1a\u63a8\u7406\u8fd4\u56de\u7a7a\u7ed3\u679c"));
        updateLog(W("[\u9519\u8bef] \u63a8\u7406\u8fd4\u56de\u7a7a\u7ed3\u679c\uff0c\u8bf7\u68c0\u67e5\u6a21\u578b\u8f93\u5165\u8f93\u51fa\u540d\u4e0e\u7ef4\u5ea6"), inferLog);
        return;
    }

    updateInferProbabilityBars(previewResults);

    int best = (int)(std::max_element(previewResults.begin(), previewResults.end()) - previewResults.begin());
    const juce::String bestName = (best >= 0 && best < inferActiveClassCount) ? inferLabelNames[(size_t)best] : W("\u672a\u77e5");
    const float bestConfidence = previewResults[(size_t) juce::jlimit(0, (int) previewResults.size() - 1, best)];
    juce::String resultStr = W("[\u63a8\u7406\u7ed3\u679c] \u9884\u6d4b\uff1a") + bestName + W("\uff08\u7f6e\u4fe1\u5ea6 ")
                             + juce::String(bestConfidence * 100.0f, 1)
                             + W("%\uff09");
    updateLog(resultStr, inferLog);
    inferStatusLabel.setText(bestName, juce::dontSendNotification);
    inferStatusLabel.setColour(juce::Label::textColourId, C_PRIMARY);

    nerou::domain::ModelArtifact modelArtifact;
    if (const auto* exportedModel = trainingService.getLastExportedModel())
        if (exportedModel->onnxPath == modelFile)
            modelArtifact = *exportedModel;

    if (!modelArtifact.isValid())
    {
        modelArtifact.id = context.hasCurrentModel() ? context.getCurrentModel().id : modelFile.getFullPathName();
        modelArtifact.projectId = resolveCurrentProjectId();
        modelArtifact.modelName = modelFile.getFileNameWithoutExtension();
        modelArtifact.version = context.hasCurrentModel() ? context.getCurrentModel().version
                                                          : juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
        modelArtifact.taskType = "classification";
        modelArtifact.onnxPath = modelFile;
        modelArtifact.manifestPath = modelFile.getSiblingFile("manifest.json");
        modelArtifact.labelsPath = modelFile.getSiblingFile("labels.npz");
        modelArtifact.inputChannels = onnxRunner.getInputChannels();
        modelArtifact.inputSamples = onnxRunner.getInputTimePoints();
        modelArtifact.inputSampleRate = prepSrCombo.getSelectedId();
        modelArtifact.outputClasses = onnxRunner.getOutputClassCount();
        modelArtifact.createdAt = juce::Time::getCurrentTime();
        if (context.hasCurrentModel())
            modelArtifact.classNames = context.getCurrentModel().classNames;
    }

    auto validationOutputDir = ProjectPaths::getValidationsDir(context.getCurrentProject().rootPath)
                                   .getChildFile(makeEntityId("validation"));
    validationOutputDir.createDirectory();

    nerou::services::ValidationService::ValidationConfig validationConfig;
    validationConfig.projectId = resolveCurrentProjectId();
    validationConfig.datasetId = context.hasCurrentDataset() && context.getCurrentDataset().path == npzFile
                                     ? context.getCurrentDataset().id
                                     : npzFile.getFullPathName();
    validationConfig.testNpzFile = npzFile;
    validationConfig.labelsNpzFile = modelArtifact.labelsPath;
    validationConfig.resultSaveDir = validationOutputDir;
    validationConfig.validationType = "offline";
    validationConfig.validatedBy = "desktop_gui";

    validationService.setState(nerou::domain::TaskState::Running);
    nerou::app::PipelineStore::getInstance().syncNodeTaskState(
        nerou::domain::NodeType::Validation,
        nerou::domain::TaskState::Running,
        W("\u9a8c\u8bc1\u8fdb\u884c\u4e2d"),
        0.1f);
    updateLog(W("\u2192 \u6b63\u5f0f\u6267\u884c\u79bb\u7ebf\u9a8c\u8bc1\u4efb\u52a1..."), inferLog);

    nerou::domain::ValidationResult serviceResult;
    const bool validationOk = validationService.runOfflineValidation(
        modelArtifact,
        validationConfig,
        [this](double progress, juce::String statusMsg) {
            nerou::app::PipelineStore::getInstance().syncNodeTaskState(
                nerou::domain::NodeType::Validation,
                nerou::domain::TaskState::Running,
                statusMsg,
                (float) progress);
        },
        [&serviceResult](bool success, nerou::domain::ValidationResult result) {
            if (success || result.isValid())
                serviceResult = result;
        });

    if (const auto* lastResult = validationService.getLastResult())
        serviceResult = *lastResult;

    if (!serviceResult.isValid())
    {
        writeValidationFailure(W("\u9a8c\u8bc1\u5931\u8d25\uff1a\u672a\u751f\u6210\u6709\u6548\u9a8c\u8bc1\u7ed3\u679c"));
        updateLog(W("[\u9519\u8bef] \u9a8c\u8bc1\u670d\u52a1\u672a\u8fd4\u56de\u53ef\u7528\u7ed3\u679c"), inferLog);
        return;
    }

    serviceResult.reportPath = validationOutputDir.getChildFile("validation_result.json");
    publishValidationResult(serviceResult);

    if (serviceResult.passed)
    {
        const auto accuracyVar = serviceResult.metrics.getProperty("accuracy", juce::var());
        if (!accuracyVar.isVoid())
            updateLog(W("[\u9a8c\u8bc1\u7ed3\u8bba] Accuracy = ") + juce::String((double) accuracyVar * 100.0, 1) + W("%"), inferLog);
        updateLog(W("[\u9a8c\u8bc1\u901a\u8fc7] ") + serviceResult.conclusion, inferLog);
        inferStatusLabel.setText(bestName + W(" | \u9a8c\u8bc1\u901a\u8fc7"), juce::dontSendNotification);
        inferStatusLabel.setColour(juce::Label::textColourId, C_SUCCESS);
    }
    else
    {
        updateLog(W("[\u9a8c\u8bc1\u672a\u901a\u8fc7] ") + serviceResult.conclusion, inferLog);
        inferStatusLabel.setText(W("\u9a8c\u8bc1\u672a\u901a\u8fc7"), juce::dontSendNotification);
        inferStatusLabel.setColour(juce::Label::textColourId, C_DANGER);
    }

    if (!validationOk)
        updateLog(W("[\u63d0\u793a] \u5df2\u4fdd\u7559\u9a8c\u8bc1\u5931\u8d25\u7ed3\u679c\uff0c\u4fbf\u4e8e\u540e\u7eed\u8ffd\u8e2a"), inferLog);

    if (auto* p = appProperties.getUserSettings()) {
        p->setValue("lastInferNpzPath", npzFile.getFullPathName());
        p->saveIfNeeded();
    }
}

void MainComponent::applyDefaultInferLabels()
{
    const juce::String def[] = { W("\u5de6\u624b\u8fd0\u52a8\u60f3\u8c61"), W("\u53f3\u624b\u8fd0\u52a8\u60f3\u8c61"), W("\u53cc\u811a\u8fd0\u52a8\u60f3\u8c61"), W("\u9759\u606f\u6001") };
    for (int i = 0; i < kMaxInferClasses; ++i)
        inferLabelNames[(size_t)i] = (i < 4) ? def[i] : (W("\u7c7b\u522b ") + juce::String(i + 1));
}

void MainComponent::applyManifestLabels(const juce::File& manifestFile)
{
    applyDefaultInferLabels();
    auto root = juce::JSON::parse(nerou::core::loadTextFileAsUtf8(manifestFile));
    if (root.isVoid())
        return;
    if (auto* arr = root["labels"].getArray()) {
        const int n = juce::jmin(arr->size(), kMaxInferClasses);
        for (int i = 0; i < n; ++i)
            inferLabelNames[(size_t)i] = arr->getReference(i).toString();
    }
}

void MainComponent::touchTrainPauseFile(bool shouldPause)
{
    if (savePathInput.getText().trim().isEmpty())
        return;
    juce::File pauseFile = juce::File(savePathInput.getText().trim()).getChildFile(".nerou_train_pause");
    if (shouldPause)
        pauseFile.create();
    else
        pauseFile.deleteFile();
}

//==============================================================================
void MainComponent::updateLog(const juce::String& line, juce::TextEditor& target)
{
    // Called from UI thread only 鈫?direct append
if (juce::MessageManager::getInstance()->isThisTheMessageThread()) {
        juce::String t = target.getText();
        if (t.length() > 80000) t = t.substring(t.length() - 40000);
        target.setText(t + line + "\n");
        target.moveCaretToEnd();
    } else {
        // Background thread 鈫?queue
if (&target == &trainLog) {
            const juce::ScopedLock sl(trainLogLock);
            trainPendingLogs.add(line);
        } else if (&target == &prepLog) {
            const juce::ScopedLock sl(prepLogLock);
            prepPendingLogs.add(line);
        }
    }
}

juce::String MainComponent::formatDuration(int s)
{
    if (s < 60)   return juce::String(s) + W(" \u79d2");
    if (s < 3600) return juce::String(s / 60) + W(" \u5206 ") + juce::String(s % 60) + W(" \u79d2");
    return juce::String(s / 3600) + W(" \u5c0f\u65f6 ") + juce::String((s % 3600) / 60) + W(" \u5206");
}

//==============================================================================
void MainComponent::setStatusMsg(const juce::String& msg, bool isError)
{
    juce::String full = "  " + msg;
    statusBar.setText(full, juce::dontSendNotification);
    statusBar.setColour(juce::Label::textColourId,
                        isError ? C_DANGER : C_TEXT_S);
}

bool MainComponent::ensureProjectContextReady(const juce::String& actionName)
{
    auto& context = nerou::app::Context();
    if (context.hasCurrentProject())
        return true;

    const auto message = actionName + W("前请先选择当前项目。\n\n当前任务会把元数据、产物和日志归档到项目上下文，请先在项目入口选择项目后再继续。");
    setStatusMsg(W("请先选择当前项目，再") + actionName, true);
    const bool goToSelector = juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon,
                                                                 W("需要先选择项目"),
                                                                 message,
                                                                 W("去项目页"),
                                                                 W("取消"));
    if (goToSelector)
        tabOverviewBtn.triggerClick();
    return false;
}

bool MainComponent::ensureSubjectContextReady(const juce::String& actionName)
{
    if (!ensureProjectContextReady(actionName))
        return false;

    auto& context = nerou::app::Context();
    if (context.hasCurrentSubject())
        return true;

    const auto message = actionName + W("前请先选择当前受试者。\n\n采集会话与 recording.json 需要绑定受试者，请先在受试者入口设置当前对象后再继续。");
    setStatusMsg(W("请先选择当前受试者，再") + actionName, true);
    const bool goToSelector = juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon,
                                                                 W("需要先选择受试者"),
                                                                 message,
                                                                 W("去采集页"),
                                                                 W("取消"));
    if (goToSelector)
        tabOverviewBtn.triggerClick();
    return false;
}

void MainComponent::showProjectQuickSwitchMenu()
{
    auto& context = nerou::app::Context();
    auto projects = context.getRecentProjects(10);
    if (context.hasCurrentProject())
    {
        const auto& current = context.getCurrentProject();
        bool exists = false;
        for (const auto& project : projects)
        {
            if (project.id == current.id)
            {
                exists = true;
                break;
            }
        }
        if (!exists)
            projects.insert(0, current);
    }

    juce::PopupMenu menu;
    int itemId = 1;
    for (const auto& project : projects)
    {
        menu.addItem(itemId++,
                     formatProjectMenuLabel(project),
                     true,
                     context.hasCurrentProject() && context.getCurrentProject().id == project.id);
    }

    if (projects.isEmpty())
    {
        menu.addItem(1003, W("新建项目..."));
        menu.addSeparator();
        menu.addItem(1001, W("去项目页"));
    }
    else
    {
        if (context.hasCurrentProject())
            menu.addItem(1004, W("编辑当前项目..."));
        menu.addItem(1003, W("新建项目..."));
        menu.addSeparator();
        menu.addItem(1001, W("管理项目..."));
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&topProjectContextBtn),
                       [this, projects](int result) {
                           if (result == 1004)
                           {
                               showEditCurrentProjectDialog();
                               return;
                           }

                           if (result == 1003)
                           {
                               showCreateProjectDialog();
                               return;
                           }

                           if (result == 1001)
                           {
                               tabOverviewBtn.triggerClick();
                               return;
                           }

                           if (result > 0 && result <= projects.size())
                           {
                               const auto selected = projects[result - 1];
                               auto& contextRef = nerou::app::Context();
                               const bool projectChanged = !contextRef.hasCurrentProject()
                                                        || contextRef.getCurrentProject().id != selected.id;
                               if (projectChanged)
                                   contextRef.clearCurrentSubject();
                               contextRef.setCurrentProject(selected);
                               setStatusMsg(W("已切换当前项目：") + (selected.name.isNotEmpty() ? selected.name : selected.id));
                           }
                       });
}

void MainComponent::showCreateProjectDialog()
{
    juce::AlertWindow dialog(W("新建项目"),
                             W("请输入项目名称。创建后会自动初始化项目目录，并切换为当前项目。"),
                             juce::AlertWindow::NoIcon);
    dialog.addTextEditor("projectName", {}, W("项目名称"));
    dialog.addTextEditor("projectDescription", {}, W("项目描述（可选）"));
    dialog.addButton(W("创建"), 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog.addButton(W("取消"), 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (dialog.runModalLoop() != 1)
        return;

    const auto projectName = dialog.getTextEditorContents("projectName").trim();
    const auto projectDescription = dialog.getTextEditorContents("projectDescription").trim();
    if (projectName.isEmpty())
    {
        setStatusMsg(W("项目名称不能为空"), true);
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               W("创建失败"),
                                               W("项目名称不能为空。"));
        return;
    }

    const auto targetDir = ProjectPaths::getProjectDir().getChildFile(projectName);
    if (targetDir.exists())
    {
        setStatusMsg(W("项目目录已存在，请使用其他名称"), true);
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               W("创建失败"),
                                               W("同名项目目录已存在，请使用其他项目名称。"));
        return;
    }

    nerou::app::Context().createProject(projectName, projectDescription);
    setStatusMsg(W("已创建并切换到项目：") + projectName);
}

void MainComponent::showEditCurrentProjectDialog()
{
    auto& context = nerou::app::Context();
    if (!context.hasCurrentProject())
    {
        setStatusMsg(W("当前没有可编辑的项目"), true);
        return;
    }

    const auto current = context.getCurrentProject();
    juce::AlertWindow dialog(W("编辑项目"),
                             W("修改当前项目的名称和描述。项目目录不会自动重命名，但项目元数据会立即更新。"),
                             juce::AlertWindow::NoIcon);
    dialog.addTextEditor("projectName", current.name, W("项目名称"));
    dialog.addTextEditor("projectDescription", current.description, W("项目描述（可选）"));
    dialog.addButton(W("保存"), 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog.addButton(W("取消"), 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (dialog.runModalLoop() != 1)
        return;

    const auto projectName = dialog.getTextEditorContents("projectName").trim();
    const auto projectDescription = dialog.getTextEditorContents("projectDescription").trim();
    if (projectName.isEmpty())
    {
        setStatusMsg(W("项目名称不能为空"), true);
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               W("保存失败"),
                                               W("项目名称不能为空。"));
        return;
    }

    auto updated = current;
    updated.name = projectName;
    updated.description = projectDescription;
    updated.lastModified = juce::Time::getCurrentTime();
    context.setCurrentProject(updated);
    setStatusMsg(W("已更新项目：") + updated.name);
}

void MainComponent::showSubjectQuickSwitchMenu()
{
    auto& context = nerou::app::Context();
    if (!context.hasCurrentProject())
    {
        setStatusMsg(W("请先选择当前项目，再切换受试者"), true);
        tabOverviewBtn.triggerClick();
        return;
    }

    auto subjects = context.getProjectSubjects();
    if (context.hasCurrentSubject())
    {
        const auto& current = context.getCurrentSubject();
        bool exists = false;
        for (const auto& subject : subjects)
        {
            if (subject.id == current.id)
            {
                exists = true;
                break;
            }
        }
        if (!exists)
            subjects.insert(0, current);
    }

    juce::PopupMenu menu;
    if (subjects.isEmpty())
    {
        menu.addItem(1003, W("新建受试者..."));
        menu.addSeparator();
        menu.addItem(1002, W("去采集页"));
    }
    else
    {
        int itemId = 1;
        for (const auto& subject : subjects)
        {
            menu.addItem(itemId++,
                         formatSubjectMenuLabel(subject),
                         true,
                         context.hasCurrentSubject() && context.getCurrentSubject().id == subject.id);
        }
        menu.addSeparator();
        if (context.hasCurrentSubject())
            menu.addItem(1004, W("编辑当前受试者..."));
        menu.addItem(1003, W("新建受试者..."));
        menu.addItem(1002, W("管理受试者..."));
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&topSubjectContextBtn),
                       [this, subjects](int result) {
                           if (result == 1004)
                           {
                               showEditCurrentSubjectDialog();
                               return;
                           }

                           if (result == 1003)
                           {
                               showCreateSubjectDialog();
                               return;
                           }

                           if (result == 1002)
                           {
                               tabOverviewBtn.triggerClick();
                               return;
                           }

                           if (result > 0 && result <= subjects.size())
                           {
                               const auto selected = subjects[result - 1];
                               nerou::app::Context().setCurrentSubject(selected);
                               setStatusMsg(W("已切换当前受试者：") + selected.getDisplayName());
                           }
                       });
}

void MainComponent::showCreateSubjectDialog()
{
    auto& context = nerou::app::Context();
    if (!context.hasCurrentProject())
    {
        setStatusMsg(W("请先选择当前项目，再创建受试者"), true);
        tabOverviewBtn.triggerClick();
        return;
    }

    juce::AlertWindow dialog(W("新建受试者"),
                             W("请输入受试者名称。创建后会自动加入当前项目并设为当前受试者。"),
                             juce::AlertWindow::NoIcon);
    dialog.addTextEditor("subjectName", {}, W("受试者名称"));
    dialog.addTextEditor("subjectNotes", {}, W("备注（可选）"));
    dialog.addButton(W("创建"), 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog.addButton(W("取消"), 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (dialog.runModalLoop() != 1)
        return;

    const auto subjectName = dialog.getTextEditorContents("subjectName").trim();
    const auto subjectNotes = dialog.getTextEditorContents("subjectNotes").trim();
    if (subjectName.isEmpty())
    {
        setStatusMsg(W("受试者名称不能为空"), true);
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               W("创建失败"),
                                               W("受试者名称不能为空。"));
        return;
    }

    nerou::app::SubjectInfo subject;
    subject.id = makeEntityId("subject");
    subject.name = subjectName;
    subject.notes = subjectNotes;
    context.addSubject(subject);
    context.setCurrentSubject(subject);
    setStatusMsg(W("已创建并切换到受试者：") + subject.getDisplayName());
}

void MainComponent::showEditCurrentSubjectDialog()
{
    auto& context = nerou::app::Context();
    if (!context.hasCurrentSubject())
    {
        setStatusMsg(W("当前没有可编辑的受试者"), true);
        return;
    }

    const auto current = context.getCurrentSubject();
    juce::AlertWindow dialog(W("编辑受试者"),
                             W("修改当前受试者的名称或备注。保存后会自动写回当前项目。"),
                             juce::AlertWindow::NoIcon);
    dialog.addTextEditor("subjectName", current.name, W("受试者名称"));
    dialog.addTextEditor("subjectNotes", current.notes, W("备注（可选）"));
    dialog.addButton(W("保存"), 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog.addButton(W("取消"), 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (dialog.runModalLoop() != 1)
        return;

    const auto subjectName = dialog.getTextEditorContents("subjectName").trim();
    const auto subjectNotes = dialog.getTextEditorContents("subjectNotes").trim();
    if (subjectName.isEmpty())
    {
        setStatusMsg(W("受试者名称不能为空"), true);
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               W("保存失败"),
                                               W("受试者名称不能为空。"));
        return;
    }

    auto updated = current;
    updated.name = subjectName;
    updated.notes = subjectNotes;
    context.addSubject(updated);
    context.setCurrentSubject(updated);
    setStatusMsg(W("已更新受试者：") + updated.getDisplayName());
}

//==============================================================================
void MainComponent::mouseDown(const juce::MouseEvent& e)
{
    // 移除废弃的弹出菜单导出摘要辑
Component::mouseDown(e);
}

//==============================================================================
bool MainComponent::keyPressed(const juce::KeyPress& key) { return handleMainShortcuts(key); }

bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    juce::ignoreUnused(originatingComponent);
    return handleMainShortcuts(key);
}

bool MainComponent::handleMainShortcuts(const juce::KeyPress& key)
{
    // 采集页导联分页：>32 导联时，↑/↓ 翻页
    if (!key.getModifiers().isAnyModifierKeyDown() && currentTab == Tab::Acquisition)
    {
        if (key.getKeyCode() == juce::KeyPress::upKey || key.getKeyCode() == juce::KeyPress::downKey)
        {
            applyRealtimeChannelPaging(false);
            if (realtimeChannelPageSize > 0)
            {
                const int selectedChannels =
                    realtimeChannelsCombo.getSelectedId() > 0 ? realtimeChannelsCombo.getSelectedId() : 8;
                const int totalChannels = juce::jmax(1, selectedChannels);
                const int maxPage = juce::jmax(0, (totalChannels - 1) / realtimeChannelPageSize);
                const int oldPage = realtimeChannelPageIndex;
                if (key.getKeyCode() == juce::KeyPress::upKey)
                    realtimeChannelPageIndex = juce::jmax(0, realtimeChannelPageIndex - 1);
                else
                    realtimeChannelPageIndex = juce::jmin(maxPage, realtimeChannelPageIndex + 1);

                if (realtimeChannelPageIndex != oldPage)
                {
                    applyRealtimeChannelPaging(false);
                    return true;
                }
            }
        }
    }

    // Ctrl+Enter  → 开始训练
if (key == juce::KeyPress(juce::KeyPress::returnKey, juce::ModifierKeys::ctrlModifier, 0))
    {
        if (currentTab == Tab::Training && !isTrainingActive)
        {
            startTraining();
            return true;
        }
    }
    // Ctrl+Enter  → 开始训练
    // Escape  停止训练
if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        if (commandPalette_.isVisiblePalette())
        {
            commandPalette_.setVisiblePalette(false);
            return true;
        }
        if (isTrainingActive)
        {
            stopTraining();
            return true;
        }
        if (isPrepRunning)
        {
            prepBridge.stopTask();
            {
                [[maybe_unused]] juce::uint32 ex = 0;
                [[maybe_unused]] bool uc         = false;
                prepBridge.tryConsumeRunResult(ex, uc);
            }
            setPrepLocked(false);
            prepStatusLabel.setText(W("\u5df2\u4e2d\u6b62"), juce::dontSendNotification);
            prepStatusLabel.setColour(juce::Label::textColourId, C_WARNING);
            updateLog(W("[\u4fe1\u606f] \u9884\u5904\u7406\u5df2\u4e2d\u6b62"), prepLog);
            return true;
        }
    }

    // Ctrl+K 命令面板；Ctrl+0~4 与快捷键帮助一致
    if (key.getModifiers().isCtrlDown() && !key.getModifiers().isShiftDown()
        && !key.getModifiers().isAltDown())
    {
        if (key.getKeyCode() == 'k' || key.getKeyCode() == 'K')
        {
            commandPalette_.setVisiblePalette(!commandPalette_.isVisiblePalette());
            if (commandPalette_.isVisiblePalette())
                commandPalette_.toFront(true);
            return true;
        }
        if (key.getKeyCode() == '0')
        {
            switchTab(Tab::Training);
            return true;
        }
        if (key.getKeyCode() == '1')
        {
            switchTab(Tab::Training);
            return true;
        }
        if (key.getKeyCode() == '2')
        {
            switchTab(Tab::Inference);
            return true;
        }
        if (key.getKeyCode() == '3')
        {
            switchTab(Tab::Training);
            return true;
        }
        if (key.getKeyCode() == '4')
        {
            switchTab(Tab::Inference);
            return true;
        }
    }

    if (key.getModifiers().isCtrlDown() && key.getModifiers().isShiftDown())
    {
        const int kc = key.getKeyCode();
        if ((kc == 'r' || kc == 'R') && currentTab == Tab::Training && !isTrainingActive)
        {
            refreshTrainPreflight();
            return true;
        }
    }

    // Ctrl+/ → 切换 AI Agent 面板
    if (key.getModifiers().isCtrlDown()
        && (key.getKeyCode() == '/' || key.getKeyCode() == juce::KeyPress::F1Key))
    {
        agentPanel.setExpanded(!agentPanel.isExpanded());
        resized();
        return true;
    }

    // Ctrl+, → 打开/关闭设置对话框
    if (key.getModifiers().isCtrlDown()
        && !key.getModifiers().isShiftDown()
        && (key.getKeyCode() == ',' || key.getTextCharacter() == ','))
    {
        if (settingsDialog.isShowing())
            settingsDialog.hideOverlay();
        else
        {
            const auto perf = nerou::app::RuntimeSettingsStore::loadPerformance(appProperties.getUserSettings());
            settingsDialog.setAccelerationState(perf.accelerationMode, onnxRunner.getActiveProvider());
            settingsDialog.showOverlay(this);
        }
        return true;
    }

    // Ctrl+L → 打开/关闭系统日志面板
    if (key.getModifiers().isCtrlDown()
        && !key.getModifiers().isShiftDown()
        && (key.getKeyCode() == 'L' || key.getKeyCode() == 'l'
            || key.getTextCharacter() == 'l' || key.getTextCharacter() == 'L'))
    {
        if (systemLogPanel.isShowing())
            systemLogPanel.hideOverlay();
        else
            showSystemLogPanel();
        return true;
    }

    // ? → 显示快捷键帮助 Overlay
    if (!key.getModifiers().isCtrlDown() && !key.getModifiers().isAltDown()
        && key.getTextCharacter() == '?')
    {
        if (shortcutHelpOverlay.isShowing())
            shortcutHelpOverlay.hideOverlay();
        else
            shortcutHelpOverlay.showOverlay(this);
        return true;
    }

    return false;
}

void MainComponent::attachShortcutKeyListeners()
{
    juce::Component* cs[] = { &nameInput,
                              &dataPathInput,
                              &epochInput,
                              &classCountInput,
                              &savePathInput,
                              &prepRawInput,
                              &prepOutInput,
                              &prepLowInput,
                              &prepHighInput,
                              &inferModelInput,
                              &inferDataInput,
                              &realtimePlaybackInput,
                              &realtimeLiveConnInput,
                              &trainLog,
                              &prepLog,
                              &inferLog,
                              &trainTemplateCombo,
                              &batchCombo,
                              &lrCombo,
                              &prepSrCombo,
                              &prepChCombo,
                              &inferOnnxCombo,
                              &realtimeSourceCombo,
                              &realtimeChannelsCombo,
                              &realtimeRateCombo,
                              &realtimeUvRangeCombo,
                              &realtimeFilterCombo,
                              &realtimeMontageCombo,
                              &realtimeWinLenCombo,
                              &realtimeNumWinCombo };
    for (auto* c : cs)
        c->addKeyListener(this);
}

void MainComponent::detachShortcutKeyListeners()
{
    juce::Component* cs[] = { &nameInput,
                              &dataPathInput,
                              &epochInput,
                              &classCountInput,
                              &savePathInput,
                              &prepRawInput,
                              &prepOutInput,
                              &prepLowInput,
                              &prepHighInput,
                              &inferModelInput,
                              &inferDataInput,
                              &realtimePlaybackInput,
                              &realtimeLiveConnInput,
                              &trainLog,
                              &prepLog,
                              &inferLog,
                              &trainTemplateCombo,
                              &batchCombo,
                              &lrCombo,
                              &prepSrCombo,
                              &prepChCombo,
                              &inferOnnxCombo,
                              &realtimeSourceCombo,
                              &realtimeChannelsCombo,
                              &realtimeRateCombo,
                              &realtimeUvRangeCombo,
                              &realtimeFilterCombo,
                              &realtimeMontageCombo,
                              &realtimeWinLenCombo,
                              &realtimeNumWinCombo };
    for (auto* c : cs)
        c->removeKeyListener(this);
}

//==============================================================================
void MainComponent::rebuildTrainTemplateCombo(bool refreshPreflightAfter)
{
    suppressTrainTemplateCallbacks = true;
    trainTemplateCombo.clear(juce::dontSendNotification);
    trainTemplateModelIds.clear();
    trainTemplateCombo.addItem(
        W("\u81ea\u5b9a\u4e49\uff08\u624b\u52a8\u586b\u5199\u6a21\u578b\u540d\u79f0\uff09"), 1);

    juce::File root = ProjectPaths::getOnnxModelsDirectory();
    int        nextId = 2;
    if (root.isDirectory())
    {
        juce::Array<juce::File> subs;
        root.findChildFiles(subs, juce::File::findDirectories, false);
        std::vector<juce::File> sorted;
        sorted.reserve((size_t)subs.size());
        for (auto& d : subs)
        {
            if (d.getChildFile("manifest.json").existsAsFile())
                sorted.push_back(d);
        }
        std::sort(sorted.begin(), sorted.end(), [](const juce::File& a, const juce::File& b) {
            return a.getFileName().compareIgnoreCase(b.getFileName()) < 0;
        });

        for (auto& dir : sorted)
        {
            juce::File   mf      = dir.getChildFile("manifest.json");
            juce::String modelId = dir.getFileName();
            juce::String desc;
            if (auto parsed = juce::JSON::parse(nerou::core::loadTextFileAsUtf8(mf)); parsed.isObject())
                if (auto* o = parsed.getDynamicObject())
                {
                    if (o->hasProperty("modelId"))
                        modelId = o->getProperty("modelId").toString();
                    if (o->hasProperty("description"))
                        desc = o->getProperty("description").toString();
                }

            juce::String itemText = modelId;
            if (desc.isNotEmpty())
            {
                juce::String shortDesc =
                    desc.length() > 40 ? (desc.substring(0, 40) + juce::String("...")) : desc;
                // 使用 ASCII 分隔符，避免 " 在部分系统编码下显示â 等乱
itemText = modelId + juce::String(" | ") + shortDesc;
            }
            trainTemplateCombo.addItem(itemText, nextId);
            trainTemplateModelIds.add(modelId);
            ++nextId;
        }
    }

    suppressTrainTemplateCallbacks = false;

    juce::String cur = nameInput.getText().trim();
    for (int i = 0; i < trainTemplateModelIds.size(); ++i)
    {
        if (trainTemplateModelIds[i] == cur)
        {
            trainTemplateCombo.setSelectedId(i + 2, juce::dontSendNotification);
            if (refreshPreflightAfter)
                refreshTrainPreflight();
            return;
        }
    }
    trainTemplateCombo.setSelectedId(1, juce::dontSendNotification);
    if (refreshPreflightAfter)
        refreshTrainPreflight();
}

void MainComponent::applyTrainTemplateFromCombo()
{
    if (suppressTrainTemplateCallbacks)
        return;
    const int id = trainTemplateCombo.getSelectedId();
    if (id <= 1)
    {
        refreshTrainPreflight();
        return;
    }
    const int idx = id - 2;
    if (idx >= 0 && idx < trainTemplateModelIds.size())
        nameInput.setText(trainTemplateModelIds[idx]);
    persistTrainingFields();
    refreshTrainPreflight();
}

void MainComponent::rebuildInferOnnxCombo()
{
    suppressInferOnnxCallbacks = true;
    inferOnnxCombo.clear(juce::dontSendNotification);
    inferOnnxPaths.clear();
    inferOnnxCombo.addItem(W("\u2014 \u624b\u52a8\u6d4f\u89c8\u6216\u8f93\u5165\u8def\u5f84 \u2014"), 1);

    juce::StringArray seenLower;
    std::vector<std::pair<juce::String, juce::String>> rows;

    auto tryAddFile = [&](const juce::File& f, const juce::String& label) {
        if (!f.existsAsFile() || !f.hasFileExtension(".onnx"))
            return;
        const juce::String key = f.getFullPathName().toLowerCase();
        if (seenLower.contains(key))
            return;
        seenLower.add(key);
        rows.push_back({ label, f.getFullPathName() });
    };

    juce::File modelsDir = ProjectPaths::getOnnxModelsDirectory();
    if (modelsDir.isDirectory())
    {
        juce::Array<juce::File> found;
        modelsDir.findChildFiles(found, juce::File::findFiles, true, "*.onnx");
        std::vector<juce::File> sorted;
        sorted.reserve((size_t)found.size());
        for (const auto& x : found)
            sorted.push_back(x);
        std::sort(sorted.begin(), sorted.end(), [](const juce::File& a, const juce::File& b) {
            return a.getFullPathName().compareIgnoreCase(b.getFullPathName()) < 0;
        });
        for (const auto& f : sorted)
        {
            juce::String rel = f.getRelativePathFrom(modelsDir);
            if (rel.isEmpty())
                rel = f.getFileName();
            tryAddFile(f, rel);
        }
    }

    juce::File trainSaveDir(savePathInput.getText().trim());
    if (trainSaveDir.isDirectory())
    {
        juce::Array<juce::File> found;
        trainSaveDir.findChildFiles(found, juce::File::findFiles, false, "*.onnx");
        found.sort();
        for (const auto& f : found)
            tryAddFile(f, W("[导出] ") + f.getFileName());
    }

    if (auto* p = appProperties.getUserSettings())
    {
        juce::File last(p->getValue("lastOnnxPath", {}));
        tryAddFile(last, W("[上次] ") + last.getFileName());
    }

    int nextId = 2;
    for (const auto& pr : rows)
    {
        inferOnnxCombo.addItem(pr.first, nextId++);
        inferOnnxPaths.add(pr.second);
    }

    suppressInferOnnxCallbacks = false;

    juce::String cur = inferModelInput.getText().trim();
    for (int i = 0; i < inferOnnxPaths.size(); ++i)
    {
        if (inferOnnxPaths[i].equalsIgnoreCase(cur))
        {
            inferOnnxCombo.setSelectedId(i + 2, juce::dontSendNotification);
            return;
        }
    }
    inferOnnxCombo.setSelectedId(1, juce::dontSendNotification);
}

void MainComponent::applyInferOnnxFromCombo()
{
    if (suppressInferOnnxCallbacks)
        return;
    const int id = inferOnnxCombo.getSelectedId();
    if (id <= 1)
        return;
    const int idx = id - 2;
    if (idx >= 0 && idx < inferOnnxPaths.size())
    {
        inferModelInput.setText(inferOnnxPaths[idx]);
        loadOnnxModel();
    }
}

//==============================================================================
void MainComponent::setupTooltips()
{
    // ── 侧边Tab 按钮（基础文案；updateTabGuardTooltips 会追加守卫/角标说明）────────
    tabOverviewBtn.setTooltip   (sidebarTabBaseTooltip(nerou::app::WorkflowTab::Overview));
    tabAcquisitionBtn.setTooltip(sidebarTabBaseTooltip(nerou::app::WorkflowTab::Acquisition));
    tabDataPrepBtn.setTooltip   (sidebarTabBaseTooltip(nerou::app::WorkflowTab::DataPrep));
    tabTrainBtn.setTooltip      (sidebarTabBaseTooltip(nerou::app::WorkflowTab::Training));
    tabInferBtn.setTooltip      (sidebarTabBaseTooltip(nerou::app::WorkflowTab::Inference));
    topProjectContextBtn.setTooltip(W("\u5feb\u6377\u5207\u6362\u5f53\u524d\u9879\u76ee\uff0c\u5217\u51fa\u6700\u8fd1\u9879\u76ee\u5e76\u53ef\u8df3\u8f6c\u5230\u9879\u76ee\u9875"));
    topSubjectContextBtn.setTooltip(W("\u5feb\u6377\u5207\u6362\u5f53\u524d\u53d7\u8bd5\u8005\uff0c\u5217\u51fa\u5f53\u524d\u9879\u76ee\u4e0b\u7684\u53d7\u8bd5\u8005"));

    uiExpertToggle.setTooltip(W("\u4e13\u5bb6\u6a21\u5f0f\uff1a\u663e\u793a\u5206\u7c7b\u6570\u3001\u5b66\u4e60\u7387\u3001\u9884\u5904\u7406\u5e26\u901a\u3001"
                                 "\u5bfc\u8054\u6807\u6ce8\u4e0e\u591a\u7a97\u7b49\u9ad8\u7ea7\u9009\u9879\u3002"
                                 "\u65b0\u7528\u6237\u53ef\u5148\u5173\u95ed\u4ee5\u51cf\u5c11\u5e72\u6270\u3002"));
    trainPreflightTitle.setTooltip(
        W("\u8bad\u7ec3\u524d\u68c0\u67e5\uff1aNPZ \u6570\u91cf\u4e0e\u7a97\u53e3 C/T\u3001\u4e0e manifest \u5bf9\u9f50\u3002Ctrl+Shift+C \u590d\u5236\u3001Ctrl+Shift+S \u5b58\u684c\u9762 TXT\u3002\u53f3\u952e\u590d\u5236"));
    trainPreflightDetail.setTooltip(
        W("\u8bad\u7ec3\u524d\u68c0\u67e5\u8be6\u60c5\u3002Ctrl+Shift+C/\u53f3\u952e\u590d\u5236\uff1bCtrl+Shift+S \u5b58\u684c\u9762 TXT"));
    trainPreflightRefreshBtn.setTooltip(
        W("\u91cd\u65b0\u626b\u63cf\u6570\u636e\u96c6\u4e0e manifest  (Ctrl+Shift+R)\u3002Ctrl+Shift+C \u590d\u5236\u3001Ctrl+Shift+S \u4fdd\u5b58 TXT \u5230\u684c\u9762\u3002\u53f3\u952e\u590d\u5236"));
    trainPreflightSidebarBadge.setTooltip(
        W("\u8bad\u7ec3\u9875\u4fa7\u680f\u9884\u68c0\u6458\u8981\u3002\u53f3\u952e/Ctrl+Shift+C \u590d\u5236\uff1bCtrl+Shift+S \u5b58\u684c\u9762 TXT"));
    realtimeAlignLabel.setTooltip(W("\u8bbe\u5907\u5bfc\u8054\u6570 /\u7a97\u957f T \u4e0e\u5df2\u52a0\u8f7d ONNX \u8f93\u5165\u5f62\u72b6 C\u00d7T \u662f\u5426\u4e00\u81f4"));

    trainTemplateCombo.setTooltip(
        W("\u4ece onnx/models \u4e0b\u542b manifest.json \u7684\u6a21\u677f\u4e2d\u9009\u62e9\u6a21\u578b ID\uff1b\u9009\u300c\u81ea\u5b9a\u4e49\u300d\u53ef\u624b\u5199\u540d\u79f0"));
    nameInput.setTooltip    (W("\u6a21\u578b\u540d\u79f0\uff0c\u5c06\u4f5c\u4e3a ONNX \u5bfc\u51fa\u6587\u4ef6\u540d\uff08\u4f8b\u5982 eegnet_v1\uff09"));
    dataPathInput.setTooltip(
        W("\u8bad\u7ec3\u6570\u636e NPZ \u76ee\u5f55\uff1b\u624b\u52a8\u7f16\u8f91\u540e\u5931\u7126\u6216\u56de\u8f66\u5237\u65b0\u201c\u8bad\u7ec3\u524d\u68c0\u67e5\u201d"));
    browseDirBtn.setTooltip (W("\u9009\u62e9\u9884\u5904\u7406\u540e\u7684 NPZ \u6570\u636e\u96c6\u76ee\u5f55"));
    epochInput.setTooltip   (W("\u8bad\u7ec3\u603b\u8f6e\u6570\uff08\u5efa\u8bae\uff1a10 ~ 200 \u8f6e\uff09"));
    classCountInput.setTooltip(W("\u5206\u7c7b\u5934\u8f93\u51fa\u7ef4\u5ea6\uff1b\u5e94\u2265\u6570\u636e\u4e2d\u6700\u5927\u6807\u7b7e + 1"));
    batchCombo.setTooltip   (W("\u6bcf\u6b21\u68af\u5ea6\u66f4\u65b0\u7684\u6837\u672c\u6570\u91cf\uff08\u8f83\u5927 batch \u9700\u8981\u66f4\u591a\u663e\u5b58\uff09"));
    lrCombo.setTooltip      (W("\u521d\u59cb\u5b66\u4e60\u7387\uff1bEEGNet \u63a8\u8350 0.0005"));
    savePathInput.setTooltip(W("ONNX \u6a21\u578b\u5bfc\u51fa\u8def\u5f84\uff08\u4f8b\u5982\uff1aonnx/deploy/\uff09"));
    browseSaveBtn.setTooltip(W("\u9009\u62e9\u8bad\u7ec3\u5b8c\u6210\u540e\u6a21\u578b\u7684\u4fdd\u5b58\u76ee\u5f55"));

    startBtn.setTooltip    (W("\u5f00\u59cb\u8bad\u7ec3\u4efb\u52a1 (Ctrl+Enter)"));
    pauseBtn.setTooltip    (W("\u6682\u505c / \u7ee7\u7eed\uff08\u7531 train.py \u68c0\u6d4b .nerou_train_pause \u6587\u4ef6\uff09"));
    stopBtn.setTooltip     (W("\u5f3a\u5236\u7ec8\u6b62\u5f53\u524d\u8bad\u7ec3 (Esc)"));
    saveLogBtn.setTooltip  (W("\u5c06\u8bad\u7ec3\u65e5\u5fd7\u5bfc\u51fa\u5230\u684c\u9762 TXT \u6587\u4ef6"));
    saveChartBtn.setTooltip(W("\u5bfc\u51fa Loss \u4e0e Accuracy \u66f2\u7ebf\u4e3a\u684c\u9762 PNG"));
    clearLogBtn.setTooltip (W("\u6e05\u7a7a\u8bad\u7ec3\u65e5\u5fd7\u9762\u677f"));

    // ── 数据预处───────────────────────────────────────────────────────────
    prepRawInput.setTooltip  (W("EDF/BDF \u539f\u59cb\u8111\u7535\u6570\u636e\u6240\u5728\u76ee\u5f55"));
    prepBrowseRaw.setTooltip (W("\u6d4f\u89c8\u9009\u62e9\u539f\u59cb\u6570\u636e\u76ee\u5f55"));
    prepOutInput.setTooltip  (W("\u9884\u5904\u7406\u540e NPZ \u6570\u636e\u4fdd\u5b58\u76ee\u5f55\uff0c\u8bad\u7ec3\u5c06\u4ece\u6b64\u8def\u5f84\u52a0\u8f7d"));
    prepBrowseOut.setTooltip (W("\u6d4f\u89c8\u9009\u62e9 NPZ \u8f93\u51fa\u76ee\u5f55"));
    prepSrCombo.setTooltip   (W("\u91cd\u91c7\u6837\u76ee\u6807\u9891\u7387\uff08Hz\uff09\uff0cEEGNet \u6807\u51c6\u8f93\u5165\u901a\u5e38\u4e3a 256 Hz"));
    prepLowInput.setTooltip  (W("\u5e26\u901a\u6ee4\u6ce2\u4f4e\u9891\u622a\u6b62\uff08Hz\uff09\uff0c\u5178\u578b\u503c 1 Hz"));
    prepHighInput.setTooltip (W("\u5e26\u901a\u6ee4\u6ce2\u9ad8\u9891\u622a\u6b62\uff08Hz\uff09\uff0c\u5178\u578b\u503c 40 Hz"));
    prepChCombo.setTooltip   (W("\u6709\u6548 EEG \u901a\u9053\u6570\uff0c\u7528\u4e8e\u6570\u636e\u5f62\u72b6\u6821\u9a8c"));
    prepStartBtn.setTooltip  (W("\u542f\u52a8 Python \u9884\u5904\u7406\u811a\u672c\uff08preprocess.py\uff09"));
    prepClearBtn.setTooltip  (W("\u6e05\u7a7a\u9884\u5904\u7406\u65e5\u5fd7\u9762\u677f"));
    prepSyncTrainBtn.setTooltip(W("\u5c06\u9884\u5904\u7406\u8f93\u51fa\u76ee\u5f55\u540c\u6b65\u5230\u8bad\u7ec3\u6570\u636e\u96c6\u8def\u5f84\uff0c\u5e76\u5207\u6362\u5230\u300c\u8bad\u7ec3\u914d\u7f6e\u300d"));

    realtimeSourceCombo.setTooltip(
        W("\u6570\u636e\u6e90\u5df2\u56fa\u5b9a\u4e3a\u8bad\u7ec3\u6587\u4ef6\u6570\u636e\uff0c\u4e0d\u63d0\u4f9b\u5408\u6210\u6e90\u6216\u5b9e\u65f6\u8bbe\u5907\u6e90"));
    realtimePlaybackInput.setTooltip(W("\u8bad\u7ec3\u6587\u4ef6\u6570\u636e\uff1a\u4ec5\u652f\u6301\u9884\u5904\u7406 NPZ\uff08N,C,T\uff09\u6216\uff08C,T\uff09"));
    realtimeLiveConnLabel.setTooltip(W("\u5df2\u79fb\u9664\u5b9e\u65f6\u8bbe\u5907\u6570\u636e\u6e90"));
    realtimeLiveConnInput.setTooltip(W("\u5df2\u79fb\u9664\u5b9e\u65f6\u8bbe\u5907\u6570\u636e\u6e90"));
    realtimeLiveConnCopyBtn.setTooltip(W("\u5df2\u79fb\u9664\u5b9e\u65f6\u8bbe\u5907\u6570\u636e\u6e90"));
    realtimePlaybackBrowseBtn.setTooltip(W("\u9009\u62e9\u9884\u5904\u7406\u8f93\u51fa\u7684 NPZ \u8bad\u7ec3\u6570\u636e"));
    realtimeChannelsCombo.setTooltip(W("\u901a\u9053\u6570 C\uff0c\u9700\u4e0e ONNX \u6a21\u578b\u8f93\u5165\u901a\u9053\u4e00\u81f4\u624d\u80fd\u5b9e\u65f6\u63a8\u7406"));
    realtimeRateCombo.setTooltip(W("\u91c7\u6837\u7387 Hz\uff0c\u5408\u6210\u6e90\u6309\u8be5\u9891\u7387\u751f\u6210\uff0c\u5b9e\u65f6\u6ce8\u5165\u4e5f\u5e94\u4e0e\u6b64\u4e00\u81f4"));
    realtimeUvRangeCombo.setTooltip(W("\u6ce2\u5f62\u7eb5\u8f74\u6ee1\u5e45\uff08\u03bcV\uff09"));
    realtimeFilterCombo.setTooltip(W("\u4ec5\u6539\u53d8\u6ce2\u5f62\u663e\u793a\uff1b\u5f55\u5236 NPZ \u4e0e ONNX \u4ecd\u4f7f\u7528 BoardManager \u539f\u59cb\u7f13\u51b2"));
    realtimeMontageCombo.setTooltip(W("\u6309\u8f6f\u4ef6\u901a\u9053\u987a\u5e8f\u5957\u7528\u6807\u51c6\u547d\u540d"));
    realtimeWinLenCombo.setTooltip(W("\u5355\u7a97\u91c7\u6837\u70b9\u6570 T\uff0c\u9700\u4e0e\u8bad\u7ec3 / \u6a21\u578b\u4e00\u81f4"));
    realtimeNumWinCombo.setTooltip(W("\u8fde\u7eed\u6ed1\u7a97\u4e2a\u6570 N\uff0c\u603b\u91c7\u6837\u70b9 = N\u00d7T"));
    realtimeOnnxInferToggle.setTooltip(W("\u5f00\u542f\u540e\u6309\u6700\u8fd1 T \u70b9\u6ed1\u7a97\u63a8\u7406\uff08\u9700\u5148\u5728\u300c\u6a21\u578b\u9a8c\u8bc1\u300d\u52a0\u8f7d ONNX\uff09"));
    realtimeRecordBtn.setTooltip(
        W("\u4ece\u5f53\u524d\u7f13\u51b2\u91c7\u96c6 N\u00d7T \u70b9\uff0c\u5199\u5165 Documents/NerouRuntime/captures/\uff0c\u6210\u529f\u540e\u81ea\u52a8\u5199\u5165\u63a8\u7406 NPZ \u8def\u5f84"));

    // ── 推理测试 ─────────────────────────────────────────────────────────────
    inferOnnxCombo.setTooltip(W("\u626b\u63cf\u5de5\u7a0b onnx/models \u4e0b\u7684 .onnx \u6587\u4ef6\uff0c\u9009\u4e2d\u540e\u81ea\u52a8\u586b\u5165\u8def\u5f84\u5e76\u52a0\u8f7d"));
    inferModelInput.setTooltip (W("ONNX \u6a21\u578b\u6587\u4ef6\u5b8c\u6574\u8def\u5f84\uff08.onnx\uff09"));
    inferBrowseModel.setTooltip(W("\u6d4f\u89c8\u9009\u62e9 ONNX \u6a21\u578b\u6587\u4ef6"));
    inferLoadBtn.setTooltip    (W("\u52a0\u8f7d\u9009\u5b9a\u7684 ONNX \u6a21\u578b\u5230\u63a8\u7406\u5f15\u64ce"));
    inferUseLastExportBtn.setTooltip(
        W("\u586b\u5165\u4e0a\u6b21\u8bad\u7ec3 / \u52a0\u8f7d\u8bb0\u5f55\u4e2d\u7684 ONNX \u8def\u5f84\uff0c\u5e76\u7acb\u5373\u5c1d\u8bd5\u52a0\u8f7d"));
    inferDataInput.setTooltip  (W("\u6d4b\u8bd5\u7528 NPZ \u6216 NPY \u6570\u636e\u6587\u4ef6\u8def\u5f84\uff08.npz / .npy\uff09"));
    inferBrowseData.setTooltip (W("\u6d4f\u89c8\u9009\u62e9 NPZ / NPY \u6d4b\u8bd5\u6570\u636e\u6587\u4ef6"));
    inferRunBtn.setTooltip     (W("\u8bfb\u53d6 NPZ \u6216 NPY \u7684\u9996\u6761\u6837\u672c\uff08data: N\u00d7C\u00d7T \u6216 C\u00d7T\uff09\u8fdb\u884c\u63a8\u7406"));
    inferClearBtn.setTooltip   (W("\u6e05\u7a7a\u63a8\u7406\u7ed3\u679c\u4e0e\u65e5\u5fd7"));
}

//==============================================================================
void MainComponent::checkPythonEnvironment()
{
    updateLog(W("========================================"), trainLog);
    updateLog(W("[\u73af\u5883\u68c0\u67e5] \u6b63\u5728\u68c0\u6d4b Python \u8fd0\u884c\u73af\u5883..."), trainLog);
    setStatusMsg(W("\u6b63\u5728\u68c0\u6d4b Python \u73af\u5883..."));

    // 关键优化：不能在消息线程上同步等待 cmd.exe + python 冷启动（最长 3s）。
    // 将 quickRun 丢到后台线程池，拿到结果再回主线程更新 UI/日志。
    juce::Component::SafePointer<MainComponent> safeSelf(this);
    juce::Thread::launch([safeSelf]
    {
        juce::StringArray verArgs { "python", "--version" };
        const juce::String verOut = PythonBridge::quickRun(verArgs, 3000);

        juce::MessageManager::callAsync([safeSelf, verOut]
        {
            auto* self = safeSelf.getComponent();
            if (self == nullptr)
                return;

            if (verOut.isEmpty())
            {
                self->updateLog(W("[\u73af\u5883\u68c0\u67e5] \u2717 \u672a\u627e\u5230 Python\uff0c\u8bf7\u786e\u8ba4 Python \u5df2\u5b89\u88c5\u5e76\u5728 PATH \u4e2d"), self->trainLog);
                self->updateLog(W("[\u73af\u5883\u68c0\u67e5] \u4e0b\u8f7d\u5730\u5740\uff1ahttps://www.python.org/downloads/"), self->trainLog);
                self->setStatusMsg(W("Python \u672a\u5b89\u88c5\uff0c\u8bad\u7ec3\u548c\u9884\u5904\u7406\u529f\u80fd\u4e0d\u53ef\u7528"), true);
                NR_LOGW(W("\u73af\u5883"), W("Python \u672a\u627e\u5230\u6216\u542f\u52a8\u5931\u8d25"));
                return;
            }

            self->updateLog(W("[\u73af\u5883\u68c0\u67e5] \u2713 ") + verOut, self->trainLog);
            NR_LOGI(W("\u73af\u5883"), W("Python \u5c31\u7eea\uff1a") + verOut);

            if (!self->trainBridge.launchEnvCheck())
                self->updateLog(juce::String(L"[\u73af\u5883\u68c0\u67e5] \u2717 \u65e0\u6cd5\u542f\u52a8\u73af\u5883\u68c0\u6d4b\u8fdb\u7a0b\uff08\u8fdb\u7a0b\u5df2\u88ab\u5360\u7528\uff09"), self->trainLog);
        });
    });
}

//==============================================================================
void MainComponent::layoutSidebarScrollArea(juce::Rectangle<int> bounds)
{
    sidebarFieldsViewport.setBounds(bounds);

    const int w = juce::jmax(1, bounds.getWidth());
    int       y   = 0;

    auto placeLabel = [&](juce::Label& L) {
        L.setBounds(0, y, w, LABEL_H);
        y += LABEL_H + 3;
    };
    auto placeFieldRow = [&](juce::Component& fld, juce::TextButton* browseBtn) {
        juce::Rectangle<int> row(0, y, w, FIELD_H);
        y += FIELD_H;
        if (browseBtn != nullptr)
        {
            browseBtn->setBounds(row.removeFromRight(48));
            row.removeFromRight(4);
        }
        fld.setBounds(row);
        y += 10;
    };

    if (currentTab == Tab::Training)
    {
        constexpr int SEC_H   = 16; // section header height
constexpr int SEC_GAP = 8;  // gap before section header
constexpr int DATA_INFO_H = 16; // height of data count info label
auto placeSectionHdr = [&](juce::Label& L) {
            y += SEC_GAP;
            L.setBounds(0, y, w, SEC_H);
            y += SEC_H + 4;
        };

        // ─── IDENTITY ──────────────────────────────────────────────────────
placeSectionHdr(trainSec_Identity);
        placeLabel(trainTemplateSecLabel);
        trainTemplateCombo.setBounds(0, y, w, FIELD_H);
        y += FIELD_H + 10;

        placeLabel(nameSecLabel);
        placeFieldRow(nameInput, nullptr);

        // ─── DATA ──────────────────────────────────────────────────────────
placeSectionHdr(trainSec_Data);
        placeLabel(dataSecLabel);
        placeFieldRow(dataPathInput, &browseDirBtn);
        // Data count info line (updated by refreshTrainPreflight)
trainDataCountLabel.setBounds(0, y, w, DATA_INFO_H);
        y += DATA_INFO_H + 8;

        // ─── TRAIN PARAMS ──────────────────────────────────────────────────
placeSectionHdr(trainSec_Params);
        placeLabel(epochSecLabel);
        placeFieldRow(epochInput, nullptr);

        placeLabel(classCountSecLabel);
        placeFieldRow(classCountInput, nullptr);

        placeLabel(batchSecLabel);
        batchCombo.setBounds(0, y, w, FIELD_H);
        y += FIELD_H + 10;

        placeLabel(lrSecLabel);
        lrCombo.setBounds(0, y, w, FIELD_H);
        y += FIELD_H + 10;

        // ─── OUTPUT ────────────────────────────────────────────────────────
placeSectionHdr(trainSec_Output);
        placeLabel(saveSecLabel);
        placeFieldRow(savePathInput, &browseSaveBtn);
    }
    else if (currentTab == Tab::DataPrep)
    {
        placeLabel(prepRawLabel);
        placeFieldRow(prepRawInput, &prepBrowseRaw);

        placeLabel(prepOutLabel);
        placeFieldRow(prepOutInput, &prepBrowseOut);

        placeLabel(prepSrLabel);
        prepSrCombo.setBounds(0, y, w, FIELD_H);
        y += FIELD_H + 10;

        placeLabel(prepLowLabel);
        placeFieldRow(prepLowInput, nullptr);

        placeLabel(prepHighLabel);
        placeFieldRow(prepHighInput, nullptr);

        placeLabel(prepChLabel);
        prepChCombo.setBounds(0, y, w, FIELD_H);
        y += FIELD_H + 10;

        prepSyncTrainBtn.setBounds(0, y, w, 34);
        y += 34 + 8;
    }

    const int contentH = y;
    const int viewH    = juce::jmax(1, bounds.getHeight());
    sidebarFieldsHost.setSize(w, juce::jmax(contentH, viewH));
}

juce::File MainComponent::resolveSelectedManifestFile() const
{
    const int tid = trainTemplateCombo.getSelectedId();
    if (tid <= 1)
        return {};
    const int idx = tid - 2;
    if (idx < 0 || idx >= trainTemplateModelIds.size())
        return {};
    return ProjectPaths::getOnnxModelsDirectory()
        .getChildFile(trainTemplateModelIds[idx])
        .getChildFile("manifest.json");
}

void MainComponent::refreshTrainPreflight()
{
    const bool             wasPassed = trainPreflightPassed;
    juce::File             dir(dataPathInput.getText().trim());
    lastTrainPreflight               = TrainPreflight::evaluate(dir, resolveSelectedManifestFile());
    trainPreflightPassed             = lastTrainPreflight.canStart;

    TrainPreflightReport::Code c = lastTrainPreflight.code;
    juce::String               msg;
    if (c == TrainPreflightReport::Code::BadDir)
        msg = W("\u6570\u636e\u96c6\u8def\u5f84\u65e0\u6548\u6216\u4e0d\u662f\u76ee\u5f55\u3002");
    else if (c == TrainPreflightReport::Code::NoNpz)
        msg = W("\u76ee\u5f55\u5185\u6ca1\u6709 .npz \u6587\u4ef6\u3002");
    else if (c == TrainPreflightReport::Code::NpzReadErr)
        msg = W("\u65e0\u6cd5\u8bfb\u53d6 NPZ\uff1a") + lastTrainPreflight.npzError;
    else if (c == TrainPreflightReport::Code::ManifestMismatch)
        msg = W("\u4e0e\u6a21\u677f manifest \u4e0d\u4e00\u81f4\uff1a\u6570\u636e C=")
              + juce::String(lastTrainPreflight.shapeC) + W(" T=")
              + juce::String(lastTrainPreflight.shapeT)
              + W(" \uff1b\u671f\u671b C=") + juce::String(lastTrainPreflight.manifestC) + W(" T=")
              + juce::String(lastTrainPreflight.manifestT) + W("\u3002");
    else
    {
        msg = W("NPZ \u5171 C\u00d7T ") + juce::String(lastTrainPreflight.npzCount)
              + W(" \u4e2a")
              + (lastTrainPreflight.npzTotalInDir > lastTrainPreflight.npzCount
                     ? W("\uff08\u76ee\u5f55\u5171 ") + juce::String(lastTrainPreflight.npzTotalInDir)
                           + W(" \u4e2a\uff0c\u5df2\u6309\u6587\u4ef6\u6570\u81ea\u52a8\u9009\u4e3b\u6a21\u6001\uff09")
                     : juce::String())
              + W("\uff1b\u7a97\u53e3 C=") + juce::String(lastTrainPreflight.shapeC) + W(" T=")
              + juce::String(lastTrainPreflight.shapeT);
        if (lastTrainPreflight.hasManifest && lastTrainPreflight.canStart)
            msg += W("\uff1b\u5df2\u4e0e\u6a21\u677f\u5bf9\u9f50\u3002");
        else if (!lastTrainPreflight.hasManifest)
            msg += W("\uff08\u81ea\u5b9a\u4e49/\u65e0 manifest\uff0c\u672a\u505a\u6a21\u677f\u6821\u9a8c\uff09");
        else
            msg += W("\u3002");
    }

    trainPreflightDetail.setText(msg, juce::dontSendNotification);
    trainPreflightDetail.setColour(juce::Label::textColourId,
                                   trainPreflightPassed ? C_SUCCESS : C_DANGER);

    trainPreflightSidebarBadge.setText(
        trainPreflightPassed ? W("\u9884\u68c0\uff1a\u901a\u8fc7") : W("\u9884\u68c0\uff1a\u672a\u901a\u8fc7"),
        juce::dontSendNotification);
    trainPreflightSidebarBadge.setColour(juce::Label::textColourId,
                                         trainPreflightPassed ? C_SUCCESS : C_DANGER);
    trainPreflightSidebarBadge.setTooltip(msg);

    if (currentTab == Tab::Training)
    {
        if (!trainPreflightPassed)
        {
            if (c != trainPreflightLoggedCode || msg != trainPreflightLoggedSummary)
            {
                trainPreflightLoggedCode     = c;
                trainPreflightLoggedSummary  = msg;
                updateLog(W("[\u9884\u68c0] ") + msg, trainLog);
                trainPreflightFailTitleFlashTicks = kPreflightFailTitleFlashTicks;
                trainPreflightOkTitleFlashTicks   = 0;
                trainPreflightTitle.setColour(juce::Label::textColourId, C_WARNING);
            }
        }
        else if (!wasPassed && trainPreflightLoggedSummary.isNotEmpty())
        {
            trainPreflightLoggedCode    = TrainPreflightReport::Code::Ok;
            trainPreflightLoggedSummary = {};
            updateLog(W("[\u9884\u68c0] \u5df2\u901a\u8fc7\uff0c\u53ef\u4ee5\u5f00\u59cb\u8bad\u7ec3\u3002"), trainLog);
            trainPreflightOkTitleFlashTicks   = kPreflightOkTitleFlashTicks;
            trainPreflightFailTitleFlashTicks = 0;
            trainPreflightTitle.setColour(juce::Label::textColourId, C_SUCCESS);
        }
    }

    if (currentTab == Tab::Training)
        syncTrainPreflightTitleVisualState();

    if (currentTab == Tab::Training)
        updateStatusBarForCurrentTab();

    // ── 更新数据集简要标注（在侧栏数据路径字段正下方）────────────────────────
    {
        TrainPreflightReport::Code c2 = lastTrainPreflight.code;
        juce::String infoTxt;
        juce::Colour infoClr = C_TEXT_S;
        if (c2 == TrainPreflightReport::Code::BadDir || dataPathInput.getText().trim().isEmpty())
        {
            infoTxt = W("\u8def\u5f84\u65e0\u6548");
            infoClr = C_DANGER.withAlpha(0.7f);
        }
        else if (c2 == TrainPreflightReport::Code::NoNpz)
        {
            infoTxt = W("\u672a\u627e\u5230 NPZ \u6587\u4ef6");
            infoClr = C_DANGER.withAlpha(0.7f);
        }
        else if (c2 == TrainPreflightReport::Code::NpzReadErr)
        {
            infoTxt = W("NPZ \u8bfb\u53d6\u9519\u8bef");
            infoClr = C_WARNING;
        }
        else
        {
            infoTxt = juce::String(lastTrainPreflight.npzCount) + W(" \u4e2a NPZ")
                    + W(" \u00b7 C=") + juce::String(lastTrainPreflight.shapeC)
                    + W(" T=") + juce::String(lastTrainPreflight.shapeT);
            infoClr = C_SUCCESS;
        }
        trainDataCountLabel.setText(infoTxt, juce::dontSendNotification);
        trainDataCountLabel.setColour(juce::Label::textColourId, infoClr);
    }

    if (!isTrainingActive)
        setTrainUiLocked(false);

    if (currentTab == Tab::Training)
        updateTopBarChrome();
}

void MainComponent::syncTrainPreflightTitleVisualState()
{
    if (trainPreflightFailTitleFlashTicks > 0 || trainPreflightOkTitleFlashTicks > 0)
        return;
    trainPreflightTitle.setColour(juce::Label::textColourId,
                                  trainPreflightPassed ? C_TEXT_S : C_DANGER);
}

juce::String MainComponent::composeTrainPreflightSummaryText() const
{
    juce::String clip;
    clip << W("NerouRuntime \u8bad\u7ec3\u524d\u68c0\u67e5\n");
    clip << W("\u901a\u8fc7: ") << (trainPreflightPassed ? W("\u662f\n") : W("\u5426\n"));
    clip << W("\u8be6\u60c5: ") << trainPreflightDetail.getText() << "\n";
    clip << W("\u6570\u636e\u96c6: ") << dataPathInput.getText().trim() << "\n";
    clip << W("\u6a21\u578b\u540d\u79f0: ") << nameInput.getText().trim() << "\n";
    juce::File mf = resolveSelectedManifestFile();
    if (mf.existsAsFile())
        clip << W("manifest: ") << mf.getFullPathName() << "\n";
    else
        clip << W("manifest: (\u672a\u9009\u6a21\u677f\u6216\u65e0\u6587\u4ef6)\n");
    clip << W("NPZ(\u540c\u5f62\u72b6): ") << lastTrainPreflight.npzCount
         << W("  \u76ee\u5f55\u603b: ") << lastTrainPreflight.npzTotalInDir << W("  C=")
         << lastTrainPreflight.shapeC << W(" T=") << lastTrainPreflight.shapeT;
    if (lastTrainPreflight.hasManifest)
        clip << W("  manifest C=") << lastTrainPreflight.manifestC << W(" T=")
             << lastTrainPreflight.manifestT;
    clip << "\n";
    clip << "\n";
    return clip;
}

void MainComponent::updateStatusBarForCurrentTab()
{
    workflowOrchestrator.setActiveTab(currentTab);

    nerou::app::WorkflowStatusContext statusContext;
    statusContext.trainPreflightPassed = trainPreflightPassed;
    statusContext.trainPreflightDetail = trainPreflightDetail.getText();

    const auto statusLine = workflowOrchestrator.composeStatusLine(statusContext);
    setStatusMsg(statusLine.text, statusLine.isError);
}

void MainComponent::updateTabGuardTooltips()
{
    // 为每个 Tab 按钮更新 tooltip 和徽章文字
    struct TabBtnPair { nerou::app::WorkflowTab tab; juce::TextButton* btn; int stepIndex; };
    TabBtnPair pairs[] = {
        { Tab::Overview,    &tabOverviewBtn,    4 },
        { Tab::Acquisition, &tabAcquisitionBtn, 0 },
        { Tab::DataPrep,    &tabDataPrepBtn,    1 },
        { Tab::Training,    &tabTrainBtn,       2 },
        { Tab::Inference,   &tabInferBtn,       3 }
    };

    for (auto& p : pairs)
    {
        auto guard = workflowOrchestrator.checkCanNavigate(p.tab);
        auto badge = workflowOrchestrator.getTabBadge(p.tab);

        juce::String tip = sidebarTabBaseTooltip(p.tab);
        if (!guard.canEnter)
            tip += W("\n\n") + guard.blockerReason + W(" \u2014 ") + guard.actionHint;
        else if (badge.isNotEmpty())
            tip += W("\n") + W("\u8fdb\u5ea6\u89d2\u6807\uff1a \u2713 \u5df2\u5c31\u7eea / ! \u5f85\u5b8c\u6210 / \u25cf \u91c7\u96c6\u4e2d")
                 + W("  \u3010") + badge + W("\u3011");

        p.btn->setTooltip(tip);

        // 在按钮属性中存储徽章文字，供自定义绘制使用
        p.btn->getProperties().set("badge", badge);
        p.btn->getProperties().set("guard_blocked", !guard.canEnter);

        if (p.stepIndex >= 0)
        {
            workflowStepper.setStepBlocked(p.stepIndex, !guard.canEnter);
            workflowStepper.setStepTooltip(
                p.stepIndex,
                !guard.canEnter
                    ? (guard.blockerReason + W("\n") + guard.actionHint)
                    : juce::String());
        }
    }
}

void MainComponent::updateTopBarChrome()
{
    juce::String title;
    juce::String subtitle;
    juce::String pillPrimary;
    juce::String pillSecondary;
    juce::String tabSecondaryInfo;
    const auto primaryAction = describeTopBarPrimaryAction();
    const auto secondaryAction = describeTopBarSecondaryAction();

    switch (currentTab)
    {
        case Tab::Overview:
        {
            title = W("\u5de5\u4f5c\u53f0\u603b\u89c8");
            subtitle = W("\u6309\u6570\u636e \u2192 \u9884\u5904\u7406 \u2192 \u8bad\u7ec3 \u2192 \u9a8c\u8bc1\u5bfc\u51fa\u7684\u4e3b\u6d41\u7a0b\u5b8c\u6210 ONNX Runtime DATA");
            pillPrimary = W("\u4e0b\u4e00\u6b65");
            tabSecondaryInfo = {};
            break;
        }
        case Tab::Acquisition:
        {
            title = W("\u6570\u636e");
            subtitle = BoardManager::getInstance().isStreaming()
                           ? composeRealtimeIdleStatus()
                           : W("\u5bfc\u5165\u6216\u56de\u653e EEG/\u795e\u7ecf\u4fe1\u53f7\u6570\u636e\uff0c\u5b8c\u6210\u8d28\u91cf\u68c0\u67e5\u540e\u8fdb\u5165\u9884\u5904\u7406");
            pillPrimary = W("\u6570\u636e\u6e90");
            tabSecondaryInfo = onnxRunner.isModelLoaded()
                                   ? W("ONNX \u5df2\u5c31\u7eea")
                                   : W("ONNX \u672a\u52a0\u8f7d");
            break;
        }
        case Tab::Training:
        {
            title = W("\u6a21\u578b\u8bad\u7ec3");
            subtitle = isTrainingActive
                           ? W("\u6587\u4ef6\u6e90\u8bad\u7ec3\u4efb\u52a1\u8fd0\u884c\u4e2d")
                           : (trainPreflightPassed
                                  ? W("\u8bad\u7ec3\u6587\u4ef6\u68c0\u67e5\u5df2\u901a\u8fc7\uff0c\u53ef\u76f4\u63a5\u5f00\u59cb\u8bad\u7ec3")
                                  : W("\u9009\u62e9 NPZ \u8bad\u7ec3\u6587\u4ef6\u6216\u6807\u51c6\u8bad\u7ec3\u76ee\u5f55"));
            pillPrimary = W("\u6a21\u578b\uff1a") + nameInput.getText().trim();
            tabSecondaryInfo = trainDataCountLabel.getText().isNotEmpty()
                                   ? trainDataCountLabel.getText()
                                   : W("\u6570\u636e\u96c6\uff1a\u5f85\u68c0\u67e5");
            break;
        }
        case Tab::DataPrep:
        {
            title = W("\u6570\u636e\u51c6\u5907");
            subtitle = isPrepRunning
                           ? W("\u6b63\u5728\u751f\u6210\u6807\u51c6 NPZ \u8bad\u7ec3\u6570\u636e")
                           : W("\u5c06\u539f\u59cb\u6570\u636e\u5904\u7406\u4e3a\u8bad\u7ec3\u4e0e\u63a8\u7406\u8f93\u5165");
            pillPrimary = W("\u91c7\u6837\u7387\uff1a") + prepSrCombo.getText();
            tabSecondaryInfo = W("\u901a\u9053\uff1a") + prepChCombo.getText();
            break;
        }
        case Tab::Inference:
        {
            title = W("\u63a8\u7406\u6a21\u578b\u751f\u6210");
            subtitle = onnxRunner.isModelLoaded()
                           ? W("ONNX \u5df2\u5c31\u7eea\uff0c\u53ef\u751f\u6210 ONNX Runtime DATA \u751f\u4ea7\u63a8\u7406\u5305")
                           : W("\u5148\u5b8c\u6210\u8bad\u7ec3\u5e76\u52a0\u8f7d ONNX \u6a21\u578b");
            pillPrimary = onnxRunner.isModelLoaded()
                              ? W("C=") + juce::String(onnxRunner.getInputChannels()) + W("  T=")
                                    + juce::String(onnxRunner.getInputTimePoints())
                              : W("\u6a21\u578b\uff1a\u672a\u52a0\u8f7d");
            tabSecondaryInfo = inferDataInput.getText().trim().isNotEmpty()
                                   ? juce::File(inferDataInput.getText().trim()).getFileName()
                                   : W("\u6d4b\u8bd5\u6570\u636e\uff1a\u672a\u9009\u62e9");
            break;
        }
    }

    pillSecondary = composeBusinessContextPill();
    if (tabSecondaryInfo.isNotEmpty())
        subtitle += W(" | ") + tabSecondaryInfo;

    topBarTitle.setText(title, juce::dontSendNotification);
    topBarSubtitle.setText(subtitle, juce::dontSendNotification);
    topBarPillPrimary.setText(pillPrimary, juce::dontSendNotification);
    topBarPillSecondary.setText(pillSecondary, juce::dontSendNotification);
    statusBarRight.setText(composeStatusBarContextText(), juce::dontSendNotification);
    topProjectContextBtn.setButtonText(W("\u5207\u6362\u9879\u76ee"));
    topSubjectContextBtn.setButtonText(W("\u5207\u6362\u53d7\u8bd5\u8005"));
    topPrimaryActionBtn.setButtonText(primaryAction.label);
    topSecondaryActionBtn.setButtonText(secondaryAction.label);
    topPrimaryActionBtn.setEnabled(primaryAction.enabled);
    topSecondaryActionBtn.setEnabled(secondaryAction.enabled);
    topPrimaryActionBtn.getProperties().set("guard_blocked", primaryAction.guardBlocked);
    topSecondaryActionBtn.getProperties().set("guard_blocked", secondaryAction.guardBlocked);
    refreshWorkspaceCommands();
    refreshTopBarActionTooltips();
    refreshTopBarPillTooltips();
}

void MainComponent::refreshTopBarActionTooltips()
{
    topPrimaryActionBtn.setTooltip(describeTopBarPrimaryAction().tooltip);
    topSecondaryActionBtn.setTooltip(describeTopBarSecondaryAction().tooltip);
}

void MainComponent::refreshTopBarPillTooltips()
{
    juce::String primTip, secTip;

    secTip = W("\u5f53\u524d\u9009\u4e2d\u7684\u9879\u76ee\u4e0e\u53d7\u8bd5\u8005\u3002\u70b9\u51fb\u9876\u680f\u300c\u5207\u6362\u9879\u76ee\u300d\u6216\u300c\u5207\u6362\u53d7\u8bd5\u8005\u300d\u53ef\u4fee\u6539\u3002");

    switch (currentTab)
    {
        case Tab::Overview:
            primTip = W("\u5de5\u4f5c\u53f0\u6a21\u5f0f\u4e0b\u7684\u4e3b\u4e0a\u4e0b\u6587\u6807\u7b7e\uff08\u4e0e\u6d41\u7a0b\u603b\u89c8\u5185\u5bb9\u5bf9\u5e94\uff09\u3002");
            break;
        case Tab::Acquisition:
            primTip = W("\u5f53\u524d\u6570\u636e\u6e90\u6458\u8981\uff08\u5b9e\u65f6\u8bbe\u5907 / \u56de\u653e / \u5408\u6210\uff09\uff0c\u4e0e\u91c7\u96c6\u9875\u4e0b\u62c9\u6846\u540c\u6b65\u3002");
            secTip += W(" ")
                + W("\u526f\u6807\u9898\u533a\u57df\u8fd8\u4f1a\u663e\u793a ONNX \u52a0\u8f7d\u72b6\u6001\u3002");
            break;
        case Tab::Training:
            primTip = W("\u5f53\u524d\u8bad\u7ec3\u6a21\u578b\u540d\u79f0\uff08\u4e0e\u8bad\u7ec3\u9875\u540d\u79f0\u8f93\u5165\u6846\u540c\u6b65\uff09\u3002");
            secTip += W(" ")
                + W("\u526f\u6807\u9898\u533a\u57df\u8fd8\u4f1a\u663e\u793a\u6570\u636e\u96c6/\u6837\u672c\u6458\u8981\u3002");
            break;
        case Tab::DataPrep:
            primTip = W("\u5f53\u524d\u9884\u5904\u7406\u91c7\u6837\u7387\uff0c\u4e0e\u6570\u636e\u51c6\u5907\u9875\u4e0b\u62c9\u6846\u540c\u6b65\u3002");
            secTip += W(" ") + W("\u526f\u6807\u9898\u533a\u57df\u8fd8\u4f1a\u663e\u793a\u901a\u9053\u6570\u6458\u8981\u3002");
            break;
        case Tab::Inference:
        default:
            primTip = onnxRunner.isModelLoaded()
                          ? W("\u5df2\u52a0\u8f7d\u6a21\u578b\u7684\u8f93\u5165\u7ef4\u5ea6\uff1aC \u4e3a\u901a\u9053\u6570\uff0cT \u4e3a\u65f6\u95f4\u70b9\u6570\u3002")
                          : W("\u5c1a\u672a\u52a0\u8f7d ONNX\uff0c\u8bf7\u5148\u52a0\u8f7d\u6a21\u578b\u540e\u518d\u9a8c\u8bc1\u3002");
            secTip += W(" ")
                + W("\u526f\u6807\u9898\u533a\u57df\u8fd8\u4f1a\u663e\u793a\u5f53\u524d\u6d4b\u8bd5\u6570\u636e\u6587\u4ef6\u540d\u3002");
            break;
    }

    topBarPillPrimary.setTooltip(primTip);
    topBarPillSecondary.setTooltip(secTip);
}

MainComponent::TopBarActionDescriptor MainComponent::describeTopBarPrimaryAction() const
{
    TopBarActionDescriptor descriptor;
    nerou::domain::NodeType nodeType {};

    if (getTopBarPrimaryNodeActionTarget(nodeType))
    {
        const auto action = workflowOrchestrator.describeNodePrimaryAction(
            nodeType,
            W("\u6267\u884c\u5de5\u4f5c\u53f0\u4e3b\u52a8\u4f5c\uff1a")
                + nerou::app::WorkflowOrchestrator::nodePrimaryActionLabel(nodeType),
            true);
        descriptor.kind = TopBarActionDescriptor::Kind::NodeAction;
        descriptor.label = action.label;
        descriptor.tooltip = action.tooltip;
        descriptor.enabled = action.enabled;
        descriptor.guardBlocked = action.guardBlocked;
        descriptor.nodeType = nodeType;
        return descriptor;
    }

    switch (currentTab)
    {
        case Tab::Acquisition:
            descriptor.kind = TopBarActionDescriptor::Kind::ToggleAcquisitionStream;
            descriptor.label = BoardManager::getInstance().isStreaming()
                ? W("\u505c\u6b62\u91c7\u96c6")
                : W("\u542f\u52a8\u8bbe\u5907");
            descriptor.tooltip = BoardManager::getInstance().isStreaming()
                ? W("\u505c\u6b62\u5f53\u524d\u6570\u636e\u6d41\uff08\u4e0e\u9875\u5185\u300c\u505c\u6b62\u8bbe\u5907\u300d\u76f8\u540c\uff09")
                : W("\u6309\u5f53\u524d\u6570\u636e\u6e90\u8bbe\u7f6e\u542f\u52a8\u91c7\u96c6");
            return descriptor;

        case Tab::Training:
            descriptor.kind = TopBarActionDescriptor::Kind::ToggleTrainingPause;
            descriptor.label = isPaused ? W("\u7ee7\u7eed\u8bad\u7ec3")
                                        : W("\u6682\u505c\u8bad\u7ec3");
            descriptor.tooltip = isPaused
                ? W("\u7ee7\u7eed\u8bad\u7ec3\uff08\u4e0e\u6682\u505c\u6309\u94ae\u76f8\u540c\uff09")
                : W("\u6682\u505c\u8bad\u7ec3\uff08\u4e0e\u6682\u505c\u6309\u94ae\u76f8\u540c\uff09");
            descriptor.enabled = isTrainingActive;
            return descriptor;

        case Tab::DataPrep:
            descriptor.label = W("\u9884\u5904\u7406\u4e2d...");
            descriptor.tooltip = W("\u9884\u5904\u7406\u4efb\u52a1\u8fdb\u884c\u4e2d\uff0c\u8bf7\u7a0d\u5019\u2026");
            descriptor.enabled = false;
            return descriptor;

        case Tab::Inference:
        default:
            if (onnxRunner.isModelLoaded() && inferDataInput.getText().trim().isNotEmpty())
            {
                descriptor.kind = TopBarActionDescriptor::Kind::RunInference;
                descriptor.label = W("\u8fd0\u884c\u9a8c\u8bc1");
                descriptor.tooltip = W("\u7528\u5f53\u524d\u6d4b\u8bd5\u6570\u636e\u8fd0\u884c\u9a8c\u8bc1");
            }
            else
            {
                descriptor.kind = TopBarActionDescriptor::Kind::LoadLatestOnnx;
                descriptor.label = W("\u52a0\u8f7d\u6700\u8fd1\u6a21\u578b");
                descriptor.tooltip = W("\u52a0\u8f7d\u6700\u8fd1\u5bfc\u51fa\u7684 ONNX\uff08\u82e5\u5b58\u5728\uff09");
            }
            return descriptor;
    }
}

MainComponent::TopBarActionDescriptor MainComponent::describeTopBarSecondaryAction() const
{
    TopBarActionDescriptor descriptor;
    nerou::domain::NodeType nodeType {};

    if (getTopBarSecondaryNodeActionTarget(nodeType))
    {
        const auto action = workflowOrchestrator.describeNodePrimaryAction(
            nodeType,
            W("\u6267\u884c\u8282\u70b9\u5feb\u901f\u8df3\u8f6c\uff1a")
                + nerou::app::WorkflowOrchestrator::nodePrimaryActionLabel(nodeType),
            true);
        descriptor.kind = TopBarActionDescriptor::Kind::NodeAction;
        descriptor.label = (currentTab == Tab::Acquisition && nodeType == nerou::domain::NodeType::Validation)
            ? W("\u8f6c\u5230\u6a21\u578b\u9a8c\u8bc1")
            : action.label;
        descriptor.tooltip = action.tooltip;
        descriptor.enabled = action.enabled;
        descriptor.guardBlocked = action.guardBlocked;
        descriptor.nodeType = nodeType;
        return descriptor;
    }

    switch (currentTab)
    {
        case Tab::Overview:
            descriptor.kind = TopBarActionDescriptor::Kind::ToggleCommandPalette;
            descriptor.label = W("\u5feb\u6377\u952e Ctrl+K");
            descriptor.tooltip = W("\u6253\u5f00\u547d\u4ee4\u9762\u677f\uff08Ctrl+K\uff09\uff0c\u5feb\u901f\u641c\u7d22\u64cd\u4f5c");
            return descriptor;

        case Tab::Training:
            descriptor.kind = TopBarActionDescriptor::Kind::GenerateRuntimeData;
            descriptor.label = W("\u751f\u6210 Runtime DATA");
            descriptor.tooltip = W("\u5c06\u6700\u8fd1\u5bfc\u51fa\u7684 ONNX \u4e0e\u8bad\u7ec3\u6570\u636e\u6253\u5305\u4e3a\u53ef\u90e8\u7f72\u7684 ONNX Runtime DATA");
            descriptor.enabled = !runtimeDataExportService.isRunning();
            return descriptor;

        case Tab::DataPrep:
            descriptor.kind = TopBarActionDescriptor::Kind::SyncPrepToTraining;
            descriptor.label = W("\u540c\u6b65\u5230\u8bad\u7ec3");
            descriptor.tooltip = W("\u5c06\u8f93\u51fa\u76ee\u5f55\u540c\u6b65\u5230\u8bad\u7ec3\u6570\u636e\u8def\u5f84\u5e76\u5207\u6362\u5230\u8bad\u7ec3\u9875");
            return descriptor;

        case Tab::Inference:
        default:
            descriptor.kind = TopBarActionDescriptor::Kind::LoadGoldenSample;
            descriptor.label = W("Golden Sample");
            descriptor.tooltip = W("\u52a0\u8f7d Golden Sample \u5e76\u53ef\u9009\u81ea\u52a8\u8fd0\u884c");
            return descriptor;
    }
}

void MainComponent::executeTopBarAction(const TopBarActionDescriptor& action)
{
    if (!action.enabled)
        return;

    switch (action.kind)
    {
        case TopBarActionDescriptor::Kind::NodeAction:
            triggerWorkspaceNodePrimaryAction(action.nodeType);
            break;

        case TopBarActionDescriptor::Kind::ToggleCommandPalette:
            if (!commandPalette_.isVisiblePalette())
            {
                commandPalette_.setVisiblePalette(true);
                commandPalette_.toFront(true);
            }
            else
            {
                commandPalette_.setVisiblePalette(false);
            }
            break;

        case TopBarActionDescriptor::Kind::ToggleAcquisitionStream:
            if (BoardManager::getInstance().isStreaming())
                stopBoardBtn.triggerClick();
            else
                startBoardBtn.triggerClick();
            break;

        case TopBarActionDescriptor::Kind::ToggleTrainingPause:
            pauseBtn.triggerClick();
            break;

        case TopBarActionDescriptor::Kind::SyncPrepToTraining:
            prepSyncTrainBtn.triggerClick();
            break;

        case TopBarActionDescriptor::Kind::LoadLatestOnnx:
            loadLatestOnnxIntoInference(true);
            break;

        case TopBarActionDescriptor::Kind::RunInference:
            inferRunBtn.triggerClick();
            break;

        case TopBarActionDescriptor::Kind::OpenTrainingExportDir:
        {
            const juce::File exportDir(savePathInput.getText().trim());
            if (exportDir.isDirectory())
                exportDir.startAsProcess();
            else
                loadLatestOnnxIntoInference(true);
            break;
        }

        case TopBarActionDescriptor::Kind::LoadGoldenSample:
            loadGoldenSampleForCurrentModel(true);
            break;

        case TopBarActionDescriptor::Kind::GenerateRuntimeData:
            exportRuntimeDataPackage();
            break;

        case TopBarActionDescriptor::Kind::None:
        default:
            break;
    }
}

std::vector<MainComponent::WorkspaceCommandRegistration> MainComponent::buildWorkspaceCommandRegistry() const
{
    using CommandItem = nerou::ui::CommandPalette::CommandItem;
    using Registration = WorkspaceCommandRegistration;
    using Action = WorkspaceCommandAction;
    std::vector<Registration> commands;

    const auto addCommand = [&commands](juce::String id,
                                        juce::String label,
                                        juce::String tooltip,
                                        Action action,
                                        bool enabled = true,
                                        juce::StringArray aliases = {})
    {
        const auto it = std::find_if(commands.begin(), commands.end(),
                                     [&id](const Registration& item) { return item.item.id == id; });
        if (it != commands.end())
            return;

        commands.push_back({
            { std::move(id), std::move(label), std::move(aliases), std::move(tooltip), enabled },
            action
        });
    };

    addCommand("project.create",
               W("\u65b0\u5efa\u9879\u76ee"),
               W("\u521b\u5efa\u65b0\u9879\u76ee\u5e76\u521d\u59cb\u5316\u5de5\u4f5c\u6d41"),
               Action::ProjectCreate,
               true,
               { W("\u65b0\u9879\u76ee"), W("\u521b\u5efa\u9879\u76ee") });
    addCommand("nav.training", W("\u5207\u6362\u8bad\u7ec3"), W("\u8fdb\u5165\u8bad\u7ec3\u4e2d\u5fc3"), Action::NavTraining, true, { W("\u8bad\u7ec3"), W("training") });
    addCommand("nav.validation", W("\u5207\u6362\u9a8c\u8bc1\u5bfc\u51fa"), W("\u8fdb\u5165 ONNX Runtime \u9a8c\u8bc1\u4e0e DATA \u5bfc\u51fa\u9875"), Action::NavValidation, true, { W("\u9a8c\u8bc1"), W("\u5bfc\u51fa"), W("validation") });
    addCommand("system.settings", W("\u6253\u5f00\u8bbe\u7f6e"), W("\u6253\u5f00\u7cfb\u7edf\u8bbe\u7f6e\u4e0e\u6027\u80fd\u504f\u597d"), Action::SystemSettings, true, { W("\u8bbe\u7f6e") });
    addCommand("system.about", W("\u5173\u4e8e\u795e\u7ecf\u8fd0\u884c\u65f6"), W("\u67e5\u770b\u4ea7\u54c1\u7248\u672c\u4e0e\u8bf4\u660e"), Action::SystemAbout, true, { W("\u5173\u4e8e") });
    addCommand("system.shortcuts", W("\u663e\u793a\u5feb\u6377\u952e"), W("\u67e5\u770b\u5168\u5c40\u5feb\u6377\u952e\u901f\u67e5"), Action::SystemShortcuts, true, { W("\u5feb\u6377\u952e"), W("\u5e2e\u52a9") });
    addCommand("model.export", W("\u6253\u5f00\u9a8c\u8bc1\u5bfc\u51fa"), W("\u8fdb\u5165 ONNX Runtime \u9a8c\u8bc1\u4e0e DATA \u5bfc\u51fa\u9875"), Action::ModelExport, true, { W("\u5bfc\u51fa"), W("runtime data") });
    addCommand("training.stop", W("\u505c\u6b62\u8bad\u7ec3"),
               W("\u7ec8\u6b62\u5f53\u524d\u8bad\u7ec3\u4efb\u52a1"),
               Action::TrainingStop,
               isTrainingActive,
               { W("\u7ec8\u6b62\u8bad\u7ec3"), W("\u505c\u6b62") });

    const auto primaryAction = describeTopBarPrimaryAction();
    const auto secondaryAction = describeTopBarSecondaryAction();

    if (primaryAction.label.isNotEmpty())
        addCommand("top.primary", primaryAction.label, primaryAction.tooltip, Action::TopPrimary, primaryAction.enabled);
    if (secondaryAction.label.isNotEmpty())
        addCommand("top.secondary", secondaryAction.label, secondaryAction.tooltip, Action::TopSecondary, secondaryAction.enabled);

    return commands;
}

std::vector<nerou::ui::CommandPalette::CommandItem> MainComponent::buildWorkspaceCommands() const
{
    std::vector<nerou::ui::CommandPalette::CommandItem> commands;
    for (const auto& registration : buildWorkspaceCommandRegistry())
        commands.push_back(registration.item);
    return commands;
}

std::optional<MainComponent::WorkspaceCommandRegistration> MainComponent::resolveWorkspaceCommandRegistration(const juce::String& query) const
{
    const auto trimmed = query.trim();
    if (trimmed.isEmpty())
        return std::nullopt;

    const auto lowered = trimmed.toLowerCase();
    const auto commands = buildWorkspaceCommandRegistry();

    for (const auto& item : commands)
    {
        if (item.item.id.toLowerCase() == lowered || item.item.label.toLowerCase() == lowered)
            return item;

        for (const auto& alias : item.item.aliases)
        {
            if (alias.toLowerCase() == lowered)
                return item;
        }
    }

    return std::nullopt;
}

void MainComponent::refreshWorkspaceCommands()
{
    commandPalette_.setCommands(buildWorkspaceCommands());
}

bool MainComponent::executeWorkspaceCommandAction(WorkspaceCommandAction action)
{
    switch (action)
    {
        case WorkspaceCommandAction::TopPrimary:
            executeTopBarAction(describeTopBarPrimaryAction());
            return true;
        case WorkspaceCommandAction::TopSecondary:
            executeTopBarAction(describeTopBarSecondaryAction());
            return true;
        case WorkspaceCommandAction::ProjectCreate:
            showCreateProjectDialog();
            return true;
        case WorkspaceCommandAction::NavOverview:
            switchTab(Tab::Overview);
            return true;
        case WorkspaceCommandAction::NavAcquisition:
            switchTab(Tab::Acquisition);
            return true;
        case WorkspaceCommandAction::NavPreparation:
            switchTab(Tab::DataPrep);
            return true;
        case WorkspaceCommandAction::NavTraining:
            switchTab(Tab::Training);
            return true;
        case WorkspaceCommandAction::NavValidation:
            switchTab(Tab::Inference);
            return true;
        case WorkspaceCommandAction::NavArchive:
            switchTab(Tab::Training);
            return true;
        case WorkspaceCommandAction::SystemSettings:
            if (!settingsDialog.isShowing())
            {
                const auto perf = nerou::app::RuntimeSettingsStore::loadPerformance(appProperties.getUserSettings());
                settingsDialog.setAccelerationState(perf.accelerationMode, onnxRunner.getActiveProvider());
                settingsDialog.showOverlay(this);
            }
            return true;
        case WorkspaceCommandAction::SystemAbout:
            if (!aboutDialog.isShowing())
                aboutDialog.showOverlay(this);
            return true;
        case WorkspaceCommandAction::SystemShortcuts:
            if (!shortcutHelpOverlay.isShowing())
                shortcutHelpOverlay.showOverlay(this);
            return true;
        case WorkspaceCommandAction::ModelExport:
            switchTab(Tab::Inference);
            return true;
        case WorkspaceCommandAction::TrainingStop:
            if (isTrainingActive)
                stopTraining();
            return true;
        case WorkspaceCommandAction::PreprocessStop:
            if (isPrepRunning)
                prepBridge.stopTask();
            return true;
        case WorkspaceCommandAction::NodeAcquisition:
            triggerWorkspaceNodePrimaryAction(nerou::domain::NodeType::Acquisition);
            return true;
        case WorkspaceCommandAction::NodePreprocessing:
            triggerWorkspaceNodePrimaryAction(nerou::domain::NodeType::Preprocessing);
            return true;
        case WorkspaceCommandAction::NodeTraining:
            triggerWorkspaceNodePrimaryAction(nerou::domain::NodeType::Training);
            return true;
        case WorkspaceCommandAction::NodeValidation:
            triggerWorkspaceNodePrimaryAction(nerou::domain::NodeType::Validation);
            return true;
        case WorkspaceCommandAction::NodeArchive:
            triggerWorkspaceNodePrimaryAction(nerou::domain::NodeType::Archive);
            return true;
        case WorkspaceCommandAction::AutoPipelineRun:
            executeAutoPipeline();
            return true;
        case WorkspaceCommandAction::None:
        default:
            return false;
    }
}

void MainComponent::handleWorkspaceCommand(const nerou::ui::CommandPalette::CommandItem& rawCmd)
{
    const auto key = rawCmd.id.isNotEmpty() ? rawCmd.id : rawCmd.label;
    const auto registration = resolveWorkspaceCommandRegistration(key);
    if (!registration.has_value())
        return;

    if (!registration->item.enabled)
        return;

    executeWorkspaceCommandAction(registration->action);
}

bool MainComponent::getTopBarPrimaryNodeActionTarget(nerou::domain::NodeType& type) const
{
    switch (currentTab)
    {
        case Tab::Overview:
            type = nerou::domain::NodeType::Acquisition;
            return true;
        case Tab::DataPrep:
            if (!isPrepRunning)
            {
                type = nerou::domain::NodeType::Preprocessing;
                return true;
            }
            return false;
        case Tab::Training:
            if (!isTrainingActive)
            {
                type = nerou::domain::NodeType::Training;
                return true;
            }
            return false;
        default:
            return false;
    }
}

bool MainComponent::getTopBarSecondaryNodeActionTarget(nerou::domain::NodeType& type) const
{
    switch (currentTab)
    {
        case Tab::Acquisition:
            type = nerou::domain::NodeType::Validation;
            return true;
        default:
            return false;
    }
}

void MainComponent::showSystemLogPanel()
{
    if (settingsDialog.isShowing())     settingsDialog.hideOverlay();
    if (shortcutHelpOverlay.isShowing()) shortcutHelpOverlay.hideOverlay();
    if (aboutDialog.isShowing())        aboutDialog.hideOverlay();

    if (!systemLogPanel.isShowing())
        systemLogPanel.showOverlay(this);

    NR_LOGD(W("\u754c\u9762"), W("\u6253\u5f00\u7cfb\u7edf\u65e5\u5fd7\u9762\u677f"));
}

void MainComponent::handleTopPrimaryAction()
{
    executeTopBarAction(describeTopBarPrimaryAction());
}

void MainComponent::handleTopSecondaryAction()
{
    executeTopBarAction(describeTopBarSecondaryAction());
}

void MainComponent::loadLatestOnnxIntoInference(bool switchToInference)
{
    juce::String path;
    if (auto* p = appProperties.getUserSettings())
        path = p->getValue("lastOnnxPath", {});

    if (path.isEmpty() || !juce::File(path).existsAsFile())
    {
        setStatusMsg(W("\u672a\u627e\u5230\u6700\u8fd1\u5bfc\u51fa\u7684 ONNX \u6a21\u578b"), true);
        return;
    }

    inferModelInput.setText(path);
    rebuildInferOnnxCombo();
    if (switchToInference)
        switchTab(Tab::Inference);
    loadOnnxModel();
}

bool MainComponent::loadGoldenSampleForCurrentModel(bool autoRun)
{
    juce::String path = inferModelInput.getText().trim();
    if (path.isEmpty())
        if (auto* p = appProperties.getUserSettings())
            path = p->getValue("lastOnnxPath", {});

    const juce::File modelFile(path);
    if (!modelFile.existsAsFile())
    {
        setStatusMsg(W("\u8bf7\u5148\u52a0\u8f7d\u6709\u6548\u7684 ONNX \u6a21\u578b"), true);
        return false;
    }

    const juce::File goldenDir = modelFile.getParentDirectory().getChildFile("golden_samples");
    if (!goldenDir.isDirectory())
    {
        setStatusMsg(W("\u5f53\u524d\u6a21\u578b\u76ee\u5f55\u4e0b\u6ca1\u6709 golden_samples"), true);
        return false;
    }

    juce::Array<juce::File> npyFiles;
    goldenDir.findChildFiles(npyFiles, juce::File::findFiles, false, "*.npy");
    if (npyFiles.isEmpty())
    {
        setStatusMsg(W("golden_samples \u4e2d\u672a\u627e\u5230 .npy \u6d4b\u8bd5\u6837\u672c"), true);
        return false;
    }

    auto chosen = npyFiles.getFirst();
    for (auto& f : npyFiles)
        if (f.getFileNameWithoutExtension().containsIgnoreCase("input"))
        {
            chosen = f;
            break;
        }

    inferDataInput.setText(chosen.getFullPathName());
    if (!onnxRunner.isModelLoaded() || !modelFile.getFullPathName().equalsIgnoreCase(inferModelInput.getText().trim()))
    {
        inferModelInput.setText(modelFile.getFullPathName());
        loadOnnxModel();
    }
    updateLog(W("[\u4fe1\u606f] \u5df2\u8f7d\u5165 Golden Sample\uff1a") + chosen.getFileName(), inferLog);
    if (autoRun && onnxRunner.isModelLoaded())
        runInference();
    return true;
}

void MainComponent::exportRuntimeDataPackage()
{
    if (runtimeDataExportService.isRunning())
    {
        setStatusMsg(W("Runtime DATA \u6b63\u5728\u751f\u6210\u4e2d\uff0c\u8bf7\u7a0d\u5019"));
        return;
    }

    juce::String onnxPath = inferModelInput.getText().trim();
    if (onnxPath.isEmpty())
        if (auto* p = appProperties.getUserSettings())
            onnxPath = p->getValue("lastOnnxPath", {});

    juce::File onnxFile(onnxPath);
    if (!onnxFile.existsAsFile())
        onnxFile = findFirstOnnxFile(juce::File(savePathInput.getText().trim()));

    if (!onnxFile.existsAsFile())
    {
        setStatusMsg(W("\u672a\u627e\u5230\u53ef\u6253\u5305\u7684 ONNX \u6a21\u578b"), true);
        nerou::ui::SnackbarManager::getInstance().show(
            W("\u8bf7\u5148\u5b8c\u6210\u8bad\u7ec3\u5e76\u5bfc\u51fa ONNX"),
            nerou::ui::MaterialSnackbar::Duration::Long,
            nerou::ui::MaterialSnackbar::Type::Warning);
        return;
    }

    const juce::File dataDir(dataPathInput.getText().trim());
    if (!dataDir.isDirectory() || countFilesWithExtension(dataDir, "*.npz") == 0)
    {
        setStatusMsg(W("\u672a\u627e\u5230\u53ef\u7528\u7684 NPZ \u8bad\u7ec3\u6570\u636e\uff0c\u65e0\u6cd5\u751f\u6210 Runtime DATA"), true);
        return;
    }

    const juce::String modelName = makeFilesystemSafeName(onnxFile.getFileNameWithoutExtension());
    const juce::String stamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    const juce::File outputDir = ProjectPaths::getRuntimePackagesDirectory()
        .getChildFile(modelName + "_" + stamp);

    nerou::services::RuntimeDataExportService::ExportConfig config;
    config.modelDir = onnxFile.getParentDirectory();
    config.dataDir = dataDir;
    config.outputDir = outputDir;
    config.modelName = modelName;
    config.sampleRateHz = (float) juce::jmax(1, prepSrCombo.getSelectedId());
    config.channelNames = buildChannelNames(juce::jmax(1, prepChCombo.getSelectedId()), 2).joinIntoString(",");

    updateLog(W("\u2192 \u751f\u6210 Runtime DATA\uff1a") + outputDir.getFullPathName(), trainLog);
    setStatusMsg(W("Runtime DATA \u751f\u6210\u4e2d..."));

    const bool launched = runtimeDataExportService.exportRuntimeData(
        config,
        [this](juce::String line) {
            updateLog(W("[Runtime DATA] ") + line, trainLog);
        },
        [this](double pct, juce::String msg) {
            setStatusMsg(W("Runtime DATA ") + juce::String(pct * 100.0, 0) + W("% | ") + msg);
        },
        [this](bool success, juce::File output, int fileCount) {
            if (success)
            {
                updateLog(W("[\u5b8c\u6210] Runtime DATA \u751f\u6210\u5b8c\u6210\uff1a") + output.getFullPathName()
                              + W("\uff0c\u6587\u4ef6\u6570\uff1a") + juce::String(fileCount),
                          trainLog);
                setStatusMsg(W("Runtime DATA \u5df2\u751f\u6210\uff0c\u53ef\u7528\u4e8e ONNX Runtime \u751f\u4ea7\u63a8\u7406"));
                nerou::ui::SnackbarManager::getInstance().show(
                    W("Runtime DATA \u751f\u6210\u5b8c\u6210"),
                    nerou::ui::MaterialSnackbar::Duration::Long,
                    nerou::ui::MaterialSnackbar::Type::Success);
                output.startAsProcess();
            }
            else
            {
                updateLog(W("[\u9519\u8bef] Runtime DATA \u751f\u6210\u5931\u8d25"), trainLog);
                setStatusMsg(W("Runtime DATA \u751f\u6210\u5931\u8d25"), true);
            }
            updateTopBarChrome();
        });

    if (!launched)
    {
        setStatusMsg(W("Runtime DATA \u5bfc\u51fa\u811a\u672c\u542f\u52a8\u5931\u8d25"), true);
        updateLog(W("[\u9519\u8bef] \u65e0\u6cd5\u542f\u52a8 Runtime DATA \u5bfc\u51fa\u5668"), trainLog);
    }
    updateTopBarChrome();
}

void MainComponent::updateRealtimeOnnxAlignmentUi()
{
    if (currentTab != Tab::Acquisition)
        return;

    const int devC   = BoardManager::getInstance().getNumChannels();
    const int selT   = realtimeWinLenCombo.getSelectedId();
    const bool onnxL = onnxRunner.isModelLoaded();
    const int needC  = onnxRunner.getInputChannels();
    const int needT  = onnxRunner.getInputTimePoints();

    juce::String line = W("\u5b9e\u65f6 / ONNX\u5bf9\u9f50\uff1a\u8bbe\u5907 ")
                        + juce::String(devC) + W(" ch\uff0c\u7a97\u957f ");
    if (selT > 0)
        line += juce::String(selT);
    else
        line += W("\u2014");

    if (onnxL)
        line += W(" \uff5c \u6a21\u578b\u671f\u671b C=") + juce::String(needC) + W(" T=")
                + juce::String(needT);
    else
        line += W(" \uff5c \u672a\u52a0\u8f7d ONNX");

    const bool wantInfer = realtimeOnnxInferToggle.getToggleState();
    const bool shapeOk =
        !wantInfer || !onnxL || (needC > 0 && needT > 0 && needC == devC && needT == selT);
    if (wantInfer && onnxL && !shapeOk)
        line += W(" \u26a0 \u5f62\u72b6\u4e0d\u5339\u914d\uff0c\u5b9e\u65f6\u63a8\u7406\u5df2\u8df3\u8fc7");

    realtimeAlignLabel.setText(line, juce::dontSendNotification);
    realtimeAlignLabel.setColour(juce::Label::textColourId, shapeOk ? C_TEXT_B : C_WARNING);
}

void MainComponent::dispatchRealtimeInferenceIfNeeded()
{
    if (!(currentTab == Tab::Acquisition && realtimeOnnxInferToggle.getToggleState()
          && BoardManager::getInstance().isStreaming() && onnxRunner.isModelLoaded()
          && !BoardManager::getInstance().isRecordingArmed()))
    {
        realtimeInferCooldown = 0;
        return;
    }

    const int needT = onnxRunner.getInputTimePoints();
    const int needC = onnxRunner.getInputChannels();
    const int selT  = realtimeWinLenCombo.getSelectedId();
    const int devC  = BoardManager::getInstance().getNumChannels();
    const bool shapeOk = (needT > 0 && needC > 0 && selT == needT && needC == devC);

    if (!shapeOk)
    {
        if (++realtimeInferMismatchLog > 90)
        {
            realtimeInferMismatchLog = 0;
            if (needT > 0 && selT != needT)
                juce::Logger::writeToLog(W("[实时监控] 窗长与模型 T 不一致，已跳过实时推理"));
            if (needC > 0 && needC != devC)
                juce::Logger::writeToLog(W("[实时监控] 通道数与模型 C 不一致，已跳过实时推理"));
        }
        return;
    }

    if (--realtimeInferCooldown > 0)
        return;
    realtimeInferCooldown = realtimeInferCooldownBase;

    bool expected = false;
    if (!realtimeInferBusy.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        return;

    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::Thread::launch([safeThis, needT, needC]() {
        if (safeThis == nullptr)
            return;

        std::vector<float> buf;
        if (!BoardManager::getInstance().snapshotLastFramesForOnnx(needT, buf))
        {
            safeThis->realtimeInferBusy.store(false, std::memory_order_release);
            return;
        }

        std::vector<float> results;
        {
            std::lock_guard<std::mutex> lock(safeThis->onnxRunnerMutex);
            if ((int)buf.size() != safeThis->onnxRunner.getExpectedInputElements())
            {
                safeThis->realtimeInferBusy.store(false, std::memory_order_release);
                return;
            }
            SignalNorm::normalizeChannelZScoreClipped(buf, needC, needT, 10.0f);
            results = safeThis->onnxRunner.runInference(buf, false);
        }

        if (results.empty())
        {
            safeThis->realtimeInferBusy.store(false, std::memory_order_release);
            return;
        }

        const int best =
            (int)(std::max_element(results.begin(), results.end()) - results.begin());
        const juce::String bestName =
            (best >= 0 && best < safeThis->inferActiveClassCount)
                ? safeThis->inferLabelNames[(size_t)best]
                : (W("类 ") + juce::String(best));
        const float bestProb = results[(size_t)juce::jmin(best, (int)results.size() - 1)];

        juce::MessageManager::callAsync([safeThis, results = std::move(results), bestName, bestProb]() {
            if (safeThis == nullptr)
                return;
            safeThis->updateInferProbabilityBars(results);
            {
                std::lock_guard<std::mutex> lock(safeThis->realtimeInferResultMutex);
                safeThis->realtimeInferBestName = bestName;
                safeThis->realtimeInferBestProb = bestProb;
            }
            safeThis->realtimeInferResultPending.store(true, std::memory_order_release);
            safeThis->realtimeInferBusy.store(false, std::memory_order_release);
        });
    });
}

void MainComponent::updateRealtimeAdaptiveControl()
{
    if (!(currentTab == Tab::Acquisition && BoardManager::getInstance().isStreaming()))
        return;

    const auto nowMs = juce::Time::getMillisecondCounter();
    if (healthLastEvalMs == 0)
    {
        healthLastEvalMs = nowMs;
        const auto s0 = BoardManager::getInstance().getStreamHealthStats();
        healthLastDroppedTotal = s0.droppedLiveFrames + s0.droppedRingFrames;
        healthLastIngestedTotal = s0.ingestedFrames;
        return;
    }

    if ((nowMs - healthLastEvalMs) < 1000)
        return;

    const auto stats = BoardManager::getInstance().getStreamHealthStats();
    const uint64_t droppedTotal = stats.droppedLiveFrames + stats.droppedRingFrames;
    const uint64_t deltaDropped = droppedTotal - healthLastDroppedTotal;
    const uint64_t deltaIngested = stats.ingestedFrames - healthLastIngestedTotal;
    const double dropRate = (deltaIngested > 0)
        ? (double)deltaDropped / (double)deltaIngested
        : 0.0;

    healthLastDroppedTotal = droppedTotal;
    healthLastIngestedTotal = stats.ingestedFrames;
    healthLastEvalMs = nowMs;

    const bool overloaded = (dropRate > 0.02) || (stats.livePendingDepth > 20000);
    const bool moderate = (dropRate > 0.005) || (stats.livePendingDepth > 5000);
    const bool healthy = (dropRate < 0.001) && (stats.livePendingDepth < 1000);

    int targetLevel = realtimeLoadSheddingLevel;
    if (overloaded)
        targetLevel = 2;
    else if (moderate)
        targetLevel = juce::jmax(targetLevel, 1);
    else if (healthy)
        targetLevel = juce::jmax(0, targetLevel - 1);

    if (healthy)
        ++healthStableWindows;
    else
        healthStableWindows = 0;

    if (healthStableWindows >= 5)
    {
        targetLevel = 0;
        healthStableWindows = 0;
    }

    targetLevel = juce::jlimit(0, 2, targetLevel);
    if (targetLevel != realtimeLoadSheddingLevel)
    {
        realtimeLoadSheddingLevel = targetLevel;
        waveformCanvas.setLoadSheddingLevel(realtimeLoadSheddingLevel);
        realtimeInferCooldownBase = (realtimeLoadSheddingLevel == 0 ? 8
                                 : (realtimeLoadSheddingLevel == 1 ? 14 : 22));

        juce::String modeText = (realtimeLoadSheddingLevel == 0 ? W("实时负载：正常")
                             : (realtimeLoadSheddingLevel == 1 ? W("实时负载：降级一级")
                                                                : W("实时负载：降级二级")));
        juce::Logger::writeToLog(modeText
                                 + W(" | 丢帧率 ")
                                 + juce::String(dropRate * 100.0, 2)
                                 + W("% | 队列 ")
                                 + juce::String(stats.livePendingDepth));
    }
}

void MainComponent::consumeRealtimeInferenceResult()
{
    if (!realtimeInferResultPending.exchange(false, std::memory_order_acq_rel))
        return;
    if (currentTab != Tab::Acquisition)
        return;

    juce::String bestName;
    float bestProb = 0.0f;
    {
        std::lock_guard<std::mutex> lock(realtimeInferResultMutex);
        bestName = realtimeInferBestName;
        bestProb = realtimeInferBestProb;
    }

    realtimeStatusLabel.setText(W("状态：运行中 | 实时 ") + bestName + W(" ")
                                    + juce::String((double)bestProb * 100.0, 0)
                                    + W("%"),
                                juce::dontSendNotification);
    realtimeStatusLabel.setColour(juce::Label::textColourId, C_PRIMARY);
    acquisitionPage.setLiveInferenceResult(bestName, bestProb);
}

int MainComponent::inferSimpleModeNumClasses() const
{
    juce::File m = resolveSelectedManifestFile();
    if (!m.existsAsFile())
        return 4;

    auto parsed = juce::JSON::parse(nerou::core::loadTextFileAsUtf8(m));
    if (!parsed.isObject())
        return 4;

    juce::var labels = parsed.getProperty("labels", juce::var());
    if (auto* arr = labels.getArray())
        return juce::jmax(2, arr->size());
    return 4;
}

void MainComponent::applyExpertUiVisibility()
{
    uiExpertToggle.setToggleState(expertUi, juce::dontSendNotification);
    uiExpertToggle.setButtonText(expertUi
                                     ? W("\u4e13\u5bb6\u9009\u9879\uff08\u5f00\uff09")
                                     : W("\u4e13\u5bb6\u9009\u9879\uff08\u5173\uff09"));
    refreshWorkspaceCommands();

    classCountSecLabel.setVisible(expertUi);
    classCountInput.setVisible(expertUi);
    lrSecLabel.setVisible(expertUi);
    lrCombo.setVisible(expertUi);

    prepLowLabel.setVisible(expertUi);
    prepLowInput.setVisible(expertUi);
    prepHighLabel.setVisible(expertUi);
    prepHighInput.setVisible(expertUi);

    realtimeMontageRowLabel.setVisible(expertUi);
    realtimeMontageCombo.setVisible(expertUi);
    realtimeNumWinCombo.setVisible(expertUi);

    if (!expertUi && realtimeNumWinCombo.getSelectedId() != 1)
        realtimeNumWinCombo.setSelectedId(1, juce::sendNotificationSync);

    persistRealtimeSettings();
}

//==============================================================================
void MainComponent::persistPrepSettings()
{
    nerou::app::PrepSettings settings;
    settings.rawPath = prepRawInput.getText();
    settings.outPath = prepOutInput.getText();
    settings.lowHz = prepLowInput.getText();
    settings.highHz = prepHighInput.getText();
    settings.sampleRateHz = prepSrCombo.getSelectedId();
    settings.channelCount = prepChCombo.getSelectedId();
    nerou::app::RuntimeSettingsStore::savePrep(appProperties.getUserSettings(), settings);
}

void MainComponent::persistTrainingFields()
{
    nerou::app::TrainingSettings settings;
    settings.modelName = nameInput.getText();
    settings.dataPath = dataPathInput.getText();
    settings.epochs = epochInput.getText();
    settings.trainClassCount = classCountInput.getText();
    settings.savePath = savePathInput.getText();
    settings.batchId = batchCombo.getSelectedId();
    settings.lrId = lrCombo.getSelectedId();
    nerou::app::RuntimeSettingsStore::saveTraining(appProperties.getUserSettings(), settings);
}

void MainComponent::persistRealtimeSettings()
{
    nerou::app::RealtimeSettings settings;
    settings.sourceId = 3;
    settings.channelsId = realtimeChannelsCombo.getSelectedId();
    settings.rateId = realtimeRateCombo.getSelectedId();
    settings.playbackPath = realtimePlaybackInput.getText();
    settings.liveConnection = realtimeLiveConnInput.getText();
    settings.uvRangeId = realtimeUvRangeCombo.getSelectedId();
    settings.filterId = realtimeFilterCombo.getSelectedId();
    settings.montageId = realtimeMontageCombo.getSelectedId();
    settings.winLenId = realtimeWinLenCombo.getSelectedId();
    settings.numWinId = realtimeNumWinCombo.getSelectedId();
    nerou::app::RuntimeSettingsStore::saveRealtime(appProperties.getUserSettings(), settings);
}

void MainComponent::loadRealtimeSettings()
{
    const auto settings =
        nerou::app::RuntimeSettingsStore::loadRealtime(appProperties.getUserSettings());

    realtimeSourceCombo.setSelectedId(3, juce::dontSendNotification);

    if (settings.channelsId == 8 || settings.channelsId == 16 || settings.channelsId == 32
        || settings.channelsId == 64)
    {
        realtimeChannelsCombo.setSelectedId(settings.channelsId, juce::dontSendNotification);
    }

    for (int cand : { 256, 250, 500, 1000, 2000 })
        if (cand == settings.rateId)
        {
            realtimeRateCombo.setSelectedId(settings.rateId, juce::dontSendNotification);
            break;
        }

    realtimePlaybackInput.setText(settings.playbackPath, false);
    realtimeLiveConnInput.setText(settings.liveConnection, false);

    if (settings.uvRangeId == 50 || settings.uvRangeId == 100 || settings.uvRangeId == 1000
        || settings.uvRangeId == 10000 || settings.uvRangeId == 100000)
    {
        realtimeUvRangeCombo.setSelectedId(settings.uvRangeId, juce::dontSendNotification);
        waveformCanvas.setVerticalHalfRangeUv((float) settings.uvRangeId);
    }

    if (settings.filterId >= 1 && settings.filterId <= 5)
    {
        realtimeFilterCombo.setSelectedId(settings.filterId, juce::dontSendNotification);
        applyRealtimeDisplayFilterFromCombo();
    }

    if (settings.montageId >= 1 && settings.montageId <= 4)
    {
        realtimeMontageCombo.setSelectedId(settings.montageId, juce::dontSendNotification);
        applyRealtimeMontageFromCombo();
    }

    if (settings.winLenId == 128 || settings.winLenId == 256 || settings.winLenId == 512)
        realtimeWinLenCombo.setSelectedId(settings.winLenId, juce::dontSendNotification);

    for (int cand : { 1, 2, 4, 8, 16 })
        if (cand == settings.numWinId)
        {
            realtimeNumWinCombo.setSelectedId(settings.numWinId, juce::dontSendNotification);
            break;
        }

    applyRealtimeTimeWindowFromControls();
    applyRealtimeChannelPaging(true);
}

//==============================================================================
// ENCODING FIX: All Chinese strings set via L"..." wide literals.
// On MSVC Windows, wchar_t literals are ALWAYS stored as UTF-16 codepoints
// regardless of /utf-8, /source-charset, BOM, or system ANSI codepage.
// juce::String(const wchar_t*) correctly converts UTF-16 to internal UTF-8.
//==============================================================================
void MainComponent::setupComponentText()
{
    using S = juce::String;
    const auto N = juce::dontSendNotification;

    // ── Sidebar ────────────────────────────────────────────────────────────────
appTitleLabel.setText(S(L"\u795e\u7ecf\u8fd0\u884c\u65f6"), N);           // 神经运行
appSubLabel.setText  (S(L"EEG / \u795e\u7ecf\u751f\u7406\u4fe1\u53f7\u5206\u7c7b\u8bad\u7ec3\u4e0e ONNX Runtime DATA"), N);
tabOverviewBtn.setButtonText   (S(L"\u6a21\u578b\u8bad\u7ec3"));
tabAcquisitionBtn.setButtonText(S(L"\u6a21\u578b\u8bad\u7ec3"));
tabDataPrepBtn.setButtonText   (S(L"\u6a21\u578b\u8bad\u7ec3"));
tabTrainBtn.setButtonText      (S(L"\u6a21\u578b\u8bad\u7ec3"));
tabInferBtn.setButtonText      (S(L"\u63a8\u7406\u6a21\u578b\u751f\u6210"));
trainPreflightTitle.setText(S(L"\u8bad\u7ec3\u524d\u68c0\u67e5"), N);
    trainPreflightRefreshBtn.setButtonText(S(L"\u5237\u65b0"));

    // ── Realtime tab header（先前仅用头文件窄字符初始化 部分环境乱码）──────────
realtimeTitleLabel.setText(S(L"\u8bad\u7ec3\u6587\u4ef6\u6570\u636e"), N);
realtimeStatusLabel.setText(S(L"\u72b6\u6001\uff1a\u7b49\u5f85\u8bad\u7ec3\u6587\u4ef6"), N);
startBoardBtn.setButtonText(S(L"\u52a0\u8f7d\u6587\u4ef6"));
stopBoardBtn.setButtonText(S(L"\u505c\u6b62\u9884\u89c8"));
realtimeRecordBtn.setButtonText(S(L"\u5bfc\u51fa\u9884\u89c8 NPZ"));
realtimeOnnxInferToggle.setButtonText(S(L"\u6587\u4ef6 ONNX"));
realtimePlaybackLabel.setText(S(L"\u8bad\u7ec3\u6587\u4ef6"), N);
realtimePlaybackBrowseBtn.setButtonText(S(L"\u6d4f\u89c8"));        // 浏览
realtimeLiveConnLabel.setText(S(L"\u8fde\u63a5\u5df2\u7981\u7528"), N);
realtimeLiveConnCopyBtn.setButtonText(S(L"\u590d\u5236"));           // 复制
realtimeFilterRowLabel.setText(S(L"\u6ee4\u6ce2\u8bbe\u7f6e"), N);  // 滤波设置
realtimeMontageRowLabel.setText(S(L"\u5bfc\u8054\u6807\u6ce8"), N); // 瀵艰仈鏍囨敞

    // ── Tab 1: Training params ─────────────────────────────────────────────────
nameSecLabel.setText (S(L"\u25cf \u6a21\u578b\u540d\u79f0"), N);                      // 模型名称
dataSecLabel.setText (S(L"\u25cf \u6570\u636e\u96c6\u8def\u5f84"), N);                 // 数据集路
browseDirBtn.setButtonText(S(L"\u6d4f\u89c8"));                                        // 浏览
epochSecLabel.setText(S(L"\u25cf \u8bad\u7ec3\u8f6e\u6570 (Epoch)"), N);               // 训练轮数
classCountSecLabel.setText(S(L"\u25cf \u5206\u7c7b\u6570\u91cf"), N);                   // 分类数量
batchSecLabel.setText(S(L"\u25cf \u6279\u6b21\u5927\u5c0f (Batch)"), N);               // 批次大小
lrSecLabel.setText   (S(L"\u25cf \u521d\u59cb\u5b66\u4e60\u7387"), N);                 // 初始学习
saveSecLabel.setText (S(L"\u25cf \u4fdd\u5b58\u8def\u5f84"), N);                       // 保存路径
browseSaveBtn.setButtonText(S(L"\u6d4f\u89c8"));                                       // 浏览

    // ── Training center ────────────────────────────────────────────────────────
pageTitleLabel.setText       (S(L"\u8bad\u7ec3\u914d\u7f6e"), N);          // 训练配置
statusChipLabel.setText      (S(L"\u5c31\u7eea"), N);                      // 就绪
elapsedLabel.setText         (S(L"\u5df2\u7528\u65f6 \u2014\u2014"), N);  // 已用—
etaLabel.setText             (S(L"\u9884\u8ba1\u5269\u4f59 \u2014\u2014"), N); // 预计剩余 —
progressSectionLabel.setText (S(L"\u8bad\u7ec3\u8fdb\u5ea6"), N);          // 训练进度
epochBarLabel.setText        (S(L"\u5f53\u524d\u8bad\u7ec3\u8f6e\u6b21"), N);          // 当前训练轮次
trainLogTitle.setText        (S(L"\u5b9e\u65f6\u8bad\u7ec3\u65e5\u5fd7"), N); // 实时训练日志

    // ── Training buttons ───────────────────────────────────────────────────────
startBtn.setButtonText    (S(L"\u5f00\u59cb\u8bad\u7ec3"));  // 开始训练
pauseBtn.setButtonText    (S(L"\u6682\u505c"));              // 鏆傚仠
stopBtn.setButtonText     (S(L"\u505c\u6b62"));              // 停止
saveLogBtn.setButtonText  (S(L"\u4fdd\u5b58\u65e5\u5fd7")); // 保存日志
saveChartBtn.setButtonText(S(L"\u4fdd\u5b58\u56fe\u8868")); // 保存图表
clearLogBtn.setButtonText (S(L"\u6e05\u7a7a\u65e5\u5fd7")); // 清空日志

    // ── Tab 2: Preprocess ──────────────────────────────────────────────────────
prepTitleLabel.setText  (S(L"\u6570\u636e\u9884\u5904\u7406"), N);              // 数据预处理
prepBrowseRaw.setButtonText(S(L"\u6d4f\u89c8"));
    prepOutLabel.setText    (S(L"\u25cf \u8f93\u51fa\u76ee\u5f55 (NPZ)"), N);       // 输出目录
prepBrowseOut.setButtonText(S(L"\u6d4f\u89c8"));
    prepSrLabel.setText     (S(L"\u25cf \u76ee\u6807\u91c7\u6837\u7387 (Hz)"), N);  // 目标采样
prepLowLabel.setText    (S(L"\u25cf \u5e26\u901a\u6ee4\u6ce2 \u4f4e\u9891 (Hz)"), N); // ● 带通滤波 低频
prepHighLabel.setText   (S(L"\u25cf \u5e26\u901a\u6ee4\u6ce2 \u9ad8\u9891 (Hz)"), N); // ● 带通滤波 高频
prepChLabel.setText     (S(L"\u25cf \u901a\u9053\u6570"), N);                    // 通道
prepStartBtn.setButtonText(S(L"\u5f00\u59cb\u9884\u5904\u7406"));               // 开始预处理
prepClearBtn.setButtonText(S(L"\u6e05\u7a7a\u65e5\u5fd7"));                     // 清空日志
prepSyncTrainBtn.setButtonText(S(L"\u540c\u6b65\u5230\u8bad\u7ec3"));          // 同步到训
trainTemplateSecLabel.setText(
        S(L"\u25cf \u6a21\u578b\u6a21\u677f\uff08manifest\uff09"), N); // 模型模板（manifest
inferOnnxPickLabel.setText(S(L"\u25cf \u5feb\u901f\u9009\u62e9 ONNX"), N); // ● 快速选择 ONNX
prepStatusLabel.setText (S(L"\u7b49\u5f85\u4e2d"), N);                           // 等待
prepLogTitle.setText    (S(L"\u9884\u5904\u7406\u65e5\u5fd7"), N);              // 预处理日
    // ── Tab 3: Inference ───────────────────────────────────────────────────────
inferTitleLabel.setText   (S(L"ONNX Runtime \u9a8c\u8bc1"), N);
inferModelLabel.setText   (S(L"\u25cf ONNX \u6a21\u578b\u8def\u5f84"), N);      // ONNX 模型路径
inferBrowseModel.setButtonText(S(L"\u6d4f\u89c8"));
    inferLoadBtn.setButtonText(S(L"\u52a0\u8f7d\u6a21\u578b"));                      // 加载模型
inferUseLastExportBtn.setButtonText(S(L"\u4f7f\u7528\u4e0a\u6b21\u5bfc\u51fa")); // 使用上次导出
inferOnnxPickLabel.setText(S(L"\u25cf \u5feb\u901f\u9009\u62e9 ONNX"), N); // ● 快速选择 ONNX
inferBrowseData.setButtonText(S(L"\u6d4f\u89c8"));
    inferRunBtn.setButtonText (S(L"\u8fd0\u884c\u9a8c\u8bc1"));                      // 运行验证
inferClearBtn.setButtonText(S(L"\u6e05\u7a7a\u7ed3\u679c"));                    // 清空结果
inferResultTitle.setText  (S(L"\u9a8c\u8bc1\u7ed3\u679c"), N);                  // 验证结果
inferStatusLabel.setText  (S(L"\u7b49\u5f85\u52a0\u8f7d\u6a21\u578b..."), N);  // 等待加载模型...
inferLogTitle.setText     (S(L"\u9a8c\u8bc1\u65e5\u5fd7"), N);                  // 验证日志

    // ── Status bar ─────────────────────────────────────────────────────────────
statusBarRight.setText(S(L"\u795e\u7ecf\u8fd0\u884c\u65f6 v1.0.0  |  JUCE 7.0.9  "),
                           N); // 神经运行
}













// ============================================================
// 节点主动作触发（顶栏 / 命令面板）
// ============================================================

void MainComponent::triggerWorkspaceNodePrimaryAction(nerou::domain::NodeType t)
{
    overviewSelectedNode = t;

    const auto guard = workflowOrchestrator.checkNodeAccess(t);
    if (!guard.canEnter)
    {
        // 守卫拦截：仅切到对应 Tab，让用户看到阻断原因
        switchTab(nerou::app::WorkflowOrchestrator::nodePrimaryTab(t));
        return;
    }

    switch (t)
    {
        case nerou::domain::NodeType::Preprocessing:
            switchTab(Tab::DataPrep);
            startPreprocess();
            break;
        case nerou::domain::NodeType::Training:
            switchTab(Tab::Training);
            startTraining();
            break;
        case nerou::domain::NodeType::Acquisition:
            switchTab(Tab::Acquisition);
            break;
        case nerou::domain::NodeType::Validation:
            switchTab(Tab::Inference);
            break;
        case nerou::domain::NodeType::Archive:
            switchTab(Tab::Overview);
            break;
        default:
            break;
    }
}

// ============================================================
// SettingsDialog wiring
// ============================================================

// 注册 SettingsDialog 的所有按钮回调。
// 历史遗留：这些回调原先嵌在已删除的 setupPipelineCanvas() 中且从未被调用，
// 导致"快捷键帮助 / 关于 / 加速模式切换"3 个功能哑火。重构后由构造函数显式调用本函数修复。
void MainComponent::wireSettingsDialog()
{
    settingsDialog.onOpenShortcutHelp = [this]
    {
        shortcutHelpOverlay.showOverlay(this);
    };
    settingsDialog.onOpenAbout = [this]
    {
        aboutDialog.showOverlay(this);
    };

    // 加速模式切换：持久化 → 应用到推理引擎 / 验证服务 → 系统日志
    settingsDialog.onAccelerationModeChanged = [this](const juce::String& key)
    {
        nerou::app::PerformanceSettings ps;
        ps.accelerationMode = key;
        nerou::app::RuntimeSettingsStore::savePerformance(appProperties.getUserSettings(), ps);

        const auto mode = accel::fromKey(key);
        onnxRunner.setAccelerationMode(mode);
        validationService.setPreferredAccelerationMode(mode);
        NR_LOGI("System", juce::String::fromUTF8(u8"推理加速偏好已变更：") + accel::toDisplayName(mode)
                 + juce::String::fromUTF8(u8"（下次加载模型生效）"));

        settingsDialog.setAccelerationState(key, onnxRunner.getActiveProvider());
    };
}

void MainComponent::wirePipelineStoreCallbacks()
{
    // AcquisitionService 没有全局回调接口，采集完成通知在
    // finalizeRecording 的 doneCb lambda 中直接调用（见 startAcquisitionCapture）
    // 此处留作未来扩展点
}

void MainComponent::executeAutoPipeline()
{
    isAutoPipelineActive = true;
    switchTab(Tab::DataPrep);
    startPreprocess();
    nerou::ui::SnackbarManager::getInstance().show(
        NR_STR("\u5df2\u542f\u52a8 Auto-Pipeline: \u9884\u5904\u7406 -> \u8bad\u7ec3 -> \u63a8\u7406"),
        nerou::ui::MaterialSnackbar::Duration::Long,
        nerou::ui::MaterialSnackbar::Type::Info);
}
