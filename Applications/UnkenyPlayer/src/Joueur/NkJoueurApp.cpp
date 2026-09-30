//
// NkJoueurApp.cpp
// =============================================================================
// Description :
//   Le joueur autonome d'Unkeny (voir l'en-tete).
//
// Caracteristiques :
//   - L'ordre d'une trame est celui du moteur (Unkeny.h) : entrees, puis
//     NkAvancerPartie, puis NkDessinerPartie -- les deux fonctions que
//     « Jouer » appelle dans l'editeur.
//   - La pause de l'OS (perte de focus, arriere-plan) coupe le son et relache
//     les touches ; elle ne fige PAS la simulation sur ordinateur : une
//     fenetre de jeu sans le focus continue de tourner, comme partout.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Joueur/NkJoueurApp.h"

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKFileSystem/NkPath.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkWESystem.h"
#include "Unkeny/Banc/NkUnkenyBancLivraison.h"
#include "Unkeny/Partie/NkUnkenyPartie.h"
#include "Unkeny/Scene/NkUnkenySauvegarde.h"

#include <cstdio>

namespace nkentseu {
	namespace joueur {

		using nkgui::NkColor;
		using nkgui::NkRect;

		namespace {
			/// Le fond du viseur de l'editeur : le jeu construit ne change pas de
			/// ciel en quittant l'editeur.
			const NkColor kFond(20, 23, 31);

			NkString AvecBarre(const NkString &d) {
				if (d.Empty() || d.EndsWith('/') || d.EndsWith('\\')) {
					return d;
				}
				return d + "/";
			}

			void Ecrire(const unkeny::NkJeuCharge &jeu, const NkString &dossier) {
				std::printf("UnkenyPlayer — verification du jeu\n");
				std::printf("  dossier        : %s\n", dossier.CStr());
				std::printf("  jeu            : %s\n", jeu.nom.Empty() ? "(sans nom)" : jeu.nom.CStr());
				std::printf("  scene          : %s\n", jeu.scene.CStr());
				std::printf("  textures lues  : %u\n", static_cast<unsigned>(jeu.textures));
				std::printf("  sons lus       : %u\n", static_cast<unsigned>(jeu.sons));
				std::printf("  empreinte cuite: %016llx\n", static_cast<unsigned long long>(jeu.empreinteAttendue));
				std::printf("  empreinte relue: %016llx  %s\n", static_cast<unsigned long long>(jeu.empreinteLue),
							jeu.EmpreinteIdentique() ? "IDENTIQUE" : "DIFFERENTE");
				if (!jeu.erreur.Empty()) {
					std::printf("  ERREUR         : %s\n", jeu.erreur.CStr());
				}
				for (usize i = 0; i < jeu.manquantes.Size(); ++i) {
					std::printf("  MANQUE         : %s\n", jeu.manquantes[i].CStr());
				}
			}
		} // namespace

		NkString NkDossierDuJeuParDefaut() {
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_HARMONYOS)
			// Rien devant : NkFile ouvre d'abord par fopen, puis dans le paquet
			// (AAssetManager, rawfile) ou Jenga a range le dossier des donnees.
			return NkString();
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
			// Le systeme de fichiers virtuel du Web : le dossier des donnees y est
			// precharge sous le meme nom (voir le .jenga genere).
			return NkString("assets/");
#else
			// A COTE DE L'EXECUTABLE, pas dans le dossier courant : un jeu lance
			// par un raccourci ou depuis une console ailleurs doit trouver ses
			// donnees quand meme.
			return AvecBarre(NkPath::GetExecutableDirectory().ToString()) + "assets/";
#endif
		}

		int32 NkJoueurVerifier(const NkString &dossier) {
			NkPartieJouee *p = memory::NkGetDefaultAllocator().New<NkPartieJouee>();
			const bool jouable = unkeny::NkChargerJeu(dossier.CStr(), p->scene, p->textures, nullptr, p->jeu);
			Ecrire(p->jeu, dossier);
			const bool ok = jouable && p->jeu.manquantes.Empty() && p->jeu.EmpreinteIdentique();
			std::printf("%s\n", ok ? "JEU VERIFIE : jouable, complet, empreinte de l'editeur" : "JEU EN DEFAUT");
			memory::NkGetDefaultAllocator().Delete(p);
			return ok ? 0 : 1;
		}

		// =====================================================================
		NkJoueurApp::NkJoueurApp() : mPartie(memory::NkMakeUnique<NkPartieJouee>()) {
			renderer::NkCanvasAppConfig &cfg = Config();
			cfg.title = "Unkeny";
			cfg.width = 1280;
			cfg.height = 720;
			cfg.clearColor = renderer::NkColor2D{kFond.r, kFond.g, kFond.b, 255};
			mDossier = NkDossierDuJeuParDefaut();
		}

