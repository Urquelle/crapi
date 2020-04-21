@echo off

SET PROJECT_NAME=crapi
SET PROJECT_MAIN_FILE=main.c
SET PROJECT_LINKER_FLAGS=user32.lib gdi32.lib winmm.lib Shlwapi.lib

SET PATH=%userprofile%\Dev\C\bin;%userprofile%\Dev\C\bin;%PATH%

call global_shell.bat
