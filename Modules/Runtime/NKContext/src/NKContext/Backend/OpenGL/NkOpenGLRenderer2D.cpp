// =============================================================================
// NkOpenGLRenderer2D.cpp — OpenGL 4.3 / GLES 3.0 implementation
// =============================================================================
#include "NkOpenGLRenderer2D.h"
#include "NKContext/Renderer/Resources/NkTexture.h"
#include "NKContext/Renderer/Core/NkRenderer2DTypes.h"
#include "NKContext/Core/NkIGraphicsContext.h"
#include "NKContext/Core/NkNativeContextAccess.h"
#include "NKLogger/NkLog.h"

#include <cstring>

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    // Windows : glad headers via NKGlad
    // Ajuster le chemin selon où NKGlad expose ses headers dans les includedirs
#   if __has_include("glad/wgl.h")
#       include "glad/wgl.h"
#       include "glad/gl.h"
#   elif __has_include("NKGlad/glad/wgl.h")
#       include "NKGlad/glad/wgl.h"
#       include "NKGlad/glad/gl.h"
#   else
        // Fallback : charger glext.h qui déclare les types et macros GL 3.3+
        // mais pas les pointeurs de fonctions — utiliser les fonctions via
        // le mécanisme de résolution dynamique du loader existant.
#       include <GL/gl.h>
#       include <GL/glext.h>
        // Déclarer les pointeurs de fonctions manuellement pour GL 3.3+
        // (ils sont résolus par NkOpenGLContext::LoadOpenGLEntryPoints via gladLoadGL)
        typedef GLuint (APIENTRY *PFNGLCREATESHADERPROC)(GLenum);
        typedef void   (APIENTRY *PFNGLSHADERSOURCEPROC)(GLuint,GLsizei,const GLchar* const*,const GLint*);
        typedef void   (APIENTRY *PFNGLCOMPILESHADERPROC)(GLuint);
        typedef void   (APIENTRY *PFNGLGETSHADERIVPROC)(GLuint,GLenum,GLint*);
        typedef void   (APIENTRY *PFNGLGETSHADERINFOLOGPROC)(GLuint,GLsizei,GLsizei*,GLchar*);
        typedef void   (APIENTRY *PFNGLDELETESHADERPROC)(GLuint);
        typedef GLuint (APIENTRY *PFNGLCREATEPROGRAMPROC)(void);
        typedef void   (APIENTRY *PFNGLATTACHSHADERPROC)(GLuint,GLuint);
        typedef void   (APIENTRY *PFNGLLINKPROGRAMPROC)(GLuint);
        typedef void   (APIENTRY *PFNGLGETPROGRAMIVPROC)(GLuint,GLenum,GLint*);
        typedef void   (APIENTRY *PFNGLGETPROGRAMINFOLOGPROC)(GLuint,GLsizei,GLsizei*,GLchar*);
        typedef void   (APIENTRY *PFNGLDELETEPROGRAMPROC)(GLuint);
        typedef void   (APIENTRY *PFNGLUSEPROGRAMPROC)(GLuint);
        typedef GLint  (APIENTRY *PFNGLGETUNIFORMLOCATIONPROC)(GLuint,const GLchar*);
        typedef void   (APIENTRY *PFNGLUNIFORM1IPROC)(GLint,GLint);
        typedef void   (APIENTRY *PFNGLUNIFORMMATRIX4FVPROC)(GLint,GLsizei,GLboolean,const GLfloat*);
        typedef void   (APIENTRY *PFNGLGENVERTEXARRAYSPROC)(GLsizei,GLuint*);
        typedef void   (APIENTRY *PFNGLBINDVERTEXARRAYPROC)(GLuint);
        typedef void   (APIENTRY *PFNGLDELETEVERTEXARRAYSPROC)(GLsizei,const GLuint*);
        typedef void   (APIENTRY *PFNGLGENBUFFERSPROC)(GLsizei,GLuint*);
        typedef void   (APIENTRY *PFNGLBINDBUFFERPROC)(GLenum,GLuint);
        typedef void   (APIENTRY *PFNGLBUFFERDATAPROC)(GLenum,GLsizeiptr,const void*,GLenum);
        typedef void   (APIENTRY *PFNGLBUFFERSUBDATAPROC)(GLenum,GLintptr,GLsizeiptr,const void*);
        typedef void   (APIENTRY *PFNGLDELETEBUFFERSPROC)(GLsizei,const GLuint*);
        typedef void   (APIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint);
        typedef void   (APIENTRY *PFNGLVERTEXATTRIBPOINTERPROC)(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*);
        typedef void   (APIENTRY *PFNGLDRAWELEMENTSBASEVERTEXPROC)(GLenum,GLsizei,GLenum,const void*,GLint);
        typedef void   (APIENTRY *PFNGLACTIVETEXTUREPROC)(GLenum);
        typedef void   (APIENTRY *PFNGLBLENDFUNCSEPARATEPROC)(GLenum,GLenum,GLenum,GLenum);
        typedef void   (APIENTRY *PFNGLTEXIMAGE2DPROC_FN)(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*);
        typedef void   (APIENTRY *PFNGLTEXSUBIMAGE2DPROC_FN)(GLenum,GLint,GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,const void*);
        typedef void   (APIENTRY *PFNGLGETINTEGERVPROC_FN)(GLenum,GLint*);
        typedef void   (APIENTRY *PFNGLVIEWPORTPROC_FN)(GLint,GLint,GLsizei,GLsizei);
 
        // Résolution via le loader existant (NkOpenGLContext l'a déjà fait)
        // On utilise wglGetProcAddress pour les extensions GL 1.2+
        static PFNGLCREATESHADERPROC            glCreateShader           = nullptr;
        static PFNGLSHADERSOURCEPROC            glShaderSource           = nullptr;
        static PFNGLCOMPILESHADERPROC           glCompileShader          = nullptr;
        static PFNGLGETSHADERIVPROC             glGetShaderiv            = nullptr;
        static PFNGLGETSHADERINFOLOGPROC        glGetShaderInfoLog       = nullptr;
        static PFNGLDELETESHADERPROC            glDeleteShader           = nullptr;
        static PFNGLCREATEPROGRAMPROC           glCreateProgram          = nullptr;
        static PFNGLATTACHSHADERPROC            glAttachShader           = nullptr;
        static PFNGLLINKPROGRAMPROC             glLinkProgram            = nullptr;
        static PFNGLGETPROGRAMIVPROC            glGetProgramiv           = nullptr;
        static PFNGLGETPROGRAMINFOLOGPROC       glGetProgramInfoLog      = nullptr;
        static PFNGLDELETEPROGRAMPROC           glDeleteProgram          = nullptr;
        static PFNGLUSEPROGRAMPROC              glUseProgram             = nullptr;
        static PFNGLGETUNIFORMLOCATIONPROC      glGetUniformLocation     = nullptr;
        static PFNGLUNIFORM1IPROC               glUniform1i              = nullptr;
        static PFNGLUNIFORMMATRIX4FVPROC        glUniformMatrix4fv       = nullptr;
        static PFNGLGENVERTEXARRAYSPROC         glGenVertexArrays        = nullptr;
        static PFNGLBINDVERTEXARRAYPROC         glBindVertexArray        = nullptr;
        static PFNGLDELETEVERTEXARRAYSPROC      glDeleteVertexArrays     = nullptr;
        static PFNGLGENBUFFERSPROC              glGenBuffers             = nullptr;
        static PFNGLBINDBUFFERPROC              glBindBuffer             = nullptr;
        static PFNGLBUFFERDATAPROC              glBufferData             = nullptr;
        static PFNGLBUFFERSUBDATAPROC           glBufferSubData          = nullptr;
        static PFNGLDELETEBUFFERSPROC           glDeleteBuffers          = nullptr;
        static PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray= nullptr;
        static PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer    = nullptr;
        static PFNGLACTIVETEXTUREPROC           glActiveTexture          = nullptr;
        static PFNGLBLENDFUNCSEPARATEPROC       glBlendFuncSeparate      = nullptr;
 
        // Chargement manuel (appelé une fois dans NkOpenGLRenderer2D::Initialize)
        static bool sGL33Loaded = false;
        static void LoadGL33Procs() {
            if (sGL33Loaded) return;
            auto get = [](const char* name) -> void* {
                void* p = (void*)wglGetProcAddress(name);
                if (!p || p == (void*)1 || p == (void*)2 || p == (void*)3 || p == (void*)-1) {
                    static HMODULE kLib = LoadLibraryA("opengl32.dll");
                    p = kLib ? (void*)GetProcAddress(kLib, name) : nullptr;
                }
                return p;
            };
            #define LOAD(fn) fn = (decltype(fn))get(#fn)
            LOAD(glCreateShader);       LOAD(glShaderSource);
            LOAD(glCompileShader);      LOAD(glGetShaderiv);
            LOAD(glGetShaderInfoLog);   LOAD(glDeleteShader);
            LOAD(glCreateProgram);      LOAD(glAttachShader);
            LOAD(glLinkProgram);        LOAD(glGetProgramiv);
            LOAD(glGetProgramInfoLog);  LOAD(glDeleteProgram);
            LOAD(glUseProgram);         LOAD(glGetUniformLocation);
            LOAD(glUniform1i);          LOAD(glUniformMatrix4fv);
            LOAD(glGenVertexArrays);    LOAD(glBindVertexArray);
            LOAD(glDeleteVertexArrays); LOAD(glGenBuffers);
            LOAD(glBindBuffer);         LOAD(glBufferData);
            LOAD(glBufferSubData);      LOAD(glDeleteBuffers);
            LOAD(glEnableVertexAttribArray); LOAD(glVertexAttribPointer);
            LOAD(glActiveTexture);      LOAD(glBlendFuncSeparate);
            #undef LOAD
            sGL33Loaded = true;
        }
