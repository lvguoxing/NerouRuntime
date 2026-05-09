#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <cstddef>
#include <vector>

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace PythonBridgeDetail {

inline juce::String quoteCmdArg(const juce::String& s)
{
    return (s.containsAnyOf(" \t\"&^|<>()") || s.isEmpty()) ? s.quoted() : s;
}

/** Decode child stdout bytes: UTF-8 skip BOM; Windows: retry GBK/ACP if UTF-8 yields U+FFFD. */
inline juce::String decodeChildText(const char* data, int byteLen)
{
    if (byteLen <= 0 || data == nullptr)
        return {};

    int off = 0;
    if (byteLen >= 3 && (unsigned char) data[0] == 0xef && (unsigned char) data[1] == 0xbb && (unsigned char) data[2] == 0xbf)
        off = 3;

    const char* ptr = data + off;
    const int blen = byteLen - off;
    if (blen <= 0)
        return {};

    auto hasReplacement = [](const juce::String& s) {
        for (int i = 0; i < s.length(); ++i)
            if (s[i] == (juce::juce_wchar) 0xfffd)
                return true;
        return false;
    };

    juce::String line = juce::String::fromUTF8(ptr, blen).trim();

#if JUCE_WINDOWS
    if (!hasReplacement(line))
        return line;

    auto tryCp = [&](UINT cp) -> juce::String {
        const int n = MultiByteToWideChar(cp, 0, ptr, blen, nullptr, 0);
        if (n <= 0)
            return {};
        juce::HeapBlock<wchar_t> wb((size_t) n + 1);
        if (MultiByteToWideChar(cp, 0, ptr, blen, wb.get(), n) <= 0)
            return {};
        wb[n] = 0;
        return juce::String(wb.get()).trim();
    };

    juce::String g = tryCp(936);
    if (g.isEmpty() || hasReplacement(g))
        g = tryCp(CP_ACP);
    if (g.isNotEmpty() && !hasReplacement(g))
        line = g;
#endif
    return line;
}

#if JUCE_WINDOWS
/** Windows: UTF-8 console + Python UTF-8 mode through cmd (avoids garbled CJK in logs). */
inline bool startPythonViaCmd(juce::ChildProcess& proc, const juce::String& innerCommandLine)
{
    juce::StringArray w;
    w.add("cmd.exe");
    w.add("/s");
    w.add("/c");
    w.add(innerCommandLine);
    return proc.start(w);
}
#endif

/** Start `python -X utf8 -u <script> [extraArgs...]` (same env as launchScript); for one-shot ChildProcess uses. */
inline bool runPythonScriptProcess(juce::ChildProcess& proc, const juce::File& pyScript, const juce::StringArray& extraArgs)
{
#if JUCE_WINDOWS
    juce::String inner = "chcp 65001>nul && set PYTHONUTF8=1&& set PYTHONIOENCODING=utf-8&& python -X utf8 -u ";
    inner += quoteCmdArg(pyScript.getFullPathName());
    for (auto& a : extraArgs)
        inner << " " << quoteCmdArg(a);
    return startPythonViaCmd(proc, inner);
#else
    juce::StringArray args;
    args.add("python");
    args.add("-Xutf8");
    args.add("-u");
    args.add(pyScript.getFullPathName());
    args.addArray(extraArgs);
    return proc.start(args);
#endif
}

} // namespace PythonBridgeDetail

struct TrainingLogEvent {
    enum class Kind {
        TrainingMetrics,   // train.py: {"type":"epoch", ...}
        PrepProgress,      // preprocess.py: {"type":"progress", ...}  或旧 {"prep":true,...}
        PrepSummary,       // preprocess.py: {"type":"summary", ...}
        ScriptDone,        // 任意脚本: {"type":"done", ...}
        RawText            // 其他纯文本或未识别行
    };

    Kind         kind = Kind::RawText;

    // ── TrainingMetrics 字段 ─────────────────────────────────────────────────
    int          epoch   = 0;
    int          total   = 0;     // 总 epoch 数（train.py 新格式输出）
    float        loss    = 0.f;
    float        acc     = 0.f;
    float        valLoss = -1.f;
    float        valAcc  = -1.f;
    float        lr      = -1.f;

    // ── PrepProgress 字段 ────────────────────────────────────────────────────
    int          prepDone  = 0;
    int          prepTotal = 0;
    float        prepPct   = 0.f;  // 0-1 浮点进度
    juce::String prepStep;         // 当前流水线步骤名
    juce::String prepMsg;          // 可读进度描述

