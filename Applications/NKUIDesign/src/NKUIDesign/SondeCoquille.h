#pragma once
// -----------------------------------------------------------------------------
// @File    SondeCoquille.h
// @Brief   `--sonde-coquille` : les documents `.nkgui` de NKUIDesign rendent un
//          verdict SANS fenêtre et SANS GPU.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QU'ELLE MESURE, ET POURQUOI CE N'EST PAS « ÇA CHARGE »
// =============================================================================
//  Un document qui se lit, se monte et n'affiche rien passe tous les critères de
//  lecture du monde. Les quatre chiffres exigés ici sont ceux qui peuvent ROUGIR :
//
//   1. `lu` par document — et le NOM de celui qui manque. « Ça n'a pas chargé »
//      sans nom se cherche à la main.
//   2. `rolesInconnus` — un rôle absent du monteur fait REFUSER le document
//      entier, ses voisins valides compris. Zéro, ou on nomme.
//   3. `montes` — un document lu dont rien ne monte est le silence le plus
//      coûteux : aucune erreur, et une bande vide.
//   4. **Chaque identifiant de `MenuItem` est-il SERVI par la table d'actions ?**
//      C'est le seul critère qui attrape le « menu muet » : une entrée qui
//      s'affiche, se survole, s'appuie — et ne fait rien. Elle est indiscernable
//      d'une entrée qui marche, à l'œil comme au compteur de montage.
//
//  ⚠️ ELLE JUGE LA TABLE QUE L'APPLICATION BRANCHE, PAS UNE COPIE. Elle reçoit
//     `gActionsDocument` en paramètre, la même que `PoserTables` reçoit. En tenir
//     une copie ici aurait donné deux tables qui ne peuvent pas se contredire —
//     donc une sonde qui ne peut rien prouver sur celle qui sert vraiment.
//
//   5. `montes` par document, ET `rolesInconnus`. Un document LU et VALIDE dont
//      RIEN ne monte est une bande vide sans erreur — le pire des silences. Et un
//      rôle inconnu fait REFUSER le document entier, ses voisins valides compris.
//
//  🔴 J'AVAIS D'ABORD ÉCRIT ICI QUE LA SONDE « NE MONTE RIEN », avec une condition
//     de retrait pour le jour où le monteur saurait monter hors écran. C'était une
//     paresse déguisée en limite : `Monter` ne demande qu'un `NkGuiContext` et une
//     région, et le banc `NKGuiInteractTest` le fait depuis toujours. Une limite
//     qu'on déclare sans l'avoir vérifiée devient une excuse permanente.
//
//  ⚠️ LE MONTAGE SE FAIT SANS POLICE, et c'est sans conséquence pour ce qu'on
//     mesure : le texte ne se rasterise pas, mais les widgets se MONTENT — c'est
//     `montes` qu'on lit, pas des pixels. Exiger des pixels demanderait la police
//     et le rasteriseur : un autre critère, et il est nommé ici plutôt que promis.
// -----------------------------------------------------------------------------

#include "NKContainers/String/NkString.h"
#include "NKGui/Doc/NkGuiCoquille.h"
#include "NKSerialization/NkGui/NkGuiArchive.h"

namespace nkuidesign {

	namespace sonde {

