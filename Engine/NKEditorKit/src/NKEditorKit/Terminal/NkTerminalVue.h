#pragma once
// -----------------------------------------------------------------------------
// @File    NkTerminalVue.h
// @Brief   LE DESSIN D'UN TERMINAL : la grille (police a chasse fixe, marges,
//          curseur net qui clignote, selection, surlignages, liens
//          `fichier:ligne`), l'en-tete (shell + dossier courant), le clavier
//          (frappes -> sequences VT), et LA PALETTE accordee au theme.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI UN DESSIN NEUF (Rihen, 01/10)
// =============================================================================
//  « Le terminal de NKCode ne convient pas encore cote clarte et beaute face
//  aux autres applications. » Mesure sur la capture « avant » : texte colle au
//  bord, palette GitHub ecrite en dur (illisible en theme clair), curseur plein
//  sans clignotement, barre horizontale toujours visible pour rien, aucun
//  en-tete. Le MOTEUR (NkPty, NkTerm) etait bon ; le dessin est refait ici,
//  dans le kit, pour que NKCode, UnkenyEditor et les suivants aient LE MEME.
//
//  ⚠️ LES COULEURS NE SONT PAS ECRITES ICI, ELLES SONT DERIVEES DES ROLES DU
//     THEME (NkTerminalPaletteDuTheme). Les seize couleurs ANSI viennent des
//     roles d'etat (StatusErr, StatusOk, StatusWarn, AccentUi, AccentAI) et
//     sont ensuite GARANTIES lisibles contre le fond : une couleur trop
//     proche du fond est rapprochee du texte jusqu'a un contraste de 3:1. Un
//     theme que l'utilisateur ecrira demain obtient donc un terminal lisible
//     sans rien declarer.
//
//  OU AJOUTER QUELQUE CHOSE
//   • une couleur        -> un champ de NkTerminalPalette + sa derivation
//   • une touche         -> NkTerminalClavier (NkTerminalVue.cpp), table unique
//   • une forme de lien  -> NkTerminalLienSous (meme fichier)
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkTheme.h"
#include "NKEditorKit/Terminal/NkTerminalEmulateur.h"
#include "NKEditorKit/Terminal/NkTerminalShells.h"
#include "NKGui/NKGui.h"

namespace nkentseu {
	namespace editorkit {

		// ── LA PALETTE ─────────────────────────────────────────────────────────
		struct NkTerminalPalette {
				nkgui::NkColor ansi[16];
				nkgui::NkColor texte;		 ///< couleur par defaut du texte
				nkgui::NkColor fond;		 ///< fond de la grille
				nkgui::NkColor curseur;		 ///< bloc / barre du curseur
				nkgui::NkColor sousCurseur;	 ///< le glyphe SOUS un curseur bloc
				nkgui::NkColor selection;	 ///< voile de selection (translucide)
				nkgui::NkColor lien;		 ///< soulignement d'un lien `fichier:ligne`
				nkgui::NkColor trouve;		 ///< correspondance de recherche
				nkgui::NkColor trouveActif;	 ///< la correspondance courante
				nkgui::NkColor entete;		 ///< bande d'en-tete
				nkgui::NkColor enteteTexte;
				nkgui::NkColor attenue;		 ///< texte secondaire (dossier, taille)
				nkgui::NkColor bord;
				nkgui::NkColor erreur;
				bool sombre = true;
		};

		/// La palette d'un theme : 16 couleurs ANSI accordees aux jetons, lisibles
		/// en sombre ET en clair (contraste minimal garanti contre le fond).
		NkTerminalPalette NkTerminalPaletteDuTheme(const NkTheme &theme);

		/// La couleur RGBA d'une couleur de cellule (defaut, indice 0..255, RGB).
		nkgui::NkColor NkTerminalResoudre(const NkTerminalPalette &pal, uint32 couleur, bool estFond, bool gras);

		/// Rapport de contraste WCAG entre deux couleurs (1..21).
		float32 NkTerminalContraste(const nkgui::NkColor &a, const nkgui::NkColor &b);

		/// La couleur d'IDENTITE d'une famille de shell (pastille de l'onglet).
		nkgui::NkColor NkTerminalCouleurShell(NkShellGenre g);

		/// L'icone d'un shell : une pastille arrondie a sa couleur, et l'invite
		/// « >_ » TRACEE (le kit n'a pas d'atlas d'icones : ce qui doit se voir se
		/// trace).
		void NkTerminalDessinerIcone(nkgui::NkGuiDrawList &dl, const nkgui::NkRect &r, NkShellGenre g);

		// ── L'ETAT DE VUE D'UN TERMINAL ────────────────────────────────────────
		struct NkTerminalVue {
				float32 defil = 0.f;   ///< defilement vertical, en pixels depuis le haut
				bool suivre = true;	   ///< colle au bas (desactive par un defilement manuel)
				bool focus = false;	   ///< le clavier va-t-il au shell ?
				/// Selection en cellules : ancre A, extremite B (ligne ABSOLUE, colonne).
				int32 selAL = 0, selAC = 0, selBL = 0, selBC = 0;
				bool glisse = false;
				float32 derniereActivite = -10.f; ///< ctx.time de la derniere frappe (curseur plein)

