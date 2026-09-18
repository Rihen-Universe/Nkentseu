#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerDeleteMenu.h
// @Brief   LE MENU X, celui de Blender : X n'execute pas, il OUVRE un menu dont
//          le contenu depend du sous-mode. Une table, un masque, un repartiteur.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI CE FICHIER EXISTE
//   Chez Blender, X ouvre un menu de ONZE entrees ; chez nous il executait une
//   seule chose, silencieusement, et l'utilisateur ne pouvait pas savoir que les
//   dix autres existaient. Rodolf demande « fais comme Blender ».
//
// ── LA REGLE, ET ELLE EST DEJA CELLE DE `NkModelerMeshMenu.h` ───────────────
//   UNE ENTREE QUI NE PEUT RIEN PRODUIRE SE MONTRE ET SE REFUSE, elle ne
//   disparait pas. Un menu ampute apprendrait a l'utilisateur que Blender n'a
//   pas cette commande ; un menu complet dont trois entrees disent POURQUOI
//   elles ne marchent pas encore dit la verite, et se corrige.
//   ⚠ ET LE MOTIF EST OBLIGATOIRE. Une entree grisee sans raison est un mur
//   sans panneau : l'utilisateur ne sait pas s'il lui manque une selection, un
//   sous-mode, ou une fonctionnalite qui n'existe pas encore.
//
// ── CE QUI NE BOUGE PAS, ET C'EST LE PIEGE DE CE LOT ───────────────────────
//   La TOUCHE X ouvre le menu (Blender). L'ACTION du shell (`NkVpAction::Delete`,
//   le bouton et le menu contextuel « Supprimer ») EXECUTE le defaut du
//   sous-mode, comme avant. Deux entrees, une commande -- exactement la regle du
//   fichier jumeau. Si l'action ouvrait un menu, les trois courses de bout en
//   bout cesseraient de supprimer.
//
// ECRIT SUR LE MOULE DE `NkModelerMeshMenu.h`, et pour la meme raison : ce
// fichier n'inclut PAS `NkEditorContextMenu.h`. La construction de la liste est
// du CALCUL PUR, donc mesurable en console, sans fenetre. Le banc est
// `NKMeshMenuTest`.
// -----------------------------------------------------------------------------
#include "NK3DModeler/Viewport/NkDemo3DHost.h"

namespace nkentseu {
	namespace nk3d {

		// Les onze entrees du menu X de Blender, dans SON ordre.
		enum class NkDelCmd : int32 {
			Vertices = 0,
			Edges,
			Faces,
			OnlyEdgesFaces,
			OnlyFaces,
			DissolveVerts,
			DissolveEdges,
			DissolveFaces,
			LimitedDissolve,
			EdgeCollapse,
			EdgeLoops,
		};

		enum { kDelMenuCap = 16 };

		struct NkDelMenuEntry {
				NkDelCmd cmd;
				const char *label;
				/// ⚠️ LE GROUPE, PAS LE TRAIT. Blender range ses onze commandes en
				///    QUATRE groupes, et le groupe est une information : « ces trois-la
				///    vont ensemble » se lit sans un mot. Une liste de onze libelles
				///    oblige a tout lire pour en trouver un.
				///    On stocke le NUMERO DE GROUPE et on en DERIVE le trait, au lieu
				///    de poser un booleen « trait apres » : ajouter une entree au milieu
				///    d'un groupe deplacerait sinon le trait d'une ligne sans que
				///    personne le remarque.
				int32 groupe;
		};

		inline const NkDelMenuEntry *NkDelMenuTable(int32 &count) {
			// ⚠ LES ONZE SONT LA, DANS LES TROIS SOUS-MODES. Ce qui change d'un
			// sous-mode a l'autre n'est pas la PRESENCE mais la DISPONIBILITE --
			// voir `NkDelMenuMotif`.
			static const NkDelMenuEntry kT[] = {
				{NkDelCmd::Vertices, "Sommets", 0},
				{NkDelCmd::Edges, "Aretes", 0},
				{NkDelCmd::Faces, "Faces", 0},
				{NkDelCmd::OnlyEdgesFaces, "Seulement aretes et faces", 0},
				{NkDelCmd::OnlyFaces, "Seulement les faces", 0},
				{NkDelCmd::DissolveVerts, "Dissoudre les sommets", 1},
				{NkDelCmd::DissolveEdges, "Dissoudre les aretes", 1},
				{NkDelCmd::DissolveFaces, "Dissoudre les faces", 1},
				{NkDelCmd::LimitedDissolve, "Dissolution limitee", 2},
				{NkDelCmd::EdgeCollapse, "Effondrer les aretes et les faces", 3},
				{NkDelCmd::EdgeLoops, "Boucles d'aretes", 3},
			};
			count = (int32)(sizeof(kT) / sizeof(kT[0]));
			return kT;
		}

