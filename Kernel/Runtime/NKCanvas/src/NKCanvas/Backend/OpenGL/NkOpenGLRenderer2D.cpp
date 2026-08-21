// =============================================================================
// NkOpenGLRenderer2D.cpp — OpenGL 4.3 / GLES 3.0 implementation
// =============================================================================
#include "NkOpenGLRenderer2D.h"
#include "NKCanvas/Renderer/Resources/NkTexture.h"
#include "NKCanvas/Renderer/Resources/NkTextureBackend.h"
#include "NKCanvas/Renderer/Resources/NkShader.h"
#include "NKCanvas/Renderer/Resources/NkShaderBackend.h"
#include "NKCanvas/Renderer/Targets/NkRenderTextureBackend.h"
#include "NKCanvas/Renderer/Core/NkRenderer2DTypes.h"
#include "NKCanvas/Core/NkIGraphicsContext.h"
#include "NKCanvas/Core/NkNativeContextAccess.h"
#include "NKLogger/NkLog.h"
#include "NKMemory/NkAllocator.h"

#include <cstring>

#if defined(NKENTSEU_PLATFORM_WINDOWS)
// Windows : glad headers via NKGlad
// Ajuster le chemin selon où NKGlad expose ses headers dans les includedirs
#if __has_include("glad/wgl.h")
#include "glad/wgl.h"
#include "glad/gl.h"
#elif __has_include("NKGlad/glad/wgl.h")
#include "NKGlad/glad/wgl.h"
#include "NKGlad/glad/gl.h"
#else
// Fallback : charger glext.h qui déclare les types et macros GL 3.3+
// mais pas les pointeurs de fonctions — utiliser les fonctions via
// le mécanisme de résolution dynamique du loader existant.
#include <GL/gl.h>
#include <GL/glext.h>
// Déclarer les pointeurs de fonctions manuellement pour GL 3.3+
// (ils sont résolus par NkOpenGLContext::LoadOpenGLEntryPoints via gladLoadGL)
typedef GLuint(APIENTRY *PFNGLCREATESHADERPROC)(GLenum);
typedef void(APIENTRY *PFNGLSHADERSOURCEPROC)(GLuint, GLsizei, const GLchar *const *, const GLint *);
typedef void(APIENTRY *PFNGLCOMPILESHADERPROC)(GLuint);
typedef void(APIENTRY *PFNGLGETSHADERIVPROC)(GLuint, GLenum, GLint *);
typedef void(APIENTRY *PFNGLGETSHADERINFOLOGPROC)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef void(APIENTRY *PFNGLDELETESHADERPROC)(GLuint);
typedef GLuint(APIENTRY *PFNGLCREATEPROGRAMPROC)(void);
typedef void(APIENTRY *PFNGLATTACHSHADERPROC)(GLuint, GLuint);
typedef void(APIENTRY *PFNGLLINKPROGRAMPROC)(GLuint);
typedef void(APIENTRY *PFNGLGETPROGRAMIVPROC)(GLuint, GLenum, GLint *);
typedef void(APIENTRY *PFNGLGETPROGRAMINFOLOGPROC)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef void(APIENTRY *PFNGLDELETEPROGRAMPROC)(GLuint);
typedef void(APIENTRY *PFNGLUSEPROGRAMPROC)(GLuint);
typedef GLint(APIENTRY *PFNGLGETUNIFORMLOCATIONPROC)(GLuint, const GLchar *);
typedef void(APIENTRY *PFNGLUNIFORM1IPROC)(GLint, GLint);
typedef void(APIENTRY *PFNGLUNIFORMMATRIX4FVPROC)(GLint, GLsizei, GLboolean, const GLfloat *);
typedef void(APIENTRY *PFNGLGENVERTEXARRAYSPROC)(GLsizei, GLuint *);
typedef void(APIENTRY *PFNGLBINDVERTEXARRAYPROC)(GLuint);
typedef void(APIENTRY *PFNGLDELETEVERTEXARRAYSPROC)(GLsizei, const GLuint *);
typedef void(APIENTRY *PFNGLGENBUFFERSPROC)(GLsizei, GLuint *);
typedef void(APIENTRY *PFNGLBINDBUFFERPROC)(GLenum, GLuint);
typedef void(APIENTRY *PFNGLBUFFERDATAPROC)(GLenum, GLsizeiptr, const void *, GLenum);
typedef void(APIENTRY *PFNGLBUFFERSUBDATAPROC)(GLenum, GLintptr, GLsizeiptr, const void *);
typedef void(APIENTRY *PFNGLDELETEBUFFERSPROC)(GLsizei, const GLuint *);
typedef void(APIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint);
typedef void(APIENTRY *PFNGLVERTEXATTRIBPOINTERPROC)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
typedef void(APIENTRY *PFNGLDRAWELEMENTSBASEVERTEXPROC)(GLenum, GLsizei, GLenum, const void *, GLint);
typedef void(APIENTRY *PFNGLACTIVETEXTUREPROC)(GLenum);
typedef void(APIENTRY *PFNGLBLENDFUNCSEPARATEPROC)(GLenum, GLenum, GLenum, GLenum);
typedef void(APIENTRY *PFNGLTEXIMAGE2DPROC_FN)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum,
											   const void *);
typedef void(APIENTRY *PFNGLTEXSUBIMAGE2DPROC_FN)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum,
												  const void *);
