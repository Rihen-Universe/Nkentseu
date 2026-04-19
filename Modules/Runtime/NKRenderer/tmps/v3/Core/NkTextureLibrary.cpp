// =============================================================================
// NkTextureLibrary.cpp
// =============================================================================
#include "NkTextureLibrary.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cmath>

// stb_image (header-only, inclure une seule fois via un .cpp dédié)
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

namespace nkentseu {

    NkTextureLibrary::NkTextureLibrary(NkIDevice* device) : mDevice(device) {}

    NkTextureLibrary::~NkTextureLibrary() { Shutdown(); }

    bool NkTextureLibrary::Initialize() {
        // Textures système
        mWhiteId   = CreateSolid(255,255,255,255,  "nk_white");
        mBlackId   = CreateSolid(0,0,0,255,         "nk_black");
        mNormalId  = CreateSolid(128,128,255,255,   "nk_normal"); // (0.5,0.5,1.0) = Z-up
        mCheckerId = CreateCheckerboard(64,8,
            {0.8f,0.1f,0.8f,1.f},{0.1f,0.1f,0.1f,1.f});
        GenerateBRDFLUT();
        logger.Infof("[NkTextureLibrary] Initialized (white=%llu normal=%llu)\n",
                     (unsigned long long)mWhiteId,
                     (unsigned long long)mNormalId);
        return true;
    }

    void NkTextureLibrary::Shutdown() {
        ReleaseAll();
        mWhiteId=mBlackId=mNormalId=mCheckerId=mBRDFLUTId=0;
    }

    NkTexId NkTextureLibrary::AllocId() { return mNextId++; }

    // =========================================================================
    // Sampler cache
    // =========================================================================
    NkSamplerHandle NkTextureLibrary::GetOrCreateSampler(NkFilter f, NkAddressMode a,
                                                          float32 aniso) {
        uint64 key = (uint64)f | ((uint64)a << 8) | ((uint64)(aniso*10.f) << 16);
        auto it    = mSamplerCache.Find(key);
        if (it) return it->Second;
        NkSamplerDesc desc;
        desc.magFilter=f; desc.minFilter=f;
        desc.mipFilter=NkMipFilter::NK_LINEAR;
        desc.addressU=a; desc.addressV=a; desc.addressW=a;
        desc.maxAnisotropy=aniso;
        NkSamplerHandle h = mDevice->CreateSampler(desc);
        mSamplerCache[key] = h;
        return h;
    }

    // =========================================================================
    // Chargement fichier via stb_image
    // =========================================================================
    bool NkTextureLibrary::LoadSTB(const NkTextureLoadDesc& desc, NkTextureInfo& out) {
        stbi_set_flip_vertically_on_load(desc.flipY ? 1 : 0);
        int w,h,ch;
        uint8* pixels = stbi_load(desc.path.CStr(), &w, &h, &ch, 4);
        if (!pixels) {
            logger.Errorf("[NkTextureLibrary] STB load failed: %s — %s\n",
                          desc.path.CStr(), stbi_failure_reason());
            return false;
        }
        NkGPUFormat fmt = desc.srgb ? NkGPUFormat::NK_RGBA8_SRGB : NkGPUFormat::NK_RGBA8_UNORM;
        NkTextureDesc td = NkTextureDesc::Tex2D((uint32)w,(uint32)h,fmt,
                                                 desc.generateMips?0:1);
        td.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
        td.usage     = NkResourceUsage::NK_DEFAULT;
        if (desc.debugName) td.debugName = desc.debugName;
        else                td.debugName = desc.path.CStr();
        out.handle = mDevice->CreateTexture(td);
        if (!out.handle.IsValid()) { stbi_image_free(pixels); return false; }
        mDevice->WriteTexture(out.handle, pixels, (uint32)w*4);
        if (desc.generateMips)
            mDevice->GenerateMipmaps(out.handle, desc.filter);
        stbi_image_free(pixels);
        out.width      = (uint32)w;
        out.height     = (uint32)h;
        out.format     = fmt;
        out.mipLevels  = desc.generateMips ? (1+(uint32)log2f((float32)std::max(w,h))) : 1;
        out.sampler    = GetOrCreateSampler(desc.filter, desc.addressMode, desc.anisotropy);
        return true;
    }

