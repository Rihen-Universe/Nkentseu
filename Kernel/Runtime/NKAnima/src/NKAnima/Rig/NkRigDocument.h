#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Rig/ — le rig 3D : armature editable (tete/queue/roulis), rig automatique, document du rig.
// -----------------------------------------------------------------------------
// FICHIER: NKAnima/Rig/NkRigDocument.h
// DESCRIPTION: LE DOCUMENT DU RIG : un maillage, son armature, ses poids, ses
//   formes, les reperes du rig automatique et ses controles -- et UNE pile
//   d'annulation pour tout ce qu'on y fait.
//
//   Chaque operation (editer un os, peindre un trait, placer un repere, creer
//   une forme, faire le rig automatique...) prend une PHOTO des parties qu'elle
//   touche AVANT d'agir. Annuler rend la photo ; refaire rend l'etat d'apres.
//   C'est ce que l'interface (Ctrl+Z) et l'IA (« chaque action passe par
//   l'annulation ») appellent : aucun des deux n'ecrit de code d'annulation.
//
//   Un trait de pinceau est UNE entree (BeginStroke ... EndStroke), un curseur
//   qu'on glisse aussi (les valeurs successives d'une meme forme fusionnent).
//
// Aucune interface ici : le document se teste sans fenetre (NKAnima_Tests).
// -----------------------------------------------------------------------------

#include "NKAnima/Morph/NkShapeKeys.h"
#include "NKAnima/Rig/NkArmature.h"
#include "NKAnima/Rig/NkAutoRig.h"
#include "NKAnima/Skin/NkSkinMesh.h"
#include "NKAnima/Skin/NkSkinWeights.h"

namespace nkentseu {
	namespace anim {

		/// Les PARTIES d'un document (ce qu'une photo d'annulation garde).
		enum NkRigPart : uint32 {
			NK_RIG_ARMATURE = 1u,
			NK_RIG_POIDS = 2u,
			NK_RIG_FORMES = 4u,
			NK_RIG_REPERES = 8u,
			NK_RIG_CONTROLES = 16u,
			NK_RIG_TOUT = 31u,
		};

		class NkRigDocument {
			public:
				NkSkinMesh mesh; ///< la topologie ne change pas : elle n'entre pas dans les photos
				NkArmature armature;
				NkSkinWeights weights;
				NkShapeKeySet shapes;
				NkVector<NkRigLandmark> landmarks;
				NkRigTemplate rigTemplate = NkRigTemplate::NK_HUMANOIDE;
				NkVector<NkRigControl> controls;
				/// La SYMETRIE X des gestes (os, reperes, peinture, sculpture).
				bool symetrie = true;

				/// Change a chaque modification (l'interface resynchronise le GPU).
				uint32 revision = 0;
				/// Les parties touchees depuis le dernier `TakeChanges` (masque NkRigPart).
				uint32 changes = 0;
				uint32 TakeChanges() {
					const uint32 c = changes;
					changes = 0;
					return c;
				}

				// ── Le maillage ─────────────────────────────────────────────────
				/// Pose le maillage (positions de repos, triangles) : topologie, miroir,
				/// normales, base des formes, poids redimensionnes. VIDE l'historique.
				void SetMesh(const math::NkVec3f *pos, uint32 n, const uint32 *idx, uint32 nIdx);

				// ── L'annulation ────────────────────────────────────────────────
				/// Photo des `parts` AVANT une operation. `fusion` != 0 : une operation
				/// de meme cle juste apres la precedente n'en reprend pas (un curseur).
				void Begin(const char *label, uint32 parts, uint32 fusion = 0);
				/// L'operation n'a rien change : la photo est retiree.
				void Cancel();
				/// Marque les parties modifiees (revision, changes). Appele par les
				/// operations ; a appeler apres une modification directe des champs.
				void Touch(uint32 parts);
				bool CanUndo() const {
					return !mUndo.Empty();
				}
				bool CanRedo() const {
					return !mRedo.Empty();
				}
				bool Undo();
				bool Redo();
				const char *UndoLabel() const;
				const char *RedoLabel() const;
				uint32 UndoDepth() const {
					return (uint32)mUndo.Size();
				}
				void ClearHistory() {
					mUndo.Clear();
					mRedo.Clear();
					mFusion = 0;
				}

