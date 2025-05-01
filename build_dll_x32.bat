@echo off
chcp 65001 >nul
echo === Запуск сборки 32-битной DLL (Release) ===
SET MSYS2_PATH=C:\msys64
"%MSYS2_PATH%\mingw32.exe" make dll