				bool AUneSelection() const {
					return selAL != selBL || selAC != selBC;
				}
				void Deselectionner() {
					selAL = selBL = selAC = selBC = 0;
				}
		};

		/// Un surlignage (recherche) : `len` cellules depuis (ligne, col).
		struct NkTerminalSurlignage {
				int32 ligne = 0, col = 0, len = 0;
				bool courant = false;
		};

		struct NkTerminalGrilleStyle {
				/// La police a CHASSE FIXE. Nulle : `ctx.codeFont`, puis `ctx.font`
				/// (proportionnelle : les colonnes ne s'alignent plus, mais le texte
				/// reste lisible plutot que rien).
				const nkgui::NkGuiFont *police = nullptr;
				float32 marge = 10.f;	   ///< marge interieure, en pixels
				float32 interligne = 2.f;  ///< pixels ajoutes a la hauteur de ligne
				bool curseurClignote = true;
				bool barreDefilement = true;
		};

		struct NkTerminalGrilleResultat {
				int16 cols = 0, rows = 0; ///< la taille que la zone peut afficher
				bool survol = false;
				bool clicDedans = false;
				bool menuDemande = false; ///< clic droit dans la grille
				float32 menuX = 0.f, menuY = 0.f;
				/// Ctrl+clic sur un lien : `lienFichier` (et `lienLigne`/`lienColonne`
				/// pour un `fichier:ligne:col`), ou une URL (`lienEstUrl`).
				bool lienClique = false;
				bool lienEstUrl = false;
				NkString lienFichier;
				int32 lienLigne = 0, lienColonne = 0;
		};

		/// Dessine la grille de `term` dans `zone`. Ne touche PAS au shell : la
		/// taille que la zone peut afficher est RENDUE (`cols`, `rows`), et c'est
		/// l'appelant qui redimensionne l'emulateur et le pty.
		NkTerminalGrilleResultat NkTerminalDessinerGrille(nkgui::NkGuiContext &ctx, nkgui::NkGuiDrawList &dl,
														   const nkgui::NkRect &zone, const NkTerm &term,
														   NkTerminalVue &vue, const NkTerminalPalette &pal,
														   const NkTerminalGrilleStyle &style,
														   const NkTerminalSurlignage *surlignages = nullptr,
														   int32 nbSurlignages = 0);

		/// La taille (colonnes, lignes) qu'une zone peut afficher avec ce style.
		void NkTerminalTailleGrille(const nkgui::NkGuiContext &ctx, const nkgui::NkRect &zone,
									const NkTerminalGrilleStyle &style, int16 &cols, int16 &rows);

		/// L'en-tete : icone du shell, son nom, le dossier courant (attenue), et a
		/// droite un texte libre (moteur, taille). Rend la largeur prise a gauche.
		void NkTerminalDessinerEntete(nkgui::NkGuiContext &ctx, nkgui::NkGuiDrawList &dl, const nkgui::NkRect &bande,
									  const NkTerminalPalette &pal, NkShellGenre genre, const char *nom,
									  const char *dossier, const char *droite);

		// ── LE CLAVIER ─────────────────────────────────────────────────────────
		struct NkTerminalClavierOptions {
				/// Ctrl+A : « tout selectionner » (NKCode) au lieu d'aller au shell
				/// (debut de ligne pour readline). Defaut : au shell.
				bool ctrlAToutSelectionne = false;
		};

		/// Traduit les frappes de CETTE image en octets pour le shell (UTF-8 +
		/// sequences VT, modes de `term` respectes). Gere aussi copier (selection
		/// -> presse-papiers), coller (presse-papiers -> octets, encadre si le
		/// shell l'a demande) et tout selectionner. Rend vrai si quelque chose est
		/// a envoyer.
		bool NkTerminalClavier(nkgui::NkGuiContext &ctx, const NkTerm &term, NkTerminalVue &vue, NkVector<char> &sortie,
							   const NkTerminalClavierOptions &options = NkTerminalClavierOptions());

		/// Le texte du presse-papiers, pret pour le shell : fins de ligne en CR,
		/// encadre (ESC[200~ ... ESC[201~) si le shell a active le collage encadre.
		void NkTerminalPreparerCollage(const NkTerm &term, const NkString &texte, NkVector<char> &sortie);

		/// Le texte de la selection (UTF-8, espaces de fin retires par ligne).
		NkString NkTerminalTexteSelection(const NkTerm &term, const NkTerminalVue &vue);

		void NkTerminalToutSelectionner(const NkTerm &term, NkTerminalVue &vue);

		/// Le lien sous la cellule (ligne, col) : `chemin:ligne[:col]`,
		/// `chemin(ligne[,col])` (MSVC) ou une URL. Rend faux s'il n'y en a pas.
		/// `c0`/`c1` bornent le lien dans la ligne. Fonction pure (bancs).
		bool NkTerminalLienSous(const NkTerm &term, int32 ligne, int32 col, int32 &c0, int32 &c1,
								NkTerminalGrilleResultat &lien);

	} // namespace editorkit
} // namespace nkentseu
