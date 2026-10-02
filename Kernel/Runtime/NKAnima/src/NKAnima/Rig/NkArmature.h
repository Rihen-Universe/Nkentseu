#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Rig/ — le rig 3D : armature editable (tete/queue/roulis), rig automatique, document du rig.
// -----------------------------------------------------------------------------
// FICHIER: NKAnima/Rig/NkArmature.h
// DESCRIPTION: L'ARMATURE EDITABLE (le « mode Edition » de Blender) : des os
//   definis par une TETE, une QUEUE et un ROULIS, en espace modele, au repos.
//
// =============================================================================
//  CE QUE C'EST, ET CE QUE CE N'EST PAS
// =============================================================================
//  NkSkeletonDef (Skeleton/) est l'actif que la PEAU consomme : des matrices de
//  repos et leurs inverses. Il ne dit ni ou un os SE TERMINE, ni comment il
//  ROULE autour de lui-meme -- deux choses qu'un artiste manipule toute la
//  journee dans Blender, Maya ou le Skeleton Editor d'UE5. Cette structure les
//  porte, et produit l'actif (RestJointWorld) quand on la valide.
//
//  LE REPERE D'UN OS (convention de Blender, `vec_roll_to_mat3`) :
//    Y = tete -> queue ; X et Z = la rotation minimale qui amene (0,1,0) sur Y,
//    puis le roulis autour de Y. Origine = la tete.
//
//  LE REPERE DU JOINT = repere de l'os * `jointOffset`. Un os cree ici a un
//  decalage IDENTITE. Un os IMPORTE (glTF, FBX) garde son repere d'origine,
//  quel qu'il soit (bien des exporteurs mettent X le long de l'os) : le decalage
//  est calcule une fois a l'import (FromJoints). Editer la tete, la queue ou le
//  roulis fait tourner le joint AVEC l'os -- les pistes d'animation, ecrites
//  dans les reperes de joints, restent lisibles.
//
//  ⚠️ CONVENTIONS DE L'ESPACE MODELE : Y en haut (glTF). Le cote GAUCHE du
//     personnage est +X (face a +Z, comme glTF) -- c'est le cote « .L » des
//     noms (Blender met aussi .L du cote +X).
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace anim {

		/// Le cote d'un os, lu dans son NOM d'abord (.L/.R, Left/Right...), sinon
		/// dans sa position (x > 0 : gauche).
		enum class NkBoneSide : uint8 { NK_CENTRE = 0, NK_GAUCHE, NK_DROITE };

		/// La forme d'affichage d'un os (Blender : Octahedral, Stick, Envelope, B-Bone).
		enum class NkBoneDisplay : uint8 { NK_OCTAEDRE = 0, NK_BATON, NK_ENVELOPPE, NK_BBONE, NK_COUNT };

		struct NkArmatureBone {
				NkString name;
				/// Identite PERSISTANTE (renommer, supprimer puis annuler, deplacer dans
				/// la liste : l'uid suit l'os). Les pistes d'animation s'y rattachent.
				uint32 uid = 0;
				int32 parent = -1;
				math::NkVec3f head{0.f, 0.f, 0.f};
				math::NkVec3f tail{0.f, 0.1f, 0.f};
				/// Roulis autour de l'axe tete -> queue, en RADIANS.
				float32 roll = 0.f;
				/// Tete collee a la queue du parent (les deux bougent ensemble).
				bool connected = false;
				/// L'os deforme la peau (les os de controle, eux, non).
				bool deform = true;
				/// Groupe de couleur (-1 : selon le cote).
				int32 group = -1;
				/// Rayon de l'enveloppe (affichage, poids par enveloppe) ; 0 = 10 % de la longueur.
				float32 envelope = 0.f;
				/// Segments d'un B-Bone (affichage) ; 1 = os rigide.
				uint8 segments = 1;
				/// Repere du joint = repere de l'os * jointOffset (identite pour un os ne ici).
				math::NkMat4f jointOffset = math::NkMat4f::Identity();
		};

		class NkArmature {
			public:
				NkVector<NkArmatureBone> bones;

				// ── Lecture ──────────────────────────────────────────────────────
				uint32 Count() const {
					return (uint32)bones.Size();
				}
				int32 Find(const char *name) const;
				int32 FindUid(uint32 uid) const;
				float32 Length(uint32 i) const;
				/// Le repere de l'OS (Y = tete -> queue, roulis), origine a la tete.
				math::NkMat4f BoneMatrix(uint32 i) const;
				/// Le repere du JOINT au repos (= BoneMatrix * jointOffset) : ce que la peau voit.
				math::NkMat4f JointMatrix(uint32 i) const;
				/// Rayon d'enveloppe effectif (10 % de la longueur si non pose).
				float32 EnvelopeRadius(uint32 i) const;
				NkBoneSide Side(uint32 i) const;
				/// Vrai si `ancestor` est `i` ou l'un de ses parents.
				bool IsAncestor(uint32 ancestor, uint32 i) const;
				/// Ordre topologique (parent avant enfant) ; faux en cas de cycle.
				bool Topo(NkVector<uint32> &out) const;
				/// Les reperes de JOINTS au repos, un par os (la pose de liaison, monde).
				void RestJointWorld(NkVector<math::NkMat4f> &out) const;
				/// Le repere LOCAL de repos d'un joint (relatif a son parent).
				math::NkMat4f RestJointLocal(uint32 i) const;

				// ── Operations du mode Edition (rendent l'indice touche, -1 si refus) ──
				int32 Add(const char *name, const math::NkVec3f &head, const math::NkVec3f &tail, int32 parent = -1,
						  bool connected = false);
				/// Un enfant CONNECTE qui part de la queue de `i` et finit en `tail`.
				int32 Extrude(uint32 i, const math::NkVec3f &tail);
				/// Coupe `i` en `cuts + 1` os connectes ; les enfants passent au dernier.
				bool Subdivide(uint32 i, uint32 cuts);
				/// Supprime `i` : ses enfants passent a son parent (deconnectes).
				/// `remap` (optionnel) : ancien indice -> nouveau (-1 pour `i`).
				bool Remove(uint32 i, NkVector<int32> *remap = nullptr);
				/// Deplace la tete (une tete connectee entraine la queue du parent).
				void SetHead(uint32 i, const math::NkVec3f &p);
				/// Deplace la queue (les enfants connectes suivent).
				void SetTail(uint32 i, const math::NkVec3f &p);
				/// Deplace l'os entier (tete et queue).
				void Translate(uint32 i, const math::NkVec3f &d);
				void SetRoll(uint32 i, float32 radians);
				/// Change de parent ; refuse un cycle. `connected` : la tete saute a la
				/// queue du parent.
				bool SetParent(uint32 i, int32 parent, bool connected);
				/// Renomme ; un nom deja pris recoit un suffixe « .001 ». Rend le nom pose.
				NkString Rename(uint32 i, const char *name);
				/// Le nom libre le plus proche de `souhait` (« Os », « Os.001 »...).
				NkString UniqueName(const char *souhait, int32 sauf = -1) const;
				/// La SYMETRIE X : cree (ou met a jour) le miroir de `i` -- nom miroir
				/// (.L <-> .R ; un os de cote sans suffixe recoit .L/.R), tete et queue
				/// x -> -x, roulis oppose, parent miroir s'il existe. Rend l'indice du
				/// miroir ; -1 pour un os du centre.
				int32 Symmetrize(uint32 i);

				// ── Import ───────────────────────────────────────────────────────
				/// Depuis des JOINTS (glTF, FBX, clip) : la tete au joint, la queue vers
				/// les enfants (ou prolongee pour une feuille), le roulis et le decalage
				/// calcules pour que JointMatrix(i) rende EXACTEMENT `world[i]`.
				static NkArmature FromJoints(const math::NkMat4f *world, const int32 *parent, const NkString *names, uint32 n);

				// ── Noms et cotes ────────────────────────────────────────────────
				/// « bras.L » -> « bras.R », « LeftArm » -> « RightArm », « main_gauche » ->
				/// « main_droite »... Un nom sans cote rend une chaine VIDE.
				static NkString MirrorName(const char *name);
				/// Le cote dit par le nom seul (CENTRE si le nom n'en dit rien).
				static NkBoneSide SideOfName(const char *name);

				/// Le compteur d'uid (le prochain a donner).
				uint32 nextUid = 1;

			private:
				uint32 NewUid() {
					return nextUid++;
				}
		};

	} // namespace anim
} // namespace nkentseu
