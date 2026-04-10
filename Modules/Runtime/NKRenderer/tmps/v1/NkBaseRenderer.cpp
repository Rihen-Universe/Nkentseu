// =============================================================================
// NkBaseRenderer.cpp
// =============================================================================
#include "NkBaseRenderer.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKLogger/NkLog.h"
#include "NKImage/Core/NkImage.h"
#include <cmath>

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // NkCameraEntry::Recompute
    // =========================================================================
    void NkCameraEntry::Recompute() {
        if (is2D) {
            const auto& d = desc2D;
            const float32 w = (float32)d.viewW;
            const float32 h = (float32)d.viewH;
            const float32 hw = w * 0.5f / d.zoom;
            const float32 hh = h * 0.5f / d.zoom;
            proj = NkMat4f::Orthogonal(
                NkVec2f(-hw, -hh), NkVec2f(hw, hh),
                d.nearPlane, d.farPlane, false);
            view = NkMat4f::Translation(NkVec3f(-d.position.x, -d.position.y, 0.f))
                 * NkMat4f::RotationZ(NkAngle(-d.rotation));
        } else {
            const auto& d = desc3D;
            if (d.projection == NkProjectionType::Perspective) {
                proj = NkMat4f::Perspective(NkAngle(d.fovY), d.aspect,
                                             d.nearPlane, d.farPlane);
            } else {
                const float32 h = 5.f, w = h * d.aspect;
                proj = NkMat4f::Orthogonal(
                    NkVec2f(-w, -h), NkVec2f(w, h),
                    d.nearPlane, d.farPlane, d.reverseZ);
            }
            view = NkMat4f::LookAt(d.position, d.target, d.up);
        }
        viewProj = proj * view;
    }

    // =========================================================================
    // Construction / destruction
    // =========================================================================
    NkBaseRenderer::NkBaseRenderer(NkIDevice* device)
        : mDevice(device)
    {}

    NkBaseRenderer::~NkBaseRenderer() {
        if (mValid) Shutdown();
    }

    bool NkBaseRenderer::Initialize() {
        if (!mDevice || !mDevice->IsValid()) {
            logger_src.Infof("[NKRenderer] NkBaseRenderer::Initialize — device invalide\n");
            return false;
        }
        mValid = true;
        logger_src.Infof("[NKRenderer] Renderer initialisé pour API: %s\n",
                          NkGraphicsApiName(mDevice->GetApi()));
        return true;
    }

    void NkBaseRenderer::Shutdown() {
        if (!mValid) return;
        mDevice->WaitIdle();

        // Détruire toutes les ressources dans l'ordre inverse de dépendance
        { threading::NkScopedLock lk(mFontMutex);
          mFonts.ForEach([this](uint64, NkFontEntry& e){ if(e.valid) DestroyFont_Impl(e); }); mFonts.Clear(); }

        { threading::NkScopedLock lk(mRTMutex);
          mRenderTargets.ForEach([this](uint64, NkRenderTargetEntry& e){ if(e.valid) DestroyRenderTarget_Impl(e); }); mRenderTargets.Clear(); }

        { threading::NkScopedLock lk(mMatMutex);
          mMaterialInsts.ForEach([this](uint64, NkMaterialInstEntry& e){ if(e.valid) DestroyMatInst_Impl(e); }); mMaterialInsts.Clear();
          mMaterials.ForEach([this](uint64, NkMaterialEntry& e){ if(e.valid) DestroyMaterial_Impl(e); }); mMaterials.Clear(); }

        { threading::NkScopedLock lk(mMeshMutex);
          mMeshes.ForEach([this](uint64, NkMeshEntry& e){ if(e.valid) DestroyMesh_Impl(e); }); mMeshes.Clear(); }

        { threading::NkScopedLock lk(mTextureMutex);
          mTextures.ForEach([this](uint64, NkTextureEntry& e){ if(e.valid) DestroyTexture_Impl(e); }); mTextures.Clear(); }

        { threading::NkScopedLock lk(mShaderMutex);
          mShaders.ForEach([this](uint64, NkShaderEntry& e){ if(e.valid) DestroyShader_Impl(e); }); mShaders.Clear(); }

        mValid = false;
        logger_src.Infof("[NKRenderer] Renderer arrêté.\n");
    }

    // =========================================================================
    // Helpers accès registres
    // =========================================================================
    NkShaderEntry*       NkBaseRenderer::GetShader      (NkShaderHandle h)       { return mShaders.Find(h.id); }
    NkTextureEntry*      NkBaseRenderer::GetTexture     (NkTextureHandle h)      { return mTextures.Find(h.id); }
    NkMeshEntry*         NkBaseRenderer::GetMesh        (NkMeshHandle h)         { return mMeshes.Find(h.id); }
    NkMaterialEntry*     NkBaseRenderer::GetMaterial    (NkMaterialHandle h)     { return mMaterials.Find(h.id); }
    NkMaterialInstEntry* NkBaseRenderer::GetMaterialInst(NkMaterialInstHandle h) { return mMaterialInsts.Find(h.id); }
    NkCameraEntry*       NkBaseRenderer::GetCamera      (NkCameraHandle h)       { return mCameras.Find(h.id); }
    NkRenderTargetEntry* NkBaseRenderer::GetRenderTarget(NkRenderTargetHandle h) { return mRenderTargets.Find(h.id); }
    NkFontEntry*         NkBaseRenderer::GetFont        (NkFontHandle h)         { return mFonts.Find(h.id); }
    NkBaseRenderer::UniformBlockEntry* NkBaseRenderer::GetUniformBlock(NkUniformBlockHandle h) { return mUniformBlocks.Find(h.id); }

    const NkShaderEntry*       NkBaseRenderer::GetShader      (NkShaderHandle h) const       { return mShaders.Find(h.id); }
    const NkTextureEntry*      NkBaseRenderer::GetTexture     (NkTextureHandle h) const      { return mTextures.Find(h.id); }
    const NkMeshEntry*         NkBaseRenderer::GetMesh        (NkMeshHandle h) const         { return mMeshes.Find(h.id); }
    const NkMaterialEntry*     NkBaseRenderer::GetMaterial    (NkMaterialHandle h) const     { return mMaterials.Find(h.id); }
    const NkMaterialInstEntry* NkBaseRenderer::GetMaterialInst(NkMaterialInstHandle h) const { return mMaterialInsts.Find(h.id); }
    const NkCameraEntry*       NkBaseRenderer::GetCamera      (NkCameraHandle h) const       { return mCameras.Find(h.id); }
    const NkRenderTargetEntry* NkBaseRenderer::GetRenderTarget(NkRenderTargetHandle h) const { return mRenderTargets.Find(h.id); }
    const NkFontEntry*         NkBaseRenderer::GetFont        (NkFontHandle h) const         { return mFonts.Find(h.id); }

    // =========================================================================
    // Shader backend selection
    // =========================================================================
    NkShaderBackend NkBaseRenderer::CurrentBackend() const {
        switch (mDevice->GetApi()) {
            case NkGraphicsApi::NK_API_VULKAN:    return NkShaderBackend::Vulkan;
            case NkGraphicsApi::NK_API_DIRECTX11: return NkShaderBackend::DX11;
            case NkGraphicsApi::NK_API_DIRECTX12: return NkShaderBackend::DX12;
            default:                              return NkShaderBackend::OpenGL;
        }
    }

    const NkShaderStageDesc* NkBaseRenderer::SelectStage(const NkShaderDesc& desc,
                                                          NkShaderStageType stage) const
    {
        const NkShaderBackend target = CurrentBackend();
        // Premier passage : correspondance exacte backend + stage
        for (usize i = 0; i < desc.stages.Size(); ++i) {
            if (desc.stages[i].stage == stage && desc.stages[i].backend == target)
                return &desc.stages[i];
        }
        // Second passage : fallback OpenGL si pas de variante pour ce backend
        if (target != NkShaderBackend::OpenGL) {
            for (usize i = 0; i < desc.stages.Size(); ++i) {
                if (desc.stages[i].stage == stage && desc.stages[i].backend == NkShaderBackend::OpenGL)
                    return &desc.stages[i];
            }
        }
        return nullptr;
    }

    // =========================================================================
    // Shader
    // =========================================================================
    NkShaderHandle NkBaseRenderer::CreateShader(const NkShaderDesc& desc) {
        threading::NkScopedLock lk(mShaderMutex);
        const uint64 id = NextId();
        NkShaderEntry& e = mShaders[id];
        e.desc = desc;
        if (!CreateShader_Impl(e)) {
            mShaders.Erase(id);
            logger_src.Infof("[NKRenderer] CreateShader FAILED: %s\n",
                              desc.debugName.CStr());
            return NkShaderHandle::Null();
        }
        e.valid = true;
        return NkShaderHandle{id};
    }

    void NkBaseRenderer::DestroyShader(NkShaderHandle& h) {
        if (!h.IsValid()) return;
        threading::NkScopedLock lk(mShaderMutex);
        NkShaderEntry* e = mShaders.Find(h.id);
        if (e && e->valid) { DestroyShader_Impl(*e); mShaders.Erase(h.id); }
        h = NkShaderHandle::Null();
    }

    bool NkBaseRenderer::ReloadShader(NkShaderHandle h) {
        if (!h.IsValid()) return false;
        threading::NkScopedLock lk(mShaderMutex);
        NkShaderEntry* e = mShaders.Find(h.id);
        if (!e || !e->valid) return false;
        DestroyShader_Impl(*e);
        return CreateShader_Impl(*e);
    }

    // =========================================================================
    // Texture
    // =========================================================================
    NkTextureHandle NkBaseRenderer::CreateTexture(const NkTextureDesc& desc) {
        threading::NkScopedLock lk(mTextureMutex);
        const uint64 id = NextId();
        NkTextureEntry& e = mTextures[id];
        e.desc = desc;
        if (!CreateTexture_Impl(e)) {
            mTextures.Erase(id);
            logger_src.Infof("[NKRenderer] CreateTexture FAILED: %s\n",
                              desc.debugName ? desc.debugName : "?");
            return NkTextureHandle::Null();
        }
        e.valid = true;
        return NkTextureHandle{id};
    }

    void NkBaseRenderer::DestroyTexture(NkTextureHandle& h) {
        if (!h.IsValid()) return;
        threading::NkScopedLock lk(mTextureMutex);
        NkTextureEntry* e = mTextures.Find(h.id);
        if (e && e->valid) { DestroyTexture_Impl(*e); mTextures.Erase(h.id); }
        h = NkTextureHandle::Null();
    }

    bool NkBaseRenderer::UpdateTexture(NkTextureHandle tex, const void* data,
                                       uint32 rowPitch, uint32 mip, uint32 layer) {
        threading::NkScopedLock lk(mTextureMutex);
        NkTextureEntry* e = mTextures.Find(tex.id);
        if (!e || !e->valid) return false;
        return UpdateTexture_Impl(*e, data, rowPitch, mip, layer);
    }

    bool NkBaseRenderer::GenerateMipmaps(NkTextureHandle tex) {
        threading::NkScopedLock lk(mTextureMutex);
        NkTextureEntry* e = mTextures.Find(tex.id);
        if (!e || !e->valid) return false;
        nkentseu::NkTextureHandle rhiH; rhiH.id = e->rhiTextureId;
        return mDevice->GenerateMipmaps(rhiH);
    }

    // =========================================================================
    // Mesh
    // =========================================================================
    NkMeshHandle NkBaseRenderer::CreateMesh(const NkMeshDesc& desc) {
        threading::NkScopedLock lk(mMeshMutex);
        const uint64 id = NextId();
        NkMeshEntry& e = mMeshes[id];
        e.desc        = desc;
        e.vertexCount = desc.vertexCount;
        e.indexCount  = desc.indexCount;
        if (!CreateMesh_Impl(e)) {
            mMeshes.Erase(id);
            logger_src.Infof("[NKRenderer] CreateMesh FAILED: %s\n",
                              desc.debugName ? desc.debugName : "?");
            return NkMeshHandle::Null();
        }
        e.valid = true;
        return NkMeshHandle{id};
    }

    void NkBaseRenderer::DestroyMesh(NkMeshHandle& h) {
        if (!h.IsValid()) return;
        threading::NkScopedLock lk(mMeshMutex);
        NkMeshEntry* e = mMeshes.Find(h.id);
        if (e && e->valid) { DestroyMesh_Impl(*e); mMeshes.Erase(h.id); }
        h = NkMeshHandle::Null();
    }

    bool NkBaseRenderer::UpdateMesh(NkMeshHandle mesh,
                                    const void* verts, uint32 vCount,
                                    const void* idxs,  uint32 iCount)
    {
        threading::NkScopedLock lk(mMeshMutex);
        NkMeshEntry* e = mMeshes.Find(mesh.id);
        if (!e || !e->valid) return false;
        e->vertexCount = vCount;
        e->indexCount  = iCount;
        return UpdateMesh_Impl(*e, verts, vCount, idxs, iCount);
    }

    // =========================================================================
    // Material
    // =========================================================================
    NkMaterialHandle NkBaseRenderer::CreateMaterialTemplate(const NkMaterialTemplateDesc& desc) {
        threading::NkScopedLock lk(mMatMutex);
        const uint64 id = NextId();
        NkMaterialEntry& e = mMaterials[id];
        e.desc   = desc;
        e.shader = desc.shader;
        if (!CreateMaterial_Impl(e)) {
            mMaterials.Erase(id);
            logger_src.Infof("[NKRenderer] CreateMaterialTemplate FAILED: %s\n",
                              desc.debugName ? desc.debugName : "?");
            return NkMaterialHandle::Null();
        }
        e.valid = true;
        return NkMaterialHandle{id};
    }

    void NkBaseRenderer::DestroyMaterialTemplate(NkMaterialHandle& h) {
        if (!h.IsValid()) return;
        threading::NkScopedLock lk(mMatMutex);
        NkMaterialEntry* e = mMaterials.Find(h.id);
        if (e && e->valid) { DestroyMaterial_Impl(*e); mMaterials.Erase(h.id); }
        h = NkMaterialHandle::Null();
    }

    NkMaterialInstHandle NkBaseRenderer::CreateMaterialInstance(const NkMaterialInstanceDesc& desc) {
        threading::NkScopedLock lk(mMatMutex);
        const NkMaterialEntry* tmpl = mMaterials.Find(desc.templateHandle.id);
        if (!tmpl || !tmpl->valid) {
            logger_src.Infof("[NKRenderer] CreateMaterialInstance — template invalide\n");
            return NkMaterialInstHandle::Null();
        }
        const uint64 id = NextId();
        NkMaterialInstEntry& e = mMaterialInsts[id];
        e.templateHandle = desc.templateHandle;
        // Copier les paramètres du template puis appliquer les overrides
        e.params = tmpl->desc.params;
        for (const auto& ov : desc.overrides) {
            for (usize i = 0; i < e.params.Size(); ++i) {
                if (e.params[i].name == ov.name) {
                    e.params[i] = ov;
                    break;
                }
            }
        }
        if (!CreateMatInst_Impl(e, *tmpl)) {
            mMaterialInsts.Erase(id);
            logger_src.Infof("[NKRenderer] CreateMaterialInstance FAILED: %s\n",
                              desc.debugName ? desc.debugName : "?");
            return NkMaterialInstHandle::Null();
        }
        e.valid = true;
        e.dirty = false;
        return NkMaterialInstHandle{id};
    }

    void NkBaseRenderer::DestroyMaterialInstance(NkMaterialInstHandle& h) {
        if (!h.IsValid()) return;
        threading::NkScopedLock lk(mMatMutex);
        NkMaterialInstEntry* e = mMaterialInsts.Find(h.id);
        if (e && e->valid) { DestroyMatInst_Impl(*e); mMaterialInsts.Erase(h.id); }
        h = NkMaterialInstHandle::Null();
    }

    bool NkBaseRenderer::SetMaterialParam(NkMaterialInstHandle mat, const NkMaterialParam& param) {
        threading::NkScopedLock lk(mMatMutex);
        NkMaterialInstEntry* e = mMaterialInsts.Find(mat.id);
        if (!e || !e->valid) return false;
        for (usize i = 0; i < e->params.Size(); ++i) {
            if (e->params[i].name == param.name) {
                e->params[i] = param;
                e->dirty = true;
                return true;
            }
        }
        e->params.PushBack(param);
        e->dirty = true;
        return true;
    }

    bool NkBaseRenderer::SetMaterialFloat(NkMaterialInstHandle mat, const char* name, float32 v) {
        NkMaterialParam p; p.type=NkMaterialParam::Type::Float; p.f=v; p.name=name;
        return SetMaterialParam(mat, p);
    }
    bool NkBaseRenderer::SetMaterialVec2(NkMaterialInstHandle mat, const char* name, NkVec2f v) {
        NkMaterialParam p; p.type=NkMaterialParam::Type::Vec2; p.v2[0]=v.x; p.v2[1]=v.y; p.name=name;
        return SetMaterialParam(mat, p);
    }
    bool NkBaseRenderer::SetMaterialVec3(NkMaterialInstHandle mat, const char* name, NkVec3f v) {
        NkMaterialParam p; p.type=NkMaterialParam::Type::Vec3; p.v3[0]=v.x; p.v3[1]=v.y; p.v3[2]=v.z; p.name=name;
        return SetMaterialParam(mat, p);
    }
    bool NkBaseRenderer::SetMaterialVec4(NkMaterialInstHandle mat, const char* name, NkVec4f v) {
        NkMaterialParam p; p.type=NkMaterialParam::Type::Vec4; p.v4[0]=v.x; p.v4[1]=v.y; p.v4[2]=v.z; p.v4[3]=v.w; p.name=name;
        return SetMaterialParam(mat, p);
    }
    bool NkBaseRenderer::SetMaterialInt(NkMaterialInstHandle mat, const char* name, int32 v) {
        NkMaterialParam p; p.type=NkMaterialParam::Type::Int; p.i=v; p.name=name;
        return SetMaterialParam(mat, p);
    }
    bool NkBaseRenderer::SetMaterialBool(NkMaterialInstHandle mat, const char* name, bool v) {
        NkMaterialParam p; p.type=NkMaterialParam::Type::Bool; p.b=v; p.name=name;
        return SetMaterialParam(mat, p);
    }
    bool NkBaseRenderer::SetMaterialTexture(NkMaterialInstHandle mat, const char* name, NkTextureHandle tex) {
        NkMaterialParam p; p.type=NkMaterialParam::Type::Texture; p.texture=tex; p.name=name;
        return SetMaterialParam(mat, p);
    }

    // =========================================================================
    // Camera
    // =========================================================================
    NkCameraHandle NkBaseRenderer::CreateCamera2D(const NkCameraDesc2D& desc) {
        threading::NkScopedLock lk(mCamMutex);
        const uint64 id = NextId();
        NkCameraEntry& e = mCameras[id];
        e.is2D   = true;
        e.desc2D = desc;
        e.valid  = true;
        e.Recompute();
        return NkCameraHandle{id};
    }

    NkCameraHandle NkBaseRenderer::CreateCamera3D(const NkCameraDesc3D& desc) {
        threading::NkScopedLock lk(mCamMutex);
        const uint64 id = NextId();
        NkCameraEntry& e = mCameras[id];
        e.is2D   = false;
        e.desc3D = desc;
        e.valid  = true;
        e.Recompute();
        return NkCameraHandle{id};
    }

    void NkBaseRenderer::DestroyCamera(NkCameraHandle& h) {
        if (!h.IsValid()) return;
        threading::NkScopedLock lk(mCamMutex);
        mCameras.Erase(h.id);
        h = NkCameraHandle::Null();
    }

    bool NkBaseRenderer::UpdateCamera2D(NkCameraHandle cam, const NkCameraDesc2D& desc) {
        threading::NkScopedLock lk(mCamMutex);
        NkCameraEntry* e = mCameras.Find(cam.id);
        if (!e) return false;
        e->desc2D = desc;
        e->Recompute();
        return true;
    }

    bool NkBaseRenderer::UpdateCamera3D(NkCameraHandle cam, const NkCameraDesc3D& desc) {
        threading::NkScopedLock lk(mCamMutex);
        NkCameraEntry* e = mCameras.Find(cam.id);
        if (!e) return false;
        e->desc3D = desc;
        e->Recompute();
        return true;
    }

    NkMat4f NkBaseRenderer::GetViewMatrix(NkCameraHandle cam) const {
        threading::NkScopedLock lk(mCamMutex);
        const NkCameraEntry* e = mCameras.Find(cam.id);
        return e ? e->view : NkMat4f::Identity();
    }

    NkMat4f NkBaseRenderer::GetProjectionMatrix(NkCameraHandle cam) const {
        threading::NkScopedLock lk(mCamMutex);
        const NkCameraEntry* e = mCameras.Find(cam.id);
        return e ? e->proj : NkMat4f::Identity();
    }

    NkMat4f NkBaseRenderer::GetViewProjectionMatrix(NkCameraHandle cam) const {
        threading::NkScopedLock lk(mCamMutex);
        const NkCameraEntry* e = mCameras.Find(cam.id);
        return e ? e->viewProj : NkMat4f::Identity();
    }

    // =========================================================================
    // Render target
    // =========================================================================
    NkRenderTargetHandle NkBaseRenderer::CreateRenderTarget(const NkRenderTargetDesc& desc) {
        threading::NkScopedLock lk(mRTMutex);
        const uint64 id = NextId();
        NkRenderTargetEntry& e = mRenderTargets[id];
        e.desc = desc;
        if (!CreateRenderTarget_Impl(e)) {
            mRenderTargets.Erase(id);
            logger_src.Infof("[NKRenderer] CreateRenderTarget FAILED: %s\n",
                              desc.debugName ? desc.debugName : "?");
            return NkRenderTargetHandle::Null();
        }
        e.valid = true;
        return NkRenderTargetHandle{id};
    }

    void NkBaseRenderer::DestroyRenderTarget(NkRenderTargetHandle& h) {
        if (!h.IsValid()) return;
        threading::NkScopedLock lk(mRTMutex);
        NkRenderTargetEntry* e = mRenderTargets.Find(h.id);
        if (e && e->valid) { DestroyRenderTarget_Impl(*e); mRenderTargets.Erase(h.id); }
        h = NkRenderTargetHandle::Null();
    }

    NkTextureHandle NkBaseRenderer::GetRenderTargetColor(NkRenderTargetHandle rt, uint32 slot) const {
        threading::NkScopedLock lk(mRTMutex);
        const NkRenderTargetEntry* e = mRenderTargets.Find(rt.id);
        if (!e || !e->valid || slot >= e->colorTextures.Size()) return NkTextureHandle::Null();
        return e->colorTextures[slot];
    }

    NkTextureHandle NkBaseRenderer::GetRenderTargetDepth(NkRenderTargetHandle rt) const {
        threading::NkScopedLock lk(mRTMutex);
        const NkRenderTargetEntry* e = mRenderTargets.Find(rt.id);
        return (e && e->valid) ? e->depthTexture : NkTextureHandle::Null();
    }

    // =========================================================================
    // Font
    // =========================================================================
    NkFontHandle NkBaseRenderer::CreateFont(const NkFontDesc& desc) {
        threading::NkScopedLock lk(mFontMutex);
        const uint64 id = NextId();
        NkFontEntry& e = mFonts[id];
        e.desc = desc;
        if (!CreateFont_Impl(e)) {
            mFonts.Erase(id);
            logger_src.Infof("[NKRenderer] CreateFont FAILED: %s\n",
                              desc.filePath ? desc.filePath : "?");
            return NkFontHandle::Null();
        }
        e.valid = true;
        return NkFontHandle{id};
    }

    void NkBaseRenderer::DestroyFont(NkFontHandle& h) {
        if (!h.IsValid()) return;
        threading::NkScopedLock lk(mFontMutex);
        NkFontEntry* e = mFonts.Find(h.id);
        if (e && e->valid) { DestroyFont_Impl(*e); mFonts.Erase(h.id); }
        h = NkFontHandle::Null();
    }

    bool NkBaseRenderer::GetGlyph(NkFontHandle font, uint32 cp, NkGlyphInfo& out) const {
        threading::NkScopedLock lk(mFontMutex);
        const NkFontEntry* e = mFonts.Find(font.id);
        if (!e || !e->valid) return false;
        const NkGlyphInfo* g = e->glyphs.Find(cp);
        if (!g) return false;
        out = *g;
        return true;
    }

    NkTextureHandle NkBaseRenderer::GetFontAtlasTexture(NkFontHandle font) const {
        threading::NkScopedLock lk(mFontMutex);
        const NkFontEntry* e = mFonts.Find(font.id);
        return (e && e->valid) ? e->atlasHandle : NkTextureHandle::Null();
    }

    // =========================================================================
    // Uniform block
    // =========================================================================
    NkUniformBlockHandle NkBaseRenderer::CreateUniformBlock(const NkUniformBlockDesc& desc) {
        threading::NkScopedLock lk(mUBMutex);
        nkentseu::NkBufferHandle rhi = mDevice->CreateBuffer(
            NkBufferDesc::Uniform(desc.sizeBytes));
        if (!rhi.IsValid()) return NkUniformBlockHandle::Null();
        const uint64 id = NextId();
        UniformBlockEntry& e = mUniformBlocks[id];
        e.rhiBuffer = rhi.id;
        e.sizeBytes = desc.sizeBytes;
        e.valid     = true;
        return NkUniformBlockHandle{id};
    }

    void NkBaseRenderer::DestroyUniformBlock(NkUniformBlockHandle& h) {
        if (!h.IsValid()) return;
        threading::NkScopedLock lk(mUBMutex);
        UniformBlockEntry* e = mUniformBlocks.Find(h.id);
        if (e && e->valid) {
            nkentseu::NkBufferHandle rhi; rhi.id = e->rhiBuffer;
            mDevice->DestroyBuffer(rhi);
            mUniformBlocks.Erase(h.id);
        }
        h = NkUniformBlockHandle::Null();
    }

    bool NkBaseRenderer::WriteUniformBlock(NkUniformBlockHandle ub,
                                           const void* data, uint32 size, uint32 offset) {
        threading::NkScopedLock lk(mUBMutex);
        UniformBlockEntry* e = mUniformBlocks.Find(ub.id);
        if (!e || !e->valid) return false;
        nkentseu::NkBufferHandle rhi; rhi.id = e->rhiBuffer;
        return mDevice->WriteBuffer(rhi, data, size, offset);
    }

    // =========================================================================
    // Frame
    // =========================================================================
    bool NkBaseRenderer::BeginFrame() {
        if (!mValid) return false;
        mStats = NkRendererStats{};
        mCommand.Reset();
        return BeginFrame_Impl();
    }

    bool NkBaseRenderer::BeginRenderPass(const NkRenderPassInfo& info) {
        mCurrentPass = info;
        mCurrentCmd  = mDevice->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
        if (!mCurrentCmd || !mCurrentCmd->IsValid()) return false;
        mCurrentCmd->Begin();
        return BeginRenderPass_Impl(info, mCurrentCmd);
    }

    void NkBaseRenderer::EndRenderPass() {
        if (!mCurrentCmd) return;

        // Trier les draw calls
        mCommand.Sort();

        // Soumettre au backend
        SubmitDrawCalls_Impl(mCommand, mCurrentCmd, mCurrentPass);

        EndRenderPass_Impl(mCurrentCmd);
        mCurrentCmd->End();
    }

    void NkBaseRenderer::EndFrame() {
        if (!mCurrentCmd) return;
        mDevice->SubmitAndPresent(mCurrentCmd);
        EndFrame_Impl(mCurrentCmd);
        mDevice->DestroyCommandBuffer(mCurrentCmd);
        mCurrentCmd = nullptr;
    }

    void NkBaseRenderer::OnResize(uint32 w, uint32 h) {
        mDevice->OnResize(w, h);
    }

    uint32 NkBaseRenderer::GetSwapchainWidth()  const { return mDevice->GetSwapchainWidth(); }
    uint32 NkBaseRenderer::GetSwapchainHeight() const { return mDevice->GetSwapchainHeight(); }

} // namespace renderer
} // namespace nkentseu