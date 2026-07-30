#pragma once
// =============================================================================
// Nkentseu/IO/NkGLTFIO.h
// =============================================================================
// [RÉVISION 2026-07-23] : NkGLTFIO est désormais une fine COUCHE D'ADAPTATION
// au-dessus du loader glTF 2.0 from-scratch déjà en production côté
// NKRenderer (Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/NkGLTFLoader.cpp,
// `renderer::LoadGLTF`, zéro-STL, supporte .gltf/.glb, PBR metallic-roughness,
// skinning JOINTS_0/WEIGHTS_0, morph targets, hiérarchie de nodes, animations
// TRS), et non plus une spec morte (ce fichier n'a jamais été compilé — API
// ambitieuse écrite à l'avance, aucune des méthodes déclarées n'avait de
// corps). Import() délègue TOUJOURS le parsing à `renderer::LoadGLTF` — zéro
// nouvelle logique JSON/GLB/accessor ici, uniquement un remappage de
// renderer::NkGLTFMeshData (déjà entièrement parsé) vers les types
// NkGLTFScene/NkGLTFMeshData/NkGLTFMaterialData/NkGLTFNodeData/
// NkGLTFAnimationData de Noge.
//
// CE QUI EST RÉELLEMENT IMPORTÉ (voir NkGLTFIO.cpp, remappage direct de champs
// déjà parsés par renderer::LoadGLTF — pas de nouvelle logique) :
//   ✅ Géométrie fusionnée (tous les primitives glTF en UN NkEditableMesh —
//      c'est déjà le format de sortie du loader réel : NkGLTFMeshData::vertices
//      /indices/subMeshes contiennent TOUTES les primitives du fichier, la
//      scène glTF n'est PAS re-décomposée mesh-par-mesh ni node-par-node).
//   ✅ Matériaux PBR metallic-roughness (facteurs + noms de fichier texture
//      pour les images à URI externe ; les textures embarquées en base64/GLB
//      n'ont pas de chemin fichier — champ texture laissé vide, la donnée
//      pixel décodée existe côté renderer::NkGLTFMeshData::images mais
//      NkGLTFMaterialData n'a pas de slot pour l'embarquer).
//   ✅ Hiérarchie de nodes (TRS + parent/enfants, recalculés depuis les
//      listes children déjà parsées — le loader réel ne stocke QUE les
//      enfants, pas de nom de node : NkGLTFNodeData::name reste vide).
//   ✅ Squelette (si JOINTS_0/WEIGHTS_0 + skin présents) : un seul
//      ecs::NkSkeleton reconstruit depuis skinJoints/inverseBind/nodes.
//      NkBone::name reste vide (même limitation que les nodes). Non couvert
//      par la démo d'exécution de cet incrément (aucun asset skinné simple
//      disponible n'a été exercé au-delà du comptage de joints).
//   ✅ Animations TRS (channels node/path/interp/times/values, remappage
//      direct — les canaux WEIGHTS (morph) ne sont PAS transcrits : le
//      format de canal Noge (NkVec4f par clé) ne peut pas représenter un
//      nombre arbitraire de poids de morph target par keyframe).
//
// NON IMPLÉMENTÉ (honnête, pas de fausse réussite) :
//   • ImportFromMemory : `renderer::LoadGLTF` est fichier-only (pas de
//     variante buffer côté loader réel) — retourne une scène invalide + log.
//   • NkGLTFScene::SpawnIntoWorld : instancier réellement dans un NkWorld ECS
//     (transform + mesh GPU + matériaux + squelette) est un travail
//     d'intégration à part entière (NkGameObjectFactory, upload GPU via
//     NkMeshSystem::Create, pont NkGLTFMaterialBridge déjà existant pour les
//     démos Sandbox) — hors scope d'un adaptateur IO fin. TODO honnête.
//   • NkGLTFExporter : aucun écrivain glTF/GLB n'existe côté NKRenderer.
//     Export()/ExportToMemory() retournent false + log.
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKECS/World/NkWorld.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "Noge/Modeling/NkEditableMesh.h"
#include "Noge/ECS/Components/Animation/NkAnimation.h"

namespace nkentseu {

	using namespace math;

	// =========================================================================
	// NkGLTFMaterialData — données matériau importé (remappage direct de
	// renderer::NkGLTFMaterial, cf. NkGLTFLoader.h)
	// =========================================================================
	struct NkGLTFMaterialData {
			NkString name;

			NkVec4f baseColorFactor = {1, 1, 1, 1};
			NkString baseColorTexture; ///< URI externe uniquement (vide si embarqué/absent)
			float32 metallicFactor = 0.f;
			float32 roughnessFactor = 1.f;
			NkString metallicRoughnessTexture;

			NkString normalTexture;
			float32 normalScale = 1.f;

			NkString occlusionTexture;
			float32 occlusionStrength = 1.f;

			NkVec3f emissiveFactor = {};
			NkString emissiveTexture;

			enum class AlphaMode : uint8 { Opaque, Mask, Blend };
			AlphaMode alphaMode = AlphaMode::Opaque;
			float32 alphaCutoff = 0.5f;
			bool doubleSided = false;
	};

	// =========================================================================
	// NkGLTFMeshData — mesh importé (le loader réel fusionne déjà TOUTES les
	// primitives glTF du fichier en un seul jeu de buffers — cette struct
	// représente donc l'intégralité de la géométrie du fichier, pas un mesh
	// glTF individuel)
	// =========================================================================
	struct NkGLTFMeshData {
			NkString name;
			NkEditableMesh mesh; ///< Géométrie fusionnée (toutes primitives)
			uint32 materialIndex = 0;
	};

