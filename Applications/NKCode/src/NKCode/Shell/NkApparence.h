#pragma once
// =============================================================================
// NkApparence.h — LES APPARENCES DE NKCODE, reversibles (01/10, choix de Rihen :
// « une apparence dans les reglages ; revenir = rechoisir »).
//
// Une apparence = une PALETTE DE ROLES (un role par fond : cadre, panneaux,
// editeur...) + une DISPOSITION (barre d'outils, barres d'activite, onglets des
// panneaux lateraux, hauteur de la barre de titre, bloc logo au coin, barre
// d'etat, style de l'en-tete de l'Explorateur, ilots).
//
//   0 Classique  la disposition d'avant le 01/10, CORRIGEE (maquette A « dans
//                tous les cas ») : un role par fond, le panneau IA du meme monde
//                que les autres (il etait NOIR en theme clair), « Explorateur »
//                une seule fois, onglets du bas en francais, accent = bleu du logo.
//   1 Nettoyee   maquette A : la disposition d'aujourd'hui sans la rangee
//                d'outils, une seule barre d'activites, palette Primer a trois
//                tons (cadre #010409 / panneaux #0D1117 / editeur #161B22).
//   2 Famille    maquette B : la coquille d'UnkenyEditor — logo au coin sur deux
//                rangees, boutons bordes « Libelle : valeur », panneaux poses sur
//                une gouttiere, selection pleine.
//   (3 Ilots et 4 Synthese viendront sur CE mecanisme.)
//
// ⚠️ LA REVERSIBILITE N'EST PAS UNE PROMESSE, C'EST UNE PROPRIETE DU CALCUL.
//    `NkApparenceCalculer` est une FONCTION PURE de (apparence, theme, accent,
//    theme de base capture une fois au demarrage). L'application ecrit TOUT ce
//    qu'elle controle a chaque image (theme du dessin, theme du kit, NkCol,
//    reglages de la coquille) : rien ne « reste » d'une apparence quittee. Le
//    banc (NkBancApparences.h) le verifie octet par octet, et sa contre-epreuve
//    (NK_BANC_MUTATION=residu) laisse volontairement un residu pour le voir
//    rougir.
// =============================================================================
#include "NKCode/Shell/NkUi.h"
#include "NKEditorKit/NkTheme.h"

namespace nkentseu {
	namespace nkcode {

		static const int32 NK_APPARENCE_CLASSIQUE = 0;
		static const int32 NK_APPARENCE_NETTOYEE = 1;
		static const int32 NK_APPARENCE_FAMILLE = 2;
		static const int32 NK_APPARENCE_COUNT = 3;

		inline const char *NkApparenceNom(int32 id) {
			switch (id) {
				case NK_APPARENCE_NETTOYEE:
					return "Nettoy\xC3\xA9" "e";
				case NK_APPARENCE_FAMILLE:
					return "Famille";
				default:
					return "Classique";
			}
		}

		inline const char *NkApparenceDescription(int32 id) {
			switch (id) {
				case NK_APPARENCE_NETTOYEE:
					return "Sans rang\xC3\xA9" "e d'outils, une seule barre d'activit\xC3\xA9s, trois tons";
				case NK_APPARENCE_FAMILLE:
					return "La coquille d'UnkenyEditor : logo au coin, boutons bord\xC3\xA9s";
				default:
					return "La disposition d'avant, corrig\xC3\xA9" "e";
			}
		}

		/// LES ROLES DE FOND (et de trait, de texte). Un role, une couleur : c'est
		/// la correction « un role par fond » de la maquette A.
		struct NkApparencePalette {
				NkColor cadre;		 ///< barre de titre, barres d'activite, barre d'etat
				NkColor gouttiere;	 ///< ce qu'on voit ENTRE les panneaux
				NkColor panneau;	 ///< Explorateur, panneaux du bas, Assistant IA (le MEME)
				NkColor editeur;	 ///< la zone de code
				NkColor haut;		 ///< ce qui se pose sur un panneau (carte, champ en relief)
				NkColor saisie;		 ///< fond d'un champ de saisie
				NkColor bouton;		 ///< fond d'un bouton
				NkColor boutonBord;	 ///< contour d'un bouton borde (Famille)
				NkColor barreOnglets; ///< fond de la barre d'onglets de fichiers
				NkColor barreEtat;	 ///< fond de la barre d'etat
				NkColor trait;		 ///< contours, separateurs principaux
				NkColor trait2;		 ///< separateurs secondaires
				NkColor fg, fg2, fg3; ///< texte, secondaire, discret
				NkColor accent;		 ///< indicateurs (onglet actif, barre de selection)
				NkColor plein;		 ///< bouton principal, pastille pleine
				NkColor selection;	 ///< fond d'une ligne choisie
				NkColor selectionFg; ///< texte sur la selection
				NkColor survol;		 ///< fond survole
				bool clair = false;
		};

