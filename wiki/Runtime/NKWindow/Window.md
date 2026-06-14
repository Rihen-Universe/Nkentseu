# La fenêtre et son contexte

> Couche **Runtime** · NKWindow · Ouvrir une **fenêtre native** cross-plateforme (`NkWindow`),
> la configurer (`NkWindowConfig`), lui attacher un **contexte GPU** (`NkContext`), et brancher la
> boucle d'événements via le point d'entrée global (`NkWESystem` / `NkInitialise`).

Tout programme graphique commence par la même question banale et pourtant épineuse : **comment
obtenir une surface sur laquelle dessiner, de la même façon sur Windows, Linux, macOS, Android, iOS
et le Web** ? Chaque système d'exploitation a sa propre API de fenêtrage (`HWND` Win32, `Display*`
XLib, `NSWindow` Cocoa, `ANativeWindow` Android…), son propre flux d'événements, ses propres règles
de cycle de vie. NKWindow est la couche qui **gomme ces différences** : on décrit ce qu'on veut
(`NkWindowConfig`), on ouvre une `NkWindow`, et le module produit en retour un **descripteur de
surface neutre** (`NkSurfaceDesc`) que le backend de rendu saura consommer — sans jamais que votre
code applicatif touche à un handle natif.

Ce n'est **pas** un moteur de rendu, et **pas** un système d'événements : NKWindow *crée la
surface* et *intègre* le pompage des événements, mais le rendu vit dans NKRHI/NKCanvas et les
événements typés vivent dans le module **NKEvent** (que NKWindow inclut en dépendance, pas comme
sous-composant). La frontière est nette : `NkWindow::GetSurfaceDesc()` est le **seul** point de
contact entre la fenêtre et le GPU.

- **Namespace** : `nkentseu` (sous-namespaces référencés : `math::`, `graphics::`, `memory::`)
- **Header parapluie** : `#include "NKWindow/NKWindow.h"`

---

## Ouvrir une fenêtre

Le cœur du module est la classe `NkWindow`. C'est une **façade** : elle expose une API portable
(titre, taille, position, plein écran, souris…) et cache la donnée plateforme-spécifique dans un
membre public `mData` que seuls les `.cpp` du backend manipulent. On la construit vide ou à partir
d'une config, mais la fenêtre native n'existe vraiment qu'après `Create()` (ou via le ctor qui
prend une `NkWindowConfig`).

Trois règles encadrent son cycle de vie. D'abord, **rien avant `NkInitialise()`** : le point
d'entrée global doit initialiser la plateforme et le système d'événements avant toute fenêtre.
Ensuite, le pattern **Create / Close** : à tout `Create()` correspond un `Close()`, et `IsOpen()` /
`IsValid()` disent où on en est. Enfin, `NkWindow` est **non copiable mais déplaçable** (move
ctor/assign `= default`) — on transfère la propriété d'une fenêtre, on ne la duplique pas.

```cpp
NkInitialise();                       // plateforme + événements, une fois
NkWindowConfig cfg;
cfg.title  = "Hello";
cfg.width  = 1280;
cfg.height = 720;
NkWindow window(cfg);                 // construit et crée

while (window.IsOpen()) {
    NkEvents().PollEvents();          // pompe les événements OS
    // ... rendu ...
}
NkClose();
```

Ce n'est **pas** une fenêtre figée : on la pilote à chaud — `SetTitle`, `SetSize`, `SetPosition`,
`SetVisible`, `Minimize` / `Maximize` / `Restore`, `SetFullscreen`. Les getters miroirs
(`GetTitle`, `GetSize`, `GetPosition`, `GetConfig`) relisent l'état, et `GetLastError()` rend la
dernière `NkError` rencontrée.

> **En résumé.** `NkWindow` = façade native portable. Construire (vide ou par config), `Create()`,
> piloter à chaud, `Close()`. Non copiable mais **déplaçable**. Toujours encadrée par
> `NkInitialise()` / `NkClose()`. Le membre public `mData` est réservé au backend.

---

## Décrire ce qu'on veut : `NkWindowConfig`

Plutôt qu'un constructeur à dix paramètres, NKWindow utilise une **config déclarative**. On part
des valeurs par défaut (fenêtre 1280×720, centrée, redimensionnable, vsync activée, fond
`0x141414FF`) et on ne touche qu'aux champs qui comptent. La config couvre cinq familles : la
**géométrie** (`x/y`, `width/height`, et les bornes `minWidth…maxHeight`), le **comportement**
(`resizable`, `movable`, `closable`, `fullscreen`, `modal`, `vsync`, `dropEnabled`,
`screenOrientation`…), l'**apparence** (`frame`, `hasShadow`, `transparent`, `visible`, `bgColor`),
l'**identité** (`title`, `name`, `iconPath`) et le **spécifique plateforme** (Android, Web,
Safe Area, options natives avancées).

