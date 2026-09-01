// =============================================================================
// NkUnkenyChemin.h — animer un deplacement le long d'un chemin
//
// A QUOI SERT CE FICHIER
//   Faire aller quelque chose d'un point a un autre, EN PASSANT PAR les points
//   intermediaires. Une rafle de dames, un pion de ludo qui avance de six
//   cases, un personnage qui suit un trajet.
//
// ⚠️ LE PRINCIPE QUI DECIDE DE TOUT, et il vaut au-dela de l'animation
//   L'etat du jeu est mis a jour IMMEDIATEMENT ; l'animation est PUREMENT
//   VISUELLE. L'inverse — retarder la regle le temps de l'animation — cree un
//   etat intermediaire ou le plateau ment : les coups legaux, la detection de
//   fin et l'affichage ne s'accordent plus, et chaque question « ou en est-on ? »
//   a deux reponses.
//
//   Corollaire paye le 2026-09-01, signale par Rodolf : ce que l'animation
//   RETARDE, elle doit le RETENIR. Les pieces capturees disparaissaient
//   instantanement pendant que le pion attaquant volait encore — la rafle se
//   lisait a l'envers. Ce qu'on garde n'est pas la piece, c'est son IMAGE.
//
// ⚠️ ET UN CHEMIN N'EST PAS UNE DROITE
//   Un pion de ludo qui avance de six cases fait six pas LE LONG DE LA PISTE.
//   L'interpoler du depart a l'arrivee le ferait traverser le plateau en
//   diagonale, par-dessus les ecuries et le centre. C'est la difference entre
//   une animation lisible et une animation absurde.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - une courbe d'adoucissement -> ici
//   - une animation d'autre nature (couleur, taille) -> une structure a cote,
//     pas un champ de plus ici : un chemin decrit un DEPLACEMENT
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace unkeny {

		using math::NkVec2f;

		/// Nombre maximal d'etapes d'un chemin. Une rafle de dames peut en
		/// enchainer douze ; 16 laisse une marge sans jamais TRONQUER — et une
		/// troncature silencieuse ferait sauter la fin du trajet.
		static const int32 NK_UNKENY_CHEMIN_MAX = 16;

		/// Adoucissement aux deux bouts. Un deplacement lineaire demarre et
		/// s'arrete sec, ce qui se lit comme un saut plutot qu'un mouvement.
		inline float32 NkAdoucir(float32 t) noexcept {
			const float32 u = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
			return u * u * (3.f - 2.f * u);
		}

		/// Une cloche : 0 au depart, 1 au milieu, 0 a l'arrivee. Sert a SOULEVER
		/// ce qui se deplace — une piece qui se souleve puis se repose se lit
		/// mieux qu'une piece qui glisse, et bien mieux qu'une trainee.
		inline float32 NkCloche(float32 t) noexcept {
			const float32 u = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
			return 4.f * u * (1.f - u);
		}

		/// Un deplacement en cours, le long d'une suite de points.
		struct NkChemin {
				bool actif = false;
				float32 t = 0.f;	 ///< 0..1 sur la duree totale
				float32 duree = 0.f; ///< secondes
				uint8 nbPoints = 0;	 ///< depart compris
				NkVec2f points[NK_UNKENY_CHEMIN_MAX];

				void Demarrer(float32 dureeSecondes) noexcept {
					actif = nbPoints >= 2;
					t = 0.f;
					duree = dureeSecondes > 0.f ? dureeSecondes : 0.0001f;
				}

				void Ajouter(const NkVec2f &p) noexcept {
					if (nbPoints < NK_UNKENY_CHEMIN_MAX) {
						points[nbPoints++] = p;
					}
				}

				void Vider() noexcept {
					nbPoints = 0;
					actif = false;
					t = 0.f;
				}

				/// Avance le temps. Rend true tant que l'animation court.
				bool Avancer(float32 deltaTime) noexcept {
					if (!actif) {
						return false;
					}
					t += deltaTime / duree;
					if (t >= 1.f) {
						t = 1.f;
						actif = false;
					}
					return actif;
				}

				/// Position courante.
				///
				/// `parSegment` decide de la sensation :
				///   true  — chaque segment est adouci : un pas de case en case,
				///           comme une main qui deplace une piece.
				///   false — un seul adoucissement sur tout le trajet : un vol
				///           continu, pour une piece qui saute par-dessus.
				NkVec2f Position(bool parSegment = false) const noexcept {
					if (nbPoints == 0) {
						return NkVec2f(0.f, 0.f);
					}
					if (nbPoints == 1) {
						return points[0];
					}
					const int32 segments = static_cast<int32>(nbPoints) - 1;
					const float32 tt = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
					const float32 global = parSegment ? tt : NkAdoucir(tt);
					const float32 pos = global * static_cast<float32>(segments);

					int32 seg = static_cast<int32>(pos);
					if (seg >= segments) {
						seg = segments - 1;
					}
					float32 u = pos - static_cast<float32>(seg);
					if (parSegment) {
						u = NkAdoucir(u);
					}
					const NkVec2f &a = points[seg];
					const NkVec2f &b = points[seg + 1];
					return NkVec2f(a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u);
				}

				/// L'avancement exprime en SEGMENTS parcourus. C'est ce qui permet
				/// de savoir quand le mobile a franchi le n-ieme point — donc
				/// quand une piece capturee doit enfin disparaitre.
				float32 SegmentsParcourus() const noexcept {
					if (nbPoints < 2) {
						return 0.f;
					}
					const float32 tt = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
					return tt * static_cast<float32>(nbPoints - 1);
				}

				/// L'evenement numero `i` (une prise, un declencheur) se situe
				/// entre le point i et le point i+1 : il se produit quand le
				/// mobile atteint le MILIEU de ce segment.
				bool AvantEvenement(int32 i) const noexcept {
					return SegmentsParcourus() < static_cast<float32>(i) + 0.55f;
				}
		};

	} // namespace unkeny
} // namespace nkentseu
