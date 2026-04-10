#!/usr/bin/env bash
# =============================================================================
# gen_spv.sh — Linux/macOS: compile 2D renderer shaders to SPIR-V
#
# Usage:
#   ./gen_spv.sh [glslangValidator|glslc]
# =============================================================================
set -euo pipefail

COMPILER="${1:-glslangValidator}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SHADER_DIR="$SCRIPT_DIR/../shaders"

# Locate compiler
if ! command -v "$COMPILER" &>/dev/null; then
    if [[ -n "${VULKAN_SDK:-}" ]] && [[ -x "$VULKAN_SDK/bin/$COMPILER" ]]; then
        COMPILER="$VULKAN_SDK/bin/$COMPILER"
    else
        echo "ERROR: '$COMPILER' not found."
        echo "Install with:"
        echo "  Linux:  sudo apt install glslang-tools"
        echo "  macOS:  brew install glslang"
        echo "  OR:     install the Vulkan SDK from https://vulkan.lunarg.com/"
        exit 1
    fi
fi

echo "Using compiler: $COMPILER"

echo "Compiling vertex shader..."
"$COMPILER" -V "$SHADER_DIR/nkrenderer2d.vert" -o "$SHADER_DIR/nkrenderer2d_vert.spv"
echo "  OK ($(wc -c < "$SHADER_DIR/nkrenderer2d_vert.spv") bytes)"

echo "Compiling fragment shader..."
"$COMPILER" -V "$SHADER_DIR/nkrenderer2d.frag" -o "$SHADER_DIR/nkrenderer2d_frag.spv"
echo "  OK ($(wc -c < "$SHADER_DIR/nkrenderer2d_frag.spv") bytes)"

echo "Generating NkRenderer2DVkSpv.inl..."
python3 "$SCRIPT_DIR/gen_spv.py" --shader-dir "$SHADER_DIR" --out "$SHADER_DIR"

echo ""
echo "Done! Copy NkRenderer2DVkSpv.inl to:"
echo "  src/NKContext/Renderer/Backends/Vulkan/NkRenderer2DVkSpv.inl"