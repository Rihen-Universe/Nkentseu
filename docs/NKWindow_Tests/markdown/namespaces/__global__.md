# 🗂️ Namespace `Global`

> 86 éléments

[🏠 Accueil](../index.md) | [🗂️ Namespaces](./index.md)

## Éléments

### 🏛️ Classs (9)

- **[`EventSystem`](../files/NkEventSystem.h.md#eventsystem-eventsystem)** — Central event dispatcher and polling queue. * EventSystem aggregates platform event implementations, dispatches global and typed callbacks, and optionally exposes a queue API via PollEvent().
- **[`INkGamepadBackend`](../files/NkGamepadSystem.h.md#inkgamepadbackend-inkgamepadbackend)** — Platform backend interface for gamepad polling and control.
- **[`NkClock`](../files/NkClock.h.md#nkclock-nkclock)** — Cross-platform steady clock utilities. * NkClock centralizes time acquisition and sleep/yield primitives. On WebAssembly, Sleep/Yield use emscripten cooperative APIs.
- **[`NkDuration`](../files/NkDuration.h.md#nkduration-nkduration)** — Immutable duration value stored in nanoseconds. * NkDuration is the base time unit used by NkClock and NkStopwatch. It supports integer and floating-point factories, conversions, and arithmetic operations.
- **[`NkGamepadSystem`](../files/NkGamepadSystem.h.md#inkgamepadbackend-nkgamepadsystem-nkgamepadsystem)** — Cross-platform gamepad system facade. * PollGamepads() updates backend state, emits callbacks and injects NK_GAMEPAD_* events into the EventSystem queue.
- **[`NkGamepadSystem`](../files/NkGamepadSystem.h.md#nkgamepadsystem-nkgamepadsystem-nkgamepadsystem)** — Gyro/accéléromètre disponible ?
- **[`NkRenderer`](../files/NkRenderer.h.md#nkrenderer-nkrenderer)** — Public rendering facade independent from backend implementation. * NkRenderer delegates rendering work to INkRendererImpl and exposes backend-agnostic frame lifecycle and pixel operations.
- **[`NkStopwatch`](../files/NkStopwatch.h.md#nkstopwatch-nkstopwatch)** — Simple elapsed-time accumulator. * NkStopwatch can be started/stopped multiple times while preserving accumulated elapsed time.
- **[`Window`](../files/NkWindow.h.md#window-window)** — Cross-platform window facade. * Window wraps platform implementations (Win32/X11/Wayland/Cocoa/UIKit/...) and exposes a unified API for lifecycle, sizing, input policy and native surface retrieval for rendering backends.

### ⚙️ Functions (23)

- **[`ClipPoint`](../files/NkSafeArea.h.md#clippoint)** — Clipe un point dans la zone sûre (returns false si hors zone)
- **[`ColorPicker`](../files/NkDialogs.h.md#colorpicker)** — Ouvre un sélecteur de couleur.
- **[`GetConnectedCount`](../files/NkGamepadSystem.h.md#getconnectedcount)** — Nombre de manettes actuellement connectées.
- **[`GetInfo`](../files/NkGamepadSystem.h.md#getinfo)** — Infos sur la manette à l'indice idx (0-based).
- **[`GetState`](../files/NkGamepadSystem.h.md#getstate)** — Snapshot complet de l'état courant.
- **[`IsPress`](../files/NkKeyboardEvents.h.md#ispress)** — < true = touche maintenue, l'OS génère des répétitions.
- **[`NkClose`](../files/NkSystem.h.md#nkclose)** — Libère toutes les ressources du framework. Appeler avant la fin de nkmain() / main().
- **[`NkDisableDropTarget`](../files/NkDropSystem.h.md#nkdisabledroptarget)** — Désactive le support drag&drop.
- **[`NkEnableDropTarget`](../files/NkDropSystem.h.md#nkenabledroptarget)** — Active le support drag&drop sur la fenêtre donnée (handle natif).
- **[`NkGetEventImpl`](../files/NkSystem.h.md#nkgeteventimpl)** — Accède à l'implémentation d'événements active (plateforme courante).
- **[`NkInitialise`](../files/NkSystem.h.md#nkinitialise)** — Initialise le framework NkWindow. Doit être appelé une fois en début de programme (ou dans nkmain()). *
- **[`NkScancodeFromDOMCode`](../files/NkScancode.h.md#nkscancodefromdomcode)** — Convertit un DOM KeyboardEvent.code (WASM) vers NkScancode USB HID
- **[`NkScancodeFromLinux`](../files/NkScancode.h.md#nkscancodefromlinux)** — Convertit un keycode Linux evdev (XCB/XLib) vers NkScancode USB HID
- **[`NkScancodeFromMac`](../files/NkScancode.h.md#nkscancodefrommac)** — Convertit un keyCode macOS (NSEvent.keyCode) vers NkScancode USB HID
- **[`NkScancodeFromWin32`](../files/NkScancode.h.md#nkscancodefromwin32)** — Convertit un scancode Win32 Set-1 (MAPVK_VK_TO_VSC) vers NkScancode USB HID
- **[`NkScancodeToKey`](../files/NkScancode.h.md#nkscancodetokey)** — Convertit NkScancode vers NkKey (position US-QWERTY invariante)
- **[`NkScancodeToString`](../files/NkScancode.h.md#nkscancodetostring)** — Nom lisible du scancode (ex: "SC_A", "SC_SPACE")
- **[`OpenFileDialog`](../files/NkDialogs.h.md#openfiledialog)** — Ouvre un dialogue de sélection de fichier.
- **[`Rumble`](../files/NkGamepadSystem.h.md#rumble)** — Lance une vibration. Implémentation peut ignorer si non supporté.
- **[`SaveFileDialog`](../files/NkDialogs.h.md#savefiledialog)** — Ouvre un dialogue de sauvegarde de fichier.
- **[`Shutdown`](../files/IEventImpl.h.md#shutdown)** — Appelé par IWindowImpl::Close() avant destruction du handle.
- **[`ToNkDuration`](../files/NkClock.h.md#tonkduration)** — Convert std::chrono duration to NkDuration.
- **[`UsableWidth`](../files/NkSafeArea.h.md#usablewidth)** — Surface utilisable (en pixels)

### 🔧 Methods (35)

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

### 🏗️ Structs (7)

- **[`NkFramebufferInfo`](../files/NkSurface.h.md#nkframebufferinfo-nkframebufferinfo)** — Selected backend API.
- **[`NkFramebufferInfo`](../files/NkSurface.h.md#nkframebufferinfo-nkframebufferinfo-nkframebufferinfo)** — Color buffer pixel format.
- **[`NkFramebufferInfo`](../files/NkSurface.h.md#nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkframebufferinfo)** — Depth/stencil format.
- **[`NkFramebufferInfo`](../files/NkSurface.h.md#nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkframebufferinfo)** — Enable vertical synchronization.
- **[`NkFramebufferInfo`](../files/NkSurface.h.md#nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkframebufferinfo)** — Enable validation/debug layers when available.
- **[`NkFramebufferInfo`](../files/NkSurface.h.md#nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkframebufferinfo)** — When true, BeginFrame() automatically resizes the framebuffer if the
- **[`NkOpenGLRendererContext`](../files/NkSurface.h.md#nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkopenglrenderercontext-nkopenglrenderercontext)** — Software framebuffer exposed by software backend.

### 📦 Variables (12)

- **[`a`](../files/NkRenderer.h.md#nkrenderer-a)** — Active/désactive la présentation vers la fenêtre. Si désactivé, le renderer peut fonctionner en offscreen.
- **[`api`](../files/NkSurface.h.md#nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-api)** — Active backend API.
- **[`false`](../files/NkGamepadSystem.h.md#inkgamepadbackend-false)** — Pompe les événements gamepad et remplit les états internes.
- **[`framebuffer`](../files/NkSurface.h.md#nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-framebuffer)** — Build a portable runtime context for renderer backends.
- **[`mReady`](../files/NkGamepadSystem.h.md#nkgamepadsystem-nkgamepadsystem-mready)** — Initialize backend and internal state.
- **[`mReady`](../files/NkGamepadSystem.h.md#nkgamepadsystem-nkgamepadsystem-mready)** — Shutdown backend and clear state.
- **[`motorLow`](../files/NkGamepadSystem.h.md#nkgamepadsystem-nkgamepadsystem-motorlow)** — Device info for a connected gamepad index.
- **[`motorLow`](../files/NkGamepadSystem.h.md#nkgamepadsystem-nkgamepadsystem-motorlow)** — Snapshot state for a gamepad index.
- **[`nanoseconds`](../files/NkClock.h.md#nkclock-nanoseconds)** — Elapsed duration since
- **[`nanoseconds`](../files/NkClock.h.md#nkclock-nanoseconds)** — Sleep for a duration (cooperative on WASM).
- **[`title`](../files/NkDialogs.h.md#title)** — Affiche une boîte de message.
- **[`userData`](../files/NkSurface.h.md#nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-userdata)** — Optional user-owned backend data pointer.