typedef void(APIENTRY *PFNGLGETINTEGERVPROC_FN)(GLenum, GLint *);
typedef void(APIENTRY *PFNGLVIEWPORTPROC_FN)(GLint, GLint, GLsizei, GLsizei);

// Résolution via le loader existant (NkOpenGLContext l'a déjà fait)
// On utilise wglGetProcAddress pour les extensions GL 1.2+
static PFNGLCREATESHADERPROC glCreateShader = nullptr;
static PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
static PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
static PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
static PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = nullptr;
static PFNGLDELETESHADERPROC glDeleteShader = nullptr;
static PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
static PFNGLATTACHSHADERPROC glAttachShader = nullptr;
static PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
static PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
static PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = nullptr;
static PFNGLDELETEPROGRAMPROC glDeleteProgram = nullptr;
static PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
static PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
static PFNGLUNIFORM1IPROC glUniform1i = nullptr;
static PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = nullptr;
static PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = nullptr;
static PFNGLBINDVERTEXARRAYPROC glBindVertexArray = nullptr;
static PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = nullptr;
static PFNGLGENBUFFERSPROC glGenBuffers = nullptr;
static PFNGLBINDBUFFERPROC glBindBuffer = nullptr;
static PFNGLBUFFERDATAPROC glBufferData = nullptr;
static PFNGLBUFFERSUBDATAPROC glBufferSubData = nullptr;
static PFNGLDELETEBUFFERSPROC glDeleteBuffers = nullptr;
static PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = nullptr;
static PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = nullptr;
static PFNGLACTIVETEXTUREPROC glActiveTexture = nullptr;
static PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate = nullptr;
// Uniforms additionnels utilises par le dispatch NkShader (CreateGLShader/SetGLShader*).
static PFNGLUNIFORM1FPROC glUniform1f = nullptr;
static PFNGLUNIFORM2FPROC glUniform2f = nullptr;
static PFNGLUNIFORM3FPROC glUniform3f = nullptr;
static PFNGLUNIFORM4FPROC glUniform4f = nullptr;
// FBO API (utilisee par CreateGLRenderTexture / Bind / Unbind / Destroy).
static PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = nullptr;
static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = nullptr;
static PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = nullptr;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = nullptr;
static PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = nullptr;

// Chargement manuel (appelé une fois dans NkOpenGLRenderer2D::Initialize)
static bool sGL33Loaded = false;

static void LoadGL33Procs() {
	if (sGL33Loaded)
		return;
	auto get = [](const char *name) -> void * {
		void *p = (void *)wglGetProcAddress(name);
		if (!p || p == (void *)1 || p == (void *)2 || p == (void *)3 || p == (void *)-1) {
			static HMODULE kLib = LoadLibraryA("opengl32.dll");
			p = kLib ? (void *)GetProcAddress(kLib, name) : nullptr;
		}
		return p;
	};
#define LOAD(fn) fn = (decltype(fn))get(#fn)
	LOAD(glCreateShader);
	LOAD(glShaderSource);
	LOAD(glCompileShader);
	LOAD(glGetShaderiv);
	LOAD(glGetShaderInfoLog);
	LOAD(glDeleteShader);
	LOAD(glCreateProgram);
	LOAD(glAttachShader);
	LOAD(glLinkProgram);
	LOAD(glGetProgramiv);
	LOAD(glGetProgramInfoLog);
	LOAD(glDeleteProgram);
	LOAD(glUseProgram);
	LOAD(glGetUniformLocation);
	LOAD(glUniform1i);
	LOAD(glUniformMatrix4fv);
	LOAD(glGenVertexArrays);
	LOAD(glBindVertexArray);
	LOAD(glDeleteVertexArrays);
	LOAD(glGenBuffers);
	LOAD(glBindBuffer);
	LOAD(glBufferData);
	LOAD(glBufferSubData);
	LOAD(glDeleteBuffers);
	LOAD(glEnableVertexAttribArray);
	LOAD(glVertexAttribPointer);
	LOAD(glActiveTexture);
	LOAD(glBlendFuncSeparate);
	LOAD(glUniform1f);
	LOAD(glUniform2f);
	LOAD(glUniform3f);
	LOAD(glUniform4f);
	LOAD(glGenFramebuffers);
	LOAD(glBindFramebuffer);
	LOAD(glFramebufferTexture2D);
	LOAD(glCheckFramebufferStatus);
	LOAD(glDeleteFramebuffers);
#undef LOAD
	sGL33Loaded = true;
}
#endif // has_include

#elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#if __has_include("glad/glx.h")
#include "glad/glx.h"
#include "glad/gl.h"
#else
#include <GL/glx.h>
#include <GL/glext.h>
#endif
#elif defined(NKENTSEU_WINDOWING_WAYLAND)
#if __has_include("glad/egl.h")
#include "glad/egl.h"
#include "glad/gles2.h"
#else
#include <EGL/egl.h>
#include <GLES3/gl31.h>
#endif
#elif defined(NKENTSEU_PLATFORM_ANDROID)
#include <EGL/egl.h>
#include <GLES3/gl31.h>
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#include <GLES3/gl3.h>
#else
#if __has_include("glad/gl.h")
#include "glad/gl.h"
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#endif

// GL constants manquants sous certains environnements
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88B4
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH 0x8B84
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif

