// =============================================================================
// NkDames — point d'entree, et RIEN D'AUTRE
//
// Ce fichier ne contient volontairement aucune logique. Il declare les
// metadonnees de l'application et rend la main a la coquille. Tout ce qui a une
// nature differente a son fichier :
//
//   Dames/NkDamesRegles.{h,cpp}   les regles       — sans dessin ni fenetre
//   Dames/NkDamesTheme.h          les couleurs     — la seule source
//   Dames/NkDamesEcran.{h,cpp}    geometrie+dessin — ne decide rien
//   Dames/NkDamesJeu.{h,cpp}      l'application    — le seul a modifier l'etat
//   Dames/NkDamesBanc.{h,cpp}     le banc          — verdict par code de sortie
//
// ⚠️ ON NE DEFINIT PAS DE NOUVEAU main. `nkmain` EST le point d'entree portable
// du depot, avec seize points d'entree de plateforme derriere lui. On le garde
// et on delegue en une ligne : c'est ce qui laisse l'application repondre AVANT
// toute fenetre (voir OnCommandLine, ou vit --selftest).
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Dames/NkDamesJeu.h"

// NKMain.h fournit le point d'entree NATIF de chaque plateforme (WinMain,
// android_main, UIApplicationMain...). Sans lui, tout compile et l'edition de
// liens tombe sur "undefined reference to WinMain" — une erreur qui ne nomme
// aucun de nos fichiers, et qu'on cherche donc partout sauf ici.
#include "NKWindow/NKMain.h"

NKENTSEU_DEFINE_APP_DATA(([]() {
	nkentseu::NkAppData d{};
	d.appName = "NkDames";
	d.appVersion = "1.0.0";
	return d;
})());

int nkmain(const nkentseu::NkEntryState &state) {
	return nkentseu::renderer::NkCanvasApp::Run<nkentseu::jeux::dames::NkDamesJeu>(state);
}
