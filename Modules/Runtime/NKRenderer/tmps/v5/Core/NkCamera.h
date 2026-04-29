#pragma once
// =============================================================================
// NkCamera.h  — NKRenderer v4.0
// Caméras 3D et 2D avec calcul de matrices et UBOs GPU.
// =============================================================================
#include "NkRendererTypes.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // UBO standard layout GPU (std140)
    // =========================================================================
    struct alignas(16) NkCameraUBO {
        NkMat4f view;
        NkMat4f proj;
        NkMat4f viewProj;
        NkMat4f invViewProj;
        NkVec4f position;      // xyz=pos, w=nearPlane
        NkVec4f direction;     // xyz=dir, w=farPlane
        NkVec2f viewportSize;
        float32 time;
        float32 deltaTime;
    };

    // =========================================================================
    // NkCamera3D
    // =========================================================================
    class NkCamera3D {
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

        // Matrices
        const NkMat4f& GetView()     const noexcept;
        const NkMat4f& GetProj()     const noexcept;
        const NkMat4f& GetViewProj() const noexcept;

        // Frustum culling
        bool IsAABBVisible(const NkAABB& aabb) const noexcept;
        bool IsSphereVisible(NkVec3f center, float32 radius) const noexcept;

        // UBO
        NkCameraUBO BuildUBO(float32 time = 0.f, float32 dt = 0.f) const noexcept;

        // Raw data
        const NkCamera3DData& GetData() const noexcept { return mData; }
        void SetData(const NkCamera3DData& d) noexcept { mData = d; mDirty = true; }

    private:
        NkCamera3DData mData;
        mutable NkMat4f mView;
        mutable NkMat4f mProj;
        mutable NkMat4f mViewProj;
        mutable bool    mDirty = true;
        mutable float32 mFrustumPlanes[6][4] = {};

        void Rebuild()       const noexcept;
        void BuildFrustum()  const noexcept;
    };

    // =========================================================================
    // NkCamera2D
    // =========================================================================
    class NkCamera2D {
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

        const NkMat4f& GetOrtho() const noexcept;

        // Screen → World coordinate conversion
        NkVec2f ScreenToWorld(NkVec2f screen) const noexcept;
        NkVec2f WorldToScreen(NkVec2f world)  const noexcept;

        const NkCamera2DData& GetData() const noexcept { return mData; }

    private:
        NkCamera2DData mData;
        mutable NkMat4f mOrtho;
        mutable bool    mDirty = true;
        void Rebuild() const noexcept;
    };

} // namespace renderer
} // namespace nkentseu