    bool NkTextureLibrary::LoadHDRFile(const NkTextureLoadDesc& desc, NkTextureInfo& out) {
        stbi_set_flip_vertically_on_load(desc.flipY?1:0);
        int w,h,ch;
        float32* pixels = stbi_loadf(desc.path.CStr(), &w, &h, &ch, 4);
        if (!pixels) {
            logger.Errorf("[NkTextureLibrary] HDR load failed: %s\n", desc.path.CStr());
            return false;
        }
        NkTextureDesc td = NkTextureDesc::Tex2D((uint32)w,(uint32)h,
                                                 NkGPUFormat::NK_RGBA32_FLOAT, 1);
        td.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
        td.debugName = desc.debugName ? desc.debugName : desc.path.CStr();
        out.handle   = mDevice->CreateTexture(td);
        if (!out.handle.IsValid()) { stbi_image_free(pixels); return false; }
        mDevice->WriteTexture(out.handle, pixels, (uint32)w*16);
        stbi_image_free(pixels);
        out.width  = (uint32)w; out.height = (uint32)h;
        out.format = NkGPUFormat::NK_RGBA32_FLOAT;
        out.sampler= GetOrCreateSampler(NkFilter::NK_LINEAR, NkAddressMode::NK_CLAMP_TO_EDGE, 1.f);
        return true;
    }

    // =========================================================================
    // Load public
    // =========================================================================
    NkTexId NkTextureLibrary::Load(const NkString& path, bool srgb, bool generateMips) {
        // Cache par chemin
        auto it = mPathCache.Find(path);
        if (it) {
            mRefCounts[it->Second]++;
            return it->Second;
        }
        NkTextureLoadDesc desc;
        desc.path=path; desc.srgb=srgb; desc.generateMips=generateMips;
        return Load(desc);
    }

    NkTexId NkTextureLibrary::Load(const NkTextureLoadDesc& desc) {
        NkTextureInfo info;
        info.id = AllocId();

        bool ok = false;
        // Détecter le format depuis l'extension
        const char* ext = strrchr(desc.path.CStr(), '.');
        if (ext && (strcmp(ext,".hdr")==0 || strcmp(ext,".HDR")==0))
            ok = LoadHDRFile(desc, info);
        else
            ok = LoadSTB(desc, info);

        if (!ok) {
            logger.Warnf("[NkTextureLibrary] Load failed '%s' — using white fallback\n",
                         desc.path.CStr());
            return mWhiteId;
        }

        mTextures[info.id]        = info;
        mRefCounts[info.id]       = 1;
        if (!desc.path.Empty()) mPathCache[desc.path] = info.id;
        logger.Infof("[NkTextureLibrary] Loaded '%s' [%ux%u] id=%llu\n",
                     desc.path.CStr(), info.width, info.height,
                     (unsigned long long)info.id);
        return info.id;
    }

    NkTexId NkTextureLibrary::Upload(const void* pixels, uint32 w, uint32 h,
                                      NkGPUFormat fmt, bool generateMips, const char* name) {
        NkTextureInfo info;
        info.id      = AllocId();
        info.width   = w; info.height = h; info.format = fmt;
        NkTextureDesc td = NkTextureDesc::Tex2D(w,h,fmt,generateMips?0:1);
        td.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
        td.debugName = name ? name : "uploaded";
        info.handle  = mDevice->CreateTexture(td);
        if (!info.handle.IsValid()) return NK_INVALID_TEX;
        uint32 bpp = NkFormatBytesPerPixel(fmt);
        mDevice->WriteTexture(info.handle, pixels, w*bpp);
        if (generateMips) mDevice->GenerateMipmaps(info.handle);
        info.sampler = GetOrCreateSampler(NkFilter::NK_LINEAR,NkAddressMode::NK_REPEAT,16.f);
        mTextures[info.id] = info;
        mRefCounts[info.id]= 1;
        return info.id;
    }

