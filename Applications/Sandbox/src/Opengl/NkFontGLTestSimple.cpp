/**
 * @file    NkFontGLTestSimple.cpp
 * @brief   Version simplifiée du test de rendu de texte OpenGL
 * 
 * Versions simplifiée qui bypassse NkTextShaper et teste directement:
 * 1. Rastérisation de glyphes
 * 2. Rendu via OpenGL
 * 
 * Basé sur l'approche d'ImGui adaptée pour NKFont
 */

#include <stdio.h>
#include <string.h>
#include "NKCore/NkCommon.h"
#include "NKLogger/NkLogger.h"
#include "NKWindow/NkWindow.h"
#include "NKRenderer/NkGraphicsContext.h"
#include "NKWindow/NkNativeContext.h"
#include "NKFont/Core/NkFontLibrary.h"
#include "NKFont/Core/NkUnicode.h"
#include "NKMath/NkMath.h"
#include "glad/gl.h"
#include "NkFontSimpleAdapter.h"

using namespace nkentseu;

static NkLoggerFactory::LoggerPtr logger = NkLoggerFactory::GetLogger("default");

// ============================================================================
//  Simple Vertex Data
// ============================================================================

struct SimpleVtx {
    float x, y;          // Position NDC
    float u, v;          // UVs atlas
    float r, g, b, a;    // Couleur RGBA
};

// ============================================================================
//  Simple Shader Programs
// ============================================================================

static const char* kVSSimple = R"GLSL(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
out VS_OUT {
    vec2 uv;
    vec4 color;
} vs_out;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vs_out.uv = aUV;
    vs_out.color = aColor;
}
)GLSL";

static const char* kFSSimple = R"GLSL(
#version 330 core
in VS_OUT {
    vec2 uv;
    vec4 color;
} fs_in;
out vec4 fragColor;
uniform sampler2D uAltas;
void main() {
    // Sample atlas rouge channel = alpha
    float alpha = texture(uAltas, fs_in.uv).r;
    if (alpha < 0.01) discard;
    fragColor = vec4(fs_in.color.rgb, fs_in.color.a * alpha);
}
)GLSL";

// ============================================================================
//  Main Test
// ============================================================================

