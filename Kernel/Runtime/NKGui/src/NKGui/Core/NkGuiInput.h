#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiInput.h
// @Brief   État d'entrée NKGui par frame (alimenté par le backend NKEvent).
// @License Proprietary - Free to use and modify
// -----------------------------------------------------------------------------

#include "NKGui/NkGuiExport.h"
#include "NKGui/Core/NkGuiTypes.h"

namespace nkentseu {
    namespace nkgui {

        // Nombre de déclenchements « typematic » (rafale) franchis cette frame,
        // entre les durées d'appui t0 (frame précédente) et t1 (actuelle), pour un
        // délai initial `delay` puis une cadence `rate`. (Façon ImGui.)
        NKENTSEU_NKGUI_API_INLINE int32 NkGuiCalcTypematic(float32 t0, float32 t1,
                                                           float32 delay, float32 rate) noexcept {
            if (t1 == 0.f) return 0;                 // frame d'appui : géré par le clic/init
            if (t0 >= t1)  return 0;
            if (rate <= 0.f) return (t0 < delay && t1 >= delay) ? 1 : 0;
            const int32 c0 = (t0 - delay) < 0.f ? -1 : static_cast<int32>((t0 - delay) / rate);
            const int32 c1 = (t1 - delay) < 0.f ? -1 : static_cast<int32>((t1 - delay) / rate);
            return c1 - c0;
        }

        struct NKENTSEU_NKGUI_CLASS_EXPORT NkGuiInput {
            static constexpr int32 KeyCount = static_cast<int32>(NkGuiKey::Count);

            NkVec2  mousePos      { 0.f, 0.f };
            NkVec2  mouseDelta    { 0.f, 0.f };
            bool    mouseDown[3]  = {};   ///< 0=gauche 1=droit 2=milieu (brut, posé par l'app)
            bool    mousePrev[3]  = {};
            bool    mouseClicked[3] = {};
            bool    mouseReleased[3]= {};
            bool    mouseDoubleClicked[3] = {};
            bool    dblClickPending[3] = {};  ///< double-clic OS injecté par l'app (consommé en NewFrame)
            float32 mouseDownDur[3]= { -1.f, -1.f, -1.f };
            float32 clickTime[3]  = { -1.f, -1.f, -1.f };   ///< temps depuis le dernier clic (détection interne)
            float32 wheel         = 0.f;
            float32 wheelH        = 0.f;
            float32 dt            = 0.f;

            // Modificateurs (état enfoncé) — posés par l'app pour clic Ctrl/Shift/Alt.
            bool    ctrlDown      = false;
            bool    shiftDown     = false;
            bool    altDown       = false;   ///< modificateur de cascade (arbre à cases)

            // ── Saisie texte (codepoints tapés cette frame) ───────────────────
            uint32  chars[32]     = {};
            int32   charCount     = 0;

            // ── Touches d'édition : état ENFONCÉ (posé par l'app) + répétition ──
            bool    keyDown[KeyCount] = {};   ///< enfoncée (down) — posée par l'app
            bool    keyPrev[KeyCount] = {};   ///< état frame précédente (interne)
            bool    keyInit[KeyCount] = {};   ///< vient d'être enfoncée cette frame
            float32 keyDur [KeyCount] = {};   ///< durée d'appui (s ; -1 = relâchée)

            // Raccourcis d'edition demandes cette frame (poses par l'app sur Ctrl+C/X/V/A).
            bool    wantCopy      = false;
            bool    wantCut       = false;
            bool    wantPaste     = false;
            bool    wantSelectAll = false;

            void PushChar(uint32 cp) noexcept { if (charCount < 32) chars[charCount++] = cp; }
            void SetKey(NkGuiKey k, bool down) noexcept { keyDown[static_cast<int32>(k)] = down; }

            // Double-clic détecté par l'OS (NKEvent émet un événement dédié) : à
            // appeler par l'app pendant le pompage d'events. NewFrame le fusionne
            // avec la détection interne (certaines plateformes n'envoient pas un
            // 2e mouseDown — d'où l'injection explicite).
            void SetDoubleClick(int32 button) noexcept {
                if (button >= 0 && button < 3) dblClickPending[button] = true;
            }

            // Vrai à l'appui PUIS en répétition au maintien (flèches, backspace…).
            bool KeyPressedRepeat(NkGuiKey k, float32 delay = 0.30f, float32 rate = 0.04f) const noexcept {
                const int32 i = static_cast<int32>(k);
                if (keyInit[i]) return true;
                if (keyDown[i] && keyDur[i] >= 0.f)
                    return NkGuiCalcTypematic(keyDur[i] - dt, keyDur[i], delay, rate) > 0;
                return false;
            }
            // Vrai uniquement à l'appui (one-shot).
            bool KeyPressed(NkGuiKey k) const noexcept { return keyInit[static_cast<int32>(k)]; }

            // Transitions souris + durées d'appui (souris + touches). Appelé par
            // NkGuiContext::BeginFrame APRÈS que l'app ait posé l'état brut.
            void NewFrame() noexcept {
                for (int32 i = 0; i < 3; ++i) {
                    mouseClicked[i]  = mouseDown[i] && !mousePrev[i];
                    mouseReleased[i] = !mouseDown[i] && mousePrev[i];
                    if (mouseDown[i]) mouseDownDur[i] = (mousePrev[i] ? mouseDownDur[i] + dt : 0.f);
                    else              mouseDownDur[i] = -1.f;
                    // Double-clic : (a) détection interne (2e clic < 0.40 s) OU
                    // (b) injection OS via SetDoubleClick (consommée puis remise à 0).
                    if (mouseClicked[i]) {
                        mouseDoubleClicked[i] = (clickTime[i] >= 0.f && clickTime[i] < 0.40f);
                        clickTime[i] = 0.f;
                    } else {
                        mouseDoubleClicked[i] = false;
                        if (clickTime[i] >= 0.f) clickTime[i] += dt;
                    }
                    if (dblClickPending[i]) { mouseDoubleClicked[i] = true; dblClickPending[i] = false; }
                    mousePrev[i] = mouseDown[i];
                }
                for (int32 i = 0; i < KeyCount; ++i) {
                    keyInit[i] = keyDown[i] && !keyPrev[i];
                    if (keyDown[i]) keyDur[i] = (keyPrev[i] ? keyDur[i] + dt : 0.f);
                    else            keyDur[i] = -1.f;
                    keyPrev[i] = keyDown[i];
                }
            }

            // Texte consommé chaque frame (les touches restent en état down/up).
            void ClearPerFrameText() noexcept {
                charCount = 0;
                wantCopy = wantCut = wantPaste = wantSelectAll = false;
            }
        };

    } // namespace nkgui
} // namespace nkentseu