#define NK_GL2D_LOG(...) logger.Infof("[NkGL2D] " __VA_ARGS__)
#define NK_GL2D_ERR(...) logger.Errorf("[NkGL2D] " __VA_ARGS__)

namespace nkentseu {
	namespace renderer {

		// ── GLSL sources (compatible with GL 3.3 core and GLES 3.0) ─────────────────
		static const char *kVertSrc =
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

		static const char *kFragSrc =
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
		static uint32 CompileGLShader(GLenum type, const char *src) {
			GLuint s = glCreateShader(type);
			glShaderSource(s, 1, &src, nullptr);
			glCompileShader(s);
			GLint ok = 0;
			glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
			if (!ok) {
				GLint len = 0;
				glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
				char *buf = (char *)nkentseu::memory::NkAlloc((nk_size)(len + 1));
				glGetShaderInfoLog(s, len, nullptr, buf);
				logger.Errorf("[NkGL2D] Shader compile error:\n%s", buf);
				nkentseu::memory::NkFree(buf);
				glDeleteShader(s);
				return 0;
			}
			return (uint32)s;
		}

		// =============================================================================
		bool NkOpenGLRenderer2D::Initialize(NkIGraphicsContext *ctx) {
			if (mIsValid) {
				NK_GL2D_ERR("Already initialized");
				return false;
			}
			mCtx = ctx;

			ctx->MakeCurrent();

			// ── Charger les entry points GL 3.3+ via le loader du contexte existant ──
			// NkOpenGLContext a déjà appelé gladLoadGL() lors de sa propre init.
			// Sous Windows avec NK_NO_GLAD2, les fonctions sont dans le glad interne.
			// On doit appeler le loader manuellement si les pointeurs statiques
			// ci-dessus ne sont pas encore résolus.
#if defined(NKENTSEU_PLATFORM_WINDOWS)
#if !__has_include("glad/gl.h") && !__has_include("NKGlad/glad/gl.h")
			// Fallback : charger manuellement via wglGetProcAddress
			LoadGL33Procs();
#else
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
#endif
#endif

			if (!CompileShader())
				return false;
			SetupVAO();

			// 1×1 white texture
			const uint8 white[4] = {255, 255, 255, 255};
			mWhiteTexId = CreateGLTexture(1, 1, white);

			NkContextInfo info = ctx->GetInfo();
			const uint32 W = info.windowWidth > 0 ? info.windowWidth : 800;
			const uint32 H = info.windowHeight > 0 ? info.windowHeight : 600;
			mDefaultView.center = {W * 0.5f, H * 0.5f};
			mDefaultView.size = {(float)W, (float)H};
			mCurrentView = mDefaultView;
			mViewport = {0, 0, (int32)W, (int32)H};

			float proj[16];
			mCurrentView.ToProjectionMatrix(proj);
			UploadProjection(proj);

			// ── Enregistrement dispatch table NkTexture (cf NkTextureBackend.h) ──
			// Les helpers sont static dans la classe : assignables directement
			// comme function pointers. NkTexture::Create/Update/Destroy/...
			// utilisera ces callbacks pour gerer ses textures GL.
			{
				NkTextureBackend backend{};
				backend.Create = &NkOpenGLRenderer2D::CreateGLTexture;
				backend.Update = &NkOpenGLRenderer2D::UpdateGLTexture;
				backend.Destroy = &NkOpenGLRenderer2D::DeleteGLTexture;
				backend.SetFilter = &NkOpenGLRenderer2D::SetGLTextureFilter;
				backend.SetWrap = &NkOpenGLRenderer2D::SetGLTextureWrap;
				NkTextureSetBackend(backend);
			}

			// ── Enregistrement dispatch table NkShader (cf NkShaderBackend.h) ──
			// L'utilisateur passe du GLSL via NkShader::SetSourceGLSL ; ce backend
			// compile + link + active le programme via les helpers statics ci-bas.
			// Tant qu'aucun shader user n'est attache a NkRenderStates::shader,
			// le programme par defaut du renderer (mProgram) reste utilise.
			{
				NkShaderBackend sb{};
				sb.Create = &NkOpenGLRenderer2D::CreateGLShader;
				sb.Destroy = &NkOpenGLRenderer2D::DestroyGLShader;
				sb.Use = &NkOpenGLRenderer2D::UseGLShader;
				sb.SetFloat = &NkOpenGLRenderer2D::SetGLShaderFloat;
				sb.SetVec2 = &NkOpenGLRenderer2D::SetGLShaderVec2;
				sb.SetVec3 = &NkOpenGLRenderer2D::SetGLShaderVec3;
				sb.SetVec4 = &NkOpenGLRenderer2D::SetGLShaderVec4;
				sb.SetMat4 = &NkOpenGLRenderer2D::SetGLShaderMat4;
				sb.SetTexture = &NkOpenGLRenderer2D::SetGLShaderTexture;
				NkShaderSetBackend(sb);
			}

			// ── Enregistrement dispatch table NkRenderTexture (FBO OpenGL) ─────
			{
				NkRenderTextureBackend rtb{};
				rtb.Create = &NkOpenGLRenderer2D::CreateGLRenderTexture;
				rtb.Destroy = &NkOpenGLRenderer2D::DestroyGLRenderTexture;
				rtb.Bind = &NkOpenGLRenderer2D::BindGLRenderTexture;
				rtb.Unbind = &NkOpenGLRenderer2D::UnbindGLRenderTexture;
				rtb.GetColorTextureGPUId = &NkOpenGLRenderer2D::GetGLRenderTextureColorId;
				NkRenderTextureSetBackend(rtb);
			}

			mIsValid = true;
			NK_GL2D_LOG("Initialized (white tex=%u)", mWhiteTexId);
			return true;
		}

