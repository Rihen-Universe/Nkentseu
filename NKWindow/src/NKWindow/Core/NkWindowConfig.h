#pragma once

// =============================================================================
// NkWindowConfig.h
// Configuration de création de fenêtre + Safe Area pour mobile.
// =============================================================================

#include "NkTypes.h"
#include "NkSafeArea.h"
#include <string>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

/**
 * @brief Enumeration NkScreenOrientation.
 */
enum class NkScreenOrientation : NkU32 {
	NK_SCREEN_ORIENTATION_AUTO = 0,
	NK_SCREEN_ORIENTATION_PORTRAIT,
	NK_SCREEN_ORIENTATION_LANDSCAPE,
};

// ---------------------------------------------------------------------------
// Options Web (WASM) pour le routage des entrées navigateur/app
// ---------------------------------------------------------------------------

struct NkWebInputOptions {
	// Clavier
	bool captureKeyboard = true;
	bool allowBrowserShortcuts = true; // F12, Ctrl+Shift+I/J, Ctrl+R, Meta+...

	// Souris
	bool captureMouseMove = true;
	bool captureMouseLeft = true;
	bool captureMouseMiddle = true;
	bool captureMouseRight = false;
	bool captureMouseWheel = true;

	// Tactile
	bool captureTouch = true;

	// Menu contextuel navigateur sur le canvas
	bool preventContextMenu = false;
};

// ---------------------------------------------------------------------------
// NkWindowConfig
// ---------------------------------------------------------------------------

struct NkWindowConfig {
	// --- Position et taille ---
	NkI32 x = 100;
	NkI32 y = 100;
	NkU32 width = 1280;
	NkU32 height = 720;
	NkU32 minWidth = 160;
	NkU32 minHeight = 90;
	NkU32 maxWidth = 0xFFFF;
	NkU32 maxHeight = 0xFFFF;

	// --- Comportement ---
	bool centered = true;
	bool resizable = true;
	bool movable = true;
	bool closable = true;
	bool minimizable = true;
	bool maximizable = true;
	bool canFullscreen = true;
	bool fullscreen = false;
	bool modal = false;
	bool vsync = true;
	bool dropEnabled = false;
	NkScreenOrientation screenOrientation = NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO;

	// --- Apparence ---
	bool frame = true;
	bool hasShadow = true;
	bool transparent = false;
	bool visible = true;
	NkU32 bgColor = 0x141414FF;

	// --- Identité ---
	std::string title = "NkWindow";
	std::string name = "NkApp";
	std::string iconPath;

	// --- Mobile / Safe Area ---
	// Si true : le renderer recevra les insets via Window::GetSafeAreaInsets().
	// Sur desktop : sans effet.
	bool respectSafeArea = true;

	// --- Web / WASM ---
	// Contrôle fin de la capture des entrées navigateur.
	NkWebInputOptions webInput;
};

} // namespace nkentseu
