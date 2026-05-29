// =============================================================================
// GLRenderer2D.cpp
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

// GLAD2 (mêmes inclusions que GLContext.cpp)
#if defined(__has_include)
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       if __has_include(<glad/wgl.h>) && __has_include(<glad/gl.h>)
#           define PONG_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#       if __has_include(<glad/gl.h>)
#           define PONG_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#       if __has_include(<glad/gles2.h>)
#           define PONG_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#       if __has_include(<glad/gles2.h>)
#           define PONG_HAS_GLAD 1
#       endif
#   endif
#endif

#if defined(PONG_HAS_GLAD)
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       include <glad/wgl.h>
#       include <glad/gl.h>
#   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#       if defined(__has_include)
#           if __has_include(<glad/glx.h>)
#               include <glad/glx.h>
#           endif
#       endif
#       include <glad/gl.h>
#   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#       include <glad/gles2.h>
#   elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#       include <glad/gles2.h>
#   endif
#endif

#if defined(Bool)
#   undef Bool
#endif

#include "GLRenderer2D.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkFunctions.h"
#include <cstring>
#include <cmath>

namespace nkentseu { namespace songoo {

// ── Shaders portables (GLSL 3.30 core / 3.00 ES) ────────────────────────────
// On utilise une syntaxe qui marche sur les deux profils via define au début.
static const char* kVertexShader =
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    "#version 300 es\n"
    "precision highp float;\n"
#else
    "#version 330 core\n"
#endif
    "layout(location=0) in vec2 aPos;\n"
    "layout(location=1) in vec2 aUV;\n"
    "layout(location=2) in vec4 aColor;\n"
    "layout(location=3) in float aUseTex;\n"
    "uniform mat4 uProj;\n"
    "out vec2 vUV;\n"
    "out vec4 vColor;\n"
    "out float vUseTex;\n"
    "void main() {\n"
    "    vUV = aUV;\n"
    "    vColor = aColor;\n"
    "    vUseTex = aUseTex;\n"
    "    gl_Position = uProj * vec4(aPos, 0.0, 1.0);\n"
    "}\n";

static const char* kFragmentShader =
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    "#version 300 es\n"
    "precision mediump float;\n"
#else
    "#version 330 core\n"
#endif
    "in vec2 vUV;\n"
    "in vec4 vColor;\n"
    "in float vUseTex;\n"
    "uniform sampler2D uTex;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    if (vUseTex < 0.5) {\n"
    "        // Color only (primitive solide)\n"
    "        fragColor = vColor;\n"
    "    } else if (vUseTex < 1.5) {\n"
    "        // Alpha mask (atlas font GL_R8) modulee par couleur\n"
    "        float alpha = texture(uTex, vUV).r;\n"
    "        fragColor = vec4(vColor.rgb, vColor.a * alpha);\n"
    "    } else {\n"
    "        // RGBA texture (logo/sprite) modulee par couleur vertex\n"
    "        vec4 tex = texture(uTex, vUV);\n"
    "        fragColor = tex * vColor;\n"
    "    }\n"
    "}\n";

uint32 GLRenderer2D::CompileShader(const char* vsSrc, const char* fsSrc) {
    auto compile = [](GLenum type, const char* src) -> GLuint {
        GLuint sh = glCreateShader(type);
        glShaderSource(sh, 1, &src, nullptr);
        glCompileShader(sh);
        GLint ok = 0;
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024]; GLsizei len = 0;
            glGetShaderInfoLog(sh, sizeof(log), &len, log);
            logger.Error("[GLRenderer2D] shader compile error: %s", log);
            glDeleteShader(sh);
            return 0;
        }
        return sh;
    };

