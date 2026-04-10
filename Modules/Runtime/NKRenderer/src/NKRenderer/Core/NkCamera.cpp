#include "NkCamera.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu
{
    namespace renderer
    {
        // ── ortho column-major (convention OpenGL / GLSL) ─────────────────────────
        // Projection NDC [-1,1] → ajustée pour [0,1] si nécessaire via flip
        static NkMat4 OrthoGL(float32 l, float32 r, float32 b, float32 t,
                            float32 n, float32 f) noexcept {
            NkMat4 m;
            m.col[0] = {2.f/(r-l),    0,          0,          0};
            m.col[1] = {0,            2.f/(t-b),  0,          0};
            m.col[2] = {0,            0,          -2.f/(f-n), 0};
            m.col[3] = {-(r+l)/(r-l),-(t+b)/(t-b),-(f+n)/(f-n), 1};
            return m;
        }

        // ── ortho NDC [0,1] (Vulkan / DX) ──────────────────────────────────────
        static NkMat4 OrthoVK(float32 l, float32 r, float32 b, float32 t,
                            float32 n, float32 f) noexcept {
            NkMat4 m;
            m.col[0] = {2.f/(r-l),    0,          0,         0};
            m.col[1] = {0,            2.f/(t-b),  0,         0};
            m.col[2] = {0,            0,          1.f/(f-n), 0};
            m.col[3] = {-(r+l)/(r-l),-(t+b)/(t-b),  -n/(f-n),  1};
            return m;
        }

        // ── perspective NDC [0,1] (Vulkan / DX, reverse-Z option) ─────────────────
        static NkMat4 PerspVK(float32 fovY, float32 aspect, float32 n, float32 f) noexcept {
            float32 tanHalf = math::NkTan(math::DEG_TO_RAD_F * (fovY) * 0.5f);
            NkMat4 m;
            m.col[0] = {1.f/(aspect*tanHalf), 0,           0,           0};
            m.col[1] = {0,                     1.f/tanHalf, 0,           0};
            m.col[2] = {0,                     0,           f/(f-n),     1};
            m.col[3] = {0,                     0,          -f*n/(f-n),   0};
            return m;
        }

        // ── perspective NDC [-1,1] (OpenGL) ────────────────────────────────────
        static NkMat4 PerspGL(float32 fovY, float32 aspect, float32 n, float32 f) noexcept {
            float32 tanHalf = math::NkTan(math::DEG_TO_RAD_F * (fovY) * 0.5f);
            NkMat4 m;
            m.col[0] = {1.f/(aspect*tanHalf), 0,           0,              0};
            m.col[1] = {0,                     1.f/tanHalf, 0,              0};
            m.col[2] = {0,                     0,          -(f+n)/(f-n),   -1};
            m.col[3] = {0,                     0,          -2.f*f*n/(f-n),  0};
            return m;
        }
        // ── LookAt column-major (right-hand) ──────────────────────────────────────
        static NkMat4 LookAtRH(NkVec3 eye, NkVec3 center, NkVec3 up) noexcept {
            NkVec3 f = (center - eye).Normalized();
            NkVec3 s = f.Cross(up).Normalized();
            NkVec3 u = s.Cross(f);

            NkMat4 m;
            m.col[0] = { s.x,  u.x, -f.x, 0};
            m.col[1] = { s.y,  u.y, -f.y, 0};
            m.col[2] = { s.z,  u.z, -f.z, 0};
            m.col[3] = {-s.Dot(eye), -u.Dot(eye), f.Dot(eye), 1};
            return NkMat4::LookAt(eye, center, up);
        }

        // =========================================================================
        // NkCamera2D
        // =========================================================================

        NkCamera2D::NkCamera2D(float32 width, float32 height,
                                float32 nearZ, float32 farZ) noexcept {
            SetOrtho(width, height, nearZ, farZ);
        }

        void NkCamera2D::SetOrtho(float32 width, float32 height,
                                float32 nearZ, float32 farZ) noexcept {
            mWidth  = width;
            mHeight = height;
            mNearZ  = nearZ;
            mFarZ   = farZ;
            mDirty  = true;
        }

        // NkMat4 NkCamera2D::GetViewProjection() const noexcept {
        //     RebuildIfDirty();
        //     return mProjection * mView;
        // }

        NkVec2 NkCamera2D::ScreenToWorld(NkVec2 sp) const noexcept {
            // Normalise → NDC → inverse VP
            float32 ndcX =  2.f * sp.x / mWidth  - 1.f;
            float32 ndcY = -2.f * sp.y / mHeight + 1.f;
            NkMat4 invVP = GetViewProjectionMatrix(10).Inverse();
            math::NkVec4f v = invVP * math::NkVec4f{ndcX, ndcY, 0.f, 1.f};
            return {v.x, v.y};
        }

        NkVec2 NkCamera2D::WorldToScreen(NkVec2 wp) const noexcept {
            math::NkVec4f clip = GetViewProjectionMatrix(10) * math::NkVec4f{wp.x, wp.y, 0.f, 1.f};
            return {(clip.x * 0.5f + 0.5f) * mWidth, (1.f - clip.y * 0.5f - 0.5f) * mHeight};
        }

        void NkCamera2D::RebuildIfDirty() const noexcept {
            if (!mDirty) return;
            mDirty = false;

            // Projection ortho : Y vers le bas (convention UI/2D standard)
            mProjection = OrthoVK(0.f, mWidth * (1.f/mZoom), mHeight * (1.f/mZoom), 0.f, mNearZ, mFarZ);

            // View = translate + rotate autour de Z
            float32 rad = math::DEG_TO_RAD * (mRotation);
            float32 cosR = math::NkCos(rad);
            float32 sinR = math::NkSin(rad);
            NkMat4 rot;
            rot.col[0] = { cosR, sinR, 0, 0};
            rot.col[1] = {-sinR, cosR, 0, 0};
            rot.col[2] = {    0,    0, 1, 0};
            rot.col[3] = {    0,    0, 0, 1};
            NkMat4 trans;
            trans.col[3] = {-mPosition.x, -mPosition.y, 0, 1};
            mView = rot * trans;
        }

        // =========================================================================
        // NkCamera3D
        // =========================================================================

        NkCamera3D::NkCamera3D(float32 fovDeg, float32 aspect,
                                float32 nearZ, float32 farZ) noexcept {
            SetPerspective(fovDeg, aspect, nearZ, farZ);
        }

        void NkCamera3D::SetPerspective(float32 fovDeg, float32 aspect,
                                        float32 nearZ, float32 farZ) noexcept {
            mIsPerspective = true;
            mFovDeg = fovDeg; mAspect = aspect;
            mNearZ  = nearZ;  mFarZ   = farZ;
            mDirty  = true;
        }

        void NkCamera3D::SetOrthographic(float32 l, float32 r, float32 b, float32 t,
                                        float32 nearZ, float32 farZ) noexcept {
            mIsPerspective = false;
            mOrthoLeft = l; mOrthoRight = r;
            mOrthoBottom = b; mOrthoTop = t;
            mNearZ = nearZ; mFarZ = farZ;
            mDirty = true;
        }

        void NkCamera3D::LookAt(NkVec3 eye, NkVec3 target, NkVec3 up) noexcept {
            mPosition = eye; mTarget = target; mUp = up;
            mDirty = true;
        }

        NkVec3 NkCamera3D::GetForward() const {
            return (mTarget - mPosition).Normalized();
        }

        NkVec3 NkCamera3D::GetRight() const {
            return GetForward().Cross(mUp).Normalized();
        }

        // NkMat4 NkCamera3D::GetViewProjection() const noexcept {
        //     RebuildIfDirty();
        //     return mProjection * mView;
        // }

        void NkCamera3D::RebuildIfDirty() const noexcept {
            if (!mDirty) return;
            mDirty = false;
            RebuildPerspective();
            RebuildView();
        }

        void NkCamera3D::RebuildPerspective() const noexcept {
            bool gl = (mProjSpace == NkProjectionSpace::NK_NDC_NEG_ONE);
            if (mIsPerspective) {
                mProjection = gl ? PerspGL(mFovDeg, mAspect, mNearZ, mFarZ)
                                : PerspVK(mFovDeg, mAspect, mNearZ, mFarZ);
            } else {
                mProjection = gl ? OrthoGL(mOrthoLeft, mOrthoRight, mOrthoBottom, mOrthoTop, mNearZ, mFarZ)
                                : OrthoVK(mOrthoLeft, mOrthoRight, mOrthoBottom, mOrthoTop, mNearZ, mFarZ);
            }
        }

        void NkCamera3D::RebuildView() const noexcept {
            mView = LookAtRH(mPosition, mTarget, mUp);
        }
    }
}