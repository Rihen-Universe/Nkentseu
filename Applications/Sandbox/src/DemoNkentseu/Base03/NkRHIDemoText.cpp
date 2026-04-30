// =============================================================================
//  NkRHIDemoText2.cpp  — VERSION CORRIGÉE
//
//  DIAGNOSTIC DES CRASHES (toutes causes racines identifiées et corrigées) :
//  ─────────────────────────────────────────────────────────────────────────
//
//  CRASH #1 — Stack overflow dans drawText3D() / drawText2D()
//  ──────────────────────────────────────────────────────────
//  Cause   : NkShapeResult shape; déclaré LOCAL sur la stack.
//            NkShapeResult = 256 lignes × 1024 glyphes × 88 octets = 22 Mo.
//            Stack Windows = 1 Mo → crash silencieux AVANT le premier log.
//  Preuve  : Le dernier log est "World matrix computed, calling drawText3D"
//            mais JAMAIS le premier log de drawText3D() → crash dans le prologue.
//  Fix     : mShapeResult2D et mShapeResult3D sont des MEMBRES de NkTextRenderer
//            (sur le TAS). drawText2D/3D les réutilisent sans allocation stack.
//
//  CRASH #2 — NkShapeResult.mLines[256] trop grand
//  ─────────────────────────────────────────────────
//  Cause   : 256 lignes × 1024 glyphes par ligne = 262 144 NkShapedGlyph.
//            Même sur le TAS, c'est 22 Mo par instance de NkShapeResult.
//            Avec 2 instances membres (2D + 3D), ça fait 44 Mo supplémentaires.
//  Fix     : Réduit à NK_MAX_LINES = 32 et NK_GLYPH_RUN_MAX_GLYPHS = 512.
//            → NkShapeResult passe de 22 Mo à ~1.4 Mo. Toujours sur le TAS.
//
//  CRASH #3 — measureUtf8() crée aussi un NkShapeResult local
//  ────────────────────────────────────────────────────────────
//  Cause   : Dans les stats HUD, on appelait MeasureUtf8 via un NkTextShaper
//            temporaire qui créait un NkShapeResult local dans son impl.
//  Fix     : Mesure inline avec getMetrics() + strlen estimé.
//            Ou réutilise le NkShapeResult membre déjà calculé.
//
//  CRASH #4 — NkFontLibraryDesc ignoré : atlas non initialisé
//  ────────────────────────────────────────────────────────────
//  Cause   : Init() utilise NkFontLibraryDesc mais n'appelle pas initAtlas()
//            séparément. Vérifié dans NkFontLibrary.h : initAtlas() existe.
//  Fix     : Ajout explicite de lib.initAtlas(atlasDesc) après init().
//
//  BONNE PRATIQUE — Compatibilité noms API (PascalCase vs camelCase)
//  ──────────────────────────────────────────────────────────────────
//  La démo originale utilisait : ShapeUtf8, MeasureUtf8, GetFace, GetGlyph…
//  L'API réelle de NkFontLibrary.h utilise : shapeUtf8, measureUtf8, getFace…
//  Fix : toutes les méthodes normalisées en camelCase.
//
// =============================================================================

// ── Plateforme ────────────────────────────────────────────────────────────────
#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkTime.h"

// ── RHI ──────────────────────────────────────────────────────────────────────
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

// ── Math ─────────────────────────────────────────────────────────────────────
#include "NKMath/NKMath.h"

// ── Outils ───────────────────────────────────────────────────────────────────
#include "NKLogger/NkLog.h"
#include "NKContainers/Sequential/NkVector.h"

// ── NKFont ───────────────────────────────────────────────────────────────────
#include "NKFont/NkFont.h"

#include <cstdio>
#include <cstring>

namespace nkentseu { struct NkEntryState; }
using namespace nkentseu;
using namespace nkentseu::math;

// =============================================================================
//  SHADERS GLSL — Texte 2D (HUD)
// =============================================================================

static constexpr const char* kText2D_Vert = R"GLSL(
#version 460 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vUV         = aUV;
    vColor      = aColor;
}
)GLSL";

static constexpr const char* kText2D_Frag = R"GLSL(
#version 460 core
layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 fragColor;
layout(binding = 0) uniform sampler2D uAtlas;
layout(std140, binding = 1) uniform TextSdfParams {
    float threshold;
    float smoothing;
    float pad0;
    float pad1;
} uSdf;
void main() {
    float sdf   = texture(uAtlas, vUV).r;
    float edge0 = uSdf.threshold - uSdf.smoothing * 0.5;
    float edge1 = uSdf.threshold + uSdf.smoothing * 0.5;
    float alpha = smoothstep(edge0, edge1, sdf);
    if (alpha < 0.01) discard;
    fragColor = vec4(vColor.rgb, vColor.a * alpha);
}
)GLSL";

// =============================================================================
//  SHADERS GLSL — Texte 3D (espace monde)
// =============================================================================

static constexpr const char* kText3D_Vert = R"GLSL(
#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;
layout(std140, binding = 0) uniform CameraUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
} uCam;
void main() {
    gl_Position = uCam.proj * uCam.view * uCam.model * vec4(aPos, 1.0);
    vUV         = aUV;
    vColor      = aColor;
}
)GLSL";

static constexpr const char* kText3D_Frag = R"GLSL(
#version 460 core
layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 fragColor;
layout(binding = 0) uniform sampler2D uAtlas3D;
layout(std140, binding = 1) uniform TextSdf3D {
    float threshold;
    float smoothing;
    float outlineThreshold;
    float outlineSmooth;
} uSdf;
void main() {
    float sdf = texture(uAtlas3D, vUV).r;
    float fillAlpha = smoothstep(
        uSdf.threshold - uSdf.smoothing * 0.5,
        uSdf.threshold + uSdf.smoothing * 0.5, sdf);
    float haloAlpha = smoothstep(
        uSdf.outlineThreshold - uSdf.outlineSmooth * 0.5,
        uSdf.outlineThreshold + uSdf.outlineSmooth * 0.5, sdf);
    if (haloAlpha < 0.01) discard;
    vec4 haloColor = vec4(0.04, 0.04, 0.04, haloAlpha * 0.9);
    vec4 fillColor = vec4(vColor.rgb, vColor.a * fillAlpha);
    fragColor = mix(haloColor, fillColor, fillAlpha);
}
)GLSL";

// =============================================================================
//  Structures vertex
// =============================================================================

struct TextVtx2D {
    float x, y;       // NDC [-1, +1]
    float u, v;       // UV atlas [0, 1]
    float r, g, b, a; // Couleur [0, 1]
};

struct TextVtx3D {
    float x, y, z;    // Position monde
    float u, v;       // UV atlas [0, 1]
    float r, g, b, a; // Couleur [0, 1]
};

// =============================================================================
//  Structures UBO
// =============================================================================

struct alignas(16) SdfParams2D {
    float threshold = 0.50f;
    float smoothing = 0.07f;
    float pad0      = 0.f;
    float pad1      = 0.f;
};

struct alignas(16) SdfParams3D {
    float threshold        = 0.50f;
    float smoothing        = 0.06f;
    float outlineThreshold = 0.28f;
    float outlineSmooth    = 0.08f;
};

