// -----------------------------------------------------------------------------
// FICHIER: Dames/NkDamesRegles.cpp
// DESCRIPTION: Regles du jeu de dames international. Aucun dessin, aucune
//              fenetre : ce fichier doit rester compilable seul.
//
// LE POINT DELICAT, ET IL N'Y EN A QU'UN : la rafle.
//   Une rafle est une SUITE de prises faite dans le meme coup. On l'explore en
//   profondeur, et la piece prise est marquee "deja prise" pendant l'exploration
//   sans etre retiree du damier : elle continue de BLOQUER le passage. C'est la
//   regle du jeu, et c'est aussi ce qui evite de compter deux fois la meme
//   piece. Retirer la piece pendant l'exploration produirait des rafles qui
//   n'existent pas — l'erreur classique, et elle est silencieuse.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Dames/NkDamesRegles.h"

namespace nkentseu {
	namespace jeux {

		namespace {
			// Les quatre diagonales. Ordre fixe : il decide, a rafle egale, quel
			// coup l'ordinateur trouve en premier — donc il doit etre stable.
			const int32 kDirR[4] = {-1, -1, 1, 1};
			const int32 kDirC[4] = {-1, 1, -1, 1};
		} // namespace

		// =====================================================================
		void NkDamesPartie::Initialiser() noexcept {
			for (int32 r = 0; r < NK_TAILLE; ++r) {
				for (int32 c = 0; c < NK_TAILLE; ++c) {
					mCases[r][c] = NkDamesPiece::NK_VIDE;
				}
			}
			// Rangees 0..3 = noirs (en haut), 6..9 = blancs (en bas).
			for (int32 r = 0; r < 4; ++r) {
				for (int32 c = 0; c < NK_TAILLE; ++c) {
					if (CaseJouable(r, c)) {
						mCases[r][c] = NkDamesPiece::NK_PION_NOIR;
					}
				}
			}
			for (int32 r = 6; r < NK_TAILLE; ++r) {
				for (int32 c = 0; c < NK_TAILLE; ++c) {
					if (CaseJouable(r, c)) {
						mCases[r][c] = NkDamesPiece::NK_PION_BLANC;
					}
				}
			}
			mTrait = NkDamesCamp::NK_BLANC;
		}

		// =====================================================================
		int32 NkDamesPartie::CompterPieces(NkDamesCamp camp) const noexcept {
			int32 n = 0;
			for (int32 r = 0; r < NK_TAILLE; ++r) {
				for (int32 c = 0; c < NK_TAILLE; ++c) {
					if (NkDamesAppartient(mCases[r][c], camp)) {
						++n;
					}
				}
			}
			return n;
		}

