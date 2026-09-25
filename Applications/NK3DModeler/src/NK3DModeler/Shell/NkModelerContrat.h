#pragma once
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Applications/NK3DModeler/src/NK3DModeler/Shell/NkModelerContrat.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// =============================================================================
//  LE CONTRAT D'OUTILS — la table des verbes DEVIENT UNE DONNEE
// =============================================================================
//  Arbitrage de Rodolf (17/09) : « on peut choisir le modele a utiliser, local ou
//  distant ; le plus important est la PERFORMANCE DES OUTILS qui sont associes au
//  modele. » Consequence directe, et c'est celle qui coute : **le contrat d'outil
//  doit etre un livrable**, pas une connaissance enfermee dans du code.
//
//  ⚠️ CE QUE CE FICHIER CORRIGE. La table des verbes vivait dans une CHAINE DE
//     `else if (est("..."))` au milieu de `main.cpp`. Elle etait donc :
//       - illisible pour qui n'ouvre pas le C++ ;
//       - impossible a donner a un modele distant, a Ilyana, ou a quiconque ;
//       - et surtout, elle ne pouvait pas etre IMPRIMEE sans etre RECOPIEE --
//         et une chaine recopiee peut mentir sans que rien ne le signale.
//     Elle est desormais UNE DONNEE, lue par le pont ET par l'imprimeur. **Une
//     seule autorite, deux lecteurs.**
//
//  ⚠️ CE QUE LE CONTRAT N'INVENTE PAS. Les parametres, leurs bornes et leurs
//     types ne sont PAS ecrits ici : ils sont lus dans `Demo3DHostOpParamInfo`,
//     c'est-a-dire dans la table que le panneau de proprietes affiche deja et
//     que le clamp applique. Les recopier ici aurait cree la divergence que ce
//     fichier existe pour empecher.
//
//  ⚠️ LE REFUS EST UN OUTIL, PAS UNE PANNE. Un outillage se juge autant a ce
//     qu'il refuse proprement qu'a ce qu'il execute : une IA invente des verbes,
//     c'est le cas NORMAL. Les motifs de refus font donc partie du contrat, au
//     meme titre que les verbes.
// -----------------------------------------------------------------------------

#include "NK3DModeler/Shell/NkModelerInput.h"
#include "NK3DModeler/Viewport/NkDemo3DHost.h"

#include <cstdio>

namespace nkentseu {
	namespace nk3d {

		/// UN VERBE DU CONTRAT. `effet` est une phrase courte, destinee a un
		/// lecteur humain ET a un modele : c'est ce qui permet de choisir le verbe
		/// sans lire le code.
		struct NkVerbe {
				const char *nom;
				NkVpAction act;
				const char *effet;
		};

		/// LA TABLE. Unique autorite : `NkVpPoserAction` la parcourt, l'imprimeur
		/// du contrat la parcourt. Ajouter un verbe ici le rend reconnu ET
		/// documente du meme geste -- il n'y a pas d'endroit ou l'oublier.
		inline const NkVerbe *NkVerbes(int32 &n) {
			static const NkVerbe kV[] = {
				{"togglexray", NkVpAction::ToggleXray, "affiche ou masque le rayon X"},
				{"frameall", NkVpAction::FrameAll, "cadre la vue sur toute la scene"},
				{"viewfront", NkVpAction::ViewFront, "place la camera de face"},
				{"viewtop", NkVpAction::ViewTop, "place la camera de dessus"},
				{"viewright", NkVpAction::ViewRight, "place la camera a droite"},
				{"selectall", NkVpAction::SelectAll, "selectionne tout"},
				{"selectnone", NkVpAction::SelectNone, "vide la selection"},
				{"submodevert", NkVpAction::SubModeVertex, "passe en sous-mode Sommet"},
				{"submodeedge", NkVpAction::SubModeEdge, "passe en sous-mode Arete"},
				{"submodeface", NkVpAction::SubModeFace, "passe en sous-mode Face"},
				{"undo", NkVpAction::Undo, "annule la derniere operation"},
				{"redo", NkVpAction::Redo, "refait l'operation annulee"},
				{"subdivide", NkVpAction::Subdivide, "subdivise la selection"},
				// ⚠️ IL MANQUAIT, ET C'EST LE CONTRAT LUI-MEME QUI L'A MONTRE : l'outil
				//    existe depuis le 16/09, prouve en deux phases, et AUCUN modele ne
				//    pouvait le demander. Un outil qu'on ne peut pas nommer n'existe pas
				//    pour qui parle. Ses deux parametres -- nombre de boucles et
				//    glissement -- sont LUS dans la table de l'hote, comme tous les
				//    autres : rien n'est recopie ici.
				{"loopcut", NkVpAction::LoopCut, "insere une ou plusieurs boucles d'aretes"},
				{"extrude", NkVpAction::Extrude, "extrude la selection"},
				{"inset", NkVpAction::Inset, "insere une face dans la selection"},
				{"bevel", NkVpAction::BevelEdge, "biseaute la selection"},
				{"delete", NkVpAction::Delete, "supprime la selection"},
				{"dissolve", NkVpAction::Dissolve, "dissout la selection"},
				{"modalaxisx", NkVpAction::ModalAxisX, "contraint la modale en cours a l'axe X"},
				{"modalaxisy", NkVpAction::ModalAxisY, "contraint la modale en cours a l'axe Y"},
				{"modalaxisz", NkVpAction::ModalAxisZ, "contraint la modale en cours a l'axe Z"},
				{"modalmove", NkVpAction::ModalMove, "ouvre une modale de deplacement"},
				{"modalconfirm", NkVpAction::ModalConfirm, "valide la modale en cours"},
				{"modalcancel", NkVpAction::ModalCancel, "annule la modale en cours"},
				{"toggleedit", NkVpAction::ToggleEdit, "entre ou sort du mode Edition"},
			};
			n = (int32)(sizeof(kV) / sizeof(kV[0]));
			return kV;
		}

