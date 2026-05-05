#pragma once
// =============================================================================
// Nkentseu/IO/NkGLTFIO.h
// =============================================================================
// Import/Export glTF 2.0 (GL Transmission Format) — standard interop moderne.
//
// PRIORITÉ : glTF 2.0 est LE format prioritaire car :
//   • Standard Khronos ouvert et gratuit
//   • Supporte : mesh, matériaux PBR, squelette, animation, blend shapes
//   • Utilisé par Blender (export natif), Unreal, Unity, Three.js, Godot
//   • Format GLB = bundle binaire auto-contenu (0 fichier externe)
//
// CAPACITÉS D'IMPORT :
//   ✅ Mesh (positions, normales, UVs, vertex colors, tangentes)
//   ✅ Multi-matériaux (PBR Metallic-Roughness, Unlit, extensions)
//   ✅ Squelette (NkSkeleton) avec bind pose
//   ✅ Skinning (vertex weights, jusqu'à 4 influences par vertex)
//   ✅ Animation (TRS par os, blend shapes via morph targets)
//   ✅ Hiérarchie de nœuds (NkTransform + NkParent + NkChildren)
//   ✅ Caméras (perspective et orthographique)
//   ✅ Lumières (extension KHR_lights_punctual)
//   ✅ Blend Shapes / Morph Targets (NkBlendShape)
//   ✅ Multi-scènes dans un seul fichier
//   ⚠️ Extensions : KHR_draco_mesh_compression (Phase 2)
//   ❌ Extensions Ray Tracing (non prévu)
//
// CAPACITÉS D'EXPORT :
//   ✅ Mesh avec tous les attributs
//   ✅ Matériaux PBR
//   ✅ Squelette + animations
//   ✅ Format GLB (binaire) et glTF+BIN+textures séparés
//
// USAGE IMPORT :
//   NkGLTFImporter importer;
//   NkGLTFScene scene = importer.Import("character.glb");
//   if (scene.IsValid()) {
//       // Spawn dans le monde ECS
//       scene.SpawnIntoWorld(world, factory);
//   }
//
// USAGE EXPORT :
//   NkGLTFExporter exporter;
//   exporter.SetFormat(NkGLTFExporter::Format::GLB);
//   exporter.AddMesh(editableMesh, materialDesc);
//   exporter.Export("output.glb");
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKECS/World/NkWorld.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKRenderer/src/Resources/NkResourceDescs.h"
#include "Nkentseu/Modeling/NkEditableMesh.h"
#include "Nkentseu/ECS/Components/Animation/NkAnimation.h"
#include "Nkentseu/ECS/Factory/NkGameObjectFactory.h"

namespace nkentseu {

    using namespace math;

    // =========================================================================
    // NkGLTFMaterialData — données matériau importé
    // =========================================================================
    struct NkGLTFMaterialData {
        NkString name;

        // PBR Metallic-Roughness
        NkVec4f  baseColorFactor    = {1, 1, 1, 1};
        NkString baseColorTexture;
        float32  metallicFactor     = 0.f;
        float32  roughnessFactor    = 1.f;
        NkString metallicRoughnessTexture;

        // Normales
        NkString normalTexture;
        float32  normalScale        = 1.f;

        // Occlusion
        NkString occlusionTexture;
        float32  occlusionStrength  = 1.f;

        // Émission
        NkVec3f  emissiveFactor     = {};
        NkString emissiveTexture;

        // Mode alpha
        enum class AlphaMode : uint8 { Opaque, Mask, Blend };
        AlphaMode alphaMode         = AlphaMode::Opaque;
        float32   alphaCutoff       = 0.5f;
        bool      doubleSided       = false;
    };

    // =========================================================================
    // NkGLTFMeshData — données mesh importé
    // =========================================================================
    struct NkGLTFMeshData {
        NkString name;
        NkEditableMesh mesh;            ///< Données CPU
        uint32 materialIndex = 0;       ///< Index dans les matériaux de la scène

        // Blend shapes (morph targets)
        struct MorphTarget {
            NkString name;
            NkVector<NkVec3f> positionDeltas;
            NkVector<NkVec3f> normalDeltas;
        };
        NkVector<MorphTarget> morphTargets;

        // Skinning
        struct SkinData {
            NkVector<uint32>   boneIndices;  ///< 4 indices par vertex
            NkVector<NkVec4f>  boneWeights;  ///< 4 poids par vertex
        };
        SkinData skin;
        bool hasSkinning = false;
    };

    // =========================================================================
    // NkGLTFNodeData — nœud de scène importé
    // =========================================================================
    struct NkGLTFNodeData {
        NkString name;
        NkMat4f  localTransform = NkMat4f::Identity();
        NkVec3f  translation    = {};
        NkQuatf  rotation       = NkQuatf::Identity();
        NkVec3f  scale          = {1, 1, 1};

        int32    meshIndex      = -1;   ///< -1 = pas de mesh
        int32    cameraIndex    = -1;
        int32    skinIndex      = -1;
        int32    parentIndex    = -1;

        NkVector<uint32> children;
    };

    // =========================================================================
    // NkGLTFAnimationData — animation importée
    // =========================================================================
    struct NkGLTFAnimationData {
        NkString name;
        float32  duration = 0.f;

        struct Channel {
            uint32 targetNode = 0;
            enum class Path : uint8 { Translation, Rotation, Scale, MorphWeights } path;
            NkVector<float32> times;
            NkVector<NkVec4f> values;  ///< Vec3 pour TRS, Vec4 pour quaternions
            enum class Interpolation : uint8 { Linear, Step, CubicSpline } interp;
        };
        NkVector<Channel> channels;
    };

