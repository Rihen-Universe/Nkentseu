#pragma once
// NkIGraphicsContext.h — Interface abstraite contexte graphique + compute
#include "NkContextDesc.h"
#include "NkContextInfo.h"

namespace nkentseu {

    class NkWindow;

    class NkIGraphicsContext {
    public:
        virtual ~NkIGraphicsContext() = default;

        // Cycle de vie
        virtual bool Initialize(const NkWindow& window, const NkContextDesc& desc) = 0;
        virtual void Shutdown()  = 0;
        virtual bool IsValid()   const = 0;

        // Boucle de rendu
        virtual bool BeginFrame() = 0;
        virtual void EndFrame()   = 0;
        virtual void Present()    = 0;

        // Surface
        virtual bool OnResize(uint32 width, uint32 height) = 0;
        virtual void SetVSync(bool enabled) = 0;
        virtual bool GetVSync() const = 0;

        // Infos
        virtual NkGraphicsApi GetApi()  const = 0;
        virtual NkContextInfo GetInfo() const = 0;
        virtual NkContextDesc GetDesc() const = 0;

        // Accès natif opaque — caster via NkNativeContext::XXX()
        virtual void* GetNativeContextData() = 0;

        // Compute
        virtual bool SupportsCompute() const = 0;
    };

    using NkGraphicsContextPtr = std::unique_ptr<NkIGraphicsContext>;

} // namespace nkentseu