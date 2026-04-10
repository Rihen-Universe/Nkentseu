#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"
#include "NKContainers/Sequential/NkVector.h"

#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

#include "NKFont/NkFont.h"
#include "NKUI/NkUIFont.h"
#include "NKUI/NkUIFontBridge.h"

#include <cstdio>
#include <cstring>
#include <cmath>

namespace nkentseu { struct NkEntryState; }
using namespace nkentseu;
using namespace nkentseu::nkui;

// =============================================================================
//  SHADERS
// =============================================================================

static constexpr const char* kTextVS = R"GLSL(
#version 460 core
layout(location=0) in vec2  aPos;
layout(location=1) in vec2  aUV;
layout(location=2) in vec4  aCol;
out vec2 vUV;
out vec4 vCol;
layout(std140, binding=0) uniform ScreenUBO {
    vec2 invScreen;
};
void main() {
    vec2 ndc = aPos * invScreen - vec2(1.0, 1.0);
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = aUV;
    vCol = aCol;
}
)GLSL";

static constexpr const char* kTextFS = R"GLSL(
#version 460 core
in  vec2 vUV;
in  vec4 vCol;
out vec4 fragColor;
layout(binding=0) uniform sampler2D uAtlas;
void main() {
    // L'atlas est uploadé en RGBA8 (conversion Gray8→RGBA8 faite côté CPU).
    // Le canal alpha contient la couverture du glyphe.
    float alpha = texture(uAtlas, vUV).a;
    if (alpha < 0.01) discard;
    fragColor = vec4(vCol.rgb, vCol.a * alpha);
}
)GLSL";

// =============================================================================
//  Structs
// =============================================================================

struct TextVtx {
    float x, y;
    float u, v;
    float r, g, b, a;
};

struct alignas(16) ScreenUBO {
    float invW, invH, _pad0, _pad1;
};

// =============================================================================
//  NkFontRenderer2D
// =============================================================================

class NkFontRenderer2D {
public:
    static constexpr uint32 MAX_VERTS = 32768u;

    // Polices exposées (pointeurs vers les slots internes)
    NkUIFont* fontSmall  = nullptr;
    NkUIFont* fontUI     = nullptr;
    NkUIFont* fontMedium = nullptr;
    NkUIFont* fontBig    = nullptr;

    NkUIFontAtlas atlas;

