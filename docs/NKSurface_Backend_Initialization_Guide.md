# Guide d'initialisation des backends de rendu avec `NkSurface` et `NkSurfaceHint`

## Objectif

Ce document montre comment utiliser `NkSurfaceDesc` et `NkSurfaceHints` pour initialiser les bases d'un backend de rendu:

- Software
- OpenGL (WGL/GLX/EGL/WebGL)
- Vulkan
- DirectX 11 / DirectX 12
- Metal

Références principales:

- `Modules/Runtime/NKWindow/src/NKWindow/Core/NkSurface.h`
- `Modules/Runtime/NKWindow/src/NKWindow/Core/NkSurfaceHint.h`
- `Modules/Runtime/NKWindow/src/NKWindow/Core/NkWindowConfig.h`

## 1. Flux commun

Le flux recommandé est toujours:

1. Préparer `NkWindowConfig`.
2. Ajouter des `surfaceHints` uniquement si nécessaire.
3. Créer la fenêtre (`NkWindow`).
4. Récupérer `NkSurfaceDesc` via `window.GetSurfaceDesc()`.
5. Initialiser le contexte du backend avec ce `NkSurfaceDesc`.

Exemple de base:

```cpp
#include "NKWindow/NkWindow.h"

nkentseu::NkWindowConfig wcfg;
wcfg.title = "App";

nkentseu::NkWindow window;
if (!window.Create(wcfg)) {
    // gérer erreur
    return;
}

const nkentseu::NkSurfaceDesc sd = window.GetSurfaceDesc();
// sd est l'entrée unique pour créer le contexte backend.
```

## 2. `NkSurfaceHint`: quand et comment l'utiliser

`NkSurfaceHint` sert à transporter des paramètres opaques de création de surface.

Cas utiles actuels:

- `NK_GLX_VISUAL_ID`
- `NK_GLX_FB_CONFIG_PTR`

Cas qui n'ont généralement pas besoin de hint pré-création:

- Vulkan
- Metal
- DirectX 11/12
- Software
- EGL (Wayland/Android, selon design actuel)

Exemple GLX:

```cpp
wcfg.surfaceHints.Set(
    nkentseu::NkSurfaceHintKey::NK_GLX_VISUAL_ID,
    static_cast<nkentseu::uintptr>(visualId));

wcfg.surfaceHints.Set(
    nkentseu::NkSurfaceHintKey::NK_GLX_FB_CONFIG_PTR,
    reinterpret_cast<nkentseu::uintptr>(fbConfig));
```

Après création, le backend peut relire les hints appliqués via `sd.appliedHints`.

## 3. Software rendering

### But

Allouer un framebuffer CPU (`RGBA8`) puis le présenter vers l'OS avec les handles de `NkSurfaceDesc`.

### Initialisation minimale

```cpp
struct SoftwareContext {
    nkentseu::NkVector<nkentseu::uint8> pixels;
    nkentseu::uint32 width = 0;
    nkentseu::uint32 height = 0;
    nkentseu::uint32 pitch = 0;
};

bool InitSoftwareFromSurface(const nkentseu::NkSurfaceDesc& sd, SoftwareContext& ctx) {
    if (!sd.IsValid()) return false;
    ctx.width = sd.width;
    ctx.height = sd.height;
    ctx.pitch = sd.width * 4;
    ctx.pixels.Resize(static_cast<nkentseu::usize>(ctx.pitch) * ctx.height, 0);
    return true;
}
```

### Présentation

La présentation dépend de la plateforme:

- Win32: `HWND`
- XCB: `connection + window`
- XLib: `display + window`
- Wayland: `surface + shmPixels/shmBuffer/shmStride`
- Android: `ANativeWindow*`
- Web: `canvasId`

## 4. OpenGL

## 4.1 Win32 / WGL

Entrées nécessaires depuis `NkSurfaceDesc`:

- `sd.hwnd`
- `sd.hinstance`

Séquence:

1. `GetDC(sd.hwnd)`
2. `SetPixelFormat(...)`
3. `wglCreateContextAttribsARB(...)` (ou legacy)
4. `wglMakeCurrent(...)`

Pas de hint requis en amont dans le design actuel.

## 4.2 Linux / GLX (XLib, XCB)

Entrées nécessaires:

- XLib: `sd.display`, `sd.window`, `sd.screen`
- XCB: `sd.connection`, `sd.window`, `sd.screen`
- Hints GLX: `sd.appliedHints`

Séquence typique:

1. Préparer `NK_GLX_VISUAL_ID` (+ optionnel `NK_GLX_FB_CONFIG_PTR`) avant `Create()`.
2. Créer la fenêtre.
3. Lire `sd.appliedHints`.
4. Créer le contexte GLX avec la visual/FBConfig correspondante.

