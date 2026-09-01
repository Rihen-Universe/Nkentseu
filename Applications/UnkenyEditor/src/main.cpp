// =============================================================================
// UnkenyEditor — point d'entree, et RIEN D'AUTRE
//
//   Editeur/NkEditeurAppareils.h    profils d'appareils, zones sures simulees
//   Editeur/NkEditeurViseur.{h,cpp} le viseur : grille, scene, selection
//   Editeur/NkEditeurPanneaux.{h,cpp} hierarchie, inspecteur, barre, pied
//   Editeur/NkEditeurApp.{h,cpp}    l'assemblage et les entrees
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Editeur/NkEditeurApp.h"

// NKMain.h fournit le point d'entree NATIF de chaque plateforme. Sans lui, tout
// compile et l'edition de liens tombe sur "undefined reference to WinMain".
#include "NKWindow/NKMain.h"

NKENTSEU_DEFINE_APP_DATA(([]() {
	nkentseu::NkAppData d{};
	d.appName = "UnkenyEditor";
	d.appVersion = "0.1.0";
	return d;
})());

int nkmain(const nkentseu::NkEntryState &state) {
	return nkentseu::renderer::NkCanvasApp::Run<nkentseu::editeur::NkEditeurApp>(state);
}
