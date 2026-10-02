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
//   3 Synthese   maquette D (choix de Rihen, 01/10 : « je kiffe tout », PAR DEFAUT) :
//                la base Ilots (panneaux arrondis detaches sur un fond), UNE barre
//                en haut avec la barre d'outils complete de Famille (≡, Workspace,
//                Branche, Partager, palette Ctrl K, Cible ▶, Reglages), une bande
//                verticale (extensions en haut, fenetres d'outils en bas), les
//                segments de vues en tete de l'ilot de gauche, la page d'accueil
//                de A, les icones Pastilles.
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
		static const int32 NK_APPARENCE_SYNTHESE = 3;
		static const int32 NK_APPARENCE_COUNT = 4;
		/// L'apparence d'un NKCode neuf (Rihen, 01/10 : la Synthese remplace l'existant).
		static const int32 NK_APPARENCE_DEFAUT = NK_APPARENCE_SYNTHESE;

		inline const char *NkApparenceNom(int32 id) {
			switch (id) {
				case NK_APPARENCE_NETTOYEE:
					return "Nettoy\xC3\xA9" "e";
				case NK_APPARENCE_FAMILLE:
					return "Famille";
				case NK_APPARENCE_SYNTHESE:
					return "Synth\xC3\xA8" "se";
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
				case NK_APPARENCE_SYNTHESE:
					return "\xC3\x8Elots, une seule barre en haut, bande d'outils \xC3\xA0 gauche";
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
				// ── Synthese (maquette D) ──
				bool synthese = false;		  ///< la barre « Synthese » dans la barre de titre, la bande, les segments...
				bool ilots = false;			  ///< panneaux arrondis detaches
				float32 ilotRayon = 0.f;	  ///< rayon des ilots (px)
				float32 dockEcart = 0.f;	  ///< ecart entre deux ilots (0 = l'historique)
				float32 dockMarges[4] = {0.f, 0.f, 0.f, 0.f}; ///< gauche, haut, droite, bas
				float32 barreActiviteL = 0.f; ///< largeur de la bande de gauche (0 = 48 historiques)
				bool titreCentre = true;	  ///< le nom du document au centre de la barre de titre
				bool menusVisibles = false;	  ///< Synthese : la barre de menus TOUJOURS visible (variante 3)
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

		/// LA SYNTHESE (maquette D, ilots.css) : un fond, des ilots, un ton « haut »
		/// pour ce qui se pose dessus. Les traits sont des voiles (rgba) : on les
		/// compose ici sur l'ilot.
		inline NkApparencePalette NkPaletteSynthese(int32 theme, const char *accent) {
			NkApparencePalette a;
			a.clair = NkThemeIdEstClair(theme);
			const bool c = a.clair;
			if (theme == 0 || theme == 3) {
				a.gouttiere = NkHex(c ? "#E5E8ED" : "#0B0D11");
				a.panneau = NkHex(c ? "#FFFFFF" : "#14171D");
				a.haut = NkHex(c ? "#F1F3F6" : "#1C2027");
				a.fg = NkHex(c ? "#1D2128" : "#E4E8EE");
				a.fg2 = NkHex(c ? "#5A6370" : "#A0A8B3");
				a.fg3 = NkHex(c ? "#8A929D" : "#6C7480");
			} else {
				const NkThemePalette p = NkThemePreset(theme);
				a.gouttiere = p.sidebar;
				a.panneau = p.background;
				a.haut = p.surface;
				a.fg = p.foreground;
				a.fg2 = p.sidebarFg;
				a.fg3 = p.mutedFg;
			}
			const NkColor voile = c ? NkColor{15, 23, 42, 255} : NkColor{255, 255, 255, 255};
			a.cadre = a.gouttiere;
			a.editeur = a.panneau;
			a.saisie = a.haut;
			a.bouton = a.panneau;
			a.boutonBord = NkMelange(a.panneau, voile, c ? 0.09f : 0.07f);
			a.barreOnglets = a.panneau;
			a.barreEtat = a.gouttiere;
			a.trait = NkMelange(a.panneau, voile, c ? 0.09f : 0.07f);
			a.trait2 = NkMelange(a.panneau, voile, c ? 0.06f : 0.045f);
			NkAccentTons(accent, c, a.plein, a.accent);
			a.selection = NkMelange(a.panneau, a.plein, c ? 0.11f : 0.22f);
			a.selectionFg = a.fg;
			a.survol = NkMelange(a.panneau, voile, 0.05f);
			return a;
		}

		/// Coloration « One » (maquette D, ilots.css : --s-*).
		inline NkGuiSyntax NkSyntaxeOne(const NkGuiSyntax &base, bool clair) {
			NkGuiSyntax s = base;
			s.text = NkHex(clair ? "#383A42" : "#C3CAD4");
			s.keyword = NkHex(clair ? "#A626A4" : "#C678DD");
			s.type = NkHex(clair ? "#B07A00" : "#E5C07B");
			s.function = NkHex(clair ? "#3A6FE0" : "#61AFEF");
			s.string = NkHex(clair ? "#4A9A4A" : "#98C379");
			s.number = NkHex(clair ? "#986801" : "#D19A66");
			s.comment = NkHex(clair ? "#A0A1A7" : "#7F848E");
			s.preproc = NkHex(clair ? "#A626A4" : "#C678DD");
			s.constant = NkHex(clair ? "#0184BC" : "#56B6C2");
			s.heading = s.keyword;
			s.mdcode = s.string;
			s.oper = s.text;
			return s;
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
												   const NkGuiTheme &baseGui, const NkGuiSyntax &baseSyntaxe,
												   bool menusVisibles = false) {
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
				case NK_APPARENCE_SYNTHESE:
					a = NkPaletteSynthese(theme, accent);
					d.synthese = true;
					d.barreOutils = menusVisibles; // variante 3 : la barre Synthese descend sous les menus
					d.activiteDroite = false;
					d.ongletsLateraux = false;
					d.titreH = menusVisibles ? 32.f : 50.f;
					d.bandeH = menusVisibles ? 46.f : 0.f;
					d.barreEtatH = 28.f;
					d.enteteExplorateur = 3;
					d.ligneArbre = 27.f;
					d.ilots = true;
					d.ilotRayon = 12.f;
					d.dockEcart = 8.f;
					d.dockMarges[0] = 8.f;
					d.dockMarges[2] = 8.f;
					d.barreActiviteL = 48.f;
					d.titreCentre = menusVisibles;
					d.menusVisibles = menusVisibles;
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
				e.syntaxe = (id == NK_APPARENCE_CLASSIQUE)  ? s
							: (id == NK_APPARENCE_SYNTHESE) ? NkSyntaxeOne(s, clair)
															: NkSyntaxePrimer(s, clair);
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
