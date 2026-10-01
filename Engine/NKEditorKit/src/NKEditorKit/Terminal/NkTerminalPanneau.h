#pragma once
// -----------------------------------------------------------------------------
// @File    NkTerminalPanneau.h
// @Brief   LE PANNEAU TERMINAL COMPLET, PARTAGE : plusieurs terminaux en
//          ONGLETS (icone du shell, fermer, renommer par double-clic), le menu
//          « + » qui liste les shells trouves sur la machine, l'en-tete (shell
//          + dossier courant), la grille, le menu contextuel. Un editeur le
//          pose dans un rectangle et appelle `Dessiner` -- c'est tout.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
//  Utilisation (UnkenyEditor, NkEditeurTerminal.cpp) :
//      static editorkit::NkTerminalPanneau t;
//      t.dossierDepart = <dossier du projet>;
//      t.Dessiner(ctx, ctx.dl, zone, theme);   // chaque image ou il est visible
//      t.Pomper();                              // chaque image, visible ou non
//      if (t.AFocus()) { ... l'editeur ne prend pas ses raccourcis ... }
//
//  NKCode garde SON panneau (liste a droite, recherche Ctrl+F, panneau
//  EXECUTION, glisser-deposer de chemins) mais en dessine la grille, l'en-tete
//  et le clavier PAR ICI : le meme rendu dans toute la famille.
//
//  ⚠️ CE QUE LE PANNEAU NE FAIT PAS : il n'ouvre pas les liens `fichier:ligne`
//     (il les RAPPORTE : `LienClique`), parce que seul l'editeur sait ce qu'est
//     « ouvrir un fichier » chez lui.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkEditorContextMenu.h"
#include "NKEditorKit/NkTheme.h"
#include "NKEditorKit/Terminal/NkTerminalEmulateur.h"
#include "NKEditorKit/Terminal/NkTerminalPty.h"
#include "NKEditorKit/Terminal/NkTerminalShells.h"
#include "NKEditorKit/Terminal/NkTerminalVue.h"

namespace nkentseu {
	namespace editorkit {

		// ── UNE SESSION : un shell vivant, son ecran, sa vue ─────────────────
		struct NkTerminalSession {
				nk_uint64 id = 0;
				NkPty pty;
				NkTerm ecran;
				NkTerminalVue vue;
				NkShellDecouvert shell;
				NkString titre;		   ///< le libelle de l'onglet
				bool renomme = false;  ///< renomme par l'utilisateur : le titre du shell ne l'ecrase plus
				NkString dossierDepart;
				bool demarre = false;
				bool echec = false;
				bool finAnnoncee = false;
				NkString aTaper;	   ///< a envoyer des que le shell a demarre (bancs, captures)
				NkVector<char> tampon; ///< drain reutilise

				/// Lance le shell a la taille donnee (une seule fois).
				bool Demarrer(int16 cols, int16 rows);
				/// Sortie du shell -> emulateur ; reponses de l'emulateur -> shell.
				/// Rend vrai s'il y avait du nouveau.
				bool Pomper();
				void Envoyer(const char *s, usize n);
				/// Redimensionne l'ecran ET le shell (s'ils different).
				void Redimensionner(int16 cols, int16 rows);
		};

		class NkTerminalPanneau {
			public:
				NkTerminalPanneau();
				~NkTerminalPanneau();
				NkTerminalPanneau(const NkTerminalPanneau &) = delete;
				NkTerminalPanneau &operator=(const NkTerminalPanneau &) = delete;

				// ── Reglages (a poser par l'hote) ─────────────────────────────
				NkString dossierDepart;		   ///< le dossier du projet : la ou les shells demarrent
				const nkgui::NkGuiFont *police = nullptr; ///< chasse fixe (nul : ctx.codeFont)
				NkTerminalGrilleStyle styleGrille;
				/// Ouvre le shell par defaut a la premiere apparition du panneau.
				bool ouvrirAuPremierAffichage = true;