    // ─── Init ────────────────────────────────────────────────────────────────
    bool init(NkIDevice* dev, NkRenderPassHandle swapRP, const char* fontPath)
    {
        mDev = dev;
        if (!dev || !fontPath) return false;

        // 1. Atlas
        atlas.Clear();
        atlas.yAxisUp = false;

        // 2. Charge les 4 tailles via NkUIFontBridge
        //    Les bridges et les NkUIFont sont stockés en membres pour que leurs
        //    durées de vie correspondent à celle du renderer.
        struct Entry { float size; NkUIFont** out; };
        const Entry entries[] = {
            { 12.f, &fontSmall  },
            { 16.f, &fontUI     },
            { 20.f, &fontMedium },
            { 32.f, &fontBig    },
        };

        bool anyOk = false;
        for (int i = 0; i < 4; ++i) {
            mFonts[i] = NkUIFont{};
            if (mBridges[i].InitFromFile(&atlas, &mFonts[i],
                                          fontPath, entries[i].size,
                                          NkUIFontBridge::RangesDefault()))
            {
                *entries[i].out = &mFonts[i];
                anyOk = true;
                logger.Info("[NkUIDemo] Police {0}px OK\n", (int)entries[i].size);
            } else {
                logger.Warn("[NkUIDemo] Police {0}px échec: {1}\n",
                            (int)entries[i].size, fontPath);
            }
        }

        if (!anyOk) {
            // Fallback bitmap intégré : on configure une NkUIFont sans atlas
            logger.Warn("[NkUIDemo] Aucune police TTF — fallback bitmap\n");
            mFonts[0] = NkUIFont{};
            mFonts[0].size      = 16.f;
            mFonts[0].isBuiltin = true;
            mFonts[0].atlas     = nullptr;
            mFonts[0].metrics.ascender   = 12.f;
            mFonts[0].metrics.descender  = 4.f;
            mFonts[0].metrics.lineHeight = 20.f;
            mFonts[0].metrics.spaceWidth = 8.f;
            fontSmall = fontUI = fontMedium = fontBig = &mFonts[0];
        }

        // Note : les bridges restent vivants (membres de NkFontRenderer2D).
        // L'atlas est rempli par InitFromFile — on ne détruit PAS les bridges ici.

        // 3. Upload atlas → GPU (Gray8 → RGBA8)
        {
            // Vérifie qu'il y a des pixels
            bool hasPixels = false;
            const int atlasTotal = NkUIFontAtlas::ATLAS_W * NkUIFontAtlas::ATLAS_H;
            for (int i = 0; i < atlasTotal && !hasPixels; ++i)
                if (atlas.pixels[i]) hasPixels = true;

            if (hasPixels) {
                // Convertit Gray8 → RGBA8 (blanc + alpha = couverture)
                uint8* rgba = new uint8[atlasTotal * 4];
                for (int i = 0; i < atlasTotal; ++i) {
                    rgba[i*4+0] = 255;
                    rgba[i*4+1] = 255;
                    rgba[i*4+2] = 255;
                    rgba[i*4+3] = atlas.pixels[i]; // alpha = couverture
                }

                NkTextureDesc td = NkTextureDesc::Tex2D(
                    NkUIFontAtlas::ATLAS_W, NkUIFontAtlas::ATLAS_H,
                    NkGPUFormat::NK_RGBA8_UNORM);
                td.bindFlags  = NkBindFlags::NK_SHADER_RESOURCE;
                td.mipLevels  = 1;
                td.debugName  = "NkUIFontAtlas";
                mAtlasTex = dev->CreateTexture(td);

                if (!mAtlasTex.IsValid()) {
                    delete[] rgba;
                    logger.Error("[NkUIDemo] CreateTexture atlas FAIL\n");
                    return false;
                }
                // stride = largeur × 4 octets/pixel
                dev->WriteTexture(mAtlasTex, rgba,
                                  static_cast<uint32>(NkUIFontAtlas::ATLAS_W * 4));
                delete[] rgba;

                // Assigne un texId non-nul pour que RenderCharAtlas fonctionne
                atlas.texId = 1u;
                atlas.dirty = false;

                logger.Info("[NkUIDemo] Atlas GPU: {0}x{1}, {2} glyphes\n",
                            NkUIFontAtlas::ATLAS_W, NkUIFontAtlas::ATLAS_H,
                            atlas.numGlyphs);
            } else {
                logger.Warn("[NkUIDemo] Atlas vide — rendu bitmap uniquement\n");
            }
        }

        // 4. Sampler
        {
            NkSamplerDesc sd{};
            sd.magFilter = NkFilter::NK_LINEAR;
            sd.minFilter = NkFilter::NK_LINEAR;
            sd.mipFilter = NkMipFilter::NK_NONE;
            sd.addressU  = NkAddressMode::NK_CLAMP_TO_EDGE;
            sd.addressV  = NkAddressMode::NK_CLAMP_TO_EDGE;
            mSampler = dev->CreateSampler(sd);
            if (!mSampler.IsValid()) return false;
        }

        // 5. Pipeline
        if (!buildShader())         return false;
        if (!buildLayouts())        return false;
        if (!buildPipeline(swapRP)) return false;
        if (!buildBuffers())        return false;
        buildDescriptors();

        mOk = true;
        logger.Info("[NkUIDemo] NkFontRenderer2D OK\n");
        return true;
    }

