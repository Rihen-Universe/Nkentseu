// =============================================================================
// NkRHI_Device_GL.cpp — Implémentation OpenGL 4.3+ du NkIDevice
// Utilise Direct State Access (GL 4.5+) avec fallback OpenGL ES sur Android
// =============================================================================
#include "NkOpenglDevice.h"
#include "NkOpenglCommandBuffer.h"
#include "NKRHI/Core/NkGpuPolicy.h"
#include "NKLogger/NkLog.h"
#include "NKMemory/NkAllocator.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include <cmath>
#include <cstring>

// ── Contexte GLX (Linux/X11) ────────────────────────────────────────────────
// glad (via NkOpenglDevice.h) définit __gl_h_ AVANT -> <GL/glx.h> n'inclut pas
// <GL/gl.h> une 2e fois (pas de conflit avec glad). On retire ensuite les macros
// Xlib polluantes (None/Status/Success/Always) qui collisionnent avec des
// identifiants du reste de ce TU / NKRHI. True/False/Bool sont conservés (glx.h).
#if defined(NKENTSEU_WINDOWING_XLIB)
#include <cstdlib> // setenv/getenv (override version Mesa WSLg)
#include <X11/Xlib.h>
#include <glad/glx.h> // wrappers GLX glad (chargés via gladLoaderLoadGLX)
#ifdef None
#undef None
#endif
#ifdef Status
#undef Status
#endif
#ifdef Success
#undef Success
#endif
#ifdef Always
#undef Always
#endif
#endif

// ── Contexte EGL (Android/HarmonyOS) ────────────────────────────────────────
// glad/gles2.h ne peut PAS être inclus ici : son garde `__gl3_h_` entre en
// conflit avec glad/gl.h (déjà inclus par NkOpenglDevice.h, utilisé partout
// dans ce fichier via le define NK_OPENGL_ES) et déclenche un #error "already
// included". Les DEUX headers glad déclarent pourtant les MÊMES symboles
// globaux (glad_glGetIntegerv, etc. — même convention de nommage, indépendante
// du dialecte) : seule l'implémentation change selon qui est linké (gl.c sur
// desktop, gles2.c sur mobile/web). Il suffit donc de déclarer localement le
// point d'entrée du LOADER ES (gladLoadGLES2, défini dans gles2.c, qui remplit
// ces globaux) sans inclure gles2.h. glad/egl.h n'a pas ce conflit (namespace
// EGL_*/EGLxxx séparé) et peut s'inclure normalement.
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_HARMONYOS)
#include <glad/egl.h>
#include <unistd.h> // usleep (repli eglCreateWindowSurface, cf. commentaire plus bas)
extern "C" int gladLoadGLES2(GLADloadfunc load);
// Loader glad "dlopen" (ouvre libGLESv2.so directement) : NKCanvas s'en sert
// EXCLUSIVEMENT sur mobile car eglGetProcAddress ne resout PAS toujours les
// fonctions GLES2 CORE de la MEME implementation que celle liee au swapchain
// (couches d'emulation/trace). Utiliser ce loader = charger EXACTEMENT les
// memes entrypoints que Mou/Pong (qui s'affichent).
extern "C" int gladLoaderLoadGLES2(void);
#endif

// ── Contexte WebGL (Emscripten/WASM) ────────────────────────────────────────
// Meme contrainte de header que le bloc EGL ci-dessus : glad/gles2.h est
// inconciliable avec glad/gl.h deja inclus — on declare donc localement le
// loader ES (defini dans gles2.c, seul glad compile sur Web). Le contexte est
// cree via l'API HTML5 Emscripten (motif identique a NKWindow/Core/
// NkContext.cpp, branche NKENTSEU_PLATFORM_EMSCRIPTEN).
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#include <emscripten/html5.h>
extern "C" int gladLoadGLES2(GLADloadfunc load);
// NKTEMP-DIAG : a retirer — declare le setter de post-callback de gles2.c
// (gl.h ne declare que la variante GL desktop, jamais linkee sur Web).
extern "C" void gladSetGLES2PostCallback(GLADpostcallback cb);
// NKTEMP-DIAG : a retirer (fonction libre : une lambda variadique n'est pas
// convertible en pointeur de fonction variadique sous clang/wasm).
static void NkWebGladPostCallback(void *, const char *name, GLADapiproc, int, ...) {
	if (!glad_glGetError)
		return;
	GLenum err = glad_glGetError();
	if (err == GL_NO_ERROR)
		return;
	GLint prog = 0, vao = 0, fbo = 0;
	glad_glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
	glad_glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
	glad_glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
	static int sBudget = 40; // borne le spam
	if (sBudget > 0) {
		--sBudget;
		fprintf(stderr, "[WebDiag] GLERR 0x%X in %s prog=%d vao=%d fbo=%d\n", err, name, prog, vao, fbo);
	}
}
#endif

#define NK_GL_LOG(...) logger_src.Infof("[NkRHI_GL] " __VA_ARGS__)
#define NK_GL_ERR(...) logger_src.Infof("[NkRHI_GL][ERR] " __VA_ARGS__)
#define NK_GL_CHECK()                                                                                                  \
	do {                                                                                                               \
		GLenum e = glGetError();                                                                                       \
		if (e != GL_NO_ERROR)                                                                                          \
			NK_GL_ERR("GL error 0x%X at %s:%d\n", e, __FILE__, __LINE__);                                              \
	} while (0)

namespace nkentseu {

	namespace {
		// ── Debug callback OpenGL (GL_KHR_debug, core 4.3+) ─────────────────────
		// Route les messages drivers/validation vers NkLog avec mapping severity.
		// 'minSeverity' filtre via NkOpenGLRuntimeOptions::debugSeverityLevel :
		//   0 = NOTIFICATION (tout), 1 = LOW, 2 = MEDIUM (defaut), 3 = HIGH.
		static uint32 gGLDebugMinSeverity = 2;

		static void GLAPIENTRY GlDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
											   GLsizei /*length*/, const GLchar *message, const void * /*userParam*/) {
			// Mapper severity -> tier (0..3) pour filtrage.
			uint32 tier = 0;
			switch (severity) {
				case GL_DEBUG_SEVERITY_HIGH:
					tier = 3;
					break;
				case GL_DEBUG_SEVERITY_MEDIUM:
					tier = 2;
					break;
				case GL_DEBUG_SEVERITY_LOW:
					tier = 1;
					break;
				case GL_DEBUG_SEVERITY_NOTIFICATION:
					tier = 0;
					break;
				default:
					tier = 2;
					break;
			}
			if (tier < gGLDebugMinSeverity)
				return;

			const char *srcStr = "?";
			switch (source) {
				case GL_DEBUG_SOURCE_API:
					srcStr = "API";
					break;
				case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
					srcStr = "WIN";
					break;
				case GL_DEBUG_SOURCE_SHADER_COMPILER:
					srcStr = "SHD";
					break;
				case GL_DEBUG_SOURCE_THIRD_PARTY:
					srcStr = "3RD";
					break;
				case GL_DEBUG_SOURCE_APPLICATION:
					srcStr = "APP";
					break;
				case GL_DEBUG_SOURCE_OTHER:
					srcStr = "OTH";
					break;
			}
			const char *typStr = "?";
			switch (type) {
				case GL_DEBUG_TYPE_ERROR:
					typStr = "ERROR";
					break;
				case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
					typStr = "DEPR";
					break;
				case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
					typStr = "UB";
					break;
				case GL_DEBUG_TYPE_PORTABILITY:
					typStr = "PORT";
					break;
				case GL_DEBUG_TYPE_PERFORMANCE:
					typStr = "PERF";
					break;
				case GL_DEBUG_TYPE_MARKER:
					typStr = "MARK";
					break;
				case GL_DEBUG_TYPE_PUSH_GROUP:
					typStr = "PUSH";
					break;
				case GL_DEBUG_TYPE_POP_GROUP:
					typStr = "POP";
					break;
				case GL_DEBUG_TYPE_OTHER:
					typStr = "OTH";
					break;
			}
			switch (tier) {
				case 3:
					logger.Errorf("[NkRHI_GL][%s/%s/%u] %s", srcStr, typStr, id, message);
					break;
				case 2:
					logger.Warnf("[NkRHI_GL][%s/%s/%u] %s", srcStr, typStr, id, message);
					break;
				case 1:
					logger.Debugf("[NkRHI_GL][%s/%s/%u] %s", srcStr, typStr, id, message);
					break;
				default:
					logger.Tracef("[NkRHI_GL][%s/%s/%u] %s", srcStr, typStr, id, message);
					break;
			}
		}

		static void InstallGLDebugCallback(uint32 minSeverity) {
#if defined(NK_OPENGL_ES)
			// GL_KHR_debug est nominalement core en ES 3.2, mais certains pilotes ES
			// émulés (observé : goldfish/Adreno virtualisé, MEmu) annoncent le support
			// (fonctions résolues non-null par gladLoadGLES2) sans l'implémenter
			// réellement côté serveur -> glEnable(GL_DEBUG_OUTPUT)/glDebugMessageCallback
			// répondent "called unimplemented OpenGL ES API". Purement diagnostique
			// (RenderDoc/logs), sans impact sur le rendu voulu : désactivé sur ES,
			// comme NKCanvas qui n'utilise pas du tout cette API.
			(void)minSeverity;
			return;
#else
			if (!glDebugMessageCallback || !glDebugMessageControl)
				return;
			gGLDebugMinSeverity = minSeverity;
			glEnable(GL_DEBUG_OUTPUT);
			// GL_DEBUG_OUTPUT_SYNCHRONOUS force le driver a vider sa pipeline avant
			// chaque callback — la stack du call fautif est preservee pour breakpoint
			// mais le FPS chute (2-3x). Active uniquement en build debug.
#if defined(NKENTSEU_DEBUG)
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif
			glDebugMessageCallback(GlDebugCallback, nullptr);
			// Activer tout, le filtrage fin est fait cote callback via gGLDebugMinSeverity.
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
#endif
		}
	} // namespace

#if defined(NKENTSEU_PLATFORM_WINDOWS)
	namespace {

		static PIXELFORMATDESCRIPTOR BuildFallbackPfd(const NkWGLFallbackPixelFormat &fb) {
			PIXELFORMATDESCRIPTOR pfd{};
			pfd.nSize = sizeof(pfd);
			pfd.nVersion = fb.version;
			pfd.dwFlags = static_cast<DWORD>(fb.flags);
			pfd.iPixelType = (fb.pixelType == NkPFDPixelType::NK_PFD_PIXEL_RGBA) ? PFD_TYPE_RGBA : PFD_TYPE_COLORINDEX;
			pfd.cColorBits = fb.colorBits;
			pfd.cAlphaBits = fb.alphaBits;
			pfd.cDepthBits = fb.depthBits;
			pfd.cStencilBits = fb.stencilBits;
			pfd.cAccumBits = fb.accumBits;
			pfd.cAuxBuffers = fb.auxBuffers;
			pfd.iLayerType = PFD_MAIN_PLANE;
			return pfd;
		}

		static void *NkOpenGLGetProcAddressCompat(const char *name) {
			if (!name)
				return nullptr;
			void *proc = reinterpret_cast<void *>(wglGetProcAddress(name));
			if (proc && proc != reinterpret_cast<void *>(0x1) && proc != reinterpret_cast<void *>(0x2) &&
				proc != reinterpret_cast<void *>(0x3) && proc != reinterpret_cast<void *>(-1)) {
				return proc;
			}
			static HMODULE opengl32 = GetModuleHandleA("opengl32.dll");
			return opengl32 ? reinterpret_cast<void *>(GetProcAddress(opengl32, name)) : nullptr;
		}

	} // namespace
#endif

#if defined(NKENTSEU_WINDOWING_XLIB)
	namespace {
		// Loader glad pour GLX : glXGetProcAddressARB(const GLubyte*) -> GLADapiproc.
		static GLADapiproc NkGlxGetProcCompat(const char *name) {
			if (!name)
				return nullptr;
			return reinterpret_cast<GLADapiproc>(glXGetProcAddressARB(reinterpret_cast<const GLubyte *>(name)));
		}

		// glXCreateContextAttribsARB lève des ERREURS X ASYNCHRONES (ex. GLXBadFBConfig
		// si la version/profil demandé n'est pas supporté par le FBConfig, fréquent sur
		// WSLg/Mesa). Le handler X par défaut TUE le process -> notre fallback (4.3) ne
		// s'exécute jamais. On installe un handler qui note l'erreur au lieu de crasher.
		static int gGlxCtxErr = 0;

		static int NkGlxCtxErrorHandler(Display *, XErrorEvent *) {
			gGlxCtxErr = 1;
			return 0;
		}
	} // namespace
