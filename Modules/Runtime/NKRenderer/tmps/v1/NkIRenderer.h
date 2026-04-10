#pragma once
// =============================================================================
// NkIRenderer.h
// Interface abstraite du renderer haut niveau.
//
// NkIRenderer est LE point d'entrée unique de NKRenderer pour l'application.
// Il expose une API orientée ressources (Create/Destroy) et une API de rendu
// par frame (BeginFrame / Submit / EndFrame).
//
// La création effective passe par NkRendererFactory::Create(device, api).
// L'implémentation concrète (NkOpenGLRenderer, NkVulkanRenderer, etc.) cache
// complètement le RHI — l'application n'inclut jamais NkIDevice directement.
//
// Usage typique :
//   NkIRenderer* r = NkRendererFactory::Create(device, api);
//   NkShaderHandle hSh    = r->CreateShader(shaderDesc);
//   NkTextureHandle hTex  = r->CreateTexture(texDesc);
//   NkMeshHandle hMesh    = r->CreateMesh(meshDesc);
//   NkMaterialHandle hMat = r->CreateMaterialTemplate(matDesc);
//   NkMaterialInstHandle hInst = r->CreateMaterialInstance(instDesc);
//   NkCameraHandle hCam   = r->CreateCamera3D(camDesc);
//
//   // Frame loop:
//   r->BeginFrame();
//   r->BeginRenderPass(passInfo);
//     NkRendererCommand& cmd = r->GetCommand();
//     cmd.Draw3D({hMesh, hInst, transform});
//     cmd.DrawText({hFont, "Hello", {0,0}});
//   r->EndRenderPass();
//   r->EndFrame();
// =============================================================================
#include "NkRendererTypes.h"

namespace nkentseu {
namespace renderer {

    class NkRendererCommand; // forward

    class NkIRenderer {
    public:
        virtual ~NkIRenderer() = default;

        // ── Cycle de vie ─────────────────────────────────────────────────────
        virtual bool Initialize()   = 0;
        virtual void Shutdown()     = 0;
        virtual bool IsValid() const= 0;

        // ── Création / destruction de ressources ──────────────────────────────

        // Shader (source ou fichier — le renderer choisit la variante selon l'API)
        virtual NkShaderHandle       CreateShader(const NkShaderDesc& desc)                     = 0;
        virtual void                 DestroyShader(NkShaderHandle& handle)                      = 0;
        virtual bool                 ReloadShader(NkShaderHandle handle)                        = 0;

        // Texture
        virtual NkTextureHandle      CreateTexture(const NkTextureDesc& desc)                   = 0;
        virtual void                 DestroyTexture(NkTextureHandle& handle)                    = 0;
        virtual bool                 UpdateTexture(NkTextureHandle tex, const void* data,
                                                   uint32 rowPitch=0,
                                                   uint32 mipLevel=0, uint32 layer=0)           = 0;
        virtual bool                 GenerateMipmaps(NkTextureHandle tex)                       = 0;

        // Mesh
        virtual NkMeshHandle         CreateMesh(const NkMeshDesc& desc)                        = 0;
        virtual void                 DestroyMesh(NkMeshHandle& handle)                         = 0;
        virtual bool                 UpdateMesh(NkMeshHandle mesh,
                                                const void* vertices, uint32 vertexCount,
                                                const void* indices,  uint32 indexCount)       = 0;

        // Material template (lié à un shader, définit les paramètres par défaut)
        virtual NkMaterialHandle     CreateMaterialTemplate(const NkMaterialTemplateDesc& desc) = 0;
        virtual void                 DestroyMaterialTemplate(NkMaterialHandle& handle)          = 0;

        // Material instance (overrides d'un template)
        virtual NkMaterialInstHandle CreateMaterialInstance(const NkMaterialInstanceDesc& desc) = 0;
        virtual void                 DestroyMaterialInstance(NkMaterialInstHandle& handle)      = 0;

        // Paramètre d'instance (texture, float, vec4…)
        virtual bool SetMaterialFloat  (NkMaterialInstHandle mat, const char* name, float32 v)  = 0;
        virtual bool SetMaterialVec2   (NkMaterialInstHandle mat, const char* name, NkVec2f v)  = 0;
        virtual bool SetMaterialVec3   (NkMaterialInstHandle mat, const char* name, NkVec3f v)  = 0;
        virtual bool SetMaterialVec4   (NkMaterialInstHandle mat, const char* name, NkVec4f v)  = 0;
        virtual bool SetMaterialInt    (NkMaterialInstHandle mat, const char* name, int32 v)    = 0;
        virtual bool SetMaterialBool   (NkMaterialInstHandle mat, const char* name, bool v)     = 0;
        virtual bool SetMaterialTexture(NkMaterialInstHandle mat, const char* name,
                                        NkTextureHandle tex)                                    = 0;

