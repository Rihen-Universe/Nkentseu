// =============================================================================
// Nkentseu/IO/NkGLTFIO.cpp
// =============================================================================
// Implémentation de la fine couche d'adaptation NkGLTFIO -> renderer::LoadGLTF
// (voir NkGLTFIO.h pour le contexte complet). Zéro logique de parsing
// JSON/GLB/accessor dupliquée ici : tout le travail est fait par
// Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/NkGLTFLoader.cpp — ce fichier
// ne fait que remapper renderer::NkGLTFMeshData (déjà entièrement parsé) vers
// les types NkGLTFScene/NkGLTFMeshData/... de Noge.
// =============================================================================
#include "NkGLTFIO.h"
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKRenderer/Mesh/NkMeshLoaderUtil.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {

	namespace {

		// Reconstruit, pour chaque node, l'index de son node PARENT (-1 = racine).
		// Le loader réel (renderer::NkGLTFNode) ne stocke que les listes
		// `children` — remappage trivial des données déjà parsées, pas une
		// nouvelle logique de parsing.
		NkVector<int32> BuildParentMap(const NkVector<renderer::NkGLTFNode> &nodes) noexcept {
			NkVector<int32> parentOf;
			parentOf.Resize((uint32)nodes.Size(), -1);
			for (uint32 i = 0; i < (uint32)nodes.Size(); ++i) {
				const auto &children = nodes[i].children;
				for (uint32 c = 0; c < (uint32)children.Size(); ++c) {
					const int32 childIdx = children[c];
					if (childIdx >= 0 && (uint32)childIdx < parentOf.Size()) {
						parentOf[(uint32)childIdx] = (int32)i;
					}
				}
			}
			return parentOf;
		}

		NkGLTFMaterialData::AlphaMode MapAlphaMode(int32 glTFAlphaMode) noexcept {
			switch (glTFAlphaMode) {
				case 1:
					return NkGLTFMaterialData::AlphaMode::Mask;
				case 2:
					return NkGLTFMaterialData::AlphaMode::Blend;
				default:
					return NkGLTFMaterialData::AlphaMode::Opaque;
			}
		}

		NkString ImageURI(const renderer::NkGLTFMeshData &data, int32 imageIdx) noexcept {
			if (imageIdx < 0 || (uint32)imageIdx >= (uint32)data.images.Size()) {
				return NkString();
			}
			const renderer::NkGLTFImage &img = data.images[(uint32)imageIdx];
			return (img.valid && !img.uri.Empty()) ? img.uri : NkString();
		}

	} // namespace

	NkGLTFScene NkGLTFImporter::Import(const char *path, const NkGLTFImportOptions &opts) noexcept {
		mLastError.Clear();
		NkGLTFScene scene;

		if (!path || !path[0]) {
			mLastError = "NkGLTFImporter::Import: chemin vide";
			logger.Errorf("[NkGLTFIO] Import: chemin vide\n");
			return scene;
		}

		renderer::NkGLTFMeshData data;
		if (!renderer::LoadGLTF(NkString(path), data) || !data.IsValid()) {
			mLastError = "NkGLTFImporter::Import: renderer::LoadGLTF a échoué (voir logs NKRenderer)";
			return scene;
		}

		if (opts.scaleFactor != 1.f || opts.flipY) {
			logger.Warnf("[NkGLTFIO] Import('%s'): scaleFactor/flipY appliqués UNIQUEMENT à la géométrie du "
						 "mesh — les transforms de nodes/squelette ne sont PAS re-échelonnés (limitation "
						 "connue de cet adaptateur)\n",
						 path);
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

		// ── Mesh fusionné ────────────────────────────────────────────────────
		renderer::NkMeshDesc desc;
		desc.layout = renderer::NkVertexLayout::Default3D();
		desc.vertices = data.vertices.Data();
		desc.vertexCount = (uint32)data.vertices.Size();
		desc.indices = data.indices.Data();
		desc.indexCount = (uint32)data.indices.Size();
		desc.subMeshes = data.subMeshes;
		desc.bounds = data.bounds;
		desc.debugName = data.debugName;

		NkGLTFMeshData meshEntry;
		meshEntry.name = data.debugName;
		meshEntry.mesh = NkEditableMesh::FromMeshDesc(desc);
		meshEntry.materialIndex = 0;
		scene.meshes.PushBack(static_cast<NkGLTFMeshData &&>(meshEntry));

		// ── Matériaux (remappage direct de renderer::NkGLTFMaterial) ────────
		for (uint32 i = 0; i < (uint32)data.materials.Size(); ++i) {
			const renderer::NkGLTFMaterial &m = data.materials[i];
			NkGLTFMaterialData md;
			md.name = m.name;
			md.baseColorFactor = m.baseColorFactor;
			md.baseColorTexture = ImageURI(data, m.baseColorImage);
			md.metallicFactor = m.metallicFactor;
			md.roughnessFactor = m.roughnessFactor;
			md.metallicRoughnessTexture = ImageURI(data, m.metallicRoughnessImage);
			md.normalTexture = ImageURI(data, m.normalImage);
			md.normalScale = m.normalScale;
			md.occlusionTexture = ImageURI(data, m.occlusionImage);
			md.occlusionStrength = m.occlusionStrength;
			md.emissiveFactor = m.emissiveFactor;
			md.emissiveTexture = ImageURI(data, m.emissiveImage);
			md.alphaMode = MapAlphaMode(m.alphaMode);
			md.alphaCutoff = m.alphaCutoff;
			md.doubleSided = m.doubleSided;
			scene.materials.PushBack(md);
		}

		// ── Nodes (TRS + hiérarchie recalculée) ──────────────────────────────
		const NkVector<int32> parentOf = BuildParentMap(data.nodes);
		for (uint32 i = 0; i < (uint32)data.nodes.Size(); ++i) {
			const renderer::NkGLTFNode &n = data.nodes[i];
			NkGLTFNodeData nd;
			nd.translation = n.translation;
			nd.rotation.x = n.rotation.x;
			nd.rotation.y = n.rotation.y;
			nd.rotation.z = n.rotation.z;
			nd.rotation.w = n.rotation.w;
			nd.scale = n.scale;
			nd.hasMatrix = n.hasMatrix;
			nd.matrix = n.matrix;
			nd.meshIndex = n.mesh;
			nd.parentIndex = parentOf[i];
			for (uint32 c = 0; c < (uint32)n.children.Size(); ++c) {
				nd.children.PushBack((uint32)n.children[c]);
			}
			scene.nodes.PushBack(nd);
			if (nd.parentIndex < 0) {
				scene.rootNodes.PushBack(i);
			}
		}

		// ── Squelette (si skinné) ─────────────────────────────────────────────
		if (data.isSkinned && !data.skinJoints.Empty()) {
			NkVector<int32> nodeToJoint;
			nodeToJoint.Resize((uint32)data.nodes.Size(), -1);
			for (uint32 j = 0; j < (uint32)data.skinJoints.Size(); ++j) {
				const int32 nodeIdx = data.skinJoints[j];
				if (nodeIdx >= 0 && (uint32)nodeIdx < nodeToJoint.Size()) {
					nodeToJoint[(uint32)nodeIdx] = (int32)j;
				}
			}

			ecs::NkSkeleton skel;
			const uint32 boneCount = math::NkMin<uint32>((uint32)data.skinJoints.Size(), ecs::NkSkeleton::kMaxBones);
			if (boneCount < (uint32)data.skinJoints.Size()) {
				logger.Warnf("[NkGLTFIO] Import('%s'): squelette tronqué à %u os (ecs::NkSkeleton::kMaxBones), "
							 "le fichier en a %u\n",
							 path, boneCount, (uint32)data.skinJoints.Size());
			}
			skel.boneCount = boneCount;
			for (uint32 j = 0; j < boneCount; ++j) {
				ecs::NkBone &b = skel.bones[j];
				// b.name reste vide : renderer::NkGLTFNode ne parse pas glTF
				// nodes[].name (limitation du loader réel, documentée en tête de
				// NkGLTFIO.h) — pas d'invention de nom ici.
				const int32 nodeIdx = data.skinJoints[j];
				if (nodeIdx >= 0 && (uint32)nodeIdx < parentOf.Size()) {
					const int32 parentNode = parentOf[(uint32)nodeIdx];
					b.parent = (parentNode >= 0 && (uint32)parentNode < nodeToJoint.Size()) ? nodeToJoint[(uint32)parentNode]
																							: -1;
				}
				if (j < (uint32)data.inverseBind.Size()) {
					b.inverseBindPose = data.inverseBind[j];
					b.bindPose = b.inverseBindPose.Inverse();
				}
				if (nodeIdx >= 0 && (uint32)nodeIdx < (uint32)data.nodes.Size()) {
					const renderer::NkGLTFNode &gn = data.nodes[(uint32)nodeIdx];
					b.localPosition = gn.translation;
					b.localRotation.x = gn.rotation.x;
					b.localRotation.y = gn.rotation.y;
					b.localRotation.z = gn.rotation.z;
					b.localRotation.w = gn.rotation.w;
					b.localScale = gn.scale;
				}
			}
			scene.skeletons.PushBack(skel);
		}

		// ── Animations (remappage direct des channels — WEIGHTS ignorés) ────
		uint32 skippedWeightChannels = 0;
		for (uint32 a = 0; a < (uint32)data.animations.Size(); ++a) {
			const renderer::NkGLTFAnimation &anim = data.animations[a];
			NkGLTFAnimationData outAnim;
			outAnim.name = anim.name;
			outAnim.duration = anim.duration;
			for (uint32 c = 0; c < (uint32)anim.channels.Size(); ++c) {
				const renderer::NkGLTFAnimChannel &ch = anim.channels[c];
				if (ch.path == renderer::NkGLTFPath::WEIGHTS) {
					// NkGLTFAnimationData::Channel::values est un NkVec4f par clé —
					// ne peut pas représenter un nombre arbitraire de poids de morph
					// target par keyframe (renderer::NkGLTFAnimChannel::weightValues
					// est un tableau PLAT n_keys*n_targets). Pas transcrit ici : voir
					// note en tête de NkGLTFIO.h.
					++skippedWeightChannels;
					continue;
				}
				NkGLTFAnimationData::Channel outCh;
				outCh.targetNode = (uint32)ch.node;
				switch (ch.path) {
					case renderer::NkGLTFPath::TRANSLATION:
						outCh.path = NkGLTFAnimationData::Channel::Path::Translation;
						break;
					case renderer::NkGLTFPath::ROTATION:
						outCh.path = NkGLTFAnimationData::Channel::Path::Rotation;
						break;
					case renderer::NkGLTFPath::SCALE:
						outCh.path = NkGLTFAnimationData::Channel::Path::Scale;
						break;
					default:
						outCh.path = NkGLTFAnimationData::Channel::Path::Translation;
						break;
				}
				switch (ch.interp) {
					case renderer::NkGLTFInterp::STEP:
						outCh.interp = NkGLTFAnimationData::Channel::Interpolation::Step;
						break;
					case renderer::NkGLTFInterp::CUBICSPLINE:
						outCh.interp = NkGLTFAnimationData::Channel::Interpolation::CubicSpline;
						break;
					default:
						outCh.interp = NkGLTFAnimationData::Channel::Interpolation::Linear;
						break;
				}
				outCh.times = ch.times;
				outCh.values = ch.values;
				outAnim.channels.PushBack(outCh);
			}
			scene.animations.PushBack(outAnim);
		}
		if (skippedWeightChannels > 0) {
			logger.Warnf("[NkGLTFIO] Import('%s'): %u channel(s) WEIGHTS (morph target) ignoré(s) — non "
						 "représentable dans NkGLTFAnimationData::Channel (voir NkGLTFIO.h)\n",
						 path, skippedWeightChannels);
		}

		scene.name = data.debugName;
		scene.valid = true;
		return scene;
	}

	NkGLTFScene NkGLTFImporter::ImportFromMemory(const uint8 * /*data*/, nk_usize /*size*/,
												 const NkGLTFImportOptions & /*opts*/) noexcept {
		mLastError = "NkGLTFImporter::ImportFromMemory: NON IMPLEMENTE — renderer::LoadGLTF est fichier-only "
					 "(TODO)";
		logger.Warnf("[NkGLTFIO] ImportFromMemory: NON IMPLEMENTE — renderer::LoadGLTF est fichier-only "
					 "(aucune variante buffer cote loader reel)\n");
		return NkGLTFScene();
	}

	void NkGLTFScene::SpawnIntoWorld(ecs::NkWorld & /*world*/, NkVector<ecs::NkEntityId> * /*out*/) const noexcept {
		logger.Warnf("[NkGLTFIO] NkGLTFScene::SpawnIntoWorld: NON IMPLEMENTE — instanciation ECS reelle "
					 "(transform + mesh GPU + materiaux + squelette) hors scope de l'adaptateur IO (TODO)\n");
	}

	NkEditableMesh NkGLTFScene::MergeAllMeshes() const noexcept {
		if (meshes.Empty()) {
			return NkEditableMesh();
		}
		// Le loader réel a déjà fusionné toutes les primitives glTF en un seul
		// NkGLTFMeshData -> `meshes` ne contient qu'UNE entrée (voir note en
		// tête de NkGLTFIO.h) : un simple Clone() suffit, pas de fusion à faire.
		return meshes[0].mesh.Clone();
	}

	// ── NkGLTFExporter — aucun écrivain glTF/GLB côté NKRenderer ────────────
	void NkGLTFExporter::AddMesh(const NkEditableMesh &mesh, const NkGLTFMaterialData &mat, const char * /*name*/) noexcept {
		mMeshes.PushBack(mesh.Clone());
		mMaterials.PushBack(mat);
	}

	bool NkGLTFExporter::Export(const char *path) noexcept {
		mLastError = "NkGLTFExporter::Export: NON IMPLEMENTE — aucun ecrivain glTF/GLB cote NKRenderer (TODO)";
		logger.Warnf("[NkGLTFIO] Export('%s'): NON IMPLEMENTE — aucun ecrivain glTF/GLB cote NKRenderer (TODO)\n",
					 path ? path : "?");
		return false;
	}

	bool NkGLTFExporter::ExportToMemory(NkVector<uint8> & /*out*/) noexcept {
		mLastError = "NkGLTFExporter::ExportToMemory: NON IMPLEMENTE (TODO)";
		logger.Warnf("[NkGLTFIO] ExportToMemory: NON IMPLEMENTE — aucun ecrivain glTF/GLB cote NKRenderer (TODO)\n");
		return false;
	}

} // namespace nkentseu
