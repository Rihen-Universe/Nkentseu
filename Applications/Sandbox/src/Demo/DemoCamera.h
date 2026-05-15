#pragma once
// =============================================================================
// DemoCamera.h — Adapter input -> NkOrbitCameraController3D pour les demos.
//
// Hook NkEvent souris/clavier et delegue les deltas au controller renderer.
// NKRenderer reste independant de NKEvent : c'est ce wrapper Sandbox-side qui
// fait le pont. Pour reutiliser dans une autre app, refaire un wrapper similaire
// (ou hooker directement depuis le code application).
//
// Mode orbit (defaut) :
//   - Souris LEFT drag       : Rotate(yaw/pitch)
//   - Souris MIDDLE/RIGHT    : Pan target
//   - Mouse wheel            : Zoom
//   - WASD / fleches         : MoveCameraRelative dans plan XZ
//   - Q/E ou PAGE_UP/DOWN    : MoveCameraRelative en Y
//   - T                      : toggle auto-orbit
//   - HOME                   : Recenter
//
// Usage :
//   demo::DemoCamera cam;
//   cam.Controller().SetCenter({0,0.5f,0}, 9.f, 0.f, -0.2f);
//   cam.Controller().SetAutoOrbit(true);
//   cam.InstallEvents();
//   // dans Frame() :
//   cam.Update(dt);
//   cam.Controller().Apply(myCamera3D);   // met a jour pos/target
// =============================================================================
#include "DemoCommon.h"
#include "NKRenderer/Core/NkCameraController.h"
#include "NKWindow/Core/NkWESystem.h"   // NkEvents()
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkEventSystem.h"

namespace nkentseu { namespace demo {

    class DemoCamera {
        public:
            renderer::NkOrbitCameraController3D&       Controller()       { return mCtrl; }
            const renderer::NkOrbitCameraController3D& Controller() const { return mCtrl; }

            void InstallEvents() {
                if (mInstalled) return;
                mInstalled = true;

                NkEvents().AddEventCallback<NkKeyPressEvent>([this](NkKeyPressEvent* e) {
                    OnKey(e->GetKey(), true);
                });
                NkEvents().AddEventCallback<NkKeyReleaseEvent>([this](NkKeyReleaseEvent* e) {
                    OnKey(e->GetKey(), false);
                });
                NkEvents().AddEventCallback<NkMouseButtonPressEvent>(
                    [this](NkMouseButtonPressEvent* e) {
                        if (e->IsLeft())   mLeftDown   = true;
                        if (e->IsMiddle()) mMiddleDown = true;
                        if (e->IsRight())  mRightDown  = true;
                    });
                NkEvents().AddEventCallback<NkMouseButtonReleaseEvent>(
                    [this](NkMouseButtonReleaseEvent* e) {
                        if (e->IsLeft())   mLeftDown   = false;
                        if (e->IsMiddle()) mMiddleDown = false;
                        if (e->IsRight())  mRightDown  = false;
                    });
                NkEvents().AddEventCallback<NkMouseMoveEvent>([this](NkMouseMoveEvent* e) {
                    OnMouseMove(e->GetDeltaX(), e->GetDeltaY());
                });
                NkEvents().AddEventCallback<NkMouseWheelVerticalEvent>(
                    [this](NkMouseWheelVerticalEvent* e) {
                        mCtrl.Zoom((float32)e->GetDeltaY());
                    });
            }

            void Update(float32 dt) {
                // WASD/fleches : pan camera-relatif sur plan XZ + Q/E en Y.
                float32 dx = 0.f, dy = 0.f, dz = 0.f;
                if (mKeyW || mKeyUp)     dz += 1.f;
                if (mKeyS || mKeyDown)   dz -= 1.f;
                if (mKeyD || mKeyRight)  dx += 1.f;
                if (mKeyA || mKeyLeft)   dx -= 1.f;
                if (mKeyE || mKeyPgUp)   dy += 1.f;
                if (mKeyQ || mKeyPgDn)   dy -= 1.f;
                const float32 inv = (dx*dx + dy*dy + dz*dz);
                if (inv > 1e-6f) {
                    const float32 norm = mKeyMoveSpeed * dt / sqrtf(inv);
                    mCtrl.MoveCameraRelative(dx * norm, dy * norm, dz * norm);
                }
                mCtrl.Update(dt);
            }

            void SetKeyMoveSpeed(float32 v) { mKeyMoveSpeed = v; }

        private:
            void OnKey(NkKey key, bool down) {
                switch (key) {
                    case NkKey::NK_W:        mKeyW     = down; break;
                    case NkKey::NK_A:        mKeyA     = down; break;
                    case NkKey::NK_S:        mKeyS     = down; break;
                    case NkKey::NK_D:        mKeyD     = down; break;
                    case NkKey::NK_Q:        mKeyQ     = down; break;
                    case NkKey::NK_E:        mKeyE     = down; break;
                    case NkKey::NK_UP:       mKeyUp    = down; break;
                    case NkKey::NK_DOWN:     mKeyDown  = down; break;
                    case NkKey::NK_LEFT:     mKeyLeft  = down; break;
                    case NkKey::NK_RIGHT:    mKeyRight = down; break;
                    case NkKey::NK_PAGE_UP:  mKeyPgUp  = down; break;
                    case NkKey::NK_PAGE_DOWN:mKeyPgDn  = down; break;
                    case NkKey::NK_T:        if (down) mCtrl.SetAutoOrbit(!mCtrl.IsAutoOrbit()); break;
                    case NkKey::NK_HOME:     if (down) mCtrl.Recenter(); break;
                    default: break;
                }
            }

            void OnMouseMove(int32 dx, int32 dy) {
                if (mLeftDown) {
                    mCtrl.Rotate((float32)dx, (float32)dy);
                } else if (mMiddleDown || mRightDown) {
                    mCtrl.Pan((float32)dx, (float32)dy);
                }
            }

            renderer::NkOrbitCameraController3D mCtrl;
            bool mInstalled  = false;
            bool mLeftDown   = false;
            bool mMiddleDown = false;
            bool mRightDown  = false;
            bool mKeyW=false, mKeyA=false, mKeyS=false, mKeyD=false;
            bool mKeyQ=false, mKeyE=false;
            bool mKeyUp=false, mKeyDown=false, mKeyLeft=false, mKeyRight=false;
            bool mKeyPgUp=false, mKeyPgDn=false;
            float32 mKeyMoveSpeed = 4.f;
    };

}} // namespace nkentseu::demo
