// -----------------------------------------------------------------------------
// FICHIER: Echecs/NkEchecsRegles.cpp
// DESCRIPTION: Regles completes du jeu d'echecs. Aucun dessin, aucune fenetre.
//
// LES TROIS ENDROITS OU L'ON SE TROMPE, ET POURQUOI
//   1. LA LEGALITE. Generer les deplacements ne suffit pas : un coup qui laisse
//      son propre roi en echec est ILLEGAL, meme s'il est geometriquement
//      correct. On genere donc des pseudo-legaux, on joue chacun sur une copie,
//      et on garde ceux qui ne laissent pas le roi attaque. C'est couteux, et
//      c'est la seule facon simple d'etre juste.
//   2. LE ROQUE traverse. Il ne suffit pas que le roi n'arrive pas sur une case
//      attaquee : il ne doit pas non plus la TRAVERSER, ni partir d'un echec.
//      Trois cases a verifier, pas une.
//   3. LA PRISE EN PASSANT ne vaut qu'UN coup. La case cible se remet a -1 a
//      chaque coup joue ; l'oublier autorise une prise en passant trois coups
//      plus tard, et rien ne le signale.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Echecs/NkEchecsRegles.h"

namespace nkentseu {
	namespace jeux {

		namespace {
			const int32 kCavR[8] = {-2, -2, -1, -1, 1, 1, 2, 2};
			const int32 kCavC[8] = {-1, 1, -2, 2, -2, 2, -1, 1};
			const int32 kRoiR[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
			const int32 kRoiC[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
			const int32 kFouR[4] = {-1, -1, 1, 1};
			const int32 kFouC[4] = {-1, 1, -1, 1};
			const int32 kTourR[4] = {-1, 1, 0, 0};
			const int32 kTourC[4] = {0, 0, -1, 1};
		} // namespace

		int32 NkEchecsValeur(NkEchecsPiece p) noexcept {
			switch (NkEchecsType(p)) {
				case 1: return 100;
				case 2: return 320;
				case 3: return 330;
				case 4: return 500;
				case 5: return 900;
				case 6: return 20000;
				default: return 0;
			}
		}

		// =====================================================================
		void NkEchecsPartie::Vider() noexcept {
			for (int32 r = 0; r < NK_TAILLE; ++r) {
				for (int32 c = 0; c < NK_TAILLE; ++c) {
					mCases[r][c] = NkEchecsPiece::NK_VIDE;
				}
			}
			// ⚠️ Les droits de roque tombent AUSSI. Une position construite a la
			// main sans roi ni tour aux cases d'origine, mais avec les droits
			// restes a vrai, produirait un roque impossible — et le banc le
			// compterait comme un coup legal.
			mRoqueBlancPetit = mRoqueBlancGrand = false;
			mRoqueNoirPetit = mRoqueNoirGrand = false;
			mEnPassantR = -1;
			mEnPassantC = -1;
			mDemiCoupsSansPriseNiPion = 0;
		}

		void NkEchecsPartie::Initialiser() noexcept {
			Vider();
			const NkEchecsPiece rangN[8] = {NkEchecsPiece::NK_TOUR_N,	 NkEchecsPiece::NK_CAVALIER_N,
											NkEchecsPiece::NK_FOU_N,	 NkEchecsPiece::NK_DAME_N,
											NkEchecsPiece::NK_ROI_N,	 NkEchecsPiece::NK_FOU_N,
											NkEchecsPiece::NK_CAVALIER_N, NkEchecsPiece::NK_TOUR_N};
			const NkEchecsPiece rangB[8] = {NkEchecsPiece::NK_TOUR_B,	 NkEchecsPiece::NK_CAVALIER_B,
											NkEchecsPiece::NK_FOU_B,	 NkEchecsPiece::NK_DAME_B,
											NkEchecsPiece::NK_ROI_B,	 NkEchecsPiece::NK_FOU_B,
											NkEchecsPiece::NK_CAVALIER_B, NkEchecsPiece::NK_TOUR_B};
			for (int32 c = 0; c < NK_TAILLE; ++c) {
				mCases[0][c] = rangN[c];
				mCases[1][c] = NkEchecsPiece::NK_PION_N;
				mCases[6][c] = NkEchecsPiece::NK_PION_B;
				mCases[7][c] = rangB[c];
			}
			mRoqueBlancPetit = mRoqueBlancGrand = true;
			mRoqueNoirPetit = mRoqueNoirGrand = true;
			mTrait = NkEchecsCamp::NK_BLANC;
		}

		// =====================================================================
		// CaseAttaquee — sert a l'echec ET au roque
		//
		// On raisonne A L'ENVERS : depuis la case visee, on regarde si une piece
		// ennemie du bon type se trouve au bout d'une de ses lignes d'attaque.
		// C'est bien moins couteux que de generer tous les coups adverses, et
		// surtout ca ne peut pas boucler (generer les coups adverses demanderait
		// de savoir s'ils laissent LEUR roi en echec, donc de s'appeler soi-meme).
		// =====================================================================
		bool NkEchecsPartie::CaseAttaquee(int32 r, int32 c, NkEchecsCamp parCamp) const {
			const bool parBlanc = (parCamp == NkEchecsCamp::NK_BLANC);

			// --- Pions. Ils prennent en diagonale, dans LEUR sens d'avance.
			// Le blanc monte (r decroit) : il attaque donc depuis r+1.
			const int32 pr = parBlanc ? r + 1 : r - 1;
			const NkEchecsPiece pion = parBlanc ? NkEchecsPiece::NK_PION_B : NkEchecsPiece::NK_PION_N;
			if (DansDamier(pr, c - 1) && mCases[pr][c - 1] == pion) {
				return true;
			}
			if (DansDamier(pr, c + 1) && mCases[pr][c + 1] == pion) {
				return true;
			}

			// --- Cavaliers
			const NkEchecsPiece cav = parBlanc ? NkEchecsPiece::NK_CAVALIER_B : NkEchecsPiece::NK_CAVALIER_N;
			for (int32 i = 0; i < 8; ++i) {
				const int32 nr = r + kCavR[i];
				const int32 nc = c + kCavC[i];
				if (DansDamier(nr, nc) && mCases[nr][nc] == cav) {
					return true;
				}
			}

			// --- Roi adverse (cases adjacentes)
			const NkEchecsPiece roi = parBlanc ? NkEchecsPiece::NK_ROI_B : NkEchecsPiece::NK_ROI_N;
			for (int32 i = 0; i < 8; ++i) {
				const int32 nr = r + kRoiR[i];
				const int32 nc = c + kRoiC[i];
				if (DansDamier(nr, nc) && mCases[nr][nc] == roi) {
					return true;
				}
			}

			// --- Diagonales : fou ou dame
			const NkEchecsPiece fou = parBlanc ? NkEchecsPiece::NK_FOU_B : NkEchecsPiece::NK_FOU_N;
			const NkEchecsPiece dame = parBlanc ? NkEchecsPiece::NK_DAME_B : NkEchecsPiece::NK_DAME_N;
			for (int32 d = 0; d < 4; ++d) {
				int32 nr = r + kFouR[d];
				int32 nc = c + kFouC[d];
				while (DansDamier(nr, nc)) {
					const NkEchecsPiece p = mCases[nr][nc];
					if (p != NkEchecsPiece::NK_VIDE) {
						if (p == fou || p == dame) {
							return true;
						}
						break; // une piece bloque la ligne, quelle qu'elle soit
					}
					nr += kFouR[d];
					nc += kFouC[d];
				}
			}

			// --- Lignes et colonnes : tour ou dame
			const NkEchecsPiece tour = parBlanc ? NkEchecsPiece::NK_TOUR_B : NkEchecsPiece::NK_TOUR_N;
			for (int32 d = 0; d < 4; ++d) {
				int32 nr = r + kTourR[d];
				int32 nc = c + kTourC[d];
				while (DansDamier(nr, nc)) {
					const NkEchecsPiece p = mCases[nr][nc];
					if (p != NkEchecsPiece::NK_VIDE) {
						if (p == tour || p == dame) {
							return true;
						}
						break;
					}
					nr += kTourR[d];
					nc += kTourC[d];
				}
			}
			return false;
		}

		// =====================================================================
		bool NkEchecsPartie::EstEnEchec(NkEchecsCamp camp) const {
			const NkEchecsPiece roi =
				(camp == NkEchecsCamp::NK_BLANC) ? NkEchecsPiece::NK_ROI_B : NkEchecsPiece::NK_ROI_N;
			for (int32 r = 0; r < NK_TAILLE; ++r) {
				for (int32 c = 0; c < NK_TAILLE; ++c) {
					if (mCases[r][c] == roi) {
						return CaseAttaquee(r, c, camp == NkEchecsCamp::NK_BLANC ? NkEchecsCamp::NK_NOIR
																				 : NkEchecsCamp::NK_BLANC);
					}
				}
			}
			// Pas de roi : position de banc, jamais une vraie partie. On ne
			// declare PAS d'echec — sinon toute position de test sans roi
			// deviendrait un mat, et les cas cibles seraient faux.
			return false;
		}

		// =====================================================================
		void NkEchecsPartie::AjouterCoupPion(int32 r, int32 c, int32 nr, int32 nc, bool prise,
											 NkVector<NkEchecsCoup> &sortie) const {
			NkEchecsCoup coup;
			coup.depR = static_cast<int8>(r);
			coup.depC = static_cast<int8>(c);
			coup.arrR = static_cast<int8>(nr);
			coup.arrC = static_cast<int8>(nc);
			(void)prise;

			const bool blanc = NkEchecsEstBlanc(mCases[r][c]);
			const int32 rangPromo = blanc ? 0 : NK_TAILLE - 1;
			if (nr == rangPromo) {
				// QUATRE coups distincts, pas un. La sous-promotion en cavalier
				// est parfois le seul coup qui gagne, et perft la compte.
				const NkEchecsPiece choix[4] = {
					blanc ? NkEchecsPiece::NK_DAME_B : NkEchecsPiece::NK_DAME_N,
					blanc ? NkEchecsPiece::NK_TOUR_B : NkEchecsPiece::NK_TOUR_N,
					blanc ? NkEchecsPiece::NK_FOU_B : NkEchecsPiece::NK_FOU_N,
					blanc ? NkEchecsPiece::NK_CAVALIER_B : NkEchecsPiece::NK_CAVALIER_N};
				for (int32 i = 0; i < 4; ++i) {
					coup.promotion = choix[i];
					sortie.PushBack(coup);
				}
				return;
			}
			sortie.PushBack(coup);
		}

		// =====================================================================
		void NkEchecsPartie::CoupsPseudoLegaux(NkVector<NkEchecsCoup> &sortie) const {
			const bool blanc = (mTrait == NkEchecsCamp::NK_BLANC);
			const NkEchecsCamp adverse = blanc ? NkEchecsCamp::NK_NOIR : NkEchecsCamp::NK_BLANC;

			for (int32 r = 0; r < NK_TAILLE; ++r) {
				for (int32 c = 0; c < NK_TAILLE; ++c) {
					const NkEchecsPiece p = mCases[r][c];
					if (p == NkEchecsPiece::NK_VIDE || !NkEchecsAppartient(p, mTrait)) {
						continue;
					}
					const uint8 type = NkEchecsType(p);

					if (type == 1) { // ---- PION ----------------------------
						const int32 sens = blanc ? -1 : 1;
						const int32 rangDepart = blanc ? 6 : 1;
						const int32 av = r + sens;

						if (DansDamier(av, c) && mCases[av][c] == NkEchecsPiece::NK_VIDE) {
							AjouterCoupPion(r, c, av, c, false, sortie);
							// Double pas : uniquement depuis la rangee de depart,
							// et la case intermediaire doit etre libre aussi.
							const int32 av2 = r + sens * 2;
							if (r == rangDepart && DansDamier(av2, c) && mCases[av2][c] == NkEchecsPiece::NK_VIDE) {
								AjouterCoupPion(r, c, av2, c, false, sortie);
							}
						}
						for (int32 dc = -1; dc <= 1; dc += 2) {
							const int32 nc = c + dc;
							if (!DansDamier(av, nc)) {
								continue;
							}
							if (NkEchecsAppartient(mCases[av][nc], adverse)) {
								AjouterCoupPion(r, c, av, nc, true, sortie);
							} else if (mEnPassantR == static_cast<int8>(av) && mEnPassantC == static_cast<int8>(nc)) {
								NkEchecsCoup coup;
								coup.depR = static_cast<int8>(r);
								coup.depC = static_cast<int8>(c);
								coup.arrR = static_cast<int8>(av);
								coup.arrC = static_cast<int8>(nc);
								coup.enPassant = true;
								sortie.PushBack(coup);
							}
						}

					} else if (type == 2 || type == 6) { // ---- CAVALIER / ROI ----
						const int32 *dr = (type == 2) ? kCavR : kRoiR;
						const int32 *dc = (type == 2) ? kCavC : kRoiC;
						for (int32 i = 0; i < 8; ++i) {
							const int32 nr = r + dr[i];
							const int32 nc = c + dc[i];
							if (!DansDamier(nr, nc) || NkEchecsAppartient(mCases[nr][nc], mTrait)) {
								continue;
							}
							NkEchecsCoup coup;
							coup.depR = static_cast<int8>(r);
							coup.depC = static_cast<int8>(c);
							coup.arrR = static_cast<int8>(nr);
							coup.arrC = static_cast<int8>(nc);
							sortie.PushBack(coup);
						}

					} else { // ---- FOU / TOUR / DAME : pieces glissantes ----
						const int32 *dr = (type == 3) ? kFouR : kTourR;
						const int32 *dc = (type == 3) ? kFouC : kTourC;
						const int32 nbDir = (type == 5) ? 8 : 4;
						for (int32 i = 0; i < nbDir; ++i) {
							// La dame est un fou PLUS une tour : au-dela de 4, on
							// bascule sur l'autre table plutot que d'en ecrire une
							// troisieme qui divergerait au premier correctif.
							const int32 pasR = (type == 5) ? (i < 4 ? kFouR[i] : kTourR[i - 4]) : dr[i];
							const int32 pasC = (type == 5) ? (i < 4 ? kFouC[i] : kTourC[i - 4]) : dc[i];
							int32 nr = r + pasR;
							int32 nc = c + pasC;
							while (DansDamier(nr, nc)) {
								if (NkEchecsAppartient(mCases[nr][nc], mTrait)) {
									break;
								}
								NkEchecsCoup coup;
								coup.depR = static_cast<int8>(r);
								coup.depC = static_cast<int8>(c);
								coup.arrR = static_cast<int8>(nr);
								coup.arrC = static_cast<int8>(nc);
								sortie.PushBack(coup);
								if (mCases[nr][nc] != NkEchecsPiece::NK_VIDE) {
									break; // on prend, puis on s'arrete
								}
								nr += pasR;
								nc += pasC;
							}
						}
					}
				}
			}

			// ---- ROQUE ------------------------------------------------------
			// QUATRE conditions, et la troisieme est celle qu'on oublie : le roi
			// ne doit pas TRAVERSER une case attaquee.
			const int32 rang = blanc ? 7 : 0;
			const bool petit = blanc ? mRoqueBlancPetit : mRoqueNoirPetit;
			const bool grand = blanc ? mRoqueBlancGrand : mRoqueNoirGrand;
			const NkEchecsPiece roi = blanc ? NkEchecsPiece::NK_ROI_B : NkEchecsPiece::NK_ROI_N;
			const NkEchecsPiece tour = blanc ? NkEchecsPiece::NK_TOUR_B : NkEchecsPiece::NK_TOUR_N;

			if ((petit || grand) && mCases[rang][4] == roi && !CaseAttaquee(rang, 4, adverse)) {
				if (petit && mCases[rang][7] == tour && mCases[rang][5] == NkEchecsPiece::NK_VIDE &&
					mCases[rang][6] == NkEchecsPiece::NK_VIDE && !CaseAttaquee(rang, 5, adverse) &&
					!CaseAttaquee(rang, 6, adverse)) {
					NkEchecsCoup coup;
					coup.depR = static_cast<int8>(rang);
					coup.depC = 4;
					coup.arrR = static_cast<int8>(rang);
					coup.arrC = 6;
					coup.roque = true;
					sortie.PushBack(coup);
				}
				if (grand && mCases[rang][0] == tour && mCases[rang][1] == NkEchecsPiece::NK_VIDE &&
					mCases[rang][2] == NkEchecsPiece::NK_VIDE && mCases[rang][3] == NkEchecsPiece::NK_VIDE &&
					!CaseAttaquee(rang, 3, adverse) && !CaseAttaquee(rang, 2, adverse)) {
					// ⚠️ La case b1/b8 (colonne 1) doit etre LIBRE mais peut etre
					// ATTAQUEE : la tour la traverse, pas le roi. Une verification
					// d'attaque sur elle interdirait des grands roques legaux.
					NkEchecsCoup coup;
					coup.depR = static_cast<int8>(rang);
					coup.depC = 4;
					coup.arrR = static_cast<int8>(rang);
					coup.arrC = 2;
					coup.roque = true;
					sortie.PushBack(coup);
				}
			}
		}

		// =====================================================================
		void NkEchecsPartie::CoupsLegaux(NkVector<NkEchecsCoup> &sortie) const {
			sortie.Clear();
			NkVector<NkEchecsCoup> pseudo;
			CoupsPseudoLegaux(pseudo);

			for (uint32 i = 0; i < pseudo.Size(); ++i) {
				NkEchecsPartie essai = *this; // copie : on n'abime jamais l'original
				if (!essai.Jouer(pseudo[i])) {
					continue;
				}
				if (!essai.EstEnEchec(mTrait)) {
					sortie.PushBack(pseudo[i]);
				}
			}
		}

		void NkEchecsPartie::CoupsDepuis(int32 r, int32 c, NkVector<NkEchecsCoup> &sortie) const {
			sortie.Clear();
			NkVector<NkEchecsCoup> tous;
			CoupsLegaux(tous);
			for (uint32 i = 0; i < tous.Size(); ++i) {
				if (tous[i].depR == static_cast<int8>(r) && tous[i].depC == static_cast<int8>(c)) {
					sortie.PushBack(tous[i]);
				}
			}
		}

		// =====================================================================
		bool NkEchecsPartie::Jouer(const NkEchecsCoup &coup) noexcept {
			if (!DansDamier(coup.depR, coup.depC) || !DansDamier(coup.arrR, coup.arrC)) {
				return false;
			}
			NkEchecsPiece piece = mCases[coup.depR][coup.depC];
			if (piece == NkEchecsPiece::NK_VIDE || !NkEchecsAppartient(piece, mTrait)) {
				return false;
			}
			const bool blanc = NkEchecsEstBlanc(piece);
			const bool prise = mCases[coup.arrR][coup.arrC] != NkEchecsPiece::NK_VIDE || coup.enPassant;
			const uint8 type = NkEchecsType(piece);

			mCases[coup.depR][coup.depC] = NkEchecsPiece::NK_VIDE;

			if (coup.enPassant) {
				// Le pion capture n'est PAS sur la case d'arrivee : il est juste
				// a cote. C'est la seule prise du jeu ou la piece retiree n'est
				// pas celle qu'on remplace.
				mCases[coup.depR][coup.arrC] = NkEchecsPiece::NK_VIDE;
			}
			if (coup.roque) {
				// Le roi bouge de deux : la tour saute par-dessus.
				const int32 rang = coup.depR;
				if (coup.arrC == 6) {
					mCases[rang][5] = mCases[rang][7];
					mCases[rang][7] = NkEchecsPiece::NK_VIDE;
				} else {
					mCases[rang][3] = mCases[rang][0];
					mCases[rang][0] = NkEchecsPiece::NK_VIDE;
				}
			}
			if (coup.promotion != NkEchecsPiece::NK_VIDE) {
				piece = coup.promotion;
			}
			mCases[coup.arrR][coup.arrC] = piece;

			// --- Droits de roque : ils se perdent, jamais ils ne reviennent ---
			if (type == 6) {
				if (blanc) {
					mRoqueBlancPetit = mRoqueBlancGrand = false;
				} else {
					mRoqueNoirPetit = mRoqueNoirGrand = false;
				}
			}
			if (type == 4) { // une tour qui bouge perd SON cote
				if (blanc && coup.depR == 7) {
					if (coup.depC == 0) {
						mRoqueBlancGrand = false;
					}
					if (coup.depC == 7) {
						mRoqueBlancPetit = false;
					}
				} else if (!blanc && coup.depR == 0) {
					if (coup.depC == 0) {
						mRoqueNoirGrand = false;
					}
					if (coup.depC == 7) {
						mRoqueNoirPetit = false;
					}
				}
			}
			// ⚠️ Une tour CAPTUREE sur sa case d'origine fait perdre le droit
			// aussi. Ne pas le traiter laisse un roque avec une tour qui n'existe
			// plus — perft le detecte, une partie ordinaire presque jamais.
			if (coup.arrR == 7 && coup.arrC == 0) {
				mRoqueBlancGrand = false;
			}
			if (coup.arrR == 7 && coup.arrC == 7) {
				mRoqueBlancPetit = false;
			}
			if (coup.arrR == 0 && coup.arrC == 0) {
				mRoqueNoirGrand = false;
			}
			if (coup.arrR == 0 && coup.arrC == 7) {
				mRoqueNoirPetit = false;
			}

			// --- Prise en passant : valable UN SEUL coup --------------------
			mEnPassantR = -1;
			mEnPassantC = -1;
			if (type == 1 && (coup.arrR - coup.depR == 2 || coup.depR - coup.arrR == 2)) {
				mEnPassantR = static_cast<int8>((coup.depR + coup.arrR) / 2);
				mEnPassantC = coup.depC;
			}

			mDemiCoupsSansPriseNiPion = (prise || type == 1) ? 0 : mDemiCoupsSansPriseNiPion + 1;
			mTrait = blanc ? NkEchecsCamp::NK_NOIR : NkEchecsCamp::NK_BLANC;
			return true;
		}

		// =====================================================================
		bool NkEchecsPartie::MaterielInsuffisant() const {
			int32 pieces = 0, fousCav = 0;
			for (int32 r = 0; r < NK_TAILLE; ++r) {
				for (int32 c = 0; c < NK_TAILLE; ++c) {
					const uint8 t = NkEchecsType(mCases[r][c]);
					if (t == 0 || t == 6) {
						continue;
					}
					// Un pion, une tour ou une dame suffisent a mater : des
					// qu'il y en a un, la partie n'est pas nulle par materiel.
					if (t == 1 || t == 4 || t == 5) {
						return false;
					}
					++fousCav;
					++pieces;
				}
			}
			(void)pieces;
			return fousCav <= 1; // roi seul, ou roi + un fou/cavalier
		}

		NkEchecsEtat NkEchecsPartie::Etat() const {
			NkVector<NkEchecsCoup> coups;
			CoupsLegaux(coups);
			if (coups.Size() == 0) {
				// AUCUN coup legal. Deux issues TRES differentes selon qu'on est
				// en echec ou non — et les confondre transforme une nulle en
				// defaite.
				return EstEnEchec(mTrait) ? NkEchecsEtat::NK_ECHEC_ET_MAT : NkEchecsEtat::NK_PAT;
			}
			if (MaterielInsuffisant()) {
				return NkEchecsEtat::NK_NULLE_MATERIEL;
			}
			if (mDemiCoupsSansPriseNiPion >= 100) {
				return NkEchecsEtat::NK_NULLE_50_COUPS; // 50 coups = 100 demi-coups
			}
			return EstEnEchec(mTrait) ? NkEchecsEtat::NK_ECHEC : NkEchecsEtat::NK_EN_COURS;
		}

		// =====================================================================
		// Perft — l'oracle exterieur
		// =====================================================================
		uint64 NkEchecsPartie::Perft(int32 profondeur) const {
			if (profondeur <= 0) {
				return 1;
			}
			NkVector<NkEchecsCoup> coups;
			CoupsLegaux(coups);
			if (profondeur == 1) {
				return static_cast<uint64>(coups.Size());
			}
			uint64 total = 0;
			for (uint32 i = 0; i < coups.Size(); ++i) {
				NkEchecsPartie suite = *this;
				if (suite.Jouer(coups[i])) {
					total += suite.Perft(profondeur - 1);
				}
			}
			return total;
		}

		// =====================================================================
		const NkEchecsCoup *NkEchecsChoisirCoup(const NkEchecsPartie &partie, const NkVector<NkEchecsCoup> &coups,
												uint32 &graine, NkNiveauIA niveau) noexcept {
			if (coups.Size() == 0) {
				return nullptr;
			}

			// ── FACILE : au hasard parmi les coups LEGAUX ────────────────────
			// Elle ne triche pas et ne joue jamais un coup impossible ; elle ne
			// choisit simplement pas. Affaiblir une IA en lui interdisant des
			// coups la rendrait exploitable, et le joueur le sentirait.
			if (niveau == NkNiveauIA::NK_FACILE) {
				graine = graine * 1664525u + 1013904223u;
				return &coups[(graine >> 16) % coups.Size()];
			}
			const bool difficile = (niveau == NkNiveauIA::NK_DIFFICILE);

			int32 meilleur = -1;
			int32 meilleurScore = -100000000;

			for (uint32 i = 0; i < coups.Size(); ++i) {
				const NkEchecsCoup &c = coups[i];
				int32 score = 0;

				// Ce qu'on prend.
				score += NkEchecsValeur(partie.Case(c.arrR, c.arrC));
				if (c.promotion != NkEchecsPiece::NK_VIDE) {
					score += NkEchecsValeur(c.promotion) - 100;
				}
				// Le centre vaut mieux que le bord.
				const int32 dc = c.arrC < 4 ? c.arrC : 7 - c.arrC;
				const int32 dr = c.arrR < 4 ? c.arrR : 7 - c.arrR;
				score += (dc + dr) * 4;

				// Un mat termine la partie : on le prend toujours.
				NkEchecsPartie essai = partie;
				if (essai.Jouer(c)) {
					const NkEchecsEtat e = essai.Etat();
					if (e == NkEchecsEtat::NK_ECHEC_ET_MAT) {
						score += 1000000;
					} else if (e == NkEchecsEtat::NK_ECHEC) {
						score += 40;
					} else if (e == NkEchecsEtat::NK_PAT) {
						// ⚠️ Le pat est une NULLE, pas une victoire. Sans cette
						// penalite, l'ordinateur gagnant matériellement fait pat
						// avec entrain et annule une partie gagnee.
						score -= 5000;
					}
				}

				// ── DIFFICILE : UN COUP D'AVANCE ─────────────────────────────
				// `essai` porte deja la position APRES notre coup : on lui
				// demande ce que l'adversaire prendrait en retour, et l'on retire
				// la piece la plus chere qu'on lui laisse.
				//
				// ⚠️ Ce n'est PAS une recherche : un seul niveau, aucune
				// evaluation recursive. On ne pretend pas jouer aux echecs -- on
				// evite seulement de donner une piece a chaque tour, qui est le
				// defaut le plus visible d'une IA d'un demi-coup.
				//
				// ⚠️ Et l'on reutilise `essai` plutot que de rejouer le coup sur
				// une seconde copie : deux copies, c'est deux etats a garder
				// d'accord, et le jour ou l'une des deux oublie un cas -- le
				// roque, la prise en passant -- rien ne le signale.
				if (difficile) {
					NkVector<NkEchecsCoup> replique;
					essai.CoupsLegaux(replique);
					int32 pire = 0;
					for (uint32 k = 0; k < replique.Size(); ++k) {
						const int32 v = NkEchecsValeur(essai.Case(replique[k].arrR, replique[k].arrC));
						if (v > pire) {
							pire = v;
						}
					}
					score -= pire;
				}

				graine = graine * 1664525u + 1013904223u;
				score += static_cast<int32>((graine >> 16) & 0xFu);

				if (score > meilleurScore) {
					meilleurScore = score;
					meilleur = static_cast<int32>(i);
				}
			}
			return meilleur >= 0 ? &coups[static_cast<uint32>(meilleur)] : nullptr;
		}

	} // namespace jeux
} // namespace nkentseu
