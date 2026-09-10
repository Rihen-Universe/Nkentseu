// =============================================================================
// NkEchecs — point d'entree, et RIEN D'AUTRE
//
//   Echecs/NkEchecsRegles.{h,cpp}   les regles       — sans dessin ni fenetre
//   Echecs/NkEchecsEcran.{h,cpp}    palette+dessin   — ne decide rien
//   Echecs/NkEchecsJeu.{h,cpp}      l'application    — le seul a modifier l'etat
//   Echecs/NkEchecsBanc.{h,cpp}     le banc          — adosse aux valeurs perft
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Echecs/NkEchecsJeu.h"

// NKMain.h fournit le point d'entree NATIF de chaque plateforme. Sans lui, tout
// compile et l'edition de liens tombe sur "undefined reference to WinMain".
#include "NKWindow/NKMain.h"

NKENTSEU_DEFINE_APP_DATA(([]() {
	nkentseu::NkAppData d{};
	d.appName = "NkEchecs";
	d.appVersion = "1.0.0";
	return d;
})());

int nkmain(const nkentseu::NkEntryState &state) {
	return nkentseu::renderer::NkCanvasApp::Run<nkentseu::jeux::echecs::NkEchecsJeu>(state);
}
