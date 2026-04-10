// -----------------------------------------------------------------------------
// FICHIER: NKFont/Demo/NkFontDemo.cpp
// DESCRIPTION: Démo NKFont V2.1 — artéfacts corrigés + SDF + WOFF/CFF
//
// Touches :
//   +/- : augmente/diminue la taille du texte (scale GPU)
//   S   : bascule mode SDF / mode normal (rebuild de l'atlas)
//   ESC : quitte
// -----------------------------------------------------------------------------

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKFont/NkFont.h"
#include "NKFont/Core/NkFontDetect.h"
#include <math.h>
#include <string.h>

namespace nkentseu { struct NkEntryState; }
using namespace nkentseu;
using namespace nkentseu::math;

// ============================================================
// SHADERS
// ============================================================

static constexpr const char* kVert2D = R"GLSL(
#version 460 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
out vec2 vUV; out vec4 vColor;
void main(){gl_Position=vec4(aPos,0,1);vUV=aUV;vColor=aColor;}
)GLSL";

// Shader normal (mode non-SDF)
static constexpr const char* kFrag2D_Normal = R"GLSL(
#version 460 core
in vec2 vUV; in vec4 vColor; out vec4 fragColor;
layout(binding=0) uniform sampler2D uAtlas;
void main(){
    float a=texture(uAtlas,vUV).a;
    if(a<0.01)discard;
    fragColor=vec4(vColor.rgb,vColor.a*a);
}
)GLSL";

// Shader SDF (mode SDF)
static constexpr const char* kFrag2D_SDF = R"GLSL(
#version 460 core
in vec2 vUV; in vec4 vColor; out vec4 fragColor;
layout(binding=0) uniform sampler2D uAtlas;
layout(location=1) uniform float uSmoothing;
void main(){
    float dist=texture(uAtlas,vUV).r;
    float a=smoothstep(0.5-uSmoothing, 0.5+uSmoothing, dist);
    if(a<0.01) discard;
    fragColor=vec4(vColor.rgb, vColor.a*a);
}
)GLSL";

static constexpr const char* kVert3D = R"GLSL(
#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
out vec2 vUV; out vec4 vColor;
layout(std140,binding=0) uniform CamUBO{mat4 mvp;}uCam;
void main(){gl_Position=uCam.mvp*vec4(aPos,1);vUV=aUV;vColor=aColor;}
)GLSL";

// Le shader 3D utilise aussi le canal .a pour non-SDF
static constexpr const char* kFrag3D = R"GLSL(
#version 460 core
in vec2 vUV; in vec4 vColor; out vec4 fragColor;
layout(binding=0) uniform sampler2D uAtlas;
void main(){float a=texture(uAtlas,vUV).a;if(a<0.01)discard;fragColor=vec4(vColor.rgb,vColor.a*a);}
)GLSL";

struct FontVtx2D { float x, y, u, v, r, g, b, a; };
struct FontVtx3D { float x, y, z, u, v, r, g, b, a; };
struct alignas(16) CamUBO { float mvp[16]; };

// ============================================================
// NkFontRenderer
// ============================================================

class NkFontRenderer {
    public:
        static constexpr nkft_uint32 MAX_VERTS_2D = 16384u;
        static constexpr nkft_uint32 MAX_VERTS_3D = 6144u;

        NkFontAtlas   atlas;
        NkFont*       fontRef  = nullptr;
        NkFont*       font2D   = nullptr;
        NkFont*       font3D   = nullptr;
        NkFontProfile profile;
        bool          isSDF    = false;

        bool Init(NkIDevice* device, NkRenderPassHandle rp, const char* fontPath, bool sdf = false) {
            mDevice = device;
            if (!device) return false;
            mFontPath = fontPath;
            return Rebuild(rp, sdf);
        }

