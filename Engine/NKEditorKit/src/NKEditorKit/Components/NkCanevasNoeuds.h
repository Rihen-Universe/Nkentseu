//
// NkCanevasNoeuds.h
// =============================================================================
// Description :
//   LA TOILE NODALE du kit (couche 2 de NKGraph : « le canevas, dans
//   NKEditorKit »). Elle dessine un graph::NkNodeGraph et l'EDITE, a la maniere
//   de l'editeur de Blueprint d'Unreal 5 : choisir (clic, Ctrl / Maj + clic,
//   rectangle), deplacer, tirer un fil d'une prise a une autre (refus NOMME par
//   NkLinkErrorName), lacher un fil dans le VIDE (le menu « sensible au
//   contexte »), couper les fils d'une prise (Alt + clic, clic droit), saisir
//   la valeur d'une entree libre, double-clic sur un fil (un relais),
//   commentaires (touche C), copier / couper / coller / dupliquer, aligner
//   (Maj + W A S D, Q redresse), Suppr, se deplacer (bouton droit ou du milieu)
//   et zoomer (molette).
//
// Caracteristiques :
//   - GENERIQUE : elle ne connait aucun domaine. Natures, formes, libelles,
//     couleurs des types, saisie des valeurs, trace d'execution, relais et
//     commentaires viennent du DOMAINE (couche 3) par NkDomaineCanevas. Toutes
//     les couleurs et longueurs sont des JETONS (NkStyleNodal.h) : un seul
//     endroit pour suivre une charte.
//   - LA TRACE : un domaine qui sait combien d'evenements sont passes par un
//     noeud (« Simuler ») le dit par Compte() : la toile fait BRILLER les fils
//     d'execution qui ont servi, met « x3 » en en-tete, ESTOMPE les noeuds
//     jamais atteints (« x0 » rouge).
//   - Mode immediat : l'etat (vue, choix, geste en cours) est a l'appelant
//     (NkEtatCanevas), qui lit en retour `modifie` (son historique), les
//     DEMANDES (menu de la toile, menu d'un noeud, menu depuis une prise) et
//     `clavierPris`.
//   - Peinte avec nkgui::NkGuiDrawList ; les fils sont des cubiques
//     ECHANTILLONNEES (G4 du document 01).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_NKEDITORKIT_NKCANEVASNOEUDS_H__
#define __NKENTSEU_NKEDITORKIT_NKCANEVASNOEUDS_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKEditorKit/Components/NkStyleNodal.h"
#include "NKGraph/NkNodeGraph.h"
#include "NKGui/Core/NkGuiDrawList.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKGui/Core/NkGuiInput.h"

namespace nkentseu {
	namespace editorkit {

		/// Le style de la toile : les JETONS (NkStyleNodal.h).
		using NkStyleCanevas = NkJetonsNodal;

		/// Le genre du CHAMP d'une entree libre (charte, planche 02 : un booleen
		/// est une case a cocher, « jamais un champ » ; une couleur, un nuancier).
		enum class NkGenreChamp : uint8 { NK_TEXTE = 0, NK_CASE, NK_NUANCIER, NK_AUCUN };

