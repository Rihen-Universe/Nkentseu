# NKWindow — Roadmap

État actuel (mai 2026) : Façade `NkWindow` cross-plateforme stable sur Win32,
XLib, XCB, Wayland, Cocoa, UIKit et Android. UWP, Xbox, Emscripten et Noop
sont câblés mais à des stades de maturité variables. **HarmonyOS** : backend
natif réel (XComponent + display_manager, ~1960 LOC) désormais compilé par le
build (corrigé 2026-05-28 — voir Bugs corrigés). 13 backends platform,
14 entry points (nkmain), ~9500 LOC d'implémentation native.

**2026-05-29 — Hot-plug/DPI Phase 2 LIVRÉE** sur les 9 backends restants :
- **XLib + XCB** : XRandR réel (XRRGetScreenResources/XRRGetCrtcInfo/XRRGetOutputInfo) + hot-plug `RRScreenChangeNotify` → `NkSystemDisplayEvent`
- **Wayland** : `wl_output` multi-écran réel + geometry/mode/done listeners + hot-plug ADDED/REMOVED (note : `wl_output.scale` entier, scaling fractionnel `wp_fractional_scale_v1` non géré)
- **Cocoa** : `[NSScreen screens]` + `CGDisplayCopyDisplayMode` + observer `NSApplicationDidChangeScreenParameters`
- **UIKit** : `[UIScreen screens]` + observers `UIScreenDidConnect/Disconnect` (avec `@available` guards)
- **Android** : `AConfiguration_getDensity` (dpiScale = density/160) + helper JNI `NkAndroidQueryRefreshRate`
- **Emscripten** : `emscripten_get_device_pixel_ratio` + window.screen
- **HarmonyOS** : `OH_NativeDisplayManager_GetDefaultDisplay*` (guardé) + event DPI émis dans `OnSurfaceCreated` quand scale change
- **Noop** : moniteur factice 1920x1080@60 DPI 1.0

API consolidée sur `NkWindow` : `EnumerateMonitors()`, `GetCurrentMonitor()`, `GetMonitorCount()` retournant `NkDisplayInfo` ; events `NkSystemDisplayEvent` (ADDED/REMOVED/RESOLUTION_CHANGED) et `NkWindowDpiEvent`.

**2026-05-29 — Harmony stubs pour débloquer le build** : `NkHarmonyGamepad` (.h+.cpp) réécrits en stub aligné sur l'interface `NkIGamepad` (signatures `Init/Shutdown/Poll/GetConnectedCount/GetSnapshot/Rumble/GetName` correctes, snapshot vide, no-op Rumble). `NkHarmonyEventSystem.cpp` stubbé (le header SDK `native_xcomponent_event.h` a été retiré dans les versions récentes d'OHOS, plus les constantes `KEY_*` manquantes et les types `NkTouch{Began,Moved,Ended,Cancelled}Event` obsolètes accumulés). Le `.h` (data struct `NkEventSystemData` avec `OH_NativeXComponent*`) est conservé. `NkHarmonyWindow.cpp` : `d.nativeWindow` → `d.ohNativeWindow` (membre correct de `NkSurfaceDesc` pour Harmony, type `OHNativeWindow*`). **À refaire** : pipeline XComponent réel (touch/mouse/key) avec OH_Input quand un device sera connecté pour test.

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| Architecture core (`NkWindow`, `NkWindowConfig`, `NkSurfaceDesc`) | Livré | — | — |
| Cycle de vie global (`NkWESystem`, `NkInitialise`/`NkClose`) | Livré | — | — |
| Registre fenêtres (`NkWindowId` + `NkWESystem`) | Livré | — | — |
| Entry point `nkmain` (13 plateformes) | Livré | — | — |
| Backend Win32 | Livré | — | — |
| Backend XLib | Livré | — | — |
| Backend XCB | Livré | — | — |
| Backend Wayland (xdg-shell + SSD) | Livré | — | — |
| Backend Cocoa (macOS) | Livré | — | — |
| Backend UIKit (iOS) | Livré | — | — |
| Backend Android (NativeActivity) | Livré | — | — |
| Backend Emscripten (WASM/canvas) | Partiel | M | Moyenne |
| Backend UWP (CoreWindow) | Partiel | M | Basse |
| Backend Xbox (GDK fallback) | Partiel | L | Basse |
| Backend Noop (headless/fallback) | Livré | — | — |
| Drag & Drop natif | Partiel | M | Moyenne |
| Dialogs natifs (open/save/color/msg) | Partiel | M | Moyenne |
| Orientation écran + safe area mobile | Partiel | S | Moyenne |
| Multi-fenêtre | Livré | — | — |
| Hot-plug d'écran / DPI change runtime | Partiel | M | Moyenne |
| Tests unitaires plateformes | TODO | L | Moyenne |
| Wayland libdecor (CSD path) | TODO | M | Basse |
| WatchOS / tvOS entry points | TODO | L | Basse |