    void shutdown()
    {
        if (!mDev) return;
        auto& d = *mDev;
        if (mDs.IsValid())       d.FreeDescriptorSet(mDs);
        if (mDsl.IsValid())      d.DestroyDescriptorSetLayout(mDsl);
        if (mPipe.IsValid())     d.DestroyPipeline(mPipe);
        if (mShd.IsValid())      d.DestroyShader(mShd);
        if (mVbo.IsValid())      d.DestroyBuffer(mVbo);
        if (mUbo.IsValid())      d.DestroyBuffer(mUbo);
        if (mSampler.IsValid())  d.DestroySampler(mSampler);
        if (mAtlasTex.IsValid()) d.DestroyTexture(mAtlasTex);
        // Détruit les bridges en dernier (ils possèdent les données TTF)
        for (int i = 0; i < 4; ++i) mBridges[i].Destroy();
        mOk  = false;
        mDev = nullptr;
    }

    // ─── Frame ───────────────────────────────────────────────────────────────

    void beginFrame(uint32 W, uint32 H)
    {
        mW = W; mH = H;
        mVerts.Resize(0); // vide sans désallouer
    }

    // pos.y = baseline
    void text(NkUIFont* fnt, const char* str,
              float px, float py,
              float r=1.f, float gc=1.f, float b=1.f, float a=1.f,
              float scale=1.f)
    {
        if (!mOk || !fnt || !str) return;

        float curX    = px;
        float baseline= py;

        const char* p = str;
        while (*p) {
            uint32 cp  = 0;
            int    bytes = decodeUTF8(p, cp);
            if (bytes <= 0) break;
            p += bytes;

            if (cp == '\n') {
                curX = px;
                baseline += fnt->metrics.lineHeight * scale;
                continue;
            }
            if (cp == ' ')  { curX += fnt->metrics.spaceWidth * scale; continue; }
            if (cp == '\t') { curX += fnt->metrics.tabWidth   * scale; continue; }

            const NkUIGlyph* g = fnt->atlas ? fnt->atlas->Find(cp) : nullptr;

            if (g && g->x1 > g->x0 && g->y1 > g->y0) {
                float top  = baseline - g->bearingY * scale;
                float left = curX     + g->bearingX * scale;
                float gw   = (g->x1 - g->x0) * scale;
                float gh   = (g->y1 - g->y0) * scale;
                pushQuad(left, top, gw, gh,
                         g->u0, g->v0, g->u1, g->v1,
                         r, gc, b, a);
                curX += g->advanceX * scale;
            } else {
                // Espace-placeholder si pas de glyphe dans l'atlas
                curX += (fnt->metrics.spaceWidth > 0.f
                         ? fnt->metrics.spaceWidth : fnt->size * 0.5f) * scale;
            }
        }
    }

    // Coin haut-gauche → convertit Y automatiquement
    void textAt(NkUIFont* fnt, const char* str,
                float left, float top,
                float r=1.f, float gc=1.f, float b=1.f, float a=1.f,
                float scale=1.f)
    {
        if (!fnt) return;
        float asc = fnt->metrics.ascender > 0.f ? fnt->metrics.ascender : fnt->size * 0.8f;
        text(fnt, str, left, top + asc * scale, r, gc, b, a, scale);
    }

    void flush(NkICommandBuffer* cmd)
    {
        if (!mOk || !cmd || mVerts.Empty()) return;

        ScreenUBO ubo;
        ubo.invW = 2.f / static_cast<float>(mW);
        ubo.invH = 2.f / static_cast<float>(mH);
        ubo._pad0 = ubo._pad1 = 0.f;
        mDev->WriteBuffer(mUbo, &ubo, sizeof(ubo));

        rebindDescriptors();

        uint32 n = static_cast<uint32>(mVerts.Size());
        mDev->WriteBuffer(mVbo, mVerts.Data(), n * sizeof(TextVtx));

        cmd->BindGraphicsPipeline(mPipe);
        cmd->BindDescriptorSet(mDs, 0u);
        cmd->BindVertexBuffer(0, mVbo);
        cmd->Draw(n);

        mVerts.Resize(0);
    }

    bool isOk() const { return mOk; }

private:
    NkIDevice*    mDev = nullptr;
    bool          mOk  = false;
    uint32        mW   = 0, mH = 0;