Deux sous-structures méritent l'attention. `NkNativeWindowOptions` (champ `native`) permet
d'**embarquer** la surface dans une fenêtre déjà existante (`useExternalWindow` +
`externalWindowHandle`), de viser un parent, ou — sur Win32 — de copier le pixel format d'un autre
`HWND` (`win32PixelFormatShareWindowHandle`). `NkWebInputOptions` (champ `webInput`) décide, en
WASM, quels événements navigateur on capture (clavier, mouvements souris, boutons, molette,
toucher) et lesquels on laisse au navigateur — par exemple `captureMouseRight=false` laisse le menu
contextuel du navigateur tranquille.

Un champ est **particulier** : `surfaceHints`. On le laisse **vide** dans l'immense majorité des
cas (Vulkan, Metal, DirectX, Software, WGL, EGL) ; il n'est requis que pour **OpenGL / GLX sous
Linux**, et il est rempli automatiquement par `NkContextApplyWindowHints()` **avant** `Create()`.
NkWindow transmet ces hints au backend sans les interpréter.

> **En résumé.** `NkWindowConfig` est une config déclarative à défauts sains : on ne renseigne que
> ce qui change. `native` pour embarquer/parenter une surface, `webInput` pour la capture
> navigateur, et `surfaceHints` qu'on laisse vide sauf OpenGL/GLX Linux (rempli par
> `NkContextApplyWindowHints`).

---

## Brancher le GPU : `NkContext`

Avoir une fenêtre ne suffit pas à dessiner : il faut une **surface** que le backend graphique
comprenne. C'est le rôle de deux pièces complémentaires. `NkWindow::GetSurfaceDesc()` renvoie un
`NkSurfaceDesc` — un descripteur **neutre** qui contient les handles natifs réellement nécessaires
(taille, DPI, et selon la plateforme `HWND`, `Display*`, `ANativeWindow*`…), et c'est le **seul
contrat** entre la fenêtre et le moteur de rendu. Pour les API à contexte (OpenGL), NKWindow ajoute
une couche **style GLFW/SDL** : le module `NkContext`, fait de **fonctions libres**, qui crée,
rend courant et présente un contexte GPU.