		// =============================================================================
		void NkOpenGLRenderer2D::Shutdown() {
			if (!mIsValid)
				return;
			if (mWhiteTexId) {
				glDeleteTextures(1, (GLuint *)&mWhiteTexId);
				mWhiteTexId = 0;
			}
			if (mEBO) {
				glDeleteBuffers(1, (GLuint *)&mEBO);
				mEBO = 0;
			}
			if (mVBO) {
				glDeleteBuffers(1, (GLuint *)&mVBO);
				mVBO = 0;
			}
			if (mVAO) {
				glDeleteVertexArrays(1, (GLuint *)&mVAO);
				mVAO = 0;
			}
			if (mProgram) {
				glDeleteProgram((GLuint)mProgram);
				mProgram = 0;
			}
			mIsValid = false;
			NK_GL2D_LOG("Shutdown");
		}

		// =============================================================================
		void NkOpenGLRenderer2D::Clear(const NkColor2D &col) {
			math::NkVec4f cf = (math::NkVec4f)col;
			glClearColor(cf.r, cf.g, cf.b, cf.a);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}

		// =============================================================================
		bool NkOpenGLRenderer2D::CompileShader() {
			uint32 vs = CompileGLShader(GL_VERTEX_SHADER, kVertSrc);
			uint32 fs = CompileGLShader(GL_FRAGMENT_SHADER, kFragSrc);
			if (!vs || !fs) {
				if (vs)
					glDeleteShader((GLuint)vs);
				if (fs)
					glDeleteShader((GLuint)fs);
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
				char *buf = (char *)nkentseu::memory::NkAlloc((nk_size)(len + 1));
				glGetProgramInfoLog((GLuint)mProgram, len, nullptr, buf);
				NK_GL2D_ERR("Shader link:\n%s", buf);
				nkentseu::memory::NkFree(buf);
				return false;
			}
			mUniProj = glGetUniformLocation((GLuint)mProgram, "u_Projection");
			mUniTex = glGetUniformLocation((GLuint)mProgram, "u_Texture");
			return true;
		}

