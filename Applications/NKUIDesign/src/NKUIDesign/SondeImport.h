#pragma once
// -----------------------------------------------------------------------------
// @File    SondeImport.h
// @Brief   `--sonde-import` : « Fichier ▸ Importer ▸ Document .nkgui… » rend un
//          verdict, SANS fenêtre et SANS GPU.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI ELLE EXISTE, ET CE QU'ELLE MESURE DE PLUS QUE `--sonde-lecture`
// =============================================================================
//  Rodolf, 28/09 : « je ne vois pas où et comment importer des fichiers .nkgui ».
//
//  🔴 IL CHERCHAIT UNE PORTE DEVANT UN CHAÎNON DÉJÀ CONSTRUIT. `NkGuiLire.h`
//     convertissait une archive en modèle depuis le chantier A, et
//     `--sonde-lecture` le PROUVAIT — sur la fonction, appelée directement par la
//     sonde. Personne, dans l'application, ne l'appelait.
//
//  C'est exactement la forme de défaut que ce dépôt nomme : *une sonde qui
//  appelle la fonction au lieu de passer par la porte ne dit rien de la porte.*
//  Celle-ci passe donc par `DesignState::AppliquerImportNkgui`, c'est-à-dire par
//  le MÊME code que le clic sur « Ouvrir » du sélecteur — seul le sélecteur, qui
//  a besoin d'une fenêtre, est court-circuité en écrivant le chemin choisi.
//
// =============================================================================
//  LES CINQ CRITÈRES
// =============================================================================
//   (i1) UN NOUVEL ONGLET naît, et il devient l'actif.
//   (i2) il porte des nœuds — un import qui ouvre un onglet VIDE serait pire
//        qu'un import qui échoue, parce qu'il aurait l'air d'avoir marché.
//   (i3) 🔴 LE DOCUMENT PRÉCÉDENT EST INTACT, octet pour octet. « Importer n'est
//        pas remplacer » : sans ce critère, une version qui écraserait le
//        document courant passerait (i1) et (i2) les yeux fermés.
//   (i4) LE RAPPORT NOMME CE QUI N'EST PAS ARRIVÉ. Rodolf, tout de suite après :
//        « en les ouvrant on est sûr de tout voir, aussi bien design graphique,
//        animation si contenu, blueprint si contenu ? » — non, et le silence
//        serait le vrai défaut. Sur un fichier PORTANT un `behavior`, la sonde
//        exige que le mot « behavior » figure dans ce qui est annoncé.
//   (i5) NÉGATIF : un fichier qui n'est pas un `.nkgui` est REFUSÉ, il le DIT, et
//        il ne crée AUCUN onglet. Sans lui, une porte qui ouvrirait un onglet
//        vide pour n'importe quel octet passerait les quatre premiers.
//
// ⚠️ ELLE N'OUVRE AUCUNE FENÊTRE ET N'INJECTE AUCUNE ENTRÉE. Le sélecteur n'est
//    pas dessiné : son résultat est posé dans `choixNkguiBuf`, le champ que
//    `AppliquerImportNkgui` lit — celui que le sélecteur remplirait.
// -----------------------------------------------------------------------------

#include "NKContainers/String/NkString.h"
#include "NKFileSystem/NkFile.h"

namespace nkuidesign {

	/// Le fichier d'épreuve du critère (i4) et (i5). Écrit dans le dossier
	/// temporaire de la sonde, jamais dans les ressources du dépôt.
	///
	/// ⚠️ ÉCRIT PAR `NkFile`, comme tout le reste : un banc qui passerait par une
	///    autre porte que le produit mesurerait cette autre porte.
	///
	/// ⚠️ ET PAR `WriteAllBytes`, JAMAIS `WriteAllText` : celui-ci écrit en CRLF
	///    et `ReadAllText` renormalise — l'écart serait masqué des DEUX côtés à
	///    la fois. Ce fichier-ci n'est pas comparé octet pour octet, mais un banc
	///    qui prend l'habitude de la porte qui masque finit par s'en servir là où
	///    ça compte. `NkGuiEcrire.h` porte la même note.
	inline bool NkSondeEcrireFichier(const char *chemin, const char *contenu) noexcept {
		nkentseu::NkVector<nkentseu::uint8> octets;
		for (const char *p = contenu; p && *p; ++p)
			octets.PushBack((nkentseu::uint8)*p);
		return nkentseu::NkFile::WriteAllBytes(chemin, octets);
	}

