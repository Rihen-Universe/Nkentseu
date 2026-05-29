// =============================================================================
// NkShadowAtlasPacker.cpp — implementation skyline rectpack
// =============================================================================
#include "NKRenderer/Tools/Shadow/NkShadowAtlasPacker.h"

#include <climits>

namespace nkentseu {
    namespace renderer {

        // ---------------------------------------------------------------------
        // Reset : repart d'un atlas vierge.
        // ---------------------------------------------------------------------
        void NkShadowAtlasPacker::Reset(uint32 atlasW, uint32 atlasH) {
            mAtlasW     = atlasW;
            mAtlasH     = atlasH;
            mAllocCount = 0;
            mSegments.Clear();
            if (atlasW > 0 && atlasH > 0) {
                // Profil initial : 1 segment plat a y=0 sur toute la largeur.
                Segment s; s.x = 0; s.y = 0; s.w = atlasW;
                mSegments.PushBack(s);
            }
        }

        // ---------------------------------------------------------------------
        // ComputeFitY : calcule le y minimum auquel on peut poser un rect tileW
        // de large en commencant a startIdx. L'insertion peut chevaucher
        // plusieurs segments consecutifs : on prend le max de leurs y, qui est
        // la hauteur a laquelle le rect doit reposer pour ne pas etre en sous-sol.
        // Retourne UINT32_MAX si tileW depasse la largeur restante depuis startIdx.
        // ---------------------------------------------------------------------
        uint32 NkShadowAtlasPacker::ComputeFitY(uint32 startIdx, uint32 tileW) const {
            uint32 width    = 0;
            uint32 maxY     = 0;
            const uint32 N  = mSegments.Size();
            uint32 i        = startIdx;
            while (i < N && width < tileW) {
                if (mSegments[i].y > maxY) maxY = mSegments[i].y;
                width += mSegments[i].w;
                i++;
            }
            if (width < tileW) return UINT_MAX;  // depasse la fin de l'atlas
            return maxY;
        }

        // ---------------------------------------------------------------------
        // Allocate : trouve le meilleur emplacement (best-fit en y), insere le
        // segment, retourne le rect alloue (x, y, w, h).
        // ---------------------------------------------------------------------
        bool NkShadowAtlasPacker::Allocate(uint32 tileW, uint32 tileH,
                                            NkShadowTileRect& outRect) {
            outRect.valid = false;
            if (tileW == 0 || tileH == 0)              return false;
            if (tileW > mAtlasW || tileH > mAtlasH)    return false;
            if (mSegments.IsEmpty())                   return false;

            // Best-fit : on cherche le segment qui donne le y le plus bas.
            uint32 bestIdx  = UINT_MAX;
            uint32 bestY    = UINT_MAX;
            uint32 bestX    = 0;
            const uint32 N  = mSegments.Size();
            for (uint32 i = 0; i < N; i++) {
                // Verifie que tileW tient depuis ce segment (largeur cumulee).
                uint32 fitY = ComputeFitY(i, tileW);
                if (fitY == UINT_MAX)                  continue;
                if (fitY + tileH > mAtlasH)            continue;  // depasse haut
                if (fitY < bestY) {
                    bestY   = fitY;
                    bestIdx = i;
                    bestX   = mSegments[i].x;
                }
            }
            if (bestIdx == UINT_MAX) return false;

            // Insere le segment dans la liste.
            InsertSegment(bestIdx, tileW, tileH, bestY);

            outRect.x     = bestX;
            outRect.y     = bestY;
            outRect.w     = tileW;
            outRect.h     = tileH;
            outRect.valid = true;
            mAllocCount++;
            return true;
        }

        // ---------------------------------------------------------------------
        // InsertSegment : modifie la liste de segments pour refleter
        // l'allocation du rect (bestX, fitY, tileW, tileH).
        //
        // Action :
        //   1. Cree un nouveau segment (bestX, fitY + tileH, tileW) qui est le
        //      nouveau profil au-dessus du rect.
        //   2. Supprime/raccourcit les segments existants qui etaient sous le
        //      rect (de bestX a bestX+tileW).
        //   3. Fusionne avec voisins de meme y.
        // ---------------------------------------------------------------------
        void NkShadowAtlasPacker::InsertSegment(uint32 startIdx, uint32 tileW,
                                                  uint32 tileH, uint32 fitY) {
            const uint32 newY  = fitY + tileH;
            const uint32 newX  = mSegments[startIdx].x;
            // Nouveau segment a inserer.
            Segment ns; ns.x = newX; ns.y = newY; ns.w = tileW;

            // Consomme les segments couverts par tileW.
            uint32 consumed = 0;
            uint32 i        = startIdx;
            while (i < mSegments.Size() && consumed < tileW) {
                const uint32 segW = mSegments[i].w;
                if (consumed + segW <= tileW) {
                    // Segment entierement consomme : on l'enleve.
                    consumed += segW;
                    mSegments.RemoveAt(i);  // n'incremente pas i (la liste glisse)
                } else {
                    // Segment partiellement consomme : on raccourcit son x et w.
                    const uint32 leftOver = (consumed + segW) - tileW;
                    mSegments[i].x        = newX + tileW;
                    mSegments[i].w        = leftOver;
                    consumed              = tileW;
                    break;
                }
            }

            // Insere le nouveau segment a la position startIdx.
            mSegments.Insert(mSegments.Begin() + startIdx, ns);

            // Fusionne avec voisins de meme y (gauche + droite). Cette fusion
            // garde la liste compacte (sinon on accumule des micro-segments).
            uint32 curIdx = startIdx;
            if (curIdx > 0
                && mSegments[curIdx - 1].y == mSegments[curIdx].y) {
                mSegments[curIdx - 1].w += mSegments[curIdx].w;
                mSegments.RemoveAt(curIdx);
                curIdx -= 1;
            }
            if (curIdx + 1 < mSegments.Size()
                && mSegments[curIdx + 1].y == mSegments[curIdx].y) {
                mSegments[curIdx].w += mSegments[curIdx + 1].w;
                mSegments.RemoveAt(curIdx + 1);
            }
        }

        // ---------------------------------------------------------------------
        // ToUV : conversion pixels -> UV [0, 1].
        // ---------------------------------------------------------------------
        NkVec4f NkShadowAtlasPacker::ToUV(const NkShadowTileRect& r) const {
            if (!r.valid || mAtlasW == 0 || mAtlasH == 0) {
                return NkVec4f{0.f, 0.f, 0.f, 0.f};
            }
            const float32 invW = 1.f / float32(mAtlasW);
            const float32 invH = 1.f / float32(mAtlasH);
            NkVec4f uv;
            uv.x = float32(r.x)         * invW;
            uv.y = float32(r.y)         * invH;
            uv.z = float32(r.x + r.w)   * invW;
            uv.w = float32(r.y + r.h)   * invH;
            return uv;
        }

    } // namespace renderer
} // namespace nkentseu