        // Reconstruit l'atlas (appelé lors du switch SDF/normal)
        bool Rebuild(NkRenderPassHandle rp, bool sdf) {
            if (!mDevice) return false;

            // Détruit les ressources GPU existantes
            DestroyGPU();

            // Réinitialise l'atlas
            atlas.Clear();
            isSDF = sdf;

            // Détection auto
            profile = NkFontDetector::AnalyzeFile(mFontPath.CStr());
            nkft_float32 sz2D  = NkFontDetector::SnapSize(profile, 18.f);
            nkft_float32 sz3D  = NkFontDetector::SnapSize(profile, 56.f);
            nkft_float32 szRef = NkFontDetector::SnapSize(profile, 64.f);

            NkFontConfig c2D, c3D, cRef;
            NkFontDetector::ApplyOptimalConfig(profile, sz2D, &c2D);
            NkFontDetector::ApplyOptimalConfig(profile, sz3D, &c3D);
            NkFontDetector::ApplyOptimalConfig(profile, szRef, &cRef);

            c2D.glyphRanges = c3D.glyphRanges = cRef.glyphRanges = NkFontAtlas::GetGlyphRangesDefault();

            atlas.texGlyphPadding = sdf ? 4 : 2; // padding plus grand en mode SDF
            atlas.sdfMode   = sdf;
            atlas.sdfSpread = 6;

            font2D  = atlas.AddFontFromFile(mFontPath.CStr(), sz2D,  &c2D);
            font3D  = atlas.AddFontFromFile(mFontPath.CStr(), sz3D,  &c3D);
            fontRef = atlas.AddFontFromFile(mFontPath.CStr(), szRef, &cRef);

            if (!font2D || !font3D || !fontRef) {
                logger.Info("[NkFontRenderer] AddFontFromFile FAIL\n");
                return false;
            }
            if (!atlas.Build()) {
                logger.Info("[NkFontRenderer] Build FAIL\n");
                return false;
            }

            // Upload GPU
            {
                nkft_uint8* px = nullptr;
                nkft_int32 W = 0, H = 0;
                if (sdf) {
                    // En mode SDF, le canal utile est R (pas alpha)
                    // On passe quand même en RGBA32 pour compatibilité
                    atlas.GetTexDataAsRGBA32(&px, &W, &H);
                } else {
                    atlas.GetTexDataAsRGBA32(&px, &W, &H);
                }
                if (!px || !W || !H) return false;
                NkTextureDesc td = NkTextureDesc::Tex2D((nkft_uint32)W, (nkft_uint32)H, NkGPUFormat::NK_RGBA8_UNORM);
                td.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
                td.mipLevels = 1u;
                td.debugName = "NkFontAtlasV2";
                mAtlasTex = mDevice->CreateTexture(td);
                if (!mAtlasTex.IsValid()) return false;
                mDevice->WriteTexture(mAtlasTex, px, (nkft_uint32)W * 4u);
                atlas.ClearTexData();
                logger.Info("[NkFontRenderer] Atlas GPU:{0}x{1} mode={2}\n", W, H, sdf ? "SDF" : "Normal");
            }

            // Sampler
            {
                NkSamplerDesc sd{};
                // SDF nécessite toujours LINEAR pour l'interpolation
                bool nearest = !sdf && NkFontDetector::RecommendsNearestFilter(profile);
                sd.magFilter = /*nearest ? NkFilter::NK_NEAREST : */NkFilter::NK_LINEAR;
                sd.minFilter = /*nearest ? NkFilter::NK_NEAREST : */NkFilter::NK_LINEAR;
                sd.mipFilter = NkMipFilter::NK_NONE;
                sd.addressU  = NkAddressMode::NK_CLAMP_TO_EDGE;
                sd.addressV  = NkAddressMode::NK_CLAMP_TO_EDGE;
                mSampler = mDevice->CreateSampler(sd);
                if (!mSampler.IsValid()) return false;
            }

            if (!BuildShaders(sdf)) return false;
            if (!BuildLayouts()) return false;
            if (!BuildPipelines(rp)) return false;
            if (!BuildBuffers()) return false;
            if (!BuildDescriptors()) return false;

            mEnabled = true;
            logger.Info("[NkFontRenderer] Rebuild OK mode={0}\n", sdf ? "SDF" : "Normal");
            return true;
        }

