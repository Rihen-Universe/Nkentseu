#pragma once
// -----------------------------------------------------------------------------
// @File    RecetteIdentite.h
// @Brief   La recette « Identite » — sortie de `main.cpp` le 28/09/2026.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE
// =============================================================================
//  Rodolf, 28/09 : « tout fichier de plus de 1k ligne reste trop volumineux [...]
//  on peut encore subdiviser dans des fonctions et des fichiers spécifiques ».
//
//  `main.cpp` faisait 10 765 lignes, dont **6 497 de recettes** -- des bancs de
//  mesure qui ne tournent pas quand l'application s'ouvre. Ce n'etait donc pas
//  l'application qui etait grosse : c'etait un fichier qui hebergeait huit bancs.
//
//  Apres le decoupage : `main.cpp` tombe a 4 268 lignes.
//
// =============================================================================
//  ⚠️ CE FICHIER S'INCLUT DEPUIS `main.cpp`, ET SEULEMENT DE LA
// =============================================================================
//  Une recette lit l'etat global de l'application (`gDesign`, les tables
//  d'actions, les crochets). Elle n'est pas une bibliotheque : c'est un banc qui
//  vit dans l'unite de traduction du programme.
//
//  Le decoupage est un RANGEMENT, pas une modularisation : rien n'a ete renomme,
//  aucune signature n'a bouge, et le code est identique a l'octet pres. C'est ce
//  qui permet de le verifier par la construction et par le verdict des recettes
//  elles-memes -- si l'une d'elles changeait de resultat, le deplacement aurait
//  ete faux.
// -----------------------------------------------------------------------------