int main() {
    logger->Info("[TestSimple] Démarrage...");
    
    // Fenêtre
    NkWindowConfig wc;
    wc.title = "NKFont Simple GL Test";
    wc.width = 1024;
    wc.height = 600;
    wc.centered = true;
    
    NkWindow window;
    if (!window.Create(wc)) {
        logger->Error("[TestSimple] Fenêtre échouée");
        return 1;
    }
    
    // Contexte GL
    NkContextDesc desc;
    desc.api = NkGraphicsApi::NK_API_OPENGL;
    desc.window = &window;
    
    auto ctx = NkContextFactory::Create(desc);
    if (!ctx || !ctx->IsReady()) {
        logger->Error("[TestSimple] Contexte OpenGL échoué");
        return 1;
    }
    
    // Load GLAD
    auto loader = NkNativeContext::GetOpenGLProcAddressLoader(ctx.get());
    if (!loader || !gladLoadGL((GLADloadfunc)loader)) {
        logger->Error("[TestSimple] GLAD failed");
        return 1;
    }
    
    logger->Info("[TestSimple] OpenGL {}", (const char*)glGetString(GL_VERSION));
    
    // ── Init NKFont ───────────────────────────────────────────────────────
    
    NkFontLibrary fontLib;
    NkFontLibraryDesc libDesc;
    libDesc.atlasPageSize = 512u;
    libDesc.atlasChannels = 1u;
    libDesc.atlasPadding = 2u;
    
    if (!fontLib.Init(libDesc)) {
        logger->Error("[TestSimple] Font lib init failed");
        return 1;
    }
    
    logger->Info("[TestSimple] Font lib initialized");
    
    // Charge la fonte
    nk_uint32 fontId = 0u;
    const char* fontPath = "Resources/Fonts/ProggyCleanTT.ttf";
    NkFontResult res = fontLib.LoadFromFile(fontPath, NK_LOAD_OUTLINES, fontId);
    
    if (!res.IsOk()) {
        logger->Error("[TestSimple] Font load failed: {} ({})", fontPath, res.ToString());
        return 1;
    }
    
    const NkFontInfo* info = fontLib.GetFontInfo(fontId);
    if (info) {
        logger->Info("[TestSimple] Font '{} {}' loaded with {} glyphs", 
                    info->familyName, info->styleName, info->numGlyphs);
    }
    
    // Pré-rastérise
    NkRasterizeParams rp = NkRasterizeParams::ForBitmap(2u);
    fontLib.PrerasterizeRange(fontId, NkUnicodeRange::Ascii(), 24.f, 96.f, rp);
    fontLib.UploadAtlas();
    
    logger->Info("[TestSimple] Atlas: {} pages, {} glyphes", 
                fontLib.Atlas().PageCount(), 
                fontLib.Atlas().GlyphCount());
    
    // ── Create GL Texture ──────────────────────────────────────────────────
    
    GLuint atlasTex = 0;
    {
        glGenTextures(1, &atlasTex);
        glBindTexture(GL_TEXTURE_2D, atlasTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        // Atlas page 0
        auto& page = fontLib.Atlas().Page(0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 
                    page.width(), page.height(), 0, 
                    GL_RED, GL_UNSIGNED_BYTE, page.data());
        
        glBindTexture(GL_TEXTURE_2D, 0);
        logger->Info("[TestSimple] GL Texture {} created {}x{}", 
                    atlasTex, page.width(), page.height());
    }
    
    // ── Compile Shaders ───────────────────────────────────────────────────
    
    auto compileShader = [](GLenum type, const char* src) -> GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char buf[512];
            glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
            printf("[GLShader] Error: %s\n", buf);
            glDeleteShader(s);
            return 0;
        }
        return s;
    };
    
    GLuint vs = compileShader(GL_VERTEX_SHADER, kVSSimple);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFSSimple);
    
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetProgramInfoLog(prog, sizeof(buf), nullptr, buf);
        printf("[GLProgram] Link error: %s\n", buf);
    }
    
    logger->Info("[TestSimple] Shader compiled and linked");
    
    // ── Setup VAO/VBO ─────────────────────────────────────────────────────
    
    GLuint vao = 0, vbo = 0;
    {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, 65536 * sizeof(SimpleVtx), nullptr, GL_DYNAMIC_DRAW);
        
        // pos @ 0
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(SimpleVtx), (void*)offsetof(SimpleVtx, x));
        
        // uv @ 1
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SimpleVtx), (void*)offsetof(SimpleVtx, u));
        
        // color @ 2
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(SimpleVtx), (void*)offsetof(SimpleVtx, r));
        
        glBindVertexArray(0);
        logger->Info("[TestSimple] VAO/VBO setup complete");
    }
    
    // ── Boucle principale ──────────────────────────────────────────────────
    
    bool running = true;
    int frameCount = 0;
    
    while (running && frameCount < 300) {  // Run 5 seconds at 60 FPS
        frameCount++;
        
        glClearColor(0.15f, 0.15f, 0.20f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // ── Build vertex list avec texte simple ────────────────────────────
        
        std::vector<SimpleVtx> vertices;
        float px = -0.95f;  // NDC left
        float py = 0.80f;   // NDC top
        float scale = 1.0f / 512.0f;  // Convert pixel to NDC
        
        const char* text = "Hello World!";
        for (const char* p = text; *p; ++p) {
            nk_codepoint cp = (nk_codepoint)(unsigned char)*p;
            
            // Demande le glyphe
            const NkCachedGlyph* gl = nullptr;
            NkRasterizeParams rp = NkRasterizeParams::ForBitmap(2u);
            NkFontResult res = fontLib.GetGlyph(fontId, cp, 24.f, 96.f, rp, gl);
            
            if (!res.IsOk() || !gl || !gl->isValid || !gl->isInAtlas) {
                logger->Warn("[Glyph] Failed for codepoint {} ({})", cp, res.ToString());
                continue;
            }
            
            logger->Debug("[Glyph] Found cp={} uv=({:.3f},{:.3f})-({:.3f},{:.3f}) bearing=({},{}) dims={}x{}", 
                        cp, gl->uvX0, gl->uvY0, gl->uvX1, gl->uvY1, 
                        gl->bearingX, gl->bearingY, gl->width, gl->height);
            
            // Crée le quad
            float x0 = px + gl->bearingX * scale;
            float y0 = py - gl->bearingY * scale;
            float x1 = x0 + gl->width * scale;
            float y1 = y0 - gl->height * scale;
            
            // Triangle 1
            vertices.push_back({x0, y0, gl->uvX0, gl->uvY0, 1,1,1,1});
            vertices.push_back({x1, y0, gl->uvX1, gl->uvY0, 1,1,1,1});
            vertices.push_back({x0, y1, gl->uvX0, gl->uvY1, 1,1,1,1});
            
            // Triangle 2
            vertices.push_back({x1, y0, gl->uvX1, gl->uvY0, 1,1,1,1});
            vertices.push_back({x1, y1, gl->uvX1, gl->uvY1, 1,1,1,1});
            vertices.push_back({x0, y1, gl->uvX0, gl->uvY1, 1,1,1,1});
            
            px += gl->advanceX * scale;
        }
        
        logger->Info("[Frame] Built {} vertices from '{}' ({} chars)", 
                    vertices.size(), text, strlen(text));
        
        // ── Draw ───────────────────────────────────────────────────────────
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        glUseProgram(prog);
        glUniform1i(glGetUniformLocation(prog, "uAltas"), 0);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, atlasTex);
        
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(SimpleVtx), vertices.data());
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
        glBindVertexArray(0);
        
        glDisable(GL_BLEND);
        glUseProgram(0);
        
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            logger->Warn("[GL] Error 0x{:X}", (unsigned)err);
        }
        
        ctx->Present();
        window.ProcessEvents();
        
        if (frameCount == 1) {
            logger->Info("[Frame 1] First render complete");
        }
    }
    
    // ── Cleanup ────────────────────────────────────────────────────────────
    
    glDeleteProgram(prog);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(1, &atlasTex);
    
    fontLib.Shutdown();
    
    logger->Info("[TestSimple] Done - {} frames rendered", frameCount);
    return 0;
}
