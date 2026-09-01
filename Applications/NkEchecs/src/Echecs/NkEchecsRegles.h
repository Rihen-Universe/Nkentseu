// =============================================================================
// NkEchecsRegles.h — regles completes du jeu d'echecs
//
// A QUOI SERT CE FICHIER
//   Toute la regle, et RIEN d'autre : ni dessin, ni fenetre, ni evenement. Il
//   se compile sans NKCanvas et sans NKGui, ce qui rend le banc lancable sur
//   une machine sans ecran ni GPU.
//
// CE QUI EST COUVERT — la liste compte, parce que les echecs sont le jeu ou
// l'on croit toujours avoir fini alors qu'il reste trois regles :
//   deplacements des six pieces ;
//   ROQUE, petit et grand, avec ses quatre conditions (droits intacts, cases
//     libres, roi pas en echec, et le roi ne TRAVERSE pas une case attaquee) ;
//   PRISE EN PASSANT, valable un seul coup ;
//   PROMOTION en dame, tour, fou ou cavalier ;
//   ECHEC, ECHEC ET MAT, PAT ;
//   nulle par materiel insuffisant et par la regle des 50 coups.
//
// ⚠️ CE QUI N'EST PAS COUVERT, ET QUI EST DIT PLUTOT QUE TU : la nulle par
//   REPETITION DE POSITION. Elle demande de retenir toutes les positions de la
//   partie ; ce n'est pas fait. Une partie peut donc tourner en rond sans etre
//   declaree nulle. C'est une absence connue, pas un oubli — la declarer ici
//   coute une ligne, la decouvrir en jouant coute une partie.
//
// COMMENT ON SAIT QUE C'EST JUSTE — et c'est le point important
//   Le banc ne se compare pas a MES attentes : il se compare aux valeurs
//   PERFT publiees, qui comptent les positions atteignables a N coups depuis
//   la position de depart (20, 400, 8902, 197281). Ce sont des nombres
//   verifies par la communaute des echecs depuis des decennies, sur des
//   moteurs qui n'ont rien a voir avec celui-ci.
//   ⚠️ Un banc qui compare un code a l'attente de son auteur valide surtout son
//   auteur. Perft est un oracle EXTERIEUR : il ne peut pas etre d'accord avec
//   une erreur que j'aurais commise en le concevant.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - une regle              -> ici, avec son cas de banc ecrit AVANT
//   - une intelligence forte -> NkEchecsIA, a cote ; les regles ignorent qui joue
// =============================================================================
#pragma once

