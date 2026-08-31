#pragma once
// -----------------------------------------------------------------------------
// @File    MenuFormat.h
// @Brief   LE MENU DU CATALOGUE DE FORMATS (demande de Rodolf, 31/08). Ouvert
//          par la section CIBLE de l'Inspecteur, dessine en OVERLAY (le patron
//          de MenuRole.h — meme voile, meme fermeture Echap/clic dehors, meme
//          « vientDOuvrir » qui mange le clic d'ouverture). Deux volets :
//          les CATEGORIES a gauche (derivees de la table Formats.h + la ligne
//          « Personnalisé »), les FORMATS de la categorie a droite — chaque
//          rangee porte nom, « L × H » et la note dpi quand elle existe (la
//          provenance du papier S'AFFICHE, elle ne se devine pas).
//          « Personnalisé » : deux champs L/H au glisser + « Appliquer ».
//          ⚠️ Le menu ne TOUCHE PAS le document : il rend un Choix, et c'est
//          DesignState::AppliquerFormat qui ecrit (cible + redimension +
//          constats de depassement au rapport) — une seule main sur le modele.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Costume.h"
#include "Formats.h"

namespace nkuidesign {
	namespace menuformat {

		using nkentseu::float32;
		using nkentseu::int32;
		using nkentseu::uint32;
		using nkgui::NkColor;
		using nkgui::NkGuiContext;
		using nkgui::NkRect;

		/// L'état du menu — vit dans DesignState (le patron menurole::Etat).
		struct Etat {
				bool ouvert = false;
				NkRect ancre = {0.f, 0.f, 0.f, 0.f}; ///< la boîte « Cible » de l'Inspecteur
				bool vientDOuvrir = false;			 ///< mange le clic d'ouverture (1 image)
				int32 page = -1;					 ///< le cadre à reformater
				int32 categorie = 0;				 ///< volet gauche sélectionné
				float32 customW = 800.f;			 ///< le couple libre du « Personnalisé »
				float32 customH = 600.f;
				nkgui::NkGuiId drag = 0; ///< champ L/H en cours de glisser
				float32 dragX = 0.f;
		};

		/// Ce que le menu rend quand un format est choisi.
		struct Choix {
				bool fait = false;
				const char *nom = nullptr; ///< « Full HD », « A4 portrait », « Personnalisé »
				float32 w = 0.f, h = 0.f;
				const char *note = ""; ///< « @ 96 dpi » pour le papier
		};

		/// Un champ numérique au glisser (le geste des champs d'Inspecteur,
		/// local au menu — l'overlay n'a pas accès aux méthodes du panneau).
		inline bool ChampGlisser(NkGuiContext &ctx, nkgui::NkGuiDrawList &dl, Etat &e,
								 const char *id, const NkRect &r, float32 &v) {
			auto &F = costume::Fontes();
			dl.AddRectFilled(r, {1, 4, 9, 255}, 4.f);
			dl.AddRect(r, ctx.theme.border, 1.f, 4.f);
			char b[16];
			snprintf(b, sizeof(b), "%d", (int32)v);
			costume::Texte(dl, F.px11, r.x + 6.f, costume::CentrerY(F.px11, r.y, r.h), b,
						   ctx.theme.text);
			const nkgui::NkGuiId gid = ctx.GetId(id);
			bool change = false;
			const bool dans = NkGuiRectContains(r, ctx.input.mousePos);
			if (dans)
				ctx.wantCursor = nkgui::NkGuiCursor::ResizeEW;
			if (dans && ctx.input.mouseClicked[0] && !e.vientDOuvrir) {
				e.drag = gid;
				e.dragX = ctx.input.mousePos.x;
			}
			if (e.drag == gid) {
				if (!ctx.input.mouseDown[0])
					e.drag = 0;
				else {
					const float32 dx = ctx.input.mousePos.x - e.dragX;
					if (dx != 0.f) {
						e.dragX = ctx.input.mousePos.x;
						float32 nv = v + dx * 2.f;
						if (nv < 16.f)
							nv = 16.f;
						if (nv > 8192.f)
							nv = 8192.f;
						if (nv != v) {
							v = nv;
							change = true;
						}
					}
				}
			}
			return change;
		}

