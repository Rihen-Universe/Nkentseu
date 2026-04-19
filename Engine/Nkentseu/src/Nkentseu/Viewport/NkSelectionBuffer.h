#pragma once
// =============================================================================
// Nkentseu/Viewport/NkSelectionBuffer.h
// =============================================================================
// Picking GPU par Color-ID Buffer.
// Rend une frame en remplaçant les shaders par un shader "flat color ID",
// lit le pixel sous le curseur pour identifier l'objet cliqué.
// =============================================================================
#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKRenderer/src/NkRenderer.h"

namespace nkentseu {
    using namespace math;
    using namespace renderer;

    struct NkPickResult {
        ecs::NkEntityId entity  = ecs::NkEntityId::Invalid();
        uint32          subIdx  = 0;    // face / vertex / edge index si pertinent
        float32         depth   = 1.f;  // profondeur normalisée [0..1]
        NkVec3f         worldPos = {};  // position monde approx (via depth reconstruct)
    };

    class NkSelectionBuffer {
    public:
        NkSelectionBuffer() noexcept = default;
        ~NkSelectionBuffer() noexcept { Shutdown(); }

        bool Init(NkIDevice* device, uint32 width, uint32 height) noexcept;
        void Shutdown() noexcept;
        void Resize(uint32 w, uint32 h) noexcept;

        // Rend les entités avec leur ID couleur (appeler avant le rendu normal)
        void RenderIDPass(NkICommandBuffer* cmd,
                          ecs::NkWorld& world,
                          const NkViewportCamera& cam,
                          float32 aspect) noexcept;

        // Lit le résultat du picking pour un pixel
        [[nodiscard]] NkPickResult Pick(uint32 pixelX, uint32 pixelY) const noexcept;

        // Box-select : retourne tous les objets dans un rectangle écran
        void BoxSelect(uint32 x0, uint32 y0, uint32 x1, uint32 y1,
                       NkVector<ecs::NkEntityId>& out) const noexcept;

        static uint32   EntityToColor(ecs::NkEntityId id) noexcept;
        static ecs::NkEntityId ColorToEntity(uint32 color) noexcept;

    private:
        NkIDevice*      mDevice = nullptr;
        NkRenderTargetHandle mColorRT;   // R8G8B8A8 — ID couleur
        NkRenderTargetHandle mDepthRT;
        NkShaderHandle  mIDShader;
        uint32          mWidth = 0, mHeight = 0;

        mutable NkVector<uint8> mReadback; // Buffer CPU pour readback
        mutable bool            mDirty = true;

        void Readback() const noexcept;
    };
} // namespace nkentseu
