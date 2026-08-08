#pragma once
// -----------------------------------------------------------------------------
// @File    NkGraphDocument.h
// @Brief   Document multi-graphes : sous-graphes reutilisables et PLAN APLATI.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI UN DOCUMENT PLUTOT QU'UN GRAPHE QUI EN CONTIENT D'AUTRES
//   Un sous-graphe n'est pas « un graphe range dans un noeud » : c'est une
//   BRIQUE NOMMEE, definie une fois et INSTANCIEE plusieurs fois. C'est ainsi
//   que fonctionnent les groupes de noeuds de Blender et les graphes replies
//   d'Unreal, et c'est ce que veut l'utilisateur : corriger le groupe une fois,
//   et voir les cinq instances se corriger.
//
//   Techniquement, ca evite aussi qu'un `NkNodeGraph` se contienne lui-meme —
//   un type recursif que NkVector ne peut pas instancier proprement.
//
// LES TROIS CLES DE STRUCTURE ci-dessous sont-elles du « metier » interdit par
//   le garde-fou n°1 ? Non. Elles decrivent la structure DU GRAPHE lui-meme —
//   ou il commence, ou il finit, ou il est instancie. Le garde-fou interdit les
//   types du DOMAINE (« texture », « os », « son »), pas la grammaire du graphe.
//
// LE PLAN APLATI EST LE LIVRABLE REEL. Un consommateur ne veut pas parcourir des
//   frontieres de groupes a chaque evaluation : il veut une liste lineaire ou les
//   instances et les frontieres ONT DISPARU, et ou chaque entree pointe
//   directement sur l'etape qui produit sa valeur. C'est ce que rend BuildPlan.
// -----------------------------------------------------------------------------

#include "NKGraph/NkNodeGraph.h"

namespace nkentseu {
	namespace graph {

		// Cles de structure. Un noeud d'INSTANCE porte le nom du graphe appele dans
		// son champ `subgraph` ; les noeuds d'ENTREE et de SORTIE forment la
		// frontiere interieure du groupe, exactement comme « Group Input » et
		// « Group Output » chez Blender.
		static const char *const NK_NODE_INSTANCE = "graph.instance";
		static const char *const NK_NODE_GROUP_IN = "graph.entree";
		static const char *const NK_NODE_GROUP_OUT = "graph.sortie";

		// Entree NON CONNECTEE. La valeur n'est PAS 0 : 0 est un index d'etape
		// parfaitement valide (la premiere), et une sentinelle a 0 ferait lire la
		// sortie de la premiere etape a chaque entree libre — un defaut qui produit
		// un resultat plausible.
		static const uint32 NK_EVAL_NO_SOURCE = 0xFFFFFFFFu;

		struct NkEvalInput {
				int32 dstSocket = -1;
				uint32 srcStep = NK_EVAL_NO_SOURCE; ///< index dans NkEvalPlan::steps
				int32 srcSocket = -1;
		};

		struct NkEvalStep {
				uint32 graph = 0; ///< index du graphe d'origine dans le document
				NkNodeId node = NK_NODE_INVALID;
				uint32 depth = 0; ///< profondeur d'imbrication (0 = graphe racine)
				NkString path;	  ///< chemin d'instanciation, pour les messages d'erreur
				NkVector<NkEvalInput> inputs;
		};

		struct NkEvalPlan {
				NkVector<NkEvalStep> steps;
				// Rempli en cas d'echec : QUEL socket, DANS QUEL groupe. Un code
				// d'erreur seul obligerait l'utilisateur a chercher lui-meme, dans un
				// document qui peut compter des dizaines de groupes.
				NkString errorDetail;
				void Clear() {
					steps.Clear();
					errorDetail = NkString("");
				}
				uint32 Size() const {
					return (uint32)steps.Size();
				}
		};

		enum class NkPlanError : uint8 {
			Ok = 0,
			EmptyDocument,
			UnknownSubgraph,   ///< un noeud d'instance nomme un graphe absent
			RecursiveSubgraph, ///< un groupe s'instancie lui-meme, direct ou non
			Cycle,			   ///< cycle a l'interieur d'un graphe
			TooDeep,
			// Les sockets du noeud d'instance ne correspondent pas a l'interface du
			// groupe : nom absent, en trop, ou type different. Ce cas se produit des
			// qu'un groupe est MODIFIE APRES avoir ete instancie — et sans ce
			// controle il passerait inapercu, en debranchant silencieusement une
			// entree.
			InterfaceMismatch,
		};

		const char *NkPlanErrorName(NkPlanError e);

		class NkGraphDocument {
			public:
				uint32 AddGraph(const char *name);
				int32 FindGraph(const char *name) const;
				uint32 GraphCount() const {
					return (uint32)mGraphs.Size();
				}
				NkNodeGraph &GraphAt(uint32 i) {
					return mGraphs[i];
				}
				const NkNodeGraph &GraphAt(uint32 i) const {
					return mGraphs[i];
				}
				const NkString &GraphName(uint32 i) const {
					return mNames[i];
				}
				void SetRoot(uint32 i) {
					mRoot = i;
				}
				uint32 Root() const {
					return mRoot;
				}

				// APLATISSEMENT. Les noeuds d'instance et les noeuds de frontiere
				// DISPARAISSENT du plan : chaque entree pointe directement sur l'etape
				// qui produit sa valeur, meme si la source se trouve trois groupes plus
				// loin. Un plan qui garderait les frontieres obligerait chaque
				// consommateur a refaire ce travail, et a le refaire differemment.
				NkPlanError BuildPlan(NkEvalPlan &out) const;

				void Serialize(NkString &out) const;
				bool Deserialize(const char *text);
				void Clear();

			private:
				NkVector<NkNodeGraph> mGraphs;
				NkVector<NkString> mNames;
				uint32 mRoot = 0;
		};

	} // namespace graph
} // namespace nkentseu

#include "NKGraph/NkGraphDocument.inl"
