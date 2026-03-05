# 🔧 Methods

> 35 éléments

[🏠 Accueil](../index.md) | [🎯 Types](./index.md)

## Liste

- **[`AttachImpl`](../files/NkEventSystem.h.md#eventsystem-attachimpl)** — Lie une IEventImpl concrète (appelé par Window::Create). Plusieurs implémentations peuvent être liées simultanément (plusieurs fenêtres).
- **[`Create`](../files/NkWindow.h.md#window-create)** — Crée la fenêtre. NkInitialise() doit avoir été appelé avant.
- **[`DetachImpl`](../files/NkEventSystem.h.md#eventsystem-detachimpl)** — Détache une implémentation (appelé quand la fenêtre est fermée).
- **[`FromMicroseconds`](../files/NkDuration.h.md#nkduration-frommicroseconds)** — Build from microseconds.
- **[`FromMicroseconds`](../files/NkDuration.h.md#nkduration-frommicroseconds)** — Build from floating microseconds (rounded).
- **[`FromMilliseconds`](../files/NkDuration.h.md#nkduration-frommilliseconds)** — Build from milliseconds.
- **[`FromMilliseconds`](../files/NkDuration.h.md#nkduration-frommilliseconds)** — Build from floating milliseconds (rounded).
- **[`FromNanoseconds`](../files/NkDuration.h.md#nkduration-fromnanoseconds)** — Build from nanoseconds.
- **[`FromSeconds`](../files/NkDuration.h.md#nkduration-fromseconds)** — Build from seconds.
- **[`FromSeconds`](../files/NkDuration.h.md#nkduration-fromseconds)** — Build from floating seconds (rounded).
- **[`GetConnectedCount`](../files/NkGamepadSystem.h.md#nkgamepadsystem-nkgamepadsystem-getconnectedcount)** — Number of connected gamepads.
- **[`Init`](../files/NkGamepadSystem.h.md#inkgamepadbackend-nkgamepadsystem-init)** — Initialise le backend (ouvre les devices, enregistre les callbacks OS…)
- **[`Instance`](../files/NkGamepadSystem.h.md#nkgamepadsystem-nkgamepadsystem-instance)** — Access singleton instance.
- **[`IsConnected`](../files/NkGamepadSystem.h.md#nkgamepadsystem-nkgamepadsystem-isconnected)** — True if gamepad index is connected.
- **[`IsRunning`](../files/NkStopwatch.h.md#nkstopwatch-isrunning)** — True when stopwatch is currently running.
- **[`NkGetNativeDisplayHandle`](../files/NkSurface.h.md#nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkgetnativedisplayhandle)** — Return platform-native display/instance handle as opaque pointer.
- **[`NkGetNativeWindowHandle`](../files/NkSurface.h.md#nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkgetnativewindowhandle)** — Return platform-native window handle as opaque pointer.
- **[`Now`](../files/NkClock.h.md#nkclock-now)** — Current monotonic timestamp.
- **[`PollEvent`](../files/NkEventSystem.h.md#eventsystem-pollevent)** — Retourne le prochain événement de la queue, ou nullptr si vide. * Le pointeur est valide jusqu'au prochain appel de PollEvent ou PollEvents. Ne pas stocker ce pointeur. * Si la queue est vide, PollEvent() pompe automatiquement les événements (callbacks + queue) en interne. À utiliser seul (sans PollEvents() juste avant).
- **[`PollEvent`](../files/NkEventSystem.h.md#eventsystem-pollevent)** — Variante style SFML: copie le prochain événement dans
- **[`PollEvents`](../files/NkEventSystem.h.md#eventsystem-pollevents)** — Pompe les événements OS et exécute uniquement les callbacks. * Ce mode ne fournit pas de batch lisible par PollEvent(). Ne pas combiner avec while(PollEvent()) dans la même trame.
- **[`RegisterExternalRendererFactory`](../files/NkRenderer.h.md#nkrenderer-registerexternalrendererfactory)** — Register a user-defined renderer factory for a given API. * This lets users plug their own OpenGL/Vulkan/DirectX/Metal/WebGL backend implementation while reusing NkWindow surface creation.
- **[`RemoveEventCallback`](../files/NkEventSystem.h.md#eventsystem-removeeventcallback)** — Supprime le callback typé pour T.
- **[`Restart`](../files/NkStopwatch.h.md#nkstopwatch-restart)** — Reset and start immediately.
- **[`SetGlobalEventCallback`](../files/NkEventSystem.h.md#eventsystem-setglobaleventcallback)** — Callback reçoit TOUS les événements (avant la queue).
- **[`Shutdown`](../files/NkGamepadSystem.h.md#inkgamepadbackend-nkgamepadsystem-shutdown)** — Libère toutes les ressources.
- **[`SleepMilliseconds`](../files/NkClock.h.md#nkclock-sleepmilliseconds)** — Sleep for a number of milliseconds.
- **[`Start`](../files/NkStopwatch.h.md#nkstopwatch-start)** — Start timing if currently stopped.
- **[`ToMilliseconds`](../files/NkDuration.h.md#nkduration-tomilliseconds)** — Convert to milliseconds (integer truncation).
- **[`ToNanoseconds`](../files/NkDuration.h.md#nkduration-tonanoseconds)** — Convert to nanoseconds.
- **[`ToString`](../files/NkDuration.h.md#nkduration-tostring)** — Human-readable representation (ns/us/ms/s).
- **[`YieldThread`](../files/NkClock.h.md#nkclock-yieldthread)** — Yield execution to other tasks/threads.
- **[`Zero`](../files/NkDuration.h.md#nkduration-zero)** — Create a zero duration.
- **[`Zero`](../files/NkDuration.h.md#nkduration-zero)** — Create a duration from raw nanoseconds.
- **[`Zero`](../files/NkDuration.h.md#nkduration-zero)** — Return a zero duration constant.
