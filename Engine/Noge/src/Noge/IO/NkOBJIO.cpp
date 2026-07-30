// =============================================================================
// Nkentseu/IO/NkOBJIO.cpp
// =============================================================================
// Implémentation de la fine couche d'adaptation NkOBJIO -> renderer::LoadOBJ
// (voir NkOBJIO.h pour le contexte complet). Zéro logique de parsing OBJ/MTL
// dupliquée ici : tout le travail de lecture est fait par
// Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/NkOBJLoader.cpp.
// =============================================================================
#include "NkOBJIO.h"
#include "NKRenderer/Mesh/NkOBJLoader.h"
#include "NKRenderer/Mesh/NkMeshLoaderUtil.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {

	namespace {
		NkString &LastErrorStorage() noexcept {
			static NkString sLastError;
			return sLastError;
		}
	} // namespace

	NkEditableMesh NkOBJIO::Import(const char *path, const NkOBJImportOptions &opts) noexcept {
		LastErrorStorage().Clear();

		if (!path || !path[0]) {
			LastErrorStorage() = "NkOBJIO::Import: chemin vide";
			logger.Errorf("[NkOBJIO] Import: chemin vide\n");
			return NkEditableMesh();
		}

		if (!opts.triangulate) {
			// renderer::LoadOBJ triangule TOUJOURS (fan) — pas de commutateur.
			logger.Warnf("[NkOBJIO] Import('%s'): triangulate=false ignoré — renderer::LoadOBJ triangule "
						 "toujours les polygones (fan)\n",
						 path);
		}
		if (!opts.generateNormals) {
			logger.Warnf("[NkOBJIO] Import('%s'): generateNormals=false ignoré — renderer::LoadOBJ calcule "
						 "toujours des normales lissées si absentes du fichier\n",
						 path);
		}

		renderer::NkGLTFMeshData data;
		if (!renderer::LoadOBJ(NkString(path), data)) {
			LastErrorStorage() = "NkOBJIO::Import: renderer::LoadOBJ a échoué (voir logs NKRenderer)";
			return NkEditableMesh();
		}
		if (!data.IsValid()) {
			LastErrorStorage() = "NkOBJIO::Import: renderer::LoadOBJ n'a produit aucun vertex";
			return NkEditableMesh();
		}

		// Post-traitement trivial sur les données déjà parsées (pas une nouvelle
		// logique de parsing OBJ) : échelle + flip Y, si demandés.
		if (opts.scaleFactor != 1.f || opts.flipY) {
			for (uint32 i = 0; i < (uint32)data.vertices.Size(); ++i) {
				renderer::NkVertex3D &v = data.vertices[i];
				v.pos.x *= opts.scaleFactor;
				v.pos.y *= opts.scaleFactor * (opts.flipY ? -1.f : 1.f);
				v.pos.z *= opts.scaleFactor;
				if (opts.flipY) {
					v.normal.y = -v.normal.y;
				}
			}
			renderer::meshutil::ComputeBounds(data);
		}

		renderer::NkMeshDesc desc;
		desc.layout = renderer::NkVertexLayout::Default3D();
		desc.vertices = data.vertices.Data();
		desc.vertexCount = (uint32)data.vertices.Size();
		desc.indices = data.indices.Data();
		desc.indexCount = (uint32)data.indices.Size();
		desc.subMeshes = data.subMeshes;
		desc.bounds = data.bounds;
		desc.debugName = data.debugName;

		return NkEditableMesh::FromMeshDesc(desc);
	}

	bool NkOBJIO::Export(const NkEditableMesh & /*mesh*/, const char *path, const NkOBJExportOptions & /*opts*/) noexcept {
		LastErrorStorage() = "NkOBJIO::Export: NON IMPLEMENTE — aucun ecrivain OBJ/MTL cote NKRenderer (TODO)";
		logger.Warnf("[NkOBJIO] Export('%s'): NON IMPLEMENTE — aucun ecrivain OBJ/MTL cote NKRenderer (TODO)\n",
					 path ? path : "?");
		return false;
	}

	const NkString &NkOBJIO::GetLastError() noexcept {
		return LastErrorStorage();
	}

} // namespace nkentseu