		/// Ce que le DOMAINE dit a la toile. Chaque rappel est facultatif.
		struct NkDomaineCanevas {
				void *donnees = nullptr;
				/// La couleur d'une prise de donnees de type `t`.
				nkgui::NkColor (*CouleurType)(void *d, const graph::NkNodeGraph &g, graph::NkTypeId t) = nullptr;
				/// (ancien) La couleur de l'en-tete d'un noeud -- prioritaire sur Nature.
				nkgui::NkColor (*CouleurEntete)(void *d, const graph::NkNode &n) = nullptr;
				/// La NATURE d'un noeud (la couleur de son en-tete vient des jetons).
				NkNatureNoeud (*Nature)(void *d, const graph::NkNode &n) = nullptr;
				/// La FORME d'un noeud (normal, compact, relais, commentaire).
				NkFormeNoeud (*Forme)(void *d, const graph::NkNode &n) = nullptr;
				/// Le libelle AFFICHE d'une prise (vide : aucun) ; nul : son nom.
				NkString (*LibellePrise)(void *d, const graph::NkNodeGraph &g, const graph::NkNode &n, const graph::NkSocket &s) = nullptr;
				/// Le SOUS-TITRE d'un noeud (« Cible : Soi »...), vide : aucun.
				NkString (*SousTitre)(void *d, const graph::NkNodeGraph &g, const graph::NkNode &n) = nullptr;
				/// La valeur d'une entree LIBRE en texte ; vide = pas de champ.
				NkString (*TexteDefaut)(void *d, const graph::NkNodeGraph &g, const graph::NkNode &n,
										const graph::NkSocket &s) = nullptr;
				/// Applique une saisie a l'entree `prise`. false : refusee.
				bool (*PoserDefaut)(void *d, graph::NkNodeGraph &g, graph::NkNodeId n, const char *prise,
									const char *texte) = nullptr;
				/// LA TRACE : combien d'evenements sont passes par `n` (-1 : pas de
				/// trace, la toile dessine comme d'habitude).
				int32 (*Compte)(void *d, const graph::NkNodeGraph &g, const graph::NkNode &n) = nullptr;
				/// Un RELAIS du type `type` (« exec », « reel »...) a (x, y) du graphe.
				graph::NkNodeId (*CreerRelais)(void *d, graph::NkNodeGraph &g, const char *type, float32 x, float32 y) = nullptr;
				/// Un COMMENTAIRE de (x, y, w, h).
				graph::NkNodeId (*CreerCommentaire)(void *d, graph::NkNodeGraph &g, float32 x, float32 y, float32 w, float32 h) = nullptr;
				/// La taille d'un commentaire, et la poser.
				void (*TailleCommentaire)(void *d, const graph::NkNodeGraph &g, const graph::NkNode &n, float32 &w, float32 &h) = nullptr;
				void (*PoserTailleCommentaire)(void *d, graph::NkNodeGraph &g, graph::NkNodeId n, float32 w, float32 h) = nullptr;
				/// Ce noeud peut-il etre copie, coupe, supprime ? (l'entree d'une
				/// fonction ne l'est pas). Nul : oui.
				bool (*Supprimable)(void *d, const graph::NkNode &n) = nullptr;
				/// Le genre du champ d'une entree libre (nul : texte).
				NkGenreChamp (*Champ)(void *d, const graph::NkNodeGraph &g, const graph::NkNode &n, const graph::NkSocket &s) = nullptr;
				/// La largeur minimale d'un noeud (unites du graphe ; 0 : celle des jetons).
				float32 (*LargeurMin)(void *d, const graph::NkNode &n) = nullptr;
				/// UN BLOC DE CORPS pleine largeur, entre les entrees et les sorties de
				/// donnees : le CODE d'un noeud Expression, un apercu (charte : « un
				/// BLOC DE CORPS escamotable en pleine largeur »). Sa hauteur (unites
				/// du graphe ; 0 : aucun bloc) pour une largeur de noeud donnee...
				float32 (*HauteurBloc)(void *d, const graph::NkNodeGraph &g, const graph::NkNode &n, float32 largeur) = nullptr;
				/// ... et son dessin, dans `r` (ECRAN) : le domaine le fait vivre
				/// (clic, saisie). La souris dans le bloc n'est PAS prise par la toile.
				/// Rend true si le graphe a change.
				bool (*Bloc)(void *d, nkgui::NkGuiDrawList &dl, nkgui::NkGuiInput &in, nkgui::NkGuiFont *f, graph::NkNodeGraph &g,
							 graph::NkNodeId n, const nkgui::NkRect &r, float32 zoom, bool survole) = nullptr;
				/// Le message d'erreur ECRIT dans le noeud (charte : « la raison est
				/// LISIBLE sans survol ») ; vide : aucun. S'ajoute a NkEtatCanevas::enErreur.
				NkString (*Erreur)(void *d, const graph::NkNodeGraph &g, const graph::NkNode &n) = nullptr;
		};

		/// Une demande que la toile ne sait pas satisfaire seule.
		enum class NkDemandeCanevas : uint8 {
			NK_AUCUNE = 0,
			NK_MENU_TOILE,	///< clic droit dans le vide : le menu des noeuds
			NK_MENU_PRISE,	///< un fil tire LACHE dans le vide : le menu sensible au contexte
			NK_MENU_NOEUD,	///< clic droit sur un noeud : son menu
			NK_DOUBLE_CLIC, ///< double-clic sur un noeud (ouvrir une fonction...)
			NK_PORTEE		///< clic sur le selecteur de « Portee »
		};

		/// Les outils de la barre verticale.
		enum class NkOutilCanevas : uint8 { NK_SELECTION = 0, NK_MAIN, NK_COMMENTAIRE, NK_RELAIS, NK_CADRER, NK_NOMBRE };