    NkTexId NkTextureLibrary::UploadGray8(const uint8* pixels, uint32 w, uint32 h, const char* name) {
        // Convertir Gray8 → RGBA8
        uint32 count = w*h;
        NkVector<uint8> rgba(count*4);
        for (uint32 i=0;i<count;i++){
            rgba[i*4+0]=255; rgba[i*4+1]=255; rgba[i*4+2]=255; rgba[i*4+3]=pixels[i];
        }
        return Upload(rgba.Data(), w, h, NkGPUFormat::NK_RGBA8_UNORM, false, name);
    }

    NkTexId NkTextureLibrary::UploadHDR(const float32* pixels, uint32 w, uint32 h, const char* name) {
        return Upload(pixels, w, h, NkGPUFormat::NK_RGBA32_FLOAT, false, name);
    }

    // =========================================================================
    // Création programmatique
    // =========================================================================
    NkTexId NkTextureLibrary::CreateSolid(uint8 r,uint8 g,uint8 b,uint8 a,const char* name) {
        uint8 p[4]={r,g,b,a};
        return Upload(p,1,1,NkGPUFormat::NK_RGBA8_UNORM,false,name);
    }

    NkTexId NkTextureLibrary::CreateSolidF(float32 r,float32 g,float32 b,float32 a,const char* name) {
        return CreateSolid((uint8)(r*255),(uint8)(g*255),(uint8)(b*255),(uint8)(a*255),name);
    }

    NkTexId NkTextureLibrary::CreateCheckerboard(uint32 size,uint32 cellSize,
                                                   NkClearColor c0,NkClearColor c1) {
        NkVector<uint8> pixels(size*size*4);
        for(uint32 y=0;y<size;y++) for(uint32 x=0;x<size;x++){
            bool even=((x/cellSize)+(y/cellSize))%2==0;
            NkClearColor& c=even?c0:c1;
            uint32 idx=(y*size+x)*4;
            pixels[idx+0]=(uint8)(c.r*255); pixels[idx+1]=(uint8)(c.g*255);
            pixels[idx+2]=(uint8)(c.b*255); pixels[idx+3]=(uint8)(c.a*255);
        }
        return Upload(pixels.Data(),size,size,NkGPUFormat::NK_RGBA8_UNORM,true,"checkerboard");
    }

    NkTexId NkTextureLibrary::CreateRenderTarget(uint32 w,uint32 h,NkGPUFormat fmt,
                                                   NkSampleCount msaa,const char* dbg) {
        NkTextureInfo info;
        info.id=AllocId(); info.width=w; info.height=h; info.format=fmt;
        NkTextureDesc td=NkTextureDesc::RenderTarget(w,h,fmt,msaa);
        td.debugName=dbg?dbg:"RenderTarget";
        info.handle=mDevice->CreateTexture(td);
        if(!info.handle.IsValid()) return NK_INVALID_TEX;
        info.sampler=GetOrCreateSampler(NkFilter::NK_LINEAR,NkAddressMode::NK_CLAMP_TO_EDGE,1.f);
        mTextures[info.id]=info; mRefCounts[info.id]=1;
        return info.id;
    }

    NkTexId NkTextureLibrary::CreateDepthTarget(uint32 w,uint32 h,NkGPUFormat fmt,
                                                  NkSampleCount msaa,const char* dbg) {
        NkTextureInfo info;
        info.id=AllocId(); info.width=w; info.height=h; info.format=fmt;
        NkTextureDesc td=NkTextureDesc::DepthStencil(w,h,fmt,msaa);
        td.debugName=dbg?dbg:"DepthTarget";
        info.handle=mDevice->CreateTexture(td);
        if(!info.handle.IsValid()) return NK_INVALID_TEX;
        info.sampler=GetOrCreateSampler(NkFilter::NK_LINEAR,NkAddressMode::NK_CLAMP_TO_EDGE,1.f);
        mTextures[info.id]=info; mRefCounts[info.id]=1;
        return info.id;
    }

