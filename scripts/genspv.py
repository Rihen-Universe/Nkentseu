#!/usr/bin/env python3
# =============================================================================
# genspv.py — Compile nkrenderer2d.vert/.frag → SPIR-V → NkRenderer2DVkSpv.inl
#
# Usage:
#   python gen_spv.py [--compiler glslangValidator|glslc] [--shader-dir .] [--out .]
#
# Requirements (install one of):
#   • Vulkan SDK  (includes glslangValidator + glslc)
#       Windows: https://vulkan.lunarg.com/sdk/home
#       macOS:   brew install vulkan-headers vulkan-validationlayers glslang
#       Linux:   sudo apt install glslang-tools  OR  pip install glslang
#   • shaderc standalone: https://github.com/google/shaderc
#
# After running this script, copy the generated NkRenderer2DVkSpv.inl into:
#   src/NKContext/Renderer/Backends/Vulkan/NkRenderer2DVkSpv.inl
# =============================================================================

import argparse
import os
import shutil
import struct
import subprocess
import sys


def find_compiler(prefer: str) -> str | None:
    """Find glslangValidator or glslc on PATH or in VULKAN_SDK."""
    candidates = [prefer, "glslangValidator", "glslc"]
    sdk = os.environ.get("VULKAN_SDK", "")
    if sdk:
        bin_dir = os.path.join(sdk, "Bin")
        candidates = [
            os.path.join(bin_dir, c) for c in candidates
        ] + candidates

    for c in candidates:
        if shutil.which(c):
            return c
    return None


def compile_shader(compiler: str, src: str, dst: str) -> bool:
    """Compile a GLSL shader to SPIR-V using glslangValidator or glslc."""
    if "glslc" in os.path.basename(compiler):
        cmd = [compiler, src, "-o", dst]
    else:
        # glslangValidator
        cmd = [compiler, "-V", src, "-o", dst]

    print(f"  Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  ERROR: {result.stderr or result.stdout}", file=sys.stderr)
        return False
    return True


def spv_to_c_array(spv_path: str, array_name: str) -> str:
    """Convert a .spv binary file to a C uint32 array declaration."""
    with open(spv_path, "rb") as f:
        data = f.read()

    if len(data) % 4 != 0:
        raise ValueError(f"SPIR-V size {len(data)} is not a multiple of 4")

    words = struct.unpack(f"<{len(data)//4}I", data)

    # Validate SPIR-V magic
    SPIRV_MAGIC = 0x07230203
    if not words or words[0] != SPIRV_MAGIC:
        raise ValueError(f"Invalid SPIR-V magic: 0x{words[0]:08X}" if words else "Empty file")

    # Format as C array
    count = len(words)
    lines = [f"static const uint32_t {array_name}[] = {{"]
    for i in range(0, count, 8):
        chunk = words[i:i+8]
        lines.append("    " + ", ".join(f"0x{w:08X}u" for w in chunk) + ",")
    lines.append("};")
    lines.append(f"static const uint32_t {array_name}WordCount = {count}u;")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Compile 2D renderer shaders to SPIR-V .inl")
    parser.add_argument("--compiler", default="glslangValidator",
                        help="Shader compiler to use (glslangValidator or glslc)")
    parser.add_argument("--shader-dir", default=os.path.dirname(__file__),
                        help="Directory containing .vert/.frag sources")
    parser.add_argument("--out", default=os.path.dirname(__file__),
                        help="Output directory for .spv files and .inl")
    args = parser.parse_args()

    shader_dir = os.path.abspath(args.shader_dir)
    out_dir    = os.path.abspath(args.out)
    os.makedirs(out_dir, exist_ok=True)

    # Locate compiler
    compiler = find_compiler(args.compiler)
    if not compiler:
        print(
            "ERROR: No GLSL→SPIR-V compiler found.\n"
            "Install the Vulkan SDK (https://vulkan.lunarg.com/) or glslang:\n"
            "  Windows: install Vulkan SDK, add %VULKAN_SDK%\\Bin to PATH\n"
            "  Linux:   sudo apt install glslang-tools\n"
            "  macOS:   brew install glslang",
            file=sys.stderr,
        )
        sys.exit(1)
    print(f"Using compiler: {compiler}")

    shaders = [
        ("nkrenderer2d.vert", "nkrenderer2d_vert.spv", "kVk2DVertSpv"),
        ("nkrenderer2d.frag", "nkrenderer2d_frag.spv", "kVk2DFragSpv"),
    ]

    arrays = []
    for src_name, spv_name, array_name in shaders:
        src_path = os.path.join(shader_dir, src_name)
        spv_path = os.path.join(out_dir, spv_name)

        if not os.path.exists(src_path):
            print(f"ERROR: Source not found: {src_path}", file=sys.stderr)
            sys.exit(1)

        print(f"Compiling {src_name} → {spv_name}")
        if not compile_shader(compiler, src_path, spv_path):
            sys.exit(1)

        arrays.append(spv_to_c_array(spv_path, array_name))
        print(f"  OK ({os.path.getsize(spv_path)} bytes)")

    # Write .inl
    inl_path = os.path.join(out_dir, "NkRenderer2DVkSpv.inl")
    inl_content = """\
#pragma once
// =============================================================================
// NkRenderer2DVkSpv.inl — AUTO-GENERATED by gen_spv.py
// DO NOT EDIT MANUALLY — regenerate with:
//   python scripts/gen_spv.py
// =============================================================================
// GLSL source: shaders/nkrenderer2d.vert + nkrenderer2d.frag
// Compiler:    {compiler}
// =============================================================================

""".format(compiler=compiler)

    inl_content += "\n\n".join(arrays) + "\n"

    with open(inl_path, "w") as f:
        f.write(inl_content)

    print(f"\nGenerated: {inl_path}")
    print("\nCopy this file to:")
    print("  src/NKContext/Renderer/Backends/Vulkan/NkRenderer2DVkSpv.inl")


if __name__ == "__main__":
    main()