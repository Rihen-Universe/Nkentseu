#pragma once
// =============================================================================
// NkSculptTypes.h  — NKRenderer v5.0  (Tools/PixolSculpt/)
//
// Types fondamentaux du sous-systeme "PixolSculpt" : sculpt en espace-ecran
// inspire de l'approche pixol / 2.5D de ZBrush. Principe : on ne pousse PAS
// plus de polygones. On maintient un G-buffer "pixol" (profondeur + normale +
// materiau + masque + couleur) et on le mute via des dispatchs compute bornes
// a la tuile sous la brosse. Le cout est borne par la resolution ecran, pas
// par le nombre de triangles.
//
// ⚠️ SQUELETTE — a implementer / tester plus tard. Aucun chemin ne tourne en
//    production aujourd'hui. Voir DESIGN.md (meme dossier) pour l'architecture
//    complete, le flux par frame et le diff d'integration dans NkRendererImpl.
// =============================================================================
#include "NKContainers/String/NkString.h"
#include "NKMath/NkMath.h"
#include "NKRHI/Core/NkTypes.h"

namespace nkentseu {
    namespace renderer {

        using namespace math;

        static constexpr uint32 kNkPixolSculptVersion = 1;

        // Taille de tuile compute par defaut (local_size_x == local_size_y).
        // Tout le travail d'un coup de brosse est decoupe en tuiles de ce cote.
        static constexpr uint32 kNkSculptTileSize = 16;

        // ─────────────────────────────────────────────────────────────────────
        // Mode de brosse. Chaque mode correspond a un kernel compute distinct
        // (voir shaders/sculpt_brush.comp.glsl + NkSculptPipelines).
        // ─────────────────────────────────────────────────────────────────────
        enum class NkSculptBrushMode : uint8 {
            NK_RAISE   = 0,  ///< Pousse la profondeur vers la camera (standard draw).
            NK_LOWER   = 1,  ///< Creuse (raise inverse).
            NK_SMOOTH  = 2,  ///< Moyenne locale du champ de profondeur (lissage).
            NK_PINCH   = 3,  ///< Attire les pixols vers le centre de la brosse.
            NK_INFLATE = 4,  ///< Deplace le long de la normale du pixol.
            NK_FLATTEN = 5,  ///< Rapproche d'un plan moyen local.
            NK_MASK    = 6,  ///< Peint le masque (protege / expose des zones).
            NK_PAINT   = 7,  ///< Peint le canal couleur (albedo du pixol).
            NK_COUNT
        };

        // Profil d'attenuation radiale, du centre vers le bord de la brosse.
        enum class NkSculptFalloff : uint8 {
            NK_SMOOTH   = 0,  ///< smoothstep (defaut, doux).
            NK_LINEAR   = 1,
            NK_CONSTANT = 2,  ///< Plein jusqu'au bord (tampon dur).
            NK_SHARP    = 3,
            NK_SPHERE   = 4,  ///< Profil hemispherique.
            NK_COUNT
        };

        // ─────────────────────────────────────────────────────────────────────
        // Formats des cibles du buffer pixol. Toutes creees en
        // NK_UNORDERED_ACCESS (storage images) pour etre ecrites par compute.
        // ─────────────────────────────────────────────────────────────────────
        struct NkPixolFormat {
            NkGPUFormat depth    = NkGPUFormat::NK_R32_FLOAT;     ///< Z lineaire par pixol.
            NkGPUFormat normal   = NkGPUFormat::NK_RGBA16_FLOAT;  ///< Normale reconstruite (xyz) + courbure (w).
            NkGPUFormat material = NkGPUFormat::NK_RGBA8_UNORM;   ///< id materiau / roughness / metallic.
            NkGPUFormat color    = NkGPUFormat::NK_RGBA8_UNORM;   ///< Albedo peint (mode NK_PAINT).
            NkGPUFormat mask     = NkGPUFormat::NK_R8_UNORM;      ///< Masque protecteur [0..1].
        };

        // Config a la creation du sous-systeme.
        struct NkPixolSculptConfig {
            uint32        width            = 0;     ///< 0 => suit le swapchain.
            uint32        height           = 0;
            uint32        tileSize         = kNkSculptTileSize;
            NkPixolFormat formats          = {};
            bool          enableColor      = true;  ///< Alloue la cible couleur (paint).
            bool          enableMask       = true;  ///< Alloue la cible masque.
            bool          resolveToGBuffer = true;  ///< Composite le pixol vers le G-buffer deferred.
            uint32        maxDabsPerFrame  = 256;   ///< Borne DURE du travail par frame (anti-explosion).
            float32       dabSpacing       = 0.25f; ///< Espacement des tampons le long du trace (x rayon).
        };

        // Statistiques d'une frame (debug / HUD).
        struct NkSculptStats {
            uint32  dabsDispatched  = 0;
            uint32  tilesDispatched = 0;  ///< Tuiles tileSize^2 reellement touchees ce frame.
            uint32  pixolsTouched   = 0;  ///< Estimation (toujours bornee par width*height).
            float32 cpuMs           = 0.f;
            void Reset() noexcept { *this = NkSculptStats{}; }
        };

        // Rectangle de tuiles en espace-ecran : le "dirty region" = working set
        // borne d'une frame. C'est ce qui garantit le cout constant en resolution.
        struct NkSculptRect {
            int32 x = 0, y = 0;
            int32 w = 0, h = 0;
            [[nodiscard]] bool IsEmpty() const noexcept { return w <= 0 || h <= 0; }
        };

    } // namespace renderer
} // namespace nkentseu