		/// LE TEST DE RECONNAISSANCE, ET IL N'EXISTE QU'ICI.
		/// ⚠️ IL A ETE SORTI DU PONT PARCE QU'UN SECOND LECTEUR EST APPARU : le
		///    panneau doit savoir si ce que tape Rodolf est DEJA un verbe (auquel
		///    cas on l'execute sans deranger le modele) ou une phrase (auquel cas
		///    on interroge le modele). Deux tests de reconnaissance auraient
		///    diverge au premier caractere terminateur ajoute -- et la divergence
		///    se serait vue comme « le panneau accepte ce que le pont refuse ».
		/// Le verbe s'arrete sur 0, ',' ou ':' : « bevel:0.2:4 » nomme la commande
		/// PUIS ses parametres.
		inline const NkVerbe *NkVerbeTrouve(const char *texte) {
			if (!texte || !*texte)
				return nullptr;
			int32 nv = 0;
			const NkVerbe *V = NkVerbes(nv);
			for (int32 i = 0; i < nv; ++i) {
				const char *a = texte;
				const char *b = V[i].nom;
				bool egal = true;
				while (*b && egal) {
					char x = *a++, y = *b++;
					if (x >= 'A' && x <= 'Z')
						x = (char)(x - 'A' + 'a');
					if (x != y)
						egal = false;
				}
				if (egal && (*a == 0 || *a == ',' || *a == ':'))
					return &V[i];
			}
			return nullptr;
		}

		/// LES MOTIFS DE REFUS, ET ILS FONT PARTIE DU CONTRAT. Un modele doit
		/// savoir ce qu'on lui repondra quand il se trompe -- sinon il ne peut pas
		/// se corriger.
		inline const char *const *NkMotifsDeRefus(int32 &n) {
			static const char *const kM[] = {
				"nom inconnu : le verbe ne figure pas dans la table ; aucune action n'est posee",
				"parametre en trop : la commande en accepte moins que ce qui est donne",
				"demande vide : rien n'est soumis",
				"aucun projet ouvert : l'assistant agit sur un maillage",
				// ⚠️ DEUX ETATS QUI NE SE CONFONDENT PAS. Un seul message pour les
				//    deux -- « ca n'a pas marche » -- ne permettrait a aucun modele de
				//    se corriger : dans un cas il doit entrer en Edition, dans l'autre
				//    choisir une autre arete.
				"loopcut hors du mode Edition : l'operation agit sur un maillage ouvert",
				"loopcut sans anneau : il part d'une ARETE selectionnee, et l'anneau doit se fermer",
			};
			n = (int32)(sizeof(kM) / sizeof(kM[0]));
			return kM;
		}

