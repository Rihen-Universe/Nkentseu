#pragma once
// =============================================================================
// NkNativeHandles.h
// Handles natifs exposÃ©s par chaque backend via GetNativeHandle<T>().
//
// Ces structs sont des POD â€” pas d'allocation, pas de propriÃ©tÃ© (ownership).
// Le contexte graphique les remplit et les expose en lecture.
// Les renderers les consomment pour accÃ©der aux ressources GPU.
//
// IMPORTANT : les pointeurs sont des vues (non-owning).
//   Ne jamais les libÃ©rer depuis un renderer.
//   Les durÃ©es de vie sont gÃ©rÃ©es par NkGraphicsContext::Shutdown().
// =============================================================================

#include "NKWindow/Core/NkTypes.h"
#include "NKPlatform/NkPlatformDetect.h"

// Handles WGL (Win32 OpenGL)
#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#   endif
#   include <windows.h>
#endif

// Handles EGL (Wayland, Android, X11+EGL)
#if defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID) \
 || defined(NKENTSEU_USE_EGL)
#   include <EGL/egl.h>
#endif

namespace nkentseu {

// ===========================================================================
// OpenGL
// ===========================================================================
struct NkOpenGLNativeHandles {
    static constexpr uint32 kTypeId = 0x4F474C00u; // 'OGL\0'

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    // WGL
    HDC   hdc   = nullptr; ///< Device Context (GetDC(hwnd))
    HGLRC hglrc = nullptr; ///< OpenGL Rendering Context (wglCreateContextAttribsARB)

#elif defined(NKENTSEU_PLATFORM_MACOS)
    // NSOpenGLContext (legacy) ou NSOpenGLContext sur CAMetalLayer
    void* nsOpenGLContext = nullptr; ///< NSOpenGLContext* (ObjC opaque)

#elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
    // GLX
    void*  glxContext = nullptr; ///< GLXContext
    void*  display    = nullptr; ///< Display*
    uint64  drawable   = 0;       ///< GLXDrawable (Window XID)
    // GLXFBConfig rÃ©cupÃ©rÃ© depuis NkSurfaceDesc::appliedHints si besoin
    void*  fbConfig   = nullptr; ///< GLXFBConfig (pour les extensions)

#elif defined(NKENTSEU_WINDOWING_WAYLAND) \
   || defined(NKENTSEU_PLATFORM_ANDROID) \
   || defined(NKENTSEU_USE_EGL)
    // EGL
    EGLDisplay eglDisplay = EGL_NO_DISPLAY;
    EGLSurface eglSurface = EGL_NO_SURFACE;
    EGLContext eglContext  = EGL_NO_CONTEXT;

#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    // WebGL via Emscripten
    int webglContext = 0; ///< EMSCRIPTEN_WEBGL_CONTEXT_HANDLE
#endif

    // Pointeur vers la fonction de chargement des extensions.
    // Ã€ passer Ã  gladLoadGLLoader() ou Ã©quivalent.
    // Rempli par le contexte OpenGL aprÃ¨s crÃ©ation.
    using ProcAddressFn = void*(*)( const char* );
    ProcAddressFn getProcAddress = nullptr;
};

// ===========================================================================
// Vulkan
// ===========================================================================
struct NkVulkanNativeHandles {
    static constexpr uint32 kTypeId = 0x564B0000u; // 'VK\0\0'

    // Tous des void* pour Ã©viter d'inclure vulkan.h dans ce header.
    // Caster vers VkInstance, VkDevice, etc. dans le code Vulkan.
    void* instance         = nullptr; ///< VkInstance
    void* physicalDevice   = nullptr; ///< VkPhysicalDevice
    void* device           = nullptr; ///< VkDevice
    void* graphicsQueue    = nullptr; ///< VkQueue
    void* presentQueue     = nullptr; ///< VkQueue
    void* computeQueue     = nullptr; ///< VkQueue (peut Ãªtre = graphicsQueue)
    void* surface          = nullptr; ///< VkSurfaceKHR
    void* swapchain        = nullptr; ///< VkSwapchainKHR
    void* renderPass       = nullptr; ///< VkRenderPass principal
    void* commandPool      = nullptr; ///< VkCommandPool
    void* descriptorPool   = nullptr; ///< VkDescriptorPool global (optionnel)

