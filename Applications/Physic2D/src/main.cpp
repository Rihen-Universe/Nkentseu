// =============================================================================
// Physic2D — point d'entree, et RIEN D'AUTRE
//
// Tout ce qui a une nature differente a son fichier :
//
//   Physic2D/Physique/NkPhysMath.h          vecteur 2D, hasard deterministe
//   Physic2D/Physique/NkPhysMonde.{h,cpp}   le moteur (sans dessin ni fenetre)
//   Physic2D/Physique/NkPhysFabrique.{h,cpp} les acteurs et les niveaux
//   Physic2D/Physique/NkPhysBanc.{h,cpp}    le banc : --selftest
//   Physic2D/Editeur/NkUE5Theme.h           les couleurs — la seule source
//   Physic2D/Editeur/NkUE5Ui.{h,cpp}        widgets et icones facon UE5 (sur NKGui)
//   Physic2D/Editeur/NkPhysRendu.{h,cpp}    le dessin du monde dans la vue
//   Physic2D/Editeur/Physic2D.{h,cpp}       l'application : etats, entrees, outils
//   Physic2D/Editeur/Physic2DPanneaux.cpp   les panneaux de l'editeur
//
// Lancer :   Physic2D                 l'editeur
//            Physic2D --niveau=1      directement sur "Bac a slime"
//            Physic2D --selftest      le banc du moteur, sans fenetre
//            Physic2D --capture=a.png une image de la trame 30, puis sortie
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "NKWindow/NKMain.h"
#include "NKWindow/NKWindow.h"
#include "NKLogger/NkLog.h"

// NKCanvas
#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Renderer/Core/NkRenderer2D.h"
#include "NKCanvas/App/NkCanvasApp.h"

#include "NKMath/NKMath.h"
#include "NKMath/NkColor.h"
#include "NKTime/NkTime.h"

#include "Physic2D/Editeur/Physic2D.h"

NKENTSEU_DEFINE_APP_DATA(([]() {
	nkentseu::NkAppData d{};
	d.appName = "Physic2D";
	d.appVersion = "1.0.0";
	return d;
})());

int nkmain(const nkentseu::NkEntryState &state) {
	return nkentseu::renderer::NkCanvasApp::Run<nkentseu::Physic2D>(state);
}
