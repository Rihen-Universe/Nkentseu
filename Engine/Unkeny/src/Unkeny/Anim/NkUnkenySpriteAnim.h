// =============================================================================
// NkUnkenySpriteAnim.h — animation par IMAGES (le personnage qui marche)
//
// A QUOI SERT CE FICHIER
//   Faire defiler des images dans un atlas : marche, course, attaque, idle.
//   C'est ce qui distingue un jeu d'action ou un RPG d'un jeu de plateau, et
//   c'est la brique qu'aucun des trois jeux de plateau n'avait donc reclamee.
//
// ⚠️ L'ANIMATION EST UN COMPOSANT, LE SYSTEME EST AILLEURS
//   `NkAnimSprite2D` ne contient QUE des donnees : quel clip, ou on en est.
//   C'est `NkAvancerAnimations` qui fait tourner le temps. Mettre le `Update`
//   dans le composant en ferait un objet, et un objet ne se serialise pas, ne
//   s'inspecte pas et ne se copie pas entre archetypes.
//
// ⚠️ LE PIEGE DES IMAGES PAR SECONDE
//   Une animation qui avance de `dt * fps` images accumule l'erreur de virgule
//   flottante et finit par sauter une image toutes les quelques secondes — un
//   defaut qu'on ne voit qu'en regardant longtemps, et qu'on attribue alors au
//   rendu. On garde donc un TEMPS accumule et on en deduit l'image, plutot que
//   d'incrementer un indice.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - un mode de lecture (aller-retour, aleatoire) -> NkModeLecture
//   - un evenement a une image donnee (pas, coup)  -> NkClipSprite::imageEvent
//   - une machine a etats d'animation              -> PAS ici : NKAnimation
//     porte deja une HFSM et du blending. Unkeny ne la reecrit pas.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace unkeny {

		using math::NkVec2f;

		enum class NkModeLecture : uint8 {
			NK_BOUCLE = 0,	///< recommence indefiniment : marche, idle
			NK_UNE_FOIS,	///< s'arrete sur la derniere image : attaque, mort
			NK_ALLER_RETOUR ///< va et revient : balancement, respiration
		};

		/// Un clip = une bande d'images CONTIGUES dans l'atlas.
		///
		/// Contigues, et c'est un choix : un clip qui listerait des indices
		/// quelconques serait plus souple et beaucoup plus lourd a decrire. Un
		/// atlas se range par ligne, une animation occupe une ligne.
		struct NkClipSprite {
				uint16 premiere = 0; ///< index de la premiere image dans l'atlas
				uint16 nombre = 1;
				float32 imagesParSeconde = 12.f;
				NkModeLecture mode = NkModeLecture::NK_BOUCLE;

				/// Image a laquelle un evenement se declenche (impact, pas au sol).
				/// 0xFFFF = aucun. Le systeme le signale, il ne le traite pas :
				/// ce que l'evenement DECLENCHE appartient au jeu.
				uint16 imageEvent = 0xFFFFu;
		};

		/// Nombre maximal de clips par animateur. Huit couvre idle / marche /
		/// course / saut / chute / attaque / touche / mort — le vocabulaire
		/// habituel d'un personnage 2D.
		static const int32 NK_UNKENY_CLIPS_MAX = 8;

		/// Le composant. DONNEES SEULEMENT.
		struct NkAnimSprite2D {
				NkClipSprite clips[NK_UNKENY_CLIPS_MAX];
				uint8 nbClips = 0;
				uint8 clipCourant = 0;

				/// ⚠️ Un TEMPS, pas un indice. Voir l'en-tete : incrementer un
				/// indice accumule l'erreur et saute une image de temps en temps.
				float32 temps = 0.f;
				bool enPause = false;
				bool termine = false; ///< vrai en mode UNE_FOIS, une fois arrive au bout

				/// Rempli par le systeme quand l'image d'evenement vient d'etre
				/// atteinte. Le jeu le lit puis le remet a false — c'est un signal
				/// d'une trame, pas un etat.
				bool evenementAtteint = false;

				/// La geometrie de l'atlas, pour convertir un index en region UV.
				uint16 colonnes = 1;
				uint16 lignes = 1;

				void Jouer(uint8 clip) noexcept {
					// ⚠️ Rejouer le clip DEJA courant ne le redemarre pas : sinon
					// un appel a chaque trame (« joue marche ») fige le personnage
					// sur sa premiere image. C'est le defaut classique, et il se
					// presente comme « l'animation ne marche pas ».
					if (clip == clipCourant && !termine) {
						return;
					}
					clipCourant = clip < nbClips ? clip : 0;
					temps = 0.f;
					termine = false;
					evenementAtteint = false;
				}

				void Redemarrer() noexcept {
					temps = 0.f;
					termine = false;
					evenementAtteint = false;
				}

				/// L'index d'image courant DANS L'ATLAS.
				uint16 ImageCourante() const noexcept {
					if (nbClips == 0) {
						return 0;
					}
					const NkClipSprite &c = clips[clipCourant < nbClips ? clipCourant : 0];
					if (c.nombre <= 1 || c.imagesParSeconde <= 0.f) {
						return c.premiere;
					}
					const int32 total = static_cast<int32>(temps * c.imagesParSeconde);
					int32 k = 0;
					switch (c.mode) {
						case NkModeLecture::NK_UNE_FOIS:
							k = total >= c.nombre ? c.nombre - 1 : total;
							break;
						case NkModeLecture::NK_ALLER_RETOUR: {
							// Periode = 2n-2 : on ne rejoue pas les deux extremes,
							// sinon le mouvement marque un temps d'arret aux bouts.
							const int32 periode = c.nombre * 2 - 2;
							const int32 p = periode > 0 ? total % periode : 0;
							k = p < c.nombre ? p : periode - p;
							break;
						}
						default:
							k = total % c.nombre;
							break;
					}
					return static_cast<uint16>(c.premiere + k);
				}

				/// La region de l'atlas correspondante, en UV normalises.
				void RegionUV(NkVec2f &uv0, NkVec2f &uv1) const noexcept {
					const uint16 img = ImageCourante();
					const uint16 col = colonnes > 0 ? colonnes : 1;
					const uint16 lig = lignes > 0 ? lignes : 1;
					const float32 w = 1.f / static_cast<float32>(col);
					const float32 h = 1.f / static_cast<float32>(lig);
					const uint16 cx = static_cast<uint16>(img % col);
					const uint16 cy = static_cast<uint16>((img / col) % lig);
					uv0 = NkVec2f(static_cast<float32>(cx) * w, static_cast<float32>(cy) * h);
					uv1 = NkVec2f(uv0.x + w, uv0.y + h);
				}
		};

	} // namespace unkeny
} // namespace nkentseu
