#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Morph/ — les FORMES (shape keys de Blender, blend shapes de Maya et d'UE5, morph targets de glTF).
// -----------------------------------------------------------------------------
// FICHIER: NKAnima/Morph/NkShapeKeys.h
// DESCRIPTION: les FORMES d'un maillage : une BASE et des CIBLES, chacune avec
//   sa valeur (un curseur 0..1), relative a la base ou a une AUTRE forme.
//
//   RESULTAT = base + somme_k  valeur_k * (forme_k - relative_k)
//
//   C'est exactement la regle des formes RELATIVES de Blender : une forme
//   « sourire_large » relative a « sourire » n'ajoute que ce qui DEPASSE le
//   sourire. A 0,5, une forme relative a la base rend la MOYENNE EXACTE de la
//   base et de la cible (c'est le temoin du banc).
//
//   PILOTES (formes « correctives ») : la valeur d'une forme peut etre DEDUITE
//   de la rotation d'un os (le coude plie a 90 deg -> la forme « coude_plie »
//   a 1), par une rampe lineaire bornee -- le pilote d'un os de Blender, le
//   « pose driver » d'UE5. Une forme peut aussi etre jouee par une PISTE du
//   clip (NkAnimationClip::morphTracks, appariee par NOM).
//
// ⚠️ LES FORMES SONT STOCKEES EN POSITIONS ABSOLUES (comme Blender), pas en
//    deltas : « relative a » se calcule alors sans cas particulier, et une
//    forme survit au changement de sa relative. glTF, lui, veut des deltas
//    par rapport a la base : la conversion est faite a l'import / a l'export.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace anim {

		struct NkSkinMesh;

		/// (05/10) LES PINCEAUX DE SCULPTURE d'une forme (Blender) : SAISIR (le
		/// deplacement, MoveVertices), LISSER (le decalage tend vers celui des
		/// voisins), GONFLER / DEGONFLER (le long de la normale), EFFACER (retour
		/// vers la forme de reference).
		enum class NkShapeBrush : uint8 {
			NK_ShapeBrush_Saisir = 0,
			NK_ShapeBrush_Lisser,
			NK_ShapeBrush_Gonfler,
			NK_ShapeBrush_Degonfler,
			NK_ShapeBrush_Effacer,
			NK_ShapeBrush_Count
		};
		const char *NkShapeBrushName(NkShapeBrush b);

		class NkAnimationClip;

		/// Ce qui pilote une forme (en plus de son curseur).
		enum class NkShapeDriverKind : uint8 {
			NK_ShapeDriverKind_Aucun = 0,
			NK_ShapeDriverKind_Rotation_Os, ///< l'angle d'un os par rapport a son repos
		};

		struct NkShapeDriver {
				NkShapeDriverKind kind = NkShapeDriverKind::NK_ShapeDriverKind_Aucun;
				NkString bone;		   ///< le nom de l'os qui pilote
				uint8 axis = 3;		   ///< 0 X, 1 Y, 2 Z (angle autour de l'axe local), 3 angle total
				float32 angleMin = 0.f;	 ///< degres : la forme vaut 0 ici...
				float32 angleMax = 90.f; ///< ...et 1 la
		};

		struct NkShapeKey {
				NkString name;
				NkVector<math::NkVec3f> positions; ///< absolues, une par sommet
				float32 value = 0.f;
				float32 sliderMin = 0.f, sliderMax = 1.f;
				int32 relativeTo = 0; ///< la forme de reference (0 = la base)
				bool mute = false;
				NkShapeDriver driver;
		};

		class NkShapeKeySet {
			public:
				/// keys[0] est la BASE (« Base ») ; elle ne se pondere pas.
				NkVector<NkShapeKey> keys;

				uint32 Count() const {
					return (uint32)keys.Size();
				}
				uint32 VertexCount() const {
					return keys.Empty() ? 0u : (uint32)keys[0].positions.Size();
				}
				bool HasBasis() const {
					return !keys.Empty();
				}
				int32 Find(const char *name) const;
				/// Pose la base (efface les formes si le nombre de sommets change).
				void SetBasis(const math::NkVec3f *pos, uint32 n);
				/// Une forme NEUVE, copie de la base (on la sculpte ensuite).
				int32 AddFromBasis(const char *name);
				/// Une forme depuis des POSITIONS donnees (« forme depuis la pose » : le
				/// maillage deforme tel qu'il est a l'ecran devient une cible).
				int32 AddFromPositions(const char *name, const math::NkVec3f *pos, uint32 n);
				/// Le MELANGE courant devient une forme (« New Shape from Mix » de Blender).
				int32 AddFromMix(const char *name);
				bool Remove(uint32 k);
				/// Renomme (unique) ; rend le nom pose.
				NkString Rename(uint32 k, const char *name);
				NkString UniqueName(const char *souhait, int32 sauf = -1) const;
				void SetValue(uint32 k, float32 v);
				/// Deplace des sommets d'une forme. `symetrie` : le miroir de chaque
				/// sommet (table `mirror`, -1 aucun) recoit le deplacement en miroir X.
				void MoveVertices(uint32 k, const uint32 *verts, const float32 *factors, uint32 n, const math::NkVec3f &delta,
								  bool symetrie, const NkVector<int32> *mirror);
				/// (05/10) Un coup de PINCEAU de sculpture (sauf Saisir : MoveVertices).
				/// `amount` : la hauteur de Gonfler / Degonfler (unite du maillage) ;
				/// `mesh` : les voisins (Lisser), les normales (Gonfler), le miroir.
				void SculptVertices(uint32 k, NkShapeBrush brush, const uint32 *verts, const float32 *factors, uint32 n, float32 amount,
									const NkSkinMesh &mesh, bool symetrie);
				/// Le MIROIR X d'une forme : « sourire.L » -> « sourire.R » (nouvelle
				/// forme ou mise a jour). Rend l'indice, -1 si refus.
				int32 Mirror(uint32 k, const NkVector<int32> &mirror, const char *nouveauNom = nullptr);

				/// Le resultat a des valeurs donnees (null : les valeurs des formes).
				void Evaluate(NkVector<math::NkVec3f> &out, const float32 *values = nullptr) const;
				/// L'apport d'UNE forme a sa valeur (pour la visualiser seule).
				void Delta(uint32 k, NkVector<math::NkVec3f> &out) const;
				/// Les PILOTES : pour chaque forme pilotee, sa valeur depuis les rotations
				/// LOCALES des os (par rapport au repos). `boneNames`, `restLocal`,
				/// `poseLocal` : un par os. Rend le nombre de formes changees.
				uint32 ApplyDrivers(const NkString *boneNames, const math::NkMat4f *restLocal, const math::NkMat4f *poseLocal,
									uint32 boneCount);
				/// Les valeurs d'une PISTE de clip (morphTracks, par nom) a `t`. Rend le
				/// nombre de formes jouees.
				uint32 ApplyClip(const NkAnimationClip &clip, float32 t);

				/// L'angle (degres) que mesure un pilote entre deux reperes locaux.
				static float32 DriverAngle(const NkShapeDriver &d, const math::NkMat4f &restLocal, const math::NkMat4f &poseLocal);
		};

	} // namespace anim
} // namespace nkentseu
