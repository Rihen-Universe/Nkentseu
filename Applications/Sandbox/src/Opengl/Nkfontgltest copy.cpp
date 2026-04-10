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

namespace nkentseu { struct NkEntryState; }
using namespace nkentseu;

// =============================================================================
//  SHADERS GLSL MINIMAUX — Texte 2D bitmap
//  ─────────────────────────────────────────────────────────────────────────
//  Vertex :  [vec2 pos NDC, vec2 uv] → pas d'UBO, transformation inline
//  Fragment : atlas R8 → alpha. Couleur passée en uniform vec4.
// =============================================================================

static const char* kFontVert = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPos;  // NDC [-1, +1]
layout(location = 1) in vec2 aUV;   // UV atlas [0, 1]
out vec2 vUV;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vUV = aUV;
}
)GLSL";

static const char* kFontFrag = R"GLSL(
#version 330 core
in  vec2 vUV;
out vec4 fragColor;
uniform sampler2D uAtlas;
uniform vec4      uColor;
void main() {
    // Hybrid: Lit l'atlas ET affiche la couleur
    // Si alpha > 0 : affiche uColor avec alpha
    // Si alpha = 0 : discard (transparent)
    float alpha = texture(uAtlas, vUV).r;
    
    if (alpha < 0.01) {
        discard;  // Zone vide, transparent
    }
    
    // Affiche la couleur avec l'alpha de l'atlas
    fragColor = vec4(uColor.rgb, uColor.a * alpha);
}
)GLSL";

// Shader de diagnostic : quad rouge pur, sans texture
static const char* kDiagVert = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPos;
void main() { gl_Position = vec4(aPos, 0.0, 1.0); }
)GLSL";

static const char* kDiagFrag = R"GLSL(
#version 330 core
out vec4 fragColor;
void main() { fragColor = vec4(1.0, 0.0, 0.0, 1.0); }
)GLSL";

// =============================================================================
//  Helpers OpenGL — compilation shader
// =============================================================================

static GLuint compileShader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        logger.Info("[GL] Shader compile error: {0}\n", log);
        glDeleteShader(sh);
        return 0u;
    }
    return sh;
}

static GLuint linkProgram(const char* vert, const char* frag) {
    GLuint vs = compileShader(GL_VERTEX_SHADER,   vert);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, frag);
    if (!vs || !fs) { glDeleteShader(vs); glDeleteShader(fs); return 0u; }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    glDeleteShader(vs); glDeleteShader(fs);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        logger.Info("[GL] Program link error: {0}\n", log);
        glDeleteProgram(prog);
        return 0u;
    }
    return prog;
}

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
//  Struct vertex texte
// =============================================================================

struct FontVtx { float x, y, u, v; };

// =============================================================================
//  NkFontGLRenderer — tout le rendu texte en OpenGL pur
// =============================================================================

class NkFontGLRenderer {
public:
    static constexpr int MAX_VERTS = 6 * 4096; // 4096 glyphes × 6 vertices

    // ── Textures atlas par page ────────────────────────────────────────────────
    // On supporte jusqu'à 8 pages comme NkFontAtlas.
    static constexpr int MAX_ATLAS_PAGES = 8;
    GLuint mAtlasTex[MAX_ATLAS_PAGES] = {};
    int    mAtlasPageCount = 0;

    // ── GPU objects ───────────────────────────────────────────────────────────
    GLuint mProgFont  = 0;  // shader texte
    GLuint mProgDiag  = 0;  // shader diagnostic (quad rouge)
    GLuint mVAO  = 0, mVBO  = 0;  // pour le texte
    GLuint mDiagVAO = 0, mDiagVBO = 0; // pour le quad rouge

    // ── NKFont ────────────────────────────────────────────────────────────────
    NkFontLibrary* mLib   = nullptr;
    nk_uint32      mFontId = 0u;
    NkTextShaper   mShaper;
    NkShapeResult  mShapeResult;  // MEMBRE (pas local !) — voir crash précédent

    // ── CPU vertex buffer ─────────────────────────────────────────────────────
    FontVtx mVerts[MAX_VERTS];
    int     mVertCount = 0;