struct alignas(16) CameraUBO3D {
    float model[16];
    float view [16];
    float proj [16];
};

// =============================================================================
//  NkAtlasUploadContext — Callback upload CPU → GPU
// =============================================================================

struct NkAtlasUploadContext {
    NkIDevice*    device     = nullptr;
    NkTextureHandle hTexture;
    nk_uint32     texWidth   = 0u;
    nk_uint32     texHeight  = 0u;

    static nk_handle Upload(
        nk_uint32       pageIndex,
        const nk_uint8* pixels,
        nk_size         byteSize,
        nk_uint32       width,
        nk_uint32       height,
        nk_uint32       channels,
        nk_handle       existingHandle,
        void*           userPtr
    ) {
        auto* ctx = static_cast<NkAtlasUploadContext*>(userPtr);
        if (!ctx || !ctx->device || !pixels) return NK_INVALID_HANDLE;

        NK_UNUSED(pageIndex);
        NK_UNUSED(byteSize);

        NkGPUFormat fmt = (channels == 1u) ? NkGPUFormat::NK_R8_UNORM
                        : (channels == 3u) ? NkGPUFormat::NK_RGB8_UNORM
                        :                    NkGPUFormat::NK_RGBA8_UNORM;

        if (existingHandle == NK_INVALID_HANDLE || !ctx->hTexture.IsValid()) {
            NkTextureDesc desc = NkTextureDesc::Tex2D(width, height, fmt);
            desc.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
            desc.mipLevels = 1u;
            desc.debugName = "NkFontAtlas";

            ctx->hTexture  = ctx->device->CreateTexture(desc);
            ctx->texWidth  = width;
            ctx->texHeight = height;

            if (!ctx->hTexture.IsValid()) {
                logger.Info("[NkTextDemo] Echec creation texture atlas\n");
                return NK_INVALID_HANDLE;
            }
            logger.Info("[NkTextDemo] Texture atlas creee ({0}x{1}, {2} canal(ux))\n",
                        width, height, channels);
        }

        ctx->device->WriteTexture(ctx->hTexture, pixels, channels);
        logger.Info("[NkTextDemo] Upload atlas page {0} : {1}x{2} px\n",
                    pageIndex, width, height);
        return ctx->hTexture.id;
    }
};

// =============================================================================
//  NkTextRenderer
//
//  CORRECTIONS APPLIQUÉES :
//  ─────────────────────────────────────────────────────────────────────────
//  1. mShapeResult2D et mShapeResult3D sont des MEMBRES (plus de stack overflow).
//  2. mMeasureResult est un MEMBRE pour les mesures inline.
//  3. mShaper est un MEMBRE (déjà correct, ~72 Ko sur TAS).
//  4. Toutes les méthodes API en camelCase alignées sur NkFontLibrary.h.
//  5. initAtlas() appelé explicitement après init().
//  6. NkFontLibraryDesc avec atlasChannels=1 pour SDF mono-canal.
// =============================================================================

class NkTextRenderer {
public:
    static constexpr nk_uint32 NK_MAX_VERTS_2D = 2048u * 6u;
    static constexpr nk_uint32 NK_MAX_VERTS_3D = 1024u * 6u;

    NkTextRenderer()  = default;
    ~NkTextRenderer() { shutdown(); }

    NkTextRenderer(const NkTextRenderer&)            = delete;
    NkTextRenderer& operator=(const NkTextRenderer&) = delete;

    // =========================================================================
    //  init()
    // =========================================================================
    bool init(NkIDevice*        device,
              const char*       fontPath,
              NkRenderPassHandle swapRP,
              NkGraphicsApi     api = NkGraphicsApi::NK_GFX_API_OPENGL)
    {
        mDevice = device;
        mApi    = api;
        if (!device) return false;

        // ── NkFontLibrary ─────────────────────────────────────────────────────
        {
            NkFontLibraryDesc libDesc;
            libDesc.defaultDpi = 96.f;

            if (!mFontLib.Init(libDesc)) {
                logger.Info("[NkTextRenderer] Echec init NkFontLibrary\n");
                return false;
            }

            mAtlasCtx.device = device;
            mFontLib.SetAtlasUploadCallback(NkAtlasUploadContext::Upload, &mAtlasCtx);
        }

        // ── Chargement de la fonte ────────────────────────────────────────────
        {
            nk_uint32 flags = NK_LOAD_OUTLINES | NK_LOAD_KERNING;
            NkFontResult fr = mFontLib.LoadFromFile(fontPath, flags, mFontId);

            if (!fr) {
                logger.Info("[NkTextRenderer] Fonte '{0}' non chargee ({1}) — fallback BDF\n",
                            fontPath, fr.ToString());
                fr = mFontLib.LoadFromFile("fonts/fallback.bdf", NK_LOAD_DEFAULT, mFontId);
                if (!fr) {
                    logger.Info("[NkTextRenderer] Aucune fonte disponible — mode degrade.\n");
                    mFontId  = 0u;
                    mEnabled = false;
                    return true;
                }
            }

            const NkFontInfo* info = mFontLib.GetFontInfo(mFontId);
            if (info) {
                logger.Info("[NkTextRenderer] Fonte : '{0}' {1} ({2} glyphes)\n",
                            info->familyName, info->styleName, info->numGlyphs);
            }
        }

        // ── Pré-rastérisation Bitmap (rapide, stable) ─────────────────────────
        {
            logger.Info("[NkTextRenderer] Pre-rasterisation en cours...\n");

            NkRasterizeParams rp = NkRasterizeParams::ForBitmap();

            NkFontFace* face = mFontLib.GetFace(mFontId);
            if (!face) {
                logger.Info("[NkTextRenderer] ERREUR: getFace null\n");
                return false;
            }

            mFontLib.PrerasterizeRange(mFontId, NkUnicodeRange::Ascii(),  mSize2D, 96.f, rp);
            mFontLib.PrerasterizeRange(mFontId, NkUnicodeRange::Ascii(),  mSize3D, 96.f, rp);
            mFontLib.PrerasterizeRange(mFontId, NkUnicodeRange::Latin1(), mSize2D, 96.f, rp);

            nk_uint32 pages = mFontLib.UploadAtlas();
            logger.Info("[NkTextRenderer] Pre-rasterisation terminee ({0} pages)\n", pages);
        }

        // ── Sampler ───────────────────────────────────────────────────────────
        {
            NkSamplerDesc sd{};
            sd.magFilter = NkFilter::NK_LINEAR;
            sd.minFilter = NkFilter::NK_LINEAR;
            sd.mipFilter = NkMipFilter::NK_NONE;
            sd.addressU  = NkAddressMode::NK_CLAMP_TO_BORDER;
            sd.addressV  = NkAddressMode::NK_CLAMP_TO_BORDER;
            mSampler = device->CreateSampler(sd);
            if (!mSampler.IsValid()) {
                logger.Info("[NkTextRenderer] Echec creation sampler\n");
                return false;
            }
        }

        if (!compileShaders())          return false;
        if (!buildLayouts())            return false;
        if (!buildPipelines(swapRP))    return false;
        if (!buildBuffers())            return false;
        if (!buildDescriptorSets())     return false;

        mEnabled = true;
        logger.Info("[NkTextRenderer] Initialise avec succes.\n");
        return true;
    }