		NkOptional<int> NkJoueurApp::OnCommandLine(const NkVector<NkString> &args) {
			bool verifier = false;
			for (uint32 i = 0; i < args.Size(); ++i) {
				if (args[i].StartsWith("--jeu=")) {
					mDossier = AvecBarre(NkString(args[i].SubStr(6)));
					continue;
				}
				if (args[i] == "--verifier") {
					verifier = true;
					continue;
				}
				if (args[i] == "--selftest") {
					// La livraison, puis les entrees du joueur (Espace fait sauter
					// le heros de Gelee) : les deux bilans s'impriment.
					const int32 livraison = unkeny::NkUnkenyLancerBancLivraison();
					const int32 entrees = NkJoueurLancerBancEntrees();
					return NkOptional<int>(livraison == 0 && entrees == 0 ? 0 : 1);
				}
			}
			if (verifier) {
				return NkOptional<int>(NkJoueurVerifier(mDossier));
			}
			// Le titre AVANT la fenetre : relu apres le chargement, il arrivait une
			// seconde et demie trop tard (le son s'initialise avant ; mesure du
			// 2026-09-30, le titre lu a 3 s etait encore « Unkeny »). Sur Android
			// le paquet n'est pas encore lisible ici : le titre y est sans objet.
			const NkString nom = unkeny::NkNomDuJeu(mDossier.CStr());
			if (!nom.Empty()) {
				Config().title = nom;
			}
			return NkOptional<int>();
		}

		bool NkJoueurApp::OnGuiInit() {
			NkPartieJouee &p = *mPartie;
			p.textures.Brancher(&renderer::NkCanvasGuiApp::RelaisTeleversement, static_cast<renderer::NkCanvasGuiApp *>(this));
			// Sans carte son, le jeu CONTINUE : NkSons2D reste appelable.
			const bool son = p.sons.Demarrer();
			unkeny::NkChargerJeu(mDossier.CStr(), p.scene, p.textures, son ? &p.sons : nullptr, p.jeu);
			if (p.jeu.Jouable()) {
				p.zoomRelu = p.scene.Camera().Zoom();
				// Les entrees cuites avec le jeu (sinon les standard), puis les
				// controleurs de personnage branches sur leurs actions.
				const unkeny::NkRapportLiaisons r = NkJoueurEntreesLire(p.entrees, p.jeu.entrees);
				if (!r.Ok()) {
					p.jeu.manquantes.PushBack(NkString("entrees : ") + r.erreurs[0]);
				}
				NkJoueurEntreesBrancher(p.entrees, p.scene);
			}
			if (!p.jeu.nom.Empty()) {
				Window().SetTitle(p.jeu.nom);
			}
			std::printf("[joueur] %s : %s, %u texture(s), %u manque(s), empreinte %s\n",
						p.jeu.nom.Empty() ? "(sans nom)" : p.jeu.nom.CStr(), p.jeu.Jouable() ? "jouable" : p.jeu.erreur.CStr(),
						static_cast<unsigned>(p.jeu.textures), static_cast<unsigned>(p.jeu.manquantes.Size()),
						p.jeu.EmpreinteIdentique() ? "identique a l'editeur" : "DIFFERENTE");
			std::fflush(stdout);
			return true;
		}

		void NkJoueurApp::OnLayout(const renderer::NkLayoutInfo &info) {
			// ⚠️ La base d'abord : c'est elle qui refait les polices a la bonne
			// densite. Le reste (viseur, zoom) se recalcule a chaque trame, dans
			// OnDraw, sur Layout() : un redimensionnement n'a rien d'autre a faire.
			renderer::NkCanvasGuiApp::OnLayout(info);
		}

		void NkJoueurApp::OnTick(float32 deltaTime) {
			NkPartieJouee &p = *mPartie;
			// Pause est une ACTION du jeu (P, Start) : la manette aussi la bascule.
			if (NkJoueurEntreesTrame(p.entrees, &NkWESystem::Gamepads(), deltaTime) == NkDemandeJoueur::NK_BASCULER_PAUSE) {
				p.pause = !p.pause;
			}
			if (!p.jeu.Jouable() || p.pause) {
				return;
			}
			unkeny::NkAvancerPartie(p.scene, deltaTime);
			p.sons.Avancer(p.scene);
		}

