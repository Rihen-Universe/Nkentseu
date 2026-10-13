#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRtTwist.h — LA TORSION DES OS (13/10/2026).
//
// D'OU VIENT CE FICHIER. Le patient virtuel de PV3DE : « quand le patient bouge
// ses mains on dirait que c'est pas logique, ca se tord tres mal, pas de muscle
// qui suit, on dirait juste un elastique tendre, mou » (Rodolf, 13/10). Rien, ni
// dans PV3DE ni ici, ne bornait ni ne repartissait la torsion d'un os : « c'est
// pour ca qu'il devait utiliser NKAnima ». L'outil manquait a NKAnima.
//
// LE DEFAUT. Un squelette sans os de torsion (Biped exporte nu : Clavicle ->
// UpperArm -> Forearm -> Hand) fait TOUTE la rotation d'un os autour de son
// propre axe a son articulation : quelques centimetres de peau se vrillent (le
// « papier de bonbon »). Un vrai poignet ne tourne presque pas : c'est
// l'avant-bras qui tourne, sur toute sa longueur. Un critere de LONGUEUR d'arete
// ne le voit pas : une peau vrillee garde ses longueurs le long de l'os.
//
// CE QUE CE FICHIER OFFRE, en fonctions pures (ni etat, ni allocation, ni
// fenetre), qui ne supposent RIEN des reperes des os (l'axe d'un os est donne) :
//
//   1. MESURER — NkRtSwingTwist : une rotation = une TORSION (l'os tourne autour
//      de son axe) suivie d'un BALANCEMENT (l'axe de l'os change de direction :
//      l'articulation plie). NkRtBoneTwist : la torsion d'un os depuis une rotation
//      de reference (sa pose de liaison). NkRtChildTwist : celle d'un os autour
//      de l'axe de son PARENT (la main autour de l'avant-bras).
//   2. BORNER ET REPORTER — NkRtLimitTwistCarry : la torsion d'un os est ramenee
//      dans sa borne, et l'excedent passe a son parent, qui tourne d'autant
//      autour de SON axe ; l'os garde sa place et son orientation dans le monde
//      (s'il est attache sur l'axe du parent, ce qui est le cas d'une main).
//   3. REPARTIR — les OS DE TORSION. Une chaine (NkRtTwistChain) place des
//      stations le long d'un os ; NkRtTwistReweight partage le poids de peau
//      qu'un sommet avait sur l'os entre les deux stations qui l'encadrent ;
//      NkRtTwistStationAngle dit de combien chaque station tourne autour de l'axe.
//      Deux regimes :
//        - NK_RT_TWIST_FOLLOW_CHILD (l'avant-bras) : la torsion vient de l'os
//          SUIVANT (la main). La peau ne tourne pas au coude, tourne avec la main
//          au poignet, et de s en s entre les deux.
//        - NK_RT_TWIST_HOLD_PARENT (le bras) : l'os lui-meme tourne contre son
//          parent. La peau reste avec l'epaule a l'origine (elle est detournee de
//          toute la torsion), suit l'os a son bout.
//
// CONVENTIONS : celles de NkRtMath.h -- quaternions (x, y, z, w), produit de
// Hamilton, rotation active ; une rotation locale d'os porte les vecteurs du
// repere de l'os dans celui de son parent. Angles en RADIANS.
// =============================================================================

