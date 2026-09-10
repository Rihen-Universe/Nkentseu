#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerMeshMenu.h
// @Brief   LES COMMANDES DE MAILLAGE, declarees UNE FOIS, atteignables par
//          PLUSIEURS chemins : barre de menu, menu contextuel (clic droit),
//          clavier. Sensible au sous-mode sommet / arete / face, comme les trois
//          « Context Menu » distincts de Blender.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI CE FICHIER EXISTE
//   Huit operations de maillage FONCTIONNAIENT depuis toujours sans que personne
//   puisse les decouvrir : sept n'etaient accessibles qu'au clavier, une (bisect)
//   par aucun chemin. Le probleme n'a jamais ete « donner un acces » -- il etait
//   de RENDRE DECOUVRABLE ce qui existait deja.
//
// LA REGLE QUI TIENT CE FICHIER : UNE COMMANDE, PLUSIEURS ENTREES
//   Tous les chemins aboutissent a `NkMeshMenuRun`, qui appelle la facade de
//   NkDemo3D, qui pose l'operation dans le MEME entonnoir que la touche clavier
//   (`NkMeshEditCommand::Apply`). La logique n'est rejouee NULLE PART.
//   ⚠ Test de sante de la structure : ajouter un quatrieme chemin (une palette,
//   une barre d'outils, un script) doit couter UNE ligne d'appel. Si un jour ca
//   coute davantage, c'est la structure qu'il faut corriger, pas le chemin.
//
// ECRIT POUR ETRE COPIE
//   Sculpture, sculpture 2.5D et texturing auront le meme besoin, et « LES
//   ESPACES SONT LES MODES » : chaque menu vit dans l'onglet de son mode. Le
//   moule est ici -- une TABLE de declarations, un masque de modes, un seul
//   repartiteur. Copier ce fichier et changer la table doit suffire.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkEditorContextMenu.h"
#include "NKEditorKit/NkShortcutTable.h"
#include "NK3DModeler/Viewport/NkDemo3DHost.h"

namespace nkentseu {
	namespace nk3d {

		using editorkit::NkShortcutTable;

		// Sous-modes d'edition. MEME convention de bits que
		// `Demo3DHostEditSelMask` : 1 sommet, 2 arete, 4 face. Recopier une
		// convention en la decalant est le genre d'erreur qui ne se voit qu'a
		// l'usage, sur un seul des trois modes.
		enum NkMeshMenuMode : uint8 {
			NK_MM_VERTEX = 1,
			NK_MM_EDGE = 2,
			NK_MM_FACE = 4,
			NK_MM_ALL = 7,
		};

		// Identifiant STABLE d'une commande de maillage. Sert au repartiteur ; ce
		// n'est pas un indice de ligne de menu (le contenu change avec le mode).
		enum class NkMeshCmd : int32 {
			Extruder = 0,
			Inserer,
			Biseauter,
			Subdiviser,
			LoopCut,
			Fusionner,
			CreerFace,
			SeparerAretes,
			Spin,
			Bisect,
			Spheriser,
			Gonfler,
			Dissoudre,
			Supprimer,
		};

		struct NkMeshMenuEntry {
				NkMeshCmd cmd;
				const char *label;
				// Cle de NkShortcutTable, ou "" quand la commande n'a AUCUNE touche.
				// Le raccourci affiche est LU dans la table, jamais recopie ici :
				// une chaine recopiee peut mentir sans que rien ne le signale.
				const char *command;
				uint8 modes;	///< masque des sous-modes ou l'entree apparait
				bool needsSel;	///< grisee tant que rien n'est selectionne
		};

		// ── LA TABLE ────────────────────────────────────────────────────────────
		// Le CONTENU depend du mode (comme Blender : Vertex / Edge / Face Context
		// Menu n'ont pas les memes entrees), mais a l'interieur d'un mode, une
		// entree qui ne peut rien produire se GRISE -- elle ne disparait pas.
		// Disparaitre apprend a l'utilisateur que la commande n'existe pas ;
		// griser lui apprend qu'elle existe et ce qui lui manque.
		inline const NkMeshMenuEntry *NkMeshMenuTable(int32 &count) {
			static const NkMeshMenuEntry kT[] = {
				{NkMeshCmd::Extruder, "Extruder", "edit.extruder", NK_MM_ALL, true},
				{NkMeshCmd::Inserer, "Inserer une face", "edit.inserer", NK_MM_FACE, true},
				{NkMeshCmd::Biseauter, "Biseauter", "edit.biseauter", NK_MM_ALL, true},
				{NkMeshCmd::Subdiviser, "Subdiviser", "edit.subdiviser", NK_MM_ALL, true},
				{NkMeshCmd::LoopCut, "Loop cut", "edit.loop_cut", NK_MM_EDGE | NK_MM_FACE, true},
				{NkMeshCmd::Fusionner, "Fusionner", "edit.fusionner", NK_MM_VERTEX, true},
				{NkMeshCmd::CreerFace, "Creer une face", "edit.creer_face",
				 NK_MM_VERTEX | NK_MM_EDGE, true},
				// ── SEPARER LES ARETES ──────────────────────────────────────────
				// Nomme d'apres ce qu'elle FAIT, mesure avant d'etre nommee : elle
				// delie les faces voisines le long des aretes selectionnees, ce qui
				// est `mesh.edge_split` chez Blender -- et `mesh.edge_split` n'a
				// AUCUN raccourci par defaut. D'ou `""`. Le depot employait deja ce
				// nom pour le modificateur equivalent.
				{NkMeshCmd::SeparerAretes, "Separer les aretes", "", NK_MM_EDGE, true},
				// Spin et Bisect : sans touche, comme chez Blender (outil de barre
				// laterale et entree de menu). Leur absence de raccourci n'est pas
				// un oubli, c'est la conformite.
				{NkMeshCmd::Spin, "Spin (revolution)", "", NK_MM_ALL, true},
				{NkMeshCmd::Bisect, "Couper (bisect)", "", NK_MM_ALL, false},
				{NkMeshCmd::Spheriser, "Spheriser", "edit.spheriser", NK_MM_ALL, true},
				{NkMeshCmd::Gonfler, "Gonfler / retrecir", "edit.gonfler", NK_MM_ALL, true},
				{NkMeshCmd::Dissoudre, "Dissoudre", "edit.dissoudre", NK_MM_ALL, true},
				{NkMeshCmd::Supprimer, "Supprimer", "edit.supprimer", NK_MM_ALL, true},
			};
			count = (int32)(sizeof(kT) / sizeof(kT[0]));
			return kT;
		}

