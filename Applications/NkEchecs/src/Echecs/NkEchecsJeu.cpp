// -----------------------------------------------------------------------------
// FICHIER: Echecs/NkEchecsJeu.cpp
// DESCRIPTION: L'enchainement du jeu. Le SEUL fichier qui modifie l'etat.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Echecs/NkEchecsJeu.h"
#include "Echecs/NkEchecsBanc.h"

namespace nkentseu {
	namespace jeux {
		namespace echecs {

			NkEchecsJeu::NkEchecsJeu() {
				Config().title = "NkEchecs";
				Config().width = 560;
				Config().height = 780;
				Config().clearColor = renderer::NkColor2D{kFond.r, kFond.g, kFond.b, 255};
			}

			NkOptional<int> NkEchecsJeu::OnCommandLine(const NkVector<NkString> &args) {
				for (uint32 i = 0; i < args.Size(); ++i) {
					if (args[i] == "--selftest") {
						return NkOptional<int>(NkEchecsLancerBanc());
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
			void NkEchecsJeu::ChoisirMode(NkMode mode) {
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

			void NkEchecsJeu::AppliquerMode(const NkString &mode) {
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

			bool NkEchecsJeu::OnGuiInit() {
				mSplash.PoserJeu("Echecs");
				Rejouer();
				return true;
			}

			void NkEchecsJeu::OnLayout(const renderer::NkLayoutInfo &info) {
				renderer::NkCanvasGuiApp::OnLayout(info);
				mGeo.Calculer(info);
			}

			NkEchecsVue NkEchecsJeu::Vue() const {
				NkEchecsVue v;
				v.partie = &mPartie;
				v.coupsProposes = &mCoupsProposes;
				v.controleur = mControleur;
				v.selR = mSelR;
				v.selC = mSelC;
				v.etat = mEtat;
				v.anim = &mAnim;
				v.finie = mFinie;
				return v;
			}

			// =====================================================================
			void NkEchecsJeu::ArmerAnimation(const NkEchecsCoup &coup, NkEchecsPiece piece, NkEchecsPiece prise,
											 int32 priseR, int32 priseC) {
				mAnim.actif = true;
				mAnim.t = 0.f;
				mAnim.duree = 0.22f;
				mAnim.nb = 1;
				mAnim.mvt[0].piece = piece;
				mAnim.mvt[0].depR = coup.depR;
				mAnim.mvt[0].depC = coup.depC;
				mAnim.mvt[0].arrR = coup.arrR;
				mAnim.mvt[0].arrC = coup.arrC;
				mAnim.prise = prise;
				mAnim.priseR = static_cast<int8>(priseR);
				mAnim.priseC = static_cast<int8>(priseC);

				// ROQUE : la tour bouge aussi. Elle est deja a sa case d'arrivee
				// dans les regles ; on rejoue son trajet a l'ecran.
				if (coup.roque) {
					const int8 rang = coup.depR;
					const bool petit = (coup.arrC == 6);
					mAnim.nb = 2;
					mAnim.mvt[1].piece = NkEchecsEstBlanc(piece) ? NkEchecsPiece::NK_TOUR_B : NkEchecsPiece::NK_TOUR_N;
					mAnim.mvt[1].depR = rang;
					mAnim.mvt[1].depC = petit ? 7 : 0;
					mAnim.mvt[1].arrR = rang;
					mAnim.mvt[1].arrC = petit ? 5 : 3;
				}
			}

			// =====================================================================
			bool NkEchecsJeu::OnPointer(const NkPointer &p) {
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

				if (NkDansRect(mGeo.retour, pos)) {
					mEcran = NkEcran::NK_MENU;
					return true;
				}

				// Les bascules de siege AVANT tout : elles doivent rester
				// atteignables meme partie finie et meme pendant qu'une IA joue,
				// sinon on ne peut plus reprendre la main sur une simulation.
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
					Rejouer();
					return true;
				}
				if (!EstHumain(TraitIndex())) {
					return true;
				}
				if (mAnim.actif) {
					// Pendant l'animation, le plateau est deja dans l'etat SUIVANT :
					// accepter un clic ferait jouer sur une position que
					// l'utilisateur ne voit pas encore.
					return true;
				}

				int32 r = 0, c = 0;
				if (!mGeo.CaseSous(pos, r, c)) {
					return false;
				}

				if (mSelR >= 0) {
					for (uint32 i = 0; i < mCoupsProposes.Size(); ++i) {
						const NkEchecsCoup &coup = mCoupsProposes[i];
						if (coup.arrR == static_cast<int8>(r) && coup.arrC == static_cast<int8>(c)) {
							// PROMOTION : on prend la dame. Les trois autres
							// existent dans les regles (perft les compte) ; ce
							// choix-la est une decision d'INTERFACE, et il est dit
							// plutot que tu. Une boite de choix viendra ici.
							const NkEchecsPiece piece = mPartie.Case(coup.depR, coup.depC);
							const NkEchecsCoup copie = coup;
							// ⚠️ La PRISE EN PASSANT ne retire pas la piece de la
							// case d'arrivee mais de celle d'a cote : c'est la
							// seule prise du jeu ou les deux different.
							const int32 pR = copie.enPassant ? copie.depR : copie.arrR;
							const int32 pC = copie.arrC;
							const NkEchecsPiece prise = mPartie.Case(pR, pC);
							mPartie.Jouer(copie);
							ArmerAnimation(copie, piece, prise, pR, pC);
							mSelR = -1;
							mSelC = -1;
							mCoupsProposes.Clear();
							mAttenteIA = 0.5f;
							VerifierFin();
							return true;
						}
					}
				}

				// La piece du camp AU TRAIT, pas "les blancs" : a deux joueurs,
				// les deux saisissent.
				if (NkEchecsAppartient(mPartie.Case(r, c), mPartie.Trait())) {
					mPartie.CoupsDepuis(r, c, mCoupsProposes);
					mSelR = mCoupsProposes.Size() > 0 ? r : -1;
					mSelC = mCoupsProposes.Size() > 0 ? c : -1;
					return true;
				}
				mSelR = -1;
				mSelC = -1;
				mCoupsProposes.Clear();
				return true;
			}

			// =====================================================================
			void NkEchecsJeu::OnTick(float32 deltaTime) {
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

				// L'animation avance TOUJOURS, meme partie finie : le dernier coup
				// doit s'achever avant que le panneau de fin le recouvre.
				if (mAnim.actif) {
					mAnim.t += (mAnim.duree > 0.f) ? deltaTime / mAnim.duree : 1.f;
					if (mAnim.t >= 1.f) {
						mAnim.t = 1.f;
						mAnim.actif = false;
					}
					return;
				}

				if (mFinie || EstHumain(TraitIndex())) {
					return;
				}
				mAttenteIA -= deltaTime;
				if (mAttenteIA > 0.f) {
					return;
				}
				NkVector<NkEchecsCoup> coups;
				mPartie.CoupsLegaux(coups);
				const NkEchecsCoup *choisi = NkEchecsChoisirCoup(mPartie, coups, mGraine);
				if (choisi != nullptr) {
					const NkEchecsPiece piece = mPartie.Case(choisi->depR, choisi->depC);
					const NkEchecsCoup copie = *choisi; // `choisi` pointe dans `coups`, local
					const int32 pR = copie.enPassant ? copie.depR : copie.arrR;
					const int32 pC = copie.arrC;
					const NkEchecsPiece prise = mPartie.Case(pR, pC);
					mPartie.Jouer(copie);
					ArmerAnimation(copie, piece, prise, pR, pC);
				}
				// La selection humaine ne survit pas au coup de l'IA : sinon les
				// destinations affichees appartiennent a une position revolue.
				mSelR = -1;
				mSelC = -1;
				mCoupsProposes.Clear();
				VerifierFin();

				// En simulation on garde un rythme LISIBLE : une partie qui
				// defile trop vite ne se regarde pas.
				const bool simulation = !EstHumain(0) && !EstHumain(1);
				mAttenteIA = simulation ? 0.35f : 0.5f;
			}

			// =====================================================================
			void NkEchecsJeu::OnDraw(nkgui::NkGuiDrawList &dl) {
				const renderer::NkLayoutInfo &info = Layout();
				NkEchecsPolices f;
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
				const NkEchecsVue vue = Vue();

				DessinerFond(dl, info);
				if (mEcran == NkEcran::NK_MENU) {
					DessinerMenu(dl, mGeo, f);
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
			void NkEchecsJeu::Rejouer() {
				mPartie.Initialiser();
				mCoupsProposes.Clear();
				mSelR = -1;
				mSelC = -1;
				mFinie = false;
				mEtat = NkEchecsEtat::NK_EN_COURS;
				mAnim.actif = false;
				mAttenteIA = 0.4f;
			}

			void NkEchecsJeu::VerifierFin() {
				mEtat = mPartie.Etat();
				mFinie = (mEtat == NkEchecsEtat::NK_ECHEC_ET_MAT || mEtat == NkEchecsEtat::NK_PAT ||
						  mEtat == NkEchecsEtat::NK_NULLE_MATERIEL || mEtat == NkEchecsEtat::NK_NULLE_50_COUPS);
			}

		} // namespace echecs
	} // namespace jeux
} // namespace nkentseu