		// ── LE MOTIF DE REFUS, OU "" QUAND L'ENTREE EST DISPONIBLE ──────────────
		// Une seule fonction repond « pourquoi pas », et c'est elle qui grise. Deux
		// endroits -- un qui grise, un qui explique -- finiraient par se
		// contredire, et c'est l'explication qu'on croirait.
		inline const char *NkDelMenuMotif(NkDelCmd c, int32 selMask, int32 selCount) {
			if (selCount <= 0)
				return "rien n'est selectionne";
			switch (c) {
				case NkDelCmd::Vertices:
				case NkDelCmd::Edges:
				case NkDelCmd::Faces:
					return "";
				// ── LE DISSOLVE EST CONTEXTUEL CHEZ NOUS ────────────────────────
				// `Demo3DHostEditDissolve` suit le SOUS-MODE : il ne sait pas encore
				// dissoudre un element autre que celui du sous-mode courant. L'entree
				// qui correspond est donc disponible, les deux autres portent ce
				// motif -- qui dit a l'utilisateur QUOI FAIRE (changer de sous-mode)
				// et non seulement que c'est interdit.
				case NkDelCmd::DissolveVerts:
					return (selMask & 4) || (selMask & 2)
							   ? "notre dissolve suit le sous-mode : passe en sous-mode Sommet"
							   : "";
				case NkDelCmd::DissolveEdges:
					return (selMask & 2) ? "" : "notre dissolve suit le sous-mode : passe en sous-mode Arete";
				case NkDelCmd::DissolveFaces:
					return (selMask & 4) ? "" : "notre dissolve suit le sous-mode : passe en sous-mode Face";
				// ── CE QUI N'EXISTE PAS ENCORE, ET QUI LE DIT ───────────────────
				case NkDelCmd::OnlyEdgesFaces:
					return "pas encore ecrit : il faudrait retirer aretes et faces en gardant les sommets";
				case NkDelCmd::OnlyFaces:
					return "pas encore ecrit : il faudrait retirer les faces en gardant leur contour";
				case NkDelCmd::LimitedDissolve:
					return "pas encore ecrit : dissolution par angle limite";
				case NkDelCmd::EdgeCollapse:
					return "pas encore ecrit : effondrer une arete sur son milieu";
				case NkDelCmd::EdgeLoops:
					return "pas encore ecrit : retirer une boucle d'aretes en recousant les faces";
			}
			return "commande inconnue";
		}

		// L'entree que X executait AVANT ce menu, et qui reste le defaut du
		// sous-mode : c'est elle que l'action du shell declenche.
		inline NkDelCmd NkDelMenuDefaut(int32 selMask) {
			if (selMask & 4)
				return NkDelCmd::Faces;
			if (selMask & 2)
				return NkDelCmd::Edges;
			return NkDelCmd::Vertices;
		}

		// ── LE REPARTITEUR : LE SEUL POINT D'ARRIVEE ────────────────────────────
		// Il REFUSE ce que `NkDelMenuMotif` refuse -- il ne redecide pas. Deux
		// autorites sur la meme question finiraient par diverger, et l'ecran
		// montrerait grise ce que le repartiteur executerait quand meme.
		inline bool NkDelMenuRun(NkDelCmd c, int32 selMask, int32 selCount) {
			if (NkDelMenuMotif(c, selMask, selCount)[0] != 0)
				return false;
			switch (c) {
				case NkDelCmd::Vertices: return demo::Demo3DHostEditDeleteMode(1);
				case NkDelCmd::Edges: return demo::Demo3DHostEditDeleteMode(2);
				case NkDelCmd::Faces: return demo::Demo3DHostEditDeleteMode(4);
				case NkDelCmd::DissolveVerts:
				case NkDelCmd::DissolveEdges:
				case NkDelCmd::DissolveFaces: return demo::Demo3DHostEditDissolve();
				default: return false;
			}
		}

		// Construit les tableaux paralleles attendus par `NkCtxMenuDraw`, et REND
		// les motifs : la vue les affiche, le banc les verifie.
		inline int32 NkDelMenuBuild(int32 selMask, int32 selCount, const char **labels, bool *enabled,
									NkDelCmd *ids, const char **motifs, bool *sepAfter = nullptr) {
			int32 nT = 0;
			const NkDelMenuEntry *T = NkDelMenuTable(nT);
			int32 n = 0;
			for (int32 i = 0; i < nT && n < kDelMenuCap; ++i) {
				const char *m = NkDelMenuMotif(T[i].cmd, selMask, selCount);
				labels[n] = T[i].label;
				enabled[n] = (m[0] == 0);
				ids[n] = T[i].cmd;
				if (motifs)
					motifs[n] = m;
				// LE TRAIT SE DERIVE DU GROUPE : il suit la DERNIERE entree de son
				// groupe, donc il reste juste quand on ajoute ou retire une entree.
				// ⚠️ Et jamais apres la derniere : un trait en bas de menu separe le
				//    menu de rien.
				if (sepAfter)
					sepAfter[n] = (i + 1 < nT) && (T[i + 1].groupe != T[i].groupe);
				++n;
			}
			return n;
		}

	} // namespace nk3d
} // namespace nkentseu
