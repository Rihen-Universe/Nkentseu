/**
 * @file    NkFontGLTest.cpp
 * @brief   Test NKFont SANS NKRHI — OpenGL 3.3 direct via GLAD.
 *
 * OBJECTIF : Valider que NKFont produit des pixels corrects
 *            indépendamment de tout problème dans ton RHI.
 *
 * ── Ce que ce test vérifie ──────────────────────────────────────────────────
 *   ① NkFontLibrary::loadFromFile()      → fonte chargée en mémoire
 *   ② NkFontLibrary::prerasterizeRange() → glyphes rastérisés en bitmap
 *   ③ Le callback upload → texture GL créée avec les bons pixels
 *   ④ NkTextShaper::shapeUtf8()         → UV valides dans l'atlas
 *   ⑤ glDrawArrays()                    → texte visible à l'écran
 *   ⑥ debugDumpAtlasPage0() → atlas_debug.pgm pour inspection offline
 *
 * ── Configuration build (Jenga) ─────────────────────────────────────────────
 *   Dépendances : NKFont, NKWindow, NKContext, NKGlad, NKLogger,
 *                 NKMath, NKTime, NKContainers, NKMemory, NKCore,
 *                 NKPlatform, NKThreading
 *   Pas de NKRHI.
 *
 * ── Diagnostics disponibles ─────────────────────────────────────────────────
 *   Si atlasTexGL == 0 après init() → debugDumpAtlasPage0() produit
 *   atlas_debug.pgm (visualisable dans tout éditeur d'image supportant PGM).
 *   Ouvre-le pour voir si NKFont produit réellement des pixels.
 */

// ── Plateforme ────────────────────────────────────────────────────────────────
// =============================================================================
//  NkFontGLTest.cpp
//  Démo OpenGL pur — ZÉRO RHI, ZÉRO abstraction.
//
//  OBJECTIF :
//    Isoler si le problème d'affichage vient de NKFont ou du RHI.
//    Ce fichier utilise uniquement : NKFont + glad + NKWindow + NKEvent.
//
//  CE QU'ON VOIT À L'ÉCRAN si NKFont fonctionne :
//    • Fond noir
//    • "Hello NKFont!" en blanc en haut à gauche
//    • "FPS: 60" en jaune
//    • "ABCDEF" en vert (test de base)
//    • Un carré rouge dans le coin haut-droit (témoin OpenGL fonctionne)
//
//  DIAGNOSTIC :
//    • Carré rouge visible + pas de texte → problème NKFont ou shader
//    • Rien visible → problème RHI/fenêtre/contexte
//    • Texte visible → NKFont OK, problème dans le RHI abstrait
//
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"

#include <glad/wgl.h>
#include <glad/gl.h>

#if defined(Bool)
    #undef Bool
#endif

#include "NKContext/Factory/NkContextFactory.h"
#include "NKContext/Core/NkContextDesc.h"
#include "NKContext/Core/NkNativeContextAccess.h"
#include "NKContext/Core/NkOpenGLDesc.h"

// NKFont — l'unique include
#include "NKFont/NKFont.h"

#include <cstdio>
#include <cstring>
#include <cmath>

#include <cstdio>
#include <cstring>

namespace nkentseu { struct NkEntryState; }
using namespace nkentseu;
using namespace nkentseu::math;

// =============================================================================
//  Shaders — GLSL 330 core (compatible GL 3.3+)
// =============================================================================

// Vertex shader partagé 2D : coordonnées NDC déjà calculées par le CPU
static const char* kGLSL_Vert2D = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPos;    // NDC
layout(location = 1) in vec2 aUV;     // UV atlas [0,1]
layout(location = 2) in vec4 aColor;  // RGBA [0,1]
out vec2 vUV;
out vec4 vColor;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vUV   = aUV;
    vColor= aColor;
}
)GLSL";

// Fragment bitmap — alpha direct depuis le canal rouge (sans SDF)
// C'est le plus simple à déboguer : si tu vois du texte → NKFont fonctionne.
static const char* kGLSL_FragBitmap = R"GLSL(
#version 330 core
in  vec2 vUV;
in  vec4 vColor;
out vec4 fragColor;
uniform sampler2D uAtlas;
void main() {
    // Canal rouge = alpha du glyphe (bitmap 1 canal)
    float a = texture(uAtlas, vUV).r;
    // Discard précoce : les pixels vides ne touchent pas le depth buffer
    if (a < 0.02) discard;
    fragColor = vec4(vColor.rgb, vColor.a * a);
}
)GLSL";

