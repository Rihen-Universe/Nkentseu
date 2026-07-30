// =============================================================================
// Nkentseu/IO/NkFBXImporter.cpp
// =============================================================================
// Implémentation de la fine couche d'adaptation NkFBXImporter ->
// renderer::LoadFBX (voir NkFBXImporter.h pour le contexte complet). Zéro
// logique de parsing FBX (binaire/ASCII/deflate) dupliquée ici : tout le
// travail est fait par
// Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/NkFBXLoader.cpp.
// =============================================================================
#include "NkFBXImporter.h"
#include "NKRenderer/Mesh/NkFBXLoader.h"
#include "NKRenderer/Mesh/NkMeshLoaderUtil.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {

	NkFBXScene NkFBXImporter::Import(const char *path, const NkFBXImportOptions &opts) noexcept {
		mLastError.Clear();
		NkFBXScene scene;

		if (!path || !path[0]) {
			mLastError = "NkFBXImporter::Import: chemin vide";
			logger.Errorf("[NkFBXImporter] Import: chemin vide\n");
			return scene;
		}

		if (opts.importSkeleton || opts.importAnimation || opts.importLights || opts.importCameras) {
			logger.Warnf("[NkFBXImporter] Import('%s'): importSkeleton/importAnimation/importLights/"
						 "importCameras sont des no-ops — renderer::LoadFBX ne supporte pas encore le "
						 "skinning/l'animation FBX ni les lumieres/cameras (voir NkFBXImporter.h)\n",
						 path);
		}

		renderer::NkGLTFMeshData data;
		if (!renderer::LoadFBX(NkString(path), data) || !data.IsValid()) {
			mLastError = "NkFBXImporter::Import: renderer::LoadFBX a échoué (voir logs NKRenderer)";
			return scene;
		}

		if (opts.scaleFactor != 1.f || opts.flipY || opts.flipWinding) {
			for (uint32 i = 0; i < (uint32)data.vertices.Size(); ++i) {
				renderer::NkVertex3D &v = data.vertices[i];
				v.pos.x *= opts.scaleFactor;
				v.pos.y *= opts.scaleFactor * (opts.flipY ? -1.f : 1.f);
				v.pos.z *= opts.scaleFactor;
				if (opts.flipY) {
					v.normal.y = -v.normal.y;
				}
			}
			if (opts.flipWinding) {
				// Post-traitement trivial : inverse le sens de chaque triangle
				// (swap 2 indices) — pas une nouvelle logique de parsing FBX.
				for (uint32 i = 0; i + 2 < (uint32)data.indices.Size(); i += 3) {
					const uint32 tmp = data.indices[i + 1];
					data.indices[i + 1] = data.indices[i + 2];
					data.indices[i + 2] = tmp;
				}
			}
			renderer::meshutil::ComputeBounds(data);
		}

		if (opts.importMeshes) {
			renderer::NkMeshDesc desc;
			desc.layout = renderer::NkVertexLayout::Default3D();
			desc.vertices = data.vertices.Data();
			desc.vertexCount = (uint32)data.vertices.Size();
			desc.indices = data.indices.Data();
			desc.indexCount = (uint32)data.indices.Size();
			desc.subMeshes = data.subMeshes;
			desc.bounds = data.bounds;
			desc.debugName = data.debugName;
			scene.meshes.PushBack(NkEditableMesh::FromMeshDesc(desc));
		}

		if (opts.importMaterials) {
			// Materiaux Phong FBX (Properties70 + textures DiffuseColor/NormalMap
			// ou Bump/EmissiveColor connectees) resolus par renderer::LoadFBX —
			// voir NkFBXLoader.cpp. Conversion directe NkGLTFMaterial ->
			// NkFBXScene::MaterialData (les chemins de texture sont les URI
			// relatives au .fbx, pas des NkMatHandle : NkMaterialSystem est en
			// cours de reecriture, meme limitation que le loader glTF).
			for (uint32 i = 0; i < (uint32)data.materials.Size(); ++i) {
				const renderer::NkGLTFMaterial &gm = data.materials[i];
				NkFBXScene::MaterialData md;
				md.name = gm.name;
				md.diffuseColor = gm.baseColorFactor;
				md.roughness = gm.roughnessFactor;
				md.metallic = gm.metallicFactor;
				if (gm.baseColorImage >= 0 && gm.baseColorImage < (int32)data.images.Size())
					md.diffuseTex = data.images[(uint32)gm.baseColorImage].uri;
				if (gm.normalImage >= 0 && gm.normalImage < (int32)data.images.Size())
					md.normalTex = data.images[(uint32)gm.normalImage].uri;
				// roughnessTex : le Phong FBX classique n'a pas de texture ORM
				// dediee — reste vide (pas d'invention de donnees).
				scene.materials.PushBack(md);
			}
		}

		// skeletons/animations restent vides : renderer::LoadFBX ne supporte
		// pas encore le skinning/l'animation FBX (voir note en tête de
		// NkFBXImporter.h) — pas d'invention de données.
		scene.valid = true;
		return scene;
	}

} // namespace nkentseu
