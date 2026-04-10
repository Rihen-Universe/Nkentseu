#pragma once
// =============================================================================
// NkUniform.h  —  Uniform buffers, push constants, descriptor set layouts
// =============================================================================
#include "NKRenderer/Core/NkRef.h"
#include "NKRenderer/Core/NkRenderTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include <cstring>

namespace nkentseu {
namespace render {

class NkRenderDevice;
class NkTexture;

// =============================================================================
// Push Constant raw
// =============================================================================
struct NkPushConstantRaw {
    uint8  data[kNkMaxPushConstantBytes]{};
    uint32 size   = 0;
    uint32 offset = 0;
    NkShaderStage stages = NkShaderStage::NK_ALL_GRAPHICS;

    const void* Ptr()   const { return data+offset; }
    uint32      Bytes() const { return size; }
};

template<typename T>
struct NkPushConstant {
    static_assert(sizeof(T)<=kNkMaxPushConstantBytes,
                  "NkPushConstant: T > 128 bytes limit");
    T             data{};
    NkShaderStage stages = NkShaderStage::NK_ALL_GRAPHICS;
    uint32        offset = 0;

    NkPushConstantRaw ToRaw() const {
        NkPushConstantRaw r;
        r.size=sizeof(T); r.offset=offset; r.stages=stages;
        memcpy(r.data+offset, &data, sizeof(T));
        return r;
    }
};

// =============================================================================
// Descripteur d'uniform buffer
// =============================================================================
struct NkUniformDesc {
    uint64       sizeBytes        = 0;
    bool         dynamic          = false;
    uint32       dynamicMaxCount  = 1;
    const void*  initialData      = nullptr;
    const char*  debugName        = nullptr;

    static NkUniformDesc Static(uint64 bytes, const void* data=nullptr, const char* n=nullptr) {
        NkUniformDesc d; d.sizeBytes=bytes; d.initialData=data; d.debugName=n; return d;
    }
    static NkUniformDesc Dynamic(uint64 entryBytes, uint32 maxCount, const char* n=nullptr) {
        NkUniformDesc d; d.sizeBytes=entryBytes; d.dynamic=true;
        d.dynamicMaxCount=maxCount; d.debugName=n; return d;
    }
    template<typename T>
    static NkUniformDesc ForType(const char* n=nullptr) { return Static(sizeof(T),nullptr,n); }
    template<typename T>
    static NkUniformDesc ForDynamic(uint32 count, const char* n=nullptr) { return Dynamic(sizeof(T),count,n); }
};

// =============================================================================
// NkUniform — UBO RefCounted
// =============================================================================
class NkUniform : public NkRefCounted {
public:
    const NkUniformDesc& Desc()      const { return mDesc; }
    uint64               SizeBytes() const { return mDesc.sizeBytes; }
    bool                 IsDynamic() const { return mDesc.dynamic; }
    uint64               RHIBufferId()const { return mRHIBufferId; }

protected:
    friend class NkRenderDevice;
    explicit NkUniform(const NkUniformDesc& d) : mDesc(d), mRHIBufferId(0) {}
    virtual void Destroy() override;

    NkUniformDesc mDesc;
    uint64        mRHIBufferId = 0;
    NkRenderDevice* mDevice   = nullptr;
};

using NkUniformPtr = NkRef<NkUniform>;

// =============================================================================
// Type de binding dans un descriptor set
// =============================================================================
enum class NkBindType : uint32 {
    NK_UBO,            // Uniform Buffer Object
    NK_UBO_DYNAMIC,    // UBO avec offset variable
    NK_SSBO,           // Storage Buffer
    NK_SSBO_DYNAMIC,
    NK_TEXTURE_SAMPLER,// Texture + sampler combinés
    NK_TEXTURE_ONLY,   // SRV seul
    NK_SAMPLER_ONLY,   // Sampler seul
    NK_STORAGE_IMAGE,  // Image load/store (compute)
    NK_INPUT_ATTACH,   // Subpass input (Vulkan only)
};

// =============================================================================
// Slot d'un descriptor set layout
// =============================================================================
struct NkBindSlot {
    uint32        binding = 0;
    NkBindType    type    = NkBindType::NK_UBO;
    NkShaderStage stages  = NkShaderStage::NK_ALL_GRAPHICS;
    uint32        count   = 1;
    const char*   name    = nullptr;
};

// =============================================================================
// Layout d'un descriptor set
//   Set 0 : per-frame   (camera, lumières, temps)
//   Set 1 : per-pass    (shadow map, environnement)
//   Set 2 : per-material(albedo, normal, ORM)
//   Set 3 : per-object  (model matrix, bone palette)
// =============================================================================
struct NkDescSetLayout {
    uint32               setIndex  = 0;
    NkVector<NkBindSlot> slots;
    const char*          debugName = nullptr;

