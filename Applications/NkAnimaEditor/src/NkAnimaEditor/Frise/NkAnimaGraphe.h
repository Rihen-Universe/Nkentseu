#pragma once
// =============================================================================
// NkAnimaGraphe.h — LE GRAPHE D'ETATS PARTAGE dans NkAnimaEditor (2026-10-02).
//
// Le meme composant que la page Animateur d'UnkenyEditor
// (NKEditorKit/Components/NkStateGraphModel.h) : etats, sous-machines, ARBRES
// DE MELANGE 1D / 2D, COUCHES (poids, masque d'os, additif), transitions et
// leur courbe de fondu, parametres. Il edite le controleur du modele charge
// (Frise/NkAnimaControleur.h), l'enregistre en .nkanimctl a cote du modele, et
// son APERCU joue la pose melangee dans la vue 3D.
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "NKEditorKit/Components/NkStateGraphModel.h"

namespace nkanima {

	/// Dessine la barre (Apercu, Enregistrer) et le graphe dans (x, y, w, h).
	/// `modele` : le chemin du modele (le .nkanimctl se range a cote).
	void NkAnimaDessinerGraphe(nkentseu::editorkit::NkEditorFrameContext &ec, nkentseu::float32 x, nkentseu::float32 y,
							   nkentseu::float32 w, nkentseu::float32 h, const nkentseu::editorkit::NkTheme &theme,
							   const char *modele);
	/// L'onglet du graphe n'est pas montre a cette image : l'apercu s'arrete.
	void NkAnimaGrapheCache();

	nkentseu::editorkit::NkStateGraphModel &NkAnimaGrapheModele();
	const nkentseu::editorkit::NkStateGraphResult &NkAnimaGrapheResultat();
	/// L'apercu joue-t-il ? (banc, captures)
	bool &NkAnimaGrapheApercu();

} // namespace nkanima
