# 🏛️ Classs

> 9 éléments

[🏠 Accueil](../index.md) | [🎯 Types](./index.md)

## Liste

- **[`EventSystem`](../files/NkEventSystem.h.md#eventsystem-eventsystem)** — Central event dispatcher and polling queue. * EventSystem aggregates platform event implementations, dispatches global and typed callbacks, and optionally exposes a queue API via PollEvent().
- **[`INkGamepadBackend`](../files/NkGamepadSystem.h.md#inkgamepadbackend-inkgamepadbackend)** — Platform backend interface for gamepad polling and control.
- **[`NkClock`](../files/NkClock.h.md#nkclock-nkclock)** — Cross-platform steady clock utilities. * NkClock centralizes time acquisition and sleep/yield primitives. On WebAssembly, Sleep/Yield use emscripten cooperative APIs.
- **[`NkDuration`](../files/NkDuration.h.md#nkduration-nkduration)** — Immutable duration value stored in nanoseconds. * NkDuration is the base time unit used by NkClock and NkStopwatch. It supports integer and floating-point factories, conversions, and arithmetic operations.
- **[`NkGamepadSystem`](../files/NkGamepadSystem.h.md#inkgamepadbackend-nkgamepadsystem-nkgamepadsystem)** — Cross-platform gamepad system facade. * PollGamepads() updates backend state, emits callbacks and injects NK_GAMEPAD_* events into the EventSystem queue.
- **[`NkGamepadSystem`](../files/NkGamepadSystem.h.md#nkgamepadsystem-nkgamepadsystem-nkgamepadsystem)** — Gyro/accéléromètre disponible ?
- **[`NkRenderer`](../files/NkRenderer.h.md#nkrenderer-nkrenderer)** — Public rendering facade independent from backend implementation. * NkRenderer delegates rendering work to INkRendererImpl and exposes backend-agnostic frame lifecycle and pixel operations.
- **[`NkStopwatch`](../files/NkStopwatch.h.md#nkstopwatch-nkstopwatch)** — Simple elapsed-time accumulator. * NkStopwatch can be started/stopped multiple times while preserving accumulated elapsed time.
- **[`Window`](../files/NkWindow.h.md#window-window)** — Cross-platform window facade. * Window wraps platform implementations (Win32/X11/Wayland/Cocoa/UIKit/...) and exposes a unified API for lifecycle, sizing, input policy and native surface retrieval for rendering backends.
