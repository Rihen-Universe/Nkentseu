// -----------------------------------------------------------------------------
// FICHIER: Dames/NkDamesJeu.cpp
// DESCRIPTION: L'enchainement du jeu. Le SEUL fichier qui modifie l'etat.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Dames/NkDamesJeu.h"
#include "Dames/NkDamesBanc.h"
#include "Dames/NkDamesTheme.h"

namespace nkentseu {
	namespace jeux {
		namespace dames {

			NkDamesJeu::NkDamesJeu() {
				Config().title = "NkDames";
				Config().width = 560;
				Config().height = 780;
				Config().clearColor = renderer::NkColor2D{kFond.r, kFond.g, kFond.b, 255};
			}

			// =====================================================================
			// AVANT TOUTE FENETRE — le banc repond ici, sans ecran ni GPU.
			// =====================================================================
			NkOptional<int> NkDamesJeu::OnCommandLine(const NkVector<NkString> &args) {
				for (uint32 i = 0; i < args.Size(); ++i) {
					if (args[i] == "--selftest") {
						return NkOptional<int>(NkDamesLancerBanc());
					}
					if (args[i].StartsWith("--mode=")) {
						AppliquerMode(args[i].SubStr(7));
					}
				}
				return NkOptional<int>();
			}

			/// Le menu et --mode= passent par ICI, tous les deux. Le mode n'est
			/// pas stocke : il n'est qu'un raccourci vers une configuration de
			/// sieges. Le garder en plus permettrait qu'il contredise les
			/// bascules du pied de page.
			void NkDamesJeu::ChoisirMode(NkMode mode) {
				switch (mode) {
					case NkMode::NK_A_DEUX:
						mControleur[0] = mControleur[1] = NkControleur::NK_HUMAIN;
						break;
					case NkMode::NK_IA_CONTRE_IA:
						mControleur[0] = mControleur[1] = NkControleur::NK_IA;
						break;
					default:
						mControleur[0] = NkControleur::NK_HUMAIN;
						mControleur[1] = NkControleur::NK_IA;
						break;
				}
			}

			void NkDamesJeu::AppliquerMode(const NkString &mode) {
				if (mode == "duo") {
					ChoisirMode(NkMode::NK_A_DEUX);
				} else if (mode == "ia") {
					ChoisirMode(NkMode::NK_IA_CONTRE_IA);
				} else {
					ChoisirMode(NkMode::NK_CONTRE_ORDI);
				}
				// Un mode demande en ligne de commande SAUTE le menu : sinon une
				// capture automatique s'arreterait sur l'ecran de choix.
				mEcran = NkEcran::NK_PARTIE;
			}

			bool NkDamesJeu::OnGuiInit() {
				mSplash.PoserJeu("Dames");
				Rejouer();
				return true;
			}

			void NkDamesJeu::OnLayout(const renderer::NkLayoutInfo &info) {
				renderer::NkCanvasGuiApp::OnLayout(info); // recharge les polices
				mGeo.Calculer(info);
			}

			// =====================================================================
			NkDamesVue NkDamesJeu::Vue() const {
				NkDamesVue v;
				v.partie = &mPartie;
				v.coupsProposes = &mCoupsProposes;
				v.controleur = mControleur;
				v.anim = &mAnim;
				v.selR = mSelR;
				v.selC = mSelC;
				v.finie = mFinie;
				v.gagnant = mGagnant;
				return v;
			}

			// =====================================================================
			// ArmerAnimation — appelee APRES que le coup a ete joue
			//
			// ⚠️ On lit la piece AVANT de jouer et on la passe ici : apres coup,
			// la case de depart est vide et une promotion a change le type. Lire
			// la piece a l'arrivee montrerait une DAME qui traverse le damier
			// alors que c'est un pion qui vient d'etre promu en arrivant.
			// =====================================================================
			void NkDamesJeu::ArmerAnimation(const NkDamesCoup &coup, NkDamesPiece piece, const NkDamesPiece *prises) {
				mAnim.actif = true;
				mAnim.t = 0.f;
				// Une rafle prend plus de temps qu'un pas, mais pas huit fois
				// plus : la duree croit avec le nombre d'etapes, en s'aplatissant.
				const float32 base = 0.20f;
				mAnim.duree = base + 0.11f * static_cast<float32>(coup.nbEtapes > 0 ? coup.nbEtapes - 1 : 0);
				mAnim.piece = piece;
				mAnim.depR = coup.depR;
				mAnim.depC = coup.depC;
				mAnim.nbEtapes = coup.nbEtapes;
				for (uint8 i = 0; i < coup.nbEtapes && i < NK_DAMES_MAX_PRISES; ++i) {
					mAnim.etapeR[i] = coup.etapeR[i];
					mAnim.etapeC[i] = coup.etapeC[i];
				}
				// Les pieces prises, retenues EN IMAGE : les regles les ont deja
				// retirees, mais l'oeil ne les a pas encore vues tomber.
				mAnim.nbPrises = coup.nbPrises;
				for (uint8 i = 0; i < coup.nbPrises && i < NK_DAMES_MAX_PRISES; ++i) {
					mAnim.prisR[i] = coup.prisR[i];
					mAnim.prisC[i] = coup.prisC[i];
					mAnim.prisPiece[i] = prises[i];
				}
			}