		// =============================================================================
		void NkOpenGLRenderer2D::SetupVAO() {
			glGenVertexArrays(1, (GLuint *)&mVAO);
			glBindVertexArray((GLuint)mVAO);

			glGenBuffers(1, (GLuint *)&mVBO);
			glBindBuffer(GL_ARRAY_BUFFER, (GLuint)mVBO);
			glBufferData(GL_ARRAY_BUFFER, kMaxVertices * (GLsizeiptr)sizeof(NkVertex2D), nullptr, GL_DYNAMIC_DRAW);

			glGenBuffers(1, (GLuint *)&mEBO);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)mEBO);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, kMaxIndices * (GLsizeiptr)sizeof(uint32), nullptr, GL_DYNAMIC_DRAW);

			// a_Pos  (location 0): float x, float y
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(NkVertex2D),
								  (void *)offsetof(NkVertex2D, x));
			// a_UV   (location 1): float u, float v
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(NkVertex2D),
								  (void *)offsetof(NkVertex2D, u));
			// a_Color(location 2): uint8 r,g,b,a → normalized float
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, (GLsizei)sizeof(NkVertex2D),
								  (void *)offsetof(NkVertex2D, r));

			glBindVertexArray(0);
		}

		// =============================================================================
		void NkOpenGLRenderer2D::BeginBackend() {
			// Save relevant GL state
			glEnable(GL_BLEND);
			// SEPARATE, et non glBlendFunc : le canal alpha de DESTINATION doit
			// etre traite a part.
			//
			// Avec glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA), l'alpha du
			// framebuffer devient srcA*srcA + dstA*(1-srcA) : il DIMINUE a chaque
			// couche dessinee. Sur un ecran ou l'on empile des dizaines de traces
			// par image, il s'effondre — mesure sur HarmonyOS : 255 au demarrage,
			// 206 apres une minute. La fenetre devient alors semi-transparente,
			// le compositeur laisse voir le fond au travers, et l'interface
			// n'apparait plus qu'en silhouettes sombres.
			//
			// Avec (GL_ONE, GL_ONE_MINUS_SRC_ALPHA) sur l'alpha, la couverture
			// s'accumule au lieu de se ronger, et la fenetre reste opaque.
			// ApplyBlendMode(NK_ALPHA) utilisait deja cette variante : les deux
			// chemins font desormais la meme chose.
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);
			glUseProgram((GLuint)mProgram);
			if (mUniTex >= 0)
				glUniform1i(mUniTex, 0);
			glActiveTexture(GL_TEXTURE0);
			glViewport(mViewport.left, mViewport.top, mViewport.width, mViewport.height);
			// Chaque frame demarre sans scissor (sinon un Clear serait clippe par le
			// scissor laisse par la frame precedente, l'etat GL etant persistant).
			glDisable(GL_SCISSOR_TEST);
			// Sentinelle IMPOSSIBLE, et non zero : le cache doit dire « j'ignore
			// ce qui est lie », pas « la texture 0 est liee ». Zero est une valeur
			// legitime (aucune texture, ou texture pas encore televersee) : la
			// placer ici faisait passer BindTexture pour un no-op au moment
			// precis ou il fallait agir. L'etat GL reel n'est de toute facon pas
			// connu en debut d'image — d'autres systemes dessinent aussi.
			mLastBoundTexId = 0xFFFFFFFFu;
			// L'etat MEMORISE doit decrire l'etat REEL qu'on vient de poser
			// ci-dessus : glEnable(GL_BLEND) + SRC_ALPHA/ONE_MINUS_SRC_ALPHA,
			// c'est-a-dire le mode ALPHA. Declarer NK_NONE ici mentait au cache :
			// une demande ulterieure de NK_NONE sortait aussitot (« deja dans ce
			// mode ») sans jamais appeler glDisable, et inversement le premier
			// ApplyBlendMode(NK_ALPHA) refaisait un travail deja fait. Un cache
			// d'etat qui ment finit toujours par se voir a l'ecran.
			mLastBlend = NkBlendMode::NK_ALPHA;
		}

		// =============================================================================
		void NkOpenGLRenderer2D::ApplyScissor(bool enabled, const NkRect2i &rect) {
			if (!enabled) {
				glDisable(GL_SCISSOR_TEST);
				return;
			}
			// Clip en pixels, origine haut-gauche -> glScissor a l'origine bas-gauche :
			// on inverse Y avec la hauteur de la surface (= mViewport.height, viewport
			// plein ecran a top=0).
			int32 w = rect.width < 0 ? 0 : rect.width;
			int32 h = rect.height < 0 ? 0 : rect.height;
			const int32 x = rect.x;
			const int32 y = mViewport.height - rect.y - h; // flip Y
			glEnable(GL_SCISSOR_TEST);
			glScissor((GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h);
		}

		// =============================================================================
		void NkOpenGLRenderer2D::EndBackend() {
			glBindVertexArray(0);
			glUseProgram(0);
		}

		// =============================================================================
		void NkOpenGLRenderer2D::ApplyBlendMode(NkBlendMode mode) {
			if (mode == mLastBlend)
				return;
#if defined(NKENTSEU_DEBUG) && (defined(NKENTSEU_PLATFORM_HARMONYOS) || defined(NKENTSEU_PLATFORM_ANDROID))
			// Une image detouree qui ressort en carre noir opaque signifie que le
			// blending etait DESACTIVE au moment de son trace. Les transitions
			// sont rares (quelques-unes par frame) : les journaliser dit quel mode
			// portait reellement le dessin, au lieu de le supposer.
			logger.Infof("[NkGL2D] blend %d -> %d\n", (int)mLastBlend, (int)mode);
#endif
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
		void NkOpenGLRenderer2D::BindTexture(const NkTexture *tex) {
			uint32 id = tex ? tex->GetGPUId() : mWhiteTexId;
			if (!id)
				id = mWhiteTexId;

			// Ne JAMAIS court-circuiter sur l'identifiant 0.
			//
			// Zero n'est pas un nom de texture : c'est « aucune texture », et
			// c'est aussi ce que vaut une texture pas encore televersee. Quand le
			// cache contenait cette valeur — ce qu'il faisait au debut de chaque
			// image — un dessin demandant 0 sortait d'ici sans rien lier, et le
			// shader echantillonnait une unite vide. Or GL rend alors exactement
			// (0,0,0,1) : du NOIR OPAQUE.
			//
			// A l'ecran, cela donnait un logo circulaire transforme en carre noir
			// (ses zones transparentes devenant opaques) et des elements colores
			// rendus en gris — le noir module par la couleur du sommet reste noir,
			// seul l'alpha du sommet subsistait. Une fois sur trois seulement, le
			// hasard de l'ordre d'initialisation donnait le bon rendu.
			if (id == 0u) {
				mLastBoundTexId = 0u;
				glBindTexture(GL_TEXTURE_2D, 0);
				return;
			}
			if (id == mLastBoundTexId)
				return;
			mLastBoundTexId = id;
			glBindTexture(GL_TEXTURE_2D, (GLuint)id);
		}

		// =============================================================================
		void NkOpenGLRenderer2D::SubmitBatches(const NkBatchGroup *groups, uint32 groupCount, const NkVertex2D *verts,
											   uint32 vCount, const uint32 *idx, uint32 iCount) {
			if (!mVAO || !vCount || !iCount)
				return;

#if defined(NKENTSEU_DEBUG) && (defined(NKENTSEU_PLATFORM_HARMONYOS) || defined(NKENTSEU_PLATFORM_ANDROID))
			// Ce que le renderer RECOIT, et non ce qu'il a en entree.
			//
			// Tout l'amont a ete verifie et se revele sain : textures chargees,
			// atlas peuple, alpha preserve, viewport correct. Si l'ecran reste
			// sombre malgre cela, la reponse est dans les COMMANDES : combien de
			// quads, et surtout de quelle couleur sont leurs sommets. Une teinte
			// de sommet noire ou a alpha nul eteint le rendu quelle que soit la
			// qualite des textures.
			{
				static uint64 frame = 0;
				if ((frame++ % 120u) == 0u) {
					unsigned cMin[4] = {255u, 255u, 255u, 255u};
					unsigned cMax[4] = {0u, 0u, 0u, 0u};
					const uint32 pas = vCount > 512u ? vCount / 256u : 1u;
					for (uint32 v = 0; v < vCount; v += pas) {
						const unsigned canaux[4] = {verts[v].r, verts[v].g, verts[v].b, verts[v].a};
						for (int k = 0; k < 4; ++k) {
							if (canaux[k] < cMin[k]) {
								cMin[k] = canaux[k];
							}
							if (canaux[k] > cMax[k]) {
								cMax[k] = canaux[k];
							}
						}
					}
					// Le VIEWPORT est joint a dessein : il est calcule une seule
					// fois, a l'initialisation du renderer. S'il ne correspond pas
					// a la surface courante, la geometrie est dessinee dans un coin
					// de l'ecran, ou hors champ — et le reste garde la couleur
					// d'effacement, ce qui donne un ecran sombre ou l'on ne devine
					// que quelques formes.
					logger.Infof("[NkGL2D lots] %u groupes, %u sommets | teinte R%u-%u V%u-%u B%u-%u A%u-%u | "
								 "viewport %d,%d %dx%d\n",
								 groupCount, vCount, cMin[0], cMax[0], cMin[1], cMax[1], cMin[2], cMax[2], cMin[3],
								 cMax[3], mViewport.left, mViewport.top, mViewport.width, mViewport.height);
				}
			}
#endif

			glBindVertexArray((GLuint)mVAO);

			// Upload vertex/index data
			glBindBuffer(GL_ARRAY_BUFFER, (GLuint)mVBO);
			glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(vCount * sizeof(NkVertex2D)), verts);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)mEBO);
			glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, (GLsizeiptr)(iCount * sizeof(uint32)), idx);

			// Issue one draw call per group
			for (uint32 i = 0; i < groupCount; ++i) {
				const auto &g = groups[i];
				ApplyBlendMode(g.blendMode);
				BindTexture(g.texture);
				glDrawElements(GL_TRIANGLES, (GLsizei)g.indexCount, GL_UNSIGNED_INT,
							   (void *)(uintptr_t)(g.indexStart * sizeof(uint32)));
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
		uint32 NkOpenGLRenderer2D::CreateGLTexture(uint32 w, uint32 h, const uint8 *rgba) {
			GLuint id = 0;
			glGenTextures(1, &id);
			glBindTexture(GL_TEXTURE_2D, id);

			// Le contenu source est-il seulement NON VIDE ?
			//
			// Une texture entierement noire a l'ecran a deux causes possibles, et
			// une seule question les separe : les octets televerses sont-ils deja a
			// zero, ou le probleme est-il cote GPU ? Un decodeur peut rendre
			// « succes » avec des dimensions valides et un contenu vide — c'est
			// precisement ce que rapporte l'atlas de police (« all zero »).
			//
			// On echantillonne plutot que de tout parcourir : une grande texture
			// couterait cher a chaque chargement.
			// ⚠️ Un echantillonnage a pas FIXE ne dit RIEN d'une image creuse.
			//
			// Deux fois cette sonde a conclu « entierement noire » a tort : sur des
			// PNG detoures remplis a 5 %, puis sur un atlas de police rempli a
			// 0,06 % (mesure : 2417 points non nuls sur quatre millions, pour 720
			// glyphes parfaitement rasterises). Le pas tombait sur le vide et
			// declarait l'image morte. Deux fausses pistes ont ete suivies a cause
			// de cela.
			//
			// On parcourt donc TOUT le contenu, et on ne signale qu'un fait
			// indiscutable : pas un seul pixel visible dans toute l'image. Le cout
			// est paye une fois par texture, au chargement.
			if (rgba && w > 0u && h > 0u) {
				const usize total = static_cast<usize>(w) * static_cast<usize>(h) * 4u;
				bool aucunPixelVisible = true;
				for (usize i = 0; (i + 3u) < total && aucunPixelVisible; i += 4u) {
					// Visible = un canal de couleur non nul ET une opacite non nulle.
					if (rgba[i + 3u] != 0u && (rgba[i] != 0u || rgba[i + 1u] != 0u || rgba[i + 2u] != 0u)) {
						aucunPixelVisible = false;
					}
				}
				if (aucunPixelVisible) {
					logger.Warnf("[NkGL2D] texture %ux%u : AUCUN pixel visible dans toute l'image\n", w, h);
				}
			}

			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			NK_GL2D_LOG("CreateGLTexture w=%u h=%u rgba=%p -> id=%u", w, h, (const void *)rgba, (uint32)id);
			return (uint32)id;
		}

		void NkOpenGLRenderer2D::UpdateGLTexture(uint32 id, uint32 x, uint32 y, uint32 w, uint32 h, const uint8 *rgba) {
			NK_GL2D_LOG("UpdateGLTexture id=%u rect=(%u,%u,%u,%u)", id, x, y, w, h);
			glBindTexture(GL_TEXTURE_2D, (GLuint)id);
			glTexSubImage2D(GL_TEXTURE_2D, 0, (GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h, GL_RGBA, GL_UNSIGNED_BYTE,
							rgba);
		}

		void NkOpenGLRenderer2D::DeleteGLTexture(uint32 id) {
			NK_GL2D_LOG("DeleteGLTexture id=%u", id);
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
			if (wrap == NkTextureWrap::NK_REPEAT)
				gl = GL_REPEAT;
			else if (wrap == NkTextureWrap::NK_MIRROR_REPEAT)
				gl = GL_MIRRORED_REPEAT;
			glBindTexture(GL_TEXTURE_2D, (GLuint)id);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl);
		}

		// =====================================================================
		// NkShader dispatch — OpenGL (GLSL vert + frag, programme linke).
		// =====================================================================

		static GLuint CompileGLStage(GLenum kind, const char *src) {
			if (!src)
				return 0;
			GLuint sh = glCreateShader(kind);
			glShaderSource(sh, 1, &src, nullptr);
			glCompileShader(sh);
			GLint ok = 0;
			glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
			if (!ok) {
				char log[1024]{};
				glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
				NK_GL2D_ERR("NkShader compile (%s) failed: %s", kind == GL_VERTEX_SHADER ? "VS" : "FS", log);
				glDeleteShader(sh);
				return 0;
			}
			return sh;
		}

		uint32 NkOpenGLRenderer2D::CreateGLShader(const NkShaderSources &sources) {
			NK_GL2D_LOG("CreateGLShader (glslVert=%p, glslFrag=%p)", (const void *)sources.glslVert,
						(const void *)sources.glslFrag);
			if (!sources.glslVert || !sources.glslFrag) {
				NK_GL2D_ERR("NkShader OpenGL: GLSL sources manquantes (use SetSourceGLSL)");
				return 0;
			}
			GLuint vs = CompileGLStage(GL_VERTEX_SHADER, sources.glslVert);
			if (!vs)
				return 0;
			GLuint fs = CompileGLStage(GL_FRAGMENT_SHADER, sources.glslFrag);
			if (!fs) {
				glDeleteShader(vs);
				return 0;
			}
			GLuint prog = glCreateProgram();
			glAttachShader(prog, vs);
			glAttachShader(prog, fs);
			glLinkProgram(prog);
			GLint ok = 0;
			glGetProgramiv(prog, GL_LINK_STATUS, &ok);
			// Shaders attaches deletable apres link (le programme garde une ref).
			glDeleteShader(vs);
			glDeleteShader(fs);
			if (!ok) {
				char log[1024]{};
				glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
				NK_GL2D_ERR("NkShader OpenGL link failed: %s", log);
				glDeleteProgram(prog);
				return 0;
			}
			NK_GL2D_LOG("CreateGLShader OK -> program=%u", (uint32)prog);
			return (uint32)prog;
		}

		void NkOpenGLRenderer2D::DestroyGLShader(uint32 id) {
			NK_GL2D_LOG("DestroyGLShader id=%u", id);
			if (id)
				glDeleteProgram((GLuint)id);
		}

		void NkOpenGLRenderer2D::UseGLShader(uint32 id) {
			// id=0 => unbind (revient au programme par defaut du renderer au
			// prochain SubmitBatches qui re-glUseProgram(mProgram)).
			glUseProgram((GLuint)id);
		}

		// Sur les plateformes a fonctions GL liees directement (GLES : Android/iOS/
		// Web/HarmonyOS), les symboles gl* sont des FONCTIONS (adresse toujours non
		// nulle) -> un garde "if (glXxx)" declenche -Wpointer-bool-conversion. Sur
		// desktop (glad), ce sont des POINTEURS resolus dynamiquement qu'il faut
		// verifier. NK_GL_AVAIL encapsule cette difference proprement.
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_IOS) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN) ||   \
	defined(NKENTSEU_PLATFORM_HARMONYOS)