        // Camera 2D / 3D
        virtual NkCameraHandle       CreateCamera2D(const NkCameraDesc2D& desc)                = 0;
        virtual NkCameraHandle       CreateCamera3D(const NkCameraDesc3D& desc)                = 0;
        virtual void                 DestroyCamera(NkCameraHandle& handle)                     = 0;
        virtual bool                 UpdateCamera2D(NkCameraHandle cam, const NkCameraDesc2D& desc) = 0;
        virtual bool                 UpdateCamera3D(NkCameraHandle cam, const NkCameraDesc3D& desc) = 0;
        virtual NkMat4f              GetViewMatrix(NkCameraHandle cam) const                   = 0;
        virtual NkMat4f              GetProjectionMatrix(NkCameraHandle cam) const             = 0;
        virtual NkMat4f              GetViewProjectionMatrix(NkCameraHandle cam) const         = 0;

        // Render target offscreen
        virtual NkRenderTargetHandle CreateRenderTarget(const NkRenderTargetDesc& desc)        = 0;
        virtual void                 DestroyRenderTarget(NkRenderTargetHandle& handle)         = 0;
        virtual NkTextureHandle      GetRenderTargetColor(NkRenderTargetHandle rt,
                                                          uint32 slot=0) const                 = 0;
        virtual NkTextureHandle      GetRenderTargetDepth(NkRenderTargetHandle rt) const       = 0;

        // Font
        virtual NkFontHandle         CreateFont(const NkFontDesc& desc)                        = 0;
        virtual void                 DestroyFont(NkFontHandle& handle)                         = 0;
        virtual bool                 GetGlyph(NkFontHandle font, uint32 codepoint,
                                              NkGlyphInfo& out) const                          = 0;
        virtual NkTextureHandle      GetFontAtlasTexture(NkFontHandle font) const              = 0;

        // Uniform block explicite (pour les shaders custom)
        virtual NkUniformBlockHandle CreateUniformBlock(const NkUniformBlockDesc& desc)        = 0;
        virtual void                 DestroyUniformBlock(NkUniformBlockHandle& handle)         = 0;
        virtual bool                 WriteUniformBlock(NkUniformBlockHandle ub,
                                                       const void* data, uint32 size,
                                                       uint32 offset=0)                        = 0;

        // ── Frame ─────────────────────────────────────────────────────────────
        // BeginFrame prépare le command buffer du frame courant.
        virtual bool               BeginFrame()                              = 0;

        // BeginRenderPass configure la passe de rendu (camera, clear, target).
        virtual bool               BeginRenderPass(const NkRenderPassInfo& info) = 0;

        // Accès à la commande list du pass courant.
        virtual NkRendererCommand& GetCommand()                              = 0;

        // EndRenderPass soumet la passe et insère les barrières nécessaires.
        virtual void               EndRenderPass()                           = 0;

        // EndFrame présente et libère les ressources frame.
        virtual void               EndFrame()                                = 0;

        // ── Resize ────────────────────────────────────────────────────────────
        virtual void OnResize(uint32 width, uint32 height)                   = 0;
        virtual uint32 GetSwapchainWidth()  const                            = 0;
        virtual uint32 GetSwapchainHeight() const                            = 0;

        // ── Stats et debug ────────────────────────────────────────────────────
        virtual const NkRendererStats& GetLastFrameStats() const             = 0;
        virtual void SetDebugName(NkShaderHandle h,       const char* name) {}
        virtual void SetDebugName(NkTextureHandle h,      const char* name) {}
        virtual void SetDebugName(NkMeshHandle h,         const char* name) {}
        virtual void SetDebugName(NkMaterialHandle h,     const char* name) {}
        virtual void SetDebugName(NkRenderTargetHandle h, const char* name) {}

        // ── Shortcuts ─────────────────────────────────────────────────────────
        // Créer un mesh Quad 2D centré sur l'origine ([-0.5, 0.5] en X et Y)
        NkMeshHandle CreateQuad2D(float32 width=1.f, float32 height=1.f);

        // Créer un mesh cube 3D unitaire
        NkMeshHandle CreateCube3D(float32 halfExtent=0.5f);

        // Créer une sphère UV 3D
        NkMeshHandle CreateSphere3D(float32 radius=0.5f, uint32 stacks=16, uint32 slices=24);

        // Créer un plan XZ 3D
        NkMeshHandle CreatePlane3D(float32 width=1.f, float32 depth=1.f,
                                   uint32 subdivW=1, uint32 subdivD=1);

        // Charger une texture depuis un fichier image
        NkTextureHandle LoadTexture(const char* filePath,
                                    NkTextureFormat fmt = NkTextureFormat::RGBA8_SRGB,
                                    bool generateMipmaps = true);
    };

} // namespace renderer
} // namespace nkentseu