Légende : Livré (OK), Partiel (fonctionne mais manques), En cours, TODO,
Abandonné.

---

## Livré

### Phase A — Core API publique
- Façade `NkWindow` sans PIMPL : données natives intégrées via `NkWindowData`
  défini par chaque backend platform (voir
  [Core/NkWindow.h](src/NKWindow/Core/NkWindow.h)).
- Configuration unifiée `NkWindowConfig` :
  position, taille, contraintes min/max, fullscreen, modal, transparent,
  frame, icône, hints natifs (`NkNativeWindowOptions`),
  options mobile (`hideSystemUI`, `lockOrientation`, `respectSafeArea`),
  options web (`NkWebInputOptions`), surface hints OpenGL/GLX
  (`NkSurfaceHints` rempli par `NkContextFactory::PrepareWindowConfig`).
- Cycle de vie : `Create`, `Close`, `IsOpen`, `IsValid`.
- Manipulation : `SetSize`, `SetPosition`, `SetVisible`, `Minimize`,
  `Maximize`, `Restore`, `SetFullscreen`, `SetTitle`, `SetMousePosition`,
  `ShowMouse`, `CaptureMouse`, `ClipMouseToClient`, `SetProgress`.
- Orientation : `SetScreenOrientation`, `SetAutoRotateEnabled`,
  `SupportsOrientationControl` (no-op desktop, actif mobile).
- Safe Area : `GetSafeAreaInsets()` pour iOS/Android notch & gesture bar.
- Surface : `GetSurfaceDesc()` renvoie un `NkSurfaceDesc` natif conditionnel
  (hwnd/hinstance, Display+Window+screen, xcb_connection+window+screen,
  wl_display+wl_surface, NSView+CAMetalLayer, UIView+CAMetalLayer,
  ANativeWindow, canvasId, etc.) consommé par NKContext.

### Phase B — Système global `NkWESystem`
- Owner unique de `NkEventSystem` + `NkGamepadSystem` (plus de singleton
  séparé), voir [Core/NkWESystem.h](src/NKWindow/Core/NkWESystem.h).
- Registre fenêtres : `RegisterWindow` / `UnregisterWindow` / `GetWindow`,
  `NkWindowId` typé, attribution d'ID séquentiel via `mNextWindowId`.
- `NkInitialise(NkAppData)` / `NkClose()` idempotents.
- Sur Win32, `OleInitialize` est appelé ici une seule fois par processus
  (`mOleInitialised`) avant toute création de fenêtre ou de DropTarget.

### Phase C — Entry points `nkmain`
13 entry points platform dans [EntryPoints/](src/NKWindow/EntryPoints/) :
`NkWindowsDesktop.h`, `NkUWP.h`, `NkXbox.h`, `NkXLib.h`, `NkXCB.h`,
`NkWayland.h`, `NkCocoa.h`, `NkAppleMobile.h`/`NkUikit.h`, `NkAndroid.h`,
`NkEmscripten.h`, `NkNoob.h`, plus `NkMetalEntryPoint.mm` (303 LOC) et
`NkWatchOS.h` (stub).

- `NkEntryState` conditionnel par plateforme : `HINSTANCE`+`nCmdShow` sur
  Windows, `xcb_connection_t*`+`xcb_screen_t*` sur XCB, `Display*` sur XLib,
  `android_app*` sur Android, `void* uwpCoreWindow` sur UWP, etc.
- Helpers runtime : `NkBuildEntryAppData`, `NkRegisterEntryAppDataUpdater`,
  macros `NK_REGISTER_ENTRY_APPDATA_UPDATER(fn)` et
  `NKENTSEU_DEFINE_APP_DATA(data)` pour configurer `NkAppData` avant `nkmain`.
- Variable globale `gState` propre par TU (créée par l'entrypoint, détruite
  après retour de `nkmain`).