		/// La geometrie d'un noeud a la derniere trame (en unites du GRAPHE).
		struct NkGeomNoeud {
				graph::NkNodeId id = graph::NK_NODE_INVALID;
				float32 w = 0.f, h = 0.f;
				float32 entete = 0.f; ///< la hauteur de l'en-tete (sous-titre compris)
				NkFormeNoeud forme = NkFormeNoeud::NK_NORMAL;
				/// La hauteur (depuis le haut du noeud) du CENTRE de chaque prise ;
				/// la premiere entree d'execution est sur l'en-tete.
				NkVector<float32> yPrises;
				float32 blocY = 0.f, blocH = 0.f; ///< le bloc de corps (0 : aucun)
				float32 erreurY = 0.f;			  ///< la rangee du message d'erreur (0 : aucune)
				NkString erreur;
		};

		/// L'etat de la toile, garde par l'appelant d'une trame a l'autre.
		struct NkEtatCanevas {
				/// Le point du graphe au coin haut-gauche de la zone, et le zoom.
				float32 vueX = -40.f, vueY = -40.f;
				float32 zoom = 1.f;
				/// Le noeud CHOISI en dernier (les Details le montrent)...
				graph::NkNodeId selection = graph::NK_NODE_INVALID;
				/// ... et TOUT le choix (Ctrl / Maj + clic, rectangle).
				NkVector<graph::NkNodeId> choix;
				/// Pose par l'appelant : le noeud d'une erreur, entoure de rouge, et
				/// son message (une banniere sous le noeud).
				graph::NkNodeId enErreur = graph::NK_NODE_INVALID;
				NkString messageErreur;
				NkOutilCanevas outil = NkOutilCanevas::NK_SELECTION;
				/// Le libelle du selecteur de « Portee » (vide : pas de selecteur).
				NkString portee;
				/// Montrer la barre d'outils verticale.
				bool barreOutils = true;
				/// (2026-10-02) La largeur de la GOUTTIERE qui la porte, a gauche : le
				/// graphe commence a sa droite (NkCanevasVersEcran en tient compte),
				/// la barre ne couvre jamais un noeud. Sans barre : aucune.
				float32 gouttiere = 48.f;
				/// Pose par l'appelant : un AUTRE champ a le clavier (le code d'un
				/// noeud, un panneau) -- la toile ne prend aucun raccourci.
				bool clavierAilleurs = false;

				// --- Le geste en cours -------------------------------------------
				/// 0 aucun, 1 deplacer des noeuds, 2 tirer un fil, 3 vue, 4 rectangle,
				/// 5 commentaire (deplacer avec son contenu), 6 taille d'un commentaire,
				/// 7 dessiner un commentaire (outil).
				int32 geste = 0;
				graph::NkNodeId gesteNoeud = graph::NK_NODE_INVALID;
				int32 gestePrise = -1;
				float32 gesteDx = 0.f, gesteDy = 0.f; ///< saisie (noeud) ou depart (vue)
				float32 appuiX = 0.f, appuiY = 0.f;
				int32 bouton = 0;
				bool gesteBouge = false;
				NkVector<graph::NkNodeId> gesteGroupe; ///< les noeuds deplaces ensemble
				NkVector<float32> gesteOrigines;	   ///< leurs (x, y) au depart

				// --- La saisie d'une valeur --------------------------------------
				graph::NkNodeId saisieNoeud = graph::NK_NODE_INVALID;
				int32 saisiePrise = -1;
				char saisie[96] = {};

				// --- Ce que la trame rend ----------------------------------------
				bool modifie = false; ///< le graphe a change (a retenir dans l'historique)
				NkDemandeCanevas demande = NkDemandeCanevas::NK_AUCUNE;
				/// Ou, en coordonnees du GRAPHE (le noeud pose y ira).
				float32 menuX = 0.f, menuY = 0.f;
				/// Ou, a l'ECRAN (le menu s'y ouvre).
				float32 menuEcranX = 0.f, menuEcranY = 0.f;
				/// NK_MENU_PRISE : la prise d'ou part le fil (le nouveau noeud s'y
				/// relie) ; NK_MENU_NOEUD, NK_DOUBLE_CLIC : le noeud.
				graph::NkNodeId menuNoeud = graph::NK_NODE_INVALID;
				int32 menuPrise = -1;
				/// (compat) clic droit dans le vide.
				bool menuDemande = false;
				/// Le fil LACHE dans le vide reste dessine (pointille) tant que
				/// l'APPELANT le garde -- son menu sensible au contexte ouvert, comme
				/// UE5 ; il le pose a chaque trame (NK_NODE_INVALID : aucun).
				graph::NkNodeId filAttenteNoeud = graph::NK_NODE_INVALID;
				int32 filAttentePrise = -1;
				float32 filAttenteX = 0.f, filAttenteY = 0.f; ///< le lacher (graphe)
				NkString refus; ///< le dernier fil refuse, et pourquoi
				float32 ageRefus = 99.f;
				bool clavierPris = false; ///< la toile a pris le clavier (Suppr, saisie, raccourcis)
				graph::NkLinkId filSurvole = 0;

