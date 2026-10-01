//
// NkEditeurGraphe.h
// =============================================================================
// Description :
//   LA PAGE DU GRAPHE d'un Blueprint (document 01, § 4.7) : un double-clic sur
//   un .nkbp du Contenu l'ouvre a la place du viseur, DANS SON ONGLET (2026-10-02,
//   NkEditeurDocuments.h) : chaque Blueprint ouvert a le sien, a cote de celui
//   de la scene, qui reste accessible d'un clic. Une palette des noeuds a
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
		struct NkEditeurInterface;

		/// UN Blueprint ouvert (son onglet de document).
		struct NkEditeurGrapheEtat {
				/// Son identite dans la barre des documents (NkEditeurDocuments.h).
				nk_uint64 id = 0;
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

		/// Ouvre le .nkbp `chemin` (absolu) dans SA page, qui devient le graphe
		/// COURANT (celui que la page montre, que Compiler et Fermer visent). Deja
		/// ouvert : il redevient courant (pas de second onglet). `ui` non nul : son
		/// onglet passe au premier plan (NkEditeurActiverDocument) ; nul : un banc
		/// sans interface. false : illisible (annonce).
		bool NkEditeurOuvrirGraphe(NkEditeurScripts &s, NkEditeurModele &m, const char *chemin, NkEditeurInterface *ui = nullptr);
		/// Ferme le graphe COURANT, sans interface (bancs). Avec elle, passer par la
		/// barre (NkEditeurFermerDocument) : le voisin passe devant.
		void NkEditeurFermerGraphe(NkEditeurScripts &s);
		/// Le Blueprint `id`, ou nul.
		NkEditeurGrapheEtat *NkEditeurGrapheParId(NkEditeurScripts &s, nk_uint64 id);
		/// `id` devient le graphe courant. false : il n'existe pas.
		bool NkEditeurGrapheDevenirCourant(NkEditeurScripts &s, nk_uint64 id);
		/// Retire le Blueprint `id` (la barre l'a deja fait passer derriere).
		bool NkEditeurDetruireGraphe(NkEditeurScripts &s, nk_uint64 id);
		/// Compile, enregistre et donne au registre le graphe COURANT. false :
		/// l'erreur est dans `s.Graphe().message`, son noeud entoure.
		bool NkEditeurCompilerGraphe(NkEditeurScripts &s, NkEditeurModele &m);
		/// Un Blueprint est-il ouvert (un graphe courant existe) ?
		bool NkEditeurGrapheOuvert(const NkEditeurModele &m);
		/// Dessine la page du graphe courant dans la colonne centrale (ui.vue) --
		/// quand son onglet est au premier plan (NkEditeurBlueprintDevant).
		void NkEditeurDessinerGraphe(NkEditeurCadre &c);
		/// Pose un noeud du catalogue au point d'ajout (ou au centre de la vue).
		graph::NkNodeId NkEditeurGrapheAjouter(NkEditeurScripts &s, const char *type);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURGRAPHE_H__
