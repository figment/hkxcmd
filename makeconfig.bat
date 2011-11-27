@echo off
rem Quick script to create config.h
IF EXIST "%PROGRAMFILES%\TortoiseSVN\bin\SubWCRev.exe" (
    "%PROGRAMFILES%\TortoiseSVN\bin\SubWCRev.exe" . Core\config.h.in Core\config.h > nul 2<&1
)
