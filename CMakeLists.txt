cmake_minimum_required(VERSION 3.20)
project(NerouRuntime VERSION 1.0.0) # 显示版本见 Main.cpp（v1.0）

# Set IDE to use C++20 for optimal performance and JUCE 8 compatibility.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------------------
# JUCE 8.0.12 — 使用本地已安装版本 (D:/JUCE)，直接 add_subdirectory
# 若要切换到网络下载，改用 FetchContent_Declare 指定 GIT_TAG
# -------------------------------------------------------------
add_subdirectory("D:/JUCE" "${CMAKE_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)

# 生产交付默认嵌入 EXE 图标
# 若本机 juceaide winicon 在 configure 阶段失败，可临时：cmake -B build -DNEROU_USE_EXE_ICON=OFF
option(NEROU_USE_EXE_ICON "Windows: embed logo.png as EXE icon" ON)
set(_nr_icon "${CMAKE_CURRENT_SOURCE_DIR}/logo.png")
if(NEROU_USE_EXE_ICON AND EXISTS "${_nr_icon}")
    message(STATUS "NEROU_USE_EXE_ICON：使用 ${_nr_icon} 作为应用程序图标")
    juce_add_gui_app(NerouRuntime
        PRODUCT_NAME "NerouRuntime"
        VERSION 1.0.0
        ICON_BIG "${_nr_icon}")
elseif(NEROU_USE_EXE_ICON AND NOT EXISTS "${_nr_icon}")
    message(WARNING "NEROU_USE_EXE_ICON=ON 但未找到 logo.png，已回退为无 EXE 图标。")
    juce_add_gui_app(NerouRuntime
        PRODUCT_NAME "NerouRuntime"
        VERSION 1.0.0)
else()
    juce_add_gui_app(NerouRuntime
        PRODUCT_NAME "NerouRuntime"
        VERSION 1.0.0)
endif()

# ── Embed binary assets (logo + icon) ────────────────────────────────────────
juce_add_binary_data(NerouRuntime_Assets
    SOURCES
        logo.png
)

# CRITICAL FIX: Tell JUCE to explicitly generate the global <JuceHeader.h> 
juce_generate_juce_header(NerouRuntime)

# ── Precompiled Header ─────────────────────────────────────────────────────
# JuceHeader.h 展开后有数万行模板代码；项目里 80+ 个 cpp 几乎每个都 include 它。
# 用 PCH 一次性编译好，之后的 cpp 直接复用，首次全量 -40~60%，日常增量 -30%。
#
# 可通过 -DNEROU_USE_PCH=OFF 关闭（调试 PCH 冲突或极少数特殊编译场景）。
option(NEROU_USE_PCH "Use precompiled headers for JuceHeader.h" ON)
if(NEROU_USE_PCH)
    # 限定仅 C++ 编译器生效：CMake 默认会为 target 所有语言（C/CXX）各生成一份
    # cmake_pch 桩文件，JuceHeader.h 拉入 <yvals_core.h> 会拒绝 C 编译器
    # (STL1003: Unexpected compiler, expected C++ compiler)。
    # 注意：不能写 "<JuceHeader.h>"（角括号会和 $<...> 生成器表达式冲突，
    # 被解析成畸形 #include ">"，导致 C1083）。改写为裸文件名，CMake 会生成
    # #include "JuceHeader.h"，JUCE 的 include 路径上已有该头，解析照常。
    target_precompile_headers(NerouRuntime PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:JuceHeader.h>"
    )

    # JUCE 模块 cpp 明确禁止任何编译器自动前缀头注入（/FI），否则会触发
    # "Incorrect use of JUCE cpp file" 编译错误。把它们从 PCH 排除。
    # 匹配 "D:/JUCE/modules/<name>/juce_*.cpp|mm|c"。
    file(GLOB _nr_juce_module_srcs LIST_DIRECTORIES false
        "D:/JUCE/modules/*/juce_*.cpp"
        "D:/JUCE/modules/*/juce_*.mm"
        "D:/JUCE/modules/*/juce_*.c")
    if(_nr_juce_module_srcs)
        set_source_files_properties(${_nr_juce_module_srcs}
            PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
        list(LENGTH _nr_juce_module_srcs _nr_juce_srcs_n)
        message(STATUS "NerouRuntime: PCH 已启用 (JuceHeader.h, CXX only, ${_nr_juce_srcs_n} JUCE module srcs excluded)")
    else()
        message(STATUS "NerouRuntime: PCH 已启用 (JuceHeader.h, CXX only)")
    endif()
endif()

# Add all the architectural GUI files here.
target_sources(NerouRuntime PRIVATE
    # Main
    Source/Main.cpp
    Source/MainComponent.h
    Source/MainComponent.cpp

    # Domain Layer
    Source/Domain/TaskState.h
    Source/Domain/Entities.h
    Source/Domain/EEGRecord.h
    Source/Domain/PipelineNode.h
    
    # Application Layer
    Source/Application/WorkflowOrchestrator.h
    Source/Application/WorkflowOrchestrator.cpp
    Source/Application/RuntimeSettingsStore.h
    Source/Application/RuntimeSettingsStore.cpp
    Source/Application/GlobalContextStore.h
    Source/Application/GlobalContextStore.cpp
    Source/Application/PipelineStore.h
    Source/Application/PipelineStore.cpp

    # Service Layer
    Source/Services/AcquisitionService.h
    Source/Services/AcquisitionService.cpp
    Source/Services/DatasetPreparationService.h
    Source/Services/DatasetPreparationService.cpp
    Source/Services/TrainingService.h
    Source/Services/TrainingService.cpp
    Source/Services/ModelRegistryService.h
    Source/Services/ModelRegistryService.cpp
    Source/Services/ValidationService.h
    Source/Services/ValidationService.cpp
    Source/Services/RuntimeDataExportService.h
    Source/Services/RuntimeDataExportService.cpp
    
    # Core Layer
    Source/Core/PythonBridge.h
    Source/Core/ProjectPaths.h
    Source/Core/NpzEEGLoader.h
    Source/Core/EEGFileReader.h
    Source/Core/TrainPreflight.h
    Source/Core/NpzEEGLoader.cpp
    Source/Core/EEGFileReader.cpp
    Source/Core/JsonFileIO.h
    Source/Core/SignalNorm.h
    Source/Core/Utf8FileText.h
    Source/Core/JsonVarHelpers.h
    Source/Core/Utf8Literals.h
    Source/Core/CjkFontFallback.h
    Source/Core/ChineseLocale.h
    Source/Core/AuditTrail.h
    Source/Core/AuditTrail.cpp
    Source/Core/SystemLogger.h
    Source/Core/SystemLogger.cpp
    
    # UI Theme
    Source/UI/Theme/DesignTokens.h
    Source/UI/Theme/DesignTokenStore.h
    Source/UI/Theme/ModernLookAndFeel.h
    Source/UI/Theme/ModernLookAndFeel.cpp
    
    # UI Components
    Source/UI/Components/MaterialCard.h
    Source/UI/Components/MaterialCard.cpp
    Source/UI/Components/MaterialChip.h
    Source/UI/Components/MaterialChip.cpp
    Source/UI/Components/MaterialSnackbar.h
    Source/UI/Components/MaterialSnackbar.cpp
    Source/UI/Components/ConfusionMatrixView.h
    Source/UI/Components/ConfusionMatrixView.cpp
    Source/UI/Components/PerformanceCurveView.h
    Source/UI/Components/PerformanceCurveView.cpp
    Source/UI/Components/AboutDialog.h
    Source/UI/Components/AboutDialog.cpp
    Source/UI/Components/ActivityTimeline.h
    Source/UI/Components/ActivityTimeline.cpp
    Source/UI/Components/CommandPalette.h
    Source/UI/Components/CommandPalette.cpp
    Source/UI/Components/SettingsDialog.h
    Source/UI/Components/SettingsDialog.cpp
    Source/UI/Components/ShortcutHelpOverlay.h
    Source/UI/Components/ShortcutHelpOverlay.cpp
    Source/UI/Components/SystemLogPanel.h
    Source/UI/Components/SystemLogPanel.cpp
    Source/UI/Components/WorkflowStepperBar.h
    Source/UI/Components/WorkflowStepperBar.cpp
    Source/UI/Components/LayoutComponents.h
    Source/UI/Components/BeautifyComponents.h

    # UI Pages
    Source/UI/Pages/OverviewPage.h
    Source/UI/Pages/OverviewPage.cpp
    Source/UI/Pages/AcquisitionPage.h
    Source/UI/Pages/AcquisitionPage.cpp
    Source/UI/Pages/PreparationPage.h
    Source/UI/Pages/PreparationPage.cpp
    Source/UI/Pages/TrainingPage.h
    Source/UI/Pages/TrainingPage.cpp
    Source/UI/Pages/ValidationPage.h
    Source/UI/Pages/ValidationPage.cpp
    
    # UI Canvas
    Source/UI/RealtimeMetricsCanvas.h
    Source/UI/RealtimeMetricsCanvas.cpp
    Source/UI/WaveformCanvas.h
    Source/UI/WaveformCanvas.cpp

    # UI Panels
    Source/UI/Panels/WorkflowPanels.h
    Source/UI/Panels/WorkflowPanels.cpp

    # UI Shell (new minimal architecture)
    Source/UI/Shell/AppShell.h
    Source/UI/Google/GoogleTheme.h
    Source/UI/Google/GoogleNavigationRail.h
    Source/UI/Google/GoogleNavigationRail.cpp
    Source/UI/Google/GoogleTopAppBar.h
    Source/UI/Google/GoogleTopAppBar.cpp
    Source/UI/Google/MaterialFAB.h
    Source/UI/Google/MaterialFAB.cpp
    Source/UI/Google/ParamFieldRegistry.h
    Source/UI/Google/ParamFieldRegistry.cpp
    Source/UI/Google/ParamInspectorPanel.h
    Source/UI/Google/ParamInspectorPanel.cpp
    
    # Inference Layer
    Source/Inference/OnnxRunner.h
    Source/Inference/OnnxRunner.cpp
    
    # Acquisition Layer
    Source/Acquisition/BoardManager.h
    Source/Acquisition/BoardManager.cpp

    # AI Layer - core engines (panel removed)
    Source/AI/AgentMemoryStore.h
    Source/AI/AgentMemoryStore.cpp
    Source/AI/ExperienceEngine.h
    Source/AI/ExperienceEngine.cpp
    Source/AI/LLMBridge.h
    Source/AI/LLMBridge.cpp
    Source/AI/AgentCore.h
    Source/AI/AgentCore.cpp
    Source/AI/AgentPanel.h
    Source/AI/AgentPanel.cpp
)

# ── Force UTF-8 charset on MSVC — required for Chinese string literals ─────────
# /source-charset:utf-8  → read .cpp/.h as UTF-8 (overrides system ANSI codepage)
# /execution-charset:utf-8 → store string literals as UTF-8 bytes in the binary
# NOTE: Only applied to NerouRuntime target (NOT globally) to avoid interfering
#       with JUCE internal sources which may have their own encoding assumptions.
if(MSVC)
    target_compile_options(NerouRuntime PRIVATE
        /source-charset:utf-8
        /execution-charset:utf-8
        /wd4819   # suppress "file contains a character that cannot be represented in the current code page"
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(NerouRuntime PRIVATE
        -finput-charset=UTF-8
        -fexec-charset=UTF-8
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(NerouRuntime PRIVATE
        -finput-charset=UTF-8
    )
endif()

# ── ONNX Runtime (Native 推理引擎) — 使用本地预解压版本 ──────────────────────────
# 默认使用 CPU 包；若存在 DirectML 包目录会自动切换并启用 DirectML EP。
#
# 目录约定：
#   onnxruntime-local/onnxruntime-win-x64-1.16.3/                 ← CPU-only
#   onnxruntime-local/onnxruntime-win-x64-directml-1.16.3/        ← DirectML
#   onnxruntime-local/onnxruntime-win-x64-gpu-1.16.3/             ← CUDA
#
# 通过 tools/fetch_onnxruntime_directml.ps1 可一键下载 DirectML 包。
set(ONNXRUNTIME_CPU_DIR      "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-local/onnxruntime-win-x64-1.16.3")
set(ONNXRUNTIME_DIRECTML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-local/onnxruntime-win-x64-directml-1.16.3")
set(ONNXRUNTIME_CUDA_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-local/onnxruntime-win-x64-gpu-1.16.3")

if(WIN32)
    option(NEROU_ORT_DIRECTML "Enable DirectML execution provider (Windows GPU)" ON)
else()
    option(NEROU_ORT_DIRECTML "Enable DirectML execution provider (Windows GPU)" OFF)
endif()
option(NEROU_ORT_CUDA "Enable CUDA execution provider (NVIDIA GPU)" OFF)

# 根据用户选项与实际存在的包，决定使用哪个 ORT 目录
set(ONNXRUNTIME_LOCAL_DIR "${ONNXRUNTIME_CPU_DIR}")
set(_nr_ort_variant "CPU")

if(NEROU_ORT_DIRECTML AND EXISTS "${ONNXRUNTIME_DIRECTML_DIR}/lib/onnxruntime.dll")
    set(ONNXRUNTIME_LOCAL_DIR "${ONNXRUNTIME_DIRECTML_DIR}")
    set(_nr_ort_variant "DirectML")
elseif(NEROU_ORT_CUDA AND EXISTS "${ONNXRUNTIME_CUDA_DIR}/lib/onnxruntime.dll")
    set(ONNXRUNTIME_LOCAL_DIR "${ONNXRUNTIME_CUDA_DIR}")
    set(_nr_ort_variant "CUDA")
endif()

message(STATUS "NerouRuntime ONNX Runtime 变体：${_nr_ort_variant}  (dir=${ONNXRUNTIME_LOCAL_DIR})")

add_library(onnxruntime_ext SHARED IMPORTED)
set_target_properties(onnxruntime_ext PROPERTIES
    IMPORTED_LOCATION "${ONNXRUNTIME_LOCAL_DIR}/lib/onnxruntime.dll"
    IMPORTED_IMPLIB "${ONNXRUNTIME_LOCAL_DIR}/lib/onnxruntime.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_LOCAL_DIR}/include"
)

# 主 DLL
add_custom_command(TARGET NerouRuntime POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_LOCAL_DIR}/lib/onnxruntime.dll"
        "$<TARGET_FILE_DIR:NerouRuntime>"
)

# shared providers（所有变体都提供这个 dll）
if(EXISTS "${ONNXRUNTIME_LOCAL_DIR}/lib/onnxruntime_providers_shared.dll")
    add_custom_command(TARGET NerouRuntime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_LOCAL_DIR}/lib/onnxruntime_providers_shared.dll"
            "$<TARGET_FILE_DIR:NerouRuntime>"
    )
endif()

# DirectML：检测 DirectML.dll（随 DirectML 包提供）+ 启用 NEROU_WITH_DML
if(NEROU_ORT_DIRECTML AND _nr_ort_variant STREQUAL "DirectML")
    target_compile_definitions(NerouRuntime PRIVATE NEROU_WITH_DML=1)
    # DirectML.dll 通常位于 lib/ 或 runtimes/win-x64/native/
    foreach(_dml_candidate
        "${ONNXRUNTIME_LOCAL_DIR}/lib/DirectML.dll"
        "${ONNXRUNTIME_LOCAL_DIR}/runtimes/win-x64/native/DirectML.dll")
        if(EXISTS "${_dml_candidate}")
            add_custom_command(TARGET NerouRuntime POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_dml_candidate}"
                    "$<TARGET_FILE_DIR:NerouRuntime>"
            )
            message(STATUS "NerouRuntime DirectML 运行时：${_dml_candidate}")
            break()
        endif()
    endforeach()
    message(STATUS "NerouRuntime: DirectML EP 已启用 (NEROU_WITH_DML=1)")
endif()

# CUDA：拷贝 CUDA providers DLL + 启用 NEROU_WITH_CUDA
if(NEROU_ORT_CUDA AND _nr_ort_variant STREQUAL "CUDA")
    target_compile_definitions(NerouRuntime PRIVATE NEROU_WITH_CUDA=1)
    foreach(_cuda_dll
        "onnxruntime_providers_cuda.dll"
        "onnxruntime_providers_tensorrt.dll")
        set(_cuda_path "${ONNXRUNTIME_LOCAL_DIR}/lib/${_cuda_dll}")
        if(EXISTS "${_cuda_path}")
            add_custom_command(TARGET NerouRuntime POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_cuda_path}"
                    "$<TARGET_FILE_DIR:NerouRuntime>"
            )
        endif()
    endforeach()
    message(STATUS "NerouRuntime: CUDA EP 已启用 (NEROU_WITH_CUDA=1)")
endif()

# Initial production resource skeleton beside the executable.
# The app also verifies this on startup, but compile-time creation makes a fresh
# Release folder immediately usable and keeps first-launch paths deterministic.
add_custom_command(TARGET NerouRuntime POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:NerouRuntime>/data/training_files"
        "$<TARGET_FILE_DIR:NerouRuntime>/data/raw_files"
        "$<TARGET_FILE_DIR:NerouRuntime>/data/processed_npz"
        "$<TARGET_FILE_DIR:NerouRuntime>/data/samples"
        "$<TARGET_FILE_DIR:NerouRuntime>/data/cache"
        "$<TARGET_FILE_DIR:NerouRuntime>/onnx/models"
        "$<TARGET_FILE_DIR:NerouRuntime>/onnx/deploy"
        "$<TARGET_FILE_DIR:NerouRuntime>/onnx/runtime_data"
        "$<TARGET_FILE_DIR:NerouRuntime>/onnx/runtime_packages"
        "$<TARGET_FILE_DIR:NerouRuntime>/onnx/templates"
        "$<TARGET_FILE_DIR:NerouRuntime>/reports/training"
        "$<TARGET_FILE_DIR:NerouRuntime>/reports/validation"
        "$<TARGET_FILE_DIR:NerouRuntime>/logs/training"
        "$<TARGET_FILE_DIR:NerouRuntime>/logs/inference"
        "$<TARGET_FILE_DIR:NerouRuntime>/logs/runtime_data"
        "$<TARGET_FILE_DIR:NerouRuntime>/projects/default_project/training_files"
        "$<TARGET_FILE_DIR:NerouRuntime>/projects/default_project/datasets"
        "$<TARGET_FILE_DIR:NerouRuntime>/projects/default_project/preprocessing"
        "$<TARGET_FILE_DIR:NerouRuntime>/projects/default_project/training_runs"
        "$<TARGET_FILE_DIR:NerouRuntime>/projects/default_project/models"
        "$<TARGET_FILE_DIR:NerouRuntime>/projects/default_project/exports"
        "$<TARGET_FILE_DIR:NerouRuntime>/projects/default_project/runtime_data"
        "$<TARGET_FILE_DIR:NerouRuntime>/projects/default_project/runtime_packages"
        "$<TARGET_FILE_DIR:NerouRuntime>/projects/default_project/validations"
        "$<TARGET_FILE_DIR:NerouRuntime>/projects/default_project/reports"
        "$<TARGET_FILE_DIR:NerouRuntime>/projects/default_project/logs"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/resource_layout.json"
        "$<TARGET_FILE_DIR:NerouRuntime>/resource_layout.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/init_workspace.bat"
        "$<TARGET_FILE_DIR:NerouRuntime>/init_workspace.bat"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/python_core"
        "$<TARGET_FILE_DIR:NerouRuntime>/python_core"
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/npz/demo_bci_4class.npz")
    add_custom_command(TARGET NerouRuntime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/data/npz/demo_bci_4class.npz"
            "$<TARGET_FILE_DIR:NerouRuntime>/data/training_files/demo_bci_4class.npz"
    )
endif()

# Compile using highest JUCE graphics backends
target_compile_definitions(NerouRuntime PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0   # Windows 上 JUCE 使用 WinInet，无需 libcurl
    JUCE_OPENGL=1
)
# 独立桌面程序：允许 AlertWindow::runModalLoop（JUCE 8 默认 JUCE_MODAL_LOOPS_PERMITTED=0）
target_compile_definitions(NerouRuntime PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

target_link_libraries(NerouRuntime PRIVATE
    NerouRuntime_Assets
    onnxruntime_ext
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_opengl
)

# ── Windows: JUCE_USE_CURL=1 需要链接 WinInet（JUCE 内置 HTTP 后端） ─────────────
# JUCE 在 Windows 上默认使用 WinInet 而非 libcurl，无需额外 curl.lib
# 若在 Linux/macOS 上使用 JUCE_USE_CURL=1，需要额外 find_package(CURL REQUIRED)
# 并添加 CURL::libcurl 到 target_link_libraries