#endif

	NkOpenGLDevice::~NkOpenGLDevice() {
		if (mIsValid)
			Shutdown();
	}

	// =============================================================================
	bool NkOpenGLDevice::Initialize(const NkDeviceInitInfo &init) {
		mInit = init;

		NkGpuPolicy::ApplyPreContext(mInit.context);

#if defined(NKENTSEU_PLATFORM_WINDOWS)
		mNativeHwnd = init.surface.hwnd;
		if (!mNativeHwnd) {
			// Headless / compute-only : OpenGL exige un contexte lié à une surface, on
			// crée donc une fenêtre CACHÉE 1x1 (jamais affichée, classe STATIC built-in)
			// juste pour obtenir un HDC. C'est la voie "compute GL sans fenêtre visible"
			// (équivalent WGL du pbuffer / EGL surfaceless).
			mNativeHwnd = CreateWindowExW(0, L"STATIC", L"NkGLHeadless", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr,
										  GetModuleHandleW(nullptr), nullptr);
			if (!mNativeHwnd) {
				NK_GL_ERR("HWND absent et création fenêtre cachée échouée\n");
				return false;
			}
			mOwnsHeadlessWindow = true;
			NK_GL_LOG("Mode headless : fenêtre cachée créée pour le contexte GL compute\n");
		}

		mNativeHdc = GetDC(mNativeHwnd);
		if (!mNativeHdc) {
			NK_GL_ERR("GetDC failed for window handle\n");
			return false;
		}

		if (GetPixelFormat(mNativeHdc) == 0) {
			const PIXELFORMATDESCRIPTOR pfd = BuildFallbackPfd(init.context.opengl.wglFallback);
			const int pixelFormat = ChoosePixelFormat(mNativeHdc, &pfd);
			if (pixelFormat == 0 || !SetPixelFormat(mNativeHdc, pixelFormat, &pfd)) {
				NK_GL_ERR("Failed to setup WGL pixel format\n");
				ReleaseDC(mNativeHwnd, mNativeHdc);
				mNativeHdc = nullptr;
				return false;
			}
		}

		// Etape 1 — Dummy context pour charger wglCreateContextAttribsARB.
		// wglCreateContext seul produit un contexte Compatibility legacy non capturable
		// par RenderDoc. On a besoin d'un Core profile + Debug bit explicite, qui
		// n'est accessible que via wglCreateContextAttribsARB (extension ARB).
		HGLRC dummyCtx = wglCreateContext(mNativeHdc);
		if (!dummyCtx || !wglMakeCurrent(mNativeHdc, dummyCtx)) {
			NK_GL_ERR("Bootstrap wglCreateContext failed\n");
			if (dummyCtx)
				wglDeleteContext(dummyCtx);
			ReleaseDC(mNativeHwnd, mNativeHdc);
			mNativeHdc = nullptr;
			return false;
		}

		// Etape 2 — Resolve wglCreateContextAttribsARB depuis le dummy.
		typedef HGLRC(WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int *);
		PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB_ =
			(PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

		if (!wglCreateContextAttribsARB_) {
			NK_GL_ERR("wglCreateContextAttribsARB introuvable — driver trop ancien?\n");
			wglMakeCurrent(nullptr, nullptr);
			wglDeleteContext(dummyCtx);
			ReleaseDC(mNativeHwnd, mNativeHdc);
			mNativeHdc = nullptr;
			return false;
		}

		// Etape 3 — Vrai contexte avec attribs Core 4.6 + Debug + ForwardCompat.
		// Ces constantes ARB ne sont pas dans <gl/GL.h> standard ; on les définit
		// localement (mêmes valeurs que NKCanvas/Backend/OpenGL/NkOpenGLContext.cpp).
		constexpr int NK_WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091;
		constexpr int NK_WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092;
		constexpr int NK_WGL_CONTEXT_FLAGS_ARB = 0x2094;
		constexpr int NK_WGL_CONTEXT_PROFILE_MASK_ARB = 0x9126;
		constexpr int NK_WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x00000001;
		constexpr int NK_WGL_CONTEXT_DEBUG_BIT_ARB = 0x00000001;
		constexpr int NK_WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB = 0x00000002;

		const int ctxAttribs[] = {NK_WGL_CONTEXT_MAJOR_VERSION_ARB,
								  4,
								  NK_WGL_CONTEXT_MINOR_VERSION_ARB,
								  6,
								  NK_WGL_CONTEXT_PROFILE_MASK_ARB,
								  NK_WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
								  NK_WGL_CONTEXT_FLAGS_ARB,
								  NK_WGL_CONTEXT_DEBUG_BIT_ARB | NK_WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
								  0};

		mNativeGlrc = wglCreateContextAttribsARB_(mNativeHdc, nullptr, ctxAttribs);
		if (!mNativeGlrc) {
			NK_GL_ERR("wglCreateContextAttribsARB failed (Core 4.6 + Debug)\n");
			wglMakeCurrent(nullptr, nullptr);
			wglDeleteContext(dummyCtx);
			ReleaseDC(mNativeHwnd, mNativeHdc);
			mNativeHdc = nullptr;
			return false;
		}

		// Switch sur le vrai contexte et détruit le dummy.
		wglMakeCurrent(nullptr, nullptr);
		wglDeleteContext(dummyCtx);
		if (!wglMakeCurrent(mNativeHdc, mNativeGlrc)) {
			NK_GL_ERR("wglMakeCurrent sur Core context failed\n");
			wglDeleteContext(mNativeGlrc);
			mNativeGlrc = nullptr;
			ReleaseDC(mNativeHwnd, mNativeHdc);
			mNativeHdc = nullptr;
			return false;
		}

#ifndef NK_NO_GLAD2
		if (!gladLoadGL((GLADloadfunc)NkOpenGLGetProcAddressCompat)) {
			NK_GL_ERR("gladLoadGL failed\n");
			wglMakeCurrent(nullptr, nullptr);
			wglDeleteContext(mNativeGlrc);
			mNativeGlrc = nullptr;
			ReleaseDC(mNativeHwnd, mNativeHdc);
			mNativeHdc = nullptr;
			return false;
		}
#endif

		mWglSwapIntervalExt = reinterpret_cast<BOOL(WINAPI *)(int)>(NkOpenGLGetProcAddressCompat("wglSwapIntervalEXT"));
		if (mWglSwapIntervalExt) {
			mWglSwapIntervalExt(static_cast<int>(init.context.opengl.swapInterval));
		}

#elif defined(NKENTSEU_WINDOWING_XLIB)
		// ── Contexte GLX (Linux/X11) ──────────────────────────────────────────
		// init.surface porte Display*/Window/screen (rempli par le backend NKWindow
		// XLIB). Le RHI crée SON contexte GLX Core 4.3+ sur cette fenêtre, comme le
		// fait le chemin WGL sous Windows, puis charge glad via glXGetProcAddressARB.
		Display *dpy = init.surface.display;
		::Window win = init.surface.window;
		if (!dpy || !win) {
			NK_GL_ERR("X11 Display/Window manquant dans NkDeviceInitInfo.surface\n");
			return false;
		}
		const int screen = init.surface.screen;

		// WSLg (et certains Mesa) rapportent GL 4.2 PAR DÉFAUT alors que le driver réel
		// (ex. D3D12/RTX) supporte 4.6 — or le moteur exige 4.3 (compute). On force le
		// niveau rapporté AVANT toute création de contexte, SANS écraser une valeur déjà
		// posée par l'utilisateur (overwrite=0). N'affecte QUE les drivers Mesa (ignoré
		// par NVIDIA/AMD propriétaires, où 4.6 est déjà natif). Désactivable en exportant
		// soi-même MESA_GL_VERSION_OVERRIDE (ex. à une autre valeur).
		setenv("MESA_GL_VERSION_OVERRIDE", "4.6", 0);
		setenv("MESA_GLSL_VERSION_OVERRIDE", "460", 0);

		// Charge les fonctions GLX de glad via son loader dlopen(libGL) — nécessaire
		// AVANT tout appel glXxxx (les wrappers glad sont NULL sinon -> segfault).
		if (!gladLoaderLoadGLX(dpy, screen)) {
			NK_GL_ERR("gladLoaderLoadGLX failed (libGL introuvable ?)\n");
			return false;
		}

		// FBConfig volontairement PERMISSIF (WSLg/Mesa software rejettent les configs
		// strictes) : pas de GLX_X_VISUAL_TYPE imposé, stencil optionnel.
		const int fbAttrs[] = {GLX_X_RENDERABLE,
							   True,
							   GLX_DRAWABLE_TYPE,
							   GLX_WINDOW_BIT,
							   GLX_RENDER_TYPE,
							   GLX_RGBA_BIT,
							   GLX_RED_SIZE,
							   8,
							   GLX_GREEN_SIZE,
							   8,
							   GLX_BLUE_SIZE,
							   8,
							   GLX_ALPHA_SIZE,
							   8,
							   GLX_DEPTH_SIZE,
							   24,
							   GLX_STENCIL_SIZE,
							   8,
							   GLX_DOUBLEBUFFER,
							   True,
							   0};
		int fbCount = 0;
		GLXFBConfig *fbs = glXChooseFBConfig(dpy, screen, fbAttrs, &fbCount);
		if (!fbs || fbCount <= 0) {
			// 2e essai encore plus permissif (sans depth/stencil imposés).
			const int fbAttrsMin[] = {GLX_RENDER_TYPE,
									  GLX_RGBA_BIT,
									  GLX_DOUBLEBUFFER,
									  True,
									  GLX_RED_SIZE,
									  8,
									  GLX_GREEN_SIZE,
									  8,
									  GLX_BLUE_SIZE,
									  8,
									  0};
			fbs = glXChooseFBConfig(dpy, screen, fbAttrsMin, &fbCount);
		}
		if (!fbs || fbCount <= 0) {
			NK_GL_ERR("glXChooseFBConfig : aucun FBConfig compatible\n");
			return false;
		}
		GLXFBConfig fbConfig = fbs[0];
		XFree(fbs);

		typedef GLXContext (*NkGlxCreateCtxProc)(Display *, GLXFBConfig, GLXContext, Bool, const int *);
		NkGlxCreateCtxProc glXCreateContextAttribsARB_ = reinterpret_cast<NkGlxCreateCtxProc>(
			glXGetProcAddressARB(reinterpret_cast<const GLubyte *>("glXCreateContextAttribsARB")));
		if (!glXCreateContextAttribsARB_) {
			NK_GL_ERR("glXCreateContextAttribsARB introuvable (driver trop ancien?)\n");
			return false;
		}

		constexpr int NK_GLX_MAJOR = 0x2091, NK_GLX_MINOR = 0x2092, NK_GLX_FLAGS = 0x2094, NK_GLX_PROFILE = 0x9126,
					  NK_GLX_CORE_BIT = 0x1, NK_GLX_DEBUG_BIT = 0x1;
		// On tente 4.6 -> 4.5 -> 4.3 Core, chaque version AVEC puis SANS le bit debug
		// (Mesa software rejette souvent les contextes debug). Le handler X rend
		// GLXBadFBConfig non-fatal pour enchaîner les fallbacks.
		int (*oldXErr)(Display *, XErrorEvent *) = XSetErrorHandler(NkGlxCtxErrorHandler);
		GLXContext glx = nullptr;
		const int kMinors[] = {6, 5, 3};
		for (int mIdx = 0; mIdx < 3 && !glx; ++mIdx) {
			for (int dbg = 1; dbg >= 0 && !glx; --dbg) {
				const int ctxAttribs[] = {NK_GLX_MAJOR,
										  4,
										  NK_GLX_MINOR,
										  kMinors[mIdx],
										  NK_GLX_PROFILE,
										  NK_GLX_CORE_BIT,
										  NK_GLX_FLAGS,
										  dbg ? NK_GLX_DEBUG_BIT : 0,
										  0};
				gGlxCtxErr = 0;
				GLXContext c = glXCreateContextAttribsARB_(dpy, fbConfig, nullptr, True, ctxAttribs);
				XSync(dpy, False);
				if (c && !gGlxCtxErr)
					glx = c;
				else if (c)
					glXDestroyContext(dpy, c);
			}
		}
		// Dernier recours : contexte sans contrainte de version/profil (le driver donne
		// son max ; on vérifie ensuite qu'on a bien >= 4.3 plus bas).
		if (!glx) {
			gGlxCtxErr = 0;
			GLXContext c = glXCreateNewContext(dpy, fbConfig, GLX_RGBA_TYPE, nullptr, True);
			XSync(dpy, False);
			if (c && !gGlxCtxErr)
				glx = c;
			else if (c)
				glXDestroyContext(dpy, c);
		}
		XSetErrorHandler(oldXErr);
		if (!glx) {
			NK_GL_ERR("Création contexte GLX échouée (aucun profil compatible sur ce driver)\n");
			return false;
		}

		if (!glXMakeCurrent(dpy, win, glx)) {
			NK_GL_ERR("glXMakeCurrent failed\n");
			glXDestroyContext(dpy, glx);
			return false;
		}

		mGlxDisplay = reinterpret_cast<void *>(dpy);
		mGlxWindow = static_cast<unsigned long>(win);
		mGlxContext = reinterpret_cast<void *>(glx);

#ifndef NK_NO_GLAD2
		if (!gladLoadGL(NkGlxGetProcCompat)) {
			NK_GL_ERR("gladLoadGL (GLX) failed\n");
			glXMakeCurrent(dpy, 0, nullptr);
			glXDestroyContext(dpy, glx);
			mGlxContext = nullptr;
			return false;
		}
#endif

		// VSync via GLX_EXT_swap_control (best-effort).
		{
			typedef void (*NkGlxSwapIntervalProc)(Display *, GLXDrawable, int);
			NkGlxSwapIntervalProc glXSwapIntervalEXT_ = reinterpret_cast<NkGlxSwapIntervalProc>(
				glXGetProcAddressARB(reinterpret_cast<const GLubyte *>("glXSwapIntervalEXT")));
			if (glXSwapIntervalEXT_)
				glXSwapIntervalEXT_(dpy, win, static_cast<int>(init.context.opengl.swapInterval));
		}

#elif defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_HARMONYOS)
		// ── Contexte EGL (Android/HarmonyOS) ────────────────────────────────────
		// Sans ce bloc, AUCUN pointeur de fonction glad n'était jamais chargé sur
		// mobile (aucun eglCreateContext ni gladLoadGLES2 nulle part dans NKRHI) :
		// les appels gl* suivants (glGetIntegerv, QueryCaps, etc.) sautaient à une
		// adresse 0 -> SIGSEGV fault addr 0x0 au premier lancement réel sur device.
		void *nativeWindow = nullptr;
#if defined(NKENTSEU_PLATFORM_ANDROID)
		nativeWindow = reinterpret_cast<void *>(init.surface.nativeWindow);
#else // HarmonyOS
		nativeWindow = reinterpret_cast<void *>(init.surface.ohNativeWindow);
#endif
		if (!nativeWindow) {
			NK_GL_ERR("Native window (ANativeWindow*/OHNativeWindow*) manquant dans surface\n");
			return false;
		}

		// gladLoaderLoadEGL(EGL_NO_DISPLAY) : bootstrap sans display (dlopen libEGL),
		// nécessaire car eglGetProcAddress lui-même est un pointeur glad NULL tant
		// que rien ne l'a chargé (même oeuf-poule que GLX plus haut).
		if (!gladLoaderLoadEGL(nullptr)) {
			NK_GL_ERR("gladLoaderLoadEGL(EGL_NO_DISPLAY) failed\n");
			return false;
		}

		EGLDisplay eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		if (eglDisplay == EGL_NO_DISPLAY) {
			NK_GL_ERR("eglGetDisplay failed\n");
			return false;
		}
		EGLint eglMaj = 0, eglMin = 0;
		if (!eglInitialize(eglDisplay, &eglMaj, &eglMin)) {
			NK_GL_ERR("eglInitialize failed\n");
			return false;
		}
		// Recharge glad EGL avec le display : charge les fonctions dépendantes de celui-ci.
		gladLoaderLoadEGL(eglDisplay);

		// Config : ES3 d'abord, repli ES2 (drivers/émulateurs EGL 1.4 qui n'exposent
		// pas EGL_OPENGL_ES3_BIT), puis énumération complète en dernier recours —
		// même cascade que NKCanvas (NkOpenGLContext::InitEGL, éprouvée en prod).
		EGLConfig eglConfig = nullptr;
		EGLint numCfg = 0;
		const EGLint cfgAttribs[] = {EGL_RENDERABLE_TYPE,
									 EGL_OPENGL_ES3_BIT,
									 EGL_SURFACE_TYPE,
									 EGL_WINDOW_BIT,
									 EGL_RED_SIZE,
									 8,
									 EGL_GREEN_SIZE,
									 8,
									 EGL_BLUE_SIZE,
									 8,
									 EGL_ALPHA_SIZE,
									 8,
									 EGL_DEPTH_SIZE,
									 24,
									 EGL_STENCIL_SIZE,
									 8,
									 EGL_NONE};
		eglChooseConfig(eglDisplay, cfgAttribs, &eglConfig, 1, &numCfg);
		if (numCfg == 0) {
			const EGLint fbAttribs[] = {EGL_RENDERABLE_TYPE,
										EGL_OPENGL_ES2_BIT,
										EGL_SURFACE_TYPE,
										EGL_WINDOW_BIT,
										EGL_RED_SIZE,
										8,
										EGL_GREEN_SIZE,
										8,
										EGL_BLUE_SIZE,
										8,
										EGL_DEPTH_SIZE,
										16,
										EGL_NONE};
			eglChooseConfig(eglDisplay, fbAttribs, &eglConfig, 1, &numCfg);
		}
		if (numCfg == 0) {
			EGLint total = 0;
			eglGetConfigs(eglDisplay, nullptr, 0, &total);
			if (total > 0) {
				if (total > 256)
					total = 256;
				EGLConfig all[256];
				EGLint got = 0;
				eglGetConfigs(eglDisplay, all, total, &got);
				for (EGLint i = 0; i < got; ++i) {
					EGLint st = 0, rt = 0, rs = 0, gs = 0, bs = 0;
					eglGetConfigAttrib(eglDisplay, all[i], EGL_SURFACE_TYPE, &st);
					eglGetConfigAttrib(eglDisplay, all[i], EGL_RENDERABLE_TYPE, &rt);
					eglGetConfigAttrib(eglDisplay, all[i], EGL_RED_SIZE, &rs);
					eglGetConfigAttrib(eglDisplay, all[i], EGL_GREEN_SIZE, &gs);
					eglGetConfigAttrib(eglDisplay, all[i], EGL_BLUE_SIZE, &bs);
					const bool windowOk = (st & EGL_WINDOW_BIT) != 0;
					const bool esOk = (rt & (EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT)) != 0;
					if (windowOk && esOk && rs >= 8 && gs >= 8 && bs >= 8) {
						eglConfig = all[i];
						numCfg = 1;
						break;
					}
				}
			}
		}
		if (numCfg == 0) {
			NK_GL_ERR("eglChooseConfig failed (aucune config ES compatible)\n");
			return false;
		}

		// Repli avec ré-essais : juste après le démarrage d'une NativeActivity,
		// le splash screen système (Android 12+) peut encore tenir la connexion
		// EGL sur CE MÊME ANativeWindow pendant quelques dizaines de ms ->
		// eglCreateWindowSurface échoue une fois avec EGL_BAD_ALLOC / "already
		// connected to another API" (native_window_api_connect). Le système
		// libère la fenêtre très vite ; quelques tentatives espacées suffisent
		// (comportement observé et contourné de la même façon par les moteurs
		// natifs Android usuels).
		EGLSurface eglSurface = EGL_NO_SURFACE;
		for (int attempt = 0; attempt < 10; ++attempt) {
			eglSurface = eglCreateWindowSurface(eglDisplay, eglConfig, reinterpret_cast<EGLNativeWindowType>(nativeWindow),
												nullptr);
			if (eglSurface != EGL_NO_SURFACE)
				break;
			NK_GL_LOG("eglCreateWindowSurface essai %d/10 echoue (0x%x), nouvelle tentative...\n", attempt + 1,
					  (int)eglGetError());
			usleep(50000); // 50 ms
		}
		if (eglSurface == EGL_NO_SURFACE) {
			NK_GL_ERR("eglCreateWindowSurface failed apres 10 tentatives\n");
			return false;
		}

		eglBindAPI(EGL_OPENGL_ES_API);
		const EGLint ctxAttribs3[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
		EGLContext eglContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, ctxAttribs3);
		if (eglContext == EGL_NO_CONTEXT) {
			// Repli ES2 : certains émulateurs EGL 1.4 refusent un contexte ES3
			// même sur une config qui l'annonçait.
			const EGLint ctxAttribs2[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
			eglContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, ctxAttribs2);
		}
		if (eglContext == EGL_NO_CONTEXT) {
			eglDestroySurface(eglDisplay, eglSurface);
			NK_GL_ERR("eglCreateContext failed\n");
			return false;
		}

		if (!eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
			eglDestroyContext(eglDisplay, eglContext);
			eglDestroySurface(eglDisplay, eglSurface);
			NK_GL_ERR("eglMakeCurrent failed\n");
			return false;
		}

		mEglDisplay = eglDisplay;
		mEglSurface = eglSurface;
		mEglContext = eglContext;
		mEglConfig = eglConfig;
		mEglNativeWindow = nativeWindow;
		NK_GL_LOG("EGL surface creee sur ANativeWindow=%p\n", nativeWindow);
		// DIAGNOSTIC (temporaire) : attributs REELS du config et de la surface
		// obtenus. Compare a NKCanvas (qui s'affiche) : on cherche pourquoi un swap
		// "reussi" sur cette surface n'atteint pas l'ecran (RENDER_BUFFER=SINGLE ?
		// SURFACE_TYPE sans WINDOW_BIT ? NATIVE_VISUAL_ID != format fenetre RGBA_8888 ?
		// mauvais config pris par le fallback d'enumeration ?). A retirer ensuite.
		{
			EGLint cfgId = -1, visId = -1, surfType = -1, renderable = -1, redS = -1, alphaS = -1;
			eglGetConfigAttrib(eglDisplay, eglConfig, EGL_CONFIG_ID, &cfgId);
			eglGetConfigAttrib(eglDisplay, eglConfig, EGL_NATIVE_VISUAL_ID, &visId);
			eglGetConfigAttrib(eglDisplay, eglConfig, EGL_SURFACE_TYPE, &surfType);
			eglGetConfigAttrib(eglDisplay, eglConfig, EGL_RENDERABLE_TYPE, &renderable);
			eglGetConfigAttrib(eglDisplay, eglConfig, EGL_RED_SIZE, &redS);
			eglGetConfigAttrib(eglDisplay, eglConfig, EGL_ALPHA_SIZE, &alphaS);
			EGLint renderBuf = -1, swapBehav = -1;
			eglQuerySurface(eglDisplay, eglSurface, EGL_RENDER_BUFFER, &renderBuf);
			eglQuerySurface(eglDisplay, eglSurface, EGL_SWAP_BEHAVIOR, &swapBehav);
			NK_GL_LOG("EGL cfg: id=%d visualId=%d surfType=0x%X renderable=0x%X R=%d A=%d | surf: "
					  "renderBuffer=0x%X(back=0x%X) swapBehavior=0x%X\n",
					  cfgId, visId, surfType, renderable, redS, alphaS, renderBuf, EGL_BACK_BUFFER, swapBehav);
		}

#ifndef NK_NO_GLAD2
		// Charge les entrypoints GLES via le loader dlopen (libGLESv2.so), comme
		// NKCanvas (Mou/Pong qui s'affichent). AVANT on passait par
		// gladLoadGLES2(eglGetProcAddress) : sur certains drivers (dont l'emulateur
		// MEmu) eglGetProcAddress renvoie des pointeurs vers une implementation GLES
		// distincte de celle reellement liee au swapchain -> le rendu "reussit" mais
		// le back buffer poste par eglSwapBuffers n'est jamais compose (ecran noir,
		// swap ok=1). Repli sur l'ancien chemin si le loader dlopen echoue.
		int glesVer = gladLoaderLoadGLES2();
		if (!glesVer) {
			NK_GL_LOG("gladLoaderLoadGLES2 (dlopen) failed, repli sur eglGetProcAddress\n");
			glesVer = gladLoadGLES2((GLADloadfunc)eglGetProcAddress);
		}
		if (!glesVer) {
			NK_GL_ERR("gladLoadGLES2 failed\n");
			eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
			eglDestroyContext(eglDisplay, eglContext);
			eglDestroySurface(eglDisplay, eglSurface);
			mEglContext = nullptr;
			mEglSurface = nullptr;
			return false;
		}
#endif
		eglSwapInterval(eglDisplay, static_cast<EGLint>(init.context.opengl.swapInterval));
		NK_GL_LOG("EGL OK (%d.%d)\n", (int)eglMaj, (int)eglMin);

#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
		// ── Contexte WebGL (Emscripten) ─────────────────────────────────────────
		// Sans cette branche, AUCUN contexte n'etait cree sur Web : les pointeurs
		// glad restaient nuls et le premier glGetIntegerv plantait le module WASM.
		// Motif repris de NkContext.cpp (NKWindow) : emscripten_webgl_create_context
		// + make_context_current + gladLoadGLES2(emscripten_webgl_get_proc_address).
		EmscriptenWebGLContextAttributes webglAttrs;
		emscripten_webgl_init_context_attributes(&webglAttrs);
		webglAttrs.alpha = false;
		webglAttrs.depth = true;
		webglAttrs.stencil = true;
		webglAttrs.antialias = false; // MSAA gere par le renderer (offscreen), pas par le canvas
		webglAttrs.premultipliedAlpha = false;
		webglAttrs.preserveDrawingBuffer = false;
		webglAttrs.enableExtensionsByDefault = true;
		// WebGL 2 obligatoire : le moteur exige ES 3.0+ (verification plus bas).
		// Pas de repli WebGL 1 (= ES 2.0) : il echouerait de toute facon au check.
		webglAttrs.majorVersion = 2;
		webglAttrs.minorVersion = 0;

		const char *canvasSelector = init.surface.canvasId ? init.surface.canvasId : "#canvas";
		EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webglCtx = emscripten_webgl_create_context(canvasSelector, &webglAttrs);
		if (webglCtx <= 0) {
			NK_GL_ERR("emscripten_webgl_create_context failed (canvas '%s', WebGL2 requis)\n", canvasSelector);
			return false;
		}
		if (emscripten_webgl_make_context_current(webglCtx) != EMSCRIPTEN_RESULT_SUCCESS) {
			NK_GL_ERR("emscripten_webgl_make_context_current failed\n");
			emscripten_webgl_destroy_context(webglCtx);
			return false;
		}
		mWebGLContext = static_cast<long>(webglCtx);

#ifndef NK_NO_GLAD2
		// emscripten_webgl_get_proc_address resout les symboles GLES2/3 exposes
		// par la libGL Emscripten (linkee dans le module WASM).
		if (!gladLoadGLES2((GLADloadfunc)emscripten_webgl_get_proc_address)) {
			NK_GL_ERR("gladLoadGLES2 (emscripten_webgl_get_proc_address) failed\n");
			emscripten_webgl_make_context_current(0);
			emscripten_webgl_destroy_context(webglCtx);
			mWebGLContext = 0;
			return false;
		}
#endif
		NK_GL_LOG("WebGL2 context OK (canvas '%s')\n", canvasSelector);
		// NKTEMP-DIAG : a retirer (post-callback glad custom — sur erreur GL,
		// log le nom de la fonction + programme/VAO courants via les pointeurs
		// BRUTS glad_* pour ne pas re-passer par le wrapper debug).
		gladSetGLES2PostCallback(&NkWebGladPostCallback);
#endif

		// Vérifier GL 4.3 minimum (compute shaders) ou OpenGL ES 3.1+
		GLint major = 0, minor = 0;
		glGetIntegerv(GL_MAJOR_VERSION, &major);
		glGetIntegerv(GL_MINOR_VERSION, &minor);
#if defined(NK_OPENGL_ES)
		// Sur Android, on accepte OpenGL ES 3.0+
		if (major < 3) {
			NK_GL_ERR("OpenGL ES 3.0+ required (got %d.%d)\n", major, minor);
			return false;
		}
#else
		if (major < 4 || (major == 4 && minor < 3)) {
			NK_GL_ERR("OpenGL 4.3+ required (got %d.%d)\n", major, minor);
			return false;
		}
#endif

		mWidth = NkDeviceInitWidth(init);
		mHeight = NkDeviceInitHeight(init);
		if (mWidth == 0)
			mWidth = 1280;
		if (mHeight == 0)
			mHeight = 720;

		// Debug callback OpenGL (KHR_debug, core 4.3+). Route les warnings/erreurs
		// drivers vers NkLog. Filtre selon runtime.debugSeverityLevel (0=tout,
		// 1=LOW+, 2=MEDIUM+, 3=HIGH only). No-op si extension non chargee.
		if (init.context.opengl.runtime.installDebugCallback) {
			InstallGLDebugCallback(init.context.opengl.runtime.debugSeverityLevel);
			NK_GL_LOG("Debug callback installe (severity>=%u -> NkLog)\n",
					  init.context.opengl.runtime.debugSeverityLevel);
		}

		QueryCaps();

#if !defined(NK_OPENGL_ES)
		// Aligner la plage de profondeur sur Vulkan ([0,1] au lieu du [-1,1] OpenGL par défaut).
		// Le Y est corrigé par flip_vert_y dans SPIRV-Cross pour les vertex shaders géométrie.
		// Desktop seulement : glClipControl n'existe pas en GLES et son symbole glad
		// (glad_debug_glClipControl, défini dans gl.c) n'est pas linké sur mobile/web —
		// le null-check runtime ne suffit pas, il faut aussi garder le site au link.
		if (glClipControl)
			glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
#endif

		// Créer le render pass et framebuffer swapchain virtuels
		{
			NkRenderPassDesc rpd;
			rpd.AddColor(NkAttachmentDesc::Color(mSwapchainFormat)).SetDepth(NkAttachmentDesc::Depth());
			mSwapchainRP = CreateRenderPass(rpd);
			// FBO 0 = back buffer GL — on crée un handle factice
			uint64 fbId = NextId();
			mFramebuffers[fbId] = {0, mWidth, mHeight}; // GL FBO 0
			mSwapchainFB.id = fbId;
		}

		mIsValid = true;
		NK_GL_LOG("Initialized (GL %d.%d, %s)\n", major, minor, (const char *)glGetString(GL_RENDERER));
#if defined(NK_OPENGL_ES)
		// Diagnostic temporaire (enquete ecran noir Tuto02Renderer Android) :
		// glDrawElementsBaseVertex n'est core qu'a partir de GLES 3.2 (extension
		// EXT/OES_draw_elements_base_vertex sinon) — si le driver ne negocie que
		// 3.0/3.1 et n'expose pas l'extension, tout DrawIndexed avec vtxOff!=0
		// appelait un symbole absent. On journalise ici de quoi trancher.
		const char *glslv = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
		const char *ext = (const char *)glGetString(GL_EXTENSIONS);
		bool hasBaseVtx = ext && (strstr(ext, "draw_elements_base_vertex") != nullptr);
		bool hasVAB = ext && (strstr(ext, "vertex_attrib_binding") != nullptr);
		NK_GL_LOG("ES caps: GLSL=%s draw_elements_base_vertex(ext)=%d vertex_attrib_binding(ext)=%d (3.1+ core "
				  "sinon)\n",
				  glslv ? glslv : "?", (int)hasBaseVtx, (int)hasVAB);
#endif
		return true;
	}

	void NkOpenGLDevice::Shutdown() {
		WaitIdle();
		// Détruire toutes les ressources restantes
		mBuffers.ForEach([](const uint64 &, GLBuffer &b) { glDeleteBuffers(1, &b.id); });
		mTextures.ForEach([](const uint64 &, GLTexture &t) { glDeleteTextures(1, &t.id); });
		mSamplers.ForEach([](const uint64 &, GLSampler &s) { glDeleteSamplers(1, &s.id); });
		mShaders.ForEach([](const uint64 &, GLShader &sh) { glDeleteProgram(sh.program); });
		mPipelines.ForEach([](const uint64 &, GLPipeline &p) {
			if (p.vao)
				glDeleteVertexArrays(1, &p.vao);
		});
		mFramebuffers.ForEach([](const uint64 &, GLFBO &f) {
			if (f.id != 0)
				glDeleteFramebuffers(1, &f.id);
		});
		mFences.ForEach([](const uint64 &, GLFenceObj &fn) {
			if (fn.sync)
				glDeleteSync(fn.sync);
		});
		mBuffers.Clear();
		mTextures.Clear();
		mSamplers.Clear();
		mShaders.Clear();
		mPipelines.Clear();
		mFramebuffers.Clear();
		mRenderPasses.Clear();
		mDescLayouts.Clear();
		mDescSets.Clear();
		mFences.Clear();

#if defined(NKENTSEU_PLATFORM_WINDOWS)
		if (mNativeHdc && mNativeGlrc) {
			wglMakeCurrent(nullptr, nullptr);
		}
		if (mNativeGlrc) {
			wglDeleteContext(mNativeGlrc);
			mNativeGlrc = nullptr;
		}
		if (mNativeHwnd && mNativeHdc) {
			ReleaseDC(mNativeHwnd, mNativeHdc);
			mNativeHdc = nullptr;
		}
		// Détruire la fenêtre cachée si on l'a créée nous-mêmes (headless).
		if (mOwnsHeadlessWindow && mNativeHwnd) {
			DestroyWindow(mNativeHwnd);
			mOwnsHeadlessWindow = false;
		}
		mNativeHwnd = nullptr;
#elif defined(NKENTSEU_WINDOWING_XLIB)
		if (mGlxDisplay && mGlxContext) {
			Display *dpy = reinterpret_cast<Display *>(mGlxDisplay);
			glXMakeCurrent(dpy, 0, nullptr);
			glXDestroyContext(dpy, reinterpret_cast<GLXContext>(mGlxContext));
			mGlxContext = nullptr;
		}
		// Display appartient au backend NKWindow (surface) : ne pas XCloseDisplay ici.
		mGlxDisplay = nullptr;
		mGlxWindow = 0;
#elif defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_HARMONYOS)
		if (mEglDisplay) {
			EGLDisplay dpy = reinterpret_cast<EGLDisplay>(mEglDisplay);
			eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
			if (mEglContext) {
				eglDestroyContext(dpy, reinterpret_cast<EGLContext>(mEglContext));
				mEglContext = nullptr;
			}
			if (mEglSurface) {
				eglDestroySurface(dpy, reinterpret_cast<EGLSurface>(mEglSurface));
				mEglSurface = nullptr;
			}
			// Le display EGL_DEFAULT_DISPLAY est un singleton process-wide : ne pas
			// eglTerminate ici (d'autres devices/contextes peuvent le partager).
			mEglDisplay = nullptr;
		}
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
		if (mWebGLContext) {
			emscripten_webgl_make_context_current(0);
			emscripten_webgl_destroy_context(static_cast<EMSCRIPTEN_WEBGL_CONTEXT_HANDLE>(mWebGLContext));
			mWebGLContext = 0;
		}
#endif

		mIsValid = false;
		NK_GL_LOG("Shutdown\n");
	}

	void NkOpenGLDevice::QueryCaps() {
		GLint v = 0;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &v);
		mCaps.maxTextureDim2D = (uint32)v;
		glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &v);
		mCaps.maxTextureDim3D = (uint32)v;
		glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &v);
		mCaps.maxTextureCubeSize = (uint32)v;
		glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &v);
		mCaps.maxTextureArrayLayers = (uint32)v;
		glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &v);
		mCaps.maxColorAttachments = (uint32)v;
		glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &v);
		mCaps.maxVertexAttributes = (uint32)v;
		glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &v);
		mCaps.maxUniformBufferRange = (uint32)v;
		glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &v);
		mCaps.maxStorageBufferRange = (uint32)v;
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &v);
		mCaps.maxComputeGroupSizeX = (uint32)v;
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &v);
		mCaps.maxComputeGroupSizeY = (uint32)v;
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &v);
		mCaps.maxComputeGroupSizeZ = (uint32)v;
		glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, &v);
		mCaps.maxComputeSharedMemory = (uint32)v;
		glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &v);
		mCaps.maxSamplerAnisotropy = (uint32)v;
		glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &v);
		mCaps.minUniformBufferAlign = (uint32)v;
		glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &v);
		mCaps.minStorageBufferAlign = (uint32)v;

		mCaps.computeShaders = NkDeviceInitComputeEnabledForApi(mInit, NkGraphicsApi::NK_GFX_API_OPENGL);
		mCaps.geometryShaders = true;
		mCaps.tessellationShaders = true;
		mCaps.drawIndirect = true;
		mCaps.multiViewport = true;
		mCaps.independentBlend = true;
		mCaps.timestampQueries = true;
		mCaps.textureCompressionBC = true; // sur desktop

		// MSAA support
		GLint maxS = 0;
		glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &maxS);
		mCaps.msaa2x = maxS >= 2;
		mCaps.msaa4x = maxS >= 4;
		mCaps.msaa8x = maxS >= 8;
		mCaps.msaa16x = maxS >= 16;
	}

	// =============================================================================
	// Buffers
	// =============================================================================
	NkBufferHandle NkOpenGLDevice::CreateBuffer(const NkBufferDesc &desc) {
		if (desc.sizeBytes == 0)
			return {};
		threading::NkScopedLockMutex lock(mMutex);

		GLuint id = 0;
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
		// WebGL2 : le PREMIER bind non-COPY fige la "classe" du buffer (index vs
		// autre) et tout bind ulterieur d'une autre classe est un INVALID_OPERATION
		// ("buffers bound to non ELEMENT_ARRAY_BUFFER targets can not be bound...",
		// constate — nos descs ont souvent type=NK_INDEX mais bindFlags=NONE).
		// COPY_READ/COPY_WRITE sont EXEMPTES : upload initial via COPY_WRITE, la
		// classe sera fixee par le premier bind reel (ARRAY / ELEMENT_ARRAY / UBO).
		glGenBuffers(1, &id);
		GLenum usage = ToGLBufferUsage(desc.usage, desc.bindFlags);
		const bool isIndex =
			desc.type == NkBufferType::NK_INDEX || NkHasFlag(desc.bindFlags, NkBindFlags::NK_INDEX_BUFFER);
		if (isIndex) {
			// Buffer d'INDEX : la classe "element" doit etre fixee ICI, des la
			// creation. Constate en boucle de rendu : le tout PREMIER
			// glBindBuffer(GL_ELEMENT_ARRAY_BUFFER) echouait deja en
			// INVALID_OPERATION ("buffers bound to non ELEMENT_ARRAY_BUFFER
			// targets...") -> un chemin intermediaire classait le buffer "other"
			// avant son premier bind index, et TOUS les draws indexes etaient
			// alors rejetes (scene invisible). En le liant une fois sur
			// ELEMENT_ARRAY_BUFFER a la creation, la classe est correcte et tout
			// bind fautif ulterieur devient bruyant au lieu de casser les draws.
			// PRECAUTION VAO : ELEMENT_ARRAY_BUFFER est un etat DU VAO courant —
			// on passe par le VAO 0 (defaut, jamais utilise par nos pipelines)
			// pour ne pas ecraser l'IBO d'un pipeline deja cree.
			GLint prevVAO = 0;
			glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);
			glBindVertexArray(0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)desc.sizeBytes, desc.initialData, usage);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			glBindVertexArray((GLuint)prevVAO);
		} else {
			glBindBuffer(GL_COPY_WRITE_BUFFER, id);
			glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)desc.sizeBytes, desc.initialData, usage);
			glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
		}
