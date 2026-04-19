#pragma once
// =============================================================================
// Nkentseu/Modeling/NkHalfEdge.h
// =============================================================================
// Structure Half-Edge pour la modélisation polygonale avancée.
//
// POURQUOI HALF-EDGE ?
//   NkEditableMesh stocke les données (positions, UVs, couleurs) mais
//   ne supporte pas efficacement les opérations topologiques (quels
//   faces entourent ce vertex ? quelles edges bordent cette face ?).
//   La structure Half-Edge résout exactement ce problème en O(1).
//
// STRUCTURE :
//   Chaque edge bidirectionnel est divisé en 2 half-edges.
//   Un half-edge pointe de v0 vers v1 et appartient à une face.
//   Son twin pointe de v1 vers v0 et appartient à la face adjacente.
//
//   HalfEdge {
//       vertex  — vertex de départ
//       face    — face contenant ce half-edge
//       next    — prochain half-edge dans la boucle de la face (CCW)
//       prev    — précédent half-edge dans la boucle
//       twin    — half-edge opposé (face adjacente)
//   }
//
// OPÉRATIONS SUPPORTÉES :
//   • Edge flip, edge split, edge collapse (Euler operators)
//   • Edge loop traversal (loop cut, edge ring select)
//   • Vertex valence (nombre de faces adjacentes)
//   • Boundary detection (edge sans twin = bord du mesh)
//   • Hole filling, bridge entre deux bords
//   • Conversion vers/depuis NkEditableMesh
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NkEditableMesh.h"

namespace nkentseu {
    using namespace math;

    static constexpr uint32 kNkHEInvalid = 0xFFFFFFFFu;

    // =========================================================================
    // Types de base Half-Edge
    // =========================================================================
    struct NkHEVertex {
        NkVec3f  pos       = {};
        NkVec3f  normal    = {0,1,0};
        NkVec2f  uv        = {};
        NkVec4f  color     = {1,1,1,1};
        uint32   halfEdge  = kNkHEInvalid;  ///< Un half-edge sortant de ce vertex
        bool     selected  = false;
        bool     boundary  = false;         ///< Vertex sur le bord
    };

    struct NkHEHalfEdge {
        uint32  vertex    = kNkHEInvalid;   ///< Vertex de départ
        uint32  face      = kNkHEInvalid;   ///< Face gauche (kNkHEInvalid = boundary)
        uint32  next      = kNkHEInvalid;   ///< Prochain HE dans la face (CCW)
        uint32  prev      = kNkHEInvalid;   ///< Précédent HE dans la face
        uint32  twin      = kNkHEInvalid;   ///< HE opposé (kNkHEInvalid = bord)
        bool    selected  = false;
        bool    seam      = false;          ///< Couture UV
        bool    sharp     = false;          ///< Arête vive (hard normal)
        float32 crease    = 0.f;            ///< Poids Catmull-Clark [0..1]
        uint32  uvA       = kNkHEInvalid;   ///< Index UV face-côté gauche
        uint32  uvB       = kNkHEInvalid;   ///< Index UV face-côté droite

        [[nodiscard]] bool IsBoundary() const noexcept { return twin == kNkHEInvalid; }
    };

    struct NkHEFace {
        uint32  halfEdge  = kNkHEInvalid;   ///< Un des half-edges de la face
        uint32  vertCount = 3;              ///< Nombre de vertices (3=tri, 4=quad)
        NkVec3f normal    = {0,1,0};
        uint32  material  = 0;
        bool    selected  = false;
        bool    hidden    = false;
        bool    smooth    = true;
    };

    // =========================================================================
    // NkHalfEdgeMesh — mesh avec topologie half-edge
    // =========================================================================
    class NkHalfEdgeMesh {
    public:
        NkHalfEdgeMesh()  noexcept = default;
        ~NkHalfEdgeMesh() noexcept = default;
        NkHalfEdgeMesh(NkHalfEdgeMesh&&) noexcept = default;
        NkHalfEdgeMesh& operator=(NkHalfEdgeMesh&&) noexcept = default;
        NkHalfEdgeMesh(const NkHalfEdgeMesh&) = delete;
        NkHalfEdgeMesh& operator=(const NkHalfEdgeMesh&) = delete;

