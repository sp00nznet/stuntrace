@echo off
rem Quick start: checks prerequisites, finds your ROM, builds the game and leaves
rem "Stunt Race FX (recomp).cmd" to play. Safe to rerun. See README.md, Getting Started.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\setup.ps1" %*
set RC=%ERRORLEVEL%
if not defined RECOMP_LAUNCHER pause
exit /b %RC%
