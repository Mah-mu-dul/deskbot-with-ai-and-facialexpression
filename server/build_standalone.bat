@echo off
rem Builds the standalone DeskBot app into  ..\dist\DeskBot\DeskBot.exe
rem Needs Python 3.10-3.12 on THIS machine only. The result runs without Python.
setlocal
cd /d "%~dp0"

if not exist models\emotion-ferplus-8.onnx (
  echo Missing models\emotion-ferplus-8.onnx - run: python download_models.py
  exit /b 1
)
if not exist models\whisper-base\model.bin (
  echo Missing models\whisper-base - run: python download_models.py
  exit /b 1
)
if not exist models\yolov8n.onnx (
  echo Missing models\yolov8n.onnx - see README, "Build the standalone app"
  exit /b 1
)

if not exist .venv-build\Scripts\python.exe (
  echo Creating build environment...
  python -m venv .venv-build || exit /b 1
)
.venv-build\Scripts\python.exe -m pip install --upgrade pip -q
.venv-build\Scripts\python.exe -m pip install -r requirements-standalone.txt -q || exit /b 1

.venv-build\Scripts\python.exe -m PyInstaller app.py ^
  --name DeskBot --onedir --console --noconfirm --clean ^
  --distpath ..\dist --workpath build\pyinstaller --specpath build ^
  --add-data "%~dp0static;static" ^
  --add-data "%~dp0models\emotion-ferplus-8.onnx;models" ^
  --add-data "%~dp0models\yolov8n.onnx;models" ^
  --add-data "%~dp0models\whisper-base;models\whisper-base" ^
  --collect-all faster_whisper ^
  --collect-all ctranslate2 ^
  --collect-submodules pyttsx3 ^
  --hidden-import pyttsx3.drivers.sapi5 ^
  --hidden-import comtypes.client ^
  --collect-submodules uvicorn ^
  --collect-submodules google.genai ^
  --collect-data cv2 ^
  --collect-data certifi ^
  --collect-all miniaudio ^
  --collect-all edge_tts ^
  --exclude-module torch --exclude-module torchvision --exclude-module ultralytics ^
  --exclude-module matplotlib --exclude-module tkinter --exclude-module polars || exit /b 1

copy /y .env.example ..\dist\DeskBot\.env.example >nul
echo.
echo Done: ..\dist\DeskBot\DeskBot.exe
echo Copy the whole dist\DeskBot folder to any Windows laptop and double-click DeskBot.exe.
endlocal
