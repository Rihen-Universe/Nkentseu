#include "NkMaterial.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
namespace renderer {

uint64 NkMaterial::sIDCounter = 1;

namespace {
static NkDescriptorType kTexDescType = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
}

bool NkMaterial::Create(NkIDevice* device, NkShadingModel model, NkBlendMode blend, NkMaterialDomain domain) {
    Destroy();
    if (!device) {
        return false;
    }

    mDevice = device;
    mID.id = sIDCounter++;
    mShadingModel = model;
    mBlendMode = blend;
    mDomain = domain;
    mDirty = true;

    mUBO = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(NkMaterialGPUData)));
    if (!mUBO.IsValid()) {
        Destroy();
        return false;
    }

    CreateDescSetLayout();
    AllocDescSet();
    UpdateDescSet();
    return mDescSet.IsValid();
}

NkMaterial* NkMaterial::CreateInstance() const {
    return Clone();
}

void NkMaterial::Destroy() {
    if (!mDevice) {
        return;
    }

    for (auto& p : mPipelines) {
        if (p.pipe.IsValid()) {
            mDevice->DestroyPipeline(p.pipe);
        }
        if (p.shadowPipe.IsValid()) {
            mDevice->DestroyPipeline(p.shadowPipe);
        }
    }
    mPipelines.Clear();

    if (mDescSet.IsValid()) {
        mDevice->FreeDescriptorSet(mDescSet);
    }
    if (mDescSetLayout.IsValid()) {
        mDevice->DestroyDescriptorSetLayout(mDescSetLayout);
    }
    if (mUBO.IsValid()) {
        mDevice->DestroyBuffer(mUBO);
    }

    mDevice = nullptr;
    mDescSet = {};
    mDescSetLayout = {};
    mUBO = {};
}

NkMaterial& NkMaterial::SetRoughnessMap(NkTexture2D* tex) {
    mRoughnessMap = tex;
    mDirty = true;
    return *this;
}

NkMaterial& NkMaterial::SetMetallicMap(NkTexture2D* tex) {
    mMetallicMap = tex;
    mDirty = true;
    return *this;
}

NkMaterial& NkMaterial::SetAOMap(NkTexture2D* tex) {
    mAOMap = tex;
    mDirty = true;
    return *this;
}

NkMaterial& NkMaterial::SetAnisotropy(float anisotropy, float rotation) {
    mGPUData.specular[2] = anisotropy;
    mGPUData.specular[3] = rotation;
    mDirty = true;
    return *this;
}

NkMaterial& NkMaterial::SetToonBands(int bands, float smoothness) {
    mToonBands = NkClamp((float)bands, 1.f, 16.f);
    mToonSmooth = NkClamp(smoothness, 0.f, 1.f);
    mDirty = true;
    return *this;
}

NkMaterial& NkMaterial::SetToonRimLight(const NkColor4f& color, float width) {
    mToonRim = color;
    mToonRimWidth = NkClamp(width, 0.f, 1.f);
    mDirty = true;
    return *this;
}

NkMaterial& NkMaterial::SetToonOutline(const NkColor4f& color, float thickness) {
    mToonOutlineColor = color;
    mToonOutlineThick = NkClamp(thickness, 0.f, 0.1f);
    mDirty = true;
    return *this;
}

NkMaterial& NkMaterial::SetSubsurface(float v) {
    mGPUData.subsurface[3] = NkClamp(v, 0.f, 10.f);
    mDirty = true;
    return *this;
}

NkMaterial& NkMaterial::SetSpecularTint(float v) {
    mGPUData.specular[1] = NkClamp(v, 0.f, 1.f);
    mDirty = true;
    return *this;
}

NkMaterial& NkMaterial::SetSheenTint(float v) {
    mGPUData.sheen[3] = NkClamp(v, 0.f, 1.f);
    mDirty = true;
    return *this;
}

NkMaterial& NkMaterial::SetClearCoatGloss(float v) {
    mGPUData.clearCoat[1] = NkClamp(v, 0.f, 1.f);
    mDirty = true;
    return *this;
}

