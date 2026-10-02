#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Skeleton/ — LE SQUELETTE 2D : NkSkeletonDef CONTRAINT AU PLAN, et ce que la 2D ajoute.
// -----------------------------------------------------------------------------
// FICHIER: NKAnima/Skeleton/NkSkeleton2D.h
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE (R30, plan valide par Rihen le 02/10/2026)
// =============================================================================
//  « Le squelette 2D vit dans NKAnima, comme un squelette 3D CONTRAINT AU PLAN
//  (os dans XY, rotation autour de Z), avec un mode 2D qui verrouille ces
//  contraintes dans les outils -- pas de second moteur d'animation. »
//
//  Donc : AUCUNE structure d'os nouvelle. Un squelette 2D EST un NkSkeletonDef
//  (memes os, memes matrices de repos MONDE, meme FK), dont chaque matrice reste
//  dans le plan : translation (x, y, 0), rotation autour de Z, echelle (sx, sy, 1).
//  Les clips sont des NkAnimationClip ordinaires (pistes d'os LOCALES,
//  skeletalLocal), melanges par Blend/NkAnimMix.h comme ceux de Noge. Un outil
//  3D qui ouvre le fichier voit un squelette ; un outil 2D y voit en plus :
//
//   - la LONGUEUR de chaque os (le long de son X local) : la 3D la deduit des
//     enfants, la 2D la dessine et l'IK s'en sert pour un os sans enfant ;
//   - les EMPLACEMENTS (« slots » de Spine) : une place de dessin rattachee a un
//     os, qui montre UNE de ses attaches (main ouverte / fermee, masque leve /
//     baisse) a un ORDRE DE DESSIN ; attache et ordre sont ANIMABLES -- par des
//     pistes de PROPRIETES a paliers du clip (NkSlot2DProperty), donc sans rien
//     changer au format du clip.
//
//  « Mode 2D » = NkLockToPlane : ce que les outils appliquent a toute matrice
//  qu'ils ecrivent ; NkSkeleton2D::Validate le verifie sur tout le squelette.
//
// =============================================================================
//  LE FICHIER .nkskel (CONVENTIONS_FICHIERS.md § 2)
// =============================================================================
//  [magic 'NKSK'(u32)] [version(u32)=1] [nbSections(u32)] puis des sections
//  [etiquette(u32)] [taille(u32)] [octets] -- une section inconnue est SAUTEE :
//   'BONE'  le squelette (NkSkeletonDef) : [n] par os : [nom] [parent(i32)]
//           [repos MONDE (16 f32)]  -- ce qu'un lecteur 3D lit ;
//   'PL2D'  LA MARQUE DU SQUELETTE 2D : [version=1] [n longueurs (f32)]
//           [nbEmplacements] par emplacement : [nom] [os(i32)] [nbAttaches]
//           [attaches...] [attache par defaut(i32)] [ordre par defaut(i32)].
//  Un .nkskel sans 'PL2D' est un squelette 3D : NkSkeleton2D le refuse s'il
//  sort du plan, l'accepte sinon (longueurs deduites des enfants).
//
//  CE QUI N'EST PAS ICI : aucun dessin, aucune physique (la chaine molle d'une
//  echarpe est celle du consommateur : Unkeny la confie a ses corps XPBD), aucun
//  maillage (Unkeny deforme son NkMaillage2D, Maillage/NkUnkenyMaillagePhysique).
// -----------------------------------------------------------------------------

#include "NKAnima/Blend/NkAnimMix.h"
#include "NKAnima/Skeleton/NkSkeletonDef.h"

namespace nkentseu {
	namespace anim {

		// ── LA PLACE D'UN OS DANS LE PLAN ─────────────────────────────────────────
		/// Relative a son parent (ou au repere du squelette pour une racine) :
		/// translation dans XY, rotation autour de Z (RADIANS, sens trigonometrique),
		/// echelle XY. ⚠️ Echelles POSITIVES : un miroir se fait sur l'objet entier
		/// (son transform), pas sur un os -- la decomposition TRS ne le rendrait pas.
		struct NkBone2D {
				float32 x = 0.f;
				float32 y = 0.f;
				float32 angle = 0.f;
				float32 sx = 1.f;
				float32 sy = 1.f;
		};

		/// T(x, y, 0) * Rz(angle) * S(sx, sy, 1).
		NkMat4f NkBone2DToMatrix(const NkBone2D &b);
		/// L'inverse (la matrice est supposee dans le plan ; sinon, sa projection).
		NkBone2D NkBone2DFromMatrix(const NkMat4f &m);
		NkBoneTRS NkBone2DToTRS(const NkBone2D &b);
		NkBone2D NkBone2DFromTRS(const NkBoneTRS &t);

