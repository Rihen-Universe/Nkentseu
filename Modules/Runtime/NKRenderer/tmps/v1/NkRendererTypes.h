#pragma once
// =============================================================================
// NkRendererTypes.h
// Types fondamentaux du module NKRenderer.
// Ce fichier est l'unique source de vérité pour les handles, enums et structs
// utilisateurs — aucune référence aux types RHI internes (NkRhiHandle, etc.)
// =============================================================================
#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
namespace renderer {

    using namespace math;

    // =========================================================================
    // Handles opaques — identifiants 64-bit, 0 = invalide
    // =========================================================================
    template<typename Tag>
    struct NkRendererHandle {
        uint64 id = 0;
        bool IsValid() const noexcept { return id != 0; }
        bool operator==(const NkRendererHandle& o) const noexcept { return id == o.id; }
        bool operator!=(const NkRendererHandle& o) const noexcept { return id != o.id; }
        static NkRendererHandle Null() noexcept { return {0}; }
    };

    struct TagShader       {};
    struct TagTexture      {};
    struct TagMesh         {};
    struct TagMaterial     {};
    struct TagMaterialInst {};
    struct TagCamera       {};
    struct TagFont         {};
    struct TagModel        {};
    struct TagRenderTarget {};
    struct TagUniformBlock {};

    using NkShaderHandle       = NkRendererHandle<TagShader>;
    using NkTextureHandle      = NkRendererHandle<TagTexture>;
    using NkMeshHandle         = NkRendererHandle<TagMesh>;
    using NkMaterialHandle     = NkRendererHandle<TagMaterial>;
    using NkMaterialInstHandle = NkRendererHandle<TagMaterialInst>;
    using NkCameraHandle       = NkRendererHandle<TagCamera>;
    using NkFontHandle         = NkRendererHandle<TagFont>;
    using NkModelHandle        = NkRendererHandle<TagModel>;
    using NkRenderTargetHandle = NkRendererHandle<TagRenderTarget>;
    using NkUniformBlockHandle = NkRendererHandle<TagUniformBlock>;

    // =========================================================================
    // Vertex layouts
    // =========================================================================
    struct NkVertex2D {
        NkVec2f position;
        NkVec2f uv;
        NkVec4f color;  // linear float [0,1]
    };

    struct NkVertex3D {
        NkVec3f position;
        NkVec3f normal;
        NkVec3f tangent;
        NkVec2f uv;
        NkVec2f uv2;    // lightmap UV
        NkVec4f color;
    };

    struct NkVertexSkinned : NkVertex3D {
        uint32  boneIndices[4] = {0,0,0,0};
        float32 boneWeights[4] = {1.f,0.f,0.f,0.f};
    };

    // =========================================================================
    // Shader — types de stage et de backend
    // =========================================================================
    enum class NkShaderStageType : uint8 {
        Vertex   = 0,
        Fragment = 1,
        Geometry = 2,
        TessCtrl = 3,
        TessEval = 4,
        Compute  = 5,
    };

    enum class NkShaderBackend : uint8 {
        OpenGL  = 0,  // GLSL 4.6 / 4.3
        Vulkan  = 1,  // GLSL-VK (set/binding decorators), compilé SPIRV
        DX11    = 2,  // HLSL SM 5.0 — fxc
        DX12    = 3,  // HLSL SM 6.x — dxc
    };

    struct NkShaderStageDesc {
        NkShaderStageType stage  = NkShaderStageType::Vertex;
        NkShaderBackend   backend= NkShaderBackend::OpenGL;
        NkString          source;        // source texte OU chemin de fichier
        NkString          entryPoint = "main";
        bool              isFilePath = false;
    };

    struct NkShaderDesc {
        NkVector<NkShaderStageDesc> stages;
        NkString debugName;

        NkShaderDesc& AddGL  (NkShaderStageType s, const char* src, const char* entry="main");
        NkShaderDesc& AddVK  (NkShaderStageType s, const char* src, const char* entry="main");
        NkShaderDesc& AddDX11(NkShaderStageType s, const char* src, const char* entry="main");
        NkShaderDesc& AddDX12(NkShaderStageType s, const char* src, const char* entry="main");
        NkShaderDesc& AddGLFile  (NkShaderStageType s, const char* path, const char* entry="main");
        NkShaderDesc& AddVKFile  (NkShaderStageType s, const char* path, const char* entry="main");
        NkShaderDesc& AddDX11File(NkShaderStageType s, const char* path, const char* entry="main");
        NkShaderDesc& AddDX12File(NkShaderStageType s, const char* path, const char* entry="main");
    };

