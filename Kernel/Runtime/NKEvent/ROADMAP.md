# NKEvent — Roadmap

État actuel (mai 2026) : Système d'événements typés mature avec hiérarchie
complète (Application, Window, Keyboard, Mouse, Touch, Gamepad, HID, Drop,
Transfer, System, Custom, Graphics) + dispatcher push/pull + ring buffer
dual-priorité. 28 fichiers, ~3-4k LOC. Stable et utilisé par NKWindow,
NKContext, NKUI, NKRenderer et Nkentseu/Core.

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| `NkEvent` base + RTTI léger (`Is<T>`/`As<T>`) | Livré | — | — |
| Hiérarchie `NkEventType` + `NkEventCategory` | Livré | — | — |
| Events Application (Launch/Tick/Update/Render/Close) | Livré | — | — |
| Events Window (Create/Close/Resize/Move/Focus/...) | Livré | — | — |
| Events Keyboard (Press/Repeat/Release/TextInput) | Livré | — | — |
| Events Mouse (Move/Raw/Button/Wheel/Scroll/Enter/Leave) | Livré | — | — |
| Events Touch + Gestures (Pinch/Rotate/Pan/Swipe/Tap) | Livré | — | — |
| Events Gamepad (Connect/Button/Axis/Stick/Rumble) | Livré | — | — |
| Events HID générique + mapper | Livré | — | — |
| Events Drop (File/Text/Image/Enter/Over/Leave) | Livré | — | — |
| Events System (Power/Locale/Display/Memory) | Partiel | M | Moyenne |
| Events Transfer + Custom + Graphics | Livré | — | — |
| `NkEventSystem` ring buffer dual-priorité | Livré | — | — |
| `NkEventDispatcher` (push) + `NkInputQuery` (pull) | Livré | — | — |
| `NkActionManager` / `NkAxisManager` (input bindings) | Livré | — | — |
| `NkGamepadSystem` cross-platform | Livré | — | — |
| `NkGamepadMappingPersistence` (SDL DB-like) | Livré | — | — |
| `NkDropSystem` (façade activation backend) | Livré | — | — |
| `NkEventState` snapshots polling | Livré | — | — |
| `NkCallbackGuard` RAII | Livré | — | — |
| Replay / record d'événements | TODO | M | Basse |
| Thread-safe enqueue cross-thread | Partiel | M | Moyenne |
| Tests unitaires complets | TODO | M | Haute |
| Système d'observateurs typés (observer pattern) | TODO | S | Basse |

Légende : Livré, Partiel, En cours, TODO, Abandonné.

---

## Livré

### Phase A — `NkEvent` core
- Classe de base polymorphe avec destructeur virtuel
  ([NkEvent.h](src/NKEvent/NkEvent.h)).
- RTTI léger via `GetStaticType()` / `Is<T>()` / `As<T>()` — pas de
  `dynamic_cast` requis, casting type-safe en O(1).
- Macros boilerplate `NK_EVENT_TYPE_FLAGS(type)` +
  `NK_EVENT_CATEGORY_FLAGS(cat)` + `NK_EVENT_BIND_HANDLER(method)`.
- Membres protégés : `mWindowID` (uint64), `mTimestamp` (uint64 ms),
  `mHandled` (bool).
- Méthodes : `MarkHandled` / `Unmark`, `IsValid`, `IsType`, `HasCategory`,
  `ToString`, `Clone` (copie polymorphe).
- Alias callbacks : `EventObserver`, `EventObserverRef`, `EventHandler`,
  `EventHandlerRef`, `NkEventCallback` (basé `NkFunction`).

### Phase B — Typologie complète
- **`NkEventType`** : ~70 valeurs réparties en groupes thématiques
  (App, Window, Keyboard, Mouse, Gamepad, HID, Touch, Drop, System,
  Transfer, Custom). Sentinelle finale `NK_EVENT_COUNT`.
- **`NkEventCategory`** : 13 flags bitfield (NONE, APPLICATION, INPUT,
  KEYBOARD, MOUSE, WINDOW, GRAPHICS, TOUCH, GAMEPAD, CUSTOM, TRANSFER,
  GENERIC_HID, DROP, SYSTEM, ALL).
