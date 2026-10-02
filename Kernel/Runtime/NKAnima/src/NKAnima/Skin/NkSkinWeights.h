#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Skin/ — la PEAU : le maillage qu'un squelette deforme, ses poids (automatiques et peints).
// -----------------------------------------------------------------------------
// FICHIER: NKAnima/Skin/NkSkinWeights.h
// DESCRIPTION: LES POIDS DE PEAU — combien chaque os tire chaque sommet.
//
//   NkSkinWeights      jusqu'a 8 influences par sommet pendant l'EDITION (la
//                      peinture en ajoute avant qu'on limite) ; le GPU en prend 4
//   NkAutoWeights      les poids AUTOMATIQUES, deux methodes :
//                        - CHALEUR (Baran & Popovic 2007, « Automatic Weights » de
//                          Blender) : chaque os chauffe les sommets dont il est le
//                          plus proche, la chaleur DIFFUSE sur la surface
//                          (laplacien cotangent) ;
//                        - VOXELS GEODESIQUES (Dionne & de Lasa 2013, la liaison
//                          « geodesic voxel » de Maya et d'Unreal) : le volume est
//                          voxelise, la distance de chaque os se mesure A
//                          L'INTERIEUR du volume -- un bras colle au flanc ne tire
//                          pas le flanc, l'air entre eux ne compte pas ;
//                      plus la PROXIMITE simple (l'os le plus proche), pour comparer.
//   NkWeightBrush      la PEINTURE : ajouter, soustraire, lisser, remplacer ;
//                      normalisation automatique, symetrie X
//   NkCheckWeights     la VERIFICATION : os orphelins, sommets sans poids, sommes
//                      fausses, trop d'influences
// -----------------------------------------------------------------------------

