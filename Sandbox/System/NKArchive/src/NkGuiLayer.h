// =============================================================================
// Sandbox/System/NKArchive/src/NkGuiLayer.h
// LE BANC DE LA COUCHE `.nkgui` <-> NkArchive  (etape 4)
//
// T9, T10 et T11 avaient mesure les trois PREREQUIS de representation. Ce
// fichier mesure la COUCHE elle-meme : le lecteur, l'ecrivain, et le critere
// d'acceptation -- le corpus des 10 sources, OCTET POUR OCTET, le meme que
// `NkGuiFormat.h` tient deja. Sans lui, la couche ne remplace rien.
//
// =============================================================================
//  CE QUE « OCTET POUR OCTET » NE PROUVE PAS TOUT SEUL
// =============================================================================
//  Un « lecteur » qui rangerait le fichier ENTIER dans une seule chaine et le
//  recracherait passerait le corpus a 10/10. Le critere d'acceptation est donc
//  accompagne, pour CHAQUE fichier, de deux temoins qui le rendent impossible :
//
//   - un COMPTE DE BLOCS compare a celui d'une implementation INDEPENDANTE
//     (`NkGuiFormat.h`, releve dans `nkuidesign_roundtrip.txt` du 2026-08-22).
//     Comparer deux textes du meme ecrivain ne juge pas un changement de format ;
//     comparer une structure a un compte produit ailleurs, si ;
//   - un CONTROLE POSITIF : une propriete est MODIFIEE DANS L'ARCHIVE, et la
//     sortie doit alors DIFFERER de l'original. Le texte passe donc bien par
//     l'archive, il ne la contourne pas.
//
// =============================================================================
//  LES OCTETS SONT LUS EN BINAIRE, ET C'EST DELIBERE
// =============================================================================
//  Le corpus est en CRLF. Une lecture en mode texte traduirait les fins de ligne
//  a l'entree, et l'aller-retour serait mesure sur un fichier que personne n'a
//  sur son disque. `fopen(..., "rb")` : les octets, tels quels.
//
// Auteur : Rihen
// License : Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================

#pragma once

#include <cstdio>

#include "NKSerialization/NkGui/NkGuiArchive.h"

// ---------------------------------------------------------------------------
//  OUTILLAGE
// ---------------------------------------------------------------------------

/// Lit un fichier EN OCTETS. Rend false si le fichier n'existe pas -- et le banc
/// le dit au lieu de compter un controle vert sur un fichier absent.
static bool NkGLoadBytes(const char *path, NkString &out) {
	FILE *f = nullptr;
#if defined(_WIN32)
	if (fopen_s(&f, path, "rb") != 0) {
		f = nullptr;
	}
#else
	f = fopen(path, "rb");
#endif
	if (!f) {
		return false;
	}
	fseek(f, 0, SEEK_END);
	const long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (n < 0) {
		fclose(f);
		return false;
	}
	char *buf = new char[(size_t)n + 1];
	const size_t got = fread(buf, 1, (size_t)n, f);
	fclose(f);
	buf[got] = 0;
	out = NkString(buf, (NkString::SizeType)got);
	delete[] buf;
	return true;
}

/// Le premier octet qui differe, ou -1 si les deux textes sont identiques.
static long NkGFirstDiff(const NkString &a, const NkString &b) {
	const nk_size n = (a.Size() < b.Size()) ? a.Size() : b.Size();
	for (nk_size i = 0; i < n; ++i) {
		if (a.Data()[i] != b.Data()[i]) {
			return (long)i;
		}
	}
	if (a.Size() != b.Size()) {
		return (long)n;
	}
	return -1;
}

struct NkGCounts {
		int blocks = 0;
		int props = 0;
		int raws = 0;
};

static void NkGCount(const NkArchive &ar, NkGCounts &c) {
	const NkVector<NkArchiveEntry> &e = ar.Entries();
	for (nk_size i = 0; i < e.Size(); ++i) {
		if (!NkGuiArchive::IsReservedKey(NkStringView(e[i].key))) {
			++c.props;
		}
	}
	const NkArchiveNode *body = ar.FindNode(NkStringView(NkGuiArchive::KeyBody()));
	if (!body || !body->IsArray()) {
		return;
	}
	for (nk_size i = 0; i < body->array.Size(); ++i) {
		const NkArchiveNode &n = body->array[i];
		if (n.IsObject() && n.object) {
			++c.blocks;
			NkGCount(*n.object, c);
		} else {
			++c.raws;
		}
	}
}

/// Modifie LA PREMIERE propriete trouvee, en profondeur d'abord. Rend le nom de
/// la propriete touchee, vide si le document n'en a aucune.
static NkString NkGMutateFirstProp(NkArchive &ar, const char *nouvelle) {
	NkVector<NkArchiveEntry> &e = ar.Entries();
	for (nk_size i = 0; i < e.Size(); ++i) {
		if (NkGuiArchive::IsReservedKey(NkStringView(e[i].key))) {
			continue;
		}
		const NkString key = e[i].key;
		ar.SetString(NkStringView(key), NkStringView(nouvelle));
		return key;
	}
	NkArchiveNode *body = ar.FindNode(NkStringView(NkGuiArchive::KeyBody()));
	if (!body || !body->IsArray()) {
		return NkString();
	}
	for (nk_size i = 0; i < body->array.Size(); ++i) {
		if (!body->array[i].IsObject() || !body->array[i].object) {
			continue;
		}
		const NkString k = NkGMutateFirstProp(*body->array[i].object, nouvelle);
		if (!k.Empty()) {
			return k;
		}
	}
	return NkString();
}

/// L'aller-retour sur un texte tenu A LA MAIN : lire, reecrire avec le style
/// detecte dans la source, comparer les octets.
static bool NkGRoundTrip(const char *src, NkString &emitted, NkGuiDiag &err) {
	const nk_uint32 n = (nk_uint32)NkString(src).Size();
	NkArchive ar;
	if (!NkGuiArchive::Read(src, n, ar, err)) {
		return false;
	}
	emitted = NkGuiArchive::Write(ar, NkGuiArchive::DetectStyle(src, n));
	return true;
}

// =============================================================================
// L1 -- LE CORPUS DES 10 SOURCES, OCTET POUR OCTET
// =============================================================================
// Le critere d'acceptation, et ses deux temoins (voir l'en-tete du fichier).
//
// Les comptes attendus viennent de `nkuidesign_roundtrip.txt`, produit par
// `NkGuiFormat.h` -- une implementation INDEPENDANTE, ecrite avant celle-ci et
// avec un modele entierement different. Elle compte les NOEUDS DE WIDGET ; cette
// couche compte les BLOCS, et un bloc de plus existe : la section `widgets`
// elle-meme. D'ou le `+ 1`, qui n'est pas un ajustement de confort mais la
// difference exacte entre « ce que le modele voit » et « ce que la syntaxe dit ».
struct NkGCorpusCase {
		const char *file;
		int nkgFormatNodes;	 ///< releve independant (NkGuiFormat.h)
};

static void L1_Corpus(const char *dir) {
	printf("[L1] Le corpus des 10 sources, octet pour octet -- critere d'acceptation\n");

	static const NkGCorpusCase kCases[] = {
		{"38a8800b62b6.nkgui", 979},	 {"40e50c776b73.nkgui", 333},
		{"446dcf695d68.nkgui", 1614}, {"544cb68c379e.nkgui", 366},
		{"6bc4296cb7c9.nkgui", 1230}, {"ac27fa2f402b.nkgui", 493},
		{"bc88b8dc8a45.nkgui", 2940}, {"c8c427d5bf3a.nkgui", 3015},
		{"e723a8f915d6.nkgui", 270},	 {"f72eb72fdef9.nkgui", 336},
	};
	const int kN = (int)(sizeof(kCases) / sizeof(kCases[0]));

	int octet = 0;
	for (int i = 0; i < kN; ++i) {
		NkString path(dir);
		path.Append('/');
		path.Append(kCases[i].file);

		NkString src;
		// ⚠️ UN FICHIER ABSENT DOIT ETRE UN ECHEC, PAS UN SAUT. Un banc qui passe
		//    son chemin sur ce qu'il ne trouve pas rend un vert qui ne mesure rien.
		if (!NkGLoadBytes(path.CStr(), src)) {
			printf("  FAIL  fichier introuvable : %s\n", path.CStr());
			++s_fail;
			continue;
		}

		NkArchive ar;
		NkGuiDiag err;
		const nk_uint32 n = (nk_uint32)src.Size();
		if (!NkGuiArchive::Read(src.Data(), n, ar, err)) {
			printf("  FAIL  %s refuse : %s %s (l.%u c.%u)\n", kCases[i].file, err.code.CStr(),
				   err.message.CStr(), err.line, err.column);
			++s_fail;
			continue;
		}

		// -- temoin 1 : la STRUCTURE, comptee par une implementation independante
		NkGCounts c;
		NkGCount(ar, c);
		if (c.blocks != kCases[i].nkgFormatNodes + 1) {
			printf("  FAIL  %s : %d blocs, %d attendus (NkGuiFormat : %d noeuds + la section)\n",
				   kCases[i].file, c.blocks, kCases[i].nkgFormatNodes + 1,
				   kCases[i].nkgFormatNodes);
			++s_fail;
		} else {
			++s_pass;
		}

		const NkGuiStyle style = NkGuiArchive::DetectStyle(src.Data(), n);
		const NkString out = NkGuiArchive::Write(ar, style);
		const long d = NkGFirstDiff(src, out);
		if (d < 0) {
			++octet;
			++s_pass;
		} else {
			printf("  FAIL  %s : premiere difference a l'octet %ld (%u lu, %u ecrit)\n",
				   kCases[i].file, d, (unsigned)src.Size(), (unsigned)out.Size());
			++s_fail;
		}

		// -- temoin 2 : CONTROLE POSITIF. Le texte passe par l'archive, il ne la
		//    contourne pas : une propriete modifiee DANS L'ARCHIVE change la sortie.
		NkArchive mute = ar;
		const NkString touched = NkGMutateFirstProp(mute, "un texte que le corpus n'a pas");
		const NkString out2 = NkGuiArchive::Write(mute, style);
		if (touched.Empty() || NkGFirstDiff(out, out2) < 0) {
			printf("  FAIL  %s : la sortie ne bouge pas quand l'archive bouge\n", kCases[i].file);
			++s_fail;
		} else {
			++s_pass;
		}
	}
	printf("       -> %d / %d fichiers identiques OCTET POUR OCTET\n", octet, kN);
	EXPECT_TRUE(octet == kN);
}