        void Shutdown() {
            DestroyGPU();
            mDevice = nullptr;
        }

        void BeginFrame() {
            mVerts2D.Clear();
            mVerts3D.Clear();
        }

        void DrawText2D(NkFont* fnt, const char* text,
                        float px, float py, float sw, float sh,
                        float r = 1, float g = 1, float b = 1, float a = 1,
                        float scale = 1.f) {
            if (!mEnabled || !fnt || !text) return;

            auto NX = [sw](float x) { return x / sw * 2.f - 1.f; };
            auto NY = [sh](float y) { return 1.f - y / sh * 2.f; };

            const char* p = text;
            float curX = px;

            while (*p) {
                NkFontCodepoint cp = NkFont::DecodeUTF8(&p, nullptr);
                if (cp == '\n') {
                    curX = px;
                    py += fnt->lineAdvance * scale;
                    continue;
                }
                const NkFontGlyph* gl = fnt->FindGlyph(cp);
                if (!gl) continue;

                if (gl->visible && (mVerts2D.Size() + 6u) <= MAX_VERTS_2D) {
                    float x0 = curX + gl->x0 * scale;
                    float y0 = py   + gl->y0 * scale;
                    float x1 = curX + gl->x1 * scale;
                    float y1 = py   + gl->y1 * scale;
                    float nx0 = NX(x0), ny0 = NY(y0);
                    float nx1 = NX(x1), ny1 = NY(y1);

                    mVerts2D.PushBack({nx0, ny0, gl->u0, gl->v0, r, g, b, a});
                    mVerts2D.PushBack({nx1, ny0, gl->u1, gl->v0, r, g, b, a});
                    mVerts2D.PushBack({nx0, ny1, gl->u0, gl->v1, r, g, b, a});
                    mVerts2D.PushBack({nx1, ny0, gl->u1, gl->v0, r, g, b, a});
                    mVerts2D.PushBack({nx1, ny1, gl->u1, gl->v1, r, g, b, a});
                    mVerts2D.PushBack({nx0, ny1, gl->u0, gl->v1, r, g, b, a});
                }
                curX += gl->advanceX * scale;
            }
        }

        void DrawText3D(NkFont* fnt, const char* text, const NkMat4f& wm,
                        float r = 1, float g = 1, float b = 1, float a = 1,
                        bool centered = true, float scale = 1.f) {
            if (!mEnabled || !fnt || !text) return;

            float tw = fnt->CalcTextSizeX(text);
            const float pxW = 0.005f * scale;
            float curX = centered ? -tw * 0.5f * pxW : 0.f;
            float curY = centered ?  fnt->ascent * 0.5f * pxW : 0.f;

            auto W = [&](float lx, float ly) {
                const float* m = wm.data;
                return NkVec3f{
                    m[0] * lx + m[4] * ly + m[12],
                    m[1] * lx + m[5] * ly + m[13],
                    m[2] * lx + m[6] * ly + m[14]
                };
            };

            const char* p = text;
            while (*p) {
                NkFontCodepoint cp = NkFont::DecodeUTF8(&p, nullptr);
                if (cp == '\n') {
                    curX = centered ? -tw * 0.5f * pxW : 0.f;
                    curY -= fnt->lineAdvance * pxW;
                    continue;
                }
                const NkFontGlyph* gl = fnt->FindGlyph(cp);
                if (!gl) continue;

                if (gl->visible && (mVerts3D.Size() + 6u) <= MAX_VERTS_3D) {
                    float lx0 = curX + gl->x0 * pxW;
                    float ly0 = curY - gl->y0 * pxW;
                    float lx1 = curX + gl->x1 * pxW;
                    float ly1 = curY - gl->y1 * pxW;

                    NkVec3f tl = W(lx0, ly0);
                    NkVec3f tr = W(lx1, ly0);
                    NkVec3f bl = W(lx0, ly1);
                    NkVec3f br = W(lx1, ly1);

                    mVerts3D.PushBack({tl.x, tl.y, tl.z, gl->u0, gl->v0, r, g, b, a});
                    mVerts3D.PushBack({tr.x, tr.y, tr.z, gl->u1, gl->v0, r, g, b, a});
                    mVerts3D.PushBack({bl.x, bl.y, bl.z, gl->u0, gl->v1, r, g, b, a});
                    mVerts3D.PushBack({tr.x, tr.y, tr.z, gl->u1, gl->v0, r, g, b, a});
                    mVerts3D.PushBack({br.x, br.y, br.z, gl->u1, gl->v1, r, g, b, a});
                    mVerts3D.PushBack({bl.x, bl.y, bl.z, gl->u0, gl->v1, r, g, b, a});
                }
                curX += gl->advanceX * pxW;
            }
        }