NkMaterial& NkMaterial::SetScalar(const char* name, float value) {
    if (!name) {
        return *this;
    }

    for (auto& s : mCustomScalars) {
        if (s.name == name) {
            s.value = value;
            s.isDirty = true;
            mDirty = true;
            return *this;
        }
    }

    NkMatScalar s;
    s.name = name;
    s.value = value;
    s.isDirty = true;
    mCustomScalars.PushBack(s);
    mDirty = true;
    return *this;
}

NkMaterial& NkMaterial::SetVector(const char* name, const NkVec4f& value) {
    if (!name) {
        return *this;
    }

    for (auto& v : mCustomVectors) {
        if (v.name == name) {
            v.value = value;
            v.isDirty = true;
            mDirty = true;
            return *this;
        }
    }

    NkMatVector v;
    v.name = name;
    v.value = value;
    v.isDirty = true;
    mCustomVectors.PushBack(v);
    mDirty = true;
    return *this;
}

float NkMaterial::GetScalar(const char* name, float def) const {
    if (!name) {
        return def;
    }
    for (const auto& s : mCustomScalars) {
        if (s.name == name) {
            return s.value;
        }
    }
    return def;
}

NkVec4f NkMaterial::GetVector(const char* name) const {
    if (!name) {
        return {0, 0, 0, 0};
    }
    for (const auto& v : mCustomVectors) {
        if (v.name == name) {
            return v.value;
        }
    }
    return {0, 0, 0, 0};
}

bool NkMaterial::FlushToGPU() {
    if (!mDevice || !mUBO.IsValid()) {
        return false;
    }
    if (!mDirty) {
        return true;
    }

    UpdateDescSet();
    if (!mDevice->WriteBuffer(mUBO, &mGPUData, sizeof(NkMaterialGPUData))) {
        return false;
    }
    mDirty = false;
    return true;
}

void NkMaterial::Bind(NkICommandBuffer* cmd, NkIDevice* device) {
    NkIDevice* dev = device ? device : mDevice;
    if (!cmd || !dev) {
        return;
    }

    FlushToGPU();
    if (mDescSet.IsValid()) {
        // Convention renderer: set 2 = matériau.
        cmd->BindDescriptorSet(mDescSet, 2);
    }
}

NkPipelineHandle NkMaterial::GetPipeline(NkRenderPassHandle rp, const NkVertexLayout& vtxLayout, bool shadowPass) {
    for (auto& p : mPipelines) {
        if (p.rp == rp) {
            NkPipelineHandle h = shadowPass ? p.shadowPipe : p.pipe;
            if (h.IsValid()) {
                return h;
            }
        }
    }

    if (!mDevice) {
        return {};
    }

    NkGraphicsPipelineDesc desc{};
    desc.renderPass = rp;
    desc.vertexLayout = vtxLayout;
    desc.topology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
    desc.blend = BuildBlendDesc();
    desc.rasterizer = BuildRasterizerDesc();
    desc.depthStencil = BuildDepthStencilDesc();
    desc.descriptorSetLayouts.PushBack(mDescSetLayout);

    NkPipelineHandle pipe = mDevice->CreateGraphicsPipeline(desc);

    PipelineEntry entry{};
    entry.rp = rp;
    entry.vtxLayout = vtxLayout;
    if (shadowPass) {
        entry.shadowPipe = pipe;
    } else {
        entry.pipe = pipe;
    }
    mPipelines.PushBack(entry);
    return pipe;
}