		/// LE MODE 2D : la matrice ramenee au plan (z de translation a 0, rotation
		/// reduite a sa part autour de Z, echelle z a 1). Ce que les outils 2D
		/// appliquent a tout ce qu'ils ecrivent.
		NkMat4f NkLockToPlane(const NkMat4f &m);
		/// La matrice est-elle dans le plan (a `tol` pres) ?
		bool NkIsPlanar(const NkMat4f &m, float32 tol = 1e-4f);

		/// Angle ramene dans ]-pi, pi].
		float32 NkWrapAngle(float32 a);

		// ── LES EMPLACEMENTS (slots) ──────────────────────────────────────────────
		struct NkSlot2DDef {
				NkString name;
				int32 bone = -1;				  ///< l'os qui le porte
				NkVector<NkString> attachments;	  ///< ses attaches, par nom (Unkeny : des parties de maillage)
				int32 attachment = 0;			  ///< l'attache montree au repos (-1 = rien)
				int32 order = 0;				  ///< l'ordre de dessin au repos (le plus grand devant)
		};

		/// Le nom de la piste de PROPRIETE qui anime un emplacement (cible « » : l'objet
		/// qui porte le squelette ; genre NK_STEP ; valeur en x) :
		///   « Emplacement[<nom>].image »  l'indice de l'attache montree (-1 = rien) ;
		///   « Emplacement[<nom>].ordre »  son ordre de dessin.
		NkString NkSlot2DProperty(const NkString &slot, bool order);
		/// L'inverse : faux si `property` n'est pas une piste d'emplacement.
		bool NkParseSlot2DProperty(const NkString &property, NkString &slot, bool &order);

		// ── LE SQUELETTE 2D ───────────────────────────────────────────────────────
		struct NkSkeleton2D {
				NkSkeletonDef skeleton;		///< LA structure (repos MONDE, dans le plan)
				NkVector<float32> lengths;	///< longueur de chaque os, le long de son X local
				NkVector<NkSlot2DDef> slots;

				[[nodiscard]] uint32 Count() const noexcept {
					return skeleton.Count();
				}
				[[nodiscard]] int32 FindBone(const char *name) const noexcept {
					return skeleton.FindBone(name);
				}
				[[nodiscard]] int32 Parent(uint32 j) const noexcept {
					return skeleton.Parent(j);
				}
				void Clear();

				/// Ajoute un os A LA FIN (son parent doit exister : -1 = racine), pose
				/// relativement a son parent. Rend son indice, -1 si refuse.
				int32 AddBone(const char *name, int32 parent, const NkBone2D &local, float32 length);
				/// Ajoute un os par sa TETE et sa QUEUE en coordonnees du squelette
				/// (l'angle et la longueur en sortent). Rend son indice.
				int32 AddBoneHeadTail(const char *name, int32 parent, const NkVec2f &head, const NkVec2f &tail);

				/// Le repos LOCAL d'un os (derive du monde, comme NkSkeletonDef::BindLocal).
				[[nodiscard]] NkBone2D BindLocal2D(uint32 j) const;
				/// Change le repos LOCAL de `j` : son monde ET celui de ses descendants
				/// suivent (les locaux des descendants sont gardes).
				void SetBindLocal2D(uint32 j, const NkBone2D &local);
				/// La pose de repos, en locaux (Count() entrees).
				void BindPose2D(NkVector<NkBone2D> &out) const;

				/// FK d'une pose LOCALE 2D -> matrices MONDE (Count() entrees).
				void PoseToWorld(const NkBone2D *local, NkMat4f *world) const;
				/// La tete et la queue d'un os dans une pose MONDE.
				[[nodiscard]] NkVec2f Head(const NkMat4f *world, uint32 j) const;
				[[nodiscard]] NkVec2f Tail(const NkMat4f *world, uint32 j) const;

				/// Tout tient-il ? (topologie, longueurs, os DANS LE PLAN, emplacements
				/// sur des os existants). `why` : la premiere faute.
				bool Validate(const char **why = nullptr) const;

				/// Le clip pret a recevoir des cles de CE squelette : mode local, parents,
				/// noms (pistes et jointNames), ordre, inverse-repos ; chaque os CLE au
				/// temps 0 a son repos si `keyBind` (un os non cle d'un clip vaudrait
				/// l'identite -- le squelette s'effondrerait).
				void PrepareClip(NkAnimationClip &clip, bool keyBind = true) const;

				// .nkskel (voir l'en-tete)
				void SaveToBytes(NkVector<nk_uint8> &out) const;
				bool LoadFromBytes(const nk_uint8 *data, usize size, bool *was2D = nullptr);
				bool SaveBinary(const NkString &path) const;
				bool LoadBinary(const NkString &path, bool *was2D = nullptr);
		};

		/// Lit SEULEMENT le squelette d'un .nkskel (ce que fait un outil 3D) ; `is2D`
		/// recoit la presence de la marque 'PL2D'.
		bool NkLoadSkeletonDef(const NkString &path, NkSkeletonDef &out, bool *is2D = nullptr);

