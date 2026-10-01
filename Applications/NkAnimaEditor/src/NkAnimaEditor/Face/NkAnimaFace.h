#pragma once
// -----------------------------------------------------------------------------
// @File    NkAnimaFace.h
// @Brief   LA FACE D'UNREAL 5 DE NKANIMAEDITOR (feuille de route R32 : « EXACTEMENT
//          l'apparence d'UnkenyEditor, avec la vue 3D au centre ») : la trame de
//          la famille (NKEditorKit/Famille/NkFamilleEditeur) et ses pieces
//          partagees, autour de la vue 3D d'AnimBridge.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// SA TOUCHE (document 02 §0 d'UnkenyEditor : « frise temporelle au premier
// plan ; assets d'animation, squelettes, controleurs ») :
//   - Placer des acteurs : les PERSONNAGES animes du depot (CesiumMan, Fox,
//     BrainStem, SimpleSkin) -- un clic l'ouvre dans une nouvelle fenetre, par
//     le chemin de modele que l'editeur accepte deja en ligne de commande ;
//   - Outliner : le modele, son clip, son SQUELETTE (un noeud par os) ;
//   - Details : les cartes Clip, Lecture, Pose, Vue, Physique, Os ;
//   - tiroir du bas : FRISE (au premier plan), Graphe d'etats, Contenu,
//     Journal, Terminal.
//
// ⚠️ LA FRISE ET LE GRAPHE D'ETATS PARTAGES s'ecrivent EN MEME TEMPS dans
//    NKEditorKit (branche editorkit/animation-animateur, autre chantier). Ils
//    ne sont PAS ecrits ici : la frise historique (TimelinePanel) tient
//    l'onglet « Frise », et l'onglet « Graphe d'états » garde sa place. Les deux
//    points d'accroche sont marques « ACCROCHE FRISE » et « ACCROCHE GRAPHE »
//    dans NkAnimaFace.cpp.
//
// TOUTES LES FONCTIONS D'AVANT RESTENT : les actions nommees (NkAnimaActions),
// les outils de pose (Drag IK, Rotation FK, Translation FK), le ragdoll, le
// centre de masse, les modes de vue, les compteurs ; et l'ancienne coquille
// (NkEditorShell, documents d'interface, palette Ctrl+P, sonde) s'ouvre avec
// `--ancienne-coquille` ou depuis Fenetre > Ancienne coquille.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Famille/NkFamilleEditeur.h"

namespace nkanima {

	/// Lance la face d'UE5 (fenetre sans cadre, rendu NKRHI par
	/// NkEditorRHIRenderer, AnimBridge pour la vue 3D) ; rend le code de sortie.
	/// `api` : nkentseu::editorkit::NkEditorGfxApi. `secondes` > 0 : l'editeur se
	/// ferme seul apres ce temps (captures hors ecran, bancs). `horsEcran` : la
	/// fenetre se pose HORS de tout ecran, sans focus ni bouton de barre des
	/// taches (une capture qui ne gene personne, le geste de NogeDemo --capture).
	int NkAnimaLancerFace(const char *modele, int api, float secondes, bool horsEcran = false);

} // namespace nkanima