        void EndFrame(NkICommandBuffer* cmd, const NkMat4f& view, const NkMat4f& proj,
                    float sdfSmoothing = 0.1f) {
            if (!mEnabled || !cmd) return;

            RefreshDescriptors();

            nkft_uint32 n2 = mVerts2D.Size();
            if (n2 > 0) {
                mDevice->WriteBuffer(mVbo2D, mVerts2D.Begin(), n2 * sizeof(FontVtx2D));
                cmd->BindGraphicsPipeline(mPipe2D);
                cmd->BindDescriptorSet(mDs2D, 0u);
                // SDF smoothing uniform
                if (isSDF) {
                    // cmd->SetPushConstant(1, &sdfSmoothing, sizeof(float));
                }
                cmd->BindVertexBuffer(0, mVbo2D);
                cmd->Draw(n2);
            }

            nkft_uint32 n3 = mVerts3D.Size();
            if (n3 > 0) {
                mDevice->WriteBuffer(mVbo3D, mVerts3D.Begin(), n3 * sizeof(FontVtx3D));
                CamUBO cam;
                NkMat4f mvp = proj * view;
                for (int i = 0; i < 16; ++i) cam.mvp[i] = mvp.data[i];
                mDevice->WriteBuffer(mUbo3D, &cam, sizeof(cam));
                cmd->BindGraphicsPipeline(mPipe3D);
                cmd->BindDescriptorSet(mDs3D, 0u);
                cmd->BindVertexBuffer(0, mVbo3D);
                cmd->Draw(n3);
            }
        }

        bool IsEnabled() const { return mEnabled; }

    private:
        NkIDevice*   mDevice = nullptr;
        bool         mEnabled = false;
        NkString     mFontPath;

        NkTextureHandle  mAtlasTex;
        NkSamplerHandle  mSampler;
        NkShaderHandle   mShd2D, mShd3D;
        NkDescSetHandle  mDsl2D, mDsl3D, mDs2D, mDs3D;
        NkPipelineHandle mPipe2D, mPipe3D;
        NkBufferHandle   mVbo2D, mVbo3D, mUbo3D;
        NkVector<FontVtx2D> mVerts2D;
        NkVector<FontVtx3D> mVerts3D;

        void DestroyGPU() {
            if (!mDevice) {
                mEnabled = false;
                return;
            }
            auto d = mDevice;

            if (mDs2D.IsValid())    d->FreeDescriptorSet(mDs2D);
            if (mDs3D.IsValid())    d->FreeDescriptorSet(mDs3D);
            if (mDsl2D.IsValid())   d->DestroyDescriptorSetLayout(mDsl2D);
            if (mDsl3D.IsValid())   d->DestroyDescriptorSetLayout(mDsl3D);
            if (mPipe2D.IsValid())  d->DestroyPipeline(mPipe2D);
            if (mPipe3D.IsValid())  d->DestroyPipeline(mPipe3D);
            if (mShd2D.IsValid())   d->DestroyShader(mShd2D);
            if (mShd3D.IsValid())   d->DestroyShader(mShd3D);
            if (mVbo2D.IsValid())   d->DestroyBuffer(mVbo2D);
            if (mVbo3D.IsValid())   d->DestroyBuffer(mVbo3D);
            if (mUbo3D.IsValid())   d->DestroyBuffer(mUbo3D);
            if (mSampler.IsValid()) d->DestroySampler(mSampler);
            if (mAtlasTex.IsValid()) d->DestroyTexture(mAtlasTex);

            mEnabled = false;
        }

