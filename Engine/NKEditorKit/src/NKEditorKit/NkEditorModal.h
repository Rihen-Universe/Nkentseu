#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorModal.h
// @Brief   Dialogue MODAL deplacable (barre de titre + message + boutons), pour
//          toute confirmation bloquante (fermeture non enregistree,
//          suppression...). Contrairement a NkCtxMenu (NkEditorContextMenu.h -
//          menu leger ancre au clic, "modal-lite" : ne consomme le clic QUE
//          quand la souris est dedans), celui-ci force ctx.popupDepth > 0 pour
//          TOUTE la duree ou il reste affiche (gestion DIRECTE, pas de
//          dependance au cycle OpenPopup/IsPopupOpen de NkGui — qui suppose un
//          seul popup exclusif au niveau 0 et peut se faire ecraser par un
//          AUTRE widget de la meme frame). C'est deja le signal verifie par la
//          quasi-totalite des points d'interaction de NKCode
//          (`ctx.popupDepth == 0` avant d'agir) : la modalite est donc
//          GLOBALE (tous panneaux) sans aucune modification ailleurs.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NKGui/NKGui.h"

namespace nkentseu {
	namespace editorkit {

		using namespace nkentseu;
		using namespace nkentseu::nkgui;

		// Etat d'un dialogue modal (un par confirmation possible dans l'appli).
		struct NkModal {
				bool open = false;
				bool posInit = false; // pos centree calculee au premier affichage
				NkVec2 pos{0.f, 0.f}; // coin haut-gauche — deplacable via la barre de titre
				bool dragging = false;
				NkVec2 dragOff{0.f, 0.f};
		};

