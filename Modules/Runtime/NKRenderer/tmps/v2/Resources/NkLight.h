#pragma once
// =============================================================================
// NkLight.h — Types de lumières pour NkRenderer3D.
//
//  NkLightType          Enum des types de lumières
//  NkDirectionalLight   Soleil / lune (direction globale, pas de position)
//  NkPointLight         Ampoule omnidirectionnelle (position + portée)
//  NkSpotLight          Projecteur (position + direction + cône)
//  NkAreaLight          Panneau lumineux rectangulaire (LTC)
//  NkSkyLight           Lumière ambiante IBL / HDRI
//  NkLightEnvironment   Agrégat complet — toutes les lumières d'une scène
//  NkGpuLight           Struct packée std140 envoyée au fragment shader
//  NkGpuLightBlock      Bloc uniforme complet (tableau + ambient)
// =============================================================================
#include "NKRenderer/Core/NkRenderTypes.h"

namespace nkentseu {
    namespace renderer {

        // =====================================================================
        // NkLightType
        // =====================================================================
        enum class NkLightType : uint8 {
            NK_DIRECTIONAL = 0,
            NK_POINT,
            NK_SPOT,
            NK_AREA,
            NK_SKY,         // IBL / HDRI ambient
        };

        // =====================================================================
        // NkDirectionalLight — lumière infinie (soleil, lune)
        // Direction pointe VERS la source (ex: {0.577, 0.577, 0.577} = haut-droite)
        // =====================================================================
        struct NkDirectionalLight {
            NkVec3f   direction  = { 0.577f, 0.577f, 0.577f }; // normalisé, vers la source
            NkColor4f color      = NkColor4f::White();
            float     intensity  = 1.f;
            bool      castShadow = true;
            uint32    shadowMapSize = 2048;
        };

        // =====================================================================
        // NkPointLight — lumière omnidirectionnelle (ampoule)
        // Atténuation physique : 1 / (distance² × falloff)
        // =====================================================================
        struct NkPointLight {
            NkVec3f   position   = {};
            NkColor4f color      = NkColor4f::White();
            float     intensity  = 1.f;
            float     range      = 10.f;   // portée maximale en unités monde
            float     falloff    = 2.f;    // exposant d'atténuation (2 = physique)
            bool      castShadow = false;
        };

        // =====================================================================
        // NkSpotLight — projecteur (lampe de scène, phare de voiture, etc.)
        // =====================================================================
        struct NkSpotLight {
            NkVec3f   position     = {};
            NkVec3f   direction    = { 0.f, -1.f, 0.f }; // normalisé, vers la cible
            NkColor4f color        = NkColor4f::White();
            float     intensity    = 1.f;
            float     range        = 10.f;
            float     innerConeDeg = 25.f;   // bord net (pénombre intérieure)
            float     outerConeDeg = 35.f;   // bord doux (cône complet)
            bool      castShadow   = false;
        };

        // =====================================================================
        // NkAreaLight — panneau lumineux rectangulaire (LTC)
        // =====================================================================
        struct NkAreaLight {
            NkVec3f   position  = {};
            NkVec3f   normal    = { 0.f, -1.f, 0.f }; // normalisé, face émettrice
            NkVec3f   right     = { 1.f,  0.f, 0.f }; // vecteur droit du panneau
            NkVec2f   size      = { 1.f,  1.f };       // largeur × hauteur (unités monde)
            NkColor4f color     = NkColor4f::White();
            float     intensity = 1.f;
        };

        // =====================================================================
        // NkSkyLight — lumière ambiante IBL (Image Based Lighting)
        // Peut être une couleur solide ou une texture cubemap HDRI.
        // =====================================================================
        struct NkSkyLight {
            NkColor4f color         = { 0.05f, 0.05f, 0.07f, 1.f }; // couleur de base
            float     intensity     = 1.f;
            // handle optionnel vers une NkTextureCube (HDRI) — nullptr = couleur solide
            // NkTextureCubeID iblCubemap = {};  // décommenté quand NkTextureCubeID est disponible
        };

        // =====================================================================
        // NkLightEnvironment
        // Ensemble complet de lumières d'une scène — passé à NkRenderer3D.
        // Limites forward shading : 1 directionnelle + 16 ponctuelles + 8 spots + 4 areas.
        // =====================================================================
        struct NKRENDERER_API NkLightEnvironment {
            // En forward rendering, les limites CPU correspondent aux slots GPU.
            // Augmenter ici nécessite d'augmenter MAX_GPU_LIGHTS en conséquence.
            static constexpr uint32 MAX_POINT_LIGHTS = 32;
            static constexpr uint32 MAX_SPOT_LIGHTS  = 16;
            static constexpr uint32 MAX_AREA_LIGHTS  =  8;

            // ── Lumière directionnelle principale ────────────────────────────
            NkDirectionalLight sun;
            bool               hasSun = false;

            // ── Lumière ambiante / IBL ────────────────────────────────────────
            NkSkyLight         sky;

            // ── Lumières dynamiques ───────────────────────────────────────────
            NkPointLight points[MAX_POINT_LIGHTS];
            NkSpotLight  spots [MAX_SPOT_LIGHTS];
            NkAreaLight  areas [MAX_AREA_LIGHTS];
            uint32       numPoints = 0;
            uint32       numSpots  = 0;
            uint32       numAreas  = 0;

            // ── Helpers ───────────────────────────────────────────────────────
            void Reset() noexcept {
                hasSun    = false;
                numPoints = numSpots = numAreas = 0;
                sky       = {};
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

        // =====================================================================
        // NkGpuLight — struct packée std140 pour le fragment shader
        //
        //  posOrDir.xyz  position (point/spot) ou direction (directionnel)
        //  color.rgb     couleur linéaire
        //  color.w       intensité
        //  params.x      range
        //  params.y      cos(innerCone) pour spot, 0 sinon
        //  params.z      cos(outerCone) pour spot, 0 sinon
        //  params.w      type : 0=directionnel, 1=point, 2=spot
        // =====================================================================
        struct alignas(16) NkGpuLight {
            NkVec4f posOrDir;
            NkVec4f color;
            NkVec4f params;
        };

        // =====================================================================
        // NkGpuLightBlock — UBO complet envoyé une fois par frame
        //
        // Limite forward standard : 1 dir + 16 point + 8 spot + 4 area = 29.
        // On garde 64 pour avoir de la marge sans dépasser les 64 KB d'UBO.
        // (64 × 48 bytes = 3072 bytes). Pour du clustered forward (>64),
        // migrer vers SSBO est recommandé.
        // =====================================================================
        struct NKRENDERER_API NkGpuLightBlock {
            static constexpr uint32 MAX_GPU_LIGHTS = 64;

            NkGpuLight lights[MAX_GPU_LIGHTS];
            uint32     count   = 0;
            NkVec4f    ambient = {};    // rgb = couleur ambiante, w = intensité
            uint32     _pad[3] = {};

            // Construit le bloc depuis un NkLightEnvironment
            void Build(const NkLightEnvironment& env) noexcept;
        };

    } // namespace renderer
} // namespace nkentseu
