@echo off
setlocal
cd /d "%~dp0"
where cmake.exe >nul 2>nul
if errorlevel 1 (
  echo CMake 3.25 or newer is required. Install the Visual Studio C++ CMake tools.
  exit /b 1
)
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
if errorlevel 1 exit /b 1
cmake --build build --config Release --parallel
if errorlevel 1 exit /b 1
ctest --test-dir build -C Release --output-on-failure
if errorlevel 1 exit /b 1
echo Built build\Release\PrintShift.exe
