// =============================================================================
// NkRenderer.h — Moteur de rendu multi-objet au-dessus du RHI
//
// Démontre comment Unity/Unreal organisent leur pipeline de rendu :
//
// NkRenderer
//  ├── NkMeshRenderer      — rendu de géométrie (vertex/index buffers)
//  ├── NkMaterialSystem    — gère pipelines + descriptors par matériau
//  └── NkSceneRenderer     — trie et soumet N objets par frame
//
// Chaque objet de scène a :
//  - un NkMesh    (géométrie : vertex buffer, index buffer)
//  - un NkMaterial(pipeline, descriptors, textures, uniforms)
//  - une transform (NkMat4x4 MVP via push constants)
// =============================================================================

#pragma once
#include "NkRHIDevice.h"
#include "NKLogger/NkLog.h"
#include <vector>
#include <memory>

namespace nkentseu { namespace rhi {

// ---------------------------------------------------------------------------
// NkMesh — géométrie GPU
// ---------------------------------------------------------------------------
struct NkVertex {
    float pos[3];    // position xyz
    float normal[3]; // normale xyz
    float uv[2];     // texture coords
    float color[4];  // couleur vertex RGBA
};

struct NkMesh {
    NkBufferHandle vertexBuffer;
    NkBufferHandle indexBuffer;
    NkU32          indexCount  = 0;
    NkU32          vertexCount = 0;
    NkIndexType    indexType   = NkIndexType::Uint32;

    static NkMesh Create(NkRHIDevice& dev,
                          const NkVertex* verts, NkU32 vcount,
                          const NkU32*    inds,  NkU32 icount) {
        NkMesh m;
        m.indexCount  = icount;
        m.vertexCount = vcount;

        NkBufferDesc vd;
        vd.size     = sizeof(NkVertex) * vcount;
        vd.usage    = NkBufferUsage::Vertex;
        vd.memory   = NkMemoryType::GpuOnly;
        vd.initData = verts;
        m.vertexBuffer = dev.CreateBuffer(vd);

        NkBufferDesc id_;
        id_.size     = sizeof(NkU32) * icount;
        id_.usage    = NkBufferUsage::Index;
        id_.memory   = NkMemoryType::GpuOnly;
        id_.initData = inds;
        m.indexBuffer = dev.CreateBuffer(id_);
        return m;
    }

    void Destroy(NkRHIDevice& dev) {
        dev.DestroyBuffer(vertexBuffer);
        dev.DestroyBuffer(indexBuffer);
    }
};

// ---------------------------------------------------------------------------
// CameraUBO — uniform buffer caméra (set 0, binding 0)
// ---------------------------------------------------------------------------
struct CameraUBO {
    float view[16];
    float proj[16];
    float viewPos[4];
};

// ---------------------------------------------------------------------------
// ObjectPushConstants — données par objet via push constants
// ---------------------------------------------------------------------------
struct ObjectPushConstants {
    float model[16]; // NkMat4x4
    float color[4];  // tint couleur
};

// ---------------------------------------------------------------------------
// NkMaterial — pipeline + descriptor set par matériau
// ---------------------------------------------------------------------------
struct NkMaterial {
    NkPipelineHandle         pipeline;
    NkDescriptorSetHandle    cameraSet;   // set 0 : camera UBO
    NkDescriptorSetHandle    materialSet; // set 1 : textures + material UBO
    NkBufferHandle           materialUBO; // roughness, metallic, etc.
    NkTextureHandle          albedoTex;
    NkTextureHandle          normalTex;
    NkSamplerHandle          sampler;
};

// ---------------------------------------------------------------------------
// NkRenderObject — un objet de scène renderable
// ---------------------------------------------------------------------------
struct NkRenderObject {
    NkMesh*     mesh     = nullptr;
    NkMaterial* material = nullptr;
    float       transform[16]{};  // NkMat4x4 model
    float       color[4]{ 1,1,1,1 };
    bool        visible = true;
};

// ---------------------------------------------------------------------------
// NkSceneRenderer — orchestre le rendu de N objets
// ---------------------------------------------------------------------------
class NkSceneRenderer {
public:
    static constexpr NkU32 MAX_FRAMES = 2;