    // ── PrepSummary 字段 ─────────────────────────────────────────────────────
    int          summaryCount    = 0;
    int          summaryChannels = 0;
    int          summaryLabels   = 0;
    juce::String summaryPath;

    // ── ScriptDone 字段 ──────────────────────────────────────────────────────
    float        bestValAcc    = -1.f;
    juce::String doneModelPath;
    juce::String doneOutputPath;
    float        elapsedSec    = 0.f;

    // ── 通用 ──────────────────────────────────────────────────────────────────
    juce::String rawJson;          // 原始 JSON 行（调试 / RawText 消息体）
};

class PythonBridge : public juce::Thread
{
public:
    PythonBridge() : juce::Thread("PythonBridgeThread") {}
    
    ~PythonBridge() override 
    {
        stopTask();
    }

    // Pass a callback so UI can react (ensure thread-safety or use MessageManagerQueue!)
    void setLogCallback(std::function<void(const TrainingLogEvent&)> cb)
    {
        onLogReceived = cb;
    }

    bool launchTrainingTask(int epochs, int batchSize, float lr,
                             const juce::String& modelName,
                             const juce::String& dataPath = "",
                             const juce::String& savePath = "",
                             int numClasses = 4,
                             float manifestSampleRateHz = 256.f,
                             const juce::String& modelTemplate = "",
                             const juce::String& paradigm = "supervised",
                             const juce::String& backboneCkpt = "",
                             int freezeLayers = 0,
                             float validationSplit = 0.2f,
                             int randomSeed = 42)
    {
        juce::StringArray extra {
            "--epochs=" + juce::String(epochs),
            "--batch="  + juce::String(batchSize),
            "--lr="     + juce::String(lr),
            "--name="   + modelName
        };
        if (dataPath.isNotEmpty())
            extra.add("--data=" + dataPath);
        if (savePath.isNotEmpty())
        {
            extra.add("--save=" + savePath);
            juce::String pausePath = juce::File(savePath)
                .getChildFile(".nerou_train_pause").getFullPathName();
            extra.add("--pause-file=" + pausePath);
        }
        extra.add("--num_classes=" + juce::String(juce::jmax(1, numClasses)));
        if (manifestSampleRateHz > 0.f)
            extra.add("--sample-rate=" + juce::String(manifestSampleRateHz, 2));

        // 新增：模型模板
        extra.add("--val-split=" + juce::String(juce::jlimit(0.f, 0.9f, validationSplit), 3));
        extra.add("--seed=" + juce::String(juce::jmax(0, randomSeed)));

        if (modelTemplate.isNotEmpty())
            extra.add("--model-template=" + modelTemplate);

        // 新增：训练范式（supervised / finetune）
        if (paradigm.isNotEmpty() && paradigm != "supervised")
            extra.add("--paradigm=" + paradigm);

        // 新增：预训练骨干路径（finetune 时有效）
        if (backboneCkpt.isNotEmpty() && juce::File(backboneCkpt).existsAsFile())
            extra.add("--backbone-ckpt=" + backboneCkpt);

        // 新增：冻结层数
        if (freezeLayers != 0)
            extra.add("--freeze-layers=" + juce::String(freezeLayers));

        return launchScript("python_core/train.py", extra);
    }

    // ── Generic script launcher ───────────────────────────────────────────────
    // scriptRelPath: relative path from project root, e.g. "python_core/preprocess.py"
    // extraArgs: e.g. { "--input=data/raw", "--output=data/npz", "--sr=256" }
    bool launchScript(const juce::String& scriptRelPath,
                      const juce::StringArray& extraArgs = {})
    {
        if (childProcess.isRunning()) return false;

        runResultPending.store(false);
        userRequestedStop.store(false);

        // Walk up from executable to find the project root
        juce::File exeFile   = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        juce::File searchDir = exeFile.getParentDirectory();
        juce::File pyScript;

        for (int i = 0; i < 8; ++i)
        {
            juce::File candidate = searchDir.getChildFile(scriptRelPath);
            if (candidate.existsAsFile()) { pyScript = candidate; break; }
            juce::File parent = searchDir.getParentDirectory();
            if (parent == searchDir) break;
            searchDir = parent;
        }
        if (!pyScript.existsAsFile())
            pyScript = juce::File::getCurrentWorkingDirectory().getChildFile(scriptRelPath);
        if (!pyScript.existsAsFile())
        {
            juce::Logger::writeToLog("ERROR: Cannot find " + scriptRelPath);
            return false;
        }

#if JUCE_WINDOWS
        juce::String inner = "chcp 65001>nul && set PYTHONUTF8=1&& set PYTHONIOENCODING=utf-8&& python -X utf8 -u ";
        inner += PythonBridgeDetail::quoteCmdArg(pyScript.getFullPathName());
        for (auto& a : extraArgs)
            inner << " " << PythonBridgeDetail::quoteCmdArg(a);

        if (PythonBridgeDetail::startPythonViaCmd(childProcess, inner)) { startThread(); return true; }
        return false;
#else
        juce::StringArray args;
        args.add("python");
        args.add("-Xutf8");
        args.add("-u");
        args.add(pyScript.getFullPathName());
        args.addArray(extraArgs);

        if (childProcess.start(args)) { startThread(); return true; }
        return false;
#endif
    }

