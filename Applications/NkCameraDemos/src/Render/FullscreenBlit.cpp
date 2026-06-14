// =============================================================================
// FullscreenBlit.cpp
// -----------------------------------------------------------------------------
// Implementation : programme GL minimaliste (1 attribut pos, 1 attribut uv),
// uniform uRect en NDC, triangle strip 4 vertices. Le V est deja inverse
// directement dans le VBO (vec uv = (u, 1 - row)) pour que le row 0 du
// upload glTexSubImage2D apparaisse en haut a l'ecran.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

// ── GLAD2 : avant tout autre include qui pourrait tirer gl.h ─────────────────
#if defined(__has_include)
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       if __has_include(<glad/wgl.h>) && __has_include(<glad/gl.h>)
#           define NKCAMDEM_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#       if __has_include(<glad/gl.h>)
#           define NKCAMDEM_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#       if __has_include(<glad/gles2.h>)
#           define NKCAMDEM_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#       if __has_include(<glad/gles2.h>)
#           define NKCAMDEM_HAS_GLAD 1
#       endif
#   endif
#endif

#if defined(NKCAMDEM_HAS_GLAD)
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

#include "FullscreenBlit.h"
#include "NKLogger/NkLog.h"

namespace nkentseu
{
    namespace cameradem
    {

#if defined(NKCAMDEM_HAS_GLAD)

        // ─────────────────────────────────────────────────────────────────────
        // Versionnement shader : GLES 3.0 pour Wayland/Android/Web, sinon GLSL
        // 330 core desktop. Aucun feature avance => les deux versions sont
        // quasiment identiques.
        // ─────────────────────────────────────────────────────────────────────
#   if defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN) || defined(NKENTSEU_WINDOWING_WAYLAND)
        static const char* kVS = R"(#version 300 es
precision mediump float;
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
uniform vec4 uRect;       // (x, y, w, h) en NDC
out vec2 vUV;
void main() {
    vec2 p = uRect.xy + aPos * uRect.zw;
    gl_Position = vec4(p, 0.0, 1.0);
    vUV = aUV;
}
)";

        static const char* kFS = R"(#version 300 es
precision mediump float;
in vec2 vUV;
uniform sampler2D uTex;
out vec4 fragColor;
void main() {
    fragColor = texture(uTex, vUV);
}
)";
#   else
        static const char* kVS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
uniform vec4 uRect;       // (x, y, w, h) en NDC
out vec2 vUV;
void main() {
    vec2 p = uRect.xy + aPos * uRect.zw;
    gl_Position = vec4(p, 0.0, 1.0);
    vUV = aUV;
}
)";

        static const char* kFS = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
out vec4 fragColor;
void main() {
    fragColor = texture(uTex, vUV);
}
)";
#   endif

        // ─────────────────────────────────────────────────────────────────────
        // Helper : compilation + log de shader (renvoie 0 si echec).
        // ─────────────────────────────────────────────────────────────────────
        static uint32 CompileShader(uint32 type, const char* src, const char* tag)
        {
            uint32 sh = glCreateShader(type);
            glShaderSource(sh, 1, &src, nullptr);
            glCompileShader(sh);
            GLint ok = 0;
            glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
            if (ok == GL_FALSE)
            {
                char buf[1024] = {};
                GLsizei len = 0;
                glGetShaderInfoLog(sh, (GLsizei)sizeof(buf) - 1, &len, buf);
                logger.Errorf("[FullscreenBlit] Compile %s : %s", tag, buf);
                glDeleteShader(sh);
                return 0;
            }
            return sh;
        }

        // ─────────────────────────────────────────────────────────────────────
        // Helper : link + log (renvoie 0 si echec).
        // ─────────────────────────────────────────────────────────────────────
        static uint32 LinkProgram(uint32 vs, uint32 fs)
        {
            uint32 p = glCreateProgram();
            glAttachShader(p, vs);
            glAttachShader(p, fs);
            glLinkProgram(p);
            GLint ok = 0;
            glGetProgramiv(p, GL_LINK_STATUS, &ok);
            if (ok == GL_FALSE)
            {
                char buf[1024] = {};
                GLsizei len = 0;
                glGetProgramInfoLog(p, (GLsizei)sizeof(buf) - 1, &len, buf);
                logger.Errorf("[FullscreenBlit] Link : %s", buf);
                glDeleteProgram(p);
                return 0;
            }
            return p;
        }