		// ── LE REPARTITEUR : LE SEUL POINT D'ARRIVEE ────────────────────────────
		// Chaque cas est UN appel de facade. Les operations a apercu passent par
		// le cadre MODAL -- exactement ce que fait la touche du clavier, qui pose
		// le meme `modalStartPending`. Deux chemins, un seul comportement.
		inline bool NkMeshMenuRun(NkMeshCmd c) {
			switch (c) {
				case NkMeshCmd::Extruder: return demo::Demo3DHostEditModal(6);
				case NkMeshCmd::Inserer: return demo::Demo3DHostEditModal(3);
				case NkMeshCmd::Biseauter: return demo::Demo3DHostEditModal(1);
				case NkMeshCmd::LoopCut: return demo::Demo3DHostEditModal(4);
				case NkMeshCmd::Spin: return demo::Demo3DHostEditModal(5);
				case NkMeshCmd::Spheriser: return demo::Demo3DHostEditModal(7);
				case NkMeshCmd::Gonfler: return demo::Demo3DHostEditModal(8);
				case NkMeshCmd::Subdiviser: return demo::Demo3DHostEditSubdivide();
				case NkMeshCmd::Fusionner: return demo::Demo3DHostEditMerge();
				case NkMeshCmd::CreerFace: return demo::Demo3DHostEditMakeFace();
				case NkMeshCmd::SeparerAretes: return demo::Demo3DHostEditEdgeSplit();
				case NkMeshCmd::Bisect: return demo::Demo3DHostArmKnife();
				case NkMeshCmd::Dissoudre: return demo::Demo3DHostEditDissolve();
				case NkMeshCmd::Supprimer: return demo::Demo3DHostEditDelete();
			}
			return false;
		}

		// Capacite du menu construit. La table en compte 14 : 24 laisse la place
		// aux prochaines sans reflechir, et le depassement est BORNE, pas ignore.
		enum { kMeshMenuCap = 24 };

		// Construit les tableaux paralleles attendus par `NkCtxMenuDraw`.
		// `keybuf` recoit les raccourcis FORMATES depuis la table (un tampon par
		// ligne) : la vue n'invente aucune chaine de touche.
		// Retourne le nombre d'entrees retenues pour ce mode.
		inline int32 NkMeshMenuBuild(int32 selMask, int32 selCount, const NkShortcutTable &sc,
									 const char **labels, const char **shorts, bool *enabled,
									 NkMeshCmd *ids, char keybuf[kMeshMenuCap][32]) {
			int32 nT = 0;
			const NkMeshMenuEntry *T = NkMeshMenuTable(nT);
			// Sous-mode courant. `editSelMask` peut porter plusieurs bits ; on
			// retient celui qui decide de l'affichage, du plus specifique au moins
			// specifique -- meme ordre de priorite que l'en-tete de la vue.
			const uint8 mode = (uint8)((selMask & 4) ? NK_MM_FACE : ((selMask & 2) ? NK_MM_EDGE : NK_MM_VERTEX));
			int32 n = 0;
			for (int32 i = 0; i < nT && n < kMeshMenuCap; ++i) {
				if (!(T[i].modes & mode))
					continue;
				labels[n] = T[i].label;
				ids[n] = T[i].cmd;
				keybuf[n][0] = '\0';
				if (T[i].command && *T[i].command)
					(void)sc.FormatFor(T[i].command, keybuf[n], (uint32)sizeof(keybuf[n]));
				shorts[n] = keybuf[n];
				enabled[n] = (!T[i].needsSel || selCount > 0);
				++n;
			}
			return n;
		}

	} // namespace nk3d
} // namespace nkentseu
