# ============================================================================
# fetch_onnxruntime_directml.ps1
#
# 一键下载 DirectML 版 ONNX Runtime（与当前仓库使用的 1.16.3 对齐），
# 解压到 onnxruntime-local/onnxruntime-win-x64-directml-1.16.3/
#
# 使用：
#   PS> .\tools\fetch_onnxruntime_directml.ps1            # 默认 1.16.3
#   PS> .\tools\fetch_onnxruntime_directml.ps1 -Version 1.17.3
#
# 下载源：
#   GitHub Release - Microsoft/onnxruntime
#   https://github.com/microsoft/onnxruntime/releases/download/v<ver>/onnxruntime-win-x64-directml-<ver>.zip
# ============================================================================

[CmdletBinding()]
param(
    [string] $Version = "1.16.3",
    [switch] $Force
)

$ErrorActionPreference = "Stop"

# 仓库根：脚本上一级
$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$localDir = Join-Path $repoRoot "onnxruntime-local"
$targetName = "onnxruntime-win-x64-directml-$Version"
$targetDir  = Join-Path $localDir $targetName

if ((Test-Path $targetDir) -and -not $Force) {
    Write-Host "[OK] 目标目录已存在：$targetDir"
    Write-Host "     如需重下，追加 -Force"
    exit 0
}

if (-not (Test-Path $localDir)) {
    New-Item -ItemType Directory -Path $localDir | Out-Null
}

$zipName = "onnxruntime-win-x64-directml-$Version.zip"
$zipPath = Join-Path $localDir $zipName
$url = "https://github.com/microsoft/onnxruntime/releases/download/v$Version/$zipName"

Write-Host "[→] 下载：$url"
try {
    Invoke-WebRequest -Uri $url -OutFile $zipPath -UseBasicParsing
} catch {
    Write-Error "下载失败：$($_.Exception.Message)"
    Write-Host ""
    Write-Host "备用方案：手动下载后放到 $zipPath"
    Write-Host "  GitHub: https://github.com/microsoft/onnxruntime/releases/tag/v$Version"
    exit 1
}

Write-Host "[→] 解压到：$targetDir"
if (Test-Path $targetDir) {
    Remove-Item -Recurse -Force $targetDir
}
Expand-Archive -Path $zipPath -DestinationPath $localDir -Force

# 解压后目录名通常是 onnxruntime-win-x64-directml-<ver>；若不是，做一次 rename
$extracted = Get-ChildItem $localDir -Directory |
    Where-Object { $_.Name -like "onnxruntime-win-x64-directml-*" -and $_.Name -like "*$Version*" } |
    Select-Object -First 1
if ($extracted -and $extracted.FullName -ne $targetDir) {
    Rename-Item -Path $extracted.FullName -NewName $targetName
}

# 检查关键文件
$requiredFiles = @(
    "lib\onnxruntime.dll",
    "lib\onnxruntime.lib",
    "include\onnxruntime_cxx_api.h",
    "include\dml_provider_factory.h"
)
$missing = @()
foreach ($f in $requiredFiles) {
    $p = Join-Path $targetDir $f
    if (-not (Test-Path $p)) { $missing += $f }
}
if ($missing.Count -gt 0) {
    Write-Warning "包内未找到以下预期文件："
    $missing | ForEach-Object { Write-Warning "  - $_" }
    Write-Warning "请确认下载的是 -directml- 变体而非 -cpu 或 -gpu 变体。"
}

Write-Host ""
Write-Host "[OK] DirectML 版 ONNX Runtime 已就绪"
Write-Host "     位置：$targetDir"
Write-Host ""
Write-Host "下一步（首次启用）："
Write-Host "  1. 删除 build\ 后重新 cmake -B build"
Write-Host "     CMake 会自动检测并链接 DirectML 版，输出："
Write-Host "       NerouRuntime ONNX Runtime 变体：DirectML"
Write-Host "  2. 构建并运行 NerouRuntime.exe，在"
Write-Host "     设置 → 性能与加速 → 推理加速 选择 自动 / DirectML"
Write-Host "  3. 加载模型后，日志会输出：「已启用 DirectML（设备 0）」"
