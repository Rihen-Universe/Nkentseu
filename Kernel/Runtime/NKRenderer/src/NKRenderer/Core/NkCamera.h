#pragma once
// =============================================================================
// NkCamera.h  — NKRenderer v5.0  (Core/)
//
// Hierarchie de cameras :
//
//      NkCamera   (base — view + proj + viewProj + UBO commun)
//        |
//        +- NkCamera3D  (perspective/ortho 3D, LookAt, frustum culling)
//        |
//        +- NkCamera2D  (ortho 2D, center/zoom/rotation)
//
// La base permet d'ecrire des fonctions qui acceptent n'importe quelle camera
// (utile pour upload UBO generique, debug overlay, picking 2D dans une vue 3D).
// La specialisation forte garantit a compile-time qu'on n'utilise pas une cam
// 2D ou ne s'attend a une 3D (et vice-versa) — pas de bugs runtime du genre
// "GetForward() sur une cam 2D".
// =============================================================================
#include "NkRendererTypes.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
    namespace renderer {

        // (NkCameraUBO defini dans NkRendererTypes.h — version etendue avec
        //  invView/invProj, depthParams, frustumPlanes pour shader-side culling.)

        // =====================================================================
        // NkCamera — classe de base
        // =====================================================================
        // Fournit l'API commune : matrices view/proj/viewProj, BuildUBO, dirty flag.
        // Les sous-classes implementent Rebuild() pour reconstruire les matrices.
        // =====================================================================
        class NkCamera {
            public:
                NkCamera() = default;
                virtual ~NkCamera() = default;

                // Matrices (lazy-rebuild si dirty)
                const NkMat4f& GetView()     const noexcept { Rebuild(); return mView; }
                const NkMat4f& GetProj()     const noexcept { Rebuild(); return mProj; }
                const NkMat4f& GetViewProj() const noexcept { Rebuild(); return mViewProj; }

                // UBO standard (rempli en fonction du type concret).
                virtual NkCameraUBO BuildUBO(float32 time = 0.f, float32 dt = 0.f) const noexcept = 0;

                // Marque les matrices comme obsoletes (les setters des sous-classes l'appellent).
                void Invalidate() noexcept { mDirty = true; }

            protected:
                // Reconstruit mView, mProj, mViewProj (et derives — frustum 3D, ortho 2D).
                virtual void RebuildImpl() const noexcept = 0;

                void Rebuild() const noexcept {
                    if (!mDirty) return;
                    RebuildImpl();
                    mDirty = false;
                }

                mutable NkMat4f mView      = NkMat4f::Identity();
                mutable NkMat4f mProj      = NkMat4f::Identity();
                mutable NkMat4f mViewProj  = NkMat4f::Identity();
                mutable bool    mDirty     = true;
        };

        // =====================================================================
        // NkCamera3D
        // =====================================================================
        class NkCamera3D : public NkCamera {
            public:
                NkCamera3D() = default;
                explicit NkCamera3D(const NkCamera3DData& data);

                // Setters
                void SetPosition (NkVec3f pos)                noexcept;
                void SetTarget   (NkVec3f target)             noexcept;
                void SetUp       (NkVec3f up)                 noexcept;
                void SetFOV      (float32 fovDeg)             noexcept;
                void SetAspect   (float32 aspect)             noexcept;
                void SetAspect   (uint32 w, uint32 h)         noexcept;
                void SetNearFar  (float32 near_, float32 far_)noexcept;
                void SetOrtho    (bool ortho, float32 size=10.f) noexcept;

                // Getters
                NkVec3f GetPosition()  const noexcept { return mData.position; }
                NkVec3f GetTarget()    const noexcept { return mData.target; }
                NkVec3f GetForward()   const noexcept;
                NkVec3f GetRight()     const noexcept;
                NkVec3f GetUp()        const noexcept { return mData.up; }
                float32 GetFOV()       const noexcept { return mData.fovY; }
                float32 GetNear()      const noexcept { return mData.nearPlane; }
                float32 GetFar()       const noexcept { return mData.farPlane; }
                float32 GetAspect()    const noexcept { return mData.aspect; }
                bool    IsOrtho()      const noexcept { return mData.ortho; }

                // Frustum culling (calcule a chaque appel les 6 plans normalises a partir du viewProj)
                bool IsAABBVisible(const NkAABB& aabb)               const noexcept;
                bool IsSphereVisible(NkVec3f center, float32 radius) const noexcept;

                // UBO override
                NkCameraUBO BuildUBO(float32 time = 0.f, float32 dt = 0.f) const noexcept override;

                // Raw data
                const NkCamera3DData& GetData() const noexcept { return mData; }
                void SetData(const NkCamera3DData& d) noexcept { mData = d; mDirty = true; }

            private:
                NkCamera3DData            mData;
                mutable float32           mFrustumPlanes[6][4] = {};

                void RebuildImpl() const noexcept override;
                void BuildFrustum() const noexcept;
        };

        // =====================================================================
        // NkCamera2D
        // =====================================================================
        class NkCamera2D : public NkCamera {
            public:
                NkCamera2D() = default;
                explicit NkCamera2D(const NkCamera2DData& data);

                void SetCenter  (NkVec2f c)    noexcept;
                void SetZoom    (float32 z)    noexcept;
                void SetRotation(float32 deg)  noexcept;
                void SetViewport(uint32 w, uint32 h) noexcept;

                NkVec2f GetCenter()   const noexcept { return mData.center; }
                float32 GetZoom()     const noexcept { return mData.zoom; }
                float32 GetRotation() const noexcept { return mData.rotation; }
                uint32  GetWidth()    const noexcept { return mData.width; }
                uint32  GetHeight()   const noexcept { return mData.height; }

                // Alias pour API uniforme (= GetProj() de la base, qui est l'ortho ici)
                const NkMat4f& GetOrtho() const noexcept { return GetProj(); }

                // Conversion screen <-> world
                NkVec2f ScreenToWorld(NkVec2f screen) const noexcept;
                NkVec2f WorldToScreen(NkVec2f world)  const noexcept;

                // UBO override (vide les champs 3D, remplit ortho)
                NkCameraUBO BuildUBO(float32 time = 0.f, float32 dt = 0.f) const noexcept override;

                const NkCamera2DData& GetData() const noexcept { return mData; }
                void SetData(const NkCamera2DData& d) noexcept { mData = d; mDirty = true; }

            private:
                NkCamera2DData mData;

                void RebuildImpl() const noexcept override;
        };

    } // namespace renderer
} // namespace nkentseu