    // ── Dimensions fenêtre ────────────────────────────────────────────────────
    int mW = 1280, mH = 720;

    bool init(const char* fontPath, int w, int h) {
        logger.Info("\n\n========== INIT RENDERER START ==========\n");
        mW = w; mH = h;

        // ── Compile shaders ───────────────────────────────────────────────────
        mProgFont = linkProgram(kFontVert, kFontFrag);
        mProgDiag = linkProgram(kDiagVert, kDiagFrag);
        if (!mProgFont || !mProgDiag) {
            logger.Info("[NkFontGLRenderer] Echec compilation shaders\n");
            return false;
        }
        logger.Info("[NkFontGLRenderer] Shaders compiles OK\n");

        // ── VAO/VBO texte ─────────────────────────────────────────────────────
        glGenVertexArrays(1, &mVAO);
        glGenBuffers(1, &mVBO);
        glBindVertexArray(mVAO);
        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(mVerts), nullptr, GL_DYNAMIC_DRAW);
        // Attribut 0 : pos (vec2)
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(FontVtx),
                              (void*)offsetof(FontVtx, x));
        glEnableVertexAttribArray(0);
        // Attribut 1 : uv (vec2)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(FontVtx),
                              (void*)offsetof(FontVtx, u));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);

        // ── VAO/VBO diagnostic (quad rouge coin haut-droit) ───────────────────
        // NDC : x=[0.8, 1.0], y=[0.8, 1.0]
        float diagVerts[] = {
            0.8f, 0.8f,   1.0f, 0.8f,   0.8f, 1.0f,
            1.0f, 0.8f,   1.0f, 1.0f,   0.8f, 1.0f,
        };
        glGenVertexArrays(1, &mDiagVAO);
        glGenBuffers(1, &mDiagVBO);
        glBindVertexArray(mDiagVAO);
        glBindBuffer(GL_ARRAY_BUFFER, mDiagVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(diagVerts), diagVerts, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
        logger.Info("[NkFontGLRenderer] VAO/VBO crees OK\n");

        // ── NkFontLibrary ─────────────────────────────────────────────────────
        mLib = new NkFontLibrary();
        if (!mLib) return false;

        NkFontLibraryDesc libDesc;
        libDesc.defaultDpi = 96.f;
        if (!mLib->Init(libDesc)) {
            logger.Info("[NkFontGLRenderer] Echec NkFontLibrary::init\n");
            return false;
        }
        logger.Info("[NkFontGLRenderer] NkFontLibrary initialisee\n");

        // Callback d'upload OpenGL direct
        mLib->SetAtlasUploadCallback(glUploadCallback, this);

        // ── Charge la fonte ───────────────────────────────────────────────────
        nk_uint32 flags = NK_LOAD_OUTLINES | NK_LOAD_KERNING;
        NkFontResult fr = mLib->LoadFromFile(fontPath, flags, mFontId);
        if (!fr.IsOk()) {
            logger.Info("[NkFontGLRenderer] Fonte '{0}' introuvable : {1}\n", fontPath, fr.ToString());
            // Essai chemin relatif alternatif
            fr = mLib->LoadFromFile("ProggyClean.ttf", flags, mFontId);
            if (!fr.IsOk()) {
                logger.Info("[NkFontGLRenderer] Aucune fonte trouvee\n");
                return false;
            }
        }
        const NkFontInfo* info = mLib->GetFontInfo(mFontId);
        if (info)
            logger.Info("[NkFontGLRenderer] Fonte : '{0}' {1} ({2} glyphes)\n",
                        info->familyName, info->styleName, info->numGlyphs);

        // ── Pré-rastérise ASCII ────────────────────────────────────────────────
        NkRasterizeParams rp = NkRasterizeParams::ForBitmap();
        
        FILE* initlog = fopen("C:\\Users\\Rihen\\Desktop\\initlog.txt", "w");
        fprintf(initlog, "=== INIT START ===\n");
        
        fprintf(initlog, "AVANT PrerasterizeRange #1\n");
        fflush(initlog);
        mLib->PrerasterizeRange(mFontId, NkUnicodeRange::Ascii(), 18.f, 96.f, rp);
        fprintf(initlog, "APRES PrerasterizeRange #1\n");
        fflush(initlog);
                // Test : Vérifie les dimensions d'un glyphe
        fprintf(initlog, "\n=== GLYPH METRICS DEBUG ===\n");
        const NkCachedGlyph* testGlyph = nullptr;
        mLib->GetGlyph(mFontId, 'A', 18.f, 96.f, rp, testGlyph);
        if (testGlyph && testGlyph->isValid) {
            fprintf(initlog, "Glyph 'A': width=%.1f height=%.1f inAtlas=%d\n", 
                    testGlyph->metrics.width, testGlyph->metrics.height, testGlyph->isInAtlas);
        } else {
            fprintf(initlog, "Glyph 'A': NOT FOUND or INVALID\n");
        }
        fflush(initlog);
                mLib->PrerasterizeRange(mFontId, NkUnicodeRange::Ascii(), 14.f, 96.f, rp);
        fprintf(initlog, "AVANT UploadAtlas()\n");
        fflush(initlog);
        
        nk_uint32 pages = mLib->UploadAtlas();
        
        fprintf(initlog, "APRES UploadAtlas() : pages=%u\n", pages);
        fprintf(initlog, "DEBUG: mAtlasPageCount=%d\n", mAtlasPageCount);
        fprintf(initlog, "DEBUG: mAtlasTex[0]=%u\n", mAtlasTex[0]);
        fprintf(initlog, "DEBUG: mAtlasTex[1]=%u\n", mAtlasTex[1]);
        fflush(initlog);

        if (pages == 0) {
            fprintf(initlog, "WARNING: 0 pages uploadees !\n");
            fflush(initlog);
        }

        // ── Vérifie les textures ──────────────────────────────────
        fprintf(initlog, "Verifying %d pages...\n", mAtlasPageCount);
        for (int i = 0; i < mAtlasPageCount; ++i) {
            fprintf(initlog, "  Page %d -> texID=%u\n", i, mAtlasTex[i]);
        }
        fprintf(initlog, "Dense: pages=%u pageCount=%d tex0=%u\n", pages, mAtlasPageCount, mAtlasTex[0]);
        fflush(initlog);

        logger.Info("========== INIT RENDERER END (SUCCESS) ==========\n\n");
        return true;
    }

    // ── Callback d'upload OpenGL ─────────────────────────────────────────────

    static nk_handle glUploadCallback(
        nk_uint32       pageIndex,
        const nk_uint8* pixelData,
        nk_size         dataSize,
        nk_uint32       width,
        nk_uint32       height,
        nk_uint32       channels,
        nk_handle       gpuHandle,
        void*           userPtr
    ) {
        // Log en fichier relatif au répertoire courant
        FILE* cblog = fopen("C:\\Users\\Rihen\\Desktop\\callback_debug.txt", "a");
        auto* self = static_cast<NkFontGLRenderer*>(userPtr);
        
        fprintf(cblog, "\n>>> CALLBACK PAGE %u: %ux%u ch=%u\n", pageIndex, width, height, channels);
        fprintf(cblog, ">>> pixelData=%p dataSize=%zu\n", pixelData, dataSize);
        
        if (!self || !pixelData) {
            fprintf(cblog, ">>> ERROR: self=%p pixelData=%p\n", self, pixelData);
            if (cblog) fflush(cblog); fclose(cblog);
            return 0ull;
        }

        // Analyser les données
        nk_uint32 nonZeroPixels = 0;
        for (nk_size i = 0u; i < dataSize; ++i) {
            if (pixelData[i] != 0u) nonZeroPixels++;
        }
        
        fprintf(cblog, ">>> Data: %u / %zu non-zero pixels\n", nonZeroPixels, dataSize);
        if (dataSize > 0) {
            fprintf(cblog, ">>> First 32 bytes: ");
            for (int i = 0; i < 32 && i < (int)dataSize; ++i)  
                fprintf(cblog, "%02X ", pixelData[i]);
            fprintf(cblog, "\n");
        }
        fflush(cblog);
        
        // Loguer le résultat au logger aussi
        logger.Info(">>> CALLBACK: page={0} {1}x{2} {3}ch - {4} non-zero pixels / {5}\n",
                    pageIndex, width, height, channels, nonZeroPixels, dataSize);

        GLuint texID = (GLuint)(gpuHandle & 0xFFFFFFFFull);

        if (texID == 0u) {
            // Première fois : crée la texture
            glGenTextures(1, &texID);
            glBindTexture(GL_TEXTURE_2D, texID);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Swizzle R8 → RGBA (le fragment shader lit .r comme alpha)
            if (channels == 1u) {
                GLint swizzle[] = { GL_RED, GL_RED, GL_RED, GL_RED };
                glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
            }

            GLenum fmt    = (channels == 1u) ? GL_RED  :
                            (channels == 3u) ? GL_RGB  : GL_RGBA;
            GLenum intfmt = (channels == 1u) ? GL_R8   :
                            (channels == 3u) ? GL_RGB8 : GL_RGBA8;

            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, intfmt, (GLsizei)width, (GLsizei)height,
                         0, fmt, GL_UNSIGNED_BYTE, pixelData);
            
            GLenum err = glGetError();
            fprintf(stdout, ">>> glTexImage2D: texID=%u, glGetError()=%u (0=OK)\n", texID, err);
            fflush(stdout);

            logger.Info("[GL] Atlas page {0} : texture cree texID={1} ({2}x{3} {4}ch)\n",
                        pageIndex, texID, width, height, channels);
        } else {
            // Mise à jour partielle
            glBindTexture(GL_TEXTURE_2D, texID);
            GLenum fmt = (channels == 1u) ? GL_RED :
                         (channels == 3u) ? GL_RGB  : GL_RGBA;
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                            (GLsizei)width, (GLsizei)height,
                            fmt, GL_UNSIGNED_BYTE, pixelData);
            
            GLenum err = glGetError();
            fprintf(stdout, ">>> glTexSubImage2D: texID=%u, glGetError()=%u (0=OK)\n", texID, err);
            fflush(stdout);
        }
        glBindTexture(GL_TEXTURE_2D, 0);

        // Stocke dans la table
        if ((int)pageIndex < MAX_ATLAS_PAGES) {
            self->mAtlasTex[pageIndex] = texID;
            if ((int)pageIndex >= self->mAtlasPageCount)
                self->mAtlasPageCount = (int)pageIndex + 1;
        }

        fprintf(cblog, ">>> Stored: mAtlasTex[%u]=%u\n", pageIndex, texID);
        fflush(cblog);
        fclose(cblog);
        
        return (nk_handle)texID;
    }

    // ── Mise à jour de la taille fenêtre ─────────────────────────────────────

    void resize(int w, int h) { mW = w; mH = h; }

    // ── Méthode principale de rendu texte ─────────────────────────────────────

    void drawText(const char* text, float px, float py,
                  float ptSize, float r, float g, float b, float a = 1.f) {
        if (!mLib || !text || mAtlasPageCount == 0) {
            logger.Info("[drawText] Early exit: mLib={0} text={1} pageCount={2}\n", 
                !!mLib, !!text, mAtlasPageCount);
            return;
        }

        NkFontFace* face = mLib->GetFace(mFontId);
        if (!face) {
            logger.Info("[drawText] GetFace failed\n");
            return;
        }

        NkShapeParams sp;
        sp.face          = face;
        sp.pointSize     = ptSize;
        sp.dpi           = 96.f;
        sp.renderMode    = NkGlyphRenderMode::NK_RENDER_BITMAP;
        sp.enableKerning = true;

        // MEMBRE — pas de stack overflow
        mShapeResult.Clear();
        if (!mShaper.ShapeUtf8(text, (nk_size)-1, sp, mShapeResult).IsOk()) {
            logger.Info("[drawText] ShapeUtf8 failed\n");
            return;
        }
        logger.Info("[drawText] Shaped text: {0} lines\n", mShapeResult.lineCount);

        // Lazy-load les glyphes manquants
        NkRasterizeParams rp = NkRasterizeParams::ForBitmap();

        // Lambdas pixel → NDC
        auto toNdcX = [&](float x) { return  2.f * x / (float)mW - 1.f; };
        auto toNdcY = [&](float y) { return -2.f * y / (float)mH + 1.f; };

        int preVertCount = mVertCount;
        for (nk_uint32 l = 0u; l < mShapeResult.lineCount; ++l) {
            const NkGlyphRun& run = mShapeResult.Line(l);
            const float baseY = py + run.baselineY;
            logger.Info("[drawText] Line {0}: {1} glyphes\n", l, run.glyphCount);

            for (nk_uint32 gi = 0u; gi < run.glyphCount; ++gi) {
                const NkShapedGlyph& sg = run.Glyph(gi);
                if (sg.isWhitespace || sg.isLineBreak) continue;

                const NkCachedGlyph* glyph = nullptr;
                NkFontResult fr = mLib->GetGlyph(mFontId, sg.codepoint, ptSize, 96.f, rp, glyph);
                if (!fr.IsOk()) {
                    logger.Info("[drawText] GetGlyph failed for codepoint U+{0:04X}\n", sg.codepoint);
                    continue;
                }
                if (!glyph) {
                    logger.Info("[drawText] Glyph null for U+{0:04X}\n", sg.codepoint);
                    continue;
                }
                if (!glyph->isValid || !glyph->isInAtlas) {
                    logger.Info("[drawText] Glyph invalid/notInAtlas U+{0:04X}: valid={1} inAtlas={2}\n", 
                        sg.codepoint, glyph->isValid, glyph->isInAtlas);
                    continue;
                }

                // Position pixel
                // Utilise sg.metrics (métriques du shaper) pour la taille visible.
                // glyph->metrics = taille du bitmap raw (inclut le padding).
                // sg.metrics.width   = taille d'affichage typographique.
                // On choisit metrics.width/Height pour couvrir exactement le bitmap.
                const float bw = (glyph->metrics.width  > 0.f)
                                ? glyph->metrics.width
                                : sg.metrics.width;
                const float bh = (glyph->metrics.height > 0.f)
                                ? glyph->metrics.height
                                : sg.metrics.height;

                float x0 = px + sg.pen.x + sg.xOffset + sg.metrics.bearingX;
                float y0 = baseY - sg.metrics.bearingY;
                float x1 = x0 + bw;
                float y1 = y0 + bh;

                // NDC
                float nx0 = toNdcX(x0), ny0 = toNdcY(y0);
                float nx1 = toNdcX(x1), ny1 = toNdcY(y1);

                float u0 = glyph->uvX0, v0 = glyph->uvY0;
                float u1 = glyph->uvX1, v1 = glyph->uvY1;

                if (mVertCount == 0) { // Log seulement le premier glyph
                    logger.Info("[drawText] First glyph U+{0:04X}: pos=({1},{2})-({3},{4}) uv=({5},{6})-({7},{8})\n",
                        sg.codepoint, nx0, ny0, nx1, ny1, u0, v0, u1, v1);
                }

                if (mVertCount + 6 > MAX_VERTS) flush();

                // Tri 1 : TL TR BL
                mVerts[mVertCount++] = { nx0, ny0, u0, v0 };
                mVerts[mVertCount++] = { nx1, ny0, u1, v0 };
                mVerts[mVertCount++] = { nx0, ny1, u0, v1 };
                // Tri 2 : TR BR BL
                mVerts[mVertCount++] = { nx1, ny0, u1, v0 };
                mVerts[mVertCount++] = { nx1, ny1, u1, v1 };
                mVerts[mVertCount++] = { nx0, ny1, u0, v1 };
            }
        }

        // Flush immédiat avec la couleur donnée
        logger.Info("[drawText] Created {0} vertices (was {1}), calling flush\n", mVertCount - preVertCount, preVertCount);
        if (mVertCount > 0) {
            flushWithColor(r, g, b, a);
        }
    }

    // ── Flush vers OpenGL ────────────────────────────────────────────────────

    void flush() { flushWithColor(1.f, 1.f, 1.f, 1.f); }

    void flushWithColor(float r, float g, float b, float a) {
        if (mVertCount == 0 || mAtlasPageCount == 0) {
            logger.Info("[flushWithColor] Early exit: vertCount={0} pageCount={1}\n", mVertCount, mAtlasPageCount);
            return;
        }
        if (!mProgFont || !mVAO) {
            logger.Info("[flushWithColor] Missing program or VAO\n");
            return;
        }

        logger.Info("[flushWithColor] Drawing {0} vertices with color ({1},{2},{3},{4})\n", 
            mVertCount, r, g, b, a);

        // Pour l'instant on utilise toujours la page 0
        // (amélioration future : trier par page)
        GLuint texID = mAtlasTex[0];
        if (texID == 0u) {
            logger.Info("[flush] texID=0 ! Atlas non cree.\n");
            mVertCount = 0;
            return;
        }

        glUseProgram(mProgFont);

        // Lie la texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texID);
        glUniform1i(glGetUniformLocation(mProgFont, "uAtlas"), 0);
        glUniform4f(glGetUniformLocation(mProgFont, "uColor"), r, g, b, a);

        logger.Info("[flushWithColor] Texture bound: texID={0}, uniform uColor set\n", texID);

        // Upload vertices
        glBindVertexArray(mVAO);
        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        mVertCount * sizeof(FontVtx), mVerts);

        // Draw
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        glDrawArrays(GL_TRIANGLES, 0, mVertCount);

        logger.Info("[flushWithColor] glDrawArrays called with {0} vertices\n", mVertCount);

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);

        mVertCount = 0;
    }

    // ── Quad rouge de diagnostic ──────────────────────────────────────────────

    void drawDiagQuad() {
        if (!mProgDiag || !mDiagVAO) return;
        glUseProgram(mProgDiag);
        glBindVertexArray(mDiagVAO);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glUseProgram(0);
    }

    // ── Dump de debug : vérifie l'atlas ──────────────────────────────────────

    void debugAtlasInfo() {
        logger.Info("[Atlas] Pages: {0}\n", mAtlasPageCount);
        for (int i = 0; i < mAtlasPageCount; ++i)
            logger.Info("[Atlas] Page {0} texID={1}\n", i, mAtlasTex[i]);

        // Vérifie quelques glyphes ASCII
        const NkCachedGlyph* g = nullptr;
        NkRasterizeParams rp = NkRasterizeParams::ForBitmap();
        mLib->GetGlyph(mFontId, 'A', 18.f, 96.f, rp, g);
        if (g && g->isValid && g->isInAtlas)
            logger.Info("[Atlas] 'A' : valid={0} inAtlas={1} uv=({2},{3})-({4},{5}) bmp={6}x{7}\n",
                g->isValid, g->isInAtlas,
                g->uvX0, g->uvY0, g->uvX1, g->uvY1,
                g->metrics.width, g->metrics.height);
        else
            logger.Info("[Atlas] 'A' non trouve dans le cache !\n");
    }

    void shutdown() {
        if (mProgFont) { glDeleteProgram(mProgFont); mProgFont = 0; }
        if (mProgDiag) { glDeleteProgram(mProgDiag); mProgDiag = 0; }
        if (mVAO)      { glDeleteVertexArrays(1, &mVAO); mVAO = 0; }
        if (mVBO)      { glDeleteBuffers(1, &mVBO); mVBO = 0; }
        if (mDiagVAO)  { glDeleteVertexArrays(1, &mDiagVAO); mDiagVAO = 0; }
        if (mDiagVBO)  { glDeleteBuffers(1, &mDiagVBO); mDiagVBO = 0; }
        for (int i = 0; i < MAX_ATLAS_PAGES; ++i)
            if (mAtlasTex[i]) { glDeleteTextures(1, &mAtlasTex[i]); mAtlasTex[i] = 0; }
        delete mLib; mLib = nullptr;
    }
};

