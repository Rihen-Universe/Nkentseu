# NKWindow - Surface vs Context

Ce document explique:
- le rapport entre `NkSurface`, `NkSurfaceHint` et `NkContext`
- ce qu'il faut conserver si tu veux une architecture personnalisee
- la procedure pour des etudiants qui doivent implementer leur propre renderer backend

## 1) Rapport entre Surface et Context

### `NkSurfaceDesc` (obligatoire)

`NkSurfaceDesc` est le contrat bas niveau entre fenetre et rendu.
Il expose les handles natifs deja crees par `NKWindow`:
- Windows: `HWND`, `HINSTANCE`
- Linux XCB: `xcb_connection_t*`, `xcb_window_t`
- Linux Xlib: `Display*`, `Window`
- Wayland: `wl_display*`, `wl_surface*`
- Android/macOS/iOS/etc: handles equivalents

Sans `NkSurfaceDesc`, chaque backend renderer devrait parler directement aux classes platform de `NKWindow`, ce qui casserait l'isolation.

### `NkSurfaceHint` (conditionnel, pre-create)

`NkSurfaceHint` sert aux infos qui doivent etre decidees avant `window.Create()`.
Exemple classique: GLX visual/fbconfig sur Xlib/XCB.

Dans beaucoup de cas (`Vulkan`, `DX11`, `DX12`, `Metal`, `Software`), les hints peuvent rester vides.

### `NkContext` (helper optionnel)

`NkContext` est une API de commodite style GLFW/SDL:
- configure des hints globaux
- applique les hints avant creation fenetre
- cree un contexte natif OpenGL (ou mode surface-only pour les autres APIs)

Important:
- `NkContext` ne remplace pas `NkSurfaceDesc`.
- `NkContext` est une couche utilitaire au-dessus de `NkSurfaceDesc`.

## 2) Ce qu'il faut conserver (meme avec ta touche perso)

Si tu veux une signature propre du framework, garde ces invariants:

1. `NkSurfaceDesc` reste le contrat unique entre fenetre et rendu.
2. `NkSurfaceHint` reste opaque (key/value) et pre-create uniquement.
3. `NkContext` reste optionnel: utile pour apps simples, pas obligatoire pour backends experts.
4. Les backends renderer ne dependent pas des classes platform de `NKWindow`.
5. Les details platform restent confines dans:
   - `NKWindow/Platform/*` (creation fenetre)
   - `NKWindow/Core/NkContext*` (creation contexte utilitaire)

Tu peux personnaliser:
- les noms de facades
- les builders/configs
- les helpers de bootstrap

Mais evite de casser ces 5 invariants, sinon la portabilite baisse vite.

## 3) Quand utiliser Surface ou Context

### Mode A - Surface-first (recommande pour backend pro)

A utiliser si tes etudiants codent un vrai backend (Vulkan/DX/Metal/OpenGL avance):
- ils recuperent `NkSurfaceDesc`
- ils construisent eux-memes `device/swapchain/surface/context`

`NkContext` devient facultatif dans ce mode.

### Mode B - Context-first (recommande pour onboarding)

A utiliser pour demarrer vite (surtout OpenGL):
- `NkContextInit`
- `NkContextWindowHint(...)`
- `NkContextApplyWindowHints(windowCfg)`
- `window.Create(windowCfg)`
- `NkContextCreate(window, ctx)`
- OpenGL: `NkContextMakeCurrent(ctx)` + loader GL (GLAD)

Ensuite, leur renderer consomme `ctx.surface` (ou `window.GetSurfaceDesc()`).

### Mode C - Hints manuels sans NkContext

Oui, c'est possible. `NkSurfaceHint` peut etre utilise sans `NkContext`.

Le principe:
- tu ecris les hints dans `NkWindowConfig.surfaceHints` avant `window.Create(...)`
- le backend fenetre les applique (si concerné)
- tu relis ce qui a ete applique via `window.GetSurfaceDesc().appliedHints`

Exemple minimal:

```cpp
NkWindowConfig wc{};
wc.surfaceHints.Set(NkSurfaceHintKey::NK_GLX_VISUAL_ID, wantedVisualId);
// Optionnel: fournir un EGLConfig deja choisi
// wc.surfaceHints.Set(NkSurfaceHintKey::NK_EGL_CONFIG, reinterpret_cast<uintptr>(eglConfig));

NkWindow window(wc);
NkSurfaceDesc sd = window.GetSurfaceDesc();
uintptr visual = sd.appliedHints.Get(NkSurfaceHintKey::NK_GLX_VISUAL_ID, 0);
```

Conclusion:
- `NkContext` est un helper pratique pour remplir ces hints automatiquement.
- `NkSurfaceHint` reste la couche bas niveau si tu veux un controle total.

## 4) Procedure etudiant - Implementer un backend renderer

