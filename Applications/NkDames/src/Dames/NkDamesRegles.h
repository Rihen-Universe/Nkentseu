// =============================================================================
// NkDamesRegles.h — regles du jeu de dames INTERNATIONAL (10x10)
//
// A QUOI SERT CE FICHIER
//   Toute la regle du jeu, et RIEN d'autre : pas de dessin, pas de fenetre,
//   pas d'evenement. Il se compile sans NKCanvas et sans NKGui.
//   C'est ce qui permet au banc `--selftest` de tourner sur une machine sans
//   ecran ni GPU, et c'est la seule facon d'avoir un verdict sur les regles
//   qui ne depende pas de ce qu'on croit voir a l'ecran.
//
// LA VARIANTE — c'est le jeu FRANCAIS / INTERNATIONAL, pas les dames anglaises
//   damier 10x10, 20 pieces par camp, on joue sur les cases SOMBRES ;
//   le pion avance en diagonale d'une case, sans reculer ;
//   le pion PREND en avant ET EN ARRIERE (difference majeure avec l'anglais) ;
//   la dame est VOLANTE : elle glisse de plusieurs cases et prend a distance ;
//   la prise est OBLIGATOIRE, et c'est la RAFLE MAXIMALE qui s'impose : on doit
//     jouer la sequence qui capture le PLUS de pieces, meme si une autre est
//     plus interessante ;
//   une piece prise est retiree A LA FIN de la rafle : on ne peut pas repasser
//     deux fois sur la meme piece, mais elle bloque encore le trajet ;
//   la promotion n'a lieu que si le coup S'ACHEVE sur la derniere rangee — un
//     pion qui traverse la rangee au milieu d'une rafle ne devient PAS dame.
//
//   ⚠️ Ces cinq dernieres regles sont celles que toutes les implementations
//   ratent. Elles ont chacune un cas dedie dans le banc, et le banc a ete
//   ecrit AVANT de savoir si le code les respectait.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - une variante (anglaise, bresilienne) -> un champ de configuration, pas
//     un second fichier de regles : deux jeux de regles divergent au premier
//     correctif applique a un seul.
//   - une intelligence artificielle plus forte -> NkDamesIA, a cote. Les regles
//     n'ont pas a savoir qui joue.
// =============================================================================
#pragma once

