#pragma once
// =============================================================================
// NkOffscreenTarget.h
// Rendu hors-écran : render-to-texture, minimap, portails, shadow maps, picking.
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKContainers/String/NkString.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
    namespace renderer {

        class NkResourceManager;
        class NkRender3D;

        // =============================================================================
        // NkOffscreenTarget — render target réutilisable pour RTT
        // =============================================================================
        class NkOffscreenTarget {
           public:
                NkOffscreenTarget()  = default;
                ~NkOffscreenTarget() { Destroy(); }

                // Créer un render target off-screen
                bool Create(NkIDevice* device, NkResourceManager* resources,
                             uint32 width, uint32 height,
                             NkPixelFormat colorFmt = NkPixelFormat::NK_RGBA16F,
                             bool hasDepth = true,
                             NkMSAA msaa = NkMSAA::NK_1X,
                             const char* debugName = nullptr);

                // Multi-attachments (ex: GBuffer)
                bool CreateMRT(NkIDevice* device, NkResourceManager* resources,
                                uint32 width, uint32 height,
                                const NkPixelFormat* colorFmts, uint32 colorCount,
                                bool hasDepth = true,
                                const char* debugName = nullptr);

                void Destroy();
                void Resize(uint32 w, uint32 h);
                bool IsValid() const { return mColorTexture.IsValid(); }

                // ── Accès aux textures de résultat ────────────────────────────────────────
                NkTextureHandle GetColor   (uint32 slot = 0) const;
                NkTextureHandle GetDepth   ()                const { return mDepthTexture; }
                uint32          GetWidth   ()                const { return mWidth; }
                uint32          GetHeight  ()                const { return mHeight; }

                // ── Frame RTT ─────────────────────────────────────────────────────────────
                void BeginCapture(NkICommandBuffer* cmd,
                                   bool clearColor = true,
                                   const NkColorF& clearC = NkColorF(0,0,0,1),
                                   bool clearDepth = true);
                void EndCapture  (NkICommandBuffer* cmd);
                bool IsCapturing ()                          const { return mCapturing; }

                // ── Résolution du render target pour binding ──────────────────────────────
                NkRenderTargetHandle GetRTHandle() const { return mRTHandle; }

           private:
                NkIDevice*         mDevice    = nullptr;
                NkResourceManager* mResources = nullptr;
                uint32             mWidth     = 0;
                uint32             mHeight    = 0;
                bool               mCapturing = false;

                NkTextureHandle      mColorTexture;
                NkVector<NkTextureHandle> mColorTextures;  // Pour MRT
                NkTextureHandle      mDepthTexture;
                NkRenderTargetHandle mRTHandle;
        };

    } // namespace renderer
} // namespace nkentseu