### Phase D — Matrice plateformes × features

| Feature                | Win32 | XLib | XCB | Wayland | Cocoa | UIKit | UWP | Xbox | Android | WASM | Noop |
|------------------------|:-----:|:----:|:---:|:-------:|:-----:|:-----:|:---:|:----:|:-------:|:----:|:----:|
| Create / Close         | OK   | OK  | OK | OK     | OK   | OK   | OK | OK  | OK     | OK  | OK  |
| Title / icône          | OK   | OK  | OK | OK     | OK   | n/a   | --- | --- | n/a    | OK  | OK  |
| SetSize / SetPosition  | OK   | OK  | OK | partiel| OK   | n/a   | --- | --- | n/a    | OK  | OK  |
| Resizable / Min/Max    | OK   | OK  | OK | OK     | OK   | n/a   | --- | --- | n/a    | partiel | OK |
| Fullscreen             | OK   | OK  | OK | OK     | OK   | n/a   | --- | --- | n/a    | OK  | OK  |
| Modal                  | OK   | OK  | OK | partiel| OK   | n/a   | --- | --- | n/a    | n/a  | OK  |
| Frame / hasShadow      | OK   | OK  | OK | OK SSD | OK   | n/a   | --- | --- | n/a    | n/a  | OK  |
| Transparent            | OK   | OK  | OK | OK     | OK   | n/a   | --- | --- | n/a    | OK  | OK  |
| DPI scale runtime      | OK   | OK  | OK | OK     | OK   | OK   | --- | --- | OK     | OK  | OK  |
| MousePos / Capture     | OK   | OK  | OK | partiel| OK   | n/a   | --- | --- | n/a    | OK  | OK  |
| ClipMouseToClient      | OK   | OK  | OK | --- *  | OK   | n/a   | --- | --- | n/a    | n/a  | OK  |
| ShowMouse              | OK   | OK  | OK | OK     | OK   | n/a   | --- | --- | n/a    | OK  | OK  |
| Drag & Drop fichiers   | OK OLE | OK XDND | OK XDND | partiel | OK | partiel | --- | --- | partiel | OK  | OK  |
| Multi-window           | OK   | OK  | OK | OK     | OK   | n/a   | --- | --- | n/a    | n/a  | OK  |
| External window handle | OK   | OK  | OK | partiel| OK   | OK   | OK | OK  | OK     | OK  | OK  |
| Surface OpenGL hints   | OK   | OK GLX | OK GLX | OK EGL | OK   | OK   | --- | --- | OK EGL | OK WebGL | OK |
| Surface Vulkan         | OK   | OK  | OK | OK     | OK MoltenVK | OK MoltenVK | n/a | n/a | OK | n/a | OK |
| Surface Metal          | n/a  | n/a  | n/a | n/a    | OK   | OK   | n/a | n/a | n/a    | n/a  | OK  |
| Surface Software       | OK GDI | OK XShm | OK xcb_image | OK SHM | --- | --- | --- | --- | --- | OK Canvas | OK |
| Safe area / orientation | n/a  | n/a  | n/a | n/a    | n/a  | OK   | n/a | n/a | OK     | n/a  | n/a  |
| HideSystemUI / Lock     | n/a  | n/a  | n/a | n/a    | n/a  | partiel | n/a | n/a | OK     | n/a  | n/a  |
| Native dialogs          | OK   | OK Zenity | OK Zenity | partiel | OK osascript | --- | --- | --- | --- | --- | stub |
| SetProgress (taskbar)   | OK ITaskbarList3 | --- | --- | --- | OK NSDockTile | --- | --- | --- | --- | --- | --- |
| Themes (dark/light)     | OK   | partiel | partiel | partiel | OK | OK   | --- | --- | partiel | partiel | --- |
| nkmain entry            | OK   | OK  | OK | OK     | OK   | OK   | OK | partiel | OK | OK | OK  |

Légende : OK = fonctionnel, partiel = câblé mais manques, --- = non
implémenté / non applicable, n/a = pas pertinent sur cette plateforme.

*Wayland ClipMouseToClient : pas d'API standard pour clip absolu, fallback
via `pointer-constraints-v1` à câbler.

### Phase D.1 — Backend Win32 (`Platform/Win32/`, 864 LOC `.cpp`)
- WndProc dédié + registre HWND → `NkWindow*` statique dans le `.cpp`
  (helpers `NkWin32FindWindow` / `NkWin32RegisterWindow`).
