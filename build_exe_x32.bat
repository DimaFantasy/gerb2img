@echo off
echo === Запуск сборки 32-битного EXE (Release) ===
SET MSYS2_PATH=C:\msys64
"%MSYS2_PATH%\msys2_shell.cmd" -mingw32 -here -c "cd '%CD%' && make exe"
echo.
echo === Сборка завершена ===
pause