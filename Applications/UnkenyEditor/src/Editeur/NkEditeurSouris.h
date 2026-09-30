//
// NkEditeurSouris.h
// =============================================================================
// Description :
//   Les boutons de la souris, des evenements de l'OS jusqu'a NKGui, sans qu'un
//   clic se perde. NKGui derive un clic de la TRANSITION de `mouseDown` d'une
//   trame a l'autre (NkGuiInput::NewFrame) : tout ce qui arrive entre deux
//   trames et se defait avant la suivante n'existe pas pour lui.
//
// Caracteristiques :
//   - Deux cas perdaient un clic, et chacun est retenu ici UNE trame :
//       1. appui ET relachement entre deux trames (clic sec, pave tactile) :
//          le relachement attend que la trame ait vu l'appui ;
//       2. relachement PUIS appui entre deux trames, alors que la trame
//          precedente avait vu le bouton enfonce : l'appui attend que la trame
//          ait vu le relachement. Mesure du 29/09 sur le bouton agrandir :
//          apres une restauration, le clic suivant « ne faisait rien ».
//   - Structure PURE, sans fenetre : le banc de l'editeur la rejoue contre un
//     vrai NkGuiInput (temoin e40).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURSOURIS_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURSOURIS_H__

#include "NKGui/Core/NkGuiInput.h"

namespace nkentseu {
	namespace editeur {

		/// Les trois boutons de NKGui : [0] gauche, [1] droit, [2] milieu.
		struct NkEditeurSouris {
				bool appuiNonVu[3] = {};	   ///< appui pose, pas encore vu par une trame
				bool relacheDiffere[3] = {};   ///< relachement retenu : son appui n'a pas ete vu
				bool appuiDiffere[3] = {};	   ///< appui retenu : le relachement d'avant n'a pas ete vu
				bool relacheApresAppui[3] = {}; ///< ... et son propre relachement, deja arrive

				/// Un appui de l'OS.
				void Appui(nkgui::NkGuiInput &in, int32 b) noexcept {
					if (b < 0 || b > 2) {
						return;
					}
					if (!in.mouseDown[b] && in.mousePrev[b]) {
						// Pose tel quel, `mouseDown` resterait vrai d'une trame a
						// l'autre : ni relachement, ni nouveau clic.
						appuiDiffere[b] = true;
						return;
					}
					in.mouseDown[b] = true;
					appuiNonVu[b] = true;
				}

				/// Un relachement de l'OS.
				void Relache(nkgui::NkGuiInput &in, int32 b) noexcept {
					if (b < 0 || b > 2) {
						return;
					}
					if (appuiDiffere[b]) {
						relacheApresAppui[b] = true;
					} else if (appuiNonVu[b]) {
						relacheDiffere[b] = true;
					} else {
						in.mouseDown[b] = false;
					}
				}

				/// A la FIN de la trame, apres que NKGui a lu l'etat : ce qui etait
				/// retenu part pour la suivante.
				void FinDeTrame(nkgui::NkGuiInput &in) noexcept {
					for (int32 b = 0; b < 3; ++b) {
						appuiNonVu[b] = false;
						if (relacheDiffere[b]) {
							relacheDiffere[b] = false;
							in.mouseDown[b] = false;
						}
						if (appuiDiffere[b]) {
							appuiDiffere[b] = false;
							in.mouseDown[b] = true;
							appuiNonVu[b] = true;
							if (relacheApresAppui[b]) {
								relacheApresAppui[b] = false;
								relacheDiffere[b] = true;
							}
						}
					}
				}
		};

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURSOURIS_H__
