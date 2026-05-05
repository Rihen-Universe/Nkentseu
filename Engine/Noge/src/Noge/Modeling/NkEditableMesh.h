#pragma once
// =============================================================================
// Nkentseu/Modeling/NkEditableMesh.h
// =============================================================================
// Représentation CPU d'un mesh éditable pour la modélisation polygonale.
//
// DESIGN :
//   • Données stockées côté CPU (pas de référence GPU directe)
//   • Opérations non-destructives via NkMeshModifier stack
//   • Conversion vers NkMeshDesc pour upload GPU via NkMeshUploader
//   • Tout changement structurel (add/remove vertex/face) invalide les handles
//
// TOPOLOGIE :
//   Vertex → référence des edges
//   Edge   → half-edge pair (src → dst, et dst → src dans la même NkEdge)
//   Face   → référence 3-4 vertices (triangle ou quad)
//
// USAGE TYPIQUE :
//   NkEditableMesh em;
//   uint32 v0 = em.AddVertex({-1,0,0}, {0,0});
//   uint32 v1 = em.AddVertex({ 1,0,0}, {1,0});
//   uint32 v2 = em.AddVertex({ 0,1,0}, {0.5f,1});
//   em.AddTri(v0, v1, v2);
//   em.RecalcNormals();
//   renderer::NkMeshDesc gpuDesc;
//   em.ToMeshDesc(gpuDesc);
//   NkMeshHandle h = resources.CreateMesh(gpuDesc);
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKRenderer/src/Resources/NkResourceDescs.h"

namespace nkentseu {

    using namespace math;

    // =========================================================================
    // Types fondamentaux
    // =========================================================================

    static constexpr uint32 kNkInvalidIdx = 0xFFFFFFFFu;

    // ── Vertex ────────────────────────────────────────────────────────────────
    struct NkEditVertex {
        NkVec3f  position  = {0, 0, 0};
        NkVec3f  normal    = {0, 1, 0};  ///< Recomputed par RecalcNormals()
        NkVec2f  uv        = {0, 0};     ///< UV channel 0
        NkVec2f  uv2       = {0, 0};     ///< UV channel 1 (lightmap)
        NkVec4f  color     = {1, 1, 1, 1}; ///< Vertex color

        uint32   edgeStart = kNkInvalidIdx; ///< Premier edge adjacent (half-edge)

        bool     selected  = false;
        bool     hidden    = false;
        uint8    _pad[2]   = {};
    };

    // ── Edge (half-edge pair) ─────────────────────────────────────────────────
    struct NkEditEdge {
        uint32 v0 = kNkInvalidIdx;  ///< Vertex source
        uint32 v1 = kNkInvalidIdx;  ///< Vertex destination
        uint32 twin = kNkInvalidIdx; ///< Edge opposé (v1→v0)
        uint32 next = kNkInvalidIdx; ///< Prochain half-edge sur la même face
        uint32 face = kNkInvalidIdx; ///< Face adjacente

        bool   selected = false;
        bool   seam     = false;    ///< Couture UV (pour unwrap)
        bool   sharp    = false;    ///< Arête vive (pour normales)
        uint8  _pad     = 0;

        [[nodiscard]] bool IsValid() const noexcept {
            return v0 != kNkInvalidIdx && v1 != kNkInvalidIdx;
        }
    };

    // ── Face ──────────────────────────────────────────────────────────────────
    struct NkEditFace {
        static constexpr uint32 kMaxVerts = 4u;

        uint32  verts[kMaxVerts] = { kNkInvalidIdx, kNkInvalidIdx,
                                     kNkInvalidIdx, kNkInvalidIdx };
        uint32  edges[kMaxVerts] = { kNkInvalidIdx, kNkInvalidIdx,
                                     kNkInvalidIdx, kNkInvalidIdx };
        uint32  vertCount = 0;      ///< 3 = triangle, 4 = quad

        NkVec3f normal     = {0, 1, 0};
        uint32  materialId = 0;

        bool    selected   = false;
        bool    hidden     = false;
        bool    smooth     = true;  ///< Smooth vs flat shading
        uint8   _pad       = 0;