#endif // NKCAMDEM_HAS_GLAD

        FullscreenBlit::~FullscreenBlit()
        {
            Shutdown();
        }

        // ─────────────────────────────────────────────────────────────────────
        bool FullscreenBlit::Initialize()
        {
#if defined(NKCAMDEM_HAS_GLAD)
            uint32 vs = CompileShader(GL_VERTEX_SHADER,   kVS, "VS");
            if (vs == 0) return false;
            uint32 fs = CompileShader(GL_FRAGMENT_SHADER, kFS, "FS");
            if (fs == 0) { glDeleteShader(vs); return false; }

            mProgram = LinkProgram(vs, fs);
            glDeleteShader(vs);
            glDeleteShader(fs);
            if (mProgram == 0) return false;

            mLocRect = glGetUniformLocation(mProgram, "uRect");
            mLocTex  = glGetUniformLocation(mProgram, "uTex");

            // Quad unitaire en triangle strip. Position (0..1) sera multipliee
            // par uRect.zw puis decalee de uRect.xy dans le VS pour donner la
            // position NDC finale. UV inverse en V : (0,0) en bas-gauche -> v=1
            // ce qui place la PREMIERE ligne uploadee (top de la frame camera)
            // en haut de l'ecran.
            const float verts[] = {
                // x,    y,    u,   v
                0.0f, 0.0f,  0.0f, 1.0f,   // bottom-left  -> sample top-left
                1.0f, 0.0f,  1.0f, 1.0f,   // bottom-right -> sample top-right
                0.0f, 1.0f,  0.0f, 0.0f,   // top-left     -> sample bottom-left
                1.0f, 1.0f,  1.0f, 0.0f,   // top-right    -> sample bottom-right
            };

            glGenVertexArrays(1, &mVAO);
            glGenBuffers(1, &mVBO);
            glBindVertexArray(mVAO);
            glBindBuffer(GL_ARRAY_BUFFER, mVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                                  4 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                                  4 * sizeof(float),
                                  (void*)(2 * sizeof(float)));

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
            return true;
#else
            logger.Warn("[FullscreenBlit] glad indisponible : pipeline absent");
            return false;
#endif
        }

        // ─────────────────────────────────────────────────────────────────────
        void FullscreenBlit::Shutdown()
        {
#if defined(NKCAMDEM_HAS_GLAD)
            if (mVBO     != 0) { glDeleteBuffers(1,      &mVBO); mVBO = 0; }
            if (mVAO     != 0) { glDeleteVertexArrays(1, &mVAO); mVAO = 0; }
            if (mProgram != 0) { glDeleteProgram(mProgram);      mProgram = 0; }
#endif
            mLocRect = -1;
            mLocTex  = -1;
        }

        // ─────────────────────────────────────────────────────────────────────
        void FullscreenBlit::Clear(float r, float g, float b)
        {
#if defined(NKCAMDEM_HAS_GLAD)
            glClearColor(r, g, b, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);
#else
            (void)r; (void)g; (void)b;
#endif
        }

        // ─────────────────────────────────────────────────────────────────────
        void FullscreenBlit::Draw(uint32 texId,
                                  float x, float y, float w, float h)
        {
#if defined(NKCAMDEM_HAS_GLAD)
            if (mProgram == 0 || mVAO == 0 || texId == 0) return;

            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glDisable(GL_BLEND);

            glUseProgram(mProgram);
            if (mLocRect >= 0) glUniform4f(mLocRect, x, y, w, h);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texId);
            if (mLocTex >= 0) glUniform1i(mLocTex, 0);

            glBindVertexArray(mVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);

            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0);
#else
            (void)texId; (void)x; (void)y; (void)w; (void)h;
#endif
        }

    } // namespace cameradem
} // namespace nkentseu
