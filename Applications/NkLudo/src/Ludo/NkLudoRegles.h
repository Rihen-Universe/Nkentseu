// =============================================================================
// NkLudoRegles.h — regles du ludo a quatre joueurs
//
// A QUOI SERT CE FICHIER
//   Toute la regle, sans dessin ni fenetre. Compilable seul, donc testable sans
//   ecran ni GPU.
//
// LES REGLES RETENUES — il existe des dizaines de variantes locales, alors on
// ecrit celle-ci noir sur blanc plutot que de laisser deviner :
//   quatre joueurs, quatre pions chacun, piste commune de 52 cases ;
//   il faut un SIX pour sortir un pion de l'ecurie ;
//   un six REJOUE (au plus trois fois de suite, sinon on rend la main) ;
//   on capture en tombant sur un pion adverse SEUL et hors case sure ;
//   huit CASES SURES : les quatre entrees et les quatre cases a +8 ;
//   apres 51 pas de piste, on entre dans la COLONNE DE MAISON (6 cases) ;
//   il faut le compte EXACT pour rentrer : un de trop et le coup est refuse ;
//   le premier a rentrer ses quatre pions gagne.
//
// ⚠️ CE QUI N'EST PAS RETENU, ET C'EST DIT : le blocage par paire (deux pions
//   du meme camp sur une case interdisant le passage). Cette variante est
//   courante mais pas universelle, et elle change profondement la tactique.
//   L'absence est un CHOIX ecrit ici, pas un oubli.
//
// LA GEOMETRIE EST DANS LES REGLES, ET C'EST VOULU
//   La piste, les entrees et les colonnes de maison sont des donnees du JEU,
//   pas du dessin : c'est la meme table qui decide ou un pion va et ou on le
//   dessine. Deux tables — une pour la regle, une pour l'affichage — finiraient
//   par diverger, et un pion s'afficherait ailleurs qu'il n'est.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - une variante de regle -> un champ de configuration, pas un second fichier
//   - le dessin             -> main.cpp, qui LIT cette geometrie
// =============================================================================
#pragma once

