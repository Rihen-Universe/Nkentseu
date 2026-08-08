#pragma once
// -----------------------------------------------------------------------------
// @File    NkNodeGraph.h
// @Brief   Coeur du substrat de graphe de noeuds — modele de donnees PUR.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// DECISION D'ARCHITECTURE (cf. la roadmap NKGraph)
//   UN SEUL systeme de graphe pour tout l'ecosysteme : materiaux, VFX,
//   blueprint/scratch, modelisation procedurale, graphes d'animation, futur rig.
//   Trois couches, et la separation EST la regle :
//     3 — semantique metier : chez chaque consommateur (bibliotheques de noeuds,
//         backends d'execution) ;
//     2 — edition : le canevas, dans NKEditorKit ;
//     1 — CE MODULE : le modele pur.
//
//   « On unifie l'AUTORAT — modele, serialisation, interface — JAMAIS
//   l'EXECUTION. » Les materiaux se COMPILENT vers NkSL a l'edition ; la
//   modelisation PRODUIT DES COMMANDES rejouables ; le VFX EVALUE un plan aplati
//   par frame. Ces besoins sont incompatibles : le coeur fournit l'ordre
//   d'evaluation et les types, chaque domaine fournit ses noeuds et ce qu'il en
//   fait.
//
// GARDE-FOU N°1, ecrit dans la roadmap et tenu ici : AUCUN TYPE METIER dans le
//   coeur. Pas de « texture », pas d'« os », pas de « son ». Les types de sockets
//   sont des IDENTIFIANTS enregistres par les consommateurs. Un `if (type == …)`
//   metier dans ce fichier signerait la mort de l'architecture.
//
// POURQUOI « NkNodeGraph » ET NON « NkGraph » : `nkentseu::NkGraph<V, Alloc>`
//   EXISTE DEJA — c'est le graphe pondere generique de NKContainers (sommets,
//   aretes, DFS/BFS, 877 lignes). Deux choses differentes ne peuvent pas porter le
//   meme nom dans le meme espace de noms. Ce module-ci est un graphe de NOEUDS a
//   SOCKETS TYPES ; le conteneur ne l'est pas et ne doit pas etre detourne.
//
// EN-TETE PUR, sans cible de build, pour la meme raison que NkShortcutTable :
//   le harnais peut le tester sans lier quoi que ce soit. La cible deviendra
//   legitime quand un consommateur reel la liera.
//
// ZERO-STL : NkVector/NkString.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace graph {

		// Identifiant de TYPE de socket. Le coeur ne sait pas ce qu'il designe : il
		// sait seulement que deux sockets de meme identifiant sont compatibles.
		// C'est ce qui lui permet de servir les materiaux et la modelisation sans
		// connaitre ni les textures ni les maillages.
		using NkTypeId = uint32;
		static const NkTypeId NK_TYPE_INVALID = 0;

		// Identifiants STABLES. Ils survivent aux suppressions et aux
		// reordonnancements — c'est ce qui permet a une sauvegarde, a une courbe
		// d'animation ou a une annulation de designer un noeud sans se casser au
		// premier remaniement. Meme regle que les modificateurs et les raccourcis.
		using NkNodeId = uint32;
		using NkLinkId = uint32;
		static const NkNodeId NK_NODE_INVALID = 0;

		enum class NkSocketDir : uint8 { Input = 0, Output = 1 };

		struct NkSocket {
				NkString name;					  ///< CLE stable, jamais un libelle
				NkTypeId type = NK_TYPE_INVALID;
				NkSocketDir dir = NkSocketDir::Input;
		};

		struct NkNode {
				NkNodeId id = NK_NODE_INVALID;
				NkString type;			  ///< CLE du type de noeud, ex. « mesh.extrude »
				NkString label;			  ///< libelle affichable, traduisible
				// Renseigne UNIQUEMENT sur un noeud d'instance (type
				// NK_NODE_INSTANCE) : nom du graphe instancie dans le document.
				// Vide partout ailleurs.
				NkString subgraph;
				float32 x = 0.f, y = 0.f; ///< position dans le canevas (couche 2)
				NkVector<NkSocket> sockets;
				bool alive = true;

				int32 FindSocket(const char *name, NkSocketDir dir) const;
		};

		struct NkLink {
				NkLinkId id = 0;
				NkNodeId fromNode = NK_NODE_INVALID;
				int32 fromSocket = -1;
				NkNodeId toNode = NK_NODE_INVALID;
				int32 toSocket = -1;
				bool alive = true;
		};

		// Raison d'un refus de connexion. On REND une raison plutot qu'un simple
		// booleen : l'interface doit pouvoir DIRE pourquoi elle refuse au lieu de
		// laisser l'utilisateur deviner — meme regle que partout ailleurs dans le
		// produit.
		enum class NkLinkError : uint8 {
			Ok = 0,
			UnknownNode,
			UnknownSocket,
			SameNode,		   ///< un noeud ne se connecte pas a lui-meme
			DirectionMismatch, ///< sortie -> entree, jamais autre chose
			TypeMismatch,
			WouldCycle, ///< la connexion fermerait une boucle
		};

		const char *NkLinkErrorName(NkLinkError e);

		class NkNodeGraph {
			public:
				// ── TYPES ────────────────────────────────────────────────────────
				// Enregistres par les CONSOMMATEURS. Le coeur ne fait que comparer.
				NkTypeId RegisterType(const char *name);
				NkTypeId FindType(const char *name) const;
				const NkString *TypeName(NkTypeId t) const;

				// Conversion IMPLICITE autorisee : `from` peut alimenter `to`.
				// Declaree par le consommateur (ex. un flottant alimente un vecteur).
				// Le coeur ne l'invente jamais — deviner une conversion produirait des
				// graphes qui « marchent » et calculent autre chose.
				void AllowConversion(NkTypeId from, NkTypeId to);
				bool Accepts(NkTypeId socketType, NkTypeId valueType) const;

				// ── NOEUDS ───────────────────────────────────────────────────────
				NkNodeId AddNode(const char *type, const char *label = nullptr);
				bool AddSocket(NkNodeId n, const char *name, NkTypeId type, NkSocketDir dir);
				// Supprime le noeud ET toutes ses connexions. Laisser des liens
				// pendants serait pire qu'une suppression refusee : le graphe
				// paraitrait valide et s'evaluerait faux.
				bool RemoveNode(NkNodeId n);
				NkNode *Find(NkNodeId n);
				const NkNode *Find(NkNodeId n) const;
				uint32 NodeCount() const; ///< noeuds VIVANTS

				// Parcours BRUT, noeuds morts compris. Reserve aux traitements qui
				// doivent voir toute la table (validation, outillage). Le nom dit
				// « brut » pour qu'on ne l'utilise pas par megarde a la place de
				// NodeCount(), qui ne compte que les vivants.
				uint32 RawNodeCount() const {
					return (uint32)mNodes.Size();
				}
				const NkNode *RawNodeAt(uint32 i) const {
					return i < (uint32)mNodes.Size() ? &mNodes[i] : nullptr;
				}

				// ── CONNEXIONS ───────────────────────────────────────────────────
				// Une ENTREE n'accepte qu'UNE source : brancher une seconde REMPLACE
				// la premiere (comportement de Blender et d'Unreal). Refuser
				// obligerait a debrancher avant de rebrancher, pour aucun gain.
				NkLinkError Connect(NkNodeId from, const char *fromSocket, NkNodeId to, const char *toSocket,
									NkLinkId *outId = nullptr);
				bool Disconnect(NkLinkId link);
				uint32 LinkCount() const;
				const NkLink *LinkAt(uint32 i) const;
				const NkLink *IncomingOf(NkNodeId n, int32 socketIndex) const; ///< nullptr si l'entree est libre

				// ── ORDRE D'EVALUATION ───────────────────────────────────────────
				// Tri topologique : les producteurs avant les consommateurs. Renvoie
				// false s'il existe un CYCLE — et c'est un REFUS, pas un ordre
				// approximatif : evaluer un graphe cyclique boucle, ou produit un
				// resultat qui depend de l'ordre d'insertion.
				bool TopoSort(NkVector<NkNodeId> &out) const;
				bool HasCycle() const;

				// ── SERIALISATION `.nkgraph` ─────────────────────────────────────
				// Format TEXTE, une directive par ligne (cf. NkNodeGraphIO.inl). Un
				// graphe se relit, se compare avec `git diff` et se repare a la main ;
				// un format binaire ferait gagner des octets sur des fichiers qui
				// pesent quelques kilo-octets.
				void Serialize(NkString &out) const;
				bool Deserialize(const char *text);

				void Clear();

			private:
				bool WouldCreateCycle(NkNodeId from, NkNodeId to) const;

				NkVector<NkNode> mNodes;
				NkVector<NkLink> mLinks;
				NkVector<NkString> mTypeNames; ///< index 0 reserve = invalide
				NkVector<uint64> mConversions; ///< (from << 32) | to, DIRIGEE
				NkNodeId mNextNode = 1;
				NkLinkId mNextLink = 1;
		};

		// ── ANNULER / REFAIRE ────────────────────────────────────────────────
		// ECART ASSUME PAR RAPPORT A LA ROADMAP, qui disait « commandes
		// inversibles ». On garde des INSTANTANES serialises, pas des inverses.
		//
		// Pourquoi : l'inverse de « supprimer un noeud » doit restaurer le noeud,
		// TOUS ses liens, ET leurs identifiants d'origine. C'est precisement le
		// genre d'inverse qu'on ecrit presque juste, et dont l'erreur ne se voit
		// que trois manipulations plus tard. L'instantane est correct par
		// construction, puisqu'il reutilise une serialisation deja prouvee.
		//
		// Ce que ca coute, honnetement : memoire proportionnelle a
		// (taille du graphe x profondeur). Pour des graphes de quelques centaines
		// de noeuds c'est negligeable. Si un graphe reel devient assez gros pour
		// que ca compte, on passera aux inverses A CE MOMENT-LA — l'interface
		// publique ci-dessous ne changera pas.
		class NkGraphHistory {
			public:
				void Reset(const NkNodeGraph &g);  ///< etat initial, vide l'historique
				void Commit(const NkNodeGraph &g); ///< APRES chaque modification
				bool Undo(NkNodeGraph &g);
				bool Redo(NkNodeGraph &g);
				uint32 UndoDepth() const; ///< nombre d'annulations encore possibles
				uint32 RedoDepth() const;
				void SetLimit(uint32 n) {
					mLimit = n < 2u ? 2u : n;
				}

			private:
				NkVector<NkString> mStack;
				uint32 mCursor = 0;
				uint32 mLimit = 128;
		};

	} // namespace graph
} // namespace nkentseu

#include "NKGraph/NkNodeGraph.inl"
#include "NKGraph/NkNodeGraphIO.inl"