			// =====================================================================
			bool NkDamesJeu::OnPointer(const NkPointer &p) {
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
				// On agit au RELACHE, pas a l'appui : c'est ce qui permet de
				// glisser hors d'un bouton pour annuler, comportement attendu
				// partout et gratuit ici.
				if (p.phase != NkPointerPhase::NK_POINTER_UP) {
					return false;
				}
				const NkVec2f pos(p.x, p.y);

				// --- L'ECRAN DE MENU ---------------------------------------
				if (mEcran == NkEcran::NK_MENU) {
					const NkMode modes[3] = {NkMode::NK_CONTRE_ORDI, NkMode::NK_A_DEUX, NkMode::NK_IA_CONTRE_IA};
					for (int32 i = 0; i < 3; ++i) {
						if (NkDansRect(mGeo.choix[i], pos)) {
							ChoisirMode(modes[i]);
							Rejouer();
							mEcran = NkEcran::NK_PARTIE;
							return true;
						}
					}
					return true;
				}

				// Retour au menu. Il passe avant les bascules : il occupe le
				// bandeau, elles occupent le pied de page.
				if (NkDansRect(mGeo.retour, pos)) {
					mEcran = NkEcran::NK_MENU;
					return true;
				}

				// Les bascules de siege AVANT tout le reste : elles doivent
				// rester atteignables meme partie finie, et meme pendant qu'une
				// IA joue — sinon on ne peut plus reprendre la main sur une
				// simulation lancee.
				for (int32 i = 0; i < 2; ++i) {
					if (NkDansRect(mGeo.siege[i], pos)) {
						mControleur[i] =
							(mControleur[i] == NkControleur::NK_HUMAIN) ? NkControleur::NK_IA : NkControleur::NK_HUMAIN;
						mSelR = -1;
						mSelC = -1;
						mCoupsProposes.Clear();
						mAttenteIA = 0.35f;
						return true;
					}
				}
				if (NkDansRect(mGeo.rejouer, pos)) {
					Rejouer();
					return true;
				}
				if (mFinie) {
					Rejouer(); // n'importe ou : on ne piege pas l'utilisateur
					return true;
				}
				if (!EstHumain(TraitIndex())) {
					return true; // c'est le tour d'une IA
				}
				if (mAnim.actif) {
					// Pendant l'animation, le plateau est deja dans l'etat
					// SUIVANT : accepter un clic ferait jouer sur une position
					// que l'utilisateur ne voit pas encore.
					return true;
				}

				int32 r = 0, c = 0;
				if (!mGeo.CaseSous(pos, r, c)) {
					return false;
				}

				// 1. Une piece est saisie et la case cliquee est une destination
				//    legale : on joue.
				if (mSelR >= 0) {
					for (uint32 i = 0; i < mCoupsProposes.Size(); ++i) {
						const NkDamesCoup &coup = mCoupsProposes[i];
						if (coup.arrR == static_cast<int8>(r) && coup.arrC == static_cast<int8>(c)) {
							const NkDamesPiece piece = mPartie.Case(coup.depR, coup.depC);
							// On releve les pieces prises AVANT de jouer : apres,
							// elles ne sont plus sur le damier.
							NkDamesPiece prises[NK_DAMES_MAX_PRISES] = {};
							for (uint8 k = 0; k < coup.nbPrises && k < NK_DAMES_MAX_PRISES; ++k) {
								prises[k] = mPartie.Case(coup.prisR[k], coup.prisC[k]);
							}
							mPartie.Jouer(coup);
							ArmerAnimation(coup, piece, prises);
							mSelR = -1;
							mSelC = -1;
							mCoupsProposes.Clear();
							mAttenteIA = 0.45f;
							VerifierFin();
							return true;
						}
					}
				}

				// 2. Sinon on tente de saisir la piece cliquee — celle du camp AU
				//    TRAIT, pas "les blancs" : a deux joueurs, les deux saisissent.
				if (NkDamesAppartient(mPartie.Case(r, c), mPartie.Trait())) {
					mPartie.CoupsDepuis(r, c, mCoupsProposes);
					if (mCoupsProposes.Size() > 0) {
						mSelR = r;
						mSelC = c;
					} else {
						// Piece a nous mais sans coup legal (la rafle maximale
						// l'interdit). On DESELECTIONNE plutot que de laisser
						// croire a une saisie muette.
						mSelR = -1;
						mSelC = -1;
					}
					return true;
				}

				mSelR = -1;
				mSelC = -1;
				mCoupsProposes.Clear();
				return true;
			}