- Opérateurs `|` `&` `~` `|=` `&=` définis pour combinaisons naturelles.
- Helpers : `NkCategoryHas`, `NkCategoryEmpty`, `NkCategoryFull`.
- Conversions string ↔ enum (`ToString` / `FromString`) pour debug et logs.

### Phase C — Events spécifiques

#### Application ([NkApplicationEvent.h](src/NKEvent/NkApplicationEvent.h))
- `NkAppLaunchEvent` (args CLI), `NkAppTickEvent` (deltaTime),
  `NkAppUpdateEvent` (logique métier), `NkAppRenderEvent`
  (interpolation), `NkAppCloseEvent` (annulable + `IsForced`).

#### Window ([NkWindowEvent.h](src/NKEvent/NkWindowEvent.h))
- 17 sous-types : `NkWindowCreate/Close/Destroy/Paint`, `Resize` +
  `ResizeBegin/End`, `Move` + `MoveBegin/End`, `FocusGained/Lost`,
  `Minimize/Maximize/Restore/Fullscreen/Windowed/Shown/Hidden`,
  `DpiChange`, `ThemeChange` (clair/sombre).

#### Keyboard ([NkKeyboardEvent.h](src/NKEvent/NkKeyboardEvent.h))
- Cross-platform : `NkKey` (codes sémantiques US-QWERTY), `NkScancode`
  (codes USB HID indépendants du layout), `NkModifierState` (Ctrl/Shift/
  Alt/Super), `NkKeycodeMap.h` pour conversion plateforme → scancode.
- Helpers : `NkScancodeFromWin32`, `NkScancodeFromLinux` (evdev),
  `NkScancodeFromXKeycode`, `NkScancodeFromMac` (Carbon),
  `NkScancodeFromDOMCode` (Web).
- Sous-types : `NkKeyPressEvent` (raccourcis/gameplay), `NkKeyRepeatEvent`
  (auto-repeat OS), `NkKeyReleaseEvent`, `NkTextInputEvent` (Unicode UTF-8
  post-IME), `NkCharEnteredEvent`.

#### Mouse ([NkMouseEvent.h](src/NKEvent/NkMouseEvent.h))
- `NkMouseButton`, `NkButtonState`, `NkMouseButtons` (bitmask multi-btn).
- Sous-types : `NkMouseMove` (coords client), `NkMouseRaw` (delta sans
  accel), `NkMouseButtonPressed/Released`, `NkMouseDoubleClick`,
  `NkMouseWheelVertical/Horizontal`, `NkMouseScroll` (unifié trackpad
  smooth), `NkMouseEnter/Leave` + `WindowEnter/Leave`,
  `NkMouseCaptureBegin/End`.

#### Touch ([NkTouchEvent.h](src/NKEvent/NkTouchEvent.h))
- `NkTouchPoint`, `NK_MAX_TOUCH_POINTS`.
- Phases : Begin/Move/End/Cancel/Pressed/Released.
- Gestures : Pinch (zoom), Rotate, Pan, Swipe, Tap, LongPress.

#### Gamepad ([NkGamepadEvent.h](src/NKEvent/NkGamepadEvent.h))
- `NkGamepadButton` (102 capacités), `NkGamepadAxis` (54).
- Events : Connect/Disconnect, ButtonPressed/Released, AxisMotion,
  Stick (XY normalisé [-1,1]), Triggered (LT/RT), Rumble.

#### HID générique ([NkGenericHidEvent.h](src/NKEvent/NkGenericHidEvent.h))
- Pour périphériques exotiques (joysticks volants, throttles, gear
  shifters, pédaliers).
- Connect/Disconnect, ButtonPressed/Released, AxisMotion, Stick,
  Triggered, Rumble.
- Mapping personnalisable via `NkGenericHidMapper`
  ([NkGenericHidMapper.h](src/NKEvent/NkGenericHidMapper.h)) :
  bouton physique → bouton logique avec scale/offset/deadzone/invert
  par axe.

#### Drop ([NkDropEvent.h](src/NKEvent/NkDropEvent.h))
- File / Text / Image / Enter / Over / Leave.