- Style + ExStyle dynamiques selon `frame`, `resizable`, `transparent`.
- DEVMODE pour fullscreen exclusif, fallback bordless plein écran.
- ITaskbarList3 pour `SetProgress`.
- IDropTarget (`NkWin32DropTarget.h`) + `DragAcceptFiles` fallback.
- Mouse tracking via `TrackMouseEvent` pour enter/leave.
- Subclassing (`mPrevWndProc`) pour external HWND.

### Phase D.2 — Backend Wayland (`Platform/Wayland/`, 1232 LOC `.cpp`)
Le plus gros backend en LOC. xdg-shell + xdg-decoration (SSD) +
fallback SHM buffer. Inclut les protocoles générés
(`xdg-shell-protocol.c`, `xdg-decoration-protocol.c`, headers
clients). Couvre :
- Connexion `wl_display`, registry, seats, outputs, scale tracking.
- xdg-toplevel : set_title, set_minimized, set_maximized,
  set_fullscreen, suggestions min/max.
- SSD via `zxdg_decoration_manager_v1` (path libdecor reste TODO via
  flag `NKENTSEU_WAYLAND_LIBDECOR`).
- Buffer SHM pour software backend (`shmPixels`, `shmBuffer`,
  `shmStride` dans `NkSurfaceDesc`).
- Curseur via `wl_cursor` + `xkbcommon` pour keyboard.

### Phase D.3 — Backend XCB (`Platform/XCB/`, 844 LOC `.cpp`)
- Connexion `xcb_connection_t` partagée entre toutes les fenêtres
  (refcount via `sWindowCount`).
- Atoms cachés (WM_DELETE_WINDOW, WM_PROTOCOLS, _NET_WM_NAME, UTF8_STRING).
- Registre `xcb_window_t` → `NkWindow*` (function-local static map).
- XDND v5 pour drag & drop, ICCCM hints (min/max size, fullscreen).
- `NkSpinLock` pour la connexion partagée.

### Phase D.4 — Backend XLib (`Platform/XLib/`, 753 LOC `.cpp`)
Similaire à XCB mais via `Display*` + `XEvent`. GLX-friendly
(`GlxFBConfigPtr` transporté dans `NkSurfaceHints`). XShm pour software
backend.

### Phase D.5 — Backend Cocoa (`Platform/Cocoa/`, 682 LOC `.mm`)
- `NkCocoaWindowDelegate` Objective-C++ qui capture
  `windowWillStartLiveResize` / `windowDidResize` / focus / move.
- `NSWindow` + `NSView` + `CAMetalLayer` créé automatiquement.
- backingScaleFactor honoré pour DPI runtime.

### Phase D.6 — Backend UIKit (`Platform/UIKit/`, 616 LOC `.mm`)
- `NkUIKitTouchView` UIView intercepte touches → `NkTouchEvent` (multi-touch
  jusqu'à `NK_MAX_TOUCH_POINTS`).
- `CAMetalLayer` configuré pour rendu Metal/MoltenVK.
- Safe area lue depuis `UIWindow.safeAreaInsets`.

### Phase D.7 — Backend Android (`Platform/Android/`, 778 LOC `.cpp`)
- `android_app*` global (`nk_android_global_app`) partagé par l'entrypoint.
- `ANativeWindow` cycle de vie (APP_CMD_INIT_WINDOW / TERM_WINDOW) avec
  notification au contexte GL/Vulkan via `NkOpenGLContext::RecreateSurface`.
- JNI pour lock orientation / hide system UI / safe area / orientation
  events.
- Multi-window non pertinent (Android = 1 fenêtre par activity).

### Phase D.7b — Backend HarmonyOS (`Platform/HarmonyOS/`, ~1960 LOC) ⭐ 2026-05-28
- `NkHarmonyWindow.cpp` (1097 LOC), `NkHarmonyEventSystem.cpp` (582 LOC),
  `NkHarmonyGamepad.cpp` (281 LOC) + `NkHarmonyBridge.ts` (pont ArkTS).
- APIs natives OpenHarmony : `ace/xcomponent/native_xcomponent.h` (surface GL),
  `display_manager/oh_display_manager.h` (guard `NK_HARMONY_HAS_DISPLAY_API`),
  hilog. Supporte phone / tablet / PC 2in1 + orientation + safe area + IME.
- Toolchain `ohos-ndk` (triple `aarch64-linux-ohos`, `-D__OHOS__`).
  Détecté via `__OHOS__`/`__HARMONY__` dans NkPlatformDetect.h.
- EntryPoint `EntryPoints/NkHarmonyOS.h` + `Core/EntryAbility.ts`.
- ⏳ Reste à valider : build réel avec SDK OpenHarmony, packaging `.hap`
  via hvigor, surface EGL (`NKENTSEU_HAS_HARMONY_EGL`).

### Phase D.8 — Backend Noop (`Platform/Noop/`, 267 LOC `.cpp`)
- Fenêtre virtuelle headless complète. Émet quand même `NkWindowCreateEvent`
  + `NkWindowShownEvent` pour les tests automatisés.
- Activé par `NKENTSEU_FORCE_WINDOWING_NOOP_ONLY` ou en fallback si aucune
  plateforme reconnue.

### Phase D.9 — Backends partiels

#### UWP (`Platform/UWP/`, 271 LOC `.cpp`)
- `CoreWindow*` récupéré depuis l'entrypoint (`NkUWPIsCoreWindowReady`,
  `NkUWPGetCoreWindowHandle`, `NkUWPPumpSystemEvents`).
