#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiInput.h
// @Brief   État d'entrée NKGui par frame (alimenté par le backend NKEvent).
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKGui/NkGuiExport.h"
#include "NKGui/Core/NkGuiTypes.h"

namespace nkentseu {
	namespace nkgui {

		// Nombre de déclenchements « typematic » (rafale) franchis cette frame,
		// entre les durées d'appui t0 (frame précédente) et t1 (actuelle), pour un
		// délai initial `delay` puis une cadence `rate`. (Façon ImGui.)
		NKENTSEU_NKGUI_API_INLINE int32 NkGuiCalcTypematic(float32 t0, float32 t1, float32 delay,
														   float32 rate) noexcept {
			if (t1 == 0.f)
				return 0; // frame d'appui : géré par le clic/init
			if (t0 >= t1)
				return 0;
			if (rate <= 0.f)
				return (t0 < delay && t1 >= delay) ? 1 : 0;
			const int32 c0 = (t0 - delay) < 0.f ? -1 : static_cast<int32>((t0 - delay) / rate);
			const int32 c1 = (t1 - delay) < 0.f ? -1 : static_cast<int32>((t1 - delay) / rate);
			return c1 - c0;
		}

		struct NKENTSEU_NKGUI_CLASS_EXPORT NkGuiInput {
				static constexpr int32 KeyCount = static_cast<int32>(NkGuiKey::Count);

				NkVec2 mousePos{0.f, 0.f};
				NkVec2 mouseDelta{0.f, 0.f};
				bool mouseDown[3] = {}; ///< 0=gauche 1=droit 2=milieu (brut, posé par l'app)
				bool mousePrev[3] = {};
				bool mouseClicked[3] = {};
				bool mouseReleased[3] = {};
				bool mouseDoubleClicked[3] = {};
				bool dblClickPending[3] = {}; ///< double-clic OS injecté par l'app (consommé en NewFrame)
				float32 mouseDownDur[3] = {-1.f, -1.f, -1.f};
				float32 clickTime[3] = {-1.f, -1.f, -1.f}; ///< temps depuis le dernier clic (détection interne)
				NkVec2 clickPos[3] = {}; ///< POSITION du dernier clic — cf. kDblMaxDist
				/// Écart maximal, en pixels, entre deux clics pour qu'ils forment un
				/// DOUBLE-CLIC. Sans ce rayon, la détection interne était purement
				/// TEMPORELLE : deux clics distants de moins de 0,40 s n'importe où sur
				/// l'écran passaient pour un double-clic, et chaque consommateur
				/// l'attribuait à la zone survolée à cet instant — donc à un widget que
				/// l'utilisateur n'avait jamais cliqué deux fois.
				static constexpr float32 kDblMaxDist = 6.f;
				float32 wheel = 0.f;
				float32 wheelH = 0.f;
				float32 wheelPending = 0.f; ///< molette differee (AddWheelDeferred), fusionnee au NewFrame
				float32 dt = 0.f;

				// Modificateurs (état enfoncé) — posés par l'app pour clic Ctrl/Shift/Alt.
				bool ctrlDown = false;
				bool shiftDown = false;
				bool altDown = false; ///< modificateur de cascade (arbre à cases)

				// ── Saisie texte (codepoints tapés cette frame) ───────────────────
				uint32 chars[32] = {};
				int32 charCount = 0;

				// ── Touches d'édition : état ENFONCÉ (posé par l'app) + répétition ──
				bool keyDown[KeyCount] = {};   ///< enfoncée (down) — posée par l'app
				bool keyPrev[KeyCount] = {};   ///< état frame précédente (interne)
				bool keyInit[KeyCount] = {};   ///< vient d'être enfoncée cette frame
				float32 keyDur[KeyCount] = {}; ///< durée d'appui (s ; -1 = relâchée)

				// Raccourcis d'edition demandes cette frame (poses par l'app sur Ctrl+C/X/V/A).
				bool wantCopy = false;
				bool wantCut = false;
				bool wantPaste = false;
				bool wantSelectAll = false;

				void PushChar(uint32 cp) noexcept {
					if (charCount < 32)
						chars[charCount++] = cp;
				}

				void SetKey(NkGuiKey k, bool down) noexcept {
					keyDown[static_cast<int32>(k)] = down;
				}

				// Double-clic détecté par l'OS (NKEvent émet un événement dédié) : à
				// appeler par l'app pendant le pompage d'events. NewFrame le fusionne
				// avec la détection interne (certaines plateformes n'envoient pas un
				// 2e mouseDown — d'où l'injection explicite).
				void SetDoubleClick(int32 button) noexcept {
					if (button >= 0 && button < 3)
						dblClickPending[button] = true;
				}