    uint32 graphicsFamily   = 0;
    uint32 presentFamily    = 0;
    uint32 computeFamily    = 0;
    uint32 swapchainImages  = 0;  ///< Nombre d'images dans la swapchain

    // Frame courante (mis Ã  jour par BeginFrame)
    void* commandBuffer    = nullptr; ///< VkCommandBuffer (frame courante)
    void* framebuffer      = nullptr; ///< VkFramebuffer  (frame courante)
    uint32 currentImage     = 0;       ///< Index dans la swapchain
    uint32 currentFrame     = 0;       ///< Index en vol (0..MAX_FRAMES-1)
};

// ===========================================================================
// Metal
// ===========================================================================
struct NkMetalNativeHandles {
    static constexpr uint32 kTypeId = 0x4D544C00u; // 'MTL\0'

    void* device          = nullptr; ///< id<MTLDevice>
    void* commandQueue    = nullptr; ///< id<MTLCommandQueue>
    void* layer           = nullptr; ///< CAMetalLayer*
    // Frame courante (mis Ã  jour par BeginFrame)
    void* currentDrawable = nullptr; ///< id<CAMetalDrawable>
    void* commandBuffer   = nullptr; ///< id<MTLCommandBuffer>
    void* renderPassDesc  = nullptr; ///< MTLRenderPassDescriptor*
};

// ===========================================================================
// DirectX 11
// ===========================================================================
struct NkDirectX11NativeHandles {
    static constexpr uint32 kTypeId = 0x44583131u; // 'DX11'

    void* device         = nullptr; ///< ID3D11Device*
    void* deviceContext  = nullptr; ///< ID3D11DeviceContext*
    void* swapChain      = nullptr; ///< IDXGISwapChain*
    void* renderTarget   = nullptr; ///< ID3D11RenderTargetView* (backbuffer)
    void* depthStencil   = nullptr; ///< ID3D11DepthStencilView*
};

// ===========================================================================
// DirectX 12
// ===========================================================================
struct NkDirectX12NativeHandles {
    static constexpr uint32 kTypeId = 0x44583132u; // 'DX12'

    void* device          = nullptr; ///< ID3D12Device*
    void* commandQueue    = nullptr; ///< ID3D12CommandQueue*
    void* swapChain       = nullptr; ///< IDXGISwapChain4*
    void* rtvHeap         = nullptr; ///< ID3D12DescriptorHeap* (RTV)
    void* dsvHeap         = nullptr; ///< ID3D12DescriptorHeap* (DSV)
    // Frame courante
    void* commandAllocator= nullptr; ///< ID3D12CommandAllocator*
    void* commandList     = nullptr; ///< ID3D12GraphicsCommandList*
    void* backBuffer      = nullptr; ///< ID3D12Resource* (backbuffer courant)
    uint32 frameIndex      = 0;
};

// ===========================================================================
// Software Renderer
// ===========================================================================
struct NkSoftwareNativeHandles {
    static constexpr uint32 kTypeId = 0x53575200u; // 'SWR\0'

    void*  pixels      = nullptr; ///< Framebuffer RGBA8 (largeur Ã— hauteur Ã— 4 bytes)
    uint32  width       = 0;
    uint32  height      = 0;
    uint32  stride      = 0;       ///< En bytes (= width * 4 pour RGBA8)
    uint32  pixelFormat = 0;       ///< 0 = RGBA8
    // Le contexte logiciel copie ce buffer vers la fenÃªtre dans Present().
};

} // namespace nkentseu