    NkDescSetLayout& Slot(uint32 b, NkBindType t,
                           NkShaderStage s=NkShaderStage::NK_ALL_GRAPHICS,
                           uint32 count=1, const char* n=nullptr) {
        slots.PushBack({b,t,s,count,n}); return *this;
    }
    NkDescSetLayout& UBO(uint32 b, NkShaderStage s=NkShaderStage::NK_ALL_GRAPHICS,
                          const char* n=nullptr) {
        return Slot(b, NkBindType::NK_UBO, s, 1, n);
    }
    NkDescSetLayout& Tex(uint32 b, NkShaderStage s=NkShaderStage::NK_FRAGMENT,
                          const char* n=nullptr) {
        return Slot(b, NkBindType::NK_TEXTURE_SAMPLER, s, 1, n);
    }
    NkDescSetLayout& SSBO(uint32 b, NkShaderStage s=NkShaderStage::NK_COMPUTE,
                           const char* n=nullptr) {
        return Slot(b, NkBindType::NK_SSBO, s, 1, n);
    }
    NkDescSetLayout& StorageImage(uint32 b, NkShaderStage s=NkShaderStage::NK_COMPUTE,
                                   const char* n=nullptr) {
        return Slot(b, NkBindType::NK_STORAGE_IMAGE, s, 1, n);
    }
};

// =============================================================================
// UBOs standards (std140, 16-byte aligned)
// =============================================================================

// Per-frame (Set 0, Binding 0) — 304 bytes
struct alignas(16) NkSceneUBO {
    float model[16];    float view[16];    float proj[16];
    float lightVP[16];
    float lightDir[4];  float eyePos[4];
    float time;         float dt;
    float ndcZScale;    float ndcZOffset;
    // total = 4×64 + 2×16 + 16 = 288 bytes
};

// Per-object push constant — 80 bytes (< 128)
struct alignas(16) NkObjectPC {
    float model[16];  // 64
    float tint[4];    // 16
    // total = 80
};

// Bones palette pour skinning (Set 3)
// Supporte jusqu'à 64 bones (64×64 = 4096 bytes)
struct alignas(16) NkBonesUBO {
    static constexpr uint32 kMaxBones = 64;
    float bones[kMaxBones][16];  // mat4[64]
};

// Material PBR (Set 2, Binding 0)
struct alignas(16) NkPBRMaterialUBO {
    float baseColor[4];        // vec4 (rgba)
    float emissiveColor[4];    // vec4 (rgb + intensité)
    float metallic;            // float
    float roughness;           // float
    float occlusion;           // float
    float normalScale;         // float
    float alphaClip;           // float
    float emissiveScale;       // float
    float clearcoat;           // float
    float clearcoatRoughness;  // float
    float transmission;        // float
    float ior;                 // float
    float _pad[2];
};

// Material Phong (Set 2, Binding 0)
struct alignas(16) NkPhongMaterialUBO {
    float ambient[4];   float diffuse[4];   float specular[4];
    float shininess;    float _pad[3];
};

// Material Toon (Set 2, Binding 0)
struct alignas(16) NkToonMaterialUBO {
    float baseColor[4]; float shadowColor[4]; float outlineColor[4];
    float outlineWidth; float shadowThreshold; float shadowSmoothness;
    float specThreshold; float specSmoothness;
    uint32 shadeSteps;
    float _pad[2];
};

} // namespace render
} // namespace nkentseu