#include "NKAnima/Rig/NkArmature.h"
#include "NKAnima/Skin/NkSkinMesh.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace anim {

		struct NkSkinInfluence {
				int32 bone = -1;
				float32 weight = 0.f;
		};

		class NkSkinWeights {
			public:
				static constexpr uint32 kSlots = 8u; ///< influences gardees pendant l'edition
				static constexpr uint32 kGpu = 4u;	 ///< celles que la peau du GPU lit

				void Resize(uint32 vertexCount);
				void Clear() {
					data.Clear();
					count = 0;
				}
				uint32 VertexCount() const {
					return count;
				}
				/// Le poids de `bone` sur `v` (0 s'il ne le tire pas).
				float32 Get(uint32 v, int32 bone) const;
				/// Pose le poids (0 retire l'influence). Sans place : remplace la plus faible.
				void Set(uint32 v, int32 bone, float32 w);
				/// Les influences de `v` (kSlots cases, bone = -1 pour une case vide).
				const NkSkinInfluence *Of(uint32 v) const {
					return data.Data() + (usize)v * kSlots;
				}
				NkSkinInfluence *Of(uint32 v) {
					return data.Data() + (usize)v * kSlots;
				}
				uint32 InfluenceCount(uint32 v) const;
				float32 Sum(uint32 v) const;
				/// Ramene la somme a 1 (un sommet sans poids reste sans poids).
				void Normalize(uint32 v);
				void NormalizeAll();
				/// Garde les `n` plus fortes, renormalise.
				void Limit(uint32 v, uint32 n);
				void LimitAll(uint32 n);
				/// Retire les poids sous `seuil`, renormalise ; rend le nombre retire.
				uint32 Prune(float32 seuil);
				/// Les 4 plus fortes, triees, normalisees -- ce que le GPU recoit.
				void ToGpu(uint32 v, float32 idx[4], float32 w[4]) const;
				void FromGpu(uint32 v, const float32 idx[4], const float32 w[4]);
				/// Un os a disparu (`remap` : ancien -> nouveau, -1 retire) ; son poids
				/// passe a `heritier[ancien]` s'il est >= 0 (le parent, en general).
				void RemapBones(const NkVector<int32> &remap, const NkVector<int32> &heritier);
				/// Le miroir X des poids : chaque sommet du cote `depuisGauche` (x > 0 si
				/// vrai) donne ses poids a son miroir, os remplaces par leur miroir.
				uint32 MirrorX(const NkSkinMesh &mesh, const NkArmature &arm, bool depuisGauche);

				NkVector<NkSkinInfluence> data; ///< count * kSlots
				uint32 count = 0;
		};

		// =====================================================================
		// LES POIDS AUTOMATIQUES
		// =====================================================================
		enum class NkAutoWeightMethod : uint8 { NK_CHALEUR = 0, NK_VOXELS_GEODESIQUES, NK_PROXIMITE, NK_COUNT };
		const char *NkAutoWeightMethodName(NkAutoWeightMethod m);

		struct NkAutoWeightOptions {
				NkAutoWeightMethod method = NkAutoWeightMethod::NK_CHALEUR;
				uint32 maxInfluences = 4;	 ///< limite finale (le GPU en lit 4)
				float32 pruneBelow = 0.01f;	 ///< poids retires sous ce seuil
				uint32 voxelResolution = 72; ///< voxels sur la plus grande dimension
				float32 falloff = 4.f;		 ///< exposant de la decroissance geodesique
				uint32 smoothPasses = 2;	 ///< lissages finaux sur la surface (voxels)
		};

		struct NkAutoWeightReport {
				bool ok = false;
				NkString message;
				uint32 iterations = 0;	  ///< chaleur : iterations du gradient conjugue (somme)
				uint32 voxelsSolides = 0; ///< voxels : interieur + surface
				uint32 osDeformants = 0;
		};

		/// Calcule les poids de `mesh` pour les os DEFORMANTS de `arm` (au repos).
		bool NkAutoWeights(const NkSkinMesh &mesh, const NkArmature &arm, const NkAutoWeightOptions &opt, NkSkinWeights &out,
						   NkAutoWeightReport *report = nullptr);

		// =====================================================================
		// LA PEINTURE
		// =====================================================================
		enum class NkBrushMode : uint8 { NK_AJOUTER = 0, NK_SOUSTRAIRE, NK_LISSER, NK_REMPLACER, NK_COUNT };
		const char *NkBrushModeName(NkBrushMode m);

		struct NkWeightBrush {
				NkBrushMode mode = NkBrushMode::NK_AJOUTER;
				float32 strength = 0.5f; ///< 0..1 par touche
				float32 value = 1.f;	 ///< la valeur visee (REMPLACER)
				bool autoNormalize = true;
				bool symmetryX = false;
		};

		/// UNE touche de pinceau : `verts[i]` recoit le facteur `factors[i]` (0..1,
		/// la decroissance du pinceau, calculee par qui sait ou est le pinceau).
		/// Rend le nombre de sommets changes. `mesh` sert au lissage (voisins) et a
		/// la symetrie (miroir) ; `arm`, au nom de l'os miroir.
		uint32 NkApplyBrush(NkSkinWeights &w, const NkSkinMesh &mesh, const NkArmature &arm, int32 bone, const NkWeightBrush &brush,
							const uint32 *verts, const float32 *factors, uint32 n);

		// =====================================================================
		// LA VERIFICATION
		// =====================================================================
		struct NkWeightCheck {
				uint32 vertices = 0;
				uint32 sansPoids = 0;	   ///< aucun os ne les tire
				uint32 sommesFausses = 0;  ///< somme loin de 1
				uint32 tropInfluences = 0; ///< plus de 4 os
				uint32 maxInfluences = 0;
				NkVector<int32> osOrphelins;	///< os deformants qui ne tirent aucun sommet
				NkVector<int32> osNonDeformants; ///< os non deformants qui ont des poids
				NkVector<uint32> exemplesSansPoids; ///< les premiers sommets sans poids
				bool Ok() const {
					return sansPoids == 0 && sommesFausses == 0 && tropInfluences == 0 && osOrphelins.Empty();
				}
				/// Une phrase par probleme, ou « Les poids sont sains ».
				NkString Describe(const NkArmature &arm) const;
		};
		NkWeightCheck NkCheckWeights(const NkSkinWeights &w, const NkArmature &arm, float32 tolerance = 1e-3f);

	} // namespace anim
} // namespace nkentseu
