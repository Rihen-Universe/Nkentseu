// =============================================================================
// Physic2D — point d'entree, et RIEN D'AUTRE
//
// La demo de simulation 2D d'Unkeny. Ce qu'elle simule vit plus bas :
//
//   NKPhysics/NkParticules2D          corps mous, blobs, fluides, atomes (XPBD)
//   NKPhysics/NkPhysicsWorld          corps rigides, couples aux particules
//   Unkeny/Scene/NkUnkenyScene        les entites NKECS qui portent les deux
//   Unkeny/Rendu/*                    le dessin des corps mous, de la grille
//
// Ce qui est propre a la demo :
//
//   Physic2D/Physic2D.{h,cpp}         etats (edition / jeu / pause), entrees, outils
//   Physic2D/Physic2DEcran.cpp        mise en page et dessin
//   Physic2D/Physic2DActeurs.{h,cpp}  les acteurs et les niveaux
//   Physic2D/Physic2DBanc.{h,cpp}     le banc de l'integration : --selftest
//
// Lancer :   Physic2D                 la demo
//            Physic2D --niveau=1      directement sur "Bac a slime"
//            Physic2D --selftest      le banc, sans fenetre (code de sortie = verdict)
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

#include "Physic2D/Physic2D.h"

NKENTSEU_DEFINE_APP_DATA(([]() {
	nkentseu::NkAppData d{};
	d.appName = "Physic2D";
	d.appVersion = "1.0.0";
	return d;
})());

int nkmain(const nkentseu::NkEntryState &state) {
	return nkentseu::renderer::NkCanvasApp::Run<nkentseu::Physic2D>(state);
}