    // =========================================================================
    // Texture
    // =========================================================================
    enum class NkTextureType : uint8 {
        Tex2D     = 0,
        Tex3D     = 1,
        Cube      = 2,
        Array2D   = 3,
        CubeArray = 4,
    };

    enum class NkTextureFormat : uint8 {
        RGBA8_SRGB  = 0,
        RGBA8_UNORM = 1,
        RGBA16F     = 2,
        RGBA32F     = 3,
        RG16F       = 4,
        R32F        = 5,
        BC1_SRGB    = 6,
        BC3_SRGB    = 7,
        BC5_UNORM   = 8,
        BC7_SRGB    = 9,
        D32F        = 10,
        D24S8       = 11,
    };

    enum class NkSamplerFilter : uint8 {
        Nearest    = 0,
        Linear     = 1,
        Anisotropic= 2,
    };

    enum class NkSamplerWrap : uint8 {
        Repeat        = 0,
        ClampToEdge   = 1,
        ClampToBorder = 2,
        MirroredRepeat= 3,
    };

    struct NkSamplerDesc {
        NkSamplerFilter filter    = NkSamplerFilter::Linear;
        NkSamplerWrap   wrapU     = NkSamplerWrap::Repeat;
        NkSamplerWrap   wrapV     = NkSamplerWrap::Repeat;
        NkSamplerWrap   wrapW     = NkSamplerWrap::Repeat;
        float32         anisotropy= 1.f;
        float32         mipLodBias= 0.f;
        bool            mipmaps   = true;

        static NkSamplerDesc Linear()      { return {}; }
        static NkSamplerDesc Nearest()     { NkSamplerDesc d; d.filter=NkSamplerFilter::Nearest; d.mipmaps=false; return d; }
        static NkSamplerDesc Anisotropic(float32 n=16.f) { NkSamplerDesc d; d.filter=NkSamplerFilter::Anisotropic; d.anisotropy=n; return d; }
        static NkSamplerDesc Clamp()       { NkSamplerDesc d; d.wrapU=d.wrapV=d.wrapW=NkSamplerWrap::ClampToEdge; return d; }
    };

    struct NkTextureDesc {
        NkTextureType   type       = NkTextureType::Tex2D;
        NkTextureFormat format     = NkTextureFormat::RGBA8_SRGB;
        uint32          width      = 1;
        uint32          height     = 1;
        uint32          depth      = 1;
        uint32          layers     = 1;
        uint32          mipLevels  = 0;   // 0 = auto-generate full chain
        bool            isRenderTarget = false;
        bool            isDepthTarget  = false;
        NkSamplerDesc   sampler;
        const void*     initialData = nullptr;
        uint32          rowPitch    = 0;
        const char*     debugName   = nullptr;

        static NkTextureDesc Tex2D(uint32 w, uint32 h,
                                   NkTextureFormat fmt=NkTextureFormat::RGBA8_SRGB,
                                   uint32 mips=0)
        { NkTextureDesc d; d.width=w; d.height=h; d.format=fmt; d.mipLevels=mips; return d; }

        static NkTextureDesc RenderTarget(uint32 w, uint32 h,
                                          NkTextureFormat fmt=NkTextureFormat::RGBA16F)
        { NkTextureDesc d; d.width=w; d.height=h; d.format=fmt; d.isRenderTarget=true; d.mipLevels=1; return d; }

        static NkTextureDesc Depth(uint32 w, uint32 h)
        { NkTextureDesc d; d.width=w; d.height=h; d.format=NkTextureFormat::D32F; d.isDepthTarget=true; d.mipLevels=1; return d; }

        static NkTextureDesc Cubemap(uint32 sz, NkTextureFormat fmt=NkTextureFormat::RGBA16F, uint32 mips=0)
        { NkTextureDesc d; d.type=NkTextureType::Cube; d.width=sz; d.height=sz; d.layers=6; d.format=fmt; d.mipLevels=mips; return d; }
    };

