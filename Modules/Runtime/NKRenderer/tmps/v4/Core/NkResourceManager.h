#pragma once
// =============================================================================
// NkResourceManager.h
// Gestionnaire centralisé de toutes les ressources GPU.
//
// Philosophie :
//   - Chaque ressource est identifiée par un handle opaque typé.
//   - Le ResourceManager est l'UNIQUE interlocuteur avec NKRHI pour la
//     création/destruction des ressources.
//   - Les ressources sont référencées par compteur (RefCounted).
//   - Le hot-reload est supporté de manière transparente.
//   - Thread-safe : création/destruction protégées par mutex séparés.
//
// Accès :
//   auto tex  = resources.LoadTexture("assets/albedo.png");
//   auto mesh = resources.CreateMesh(NkMeshDesc::Sphere());
//   auto mat  = resources.CreateMaterialInstance(matTemplate);
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Resources/NkTextureDesc.h"
#include "NKRenderer/Resources/NkMeshDesc.h"
#include "NKRenderer/Resources/NkShaderDesc.h"
#include "NKRenderer/Resources/NkMaterialDesc.h"
#include "NKRenderer/Resources/NkFontDesc.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKThreading/NkMutex.h"
#include "NKCore/NkAtomic.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // NkResourceManager
        // =============================================================================
        class NkResourceManager {
           public:
                NkResourceManager()  = default;
                ~NkResourceManager() { Shutdown(); }

                NkResourceManager(const NkResourceManager&)            = delete;
                NkResourceManager& operator=(const NkResourceManager&) = delete;

                // ── Cycle de vie ──────────────────────────────────────────────────────────
                bool Init    (NkIDevice* device);
                void Shutdown();
                bool IsValid () const { return mDevice != nullptr; }

                // ── Textures ──────────────────────────────────────────────────────────────

                // Charger depuis un fichier (cache automatique — même chemin = même handle)
                NkTextureHandle LoadTexture     (const char* path,
                                                  bool srgb = true,
                                                  bool generateMips = true);

                // Créer depuis un descripteur
                NkTextureHandle CreateTexture   (const NkTextureDesc& desc);

                // Créer un render target
                NkTextureHandle CreateRenderTarget(uint32 w, uint32 h,
                                                   NkPixelFormat fmt = NkPixelFormat::NK_RGBA16F,
                                                   NkMSAA msaa = NkMSAA::NK_1X,
                                                   const char* debugName = nullptr);

                // Créer une depth texture
                NkTextureHandle CreateDepthTexture(uint32 w, uint32 h,
                                                   NkPixelFormat fmt = NkPixelFormat::NK_D32F,
                                                   NkMSAA msaa = NkMSAA::NK_1X,
                                                   const char* debugName = nullptr);

                // Mettre à jour les données d'une texture
                bool            WriteTexture    (NkTextureHandle tex,
                                                  const void* data, uint32 rowPitch = 0,
                                                  uint32 mip = 0, uint32 layer = 0);

                // Générer les mip-maps GPU
                bool            GenerateMipmaps (NkTextureHandle tex);

                // Libérer (décrémente le ref-count, détruit si 0)
                void            ReleaseTexture  (NkTextureHandle& h);

                // Accès info
                uint32          GetTextureWidth (NkTextureHandle h) const;
                uint32          GetTextureHeight(NkTextureHandle h) const;
                NkPixelFormat   GetTextureFormat(NkTextureHandle h) const;

                // ── Meshes ────────────────────────────────────────────────────────────────

                NkMeshHandle    CreateMesh      (const NkMeshDesc& desc);
                NkMeshHandle    LoadMesh        (const char* path); // .obj/.glb/.fbx

                // Mise à jour de géométrie dynamique
                bool            UpdateMeshVertices(NkMeshHandle mesh,
                                                   const void* data, uint32 vertexCount);
                bool            UpdateMeshIndices (NkMeshHandle mesh,
                                                   const void* data, uint32 indexCount,
                                                   NkIndexFormat fmt = NkIndexFormat::NK_UINT32);

                void            ReleaseMesh     (NkMeshHandle& h);
                uint32          GetMeshVertexCount(NkMeshHandle h) const;
                uint32          GetMeshIndexCount (NkMeshHandle h) const;
                const NkAABB&   GetMeshAABB     (NkMeshHandle h) const;

                // ── Shaders ───────────────────────────────────────────────────────────────

                NkShaderHandle  CreateShader    (const NkShaderDesc& desc);
                bool            ReloadShader    (NkShaderHandle h);
                void            ReleaseShader   (NkShaderHandle& h);

                // ── Matériaux ─────────────────────────────────────────────────────────────

                // Créer un template (pipeline compilé + layout)
                NkMaterialHandle     CreateMaterial    (const NkMaterialDesc& desc);
                void                 ReleaseMaterial   (NkMaterialHandle& h);

                // Créer une instance (descriptor set + paramètres spécifiques)
                NkMaterialInstHandle CreateMaterialInst(NkMaterialHandle tmpl);
                NkMaterialInstHandle CreateMaterialInst(const NkMaterialInstanceDesc& desc);
                void                 ReleaseMaterialInst(NkMaterialInstHandle& h);

                // Modifier les paramètres d'une instance
                bool SetMaterialTexture(NkMaterialInstHandle inst,
                                         const char* name, NkTextureHandle tex);
                bool SetMaterialFloat  (NkMaterialInstHandle inst,
                                         const char* name, float32 v);
                bool SetMaterialVec2   (NkMaterialInstHandle inst,
                                         const char* name, NkVec2f v);
                bool SetMaterialVec3   (NkMaterialInstHandle inst,
                                         const char* name, NkVec3f v);
                bool SetMaterialVec4   (NkMaterialInstHandle inst,
                                         const char* name, NkVec4f v);
                bool SetMaterialColor  (NkMaterialInstHandle inst,
                                         const char* name, const NkColorF& c) {
                    return SetMaterialVec4(inst, name, {c.r, c.g, c.b, c.a});
                }
                bool SetMaterialInt    (NkMaterialInstHandle inst,
                                         const char* name, int32 v);
                bool SetMaterialBool   (NkMaterialInstHandle inst,
                                         const char* name, bool v);

                // Forcer la mise à jour du descriptor set (utile après plusieurs Set*)
                void FlushMaterialInst (NkMaterialInstHandle inst);

                // ── Fonts ─────────────────────────────────────────────────────────────────

                NkFontHandle    LoadFont        (const char* path, float32 size = 16.f,
                                                  uint32 atlasSize = 1024);
                NkFontHandle    LoadFontDesc    (const NkFontDesc& desc);
                void            ReleaseFont     (NkFontHandle& h);

                // Accès à l'atlas texture d'une police
                NkTextureHandle GetFontAtlas    (NkFontHandle font) const;

                // ── Render Targets ────────────────────────────────────────────────────────

                NkRenderTargetHandle CreateRenderPass(const NkRenderTargetDesc& desc);
                void                 ReleaseRenderPass(NkRenderTargetHandle& h);

                NkTextureHandle GetRenderTargetColor(NkRenderTargetHandle rt,
                                                      uint32 slot = 0) const;
                NkTextureHandle GetRenderTargetDepth(NkRenderTargetHandle rt) const;

                // ── Textures par défaut (1×1, toujours disponibles) ────────────────────
                NkTextureHandle GetWhiteTexture()      const { return mDefaultWhite; }
                NkTextureHandle GetBlackTexture()      const { return mDefaultBlack; }
                NkTextureHandle GetFlatNormalTexture() const { return mDefaultNormal; }
                NkTextureHandle GetORM_DefaultTexture()const { return mDefaultORM; }

                // ── Meshes primitives (shared, lazy-créés) ────────────────────────────
                NkMeshHandle GetQuadMesh()       const { return mQuadMesh; }
                NkMeshHandle GetCubeMesh()       const { return mCubeMesh; }
                NkMeshHandle GetSphereMesh()     const { return mSphereMesh; }
                NkMeshHandle GetFullscreenQuad() const { return mFullscreenQuad; }

                // ── Statistiques ──────────────────────────────────────────────────────────
                struct Stats {
                    uint32 textureCount  = 0;
                    uint32 meshCount     = 0;
                    uint32 shaderCount   = 0;
                    uint32 materialCount = 0;
                    uint64 gpuMemBytes   = 0;
                };
                Stats GetStats() const;

                // ── Accès interne (pour les renderers internes) ───────────────────────
                // Ces méthodes retournent les IDs RHI internes.
                // NE PAS appeler depuis le code applicatif.
                uint64 GetTextureRHIId  (NkTextureHandle h) const;
                uint64 GetSamplerRHIId  (NkTextureHandle h) const;
                uint64 GetMeshVBORHIId  (NkMeshHandle h)    const;
                uint64 GetMeshIBORHIId  (NkMeshHandle h)    const;
                uint64 GetShaderRHIId   (NkShaderHandle h)  const;
                uint64 GetMaterialRHIId (NkMaterialHandle h)const;
                uint64 GetMatInstRHIId  (NkMaterialInstHandle h) const;

                NkIDevice* GetDevice() const { return mDevice; }

           private:
                // ── Entrées internes ──────────────────────────────────────────────────────
                struct TextureEntry {
                    uint64        rhiTextureId  = 0;
                    uint64        rhiSamplerId  = 0;
                    NkTextureDesc desc;
                    NkString      filePath;
                    uint32        refCount      = 1;
                    bool          valid         = false;
                };

                struct MeshEntry {
                    uint64        rhiVBO        = 0;
                    uint64        rhiIBO        = 0;
                    NkMeshDesc    desc;
                    NkAABB        aabb;
                    uint32        vertexCount   = 0;
                    uint32        indexCount    = 0;
                    uint32        refCount      = 1;
                    bool          valid         = false;
                };

                struct ShaderEntry {
                    uint64        rhiShaderId   = 0;
                    NkShaderDesc  desc;
                    uint32        refCount      = 1;
                    bool          valid         = false;
                };

                struct MaterialEntry {
                    uint64            rhiPipelineId    = 0;
                    uint64            rhiDescLayoutId  = 0;
                    NkMaterialDesc    desc;
                    uint32            refCount         = 1;
                    bool              valid            = false;
                };

                struct MaterialInstEntry {
                    uint64                    rhiDescSetId   = 0;
                    uint64                    rhiUBOId       = 0;
                    NkMaterialHandle          templateHandle;
                    NkVector<NkMaterialParam> params;
                    uint32                    refCount       = 1;
                    bool                      dirty          = true;
                    bool                      valid          = false;
                };

                struct FontEntry {
                    NkFontDesc     desc;
                    NkTextureHandle atlasHandle;
                    uint32         refCount    = 1;
                    bool           valid       = false;
                };

                struct RenderTargetEntry {
                    NkRenderTargetDesc         desc;
                    uint64                     rhiRenderPass  = 0;
                    uint64                     rhiFramebuffer = 0;
                    NkVector<NkTextureHandle>  colorTextures;
                    NkTextureHandle            depthTexture;
                    bool                       valid          = false;
                };

                // ── Implémentations internes ──────────────────────────────────────────────
                NkTextureHandle CreateTextureInternal (const NkTextureDesc& desc,
                                                        const char* filePath = nullptr);
                bool            BuildMaterialPipeline (MaterialEntry& entry);
                bool            RebuildMatInstDescSet (MaterialInstEntry& entry);
                void            CreateDefaultResources();
                void            CreatePrimitives();

                uint64 NextId() { return ++mNextId; }

                NkIDevice*       mDevice   = nullptr;
                NkAtomic<uint64> mNextId{0};

                mutable threading::NkMutex mTextureMutex;
                mutable threading::NkMutex mMeshMutex;
                mutable threading::NkMutex mShaderMutex;
                mutable threading::NkMutex mMaterialMutex;
                mutable threading::NkMutex mFontMutex;
                mutable threading::NkMutex mRTMutex;

                NkUnorderedMap<uint64, TextureEntry>      mTextures;
                NkUnorderedMap<uint64, MeshEntry>         mMeshes;
                NkUnorderedMap<uint64, ShaderEntry>       mShaders;
                NkUnorderedMap<uint64, MaterialEntry>     mMaterials;
                NkUnorderedMap<uint64, MaterialInstEntry> mMatInsts;
                NkUnorderedMap<uint64, FontEntry>         mFonts;
                NkUnorderedMap<uint64, RenderTargetEntry> mRenderTargets;

                // Cache par chemin de fichier
                NkUnorderedMap<NkString, NkTextureHandle> mTexturePathCache;
                NkUnorderedMap<NkString, NkMeshHandle>    mMeshPathCache;
                NkUnorderedMap<NkString, NkFontHandle>    mFontPathCache;

                // Ressources partagées par défaut
                NkTextureHandle mDefaultWhite;
                NkTextureHandle mDefaultBlack;
                NkTextureHandle mDefaultNormal;
                NkTextureHandle mDefaultORM;

                NkMeshHandle    mQuadMesh;
                NkMeshHandle    mCubeMesh;
                NkMeshHandle    mSphereMesh;
                NkMeshHandle    mFullscreenQuad;
        };

    } // namespace renderer
} // namespace nkentseu