    // =========================================================================
    // BRDF LUT (Split-Sum approximation pour IBL)
    // Génère une texture 512x512 RGBA16F contenant les intégrales BRDF
    // =========================================================================
    void NkTextureLibrary::GenerateBRDFLUT() {
        const uint32 SIZE = 512;
        NkVector<float32> pixels(SIZE*SIZE*4, 0.f);

        auto RadicalInverseVdC = [](uint32 bits) -> float32 {
            bits = (bits << 16u) | (bits >> 16u);
            bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
            bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
            bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
            bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
            return float32(bits) * 2.3283064365386963e-10f;
        };
        auto Hammersley = [&](uint32 i,uint32 N) -> float32* {
            static float32 v[2];
            v[0]=float32(i)/float32(N); v[1]=RadicalInverseVdC(i); return v;
        };
        auto ImportanceSampleGGX = [](float32 Xi0,float32 Xi1,float32 rough,
                                       float32* Hx,float32* Hy,float32* Hz){
            float32 a=rough*rough;
            float32 phi=2.f*3.14159f*Xi0;
            float32 cosT=sqrtf((1.f-Xi1)/(1.f+(a*a-1.f)*Xi1));
            float32 sinT=sqrtf(1.f-cosT*cosT);
            *Hx=cosf(phi)*sinT; *Hy=sinf(phi)*sinT; *Hz=cosT;
        };

        const uint32 SAMPLES=1024;
        for(uint32 yi=0;yi<SIZE;yi++) for(uint32 xi=0;xi<SIZE;xi++){
            float32 NdotV=(xi+0.5f)/SIZE, rough=(yi+0.5f)/SIZE;
            float32 Vx=sqrtf(1.f-NdotV*NdotV), Vy=0.f, Vz=NdotV;
            float32 A=0.f, B=0.f;
            for(uint32 i=0;i<SAMPLES;i++){
                float32* Xi=Hammersley(i,SAMPLES);
                float32 Hx,Hy,Hz;
                ImportanceSampleGGX(Xi[0],Xi[1],rough,&Hx,&Hy,&Hz);
                float32 Lx=2.f*(Vx*Hx+Vy*Hy+Vz*Hz)*Hx-Vx;
                float32 Ly=2.f*(Vx*Hx+Vy*Hy+Vz*Hz)*Hy-Vy;
                float32 Lz=2.f*(Vx*Hx+Vy*Hy+Vz*Hz)*Hz-Vz;
                float32 NdotL=std::max(Lz,0.f);
                float32 NdotH=std::max(Hz,0.f);
                float32 VdotH=std::max(Vx*Hx+Vy*Hy+Vz*Hz,0.f);
                if(NdotL>0.f){
                    float32 G_ggx=[](float32 NdotV,float32 rough)->float32{
                        float32 k=rough*rough/2.f;
                        return NdotV/(NdotV*(1.f-k)+k);
                    }(NdotV,rough)*[](float32 NdotL,float32 rough)->float32{
                        float32 k=rough*rough/2.f;
                        return NdotL/(NdotL*(1.f-k)+k);
                    }(NdotL,rough);
                    float32 G_vis=(G_ggx*VdotH)/(NdotH*NdotV);
                    float32 Fc=powf(1.f-VdotH,5.f);
                    A+=(1.f-Fc)*G_vis;
                    B+=Fc*G_vis;
                }
            }
            uint32 idx=(yi*SIZE+xi)*4;
            pixels[idx+0]=A/SAMPLES; pixels[idx+1]=B/SAMPLES;
            pixels[idx+2]=0.f; pixels[idx+3]=1.f;
        }
        mBRDFLUTId=Upload(pixels.Data(),SIZE,SIZE,NkGPUFormat::NK_RGBA16_FLOAT,false,"nk_brdf_lut");
        logger.Infof("[NkTextureLibrary] BRDF LUT generated (%ux%u)\n",SIZE,SIZE);
    }

    // =========================================================================
    // Accès
    // =========================================================================
    const NkTextureInfo* NkTextureLibrary::Get(NkTexId id) const {
        auto it=mTextures.Find(id); return it?&it->Second:nullptr;
    }
    NkTextureHandle NkTextureLibrary::GetHandle(NkTexId id) const {
        auto it=mTextures.Find(id); return it?it->Second.handle:NkTextureHandle{};
    }
    NkSamplerHandle NkTextureLibrary::GetSampler(NkTexId id) const {
        auto it=mTextures.Find(id); return it?it->Second.sampler:NkSamplerHandle{};
    }
    bool NkTextureLibrary::IsValid(NkTexId id) const { return mTextures.Find(id)!=nullptr; }