static int RecetteIdentite() {
	using namespace nkentseu;
	using namespace nkentseu::editorkit;
	int echecs = 0;
	auto verifier = [&](bool ok, const char *quoi) {
		printf("%s  %s\n", ok ? "OK   " : "ECHEC", quoi);
		if (!ok)
			++echecs;
	};
	printf("=== RECETTE : l'identite d'un panneau (renommer ne doit pas perdre la disposition) ===\n");

	const char *chemin = "Build/sondes-nkuidesign/disposition_identite.cfg";

	// ── 1. On enregistre une disposition : « Propriétés » OUVERT, « Console » ferme.
	{
		PanneauSonde props("proprietes", "Propriétés");
		PanneauSonde console("console", "Console");
		NkEditorShell sh;
		sh.AddPanel(&props);
		sh.AddPanel(&console);
		props.SetOpen(true);
		console.SetOpen(false);
		sh.SaveUiState(chemin);
	}
	const NkString ecrit = NkFile::ReadAllText(NkPath(chemin));
	verifier(!ecrit.Empty(), "la disposition s'enregistre");
	printf("      fichier ecrit :\n%s", ecrit.CStr());

	// ── 2. ON RENOMME. « Propriétés » devient « Proprietes » : c'est exactement ce que
	//    fait une traduction, ou un simple retrait d'accent. L'IDENTIFIANT, lui, ne
	//    bouge pas -- c'est tout l'objet de ce lot.
	{
		PanneauSonde props("proprietes", "Proprietes");
		PanneauSonde console("console", "Console");
		NkEditorShell sh;
		sh.AddPanel(&props);
		sh.AddPanel(&console);
		props.SetOpen(false);
		console.SetOpen(false);
		sh.LoadUiState(chemin);
		printf("      apres renommage : « %s » est %s\n", props.Title(),
			   props.IsOpen() ? "RETROUVE (ouvert)" : "PERDU (ferme)");
		verifier(props.IsOpen(),
				 "LE CRITERE : un panneau RENOMME retrouve sa place (mutation : NK_IDENT_MUTATION=titre)");
	}

	// ── 3. LE ZERO : une application qui ne donne PAS d'identifiant ne change pas de
	//    comportement. Son identifiant VAUT son titre, et tout se passe comme avant.
	{
		const char *cheminVieux = "Build/sondes-nkuidesign/disposition_sans_ident.cfg";
		struct PanneauSansIdent : public NkEditorPanel {
				explicit PanneauSansIdent(const char *t) : NkEditorPanel(t) {}
				void OnUI(NkEditorFrameContext &) override {}
		};
		PanneauSansIdent p1("Console");
		NkEditorShell sh;
		sh.AddPanel(&p1);
		p1.SetOpen(true);
		sh.SaveUiState(cheminVieux);
		PanneauSansIdent p2("Console");
		NkEditorShell sh2;
		sh2.AddPanel(&p2);
		p2.SetOpen(false);
		sh2.LoadUiState(cheminVieux);
		verifier(p2.IsOpen(), "LE ZERO : sans identifiant declare, tout se comporte comme avant");
	}

	// ── 4. UN ANCIEN FICHIER, ecrit a la main comme la coquille l'ecrivait hier :
	//    il ne porte que des TITRES. Il doit encore se relire -- un fichier de
	//    disposition existant chez Rodolf ne devient pas illisible parce qu'on a
	//    ameliore le format.
	{
		const char *cheminAncien = "Build/sondes-nkuidesign/disposition_ancienne.cfg";
		NkFile::WriteAllText(cheminAncien, "maximized=0\npanel=Propriétés\n");
		PanneauSonde props("proprietes", "Propriétés");
		NkEditorShell sh;
		sh.AddPanel(&props);
		props.SetOpen(false);
		sh.LoadUiState(cheminAncien);
		verifier(props.IsOpen(),
				 "UN ANCIEN FICHIER (titres seuls) se relit encore -- le repli par titre");
	}

	// ── 5. LE MELANGE, et c'est l'etat dans lequel le depot va vivre plusieurs jours :
	//    NKUIDesign migree, les quatre autres non. Les deux sortes de panneaux doivent
	//    cohabiter dans la MEME disposition.
	{
		const char *cheminMix = "Build/sondes-nkuidesign/disposition_melange.cfg";
		struct PanneauSansIdent2 : public NkEditorPanel {
				explicit PanneauSansIdent2(const char *t) : NkEditorPanel(t) {}
				void OnUI(NkEditorFrameContext &) override {}
		};
		{
			PanneauSonde migre("proprietes", "Propriétés");
			PanneauSansIdent2 ancien("Console");
			NkEditorShell sh;
			sh.AddPanel(&migre);
			sh.AddPanel(&ancien);
			migre.SetOpen(true);
			ancien.SetOpen(true);
			sh.SaveUiState(cheminMix);
		}
		PanneauSonde migre2("proprietes", "Proprietes"); // RENOMME
		PanneauSansIdent2 ancien2("Console");			 // inchange
		NkEditorShell sh2;
		sh2.AddPanel(&migre2);
		sh2.AddPanel(&ancien2);
		migre2.SetOpen(false);
		ancien2.SetOpen(false);
		sh2.LoadUiState(cheminMix);
		verifier(migre2.IsOpen(), "LE MELANGE : le panneau MIGRE et renomme est retrouve");
		verifier(ancien2.IsOpen(), "LE MELANGE : le panneau NON MIGRE, lui, marche comme avant");
	}

	// ── 6. LE CAS INVERSE, demande par ecrit : UN PANNEAU QUE L'APPLICATION FOURNIT
	//    ET QUE LA DISPOSITION ENREGISTREE NE MENTIONNE PAS.
	//    MON ATTENDU, ecrit avant la mesure : « FERME », et non « il reste flottant ».
	//    La raison est lisible dans `NkEditorShell::LoadUiState` : des qu'une seule ligne
	//    `panel=` existe, la coquille FERME TOUS les panneaux, puis rouvre uniquement ceux
	//    qui sont nommes. Un panneau absent du fichier n'est donc pas « laisse tel quel » :
	//    il est ferme, meme s'il etait ouvert une microseconde plus tot.
	//    ⚠️ C'est la politique INVERSE de celle du document `.nkgui`, ou le monteur ne
	//    parcourt que ses propres zones et ne peut RIEN faire d'un panneau qu'il ne nomme
	//    pas (mesure dans NKGuiMonteTest, section (m12)). Deux formats, deux politiques
	//    opposees sur la meme question : c'est ecrit ici pour que personne ne transporte
	//    la reponse de l'un vers l'autre.
	{
		const char *cheminPartiel = "Build/sondes-nkuidesign/disposition_partielle.cfg";
		NkFile::WriteAllText(cheminPartiel, "maximized=0\npanel=proprietes\n");
		PanneauSonde props("proprietes", "Propriétés");
		PanneauSonde console("console", "Console");
		NkEditorShell sh;
		sh.AddPanel(&props);
		sh.AddPanel(&console);
		props.SetOpen(false);
		console.SetOpen(true); // OUVERT avant la relecture, et non mentionne dans le fichier
		sh.LoadUiState(cheminPartiel);
		printf("      non mentionne : « %s » est %s (attendu : FERME)\n", console.Title(),
			   console.IsOpen() ? "OUVERT" : "FERME");
		verifier(props.IsOpen(), "LE CAS INVERSE : le panneau NOMME est bien rouvert");
		verifier(!console.IsOpen(),
				 "LE CAS INVERSE : un panneau NON MENTIONNE est FERME, pas laisse flottant");
	}

	printf("=== %d echec(s) ===\n", echecs);
	return echecs > 0 ? 1 : 0;
}
