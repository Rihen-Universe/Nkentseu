#pragma once
// MenuRole.h — LE MENU DES RÔLES (écrans Banani 5, 6 et 7) : le geste
// « promouvoir en widget » du §4.3 — « le moment décisif de tout l'outil ».
//
//  LA TAXONOMIE EST CELLE DE L'ÉCRAN 6, MOT POUR MOT — la « liste canonique
//  des rôles » du dépouillé (doc 11 §4). Elle vit ici en TABLE, en attendant
//  le registre de rôles de projet (« des millions d'auteurs » — la borne du
//  registre statique ne s'applique pas : ce sont des NOMS, pas des widgets).
//
//  CE QUE LE MENU FAIT VRAIMENT : choisir un rôle ÉCRIT la clé additive
//  `role` du nœud (Document.h, 31/08) — l'arbre montre la pilule, la toile
//  le badge, l'onglet Behavior ses événements. « Retirer le rôle » efface la
//  clé. C'est la donnée, pas un décor.
//
//  ÉCARTS NOMMÉS (rapport Q35) : les ~50 icônes distinctes de la maquette
//  sont approchées par UNE icône par CATÉGORIE (le redessin complet vient
//  avec le vocabulaire d'icônes) ; « Récents » est statique (l'historique
//  des choix n'est pas encore porté) ; « + Définir un rôle de projet… » est
//  inerte et le dit.

#include "Costume.h"
#include "Document.h"
#include "NKEditorKit/NkEditorTextField.h" // NkOverlayTextField (la recherche)

namespace nkuidesign {
	namespace menurole {

		using nkentseu::float32;
		using nkentseu::int32;
		using nkentseu::uint32;
		using nkgui::NkColor;
		using nkgui::NkGuiContext;
		using nkgui::NkRect;

		/// Une entrée : section (en-tête) ou rôle (choisissable).
		struct Entree {
				const char *nom;
				const char *sousTitre; ///< « dérive de… » / « composition » (rôles de projet)
				nkentseu::uint8 categorie; ///< 0 récents, 1 actions, 2 saisie, 3 sélection,
										   ///< 4 navigation, 5 conteneurs, 6 affichage, 7 projet
				bool estSection;
		};

		/// L'écran 6, mot pour mot, dans l'ordre.
		inline const Entree *Taxonomie(int32 &count) {
			static const Entree kT[] = {
				{"Récents", nullptr, 0, true},
				{"bouton", nullptr, 1, false},
				{"champ de saisie", nullptr, 2, false},
				{"carte cliquable", "composition", 7, false},
				{"Actions", nullptr, 1, true},
				{"bouton", nullptr, 1, false},
				{"bouton à répétition", nullptr, 1, false},
				{"bouton image", nullptr, 1, false},
				{"bouton de couleur", nullptr, 1, false},
				{"élément de menu", nullptr, 1, false},
				{"Saisie", nullptr, 2, true},
				{"champ de saisie", nullptr, 2, false},
				{"champ multiligne", nullptr, 2, false},
				{"champ entier", nullptr, 2, false},
				{"champ décimal", nullptr, 2, false},
				{"curseur", nullptr, 2, false},
				{"glisseur", nullptr, 2, false},
				{"sélecteur de couleur", nullptr, 2, false},
				{"Sélection", nullptr, 3, true},
				{"case à cocher", nullptr, 3, false},
				{"case à trois états", nullptr, 3, false},
				{"liste déroulante", nullptr, 3, false},
				{"liste", nullptr, 3, false},
				{"élément sélectionnable", nullptr, 3, false},
				{"Navigation", nullptr, 4, true},
				{"barre de menu", nullptr, 4, false},
				{"menu", nullptr, 4, false},
				{"menu contextuel", nullptr, 4, false},
				{"en-tête repliable", nullptr, 4, false},
				{"Conteneurs", nullptr, 5, true},
				{"fenêtre", nullptr, 5, false},
				{"panneau", nullptr, 5, false},
				{"groupe", nullptr, 5, false},
				{"boîte verticale", nullptr, 5, false},
				{"boîte horizontale", nullptr, 5, false},
				{"grille", nullptr, 5, false},
				{"tableau", nullptr, 5, false},
				{"zone défilante", nullptr, 5, false},
				{"Affichage", nullptr, 6, true},
				{"texte", nullptr, 6, false},
				{"image", nullptr, 6, false},
				{"barre de progression", nullptr, 6, false},
				{"courbe", nullptr, 6, false},
				{"séparateur", nullptr, 6, false},
				{"infobulle", nullptr, 6, false},
				{"Rôles de projet", nullptr, 7, true},
				{"interrupteur", "dérive de case à cocher", 7, false},
				{"bouton à bascule", "dérive de case à cocher", 7, false},
				{"accordéon", "composition", 7, false},
				{"carte cliquable", "dérive de bouton", 7, false},
				{"champ recherche", "dérive de champ de saisie", 7, false},
				{"badge", "composition", 7, false},
				{"carte produit", "composition", 7, false},
			};
			count = (int32)(sizeof(kT) / sizeof(kT[0]));
			return kT;
		}

