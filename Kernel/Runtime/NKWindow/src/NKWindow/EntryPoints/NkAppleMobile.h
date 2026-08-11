#pragma once

// =============================================================================
// NkAppleMobile.h
// Point d'entrée iOS / tvOS / visionOS — côté C++ PUR.
//
// Tout l'Objective-C (UIApplication, délégué) vit dans NkAppleMobileMain.mm
// (compilé par NKWindow) : ce header, inclus par le main.cpp C++ de chaque
// application, ne fait que définir main() et déléguer. L'ancienne version
// définissait le délégué UIKit ICI même → Foundation dans une unité C++ =
// mur d'erreurs (révélé par le premier probe iOS en CI, 2026-08-11).
// =============================================================================

#include "NKWindow/Core/NkEntry.h"

// TARGET_OS_* : indisponibles en C++ pur sans TargetConditionals — le nom
// d'app par défaut se décide côté .mm si besoin ; ici un défaut générique.
#ifndef NK_APP_NAME
#define NK_APP_NAME "ios_app"
#endif

namespace nkentseu {
	// Implémentée dans EntryPoints/NkAppleMobileMain.mm (Objective-C++).
	int NkAppleMobileRunApp(int argc, char **argv, const char *appName);
} // namespace nkentseu

int main(int argc, char *argv[]) {
	return nkentseu::NkAppleMobileRunApp(argc, argv, NK_APP_NAME);
}
