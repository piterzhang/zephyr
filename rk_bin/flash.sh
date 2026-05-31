#!/bin/bash

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Path to the image file relative to script location
IMAGE_PATH="${SCRIPT_DIR}/Image/zephyr.itb"

# Target device
DEVICE="/dev/disk/by-id/usb-Generic-_SD_MMC_20120501030900000-0:0"

# Check if image file exists
if [ ! -f "$IMAGE_PATH" ]; then
    echo "Error: Image file not found at $IMAGE_PATH"
    echo "Please build the firmware first"
    exit 1
fi

# Check if target device exists
if [ ! -e "$DEVICE" ]; then
    echo "Error: Target device not found: $DEVICE"
    echo ""
    echo "Available devices in /dev/disk/by-id/:"
    ls -la /dev/disk/by-id/ 2>/dev/null | grep -E "usb-|sd-" || echo "  No devices found"
    echo ""
    echo "Please ensure:"
    echo "  1. The SD card reader is connected"
    echo "  2. An SD card is inserted"
    echo "  3. The device path matches your hardware"
    echo ""
    echo "Note: In WSL2, make sure the device is accessible from Linux"
    exit 1
fi

# Flash the image
echo "Flashing $IMAGE_PATH to SD card..."
echo "Target device: $DEVICE"
sudo dd if="$IMAGE_PATH" of="$DEVICE" seek=16384 status=progress
sync
echo "Flashing completed successfully!"