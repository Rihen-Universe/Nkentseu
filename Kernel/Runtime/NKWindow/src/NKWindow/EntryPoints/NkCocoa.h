#pragma once

// =============================================================================
// NkCocoa.h
// Point d'entrée macOS Cocoa — côté C++ PUR.
//
// Tout l'Objective-C (NSApplication, délégué, boucle) vit dans NkCocoaMain.mm
// (compilé par NKWindow) : ce header, inclus par le main.cpp C++ de chaque
// application, ne fait que définir main() et déléguer à NkCocoaRunApp.
// L'ancienne version définissait le délégué ObjC ICI même → Foundation dans
// une unité C++ = mur d'erreurs (révélé par la première CI macOS, 2026-08-11).
// =============================================================================

#include "NKWindow/Core/NkEntry.h"

#ifndef NK_APP_NAME
#define NK_APP_NAME "cocoa_app"
#endif

namespace nkentseu {
	// Implémentée dans EntryPoints/NkCocoaMain.mm (Objective-C++).
	int NkCocoaRunApp(int argc, const char **argv, const char *appName);
} // namespace nkentseu

int main(int argc, const char *argv[]) {
	return nkentseu::NkCocoaRunApp(argc, argv, NK_APP_NAME);
}
