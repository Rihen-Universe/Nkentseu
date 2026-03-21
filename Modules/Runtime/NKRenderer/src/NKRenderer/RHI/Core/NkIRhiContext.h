#pragma once
// =============================================================================
// NkIRhiContext.h
// Contexte minimal consomme par le RHI.
//
// Objectif:
//   - decoupler le RHI de NKRenderer/Context
//   - exposer uniquement les informations necessaires au device/swapchain
// =============================================================================
#include "NKRenderer/Context/Core/NkGraphicsApi.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

struct NkRhiContextInfo {
    NkGraphicsApi api         = NkGraphicsApi::NK_API_NONE;
    union { uint32 width;  uint32 windowWidth;  };
    union { uint32 height; uint32 windowHeight; };
    bool          minimized   = false;

    NkRhiContextInfo() : api(NkGraphicsApi::NK_API_NONE), width(0), height(0), minimized(false) {}
};

class NkIRhiContext {
public:
    virtual ~NkIRhiContext() = default;

    virtual bool IsValid() const = 0;
    virtual NkGraphicsApi GetApi() const = 0;
    virtual NkRhiContextInfo GetInfo() const = 0;

    virtual void*       GetNativeContextData()       = 0;
    virtual const void* GetNativeContextData() const = 0;

    // OpenGL/EGL/GLX/WGL peuvent necessiter un contexte courant explicite.
    // No-op pour les backends sans notion de contexte courant thread-local.
    virtual bool MakeCurrent() { return true; }
    virtual void ReleaseCurrent() {}

    virtual bool OnResize(uint32 width, uint32 height) {
        (void)width;
        (void)height;
        return true;
    }

    virtual void Present() {}
};

} // namespace nkentseu
