// -----------------------------------------------------------------------------
// FICHIER: Ludo/NkLudoJeu.cpp
// DESCRIPTION: L'enchainement du jeu. Le SEUL fichier qui modifie l'etat.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Ludo/NkLudoJeu.h"
#include "Ludo/NkLudoBanc.h"

namespace nkentseu {
	namespace jeux {
		namespace ludo {

			NkLudoJeu::NkLudoJeu() {
				Config().title = "NkLudo";
				Config().width = 560;
				Config().height = 800;
				Config().clearColor = renderer::NkColor2D{kFond.r, kFond.g, kFond.b, 255};
			}

			NkOptional<int> NkLudoJeu::OnCommandLine(const NkVector<NkString> &args) {
				for (uint32 i = 0; i < args.Size(); ++i) {
					if (args[i] == "--selftest") {
						return NkOptional<int>(NkLudoLancerBanc());
					}
					if (args[i].StartsWith("--mode=")) {
						AppliquerMode(args[i].SubStr(7));
					}
				}
				return NkOptional<int>();
			}

			/// Le menu et --mode= passent par ICI tous les deux. Le mode n'est pas
			/// stocke : il n'est qu'un raccourci vers une configuration de sieges,
			/// sinon il pourrait contredire les bascules du pied de page.
			void NkLudoJeu::ChoisirMode(NkMode mode) {
				int32 humains = 1;
				switch (mode) {
					case NkMode::NK_DEUX_JOUEURS: humains = 2; break;
					case NkMode::NK_TROIS_JOUEURS: humains = 3; break;
					case NkMode::NK_QUATRE_JOUEURS: humains = 4; break;
					case NkMode::NK_SIMULATION: humains = 0; break;
					default: humains = 1; break;
				}
				for (int32 i = 0; i < NK_LUDO_JOUEURS; ++i) {
					mControleur[i] = (i < humains) ? NkControleur::NK_HUMAIN : NkControleur::NK_IA;
				}
			}

			void NkLudoJeu::AppliquerMode(const NkString &mode) {
				if (mode == "duo") {
					ChoisirMode(NkMode::NK_DEUX_JOUEURS);
				} else if (mode == "trois") {
					ChoisirMode(NkMode::NK_TROIS_JOUEURS);
				} else if (mode == "quatre") {
					ChoisirMode(NkMode::NK_QUATRE_JOUEURS);
				} else if (mode == "ia") {
					ChoisirMode(NkMode::NK_SIMULATION);
				} else {
					ChoisirMode(NkMode::NK_UN_JOUEUR);
				}
				// Un mode demande en ligne de commande SAUTE le menu : sinon une
				// capture automatique s'arreterait sur l'ecran de choix.
				mEcran = NkEcran::NK_PARTIE;
			}

			bool NkLudoJeu::OnGuiInit() {
				mSplash.PoserJeu("Ludo");
				Rejouer();
				return true;
			}

			void NkLudoJeu::OnLayout(const renderer::NkLayoutInfo &info) {
				renderer::NkCanvasGuiApp::OnLayout(info);
				mGeo.Calculer(info);
			}

			NkLudoVue NkLudoJeu::Vue() const {
				NkLudoVue v;
				v.partie = &mPartie;
				v.coups = &mCoups;
				v.controleur = mControleur;
				v.deLance = mDeLance;
				v.dernierDe = mDernierDe;
				v.finie = mFinie;
				v.gagnant = mGagnant;
				v.anim = &mAnim;
				v.deAnim = &mDeAnim;
				return v;
			}

			// =====================================================================
			// ArmerAnimation — le pion suit SON CHEMIN, case par case
			//
			// On reconstruit les cases traversees a partir de l'avancement AVANT
			// et de l'avancement APRES. Une interpolation en droite ferait passer
			// le pion par-dessus les ecuries et le carre central.
			// =====================================================================
			void NkLudoJeu::ArmerAnimation(int32 joueur, const NkLudoCoup &coup, int32 avancementAvant) {
				mAnim.actif = true;
				mAnim.t = 0.f;
				mAnim.joueur = static_cast<int8>(joueur);
				mAnim.pion = coup.pion;
				mAnim.nbCases = 0;

				auto poser = [&](int32 avancement) {
					if (mAnim.nbCases >= NK_LUDO_ANIM_MAX) {
						return;
					}
					int32 l = 0, c = 0;
					NkPositionPion(joueur, coup.pion, avancement, l, c);
					if (l < 0) {
						return;
					}
					mAnim.ligne[mAnim.nbCases] = static_cast<int8>(l);
					mAnim.colonne[mAnim.nbCases] = static_cast<int8>(c);
					++mAnim.nbCases;
				};

				// Depuis l'ecurie, il n'y a pas de chemin : le pion se POSE sur sa
				// case d'entree. Deux cases suffisent, et le saut se lit bien.
				poser(avancementAvant);
				if (!coup.sortieEcurie) {
					for (int32 a = avancementAvant + 1; a < coup.avancementApres; ++a) {
						poser(a);
					}
				}
				poser(coup.avancementApres);

				// La duree suit le nombre de pas : un pas de six doit prendre plus
				// de temps qu'un pas de un, sinon les deux se ressemblent.
				const int32 pas = mAnim.nbCases > 1 ? mAnim.nbCases - 1 : 1;
				mAnim.duree = 0.10f + 0.055f * static_cast<float32>(pas);
			}