		/// Compte les `MenuItem` d'un arbre et ceux dont l'identifiant n'est servi
		/// par personne. Récursive : un `MenuItem` vit dans un `Menu`.
		inline void CompterEntrees(const nkentseu::NkArchive &bloc,
								   const nkgui::NkActionNommee *actions, nkentseu::uint32 nActions,
								   nkentseu::uint32 &entrees, nkentseu::uint32 &muettes,
								   nkentseu::uint32 &grisees,
								   nkentseu::NkString &premiereMuette) noexcept {
			using nkentseu::NkArchiveNode;
			using nkentseu::NkString;
			using nkentseu::NkStringView;
			using nkentseu::uint32;
			// 🔴 LE NOM DU CORPS NE SE DEVINE PAS, IL SE DEMANDE. Ma premiere version
			//    essayait `"__body__"` puis `"body"` et rendait la main si aucun ne
			//    repondait : une sonde MUETTE sur un document parfaitement valide,
			//    c'est-a-dire un verdict vert qui n'a rien lu. `NkGMonteCorps` est la
			//    porte que le monteur lui-meme emploie, et elle demande la cle a
			//    `NkGuiArchive::KeyBody()` — le nom est DECLARE, pas suppose.
			const NkArchiveNode *corps = nkgui::NkGMonteCorps(bloc);
			if (!corps)
				return;
			for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
				if (!corps->array[i].IsObject() || !corps->array[i].object)
					continue;
				const nkentseu::NkArchive &w = *corps->array[i].object;
				// ⚠️ `NkGuiArchive` EST DANS `nkentseu`, PAS DANS `nkentseu::nkgui`. Il
				//    appartient a NKSerialization ; le monteur l'ecrit sans qualifier
				//    parce qu'il est DANS `nkgui`, et que la recherche remonte au
				//    namespace englobant. Copier son ecriture ici, hors de `nkgui`,
				//    donnait `nkgui::NkGuiArchive` — qui n'existe pas.
				const NkStringView role = nkentseu::NkGuiArchive::TypeOf(w);
				const NkString id(nkentseu::NkGuiArchive::IdOf(w));
				// ⚠️ `NkGMotEgal` ET NON UNE COMPARAISON A LA MAIN. Elle existe dans
				//    `NkGuiMonteur.h` et c'est celle que le monteur emploie pour
				//    reconnaitre un role : en ecrire une seconde ici aurait donne deux
				//    facons de dire « c'est un MenuItem », et la mienne comptait la
				//    longueur en dur (8), donc se serait tue au premier renommage.
				if (nkgui::NkGMotEgal(role, "MenuItem")) {
					++entrees;
					// 🔴 UNE ENTREE GRISEE N'EST PAS UNE ENTREE MUETTE, ET LES
					//    CONFONDRE AURAIT PUNI LA BONNE PRATIQUE. La regle de la
					//    maison est « cabler, griser ou retirer » : une entree dont la
					//    fonction n'existe pas encore reste VISIBLE et desactivee, avec
					//    sa raison ecrite dans le document. Elle n'a aucune action a
					//    servir, par construction. La compter muette aurait rendu ce
					//    critere rouge sur exactement les documents les mieux ecrits —
					//    et la seule facon de le verdir aurait ete d'OMETTRE l'entree,
					//    c'est-a-dire de faire croire que la fonction n'est pas prevue.
					//
					// ⚠️ UN `else`, PAS UN `continue`. Ma premiere ecriture sortait de
					//    l'iteration — et sautait donc la descente recursive posee en fin
					//    de boucle. Sans effet sur un `MenuItem`, qui n'a pas d'enfants,
					//    et c'est exactement ce qui rend ce genre de raccourci dangereux :
					//    il est juste jusqu'au jour ou le role en gagne.
					bool servi = !nkgui::NkGBooleen(w, "enabled", true);
					if (servi) {
						++grisees;
					} else {
						for (uint32 a = 0; a < nActions; ++a) {
							const char *n = actions[a].nom;
							if (!n)
								continue;
							if (nkgui::NkGMotEgal(NkStringView(id.CStr()), n)) {
								servi = true;
								break;
							}
						}
					}
					if (!servi) {
						++muettes;
						if (premiereMuette.Size() == 0u)
							premiereMuette = id;
					}
				}
				CompterEntrees(w, actions, nActions, entrees, muettes, grisees, premiereMuette);
			}
		}

	} // namespace sonde

	/// Le verdict. Rend 0 si tout est vert, 1 sinon — c'est le code de sortie que
	/// l'intégration lit.
	///
	/// ⚠️ LE CODE DE SORTIE EST LE VERDICT, ET LE TEXTE EST L'EXPLICATION. Un
	///    rapport qui imprime « KO » en rendant 0 est un rapport qu'aucune chaîne
	///    automatique ne lira — ce dépôt a déjà payé un code de sortie qui mentait.
	inline int SondeCoquille(const nkgui::NkActionNommee *actions,
							 nkentseu::uint32 nActions) noexcept {
		using nkentseu::NkString;
		using nkentseu::uint32;
		printf("=== SONDE COQUILLE NKUIDesign — les documents .nkgui ===\n");
		printf("  table d'actions : %u noms\n", nActions);

		NkCoquilleDocument coq;
		const bool tout = coq.ChargerDepuisDossier("Resources/Interface/NKUIDesign");
		struct Bande {
				const char *nom;
				nkgui::NkBandeDocument *b;
		};
		const Bande bandes[2] = {{"menu_design.nkgui", &coq.menuApp},
								 {"barre_etat.nkgui", &coq.barreEtat}};
		uint32 ko = 0u;
		for (uint32 i = 0; i < 2u; ++i) {
			printf("  -- %s : %s\n", bandes[i].nom, bandes[i].b->lu ? "lu" : "NON LU");
			if (!bandes[i].b->lu)
				++ko;
		}
		printf("  refus total = %u (tout lu : %s)\n", coq.RefusTotal(), tout ? "oui" : "non");

		// Les entrees de menu, et celles que personne ne sert.
		uint32 entrees = 0u, muettes = 0u, grisees = 0u;
		NkString premiere;
		// ⚠️ C'EST `doc` QU'ON LIT, ET C'EST CELUI QUE LE MONTEUR MONTE : `Adopter`
		//    developpe les `include` et les `component` DANS `doc`, pas dans l'arbre
		//    brut. Juger l'arbre brut aurait compte les entrees AVANT inclusion —
		//    donc declare « aucune muette » sur un menu dont la moitie arrive par un
		//    `include` et n'est servie par personne.
		if (coq.menuApp.lu)
			sonde::CompterEntrees(coq.menuApp.doc, actions, nActions, entrees, muettes, grisees,
								  premiere);
		printf("  entrees de menu = %u, servies = %u, grisees = %u, MUETTES = %u",
			   entrees, entrees - muettes - grisees, grisees, muettes);
		if (muettes > 0u)
			printf(" (premiere : %s)", premiere.CStr());
		printf("\n");
		// 🔴 LE CRITERE POUR LEQUEL CETTE SONDE EXISTE. Une entree muette s'affiche,
		//    se survole, s'appuie — et ne fait rien. Rien ne la distingue d'une
		//    entree qui marche, ni a l'oeil ni au compteur de montage.
		if (muettes > 0u)
			++ko;
		// Un menu vide est un menu qui a disparu : le document est lu, et il ne
		// porte rien. Aucune erreur ne le dirait.
		if (coq.menuApp.lu && entrees == 0u) {
			printf("  KO : le menu est LU et ne porte AUCUNE entree\n");
			++ko;
		}

		// ── LE MONTAGE, SANS FENETRE NI GPU ────────────────────────────────
		// 🔴 CE CRITERE ETAIT D'ABORD UNE NOTE (« condition de retrait : le jour ou
		//    le monteur monte hors ecran »), ET LA NOTE ETAIT UNE PARESSE. `Monter`
		//    ne demande qu'un `NkGuiContext` avec une region — c'est exactement ce
		//    que le banc `NKGuiInteractTest` fait depuis toujours. Sans ce chiffre,
		//    la sonde etait verte sur un document LU, VALIDE, dont RIEN ne monte :
		//    une bande vide, sans erreur, et le pire des silences.
		//
		// ⚠️ SANS POLICE, ET C'EST SANS CONSEQUENCE ICI. Le texte ne se rasterise
		//    pas, mais les widgets se MONTENT : c'est `montes` qu'on lit, pas des
		//    pixels. Exiger des pixels demanderait la police et le rasteriseur — un
		//    autre critere, pour un autre jour, et il est nomme.
		for (uint32 i = 0; i < 2u; ++i) {
			if (!bandes[i].b->lu)
				continue;
			nkgui::NkGuiContext ctx;
			ctx.viewW = 1280;
			ctx.viewH = 40;
			const nkgui::NkRect region{0.f, 0.f, 1280.f, 40.f};
			ctx.BeginFrame(0.016f);
			ctx.BeginLayout(region);
			ctx.DL().Reset();
			bandes[i].b->Monter(ctx);
			ctx.EndFrame();
			const uint32 m = bandes[i].b->rap.montes;
			const uint32 inc = bandes[i].b->rap.rolesInconnus;
			printf("  -- %s : montes = %u, roles inconnus = %u, apparences non peintes = %u\n",
				   bandes[i].nom, m, inc, bandes[i].b->rap.apparencesNonPeintes);
			// Un role inconnu fait REFUSER le document entier, ses voisins valides
			// compris : zero, ou on le nomme.
			if (inc > 0u) {
				printf("     KO : %u role(s) inconnu(s) — le document entier est refuse\n", inc);
				++ko;
			}
			if (m == 0u) {
				printf("     KO : document LU et VALIDE dont RIEN ne monte — bande vide\n");
				++ko;
			}
		}

		printf("=== %s (ko = %u) ===\n", ko == 0u ? "OK" : "KO", ko);
		return ko == 0u ? 0 : 1;
	}

} // namespace nkuidesign

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