    // Polices et bridges : membres pour garantir la durée de vie
    NkUIFont       mFonts[4]   = {};
    NkUIFontBridge mBridges[4] = {};

    NkTextureHandle  mAtlasTex;
    NkSamplerHandle  mSampler;
    NkShaderHandle   mShd;
    NkDescSetHandle  mDsl, mDs;
    NkPipelineHandle mPipe;
    NkBufferHandle   mVbo, mUbo;

    NkVector<TextVtx> mVerts;

    static int decodeUTF8(const char* s, uint32& cp)
    {
        const uint8* u = reinterpret_cast<const uint8*>(s);
        if (!u || !*u) { cp = 0; return 0; }
        uint8 c0 = u[0];
        if (c0 < 0x80)                          { cp = c0; return 1; }
        if ((c0&0xE0)==0xC0 && u[1])            { cp=((c0&0x1F)<<6)|(u[1]&0x3F); return 2; }
        if ((c0&0xF0)==0xE0 && u[1] && u[2])    { cp=((c0&0x0F)<<12)|((u[1]&0x3F)<<6)|(u[2]&0x3F); return 3; }
        if ((c0&0xF8)==0xF0 && u[1]&&u[2]&&u[3]){ cp=((c0&0x07)<<18)|((u[1]&0x3F)<<12)|((u[2]&0x3F)<<6)|(u[3]&0x3F); return 4; }
        cp = '?'; return 1;
    }

    void pushQuad(float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1,
                  float r, float g, float b, float a)
    {
        if (mVerts.Size() + 6u > MAX_VERTS) return;
        const float x1 = x+w, y1 = y+h;
        mVerts.PushBack({x,  y,  u0, v0, r, g, b, a});
        mVerts.PushBack({x1, y,  u1, v0, r, g, b, a});
        mVerts.PushBack({x,  y1, u0, v1, r, g, b, a});
        mVerts.PushBack({x1, y,  u1, v0, r, g, b, a});
        mVerts.PushBack({x1, y1, u1, v1, r, g, b, a});
        mVerts.PushBack({x,  y1, u0, v1, r, g, b, a});
    }

    bool buildShader() {
        NkShaderDesc sd;
        sd.debugName = "NkUITextShader";
        sd.AddGLSL(NkShaderStage::NK_VERTEX,   kTextVS);
        sd.AddGLSL(NkShaderStage::NK_FRAGMENT, kTextFS);
        mShd = mDev->CreateShader(sd);
        return mShd.IsValid();
    }

    bool buildLayouts() {
        NkDescriptorSetLayoutDesc ld;
        ld.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
        ld.Add(1, NkDescriptorType::NK_UNIFORM_BUFFER,         NkShaderStage::NK_VERTEX);
        mDsl = mDev->CreateDescriptorSetLayout(ld);
        return mDsl.IsValid();
    }

