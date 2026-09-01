// =============================================================================
// NkLudo — point d'entree, et RIEN D'AUTRE
//
//   Ludo/NkLudoRegles.{h,cpp}   regles ET geometrie du plateau — sans dessin
//   Ludo/NkLudoEcran.{h,cpp}    palette + dessin — ne decide rien
//   Ludo/NkLudoJeu.{h,cpp}      l'application    — le seul a modifier l'etat
//   Ludo/NkLudoBanc.{h,cpp}     le banc          — regles ET geometrie
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Ludo/NkLudoJeu.h"

// NKMain.h fournit le point d'entree NATIF de chaque plateforme. Sans lui, tout
// compile et l'edition de liens tombe sur "undefined reference to WinMain".
#include "NKWindow/NKMain.h"

NKENTSEU_DEFINE_APP_DATA(([]() {
	nkentseu::NkAppData d{};
	d.appName = "NkLudo";
	d.appVersion = "1.0.0";
	return d;
})());

int nkmain(const nkentseu::NkEntryState &state) {
	return nkentseu::renderer::NkCanvasApp::Run<nkentseu::jeux::ludo::NkLudoJeu>(state);
}
