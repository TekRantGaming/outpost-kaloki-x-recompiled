@echo off
rem Double-click to build Outpost Kaloki X on this PC from your own XBLA package.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Build-OutpostKalokiX.ps1" %*