				// --- Presse-papiers (Ctrl+C / X / V / D) ----------------------------
				NkString pressePapiers;

				// --- La geometrie de la derniere trame (bancs, menus) --------------
				NkVector<NkGeomNoeud> geom;
				nkgui::NkRect rectPortee{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect rectOutils[static_cast<uint32>(NkOutilCanevas::NK_NOMBRE)] = {};

				bool EstChoisi(graph::NkNodeId n) const noexcept {
					for (uint32 i = 0; i < choix.Size(); ++i) {
						if (choix[i] == n) {
							return true;
						}
					}
					return false;
				}
				void Choisir(graph::NkNodeId n) {
					choix.Clear();
					selection = n;
					if (n != graph::NK_NODE_INVALID) {
						choix.PushBack(n);
					}
				}
		};

		/// Dessine le graphe dans `zone` et applique les gestes de `in` (dont la
		/// souris est dans `zone`). `police` peut etre nulle (pas de texte).
		void NkCanevasNoeuds(nkgui::NkGuiDrawList &dl, nkgui::NkGuiInput &in, nkgui::NkGuiFont *police,
							 const nkgui::NkRect &zone, graph::NkNodeGraph &g, NkEtatCanevas &e, const NkStyleCanevas &s,
							 const NkDomaineCanevas &d);

		// --- La geometrie (bancs, captures, menus) ------------------------------
		nkgui::NkVec2 NkCanevasVersEcran(const NkEtatCanevas &e, const nkgui::NkRect &zone, float32 x, float32 y) noexcept;
		nkgui::NkVec2 NkCanevasDepuisEcran(const NkEtatCanevas &e, const nkgui::NkRect &zone, const nkgui::NkVec2 &p) noexcept;
		/// Le rectangle A L'ECRAN de `n` (sa taille de la derniere trame, sinon la
		/// largeur minimale).
		nkgui::NkRect NkCanevasRectNoeud(const NkEtatCanevas &e, const nkgui::NkRect &zone, const graph::NkNode &n,
										 const NkStyleCanevas &s) noexcept;
		/// La position A L'ECRAN de la prise `k` de `n`. false : pas de prise k.
		bool NkCanevasPrise(const NkEtatCanevas &e, const nkgui::NkRect &zone, const graph::NkNode &n, int32 k,
							const NkStyleCanevas &s, nkgui::NkVec2 &sortie) noexcept;
		/// Cadre le graphe dans la zone (zoom borne a [0,7 ; 1,25] : lisible ; plus
		/// grand que la vue, il part de son coin haut-gauche).
		void NkCanevasCadrer(NkEtatCanevas &e, const nkgui::NkRect &zone, const graph::NkNodeGraph &g,
							 const NkStyleCanevas &s) noexcept;

		// --- Les gestes, aussi en API (menus, bancs) ------------------------------
		/// Copie le CHOIX (noeuds et fils internes) en texte .nkgraph.
		NkString NkCanevasCopier(const graph::NkNodeGraph &g, const NkEtatCanevas &e, const NkDomaineCanevas &d);
		/// Colle `texte` (rendu par NkCanevasCopier) avec son coin haut-gauche en
		/// (x, y) du graphe ; les noeuds colles deviennent le choix. Rend leur nombre.
		uint32 NkCanevasColler(graph::NkNodeGraph &g, NkEtatCanevas &e, const char *texte, float32 x, float32 y);
		/// Supprime le choix (sauf l'insupprimable). Rend le nombre supprime.
		uint32 NkCanevasSupprimerChoix(graph::NkNodeGraph &g, NkEtatCanevas &e, const NkDomaineCanevas &d);
		/// Aligne le choix : 0 gauche, 1 droite, 2 haut, 3 bas, 4 centre
		/// horizontal (une colonne), 5 centre vertical (une ligne).
		void NkCanevasAligner(graph::NkNodeGraph &g, NkEtatCanevas &e, int32 mode);
		/// Redresse (Q) : chaque noeud choisi s'aligne sur la prise qui le nourrit.
		void NkCanevasRedresser(graph::NkNodeGraph &g, NkEtatCanevas &e, const NkStyleCanevas &s);
		/// Le rectangle (graphe) qui entoure le choix, marge comprise. false : vide.
		bool NkCanevasBoiteChoix(const graph::NkNodeGraph &g, const NkEtatCanevas &e, const NkStyleCanevas &s, float32 &x,
								 float32 &y, float32 &w, float32 &h);

	} // namespace editorkit
} // namespace nkentseu

#endif // __NKENTSEU_NKEDITORKIT_NKCANEVASNOEUDS_H__