		// =====================================================================
		// ExplorerPrises — le coeur de la rafle
		//
		// `courant` est la rafle construite jusqu'ici ; `prisesFaites` marque les
		// pieces deja capturees DANS CETTE rafle. On empile toute rafle qui ne
		// peut plus continuer : le filtrage par nombre maximal se fait en sortie,
		// pas ici — filtrer trop tot ferait perdre des branches plus longues.
		// =====================================================================
		void NkDamesPartie::ExplorerPrises(int32 r, int32 c, NkDamesPiece piece, NkDamesCoup &courant,
										   bool prisesFaites[NK_TAILLE][NK_TAILLE],
										   NkVector<NkDamesCoup> &sortie) const {
			const NkDamesCamp camp = NkDamesEstBlanc(piece) ? NkDamesCamp::NK_BLANC : NkDamesCamp::NK_NOIR;
			const NkDamesCamp adverse = (camp == NkDamesCamp::NK_BLANC) ? NkDamesCamp::NK_NOIR : NkDamesCamp::NK_BLANC;
			const bool dame = NkDamesEstDame(piece);
			bool aContinue = false;

			for (int32 d = 0; d < 4; ++d) {
				// --- Trouver la piece a sauter dans cette direction -----------
				int32 vr = r + kDirR[d];
				int32 vc = c + kDirC[d];

				if (dame) {
					// La dame glisse sur les cases vides avant de rencontrer une
					// piece. ⚠️ La case de DEPART de la rafle est vide pour de
					// bon (la piece l'a quittee) : ne pas la traiter comme
					// occupee, sinon une dame ne pourrait jamais se recroiser.
					while (DansDamier(vr, vc) && Case(vr, vc) == NkDamesPiece::NK_VIDE) {
						vr += kDirR[d];
						vc += kDirC[d];
					}
				}
				if (!DansDamier(vr, vc)) {
					continue;
				}
				const NkDamesPiece cible = Case(vr, vc);
				if (!NkDamesAppartient(cible, adverse)) {
					continue; // vide, ou une de nos pieces : pas de prise
				}
				if (prisesFaites[vr][vc]) {
					continue; // deja prise dans cette rafle : elle bloque, on ne la reprend pas
				}

				// --- Cases d'atterrissage derriere la piece prise -------------
				int32 ar = vr + kDirR[d];
				int32 ac = vc + kDirC[d];
				while (DansDamier(ar, ac) && Case(ar, ac) == NkDamesPiece::NK_VIDE) {

					// On descend d'un cran dans la rafle.
					prisesFaites[vr][vc] = true;
					const uint8 nP = courant.nbPrises;
					const uint8 nE = courant.nbEtapes;
					if (nP < NK_DAMES_MAX_PRISES && nE < NK_DAMES_MAX_PRISES) {
						courant.prisR[nP] = static_cast<int8>(vr);
						courant.prisC[nP] = static_cast<int8>(vc);
						courant.nbPrises = static_cast<uint8>(nP + 1);
						courant.etapeR[nE] = static_cast<int8>(ar);
						courant.etapeC[nE] = static_cast<int8>(ac);
						courant.nbEtapes = static_cast<uint8>(nE + 1);
						courant.arrR = static_cast<int8>(ar);
						courant.arrC = static_cast<int8>(ac);

						ExplorerPrises(ar, ac, piece, courant, prisesFaites, sortie);
						aContinue = true;

						// On remonte : tout se remet exactement dans l'etat d'avant.
						courant.nbPrises = nP;
						courant.nbEtapes = nE;
					}
					prisesFaites[vr][vc] = false;

					if (!dame) {
						break; // un pion atterrit juste derriere, jamais plus loin
					}
					ar += kDirR[d];
					ac += kDirC[d];
				}
			}

			// Aucune continuation possible : la rafle s'acheve ici.
			if (!aContinue && courant.nbPrises > 0) {
				NkDamesCoup fini = courant;
				fini.arrR = static_cast<int8>(r);
				fini.arrC = static_cast<int8>(c);
				sortie.PushBack(fini);
			}
		}

		// =====================================================================
		void NkDamesPartie::GenererPrises(int32 r, int32 c, NkVector<NkDamesCoup> &sortie) const {
			const NkDamesPiece piece = Case(r, c);
			if (piece == NkDamesPiece::NK_VIDE) {
				return;
			}
			bool prisesFaites[NK_TAILLE][NK_TAILLE] = {};
			NkDamesCoup courant;
			courant.depR = static_cast<int8>(r);
			courant.depC = static_cast<int8>(c);

			// ⚠️ La case de depart doit etre VIDE pendant l'exploration : la
			// piece est en mouvement. Sans cela, une dame qui repasse par sa
			// case d'origine se croit bloquee par elle-meme — rafle perdue, et
			// aucun symptome visible sauf un coup legal qui manque.
			NkDamesPartie &modifiable = const_cast<NkDamesPartie &>(*this);
			const NkDamesPiece sauve = modifiable.mCases[r][c];
			modifiable.mCases[r][c] = NkDamesPiece::NK_VIDE;

			ExplorerPrises(r, c, piece, courant, prisesFaites, sortie);

			modifiable.mCases[r][c] = sauve;
		}

		// =====================================================================
		void NkDamesPartie::GenererSimples(int32 r, int32 c, NkVector<NkDamesCoup> &sortie) const {
			const NkDamesPiece piece = Case(r, c);
			if (piece == NkDamesPiece::NK_VIDE) {
				return;
			}
			const bool dame = NkDamesEstDame(piece);
			// Le pion blanc monte (r decroit), le pion noir descend.
			const int32 sens = NkDamesEstBlanc(piece) ? -1 : 1;

			for (int32 d = 0; d < 4; ++d) {
				if (!dame && kDirR[d] != sens) {
					continue; // un pion n'avance pas a reculons
				}
				int32 nr = r + kDirR[d];
				int32 nc = c + kDirC[d];
				while (DansDamier(nr, nc) && Case(nr, nc) == NkDamesPiece::NK_VIDE) {
					NkDamesCoup coup;
					coup.depR = static_cast<int8>(r);
					coup.depC = static_cast<int8>(c);
					coup.arrR = static_cast<int8>(nr);
					coup.arrC = static_cast<int8>(nc);
					coup.nbEtapes = 1;
					coup.etapeR[0] = static_cast<int8>(nr);
					coup.etapeC[0] = static_cast<int8>(nc);
					sortie.PushBack(coup);
					if (!dame) {
						break; // le pion avance d'une seule case
					}
					nr += kDirR[d];
					nc += kDirC[d];
				}
			}
		}