#include "NKContainers/Sequential/NkVector.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace jeux {

		enum class NkDamesCamp : uint8 { NK_BLANC = 0, NK_NOIR = 1 };

		enum class NkDamesPiece : uint8 {
			NK_VIDE = 0,
			NK_PION_BLANC,
			NK_PION_NOIR,
			NK_DAME_BLANCHE,
			NK_DAME_NOIRE
		};

		inline bool NkDamesEstBlanc(NkDamesPiece p) noexcept {
			return p == NkDamesPiece::NK_PION_BLANC || p == NkDamesPiece::NK_DAME_BLANCHE;
		}
		inline bool NkDamesEstNoir(NkDamesPiece p) noexcept {
			return p == NkDamesPiece::NK_PION_NOIR || p == NkDamesPiece::NK_DAME_NOIRE;
		}
		inline bool NkDamesEstDame(NkDamesPiece p) noexcept {
			return p == NkDamesPiece::NK_DAME_BLANCHE || p == NkDamesPiece::NK_DAME_NOIRE;
		}
		inline bool NkDamesAppartient(NkDamesPiece p, NkDamesCamp c) noexcept {
			return c == NkDamesCamp::NK_BLANC ? NkDamesEstBlanc(p) : NkDamesEstNoir(p);
		}

		/// Nombre maximal de prises dans une rafle. Le record theorique connu au
		/// jeu international est de 12 ; 20 laisse une marge sans jamais tronquer
		/// une rafle legale — et une troncature silencieuse fausserait le choix
		/// de la rafle maximale, donc la regle elle-meme.
		static const int32 NK_DAMES_MAX_PRISES = 20;

		/// Un coup complet : un deplacement simple, ou une rafle entiere.
		struct NkDamesCoup {
				int8 depR = -1, depC = -1; ///< case de depart
				int8 arrR = -1, arrC = -1; ///< case d'arrivee FINALE

				uint8 nbPrises = 0;
				int8 prisR[NK_DAMES_MAX_PRISES] = {};
				int8 prisC[NK_DAMES_MAX_PRISES] = {};

				/// Les cases d'atterrissage successives, pour l'affichage.
				uint8 nbEtapes = 0;
				int8 etapeR[NK_DAMES_MAX_PRISES] = {};
				int8 etapeC[NK_DAMES_MAX_PRISES] = {};

				bool EstPrise() const noexcept {
					return nbPrises > 0;
				}
		};

		// =====================================================================
		// NkDamesPartie
		// =====================================================================
		class NkDamesPartie {
			public:
				static const int32 NK_TAILLE = 10;

				NkDamesPartie() {
					Initialiser();
				}

				/// Position de depart : 20 pieces par camp sur les quatre premieres
				/// rangees de chaque cote, cases sombres uniquement.
				void Initialiser() noexcept;

				NkDamesPiece Case(int32 r, int32 c) const noexcept {
					return DansDamier(r, c) ? mCases[r][c] : NkDamesPiece::NK_VIDE;
				}
				void PoserCase(int32 r, int32 c, NkDamesPiece p) noexcept {
					if (DansDamier(r, c)) {
						mCases[r][c] = p;
					}
				}

				NkDamesCamp Trait() const noexcept {
					return mTrait;
				}
				void PoserTrait(NkDamesCamp c) noexcept {
					mTrait = c;
				}

				/// Tous les coups legaux du camp au trait, RAFLE MAXIMALE APPLIQUEE.
				/// Si la liste rendue est vide, le camp au trait a perdu.
				void CoupsLegaux(NkVector<NkDamesCoup> &sortie) const;

				/// Joue un coup suppose legal. Rend false si le depart ne porte
				/// pas une piece du camp au trait — un refus se DIT, il ne se
				/// devine pas a l'absence d'effet.
				bool Jouer(const NkDamesCoup &coup) noexcept;

				/// true si la partie est finie ; `gagnant` n'a de sens que dans ce cas.
				bool EstTerminee(NkDamesCamp &gagnant) const;

				int32 CompterPieces(NkDamesCamp camp) const noexcept;

				/// Coups jouables depuis une case donnee — pour l'interface, qui
				/// doit montrer les destinations quand on saisit une piece.
				void CoupsDepuis(int32 r, int32 c, NkVector<NkDamesCoup> &sortie) const;

				static bool DansDamier(int32 r, int32 c) noexcept {
					return r >= 0 && r < NK_TAILLE && c >= 0 && c < NK_TAILLE;
				}
				/// Les cases jouables sont les SOMBRES. Convention : (r+c) impair.
				static bool CaseJouable(int32 r, int32 c) noexcept {
					return ((r + c) & 1) != 0;
				}

			private:
				void GenererPrises(int32 r, int32 c, NkVector<NkDamesCoup> &sortie) const;
				void ExplorerPrises(int32 r, int32 c, NkDamesPiece piece, NkDamesCoup &courant,
									bool prisesFaites[NK_TAILLE][NK_TAILLE], NkVector<NkDamesCoup> &sortie) const;
				void GenererSimples(int32 r, int32 c, NkVector<NkDamesCoup> &sortie) const;

				NkDamesPiece mCases[NK_TAILLE][NK_TAILLE] = {};
				NkDamesCamp mTrait = NkDamesCamp::NK_BLANC;
		};

		/// Choisit un coup pour l'ordinateur. Simple par construction : elle
		/// prefere la rafle la plus grosse, puis la promotion, puis l'avance.
		/// Ce n'est pas un adversaire fort, et le fichier ne pretend pas l'etre.
		const NkDamesCoup *NkDamesChoisirCoup(const NkDamesPartie &partie, const NkVector<NkDamesCoup> &coups,
											  uint32 &graine) noexcept;

	} // namespace jeux
} // namespace nkentseu
