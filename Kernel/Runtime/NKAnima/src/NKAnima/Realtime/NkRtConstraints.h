#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRtConstraints.h : LES CONTRAINTES D'OS, par image (14/10/2026) :
// l'IK d'une chaine de N os, la visee (regarder une cible), les limites d'angle.
//
// D'OU VIENT CE FICHIER. Le document produit de NkAnimaEditor (design/07, § 4.4,
// manque n° 7) : « aucune contrainte n'est posee sur les os : chaine IK a plus de
// deux os, copie de rotation, limites d'angle ». NKAnima n'offrait que deux IK a
// deux os (NkRtSolveTwoBone, NkRtSolveTwoBoneHinge) et une borne de TORSION
// (NkRtTwist.h) : rien pour une queue, un tentacule, un doigt, un cou de serpent
// (le rig vise aussi les creatures, decision du 05/10) ; rien pour que les yeux du
// patient de PV3DE regardent le medecin, ou qu'un personnage de Nogee suive le
// joueur des yeux ; aucune borne de BALANCEMENT (le cone d'une epaule, d'une
// hanche). Ces trois outils sont ici, en fonctions pures sur des matrices MONDE,
// avec la meme convention que les autres solveurs du dossier :
//   world[j] = world[parent(j)] * local[j] ; les sous-arbres sont rafraichis depuis
//   `local`, qui n'est PAS mis a jour (l'appelant relit le local dans `world`).
// Rien n'est suppose des reperes des os : l'axe qui vise, l'axe de torsion sont des
// REGLAGES, dits dans le repere de l'os.
//
// LE DOSAGE EST UN VRAI POIDS (note « un poids applique chaque image devient un
// taux », 26/09). Chaque fonction part de la pose qu'on lui DONNE (l'animation de
// cette image) et rend, au poids w, la rotation locale a la fraction w du chemin
// entre cette pose et la solution : a 0,5, la moitie de l'angle, mesuree par le
// banc. Cela suppose que l'appelant redonne chaque image la pose d'ANIMATION, et
// non le resultat de l'image precedente : c'est la reference stable.
//
// CE QUI N'EST PAS FAIT, et dit : pendant la resolution, la chaine IK ne tient que
// le PLI de chaque joint (maxBend), pas les limites en ellipse ni la torsion
// (NkRtLimitBone se pose APRES : le bout peut alors manquer la cible, et la distance
// rendue le dit).
// =============================================================================

