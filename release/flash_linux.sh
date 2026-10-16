#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PORT=${1:-""}
if [ -z "$PORT" ]; then
    PORT=$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -n 1 || true)
fi

if [ -z "$PORT" ]; then
    echo "=========================================================="
    echo " Помилка: Плату ESP32 (TTGO_CyberDeck) не знайдено!"
    echo " Підключіть плату кабелем USB та запустіть знову:"
    echo "   sudo ./flash_linux.sh"
    echo " Або вкажіть порт вручну:"
    echo "   sudo ./flash_linux.sh /dev/ttyUSB0"
    echo "   sudo ./flash_linux.sh /dev/ttyACM0"
    echo "=========================================================="
    exit 1
fi

ESPTOOL=$(command -v esptool || true)
if [ -z "$ESPTOOL" ] || [ ! -x "$ESPTOOL" ]; then
    if [ -x "/home/rxuser/.local/bin/esptool" ]; then
        ESPTOOL="/home/rxuser/.local/bin/esptool"
    elif [ -x "/home/rxuser/.platformio/penv/bin/esptool" ]; then
        ESPTOOL="/home/rxuser/.platformio/penv/bin/esptool"
    elif [ -x "$HOME/.local/bin/esptool" ]; then
        ESPTOOL="$HOME/.local/bin/esptool"
    else
        ESPTOOL="esptool"
    fi
fi

BIN_FILE="$SCRIPT_DIR/TTGO_CyberDeck.bin"
if [ ! -f "$BIN_FILE" ] && [ -f "$SCRIPT_DIR/release/TTGO_CyberDeck.bin" ]; then
    BIN_FILE="$SCRIPT_DIR/release/TTGO_CyberDeck.bin"
fi

echo "=========================================================="
echo " 1. Очищення флеш-пам'яті ESP32 (Full Erase) на $PORT..."
echo "=========================================================="
"$ESPTOOL" --chip esp32 -p "$PORT" erase_flash

echo "=========================================================="
echo " 2. Прошивання TTGO_CyberDeck Firmware у порт $PORT"
echo "=========================================================="
"$ESPTOOL" --chip esp32 -p "$PORT" -b 921600 write_flash 0x0 "$BIN_FILE"

echo ""
echo "=========================================================="
echo " Прошивання завершено! Натисніть RESET на TTGO_CyberDeck."
echo "=========================================================="