			// =====================================================================
			void NkDamesJeu::JouerCoupIA() {
				NkVector<NkDamesCoup> coups;
				mPartie.CoupsLegaux(coups);
				const NkDamesCoup *choisi = NkDamesChoisirCoup(mPartie, coups, mGraine);
				if (choisi != nullptr) {
					const NkDamesPiece piece = mPartie.Case(choisi->depR, choisi->depC);
					const NkDamesCoup copie = *choisi; // `choisi` pointe dans `coups`, local
					NkDamesPiece prises[NK_DAMES_MAX_PRISES] = {};
					for (uint8 k = 0; k < copie.nbPrises && k < NK_DAMES_MAX_PRISES; ++k) {
						prises[k] = mPartie.Case(copie.prisR[k], copie.prisC[k]);
					}
					mPartie.Jouer(copie);
					ArmerAnimation(copie, piece, prises);
				}
				if (choisi == nullptr) {
					// aucun coup : rien a animer
				}
				// La selection humaine ne survit pas au coup de l'IA : sinon les
				// destinations affichees appartiennent a une position revolue.
				mSelR = -1;
				mSelC = -1;
				mCoupsProposes.Clear();
				VerifierFin();
			}

			void NkDamesJeu::OnTick(float32 deltaTime) {
				// ⚠️ L'OUVERTURE AVANCE AVANT TOUT LE RESTE, ET ELLE ARRETE LA
				// TRAME. Sans ce retour, le jeu tournerait DERRIERE l'ecran de
				// marque : une IA jouerait ses premiers coups pendant les quatre
				// secondes d'ouverture, et le joueur trouverait la partie deja
				// entamee en arrivant.
				if (mSplash.Avancer(deltaTime)) {
					return;
				}
				if (mEcran == NkEcran::NK_MENU) {
					return; // rien ne se joue tant qu'on n'a pas choisi
				}

				// L'animation avance TOUJOURS, meme quand la partie est finie :
				// le dernier coup doit s'achever avant le panneau de fin.
				if (mAnim.actif) {
					mAnim.t += (mAnim.duree > 0.f) ? deltaTime / mAnim.duree : 1.f;
					if (mAnim.t >= 1.f) {
						mAnim.t = 1.f;
						mAnim.actif = false;
					}
					return; // rien d'autre pendant qu'une piece se deplace
				}

				if (mFinie || EstHumain(TraitIndex())) {
					return; // au joueur humain de decider
				}
				mAttenteIA -= deltaTime;
				if (mAttenteIA > 0.f) {
					return;
				}
				JouerCoupIA();
				// En simulation IA contre IA on garde un rythme LISIBLE : une
				// partie qui defile trop vite ne se regarde pas, et c'est
				// justement ce qu'on demande a une simulation.
				const bool simulation = !EstHumain(0) && !EstHumain(1);
				mAttenteIA = simulation ? 0.30f : 0.45f;
			}

			// =====================================================================
			void NkDamesJeu::OnDraw(nkgui::NkGuiDrawList &dl) {
				const renderer::NkLayoutInfo &info = Layout();
				NkDamesPolices f;
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
				const NkDamesVue vue = Vue();

				DessinerFond(dl, info);
				if (mEcran == NkEcran::NK_MENU) {
					DessinerMenu(dl, info, mGeo, f);
					return;
				}
				DessinerBandeau(dl, mGeo, f, vue);
				DessinerDamier(dl, mGeo, vue);
				DessinerPieces(dl, mGeo, vue);
				DessinerPiedDePage(dl, mGeo, f, vue);
				if (mFinie) {
					DessinerFin(dl, info, f, vue);
				}
			}

			// =====================================================================
			void NkDamesJeu::Rejouer() {
				mPartie.Initialiser();
				mCoupsProposes.Clear();
				mSelR = -1;
				mSelC = -1;
				mFinie = false;
				mAnim.actif = false;
				mAttenteIA = 0.4f;
			}

			void NkDamesJeu::VerifierFin() {
				NkDamesCamp gagnant = NkDamesCamp::NK_BLANC;
				if (mPartie.EstTerminee(gagnant)) {
					mFinie = true;
					mGagnant = gagnant;
				}
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
			bool NkDamesJeu::OnKeyPress(const NkKeyPressEvent &e) {
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
		} // namespace dames
	} // namespace jeux
} // namespace nkentseu
