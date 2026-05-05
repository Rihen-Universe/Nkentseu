#include "NkEditorCamera.h"
#include "NKMath/NKMath.h"
#include <math.h>
#include <string.h>

using namespace nkentseu::math;

namespace nkentseu {
    namespace Unkeny {

        void NkEditorCamera::Update(float32 dt, bool hovered) noexcept {
            if (mode == NkEditorCameraMode::Orbit) UpdateOrbit(dt, hovered);
            else                                   UpdateFly  (dt, hovered);
            RecalcMatrices();
        }

        void NkEditorCamera::UpdateOrbit(float32 /*dt*/, bool /*hovered*/) noexcept {
            // Les entrées sont appliquées directement dans OnMouseMove/OnMouseScroll
            orbitPitch = NkClamp(orbitPitch, -89.f, 89.f);
            orbitDist  = NkMax(orbitDist, 0.1f);

            float32 py = NkToRadians(orbitPitch);
            float32 yy = NkToRadians(orbitYaw);

            float32 cx = NkCos(py) * NkSin(yy);
            float32 cy = NkSin(py);
            float32 cz = NkCos(py) * NkCos(yy);

            position = target + NkVec3f(cx, cy, cz) * orbitDist;
        }

        void NkEditorCamera::UpdateFly(float32 dt, bool hovered) noexcept {
            if (!hovered && !mRightDown) return;

            float32 speed = flySpeed * dt;
            float32 pyRad = NkToRadians(flyPitch);
            float32 yyRad = NkToRadians(flyYaw);

            NkVec3f fwd = {
                NkCos(pyRad) * NkSin(yyRad),
                NkSin(pyRad),
               -NkCos(pyRad) * NkCos(yyRad)
            };
            NkVec3f right = NkVec3f(NkCos(yyRad), 0.f, NkSin(yyRad));
            NkVec3f up    = {0.f, 1.f, 0.f};

            if (mKeys[static_cast<int>(NkKey::NK_W)])  flyPos = flyPos + fwd   *  speed;
            if (mKeys[static_cast<int>(NkKey::NK_S)])  flyPos = flyPos + fwd   * -speed;
            if (mKeys[static_cast<int>(NkKey::NK_A)])  flyPos = flyPos + right * -speed;
            if (mKeys[static_cast<int>(NkKey::NK_D)])  flyPos = flyPos + right *  speed;
            if (mKeys[static_cast<int>(NkKey::NK_Q)])  flyPos = flyPos + up    * -speed;
            if (mKeys[static_cast<int>(NkKey::NK_E)])  flyPos = flyPos + up    *  speed;

            position = flyPos;
        }

        void NkEditorCamera::OnMouseMove(float32 dx, float32 dy,
                                         bool altDown, bool rightDown) noexcept {
            mAltDown    = altDown;
            mRightDown  = rightDown;

            if (mode == NkEditorCameraMode::Orbit) {
                if (altDown && rightDown) {
                    // Orbit
                    orbitYaw   += dx * 0.4f;
                    orbitPitch -= dy * 0.4f;
                } else if (altDown) {
                    // Pan
                    float32 py = NkToRadians(orbitPitch);
                    float32 yy = NkToRadians(orbitYaw);
                    NkVec3f right = {NkCos(yy), 0.f, NkSin(yy)};
                    NkVec3f up    = {-NkSin(py)*NkSin(yy), NkCos(py), -NkSin(py)*NkCos(yy)};
                    float32 pan   = orbitDist * 0.001f;
                    target = target - right * (dx * pan) + up * (dy * pan);
                }
            } else {
                if (rightDown) {
                    flyYaw   += dx * 0.25f;
                    flyPitch -= dy * 0.25f;
                    flyPitch  = NkClamp(flyPitch, -89.f, 89.f);
                }
            }
        }

        void NkEditorCamera::OnMouseScroll(float32 delta) noexcept {
            if (mode == NkEditorCameraMode::Orbit) {
                orbitDist -= delta * orbitDist * 0.1f;
                orbitDist  = NkMax(orbitDist, 0.1f);
            } else {
                flySpeed = NkClamp(flySpeed + delta * 0.5f, 0.1f, 100.f);
            }
        }

        void NkEditorCamera::OnKeyDown(NkKey key) noexcept {
            int k = static_cast<int>(key);
            if (k >= 0 && k < 512) mKeys[k] = true;
        }

        void NkEditorCamera::OnKeyUp(NkKey key) noexcept {
            int k = static_cast<int>(key);
            if (k >= 0 && k < 512) mKeys[k] = false;
        }

        void NkEditorCamera::FocusOn(const NkVec3f& point, float32 dist) noexcept {
            target    = point;
            if (dist > 0.f) orbitDist = dist;
        }

        void NkEditorCamera::RecalcMatrices() noexcept {
            NkVec3f up = {0.f, 1.f, 0.f};
            // Éviter gimbal lock si exactement vertical
            if (NkAbs(NkDot(position - target, up)) > 0.999f * orbitDist)
                up = {0.f, 0.f, 1.f};
            viewMatrix    = NkMat4f::LookAt(position, target, up);
            projMatrix    = NkMat4f::Perspective(NkAngle(fovDeg), aspect, nearClip, farClip);
            viewProjMatrix= projMatrix * viewMatrix;
        }

        NkEditorCamera::Ray NkEditorCamera::ScreenRay(float32 vpX, float32 vpY,
                                                       float32 vpW, float32 vpH) const noexcept {
            // NDC
            float32 ndcX = (2.f * vpX / vpW) - 1.f;
            float32 ndcY = 1.f - (2.f * vpY / vpH);

            NkMat4f invVP = NkInverse(viewProjMatrix);
            NkVec4f nearH = invVP * NkVec4f(ndcX, ndcY, -1.f, 1.f);
            NkVec4f farH  = invVP * NkVec4f(ndcX, ndcY,  1.f, 1.f);

            NkVec3f nearP = NkVec3f(nearH.x, nearH.y, nearH.z) * (1.f / nearH.w);
            NkVec3f farP  = NkVec3f(farH.x,  farH.y,  farH.z)  * (1.f / farH.w);

            Ray r;
            r.origin = nearP;
            r.dir    = NkNormalize(farP - nearP);
            return r;
        }

    } // namespace Unkeny
} // namespace nkentseu
