#pragma once
// =============================================================================
// NkCameraController.h  — NKRenderer Core
//
// Controllers de camera independants de l'input/event system. Ils tiennent
// un etat (yaw, pitch, distance, target) et exposent une API pure (Rotate,
// Pan, Zoom, Move) que l'application appelle quand un input survient.
//
// La logique est SEPAREE des events car NKRenderer doit rester independant
// de NKEvent/NKWindow. C'est l'application (ou un wrapper dans Sandbox/Editor)
// qui hook les events et traduit les deltas vers Rotate/Pan/Zoom.
//
// Apply(cam) met a jour position/target d'un NkCamera3D existant ; pas de
// possession de la cam, juste de la modification.
//
// Usage :
//   NkOrbitCameraController3D orbit;
//   orbit.SetCenter({0,0.5f,0}, 9.f, 0.f, -0.2f);
//   // dans le code input :
//   if (leftDrag) orbit.Rotate(dx, dy);
//   if (wheel)    orbit.Zoom(wheelStep);
//   // dans Frame() :
//   orbit.Update(dt);
//   orbit.Apply(myCam);   // myCam = NkCamera3D
// =============================================================================
#include "NkCamera.h"
#include <cmath>

namespace nkentseu {
    namespace renderer {

        // =====================================================================
        // NkOrbitCameraController3D
        //
        // Camera orbit autour d'un point cible. Coordonnees spheriques (yaw,
        // pitch, distance). Clamp pitch a +-89 deg pour eviter gimbal lock.
        // =====================================================================
        class NkOrbitCameraController3D {
            public:
                // Reset l'etat complet (center + reset state pour Recenter()).
                void SetCenter(NkVec3f target, float32 distance,
                               float32 yaw, float32 pitch) {
                    mTarget   = target;
                    mDistance = distance;
                    mYaw      = yaw;
                    mPitch    = pitch;
                    mResetTarget   = target;
                    mResetDistance = distance;
                    mResetYaw      = yaw;
                    mResetPitch    = pitch;
                }

                // Reset a la derniere position passee a SetCenter.
                void Recenter() {
                    mTarget   = mResetTarget;
                    mDistance = mResetDistance;
                    mYaw      = mResetYaw;
                    mPitch    = mResetPitch;
                }

                // Rotation : dx en pixels (souris) ou unite arbitraire. Sensibilite
                // appliquee en interne via mRotateSpeed.
                void Rotate(float32 dx, float32 dy) {
                    mYaw   += dx * mRotateSpeed;
                    mPitch += dy * mRotateSpeed;
                    ClampPitch();
                }

                // Pan : translate target dans le plan camera (right * dx + up * dy).
                // Echelle proportionnelle a la distance (panSpeed * distance).
                void Pan(float32 dx, float32 dy) {
                    const NkVec3f f = ForwardDir();
                    const NkVec3f r = RightDir(f);
                    const NkVec3f u = CrossSafe(r, f);
                    const float32 scale = mPanSpeed * mDistance;
                    mTarget = mTarget - r * (dx * scale) + u * (dy * scale);
                }

                // Zoom : step positif = zoom-in (distance plus petite). Factor
                // multiplicatif pow(mZoomStep, step) pour echelle naturelle.
                void Zoom(float32 step) {
                    const float32 factor = powf(mZoomStep, step);
                    mDistance *= factor;
                    if (mDistance < mMinDistance) mDistance = mMinDistance;
                    if (mDistance > mMaxDistance) mDistance = mMaxDistance;
                }

                // Move target : delta en world-space direct (pas de scaling par dt,
                // l'appelant gere son scaling). Pratique pour pan-en-Y pur.
                void MoveTarget(NkVec3f delta) {
                    mTarget = mTarget + delta;
                }

                // Move target dans le repere camera-XZ (forward planaire + right).
                // dx = strafe droite, dz = forward, dy = elevation directe.
                void MoveCameraRelative(float32 dx, float32 dy, float32 dz) {
                    NkVec3f f = ForwardDir();
                    NkVec3f fXZ = f; fXZ.y = 0.f;
                    const float32 fXZlen = Length(fXZ);
                    if (fXZlen > 1e-6f) fXZ = fXZ * (1.f / fXZlen);
                    NkVec3f r = RightDir(f);
                    NkVec3f move = fXZ * dz + r * dx + NkVec3f{0, dy, 0};
                    mTarget = mTarget + move;
                }

