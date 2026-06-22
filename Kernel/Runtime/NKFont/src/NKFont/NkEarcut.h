#pragma once

/*
    NkEarcut.h — Triangulation ear-clipping avec support complet des trous.

    Corrections v2 :
      - NkEarcutConnectHole : utilisait holeNext (=hole->next) au lieu de holeLast
        (=hole->prev) pour le pont retour → créait une boucle infinie dans le ring.
      - Indices globaux : NkEarcut retourne maintenant des indices dans un tableau
        plat [outer | hole1 | hole2 | ...], non plus des indices locaux par contour.
        Cela permet d'indexer directement dans un tableau de vertices combiné.

    Usage :
        NkVector<NkVector<NkVec2f>> polygon;
        polygon.PushBack(outerContour);   // CCW, index 0..outer.Size()-1
        polygon.PushBack(holeContour);    // CW, index outer.Size()..outer.Size()+hole.Size()-1
        auto indices = NkEarcut<float>(polygon);
        // indices[i] est un indice global dans la flat list outer+hole
*/

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMemory/NkAllocator.h"

#include <cmath>
#include <cstddef>
#include <limits>

namespace nkentseu {

    namespace detail {

        // =====================================================================
        // NŒUDS POUR LA LISTE DOUBLEMENT CHAÎNÉE
        // =====================================================================

        template <typename T>
        struct NkEarcutNode {
            T x, y;
            std::size_t i;   // Index GLOBAL dans le tableau flat (outer+holes)
            NkEarcutNode* prev;
            NkEarcutNode* next;

            NkEarcutNode(T x_, T y_, std::size_t i_)
                : x(x_), y(y_), i(i_), prev(nullptr), next(nullptr) {}
        };

        template <typename T>
        inline NkEarcutNode<T>* NkEarcutInsertNode(std::size_t i, T x, T y,
                                                    NkEarcutNode<T>* last) {
            NkEarcutNode<T>* p = nkentseu::memory::NkGetDefaultAllocator().New<NkEarcutNode<T>>(x, y, i);
            if (!last) {
                p->prev = p;
                p->next = p;
            } else {
                p->next       = last->next;
                p->prev       = last;
                last->next->prev = p;
                last->next    = p;
            }
            return p;
        }

        template <typename T>
        inline void NkEarcutRemoveNode(NkEarcutNode<T>* p) {
            p->next->prev = p->prev;
            p->prev->next = p->next;
        }

        template <typename T>
        inline void NkEarcutDeleteList(NkEarcutNode<T>* start) {
            if (!start) return;
            NkEarcutNode<T>* p = start;
            do {
                NkEarcutNode<T>* nxt = p->next;
                nkentseu::memory::NkGetDefaultAllocator().Delete(p);
                p = nxt;
            } while (p != start);
        }

        // =====================================================================
        // GÉOMÉTRIE DE BASE
        // =====================================================================

        // Aire signée du triangle (p,q,r) pour Y-up :
        //   < 0  →  CCW   (oreille valide pour un polygone CCW)
        //   > 0  →  CW    (oreille invalide)
        template <typename T>
        inline T NkEarcutArea(NkEarcutNode<T>* p, NkEarcutNode<T>* q, NkEarcutNode<T>* r) {
            return (q->y - p->y) * (r->x - q->x) - (q->x - p->x) * (r->y - q->y);
        }

        // Test si P est dans le triangle ABC
        template <typename T>
        inline bool NkEarcutPointInTriangle(T ax, T ay, T bx, T by,
                                             T cx, T cy, T px, T py) {
            T s1 = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
            T s2 = (cx - bx) * (py - by) - (cy - by) * (px - bx);
            T s3 = (ax - cx) * (py - cy) - (ay - cy) * (px - cx);
            return (s1 >= 0 && s2 >= 0 && s3 >= 0) || (s1 <= 0 && s2 <= 0 && s3 <= 0);
        }