        bool BuildShaders(bool sdf) {
            {
                NkShaderDesc sd;
                sd.debugName = "FS2D";
                sd.AddGLSL(NkShaderStage::NK_VERTEX, kVert2D);
                sd.AddGLSL(NkShaderStage::NK_FRAGMENT, sdf ? kFrag2D_SDF : kFrag2D_Normal);
                mShd2D = mDevice->CreateShader(sd);
                if (!mShd2D.IsValid()) return false;
            }
            {
                NkShaderDesc sd;
                sd.debugName = "FS3D";
                sd.AddGLSL(NkShaderStage::NK_VERTEX, kVert3D);
                sd.AddGLSL(NkShaderStage::NK_FRAGMENT, kFrag3D);
                mShd3D = mDevice->CreateShader(sd);
                if (!mShd3D.IsValid()) return false;
            }
            return true;
        }

        bool BuildLayouts() {
            {
                NkDescriptorSetLayoutDesc l;
                l.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
                mDsl2D = mDevice->CreateDescriptorSetLayout(l);
                if (!mDsl2D.IsValid()) return false;
            }
            {
                NkDescriptorSetLayoutDesc l;
                l.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
                l.Add(1, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_VERTEX);
                mDsl3D = mDevice->CreateDescriptorSetLayout(l);
                if (!mDsl3D.IsValid()) return false;
            }
            return true;
        }