                // Tick auto-orbit (continu en yaw). Active via SetAutoOrbit.
                void Update(float32 dt) {
                    if (mAutoOrbit) mYaw += mAutoOrbitSpeed * dt;
                }

                // Applique l'etat orbit (position calculee, target) au NkCamera3D
                // passe en parametre. Ne touche pas a fov/aspect/near/far.
                void Apply(NkCamera3D& cam) const {
                    cam.SetPosition(GetPosition());
                    cam.SetTarget(mTarget);
                }

                // Position camera calculee depuis yaw/pitch/distance/target.
                NkVec3f GetPosition() const {
                    const float32 cp = cosf(mPitch);
                    const float32 x  = mDistance * cp * cosf(mYaw);
                    const float32 y  = mDistance * sinf(mPitch);
                    const float32 z  = mDistance * cp * sinf(mYaw);
                    return { mTarget.x + x, mTarget.y + y, mTarget.z + z };
                }

                // Accessors etat
                NkVec3f GetTarget()   const { return mTarget;   }
                float32 GetDistance() const { return mDistance; }
                float32 GetYaw()      const { return mYaw;      }
                float32 GetPitch()    const { return mPitch;    }

                // Configuration sensibilites
                void SetRotateSpeed(float32 v)    { mRotateSpeed = v; }
                void SetPanSpeed(float32 v)       { mPanSpeed = v; }
                void SetZoomStep(float32 v)       { mZoomStep = v; }
                void SetAutoOrbit(bool on)        { mAutoOrbit = on; }
                void SetAutoOrbitSpeed(float32 v) { mAutoOrbitSpeed = v; }
                void SetMinDistance(float32 v)    { mMinDistance = v; }
                void SetMaxDistance(float32 v)    { mMaxDistance = v; }

                bool IsAutoOrbit() const { return mAutoOrbit; }

            private:
                void ClampPitch() {
                    const float32 kLimit = 1.553f; // ~89 deg
                    if (mPitch >  kLimit) mPitch =  kLimit;
                    if (mPitch < -kLimit) mPitch = -kLimit;
                }
                NkVec3f ForwardDir() const {
                    const float32 cp = cosf(mPitch);
                    return { -cp * cosf(mYaw), -sinf(mPitch), -cp * sinf(mYaw) };
                }
                NkVec3f RightDir(NkVec3f f) const {
                    NkVec3f up = {0,1,0};
                    NkVec3f r  = { f.z*up.y - f.y*up.z,
                                   f.x*up.z - f.z*up.x,
                                   f.y*up.x - f.x*up.y };
                    const float32 len = Length(r);
                    if (len > 1e-6f) r = r * (1.f / len);
                    return r;
                }
                static NkVec3f CrossSafe(NkVec3f a, NkVec3f b) {
                    NkVec3f c = { a.y*b.z - a.z*b.y,
                                  a.z*b.x - a.x*b.z,
                                  a.x*b.y - a.y*b.x };
                    const float32 len = Length(c);
                    if (len > 1e-6f) c = c * (1.f / len);
                    return c;
                }
                static float32 Length(NkVec3f v) {
                    return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
                }

                // Etat orbit
                NkVec3f mTarget   = {0, 0.5f, 0};
                float32 mDistance = 9.f;
                float32 mYaw      = 0.f;
                float32 mPitch    = -0.2f;

                // Etat reset (Recenter)
                NkVec3f mResetTarget   = {0, 0.5f, 0};
                float32 mResetDistance = 9.f;
                float32 mResetYaw      = 0.f;
                float32 mResetPitch    = -0.2f;

                // Mode
                bool mAutoOrbit = false;

                // Sensibilites
                float32 mRotateSpeed    = 0.005f;   // radians par unite de delta
                float32 mPanSpeed       = 0.0015f;  // unit par delta * distance
                float32 mZoomStep       = 0.88f;    // multiplicateur par tick
                float32 mAutoOrbitSpeed = 0.35f;    // radians par seconde
                float32 mMinDistance    = 0.5f;
                float32 mMaxDistance    = 200.f;
        };

    } // namespace renderer
} // namespace nkentseu