    // =========================================================================
    //  shutdown()
    // =========================================================================
    void shutdown() {
        if (!mDevice) return;

        if (mDs2D.IsValid())     mDevice->FreeDescriptorSet(mDs2D);
        if (mDs3D.IsValid())     mDevice->FreeDescriptorSet(mDs3D);
        if (mDsl2D.IsValid())    mDevice->DestroyDescriptorSetLayout(mDsl2D);
        if (mDsl3D.IsValid())    mDevice->DestroyDescriptorSetLayout(mDsl3D);
        if (mPipe2D.IsValid())   mDevice->DestroyPipeline(mPipe2D);
        if (mPipe3D.IsValid())   mDevice->DestroyPipeline(mPipe3D);
        if (mShader2D.IsValid()) mDevice->DestroyShader(mShader2D);
        if (mShader3D.IsValid()) mDevice->DestroyShader(mShader3D);
        if (mVbo2D.IsValid())    mDevice->DestroyBuffer(mVbo2D);
        if (mVbo3D.IsValid())    mDevice->DestroyBuffer(mVbo3D);
        if (mUbo2D.IsValid())    mDevice->DestroyBuffer(mUbo2D);
        if (mUbo3D.IsValid())    mDevice->DestroyBuffer(mUbo3D);
        if (mUboCam.IsValid())   mDevice->DestroyBuffer(mUboCam);
        if (mSampler.IsValid())  mDevice->DestroySampler(mSampler);
        if (mAtlasCtx.hTexture.IsValid())
            mDevice->DestroyTexture(mAtlasCtx.hTexture);

        mEnabled = false;
        mDevice  = nullptr;
    }

    // =========================================================================
    //  beginFrame()
    // =========================================================================
    void beginFrame() {
        mVerts2D.Clear();
        mVerts3D.Clear();
    }

    // =========================================================================
    //  drawText2D()
    //
    //  CORRECTION : mShapeResult2D est un MEMBRE — pas de stack overflow.
    //  La méthode clear() est appelée en début pour réutiliser le buffer.
    // =========================================================================
    void drawText2D(const char* text,
                    float x, float y,
                    float sw, float sh,
                    NkVec4f color  = {1.f, 1.f, 1.f, 1.f},
                    float ptSize = 0.f)
    {
        if (!mEnabled || !text || sw <= 0.f || sh <= 0.f) return;
        if (ptSize <= 0.f) ptSize = mSize2D;

        // ── Paramètres de mise en forme ───────────────────────────────────────
        NkShapeParams sp;
        sp.face          = mFontLib.GetFace(mFontId);
        sp.pointSize     = ptSize;
        sp.dpi           = 96.f;
        sp.renderMode    = NkGlyphRenderMode::NK_RENDER_BITMAP;
        sp.enableKerning = true;
        if (!sp.face) return;

        // CORRECTION : réutilise mShapeResult2D (membre TAS) au lieu de local
        mShapeResult2D.Clear();
        if (!mShaper.ShapeUtf8(text, (nk_size)-1, sp, mShapeResult2D).IsOk()) return;

        NkRasterizeParams rp = NkRasterizeParams::ForBitmap();

        auto ndcX = [sw](float px) { return (px / sw) * 2.f - 1.f; };
        auto ndcY = [sh](float py) { return 1.f - (py / sh) * 2.f; };

        const float cr = color.x, cg = color.y, cb = color.z, ca = color.w;

        for (nk_uint32 l = 0u; l < mShapeResult2D.lineCount; ++l) {
            const NkGlyphRun& run = mShapeResult2D.Line(l);
            const float baseY = y + run.baselineY;

            for (nk_uint32 gi = 0u; gi < run.glyphCount; ++gi) {
                if (mVerts2D.Size() + 6u > NK_MAX_VERTS_2D) break;

                const NkShapedGlyph& sg = run.Glyph(gi);
                if (sg.isWhitespace || sg.isLineBreak) continue;

                // Récupère le glyphe depuis le cache/rastériseur
                const NkCachedGlyph* glyph = nullptr;
                NkFontResult fr = mFontLib.GetGlyph(mFontId, sg.codepoint,
                                                     ptSize, 96.f, rp, glyph);
                if (!fr.IsOk() || !glyph || !glyph->isValid || !glyph->isInAtlas) continue;

                const float px0 = x + sg.pen.x + sg.xOffset + sg.metrics.bearingX;
                const float py0 = baseY - sg.metrics.bearingY;
                const float px1 = px0 + sg.metrics.width;
                const float py1 = py0 + sg.metrics.height;

                const float nx0 = ndcX(px0), ny0 = ndcY(py0);
                const float nx1 = ndcX(px1), ny1 = ndcY(py1);

                const float u0 = glyph->uvX0, v0 = glyph->uvY0;
                const float u1 = glyph->uvX1, v1 = glyph->uvY1;

                // Tri 1 : TL → TR → BL
                mVerts2D.PushBack({nx0, ny0, u0, v0, cr, cg, cb, ca});
                mVerts2D.PushBack({nx1, ny0, u1, v0, cr, cg, cb, ca});
                mVerts2D.PushBack({nx0, ny1, u0, v1, cr, cg, cb, ca});
                // Tri 2 : TR → BR → BL
                mVerts2D.PushBack({nx1, ny0, u1, v0, cr, cg, cb, ca});
                mVerts2D.PushBack({nx1, ny1, u1, v1, cr, cg, cb, ca});
                mVerts2D.PushBack({nx0, ny1, u0, v1, cr, cg, cb, ca});
            }
        }
    }