// =============================================================================
// L2 -- L'ENTRELACEMENT, EN VRAI (T9 n'en avait mesure que la representation)
// =============================================================================
static void L2_Entrelacement() {
	printf("[L2] L'ordre entrelace proprietes / enfants, dans la couche\n");

	const char *src =
		"nkgui 0.3\n"
		"widgets {\n"
		"  VBox \"v\" {\n"
		"    a = 1\n"
		"    Text \"t\" { }\n"
		"    b = 2\n"
		"  }\n"
		"}\n";

	NkString out;
	NkGuiDiag err;
	EXPECT_TRUE(NkGRoundTrip(src, out, err));
	EXPECT_STREQ(out, NkString(src));

	// Et le temoin qui empeche « verbatim » de passer pour « structure » : les
	// deux proprietes ET l'enfant sont dans l'archive, chacun a sa place.
	NkArchive ar;
	EXPECT_TRUE(NkGuiArchive::Read(src, (nk_uint32)NkString(src).Size(), ar, err));
	const NkArchiveNode *body = ar.FindNode(NkStringView(NkGuiArchive::KeyBody()));
	EXPECT_TRUE(body && body->IsArray() && body->array.Size() == 1);
	if (!body || !body->IsArray() || body->array.Empty()) {
		return;
	}
	const NkArchive *widgets = body->array[0].object;
	EXPECT_TRUE(widgets != nullptr);
	if (!widgets) {
		return;
	}
	const NkArchiveNode *wb = widgets->FindNode(NkStringView(NkGuiArchive::KeyBody()));
	EXPECT_TRUE(wb && wb->IsArray() && wb->array.Size() == 1);
	if (!wb || !wb->IsArray() || wb->array.Empty()) {
		return;
	}
	const NkArchive *v = wb->array[0].object;
	EXPECT_TRUE(v != nullptr);
	if (!v) {
		return;
	}
	EXPECT_TRUE(v->Has(NkStringView("a")) && v->Has(NkStringView("b")));
	// L'ENFANT EST ENTRE LES DEUX, et c'est le rang qui le dit -- pas la position
	// dans le conteneur, qui les separe justement en deux suites.
	EXPECT_TRUE(v->GetSourceOrder(NkStringView("a")) < v->GetSourceOrder(NkStringView("b")));
	const NkArchiveNode *vb = v->FindNode(NkStringView(NkGuiArchive::KeyBody()));
	EXPECT_TRUE(vb && vb->IsArray() && vb->array.Size() == 1);
	if (vb && vb->IsArray() && !vb->array.Empty()) {
		const nk_int32 kid = vb->array[0].SourceOrder();
		EXPECT_TRUE(kid > v->GetSourceOrder(NkStringView("a"))
					&& kid < v->GetSourceOrder(NkStringView("b")));
	}
}

// =============================================================================
// L3 -- COMMENTAIRES, LIGNES VIDES, COMMENTAIRE DE FIN DE LIGNE
// =============================================================================
static void L3_Commentaires() {
	printf("[L3] Commentaires, lignes vides et fins de ligne survivent\n");

	const char *src =
		"// l'en-tete du fichier\n"
		"nkgui 0.3   // et sa version\n"
		"\n"
		"widgets {   // apres l'accolade\n"
		"  // avant le bloc\n"
		"\n"
		"  VBox \"v\" {\n"
		"    a = 1   // apres la valeur\n"
		"    // avant l'accolade fermante\n"
		"  }\n"
		"}\n"
		"// et la fin du fichier\n";

	NkString out;
	NkGuiDiag err;
	EXPECT_TRUE(NkGRoundTrip(src, out, err));
	EXPECT_STREQ(out, NkString(src));
}

// =============================================================================
// L4 -- CE QU'ON NE COMPREND PAS  (la regle (d), devenue le regime normal)
// =============================================================================
static void L4_Inconnu() {
	printf("[L4] Une section d'une version future se relit et se reemet telle quelle\n");

	// `theme_futur` a la FORME d'un bloc : la couche etant purement syntaxique,
	// elle le lit comme n'importe quel autre. C'est ca, la regle (d) qui cesse
	// d'etre un mecanisme.
	const char *src =
		"nkgui 0.9\n"
		"theme_futur {\n"
		"  couleur = #F79A28\n"
		"  Palette \"p\" { }\n"
		"}\n"
		"behavior \"b\" {\n"
		"  set n1.value = a + 2\n"
		"  a.out -> b.in\n"
		"}\n";

	NkString out;
	NkGuiDiag err;
	EXPECT_TRUE(NkGRoundTrip(src, out, err));
	EXPECT_STREQ(out, NkString(src));

	NkArchive ar;
	EXPECT_TRUE(NkGuiArchive::Read(src, (nk_uint32)NkString(src).Size(), ar, err));
	NkGCounts c;
	NkGCount(ar, c);
	// deux blocs de tete + le bloc Palette = 3 ; deux constructions que la
	// couche NE MODELISE PAS (`set ...` et `a.out -> b.in`) = 2 tranches brutes.
	EXPECT_TRUE(c.blocks == 3);
	EXPECT_TRUE(c.raws == 2);

	// ⚠️ T11, POINT 6 : une tranche brute est du CONTENU, pas de la mise en forme.
	//    Rangee dans la trivia, elle disparaitrait d'un export JSON ou du binaire
	//    natif SANS QUE RIEN NE LE SIGNALE. Elle est donc un noeud CHAINE visible.
	const NkArchiveNode *body = ar.FindNode(NkStringView(NkGuiArchive::KeyBody()));
	EXPECT_TRUE(body && body->IsArray() && body->array.Size() == 2);
	if (body && body->IsArray() && body->array.Size() == 2 && body->array[1].object) {
		const NkArchiveNode *bb =
			body->array[1].object->FindNode(NkStringView(NkGuiArchive::KeyBody()));
		EXPECT_TRUE(bb && bb->IsArray() && bb->array.Size() == 2);
		if (bb && bb->IsArray() && bb->array.Size() == 2) {
			EXPECT_TRUE(bb->array[0].IsScalar()
						&& bb->array[0].value.type == NkArchiveValueType::NK_VALUE_STRING);
			EXPECT_STREQ(NkString(bb->array[0].Lexeme()), NkString("set n1.value = a + 2"));
			EXPECT_TRUE(!bb->array[0].HasUsableLiteral());  // aucun mecanisme : T11
		}
	}
}

// =============================================================================
// L5 -- LE CANARI DU `$`  (la garantie EMPRUNTEE, gardee la ou elle est produite)
// =============================================================================
// Les cles reservees `$type` / `$id` / `$body` ne sont sures que parce que `$`
// n'est PAS un caractere d'identifiant du lexeur. Cette garantie ne vient pas de
// l'archive : elle vient du lexeur, dix lignes plus haut dans le meme module.
// Le jour ou quelqu'un ajoutera `$` aux identifiants -- pour des variables, pour
// une interpolation -- ce controle tombera, et c'est tout le but.
static void L5_CanariDollar() {
	printf("[L5] Le canari : `$` reste illegal dans un identifiant\n");

	const char *interdit = "nkgui 0.3\nwidgets {\n  $type = 1\n}\n";
	NkArchive ar;
	NkGuiDiag err;
	const bool lu = NkGuiArchive::Read(interdit, (nk_uint32)NkString(interdit).Size(), ar, err);
	EXPECT_TRUE(!lu);
	EXPECT_STREQ(err.code, NkString("E-PARSE"));

	// LE TEMOIN, sans lequel le refus pourrait venir de n'importe quoi d'autre :
	// le MEME fichier avec un nom legal se lit.
	const char *permis = "nkgui 0.3\nwidgets {\n  type = 1\n}\n";
	NkArchive ok;
	NkGuiDiag err2;
	EXPECT_TRUE(NkGuiArchive::Read(permis, (nk_uint32)NkString(permis).Size(), ok, err2));
}

