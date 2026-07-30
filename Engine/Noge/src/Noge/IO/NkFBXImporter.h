
#pragma once
// =============================================================================
// Nkentseu/IO/NkFBXImporter.h — Import FBX (Autodesk) legacy format
// =============================================================================
// [RÉVISION 2026-07-23] : NkFBXImporter est désormais une fine COUCHE
// D'ADAPTATION au-dessus du loader FBX from-scratch déjà en production côté
// NKRenderer (Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/NkFBXLoader.cpp,
// `renderer::LoadFBX`, zéro-STL, FBX binaire ET ASCII, arrays compressés
// deflate), et non plus une spec morte (includes cassés vers
// "Nkentseu/Modeling/NkEditableMesh.h" — n'existe pas — jamais compilée).
// Import() délègue TOUJOURS le parsing à `renderer::LoadFBX` — zéro nouvelle
// logique de lecture FBX ici.
//
// CE QUE LE LOADER RÉEL SUPPORTE (donc ce qui est réellement importé) :
//   ✅ Géométrie (Vertices/PolygonVertexIndex/Normals/UV, 1 sous-mesh par
//      Geometry FBX, triangulation fan, normales recalculées si absentes).
//   ✅ Matériaux Phong (Properties70 : DiffuseColor/DiffuseFactor/
//      TransparencyFactor/EmissiveColor/EmissiveFactor/ShininessExponent) +
//      textures connectées (DiffuseColor/NormalMap ou Bump/EmissiveColor,
//      résolues par rapport au dossier du .fbx) — `NkFBXScene::materials`
//      rempli si `importMaterials` (résolution du graphe Objects/Connections :
//      Geometry -> Model propriétaire -> 1er Material connecté).
// CE QUE LE LOADER RÉEL NE SUPPORTE PAS ENCORE (donc jamais rempli ici — pas
// d'invention, `NkFBXScene::skeletons/animations` restent VIDES) :
//   ❌ Skinning / animations FBX (Deformer/Cluster, AnimationCurve).
//   ❌ Transforms de nœuds Model (les Geometry FBX sont déjà en espace local,
//      orientation bakée dans les sommets — voir NkFBXLoader.h).
//   ❌ Textures FBX embarquées (Video binaire) — seuls les fichiers externes
//      (RelativeFilename/FileName) sont résolus.
// En conséquence, `NkFBXImportOptions::importSkeleton/importAnimation/
// importLights/importCameras` restent des no-ops honnêtes (log d'avertissement
// si demandés à true) : rien à importer côté loader réel pour ces aspects.
//
// OPTIONS RÉELLEMENT HONORÉES (post-traitement trivial sur les données déjà
// parsées, pas une nouvelle logique de parsing) :
//   • scaleFactor : échelle uniforme des positions.
//   • flipY : inverse Y (positions + normales).
//   • flipWinding : inverse le sens des triangles (swap 2 indices par tri).
//   • bakeGeomTransforms : déjà TOUJOURS le cas côté loader réel (les
//     Geometry FBX sont en espace local bakée) — option acceptée mais sans
//     effet, cf. ci-dessus.
// =============================================================================
#include "NKECS/NkECSDefines.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "Noge/Modeling/NkEditableMesh.h"
#include "Noge/ECS/Components/Animation/NkAnimation.h"

namespace nkentseu {

	using namespace math;

	struct NkFBXImportOptions {
			bool importMeshes = true;
			bool importSkeleton = true;	 ///< no-op honnête (loader réel : pas de skinning)
			bool importAnimation = true;	 ///< no-op honnête (loader réel : pas d'animation)
			bool importMaterials = true;	 ///< réellement importé (Phong + textures, cf. NkFBXScene::materials)
			bool importLights = false;		 ///< no-op honnête (loader réel : géométrie uniquement)
			bool importCameras = false;	 ///< no-op honnête (loader réel : géométrie uniquement)
			float32 scaleFactor = 0.01f;	 // FBX en cm → metres
			bool flipY = false;
			bool flipWinding = false;
			bool bakeGeomTransforms = true; ///< toujours vrai côté loader réel (voir note en tête de fichier)
	};

	struct NkFBXScene {
			bool valid = false;
			NkVector<NkEditableMesh> meshes; ///< 0 ou 1 entrée : le loader réel fusionne toutes les Geometry
			NkVector<ecs::NkSkeleton> skeletons;	   ///< toujours vide (non supporté par le loader réel)
			NkVector<ecs::NkAnimationClip> animations; ///< toujours vide (non supporté par le loader réel)

			struct MaterialData {
					NkString name, diffuseTex, normalTex, roughnessTex;
					NkVec4f diffuseColor = {0.8f, 0.8f, 0.8f, 1.f};
					float32 roughness = 0.5f, metallic = 0.f;
			};

			NkVector<MaterialData> materials; ///< rempli si importMaterials (Phong + textures externes)
			NkString lastError;
	};

	class NkFBXImporter {
		public:
			[[nodiscard]] NkFBXScene Import(const char *path, const NkFBXImportOptions &opts = {}) noexcept;

			[[nodiscard]] const NkString &GetLastError() const noexcept {
				return mLastError;
			}

		private:
			NkString mLastError;
	};
} // namespace nkentseu