    explicit NkSceneRenderer(NkRHIDevice& dev);
    ~NkSceneRenderer();

    // Initialisation : crée les ressources partagées (camera UBO, layouts)
    bool Init(NkU32 width, NkU32 height);
    void Shutdown();

    // Mise à jour de la caméra
    void SetCamera(const CameraUBO& cam);

    // Taille de swapchain changée
    void OnResize(NkU32 width, NkU32 height);

    // Ajout/suppression d'objets
    NkU32  AddObject   (NkRenderObject obj);
    void   RemoveObject(NkU32 id);
    NkRenderObject* GetObject(NkU32 id);

    // ── Rendu d'une frame ────────────────────────────────────────────────────
    // Appelé entre BeginFrame() et EndFrame() du device.
    void Render(NkRHICommandBuffer& cmd, NkU32 frameIndex);

    // ── Création de matériau standard ────────────────────────────────────────
    NkMaterial CreatePBRMaterial(const NkTextureHandle albedo,
                                  const NkTextureHandle normal,
                                  float roughness, float metallic);

    // ── Descriptor layouts partagés ──────────────────────────────────────────
    NkDescriptorLayoutHandle GetCameraLayout()   const { return mCameraLayout;   }
    NkDescriptorLayoutHandle GetMaterialLayout() const { return mMaterialLayout; }
    NkPipelineHandle         GetDefaultPipeline()const { return mDefaultPipeline;}

private:
    NkRHIDevice& mDevice;
    NkU32        mWidth = 0, mHeight = 0;

    // Layouts partagés par tous les matériaux
    NkDescriptorLayoutHandle mCameraLayout;
    NkDescriptorLayoutHandle mMaterialLayout;
    NkPipelineHandle         mDefaultPipeline;
    NkShaderHandle           mVertShader;
    NkShaderHandle           mFragShader;

    // Camera UBO (un par frame en vol pour éviter les data races)
    NkBufferHandle           mCameraUBO[MAX_FRAMES];
    NkDescriptorSetHandle    mCameraSet[MAX_FRAMES];
    CameraUBO                mCameraData{};

    // Objets de scène
    std::vector<NkRenderObject> mObjects;
    NkU32                       mNextId = 1;

    // Helpers
    NkShaderHandle LoadSPIRV(const char* path, NkShaderStage stage);
    void           BuildDefaultPipeline();
    void           CreateCameraResources();
};

}} // namespace nkentseu::rhi


// =============================================================================
// NkRenderer.cpp
// =============================================================================

// NOTE : Les shaders SPIR-V sont normalement chargés depuis des fichiers .spv.
// Ici on embarque des shaders minimalistes en bytes pour l'exemple.
// En production : utiliser glslc ou shaderc pour compiler .vert/.frag → .spv

#include "NkRenderer.h"
#include <cstring>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <vector>

namespace nkentseu { namespace rhi {

// SPIR-V minimal embarqué : vertex shader (position + normal + uv + push MVP)
// Généré depuis :
//   #version 450
//   layout(location=0) in vec3 inPos;
//   layout(location=1) in vec3 inNormal;
//   layout(location=2) in vec2 inUV;
//   layout(location=3) in vec4 inColor;
//   layout(set=0, binding=0) uniform Camera { mat4 view; mat4 proj; vec4 viewPos; };
//   layout(push_constant) uniform Push { mat4 model; vec4 color; };
//   layout(location=0) out vec3 outNormal;
//   layout(location=1) out vec2 outUV;
//   layout(location=2) out vec4 outColor;
//   layout(location=3) out vec3 outFragPos;
//   void main() {
//       vec4 worldPos = model * vec4(inPos, 1.0);
//       gl_Position   = proj * view * worldPos;
//       outNormal     = mat3(transpose(inverse(model))) * inNormal;
//       outUV         = inUV;
//       outColor      = inColor * color;
//       outFragPos    = worldPos.xyz;
//   }
// (binaire non inclus ici — charger depuis fichier .spv en production)

NkSceneRenderer::NkSceneRenderer(NkRHIDevice& dev) : mDevice(dev) {}

NkSceneRenderer::~NkSceneRenderer() { Shutdown(); }

NkShaderHandle NkSceneRenderer::LoadSPIRV(const char* path, NkShaderStage stage) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        logger.Errorf("[NkRenderer] Cannot open shader: %s", path);
        return NkShaderHandle::Null();
    }
    size_t size = (size_t)file.tellg();
    std::vector<char> code(size);
    file.seekg(0); file.read(code.data(), size); file.close();