// =============================================================================
// L6 -- LA VERSION  (regles (a) et (c))
// =============================================================================
static void L6_Version() {
	printf("[L6] La version : reemise telle quelle, et une majeure trop recente est refusee\n");

	const char *futur = "nkgui 1.0\nwidgets {\n}\n";
	NkArchive ar;
	NkGuiDiag err;
	EXPECT_TRUE(!NkGuiArchive::Read(futur, (nk_uint32)NkString(futur).Size(), ar, err));
	EXPECT_STREQ(err.code, NkString("E-VERSION-INCOMPATIBLE"));

	// (a) UN DOCUMENT ECRIT EN 0.2 SE REECRIT EN 0.2. Un outil qui reestampille
	//     en silence les fichiers qu'il touche rend tout diagnostic impossible.
	const char *ancien = "nkgui 0.2\nwidgets {\n  Text \"t\" { }\n}\n";
	NkString out;
	NkGuiDiag e2;
	EXPECT_TRUE(NkGRoundTrip(ancien, out, e2));
	EXPECT_STREQ(out, NkString(ancien));
}

// =============================================================================
// L7 -- LE BLOC VIDE : `{ }` ou deux lignes, selon CE QUE LE FICHIER DISAIT
// =============================================================================
static void L7_BlocVide() {
	printf("[L7] Un bloc vide garde la forme qu'il avait dans le fichier\n");

	const char *src =
		"nkgui 0.3\n"
		"widgets {\n"
		"  Image \"a\" { }\n"
		"  Image \"b\" {\n"
		"  }\n"
		"}\n";

	NkString out;
	NkGuiDiag err;
	EXPECT_TRUE(NkGRoundTrip(src, out, err));
	EXPECT_STREQ(out, NkString(src));
}

// =============================================================================
// L8 -- LE JETON NU, ET LA MOITIE MANQUANTE DE SetString
// =============================================================================
static void L8_JetonNu() {
	printf("[L8] Un document FABRIQUE PAR LE CODE s'ecrit, et les jetons nus le restent\n");

	NkArchive doc;
	NkGuiArchive::SetToken(doc, NkStringView(NkGuiArchive::KeyVersion()), NkStringView("0.3"));
	NkArchiveNode *sec = NkGuiArchive::AddBlock(doc, NkStringView("widgets"), NkStringView(""));
	EXPECT_TRUE(sec != nullptr);
	if (!sec || !sec->object) {
		return;
	}
	NkArchive *w = sec->object;
	NkArchiveNode *btn = NkGuiArchive::AddBlock(*w, NkStringView("Button"), NkStringView("ok"));
	EXPECT_TRUE(btn != nullptr);
	if (!btn || !btn->object) {
		return;
	}
	btn->object->SetString(NkStringView("label"), NkStringView("Envoyer \"tout\""));
	NkGuiArchive::SetToken(*btn->object, NkStringView("couleur"), NkStringView("#F79A28"));
	btn->object->SetBool(NkStringView("actif"), true);

	NkGuiStyle st;
	st.crlf = false;
	const NkString out = NkGuiArchive::Write(doc, st);

	// La chaine repart ENTRE GUILLEMETS et ECHAPPEE ; le jeton nu repart NU ;
	// le booleen repart canoniquement.
	const NkString attendu(
		"nkgui 0.3\n"
		"widgets {\n"
		"  Button \"ok\" {\n"
		"    label = \"Envoyer \\\"tout\\\"\"\n"
		"    couleur = #F79A28\n"
		"    actif = true\n"
		"  }\n"
		"}\n");
	EXPECT_STREQ(out, attendu);

	// Et ca se relit : l'aller-retour part aussi d'un document sans fichier.
	NkString re;
	NkGuiDiag err;
	EXPECT_TRUE(NkGRoundTrip(out.CStr(), re, err));
	EXPECT_STREQ(re, attendu);

	// ⚠️ CE QUE LE GARDE-FOU ANTI-PERIME FAIT ICI, et il vaut mieux le mesurer
	//    que le croire : un jeton nu dont on change la valeur SANS repasser par
	//    SetToken repart ENTRE GUILLEMETS. C'est visible, donc rattrapable ; une
	//    sortie qui aurait l'air correcte ne le serait pas.
	NkArchive relu;
	NkGuiDiag e3;
	EXPECT_TRUE(NkGuiArchive::Read(out.Data(), (nk_uint32)out.Size(), relu, e3));
	NkArchiveNode *b2 = relu.FindNode(NkStringView(NkGuiArchive::KeyBody()));
	if (b2 && b2->IsArray() && !b2->array.Empty() && b2->array[0].object) {
		NkArchiveNode *wb = b2->array[0].object->FindNode(NkStringView(NkGuiArchive::KeyBody()));
		if (wb && wb->IsArray() && !wb->array.Empty() && wb->array[0].object) {
			NkArchive &bt = *wb->array[0].object;
			EXPECT_TRUE(NkGuiArchive::IsToken(*bt.FindNode(NkStringView("couleur"))));
			bt.SetString(NkStringView("couleur"), NkStringView("#000000"));
			EXPECT_TRUE(!NkGuiArchive::IsToken(*bt.FindNode(NkStringView("couleur"))));
			const NkString abime = NkGuiArchive::Write(relu, st);
			EXPECT_TRUE(NkGFirstDiff(abime, out) >= 0);
			// et la preuve que la difference est BIEN la mise entre guillemets
			NkArchive verif;
			NkGuiDiag e4;
			EXPECT_TRUE(NkGuiArchive::Read(abime.Data(), (nk_uint32)abime.Size(), verif, e4));
		}
	}
	// La contre-epreuve : repasser par SetToken rend le jeton nu.
	NkArchive rejoue;
	NkGuiDiag e5;
	EXPECT_TRUE(NkGuiArchive::Read(out.Data(), (nk_uint32)out.Size(), rejoue, e5));
	NkArchiveNode *b3 = rejoue.FindNode(NkStringView(NkGuiArchive::KeyBody()));
	if (b3 && b3->IsArray() && !b3->array.Empty() && b3->array[0].object) {
		NkArchiveNode *wb = b3->array[0].object->FindNode(NkStringView(NkGuiArchive::KeyBody()));
		if (wb && wb->IsArray() && !wb->array.Empty() && wb->array[0].object) {
			NkGuiArchive::SetToken(*wb->array[0].object, NkStringView("couleur"),
								   NkStringView("#000000"));
			const NkString bon = NkGuiArchive::Write(rejoue, st);
			EXPECT_TRUE(NkGFirstDiff(bon, out) >= 0);  // la valeur a bien change
			NkArchive v2;
			NkGuiDiag e6;
			EXPECT_TRUE(NkGuiArchive::Read(bon.Data(), (nk_uint32)bon.Size(), v2, e6));
		}
	}
}

// =============================================================================
// L9 -- LA COPIE PAR VALEUR  (`NkUIDocument` EST copiee, cinq fois dans Probe.h)
// =============================================================================
static void L9_Copie() {
	printf("[L9] L'archive lue survit a une copie profonde, mise en forme comprise\n");

	const char *src =
		"nkgui 0.3\n"
		"// un commentaire\n"
		"widgets {\n"
		"  VBox \"v\" {\n"
		"    epaisseur = 0.50   // un litteral que la forme canonique n'aurait pas\n"
		"  }\n"
		"}\n";

	NkArchive ar;
	NkGuiDiag err;
	const nk_uint32 n = (nk_uint32)NkString(src).Size();
	EXPECT_TRUE(NkGuiArchive::Read(src, n, ar, err));

	// La copie, puis on DETRUIT l'original : si quoi que ce soit etait partage,
	// la copie ne survivrait pas.
	NkArchive *tas = new NkArchive(ar);
	NkArchive copie = *tas;
	delete tas;
	ar.Clear();
	ar.ClearTrivia();

	const NkGuiStyle st = NkGuiArchive::DetectStyle(src, n);
	EXPECT_STREQ(NkGuiArchive::Write(copie, st), NkString(src));
}

// =============================================================================
// L10 -- UNE PROPRIETE AJOUTEE APRES LECTURE VA A LA FIN
// =============================================================================
static void L10_AjoutApresLecture() {
	printf("[L10] Une propriete ajoutee apres lecture s'ecrit A LA FIN, pas au hasard\n");

	const char *src = "nkgui 0.3\nVBox \"v\" {\n  a = 1\n  b = 2\n}\n";
	NkArchive ar;
	NkGuiDiag err;
	EXPECT_TRUE(NkGuiArchive::Read(src, (nk_uint32)NkString(src).Size(), ar, err));

	NkArchiveNode *body = ar.FindNode(NkStringView(NkGuiArchive::KeyBody()));
	EXPECT_TRUE(body && body->IsArray() && !body->array.Empty());
	if (!body || !body->IsArray() || body->array.Empty() || !body->array[0].object) {
		return;
	}
	body->array[0].object->SetInt64(NkStringView("c"), 3);

	NkGuiStyle st;
	st.crlf = false;
	const NkString attendu("nkgui 0.3\nVBox \"v\" {\n  a = 1\n  b = 2\n  c = 3\n}\n");
	EXPECT_STREQ(NkGuiArchive::Write(ar, st), attendu);
}