		/// ECRIT LE CONTRAT, LISIBLE PAR UN HUMAIN ET PAR UN MODELE.
		/// ⚠️ IL N'EST PAS ECRIT A LA MAIN : chaque ligne vient de la table des
		///    verbes ou de `Demo3DHostOpParamInfo`. C'est ce qui garantit qu'un
		///    verbe ajoute demain apparait ici sans que personne y pense -- et
		///    qu'un contrat imprime ne peut pas decrire un outil qui n'existe pas.
		/// `cmdDuVerbe` est passee par l'appelant parce qu'elle vit dans `main.cpp`
		/// avec le pont : la lui demander evite de la recopier ici.
		inline bool NkEcrireContrat(const char *chemin, int32 (*cmdDuVerbe)(NkVpAction)) {
			FILE *f = nullptr;
#ifdef _WIN32
			fopen_s(&f, chemin, "wb");
#else
			f = fopen(chemin, "wb");
#endif
			if (!f)
				return false;
			int32 nv = 0;
			const NkVerbe *V = NkVerbes(nv);
			int32 np = 0;
			np = demo::Demo3DHostOpParamCount();

			fprintf(f, "# Contrat d'outils — NK3DModeler\n\n");
			fprintf(f, "AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen\n\n");
			fprintf(f, "Ce document est ECRIT PAR L'APPLICATION, jamais a la main : chaque ligne\n");
			fprintf(f, "vient de la table des verbes du pont ou de la table des parametres que le\n");
			fprintf(f, "panneau de proprietes affiche. Un contrat recopie a la main decrirait, tot\n");
			fprintf(f, "ou tard, un outil qui n'existe plus.\n\n");
			fprintf(f, "Syntaxe d'une demande : `verbe` ou `verbe:p1[:p2...]`, les parametres dans\n");
			fprintf(f, "l'ordre du tableau de la commande. Une valeur hors bornes est RAMENEE aux\n");
			fprintf(f, "bornes (elle n'est pas refusee) ; un parametre EN TROP est refuse avec son\n");
			fprintf(f, "motif.\n\n");
			fprintf(f, "## Les %d verbes\n\n", (int)nv);
			fprintf(f, "| verbe | effet | parametres |\n|---|---|---|\n");
			for (int32 i = 0; i < nv; ++i) {
				const int32 cmd = cmdDuVerbe ? cmdDuVerbe(V[i].act) : -1;
				fprintf(f, "| `%s` | %s | ", V[i].nom, V[i].effet);
				int32 ecrits = 0;
				for (int32 p = 0; p < np; ++p) {
					int32 pc = -1, type = 0;
					const char *lib = "";
					float32 vmin = 0.f, vmax = 0.f;
					if (!demo::Demo3DHostOpParamInfo(p, &pc, &lib, &type, &vmin, &vmax))
						continue;
					if (cmd < 0 || pc != cmd)
						continue;
					fprintf(f, "%s`%s` (%s, %g..%g)", ecrits ? " · " : "", lib,
							type == 0 ? "booleen" : (type == 1 ? "entier" : "reel"), (double)vmin,
							(double)vmax);
					++ecrits;
				}
				if (!ecrits)
					fprintf(f, "aucun");
				fprintf(f, " |\n");
			}
			int32 nm = 0;
			const char *const *M = NkMotifsDeRefus(nm);
			fprintf(f, "\n## Les %d refus nommes\n\n", (int)nm);
			fprintf(f, "Un refus est un OUTIL, pas une panne : un modele invente des verbes, c'est\n");
			fprintf(f, "le cas normal. Chaque refus porte son motif, a l'ecran et au journal.\n\n");
			for (int32 i = 0; i < nm; ++i)
				fprintf(f, "- %s\n", M[i]);
			fprintf(f, "\n## Ce que le contrat NE promet PAS\n\n");
			fprintf(f, "- **La designation par le langage n'existe pas.** Nos commandes portent des\n");
			fprintf(f, "  INDICES ; rien ne traduit « l'arete du haut ». Une demande qui designe\n");
			fprintf(f, "  par les mots est refusee, elle n'est pas devinee.\n");
			// (21/09) CETTE LIGNE DISAIT « la generation d'objets n'est pas ici », et
			// c'etait la cause exacte du « aucune » que Rodolf a recu. Les verbes
			// ci-dessus EDITENT ; la creation suit, dans sa propre section.
			fprintf(f, "- **Ces verbes EDITENT un maillage existant**, a partir de la selection.\n");
			fprintf(f, "  CREER un objet passe par la section « Creation » qui suit.\n");
			fclose(f);
			return true;
		}

	} // namespace nk3d
} // namespace nkentseu