		/// L'icône de CATÉGORIE (approximation nommée — le jeu complet viendra).
		inline void IconeCategorie(nkgui::NkGuiDrawList &dl, float32 x, float32 y,
								   nkentseu::uint8 cat, const NkColor &c) {
			switch (cat) {
				case 0: // récents : l'horloge
					costume::ModeAnimation(dl, x, y, c);
					break;
				case 1: // actions : le bouton
					costume::IcBouton(dl, x, y, c);
					break;
				case 2: // saisie : le « T »
					costume::IcTexte(dl, x, y, c);
					break;
				case 3: { // sélection : la case cochée
					dl.AddRect({x + 1.f, y + 1.f, 9.f, 9.f}, c, 1.2f, 2.f);
					const nkgui::NkVec2 p[3] = {{x + 3.f, y + 5.5f}, {x + 5.f, y + 7.5f},
												{x + 8.f, y + 3.5f}};
					dl.AddPolyline(p, 3, c, 1.2f);
					break;
				}
				case 4: // navigation : la barre de menu
					dl.AddRect({x + 1.f, y + 1.f, 9.f, 9.f}, c, 1.2f, 1.f);
					dl.AddLine({x + 1.f, y + 4.f}, {x + 10.f, y + 4.f}, c, 1.f);
					break;
				case 5: // conteneurs : le panneau
					costume::IcPanneau(dl, x, y, c);
					break;
				case 6: // affichage : la page
					costume::IcPage(dl, x, y, c);
					break;
				default: // projet : les carreaux (réduits)
					dl.AddRect({x + 1.f, y + 1.f, 4.f, 4.f}, c, 1.f, 1.f);
					dl.AddRect({x + 6.f, y + 1.f, 4.f, 4.f}, c, 1.f, 1.f);
					dl.AddRect({x + 1.f, y + 6.f, 4.f, 4.f}, c, 1.f, 1.f);
					dl.AddRect({x + 6.f, y + 6.f, 4.f, 4.f}, c, 1.f, 1.f);
					break;
			}
		}

		/// L'état du menu — vit dans DesignState (voir Panels.h).
		struct Etat {
				bool ouvert = false;
				NkRect ancre = {0.f, 0.f, 0.f, 0.f}; ///< la boîte « Rôle » de l'Inspecteur
				char filtre[64] = {0};
				bool filtreFocus = true;
				float32 defil = 0.f;
				bool vientDOuvrir = false; ///< mange le clic d'ouverture (1 image)
		};

		/// Filtre insensible à la casse (ASCII — les accents passent tels quels).
		inline bool Contient(const char *txt, const char *motif) {
			if (!motif || !*motif)
				return true;
			auto low = [](char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; };
			for (const char *t = txt; *t; ++t) {
				const char *a = t;
				const char *b = motif;
				while (*a && *b && low(*a) == low(*b)) {
					++a;
					++b;
				}
				if (!*b)
					return true;
			}
			return false;
		}

