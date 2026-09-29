// -----------------------------------------------------------------------------
// FICHIER: Editeur/NkEditeurApp.cpp
// DESCRIPTION: L'assemblage de l'editeur sur NKEditorKit.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Editeur/NkEditeurApp.h"

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurViseur.h"
#include "NKEditorKit/NkEditorCanvasRenderer.h"
#include "NKLogger/NkLog.h"
#include "Unkeny/Banc/NkUnkenyBanc.h"

namespace nkentseu {
	namespace editeur {

		using editorkit::NkEditorShell;
		using editorkit::NkEditorShellConfig;

		namespace {

			// Les commandes de la palette (Ctrl+Maj+P) et du menu.
			//
			// ⚠️ Une commande recoit un `void *` : c'est le prix d'une signature
			// C simple, et il se paie en vigilance -- on ne lui passe QUE
			// l'objet qu'elle attend, et rien d'autre ne partage ce pointeur.
			void CmdCadrer(void *u) {
				if (u != nullptr) {
					static_cast<NkPanneauViseur *>(u)->CadrerSurTout();
				}
			}

			NkEditeurModele &M(void *u) {
				return *static_cast<NkEditeurModele *>(u);
			}
			void CmdSupprimer(void *u) {
				if (u != nullptr) {
					NkEditeurSupprimerSelection(M(u));
				}
			}
			void CmdDupliquer(void *u) {
				if (u != nullptr) {
					NkEditeurDupliquer(M(u));
				}
			}
			void CmdNouvelleEntite(void *u) {
				if (u != nullptr) {
					NkEditeurCreerEntite(M(u), "Entite", M(u).scene.Camera().Centre());
				}
			}
			void CmdJouerPause(void *u) {
				if (u != nullptr) {
					M(u).etat == NkEtatJeu::NK_JEU ? NkEditeurPause(M(u)) : NkEditeurJouer(M(u));
				}
			}
			void CmdArreter(void *u) {
				if (u != nullptr) {
					NkEditeurArreter(M(u));
				}
			}
			void CmdPas(void *u) {
				if (u != nullptr) {
					NkEditeurUnPas(M(u));
				}
			}
			void CmdNouveau(void *u) {
				if (u != nullptr) {
					NkEditeurNouvelleScene(M(u));
				}
			}
			void CmdEnregistrer(void *u) {
				if (u != nullptr) {
					NkEditeurSauver(M(u));
				}
			}
			void CmdOuvrir(void *u) {
				if (u != nullptr) {
					NkEditeurOuvrir(M(u));
				}
			}

			/// Le relais de televersement de NkTextures2D vers le rendu du kit.
			bool TeleverserVersKit(void *rendu, uint32 texId, const uint8 *rgba, int32 w, int32 h) {
				return rendu != nullptr &&
					   static_cast<editorkit::NkIEditorRenderer *>(rendu)->UploadImageRGBA(texId, rgba, w, h);
			}

			void CmdAppareilSuivant(void *u) {
				if (u != nullptr) {
					NkEditeurModele &m = *static_cast<NkEditeurModele *>(u);
					m.profil = (m.profil + 1) % NkNbProfils();
				}
			}

			void CmdQuitter(void *u) {
				if (u != nullptr) {
					static_cast<NkEditorShell *>(u)->RequestClose();
				}
			}

		} // namespace

		// =====================================================================
		NkEditeurApp::NkEditeurApp() noexcept
			: mViseur(mModele), mHierarchie(mModele), mActeurs(mModele), mInspecteur(mModele), mMonde(mModele), mOutils(mModele) {
		}

		// =====================================================================
		NkOptional<int> NkEditeurApp::LireArguments(const NkVector<NkString> &args) {
			for (uint32 i = 0; i < args.Size(); ++i) {
				if (args[i].StartsWith("--profil=")) {
					const int32 n = NkString(args[i].SubStr(9)).ToInt32();
					mModele.profil = (n >= 0 && n < NkNbProfils()) ? n : 0;
					continue;
				}
				if (args[i] == "--paysage") {
					mModele.paysage = true;
					continue;
				}
				if (args[i] == "--simuler") {
					mModele.simuler = true;
					continue;
				}
				if (args[i] == "--selftest") {
					// L'editeur n'a pas de regles a lui : ce qu'il y a a verifier
					// appartient a Unkeny. Jusqu'au 2026-09-29, Unkeny n'avait pas
					// de banc et celui-ci rendait INDETERMINE plutot qu'un faux
					// vert. Le moteur en a un desormais : on le lance.
					// Le moteur d'abord (textures, sauvegarde, son, systemes), puis
					// les ACTIONS de l'editeur : un echec d'Unkeny se lit ainsi a
					// sa source, pas dans ses consequences.
					const int32 moteur = unkeny::NkUnkenyLancerBanc();
					const int32 editeur = NkEditeurLancerBanc();
					return NkOptional<int>((moteur != 0 || editeur != 0) ? 1 : 0);
				}
			}
			return NkOptional<int>();
		}