// =============================================================================
// L11 -- LA PROPRIETE EN DOUBLE : conservee, pas perdue
// =============================================================================
// Une archive n'a qu'UNE entree par cle. Un fichier qui repete une propriete
// (ce que le format n'interdit pas syntaxiquement) verrait la premiere ECRASEE
// -- une perte silencieuse. La seconde occurrence est donc gardee en tranche
// brute : elle sort telle qu'elle est entree.
static void L11_ProprieteEnDouble() {
	printf("[L11] Une propriete repetee n'est pas ecrasee en silence\n");

	const char *src = "nkgui 0.3\nVBox \"v\" {\n  a = 1\n  a = 2\n}\n";
	NkString out;
	NkGuiDiag err;
	EXPECT_TRUE(NkGRoundTrip(src, out, err));
	EXPECT_STREQ(out, NkString(src));

	NkArchive ar;
	EXPECT_TRUE(NkGuiArchive::Read(src, (nk_uint32)NkString(src).Size(), ar, err));
	NkGCounts c;
	NkGCount(ar, c);
	EXPECT_TRUE(c.props == 1 && c.raws == 1);
}

// =============================================================================
// L12 -- LE LITTERAL D'UNE CHAINE : LE SEUL CAS OU IL SERT VRAIMENT
// =============================================================================
// ⚠️ CE CONTROLE EXISTE PARCE QU'UNE MUTATION A SURVECU. Mutation M11 -- aucun
//    litteral pose sur une chaine LUE -- restait a 218/218, LE CORPUS COMPRIS.
//    Raison : le re-encodage de l'ecrivain est l'inverse exact du decodage du
//    lexeur, donc pour toute chaine du corpus il redonne OCTET POUR OCTET le
//    lexeme d'origine. Le litteral ne servait a rien... sur ce corpus.
//
//    Il existe pourtant un cas ou la source et le re-encodage DIFFERENT, et un
//    seul : un SAUT DE LIGNE ECRIT TEL QUEL dans la chaine. Le lexeur l'accepte
//    (il ne s'arrete qu'au guillemet fermant) et le decode en `\n` ; l'ecrivain,
//    lui, ecrit `\n` en deux caracteres. Sans litteral, la chaine reste la meme
//    mais le fichier change -- exactement le genre de reecriture silencieuse
//    qu'on ne veut pas.
//
// >>> Le corpus ne pouvait pas voir ca. Dix fichiers verts ne remplacent pas une
//     mutation : c'est elle qui a dit quelle partie du code n'etait pas mesuree.
static void L12_LitteralDeChaine() {
	printf("[L12] Le litteral d'une chaine sert quand le re-encodage n'est PAS la source\n");

	// La chaine porte un VRAI saut de ligne, pas la sequence `\n`.
	const char *src =
		"nkgui 0.3\n"
		"Text \"t\" {\n"
		"  contenu = \"deux\nlignes\"\n"
		"}\n";

	NkString out;
	NkGuiDiag err;
	EXPECT_TRUE(NkGRoundTrip(src, out, err));
	EXPECT_STREQ(out, NkString(src));

	// Le TEMOIN qui empeche ce controle d'etre vert pour une mauvaise raison :
	// la valeur DECODEE porte bien un saut de ligne (donc le lexeur a fait son
	// travail), et sans le litteral l'ecrivain ecrirait deux caracteres.
	NkArchive ar;
	EXPECT_TRUE(NkGuiArchive::Read(src, (nk_uint32)NkString(src).Size(), ar, err));
	NkArchiveNode *body = ar.FindNode(NkStringView(NkGuiArchive::KeyBody()));
	EXPECT_TRUE(body && body->IsArray() && !body->array.Empty());
	if (!body || !body->IsArray() || body->array.Empty() || !body->array[0].object) {
		return;
	}
	NkArchiveNode *v = body->array[0].object->FindNode(NkStringView("contenu"));
	EXPECT_TRUE(v != nullptr && v->IsScalar());
	if (!v) {
		return;
	}
	EXPECT_STREQ(v->value.text, NkString("deux\nlignes"));
	EXPECT_TRUE(v->HasUsableLiteral());

	// CONTROLE POSITIF DU MECANISME. ⚠️ Premiere version fausse, et elle a echoue
	// tout de suite : je « desarmais » le litteral en REPOSANT LA MEME VALEUR --
	// or le garde-fou anti-perime compare le litteral au texte canonique COURANT,
	// qui n'avait pas bouge. Reposer une valeur identique ne desarme rien, et
	// c'est exactement ce qu'on veut de lui.
	//
	// Le bon controle dit la vraie phrase : LA MEME VALEUR, POSEE SANS LITTERAL,
	// ne redonne PAS le meme fichier. C'est ca que le litteral achete.
	NkArchive nu;
	NkGuiArchive::SetToken(nu, NkStringView(NkGuiArchive::KeyVersion()), NkStringView("0.3"));
	NkArchiveNode *t = NkGuiArchive::AddBlock(nu, NkStringView("Text"), NkStringView("t"));
	EXPECT_TRUE(t != nullptr && t->object != nullptr);
	if (!t || !t->object) {
		return;
	}
	t->object->SetString(NkStringView("contenu"), NkStringView("deux\nlignes"));
	// la VALEUR est bien la meme des deux cotes
	EXPECT_STREQ(t->object->FindNode(NkStringView("contenu"))->value.text, v->value.text);
	NkGuiStyle st;
	st.crlf = false;
	const NkString sans = NkGuiArchive::Write(nu, st);
	EXPECT_TRUE(NkGFirstDiff(sans, NkString(src)) >= 0);
	// Le saut de ligne est reecrit ECHAPPE : la valeur est intacte, le fichier
	// non. C'est precisement ce que le litteral evite sur un fichier lu.
	EXPECT_STREQ(sans, NkString("nkgui 0.3\nText \"t\" {\n  contenu = \"deux\\nlignes\"\n}\n"));
}

#include "NkGuiDocCorpus.h"

// =============================================================================
// L14 -- LE BLOC SUR UNE LIGNE : ET LA ON MESURE L'ARCHIVE, PAS LES OCTETS
// =============================================================================
// ATTENTION -- CE CONTROLE EXISTE PARCE QUE DEUX MUTATIONS ONT SURVECU, ET
// ELLES DISAIENT TOUTES LES DEUX LA MEME CHOSE.
//
//   M17 (SpanEnd ne s'arrete plus a la virgule) et M18 (SpanEnd ne s'arrete plus
//   devant `Ident =`) restaient a 238/238. Raison : sans ces arrets, la valeur
//   de la premiere propriete AVALE le reste de la ligne -- `offset` vaut alors
//   `(0, 2), blur = 6, color = #0000003A`. La tranche etant reemise VERBATIM, le
//   fichier ressort a l'octet pres. Le fichier est juste, L'ARCHIVE EST FAUSSE.
//
// >>> C'EST LE PIRE CAS QU'UN BANC PUISSE LAISSER PASSER : vert et faux. Un
//     aller-retour ne peut PAS le voir, par construction -- ce qui entre
//     verbatim ressort verbatim. Il faut regarder ce qu'il y a DANS l'archive.
static const NkArchive *NkGSousBloc(const NkArchive &parent, nk_size i) {
	const NkArchiveNode *b = parent.FindNode(NkStringView(NkGuiArchive::KeyBody()));
	if (!b || !b->IsArray() || i >= b->array.Size() || !b->array[i].IsObject()) {
		return nullptr;
	}
	return b->array[i].object;
}