#   endif // has_include
 
#elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#   if __has_include("glad/glx.h")
#       include "glad/glx.h"
#       include "glad/gl.h"
#   else
#       include <GL/glx.h>
#       include <GL/glext.h>
#   endif
#elif defined(NKENTSEU_WINDOWING_WAYLAND)
#   if __has_include("glad/egl.h")
#       include "glad/egl.h"
#       include "glad/gles2.h"
#   else
#       include <EGL/egl.h>
#       include <GLES3/gl31.h>
#   endif
#elif defined(NKENTSEU_PLATFORM_ANDROID)
#       include <EGL/egl.h>
#       include <GLES3/gl31.h>
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#   include <GLES3/gl3.h>
#else
#   if __has_include("glad/gl.h")
#       include "glad/gl.h"
#   else
#       include <GL/gl.h>
#       include <GL/glext.h>
#   endif
#endif
 
// GL constants manquants sous certains environnements
#ifndef GL_CLAMP_TO_EDGE
#   define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_ARRAY_BUFFER
#   define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
#   define GL_ELEMENT_ARRAY_BUFFER 0x8893
#endif
#ifndef GL_DYNAMIC_DRAW
#   define GL_DYNAMIC_DRAW 0x88E8
#endif
#ifndef GL_STATIC_DRAW
#   define GL_STATIC_DRAW 0x88B4
#endif
#ifndef GL_VERTEX_SHADER
#   define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#   define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_COMPILE_STATUS
#   define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#   define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#   define GL_INFO_LOG_LENGTH 0x8B84
#endif
#ifndef GL_TEXTURE0
#   define GL_TEXTURE0 0x84C0
#endif
 