#elif defined(NK_OPENGL_ES)
		glGenBuffers(1, &id);
		GLenum target = (NkHasFlag(desc.bindFlags, NkBindFlags::NK_UNIFORM_BUFFER))	  ? GL_UNIFORM_BUFFER
						: (NkHasFlag(desc.bindFlags, NkBindFlags::NK_STORAGE_BUFFER)) ? GL_SHADER_STORAGE_BUFFER
						: (NkHasFlag(desc.bindFlags, NkBindFlags::NK_VERTEX_BUFFER))  ? GL_ARRAY_BUFFER
						: (NkHasFlag(desc.bindFlags, NkBindFlags::NK_INDEX_BUFFER))	  ? GL_ELEMENT_ARRAY_BUFFER
																					  : GL_COPY_READ_BUFFER; // fallback
		glBindBuffer(target, id);
		GLenum usage = ToGLBufferUsage(desc.usage, desc.bindFlags);
		glBufferData(target, (GLsizeiptr)desc.sizeBytes, desc.initialData, usage);
		glBindBuffer(target, 0);
#else
		glCreateBuffers(1, &id);
		GLenum usage = ToGLBufferUsage(desc.usage, desc.bindFlags);
		glNamedBufferData(id, (GLsizeiptr)desc.sizeBytes, desc.initialData, usage);
#endif

#if !defined(NK_OPENGL_ES)
		// Purement diagnostique (RenderDoc/débogueur) : sans impact voulu sur le
		// rendu. GL_KHR_debug est core en ES 3.2, mais certains pilotes ES émulés
		// (observé : goldfish/Adreno virtualisé) ne l'implémentent pas vraiment et
		// répondent "called unimplemented OpenGL ES API" — désactivé sur mobile/ES
		// par précaution (même choix que NKCanvas, qui n'utilise pas cette API).
		if (desc.debugName) {
			glObjectLabel(GL_BUFFER, id, -1, desc.debugName);
		}