		/// LA DISPOSITION : ce que l'apparence demande a la coquille et aux panneaux.
		struct NkApparenceDispo {
				bool barreOutils = true;	  ///< la rangee d'outils sous les menus
				bool styleFamille = false;	  ///< rangee d'outils a boutons bordes
				bool activiteGauche = true;
				bool activiteDroite = true;
				bool ongletsLateraux = true; ///< onglet d'un panneau lateral SEUL
				float32 titreH = 0.f;		 ///< SetHeaderLayout (0 = calcul historique)
				float32 bandeH = 0.f;
				float32 logoCoin = 0.f;		 ///< bloc logo a cheval sur les deux rangees
				float32 barreEtatH = 0.f;	 ///< 0 = les 22 historiques
				/// En-tete de l'Explorateur : 0 = boutons seuls (son onglet porte le
				/// nom), 1 = « EXPLORATEUR » + boutons, 2 = « Explorateur ▾ » gras.
				int32 enteteExplorateur = 0;
				float32 ligneArbre = 0.f; ///< hauteur d'une ligne de l'arbre (0 = celle du theme)
				bool selectionPleine = false; ///< selection pleine (Famille) au lieu de douce + barre
		};

		/// TOUT ce qu'une apparence ecrit, a chaque image.
		struct NkApparenceEtat {
				int32 id = 0, theme = 0;
				NkApparencePalette pal;
				NkApparenceDispo dispo;
				NkGuiTheme gui;			  ///< ce que le dessin lit (ctx.theme)
				NkGuiSyntax syntaxe;	  ///< coloration du code
				NkThemePalette col;		  ///< NkCol::*
				editorkit::NkTheme kit;	  ///< ce que lisent les composants du kit (panneau IA)
		};

		// ── Petits outils de couleur ────────────────────────────────────────────
		inline NkColor NkHex(const char *h) {
			return NkParseHex(h, NkColor{255, 0, 255, 255});
		}
		/// `a` pose par-dessus `b` avec l'opacite `t` (0..1) -> couleur opaque.
		inline NkColor NkMelange(const NkColor &b, const NkColor &a, float32 t) {
			const float32 u = 1.f - t;
			return NkColor{(uint8)(b.r * u + a.r * t + 0.5f), (uint8)(b.g * u + a.g * t + 0.5f),
						   (uint8)(b.b * u + a.b * t + 0.5f), 255};
		}

		/// L'accent de l'utilisateur (« #0075CF » par defaut) en deux tons : le
		/// PLEIN tel quel, et l'INDICATEUR — eclairci en sombre (#3A9DE9 pour le
		/// bleu du logo), assombri en clair (#0067B8) : les deux valeurs des maquettes.
		inline void NkAccentTons(const char *hex, bool clair, NkColor &plein, NkColor &indicateur) {
			plein = NkParseHex(hex, NkColor{0, 117, 207, 255});
			indicateur = clair ? NkMelange(plein, NkColor{0, 0, 0, 255}, 0.12f)
							   : NkMelange(plein, NkColor{255, 255, 255, 255}, 0.24f);
		}

		// ── Les palettes ────────────────────────────────────────────────────────
		inline NkApparencePalette NkPaletteClassique(int32 theme, const char *accent) {
			const NkThemePalette p = NkThemePreset(theme);
			NkApparencePalette a;
			a.clair = NkThemeIdEstClair(theme);
			a.cadre = p.sidebar;
			a.gouttiere = p.sidebar;
			a.panneau = p.surface; // Explorateur, bas ET Assistant IA : le meme fond
			a.editeur = p.background;
			a.haut = p.input;
			a.saisie = p.background;
			a.bouton = p.muted;
			a.boutonBord = p.border;
			a.barreOnglets = p.sidebar;
			a.barreEtat = p.sidebar;
			a.trait = p.border;
			a.trait2 = p.border;
			a.fg = p.foreground;
			a.fg2 = p.sidebarFg;
			a.fg3 = p.mutedFg;
			NkAccentTons(accent, a.clair, a.plein, a.accent);
			a.selection = p.selection;
			a.selectionFg = p.foreground;
			a.survol = p.hover;
			return a;
		}