        bool BuildPipelines(NkRenderPassHandle rp) {
            NkVertexLayout vl2D;
            vl2D.AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT, 0u, "POSITION", 0)
                .AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT, 2 * sizeof(float), "TEXCOORD", 0)
                .AddAttribute(2, 0, NkGPUFormat::NK_RGBA32_FLOAT, 4 * sizeof(float), "COLOR", 0)
                .AddBinding(0, sizeof(FontVtx2D));

            NkVertexLayout vl3D;
            vl3D.AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT, 0u, "POSITION", 0)
                .AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT, 3 * sizeof(float), "TEXCOORD", 0)
                .AddAttribute(2, 0, NkGPUFormat::NK_RGBA32_FLOAT, 5 * sizeof(float), "COLOR", 0)
                .AddBinding(0, sizeof(FontVtx3D));

            {
                NkDepthStencilDesc ds = NkDepthStencilDesc::Default();
                ds.depthTestEnable  = false;
                ds.depthWriteEnable = false;
                NkRasterizerDesc rast = NkRasterizerDesc::Default();
                rast.cullMode = NkCullMode::NK_NONE;
                NkGraphicsPipelineDesc pd;
                pd.shader = mShd2D;
                pd.vertexLayout = vl2D;
                pd.topology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
                pd.rasterizer = rast;
                pd.depthStencil = ds;
                pd.blend = NkBlendDesc::Alpha();
                pd.renderPass = rp;
                pd.debugName = "FP2D";
                pd.descriptorSetLayouts.PushBack(mDsl2D);
                mPipe2D = mDevice->CreateGraphicsPipeline(pd);
                if (!mPipe2D.IsValid()) return false;
            }
            {
                NkDepthStencilDesc ds = NkDepthStencilDesc::Default();
                ds.depthTestEnable  = true;
                ds.depthWriteEnable = false;
                NkRasterizerDesc rast = NkRasterizerDesc::Default();
                rast.cullMode = NkCullMode::NK_NONE;
                NkGraphicsPipelineDesc pd;
                pd.shader = mShd3D;
                pd.vertexLayout = vl3D;
                pd.topology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
                pd.rasterizer = rast;
                pd.depthStencil = ds;
                pd.blend = NkBlendDesc::Alpha();
                pd.renderPass = rp;
                pd.debugName = "FP3D";
                pd.descriptorSetLayouts.PushBack(mDsl3D);
                mPipe3D = mDevice->CreateGraphicsPipeline(pd);
                if (!mPipe3D.IsValid()) return false;
            }
            return true;
        }

        bool BuildBuffers() {
            {
                NkBufferDesc bd = NkBufferDesc::Vertex(MAX_VERTS_2D * sizeof(FontVtx2D), nullptr);
                bd.usage = NkResourceUsage::NK_UPLOAD;
                bd.debugName = "FVbo2D";
                mVbo2D = mDevice->CreateBuffer(bd);
                if (!mVbo2D.IsValid()) return false;
            }
            {
                NkBufferDesc bd = NkBufferDesc::Vertex(MAX_VERTS_3D * sizeof(FontVtx3D), nullptr);
                bd.usage = NkResourceUsage::NK_UPLOAD;
                bd.debugName = "FVbo3D";
                mVbo3D = mDevice->CreateBuffer(bd);
                if (!mVbo3D.IsValid()) return false;
            }
            mUbo3D = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(CamUBO)));
            if (!mUbo3D.IsValid()) return false;
            mVerts2D.Reserve(MAX_VERTS_2D);
            mVerts3D.Reserve(MAX_VERTS_3D);
            return true;
        }

        bool BuildDescriptors() {
            mDs2D = mDevice->AllocateDescriptorSet(mDsl2D);
            mDs3D = mDevice->AllocateDescriptorSet(mDsl3D);
            if (!mDs2D.IsValid() || !mDs3D.IsValid()) return false;

            RefreshDescriptors();

            if (mUbo3D.IsValid()) {
                NkDescriptorWrite w{};
                w.set = mDs3D;
                w.binding = 1u;
                w.type = NkDescriptorType::NK_UNIFORM_BUFFER;
                w.buffer = mUbo3D;
                w.bufferRange = sizeof(CamUBO);
                mDevice->UpdateDescriptorSets(&w, 1u);
            }
            return true;
        }

        void RefreshDescriptors() {
            if (!mAtlasTex.IsValid() || !mSampler.IsValid()) return;

            auto Bind = [&](NkDescSetHandle ds, nkft_uint32 slot) {
                NkDescriptorWrite w{};
                w.set = ds;
                w.binding = slot;
                w.type = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
                w.texture = mAtlasTex;
                w.sampler = mSampler;
                w.textureLayout = NkResourceState::NK_SHADER_READ;
                mDevice->UpdateDescriptorSets(&w, 1u);
            };
            Bind(mDs2D, 0u);
            Bind(mDs3D, 0u);
        }
};

// ============================================================
// nkmain
// ============================================================