    NkShaderDesc sd;
    sd.stage    = stage;
    sd.code     = code.data();
    sd.codeSize = (NkU32)size;
    sd.entry    = "main";
    return mDevice.CreateShader(sd);
}

bool NkSceneRenderer::Init(NkU32 width, NkU32 height) {
    mWidth = width; mHeight = height;

    // ── Descriptor layouts ───────────────────────────────────────────────────

    // Set 0 : Camera UBO
    NkDescriptorLayoutDesc camLayout;
    camLayout.bindingCount = 1;
    camLayout.bindings[0]  = { 0, NkDescriptorType::UniformBuffer,
                                1, NkShaderStage::Vertex | NkShaderStage::Fragment };
    mCameraLayout = mDevice.CreateDescriptorLayout(camLayout);

    // Set 1 : Albedo + Normal textures + Material UBO
    NkDescriptorLayoutDesc matLayout;
    matLayout.bindingCount = 3;
    matLayout.bindings[0]  = { 0, NkDescriptorType::UniformBuffer,       1, NkShaderStage::Fragment };
    matLayout.bindings[1]  = { 1, NkDescriptorType::CombinedImageSampler,1, NkShaderStage::Fragment };
    matLayout.bindings[2]  = { 2, NkDescriptorType::CombinedImageSampler,1, NkShaderStage::Fragment };
    mMaterialLayout = mDevice.CreateDescriptorLayout(matLayout);

    // ── Camera UBOs (un par frame) ───────────────────────────────────────────
    CreateCameraResources();

    // ── Pipeline par défaut ──────────────────────────────────────────────────
    mVertShader = LoadSPIRV("shaders/pbr.vert.spv", NkShaderStage::Vertex);
    mFragShader = LoadSPIRV("shaders/pbr.frag.spv", NkShaderStage::Fragment);

    if (mVertShader.IsValid() && mFragShader.IsValid())
        BuildDefaultPipeline();

    return true;
}

void NkSceneRenderer::CreateCameraResources() {
    NkBufferDesc uboDesc;
    uboDesc.size   = sizeof(CameraUBO);
    uboDesc.usage  = NkBufferUsage::Uniform;
    uboDesc.memory = NkMemoryType::CpuToGpu;

    for (NkU32 i = 0; i < MAX_FRAMES; ++i) {
        mCameraUBO[i] = mDevice.CreateBuffer(uboDesc);
        mCameraSet[i] = mDevice.AllocDescriptorSet(mCameraLayout);

        NkDescriptorWrite w;
        w.binding     = 0;
        w.type        = NkDescriptorType::UniformBuffer;
        w.buffer      = mCameraUBO[i];
        w.bufferOffset= 0;
        w.bufferRange = sizeof(CameraUBO);
        mDevice.UpdateDescriptorSet(mCameraSet[i], &w, 1);
    }
}