		inline NkApparencePalette NkPaletteNettoyee(int32 theme, const char *accent) {
			NkApparencePalette a;
			a.clair = NkThemeIdEstClair(theme);
			if (theme == 0 || theme == 3) { // les valeurs EXACTES de la maquette A (a.html)
				const bool c = a.clair;
				a.cadre = NkHex(c ? "#EEF1F4" : "#010409");
				a.panneau = NkHex(c ? "#F6F8FA" : "#0D1117");
				a.editeur = NkHex(c ? "#FFFFFF" : "#161B22");
				a.saisie = NkHex(c ? "#FFFFFF" : "#010409");
				a.trait = NkHex(c ? "#D0D7DE" : "#262C36");
				a.trait2 = NkHex(c ? "#E4E8EC" : "#1C2129");
				a.fg = NkHex(c ? "#1F2328" : "#E6EDF3");
				a.fg2 = NkHex(c ? "#59636E" : "#9DA7B3");
				a.fg3 = NkHex(c ? "#818B98" : "#6E7681");
			} else { // Dark, Midnight : les memes ROLES, tires du preset
				const NkThemePalette p = NkThemePreset(theme);
				a.cadre = p.sidebar;
				a.panneau = p.background;
				a.editeur = p.surface;
				a.saisie = p.sidebar;
				a.trait = p.border;
				a.trait2 = p.border;
				a.fg = p.foreground;
				a.fg2 = p.sidebarFg;
				a.fg3 = p.mutedFg;
			}
			a.gouttiere = a.cadre;
			a.haut = a.editeur;
			a.bouton = a.panneau;
			a.boutonBord = a.trait;
			a.barreOnglets = a.panneau;
			a.barreEtat = a.cadre;
			NkAccentTons(accent, a.clair, a.plein, a.accent);
			// --accent-doux : rgba(0,117,207,.24) en sombre, .13 en clair
			a.selection = NkMelange(a.panneau, a.plein, a.clair ? 0.13f : 0.24f);
			a.selectionFg = a.fg;
			a.survol = a.clair ? NkMelange(a.panneau, NkColor{31, 35, 40, 255}, 0.06f)
							   : NkMelange(a.panneau, NkColor{177, 186, 196, 255}, 0.08f);
			return a;
		}

		inline NkApparencePalette NkPaletteFamille(int32 theme, const char *accent) {
			NkApparencePalette a;
			a.clair = NkThemeIdEstClair(theme);
			if (theme == 0 || theme == 3) { // les valeurs EXACTES de la maquette B (b.html)
				const bool c = a.clair;
				a.gouttiere = NkHex(c ? "#D3D9E0" : "#010409");
				a.cadre = NkHex(c ? "#F3F5F8" : "#010409");
				a.panneau = NkHex(c ? "#FFFFFF" : "#0D1117");
				a.barreEtat = NkHex(c ? "#F3F5F8" : "#161B22");
				a.haut = NkHex(c ? "#FAFBFC" : "#11151B");
				a.saisie = NkHex(c ? "#FFFFFF" : "#0D1117");
				a.boutonBord = NkHex(c ? "#C9D1D9" : "#30363D");
				a.bouton = NkHex(c ? "#FFFFFF" : "#161B22");
				a.trait = NkHex(c ? "#E1E5EA" : "#21262D");
				a.fg = NkHex(c ? "#1F2328" : "#E6EDF3");
				a.fg2 = NkHex(c ? "#59636E" : "#9DA7B3");
				a.fg3 = NkHex(c ? "#818B98" : "#6E7681");
				a.selection = NkHex(c ? "#C7E0F8" : "#1C4C82");
				a.selectionFg = NkHex(c ? "#0B2540" : "#FFFFFF");
				a.barreOnglets = c ? a.cadre : a.gouttiere;
			} else {
				const NkThemePalette p = NkThemePreset(theme);
				a.gouttiere = p.sidebar;
				a.cadre = p.sidebar;
				a.panneau = p.background;
				a.barreEtat = p.surface;
				a.haut = p.input;
				a.saisie = p.background;
				a.boutonBord = p.border;
				a.bouton = p.surface;
				a.trait = p.border;
				a.fg = p.foreground;
				a.fg2 = p.sidebarFg;
				a.fg3 = p.mutedFg;
				a.selection = p.selection;
				a.selectionFg = p.foreground;
				a.barreOnglets = p.sidebar;
			}
			a.editeur = a.panneau; // B : l'editeur est un panneau comme les autres
			a.trait2 = a.trait;
			NkAccentTons(accent, a.clair, a.plein, a.accent);
			a.survol = a.clair ? NkMelange(a.panneau, NkColor{31, 35, 40, 255}, 0.06f)
							   : NkMelange(a.panneau, NkColor{177, 186, 196, 255}, 0.08f);
			return a;
		}