#### Transfer ([NkTransferEvent.h](src/NKEvent/NkTransferEvent.h))
- Transfert async fichiers/clipboard.

#### System ([NkSystemEvent.h](src/NKEvent/NkSystemEvent.h))
- Power (veille, réveil, batterie), Locale (langue/région),
  Display (hot-plug écran), Memory (low memory warning).

#### Graphics ([NkGraphicsEvent.h](src/NKEvent/NkGraphicsEvent.h))
- Événements liés au contexte graphique (recréation swapchain, device lost).
- Contient l'enum `NkGraphicsApi` central (utilisé par NKContext).

#### Custom ([NkCustomEvent.h](src/NKEvent/NkCustomEvent.h))
- Type générique utilisateur avec payload `void*` + id custom.

### Phase D — `NkEventSystem`
- Owné par `NkWESystem` (NKWindow), plus de singleton autoproclamé.
- **Ring buffer dual-priorité** ([NkEventSystem.h](src/NKEvent/NkEventSystem.h)
  ligne 137) :
  - HIGH (128 slots, no-drop) : window lifecycle, keyboard, mouse buttons,
    gamepad connect, app close/launch.
  - NORMAL (512 slots, drop-oldest) : mouse move, touch move, axis motion.
  - Pop priorise HIGH puis NORMAL.
- **Filtres typés par fenêtre** : `AddEventCallback<T>(cb, windowId)` —
  callback ignore les events provenant d'autres fenêtres.
- **RAII guard** : `AddEventCallbackGuard<T>` retourne un `NkCallbackGuard`
  qui désinscrit automatiquement à destruction.
- **Polling** :
  - `PollEvent()` — pointeur valide jusqu'au prochain appel (zero-alloc).
  - `PollEventCopy()` — `NkEventPtr` (durée de vie contrôlée).
  - `PollEvents()` — boucle interne avec dispatch global + per-window.
- **Direct dispatch** : `DispatchEvent(T&&)` immédiat (template forward).
- **Pont public** : `Enqueue_Public(evt, winId)` pour les callbacks
  statiques platform (Wayland, Android, WASM, UIKit).
- **Auto-update input state** : `NkEventState` (snapshot global) mis à jour
  par l'EventSystem à chaque event input.
- **Auto-gamepad poll** : `PollEvents()` appelle `NkGamepadSystem::Poll`
  automatiquement si activé.
- Assertions thread ID en debug sur PollEvent / PollEvents.

### Phase E — `NkEventDispatcher`
([NkEventDispatcher.h](src/NKEvent/NkEventDispatcher.h))
- Routeur push : `dispatcher.Dispatch<T>(handler)` renvoie bool
  (true = consommé).
- Macro `NK_DISPATCH(d, T, m)` pour binding méthode membre.
- `NkInputQuery` (instance globale stateless `NkInput`) : modèle pull
  délégant à `NkEventState`. Méthodes : `IsKeyDown`, `IsMouseDown`,
  `MouseX/Y`, `MouseDeltaX/Y`, `GamepadAxis`, `IsGamepadButtonDown`.
- `NkActionManager` : actions nommées (ex: "Jump", "Fire") résolvant
  vers un set de `NkInputCode` avec modificateurs et répétition.
- `NkAxisManager` : axes nommés (ex: "MoveX", "LookY") avec scale,
  invert, deadzone, neutralPosition.
- Conversion 1:1 depuis l'ancien EventBroker / InputManager / ActionManager
  / AxisManager du projet précédent.

### Phase F — `NkEventState`
([NkEventState.h](src/NKEvent/NkEventState.h))
- **États sémantiques légers** : `NkWindowState`, `NkRegionState`,
  `NkConnectionState`, `NkResizeState`, `NkAxisState`, `NkAxisDirection`,
  `NkStatusState`, `NkDraggedState`, `NkFocusState`.
- **Snapshots polling** :
  - `NkKeyboardInputState` (touches pressées, modifiers, dernière touche)
  - `NkMouseInputState` (position, boutons, capture, survol)
  - `NkTouchInputState` (contacts actifs, centroïde)
  - `NkGamepadInputState` (1 manette : boutons, axes, capteurs, batterie)
  - `NkGamepadSetState` (8 slots NK_MAX_GAMEPADS)
  - `NkEventState` (agrégat global, lecture seule applicative).