void NkSceneRenderer::BuildDefaultPipeline() {
    // Vertex attributes : pos(0), normal(1), uv(2), color(3)
    NkPipelineDesc pd;
    pd.type         = NkPipelineType::Graphics;
    pd.renderPass   = mDevice.GetSwapchainRenderPass();
    pd.shaders[0]   = mVertShader;
    pd.shaders[1]   = mFragShader;
    pd.shaderCount  = 2;

    pd.attributeCount = 4;
    pd.attributes[0]  = { 0, 0, offsetof(NkVertex, pos),    NkVertexFormat::Float3 };
    pd.attributes[1]  = { 1, 0, offsetof(NkVertex, normal), NkVertexFormat::Float3 };
    pd.attributes[2]  = { 2, 0, offsetof(NkVertex, uv),     NkVertexFormat::Float2 };
    pd.attributes[3]  = { 3, 0, offsetof(NkVertex, color),  NkVertexFormat::Float4 };

    pd.bindingCount = 1;
    pd.bindings[0]  = { 0, sizeof(NkVertex), false };

    pd.layoutCount  = 2;
    pd.layouts[0]   = mCameraLayout;
    pd.layouts[1]   = mMaterialLayout;

    pd.pushConstantSize   = sizeof(ObjectPushConstants);
    pd.pushConstantStages = NkShaderStage::Vertex | NkShaderStage::Fragment;

    pd.depthStencil.depthTest  = true;
    pd.depthStencil.depthWrite = true;
    pd.depthStencil.depthOp    = NkCompareOp::Less;

    pd.raster.cullMode  = NkCullMode::Back;
    pd.raster.frontFace = NkFrontFace::CCW;

    // Alpha blending désactivé pour le pass opaque
    pd.blends[0].enabled = false;

    mDefaultPipeline = mDevice.CreatePipeline(pd);
}

void NkSceneRenderer::SetCamera(const CameraUBO& cam) {
    mCameraData = cam;
}

void NkSceneRenderer::OnResize(NkU32 width, NkU32 height) {
    mWidth = width; mHeight = height;
}

NkU32 NkSceneRenderer::AddObject(NkRenderObject obj) {
    mObjects.push_back(obj);
    return mNextId++;
}

void NkSceneRenderer::RemoveObject(NkU32) {
    // TODO: gestion des IDs stables (pool ou map)
}

NkRenderObject* NkSceneRenderer::GetObject(NkU32 id) {
    if (id == 0 || id > mObjects.size()) return nullptr;
    return &mObjects[id - 1];
}

// =============================================================================
// Render — cœur du système
// Tri minimaliste : pas de sort ici (à ajouter : sort par matériau pour
// réduire les changements de pipeline, sort front-to-back pour le depth culling)
// =============================================================================

void NkSceneRenderer::Render(NkRHICommandBuffer& cmd, NkU32 frameIndex) {
    NkU32 fi = frameIndex % MAX_FRAMES;

    // 1. Mettre à jour le camera UBO
    void* mapped = mDevice.MapBuffer(mCameraUBO[fi]);
    if (mapped) {
        memcpy(mapped, &mCameraData, sizeof(CameraUBO));
        mDevice.UnmapBuffer(mCameraUBO[fi]);
    }

    // 2. Commencer le render pass principal (swapchain)
    NkClearValue clears[2];
    clears[0] = { 0.1f, 0.1f, 0.2f, 1.f };  // couleur fond
    clears[1].depth = 1.f; clears[1].stencil = 0;

    cmd.BeginRenderPass(mDevice.GetSwapchainRenderPass(),
                         mDevice.GetCurrentFramebuffer(),
                         clears, 2);

    // 3. Viewport + scissor dynamiques
    NkViewport vp{ 0.f, 0.f, (float)mWidth, (float)mHeight, 0.f, 1.f };
    NkScissor  sc{ 0, 0, mWidth, mHeight };
    cmd.SetViewport(vp);
    cmd.SetScissor(sc);

    // 4. Boucle de rendu des objets
    // ── Optimisation clé : trier par pipeline pour minimiser vkCmdBindPipeline ──
    // En production : std::sort(mObjects, [](auto& a, auto& b) {
    //     return a.material->pipeline.id < b.material->pipeline.id; })

    NkPipelineHandle lastPipeline{ 0 };

    for (auto& obj : mObjects) {
        if (!obj.visible || !obj.mesh || !obj.material) continue;

        // 4a. Changer de pipeline seulement si nécessaire
        if (obj.material->pipeline.id != lastPipeline.id) {
            cmd.BindPipeline(obj.material->pipeline);
            lastPipeline = obj.material->pipeline;
        }

        // 4b. Bind descriptor sets
        cmd.BindDescriptorSet(0, mCameraSet[fi],          obj.material->pipeline);
        cmd.BindDescriptorSet(1, obj.material->materialSet, obj.material->pipeline);

        // 4c. Push constants : model matrix + tint color
        ObjectPushConstants pc;
        memcpy(pc.model, obj.transform, sizeof(float) * 16);
        memcpy(pc.color, obj.color,     sizeof(float) * 4);
        cmd.PushConstants(obj.material->pipeline,
                          NkShaderStage::Vertex | NkShaderStage::Fragment,
                          0, sizeof(ObjectPushConstants), &pc);

        // 4d. Bind géométrie et dessiner
        cmd.BindVertexBuffer(0, obj.mesh->vertexBuffer, 0);
        cmd.BindIndexBuffer(obj.mesh->indexBuffer, obj.mesh->indexType, 0);
        cmd.DrawIndexed({ obj.mesh->indexCount, 1, 0, 0, 0 });
    }

    cmd.EndRenderPass();
}

