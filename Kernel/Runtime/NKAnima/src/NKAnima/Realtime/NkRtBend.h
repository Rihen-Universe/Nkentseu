#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRtBend.h — L'OS D'AIDE D'UNE ARTICULATION QUI PLIE (coude, genou)
// (13/10/2026).
//
// D'OU VIENT CE FICHIER. Le patient virtuel de PV3DE : rendu a ses poids de peau
// d'origine, son coude a retrouve sa pointe -- mais a forte flexion le creux du
// coude s'ecrase et la pointe s'effondre. Rodolf (13/10) : « on doit l'ameliorer »,
// et l'outil vit dans NKAnima, a cote de la torsion (NkRtTwist.h).
//
// LE DEFAUT. Une peau attachee a deux os (le bras, l'avant-bras) est, autour de
// l'articulation, un MELANGE LINEAIRE de deux positions : celle que lui donne l'os
// du haut, celle que lui donne l'os du bas. Quand l'articulation plie de l'angle A,
// ces deux positions sont a A l'une de l'autre autour du pivot, et leur milieu
// tombe sur la CORDE : un point porte a parts egales par les deux os ne garde que
// cos(A / 2) de sa distance au pivot -- 34 % a 140 degres. Le volume fond des deux
// cotes : le creux se pince, la pointe rentre.
//
// LE REMEDE : UN OS D'AIDE SUR LA BISSECTRICE. Un os de plus, au meme pivot, qui
// prend la MOITIE de la flexion. La peau a cheval sur l'articulation lui est
// rattachee : un point que les deux os portaient a parts egales le suit en entier
// (il tourne de A / 2 autour du pivot : il garde TOUTE sa distance), et les autres
// se melangent entre deux os qui ne sont plus qu'a A / 2 l'un de l'autre : au pire
// cos(A / 4) -- 82 % a 140 degres. Les poids de l'artiste restent la base : la
// part de chaque os est gardee, seul le chemin entre les deux passe par le milieu.
// Tant que l'articulation ne plie pas, la peau est EXACTEMENT celle d'avant.
//
// CE QUE CE FICHIER OFFRE, en fonctions pures, sans rien supposer des reperes :
//   - NkRtBendHelperRotation : la rotation locale de l'os d'aide ;
//   - NkRtBendReweight       : la repartition des poids d'un sommet ;
//   - NkRtBlendRadius, NkRtBendRadius : LA MESURE -- ce qu'un point de peau garde de
//     sa distance au pivot, sans et avec l'os d'aide (les bancs s'en servent).
//
// ECARTE (et pourquoi) -- voir aussi la mesure sur le maillage du patient, temoin
// x6 de PV3DE :
//   - le melange a quaternions duaux : pour une charniere il garde TOUT le rayon
//     (100 %, contre 82 % au pire ici), mais ce n'est pas un outil de pose : il se
//     ferait dans le shader de peau de NKRenderer (il n'y est pas : aucun des
//     dorsaux ne l'a), pour TOUTE la peau de TOUTES les applications, epaules et
//     visage compris, et les bancs qui skinnent sur le processeur devraient le
//     refaire. Hors de portee d'un outil de NKAnima ;
//   - une forme correctrice pilotee par l'angle : juste a UN angle, fausse entre
//     deux, et il en faudrait une sculptee par personnage et par articulation ;
//   - DEUX os d'aide (au tiers et aux deux tiers) : cos(A / 6), 92 % a 140 degres,
//     pour deux os de plus par articulation -- le rendu de PV3DE en a 64, 60 pris ;
//   - relisser les poids autour de l'articulation : c'est le bras « en tuyau ».
// =============================================================================

#include "NKAnima/Realtime/NkRtMath.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			/// LA ROTATION LOCALE DE L'OS D'AIDE. Il est l'enfant du MEME parent que l'os
			/// qui plie, au meme pivot, avec la meme matrice de liaison. `childRef` : la
			/// rotation locale de l'os qui plie dans sa pose de liaison ; `childLocal` : sa
			/// rotation locale courante. L'os d'aide prend la part `share` (0,5 : la
			/// bissectrice) de ce qui les separe : childRef * (childRef^-1 * childLocal)^share.
			/// Sans flexion, c'est childRef au bit pres.
			[[nodiscard]] NkRtQuat NkRtBendHelperRotation(const NkRtQuat &childRef, const NkRtQuat &childLocal, float32 share = 0.5f) noexcept;

			/// REPARTIT LE POIDS DE PEAU d'un sommet a cheval sur l'articulation. `joints`
			/// / `weights` : ses influences (`influenceCount` au plus, poids nul = case
			/// libre). `parentSide` / `childSide` : les joints du cote du haut et du cote du
			/// bas (l'os et, s'il en a, ses os de torsion). Si le sommet est porte des deux
			/// cotes (wp et wc), l'os d'aide `helperJoint` recoit 2 x min(wp, wc), le plus
			/// petit cote est vide, le plus grand garde la difference (ses joints dans les
			/// memes proportions) : la somme des poids ne change pas, et le nombre de cases
			/// occupees n'augmente jamais. Un sommet porte d'un seul cote n'est pas touche.
			/// Rend le poids passe a l'os d'aide.
			float32 NkRtBendReweight(uint32 *joints, float32 *weights, uint32 influenceCount, const uint32 *parentSide, uint32 parentCount,
									 const uint32 *childSide, uint32 childCount, uint32 helperJoint) noexcept;

			/// LA MESURE. Ce qu'un point de peau garde de sa distance au pivot quand il est
			/// le melange lineaire de deux os tournes de `angle` (radians) l'un par rapport
			/// a l'autre, a la fraction `b` du second (0 : le premier os seul, 1 : le
			/// second) : sqrt((1-b)^2 + b^2 + 2 b (1-b) cos(angle)). A b = 0,5 : cos(angle/2).
			[[nodiscard]] float32 NkRtBlendRadius(float32 angle, float32 b) noexcept;
			/// La meme, avec un os d'aide a mi-flexion et les poids repartis par
			/// NkRtBendReweight : le point se melange entre deux os a angle / 2.
			[[nodiscard]] float32 NkRtBendRadius(float32 angle, float32 b) noexcept;

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
