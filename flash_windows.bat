@echo off
REM ============================================================================
REM TTGO_CyberDeck — Windows One-Click Build, Flash & Recovery Helper
REM Target: LilyGO TTGO T-Display 1.14" ESP32 Clone
REM ============================================================================
title TTGO_CyberDeck // Firmware Flasher for Windows

echo ============================================================
echo   TTGO_CyberDeck — POCKET CYBERDECK FIRMWARE (ESP32)
echo ============================================================
echo.
echo   [1] Build and Flash Standard Clone (Variant A - Default)
echo   [2] Build and Flash Alternative Clone (Variant B)
echo   [3] Open Serial Monitor (115200 baud)
echo   [4] SAFE RECOVERY: Erase Flash Completely + Reflash
echo   [5] Exit
echo.
set /p choice="Select action [1-5]: "

if "%choice%"=="1" (
    pio run -e ttgo-t-display-clone -t upload
    pause
    exit /b
)
if "%choice%"=="2" (
    pio run -e ttgo-t-display-clone-alt -t upload
    pause
    exit /b
)
if "%choice%"=="3" (
    pio device monitor -b 115200
    pause
    exit /b
)
if "%choice%"=="4" (
    echo.
    echo [RECOVERY] Hold Down BUTTON A (GPIO 0 / BOOT) on the board if needed...
    pio run -e ttgo-t-display-clone -t erase
    pio run -e ttgo-t-display-clone -t upload
    pause
    exit /b
)
exit /b