    GLuint vs = compile(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) { if (vs) glDeleteShader(vs); if (fs) glDeleteShader(fs); return 0; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]; GLsizei len = 0;
        glGetProgramInfoLog(prog, sizeof(log), &len, log);
        logger.Error("[GLRenderer2D] program link error: %s", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

bool GLRenderer2D::Init() {
    mProgram = CompileShader(kVertexShader, kFragmentShader);
    if (!mProgram) return false;

    mLocProj = glGetUniformLocation(mProgram, "uProj");
    mLocTex  = glGetUniformLocation(mProgram, "uTex");

    // VAO/VBO/EBO
    glGenVertexArrays(1, &mVAO);
    glBindVertexArray(mVAO);

    glGenBuffers(1, &mVBO);
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferData(GL_ARRAY_BUFFER, kMaxQuads * 4 * sizeof(GLVertex), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &mEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
    // Indices statiques : (4*N) quads => 6*N indices
    uint16* idx = new uint16[kMaxQuads * 6];
    for (int i = 0; i < kMaxQuads; ++i) {
        uint16 b = i * 4;
        idx[i*6 + 0] = b + 0;
        idx[i*6 + 1] = b + 1;
        idx[i*6 + 2] = b + 2;
        idx[i*6 + 3] = b + 0;
        idx[i*6 + 4] = b + 2;
        idx[i*6 + 5] = b + 3;
    }
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, kMaxQuads * 6 * sizeof(uint16), idx, GL_STATIC_DRAW);
    delete[] idx;

    // Attribs : pos(2), uv(2), color(4), useTex(1)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLVertex), (void*)offsetof(GLVertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLVertex), (void*)offsetof(GLVertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GLVertex), (void*)offsetof(GLVertex, r));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(GLVertex), (void*)offsetof(GLVertex, useTex));

    glBindVertexArray(0);

    // Texture blanche 1x1 pour les draws non texturés (le shader sample toujours)
    glGenTextures(1, &mWhiteTex);
    glBindTexture(GL_TEXTURE_2D, mWhiteTex);
    uint8 whitePixel[4] = { 255, 255, 255, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    mVertices = new GLVertex[kMaxQuads * 4];
    mQuadCount = 0;
    mCurrentTex = 0;

    return true;
}

void GLRenderer2D::Shutdown() {
    if (mWhiteTex) { glDeleteTextures(1, &mWhiteTex); mWhiteTex = 0; }
    if (mEBO)      { glDeleteBuffers(1, &mEBO);       mEBO = 0; }
    if (mVBO)      { glDeleteBuffers(1, &mVBO);       mVBO = 0; }
    if (mVAO)      { glDeleteVertexArrays(1, &mVAO);  mVAO = 0; }
    if (mProgram)  { glDeleteProgram(mProgram);       mProgram = 0; }
    delete[] mVertices; mVertices = nullptr;
}

void GLRenderer2D::Begin(int W, int H) {
    mViewportW = W; mViewportH = H;
    glViewport(0, 0, W, H);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    // Ortho top-left (0,0)..(W,H), Y down comme l'ecran
    float L = 0.0f, R = (float)W, T = 0.0f, B = (float)H;
    float* P = mProj;
    P[0] = 2.0f / (R - L); P[1] = 0;              P[2] = 0;  P[3]  = 0;
    P[4] = 0;              P[5] = 2.0f / (T - B); P[6] = 0;  P[7]  = 0;
    P[8] = 0;              P[9] = 0;              P[10]= -1; P[11] = 0;
    P[12]= -(R+L)/(R-L);   P[13]= -(T+B)/(T-B);   P[14]= 0;  P[15] = 1;

    glUseProgram(mProgram);
    glUniformMatrix4fv(mLocProj, 1, GL_FALSE, mProj);
    glUniform1i(mLocTex, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mWhiteTex);
    mCurrentTex = mWhiteTex;

    mQuadCount = 0;
}

void GLRenderer2D::End() { Flush(); }
void GLRenderer2D::Flush() { FlushBatch(); }

void GLRenderer2D::FlushBatch() {
    if (mQuadCount == 0) return;
    glBindVertexArray(mVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, mQuadCount * 4 * sizeof(GLVertex), mVertices);
    glDrawElements(GL_TRIANGLES, mQuadCount * 6, GL_UNSIGNED_SHORT, nullptr);
    glBindVertexArray(0);
    mQuadCount = 0;
}

void GLRenderer2D::EnsureRoom(int n) {
    if (mQuadCount + n > kMaxQuads) FlushBatch();
}

void GLRenderer2D::BindTexture(uint32 textureId) {
    if (textureId == 0) textureId = mWhiteTex;
    if (textureId == mCurrentTex) return;
    FlushBatch();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    mCurrentTex = textureId;
}

void GLRenderer2D::SetClip(int x, int y, int w, int h) {
    FlushBatch();
    glEnable(GL_SCISSOR_TEST);
    // GL scissor : Y bottom-up. Convertir top-left → bottom-left.
    int glY = mViewportH - (y + h);
    glScissor(x, glY, w, h);
    mScissorEnabled = true;
}
void GLRenderer2D::ClearClip() {
    FlushBatch();
    glDisable(GL_SCISSOR_TEST);
    mScissorEnabled = false;
}
void GLRenderer2D::Clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void GLRenderer2D::PushQuad(float x, float y, float w, float h,
                            float u0, float v0, float u1, float v1,
                            math::NkColor c, float useTex)
{
    EnsureRoom(1);
    GLVertex* q = mVertices + mQuadCount * 4;
    float r = c.r / 255.0f, gg = c.g / 255.0f, bb = c.b / 255.0f, aa = c.a / 255.0f;
    q[0] = { x,     y,     u0, v0, r, gg, bb, aa, useTex };
    q[1] = { x + w, y,     u1, v0, r, gg, bb, aa, useTex };
    q[2] = { x + w, y + h, u1, v1, r, gg, bb, aa, useTex };
    q[3] = { x,     y + h, u0, v1, r, gg, bb, aa, useTex };
    ++mQuadCount;
}

void GLRenderer2D::DrawQuad(float x, float y, float w, float h, math::NkColor c) {
    PushQuad(x, y, w, h, 0, 0, 1, 1, c, 0.0f);
}

void GLRenderer2D::DrawQuadOutline(float x, float y, float w, float h, math::NkColor c, float t) {
    DrawQuad(x,         y,         w, t,         c); // top
    DrawQuad(x,         y + h - t, w, t,         c); // bottom
    DrawQuad(x,         y + t,     t, h - 2 * t, c); // left
    DrawQuad(x + w - t, y + t,     t, h - 2 * t, c); // right
}

void GLRenderer2D::DrawLine(float x1, float y1, float x2, float y2, math::NkColor c, float thickness) {
    // Trace une ligne en construisant un quad oriente entre les 2 points.
    float dx = x2 - x1, dy = y2 - y1;
    float len = math::NkSqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;
    float nx = -dy / len, ny = dx / len;
    float t = thickness * 0.5f;
    float ax = x1 + nx * t, ay = y1 + ny * t;
    float bx = x2 + nx * t, by = y2 + ny * t;
    float cx = x2 - nx * t, cy = y2 - ny * t;
    float dx2 = x1 - nx * t, dy2 = y1 - ny * t;
    EnsureRoom(1);
    GLVertex* q = mVertices + mQuadCount * 4;
    float r = c.r / 255.0f, gg = c.g / 255.0f, bb = c.b / 255.0f, aa = c.a / 255.0f;
    q[0] = { ax, ay, 0, 0, r, gg, bb, aa, 0.0f };
    q[1] = { bx, by, 1, 0, r, gg, bb, aa, 0.0f };
    q[2] = { cx, cy, 1, 1, r, gg, bb, aa, 0.0f };
    q[3] = { dx2, dy2, 0, 1, r, gg, bb, aa, 0.0f };
    ++mQuadCount;
}

void GLRenderer2D::DrawCircle(float cx, float cy, float radius, math::NkColor c, int segments) {
    // Triangle fan via paire de triangles : on emet (segments) triangles, mais on
    // doit les emettre en tant que quads degeneres. Plus simple : on emet
    // (segments) triangles via FlushBatch + glDrawArrays. Mais notre pipeline est
    // base sur quads ; alternative simple : decouper en quads degeneres.
    // Implementation : approximer par un poly via N quads degeneres (2 triangles dont 1 nul)
    // OK pour visuel ; pour la performance on Flush par batch.
    // Plus simple : utiliser DrawTriangle (qui pousse un quad degenere).
    if (segments < 3) segments = 3;
    float r = c.r / 255.0f, gg = c.g / 255.0f, bb = c.b / 255.0f, aa = c.a / 255.0f;
    for (int i = 0; i < segments; ++i) {
        float a0 = (float)i / segments * 6.28318530f;
        float a1 = (float)(i + 1) / segments * 6.28318530f;
        float x0 = cx + math::NkCos(a0) * radius;
        float y0 = cy + math::NkSin(a0) * radius;
        float x1 = cx + math::NkCos(a1) * radius;
        float y1 = cy + math::NkSin(a1) * radius;
        // Tri (cx,cy) (x0,y0) (x1,y1) : emis comme quad degenere v0=v3
        EnsureRoom(1);
        GLVertex* q = mVertices + mQuadCount * 4;
        q[0] = { cx, cy, 0, 0, r, gg, bb, aa, 0.0f };
        q[1] = { x0, y0, 0, 0, r, gg, bb, aa, 0.0f };
        q[2] = { x1, y1, 0, 0, r, gg, bb, aa, 0.0f };
        q[3] = { cx, cy, 0, 0, r, gg, bb, aa, 0.0f };  // doublon pour fermer le quad
        ++mQuadCount;
    }
}

void GLRenderer2D::DrawCircleOutline(float cx, float cy, float radius, math::NkColor c, float thickness, int segments) {
    if (segments < 3) segments = 3;
    for (int i = 0; i < segments; ++i) {
        float a0 = (float)i / segments * 6.28318530f;
        float a1 = (float)(i + 1) / segments * 6.28318530f;
        float x0 = cx + math::NkCos(a0) * radius;
        float y0 = cy + math::NkSin(a0) * radius;
        float x1 = cx + math::NkCos(a1) * radius;
        float y1 = cy + math::NkSin(a1) * radius;
        DrawLine(x0, y0, x1, y1, c, thickness);
    }
}

void GLRenderer2D::DrawTriangle(float x0, float y0, float x1, float y1, float x2, float y2, math::NkColor c) {
    EnsureRoom(1);
    GLVertex* q = mVertices + mQuadCount * 4;
    float r = c.r / 255.0f, gg = c.g / 255.0f, bb = c.b / 255.0f, aa = c.a / 255.0f;
    q[0] = { x0, y0, 0, 0, r, gg, bb, aa, 0.0f };
    q[1] = { x1, y1, 0, 0, r, gg, bb, aa, 0.0f };
    q[2] = { x2, y2, 0, 0, r, gg, bb, aa, 0.0f };
    q[3] = { x0, y0, 0, 0, r, gg, bb, aa, 0.0f };
    ++mQuadCount;
}

void GLRenderer2D::DrawTexturedQuad(float x, float y, float w, float h,
                                    float u0, float v0, float u1, float v1,
                                    math::NkColor c) {
    // useTex = 1.0 -> alpha mask (font atlas GL_R8)
    PushQuad(x, y, w, h, u0, v0, u1, v1, c, 1.0f);
}

void GLRenderer2D::DrawTexturedQuadRGBA(float x, float y, float w, float h,
                                        float u0, float v0, float u1, float v1,
                                        math::NkColor c) {
    // useTex = 2.0 -> texture RGBA modulee
    PushQuad(x, y, w, h, u0, v0, u1, v1, c, 2.0f);
}

}} // namespace nkentseu::pong
