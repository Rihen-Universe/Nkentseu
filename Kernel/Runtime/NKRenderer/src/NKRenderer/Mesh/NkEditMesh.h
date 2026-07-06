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

        // ── Paramètres des commandes d'édition (niveau namespace : réutilisables par les
        //    modificateurs / l'IA, et défauts utilisables comme arguments par défaut). ──
        struct NkExtrudeParams  { bool  individual = false;  float32 offset = -1.f; };  // offset<0 => auto (8 % bbox)
        struct NkMergeParams    { enum Mode { Center = 0, First = 1, Last = 2 }; int32 mode = Center; };
        struct NkSubdivideParams{ int32 cuts = 1; };   // faces sélectionnées, ou TOUT si rien n'est sélectionné

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
                    uint8    alive   = 1;               // 0 = arête interne dissoute (quadify)
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

                // ── Représentation POLYGONES (n-gons) — CSR ─────────────────────────
                // Extrait les faces vivantes : sommets + boucles (face i = outFaceVerts
                // [outFaceStart[i] .. outFaceStart[i+1]]). outFaceStart a faceCount+1 entrées.
                void ToPolygons(NkVector<NkVertex3D>& outVerts,
                                NkVector<uint32>& outFaceStart, NkVector<uint32>& outFaceVerts) const;
                // (Re)construit le half-edge depuis des n-gons (même format CSR).
                void BuildFromPolygons(const NkVertex3D* v, uint32 vc,
                                       const uint32* faceStart, uint32 faceCount, const uint32* faceVerts);

                // Arêtes uniques (paires de sommets) pour la cage d'affichage.
                void GetUniqueEdges(NkVector<uint32>& outPairs) const;

                // Sommets (dans l'ordre du bord) d'une face n-gon.
                void GetFaceVerts(NkEmId f, NkVector<NkEmId>& out) const;
                uint32 FaceSize(NkEmId f) const;   // nombre de sommets du bord

                // Fusionne les paires de triangles CONSÉCUTIFS (2k,2k+1) adjacents et
                // coplanaires en QUADS. Adapté aux meshes triangulés quad-par-quad
                // (primitives, grilles). coplanarDot ~0.9995 (cube) à 0.98 (sphère fine).
                void Quadify(float32 coplanarDot = 0.985f);

                // Normales par face (produit vectoriel) puis par sommet (moyenne).
                void RecomputeNormals();

                // ── COUCHE DE COMMANDES D'ÉDITION (paramétrée, découplée de l'UI) ────
                // Ces opérations agissent sur la SÉLECTION interne (Vert::sel, ou par
                // face = tous ses sommets sélectionnés) et mutent la topologie n-gon.
                // Elles NE touchent PAS au GPU : l'appelant régénère le rendu (Triangulate)
                // après coup. C'est la base pour l'undo/redo, les modificateurs (stack
                // non-destructif) et l'espace d'actions IA (NKAI). Chaque op renvoie true
                // si la topologie/géométrie a changé. Paramètres : Nk*Params (namespace).

                // Sélection interne (Vert::sel).
                void SelectAll();
                void SelectNone();
                bool AnyVertSelected() const;

                bool ExtrudeSelectedFaces  (const NkExtrudeParams&   p = NkExtrudeParams{});
                bool DeleteSelectedFaces   ();
                bool MergeSelectedVerts    (const NkMergeParams&     p = NkMergeParams{});
                bool MakeFaceFromSelected  ();
                bool SubdivideSelectedFaces(const NkSubdivideParams& p = NkSubdivideParams{});
                bool LoopCutFromSelectedEdge();
                // planePoint / planeNormal sont exprimés dans l'espace de `localToPlaneSpace`
                // (= matrice modèle→monde côté éditeur ; identité pour une op locale pure IA).
                bool BisectByPlane(const NkVec3f& planePoint, const NkVec3f& planeNormal,
                                   const NkMat4f& localToPlaneSpace);

            private:
                // Lie les jumeaux (twin) via une table de hachage sur (min,max) des sommets.
                void LinkTwins();
                // Une face polygone (indices [s..e[ dans fv) est sélectionnée si TOUS ses
                // sommets le sont (Vert::sel).
                bool PolyFaceSelected(const NkVector<uint32>& fv, uint32 s, uint32 e) const;
                // Recopie une sélection par-sommet (indexée sur le nouveau maillage) dans Vert::sel.
                void ApplyVertSel(const NkVector<uint8>& vsel);
                // Sous-étape de subdivision (une passe Catmull-Clark).
                bool SubdivideSelectedOnce();
        };

        // ── HISTORIQUE UNDO/REDO (mémento) ──────────────────────────────────────
        // Stocke des SNAPSHOTS complets de NkEditMesh (les maillages d'édition sont
        // petits -> simple et robuste pour toute topologie, approche edit-mode Blender).
        // Modèle : Commit(pré-état) AVANT une commande mutante ; Undo/Redo échangent
        // l'état courant avec la pile. Réutilisable (éditeur, rejeu IA). Cap par défaut 64.
        class NkEditHistory {
            public:
                void   Clear();
                void   SetLimit(uint32 n) { mLimit = (n<1u)?1u:n; }
                // À appeler AVANT une commande mutante réussie, avec l'état d'AVANT la
                // mutation : empile le point de retour et invalide la pile de redo.
                void   Commit(const NkEditMesh& preState);
                bool   CanUndo() const { return !mUndo.Empty(); }
                bool   CanRedo() const { return !mRedo.Empty(); }
                // Échange `mesh` avec l'état précédent/suivant. false si rien à faire.
                bool   Undo(NkEditMesh& mesh);
                bool   Redo(NkEditMesh& mesh);
                uint32 UndoCount() const { return (uint32)mUndo.Size(); }
                uint32 RedoCount() const { return (uint32)mRedo.Size(); }
            private:
                NkVector<NkEditMesh> mUndo;   // états passés (sommet = le plus récent)
                NkVector<NkEditMesh> mRedo;   // états annulés (rejouables)
                uint32 mLimit = 64u;
        };

        // ── COMMANDE D'ÉDITION SÉRIALISABLE (la couche de commandes rendue DONNÉE) ──
        // Représente UNE opération d'édition comme une DONNÉE (type + paramètres +
        // sélection au moment de l'application). Deux usages clés :
        //   • MODIFICATEURS non-destructifs : une pile de commandes rejouée depuis un
        //     maillage de base (mirror/array/subsurf = des commandes paramétrées).
        //   • IA (NKAI) : espace d'actions + données d'IMITATION (on enregistre les
        //     sessions de modélisation, on rejoue / on apprend une policy).
        enum class NkMeshEditOp : uint8 {
            None = 0, Extrude, Delete, Merge, MakeFace, Subdivide, LoopCut, Bisect, Move
        };
        struct NkMeshEditCommand {
            NkMeshEditOp     op = NkMeshEditOp::None;
            NkVector<uint32> selection;                  // sommets sélectionnés à l'application
            NkExtrudeParams   extrude;                   // (op == Extrude)
            NkMergeParams     merge;                     // (op == Merge)
            NkSubdivideParams subdiv;                    // (op == Subdivide)
            NkVec3f           planePoint  = {0.f,0.f,0.f};   // (op == Bisect)
            NkVec3f           planeNormal = {0.f,1.f,0.f};
            NkMat4f           bisectXform = NkMat4f::Identity();
            NkVector<NkVec3f> moveDeltas;                // (op == Move) delta par sommet (aligné sur selection)

            // Pose la sélection sur `m` puis exécute l'op. true si la géométrie a changé.
            bool Apply(NkEditMesh& m) const;
        };

        // Journal de commandes : enregistre une session, la rejoue sur un maillage.
        class NkMeshEditRecorder {
            public:
                void   Clear() { mCommands.Clear(); }
                void   Push(const NkMeshEditCommand& c) { mCommands.PushBack(c); }
                uint32 Count() const { return (uint32)mCommands.Size(); }
                const NkMeshEditCommand& At(uint32 i) const { return mCommands[i]; }
                // Rejoue toutes les commandes (dans l'ordre) sur `mesh`. Renvoie le nb appliquées.
                uint32 ReplayOnto(NkEditMesh& mesh) const;
                // Sérialisation binaire autonome (magic "NMEC", versionnée) — persiste une
                // session sur disque : données d'imitation IA + modificateurs sauvegardables.
                void   Serialize(NkVector<uint8>& out) const;
                bool   Deserialize(const uint8* data, uint32 size);
            private:
                NkVector<NkMeshEditCommand> mCommands;
        };

    } // namespace renderer
} // namespace nkentseu