				// ── Les shells de la machine ───────────────────────────────────
				/// (Re)decouvre. Appele de lui-meme a la premiere ouverture du menu.
				void Decouvrir(bool avecWsl = true);
				const NkVector<NkShellDecouvert> &Shells();

				// ── Les onglets ────────────────────────────────────────────────
				/// Ouvre un terminal du shell `indice` (-1 = par defaut) et l'active.
				/// Rend son indice, ou -1 si la plateforme n'a pas de shell.
				int32 Ouvrir(int32 indice = -1);
				int32 OuvrirShell(const NkShellDecouvert &s);
				void Fermer(int32 i);
				void FermerTout();
				void Renommer(int32 i, const NkString &titre);
				int32 Nombre() const {
					return static_cast<int32>(mSessions.Size());
				}
				NkTerminalSession *Session(int32 i) {
					return (i >= 0 && i < Nombre()) ? mSessions[static_cast<usize>(i)] : nullptr;
				}
				int32 Actif() const {
					return mActif;
				}
				void Activer(int32 i);

				// ── Chaque image ───────────────────────────────────────────────
				/// Fait avancer TOUS les shells, visibles ou non : un `jenga build`
				/// lance dans un onglet cache ne doit pas s'accumuler sans fin.
				void Pomper();

				/// Le terminal actif a-t-il le clavier ? L'editeur ne prend alors
				/// AUCUN de ses raccourcis (Suppr, Espace, Q/W/E/R...).
				bool AFocus() const;
				void PerdreFocus();
				/// Donne le clavier au terminal actif (comme un clic dans la grille).
				void PrendreFocus();

				/// Envoie `texte` au terminal actif (ou des son demarrage).
				void Taper(const char *texte);

				/// Dessine le panneau dans `zone`. `dl` : la liste principale.
				void Dessiner(nkgui::NkGuiContext &ctx, nkgui::NkGuiDrawList &dl, const nkgui::NkRect &zone,
							  const NkTheme &theme);

				// ── Le menu « + » (et son etat, pour les captures sans souris) ──
				void OuvrirMenuShells() {
					mMenuShells = true;
				}
				bool MenuShellsOuvert() const {
					return mMenuShells;
				}

				/// Le dernier lien `fichier:ligne` Ctrl+clique (consomme a la lecture).
				bool LienClique(NkString &fichier, int32 &ligne, int32 &colonne, bool &estUrl);

			private:
				void DessinerOnglets(nkgui::NkGuiContext &ctx, nkgui::NkGuiDrawList &dl, const nkgui::NkRect &bande,
									 const NkTheme &theme);
				void DessinerMenuShells(nkgui::NkGuiContext &ctx, const nkgui::NkRect &borne);
				void DessinerVide(nkgui::NkGuiContext &ctx, nkgui::NkGuiDrawList &dl, const nkgui::NkRect &zone,
								  const char *titre, const char *detail, bool bouton);

				NkVector<NkTerminalSession *> mSessions;
				int32 mActif = -1;
				nk_uint64 mProchainId = 1;
				NkVector<NkShellDecouvert> mShells;
				bool mShellsConnus = false;
				bool mWslConnus = false;
				bool mDejaAffiche = false;
				bool mMenuShells = false;
				nkgui::NkRect mAncrePlus = {0.f, 0.f, 0.f, 0.f};
				int32 mRenomme = -1;
				char mRenommeTexte[128] = {};
				bool mRenommeArme = false;
				nkgui::NkRect mRenommeRect = {0.f, 0.f, 0.f, 0.f};
				NkCtxMenu mMenuContexte;
				NkTerminalPalette mPalette;
				NkVector<char> mSortie;
				bool mLien = false;
				NkTerminalGrilleResultat mDernierLien;
				nkgui::NkRect mZone = {0.f, 0.f, 0.f, 0.f};
		};

	} // namespace editorkit
} // namespace nkentseu
