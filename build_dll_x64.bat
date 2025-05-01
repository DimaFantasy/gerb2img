@echo off
chcp 65001 >nul
echo === Запуск сборки 64-битной DLL (Release) ===
SET MSYS2_PATH=C:\msys64
"%MSYS2_PATH%\mingw64.exe" make dll