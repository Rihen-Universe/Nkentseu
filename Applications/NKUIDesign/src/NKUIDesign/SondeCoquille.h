#pragma once
// -----------------------------------------------------------------------------
// @File    SondeCoquille.h
// @Brief   `--sonde-coquille` : le document `.nkgui` de NKUIDesign rend un verdict
//          SANS fenêtre et SANS GPU.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QU'ELLE MESURE, ET POURQUOI CE N'EST PAS « ÇA CHARGE »
// =============================================================================
//  Un document qui se lit, se monte et n'affiche rien passe tous les critères de
//  lecture du monde. Les cinq chiffres exigés ici sont ceux qui peuvent ROUGIR :
//
//   1. le document est LU — et s'il manque, on le dit.
//   2. **Chaque RACINE NOMMÉE existe.** Une faute de frappe dans un nom de racine
//      ne se voit pas à l'œil : la bande est simplement vide, sans erreur. C'est
//      la faute la plus probable depuis que l'interface tient dans un seul
//      fichier, et c'est le critère le plus neuf de cette sonde.
//   3. `montes` par racine — une racine LUE et VALIDE dont RIEN ne monte est une
//      bande vide sans erreur, le pire des silences.
//   4. `rolesInconnus` — un rôle absent du monteur fait REFUSER le document
//      entier, ses voisins valides compris. Zéro, ou on nomme.
//   5. **Chaque identifiant interactif est-il SERVI par la table d'actions ?**
//      C'est le seul critère qui attrape le « widget muet » : une entrée ou un
//      bouton qui s'affiche, se survole, s'appuie — et ne fait rien. Indiscernable
//      d'un qui marche, à l'œil comme au compteur de montage.
//
//  ⚠️ ELLE JUGE LA TABLE QUE L'APPLICATION BRANCHE, PAS UNE COPIE. Elle reçoit
//     `gActionsDocument` en paramètre, la même que `PoserTables` reçoit. En tenir
//     une copie ici aurait donné deux tables qui ne peuvent pas se contredire —
//     donc une sonde incapable de prouver quoi que ce soit sur celle qui sert.
//
//  ⚠️ ET LES NOMS DE RACINE VIENNENT DE `NkCoquilleDocument::NomsRacines`, la même
//     liste que les crochets emploient. Deux listes auraient pu diverger, et la
//     sonde aurait déclaré « les quatre racines existent » pour quatre noms que
//     personne ne monte.
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

		/// Compte les widgets INTERACTIFS d'un arbre et ceux dont l'identifiant n'est
		/// servi par personne. Récursive : un `MenuItem` vit dans un `Menu`, un
		/// `Button` dans une `HBox`.
		///
		/// ⚠️ `MenuItem` **ET** `Button`, depuis que le document porte aussi la barre
		///    d'outils et un panneau. Ne compter que les entrées de menu aurait laissé
		///    huit boutons de barre d'outils et cinq boutons de panneau sans aucun
		///    contrôle — et un bouton muet coûte exactement ce que coûte une entrée
		///    muette.
		inline void CompterInteractifs(const nkentseu::NkArchive &bloc,
									   const nkgui::NkActionNommee *actions,
									   nkentseu::uint32 nActions, nkentseu::uint32 &interactifs,
									   nkentseu::uint32 &muets, nkentseu::uint32 &grises,
									   nkentseu::NkString &premierMuet) noexcept {
			using nkentseu::NkArchiveNode;
			using nkentseu::NkString;
			using nkentseu::NkStringView;
			using nkentseu::uint32;
			// 🔴 LE NOM DU CORPS NE SE DEVINE PAS, IL SE DEMANDE. Ma première version
			//    essayait `"__body__"` puis `"body"` et rendait la main si aucun ne
			//    répondait : une sonde MUETTE sur un document parfaitement valide,
			//    c'est-à-dire un verdict vert qui n'a rien lu. `NkGMonteCorps` demande
			//    la clé à `NkGuiArchive::KeyBody()` — le nom est DÉCLARÉ, pas supposé.
			const NkArchiveNode *corps = nkgui::NkGMonteCorps(bloc);
			if (!corps)
				return;
			for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
				if (!corps->array[i].IsObject() || !corps->array[i].object)
					continue;
				const nkentseu::NkArchive &w = *corps->array[i].object;
				// ⚠️ `NkGuiArchive` EST DANS `nkentseu`, PAS DANS `nkentseu::nkgui`. Il
				//    appartient à NKSerialization ; le monteur l'écrit sans qualifier
				//    parce qu'il est DANS `nkgui` et que la recherche remonte au
				//    namespace englobant.
				const NkStringView role = nkentseu::NkGuiArchive::TypeOf(w);
				const NkString id(nkentseu::NkGuiArchive::IdOf(w));
				// ⚠️ `NkGMotEgal` ET NON UNE COMPARAISON A LA MAIN : c'est celle que le
				//    monteur emploie pour reconnaître un rôle. En écrire une seconde ici
				//    aurait donné deux façons de dire « c'est un Button », et la mienne
				//    comptait la longueur en dur, donc se serait tue au premier renommage.
				if (nkgui::NkGMotEgal(role, "MenuItem") || nkgui::NkGMotEgal(role, "Button")) {
					++interactifs;
					// 🔴 UN WIDGET GRISÉ N'EST PAS UN WIDGET MUET, ET LES CONFONDRE
					//    AURAIT PUNI LA BONNE PRATIQUE. La règle de la maison est
					//    « câbler, griser ou retirer » : une entrée dont la fonction
					//    n'existe pas encore reste VISIBLE et désactivée, avec sa raison
					//    écrite dans le document. Elle n'a aucune action à servir, par
					//    construction. La compter muette aurait rendu ce critère rouge
					//    sur exactement les documents les MIEUX écrits — et la seule
					//    façon de le verdir aurait été d'OMETTRE l'entrée, c'est-à-dire
					//    de faire croire que la fonction n'est pas prévue.
					//
					// ⚠️ UN `else`, PAS UN `continue` : sortir de l'itération sauterait
					//    la descente récursive posée en fin de boucle. Sans effet sur un
					//    `MenuItem`, et c'est ce qui rend ce raccourci dangereux — il est
					//    juste jusqu'au jour où le rôle gagne des enfants.
					bool servi = !nkgui::NkGBooleen(w, "enabled", true);
					if (servi) {
						++grises;
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
						++muets;
						if (premierMuet.Size() == 0u)
							premierMuet = id;
					}
				}
				CompterInteractifs(w, actions, nActions, interactifs, muets, grises, premierMuet);
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
		printf("=== SONDE COQUILLE NKUIDesign — le document .nkgui ===\n");
		printf("  table d'actions : %u noms\n", nActions);

		NkCoquilleDocument coq;
		const bool lu = coq.ChargerDepuisDossier("Resources/Interface/NKUIDesign");
		uint32 ko = 0u;
		printf("  -- interface.nkgui : %s\n", lu ? "lu" : "NON LU");
		if (!lu) {
			++ko;
			printf("=== KO (ko = %u) ===\n", ko);
			return 1;
		}
		coq.PoserTables(actions, nActions, nullptr, 0u);

		// ── LES QUATRE RACINES, MONTEES UNE PAR UNE ────────────────────────
		// 🔴 LE CRITERE LE PLUS NEUF, ET LE PLUS SILENCIEUX SANS LUI. Depuis que
		//    l'interface tient dans UN fichier a racines nommees, une faute de frappe
		//    dans un nom ne se voit pas : la bande est simplement vide, sans erreur,
		//    sans refus, sans rien. Le rapport porte `racinesIntrouvables` ET le
		//    dernier nom demande — c'est ce qui transforme « il manque quelque chose »
		//    en « tu as ecrit barre_etatt ».
		uint32 nRacines = 0u;
		const char *const *racines = NkCoquilleDocument::NomsRacines(nRacines);
		for (uint32 i = 0; i < nRacines; ++i) {
			nkgui::NkGuiContext ctx;
			ctx.viewW = 1280;
			ctx.viewH = 200;
			const nkgui::NkRect region{0.f, 0.f, 1280.f, 200.f};
			ctx.BeginFrame(0.016f);
			ctx.BeginLayout(region);
			ctx.DL().Reset();
			coq.bande.Monter(ctx, racines[i]);
			ctx.EndFrame();
			const uint32 m = coq.bande.rap.montes;
			const uint32 inc = coq.bande.rap.rolesInconnus;
			const uint32 intr = coq.bande.rap.racinesIntrouvables;
			printf("  -- racine \"%s\" : montes = %u, roles inconnus = %u, introuvable = %u\n",
				   racines[i], m, inc, intr);
			if (intr > 0u) {
				printf("     KO : racine INTROUVABLE (« %s ») — la bande serait VIDE, sans "
					   "erreur\n",
					   coq.bande.rap.derniereRacineIntrouvable.CStr());
				++ko;
			}
			if (inc > 0u) {
				printf("     KO : %u role(s) inconnu(s) — le document entier est refuse\n", inc);
				++ko;
			}
			if (intr == 0u && m == 0u) {
				printf("     KO : racine TROUVEE et VALIDE dont RIEN ne monte — bande vide\n");
				++ko;
			}
		}

		// ── LES WIDGETS INTERACTIFS, ET CEUX QUE PERSONNE NE SERT ──────────
		// ⚠️ C'EST `doc` QU'ON LIT, ET C'EST CELUI QUE LE MONTEUR MONTE : `Adopter`
		//    developpe les `include` et les `component` DANS `doc`, pas dans l'arbre
		//    brut. Juger l'arbre brut aurait compte les entrees AVANT inclusion — donc
		//    declare « aucune muette » sur un menu dont la moitie arrive par un
		//    `include` et n'est servie par personne.
		uint32 interactifs = 0u, muets = 0u, grises = 0u;
		NkString premier;
		sonde::CompterInteractifs(coq.bande.doc, actions, nActions, interactifs, muets, grises,
								  premier);
		printf("  widgets interactifs = %u, servis = %u, grises = %u, MUETS = %u",
			   interactifs, interactifs - muets - grises, grises, muets);
		if (muets > 0u)
			printf(" (premier : %s)", premier.CStr());
		printf("\n");
		// 🔴 LE CRITERE POUR LEQUEL CETTE SONDE EXISTE. Un widget muet s'affiche, se
		//    survole, s'appuie — et ne fait rien. Rien ne le distingue d'un widget qui
		//    marche, ni a l'oeil ni au compteur de montage.
		if (muets > 0u)
			++ko;
		if (interactifs == 0u) {
			printf("  KO : le document est LU et ne porte AUCUN widget interactif\n");
			++ko;
		}

		// ── LE PATRON INSTANCIÉ DEPUIS LE C++ (28/09) ──────────────────────
		// 🔴 TROIS CRITÈRES, ET LE PREMIER EST CELUI QUI A FAILLI MANQUER : le
		//    patron doit SURVIVRE au développement des composants. Avant le
		//    28/09, `NkGuiDevelopperComposants` retirait les définitions du
		//    document — avec raison pour le montage, mais il ne restait alors
		//    plus rien à instancier à l'exécution, et rien ne le disait.
		//
		// ⚠️ ET LE TROISIÈME EXIGE UN WIDGET, PAS UN APPEL QUI REND VRAI. Une
		//    instanciation qui « réussit » sans rien monter serait un menu vide
		//    qui se croit rempli — la forme exacte du défaut que cette sonde
		//    existe pour attraper.
		{
			printf("  patrons gardes apres developpement = %u\n", coq.bande.patrons.Taille());
			if (coq.bande.patrons.Taille() == 0u) {
				printf("     KO : aucun patron — `component` declare mais perdu au "
					   "developpement, rien a instancier depuis le C++\n");
				++ko;
			}
			if (!coq.bande.patrons.Trouver("entreeListe")) {
				printf("     KO : le patron « entreeListe » est introuvable\n");
				++ko;
			} else {
				// ⚠️ L'INSTANCE SE MONTE DANS UN MENU RÉELLEMENT OUVERT, et ce
				//    n'est pas du décorum de banc : le monteur écarte un
				//    `MenuItem` posé hors d'une chaîne de menus (il le compte à
				//    part, `elementsMenuHorsMenu`). Mesuré au premier essai —
				//    « montée = OUI, entrées de menu = 0 » : l'instanciation
				//    réussissait et ne dessinait rien. Le critère était juste, le
				//    CONTEXTE était faux.
				//
				// ⚠️ ET PLUSIEURS IMAGES, parce qu'un menu de NKGui se MESURE une
				//    image et s'applique à la suivante. Une seule image ne verrait
				//    jamais un menu ouvert.
				nkgui::NkGuiContext ctx;
				ctx.viewW = 400;
				ctx.viewH = 300;
				nkgui::NkGuiIntrospectActiver(ctx, true);
				const nkgui::NkRect bande{0.f, 0.f, 400.f, 26.f};
				nkgui::NkGuiMonteRapport rapP;
				bool monte = false, actif = false;
				for (uint32 img = 0u; img < 4u; ++img) {
					ctx.input.mousePos = {30.f, 12.f}; // sur le titre du menu
					ctx.input.mouseDown[0] = (img == 1u);
					ctx.BeginFrame(0.016f);
					ctx.BeginLayout(bande);
					ctx.DL().Reset();
					rapP = nkgui::NkGuiMonteRapport();
					if (nkgui::BeginMenuBar(ctx, bande)) {
						if (nkgui::BeginMenu(ctx, "Épreuve")) {
							const nkgui::NkGuiValeurPatron v[2] = {
								nkgui::NkGuiValeurPatron::Texte("label", "Entrée d'épreuve"),
								nkgui::NkGuiValeurPatron::Booleen("checked", true)};
							monte = nkgui::NkGuiMonterPatron(
								ctx, coq.bande.patrons, "entreeListe", "sonde.entree.1", v, 2u,
								coq.bande.etat, rapP, nullptr, &actif);
							nkgui::EndMenu(ctx);
						}
						nkgui::EndMenuBar(ctx);
					}
					ctx.EndFrame();
				}
				// 🔴 LE CRITÈRE QUI COMPTE N'EST PAS UN COMPTEUR, C'EST LE NOM.
				//    Tout l'intérêt d'un patron instancié est que la racine PREND
				//    l'identifiant que l'hôte donne : c'est lui que la table
				//    d'actions sert. Un widget monté sous un autre nom serait
				//    muet, et un compteur d'entrées ne le dirait pas.
				const nkgui::NkGuiNote *note =
					nkgui::NkGuiIntrospectTrouverCle(ctx, "sonde.entree.1");
				printf("  instanciation d'un patron (dans un menu ouvert) : montee = %s, "
					   "entrees = %u, hors menu = %u, note « sonde.entree.1 » = %s\n",
					   monte ? "OUI" : "non", rapP.elementsMenu, rapP.elementsMenuHorsMenu,
					   note ? "TROUVEE" : "absente");
				if (!monte) {
					printf("     KO : le patron existe et ne s'instancie pas\n");
					++ko;
				}
				if (rapP.elementsMenu == 0u) {
					printf("     KO : l'instanciation n'a monte AUCUNE entree — un menu vide "
						   "qui se croit rempli\n");
					++ko;
				}
				if (!note) {
					printf("     KO : l'instance ne porte PAS l'identifiant donne par l'hote — "
						   "son action ne serait servie par personne\n");
					++ko;
				} else if (note->libelle[0] == '\0') {
					printf("     KO : l'instance n'a pas pris le libelle pose par l'hote\n");
					++ko;
				}
			}
		}

		// ── LES COMMANDES QUE LE DOCUMENT DÉCRIT (28/09) ───────────────────
		// 🔴 LE CRITÈRE QUI EMPÊCHE LE RACCOURCI DÉCORATIF. Depuis que le libellé
		//    et le raccourci d'une commande viennent du `.nkgui`, rien
		//    n'empêcherait d'y écrire un `F1` ou un `Ctrl+;` que
		//    `NkEditorShell::TryRunShortcut` ne sait PAS composer : il
		//    s'afficherait à droite de l'entrée et ne partirait jamais — un
		//    paramètre déclaré qui n'est pas honoré, et ce dépôt en a déjà payé
		//    un (`Ctrl+1`). L'énumérateur les écarte ; ce critère exige qu'il
		//    n'ait RIEN à écarter, et les NOMME sinon.
		//
		// ⚠️ ET UN LIBELLÉ VIDE EST UNE COMMANDE INTROUVABLE : la palette
		//    `Ctrl+P` cherche par le nom. Une ligne sans nom y serait une entrée
		//    blanche, inatteignable au clavier.
		{
			nkgui::NkGuiCommandeDite dites[64];
			nkgui::NkGuiCommandesRapport rc;
			const uint32 nd = nkgui::NkGuiEnumererCommandes(coq.bande.doc, actions, nActions,
														   dites, 64u, rc);
			printf("  commandes du document = %u (%u raccourcis lies, %u non lies par "
				   "declaration, %u rejetes, %u grisees, %u non servies)\n",
				   nd, rc.raccourcisLies, rc.raccourcisNonLies, rc.raccourcisRejetes, rc.grisees,
				   rc.nonServies);
			if (nd == 0u) {
				printf("     KO : le document ne decrit AUCUNE commande — la palette Ctrl+P "
					   "serait vide\n");
				++ko;
			}
			if (rc.raccourcisRejetes > 0u) {
				printf("     KO : %u raccourci(s) ECRIT(S) que le comparateur ne peut pas "
					   "composer (premier : %s) — affiches, jamais declenches\n",
					   rc.raccourcisRejetes, rc.premierRejete.CStr());
				++ko;
			}
			for (uint32 i = 0; i < nd; ++i) {
				if (dites[i].libelle.Size() == 0u) {
					printf("     KO : « %s » n'a AUCUN libelle — introuvable dans la palette\n",
						   dites[i].id.CStr());
					++ko;
				}
				// ⚠️ ET LE RACCOURCI RETENU EST RELU, pas seulement « non rejete » :
				//    sans cette seconde lecture, une faute DANS le filtre de
				//    l'enumerateur passerait inapercue — le critere jugerait le
				//    chemin au lieu du resultat.
				if (dites[i].raccourci.Size() > 0u
					&& !nkgui::NkGuiRaccourciPeutPartir(dites[i].raccourci.CStr())) {
					printf("     KO : « %s » garde un raccourci illisible (%s)\n",
						   dites[i].id.CStr(), dites[i].raccourci.CStr());
					++ko;
				}
			}
		}

		printf("=== %s (ko = %u) ===\n", ko == 0u ? "OK" : "KO", ko);
		return ko == 0u ? 0 : 1;
	}

} // namespace nkuidesign

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
