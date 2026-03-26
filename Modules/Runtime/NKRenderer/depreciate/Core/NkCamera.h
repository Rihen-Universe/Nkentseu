#pragma once
// =============================================================================
// NkCamera.h — Caméra 2D orthographique et caméra 3D perspective.
// =============================================================================
#include "NKRenderer/Core/NkRenderTypes.h"

namespace nkentseu {

    // =========================================================================
    // NkCamera2D — caméra orthographique 2D
    // =========================================================================
    class NKRENDERER_API NkCamera2D {
        public:
            // Construit une caméra ortho [0,width] x [0,height], Y vers le bas (UI).
            NkCamera2D() = default;
            NkCamera2D(float32 width, float32 height,
                    float32 nearZ = -1.f, float32 farZ = 1.f) noexcept;

            void SetOrtho(float32 width, float32 height,
                        float32 nearZ = -1.f, float32 farZ = 1.f) noexcept;

            void SetPosition(NkVec2 pos) noexcept { mPosition = pos; mDirty = true; }
            void SetZoom(float32 zoom)   noexcept { mZoom = zoom;    mDirty = true; }
            void SetRotation(float32 angleDeg) noexcept { mRotation = angleDeg; mDirty = true; }

            NkVec2  GetPosition() const noexcept { return mPosition; }
            float32 GetZoom()     const noexcept { return mZoom; }
            float32 GetRotation() const noexcept { return mRotation; }
            float32 GetWidth()    const noexcept { return mWidth; }
            float32 GetHeight()   const noexcept { return mHeight; }

            const NkMat4& GetProjection() const noexcept { RebuildIfDirty(); return mProjection; }
            const NkMat4& GetView()       const noexcept { RebuildIfDirty(); return mView; }
            NkMat4        GetViewProjection() const noexcept;

            // Conversion écran → monde
            NkVec2 ScreenToWorld(NkVec2 screenPos) const noexcept;
            NkVec2 WorldToScreen(NkVec2 worldPos)  const noexcept;

        private:
            void RebuildIfDirty() const noexcept;

            float32       mWidth    = 800.f;
            float32       mHeight   = 600.f;
            float32       mNearZ    = -1.f;
            float32       mFarZ     = 1.f;
            NkVec2        mPosition = {};
            float32       mZoom     = 1.f;
            float32       mRotation = 0.f;
            mutable NkMat4 mProjection;
            mutable NkMat4 mView;
            mutable bool  mDirty   = true;
    };

    // =========================================================================
    // NkCamera3D — caméra perspective 3D
    // =========================================================================
    class NKRENDERER_API NkCamera3D {
        public:
            NkCamera3D() = default;
            NkCamera3D(float32 fovDeg, float32 aspect,
                    float32 nearZ = 0.01f, float32 farZ = 1000.f) noexcept;

            // ── Projection ──────────────────────────────────────────────────────
            void SetPerspective(float32 fovDeg, float32 aspect,
                                float32 nearZ = 0.01f, float32 farZ = 1000.f) noexcept;
            void SetOrthographic(float32 left, float32 right,
                                float32 bottom, float32 top,
                                float32 nearZ = -10.f, float32 farZ = 10.f) noexcept;

            // ── Transform ───────────────────────────────────────────────────────
            void SetPosition(NkVec3 pos)    noexcept { mPosition = pos;    mDirty = true; }
            void SetTarget  (NkVec3 target) noexcept { mTarget   = target; mDirty = true; }
            void SetUp      (NkVec3 up)     noexcept { mUp       = up;     mDirty = true; }

            void LookAt(NkVec3 eye, NkVec3 target, NkVec3 up) noexcept;

            // FPS-style rotation
            void SetYaw  (float32 deg) noexcept { mYaw   = deg; mDirty = true; }
            void SetPitch(float32 deg) noexcept { mPitch = deg; mDirty = true; }

            NkVec3  GetPosition() const noexcept { return mPosition; }
            NkVec3  GetTarget()   const noexcept { return mTarget;   }
            float32 GetFOV()      const noexcept { return mFovDeg;   }
            float32 GetAspect()   const noexcept { return mAspect;   }
            float32 GetNearZ()    const noexcept { return mNearZ;    }
            float32 GetFarZ()     const noexcept { return mFarZ;     }
            NkVec3  GetForward()  const noexcept;
            NkVec3  GetRight()    const noexcept;

            const NkMat4& GetProjection() const noexcept { RebuildIfDirty(); return mProjection; }
            const NkMat4& GetView()       const noexcept { RebuildIfDirty(); return mView; }
            NkMat4        GetViewProjection() const noexcept;

            // Espace NDC selon l'API GPU (Vulkan = zero-one, OpenGL = neg-one)
            void SetProjectionSpace(NkProjectionSpace ps) noexcept { mProjSpace = ps; mDirty = true; }

        private:
            void RebuildIfDirty() const noexcept;
            void RebuildPerspective() const noexcept;
            void RebuildView() const noexcept;

            bool           mIsPerspective = true;
            float32        mFovDeg  = 60.f;
            float32        mAspect  = 16.f/9.f;
            float32        mNearZ   = 0.01f;
            float32        mFarZ    = 1000.f;
            // Ortho
            float32        mOrthoLeft = -1.f, mOrthoRight = 1.f;
            float32        mOrthoBottom = -1.f, mOrthoTop = 1.f;

            NkVec3         mPosition = {0,0,5};
            NkVec3         mTarget   = {0,0,0};
            NkVec3         mUp       = {0,1,0};
            float32        mYaw      = -90.f;
            float32        mPitch    = 0.f;

            NkProjectionSpace mProjSpace = NkProjectionSpace::NK_NDC_ZERO_ONE;

            mutable NkMat4 mProjection;
            mutable NkMat4 mView;
            mutable bool   mDirty = true;
    };

} // namespace nkentseu
