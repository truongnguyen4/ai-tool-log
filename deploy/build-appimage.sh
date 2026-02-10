#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# build-appimage.sh
# Builds ToolLogPro AppImage inside an Ubuntu 20.04 Docker container.
#
# This ensures the binary is linked against GLIBC 2.31, making it run on:
#   Ubuntu 20.04+, Debian 11+, Fedora 32+, and any distro with GLIBC >= 2.31.
#
# Requirements on the host:
#   - Docker (sudo docker or user in the "docker" group)
#   - Internet access (first run downloads Qt 6.9.1, ~1 GB)
#
# Usage:
#   cd <project-root>
#   ./deploy/build-appimage.sh
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE_TAG="toollogpro-builder:ubuntu20"
OUTPUT_APPIMAGE="$PROJECT_ROOT/deploy/ToolLogPro-x86_64.AppImage"

echo "============================================================"
echo " ToolLogPro AppImage builder (Ubuntu 20.04 / GLIBC 2.31)"
echo "============================================================"
echo "Project root : $PROJECT_ROOT"
echo "Output       : $OUTPUT_APPIMAGE"
echo ""

# ── Step 1: Build the Docker image ───────────────────────────────────────────
echo "[1/3] Building Docker image ($IMAGE_TAG) ..."
docker build \
    -f "$PROJECT_ROOT/Dockerfile.build" \
    -t "$IMAGE_TAG" \
    "$PROJECT_ROOT"

# ── Step 2: Extract the AppImage from the container ──────────────────────────
echo ""
echo "[2/3] Extracting AppImage from container ..."
CONTAINER_ID=$(docker create "$IMAGE_TAG")
docker cp "$CONTAINER_ID:/output/ToolLogPro-x86_64.AppImage" "$OUTPUT_APPIMAGE"
docker rm "$CONTAINER_ID" > /dev/null

# ── Step 3: Done ─────────────────────────────────────────────────────────────
echo ""
echo "[3/3] Done!"
echo ""
ls -lh "$OUTPUT_APPIMAGE"
echo ""

# Show the GLIBC version requirement of the produced binary
echo "---- GLIBC requirements (should be <= 2.31) ----"
APPIMAGE_EXTRACT_AND_RUN=1 "$OUTPUT_APPIMAGE" --appimage-extract-and-run 2>/dev/null || true
# Use objdump on the extracted binary if available
if command -v objdump &>/dev/null; then
    TMP_DIR=$(mktemp -d)
    cd "$TMP_DIR"
    # Extract AppImage to inspect the binary
    APPIMAGE_EXTRACT_AND_RUN=1 "$OUTPUT_APPIMAGE" --appimage-extract squashfs-root/usr/bin/ToolLogPro 2>/dev/null || true
    if [[ -f "$TMP_DIR/squashfs-root/usr/bin/ToolLogPro" ]]; then
        objdump -p "$TMP_DIR/squashfs-root/usr/bin/ToolLogPro" \
            | grep -E "GLIBC_[0-9]" | awk '{print $NF}' | sort -V | uniq
    fi
    cd - > /dev/null
    rm -rf "$TMP_DIR"
fi
echo "------------------------------------------------"
echo ""
echo "The AppImage should now run on any x86_64 Linux with GLIBC >= 2.31."
