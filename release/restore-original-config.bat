@echo off
title Gamepads of Valhalla - restore config
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0patch-config.ps1" -Restore
