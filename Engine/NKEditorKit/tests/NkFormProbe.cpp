// -----------------------------------------------------------------------------
// @File    NkFormProbe.cpp
// @Brief   LE BANC DE LA FORME -- il compile, il LIE et il TOURNE sans rien lier.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  COMMENT LE REJOUER -- une seule commande, depuis la racine du depot
// =============================================================================
//
//    g++ -std=c++17 -Wall
//      -I Engine/NKEditorKit/src
//      -I Kernel/Foundation/NKCore/src -I Kernel/Foundation/NKContainers/src
//      -I Kernel/Foundation/NKMemory/src -I Kernel/Foundation/NKMath/src
//      -I Kernel/Foundation/NKPlatform/src
//      Engine/NKEditorKit/tests/NkFormProbe.cpp -o form_probe && ./form_probe
//
//  (une seule ligne en pratique -- les retours ci-dessus sont pour la lecture ;
//   une continuation par `\` en fin de ligne de commentaire declencherait
//   `-Wcomment`, et un banc qui avertit sur lui-meme se fait ignorer.)
//
//  AUCUNE BIBLIOTHEQUE, AUCUN `.cpp` DU DEPOT, AUCUN NKGui, AUCUN GPU. C'est la
//  propriete que toute cette forme revendique -- « une declaration se verifie
//  sans rien lier » -- et c'est ici qu'elle cesse d'etre une affirmation.
//
//  ⚠️ IL NE FAIT PAS PARTIE D'UNE CIBLE JENGA, ET C'EST VOLONTAIRE : le jour ou
//     il en ferait partie, il ne prouverait plus qu'on peut verifier une
//     declaration sans construire le kit. Son cout de rejeu est une ligne.
//
// =============================================================================
//  CE QU'IL PROUVE, ET CE QU'IL NE PROUVE PAS
// =============================================================================
//  IL PROUVE : que les DEUX declarations de la bibliotheque sont bien formees ;
//              que le controle sait ROUGIR (dix temoins negatifs) ; que la
//              disposition se CALCULE et qu'elle suit les nombres nommes ; que
//              les bornes sont honorees ; et que le renvoi d'un composant a un
//              autre se resout.
//
//  ⚠️ IL NE PROUVE PAS que le dessin honore ce qu'il lit -- c'est
//     `NKUIDesign --probe` qui compare les commandes de dessin, et lui seul.
//     Il ne prouve rien non plus sur le RENDU : aucun pixel n'est produit ici.
//
//  ⚠️ ET IL DEPEND D'UN FICHIER QUI BOUGE : `NkTreeViewModel.h` est ecrit en
//     parallele par l'agent du composant arbre. Ce banc n'y touche pas et n'en
//     lit que `NkTreeViewDecl()`. Si un essai « tree_view » rougit un jour, la
//     premiere question est *ou en est ce fichier*, pas *la forme est-elle
//     cassee*.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkComponentCheck.h"
#include "NKEditorKit/Components/NkContentBrowserModel.h"
#include "NKEditorKit/Components/NkLayoutSolve.h"
#include "NKEditorKit/Components/NkTreeViewModel.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::editorkit;

static int gPass = 0, gTotal = 0;

static void check(const char *what, bool ok, const char *detail = "") {
	++gTotal;
	if (ok)
		++gPass;
	printf("  [%s] %-72s %s%s\n", ok ? "ok  " : "ECHEC", what, (detail && *detail) ? " -- " : "",
		   detail ? detail : "");
}

// ── UNE SOURCE DE NOMBRES QU'ON PEUT PERTURBER ──────────────────────────────
// L'equivalent d'une `NkComponentInstance` pour ce banc, en huit lignes et sans
// `NkVector`/`NkString` -- qui exigeraient de lier le depot et feraient perdre
// au banc sa seule vertu. Elle ne remplace pas l'instance : elle en imite le
// SEUL comportement dont la disposition a besoin, « un nom, une valeur ».
struct Tweak {
		const NkComponentDecl *decl = nullptr;
		const char *key = nullptr;
		float32 value = 0.f;
};
static float32 TweakGet(const void *u, const char *n, float32 f) {
	const Tweak *t = (const Tweak *)u;
	if (t->key && NkComponentDecl::StrEq(t->key, n))
		return t->value;
	return t->decl->Number(n, f);
}
static NkMetricSource SourceOf(const Tweak &t) {
	NkMetricSource s;
	s.user = &t;
	s.get = &TweakGet;
	return s;
}

static uint16 DiffCount(const NkSolvedRect *a, const NkSolvedRect *b, uint16 n) {
	uint16 d = 0;
	for (uint16 i = 0; i < n; ++i)
		if (a[i].x != b[i].x || a[i].y != b[i].y || a[i].w != b[i].w || a[i].h != b[i].h)
			++d;
	return d;
}

/// Tout enfant tient-il dans son parent ? La tolerance existe parce qu'on
/// compare des flottants issus de divisions ; elle est serree exprès (0.01 px),
/// une tolerance large ferait passer un vrai debordement.
static bool Contained(const NkComponentDecl &d, const NkSolvedRect *r, const char *&outWho) {
	for (uint16 i = 0; i < d.elementCount; ++i) {
		const char *par = d.elements[i].parent;
		if (!par || !*par)
			continue;
		const int32 p = d.ElementIndex(par);
		if (p < 0)
			continue;
		const float32 e = 0.01f;
		if (r[i].x < r[p].x - e || r[i].y < r[p].y - e ||
			r[i].x + r[i].w > r[p].x + r[p].w + e || r[i].y + r[i].h > r[p].y + r[p].h + e) {
			outWho = d.elements[i].name;
			return false;
		}
	}
	return true;
}

