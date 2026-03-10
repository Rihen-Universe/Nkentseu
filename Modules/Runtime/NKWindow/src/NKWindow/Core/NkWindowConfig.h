#pragma once
// =============================================================================
// NkWindowConfig.h
// Configuration de création de fenêtre.
//
// Changement par rapport à la version précédente :
//   → Ajout du champ `surfaceHints` (NkSurfaceHints).
//     Ce champ est optionnel. Laisser vide pour Vulkan, DirectX, Metal,
//     Software, EGL. Rempli automatiquement par
//     NkGraphicsContextFactory::PrepareWindowConfig() pour OpenGL/GLX.
// =============================================================================

#include "NkTypes.h"
#include "NkSafeArea.h"
#include "NkSurfaceHint.h"   // ← seul ajout d'include

namespace nkentseu {

	enum class NkScreenOrientation : NkU32 {
		NK_SCREEN_ORIENTATION_AUTO = 0,
		NK_SCREEN_ORIENTATION_PORTRAIT,
		NK_SCREEN_ORIENTATION_LANDSCAPE,
	};

	struct NkWebInputOptions {
		bool captureKeyboard        = true;
		bool allowBrowserShortcuts  = true;
		bool captureMouseMove       = true;
		bool captureMouseLeft       = true;
		bool captureMouseMiddle     = true;
		bool captureMouseRight      = false;
		bool captureMouseWheel      = true;
		bool captureTouch           = true;
		bool preventContextMenu     = false;
	};

	struct NkWindowConfig {
		// --- Position et taille ---
		NkI32 x         = 100;
		NkI32 y         = 100;
		NkU32 width     = 1280;
		NkU32 height    = 720;
		NkU32 minWidth  = 160;
		NkU32 minHeight = 90;
		NkU32 maxWidth  = 0xFFFF;
		NkU32 maxHeight = 0xFFFF;

		// --- Comportement ---
		bool centered       = true;
		bool resizable      = true;
		bool movable        = true;
		bool closable       = true;
		bool minimizable    = true;
		bool maximizable    = true;
		bool canFullscreen  = true;
		bool fullscreen     = false;
		bool modal          = false;
		bool vsync          = true;
		bool dropEnabled    = false;
		NkScreenOrientation screenOrientation = NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO;

		// --- Apparence ---
		bool   frame       = true;
		bool   hasShadow   = true;
		bool   transparent = false;
		bool   visible     = true;
		NkU32  bgColor     = 0x141414FF;

		// --- Identité ---
		NkString title    = "NkWindow";
		NkString name     = "NkApp";
		NkString iconPath;

		// --- Mobile / Safe Area ---
		bool respectSafeArea = true;

		// --- Web / WASM ---
		NkWebInputOptions webInput;

		// ── Hints de surface ────────────────────────────────────────────────────
		// Remplis par NkGraphicsContextFactory::PrepareWindowConfig() AVANT Create().
		// NkWindow transmet ces hints au backend platform (XLib, XCB…)
		// sans les interpréter.
		// Pour la grande majorité des cas (Vulkan, Metal, DirectX, Software,
		// OpenGL/WGL, OpenGL/EGL) : laisser vide, ne rien faire.
		// Pour OpenGL/GLX sur Linux uniquement : appeler PrepareWindowConfig().
		NkSurfaceHints surfaceHints;   // ← seul ajout dans ce struct
	};

} // namespace nkentseu