static void L14_BlocSurUneLigne() {
	printf("[L14] Un bloc sur UNE ligne, et ce que l'archive en contient vraiment\n");

	const char *src =
		"nkgui 0.3\n"
		"appearance {\n"
		"  shadow { offset = (0, 2), blur = 6, color = #0000003A }\n"
		"  space { a = 1 b = 2 }\n"
		"  track \"s\" { key 0.0 -> 1.0 }\n"
		"}\n";

	NkString out;
	NkGuiDiag err;
	EXPECT_TRUE(NkGRoundTrip(src, out, err));
	EXPECT_STREQ(out, NkString(src));

	NkArchive ar;
	EXPECT_TRUE(NkGuiArchive::Read(src, (nk_uint32)NkString(src).Size(), ar, err));
	const NkArchive *app = NkGSousBloc(ar, 0);
	EXPECT_TRUE(app != nullptr);
	if (!app) {
		return;
	}

	// 1. LES VIRGULES SEPARENT DES MEMBRES : trois proprietes, pas une.
	const NkArchive *shadow = NkGSousBloc(*app, 0);
	EXPECT_TRUE(shadow != nullptr);
	if (shadow) {
		NkGCounts c;
		NkGCount(*shadow, c);
		EXPECT_TRUE(c.props == 3 && c.raws == 0);
		EXPECT_STREQ(NkString(shadow->Lexeme(NkStringView("offset"))), NkString("(0, 2)"));
		EXPECT_STREQ(NkString(shadow->Lexeme(NkStringView("blur"))), NkString("6"));
		EXPECT_STREQ(NkString(shadow->Lexeme(NkStringView("color"))), NkString("#0000003A"));
	}

	// 2. UNE ESPACE AUSSI : `Ident =` ouvre le membre suivant.
	const NkArchive *space = NkGSousBloc(*app, 1);
	EXPECT_TRUE(space != nullptr);
	if (space) {
		NkGCounts c;
		NkGCount(*space, c);
		EXPECT_TRUE(c.props == 2 && c.raws == 0);
		EXPECT_STREQ(NkString(space->Lexeme(NkStringView("a"))), NkString("1"));
		EXPECT_STREQ(NkString(space->Lexeme(NkStringView("b"))), NkString("2"));
	}

	// 3. CONTROLE NEGATIF, sans lequel les deux precedents pourraient venir d'un
	//    decoupage trop zele : `key 0.0 -> 1.0` n'est PAS `cle = valeur`, il
	//    reste UNE tranche brute entiere.
	const NkArchive *track = NkGSousBloc(*app, 2);
	EXPECT_TRUE(track != nullptr);
	if (track) {
		NkGCounts c;
		NkGCount(*track, c);
		EXPECT_TRUE(c.props == 0 && c.raws == 1);
		const NkArchiveNode *b = track->FindNode(NkStringView(NkGuiArchive::KeyBody()));
		if (b && b->IsArray() && !b->array.Empty()) {
			EXPECT_STREQ(NkString(b->array[0].Lexeme()), NkString("key 0.0 -> 1.0"));
		}
	}
}

// =============================================================================
// L15 -- OUVRIR ET FERMER UNE LIGNE SONT LA MEME QUESTION
// =============================================================================
// ATTENTION -- DEUX AUTRES MUTATIONS SURVIVANTES, ET UNE D'ELLES A FAIT RETIRER
// DU CODE. M20 (l'intervalle sans saut de ligne est jete) et M21 (le pied de
// bloc accepte n'importe quel intervalle) restaient vertes : aucun des deux
// corpus n'ecrit un membre sur la ligne de son `{`, ni son `}` sur la ligne du
// dernier membre. Les deux mecanismes existaient donc sans etre mesures -- et en
// les mesurant, le second s'est revele FAUX : la garde `HasNewLineIn` PERDAIT
// l'accolade fermante posee en fin de ligne au lieu de la rendre.
//
// Elle a ete remplacee par la regle qui servait deja a OUVRIR un membre. Une
// regle de moins, et un cas de plus qui marche.
static void L15_MemeLigne() {
	printf("[L15] Un membre sur la ligne du `{`, et un `}` sur la ligne du dernier membre\n");

	const char *src =
		"nkgui 0.3\n"
		"VBox \"v\" { a = 1\n"
		"  b = 2 }\n";

	NkString out;
	NkGuiDiag err;
	EXPECT_TRUE(NkGRoundTrip(src, out, err));
	EXPECT_STREQ(out, NkString(src));

	// Le temoin : ce sont bien DEUX proprietes d'un bloc a plusieurs lignes, pas
	// un bloc sur une ligne (sinon le controle ne mesurerait pas ce qu'il dit).
	NkArchive ar;
	EXPECT_TRUE(NkGuiArchive::Read(src, (nk_uint32)NkString(src).Size(), ar, err));
	const NkArchive *v = NkGSousBloc(ar, 0);
	EXPECT_TRUE(v != nullptr);
	if (v) {
		NkGCounts c;
		NkGCount(*v, c);
		EXPECT_TRUE(c.props == 2 && c.raws == 0);
		EXPECT_TRUE(!v->Has(NkStringView(NkGuiArchive::KeyLayout())));
	}
}

/// La transformation de la fixture de L16 : `label` devient `text`, partout, en
/// descendant. Elle passe par l'ARCHIVE et rien d'autre -- c'est tout l'interet
/// d'avoir une couche qui rend une archive et pas un modele.
static void NkGRenommeRec(NkArchive &ar) {
	NkArchiveNode *v = ar.FindNode(NkStringView("label"));
	if (v) {
		const NkArchiveNode copie = *v;
		const nk_int32 rang = ar.GetSourceOrder(NkStringView("label"));
		const NkString lead(ar.FindNode(NkStringView("label"))->LeadingTrivia());
		ar.Remove(NkStringView("label"));
		ar.SetNode(NkStringView("text"), copie);
		ar.SetSourceOrder(NkStringView("text"), rang);
		ar.SetLeadingTrivia(NkStringView("text"), NkStringView(lead));
	}
	NkArchiveNode *body = ar.FindNode(NkStringView(NkGuiArchive::KeyBody()));
	if (!body || !body->IsArray()) {
		return;
	}
	for (nk_size i = 0; i < body->array.Size(); ++i) {
		if (body->array[i].IsObject() && body->array[i].object) {
			NkGRenommeRec(*body->array[i].object);
		}
	}
}

static nk_bool NkGRenommeLabel(NkArchive &ar, NkSchemaVersion, NkSchemaVersion) noexcept {
	NkGRenommeRec(ar);
	return true;
}

// =============================================================================
// L16 -- LES MIGRATIONS  (etape 5)
// =============================================================================
// `NkSchemaRegistry` migre des `NkArchive`, et cette couche PRODUIT des
// `NkArchive` : le branchement ne demande aucun adaptateur. Trois choses ont
// pourtant du etre reglees, et aucune n'etait devinable sans essayer.
//
//  1. `MigrateArchive` ecrit la version atteinte dans `__meta__.schema_version`.
//     C'est une convention de format BINAIRE. Laissee en place dans un document
//     texte, elle SORT DANS LE FICHIER : `__meta__ = { schema_version = 0.3.0 }`,
//     une propriete que personne n'a ecrite, au milieu du document de
//     l'utilisateur. La version d'un `.nkgui` a un seul domicile : `$version`.
//
//  2. Le registre n'avait pas de `GetCurrentVersion`. Un format TEXTE doit
//     reestampiller son fichier apres migration, donc savoir vers quelle version
//     il vient d'etre amene -- sans quoi il recopie la constante chez lui et les
//     deux derivent. La methode a ete ajoutee a `NkSchemaVersioning.h`.
//
//  3. Il FAUT une migration 0.2 -> 0.3 meme si elle ne transforme rien : sans
//     elle le registre repond « No migration path » et REFUSE les dix fichiers
//     du corpus, qui sont tous en 0.2.
static void L16_Migrations() {
	printf("[L16] Les migrations de schema, branchees sur un document `.nkgui`\n");

	const char *src =
		"nkgui 0.2\n"
		"widgets {\n"
		"  Text \"t\" {\n"
		"    label = \"bonjour\"\n"
		"  }\n"
		"}\n";
	const nk_uint32 n = (nk_uint32)NkString(src).Size();

	// -- 1. LA REGLE (a) TIENT TOUJOURS : ecrire ne reestampille PAS -----------
	NkArchive brut;
	NkGuiDiag e0;
	EXPECT_TRUE(NkGuiArchive::Read(src, n, brut, e0));
	NkGuiStyle st;
	st.crlf = false;
	EXPECT_STREQ(NkGuiArchive::Write(brut, st), NkString(src));
	EXPECT_TRUE(NkGuiArchive::VersionOf(brut) == NkSchemaVersion(0, 2, 0));

	// -- 2. MIGRER est une DECISION, et elle se voit dans le fichier -----------
	NkArchive doc;
	NkGuiDiag err;
	EXPECT_TRUE(NkGuiArchive::Read(src, n, doc, err));
	EXPECT_TRUE(NkGuiArchive::Migrate(doc, err));
	EXPECT_TRUE(NkGuiArchive::VersionOf(doc) == NkSchemaVersion(0, 3, 0));

	// Le document migre est le MEME, a la ligne de version pres. La v0.3 n'a
	// rien retire a la v0.2 : la migration ne transforme donc rien, et c'est ce
	// controle qui l'etablit au lieu de le promettre.
	const NkString attendu(
		"nkgui 0.3\n"
		"widgets {\n"
		"  Text \"t\" {\n"
		"    label = \"bonjour\"\n"
		"  }\n"
		"}\n");
	EXPECT_STREQ(NkGuiArchive::Write(doc, st), attendu);

	// -- 3. `__meta__` NE SORT PAS DANS LE FICHIER -----------------------------
	// ⚠️ Et le temoin qui empeche ce controle d'etre vert pour rien : `__meta__`
	//    A BIEN ETE ECRIT par le registre. On le verifie en migrant une archive
	//    NUE (pas un document) -- si le registre ne l'ecrivait pas, le point
	//    ci-dessus ne prouverait rien du tout.
	EXPECT_TRUE(!doc.Has(NkStringView("__meta__")));
	{
		NkArchive temoin;
		NkGuiArchive::RegisterFormat();
		NkString motif;
		EXPECT_TRUE(NkSchemaRegistry::MigrateArchive(NkGuiArchive::DocumentType(), temoin,
													 NkSchemaVersion(0, 2, 0), &motif));
		EXPECT_TRUE(temoin.Has(NkStringView("__meta__")));
	}

	// -- 4. UNE VERSION SANS CHEMIN EST REFUSEE, ET LE DOCUMENT N'EST PAS TOUCHE
	// La documentation de `MigrateArchive` promet un « rollback implicite ». On
	// le MESURE : le document doit se reecrire exactement comme avant l'echec.
	NkArchive orphelin;
	NkGuiDiag e2;
	const char *vieux = "nkgui 0.1\nwidgets {\n  Text \"t\" { }\n}\n";
	EXPECT_TRUE(NkGuiArchive::Read(vieux, (nk_uint32)NkString(vieux).Size(), orphelin, e2));
	NkGuiDiag e3;
	EXPECT_TRUE(!NkGuiArchive::Migrate(orphelin, e3));
	EXPECT_STREQ(e3.code, NkString("E-MIGRATION"));
	EXPECT_STREQ(NkGuiArchive::Write(orphelin, st), NkString(vieux));

	// -- 5. UNE MIGRATION QUI TRANSFORME VRAIMENT -----------------------------
	// ⚠️ FIXTURE DE BANC, PAS UNE MIGRATION LIVREE. Les trois points precedents
	//    ne montrent qu'une migration VIDE -- ils ne prouvent donc pas que le
	//    mecanisme SAIT transformer. Il n'existe aucune transformation reelle a
	//    ce jour (la v0.3 est purement additive), alors on en fabrique une : une
	//    0.3 -> 0.4 hypothetique qui renomme `label` en `text`, le temps de ce
	//    controle. Dire « ca marchera » sans l'avoir fait tourner une fois, c'est
	//    exactement ce que le registre a fait pendant deux mois.
	NkSchemaRegistry::RegisterMigration(NkGuiArchive::DocumentType(), NkSchemaVersion(0, 3, 0),
										NkSchemaVersion(0, 4, 0), NkGRenommeLabel);
	NkSchemaRegistry::SetCurrentVersion(NkGuiArchive::DocumentType(), NkSchemaVersion(0, 4, 0));

	NkArchive futur;
	NkGuiDiag e4;
	EXPECT_TRUE(NkGuiArchive::Read(src, n, futur, e4));
	EXPECT_TRUE(NkGuiArchive::Migrate(futur, e4));
	const NkString attendu4(
		"nkgui 0.4\n"
		"widgets {\n"
		"  Text \"t\" {\n"
		"    text = \"bonjour\"\n"
		"  }\n"
		"}\n");
	EXPECT_STREQ(NkGuiArchive::Write(futur, st), attendu4);

	// On remet le registre dans l'etat que le reste du programme attend.
	NkSchemaRegistry::SetCurrentVersion(NkGuiArchive::DocumentType(), NkSchemaVersion(0, 3, 0));
}

