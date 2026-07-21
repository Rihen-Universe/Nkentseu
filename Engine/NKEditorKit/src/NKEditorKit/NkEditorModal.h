#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorModal.h
// @Brief   Dialogue MODAL centre (titre + message + boutons), pour toute
//          confirmation bloquante (fermeture non enregistree, suppression...).
//          Contrairement a NkCtxMenu (NkEditorContextMenu.h - menu leger ancre
//          au clic, "modal-lite" : ne consomme le clic QUE quand la souris est
//          dedans), celui-ci s'enregistre dans la pile de popups REELLE de
//          NkGui (ctx.OpenPopup) -> ctx.popupDepth > 0 tant qu'il reste ouvert.
//          C'est deja le signal verifie par la quasi-totalite des points
//          d'interaction de NKCode (`ctx.popupDepth == 0` avant d'agir) : la
//          modalite devient donc GLOBALE (tous panneaux confondus, pas
//          seulement celui qui a ouvert le dialogue) sans aucune modification
//          ailleurs dans le code.
// @Author  Rihen
// @License Proprietary - Free to use and modify
// -----------------------------------------------------------------------------
#include "NKGui/NKGui.h"

namespace nkentseu {
	namespace editorkit {

		using namespace nkentseu;
		using namespace nkentseu::nkgui;

		// Etat d'un dialogue modal (un par confirmation possible dans l'appli —
		// PAS partage entre plusieurs confirmations distinctes, contrairement a
		// NkCtxMenu qui peut etre reutilise en sequence pour des menus differents).
		struct NkModal {
				bool open = false;
				NkGuiId id = NKGUI_ID_NONE;	   // assigne au premier open (adresse stable de *this)
				bool registered = false; // vrai des que ctx.OpenPopup(id) a ete appele pour CETTE ouverture
		};

