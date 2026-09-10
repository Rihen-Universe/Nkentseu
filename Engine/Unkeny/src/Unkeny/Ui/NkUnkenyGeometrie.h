// =============================================================================
// NkUnkenyGeometrie.h — la mise en page d'un ecran de jeu 2D
//
// A QUOI SERT CE FICHIER
//   Il calcule OU vont les choses, a partir de la taille de l'ecran et de la
//   zone sure. Il ne dessine rien et ne decide rien.
//
// POURQUOI IL EXISTE — mesure du 2026-09-01
//   Les trois jeux de plateau ecrits avant Unkeny calculent TOUS la meme mise
//   en page : un bandeau en haut, un carre au milieu, une ou deux bandes de
//   boutons en bas, le tout ancre sur la zone sure et valable en portrait comme
//   en paysage. Trois fois le meme calcul, aux memes constantes pres.
//
// ⚠️ LA ZONE SURE N'EST PAS UNE MARGE DECORATIVE
//   Sous l'encoche, la barre de statut ou l'indicateur de geste, un bouton
//   devient INATTEIGNABLE — pas moche : inatteignable. Et elle CHANGE a la
//   rotation, a l'ouverture du clavier, d'un appareil a l'autre. Elle se
//   demande a la plateforme, jamais codee en dur.
//
//   Regle qui en decoule, et qui fait la difference entre une interface mobile
//   correcte et une qui a l'air bricolee :
//     ce qui va JUSQU'AU BORD    fonds, degrades, listes qui defilent
//     ce qui reste DANS LA ZONE  texte, boutons, champs — tout ce qui se
//                                lit ou se touche
//   C'est un choix PAR ELEMENT, jamais un reglage global.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - une zone commune a plusieurs jeux -> ici
//   - une zone propre a UN jeu          -> chez ce jeu
//   - une couleur                       -> NkUnkenyTheme.h, jamais ici
// =============================================================================
#pragma once

#include "NKCanvas/App/NkCanvasApp.h"
#include "NKGui/Core/NkGuiTypes.h"

namespace nkentseu {
	namespace unkeny {

		using nkgui::NkRect;
		using math::NkVec2f;

		/// Le point est-il dans le rectangle ? Bornes basses incluses, hautes
		/// exclues — deux rectangles jointifs ne se disputent donc jamais un
		/// pixel de frontiere.
		inline bool NkDansRect(const NkRect &r, const NkVec2f &p) noexcept {
			return p.x >= r.x && p.y >= r.y && p.x < r.x + r.w && p.y < r.y + r.h;
		}

		inline bool NkDansCercle(const NkVec2f &centre, float32 rayon, const NkVec2f &p) noexcept {
			const float32 dx = p.x - centre.x;
			const float32 dy = p.y - centre.y;
			return dx * dx + dy * dy <= rayon * rayon;
		}

		/// Taille minimale conseillee d'une cible, en pixels.
		/// Environ 9 mm au doigt — la recommandation commune d'Apple et de
		/// Google ramenee au meme nombre. A la souris le pointeur est precis, et
		/// une cible enorme gaspille de la place.
		inline float32 NkCibleMin(float32 densite, bool auDoigt) noexcept {
			const float32 base = auDoigt ? 44.f : 24.f;
			return base * (densite > 0.f ? densite : 1.f);
		}

		// =====================================================================
		// NkPlanEcran — la mise en page mesuree comme commune aux trois jeux
		//
		// Un bandeau en haut, un CARRE au milieu, et jusqu'a deux bandes de
		// boutons en bas. Le carre prend la plus petite des deux places
		// disponibles : c'est ce qui le fait tenir en portrait comme en paysage
		// SANS deux mises en page separees.
		// =====================================================================
		struct NkPlanEcran {
				NkRect bandeau{0.f, 0.f, 0.f, 0.f}; ///< l'entete
				NkRect aire{0.f, 0.f, 0.f, 0.f};	///< le carre central (plateau, scene…)
				NkRect bande1{0.f, 0.f, 0.f, 0.f};	///< premiere bande de pied
				NkRect bande2{0.f, 0.f, 0.f, 0.f};	///< seconde bande, si demandee
				NkRect retour{0.f, 0.f, 0.f, 0.f};	///< bouton carre, coin droit du bandeau
				float32 marge = 0.f;

				/// `bandes` vaut 1 ou 2 : le nombre de rangees de boutons sous
				/// l'aire. Les demander d'avance evite que l'aire soit calculee
				/// trop grande puis rognee — un rognage a posteriori decale tout
				/// ce qui a deja ete place.
				void Calculer(const renderer::NkLayoutInfo &info, int32 bandes = 1) noexcept;

				/// Decoupe une bande en `n` cases egales, avec un ecart entre
				/// elles. Sert aux rangees de boutons de siege.
				static NkRect Case(const NkRect &bande, int32 index, int32 n, float32 ecart) noexcept {
					if (n <= 0) {
						return bande;
					}
					const float32 largeur = (bande.w - ecart * static_cast<float32>(n - 1)) / static_cast<float32>(n);
					return NkRect{bande.x + static_cast<float32>(index) * (largeur + ecart), bande.y, largeur, bande.h};
				}
		};

		// =====================================================================
		// NkPlanGrille — une grille reguliere posee dans une aire carree
		//
		// Damier 8x8, 10x10, plateau 15x15 : la meme conversion (case <-> pixel)
		// dans les trois jeux.
		// =====================================================================
		struct NkPlanGrille {
				NkRect aire{0.f, 0.f, 0.f, 0.f};
				int32 cases = 8;
				float32 cellule = 0.f;

				void Poser(const NkRect &zone, int32 nbCases) noexcept {
					aire = zone;
					cases = nbCases > 0 ? nbCases : 1;
					cellule = zone.w / static_cast<float32>(cases);
				}

				NkRect CaseRect(int32 ligne, int32 colonne) const noexcept {
					return NkRect{aire.x + static_cast<float32>(colonne) * cellule,
								  aire.y + static_cast<float32>(ligne) * cellule, cellule, cellule};
				}
				NkVec2f Centre(int32 ligne, int32 colonne) const noexcept {
					return NkVec2f(aire.x + (static_cast<float32>(colonne) + 0.5f) * cellule,
								   aire.y + (static_cast<float32>(ligne) + 0.5f) * cellule);
				}
				/// Centre a coordonnees FRACTIONNAIRES — pour une piece animee,
				/// qui se trouve entre deux cases.
				NkVec2f CentreFractionnaire(float32 ligne, float32 colonne) const noexcept {
					return NkVec2f(aire.x + (colonne + 0.5f) * cellule, aire.y + (ligne + 0.5f) * cellule);
				}

				/// Rend false quand le point tombe HORS de l'aire : un clic dans
				/// le bandeau ne doit pas se traduire par une case valide.
				bool CaseSous(const NkVec2f &p, int32 &ligne, int32 &colonne) const noexcept {
					if (cellule <= 0.f || !NkDansRect(aire, p)) {
						return false;
					}
					colonne = static_cast<int32>((p.x - aire.x) / cellule);
					ligne = static_cast<int32>((p.y - aire.y) / cellule);
					return ligne >= 0 && ligne < cases && colonne >= 0 && colonne < cases;
				}
		};

	} // namespace unkeny
} // namespace nkentseu