// =============================================================================
//  nkmain
// =============================================================================

int nkmain(const NkEntryState& state) {
    NK_UNUSED(state);

    // ── Fenêtre ───────────────────────────────────────────────────────────────
    NkWindowConfig wc;
    wc.title     = "NKFont GL Test — diagnostic";
    wc.width     = 1280;
    wc.height    = 720;
    wc.centered  = true;
    wc.resizable = true;

    NkWindow window;
    if (!window.Create(wc)) {
        logger.Info("[Test] Fenetre : echec\n");
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
    logger.Info("[Test] OpenGL {0} charge\n", (const char*)glGetString(GL_VERSION));

    int W = wc.width, H = wc.height;

    // ── NkFontGLRenderer ─────────────────────────────────────────────────────
    // Alloué sur le TAS : NkShapeResult = 22 Mo, NkFontLibrary = ~18 Mo
    NkFontGLRenderer* renderer = new NkFontGLRenderer();

    const char* fontPath = "Resources/Fonts/ProggyClean.ttf";
    if (!renderer->init(fontPath, W, H)) {
        logger.Info("[Test] Echec init renderer\n");
        // Continue quand meme pour voir le quad rouge
    }

    // ── Debug initial ─────────────────────────────────────────────────────────
    renderer->debugAtlasInfo();

    // ── Boucle ───────────────────────────────────────────────────────────────
    auto& events = NkEvents();
    bool running = true;
    NkChrono timer;
    double timePrev = timer.Elapsed().seconds;
    double fpsAccum = 0.0;
    int    fpsFrames = 0;
    float  fpsDisplay = 0.f;

    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) {
        running = false;
    });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        if (e->GetKey() == NkKey::NK_ESCAPE) running = false;
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        W = e->GetWidth();
        H = e->GetHeight();
        renderer->resize(W, H);
        glViewport(0, 0, W, H);
    });

    logger.Info("[Test] Boucle principale...\n");

    while (running) {
        events.PollEvents();
        if (!running) break;

        double now = timer.Elapsed().seconds;
        float  dt  = (float)(now - timePrev);
        timePrev = now;
        fpsAccum += dt; ++fpsFrames;
        if (fpsAccum >= 0.5) {
            fpsDisplay = fpsFrames / (float)fpsAccum;
            fpsAccum = 0.0; fpsFrames = 0;
        }
        
        if (ctx->BeginFrame()) {
            // ── Upload atlas si dirty ─────────────────────────────────────────────
            if (renderer->mLib)
                renderer->mLib->UploadAtlas();

            // ── Clear ─────────────────────────────────────────────────────────────
            glViewport(0, 0, W, H);
            glClearColor(0.08f, 0.08f, 0.12f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);

            // ── DIAGNOSTIC 1 : quad rouge (test OpenGL pur) ───────────────────────
            renderer->drawDiagQuad();

            // ── DIAGNOSTIC 2 : texte NKFont ───────────────────────────────────────
            // Si ce bloc s'affiche, NKFont + OpenGL fonctionnent.
            // Si seul le quad rouge s'affiche, NKFont a un problème.

            {
                char buf[64];
                snprintf(buf, sizeof(buf), "FPS: %.0f", fpsDisplay);
                renderer->drawText(buf, 10.f, 10.f, 18.f, 1.f, 1.f, 0.2f);
            }

            renderer->drawText("Hello NKFont!",
                            10.f, 35.f, 18.f,
                            1.f, 1.f, 1.f);

            renderer->drawText("ABCDEFGHIJKLMNOPQRSTUVWXYZ",
                            10.f, 60.f, 14.f,
                            0.3f, 1.f, 0.3f);

            renderer->drawText("abcdefghijklmnopqrstuvwxyz 0123456789",
                            10.f, 80.f, 14.f,
                            0.7f, 0.7f, 1.f);

            renderer->drawText("NKFont from-scratch — sans FreeType",
                            10.f, 100.f, 14.f,
                            1.f, 0.7f, 0.3f);

            // Status
            {
                char buf[128];
                snprintf(buf, sizeof(buf),
                        "Atlas: %d pages",
                        renderer->mAtlasPageCount);
                renderer->drawText(buf, 10.f, 125.f, 12.f, 0.5f, 1.f, 0.5f);
            }

            renderer->drawText("Quad rouge = OpenGL OK  |  Texte = NKFont OK",
                            10.f, (float)H - 30.f, 13.f,
                            0.8f, 0.8f, 0.8f);

            // ── Swap ──────────────────────────────────────────────────────────────

            ctx->EndFrame();
            ctx->Present();
        }
    }

    renderer->shutdown();
    delete renderer;
    ctx->Shutdown();
    window.Close();

    logger.Info("[Test] Termine.\n");
    return 0;
}