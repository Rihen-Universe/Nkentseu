// -----------------------------------------------------------------------------
// @File    NKScenaLanceur.cpp
// @Brief   LE LANCEUR DE NKSCENA : le composant PARTAGE de la famille
//          (NKEditorKit/Components/NkProjectLauncherModel.h, facon Unreal 5 /
//          Unity Hub) avec la touche de NKScena -- son nom, son accent rouge
//          « antenne », son glyphe (la camera, le logo provisoire du lanceur), ses
//          sequences recentes, ses modeles de depart.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// IL S'OUVRE au lancement interactif (sans --sequence, --scene, --exemple) et
// par Fichier > Accueil. Il occupe toute la fenetre, comme la fenetre de
// projets d'Unreal : c'est une MODALE de la trame de la famille (Modale() /
// PeindreModale), avec ses propres boutons de fenetre (`chromeFenetre`, la
// fenetre est sans cadre). Ce qu'il ouvre passe par les MEMES actions que les
// menus (NkScenaModele::Exemple, Ouvrir, OuvrirScene).
//
// LES EXTENSIONS (CONVENTIONS_FICHIERS.md §6) : il ouvre des sequences `.nkseq`,
// qui POINTENT vers leur scene `.nkscene3d` (le projet partage `.nknoge` de la
// famille 3D viendra avec le lanceur commun ; il n'est pas invente ici).
// -----------------------------------------------------------------------------

#include "NKScena/NKScenaInterface.h"
#include "NKScena/NKScenaLogo.h"

#include "NKEditorKit/NkFilePickerNav.h"
#include "NKFileSystem/NkFile.h"

namespace nkentseu {
	namespace nkscena {

		using namespace editorkit;
		using G = NkLanceurGlyphe;

		namespace {
			void LogoDuLanceur(void *u, NkComponentPaint &, const NkPaintRect &r) {
				// Le clap de la barre de titre, dans le carre du lanceur : une seule
				// marque, ou qu'elle paraisse. Le peintre NKGui dessine dans la liste
				// COURANTE du contexte ; le clap y va aussi.
				NkScenaInterface &ui = *static_cast<NkScenaInterface *>(u);
				const float32 cote = r.w < r.h ? r.w : r.h;
				NkScenaDessinerLogo(ui.Gui().DL(), r.x + (r.w - cote) * 0.5f, r.y + (r.h - cote) * 0.5f, cote,
									NkFamilleFondSombre(ui.pal.fond));
			}
		} // namespace

		void NkScenaInterface::PreparerLanceur() {
			NkProjectLauncherModel &m = mLanceur;
			m.identite.nom = NkString("NKScena");
			m.identite.prefixe = NkString("NK");
			m.identite.sousTitre = NkString("Le temps sur une scène — Nkentseu");
			m.identite.version = NkString("0.1.0");
			m.identite.extensions = NkString(".nkseq");
			m.identite.motProjet = NkString("projet");
			m.identite.motProjets = NkString("projets");
			m.identite.accent = kNkScenaAccent;
			m.identite.glyphe = G::Camera;
			m.chromeFenetre = true;
			m.themeBasculable = true;
			m.ouvrirPossible = true;
			m.modeles.Clear();
			struct D {
					const char *nom, *cat, *desc;
					G glyphe;
			};
			const D kModeles[] = {
				{"Exemple : le joueur traverse", "EXEMPLE",
				 "La scène de NogeDemo, un plan caméra, le joueur et la caméra animés. Espace pour jouer, « Rendre » pour le film.",
				 G::Camera},
				{"Séquence vide", "NOUVEAU", "La scène de démonstration (NogeDemo) et une frise vide de 5 s : « + Piste » pour commencer.",
				 G::Horloge},
				{"Sur une scène…", "NOUVEAU", "Choisir une scène .nkscene3d (Nogee, NogeDemo), puis poser le temps dessus.",
				 G::Paysage},
			};
			for (const D &d : kModeles) {
				NkLanceurModele md;
				md.nom = NkString(d.nom);
				md.categorie = NkString(d.cat);
				md.description = NkString(d.desc);
				md.glyphe = d.glyphe;
				m.modeles.PushBack(md);
			}
			m.pages.Clear();
			NkLanceurPage projets;
			projets.libelle = NkString("Projets");
			projets.glyphe = G::Projets;
			m.pages.PushBack(projets);
			m.astuce = NkString("Astuce : Espace joue, I pose une clé, 0 regarde par la caméra du plan, Ctrl+R rend le film.");
			m.piedDePage = NkString("Ouvre les séquences .nkseq ; chaque séquence pointe vers sa scène .nkscene3d.");
			mRecents.Charger("NKScena");
			// Les polices du lanceur (Inter, trois tailles), televersees par le
			// backend de l'interface (identifiants 'NL' : loin de celles de la famille).
			if (mPolicesLanceur.Charger(1.f, nullptr) && mBackendPret) {
				nkgui::NkGuiFont *p[3] = {&mPolicesLanceur.titre, &mPolicesLanceur.intertitre, &mPolicesLanceur.petite};
				for (nkgui::NkGuiFont *f : p) {
					if (f->Valid() && f->pixels != nullptr) {
						mBackend.UploadTextureGray8(f->TexId(), f->pixels, f->atlasW, f->atlasH);
					}
				}
			}
			mLanceurActif = mHote.lanceur;
		}

