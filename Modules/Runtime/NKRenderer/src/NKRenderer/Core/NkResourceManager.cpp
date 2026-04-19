// =============================================================================
// NkResourceManager.cpp
// Gestionnaire centralisé des ressources GPU.
// Toute la communication avec NKRHI passe ICI — le reste du renderer
// ne touche jamais NkIDevice directement.
// =============================================================================
#include "NKRenderer/Core/NkResourceManager.h"
#include "NKRenderer/Resources/NkResourceDescs.h"
#include "NKRHI/Core/NkDescs.h"
#include "NKRHI/Core/NkTypes.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cmath>

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // Helpers de conversion NkPixelFormat → NkGPUFormat
        // =============================================================================
        static nkentseu::NkGPUFormat ToRHIFormat(NkPixelFormat f) {
            switch (f) {
                case NkPixelFormat::NK_R8:          return nkentseu::NkGPUFormat::NK_R8_UNORM;
                case NkPixelFormat::NK_RG8:         return nkentseu::NkGPUFormat::NK_RG8_UNORM;
                case NkPixelFormat::NK_RGBA8:       return nkentseu::NkGPUFormat::NK_RGBA8_UNORM;
                case NkPixelFormat::NK_RGBA8_SRGB:  return nkentseu::NkGPUFormat::NK_RGBA8_SRGB;
                case NkPixelFormat::NK_BGRA8:       return nkentseu::NkGPUFormat::NK_BGRA8_UNORM;
                case NkPixelFormat::NK_BGRA8_SRGB:  return nkentseu::NkGPUFormat::NK_BGRA8_SRGB;
                case NkPixelFormat::NK_RGBA16F:     return nkentseu::NkGPUFormat::NK_RGBA16_FLOAT;
                case NkPixelFormat::NK_RGBA32F:     return nkentseu::NkGPUFormat::NK_RGBA32_FLOAT;
                case NkPixelFormat::NK_R32F:        return nkentseu::NkGPUFormat::NK_R32_FLOAT;
                case NkPixelFormat::NK_RG32F:       return nkentseu::NkGPUFormat::NK_RG32_FLOAT;
                case NkPixelFormat::NK_R16F:        return nkentseu::NkGPUFormat::NK_R16_FLOAT;
                case NkPixelFormat::NK_RG16F:       return nkentseu::NkGPUFormat::NK_RG16_FLOAT;
                case NkPixelFormat::NK_R8_UINT:     return nkentseu::NkGPUFormat::NK_R8_UINT;
                case NkPixelFormat::NK_R32_UINT:    return nkentseu::NkGPUFormat::NK_R32_UINT;
                case NkPixelFormat::NK_D16:         return nkentseu::NkGPUFormat::NK_D16_UNORM;
                case NkPixelFormat::NK_D24S8:       return nkentseu::NkGPUFormat::NK_D24_UNORM_S8_UINT;
                case NkPixelFormat::NK_D32F:        return nkentseu::NkGPUFormat::NK_D32_FLOAT;
                case NkPixelFormat::NK_D32FS8:      return nkentseu::NkGPUFormat::NK_D32_FLOAT_S8_UINT;
                case NkPixelFormat::NK_BC1_SRGB:    return nkentseu::NkGPUFormat::NK_BC1_RGB_SRGB;
                case NkPixelFormat::NK_BC3_SRGB:    return nkentseu::NkGPUFormat::NK_BC3_SRGB;
                case NkPixelFormat::NK_BC5:         return nkentseu::NkGPUFormat::NK_BC5_UNORM;
                case NkPixelFormat::NK_BC7_SRGB:    return nkentseu::NkGPUFormat::NK_BC7_SRGB;
                default:                            return nkentseu::NkGPUFormat::NK_RGBA8_UNORM;
            }
        }

        static nkentseu::NkFilter ToRHIFilter(NkFilterMode f) {
            return (f == NkFilterMode::NK_NEAREST)
                ? nkentseu::NkFilter::NK_NEAREST
                : nkentseu::NkFilter::NK_LINEAR;
        }

        static nkentseu::NkMipFilter ToRHIMipFilter(NkMipFilter f) {
            switch (f) {
                case NkMipFilter::NK_NONE:    return nkentseu::NkMipFilter::NK_NONE;
                case NkMipFilter::NK_NEAREST: return nkentseu::NkMipFilter::NK_NEAREST;
                default:                      return nkentseu::NkMipFilter::NK_LINEAR;
            }
        }

        static nkentseu::NkAddressMode ToRHIWrap(NkWrapMode w) {
            switch (w) {
                case NkWrapMode::NK_REPEAT:  return nkentseu::NkAddressMode::NK_REPEAT;
                case NkWrapMode::NK_MIRROR:  return nkentseu::NkAddressMode::NK_MIRRORED_REPEAT;
                case NkWrapMode::NK_CLAMP:   return nkentseu::NkAddressMode::NK_CLAMP_TO_EDGE;
                case NkWrapMode::NK_BORDER:  return nkentseu::NkAddressMode::NK_CLAMP_TO_BORDER;
                default:                     return nkentseu::NkAddressMode::NK_REPEAT;
            }
        }

        static nkentseu::NkTextureType ToRHITextureType(NkTextureKind k) {
            switch (k) {
                case NkTextureKind::NK_CUBE:       return nkentseu::NkTextureType::NK_CUBE;
                case NkTextureKind::NK_2D_ARRAY:   return nkentseu::NkTextureType::NK_TEX2D_ARRAY;
                case NkTextureKind::NK_3D:         return nkentseu::NkTextureType::NK_TEX3D;
                default:                           return nkentseu::NkTextureType::NK_TEX2D;
            }
        }

        // =============================================================================
        // Init / Shutdown
        // =============================================================================
        bool NkResourceManager::Init(NkIDevice* device) {
            if (!device) return false;
            mDevice = device;
            CreateDefaultResources();
            CreatePrimitives();
            return true;
        }

        void NkResourceManager::Shutdown() {
            if (!mDevice) return;

            // Libérer les defaults
            ReleaseTexture(mDefaultWhite);
            ReleaseTexture(mDefaultBlack);
            ReleaseTexture(mDefaultNormal);
            ReleaseTexture(mDefaultORM);
            ReleaseMesh(mQuadMesh);
            ReleaseMesh(mCubeMesh);
            ReleaseMesh(mSphereMesh);
            ReleaseMesh(mFullscreenQuad);

            // Libérer toutes les ressources restantes
            {
                threading::NkScopedLock lock(mTextureMutex);
                mTextures.ForEach([this](const uint64&, TextureEntry& e) {
                    if (!e.valid) return;
                    nkentseu::NkTextureHandle th; th.id = e.rhiTextureId;
                    nkentseu::NkSamplerHandle sh; sh.id = e.rhiSamplerId;
                    if (th.IsValid()) mDevice->DestroyTexture(th);
                    if (sh.IsValid()) mDevice->DestroySampler(sh);
                });
                mTextures.Clear();
                mTexturePathCache.Clear();
            }
            {
                threading::NkScopedLock lock(mMeshMutex);
                mMeshes.ForEach([this](const uint64&, MeshEntry& e) {
                    if (!e.valid) return;
                    nkentseu::NkBufferHandle vbo; vbo.id = e.rhiVBO;
                    nkentseu::NkBufferHandle ibo; ibo.id = e.rhiIBO;
                    if (vbo.IsValid()) mDevice->DestroyBuffer(vbo);
                    if (ibo.IsValid()) mDevice->DestroyBuffer(ibo);
                });
                mMeshes.Clear();
                mMeshPathCache.Clear();
            }
            {
                threading::NkScopedLock lock(mShaderMutex);
                mShaders.ForEach([this](const uint64&, ShaderEntry& e) {
                    if (!e.valid) return;
                    nkentseu::NkShaderHandle sh; sh.id = e.rhiShaderId;
                    if (sh.IsValid()) mDevice->DestroyShader(sh);
                });
                mShaders.Clear();
            }

            mDevice = nullptr;
        }

        // =============================================================================
        // Ressources par défaut
        // =============================================================================
        void NkResourceManager::CreateDefaultResources() {
            // Blanc 1×1
            static const uint8 white[4] = {255, 255, 255, 255};
            auto dw = NkTextureDesc::FromMemory(white, 1, 1, NkPixelFormat::NK_RGBA8);
            dw.srgb = false; dw.debugName = "Default_White";
            mDefaultWhite = CreateTexture(dw);

            // Noir 1×1
            static const uint8 black[4] = {0, 0, 0, 255};
            auto db = NkTextureDesc::FromMemory(black, 1, 1, NkPixelFormat::NK_RGBA8);
            db.srgb = false; db.debugName = "Default_Black";
            mDefaultBlack = CreateTexture(db);

            // Normal plat 1×1 (128,128,255 = Z+)
            static const uint8 normal[4] = {128, 128, 255, 255};
            auto dn = NkTextureDesc::FromMemory(normal, 1, 1, NkPixelFormat::NK_RGBA8);
            dn.srgb = false; dn.debugName = "Default_FlatNormal";
            mDefaultNormal = CreateTexture(dn);

            // ORM 1×1 : Occlusion=1, Roughness=0.5, Metallic=0
            static const uint8 orm[4] = {255, 128, 0, 255};
            auto do_ = NkTextureDesc::FromMemory(orm, 1, 1, NkPixelFormat::NK_RGBA8);
            do_.srgb = false; do_.debugName = "Default_ORM";
            mDefaultORM = CreateTexture(do_);
        }

        void NkResourceManager::CreatePrimitives() {
            // ── Quad 2D centré ────────────────────────────────────────────────────────
            {
                static const NkVertex3D verts[4] = {
                    {-1,-1, 0,  0,0,-1, 1,0,0,  0,0},
                    { 1,-1, 0,  0,0,-1, 1,0,0,  1,0},
                    { 1, 1, 0,  0,0,-1, 1,0,0,  1,1},
                    {-1, 1, 0,  0,0,-1, 1,0,0,  0,1},
                };
                static const uint32 idx[6] = {0,1,2, 0,2,3};
                auto d = NkMeshDesc::FromBuffers(verts, 4, idx, 6, "Primitive_Quad");
                mFullscreenQuad = CreateMesh(d);
            }

            // ── Cube ──────────────────────────────────────────────────────────────────
            {
                struct Face {
                    float vx[4][3]; float uv[4][2]; float nx, ny, nz;
                };
                static const Face faces[6] = {
                    {{{ .5f,-.5f, .5f},{ .5f, .5f, .5f},{-.5f, .5f, .5f},{-.5f,-.5f, .5f}},{{1,0},{1,1},{0,1},{0,0}},0,0,1},
                    {{{-.5f,-.5f,-.5f},{-.5f, .5f,-.5f},{ .5f, .5f,-.5f},{ .5f,-.5f,-.5f}},{{1,0},{1,1},{0,1},{0,0}},0,0,-1},
                    {{{-.5f,-.5f,-.5f},{ .5f,-.5f,-.5f},{ .5f,-.5f, .5f},{-.5f,-.5f, .5f}},{{0,1},{1,1},{1,0},{0,0}},0,-1,0},
                    {{{-.5f, .5f, .5f},{ .5f, .5f, .5f},{ .5f, .5f,-.5f},{-.5f, .5f,-.5f}},{{0,1},{1,1},{1,0},{0,0}},0,1,0},
                    {{{-.5f,-.5f,-.5f},{-.5f,-.5f, .5f},{-.5f, .5f, .5f},{-.5f, .5f,-.5f}},{{0,0},{1,0},{1,1},{0,1}},-1,0,0},
                    {{{ .5f,-.5f, .5f},{ .5f,-.5f,-.5f},{ .5f, .5f,-.5f},{ .5f, .5f, .5f}},{{0,0},{1,0},{1,1},{0,1}},1,0,0},
                };
                static const int quad[6] = {0,1,2, 0,2,3};
                NkVector<NkVertex3D> v; v.Reserve(24);
                NkVector<uint32>     i; i.Reserve(36);
                for (auto& f : faces) {
                    uint32 base = (uint32)v.Size();
                    for (int q = 0; q < 4; ++q) {
                        NkVertex3D vtx;
                        vtx.px=f.vx[q][0]; vtx.py=f.vx[q][1]; vtx.pz=f.vx[q][2];
                        vtx.nx=f.nx; vtx.ny=f.ny; vtx.nz=f.nz;
                        vtx.u=f.uv[q][0]; vtx.v=f.uv[q][1];
                        v.PushBack(vtx);
                    }
                    for (int q : quad) i.PushBack(base + q);
                }
                auto d = NkMeshDesc::FromBuffers(v.Data(), (uint32)v.Size(),
                                                  i.Data(), (uint32)i.Size(),
                                                  "Primitive_Cube");
                mCubeMesh = CreateMesh(d);
            }

            // ── Sphère UV (32 stacks × 48 slices) ────────────────────────────────────
            {
                const uint32 stacks = 24, slices = 32;
                const float pi = 3.14159265f;
                NkVector<NkVertex3D> v;
                NkVector<uint32>     i;
                for (uint32 s = 0; s <= stacks; ++s) {
                    const float phi = pi * s / stacks;
                    for (uint32 sl = 0; sl <= slices; ++sl) {
                        const float theta = 2.f * pi * sl / slices;
                        const float x = sinf(phi) * cosf(theta);
                        const float y = cosf(phi);
                        const float z = sinf(phi) * sinf(theta);
                        NkVertex3D vtx;
                        vtx.px=x*.5f; vtx.py=y*.5f; vtx.pz=z*.5f;
                        vtx.nx=x; vtx.ny=y; vtx.nz=z;
                        vtx.u=(float)sl/slices; vtx.v=(float)s/stacks;
                        v.PushBack(vtx);
                    }
                }
                for (uint32 s = 0; s < stacks; ++s) {
                    for (uint32 sl = 0; sl < slices; ++sl) {
                        uint32 a = s*(slices+1)+sl, b = a+slices+1;
                        i.PushBack(a); i.PushBack(b); i.PushBack(a+1);
                        i.PushBack(b); i.PushBack(b+1); i.PushBack(a+1);
                    }
                }
                auto d = NkMeshDesc::FromBuffers(v.Data(), (uint32)v.Size(),
                                                  i.Data(), (uint32)i.Size(),
                                                  "Primitive_Sphere");
                mSphereMesh = CreateMesh(d);
                mQuadMesh   = mFullscreenQuad; // réutiliser
            }
        }

        // =============================================================================
        // Textures
        // =============================================================================
        NkTextureHandle NkResourceManager::LoadTexture(const char* path,
                                                         bool srgb,
                                                         bool generateMips) {
            if (!path) return {};
            NkString key(path);

            {
                threading::NkScopedLock lock(mTextureMutex);
                auto* found = mTexturePathCache.Find(key);
                if (found && found->IsValid()) {
                    // Incrémenter le ref count
                    TextureEntry* e = mTextures.Find(found->id);
                    if (e) { ++e->refCount; return *found; }
                }
            }

            auto desc = NkTextureDesc::FromFile(path, srgb, generateMips);
            NkTextureHandle h = CreateTextureInternal(desc, path);
            if (h.IsValid()) {
                threading::NkScopedLock lock(mTextureMutex);
                mTexturePathCache[key] = h;
            }
            return h;
        }

        NkTextureHandle NkResourceManager::CreateTexture(const NkTextureDesc& desc) {
            return CreateTextureInternal(desc, nullptr);
        }

        NkTextureHandle NkResourceManager::CreateTextureInternal(
                const NkTextureDesc& desc, const char* filePath) {

            if (!mDevice) return {};

            // Construire le NkTextureDesc RHI
            nkentseu::NkTextureDesc rhiTex{};
            rhiTex.type       = ToRHITextureType(desc.kind);
            rhiTex.format     = ToRHIFormat(desc.format);
            rhiTex.width      = desc.width;
            rhiTex.height     = desc.height;
            rhiTex.depth      = desc.depth;
            rhiTex.arrayLayers= desc.layers;
            rhiTex.samples    = nkentseu::NkSampleCount::NK_S1;
            rhiTex.debugName  = desc.debugName;

            uint32 mips = desc.mipLevels;
            if (mips == 0 && desc.generateMips) {
                uint32 maxDim = desc.width > desc.height ? desc.width : desc.height;
                mips = (uint32)(floorf(log2f((float)maxDim))) + 1;
            }
            if (mips == 0) mips = 1;
            rhiTex.mipLevels = mips;

            // Bind flags selon le type
            using BF = nkentseu::NkBindFlags;
            if (desc.kind == NkTextureKind::NK_RENDER_TARGET) {
                rhiTex.bindFlags = BF::NK_RENDER_TARGET | BF::NK_SHADER_RESOURCE;
                rhiTex.usage     = nkentseu::NkResourceUsage::NK_DEFAULT;
            } else if (desc.kind == NkTextureKind::NK_DEPTH_STENCIL) {
                rhiTex.bindFlags = BF::NK_DEPTH_STENCIL | BF::NK_SHADER_RESOURCE;
                rhiTex.usage     = nkentseu::NkResourceUsage::NK_DEFAULT;
            } else {
                rhiTex.bindFlags = BF::NK_SHADER_RESOURCE;
                rhiTex.usage     = nkentseu::NkResourceUsage::NK_DEFAULT;
                if (desc.source == NkTextureDesc::Source::NK_RAW_PIXELS) {
                    rhiTex.initialData = desc.pixels;
                    rhiTex.rowPitch    = desc.rowPitch;
                }
            }

            // TODO: pour NK_FILE_PATH, charger via NkImage (stb_image)
            // if (filePath) { NkImage img; img.Load(filePath); ... }

            nkentseu::NkTextureHandle rhiHandle = mDevice->CreateTexture(rhiTex);
            if (!rhiHandle.IsValid()) return {};

            // Sampler
            nkentseu::NkSamplerDesc sd{};
            sd.magFilter     = ToRHIFilter(desc.filterMag);
            sd.minFilter     = ToRHIFilter(desc.filterMin);
            sd.mipFilter     = ToRHIMipFilter(desc.filterMip);
            sd.addressU      = ToRHIWrap(desc.wrapU);
            sd.addressV      = ToRHIWrap(desc.wrapV);
            sd.addressW      = ToRHIWrap(desc.wrapW);
            sd.maxAnisotropy = desc.maxAniso;
            if (desc.shadowSampler) {
                sd.compareEnable = true;
                sd.compareOp     = nkentseu::NkCompareOp::NK_LESS_EQUAL;
            }
            nkentseu::NkSamplerHandle sampHandle = mDevice->CreateSampler(sd);

            uint64 hid = NextId();
            TextureEntry entry;
            entry.rhiTextureId = rhiHandle.id;
            entry.rhiSamplerId = sampHandle.id;
            entry.desc         = desc;
            entry.filePath     = filePath ? filePath : "";
            entry.refCount     = 1;
            entry.valid        = true;

            {
                threading::NkScopedLock lock(mTextureMutex);
                mTextures[hid] = entry;
            }

            NkTextureHandle h; h.id = hid;
            return h;
        }

        NkTextureHandle NkResourceManager::CreateRenderTarget(
                uint32 w, uint32 h, NkPixelFormat fmt, NkMSAA msaa,
                const char* debugName) {
            auto desc = NkTextureDesc::RenderTarget(w, h, fmt, msaa);
            desc.debugName = debugName;
            return CreateTexture(desc);
        }

        NkTextureHandle NkResourceManager::CreateDepthTexture(
                uint32 w, uint32 h, NkPixelFormat fmt, NkMSAA msaa,
                const char* debugName) {
            auto desc = NkTextureDesc::DepthStencil(w, h, fmt, msaa);
            desc.debugName = debugName;
            return CreateTexture(desc);
        }

        bool NkResourceManager::WriteTexture(NkTextureHandle tex, const void* data,
                                               uint32 rowPitch, uint32 mip, uint32 layer) {
            TextureEntry* e = nullptr;
            { threading::NkScopedLock lock(mTextureMutex); e = mTextures.Find(tex.id); }
            if (!e || !e->valid) return false;
            nkentseu::NkTextureHandle rh; rh.id = e->rhiTextureId;
            if (mip == 0 && layer == 0)
                return mDevice->WriteTexture(rh, data, rowPitch);
            return mDevice->WriteTextureRegion(rh, data,
                0, 0, 0, e->desc.width, e->desc.height, 1, mip, layer, rowPitch);
        }

        bool NkResourceManager::GenerateMipmaps(NkTextureHandle tex) {
            TextureEntry* e = nullptr;
            { threading::NkScopedLock lock(mTextureMutex); e = mTextures.Find(tex.id); }
            if (!e || !e->valid) return false;
            nkentseu::NkTextureHandle rh; rh.id = e->rhiTextureId;
            return mDevice->GenerateMipmaps(rh);
        }

        void NkResourceManager::ReleaseTexture(NkTextureHandle& h) {
            if (!h.IsValid()) return;
            threading::NkScopedLock lock(mTextureMutex);
            TextureEntry* e = mTextures.Find(h.id);
            if (!e) { h.id = 0; return; }
            if (--e->refCount > 0) { h.id = 0; return; }
            nkentseu::NkTextureHandle th; th.id = e->rhiTextureId;
            nkentseu::NkSamplerHandle sh; sh.id = e->rhiSamplerId;
            if (th.IsValid()) mDevice->DestroyTexture(th);
            if (sh.IsValid()) mDevice->DestroySampler(sh);
            if (!e->filePath.Empty()) mTexturePathCache.Erase(e->filePath);
            mTextures.Erase(h.id);
            h.id = 0;
        }

        // ── Getters texture ────────────────────────────────────────────────────────
        uint32 NkResourceManager::GetTextureWidth(NkTextureHandle h) const {
            threading::NkScopedLock lock(mTextureMutex);
            const TextureEntry* e = mTextures.Find(h.id);
            return e ? e->desc.width : 0;
        }
        uint32 NkResourceManager::GetTextureHeight(NkTextureHandle h) const {
            threading::NkScopedLock lock(mTextureMutex);
            const TextureEntry* e = mTextures.Find(h.id);
            return e ? e->desc.height : 0;
        }
        NkPixelFormat NkResourceManager::GetTextureFormat(NkTextureHandle h) const {
            threading::NkScopedLock lock(mTextureMutex);
            const TextureEntry* e = mTextures.Find(h.id);
            return e ? e->desc.format : NkPixelFormat::NK_UNDEFINED;
        }

        // =============================================================================
        // Meshes
        // =============================================================================
        NkMeshHandle NkResourceManager::CreateMesh(const NkMeshDesc& desc) {
            if (!mDevice || desc.vertexCount == 0) return {};

            uint64 vsz = (uint64)desc.vertexCount * desc.vertexStride;
            uint64 isz = (uint64)desc.indexCount  *
                (desc.indexFormat == NkIndexFormat::NK_UINT16 ? 2 : 4);

            using BD = nkentseu::NkBufferDesc;
            nkentseu::NkBufferHandle vbo, ibo;

            if (desc.isDynamic || !desc.vertexData) {
                vbo = mDevice->CreateBuffer(
                    BD::VertexDynamic(vsz > 0 ? vsz : 64));
            } else {
                vbo = mDevice->CreateBuffer(BD::Vertex(vsz, desc.vertexData));
            }
            if (!vbo.IsValid()) return {};

            if (isz > 0) {
                if (desc.isDynamic || !desc.indexData) {
                    ibo = mDevice->CreateBuffer(BD::IndexDynamic(isz));
                } else {
                    ibo = mDevice->CreateBuffer(BD::Index(isz, desc.indexData));
                }
                if (!ibo.IsValid()) {
                    mDevice->DestroyBuffer(vbo); return {};
                }
            }

            if (desc.debugName) {
                mDevice->SetDebugName(vbo, desc.debugName);
                if (ibo.IsValid()) mDevice->SetDebugName(ibo, desc.debugName);
            }

            // Calculer l'AABB si demandé
            NkAABB aabb;
            if (desc.computeAABB && desc.vertexData && desc.vertexStride >= 12) {
                aabb.min = {1e30f, 1e30f, 1e30f};
                aabb.max = {-1e30f,-1e30f,-1e30f};
                const uint8* vd = (const uint8*)desc.vertexData;
                for (uint32 i = 0; i < desc.vertexCount; ++i) {
                    const float* p = (const float*)(vd + i * desc.vertexStride);
                    aabb.min.x = p[0] < aabb.min.x ? p[0] : aabb.min.x;
                    aabb.min.y = p[1] < aabb.min.y ? p[1] : aabb.min.y;
                    aabb.min.z = p[2] < aabb.min.z ? p[2] : aabb.min.z;
                    aabb.max.x = p[0] > aabb.max.x ? p[0] : aabb.max.x;
                    aabb.max.y = p[1] > aabb.max.y ? p[1] : aabb.max.y;
                    aabb.max.z = p[2] > aabb.max.z ? p[2] : aabb.max.z;
                }
            }

            uint64 hid = NextId();
            MeshEntry entry;
            entry.rhiVBO      = vbo.id;
            entry.rhiIBO      = ibo.id;
            entry.desc        = desc;
            entry.aabb        = aabb;
            entry.vertexCount = desc.vertexCount;
            entry.indexCount  = desc.indexCount;
            entry.refCount    = 1;
            entry.valid       = true;

            {
                threading::NkScopedLock lock(mMeshMutex);
                mMeshes[hid] = entry;
            }

            NkMeshHandle h; h.id = hid;
            return h;
        }

        bool NkResourceManager::UpdateMeshVertices(NkMeshHandle mesh,
                                                     const void* data, uint32 count) {
            MeshEntry* e = nullptr;
            { threading::NkScopedLock lock(mMeshMutex); e = mMeshes.Find(mesh.id); }
            if (!e || !e->valid || !data) return false;
            nkentseu::NkBufferHandle vbo; vbo.id = e->rhiVBO;
            uint64 sz = (uint64)count * e->desc.vertexStride;
            bool ok = mDevice->WriteBuffer(vbo, data, sz);
            if (ok) e->vertexCount = count;
            return ok;
        }

        bool NkResourceManager::UpdateMeshIndices(NkMeshHandle mesh,
                                                    const void* data, uint32 count,
                                                    NkIndexFormat fmt) {
            MeshEntry* e = nullptr;
            { threading::NkScopedLock lock(mMeshMutex); e = mMeshes.Find(mesh.id); }
            if (!e || !e->valid || !data || !e->rhiIBO) return false;
            nkentseu::NkBufferHandle ibo; ibo.id = e->rhiIBO;
            uint64 stride = (fmt == NkIndexFormat::NK_UINT16) ? 2 : 4;
            bool ok = mDevice->WriteBuffer(ibo, data, (uint64)count * stride);
            if (ok) e->indexCount = count;
            return ok;
        }

        void NkResourceManager::ReleaseMesh(NkMeshHandle& h) {
            if (!h.IsValid()) return;
            threading::NkScopedLock lock(mMeshMutex);
            MeshEntry* e = mMeshes.Find(h.id);
            if (!e) { h.id = 0; return; }
            if (--e->refCount > 0) { h.id = 0; return; }
            nkentseu::NkBufferHandle vbo; vbo.id = e->rhiVBO;
            nkentseu::NkBufferHandle ibo; ibo.id = e->rhiIBO;
            if (vbo.IsValid()) mDevice->DestroyBuffer(vbo);
            if (ibo.IsValid()) mDevice->DestroyBuffer(ibo);
            mMeshes.Erase(h.id);
            h.id = 0;
        }

        uint32 NkResourceManager::GetMeshVertexCount(NkMeshHandle h) const {
            threading::NkScopedLock lock(mMeshMutex);
            const MeshEntry* e = mMeshes.Find(h.id); return e ? e->vertexCount : 0;
        }
        uint32 NkResourceManager::GetMeshIndexCount(NkMeshHandle h) const {
            threading::NkScopedLock lock(mMeshMutex);
            const MeshEntry* e = mMeshes.Find(h.id); return e ? e->indexCount : 0;
        }

        static NkAABB sDefaultAABB;
        const NkAABB& NkResourceManager::GetMeshAABB(NkMeshHandle h) const {
            threading::NkScopedLock lock(mMeshMutex);
            const MeshEntry* e = mMeshes.Find(h.id);
            return e ? e->aabb : sDefaultAABB;
        }

        // =============================================================================
        // Accès interne RHI (pour les renderers)
        // =============================================================================
        uint64 NkResourceManager::GetTextureRHIId(NkTextureHandle h) const {
            threading::NkScopedLock lock(mTextureMutex);
            const TextureEntry* e = mTextures.Find(h.id);
            return e ? e->rhiTextureId : 0;
        }
        uint64 NkResourceManager::GetSamplerRHIId(NkTextureHandle h) const {
            threading::NkScopedLock lock(mTextureMutex);
            const TextureEntry* e = mTextures.Find(h.id);
            return e ? e->rhiSamplerId : 0;
        }
        uint64 NkResourceManager::GetMeshVBORHIId(NkMeshHandle h) const {
            threading::NkScopedLock lock(mMeshMutex);
            const MeshEntry* e = mMeshes.Find(h.id);
            return e ? e->rhiVBO : 0;
        }
        uint64 NkResourceManager::GetMeshIBORHIId(NkMeshHandle h) const {
            threading::NkScopedLock lock(mMeshMutex);
            const MeshEntry* e = mMeshes.Find(h.id);
            return e ? e->rhiIBO : 0;
        }

        // =============================================================================
        // Stats
        // =============================================================================
        NkResourceManager::Stats NkResourceManager::GetStats() const {
            Stats s;
            { threading::NkScopedLock l(mTextureMutex); s.textureCount  = mTextures.Size(); }
            { threading::NkScopedLock l(mMeshMutex);    s.meshCount     = mMeshes.Size(); }
            { threading::NkScopedLock l(mShaderMutex);  s.shaderCount   = mShaders.Size(); }
            { threading::NkScopedLock l(mMaterialMutex);s.materialCount = mMaterials.Size(); }
            return s;
        }

    } // namespace renderer
} // namespace nkentseu
