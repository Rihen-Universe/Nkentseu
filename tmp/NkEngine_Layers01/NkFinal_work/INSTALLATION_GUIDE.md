# NkGraphics — Guide d'installation et d'intégration

## Vue d'ensemble

```
NkFinal/
├── Core/                  # Types, descripteurs, interfaces abstraites
│   ├── NkTypes.h
│   ├── NkSurfaceDesc.h
│   ├── NkGraphicsApi.h
│   ├── NkWGLPixelFormat.h
│   ├── NkOpenGLDesc.h
│   ├── NkContextDesc.h
│   ├── NkContextInfo.h
│   ├── NkIGraphicsContext.h
│   └── NkNativeContextAccess.h   ← accès typé sans cast void*
├── OpenGL/
│   ├── NkOpenGLContextData.h
│   ├── NkOpenGLContext.h/.cpp    ← WGL/GLX/EGL/WebGL/NSGL
│   └── NkOpenGLContext_macOS.mm  ← NSGL Objective-C++
├── Vulkan/
│   ├── NkVulkanContextData.h
│   └── NkVulkanContext.h/.cpp
├── DirectX/                       ← Windows uniquement
│   ├── NkDirectXContextData.h
│   ├── NkDXContext.h
│   ├── NkDX11Context.cpp
│   └── NkDX12Context.cpp
├── Metal/                         ← macOS/iOS uniquement
│   ├── NkMetalContext.h
│   └── NkMetalContext.mm          ← Objective-C++
├── Software/
│   ├── NkSoftwareContext.h
│   └── NkSoftwareContext.cpp
├── Compute/
│   ├── NkIComputeContext.h        ← Interface abstraite Compute
│   ├── NkOpenGLComputeContext.h/.cpp
│   ├── NkVulkanComputeContext.h/.cpp
│   ├── NkDX11ComputeContext.h/.cpp
│   ├── NkDX12ComputeContext.h/.cpp
│   └── NkMetalComputeContext.h    ← stub, .mm à compléter
├── Factory/
│   ├── NkContextFactory.h/.cpp    ← Create/CreateWithFallback/CreateCompute
├── Demo/
│   └── NkGraphicsDemos.cpp
└── CMakeLists.txt
```

---

## 1. GLAD2 (chargeur OpenGL/WGL/GLX/EGL)

### Générer GLAD2
1. Aller sur https://gen.glad.sh/
2. Choisir :
   - **Language** : C/C++
   - **Specification** : OpenGL
   - **Profile** : Core
   - **Version** : 4.6
   - **Extensions** :
     - `WGL_ARB_create_context`
     - `WGL_ARB_create_context_profile`
     - `WGL_ARB_pixel_format`
     - `WGL_EXT_swap_control`
     - `WGL_EXT_swap_control_tear`
     - `GLX_ARB_create_context`
     - `GLX_ARB_create_context_profile`
     - `GLX_EXT_swap_control`
     - `GLX_MESA_swap_control`
     - `GLX_SGI_swap_control`
   - Cocher **Generate loader**
3. Télécharger et placer dans `extern/glad2/`

### Structure attendue
```
extern/glad2/
├── CMakeLists.txt
├── include/
│   └── glad/
│       ├── gl.h
│       ├── wgl.h      (Windows)
│       ├── glx.h      (Linux XLib/XCB)
│       ├── egl.h      (Wayland/Android)
│       └── gles2.h    (ES/WebGL)
└── src/
    ├── gl.c
    ├── wgl.c
    ├── glx.c
    └── egl.c
```

### Sans GLAD2 (NK_NO_GLAD2)
```cmake
target_compile_definitions(NkGraphics PUBLIC NK_NO_GLAD2)
```
Le code utilise alors `wglGetProcAddress` / `glXGetProcAddressARB` directement.

---

## 2. Vulkan SDK

### Windows / Linux / macOS
1. Télécharger : https://vulkan.lunarg.com/sdk/home
2. Installer (ou extraire sur Linux)
3. Définir la variable d'environnement :
   ```bash
   export VULKAN_SDK=/path/to/VulkanSDK/x.y.z/x86_64
   ```
4. CMake le trouvera via `find_package(Vulkan)`

### Vérification
```bash
vulkaninfo --summary
```

### Android
Utiliser le NDK — `vulkan/vulkan.h` est inclus dans le NDK >= r17.

---

## 3. DirectX 11 / 12 (Windows uniquement)

Aucune installation séparée requise.

Les libs nécessaires sont dans le Windows SDK :
```
d3d11.lib     d3d12.lib
dxgi.lib      dxguid.lib
d3dcompiler.lib
```