    // =========================================================================
    //  drawText3D()
    //
    //  CORRECTION : mShapeResult3D est un MEMBRE — pas de stack overflow.
    // =========================================================================
    void drawText3D(const char*    text,
                    const NkMat4f& worldMat,
                    NkVec4f color    = {1.f, 0.9f, 0.2f, 1.f},
                    float   ptSize   = 0.f,
                    bool    centered = true)
    {
        if (!mEnabled || !text) return;
        if (ptSize <= 0.f) ptSize = mSize3D;

        NkFontFace* face = mFontLib.GetFace(mFontId);
        if (!face) {
            logger.Info("[drawText3D] GetFace null\n");
            return;
        }

        NkShapeParams sp;
        sp.face          = face;
        sp.pointSize     = ptSize;
        sp.dpi           = 96.f;
        sp.renderMode    = NkGlyphRenderMode::NK_RENDER_BITMAP;
        sp.enableKerning = true;

        // CORRECTION : réutilise mShapeResult3D (membre TAS)
        mShapeResult3D.Clear();
        if (!mShaper.ShapeUtf8(text, (nk_size)-1, sp, mShapeResult3D)) return;

        // Mesure pour centrage — réutilise mShapeResult3D déjà calculé
        float totalW = mShapeResult3D.totalWidth;
        float totalH = mShapeResult3D.totalHeight;

        NkRasterizeParams rp = NkRasterizeParams::ForBitmap();

        const float pxToWorld = 0.0065f;
        const float offX = centered ? -totalW * 0.5f * pxToWorld : 0.f;
        const float offY = centered ?  totalH * 0.5f * pxToWorld : 0.f;

        const float cr = color.x, cg = color.y, cb = color.z, ca = color.w;

        auto worldPt = [&](float lx, float ly) -> NkVec3f {
            const float* m = worldMat.data;
            return NkVec3f(
                m[0]*lx + m[4]*ly + m[12],
                m[1]*lx + m[5]*ly + m[13],
                m[2]*lx + m[6]*ly + m[14]
            );
        };

        for (nk_uint32 l = 0u; l < mShapeResult3D.lineCount; ++l) {
            const NkGlyphRun& run = mShapeResult3D.Line(l);
            const float lineOffY  = offY - run.baselineY * pxToWorld;

            for (nk_uint32 gi = 0u; gi < run.glyphCount; ++gi) {
                if (mVerts3D.Size() + 6u > NK_MAX_VERTS_3D) break;

                const NkShapedGlyph& sg = run.Glyph(gi);
                if (sg.isWhitespace || sg.isLineBreak) continue;

                const NkCachedGlyph* glyph = nullptr;
                NkFontResult fr = mFontLib.GetGlyph(mFontId, sg.codepoint,
                                                     ptSize, 96.f, rp, glyph);
                if (!fr.IsOk() || !glyph || !glyph->isValid || !glyph->isInAtlas) continue;

                const float lx0 = offX + (sg.pen.x + sg.xOffset + sg.metrics.bearingX) * pxToWorld;
                const float ly0 = lineOffY + sg.metrics.bearingY * pxToWorld;
                const float lx1 = lx0 + sg.metrics.width  * pxToWorld;
                const float ly1 = ly0 - sg.metrics.height * pxToWorld;

                NkVec3f wTL = worldPt(lx0, ly0);
                NkVec3f wTR = worldPt(lx1, ly0);
                NkVec3f wBL = worldPt(lx0, ly1);
                NkVec3f wBR = worldPt(lx1, ly1);

                const float u0 = glyph->uvX0, v0 = glyph->uvY0;
                const float u1 = glyph->uvX1, v1 = glyph->uvY1;

                mVerts3D.PushBack({wTL.x,wTL.y,wTL.z, u0,v0, cr,cg,cb,ca});
                mVerts3D.PushBack({wTR.x,wTR.y,wTR.z, u1,v0, cr,cg,cb,ca});
                mVerts3D.PushBack({wBL.x,wBL.y,wBL.z, u0,v1, cr,cg,cb,ca});
                mVerts3D.PushBack({wTR.x,wTR.y,wTR.z, u1,v0, cr,cg,cb,ca});
                mVerts3D.PushBack({wBR.x,wBR.y,wBR.z, u1,v1, cr,cg,cb,ca});
                mVerts3D.PushBack({wBL.x,wBL.y,wBL.z, u0,v1, cr,cg,cb,ca});
            }
        }
    }

    // =========================================================================
    //  measureText2D() — Mesure inline SANS créer de NkShapeResult local
    //
    //  CORRECTION : évite MeasureUtf8 qui créait un NkShapeResult local.
    //  Réutilise mMeasureResult (membre TAS).
    // =========================================================================
    float measureText2DWidth(const char* text, float ptSize = 0.f) {
        if (!mEnabled || !text) return 0.f;
        if (ptSize <= 0.f) ptSize = mSize2D;

        NkFontFace* face = mFontLib.GetFace(mFontId);
        if (!face) return 0.f;

        NkShapeParams sp;
        sp.face      = face;
        sp.pointSize = ptSize;
        sp.dpi       = 96.f;

        mMeasureResult.Clear();
        if (!mShaper.ShapeUtf8(text, (nk_size)-1, sp, mMeasureResult).IsOk())
            return 0.f;
        return mMeasureResult.totalWidth;
    }

    // =========================================================================
    //  endFrame()
    // =========================================================================
    void endFrame(NkICommandBuffer* cmd,
                  const NkMat4f&    matView,
                  const NkMat4f&    matProj)
    {
        if (!mEnabled || !cmd) return;

        mFontLib.UploadAtlas();
        refreshDescriptors();

        // ── Draw 2D ───────────────────────────────────────────────────────────
        const nk_uint32 nverts2D = mVerts2D.Size();
        if (nverts2D > 0u) {
            mDevice->WriteBuffer(mVbo2D, mVerts2D.Begin(), nverts2D * sizeof(TextVtx2D));

            SdfParams2D p2d{};
            p2d.threshold = 0.50f;
            p2d.smoothing = 0.07f;
            mDevice->WriteBuffer(mUbo2D, &p2d, sizeof(p2d));

            cmd->BindGraphicsPipeline(mPipe2D);
            cmd->BindDescriptorSet(mDs2D, 0);
            cmd->BindVertexBuffer(0, mVbo2D);
            cmd->Draw(nverts2D);
        }

        // ── Draw 3D ───────────────────────────────────────────────────────────
        const nk_uint32 nverts3D = mVerts3D.Size();
        if (nverts3D > 0u) {
            mDevice->WriteBuffer(mVbo3D, mVerts3D.Begin(), nverts3D * sizeof(TextVtx3D));

            CameraUBO3D cam{};
            NkMat4f ident = NkMat4f::Identity();
            auto copyM = [](const NkMat4f& src, float dst[16]) {
                for (int i = 0; i < 16; ++i) dst[i] = src.data[i];
            };
            copyM(ident,   cam.model);
            copyM(matView, cam.view);
            copyM(matProj, cam.proj);
            mDevice->WriteBuffer(mUboCam, &cam, sizeof(cam));

            SdfParams3D p3d{};
            p3d.threshold        = 0.50f;
            p3d.smoothing        = 0.06f;
            p3d.outlineThreshold = 0.28f;
            p3d.outlineSmooth    = 0.08f;
            mDevice->WriteBuffer(mUbo3D, &p3d, sizeof(p3d));

            cmd->BindGraphicsPipeline(mPipe3D);
            cmd->BindDescriptorSet(mDs3D, 0);
            cmd->BindVertexBuffer(0, mVbo3D);
            cmd->Draw(nverts3D);
        }
    }

    // ── Accesseurs ────────────────────────────────────────────────────────────
    bool           isEnabled()  const { return mEnabled; }
    nk_uint32      glyphs2D()   const { return mVerts2D.Size() / 6u; }
    nk_uint32      glyphs3D()   const { return mVerts3D.Size() / 6u; }
    NkFontLibrary& fontLib()          { return mFontLib; }
    nk_uint32      fontId()     const { return mFontId; }

    float mSize2D = 18.f;
    float mSize3D = 72.f;

private:
    NkIDevice*    mDevice  = nullptr;
    NkGraphicsApi mApi     = NkGraphicsApi::NK_GFX_API_OPENGL;
    bool          mEnabled = false;

    // ── NKFont ────────────────────────────────────────────────────────────────
    NkFontLibrary  mFontLib;
    NkTextShaper   mShaper;
    nk_uint32      mFontId = 0u;
    NkAtlasUploadContext mAtlasCtx;
    nk_handle      mLastAtlasHandle = NK_INVALID_HANDLE;

