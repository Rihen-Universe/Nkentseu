#pragma once
// =============================================================================
// NkBaseRenderer.h
// Classe de base commune pour tous les backends de NKRenderer.
//
// Elle fournit :
//   • Registres thread-safe pour les ressources (shaders, textures, meshes…)
//   • Sélection automatique de la variante de shader selon l'API courante
//   • Implémentation des shortcuts (CreateQuad2D, CreateSphere3D, LoadTexture…)
//   • Gestion des stats de frame
//   • Accumulateur de draw calls (NkRendererCommand)
//
// Les sous-classes (NkOpenGLRenderer, NkVulkanRenderer…) implémentent :
//   • CreateShader_Impl / CreateTexture_Impl / CreateMesh_Impl / etc.
//   • SubmitDrawCalls_Impl (traduit NkRendererCommand → NkICommandBuffer)
//   • BeginFrame_Impl / EndFrame_Impl (gestion swapchain)
// =============================================================================
#include "NkIRenderer.h"
#include "NkRendererFactory.h"
#include "NKRenderer/Command/NkRendererCommand.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKThreading/NkMutex.h"

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // Entrées des registres internes
    // =========================================================================
    struct NkShaderEntry {
        NkShaderHandle rhiHandle;    // handle RHI interne (NkShaderHandle du RHI)
        NkShaderDesc   desc;
        bool           valid = false;
    };

    struct NkTextureEntry {
        // handles RHI internes stockés comme uint64 pour éviter la dépendance
        uint64 rhiTextureId = 0;
        uint64 rhiSamplerId = 0;
        NkTextureDesc desc;
        bool   valid = false;
    };

    struct NkMeshEntry {
        uint64 rhiVBO  = 0;
        uint64 rhiIBO  = 0;
        uint32 vertexCount = 0;
        uint32 indexCount  = 0;
        NkMeshDesc desc;
        bool   valid = false;
    };

    struct NkMaterialEntry {
        NkMaterialTemplateDesc desc;
        NkShaderHandle         shader;   // handle renderer (pas RHI)
        uint64                 rhiPipeline = 0;
        bool                   valid = false;
    };

    struct NkMaterialInstEntry {
        NkMaterialInstHandle  templateHandle;
        NkVector<NkMaterialParam> params;
        uint64 rhiDescSet = 0;  // descriptor set RHI
        bool   valid = false;
        bool   dirty = true;    // params changés → rebuild descriptor set
    };

    struct NkCameraEntry {
        bool       is2D = false;
        NkCameraDesc2D desc2D;
        NkCameraDesc3D desc3D;
        NkMat4f    view;
        NkMat4f    proj;
        NkMat4f    viewProj;
        bool       valid = false;

        void Recompute();
    };

    struct NkRenderTargetEntry {
        NkRenderTargetDesc desc;
        uint64 rhiRenderPass  = 0;
        uint64 rhiFramebuffer = 0;
        NkVector<NkTextureHandle> colorTextures;
        NkTextureHandle           depthTexture;
        bool valid = false;
    };

    struct NkFontEntry {
        NkFontDesc desc;
        uint64 rhiAtlasTexture = 0;
        uint64 rhiSampler      = 0;
        NkTextureHandle atlasHandle;
        // Glyph map : codepoint → NkGlyphInfo
        NkHashMap<uint32, NkGlyphInfo> glyphs;
        bool valid = false;
    };

    // =========================================================================
    // NkBaseRenderer
    // =========================================================================
    class NkBaseRenderer : public NkIRenderer {
    public:
        explicit NkBaseRenderer(NkIDevice* device);
        ~NkBaseRenderer() override;

        // ── NkIRenderer — lifecycle ──────────────────────────────────────────
        bool Initialize() override;
        void Shutdown()   override;
        bool IsValid()    const override { return mValid; }

        // ── NkIRenderer — resources ───────────────────────────────────────────
        NkShaderHandle       CreateShader              (const NkShaderDesc& desc)           override;
        void                 DestroyShader             (NkShaderHandle& h)                  override;
        bool                 ReloadShader              (NkShaderHandle h)                   override;

        NkTextureHandle      CreateTexture             (const NkTextureDesc& desc)          override;
        void                 DestroyTexture            (NkTextureHandle& h)                 override;
        bool                 UpdateTexture             (NkTextureHandle tex, const void* d,
                                                        uint32 rowPitch, uint32 mip, uint32 layer) override;
        bool                 GenerateMipmaps           (NkTextureHandle tex)                override;

        NkMeshHandle         CreateMesh                (const NkMeshDesc& desc)             override;
        void                 DestroyMesh               (NkMeshHandle& h)                    override;
        bool                 UpdateMesh                (NkMeshHandle mesh,
                                                        const void* verts, uint32 vCount,
                                                        const void* idxs,  uint32 iCount)  override;

        NkMaterialHandle     CreateMaterialTemplate    (const NkMaterialTemplateDesc& desc) override;
        void                 DestroyMaterialTemplate   (NkMaterialHandle& h)               override;

        NkMaterialInstHandle CreateMaterialInstance    (const NkMaterialInstanceDesc& desc) override;
        void                 DestroyMaterialInstance   (NkMaterialInstHandle& h)           override;

        bool SetMaterialFloat  (NkMaterialInstHandle mat, const char* name, float32 v)      override;
        bool SetMaterialVec2   (NkMaterialInstHandle mat, const char* name, NkVec2f v)      override;
        bool SetMaterialVec3   (NkMaterialInstHandle mat, const char* name, NkVec3f v)      override;
        bool SetMaterialVec4   (NkMaterialInstHandle mat, const char* name, NkVec4f v)      override;
        bool SetMaterialInt    (NkMaterialInstHandle mat, const char* name, int32 v)        override;
        bool SetMaterialBool   (NkMaterialInstHandle mat, const char* name, bool v)        override;
        bool SetMaterialTexture(NkMaterialInstHandle mat, const char* name,
                                NkTextureHandle tex)                                        override;

        NkCameraHandle       CreateCamera2D            (const NkCameraDesc2D& desc)         override;
        NkCameraHandle       CreateCamera3D            (const NkCameraDesc3D& desc)         override;
        void                 DestroyCamera             (NkCameraHandle& h)                  override;
        bool                 UpdateCamera2D            (NkCameraHandle cam,
                                                        const NkCameraDesc2D& desc)         override;
        bool                 UpdateCamera3D            (NkCameraHandle cam,
                                                        const NkCameraDesc3D& desc)         override;
        NkMat4f              GetViewMatrix             (NkCameraHandle cam) const           override;
        NkMat4f              GetProjectionMatrix       (NkCameraHandle cam) const           override;
        NkMat4f              GetViewProjectionMatrix   (NkCameraHandle cam) const           override;

        NkRenderTargetHandle CreateRenderTarget        (const NkRenderTargetDesc& desc)     override;
        void                 DestroyRenderTarget       (NkRenderTargetHandle& h)            override;
        NkTextureHandle      GetRenderTargetColor      (NkRenderTargetHandle rt,
                                                        uint32 slot) const                  override;
        NkTextureHandle      GetRenderTargetDepth      (NkRenderTargetHandle rt) const      override;

        NkFontHandle         CreateFont                (const NkFontDesc& desc)             override;
        void                 DestroyFont               (NkFontHandle& h)                    override;
        bool                 GetGlyph                  (NkFontHandle font, uint32 cp,
                                                        NkGlyphInfo& out) const             override;
        NkTextureHandle      GetFontAtlasTexture       (NkFontHandle font) const            override;

        NkUniformBlockHandle CreateUniformBlock        (const NkUniformBlockDesc& desc)     override;
        void                 DestroyUniformBlock       (NkUniformBlockHandle& h)            override;
        bool                 WriteUniformBlock         (NkUniformBlockHandle ub,
                                                        const void* data, uint32 size,
                                                        uint32 offset)                      override;

        // ── NkIRenderer — frame ───────────────────────────────────────────────
        bool               BeginFrame()                              override;
        bool               BeginRenderPass(const NkRenderPassInfo&) override;
        NkRendererCommand& GetCommand()                              override { return mCommand; }
        void               EndRenderPass()                           override;
        void               EndFrame()                               override;

        void   OnResize(uint32 w, uint32 h) override;
        uint32 GetSwapchainWidth()  const   override;
        uint32 GetSwapchainHeight() const   override;

        const NkRendererStats& GetLastFrameStats() const override { return mStats; }

    protected:
        // ── À implémenter par les sous-classes ────────────────────────────────
        virtual bool CreateShader_Impl   (NkShaderEntry&)          = 0;
        virtual void DestroyShader_Impl  (NkShaderEntry&)          = 0;

        virtual bool CreateTexture_Impl  (NkTextureEntry&)         = 0;
        virtual void DestroyTexture_Impl (NkTextureEntry&)         = 0;
        virtual bool UpdateTexture_Impl  (NkTextureEntry&, const void*, uint32, uint32, uint32) = 0;

        virtual bool CreateMesh_Impl     (NkMeshEntry&)            = 0;
        virtual void DestroyMesh_Impl    (NkMeshEntry&)            = 0;
        virtual bool UpdateMesh_Impl     (NkMeshEntry&, const void*, uint32, const void*, uint32) = 0;

        virtual bool CreateMaterial_Impl (NkMaterialEntry&)        = 0;
        virtual void DestroyMaterial_Impl(NkMaterialEntry&)        = 0;

        virtual bool CreateMatInst_Impl  (NkMaterialInstEntry&, const NkMaterialEntry&) = 0;
        virtual void DestroyMatInst_Impl (NkMaterialInstEntry&)    = 0;
        virtual bool RebuildMatInst_Impl (NkMaterialInstEntry&, const NkMaterialEntry&) = 0;

        virtual bool CreateRenderTarget_Impl(NkRenderTargetEntry&) = 0;
        virtual void DestroyRenderTarget_Impl(NkRenderTargetEntry&)= 0;

        virtual bool CreateFont_Impl     (NkFontEntry&)            = 0;
        virtual void DestroyFont_Impl    (NkFontEntry&)            = 0;

        // Frame
        virtual bool BeginFrame_Impl()                                           = 0;
        virtual bool BeginRenderPass_Impl(const NkRenderPassInfo&,
                                          NkICommandBuffer* cmd)                 = 0;
        virtual void SubmitDrawCalls_Impl(NkRendererCommand& cmd,
                                          NkICommandBuffer* rhiCmd,
                                          const NkRenderPassInfo& pass)          = 0;
        virtual void EndRenderPass_Impl  (NkICommandBuffer* cmd)                 = 0;
        virtual void EndFrame_Impl       (NkICommandBuffer* cmd)                 = 0;

        // Shader selection helper : retourne la variante adaptée à l'API courante
        const NkShaderStageDesc* SelectStage(const NkShaderDesc& desc,
                                              NkShaderStageType stage) const;
        NkShaderBackend          CurrentBackend() const;

        // Génération d'IDs
        uint64 NextId() {
            return ++mIdCounter;
        }

        // ── Accès aux entrées depuis les sous-classes ─────────────────────────
        NkShaderEntry*       GetShader      (NkShaderHandle h);
        NkTextureEntry*      GetTexture     (NkTextureHandle h);
        NkMeshEntry*         GetMesh        (NkMeshHandle h);
        NkMaterialEntry*     GetMaterial    (NkMaterialHandle h);
        NkMaterialInstEntry* GetMaterialInst(NkMaterialInstHandle h);
        NkCameraEntry*       GetCamera      (NkCameraHandle h);
        NkRenderTargetEntry* GetRenderTarget(NkRenderTargetHandle h);
        NkFontEntry*         GetFont        (NkFontHandle h);

        const NkShaderEntry*       GetShader      (NkShaderHandle h) const;
        const NkTextureEntry*      GetTexture     (NkTextureHandle h) const;
        const NkMeshEntry*         GetMesh        (NkMeshHandle h) const;
        const NkMaterialEntry*     GetMaterial    (NkMaterialHandle h) const;
        const NkMaterialInstEntry* GetMaterialInst(NkMaterialInstHandle h) const;
        const NkCameraEntry*       GetCamera      (NkCameraHandle h) const;
        const NkRenderTargetEntry* GetRenderTarget(NkRenderTargetHandle h) const;
        const NkFontEntry*         GetFont        (NkFontHandle h) const;

        // Registre des uniform blocks (opaque vers le RHI)
        struct UniformBlockEntry {
            uint64 rhiBuffer = 0;
            uint32 sizeBytes = 0;
            bool   valid     = false;
        };
        UniformBlockEntry* GetUniformBlock(NkUniformBlockHandle h);

        NkIDevice* mDevice = nullptr;
        bool       mValid  = false;

        NkRendererCommand mCommand;
        NkRendererStats   mStats;

        NkRenderPassInfo       mCurrentPass;
        NkICommandBuffer*      mCurrentCmd = nullptr;

    private:
        // Registres (thread-safe via mutex par registre)
        mutable threading::NkMutex mShaderMutex;
        mutable threading::NkMutex mTextureMutex;
        mutable threading::NkMutex mMeshMutex;
        mutable threading::NkMutex mMatMutex;
        mutable threading::NkMutex mCamMutex;
        mutable threading::NkMutex mRTMutex;
        mutable threading::NkMutex mFontMutex;
        mutable threading::NkMutex mUBMutex;

        NkHashMap<uint64, NkShaderEntry>       mShaders;
        NkHashMap<uint64, NkTextureEntry>      mTextures;
        NkHashMap<uint64, NkMeshEntry>         mMeshes;
        NkHashMap<uint64, NkMaterialEntry>     mMaterials;
        NkHashMap<uint64, NkMaterialInstEntry> mMaterialInsts;
        NkHashMap<uint64, NkCameraEntry>       mCameras;
        NkHashMap<uint64, NkRenderTargetEntry> mRenderTargets;
        NkHashMap<uint64, NkFontEntry>         mFonts;
        NkHashMap<uint64, UniformBlockEntry>   mUniformBlocks;

        uint64 mIdCounter = 0;

        bool SetMaterialParam(NkMaterialInstHandle mat, const NkMaterialParam& param);
    };

} // namespace renderer
} // namespace nkentseu