		/// Coloration du code : celle d'avant (VS Dark+/Light+) pour Classique,
		/// celle des maquettes (Primer) pour Nettoyee et Famille.
		inline NkGuiSyntax NkSyntaxePrimer(const NkGuiSyntax &base, bool clair) {
			NkGuiSyntax s = base;
			s.text = NkHex(clair ? "#1F2328" : "#E6EDF3");
			s.keyword = NkHex(clair ? "#CF222E" : "#FF7B72");
			s.type = NkHex(clair ? "#953800" : "#FFA657");
			s.function = NkHex(clair ? "#8250DF" : "#D2A8FF");
			s.string = NkHex(clair ? "#0A3069" : "#A5D6FF");
			s.number = NkHex(clair ? "#0550AE" : "#79C0FF");
			s.comment = NkHex(clair ? "#6E7781" : "#8B949E");
			s.preproc = NkHex(clair ? "#CF222E" : "#FF7B72");
			s.constant = NkHex(clair ? "#0550AE" : "#79C0FF");
			s.heading = s.keyword;
			s.mdcode = s.string;
			s.oper = s.text;
			return s;
		}

		/// LE CALCUL, PUR. `baseGui` / `baseSyntaxe` : le theme du dessin tel que
		/// la coquille l'a pose a l'Init, capture UNE fois (les champs qu'aucune
		/// apparence ne decide en viennent, toujours les memes).
		inline NkApparenceEtat NkApparenceCalculer(int32 id, int32 theme, const char *accent,
												   const NkGuiTheme &baseGui, const NkGuiSyntax &baseSyntaxe) {
			NkApparenceEtat e;
			if (id < 0 || id >= NK_APPARENCE_COUNT)
				id = NK_APPARENCE_CLASSIQUE;
			if (theme < 0 || theme >= NK_THEME_COUNT)
				theme = 0;
			e.id = id;
			e.theme = theme;
			const NkThemePalette preset = NkThemePreset(theme);
			NkApparencePalette &a = e.pal;
			NkApparenceDispo &d = e.dispo;
			switch (id) {
				case NK_APPARENCE_NETTOYEE:
					a = NkPaletteNettoyee(theme, accent);
					d.barreOutils = false;
					d.activiteDroite = false;
					d.ongletsLateraux = false;
					d.titreH = 36.f;
					d.barreEtatH = 24.f;
					d.enteteExplorateur = 1;
					d.ligneArbre = 24.f;
					break;
				case NK_APPARENCE_FAMILLE:
					a = NkPaletteFamille(theme, accent);
					d.barreOutils = true;
					d.styleFamille = true;
					d.activiteDroite = false;
					d.ongletsLateraux = false;
					d.titreH = 32.f;
					d.bandeH = 40.f;
					d.logoCoin = 72.f;
					d.barreEtatH = 26.f;
					d.enteteExplorateur = 2;
					d.ligneArbre = 26.f;
					d.selectionPleine = true;
					break;
				default:
					a = NkPaletteClassique(theme, accent);
					break; // la disposition d'avant : tous les defauts
			}

			// ── 1) LE THEME DU DESSIN (ctx.theme) — TOUS les champs de couleur ──
			NkGuiTheme &g = e.gui;
			g = baseGui;
			if (id == NK_APPARENCE_CLASSIQUE) {
				// Exactement ce que NkApplyEditorTheme posait avant le 01/10...
				g.bgPrimary = preset.background;
				g.panel = preset.surface;
				g.header = preset.sidebar;
				g.button = preset.muted;
				g.buttonHover = preset.hover;
				g.buttonActive = preset.primary;
				g.border = preset.border;
				g.text = preset.foreground;
				g.textDisabled = preset.mutedFg;
				g.selection = preset.selection;
				g.track = preset.muted;
				g.tabBar = preset.sidebar;
				g.tab = preset.surface;
				g.tabHover = preset.hover;
				g.tabActive = preset.background;
				// ... sauf l'ACCENT : celui de l'utilisateur (le bleu du logo par
				// defaut, a 2 % pres le primaire d'avant).
				g.accent = a.plein;
			} else {
				g.bgPrimary = a.editeur;
				g.panel = a.panneau;
				g.header = a.cadre;
				g.card = a.haut;
				g.button = a.bouton;
				g.buttonHover = NkMelange(a.bouton, a.fg, 0.08f);
				g.buttonActive = a.plein;
				g.border = a.trait;
				g.separator = a.trait2;
				g.text = a.fg;
				g.textDisabled = a.fg3;
				g.textMuted = a.fg2;
				g.selection = a.selection;
				g.accent = a.accent;
				g.track = a.saisie;
				g.tabBar = a.barreOnglets;
				g.tab = a.barreOnglets;
				g.tabHover = a.survol;
				g.tabActive = a.editeur;
				g.rowHover = a.survol;
				g.scrollbar = NkMelange(a.panneau, a.fg, 0.30f);
				g.scrollbarHover = NkMelange(a.panneau, a.fg, 0.50f);
				g.onAccent = NkColor{255, 255, 255, 255};
			}

			// ── 2) LA COLORATION ──
			{
				const bool clair = a.clair;
				NkGuiSyntax s = baseSyntaxe;
				if (clair) {
					s.text = {36, 41, 46, 255};
					s.keyword = {0, 0, 255, 255};
					s.type = {38, 127, 153, 255};
					s.string = {163, 21, 21, 255};
					s.comment = {0, 128, 0, 255};
					s.number = {9, 134, 88, 255};
					s.preproc = {175, 0, 219, 255};
					s.heading = {128, 0, 0, 255};
					s.mdcode = {163, 21, 21, 255};
					s.function = {121, 94, 38, 255};
					s.constant = {0, 92, 197, 255};
					s.oper = {90, 100, 110, 255};
				} else {
					s.text = {212, 212, 212, 255};
					s.keyword = {86, 156, 214, 255};
					s.type = {78, 201, 176, 255};
					s.string = {206, 145, 120, 255};
					s.comment = {106, 153, 85, 255};
					s.number = {181, 206, 168, 255};
					s.preproc = {197, 134, 192, 255};
					s.heading = {78, 201, 176, 255};
					s.mdcode = {206, 145, 120, 255};
					s.function = {225, 205, 120, 255};
					s.constant = {79, 193, 255, 255};
					s.oper = {200, 200, 200, 255};
				}
				e.syntaxe = (id == NK_APPARENCE_CLASSIQUE) ? s : NkSyntaxePrimer(s, clair);
			}

			// ── 3) LA PALETTE DE NKCODE (NkCol : launcher, reglages, dialogues, barre d'outils) ──
			{
				NkThemePalette &c = e.col;
				c = preset;
				c.accent = NkParseHex(accent, preset.accent);
				if (id != NK_APPARENCE_CLASSIQUE) {
					c.background = a.panneau;
					c.surface = a.haut;
					c.input = a.saisie;
					c.border = a.trait;
					c.sidebar = a.cadre;
					c.foreground = a.fg;
					c.sidebarFg = a.fg2;
					c.mutedFg = a.fg3;
					c.muted = a.bouton;
					c.hover = a.survol;
					c.selection = a.selection;
					c.primary = a.plein;
				}
			}

			// ── 4) LE THEME DU KIT (panneau IA, terminal partage...) : les MEMES roles ──
			{
				using editorkit::NkRole;
				editorkit::NkTheme &k = e.kit;
				k = a.clair ? editorkit::NkTheme::Light() : editorkit::NkTheme::Dark();
				k.Set(NkRole::WindowBg, a.cadre.ToUint32A());
				k.Set(NkRole::PanelBg, a.panneau.ToUint32A());
				k.Set(NkRole::PanelHeader, a.haut.ToUint32A());
				k.Set(NkRole::InputBg, a.saisie.ToUint32A());
				k.Set(NkRole::Border, a.trait.ToUint32A());
				k.Set(NkRole::Text, a.fg.ToUint32A());
				k.Set(NkRole::TextMuted, a.fg2.ToUint32A());
				k.Set(NkRole::TextOnAccent, NkColor{255, 255, 255, 255}.ToUint32A());
				k.Set(NkRole::AccentUi, a.plein.ToUint32A());
				k.Set(NkRole::AccentSel, NkParseHex(accent, a.plein).ToUint32A());
				k.Set(NkRole::ButtonBg, a.bouton.ToUint32A());
				k.Set(NkRole::TabBarBg, a.barreOnglets.ToUint32A());
				k.Set(NkRole::StatusErr, preset.danger.ToUint32A());
			}
			return e;
		}

		/// L'etat APPLIQUE (le dernier calcul) : lu par les panneaux de NKCode qui
		/// ont besoin d'un role que le theme du dessin ne porte pas (fond du terminal,
		/// en-tete de l'Explorateur...).
		inline NkApparenceEtat &NkApparenceCourante() {
			static NkApparenceEtat e = NkApparenceCalculer(0, 0, "#0075CF", NkGuiTheme{}, NkGuiSyntax{});
			return e;
		}

	} // namespace nkcode
} // namespace nkentseu