// =============================================================================
// L17 -- CLASSER UNE VALEUR, ET LES QUATRE REFUS QUI ONT CHANGE DE DOMICILE
// =============================================================================
// ATTENTION -- LA FRONTIERE A BOUGE, ELLE N'A PAS DISPARU, ET C'EST LE POINT LE
// PLUS FACILE A PERDRE DE TOUTE LA BASCULE.
//
// L'ancien lecteur (`NkGuiFormat.h`) REFUSAIT neuf documents fautifs. Le nouveau
// n'en refuse que cinq -- ceux qu'il ne sait pas REPRESENTER (echappement
// inconnu, accolade jamais fermee, en-tete absent, commentaire de bloc ouvert,
// propriete sans valeur). Les quatre autres (`#12345`, une virgule finale dans
// une liste, une cle de dictionnaire qui n'en est pas une, une section inconnue)
// sont des JETONS NUS parfaitement lisibles pour une couche purement syntaxique.
//
// Ils redeviennent des fautes ICI, au moment de JUGER. Ce n'est pas un repli :
// c'est ce que le format demandait deja, mot pour mot -- « lire n'est pas juger,
// et un document invalide doit rester LISIBLE ». Un editeur qui refuse d'ouvrir
// le fichier dont il signale la faute rend cette faute incorrigible.
//
// >>> Si ce controle disparait, quatre diagnostics disparaissent avec lui SANS
//     QUE RIEN NE TOMBE : le fichier se lit, s'ecrit a l'octet, et personne ne
//     dit que la couleur a cinq chiffres.
static NkGuiValueKind NkGKindDe(const char *valeur) {
	NkString src("nkgui 0.3\nT \"t\" {\n  p = ");
	src.Append(valeur);
	src.Append("\n}\n");
	NkArchive ar;
	NkGuiDiag err;
	if (!NkGuiArchive::Read(src.Data(), (nk_uint32)src.Size(), ar, err)) {
		return NkGuiValueKind::Invalid;
	}
	const NkArchive *b = NkGSousBloc(ar, 0);
	if (!b) {
		return NkGuiValueKind::Invalid;
	}
	const NkArchiveNode *n = b->FindNode(NkStringView("p"));
	if (!n) {
		return NkGuiValueKind::Invalid;
	}
	return NkGuiArchive::KindOf(*n);
}

static void L17_ClasserUneValeur() {
	printf("[L17] Classer une valeur, et les quatre refus qui ont change de domicile\n");

	// -- 1. LES FORMES BIEN FORMEES -------------------------------------------
	EXPECT_TRUE(NkGKindDe("\"bonjour\"") == NkGuiValueKind::String);
	EXPECT_TRUE(NkGKindDe("\"\"") == NkGuiValueKind::String);
	EXPECT_TRUE(NkGKindDe("12") == NkGuiValueKind::Number);
	EXPECT_TRUE(NkGKindDe("0.50") == NkGuiValueKind::Number);
	EXPECT_TRUE(NkGKindDe("-3") == NkGuiValueKind::Number);
	EXPECT_TRUE(NkGKindDe("#F79A28") == NkGuiValueKind::Color);
	EXPECT_TRUE(NkGKindDe("#0000003A") == NkGuiValueKind::Color);
	EXPECT_TRUE(NkGKindDe("(0, 2)") == NkGuiValueKind::Vec2);
	EXPECT_TRUE(NkGKindDe("(-1.0, 2)") == NkGuiValueKind::Vec2);
	EXPECT_TRUE(NkGKindDe("true") == NkGuiValueKind::Ident);
	EXPECT_TRUE(NkGKindDe("false") == NkGuiValueKind::Ident);
	EXPECT_TRUE(NkGKindDe("Enum.X") == NkGuiValueKind::Ident);
	EXPECT_TRUE(NkGKindDe("Resizable | Closable") == NkGuiValueKind::Flags);
	EXPECT_TRUE(NkGKindDe("[]") == NkGuiValueKind::List);
	EXPECT_TRUE(NkGKindDe("[1, \"a\", #FF0000]") == NkGuiValueKind::List);
	EXPECT_TRUE(NkGKindDe("{ }") == NkGuiValueKind::Dict);
	EXPECT_TRUE(NkGKindDe("{ a = 1, \"b\" = [2] }") == NkGuiValueKind::Dict);

	// -- 2. LES QUATRE REFUS QUI ONT CHANGE DE DOMICILE ------------------------
	EXPECT_TRUE(NkGKindDe("#12345") == NkGuiValueKind::Invalid);		 // 5 chiffres
	EXPECT_TRUE(NkGKindDe("#F79A288F2") == NkGuiValueKind::Invalid);	 // 9 chiffres
	EXPECT_TRUE(NkGKindDe("[\"a\",]") == NkGuiValueKind::Invalid);		 // virgule finale
	EXPECT_TRUE(NkGKindDe("{ 1 = 2 }") == NkGuiValueKind::Invalid);		 // cle qui n'en est pas une

	// -- 3. ET LE TEMOIN QUI EMPECHE `Invalid` D'ETRE UNE REPONSE PARESSEUSE ---
	// Chacun des quatre a un JUMEAU legal qui ne differe que par la faute. Sans
	// lui, un classificateur qui rendrait Invalid un peu trop souvent passerait.
	EXPECT_TRUE(NkGKindDe("#123456") == NkGuiValueKind::Color);
	EXPECT_TRUE(NkGKindDe("#F79A288F") == NkGuiValueKind::Color);
	EXPECT_TRUE(NkGKindDe("[\"a\"]") == NkGuiValueKind::List);
	EXPECT_TRUE(NkGKindDe("{ a = 2 }") == NkGuiValueKind::Dict);

	// -- 4. TOUT LE LEXEME DOIT ETRE CONSOMME ---------------------------------
	// `#12345 zut` n'est pas une couleur suivie de bruit : c'est une faute.
	EXPECT_TRUE(NkGKindDe("#123456 zut") == NkGuiValueKind::Invalid);
	EXPECT_TRUE(NkGKindDe("(1, 2) (3, 4)") == NkGuiValueKind::Invalid);

	// -- 5. LE FICHIER RESTE LISIBLE ET REENREGISTRABLE MALGRE LA FAUTE -------
	// C'est la moitie que le classificateur ne dit pas tout seul, et c'est la
	// raison d'etre du deplacement : signaler sans empecher de corriger.
	const char *fautif = "nkgui 0.3\nT \"t\" {\n  c = #12345\n}\n";
	NkString out;
	NkGuiDiag err;
	EXPECT_TRUE(NkGRoundTrip(fautif, out, err));
	EXPECT_STREQ(out, NkString(fautif));

	// -- 6. UNE VALEUR FABRIQUEE PAR LE CODE ----------------------------------
	// Une chaine posee par `SetString` n'a pas de litteral : elle s'ecrira entre
	// guillemets, donc c'est une chaine -- quel que soit son contenu.
	{
		NkArchive a;
		a.SetString(NkStringView("p"), NkStringView("#12345"));
		EXPECT_TRUE(NkGuiArchive::KindOf(*a.FindNode(NkStringView("p")))
					== NkGuiValueKind::String);
		NkGuiArchive::SetToken(a, NkStringView("q"), NkStringView("#12345"));
		EXPECT_TRUE(NkGuiArchive::KindOf(*a.FindNode(NkStringView("q")))
					== NkGuiValueKind::Invalid);
		a.SetInt64(NkStringView("r"), 7);
		EXPECT_TRUE(NkGuiArchive::KindOf(*a.FindNode(NkStringView("r")))
					== NkGuiValueKind::Number);
	}
}

