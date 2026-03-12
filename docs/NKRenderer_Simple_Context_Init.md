# Initialisation simple des contextes graphiques (NkWindow + NKRenderer)

Ce flux remplace l'équivalent GLFW/SDL (`WindowHint` / `MakeCurrent` / `GetProcAddress`) avec une API unifiée.

## 1) OpenGL (équivalent GLFW/SDL)

```cpp
#include "NKWindow/Core/NkWindow.h"
#include "NKRenderer/Deprecate/Context/NkSimpleGraphicsContext.h"

nkentseu::NkWindowConfig wcfg;
wcfg.title = "OpenGL App";

nkentseu::NkSimpleGraphicsContextDesc cdesc;
cdesc.api = nkentseu::NkGraphicsApi::OpenGL;
cdesc.glMajor = 3;
cdesc.glMinor = 3;
cdesc.glProfile = nkentseu::NkOpenGLProfile::Core;
cdesc.debug = true;

// Equivalent des hints GLFW/SDL (GLX sur Linux)
nkentseu::NkSimpleGraphicsContext::PrepareWindowConfig(wcfg, cdesc);

nkentseu::NkWindow window;
if (!window.Create(wcfg)) { return; }

auto ctx = nkentseu::NkSimpleGraphicsContext::Create(window, cdesc);
if (!ctx || !ctx->IsInitialized()) { return; }

// Callback de chargement OpenGL (à utiliser APRÈS MakeCurrent dans le backend GL concret)
auto* gl = ctx->GetNativeHandle<nkentseu::NkOpenGLNativeHandles>();
// gl->getProcAddress est le callback à passer à votre loader OpenGL.
```

## 2) Vulkan (surface-driven)

```cpp
nkentseu::NkSimpleGraphicsContextDesc cdesc;
cdesc.api = nkentseu::NkGraphicsApi::Vulkan;
cdesc.vkValidation = true;

// Pas de window hints Vulkan requis
nkentseu::NkWindow window;
window.Create(wcfg);

auto ctx = nkentseu::NkSimpleGraphicsContext::Create(window, cdesc);
auto* vk = ctx ? ctx->GetNativeHandle<nkentseu::NkVulkanNativeHandles>() : nullptr;
```

## 3) Information obligatoire par API

```cpp
auto info = nkentseu::NkSimpleGraphicsContext::QueryInitInfo(nkentseu::NkGraphicsApi::OpenGL);
// info.requiresWindowHints
// info.requiresCurrentContext
// info.requiresProcAddressLoader
```

Règle pratique:

- OpenGL: oui, il faut un contexte courant + un loader de fonctions.
- Vulkan: pas de contexte courant OpenGL, il faut une surface native.
- DirectX/Metal: contexte/device backend, pas de loader GL.
- Software: seulement surface + framebuffer CPU.

Note: la couche `Context` actuelle fournit le flux d'initialisation unifié et les handles.
La création GPU finale backend (ex: `wglCreateContext/glXCreateContext/eglCreateContext`, `vkCreate*`, `D3D*Create*`) reste à implémenter dans les backends dédiés.