#include "NKContainers/Sequential/NkVector.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace jeux {

		static const int32 NK_LUDO_JOUEURS = 4;
		static const int32 NK_LUDO_PIONS = 4;
		static const int32 NK_LUDO_PISTE = 52;	  ///< cases de la piste commune
		static const int32 NK_LUDO_MAISON = 6;	  ///< cases de la colonne de maison
		static const int32 NK_LUDO_GRILLE = 15;	  ///< le plateau fait 15x15 cases

		/// Avancement d'un pion :
		///   -1        a l'ecurie
		///   0 .. 50   sur la piste commune (compte a partir de SON entree)
		///   51 .. 56  dans la colonne de maison
		///   57        rentre
		static const int32 NK_LUDO_ECURIE = -1;
		static const int32 NK_LUDO_ARRIVEE = 57;

		// ── LES PALIERS D'IA ────────────────────────────────────────────────
		// ⚠️ UN PALIER NE CHANGE QUE LA STRATEGIE. Il ne touche ni au de, ni aux
		// coups legaux, ni a une regle : les trois paliers jouent le MEME jeu
		// avec les MEMES des. C'est ce qui rend la difference MESURABLE -- si le
		// palier changeait aussi le hasard, on ne saurait pas ce qu'on compare.
		//
		// Trois, pas cinq : chaque palier doit se DECRIRE en une phrase, sinon
		// le joueur ne sait pas ce qu'il choisit.
		enum class NkNiveauIA : uint8 {
			NK_FACILE = 0, ///< au hasard parmi les coups legaux
			NK_MOYEN,	   ///< rentrer > capturer > sortir > avancer > case sure
			NK_DIFFICILE   ///< le meme, plus UN COUP D'AVANCE : eviter de s'exposer
		};

		/// Case d'entree de chaque joueur sur la piste commune.
		/// 0, 13, 26, 39 : les quatre quarts, exactement.
		inline int32 NkLudoEntree(int32 joueur) noexcept {
			return joueur * 13;
		}

		/// Position ABSOLUE sur la piste (0..51) d'un pion a l'avancement donne.
		/// Rend -1 quand le pion n'est pas sur la piste commune.
		inline int32 NkLudoCasePiste(int32 joueur, int32 avancement) noexcept {
			if (avancement < 0 || avancement > 50) {
				return -1;
			}
			return (NkLudoEntree(joueur) + avancement) % NK_LUDO_PISTE;
		}

		/// Les huit cases sures : les quatre entrees, et les quatre a +8.
		/// Un pion y est intouchable — c'est ce qui rend le jeu jouable.
		inline bool NkLudoCaseSure(int32 casePiste) noexcept {
			if (casePiste < 0) {
				return false;
			}
			const int32 m = casePiste % 13;
			return m == 0 || m == 8;
		}

		/// Coordonnee (ligne, colonne) d'une case de piste sur la grille 15x15.
		void NkLudoGeometriePiste(int32 casePiste, int32 &ligne, int32 &colonne) noexcept;
		/// Coordonnee d'une case de la colonne de maison (index 0..5).
		void NkLudoGeometrieMaison(int32 joueur, int32 index, int32 &ligne, int32 &colonne) noexcept;
		/// Coordonnee d'un emplacement d'ecurie (index 0..3).
		void NkLudoGeometrieEcurie(int32 joueur, int32 index, int32 &ligne, int32 &colonne) noexcept;

		struct NkLudoCoup {
				int8 pion = -1;			///< index du pion deplace (0..3)
				int8 avancementApres = 0;
				bool sortieEcurie = false;
				bool capture = false;
				int8 pionCaptureJoueur = -1; ///< qui est renvoye, -1 si personne
				int8 pionCaptureIndex = -1;
		};

		// =====================================================================
		class NkLudoPartie {
			public:
				NkLudoPartie() {
					Initialiser();
				}

				void Initialiser() noexcept;

				int32 Avancement(int32 joueur, int32 pion) const noexcept {
					return Valide(joueur, pion) ? mAvancement[joueur][pion] : NK_LUDO_ECURIE;
				}
				void PoserAvancement(int32 joueur, int32 pion, int32 v) noexcept {
					if (Valide(joueur, pion)) {
						mAvancement[joueur][pion] = static_cast<int8>(v);
					}
				}

				int32 Joueur() const noexcept {
					return mJoueur;
				}
				void PoserJoueur(int32 j) noexcept {
					mJoueur = j % NK_LUDO_JOUEURS;
				}

				// ── SIEGES ACTIFS ────────────────────────────────────────────
				// Un siege DESACTIVE n'a pas de pions sur le plateau, ne prend
				// jamais la main, et ne peut pas gagner. Par defaut les quatre
				// sont actifs : une partie qui ne dit rien est une partie a
				// quatre, comme avant.
				//
				// ⚠️ ON NE DESCEND JAMAIS SOUS DEUX SIEGES ACTIFS. Un ludo a un
				// seul joueur ne se termine pas -- il tourne indefiniment, et le
				// symptome serait « le jeu se fige » alors que la regle est
				// simplement absurde. Le refus est ici, pas dans l'ecran :
				// l'ecran peut changer, la regle non.
				bool SiegeActif(int32 j) const noexcept {
					return j >= 0 && j < NK_LUDO_JOUEURS && mActif[j];
				}

				int32 NbSiegesActifs() const noexcept {
					int32 n = 0;
					for (int32 i = 0; i < NK_LUDO_JOUEURS; ++i) {
						if (mActif[i]) {
							++n;
						}
					}
					return n;
				}

				/// Rend `false` -- et ne change RIEN -- si la desactivation
				/// ferait tomber sous deux sieges actifs.
				bool PoserSiegeActif(int32 j, bool actif) noexcept {
					if (j < 0 || j >= NK_LUDO_JOUEURS) {
						return false;
					}
					if (!actif && mActif[j] && NbSiegesActifs() <= 2) {
						return false;
					}
					mActif[j] = actif;
					return true;
				}

				/// Le premier siege actif a partir de `j` inclus. Rend `j` si
				/// aucun ne l'est -- cas impossible tant que la borne de deux
				/// tient, mais on ne rend pas une valeur hors bornes.
				int32 ProchainSiegeActif(int32 j) const noexcept {
					for (int32 k = 0; k < NK_LUDO_JOUEURS; ++k) {
						const int32 c = (j + k) % NK_LUDO_JOUEURS;
						if (mActif[c]) {
							return c;
						}
					}
					return j;
				}

				int32 De() const noexcept {
					return mDe;
				}
				void PoserDe(int32 v) noexcept {
					mDe = v;
				}
				/// Lance le de. Deterministe a graine egale : un banc peut rejouer
				/// une partie entiere et obtenir exactement la meme.
				int32 LancerDe(uint32 &graine) noexcept;

				/// Coups legaux du joueur courant avec la valeur de de courante.
				/// Liste VIDE = le joueur ne peut rien faire et passe son tour.
				void CoupsLegaux(NkVector<NkLudoCoup> &sortie) const;

				bool Jouer(const NkLudoCoup &coup) noexcept;

				/// Fait passer la main. Un SIX rejoue, sauf au troisieme d'affilee.
				void FinDeTour(bool aFaitSix) noexcept;

				int32 PionsRentres(int32 joueur) const noexcept;
				bool EstTerminee(int32 &gagnant) const noexcept;

				static bool Valide(int32 j, int32 p) noexcept {
					return j >= 0 && j < NK_LUDO_JOUEURS && p >= 0 && p < NK_LUDO_PIONS;
				}

			private:
				int8 mAvancement[NK_LUDO_JOUEURS][NK_LUDO_PIONS] = {};

				/// Quels sieges jouent. Tous, par defaut.
				bool mActif[NK_LUDO_JOUEURS] = {true, true, true, true};
				int32 mJoueur = 0;
				int32 mDe = 0;
				int32 mSixDaffilee = 0;
		};

		/// Choix de l'ordinateur : rentrer, capturer, sortir, avancer.
		/// `niveau` par DEFAUT au palier moyen : les appelants existants -- le
		/// banc, une reprise -- gardent le comportement qu'ils mesuraient.
		const NkLudoCoup *NkLudoChoisirCoup(const NkLudoPartie &partie, const NkVector<NkLudoCoup> &coups,
											uint32 &graine,
											NkNiveauIA niveau = NkNiveauIA::NK_MOYEN) noexcept;

	} // namespace jeux
} // namespace nkentseu
