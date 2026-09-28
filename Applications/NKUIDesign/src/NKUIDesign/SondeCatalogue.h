#pragma once
// -----------------------------------------------------------------------------
// @File    SondeCatalogue.h
// @Brief   `--sonde-catalogue` : le catalogue des composants dit-il la vérité, et
//          un greffon peut-il vraiment y ajouter le sien ?
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI ELLE EXISTE
// =============================================================================
//  Le catalogue est une SECONDE LISTE des rôles du format ; la première est
//  `NkGuiRoleDepuisNom`, celle que le monteur consulte. *Deux listes qui peuvent
//  se contredire finissent par le faire* — et celle-ci se lit dans l'interface,
//  donc son mensonge serait pris pour la vérité.
//
// =============================================================================
//  LES CRITÈRES
// =============================================================================
//   (c1) tout nom INTÉGRÉ du catalogue se RÉSOUT chez le monteur.
//   (c2) 🔴 tout rôle du format EST au catalogue. C'est le sens qu'on oublie :
//        sans lui, un rôle ajouté demain resterait invisible pour toujours, et
//        rien ne le dirait.
//   (c3) aucun héritage orphelin — un `herite` vers un parent absent est une
//        promesse vide.
//   (c4) UN GREFFON PEUT AJOUTER : nom neuf, catégorie neuve, héritage d'un rôle
//        existant. Sans ce critère, « extensible » reste une intention.
//   (c5) NÉGATIF, ET IL EN FAUT TROIS : un doublon est REFUSÉ, un héritage vers
//        un parent inconnu est REFUSÉ, un nom vide est REFUSÉ. Un registre qui
//        accepte tout ne protège rien.
//   (c6) le RETRAIT d'un greffon le retire VRAIMENT — sinon on cliquerait un
//        composant que plus rien ne sait monter.
// -----------------------------------------------------------------------------

#include "NKGui/Doc/NkGuiCatalogue.h"

namespace nkuidesign {

