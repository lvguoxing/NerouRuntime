#include "NpzEEGLoader.h"
#include "Utf8Literals.h"
#include <cstring>
#include <numeric>

namespace {
bool readLeU16(const uint8_t* p, uint16_t& out)
{
    out = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    return true;
}

bool readLeU32(const uint8_t* p, uint32_t& out)
{
    out = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    return true;
}

bool parseNpyHeader(const uint8_t* base, size_t len, size_t& dataOffset,
                    std::vector<int64_t>& shape, bool& fortranOrder,
                    int& elemBytes, bool& isFloat, juce::String& err)
{
    static const uint8_t kMagic[] = { 0x93, 'N', 'U', 'M', 'P', 'Y' };
    if (len < 10 || std::memcmp(base, kMagic, 6) != 0)
    {
        err = NR_STR("非有效 .npy 文件头");
        return false;
    }

    const uint8_t major = base[6];
    const uint8_t minor = base[7];
    size_t headerLen = 0;
    size_t headerStart = 0;

    if (major == 1 && minor == 0)
    {
        uint16_t hl = 0;
        readLeU16(base + 8, hl);
        headerLen = hl;
        headerStart = 10;
    }
    else if (major == 2 && minor == 0)
    {
        if (len < 12)
        {
            err = NR_STR(".npy 头过短(v2)");
            return false;
        }
        uint32_t hl = 0;
        readLeU32(base + 8, hl);
        headerLen = hl;
        headerStart = 12;
    }
    else
    {
        err = NR_STR("不支持的 .npy 版本");
        return false;
    }

    if (headerStart + headerLen > len)
    {
        err = NR_STR(".npy 声明长度越界");
        return false;
    }

    juce::String headerStr(reinterpret_cast<const char*>(base + headerStart),
                           static_cast<int>(headerLen));

    // 'fortran_order': False
    fortranOrder = headerStr.containsIgnoreCase("'fortran_order': True");

    // 'descr': '<f4'
    int descrPos = headerStr.indexOfIgnoreCase(0, "'descr'");
    if (descrPos < 0)
        descrPos = headerStr.indexOfIgnoreCase(0, "\"descr\"");
    if (descrPos < 0)
    {
        err = NR_STR("缺少 descr");
        return false;
    }
    const int descrColon = headerStr.indexOfChar(descrPos, ':');
    int valQuote = headerStr.indexOfChar(descrColon + 1, '\'');
    if (valQuote < 0)
        valQuote = headerStr.indexOfChar(descrColon + 1, '"');
    if (valQuote < 0)
    {
        err = NR_STR("无法解析 descr 值");
        return false;
    }
    const char closeQ = headerStr[valQuote];
    int valEnd = headerStr.indexOfChar(valQuote + 1, closeQ);
    if (valEnd <= valQuote)
    {
        err = NR_STR("descr 引号不配对");
        return false;
    }
    juce::String descr = headerStr.substring(valQuote + 1, valEnd).trim();
    if (descr == "<f4" || descr == "|f4" || descr == "=f4" || descr.endsWith("f4"))
    {
        elemBytes = 4;
        isFloat = true;
    }
    else if (descr == "<f8" || descr == "|f8" || descr == "=f8" || descr.endsWith("f8"))
    {
        elemBytes = 8;
        isFloat = true;
    }
    else
    {
        err = NR_STR("仅支持 float32/f64，当前 descr=") + descr;
        return false;
    }

    int shPos = headerStr.indexOfIgnoreCase(0, "'shape'");
    if (shPos < 0)
        shPos = headerStr.indexOfIgnoreCase(0, "\"shape\"");
    if (shPos < 0)
    {
        err = NR_STR("缺少 shape");
        return false;
    }
    int lpar = headerStr.indexOfChar(shPos, '(');
    int rpar = headerStr.indexOfChar(lpar >= 0 ? lpar : 0, ')');
    if (lpar < 0 || rpar <= lpar)
    {
        err = NR_STR("shape 括号异常");
        return false;
    }
    juce::String inside = headerStr.substring(lpar + 1, rpar);
    inside = inside.replace("(", "").replace(")", "");
    juce::StringArray parts;
    parts.addTokens(inside, ",", "");
    shape.clear();
    for (auto& p : parts)
    {
        p = p.trim();
        if (p.isEmpty())
            continue;
        auto v = p.getLargeIntValue();
        if (v <= 0 && !p.startsWith("0"))
        {
            err = NR_STR("非法 shape 分量: ") + p;
            return false;
        }
        shape.push_back(v);
    }

    dataOffset = headerStart + headerLen;
    if (dataOffset > len)
    {
        err = NR_STR("数据偏移越界");
        return false;
    }
    return true;
}

bool extractWindow(const std::vector<int64_t>& sh, int sampleIndex,
                   const uint8_t* payload, size_t payloadBytes,
                   int elemBytes, bool fortranOrder,
                   NpzWindowResult& out, juce::String& err)
{
    if (fortranOrder)
    {
        err = NR_STR("不支持 Fortran 序数组");
        return false;
    }

    int64_t nChan = 0, nTime = 0, nBatch = 1;
    if (sh.size() == 2)
    {
        nChan = sh[0];
        nTime = sh[1];
        sampleIndex = 0;
    }
    else if (sh.size() == 3)
    {
        nBatch = sh[0];
        nChan = sh[1];
        nTime = sh[2];
        if (sampleIndex < 0 || sampleIndex >= nBatch)
        {
            err = juce::String::formatted(NR_STR("sampleIndex 超界: %d / %lld"), sampleIndex, (long long)nBatch);
            return false;
        }
    }
    else
    {
        err = juce::String::formatted(NR_STR("期望 2D (C,T) 或 3D (N,C,T)，当前维度=%d"), (int)sh.size());
        return false;
    }

    const int64_t stride = nChan * nTime;
    const size_t need = (size_t)stride * (size_t)elemBytes;
    const size_t offsetElems = (sh.size() == 3) ? (size_t)sampleIndex * (size_t)stride : 0;
    const size_t byteOff = offsetElems * (size_t)elemBytes;
    if (byteOff + need > payloadBytes)
    {
        err = NR_STR("数组体积与文件不符");
        return false;
    }

    out.channels = (int)nChan;
    out.timePoints = (int)nTime;
    out.usedSampleIndex = sampleIndex;
    out.flat.resize((size_t)stride);

    const uint8_t* p = payload + byteOff;
    if (elemBytes == 4)
    {
        std::memcpy(out.flat.data(), p, need);
    }
    else
    {
        const double* dp = reinterpret_cast<const double*>(p);
        for (int64_t i = 0; i < stride; ++i)
            out.flat[(size_t)i] = (float)dp[(size_t)i];
    }
    return true;
}

juce::String entryLeaf(const juce::String& zipPath)
{
    auto ix = zipPath.lastIndexOfChar('/');
    if (ix < 0)
        ix = zipPath.lastIndexOfChar('\\');
    return ix >= 0 ? zipPath.substring(ix + 1) : zipPath;
}
} // namespace

