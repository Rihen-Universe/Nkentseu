//
// NkEditeurGraphe.h
// =============================================================================
// Description :
//   LA PAGE DU GRAPHE d'un Blueprint (document 01, § 4.7) : un double-clic sur
//   un .nkbp du Contenu l'ouvre a la place du viseur. Une palette des noeuds a
//   gauche (recherche, categories), le canevas du kit (NkCanevasNoeuds) au
//   centre, la barre « Compiler / Enregistrer / Fermer » en haut, le resultat
//   de la compilation en bas -- une erreur ENTOURE son noeud en rouge.
//
// Caracteristiques :
//   - Compiler = compiler + ENREGISTRER le .nkbp (graphe et module) + le
//     donner au registre : pendant JEU, le comportement change tout de suite
//     (l'hote migre ses instances, les variables restent).
//   - Annuler / refaire (Ctrl+Z / Ctrl+Y) par NkGraphHistory, tant que la
//     souris est sur la page. Ctrl+S compile et enregistre.
//   - Clic droit dans le vide : le prochain noeud de la palette s'y pose.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURGRAPHE_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURGRAPHE_H__

#include "NKContainers/String/NkString.h"
#include "NKEditorKit/Components/NkCanevasNoeuds.h"
#include "NKGraph/NkNodeGraph.h"

namespace nkentseu {
	namespace editeur {

		struct NkEditeurScripts;
		struct NkEditeurModele;
		struct NkEditeurCadre;

		struct NkEditeurGrapheEtat {
				bool ouvert = false;
				NkString chemin; ///< le .nkbp, absolu
				NkString ref;	 ///< « Contenu/Scripts/Porte.nkbp »
				graph::NkNodeGraph graphe;
				graph::NkGraphHistory historique;
				editorkit::NkEtatCanevas canevas;
				bool modifie = false;
				NkString message;
				bool erreur = false;
				char filtre[48] = {};
				bool filtreFocus = false;
				float32 defil = 0.f;
				bool ajoutPose = false; ///< un clic droit a choisi le point du prochain noeud
				float32 ajoutX = 0.f, ajoutY = 0.f;
				bool cadrer = true;
				/// Le rectangle de chaque entree de la palette a la derniere trame
				/// (les bancs y visent), et le canevas.
				NkVector<nkgui::NkRect> paletteRects;
				NkVector<int32> paletteProtos;
				nkgui::NkRect zoneCanevas{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect boutonCompiler{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect boutonFermer{0.f, 0.f, 0.f, 0.f};
		};

		/// Ouvre le .nkbp `chemin` (absolu) dans la page. false : illisible (annonce).
		bool NkEditeurOuvrirGraphe(NkEditeurScripts &s, NkEditeurModele &m, const char *chemin);
		void NkEditeurFermerGraphe(NkEditeurScripts &s);
		/// Compile, enregistre et donne au registre. false : l'erreur est dans
		/// `s.graphe.message`, son noeud entoure.
		bool NkEditeurCompilerGraphe(NkEditeurScripts &s, NkEditeurModele &m);
		/// La page est-elle ouverte ? (le viseur lui cede sa place)
		bool NkEditeurGrapheOuvert(const NkEditeurModele &m);
		/// Dessine la page dans la colonne centrale (ui.vue).
		void NkEditeurDessinerGraphe(NkEditeurCadre &c);
		/// Pose un noeud du catalogue au point d'ajout (ou au centre de la vue).
		graph::NkNodeId NkEditeurGrapheAjouter(NkEditeurScripts &s, const char *type);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURGRAPHE_H__