// =============================================================================
// L18 -- COMPARER DEUX DOCUMENTS  (la seconde mesure du banc `NKUIDesign`)
// =============================================================================
// ATTENTION -- CE CONTROLE EXISTE PARCE QUE `Equal` N'ETAIT MESURE NULLE PART.
//    Il a ete ecrit pour le banc de `NKUIDesign`, qui vit dans un autre binaire
//    et n'est pas rejoue par le harnais de mutation. Un `Equal` qui rendrait
//    toujours `true` y aurait rendu la moitie des controles verts sans que rien
//    ne tombe ici. Une fonction de la couche se mesure dans le banc de la
//    couche, sinon elle ne se mesure pas.
static void L18_Comparer() {
	printf("[L18] Comparer deux documents : ce que `Equal` doit VOIR\n");

	const char *ref = "nkgui 0.3\nVBox \"v\" {\n  a = 1\n  b = 2\n  Text \"t\" { }\n}\n";

	// Un helper local : lire deux sources et les comparer.
	struct Cmp {
			static bool Egal(const char *x, const char *y, bool trivia) {
				NkArchive a;
				NkArchive b;
				NkGuiDiag e;
				if (!NkGuiArchive::Read(x, (nk_uint32)NkString(x).Size(), a, e)) {
					return false;
				}
				if (!NkGuiArchive::Read(y, (nk_uint32)NkString(y).Size(), b, e)) {
					return false;
				}
				return NkGuiArchive::Equal(a, b, trivia);
			}
	};

	// 1. TEMOIN DE BRUIT : la meme source deux fois -> egales. Sans lui, tous les
	//    controles ci-dessous seraient verts avec un `Equal` toujours faux.
	EXPECT_TRUE(Cmp::Egal(ref, ref, true));

	// 2. LES DIFFERENCES QUE `Equal` DOIT VOIR. Chacune est un controle POSITIF :
	//    un `Equal` toujours vrai meurt sur la premiere.
	EXPECT_TRUE(!Cmp::Egal(ref, "nkgui 0.3\nVBox \"v\" {\n  a = 9\n  b = 2\n  Text \"t\" { }\n}\n",
						   true));	// une VALEUR
	EXPECT_TRUE(!Cmp::Egal(ref, "nkgui 0.3\nVBox \"w\" {\n  a = 1\n  b = 2\n  Text \"t\" { }\n}\n",
						   true));	// un IDENTIFIANT
	EXPECT_TRUE(!Cmp::Egal(ref, "nkgui 0.3\nHBox \"v\" {\n  a = 1\n  b = 2\n  Text \"t\" { }\n}\n",
						   true));	// un TYPE
	EXPECT_TRUE(!Cmp::Egal(ref, "nkgui 0.3\nVBox \"v\" {\n  b = 2\n  a = 1\n  Text \"t\" { }\n}\n",
						   true));	// l'ORDRE des membres
	EXPECT_TRUE(!Cmp::Egal(ref, "nkgui 0.3\nVBox \"v\" {\n  a = 1\n  b = 2\n}\n",
						   true));	// un ENFANT en moins
	EXPECT_TRUE(!Cmp::Egal(ref, "nkgui 0.3\nVBox \"v\" {\n  a = 1\n  b = 2\n  c = 3\n"
								"  Text \"t\" { }\n}\n",
						   true));	// une PROPRIETE en plus

	// 3. LE LEXEME, PAS LA VALEUR. `0.20` et `0.2` denotent le meme nombre et NE
	//    SONT PAS le meme document : reecrire l'un a la place de l'autre modifie
	//    une ligne que l'auteur n'a pas touchee.
	EXPECT_TRUE(!Cmp::Egal("nkgui 0.3\nT \"t\" {\n  x = 0.20\n}\n",
						   "nkgui 0.3\nT \"t\" {\n  x = 0.2\n}\n", true));

	// 4. LA TRIVIA ENTRE DANS L'EGALITE quand on la demande, et pas sinon. Les
	//    deux moities comptent : sans la seconde, `withTrivia` ne servirait a rien.
	const char *avecCom = "nkgui 0.3\n// un commentaire\nVBox \"v\" {\n  a = 1\n  b = 2\n"
						  "  Text \"t\" { }\n}\n";
	EXPECT_TRUE(!Cmp::Egal(ref, avecCom, true));
	EXPECT_TRUE(Cmp::Egal(ref, avecCom, false));

	// 5. UNE LIGNE VIDE EN MOINS EST UNE DIFFERENCE, elle aussi.
	const char *avecVide = "nkgui 0.3\nVBox \"v\" {\n  a = 1\n\n  b = 2\n  Text \"t\" { }\n}\n";
	EXPECT_TRUE(!Cmp::Egal(ref, avecVide, true));
	EXPECT_TRUE(Cmp::Egal(ref, avecVide, false));

	// 6. ATTENTION -- DEUX ARCHIVES SANS RANG, ET C'EST UNE MUTATION SURVIVANTE
	//    QUI A IMPOSE CE POINT. La mutation « Equal ignore l'ORDRE des entrees »
	//    restait VERTE : tous les documents des points 1 a 5 viennent d'un
	//    FICHIER, donc chaque entree porte un rang, et c'est la comparaison des
	//    RANGS qui voyait la difference d'ordre. La comparaison des CLES ne
	//    servait a rien -- sur des documents lus.
	//
	//    Elle sert des qu'un document est FABRIQUE PAR LE CODE : aucune entree
	//    n'a de rang, et deux archives dont les memes valeurs sont rangees sous
	//    des cles echangees seraient declarees egales. Ce sont pourtant deux
	//    documents differents : ils ne s'ecrivent pas pareil.
	{
		NkArchive x;
		x.SetInt64(NkStringView("a"), 1);
		x.SetInt64(NkStringView("b"), 1);
		NkArchive y;
		y.SetInt64(NkStringView("b"), 1);
		y.SetInt64(NkStringView("a"), 1);
		// CONDITION D'EXISTENCE : sans rang, sinon ce controle mesurerait encore
		// les rangs et pas les cles.
		EXPECT_TRUE(x.GetSourceOrder(NkStringView("a")) < 0);
		EXPECT_TRUE(!NkGuiArchive::Equal(x, y, true));
		// Le temoin : la MEME archive reste egale a elle-meme.
		NkArchive z = x;
		EXPECT_TRUE(NkGuiArchive::Equal(x, z, true));
	}
}