		/// Dessine le menu (couche OVERLAY) et rend le rôle choisi (nullptr si
		/// rien ; « \x01 » = retirer le rôle). Ferme sur Échap / clic dehors.
		inline const char *Dessiner(NkGuiContext &ctx, Etat &e, const char *roleActuel) {
			if (!e.ouvert)
				return nullptr;
			auto &dl = ctx.dlOverlay;
			auto &F = costume::Fontes();
			ctx.appModal = true; // le corps est masqué (patron NkModalFrameDraw)

			// ── Le voile (écran 6 : rgba(0,0,0,0.35)) ────────────────────────
			const float32 W = (float32)ctx.viewW, H = (float32)ctx.viewH;
			dl.AddRectFilled({0.f, 0.f, W, H}, {0, 0, 0, 89});

			// ── La boîte : 300 de large, à GAUCHE de l'ancre (sur la toile) ──
			const float32 mw = 300.f;
			float32 mh = H - e.ancre.y - 24.f;
			if (mh > 620.f)
				mh = 620.f;
			if (mh < 200.f)
				mh = 200.f;
			float32 mx = e.ancre.x - mw - 6.f;
			if (mx < 8.f)
				mx = 8.f;
			float32 my = e.ancre.y;
			if (my + mh > H - 8.f)
				my = H - 8.f - mh;
			const NkRect m = {mx, my, mw, mh};
			// menuBg #1e2631, bord #30363d, rayon 6, ombre (écran 6)
			dl.AddRectFilled({m.x - 1.f, m.y + 3.f, m.w + 2.f, m.h + 5.f}, {0, 0, 0, 70}, 10.f);
			dl.AddRectFilled(m, {30, 38, 49, 255}, 6.f);
			dl.AddRect(m, ctx.theme.border, 1.f, 6.f);

			// ── La recherche, ANCRÉE en haut ─────────────────────────────────
			const NkRect rf = {m.x + 10.f, m.y + 10.f, m.w - 20.f, 24.f};
			nkentseu::editorkit::NkOverlayTextField(ctx, dl, ctx.font, rf, e.filtre,
													(int32)sizeof(e.filtre), e.filtreFocus);
			if (!e.filtre[0])
				costume::Texte(dl, F.px11, rf.x + 8.f, costume::CentrerY(F.px11, rf.y, rf.h),
							   "Rechercher un rôle…", ctx.theme.textMuted);

			// ── La liste, défilante ──────────────────────────────────────────
			const NkRect zone = {m.x, rf.y + rf.h + 8.f, m.w, m.y + m.h - (rf.y + rf.h + 8.f)
																 - 40.f};
			dl.PushClipRect(zone, true);
			int32 n = 0;
			const Entree *tx = Taxonomie(n);
			float32 y = zone.y - e.defil;
			const char *choisi = nullptr;
			const nkgui::NkVec2 souris = ctx.input.mousePos;
			const bool clic = ctx.input.mouseClicked[0] && !e.vientDOuvrir;
			for (int32 i = 0; i < n; ++i) {
				const Entree &t = tx[i];
				if (t.estSection) {
					// une section ne s'affiche que si un de ses rôles passe le filtre
					bool visible = false;
					for (int32 j = i + 1; j < n && !tx[j].estSection; ++j)
						if (Contient(tx[j].nom, e.filtre))
							visible = true;
					if (!visible)
						continue;
					if (y + 24.f > zone.y && y < zone.y + zone.h)
						costume::TexteGras(dl, F.px9, m.x + 12.f, y + 8.f, t.nom,
										   ctx.theme.textMuted, 0.4f);
					y += 24.f;
					continue;
				}
				if (!Contient(t.nom, e.filtre))
					continue;
				const float32 rh = t.sousTitre ? 32.f : 26.f;
				const NkRect r = {m.x + 4.f, y, m.w - 8.f, rh};
				const bool actuel = roleActuel && *roleActuel
									&& NkComponentDecl::StrEq(t.nom, roleActuel);
				const bool survol = y + rh > zone.y && y < zone.y + zone.h
									&& NkGuiRectContains(r, souris)
									&& NkGuiRectContains(zone, souris);
				if (y + rh > zone.y && y < zone.y + zone.h) {
					if (actuel)
						dl.AddRectFilled(r, ctx.theme.accent, 4.f);
					else if (survol)
						dl.AddRectFilled(r, ctx.theme.buttonHover, 4.f);
					const NkColor ic = actuel ? ctx.theme.onAccent : ctx.theme.textMuted;
					IconeCategorie(dl, r.x + 10.f, y + (rh - 11.f) * 0.5f, t.categorie, ic);
					costume::Texte(dl, F.px11, r.x + 28.f,
								   t.sousTitre ? y + 3.f : costume::CentrerY(F.px11, y, rh),
								   t.nom, actuel ? ctx.theme.onAccent : ctx.theme.text);
					if (t.sousTitre)
						costume::Texte(dl, F.px9, r.x + 28.f, y + 17.f, t.sousTitre,
									   actuel ? ctx.theme.onAccent : ctx.theme.textMuted);
					if (actuel) {
						// la coche du rôle porté (écran 3 : la ligne active)
						const nkgui::NkVec2 p[3] = {{r.x + r.w - 20.f, y + rh * 0.5f},
													{r.x + r.w - 16.f, y + rh * 0.5f + 4.f},
													{r.x + r.w - 10.f, y + rh * 0.5f - 4.f}};
						dl.AddPolyline(p, 3, ctx.theme.onAccent, 1.4f);
					}
				}
				if (survol && clic)
					choisi = t.nom;
				y += rh;
			}
			const float32 contenuH = (y + e.defil) - zone.y;
			dl.PopClipRect();

			// ── Le pied ANCRÉ : « + Définir… » et « Retirer le rôle » ────────
			{
				const NkRect pied = {m.x, m.y + m.h - 40.f, m.w, 40.f};
				dl.AddLine({pied.x, pied.y}, {pied.x + pied.w, pied.y}, ctx.theme.border, 1.f);
				costume::Texte(dl, F.px11, pied.x + 12.f,
							   costume::CentrerY(F.px11, pied.y, 26.f) + 7.f,
							   "+ Définir un rôle de projet…", ctx.theme.textMuted);
				if (roleActuel && *roleActuel) {
					const char *lib = "Retirer";
					const float32 wl = costume::Largeur(F.px11, lib);
					const NkRect rr = {pied.x + pied.w - wl - 24.f, pied.y + 8.f, wl + 16.f,
									   24.f};
					const bool sv = NkGuiRectContains(rr, souris);
					if (sv)
						dl.AddRectFilled(rr, ctx.theme.buttonHover, 4.f);
					costume::Texte(dl, F.px11, rr.x + 8.f, costume::CentrerY(F.px11, rr.y, rr.h),
								   lib, ctx.theme.danger);
					if (sv && clic)
						choisi = "\x01"; // retirer
				}
			}

			// ── Molette, Échap, clic dehors ──────────────────────────────────
			if (NkGuiRectContains(m, souris) && ctx.input.wheel != 0.f) {
				e.defil -= ctx.input.wheel * 48.f;
				const float32 maxD = contenuH - zone.h;
				if (e.defil > (maxD > 0.f ? maxD : 0.f))
					e.defil = maxD > 0.f ? maxD : 0.f;
				if (e.defil < 0.f)
					e.defil = 0.f;
			}
			bool fermer = false;
			if (ctx.input.KeyPressed(nkgui::NkGuiKey::Escape))
				fermer = true;
			if (clic && !NkGuiRectContains(m, souris))
				fermer = true;
			if (choisi || fermer) {
				e.ouvert = false;
				e.filtre[0] = 0;
				e.defil = 0.f;
			}
			e.vientDOuvrir = false;
			return choisi;
		}

	} // namespace menurole
} // namespace nkuidesign