int main() {
	printf("\n=== BANC DE LA FORME -- NkComponentDecl et ses fichiers ===\n");
	printf("    (aucune bibliotheque liee, aucun NKGui, aucun GPU)\n\n");

	char buf[256];

	// =======================================================================
	//  1. LES DEUX DECLARATIONS REELLES
	// =======================================================================
	printf("--- 1. les deux composants declares de la bibliotheque ---\n");

	NkFormIssue issues[48];
	const NkComponentDecl &cb = NkContentBrowserDecl();
	NkCheckReport rcb = NkCheckComponent(cb, issues, 48);
	snprintf(buf, sizeof(buf), "%u erreur(s), %u note(s), %u sous-element(s)", rcb.errors, rcb.notes,
			 cb.elementCount);
	check("1. `content_browser` est bien forme", rcb.errors == 0, buf);
	for (uint16 i = 0; i < rcb.written; ++i)
		printf("        %s %s (%s)\n", issues[i].level == NkIssueLevel::Error ? "ERREUR" : "note",
			   issues[i].code, issues[i].subject);

	// ⚠️ CELUI-CI EST LE VRAI TEST DE LA FORME : sa declaration a ete ecrite par
	//    UN AUTRE AGENT, dans une autre famille de composant, sans que je la
	//    relise avant. Une forme validee sur le composant pour lequel elle a ete
	//    ecrite ne prouve rien -- c'est le second qui parle.
	const NkComponentDecl &tv = NkTreeViewDecl();
	NkCheckReport rtv = NkCheckComponent(tv, issues, 48);
	snprintf(buf, sizeof(buf), "%u erreur(s), %u note(s) -- declaration ecrite par l'agent du composant",
			 rtv.errors, rtv.notes);
	check("2. `tree_view` est bien forme (SECOND COMPOSANT, AUTRE MAIN)", rtv.errors == 0, buf);
	for (uint16 i = 0; i < rtv.written; ++i)
		printf("        %s %s (%s)\n", issues[i].level == NkIssueLevel::Error ? "ERREUR" : "note",
			   issues[i].code, issues[i].subject);

	// =======================================================================
	//  2. LE CONTROLE SAIT-IL ROUGIR ? -- sans ca, tout ce qui precede est nul
	// =======================================================================
	// « Un banc qui ne sait dire que oui ne mesure rien. » Dix declarations
	// FAUSSES, une par regle de la forme. Chacune DOIT produire au moins une
	// erreur ; le temoin de tete, lui, doit rester vert.
	printf("\n--- 2. dix temoins qui doivent ECHOUER, et un qui doit passer ---\n");

	static const NkMetricDecl kM[] = {{"gap", 4.f, "gouttiere"}, {"pad", 6.f, "marge"}};
	static const NkParamDecl kP[] = {
		{"frac", "Fraction", NkParamKind::Float, 0.5f, 0.f, 1.f, nullptr, 0}};

	auto minimal = [&](const NkElementDecl *els, uint16 n) {
		NkComponentDecl d;
		d.name = "temoin";
		d.title = "Temoin";
		d.metrics = kM;
		d.metricCount = 2;
		d.params = kP;
		d.paramCount = 1;
		d.elements = els;
		d.elementCount = n;
		return d;
	};

	{
		static const NkElementDecl ok[] = {
			{"racine", "", "", "container", "", NkExpand(), NkExpand(),
			 {NkLayoutKind::Row, "gap", "pad", NkAlign::Start, NkAlign::Stretch, 0, ""}, 0},
			{"a", "racine", "", "label", "", NkFractionM("frac"), NkExpand(), {}, 0},
			{"b", "racine", "", "label", "", NkExpand(), NkExpand(), {}, 0},
		};
		check("3. TEMOIN DE TETE : la declaration minimale valide reste verte",
			  NkCheckComponent(minimal(ok, 3)).errors == 0);
	}
	{
		static const NkElementDecl bad[] = {
			{"racine", "", "", "", "", NkExpand(), NkExpand(),
			 {NkLayoutKind::Row, "gap", "", NkAlign::Start, NkAlign::Stretch, 0, ""}, 0},
			{"a", "fantome", "", "", "", NkExpand(), NkExpand(), {}, 0},
		};
		check("4. un parent qui n'existe pas est REFUSE", NkCheckComponent(minimal(bad, 2)).errors > 0);
	}
	{
		// L'enfant est ecrit AVANT son parent : la contrainte qui rend les cycles
		// impossibles a ecrire est la seule chose qui autorise un resolveur non
		// recursif. Si elle n'est pas verifiee, elle n'existe pas.
		static const NkElementDecl bad[] = {
			{"a", "racine", "", "", "", NkExpand(), NkExpand(), {}, 0},
			{"racine", "", "", "", "", NkExpand(), NkExpand(),
			 {NkLayoutKind::Row, "gap", "", NkAlign::Start, NkAlign::Stretch, 0, ""}, 0},
		};
		check("5. un parent ecrit APRES son enfant est REFUSE (c'est ce qui interdit les cycles)",
			  NkCheckComponent(minimal(bad, 2)).errors > 0);
	}
	{
		static const NkElementDecl bad[] = {
			{"r1", "", "", "", "", NkExpand(), NkExpand(), {}, 0},
			{"r2", "", "", "", "", NkExpand(), NkExpand(), {}, 0},
		};
		check("6. deux racines sont REFUSEES (ce n'est pas un arbre)",
			  NkCheckComponent(minimal(bad, 2)).errors > 0);
	}
	{
		static const NkElementDecl bad[] = {
			{"racine", "", "", "", "", NkExpand(), NkExpand(), {}, 0}, // aucun agencement
			{"a", "racine", "", "", "", NkExpand(), NkExpand(), {}, 0},
		};
		check("7. des enfants sans agencement sont REFUSES (ils ne seraient jamais poses)",
			  NkCheckComponent(minimal(bad, 2)).errors > 0);
	}
	{
		static const NkElementDecl bad[] = {
			{"racine", "", "", "", "", NkExpand(), NkExpand(),
			 {NkLayoutKind::Row, "gouttiere_inconnue", "", NkAlign::Start, NkAlign::Stretch, 0, ""},
			 0},
			{"a", "racine", "", "", "", NkExpand(), NkExpand(), {}, 0},
		};
		check("8. une metrique nommee mais non declaree est REFUSEE (elle vaudrait 0 en silence)",
			  NkCheckComponent(minimal(bad, 2)).errors > 0);
	}
	{
		static const NkElementDecl bad[] = {
			{"racine", "", "", "capacite_inventee", "", NkExpand(), NkExpand(), {}, 0},
		};
		check("9. un role hors catalogue est REFUSE", NkCheckComponent(minimal(bad, 1)).errors > 0);
	}
	{
		static const NkElementDecl bad[] = {
			{"racine", "", "", "", "", NkExpand(), NkExpand(),
			 {NkLayoutKind::Grid, "gap", "", NkAlign::Start, NkAlign::Stretch, 0, ""}, 0},
			{"a", "racine", "", "", "", NkExpand(), NkExpand(), {}, 0},
		};
		check("10. une grille sans colonnes NI largeur de cellule est REFUSEE",
			  NkCheckComponent(minimal(bad, 2)).errors > 0);
	}
	{
		static const NkElementDecl bad[] = {
			{"racine", "", "", "", "", NkExpand(), NkExpand(), {}, 0},
			{"racine", "", "", "", "", NkExpand(), NkExpand(), {}, 0},
		};
		check("11. deux sous-elements de meme nom sont REFUSES (`parent` deviendrait ambigu)",
			  NkCheckComponent(minimal(bad, 2)).errors > 0);
	}
	{
		// Un role annonce et non honore : l'application branche un ecouteur que
		// rien n'appellera. C'est « un parametre qui n'est pas honore ».
		NkComponentDecl d;
		d.name = "faux_bouton";
		d.role = "button"; // exige `onClick`, qui n'est declare nulle part
		check("12. un ROLE annonce et non honore est REFUSE", NkCheckComponent(d).errors > 0);
	}
	{
		static const NkArgDecl a[] = {{"rien", NkArgKind::Void, nullptr, 0}};
		static const NkEventDecl e[] = {{"onQuoi", "", "", a, 1, false}};
		NkComponentDecl d;
		d.name = "charge_vide";
		d.events = e;
		d.eventCount = 1;
		check("13. une charge d'evenement de type `Void` est REFUSEE (Void est un RETOUR)",
			  NkCheckComponent(d).errors > 0);
	}

	// =======================================================================
	//  3. LE ROLE TIRE DEUX COMPOSANTS VERS UN SEUL VOCABULAIRE
	// =======================================================================
	printf("\n--- 3. le role (ajout 1 de Rodolf) ---\n");
	{
		uint16 n = 0;
		const NkRoleDecl *cat = NkRoleCatalog(n);
		bool typesOk = true;
		for (uint16 i = 0; i < n; ++i)
			for (uint8 e = 0; e < cat[i].eventCount; ++e)
				for (uint8 a = 0; a < cat[i].events[e].argCount; ++a)
					if (cat[i].events[e].args[a].kind == NkArgKind::Void)
						typesOk = false;
		snprintf(buf, sizeof(buf), "%u roles au catalogue", n);
		check("14. le catalogue obeit a sa propre regle (aucune charge `Void`)", typesOk, buf);

		// « Une poignee de roles sert des milliers d'apparences » : la mesure est
		// que DEUX composants de familles differentes se rangent sous DEUX roles
		// du meme petit catalogue, sans qu'aucun role n'ait ete taille pour l'un
		// d'eux.
		check("15. `content_browser` endosse une capacite du catalogue", NkFindRole(cb.role) != nullptr,
			  cb.role && *cb.role ? cb.role : "(aucune)");
	}
	{
		// Le second etage de la correspondance : le fait est porte sous un autre
		// nom. On verifie que c'est bien une NOTE et pas une erreur -- et surtout
		// qu'elle designe le BON evenement (la premiere ecriture designait
		// `onSelect`, ce qui etait faux d'une facon parfaitement credible).
		NkCheckReport r = NkCheckComponent(cb, issues, 48);
		bool found = false;
		for (uint16 i = 0; i < r.written; ++i)
			if (NkComponentDecl::StrEq(issues[i].code, "role_nom_divergent") &&
				NkComponentDecl::StrEq(issues[i].subject, "onDoubleClick"))
				found = true;
		check("16. un fait porte sous un autre nom est une NOTE, et elle designe le bon evenement",
			  found && r.errors == 0, "onDoubleClick porte le fait `onActivate`");
	}

	// =======================================================================
	//  4. L'ARBRE ET LES DEUX ECHELLES
	// =======================================================================
	printf("\n--- 4. l'arbre de sous-elements (ajout 2 de Rodolf) ---\n");
	{
		const NkElementDecl *root = cb.RootElement();
		check("17. l'arbre a une racine et une seule", root != nullptr, root ? root->name : "(aucune)");
	}
	{
		// « Une interface complete est un composant qui en contient d'autres. »
		// La verification est mecanique : le renvoi porte un nom, et ce nom est
		// celui d'un composant reellement declare ailleurs.
		const NkElementDecl *ft = cb.FindElement("folder_tree");
		const bool ok = ft && NkComponentDecl::StrEq(ft->component, tv.name);
		snprintf(buf, sizeof(buf), "folder_tree -> %s", ft && ft->component ? ft->component : "(rien)");
		check("18. MEME MECANISME AUX DEUX ECHELLES : un sous-element EST un autre composant", ok,
			  buf);
	}

	// =======================================================================
	//  5. LA POSITION EST UN RESULTAT (ajout 3 de Rodolf)
	// =======================================================================
	printf("\n--- 5. taille et agencement : la position se CALCULE ---\n");

	NkSolvedRect a[32], b[32], c[32];
	const NkSolvedRect panel{0.f, 0.f, 900.f, 600.f};

	Tweak t0{&cb, nullptr, 0.f};
	const uint16 n0 = NkSolveLayout(cb, SourceOf(t0), panel, a, 32);
	snprintf(buf, sizeof(buf), "%u rectangles pour %u sous-elements", n0, cb.elementCount);
	check("19. la disposition se resout entierement", n0 == cb.elementCount, buf);

	{
		const char *who = "";
		check("20. aucun enfant ne deborde de son parent", Contained(cb, a, who),
			  *who ? who : "tous contenus");
	}
	{
		// TEMOIN DE BRUIT. Le resolveur est purement arithmetique : aucune
		// horloge, aucun aleatoire, donc le plancher attendu est EXACTEMENT zero.
		// On le verifie au lieu de le supposer -- sans ce chiffre, aucun ecart
		// mesure plus bas ne voudrait dire quoi que ce soit.
		NkSolveLayout(cb, SourceOf(t0), panel, b, 32);
		snprintf(buf, sizeof(buf), "%u differences (attendu : 0)", DiffCount(a, b, n0));
		check("21. TEMOIN DE BRUIT : deux resolutions identiques ne different pas",
			  DiffCount(a, b, n0) == 0, buf);
	}
	{
		// CONTROLE POSITIF : le nombre est LU, pas ecrit dans le resolveur.
		Tweak t{&cb, "card_pad", 24.f};
		NkSolveLayout(cb, SourceOf(t), panel, b, 32);
		snprintf(buf, sizeof(buf), "card_pad 8 -> 24 : %u rectangles changent", DiffCount(a, b, n0));
		check("22. CONTROLE POSITIF : changer une metrique DEPLACE les rectangles",
			  DiffCount(a, b, n0) > 0, buf);
	}
	{
		// CONTROLE NEGATIF : une metrique que personne ne nomme dans l'arbre ne
		// doit RIEN deplacer. Sans lui, le controle positif ne distinguerait pas
		// « le nombre est lu » de « toucher a la source change tout ».
		Tweak t{&cb, "footer_h", 999.f};
		NkSolveLayout(cb, SourceOf(t), panel, b, 32);
		snprintf(buf, sizeof(buf), "footer_h 34 -> 999 : %u differences (attendu : 0)",
				 DiffCount(a, b, n0));
		check("23. CONTROLE NEGATIF : une metrique qu'aucun agencement ne nomme ne bouge rien",
			  DiffCount(a, b, n0) == 0, buf);
	}
	{
		// `tree_width` est un PARAMETRE, pas une metrique. C'est le cas qui a
		// impose `NkComponentDecl::Number` : sans lui, le curseur de l'editeur
		// n'aurait deplace aucune colonne.
		const int32 ift = cb.ElementIndex("folder_tree");
		const int32 ig = cb.ElementIndex("grid");
		Tweak t{&cb, "tree_width", 0.30f};
		NkSolveLayout(cb, SourceOf(t), panel, b, 32);
		const bool wider = b[ift].w > a[ift].w;
		const bool absorbed = b[ig].w < a[ig].w;
		snprintf(buf, sizeof(buf), "arbre %.1f -> %.1f px, grille %.1f -> %.1f px", a[ift].w, b[ift].w,
				 a[ig].w, b[ig].w);
		check("24. un PARAMETRE pilote la disposition, et le frere a poids absorbe l'ecart",
			  wider && absorbed, buf);
	}
	{
		// LES BORNES SONT DES GARANTIES STRUCTURELLES, pas du style : c'est pour
		// ca qu'elles restent des nombres et ne se lisent pas dans le theme.
		const int32 ift = cb.ElementIndex("folder_tree");
		Tweak t{&cb, "tree_width", 0.90f};
		NkSolveLayout(cb, SourceOf(t), panel, c, 32);
		snprintf(buf, sizeof(buf), "fraction 0.90 de 884 px -> %.1f px (max declare : 400)",
				 c[ift].w);
		check("25. la borne HAUTE est honoree", c[ift].w <= 400.f + 0.01f, buf);
	}
	{
		// Panneau etroit : la borne basse du champ de recherche doit tenir meme
		// quand il ne reste plus rien a partager.
		const int32 is = cb.ElementIndex("search");
		const NkSolvedRect tiny{0.f, 0.f, 200.f, 600.f};
		Tweak t{&cb, nullptr, 0.f};
		NkSolveLayout(cb, SourceOf(t), tiny, c, 32);
		snprintf(buf, sizeof(buf), "panneau 200 px -> recherche %.1f px (min declare : 120)",
				 c[is].w);
		check("26. la borne BASSE est honoree quand la place manque", c[is].w >= 120.f - 0.01f, buf);
	}
	{
		// La ligne d'outils : deux boutons a taille de contenu et un champ
		// extensible. Le champ doit prendre tout le reste, ni plus ni moins.
		const int32 ic = cb.ElementIndex("btn_create");
		const int32 ii = cb.ElementIndex("btn_import");
		const int32 is = cb.ElementIndex("search");
		const int32 it = cb.ElementIndex("toolbar");
		const float32 gap = cb.Number("card_pad");
		const float32 sum = a[ic].w + a[ii].w + a[is].w + 2.f * gap;
		snprintf(buf, sizeof(buf), "%.1f + %.1f + %.1f + 2x%.1f = %.1f pour %.1f", a[ic].w, a[ii].w,
				 a[is].w, gap, sum, a[it].w);
		check("27. `extensible` prend EXACTEMENT le reste de la ligne", sum <= a[it].w + 0.01f &&
																			sum >= a[it].w - 0.01f,
			  buf);
	}
	{
		// L'ordre des freres est l'ordre de la table -- donc l'ordre de lecture du
		// fichier est l'ordre de l'interface. C'est ce qui rend une declaration
		// lisible par quelqu'un qui arrive.
		const int32 ih = cb.ElementIndex("header");
		const int32 it = cb.ElementIndex("toolbar");
		const int32 ib = cb.ElementIndex("body");
		check("28. l'ordre des freres dans la table est l'ordre a l'ecran",
			  a[ih].y <= a[it].y && a[it].y <= a[ib].y, "header, puis toolbar, puis body");
	}

	{
		// ⚠️ L'ESSAI QUI EXISTE POUR ECHOUER SI LA BOUCLE DISPARAIT. Trois enfants
		//    a poids egal dans 900 px, dont un borne a 100 px.
		//      une seule passe -> 100 + 300 + 300 = 700, et un creux de 200 px que
		//                         rien n'explique ;
		//      en boucle       -> 100 + 400 + 400 = 900, le parent est rempli.
		//    Sans cet essai, la correction adoptee de l'agent NkUIDesign serait du
		//    code que rien ne protege -- et la premiere simplification la defait.
		static const NkElementDecl els[] = {
			{"racine", "", "", "container", "", NkExpand(), NkExpand(),
			 {NkLayoutKind::Row, "", "", NkAlign::Start, NkAlign::Stretch, 0, ""}, 0},
			{"borne", "racine", "", "", "", NkExpand(0.f, 100.f), NkExpand(), {}, 0},
			{"libre1", "racine", "", "", "", NkExpand(), NkExpand(), {}, 0},
			{"libre2", "racine", "", "", "", NkExpand(), NkExpand(), {}, 0},
		};
		NkComponentDecl d;
		d.name = "partage";
		d.elements = els;
		d.elementCount = 4;
		NkSolvedRect r[8];
		NkMetricSource src = NkMetricsOf(d);
		NkSolveLayout(d, src, NkSolvedRect{0.f, 0.f, 900.f, 100.f}, r, 8);
		const float32 total = r[1].w + r[2].w + r[3].w;
		snprintf(buf, sizeof(buf), "%.1f + %.1f + %.1f = %.1f pour 900 (une seule passe donnerait 700)",
				 r[1].w, r[2].w, r[3].w, total);
		check("31. ce qu'une BORNE refuse est REDISTRIBUE aux autres (partage en boucle)",
			  total >= 899.99f && r[1].w <= 100.01f, buf);
	}

	// =======================================================================
	//  6. LES MOTS DU FICHIER
	// =======================================================================
	printf("\n--- 6. les mots du fichier : un seul point de verite ---\n");
	{
		// Aller-retour sur les trois enumerations qui finiront ecrites dans un
		// fichier de description. Un mot qui ne se relit pas est un fichier qu'on
		// ne rouvre pas -- et le defaut ne se verrait qu'a la reouverture, c'est-a-
		// dire trop tard.
		bool ok = true;
		for (uint8 i = 0; i < (uint8)NkSizeMode::Count; ++i) {
			NkSizeMode v;
			if (!NkParseSizeMode(NkSizeModeName((NkSizeMode)i), v) || v != (NkSizeMode)i)
				ok = false;
		}
		for (uint8 i = 0; i < (uint8)NkLayoutKind::Count; ++i) {
			NkLayoutKind v;
			if (!NkParseLayoutKind(NkLayoutKindName((NkLayoutKind)i), v) || v != (NkLayoutKind)i)
				ok = false;
		}
		for (uint8 i = 0; i < (uint8)NkAlign::Count; ++i) {
			NkAlign v;
			if (!NkParseAlign(NkAlignName((NkAlign)i), v) || v != (NkAlign)i)
				ok = false;
		}
		check("32. chaque mode, agencement et alignement fait l'ALLER-RETOUR texte", ok,
			  "fixed/content/fraction/weight/expand, none/row/column/grid/anchor, start/center/end/stretch");
	}
	{
		char s[5];
		NkAnchorName(nkanchor::Left | nkanchor::Bottom, s);
		const uint8 back = NkParseAnchor(s);
		snprintf(buf, sizeof(buf), "gauche+bas -> \"%s\" -> %u", s, (unsigned)back);
		check("33. les bords d'ancrage font l'aller-retour", back == (nkanchor::Left | nkanchor::Bottom),
			  buf);
	}
	{
		// TEMOIN NEGATIF des mots : un mot inconnu doit etre REFUSE sans toucher a
		// la sortie. Une lecture qui « reussit » sur un mot inconnu poserait
		// silencieusement la valeur 0 -- « fixed », c'est-a-dire le contraire du
		// responsive.
		NkAlign v = NkAlign::End;
		const bool refused = !NkParseAlign("aligne_a_ma_facon", v);
		check("34. TEMOIN NEGATIF : un mot inconnu est REFUSE et ne touche pas la sortie",
			  refused && v == NkAlign::End);
	}

	// =======================================================================
	//  7. LA PROVENANCE (ajout 4 de Rodolf)
	// =======================================================================
	printf("\n--- 7. la provenance ---\n");
	{
		// Trois champs, et le defaut decrit exactement une declaration ecrite a la
		// main dans ce depot. `verified = false` ne dit pas « fausse » : il dit
		// « pas encore passee au juge ».
		const bool ok = cb.provenance.author == NkAuthorKind::Human && !cb.provenance.verified &&
						!cb.provenance.corrected;
		snprintf(buf, sizeof(buf), "auteur=%s verifie=%d corrige=%d", NkAuthorName(cb.provenance.author),
				 cb.provenance.verified ? 1 : 0, cb.provenance.corrected ? 1 : 0);
		check("35. la declaration porte sa provenance, et son defaut dit la verite", ok, buf);
	}
	{
		// Les trois noms d'auteur s'ecrivent dans un fichier : un seul point de
		// verite, sinon deux orthographes et un fichier illisible par l'autre
		// moitie du code.
		const bool ok = NkComponentDecl::StrEq(NkAuthorName(NkAuthorKind::Human), "humain") &&
						NkComponentDecl::StrEq(NkAuthorName(NkAuthorKind::AI), "ia") &&
						NkComponentDecl::StrEq(NkAuthorName(NkAuthorKind::Imported), "importe");
		check("36. chaque origine a UN nom de fichier, et un seul", ok, "humain / ia / importe");
	}

	// =======================================================================
	//  7bis. LES LIBELLES SONT DES CLES (regle de Rodolf du 2026-08-18)
	// =======================================================================
	// LE CRITERE D'ECHEC DE CETTE PASSE EST ECRIT AU §11 DE `ROADMAP.md`.
	// ⚠️ ET SON ANTERIORITE N'EST PAS PROUVABLE, CONTRAIREMENT A CELLE DE LA PASSE
	//    PRECEDENTE : le §9 avait ete commite AVANT le §10, et l'ordre des deux
	//    commits en faisait foi. Ici, critere et resultat tombent dans le MEME
	//    commit. Il faut donc me croire sur parole, et c'est precisement ce qu'un
	//    banc est cense eviter -- c'est dit plutot que taise.
	// Le plus dur des quatre est le n°39 : si une
	// declaration a libelles en clair produisait ne serait-ce qu'UNE erreur, la
	// regle serait cassante, et il faudrait la RETIRER -- pas la garder en
	// esperant que les autres migrent vite.
	printf("\n--- 7bis. les libelles sont des CLES, et la regle ne casse personne ---\n");
	{
		const bool ok = NkIsLabelKey("content_browser.title") &&
						NkIsLabelKey("content_browser.variant.grid") &&
						NkIsLabelKey("thumb_size") && NkIsLabelKey("a1_b2.c3");
		check("37. une vraie cle de traduction est ACCEPTEE", ok,
			  "minuscules, chiffres, `_`, `.` comme separateur de niveau");
	}
	{
		// Chacun de ces refus a une raison : une cle traverse un fichier, un
		// catalogue et peut-etre un tableur de traduction. Ce qui s'encode mal
		// quelque part est banni d'avance.
		const bool ok = !NkIsLabelKey("Grille") &&             // majuscule
						!NkIsLabelKey("Taille des vignettes") && // espaces
						NkIsLabelKey("selection") &&             // (celle-ci EST valide)
						!NkIsLabelKey("") &&                     // vide : pas une cle
						!NkIsLabelKey(nullptr) && !NkIsLabelKey(".tete") &&
						!NkIsLabelKey("queue.") && !NkIsLabelKey("double..point");
		check("38. TEMOIN NEGATIF : texte en clair, majuscule, espace et point mal place sont REFUSES",
			  ok);
	}
	{
		// L'ESSAI LE PLUS IMPORTANT DE LA PASSE. La regle a ete ecrite alors que
		// DEUX composants portaient deja des libelles en clair, dont un ecrit par
		// une autre main qui compilait au meme moment sur ce fichier. Si elle
		// rougissait, elle punirait quelqu'un qui n'a rien casse.
		NkFormIssue iss[32];
		static const NkElementDecl els[] = {
			{"racine", "", "", "container", "", NkExpand(), NkExpand(),
			 {NkLayoutKind::Row, "gap", "pad", NkAlign::Start, NkAlign::Stretch, 0, ""}, 0},
			{"a", "racine", "", "label", "", NkExpand(), NkExpand(), {}, 0},
		};
		NkComponentDecl d = minimal(els, 2);
		d.title = "Navigateur de contenu"; // du texte, comme avant la regle
		NkCheckReport r = NkCheckComponent(d, iss, 32);
		uint16 n = 0;
		for (uint16 i = 0; i < r.written; ++i)
			if (NkComponentDecl::StrEq(iss[i].code, "libelle_non_cle"))
				++n;
		snprintf(buf, sizeof(buf), "%u erreur(s), %u note(s) `libelle_non_cle`", r.errors, n);
		check("39. un libelle en clair est une NOTE et JAMAIS une erreur (la regle ne casse personne)",
			  r.errors == 0 && n >= 1, buf);
	}
	{
		// Le controle positif du n°39 : une fois migre, le compte tombe a zero.
		// Sans lui, le n°39 se contenterait d'une note qui ne dependrait de rien.
		NkFormIssue iss[32];
		static const NkParamDecl kPk[] = {{"thumb_size", "temoin.param.thumb_size",
										   NkParamKind::Float, 96.f, 48.f, 256.f, nullptr, 0}};
		static const NkElementDecl els[] = {
			{"racine", "", "", "container", "", NkExpand(), NkExpand(),
			 {NkLayoutKind::Row, "gap", "pad", NkAlign::Start, NkAlign::Stretch, 0, ""}, 0},
			{"a", "racine", "", "label", "", NkExpand(), NkExpand(), {}, 0},
		};
		NkComponentDecl d = minimal(els, 2);
		d.title = "temoin.title";
		d.params = kPk;
		d.paramCount = 1;
		NkCheckReport r = NkCheckComponent(d, iss, 32);
		uint16 n = 0;
		for (uint16 i = 0; i < r.written; ++i)
			if (NkComponentDecl::StrEq(iss[i].code, "libelle_non_cle"))
				++n;
		check("40. CONTROLE POSITIF : une declaration entierement migree ne produit AUCUNE note",
			  n == 0 && r.errors == 0);
	}

	// =======================================================================
	//  7ter. LE GABARIT REPETE -- la reparation GENERALE annoncee au §10.4
	// =======================================================================
	printf("\n--- 7ter. le gabarit repete : le manque mesure chez DEUX composants ---\n");
	{
		// CET ESSAI PROUVE LA PROMESSE « CET AJOUT NE PEUT CASSER PERSONNE ».
		// Toutes les tables d'elements de ce banc ont ete ecrites AVANT que
		// `repeat` existe : elles s'arretent a `anchorEdges`. Si le champ avait
		// ete insere au milieu plutot qu'a la fin, ce fichier ne compilerait meme
		// pas -- c'est exactement ce qui est arrive le 19/08 avec `role`.
		static const NkElementDecl vieux[] = {
			{"racine", "", "", "container", "", NkExpand(), NkExpand(),
			 {NkLayoutKind::Row, "gap", "pad", NkAlign::Start, NkAlign::Stretch, 0, ""}, 0},
		};
		check("41. APPEND-ONLY : une table ecrite avant `repeat` compile et vaut `une fois`",
			  vieux[0].repeat == NkRepeatKind::Once && NkCheckComponent(minimal(vieux, 1)).errors == 0);
	}
	{
		static const NkElementDecl bad[] = {
			{"racine", "", "", "container", "", NkExpand(), NkExpand(),
			 {NkLayoutKind::Row, "gap", "pad", NkAlign::Start, NkAlign::Stretch, 0, ""}, 0,
			 NkRepeatKind::PerEntry},
		};
		check("42. une RACINE repetee est REFUSEE (un composant se repete en etant instancie)",
			  NkCheckComponent(minimal(bad, 1)).errors > 0);
	}
	{
		NkFormIssue iss[32];
		static const NkElementDecl els[] = {
			{"racine", "", "", "container", "", NkExpand(), NkExpand(),
			 {NkLayoutKind::Column, "gap", "pad", NkAlign::Start, NkAlign::Stretch, 0, ""}, 0},
			{"a", "racine", "", "label", "", NkExpand(), NkExpand(), {}, 0, NkRepeatKind::PerEntry},
			{"b", "racine", "", "label", "", NkExpand(), NkExpand(), {}, 0, NkRepeatKind::PerEntry},
		};
		NkCheckReport r = NkCheckComponent(minimal(els, 3), iss, 32);
		bool found = false;
		for (uint16 i = 0; i < r.written; ++i)
			if (NkComponentDecl::StrEq(iss[i].code, "repetition_freres"))
				found = true;
		check("43. deux freres repetes sont une NOTE, pas une erreur (le cas peut etre voulu)",
			  found && r.errors == 0);
	}
	{
		// LE POINT DE TOUTE LA REPARATION, et il se mesure en deux temps.
		// Temps 1 : une feuille qui declare un agencement -> la note tombe, parce
		//           que la structure repetee n'etait PAS exprimable.
		// Temps 2 : la meme, avec le gabarit enfin declare -> la note disparait.
		NkFormIssue a[32], b[32];
		static const NkElementDecl sans[] = {
			{"racine", "", "", "container", "", NkExpand(), NkExpand(),
			 {NkLayoutKind::Column, "gap", "pad", NkAlign::Start, NkAlign::Stretch, 0, ""}, 0},
			{"grid", "racine", "", "container", "", NkExpand(), NkExpand(),
			 {NkLayoutKind::Grid, "gap", "pad", NkAlign::Start, NkAlign::Stretch, 3, ""}, 0},
		};
		static const NkElementDecl avec[] = {
			{"racine", "", "", "container", "", NkExpand(), NkExpand(),
			 {NkLayoutKind::Column, "gap", "pad", NkAlign::Start, NkAlign::Stretch, 0, ""}, 0},
			{"grid", "racine", "", "container", "", NkExpand(), NkExpand(),
			 {NkLayoutKind::Grid, "gap", "pad", NkAlign::Start, NkAlign::Stretch, 3, ""}, 0},
			{"card", "grid", "une carte, UNE PAR ENTREE", "label", "", NkExpand(), NkExpand(), {}, 0,
			 NkRepeatKind::PerEntry},
		};
		auto countNote = [&](const NkElementDecl *e, uint16 n, NkFormIssue *out) -> uint16 {
			NkCheckReport r = NkCheckComponent(minimal(e, n), out, 32);
			uint16 c = 0;
			for (uint16 i = 0; i < r.written; ++i)
				if (NkComponentDecl::StrEq(out[i].code, "agencement_sans_enfant"))
					++c;
			return c;
		};
		const uint16 avant = countNote(sans, 2, a);
		const uint16 apres = countNote(avec, 3, b);
		snprintf(buf, sizeof(buf), "sans gabarit : %u note(s) -> avec gabarit : %u", avant, apres);
		check("44. le gabarit repete DESARME `agencement_sans_enfant` (la preuve de la reparation)",
			  avant >= 1 && apres == 0, buf);
	}
	{
		NkRepeatKind back = NkRepeatKind::Once;
		bool ok = true;
		for (uint8 i = 0; i < (uint8)NkRepeatKind::Count; ++i)
			if (!NkParseRepeatKind(NkRepeatKindName((NkRepeatKind)i), back) || back != (NkRepeatKind)i)
				ok = false;
		// TEMOIN NEGATIF, meme regle que le n°34 : un mot inconnu est refuse et ne
		// touche pas la sortie. Sans lui, un analyseur qui repond « oui » a tout
		// passerait l'aller-retour sans rien garantir.
		NkRepeatKind untouched = NkRepeatKind::PerEntryTree;
		const bool refuse = !NkParseRepeatKind("par_entree", untouched) &&
							untouched == NkRepeatKind::PerEntryTree;
		check("45. les trois repetitions font l'ALLER-RETOUR texte, et un mot inconnu est REFUSE",
			  ok && refuse, "once / per_entry / per_entry_tree");
	}

	// =======================================================================
	//  8. LA MESURE QUI N EST PAS UN ESSAI -- etat du second composant
	// =======================================================================
	// ⚠️ CE BLOC NE COMPTE PAS DANS LE RESULTAT, et c'est deliberе : il mesure un
	//    fichier EN COURS D'ECRITURE par un autre agent. En faire un essai
	//    reviendrait a rougir sur le travail d'autrui, ce qui n'est ni juste ni
	//    utile. Il est imprime pour que l'agent du composant sache exactement ou
	//    il en est vis-a-vis du contrat de role, le jour ou il posera
	//    `role = "tree"`.
	printf("\n--- 8. MESURE (hors essais) : ce que `tree_view` verrait avec `role = \"tree\"` ---\n");
	{
		NkComponentDecl probe = tv;
		probe.role = "tree";
		NkCheckReport r = NkCheckComponent(probe, issues, 48);
		printf("    role `tree` : %u erreur(s), %u note(s)\n", r.errors, r.notes);
		for (uint16 i = 0; i < r.written; ++i)
			printf("        %s %s (%s)\n", issues[i].level == NkIssueLevel::Error ? "ERREUR" : "note",
				   issues[i].code, issues[i].subject);
		if (r.errors == 0)
			printf("    -> le contrat est honore : `role = \"tree\"` peut etre pose.\n");
		else
			printf("    -> alignement en cours ; poser `role` maintenant rougirait le banc.\n");
	}

	printf("\n=== RESULTAT : %d / %d ===\n", gPass, gTotal);
	return (gPass == gTotal) ? 0 : 1;
}