- Manques : pas de gestion thème / orientation / DPI runtime, drag&drop
  via DataTransfer non câblé, dialogs natifs `--- stub` uniquement.

#### Xbox (`Platform/Xbox/`, 449 LOC `.cpp`)
- Fallback `NkCreateFallbackXboxWindow` (WNDCLASSEXW) quand l'entrypoint
  GDK ne fournit pas de native window. Sert pour le dev desktop.
- `NkXboxIsNativeWindowReady` / `NkXboxGetNativeWindowHandle` /
  `NkXboxPumpSystemEvents`.
- Manques : intégration GDK GameInput + GamingDeviceInformation absente,
  pas testé sur kit Xbox réel.

#### Emscripten (`Platform/Emscripten/`, 762 LOC `.cpp`)
- Canvas selector (`#canvas` par défaut), polling viewport via
  `EM_ASM_INT` (window.innerWidth/clientWidth fallback).
- HTML5 events câblés (key, mouse, touch, focus, resize).
- Manques : fullscreen API moderne (`requestFullscreen` avec keyboard
  capture), pointer lock partiel, pas de `SetProgress`.

### Phase E — Dialogs natifs
- API : `NkDialogs::OpenFileDialog`, `SaveFileDialog`, `OpenMessageBox`,
  `ColorPicker` ([Core/NkDialogs.h](src/NKWindow/Core/NkDialogs.h)).
- Backends : Windows GetOpenFileNameW/MessageBoxW/ChooseColor, Linux via
  `zenity` (popen), macOS via `osascript`.
- Manques : pas de variant Wayland natif (passe par zenity), pas d'iOS
  (UIDocumentPickerViewController à câbler), pas d'Android (Intent
  ACTION_OPEN_DOCUMENT à câbler), pas de WASM (file input HTML).

### Phase F — Gamepad (système séparé mais owned ici)
- `NkGamepadSystem` instancié dans `NkWESystem` (1 par processus).
- Backends : `NkWin32Gamepad.h` (XInput + DirectInput HID), `NkCocoaGamepad`,
  `NkUIKitGamepad`, `NkAndroidGamepad`, `NkEmscriptenGamepad`,
  `NkUWPGamepad`, `NkXboxGamepad`, `NkLinuxGamepadBackend.h` (evdev),
  `NkNoopGamepad`.

---

## En cours / TODO immédiat

### Wayland — finition
- **libdecor (CSD path)** : actuellement le code détecte libdecor via
  `__has_include` mais la branche `NKENTSEU_HAS_LIBDECOR == 1` n'est pas
  exercée sur les compositeurs sans SSD (GNOME wayland récent). Câblage
  complet ~4-6h.
- **pointer-constraints-v1** : implémenter `ClipMouseToClient` via
  `zwp_locked_pointer_v1` + `zwp_pointer_constraints_v1`. ~2h.
- **relative-pointer-v1** : raw mouse motion (FPS) sur Wayland.
- **fractional-scale-v1** : DPI 1.25x/1.5x sans arrondi entier.
- **SetPosition()** : actuellement no-op (Wayland refuse les coordonnées
  absolues client-side) — documenter explicitement le no-op + envisager
  `xdg-positioner` pour les fenêtres modales.