Le CMakeLists les lie automatiquement via :
```cmake
target_link_libraries(NkGraphics PUBLIC d3d11 d3d12 dxgi dxguid d3dcompiler)
```

### Windows SDK requis
- Minimum : Windows SDK 10.0.19041.0
- Recommandé : Windows SDK 10.0.22621.0 (Win 11)
- Lié automatiquement dans Visual Studio 2019/2022

### Activer le debug layer DX12
```cpp
auto desc = NkContextDesc::MakeDirectX12(true); // debug=true
```
Nécessite les "Graphics Tools" Windows (Paramètres → Applications → Fonctionnalités facultatives).

---

## 4. Metal (macOS / iOS)

Aucune installation séparée — Metal est inclus dans Xcode.

### Exigences
- Xcode 14+ (macOS 12+ recommandé)
- Metal est disponible sur tous les Mac depuis 2012

### Compilation Objective-C++
Les fichiers `.mm` doivent être compilés avec le flag `-x objective-c++` :

```cmake
set_source_files_properties(
    Metal/NkMetalContext.mm
    OpenGL/NkOpenGLContext_macOS.mm
    PROPERTIES COMPILE_FLAGS "-x objective-c++"
)
```

### Frameworks requis
```cmake
target_link_libraries(NkGraphics PUBLIC
    "-framework Metal"
    "-framework MetalKit"
    "-framework QuartzCore"
    "-framework AppKit"    # macOS
    # "-framework UIKit"   # iOS
)
```

---

## 5. Intégration dans NkEngine (CMake)

```cmake
# Dans votre CMakeLists.txt principal
add_subdirectory(NkFinal)
target_link_libraries(NkEngine PRIVATE NkGraphics)
```

### Adaptations NkWindow nécessaires

**Linux XLib/XCB avec OpenGL** : NkWindow doit utiliser le `VisualID` GLX pour créer la fenêtre.
La factory transmet ce hint via `NkSurfaceHints` :

```cpp
// Dans NkWindow::Create() (Linux XLib) :
uintptr_t visualId = surf.appliedHints.Get(
    NkSurfaceHintKey::NK_GLX_VISUAL_ID, 0);
if (visualId) {
    // Utiliser ce VisualID dans XCreateWindow
    XVisualInfo vinfo_template{};
    vinfo_template.visualid = (VisualID)visualId;
    int count = 0;
    XVisualInfo* vinfo = XGetVisualInfo(display, VisualIDMask,
                                         &vinfo_template, &count);
    // → passer vinfo->visual à XCreateWindow
}
```

**Tous les autres backends** : NkWindow ne nécessite aucune modification.

---

## 6. Stub NkWindow minimal (pour compilation standalone)

Si vous compilez NkGraphics sans NkEngine, créez ce stub :

```cpp
// Core/NkWindow_stub.h
#pragma once
#include "NkSurfaceDesc.h"

namespace nkentseu {
    class NkWindow {
    public:
        NkSurfaceDesc GetSurfaceDesc() const { return mSurface; }
        uint32 GetWidth()  const { return mSurface.width; }
        uint32 GetHeight() const { return mSurface.height; }
        bool   IsOpen()    const { return true; }

        NkSurfaceDesc mSurface; // Remplir manuellement pour les tests
    };
}
```

---

## 7. Utilisation — démarrage rapide

### Contexte simple
```cpp
#include "Factory/NkContextFactory.h"
#include "Core/NkNativeContextAccess.h"

// Créer fenêtre NkWindow...
auto desc = NkContextDesc::MakeOpenGL(4, 6);
NkIGraphicsContext* ctx = NkContextFactory::Create(window, desc);

while (running) {
    NkEvent ev;
    while (window.PollEvent(ev)) {
        if (ev.type == NkEventType::Resize)
            ctx->OnResize(ev.resize.width, ev.resize.height);
    }
    ctx->BeginFrame();
    // → Vos commandes de rendu
    ctx->EndFrame();
    ctx->Present();
}
ctx->Shutdown();
delete ctx;
```

### Compute depuis contexte graphique
```cpp
NkIComputeContext* compute = NkContextFactory::ComputeFromGraphics(ctx);

NkComputeBufferDesc bd{ .sizeBytes=1024*4, .cpuWritable=true, .cpuReadable=true };
auto buf = compute->CreateBuffer(bd);
auto shader  = compute->CreateShaderFromSource(glslSource);
auto pipeline= compute->CreatePipeline(shader);

compute->BindPipeline(pipeline);
compute->BindBuffer(0, buf);
compute->Dispatch(4, 1, 1); // 4 groupes × 256 threads = 1024 éléments
compute->WaitIdle();

float result[256];
compute->ReadBuffer(buf, result, sizeof(result));
```

