#pragma once
// =============================================================================
// NkModelLoader.h
// Chargeur de modèles 3D : OBJ, STL, GLB (GLTF2), FBX (partiel), PLY, 3DS
//
// Architecture :
//   NkModelLoader::Load(path) → NkModelData
//   NkModelImporter::Import(NkModelData, device) → NkStaticMesh* / NkScene*
//
// Formats supportés :
//   • OBJ + MTL  — format universel, matériaux Phong → PBR approximé
//   • STL        — binary & ASCII, mesh seul (pas de matériau)
//   • GLB / GLTF — format moderne, PBR natif, animations, scènes hiérarchiques
//   • PLY        — nuages de points / meshes simples
//   • 3DS        — legacy Autodesk (lecture seule)
//   • Collada/DAE— partiel (squelette + mesh)
// =============================================================================
#include "NKRenderer/Core/NkRenderTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Mesh/NkMesh.h"
#include "NKRenderer/Material/NkMaterial.h"
#include "NKRenderer/Core/NkTexture.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // Matériau importé (avant conversion en NkMaterial)
        // =============================================================================
        struct NkImportedMaterial {
            NkString  name;
            NkShadingModel shadingModel = NkShadingModel::NK_DEFAULT_LIT;
            NkBlendMode    blendMode    = NkBlendMode::NK_OPAQUE;

            // PBR Metallic/Roughness
            NkColor4f albedo            = {1,1,1,1};
            float     metallic          = 0.f;
            float     roughness         = 0.5f;
            float     ao                = 1.f;
            NkColor4f emissive          = {0,0,0,1};
            float     emissiveScale     = 1.f;
            float     alphaCutoff       = 0.5f;
            bool      doubleSided       = false;
            float     ior               = 1.45f;

            // Chemins de texture (relatifs au fichier source)
            NkString  albedoPath;
            NkString  normalPath;
            NkString  ormPath;           // AO+Roughness+Metallic packagé
            NkString  roughnessPath;
            NkString  metallicPath;
            NkString  aoPath;
            NkString  emissivePath;
            NkString  heightPath;
            NkString  opacityPath;

            // PBR Specular/Glossiness (si converti depuis Phong)
            NkColor4f specularColor     = {0.04f, 0.04f, 0.04f, 1.f};
            float     glossiness        = 0.5f;
            NkString  specularPath;
            NkString  glossinessPath;

            // Subsurface
            NkColor4f subsurface        = {1,0.3f,0.1f,1};
            float     ssRadius          = 0.f;

            // Transmission
            float     transmission      = 0.f;
            float     transmissionIOR   = 1.45f;
        };

        // =============================================================================
        // Nœud de scène importé (hiérarchie GLTF/FBX)
        // =============================================================================
        struct NkImportedNode {
            NkString     name;
            NkMat4f      localTransform = NkMat4f::Identity();
            int32        parent         = -1;
            NkVector<uint32> children;
            int32        meshIndex      = -1;
            int32        cameraIndex    = -1;
            int32        lightIndex     = -1;
            bool         visible        = true;
        };

        // =============================================================================
        // Animation importée (GLTF)
        // =============================================================================
        struct NkImportedKeyframe {
            float   time;
            NkVec3f value3;
            NkVec4f value4; // pour les rotations quaternion
        };

        enum class NkAnimChannel { NK_TRANSLATION, NK_ROTATION, NK_SCALE, NK_WEIGHTS };

        struct NkImportedChannel {
            uint32              targetNode  = 0;
            NkAnimChannel       property    = NkAnimChannel::NK_TRANSLATION;
            NkVector<NkImportedKeyframe> keyframes;
            enum class Interp { NK_LINEAR, NK_STEP, NK_CUBIC_SPLINE } interpolation = Interp::NK_LINEAR;
        };

        struct NkImportedAnimation {
            NkString    name;
            float       duration = 0.f;
            NkVector<NkImportedChannel> channels;
        };

        // =============================================================================
        // Données brutes importées d'un fichier 3D
        // =============================================================================
        struct NkModelData {
            NkString    sourcePath;
            NkString    sourceDir;   // répertoire pour résoudre les textures

            // Meshes
            NkVector<NkMeshData>             meshes;
            NkVector<NkString>               meshNames;

            // Matériaux
            NkVector<NkImportedMaterial>     materials;

            // Hiérarchie
            NkVector<NkImportedNode>         nodes;

            // Squelettes
            NkVector<NkBone>                 skeleton;

            // Animations
            NkVector<NkImportedAnimation>    animations;

            // Caméras importées
            NkVector<NkCamera>               cameras;

            // Lumières importées
            NkVector<NkLight>                lights;

            // Stats
            uint32 totalVertices  = 0;
            uint32 totalTriangles = 0;
            bool   hasSkeleton    = false;
            bool   hasAnimations  = false;

            bool IsValid() const { return !meshes.IsEmpty(); }
        };

        // =============================================================================
        // Options d'import
        // =============================================================================
        struct NkModelImportOptions {
            bool   flipWindingOrder  = false;
            bool   flipNormals       = false;
            bool   flipUV_Y          = false;      // certains formats ont Y=0 en haut
            bool   generateNormals   = true;       // si absentes dans le fichier
            bool   generateTangents  = true;       // pour le normal mapping
            bool   mergeSubMeshes    = false;      // fusionner tous les sous-meshes
            bool   optimizeMesh      = true;       // deduplication + tri vertex cache
            bool   importMaterials   = true;
            bool   importTextures    = true;
            bool   importAnimations  = true;
            bool   importLights      = true;
            bool   importCameras     = true;
            bool   importHierarchy   = true;
            bool   embedTextures     = false;      // copier les textures dans le mesh
            float  scale             = 1.f;
            NkVec3f axisRemapX       = {1,0,0};
            NkVec3f axisRemapY       = {0,1,0};
            NkVec3f axisRemapZ       = {0,0,1};
            float  maxTexSize        = 4096.f;     // réduire si texture trop grande
            uint32 maxLODs           = 4;
            bool   generateLODs      = false;      // créer des LODs automatiquement
            float  lodReductionFactor= 0.5f;       // % de triangles par LOD

            static NkModelImportOptions Default()    { return {}; }
            static NkModelImportOptions NoMaterial() {
                NkModelImportOptions o;
                o.importMaterials = false;
                o.importTextures  = false;
                return o;
            }
            static NkModelImportOptions ForGameEngine() {
                NkModelImportOptions o;
                o.optimizeMesh = true;
                o.generateLODs = true;
                o.maxTexSize   = 2048.f;
                return o;
            }
            static NkModelImportOptions ForFilmRendering() {
                NkModelImportOptions o;
                o.optimizeMesh = false;
                o.generateLODs = false;
                o.maxTexSize   = 8192.f;
                return o;
            }
        };

        // =============================================================================
        // NkModelLoader — parsers de format
        // =============================================================================
        class NkModelLoader {
        public:
            // Charger n'importe quel format (détection automatique par extension)
            static bool Load(const char* path, NkModelData& out,
                            const NkModelImportOptions& opts = NkModelImportOptions::Default());

            static bool LoadOBJ   (const char* path, NkModelData& out, const NkModelImportOptions&);
            static bool LoadSTL   (const char* path, NkModelData& out, const NkModelImportOptions&);
            static bool LoadGLB   (const char* path, NkModelData& out, const NkModelImportOptions&);
            static bool LoadGLTF  (const char* path, NkModelData& out, const NkModelImportOptions&);
            static bool LoadPLY   (const char* path, NkModelData& out, const NkModelImportOptions&);
            static bool Load3DS   (const char* path, NkModelData& out, const NkModelImportOptions&);
            static bool LoadCollada(const char* path, NkModelData& out, const NkModelImportOptions&);

            static bool IsSupported(const char* extension);

            // Export
            static bool SaveOBJ  (const char* path, const NkMeshData& mesh, const char* mtlPath = nullptr);
            static bool SaveSTL  (const char* path, const NkMeshData& mesh, bool binary = true);
            static bool SaveGLTF (const char* path, const NkModelData& model);

        private:
            // ── OBJ parser ────────────────────────────────────────────────────────────
            static bool ParseOBJ(const NkString& src, const NkString& dir,
                                NkModelData& out, const NkModelImportOptions& opts);
            static bool ParseMTL(const NkString& path, const NkString& dir,
                                NkVector<NkImportedMaterial>& mats);
            static NkShadingModel OBJIllumToShading(int illum);

            // ── STL parser ────────────────────────────────────────────────────────────
            static bool ParseSTLBinary(const uint8* data, usize size, NkMeshData& out);
            static bool ParseSTLASCII (const char* text, NkMeshData& out);

            // ── GLTF/GLB parser ───────────────────────────────────────────────────────
            struct GLTF; // pimpl
            static bool ParseGLTFJSON(const char* jsonStr, const uint8* binaryChunk,
                                    usize binarySize, const NkString& dir,
                                    NkModelData& out, const NkModelImportOptions& opts);
            static NkVec3f  GLTFToVec3 (const float* v);
            static NkVec4f  GLTFToVec4 (const float* v);
            static NkMat4f  GLTFToMat4 (const float* v);
            static NkColor4f GLTFToColor(const float* v);
            static NkShadingModel GLTFExtToShading(const char* ext);

            // ── PLY parser ────────────────────────────────────────────────────────────
            static bool ParsePLY(const uint8* data, usize size,
                                NkMeshData& out, const NkModelImportOptions& opts);

            // ── Helpers ───────────────────────────────────────────────────────────────
            static NkVector<uint8> ReadFile(const char* path);
            static NkString        ReadTextFile(const char* path);
            static NkString        ExtractDir(const NkString& path);
            static NkString        ExtractExt(const NkString& path);
            static NkString        ResolveTexPath(const NkString& dir, const NkString& rel);
        };

        // =============================================================================
        // NkModelImporter — convertit NkModelData en ressources GPU
        // =============================================================================
        class NkModelImporter {
        public:
            struct ImportResult {
                NkVector<NkStaticMesh*>  meshes;
                NkVector<NkMaterial*>    materials;
                NkVector<NkTexture2D*>   textures;
                NkVector<NkImportedNode> hierarchy;
                NkVector<NkImportedAnimation> animations;
                bool valid = false;
            };

            // Import complet (meshes + matériaux + textures)
            static ImportResult Import(NkIDevice* device, const NkModelData& data,
                                        const NkModelImportOptions& opts = {});

            // Import simple (un seul mesh, matériau par défaut)
            static NkStaticMesh* ImportMesh(NkIDevice* device, const char* path,
                                            NkMaterial* material = nullptr,
                                            const NkModelImportOptions& opts = {});

            // Convertir un NkImportedMaterial en NkMaterial complet
            static NkMaterial* ConvertMaterial(NkIDevice* device,
                                                const NkImportedMaterial& mat,
                                                const NkString& baseDir);

            // Résoudre et charger les textures d'un matériau importé
            static void LoadMaterialTextures(NkIDevice* device,
                                            NkMaterial* out,
                                            const NkImportedMaterial& src,
                                            const NkString& baseDir);
        };

        // =============================================================================
        // Implémentation inline des parsers simples
        // =============================================================================
        inline bool NkModelLoader::IsSupported(const char* ext) {
            NkString e = NkString(ext).ToLower();
            return e==".obj" || e==".stl" || e==".glb" || e==".gltf" ||
                e==".ply" || e==".3ds" || e==".dae";
        }

    } // namespace renderer
} // namespace nkentseu