		// =====================================================================
		// CoupsLegaux — c'est ICI que la rafle maximale s'impose
		// =====================================================================
		void NkDamesPartie::CoupsLegaux(NkVector<NkDamesCoup> &sortie) const {
			sortie.Clear();

			NkVector<NkDamesCoup> prises;
			for (int32 r = 0; r < NK_TAILLE; ++r) {
				for (int32 c = 0; c < NK_TAILLE; ++c) {
					if (NkDamesAppartient(mCases[r][c], mTrait)) {
						GenererPrises(r, c, prises);
					}
				}
			}

			if (prises.Size() > 0) {
				// PRISE OBLIGATOIRE, et RAFLE MAXIMALE : on ne garde que les
				// sequences qui capturent le plus. Une implementation qui rend
				// toutes les prises "marche" en apparence et laisse jouer des
				// coups illegaux — c'est le defaut le plus courant du jeu.
				uint8 maxi = 0;
				for (uint32 i = 0; i < prises.Size(); ++i) {
					if (prises[i].nbPrises > maxi) {
						maxi = prises[i].nbPrises;
					}
				}
				for (uint32 i = 0; i < prises.Size(); ++i) {
					if (prises[i].nbPrises == maxi) {
						sortie.PushBack(prises[i]);
					}
				}
				return;
			}

			for (int32 r = 0; r < NK_TAILLE; ++r) {
				for (int32 c = 0; c < NK_TAILLE; ++c) {
					if (NkDamesAppartient(mCases[r][c], mTrait)) {
						GenererSimples(r, c, sortie);
					}
				}
			}
		}

		// =====================================================================
		void NkDamesPartie::CoupsDepuis(int32 r, int32 c, NkVector<NkDamesCoup> &sortie) const {
			sortie.Clear();
			NkVector<NkDamesCoup> tous;
			CoupsLegaux(tous);
			// On filtre la liste LEGALE, on ne regenere pas : sinon l'interface
			// proposerait des coups que la rafle maximale interdit, et le refus
			// arriverait au moment du clic, sans explication.
			for (uint32 i = 0; i < tous.Size(); ++i) {
				if (tous[i].depR == static_cast<int8>(r) && tous[i].depC == static_cast<int8>(c)) {
					sortie.PushBack(tous[i]);
				}
			}
		}

		// =====================================================================
		bool NkDamesPartie::Jouer(const NkDamesCoup &coup) noexcept {
			if (!DansDamier(coup.depR, coup.depC) || !DansDamier(coup.arrR, coup.arrC)) {
				return false;
			}
			NkDamesPiece piece = mCases[coup.depR][coup.depC];
			if (!NkDamesAppartient(piece, mTrait)) {
				return false;
			}

			mCases[coup.depR][coup.depC] = NkDamesPiece::NK_VIDE;
			for (uint8 i = 0; i < coup.nbPrises; ++i) {
				mCases[coup.prisR[i]][coup.prisC[i]] = NkDamesPiece::NK_VIDE;
			}

			// PROMOTION — seulement si le coup S'ACHEVE sur la derniere rangee.
			// Un pion qui traverse cette rangee au milieu d'une rafle et en
			// repart reste un pion : c'est la regle, et c'est contre-intuitif.
			if (piece == NkDamesPiece::NK_PION_BLANC && coup.arrR == 0) {
				piece = NkDamesPiece::NK_DAME_BLANCHE;
			} else if (piece == NkDamesPiece::NK_PION_NOIR && coup.arrR == NK_TAILLE - 1) {
				piece = NkDamesPiece::NK_DAME_NOIRE;
			}
			mCases[coup.arrR][coup.arrC] = piece;

			mTrait = (mTrait == NkDamesCamp::NK_BLANC) ? NkDamesCamp::NK_NOIR : NkDamesCamp::NK_BLANC;
			return true;
		}