// Vertex shader 3D : transformation MVP
static const char* kGLSL_Vert3D = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
out vec2 vUV;
out vec4 vColor;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vUV   = aUV;
    vColor= aColor;
}
)GLSL";

// =============================================================================
//  LoadGL — charger les entry points OpenGL avec GLAD
// =============================================================================

static bool LoadGL(NkIGraphicsContext* ctx) {
    auto loader = NkNativeContext::GetOpenGLProcAddressLoader(ctx);
    if (!loader) { 
        logger.Info("[LoadGL] OpenGL loader introuvable\n"); 
        return false; 
    }

    int ver = gladLoadGL((GLADloadfunc)loader);
    
    if (!ver) { 
        logger.Error("[LoadGL] gladLoadGL échoué\n"); 
        return false; 
    }
    logger.Info("[LoadGL] OpenGL {0}  GLSL {1}\n",
        (const char*)glGetString(GL_VERSION), 
        (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
    return true;
}

// =============================================================================
//  Helpers GL
// =============================================================================

static GLuint glCompileShaderLocal(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[1024]; glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        logger.Info("[NkGLTest] Shader {0} error: {1}\n",
                    type == GL_VERTEX_SHADER ? "VERT" : "FRAG", buf);
        glDeleteShader(s); return 0;
    }
    return s;
}

static GLuint glLinkProg(const char* vert, const char* frag) {
    GLuint vs = glCompileShaderLocal(GL_VERTEX_SHADER,   vert);
    GLuint fs = glCompileShaderLocal(GL_FRAGMENT_SHADER,  frag);
    if (!vs || !fs) { glDeleteShader(vs); glDeleteShader(fs); return 0; }
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs); glDeleteShader(fs);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[1024]; glGetProgramInfoLog(p, sizeof(buf), nullptr, buf);
        logger.Info("[NkGLTest] Link error: {0}\n", buf);
        glDeleteProgram(p); return 0;
    }
    return p;
}

// =============================================================================
//  Structures vertex
// =============================================================================

struct GlVtx2D { float x, y, u, v, r, g, b, a; };
struct GlVtx3D { float x, y, z, u, v, r, g, b, a; };

// =============================================================================
//  NkGLTextCtx — Contexte de rendu texte OpenGL pur
// =============================================================================

class NkGLTextCtx {
public:
    // Tailles de fonte
    float size2D = 18.f;
    float size3D = 64.f;

    // ── Membres NKFont ────────────────────────────────────────────────────────
    NkFontLibrary  mLib;
    NkTextShaper   mShaper;
    nk_uint32      mFontId    = 0u;

    // CORRECTION CRITIQUE : NkShapeResult comme MEMBRE (pas local) 
    // NkShapeResult local = 22 Mo sur la stack → crash garanti
    NkShapeResult  mShape;

    // ── Membres GL ────────────────────────────────────────────────────────────
    GLuint mProg2D  = 0;
    GLuint mProg3D  = 0;
    GLuint mVao2D   = 0, mVbo2D = 0;
    GLuint mVao3D   = 0, mVbo3D = 0;
    GLuint mAtlasTex= 0;  // Texture OpenGL de l'atlas

    NkVector<GlVtx2D> mVerts2D;
    NkVector<GlVtx3D> mVerts3D;

    // ─── Callback upload pixels NKFont → texture GL ───────────────────────────
    //
    // NkFontAtlas appelle ce callback dès qu'une page est "dirty".
    // PROBLÈME TYPIQUE : le callback est appelé mais la texture n'est pas
    // correctement bindée pendant le rendu → texte invisible.
    // Solution : stocker le GLuint dans le contexte et le rebinder au flush().

    static nk_handle uploadCallback(
        nk_uint32       pageIdx,
        const nk_uint8* pixels,
        nk_size         byteSize,
        nk_uint32       w,
        nk_uint32       h,
        nk_uint32       ch,
        nk_handle       existingHandle,
        void*           userPtr)
    {
        NkGLTextCtx* ctx = static_cast<NkGLTextCtx*>(userPtr);
        if (!ctx || !pixels || w == 0 || h == 0) return NK_INVALID_HANDLE;

        // Format interne GL selon le nombre de canaux
        GLenum intFmt = (ch == 1u) ? GL_R8
                      : (ch == 3u) ? GL_RGB8 : GL_RGBA8;
        GLenum extFmt = (ch == 1u) ? GL_RED
                      : (ch == 3u) ? GL_RGB  : GL_RGBA;

        if (ctx->mAtlasTex == 0) {
            // ── Première création ─────────────────────────────────────────────
            glGenTextures(1, &ctx->mAtlasTex);
            glBindTexture(GL_TEXTURE_2D, ctx->mAtlasTex);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // IMPORTANT : pour GL_R8, le shader lit .r
            // Pour la compatibilité maximale, on fait un swizzle R→RGBA
            if (ch == 1u) {
                GLint sw[4] = {GL_RED, GL_RED, GL_RED, GL_RED};
                glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, sw);
            }

            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, intFmt, (GLsizei)w, (GLsizei)h,
                         0, extFmt, GL_UNSIGNED_BYTE, pixels);