		void NkJoueurApp::OnDraw(nkgui::NkGuiDrawList &dl) {
			NkPartieJouee &p = *mPartie;
			const renderer::NkLayoutInfo &lay = Layout();
			const NkRect ecran{0.f, 0.f, static_cast<float32>(lay.width), static_cast<float32>(lay.height)};
			dl.AddRectFilled(ecran, kFond);
			NkJoueurEntreesSurface(p.entrees, ecran.w, ecran.h);
			if (p.jeu.Jouable()) {
				unkeny::NkVue2D &cam = p.scene.Camera();
				cam.PoserViseur(ecran);
				// LE MEME MORCEAU DE MONDE que le viseur de l'editeur : le zoom relu
				// valait pour ce viseur-la, on le ramene a cette fenetre-ci.
				if (p.jeu.vueLargeur > 1.f && p.jeu.vueHauteur > 1.f && ecran.w > 1.f && ecran.h > 1.f) {
					const float32 kx = ecran.w / p.jeu.vueLargeur;
					const float32 ky = ecran.h / p.jeu.vueHauteur;
					cam.PoserZoom(p.zoomRelu * (kx < ky ? kx : ky));
				}
				dl.PushClipRect(ecran, true);
				unkeny::NkDessinerPartie(dl, p.scene, p.rendu);
				dl.PopClipRect();
			}
			if (p.pause) {
				const float32 haut = lay.safeArea.top + 16.f;
				renderer::NkTexteCentre(dl, FontTitle(), ecran.w * 0.5f, haut, "Pause", NkColor(235, 235, 240));
				renderer::NkTexteCentre(dl, FontSmall(), ecran.w * 0.5f, haut + 40.f, "P ou Start pour reprendre, Echap pour quitter",
										NkColor(180, 184, 196));
			}
			DessinerManques(dl, ecran);
		}

		void NkJoueurApp::DessinerManques(nkgui::NkGuiDrawList &dl, const NkRect &ecran) {
			const NkPartieJouee &p = *mPartie;
			if (p.jeu.Jouable() && p.jeu.manquantes.Empty()) {
				return;
			}
			const renderer::NkLayoutInfo &lay = Layout();
			nkgui::NkGuiFont *corps = FontBody();
			nkgui::NkGuiFont *petite = FontSmall();
			const float32 lh = renderer::NkTexteHauteurLigne(corps, 18.f) + 4.f;
			const usize n = p.jeu.manquantes.Size() + (p.jeu.Jouable() ? 0u : 1u);
			const float32 marge = 16.f;
			const float32 x = lay.safeArea.left + marge;
			const float32 y = lay.safeArea.top + marge;
			const float32 w = ecran.w - lay.safeArea.left - lay.safeArea.right - 2.f * marge;
			const float32 h = lh * static_cast<float32>(n + 2u) + 16.f;
			dl.AddRectFilled(NkRect{x, y, w, h}, NkColor(12, 14, 20, 225), 4.f);
			dl.AddRect(NkRect{x, y, w, h}, NkColor(220, 90, 90, 230), 1.f, 4.f);
			const char *titre = p.jeu.Jouable() ? "Ressources absentes : le jeu tourne sans elles"
												: "Le jeu ne peut pas demarrer";
			float32 ligne = y + 8.f;
			renderer::NkTexte(dl, corps, x + 12.f, ligne, titre, NkColor(240, 120, 120), w - 24.f);
			ligne += lh;
			if (!p.jeu.Jouable()) {
				renderer::NkTexte(dl, corps, x + 12.f, ligne, p.jeu.erreur.CStr(), NkColor(235, 235, 240), w - 24.f);
				ligne += lh;
			}
			for (usize i = 0; i < p.jeu.manquantes.Size(); ++i) {
				renderer::NkTexte(dl, corps, x + 12.f, ligne, p.jeu.manquantes[i].CStr(), NkColor(235, 235, 240), w - 24.f);
				ligne += lh;
			}
			const NkString ou = NkString("dossier des donnees : ") + (mDossier.Empty() ? NkString("(le paquet)") : mDossier);
			renderer::NkTexte(dl, petite, x + 12.f, ligne, ou.CStr(), NkColor(160, 166, 180), w - 24.f);
		}

		bool NkJoueurApp::OnEvent(const NkEvent &event) {
			NkPartieJouee &p = *mPartie;
			if (event.As<NkWindowFocusLostEvent>() != nullptr) {
				NkJoueurEntreesRelacher(p.entrees);
				return false;
			}
			const NkDemandeJoueur d = NkJoueurEntreesEvenement(p.entrees, event);
			if (d == NkDemandeJoueur::NK_QUITTER) {
				Quit();
				return true;
			}
			if (d == NkDemandeJoueur::NK_BASCULER_PAUSE) {
				p.pause = !p.pause;
				return true;
			}
			// false : la coquille fait son travail (fermeture, taille, retour Android).
			return false;
		}

		void NkJoueurApp::OnPause() {
			NkPartieJouee &p = *mPartie;
			p.sons.PoserMuet(true);
			NkJoueurEntreesRelacher(p.entrees);
		}

		void NkJoueurApp::OnResume() {
			NkPartieJouee &p = *mPartie;
			p.sons.PoserMuet(false);
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_HARMONYOS) || defined(NKENTSEU_PLATFORM_IOS)
			// La surface a ete REFAITE (NkCanvasApp, NkWindowShownEvent) et les
			// textures sont parties avec le contexte : on les renvoie, depuis les
			// pixels gardes en memoire. Sur ordinateur, une perte de focus ne
			// perd rien -- et renvoyer toutes les images a chaque Alt+Tab
			// couterait pour rien.
			p.textures.Reteleverser();
#endif
		}

		void NkJoueurApp::OnShutdown() {
			mPartie->sons.Arreter();
		}

	} // namespace joueur
} // namespace nkentseu
