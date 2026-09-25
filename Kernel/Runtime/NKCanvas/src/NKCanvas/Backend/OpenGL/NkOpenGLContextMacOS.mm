// =============================================================================
// NkOpenGLContext_macOS.mm â€” ImplÃ©mentation macOS (Objective-C++)
// Compiler avec : -x objective-c++
// CMake : set_source_files_properties(NkOpenGLContext_macOS.mm
//             PROPERTIES COMPILE_FLAGS "-x objective-c++")
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"
#include "NKLogger/NkLog.h"

#if defined(NKENTSEU_PLATFORM_MACOS)

#include "NkOpenGLContext.h"
// ⑪ `NKContext` n'existe plus : ce fichier est dans NKCanvas depuis le renommage, et
//    `NkSurfaceDesc.h` avec lui (mesure du 05/09 : aucun module `NKContext` dans l'arbre,
//    un seul `NkSurfaceDesc.h`, ici). L'erreur ne se voyait qu'a la compilation macOS.
#include "NKCanvas/Core/NkSurfaceDesc.h"
#include "NKContainers/NKContainers.h"
#import <AppKit/AppKit.h>
#import <OpenGL/OpenGL.h>

#define NK_GL_LOG(...) logger.Infof("[NkOpenGL/NSGL] " __VA_ARGS__)
#define NK_GL_ERR(...) logger.Errorf("[NkOpenGL/NSGL] " __VA_ARGS__)

namespace nkentseu {

	bool NkOpenGLContext::InitNSGL(const NkSurfaceDesc &surf, const NkOpenGLDesc &gl) {
		NSView *view = (__bridge NSView *)surf.view;
		if (!view) {
			NK_GL_ERR("NSView is null\n");
			return false;
		}

		// Construire les attributs pixel format depuis NkOpenGLDesc.
		//
		// REPLI (2026-09-24) : la demande complete (acceleration materielle +
		// MSAA) echouait sur une machine sans GPU accelere (VM macOS de GitHub :
		// « NSOpenGLPixelFormat creation failed ») et NKCode ne s'ouvrait pas.
		// On descend : complet -> sans MSAA -> sans exiger l'acceleration (rendu
		// logiciel d'Apple) -> profil 3.2 Core. Meme principe que le repli GLX
		// de NkOpenGLContext.cpp : ce qui ne peut pas se faire a une doublure,
		// jamais un trou.
		auto construire = [&](bool accel, bool msaa, bool profil41, NkVector<NSOpenGLPixelFormatAttribute> &attribs) {
			attribs.Clear();
			if (gl.profile == NkGLProfile::Core) {
				attribs.PushBack(NSOpenGLPFAOpenGLProfile);
				attribs.PushBack(profil41 ? NSOpenGLProfileVersion4_1Core : NSOpenGLProfileVersion3_2Core);
			}
			if (accel)
				attribs.PushBack(NSOpenGLPFAAccelerated);
			attribs.PushBack(NSOpenGLPFADoubleBuffer); // doubleBuffer toujours sur macOS
			attribs.PushBack(NSOpenGLPFAColorSize);
			attribs.PushBack(gl.colorBits >= 32 ? 32 : 24);
			if (gl.alphaBits > 0) {
				attribs.PushBack(NSOpenGLPFAAlphaSize);
				attribs.PushBack(gl.alphaBits);
			}
			if (gl.depthBits > 0) {
				attribs.PushBack(NSOpenGLPFADepthSize);
				attribs.PushBack(gl.depthBits);
			}
			if (gl.stencilBits > 0) {
				attribs.PushBack(NSOpenGLPFAStencilSize);
				attribs.PushBack(gl.stencilBits);
			}
			if (msaa && gl.msaaSamples > 1) {
				attribs.PushBack(NSOpenGLPFAMultisample);
				attribs.PushBack(NSOpenGLPFASampleBuffers);
				attribs.PushBack(1);
				attribs.PushBack(NSOpenGLPFASamples);
				attribs.PushBack(gl.msaaSamples);
			}
			attribs.PushBack(0);
		};

		const bool veut41 = gl.majorVersion >= 4;
		struct Essai { bool accel, msaa, profil41; const char *nom; };
		const Essai essais[] = {
			{true, true, veut41, "demande complete"},
			{true, false, veut41, "sans MSAA"},
			{false, false, veut41, "sans acceleration exigee"},
			{false, false, false, "profil 3.2 Core, sans acceleration exigee"},
		};
		NkVector<NSOpenGLPixelFormatAttribute> attribs;
		NSOpenGLPixelFormat *pf = nil;
		for (const Essai &e : essais) {
			construire(e.accel, e.msaa, e.profil41, attribs);
			pf = [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs.Data()];
			if (pf) {
				if (&e != &essais[0])
					NK_GL_LOG("pixel format obtenu en repli : %s\n", e.nom);
				break;
			}
		}
		if (!pf) {
			NK_GL_ERR("NSOpenGLPixelFormat creation failed (tous les replis refuses)\n");
			return false;
		}

		// Partage de ressources avec contexte parent
		NSOpenGLContext *shareNSCtx = nil;
		if (mSharedParent && mSharedParent->mData.context)
			shareNSCtx = (__bridge NSOpenGLContext *)mSharedParent->mData.context;

		NSOpenGLContext *ctx = [[NSOpenGLContext alloc] initWithFormat:pf shareContext:shareNSCtx];
		if (!ctx) {
			NK_GL_ERR("NSOpenGLContext creation failed\n");
			return false;
		}

		[ctx setView:view];
		[ctx makeCurrentContext];

		// VSync via CGLSetParameter
		GLint swapInterval = (gl.swapInterval == NkGLSwapInterval::Immediate) ? 0 : 1;
		[ctx setValues:&swapInterval forParameter:NSOpenGLContextParameterSwapInterval];

		// Stocker dans mData
		mData.context = (void *)CFBridgingRetain(ctx);
		mData.pixelFormat = (void *)CFBridgingRetain(pf);
		mData.view = (void *)CFBridgingRetain(view);

		NK_GL_LOG("NSGL OK (OpenGL %d.%d)\n", gl.majorVersion, gl.minorVersion);
		return true;
	}

	void NkOpenGLContext::ShutdownNSGL() {
		if (mData.context) {
			NSOpenGLContext *ctx = (__bridge_transfer NSOpenGLContext *)mData.context;
			[NSOpenGLContext clearCurrentContext];
			[ctx clearDrawable];
			mData.context = nullptr;
		}
		if (mData.pixelFormat) {
			CFBridgingRelease(mData.pixelFormat);
			mData.pixelFormat = nullptr;
		}
		if (mData.view) {
			CFBridgingRelease(mData.view);
			mData.view = nullptr;
		}
	}

	void NkOpenGLContext::SwapNSGL() {
		if (mData.context) {
			NSOpenGLContext *ctx = (__bridge NSOpenGLContext *)mData.context;
			[ctx flushBuffer];
		}
	}

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_MACOS
