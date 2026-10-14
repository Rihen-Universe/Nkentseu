#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Motion/NkGaitCycle.h — UN CYCLE DE MARCHE, ANALYSE (13/10/2026).
//
// D'OU VIENT CE FICHIER. Le patient virtuel de PV3DE marchait par un cycle fabrique a
// la main (une cheville qui recule en ligne droite puis revient en sinus). L'artiste,
// lui, a capture une vraie marche : un clip ou la RACINE avance (elle n'est pas « sur
// place »), ou les pieds sont plantes dans le monde, et qui boucle -- la derniere image
// est la premiere, avancee d'une foulee. Pour jouer ce clip sur un personnage dont la
// trajectoire est decidee AILLEURS (un trajet, a vitesse constante), il faut le
// decomposer. C'est ce que fait ce fichier.
//
// LA DECOMPOSITION. Sur un cycle, la racine vaut
//        racine(u) = origine + foulee x u + reste(u)            (u : la phase, de 0 a 1)
//   - `foulee` : le deplacement par cycle -- la part que l'hote produit lui-meme, en
//     avancant a la vitesse `Speed()` ;
//   - `reste` : ce que la racine fait EN PLUS (le rebond du bassin, son elan a chaque
//     pas) : periodique, nul aux deux bouts ; l'hote l'ajoute a sa trajectoire ;
//   - chaque PIED est donne dans le REPERE MOYEN (celui qui avance a vitesse constante,
//     `origine + foulee x u`) : un pied plante dans le monde y RECULE a la vitesse de la
//     marche. Un hote qui avance a `Speed()` et pose le pied a `repere + piste(u)` le
//     garde donc plante, sans rien resoudre.
// Tout est lineaire : un personnage aux jambes plus courtes multiplie la foulee, le
// reste et les pistes par le meme rapport, et ses pieds restent plantes.
//
// CE QUE CE FICHIER NE FAIT PAS : il ne pose aucun os (les rotations du clip passent
// par NkSampleClip et la pile de couches ; la jambe va a sa cible par NkRtSolveTwoBone).
// Le monde est Y vers le haut, comme le reste de NKAnima.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace anim {

		/// UN PIED du cycle (sa cheville).
		struct NkGaitFoot {
				NkVector<math::NkVec3f> track; ///< la cheville dans le REPERE MOYEN, une par image
				NkVector<uint8> planted;	   ///< 1 : a cette image le pied est au sol (il ne bouge pas dans le monde)
				float32 floor = 0.f;		   ///< la hauteur de la cheville la plus basse du cycle (pied a plat), monde du clip
				float32 plantedShare = 0.f;	   ///< la part du cycle ou il est plante
				float32 slip = 0.f;			   ///< MESURE du clip : son plus grand deplacement, plante, d'une image a l'autre
		};

		/// LE CYCLE. `N` images regulierement espacees ; la derniere ferme la boucle.
		struct NkGaitCycle {
				float32 duration = 0.f;			   ///< secondes par cycle (deux pas)
				math::NkVec3f origin = {0.f, 0.f, 0.f}; ///< la racine a la premiere image
				math::NkVec3f stride = {0.f, 0.f, 0.f}; ///< le deplacement de la racine par cycle
				NkVector<math::NkVec3f> residual;  ///< la racine moins (origine + foulee x u) ; nul aux deux bouts
				NkVector<NkGaitFoot> feet;

				[[nodiscard]] bool Valid() const noexcept {
					return duration > 0.f && residual.Size() >= 2;
				}
				/// La longueur d'une foulee (un cycle).
				[[nodiscard]] float32 StrideLength() const noexcept;
				/// La vitesse moyenne : la foulee par la duree. C'est a cette vitesse que l'hote avance.
				[[nodiscard]] float32 Speed() const noexcept;
		};

		/// ANALYSE un cycle. `root` et chaque `feet[i]` : la meme suite de `N` images (N >= 3),
		/// dans le monde du clip, sur `duration` secondes ; la derniere image est la premiere
		/// avancee d'une foulee. `plantSpeed` : la vitesse (unites du clip par seconde) sous
		/// laquelle une cheville est dite plantee ; <= 0 : un huitieme de la vitesse de marche.
		/// Faux si les suites ne se correspondent pas.
		bool NkAnalyseGait(const NkVector<math::NkVec3f> &root, const NkVector<NkVector<math::NkVec3f>> &feet, float32 duration, float32 plantSpeed,
						   NkGaitCycle &out);

		/// Une suite PERIODIQUE du cycle (N images, la derniere valant la premiere), lue a la
		/// phase `phase` : pour ce que l'hote range a cote du cycle (la rotation de la racine).
		[[nodiscard]] math::NkVec3f NkGaitSample(const NkVector<math::NkVec3f> &track, float32 phase) noexcept;
		/// Ce que la racine fait EN PLUS de la vitesse constante, a la phase `phase` (periodique).
		[[nodiscard]] math::NkVec3f NkGaitRootResidual(const NkGaitCycle &g, float32 phase) noexcept;
		/// La cheville du pied `foot` dans le repere moyen, a la phase `phase` (periodique).
		[[nodiscard]] math::NkVec3f NkGaitFootPoint(const NkGaitCycle &g, uint32 foot, float32 phase) noexcept;
		/// De combien la cheville est AU-DESSUS de sa hauteur a plat, a la phase `phase`.
		[[nodiscard]] float32 NkGaitFootLift(const NkGaitCycle &g, uint32 foot, float32 phase) noexcept;
		/// Le pied est-il plante a cette phase (l'image la plus proche) ?
		[[nodiscard]] bool NkGaitFootPlanted(const NkGaitCycle &g, uint32 foot, float32 phase) noexcept;

	} // namespace anim
} // namespace nkentseu
