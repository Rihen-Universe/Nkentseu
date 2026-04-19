// =============================================================================
// NkCamera.cpp — Implémentation des caméras 2D et 3D
// =============================================================================
#include "NKRenderer/Core/NkCamera.h"
#include <cmath>
#include <algorithm>

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // NkCamera3D
        // =============================================================================
        NkMat4f NkCamera3D::Projection() const {
            if (type == NkProjType::NK_PERSPECTIVE) {
                return NkMat4f::Perspective(NkAngle(fovDeg), aspect, nearZ, farZ);
            }
            return NkMat4f::Orthogonal(
                NkVec2f(orthoLeft,  orthoBottom),
                NkVec2f(orthoRight, orthoTop),
                orthoNear, orthoFar, depthZeroToOne);
        }

        NkVec3f NkCamera3D::Forward() const {
            NkVec3f d = target - position;
            float l = sqrtf(d.x*d.x + d.y*d.y + d.z*d.z);
            return l > 1e-6f ? NkVec3f{d.x/l, d.y/l, d.z/l} : NkVec3f{0,0,-1};
        }

        NkVec3f NkCamera3D::Right() const {
            NkVec3f f = Forward();
            NkVec3f r{
                f.y*up.z - f.z*up.y,
                f.z*up.x - f.x*up.z,
                f.x*up.y - f.y*up.x
            };
            float l = sqrtf(r.x*r.x + r.y*r.y + r.z*r.z);
            return l > 1e-6f ? NkVec3f{r.x/l, r.y/l, r.z/l} : NkVec3f{1,0,0};
        }

        void NkCamera3D::Orbit(const NkVec3f& center,
                                 float32 yawDeg, float32 pitchDeg, float32 dist) {
            pitchDeg = std::max(-89.f, std::min(89.f, pitchDeg));
            const float y = yawDeg   * (3.14159265f / 180.f);
            const float p = pitchDeg * (3.14159265f / 180.f);
            position.x = center.x + dist * cosf(p) * sinf(y);
            position.y = center.y + dist * sinf(p);
            position.z = center.z + dist * cosf(p) * cosf(y);
            target = center;
            up = fabsf(sinf(p)) > 0.98f
                ? NkVec3f{cosf(y), 0, sinf(y)}
                : NkVec3f{0, 1, 0};
        }

        void NkCamera3D::Pan(float32 dx, float32 dy) {
            NkVec3f r = Right();
            NkVec3f u = up;
            position.x += r.x*dx + u.x*dy; target.x += r.x*dx + u.x*dy;
            position.y += r.y*dx + u.y*dy; target.y += r.y*dx + u.y*dy;
            position.z += r.z*dx + u.z*dy; target.z += r.z*dx + u.z*dy;
        }

        void NkCamera3D::Zoom(float32 delta) {
            NkVec3f f = Forward();
            position.x += f.x*delta;
            position.y += f.y*delta;
            position.z += f.z*delta;
        }

        NkCamera3D::Ray NkCamera3D::ScreenRay(float32 ndcX, float32 ndcY) const {
            Ray r;
            r.origin = position;
            NkVec3f f  = Forward();
            NkVec3f ri = Right();
            float tanH = tanf(fovDeg * (3.14159265f / 180.f) * .5f);
            r.direction.x = f.x + ri.x*ndcX*tanH*aspect + up.x*(-ndcY)*tanH;
            r.direction.y = f.y + ri.y*ndcX*tanH*aspect + up.y*(-ndcY)*tanH;
            r.direction.z = f.z + ri.z*ndcX*tanH*aspect + up.z*(-ndcY)*tanH;
            float l = sqrtf(r.direction.x*r.direction.x +
                            r.direction.y*r.direction.y +
                            r.direction.z*r.direction.z);
            if (l > 1e-6f) {
                r.direction.x /= l;
                r.direction.y /= l;
                r.direction.z /= l;
            }
            return r;
        }

        NkCamera3D NkCamera3D::ShadowLight(const NkVec3f& dir,
                                              const NkVec3f& center,
                                              float32 radius, bool z01) {
            NkVec3f nd = dir;
            float l = sqrtf(nd.x*nd.x + nd.y*nd.y + nd.z*nd.z);
            if (l > 1e-6f) { nd.x/=l; nd.y/=l; nd.z/=l; }
            NkCamera3D c = Orthographic(-radius, radius, -radius, radius,
                                          1.f, radius*3.f, z01);
            c.position = {
                center.x - nd.x*radius*2,
                center.y - nd.y*radius*2,
                center.z - nd.z*radius*2
            };
            c.target = center;
            c.up = fabsf(nd.y) > .9f ? NkVec3f{1,0,0} : NkVec3f{0,1,0};
            return c;
        }

        // =============================================================================
        // NkCamera2D
        // =============================================================================
        NkMat4f NkCamera2D::View() const {
            NkMat4f T = NkMat4f::Translation({-position.x, -position.y, 0});
            NkMat4f R = NkMat4f::RotationZ(NkAngle(-rotation));
            return R * T;
        }

        NkMat4f NkCamera2D::Projection() const {
            const float hw = (width  * .5f) / zoom;
            const float hh = (height * .5f) / zoom;
            return NkMat4f::Orthogonal(
                NkVec2f(-hw, yFlip ?  -hh :  hh),
                NkVec2f( hw, yFlip ?   hh : -hh),
                -1.f, 1.f, false);
        }

        NkVec2f NkCamera2D::ScreenToWorld(float32 sx, float32 sy) const {
            const float ndcX = (sx / (width  * .5f) - 1.f) / zoom;
            const float ndcY = yFlip
                ? (1.f - sy / (height * .5f)) / zoom
                : (sy / (height * .5f) - 1.f) / zoom;
            return {position.x + ndcX * (width * .5f),
                    position.y + ndcY * (height * .5f)};
        }

        NkVec2f NkCamera2D::WorldToScreen(float32 wx, float32 wy) const {
            const float rx = (wx - position.x) * zoom;
            const float ry = (wy - position.y) * zoom;
            const float sx = (rx / (width  * .5f) + 1.f) * (width  * .5f);
            const float sy = yFlip
                ? (1.f - ry / (height * .5f)) * (height * .5f)
                : (ry / (height * .5f) + 1.f) * (height * .5f);
            return {sx, sy};
        }

    } // namespace renderer
} // namespace nkentseu