#define NK_GL_AVAIL(fn) (true)
#else
#define NK_GL_AVAIL(fn) ((fn) != nullptr)
#endif

		void NkOpenGLRenderer2D::SetGLShaderFloat(uint32 id, const char *name, float v) {
			if (!id)
				return;
			glUseProgram((GLuint)id);
			GLint loc = glGetUniformLocation((GLuint)id, name);
			if (loc >= 0 && NK_GL_AVAIL(glUniform1f))
				glUniform1f(loc, v);
		}

		void NkOpenGLRenderer2D::SetGLShaderVec2(uint32 id, const char *name, float x, float y) {
			if (!id)
				return;
			glUseProgram((GLuint)id);
			GLint loc = glGetUniformLocation((GLuint)id, name);
			if (loc >= 0 && NK_GL_AVAIL(glUniform2f))
				glUniform2f(loc, x, y);
		}

		void NkOpenGLRenderer2D::SetGLShaderVec3(uint32 id, const char *name, float x, float y, float z) {
			if (!id)
				return;
			glUseProgram((GLuint)id);
			GLint loc = glGetUniformLocation((GLuint)id, name);
			if (loc >= 0 && NK_GL_AVAIL(glUniform3f))
				glUniform3f(loc, x, y, z);
		}

		void NkOpenGLRenderer2D::SetGLShaderVec4(uint32 id, const char *name, float x, float y, float z, float w) {
			if (!id)
				return;
			glUseProgram((GLuint)id);
			GLint loc = glGetUniformLocation((GLuint)id, name);
			if (loc >= 0 && NK_GL_AVAIL(glUniform4f))
				glUniform4f(loc, x, y, z, w);
		}

		void NkOpenGLRenderer2D::SetGLShaderMat4(uint32 id, const char *name, const float *mat16) {
			if (!id || !mat16)
				return;
			glUseProgram((GLuint)id);
			GLint loc = glGetUniformLocation((GLuint)id, name);
			if (loc >= 0)
				glUniformMatrix4fv(loc, 1, GL_FALSE, mat16);
		}

		void NkOpenGLRenderer2D::SetGLShaderTexture(uint32 id, const char *name, uint32 texGPUId, uint32 slot) {
			if (!id)
				return;
			glUseProgram((GLuint)id);
			GLint loc = glGetUniformLocation((GLuint)id, name);
			if (loc < 0)
				return;
			glActiveTexture(GL_TEXTURE0 + slot);
			glBindTexture(GL_TEXTURE_2D, (GLuint)texGPUId);
			glUniform1i(loc, (GLint)slot);
		}

		// =====================================================================
		// NkRenderTexture dispatch — OpenGL FBO + color attachment GL_RGBA8.
		//
		// On stocke {fboName, colorTexName} dans une petite registry pour ne
		// retourner qu'un uint32 single-handle a l'utilisateur (l'autre id est
		// recupere quand on dispatch). Pour eviter une map STL, on alloue de
		// facon monotonique et on garde une table fixe (32 slots) — suffisant
		// pour les usages NKCanvas (1-2 render targets typiquement).
		// =====================================================================

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif

		namespace {
			struct GLFBOEntry {
					GLuint fbo = 0;
					GLuint colorTex = 0;
					bool inUse = false;
			};

			static GLFBOEntry sFBOs[32]{};
			static GLuint sSavedFBO = 0; // pour restore au Unbind
		} // namespace

		uint32 NkOpenGLRenderer2D::CreateGLRenderTexture(uint32 w, uint32 h) {
			NK_GL2D_LOG("CreateGLRenderTexture w=%u h=%u", w, h);
			if (!NK_GL_AVAIL(glGenFramebuffers) || !NK_GL_AVAIL(glBindFramebuffer) ||
				!NK_GL_AVAIL(glFramebufferTexture2D) || !NK_GL_AVAIL(glCheckFramebufferStatus)) {
				NK_GL2D_ERR("CreateGLRenderTexture : procs FBO non charges");
				return 0;
			}
			int slot = -1;
			for (int i = 0; i < 32; ++i)
				if (!sFBOs[i].inUse) {
					slot = i;
					break;
				}
			if (slot < 0) {
				NK_GL2D_ERR("CreateGLRenderTexture : 32 slots satures");
				return 0;
			}

			GLuint fbo = 0;
			glGenFramebuffers(1, &fbo);
			if (!fbo)
				return 0;

			GLuint tex = 0;
			glGenTextures(1, &tex);
			glBindTexture(GL_TEXTURE_2D, tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glBindFramebuffer(GL_FRAMEBUFFER, fbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
			GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			if (status != GL_FRAMEBUFFER_COMPLETE) {
				NK_GL2D_ERR("NkRenderTexture FBO incomplete (status=0x%X)", (uint32)status);
				glDeleteFramebuffers(1, &fbo);
				glDeleteTextures(1, &tex);
				return 0;
			}
			sFBOs[slot] = {fbo, tex, true};
			const uint32 handle = (uint32)(slot + 1);
			NK_GL2D_LOG("CreateGLRenderTexture OK -> handle=%u (fbo=%u colorTex=%u)", handle, (uint32)fbo, (uint32)tex);
			return handle; // 0 reserve = invalide
		}

		void NkOpenGLRenderer2D::DestroyGLRenderTexture(uint32 handle) {
			NK_GL2D_LOG("DestroyGLRenderTexture handle=%u", handle);
			if (handle == 0 || handle > 32)
				return;
			const int slot = (int)handle - 1;
			if (!sFBOs[slot].inUse)
				return;
			if (sFBOs[slot].fbo)
				glDeleteFramebuffers(1, &sFBOs[slot].fbo);
			if (sFBOs[slot].colorTex)
				glDeleteTextures(1, &sFBOs[slot].colorTex);
			sFBOs[slot] = {};
		}

		void NkOpenGLRenderer2D::BindGLRenderTexture(uint32 handle) {
			if (handle == 0 || handle > 32 || !NK_GL_AVAIL(glBindFramebuffer))
				return;
			const int slot = (int)handle - 1;
			if (!sFBOs[slot].inUse)
				return;
			// Sauvegarde le FBO courant pour pouvoir le restorer.
			GLint cur = 0;
			glGetIntegerv(0x8CA6 /*GL_FRAMEBUFFER_BINDING*/, &cur);
			sSavedFBO = (GLuint)cur;
			glBindFramebuffer(GL_FRAMEBUFFER, sFBOs[slot].fbo);
		}

		void NkOpenGLRenderer2D::UnbindGLRenderTexture() {
			if (!NK_GL_AVAIL(glBindFramebuffer))
				return;
			glBindFramebuffer(GL_FRAMEBUFFER, sSavedFBO);
			sSavedFBO = 0;
		}

		uint32 NkOpenGLRenderer2D::GetGLRenderTextureColorId(uint32 handle) {
			if (handle == 0 || handle > 32)
				return 0;
			const int slot = (int)handle - 1;
			return sFBOs[slot].inUse ? (uint32)sFBOs[slot].colorTex : 0;
		}

	} // namespace renderer
} // namespace nkentseu