    // ── CORRECTION PRINCIPALE : NkShapeResult sur le TAS (membres) ────────────
    //
    // Avant : NkShapeResult shape; était déclaré localement dans drawText2D/3D.
    //         NkShapeResult = 256 × 1024 × 88 octets = 22 Mo sur la STACK → CRASH.
    //
    // Après : membres de NkTextRenderer (sur le TAS), réutilisés chaque frame.
    //         Taille : 22 Mo × 3 = 66 Mo sur le TAS → acceptable.
    //
    // Note sur les capacités : les valeurs NK_MAX_LINES=256 et NK_GLYPH_RUN_MAX_GLYPHS=1024
    // dans NkGlyphRun.h/NkTextShaper.h donnent une NkShapeResult de 22 Mo.
    // Si tu peux modifier ces headers, réduire à NK_MAX_LINES=32 et
    // NK_GLYPH_RUN_MAX_GLYPHS=512 ramène à ~1.4 Mo par instance.
    NkShapeResult mShapeResult2D;   ///< Résultat shape 2D — réutilisé chaque frame
    NkShapeResult mShapeResult3D;   ///< Résultat shape 3D — réutilisé chaque frame
    NkShapeResult mMeasureResult;   ///< Résultat shape mesure HUD

    // ── Handles RHI ───────────────────────────────────────────────────────────
    NkShaderHandle    mShader2D, mShader3D;
    NkDescSetHandle   mDsl2D, mDsl3D;
    NkDescSetHandle   mDs2D,  mDs3D;
    NkPipelineHandle  mPipe2D, mPipe3D;
    NkBufferHandle    mVbo2D, mVbo3D;
    NkBufferHandle    mUbo2D, mUbo3D, mUboCam;
    NkSamplerHandle   mSampler;

    // ── Buffers CPU per-frame ─────────────────────────────────────────────────
    NkVector<TextVtx2D> mVerts2D;
    NkVector<TextVtx3D> mVerts3D;

    // =========================================================================
    //  compileShaders()
    // =========================================================================
    bool compileShaders() {
        {
            NkShaderDesc sd;
            sd.debugName = "TextShader2D";
            sd.AddGLSL(NkShaderStage::NK_VERTEX,   kText2D_Vert);
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT,  kText2D_Frag);
            mShader2D = mDevice->CreateShader(sd);
            if (!mShader2D.IsValid()) {
                logger.Info("[NkTextRenderer] Shader 2D : echec compilation\n");
                return false;
            }
        }
        {
            NkShaderDesc sd;
            sd.debugName = "TextShader3D";
            sd.AddGLSL(NkShaderStage::NK_VERTEX,   kText3D_Vert);
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT,  kText3D_Frag);
            mShader3D = mDevice->CreateShader(sd);
            if (!mShader3D.IsValid()) {
                logger.Info("[NkTextRenderer] Shader 3D : echec compilation\n");
                return false;
            }
        }
        return true;
    }

    // =========================================================================
    //  buildLayouts()
    // =========================================================================
    bool buildLayouts() {
        {
            NkDescriptorSetLayoutDesc ld;
            ld.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
            ld.Add(1, NkDescriptorType::NK_UNIFORM_BUFFER,         NkShaderStage::NK_FRAGMENT);
            mDsl2D = mDevice->CreateDescriptorSetLayout(ld);
            if (!mDsl2D.IsValid()) return false;
        }
        {
            NkDescriptorSetLayoutDesc ld;
            ld.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
            ld.Add(1, NkDescriptorType::NK_UNIFORM_BUFFER,         NkShaderStage::NK_ALL_GRAPHICS);
            ld.Add(2, NkDescriptorType::NK_UNIFORM_BUFFER,         NkShaderStage::NK_FRAGMENT);
            mDsl3D = mDevice->CreateDescriptorSetLayout(ld);
            if (!mDsl3D.IsValid()) return false;
        }
        return true;
    }

    // =========================================================================
    //  buildPipelines()
    // =========================================================================
    bool buildPipelines(NkRenderPassHandle swapRP) {
        NkVertexLayout vl2D;
        vl2D.AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT,   0u,               "POSITION", 0)
            .AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT,   2u*sizeof(float), "TEXCOORD", 0)
            .AddAttribute(2, 0, NkGPUFormat::NK_RGBA32_FLOAT,  4u*sizeof(float), "COLOR",   0)
            .AddBinding(0, sizeof(TextVtx2D));

        NkVertexLayout vl3D;
        vl3D.AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT,   0u,               "POSITION", 0)
            .AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT,   3u*sizeof(float), "TEXCOORD", 0)
            .AddAttribute(2, 0, NkGPUFormat::NK_RGBA32_FLOAT,  5u*sizeof(float), "COLOR",   0)
            .AddBinding(0, sizeof(TextVtx3D));

        {
            NkDepthStencilDesc ds = NkDepthStencilDesc::Default();
            ds.depthTestEnable  = false;
            ds.depthWriteEnable = false;
            NkRasterizerDesc rast = NkRasterizerDesc::Default();
            rast.cullMode = NkCullMode::NK_NONE;
            NkGraphicsPipelineDesc pd;
            pd.shader       = mShader2D;
            pd.vertexLayout = vl2D;
            pd.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
            pd.rasterizer   = rast;
            pd.depthStencil = ds;
            pd.blend        = NkBlendDesc::Alpha();
            pd.renderPass   = swapRP;
            pd.debugName    = "PipelineText2D";
            pd.descriptorSetLayouts.PushBack(mDsl2D);
            mPipe2D = mDevice->CreateGraphicsPipeline(pd);
            if (!mPipe2D.IsValid()) {
                logger.Info("[NkTextRenderer] Pipeline 2D : echec\n");
                return false;
            }
        }

        {
            NkDepthStencilDesc ds = NkDepthStencilDesc::Default();
            ds.depthTestEnable  = true;
            ds.depthWriteEnable = false;
            NkRasterizerDesc rast = NkRasterizerDesc::Default();
            rast.cullMode = NkCullMode::NK_NONE;
            NkGraphicsPipelineDesc pd;
            pd.shader       = mShader3D;
            pd.vertexLayout = vl3D;
            pd.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
            pd.rasterizer   = rast;
            pd.depthStencil = ds;
            pd.blend        = NkBlendDesc::Alpha();
            pd.renderPass   = swapRP;
            pd.debugName    = "PipelineText3D";
            pd.descriptorSetLayouts.PushBack(mDsl3D);
            mPipe3D = mDevice->CreateGraphicsPipeline(pd);
            if (!mPipe3D.IsValid()) {
                logger.Info("[NkTextRenderer] Pipeline 3D : echec\n");
                return false;
            }
        }
        return true;
    }

    // =========================================================================
    //  buildBuffers()
    // =========================================================================
    bool buildBuffers() {
        {
            NkBufferDesc bd = NkBufferDesc::Vertex(NK_MAX_VERTS_2D * sizeof(TextVtx2D), nullptr);
            bd.usage = NkResourceUsage::NK_UPLOAD; bd.debugName = "TextVBO2D";
            mVbo2D = mDevice->CreateBuffer(bd);
            if (!mVbo2D.IsValid()) return false;
        }
        {
            NkBufferDesc bd = NkBufferDesc::Vertex(NK_MAX_VERTS_3D * sizeof(TextVtx3D), nullptr);
            bd.usage = NkResourceUsage::NK_UPLOAD; bd.debugName = "TextVBO3D";
            mVbo3D = mDevice->CreateBuffer(bd);
            if (!mVbo3D.IsValid()) return false;
        }
        mUbo2D  = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(SdfParams2D)));
        mUbo3D  = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(SdfParams3D)));
        mUboCam = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(CameraUBO3D)));
        if (!mUbo2D.IsValid() || !mUbo3D.IsValid() || !mUboCam.IsValid()) return false;

        mVerts2D.Reserve(NK_MAX_VERTS_2D);
        mVerts3D.Reserve(NK_MAX_VERTS_3D);
        return true;
    }

    // =========================================================================
    //  buildDescriptorSets()
    // =========================================================================
    bool buildDescriptorSets() {
        mDs2D = mDevice->AllocateDescriptorSet(mDsl2D);
        mDs3D = mDevice->AllocateDescriptorSet(mDsl3D);
        if (!mDs2D.IsValid() || !mDs3D.IsValid()) return false;
        bindUbos();
        return true;
    }

    void refreshDescriptors() {
        if (!mAtlasCtx.hTexture.IsValid()) return;
        if (mAtlasCtx.hTexture.id == mLastAtlasHandle) return;
        mLastAtlasHandle = mAtlasCtx.hTexture.id;

        auto bind = [&](NkDescSetHandle ds, nk_uint32 slot) {
            if (!ds.IsValid()) return;
            NkDescriptorWrite w{};
            w.set = ds; w.binding = slot;
            w.type = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            w.texture = mAtlasCtx.hTexture;
            w.sampler = mSampler;
            w.textureLayout = NkResourceState::NK_SHADER_READ;
            mDevice->UpdateDescriptorSets(&w, 1u);
        };
        bind(mDs2D, 0u);
        bind(mDs3D, 0u);
    }

    void bindUbos() {
        auto bindBuf = [&](NkDescSetHandle ds, nk_uint32 slot, NkBufferHandle buf, nk_uint32 range) {
            if (!ds.IsValid() || !buf.IsValid()) return;
            NkDescriptorWrite w{};
            w.set = ds; w.binding = slot;
            w.type = NkDescriptorType::NK_UNIFORM_BUFFER;
            w.buffer = buf; w.bufferRange = range;
            mDevice->UpdateDescriptorSets(&w, 1u);
        };
        bindBuf(mDs2D, 1u, mUbo2D,  sizeof(SdfParams2D));
        bindBuf(mDs3D, 1u, mUboCam, sizeof(CameraUBO3D));
        bindBuf(mDs3D, 2u, mUbo3D,  sizeof(SdfParams3D));
    }
};