    // =========================================================================
    // NkGLTFScene — scène complète importée
    // =========================================================================
    struct NkGLTFScene {
        NkString name;
        bool     valid = false;

        NkVector<NkGLTFMeshData>      meshes;
        NkVector<NkGLTFMaterialData>  materials;
        NkVector<NkGLTFNodeData>      nodes;
        NkVector<NkGLTFAnimationData> animations;
        NkVector<ecs::NkSkeleton>     skeletons;

        // Nœuds racines de la scène
        NkVector<uint32> rootNodes;

        [[nodiscard]] bool IsValid() const noexcept { return valid; }

        /**
         * @brief Instancie toute la scène dans un NkWorld ECS.
         * @param world   Monde ECS cible.
         * @param factory Factory pour créer les GameObjects.
         * @param out     Optionnel : liste des entités créées.
         */
        void SpawnIntoWorld(ecs::NkWorld& world,
                             NkVector<ecs::NkEntityId>* out = nullptr) const noexcept;

        /**
         * @brief Retourne tous les NkEditableMesh fusionnés (pour import mesh seul).
         */
        [[nodiscard]] NkEditableMesh MergeAllMeshes() const noexcept;
    };

    // =========================================================================
    // NkGLTFImporter
    // =========================================================================
    struct NkGLTFImportOptions {
        bool importMeshes       = true;
        bool importMaterials    = true;
        bool importSkeletons    = true;
        bool importAnimations   = true;
        bool importCameras      = true;
        bool importLights       = true;
        bool importBlendShapes  = true;

        float32 scaleFactor     = 1.f;      ///< Facteur d'échelle global
        bool    flipY           = false;    ///< Retourner Y (pour conventions différentes)
        bool    flipWinding     = false;    ///< Inverser le sens des faces

        // Résolution des textures
        NkString textureBasePath;           ///< Dossier de base pour les textures externes
    };

    class NkGLTFImporter {
        public:
            NkGLTFImporter()  noexcept = default;
            ~NkGLTFImporter() noexcept = default;

            /**
             * @brief Importe un fichier glTF 2.0 ou GLB.
             * @param path Chemin vers le fichier (.gltf ou .glb).
             * @param opts Options d'import.
             * @return NkGLTFScene contenant toutes les données importées.
             */
            [[nodiscard]] NkGLTFScene Import(
                const char* path,
                const NkGLTFImportOptions& opts = {}) noexcept;

            /**
             * @brief Importe depuis un buffer mémoire (GLB uniquement).
             */
            [[nodiscard]] NkGLTFScene ImportFromMemory(
                const uint8* data,
                nk_usize size,
                const NkGLTFImportOptions& opts = {}) noexcept;

            [[nodiscard]] const NkString& GetLastError() const noexcept { return mLastError; }

        private:
            NkString mLastError;

            bool ParseGLB (const uint8* data, nk_usize size) noexcept;
            bool ParseGLTF(const char* json) noexcept;

            void LoadMeshes    (NkGLTFScene& scene) noexcept;
            void LoadMaterials (NkGLTFScene& scene) noexcept;
            void LoadNodes     (NkGLTFScene& scene) noexcept;
            void LoadAnimations(NkGLTFScene& scene) noexcept;
            void LoadSkeletons (NkGLTFScene& scene) noexcept;

            // Buffer JSON et BIN internes
            NkVector<uint8>  mJsonData;
            NkVector<uint8>  mBinData;
            NkGLTFImportOptions mOpts;
    };

    // =========================================================================
    // NkGLTFExporter
    // =========================================================================
    struct NkGLTFExportOptions {
        enum class Format : uint8 { GLTF, GLB };
        Format  format          = Format::GLB;
        bool    embedTextures   = true;     ///< Embed textures dans le GLB/GLTF
        bool    exportNormals   = true;
        bool    exportUVs       = true;
        bool    exportColors    = false;
        bool    exportTangents  = true;
        bool    exportSkinning  = true;
        bool    exportAnimation = true;
        float32 scaleFactor     = 1.f;
    };

    class NkGLTFExporter {
        public:
            NkGLTFExporter()  noexcept = default;
            ~NkGLTFExporter() noexcept = default;

            void SetOptions(const NkGLTFExportOptions& opts) noexcept { mOpts = opts; }

            void AddMesh(const NkEditableMesh& mesh,
                         const NkGLTFMaterialData& mat = {},
                         const char* name = nullptr) noexcept;

            void AddSkeleton(const ecs::NkSkeleton& skeleton) noexcept;

            void AddAnimation(const ecs::NkAnimationClip& clip,
                              const ecs::NkSkeleton& skeleton) noexcept;

            /**
             * @brief Exporte la scène vers un fichier.
             * @return true si succès.
             */
            [[nodiscard]] bool Export(const char* path) noexcept;

            /**
             * @brief Exporte vers un buffer mémoire (GLB).
             */
            [[nodiscard]] bool ExportToMemory(NkVector<uint8>& out) noexcept;

            [[nodiscard]] const NkString& GetLastError() const noexcept { return mLastError; }

        private:
            NkGLTFExportOptions     mOpts;
            NkVector<NkEditableMesh> mMeshes;
            NkVector<NkGLTFMaterialData> mMaterials;
            NkString mLastError;
    };

} // namespace nkentseu
