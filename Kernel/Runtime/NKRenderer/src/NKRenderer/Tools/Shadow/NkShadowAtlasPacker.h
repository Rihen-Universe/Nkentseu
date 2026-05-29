#pragma once
// =============================================================================
// NkShadowAtlasPacker.h — NKRenderer Tools/Shadow
//
// Algo bin-packing skyline (bottom-left) pour allouer dynamiquement des tiles
// rectangulaires dans un atlas 2D. Utilise par NkVirtualShadowMaps pour packer
// les shadow maps de toutes les lumieres (CSM cascades + spot + point cubemap)
// dans une seule texture atlas D32_FLOAT.
//
// Algo skyline :
//   - L'occupation de l'atlas est decrite par une liste de "segments" (x,y,w)
//     formant le profil superieur. Initial : 1 segment a (0, 0, atlasW).
//   - Pour allouer (w, h) : on cherche le segment qui minimise la hauteur
//     d'insertion (best-fit en y). Apres allocation, on modifie la liste pour
//     refleter le nouveau profil.
//   - Complexite : O(N) par allocate, ou N = nb segments (~ nb tiles dans
//     l'atlas). Pour 256 slots c'est nego.
//
// Limitations V0 : pas de defragmentation (Free puis Allocate peut laisser des
// trous). Pour un usage frame-by-frame, on Reset() au debut de chaque frame.
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"

namespace nkentseu {
    namespace renderer {

        // Resultat d'une allocation : rectangle integer dans l'atlas (pixels).
        struct NkShadowTileRect {
            uint32  x      = 0;
            uint32  y      = 0;
            uint32  w      = 0;
            uint32  h      = 0;
            bool    valid  = false;
        };

        class NkShadowAtlasPacker {
        public:
            // Init/Reset : redefinit l'atlas a (atlasW, atlasH) et vide la liste.
            // Appelable a chaque frame pour repartir d'un atlas vierge.
            void   Reset(uint32 atlasW, uint32 atlasH);

            // Allocate : tente d'inserer un rectangle (tileW, tileH).
            // Retourne true + remplit outRect si succes ; false si plus de place.
            bool   Allocate(uint32 tileW, uint32 tileH, NkShadowTileRect& outRect);

            // Converti un NkShadowTileRect (pixels) en uv vec4 (minU, minV, maxU, maxV)
            // dans [0, 1] selon la taille courante de l'atlas.
            NkVec4f ToUV(const NkShadowTileRect& r) const;

            // Diagnostics.
            uint32 GetAtlasWidth()  const { return mAtlasW; }
            uint32 GetAtlasHeight() const { return mAtlasH; }
            uint32 GetAllocCount()  const { return mAllocCount; }

        private:
            // Segment du profil : (x, y, w) -> de x a x+w en haut a y.
            struct Segment {
                uint32 x;
                uint32 y;
                uint32 w;
            };

            // Trouve y minimum pour inserer tileW pixels depuis le segment startIdx.
            // Retourne UINT32_MAX si l'insertion ne tient pas dans la largeur dispo.
            uint32 ComputeFitY(uint32 startIdx, uint32 tileW) const;

            // Insere le nouveau segment a (x, y+h, w) en fusionnant les segments
            // touches a gauche/droite. Met a jour mSegments.
            void   InsertSegment(uint32 startIdx, uint32 tileW, uint32 tileH, uint32 fitY);

            uint32           mAtlasW     = 0;
            uint32           mAtlasH     = 0;
            uint32           mAllocCount = 0;
            NkVector<Segment> mSegments;  // profil skyline ordonne par x
        };

    } // namespace renderer
} // namespace nkentseu