#endif

		uint64 hid = NextId();
		mBuffers[hid] = {id, desc.sizeBytes, desc.usage, desc.bindFlags};

		NkBufferHandle h;
		h.id = hid;
		return h;
	}

	void NkOpenGLDevice::DestroyBuffer(NkBufferHandle &h) {
		threading::NkScopedLockMutex lock(mMutex);
		GLBuffer *buffer = mBuffers.Find(h.id);
		if (!buffer)
			return;
		glDeleteBuffers(1, &buffer->id);
		mBuffers.Erase(h.id);
		h.id = 0;
	}

	bool NkOpenGLDevice::WriteBuffer(NkBufferHandle buf, const void *data, uint64 size, uint64 offset) {
		threading::NkScopedLockMutex lock(mMutex);
		GLBuffer *buffer = mBuffers.Find(buf.id);
		if (!buffer || !data)
			return false;
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
		// WebGL2 : cible NEUTRE (ne fige pas la classe du buffer, cf. CreateBuffer).
		glBindBuffer(GL_COPY_WRITE_BUFFER, buffer->id);
		glBufferSubData(GL_COPY_WRITE_BUFFER, (GLintptr)offset, (GLsizeiptr)size, data);
		glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
#elif defined(NK_OPENGL_ES)
		GLenum target = (NkHasFlag(buffer->bind, NkBindFlags::NK_UNIFORM_BUFFER))	? GL_UNIFORM_BUFFER
						: (NkHasFlag(buffer->bind, NkBindFlags::NK_STORAGE_BUFFER)) ? GL_SHADER_STORAGE_BUFFER
						: (NkHasFlag(buffer->bind, NkBindFlags::NK_VERTEX_BUFFER))	? GL_ARRAY_BUFFER
																					: GL_ELEMENT_ARRAY_BUFFER;
		glBindBuffer(target, buffer->id);
		glBufferSubData(target, (GLintptr)offset, (GLsizeiptr)size, data);
		glBindBuffer(target, 0);
#else
		glNamedBufferSubData(buffer->id, (GLintptr)offset, (GLsizeiptr)size, data);
#endif
		return true;
	}

	bool NkOpenGLDevice::WriteBufferAsync(NkBufferHandle buf, const void *data, uint64 size, uint64 offset) {
		threading::NkScopedLockMutex lock(mMutex);
		GLBuffer *buffer = mBuffers.Find(buf.id);
		if (!buffer || !data)
			return false;
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
		// WebGL2 : cible neutre + pas de vrai map (emulation Emscripten) — un
		// simple glBufferSubData est equivalent et evite la classe de cible.
		glBindBuffer(GL_COPY_WRITE_BUFFER, buffer->id);
		glBufferSubData(GL_COPY_WRITE_BUFFER, (GLintptr)offset, (GLsizeiptr)size, data);
		glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
#elif defined(NK_OPENGL_ES)
		GLenum target = (NkHasFlag(buffer->bind, NkBindFlags::NK_UNIFORM_BUFFER))	? GL_UNIFORM_BUFFER
						: (NkHasFlag(buffer->bind, NkBindFlags::NK_STORAGE_BUFFER)) ? GL_SHADER_STORAGE_BUFFER
						: (NkHasFlag(buffer->bind, NkBindFlags::NK_VERTEX_BUFFER))	? GL_ARRAY_BUFFER
																					: GL_ELEMENT_ARRAY_BUFFER;
		glBindBuffer(target, buffer->id);
		void *ptr = glMapBufferRange(target, (GLintptr)offset, (GLsizeiptr)size,
									 GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
		if (!ptr) {
			glBindBuffer(target, 0);
			return false;
		}
		memcpy(ptr, data, (size_t)size);
		glUnmapBuffer(target);
		glBindBuffer(target, 0);
#else
		void *ptr = glMapNamedBufferRange(buffer->id, (GLintptr)offset, (GLsizeiptr)size,
										  GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
		if (!ptr)
			return false;
		memcpy(ptr, data, (size_t)size);
		glUnmapNamedBuffer(buffer->id);
#endif
		return true;
	}

	bool NkOpenGLDevice::ReadBuffer(NkBufferHandle buf, void *out, uint64 size, uint64 offset) {
		threading::NkScopedLockMutex lock(mMutex);
		GLBuffer *buffer = mBuffers.Find(buf.id);
		if (!buffer)
			return false;

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
		// WebGL2 : cible neutre (classe du buffer non figee, cf. CreateBuffer).
		glBindBuffer(GL_COPY_READ_BUFFER, buffer->id);
		void *ptr = glMapBufferRange(GL_COPY_READ_BUFFER, (GLintptr)offset, (GLsizeiptr)size, GL_MAP_READ_BIT);
		if (ptr) {
			memcpy(out, ptr, (size_t)size);
			glUnmapBuffer(GL_COPY_READ_BUFFER);
		}
		glBindBuffer(GL_COPY_READ_BUFFER, 0);
		return ptr != nullptr;
#elif defined(NK_OPENGL_ES)
		GLenum target = NkHasFlag(buffer->bind, NkBindFlags::NK_UNIFORM_BUFFER)	  ? GL_UNIFORM_BUFFER
						: NkHasFlag(buffer->bind, NkBindFlags::NK_STORAGE_BUFFER) ? GL_SHADER_STORAGE_BUFFER
						: NkHasFlag(buffer->bind, NkBindFlags::NK_VERTEX_BUFFER)  ? GL_ARRAY_BUFFER
																				  : GL_ELEMENT_ARRAY_BUFFER;
		glBindBuffer(target, buffer->id);
		void *ptr = glMapBufferRange(target, (GLintptr)offset, (GLsizeiptr)size, GL_MAP_READ_BIT);
		if (ptr) {
			memcpy(out, ptr, (size_t)size);
			glUnmapBuffer(target);
		}
		glBindBuffer(target, 0);
		return ptr != nullptr;
#else
		glGetNamedBufferSubData(buffer->id, (GLintptr)offset, (GLsizeiptr)size, out);
		return true;
#endif
	}

	NkMappedMemory NkOpenGLDevice::MapBuffer(NkBufferHandle buf, uint64 off, uint64 sz) {
		threading::NkScopedLockMutex lock(mMutex);
		GLBuffer *buffer = mBuffers.Find(buf.id);
		if (!buffer)
			return {};
		uint64 mapSz = sz > 0 ? sz : buffer->size - off;
		// Flags d'accès selon l'usage du buffer :
		//  - NK_READBACK : lecture CPU (capture/readback) → GL_MAP_READ_BIT.
		//  - sinon        : écriture CPU → GL_MAP_WRITE_BIT.
		// PAS de PERSISTENT/COHERENT : nos buffers sont créés par
		// glNamedBufferData (storage MUTABLE) et ces flags exigent un storage
		// immutable (glBufferStorage) → GL_INVALID_OPERATION 1282 garanti
		// (c'était le bug : MapBuffer échouait pour TOUS les buffers GL).
		const GLbitfield access =
			(buffer->usage == NkResourceUsage::NK_READBACK) ? GL_MAP_READ_BIT : GL_MAP_WRITE_BIT;
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
		// WebGL2 : cible neutre (map emule par Emscripten, upload au Unmap).
		glBindBuffer(GL_COPY_WRITE_BUFFER, buffer->id);
		void *ptr = glMapBufferRange(GL_COPY_WRITE_BUFFER, (GLintptr)off, (GLsizeiptr)mapSz, access);
		glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
		if (!ptr)
			return {};
		return {ptr, mapSz};
#elif defined(NK_OPENGL_ES)
		GLenum target = (NkHasFlag(buffer->bind, NkBindFlags::NK_UNIFORM_BUFFER))	? GL_UNIFORM_BUFFER
						: (NkHasFlag(buffer->bind, NkBindFlags::NK_STORAGE_BUFFER)) ? GL_SHADER_STORAGE_BUFFER
						: (NkHasFlag(buffer->bind, NkBindFlags::NK_VERTEX_BUFFER))	? GL_ARRAY_BUFFER
																					: GL_ELEMENT_ARRAY_BUFFER;
		glBindBuffer(target, buffer->id);
		void *ptr = glMapBufferRange(target, (GLintptr)off, (GLsizeiptr)mapSz, access);
		glBindBuffer(target, 0);
		if (!ptr)
			return {};
		return {ptr, mapSz};
#else
		void *ptr = glMapNamedBufferRange(buffer->id, (GLintptr)off, (GLsizeiptr)mapSz, access);
		// Echec -> memoire NULLE, jamais un pointeur nul accompagne d'une TAILLE
		// non nulle : l'appelant y lirait une plage qu'il croit valide.
		if (!ptr)
			return {};
		return {ptr, mapSz};
#endif
	}

	void NkOpenGLDevice::UnmapBuffer(NkBufferHandle buf) {
		GLBuffer *buffer = mBuffers.Find(buf.id);
		if (buffer) {
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
			// WebGL2 : meme cible neutre que MapBuffer (l'emulation Emscripten
			// flush l'ecriture au Unmap via glBufferSubData sur cette cible).
			glBindBuffer(GL_COPY_WRITE_BUFFER, buffer->id);
			glUnmapBuffer(GL_COPY_WRITE_BUFFER);
			glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
#elif defined(NK_OPENGL_ES)
			GLenum target = (NkHasFlag(buffer->bind, NkBindFlags::NK_UNIFORM_BUFFER))	? GL_UNIFORM_BUFFER
							: (NkHasFlag(buffer->bind, NkBindFlags::NK_STORAGE_BUFFER)) ? GL_SHADER_STORAGE_BUFFER
							: (NkHasFlag(buffer->bind, NkBindFlags::NK_VERTEX_BUFFER))	? GL_ARRAY_BUFFER
																						: GL_ELEMENT_ARRAY_BUFFER;
			glBindBuffer(target, buffer->id);
			glUnmapBuffer(target);
			glBindBuffer(target, 0);
#else
			glUnmapNamedBuffer(buffer->id);
#endif
		}
	}

	// =============================================================================
	// Textures
	// =============================================================================
	NkTextureHandle NkOpenGLDevice::CreateTexture(const NkTextureDesc &desc) {
		threading::NkScopedLockMutex lock(mMutex);
		GLuint id = 0;
		GLenum target = ToGLTextureTarget(desc.type, desc.samples);
#if defined(NK_OPENGL_ES)
		glGenTextures(1, &id);
		glBindTexture(target, id);
#else
		glCreateTextures(target, 1, &id);
#endif

		const uint32 maxDim = desc.width > desc.height ? desc.width : desc.height;
		uint32 mips = desc.mipLevels == 0 ? (uint32)(std::floor(std::log2((double)maxDim)) + 1) : desc.mipLevels;

		GLenum internal = ToGLInternalFormat(desc.format);

		switch (desc.type) {
			case NkTextureType::NK_TEX2D:
				if (desc.samples > NkSampleCount::NK_S1) {
#if defined(NK_OPENGL_ES)
					// OpenGL ES n'a pas de texture storage multisample, on utilise renderbuffer ou on ignore
					// Pour simplifier, on stocke comme texture normale
					glTexStorage2D(target, mips, internal, desc.width, desc.height);
#else
					glTextureStorage2DMultisample(id, (GLsizei)desc.samples, internal, desc.width, desc.height,
												  GL_TRUE);
#endif
				} else {
#if defined(NK_OPENGL_ES)
					glTexStorage2D(target, mips, internal, desc.width, desc.height);
#else
					glTextureStorage2D(id, mips, internal, desc.width, desc.height);
#endif
				}
				break;
			case NkTextureType::NK_TEX2D_ARRAY:
#if defined(NK_OPENGL_ES)
				glTexStorage3D(target, mips, internal, desc.width, desc.height, desc.arrayLayers);
#else
				glTextureStorage3D(id, mips, internal, desc.width, desc.height, desc.arrayLayers);
#endif
				break;
			case NkTextureType::NK_CUBE:
#if defined(NK_OPENGL_ES)
				glTexStorage2D(target, mips, internal, desc.width, desc.height);
#else
				glTextureStorage2D(id, mips, internal, desc.width, desc.height);
#endif
				break;
			case NkTextureType::NK_CUBE_ARRAY:
#if defined(NK_OPENGL_ES)
				glTexStorage3D(target, mips, internal, desc.width, desc.height, desc.arrayLayers);
#else
				glTextureStorage3D(id, mips, internal, desc.width, desc.height, desc.arrayLayers);
#endif
				break;
			case NkTextureType::NK_TEX3D:
#if defined(NK_OPENGL_ES)
				glTexStorage3D(target, mips, internal, desc.width, desc.height, desc.depth);
#else
				glTextureStorage3D(id, mips, internal, desc.width, desc.height, desc.depth);
#endif
				break;
			case NkTextureType::NK_TEX1D:
#if defined(NK_OPENGL_ES)
				// OpenGL ES ne supporte pas 1D textures
				NK_GL_ERR("1D textures not supported on OpenGL ES\n");
#else
				glTextureStorage1D(id, mips, internal, desc.width);
#endif
				break;
		}

		if (desc.initialData) {
			GLenum base = ToGLBaseFormat(desc.format), type2 = ToGLType(desc.format);
			uint32 rp = desc.rowPitch > 0 ? desc.rowPitch : desc.width * NkFormatBytesPerPixel(desc.format);
			const uint32 bpp = NkFormatBytesPerPixel(desc.format);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, rp / (bpp > 0 ? bpp : 1u));
#if defined(NK_OPENGL_ES)
			glTexSubImage2D(target, 0, 0, 0, desc.width, desc.height, base, type2, desc.initialData);
#else
			glTextureSubImage2D(id, 0, 0, 0, desc.width, desc.height, base, type2, desc.initialData);
#endif
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
			if (mips > 1) {
#if defined(NK_OPENGL_ES)
				glGenerateMipmap(target);
#else
				glGenerateTextureMipmap(id);
#endif
			}
		}

#if defined(NK_OPENGL_ES)
		glBindTexture(target, 0);
#endif

#if !defined(NK_OPENGL_ES)
		// Cf. commentaire de CreateBuffer : diagnostic seul, désactivé sur ES.
		if (desc.debugName)
			glObjectLabel(GL_TEXTURE, id, -1, desc.debugName);
#endif

		uint64 hid = NextId();
		mTextures[hid] = {id, target, desc};
		NkTextureHandle h;
		h.id = hid;
		return h;
	}

	void NkOpenGLDevice::DestroyTexture(NkTextureHandle &h) {
		threading::NkScopedLockMutex lock(mMutex);
		GLTexture *texture = mTextures.Find(h.id);
		if (!texture)
			return;
		glDeleteTextures(1, &texture->id);
		mTextures.Erase(h.id);
		h.id = 0;
	}

	bool NkOpenGLDevice::WriteTexture(NkTextureHandle t, const void *p, uint32 rp) {
		GLTexture *texture = mTextures.Find(t.id);
		if (!texture)
			return false;
		const NkTextureDesc &desc = texture->desc;
		// Phase H.6 : pour les textures 3D, ecrire les `desc.depth` slices.
		uint32 d = (desc.type == NkTextureType::NK_TEX3D) ? desc.depth : 1;
		return WriteTextureRegion(t, p, 0, 0, 0, desc.width, desc.height, d, 0, 0, rp);
	}

	bool NkOpenGLDevice::WriteTextureRegion(NkTextureHandle t, const void *pixels, uint32 x, uint32 y, uint32 z,
											uint32 w, uint32 h, uint32 d2, uint32 mip, uint32 layer, uint32 rowPitch) {
		GLTexture *texture = mTextures.Find(t.id);
		if (!texture || !pixels)
			return false;
		const NkTextureDesc &desc = texture->desc;
		GLenum base = ToGLBaseFormat(desc.format), type2 = ToGLType(desc.format);
		uint32 bpp = NkFormatBytesPerPixel(desc.format);
		uint32 rp2 = rowPitch > 0 ? rowPitch : w * bpp;
		glPixelStorei(GL_UNPACK_ROW_LENGTH, rp2 / (bpp > 0 ? bpp : 1u));
#if defined(NK_OPENGL_ES)
		glBindTexture(texture->target, texture->id);
		if (desc.type == NkTextureType::NK_TEX2D) {
			glTexSubImage2D(texture->target, (GLint)mip, (GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h, base, type2,
							pixels);
		} else if (desc.type == NkTextureType::NK_CUBE) {
			// Sur ES non-DSA : cible specifique a la face. Layer 0..5 -> +X, -X, +Y, -Y, +Z, -Z.
			GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer;
			glTexSubImage2D(face, (GLint)mip, (GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h, base, type2, pixels);
		} else if (desc.type == NkTextureType::NK_TEX3D || desc.type == NkTextureType::NK_TEX2D_ARRAY ||
				   desc.type == NkTextureType::NK_CUBE_ARRAY) {
			glTexSubImage3D(texture->target, (GLint)mip, (GLint)x, (GLint)y, (GLint)(layer + z), (GLsizei)w, (GLsizei)h,
							(GLsizei)d2, base, type2, pixels);
		}
		glBindTexture(texture->target, 0);
#else
		if (desc.type == NkTextureType::NK_TEX2D) {
			glTextureSubImage2D(texture->id, (GLint)mip, (GLint)x, (GLint)y, (GLsizei)w, (GLsizei)h, base, type2,
								pixels);
		} else if (desc.type == NkTextureType::NK_TEX3D || desc.type == NkTextureType::NK_TEX2D_ARRAY ||
				   desc.type == NkTextureType::NK_CUBE || desc.type == NkTextureType::NK_CUBE_ARRAY) {
			// En DSA, cubemap = TEX2D_ARRAY a 6 layers : on upload face N comme layer N
			// via glTextureSubImage3D. Sans ce cas, les faces n'etaient PAS uploadees
			// (bug silencieux de l'ancien code -> cubemap restait noire).
			glTextureSubImage3D(texture->id, (GLint)mip, (GLint)x, (GLint)y, (GLint)(layer + z), (GLsizei)w, (GLsizei)h,
								(GLsizei)d2, base, type2, pixels);
		}
#endif
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		return true;
	}

	bool NkOpenGLDevice::GenerateMipmaps(NkTextureHandle t, NkFilter) {
		GLTexture *texture = mTextures.Find(t.id);
		if (!texture)
			return false;
#if defined(NK_OPENGL_ES)
		glBindTexture(texture->target, texture->id);
		glGenerateMipmap(texture->target);
		glBindTexture(texture->target, 0);
#else
		glGenerateTextureMipmap(texture->id);
#endif
		return true;
	}

	// =============================================================================
	// Extension EXT_texture_filter_anisotropic
	// =============================================================================
	// Le filtrage anisotrope n'appartient PAS au coeur d'OpenGL ES ni de WebGL2 :
	// c'est une extension (GL_TEXTURE_MAX_ANISOTROPY = 0x84FE). Emettre son enum
	// quand elle est absente leve GL_INVALID_ENUM et laisse l'objet sampler (ou la
	// texture) partiellement configure cote pilote — exactement le meme piege que
	// GL_TEXTURE_LOD_BIAS deja neutralise en ES.
	//   - desktop / GLES natif : presente partout (core depuis GL 4.6) -> true,
	//     aucun changement de comportement.
	//   - WebGL2 : interrogee UNE SEULE FOIS sur le contexte courant. Chrome/
	//     SwiftShader headless ne l'expose pas, d'ou la cascade
	//     "samplerParameter: invalid parameter".
	static bool NkGLHasAnisotropicFilter() {
		#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
		static int cached = -1;
		if (cached < 0) {
			cached = 0;
			EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_get_current_context();
			if (ctx != 0 && emscripten_webgl_enable_extension(ctx, "EXT_texture_filter_anisotropic"))
				cached = 1;
		}
		return cached == 1;
		#else
		return true;
		#endif
	}

	// =============================================================================
	// Samplers
	// =============================================================================
	NkSamplerHandle NkOpenGLDevice::CreateSampler(const NkSamplerDesc &d) {
		threading::NkScopedLockMutex lock(mMutex);
		GLuint id = 0;
#if defined(NK_OPENGL_ES)
		glGenSamplers(1, &id);
#else
		glCreateSamplers(1, &id);
#endif
		glSamplerParameteri(id, GL_TEXTURE_MAG_FILTER, (GLint)ToGLFilter(d.magFilter, NkMipFilter::NK_NONE));
		glSamplerParameteri(id, GL_TEXTURE_MIN_FILTER, (GLint)ToGLFilter(d.minFilter, d.mipFilter));
		glSamplerParameteri(id, GL_TEXTURE_WRAP_S, (GLint)ToGLWrap(d.addressU));
		glSamplerParameteri(id, GL_TEXTURE_WRAP_T, (GLint)ToGLWrap(d.addressV));
		glSamplerParameteri(id, GL_TEXTURE_WRAP_R, (GLint)ToGLWrap(d.addressW));
#if !defined(NK_OPENGL_ES)
		// GL_TEXTURE_LOD_BIAS n'existe PAS en OpenGL ES (absent de ES 2.0/3.x) :
		// l'appeler leve GL_INVALID_ENUM et laisse l'objet sampler partiellement
		// initialise cote pilote.
		glSamplerParameterf(id, GL_TEXTURE_LOD_BIAS, d.mipLodBias);
#endif
		glSamplerParameterf(id, GL_TEXTURE_MIN_LOD, d.minLod);
		glSamplerParameterf(id, GL_TEXTURE_MAX_LOD, d.maxLod);
		// Extension-only : cf. NkGLHasAnisotropicFilter (absente du coeur WebGL2).
		if (d.maxAnisotropy > 1.f && NkGLHasAnisotropicFilter())
			glSamplerParameterf(id, GL_TEXTURE_MAX_ANISOTROPY, d.maxAnisotropy);
		if (d.compareEnable) {
			glSamplerParameteri(id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glSamplerParameteri(id, GL_TEXTURE_COMPARE_FUNC, (GLint)ToGLCompareOp(d.compareOp));
		}
		GLSampler s{};
		s.id = id;
		s.magFilter = (GLint)ToGLFilter(d.magFilter, NkMipFilter::NK_NONE);
		s.minFilter = (GLint)ToGLFilter(d.minFilter, d.mipFilter);
		s.wrapS = (GLint)ToGLWrap(d.addressU);
		s.wrapT = (GLint)ToGLWrap(d.addressV);
		s.wrapR = (GLint)ToGLWrap(d.addressW);
		s.minLod = d.minLod;
		s.maxLod = d.maxLod;
		s.maxAnisotropy = d.maxAnisotropy;
		s.compareEnable = d.compareEnable;
		s.compareFunc = (GLint)ToGLCompareOp(d.compareOp);
		uint64 hid = NextId();
		mSamplers[hid] = s;
		NkSamplerHandle h;
		h.id = hid;
		return h;
	}

	void NkOpenGLDevice::DestroySampler(NkSamplerHandle &h) {
		threading::NkScopedLockMutex lock(mMutex);
		GLSampler *sampler = mSamplers.Find(h.id);
		if (!sampler)
			return;
		glDeleteSamplers(1, &sampler->id);
		mSamplers.Erase(h.id);
		h.id = 0;
	}

	// =============================================================================
	// Adaptation WebGL2 (Emscripten uniquement)
	// =============================================================================
	// WebGL2 = GLSL ES 3.00 STRICT (mesure sur Chrome/ANGLE, cf. PLATEFORMES_ETAT) :
	//   - seul "#version 300 es" est accepte (310/320 es et tout profil desktop
	//     "NNN core" sont rejetes : "invalid version directive") ;
	//   - layout(binding = N) est INTERDIT partout (samplers ET blocs UBO :
	//     "invalid layout qualifier: not supported") ;
	//   - layout(location = N) n'est legal QUE sur les entrees du VS et les
	//     sorties du FS (pas sur les varyings) — le matching varying se fait
	//     alors PAR NOM (identiques cote VS/FS dans nos generateurs).
	// Or nos generateurs (NkSL codegen GLSL "#version 430 core" et SPIRV-Cross
	// ES "#version 320 es") produisent tous des bindings explicites — c'est le
	// contrat du backend GL (il ne fait AUCUN cablage par nom). Ce shim adapte
	// donc le GLSL a la volee au moment de CreateShader :
	//   1. en-tete remplace par "#version 300 es" + precisions par defaut
	//      (les samplers d'ombre/array n'ont PAS de precision par defaut en ES) ;
	//   2. qualifiers binding retires du texte, MAIS collectes (nom -> binding) ;
	//   3. qualifiers location retires des varyings (gardes sur VS in / FS out) ;
	//   4. apres glLinkProgram : les bindings collectes sont re-appliques par nom
	//      (glUniformBlockBinding pour les UBO, glUniform1i pour les samplers) —
	//      le reste du backend (unites de texture, points UBO) est inchange.
	// Zero impact hors Web : tout est sous NKENTSEU_PLATFORM_EMSCRIPTEN.
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
	namespace {

		struct NkWebGLBindingFix {
				char name[96] = {0};
				int binding = 0;
				bool isBlock = false; // true = bloc UBO, false = sampler
		};

		inline bool NkWebIsIdent(char c) {
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
		}

		// Cherche la sous-chaine `sub` dans [s,e) (strstr BORNE a la ligne :
		// strstr classique lirait au-dela et matcherait les lignes suivantes).
		const char *NkWebFindSub(const char *s, const char *e, const char *sub) {
			const size_t sl = strlen(sub);
			for (const char *p = s; p + sl <= e; ++p)
				if (strncmp(p, sub, sl) == 0)
					return p;
			return nullptr;
		}

		// Cherche le mot entier `word` dans [s,e). Retourne le pointeur ou nullptr.
		const char *NkWebFindWord(const char *s, const char *e, const char *word) {
			const size_t wl = strlen(word);
			for (const char *p = s; p + wl <= e; ++p) {
				if (strncmp(p, word, wl) != 0)
					continue;
				const bool okL = (p == s) || !NkWebIsIdent(p[-1]);
				const bool okR = (p + wl == e) || !NkWebIsIdent(p[wl]);
				if (okL && okR)
					return p;
			}
			return nullptr;
		}

		// Adapte UNE ligne de GLSL genere. Ajoute le resultat a `out` et collecte
		// les bindings retires dans `fixes`.
		void NkWebAdaptLine(const char *line, const char *lineEnd, GLenum stage, NkString &out,
							NkVector<NkWebGLBindingFix> &fixes) {
			const char *p = line;
			while (p < lineEnd && (*p == ' ' || *p == '\t'))
				++p;

			// ── 1. En-tete #version -> 300 es + precisions par defaut ────────────
			if (lineEnd - p >= 8 && strncmp(p, "#version", 8) == 0) {
				out += "#version 300 es\n";
				out += "precision highp float;\n";
				out += "precision highp int;\n";
				out += "precision highp sampler2D;\n";
				out += "precision highp sampler3D;\n";
				out += "precision highp samplerCube;\n";
				out += "precision highp sampler2DArray;\n";
				out += "precision highp sampler2DShadow;\n";
				out += "precision highp samplerCubeShadow;\n";
				out += "precision highp sampler2DArrayShadow;\n";
				out += "precision highp isampler2D;\n";
				out += "precision highp usampler2D;\n";
				return;
			}

			// ── 2. Ligne sans layout(...) : copie telle quelle ───────────────────
			const char *lay = NkWebFindWord(p, lineEnd, "layout");
			const char *open = nullptr;
			if (lay) {
				open = lay + 6;
				while (open < lineEnd && (*open == ' ' || *open == '\t'))
					++open;
				if (open >= lineEnd || *open != '(')
					lay = nullptr;
			}
			if (!lay) {
				out.Append(line, (uint32)(lineEnd - line));
				out += "\n";
				return;
			}
			const char *close = open;
			while (close < lineEnd && *close != ')')
				++close;
			if (close >= lineEnd) { // layout( non ferme sur la ligne : inattendu
				out.Append(line, (uint32)(lineEnd - line));
				out += "\n";
				return;
			}

			// Nature de la declaration (partie APRES la parenthese fermante).
			const char *rest = close + 1;
			const bool hasUniform = NkWebFindWord(rest, lineEnd, "uniform") != nullptr;
			const bool hasSampler =
				NkWebFindSub(rest, lineEnd, "sampler") != nullptr || NkWebFindSub(rest, lineEnd, "image") != nullptr;
			const bool isIn = NkWebFindWord(rest, lineEnd, "in") != nullptr;
			const bool isOut = NkWebFindWord(rest, lineEnd, "out") != nullptr;

			// ── 3. Reconstruire la liste de qualifiers sans binding/location ─────
			char kept[160] = {0};
			size_t keptLen = 0;
			int bindingVal = -1;
			const char *q = open + 1;
			while (q < close) {
				// isole un qualifier [q, qe) (separe par des virgules)
				const char *qe = q;
				while (qe < close && *qe != ',')
					++qe;
				// trim
				const char *ts = q;
				while (ts < qe && (*ts == ' ' || *ts == '\t'))
					++ts;
				const char *te = qe;
				while (te > ts && (te[-1] == ' ' || te[-1] == '\t'))
					--te;
				const size_t tl = (size_t)(te - ts);
				const bool isBinding = tl >= 7 && strncmp(ts, "binding", 7) == 0 && !NkWebIsIdent(ts[7]);
				const bool isLocation = tl >= 8 && strncmp(ts, "location", 8) == 0 && !NkWebIsIdent(ts[8]);
				// "set = N" (dialecte Vulkan, jamais legal en GL/WebGL) : retire.
				const bool isSet = tl >= 3 && strncmp(ts, "set", 3) == 0 && !NkWebIsIdent(ts[3]);
				if (isSet) {
					q = qe + 1;
					continue;
				}
				if (isBinding) {
					const char *v = ts + 7;
					while (v < te && (*v == ' ' || *v == '=' || *v == '\t'))
						++v;
					bindingVal = atoi(v);
				} else {
					// location : retiree des varyings (VS out / FS in), gardee sur
					// VS in (attributs) et FS out (cibles de rendu).
					const bool dropLocation =
						isLocation && !hasUniform &&
						((stage == GL_VERTEX_SHADER && isOut && !isIn) || (stage == GL_FRAGMENT_SHADER && isIn));
					if (!dropLocation && tl > 0 && keptLen + tl + 2 < sizeof(kept)) {
						if (keptLen) {
							kept[keptLen++] = ',';
							kept[keptLen++] = ' ';
						}
						memcpy(kept + keptLen, ts, tl);
						keptLen += tl;
						kept[keptLen] = '\0';
					}
				}
				q = qe + 1;
			}

			// ── 4. Collecte du binding retire (nom -> point de binding) ──────────
			if (bindingVal >= 0 && hasUniform) {
				NkWebGLBindingFix fix;
				fix.binding = bindingVal;
				if (hasSampler) {
					// sampler : dernier identifiant avant ';' (ignore un eventuel [N])
					fix.isBlock = false;
					const char *semi = rest;
					const char *lastSemi = nullptr;
					while ((semi = (const char *)memchr(semi, ';', (size_t)(lineEnd - semi))) != nullptr) {
						lastSemi = semi;
						++semi;
					}
					const char *ne = lastSemi ? lastSemi : lineEnd;
					while (ne > rest && !NkWebIsIdent(ne[-1]) && ne[-1] != ']')
						--ne;
					if (ne > rest && ne[-1] == ']') { // strip [N]
						while (ne > rest && ne[-1] != '[')
							--ne;
						if (ne > rest)
							--ne; // '['
					}
					const char *ns = ne;
					while (ns > rest && NkWebIsIdent(ns[-1]))
						--ns;
					const size_t nl = (size_t)(ne - ns);
					if (nl > 0 && nl < sizeof(fix.name)) {
						memcpy(fix.name, ns, nl);
						fix.name[nl] = '\0';
						fixes.PushBack(fix);
					}
				} else {
					// bloc UBO : premier identifiant apres le mot-cle "uniform"
					// (en sautant les qualifiers de precision).
					fix.isBlock = true;
					const char *u = NkWebFindWord(rest, lineEnd, "uniform");
					const char *ns = u ? u + 7 : rest;
					const char *name = nullptr;
					size_t nameLen = 0;
					while (ns < lineEnd) {
						while (ns < lineEnd && !NkWebIsIdent(*ns))
							++ns;
						const char *ne = ns;
						while (ne < lineEnd && NkWebIsIdent(*ne))
							++ne;
						const size_t nl = (size_t)(ne - ns);
						const bool isPrec = (nl == 5 && strncmp(ns, "highp", 5) == 0) ||
											(nl == 4 && strncmp(ns, "lowp", 4) == 0) ||
											(nl == 7 && strncmp(ns, "mediump", 7) == 0);
						if (nl > 0 && !isPrec) {
							name = ns;
							nameLen = nl;
							break;
						}
						ns = ne;
					}
					if (name && nameLen < sizeof(fix.name)) {
						memcpy(fix.name, name, nameLen);
						fix.name[nameLen] = '\0';
						fixes.PushBack(fix);
					}
				}
			}

			// ── 5. Re-emission de la ligne ───────────────────────────────────────
			out.Append(line, (uint32)(lay - line)); // avant "layout"
			if (keptLen) {
				out += "layout(";
				out.Append(kept, (uint32)keptLen);
				out += ") ";
			}
			// apres ')' : trim des espaces de tete pour eviter "  uniform"
			const char *tail = close + 1;
			while (tail < lineEnd && (*tail == ' ' || *tail == '\t'))
				++tail;
			out.Append(tail, (uint32)(lineEnd - tail));
			out += "\n";
		}

		// Fusion des samplers de cookies (limite WebGL2/SwiftShader) ─────────────
		// Le fragment PBR declare 24 samplers TOUS actifs (4 materiau + 3 IBL +
		// sky + voxelAO + 2 shadow + matcap + 8 cookies 2D + 4 cookies cube), or
		// SwiftShader n'accorde que MAX_TEXTURE_IMAGE_UNITS=16 au fragment ->
		// glLinkProgram echoue ("texture image units count exceeds...") et la
		// scene 3D ne peut pas s'initialiser. En GLSL seuls les samplers
		// REELLEMENT echantillonnes comptent : on redirige donc les USAGES des
		// cookies 1..7 (2D) et 1..3 (cube) vers le slot 0 — memes longueurs
		// d'identifiant, remplacement in-place — ce qui rend les samplers 1..N
		// inactifs (elimines par le compilateur) : 24 -> 14 unites. Cout visuel :
		// toutes les lumieres a cookie partagent la texture du slot 0 (aucune
		// dans les demos). Les declarations restent : leurs webFixes retombent
		// sur glGetUniformLocation == -1, ignore proprement.
		// Remap des unites de texture hautes (limite WebGL2/SwiftShader) ─────────
		// SwiftShader n'accorde que MAX_TEXTURE_IMAGE_UNITS=16 unites au fragment.
		// Or nos conventions NkSL placent des samplers aux bindings 21/26/27/28
		// (cube cookie 0, sky, voxel AO, matcap) : glUniform1i(loc, 26) est LEGAL
		// a l'appel, mais TOUT draw d'un programme dont un sampler actif pointe
		// une unite >= 16 leve GL_INVALID_OPERATION NATIF (sans message console —
		// la validation JS de WebGL passe, c'est le driver qui refuse). C'etait
		// la cause des "GLAD: ERROR 1282 in glDrawElements" en boucle : AUCUN
		// draw indexe 3D ne partait, ecran noir.
		// Table de remap GLOBALE (unites < 16), verifiee sans collision contre
		// les samplers ACTIFS de chaque programme concerne (apres fusion des
		// cookies, cf. NkWebMergeCookieSamplers) :
		//   PBR           : actifs {3..6,8..13} + 21,26,27,28 -> libres {2,7,14,15}
		//   DeferredLight : actifs {0..3,8..11,13} + 21       -> 14 libre
		//   Layered       : actifs {11,12} + 27               -> 2 libre
		//   Skybox        : actif  {} + 26                    -> 15 libre
		// Appliquee des DEUX cotes : glUniform1i au link (ci-dessous) ET
		// glActiveTexture dans ApplyDescriptors (meme .cpp) — toujours coherents.
		inline uint32 NkWebRemapTexUnit(uint32 b) {
			switch (b) {
				case 21:
					return 14;
				case 26:
					return 15;
				case 27:
					return 2;
				case 28:
					return 7;
				default:
					return b;
			}
		}

		// Trouve un identifiant cookie "tLight3D[Cube]CookieN" (N=1..9, mot entier)
		// dans [s,e). Retourne le pointeur sur le chiffre N, ou nullptr.
		char *NkWebFindCookieDigit(char *s, const char *e) {
			static const char *kPrefix2D = "tLight3DCookie";	   // + chiffre
			static const char *kPrefixCube = "tLight3DCubeCookie"; // + chiffre
			const size_t l2D = strlen(kPrefix2D), lCube = strlen(kPrefixCube);
			for (char *p = s; p + l2D + 1 <= e; ++p) {
				if (p > s && NkWebIsIdent(p[-1]))
					continue; // pas un debut de mot
				char *digit = nullptr;
				if (p + lCube + 1 <= e && strncmp(p, kPrefixCube, lCube) == 0)
					digit = p + lCube;
				else if (strncmp(p, kPrefix2D, l2D) == 0)
					digit = p + l2D;
				if (!digit)
					continue;
				if (*digit >= '1' && *digit <= '9' && (digit + 1 == e || !NkWebIsIdent(digit[1])))
					return digit;
			}
			return nullptr;
		}

		// Pre-passe : supprime les DECLARATIONS des cookies 1..N et redirige leurs
		// USAGES vers le slot 0. Opere ligne par ligne sur une copie de la source.
		NkString NkWebMergeCookieSamplers(const char *src) {
			NkString out;
			const char *p = src;
			const char *end = src + strlen(src);
			while (p < end) {
				const char *nl = (const char *)memchr(p, '\n', (size_t)(end - p));
				const char *lineEnd = nl ? nl : end;
				// Ligne de DECLARATION d'un cookie 1..N ? (uniform + sampler + ident)
				const bool isDecl = NkWebFindWord(p, lineEnd, "uniform") != nullptr &&
									NkWebFindSub(p, lineEnd, "sampler") != nullptr &&
									NkWebFindCookieDigit((char *)p, lineEnd) != nullptr;
				if (!isDecl) {
					const uint32 before = out.Length();
					out.Append(p, (uint32)(lineEnd - p));
					out += "\n";
					// Redirection in-place des usages (longueur inchangee).
					char *w = (char *)out.CStr() + before;
					char *we = (char *)out.CStr() + out.Length();
					for (char *d = NkWebFindCookieDigit(w, we); d; d = NkWebFindCookieDigit(d + 1, we))
						*d = '0';
				}
				p = nl ? nl + 1 : end;
			}
			return out;
		}

		// Adapte une source complete. Retourne la source WebGL2 et remplit `fixes`.
		NkString NkWebGL2AdaptGLSL(const char *src, GLenum stage, NkVector<NkWebGLBindingFix> &fixes) {
			// Pre-passe cookies : ramene le fragment PBR sous MAX_TEXTURE_IMAGE_UNITS
			// (cf. NkWebMergeCookieSamplers). No-op pour les shaders sans cookies.
			NkString merged = NkWebMergeCookieSamplers(src);
			src = merged.CStr();
			NkString out;
			const char *p = src;
			const char *end = src + strlen(src);
			while (p < end) {
				const char *nl = (const char *)memchr(p, '\n', (size_t)(end - p));
				const char *lineEnd = nl ? nl : end;
				// strip \r final (sources potentiellement CRLF)
				const char *le = lineEnd;
				if (le > p && le[-1] == '\r')
					--le;
				NkWebAdaptLine(p, le, stage, out, fixes);
				p = nl ? nl + 1 : end;
			}
			return out;
		}

	} // namespace
#endif // NKENTSEU_PLATFORM_EMSCRIPTEN

	// =============================================================================
	// Shaders
	// =============================================================================
	GLuint NkOpenGLDevice::CompileGLStage(GLenum stage, const char *src) {
		GLuint s = glCreateShader(stage);
		glShaderSource(s, 1, &src, nullptr);
		glCompileShader(s);
		GLint ok = 0;
		glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
		if (!ok) {
			char buf[2048];
			glGetShaderInfoLog(s, 2048, nullptr, buf);
			NK_GL_ERR("Shader compile error:\n%s\n", buf);
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
			// NKTEMP-DIAG : a retirer (instrumentation blocage Web)
			fprintf(stderr, "[NkRHI_GL][WebDiag] stage=0x%X compile FAIL:\n%s\n", (unsigned)stage, buf);
			{
				const char *pp = src;
				int line = 1;
				while (pp && *pp && line <= 40) {
					const char *nl = strchr(pp, '\n');
					fprintf(stderr, "%3d| %.*s\n", line, nl ? (int)(nl - pp) : (int)strlen(pp), pp);
					pp = nl ? nl + 1 : nullptr;
					++line;
				}
			}
#endif
			glDeleteShader(s);
			return 0;
		}
		return s;
	}

	NkShaderHandle NkOpenGLDevice::CreateShader(const NkShaderDesc &desc) {
		threading::NkScopedLockMutex lock(mMutex);
		GLuint prog = glCreateProgram();
		uint32 attachedCount = 0;

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
		// WebGL2 : bindings retires du GLSL par le shim (ES 3.00), re-appliques
		// par nom apres le link (voir NkWebGL2AdaptGLSL ci-dessus).
		NkVector<NkWebGLBindingFix> webFixes;
#endif

		for (uint32 i = 0; i < desc.stages.Size(); i++) {
			auto &s = desc.stages[i];
			const char *src = s.glslSource;
			if (!src || !src[0]) {
				continue;
			}
			GLenum glStage = ToGLShaderStage(s.stage);
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
			NkString adapted = NkWebGL2AdaptGLSL(src, glStage, webFixes);
			src = adapted.CStr();
#endif
			GLuint sh = CompileGLStage(glStage, src);
			if (sh) {
				glAttachShader(prog, sh);
				glDeleteShader(sh);
				++attachedCount;
			}
		}

		if (attachedCount == 0) {
			NK_GL_ERR("GL shader: no GLSL stage provided\n");
			glDeleteProgram(prog);
			return {};
		}

		glLinkProgram(prog);
		GLint ok = 0;
		glGetProgramiv(prog, GL_LINK_STATUS, &ok);
		if (!ok) {
			char buf[2048];
			glGetProgramInfoLog(prog, 2048, nullptr, buf);
			NK_GL_ERR("Shader link error:\n%s\n", buf);
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
			// NKTEMP-DIAG : a retirer (instrumentation blocage Web)
			fprintf(stderr, "[NkRHI_GL][WebDiag] link FAIL:\n%s\n", buf);
#endif
			glDeleteProgram(prog);
			return {};
		}

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
		// Re-application PAR NOM des bindings retires du GLSL (WebGL2 ne permet
		// pas layout(binding=N)). Blocs UBO -> glUniformBlockBinding ; samplers
		// -> glUniform1i (necessite le programme courant : sauve/restaure).
		if (!webFixes.Empty()) {
			GLint prevProg = 0;
			glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
			glUseProgram(prog);
			for (uint32 i = 0; i < webFixes.Size(); i++) {
				const NkWebGLBindingFix &f = webFixes[i];
				if (f.isBlock) {
					const GLuint idx = glGetUniformBlockIndex(prog, f.name);
					if (idx != GL_INVALID_INDEX)
						glUniformBlockBinding(prog, idx, (GLuint)f.binding);
				} else {
					const GLint loc = glGetUniformLocation(prog, f.name);
					// Unite remappee sous MAX_TEXTURE_IMAGE_UNITS (cf.
					// NkWebRemapTexUnit) — un sampler actif pointant une unite
					// >= 16 rendrait TOUS les draws du programme invalides.
					if (loc >= 0)
						glUniform1i(loc, (GLint)NkWebRemapTexUnit((uint32)f.binding));
					// NKTEMP-DIAG : a retirer (assignations d'unites)
					fprintf(stderr, "[WebDiag] prog=%u '%s' assign '%s' bind=%d unit=%u loc=%d\n", prog,
							desc.debugName ? desc.debugName : "?", f.name, f.binding,
							NkWebRemapTexUnit((uint32)f.binding), loc);
				}
			}
			glUseProgram((GLuint)prevProg);
		}
		// NKTEMP-DIAG : a retirer (instrumentation samplers/unites WebGL2).
		// Dump de TOUS les uniforms sampler actifs du programme et de l'unite
		// qui leur est reellement assignee apres la re-application.
		{
			GLint uniformCount = 0;
			glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &uniformCount);
			for (GLint u = 0; u < uniformCount; ++u) {
				char uname[128] = {0};
				GLint usize = 0;
				GLenum utype = 0;
				glGetActiveUniform(prog, (GLuint)u, 127, nullptr, &usize, &utype, uname);
				const bool isSampler =
					utype == GL_SAMPLER_2D || utype == GL_SAMPLER_3D || utype == GL_SAMPLER_CUBE ||
					utype == GL_SAMPLER_2D_SHADOW || utype == GL_SAMPLER_2D_ARRAY ||
					utype == GL_SAMPLER_2D_ARRAY_SHADOW || utype == GL_SAMPLER_CUBE_SHADOW ||
					utype == GL_INT_SAMPLER_2D || utype == GL_UNSIGNED_INT_SAMPLER_2D;
				if (!isSampler)
					continue;
				fprintf(stderr, "[WebDiag] prog=%u '%s' active sampler '%s' type=0x%X\n", prog,
						desc.debugName ? desc.debugName : "?", uname, utype);
			}
		}
#endif
#if !defined(NK_OPENGL_ES)
		// Cf. commentaire de CreateBuffer : diagnostic seul, désactivé sur ES.
		if (desc.debugName)
			glObjectLabel(GL_PROGRAM, prog, -1, desc.debugName);
#endif

		uint64 hid = NextId();
		mShaders[hid] = {prog};
		NkShaderHandle h;
		h.id = hid;
		return h;
	}

	void NkOpenGLDevice::DestroyShader(NkShaderHandle &h) {
		threading::NkScopedLockMutex lock(mMutex);
		GLShader *shader = mShaders.Find(h.id);
		if (!shader)
			return;
		glDeleteProgram(shader->program);
		mShaders.Erase(h.id);
		h.id = 0;
	}

	// =============================================================================
	// Pipelines
	// =============================================================================
	NkPipelineHandle NkOpenGLDevice::CreateGraphicsPipeline(const NkGraphicsPipelineDesc &d) {
		threading::NkScopedLockMutex lock(mMutex);
		GLShader *shader = mShaders.Find(d.shader.id);
		if (!shader)
			return {};

		GLPipeline p;
		p.program = shader->program;
		p.vertexLayout = d.vertexLayout;
		p.gfxDesc = d;
		p.isCompute = false;

		// Créer le VAO correspondant au vertex layout
#if defined(NK_OPENGL_ES)
		glGenVertexArrays(1, &p.vao);
		glBindVertexArray(p.vao);
#else
		glCreateVertexArrays(1, &p.vao);
#endif
		// NkUnorderedMap<uint32, uint32> bindingStride;
		// for (const auto& b : d.vertexLayout.bindings) {
		//     bindingStride[b.binding] = b.stride;
		// }

		for (uint32 i = 0; i < d.vertexLayout.attributes.Size(); i++) {
			auto &a = d.vertexLayout.attributes[i];
			auto &b = d.vertexLayout.bindings[a.binding];
			// uint32 stride = bindingStride[a.binding]; // Stride du binding associé
			uint32 stride = b.stride; // Stride du binding associé
#if defined(NK_OPENGL_ES)
			glEnableVertexAttribArray(a.location);
#else
			glEnableVertexArrayAttrib(p.vao, a.location);
#endif
			GLint compCount = 3;
			GLenum compType = GL_FLOAT;
			GLboolean norm = GL_FALSE;
			bool isInteger = false;
			switch (a.format) {
				case NkVertexFormat::NK_R32_UINT:
					compCount = 1;
					compType = GL_UNSIGNED_INT;
					isInteger = true;
					break;
				case NkVertexFormat::NK_RG32_UINT:
					compCount = 2;
					compType = GL_UNSIGNED_INT;
					isInteger = true;
					break;
				case NkVertexFormat::NK_RGBA32_UINT:
					compCount = 4;
					compType = GL_UNSIGNED_INT;
					isInteger = true;
					break;
				case NkVertexFormat::NK_R32_SINT:
					compCount = 1;
					compType = GL_INT;
					isInteger = true;
					break;
				case NkVertexFormat::NK_RG32_SINT:
					compCount = 2;
					compType = GL_INT;
					isInteger = true;
					break;
				case NkVertexFormat::NK_RGBA32_SINT:
					compCount = 4;
					compType = GL_INT;
					isInteger = true;
					break;
				case NkVertexFormat::NK_R32_FLOAT:
					compCount = 1;
					break;
				case NkVertexFormat::NK_RG32_FLOAT:
					compCount = 2;
					break;
				case NkVertexFormat::NK_RGB32_FLOAT:
					compCount = 3;
					break;
				case NkVertexFormat::NK_RGBA32_FLOAT:
					compCount = 4;
					break;
				case NkVertexFormat::NK_RGBA8_UNORM:
				case NkVertexFormat::NK_R8G8B8A8_UNORM_PACKED:
					compCount = 4;
					compType = GL_UNSIGNED_BYTE;
					norm = GL_TRUE;
					break;
				case NkVertexFormat::NK_RGBA8_SNORM:
					compCount = 4;
					compType = GL_BYTE;
					norm = GL_TRUE;
					break;
				case NkVertexFormat::NK_A2B10G10R10_UNORM:
					compCount = 4;
					compType = GL_UNSIGNED_INT_2_10_10_10_REV;
					norm = GL_TRUE;
					break;
				default:
					break;
			}
			// IMPORTANT (ES) : glVertexAttribPointer/IPointer classiques exigent un
			// buffer DEJA lie sur GL_ARRAY_BUFFER au moment de l'appel — hors, ici,
			// à la création du pipeline, AUCUN vertex buffer n'est encore lié (il ne
			// l'est que plus tard, par tirage, via glBindVertexBuffer). Les appeler
			// ici capturait `offset` comme un POINTEUR CPU brut invalide dans le VAO
			// -> GL_INVALID_OPERATION (0x502) puis crash au premier draw. On utilise
			// donc le modele "vertex attrib binding" (ES 3.1+, disponible : nos
			// contextes ES sont créés en 3.2) : Format = géométrie de l'attribut
			// SEULE (sans buffer), Binding = association attribut<->binding index,
			// et c'est glBindVertexBuffer (déjà correct, cf. NkOpenglCommandBuffer)
			// qui fournit le buffer réel à chaque tirage. Exactement le pendant non-DSA
			// du chemin desktop (glVertexArrayAttribFormat/Binding) juste en-dessous.
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
			// WebGL2 = ES 3.0 : le modele "vertex attrib binding" (glVertexAttribFormat/
			// Binding/BindingDivisor, ES 3.1+) N'EXISTE PAS — les pointeurs glad sont
			// NULS ("GLAD: ERROR glVertexAttribFormat is NULL!" puis RuntimeError:
			// null function, constate sous Chrome headless). Sur Web, le format des
			// attributs est donc applique AU MOMENT DU BIND du vertex buffer, via
			// glVertexAttrib(I)Pointer classiques + glVertexAttribDivisor (tous ES 3.0),
			// dans NkOpenglWebBindVertexBuffer (appelee par GL_BindVertexBuffer).
			// Ici on a seulement active l'attribut dans le VAO du pipeline.
			(void)compCount;
			(void)compType;
			(void)norm;
			(void)isInteger;
		}
#else
			if (isInteger) {
#if defined(NK_OPENGL_ES)
				glVertexAttribIFormat(a.location, compCount, compType, (GLuint)a.offset);
#else
				glVertexArrayAttribIFormat(p.vao, a.location, compCount, compType, (GLuint)a.offset);
#endif
			} else {
#if defined(NK_OPENGL_ES)
				glVertexAttribFormat(a.location, compCount, compType, norm, (GLuint)a.offset);
#else
				glVertexArrayAttribFormat(p.vao, a.location, compCount, compType, norm, (GLuint)a.offset);
#endif
			}
#if defined(NK_OPENGL_ES)
			glVertexAttribBinding(a.location, a.binding);
#else
			glVertexArrayAttribBinding(p.vao, a.location, a.binding);
#endif
		}
		for (uint32 i = 0; i < d.vertexLayout.bindings.Size(); i++) {
			auto &b = d.vertexLayout.bindings[i];
			if (b.perInstance) {
#if defined(NK_OPENGL_ES)
				glVertexBindingDivisor(b.binding, 1);
#else
				glVertexArrayBindingDivisor(p.vao, b.binding, 1);
#endif
			}
		}
#endif // NKENTSEU_PLATFORM_EMSCRIPTEN
#if defined(NK_OPENGL_ES)
		glBindVertexArray(0);
#endif

		uint64 hid = NextId();
		mPipelines[hid] = p;
		NkPipelineHandle h;
		h.id = hid;
		return h;
	}

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
	// =============================================================================
	// WebGL2 : application du vertex layout au bind du buffer (ES 3.0 pur)
	// =============================================================================
	// Pendant Web de glBindVertexBuffer : WebGL2 n'a pas le modele attrib-binding
	// (ES 3.1). On applique donc, pour chaque attribut du binding, le pointeur
	// classique (buffer ARRAY_BUFFER lie + glVertexAttrib(I)Pointer + divisor).
	// Le VAO du pipeline courant est deja lie (GL_BindPipeline) : l'etat est
	// capture dedans, comme sur les autres chemins.
	void NkOpenglWebBindVertexBuffer(NkOpenGLDevice *dev, uint64 pipelineId, uint32 binding, GLuint bufId, GLintptr off,
									 GLsizei stride) {
		auto *pipeline = dev->mPipelines.Find(pipelineId);
		if (!pipeline)
			return;
		const NkVertexLayout &vl = pipeline->vertexLayout;
		bool perInstance = false;
		for (uint32 i = 0; i < vl.bindings.Size(); ++i) {
			if (vl.bindings[i].binding == binding) {
				perInstance = vl.bindings[i].perInstance;
				break;
			}
		}
		// NKTEMP-DIAG : a retirer (instrumentation classes de buffers WebGL2)
		fprintf(stderr, "[WebDiag] BindVB gl=%u binding=%u\n", bufId, binding);
		glBindBuffer(GL_ARRAY_BUFFER, bufId);
		for (uint32 i = 0; i < vl.attributes.Size(); ++i) {
			const auto &a = vl.attributes[i];
			if (a.binding != binding)
				continue;
			GLint compCount = 3;
			GLenum compType = GL_FLOAT;
			GLboolean norm = GL_FALSE;
			bool isInteger = false;
			switch (a.format) { // meme decodage que CreateGraphicsPipeline
				case NkVertexFormat::NK_R32_UINT:
					compCount = 1;
					compType = GL_UNSIGNED_INT;
					isInteger = true;
					break;
				case NkVertexFormat::NK_RG32_UINT:
					compCount = 2;
					compType = GL_UNSIGNED_INT;
					isInteger = true;
					break;
				case NkVertexFormat::NK_RGBA32_UINT:
					compCount = 4;
					compType = GL_UNSIGNED_INT;
					isInteger = true;
					break;
				case NkVertexFormat::NK_R32_SINT:
					compCount = 1;
					compType = GL_INT;
					isInteger = true;
					break;
				case NkVertexFormat::NK_RG32_SINT:
					compCount = 2;
					compType = GL_INT;
					isInteger = true;
					break;
				case NkVertexFormat::NK_RGBA32_SINT:
					compCount = 4;
					compType = GL_INT;
					isInteger = true;
					break;
				case NkVertexFormat::NK_R32_FLOAT:
					compCount = 1;
					break;
				case NkVertexFormat::NK_RG32_FLOAT:
					compCount = 2;
					break;
				case NkVertexFormat::NK_RGB32_FLOAT:
					compCount = 3;
					break;
				case NkVertexFormat::NK_RGBA32_FLOAT:
					compCount = 4;
					break;
				case NkVertexFormat::NK_RGBA8_UNORM:
				case NkVertexFormat::NK_R8G8B8A8_UNORM_PACKED:
					compCount = 4;
					compType = GL_UNSIGNED_BYTE;
					norm = GL_TRUE;
					break;
				case NkVertexFormat::NK_RGBA8_SNORM:
					compCount = 4;
					compType = GL_BYTE;
					norm = GL_TRUE;
					break;
				case NkVertexFormat::NK_A2B10G10R10_UNORM:
					compCount = 4;
					compType = GL_UNSIGNED_INT_2_10_10_10_REV;
					norm = GL_TRUE;
					break;
				default:
					break;
			}
			const void *ptr = (const void *)(size_t)((uint64)off + (uint64)a.offset);
			if (isInteger)
				glVertexAttribIPointer(a.location, compCount, compType, stride, ptr);
			else
				glVertexAttribPointer(a.location, compCount, compType, norm, stride, ptr);
			// divisor par ATTRIBUT en ES 3.0 (le divisor par binding est ES 3.1)
			glVertexAttribDivisor(a.location, perInstance ? 1u : 0u);
			glEnableVertexAttribArray(a.location);
		}
	}
#endif // NKENTSEU_PLATFORM_EMSCRIPTEN

	NkPipelineHandle NkOpenGLDevice::CreateComputePipeline(const NkComputePipelineDesc &d) {
		threading::NkScopedLockMutex lock(mMutex);
		GLShader *shader = mShaders.Find(d.shader.id);
		if (!shader)
			return {};

		GLPipeline p;
		p.program = shader->program;
		p.compDesc = d;
		p.isCompute = true;

		uint64 hid = NextId();
		mPipelines[hid] = p;
		NkPipelineHandle h;
		h.id = hid;
		return h;
	}

	void NkOpenGLDevice::DestroyPipeline(NkPipelineHandle &h) {
		threading::NkScopedLockMutex lock(mMutex);
		GLPipeline *pipeline = mPipelines.Find(h.id);
		if (!pipeline)
			return;
		if (pipeline->vao)
			glDeleteVertexArrays(1, &pipeline->vao);
		mPipelines.Erase(h.id);
		h.id = 0;
	}

	// =============================================================================
	// Render Passes & Framebuffers
	// =============================================================================
	NkRenderPassHandle NkOpenGLDevice::CreateRenderPass(const NkRenderPassDesc &d) {
		threading::NkScopedLockMutex lock(mMutex);
		uint64 hid = NextId();
		mRenderPasses[hid] = d;
		NkRenderPassHandle h;
		h.id = hid;
		return h;
	}

	void NkOpenGLDevice::DestroyRenderPass(NkRenderPassHandle &h) {
		threading::NkScopedLockMutex lock(mMutex);
		mRenderPasses.Erase(h.id);
		h.id = 0;
	}

	NkFramebufferHandle NkOpenGLDevice::CreateFramebuffer(const NkFramebufferDesc &d) {
		threading::NkScopedLockMutex lock(mMutex);
		GLuint fbo = 0;
#if defined(NK_OPENGL_ES)
		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
#else
		glCreateFramebuffers(1, &fbo);
#endif

		for (uint32 i = 0; i < d.colorAttachments.Size(); i++) {
			GLTexture *texture = mTextures.Find(d.colorAttachments[i].id);
			if (texture) {
#if defined(NK_OPENGL_ES)
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, texture->target, texture->id, 0);
#else
				glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0 + i, texture->id, 0);
#endif
			}
		}
		if (d.depthAttachment.IsValid()) {
			GLTexture *texture = mTextures.Find(d.depthAttachment.id);
			if (texture) {
				const NkTextureDesc &desc = texture->desc;
				GLenum att = NkFormatHasStencil(desc.format) ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
#if defined(NK_OPENGL_ES)
				glFramebufferTexture2D(GL_FRAMEBUFFER, att, texture->target, texture->id, 0);
#else
				glNamedFramebufferTexture(fbo, att, texture->id, 0);
#endif
			}
		}

		// MRT : configurer les draw buffers pour TOUTES les cibles couleur.
		// Le defaut GL d'un FBO = GL_COLOR_ATTACHMENT0 SEUL -> sans cet appel,
		// les sorties fragment 1..N-1 (G-buffer deferred) etaient JETEES.
		if (d.colorAttachments.Size() > 1) {
			GLenum bufs[8];
			uint32 n = (uint32)d.colorAttachments.Size();
			if (n > 8)
				n = 8;
			for (uint32 i = 0; i < n; i++)
				bufs[i] = GL_COLOR_ATTACHMENT0 + i;
#if defined(NK_OPENGL_ES)
			glDrawBuffers((GLsizei)n, bufs); // FBO deja binde ci-dessus en ES
#else
			glNamedFramebufferDrawBuffers(fbo, (GLsizei)n, bufs);
#endif
		}

		// FBO depth-only (sans color attachment) : il faut explicitement dire que
		// ni le draw ni le read buffer ne sont color, sinon GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER.
		if (d.colorAttachments.Empty() && d.depthAttachment.IsValid()) {
#if defined(NK_OPENGL_ES)
			GLenum none = GL_NONE;
			glDrawBuffers(1, &none);
			glReadBuffer(GL_NONE);
#else
			glNamedFramebufferDrawBuffer(fbo, GL_NONE);
			glNamedFramebufferReadBuffer(fbo, GL_NONE);
#endif
		}

#if defined(NK_OPENGL_ES)
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
#else
		GLenum status = glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER);