        // Vérifie si le sommet `ear` est une oreille valide
        template <typename T>
        inline bool NkEarcutIsEar(NkEarcutNode<T>* ear) {
            NkEarcutNode<T>* a = ear->prev;
            NkEarcutNode<T>* b = ear;
            NkEarcutNode<T>* c = ear->next;

            // Doit être un sommet convexe (aire < 0 = CCW pour un polygone CCW)
            if (NkEarcutArea(a, b, c) >= 0) return false;

            // Aucun autre point ne doit être dans le triangle
            NkEarcutNode<T>* p = c->next;
            while (p != a) {
                if (NkEarcutPointInTriangle(a->x, a->y, b->x, b->y,
                                            c->x, c->y, p->x, p->y))
                    return false;
                p = p->next;
            }
            return true;
        }

        // =====================================================================
        // TRIANGULATION PRINCIPALE (ear-clipping sur liste chaînée)
        // =====================================================================

        template <typename T>
        inline void NkEarcutLinked(NkEarcutNode<T>* ear,
                                    NkVector<std::size_t>& triangles) {
            if (!ear) return;

            NkEarcutNode<T>* stop = ear;
            std::size_t      iterations = 0;
            const std::size_t maxIter   = 16000;

            while (ear->prev != ear->next && iterations++ < maxIter) {
                if (NkEarcutIsEar(ear)) {
                    triangles.PushBack(ear->prev->i);
                    triangles.PushBack(ear->i);
                    triangles.PushBack(ear->next->i);

                    NkEarcutNode<T>* nxt = ear->next;
                    NkEarcutRemoveNode(ear);
                    nkentseu::memory::NkGetDefaultAllocator().Delete(ear);
                    ear  = nxt;
                    stop = nxt;
                } else {
                    ear = ear->next;
                    if (ear == stop) break;
                }
            }

            // Triangle final restant
            if (ear->prev != ear && ear->next != ear &&
                ear->prev->next == ear) {
                triangles.PushBack(ear->prev->i);
                triangles.PushBack(ear->i);
                triangles.PushBack(ear->next->i);
            }
        }

        // =====================================================================
        // CRÉATION DE LISTE AVEC OFFSET GLOBAL
        // =====================================================================

        // Construit une liste chaînée circulaire depuis un contour.
        // Chaque nœud reçoit l'index GLOBAL offset+i (ou offset+(size-1-i) si inverse).
        // `reverse` = true pour les trous (inverse l'ordre de traversée).
        template <typename T>
        inline NkEarcutNode<T>* NkEarcutCreateListWithOffset(
            const NkVector<math::NkVec2T<T>>& points,
            bool reverse, std::size_t offset)
        {
            NkEarcutNode<T>* last = nullptr;
            const std::size_t n = points.Size();
            if (reverse) {
                // Parcours inverse : le point à l'index `i` dans points reçoit
                // l'index global `offset + i` mais est inséré en ordre inverse
                // pour inverser le sens de traversée (CW → CCW dans la liste).
                for (std::size_t k = n; k-- > 0;) {
                    last = NkEarcutInsertNode(offset + k, points[k].x, points[k].y, last);
                }
            } else {
                for (std::size_t k = 0; k < n; ++k) {
                    last = NkEarcutInsertNode(offset + k, points[k].x, points[k].y, last);
                }
            }
            return last;
        }

        // =====================================================================
        // GESTION DES TROUS : PONT (BRIDGE)
        // =====================================================================

        // Trouve la paire de sommets (un sur outer, un sur hole) la plus proche.
        template <typename T>
        inline void NkEarcutFindBridge(NkEarcutNode<T>*  outer,
                                        NkEarcutNode<T>*  hole,
                                        NkEarcutNode<T>*& outOuter,
                                        NkEarcutNode<T>*& outHole) {
            T minDist = std::numeric_limits<T>::max();
            outOuter  = outer;
            outHole   = hole;

            for (NkEarcutNode<T>* o = outer;; o = o->next) {
                for (NkEarcutNode<T>* h = hole;; h = h->next) {
                    T dx   = o->x - h->x;
                    T dy   = o->y - h->y;
                    T dist = dx * dx + dy * dy;
                    if (dist < minDist) {
                        minDist  = dist;
                        outOuter = o;
                        outHole  = h;
                    }
                    if (h->next == hole) break;
                }
                if (o->next == outer) break;
            }
        }

