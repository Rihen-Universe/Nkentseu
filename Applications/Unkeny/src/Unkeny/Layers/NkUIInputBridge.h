#pragma once

// =============================================================================
// NkUIInputBridge.h
// Convertit les événements Nkentseu (NkEvent) en NkUIInputState pour NKUI.
//
// Usage dans UILayer::OnAttach() :
//   mInputBridge.Attach();                     // souscrit via EventBus
//
// Usage dans UILayer::OnUIRender() :
//   mCtx.BeginFrame(mInputBridge.GetState(), dt);
//   mInputBridge.EndFrame();                   // reset des deltas
//
// Usage dans UILayer::OnDetach() :
//   mInputBridge.Detach();                     // désouscrit
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTextEvent.h"
#include "NKUI/NKUI.h"
#include "Nkentseu/Core/EventBus.h"

namespace nkentseu {
    namespace Unkeny {

        class NkUIInputBridge {
        public:
            NkUIInputBridge() = default;
            ~NkUIInputBridge() { Detach(); }

            // ── Cycle de vie ─────────────────────────────────────────────────
            void Attach() {
                if (mAttached) return;

                mIds[0] = EventBus::Subscribe<NkMouseMoveEvent>(
                    [this](NkMouseMoveEvent* e) -> bool {
                        mState.SetMousePos(static_cast<nk_float32>(e->GetX()),
                                           static_cast<nk_float32>(e->GetY()));
                        return false;
                    });

                mIds[1] = EventBus::Subscribe<NkMouseButtonPressEvent>(
                    [this](NkMouseButtonPressEvent* e) -> bool {
                        mState.SetMousePos(static_cast<nk_float32>(e->GetX()),
                                           static_cast<nk_float32>(e->GetY()));
                        int btn = ToUIButton(e->GetButton());
                        if (btn >= 0) mState.SetMouseButton(btn, true);
                        return false;
                    });

                mIds[2] = EventBus::Subscribe<NkMouseButtonReleaseEvent>(
                    [this](NkMouseButtonReleaseEvent* e) -> bool {
                        mState.SetMousePos(static_cast<nk_float32>(e->GetX()),
                                           static_cast<nk_float32>(e->GetY()));
                        int btn = ToUIButton(e->GetButton());
                        if (btn >= 0) mState.SetMouseButton(btn, false);
                        return false;
                    });

                mIds[3] = EventBus::Subscribe<NkMouseDoubleClickEvent>(
                    [this](NkMouseDoubleClickEvent* e) -> bool {
                        mState.SetMousePos(static_cast<nk_float32>(e->GetX()),
                                           static_cast<nk_float32>(e->GetY()));
                        int btn = ToUIButton(e->GetButton());
                        if (btn >= 0) {
                            mState.SetMouseButton(btn, true);
                            mState.mouseDblClick[btn] = true;
                        }
                        return false;
                    });

                mIds[4] = EventBus::Subscribe<NkMouseWheelVerticalEvent>(
                    [this](NkMouseWheelVerticalEvent* e) -> bool {
                        mState.AddMouseWheel(static_cast<nk_float32>(e->GetOffsetY()),
                                             static_cast<nk_float32>(e->GetOffsetX()));
                        return false;
                    });

                mIds[5] = EventBus::Subscribe<NkMouseWheelHorizontalEvent>(
                    [this](NkMouseWheelHorizontalEvent* e) -> bool {
                        mState.AddMouseWheel(0.f, static_cast<nk_float32>(e->GetOffsetX()));
                        return false;
                    });

                mIds[6] = EventBus::Subscribe<NkKeyPressEvent>(
                    [this](NkKeyPressEvent* e) -> bool {
                        NkKey k = e->GetKey();
                        if (k != NkKey::NK_UNKNOWN) mState.SetKey(k, true);
                        // Ctrl, Shift, Alt — mettre à jour les modifiers UI
                        mState.keyCtrl  = e->IsCtrl();
                        mState.keyShift = e->IsShift();
                        mState.keyAlt   = e->IsAlt();
                        return false; // ne consomme pas — le layer éditeur peut l'intercepter
                    });

                mIds[7] = EventBus::Subscribe<NkKeyReleaseEvent>(
                    [this](NkKeyReleaseEvent* e) -> bool {
                        NkKey k = e->GetKey();
                        if (k != NkKey::NK_UNKNOWN) mState.SetKey(k, false);
                        mState.keyCtrl  = e->IsCtrl();
                        mState.keyShift = e->IsShift();
                        mState.keyAlt   = e->IsAlt();
                        return false;
                    });

                mIds[8] = EventBus::Subscribe<NkTextInputEvent>(
                    [this](NkTextInputEvent* e) -> bool {
                        AddUtf8ToInput(e->GetUtf8());
                        return false;
                    });

                mAttached = true;
            }