#endif
		if (status != GL_FRAMEBUFFER_COMPLETE) {
#if defined(NK_OPENGL_ES)
			// Un FBO ne peut se creer que sur le thread qui detient le contexte.
			// Sans contexte courant, TOUS les appels GL sont des no-op silencieux
			// et le status renvoye ne veut rien dire : on le dit explicitement,
			// sinon on cherche un probleme d'attachement qui n'existe pas.
			NK_GL_ERR("Framebuffer incomplete: 0x%X (ctx courant=%p, thread=%lu)\n", (unsigned)status,
					  (void *)eglGetCurrentContext(), (unsigned long)pthread_self());
#else
			NK_GL_ERR("Framebuffer incomplete: 0x%X\n", (unsigned)status);
#endif
		}

#if !defined(NK_OPENGL_ES)
		// Cf. commentaire de CreateBuffer : diagnostic seul, désactivé sur ES.
		if (d.debugName)
			glObjectLabel(GL_FRAMEBUFFER, fbo, -1, d.debugName);
#endif

		uint64 hid = NextId();
		mFramebuffers[hid] = {fbo, d.width, d.height};
		NkFramebufferHandle h;
		h.id = hid;
		return h;
	}

	void NkOpenGLDevice::DestroyFramebuffer(NkFramebufferHandle &h) {
		threading::NkScopedLockMutex lock(mMutex);
		GLFBO *fbo = mFramebuffers.Find(h.id);
		if (!fbo)
			return;
		if (fbo->id != 0)
			glDeleteFramebuffers(1, &fbo->id);
		mFramebuffers.Erase(h.id);
		h.id = 0;
	}

	// =============================================================================
	// Descriptor Sets (émulation GL)
	// =============================================================================
	NkDescSetHandle NkOpenGLDevice::CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc &d) {
		threading::NkScopedLockMutex lock(mMutex);
		uint64 hid = NextId();
		mDescLayouts[hid] = {d};
		NkDescSetHandle h;
		h.id = hid;
		return h;
	}

	void NkOpenGLDevice::DestroyDescriptorSetLayout(NkDescSetHandle &h) {
		threading::NkScopedLockMutex lock(mMutex);
		mDescLayouts.Erase(h.id);
		h.id = 0;
	}

	NkDescSetHandle NkOpenGLDevice::AllocateDescriptorSet(NkDescSetHandle layoutHandle) {
		threading::NkScopedLockMutex lock(mMutex);
		GLDescSet ds;
		ds.layoutHandle = layoutHandle;
		uint64 hid = NextId();
		mDescSets[hid] = ds;
		NkDescSetHandle h;
		h.id = hid;
		return h;
	}

	void NkOpenGLDevice::FreeDescriptorSet(NkDescSetHandle &h) {
		threading::NkScopedLockMutex lock(mMutex);
		mDescSets.Erase(h.id);
		h.id = 0;
	}

	void NkOpenGLDevice::UpdateDescriptorSets(const NkDescriptorWrite *writes, uint32 n) {
		threading::NkScopedLockMutex lock(mMutex);
		for (uint32 i = 0; i < n; i++) {
			auto &w = writes[i];
			GLDescSet *set = mDescSets.Find(w.set.id);
			if (!set)
				continue;
			auto &b = set->bindings[w.binding];
			b.type = w.type;
			if (w.buffer.IsValid()) {
				GLBuffer *buffer = mBuffers.Find(w.buffer.id);
				if (buffer) {
					b.bufferId = buffer->id;
					b.bufferOffset = w.bufferOffset;
					b.bufferRange = w.bufferRange;
					b.bufferSize = buffer->size;
				}
			}
			if (w.texture.IsValid()) {
				GLTexture *texture = mTextures.Find(w.texture.id);
				if (texture) {
					b.textureId = texture->id;
					b.textureTarget = texture->target;
				}
			}
			if (w.sampler.IsValid()) {
				GLSampler *sampler = mSamplers.Find(w.sampler.id);
				if (sampler) {
					b.samplerId = sampler->id;
					b.samplerState = *sampler;
				}
			}
		}
	}

	void NkOpenGLDevice::ApplyDescriptors(const GLDescSet &ds) {
		GLDescSetLayout *layoutObj = mDescLayouts.Find(ds.layoutHandle.id);
		if (!layoutObj)
			return;
		const NkDescriptorSetLayoutDesc &layout = layoutObj->desc;
		for (uint32 i = 0; i < layout.bindings.Size(); i++) {
			auto &lb = layout.bindings[i];
			auto &b = ds.bindings[lb.binding];
			switch (b.type) {
				case NkDescriptorType::NK_UNIFORM_BUFFER:
				case NkDescriptorType::NK_UNIFORM_BUFFER_DYNAMIC:
					if (b.bufferId) {
						// glBindBufferRange EXIGE offset+size <= taille du buffer : sinon
						// GL_INVALID_VALUE, le binding n'est PAS etabli et, sur les pilotes
						// GLES emules (goldfish/MEmu), l'etat de presentation est corrompu
						// -> ecran noir sans erreur remontee. L'ancien defaut 65536 quand
						// bufferRange==0 depassait quasi toujours la taille reelle de l'UBO.
						uint64 range = b.bufferRange;
						const uint64 avail = (b.bufferSize > b.bufferOffset) ? (b.bufferSize - b.bufferOffset) : 0;
						if (range == 0 || (avail > 0 && range > avail))
							range = avail;
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
						// WebGL2 : les points de binding UBO sont BORNES par
						// GL_MAX_UNIFORM_BUFFER_BINDINGS (24 seulement sur SwiftShader,
						// le minimum spec). Le NkMaterialCollection UBO vit au binding
						// 25 : glBindBufferRange leve alors INVALID_VALUE ("index out
						// of range", constate 200+ fois/run, gl=30 range=1024) SANS
						// etablir le binding. Aucun shader de la scene 3D ne lit ce
						// bloc — on SAUTE le bind hors limite au lieu de spammer la
						// console (un remap complet programme+range serait requis le
						// jour ou un shader Web lira un bloc au-dela de la limite).
						{
							static GLint sMaxUboBindings = 0;
							if (sMaxUboBindings == 0)
								glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &sMaxUboBindings);
							if (sMaxUboBindings > 0 && (GLint)lb.binding >= sMaxUboBindings)
								break;
						}
#endif
						if (range > 0)
							glBindBufferRange(GL_UNIFORM_BUFFER, lb.binding, b.bufferId, (GLintptr)b.bufferOffset,
											  (GLsizeiptr)range);
						else
							glBindBufferBase(GL_UNIFORM_BUFFER, lb.binding, b.bufferId);
					}
					break;
				case NkDescriptorType::NK_STORAGE_BUFFER:
				case NkDescriptorType::NK_STORAGE_BUFFER_DYNAMIC:
					if (b.bufferId)
						glBindBufferBase(GL_SHADER_STORAGE_BUFFER, lb.binding, b.bufferId);
					break;
				case NkDescriptorType::NK_SAMPLED_TEXTURE:
				case NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER:
					if (b.textureId) {
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
						// Unite remappee sous MAX_TEXTURE_IMAGE_UNITS=16 — MEME table
						// que le glUniform1i du link (NkWebRemapTexUnit), sinon la
						// texture serait liee sur une unite que le shader ne lit pas.
						glActiveTexture(GL_TEXTURE0 + NkWebRemapTexUnit(lb.binding));
						glBindTexture(b.textureTarget ? b.textureTarget : GL_TEXTURE_2D, b.textureId);
#elif defined(NK_OPENGL_ES)
						glActiveTexture(GL_TEXTURE0 + lb.binding);
						glBindTexture(b.textureTarget ? b.textureTarget : GL_TEXTURE_2D, b.textureId);
#else
						glBindTextureUnit(lb.binding, b.textureId);
#endif
					}
					if (b.samplerId) {
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
						// WebGL2 : les objets sampler sont CORE et fiables (le bug
						// glBindSampler ne concernait que le driver GLES emule de
						// MEmu, cf. repli Android ci-dessous). Ils sont meme
						// INDISPENSABLES ici : ANGLE valide au draw que toute texture
						// depth lue par un sampler2DShadow a COMPARE_MODE actif
						// (INVALID_OPERATION NATIF sinon, sans message console — le
						// draw PBR echouait ainsi en boucle, prog PBR + fbo offscreen,
						// isole au post-callback glad). Le repli glTexParameteri ne
						// peut pas exprimer "compare ON via tShadowAtlas et OFF via
						// tShadowAtlasRaw" sur la MEME texture ; l'objet sampler (etat
						// par UNITE) si. Unite remappee comme le glActiveTexture
						// ci-dessus.
						glBindSampler(NkWebRemapTexUnit(lb.binding), b.samplerId);
#elif defined(NK_OPENGL_ES)
						// PAS de glBindSampler sur OpenGL ES : sur les pilotes GLES emules
						// (MEmu/goldfish), lier un objet sampler corrompt la presentation —
						// le rendu continue sans la moindre erreur GL, eglSwapBuffers renvoie
						// ok=1, mais le compositeur n'affiche plus que du NOIR (cause isolee
						// par bisection de commandes, cf. rapport). On applique donc l'etat
						// du sampler directement sur la texture liee, ce qui est le repli
						// classique GLES2 et donne le meme resultat visuel.
						const GLenum tgt = b.textureTarget ? b.textureTarget : GL_TEXTURE_2D;
						const GLSampler &sst = b.samplerState;
						glTexParameteri(tgt, GL_TEXTURE_MAG_FILTER, sst.magFilter);
						glTexParameteri(tgt, GL_TEXTURE_MIN_FILTER, sst.minFilter);
						glTexParameteri(tgt, GL_TEXTURE_WRAP_S, sst.wrapS);
						glTexParameteri(tgt, GL_TEXTURE_WRAP_T, sst.wrapT);
						glTexParameteri(tgt, GL_TEXTURE_WRAP_R, sst.wrapR);
						glTexParameterf(tgt, GL_TEXTURE_MIN_LOD, sst.minLod);
						glTexParameterf(tgt, GL_TEXTURE_MAX_LOD, sst.maxLod);
						if (sst.maxAnisotropy > 1.f && NkGLHasAnisotropicFilter())
							glTexParameterf(tgt, GL_TEXTURE_MAX_ANISOTROPY, sst.maxAnisotropy);
						glTexParameteri(tgt, GL_TEXTURE_COMPARE_MODE,
										sst.compareEnable ? GL_COMPARE_REF_TO_TEXTURE : GL_NONE);
						if (sst.compareEnable)
							glTexParameteri(tgt, GL_TEXTURE_COMPARE_FUNC, sst.compareFunc);
#else
						glBindSampler(lb.binding, b.samplerId);
#endif
					}
					break;
				case NkDescriptorType::NK_STORAGE_TEXTURE:
					if (b.textureId)
						glBindImageTexture(lb.binding, b.textureId, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
					break;
				default:
					break;
			}
		}
	}

	// =============================================================================
	// Command Buffers
	// =============================================================================
	NkICommandBuffer *NkOpenGLDevice::CreateCommandBuffer(NkCommandBufferType t) {
		return nkentseu::memory::NkGetDefaultAllocator().New<NkOpenGLCommandBuffer>(this, t);
	}

	void NkOpenGLDevice::DestroyCommandBuffer(NkICommandBuffer *&cb) {
		nkentseu::memory::NkGetDefaultAllocator().Delete(cb);
		cb = nullptr;
	}

	// =============================================================================
	// Submit & Sync
	// =============================================================================
	void NkOpenGLDevice::Submit(NkICommandBuffer *const *cbs, uint32 n, NkFenceHandle fence) {
		for (uint32 i = 0; i < n; i++) {
			auto *gl = dynamic_cast<NkOpenGLCommandBuffer *>(cbs[i]);
			if (gl)
				gl->Execute(this);
		}
		if (fence.IsValid()) {
			GLFenceObj *fenceObj = mFences.Find(fence.id);
			if (fenceObj) {
				if (fenceObj->sync)
					glDeleteSync(fenceObj->sync);
				fenceObj->sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
				fenceObj->signaled = false;
			}
		}
	}

	void NkOpenGLDevice::SubmitAndPresent(NkICommandBuffer *cb) {
		Submit(&cb, 1, {});
		if (mInit.presentCallback) {
			mInit.presentCallback();
			return;
		}
#if defined(NKENTSEU_PLATFORM_WINDOWS)
		if (mNativeHdc) {
			SwapBuffers(mNativeHdc);
		}
#elif defined(NKENTSEU_WINDOWING_XLIB)
		if (mGlxDisplay && mGlxWindow) {
			glXSwapBuffers(reinterpret_cast<Display *>(mGlxDisplay), static_cast<::Window>(mGlxWindow));
		}
#elif defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_HARMONYOS)
		if (mEglDisplay && mEglSurface) {
			// On controle le retour d'eglSwapBuffers : un swap sur une surface EGL
			// liee a un ANativeWindow recree par l'OS echoue sinon en SILENCE
			// (EGL_BAD_SURFACE / EGL_BAD_NATIVE_WINDOW) et l'ecran reste noir sans
			// qu'aucune erreur GL ne soit levee.
			EGLDisplay dpy = reinterpret_cast<EGLDisplay>(mEglDisplay);
			EGLSurface surf = reinterpret_cast<EGLSurface>(mEglSurface);
			EGLBoolean swapOk = eglSwapBuffers(dpy, surf);
			static int sSwapLog = 0;
			if (!swapOk || sSwapLog < 8) {
				sSwapLog++;
				EGLint sw = -1, sh = -1;
				eglQuerySurface(dpy, surf, EGL_WIDTH, &sw);
				eglQuerySurface(dpy, surf, EGL_HEIGHT, &sh);
				logger.Infof("[NkRHI_GL][ES] eglSwapBuffers ok=%d eglErr=0x%X surf=%p natWin=%p surfSize=%dx%d "
							 "curCtx=%p curSurf=%p\n",
							 (int)swapOk, (unsigned)eglGetError(), mEglSurface, mEglNativeWindow, (int)sw, (int)sh,
							 (void *)eglGetCurrentContext(), (void *)eglGetCurrentSurface(EGL_DRAW));
			}
		}
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
		// Pas de swap explicite sur le Web : le navigateur compose le canvas a la
		// fin du callback requestAnimationFrame. Un glFlush suffit pour pousser
		// les commandes avant le retour au navigateur.
		if (mWebGLContext) {
			glFlush();
		}
#endif
	}