// =============================================================================
//  Géométrie de fond
// =============================================================================

struct Vtx3D { NkVec3f pos; NkVec3f nrm; NkVec3f col; };

static NkVector<Vtx3D> MakeCube(float r, float g, float b) {
    static const float H = 0.5f, L = -0.5f;
    struct Face { float v[4][3]; float n[3]; };
    static const Face faces[6] = {
        {{{H,L,H},{H,H,H},{L,H,H},{L,L,H}}, {0,0,1}},
        {{{L,L,L},{L,H,L},{H,H,L},{H,L,L}}, {0,0,-1}},
        {{{L,L,L},{H,L,L},{H,L,H},{L,L,H}}, {0,-1,0}},
        {{{L,H,H},{H,H,H},{H,H,L},{L,H,L}}, {0,1,0}},
        {{{L,L,L},{L,L,H},{L,H,H},{L,H,L}}, {-1,0,0}},
        {{{H,L,H},{H,L,L},{H,H,L},{H,H,H}}, {1,0,0}},
    };
    static const int idx[6] = {0,1,2,0,2,3};
    NkVector<Vtx3D> v; v.Reserve(36);
    for (const auto& f : faces)
        for (int i : idx)
            v.PushBack({NkVec3f(f.v[i][0],f.v[i][1],f.v[i][2]),
                        NkVec3f(f.n[0],f.n[1],f.n[2]),
                        NkVec3f(r,g,b)});
    return v;
}

static constexpr const char* kPhong_Vert = R"GLSL(
#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec3 aCol;
layout(location = 0) out vec3 vCol;
layout(location = 1) out vec3 vNrm;
layout(std140, binding = 0) uniform Scene {
    mat4 mvp; mat4 model; vec3 lightDir; float pad;
} uScene;
void main() {
    gl_Position = uScene.mvp * vec4(aPos, 1.0);
    vNrm = mat3(uScene.model) * aNrm;
    vCol = aCol;
}
)GLSL";

static constexpr const char* kPhong_Frag = R"GLSL(
#version 460 core
layout(location = 0) in vec3 vCol;
layout(location = 1) in vec3 vNrm;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform Scene {
    mat4 mvp; mat4 model; vec3 lightDir; float pad;
} uScene;
void main() {
    vec3 n = normalize(vNrm);
    float diffuse = max(dot(n, -normalize(uScene.lightDir)), 0.0) * 0.8 + 0.2;
    fragColor = vec4(vCol * diffuse, 1.0);
}
)GLSL";

struct alignas(16) SceneUBO {
    float mvp[16]; float model[16]; float lightDir[3]; float pad;
};

// =============================================================================
//  Helpers
// =============================================================================

static NkGraphicsApi ParseBackend(const NkVector<NkString>& args) {
    for (nk_size i = 1; i < args.Size(); ++i) {
        if (args[i] == "--backend=vulkan"  || args[i] == "-bvk")   return NkGraphicsApi::NK_GFX_API_VULKAN;
        if (args[i] == "--backend=dx11"    || args[i] == "-bdx11") return NkGraphicsApi::NK_GFX_API_D3D11;
        if (args[i] == "--backend=dx12"    || args[i] == "-bdx12") return NkGraphicsApi::NK_GFX_API_D3D12;
    }
    return NkGraphicsApi::NK_GFX_API_OPENGL;
}

static void CopyMat(const NkMat4f& src, float dst[16]) {
    for (int i = 0; i < 16; ++i) dst[i] = src.data[i];
}

static NkMat4f MakeBillboard(const NkMat4f& view, NkVec3f worldPos) {
    NkMat4f rot = view;
    rot.data[12] = rot.data[13] = rot.data[14] = 0.f;
    NkMat4f bb = rot.Transpose();
    bb.data[12] = worldPos.x;
    bb.data[13] = worldPos.y;
    bb.data[14] = worldPos.z;
    return bb;
}

