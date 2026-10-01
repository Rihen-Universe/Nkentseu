// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkOpenglContexteApple.mm — CONTEXTE NSOPENGL 4.1 CORE POUR LE DEVICE GL
// (voir NkOpenglContexteApple.h). Compile sous ARC (nkobjcarc, NKRHI.jenga).
//
// REPLI : la VM macOS de GitHub n'a pas de GPU accelere pour OpenGL ; la
// demande « acceleree » y est refusee. On redescend alors vers le rendu
// logiciel d'Apple, toujours en 4.1 core (meme principe que NKCanvas). Le 3.2
// core n'est PAS essaye : le device exige 4.1 (separate_shader_objects,
// viewport_array, glClearDepthf...).
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_MACOS)

#define GL_SILENCE_DEPRECATION 1
#include "NkOpenglContexteApple.h"
#include "NKLogger/NkLog.h"
#import <AppKit/AppKit.h>
#import <OpenGL/OpenGL.h>

namespace nkentseu {

	bool NkGLAppleCreerContexte(void *vue, bool vsync, void *&contexte, void *&formatPixel) {
		contexte = nullptr;
		formatPixel = nullptr;

		struct Essai {
				bool accelere;
				const char *nom;
		};
		const Essai essais[] = {
			{true, "4.1 core accelere"},
			{false, "4.1 core sans acceleration exigee (rendu logiciel d'Apple possible)"},
		};
		NSOpenGLPixelFormat *pf = nil;
		for (const Essai &e : essais) {
			NSOpenGLPixelFormatAttribute a[20];
			int n = 0;
			a[n++] = NSOpenGLPFAOpenGLProfile;
			a[n++] = NSOpenGLProfileVersion4_1Core;
			if (e.accelere)
				a[n++] = NSOpenGLPFAAccelerated;
			a[n++] = NSOpenGLPFADoubleBuffer;
			a[n++] = NSOpenGLPFAColorSize;
			a[n++] = 24;
			a[n++] = NSOpenGLPFAAlphaSize;
			a[n++] = 8;
			a[n++] = NSOpenGLPFADepthSize;
			a[n++] = 24;
			a[n++] = NSOpenGLPFAStencilSize;
			a[n++] = 8;
			a[n++] = 0;
			pf = [[NSOpenGLPixelFormat alloc] initWithAttributes:a];
			if (pf) {
				logger_src.Infof("[NkRHI_GL][NSGL] format de pixel : %s\n", e.nom);
				break;
			}
		}
		if (!pf) {
			logger_src.Errorf("[NkRHI_GL][NSGL] aucun format de pixel OpenGL 4.1 core accorde\n");
			return false;
		}

		NSOpenGLContext *ctx = [[NSOpenGLContext alloc] initWithFormat:pf shareContext:nil];
		if (!ctx) {
			logger_src.Errorf("[NkRHI_GL][NSGL] NSOpenGLContext refuse\n");
			return false;
		}

		NSView *v = (__bridge NSView *)vue;
		if (v) {
			// ⚠️ RETINA : sans ce drapeau le tampon est en POINTS alors que la
			//    fenetre donne des PIXELS (cf. NkOpenGLContextMacOS.mm de NKCanvas).
			v.wantsBestResolutionOpenGLSurface = YES;
			[ctx setView:v];
		} else {
			logger_src.Infof("[NkRHI_GL][NSGL] aucune vue : contexte sans surface (rendu hors ecran)\n");
		}
		[ctx makeCurrentContext];

		GLint intervalle = vsync ? 1 : 0;
		[ctx setValues:&intervalle forParameter:NSOpenGLContextParameterSwapInterval];

		contexte = (void *)CFBridgingRetain(ctx);
		formatPixel = (void *)CFBridgingRetain(pf);
		return true;
	}

	void NkGLApplePresenter(void *contexte) {
		if (contexte)
			[(__bridge NSOpenGLContext *)contexte flushBuffer];
	}

	void NkGLAppleMettreAJour(void *contexte) {
		if (contexte)
			[(__bridge NSOpenGLContext *)contexte update];
	}

	void NkGLAppleDetruire(void *&contexte, void *&formatPixel) {
		if (contexte) {
			NSOpenGLContext *ctx = (__bridge_transfer NSOpenGLContext *)contexte;
			[NSOpenGLContext clearCurrentContext];
			[ctx clearDrawable];
			contexte = nullptr;
		}
		if (formatPixel) {
			CFBridgingRelease(formatPixel);
			formatPixel = nullptr;
		}
	}

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_MACOS