### Emscripten — modernisation
- **requestFullscreen avec keyboard capture** (KeyboardLock API).
- **Pointer Lock** complet (`emscripten_request_pointerlock` avec
  filtrage strict).
- **DPI dynamique** quand `window.devicePixelRatio` change (zoom navigateur).
- **Resize observer** au lieu de polling viewport.
- **PWA fullscreen mode** pour usage standalone.

### UWP / Xbox — kit réel
- Tester réellement sur Xbox dev kit (GDK 2024+) — actuellement seulement
  le fallback desktop est validé.
- Câbler `Windows.Gaming.Input` (gamepads système) pour remplacer
  XInput sur Xbox.
- Drag & drop UWP via `Windows.ApplicationModel.DataTransfer`.

### Drag & Drop — uniformisation
- **Android** : ContentResolver + Intent ACTION_DROP via JNI (actuellement
  `NkAndroidDropTarget.h` est un stub minimal).
- **iOS** : `UIDropInteraction` + delegate.
- **Wayland** : `wl_data_device_manager` (XDND mais pour Wayland).
- **Emscripten** : HTML5 Drag&Drop API + ondrop sur canvas (partiellement
  câblé).

### Dialogs — comblement
- **Wayland natif** : `xdg-desktop-portal` (FileChooser, OpenURI) via
  D-Bus au lieu de zenity.
- **iOS / Android / WASM** : pickers natifs (cf. ci-dessus).
- **Color picker** UWP / Cocoa : actuellement zenity-only sous Linux.

### Tests
- `tests/test_smoke.cpp` + `tests/benchmark_smoke.cpp` sont les seuls tests.
  Manque :
  - Tests par plateforme (au moins Win32 + Linux XCB + Wayland en CI).
  - Tests multi-window (create 4 windows, verify event routing).
  - Tests resize lifecycle (begin/end resize, focus transfer).
  - Tests external window handle (parent HWND embedding).
  - Benchmark création/destruction 1000 fenêtres sans leak.

---

## À venir / À ajouter (futur proche)

### WatchOS / tvOS / VisionOS
- `EntryPoints/NkWatchOS.h` est un stub (82 LOC) sans backend platform
  associé. Câbler `WKExtensionDelegate` (watchOS), `UIApplicationDelegate`
  (tvOS via UIKit), `CompositorServices` (visionOS).

### Linux : `NkLinuxGamepadBackend` complet
- `Platform/Linux/` ne contient que `NkLinuxGamepadBackend.h` (header
  uniquement). L'implémentation evdev complète manque.

### High-refresh / VRR
- `SetRefreshRate(hz)` API.
- Détecter VRR (FreeSync/G-Sync) capabilities via DXGI/XRandR/Wayland
  `presentation-time`.
- Présent timing helpers pour 120/144/240 Hz games.

### Multi-display
- API explicite `GetDisplayCount`, `GetDisplayBounds(idx)`,
  `GetDisplayDPI(idx)`. Actuellement `GetDisplaySize`/`GetDisplayPosition`
  retournent seulement l'écran courant.
- Event `NK_SYSTEM_DISPLAY` (déjà déclaré dans `NkEventType`) à émettre
  réellement sur hot-plug d'écran.

### Window management avancé
- Set window opacity dynamique (déjà câblé sur Win32 via
  `LWA_ALPHA`, à uniformiser).
- Stick to desktop / always-on-top runtime (`WS_EX_TOPMOST` toggle).
- Snap to edges hint (Windows 11 Snap Layouts).
- Tablet mode detection (Win32 `GetSystemMetrics(SM_CONVERTIBLESLATEMODE)`).

### Theming
- Dark/light mode runtime sur toutes les plateformes (déjà OK Win32 +
  Cocoa, partiel ailleurs).
- Accent color system (Win32 DwmGetColorizationColor).
- High-contrast / accessibility detection.

### IME (Input Method Editor)
- IME natif complet pour CJK : Windows IMM32, macOS NSTextInputContext,
  Wayland text-input-v3. Actuellement seules les frappes UTF-8 sont
  remontées, pas la composition pré-commit.

### Refactor `NkSurfaceDesc`
- Actuellement le contenu varie par plateforme via `#if defined()`.
  Envisager un wrapper opaque `NkSurfaceHandle` + accessors typés pour
  réduire les includes natifs dans le header public.

---