            GLenum err = glGetError();
            if (err != GL_NO_ERROR)
                logger.Info("[NkGLTest] glTexImage2D error: 0x{0:X}\n", (unsigned)err);
            else
                logger.Info("[NkGLTest] Texture atlas créée : {0}x{1} ch={2} id={3}\n",
                            w, h, ch, ctx->mAtlasTex);

        } else {
            // ── Mise à jour (nouvelles pages ou glyphes ajoutés) ──────────────
            glBindTexture(GL_TEXTURE_2D, ctx->mAtlasTex);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)w, (GLsizei)h,
                            extFmt, GL_UNSIGNED_BYTE, pixels);
            logger.Info("[NkGLTest] Atlas mis à jour page {0}\n", pageIdx);
        }

        glBindTexture(GL_TEXTURE_2D, 0);

        // Vérifie qu'on a bien des pixels non-nuls
        nk_uint32 nonZero = 0u;
        for (nk_size i = 0u; i < byteSize; ++i) if (pixels[i]) ++nonZero;
        logger.Info("[NkGLTest] Pixels non-nuls dans la page {0}: {1} / {2} ({3:.1f}%)\n",
                    pageIdx, nonZero, byteSize,
                    byteSize ? 100.f*nonZero/byteSize : 0.f);

        return (nk_handle)(nk_uint64)ctx->mAtlasTex;
    }

    // ─── Diagnostic : atlas info ─────────────────────────────────────────────────
    void debugDumpPage0() const {
        if (mLib.Atlas().PageCount() == 0u) {
            logger.Info("[Debug] Atlas vide — aucune page initialisée\n");
            return;
        }
        logger.Info("[Debug] Atlas page count: {0}\n", mLib.Atlas().PageCount());
        logger.Info("[Debug] Atlas glyph count: {0}\n", mLib.Atlas().GlyphCount());
    }

    // ─── Initialisation ───────────────────────────────────────────────────────

    bool init(const char* fontPath) {

        // ① NkFontLibrary — configure atlas via descriptor
        NkFontLibraryDesc libDesc;
        libDesc.defaultDpi    = 96.f;
        libDesc.atlasPageSize = 512u;
        libDesc.atlasChannels = 1u;   // Bitmap mode
        libDesc.atlasPadding  = 2u;

        if (!mLib.Init(libDesc)) {
            logger.Info("[NkGLTest] mLib.Init() FAIL\n");
            return false;
        }

        // ② Connecte le callback AVANT la pré-rastérisation
        mLib.SetAtlasUploadCallback(uploadCallback, this);

        // ③ Charge la fonte
        NkFontResult fr = mLib.LoadFromFile(fontPath,
                                             NK_LOAD_OUTLINES | NK_LOAD_KERNING,
                                             mFontId);
        if (!fr.IsOk()) {
            logger.Info("[NkGLTest] LoadFromFile '{0}' FAIL : {1}\n",
                        fontPath, fr.ToString());
            return false;
        }
        const NkFontInfo* info = mLib.GetFontInfo(mFontId);
        if (info)
            logger.Info("[NkGLTest] Fonte '{0}' {1} — {2} glyphes\n",
                        info->familyName, info->styleName, info->numGlyphs);

        // ④ Pré-rastérise ASCII + Latin-1 aux deux tailles
        NkRasterizeParams rp = NkRasterizeParams::ForBitmap(2u);
        mLib.PrerasterizeRange(mFontId, NkUnicodeRange::Ascii(),  size2D, 96.f, rp);
        mLib.PrerasterizeRange(mFontId, NkUnicodeRange::Latin1(), size2D, 96.f, rp);
        mLib.PrerasterizeRange(mFontId, NkUnicodeRange::Ascii(),  size3D, 96.f, rp);

        // ⑤ Upload → callback uploadCallback() est appelé ici
        nk_uint32 pages = mLib.UploadAtlas();
        logger.Info("[NkGLTest] {0} page(s) uploadée(s)\n", pages);

        // Diagnostic immédiat
        debugDumpPage0();

        if (mAtlasTex == 0) {
            logger.Info("[NkGLTest] ERREUR CRITIQUE : atlasTexGL == 0 après upload\n");
            logger.Info("[NkGLTest] Vérifier atlas_debug.pgm pour savoir si NKFont produit des pixels\n");
            return false;
        }

        // ⑦ Shaders GL
        mProg2D = glLinkProg(kGLSL_Vert2D, kGLSL_FragBitmap);
        mProg3D = glLinkProg(kGLSL_Vert3D, kGLSL_FragBitmap);
        if (!mProg2D || !mProg3D) {
            logger.Info("[NkGLTest] Shader compilation FAIL\n");
            return false;
        }

        // ⑧ VAO/VBO 2D
        glGenVertexArrays(1, &mVao2D);
        glGenBuffers(1, &mVbo2D);
        glBindVertexArray(mVao2D);
        glBindBuffer(GL_ARRAY_BUFFER, mVbo2D);
        glBufferData(GL_ARRAY_BUFFER, 12288 * sizeof(GlVtx2D), nullptr, GL_DYNAMIC_DRAW);
        // aPos = vec2 @ offset 0
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GlVtx2D), (void*)offsetof(GlVtx2D,x));
        // aUV = vec2 @ offset 8
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GlVtx2D), (void*)offsetof(GlVtx2D,u));
        // aColor = vec4 @ offset 16
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GlVtx2D), (void*)offsetof(GlVtx2D,r));
        glBindVertexArray(0);

        // ⑨ VAO/VBO 3D
        glGenVertexArrays(1, &mVao3D);
        glGenBuffers(1, &mVbo3D);
        glBindVertexArray(mVao3D);
        glBindBuffer(GL_ARRAY_BUFFER, mVbo3D);
        glBufferData(GL_ARRAY_BUFFER, 6144 * sizeof(GlVtx3D), nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GlVtx3D), (void*)offsetof(GlVtx3D,x));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GlVtx3D), (void*)offsetof(GlVtx3D,u));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GlVtx3D), (void*)offsetof(GlVtx3D,r));
        glBindVertexArray(0);

        mVerts2D.Reserve(12288);
        mVerts3D.Reserve(6144);

        logger.Info("[NkGLTest] Initialisation réussie. atlasTexGL={0}\n", mAtlasTex);
        return true;
    }

    // ─── drawText2D : pixel → NDC + accumule les vertices ────────────────────

    void drawText2D(const char* text,
                    float x, float y, float sw, float sh,
                    float r=1.f,float g=1.f,float b=1.f,float a=1.f,
                    float ptSize=0.f) {
        if (!text || sw<=0.f || sh<=0.f) {
            if (!text) logger.Info("[DrawText2D] EARLY EXIT: text=NULL");
            else if (sw<=0.f) logger.Info("[DrawText2D] EARLY EXIT: sw={0}", sw);
            else logger.Info("[DrawText2D] EARLY EXIT: sh={0}", sh);
            return;
        }
        if (ptSize<=0.f) ptSize = size2D;
        NkFontFace* face = mLib.GetFace(mFontId);
        if (!face) {
            logger.Info("[DrawText2D] EARLY EXIT: face=NULL");
            return;
        }
        logger.Info("[DrawText2D] START rendering '{0}' ({1}x{2})", text, (int)sw, (int)sh);

        NkShapeParams sp;
        sp.face      = face;
        sp.pointSize = ptSize;
        sp.dpi       = 96.f;
        sp.renderMode= NkGlyphRenderMode::NK_RENDER_BITMAP;

        logger.Info("[DrawText2D] ShapeParams ready (ptSize={0}, dpi={1})", (int)ptSize, (int)sp.dpi);

        // CORRECT : réutilise le membre mShape (pas de local 22 Mo)
        mShape.Clear();
        logger.Info("[DrawText2D] Before ShapeUtf8");
        NkFontResult shapeRes = mShaper.ShapeUtf8(text, (nk_size)-1, sp, mShape);
        logger.Info("[DrawText2D] After ShapeUtf8 (result OK={0}, lines={1})", shapeRes.IsOk(), mShape.lineCount);
        if (!shapeRes.IsOk()) {
            logger.Debug("[DrawText2D] ShapeUtf8 FAILED: {0} (text='{1}')", shapeRes.ToString(), text);
            return;
        }
        if (mShape.lineCount == 0) {
            logger.Debug("[DrawText2D] ShapeUtf8 returned 0 lines from '{0}'", text);
            return;
        }
        logger.Debug("[DrawText2D] ShapeUtf8 OK for '{0}': {1} lines", 
                     text, mShape.lineCount);

        NkRasterizeParams rp = NkRasterizeParams::ForBitmap(2u);

        for (nk_uint32 l=0u; l<mShape.lineCount; ++l) {
            const NkGlyphRun& run = mShape.Line(l);
            logger.Info("[DrawText2D] Line {0}: {1} glyphs", l, run.glyphCount);
            float baseY = y + run.baselineY;
            
            nk_uint32 skipped_ws=0, skipped_result=0, skipped_null=0, skipped_invalid=0, skipped_notinatlasу=0, added=0;

            for (nk_uint32 gi=0u; gi<run.glyphCount; ++gi) {
                const NkShapedGlyph& sg = run.Glyph(gi);
                if (sg.isWhitespace || sg.isLineBreak) { skipped_ws++; continue; }

                const NkCachedGlyph* gl = nullptr;
                NkFontResult glyphRes = mLib.GetGlyph(mFontId,sg.codepoint,ptSize,96.f,rp,gl);
                if (!glyphRes.IsOk()) { skipped_result++; continue; }
                if (!gl) { skipped_null++; continue; }
                // if (!gl->isValid) { skipped_invalid++; continue; }  // TEMPORARILY DISABLED FOR TESTING
                if (!gl->isInAtlas) { skipped_notinatlasу++; continue; }
                
                added++;

                // Coordonnées pixel du quad
                float px0 = x + sg.pen.x + sg.xOffset + sg.metrics.bearingX;
                float py0 = baseY - sg.metrics.bearingY;
                float px1 = px0 + sg.metrics.width;
                float py1 = py0 + sg.metrics.height;

                // Pixel → NDC (Y-flip : écran OpenGL = Y-up)
                float nx0 = (px0/sw)*2.f-1.f;
                float ny0 = 1.f-(py0/sh)*2.f;
                float nx1 = (px1/sw)*2.f-1.f;
                float ny1 = 1.f-(py1/sh)*2.f;

                float u0=gl->uvX0, v0=gl->uvY0;
                float u1=gl->uvX1, v1=gl->uvY1;

                if (added == 1) {  // Log UVs for first glyph only
                    logger.Info("[DrawText2D] First glyph UV: ({0},{1}) to ({2},{3})", u0, v0, u1, v1);
                }

                // 2 triangles (6 vertices)
                mVerts2D.PushBack({nx0,ny0,u0,v0,r,g,b,a});
                mVerts2D.PushBack({nx1,ny0,u1,v0,r,g,b,a});
                mVerts2D.PushBack({nx0,ny1,u0,v1,r,g,b,a});
                mVerts2D.PushBack({nx1,ny0,u1,v0,r,g,b,a});
                mVerts2D.PushBack({nx1,ny1,u1,v1,r,g,b,a});
                mVerts2D.PushBack({nx0,ny1,u0,v1,r,g,b,a});
            }
            logger.Info("[DrawText2D] Line {0} summary: added={1} ws={2} result={3} null={4} invalid={5} notatlas={6}",
                        l, added, skipped_ws, skipped_result, skipped_null, skipped_invalid, skipped_notinatlasу);
        }
    }

    // ─── drawText3D : espace local → espace monde via worldMat ───────────────

    void drawText3D(const char* text, const NkMat4f& worldMat,
                    float r=1.f,float g=1.f,float b=1.f,float a=1.f,
                    float ptSize=0.f, bool centered=true) {
        if (!text) return;
        if (ptSize<=0.f) ptSize = size3D;
        NkFontFace* face = mLib.GetFace(mFontId);
        if (!face) return;

        NkShapeParams sp;
        sp.face      = face;
        sp.pointSize = ptSize;
        sp.dpi       = 96.f;
        sp.renderMode= NkGlyphRenderMode::NK_RENDER_BITMAP;

        mShape.Clear();
        if (!mShaper.ShapeUtf8(text,(nk_size)-1,sp,mShape).IsOk()) return;

        const float pxW = 0.005f;
        float offX = centered ? -mShape.totalWidth *0.5f*pxW : 0.f;
        float offY = centered ?  mShape.totalHeight*0.5f*pxW : 0.f;

        NkRasterizeParams rp = NkRasterizeParams::ForBitmap(2u);

        // Transformation local → monde via worldMat (colonne-major)
        auto wpt = [&](float lx, float ly) {
            const float* m = worldMat.data;
            return NkVec3f{
                m[0]*lx + m[4]*ly + m[12],
                m[1]*lx + m[5]*ly + m[13],
                m[2]*lx + m[6]*ly + m[14]
            };
        };

        for (nk_uint32 l=0u; l<mShape.lineCount; ++l) {
            const NkGlyphRun& run = mShape.Line(l);
            float lineY = offY - run.baselineY*pxW;

            for (nk_uint32 gi=0u; gi<run.glyphCount; ++gi) {
                const NkShapedGlyph& sg = run.Glyph(gi);
                if (sg.isWhitespace||sg.isLineBreak) continue;

                const NkCachedGlyph* gl = nullptr;
                if (!mLib.GetGlyph(mFontId,sg.codepoint,ptSize,96.f,rp,gl).IsOk()
                    ||!gl||!gl->isValid||!gl->isInAtlas) continue;

                float lx0 = offX+(sg.pen.x+sg.xOffset+sg.metrics.bearingX)*pxW;
                float ly0 = lineY+sg.metrics.bearingY*pxW;
                float lx1 = lx0+sg.metrics.width*pxW;
                float ly1 = ly0-sg.metrics.height*pxW;

                NkVec3f tl=wpt(lx0,ly0), tr=wpt(lx1,ly0);
                NkVec3f bl=wpt(lx0,ly1), br=wpt(lx1,ly1);
                float u0=gl->uvX0,v0=gl->uvY0,u1=gl->uvX1,v1=gl->uvY1;

                mVerts3D.PushBack({tl.x,tl.y,tl.z,u0,v0,r,g,b,a});
                mVerts3D.PushBack({tr.x,tr.y,tr.z,u1,v0,r,g,b,a});
                mVerts3D.PushBack({bl.x,bl.y,bl.z,u0,v1,r,g,b,a});
                mVerts3D.PushBack({tr.x,tr.y,tr.z,u1,v0,r,g,b,a});
                mVerts3D.PushBack({br.x,br.y,br.z,u1,v1,r,g,b,a});
                mVerts3D.PushBack({bl.x,bl.y,bl.z,u0,v1,r,g,b,a});
            }
        }
    }

    // ─── flush : upload atlas dirty + draw calls ──────────────────────────────

    void flush(float sw, float sh, const NkMat4f& view, const NkMat4f& proj) {
        // Upload nouvelles pages si nécessaire (lazy rasterization en cours de frame)
        mLib.UploadAtlas();

        if (mAtlasTex == 0) {
            logger.Info("[NkGLTest] flush : mAtlasTex == 0, rien à dessiner\n");
            mVerts2D.Clear(); mVerts3D.Clear();
            return;
        }

        // Alpha blending
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

        // Bind la texture atlas
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mAtlasTex);

        // ── Draw 2D ──────────────────────────────────────────────────────────
        const nk_uint32 n2 = mVerts2D.Size();
        if (n2 > 0u) {
            glDisable(GL_DEPTH_TEST);
            glUseProgram(mProg2D);
            glUniform1i(glGetUniformLocation(mProg2D, "uAtlas"), 0);

            glBindVertexArray(mVao2D);
            glBindBuffer(GL_ARRAY_BUFFER, mVbo2D);
            glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(n2*sizeof(GlVtx2D)), mVerts2D.Begin());
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)n2);

            GLenum err = glGetError();
            if (err != GL_NO_ERROR)
                logger.Info("[NkGLTest] 2D draw error 0x{0:X}\n", (unsigned)err);
            else
                logger.Info("[NkGLTest] 2D draw OK : {0} vtx, {1} glyphes\n", n2, n2/6);

            glBindVertexArray(0);
        }

        // ── Draw 3D ──────────────────────────────────────────────────────────
        const nk_uint32 n3 = mVerts3D.Size();
        if (n3 > 0u) {
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);

            glUseProgram(mProg3D);
            glUniform1i(glGetUniformLocation(mProg3D, "uAtlas"), 0);

            // MVP = proj × view (positions 3D déjà en world space)
            NkMat4f mvp = proj * view;
            float mat[16];
            for (int i = 0; i < 16; ++i) mat[i] = mvp.data[i];
            glUniformMatrix4fv(glGetUniformLocation(mProg3D, "uMVP"), 1, GL_FALSE, mat);

            glBindVertexArray(mVao3D);
            glBindBuffer(GL_ARRAY_BUFFER, mVbo3D);
            glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(n3*sizeof(GlVtx3D)), mVerts3D.Begin());
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)n3);

            glDepthMask(GL_TRUE);
            glBindVertexArray(0);
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        glDisable(GL_BLEND);

        mVerts2D.Clear();
        mVerts3D.Clear();
    }

    // ─── Nettoyage ────────────────────────────────────────────────────────────

    void shutdown() {
        if (mVao2D)    { glDeleteVertexArrays(1, &mVao2D);  mVao2D  = 0; }
        if (mVbo2D)    { glDeleteBuffers(1, &mVbo2D);        mVbo2D  = 0; }
        if (mVao3D)    { glDeleteVertexArrays(1, &mVao3D);  mVao3D  = 0; }
        if (mVbo3D)    { glDeleteBuffers(1, &mVbo3D);        mVbo3D  = 0; }
        if (mProg2D)   { glDeleteProgram(mProg2D);           mProg2D = 0; }
        if (mProg3D)   { glDeleteProgram(mProg3D);           mProg3D = 0; }
        if (mAtlasTex) { glDeleteTextures(1, &mAtlasTex);   mAtlasTex=0; }
        mLib.Shutdown();
    }
};