    bool buildPipeline(NkRenderPassHandle rp) {
        NkVertexLayout vl;
        vl.AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT,   0,        "POSITION", 0)
          .AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT,   2*sizeof(float), "TEXCOORD", 0)
          .AddAttribute(2, 0, NkGPUFormat::NK_RGBA32_FLOAT, 4*sizeof(float), "COLOR",    0)
          .AddBinding(0, sizeof(TextVtx));

        NkGraphicsPipelineDesc pd;
        pd.shader       = mShd;
        pd.vertexLayout = vl;
        pd.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
        pd.blend        = NkBlendDesc::Alpha();
        pd.depthStencil = NkDepthStencilDesc::NoDepth();
        pd.rasterizer   = NkRasterizerDesc::NoCull();
        pd.renderPass   = rp;
        pd.debugName    = "NkUITextPipeline";
        pd.descriptorSetLayouts.PushBack(mDsl);
        mPipe = mDev->CreateGraphicsPipeline(pd);
        return mPipe.IsValid();
    }

    bool buildBuffers() {
        // VBO dynamique — VertexDynamic est la surcharge correcte pour les uploads CPU
        mVbo = mDev->CreateBuffer(NkBufferDesc::VertexDynamic(MAX_VERTS * sizeof(TextVtx)));
        if (!mVbo.IsValid()) return false;
        mUbo = mDev->CreateBuffer(NkBufferDesc::Uniform(sizeof(ScreenUBO)));
        if (!mUbo.IsValid()) return false;
        mVerts.Reserve(MAX_VERTS);
        return true;
    }

    void buildDescriptors() {
        mDs = mDev->AllocateDescriptorSet(mDsl);
        rebindDescriptors();
    }

    void rebindDescriptors() {
        if (!mDs.IsValid()) return;
        if (mAtlasTex.IsValid() && mSampler.IsValid()) {
            NkDescriptorWrite w{};
            w.set = mDs; w.binding = 0u;
            w.type = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            w.texture = mAtlasTex; w.sampler = mSampler;
            w.textureLayout = NkResourceState::NK_SHADER_READ;
            mDev->UpdateDescriptorSets(&w, 1u);
        }
        if (mUbo.IsValid()) {
            NkDescriptorWrite w{};
            w.set = mDs; w.binding = 1u;
            w.type = NkDescriptorType::NK_UNIFORM_BUFFER;
            w.buffer = mUbo; w.bufferRange = sizeof(ScreenUBO);
            mDev->UpdateDescriptorSets(&w, 1u);
        }
    }
};

// =============================================================================
//  TextPrinter
// =============================================================================

struct TextPrinter {
    NkFontRenderer2D* fr          = nullptr;
    float             lineSpacing = 1.2f;
    float             y           = 0.f;

    void reset(float startY) { y = startY; }

    void line(NkUIFont* fnt, const char* txt, float x=10.f,
              float r=1.f, float g=1.f, float b=1.f, float a=1.f,
              float scale=1.f)
    {
        if (!fr || !fnt || !txt) return;
        fr->textAt(fnt, txt, x, y, r, g, b, a, scale);
        float lineH = fnt->metrics.lineHeight > 0.f ? fnt->metrics.lineHeight : fnt->size;
        y += lineH * scale * lineSpacing;
    }

    void nl(NkUIFont* fnt, float factor=1.f) {
        if (!fnt) return;
        float lineH = fnt->metrics.lineHeight > 0.f ? fnt->metrics.lineHeight : fnt->size;
        y += lineH * factor;
    }
};

// =============================================================================
//  nkmain
// =============================================================================

