//
// main.cpp
// =============================================================================
// Description :
//   UnkenyPlayer — le joueur autonome d'Unkeny. Ce fichier ne fait que trois
//   choses : declarer l'application, mettre la console en UTF-8 (Windows),
//   lancer la coquille. Tout le reste vit dans `Joueur/`.
//
// Caracteristiques :
//   - Le MEME point d'entree que l'editeur et les jeux du depot (nkmain,
//     NKWindow/NKMain.h) : WinMain, android_main, la boucle Emscripten... sont
//     fournis par la plateforme.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Joueur/NkJoueurApp.h"

#include "NKWindow/Core/NkEntry.h"
#include "NKWindow/NKMain.h"

// ⚠️ La macro attend un `NkAppData`, pas une chaine (voir UnkenyEditor/src/main.cpp).
NKENTSEU_DEFINE_APP_DATA(([]() {
	nkentseu::NkAppData d{};
	d.appName = "UnkenyPlayer";
	d.appVersion = "0.1.0";
	return d;
})());

int nkmain(const nkentseu::NkEntryState &state) {
#if defined(_WIN32)
	// `--verifier` ecrit « empreinte relue » : sans UTF-8, une console laissee
	// en page OEM l'affiche en mojibake (vu sur l'editeur le 2026-09-29).
	SetConsoleOutputCP(CP_UTF8);
#endif
	return nkentseu::renderer::NkCanvasApp::Run<nkentseu::joueur::NkJoueurApp>(state);
}