## Bugs / quirks connus
- **Wayland sans SSD ni libdecor** : la fenêtre s'affiche sans bordures
  (pas de close button). Workaround actuel : forcer libdecor à la compil
  ou afficher un faux frame en software via le SHM buffer.
- **XCB GLX** : nécessite XGetXCBConnection() côté NKContext, non auto-câblé
  par NkWindow (l'application doit lier xcb-glx).
- **External window handle Wayland** : pas vraiment supporté — Wayland ne
  permet pas le re-parenting cross-process classique. Documenté dans
  `NkNativeWindowOptions`.
- **Emscripten resize** : polling Manuel (~60Hz) au lieu de ResizeObserver
  natif → ~16ms de latence sur le redimensionnement.

## Bugs corrigés récemment

- **HarmonyOS jamais compilé + includes cassés (2026-05-28)** : le backend natif
  HarmonyOS (~1960 LOC, complet) existait mais (1) le filtre `system:HarmonyOS`
  du jenga forçait `NKENTSEU_FORCE_WINDOWING_NOOP_ONLY` + compilait les fichiers
  `Platform/Noop/*` → le vrai backend n'était jamais buildé (HarmonyOS tournait
  en headless) ; (2) le dossier était `Platform/Harmony/` mais les 7 includes
  pointaient vers `Platform/HarmonyOS/` → compilation impossible. Fix : dossier
  renommé `Harmony/` → `HarmonyOS/`, filtre jenga réécrit (toolchain `ohos-ndk`,
  vrais fichiers backend, links `ace_ndk.z`/`hilog_ndk.z`/`native_window`/EGL/GLESv3,
  métadonnées HAP, `harmonyets`), + 3 branches HARMONYOS ajoutées dans
  `NkLauncher.cpp` (OpenURL/OpenFile/OpenFolder).

- **XInput statique → dynamique (2026-05-27)** : `NkWin32Gamepad.h` linkait
  statiquement `xinput.lib` → MinGW résolvait vers `XInput1_3.dll` (DirectX SDK
  June 2010, absent de Windows 10/11 vanilla) → exes crashaient au démarrage
  avec "XInput1_3.dll missing". Refactor : ajout du namespace
  `nkentseu::nk_xinput_dyn` (~70 LOC) qui charge la DLL au runtime via
  `LoadLibraryW` avec fallback `XInput1_4.dll → XInput1_3.dll → XInput9_1_0.dll`.
  Pattern repris d'UE5 / SDL / GLFW. Aucune lib XInput n'est plus linkée — le
  flag `"xinput"` a été retiré de 11 jengas du projet (NKWindow, NKContext,
  Pong, Sandbox, Songoo, Nogee, PV3DE, Pong copy, NkImageDemo, NKPA, Model).
  XInput1_4 est système Win 8+, XInput9_1_0 système Vista+. Si aucune DLL
  trouvée → wrappers retournent `ERROR_DEVICE_NOT_CONNECTED` → 0 manette
  XInput détectée mais l'app continue (DirectInput reste opérationnel).
  XInput9_1_0 n'expose pas `XInputGetBatteryInformation` → wrapper retourne
  `ERROR_NOT_SUPPORTED` traité comme "info indisponible".

---

## Dépendances
- **Couches en dessous (utilisées)** :
  - NKPlatform (détection compile-time, `NKENTSEU_PLATFORM_*` +
    `NKENTSEU_WINDOWING_*`, `NkCGXDetect`)
  - NKCore (types, atomics, traits)
  - NKContainers (`NkString`, `NkVector`, `NkUnorderedMap`, `NkSpinLock`)
  - NKMath (`NkVec2u`, `NkVec2f`, `NkFunctions`)
  - NKMemory (allocateurs, `NkUniquePtr`)
  - NKLogger (logs platform)
  - NKEvent (consommé pour émettre les `NkWindowEvent`)
- **Modules au-dessus qui en dépendent** :
  - NKEvent (couplage bidirectionnel via `NkWESystem` qui possède
    `NkEventSystem`)
  - NKContext (consomme `NkSurfaceDesc` via `window.GetSurfaceDesc()`)
  - NKRHI (consomme NKContext, transitivement NKWindow)
  - NKRenderer (au-dessus de NKContext/NKRHI)
  - NKUI, NKAudio (events keyboard/mouse/touch)
  - Nkentseu/Core (`Application` framework)
  - Unkeny (éditeur), PV3DE (patient virtuel)