### Phase G — `NkGamepadSystem`
([NkGamepadSystem.h](src/NKEvent/NkGamepadSystem.h))
- Cross-platform via interface `NkIGamepad` PIMPL.
- 8 backends fournis par NKWindow (XInput+DirectInput, IOKit/GCController,
  Android AInputEvent, evdev, Gamepad Web API, Noop).
- Snapshot complet : 102 boutons + 54 axes + gyro + accéléromètre +
  batterie + isCharging.
- Callbacks : Connect (info+connected), Button (idx, btn, state),
  Axis (idx, axis, value).
- Polling direct (sans events) : `IsButtonDown(slot, btn)`,
  `GetAxis(slot, axis)`.
- Vibration : `Rumble(slot, lowFreq, highFreq, leftTrig, rightTrig, ms)`.
- Mapping persistant via `NkGamepadMappingPersistence`
  ([NkGamepadMappingPersistence.h](src/NKEvent/NkGamepadMappingPersistence.h))
  : DB de mappings type SDL game controller DB, par GUID device, JSON-able.

### Phase H — `NkDropSystem`
([NkDropSystem.h](src/NKEvent/NkDropSystem.h))
- Façade activation backend Drag&Drop, déléguée à NKWindow par plateforme :
  Win32 OLE, UWP DataTransfer, Cocoa registerForDraggedTypes, UIKit
  UIDropInteraction, X11/XCB XDND, Android Intent, Emscripten HTML5,
  Xbox runtime-specific, Noop fallback.
- API : `NkEnableDropTarget(nativeHandle)` / `NkDisableDropTarget`.
- Appel automatique si `NkWindowConfig::dropEnabled == true`.

### Phase I — Logging configurable
- `NK_EVENTSYS_TRACE_VERBOSE` (off par défaut) pour tracer chaque step
  AddEventCallback / dispatch (utile debug Android).

---

## En cours / TODO immédiat

### System events réellement émis
- `NK_SYSTEM_POWER` : déclaré mais pas câblé. À implémenter :
  - Win32 : `WM_POWERBROADCAST` (suspend/resume/battery).
  - macOS : `NSWorkspaceWillSleepNotification`.
  - Linux : D-Bus `org.freedesktop.UPower`.
  - Android : `Intent.ACTION_BATTERY_CHANGED` via JNI.
- `NK_SYSTEM_LOCALE` : `WM_SETTINGCHANGE` Win32, `NSLocaleCurrentLocaleDidChange`
  macOS, etc.
- `NK_SYSTEM_DISPLAY` : `WM_DISPLAYCHANGE` Win32, XRandR event mask,
  `wl_output` add/remove.
- `NK_SYSTEM_MEMORY` : low memory warning (iOS `didReceiveMemoryWarning`,
  Android `onLowMemory`, Windows `QueryMemoryResourceNotification`).

### Thread-safety
- `Enqueue_Public` est appelé depuis des threads natifs (Wayland event
  thread, Emscripten audio worklet, Android JNI thread). Le ring buffer
  actuel n'est PAS thread-safe par construction. Solution proposée :
  - Lock-free SPSC ring per thread + drain en thread main au PollEvents.
  - Ou mutex spin léger autour des Push/Pop.
- Documenter explicitement quels chemins sont thread-safe.

### Tests unitaires
- Pas de dossier `tests/` actuel dans NKEvent.
- À écrire : tests de chaque NkEventType (construction + Clone + ToString),
  test ring buffer (HIGH no-drop / NORMAL drop-oldest), test
  AddEventCallbackGuard RAII, test filtre per-window, benchmark dispatch
  100k events/s.

### Documentation publique
- Pas de `docs.md` ou `use.md` dans NKEvent (à la différence de NKWindow).
- Les exemples sont en bas du `.h` (NkEvent.h lignes 654-980) — extraire
  vers un guide externe.

---

## À venir / À ajouter (futur proche)

### Replay / record
- `NkEventRecorder` (déjà esquissé en commentaire dans NkEvent.h) :
  Record(NkEvent&) → vector<unique_ptr> via `Clone()`. Replay vers
  dispatcher. Utile pour tests automatisés et reproduction de bugs.