		void NkScenaInterface::ToucherRecent() {
			// Une sequence OUVERTE ou ENREGISTREE (un fichier qui existe, non
			// modifie) entre en tete des recents.
			const NkScenaModele &m = M();
			if (m.modifie || m.chemin == mRecentTouche || !NkFile::Exists(m.chemin.CStr())) {
				return;
			}
			mRecentTouche = m.chemin;
			mRecents.Toucher(m.chemin, m.nom, NkLanceurAujourdhui());
		}

		void NkScenaInterface::PeindreLanceur(NkFamilleCtx &c) {
			NkScenaModele &m = M();
			mRecents.Remplir(mLanceur);
			mLanceur.themeSombre = !Clair();
			mLanceur.fenetreMaximisee = fenetre.agrandie;
			NkProjectLauncherHooks crochets;
			crochets.peindreLogo = &LogoDuLanceur;
			crochets.user = this;
			const NkRect &e = plan.ecran;
			const NkProjectLauncherResult r =
				NkLanceurPeindre(Gui(), theme, mPolicesLanceur, NkPaintRect{e.x, e.y, e.w, e.h}, mLanceur, NkProjectLauncherStyle(), crochets);
			if (r.curseurMain) {
				Gui().wantCursor = nkgui::NkGuiCursor::Hand;
			}
			switch (r.action) {
				case NkLanceurAction::NouveauProjet:
					(void)m.SceneDemo();
					m.NouvelleSequence();
					mLanceurActif = false;
					break;
				case NkLanceurAction::NouveauDepuisModele:
					if (r.index == 0) {
						(void)m.Exemple();
						mLanceurActif = false;
					} else if (r.index == 1) {
						(void)m.SceneDemo();
						m.NouvelleSequence();
						m.Annoncer("Séquence vide sur la scène de démonstration : « + Piste » pour commencer");
						mLanceurActif = false;
					} else if (r.index == 2) {
						mLanceurActif = false;
						Executer(SCENA_A_OUVRIR_SCENE);
					}
					break;
				case NkLanceurAction::Ouvrir:
					mLanceurActif = false;
					Executer(SCENA_A_OUVRIR_SEQUENCE);
					break;
				case NkLanceurAction::OuvrirRecent:
					if (r.index >= 0 && r.index < static_cast<int32>(mLanceur.projets.Size())) {
						const NkString chemin = mLanceur.projets[static_cast<uint32>(r.index)].chemin;
						if (m.Ouvrir(chemin.CStr())) {
							mLanceurActif = false;
						} else {
							mLanceur.erreur = NkString("Cette séquence ne s'ouvre pas : ") + chemin;
						}
					}
					break;
				case NkLanceurAction::Epingler:
					if (r.index >= 0 && r.index < static_cast<int32>(mLanceur.projets.Size())) {
						const uint32 k = mLanceur.projets[static_cast<uint32>(r.index)].hote;
						if (k < mRecents.entrees.Size()) {
							mRecents.entrees[k].epingle = !mRecents.entrees[k].epingle;
							mRecents.Enregistrer();
						}
					}
					break;
				case NkLanceurAction::Retirer:
					if (r.index >= 0 && r.index < static_cast<int32>(mLanceur.projets.Size())) {
						mRecents.Retirer(mLanceur.projets[static_cast<uint32>(r.index)].hote);
					}
					break;
				case NkLanceurAction::Purger:
					for (int32 k = static_cast<int32>(mRecents.entrees.Size()) - 1; k >= 0; --k) {
						if (!NkFile::Exists(mRecents.entrees[static_cast<uint32>(k)].chemin.CStr())) {
							mRecents.entrees.Erase(mRecents.entrees.Begin() + k);
						}
					}
					mRecents.Enregistrer();
					break;
				case NkLanceurAction::BasculerTheme:
					PoserTheme(!Clair());
					break;
				case NkLanceurAction::FenetreReduire:
					fenetre.reduireDemande = true;
					break;
				case NkLanceurAction::FenetreAgrandir:
					fenetre.agrandirDemande = true;
					break;
				case NkLanceurAction::FenetreFermer:
					Fermer();
					break;
				case NkLanceurAction::FenetreGlisser:
					fenetre.deplacerDemande = true;
					fenetre.deplacerFractionX = e.w > 1.f ? (Gui().input.mousePos.x - e.x) / e.w : 0.5f;
					break;
				default:
					break;
			}
			(void)c;
		}

	} // namespace nkscena
} // namespace nkentseu