NpzWindowResult NpzEEGLoader::loadWindow(const juce::File& npzFile, int sampleIndex)
{
    NpzWindowResult result;
    if (!npzFile.existsAsFile())
    {
        result.error = NR_STR("文件不存在");
        return result;
    }

    if (npzFile.hasFileExtension("npy"))
    {
        juce::MemoryBlock mb;
        if (!npzFile.loadFileAsData(mb) || mb.getSize() < 20)
        {
            result.error = NR_STR(".npy 文件读取失败或内容过小");
            return result;
        }

        const uint8_t* bytes = static_cast<const uint8_t*>(mb.getData());
        size_t total = mb.getSize();
        size_t dataOff = 0;
        std::vector<int64_t> shape;
        bool fortran = false;
        int elemBytes = 0;
        bool isFloat = false;

        if (!parseNpyHeader(bytes, total, dataOff, shape, fortran, elemBytes, isFloat, result.error))
            return result;

        if (!isFloat)
        {
            result.error = NR_STR(".npy 仅支持浮点输入");
            return result;
        }

        if (!extractWindow(shape, sampleIndex, bytes + dataOff, total - dataOff,
                           elemBytes, fortran, result, result.error))
            return result;

        return result;
    }

    juce::ZipFile zip(npzFile);
    if (zip.getNumEntries() <= 0)
    {
        result.error = NR_STR("无法作为 ZIP 读取（非 NPZ）");
        return result;
    }

    int bestIndex = -1;
    juce::String targetLeaf = "data.npy";
    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        if (auto* e = zip.getEntry(i))
        {
            if (e->isSymbolicLink || e->filename.endsWithIgnoreCase("/"))
                continue;
            auto leaf = entryLeaf(e->filename);
            if (leaf.equalsIgnoreCase(targetLeaf))
            {
                bestIndex = i;
                break;
            }
            if (bestIndex < 0 && leaf.endsWithIgnoreCase(".npy"))
                bestIndex = i;
        }
    }

    if (bestIndex < 0)
    {
        result.error = NR_STR("NPZ 内未找到任何 .npy");
        return result;
    }

    auto stream = zip.createStreamForEntry(bestIndex);
    if (stream == nullptr)
    {
        result.error = NR_STR("无法打开 ZIP 内条目");
        return result;
    }

    juce::MemoryBlock mb;
    stream->readIntoMemoryBlock(mb);
    if (mb.getSize() < 20)
    {
        result.error = NR_STR("条目过小");
        return result;
    }

    const uint8_t* bytes = static_cast<const uint8_t*>(mb.getData());
    size_t total = mb.getSize();
    size_t dataOff = 0;
    std::vector<int64_t> shape;
    bool fortran = false;
    int elemBytes = 0;
    bool isFloat = false;

    if (!parseNpyHeader(bytes, total, dataOff, shape, fortran, elemBytes, isFloat, result.error))
        return result;

    if (!isFloat)
    {
        result.error = NR_STR("内部错误：非浮点");
        return result;
    }

    if (!extractWindow(shape, sampleIndex, bytes + dataOff, total - dataOff,
                       elemBytes, fortran, result, result.error))
        return result;

    return result;
}