NkMaterial* NkMaterial::Clone(const char* newName) const {
    NkMaterial* mat = new NkMaterial();
    if (!mat->Create(mDevice, mShadingModel, mBlendMode, mDomain)) {
        delete mat;
        return nullptr;
    }

    mat->mGPUData = mGPUData;
    mat->mTwoSided = mTwoSided;
    mat->mAlbedoMap = mAlbedoMap;
    mat->mNormalMap = mNormalMap;
    mat->mORMMap = mORMMap;
    mat->mEmissiveMap = mEmissiveMap;
    mat->mHeightMap = mHeightMap;
    mat->mRoughnessMap = mRoughnessMap;
    mat->mMetallicMap = mMetallicMap;
    mat->mAOMap = mAOMap;
    for (uint32 i = 0; i < 8; ++i) {
        mat->mCustomTextures[i] = mCustomTextures[i];
    }
    mat->mCustomScalars = mCustomScalars;
    mat->mCustomVectors = mCustomVectors;
    mat->mToonBands = mToonBands;
    mat->mToonSmooth = mToonSmooth;
    mat->mToonRim = mToonRim;
    mat->mToonRimWidth = mToonRimWidth;
    mat->mToonOutlineColor = mToonOutlineColor;
    mat->mToonOutlineThick = mToonOutlineThick;
    mat->mWaterSpeed = mWaterSpeed;
    mat->mWaterFoam = mWaterFoam;
    mat->mName = newName ? NkString(newName) : mName;
    mat->mDirty = true;
    return mat;
}

bool NkMaterial::SaveToFile(const char* path) const {
    if (!path) {
        return false;
    }
    FILE* f = fopen(path, "wb");
    if (!f) {
        return false;
    }

    fprintf(f, "name=%s\n", mName.CStr());
    fprintf(f, "shading=%u\n", (uint32)mShadingModel);
    fprintf(f, "blend=%u\n", (uint32)mBlendMode);
    fprintf(f, "domain=%u\n", (uint32)mDomain);
    fprintf(f, "twoSided=%u\n", mTwoSided ? 1u : 0u);
    fprintf(f, "albedo=%f,%f,%f,%f\n", mGPUData.albedo[0], mGPUData.albedo[1], mGPUData.albedo[2], mGPUData.albedo[3]);
    fprintf(f, "pbr=%f,%f,%f,%f\n", mGPUData.pbrParams[0], mGPUData.pbrParams[1], mGPUData.pbrParams[2], mGPUData.pbrParams[3]);
    fprintf(f, "emissive=%f,%f,%f,%f\n", mGPUData.emissiveColor[0], mGPUData.emissiveColor[1], mGPUData.emissiveColor[2], mGPUData.emissiveColor[3]);
    fclose(f);
    return true;
}

bool NkMaterial::LoadFromFile(NkIDevice* device, const char* path) {
    if (!device || !path) {
        return false;
    }

    FILE* f = fopen(path, "rb");
    if (!f) {
        return false;
    }

    if (!mDevice && !Create(device)) {
        fclose(f);
        return false;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char key[64];
        if (sscanf(line, "%63[^=]=", key) != 1) {
            continue;
        }
        if (strcmp(key, "name") == 0) {
            char value[448];
            if (sscanf(line, "name=%447[^\n]", value) == 1) {
                mName = value;
            }
        } else if (strcmp(key, "shading") == 0) {
            uint32 v = 0;
            if (sscanf(line, "shading=%u", &v) == 1) {
                mShadingModel = (NkShadingModel)v;
            }
        } else if (strcmp(key, "blend") == 0) {
            uint32 v = 0;
            if (sscanf(line, "blend=%u", &v) == 1) {
                mBlendMode = (NkBlendMode)v;
            }
        } else if (strcmp(key, "domain") == 0) {
            uint32 v = 0;
            if (sscanf(line, "domain=%u", &v) == 1) {
                mDomain = (NkMaterialDomain)v;
            }
        } else if (strcmp(key, "twoSided") == 0) {
            uint32 v = 0;
            if (sscanf(line, "twoSided=%u", &v) == 1) {
                mTwoSided = (v != 0);
            }
        } else if (strcmp(key, "albedo") == 0) {
            sscanf(line, "albedo=%f,%f,%f,%f", &mGPUData.albedo[0], &mGPUData.albedo[1], &mGPUData.albedo[2], &mGPUData.albedo[3]);
        } else if (strcmp(key, "pbr") == 0) {
            sscanf(line, "pbr=%f,%f,%f,%f", &mGPUData.pbrParams[0], &mGPUData.pbrParams[1], &mGPUData.pbrParams[2], &mGPUData.pbrParams[3]);
        } else if (strcmp(key, "emissive") == 0) {
            sscanf(line, "emissive=%f,%f,%f,%f", &mGPUData.emissiveColor[0], &mGPUData.emissiveColor[1], &mGPUData.emissiveColor[2], &mGPUData.emissiveColor[3]);
        }
    }
    fclose(f);

    mDirty = true;
    return FlushToGPU();
}