        // ── Conversion ────────────────────────────────────────────────────
        /**
         * @brief Construit depuis un NkEditableMesh.
         * O(n log n) — trie les edges pour trouver les twins.
         */
        static NkHalfEdgeMesh FromEditable(const NkEditableMesh& mesh) noexcept;

        /**
         * @brief Convertit vers un NkEditableMesh.
         */
        [[nodiscard]] NkEditableMesh ToEditable() const noexcept;

        // ── Euler Operators (opérations topologiques fondamentales) ───────

        /**
         * @brief Flip une edge (inverse l'orientation d'une edge partagée).
         * Fonctionne uniquement sur les edges non-bord partageant 2 tris.
         * @return true si l'opération a réussi.
         */
        bool FlipEdge(uint32 halfEdgeId) noexcept;

        /**
         * @brief Split une edge : ajoute un vertex au milieu.
         * @param t Position sur l'edge [0..1].
         * @return Index du nouveau vertex créé.
         */
        uint32 SplitEdge(uint32 halfEdgeId, float32 t = 0.5f) noexcept;

        /**
         * @brief Collapse une edge : fusionne les 2 vertices en un.
         * @param t Paramètre de position du vertex résultant [0..1].
         * @return true si sûr à colapser (valence ≥ 3 partout).
         */
        bool CollapseEdge(uint32 halfEdgeId, float32 t = 0.5f) noexcept;

        /**
         * @brief Split une face en ajoutant un vertex au centre.
         */
        uint32 SubdivideFace(uint32 faceId) noexcept;

        /**
         * @brief Extrude des faces.
         */
        void ExtrudeFaces(NkSpan<const uint32> faceIds, float32 distance) noexcept;

        /**
         * @brief Bevel une edge (convertit edge → face).
         */
        void BevelEdge(uint32 halfEdgeId, float32 width,
                        uint32 segments = 1) noexcept;

        /**
         * @brief Collapse une face (supprime la face et fusionne les vertices).
         */
        void CollapseFace(uint32 faceId) noexcept;

        /**
         * @brief Dissolve une edge (supprime l'edge, fusionne les 2 faces).
         */
        bool DissolveEdge(uint32 halfEdgeId) noexcept;

        /**
         * @brief Dissolve un vertex (supprime le vertex et le comble).
         */
        bool DissolveVertex(uint32 vertexId) noexcept;

        // ── Traversal ─────────────────────────────────────────────────────

        /**
         * @brief Retourne tous les half-edges autour d'un vertex (1-ring).
         */
        void VertexStar(uint32 vertexId,
                         NkVector<uint32>& outHalfEdges) const noexcept;

        /**
         * @brief Retourne les faces adjacentes à un vertex.
         */
        void VertexFaces(uint32 vertexId,
                          NkVector<uint32>& outFaces) const noexcept;

        /**
         * @brief Retourne les vertices des faces adjacentes à un vertex.
         */
        void VertexNeighbors(uint32 vertexId,
                              NkVector<uint32>& outVerts) const noexcept;

        /**
         * @brief Valence d'un vertex (nombre de faces adjacentes).
         */
        [[nodiscard]] uint32 VertexValence(uint32 vertexId) const noexcept;

        /**
         * @brief Retourne les half-edges formant une edge loop.
         * Commence à halfEdgeId et suit les edges perpendiculaires.
         */
        void EdgeLoop(uint32 halfEdgeId,
                       NkVector<uint32>& outLoop) const noexcept;

        /**
         * @brief Retourne les half-edges formant un edge ring.
         */
        void EdgeRing(uint32 halfEdgeId,
                       NkVector<uint32>& outRing) const noexcept;

        /**
         * @brief Retourne les half-edges de bord (boundary).
         */
        void BoundaryEdges(NkVector<uint32>& out) const noexcept;

        /**
         * @brief Retourne les composantes connexes (îles) du mesh.
         */
        void ConnectedComponents(NkVector<NkVector<uint32>>& outFaceGroups) const noexcept;

        // ── Analyse ───────────────────────────────────────────────────────

        [[nodiscard]] bool IsManifold()           const noexcept;
        [[nodiscard]] bool IsClosed()             const noexcept;
        [[nodiscard]] bool HasBoundary()          const noexcept;
        [[nodiscard]] int32 EulerCharacteristic() const noexcept;  ///< V-E+F
        [[nodiscard]] uint32 Genus()              const noexcept;  ///< Nb de "trous"

