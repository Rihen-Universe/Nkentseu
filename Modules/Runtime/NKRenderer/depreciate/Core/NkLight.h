#pragma once
// =============================================================================
// NkLight.h — Types de lumières pour NkRenderer3D.
//
//  NkDirectionalLight — Soleil / lune (direction, pas de position)
//  NkPointLight       — Ampoule (position + portée)
//  NkSpotLight        — Lampe de scène (position + direction + cône)
//  NkAreaLight        — Panneau lumineux (rectangle, forme + étendue)
// =============================================================================
#include "NKRenderer/Core/NkRenderTypes.h"

namespace nkentseu {

    // =========================================================================
    // NkLightType
    // =========================================================================
    enum class NkLightType : uint8 {
        NK_DIRECTIONAL = 0,
        NK_POINT,
        NK_SPOT,
        NK_AREA,
    };

    // =========================================================================
    // NkDirectionalLight
    // =========================================================================
    struct NkDirectionalLight {
        NkVec3    direction  = {-0.577f, -0.577f, -0.577f}; // normalisé
        NkColor4f color      = {1,1,1,1};
        float32   intensity  = 1.f;
        bool      castShadow = true;
    };

    // =========================================================================
    // NkPointLight
    // =========================================================================
    struct NkPointLight {
        NkVec3    position   = {};
        NkColor4f color      = {1,1,1,1};
        float32   intensity  = 1.f;
        float32   range      = 10.f;     // portée max (en unités monde)
        float32   falloff    = 2.f;      // exposant d'atténuation (2 = physique)
        bool      castShadow = false;
    };

    // =========================================================================
    // NkSpotLight
    // =========================================================================
    struct NkSpotLight {
        NkVec3    position      = {};
        NkVec3    direction     = {0, -1, 0}; // normalisé
        NkColor4f color         = {1,1,1,1};
        float32   intensity     = 1.f;
        float32   range         = 10.f;
        float32   innerConeDeg  = 25.f;    // pénombre intérieure
        float32   outerConeDeg  = 35.f;    // cône complet
        bool      castShadow    = false;
    };

    // =========================================================================
    // NkAreaLight
    // =========================================================================
    struct NkAreaLight {
        NkVec3    position   = {};
        NkVec3    normal     = {0, -1, 0};
        NkVec3    right      = {1, 0, 0};
        NkVec2    size       = {1, 1};    // largeur × hauteur du panneau
        NkColor4f color      = {1,1,1,1};
        float32   intensity  = 1.f;
    };

    // =========================================================================
    // NkLightEnvironment — ensemble de lumières d'une scène
    // =========================================================================
    struct NKRENDERER_API NkLightEnvironment {
        NkDirectionalLight  sun;
        bool                hasSun = false;

        // Lumières dynamiques (max 16 de chaque type en forward shading)
        static constexpr uint32 MAX_POINT_LIGHTS = 16;
        static constexpr uint32 MAX_SPOT_LIGHTS  = 8;
        static constexpr uint32 MAX_AREA_LIGHTS  = 4;

        NkPointLight points[MAX_POINT_LIGHTS];
        NkSpotLight  spots[MAX_SPOT_LIGHTS];
        NkAreaLight  areas[MAX_AREA_LIGHTS];
        uint32       numPoints = 0;
        uint32       numSpots  = 0;
        uint32       numAreas  = 0;

        NkColor4f    ambientColor     = {0.05f, 0.05f, 0.05f, 1.f};
        float32      ambientIntensity = 1.f;

        void Reset() noexcept {
            hasSun = false;
            numPoints = numSpots = numAreas = 0;
            ambientColor     = {0.05f, 0.05f, 0.05f, 1.f};
            ambientIntensity = 1.f;
        }

        bool AddPoint(const NkPointLight& l) noexcept {
            if (numPoints >= MAX_POINT_LIGHTS) return false;
            points[numPoints++] = l; return true;
        }
        bool AddSpot(const NkSpotLight& l) noexcept {
            if (numSpots >= MAX_SPOT_LIGHTS) return false;
            spots[numSpots++] = l; return true;
        }
        bool AddArea(const NkAreaLight& l) noexcept {
            if (numAreas >= MAX_AREA_LIGHTS) return false;
            areas[numAreas++] = l; return true;
        }
    };

    // =========================================================================
    // GPU-packed light structs (std140, pour le UBO de lumières)
    // =========================================================================
    // Toutes les lumières sont emballées dans un tableau unique envoyé
    // au fragment shader. On utilise un type uniform avec un champ `type`.
    struct alignas(16) NkGpuLight {
        NkVec4  posOrDir;   // xyz = position (point/spot) ou direction (directionnal)
        NkVec4  color;      // rgb = couleur, w = intensité
        NkVec4  params;     // x=range, y=innerCos, z=outerCos, w=type(0=dir,1=pt,2=spot)
    };

    struct NKRENDERER_API NkGpuLightBlock {
        static constexpr uint32 MAX_GPU_LIGHTS = 25; // 1 dir + 16 point + 8 spot
        NkGpuLight lights[MAX_GPU_LIGHTS];
        uint32     count     = 0;
        NkVec4     ambient;   // rgb + intensity
        uint32     _pad[3]  = {};

        void Build(const NkLightEnvironment& env) noexcept;
    };

} // namespace nkentseu