    void stopTask()
    {
        signalThreadShouldExit();
        if (childProcess.isRunning())
        {
            userRequestedStop.store(true);
            childProcess.kill();
        }
        stopThread(3000);
    }

    bool isProcessRunning() const
    {
        return childProcess.isRunning();
    }

    /** After a started child has fully shut down, the reader thread posts one result here. */
    bool tryConsumeRunResult(juce::uint32& exitCode, bool& userCancelled)
    {
        if (!runResultPending.load())
            return false;
        exitCode       = lastExitCode.load();
        userCancelled  = lastRunUserCancelled.load();
        runResultPending.store(false);
        return true;
    }

    // ── Synchronous quick-run (max 5 s) — for environment probing ─────────────
    // Returns stdout content of the process, or empty string on failure/timeout.
    static juce::String quickRun(const juce::StringArray& args, int timeoutMs = 5000)
    {
        juce::ChildProcess proc;
#if JUCE_WINDOWS
        juce::String inner = "chcp 65001>nul && set PYTHONUTF8=1&& set PYTHONIOENCODING=utf-8&& ";
        for (int i = 0; i < args.size(); ++i)
        {
            if (i > 0) inner << " ";
            inner << PythonBridgeDetail::quoteCmdArg(args[i]);
        }
        if (!PythonBridgeDetail::startPythonViaCmd(proc, inner)) return {};
#else
        if (!proc.start(args)) return {};
#endif

        std::vector<char> all;
        char buf[512];
        int waited = 0;
        while (proc.isRunning() && waited < timeoutMs)
        {
            const int n = proc.readProcessOutput(buf, (int)sizeof(buf));
            if (n > 0)
                all.insert(all.end(), buf, buf + n);
            else
            {
                juce::Thread::sleep(50);
                waited += 50;
            }
        }
        proc.waitForProcessToFinish(juce::jmax(100, timeoutMs));
        for (;;)
        {
            const int n = proc.readProcessOutput(buf, (int)sizeof(buf));
            if (n <= 0) break;
            all.insert(all.end(), buf, buf + n);
        }
        if (proc.isRunning())
            proc.kill();

        return PythonBridgeDetail::decodeChildText(all.data(), (int) all.size());
    }

    // ── Python environment check ──────────────────────────────────────────────
    // Probes Python + required packages asynchronously via the log callback.
    // Returns immediately; results arrive via onLogReceived as raw-text events.
    bool launchEnvCheck()
    {
        return launchScript("python_core/env_probe.py", {});
    }