	inline nkentseu::int32 SondeCatalogue() {
		using namespace nkentseu;
		using nkentseu::uint32;
		printf("=== SONDE CATALOGUE — les composants du format, et ceux des greffons ===\n");
		uint32 ko = 0u;

		// ── (c1)(c2)(c3) LE CATALOGUE DIT-IL LA VÉRITÉ ? ───────────────────
		{
			nkgui::NkGuiCatalogueRapport rap;
			const bool ok = nkgui::NkGuiCatalogueVerifier(rap);
			printf("  entrees = %u (%u integrees, %u apportees) ; noms inconnus = %u, roles sans "
				   "entree = %u, heritages orphelins = %u\n",
				   rap.entrees, rap.integrees, rap.apportees, rap.nomsInconnus,
				   rap.rolesSansEntree, rap.heritagesOrphelins);
			if (rap.nomsInconnus > 0u) {
				printf("     KO : « %s » est au catalogue et le monteur ne le connait PAS — le "
					   "panneau annonce un role mort\n",
					   rap.premierNomInconnu.CStr());
				++ko;
			}
			if (rap.rolesSansEntree > 0u) {
				printf("     KO : le role « %s » existe et n'est PAS au catalogue — invisible "
					   "pour qui ecrit un document\n",
					   rap.premierRoleSansEntree.CStr());
				++ko;
			}
			if (rap.heritagesOrphelins > 0u) {
				printf("     KO : %u heritage(s) vers un parent absent\n", rap.heritagesOrphelins);
				++ko;
			}
			if (!ok && ko == 0u) {
				printf("     KO : la verification refuse sans qu'aucun compteur ne le dise\n");
				++ko;
			}
		}

		nkgui::NkGuiCatalogueRegistre &reg = nkgui::NkGuiCatalogueGlobal();
		const uint32 avant = reg.Taille();

		// ── (c5) LES TROIS NÉGATIFS D'ABORD ────────────────────────────────
		// 🔴 AVANT LE POSITIF : un registre qui accepte TOUT ferait passer (c4)
		//    sans rien prouver.
		{
			const bool doublon = nkgui::NkGuiCatalogueAjouterGreffon(
				"Button", "Widgets", "Boutons", "un second Button", "Button \"x\" {}", nullptr,
				"greffon.epreuve");
			const bool orphelin = nkgui::NkGuiCatalogueAjouterGreffon(
				"GreffonOrphelin", "Widgets", "Boutons", "herite d'un absent",
				"GreffonOrphelin \"x\" {}", "CeRoleNExistePas", "greffon.epreuve");
			const bool vide = nkgui::NkGuiCatalogueAjouterGreffon(
				"", "Widgets", "Boutons", "sans nom", "x", nullptr, "greffon.epreuve");
			printf("  -- (c5) refus : doublon = %s, heritage orphelin = %s, nom vide = %s\n",
				   doublon ? "ACCEPTE" : "refuse", orphelin ? "ACCEPTE" : "refuse",
				   vide ? "ACCEPTE" : "refuse");
			if (doublon) {
				printf("     KO : un greffon a pu REDEFINIR `Button` — il changerait l'extrait "
					   "de tout le monde\n");
				++ko;
			}
			if (orphelin) {
				printf("     KO : un heritage vers un parent inconnu a ete accepte\n");
				++ko;
			}
			if (vide) {
				printf("     KO : une entree SANS NOM a ete acceptee\n");
				++ko;
			}
			if (reg.Taille() != avant) {
				printf("     KO : le registre a grossi (%u -> %u) alors que tout etait refuse\n",
					   avant, reg.Taille());
				++ko;
			}
		}

		// ── (c4) UN GREFFON AJOUTE VRAIMENT ────────────────────────────────
		{
			const bool a1 = nkgui::NkGuiCatalogueAjouterGreffon(
				"BoutonNeon", "Widgets", "Boutons", "un bouton du greffon d'epreuve",
				"BoutonNeon \"mon.action\" { label = \"Neon\" }", "Button", "greffon.epreuve");
			// une FAMILLE neuve et une SOUS-CATEGORIE neuve : c'est la demande
			// exacte de Rodolf (« leurs propres categories et sous-categories »).
			const bool a2 = nkgui::NkGuiCatalogueAjouterGreffon(
				"JaugeRadiale", "Instruments", "Jauges", "une jauge ronde", "JaugeRadiale \"j\" {}",
				nullptr, "greffon.epreuve");
			const nkgui::NkGuiRoleInfo *e1 = reg.Trouver("BoutonNeon");
			const nkgui::NkGuiRoleInfo *e2 = reg.Trouver("JaugeRadiale");
			printf("  -- (c4) ajouts : BoutonNeon = %s (herite « %s », famille « %s »), "
				   "JaugeRadiale = %s (famille « %s », categorie « %s »)\n",
				   a1 ? "OK" : "REFUSE", e1 ? e1->herite.CStr() : "?",
				   e1 ? e1->Famille() : "?", a2 ? "OK" : "REFUSE", e2 ? e2->Famille() : "?",
				   e2 ? e2->categorie.CStr() : "?");
			if (!a1 || !e1) {
				printf("     KO : un greffon ne peut pas ajouter un composant qui HERITE\n");
				++ko;
			} else if (e1->herite.Compare(NkString("Button")) != 0) {
				printf("     KO : l'heritage declare n'est pas celui qui est garde\n");
				++ko;
			}
			if (!a2 || !e2) {
				printf("     KO : un greffon ne peut pas apporter sa propre famille\n");
				++ko;
			} else if (NkComponentDecl::StrEq(e2->Famille(), "Greffons")) {
				// ⚠️ LE NOM DE LA FAMILLE DOIT SURVIVRE. Retomber sur « Greffons »
				//    ferait que deux greffons partagent un seul titre — et la
				//    categorie demandee disparaitrait a l'ecran.
				printf("     KO : la famille « Instruments » est retombee sur « Greffons »\n");
				++ko;
			}
			// L'ajout ne doit pas casser la verification du format.
			nkgui::NkGuiCatalogueRapport rap2;
			(void)nkgui::NkGuiCatalogueVerifier(rap2);
			if (rap2.nomsInconnus > 0u) {
				printf("     KO : les entrees de greffon sont comptees comme des roles du "
					   "format (%u nom(s) inconnu(s))\n",
					   rap2.nomsInconnus);
				++ko;
			}
		}

		// ── (c6) LE RETRAIT RETIRE ─────────────────────────────────────────
		{
			const uint32 retires = reg.RetirerProvenance("greffon.epreuve");
			const bool resteBouton = reg.Trouver("BoutonNeon") != nullptr;
			const bool resteJauge = reg.Trouver("JaugeRadiale") != nullptr;
			printf("  -- (c6) retrait du greffon : %u entree(s) retiree(s), taille %u -> %u\n",
				   retires, avant + 2u, reg.Taille());
			if (retires != 2u || resteBouton || resteJauge) {
				printf("     KO : un greffon decharge reste au catalogue — on cliquerait un "
					   "composant que plus rien ne sait monter\n");
				++ko;
			}
			if (reg.Taille() != avant) {
				printf("     KO : le registre ne revient pas a sa taille d'origine\n");
				++ko;
			}
		}

		printf("=== %s (ko = %u) ===\n", ko == 0u ? "TOUT PASSE" : "KO", ko);
		return ko == 0u ? 0 : 1;
	}

} // namespace nkuidesign

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
