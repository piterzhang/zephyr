#!/bin/bash

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Target device (stable symlink, not raw /dev/sdX)
DEVICE="/dev/disk/by-id/usb-Generic-_SD_MMC_20120501030900000-0:0"

# Partition start sector (0x8000 = 32768, 16MB offset)
PART_START=32768

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info()  { echo -e "${GREEN}[INFO]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; }

# -------------------------------------------------------
# Checks
# -------------------------------------------------------

if [ ! -e "$DEVICE" ]; then
    error "Target device not found: $DEVICE"
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

# Resolve to block device (e.g. /dev/sdX)
BLOCK_DEV=$(readlink -f "$DEVICE")
if [ ! -b "$BLOCK_DEV" ]; then
    error "Resolved block device is not valid: $BLOCK_DEV"
    exit 1
fi

# Safety: refuse if device is mounted
if lsblk -nro MOUNTPOINT "$BLOCK_DEV" 2>/dev/null | grep -q "/"; then
    error "Device $BLOCK_DEV has mounted partitions. Unmount them first:"
    lsblk -no NAME,MOUNTPOINT "$BLOCK_DEV"
    exit 1
fi

# -------------------------------------------------------
# Confirmation
# -------------------------------------------------------
echo "================================================"
echo "  SD Card Partition & Format Tool"
echo "================================================"
echo "  Device : $DEVICE -> $BLOCK_DEV"
echo "  Start  : sector ${PART_START} (16MB offset)"
echo "  Type   : FAT32 (0x0C)"
echo "================================================"
echo ""
warn "ALL DATA on this device will be DESTROYED!"
echo ""
read -rp "Type 'YES' to continue: " confirm
if [ "$confirm" != "YES" ]; then
    info "Aborted by user."
    exit 0
fi

# -------------------------------------------------------
# Step 1: Create partition table & FAT32 partition
# -------------------------------------------------------
info "Creating new DOS partition table and FAT32 partition on $BLOCK_DEV ..."

sudo parted "$BLOCK_DEV" --script \
    mklabel msdos \
    mkpart primary fat32 $((PART_START * 512))B 100%

if [ $? -ne 0 ]; then
    error "parted failed. Aborting."
    exit 1
fi
info "Partition table created successfully."

# Re-read partition table
sudo partprobe "$BLOCK_DEV" 2>/dev/null
sleep 1

# Determine the first partition device
PART_DEV="${BLOCK_DEV}1"
if [ ! -b "$PART_DEV" ]; then
    PART_DEV="${BLOCK_DEV}p1"
fi

if [ ! -b "$PART_DEV" ]; then
    error "Cannot find partition device (${BLOCK_DEV}1 or ${BLOCK_DEV}p1)."
    echo "Available partitions:"
    lsblk -no NAME,SIZE,TYPE "$BLOCK_DEV"
    exit 1
fi

info "Partition device: $PART_DEV"

# -------------------------------------------------------
# Step 2: Format as FAT32
# -------------------------------------------------------
info "Formatting $PART_DEV as FAT32 ..."
sudo mkfs.vfat -F 32 "$PART_DEV"
if [ $? -ne 0 ]; then
    error "mkfs.vfat failed. Aborting."
    exit 1
fi

info "Done! SD card is ready to use."