    void run() override
    {
        pipeUtf8Buffer.clear();
        char buffer[1024];

        while (!threadShouldExit() && childProcess.isRunning())
        {
            const int bytesRead = childProcess.readProcessOutput(buffer, (int)sizeof(buffer));
            if (bytesRead > 0)
                appendStdoutBytes(buffer, bytesRead);
            else
                juce::Thread::sleep(10);
        }

        for (;;)
        {
            const int bytesRead = childProcess.readProcessOutput(buffer, (int)sizeof(buffer));
            if (bytesRead <= 0)
                break;
            appendStdoutBytes(buffer, bytesRead);
        }
        flushRemainingPipeUtf8();

        childProcess.waitForProcessToFinish(-1);

        const bool cancelled = userRequestedStop.load();
        userRequestedStop.store(false);
        lastRunUserCancelled.store(cancelled);
        lastExitCode.store(childProcess.getExitCode());
        runResultPending.store(true);
    }

private:
    void appendStdoutBytes(const char* data, int byteCount)
    {
        if (byteCount <= 0 || data == nullptr)
            return;

        if (pipeUtf8Buffer.empty() && byteCount >= 3 && (unsigned char)data[0] == 0xef && (unsigned char)data[1] == 0xbb
            && (unsigned char)data[2] == 0xbf)
        {
            data += 3;
            byteCount -= 3;
            if (byteCount <= 0)
                return;
        }

        pipeUtf8Buffer.insert(pipeUtf8Buffer.end(), data, data + byteCount);

        // Enterprise safeguard: cap buffer at 4MB to prevent unbounded memory growth
        // from malformed Python output lacking newlines
        static constexpr size_t kMaxPipeBufferBytes = 4 * 1024 * 1024;
        if (pipeUtf8Buffer.size() > kMaxPipeBufferBytes)
        {
            juce::Logger::writeToLog(
                juce::String::fromUTF8(u8"[PythonBridge] \u7ba1\u9053\u7f13\u51b2\u533a\u8d85\u8fc7 4MB \u9650\u5236\uff0c\u622a\u65ad\u5e76\u91ca\u653e"));
            pipeUtf8Buffer.clear();
            return;
        }

        size_t scan = 0;
        while (scan < pipeUtf8Buffer.size())
        {
            const unsigned char c = (unsigned char)pipeUtf8Buffer[scan];
            if (c == '\n')
            {
                emitUtf8Line(pipeUtf8Buffer.data(), scan);
                pipeUtf8Buffer.erase(pipeUtf8Buffer.begin(),
                                     pipeUtf8Buffer.begin() + (std::ptrdiff_t)scan + 1);
                scan = 0;
                continue;
            }
            if (c == '\r' && scan + 1 < pipeUtf8Buffer.size()
                && pipeUtf8Buffer[scan + 1] == '\n')
            {
                emitUtf8Line(pipeUtf8Buffer.data(), scan);
                pipeUtf8Buffer.erase(pipeUtf8Buffer.begin(),
                                     pipeUtf8Buffer.begin() + (std::ptrdiff_t)scan + 2);
                scan = 0;
                continue;
            }
            ++scan;
        }
    }

    void flushRemainingPipeUtf8()
    {
        if (pipeUtf8Buffer.empty())
            return;
        emitUtf8Line(pipeUtf8Buffer.data(), pipeUtf8Buffer.size());
        pipeUtf8Buffer.clear();
    }

    void emitUtf8Line(const char* ptr, size_t byteLen)
    {
        while (byteLen > 0 && (ptr[byteLen - 1] == '\r' || ptr[byteLen - 1] == '\n'))
            --byteLen;
        if (byteLen == 0)
            return;
        // 行首 UTF-8 BOM（部分 Python 输出）会导致日志首字符显示为占位符
        if (byteLen >= 3 && (unsigned char) ptr[0] == 0xef && (unsigned char) ptr[1] == 0xbb
            && (unsigned char) ptr[2] == 0xbf)
        {
            ptr += 3;
            byteLen -= 3;
            if (byteLen == 0)
                return;
        }
        juce::String line = PythonBridgeDetail::decodeChildText(ptr, (int) byteLen);
        if (line.isNotEmpty())
            parseAndBroadcast(line);
    }