        [[nodiscard]] bool IsTriangle() const noexcept { return vertCount == 3; }
        [[nodiscard]] bool IsQuad()     const noexcept { return vertCount == 4; }
        [[nodiscard]] bool IsValid()    const noexcept { return vertCount >= 3; }
    };

    // =========================================================================
    // NkEditableMesh
    // =========================================================================
    class NkEditableMesh {
        public:
            NkEditableMesh()  noexcept = default;
            ~NkEditableMesh() noexcept = default;

            // Pas de copie directe — utiliser Clone()
            NkEditableMesh(const NkEditableMesh&) = delete;
            NkEditableMesh& operator=(const NkEditableMesh&) = delete;

            // Déplacement autorisé
            NkEditableMesh(NkEditableMesh&&) noexcept = default;
            NkEditableMesh& operator=(NkEditableMesh&&) noexcept = default;

            // ── Création de topologie ─────────────────────────────────────

            /**
             * @brief Ajoute un vertex et retourne son index.
             */
            uint32 AddVertex(const NkVec3f& pos,
                             const NkVec2f& uv      = {},
                             const NkVec3f& normal   = {0,1,0},
                             const NkVec4f& color    = {1,1,1,1}) noexcept;

            /**
             * @brief Ajoute un triangle (v0, v1, v2 dans l'ordre CCW).
             * @return Index de la face créée.
             */
            uint32 AddTri(uint32 v0, uint32 v1, uint32 v2) noexcept;

            /**
             * @brief Ajoute un quad (v0, v1, v2, v3 dans l'ordre CCW).
             * @return Index de la face créée.
             */
            uint32 AddQuad(uint32 v0, uint32 v1,
                           uint32 v2, uint32 v3) noexcept;

            // ── Opérations de modélisation ────────────────────────────────

            /**
             * @brief Extrude les faces sélectionnées dans une direction.
             * @param faceIds  Faces à extruder (vide = toutes les faces sélectionnées).
             * @param direction Vecteur d'extrusion dans l'espace local.
             */
            void ExtrudeFaces(NkSpan<const uint32> faceIds,
                              const NkVec3f& direction) noexcept;

            /**
             * @brief Biseaute les arêtes sélectionnées.
             * @param edgeIds  Arêtes à biseauter.
             * @param width    Largeur du biseau.
             * @param segments Nombre de subdivisions du biseau.
             */
            void BevelEdges(NkSpan<const uint32> edgeIds,
                            float32 width,
                            uint32  segments = 1) noexcept;

            /**
             * @brief Coupe une edge loop au point t [0..1].
             * @param edgeId   Edge de référence pour localiser la loop.
             * @param factor   Position de la coupe [0=v0, 1=v1].
             */
            void LoopCut(uint32 edgeId, float32 factor = 0.5f) noexcept;

            /**
             * @brief Fusionne les vertices trop proches.
             * @param threshold Distance maximale entre vertices à fusionner.
             * @return Nombre de vertices fusionnés.
             */
            uint32 MergeByDistance(float32 threshold = 0.0001f) noexcept;

            /**
             * @brief Inverse la direction des normales des faces.
             * @param faceIds Faces à retourner (vide = toutes).
             */
            void FlipNormals(NkSpan<const uint32> faceIds = {}) noexcept;

            /**
             * @brief Triangule tous les quads.
             * @param method 0=Fan (rapide), 1=Beauty (optimal).
             */
            void Triangulate(uint32 method = 1) noexcept;

            /**
             * @brief Subdivise en appliquant Catmull-Clark.
             * @param levels Nombre de niveaux de subdivision.
             */
            void Subdivide(uint32 levels = 1) noexcept;

            // ── Normales ──────────────────────────────────────────────────

            /**
             * @brief Recalcule toutes les normales de faces et de vertices.
             * @param smooth true = moyennage des normales adjacentes (smooth shading).
             */
            void RecalcNormals(bool smooth = true) noexcept;

            /**
             * @brief Recalcule uniquement les normales des faces listées.
             */
            void RecalcFaceNormals(NkSpan<const uint32> faceIds = {}) noexcept;

            // ── Sélection ─────────────────────────────────────────────────

            void SelectAll()                         noexcept;
            void DeselectAll()                       noexcept;
            void InvertSelection()                   noexcept;
            void SelectVertex(uint32 id, bool v)     noexcept;
            void SelectFace(uint32 id, bool v)       noexcept;
            void SelectEdge(uint32 id, bool v)       noexcept;

