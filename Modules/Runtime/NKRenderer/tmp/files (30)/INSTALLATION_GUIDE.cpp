// =============================================================================
// INSTALLATION_GUIDE.md (sous forme de commentaires structurés)
// Guide complet d'installation et d'intégration de chaque API graphique
// dans le système NkGraphics.
// =============================================================================

/*
================================================================================
 GLAD2 — Loader OpenGL (obligatoire pour OpenGL, WGL, GLX, EGL)
================================================================================

RÔLE :
  GLAD2 génère un loader C/C++ qui charge dynamiquement toutes les fonctions
  OpenGL, WGL (Windows), GLX (Linux), EGL (Wayland/Android) au runtime.
  Il expose aussi toutes les constantes (WGL_ARB_*, GLX_ARB_*, etc.)
  → Plus aucun magic number hexadécimal dans le code.

GÉNÉRATION (en ligne) :
  1. Aller sur : https://gen.glad.dav1d.de/
  2. Configurer :
       Language  : C/C++
       API       : gl 4.6 (ou gles2 3.2 pour mobile)
       Spec      : gl
       Extensions à cocher :
         OpenGL core        : (tout, ou seulement ce dont tu as besoin)
         WGL (Windows)      :
           WGL_ARB_create_context
           WGL_ARB_create_context_profile
           WGL_ARB_create_context_robustness
           WGL_ARB_create_context_no_error
           WGL_ARB_pixel_format
           WGL_ARB_framebuffer_sRGB
           WGL_ARB_multisample
           WGL_EXT_swap_control
           WGL_EXT_swap_control_tear
         GLX (Linux)        :
           GLX_ARB_create_context
           GLX_ARB_create_context_profile
           GLX_ARB_multisample
           GLX_EXT_swap_control
           GLX_MESA_swap_control
           GLX_SGI_swap_control
         EGL (Wayland/Android) :
           EGL_KHR_create_context
           EGL_KHR_no_config_context
       Options    : Loader=On, Header-Only=Off, Debug=Off (ou On pour dev)
  3. Télécharger le zip → extraire dans extern/glad2/

STRUCTURE DES FICHIERS GÉNÉRÉS :
  extern/glad2/
    include/
      glad/
        gl.h        ← OpenGL core (toujours nécessaire)
        wgl.h       ← Windows WGL extensions
        glx.h       ← Linux GLX extensions
        egl.h       ← EGL (Wayland, Android, X11-EGL)
        gles2.h     ← OpenGL ES 2.0/3.x
    src/
      gl.c
      wgl.c
      glx.c
      egl.c
      gles2.c

INTÉGRATION CMAKE :
  Option A — glad_add_library (recommandé si GLAD2 fournit CMakeLists.txt) :
    add_subdirectory(extern/glad2)
    target_link_libraries(NkEngine PRIVATE glad)

  Option B — sources directes :
    add_library(glad STATIC
        extern/glad2/src/gl.c
        $<$<PLATFORM_ID:Windows>:extern/glad2/src/wgl.c>
        $<$<AND:$<PLATFORM_ID:Linux>,$<BOOL:${NKENTSEU_WINDOWING_XLIB}>>:extern/glad2/src/glx.c>
        $<$<BOOL:${NKENTSEU_WINDOWING_WAYLAND}>:extern/glad2/src/egl.c>
        $<$<PLATFORM_ID:Android>:extern/glad2/src/gles2.c>
    )
    target_include_directories(glad PUBLIC extern/glad2/include)
    target_link_libraries(NkEngine PRIVATE glad)

ALTERNATIVE — vcpkg :
  vcpkg install glad
  CMake : find_package(glad CONFIG REQUIRED)
          target_link_libraries(NkEngine PRIVATE glad::glad)

ORDRE D'APPEL DANS LE CODE (automatique via NkOpenGLContext) :
  Windows  : wglMakeCurrent(hdc, tempCtx)
             gladLoadWGL(hdc, wglGetProcAddress)    ← charge WGL ARB
             wglCreateContextAttribsARB(...)         ← maintenant disponible
             wglMakeCurrent(hdc, finalCtx)
             gladLoadGL(wglGetProcAddress)            ← charge tout OpenGL
  Linux    : glXMakeCurrent(display, window, ctx)
             gladLoadGLX(display, screen, glXGetProcAddressARB)
             gladLoadGL(glXGetProcAddressARB)
  Wayland  : eglMakeCurrent(...)
             gladLoadEGL(eglDisplay, eglGetProcAddress)
             gladLoadGLES2(eglGetProcAddress)

================================================================================
 VULKAN — SDK LunarG
================================================================================

WINDOWS :
  1. Télécharger le SDK : https://vulkan.lunarg.com/sdk/home#windows
  2. Lancer l'installeur (VulkanSDK-x.x.xxx.0-Installer.exe)
  3. Variables d'environnement auto-configurées : VULKAN_SDK, VK_LAYER_PATH
  4. Tester : vkcube (dans le SDK) doit tourner
  CMake :
    find_package(Vulkan REQUIRED)
    target_link_libraries(NkEngine PRIVATE Vulkan::Vulkan)

LINUX (Ubuntu/Debian) :
  # Vulkan loader + headers
  sudo apt install libvulkan-dev vulkan-tools vulkan-validationlayers-dev
  # GPU Intel
  sudo apt install mesa-vulkan-drivers
  # GPU NVIDIA : installer le driver propriétaire (>= 390.x)
  # GPU AMD
  sudo apt install mesa-vulkan-drivers
  CMake : identique à Windows (find_package fonctionne via pkg-config)

LINUX — Via SDK LunarG :
  1. https://vulkan.lunarg.com/sdk/home#linux
  2. tar xf vulkansdk-linux-x86_64-x.x.xxx.tar.gz
  3. source vulkan-sdk-x.x.xxx/setup-env.sh  (à mettre dans .bashrc)

MACOS :
  # Option 1 : SDK LunarG (inclut MoltenVK)
  https://vulkan.lunarg.com/sdk/home#mac
  # Option 2 : Homebrew
  brew install molten-vk
  CMake :
    find_package(Vulkan REQUIRED)
    target_link_libraries(NkEngine PRIVATE Vulkan::Vulkan)
    # MoltenVK nécessite aussi :
    target_link_libraries(NkEngine PRIVATE "-framework Metal" "-framework IOSurface")

ANDROID :
  # Inclus dans le NDK 25+
  # Dans build.gradle ou CMakeLists.txt Android :
  target_link_libraries(NkEngine PRIVATE vulkan)
  # Headers : ${ANDROID_NDK}/sources/third_party/vulkan/src/include/

VALIDATION LAYERS :
  Activées via NkContextDesc::vulkan.validationLayers = true
  → Affiche les erreurs Vulkan dans la console
  → Ne jamais activer en production (impact perf)

INTÉGRATION SYSTÈME :
  NkVulkanContext::CreateInstance()   → charge les extensions platform
  NkVulkanContext::CreateSurface()    → VkSurfaceKHR depuis NkSurfaceDesc
  NkVulkanContext::SelectPhysicalDevice() → préfère GPU discret
  Accès natif : NkNativeContext::GetVkDevice(ctx.get())
                NkNativeContext::GetVkCurrentCommandBuffer(ctx.get())

================================================================================
 DIRECTX 11 / 12 — Windows SDK
================================================================================

INSTALLATION :
  DirectX 11 et 12 sont inclus dans le Windows SDK.
  Aucun téléchargement séparé.

  Vérifier que Windows SDK est installé :
  → Visual Studio Installer → "Desktop development with C++"
    → cocher "Windows 10/11 SDK (10.0.22621)"

  Ou télécharger manuellement :
  https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/

CMAKE :
  # DX11
  target_link_libraries(NkEngine PRIVATE d3d11 dxgi d3dcompiler)

  # DX12
  target_link_libraries(NkEngine PRIVATE d3d12 dxgi d3dcompiler dxguid)

OUTILS DE DEBUG :
  PIX for Windows (Microsoft, recommandé DX12) :
    https://devblogs.microsoft.com/pix/download/
    → GPU captures, timing, shader debugging

  RenderDoc (open-source, DX11 + DX12) :
    https://renderdoc.org/
    → Frame capture, analyse des draw calls

  Visual Studio Graphics Diagnostics :
    → Intégré à VS, capture basique

VALIDATION (debug layer) :
  NkContextDesc::dx12.debugDevice = true
  NkContextDesc::dx12.gpuValidation = true  // Très lent, bug-hunting seulement

  La debug layer DX12 s'installe via :
  → Windows Features → "Graphics Tools" (optionnel)
  → Ou : Add-AppxPackage -Online -Name Microsoft.Direct3D.D3D12...

INTÉGRATION SYSTÈME :
  NkDX11Context : CreateDeviceAndSwapchain() + CreateRenderTargets()
  NkDX12Context : Enable/Create 10 étapes explicites
  Accès natif   : NkNativeContext::GetDX11Device(ctx)
                  NkNativeContext::GetDX12CommandList(ctx)
                  NkNativeContext::GetDX12CurrentRTV(ctx)

================================================================================
 METAL — macOS / iOS
================================================================================

INSTALLATION :
  Metal est inclus dans Xcode (macOS 10.11+ / iOS 8+).
  1. Installer Xcode depuis l'App Store ou https://developer.apple.com/xcode/
  2. Accepter la licence : sudo xcodebuild -license accept
  3. Installer les Command Line Tools : xcode-select --install

CMAKE :
  find_package(Metal REQUIRED)  # ou utiliser les frameworks directement :
  target_link_libraries(NkEngine PRIVATE
      "-framework Metal"
      "-framework MetalKit"
      "-framework QuartzCore"
      "-framework AppKit"     # macOS
      # "-framework UIKit"    # iOS
  )

  IMPORTANT : Les fichiers .mm (Objective-C++) doivent être compilés avec :
  set_source_files_properties(NkMetalContext.mm
      PROPERTIES COMPILE_FLAGS "-x objective-c++")

OUTILS DE DEBUG :
  Xcode GPU Frame Capture (intégré) :
    → Product → Profile → Metal System Trace
    → Ou : activer MTL_DEBUG_LAYER=1 dans les env vars

  RenderDoc + Metal Extension :
    https://renderdoc.org/ (support Metal expérimental)

INTÉGRATION SYSTÈME :
  NkMetalContext utilise CAMetalLayer créée par NkCocoaWindow.
  NkSurfaceDesc::metalLayer = (void*)caMetalLayer
  Accès natif : NkNativeContext::GetMetalDevice(ctx)
                NkNativeContext::GetMetalCommandEncoder(ctx)
  → Cast en id<MTLDevice> / id<MTLRenderCommandEncoder> côté .mm

================================================================================
 SOFTWARE RENDERER — Aucune dépendance
================================================================================

INSTALLATION :
  Rien à installer. Le renderer software est 100% CPU, standard C++17.

  Bibliothèques de présentation utilisées (incluses dans l'OS) :
    Windows  : GDI (CreateDIBSection, BitBlt) — inclus dans windows.h
    Linux    : libX11, libXext (XShm)
               sudo apt install libx11-dev libxext-dev
    Wayland  : libwayland-client
               sudo apt install libwayland-dev
    Android  : ANativeWindow (NDK, inclus)
    Web      : Canvas 2D API (navigateur, aucun install)

PERFORMANCES :
  Ce renderer est adapté pour :
  → Tests sans GPU, débogage de logique rendu
  → Plateformes sans GPU (serveurs, CI)
  → Outils d'édition légère
  Non adapté pour : rendu temps réel haute performance

================================================================================
 RÉSUMÉ CMAKE COMPLET
================================================================================

cmake_minimum_required(VERSION 3.22)
project(NkEngine CXX)
set(CMAKE_CXX_STANDARD 17)

# ── Détection de la plateforme ───────────────────────────────────────────────
if(WIN32)
    add_compile_definitions(NKENTSEU_PLATFORM_WINDOWS)
elseif(APPLE)
    add_compile_definitions(NKENTSEU_PLATFORM_MACOS)
elseif(ANDROID)
    add_compile_definitions(NKENTSEU_PLATFORM_ANDROID)
elseif(EMSCRIPTEN)
    add_compile_definitions(NKENTSEU_PLATFORM_EMSCRIPTEN)
else()
    # Linux — choisir le backend de fenêtrage
    option(NK_USE_WAYLAND "Use Wayland windowing" OFF)
    option(NK_USE_XCB     "Use XCB windowing"     OFF)
    if(NK_USE_WAYLAND)
        add_compile_definitions(NKENTSEU_WINDOWING_WAYLAND)
    elseif(NK_USE_XCB)
        add_compile_definitions(NKENTSEU_WINDOWING_XCB)
    else()
        add_compile_definitions(NKENTSEU_WINDOWING_XLIB)  # défaut Linux
    endif()
endif()

# ── GLAD2 ────────────────────────────────────────────────────────────────────
add_subdirectory(extern/glad2)
# ou :
# add_library(glad STATIC extern/glad2/src/gl.c extern/glad2/src/wgl.c ...)
# target_include_directories(glad PUBLIC extern/glad2/include)

# ── Vulkan ───────────────────────────────────────────────────────────────────
find_package(Vulkan REQUIRED)

# ── Sources ──────────────────────────────────────────────────────────────────
add_executable(NkEngine
    platform/<platform>/main.cpp
    # ... sources NkGraphics ...
    Demo/NkGraphicsDemos.cpp
)

target_include_directories(NkEngine PRIVATE
    NkGraphics/Core
    NkGraphics/OpenGL
    NkGraphics/Vulkan
    NkGraphics/DirectX
    NkGraphics/Metal
    NkGraphics/Software
    NkGraphics/Factory
    extern/glad2/include
)

target_link_libraries(NkEngine PRIVATE glad Vulkan::Vulkan)

# ── Libs selon plateforme ────────────────────────────────────────────────────
if(WIN32)
    target_link_libraries(NkEngine PRIVATE d3d11 d3d12 dxgi d3dcompiler dxguid)
elseif(APPLE)
    target_link_libraries(NkEngine PRIVATE
        "-framework Metal" "-framework MetalKit"
        "-framework QuartzCore" "-framework AppKit")
elseif(ANDROID)
    target_link_libraries(NkEngine PRIVATE EGL GLESv3 android)
elseif(EMSCRIPTEN)
    target_link_options(NkEngine PRIVATE "-sUSE_WEBGL2=1" "-sUSE_GLFW=3")
else()  # Linux
    if(NKENTSEU_WINDOWING_XLIB)
        target_link_libraries(NkEngine PRIVATE GL X11 Xext)
    elseif(NKENTSEU_WINDOWING_XCB)
        target_link_libraries(NkEngine PRIVATE GL xcb xcb-image X11 X11-xcb)
    elseif(NKENTSEU_WINDOWING_WAYLAND)
        target_link_libraries(NkEngine PRIVATE EGL GLESv3 wayland-client xkbcommon)
    endif()
endif()
*/