    void parseAndBroadcast(const juce::String& line)
    {
        auto jsonResult = juce::JSON::parse(line);
        if (!jsonResult.isObject())
        {
            if (onLogReceived)
            {
                TrainingLogEvent ev;
                ev.kind    = TrainingLogEvent::Kind::RawText;
                ev.rawJson = line;
                onLogReceived(ev);
            }
            return;
        }

        auto* obj = jsonResult.getDynamicObject();
        TrainingLogEvent ev;
        ev.rawJson = line;

        // ── 新协议：优先按 "type" 字段路由 ─────────────────────────────────
        juce::String typeStr = obj->getProperty("type").toString();

        if (typeStr == "epoch")
        {
            // {"type":"epoch","epoch":1,"total":100,"loss":0.892,"acc":0.412,
            //  "val_loss":0.901,"val_acc":0.398,"lr":0.0005}
            ev.kind  = TrainingLogEvent::Kind::TrainingMetrics;
            ev.epoch = (int)   obj->getProperty("epoch").operator int();
            ev.total = (int)   obj->getProperty("total").operator int();
            ev.loss  = (float) obj->getProperty("loss").operator double();
            ev.acc   = (float) obj->getProperty("acc").operator double();
            if (obj->hasProperty("val_loss"))
                ev.valLoss = (float) obj->getProperty("val_loss").operator double();
            if (obj->hasProperty("val_acc"))
                ev.valAcc  = (float) obj->getProperty("val_acc").operator double();
            if (obj->hasProperty("lr"))
                ev.lr = (float) obj->getProperty("lr").operator double();
            if (onLogReceived) onLogReceived(ev);
            return;
        }

        if (typeStr == "progress")
        {
            // {"type":"progress","step":"resample","pct":0.20,"msg":"...",
            //  "prep":true,"done":3,"total":10}
            ev.kind      = TrainingLogEvent::Kind::PrepProgress;
            ev.prepStep  = obj->getProperty("step").toString();
            ev.prepPct   = (float) obj->getProperty("pct").operator double();
            ev.prepDone  = (int)   obj->getProperty("done").operator int();
            ev.prepTotal = (int)   obj->getProperty("total").operator int();
            ev.prepMsg   = obj->getProperty("msg").toString();
            if (onLogReceived) onLogReceived(ev);
            return;
        }

        if (typeStr == "summary")
        {
            // {"type":"summary","sample_count":2400,"channel_count":22,"label_count":4,
            //  "output_path":"...","elapsed_sec":12.3}
            ev.kind           = TrainingLogEvent::Kind::PrepSummary;
            ev.summaryCount   = (int) obj->getProperty("sample_count").operator int();
            ev.summaryChannels= (int) obj->getProperty("channel_count").operator int();
            ev.summaryLabels  = (int) obj->getProperty("label_count").operator int();
            ev.summaryPath    = obj->getProperty("output_path").toString();
            ev.elapsedSec     = (float) obj->getProperty("elapsed_sec").operator double();
            if (onLogReceived) onLogReceived(ev);
            return;
        }

        if (typeStr == "done")
        {
            // train.py: {"type":"done","best_val_acc":0.847,"model_path":"...","elapsed_sec":185}
            // preprocess.py: {"type":"done","output_path":"...","elapsed_sec":12.3}
            ev.kind       = TrainingLogEvent::Kind::ScriptDone;
            ev.bestValAcc = (float) obj->getProperty("best_val_acc").operator double();
            ev.doneModelPath = obj->getProperty("model_path").toString();
            ev.doneOutputPath= obj->getProperty("output_path").toString();
            ev.elapsedSec = (float) obj->getProperty("elapsed_sec").operator double();
            if (onLogReceived) onLogReceived(ev);
            return;
        }

        if (typeStr == "log" || typeStr == "error")
        {
            ev.kind    = TrainingLogEvent::Kind::RawText;
            ev.rawJson = obj->getProperty("msg").toString();
            if (ev.rawJson.isEmpty()) ev.rawJson = line;
            if (onLogReceived) onLogReceived(ev);
            return;
        }

        // ── 旧协议兼容：没有 type 字段时按旧规则解析 ────────────────────────

        juce::var prepVar = obj->getProperty("prep");
        if (prepVar.isBool() && (bool) prepVar)
        {
            ev.kind      = TrainingLogEvent::Kind::PrepProgress;
            ev.prepDone  = (int) obj->getProperty("done");
            ev.prepTotal = (int) obj->getProperty("total");
            ev.prepMsg   = obj->getProperty("msg").toString();
            if (onLogReceived) onLogReceived(ev);
            return;
        }

        // 旧 epoch 行：{"epoch":1,"loss":0.8,"acc":0.4, ...}，epoch > 0 才是训练指标
        if (obj->hasProperty("epoch") && obj->hasProperty("loss") && obj->hasProperty("acc"))
        {
            int ep = (int) obj->getProperty("epoch").operator int();
            if (ep > 0)
            {
                ev.kind  = TrainingLogEvent::Kind::TrainingMetrics;
                ev.epoch = ep;
                ev.loss  = (float) obj->getProperty("loss").operator double();
                ev.acc   = (float) obj->getProperty("acc").operator double();
                if (obj->hasProperty("val_loss"))
                    ev.valLoss = (float) obj->getProperty("val_loss").operator double();
                if (obj->hasProperty("val_acc"))
                    ev.valAcc  = (float) obj->getProperty("val_acc").operator double();
                if (obj->hasProperty("lr"))
                    ev.lr = (float) obj->getProperty("lr").operator double();
                if (onLogReceived) onLogReceived(ev);
                return;
            }
        }

        // 其余当 RawText 处理（epoch=-1 日志行、msg 行等）
        ev.kind    = TrainingLogEvent::Kind::RawText;
        juce::String msgProp = obj->getProperty("msg").toString();
        ev.rawJson = msgProp.isNotEmpty() ? msgProp : line;
        if (onLogReceived) onLogReceived(ev);
    }

    juce::ChildProcess childProcess;
    std::function<void(const TrainingLogEvent&)> onLogReceived;

    std::atomic<bool>          userRequestedStop   { false };
    std::atomic<bool>          runResultPending   { false };
    std::atomic<bool>          lastRunUserCancelled { false };
    std::atomic<juce::uint32>  lastExitCode       { 0 };

    std::vector<char> pipeUtf8Buffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PythonBridge)
};
