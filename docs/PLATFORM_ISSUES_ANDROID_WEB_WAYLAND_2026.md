# Nkentseu Platform Issues Recap (Android, Web, Wayland)

Date: 2026-03-10

## Scope
This document summarizes the concrete issues seen on:
1. Android
2. Web (Emscripten)
3. Linux Wayland

It also captures what was changed, and how to avoid regressions.

## Android
### Symptoms
1. Native crash very early after app start.
2. Crash traces looked like event registration problems, but were inconsistent.
3. Project logs were not clearly visible in `adb logcat`.

### Root cause chain
1. The observed crash point in stack traces was misleading.
2. The real failure was on the Android startup path, before stable `nkmain` execution.
3. Lack of reliable platform-native logging made the crash location hard to isolate.

### Fixes applied
1. Added step-by-step startup instrumentation (`BOOTSTEP`) around initialization.
2. Updated Android logging path to use Android native log output (`logcat`) instead of generic print-only flow.
3. Removed ambiguity around startup sequence by validating the last reached step before crash.
4. Callback/event-function handling was stabilized (migration from fragile `std::function` usage in the hot path to project-native functional wrappers where required by the runtime path).

### Prevention
1. On Android, always route engine logs to `__android_log_print` (or Android sink equivalent).
2. Keep a minimal startup checkpoint sequence in place for debug builds.
3. When a trace says crash in utility/event code, validate startup entry path first before rewriting event logic.

## Web (Emscripten)
### Symptoms
1. Page loaded, status reached `Running`, but rendered output stayed black.
2. Browser console showed `Uncaught (in promise) Object`.
3. Main loop behavior differed from known good samples (25/27).

### Root cause chain
1. Platform macro split (`WEB` vs `WASM`) created backend-selection inconsistencies.
2. Naming/layout drift in platform files increased risk of wrong codepath inclusion.
3. Some console promise errors were noisy and not always the primary rendering blocker.

### Fixes applied
1. Unified platform macro usage to `NKENTSEU_PLATFORM_EMSCRIPTEN`.
2. Replaced legacy Web/WASM naming usage in Nkentseu paths and conditionals.
3. Standardized platform folder and file naming to `Emscripten`.
4. Kept runner scripts (`.bat` and `.sh`) as the canonical launch path for generated Web targets.
5. Verified successful Web startup logs through the full boot sequence.

### Prevention
1. Keep one macro for one platform family (`EMSCRIPTEN`) across all modules.
2. Do not mix historical aliases (`WEB`, `WASM`) in new code.
3. Use generated runner script first when debugging launch/runtime issues.
4. Always compare startup path with a known working sample when output is black.

## Wayland (window decorations)
### Symptoms
1. Window opened on Wayland but titlebar/decorations were missing on some compositors.
2. Existing SSD request path worked only when compositor supported `xdg-decoration`.

### Root cause
1. Wayland decorations are compositor-dependent.
2. If `zxdg_decoration_manager_v1` is missing or SSD is refused, client-side decoration fallback is required.

### Fixes applied
1. Kept SSD path (`xdg-decoration-unstable-v1`) for compositors that support it.
2. Added optional `libdecor` integration as CSD fallback in `NkWaylandWindow`:
   - frame decoration setup
   - configure callback driven size updates
   - proper frame/context cleanup
3. Added `libdecor` dispatch handling in Wayland event pump.
4. Added optional link detection via `pkg-config` in build scripts:
   - links `decor-0` only when `libdecor-0` is available
5. Added Wayland dependency note update (`libdecor-0-dev`) in setup docs/scripts.

### Prevention
1. Treat Wayland SSD as optional capability, not guaranteed behavior.
2. Keep fallback path for CSD on compositors without SSD support.
3. Make optional dependencies auto-detected in build scripts to avoid hard build breaks.
4. Validate on at least two compositor families before marking Wayland UI behavior as fixed.

## Practical commands
Install Wayland deps (Ubuntu 22.04):
```bash
sudo apt update
sudo apt install -y libwayland-dev wayland-protocols libxkbcommon-dev libdecor-0-dev
```

Build Wayland sample:
```bash
cd Jenga/Exemples/Nkentseu
../../jenga.py build --target SandboxGamepadPS3 --platform Linux --linux-backend wayland --config Debug
```

Run Android logs:
```bash
adb logcat
```

## Current status
1. Android: startup path stabilized and debug visibility improved with logcat-native logs.
2. Web: startup and rendering path now reaches running state with consistent Emscripten platform selection.
3. Wayland: SSD path preserved, CSD fallback integrated when `libdecor` is available.
