#pragma once
// =============================================================================
// NkAnimaControleur.h — le CONTROLEUR d'animation de NkAnimaEditor (2026-10-02)
// : le graphe d'etats PARTAGE de NKEditorKit compile en anim::NkAnimController
// (base, couches, arbres de melange, courbes de fondu), son fichier
// .nkanimctl, et son APERCU dans la vue 3D.
//
// ⚠️ L'unite qui l'implemente (NkAnimaControleur.cpp) inclut NKAnima et le
//    modele du graphe, JAMAIS NKRenderer (AnimBridge.h dit pourquoi) ; cet
//    en-tete, lui, n'expose que le modele du kit.
// =============================================================================
#include "NKEditorKit/Components/NkStateGraphModel.h"

namespace nkanima {

	/// Le graphe -> le controleur (a chaque changement). Faux si la base n'a aucun etat.
	bool NkAnimaControleurCompiler(const nkentseu::editorkit::NkStateGraphModel &g);
	/// Le controleur COMPILE en .nkanimctl (le fichier d'UnkenyEditor, le meme lecteur).
	bool NkAnimaControleurEnregistrer(const char *chemin);
	/// Un .nkanimctl -> le graphe (et le controleur). Faux s'il ne se lit pas.
	bool NkAnimaControleurCharger(const char *chemin, nkentseu::editorkit::NkStateGraphModel &g);
	/// UNE IMAGE D'APERCU : le controleur avance de `dt` avec les parametres
	/// VIVANTS du graphe, sa pose melangee remplace celle du lecteur dans la vue
	/// 3D, et le graphe allume l'etat actif, le fondu, la transition tiree.
	void NkAnimaControleurApercu(nkentseu::float32 dt, nkentseu::editorkit::NkStateGraphModel &g);
	/// Fin de l'apercu : la vue 3D rend la pose du lecteur.
	void NkAnimaControleurArreter(nkentseu::editorkit::NkStateGraphModel &g);

} // namespace nkanima