Note XCB:

- si backend GLX pur, il faut un `Display*` compatible.
- vous pouvez soit exposer un helper dédié côté plateforme, soit utiliser un bridge Xlib/XCB dans votre backend.

## 4.3 Wayland/Android / EGL

Entrées nécessaires:

- Wayland: `sd.display` (`wl_display*`), `sd.surface` (`wl_surface*`)
- Android: `sd.nativeWindow` (`ANativeWindow*`)

Séquence:

1. `eglGetDisplay(...)`
2. `eglInitialize(...)`
3. choisir config EGL
4. `eglCreateWindowSurface(...)`
5. `eglCreateContext(...)`
6. `eglMakeCurrent(...)`

## 4.4 Web / WebGL

Entrée nécessaire:

- `sd.canvasId`

Séquence:

1. Créer contexte WebGL sur `canvasId`.
2. Configurer viewport avec `sd.width/sd.height`.

## 5. Vulkan

## 5.1 Données de base à définir

Un backend Vulkan doit au minimum créer:

- `VkInstance`
- `VkSurfaceKHR` (depuis `NkSurfaceDesc`)
- `VkPhysicalDevice`
- `VkDevice`
- queues (graphics/present)
- swapchain + image views

## 5.2 Création de surface par plateforme (depuis `NkSurfaceDesc`)

Mapping direct:

- Win32: `sd.hwnd`, `sd.hinstance` -> `vkCreateWin32SurfaceKHR`
- XLib: `sd.display`, `sd.window` -> `vkCreateXlibSurfaceKHR`
- XCB: `sd.connection`, `sd.window` -> `vkCreateXcbSurfaceKHR`
- Wayland: `sd.display`, `sd.surface` -> `vkCreateWaylandSurfaceKHR`
- Android: `sd.nativeWindow` -> `vkCreateAndroidSurfaceKHR`
- macOS/iOS: `sd.metalLayer` -> `vkCreateMetalSurfaceEXT` (MoltenVK)

Exemple simplifié:

```cpp
bool CreateVulkanSurface(const nkentseu::NkSurfaceDesc& sd, VkInstance instance, VkSurfaceKHR& outSurface) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    VkWin32SurfaceCreateInfoKHR ci{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    ci.hwnd = sd.hwnd;
    ci.hinstance = sd.hinstance;
    return vkCreateWin32SurfaceKHR(instance, &ci, nullptr, &outSurface) == VK_SUCCESS;
#elif defined(NKENTSEU_WINDOWING_WAYLAND)
    VkWaylandSurfaceCreateInfoKHR ci{VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR};
    ci.display = reinterpret_cast<wl_display*>(sd.display);
    ci.surface = reinterpret_cast<wl_surface*>(sd.surface);
    return vkCreateWaylandSurfaceKHR(instance, &ci, nullptr, &outSurface) == VK_SUCCESS;
#endif
}
```

Aucun `NkSurfaceHint` n'est requis en standard pour Vulkan.

## 6. DirectX 11 / DirectX 12

Entrée nécessaire:

- `sd.hwnd` (Win32)

Séquence DX11:

1. `D3D11CreateDevice(...)`
2. `IDXGIFactory` + `CreateSwapChainForHwnd(...)`
3. créer RTV/DSV

Séquence DX12:

1. `D3D12CreateDevice(...)`
2. `CreateCommandQueue(...)`
3. `CreateSwapChainForHwnd(...)`
4. heaps RTV/DSV + backbuffers

Pas de hint requis.

## 7. Metal

Entrées nécessaires:

- macOS/iOS: `sd.view`, `sd.metalLayer`

Séquence:

1. `id<MTLDevice> device = MTLCreateSystemDefaultDevice()`
2. associer `device` à `CAMetalLayer`
3. créer `commandQueue`
4. `nextDrawable` à chaque frame

Pas de hint requis.

## 8. Checklist d'intégration backend

Pour chaque backend, vérifier:

1. `sd.IsValid()`.
2. champs natifs requis non nuls.
3. resize: recréer les ressources dépendantes de `sd.width/sd.height`.
4. cycle de vie: `Init -> BeginFrame -> EndFrame -> Present -> Shutdown`.
5. fallback propre si backend indisponible.

## 9. Notes d'architecture

- `NkWindow` expose le descripteur de surface, mais ne doit pas contenir la logique GPU.
- `NkSurfaceHint` doit rester opaque et minimal.
- les hints doivent être appliqués lors de la création fenêtre, puis remontés via `sd.appliedHints`.
- les backends GPU peuvent coexister tant qu'ils partagent le même contrat `NkSurfaceDesc`.