#include "NKContainers/Sequential/NkVector.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace jeux {

		enum class NkEchecsCamp : uint8 { NK_BLANC = 0, NK_NOIR = 1 };

		enum class NkEchecsPiece : uint8 {
			NK_VIDE = 0,
			NK_PION_B, NK_CAVALIER_B, NK_FOU_B, NK_TOUR_B, NK_DAME_B, NK_ROI_B,
			NK_PION_N, NK_CAVALIER_N, NK_FOU_N, NK_TOUR_N, NK_DAME_N, NK_ROI_N
		};

		inline bool NkEchecsEstBlanc(NkEchecsPiece p) noexcept {
			return p >= NkEchecsPiece::NK_PION_B && p <= NkEchecsPiece::NK_ROI_B;
		}
		inline bool NkEchecsEstNoir(NkEchecsPiece p) noexcept {
			return p >= NkEchecsPiece::NK_PION_N;
		}
		inline bool NkEchecsAppartient(NkEchecsPiece p, NkEchecsCamp c) noexcept {
			return c == NkEchecsCamp::NK_BLANC ? NkEchecsEstBlanc(p) : NkEchecsEstNoir(p);
		}
		/// Le type sans la couleur : 1=pion, 2=cavalier, 3=fou, 4=tour, 5=dame, 6=roi.
		inline uint8 NkEchecsType(NkEchecsPiece p) noexcept {
			if (p == NkEchecsPiece::NK_VIDE) {
				return 0;
			}
			const uint8 v = static_cast<uint8>(p);
			return NkEchecsEstBlanc(p) ? v : static_cast<uint8>(v - 6);
		}

		struct NkEchecsCoup {
				int8 depR = -1, depC = -1;
				int8 arrR = -1, arrC = -1;
				NkEchecsPiece promotion = NkEchecsPiece::NK_VIDE; ///< NK_VIDE = pas de promotion
				bool enPassant = false;
				bool roque = false;

				bool operator==(const NkEchecsCoup &o) const noexcept {
					return depR == o.depR && depC == o.depC && arrR == o.arrR && arrC == o.arrC &&
						   promotion == o.promotion;
				}
		};

		enum class NkEchecsEtat : uint8 {
			NK_EN_COURS = 0,
			NK_ECHEC,
			NK_ECHEC_ET_MAT,
			NK_PAT,
			NK_NULLE_MATERIEL,
			NK_NULLE_50_COUPS
		};

		// =====================================================================
		// NkEchecsPartie
		// =====================================================================
		class NkEchecsPartie {
			public:
				static const int32 NK_TAILLE = 8;

				NkEchecsPartie() {
					Initialiser();
				}

				void Initialiser() noexcept;
				/// Vide le damier ET remet les droits de roque a faux. Utile aux
				/// positions de banc — sans la remise a zero des droits, une
				/// position construite a la main autoriserait un roque absurde.
				void Vider() noexcept;

				NkEchecsPiece Case(int32 r, int32 c) const noexcept {
					return DansDamier(r, c) ? mCases[r][c] : NkEchecsPiece::NK_VIDE;
				}
				void PoserCase(int32 r, int32 c, NkEchecsPiece p) noexcept {
					if (DansDamier(r, c)) {
						mCases[r][c] = p;
					}
				}

				NkEchecsCamp Trait() const noexcept {
					return mTrait;
				}
				void PoserTrait(NkEchecsCamp c) noexcept {
					mTrait = c;
				}
				void PoserDroitsRoque(bool bPetit, bool bGrand, bool nPetit, bool nGrand) noexcept {
					mRoqueBlancPetit = bPetit;
					mRoqueBlancGrand = bGrand;
					mRoqueNoirPetit = nPetit;
					mRoqueNoirGrand = nGrand;
				}

				/// Coups LEGAUX : les pseudo-legaux dont on a retire ceux qui
				/// laissent son propre roi en echec. C'est cette derniere etape
				/// que l'on saute quand on croit avoir fini.
				void CoupsLegaux(NkVector<NkEchecsCoup> &sortie) const;
				void CoupsDepuis(int32 r, int32 c, NkVector<NkEchecsCoup> &sortie) const;

				bool Jouer(const NkEchecsCoup &coup) noexcept;

				bool EstEnEchec(NkEchecsCamp camp) const;
				/// true si `camp` attaque la case (r,c). Sert au roque et a l'echec.
				bool CaseAttaquee(int32 r, int32 c, NkEchecsCamp parCamp) const;

				NkEchecsEtat Etat() const;

				/// Compte les positions atteignables en `profondeur` demi-coups.
				/// C'est la mesure de reference du monde des echecs.
				uint64 Perft(int32 profondeur) const;

				static bool DansDamier(int32 r, int32 c) noexcept {
					return r >= 0 && r < NK_TAILLE && c >= 0 && c < NK_TAILLE;
				}

			private:
				void CoupsPseudoLegaux(NkVector<NkEchecsCoup> &sortie) const;
				void AjouterCoupPion(int32 r, int32 c, int32 nr, int32 nc, bool prise,
									 NkVector<NkEchecsCoup> &sortie) const;
				bool MaterielInsuffisant() const;

				NkEchecsPiece mCases[NK_TAILLE][NK_TAILLE] = {};
				NkEchecsCamp mTrait = NkEchecsCamp::NK_BLANC;

				bool mRoqueBlancPetit = true, mRoqueBlancGrand = true;
				bool mRoqueNoirPetit = true, mRoqueNoirGrand = true;

				/// Case traversee par un pion qui vient d'avancer de deux. -1 = aucune.
				/// ⚠️ Elle n'est valable QUE pour le coup suivant : la remettre a -1
				/// a chaque coup joue fait partie de la regle, pas du menage.
				int8 mEnPassantR = -1, mEnPassantC = -1;

				int32 mDemiCoupsSansPriseNiPion = 0;
		};

		/// Adversaire simple : materiel d'abord, puis le centre, puis un bruit
		/// deterministe. Ce n'est pas un moteur d'echecs et il ne pretend pas
		/// l'etre — il joue des coups legaux et prend ce qui est en prise.
		const NkEchecsCoup *NkEchecsChoisirCoup(const NkEchecsPartie &partie, const NkVector<NkEchecsCoup> &coups,
												uint32 &graine) noexcept;

		/// Valeur usuelle d'une piece, en centiemes de pion.
		int32 NkEchecsValeur(NkEchecsPiece p) noexcept;

	} // namespace jeux
} // namespace nkentseu