#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_HARMONYOS)
	// Re-attache la surface EGL quand l'OS a recree la fenetre native (cf. le
	// contrat dans NkIDevice.h). Pattern eprouve de NKCanvas
	// (NkOpenGLContext::SurfaceRecreated) : release current -> destroy surface ->
	// create sur la nouvelle fenetre (meme config) -> re-make-current. Le contexte
	// GL (et donc TOUTES les ressources : textures, buffers, programmes, VAOs)
	// survit — seul le point de presentation change.
	bool NkOpenGLDevice::RecreateSurface(const NkSurfaceDesc &surf) {
		if (!mIsValid || !mEglDisplay || !mEglContext || !mEglConfig)
			return false;
#if defined(NKENTSEU_PLATFORM_ANDROID)
		void *newWin = reinterpret_cast<void *>(surf.nativeWindow);
#else // HarmonyOS
		void *newWin = reinterpret_cast<void *>(surf.ohNativeWindow);
#endif
		if (!newWin) {
			// Fenetre pas (encore) disponible : app en arriere-plan. On garde
			// l'ancienne surface ; l'appelant retentera au prochain Shown.
			return false;
		}
		// Generation de la surface demandee. HarmonyOS l'incremente a chaque
		// creation ; les autres plateformes laissent 0 et ne changent donc pas
		// de comportement.
		const uint32 genDemandee = surf.ohSurfaceGeneration;
		if (newWin == mEglNativeWindow && mEglSurface && genDemandee == mEglSurfaceGeneration) {
			return true; // meme fenetre ET meme surface -> rien a faire
		}
		logger.Infof("[NkRHI_GL][ES] RecreateSurface : ANativeWindow %p -> %p\n", mEglNativeWindow, newWin);

		EGLDisplay dpy = reinterpret_cast<EGLDisplay>(mEglDisplay);
		// 1. Liberer le current (eglDestroySurface est interdit sur une surface
		//    encore courante).
		eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		// 2. Detruire l'ancienne surface (pointait sur la fenetre morte).
		if (mEglSurface) {
			eglDestroySurface(dpy, reinterpret_cast<EGLSurface>(mEglSurface));
			mEglSurface = nullptr;
		}
		// 3. Creer la nouvelle surface sur la fenetre courante (meme retry que
		//    l'init : le splash/system peut tenir la connexion quelques ms).
		EGLSurface newSurf = EGL_NO_SURFACE;
		for (int attempt = 0; attempt < 10; ++attempt) {
			newSurf = eglCreateWindowSurface(dpy, reinterpret_cast<EGLConfig>(mEglConfig),
											 reinterpret_cast<EGLNativeWindowType>(newWin), nullptr);
			if (newSurf != EGL_NO_SURFACE)
				break;
			NK_GL_LOG("RecreateSurface : eglCreateWindowSurface essai %d/10 echoue (0x%x)\n", attempt + 1,
					  (int)eglGetError());
			usleep(50000);
		}
		if (newSurf == EGL_NO_SURFACE) {
			NK_GL_ERR("RecreateSurface : eglCreateWindowSurface failed\n");
			return false;
		}
		// 4. Re-lier le contexte existant sur la nouvelle surface.
		if (!eglMakeCurrent(dpy, newSurf, newSurf, reinterpret_cast<EGLContext>(mEglContext))) {
			NK_GL_ERR("RecreateSurface : eglMakeCurrent failed (0x%x)\n", (int)eglGetError());
			eglDestroySurface(dpy, newSurf);
			return false;
		}
		mEglSurface = newSurf;
		mEglNativeWindow = newWin;
		mEglSurfaceGeneration = genDemandee;
		// Taille de la nouvelle surface -> swapchain virtuelle.
		EGLint sw = 0, sh = 0;
		eglQuerySurface(dpy, newSurf, EGL_WIDTH, &sw);
		eglQuerySurface(dpy, newSurf, EGL_HEIGHT, &sh);
		if (sw > 0 && sh > 0 && ((uint32)sw != mWidth || (uint32)sh != mHeight))
			OnResize((uint32)sw, (uint32)sh);
		logger.Infof("[NkRHI_GL][ES] RecreateSurface OK (%dx%d)\n", (int)sw, (int)sh);
		return true;
	}