		// Dessine + pilote un dialogue modal deplacable (barre de titre + message +
		// boutons). `buttons[count-1]` est la convention "Annuler" (retourne aussi
		// quand l'utilisateur clique en dehors du dialogue ou appuie sur Echap).
		// Retourne l'index du bouton clique cette frame, -1 si rien encore.
		inline int32 NkModalDraw(NkGuiContext &ctx, NkModal &m, const char *title, const char *message,
								  const char *const *buttons, int32 count) {
			if (!m.open || count <= 0)
				return -1;
			NkGuiDrawList &dl = ctx.dlOverlay;
			const NkGuiFont *font = ctx.font;
			const float32 lh = (font && font->Valid()) ? font->LineHeight() : 16.f;
			const float32 pad = 16.f, gap = 10.f, btnH = lh + 16.f, titleH = lh + 12.f;

			// Voile derriere le dialogue : signale visuellement le blocage global.
			// UNE SEULE FOIS POUR TOUTE LA PILE. Une modale peut s'ouvrir au-dessus
			// d'une autre, et celle du dessous doit rester VISIBLE (Rihen, 13 aout).
			// Un voile par modale les aurait assombries l'une apres l'autre jusqu'a
			// les effacer. Seule la modale du BAS le pose ; les suivantes s'y posent.
			const bool premiereDeLaPile = (ctx.modalDepth == 0);
			++ctx.modalDepth;
			if (premiereDeLaPile)
				dl.AddRectFilled({0.f, 0.f, static_cast<float32>(ctx.viewW), static_cast<float32>(ctx.viewH)},
								  NkColor{0, 0, 0, 120});

			// Largeur : la plus longue ligne (titre/message), jamais plus de 70% de la fenetre.
			float32 boxW = 380.f;
			if (font && font->Valid()) {
				const float32 tw = (title && *title) ? font->MeasureWidth(title) + pad * 2.f : 0.f;
				const float32 mw = (message && *message) ? font->MeasureWidth(message) + pad * 2.f : 0.f;
				if (tw > boxW)
					boxW = tw;
				if (mw > boxW)
					boxW = mw;
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

			const float32 msgH = (message && *message) ? lh + 14.f : 10.f;
			const float32 boxH = titleH + msgH + btnH + pad;
			// Le clic qui vient d'OUVRIR ce dialogue (ex. bouton X d'un onglet) reste
			// vu comme mouseClicked[0] cette meme frame, avec la souris tres
			// probablement HORS du rect (nouvellement centre) -> sans ce garde-fou,
			// le test "clic en dehors -> Annuler" plus bas fermait le dialogue
			// AUSSITOT ouvert (jamais visible assez longtemps pour cliquer/glisser).
			const bool justOpened = !m.posInit;
			if (!m.posInit) {
				m.pos = {(static_cast<float32>(ctx.viewW) - boxW) * 0.5f,
						 (static_cast<float32>(ctx.viewH) - boxH) * 0.5f};
				m.posInit = true;
			}
			// Reste dans la fenetre (redimensionnement, changement de contenu...).
			if (m.pos.x < 0.f)
				m.pos.x = 0.f;
			if (m.pos.y < 0.f)
				m.pos.y = 0.f;
			if (m.pos.x + boxW > static_cast<float32>(ctx.viewW))
				m.pos.x = static_cast<float32>(ctx.viewW) - boxW;
			if (m.pos.y + boxH > static_cast<float32>(ctx.viewH))
				m.pos.y = static_cast<float32>(ctx.viewH) - boxH;
			const NkVec2 mp = ctx.input.mousePos;
			// ── ROUTEUR D'OCCLUSION UNIFIE : ce dialogue est une surface MODALE
			// (couche 100). Ses hit-tests utilisent la couche 100 ; toute surface de
			// couche inferieure passee a InputHits/ClickIn devient automatiquement
			// aveugle sous son rect (PushOcclusion en fin de fonction). ──
			NkGuiContext::NkInputLayerScope _layer(ctx, 100);
			// Glisser de la barre de titre : traite AVANT de figer `box` pour le
			// rendu/hit-test de cette frame (sinon la position affichee reste
			// toujours en retard d'une frame sur la souris).
			{
				const NkRect prelimTitle = {m.pos.x, m.pos.y, boxW, titleH};
				const bool overTitlePrelim = mp.x >= prelimTitle.x && mp.x < prelimTitle.x + prelimTitle.w &&
											 mp.y >= prelimTitle.y && mp.y < prelimTitle.y + prelimTitle.h;
				if (!justOpened && overTitlePrelim && ctx.input.mouseClicked[0] && !m.dragging) {
					m.dragging = true;
					m.dragOff = {mp.x - m.pos.x, mp.y - m.pos.y};
				}
				if (m.dragging) {
					if (ctx.input.mouseDown[0]) {
						m.pos = {mp.x - m.dragOff.x, mp.y - m.dragOff.y};
						if (m.pos.x < 0.f)
							m.pos.x = 0.f;
						if (m.pos.y < 0.f)
							m.pos.y = 0.f;
						if (m.pos.x + boxW > static_cast<float32>(ctx.viewW))
							m.pos.x = static_cast<float32>(ctx.viewW) - boxW;
						if (m.pos.y + boxH > static_cast<float32>(ctx.viewH))
							m.pos.y = static_cast<float32>(ctx.viewH) - boxH;
					} else
						m.dragging = false;
				}
			}
			const NkRect box = {m.pos.x, m.pos.y, boxW, boxH};
			const NkRect titleBar = {box.x, box.y, box.w, titleH};
			const bool inBox = mp.x >= box.x && mp.x < box.x + box.w && mp.y >= box.y && mp.y < box.y + box.h;

			dl.AddRectFilled(box, ctx.theme.panel, 8.f);
			dl.AddRect(box, ctx.theme.border, 1.5f);
			// Barre de titre : fond distinct + libelle.
			dl.AddRectFilled({box.x + 1.f, box.y + 1.f, box.w - 2.f, titleH - 1.f}, ctx.theme.header, 7.f);
			if (font && font->Valid() && title && *title)
				dl.AddText(font->Face(), font->TexId(), {box.x + pad, box.y + (titleH - lh) * 0.5f + font->Ascent()},
						   title, ctx.theme.text);

			float32 y = box.y + titleH + pad * 0.5f;
			if (font && font->Valid() && message && *message)
				dl.AddText(font->Face(), font->TexId(), {box.x + pad, y + font->Ascent()}, message,
						   ctx.theme.textDisabled, boxW - pad * 2.f);

			int32 clicked = -1;
			float32 bx = box.x + boxW - pad - btnRowW;
			const float32 by = box.y + boxH - pad - btnH + 4.f;
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
			if (clicked < 0 && ctx.input.KeyPressed(NkGuiKey::Enter)) // Entree = bouton par defaut (primaire)
				clicked = 0;
			bool cancelled = false;
			if (clicked < 0 && ctx.input.KeyPressed(NkGuiKey::Escape))
				cancelled = true;
			if (clicked < 0 && !cancelled && !justOpened && ctx.input.mouseClicked[0] && !inBox)
				cancelled = true; // clic en dehors du dialogue -> Annuler (jamais sur la frame d'ouverture)

			// MODALITE : force ctx.popupDepth > 0 tant que ce dialogue reste affiche —
			// deja verifie par la quasi-totalite des points d'interaction de NKCode,
			// donc bloque TOUT le reste de l'appli sans autre modification.
			if (ctx.popupDepth == 0)
				ctx.popupDepth = 1;
			// NkGuiContext::Update() (debut de frame, AVANT que ce dialogue ne se dessine)
			// reinitialise ctx.popupDepth a 0 des qu'un clic tombe hors de
			// popupRects[0]/popupAnchor de la frame PRECEDENTE. Sans les renseigner, CHAQUE
			// clic (meme sur ce dialogue) faisait retomber popupDepth a 0 AVANT que la force
			// ci-dessus ne le remette a 1 — largement le temps pour un AUTRE panneau, traite
			// plus tot dans la frame, de reagir au meme clic entretemps.
			ctx.popupRects[0] = box;
			ctx.popupAnchor = box;
			// Routeur d'occlusion : declare la surface modale pour la frame suivante
			// (les hit-tests de couche < 100 sous ce rect echoueront d'eux-memes).
			ctx.PushOcclusion(box, 100);

			if (clicked >= 0 || cancelled) {
				m.open = false;
				m.posInit = false;
				m.dragging = false;
				ctx.popupDepth = 0; // libere immediatement (aucun autre popup exclusif coexistant attendu)
				return clicked >= 0 ? clicked : count - 1;
			}
			// Consomme TOUT clic cette frame (rien derriere ne doit reagir tant que
			// le dialogue reste affiche), SAUF celui qui vient d'armer le glisser
			// (deja traite ci-dessus, ne doit pas se propager non plus).
			ctx.input.mouseClicked[0] = false;
			ctx.input.mouseClicked[1] = false;
			return -1;
		}

		// ── CADRE MODAL A CONTENU LIBRE ─────────────────────────────────────────
		// NkModalDraw ci-dessus sait faire un dialogue de CONFIRMATION : titre,
		// message, boutons. Beaucoup de dialogues ont un contenu propre -- une
		// liste, un formulaire, une arborescence. Plutot que de leur faire
		// reecrire la modalite (voile, deplacement, occlusion, popupDepth), ce
		// cadre la fournit et laisse l'appelant peindre DANS le rect qu'il rend.
		//
		// L'appelant dessine son contenu APRES l'appel, dans `content`, sur la
		// couche overlay (ctx.dlOverlay) pour rester au-dessus du cadre.
		// ── STYLE DU CADRE MODAL ────────────────────────────────────────────────
		// Les couleurs sont ICI, comme pour NkFilePickerStyle : personnaliser une
		// modale ne doit demander que de remplir ce struct. Les valeurs par defaut
		// sont celles de GITHUB DARK (demande de Rihen, 13 aout 2026), la meme
		// palette que le reste des editeurs de la suite.
		struct NkModalStyle {
				NkColor backdrop = {1, 4, 9, 160};	   ///< voile (#010409)
				NkColor card = {13, 17, 23, 255};	   ///< fond de la fenetre (#0d1117)
				NkColor header = {22, 27, 34, 255};	   ///< barre de titre (#161b22)
				NkColor border = {48, 54, 61, 255};	   ///< liseres (#30363d)
				NkColor text = {201, 209, 217, 255};   ///< libelles (#c9d1d9)
				NkColor closeHover = {33, 38, 45, 255}; ///< survol de la croix (#21262d)
				NkColor accent = {88, 166, 255, 255};  ///< bleu d'accent (#58a6ff)
		};

		/// Construit le style d'une modale a partir du THEME COURANT du contexte.
		/// Prepare le multi-theme (Rihen, 13 aout) : le jour ou l'utilisateur change
		/// de theme, les modales suivent sans qu'aucune d'elles ne soit retouchee.
		/// Les valeurs par defaut de NkModalStyle restent GitHub dark, pour les
		/// contextes dont le theme n'est pas encore renseigne.
		inline NkModalStyle NkModalStyleFromTheme(const NkGuiContext &ctx) {
			NkModalStyle s;
			s.card = ctx.theme.panel;
			s.header = ctx.theme.header;
			s.border = ctx.theme.border;
			s.text = ctx.theme.text;
			s.closeHover = ctx.theme.buttonHover;
			s.accent = ctx.theme.accent;
			return s;
		}

		struct NkModalFrame {
				NkRect box{};	  ///< la fenetre entiere (titre compris)
				NkRect content{}; ///< la zone utile, sous la barre de titre
				bool closeAsked = false; ///< Echap, clic dehors, ou croix de fermeture
				bool visible = false;	 ///< faux si la modale n'est pas ouverte
		};

		/// Dessine le cadre (voile, boite, barre de titre deplacable, croix) et rend
		/// la zone de contenu. Ne consomme PAS les clics tombant dans `content` :
		/// c'est l'appelant qui les traite pour ses propres widgets.
		inline NkModalFrame NkModalFrameDraw(NkGuiContext &ctx, NkModal &m, const char *title,
											 float32 contentW, float32 contentH,
											 const NkModalStyle &sty = NkModalStyle{}) {
			NkModalFrame out;
			if (!m.open)
				return out;
			NkGuiDrawList &dl = ctx.dlOverlay;
			const NkGuiFont *font = ctx.font;
			const float32 lh = (font && font->Valid()) ? font->LineHeight() : 16.f;
			const float32 pad = 16.f, titleH = lh + 12.f;

			// VOILE : une seule fois pour toute la pile (voir NkModalDraw).
			const bool premiereDeLaPile = (ctx.modalDepth == 0);
			++ctx.modalDepth;
			if (premiereDeLaPile)
				dl.AddRectFilled({0.f, 0.f, static_cast<float32>(ctx.viewW),
								  static_cast<float32>(ctx.viewH)},
								 sty.backdrop);

			float32 boxW = contentW + pad * 2.f;
			const float32 boxH = titleH + contentH + pad;
			if (font && font->Valid() && title && *title) {
				const float32 tw = font->MeasureWidth(title) + pad * 3.f + titleH;
				if (tw > boxW)
					boxW = tw;
			}
			const bool justOpened = !m.posInit;
			if (!m.posInit) {
				m.pos = {(static_cast<float32>(ctx.viewW) - boxW) * 0.5f,
						 (static_cast<float32>(ctx.viewH) - boxH) * 0.5f};
				m.posInit = true;
			}
			if (m.pos.x < 0.f)
				m.pos.x = 0.f;
			if (m.pos.y < 0.f)
				m.pos.y = 0.f;
			if (m.pos.x + boxW > static_cast<float32>(ctx.viewW))
				m.pos.x = static_cast<float32>(ctx.viewW) - boxW;
			if (m.pos.y + boxH > static_cast<float32>(ctx.viewH))
				m.pos.y = static_cast<float32>(ctx.viewH) - boxH;

			const NkVec2 mp = ctx.input.mousePos;
			NkGuiContext::NkInputLayerScope _layer(ctx, 100);
			// Glisser par la barre de titre, traite AVANT de figer la boite (sinon
			// l'affichage reste en retard d'une frame sur la souris).
			{
				const NkRect t0 = {m.pos.x, m.pos.y, boxW, titleH};
				const bool overTitle = mp.x >= t0.x && mp.x < t0.x + t0.w && mp.y >= t0.y &&
									   mp.y < t0.y + t0.h;
				if (!justOpened && overTitle && ctx.input.mouseClicked[0] && !m.dragging) {
					m.dragging = true;
					m.dragOff = {mp.x - m.pos.x, mp.y - m.pos.y};
				}
				if (m.dragging) {
					if (ctx.input.mouseDown[0]) {
						m.pos = {mp.x - m.dragOff.x, mp.y - m.dragOff.y};
						if (m.pos.x < 0.f)
							m.pos.x = 0.f;
						if (m.pos.y < 0.f)
							m.pos.y = 0.f;
						if (m.pos.x + boxW > static_cast<float32>(ctx.viewW))
							m.pos.x = static_cast<float32>(ctx.viewW) - boxW;
						if (m.pos.y + boxH > static_cast<float32>(ctx.viewH))
							m.pos.y = static_cast<float32>(ctx.viewH) - boxH;
					} else
						m.dragging = false;
				}
			}

			const NkRect box = {m.pos.x, m.pos.y, boxW, boxH};
			const bool inBox = mp.x >= box.x && mp.x < box.x + box.w && mp.y >= box.y &&
							   mp.y < box.y + box.h;
			dl.AddRectFilled(box, sty.card, 8.f);
			dl.AddRect(box, sty.border, 1.5f);
			dl.AddRectFilled({box.x + 1.f, box.y + 1.f, box.w - 2.f, titleH - 1.f}, sty.header,
							 7.f);
			// Un LISERE D'ACCENT sous la barre de titre : c'est ce qui distingue une
			// fenetre modale d'un simple panneau, dans GitHub comme dans NKCode.
			dl.AddRectFilled({box.x + 1.f, box.y + titleH - 1.f, box.w - 2.f, 1.f}, sty.border);
			if (font && font->Valid() && title && *title)
				dl.AddText(font->Face(), font->TexId(),
						   {box.x + pad, box.y + (titleH - lh) * 0.5f + font->Ascent()}, title,
						   sty.text);

			// Croix de fermeture, a droite de la barre de titre.
			const float32 cs = titleH - 10.f;
			const NkRect cr = {box.x + boxW - cs - 6.f, box.y + 5.f, cs, cs};
			const bool overClose = mp.x >= cr.x && mp.x < cr.x + cr.w && mp.y >= cr.y &&
								   mp.y < cr.y + cr.h;
			if (overClose)
				dl.AddRectFilled(cr, sty.closeHover, 4.f);
			{
				const float32 k = 3.5f, cx = cr.x + cr.w * 0.5f, cy = cr.y + cr.h * 0.5f;
				dl.AddLine({cx - k, cy - k}, {cx + k, cy + k}, sty.text, 1.5f);
				dl.AddLine({cx + k, cy - k}, {cx - k, cy + k}, sty.text, 1.5f);
			}

			// MODALITE : meme mecanique que NkModalDraw -- popupDepth force, rects
			// de popup renseignes (sinon Update() le remet a 0 des le premier clic),
			// et emprise declaree en couche 100 pour la frame suivante.
			if (ctx.popupDepth == 0)
				ctx.popupDepth = 1;
			ctx.popupRects[0] = box;
			ctx.popupAnchor = box;
			ctx.PushOcclusion(box, 100);

			out.visible = true;
			out.box = box;
			out.content = {box.x + pad, box.y + titleH, contentW, contentH};
			out.closeAsked = (overClose && ctx.input.mouseClicked[0]) ||
							 ctx.input.KeyPressed(NkGuiKey::Escape) ||
							 (!justOpened && ctx.input.mouseClicked[0] && !inBox);
			if (out.closeAsked) {
				m.open = false;
				m.posInit = false;
				m.dragging = false;
				ctx.popupDepth = 0;
			}
			// Un clic HORS de la boite ne doit rien atteindre derriere. Celui qui
			// tombe DEDANS reste disponible : c'est l'appelant qui peint le contenu.
			if (!inBox) {
				ctx.input.mouseClicked[0] = false;
				ctx.input.mouseClicked[1] = false;
			}
			return out;
		}

	} // namespace editorkit
} // namespace nkentseu