		// Dessine + pilote un dialogue modal centre. `buttons[count-1]` est la
		// convention "Annuler" (retourne aussi quand l'utilisateur clique en
		// dehors du dialogue ou appuie sur Echap — comportement standard d'un
		// dialogue de confirmation). Retourne l'index du bouton clique cette
		// frame, -1 si rien encore.
		inline int32 NkModalDraw(NkGuiContext &ctx, NkModal &m, const char *title, const char *message,
								  const char *const *buttons, int32 count) {
			if (!m.open || count <= 0)
				return -1;
			if (m.id == NKGUI_ID_NONE) {
				// Id stable derive de l'adresse de *this (une NkModal = une confirmation
				// distincte) — pas de dependance a NkString/NkPrintf dans ce header moteur.
				const uint64 p = reinterpret_cast<uint64>(&m);
				m.id = (static_cast<NkGuiId>(p) ^ static_cast<NkGuiId>(p >> 16)) | 0x80000000u;
			}
			if (!m.registered) {
				// 1ere frame de cette ouverture : enregistrement REEL dans la pile de
				// popups NkGui -> ctx.popupDepth > 0 des cette frame, partout dans l'appli.
				ctx.OpenPopup(m.id);
				m.registered = true;
			} else if (!ctx.IsPopupOpen(m.id)) {
				// Etait ouvert, ne l'est plus : ferme tout seul (Echap/clic dehors, geres
				// par NkGuiContext::Update EN DEBUT DE FRAME via popupRects/popupAnchor de
				// LA FRAME PRECEDENTE) -> traite comme "Annuler" (dernier bouton).
				m.open = false;
				m.registered = false;
				return count - 1;
			}
			NkGuiDrawList &dl = ctx.dlOverlay;
			const NkGuiFont *font = ctx.font;
			const float32 lh = (font && font->Valid()) ? font->LineHeight() : 16.f;
			const float32 pad = 18.f, gap = 10.f, btnH = lh + 16.f;

			// Voile derriere le dialogue : signale visuellement le blocage global.
			dl.AddRectFilled({0.f, 0.f, static_cast<float32>(ctx.viewW), static_cast<float32>(ctx.viewH)},
							  NkColor{0, 0, 0, 120});

			// Largeur : la plus longue ligne (titre/message tronque a 3x la largeur
			// de base) borne le dialogue, jamais plus de 70% de la fenetre.
			float32 boxW = 380.f;
			if (font && font->Valid()) {
				if (title && *title)
					boxW = boxW > font->MeasureWidth(title) + pad * 2.f ? boxW : font->MeasureWidth(title) + pad * 2.f;
				if (message && *message)
					boxW = boxW > font->MeasureWidth(message) + pad * 2.f ? boxW
																		   : font->MeasureWidth(message) + pad * 2.f;
			}
			const float32 wCap = static_cast<float32>(ctx.viewW) * 0.7f;
			if (boxW > wCap)
				boxW = wCap;

			// Boutons : largeur auto (texte + marge), alignes a DROITE, en ligne.
			float32 btnWs[8];
			float32 btnRowW = 0.f;
			const int32 n = count > 8 ? 8 : count;
			for (int32 i = 0; i < n; ++i) {
				btnWs[i] = ((font && font->Valid()) ? font->MeasureWidth(buttons[i]) : 60.f) + 28.f;
				btnRowW += btnWs[i] + (i > 0 ? gap : 0.f);
			}
			if (btnRowW + pad * 2.f > boxW)
				boxW = btnRowW + pad * 2.f;

			const float32 titleH = (title && *title) ? lh + 6.f : 0.f;
			const float32 msgH = (message && *message) ? lh + 14.f : 6.f;
			const float32 boxH = pad + titleH + msgH + btnH + pad;
			const NkRect box = {(static_cast<float32>(ctx.viewW) - boxW) * 0.5f,
								 (static_cast<float32>(ctx.viewH) - boxH) * 0.5f, boxW, boxH};

			// Enregistre le rect pour la fermeture "clic dehors" de LA PROCHAINE
			// frame (mecanisme standard de NkGuiContext, cf. Update()).
			ctx.popupRects[0] = box;
			ctx.popupAnchor = box;

			dl.AddRectFilled(box, ctx.theme.panel, 8.f);
			dl.AddRect(box, ctx.theme.border, 1.5f);
			float32 y = box.y + pad;
			if (font && font->Valid()) {
				if (title && *title) {
					dl.AddText(font->Face(), font->TexId(), {box.x + pad, y + font->Ascent()}, title, ctx.theme.text);
					y += titleH;
				}
				if (message && *message) {
					dl.AddText(font->Face(), font->TexId(), {box.x + pad, y + font->Ascent()}, message,
							   ctx.theme.textDisabled, boxW - pad * 2.f);
					y += msgH;
				}
			}
			const NkVec2 mp = ctx.input.mousePos;
			int32 clicked = -1;
			float32 bx = box.x + boxW - pad - btnRowW;
			const float32 by = box.y + boxH - pad - btnH;
			for (int32 i = 0; i < n; ++i) {
				const NkRect br = {bx, by, btnWs[i], btnH};
				const bool hov = mp.x >= br.x && mp.x < br.x + br.w && mp.y >= br.y && mp.y < br.y + br.h;
				const bool primary = (i == 0);
				dl.AddRectFilled(br, hov ? ctx.theme.buttonHover : (primary ? ctx.theme.buttonActive : ctx.theme.button),
								 5.f);
				dl.AddRect(br, ctx.theme.border, 1.f);
				if (font && font->Valid()) {
					const float32 tw = font->MeasureWidth(buttons[i]);
					dl.AddText(font->Face(), font->TexId(),
							   {br.x + (br.w - tw) * 0.5f, br.y + (br.h - lh) * 0.5f + font->Ascent()}, buttons[i],
							   ctx.theme.text);
				}
				if (hov && ctx.input.mouseClicked[0])
					clicked = i;
				bx += btnWs[i] + gap;
			}
			if (ctx.input.KeyPressed(NkGuiKey::Enter)) // Entree = bouton par defaut (primaire)
				clicked = 0;
			if (clicked >= 0) {
				ctx.ClosePopup();
				m.open = false;
				m.registered = false;
			}
			// MODAL : consomme TOUT clic cette frame (pas seulement dans le
			// dialogue) — rien derriere ne doit reagir tant qu'il est affiche.
			ctx.input.mouseClicked[0] = false;
			ctx.input.mouseClicked[1] = false;
			return clicked;
		}

	} // namespace editorkit
} // namespace nkentseu