#endif

	NkFenceHandle NkOpenGLDevice::CreateFence(bool signaled) {
		threading::NkScopedLockMutex lock(mMutex);
		GLFenceObj f;
		f.signaled = signaled;
		if (signaled)
			f.sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		uint64 hid = NextId();
		mFences[hid] = f;
		NkFenceHandle h;
		h.id = hid;
		return h;
	}

	void NkOpenGLDevice::DestroyFence(NkFenceHandle &h) {
		threading::NkScopedLockMutex lock(mMutex);
		GLFenceObj *fenceObj = mFences.Find(h.id);
		if (!fenceObj)
			return;
		if (fenceObj->sync)
			glDeleteSync(fenceObj->sync);
		mFences.Erase(h.id);
		h.id = 0;
	}

	bool NkOpenGLDevice::WaitFence(NkFenceHandle f, uint64 timeoutNs) {
		GLFenceObj *fenceObj = mFences.Find(f.id);
		if (!fenceObj)
			return false;
		if (!fenceObj->sync)
			return true;
		GLenum r = glClientWaitSync(fenceObj->sync, GL_SYNC_FLUSH_COMMANDS_BIT, (GLuint64)timeoutNs);
		if (r == GL_ALREADY_SIGNALED || r == GL_CONDITION_SATISFIED) {
			fenceObj->signaled = true;
			return true;
		}
		return false;
	}

	bool NkOpenGLDevice::IsFenceSignaled(NkFenceHandle f) {
		GLFenceObj *fenceObj = mFences.Find(f.id);
		if (!fenceObj)
			return false;
		if (!fenceObj->sync)
			return fenceObj->signaled;
		GLint v = 0;
		GLsizei l = 0;
		glGetSynciv(fenceObj->sync, GL_SYNC_STATUS, sizeof(v), &l, &v);
		return v == GL_SIGNALED;
	}

	void NkOpenGLDevice::ResetFence(NkFenceHandle f) {
		GLFenceObj *fenceObj = mFences.Find(f.id);
		if (!fenceObj)
			return;
		if (fenceObj->sync) {
			glDeleteSync(fenceObj->sync);
			fenceObj->sync = nullptr;
		}
		fenceObj->signaled = false;
	}

	void NkOpenGLDevice::WaitIdle() {
		glFinish();
	}

	// =============================================================================
	// Frame
	// =============================================================================
	bool NkOpenGLDevice::BeginFrame(NkFrameContext &frame) {
		frame.frameIndex = mFrameIndex;
		frame.frameNumber = mFrameNumber;
		// Réglage GLOBAL d'espace colorimétrique (cf. NkContextDesc::srgbSwapchain) :
		// GL_FRAMEBUFFER_SRGB encode gamma à l'écriture du framebuffer par défaut. On le
		// pose chaque frame (idempotent, contexte courant garanti) pour rester cohérent
		// avec VK/DX : false = UNORM (affichage direct), true = sRGB (encode auto).
		if (NkSwapchainFormatIsSrgb(mInit.context.swapchainFormat))
			glEnable(GL_FRAMEBUFFER_SRGB);
		else
			glDisable(GL_FRAMEBUFFER_SRGB);
		return true;
	}

	void NkOpenGLDevice::EndFrame(NkFrameContext &) {
		mFrameIndex = (mFrameIndex + 1) % MAX_FRAMES;
		++mFrameNumber;
	}

	void NkOpenGLDevice::OnResize(uint32 w, uint32 h) {
		if (w == 0 || h == 0)
			return;
		mWidth = w;
		mHeight = h;
		// Mettre à jour le FBO swapchain virtuel
		GLFBO *fbo = mFramebuffers.Find(mSwapchainFB.id);
		if (fbo) {
			fbo->w = w;
			fbo->h = h;
		}
		if (mInit.resizeCallback) {
			mInit.resizeCallback(w, h);
		}
	}

	// =============================================================================
	// Conversions GL
	// =============================================================================
	GLenum NkOpenGLDevice::ToGLInternalFormat(NkGPUFormat f) {
		switch (f) {
			case NkGPUFormat::NK_R8_UNORM:
				return GL_R8;
			case NkGPUFormat::NK_RG8_UNORM:
				return GL_RG8;
			case NkGPUFormat::NK_RGBA8_UNORM:
				return GL_RGBA8;
			case NkGPUFormat::NK_RGBA8_SRGB:
				return GL_SRGB8_ALPHA8;
			case NkGPUFormat::NK_BGRA8_UNORM:
				return GL_RGBA8; // swizzle manuel
			case NkGPUFormat::NK_BGRA8_SRGB:
				return GL_SRGB8_ALPHA8;
			case NkGPUFormat::NK_R16_FLOAT:
				return GL_R16F;
			case NkGPUFormat::NK_RG16_FLOAT:
				return GL_RG16F;
			case NkGPUFormat::NK_RGBA16_FLOAT:
				return GL_RGBA16F;
			case NkGPUFormat::NK_R32_FLOAT:
				return GL_R32F;
			case NkGPUFormat::NK_RG32_FLOAT:
				return GL_RG32F;
			case NkGPUFormat::NK_RGB32_FLOAT:
				return GL_RGB32F;
			case NkGPUFormat::NK_RGBA32_FLOAT:
				return GL_RGBA32F;
			case NkGPUFormat::NK_R32_UINT:
				return GL_R32UI;
			case NkGPUFormat::NK_R16_UINT:
				return GL_R16UI;
			case NkGPUFormat::NK_RGBA16_UINT:
				return GL_RGBA16UI;
			case NkGPUFormat::NK_D16_UNORM:
				return GL_DEPTH_COMPONENT16;
			case NkGPUFormat::NK_D32_FLOAT:
				return GL_DEPTH_COMPONENT32F;
			case NkGPUFormat::NK_D24_UNORM_S8_UINT:
				return GL_DEPTH24_STENCIL8;
			case NkGPUFormat::NK_D32_FLOAT_S8_UINT:
				return GL_DEPTH32F_STENCIL8;
			case NkGPUFormat::NK_BC1_RGB_UNORM:
				return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
			case NkGPUFormat::NK_BC3_UNORM:
				return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			case NkGPUFormat::NK_BC5_UNORM:
				return GL_COMPRESSED_RG_RGTC2;
			case NkGPUFormat::NK_BC7_UNORM:
				return GL_COMPRESSED_RGBA_BPTC_UNORM;
			case NkGPUFormat::NK_R11G11B10_FLOAT:
				return GL_R11F_G11F_B10F;
			default:
				return GL_RGBA8;
		}
	}

	GLenum NkOpenGLDevice::ToGLBaseFormat(NkGPUFormat f) {
		if (NkFormatIsDepth(f))
			return NkFormatHasStencil(f) ? GL_DEPTH_STENCIL : GL_DEPTH_COMPONENT;
		switch (f) {
			case NkGPUFormat::NK_R8_UNORM:
			case NkGPUFormat::NK_R16_FLOAT:
			case NkGPUFormat::NK_R32_FLOAT:
			case NkGPUFormat::NK_R32_UINT:
				return GL_RED;
			case NkGPUFormat::NK_RG8_UNORM:
			case NkGPUFormat::NK_RG16_FLOAT:
			case NkGPUFormat::NK_RG32_FLOAT:
				return GL_RG;
			case NkGPUFormat::NK_RGB32_FLOAT:
				return GL_RGB;
			default:
				return GL_RGBA;
		}
	}

	GLenum NkOpenGLDevice::ToGLType(NkGPUFormat f) {
		switch (f) {
			case NkGPUFormat::NK_R16_FLOAT:
			case NkGPUFormat::NK_RG16_FLOAT:
			case NkGPUFormat::NK_RGBA16_FLOAT:
				return GL_HALF_FLOAT;
			case NkGPUFormat::NK_R32_FLOAT:
			case NkGPUFormat::NK_RG32_FLOAT:
			case NkGPUFormat::NK_RGB32_FLOAT:
			case NkGPUFormat::NK_RGBA32_FLOAT:
			case NkGPUFormat::NK_D32_FLOAT:
				return GL_FLOAT;
			case NkGPUFormat::NK_R32_UINT:
			case NkGPUFormat::NK_RGBA16_UINT:
				return GL_UNSIGNED_INT;
			case NkGPUFormat::NK_D24_UNORM_S8_UINT:
				return GL_UNSIGNED_INT_24_8;
			case NkGPUFormat::NK_D32_FLOAT_S8_UINT:
				return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
			default:
				return GL_UNSIGNED_BYTE;
		}
	}

	GLenum NkOpenGLDevice::ToGLTextureTarget(NkTextureType t, NkSampleCount s) {
		bool msaa = s > NkSampleCount::NK_S1;
		switch (t) {
			case NkTextureType::NK_TEX1D:
				return GL_TEXTURE_1D;
			case NkTextureType::NK_TEX2D:
				return msaa ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
			case NkTextureType::NK_TEX3D:
				return GL_TEXTURE_3D;
			case NkTextureType::NK_CUBE:
				return GL_TEXTURE_CUBE_MAP;
			case NkTextureType::NK_TEX2D_ARRAY:
				return GL_TEXTURE_2D_ARRAY;
			case NkTextureType::NK_CUBE_ARRAY:
				return GL_TEXTURE_CUBE_MAP_ARRAY;
			default:
				return GL_TEXTURE_2D;
		}
	}

	GLenum NkOpenGLDevice::ToGLFilter(NkFilter f, NkMipFilter mf) {
		if (mf == NkMipFilter::NK_NONE)
			return f == NkFilter::NK_NEAREST ? GL_NEAREST : GL_LINEAR;
		if (mf == NkMipFilter::NK_NEAREST)
			return f == NkFilter::NK_NEAREST ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_NEAREST;
		return f == NkFilter::NK_NEAREST ? GL_NEAREST_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_LINEAR;
	}

	GLenum NkOpenGLDevice::ToGLWrap(NkAddressMode a) {
		switch (a) {
			case NkAddressMode::NK_REPEAT:
				return GL_REPEAT;
			case NkAddressMode::NK_MIRRORED_REPEAT:
				return GL_MIRRORED_REPEAT;
			case NkAddressMode::NK_CLAMP_TO_EDGE:
				return GL_CLAMP_TO_EDGE;
			case NkAddressMode::NK_CLAMP_TO_BORDER:
#if defined(NK_OPENGL_ES)
				// GL_CLAMP_TO_BORDER (0x812D) est DESKTOP-ONLY : OpenGL ES 3.x et WebGL2
				// ne connaissent que REPEAT / MIRRORED_REPEAT / CLAMP_TO_EDGE (le border
				// clamp n'y existe que via EXT_texture_border_clamp). Le passer a
				// glSamplerParameteri/glTexParameteri leve GL_INVALID_ENUM
				// ("samplerParameter: invalid parameter") et laisse le sampler
				// partiellement configure. CLAMP_TO_EDGE est le repli standard.
				return GL_CLAMP_TO_EDGE;
#else
				return GL_CLAMP_TO_BORDER;
#endif
			default:
				return GL_REPEAT;
		}
	}

	GLenum NkOpenGLDevice::ToGLCompareOp(NkCompareOp op) {
		switch (op) {
			case NkCompareOp::NK_NEVER:
				return GL_NEVER;
			case NkCompareOp::NK_LESS:
				return GL_LESS;
			case NkCompareOp::NK_EQUAL:
				return GL_EQUAL;
			case NkCompareOp::NK_LESS_EQUAL:
				return GL_LEQUAL;
			case NkCompareOp::NK_GREATER:
				return GL_GREATER;
			case NkCompareOp::NK_NOT_EQUAL:
				return GL_NOTEQUAL;
			case NkCompareOp::NK_GREATER_EQUAL:
				return GL_GEQUAL;
			case NkCompareOp::NK_ALWAYS:
				return GL_ALWAYS;
			default:
				return GL_LEQUAL;
		}
	}

	GLenum NkOpenGLDevice::ToGLBlendFactor(NkBlendFactor f) {
		switch (f) {
			case NkBlendFactor::NK_ZERO:
				return GL_ZERO;
			case NkBlendFactor::NK_ONE:
				return GL_ONE;
			case NkBlendFactor::NK_SRC_COLOR:
				return GL_SRC_COLOR;
			case NkBlendFactor::NK_ONE_MINUS_SRC_COLOR:
				return GL_ONE_MINUS_SRC_COLOR;
			case NkBlendFactor::NK_DST_COLOR:
				return GL_DST_COLOR;
			case NkBlendFactor::NK_ONE_MINUS_DST_COLOR:
				return GL_ONE_MINUS_DST_COLOR;
			case NkBlendFactor::NK_SRC_ALPHA:
				return GL_SRC_ALPHA;
			case NkBlendFactor::NK_ONE_MINUS_SRC_ALPHA:
				return GL_ONE_MINUS_SRC_ALPHA;
			case NkBlendFactor::NK_DST_ALPHA:
				return GL_DST_ALPHA;
			case NkBlendFactor::NK_ONE_MINUS_DST_ALPHA:
				return GL_ONE_MINUS_DST_ALPHA;
			case NkBlendFactor::NK_CONSTANT_COLOR:
				return GL_CONSTANT_COLOR;
			case NkBlendFactor::NK_ONE_MINUS_CONSTANT_COLOR:
				return GL_ONE_MINUS_CONSTANT_COLOR;
			case NkBlendFactor::NK_SRC_ALPHA_SATURATE:
				return GL_SRC_ALPHA_SATURATE;
			default:
				return GL_ONE;
		}
	}

	GLenum NkOpenGLDevice::ToGLBlendOp(NkBlendOp op) {
		switch (op) {
			case NkBlendOp::NK_ADD:
				return GL_FUNC_ADD;
			case NkBlendOp::NK_SUB:
				return GL_FUNC_SUBTRACT;
			case NkBlendOp::NK_REV_SUB:
				return GL_FUNC_REVERSE_SUBTRACT;
			case NkBlendOp::NK_MIN:
				return GL_MIN;
			case NkBlendOp::NK_MAX:
				return GL_MAX;
			default:
				return GL_FUNC_ADD;
		}
	}

	GLenum NkOpenGLDevice::ToGLPrimitive(NkPrimitiveTopology t) {
		switch (t) {
			case NkPrimitiveTopology::NK_TRIANGLE_LIST:
				return GL_TRIANGLES;
			case NkPrimitiveTopology::NK_TRIANGLE_STRIP:
				return GL_TRIANGLE_STRIP;
			case NkPrimitiveTopology::NK_TRIANGLE_FAN:
				return GL_TRIANGLE_FAN;
			case NkPrimitiveTopology::NK_LINE_LIST:
				return GL_LINES;
			case NkPrimitiveTopology::NK_LINE_STRIP:
				return GL_LINE_STRIP;
			case NkPrimitiveTopology::NK_POINT_LIST:
				return GL_POINTS;
			case NkPrimitiveTopology::NK_PATCH_LIST:
				return GL_PATCHES;
			default:
				return GL_TRIANGLES;
		}
	}

	GLenum NkOpenGLDevice::ToGLShaderStage(NkShaderStage s) {
		switch (s) {
			case NkShaderStage::NK_VERTEX:
				return GL_VERTEX_SHADER;
			case NkShaderStage::NK_FRAGMENT:
				return GL_FRAGMENT_SHADER;
			case NkShaderStage::NK_GEOMETRY:
				return GL_GEOMETRY_SHADER;
			case NkShaderStage::NK_TESS_CTRL:
				return GL_TESS_CONTROL_SHADER;
			case NkShaderStage::NK_TESS_EVAL:
				return GL_TESS_EVALUATION_SHADER;
			case NkShaderStage::NK_COMPUTE:
				return GL_COMPUTE_SHADER;
			default:
				return GL_VERTEX_SHADER;
		}
	}

	GLenum NkOpenGLDevice::ToGLBufferUsage(NkResourceUsage u, NkBindFlags) {
		switch (u) {
			case NkResourceUsage::NK_UPLOAD:
				return GL_DYNAMIC_DRAW;
			case NkResourceUsage::NK_READBACK:
				return GL_DYNAMIC_READ;
			case NkResourceUsage::NK_IMMUTABLE:
				return GL_STATIC_DRAW;
			default:
				return GL_DYNAMIC_COPY;
		}
	}

} // namespace nkentseu