#pragma once
// =============================================================================
// Nkentseu/Viewport/NkGizmo.h
// =============================================================================
// Handles 3D interactifs pour transformer des objets dans le viewport.
// Translate (XYZ + plans), Rotate (XYZ + Screen), Scale (XYZ + Uniform).
//
// Rendu via NkRender3D::DrawLine/DrawAxis — pas de mesh dédié.
// =============================================================================
#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NkViewportCamera.h"

namespace nkentseu {
    using namespace math;

    class NkGizmo {
    public:
        enum class Mode  : uint8 { Translate, Rotate, Scale };
        enum class Space : uint8 { World, Local };
        enum class Axis  : uint8 { None, X, Y, Z, XY, XZ, YZ, XYZ, Screen };

        Mode    mode    = Mode::Translate;
        Space   space   = Space::World;
        float32 size    = 1.f;      // taille en pixels normalisés
        bool    snapping   = false;
        float32 snapTrans  = 0.5f;
        float32 snapRot    = 5.f;   // degrés
        float32 snapScale  = 0.1f;

        // ── Boucle éditeur ────────────────────────────────────────────────
        // Retourne true si en cours de drag
        bool Update(const NkViewportCamera& cam,
                    const NkMat4f& objectTf,
                    NkVec2f mousePos, NkVec2f vpSize,
                    bool mouseDown, bool mouseDragging) noexcept;

        [[nodiscard]] NkVec3f GetTranslateDelta() const noexcept;
        [[nodiscard]] NkQuatf GetRotateDelta()    const noexcept;
        [[nodiscard]] NkVec3f GetScaleDelta()     const noexcept;
        [[nodiscard]] bool    IsDragging()        const noexcept { return mDragging; }
        [[nodiscard]] Axis    GetActiveAxis()     const noexcept { return mActiveAxis; }

        // ── Rendu ─────────────────────────────────────────────────────────
        void Draw(renderer::NkRender3D& r3d,
                  const NkMat4f& objectTf,
                  const NkViewportCamera& cam) const noexcept;

    private:
        Axis    mActiveAxis    = Axis::None;
        bool    mDragging      = false;
        NkVec3f mDragOrigin    = {};
        NkVec3f mDragAxis      = {1,0,0};
        NkMat4f mStartTransform = NkMat4f::Identity();
        float32 mStartAngle    = 0.f;

        Axis HitTest(const NkRay& ray, const NkMat4f& tf, const NkViewportCamera& cam) const noexcept;
        NkVec3f ProjectOnAxis(const NkRay& ray, const NkVec3f& axisOrigin, const NkVec3f& axisDir) const noexcept;
    };
} // namespace nkentseu