// =============================================================================
//  nkmain — Point d'entrée
// =============================================================================

int nkmain(const NkEntryState& state) {

    // ── Fenêtre avec contexte OpenGL ──────────────────────────────────────────
    NkWindowConfig wc;
    wc.title     = "NKFont GL Test — OpenGL direct sans NKRHI";
    wc.width     = 1280;
    wc.height    = 720;
    wc.centered  = true;
    wc.resizable = true;

    NkWindow window;
    if (!window.Create(wc)) {
        logger.Info("[GLTest] Fenetre : echec\n");
        return 1;
    }

    NkContextDesc desc;
    desc.api    = NkGraphicsApi::NK_API_OPENGL;
    desc.opengl.majorVersion = 4;
    desc.opengl.minorVersion = 6;
    desc.opengl.profile = NkGLProfile::Core;
    desc.opengl.contextFlags = NkGLContextFlags::ForwardCompat | NkGLContextFlags::Debug;
    desc.opengl.runtime.installDebugCallback = true;
    desc.opengl.msaaSamples        = 4;
    desc.opengl.srgbFramebuffer    = true;
    desc.opengl.swapInterval       = NkGLSwapInterval::AdaptiveVSync;
    desc.opengl.runtime.autoLoadEntryPoints  = false;
    desc.opengl.runtime.validateVersion      = true;

    auto ctx = NkContextFactory::Create(window, desc);
    if (!ctx)  { 
        logger.Error("[RHIDemo] Contexte GL échoué"); 
        window.Close(); 
        return -3; 
    }
    if (!LoadGL(ctx)) { 
        ctx->Shutdown(); 
        window.Close(); 
        return -4; 
    }

    logger.Info("[GLTest] OpenGL : {0} / {1}\n",
                (const char*)glGetString(GL_VERSION),
                (const char*)glGetString(GL_RENDERER));

    // IMPORTANT : NkGLTextCtx sur le TAS (contient NkShapeResult ~22 Mo)
    NkGLTextCtx* gltctx = new NkGLTextCtx();
    gltctx->size2D = 18.f;
    gltctx->size3D = 64.f;

    if (!gltctx->init("Resources/Fonts/ProggyClean.ttf")) {
        logger.Info("[GLTest] init() FAIL — voir atlas_debug.pgm\n");
        // Même en cas d'échec GL, les pixels ont peut-être été générés
        gltctx->debugDumpPage0();
        delete ctx;
        window.Close();
        return 1;
    }

    // ── Variables boucle ──────────────────────────────────────────────────────
    NkChrono timer;
    double tPrev = timer.Elapsed().seconds;
    double fpsAcc = 0.0; int fpsCnt = 0; float fpsDisp = 0.f;
    float rotAngle = 0.f, bobAngle = 0.f;
    bool running = true;

    auto& ev = NkEvents();
    ev.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running = false; });
    ev.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        if (e->GetKey() == NkKey::NK_ESCAPE) running = false;
        // D : dump atlas pour debug
        if (e->GetKey() == NkKey::NK_D) gltctx->debugDumpPage0();
    });

    nk_uint32 W = window.GetSize().width;
    nk_uint32 H = window.GetSize().height;
    ev.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        W = (nk_uint32)e->GetWidth(); H = (nk_uint32)e->GetHeight();
        glViewport(0, 0, (GLsizei)W, (GLsizei)H);
    });

    // ── Boucle principale ─────────────────────────────────────────────────────
    logger.Info("[GLTest] Entrant dans la boucle...\n");
    while (running) {
        double now = timer.Elapsed().seconds;
        float dt = (float)(now - tPrev); tPrev = now;
        fpsAcc += dt; ++fpsCnt;
        if (fpsAcc >= 0.5) {
            fpsDisp = (float)fpsCnt / (float)fpsAcc;
            fpsAcc = 0.0; fpsCnt = 0;
        }
        rotAngle += 20.f * dt;
        bobAngle += 90.f * dt;

        ev.PollEvents();
        if (!running) break;

        glViewport(0, 0, (GLsizei)W, (GLsizei)H);
        glClearColor(0.10f, 0.10f, 0.14f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        NkMat4f view = NkMat4f::LookAt(
            NkVec3f(0.f, 1.5f, 4.f), NkVec3f(0,0,0), NkVec3f(0,1,0));
        NkMat4f proj = NkMat4f::Perspective(
            NkAngle(60.f), (float)W / (float)H, 0.1f, 100.f);
        
        if (ctx->BeginFrame()) {

            // ── Texte 3D ─────────────────────────────────────────────────────────

            // "NKENTSEU" — flotte et tourne
            {
                float y = 1.2f + 0.12f * NkSin(NkToRadians(bobAngle));
                NkMat4f w = NkMat4f::Translation(NkVec3f(0.f, y, 0.f))
                        * NkMat4f::RotationY(NkAngle(rotAngle));
                gltctx->drawText3D("NKENTSEU", w, 1.f, 0.85f, 0.2f, 1.f, 64.f, true);
            }

            // "GL Test" au sol (plan horizontal)
            {
                NkMat4f w = NkMat4f::Translation(NkVec3f(0.f, -0.3f, 0.3f))
                        * NkMat4f::RotationX(NkAngle(-90.f));
                gltctx->drawText3D("NKFont GL Test", w, 0.4f, 0.8f, 1.f, 0.9f, 32.f, true);
            }

            // ── Texte 2D HUD ─────────────────────────────────────────────────────

            // Ligne info
            {
                char buf[256];
                snprintf(buf, sizeof(buf),
                        "FPS: %.0f  Atlas: %u page(s)  Glyphes: %u",
                        fpsDisp,
                        gltctx->mLib.Atlas().PageCount(),
                        gltctx->mLib.Atlas().GlyphCount());
                gltctx->drawText2D(buf, 12.f, 12.f, (float)W, (float)H,
                                1.f, 1.f, 0.3f, 1.f, 16.f);
            }

            gltctx->drawText2D("NKFont from-scratch — sans FreeType, sans NKRHI",
                            12.f, 34.f, (float)W, (float)H,
                            0.6f, 0.9f, 1.f, 1.f, 14.f);

            // Lignes de validation caractères
            gltctx->drawText2D("ABCDEFGHIJKLMNOPQRSTUVWXYZ",
                            12.f, 60.f, (float)W, (float)H, 1.f, 1.f, 1.f, 1.f, 18.f);
            gltctx->drawText2D("abcdefghijklmnopqrstuvwxyz",
                            12.f, 84.f, (float)W, (float)H, 0.9f, 0.9f, 0.9f, 1.f, 18.f);
            gltctx->drawText2D("0123456789 !@#$%^&*()",
                            12.f, 108.f, (float)W, (float)H, 0.8f, 0.8f, 1.f, 1.f, 18.f);

            // Texte coloré
            gltctx->drawText2D("Bonjour le monde!", 12.f, 140.f, (float)W, (float)H,
                            1.f, 0.5f, 0.5f, 1.f, 22.f);
            gltctx->drawText2D("Hello World!",      12.f, 168.f, (float)W, (float)H,
                            0.5f, 1.f, 0.5f, 1.f, 22.f);
            gltctx->drawText2D("NKFont rocks!",     12.f, 196.f, (float)W, (float)H,
                            0.5f, 0.5f, 1.f, 1.f, 22.f);

            // Instructions
            gltctx->drawText2D("D = dump atlas PGM  |  ESC = quitter",
                            12.f, (float)H - 28.f, (float)W, (float)H,
                            0.5f, 0.5f, 0.5f, 0.8f, 13.f);

            // ── Flush tous les textes accumulés ───────────────────────────────────
            logger.Info("[MainLoop] Before flush: mVerts2D.size()={0}", gltctx->mVerts2D.Size());
            gltctx->flush((float)W, (float)H, view, proj);
            logger.Info("[MainLoop] After flush");

            // ── Swap ──────────────────────────────────────────────────────────────

            ctx->EndFrame();
            ctx->Present();
        }
    }

    gltctx->shutdown();
    delete gltctx;
    ctx->Shutdown();
    window.Close();
    logger.Info("[GLTest] Terminé.\n");
    return 0;
}