            void Detach() {
                if (!mAttached) return;
                EventBus::Unsubscribe<NkMouseMoveEvent>(mIds[0]);
                EventBus::Unsubscribe<NkMouseButtonPressEvent>(mIds[1]);
                EventBus::Unsubscribe<NkMouseButtonReleaseEvent>(mIds[2]);
                EventBus::Unsubscribe<NkMouseDoubleClickEvent>(mIds[3]);
                EventBus::Unsubscribe<NkMouseWheelVerticalEvent>(mIds[4]);
                EventBus::Unsubscribe<NkMouseWheelHorizontalEvent>(mIds[5]);
                EventBus::Unsubscribe<NkKeyPressEvent>(mIds[6]);
                EventBus::Unsubscribe<NkKeyReleaseEvent>(mIds[7]);
                EventBus::Unsubscribe<NkTextInputEvent>(mIds[8]);
                mAttached = false;
            }

            // ── Accès ─────────────────────────────────────────────────────────
            nkui::NkUIInputState&       GetState()       { return mState; }
            const nkui::NkUIInputState& GetState() const { return mState; }

            // Appelé en début de frame AVANT BeginFrame
            void BeginFrame(float dt) {
                mState.BeginFrame();
                mState.dt = dt;
            }

            // Appelé en fin de frame (reset des deltas souris, etc.)
            void EndFrame() {
                // NkUIInputState::BeginFrame réinitialise les deltas en début de frame.
                // Rien à faire ici sauf si NKUI a un EndFrame côté input.
            }

        private:
            // Conversion NkMouseButton → index NKUI (0=gauche, 1=droit, 2=milieu)
            static int ToUIButton(NkMouseButton b) {
                switch (b) {
                    case NkMouseButton::NK_MB_LEFT:    return 0;
                    case NkMouseButton::NK_MB_RIGHT:   return 1;
                    case NkMouseButton::NK_MB_MIDDLE:  return 2;
                    case NkMouseButton::NK_MB_BACK:    return 3;
                    case NkMouseButton::NK_MB_FORWARD: return 4;
                    default: return -1;
                }
            }

            // Décodage UTF-8 → codepoints → NkUIInputState::AddInputChar
            void AddUtf8ToInput(const char* utf8) {
                if (!utf8) return;
                for (int i = 0; utf8[i]; ) {
                    const nk_uint8 c0 = static_cast<nk_uint8>(utf8[i]);
                    nk_uint32 cp = '?';
                    int n = 1;
                    if      (c0 < 0x80u)                        { cp = c0; n = 1; }
                    else if ((c0 & 0xE0u) == 0xC0u && utf8[i+1]) {
                        cp = ((c0 & 0x1Fu) << 6) |
                             (static_cast<nk_uint8>(utf8[i+1]) & 0x3Fu); n = 2;
                    }
                    else if ((c0 & 0xF0u) == 0xE0u && utf8[i+1] && utf8[i+2]) {
                        cp = ((c0 & 0x0Fu) << 12) |
                             ((static_cast<nk_uint8>(utf8[i+1]) & 0x3Fu) << 6) |
                             (static_cast<nk_uint8>(utf8[i+2]) & 0x3Fu); n = 3;
                    }
                    else if ((c0 & 0xF8u) == 0xF0u && utf8[i+1] && utf8[i+2] && utf8[i+3]) {
                        cp = ((c0 & 0x07u) << 18) |
                             ((static_cast<nk_uint8>(utf8[i+1]) & 0x3Fu) << 12) |
                             ((static_cast<nk_uint8>(utf8[i+2]) & 0x3Fu) << 6) |
                             (static_cast<nk_uint8>(utf8[i+3]) & 0x3Fu); n = 4;
                    }
                    mState.AddInputChar(cp);
                    i += n;
                }
            }

            nkui::NkUIInputState mState;
            NkEventHandlerId     mIds[9] = {};
            bool                 mAttached = false;
        };

    } // namespace Unkeny
} // namespace nkentseu