	// =========================================================================
	// NkGLTFNodeData — nœud de scène importé (TRS + hiérarchie ; pas de nom,
	// le loader réel ne parse pas glTF `nodes[].name`)
	// =========================================================================
	struct NkGLTFNodeData {
			NkVec3f translation = {};
			NkQuatf rotation{}; ///< identité par défaut (x=y=z=0, w=1)
			NkVec3f scale = {1, 1, 1};
			bool hasMatrix = false;
			NkMat4f matrix = NkMat4f::Identity();

			int32 meshIndex = -1; ///< -1 = pas de mesh (glTF nodes[].mesh)
			int32 parentIndex = -1; ///< recalculé depuis les listes children du loader

			NkVector<uint32> children;
	};

	// =========================================================================
	// NkGLTFAnimationData — animation importée (remappage direct des channels
	// déjà parsés ; les channels WEIGHTS/morph ne sont pas transcrits, voir
	// note en tête de fichier)
	// =========================================================================
	struct NkGLTFAnimationData {
			NkString name;
			float32 duration = 0.f;

			struct Channel {
					uint32 targetNode = 0;
					enum class Path : uint8 { Translation, Rotation, Scale, MorphWeights } path;
					NkVector<float32> times;
					NkVector<NkVec4f> values; ///< Vec3 pour TRS, Vec4 pour quaternions
					enum class Interpolation : uint8 { Linear, Step, CubicSpline } interp;
			};

			NkVector<Channel> channels;
	};

	// =========================================================================
	// NkGLTFScene — scène complète importée
	// =========================================================================
	struct NkGLTFScene {
			NkString name;
			bool valid = false;

			NkVector<NkGLTFMeshData> meshes; ///< 1 entrée = géométrie fusionnée du fichier
			NkVector<NkGLTFMaterialData> materials;
			NkVector<NkGLTFNodeData> nodes;
			NkVector<NkGLTFAnimationData> animations;
			NkVector<ecs::NkSkeleton> skeletons; ///< 0 ou 1 (un seul skin supporté par le loader réel)

			NkVector<uint32> rootNodes; ///< nodes sans parent

			[[nodiscard]] bool IsValid() const noexcept {
				return valid;
			}

			/**
			 * @brief NON IMPLÉMENTÉ — instanciation ECS réelle (transform + mesh GPU
			 *        + matériaux + squelette) : travail d'intégration à part
			 *        entière, hors scope d'un adaptateur IO fin. Log + no-op.
			 */
			void SpawnIntoWorld(ecs::NkWorld &world, NkVector<ecs::NkEntityId> *out = nullptr) const noexcept;

			/**
			 * @brief Retourne le mesh fusionné (clone du seul NkGLTFMeshData —
			 *        le loader réel a déjà fusionné toutes les primitives).
			 */
			[[nodiscard]] NkEditableMesh MergeAllMeshes() const noexcept;
	};

	// =========================================================================
	// NkGLTFImporter
	// =========================================================================
	struct NkGLTFImportOptions {
			float32 scaleFactor = 1.f; ///< Appliqué en post-traitement sur les positions déjà parsées
			bool flipY = false;		   ///< Idem, inverse Y (positions + normales)
	};

	class NkGLTFImporter {
		public:
			NkGLTFImporter() noexcept = default;
			~NkGLTFImporter() noexcept = default;

			/**
			 * @brief Importe un fichier glTF 2.0 ou GLB via renderer::LoadGLTF.
			 * @return NkGLTFScene::valid == false si le fichier est introuvable ou
			 *         invalide — voir GetLastError().
			 */
			[[nodiscard]] NkGLTFScene Import(const char *path, const NkGLTFImportOptions &opts = {}) noexcept;

			/**
			 * @brief NON IMPLÉMENTÉ — renderer::LoadGLTF est fichier-only (pas de
			 *        variante mémoire côté loader réel). Retourne une scène
			 *        invalide + log (pas de fausse réussite).
			 */
			[[nodiscard]] NkGLTFScene ImportFromMemory(const uint8 *data, nk_usize size,
													   const NkGLTFImportOptions &opts = {}) noexcept;

			[[nodiscard]] const NkString &GetLastError() const noexcept {
				return mLastError;
			}

		private:
			NkString mLastError;
	};

	// =========================================================================
	// NkGLTFExporter — NON IMPLÉMENTÉ (aucun écrivain glTF/GLB côté
	// NKRenderer). Conservé pour compatibilité d'API amont ; toute méthode
	// d'écriture retourne false/no-op + log, pas de fausse réussite.
	// =========================================================================
	struct NkGLTFExportOptions {
			enum class Format : uint8 { GLTF, GLB };
			Format format = Format::GLB;
	};

	class NkGLTFExporter {
		public:
			NkGLTFExporter() noexcept = default;
			~NkGLTFExporter() noexcept = default;

			void SetOptions(const NkGLTFExportOptions &opts) noexcept {
				mOpts = opts;
			}

			void AddMesh(const NkEditableMesh &mesh, const NkGLTFMaterialData &mat = {},
						 const char *name = nullptr) noexcept;

			[[nodiscard]] bool Export(const char *path) noexcept;
			[[nodiscard]] bool ExportToMemory(NkVector<uint8> &out) noexcept;

			[[nodiscard]] const NkString &GetLastError() const noexcept {
				return mLastError;
			}

		private:
			NkGLTFExportOptions mOpts;
			NkVector<NkEditableMesh> mMeshes;
			NkVector<NkGLTFMaterialData> mMaterials;
			NkString mLastError;
	};

} // namespace nkentseu