		/// Dessine le menu (couche OVERLAY) et rend le Choix. Ferme sur Échap,
		/// clic dehors, ou choix fait.
		inline Choix Dessiner(NkGuiContext &ctx, Etat &e, const char *cibleActuelle) {
			Choix choix;
			if (!e.ouvert)
				return choix;
			auto &dl = ctx.dlOverlay;
			auto &F = costume::Fontes();
			ctx.appModal = true;
			const float32 W = (float32)ctx.viewW, H = (float32)ctx.viewH;
			dl.AddRectFilled({0.f, 0.f, W, H}, {0, 0, 0, 89});

			// ── La boîte : 340 × 320, à GAUCHE de l'ancre (l'Inspecteur est à
			//    droite de l'écran — le menu s'ouvre vers la toile) ──────────
			const float32 mw = 340.f, mh = 320.f;
			float32 mx = e.ancre.x - mw - 6.f;
			if (mx < 8.f)
				mx = 8.f;
			float32 my = e.ancre.y;
			if (my + mh > H - 8.f)
				my = H - 8.f - mh;
			const NkRect m = {mx, my, mw, mh};
			dl.AddRectFilled({m.x - 1.f, m.y + 3.f, m.w + 2.f, m.h + 5.f}, {0, 0, 0, 70}, 10.f);
			dl.AddRectFilled(m, {30, 38, 49, 255}, 6.f);
			dl.AddRect(m, ctx.theme.border, 1.f, 6.f);
			costume::TexteGras(dl, F.px11, m.x + 12.f, m.y + 10.f, "Format de la page",
							   ctx.theme.text, 0.35f);

			const nkgui::NkVec2 souris = ctx.input.mousePos;
			const bool clic = ctx.input.mouseClicked[0] && !e.vientDOuvrir;

			// ── Volet GAUCHE : les catégories (dérivées de la table) + libre ──
			const char *cats[8];
			const uint32 nCats = NkFormatCategories(cats, 7);
			const uint32 idxPerso = nCats; // « Personnalisé » en queue
			const NkRect volet = {m.x + 8.f, m.y + 34.f, 112.f, m.h - 42.f};
			for (uint32 c = 0; c <= nCats; ++c) {
				const char *nom = (c == idxPerso) ? "Personnalisé" : cats[c];
				const NkRect r = {volet.x, volet.y + (float32)c * 28.f, volet.w, 26.f};
				const bool actif = ((int32)c == e.categorie);
				const bool sv = NkGuiRectContains(r, souris);
				if (actif) {
					NkColor voile = ctx.theme.accent;
					voile.a = 44;
					dl.AddRectFilled(r, voile, 4.f);
				} else if (sv)
					dl.AddRectFilled(r, ctx.theme.buttonHover, 4.f);
				costume::Texte(dl, F.px11, r.x + 10.f, costume::CentrerY(F.px11, r.y, r.h), nom,
							   actif ? ctx.theme.text : ctx.theme.textMuted);
				if (sv && clic)
					e.categorie = (int32)c;
			}
			// le filet entre les volets
			dl.AddLine({volet.x + volet.w + 6.f, volet.y}, {volet.x + volet.w + 6.f, m.y + m.h - 8.f},
					   ctx.theme.border, 1.f);

			// ── Volet DROIT : les formats de la catégorie, ou le libre ───────
			const float32 dx0 = volet.x + volet.w + 14.f;
			const float32 dw = m.x + m.w - 8.f - dx0;
			if (e.categorie == (int32)idxPerso) {
				// « Personnalisé » : L × H au glisser + Appliquer.
				costume::Texte(dl, F.px10, dx0, volet.y + 4.f, "Largeur × hauteur (px)",
							   ctx.theme.textMuted);
				const NkRect rw = {dx0, volet.y + 26.f, 70.f, 22.f};
				const NkRect rh = {dx0 + 86.f, volet.y + 26.f, 70.f, 22.f};
				costume::Texte(dl, F.px11, rw.x + rw.w + 4.f,
							   costume::CentrerY(F.px11, rw.y, rw.h), "\xC3\x97",
							   ctx.theme.textMuted);
				ChampGlisser(ctx, dl, e, "fmt.custom.w", rw, e.customW);
				ChampGlisser(ctx, dl, e, "fmt.custom.h", rh, e.customH);
				const NkRect ra = {dx0, volet.y + 62.f, 110.f, 26.f};
				const bool sva = NkGuiRectContains(ra, souris);
				dl.AddRectFilled(ra, sva ? ctx.theme.buttonHover : ctx.theme.button, 5.f);
				dl.AddRect(ra, ctx.theme.accent, 1.f, 5.f);
				costume::TexteGras(dl, F.px11,
								   ra.x + (ra.w - costume::Largeur(F.px11, "Appliquer")) * 0.5f,
								   costume::CentrerY(F.px11, ra.y, ra.h), "Appliquer",
								   ctx.theme.text, 0.3f);
				if (sva && clic) {
					choix.fait = true;
					choix.nom = "Personnalisé";
					choix.w = e.customW;
					choix.h = e.customH;
				}
				costume::Texte(dl, F.px9, dx0, volet.y + 100.f,
							   "Glisser un champ règle la valeur.", ctx.theme.textMuted);
			} else {
				uint32 total = 0;
				const NkFormatPage *k = NkFormatCatalogue(total);
				float32 y = volet.y;
				for (uint32 i = 0; i < total; ++i) {
					if (!NkFormatStrEq(k[i].categorie,
									   e.categorie < (int32)nCats ? cats[e.categorie] : ""))
						continue;
					const NkRect r = {dx0, y, dw, 30.f};
					// le format ACTUEL : la cible commence par son nom
					bool actuel = false;
					if (cibleActuelle && *cibleActuelle) {
						const char *a = cibleActuelle;
						const char *b = k[i].nom;
						while (*a && *b && *a == *b) {
							++a;
							++b;
						}
						actuel = (*b == 0);
					}
					const bool sv = NkGuiRectContains(r, souris);
					if (actuel)
						dl.AddRectFilled(r, ctx.theme.accent, 4.f);
					else if (sv)
						dl.AddRectFilled(r, ctx.theme.buttonHover, 4.f);
					costume::Texte(dl, F.px11, r.x + 8.f, y + 3.f, k[i].nom,
								   actuel ? ctx.theme.onAccent : ctx.theme.text);
					char dim[48];
					snprintf(dim, sizeof(dim), "%d \xC3\x97 %d %s", (int32)k[i].w, (int32)k[i].h,
							 k[i].note);
					costume::Texte(dl, F.px9, r.x + 8.f, y + 17.f, dim,
								   actuel ? ctx.theme.onAccent : ctx.theme.textMuted);
					if (sv && clic) {
						choix.fait = true;
						choix.nom = k[i].nom;
						choix.w = k[i].w;
						choix.h = k[i].h;
						choix.note = k[i].note;
					}
					y += 32.f;
				}
			}

			// ── Échap, clic dehors, choix : fermer ───────────────────────────
			bool fermer = false;
			if (ctx.input.KeyPressed(nkgui::NkGuiKey::Escape))
				fermer = true;
			if (clic && !NkGuiRectContains(m, souris))
				fermer = true;
			if (choix.fait || fermer)
				e.ouvert = false;
			e.vientDOuvrir = false;
			return choix;
		}

	} // namespace menuformat
} // namespace nkuidesign