#define NK_GL2D_LOG(...) logger.Infof("[NkGL2D] " __VA_ARGS__)
#define NK_GL2D_ERR(...) logger.Errorf("[NkGL2D] " __VA_ARGS__)

namespace nkentseu {
    namespace renderer {

        // ── GLSL sources (compatible with GL 3.3 core and GLES 3.0) ─────────────────
        static const char* kVertSrc =
        #if defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        R"(#version 300 es
        precision mediump float;
        )"
        #else
        R"(#version 330 core
        )"
        #endif
        R"(
        layout(location=0) in vec2  a_Pos;
        layout(location=1) in vec2  a_UV;
        layout(location=2) in vec4  a_Color;

        uniform mat4 u_Projection;

        out vec2  v_UV;
        out vec4  v_Color;

        void main() {
            v_UV    = a_UV;
            v_Color = a_Color;
            gl_Position = u_Projection * vec4(a_Pos, 0.0, 1.0);
        }
        )";

        static const char* kFragSrc =
        #if defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        R"(#version 300 es
        precision mediump float;
        )"
        #else
        R"(#version 330 core
        )"
        #endif
        R"(
        in vec2  v_UV;
        in vec4  v_Color;

        uniform sampler2D u_Texture;

        out vec4 frag;

        void main() {
            frag = texture(u_Texture, v_UV) * v_Color;
        }
        )";

        // =============================================================================
        static uint32 CompileGLShader(GLenum type, const char* src) {
            GLuint s = glCreateShader(type);
            glShaderSource(s, 1, &src, nullptr);
            glCompileShader(s);
            GLint ok = 0;
            glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
            if (!ok) {
                GLint len = 0;
                glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
                char* buf = new char[len+1]();
                glGetShaderInfoLog(s, len, nullptr, buf);
                logger.Errorf("[NkGL2D] Shader compile error:\n%s", buf);
                delete[] buf;
                glDeleteShader(s);
                return 0;
            }
            return (uint32)s;
        }

        // =============================================================================
        bool NkOpenGLRenderer2D::Initialize(NkIGraphicsContext* ctx) {
            if (mIsValid) { NK_GL2D_ERR("Already initialized"); return false; }
            mCtx = ctx;
        
            ctx->MakeCurrent();
        
            // ── Charger les entry points GL 3.3+ via le loader du contexte existant ──
            // NkOpenGLContext a déjà appelé gladLoadGL() lors de sa propre init.
            // Sous Windows avec NK_NO_GLAD2, les fonctions sont dans le glad interne.
            // On doit appeler le loader manuellement si les pointeurs statiques
            // ci-dessus ne sont pas encore résolus.
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
        #   if !__has_include("glad/gl.h") && !__has_include("NKGlad/glad/gl.h")
            // Fallback : charger manuellement via wglGetProcAddress
            LoadGL33Procs();
        #   else
            // GLAD est disponible — gladLoadGL() a été appelé par NkOpenGLContext.
            // Rien à faire : les fonctions gl* sont des symboles résolus par GLAD.
            // MAIS il faut appeler gladLoadGL() si ce n'est pas déjà fait.
            // Utiliser le loader du contexte :
            {
                auto loader = NkNativeContext::GetOpenGLProcAddressLoader(ctx);
                if (loader) {
                    // gladLoadGL retourne 0 si déjà chargé ou en cas d'erreur légère
                    // dans ce contexte. On ignore le retour — on teste une fonction.
                    gladLoadGL((GLADloadfunc)loader);
                }
            }
        #   endif
        #endif
        
            if (!CompileShader()) return false;
            SetupVAO();
        
            // 1×1 white texture
            const uint8 white[4] = {255, 255, 255, 255};
            mWhiteTexId = CreateGLTexture(1, 1, white);
        
            NkContextInfo info = ctx->GetInfo();
            const uint32 W = info.windowWidth  > 0 ? info.windowWidth  : 800;
            const uint32 H = info.windowHeight > 0 ? info.windowHeight : 600;
            mDefaultView.center = { W * 0.5f, H * 0.5f };
            mDefaultView.size   = { (float)W, (float)H };
            mCurrentView        = mDefaultView;
            mViewport           = { 0, 0, (int32)W, (int32)H };
        
            float proj[16];
            mCurrentView.ToProjectionMatrix(proj);
            UploadProjection(proj);
        
            mIsValid = true;
            NK_GL2D_LOG("Initialized (white tex=%u)", mWhiteTexId);
            return true;
        }

        // =============================================================================
        void NkOpenGLRenderer2D::Shutdown() {
            if (!mIsValid) return;
            if (mWhiteTexId) { glDeleteTextures(1, (GLuint*)&mWhiteTexId); mWhiteTexId = 0; }
            if (mEBO)     { glDeleteBuffers(1, (GLuint*)&mEBO);     mEBO = 0; }
            if (mVBO)     { glDeleteBuffers(1, (GLuint*)&mVBO);     mVBO = 0; }
            if (mVAO)     { glDeleteVertexArrays(1, (GLuint*)&mVAO); mVAO = 0; }
            if (mProgram) { glDeleteProgram((GLuint)mProgram);       mProgram = 0; }
            mIsValid = false;
            NK_GL2D_LOG("Shutdown");
        }

        // =============================================================================
        void NkOpenGLRenderer2D::Clear(const NkColor2D& col) {
            math::NkVec4f cf = (math::NkVec4f)col;
            glClearColor(cf.r, cf.g, cf.b, cf.a);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        // =============================================================================
        bool NkOpenGLRenderer2D::CompileShader() {
            uint32 vs = CompileGLShader(GL_VERTEX_SHADER,   kVertSrc);
            uint32 fs = CompileGLShader(GL_FRAGMENT_SHADER, kFragSrc);
            if (!vs || !fs) {
                if (vs) glDeleteShader((GLuint)vs);
                if (fs) glDeleteShader((GLuint)fs);
                return false;
            }
            mProgram = (uint32)glCreateProgram();
            glAttachShader((GLuint)mProgram, (GLuint)vs);
            glAttachShader((GLuint)mProgram, (GLuint)fs);
            glLinkProgram((GLuint)mProgram);
            glDeleteShader((GLuint)vs);
            glDeleteShader((GLuint)fs);

            GLint ok = 0;
            glGetProgramiv((GLuint)mProgram, GL_LINK_STATUS, &ok);
            if (!ok) {
                GLint len = 0;
                glGetProgramiv((GLuint)mProgram, GL_INFO_LOG_LENGTH, &len);
                char* buf = new char[len+1]();
                glGetProgramInfoLog((GLuint)mProgram, len, nullptr, buf);
                NK_GL2D_ERR("Shader link:\n%s", buf);
                delete[] buf;
                return false;
            }
            mUniProj = glGetUniformLocation((GLuint)mProgram, "u_Projection");
            mUniTex  = glGetUniformLocation((GLuint)mProgram, "u_Texture");
            return true;
        }

        // =============================================================================
        void NkOpenGLRenderer2D::SetupVAO() {
            glGenVertexArrays(1, (GLuint*)&mVAO);
            glBindVertexArray((GLuint)mVAO);

            glGenBuffers(1, (GLuint*)&mVBO);
            glBindBuffer(GL_ARRAY_BUFFER, (GLuint)mVBO);
            glBufferData(GL_ARRAY_BUFFER, kMaxVertices * (GLsizeiptr)sizeof(NkVertex2D), nullptr, GL_DYNAMIC_DRAW);

            glGenBuffers(1, (GLuint*)&mEBO);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)mEBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, kMaxIndices * (GLsizeiptr)sizeof(uint32), nullptr, GL_DYNAMIC_DRAW);

            // a_Pos  (location 0): float x, float y
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(NkVertex2D), (void*)offsetof(NkVertex2D, x));
            // a_UV   (location 1): float u, float v
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(NkVertex2D), (void*)offsetof(NkVertex2D, u));
            // a_Color(location 2): uint8 r,g,b,a → normalized float
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, (GLsizei)sizeof(NkVertex2D), (void*)offsetof(NkVertex2D, r));

            glBindVertexArray(0);
        }

        // =============================================================================
        void NkOpenGLRenderer2D::BeginBackend() {
            // Save relevant GL state
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glUseProgram((GLuint)mProgram);
            if (mUniTex >= 0) glUniform1i(mUniTex, 0);
            glActiveTexture(GL_TEXTURE0);
            glViewport(mViewport.left, mViewport.top, mViewport.width, mViewport.height);
            mLastBoundTexId = 0;
            mLastBlend      = NkBlendMode::NK_NONE;
        }

        // =============================================================================
        void NkOpenGLRenderer2D::EndBackend() {
            glBindVertexArray(0);
            glUseProgram(0);
        }

        // =============================================================================
        void NkOpenGLRenderer2D::ApplyBlendMode(NkBlendMode mode) {
            if (mode == mLastBlend) return;
            mLastBlend = mode;
            switch (mode) {
                case NkBlendMode::NK_ALPHA:
                    glEnable(GL_BLEND);
                    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                    break;
                case NkBlendMode::NK_ADD:
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                    break;
                case NkBlendMode::NK_MULTIPLY:
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_DST_COLOR, GL_ZERO);
                    break;
                case NkBlendMode::NK_NONE:
                default:
                    glDisable(GL_BLEND);
                    break;
            }
        }

        // =============================================================================
        void NkOpenGLRenderer2D::BindTexture(const NkTexture* tex) {
            uint32 id = tex ? tex->GetGPUId() : mWhiteTexId;
            if (!id) id = mWhiteTexId;
            if (id == mLastBoundTexId) return;
            mLastBoundTexId = id;
            glBindTexture(GL_TEXTURE_2D, (GLuint)id);
        }

        // =============================================================================
        void NkOpenGLRenderer2D::SubmitBatches(const NkBatchGroup* groups, uint32 groupCount,
                                                const NkVertex2D* verts, uint32 vCount,
                                                const uint32*     idx,   uint32 iCount) {
            if (!mVAO || !vCount || !iCount) return;

            glBindVertexArray((GLuint)mVAO);

            // Upload vertex/index data
            glBindBuffer(GL_ARRAY_BUFFER, (GLuint)mVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(vCount * sizeof(NkVertex2D)), verts);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)mEBO);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, (GLsizeiptr)(iCount * sizeof(uint32)), idx);

            // Issue one draw call per group
            for (uint32 i = 0; i < groupCount; ++i) {
                const auto& g = groups[i];
                ApplyBlendMode(g.blendMode);
                BindTexture(g.texture);
                glDrawElements(GL_TRIANGLES, (GLsizei)g.indexCount, GL_UNSIGNED_INT,
                            (void*)(uintptr_t)(g.indexStart * sizeof(uint32)));
            }
        }

        // =============================================================================
        void NkOpenGLRenderer2D::UploadProjection(const float32 proj[16]) {
            if (mUniProj >= 0 && mProgram) {
                glUseProgram((GLuint)mProgram);
                glUniformMatrix4fv(mUniProj, 1, GL_FALSE, proj);
            }
        }

        // =============================================================================
        uint32 NkOpenGLRenderer2D::CreateGLTexture(uint32 w, uint32 h, const uint8* rgba) {
            GLuint id = 0;
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)w, (GLsizei)h, 0,
                        GL_RGBA, GL_UNSIGNED_BYTE, rgba);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            return (uint32)id;
        }

        void NkOpenGLRenderer2D::UpdateGLTexture(uint32 id, uint32 x, uint32 y,
                                                uint32 w, uint32 h, const uint8* rgba) {
            glBindTexture(GL_TEXTURE_2D, (GLuint)id);
            glTexSubImage2D(GL_TEXTURE_2D, 0, (GLint)x, (GLint)y,
                            (GLsizei)w, (GLsizei)h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        }

        void NkOpenGLRenderer2D::DeleteGLTexture(uint32 id) {
            GLuint gid = (GLuint)id;
            glDeleteTextures(1, &gid);
        }

        void NkOpenGLRenderer2D::SetGLTextureFilter(uint32 id, NkTextureFilter filter) {
            GLint gl = (filter == NkTextureFilter::NK_NEAREST) ? GL_NEAREST : GL_LINEAR;
            glBindTexture(GL_TEXTURE_2D, (GLuint)id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl);
        }

        void NkOpenGLRenderer2D::SetGLTextureWrap(uint32 id, NkTextureWrap wrap) {
            GLint gl = GL_CLAMP_TO_EDGE;
            if      (wrap == NkTextureWrap::NK_REPEAT)       gl = GL_REPEAT;
            else if (wrap == NkTextureWrap::NK_MIRROR_REPEAT) gl = GL_MIRRORED_REPEAT;
            glBindTexture(GL_TEXTURE_2D, (GLuint)id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl);
        }

    } // namespace renderer
} // namespace nkentseu