## 4.1 Version Surface-first (backend specifique)

1. Creer `MyBackendContext` (device, swapchain, render target, queues, etc).
2. Creer `bool InitFromSurface(const NkSurfaceDesc&, const MyConfig&)`.
3. Mapper la creation native selon plateforme avec `#if`.
4. Implementer:
   - `Resize(width,height)`
   - `BeginFrame()`
   - `Draw(...)`
   - `Present()`
   - `Shutdown()`
5. Boucle app:
   - poll events
   - draw
   - present
6. Nettoyage propre (ordre inverse init).

Template minimal:

```cpp
NkWindow window(cfg);
if (!window.IsOpen()) return -1;

NkSurfaceDesc sd = window.GetSurfaceDesc();
MyBackendContext ctx{};
if (!InitFromSurface(sd, myCfg, ctx)) return -2;

while (window.IsOpen()) {
    PollEvents();
    BeginFrame(ctx);
    DrawScene(ctx);
    Present(ctx);
}

Shutdown(ctx);
```

## 4.2 Version Context-first (OpenGL bootstrap rapide)

1. `NkContextInit()`
2. Configurer hints OpenGL (version/profile/depth/stencil/msaa)
3. `NkContextApplyWindowHints(windowCfg)`
4. Creer fenetre
5. `NkContextCreate(...)`
6. `NkContextMakeCurrent(...)`
7. Charger GL loader (GLAD)
8. Rendu + swap
9. `NkContextDestroy`, `NkContextShutdown`

Template minimal:

```cpp
NkContextInit();
NkContextSetApi(NkRendererApi::NK_OPENGL);
NkContextWindowHint(NkContextHint::NK_CONTEXT_HINT_VERSION_MAJOR, 4);
NkContextWindowHint(NkContextHint::NK_CONTEXT_HINT_VERSION_MINOR, 5);

NkWindowConfig wc{};
NkContextApplyWindowHints(wc);
NkWindow w(wc);

NkContext ctx{};
NkContextCreate(w, ctx);
NkContextMakeCurrent(ctx);
// gladLoadGLLoader((GLADloadproc)NkContextGetProcAddressLoader(ctx));
```

## 4.3 Version Surface-first par backend (concret)

### Vulkan

1. Lire `NkSurfaceDesc`.
2. Creer `VkInstance` + extensions plateforme.
3. Creer `VkSurfaceKHR` depuis les handles natifs:
   - Windows: `HWND/HINSTANCE`
   - XCB: `xcb_connection_t*` + `xcb_window_t`
   - Xlib: `Display*` + `Window`
   - Wayland: `wl_display*` + `wl_surface*`
4. Selectionner GPU/queue/present support.
5. Creer swapchain + images + render pass.

### DirectX11 / DirectX12

1. Lire `NkSurfaceDesc`.
2. Utiliser `HWND` pour creer device + swapchain.
3. Gerer resize/recreate de swapchain sur `NkWindowResizeEvent`.

### Metal

1. Lire `NkSurfaceDesc`.
2. Recuperer `metalLayer` (ou `view`) et le brancher au device Metal.
3. Creer command queue + drawable + present.

### Software

1. Lire `NkSurfaceDesc`.
2. Allouer/gerer ton backbuffer CPU.
3. Copier/afficher via le mecanisme de la plateforme (blit/present logiciel).
4. Sur Wayland, le helper expose aussi `shmPixels/shmBuffer` pour un chemin software simple.

## 5) Regles de correction pour etudiants

Exiger ces points dans chaque rendu:

1. Aucun include direct `NKWindow/Platform/*` dans leur backend public.
2. Le backend lit uniquement `NkSurfaceDesc` (et eventuellement `appliedHints`).
3. Gestion resize/recreate propre.
4. Cleanup sans fuite.
5. Une demo couleur animee minimale pour prouver `Present()` fonctionne.

## 6) Etat actuel important

- OpenGL `NkContext` est en place pour Win32 + Xlib/XCB + Wayland(EGL).
- `NkSurfaceHint` fonctionne en pipeline manuel (sans `NkContext`) via `surfaceHints` -> `appliedHints`.
- Vulkan/DX/Metal/Software sont deja en mode surface-first (handles natifs suffisants).

## 7) Fichiers de reference dans le repo

- `Modules/Runtime/NKWindow/src/NKWindow/Core/NkSurface.h`
- `Modules/Runtime/NKWindow/src/NKWindow/Core/NkSurfaceHint.h`
- `Modules/Runtime/NKWindow/src/NKWindow/Core/NkContext.h`
- `Modules/Runtime/NKWindow/src/NKWindow/Core/NkContext.cpp`
- `Applications/Sandbox/src/main9.cpp` (multi-backend)
- `Applications/Sandbox/src/main10.cpp` a `main15.cpp` (un backend par exemple)