NkMaterial NkSceneRenderer::CreatePBRMaterial(
    const NkTextureHandle albedo,
    const NkTextureHandle normal,
    float roughness, float metallic)
{
    NkMaterial mat;
    mat.pipeline   = mDefaultPipeline;
    mat.albedoTex  = albedo;
    mat.normalTex  = normal;

    // Sampler par défaut
    NkSamplerDesc sd;
    sd.minFilter = NkFilter::Linear; sd.magFilter = NkFilter::Linear;
    sd.wrapU = NkWrapMode::Repeat;   sd.wrapV = NkWrapMode::Repeat;
    mat.sampler = mDevice.CreateSampler(sd);

    // Material UBO
    struct MaterialData { float roughness, metallic, padding[2]; };
    MaterialData md{ roughness, metallic, 0, 0 };
    NkBufferDesc ubd;
    ubd.size     = sizeof(MaterialData);
    ubd.usage    = NkBufferUsage::Uniform;
    ubd.memory   = NkMemoryType::CpuToGpu;
    ubd.initData = &md;
    mat.materialUBO = mDevice.CreateBuffer(ubd);

    // Camera set (set 0) — partagé, alloué par le SceneRenderer
    mat.cameraSet = mCameraSet[0]; // sera surchargé au moment du bind

    // Material set (set 1)
    mat.materialSet = mDevice.AllocDescriptorSet(mMaterialLayout);

    NkDescriptorWrite writes[3];
    writes[0].binding = 0; writes[0].type = NkDescriptorType::UniformBuffer;
    writes[0].buffer  = mat.materialUBO;
    writes[0].bufferRange = sizeof(MaterialData);

    writes[1].binding = 1; writes[1].type = NkDescriptorType::CombinedImageSampler;
    writes[1].texture = albedo; writes[1].sampler = mat.sampler;

    writes[2].binding = 2; writes[2].type = NkDescriptorType::CombinedImageSampler;
    writes[2].texture = normal; writes[2].sampler = mat.sampler;

    mDevice.UpdateDescriptorSet(mat.materialSet, writes, 3);
    return mat;
}

void NkSceneRenderer::Shutdown() {
    if (!mDevice.GetWidth()) return; // déjà shutdown
    for (NkU32 i = 0; i < MAX_FRAMES; ++i) {
        mDevice.DestroyBuffer(mCameraUBO[i]);
        mDevice.FreeDescriptorSet(mCameraSet[i]);
    }
    if (mDefaultPipeline.IsValid()) mDevice.DestroyPipeline(mDefaultPipeline);
    if (mVertShader.IsValid())      mDevice.DestroyShader(mVertShader);
    if (mFragShader.IsValid())      mDevice.DestroyShader(mFragShader);
    mDevice.DestroyDescriptorLayout(mCameraLayout);
    mDevice.DestroyDescriptorLayout(mMaterialLayout);
}

}} // namespace nkentseu::rhi