L'astuce à comprendre : pour Vulkan, DirectX, Metal et le renderer logiciel, il n'y a **aucun
contexte natif à créer** — `NkContextCreate` réussit toujours en mode `SURFACE_ONLY` et se contente
de remplir la surface. Le mode `GRAPHICS_CONTEXT` (avec création réelle d'un `HGLRC`/`GLXContext`/…)
ne concerne qu'OpenGL. D'où le flux canonique :

```cpp
NkContextInit();
NkContextWindowHint(NkContextHint::NK_CONTEXT_HINT_API, NK_GFX_API_OPENGL);
NkContextApplyWindowHints(windowCfg);     // remplit surfaceHints AVANT Create
window.Create(windowCfg);

NkContext ctx;
NkContextCreate(window, ctx);             // APRÈS window.Create()
NkContextMakeCurrent(ctx);
gladLoadGLLoader(NkContextGetProcAddress);
// ... boucle de rendu : NkContextSwapBuffers(ctx); ...
NkContextDestroy(ctx);                     // pattern Create/Destroy
```

Ce n'est **pas** une classe à instancier : on configure des **hints globaux**
(`NkContextSetApi`, `NkContextWindowHint`, `NkContextSetHints`), puis on crée un contexte **par
fenêtre**. La structure `NkContext` retournée porte la `config`, le `mode`, la `surface`, et les
handles opaques (`nativeContext`, `nativeDisplay`…) que le backend exploitera.

> **En résumé.** `GetSurfaceDesc()` = contrat unique fenêtre↔GPU, suffisant pour Vulkan/DX/Metal/SW
> (mode `SURFACE_ONLY`). Pour OpenGL, `NkContext` (fonctions libres, style GLFW) crée un vrai
> contexte : `Init → hints → ApplyWindowHints → Create → ContextCreate → MakeCurrent → SwapBuffers
> → Destroy`.

---

## Le point d'entrée global : `NkWESystem`

Reste à coller le tout : qui pompe les événements OS, qui attribue les identifiants de fenêtres,
qui possède le système d'événements et les manettes ? Un objet unique, `NkWESystem` (Window/Event
System), accessible par `NkWESystem::Instance()`. Il est l'**unique propriétaire** du
`NkEventSystem` et du `NkGamepadSystem` (finis les singletons séparés), il tient le **registre des
fenêtres** (assignation d'`NkWindowId`, lookup), et il s'initialise une fois pour toutes via
`Initialise()` (qui, sur Windows, gère aussi `OleInitialize`).

En pratique on ne le manipule presque jamais directement : des **fonctions libres** servent de
raccourcis. `NkInitialise()` / `NkClose()` délèguent au cycle de vie, `NkEvents()` renvoie le
système d'événements et `NkGamepads()` le système de manettes. C'est `NkEvents().PollEvents()`
qu'on appelle chaque frame.

> **En résumé.** `NkWESystem` = point d'entrée + propriétaire unique des sous-systèmes événements et
> manettes + registre des fenêtres. On y accède par les raccourcis `NkInitialise` / `NkClose` /
> `NkEvents` / `NkGamepads`.

---

## Aperçu de l'API

Tous les éléments publics, regroupés par fichier. Complexités/`noexcept` indiqués quand ils sont
significatifs.

### `NkWindow` — la fenêtre (NkWindow.h)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkWindow()`, `explicit NkWindow(const NkWindowConfig&)`, `~NkWindow()` | Vide / construit+crée par config / destruction |
| Sémantique | copie **supprimée** ; move ctor/assign `= default` | Non copiable, **déplaçable** |
| Cycle de vie | `Create(config)`, `Close()`, `IsOpen()`, `IsValid()` | Créer / fermer / état ouvert / état valide |
| Identité | `GetId()` (inline) | `NkWindowId` de la fenêtre |
| Propriétés | `GetTitle`/`SetTitle`, `GetSize`, `GetPosition`, `GetConfig`, `GetLastError` | Titre, taille, position, config, dernière erreur |
| DPI / display | `GetDpiScale`, `GetDisplaySize`, `GetDisplayPosition` | Échelle DPI et géométrie écran |
| Moniteurs | `EnumerateMonitors()`, `GetCurrentMonitor()`, `GetMonitorCount()` | Hot-plug : recalculés à chaque appel |
| Manipulation | `SetSize` (×2), `SetPosition` (×2), `SetVisible`, `Minimize`, `Maximize`, `Restore`, `SetFullscreen` | Piloter la fenêtre à chaud |
| Orientation | `SupportsOrientationControl`, `Set/GetScreenOrientation`, `Set/IsAutoRotateEnabled` | Contrôle d'orientation (mobile) |
| Android | `Set/GetHideSystemUI`, `Set/GetLockOrientation` | Masquer barres système / verrouiller rotation |
| Souris | `SetMousePosition` (×2), `ShowMouse`, `CaptureMouse`, `ClipMouseToClient` | Position, visibilité, capture, clip (FPS/RTS) |
| Web | `Set/GetWebInputOptions` | Capture d'input navigateur (WASM) |
| OS extras | `SetProgress(float)` | Progression barre des tâches / dock |
| Safe Area | `GetSafeAreaInsets()` | Marges sûres (encoches mobiles) |
| Surface | `GetSurfaceDesc()` | **Seul** contrat fenêtre↔GPU |
| Données | `struct NkWindowData mData` (public) | Donnée backend, manipulée par les `.cpp` |

### `NkWindowConfig` & co. (NkWindowConfig.h)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Config | `NkWindowConfig` | Descripteur déclaratif de fenêtre (géométrie, comportement, apparence, identité…) |
| Options natives | `NkNativeWindowOptions` (champ `native`) | Embarquer/parenter une surface, partage pixel format Win32 |
| Input web | `NkWebInputOptions` (champ `webInput`) | Quels événements navigateur capturer (WASM) |
| Hints surface | `surfaceHints` | Vide sauf OpenGL/GLX Linux ; rempli par `NkContextApplyWindowHints` |

### `NkTypes.h` — types de base

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `enum class NkPixelFormat : uint32` | Formats de pixels (**valeurs dupliquées**, deux blocs `=0`) |
| Erreur | `struct NkError` { `code`, `message` } | `IsOk()`, `ToString()`, `static Ok()` |

### `NkSurfaceHint.h` — hints opaques

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `enum class NkSurfaceHintKey : uint32` | Clés (GLX/EGL/WGL ; rien pour Vulkan/Metal/DX/SW) |
| Hint | `struct NkSurfaceHint` { `key`, `value` } | Une paire clé/valeur opaque |
| Conteneur | `struct NkSurfaceHints` | Tableau compact (`kMaxHints=8`), zéro alloc : `Set`/`Get`/`Has`/`Clear` `[O(count)]` |

### `NkSurface.h` — descripteur de surface

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Descripteur | `struct NkSurfaceDesc` | Handles natifs **conditionnels par plateforme** + `width/height/dpi` + `appliedHints` |
| Validité | `IsValid()` | Validité conditionnelle par plateforme |

### `NkContext.h` — contexte GPU (fonctions libres)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enums | `NkContextProfile`, `NkContextHint`, `NkContextMode` | Profil GL, clé de hint, mode surface/contexte |
| Typedef | `NkContextProc` | Loader de proc address `void*(*)(const char*)` |
| Structs | `NkWin32PixelFormatConfig`, `NkContextConfig`, `NkContext` | Pixel format WGL / config / état de contexte |
| Lifecycle | `NkContextInit()`, `NkContextShutdown()` | Init / shutdown global |
| Hints | `NkContextResetHints`, `SetHints`, `SetApi`, `SetWin32PixelFormat`, `WindowHint`, `GetHints`, `ApplyWindowHints` | Configurer les hints globaux avant création |
| Par fenêtre | `GetModeForApi`, `Create`, `Destroy`, `MakeCurrent`, `SwapBuffers`, `GetProcAddressLoader`, `GetProcAddress` | Créer/présenter un contexte (pattern Create/Destroy) |

### `NkWESystem.h` — point d'entrée global

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| App data | `struct NkAppData` | Nom/version app, multi-fenêtre, debug, `userData` |
| Système | `class NkWESystem`, `Instance()` | Propriétaire unique events+manettes, registre fenêtres |
| Cycle de vie | `Initialise`, `Close`, `IsInitialised`, `GetAppData` | Init plateforme + event system |
| Accès | `GetEventSystem`/`Events`, `GetGamepadSystem`/`Gamepads` | Sous-systèmes possédés |
| Registre | `RegisterWindow`, `UnregisterWindow`, `GetWindow` `[O(1)~]`, `GetWindowCount`, `GetWindowAt` | Gestion des `NkWindowId` + lookup |
| Raccourcis | `NkInitialise`, `NkClose`, `NkEvents`, `NkGamepads` (libres, inline) | Délèguent à `Instance()` |

### `NkEventSystem.h` — intégration événements (vit sous NKEvent/)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Alias | `NkGlobalEventCallback`, `NkTypedEventCallback`, `NkRemoverCallback`, `NkEventPtr` | Callbacks + pointeur d'event possédé |
| Aux | `struct NkEventDelete`, `enum class NkEventPriority`, `NkGetEventPriority()` | Suppresseur / priorité HIGH-NORMAL / classification `[O(1)]` |
| Tampon | `class NkEventRingBuffer` | Ring double-priorité (HIGH 128 no-drop, NORMAL 512 drop-oldest) |
| RAII | `class NkCallbackGuard` | Désinscription automatique (movable) |
| Système | `class NkEventSystem` | Callbacks, pompe, dispatch, état d'input |
| Macro | `NK_EVENTSYS_ANDROID_TRACE(...)` | Trace Android (suppose un `logger` en portée) |

---

## Référence complète

Chaque élément repris à fond. Le trivial (getters/setters miroirs) est décrit brièvement ; ce qui
structure l'usage (surface, contexte, événements, hot-plug) l'est en détail, avec les usages par
domaine.

### `NkWindow` — cycle de vie et identité

`Create(config)` crée la fenêtre native et renvoie un `bool` de succès ; `Close()` la détruit ;
`IsOpen()` / `IsValid()` renseignent l'état. Le pattern **Create/Close** doit toujours être appairé
(comme `Init/Shutdown`, `ContextCreate/Destroy`). `GetId()` (inline) renvoie l'`NkWindowId` assigné
par le registre du `NkWESystem` — c'est cet identifiant qui sert à **router** un événement vers la
bonne fenêtre en multi-fenêtre.

- **Outils / éditeur** : un éditeur multi-fenêtres (viewport, inspecteur détaché, fenêtre de jeu)
  ouvre plusieurs `NkWindow`, chacune avec son `NkWindowId`, et dispatche les événements par id.
- **Gameplay** : la boucle principale tourne tant que `IsOpen()` ; la fermeture (croix, Alt-F4)
  passe par un événement de fenêtre qui finit par `Close()`.
- **GPU** : avant tout rendu, vérifier `IsValid()` puis `GetSurfaceDesc().IsValid()` garantit qu'on
  a une surface dessinable.

### `NkWindow` — propriétés et manipulation

Les getters/setters miroirs (`Get/SetTitle`, `GetSize`, `GetPosition`, `GetConfig`, `GetLastError`)
sont triviaux : ils lisent ou écrivent l'état courant. La **manipulation** est l'ensemble des
actions qui changent l'aspect ou la place : `SetSize` (en scalaires ou via `NkVec2u`),
`SetPosition` (idem — attention, la surcharge vectorielle prend un `NkVec2u` **non signé** alors que
la version scalaire prend `int32`, d'où une perte de signe possible pour des positions négatives),
`SetVisible`, `Minimize` / `Maximize` / `Restore`, et `SetFullscreen`.

- **UI / 2D** : un mode plein écran togglé par l'utilisateur (`SetFullscreen`), un splash-screen
  qu'on rend visible après chargement (`SetVisible(true)`).
- **Outils** : restaurer la dernière géométrie d'une fenêtre d'éditeur depuis un fichier de session
  (`SetSize` + `SetPosition`).
- **IO / réseau** : `GetLastError()` (renvoie `NkError`) journalisé quand une création de fenêtre
  échoue.

### `NkWindow` — DPI, moniteurs et hot-plug

Voici la partie qui sauve réellement des bugs. `GetDpiScale()` donne l'échelle DPI courante,
`GetDisplaySize()` / `GetDisplayPosition()` la géométrie de l'écran. Surtout, le trio
`EnumerateMonitors()` / `GetCurrentMonitor()` / `GetMonitorCount()` gère le **multi-écran à chaud** :
`EnumerateMonitors()` **recalcule à chaque appel** et reflète donc l'état après un branchement /
débranchement d'écran ; son premier élément n'est **pas** garanti primaire (tester
`NkDisplayInfo::isPrimary`). `GetCurrentMonitor()` renvoie le moniteur qui contient
majoritairement la fenêtre — et sur mobile/web (mono-écran) l'écran courant.

- **Rendu** : sur un écran à 150 % de mise à l'échelle, `GetDpiScale()` permet de dimensionner la
  swapchain en **pixels physiques** (et non en points logiques), évitant une fenêtre floue ou mal
  taillée — le bug réel qui a motivé cette API.
- **UI / 2D** : adapter la taille des polices et des marges à l'échelle DPI du moniteur courant.
- **Outils** : déplacer une fenêtre d'un écran 4K à un écran 1080p doit re-créer la swapchain ;
  `GetCurrentMonitor()` détecte le changement, `EnumerateMonitors()` liste les cibles possibles.
- **Threading** : à brancher sur les événements display (ADDED/REMOVED/RESOLUTION_CHANGED) pour
  recréer les ressources GPU dans le thread propriétaire.

### `NkWindow` — orientation, Android, souris, Web, OS

Ces familles sont essentiellement des **passe-plats** vers le natif, no-op là où ça n'a pas de sens.

- **Orientation (mobile)** : `SupportsOrientationControl()` puis `Set/GetScreenOrientation` et
  `Set/IsAutoRotateEnabled` verrouillent ou libèrent la rotation — un jeu paysage force
  l'orientation, une app de lecture laisse l'auto-rotation.
- **Android** : `SetHideSystemUI(true)` masque status bar et barre de navigation (mode immersif
  plein écran), `SetLockOrientation(true)` empêche la rotation.
- **Souris** : `ShowMouse(false)` cache le curseur, `CaptureMouse(true)` le rattache à la fenêtre,
  et `ClipMouseToClient(true)` le confine rectangulairement à la zone client (Win32 `ClipCursor`,
  XLib `XGrabPointer`) — exactement ce qu'il faut pour un **FPS** ou un **RTS**. No-op sur
  plateformes sans curseur (mobile/web). `SetMousePosition` recentre le curseur (caméra à la
  souris).
- **Web (WASM)** : `Set/GetWebInputOptions` décide quels événements navigateur sont capturés (voir
  `NkWebInputOptions`).
- **OS / outils** : `SetProgress(float)` affiche une progression dans la barre des tâches/dock
  (export, compilation d'assets, chargement de niveau).
- **Safe Area (mobile)** : `GetSafeAreaInsets()` renvoie les marges sûres (encoches, coins
  arrondis) pour ne pas placer d'UI dessous.

### `NkWindow::GetSurfaceDesc()` et `NkSurfaceData` — le contrat GPU

C'est **le** point névralgique : `GetSurfaceDesc()` renvoie un `NkSurfaceDesc`, **seul** point de
contact entre `NkWindow` et les backends graphiques. Le descripteur porte les champs communs
(`width`, `height`, `dpi`, `appliedHints` = hints réellement appliqués à la création) et, **selon la
plateforme** (`#if`), les handles natifs : `hwnd`/`hinstance` (Windows, HDC volontairement absent
car éphémère), `view`/`metalLayer` (macOS/iOS), `display`/`window`/`screen` (XLib/XCB), `display`/
`surface` + champs SHM (Wayland), `nativeWindow` (Android), `ohNativeWindow` (HarmonyOS), `canvasId`
(Emscripten). `IsValid()` teste la validité **conditionnellement** (Windows : `hwnd != null &&
width>0 && height>0` ; fallback : `width>0 && height>0`).

- **GPU / rendu** : on passe `GetSurfaceDesc()` (et/ou `NkContext.surface`) au backend RHI pour
  créer la swapchain Vulkan/DX/Metal — c'est l'unique handle dont le moteur a besoin.
- **Outils** : embarquer le viewport du moteur dans une fenêtre d'éditeur tierce passe par
  `NkNativeWindowOptions::useExternalWindow`, dont la surface ressort par ce même descripteur.

### `NkWindowConfig`, `NkNativeWindowOptions`, `NkWebInputOptions`

`NkWindowConfig` est le descripteur déclaratif détaillé en section narrative : géométrie (`x/y`,
`width/height`, bornes `min/max`), comportement (`resizable`, `fullscreen`, `vsync`, `dropEnabled`,
`screenOrientation`…), apparence (`frame`, `transparent`, `bgColor=0x141414FF`…), identité (`title`,
`name`, `iconPath`), et spécifiques plateforme. À retenir : on ne touche **que** ce qui change, les
défauts couvrent le cas courant.

- `NkNativeWindowOptions` (champ `native`) — embarquer la surface dans une fenêtre existante
  (`useExternalWindow` + `externalWindowHandle`), viser un parent (`parentWindowHandle`), marquer
  une fenêtre utilitaire, ou copier le pixel format d'un `HWND` sur Win32
  (`win32PixelFormatShareWindowHandle`). Domaine clé : **outils/éditeur** (intégration dans une UI
  hôte) et **GPU** (partage de contexte WGL).
- `NkWebInputOptions` (champ `webInput`) — neuf `bool` qui partagent les événements entre la page et
  le canvas (clavier, raccourcis navigateur, mouvements/boutons souris, molette, toucher, menu
  contextuel). Domaine : **Web / UI** — par défaut on capture tout sauf le clic droit et le menu
  contextuel.

### `NkTypes.h` — `NkPixelFormat` et `NkError`

`NkPixelFormat` est un enum de formats, avec un **piège réel** : il contient **deux blocs** qui
repartent de `0`. Le premier (formats « riches » : `NK_PIXEL_R8G8B8A8_UNORM`, `_SRGB`,
`_R16G16B16A16_FLOAT`, profondeur…) et un second (alias compacts : `NK_PIXEL_RGBA8 = 0`,
`BGRA8=1`, `RGB8=2`, `YUV420`, `NV12`, `YUYV`, `MJPEG`). Conséquence : `RGBA8` et `UNKNOWN` valent
tous deux `0`, `BGRA8` et `R8G8B8A8_UNORM` valent `1`, etc. **Ne jamais comparer à travers les deux
familles d'alias** — domaine GPU (formats de swapchain/texture) et caméra/vidéo (formats de capture
`YUV420`/`MJPEG`).

`NkError` est la valeur d'erreur du module : un `code` (`uint32`, 0 = OK) et un `message`
(`NkString`). `IsOk()` teste `code==0`, `ToString()` rend `"OK"` ou `"[code] message"` (via
`NkString::Fmtf`), `static Ok()` fabrique l'erreur neutre. On la croise dans **IO / réseau**
(échec de création, ressource indisponible) et **outils** (remontée d'erreur journalisée).

### `NkSurfaceHint.h` — hints opaques sans allocation

`NkSurfaceHints` est un **tableau compact à taille fixe** (`kMaxHints=8`, `NkArray`) — donc **zéro
allocation dynamique**. `Set(key, value)` ajoute ou remplace (recherche linéaire `O(count)` ;
ignore silencieusement au-delà de `kMaxHints`), `Get(key, default)` lit en `O(count)`, `Has(key)`
teste en `O(count)`, `Clear()` réinitialise. Les clés (`NkSurfaceHintKey`) ne concernent que les API
à configuration de visual : `NK_GLX_VISUAL_ID`, `NK_GLX_FB_CONFIG_PTR`, `NK_EGL_DISPLAY`,
`NK_EGL_CONFIG`, `NK_WGL_SHARE_PIXEL_FORMAT_HWND` — **rien** pour Vulkan/Metal/DirectX/Software.

- **GPU** : c'est le canal par lequel `NkContextApplyWindowHints()` injecte le visual GLX (Linux)
  choisi *avant* la création de la fenêtre — la fenêtre X doit naître avec le bon visual, d'où
  l'ordre `ApplyWindowHints → Create`.

### `NkContext.h` — création et présentation du contexte

API de **fonctions libres** style GLFW/SDL. Côté global : `NkContextInit()` / `NkContextShutdown()`
encadrent l'usage ; les hints se posent par `NkContextSetApi`, `NkContextWindowHint(hint, value)`,
`NkContextSetHints(config)`, et se relisent par `NkContextGetHints()`. `NkContextApplyWindowHints
(config)` est le pivot Linux/GL : il **remplit** `surfaceHints` dans la config **avant**
`NkWindow::Create()`. Côté par fenêtre : `NkContextCreate(window, ctx)` (à appeler **après**
`window.Create()`) crée le contexte, `NkContextMakeCurrent(ctx)` le rend courant,
`NkContextSwapBuffers(ctx)` présente, `NkContextGetProcAddress`/`GetProcAddressLoader` fournissent le
loader (pour `gladLoadGLLoader`), et `NkContextDestroy(ctx)` libère (pattern Create/Destroy).
`NkContextGetModeForApi(api)` indique si l'API exige un vrai contexte ou se contente d'une surface.

La structure `NkContext` porte la `config`, le `mode` (`SURFACE_ONLY` par défaut, `GRAPHICS_CONTEXT`
pour GL), la `surface`, et les handles opaques (`nativeContext` = `HGLRC`/`GLXContext`/…,
`nativeDisplay`, `nativeDrawable`…). `NkContextConfig` décrit l'API visée, la version, le profil
(`CORE`/`COMPATIBILITY`/`ES`), le double buffering, le MSAA, le vsync et les bits par canal ;
`NkWin32PixelFormatConfig` couvre le réglage **fin** du pixel format WGL (rarement nécessaire).

- **GPU / rendu** : flux OpenGL complet (init, make current, swap). Pour Vulkan/DX/Metal/SW,
  `NkContextCreate` réussit en `SURFACE_ONLY` et le backend RHI prend la surface directement.
- **Threading** : `MakeCurrent` lie le contexte GL au thread appelant — le rendu GL doit rester sur
  ce thread.
- **Outils** : `NkWin32PixelFormatConfig` + `win32PixelFormatShareWindowHandle` pour intégrer un
  viewport GL dans une fenêtre hôte au pixel format imposé.

### `NkWESystem.h` — système global et registre

`NkWESystem::Instance()` est le singleton qui **possède** `NkEventSystem` et `NkGamepadSystem`.
`Initialise(NkAppData)` initialise plateforme + événements (et `OleInitialize` une fois sur
Windows) ; `Close()` ferme ; `IsInitialised()` / `GetAppData()` (inline) renseignent. L'accès aux
sous-systèmes se fait par `GetEventSystem()` / `Events()` et `GetGamepadSystem()` / `Gamepads()`
(static inline). Le **registre** attribue les `NkWindowId` : `RegisterWindow(win)` /
`UnregisterWindow(id)`, `GetWindow(id)` (lookup map `O(1)` moyen), `GetWindowCount()`,
`GetWindowAt(index)`.

Les **raccourcis libres** (`NkInitialise`, `NkClose`, `NkEvents`, `NkGamepads`) délèguent à
`Instance()` et constituent l'API qu'on utilise au quotidien. `NkAppData` configure le nom/version
de l'app, le multi-fenêtre, les logs debug et un `userData` libre.

- **Gameplay** : `NkEvents().PollEvents()` chaque frame ; `NkGamepads()` pour lire les manettes.
- **Outils / éditeur** : le registre permet de router les événements vers la bonne fenêtre par
  `NkWindowId`, et `GetWindowCount`/`GetWindowAt` d'itérer sur toutes les fenêtres ouvertes.

### `NkEventSystem.h` — intégration des événements (module NKEvent)

Bien que **inclus** par NKWindow, le système d'événements **appartient au module NKEvent** et est
**possédé** par `NkWESystem`. On en retient surtout la **pompe** et la gestion de durée de vie des
événements.

- **Pompe** : `PollEvents()` pompe tous les événements OS et les dispatche aux callbacks.
  `PollEvent()` renvoie un `NkEvent*` **volatile** — valide uniquement jusqu'au prochain
  `PollEvent()`, **à ne pas stocker entre frames**. Pour conserver un événement (traitement
  différé/async), `PollEventCopy()` renvoie un `NkEventPtr` (unique_ptr custom) dont l'appelant
  contrôle la durée de vie.
- **Callbacks typés** : `AddEventCallback<T>(cb, windowId)` enregistre un handler filtré par type
  `T` (et optionnellement par fenêtre) ; `AddEventCallbackGuard<T>(...)` `[[nodiscard]]` en renvoie
  une version RAII (`NkCallbackGuard`) qui **désinscrit à sa destruction** — pratique pour lier la
  durée de vie d'un abonnement à celle d'un objet (un layer UI, un système ECS).
  `ClearEventCallbacks<T>()` / `ClearAllCallbacks()` nettoient.
- **Priorités** : `NkEventPriority` distingue **HIGH** (jamais droppé : cycle de vie fenêtre,
  clavier, boutons souris, app lifecycle, boutons manette) et **NORMAL** (droppable : mouvements
  souris). `NkGetEventPriority(type)` classe en `O(1)`. Le `NkEventRingBuffer` reflète ce contrat :
  file HIGH de 128 (no-drop) et NORMAL de 512 (drop-oldest).
- **État d'input** : `GetInputState()`, `GetHidMapper()`, et les bascules `SetAutoUpdateInputState`,
  `SetAutoGamepadPoll`, `SetQueueMode` (toutes `noexcept`, inline) règlent le comportement du
  pompage. `GetPlatformName()` / `GetTotalEventCount()` / `GetPendingEventCount()` pour le
  diagnostic.
- **Dispatch direct** : `DispatchEvent(event)` (et la version template) injecte un événement
  manuellement — utile pour des événements **synthétiques** (rejouer une entrée, simuler en test).
- **Domaines** : **gameplay/IA** (entrées clavier/souris/manette), **UI/2D** (clics, molette,
  toucher), **outils** (raccourcis globaux via callbacks), **threading** (pompe sur le thread
  enregistré à `Init()` ; les callbacks plateforme statiques passent par `Enqueue_Public`).

### Pièges réels à connaître

- **`NkPixelFormat` a des valeurs dupliquées** (deux blocs `=0`) : ne pas comparer à travers les
  deux familles d'alias.
- **`SetPosition(const NkVec2u&)`** prend un vecteur **non signé** alors que la version scalaire
  est `int32` : risque de perte de signe pour des positions négatives.
- **`PollEvent()`** renvoie un pointeur invalidé au prochain poll : utiliser `PollEventCopy()` pour
  conserver.
- **Ordre GL/Linux** : `NkContextApplyWindowHints()` **avant** `window.Create()`, puis
  `NkContextCreate()` **après**.
- **Pattern Create/Destroy partout** : `Create/Close`, `ContextCreate/ContextDestroy`,
  `Init/Shutdown`, `Initialise/Close` — toujours appairés.

---

### Exemple récapitulatif

```cpp
#include "NKWindow/NKWindow.h"
using namespace nkentseu;

// 1. Point d'entrée global (plateforme + événements).
NkInitialise();

// 2. Config déclarative : on ne touche qu'au nécessaire.
NkWindowConfig cfg;
cfg.title  = "Demo";
cfg.width  = 1280;
cfg.height = 720;
cfg.vsync  = true;

// 3. Hints OpenGL (ignorés pour Vulkan/DX/Metal/SW) AVANT Create.
NkContextInit();
NkContextWindowHint(NkContextHint::NK_CONTEXT_HINT_API, NK_GFX_API_OPENGL);
NkContextApplyWindowHints(cfg);

// 4. Création fenêtre + contexte GPU.
NkWindow window(cfg);
NkContext ctx;
NkContextCreate(window, ctx);            // APRÈS window.Create()
NkContextMakeCurrent(ctx);

// 5. Boucle : pompe événements (volatils), rendu, présentation.
while (window.IsOpen()) {
    NkEvents().PollEvents();
    NkEvent* ev = nullptr;
    while (NkEvents().PollEvent(ev)) {
        // ... traiter ev (ne pas le stocker au-delà du prochain poll) ...
    }
    // ... rendu via window.GetSurfaceDesc() ...
    NkContextSwapBuffers(ctx);
}

// 6. Démontage : Create/Destroy appairés, dans l'ordre inverse.
NkContextDestroy(ctx);
window.Close();
NkClose();
```

---

[← Index NKWindow](README.md) · [Récap NKWindow](../NKWindow.md) · [Couche Runtime](../README.md)