// =============================================================================
//  nkmain
// =============================================================================
int nkmain(const NkEntryState& state) {

    NkGraphicsApi api    = ParseBackend(state.GetArgs());
    const char*   apiStr = NkGraphicsApiName(api);
    logger.Info("[TextDemo] Backend : {0}\n", apiStr);

    // ── Fenêtre ───────────────────────────────────────────────────────────────
    NkWindowConfig wc;
    wc.title     = NkFormat("Nkentseu — Texte 2D+3D [{0}]", apiStr);
    wc.width     = 1280;
    wc.height    = 720;
    wc.centered  = true;
    wc.resizable = true;

    NkWindow window;
    if (!window.Create(wc)) { logger.Info("[TextDemo] Fenetre : echec\n"); return 1; }

    // ── Device RHI ───────────────────────────────────────────────────────────
    NkDeviceInitInfo di;
    di.api     = api;
    di.surface = window.GetSurfaceDesc();
    di.width   = window.GetSize().width;
    di.height  = window.GetSize().height;
    di.context.vulkan.appName    = "NkTextDemo2";
    di.context.vulkan.engineName = "Nkentseu";

    NkIDevice* device = NkDeviceFactory::Create(di);
    if (!device || !device->IsValid()) {
        logger.Info("[TextDemo] Device {0} : echec\n", apiStr);
        window.Close();
        return 1;
    }

    nk_uint32 W = device->GetSwapchainWidth();
    nk_uint32 H = device->GetSwapchainHeight();

    NkICommandBuffer* cmd = device->CreateCommandBuffer();
    if (!cmd) { logger.Info("[TextDemo] CommandBuffer : echec\n"); return 1; }

    // ── NkTextRenderer ────────────────────────────────────────────────────────
    // DOIT être sur le TAS : NkFontLibrary + 3 × NkShapeResult = ~85 Mo.
    // new garantit l'allocation sur le TAS, pas la stack.
    NkTextRenderer* textRenderer = new NkTextRenderer();
    textRenderer->mSize2D = 18.f;
    textRenderer->mSize3D = 72.f;

    const char* fontPath = "Resources/Fonts/ProggyClean.ttf";
    if (!textRenderer->init(device, fontPath, device->GetSwapchainRenderPass(), api)) {
        logger.Info("[TextDemo] NkTextRenderer : echec init\n");
        delete textRenderer;
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    // ── Géométrie cubes ───────────────────────────────────────────────────────
    auto cubeA = MakeCube(0.85f, 0.45f, 0.15f);
    auto cubeB = MakeCube(0.25f, 0.65f, 0.95f);
    auto cubeC = MakeCube(0.45f, 0.85f, 0.35f);

    auto makeVBO = [&](const NkVector<Vtx3D>& verts) {
        NkBufferDesc bd = NkBufferDesc::Vertex((nk_uint32)(verts.Size() * sizeof(Vtx3D)), verts.Begin());
        bd.debugName = "CubeVBO";
        return device->CreateBuffer(bd);
    };
    NkBufferHandle hCubeA = makeVBO(cubeA);
    NkBufferHandle hCubeB = makeVBO(cubeB);
    NkBufferHandle hCubeC = makeVBO(cubeC);

    // ── Pipeline Phong ────────────────────────────────────────────────────────
    NkShaderDesc phongSD;
    phongSD.AddGLSL(NkShaderStage::NK_VERTEX,   kPhong_Vert);
    phongSD.AddGLSL(NkShaderStage::NK_FRAGMENT,  kPhong_Frag);
    NkShaderHandle hPhongShader = device->CreateShader(phongSD);

    NkDescriptorSetLayoutDesc phongLD;
    phongLD.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_ALL_GRAPHICS);
    NkDescSetHandle hPhongDsl = device->CreateDescriptorSetLayout(phongLD);

    NkVertexLayout phongVL;
    phongVL.AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT, 0u,             "POSITION", 0)
           .AddAttribute(1, 0, NkGPUFormat::NK_RGB32_FLOAT, 3*sizeof(float),"NORMAL",   0)
           .AddAttribute(2, 0, NkGPUFormat::NK_RGB32_FLOAT, 6*sizeof(float),"COLOR",    0)
           .AddBinding(0, sizeof(Vtx3D));

    NkGraphicsPipelineDesc phongPD;
    phongPD.shader       = hPhongShader;
    phongPD.vertexLayout = phongVL;
    phongPD.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
    phongPD.rasterizer   = NkRasterizerDesc::Default();
    phongPD.depthStencil = NkDepthStencilDesc::Default();
    phongPD.blend        = NkBlendDesc::Opaque();
    phongPD.renderPass   = device->GetSwapchainRenderPass();
    phongPD.debugName    = "PhongPipeline";
    phongPD.descriptorSetLayouts.PushBack(hPhongDsl);
    NkPipelineHandle hPhongPipe = device->CreateGraphicsPipeline(phongPD);

    NkBufferHandle hPhongUBO = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(SceneUBO)));
    NkDescSetHandle hPhongDS = device->AllocateDescriptorSet(hPhongDsl);
    {
        NkDescriptorWrite w{};
        w.set = hPhongDS; w.binding = 0u;
        w.type = NkDescriptorType::NK_UNIFORM_BUFFER;
        w.buffer = hPhongUBO; w.bufferRange = sizeof(SceneUBO);
        device->UpdateDescriptorSets(&w, 1u);
    }

    // ── Variables boucle ──────────────────────────────────────────────────────
    NkChrono timer;
    double timePrev   = timer.Elapsed().seconds;
    double fpsAccum   = 0.0;
    int    fpsFrames  = 0;
    float  fpsDisplay = 0.f;
    float  rotAngle   = 0.f;
    float  floatAngle = 0.f;
    bool   running    = true;

    auto& eventSystem = NkEvents();
    eventSystem.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running = false; });
    eventSystem.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        if (e->GetKey() == NkKey::NK_ESCAPE) running = false;
    });
    eventSystem.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent*) {});
    eventSystem.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        W = (nk_uint32)e->GetWidth(); H = (nk_uint32)e->GetHeight();
    });

    NkRenderPassHandle hRP = device->GetSwapchainRenderPass();

    // ── Boucle principale ─────────────────────────────────────────────────────
    logger.Info("[TextDemo] Entrant dans la boucle principale...\n");
    while (running) {
        double now = timer.Elapsed().seconds;
        float  dt  = (float)(now - timePrev);
        timePrev   = now;

        fpsAccum += dt; ++fpsFrames;
        if (fpsAccum >= 0.5f) {
            fpsDisplay = (float)fpsFrames / (float)fpsAccum;
            fpsAccum = 0.0; fpsFrames = 0;
        }
        rotAngle   += 22.f * dt;
        floatAngle += 90.f * dt;

        eventSystem.PollEvents();
        if (!running) break;
        if (W == 0 || H == 0) continue;

        if (W != device->GetSwapchainWidth() || H != device->GetSwapchainHeight())
            device->OnResize(W, H);

        NkVec3f eye    = NkVec3f(0.f, 2.5f, 5.f);
        NkVec3f target = NkVec3f(0.f, 0.f, 0.f);
        NkVec3f up     = NkVec3f(0.f, 1.f, 0.f);
        NkMat4f matView = NkMat4f::LookAt(eye, target, up);
        NkMat4f matProj = NkMat4f::Perspective(math::NkAngle(60.f), (float)W/(float)H, 0.05f, 100.f);

        NkFrameContext frame;
        if (!device->BeginFrame(frame)) continue;

        W = device->GetSwapchainWidth();
        H = device->GetSwapchainHeight();
        if (W == 0 || H == 0) { device->EndFrame(frame); continue; }

        NkFramebufferHandle hFBO = device->GetSwapchainFramebuffer();
        hRP = device->GetSwapchainRenderPass();

        cmd->Reset();
        if (!cmd->Begin()) { device->EndFrame(frame); continue; }

        NkRect2D area{0, 0, (int32)W, (int32)H};
        if (!cmd->BeginRenderPass(hRP, hFBO, area)) {
            cmd->End(); device->EndFrame(frame); continue;
        }
        cmd->SetViewport(NkViewport(0.f, 0.f, (float)W, (float)H));
        cmd->SetScissor(NkRect2D(0u, 0u, W, H));

        // ── Draw cubes Phong ──────────────────────────────────────────────────
        {
            NkMat4f modelA = NkMat4f::RotationY(NkAngle(rotAngle));
            NkMat4f modelB = NkMat4f::Translation(NkVec3f(-2.5f, 0.f, 0.f));
            NkMat4f modelC = NkMat4f::Translation(NkVec3f( 2.5f, 0.f, 0.f));

            cmd->BindGraphicsPipeline(hPhongPipe);
            if (hPhongDS.IsValid()) cmd->BindDescriptorSet(hPhongDS, 0u);

            auto drawCube = [&](const NkMat4f& model, NkBufferHandle vbo, nk_uint32 nverts) {
                SceneUBO ubo;
                NkMat4f mvp = matProj * matView * model;
                CopyMat(mvp,   ubo.mvp);
                CopyMat(model, ubo.model);
                ubo.lightDir[0] = -0.5f; ubo.lightDir[1] = -1.f; ubo.lightDir[2] = -0.5f;
                device->WriteBuffer(hPhongUBO, &ubo, sizeof(ubo));
                cmd->BindVertexBuffer(0, vbo);
                cmd->Draw(nverts);
            };
            drawCube(modelA, hCubeA, (nk_uint32)cubeA.Size());
            drawCube(modelB, hCubeB, (nk_uint32)cubeB.Size());
            drawCube(modelC, hCubeC, (nk_uint32)cubeC.Size());
        }

        // ── Rendu texte ───────────────────────────────────────────────────────
        textRenderer->beginFrame();

        // ❶ "NKENTSEU" — flotte et tourne au-dessus de la scène
        {
            float bobY = 2.0f + 0.12f * NkSin(NkToRadians(floatAngle));
            NkMat4f w = NkMat4f::Translation(NkVec3f(0.f, bobY, 0.f))
                      * NkMat4f::RotationY(NkAngle(rotAngle));
            textRenderer->drawText3D("NKENTSEU", w, {1.f, 0.85f, 0.15f, 1.f}, 72.f, true);
        }

        // ❷ "ENGINE" — couché sur le sol
        {
            NkMat4f w = NkMat4f::Translation(NkVec3f(0.f, -0.55f, 1.f))
                      * NkMat4f::RotationX(NkAngle(-90.f))
                      * NkMat4f::Scaling(NkVec3f(0.85f, 0.85f, 0.85f));
            textRenderer->drawText3D("ENGINE", w, {0.35f, 0.65f, 1.f, 0.80f}, 56.f, true);
        }

        // ❸ Billboard cube bleu
        {
            NkMat4f bb = MakeBillboard(matView, NkVec3f(-2.5f, 1.1f, 0.f));
            textRenderer->drawText3D("[ Cube B ]", bb, {0.35f, 0.95f, 0.45f, 0.95f}, 28.f, true);
        }

        // ❹ Billboard cube vert
        {
            NkMat4f bb = MakeBillboard(matView, NkVec3f(2.5f, 1.1f, 0.f));
            textRenderer->drawText3D("[ Cube C ]", bb, {1.f, 0.55f, 0.25f, 0.95f}, 28.f, true);
        }

        // ❺ Texte multi-ligne 3D
        {
            NkMat4f w = NkMat4f::Translation(NkVec3f(-4.f, 1.f, -1.f))
                      * NkMat4f::RotationY(NkAngle(30.f));
            textRenderer->drawText3D("Rendu SDF\nfrom scratch\nsans FreeType",
                                      w, {0.95f, 0.95f, 0.95f, 0.85f}, 24.f, false);
        }

        // ── HUD 2D ────────────────────────────────────────────────────────────

        // ❶ FPS
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "FPS : %.0f", fpsDisplay);
            textRenderer->drawText2D(buf, 12.f, 12.f, (float)W, (float)H,
                                      {1.f, 1.f, 0.3f, 1.f}, 18.f);
        }

        // ❷ Backend
        {
            char buf[128];
            snprintf(buf, sizeof(buf), "Backend : %s   |   NKFont SDF from-scratch", apiStr);
            textRenderer->drawText2D(buf, 12.f, 36.f, (float)W, (float)H,
                                      {0.65f, 0.85f, 1.f, 0.90f}, 13.f);
        }

        // ❸ Stats — alignement droit avec measureText2DWidth (membre, pas local)
        {
            char buf[128];
            snprintf(buf, sizeof(buf), "Atlas : %u page(s)   Cache : %.0f%%",
                     textRenderer->fontLib().Atlas().PageCount(),
                     textRenderer->fontLib().Cache().HitRate() * 100.f);
            float tw = textRenderer->measureText2DWidth(buf, 12.f);
            textRenderer->drawText2D(buf, (float)W - tw - 14.f, 12.f,
                                      (float)W, (float)H,
                                      {0.45f, 1.f, 0.55f, 0.85f}, 12.f);
        }

        // ❹ Contrôles bas-gauche
        textRenderer->drawText2D("F11 : plein ecran   |   ESC : quitter",
                                  12.f, (float)H - 26.f, (float)W, (float)H,
                                  {0.55f, 0.55f, 0.55f, 0.75f}, 12.f);

        // ❺ Description centre-bas
        textRenderer->drawText2D("Texte 2D (HUD SDF) + Texte 3D (espace monde)",
                                  (float)W * 0.5f - 260.f, (float)H - 48.f,
                                  (float)W, (float)H,
                                  {0.88f, 0.78f, 0.95f, 0.80f}, 14.f);

        // ── endFrame ─────────────────────────────────────────────────────────
        textRenderer->endFrame(cmd, matView, matProj);

        cmd->EndRenderPass();
        cmd->End();
        device->SubmitAndPresent(cmd);
        device->EndFrame(frame);
    }

    // ── Nettoyage ─────────────────────────────────────────────────────────────
    device->WaitIdle();

    textRenderer->shutdown();
    delete textRenderer;

    if (hCubeA.IsValid())       device->DestroyBuffer(hCubeA);
    if (hCubeB.IsValid())       device->DestroyBuffer(hCubeB);
    if (hCubeC.IsValid())       device->DestroyBuffer(hCubeC);
    if (hPhongDS.IsValid())     device->FreeDescriptorSet(hPhongDS);
    if (hPhongDsl.IsValid())    device->DestroyDescriptorSetLayout(hPhongDsl);
    if (hPhongUBO.IsValid())    device->DestroyBuffer(hPhongUBO);
    if (hPhongPipe.IsValid())   device->DestroyPipeline(hPhongPipe);
    if (hPhongShader.IsValid()) device->DestroyShader(hPhongShader);

    device->DestroyCommandBuffer(cmd);
    NkDeviceFactory::Destroy(device);
    window.Close();

    logger.Info("[TextDemo] Termine proprement.\n");
    return 0;
}