    // =========================================================================
    // Mesh
    // =========================================================================
    enum class NkMeshUsage : uint8 {
        Static  = 0,   // immutable, upload once
        Dynamic = 1,   // updated per-frame (streaming, particles)
        Editor  = 2,   // CPU-readable, for editor picking / deformation
    };

    enum class NkIndexFormat : uint8 { UInt16, UInt32 };

    enum class NkPrimitiveMode : uint8 {
        Triangles     = 0,
        Lines         = 1,
        Points        = 2,
        TriangleStrip = 3,
    };

    struct NkMeshDesc {
        NkMeshUsage     usage    = NkMeshUsage::Static;
        NkPrimitiveMode mode     = NkPrimitiveMode::Triangles;
        NkIndexFormat   idxFmt   = NkIndexFormat::UInt32;

        // Vertex data (exactly one of the following)
        const NkVertex2D* vertices2D = nullptr;
        const NkVertex3D* vertices3D = nullptr;
        const NkVertexSkinned* verticesSkinned = nullptr;
        uint32 vertexCount = 0;

        const uint16* indices16 = nullptr;
        const uint32* indices32 = nullptr;
        uint32 indexCount = 0;

        const char* debugName = nullptr;

        bool Is2D()      const { return vertices2D != nullptr; }
        bool IsSkinned() const { return verticesSkinned != nullptr; }
    };

    // =========================================================================
    // Material — système template + instance
    // =========================================================================
    enum class NkMaterialDomain : uint8 {
        Surface    = 0,  // standard opaque/transparent surface
        PostProcess= 1,  // screen-space post-processing
        UI         = 2,  // 2D UI rendering
        Particle   = 3,  // particle systems
        Decal      = 4,  // decal projection
    };

    enum class NkBlendMode : uint8 {
        Opaque      = 0,
        AlphaBlend  = 1,
        Additive    = 2,
        Multiply    = 3,
        PreMultAlpha= 4,
    };

    enum class NkShadingModel : uint8 {
        Unlit         = 0,
        Phong         = 1,
        PBR           = 2,   // metallic-roughness
        PBR_Specular  = 3,   // specular-glossiness
        Toon          = 4,
        Custom        = 5,
    };

    enum class NkCullMode : uint8 { Back, Front, None };
    enum class NkFillMode : uint8 { Solid, Wireframe };

    // Valeur d'un paramètre de matériau
    struct NkMaterialParam {
        enum class Type : uint8 { Float, Vec2, Vec3, Vec4, Int, Bool, Texture } type;
        union {
            float32  f;
            float32  v2[2];
            float32  v3[3];
            float32  v4[4];
            int32    i;
            bool     b;
        };
        NkTextureHandle texture;
        NkString        name;
    };

    struct NkMaterialTemplateDesc {
        NkShaderHandle     shader;
        NkMaterialDomain   domain       = NkMaterialDomain::Surface;
        NkShadingModel     shading      = NkShadingModel::PBR;
        NkBlendMode        blend        = NkBlendMode::Opaque;
        NkCullMode         cull         = NkCullMode::Back;
        NkFillMode         fill         = NkFillMode::Solid;
        bool               depthTest    = true;
        bool               depthWrite   = true;
        bool               castShadow   = true;
        bool               receiveShadow= true;
        NkVector<NkMaterialParam> params;
        const char*        debugName    = nullptr;
    };

    struct NkMaterialInstanceDesc {
        NkMaterialHandle   templateHandle;
        NkVector<NkMaterialParam> overrides;  // subset of template params to override
        const char*        debugName = nullptr;
    };

    // =========================================================================
    // Camera
    // =========================================================================
    enum class NkProjectionType : uint8 { Perspective, Orthographic };

    struct NkCameraDesc2D {
        NkVec2f position = {0.f, 0.f};
        float32 zoom     = 1.f;
        float32 rotation = 0.f;        // degrees
        float32 nearPlane= -100.f;
        float32 farPlane =  100.f;
        uint32  viewW    = 1280;
        uint32  viewH    = 720;
    };

    struct NkCameraDesc3D {
        NkVec3f position = {0.f, 0.f, 5.f};
        NkVec3f target   = {0.f, 0.f, 0.f};
        NkVec3f up       = {0.f, 1.f, 0.f};
        float32 fovY     = 60.f;           // degrees
        float32 nearPlane= 0.01f;
        float32 farPlane = 1000.f;
        float32 aspect   = 16.f / 9.f;
        NkProjectionType projection = NkProjectionType::Perspective;
        bool    reverseZ = false;          // Vulkan / DX12 prefer this
    };

