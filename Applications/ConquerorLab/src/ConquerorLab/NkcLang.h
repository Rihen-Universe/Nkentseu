#pragma once
// =============================================================================
// NkcLang — français / anglais.
//
// LE CHOIX DE CONCEPTION : LA CLE EST LE TEXTE FRANÇAIS
// -----------------------------------------------------
// Pas de `T("board.new_game")`. On écrit `T("Nouvelle partie")`, et la table dit
// comment le rendre en anglais. Trois raisons :
//
//   1. le code reste LISIBLE — un identifiant abstrait oblige à ouvrir la table
//      pour savoir ce que le bouton affiche ;
//   2. la migration est incrémentale : une chaîne non encore traduite s'affiche
//      en français, ce qui est correct pour la moitié des utilisateurs et
//      jamais un `##missing_key##` ;
//   3. l'atelier était déjà entièrement en français : le français est donc
//      exact par construction, et seul l'anglais est à écrire.
//
// LE COUT, ET POURQUOI IL EST ACCEPTABLE ICI
// ------------------------------------------
// La recherche est linéaire sur la table. Pour quelques centaines d'entrées
// appelées quelques centaines de fois par image, c'est mesurable en microsecondes
// — et une table de hachage ajouterait un ordre d'initialisation à surveiller
// pour gagner ce qui ne se voit pas. Si un jour l'atelier affiche des milliers de
// libellés par image, ce sera le moment de changer, pas avant.
//
// CE QUI N'EST PAS TRADUIT, ET C'EST VOULU
// ----------------------------------------
//   - les noms de modules et de fichiers : ils viennent du stagiaire ;
//   - la sortie du compilateur : elle vient de clang ;
//   - les traces des modules : elles viennent du code du stagiaire.
// Traduire ce qu'on n'a pas écrit, c'est mentir sur la source.
// =============================================================================

#include "NKCore/NkTypes.h"

#include <cstring>

namespace nkentseu {
	namespace conqueror {

		enum class NkcLangKind : uint8 { Fr = 0, En = 1 };

		inline NkcLangKind &NkcCurrentLang() noexcept {
			static NkcLangKind l = NkcLangKind::Fr;
			return l;
		}

		inline void NkcSetLang(NkcLangKind l) noexcept { NkcCurrentLang() = l; }

		struct NkcPhrase {
				const char *fr;
				const char *en;
		};

