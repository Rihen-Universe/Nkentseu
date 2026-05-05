#pragma once

/*
    * NkEarcut.h - Triangulation par ear-clipping avec support des trous
    * Adapté pour le framework nkentseu
    * 
    * Usage :
    *   NkVector<NkVector<math::NkVec2f>> polygon;  // [0] = extérieur (CCW), [1..n] = trous (CW)
    *   auto indices = nkentseu::NkEarcut(polygon);
    */

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"

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
            std::size_t i;              // Index du point dans le tableau original
            NkEarcutNode* prev;
            NkEarcutNode* next;
            std::size_t z;              // Valeur Z-order (optionnel, pour optimisation spatiale)

            NkEarcutNode(T x_, T y_, std::size_t i_)
                : x(x_), y(y_), i(i_), prev(nullptr), next(nullptr), z(0) {}
        };

        template <typename T>
        inline NkEarcutNode<T>* NkEarcutInsertNode(std::size_t i, T x, T y, NkEarcutNode<T>* last) {
            NkEarcutNode<T>* p = new NkEarcutNode<T>(x, y, i);
            if (!last) {
                p->prev = p;
                p->next = p;
            } else {
                p->next = last->next;
                p->prev = last;
                last->next->prev = p;
                last->next = p;
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
                NkEarcutNode<T>* next = p->next;
                delete p;
                p = next;
            } while (p != start);
        }

        // =====================================================================
        // GÉOMÉTRIE DE BASE
        // =====================================================================

        // Aire signée du triangle (p,q,r) : >0 = CCW, <0 = CW
        template <typename T>
        inline T NkEarcutArea(NkEarcutNode<T>* p, NkEarcutNode<T>* q, NkEarcutNode<T>* r) {
            return (q->y - p->y) * (r->x - q->x) - (q->x - p->x) * (r->y - q->y);
        }

        // Test si le point P est dans le triangle ABC (méthode des signes)
        template <typename T>
        inline bool NkEarcutPointInTriangle(T ax, T ay, T bx, T by, T cx, T cy, T px, T py) {
            T s1 = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
            T s2 = (cx - bx) * (py - by) - (cy - by) * (px - bx);
            T s3 = (ax - cx) * (py - cy) - (ay - cy) * (px - cx);
            return (s1 >= 0 && s2 >= 0 && s3 >= 0) || (s1 <= 0 && s2 <= 0 && s3 <= 0);
        }

        // Test si un sommet forme une "oreille" (triangle sans point à l'intérieur)
        template <typename T>
        inline bool NkEarcutIsEar(NkEarcutNode<T>* ear) {
            NkEarcutNode<T>* a = ear->prev;
            NkEarcutNode<T>* b = ear;
            NkEarcutNode<T>* c = ear->next;

            // Doit être un sommet convexe (aire < 0 pour winding CCW)
            if (NkEarcutArea(a, b, c) >= 0) return false;

            // Vérifier qu'aucun autre point n'est dans le triangle
            NkEarcutNode<T>* p = c->next;
            while (p != a) {
                if (NkEarcutPointInTriangle(a->x, a->y, b->x, b->y, c->x, c->y, p->x, p->y))
                    return false;
                p = p->next;
            }
            return true;
        }

        // =====================================================================
        // TRIANGULATION PRINCIPALE (ear-clipping sur liste chaînée)
        // =====================================================================

        template <typename T>
        inline void NkEarcutLinked(NkEarcutNode<T>* ear, NkVector<std::size_t>& triangles) {
            if (!ear) return;

            NkEarcutNode<T>* stop = ear;
            std::size_t iterations = 0;
            const std::size_t maxIter = 10000;  // Sécurité contre boucles infinies

            while (ear->prev != ear->next && iterations++ < maxIter) {
                if (NkEarcutIsEar(ear)) {
                    // Émettre le triangle
                    triangles.PushBack(ear->prev->i);
                    triangles.PushBack(ear->i);
                    triangles.PushBack(ear->next->i);

                    // Retirer le sommet et continuer avec le suivant
                    NkEarcutNode<T>* next = ear->next;
                    NkEarcutRemoveNode(ear);
                    delete ear;
                    ear = next;
                    stop = next;
                } else {
                    ear = ear->next;
                    if (ear == stop) {
                        // Plus d'oreilles trouvées : polygone non-simple ou dégénéré
                        break;
                    }
                }
            }

            // Triangle final restant (si polygone convexe ou presque)
            if (ear->prev != ear && ear->next != ear && ear->prev->next == ear) {
                triangles.PushBack(ear->prev->i);
                triangles.PushBack(ear->i);
                triangles.PushBack(ear->next->i);
            }
        }

        // Créer une liste chaînée circulaire depuis un contour de points
        template <typename T>
        inline NkEarcutNode<T>* NkEarcutCreateList(const NkVector<math::NkVec2T<T>>& points, bool reverse) {
            NkEarcutNode<T>* last = nullptr;
            if (reverse) {
                // Parcours inverse pour inverser le winding
                for (std::size_t i = points.Size(); i-- > 0;) {
                    last = NkEarcutInsertNode(i, points[i].x, points[i].y, last);
                }
            } else {
                for (std::size_t i = 0; i < points.Size(); ++i) {
                    last = NkEarcutInsertNode(i, points[i].x, points[i].y, last);
                }
            }
            return last;
        }

        // =====================================================================
        // GESTION DES TROUS : RECHERCHE DE PONT (BRIDGE)
        // =====================================================================

        // Trouver les deux sommets (un sur outer, un sur hole) les plus proches
        template <typename T>
        inline void NkEarcutFindBridge(NkEarcutNode<T>* outer, NkEarcutNode<T>* hole,
                                       NkEarcutNode<T>*& outOuter, NkEarcutNode<T>*& outHole) {
            T minDist = std::numeric_limits<T>::max();
            outOuter = outer;
            outHole = hole;

            for (NkEarcutNode<T>* o = outer;; o = o->next) {
                for (NkEarcutNode<T>* h = hole;; h = h->next) {
                    T dx = o->x - h->x;
                    T dy = o->y - h->y;
                    T dist = dx * dx + dy * dy;
                    if (dist < minDist) {
                        minDist = dist;
                        outOuter = o;
                        outHole = h;
                    }
                    if (h->next == hole) break;
                }
                if (o->next == outer) break;
            }
        }

        // Connecter un trou au contour extérieur via un "pont" (deux arêtes coïncidentes)
        template <typename T>
        inline void NkEarcutConnectHole(NkEarcutNode<T>* outer, NkEarcutNode<T>* hole) {
            NkEarcutNode<T> *outerBridge, *holeBridge;
            NkEarcutFindBridge(outer, hole, outerBridge, holeBridge);

            // Le pont est créé en "coupant" les listes et en les reconnectant :
            // outer: ... -> A -> B -> ...    hole: ... -> X -> Y -> ...
            // Devient : ... -> A -> X -> Y -> ... -> B -> ... (boucle unique)
            
            NkEarcutNode<T>* outerNext = outerBridge->next;
            NkEarcutNode<T>* holeNext = holeBridge->next;

            // Pont aller : outerBridge -> holeBridge
            outerBridge->next = holeBridge;
            holeBridge->prev = outerBridge;

            // Pont retour : holeNext -> outerNext (ferme la boucle)
            holeNext->prev = outerNext;
            outerNext->next = holeNext;
        }

        // Test si un point est à l'intérieur d'un polygone (ray casting)
        template <typename T>
        inline bool NkEarcutPointInPolygon(const NkVector<math::NkVec2T<T>>& poly, const math::NkVec2T<T>& point) {
            bool inside = false;
            std::size_t n = poly.Size();
            for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
                const math::NkVec2T<T>& pi = poly[i];
                const math::NkVec2T<T>& pj = poly[j];
                if (((pi.y > point.y) != (pj.y > point.y)) &&
                    (point.x < (pj.x - pi.x) * (point.y - pi.y) / (pj.y - pi.y + static_cast<T>(1e-8)) + pi.x)) {
                    inside = !inside;
                }
            }
            return inside;
        }

        // Calculer l'aire signée d'un contour
        template <typename T>
        inline T NkEarcutContourArea(const NkVector<math::NkVec2T<T>>& contour) {
            T area = 0;
            std::size_t n = contour.Size();
            for (std::size_t i = 0; i < n; ++i) {
                const math::NkVec2T<T>& p0 = contour[i];
                const math::NkVec2T<T>& p1 = contour[(i + 1) % n];
                area += p0.x * p1.y - p0.y * p1.x;
            }
            return area * static_cast<T>(0.5);
        }

    } // namespace detail

    // =====================================================================
    // API PUBLIQUE : NkEarcut
    // =====================================================================

    /**
     * @brief Triangule un polygone avec trous par ear-clipping.
     * 
     * @tparam T Type des coordonnées (float, double, int...)
     * @param polygon Vecteur de contours : 
     *        - polygon[0] = contour extérieur (doit être CCW, aire > 0)
     *        - polygon[1..n] = trous (doivent être CW, aire < 0)
     * @return NkVector<std::size_t> Indices des triangles (groupés par 3)
     * 
     * @note Les indices font référence aux positions dans les contours d'origine.
     *       Pour un usage avec des vertex, il faut mapper les indices globalement.
     */
    template <typename T = float>
    NkVector<std::size_t> NkEarcut(const NkVector<NkVector<math::NkVec2T<T>>>& polygon) {
        NkVector<std::size_t> triangles;

        if (polygon.IsEmpty() || polygon[0].Size() < 3) return triangles;

        // 1. Créer la liste pour le contour extérieur (CCW = ordre normal)
        detail::NkEarcutNode<T>* outerList = detail::NkEarcutCreateList(polygon[0], false);

        // 2. Connecter chaque trou au contour extérieur via des ponts
        for (std::size_t h = 1; h < polygon.Size(); ++h) {
            if (polygon[h].Size() < 3) continue;
            
            // Optionnel : vérifier que le trou est bien à l'intérieur de l'extérieur
            if (!detail::NkEarcutPointInPolygon(polygon[0], polygon[h][0])) {
                continue;  // Ignorer les trous "flottants"
            }

            // Les trous doivent être CW → on inverse l'ordre pour la liste chaînée
            detail::NkEarcutNode<T>* holeList = detail::NkEarcutCreateList(polygon[h], true);
            detail::NkEarcutConnectHole(outerList, holeList);
        }

        // 3. Trianguler la liste unique résultante
        detail::NkEarcutLinked(outerList, triangles);

        // 4. Nettoyage mémoire
        detail::NkEarcutDeleteList(outerList);

        return triangles;
    }

} // namespace nkentseu