- Sérialisation binaire d'un trace d'events pour replay cross-session.
- Format : header (version, timestamp base) + chunks (type + size + data).

### Observer pattern typé statique
- Actuellement les callbacks passent par `NkFunction<void(NkEvent*)>` avec
  cast `As<T>()` dans le wrapper — léger overhead.
- Alternative : registres `NkVector<NkFunction<void(T*)>>` par type T,
  zéro cast à la dispatch. ~1-2h de refactor.

### Events Renderer / Asset
- `NK_ASSET_LOADED`, `NK_ASSET_FAILED`, `NK_ASSET_HOT_RELOADED` (utiles
  pour NKImage / NKFont / NKRenderer hot-reload).
- `NK_RENDER_DEVICE_LOST`, `NK_RENDER_FRAME_DROPPED` (telemetry).
- Actuellement le NKRenderer publish ce genre de signaux via NkLog, pas
  via NkEvent.

### Multi-touch gestures haut niveau
- 3-finger swipe (navigation système), 4-finger tap (Mission Control
  style), edge swipe (révéler bordure UI).
- Actuellement seules Pinch/Rotate/Pan/Swipe/Tap/LongPress sont déclarées.

### IME (Input Method Editor) events
- `NK_TEXT_COMPOSITION_START / UPDATE / END` pour les langues CJK.
- Actuellement seul `NkTextInputEvent` post-commit est émis, pas la
  composition pre-commit.

### Gamepad capteurs avancés
- Touchpad gestures (DualSense, DualShock 4) — touchpad events séparés
  des touch événements UI.
- Adaptive triggers DualSense (resistance, vibration ciblée).
- LED color set (DualSense, JoyCon).
- Mouse mode (Steam Controller, Steam Deck D-pad scroll).

### Reflection minimal
- `NkEventType::ForEach(callback)` pour énumérer les types à l'exécution.
- `NkEvent::GetFields()` schema → utile pour debug viewer en éditeur
  (Unkeny).

### Refactor `NkFunction` overhead
- `NkFunction` (NKContainers) utilise small-buffer optimization mais
  alloue pour les lambdas > 32 bytes. Audit captures lourdes
  (this+state) dans les dispatchers.

---

## Bugs / quirks connus
- **Ring buffer NORMAL drop-oldest** : les `NkMouseMove` peuvent être
  perdus si l'app n'appelle pas PollEvents au moins une fois par frame.
  Doc utilisateur : appel obligatoire chaque frame.
- **Timestamp** : `GetCurrentTimestamp()` utilise `std::chrono` malgré la
  contrainte zero-STL des couches de base — à migrer vers `NKTime/NkChrono`
  (déjà inclus dans le header).
- **`NkGenericHidMapper`** : inclut encore `<algorithm>` et `<cmath>` STL
  — à remplacer par NKMath/NkFunctions équivalents.
- **`NkEvent.h`** : inclut `<cstdint>`, `<string>`, `<chrono>` STL — à
  vérifier que c'est uniquement pour les exemples en commentaire.

---

## Dépendances
- **Couches en dessous (utilisées)** :
  - NKCore (types `uint32`/`uint64`, traits, atomics, invoke)
  - NKMath (`NkFunctions`)
  - NKContainers (`NkString`, `NkVector`, `NkUnorderedMap`,
    `NkFunction`, `NkArray`)
  - NKMemory (`NkUniquePtr`, `NkMemSet`, `NkUtils`)
  - NKTime (`NkChrono` pour timestamps)
  - NKLogger (logs platform-side)
  - NKPlatform (détection compile-time)
- **Modules au-dessus qui en dépendent** :
  - NKWindow (couplage bidirectionnel : `NkWESystem` possède `NkEventSystem`,
    les backends platform émettent les events)
  - NKContext (consomme `NkGraphicsApi` enum)
  - NKUI (consomme keyboard/mouse/touch pour widgets)
  - NKAudio, NKRenderer, NKCamera (events focus/window/keyboard)
  - Nkentseu/Core (`EventBus` est un wrapper du `NkEventSystem`)