		/// La table. Ordre indifférent ; les doublons sont sans effet.
		inline const NkcPhrase *NkcPhrases(int32 &count) noexcept {
			static const NkcPhrase kT[] = {
				// ---- menus -------------------------------------------------
				{"Fichier",							"File"},
				{"Conqueror",						"Conqueror"},
				{"Theme",							"Theme"},
				{"Langue",							"Language"},
				{"Sombre",							"Dark"},
				{"Clair",							"Light"},
				{"Francais",						"French"},
				{"Anglais",							"English"},
				{"Reinitialiser la disposition",	"Reset layout"},
				{"Quitter",							"Quit"},
				{"Nouvelle partie",					"New game"},
				{"Pause",							"Pause"},
				{"Lecture",							"Play"},
				{"Un coup",							"One move"},
				{"Revenir a la position vivante",	"Back to live position"},
				{"Rechercher et compiler les modules", "Scan and build modules"},

				// ---- panneaux ----------------------------------------------
				{"Plateau",							"Board"},
				{"Regles",							"Rules"},
				{"Joueurs",							"Players"},
				{"Modules",							"Modules"},
				{"Journal",							"Log"},
				{"Metriques",						"Metrics"},
				{"Sortie",							"Output"},

				// ---- barre du plateau --------------------------------------
				{"Pas a pas",						"Step"},
				{"IA vs IA",						"AI vs AI"},
				{"Passer",							"Pass"},
				{"Siege : Humain",					"Seat: Human"},
				{"Siege : IA",						"Seat: AI"},
				{"Voisinage",						"Neighbours"},
				{"Voisinage : on",					"Neighbours: on"},
				{"Recadrer",						"Reframe"},
				{"Humain",							"Human"},

				// ---- raisons d'attente (barre d'etat) ----------------------
				{"aucun moteur de regles jouable",	"no playable rules engine"},
				{"l'IA reflechit",					"the AI is thinking"},
				{"rejeu en pause — revenir a la position vivante",
				 "replay paused — back to live position"},
				{"partie terminee",					"game over"},
				{"au tour du joueur humain — clique un totem",
				 "human player's turn — click a totem"},
				{"en pause — Lecture ou Pas a pas", "paused — Play or Step"},

				// ---- panneau Regles ----------------------------------------
				{"Grille",							"Board file"},
				{"Forme des cellules",				"Cell shape"},
				{"Selon le plateau",				"From the board file"},
				{"Automatique",						"Automatic"},
				{"Carree",							"Square"},
				{"Hexagonale",						"Hexagonal"},
				{"Ronde",							"Round"},
				{"Rafraichir",						"Refresh"},
				{"Exporter le plateau courant",		"Export current board"},

				// ---- panneau Joueurs ---------------------------------------
				{"Pilote",							"Driver"},
				{"Palier",							"Level"},
				{"Totem",							"Totem"},
				{"Disque colore",					"Coloured disc"},
				{"Recharger les totems",			"Reload totems"},
				{"Depose tes images ici",			"Drop your images here"},
				{"Facile",							"Easy"},
				{"Normal",							"Normal"},
				{"Difficile",						"Hard"},
				{"Expert",							"Expert"},
				{"Apex",							"Apex"},

				// ---- panneau Metriques -------------------------------------
				{"Parties",							"Games"},
				{"Threads",							"Threads"},
				{"Budget IA (ms)",					"AI budget (ms)"},
				{"Inverser les cotes une partie sur deux",
				 "Swap sides every other game"},
				{"Lancer la campagne",				"Start campaign"},
				{"Interrompre",						"Stop"},

				// ---- panneau Sortie ----------------------------------------
				{"Vider",							"Clear"},
				{"Niveau minimal",					"Minimum level"},
				{"Rien pour l'instant.",			"Nothing yet."},

				// ---- journal -----------------------------------------------
				{"Debut",							"Start"},
				{"Position vivante",				"Live position"},
				{"Copier",							"Copy"},
				{"Coup",							"Move"},
				{"Relancer la partie",				"Restart game"},
				{"Valeurs par defaut",				"Default values"},
				{"Lecture automatique",				"Auto play"},
				{"Aucune partie.",					"No game."},
				{"Aucun moteur de regles charge.",	"No rules engine loaded."},
				{"Aucun moteur de regles jouable — voir le panneau Modules.",
				 "No playable rules engine — see the Modules panel."},
				{"Journal des modules indisponible.","Module log unavailable."},
				{"Catalogue indisponible.",			"Catalogue unavailable."},
				{"DUPLIQUER",						"DUPLICATE"},
				{"FUSIONNER",						"FUSE"},
				{"POUVOIR",							"POWER"},
				{"PASSER",							"PASS"},
			};
			count = static_cast<int32>(sizeof(kT) / sizeof(kT[0]));
			return kT;
		}

		/// Traduit. Rend `fr` tel quel si la langue est le français, ou si la
		/// chaîne n'est pas dans la table — jamais de marqueur d'erreur à l'écran.
		inline const char *T(const char *fr) noexcept {
			if (!fr || NkcCurrentLang() == NkcLangKind::Fr) return fr;
			int32				 n = 0;
			const NkcPhrase *const t = NkcPhrases(n);
			for (int32 i = 0; i < n; ++i)
				if (std::strcmp(t[i].fr, fr) == 0) return t[i].en;
			return fr;
		}

	} // namespace conqueror
} // namespace nkentseu