		// =====================================================================
		bool NkEditeurApp::Init() {
			// ── La scene ─────────────────────────────────────────────────────
			// Les textures des acteurs sont FABRIQUEES maintenant ; elles partent
			// au rendu quand le shell en a un (Brancher, plus bas).
			NkCreerRessourcesSim(mModele.ressources, &mModele.textures, nullptr);
			NkEditeurNouvelleScene(mModele);
			mModele.carte.Creer(40, 24, 1.f);
			mModele.carte.AjouterCouche(0, 1.f);
			mModele.carte.PoserNature(1, NkNatureTuile::NK_SOLIDE);
			if (mModele.simuler) {
				NkEditeurJouer(mModele);
			}
			mViseur.CadrerSurTout();

			// ── La coquille ──────────────────────────────────────────────────
			// Elle porte de gros etats (gestionnaire de docks, contexte NKGui) :
			// sur le TAS, jamais sur la pile.
			mShell = memory::NkMakeUnique<NkEditorShell>();
			if (!mShell) {
				return false;
			}

			// ⚠️ BACKEND DE RENDU INJECTE. Depuis le 2026-09-01, le kit n'en cree
			// plus par defaut : un defaut dans son .cpp etait une dependance de
			// LIEN pour tous ses consommateurs, y compris ceux qui rendent en
			// NKRHI. `static` parce que le shell NE POSSEDE PAS ce pointeur --
			// l'objet doit lui survivre.
			//
			// Unkeny rend en NKCanvas, donc c'est l'implementation canvas.
			static editorkit::NkEditorCanvasRenderer canvasRenderer;

			NkEditorShellConfig scfg;
			scfg.title = "Unkeny — editeur";
			scfg.width = 1280;
			scfg.height = 760;
			scfg.renderer = &canvasRenderer;
			if (!mShell->Init(scfg)) {
				return false;
			}

			// Le rendu existe : les textures en attente y partent.
			mModele.textures.Brancher(&TeleverserVersKit, mShell->Renderer());

			// ── Les panneaux ─────────────────────────────────────────────────
			// Le shell les ancre, les ferme, les rouvre, et sauve la disposition.
			// C'est exactement ce que ma version precedente calculait a la main.
			mShell->AddPanel(&mViseur);
			mShell->AddPanel(&mHierarchie);
			mShell->AddPanel(&mActeurs);
			mShell->AddPanel(&mOutils);
			mShell->AddPanel(&mInspecteur);
			mShell->AddPanel(&mMonde);
			mShell->SetToolbar(&NkBarreOutilsEditeur, &mModele);

			// ── Les commandes ────────────────────────────────────────────────
			// Elles arrivent gratuitement dans la palette (Ctrl+Maj+P) : une
			// action atteignable au clavier ET a la souris, sans avoir a dessiner
			// un bouton pour chacune.
			mShell->RegisterCommand("Vue: Cadrer sur tout", &CmdCadrer, &mViseur, "F");
			mShell->RegisterCommand("Scene: Nouvelle entite", &CmdNouvelleEntite, &mModele, "Ctrl+E");
			mShell->RegisterCommand("Edition: Dupliquer", &CmdDupliquer, &mModele, "Ctrl+D");
			mShell->RegisterCommand("Edition: Supprimer la selection", &CmdSupprimer, &mModele, "Suppr");
			mShell->RegisterCommand("Simulation: Jouer / pause", &CmdJouerPause, &mModele, "Espace");
			mShell->RegisterCommand("Simulation: Arreter", &CmdArreter, &mModele, "Echap");
			mShell->RegisterCommand("Simulation: Un pas", &CmdPas, &mModele);
			mShell->RegisterCommand("Fichier: Nouvelle scene", &CmdNouveau, &mModele, "Ctrl+N");
			mShell->RegisterCommand("Fichier: Ouvrir", &CmdOuvrir, &mModele, "Ctrl+O");
			mShell->RegisterCommand("Fichier: Enregistrer", &CmdEnregistrer, &mModele, "Ctrl+S");
			mShell->RegisterCommand("Appareil: Profil suivant", &CmdAppareilSuivant, &mModele);
			mShell->RegisterCommand("Application: Quitter", &CmdQuitter, mShell.Get(), "Ctrl+Q");

			return true;
		}

		// =====================================================================
		int NkEditeurApp::Run() {
			return mShell ? mShell->Run() : -1;
		}

	} // namespace editeur
} // namespace nkentseu