			// =====================================================================
			bool NkLudoJeu::OnPointer(const NkPointer &p) {
				// ⚠️ L'OUVERTURE MANGE L'APPUI, ET C'EST VOULU. Sans ce retour,
				// le meme appui sauterait le splash ET activerait le bouton qui
				// se trouve dessous — l'utilisateur lancerait une partie qu'il
				// n'a pas choisie, en croyant seulement passer l'ecran.
				if (!mSplash.Termine()) {
					if (p.phase == NkPointerPhase::NK_POINTER_UP) {
						mSplash.Sauter();
					}
					return true;
				}
				if (p.phase != NkPointerPhase::NK_POINTER_UP) {
					return false;
				}
				const NkVec2f pos(p.x, p.y);

				// --- L'ECRAN DE MENU -------------------------------------------
				if (mEcran == NkEcran::NK_MENU) {
					// ── Un siege : il tourne Humain -> IA -> Desactive ───────
					for (int32 j = 0; j < NK_LUDO_JOUEURS; ++j) {
						if (NkDansRect(mGeo.choix[j], pos)) {
							mControleur[j] = NkControleurSuivant(mControleur[j]);
							// ⚠️ ON NE REFUSE PAS LE TROISIEME ETAT ICI. Si le
							// reglage tombe sous deux sieges utilisables,
							// « Commencer » se grise et DIT pourquoi. Bloquer le
							// clic a la place laisserait un bouton muet, ce qui
							// se lit comme une panne et non comme une regle.
							return true;
						}
					}
					// ── Commencer ────────────────────────────────────────────
					if (NkDansRect(mGeo.choix[NK_LUDO_LIGNE_COMMENCER], pos) && PeutCommencer()) {
						Rejouer();
						mEcran = NkEcran::NK_PARTIE;
						return true;
					}
					return true;
				}

				if (NkDansRect(mGeo.retour, pos)) {
					mEcran = NkEcran::NK_MENU;
					return true;
				}

				// Les bascules de siege restent atteignables meme partie finie et
				// meme pendant qu'une IA joue : sinon on ne peut plus reprendre la
				// main sur une simulation lancee.
				for (int32 i = 0; i < NK_LUDO_JOUEURS; ++i) {
					if (NkDansRect(mGeo.siege[i], pos)) {
						mControleur[i] =
							(mControleur[i] == NkControleur::NK_HUMAIN) ? NkControleur::NK_IA : NkControleur::NK_HUMAIN;
						mAttente = 0.35f;
						return true;
					}
				}

				if (mFinie) {
					Rejouer();
					return true;
				}
				if (!EstHumain(mPartie.Joueur())) {
					return true; // ce n'est pas votre tour
				}
				if (mAnim.actif || mDeAnim.actif) {
					// Pendant une animation, l'etat est deja celui d'APRES :
					// accepter un clic ferait jouer sur ce qu'on ne voit pas.
					return true;
				}

				// Le bouton fait DEUX choses selon le temps du tour : lancer le
				// de, ou passer quand le de ne permet rien.
				if (NkDansRect(mGeo.bouton, pos)) {
					if (!mDeLance) {
						LancerLeDe();
					} else if (mCoups.Size() == 0) {
						PasserLaMain();
					}
					return true;
				}

				// Sinon on cherche un pion jouable sous le doigt.
				if (!mDeLance || mCoups.Size() == 0) {
					return true;
				}
				for (uint32 i = 0; i < mCoups.Size(); ++i) {
					int32 l = 0, c = 0;
					NkPositionPion(mPartie.Joueur(), mCoups[i].pion, mPartie.Avancement(mPartie.Joueur(), mCoups[i].pion),
								   l, c);
					if (l < 0) {
						continue;
					}
					// ⚠️ Cible ELARGIE au doigt : une case de plateau divisee par
					// quinze fait moins que la recommandation tactile de 9 mm. On
					// vise donc plus large qu'on ne dessine — c'est la seule facon
					// de rendre le jeu jouable sur telephone.
					const NkVec2f centre = mGeo.CentreCase(l, c);
					const float32 rayon = mGeo.cellule * (p.fromTouch ? 1.1f : 0.7f);
					const float32 dx = pos.x - centre.x;
					const float32 dy = pos.y - centre.y;
					if (dx * dx + dy * dy <= rayon * rayon) {
						const int32 joueur = mPartie.Joueur();
						const int32 avant = mPartie.Avancement(joueur, mCoups[i].pion);
						const NkLudoCoup copie = mCoups[i];
						mPartie.Jouer(copie);
						ArmerAnimation(joueur, copie, avant);
						TerminerCoup();
						return true;
					}
				}
				return true;
			}