	inline nkentseu::int32 SondeImport() {
		using nkentseu::NkString;
		using nkentseu::uint32;
		printf("=== SONDE IMPORT — « Fichier > Importer > Document .nkgui… » ===\n");
		uint32 ko = 0u;

		// L'état est gros : jamais sur la pile (`NkWorld` a déjà coûté cette leçon).
		static DesignState st;
		st.BuildStarterDocument();

		// 🔴 L'ONGLET DE DÉPART SE POSE À LA MAIN, ET SANS LUI CETTE SONDE ÉTAIT
		//    VERTE EN NE MESURANT RIEN. `BuildStarterDocument` remplit `doc`, il
		//    ne crée AUCUN onglet : au premier passage, `ouverts` valait 0, si
		//    bien que (i1) comparait 0 -> 1 sur une application qui n'avait pas
		//    de document ouvert, et que (i3) — « importer n'est pas remplacer »,
		//    le critère pour lequel cette sonde existe — S'EST SAUTÉ, dans un
		//    verdict qui annonçait « TOUT PASSE ».
		//    *Un critère sauté dans un verdict vert ne protège rien.*
		{
			DesignState::NkDocOuvert s0;
			s0.doc = st.doc;
			s0.nom = st.doc.title;
			st.doc.Save(s0.etatEnregistre);
			st.ouverts.Clear();
			st.ouverts.PushBack(s0);
			st.ongletActif = 0u;
		}

		// L'ancre : ce que le document courant DIT avant l'import. C'est lui que
		// (i3) exige inchangé.
		NkString avant;
		st.doc.Save(avant);
		const uint32 ongletsAvant = (uint32)st.ouverts.Size();
		if (ongletsAvant == 0u) {
			printf("  KO : aucun onglet de depart — (i3) ne pourrait pas s'executer\n");
			++ko;
		}

		// ── (i5) LE NÉGATIF D'ABORD ────────────────────────────────────────
		// 🔴 AVANT LE POSITIF, PARCE QU'IL EST LE SEUL À POUVOIR ACCUSER LA PORTE
		//    ELLE-MÊME. Si un fichier de n'importe quoi ouvrait un onglet, les
		//    quatre autres critères resteraient verts.
		{
			const char *mauvais = "logs/verif/pas-un-nkgui.txt";
			(void)nkentseu::NkDirectory::CreateRecursive("logs/verif");
			if (!NkSondeEcrireFichier(mauvais, "ceci n'est pas un document d'interface\n")) {
				printf("  -- (i5) SAUTE : le fichier d'epreuve n'a pas pu etre ecrit\n");
			} else {
				st.choixNkgui.pickerResultPath[0] = '\0';
				nkentseu::NkSnprintf(st.choixNkguiBuf, sizeof(st.choixNkguiBuf), "%s", mauvais);
				const uint32 n0 = (uint32)st.ouverts.Size();
				st.AppliquerImportNkgui();
				const uint32 n1 = (uint32)st.ouverts.Size();
				const bool dit = st.status.Contains("Import") ;
				printf("  -- (i5) fichier non-.nkgui : onglets %u -> %u, message « %s »\n", n0, n1,
					   st.status.Data() ? st.status.Data() : "(vide)");
				if (n1 != n0) {
					printf("     KO : un onglet a ete ouvert pour un fichier qui n'est pas un "
						   "document\n");
					++ko;
				}
				if (!dit) {
					printf("     KO : le refus est MUET — l'utilisateur ne saurait pas pourquoi "
						   "rien ne s'ouvre\n");
					++ko;
				}
			}
		}

		// ── (i1)(i2)(i3)(i4) LE POSITIF, SUR UN FICHIER QUI PORTE UN `behavior` ──
		{
			const char *bon = "logs/verif/import-epreuve.nkgui";
			// ⚠️ IL PORTE UN `behavior` EXPRÈS. Sur un document qui n'en aurait
			//    pas, (i4) serait vert sans rien prouver : il n'y aurait rien à
			//    ne pas porter.
			const char *src = "nkgui 0.4\n"
							  "widgets {\n"
							  "  Button \"b1\" { label = \"Un\" }\n"
							  "  Button \"b2\" { label = \"Deux\" }\n"
							  "}\n"
							  "behavior {\n"
							  "  on \"b1\" { click = \"rien\" }\n"
							  "}\n";
			if (!NkSondeEcrireFichier(bon, src)) {
				printf("  -- (i1..i4) KO : le fichier d'epreuve n'a pas pu etre ecrit\n");
				++ko;
			} else {
				st.choixNkgui.pickerResultPath[0] = '\0';
				nkentseu::NkSnprintf(st.choixNkguiBuf, sizeof(st.choixNkguiBuf), "%s", bon);
				const uint32 n0 = (uint32)st.ouverts.Size();
				const uint32 actifAvant = st.ongletActif;
				st.AppliquerImportNkgui();
				const uint32 n1 = (uint32)st.ouverts.Size();
				printf("  -- (i1) onglets %u -> %u, actif %u -> %u\n", n0, n1, actifAvant,
					   st.ongletActif);
				if (n1 != n0 + 1u) {
					printf("     KO : aucun nouvel onglet\n");
					++ko;
				} else if (st.ongletActif != n1 - 1u) {
					printf("     KO : l'onglet est ne mais l'utilisateur n'y est pas\n");
					++ko;
				}
				const uint32 noeuds = st.doc.NodeCount();
				printf("  -- (i2) noeuds du document importe = %u\n", noeuds);
				// La racine seule compte pour 1 : un import qui ne porte que la
				// toile est un onglet VIDE, et il aurait l'air d'avoir marche.
				if (noeuds <= 1u) {
					printf("     KO : onglet ouvert SANS widget — pire qu'un echec, il a l'air "
						   "d'avoir marche\n");
					++ko;
				}
				printf("  -- (i4) message : %s\n", st.status.Data() ? st.status.Data() : "(vide)");
				if (!st.status.Contains("behavior")) {
					printf("     KO : le `behavior` du fichier n'est NI montre NI annonce — "
						   "l'utilisateur croirait tout voir\n");
					++ko;
				}
			}
		}

		// Ce que chaque onglet PORTE, avant toute bascule — pour que (i3), s'il
		// rougit, dise sur QUEL onglet la trace a atterri.
		{
			NkString vif;
			st.doc.Save(vif);
			printf("        VIVANT : actif = %u, « %s », %u octet(s), %u recent(s), %u noeud(s)\n",
				   st.ongletActif, st.doc.title.Data() ? st.doc.title.Data() : "?",
				   (uint32)vif.Size(), (uint32)st.doc.dossiersRecents.Size(), st.doc.NodeCount());
		}
		for (uint32 t = 0; t < (uint32)st.ouverts.Size(); ++t) {
			NkString s;
			st.ouverts[t].doc.Save(s);
			printf("        onglet %u : « %s », %u octet(s), %u recent(s)\n", t,
				   st.ouverts[t].nom.Data() ? st.ouverts[t].nom.Data() : "?", (uint32)s.Size(),
				   (uint32)st.ouverts[t].doc.dossiersRecents.Size());
		}

		// ── (i7) LES COMPOSANTS DU DOCUMENT, SELON LE CHOIX ────────────────
		// Rodolf, 28/09 : « importer un .nkgui peut mettre ses composants en
		// bibliothèque ou pas, en fonction du choix de l'utilisateur ».
		//
		// 🔴 DEUX COURSES, PAS UNE : le NON puis le OUI, sur le MÊME fichier.
		//    Ne mesurer que le OUI laisserait passer une version qui importe
		//    TOUJOURS — et c'est le défaut le plus probable, puisque c'est le
		//    chemin qu'on écrit en premier.
		{
			const char *avecCompo = "logs/verif/import-composants.nkgui";
			const char *src = "nkgui 0.4\n"
							  "component \"CarteEpreuve\" {\n"
							  "  Button \"b\" { label = \"Carte\" }\n"
							  "}\n"
							  "widgets {\n"
							  "  Button \"b1\" { label = \"Un\" }\n"
							  "}\n";
			if (!NkSondeEcrireFichier(avecCompo, src)) {
				printf("  -- (i7) SAUTE : fichier d'epreuve non ecrit\n");
			} else {
				nkgui::NkGuiCatalogueRegistre &reg = nkgui::NkGuiCatalogueGlobal();
				const uint32 base = reg.Taille();
				// NON : la case est fausse -> rien ne rejoint la bibliotheque.
				st.importerComposants = false;
				st.choixNkgui.pickerResultPath[0] = '\0';
				nkentseu::NkSnprintf(st.choixNkguiBuf, sizeof(st.choixNkguiBuf), "%s", avecCompo);
				st.AppliquerImportNkgui();
				const uint32 apresNon = reg.Taille();
				// OUI : la meme importation ajoute le composant.
				st.importerComposants = true;
				st.choixNkgui.pickerResultPath[0] = '\0';
				nkentseu::NkSnprintf(st.choixNkguiBuf, sizeof(st.choixNkguiBuf), "%s", avecCompo);
				st.AppliquerImportNkgui();
				const uint32 apresOui = reg.Taille();
				const bool present = reg.Trouver("CarteEpreuve") != nullptr;
				printf("  -- (i7) catalogue : %u (base) -> %u (choix NON) -> %u (choix OUI) ; "
					   "« CarteEpreuve » %s\n",
					   base, apresNon, apresOui, present ? "PRESENT" : "absent");
				if (apresNon != base) {
					printf("     KO : des composants ont rejoint la bibliotheque alors que le "
						   "choix etait NON\n");
					++ko;
				}
				if (apresOui != base + 1u || !present) {
					printf("     KO : le choix OUI n'a pas ajoute le composant du document\n");
					++ko;
				}
				(void)reg.RetirerProvenance("import-composants.nkgui");
			}
		}

		// ── (i3) LE DOCUMENT PRECEDENT, RELU SUR SON ONGLET ────────────────
		if (ongletsAvant > 0u && st.ouverts.Size() > ongletsAvant) {
			st.BasculerVers(0u);
			NkString apres;
			st.doc.Save(apres);
			const bool identique = (avant.Size() == apres.Size())
								   && (avant.Size() == 0u
									   || avant.Compare(apres) == 0);
			printf("  -- (i3) document d'origine apres import : %s (%u octets avant, %u apres)\n",
				   identique ? "INTACT" : "MODIFIE", (uint32)avant.Size(), (uint32)apres.Size());
			if (!identique) {
				// ⚠️ ON NOMME L'ENDROIT, PAS SEULEMENT L'ÉCART. « 919 -> 968 »
				//    envoie chercher à la main ; le premier octet qui diffère et
				//    ce qui le suit disent QUOI a été ajouté.
				const char *a = avant.Data() ? avant.Data() : "";
				const char *b = apres.Data() ? apres.Data() : "";
				uint32 off = 0u;
				while (a[off] && a[off] == b[off])
					++off;
				char extrait[160];
				nkentseu::NkSnprintf(extrait, sizeof(extrait), "%.120s", b + off);
				printf("     KO : importer a TOUCHE le document courant — importer n'est pas "
					   "remplacer\n"
					   "          premier ecart a l'octet %u, l'apres y porte : « %s »\n",
					   off, extrait);
				++ko;
			}
		} else {
			printf("  -- (i3) SAUTE : pas de second onglet a comparer (voir i1)\n");
		}

		printf("=== %s (ko = %u) ===\n", ko == 0u ? "TOUT PASSE" : "KO", ko);
		return ko == 0u ? 0 : 1;
	}

} // namespace nkuidesign

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