		// ── UNE CLE, UN ECHANTILLON D'OS 2D ──────────────────────────────────────
		void NkAddBoneKey2D(NkAnimationClip &clip, uint32 bone, float32 t, const NkBone2D &local,
							NkInterpMode interp = NkInterpMode::NK_LINEAR);
		/// La place locale de l'os `bone` a `t` (meme echantillonnage que NkSampleClip).
		NkBone2D NkSampleBone2D(const NkAnimationClip &clip, uint32 bone, float32 t);
		/// Une pose d'os (NkAnimPose::bones, LOCALE) en os 2D.
		void NkPoseToBones2D(const NkAnimPose &pose, NkVector<NkBone2D> &out);

		// ── LES MODELES DE DEPART ─────────────────────────────────────────────────
		enum class NkSkeleton2DTemplate : uint8 {
			NK_HUMANOID = 0, ///< de face : hanches, torse, cou, tete, bras et jambes G/D
			NK_QUADRUPED,	 ///< de profil, tourne vers +X : bassin, dos, cou, tete, queue, quatre pattes
			NK_BIRD,		 ///< de profil, tourne vers +X : corps, cou, tete, bec, ailes, queue, pattes
			NK_CREATURE,	 ///< une seule racine : aucune forme imposee, on pose le reste
			NK_HUMANOID_PROFILE, ///< de profil, tourne vers +X (le jeu de plateforme) : G derriere, D devant
			NK_COUNT
		};
		const char *NkSkeleton2DTemplateName(NkSkeleton2DTemplate t);
		/// Le modele, mis a la BOITE [lo, hi] (coordonnees du squelette : celles du
		/// maillage qu'il deformera). Les os G et D portent le suffixe « G » / « D ».
		bool NkMakeSkeleton2D(NkSkeleton2DTemplate t, const NkVec2f &lo, const NkVec2f &hi, NkSkeleton2D &out);
		/// Le nom de l'os SYMETRIQUE (« BrasG » <-> « BrasD »), vide s'il n'en a pas.
		NkString NkMirrorBoneName(const NkString &name);

		// ── L'IK A DEUX OS ────────────────────────────────────────────────────────
		/// Geometrie pure : une chaine racine -> milieu -> bout, de longueurs `l1`
		/// (racine -> milieu) et `l2` (milieu -> bout). Rend les angles MONDE des deux
		/// segments qui posent le bout sur `target`. `bendPositive` : le milieu (coude,
		/// genou) passe a GAUCHE de la droite racine -> cible (sens trigonometrique).
		/// Hors de portee, la chaine s'ALIGNE vers la cible (tendue) et rend faux.
		bool NkSolveTwoBoneIK2D(const NkVec2f &root, float32 l1, float32 l2, const NkVec2f &target, bool bendPositive,
								float32 &angle1, float32 &angle2);
		/// Sur une POSE LOCALE 2D : `mid` (l'avant-bras) et son parent (le bras)
		/// tournent pour que le point `effector` (repere LOCAL de `mid`, sans
		/// echelle : la tete d'un enfant, ou (longueur, 0) pour un bout) arrive sur
		/// `target` (repere du squelette). Rend vrai si la cible est atteinte.
		/// `weight` < 1 : le melange entre la pose et la solution (rotation).
		bool NkApplyTwoBoneIK2D(const NkSkeleton2D &s, NkBone2D *local, uint32 mid, const NkVec2f &effector,
								const NkVec2f &target, bool bendPositive, float32 weight = 1.f);
		/// Le meme geste sur une pose MONDE deja calculee (le consommateur a sa FK :
		/// Unkeny, ses os a capacite fixe) : `upper` = le parent de `mid` ; seules
		/// les rotations LOCALES des deux os changent.
		bool NkApplyTwoBoneIK2DWorld(const NkMat4f &worldUpper, const NkMat4f &worldMid, NkBone2D &localUpper,
									 NkBone2D &localMid, const NkVec2f &effector, const NkVec2f &target, bool bendPositive,
									 float32 weight = 1.f);
		/// Le coude de la chaine est-il a GAUCHE de la droite racine -> bout ? (pour
		/// garder le sens d'un coude pendant qu'on tire)
		bool NkTwoBoneBendIsPositive2D(const NkMat4f &worldUpper, const NkMat4f &worldMid, const NkVec2f &effector);

		// ── LES EMPLACEMENTS D'UNE POSE ───────────────────────────────────────────
		/// L'attache et l'ordre de chaque emplacement (slots.Size() entrees) : ceux de
		/// la POSE s'ils y sont animes (couverture > 0,5, un palier), ceux du repos sinon.
		void NkEvaluateSlots2D(const NkSkeleton2D &s, const NkAnimPose &pose, int32 *attachment, int32 *order);
		/// L'ordre de dessin : les indices des emplacements, du FOND vers l'AVANT
		/// (a ordre egal, l'ordre de declaration).
		void NkDrawOrder2D(const int32 *order, uint32 n, uint32 *sorted);

	} // namespace anim
} // namespace nkentseu
