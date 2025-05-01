@echo off
echo === Запуск сборки 32-битной DLL (Debug) ===
SET MSYS2_PATH=C:\msys64
"%MSYS2_PATH%\msys2_shell.cmd" -mingw32 -here -c "cd '%CD%' && make dll_debug"
echo.
echo === Сборка завершена ===
pause