NkShaderVariantKey NkMaterial::BuildVariantKey() const {
    NkShaderVariantKey key;
    if (mNormalMap) {
        key.Set("USE_NORMAL_MAP");
    }
    if (mHeightMap) {
        key.Set("USE_PARALLAX_MAP");
    }
    if (mGPUData.flags[3] > 0.5f) {
        key.Set("USE_EMISSIVE_MAP");
    }
    key.ComputeHash();
    return key;
}

NkDescriptorSetLayoutDesc NkMaterial::BuildLayoutDesc() const {
    NkDescriptorSetLayoutDesc d;
    d.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_ALL_GRAPHICS);
    for (uint32 b = 1; b <= 13; ++b) {
        d.Add(b, kTexDescType, NkShaderStage::NK_FRAGMENT);
    }
    return d;
}

void NkMaterial::CreateDescSetLayout() {
    if (!mDevice) {
        return;
    }
    if (mDescSetLayout.IsValid()) {
        mDevice->DestroyDescriptorSetLayout(mDescSetLayout);
    }
    mDescSetLayout = mDevice->CreateDescriptorSetLayout(BuildLayoutDesc());
}

void NkMaterial::AllocDescSet() {
    if (!mDevice || !mDescSetLayout.IsValid()) {
        return;
    }
    if (mDescSet.IsValid()) {
        mDevice->FreeDescriptorSet(mDescSet);
    }
    mDescSet = mDevice->AllocateDescriptorSet(mDescSetLayout);
}

void NkMaterial::UpdateDescSet() {
    if (!mDevice || !mDescSet.IsValid()) {
        return;
    }

    NkVector<NkDescriptorWrite> writes;
    writes.Reserve(20);

    NkDescriptorWrite w0{};
    w0.set = mDescSet;
    w0.binding = 0;
    w0.type = NkDescriptorType::NK_UNIFORM_BUFFER;
    w0.buffer = mUBO;
    w0.bufferRange = sizeof(NkMaterialGPUData);
    writes.PushBack(w0);

    auto pushTex = [&](uint32 binding, NkTexture2D* tex) {
        if (!tex || !tex->IsValid()) {
            return;
        }
        NkDescriptorWrite w{};
        w.set = mDescSet;
        w.binding = binding;
        w.type = kTexDescType;
        w.texture = tex->GetHandle();
        w.sampler = tex->GetSampler();
        writes.PushBack(w);
    };

    pushTex(1, mAlbedoMap);
    pushTex(2, mNormalMap);
    pushTex(3, mORMMap);
    pushTex(4, mEmissiveMap);
    pushTex(5, mHeightMap);
    pushTex(6, mRoughnessMap);
    pushTex(7, mMetallicMap);
    pushTex(8, mAOMap);
    for (uint32 i = 0; i < 8; ++i) {
        pushTex(9 + i, mCustomTextures[i]);
    }

    if (!writes.Empty()) {
        mDevice->UpdateDescriptorSets(writes.Data(), (uint32)writes.Size());
    }
}

NkBlendDesc NkMaterial::BuildBlendDesc() const {
    switch (mBlendMode) {
        case NkBlendMode::NK_TRANSLUCENT:
        case NkBlendMode::NK_PREMULTIPLIED:
            return NkBlendDesc::Alpha();
        case NkBlendMode::NK_ADDITIVE:
            return NkBlendDesc::Additive();
        default:
            return NkBlendDesc::Opaque();
    }
}