				// Molette injectée APRÈS les widgets (overlay, sonde pilotée) : la
				// molette brute est consommée par EndFrame (`wheel = 0`) — une
				// écriture directe en fin de frame serait donc effacée avant
				// d'être lue. Même patron que SetDoubleClick : différée, fusionnée
				// au NewFrame suivant. (Mesuré 2026-08-17 : sonde --dragdrop-test,
				// défilement jamais appliqué depuis l'overlay.)
				void AddWheelDeferred(float32 delta) noexcept {
					wheelPending += delta;
				}

				// Vrai à l'appui PUIS en répétition au maintien (flèches, backspace…).
				bool KeyPressedRepeat(NkGuiKey k, float32 delay = 0.30f, float32 rate = 0.04f) const noexcept {
					const int32 i = static_cast<int32>(k);
					if (keyInit[i])
						return true;
					if (keyDown[i] && keyDur[i] >= 0.f)
						return NkGuiCalcTypematic(keyDur[i] - dt, keyDur[i], delay, rate) > 0;
					return false;
				}

				// Vrai uniquement à l'appui (one-shot).
				bool KeyPressed(NkGuiKey k) const noexcept {
					return keyInit[static_cast<int32>(k)];
				}

				// Transitions souris + durées d'appui (souris + touches). Appelé par
				// NkGuiContext::BeginFrame APRÈS que l'app ait posé l'état brut.
				void NewFrame() noexcept {
					wheel += wheelPending; // injection differee (cf. AddWheelDeferred)
					wheelPending = 0.f;
					for (int32 i = 0; i < 3; ++i) {
						mouseClicked[i] = mouseDown[i] && !mousePrev[i];
						mouseReleased[i] = !mouseDown[i] && mousePrev[i];
						if (mouseDown[i])
							mouseDownDur[i] = (mousePrev[i] ? mouseDownDur[i] + dt : 0.f);
						else
							mouseDownDur[i] = -1.f;
						// Double-clic : (a) détection interne (2e clic < 0.40 s ET à
						// moins de kDblMaxDist du premier) OU (b) injection OS via
						// SetDoubleClick (consommée puis remise à 0).
						//
						// ⚠️ LE RAYON MANQUAIT, et il a coûté un défaut d'interface
						// (rapporté par Rodolf le 18/08/2026, NK3DModeler). La condition
						// était TEMPORELLE PURE : deux clics à moins de 0,40 s d'écart
						// formaient un double-clic *où qu'ils soient sur l'écran*. Or
						// personne ne consomme `mouseDoubleClicked` seul — tous le
						// croisent avec la zone survolée MAINTENANT
						// (`hov && mouseDoubleClicked[0]`, une vingtaine de sites). Deux
						// clics rapides sur DEUX widgets voisins produisaient donc un
						// « double-clic » attribué au second, qui n'avait été cliqué
						// qu'une fois. En sélection multiple (Ctrl+clic de carte en
						// carte) le cas n'est pas rare : c'est le geste normal, et il
						// ouvrait un éditeur sous les doigts de l'utilisateur.
						//
						// Mesure sur le journal de Rodolf : deux Ctrl+clics sur des
						// cartes différentes à **388 ms** d'écart — sous le seuil de
						// 400 ms, donc double-clic déclaré.
						if (mouseClicked[i]) {
							const float32 ddx = mousePos.x - clickPos[i].x;
							const float32 ddy = mousePos.y - clickPos[i].y;
							const bool memeEndroit =
								(ddx * ddx + ddy * ddy) <= (kDblMaxDist * kDblMaxDist);
							mouseDoubleClicked[i] =
								(clickTime[i] >= 0.f && clickTime[i] < 0.40f && memeEndroit);
							clickTime[i] = 0.f;
							clickPos[i] = mousePos;
						} else {
							mouseDoubleClicked[i] = false;
							if (clickTime[i] >= 0.f)
								clickTime[i] += dt;
						}
						if (dblClickPending[i]) {
							mouseDoubleClicked[i] = true;
							dblClickPending[i] = false;
						}
						mousePrev[i] = mouseDown[i];
					}
					for (int32 i = 0; i < KeyCount; ++i) {
						keyInit[i] = keyDown[i] && !keyPrev[i];
						if (keyDown[i])
							keyDur[i] = (keyPrev[i] ? keyDur[i] + dt : 0.f);
						else
							keyDur[i] = -1.f;
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
