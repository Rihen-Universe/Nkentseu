// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// NkGridPath.h — chemin le plus court sur une GRILLE de cases (ZÉRO STL).
//
// Deux étapes, indépendantes l'une de l'autre :
//
// 1. PARCOURS EN LARGEUR (BFS, Moore 1959). On part de la case de départ et on
//    visite les cases par « couches » de distance croissante : d'abord les cases à
//    1 pas, puis à 2 pas, etc., avec une file FIFO. Sur un graphe dont toutes les
//    arêtes ont le même coût (ici : un pas vers l'un des 4 voisins), la première
//    fois qu'on atteint une case, c'est par un plus court chemin : le BFS est donc
//    optimal en nombre de pas, en O(L x H) temps et mémoire, sans tas ni
//    heuristique (A* n'apporterait rien d'utile sur une grille de la taille d'un
//    plateau de jeu). Chaque case mémorise sa case « parent » ; on reconstruit le
//    chemin en remontant du but vers le départ, puis on inverse.
//
// 2. LISSAGE PAR LIGNE DE VUE (« string pulling » simplifié). Le chemin BFS suit
//    les axes : il fait des marches d'escalier là où une droite suffirait. On
//    garde un point d'ancrage ; on avance le long du chemin tant que le segment
//    ancrage -> point reste dégagé (aucune case bloquée traversée), puis on fixe le
//    dernier point visible comme nouvel ancrage. La ligne de vue est testée par
//    l'algorithme de Bresenham (1965) : il énumère, en arithmétique entière, les
//    cases traversées par le segment, avec un terme d'erreur
//    e = dx - dy mis à jour à chaque pas. Un pas en diagonale n'est accepté que si
//    les deux cases voisines de l'angle sont libres : on ne « coupe » pas un coin
//    d'obstacle. Le chemin lissé ne contient que des cases libres, et n'est jamais
//    plus long que le chemin BFS (c'est un sous-ensemble de ses points, dans
//    l'ordre).
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace nav {

		/// Case d'une grille (colonne x, ligne y).
		struct NkGridCell {
				int32 x = 0;
				int32 y = 0;
		};

		class NkGridPath {
			public:
				/// Grille de largeur x hauteur cases, toutes libres.
				void Init(int32 largeur, int32 hauteur);

				int32 Width() const noexcept {
					return mW;
				}
				int32 Height() const noexcept {
					return mH;
				}

				void SetBlocked(int32 x, int32 y, bool bloque);
				/// Vrai si la case est hors de la grille ou bloquée.
				bool IsBlocked(int32 x, int32 y) const noexcept;

				/**
				 * Plus court chemin de (dx, dy) à (bx, by), 4 voisins. `sortie` contient le
				 * départ, les étapes et le but. Faux si le départ ou le but est bloqué ou
				 * hors grille, ou si aucun chemin n'existe (sortie vide).
				 * `lisser` applique le lissage par ligne de vue (ne garde que les
				 * changements de direction utiles).
				 */
				bool FindPath(int32 dx, int32 dy, int32 bx, int32 by, NkVector<NkGridCell> &sortie, bool lisser = true) const;

				/// Ligne de vue : le segment entre les centres des deux cases ne traverse aucune case bloquée.
				bool LineOfSight(int32 ax, int32 ay, int32 bx, int32 by) const noexcept;

			private:
				int32 mW = 0;
				int32 mH = 0;
				NkVector<uint8> mBloque;
		};

	} // namespace nav
} // namespace nkentseu
