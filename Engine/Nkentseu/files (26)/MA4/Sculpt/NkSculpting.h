#pragma once
// =============================================================================
// Nkentseu/Modeling/NkSculpting.h
// =============================================================================
// Système de sculpt 3D en temps réel.
//
// BRUSHES DISPONIBLES :
//   Draw      — pousse les vertices dans la direction de la normale
//   Grab      — déplace un groupe de vertices
//   Smooth    — lisse la surface
//   Flatten   — aplatit vers un plan moyen
//   Clay      — accumulation de matière (style glaise)
//   Inflate   — gonfle les vertices vers l'extérieur
//   Pinch     — pince les vertices vers le centre du brush
//   Crease    — creuse une rainure
//   Snake     — tire une "corde" de matière
//   Mask      — masque des zones contre la déformation
//
// ALGORITHME :
//   1. Raycast depuis la caméra → point de contact sur le mesh
//   2. Sélection des vertices dans le rayon du brush (KD-tree ou GPU)
//   3. Application du brush (déplacement selon la force et la distance)
//   4. Mise à jour GPU via NkResourceManager::UpdateMeshVertices(isDynamic=true)
//   5. Recalcul des normales affectées
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NkEditableMesh.h"
#include "Nkentseu/Viewport/NkViewportCamera.h"

namespace nkentseu {
    using namespace math;

    // =========================================================================
    // NkSculptBrushType
    // =========================================================================
    enum class NkSculptBrushType : uint8 {
        Draw, Grab, Smooth, Flatten, Clay, Inflate,
        Pinch, Crease, Snake, Mask, SmoothMask, FillMask
    };

    // =========================================================================
    // NkSculptBrush — paramètres d'un pinceau de sculpt
    // =========================================================================
    struct NkSculptBrush {
        NkSculptBrushType type       = NkSculptBrushType::Draw;
        float32           radius     = 0.5f;    ///< Rayon en unités monde
        float32           strength   = 0.5f;    ///< Force [0..1]
        float32           hardness   = 0.7f;    ///< Falloff du brush [0=doux, 1=dur]
        bool              autosmooth = 0.2f;    ///< Lissage automatique post-déformation
        bool              symmetryX  = false;   ///< Symétrie axe X
        bool              symmetryY  = false;
        bool              symmetryZ  = false;
        bool              pressureRadius   = false;  ///< Pression → rayon
        bool              pressureStrength = true;   ///< Pression → force
        float32           spacing    = 0.2f;    ///< Espacement dabs (fraction du rayon)

        // Texture de brush (module la force selon un pattern)
        nk_uint64         texHandle  = 0;       ///< 0 = pas de texture
        float32           texScale   = 1.f;
        float32           texStrength = 0.5f;
    };

    // =========================================================================
    // NkSculptSession — session de sculpt sur un mesh
    // =========================================================================
    class NkSculptSession {
    public:
        NkSculptSession() noexcept = default;
        ~NkSculptSession() noexcept = default;

        /**
         * @brief Initialise la session sur un mesh éditable.
         * @param mesh    Mesh à sculpter.
         * @param undoStack Stack undo pour les opérations de sculpt.
         */
        bool Init(NkEditableMesh* mesh,
                   NkUndoStack* undoStack = nullptr) noexcept;

        // ── Contrôle de la session ────────────────────────────────────────
        void SetBrush   (const NkSculptBrush& brush) noexcept { mBrush = brush; }
        void SetCamera  (const NkViewportCamera* cam) noexcept { mCamera = cam; }
        void SetDynamic (bool v) noexcept { mDynamic = v; }  ///< Remesh dynamique

        // ── Tracé ─────────────────────────────────────────────────────────
        /**
         * @brief Début d'un trait de sculpt.
         * @param screenPos Position écran du pointeur (pixels).
         * @param vpW, vpH  Taille du viewport.
         * @param pressure  Pression stylet [0..1].
         */
        void PointerDown(NkVec2f screenPos, float32 vpW, float32 vpH,
                          float32 pressure = 1.f) noexcept;

        void PointerMove(NkVec2f screenPos, float32 vpW, float32 vpH,
                          float32 pressure = 1.f) noexcept;

        void PointerUp() noexcept;

        // ── Résultat ──────────────────────────────────────────────────────
        /**
         * @brief true si des vertices ont été modifiés depuis le dernier appel.
         */
        [[nodiscard]] bool HasChanges() const noexcept { return mHasChanges; }

        /**
         * @brief Invalide le flag de changement (après avoir uploadé le mesh GPU).
         */
        void ClearChanges() noexcept { mHasChanges = false; }

        /**
         * @brief Retourne la région AABB des vertices modifiés (pour upload partiel).
         */
        [[nodiscard]] const NkAABB& GetDirtyRegion() const noexcept { return mDirtyRegion; }

    private:
        // ── Raycast + brush application ───────────────────────────────────
        bool Raycast(NkVec2f screenPos, float32 vpW, float32 vpH,
                      NkVec3f& outHitPos, NkVec3f& outHitNormal) const noexcept;

        void ApplyBrush(NkVec3f hitPos, NkVec3f hitNormal,
                         float32 pressure) noexcept;

        // Brushes spécifiques
        void ApplyDraw   (NkVec3f pos, NkVec3f normal, float32 strength) noexcept;
        void ApplyGrab   (NkVec3f pos, NkVec3f delta) noexcept;
        void ApplySmooth (NkVec3f pos, float32 strength) noexcept;
        void ApplyFlatten(NkVec3f pos, NkVec3f normal, float32 strength) noexcept;
        void ApplyClay   (NkVec3f pos, NkVec3f normal, float32 strength) noexcept;
        void ApplyInflate(NkVec3f pos, float32 strength) noexcept;
        void ApplyPinch  (NkVec3f pos, float32 strength) noexcept;

        float32 FalloffAt(NkVec3f vertex, NkVec3f center) const noexcept;

        // ── Collecte des vertices affectés ────────────────────────────────
        void CollectVerticesInRadius(NkVec3f center, float32 radius,
                                      NkVector<uint32>& out) const noexcept;

        // ── BVH simple pour accélérer le raycast ─────────────────────────
        void RebuildBVH() noexcept;
        bool RaycastBVH(const NkRay& ray, NkVec3f& hitPos,
                         NkVec3f& hitNormal) const noexcept;

        // ── État ──────────────────────────────────────────────────────────
        NkEditableMesh*        mMesh       = nullptr;
        NkUndoStack*           mUndoStack  = nullptr;
        const NkViewportCamera* mCamera    = nullptr;

        NkSculptBrush          mBrush;
        bool                   mDrawing    = false;
        bool                   mHasChanges = false;
        bool                   mDynamic    = false;

        NkVec3f mLastHitPos  = {};
        float32 mDistAccum   = 0.f;

        NkAABB  mDirtyRegion;

        // BVH (Bounding Volume Hierarchy) — octree simplifié
        struct BVHNode {
            NkAABB  bounds;
            uint32  firstFace = 0;
            uint32  faceCount = 0;
            uint32  left  = kNkInvalidIdx;
            uint32  right = kNkInvalidIdx;
            bool    isLeaf = true;
        };
        NkVector<BVHNode> mBVH;
        NkVector<uint32>  mSortedFaces;

        // Cache des positions originales (pour undo)
        NkVector<uint32>  mModifiedVerts;
        NkVector<NkVec3f> mOriginalPositions;
    };

} // namespace nkentseu
