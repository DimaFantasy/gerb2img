@echo off
echo === Запуск сборки 64-битной DLL (Release) ===
SET MSYS2_PATH=C:\msys64
"%MSYS2_PATH%\msys2_shell.cmd" -mingw64 -here -c "cd '%CD%' && make dll"
echo.
echo === Сборка завершена ===
pause