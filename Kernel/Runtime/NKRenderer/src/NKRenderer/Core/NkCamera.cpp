// =============================================================================
// NkCamera.cpp  — NKRenderer v5.0
// Implementations de NkCamera3D et NkCamera2D (derivees de NkCamera).
// =============================================================================
#include "NkCamera.h"
#include "NKMath/NKMath.h"
#include <cstring>
#include <cmath>

namespace nkentseu {
    namespace renderer {

        // =====================================================================
        // NkCamera3D
        // =====================================================================
        NkCamera3D::NkCamera3D(const NkCamera3DData& data) : mData(data) { mDirty = true; }

        void NkCamera3D::SetPosition(NkVec3f p)        noexcept { mData.position  = p; mDirty=true; }
        void NkCamera3D::SetTarget  (NkVec3f t)        noexcept { mData.target    = t; mDirty=true; }
        void NkCamera3D::SetUp      (NkVec3f u)        noexcept { mData.up        = u; mDirty=true; }
        void NkCamera3D::SetFOV     (float32 fov)      noexcept { mData.fovY      = fov; mDirty=true; }
        void NkCamera3D::SetAspect  (float32 a)        noexcept { mData.aspect    = a; mDirty=true; }
        void NkCamera3D::SetAspect  (uint32 w, uint32 h) noexcept {
            mData.aspect = (h > 0) ? (float32)w / (float32)h : 1.f;
            mDirty = true;
        }
        void NkCamera3D::SetNearFar (float32 n, float32 f) noexcept {
            mData.nearPlane = n; mData.farPlane = f; mDirty = true;
        }
        void NkCamera3D::SetOrtho   (bool ortho, float32 size) noexcept {
            mData.ortho = ortho; mData.orthoSize = size; mDirty=true;
        }

        void NkCamera3D::RebuildImpl() const noexcept {
            // ── View matrix (LookAt) ─────────────────────────────────────────
            NkVec3f fwd = {
                mData.target.x - mData.position.x,
                mData.target.y - mData.position.y,
                mData.target.z - mData.position.z
            };
            float32 flen = math::NkSqrt(fwd.x*fwd.x + fwd.y*fwd.y + fwd.z*fwd.z);
            if (flen > 1e-6f) { fwd.x/=flen; fwd.y/=flen; fwd.z/=flen; }

            NkVec3f up = mData.up;
            NkVec3f right = {
                fwd.y*up.z - fwd.z*up.y,
                fwd.z*up.x - fwd.x*up.z,
                fwd.x*up.y - fwd.y*up.x
            };
            float32 rlen = math::NkSqrt(right.x*right.x + right.y*right.y + right.z*right.z);
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

            // ── Projection ───────────────────────────────────────────────────
            if (!mData.ortho) {
                float32 tanHalf = math::NkTan(mData.fovY * 0.5f * 3.14159265f / 180.f);
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
            BuildFrustum();
        }

        void NkCamera3D::BuildFrustum() const noexcept {
            // Gribb-Hartmann frustum extraction
            const NkMat4f& m = mViewProj;
            for (int i = 0; i < 4; i++) {
                mFrustumPlanes[0][i] = m[i][3] + m[i][0];   // left
                mFrustumPlanes[1][i] = m[i][3] - m[i][0];   // right
                mFrustumPlanes[2][i] = m[i][3] + m[i][1];   // bottom
                mFrustumPlanes[3][i] = m[i][3] - m[i][1];   // top
                mFrustumPlanes[4][i] = m[i][3] + m[i][2];   // near
                mFrustumPlanes[5][i] = m[i][3] - m[i][2];   // far
            }
            for (int p = 0; p < 6; p++) {
                float32 len = math::NkSqrt(
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
            (void)dt;
            Rebuild();
            NkCameraUBO ubo{};
            ubo.view        = mView;
            ubo.proj        = mProj;
            ubo.viewProj    = mViewProj;
            ubo.invView     = mView.Inverse();
            ubo.invProj     = mProj.Inverse();
            ubo.invViewProj = mViewProj.Inverse();
            ubo.position    = {mData.position.x, mData.position.y, mData.position.z, time};
            ubo.viewport    = {mData.aspect, 1.f,
                               1.f / (mData.aspect > 0 ? mData.aspect : 1.f), 1.f};
            float32 n = mData.nearPlane, f = mData.farPlane;
            ubo.depthParams = {n, f, (n - f) / (n * f), 1.f / n};
            for (int p = 0; p < 6; p++) {
                ubo.frustumPlanes[p] = {
                    mFrustumPlanes[p][0],
                    mFrustumPlanes[p][1],
                    mFrustumPlanes[p][2],
                    mFrustumPlanes[p][3]
                };
            }
            return ubo;
        }

        // =====================================================================
        // NkCamera2D
        // =====================================================================
        NkCamera2D::NkCamera2D(const NkCamera2DData& data) : mData(data) { mDirty = true; }

        void NkCamera2D::SetCenter  (NkVec2f c) noexcept { mData.center   = c; mDirty=true; }
        void NkCamera2D::SetZoom    (float32 z) noexcept { mData.zoom     = z; mDirty=true; }
        void NkCamera2D::SetRotation(float32 d) noexcept { mData.rotation = d; mDirty=true; }
        void NkCamera2D::SetViewport(uint32 w, uint32 h) noexcept {
            mData.width=w; mData.height=h; mDirty=true;
        }

        void NkCamera2D::RebuildImpl() const noexcept {
            // View en 2D = identity (la translation est dans l'ortho)
            mView = NkMat4f::Identity();

            // Projection orthographique
            float32 hw = (float32)mData.width  * 0.5f / mData.zoom;
            float32 hh = (float32)mData.height * 0.5f / mData.zoom;
            float32 l  = mData.center.x - hw;
            float32 r  = mData.center.x + hw;
            float32 b  = mData.center.y + hh;
            float32 t  = mData.center.y - hh;

            mProj = NkMat4f::Zero();
            mProj[0][0] =  2.f / (r - l);
            mProj[1][1] =  2.f / (t - b);
            mProj[2][2] = -1.f;
            mProj[3][0] = -(r+l)/(r-l);
            mProj[3][1] = -(t+b)/(t-b);
            mProj[3][3] =  1.f;

            mViewProj = mProj * mView;   // = mProj puisque view = identity
        }

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

        NkCameraUBO NkCamera2D::BuildUBO(float32 time, float32 dt) const noexcept {
            (void)dt;
            Rebuild();
            NkCameraUBO ubo{};
            ubo.view        = mView;
            ubo.proj        = mProj;
            ubo.viewProj    = mViewProj;
            ubo.invView     = mView.Inverse();
            ubo.invProj     = mProj.Inverse();
            ubo.invViewProj = mViewProj.Inverse();
            ubo.position    = {mData.center.x, mData.center.y, 0.f, time};
            ubo.viewport    = {(float32)mData.width, (float32)mData.height,
                               mData.width  > 0 ? 1.f / mData.width  : 0.f,
                               mData.height > 0 ? 1.f / mData.height : 0.f};
            ubo.depthParams = {-1.f, 1.f, 0.f, 0.f};   // 2D : pas de near/far signifiant
            // frustumPlanes laisses a zero (pas de culling shader-side en 2D)
            return ubo;
        }

    } // namespace renderer
} // namespace nkentseu
