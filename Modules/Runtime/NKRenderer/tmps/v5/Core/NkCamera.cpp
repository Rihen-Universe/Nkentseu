// =============================================================================
// NkCamera.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkCamera.h"
#include <cmath>
#include <cstring>

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // NkCamera3D
    // =========================================================================
    NkCamera3D::NkCamera3D(const NkCamera3DData& data) : mData(data), mDirty(true) {}

    void NkCamera3D::SetPosition(NkVec3f pos)   noexcept { mData.position  = pos;   mDirty=true; }
    void NkCamera3D::SetTarget  (NkVec3f target)noexcept { mData.target    = target;mDirty=true; }
    void NkCamera3D::SetUp      (NkVec3f up)    noexcept { mData.up        = up;    mDirty=true; }
    void NkCamera3D::SetFOV     (float32 fov)   noexcept { mData.fovY      = fov;   mDirty=true; }
    void NkCamera3D::SetAspect  (float32 a)     noexcept { mData.aspect    = a;     mDirty=true; }
    void NkCamera3D::SetAspect  (uint32 w, uint32 h) noexcept {
        mData.aspect = (h > 0) ? (float32)w / (float32)h : 1.f;
        mDirty = true;
    }
    void NkCamera3D::SetNearFar(float32 n, float32 f) noexcept {
        mData.nearPlane = n; mData.farPlane = f; mDirty=true;
    }
    void NkCamera3D::SetOrtho(bool ortho, float32 size) noexcept {
        mData.ortho = ortho; mData.orthoSize = size; mDirty=true;
    }

    void NkCamera3D::Rebuild() const noexcept {
        if (!mDirty) return;

        // View matrix (LookAt)
        NkVec3f fwd = {
            mData.target.x - mData.position.x,
            mData.target.y - mData.position.y,
            mData.target.z - mData.position.z
        };
        float32 flen = sqrtf(fwd.x*fwd.x + fwd.y*fwd.y + fwd.z*fwd.z);
        if (flen > 1e-6f) { fwd.x/=flen; fwd.y/=flen; fwd.z/=flen; }

        NkVec3f up = mData.up;
        NkVec3f right = {
            fwd.y*up.z - fwd.z*up.y,
            fwd.z*up.x - fwd.x*up.z,
            fwd.x*up.y - fwd.y*up.x
        };
        float32 rlen = sqrtf(right.x*right.x + right.y*right.y + right.z*right.z);
        if (rlen > 1e-6f) { right.x/=rlen; right.y/=rlen; right.z/=rlen; }

        NkVec3f u = {
            right.y*fwd.z - right.z*fwd.y,
            right.z*fwd.x - right.x*fwd.z,
            right.x*fwd.y - right.y*fwd.x
        };

        mView = NkMat4f::Identity();
        mView[0][0]=right.x; mView[1][0]=right.y; mView[2][0]=right.z;
        mView[0][1]=u.x;     mView[1][1]=u.y;     mView[2][1]=u.z;
        mView[0][2]=-fwd.x;  mView[1][2]=-fwd.y;  mView[2][2]=-fwd.z;
        mView[3][0]=-(right.x*mData.position.x + right.y*mData.position.y + right.z*mData.position.z);
        mView[3][1]=-(u.x*mData.position.x     + u.y*mData.position.y     + u.z*mData.position.z);
        mView[3][2]= (fwd.x*mData.position.x   + fwd.y*mData.position.y   + fwd.z*mData.position.z);

        // Projection
        if (!mData.ortho) {
            float32 tanHalf = tanf(mData.fovY * 0.5f * 3.14159265f / 180.f);
            float32 f = mData.farPlane, n = mData.nearPlane;
            mProj = NkMat4f::Zero();
            mProj[0][0] = 1.f / (mData.aspect * tanHalf);
            mProj[1][1] = 1.f / tanHalf;
            mProj[2][2] = -(f + n) / (f - n);
            mProj[2][3] = -1.f;
            mProj[3][2] = -(2.f * f * n) / (f - n);
        } else {
            float32 s = mData.orthoSize;
            float32 a = mData.aspect;
            float32 f = mData.farPlane, n = mData.nearPlane;
            mProj = NkMat4f::Zero();
            mProj[0][0] =  1.f / (s * a);
            mProj[1][1] =  1.f / s;
            mProj[2][2] = -2.f / (f - n);
            mProj[3][2] = -(f + n) / (f - n);
            mProj[3][3] = 1.f;
        }

        mViewProj = mProj * mView;
        mDirty = false;
        BuildFrustum();
    }

    void NkCamera3D::BuildFrustum() const noexcept {
        // Gribb-Hartmann frustum extraction
        const NkMat4f& m = mViewProj;
        for (int i = 0; i < 4; i++) {
            // left
            mFrustumPlanes[0][i] = m[i][3] + m[i][0];
            // right
            mFrustumPlanes[1][i] = m[i][3] - m[i][0];
            // bottom
            mFrustumPlanes[2][i] = m[i][3] + m[i][1];
            // top
            mFrustumPlanes[3][i] = m[i][3] - m[i][1];
            // near
            mFrustumPlanes[4][i] = m[i][3] + m[i][2];
            // far
            mFrustumPlanes[5][i] = m[i][3] - m[i][2];
        }
        // Normalize
        for (int p = 0; p < 6; p++) {
            float32 len = sqrtf(
                mFrustumPlanes[p][0]*mFrustumPlanes[p][0] +
                mFrustumPlanes[p][1]*mFrustumPlanes[p][1] +
                mFrustumPlanes[p][2]*mFrustumPlanes[p][2]);
            if (len > 1e-6f) {
                mFrustumPlanes[p][0]/=len;
                mFrustumPlanes[p][1]/=len;
                mFrustumPlanes[p][2]/=len;
                mFrustumPlanes[p][3]/=len;
            }
        }
    }

    const NkMat4f& NkCamera3D::GetView()     const noexcept { Rebuild(); return mView; }
    const NkMat4f& NkCamera3D::GetProj()     const noexcept { Rebuild(); return mProj; }
    const NkMat4f& NkCamera3D::GetViewProj() const noexcept { Rebuild(); return mViewProj; }

    NkVec3f NkCamera3D::GetForward() const noexcept {
        NkVec3f f = {
            mData.target.x-mData.position.x,
            mData.target.y-mData.position.y,
            mData.target.z-mData.position.z
        };
        float32 l = sqrtf(f.x*f.x+f.y*f.y+f.z*f.z);
        if (l>1e-6f) { f.x/=l; f.y/=l; f.z/=l; }
        return f;
    }

    NkVec3f NkCamera3D::GetRight() const noexcept {
        NkVec3f fwd = GetForward();
        NkVec3f up  = mData.up;
        NkVec3f r = {
            fwd.y*up.z-fwd.z*up.y,
            fwd.z*up.x-fwd.x*up.z,
            fwd.x*up.y-fwd.y*up.x
        };
        float32 l = sqrtf(r.x*r.x+r.y*r.y+r.z*r.z);
        if (l>1e-6f) { r.x/=l; r.y/=l; r.z/=l; }
        return r;
    }

    bool NkCamera3D::IsAABBVisible(const NkAABB& b) const noexcept {
        Rebuild();
        for (int p = 0; p < 6; p++) {
            float32 px = mFrustumPlanes[p][0]>=0.f ? b.max.x : b.min.x;
            float32 py = mFrustumPlanes[p][1]>=0.f ? b.max.y : b.min.y;
            float32 pz = mFrustumPlanes[p][2]>=0.f ? b.max.z : b.min.z;
            if (mFrustumPlanes[p][0]*px+mFrustumPlanes[p][1]*py+
                mFrustumPlanes[p][2]*pz+mFrustumPlanes[p][3] < 0.f)
                return false;
        }
        return true;
    }

    bool NkCamera3D::IsSphereVisible(NkVec3f c, float32 r) const noexcept {
        Rebuild();
        for (int p = 0; p < 6; p++) {
            float32 d = mFrustumPlanes[p][0]*c.x +
                        mFrustumPlanes[p][1]*c.y +
                        mFrustumPlanes[p][2]*c.z +
                        mFrustumPlanes[p][3];
            if (d < -r) return false;
        }
        return true;
    }

    NkCameraUBO NkCamera3D::BuildUBO(float32 time, float32 dt) const noexcept {
        Rebuild();
        NkCameraUBO ubo;
        ubo.view        = GetView();
        ubo.proj        = GetProj();
        ubo.viewProj    = GetViewProj();
        ubo.invViewProj = NkMat4f::Inverse(mViewProj);
        ubo.position    = {mData.position.x, mData.position.y, mData.position.z, mData.nearPlane};
        NkVec3f fwd = GetForward();
        ubo.direction   = {fwd.x, fwd.y, fwd.z, mData.farPlane};
        ubo.viewportSize = {(float32)mData.aspect * 100.f, 100.f};
        ubo.time        = time;
        ubo.deltaTime   = dt;
        return ubo;
    }

    // =========================================================================
    // NkCamera2D
    // =========================================================================
    NkCamera2D::NkCamera2D(const NkCamera2DData& data) : mData(data), mDirty(true) {}

    void NkCamera2D::SetCenter  (NkVec2f c) noexcept { mData.center  = c; mDirty=true; }
    void NkCamera2D::SetZoom    (float32 z) noexcept { mData.zoom    = z; mDirty=true; }
    void NkCamera2D::SetRotation(float32 d) noexcept { mData.rotation= d; mDirty=true; }
    void NkCamera2D::SetViewport(uint32 w, uint32 h) noexcept {
        mData.width=w; mData.height=h; mDirty=true;
    }

    void NkCamera2D::Rebuild() const noexcept {
        if (!mDirty) return;
        float32 hw = (float32)mData.width  * 0.5f / mData.zoom;
        float32 hh = (float32)mData.height * 0.5f / mData.zoom;
        float32 l  = mData.center.x - hw;
        float32 r  = mData.center.x + hw;
        float32 b  = mData.center.y + hh;
        float32 t  = mData.center.y - hh;

        mOrtho = NkMat4f::Zero();
        mOrtho[0][0] =  2.f / (r - l);
        mOrtho[1][1] =  2.f / (t - b);
        mOrtho[2][2] = -1.f;
        mOrtho[3][0] = -(r+l)/(r-l);
        mOrtho[3][1] = -(t+b)/(t-b);
        mOrtho[3][3] =  1.f;
        mDirty = false;
    }

    const NkMat4f& NkCamera2D::GetOrtho() const noexcept { Rebuild(); return mOrtho; }

    NkVec2f NkCamera2D::ScreenToWorld(NkVec2f s) const noexcept {
        float32 hw = (float32)mData.width  * 0.5f;
        float32 hh = (float32)mData.height * 0.5f;
        float32 wx = (s.x - hw) / mData.zoom + mData.center.x;
        float32 wy = (s.y - hh) / mData.zoom + mData.center.y;
        return {wx, wy};
    }

    NkVec2f NkCamera2D::WorldToScreen(NkVec2f w) const noexcept {
        float32 hw = (float32)mData.width  * 0.5f;
        float32 hh = (float32)mData.height * 0.5f;
        float32 sx = (w.x - mData.center.x) * mData.zoom + hw;
        float32 sy = (w.y - mData.center.y) * mData.zoom + hh;
        return {sx, sy};
    }

} // namespace renderer
} // namespace nkentseu
