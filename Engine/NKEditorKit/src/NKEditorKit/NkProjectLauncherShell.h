#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkProjectLauncherShell.h — le LANCEUR DE PROJETS dans une coquille
// NkEditorShell : l'ecran de demarrage qui occupe tout le corps de la fenetre
// (sous la barre de titre) tant qu'aucun projet n'est ouvert.
//
// CE QU'UNE APPLICATION ECRIT, ET C'EST TOUT :
//   static NkLanceurCoquille lanceur;
//   MaRemplirLanceur(lanceur.modele);      // sa touche : nom, logo, modeles...
//   lanceur.agir = &MonAgir;              // ce que fait chaque demande
//   lanceur.Brancher(*shell);             // SetStartScreen + plein corps
// `agir` rend VRAI quand un projet est desormais ouvert : le lanceur se ferme
// et la coquille rend l'editeur. `lanceur.Ouvrir()` le rappelle (« Fichier >
// Accueil »).
//
// ⚠️ LES DIALOGUES NATIFS (« Ouvrir... ») PARTENT A L'IMAGE SUIVANTE, AVANT LA
//    PEINTURE : un selecteur de l'OS ouvre une boucle modale, et l'ouvrir au
//    milieu de la peinture du lanceur reentrerait dans une liste a moitie
//    ecrite. Meme regle que le « apres la frame » de NKCraft.
//
// ⚠️ LE PLEIN CORPS PASSE PAR `appFullScreen`, le drapeau que la coquille lit
//    deja pour son ecran de demarrage (NKCode). Une application qui le pose
//    elle-meme a chaque image (son SetAppMenu) garde la main : le lanceur ne
//    se bat pas contre elle.
// =============================================================================

#include "NKEditorKit/NkProjectLauncherHost.h"
#include "NKEditorKit/NkEditorShell.h"

namespace nkentseu {
	namespace editorkit {

		class NkLanceurCoquille {
			public:
				NkProjectLauncherModel modele;
				NkProjectLauncherStyle style;
				NkProjectLauncherHooks crochets;
				NkLanceurPolices polices;

				/// Ce que l'application fait d'une demande. Rend VRAI si un projet
				/// est desormais ouvert (le lanceur se ferme). Ecrire une erreur dans
				/// `l.modele.erreur` pour la montrer au pied du lanceur.
				using Agir = bool (*)(void *user, NkLanceurCoquille &l, const NkProjectLauncherResult &r);
				Agir agir = nullptr;
				void *user = nullptr;
				/// Avant chaque image : l'application rafraichit ses donnees (les
				/// recents, l'etat des fichiers). Nul = donnees figees.
				void (*rafraichir)(void *user, NkLanceurCoquille &l) = nullptr;
				/// Vrai : le bouton de theme bascule la coquille entre NkTheme::Dark()
				/// et NkTheme::Light(). Faux : le lanceur suit le theme de
				/// l'application sans proposer de bascule (themes maison).
				bool themeParCoquille = false;

				void Brancher(NkEditorShell &s, bool ouvert = true) {
					mShell = &s;
					s.SetStartScreen(&Thunk, this);
					if (ouvert)
						Ouvrir();
				}
				void Ouvrir() {
					mActif = true;
					modele.erreur.Clear();
					if (mShell)
						mShell->Ui().appFullScreen = true;
				}
				void Fermer() {
					mActif = false;
					if (mShell)
						mShell->Ui().appFullScreen = false;
				}
				bool Actif() const {
					return mActif;
				}
				NkEditorShell *Coquille() const {
					return mShell;
				}

			private:
				static void Thunk(NkEditorFrameContext &ec, void *u) {
					static_cast<NkLanceurCoquille *>(u)->Peindre(ec);
				}

				void Executer(const NkProjectLauncherResult &r) {
					if (r.action == NkLanceurAction::BasculerTheme && themeParCoquille && mShell) {
						mShell->ApplyTheme(mShell->KitTheme().IsDark() ? NkTheme::Light() : NkTheme::Dark());
						return;
					}
					if (agir && agir(user, *this, r))
						Fermer();
				}

				void Peindre(NkEditorFrameContext &ec) {
					if (!mShell || !ec.ui)
						return;
					nkgui::NkGuiContext &ctx = *ec.ui;
					if (!mActif) {
						ctx.appFullScreen = false;
						return;
					}
					const float32 ech = mShell->DpiScale();
					if (polices.echelle != ech)
						(void)polices.Charger(ech, mShell->Renderer());
					if (mDiffere) { // le dialogue natif, AVANT toute peinture
						mDiffere = false;
						Executer(mResDiffere);
						if (!mActif)
							return;
					}
					if (rafraichir)
						rafraichir(user, *this);
					// NK_LANCEUR_CHOIX=modele:<i> | recent:<i> | nouveau : la demande
					// est jouee UNE fois, comme si on avait clique (instrument de
					// sonde : aucune entree reelle n'est simulee). Sans la variable,
					// rien ne change.
					if (!mChoixJoue) {
						mChoixJoue = true;
						if (const char *v = std::getenv("NK_LANCEUR_CHOIX")) {
							NkProjectLauncherResult c;
							const char *deux = v;
							while (*deux && *deux != ':')
								++deux;
							c.index = *deux ? (int32)std::atoi(deux + 1) : 0;
							if (v[0] == 'm')
								c.action = NkLanceurAction::NouveauDepuisModele;
							else if (v[0] == 'r')
								c.action = NkLanceurAction::OuvrirRecent;
							else if (v[0] == 'n')
								c.action = NkLanceurAction::NouveauProjet;
							std::printf("[lanceur] NK_LANCEUR_CHOIX=%s joue\n", v);
							std::fflush(stdout);
							Executer(c);
							if (!mActif)
								return;
						}
					}
					modele.themeSombre = mShell->KitTheme().IsDark();
					modele.themeBasculable = modele.themeBasculable || themeParCoquille;
					const float32 haut = ctx.titleBarH > 1.f ? ctx.titleBarH : 0.f;
					const NkPaintRect r{0.f, haut, (float32)ctx.viewW, (float32)ctx.viewH - haut};
					const NkProjectLauncherResult res =
						NkLanceurPeindre(ctx, mShell->KitTheme(), polices, r, modele, style, crochets, ech, true);
					if (!mDit) { // une ligne, une fois : le journal dit que l'ecran est la
						mDit = true;
						std::printf("[lanceur] %s : ecran de demarrage %.0fx%.0f, %d projet(s), %d modele(s)\n",
									modele.identite.nom.CStr(), (double)r.w, (double)r.h, (int)modele.projets.Size(),
									(int)modele.modeles.Size());
						std::fflush(stdout);
					}
					if (res.action == NkLanceurAction::Aucune)
						return;
					if (res.action == NkLanceurAction::Ouvrir) {
						mResDiffere = res;
						mDiffere = true;
						return;
					}
					Executer(res);
				}

				NkEditorShell *mShell = nullptr;
				bool mActif = false;
				bool mDiffere = false;
				bool mDit = false;
				bool mChoixJoue = false;
				NkProjectLauncherResult mResDiffere;
		};

	} // namespace editorkit
} // namespace nkentseu