    void NkTextureLibrary::Release(NkTexId id) {
        auto rc=mRefCounts.Find(id);
        if(!rc) return;
        if(--rc->Second<=0) {
            auto it=mTextures.Find(id);
            if(it){
                if(it->Second.handle.IsValid()) mDevice->DestroyTexture(it->Second.handle);
                mTextures.Erase(id);
            }
            mRefCounts.Erase(id);
        }
    }

    void NkTextureLibrary::ReleaseAll() {
        for(auto& kv:mTextures)
            if(kv.Second.handle.IsValid()) mDevice->DestroyTexture(kv.Second.handle);
        mTextures.Clear(); mRefCounts.Clear(); mPathCache.Clear();
        for(auto& kv:mSamplerCache)
            if(kv.Second.IsValid()) mDevice->DestroySampler(kv.Second);
        mSamplerCache.Clear();
    }

    void NkTextureLibrary::CollectGarbage() {
        NkVector<NkTexId> toDelete;
        for(auto& kv:mRefCounts) if(kv.Second<=0) toDelete.PushBack(kv.First);
        for(auto id:toDelete) Release(id);
    }

    uint32 NkTextureLibrary::GetLoadedCount() const { return (uint32)mTextures.Size(); }
    uint64 NkTextureLibrary::GetGPUMemoryBytes() const {
        uint64 total=0;
        for(auto& kv:mTextures){
            auto& t=kv.Second;
            uint32 bpp=NkFormatBytesPerPixel(t.format);
            total+=bpp*(uint64)t.width*(uint64)t.height*(uint64)t.depth*t.mipLevels;
        }
        return total;
    }

    NkTexId NkTextureLibrary::LoadCubemap(const NkString paths[6], bool hdr) {
        (void)paths; (void)hdr;
        logger.Warnf("[NkTextureLibrary] LoadCubemap: not fully implemented yet\n");
        return mBlackId;
    }

    NkTexId NkTextureLibrary::LoadCubemapEquirect(const NkString& path) {
        // Charger le HDR plat puis convertir en cubemap via un compute shader
        NkTextureLoadDesc d; d.path=path; d.srgb=false;
        NkTextureInfo info; if(!LoadHDRFile(d,info)) return mBlackId;
        info.id=AllocId(); mTextures[info.id]=info; mRefCounts[info.id]=1;
        logger.Infof("[NkTextureLibrary] Equirect '%s' loaded (conversion TODO)\n",path.CStr());
        return info.id;
    }

    NkTexId NkTextureLibrary::LoadVolume(const NkString& path) {
        (void)path;
        logger.Warnf("[NkTextureLibrary] LoadVolume: not implemented\n");
        return mBlackId;
    }

    NkTexId NkTextureLibrary::CreateGradient(uint32 w,uint32 h,NkClearColor from,
                                              NkClearColor to,bool vert) {
        NkVector<uint8> px(w*h*4);
        for(uint32 y=0;y<h;y++) for(uint32 x=0;x<w;x++){
            float32 t=vert?(float32)y/h:(float32)x/w;
            uint32 idx=(y*w+x)*4;
            px[idx+0]=(uint8)((from.r+(to.r-from.r)*t)*255);
            px[idx+1]=(uint8)((from.g+(to.g-from.g)*t)*255);
            px[idx+2]=(uint8)((from.b+(to.b-from.b)*t)*255);
            px[idx+3]=(uint8)((from.a+(to.a-from.a)*t)*255);
        }
        return Upload(px.Data(),w,h,NkGPUFormat::NK_RGBA8_UNORM,false,"gradient");
    }

    NkTexId NkTextureLibrary::CreateNoise(uint32 w,uint32 h,uint32 seed) {
        NkVector<uint8> px(w*h*4);
        uint32 s=seed^0xDEADBEEF;
        for(uint32 i=0;i<w*h;i++){
            s=s*1664525u+1013904223u;
            uint8 v=(uint8)(s>>24);
            px[i*4+0]=px[i*4+1]=px[i*4+2]=v; px[i*4+3]=255;
        }
        return Upload(px.Data(),w,h,NkGPUFormat::NK_RGBA8_UNORM,false,"noise");
    }

} // namespace nkentseu