			// =====================================================================
			void NkLudoJeu::OnTick(float32 deltaTime) {
				// ⚠️ L'OUVERTURE AVANCE AVANT TOUT LE RESTE, ET ELLE ARRETE LA
				// TRAME. Sans ce retour, le jeu tournerait DERRIERE l'ecran de
				// marque : une IA jouerait ses premiers coups pendant les quatre
				// secondes d'ouverture, et le joueur trouverait la partie deja
				// entamee en arrivant.
				if (mSplash.Avancer(deltaTime)) {
					return;
				}
				if (mEcran == NkEcran::NK_MENU || mFinie) {
					return;
				}
				// Les animations avancent TOUJOURS, meme partie finie : le dernier
				// coup doit s'achever avant que le panneau de fin le recouvre.
				if (mDeAnim.actif) {
					mDeAnim.restant -= deltaTime;
					// Les faces defilent vite au debut, puis ralentissent : c'est
					// ce qui fait lire un de qui roule plutot qu'un clignotement.
					mDeAnim.faceMontree = 1 + (static_cast<int32>(mDeAnim.restant * 34.f) % 6);
					if (mDeAnim.restant <= 0.f) {
						mDeAnim.actif = false;
					}
					return;
				}
				if (mAnim.actif) {
					mAnim.t += (mAnim.duree > 0.f) ? deltaTime / mAnim.duree : 1.f;
					if (mAnim.t >= 1.f) {
						mAnim.t = 1.f;
						mAnim.actif = false;
					}
					return;
				}

				if (EstHumain(mPartie.Joueur())) {
					return; // au joueur humain de decider
				}
				mAttente -= deltaTime;
				if (mAttente > 0.f) {
					return;
				}

				// Le tour de l'IA suit les MEMES deux temps que celui d'un humain :
				// elle lance, puis elle joue. Lui faire tout faire d'un coup
				// rendrait le de invisible, et la partie incomprehensible a suivre.
				if (!mDeLance) {
					LancerLeDe();
					mAttente = 0.55f;
					return;
				}
				if (mCoups.Size() == 0) {
					PasserLaMain();
					return;
				}
				const NkLudoCoup *choisi = NkLudoChoisirCoup(mPartie, mCoups, mGraineIA);
				if (choisi != nullptr) {
					const int32 joueur = mPartie.Joueur();
					const int32 avant = mPartie.Avancement(joueur, choisi->pion);
					const NkLudoCoup copie = *choisi; // `choisi` pointe dans mCoups
					mPartie.Jouer(copie);
					ArmerAnimation(joueur, copie, avant);
				}
				TerminerCoup();
			}

			// =====================================================================
			void NkLudoJeu::OnDraw(nkgui::NkGuiDrawList &dl) {
				const renderer::NkLayoutInfo &info = Layout();
				NkLudoPolices f;
				f.titre = FontTitle();
				f.corps = FontBody();
				f.petite = FontSmall();

				// ⚠️ L'OUVERTURE SE PEINT SEULE ET COUVRE TOUT. On ne dessine pas
				// le jeu dessous : son fond est opaque, donc ce serait du travail
				// perdu a chaque trame -- et sur un telephone, ce travail se paie
				// en batterie des le premier ecran.
				if (!mSplash.Termine()) {
					mSplash.Dessiner(dl, f.titre, f.corps, f.petite,
									 nkgui::NkRect{0.f, 0.f, static_cast<float32>(info.width),
												   static_cast<float32>(info.height)},
									 LogoRihenTexId());
					return;
				}

				DessinerFond(dl, info);
				if (mEcran == NkEcran::NK_MENU) {
					DessinerMenu(dl, mGeo, f, mControleur, PeutCommencer());
					return;
				}
				const NkLudoVue vue = Vue();
				DessinerBandeau(dl, mGeo, f, vue);
				DessinerPlateau(dl, mGeo);
				DessinerPions(dl, mGeo, vue);
				DessinerPiedDePage(dl, mGeo, f, vue);
				if (mFinie) {
					DessinerFin(dl, info, f, vue);
				}
			}