NkRasterizerDesc NkMaterial::BuildRasterizerDesc() const {
    NkRasterizerDesc r = NkRasterizerDesc::Default();
    if (mTwoSided) {
        r.cullMode = NkCullMode::NK_NONE;
    }
    return r;
}

NkDepthStencilDesc NkMaterial::BuildDepthStencilDesc() const {
    if (mBlendMode == NkBlendMode::NK_TRANSLUCENT || mBlendMode == NkBlendMode::NK_ADDITIVE) {
        return NkDepthStencilDesc::ReadOnly();
    }
    return NkDepthStencilDesc::Default();
}

NkMaterialLibrary& NkMaterialLibrary::Get() {
    static NkMaterialLibrary s;
    return s;
}

void NkMaterialLibrary::Init(NkIDevice* device) {
    mDevice = device;
    CreateBuiltins();
}

void NkMaterialLibrary::Shutdown() {
    for (auto* m : mAll) {
        delete m;
    }
    mAll.Clear();
    mByName.Clear();
    mByID.Clear();
    mDefaultPBR = nullptr;
    mDefaultUnlit = nullptr;
    mDefaultWire = nullptr;
    mDevice = nullptr;
}

void NkMaterialLibrary::Register(NkMaterial* mat) {
    if (!mat) {
        return;
    }
    mByName[mat->GetName()] = mat;
    mByID[mat->GetID().id] = mat;
    mAll.PushBack(mat);
}

NkMaterial* NkMaterialLibrary::Load(const char* path) {
    if (!mDevice || !path) {
        return nullptr;
    }

    NkMaterial* m = new NkMaterial();
    if (!m->Create(mDevice) || !m->LoadFromFile(mDevice, path)) {
        delete m;
        return nullptr;
    }
    Register(m);
    return m;
}

void NkMaterialLibrary::LoadDirectory(const char* dir) {
    (void)dir;
}

NkMaterial* NkMaterialLibrary::Find(const NkString& name) const {
    auto* p = mByName.Find(name);
    return p ? *p : nullptr;
}

NkMaterial* NkMaterialLibrary::Find(NkMaterialID id) const {
    auto* p = mByID.Find(id.id);
    return p ? *p : nullptr;
}

void NkMaterialLibrary::CreateBuiltins() {
    if (!mDevice) {
        return;
    }

    if (!mDefaultPBR) {
        mDefaultPBR = new NkMaterial();
        mDefaultPBR->Create(mDevice, NkShadingModel::NK_DEFAULT_LIT, NkBlendMode::NK_OPAQUE);
        mDefaultPBR->SetName("DefaultPBR");
        mDefaultPBR->FlushToGPU();
        Register(mDefaultPBR);
    }

    if (!mDefaultUnlit) {
        mDefaultUnlit = new NkMaterial();
        mDefaultUnlit->Create(mDevice, NkShadingModel::NK_UNLIT, NkBlendMode::NK_OPAQUE);
        mDefaultUnlit->SetName("DefaultUnlit");
        mDefaultUnlit->FlushToGPU();
        Register(mDefaultUnlit);
    }

    if (!mDefaultWire) {
        mDefaultWire = new NkMaterial();
        mDefaultWire->Create(mDevice, NkShadingModel::NK_UNLIT, NkBlendMode::NK_OPAQUE);
        mDefaultWire->SetName("DefaultWireframe");
        mDefaultWire->SetTwoSided(true);
        mDefaultWire->FlushToGPU();
        Register(mDefaultWire);
    }
}

NkMaterial* NkMaterialLibrary::GetDefaultPBR() {
    if (!mDefaultPBR) {
        CreateBuiltins();
    }
    return mDefaultPBR;
}

NkMaterial* NkMaterialLibrary::GetDefaultUnlit() {
    if (!mDefaultUnlit) {
        CreateBuiltins();
    }
    return mDefaultUnlit;
}

NkMaterial* NkMaterialLibrary::GetDefaultWireframe() {
    if (!mDefaultWire) {
        CreateBuiltins();
    }
    return mDefaultWire;
}

