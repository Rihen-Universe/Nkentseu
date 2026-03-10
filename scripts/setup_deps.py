#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
setup_deps.py — Interactive dependency installer for Nkentseu build system.

Usage:
    python3 scripts/setup_deps.py [--platform <platform>] [--check-only] [--yes]

Examples:
    python3 scripts/setup_deps.py                      # auto-detect platform, interactive
    python3 scripts/setup_deps.py --platform linux-xcb
    python3 scripts/setup_deps.py --check-only         # only check, don't install
    python3 scripts/setup_deps.py --yes                # auto-accept all installs

Platforms:
    linux-xlib      Linux with X11/XLib backend
    linux-xcb       Linux with XCB backend
    linux-wayland   Linux with Wayland backend
    linux-all       All Linux backends
    wasm            WebAssembly / Emscripten
    android         Android NDK
    windows         Windows (MinGW cross-compile from Linux)
"""

import argparse
import os
import platform
import shutil
import subprocess
import sys
from typing import Dict, List, NamedTuple, Optional


# ---------------------------------------------------------------------------
# Dependency declarations
# ---------------------------------------------------------------------------

class Dep(NamedTuple):
    pkg: str          # apt package name
    check: str        # command or file to probe existence (empty = use pkg name)
    desc: str         # human description

# Common build tools needed on all Linux targets
COMMON_LINUX: List[Dep] = [
    Dep("build-essential",  "gcc",          "GCC compiler + make"),
    Dep("clang",            "clang",        "Clang/LLVM compiler"),
    Dep("cmake",            "cmake",        "CMake build system"),
    Dep("ninja-build",      "ninja",        "Ninja build system"),
    Dep("git",              "git",          "Git version control"),
    Dep("python3",          "python3",      "Python 3 runtime"),
    Dep("curl",             "curl",         "HTTP client (for downloading SDKs)"),
]

LINUX_XLIB: List[Dep] = [
    Dep("libx11-dev",           "",     "X11 client library (XLib)"),
    Dep("libxrandr-dev",        "",     "X11 RandR extension (screen resolutions)"),
    Dep("libxinerama-dev",      "",     "X11 Xinerama (multi-monitor)"),
    Dep("libxcursor-dev",       "",     "X11 cursor management"),
    Dep("libxi-dev",            "",     "X11 input extension"),
    Dep("libgl-dev",            "",     "OpenGL development headers"),
    Dep("libglu1-mesa-dev",     "",     "GLU library"),
    Dep("libpthread-stubs0-dev","",     "POSIX thread stubs"),
]

LINUX_XCB: List[Dep] = [
    Dep("libxcb1-dev",          "",     "XCB base library"),
    Dep("libx11-xcb-dev",       "",     "XLib/XCB bridge"),
    Dep("libxcb-icccm4-dev",    "",     "XCB ICCCM extension (WM hints)"),
    Dep("libxcb-randr0-dev",    "",     "XCB RandR extension (screen resolutions)"),
    Dep("libxcb-keysyms1-dev",  "",     "XCB keysym lookup"),
    Dep("libxcb-xkb-dev",       "",     "XCB XKB extension"),
    Dep("libxcb-xfixes0-dev",   "",     "XCB XFixes extension"),
    Dep("libxcb-util-dev",      "",     "XCB utility library (xcb_aux.h)"),
    Dep("libxkbcommon-dev",     "",     "XKB common library (key name lookup)"),
    Dep("libxkbcommon-x11-dev", "",     "XKB X11 extension"),
    Dep("libgl-dev",            "",     "OpenGL development headers"),
]

LINUX_WAYLAND: List[Dep] = [
    Dep("libwayland-dev",       "",     "Wayland client/server library"),
    Dep("wayland-protocols",    "",     "Wayland protocol definitions (xdg-shell etc.)"),
    Dep("libwayland-egl1",      "",     "Wayland EGL integration"),
    Dep("libdecor-0-dev",       "",     "Wayland client-side decorations fallback (libdecor)"),
    Dep("libxkbcommon-dev",     "",     "XKB common library"),
    Dep("libxkbcommon-x11-dev", "",     "XKB X11 extension"),
    Dep("wayland-scanner",      "wayland-scanner", "Wayland protocol code generator"),
    Dep("libffi-dev",           "",     "FFI library (wayland dependency)"),
]

WASM: List[Dep] = [
    # Emscripten is installed via emsdk, not apt — listed for info only
    Dep("python3",          "python3",  "Python 3 (required by emsdk)"),
    Dep("cmake",            "cmake",    "CMake"),
    Dep("ninja-build",      "ninja",    "Ninja"),
    # nodejs for emscripten
    Dep("nodejs",           "node",     "Node.js (for Emscripten JS tools)"),
]

ANDROID: List[Dep] = [
    Dep("openjdk-17-jdk",   "java",     "Java 17 (Android build tool dependency)"),
    Dep("cmake",            "cmake",    "CMake"),
    Dep("ninja-build",      "ninja",    "Ninja"),
    Dep("unzip",            "unzip",    "Unzip (NDK/SDK extraction)"),
    Dep("wget",             "wget",     "wget (NDK/SDK download)"),
]

PLATFORM_DEPS: Dict[str, List[Dep]] = {
    "linux-xlib":    COMMON_LINUX + LINUX_XLIB,
    "linux-xcb":     COMMON_LINUX + LINUX_XCB,
    "linux-wayland": COMMON_LINUX + LINUX_WAYLAND,
    "linux-all":     COMMON_LINUX + LINUX_XLIB + LINUX_XCB + LINUX_WAYLAND,
    "wasm":          WASM,
    "android":       ANDROID,
}

# Extra post-install steps (called after apt install)
POST_INSTALL: Dict[str, List[str]] = {
    "linux-wayland": [
        "# Generate xdg-shell Wayland protocol files:",
        "XDG_XML=$(pkg-config --variable=pkgdatadir wayland-protocols)/stable/xdg-shell/xdg-shell.xml",
        "WAYLAND_SRC=Modules/Runtime/NKWindow/src/NKWindow/Platform/Wayland",
        "wayland-scanner client-header $XDG_XML $WAYLAND_SRC/xdg-shell-client-protocol.h",
        "wayland-scanner private-code  $XDG_XML $WAYLAND_SRC/xdg-shell-protocol.c",
    ],
}


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _green(s: str) -> str:
    return f"\033[32m{s}\033[0m" if sys.stdout.isatty() else s

def _red(s: str) -> str:
    return f"\033[31m{s}\033[0m" if sys.stdout.isatty() else s

def _yellow(s: str) -> str:
    return f"\033[33m{s}\033[0m" if sys.stdout.isatty() else s

def _bold(s: str) -> str:
    return f"\033[1m{s}\033[0m" if sys.stdout.isatty() else s


def is_installed(dep: Dep) -> bool:
    """Return True if the dependency is already satisfied."""
    probe = dep.check or dep.pkg
    # Check as command in PATH
    if shutil.which(probe):
        return True
    # Check via dpkg
    try:
        result = subprocess.run(
            ["dpkg", "-l", dep.pkg],
            capture_output=True, text=True
        )
        for line in result.stdout.splitlines():
            if line.startswith("ii") and dep.pkg in line:
                return True
    except FileNotFoundError:
        pass
    return False


def check_deps(deps: List[Dep]) -> tuple:
    """Return (missing, present) lists."""
    missing, present = [], []
    for dep in deps:
        if is_installed(dep):
            present.append(dep)
        else:
            missing.append(dep)
    return missing, present


def apt_install(packages: List[str], yes: bool) -> bool:
    """Run apt-get install. Returns True on success."""
    cmd = ["sudo", "apt-get", "install", "-y"] + packages
    if not yes:
        print(f"\n  Will run: {_bold(' '.join(cmd))}")
        ans = input("  Proceed? [Y/n] ").strip().lower()
        if ans not in ("", "y", "yes"):
            print("  Skipped.")
            return False
    try:
        subprocess.check_call(cmd)
        return True
    except subprocess.CalledProcessError as exc:
        print(_red(f"  apt-get failed: {exc}"))
        return False


def run_post_steps(platform_key: str) -> None:
    steps = POST_INSTALL.get(platform_key, [])
    if not steps:
        return
    print(f"\n{_bold('Post-install steps:')} (run these manually in the project root)")
    for step in steps:
        print(f"  {step}")


# ---------------------------------------------------------------------------
# Auto-detect
# ---------------------------------------------------------------------------

def auto_detect_platform() -> Optional[str]:
    if platform.system() != "Linux":
        return None
    # Ask user
    print("Auto-detect: Linux host detected.")
    print("Available backends:")
    choices = ["linux-xlib", "linux-xcb", "linux-wayland", "linux-all"]
    for i, c in enumerate(choices, 1):
        print(f"  {i}. {c}")
    val = input("Choose backend [1]: ").strip()
    if not val:
        return "linux-xlib"
    try:
        return choices[int(val) - 1]
    except (ValueError, IndexError):
        return val


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(description="Nkentseu dependency installer")
    parser.add_argument("--platform", "-p", help="Target platform (linux-xlib, linux-xcb, linux-wayland, linux-all, wasm, android)")
    parser.add_argument("--check-only", action="store_true", help="Only check, do not install")
    parser.add_argument("--yes", "-y", action="store_true", help="Auto-accept all installs")
    args = parser.parse_args()

    plat = args.platform
    if not plat:
        plat = auto_detect_platform()
    if not plat:
        print(_red("Could not determine platform. Use --platform <name>."))
        return 1
    if plat not in PLATFORM_DEPS:
        print(_red(f"Unknown platform '{plat}'. Valid: {', '.join(PLATFORM_DEPS)}"))
        return 1

    deps = PLATFORM_DEPS[plat]

    print(f"\n{_bold('=== Nkentseu Dependency Check ===')} — target: {_bold(plat)}")
    print()

    missing, present = check_deps(deps)

    for dep in present:
        print(f"  {_green('OK')}  {dep.pkg:<30} {dep.desc}")
    for dep in missing:
        print(f"  {_red('--')}  {dep.pkg:<30} {dep.desc}")

    print()
    print(f"  {len(present)} installed, {len(missing)} missing")

    if not missing:
        print(_green("\nAll dependencies are satisfied!"))
        run_post_steps(plat)
        return 0

    if args.check_only:
        print(_yellow("\n(check-only mode — nothing installed)"))
        return 1

    pkgs_to_install = [d.pkg for d in missing]
    print(f"\n{_bold('Missing packages:')} {' '.join(pkgs_to_install)}")

    ok = apt_install(pkgs_to_install, args.yes)

    # Re-check after install
    if ok:
        still_missing, _ = check_deps(missing)
        if still_missing:
            print(_yellow(f"\nStill missing after install: {[d.pkg for d in still_missing]}"))
            return 1
        else:
            print(_green("\nAll dependencies installed successfully!"))
            run_post_steps(plat)
            return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
