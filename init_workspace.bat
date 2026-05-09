@echo off
chcp 65001 >nul
echo Initializing NerouRuntime workspace...

rem 1. Data directories
mkdir data\training_files 2>nul
mkdir data\raw_files 2>nul
mkdir data\processed_npz 2>nul
mkdir data\samples 2>nul
mkdir data\raw 2>nul
mkdir data\npz 2>nul
mkdir data\cache 2>nul
echo [OK] Training-file data directories created.

if exist EegdataStorage (
    echo [INFO] Legacy EegdataStorage detected.
)

rem 2. ONNX deploy directories
mkdir onnx\models 2>nul
mkdir onnx\deploy 2>nul
mkdir onnx\runtime_data 2>nul
mkdir onnx\runtime_packages 2>nul
mkdir onnx\templates 2>nul
echo [OK] ONNX and Runtime DATA directories created.

rem 3. Logs and Reports
mkdir reports\training 2>nul
mkdir reports\validation 2>nul
mkdir docs\reports 2>nul
mkdir logs\training 2>nul
mkdir logs\inference 2>nul
mkdir logs\runtime_data 2>nul
echo [OK] Logging directories created.

rem 4. Project root skeleton
mkdir projects 2>nul
mkdir projects\default_project\training_files 2>nul
mkdir projects\default_project\datasets 2>nul
mkdir projects\default_project\preprocessing 2>nul
mkdir projects\default_project\training_runs 2>nul
mkdir projects\default_project\models 2>nul
mkdir projects\default_project\exports 2>nul
mkdir projects\default_project\runtime_data 2>nul
mkdir projects\default_project\runtime_packages 2>nul
mkdir projects\default_project\validations 2>nul
mkdir projects\default_project\reports 2>nul
mkdir projects\default_project\logs 2>nul
echo [OK] Default project directories created.

rem 5. Generate built-in demo training data (if not exists)
if not exist "data\npz\demo_bci_4class.npz" (
    echo [INFO] Generating built-in demo training dataset...
    python python_core\generate_demo_data.py
    if errorlevel 1 (
        echo [WARN] Demo data generation failed - Python may not be available.
        echo        You can generate it later: python python_core\generate_demo_data.py
    ) else (
        echo [OK] Demo training data generated.
    )
) else (
    echo [OK] Demo training data already exists.
)

echo ===========================================
echo Workspace initialization complete.
echo ===========================================
echo.
echo Built-in data:
dir /B data\npz\*.npz 2>nul
dir /B data\training_files\*.npz 2>nul
echo.
echo Models:
dir /B onnx\models 2>nul
echo.
echo Runtime DATA:
dir /B onnx\runtime_data 2>nul
echo.
