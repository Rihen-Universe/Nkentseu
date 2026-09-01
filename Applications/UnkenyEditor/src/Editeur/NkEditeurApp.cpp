// -----------------------------------------------------------------------------
// FICHIER: Editeur/NkEditeurApp.cpp
// DESCRIPTION: L'assemblage de l'editeur sur NKEditorKit.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Editeur/NkEditeurApp.h"

#include "Editeur/NkEditeurViseur.h"
#include "NKEditorKit/NkEditorCanvasRenderer.h"
#include "NKLogger/NkLog.h"

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

			void CmdSupprimer(void *u) {
				if (u == nullptr) {
					return;
				}
				NkEditeurModele &m = *static_cast<NkEditeurModele *>(u);
				if (!m.aSelection) {
					return;
				}
				m.scene.Detruire(m.selection);
				m.aSelection = false;
				m.deplace = false;
			}

			void CmdBasculerSimulation(void *u) {
				if (u != nullptr) {
					NkEditeurModele &m = *static_cast<NkEditeurModele *>(u);
					m.simuler = !m.simuler;
				}
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
			: mViseur(mModele), mHierarchie(mModele), mInspecteur(mModele), mOutils(mModele) {
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
					// ⚠️ L'editeur n'a pas de regles a lui : ce qu'il y aurait a
					// verifier appartient a Unkeny. On le DIT plutot que de
					// rendre un vert qui ne mesure rien -- un banc vide est pire
					// qu'un banc absent, parce qu'on lui fait confiance.
					logger.Infof("[banc] l'editeur n'a pas de banc propre : ses regles vivent dans Unkeny.\n");
					logger.Infof("[banc] INDETERMINE (aucun cas)\n");
					return NkOptional<int>(0);
				}
			}
			return NkOptional<int>();
		}

		// =====================================================================
		bool NkEditeurApp::Init() {
			// ── La scene ─────────────────────────────────────────────────────
			NkSceneConfig cfg;
			cfg.physique = true; // l'editeur exerce la physique : c'est son role
			cfg.gravite = NkVec2f(0.f, -9.81f);
			if (!mModele.scene.Init(cfg)) {
				logger.Error("[unkeny-editeur] la scene a refuse de s'initialiser");
				return false;
			}
			mModele.carte.Creer(40, 24, 1.f);
			mModele.carte.AjouterCouche(0, 1.f);
			mModele.carte.PoserNature(1, NkNatureTuile::NK_SOLIDE);

			ConstruireSceneExemple();
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

			// ── Les panneaux ─────────────────────────────────────────────────
			// Le shell les ancre, les ferme, les rouvre, et sauve la disposition.
			// C'est exactement ce que ma version precedente calculait a la main.
			mShell->AddPanel(&mViseur);
			mShell->AddPanel(&mHierarchie);
			mShell->AddPanel(&mOutils);
			mShell->AddPanel(&mInspecteur);

			// ── Les commandes ────────────────────────────────────────────────
			// Elles arrivent gratuitement dans la palette (Ctrl+Maj+P) : une
			// action atteignable au clavier ET a la souris, sans avoir a dessiner
			// un bouton pour chacune.
			mShell->RegisterCommand("Vue: Cadrer sur tout", &CmdCadrer, &mViseur, "F");
			mShell->RegisterCommand("Edition: Supprimer la selection", &CmdSupprimer, &mModele, "Suppr");
			mShell->RegisterCommand("Simulation: Demarrer / arreter", &CmdBasculerSimulation, &mModele, "Espace");
			mShell->RegisterCommand("Appareil: Profil suivant", &CmdAppareilSuivant, &mModele);
			mShell->RegisterCommand("Application: Quitter", &CmdQuitter, mShell.Get(), "Ctrl+Q");

			return true;
		}

		// =====================================================================
		int NkEditeurApp::Run() {
			return mShell ? mShell->Run() : -1;
		}

		// =====================================================================
		// Une scene d'exemple. Elle n'est pas decorative : elle EXERCE la scene,
		// les composants, la physique et le rendu des le premier lancement.
		// Un editeur qui s'ouvre sur le vide ne prouve rien du moteur.
		// =====================================================================
		void NkEditeurApp::ConstruireSceneExemple() {
			NkScene &scene = mModele.scene;

			// Le sol : statique, large, sous l'origine.
			{
				const ecs::NkEntityId sol = scene.Creer("Sol", NkVec2f(0.f, -4.f));
				NkSprite2D s;
				s.taille = NkVec2f(20.f, 1.f);
				s.couleur = 0x3E4756FFu;
				s.couche = -10;
				scene.Monde().Add<NkSprite2D>(sol, s);

				NkCollisionneur2D c;
				c.forme = NkForme2D::NK_BOITE;
				c.demiTaille = NkVec2f(10.f, 0.5f);
				scene.Monde().Add<NkCollisionneur2D>(sol, c);

				NkCorps2D b;
				b.type = NkTypeCorps::NK_STATIQUE;
				scene.AjouterCorps(sol, b);
			}

			// Quelques caisses qui tombent : elles rendent la physique VISIBLE
			// des l'ouverture, sans qu'on ait rien a faire.
			for (int32 i = 0; i < 6; ++i) {
				NkString nom = NkString::Format("Caisse %d", i + 1);
				const float32 x = -3.f + static_cast<float32>(i) * 1.2f;
				const float32 y = 1.f + static_cast<float32>(i % 3) * 1.6f;
				const ecs::NkEntityId e = scene.Creer(nom.Data(), NkVec2f(x, y));

				NkSprite2D s;
				s.taille = NkVec2f(0.9f, 0.9f);
				s.couleur = (i % 2 == 0) ? 0xE2B028FFu : 0x3C7ACAFFu;
				scene.Monde().Add<NkSprite2D>(e, s);

				NkCollisionneur2D c;
				c.forme = NkForme2D::NK_BOITE;
				c.demiTaille = NkVec2f(0.45f, 0.45f);
				scene.Monde().Add<NkCollisionneur2D>(e, c);

				NkCorps2D b;
				b.type = NkTypeCorps::NK_DYNAMIQUE;
				scene.AjouterCorps(e, b);
			}
		}

	} // namespace editeur
} // namespace nkentseu