NkMaterial* NkMaterialLibrary::GetDefaultToon() {
    NkMaterial* m = new NkMaterial();
    if (!m->Create(mDevice, NkShadingModel::NK_TOON, NkBlendMode::NK_OPAQUE)) {
        delete m;
        return nullptr;
    }
    m->SetName("DefaultToon");
    m->SetToonBands(4, 0.02f);
    m->FlushToGPU();
    Register(m);
    return m;
}

NkMaterial* NkMaterialLibrary::GetDefaultGlass() {
    NkMaterial* m = new NkMaterial();
    if (!m->Create(mDevice, NkShadingModel::NK_GLASS_BSDF, NkBlendMode::NK_TRANSLUCENT)) {
        delete m;
        return nullptr;
    }
    m->SetName("DefaultGlass");
    m->SetTransmission(1.f, 1.52f, 0.f);
    m->SetRoughness(0.02f);
    m->SetTwoSided(true);
    m->FlushToGPU();
    Register(m);
    return m;
}

NkMaterial* NkMaterialLibrary::GetDefaultWater() {
    NkMaterial* m = new NkMaterial();
    if (!m->Create(mDevice, NkShadingModel::NK_SINGLE_LAYER_WATER, NkBlendMode::NK_TRANSLUCENT)) {
        delete m;
        return nullptr;
    }
    m->SetName("DefaultWater");
    m->SetTransmission(0.9f, 1.33f, 0.f);
    m->SetRoughness(0.05f);
    m->FlushToGPU();
    Register(m);
    return m;
}

NkMaterial* NkMaterialLibrary::GetDefaultHair() {
    NkMaterial* m = new NkMaterial();
    if (!m->Create(mDevice, NkShadingModel::NK_HAIR, NkBlendMode::NK_MASKED)) {
        delete m;
        return nullptr;
    }
    m->SetName("DefaultHair");
    m->SetTwoSided(true);
    m->SetAlphaCutoff(0.5f);
    m->FlushToGPU();
    Register(m);
    return m;
}

NkMaterial* NkMaterialLibrary::GetDefaultCloth() {
    NkMaterial* m = new NkMaterial();
    if (!m->Create(mDevice, NkShadingModel::NK_CLOTH, NkBlendMode::NK_OPAQUE)) {
        delete m;
        return nullptr;
    }
    m->SetName("DefaultCloth");
    m->SetRoughness(0.9f);
    m->FlushToGPU();
    Register(m);
    return m;
}

NkMaterial* NkMaterialLibrary::GetDefaultSkin() {
    NkMaterial* m = new NkMaterial();
    if (!m->Create(mDevice, NkShadingModel::NK_SUBSURFACE, NkBlendMode::NK_OPAQUE)) {
        delete m;
        return nullptr;
    }
    m->SetName("DefaultSkin");
    m->SetSubsurfaceColor({1.f, 0.3f, 0.2f, 1.f});
    m->SetSubsurfaceRadius(0.5f);
    m->FlushToGPU();
    Register(m);
    return m;
}

NkMaterial* NkMaterialLibrary::GetDebugNormals() { return GetDefaultUnlit(); }
NkMaterial* NkMaterialLibrary::GetDebugUV() { return GetDefaultUnlit(); }
NkMaterial* NkMaterialLibrary::GetDebugVertexColor() { return GetDefaultUnlit(); }
NkMaterial* NkMaterialLibrary::GetDebugDepth() { return GetDefaultUnlit(); }
NkMaterial* NkMaterialLibrary::GetDebugAO() { return GetDefaultUnlit(); }
NkMaterial* NkMaterialLibrary::GetDebugRoughness() { return GetDefaultUnlit(); }
NkMaterial* NkMaterialLibrary::GetDebugMetallic() { return GetDefaultUnlit(); }

void NkMaterialLibrary::Update() {
    for (auto* m : mAll) {
        if (m) {
            m->FlushToGPU();
        }
    }
}

} // namespace renderer
} // namespace nkentseu
