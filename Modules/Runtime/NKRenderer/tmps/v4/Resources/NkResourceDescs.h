#pragma once
// =============================================================================
// NkResourceDescs.h — Descripteurs de ressources renderer
// Inclure ce fichier depuis NkResourceManager.h
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Resources/NkVertexFormats.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // NkTextureDesc — descripteur de texture haut-niveau
        // =============================================================================
        struct NkTextureDesc {
            uint32        width       = 1;
            uint32        height      = 1;
            uint32        depth       = 1;
            uint32        layers      = 1;
            uint32        mipLevels   = 1;   // 0 = toute la chaîne
            NkPixelFormat format      = NkPixelFormat::NK_RGBA8_SRGB;
            NkTextureKind kind        = NkTextureKind::NK_2D;
            NkMSAA        msaa        = NkMSAA::NK_1X;

            // Source des données
            enum class Source : uint32 {
                NK_EMPTY, NK_RAW_PIXELS, NK_FILE_PATH, NK_RENDER_TARGET, NK_DEPTH_STENCIL
            } source = Source::NK_EMPTY;
            const void*   pixels      = nullptr;
            uint32        rowPitch    = 0;
            NkString      filePath;
            bool          srgb        = true;
            bool          generateMips= false;

            // Sampler intégré
            NkFilterMode  filterMag   = NkFilterMode::NK_LINEAR;
            NkFilterMode  filterMin   = NkFilterMode::NK_LINEAR;
            NkMipFilter   filterMip   = NkMipFilter::NK_LINEAR;
            NkWrapMode    wrapU       = NkWrapMode::NK_REPEAT;
            NkWrapMode    wrapV       = NkWrapMode::NK_REPEAT;
            NkWrapMode    wrapW       = NkWrapMode::NK_REPEAT;
            float32       maxAniso    = 1.f;
            bool          shadowSampler = false;

            const char*   debugName   = nullptr;

            // ── Factory helpers ──────────────────────────────────────────────────────
            static NkTextureDesc Tex2D(uint32 w, uint32 h,
                                        NkPixelFormat fmt = NkPixelFormat::NK_RGBA8_SRGB,
                                        uint32 mips = 1) {
                NkTextureDesc d; d.width=w; d.height=h; d.format=fmt;
                d.mipLevels=mips; d.kind=NkTextureKind::NK_2D; return d;
            }
            static NkTextureDesc FromFile(const char* path, bool srgb=true, bool mips=true) {
                NkTextureDesc d; d.source=Source::NK_FILE_PATH; d.filePath=path;
                d.srgb=srgb; d.generateMips=mips; d.mipLevels=0; return d;
            }
            static NkTextureDesc FromMemory(const void* px, uint32 w, uint32 h,
                                             NkPixelFormat fmt=NkPixelFormat::NK_RGBA8) {
                NkTextureDesc d; d.source=Source::NK_RAW_PIXELS; d.pixels=px;
                d.width=w; d.height=h; d.format=fmt; return d;
            }
            static NkTextureDesc RenderTarget(uint32 w, uint32 h,
                                               NkPixelFormat fmt=NkPixelFormat::NK_RGBA16F,
                                               NkMSAA msaa=NkMSAA::NK_1X) {
                NkTextureDesc d; d.kind=NkTextureKind::NK_RENDER_TARGET;
                d.source=Source::NK_RENDER_TARGET;
                d.width=w; d.height=h; d.format=fmt; d.msaa=msaa; d.mipLevels=1;
                d.wrapU=d.wrapV=NkWrapMode::NK_CLAMP; return d;
            }
            static NkTextureDesc DepthStencil(uint32 w, uint32 h,
                                               NkPixelFormat fmt=NkPixelFormat::NK_D32F,
                                               NkMSAA msaa=NkMSAA::NK_1X) {
                NkTextureDesc d; d.kind=NkTextureKind::NK_DEPTH_STENCIL;
                d.source=Source::NK_DEPTH_STENCIL;
                d.width=w; d.height=h; d.format=fmt; d.msaa=msaa; d.mipLevels=1;
                d.shadowSampler=true;
                d.wrapU=d.wrapV=NkWrapMode::NK_CLAMP; return d;
            }
            static NkTextureDesc Cubemap(uint32 sz,
                                          NkPixelFormat fmt=NkPixelFormat::NK_RGBA16F,
                                          uint32 mips=0) {
                NkTextureDesc d; d.kind=NkTextureKind::NK_CUBE;
                d.width=d.height=sz; d.layers=6; d.format=fmt; d.mipLevels=mips;
                return d;
            }
        };

        // =============================================================================
        // NkSubMesh — section d'un mesh avec son matériau
        // =============================================================================
        struct NkSubMesh {
            NkString             name;
            uint32               indexOffset  = 0;
            uint32               indexCount   = 0;
            int32                vertexBase   = 0;
            NkMaterialInstHandle material;
            NkPrimitive          primitive    = NkPrimitive::NK_TRIANGLES;
            NkAABB               aabb;

            static NkSubMesh Make(const char* n, uint32 off, uint32 count,
                                    NkMaterialInstHandle mat = {},
                                    int32 base = 0) {
                NkSubMesh s; s.name=n; s.indexOffset=off; s.indexCount=count;
                s.material=mat; s.vertexBase=base; return s;
            }
        };

        // =============================================================================
        // NkMeshDesc — descripteur de mesh
        // =============================================================================
        struct NkMeshDesc {
            NkString       name;
            NkPrimitive    primitive    = NkPrimitive::NK_TRIANGLES;

            const void*    vertexData   = nullptr;
            uint32         vertexCount  = 0;
            uint32         vertexStride = sizeof(NkVertex3D);
            NkVertexLayout vertexLayout;

            const void*    indexData    = nullptr;
            uint32         indexCount   = 0;
            NkIndexFormat  indexFormat  = NkIndexFormat::NK_UINT32;

            NkVector<NkSubMesh> subMeshes;
            NkAABB         aabb;

            bool           computeNormals  = false;
            bool           computeTangents = false;
            bool           computeAABB     = true;
            bool           isDynamic       = false;  // true = upload fréquent
            const char*    debugName       = nullptr;

            template<typename V = NkVertex3D>
            static NkMeshDesc FromBuffers(const V* verts, uint32 numV,
                                           const uint32* idx = nullptr, uint32 numI = 0,
                                           const char* n = nullptr) {
                NkMeshDesc d;
                d.name=n?n:""; d.vertexData=verts; d.vertexCount=numV;
                d.vertexStride=sizeof(V); d.vertexLayout=V::Layout();
                d.indexData=idx; d.indexCount=numI;
                d.indexFormat=NkIndexFormat::NK_UINT32; d.debugName=n;
                return d;
            }
            NkMeshDesc& AddSubMesh(NkSubMesh s) { subMeshes.PushBack(s); return *this; }
        };

        // =============================================================================
        // NkDynamicMeshDesc — mesh mis à jour chaque frame
        // =============================================================================
        struct NkDynamicMeshDesc {
            NkString       name;
            uint32         maxVertices  = 0;
            uint32         maxIndices   = 0;
            uint32         vertexStride = sizeof(NkVertex3D);
            NkVertexLayout vertexLayout;
            NkIndexFormat  indexFormat  = NkIndexFormat::NK_UINT32;
            NkPrimitive    primitive    = NkPrimitive::NK_TRIANGLES;
            const char*    debugName    = nullptr;

            template<typename V = NkVertex3D>
            static NkDynamicMeshDesc Create(uint32 maxV, uint32 maxI = 0,
                                              const char* n = nullptr) {
                NkDynamicMeshDesc d;
                d.name=n?n:""; d.maxVertices=maxV; d.maxIndices=maxI;
                d.vertexStride=sizeof(V); d.vertexLayout=V::Layout();
                d.indexFormat = (maxV > 65535) ? NkIndexFormat::NK_UINT32
                                               : NkIndexFormat::NK_UINT16;
                d.debugName=n; return d;
            }
        };

        // =============================================================================
        // NkShaderDesc — sources par backend (wrapping NkRHI sans l'exposer)
        // =============================================================================
        struct NkShaderStageSource {
            NkShaderStageFlag stage     = NkShaderStageFlag::NK_VERTEX;
            NkString          source;        // GLSL / HLSL / MSL / NkSL
            NkString          entryPoint = "main";
            NkVector<uint32>  spirv;         // SPIR-V précompilé (VK)
        };

        struct NkShaderDesc {
            const char* name = nullptr;
            NkVector<NkShaderStageSource> gl;    // OpenGL GLSL 4.60
            NkVector<NkShaderStageSource> vk;    // Vulkan GLSL / SPIR-V
            NkVector<NkShaderStageSource> dx11;  // HLSL SM 5.0
            NkVector<NkShaderStageSource> dx12;  // HLSL SM 6.x
            NkVector<NkShaderStageSource> mtl;   // MSL 2.x
            NkVector<NkShaderStageSource> nksl;  // NkSL universel

            NkShaderDesc& SetGL  (NkShaderStageFlag s, const char* src,
                                   const char* e = "main") {
                NkShaderStageSource ss; ss.stage=s; ss.source=src; ss.entryPoint=e;
                gl.PushBack(ss); return *this;
            }
            NkShaderDesc& SetVK  (NkShaderStageFlag s, const char* src,
                                   const char* e = "main") {
                NkShaderStageSource ss; ss.stage=s; ss.source=src; ss.entryPoint=e;
                vk.PushBack(ss); return *this;
            }
            NkShaderDesc& SetDX11(NkShaderStageFlag s, const char* src,
                                   const char* e = "main") {
                NkShaderStageSource ss; ss.stage=s; ss.source=src; ss.entryPoint=e;
                dx11.PushBack(ss); return *this;
            }
            NkShaderDesc& SetDX12(NkShaderStageFlag s, const char* src,
                                   const char* e = "main") {
                NkShaderStageSource ss; ss.stage=s; ss.source=src; ss.entryPoint=e;
                dx12.PushBack(ss); return *this;
            }
            NkShaderDesc& SetMTL (NkShaderStageFlag s, const char* src,
                                   const char* e = "main") {
                NkShaderStageSource ss; ss.stage=s; ss.source=src; ss.entryPoint=e;
                mtl.PushBack(ss); return *this;
            }
            NkShaderDesc& SetNkSL(NkShaderStageFlag s, const char* src,
                                   const char* e = "main") {
                NkShaderStageSource ss; ss.stage=s; ss.source=src; ss.entryPoint=e;
                nksl.PushBack(ss); return *this;
            }
            bool IsValid() const {
                return !gl.IsEmpty()||!vk.IsEmpty()||!dx11.IsEmpty()||
                       !dx12.IsEmpty()||!mtl.IsEmpty()||!nksl.IsEmpty();
            }
        };

        // =============================================================================
        // NkFontDesc — descripteur de police de caractères
        // =============================================================================
        struct NkFontDesc {
            NkString    filePath;
            float32     size        = 16.f;
            uint32      atlasSize   = 1024;    // Taille de l'atlas texture
            NkString    glyphRanges = "ASCII"; // "ASCII", "Latin", "Full", custom
            bool        sdf         = false;   // Signed Distance Field
            float32     sdfSpread   = 4.f;
            const char* debugName   = nullptr;

            static NkFontDesc TTF(const char* path, float32 sz = 16.f,
                                   uint32 atlasSize = 1024) {
                NkFontDesc d; d.filePath=path; d.size=sz; d.atlasSize=atlasSize;
                return d;
            }
            static NkFontDesc SDF(const char* path, float32 sz = 32.f) {
                NkFontDesc d; d.filePath=path; d.size=sz; d.sdf=true;
                d.atlasSize=2048; return d;
            }
        };

        // =============================================================================
        // NkGlyphInfo — info d'un glyphe dans l'atlas
        // =============================================================================
        struct NkGlyphInfo {
            uint32  codepoint  = 0;
            float32 uvX        = 0, uvY = 0;
            float32 uvW        = 0, uvH = 0;
            float32 bearingX   = 0, bearingY = 0;
            float32 width      = 0, height   = 0;
            float32 advance    = 0;
        };

        // =============================================================================
        // NkRenderTargetDesc — render target multi-attachment
        // =============================================================================
        struct NkRenderTargetDesc {
            uint32                     width  = 0;   // 0 = swapchain size
            uint32                     height = 0;
            NkVector<NkTextureHandle>  colorAttachments;
            NkTextureHandle            depthAttachment;
            bool                       hasDepth = false;
            const char*                debugName = nullptr;

            NkRenderTargetDesc& AddColor(NkTextureHandle h) {
                colorAttachments.PushBack(h); return *this;
            }
            NkRenderTargetDesc& SetDepth(NkTextureHandle h) {
                depthAttachment=h; hasDepth=true; return *this;
            }
        };

    } // namespace renderer
} // namespace nkentseu