            /**
             * @brief Étend la sélection aux voisins (un anneau).
             */
            void GrowSelection()  noexcept;
            void ShrinkSelection() noexcept;

            /**
             * @brief Retourne les indices des vertices/faces/edges sélectionnés.
             */
            void GetSelectedVertices(NkVector<uint32>& out) const noexcept;
            void GetSelectedFaces   (NkVector<uint32>& out) const noexcept;
            void GetSelectedEdges   (NkVector<uint32>& out) const noexcept;

            // ── UVs ──────────────────────────────────────────────────────

            /**
             * @brief Projection Smart UV (angle limit pour séparer les îles).
             */
            void SmartUVProject(float32 angleLimit = 66.f,
                                float32 islandMargin = 0.02f) noexcept;

            /**
             * @brief Projection cubique (6 faces, selon la normale dominante).
             */
            void CubeProject(float32 scale = 1.f) noexcept;

            /**
             * @brief Projection cylindrique autour de l'axe Y.
             */
            void CylindricalProject(float32 scale = 1.f) noexcept;

            // ── AABB ──────────────────────────────────────────────────────

            [[nodiscard]] const NkAABB& GetBounds() const noexcept;
            void RecomputeBounds() noexcept;

            // ── Accès aux données ─────────────────────────────────────────

            [[nodiscard]] NkVector<NkEditVertex>& Vertices() noexcept { return mVerts; }
            [[nodiscard]] NkVector<NkEditEdge>&   Edges()    noexcept { return mEdges; }
            [[nodiscard]] NkVector<NkEditFace>&   Faces()    noexcept { return mFaces; }

            [[nodiscard]] const NkVector<NkEditVertex>& Vertices() const noexcept { return mVerts; }
            [[nodiscard]] const NkVector<NkEditEdge>&   Edges()    const noexcept { return mEdges; }
            [[nodiscard]] const NkVector<NkEditFace>&   Faces()    const noexcept { return mFaces; }

            [[nodiscard]] uint32 VertexCount() const noexcept { return static_cast<uint32>(mVerts.Size()); }
            [[nodiscard]] uint32 FaceCount()   const noexcept { return static_cast<uint32>(mFaces.Size()); }
            [[nodiscard]] uint32 EdgeCount()   const noexcept { return static_cast<uint32>(mEdges.Size()); }

            // ── Conversion GPU ────────────────────────────────────────────

            /**
             * @brief Convertit vers NkMeshDesc pour upload GPU.
             * @note Les quads sont triangulés. Les normales sont générées si absentes.
             */
            void ToMeshDesc(renderer::NkMeshDesc& out) const noexcept;

            /**
             * @brief Clone le mesh (deep copy).
             */
            NkEditableMesh Clone() const noexcept;

            /**
             * @brief Vide complètement le mesh.
             */
            void Clear() noexcept {
                mVerts.Clear();
                mEdges.Clear();
                mFaces.Clear();
                mBoundsDirty = true;
                mNormalsDirty = true;
            }

            /**
             * @brief Génère un mesh à partir d'une NkMeshDesc (pour édition post-création GPU).
             */
            static NkEditableMesh FromMeshDesc(const renderer::NkMeshDesc& desc) noexcept;

        private:
            // ── Helpers topologie ─────────────────────────────────────────
            uint32 FindOrCreateEdge(uint32 v0, uint32 v1) noexcept;
            void   LinkFaceEdges(uint32 faceId) noexcept;
            void   ComputeFaceNormal(uint32 faceId) noexcept;

            // ── Algorithmes UV ────────────────────────────────────────────
            void ComputeIslands(NkVector<NkVector<uint32>>& islands) const noexcept;
            void PackIslands(NkVector<NkVector<uint32>>& islands,
                             float32 margin) noexcept;

            // ── Données ───────────────────────────────────────────────────
            NkVector<NkEditVertex> mVerts;
            NkVector<NkEditEdge>   mEdges;
            NkVector<NkEditFace>   mFaces;

            mutable NkAABB mBounds;
            mutable bool   mBoundsDirty  = true;
            mutable bool   mNormalsDirty = true;
    };

} // namespace nkentseu
