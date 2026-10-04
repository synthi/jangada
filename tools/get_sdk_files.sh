#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Fetch the three JieLi AC79 SDK files the package needs (Apache-2.0, not part of this tree),
# instead of cloning the whole SDK (gitee asks for a login to clone):
#   tools/get_sdk_files.sh [DEST]      (default: $AC79_SDK or ~/fw-AC79_AIoT_SDK)
# build.py checks their SHA-256 against AC79NN_SDK_V1.2.1_2023-12-13.
set -e
DEST="${1:-${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}}"
TAG=AC79NN_SDK_V1.2.1_2023-12-13
BASE="https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/raw/$TAG/cpu/wl82/tools"
mkdir -p "$DEST/cpu/wl82/tools/cfg"
for f in uboot.boot cfg_tool.bin cfg/eq_cfg_hw.bin; do
    [ -s "$DEST/cpu/wl82/tools/$f" ] && continue
    curl -fsSL --retry 3 -o "$DEST/cpu/wl82/tools/$f" "$BASE/$f"
done
cat <<SUMS | (cd "$DEST/cpu/wl82/tools" && sha256sum -c --quiet -)
4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4  uboot.boot
276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b  cfg_tool.bin
41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d  cfg/eq_cfg_hw.bin
SUMS
echo "AC79_SDK=$DEST"