int nkmain(const NkEntryState& /*state*/)
{
    NkWindowConfig wc;
    wc.title    = "NkUI — Intégration NKFont";
    wc.width    = 1280;
    wc.height   = 720;
    wc.centered = true;
    wc.resizable= true;

    NkWindow window;
    if (!window.Create(wc)) { logger.Error("[NkUIDemo] Fenêtre échec\n"); return 1; }

    NkDeviceInitInfo di;
    di.api     = NkGraphicsApi::NK_API_OPENGL;
    di.surface = window.GetSurfaceDesc();
    di.width   = window.GetSize().width;
    di.height  = window.GetSize().height;

    NkIDevice* dev = NkDeviceFactory::Create(di);
    if (!dev || !dev->IsValid()) {
        logger.Error("[NkUIDemo] Device échec\n"); window.Close(); return 1;
    }

    uint32 W = dev->GetSwapchainWidth();
    uint32 H = dev->GetSwapchainHeight();

    NkICommandBuffer* cmd = dev->CreateCommandBuffer();
    if (!cmd) { logger.Error("[NkUIDemo] CommandBuffer échec\n"); return 1; }

    // ── NkFontRenderer2D ─────────────────────────────────────────────────────
    NkFontRenderer2D* fr = new NkFontRenderer2D();

    static const char* kFontPaths[] = {
        "Resources/Fonts/Roboto-Regular.ttf",
        "Resources/Fonts/DroidSans.ttf",
        "Resources/Fonts/Karla-Regular.ttf",
        "Resources/Fonts/ProggyClean.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        nullptr
    };

    bool frOk = false;
    for (int i = 0; kFontPaths[i] && !frOk; ++i) {
        frOk = fr->init(dev, dev->GetSwapchainRenderPass(), kFontPaths[i]);
        if (frOk) logger.Info("[NkUIDemo] Police chargée: {0}\n", kFontPaths[i]);
    }

    if (!frOk) {
        logger.Error("[NkUIDemo] Impossible de charger une police\n");
        delete fr;
        NkDeviceFactory::Destroy(dev);
        window.Close();
        return 1;
    }

    NkUIFont* fSmall  = fr->fontSmall  ? fr->fontSmall  : fr->fontUI;
    NkUIFont* fUI     = fr->fontUI     ? fr->fontUI     : fSmall;
    NkUIFont* fMedium = fr->fontMedium ? fr->fontMedium : fUI;
    NkUIFont* fBig    = fr->fontBig    ? fr->fontBig    : fUI;

    // ── Événements ───────────────────────────────────────────────────────────
    bool  running   = true;
    float userScale = 1.f;
    bool  showStats = true;

    auto& ev = NkEvents();
    ev.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running = false; });
    ev.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        W = static_cast<uint32>(e->GetWidth());
        H = static_cast<uint32>(e->GetHeight());
    });
    ev.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        switch (e->GetKey()) {
        case NkKey::NK_ESCAPE:      running   = false; break;
        case NkKey::NK_NUMPAD_ADD:  userScale = userScale + 0.1f > 4.f    ? 4.f    : userScale + 0.1f; break;
        case NkKey::NK_NUMPAD_SUB:  userScale = userScale - 0.1f < 0.25f  ? 0.25f  : userScale - 0.1f; break;
        case NkKey::NK_R:           userScale = 1.f; break;
        case NkKey::NK_S:           showStats = !showStats; break;
        default: break;
        }
    });

    // ── Boucle principale ─────────────────────────────────────────────────────
    NkChrono  timer;
    double    tPrev   = timer.Elapsed().seconds;
    double    fpsAcc  = 0.0;
    int       fpsCnt  = 0;
    float     fps     = 0.f;

    while (running) {
        double now = timer.Elapsed().seconds;
        float  dt  = static_cast<float>(now - tPrev); tPrev = now;
        fpsAcc += dt; ++fpsCnt;
        if (fpsAcc >= 0.5) {
            fps = static_cast<float>(fpsCnt) / static_cast<float>(fpsAcc);
            fpsAcc = 0.0; fpsCnt = 0;
        }

        ev.PollEvents();
        if (!running) break;

        if (W != dev->GetSwapchainWidth() || H != dev->GetSwapchainHeight())
            dev->OnResize(W, H);

        NkFrameContext frame;
        if (!dev->BeginFrame(frame)) continue;
        W = dev->GetSwapchainWidth();
        H = dev->GetSwapchainHeight();
        if (W == 0 || H == 0) { dev->EndFrame(frame); continue; }

        NkFramebufferHandle fbo = dev->GetSwapchainFramebuffer();
        NkRenderPassHandle  rp  = dev->GetSwapchainRenderPass();

        cmd->Reset();
        if (!cmd->Begin()) { dev->EndFrame(frame); continue; }
        if (!cmd->BeginRenderPass(rp, fbo, NkRect2D{0,0,(int)W,(int)H})) {
            cmd->End(); dev->EndFrame(frame); continue;
        }
        cmd->SetViewport(NkViewport(0.f, 0.f, static_cast<float>(W), static_cast<float>(H)));
        cmd->SetScissor(NkRect2D(0u, 0u, W, H));

        fr->beginFrame(W, H);

        TextPrinter tp;
        tp.fr = fr;

        // FPS
        {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "FPS: %.0f  |  NkUI + NKFont  |  Scale: %.2fx (+/-/R)  |  S=stats",
                     fps, userScale);
            tp.reset(8.f);
            tp.line(fUI, buf, 10.f,  1.f, 1.f, 0.3f, 1.f);
        }

        tp.nl(fUI, 0.3f);

        // Validation glyphes
        tp.line(fUI,    "ABCDEFGHIJKLMNOPQRSTUVWXYZ (16px)",             10.f, 1.f, 1.f, 1.f, 1.f, userScale);
        tp.line(fUI,    "abcdefghijklmnopqrstuvwxyz 0123456789",         10.f, 0.9f,0.9f,0.9f,1.f, userScale);
        tp.line(fUI,    "!@#$%^&*()_+-=[]{}|;:',.<>?/~`",               10.f, 0.7f,0.85f,1.f,1.f, userScale);

        tp.nl(fUI, 0.5f);

        tp.line(fSmall, "Ceci est du texte en 12px — NkUI + NKFont",    10.f, 0.75f,0.75f,0.75f,1.f, userScale);
        tp.line(fUI,    "Bonjour le monde! (16px — rouge)",              10.f, 1.f, 0.4f, 0.4f, 1.f, userScale);
        tp.line(fUI,    "Hello World! (16px — vert)",                    10.f, 0.4f, 1.f, 0.4f, 1.f, userScale);
        tp.line(fMedium,"NkUI — Interface utilisateur (20px)",           10.f, 0.4f, 0.8f, 1.f, 1.f, userScale);
        tp.line(fBig,   "NKFont (32px)",                                 10.f, 1.f, 0.65f,0.2f, 1.f, userScale);

        tp.nl(fUI, 0.5f);

        tp.line(fUI, "Pipeline: TTF/OTF parser → Rasterizer → Atlas R8 → Quad render",
                10.f, 0.6f, 0.9f, 1.f, 1.f, userScale);
        tp.line(fUI, "NkUIFontBridge → NkFontParser + NkFontRasterizer → NkUIFontAtlas → NkUIFont",
                10.f, 0.6f, 0.9f, 1.f, 1.f, userScale);

        if (showStats) {
            tp.nl(fSmall, 0.5f);
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "Atlas: %dx%d px | Glyphes: %d | texId: %u",
                     NkUIFontAtlas::ATLAS_W, NkUIFontAtlas::ATLAS_H,
                     fr->atlas.numGlyphs, fr->atlas.texId);
            tp.line(fSmall, buf, 10.f, 0.6f, 0.6f, 0.6f, 1.f);

            if (fUI) {
                snprintf(buf, sizeof(buf),
                         "FontUI: asc=%.1f desc=%.1f lineH=%.1f spaceW=%.1f | builtin=%s",
                         fUI->metrics.ascender, fUI->metrics.descender,
                         fUI->metrics.lineHeight, fUI->metrics.spaceWidth,
                         fUI->isBuiltin ? "OUI" : "NON");
                tp.line(fSmall, buf, 10.f, 0.6f, 0.6f, 0.6f, 1.f);
            }

            snprintf(buf, sizeof(buf),
                     "Resolution: %ux%u | UserScale: %.2fx",
                     W, H, userScale);
            tp.line(fSmall, buf, 10.f, 0.6f, 0.6f, 0.6f, 1.f);
        }

        // Instructions bas d'écran
        {
            NkUIFont* fBot = fSmall ? fSmall : fUI;
            float lineH = fBot ? (fBot->metrics.lineHeight > 0.f ? fBot->metrics.lineHeight : fBot->size) : 20.f;
            tp.reset(static_cast<float>(H) - lineH - 6.f);
            tp.line(fBot, "+/- : zoom  |  R : reset  |  S : stats  |  ESC : quitter",
                    10.f, 0.5f, 0.5f, 0.5f, 0.8f);
        }

        fr->flush(cmd);

        cmd->EndRenderPass();
        cmd->End();
        dev->SubmitAndPresent(cmd);
        dev->EndFrame(frame);
    }

    dev->WaitIdle();
    fr->shutdown();
    delete fr;
    dev->DestroyCommandBuffer(cmd);
    NkDeviceFactory::Destroy(dev);
    window.Close();
    logger.Info("[NkUIDemo] Terminé.\n");
    return 0;
}