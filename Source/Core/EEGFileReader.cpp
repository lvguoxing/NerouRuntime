#include "EEGFileReader.h"
#include "NpzEEGLoader.h"
#include "Utf8Literals.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <limits>
#include <map>
#include <mutex>

namespace nerou::core {

namespace {

struct ReaderEntry
{
    EEGFileReader::ReaderFn loadFn;
    EEGFileReader::ProbeFn  probeFn;
};

std::map<juce::String, ReaderEntry>& registry()
{
    static std::map<juce::String, ReaderEntry> r;
    return r;
}

std::mutex& registryMutex()
{
    static std::mutex m;
    return m;
}

juce::String fileExt(const juce::File& f)
{
    return f.getFileExtension().trimCharactersAtStart(".").toLowerCase();
}

/** 扫描 NaN/Inf 与全零通道，可选地把异常值就地替换为 0。 */
void diagnoseAndScrub(domain::EEGRecord& rec, bool replaceWithZero)
{
    int nans = 0;
    int infs = 0;
    for (auto& v : rec.data)
    {
        if (std::isnan(v))
        {
            ++nans;
            if (replaceWithZero) v = 0.0f;
        }
        else if (std::isinf(v))
        {
            ++infs;
            if (replaceWithZero) v = 0.0f;
        }
    }
    rec.nanCount = nans;
    rec.infCount = infs;

    // 平直通道检测（每个通道是否完全相同）
    if (rec.layout == domain::EEGDataLayout::ChannelMajor)
    {
        const int C = rec.channelCount;
        const int T = rec.totalFrames;
        for (int c = 0; c < C; ++c)
        {
            const float* row = rec.data.data() + (size_t) c * (size_t) T;
            const float v0 = row[0];
            bool flat = true;
            for (int t = 1; t < T; ++t)
            {
                if (row[t] != v0) { flat = false; break; }
            }
            if (flat) { rec.hasFlatChannels = true; break; }
        }
    }
}

// ============================================================
// 内置 NPZ / NPY reader（复用 NpzEEGLoader）
// ============================================================

EEGLoadResult loadNpzInternal(const juce::File& file, const EEGLoadOptions& opts)
{
    EEGLoadResult r;
    r.record.sourcePath   = file;
    r.record.sourceFormat = file.hasFileExtension("npy") ? "npy" : "npz";
    r.record.recordedAt   = file.getLastModificationTime();
    r.record.units        = "uV";

    auto series = NpzEEGLoader::loadPlaybackSeries(file);
    if (series.error.isNotEmpty())
    {
        r.errorCode    = "ERR_FORMAT_PARSE";
        r.errorMessage = series.error;
        return r;
    }
    if (series.channels <= 0 || series.totalFrames <= 0)
    {
        r.errorCode    = "ERR_FORMAT_EMPTY";
        r.errorMessage = NR_STR("\u6587\u4ef6\u4e2d\u672a\u627e\u5230\u6709\u6548\u6570\u636e");
        return r;
    }

    r.record.channelCount = series.channels;
    r.record.totalFrames  = series.totalFrames;
    // 项目原生 NPZ 不内嵌采样率，留 0 由调用方按当前会话上下文补
    r.record.sampleRate   = 0.0;

    if (opts.layout == domain::EEGDataLayout::TimeMajor)
    {
        r.record.data   = std::move(series.timeMajor);
        r.record.layout = domain::EEGDataLayout::TimeMajor;
    }
    else
    {
        const int C = series.channels;
        const int T = series.totalFrames;
        r.record.data.resize((size_t) C * (size_t) T);
        for (int t = 0; t < T; ++t)
        {
            const size_t srcRow = (size_t) t * (size_t) C;
            for (int c = 0; c < C; ++c)
                r.record.data[(size_t) c * (size_t) T + (size_t) t] =
                    series.timeMajor[srcRow + (size_t) c];
        }
        r.record.layout = domain::EEGDataLayout::ChannelMajor;
    }

    diagnoseAndScrub(r.record, opts.replaceNanWithZero);

    if (r.record.nanCount > 0)
        r.warnings.add(NR_STR("\u68c0\u6d4b\u5230 ")
                       + juce::String(r.record.nanCount)
                       + NR_STR(" \u4e2a NaN")
                       + (opts.replaceNanWithZero
                              ? NR_STR("\uff0c\u5df2\u66ff\u6362\u4e3a 0")
                              : juce::String()));
    if (r.record.infCount > 0)
        r.warnings.add(NR_STR("\u68c0\u6d4b\u5230 ")
                       + juce::String(r.record.infCount)
                       + NR_STR(" \u4e2a Inf"));
    if (r.record.hasFlatChannels)
        r.warnings.add(NR_STR("\u5b58\u5728\u5e73\u76f4\u901a\u9053\uff08\u6570\u503c\u4e0d\u53d8\uff09\uff0c\u53ef\u80fd\u662f\u9519\u8bef\u7535\u6781\u63a5\u89e6"));

    r.ok = true;
    return r;
}

EEGLoadResult probeNpzInternal(const juce::File& file)
{
    EEGLoadResult r;
    r.record.sourcePath   = file;
    r.record.sourceFormat = file.hasFileExtension("npy") ? "npy" : "npz";
    r.record.recordedAt   = file.getLastModificationTime();
    r.record.units        = "uV";

    // probe 仅取第 0 个 sample 的形状，避免展开整个 N×C×T
    auto window = NpzEEGLoader::loadWindow(file, 0);
    if (window.error.isNotEmpty())
    {
        r.errorCode    = "ERR_FORMAT_PARSE";
        r.errorMessage = window.error;
        return r;
    }
    r.record.channelCount = window.channels;
    r.record.totalFrames  = window.timePoints;
    r.record.sampleRate   = 0.0;
    r.ok = true;
    return r;
}

// ============================================================
// 内置 CSV / TSV reader（C++ 直读，零外部依赖）
//
// 设计：
//   • 默认布局 = Time-Major（行=时间，列=通道）—— 90% 第三方采集器导出格式
//   • 分隔符自检：首行 `,` / `\t` / `;` 最多者
//   • 首行非数字 → 视为表头，提取通道名
//   • 注释行：以 `#` 或 `%` 开头跳过
//   • 空字段 / "NaN" / "nan" / "inf" → 转 NaN/Inf，由 diagnoseAndScrub 后处理
//   • 行长不一致 → 跳过该行 + 累计 warning
// ============================================================

juce::juce_wchar detectCsvDelimiter(const juce::String& firstLine)
{
    const int nComma = firstLine.toStdString().size() == 0
                           ? 0
                           : (int) std::count(firstLine.toRawUTF8(),
                                              firstLine.toRawUTF8() + firstLine.getNumBytesAsUTF8(), ',');
    const int nTab   = (int) std::count(firstLine.toRawUTF8(),
                                        firstLine.toRawUTF8() + firstLine.getNumBytesAsUTF8(), '\t');
    const int nSemi  = (int) std::count(firstLine.toRawUTF8(),
                                        firstLine.toRawUTF8() + firstLine.getNumBytesAsUTF8(), ';');
    if (nTab >= nComma && nTab >= nSemi && nTab > 0)  return '\t';
    if (nSemi > nComma && nSemi > 0)                  return ';';
    return ',';
}

bool tokenLooksLikeNumber(const juce::String& tok)
{
    auto t = tok.trim();
    if (t.isEmpty()) return true;                                  // 空 = 后续转 NaN，不算"非数字"

    auto lower = t.toLowerCase();
    if (lower == "nan" || lower == "inf" || lower == "-inf" || lower == "+inf")
        return true;

    return t.containsOnly("0123456789-+.eE ");
}

float parseCsvFloat(const juce::String& tok)
{
    auto t = tok.trim();
    if (t.isEmpty()) return std::numeric_limits<float>::quiet_NaN();

    auto lower = t.toLowerCase();
    if (lower == "nan")                              return std::numeric_limits<float>::quiet_NaN();
    if (lower == "inf"  || lower == "+inf")          return std::numeric_limits<float>::infinity();
    if (lower == "-inf")                             return -std::numeric_limits<float>::infinity();

    return (float) t.getDoubleValue();
}

EEGLoadResult loadCsvInternal(const juce::File& file, const EEGLoadOptions& opts)
{
    EEGLoadResult r;
    r.record.sourcePath   = file;
    r.record.sourceFormat = file.hasFileExtension("tsv") ? "tsv" : "csv";
    r.record.recordedAt   = file.getLastModificationTime();
    r.record.units        = "uV";

    const juce::String text = file.loadFileAsString();
    if (text.isEmpty())
    {
        r.errorCode    = "ERR_FORMAT_EMPTY";
        r.errorMessage = NR_STR("\u6587\u4ef6\u4e3a\u7a7a\u6216\u8bfb\u53d6\u5931\u8d25");
        return r;
    }

    auto lines = juce::StringArray::fromLines(text);

    // 跳过开头注释 + 空行，找到第一条候选数据行
    int firstNonEmpty = -1;
    for (int i = 0; i < lines.size(); ++i)
    {
        auto t = lines[i].trim();
        if (t.isEmpty()) continue;
        if (t.startsWith("#") || t.startsWith("%")) continue;
        firstNonEmpty = i;
        break;
    }
    if (firstNonEmpty < 0)
    {
        r.errorCode    = "ERR_FORMAT_EMPTY";
        r.errorMessage = NR_STR("\u6587\u4ef6\u5168\u662f\u7a7a\u884c\u6216\u6ce8\u91ca");
        return r;
    }

    const auto delim = file.hasFileExtension("tsv")
                            ? juce::juce_wchar('\t')
                            : detectCsvDelimiter(lines[firstNonEmpty]);
    const juce::String delimStr = juce::String::charToString(delim);

    // 判定首行是否表头
    juce::StringArray firstTokens;
    firstTokens.addTokens(lines[firstNonEmpty], delimStr, "");
    bool hasHeader = false;
    for (const auto& tok : firstTokens)
    {
        if (!tokenLooksLikeNumber(tok)) { hasHeader = true; break; }
    }

    int dataStart = firstNonEmpty;
    if (hasHeader)
    {
        for (auto& tok : firstTokens)
            r.record.channelNames.add(tok.trim());
        dataStart = firstNonEmpty + 1;
    }

    // 探测列数 = 首条数据行的非空字段数
    int firstDataIx = -1;
    for (int i = dataStart; i < lines.size(); ++i)
    {
        auto t = lines[i].trim();
        if (t.isEmpty()) continue;
        if (t.startsWith("#") || t.startsWith("%")) continue;
        firstDataIx = i;
        break;
    }
    if (firstDataIx < 0)
    {
        r.errorCode    = "ERR_FORMAT_EMPTY";
        r.errorMessage = NR_STR("\u6587\u4ef6\u53ea\u6709\u8868\u5934\u6ca1\u6709\u6570\u636e");
        return r;
    }

    juce::StringArray firstDataTokens;
    firstDataTokens.addTokens(lines[firstDataIx], delimStr, "");
    const int C = firstDataTokens.size();
    if (C <= 0)
    {
        r.errorCode    = "ERR_FORMAT_PARSE";
        r.errorMessage = NR_STR("\u65e0\u6cd5\u63a8\u65ad\u901a\u9053\u6570");
        return r;
    }

    if (hasHeader && r.record.channelNames.size() != C)
    {
        // 表头列数与数据列数不一致 —— 抛弃表头让消费方走默认数字编号
        r.warnings.add(NR_STR("\u8868\u5934\u5217\u6570\u4e0e\u6570\u636e\u5217\u6570\u4e0d\u4e00\u81f4\uff0c\u5df2\u5ffd\u7565\u8868\u5934"));
        r.record.channelNames.clear();
    }

    // 根据 maxChannels 截断（用于异常 CSV 的硬保护）
    const int Cuse = (opts.maxChannels > 0 && opts.maxChannels < C) ? opts.maxChannels : C;
    if (Cuse < C)
        r.warnings.add(NR_STR("\u5df2\u622a\u65ad\u81f3 ") + juce::String(Cuse) + NR_STR(" \u4e2a\u901a\u9053"));

    // 主循环：累积所有数据行（time-major 缓冲）
    std::vector<float> timeMajor;                         // [t*Cuse + c]
    timeMajor.reserve((size_t) Cuse * 1024);
    int T            = 0;
    int skippedRows  = 0;

    for (int i = firstDataIx; i < lines.size(); ++i)
    {
        auto raw = lines[i];
        auto t = raw.trim();
        if (t.isEmpty()) continue;
        if (t.startsWith("#") || t.startsWith("%")) continue;

        juce::StringArray tokens;
        tokens.addTokens(raw, delimStr, "");
        if (tokens.size() < C)
        {
            ++skippedRows;
            continue;
        }
        for (int c = 0; c < Cuse; ++c)
            timeMajor.push_back(parseCsvFloat(tokens[c]));
        ++T;
    }

    if (T <= 0)
    {
        r.errorCode    = "ERR_FORMAT_EMPTY";
        r.errorMessage = NR_STR("\u672a\u8bfb\u5230\u6709\u6548\u6570\u636e\u884c");
        return r;
    }

    if (skippedRows > 0)
        r.warnings.add(NR_STR("\u8df3\u8fc7\u4e86 ")
                       + juce::String(skippedRows)
                       + NR_STR(" \u884c\u4e0d\u4e00\u81f4\u957f\u5ea6\u7684\u6570\u636e"));

    r.record.channelCount = Cuse;
    r.record.totalFrames  = T;
    r.record.sampleRate   = 0.0;                          // CSV 不携带采样率，由调用方补充

    // 按 opts.layout 写入数据
    if (opts.layout == domain::EEGDataLayout::TimeMajor)
    {
        r.record.data   = std::move(timeMajor);
        r.record.layout = domain::EEGDataLayout::TimeMajor;
    }
    else
    {
        r.record.data.resize((size_t) Cuse * (size_t) T);
        for (int t = 0; t < T; ++t)
            for (int c = 0; c < Cuse; ++c)
                r.record.data[(size_t) c * (size_t) T + (size_t) t] =
                    timeMajor[(size_t) t * (size_t) Cuse + (size_t) c];
        r.record.layout = domain::EEGDataLayout::ChannelMajor;
    }

    diagnoseAndScrub(r.record, opts.replaceNanWithZero);

    if (r.record.nanCount > 0)
        r.warnings.add(NR_STR("\u68c0\u6d4b\u5230 ")
                       + juce::String(r.record.nanCount)
                       + NR_STR(" \u4e2a NaN")
                       + (opts.replaceNanWithZero
                              ? NR_STR("\uff0c\u5df2\u66ff\u6362\u4e3a 0")
                              : juce::String()));
    if (r.record.infCount > 0)
        r.warnings.add(NR_STR("\u68c0\u6d4b\u5230 ")
                       + juce::String(r.record.infCount)
                       + NR_STR(" \u4e2a Inf"));
    if (r.record.hasFlatChannels)
        r.warnings.add(NR_STR("\u5b58\u5728\u5e73\u76f4\u901a\u9053\uff08\u6570\u503c\u4e0d\u53d8\uff09\uff0c\u53ef\u80fd\u662f\u5e38\u91cf\u5217\u6216\u9519\u8bef\u7535\u6781\u63a5\u89e6"));

    r.ok = true;
    return r;
}

EEGLoadResult probeCsvInternal(const juce::File& file)
{
    // probe 只读前 ~64KB 估算列数与时长
    EEGLoadResult r;
    r.record.sourcePath   = file;
    r.record.sourceFormat = file.hasFileExtension("tsv") ? "tsv" : "csv";
    r.record.recordedAt   = file.getLastModificationTime();
    r.record.units        = "uV";

    juce::FileInputStream fis(file);
    if (!fis.openedOk())
    {
        r.errorCode    = "ERR_FILE_NOT_FOUND";
        r.errorMessage = NR_STR("\u65e0\u6cd5\u6253\u5f00\u6587\u4ef6");
        return r;
    }

    const juce::int64 totalSize = file.getSize();
    const juce::int64 sampleSize = juce::jmin<juce::int64>(totalSize, 64 * 1024);
    juce::MemoryBlock mb;
    fis.readIntoMemoryBlock(mb, (ssize_t) sampleSize);
    juce::String head = mb.toString();

    auto lines = juce::StringArray::fromLines(head);
    int firstNonEmpty = -1;
    for (int i = 0; i < lines.size(); ++i)
    {
        auto t = lines[i].trim();
        if (t.isEmpty() || t.startsWith("#") || t.startsWith("%")) continue;
        firstNonEmpty = i;
        break;
    }
    if (firstNonEmpty < 0)
    {
        r.errorCode    = "ERR_FORMAT_EMPTY";
        r.errorMessage = NR_STR("\u6587\u4ef6\u5168\u662f\u7a7a\u884c\u6216\u6ce8\u91ca");
        return r;
    }

    const auto delim = file.hasFileExtension("tsv")
                            ? juce::juce_wchar('\t')
                            : detectCsvDelimiter(lines[firstNonEmpty]);
    const juce::String delimStr = juce::String::charToString(delim);

    juce::StringArray firstTokens;
    firstTokens.addTokens(lines[firstNonEmpty], delimStr, "");
    bool hasHeader = false;
    for (const auto& tok : firstTokens)
        if (!tokenLooksLikeNumber(tok)) { hasHeader = true; break; }

    int firstDataIx = hasHeader ? firstNonEmpty + 1 : firstNonEmpty;
    while (firstDataIx < lines.size())
    {
        auto t = lines[firstDataIx].trim();
        if (!t.isEmpty() && !t.startsWith("#") && !t.startsWith("%")) break;
        ++firstDataIx;
    }
    if (firstDataIx >= lines.size())
    {
        r.errorCode    = "ERR_FORMAT_EMPTY";
        r.errorMessage = NR_STR("\u672a\u53d1\u73b0\u6570\u636e\u884c");
        return r;
    }

    juce::StringArray firstDataTokens;
    firstDataTokens.addTokens(lines[firstDataIx], delimStr, "");
    const int C = firstDataTokens.size();
    r.record.channelCount = C;

    if (hasHeader && firstTokens.size() == C)
        r.record.channelNames = firstTokens;

    // 估算总帧数：(总文件大小 / 平均行字节数) ≈ T
    if (totalSize > 0 && lines.size() > firstDataIx + 1)
    {
        const auto avgLineBytes = juce::jmax<juce::int64>(
            1, (juce::int64) head.getNumBytesAsUTF8() / juce::jmax(1, lines.size() - firstDataIx));
        r.record.totalFrames = (int) juce::jmin<juce::int64>(
            INT_MAX, (totalSize / avgLineBytes));
    }

    r.record.sampleRate = 0.0;
    r.ok = true;
    return r;
}

void ensureBuiltinReaders()
{
    static std::once_flag once;
    std::call_once(once, [] {
        std::lock_guard<std::mutex> lk(registryMutex());
        registry()["npz"] = { &loadNpzInternal, &probeNpzInternal };
        registry()["npy"] = { &loadNpzInternal, &probeNpzInternal };
        registry()["csv"] = { &loadCsvInternal, &probeCsvInternal };
        registry()["tsv"] = { &loadCsvInternal, &probeCsvInternal };
    });
}

} // namespace

// ============================================================
// 公开 API
// ============================================================

EEGLoadResult EEGFileReader::load(const juce::File& file, const EEGLoadOptions& opts)
{
    EEGLoadResult r;
    if (!file.existsAsFile())
    {
        r.errorCode    = "ERR_FILE_NOT_FOUND";
        r.errorMessage = NR_STR("\u6587\u4ef6\u4e0d\u5b58\u5728: ") + file.getFullPathName();
        return r;
    }

    ensureBuiltinReaders();
    const auto ext = fileExt(file);

    ReaderFn fn;
    {
        std::lock_guard<std::mutex> lk(registryMutex());
        auto it = registry().find(ext);
        if (it == registry().end() || !it->second.loadFn)
        {
            r.errorCode    = "ERR_FORMAT_UNSUPPORTED";
            r.errorMessage = NR_STR("\u4e0d\u652f\u6301\u7684\u683c\u5f0f: ") + ext;
            return r;
        }
        fn = it->second.loadFn;
    }
    return fn(file, opts);
}

EEGLoadResult EEGFileReader::probe(const juce::File& file)
{
    EEGLoadResult r;
    if (!file.existsAsFile())
    {
        r.errorCode    = "ERR_FILE_NOT_FOUND";
        r.errorMessage = NR_STR("\u6587\u4ef6\u4e0d\u5b58\u5728: ") + file.getFullPathName();
        return r;
    }

    ensureBuiltinReaders();
    const auto ext = fileExt(file);

    ProbeFn fn;
    {
        std::lock_guard<std::mutex> lk(registryMutex());
        auto it = registry().find(ext);
        if (it == registry().end() || !it->second.probeFn)
        {
            r.errorCode    = "ERR_FORMAT_UNSUPPORTED";
            r.errorMessage = NR_STR("\u4e0d\u652f\u6301\u7684\u683c\u5f0f: ") + ext;
            return r;
        }
        fn = it->second.probeFn;
    }
    return fn(file);
}

juce::String EEGFileReader::getSupportedExtensionsFilter()
{
    ensureBuiltinReaders();
    juce::StringArray exts;
    {
        std::lock_guard<std::mutex> lk(registryMutex());
        for (const auto& kv : registry())
            exts.add("*." + kv.first);
    }
    return exts.joinIntoString(";");
}

juce::StringArray EEGFileReader::getSupportedExtensionsList()
{
    ensureBuiltinReaders();
    juce::StringArray exts;
    {
        std::lock_guard<std::mutex> lk(registryMutex());
        for (const auto& kv : registry())
            exts.add(kv.first);
    }
    return exts;
}

void EEGFileReader::registerReader(const juce::String& extLower,
                                    ReaderFn            loadFn,
                                    ProbeFn             probeFn)
{
    std::lock_guard<std::mutex> lk(registryMutex());
    registry()[extLower] = { std::move(loadFn), std::move(probeFn) };
}

} // namespace nerou::core