			// =====================================================================
			void NkLudoJeu::Rejouer() {
				mPartie.Initialiser();

				// ⚠️ LES REGLES DOIVENT SAVOIR, ET APRES `Initialiser()`.
				// L'ecran connait l'etat des sieges ; sans cette recopie, la
				// partie donnerait la main a un siege desactive, attendrait un
				// coup qui ne peut pas venir, et le symptome serait « le jeu se
				// fige » -- a des lieues de sa cause.
				//
				// On ACTIVE d'abord, on DESACTIVE ensuite : dans l'autre sens,
				// la borne des deux sieges refuserait une desactivation legitime
				// parce que les activations n'auraient pas encore eu lieu.
				for (int32 i = 0; i < NK_LUDO_JOUEURS; ++i) {
					if (mControleur[i] != NkControleur::NK_DESACTIVE) {
						mPartie.PoserSiegeActif(i, true);
					}
				}
				for (int32 i = 0; i < NK_LUDO_JOUEURS; ++i) {
					if (mControleur[i] == NkControleur::NK_DESACTIVE) {
						mPartie.PoserSiegeActif(i, false);
					}
				}
				// `Initialiser()` remet le trait au siege 0 : s'il est desactive,
				// la partie demarrerait sur un joueur qui ne joue pas.
				mPartie.PoserJoueur(mPartie.ProchainSiegeActif(0));
				mCoups.Clear();
				mDeLance = false;
				mDernierDe = 0;
				mFinie = false;
				mGagnant = -1;
				mAnim.actif = false;
				mDeAnim.actif = false;
				mAttente = 0.4f;
			}

			void NkLudoJeu::LancerLeDe() {
				// ⚠️ La valeur est tiree ICI, une fois. L'animation qui suit ne
				// fait que retarder son affichage : si elle decidait du resultat,
				// il y aurait DEUX sources de hasard, dont une invisible au banc.
				mDernierDe = mPartie.LancerDe(mGraineDe);
				mPartie.CoupsLegaux(mCoups);
				mDeLance = true;
				mDeAnim.actif = true;
				mDeAnim.restant = 0.45f;
				mDeAnim.faceMontree = 1;
			}

			void NkLudoJeu::PasserLaMain() {
				mPartie.FinDeTour(mDernierDe == 6);
				mDeLance = false;
				mCoups.Clear();
				mAttente = 0.45f;
			}

			void NkLudoJeu::TerminerCoup() {
				int32 gagnant = -1;
				if (mPartie.EstTerminee(gagnant)) {
					mFinie = true;
					mGagnant = gagnant;
					return;
				}
				PasserLaMain();
			}


			// =====================================================================
			// ⚠️ SANS CETTE METHODE, LA TOUCHE RETOUR D'ANDROID NE FAISAIT RIEN.
			//
			// Le systeme la livrait pourtant : `NkAndroidEventSystem` la traduit
			// en NK_ESCAPE et la consomme. Elle arrivait jusqu'a la coquille, qui
			// la proposait a l'application -- laquelle ne l'ecoutait pas. Un
			// evenement livre a personne est indiscernable d'un evenement jamais
			// emis, et l'on cherche le defaut dans la plateforme.
			//
			// LE GESTE : Retour ferme ce qui est ouvert, du plus interieur au plus
			// exterieur. Au menu, on ne consomme PAS -- c'est au systeme de fermer
			// l'application, et c'est ce que l'utilisateur attend.
			bool NkLudoJeu::OnKeyPress(const NkKeyPressEvent &e) {
				if (e.GetKey() != NkKey::NK_ESCAPE) {
					return false;
				}
				if (!mSplash.Termine()) {
					mSplash.Sauter();
					return true;
				}
				if (mEcran == NkEcran::NK_PARTIE) {
					mEcran = NkEcran::NK_MENU;
					return true;
				}
				return false; // au menu : on laisse le systeme fermer
			}
		} // namespace ludo
	} // namespace jeux
} // namespace nkentseu