				// ── Le mode Edition (os) ────────────────────────────────────────
				int32 AddBone(const char *name, const math::NkVec3f &head, const math::NkVec3f &tail, int32 parent, bool connected);
				int32 ExtrudeBone(uint32 i, const math::NkVec3f &tail);
				bool SubdivideBone(uint32 i, uint32 cuts);
				/// Supprime l'os : ses poids passent a son parent.
				bool DeleteBone(uint32 i);
				/// Tete / queue / os entier ; avec la symetrie, le miroir suit.
				void MoveHead(uint32 i, const math::NkVec3f &p, uint32 fusion = 0);
				void MoveTail(uint32 i, const math::NkVec3f &p, uint32 fusion = 0);
				void TranslateBone(uint32 i, const math::NkVec3f &d, uint32 fusion = 0);
				void SetRoll(uint32 i, float32 radians, uint32 fusion = 0);
				bool SetParent(uint32 i, int32 parent, bool connected);
				NkString RenameBone(uint32 i, const char *name);
				void SetDeform(uint32 i, bool deform);
				/// Cree / met a jour le miroir de l'os (et de ses descendants de cote).
				int32 SymmetrizeBone(uint32 i);
				/// Tous les os d'un cote vers l'autre ; rend le nombre de miroirs.
				uint32 SymmetrizeAll();
				/// L'os miroir (par le nom), -1 sinon.
				int32 MirrorBone(uint32 i) const;

				// ── Le rig automatique ──────────────────────────────────────────
				bool DetectLandmarks(NkRigTemplate t, NkAutoRigReport *report = nullptr);
				bool MoveLandmark(const char *id, const math::NkVec3f &p, uint32 fusion = 0);
				/// L'armature et les controles depuis les reperes ; `poidsAuto` : les
				/// poids aussi (une seule entree d'annulation pour le tout).
				bool BuildRig(NkRigTemplate t, bool poidsAuto, NkAutoWeightMethod methode, NkAutoWeightReport *report = nullptr);
				/// Depuis des JOINTS existants (un glTF deja rigge) : l'armature, sans annulation.
				void ImportJoints(const math::NkMat4f *world, const int32 *parent, const NkString *names, uint32 n);

				// ── La peau ──────────────────────────────────────────────────────
				bool AutoWeights(NkAutoWeightMethod methode, NkAutoWeightReport *report = nullptr);
				void BeginStroke(const char *label = "Peinture des poids");
				uint32 Dab(int32 bone, const NkWeightBrush &brush, const uint32 *verts, const float32 *factors, uint32 n);
				/// Fin du trait ; un trait qui n'a rien change est retire de l'historique.
				void EndStroke();
				bool InStroke() const {
					return mTrait;
				}
				bool SetVertexWeight(uint32 v, int32 bone, float32 w, bool normaliser);
				void NormalizeWeights();
				void LimitWeights(uint32 n);
				uint32 PruneWeights(float32 seuil);
				uint32 MirrorWeights(bool depuisGauche);
				NkWeightCheck CheckWeights() const {
					return NkCheckWeights(weights, armature);
				}

				// ── Les formes ───────────────────────────────────────────────────
				int32 AddShape(const char *name);
				int32 AddShapeFromPositions(const char *name, const math::NkVec3f *pos, uint32 n);
				int32 AddShapeFromMix(const char *name);
				bool RemoveShape(uint32 k);
				NkString RenameShape(uint32 k, const char *name);
				/// Un curseur : les valeurs successives d'une meme forme FUSIONNENT.
				void SetShapeValue(uint32 k, float32 v);
				bool SetShapeRelative(uint32 k, int32 relativeTo);
				bool SetShapeDriver(uint32 k, const NkShapeDriver &d);
				void MoveShapeVertices(uint32 k, const uint32 *verts, const float32 *factors, uint32 n, const math::NkVec3f &delta,
									   uint32 fusion = 0);
				int32 MirrorShape(uint32 k);
				/// Des FORMES DE VISAGE de depart (machoire_ouverte, sourire, joues_gonflees,
				/// sourcils_leves, clin_oeil.L/.R, levres_pincees), par deformation de
				/// zones de la TETE (au-dessus du repere « menton », ou de l'os « head »).
				/// Rend le nombre de formes creees ; `pourquoi` dit un refus.
				uint32 AddFaceShapes(const NkVector<NkString> &noms, NkString *pourquoi = nullptr);
				static const char *const *FaceShapeNames(uint32 &n);

			private:
				struct Photo {
						NkString label;
						uint32 parts = 0;
						NkArmature armature;
						NkSkinWeights weights;
						NkShapeKeySet shapes;
						NkVector<NkRigLandmark> landmarks;
						NkVector<NkRigControl> controls;
						NkRigTemplate rigTemplate = NkRigTemplate::NK_HUMANOIDE;
				};
				Photo Prendre(const char *label, uint32 parts) const;
				void Rendre(const Photo &p);
				NkVector<Photo> mUndo, mRedo;
				uint32 mFusion = 0;
				bool mTrait = false;
				uint32 mTraitChanges = 0;
		};

	} // namespace anim
} // namespace nkentseu