int nkmain(const NkEntryState& state) {
    NkWindowConfig wc;
    wc.title = "NKFont V2.1 — Fix artefacts + SDF(S) + WOFF/CFF";
    wc.width = 1280;
    wc.height = 720;
    wc.centered = true;
    wc.resizable = true;

    NkWindow window;
    if (!window.Create(wc)) return 1;

    NkDeviceInitInfo di;
    di.api = NkGraphicsApi::NK_API_OPENGL;
    di.surface = window.GetSurfaceDesc();
    di.width = window.GetSize().width;
    di.height = window.GetSize().height;
    di.context.vulkan.appName = "NkFontDemo";
    di.context.vulkan.engineName = "Nkentseu";

    NkIDevice* device = NkDeviceFactory::Create(di);
    if (!device || !device->IsValid()) {
        window.Close();
        return 1;
    }

    nkft_uint32 W = device->GetSwapchainWidth();
    nkft_uint32 H = device->GetSwapchainHeight();

    NkICommandBuffer* cmd = device->CreateCommandBuffer();
    if (!cmd) return 1;

    NkFontRenderer* fr = new NkFontRenderer();
    // if (!fr->Init(device, device->GetSwapchainRenderPass(), "Resources/Fonts/ProggyClean.ttf", false)) {
    if (!fr->Init(device, device->GetSwapchainRenderPass(), "Resources/Fonts/Roboto-Regular.ttf", false)) {
        delete fr;
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    bool running = true;
    bool pendingRebuild = false;
    bool nextSDF = false;
    auto& ev = NkEvents();

    float textScale = 1.f, targetSize = 18.f;
    if (fr->fontRef && fr->fontRef->fontSize > 0) {
        textScale = targetSize / fr->fontRef->fontSize;
    }

    ev.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) {
        running = false;
    });

    ev.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        auto k = e->GetKey();
        if (k == NkKey::NK_ESCAPE) running = false;

        if (k == NkKey::NK_NUMPAD_ADD || k == NkKey::NK_EQUALS) {
            targetSize = fminf(targetSize + 2.f, 128.f);
            if (fr->fontRef && fr->fontRef->fontSize > 0) {
                textScale = targetSize / fr->fontRef->fontSize;
            }
        }
        if (k == NkKey::NK_NUMPAD_SUB || k == NkKey::NK_MINUS) {
            targetSize = fmaxf(targetSize - 2.f, 6.f);
            if (fr->fontRef && fr->fontRef->fontSize > 0) {
                textScale = targetSize / fr->fontRef->fontSize;
            }
        }
        if (k == NkKey::NK_S) {
            pendingRebuild = true;
            nextSDF = !fr->isSDF;
        }
    });

    ev.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        W = (nkft_uint32)e->GetWidth();
        H = (nkft_uint32)e->GetHeight();
    });

    NkChrono timer;
    double tPrev = timer.Elapsed().seconds;
    double fpsAcc = 0;
    int fpsCnt = 0;
    float fpsDisp = 0, rot = 0, bob = 0;

    logger.Info("[FontDemo] +/- taille | S=toggle SDF\n");

    while (running) {
        double now = timer.Elapsed().seconds;
        float dt = (float)(now - tPrev);
        tPrev = now;

        fpsAcc += dt;
        ++fpsCnt;
        if (fpsAcc >= 0.5) {
            fpsDisp = (float)fpsCnt / (float)fpsAcc;
            fpsAcc = 0;
            fpsCnt = 0;
        }
        rot += 20 * dt;
        bob += 80 * dt;

        ev.PollEvents();
        if (!running) break;

        if (W != device->GetSwapchainWidth() || H != device->GetSwapchainHeight()) {
            device->OnResize(W, H);
        }

        // Rebuild si switch SDF
        if (pendingRebuild) {
            pendingRebuild = false;
            device->WaitIdle();
            fr->Rebuild(device->GetSwapchainRenderPass(), nextSDF);
            if (fr->fontRef && fr->fontRef->fontSize > 0) {
                textScale = targetSize / fr->fontRef->fontSize;
            }
        }

        NkMat4f view = NkMat4f::LookAt(NkVec3f(0, 2, 5), NkVec3f(0, 0, 0), NkVec3f(0, 1, 0));
        NkMat4f proj = NkMat4f::Perspective(NkAngle(60.f), (float)W / (float)H, 0.05f, 100.f);

        NkFrameContext frame;
        if (!device->BeginFrame(frame)) continue;
        W = device->GetSwapchainWidth();
        H = device->GetSwapchainHeight();
        if (!W || !H) {
            device->EndFrame(frame);
            continue;
        }

        NkFramebufferHandle fbo = device->GetSwapchainFramebuffer();
        NkRenderPassHandle  rp  = device->GetSwapchainRenderPass();

        cmd->Reset();
        if (!cmd->Begin()) {
            device->EndFrame(frame);
            continue;
        }
        if (!cmd->BeginRenderPass(rp, fbo, NkRect2D{0, 0, (int)W, (int)H})) {
            cmd->End();
            device->EndFrame(frame);
            continue;
        }
        cmd->SetViewport(NkViewport(0, 0, (float)W, (float)H));
        cmd->SetScissor(NkRect2D(0u, 0u, W, H));

        fr->BeginFrame();

        // 3D
        {
            float y = 1.5f + 0.12f * NkSin(NkToRadians(bob));
            // NkMat4f w = NkMat4f::Translation(NkVec3f(0, y, 0)) * NkMat4f::RotationY(NkAngle(rot));
            NkMat4f w = NkMat4f::RotationY(NkAngle(rot));
            fr->DrawText3D(fr->font3D, "NKENTSEU", w, 1.0f, 1.0f, 1.0f, 1.0f, true, 10);
        }
        {
            // NkMat4f w = NkMat4f::Translation(NkVec3f(0, -0.3f, 0.5f)) * NkMat4f::RotationX(NkAngle(-90));
            NkMat4f w = NkMat4f::RotationX(NkAngle(-90));
            fr->DrawText3D(fr->font3D, "NKFont V2.1", w, 1.0f, 1.0f, 1.0f, 1.0f, true, 10);
        }

        // HUD 2D — avec échelle dynamique
        const char* modeStr = fr->isSDF ? "SDF (net a toute taille)" : "Normal (bitmap atlas)";
        const char* kindStr = (fr->profile.kind == NkFontKind::Bitmap) ? "Bitmap" : "Vectorielle TTF";
        float lineH = fr->fontRef ? fr->fontRef->lineAdvance * textScale : 20.f;
        float sdfSmooth = fr->isSDF ? 0.2f / (targetSize * textScale) : 0.f;

        {
            NkString s = NkFormat("FPS:{0:.0f} | {1} | {2} | {3:.0f}px | +/- taille | S=SDF", fpsDisp, kindStr, modeStr, targetSize);
            fr->DrawText2D(fr->fontRef, s.CStr(), 12, 12, (float)W, (float)H, 1, 1, 0.3f, 1, textScale);
        }
        fr->DrawText2D(fr->fontRef, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 12, 12 + lineH, (float)W, (float)H, 1, 1, 1, 1, textScale);
        fr->DrawText2D(fr->fontRef, "abcdefghijklmnopqrstuvwxyz 0123456789", 12, 12 + lineH * 2, (float)W, (float)H, 0.9f, 0.9f, 0.9f, 1, textScale);
        fr->DrawText2D(fr->fontRef, "Bonjour le monde! Hello World! @#$%", 12, 12 + lineH * 3, (float)W, (float)H, 1, 0.5f, 0.5f, 1, textScale);
        fr->DrawText2D(fr->fontRef, "Fix: advanceX/oversampleH + rasteŕiseur correct", 12, 12 + lineH * 4, (float)W, (float)H, 0.6f, 0.9f, 1, 1, textScale);
        fr->DrawText2D(fr->fontRef, "WOFF + OTF/CFF Type2 + SDF mode", 12, 12 + lineH * 5, (float)W, (float)H, 0.7f, 1, 0.7f, 1, textScale);
        fr->DrawText2D(fr->font2D, "ESC:quit  +/-:size  S:SDF mode", 12, (float)H - 26, (float)W, (float)H, 0.5f, 0.5f, 0.5f, 0.8f, 1);

        fr->EndFrame(cmd, view, proj, sdfSmooth);
        cmd->EndRenderPass();
        cmd->End();
        device->SubmitAndPresent(cmd);
        device->EndFrame(frame);
    }

    device->WaitIdle();
    fr->Shutdown();
    delete fr;
    device->DestroyCommandBuffer(cmd);
    NkDeviceFactory::Destroy(device);
    window.Close();
    return 0;
}

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================