namespace {
bool fillPlaybackFromPayload(const std::vector<int64_t>& shape,
                             const uint8_t* payload, size_t payloadBytes, size_t dataByteOffset,
                             int elemBytes, bool fortranOrder, NpzPlaybackSeries& out,
                             juce::String& err)
{
    if (fortranOrder)
    {
        err = NR_STR("不支持 Fortran 序数组");
        return false;
    }

    const uint8_t* base = payload + dataByteOffset;
    const size_t   nAvail = payloadBytes > dataByteOffset ? (payloadBytes - dataByteOffset) : 0;

    auto readFloatAt = [&](size_t index) -> float
    {
        if (elemBytes == 4)
            return reinterpret_cast<const float*>(base)[index];
        const double* dp = reinterpret_cast<const double*>(base);
        return (float)dp[index];
    };

    if (shape.size() == 2)
    {
        const int64_t C = shape[0];
        const int64_t T = shape[1];
        if (C <= 0 || T <= 0)
        {
            err = NR_STR("非法 (C,T)");
            return false;
        }
        const size_t need = (size_t)(C * T) * (size_t)elemBytes;
        if (need > nAvail)
        {
            err = NR_STR("数据长度不足");
            return false;
        }
        out.channels     = (int)C;
        out.totalFrames  = (int)T;
        out.timeMajor.resize((size_t)(T * C));
        for (int t = 0; t < (int)T; ++t)
            for (int c = 0; c < (int)C; ++c)
                out.timeMajor[(size_t)t * (size_t)C + (size_t)c] =
                    readFloatAt((size_t)c * (size_t)T + (size_t)t);
        return true;
    }

    if (shape.size() == 3)
    {
        const int64_t N = shape[0];
        const int64_t C = shape[1];
        const int64_t T = shape[2];
        if (N <= 0 || C <= 0 || T <= 0)
        {
            err = NR_STR("非法 (N,C,T)");
            return false;
        }
        const size_t    stride = (size_t)(C * T);
        const int64_t   totalT = N * T;
        const size_t need      = (size_t)(N * stride) * (size_t)elemBytes;
        if (need > nAvail)
        {
            err = NR_STR("数据长度不足");
            return false;
        }
        out.channels     = (int)C;
        out.totalFrames  = (int)totalT;
        out.timeMajor.resize((size_t)totalT * (size_t)C);
        size_t outIx = 0;
        for (int n = 0; n < (int)N; ++n)
            for (int t = 0; t < (int)T; ++t)
                for (int c = 0; c < (int)C; ++c)
                    out.timeMajor[outIx++] =
                        readFloatAt((size_t)n * stride + (size_t)c * (size_t)T + (size_t)t);
        return true;
    }

    err = juce::String::formatted(NR_STR("回放仅支持 2D (C,T) 或 3D (N,C,T)，当前=%d 维"), (int)shape.size());
    return false;
}
} // namespace

NpzPlaybackSeries NpzEEGLoader::loadPlaybackSeries(const juce::File& npzFile)
{
    NpzPlaybackSeries series;
    if (!npzFile.existsAsFile())
    {
        series.error = NR_STR("文件不存在");
        return series;
    }

    juce::ZipFile zip(npzFile);
    if (zip.getNumEntries() <= 0)
    {
        series.error = NR_STR("无法作为 ZIP 读取（非 NPZ）");
        return series;
    }

    int bestIndex = -1;
    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        if (auto* e = zip.getEntry(i))
        {
            if (e->isSymbolicLink || e->filename.endsWithIgnoreCase("/"))
                continue;
            auto leaf = entryLeaf(e->filename);
            if (leaf.equalsIgnoreCase("data.npy"))
            {
                bestIndex = i;
                break;
            }
            if (bestIndex < 0 && leaf.endsWithIgnoreCase(".npy"))
                bestIndex = i;
        }
    }

    if (bestIndex < 0)
    {
        series.error = NR_STR("NPZ 内未找到 .npy");
        return series;
    }

    auto stream = zip.createStreamForEntry(bestIndex);
    if (stream == nullptr)
    {
        series.error = NR_STR("无法打开 ZIP 条目");
        return series;
    }

    juce::MemoryBlock mb;
    stream->readIntoMemoryBlock(mb);
    if (mb.getSize() < 20)
    {
        series.error = NR_STR("条目过小");
        return series;
    }

    const uint8_t* bytes   = static_cast<const uint8_t*>(mb.getData());
    const size_t   total   = mb.getSize();
    size_t         dataOff = 0;
    std::vector<int64_t> shape;
    bool           fortran = false;
    int            elemBytes = 0;
    bool           isFloat = false;

    if (!parseNpyHeader(bytes, total, dataOff, shape, fortran, elemBytes, isFloat, series.error))
        return series;

    if (!isFloat)
    {
        series.error = NR_STR("仅支持浮点 data");
        return series;
    }

    if (!fillPlaybackFromPayload(shape, bytes, total, dataOff, elemBytes, fortran, series,
                                 series.error))
        return series;

    return series;
}