// =============================================================================
// L19 -- LA LIGNE DU FICHIER, PORTEE JUSQU'AU NOEUD
// =============================================================================
// ATTENTION -- CE CONTROLE EXISTE PARCE QUE J'AVAIS LIVRE LE CHEMIN SEUL.
//    La validation rendait `widgets / Button "x" . color` sans numero de ligne,
//    et je l'avais presente comme « un echange : on perd le saut dans l'editeur,
//    on gagne une designation stable ». C'en etait un, et il n'avait pas lieu
//    d'etre : **l'information n'etait pas perdue, elle n'etait pas transportee.**
//    Le lecteur connait la ligne au moment ou il analyse.
//
//    Le chemin dit QUOI, la ligne dit OU ALLER. Ce ne sont pas deux reponses a
//    la meme question, donc l'une ne remplace pas l'autre.
//
// >>> ET IL VERIFIE LA VALEUR, PAS SA PRESENCE. Un controle qui demanderait
//     seulement « la ligne est-elle non nulle » serait vert avec un compteur qui
//     rend toujours 1. Les lignes attendues sont ECRITES A LA MAIN, et le
//     point 3 les fait BOUGER pour qu'un numero constant ne puisse pas passer.
static void L19_LigneDuFichier() {
	printf("[L19] La ligne du fichier voyage jusqu'au noeud\n");

	//  1: nkgui 0.3
	//  2: widgets {
	//  3:   VBox "v" {
	//  4:     a = 1
	//  5:
	//  6:     // un commentaire
	//  7:     b = 2
	//  8:     Text "t" { }
	//  9:   }
	// 10: }
	const char *src =
		"nkgui 0.3\n"
		"widgets {\n"
		"  VBox \"v\" {\n"
		"    a = 1\n"
		"\n"
		"    // un commentaire\n"
		"    b = 2\n"
		"    Text \"t\" { }\n"
		"  }\n"
		"}\n";

	NkArchive ar;
	NkGuiDiag err;
	EXPECT_TRUE(NkGuiArchive::Read(src, (nk_uint32)NkString(src).Size(), ar, err));

	const NkArchive *widgets = NkGSousBloc(ar, 0);
	EXPECT_TRUE(widgets != nullptr);
	if (!widgets) {
		return;
	}
	const NkArchive *v = NkGSousBloc(*widgets, 0);
	EXPECT_TRUE(v != nullptr);
	if (!v) {
		return;
	}

	// 1. LES PROPRIETES. `b` est en ligne 7 : ni la ligne vide ni le commentaire
	//    ne comptent pour lui, ils appartiennent a sa trivia de tete.
	EXPECT_TRUE(v->GetSourceLine(NkStringView("a")) == 4);
	EXPECT_TRUE(v->GetSourceLine(NkStringView("b")) == 7);

	// 2. LES BLOCS. La ligne d'un bloc vit sur le NOEUD qui le porte, pas dans
	//    l'archive du bloc : un bloc est un element du `$body` de son parent.
	const NkArchiveNode *wb = widgets->FindNode(NkStringView(NkGuiArchive::KeyBody()));
	EXPECT_TRUE(wb && wb->IsArray() && !wb->array.Empty());
	if (wb && wb->IsArray() && !wb->array.Empty()) {
		EXPECT_TRUE(wb->array[0].SourceLine() == 3);  // VBox "v"
	}
	const NkArchiveNode *vb = v->FindNode(NkStringView(NkGuiArchive::KeyBody()));
	EXPECT_TRUE(vb && vb->IsArray() && !vb->array.Empty());
	if (vb && vb->IsArray() && !vb->array.Empty()) {
		EXPECT_TRUE(vb->array[0].SourceLine() == 8);  // Text "t"
	}

	// 3. CONTROLE POSITIF : on DECALE la source de trois lignes, et TOUT doit
	//    bouger de trois. Sans ce point, un compteur bloque sur une constante
	//    passerait les deux precedents.
	{
		NkString decale("\n\n\n");
		decale.Append(src);
		NkArchive d2;
		NkGuiDiag e2;
		EXPECT_TRUE(NkGuiArchive::Read(decale.Data(), (nk_uint32)decale.Size(), d2, e2));
		const NkArchive *w2 = NkGSousBloc(d2, 0);
		EXPECT_TRUE(w2 != nullptr);
		if (w2) {
			const NkArchive *v2 = NkGSousBloc(*w2, 0);
			EXPECT_TRUE(v2 != nullptr);
			if (v2) {
				EXPECT_TRUE(v2->GetSourceLine(NkStringView("a")) == 7);
				EXPECT_TRUE(v2->GetSourceLine(NkStringView("b")) == 10);
			}
		}
	}

	// 4. UNE TRANCHE BRUTE porte sa ligne, elle aussi -- c'est le seul endroit ou
	//    un diagnostic pourra pointer une construction non modelisee.
	{
		const char *b = "nkgui 0.3\nbehavior \"b\" {\n  set x = 1\n}\n";
		NkArchive ab;
		NkGuiDiag eb;
		EXPECT_TRUE(NkGuiArchive::Read(b, (nk_uint32)NkString(b).Size(), ab, eb));
		const NkArchive *bl = NkGSousBloc(ab, 0);
		EXPECT_TRUE(bl != nullptr);
		if (bl) {
			const NkArchiveNode *bb = bl->FindNode(NkStringView(NkGuiArchive::KeyBody()));
			EXPECT_TRUE(bb && bb->IsArray() && !bb->array.Empty());
			if (bb && bb->IsArray() && !bb->array.Empty()) {
				EXPECT_TRUE(bb->array[0].IsScalar() && bb->array[0].SourceLine() == 3);
			}
		}
	}

	// 5. CONTROLE NEGATIF : un document FABRIQUE PAR LE CODE n'a aucune ligne, et
	//    n'alloue donc rien pour en porter une. `-1`, pas `0` : « inconnue » et
	//    « premiere ligne » ne doivent pas se confondre.
	{
		NkArchive code;
		code.SetInt64(NkStringView("a"), 1);
		EXPECT_TRUE(code.GetSourceLine(NkStringView("a")) == -1);
		EXPECT_TRUE(!code.FindNode(NkStringView("a"))->HasTrivia());
	}

	// 6. LA LIGNE SURVIT A LA COPIE. `NkUIDocument` est copiee par valeur ; un
	//    diagnostic produit sur une copie doit pointer le meme endroit.
	{
		NkArchive copie = ar;
		const NkArchive *w3 = NkGSousBloc(copie, 0);
		EXPECT_TRUE(w3 != nullptr);
		if (w3) {
			const NkArchive *v3 = NkGSousBloc(*w3, 0);
			EXPECT_TRUE(v3 != nullptr && v3->GetSourceLine(NkStringView("b")) == 7);
		}
	}

	// 7. ET ELLE NE SORT PAS DANS LE FICHIER. C'est de la position de source, pas
	//    du contenu : l'aller-retour doit rester octet pour octet.
	NkString out;
	NkGuiDiag e3;
	EXPECT_TRUE(NkGRoundTrip(src, out, e3));
	EXPECT_STREQ(out, NkString(src));

	// 8. ATTENTION -- LA LIGNE SUIT `AdoptFormatting`, ET C'EST UNE MUTATION
	//    SURVIVANTE QUI A TRANCHE. La mutation « la ligne ne survit pas a
	//    AdoptFormatting » restait VERTE : rien ne le mesurait.
	//
	//    Deux issues etaient possibles -- retirer la propagation comme code mort,
	//    ou la mesurer. C'est la seconde, et pour une raison de forme :
	//    `AdoptFormatting` existe pour greffer sur une archive RECONSTRUITE tout ce
	//    que le FICHIER disait. Le rang y va deja, la trivia et le litteral aussi.
	//    Une ligne est un fait du fichier au meme titre que son rang -- la laisser
	//    de cote ferait porter a cette operation trois faits de source sur quatre,
	//    et c'est exactement le genre d'exception qu'on decouvre six mois plus tard.
	{
		NkArchive source;
		source.SetInt64(NkStringView("a"), 1);
		source.SetSourceOrder(NkStringView("a"), 3);
		source.SetSourceLine(NkStringView("a"), 42);

		// La destination est NUE : reconstruite depuis un modele, elle ne sait rien
		// du fichier. C'est le cas d'usage exact d'AdoptFormatting.
		NkArchive dest;
		dest.SetInt64(NkStringView("a"), 1);
		dest.SetInt64(NkStringView("neuve"), 7);
		EXPECT_TRUE(dest.GetSourceLine(NkStringView("a")) == -1);

		dest.AdoptFormatting(source);
		EXPECT_TRUE(dest.GetSourceLine(NkStringView("a")) == 42);
		// LE TEMOIN : le rang passe aussi. Sans lui, ce controle serait vert avec
		// un AdoptFormatting qui ne ferait plus rien du tout.
		EXPECT_TRUE(dest.GetSourceOrder(NkStringView("a")) == 3);
		// CONTROLE NEGATIF : une cle absente de la source reste NUE -- on
		// n'invente pas une ligne pour une propriete que le fichier n'avait pas.
		EXPECT_TRUE(dest.GetSourceLine(NkStringView("neuve")) == -1);
	}
}

// =============================================================================
// POINT D'ENTREE DU BANC DE LA COUCHE
// =============================================================================
static void NkGuiLayerSuite(const char *corpusDir) {
	printf("\n--- LA COUCHE `.nkgui` <-> NkArchive (etape 4) ---\n");
	L1_Corpus(corpusDir);
	L2_Entrelacement();
	L3_Commentaires();
	L4_Inconnu();
	L5_CanariDollar();
	L6_Version();
	L7_BlocVide();
	L8_JetonNu();
	L9_Copie();
	L10_AjoutApresLecture();
	L11_ProprieteEnDouble();
	L12_LitteralDeChaine();
	L13_CorpusDesDocuments();
	L14_BlocSurUneLigne();
	L15_MemeLigne();
	L16_Migrations();
	L17_ClasserUneValeur();
	L18_Comparer();
	L19_LigneDuFichier();
}

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