#include "NKAnima/Realtime/NkRtMath.h"
#include "NKAnima/Realtime/NkRtRig.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			// ── Les MUTATIONS des bancs (le negatif dans le meme binaire) ─────────
			// Meme principe que NkRtMutation (NkRtRig.h), dans un registre a part :
			// ce fichier est neuf et ne touche pas l'enumeration commune. Le produit ne
			// les pose jamais ; seul le banc appelle NkRtConstraintSetMutations.
			enum NkRtConstraintMutation : uint32 {
				NK_RTC_MUT_NONE = 0u,
				NK_RTC_MUT_CHAIN_NO_ANCHOR = 1u << 0,	///< chaine IK : la passe qui recolle la base est sautee
				NK_RTC_MUT_CHAIN_POLE_OFF = 1u << 1,	///< chaine IK : le pole est ignore
				NK_RTC_MUT_AIM_NO_LIMIT = 1u << 2,		///< visee : le cone est ignore
				NK_RTC_MUT_AIM_NO_UP = 1u << 3,			///< visee : l'axe haut est ignore (plus court chemin seul)
				NK_RTC_MUT_LIMIT_OFF = 1u << 4,			///< limites d'angle : rien n'est borne
				NK_RTC_MUT_WEIGHT_FULL = 1u << 5,		///< dosage : le poids est ignore (1 partout)
				NK_RTC_MUT_SPLINE_CHORD = 1u << 7,		///< chaine sur une courbe : le polygone des points au lieu de la courbe
				NK_RTC_MUT_CHAIN_NO_CHECK = 1u << 8,	///< chaines : la filiation des joints n'est plus verifiee (une chaine fausse est resolue)
				NK_RTC_MUT_SOFT_REACH_OFF = 1u << 9,	///< allonge douce : la distance est rendue telle quelle
				NK_RTC_MUT_POLE_FLAT = 1u << 10,		///< pole depuis la pose : le pied du milieu sur la droite (aucun cote)
				NK_RTC_MUT_BEND_FREE = 1u << 11,		///< chaine IK : le pli maximal des joints est ignore
				NK_RTC_MUT_COPY_WORLD_NAIVE = 1u << 6,	///< copie de rotation : l'ecart de la source est pris a son repos du MONDE (le parent qui tourne entraine la cible)
			};
			void NkRtConstraintSetMutations(uint32 bits) noexcept;
			[[nodiscard]] uint32 NkRtConstraintMutations() noexcept;
			[[nodiscard]] inline bool NkRtConstraintMutated(uint32 bit) noexcept {
				return (NkRtConstraintMutations() & bit) != 0u;
			}

			/// Le plus grand nombre de joints d'une chaine (les tableaux de travail sont sur
			/// la pile : aucune allocation par image).
			constexpr uint32 kNkRtChainMax = 32u;

			// ── 1. L'IK D'UNE CHAINE DE N OS (FABRIK) ─────────────────────────────
			struct NkRtChainIKSettings {
					uint32 iterations = 24;		///< passes aller-retour au plus
					float32 tolerance = 1e-5f;	///< distance au but suffisante (unites du monde : 0,01 mm)
					float32 weight = 1.f;		///< dosage depuis la pose donnee (0 : rien ne bouge, au bit pres)
					bool usePole = false;		///< plier les joints interieurs du cote de `pole`
					NkVec3f pole = {0.f, 0.f, 0.f}; ///< un POINT du monde (le genou d'une patte d'insecte, le dos d'une queue)
					bool useEndRotation = false; ///< le bout prend l'orientation `endRotation` (la main tournee par son controle)
					NkRtQuat endRotation;		///< orientation MONDE du bout, si useEndRotation
					/// Le PLI le plus grand a chaque joint interieur (radians, entre son os et celui
					/// d'avant) : une queue, un tentacule, une colonne ne se cassent pas en un point.
					/// Tenu PENDANT la resolution (passe de la base vers le bout) ; une cible que la
					/// chaine ne peut pas atteindre sans plier davantage n'est pas atteinte (la
					/// distance rendue le dit). pi : aucune borne. Une chaine bornee converge plus
					/// lentement : 64 passes (mesure : 9 joints a 15 degres, 0,15 mm apres 24
					/// passes, sous 0,1 mm avec 64). Avec un pole, le pole tourne
					/// ensuite chaque joint autour de ses voisins : le pli des voisins n'est plus
					/// garanti (mesure a faire le jour ou l'on combine les deux).
					float32 maxBend = 3.14159265f;
			};
			struct NkRtChainIKResult {
					float32 distance = 0.f; ///< distance restante du bout a la cible
					uint32 iterations = 0;	///< passes faites
					bool reached = false;	///< distance <= tolerance
			};

			/// La chaine `chain[0..count-1]` (de la base au bout ; chacun ANCETRE du suivant,
			/// des os intermediaires sont permis et suivent leur ancetre) est tournee pour que
			/// le bout atteigne `target`. La base ne bouge pas ; aucun os ne s'allonge (les
			/// distances entre joints de la chaine sont gardees). Hors d'allonge, la chaine
			/// se tend vers la cible. `count` de 2 a kNkRtChainMax.
			NkRtChainIKResult NkRtSolveChainIK(const NkRtRig &rig, const math::NkMat4f *local, math::NkMat4f *world, const uint32 *chain,
											   uint32 count, const NkVec3f &target, const NkRtChainIKSettings &settings);

			/// LE PASSAGE FK -> IK SANS SAUT (design/07 § 4.4 n° 2) : le POLE qui fait refaire a
			/// une IK a deux os (la chaine a, b, c) le pli de la pose donnee. C'est le milieu
			/// `b` pousse de `distance` hors de la droite (a, c), du cote ou il plie. Avec la
			/// cible = la place de `c` et l'orientation du bout = celle de `c`, l'IK rend la
			/// pose FK : on bascule sans que le membre bouge. (Le sens IK -> FK n'a besoin de
			/// rien : les rotations locales se relisent dans la pose.) Un membre tendu n'a pas
			/// de cote : on rend le pole le long de `repli` (unitaire), pris depuis `b`.
			[[nodiscard]] NkVec3f NkRtPoleFromPose(const math::NkMat4f *world, uint32 a, uint32 b, uint32 c, float32 distance,
												   const NkVec3f &repli) noexcept;

			// ── 1 bis. LA CHAINE SUR UNE COURBE (IK de courbe : colonne, queue, serpent) ──
			/// Le plus grand nombre de points de controle d'une courbe.
			constexpr uint32 kNkRtSplineMaxPoints = 16u;
			struct NkRtSplineIKSettings {
					float32 weight = 1.f;			  ///< dosage depuis la pose donnee
					uint32 samplesPerSegment = 32;	  ///< finesse de la courbe (au plus 64 par segment)
					bool useEndRotation = false;	  ///< le bout prend l'orientation `endRotation`
					NkRtQuat endRotation;
			};

			/// Pose la chaine SUR la courbe Catmull-Rom qui passe par les `pointCount` points
			/// de controle (monde, de 2 a kNkRtSplineMaxPoints ; la courbe est translatee pour
			/// partir de la base, qui ne bouge pas). Chaque joint est le premier point de la
			/// courbe, apres le precedent, a la distance de son os : aucun os ne s'allonge.
			/// Si la courbe est plus courte que la chaine, la suite continue droit dans la
			/// tangente de fin. Rend la longueur de chaine posee hors de la courbe (0 : toute
			/// la chaine est sur la courbe) ; -1 si la chaine ou les points sont refuses.
			float32 NkRtSolveSplineIK(const NkRtRig &rig, const math::NkMat4f *local, math::NkMat4f *world, const uint32 *chain, uint32 count,
									  const NkVec3f *points, uint32 pointCount, const NkRtSplineIKSettings &settings);

			// ── 1 ter. L'ALLONGE DOUCE (le genou qui ne claque pas) ─────────────────
			/// Pres de l'extension complete, l'angle d'un membre a deux os varie comme la
			/// racine de (L - d) : le dernier millimetre de cible deplie le genou de plusieurs
			/// degres d'un coup (mesure : 5,7 degres pour une jambe de 82 cm). L'allonge douce
			/// rapproche la cible : la distance d de la base devient, au-dela de L - s,
			///   d' = (L - s) + s (1 - exp(-(d - (L - s)) / s))
			/// continue et de pente 1 a L - s, jamais au-dela de L (sous L en reel ; en float,
			/// une cible tres lointaine donne L : exp(-17) se perd). Le prix, dit : au-dela de
			/// L - s le bout n'atteint plus tout a fait la cible (il s'en approche).
			[[nodiscard]] float32 NkRtSoftReachDistance(float32 d, float32 totalLength, float32 softness) noexcept;
			/// La cible rapprochee de la base `base` selon NkRtSoftReachDistance.
			[[nodiscard]] NkVec3f NkRtSoftReachTarget(const NkVec3f &base, const NkVec3f &target, float32 totalLength, float32 softness) noexcept;

			// ── 2. LA VISEE (regarder une cible) ──────────────────────────────────
			struct NkRtAimSettings {
					NkVec3f aimAxis = {0.f, 0.f, 1.f}; ///< l'axe de l'os (son repere) qui doit viser
					NkVec3f upAxis = {0.f, 1.f, 0.f};  ///< l'axe de l'os tenu vers `upWorld` (le roulis)
					NkVec3f upWorld = {0.f, 1.f, 0.f}; ///< le haut du monde
					bool keepUp = true;				   ///< faux : rotation la plus courte (le roulis de la pose est garde)
					float32 maxAngle = 3.14159265f;	   ///< le cone (radians) autour de la visee de la pose donnee
					float32 weight = 1.f;			   ///< dosage depuis la pose donnee
			};

			/// Tourne l'os `bone` pour que son axe `aimAxis` pointe vers `target`, sans sortir
			/// d'un cone de demi-angle `maxAngle` autour de la direction qu'il avait dans la
			/// pose donnee. Rend l'angle (radians) qui reste entre l'axe et la cible (0 si
			/// atteinte ; l'exces du cone sinon).
			float32 NkRtAimBone(const NkRtRig &rig, const math::NkMat4f *local, math::NkMat4f *world, uint32 bone, const NkVec3f &target,
								const NkRtAimSettings &settings);

			// ── 3. LES LIMITES D'ANGLE (balancement en ellipse + torsion) ─────────
			struct NkRtSwingTwistLimit {
					NkVec3f twistAxis = {0.f, 1.f, 0.f}; ///< l'axe de l'os (repere de reference)
					NkVec3f swingAxis = {0.f, 0.f, 1.f}; ///< le premier axe de balancement (orthogonalise a twistAxis)
					float32 swing1 = 3.14159265f;		 ///< demi-angle autour de swingAxis (radians)
					float32 swing2 = 3.14159265f;		 ///< demi-angle autour de twistAxis x swingAxis
					float32 twistMin = -3.14159265f;	 ///< torsion admise (radians)
					float32 twistMax = 3.14159265f;
			};

			/// Borne la rotation locale `local` autour de sa reference `ref` : l'ecart
			/// d = ref^-1 * local = balancement * torsion (NkRtSwingTwist) ; le balancement
			/// (vecteur rotation, composantes s1, s2) est ramene dans l'ellipse
			/// (s1/swing1)^2 + (s2/swing2)^2 <= 1 en gardant sa direction, la torsion dans
			/// [twistMin, twistMax]. Une rotation deja admise est rendue AU BIT PRES. Rend
			/// vrai si quelque chose a ete borne.
			bool NkRtLimitSwingTwist(NkRtQuat &local, const NkRtQuat &ref, const NkRtSwingTwistLimit &limit) noexcept;

			/// La meme borne sur un os de la pose MONDE : sa rotation locale est relue dans
			/// `world` (celle de son parent comprise), bornee autour de `refLocal`, reecrite ;
			/// son sous-arbre est rafraichi. Rend vrai si l'os a ete borne.
			bool NkRtLimitBone(const NkRtRig &rig, const math::NkMat4f *local, math::NkMat4f *world, uint32 bone, const NkRtQuat &refLocal,
							   const NkRtSwingTwistLimit &limit);

			/// Les deux angles de balancement (s1, s2) et la torsion (radians) de `local`
			/// autour de `ref`, tels que la borne les lit (pour les bancs et l'inspecteur).
			void NkRtMeasureSwingTwist(const NkRtQuat &local, const NkRtQuat &ref, const NkRtSwingTwistLimit &limit, float32 &s1, float32 &s2,
									   float32 &twist) noexcept;

			// ── 4. LA COPIE DE ROTATION ───────────────────────────────────────────
			/// Ou la rotation copiee s'applique.
			enum class NkRtCopySpace : uint8 {
				NK_WORLD = 0, ///< la cible tourne, autour de son pivot, comme la source a tourne DANS LE MONDE
				NK_LOCAL	  ///< la rotation locale de la source est recopiee telle quelle (reperes des deux os pareils)
			};
			struct NkRtCopyRotationSettings {
					/// La part copiee : 1 la meme rotation, 0,3 la paupiere qui suit l'oeil, -1 en
					/// miroir ; 0 : rien ne bouge, au bit pres. C'est un vrai dosage (0,5 : la moitie
					/// de l'angle).
					float32 factor = 1.f;
					NkRtCopySpace space = NkRtCopySpace::NK_WORLD;
					bool twistOnly = false;				 ///< ne copier que la torsion autour de `twistAxis`
					NkVec3f twistAxis = {0.f, 1.f, 0.f}; ///< dans le repere de reference de la source
			};

			/// COPIE LA ROTATION de l'os `source` sur l'os `target`. Ce qui est copie, c'est
			/// l'ECART de la source a sa rotation locale de reference `sourceRefLocal`
			/// (D = ref^-1 * local) : un parent qui tourne n'est pas une rotation de la
			/// source (la tete tourne, l'oeil ne bouge pas dans l'orbite : la paupiere non
			/// plus). En NK_WORLD, D est dit dans le monde par la place qu'aurait la source a
			/// sa reference sous son parent ACTUEL, et la cible le prend autour de son pivot,
			/// par-dessus sa pose donnee. En NK_LOCAL : local(cible) = local donne * D.
			/// Le sous-arbre de la cible est rafraichi. Rend l'angle applique (radians).
			float32 NkRtCopyRotation(const NkRtRig &rig, const math::NkMat4f *local, math::NkMat4f *world, uint32 source, const NkRtQuat &sourceRefLocal,
									 uint32 target, const NkRtCopyRotationSettings &settings);

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