        // ── Opérations algorithmiques ─────────────────────────────────────

        /**
         * @brief Catmull-Clark subdivision.
         */
        NkHalfEdgeMesh Subdivide(uint32 levels = 1) const noexcept;

        /**
         * @brief Décimation par collapse d'edge (QEM — Quadric Error Metrics).
         * @param ratio [0..1] fraction de faces à conserver.
         */
        NkHalfEdgeMesh Decimate(float32 ratio = 0.5f) const noexcept;

        /**
         * @brief Lissage Laplacien.
         * @param iterations Nombre de passes.
         * @param lambda     Facteur de lissage [0..1].
         */
        void SmoothLaplacian(uint32 iterations = 5, float32 lambda = 0.5f) noexcept;

        /**
         * @brief Recalcule toutes les normales (smooth ou flat).
         */
        void RecalcNormals(bool smooth = true) noexcept;

        /**
         * @brief Triangule le mesh (quads → 2 triangles).
         */
        NkHalfEdgeMesh Triangulate() const noexcept;

        // ── Sélection ─────────────────────────────────────────────────────
        void SelectAll()                    noexcept;
        void DeselectAll()                  noexcept;
        void SelectEdgeLoop(uint32 heId)    noexcept;
        void SelectEdgeRing(uint32 heId)    noexcept;
        void SelectLinked(uint32 faceId)    noexcept;  ///< Sélectionne composante connexe
        void GrowVertexSelection()          noexcept;
        void ShrinkVertexSelection()        noexcept;

        // ── Accès ─────────────────────────────────────────────────────────
        [[nodiscard]] NkVector<NkHEVertex>&   Vertices()  noexcept { return mVerts; }
        [[nodiscard]] NkVector<NkHEHalfEdge>& HalfEdges() noexcept { return mHEdges; }
        [[nodiscard]] NkVector<NkHEFace>&     Faces()     noexcept { return mFaces; }

        [[nodiscard]] const NkHEVertex&   V(uint32 i) const noexcept { return mVerts[i]; }
        [[nodiscard]] const NkHEHalfEdge& H(uint32 i) const noexcept { return mHEdges[i]; }
        [[nodiscard]] const NkHEFace&     F(uint32 i) const noexcept { return mFaces[i]; }

        [[nodiscard]] uint32 VertexCount()   const noexcept { return static_cast<uint32>(mVerts.Size()); }
        [[nodiscard]] uint32 HalfEdgeCount() const noexcept { return static_cast<uint32>(mHEdges.Size()); }
        [[nodiscard]] uint32 EdgeCount()     const noexcept { return static_cast<uint32>(mHEdges.Size()) / 2; }
        [[nodiscard]] uint32 FaceCount()     const noexcept { return static_cast<uint32>(mFaces.Size()); }

        void Clear() noexcept { mVerts.Clear(); mHEdges.Clear(); mFaces.Clear(); }

        /**
         * @brief Vérifie la cohérence interne de la structure (debug).
         * @return Liste des erreurs détectées (vide = OK).
         */
        NkVector<NkString> Validate() const noexcept;

    private:
        // ── Helpers ───────────────────────────────────────────────────────
        uint32 NewVertex  () noexcept { mVerts.PushBack({});  return VertexCount()-1; }
        uint32 NewHalfEdge() noexcept { mHEdges.PushBack({}); return HalfEdgeCount()-1; }
        uint32 NewFace    () noexcept { mFaces.PushBack({});  return FaceCount()-1; }

        void SetTwins(uint32 he0, uint32 he1) noexcept {
            mHEdges[he0].twin = he1;
            mHEdges[he1].twin = he0;
        }

        NkVec3f FaceCenter(uint32 faceId) const noexcept;
        NkVec3f EdgeCenter(uint32 halfEdgeId) const noexcept;
        bool    SafeToCollapse(uint32 heId) const noexcept;

        // ── Données ───────────────────────────────────────────────────────
        NkVector<NkHEVertex>   mVerts;
        NkVector<NkHEHalfEdge> mHEdges;
        NkVector<NkHEFace>     mFaces;
    };

} // namespace nkentseu