    // =========================================================================
    // Uniform / Push constant
    // =========================================================================
    struct NkUniformBlockDesc {
        const char* name      = nullptr;
        uint32      sizeBytes = 0;
        uint32      binding   = 0;
        bool        isPerFrame= false;
        bool        isPerDraw = false;
    };

    struct NkPushConstantRange {
        uint32 offset     = 0;
        uint32 sizeBytes  = 0;
        uint32 stageFlags = 0;  // bitmask of NkShaderStageType
    };

    // =========================================================================
    // Render target
    // =========================================================================
    struct NkRenderTargetDesc {
        uint32                   width  = 0;   // 0 = swapchain size
        uint32                   height = 0;
        uint32                   msaa   = 1;
        NkVector<NkTextureHandle> colorAttachments;
        NkTextureHandle           depthAttachment;
        const char*               debugName = nullptr;
    };

    // =========================================================================
    // Draw command (soumis au NkRendererCommand)
    // =========================================================================
    struct NkDrawCall2D {
        NkMeshHandle         mesh;
        NkMaterialInstHandle material;
        NkMat3f              transform;   // 2D world transform
        float32              depth = 0.f; // sort key
        uint32               layer = 0;
    };

    struct NkDrawCall3D {
        NkMeshHandle         mesh;
        NkMaterialInstHandle material;
        NkMat4f              transform;
        uint32               castShadow   : 1;
        uint32               receiveShadow: 1;
        uint32               layer        : 8;
        uint32               _pad         : 22;

        NkDrawCall3D() : castShadow(1), receiveShadow(1), layer(0), _pad(0) {}
    };

    // =========================================================================
    // Render pass info (passé au renderer en début de frame)
    // =========================================================================
    struct NkRenderPassInfo {
        NkCameraHandle      camera;
        NkRenderTargetHandle target;   // handle nul = swapchain
        bool                 clearColor = true;
        bool                 clearDepth = true;
        NkVec4f              clearColorValue = {0.1f, 0.1f, 0.12f, 1.f};
        float32              clearDepthValue = 1.f;
    };

    // =========================================================================
    // Stats de rendu (lues après EndFrame)
    // =========================================================================
    struct NkRendererStats {
        uint32 drawCalls2D     = 0;
        uint32 drawCalls3D     = 0;
        uint32 triangles       = 0;
        uint32 vertices        = 0;
        uint32 shaderChanges   = 0;
        uint32 materialChanges = 0;
        uint32 textureBinds    = 0;
        float32 gpuTimeMs      = 0.f;
        float32 cpuSortTimeMs  = 0.f;
    };

    // =========================================================================
    // Font (NkFont) — intégration rendu texte
    // =========================================================================
    struct NkFontDesc {
        const char* filePath  = nullptr;
        float32     sizePt    = 16.f;
        bool        preloadASCII = true;
        bool        enableEmoji  = true;
        int32       atlasWidth   = 1024;
        int32       atlasHeight  = 1024;
        const char* debugName    = nullptr;
    };

    struct NkGlyphInfo {
        float32 x0,y0, x1,y1;    // atlas UV rect
        float32 bx,by;            // bearing (pixels)
        float32 advance;          // horizontal advance (pixels)
    };

    // =========================================================================
    // NkText — commande de rendu texte
    // =========================================================================
    struct NkTextDesc {
        NkFontHandle  font;
        NkString      text;
        NkVec2f       position;
        float32       size     = 0.f;   // 0 = utiliser taille native de la police
        NkVec4f       color    = {1.f, 1.f, 1.f, 1.f};
        float32       maxWidth = 0.f;   // 0 = pas de wrap
        bool          worldSpace = false;
    };

    // =========================================================================
    // Erreur renderer
    // =========================================================================
    enum class NkRendererResult : uint32 {
        Ok              = 0,
        InvalidParam    = 1,
        OutOfMemory     = 2,
        DeviceLost      = 3,
        ShaderCompile   = 4,
        NotSupported    = 5,
        AlreadyExists   = 6,
        Unknown         = 0xFFFFFFFF,
    };

    inline bool NkRendererOk(NkRendererResult r) { return r == NkRendererResult::Ok; }

} // namespace renderer
} // namespace nkentseu