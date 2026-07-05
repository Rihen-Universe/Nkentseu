// =============================================================================
// NkEditMesh.h — NKRenderer
// Maillage éditable en structure DEMI-ARÊTE (half-edge), support des faces à N
// sommets (n-gons), façon BMesh (Blender) / GMesh (Hoppe). Zero-STL.
//
// Rôle : source de vérité CPU pour l'édition de maillage (Edit Mode). Le GPU en
// est un cache : on TRIANGULE (fan) pour produire un mesh de rendu classique.
// Les faces restent des n-gons côté édition ; la triangulation est un détail
// d'affichage/export (choix, comme Blender).
// =============================================================================
#pragma once

#include "NkMeshSystem.h"   // NkVertex3D + NkVector + NkVec3f/NkVec2f (transitif)

namespace nkentseu {
    namespace renderer {

        // Indices (handles). NK_EM_INVALID = absent.
        using NkEmId = uint32;
        static const NkEmId NK_EM_INVALID = 0xFFFFFFFFu;

        class NkEditMesh {
            public:
                struct Vert {
                    NkVec3f  pos     = {0.f,0.f,0.f};
                    NkVec3f  normal  = {0.f,1.f,0.f};
                    NkVec2f  uv      = {0.f,0.f};
                    NkEmId   hedge   = NK_EM_INVALID;   // une demi-arête SORTANTE
                    uint8    sel     = 0;
                };
                struct Hedge {
                    NkEmId   origin  = NK_EM_INVALID;   // sommet d'origine
                    NkEmId   twin    = NK_EM_INVALID;   // demi-arête opposée (autre face)
                    NkEmId   next    = NK_EM_INVALID;   // suivante autour de la face
                    NkEmId   face    = NK_EM_INVALID;   // face incidente
                };
                struct Face {
                    NkEmId   hedge   = NK_EM_INVALID;   // une demi-arête du bord (boucle via next)
                    NkVec3f  normal  = {0.f,1.f,0.f};
                    uint8    sel     = 0;
                    uint8    alive   = 1;               // 0 = supprimée (compactée plus tard)
                };

                NkVector<Vert>  verts;
                NkVector<Hedge> hedges;
                NkVector<Face>  faces;

                void Clear() { verts.Clear(); hedges.Clear(); faces.Clear(); }
                uint32 VertCount() const { return (uint32)verts.Size(); }
                uint32 FaceCount() const { return (uint32)faces.Size(); }

                // Construit depuis un maillage indexé (triangles). Si quadify=true, fusionne
                // les paires de triangles coplanaires partageant une arête en QUADS (n-gons).
                void BuildFromIndexed(const NkVertex3D* v, uint32 vc,
                                      const uint32* idx, uint32 ic, bool quadify);

                // Triangule toutes les faces (éventail) -> mesh de rendu. outTriFace[i] = id
                // de la face n-gon d'origine du i-ème triangle (pour le pick).
                void Triangulate(NkVector<NkVertex3D>& outV, NkVector<uint32>& outIdx,
                                 NkVector<NkEmId>& outTriFace) const;

                // Arêtes uniques (paires de sommets) pour la cage d'affichage.
                void GetUniqueEdges(NkVector<uint32>& outPairs) const;

                // Sommets (dans l'ordre du bord) d'une face n-gon.
                void GetFaceVerts(NkEmId f, NkVector<NkEmId>& out) const;

                // Normales par face (produit vectoriel) puis par sommet (moyenne).
                void RecomputeNormals();

            private:
                // Lie les jumeaux (twin) via une table de hachage sur (min,max) des sommets.
                void LinkTwins();
        };

    } // namespace renderer
} // namespace nkentseu
