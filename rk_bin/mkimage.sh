#!/bin/bash
set -e  # 一旦发生错误，脚本会立即退出

export LC_ALL=C.UTF-8
export LANG=C.UTF-8

usage() {
    echo "usage: ./mkimage.sh [its]"
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CUR_DIR="$SCRIPT_DIR"
TOOLS="$SCRIPT_DIR/tools"
IMAGE="$SCRIPT_DIR/Image"
echo "Script Directory: $SCRIPT_DIR"

# ensure image directory exists
mkdir -p "$IMAGE"
# remove previous image if present (non-fatal)
rm -f "$IMAGE/zephyr.bin"
# source binary relative to script directory
SRC_BIN="$SCRIPT_DIR/../build/zephyr/zephyr.bin"
cp "$SRC_BIN" "$IMAGE/zephyr.bin" || { echo "Error: Copy zephyr.bin failed from $SRC_BIN"; exit 1; }
# 生成镜像文件
"$TOOLS/mkimage" -f "$IMAGE/zephyr.its" -E "$IMAGE/zephyr.itb" || { echo "Error: mkimage failed"; exit 1; }

echo 'Image: zephyr.itb is ready.'