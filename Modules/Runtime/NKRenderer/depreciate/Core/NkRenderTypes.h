#pragma once
// =============================================================================
// NkRenderTypes.h — Types, enums et forward declarations NKRenderer.
// =============================================================================
#include "NKCore/NkTypes.h"
#include "NKMath/NkVec.h"
#include "NKMath/NkMat.h"
#include "NKMath/NkColor.h"
#include "NKMath/NkRectangle.h"
#include "NKMemory/NkSharedPtr.h"
#include "NKRHI/Core/NkTypes.h"
#include "NKRenderer/NkRendererExport.h"

namespace nkentseu {

    // ── Aliases pratiques ──────────────────────────────────────────────────────
    using NkVec2 = math::NkVec2f;
    using NkVec3 = math::NkVec3f;
    using NkVec4 = math::NkVec4f;
    using NkMat4 = math::NkMat4f;
    using NkColor4 = math::NkColor;   // RGBA uint8
    // Couleur normalisée float32 [0,1] — pratique pour les uniforms GPU
    struct NkColor4f { float32 r=1,g=1,b=1,a=1; };

    inline NkColor4f NkColorToF(const math::NkColor& c) {
        return {c.r/255.f, c.g/255.f, c.b/255.f, c.a/255.f};
    }

    // ── Forward declarations ───────────────────────────────────────────────────
    class NkTexture;
    class NkShader;
    class NkMaterial;
    class NkPBRMaterial;
    class NkFilmMaterial;
    class NkCamera2D;
    class NkCamera3D;
    class NkRenderer2D;
    class NkRenderer3D;
    class NkRenderer;

    // ── Shared pointer aliases ──────────────────────────────────────────────────
    using NkTextureRef    = memory::NkSharedPtr<NkTexture>;
    using NkShaderRef     = memory::NkSharedPtr<NkShader>;
    using NkMaterialRef   = memory::NkSharedPtr<NkMaterial>;
    using NkPBRMaterialRef   = memory::NkSharedPtr<NkPBRMaterial>;
    using NkFilmMaterialRef  = memory::NkSharedPtr<NkFilmMaterial>;

    // ── Primitive topologie (alias du RHI) ────────────────────────────────────
    using NkTopology = NkPrimitiveTopology;

    // ── Mode de rendu 3D ──────────────────────────────────────────────────────
    enum class NkRenderMode : uint8 {
        NK_SOLID,       // Rendu solide standard
        NK_WIREFRAME,   // Maillage filaire
        NK_POINTS,      // Nuage de vertices
        NK_LINES,       // Nuage de edge
        NK_FACES,       // Nuage de face
    };

    // ── Espace de projection ──────────────────────────────────────────────────
    enum class NkProjectionSpace : uint8 {
        NK_NDC_ZERO_ONE,  // Vulkan / DX (z ∈ [0,1])
        NK_NDC_NEG_ONE,   // OpenGL      (z ∈ [-1,1])
    };

    // ── Résultat d'une opération renderer ─────────────────────────────────────
    enum class NkRendererResult : uint8 {
        NK_OK = 0,
        NK_NOT_INITIALIZED,
        NK_INVALID_PARAM,
        NK_OUT_OF_MEMORY,
        NK_GPU_ERROR,
    };

    inline bool NkRenderOk(NkRendererResult r) { return r == NkRendererResult::NK_OK; }

} // namespace nkentseu