        // Connecte un trou au contour extérieur via un pont.
        //
        // Avant :
        //   outer: ... → A → B → C → ...  (B = outerBridge)
        //   hole:  ... → W → X → Y → ...  (X = holeBridge, W = X->prev)
        //
        // Après (single ring) :
        //   ... → A → B → X → Y → ... → W → C → ...
        //
        // CORRECTION : utilise holeLast = holeBridge->prev (= W) pour le pont
        // retour, et non holeBridge->next (= Y) comme c'était le cas avant.
        template <typename T>
        inline void NkEarcutConnectHole(NkEarcutNode<T>* outer,
                                         NkEarcutNode<T>* hole) {
            NkEarcutNode<T> *outerBridge, *holeBridge;
            NkEarcutFindBridge(outer, hole, outerBridge, holeBridge);

            NkEarcutNode<T>* outerNext = outerBridge->next; // C
            NkEarcutNode<T>* holeLast  = holeBridge->prev;  // W (dernier nœud du trou)

            // Pont aller : B → X
            outerBridge->next = holeBridge;
            holeBridge->prev  = outerBridge;

            // Pont retour : W → C  (referme le ring après le trou)
            holeLast->next  = outerNext;
            outerNext->prev = holeLast;
        }

        // Test point-dans-polygone (ray casting)
        template <typename T>
        inline bool NkEarcutPointInPolygon(
            const NkVector<math::NkVec2T<T>>& poly,
            const math::NkVec2T<T>&            point)
        {
            bool        inside = false;
            std::size_t n      = poly.Size();
            for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
                const math::NkVec2T<T>& pi = poly[i];
                const math::NkVec2T<T>& pj = poly[j];
                if (((pi.y > point.y) != (pj.y > point.y)) &&
                    (point.x < (pj.x - pi.x) * (point.y - pi.y) /
                                   (pj.y - pi.y + static_cast<T>(1e-8)) + pi.x))
                    inside = !inside;
            }
            return inside;
        }

    } // namespace detail

    // =========================================================================
    // API PUBLIQUE : NkEarcut
    // =========================================================================

    /**
     * @brief Triangule un polygone avec trous par ear-clipping.
     *
     * @tparam T Type des coordonnées (float, double…)
     *
     * @param polygon
     *   polygon[0]   = contour extérieur  → doit être CCW (aire > 0 en Y-up)
     *   polygon[1..] = trous              → doivent être CW  (aire < 0 en Y-up)
     *
     * @return Indices GLOBAUX dans le tableau plat [outer | hole1 | hole2 | …].
     *   - Indices 0 … outer.Size()-1                         → outer
     *   - Indices outer.Size() … outer.Size()+hole1.Size()-1 → hole1
     *   - etc.
     *
     * @note Normaliser le winding AVANT d'appeler :
     *   outer CCW → aire > 0  (si CW, inverser les points)
     *   holes CW  → aire < 0  (si CCW, inverser les points)
     */
    template <typename T = float>
    NkVector<std::size_t> NkEarcut(
        const NkVector<NkVector<math::NkVec2T<T>>>& polygon)
    {
        NkVector<std::size_t> triangles;
        if (polygon.IsEmpty() || polygon[0].Size() < 3) return triangles;

        // Outer avec indices globaux 0..outer.Size()-1
        detail::NkEarcutNode<T>* outerList =
            detail::NkEarcutCreateListWithOffset(polygon[0], false, 0);

        std::size_t globalOffset = polygon[0].Size();

        // Trous avec indices globaux offset..offset+hole.Size()-1
        for (std::size_t h = 1; h < polygon.Size(); ++h) {
            if (polygon[h].Size() < 3) continue;
            // Ignore les trous flottants (premier point hors du contour extérieur)
            if (!detail::NkEarcutPointInPolygon(polygon[0], polygon[h][0]))
                continue;

            // Trou CW → inversion = CCW dans la liste, puis bridge le connecte
            detail::NkEarcutNode<T>* holeList =
                detail::NkEarcutCreateListWithOffset(polygon[h], true, globalOffset);
            detail::NkEarcutConnectHole(outerList, holeList);
            globalOffset += polygon[h].Size();
        }

        detail::NkEarcutLinked(outerList, triangles);
        detail::NkEarcutDeleteList(outerList);

        return triangles;
    }

} // namespace nkentseu