		// =====================================================================
		bool NkDamesPartie::EstTerminee(NkDamesCamp &gagnant) const {
			// Deux facons de perdre, et il faut les DEUX : plus de pieces, ou
			// plus aucun coup legal (bloque). Ne tester que le nombre de pieces
			// laisserait une partie bloquee tourner indefiniment.
			if (CompterPieces(NkDamesCamp::NK_BLANC) == 0) {
				gagnant = NkDamesCamp::NK_NOIR;
				return true;
			}
			if (CompterPieces(NkDamesCamp::NK_NOIR) == 0) {
				gagnant = NkDamesCamp::NK_BLANC;
				return true;
			}
			NkVector<NkDamesCoup> coups;
			CoupsLegaux(coups);
			if (coups.Size() == 0) {
				gagnant = (mTrait == NkDamesCamp::NK_BLANC) ? NkDamesCamp::NK_NOIR : NkDamesCamp::NK_BLANC;
				return true;
			}
			return false;
		}

		// =====================================================================
		// NkDamesChoisirCoup — adversaire simple, et il ne pretend pas l'etre
		// =====================================================================
		const NkDamesCoup *NkDamesChoisirCoup(const NkDamesPartie &partie, const NkVector<NkDamesCoup> &coups,
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
			int32 meilleurScore = -1000000;

			for (uint32 i = 0; i < coups.Size(); ++i) {
				const NkDamesCoup &c = coups[i];
				int32 score = static_cast<int32>(c.nbPrises) * 100;

				const NkDamesPiece p = partie.Case(c.depR, c.depC);
				// La promotion vaut cher, et elle ne compte que si le coup FINIT
				// sur la derniere rangee — meme regle que dans Jouer().
				if (p == NkDamesPiece::NK_PION_BLANC && c.arrR == 0) {
					score += 60;
				}
				if (p == NkDamesPiece::NK_PION_NOIR && c.arrR == NkDamesPartie::NK_TAILLE - 1) {
					score += 60;
				}
				// Avancer un peu, rester au bord un peu moins : les bords sont
				// sûrs mais passifs.
				if (p == NkDamesPiece::NK_PION_NOIR) {
					score += c.arrR;
				} else if (p == NkDamesPiece::NK_PION_BLANC) {
					score += (NkDamesPartie::NK_TAILLE - 1 - c.arrR);
				}
				if (c.arrC == 0 || c.arrC == NkDamesPartie::NK_TAILLE - 1) {
					score -= 2;
				}

				// ── DIFFICILE : UN COUP D'AVANCE ─────────────────────────────
				// Le palier moyen regarde ce que le coup RAPPORTE. Celui-ci joue
				// le coup sur une COPIE et demande a l'adversaire ce qu'il
				// pourrait prendre. On penalise la plus grosse rafle qu'on lui
				// laisse.
				//
				// ⚠️ On se sert des REGLES pour cela, jamais d'une heuristique
				// « ce coup a l'air dangereux » : une seconde definition du
				// danger divergerait de la premiere.
				if (difficile) {
					NkDamesPartie apres = partie;
					apres.Jouer(c);
					NkVector<NkDamesCoup> replique;
					apres.CoupsLegaux(replique);
					int32 pire = 0;
					for (uint32 k = 0; k < replique.Size(); ++k) {
						if (static_cast<int32>(replique[k].nbPrises) > pire) {
							pire = static_cast<int32>(replique[k].nbPrises);
						}
					}
					// 90 par piece laissee : un peu moins que les 100 d'une piece
					// prise, pour qu'un echange favorable reste jouable.
					score -= pire * 90;
				}

				// Bruit deterministe : sans lui, l'ordinateur rejoue exactement
				// la meme partie a chaque lancement, ce qui la rend ennuyeuse en
				// deux essais. Generateur congruentiel, reproductible a graine
				// egale — donc un banc peut le rejouer.
				graine = graine * 1664525u + 1013904223u;
				score += static_cast<int32>((graine >> 16) & 0x7u);

				if (score > meilleurScore) {
					meilleurScore = score;
					meilleur = static_cast<int32>(i);
				}
			}
			return meilleur >= 0 ? &coups[static_cast<uint32>(meilleur)] : nullptr;
		}

	} // namespace jeux
} // namespace nkentseu
