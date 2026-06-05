#pragma once
// =============================================================================
// NkPixolBuffer.h  — NKRenderer v5.0  (Tools/PixolSculpt/)
//
// Le "canvas pixol" : un jeu de storage images en espace-ecran (NK_UNORDERED_
// ACCESS) — profondeur, normale, materiau, couleur, masque. C'est l'equivalent
// d'un G-buffer, mais ECRIT par l'utilisateur (les kernels de brosse) plutot
// que genere chaque frame par la rasterisation. Un pixol = un pixel qui porte
// Z + normale + materiau (cf. ZBrush).
//
// ⚠️ SQUELETTE — la creation/destruction des textures est a implementer.
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKRenderer/Tools/PixolSculpt/NkSculptTypes.h"

namespace nkentseu {

    class NkIDevice;          // NKRHI
    class NkICommandBuffer;   // NKRHI

    namespace renderer {

        class NkRenderGraph;  // Core/NkRenderGraph.h

        class NkPixolBuffer {
            public:
                NkPixolBuffer() noexcept = default;
                ~NkPixolBuffer() noexcept;

                bool Init(NkIDevice* device, uint32 width, uint32 height,
                          const NkPixolSculptConfig& cfg) noexcept;
                void Shutdown() noexcept;
                bool Resize(uint32 width, uint32 height) noexcept;

                // Remet le canvas a zero (depth = +inf, mask = 0, etc.).
                void Clear(NkICommandBuffer* cmd) noexcept;

                [[nodiscard]] bool   IsValid() const noexcept { return mReady; }
                [[nodiscard]] uint32 Width()   const noexcept { return mWidth; }
                [[nodiscard]] uint32 Height()  const noexcept { return mHeight; }

                // Handles RHI (storage images, NK_UNORDERED_ACCESS).
                [[nodiscard]] NkTextureHandle Depth()    const noexcept { return mDepth; }
                [[nodiscard]] NkTextureHandle Normal()   const noexcept { return mNormal; }
                [[nodiscard]] NkTextureHandle Material() const noexcept { return mMaterial; }
                [[nodiscard]] NkTextureHandle Color()    const noexcept { return mColor; }
                [[nodiscard]] NkTextureHandle Mask()     const noexcept { return mMask; }

                // Importe toutes les cibles dans le render graph (etat initial
                // NK_UNORDERED_ACCESS). Les NkGraphResId sont conserves en interne
                // et exposes via les accesseurs Res*().
                void ImportToGraph(NkRenderGraph* graph) noexcept;

                // NkGraphResId == uint32 (cf. NkRenderGraph.h). On garde uint32
                // ici pour ne pas tirer l'include du graph dans ce header.
                [[nodiscard]] uint32 ResDepth()  const noexcept { return mResDepth; }
                [[nodiscard]] uint32 ResNormal() const noexcept { return mResNormal; }
                [[nodiscard]] uint32 ResColor()  const noexcept { return mResColor; }
                [[nodiscard]] uint32 ResMask()   const noexcept { return mResMask; }

            private:
                bool CreateTargets() noexcept;
                void DestroyTargets() noexcept;

                NkIDevice*          mDevice = nullptr;
                NkPixolSculptConfig mCfg;
                uint32              mWidth = 0, mHeight = 0;
                bool                mReady = false;

                NkTextureHandle mDepth, mNormal, mMaterial, mColor, mMask;
                uint32          mResDepth = 0, mResNormal = 0, mResColor = 0, mResMask = 0;
        };

    } // namespace renderer
} // namespace nkentseu