### API auto avec fallback
```cpp
NkIGraphicsContext* ctx = NkContextFactory::CreateWithFallback(window, {
    NkGraphicsApi::NK_API_DIRECTX12,
    NkGraphicsApi::NK_API_VULKAN,
    NkGraphicsApi::NK_API_OPENGL,
    NkGraphicsApi::NK_API_SOFTWARE,
});
printf("API choisie: %s\n", NkGraphicsApiName(ctx->GetApi()));
```

### Accès natif typé (sans cast void*)
```cpp
// OpenGL
HGLRC hglrc  = NkNativeContext::GetWGLContext(ctx);   // Windows
GLXContext glx = NkNativeContext::GetGLXContext(ctx);  // Linux

// Vulkan
VkDevice dev = NkNativeContext::GetVkDevice(ctx);
VkCommandBuffer cmd = NkNativeContext::GetVkCurrentCommandBuffer(ctx);

// DX12
ID3D12Device5* dx12dev = NkNativeContext::GetDX12Device(ctx);
ID3D12GraphicsCommandList4* cmdList = NkNativeContext::GetDX12CommandList(ctx);

// Metal (macOS)
void* metalDev = NkNativeContext::GetMetalDevice(ctx);
// Caster : (__bridge id<MTLDevice>)metalDev
```

---

## 8. Patches de robustesse inclus

| Problème                             | Fix appliqué                              | Fichier                    |
|--------------------------------------|-------------------------------------------|----------------------------|
| Crash OnResize avec fenêtre minimisée| `if (w==0\|\|h==0) return true`           | Tous les contextes         |
| GL renderer vide sans GLAD2          | `FillInfo()` appelé systématiquement      | NkOpenGLContext.cpp        |
| Vulkan OnResize extent 0x0           | Guard + skip RecreateSwapchain            | NkVulkanContext.cpp        |
| DX12 FlushGPU manquant avant resize  | `FlushGPU()` dans DestroySwapchainDep.   | NkDX12Context.cpp          |
| DX12 FlushGPU manquant dans dtor     | `FlushGPU()` dans `~NkDX12Context`        | NkDX12Context.cpp          |
| DX11 device lost                     | `DXGI_ERROR_DEVICE_REMOVED` → HandleDeviceLost() | NkDX11Context.cpp  |
| DX12 device lost                     | Check dans Present()                      | NkDX12Context.cpp          |
| Metal dtor sans Shutdown()           | Auto-shutdown dans destructeur            | NkMetalContext.mm          |
| Software OnResize presenter invalide | `mCachedSurface` + ReInit presenter       | NkSoftwareContext.cpp      |

---

## 9. Multi-contexte — résumé

| Mode                              | Supporté | Notes                                         |
|-----------------------------------|----------|-----------------------------------------------|
| Plusieurs contextes par app       | ✅       | Chaque `Create()` = contexte indépendant      |
| Contexte partagé (asset loader)   | ✅ GL    | `NkOpenGLContext::CreateSharedContext()`      |
| Contexte compute standalone       | ✅       | `NkContextFactory::CreateCompute(api)`        |
| Compute depuis contexte gfx       | ✅       | `NkContextFactory::ComputeFromGraphics(ctx)`  |
| Plusieurs fenêtres                | ✅       | Un contexte par fenêtre                        |
| Contexte offscreen (no window)    | ⚠️ GL   | Possible via pbuffer — non inclus ici          |

---

## 10. Dépendances résumées

| Composant   | Windows              | Linux            | macOS         | Android      | Web         |
|-------------|----------------------|------------------|---------------|--------------|-------------|
| OpenGL      | opengl32, WGL        | libGL, GLX/EGL   | OpenGL.fw     | libGLESv3    | WebGL2      |
| Vulkan      | VulkanSDK            | VulkanSDK        | VulkanSDK     | NDK r17+     | —           |
| DX11        | Windows SDK          | —                | —             | —            | —           |
| DX12        | Windows SDK          | —                | —             | —            | —           |
| Metal       | —                    | —                | Xcode/Metal.fw| —            | —           |
| Software    | GDI (gdi32)          | XLib/XCB/WL      | CoreGraphics  | ANativeWindow| Canvas2D    |
| GLAD2       | optionnel            | optionnel        | optionnel     | optionnel    | optionnel   |
