#include "ParamFieldRegistry.h"

namespace nerou::ui::google {

static ParamSpec makeDropdown(juce::String key, juce::String label,
                              juce::StringArray options, juce::var def,
                              juce::String hint = {})
{
    ParamSpec s;
    s.key = std::move(key);
    s.label = std::move(label);
    s.kind = ParamKind::Dropdown;
    s.options = std::move(options);
    s.defaultValue = std::move(def);
    s.hint = std::move(hint);
    return s;
}

static ParamSpec makeSlider(juce::String key, juce::String label,
                            double mn, double mx, double step, juce::var def,
                            juce::String unit = {}, juce::String hint = {})
{
    ParamSpec s;
    s.key = std::move(key);
    s.label = std::move(label);
    s.kind = ParamKind::Slider;
    s.min = mn; s.max = mx; s.step = step;
    s.defaultValue = std::move(def);
    s.unit = std::move(unit);
    s.hint = std::move(hint);
    return s;
}

static ParamSpec makeNumber(juce::String key, juce::String label,
                            double mn, double mx, juce::var def,
                            juce::String unit = {}, juce::String hint = {})
{
    ParamSpec s;
    s.key = std::move(key);
    s.label = std::move(label);
    s.kind = ParamKind::Number;
    s.min = mn; s.max = mx;
    s.defaultValue = std::move(def);
    s.unit = std::move(unit);
    s.hint = std::move(hint);
    return s;
}

static ParamSpec makeToggle(juce::String key, juce::String label,
                            bool def, juce::String hint = {})
{
    ParamSpec s;
    s.key = std::move(key);
    s.label = std::move(label);
    s.kind = ParamKind::Toggle;
    s.defaultValue = juce::var(def);
    s.hint = std::move(hint);
    return s;
}

static ParamSpec makeText(juce::String key, juce::String label,
                          juce::String def, juce::String hint = {})
{
    ParamSpec s;
    s.key = std::move(key);
    s.label = std::move(label);
    s.kind = ParamKind::Text;
    s.defaultValue = juce::var(def);
    s.hint = std::move(hint);
    return s;
}

static ParamPreset makePreset(juce::String id, juce::String name,
                              juce::String description,
                              std::initializer_list<std::pair<juce::String, juce::var>> kv)
{
    ParamPreset p;
    p.id = std::move(id);
    p.name = std::move(name);
    p.description = std::move(description);
    for (auto& [k, v] : kv)
        p.values.set(k, v);
    return p;
}

NodeSchema getSchemaForNode(domain::NodeType type)
{
    using juce::String;
    NodeSchema s;

    switch (type)
    {
        case domain::NodeType::Acquisition:
        {
            s.fields = {
                makeDropdown("file_format", juce::String::fromUTF8(u8"信号文件格式"),
                             { "EDF/BDF", "BrainVision", "EEGLAB SET", "FIF", "CSV", "NPY", "MAT" },
                             juce::var("EDF/BDF"), juce::String::fromUTF8(u8"仅支持 EEG/神经生理信号文件")),
                makeDropdown("channel_schema", juce::String::fromUTF8(u8"通道命名方案"),
                             { "10-20", "10-10", "10-5", "Custom" }, juce::var("10-20")),
                makeDropdown("sample_rate", juce::String::fromUTF8(u8"目标采样率"),
                             { "128", "250", "500", "1000", "2000" }, juce::var(250), "Hz"),
                makeToggle("parse_events", juce::String::fromUTF8(u8"解析事件/Marker"), true),
                makeToggle("reject_non_signal", juce::String::fromUTF8(u8"拒绝非信号文件"), true),
            };
            s.presets = {
                makePreset("clinical_edf", juce::String::fromUTF8(u8"临床 EDF"),
                           juce::String::fromUTF8(u8"EDF/BDF · 250Hz · 10-20 · 自动事件"),
                           {
                               { "file_format",    juce::var("EDF/BDF") },
                               { "sample_rate",    juce::var(250) },
                               { "channel_schema", juce::var("10-20") },
                               { "parse_events",   juce::var(true) },
                           }),
                makePreset("research_fif", juce::String::fromUTF8(u8"研究级 FIF"),
                           juce::String::fromUTF8(u8"FIF · 1000Hz · 10-10 · 自定义通道"),
                           {
                               { "file_format",    juce::var("FIF") },
                               { "sample_rate",    juce::var(1000) },
                               { "channel_schema", juce::var("10-10") },
                               { "parse_events",   juce::var(true) },
                           }),
            };
            break;
        }

        case domain::NodeType::Preprocessing:
        {
            s.fields = {
                makeDropdown("sr", juce::String::fromUTF8(u8"重采样率"),
                             { "250", "500", "1000" }, juce::var(250), "Hz"),
                makeDropdown("filter", juce::String::fromUTF8(u8"滤波"),
                             { juce::String::fromUTF8(u8"原始值"), "1-45Hz", "5-35Hz",
                               "8-25Hz", juce::String::fromUTF8(u8"去平均") },
                             juce::var("1-45Hz")),
                makeDropdown("notch", juce::String::fromUTF8(u8"工频陷波"),
                             { "Off", "50Hz", "60Hz" }, juce::var("50Hz")),
                makeSlider("window_sec", juce::String::fromUTF8(u8"时间窗"),
                           0.5, 10.0, 0.25, juce::var(2.0), "s",
                           juce::String::fromUTF8(u8"分段时间窗长度")),
                makeSlider("overlap", juce::String::fromUTF8(u8"窗口重叠"),
                           0.0, 0.9, 0.05, juce::var(0.5)),
                makeDropdown("normalization", juce::String::fromUTF8(u8"标准化策略"),
                             { "Per-channel z-score", "Global z-score", "MinMax", "None" },
                             juce::var("Per-channel z-score")),
                makeNumber("channels", juce::String::fromUTF8(u8"导联数"),
                           1, 128, juce::var(16)),
                makeToggle("export_dataset_manifest", juce::String::fromUTF8(u8"输出 dataset_manifest"), true),
                makeToggle("augment", juce::String::fromUTF8(u8"数据增强"), false),
            };
            s.presets = {
                makePreset("bci_std", juce::String::fromUTF8(u8"常规 BCI"),
                           juce::String::fromUTF8(u8"1-45Hz · 2s 窗 · 50% 重叠"),
                           {
                               { "sr",          juce::var(250) },
                               { "filter",      juce::var("1-45Hz") },
                               { "window_sec",  juce::var(2.0) },
                               { "overlap",     juce::var(0.5) },
                               { "normalize",   juce::var(true) },
                           }),
                makePreset("mi_motor", juce::String::fromUTF8(u8"运动想象"),
                           juce::String::fromUTF8(u8"8-25Hz · 4s 窗 · μ/β 节律"),
                           {
                               { "sr",          juce::var(250) },
                               { "filter",      juce::var("8-25Hz") },
                               { "window_sec",  juce::var(4.0) },
                               { "overlap",     juce::var(0.5) },
                               { "normalize",   juce::var(true) },
                           }),
                makePreset("ssvep", juce::String::fromUTF8(u8"SSVEP 刺激"),
                           juce::String::fromUTF8(u8"5-35Hz · 1s 窗 · 25% 重叠"),
                           {
                               { "sr",          juce::var(250) },
                               { "filter",      juce::var("5-35Hz") },
                               { "window_sec",  juce::var(1.0) },
                               { "overlap",     juce::var(0.25) },
                               { "normalize",   juce::var(true) },
                           }),
            };
            break;
        }

        case domain::NodeType::Training:
        {
            s.fields = {
                makeDropdown("model_template", juce::String::fromUTF8(u8"模型模板"),
                             { "EEGNet", "EEG-Conformer", "BIOT", "LaBraM", "Custom" },
                             juce::var("EEGNet"),
                             juce::String::fromUTF8(u8"骨干网络模板")),
                makeText("task_type", juce::String::fromUTF8(u8"任务类型"), "neuro_signal_classification"),
                makeNumber("class_count", juce::String::fromUTF8(u8"分类数量"),
                           2, 64, juce::var(4)),
                makeDropdown("paradigm", juce::String::fromUTF8(u8"训练范式"),
                             { juce::String::fromUTF8(u8"监督"),
                               juce::String::fromUTF8(u8"预训练"),
                               juce::String::fromUTF8(u8"微调") },
                             juce::var(juce::String::fromUTF8(u8"监督"))),
                makeSlider("epochs", juce::String::fromUTF8(u8"训练轮次"),
                           1, 500, 1, juce::var(50), juce::String::fromUTF8(u8"epoch")),
                makeSlider("batch_size", juce::String::fromUTF8(u8"批大小"),
                           4, 256, 2, juce::var(32)),
                makeSlider("lr", juce::String::fromUTF8(u8"学习率"),
                           0.0001, 0.1, 0.0001, juce::var(0.001)),
                makeDropdown("optimizer", juce::String::fromUTF8(u8"优化器"),
                             { "Adam", "AdamW", "SGD", "RMSProp" }, juce::var("AdamW")),
                makeDropdown("loss", juce::String::fromUTF8(u8"损失函数"),
                             { "CrossEntropy", "FocalLoss", "LabelSmoothing" },
                             juce::var("CrossEntropy")),
                makeToggle("max_norm", juce::String::fromUTF8(u8"Max-Norm 约束"), true),
                makeToggle("export_onnx_after_train", juce::String::fromUTF8(u8"训练后导出 ONNX"), true),
            };
            s.presets = {
                makePreset("quick", juce::String::fromUTF8(u8"快速迭代"),
                           juce::String::fromUTF8(u8"20 epoch · bs 32 · lr 3e-3"),
                           {
                               { "epochs",     juce::var(20) },
                               { "batch_size", juce::var(32) },
                               { "lr",         juce::var(0.003) },
                               { "optimizer",  juce::var("AdamW") },
                               { "loss",       juce::var("CrossEntropy") },
                           }),
                makePreset("std", juce::String::fromUTF8(u8"标准"),
                           juce::String::fromUTF8(u8"50 epoch · bs 32 · lr 1e-3"),
                           {
                               { "epochs",     juce::var(50) },
                               { "batch_size", juce::var(32) },
                               { "lr",         juce::var(0.001) },
                               { "optimizer",  juce::var("AdamW") },
                               { "loss",       juce::var("CrossEntropy") },
                           }),
                makePreset("precision", juce::String::fromUTF8(u8"高精度"),
                           juce::String::fromUTF8(u8"200 epoch · bs 16 · lr 3e-4 · Focal"),
                           {
                               { "epochs",     juce::var(200) },
                               { "batch_size", juce::var(16) },
                               { "lr",         juce::var(0.0003) },
                               { "optimizer",  juce::var("AdamW") },
                               { "loss",       juce::var("FocalLoss") },
                           }),
            };
            break;
        }

        case domain::NodeType::Validation:
        {
            s.fields = {
                makeDropdown("runtime_ep", juce::String::fromUTF8(u8"Runtime EP"),
                             { "CPU", "DirectML", "CUDA", "Auto" }, juce::var("Auto")),
                makeSlider("onnx_abs_error", juce::String::fromUTF8(u8"ONNX 最大误差阈值"),
                           0.0, 0.01, 0.0001, juce::var(0.001)),
                makeSlider("threshold", juce::String::fromUTF8(u8"分类置信阈值"),
                           0.0, 1.0, 0.01, juce::var(0.5)),
                makeSlider("latency_ms", juce::String::fromUTF8(u8"延迟预算"),
                           50, 2000, 10, juce::var(300), "ms"),
                makeDropdown("split", juce::String::fromUTF8(u8"数据划分"),
                             { juce::String::fromUTF8(u8"留一受试者"),
                               juce::String::fromUTF8(u8"受试内交叉"),
                               juce::String::fromUTF8(u8"时间切分") },
                             juce::var(juce::String::fromUTF8(u8"时间切分"))),
                makeDropdown("metric", juce::String::fromUTF8(u8"主指标"),
                             { "Accuracy", "Macro-F1", "AUROC", "Kappa" },
                             juce::var("Macro-F1")),
                makeToggle("confusion", juce::String::fromUTF8(u8"混淆矩阵"), true),
                makeToggle("golden_sample", juce::String::fromUTF8(u8"生成 Golden Sample"), true),
            };
            s.presets = {
                makePreset("loso", juce::String::fromUTF8(u8"跨受试"),
                           juce::String::fromUTF8(u8"留一受试者 · Macro-F1"),
                           {
                               { "split",   juce::var(juce::String::fromUTF8(u8"留一受试者")) },
                               { "metric",  juce::var("Macro-F1") },
                           }),
                makePreset("timed", juce::String::fromUTF8(u8"时间切分"),
                           juce::String::fromUTF8(u8"8:2 时序划分 · Accuracy"),
                           {
                               { "split",   juce::var(juce::String::fromUTF8(u8"时间切分")) },
                               { "metric",  juce::var("Accuracy") },
                           }),
            };
            break;
        }

        case domain::NodeType::Archive:
        {
            s.fields = {
                makeText("version", juce::String::fromUTF8(u8"版本号"), "v1.0.0"),
                makeText("package_name", juce::String::fromUTF8(u8"RuntimePackage 名称"), "runtime_package"),
                makeDropdown("runtime_target", juce::String::fromUTF8(u8"生产目标"),
                             { "ONNX Runtime CPU", "ONNX Runtime DirectML", "ONNX Runtime CUDA" },
                             juce::var("ONNX Runtime CPU")),
                makeDropdown("input_dtype", juce::String::fromUTF8(u8"输入类型"),
                             { "float32", "float16", "int8" }, juce::var("float32")),
                makeToggle("include_preprocessing", juce::String::fromUTF8(u8"包含预处理配置"), true),
                makeToggle("include_sample_io", juce::String::fromUTF8(u8"包含示例输入输出"), true),
                makeToggle("include_report", juce::String::fromUTF8(u8"包含验证报告"), true),
            };
            s.presets = {
                makePreset("release", juce::String::fromUTF8(u8"生产发布"),
                           juce::String::fromUTF8(u8"ONNX Runtime DATA · manifest · sample I/O"),
                           {
                               { "runtime_target",        juce::var("ONNX Runtime CPU") },
                               { "input_dtype",           juce::var("float32") },
                               { "include_preprocessing", juce::var(true) },
                               { "include_sample_io",     juce::var(true) },
                               { "include_report",        juce::var(true) },
                           }),
                makePreset("edge", juce::String::fromUTF8(u8"边缘设备"),
                           juce::String::fromUTF8(u8"DirectML · float16 · 完整 manifest"),
                           {
                               { "runtime_target",        juce::var("ONNX Runtime DirectML") },
                               { "input_dtype",           juce::var("float16") },
                               { "include_preprocessing", juce::var(true) },
                               { "include_sample_io",     juce::var(true) },
                               { "include_report",        juce::var(true) },
                           }),
            };
            break;
        }
        default:
            break;
    }
    return s;
}

} // namespace nerou::ui::google