#include "NKAnima/Realtime/NkRtMath.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			// ── 1. MESURER ────────────────────────────────────────────────────
			/// q = balancement * torsion. `twist` tourne autour de `axisUnit` ; l'axe de
			/// `swing` lui est perpendiculaire. `axisUnit` est exprime dans le repere de
			/// DEPART de q (celui que q transforme).
			void NkRtSwingTwist(const NkRtQuat &q, const NkVec3f &axisUnit, NkRtQuat &swing, NkRtQuat &twist) noexcept;

			/// L'angle signe (radians, de -pi exclu a pi) de la torsion de `q` autour de
			/// `axisUnit`. Le meme pour q et -q.
			[[nodiscard]] float32 NkRtTwistAngle(const NkRtQuat &q, const NkVec3f &axisUnit) noexcept;

			/// LA TORSION D'UN OS autour de SON axe, depuis sa rotation locale de reference
			/// `refLocal` (sa pose de liaison) : local = refLocal * delta, et la torsion est
			/// celle de delta autour de `boneAxisLocal` (l'axe de l'os dans SON repere).
			[[nodiscard]] float32 NkRtBoneTwist(const NkRtQuat &refLocal, const NkRtQuat &local, const NkVec3f &boneAxisLocal) noexcept;

			/// LA TORSION D'UN OS autour de l'axe de SON PARENT (la main autour de
			/// l'avant-bras) : local = delta * refLocal, et la torsion est celle de delta
			/// autour de `parentAxisLocal` (l'axe du parent dans le repere du parent).
			[[nodiscard]] float32 NkRtChildTwist(const NkRtQuat &refLocal, const NkRtQuat &local, const NkVec3f &parentAxisLocal) noexcept;

			// ── 2. BORNER ET REPORTER ────────────────────────────────────────
			/// La torsion admise a une articulation (radians), depuis la reference.
			struct NkRtTwistLimit {
					float32 minAngle = -3.14159265f;
					float32 maxAngle = 3.14159265f;
			};

			/// Ce que NkRtLimitTwistCarry a fait.
			struct NkRtTwistCarry {
					float32 before = 0.f;  ///< la torsion de l'os avant (radians)
					float32 after = 0.f;   ///< apres : dans la borne
					float32 carried = 0.f; ///< ce que le parent a pris, autour de son axe
			};

			/// BORNE la torsion de l'os enfant (autour de l'axe de son parent, depuis
			/// `childRef`) et REPORTE l'excedent sur le parent, qui tourne d'autant autour
			/// de `parentAxisLocal`. `parentLocal` et `childLocal` sont modifies ; le
			/// produit parentLocal * childLocal -- l'orientation de l'enfant dans le repere
			/// du grand-parent -- ne change pas. Rien n'est fait si la torsion est dans la
			/// borne (les deux rotations sont alors rendues telles quelles, au bit pres).
			NkRtTwistCarry NkRtLimitTwistCarry(NkRtQuat &parentLocal, NkRtQuat &childLocal, const NkRtQuat &childRef,
											   const NkVec3f &parentAxisLocal, const NkRtTwistLimit &limit) noexcept;

			/// L'INVERSE : BORNE la torsion d'un os autour de SON axe (depuis `boneRef`) et
			/// REND l'excedent a son ENFANT. Pour un squelette dont l'os ne se tord jamais
			/// (un Biped : la pronation est portee par la main, l'avant-bras ne fait que
			/// plier) et qu'une IK a tordu. L'os tourne en sens inverse autour de son axe,
			/// l'enfant d'autant dans l'autre sens (dans le repere de l'os) ; le produit
			/// boneLocal * childLocal ne change pas : l'enfant garde son orientation dans
			/// le monde, et sa place s'il est attache sur l'axe de l'os.
			/// `before` / `after` : la torsion de l'OS ; `carried` : ce que l'enfant a pris.
			NkRtTwistCarry NkRtLimitTwistToChild(NkRtQuat &boneLocal, NkRtQuat &childLocal, const NkRtQuat &boneRef,
												 const NkVec3f &boneAxisLocal, const NkRtTwistLimit &limit) noexcept;

			// ── 3. REPARTIR : LES OS DE TORSION ───────────────────────────────
			enum NkRtTwistMode : uint32 {
				NK_RT_TWIST_FOLLOW_CHILD = 0u, ///< la torsion de l'os SUIVANT se repartit de l'origine (0) au bout (1)
				NK_RT_TWIST_HOLD_PARENT = 1u,  ///< la torsion de l'os lui-meme est retenue a l'origine (-1), entiere au bout (0)
			};

			constexpr uint32 kNkRtTwistMaxStations = 8u;

			/// Une chaine de torsion sur un os. Les STATIONS sont des points de l'axe de
			/// l'os, a la fraction `s` de sa longueur (0 : son origine, 1 : son bout) ;
			/// chacune est un os de torsion, SAUF la station de l'os lui-meme (`self`),
			/// qui ne tourne pas : s = 0 en FOLLOW_CHILD, s = 1 en HOLD_PARENT.
			struct NkRtTwistChain {
					NkRtTwistMode mode = NK_RT_TWIST_FOLLOW_CHILD;
					uint32 count = 0;						 ///< le nombre de stations, l'os compris
					float32 s[kNkRtTwistMaxStations] = {}; ///< leur place le long de l'os, croissante
					/// La PART de la torsion que porte la peau de chaque station : 0, elle
					/// reste avec le parent de l'os ; 1, elle tourne de toute la torsion.
					float32 a[kNkRtTwistMaxStations] = {};
					uint32 self = 0; ///< le rang de la station de l'os lui-meme
			};
			/// La chaine a `twistBones` os de torsion (1 a kNkRtTwistMaxStations - 1),
			/// regulierement places : FOLLOW_CHILD -> l'os a 0, puis 1/n, 2/n .. 1 ;
			/// HOLD_PARENT -> 0, 1/n .. (n-1)/n, puis l'os a 1.
			/// `jointKeepsShare` dit ou va la torsion :
			///   - faux : les stations portent TOUT (parts 0, 1/n .. 1) ; l'articulation au
			///     bout de la chaine (le poignet, l'epaule) ne voit plus aucun saut ;
			///   - vrai : l'articulation GARDE SA PART (parts k/(n+1) en FOLLOW_CHILD,
			///     (k+1)/(n+1) en HOLD_PARENT) : n os de torsion font n + 1 sauts EGAUX.
			///     Pour une peau dont l'articulation a ete faite pour absorber une torsion
			///     (un Biped sans os de torsion : l'artiste y tord la main de 90 degres).
			[[nodiscard]] NkRtTwistChain NkRtMakeTwistChain(NkRtTwistMode mode, uint32 twistBones, bool jointKeepsShare = false) noexcept;

			/// De combien la station `k` tourne autour de l'axe de l'os, DANS LE REPERE DE
			/// L'OS (radians), quand la torsion a repartir vaut `twist` : a_k * twist en
			/// FOLLOW_CHILD, -(1 - a_k) * twist en HOLD_PARENT. 0 pour la station de l'os.
			[[nodiscard]] float32 NkRtTwistStationAngle(const NkRtTwistChain &chain, uint32 k, float32 twist) noexcept;

			/// La part de la torsion que la PEAU suit a la fraction `s` de l'os, entre 0
			/// (elle reste avec le parent de l'os) et 1 (elle tourne de toute la torsion) :
			/// lineaire entre les stations qui encadrent `s`. Le banc s'en sert pour dire
			/// qu'une torsion est repartie ; sans os de torsion la part saute de 0 a 1.
			[[nodiscard]] float32 NkRtTwistSkinShare(const NkRtTwistChain &chain, float32 s) noexcept;

			/// LE PLUS GRAND SAUT DE TORSION (radians, signe) entre deux voisins le long de
			/// la chaine -- ce qu'une peau attachee a ces stations doit absorber sur un seul
			/// intervalle. `twist` : la torsion a repartir ; `stationAngle[k]` : l'angle
			/// REEL de la station k autour de l'axe, dans le repere de l'os (celui de l'os
			/// lui-meme est ignore). FOLLOW_CHILD : de l'os (0) aux stations, puis a
			/// l'enfant (`twist`). HOLD_PARENT : du parent (0) aux stations (`twist` +
			/// leur angle), puis a l'os (`twist`). Sans chaine, ou des stations qui ne
			/// tournent pas : `twist` entier. C'est la MESURE d'un banc : elle lit les
			/// angles poses, elle ne les suppose pas.
			[[nodiscard]] float32 NkRtTwistMaxStep(const NkRtTwistChain &chain, float32 twist, const float32 *stationAngle) noexcept;

			/// REPARTIT LE POIDS DE PEAU d'un sommet. `joints` / `weights` : ses influences
			/// (`influenceCount` au plus, poids nul = case libre). Le poids qu'il porte sur
			/// `boneJoint` est partage entre les deux stations qui encadrent sa place `s`
			/// le long de l'os (`stationJoint[k]` : l'indice de joint de la station k ;
			/// celui de l'os a `chain.self`). S'il faut plus de cases qu'il n'y en a, les
			/// plus faibles influences sont fondues dans la plus forte : la somme des poids
			/// est gardee. Rend le poids ainsi deplace hors de sa station (0 : rien perdu).
			float32 NkRtTwistReweight(uint32 *joints, float32 *weights, uint32 influenceCount, uint32 boneJoint, float32 s,
									  const NkRtTwistChain &chain, const uint32 *stationJoint) noexcept;

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
