// =============================================================================
// Sandbox/System/NKArchive/src/main.cpp
// Banc EXECUTABLE de la mise a jour NkArchive : commentaires, ordre du fichier,
// forme litterale.
//
// POURQUOI CE FICHIER EXISTE
// --------------------------
// Meme raison que Sandbox/System/NKSerialization : la politique du workspace
// (`disableunittestexecution`) COMPILE les `tests/**.cpp` SANS JAMAIS LES
// EXECUTER. Un banc qui vit sous `tests/` ne prouve rien -- c'est exactement ce
// qui a permis a l'en-tete de NkReflectSerializer.h de mentir deux mois. Ce banc
// est donc une APPLICATION CONSOLE, convention documentee par NKGuiDrawTest.
//
// CE QU'IL PROUVE
// ---------------
// `NkArchive` ne portait rien de ce qu'un format texte editable a la main doit
// rendre a l'octet. Trois manques, pas un :
//
//   1. les commentaires et les lignes vides ;
//   2. L'ORDRE DU FICHIER -- l'archive est ordonnee par insertion, donc
//      deterministe, mais l'ordre obtenu est celui de la DECLARATION DU SCHEMA.
//      Des qu'un aller-retour passe par l'archive, les proprietes sont
//      reordonnees et « octet pour octet » tombe ;
//   3. la forme litterale des valeurs (`0.50` contre `0.5`, la casse d'une
//      couleur hexadecimale, les guillemets).
//
//   T0  L'ECART EXISTE VRAIMENT. Meme aller-retour avec un ecrivain qui ne sait
//       rien de la trivia : le resultat DIFFERE de l'entree, et on nomme les
//       trois causes une par une. C'est le temoin permanent du probleme -- si
//       T0 devenait vert, c'est que le banc ne mesure plus rien.
//   T1  LE CAS QUI TRANCHE. Le meme `.nkgui` -- proprietes dans un ordre
//       DIFFERENT de l'ordre de declaration du schema, commentaire en fin de
//       ligne, flottant ecrit `0.50` -- aller-retour complet en passant par le
//       modele reflechi : OCTET POUR OCTET.
//   T2  Ordre : une propriete ajoutee apres lecture n'a pas de rang, elle va a
//       la FIN, et l'ordre relatif des sans-rang est preserve (tri stable).
//   T3  Valeur EDITEE : le commentaire survit (il appartient a la ligne), le
//       litteral perime NE ressort PAS (il appartient a la valeur).
//   T4  LE PIEGE DE NkGValue::raw, ferme par construction : une valeur sans
//       forme litterale s'imprime CANONIQUEMENT, jamais vide -- pour les six
//       types, `null` compris.
//   T5  ADDITIF / JSON : la meme archive, avec et sans trivia, produit les
//       MEMES octets JSON. Un ecrivain qui ignore la trivia ne voit rien.
//   T6  ADDITIF / NKS1 : idem pour le binaire natif, relecture comprise.
//   T7  Cycle de vie : copie profonde (aucun partage de pointeur), move
//       (source desarmee), affectation.
//   T8  AdoptFormatting : greffe par cle, recursion objets, appariement par
//       indice dans les tableaux, et une cle absente de la source reste NUE.
//
// Sortie : un compte n/n, code de sortie 0 si tout passe.
//
// Zero-STL cote moteur (<cstdio> seulement pour l'affichage du banc).
//
// Auteur : Rihen
// License : Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================

#include <cstdio>

#include "NKReflection/NkRegistry.h"
#include "NKReflection/NkContainerTrait.h"
#include "NKSerialization/NkArchive.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/Native/NkNativeFormat.h"
#include "NKSerialization/Reflection/NkReflectSerializer.h"

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

using namespace nkentseu;
using namespace nkentseu::reflection;

// ---------------------------------------------------------------------------
// Compteurs et macros du banc
// ---------------------------------------------------------------------------
static int s_pass = 0;
static int s_fail = 0;

#define EXPECT_TRUE(expr)                                                                                              \
	do {                                                                                                               \
		if (!(expr)) {                                                                                                 \
			printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, #expr);                                                  \
			++s_fail;                                                                                                  \
		} else {                                                                                                       \
			++s_pass;                                                                                                  \
		}                                                                                                              \
	} while (0)

#define EXPECT_STREQ(a, b)                                                                                             \
	do {                                                                                                               \
		const NkString &va_ = (a);                                                                                     \
		const NkString &vb_ = (b);                                                                                     \
		if (!(va_ == vb_)) {                                                                                           \
			printf("  FAIL [%s:%d] %s != %s\n", __FILE__, __LINE__, #a, #b);                                           \
			printf("    obtenu  : <<%s>>\n", va_.CStr());                                                              \
			printf("    attendu : <<%s>>\n", vb_.CStr());                                                              \
			++s_fail;                                                                                                  \
		} else {                                                                                                       \
			++s_pass;                                                                                                  \
		}                                                                                                              \
	} while (0)

// =============================================================================
// LE SCHEMA -- un panneau, cinq proprietes, DANS CET ORDRE
// =============================================================================
// L'ordre de declaration ci-dessous est title, width, height, opacity, accent.
// Le fichier temoin, lui, ecrit height, opacity, title, accent, width. C'est
// tout le probleme : l'archive rend l'ordre du SCHEMA, le fichier veut le sien.

struct Panel {
		NKENTSEU_REFLECT_CLASS(Panel)
	public:
		NKENTSEU_PROPERTY(NkString, title)
	public:
		NKENTSEU_PROPERTY(nk_int32, width)
	public:
		NKENTSEU_PROPERTY(nk_int32, height)
	public:
		NKENTSEU_PROPERTY(nk_float32, opacity)
	public:
		NKENTSEU_PROPERTY(NkString, accent)
	public:
};

static void WireClasses() {
	(void)Panel::GetStaticClass();
}

// =============================================================================
// LE FICHIER TEMOIN
// =============================================================================
// Ecrit a la main, comme un humain l'ecrirait. Trois pieges dedans :
//  - l'ordre n'est PAS celui du schema ;
//  - il y a des commentaires, dont un en FIN DE LIGNE, et une ligne vide ;
//  - `opacity` vaut `0.50` (deux decimales) et `accent` porte une casse HAUTE.
// La forme canonique de 0.50 est "0.5" : un ecrivain canonique reecrirait donc
// une ligne que personne n'a touchee.

static const char *const kWitness = "# Panneau d inspection -- temoin du banc NkArchive\n"
									"# Les proprietes ne sont PAS dans l ordre du schema.\n"
									"\n"
									"height = 240          # hauteur utile, en points\n"
									"opacity = 0.50        # deux decimales, expres\n"
									"\n"
									"title = \"Inspecteur\"\n"
									"accent = \"#F79A28\"    # casse HAUTE : un ecrivain canonique la garderait\n"
									"width = 320\n"
									"# derniere ligne, apres la derniere propriete\n";

// =============================================================================
// UNE COUCHE DE SYNTAXE CONCRETE MINIMALE, POSEE SUR NkArchive
// =============================================================================
// Ce n'est PAS le format `.nkgui` (qui viendra a l'etape suivante) : c'est le
// plus petit lecteur/ecrivain qui exerce reellement les trois manques. La
// grammaire tient en une phrase : une ligne est soit du bruit (vide ou
// commentaire), soit `cle = valeur` suivie eventuellement d'un commentaire de
// fin de ligne.

// --- helpers de chaine ------------------------------------------------------

static bool IsSpaceChar(char c) noexcept {
	return c == ' ' || c == '\t';
}

static NkString Slice(const char *s, nk_size begin, nk_size end) noexcept {
	if (end <= begin) {
		return NkString();
	}
	return NkString(s + begin, end - begin);
}

// --- lecture ----------------------------------------------------------------
//
// Ce que le lecteur depose dans l'archive, en plus de la valeur :
//   - `leading`     : les lignes de bruit qui precedent la propriete, telles quelles
//   - `trailing`    : tout ce qui suit la valeur sur la meme ligne (espaces compris)
//   - `sourceOrder` : le rang de la propriete dans le fichier
//   - `literal`     : le lexeme brut, tel qu'ecrit
//   - en-tete / pied de l'archive : le bruit du debut et celui de la fin

static bool ReadWitness(const char *src, NkArchive &out) noexcept {
	out.Clear();

	NkString pending; // bruit accumule en attente d'une propriete
	nk_int32 rank = 0;
	nk_size i = 0;

	while (src[i] != '\0') {
		// Decoupe de la ligne courante, saut de ligne compris.
		nk_size lineBegin = i;
		while (src[i] != '\0' && src[i] != '\n') {
			++i;
		}
		nk_size lineEnd = i;			  // sans le '\n'
		if (src[i] == '\n') {
			++i;
		}
		nk_size fullEnd = i;			  // avec le '\n'

		// Une ligne vide ou commencant par '#' est du bruit.
		nk_size p = lineBegin;
		while (p < lineEnd && IsSpaceChar(src[p])) {
			++p;
		}
		if (p == lineEnd || src[p] == '#') {
			pending.Append(Slice(src, lineBegin, fullEnd));
			continue;
		}

		// `cle = valeur`
		nk_size keyBegin = p;
		while (p < lineEnd && !IsSpaceChar(src[p]) && src[p] != '=') {
			++p;
		}
		NkString key = Slice(src, keyBegin, p);
		while (p < lineEnd && IsSpaceChar(src[p])) {
			++p;
		}
		if (p >= lineEnd || src[p] != '=') {
			return false; // grammaire violee
		}
		++p;
		while (p < lineEnd && IsSpaceChar(src[p])) {
			++p;
		}

		// Le lexeme : jusqu'au premier espace hors guillemets, ou fin de ligne.
		nk_size valBegin = p;
		bool inQuotes = false;
		while (p < lineEnd) {
			if (src[p] == '"') {
				inQuotes = !inQuotes;
			} else if (!inQuotes && IsSpaceChar(src[p])) {
				break;
			}
			++p;
		}
		NkString literal = Slice(src, valBegin, p);
		NkString trailing = Slice(src, p, lineEnd); // espaces + commentaire, tels quels

		// Typage du lexeme : "..." = chaine, un point = flottant, sinon entier.
		if (literal.Length() >= 2 && literal[0] == '"') {
			NkString inner = Slice(literal.CStr(), 1, literal.Length() - 1);
			out.SetString(key.View(), inner.View());
		} else if (literal.Find(NkStringView(".")) != NkString::npos) {
			nk_float64 f = 0.0;
			literal.ToDouble(f);
			out.SetFloat32(key.View(), static_cast<nk_float32>(f));
		} else {
			nk_int64 n = 0;
			literal.ToInt64(n);
			out.SetInt32(key.View(), static_cast<nk_int32>(n));
		}

		// La mise en forme, deposee sur le noeud qu'on vient de poser.
		if (!pending.Empty()) {
			out.SetLeadingTrivia(key.View(), pending.View());
			pending.Clear();
		}
		if (!trailing.Empty()) {
			out.SetTrailingTrivia(key.View(), trailing.View());
		}
		out.SetSourceOrder(key.View(), rank);
		out.SetLiteral(key.View(), literal.View());
		++rank;
	}

	// Ce qui reste apres la derniere propriete est le pied du fichier.
	if (!pending.Empty()) {
		out.SetFooterTrivia(pending.View());
	}
	return true;
}

// --- ecriture ---------------------------------------------------------------
//
// `honorTrivia == false` reproduit exactement ce que ferait un ecrivain qui ne
// sait rien de la mise en forme -- c'est la branche que T0 mesure.

static NkString WriteWitness(const NkArchive &ar, bool honorTrivia) noexcept {
	NkString out;

	if (honorTrivia) {
		out.Append(ar.HeaderTrivia());
	}

	const NkVector<NkArchiveEntry> &entries = ar.Entries();
	for (nk_size i = 0; i < entries.Size(); ++i) {
		const NkArchiveEntry &e = entries[i];

		if (honorTrivia) {
			out.Append(e.node.LeadingTrivia());
		}

		out.Append(e.key);
		out.Append(" = ");

		// Une chaine se reecrit entre guillemets si elle n'a pas de litteral :
		// le litteral, lui, PORTE DEJA ses guillemets tels qu'ecrits.
		if (honorTrivia) {
			out.Append(e.node.Lexeme());
		} else if (e.node.value.type == NkArchiveValueType::NK_VALUE_STRING) {
			out.Append('"');
			out.Append(e.node.value.text);
			out.Append('"');
		} else {
			out.Append(e.node.CanonicalLexeme());
		}

		if (honorTrivia) {
			out.Append(e.node.TrailingTrivia());
		}
		out.Append('\n');
	}

	if (honorTrivia) {
		out.Append(ar.FooterTrivia());
	}
	return out;
}

// --- le chemin complet ------------------------------------------------------
//
// fichier -> archive LUE -> modele reflechi -> archive RECONSTRUITE (nue, dans
// l'ordre du schema) -> greffe de la mise en forme -> fichier.

static NkString RoundTrip(const char *src, bool honorTrivia, Panel *edited = nullptr) noexcept {
	NkArchive read;
	if (!ReadWitness(src, read)) {
		return NkString("<<lecture impossible>>");
	}

	Panel model;
	if (!NkReflectSerializer::DeserializeObject(model, read)) {
		return NkString("<<deserialisation impossible>>");
	}
	if (edited) {
		model = *edited; // point d'injection d'une edition, pour T3
	}

	NkArchive rebuilt;
	if (!NkReflectSerializer::SerializeObject(model, rebuilt)) {
		return NkString("<<serialisation impossible>>");
	}

	if (honorTrivia) {
		rebuilt.AdoptFormatting(read);
	}
	return WriteWitness(rebuilt, honorTrivia);
}

// =============================================================================
// T0 -- L'ECART EXISTE VRAIMENT
// =============================================================================
// Temoin permanent : sans la mise a jour de NkArchive, l'aller-retour ABIME le
// fichier. On ne se contente pas de constater une difference, on nomme les
// trois causes -- sinon un jour la difference viendrait d'autre chose et
// personne ne le verrait.
static void T0_EcartSansTrivia() {
	printf("[T0] L'ecart existe : aller-retour SANS trivia -> le fichier est abime\n");

	const NkString witness(kWitness);
	const NkString plain = RoundTrip(kWitness, /*honorTrivia=*/false);

	EXPECT_TRUE(!(plain == witness)); // c'est bien le probleme qu'on corrige

	// Cause 1 : les commentaires et la ligne vide ont disparu.
	EXPECT_TRUE(plain.Find(NkStringView("# hauteur utile")) == NkString::npos);
	EXPECT_TRUE(plain.Find(NkStringView("# Panneau d inspection")) == NkString::npos);

	// Cause 2 : l'ordre est celui du SCHEMA (title en premier), pas du fichier
	// (height en premier).
	EXPECT_TRUE(plain.Find(NkStringView("title")) < plain.Find(NkStringView("height")));
	EXPECT_TRUE(witness.Find(NkStringView("height")) < witness.Find(NkStringView("title")));

	// Cause 3 : `0.50` est reecrit canoniquement `0.5`.
	EXPECT_TRUE(plain.Find(NkStringView("opacity = 0.5\n")) != NkString::npos);
	EXPECT_TRUE(plain.Find(NkStringView("0.50")) == NkString::npos);
}

// =============================================================================
// T1 -- LE CAS QUI TRANCHE : OCTET POUR OCTET
// =============================================================================
static void T1_OctetPourOctet() {
	printf("[T1] Aller-retour AVEC trivia -> OCTET POUR OCTET\n");

	const NkString witness(kWitness);
	const NkString back = RoundTrip(kWitness, /*honorTrivia=*/true);

	EXPECT_STREQ(back, witness);
	EXPECT_TRUE(back.Length() == witness.Length());
}

// =============================================================================
// T2 -- ORDRE : une propriete neuve va a la FIN, et le tri est stable
// =============================================================================
static void T2_OrdreEtProprieteNeuve() {
	printf("[T2] Ordre du fichier, et propriete neuve rangee a la fin\n");

	NkArchive read;
	EXPECT_TRUE(ReadWitness(kWitness, read));

	// Rangs tels que le fichier les a donnes.
	EXPECT_TRUE(read.GetSourceOrder(NkStringView("height")) == 0);
	EXPECT_TRUE(read.GetSourceOrder(NkStringView("opacity")) == 1);
	EXPECT_TRUE(read.GetSourceOrder(NkStringView("title")) == 2);
	EXPECT_TRUE(read.GetSourceOrder(NkStringView("accent")) == 3);
	EXPECT_TRUE(read.GetSourceOrder(NkStringView("width")) == 4);

	// Une archive dans l'ordre du schema, PLUS deux proprietes que le fichier
	// n'a jamais connues.
	NkArchive rebuilt;
	Panel model;
	EXPECT_TRUE(NkReflectSerializer::DeserializeObject(model, read));
	EXPECT_TRUE(NkReflectSerializer::SerializeObject(model, rebuilt));
	rebuilt.SetInt32(NkStringView("zIndex"), 7);   // neuve, inseree en premier
	rebuilt.SetInt32(NkStringView("marginTop"), 4); // neuve, inseree en second

	rebuilt.AdoptFormatting(read);

	// RELATION, pas compte fige : « les cles du fichier, plus les deux neuves ».
	// Le jour ou le temoin gagne une propriete, un `== 7` tomberait et serait
	// « repare » en 8 sans etre relu -- alors que la propriete testee, elle, n'a
	// pas bouge. L'ancrage de non-vacuite garde la relation d'etre 0 == 0.
	const nk_size kNeuves = 2;
	const NkVector<NkArchiveEntry> &e = rebuilt.Entries();
	EXPECT_TRUE(!read.Empty());
	EXPECT_TRUE(e.Size() == read.Size() + kNeuves);
	if (!read.Empty() && e.Size() == read.Size() + kNeuves) {
		// Les cinq du fichier, dans l'ordre du fichier.
		EXPECT_TRUE(e[0].key == NkString("height"));
		EXPECT_TRUE(e[1].key == NkString("opacity"));
		EXPECT_TRUE(e[2].key == NkString("title"));
		EXPECT_TRUE(e[3].key == NkString("accent"));
		EXPECT_TRUE(e[4].key == NkString("width"));
		// Les deux neuves a la fin, DANS LEUR ORDRE D'INSERTION (tri stable).
		EXPECT_TRUE(e[5].key == NkString("zIndex"));
		EXPECT_TRUE(e[6].key == NkString("marginTop"));
	}

	// Et une cle absente de la source reste NUE : ni commentaire, ni rang.
	EXPECT_TRUE(rebuilt.GetSourceOrder(NkStringView("zIndex")) == -1);
	const NkArchiveNode *zn = rebuilt.FindNode(NkStringView("zIndex"));
	EXPECT_TRUE(zn != nullptr && !zn->HasTrivia());
}

// =============================================================================
// T3 -- VALEUR EDITEE : le commentaire reste, le litteral perime ne revient pas
// =============================================================================
// Le commentaire appartient a la LIGNE ; la forme litterale appartient a la
// VALEUR. Reimprimer `0.50` sur une opacite devenue 0.75 ne serait pas
// « preserver la mise en forme », ce serait PERDRE la modification.
static void T3_ValeurEditee() {
	printf("[T3] Valeur editee : le commentaire survit, le litteral perime non\n");

	NkArchive read;
	EXPECT_TRUE(ReadWitness(kWitness, read));

	Panel model;
	EXPECT_TRUE(NkReflectSerializer::DeserializeObject(model, read));
	model.opacity = 0.75f; // l'utilisateur a bouge le curseur

	const NkString back = RoundTrip(kWitness, /*honorTrivia=*/true, &model);

	// La nouvelle valeur est ecrite, canoniquement.
	EXPECT_TRUE(back.Find(NkStringView("opacity = 0.75")) != NkString::npos);
	// L'ancien lexeme n'est PAS ressorti.
	EXPECT_TRUE(back.Find(NkStringView("0.50")) == NkString::npos);
	// Le commentaire de fin de ligne, lui, est toujours la.
	EXPECT_TRUE(back.Find(NkStringView("# deux decimales, expres")) != NkString::npos);
	// Et tout le reste du fichier est intact, y compris l'ordre.
	EXPECT_TRUE(back.Find(NkStringView("height = 240          # hauteur utile, en points")) != NkString::npos);
	EXPECT_TRUE(back.Find(NkStringView("accent = \"#F79A28\"")) != NkString::npos);

	// Le meme fait, vu au niveau du noeud : le litteral est encore stocke mais
	// il n'est plus UTILISABLE, et c'est ce que Lexeme() consulte.
	NkArchiveNode *n = read.FindNode(NkStringView("opacity"));
	EXPECT_TRUE(n != nullptr);
	if (n) {
		EXPECT_TRUE(n->HasUsableLiteral());
		EXPECT_TRUE(NkString(n->Lexeme()) == NkString("0.50"));
		n->value = NkArchiveValue::FromFloat32(0.75f);
		EXPECT_TRUE(!n->HasUsableLiteral()); // desarme tout seul
		EXPECT_TRUE(NkString(n->Lexeme()) == NkString("0.75"));
	}
}

// =============================================================================
// T4 -- LE PIEGE DE NkGValue::raw, FERME PAR CONSTRUCTION
// =============================================================================
// L'ecrivain `.nkgui` reemet `NkGValue::raw` verbatim : un document construit en
// memoire a un `raw` vide, il s'ecrit donc avec des valeurs VIDES, sans la
// moindre erreur, et se relit sans broncher. Ici, une valeur sans forme
// litterale doit s'imprimer CANONIQUEMENT -- jamais vide.
static void T4_JamaisVide() {
	printf("[T4] Une valeur sans litteral s'imprime canoniquement, jamais vide\n");

	NkArchive ar; // fabriquee par le CODE : aucune trivia nulle part
	ar.SetNull(NkStringView("rien"));
	ar.SetBool(NkStringView("actif"), false);
	ar.SetInt32(NkStringView("compte"), 0);
	ar.SetUInt64(NkStringView("masque"), 0u);
	ar.SetFloat32(NkStringView("ratio"), 0.5f);
	ar.SetString(NkStringView("nom"), NkStringView("Inspecteur"));
	ar.SetString(NkStringView("vide"), NkStringView(""));

	// Aucun noeud ne porte de trivia : c'est le cas normal et il est valide.
	const NkVector<NkArchiveEntry> &e = ar.Entries();
	for (nk_size i = 0; i < e.Size(); ++i) {
		EXPECT_TRUE(!e[i].node.HasTrivia());
	}

	// Et pourtant aucun Lexeme() ne rend du vide -- sauf la chaine reellement
	// vide, pour laquelle vide EST la bonne reponse.
	EXPECT_TRUE(NkString(ar.Lexeme(NkStringView("rien"))) == NkString("null"));
	EXPECT_TRUE(NkString(ar.Lexeme(NkStringView("actif"))) == NkString("false"));
	EXPECT_TRUE(NkString(ar.Lexeme(NkStringView("compte"))) == NkString("0"));
	EXPECT_TRUE(NkString(ar.Lexeme(NkStringView("masque"))) == NkString("0"));
	EXPECT_TRUE(NkString(ar.Lexeme(NkStringView("ratio"))) == NkString("0.5"));
	EXPECT_TRUE(NkString(ar.Lexeme(NkStringView("nom"))) == NkString("Inspecteur"));
	EXPECT_TRUE(NkString(ar.Lexeme(NkStringView("vide"))).Empty());

	// Les faux et les zeros, precisement ceux qu'une « optimisation » omettrait,
	// s'impriment bien.
	EXPECT_TRUE(!NkString(ar.Lexeme(NkStringView("actif"))).Empty());
	EXPECT_TRUE(!NkString(ar.Lexeme(NkStringView("compte"))).Empty());
	EXPECT_TRUE(!NkString(ar.Lexeme(NkStringView("rien"))).Empty());

	// Et un litteral pose puis vide ne peut pas faire retomber dans le vide.
	NkArchiveNode *n = ar.FindNode(NkStringView("ratio"));
	EXPECT_TRUE(n != nullptr);
	if (n) {
		n->SetLiteral(NkStringView(""));
		EXPECT_TRUE(!n->HasUsableLiteral());
		EXPECT_TRUE(NkString(n->Lexeme()) == NkString("0.5"));
	}
}

// =============================================================================
// T5 -- ADDITIF / JSON : la trivia est INVISIBLE a qui ne la demande pas
// =============================================================================
static void T5_AdditifJSON() {
	printf("[T5] Additif : le JSON produit est identique avec et sans trivia\n");

	NkArchive nue;
	nue.SetInt32(NkStringView("height"), 240);
	nue.SetFloat32(NkStringView("opacity"), 0.5f);
	nue.SetString(NkStringView("title"), NkStringView("Inspecteur"));
	NkArchive sub;
	sub.SetInt32(NkStringView("x"), 1);
	nue.SetObject(NkStringView("origine"), sub);
	NkVector<NkArchiveValue> arr;
	arr.PushBack(NkArchiveValue::FromInt32(3));
	arr.PushBack(NkArchiveValue::FromInt32(4));
	nue.SetArray(NkStringView("marges"), arr);

	NkArchive ornee(nue); // meme contenu...
	ornee.SetHeaderTrivia(NkStringView("# en-tete\n"));
	ornee.SetFooterTrivia(NkStringView("# pied\n"));
	ornee.SetLeadingTrivia(NkStringView("opacity"), NkStringView("\n# un commentaire\n"));
	ornee.SetTrailingTrivia(NkStringView("height"), NkStringView("   # en fin de ligne"));
	ornee.SetLiteral(NkStringView("opacity"), NkStringView("0.50"));
	ornee.SetSourceOrder(NkStringView("title"), 0);

	const NkString jNue = NkJSONWriter::WriteArchive(nue, true, 2);
	const NkString jOrnee = NkJSONWriter::WriteArchive(ornee, true, 2);

	// ⚠️ ANCRAGE OBLIGATOIRE, ET C'EST LE POINT DELICAT DE CE CONTROLE.
	//
	// La comparaison qui suit oppose deux sorties de LA MEME fonction. Si
	// NkJSONWriter::WriteArchive rendait du vide, les deux seraient vides,
	// l'egalite serait vraie et les deux `Find(...) == npos` seraient vrais
	// aussi : T5 passerait au vert sur un ecrivain JSON entierement casse.
	// Le controle et ce qu'il controle partageraient une cause.
	//
	// On ancre donc UN cote sur du texte ECRIT A LA MAIN, qui ne peut pas venir
	// du code teste. Sans ces trois lignes, l'egalite ne prouve rien.
	EXPECT_TRUE(!jNue.Empty());
	EXPECT_TRUE(jNue.Find(NkStringView("\"height\": 240")) != NkString::npos);
	EXPECT_TRUE(jNue.Find(NkStringView("\"title\": \"Inspecteur\"")) != NkString::npos);

	EXPECT_STREQ(jOrnee, jNue);
	EXPECT_TRUE(jOrnee.Find(NkStringView("0.50")) == NkString::npos);
	EXPECT_TRUE(jOrnee.Find(NkStringView("commentaire")) == NkString::npos);
	// L'ordre n'a pas bouge non plus : SetSourceOrder ne trie pas toute seule.
	EXPECT_TRUE(ornee.Entries()[0].key == NkString("height"));
}

// =============================================================================
// T6 -- ADDITIF / NKS1 : idem pour le binaire natif
// =============================================================================
static void T6_AdditifNKS1() {
	printf("[T6] Additif : le binaire NKS1 est identique avec et sans trivia\n");

	NkArchive nue;
	nue.SetInt32(NkStringView("height"), 240);
	nue.SetFloat32(NkStringView("opacity"), 0.5f);
	nue.SetString(NkStringView("title"), NkStringView("Inspecteur"));

	NkArchive ornee(nue);
	ornee.SetHeaderTrivia(NkStringView("# en-tete\n"));
	ornee.SetTrailingTrivia(NkStringView("height"), NkStringView("  # utile"));
	ornee.SetLiteral(NkStringView("opacity"), NkStringView("0.50"));
	ornee.SetSourceOrder(NkStringView("opacity"), 0);

	NkVector<nk_uint8> binNue;
	NkVector<nk_uint8> binOrnee;
	EXPECT_TRUE(native::NkNativeWriter::WriteArchive(nue, binNue));
	EXPECT_TRUE(native::NkNativeWriter::WriteArchive(ornee, binOrnee));

	// Meme ancrage qu'en T5 : deux sorties de la meme fonction ne prouvent rien
	// tant qu'on n'a pas verifie qu'elles ne sont pas vides. Ici la relecture
	// plus bas (h == 240) sert de second ancrage independant.
	EXPECT_TRUE(binNue.Size() > 0);

	EXPECT_TRUE(binNue.Size() == binOrnee.Size());
	bool same = (binNue.Size() == binOrnee.Size());
	if (same) {
		for (nk_size i = 0; i < binNue.Size(); ++i) {
			if (binNue[i] != binOrnee[i]) {
				same = false;
				break;
			}
		}
	}
	EXPECT_TRUE(same); // octet pour octet : NKS1 ne voit pas la trivia

	// Et la relecture rend une archive nue, sans inventer de mise en forme.
	NkArchive relu;
	EXPECT_TRUE(native::NkNativeReader::ReadArchive(binOrnee.Data(), binOrnee.Size(), relu));
	EXPECT_TRUE(!nue.Empty());
	EXPECT_TRUE(relu.Size() == nue.Size());
	EXPECT_TRUE(!relu.HasTrivia());
	for (nk_size i = 0; i < relu.Entries().Size(); ++i) {
		EXPECT_TRUE(!relu.Entries()[i].node.HasTrivia());
	}
	nk_int32 h = 0;
	EXPECT_TRUE(relu.GetInt32(NkStringView("height"), h) && h == 240);
}

// =============================================================================
// T7 -- CYCLE DE VIE : copie profonde, move, affectation
// =============================================================================
// Le bloc de trivia est un pointeur POSSEDANT. C'est exactement le piege qui a
// oblige NkArchive a cesser d'etre `= default` : un pointeur copie, c'est deux
// proprietaires et une double liberation.
static void T7_CycleDeVie() {
	printf("[T7] Cycle de vie : copie profonde, move, affectation\n");

	NkArchive a;
	a.SetInt32(NkStringView("k"), 1);
	a.SetTrailingTrivia(NkStringView("k"), NkStringView("  # note"));
	a.SetHeaderTrivia(NkStringView("# tete\n"));

	// Copie : le contenu suit, le POINTEUR non.
	NkArchive b(a);
	EXPECT_TRUE(NkString(b.HeaderTrivia()) == NkString("# tete\n"));
	const NkArchiveNode *na = a.FindNode(NkStringView("k"));
	const NkArchiveNode *nb = b.FindNode(NkStringView("k"));
	EXPECT_TRUE(na && nb && na->Trivia() != nb->Trivia());
	EXPECT_TRUE(a.Trivia() != b.Trivia());

	// Modifier la copie ne remonte pas dans l'original.
	b.SetTrailingTrivia(NkStringView("k"), NkStringView("  # autre"));
	EXPECT_TRUE(NkString(a.FindNode(NkStringView("k"))->TrailingTrivia()) == NkString("  # note"));

	// Affectation par copie : idem, et l'ancien bloc de la cible est libere.
	NkArchive c;
	c.SetInt32(NkStringView("z"), 9);
	c.SetHeaderTrivia(NkStringView("# a jeter\n"));
	c = a;
	EXPECT_TRUE(NkString(c.HeaderTrivia()) == NkString("# tete\n"));
	EXPECT_TRUE(c.Trivia() != a.Trivia());

	// Move : la source est desarmee, elle ne possede plus rien.
	NkArchive d(traits::NkMove(c));
	EXPECT_TRUE(NkString(d.HeaderTrivia()) == NkString("# tete\n"));
	EXPECT_TRUE(!c.HasTrivia());

	// Move d'un noeud : meme regle.
	NkArchiveNode n1(NkArchiveValue::FromInt32(5));
	n1.SetTrailingTrivia(NkStringView("  # x"));
	NkArchiveNode n2(traits::NkMove(n1));
	EXPECT_TRUE(n2.HasTrivia());
	EXPECT_TRUE(!n1.HasTrivia());

	// Une reaffectation de valeur par cle conserve le commentaire de la ligne
	// (il appartient a la ligne) mais desarme le litteral (il appartient a la
	// valeur).
	NkArchive e;
	e.SetFloat32(NkStringView("opacity"), 0.5f);
	e.SetTrailingTrivia(NkStringView("opacity"), NkStringView("  # curseur"));
	e.SetLiteral(NkStringView("opacity"), NkStringView("0.50"));
	EXPECT_TRUE(NkString(e.Lexeme(NkStringView("opacity"))) == NkString("0.50"));
	e.SetFloat32(NkStringView("opacity"), 0.75f);
	EXPECT_TRUE(NkString(e.FindNode(NkStringView("opacity"))->TrailingTrivia()) == NkString("  # curseur"));
	EXPECT_TRUE(NkString(e.Lexeme(NkStringView("opacity"))) == NkString("0.75"));
}

// =============================================================================
// T8 -- AdoptFormatting : objets imbriques et tableaux
// =============================================================================
static void T8_GreffeRecursive() {
	printf("[T8] AdoptFormatting : recursion dans les objets et les tableaux\n");

	// La source : ce qu'un fichier aurait donne.
	NkArchive srcSub;
	srcSub.SetInt32(NkStringView("x"), 1);
	srcSub.SetInt32(NkStringView("y"), 2);
	srcSub.SetSourceOrder(NkStringView("y"), 0);
	srcSub.SetSourceOrder(NkStringView("x"), 1);
	srcSub.SetTrailingTrivia(NkStringView("y"), NkStringView("  # ordonnee"));

	NkArchive src;
	src.SetObject(NkStringView("origine"), srcSub);
	NkVector<NkArchiveValue> vals;
	vals.PushBack(NkArchiveValue::FromFloat32(0.5f));
	vals.PushBack(NkArchiveValue::FromFloat32(0.25f));
	src.SetArray(NkStringView("marges"), vals);
	{
		NkArchiveNode *m = src.FindNode(NkStringView("marges"));
		EXPECT_TRUE(m != nullptr);
		if (m) {
			m->array[0].SetLiteral(NkStringView("0.50"));
			m->array[1].SetLiteral(NkStringView(".25"));
		}
	}

	// La cible : la meme chose, reconstruite depuis un modele, dans un autre
	// ordre et sans la moindre mise en forme.
	NkArchive dstSub;
	dstSub.SetInt32(NkStringView("x"), 1);
	dstSub.SetInt32(NkStringView("y"), 2);
	NkArchive dst;
	dst.SetObject(NkStringView("origine"), dstSub);
	dst.SetArray(NkStringView("marges"), vals);

	dst.AdoptFormatting(src);

	// Objet imbrique : ordre du fichier et commentaire greffes.
	const NkArchiveNode *o = dst.FindNode(NkStringView("origine"));
	EXPECT_TRUE(o != nullptr && o->IsObject());
	if (o && o->IsObject()) {
		EXPECT_TRUE(o->object->Entries()[0].key == NkString("y"));
		EXPECT_TRUE(o->object->Entries()[1].key == NkString("x"));
		EXPECT_TRUE(NkString(o->object->FindNode(NkStringView("y"))->TrailingTrivia()) == NkString("  # ordonnee"));
	}

	// Tableau : appariement par INDICE, litteraux repris.
	const NkArchiveNode *m = dst.FindNode(NkStringView("marges"));
	EXPECT_TRUE(!vals.Empty());
	EXPECT_TRUE(m != nullptr && m->IsArray() && m->array.Size() == vals.Size());
	if (!vals.Empty() && m && m->IsArray() && m->array.Size() == vals.Size()) {
		EXPECT_TRUE(NkString(m->array[0].Lexeme()) == NkString("0.50"));
		EXPECT_TRUE(NkString(m->array[1].Lexeme()) == NkString(".25"));
	}

	// Un tableau n'est PAS trie : son ordre est intrinseque.
	NkArchive dst2;
	dst2.SetArray(NkStringView("marges"), vals);
	dst2.AdoptFormatting(src);
	const NkArchiveNode *m2 = dst2.FindNode(NkStringView("marges"));
	EXPECT_TRUE(m2 && m2->array.Size() == vals.Size());
	if (!vals.Empty() && m2 && m2->array.Size() == vals.Size()) {
		EXPECT_TRUE(m2->array[0].value.text == NkString("0.5"));
		EXPECT_TRUE(m2->array[1].value.text == NkString("0.25"));
	}

	// Une valeur de tableau MODIFIEE ne recupere pas le litteral perime.
	NkVector<NkArchiveValue> autres;
	autres.PushBack(NkArchiveValue::FromFloat32(0.75f));
	autres.PushBack(NkArchiveValue::FromFloat32(0.25f));
	NkArchive dst3;
	dst3.SetArray(NkStringView("marges"), autres);
	dst3.AdoptFormatting(src);
	const NkArchiveNode *m3 = dst3.FindNode(NkStringView("marges"));
	EXPECT_TRUE(m3 && m3->array.Size() == autres.Size());
	if (!autres.Empty() && m3 && m3->array.Size() == autres.Size()) {
		EXPECT_TRUE(NkString(m3->array[0].Lexeme()) == NkString("0.75")); // pas "0.50"
		EXPECT_TRUE(NkString(m3->array[1].Lexeme()) == NkString(".25"));  // inchangee
	}
}


// --- outillage de T10 -------------------------------------------------------
//
// Une cle RESERVEE porte l'identite syntaxique du noeud, pas une propriete du
// modele. Le prefixe `$` est illegal dans un identifiant `.nkgui` : voir T10.
static bool EstCleReservee(const NkString &k) noexcept {
	const char *c = k.Data();
	return c && *c == '$';
}

static NkStringView ArcString(const NkArchive &ar, const char *key) noexcept {
	const NkArchiveNode *n = const_cast<NkArchive &>(ar).FindNode(NkStringView(key));
	if (!n || n->value.type != NkArchiveValueType::NK_VALUE_STRING) {
		return NkStringView("");
	}
	return NkStringView(n->value.text.Data());
}

static bool Contient(const NkString &h, const char *needle) noexcept {
	const char *s = h.Data();
	if (!s || !needle || !*needle) {
		return false;
	}
	for (; *s; ++s) {
		const char *a = s;
		const char *b = needle;
		while (*a && *b && *a == *b) {
			++a;
			++b;
		}
		if (!*b) {
			return true;
		}
	}
	return false;
}

// =============================================================================
// T9 -- ORDRE ENTRELACE PROPRIETES / ENFANTS (prerequis de l'etape 4)
// =============================================================================
// La vraie syntaxe `.nkgui` n'est pas plate. Elle imbrique, et surtout elle
// ENTRELACE les proprietes et les enfants :
//
//     widgets {
//       VBox "v" {
//         a = 1              <-- propriete
//         Text "t" { }       <-- ENFANT, entre deux proprietes
//         b = 2              <-- propriete
//       }
//     }
//
// Le banc `.nkgui` v0.3 traite deja l'ordre inverse comme une DIFFERENCE
// (controle 2c : `VBox "v" { a = 1 / Text "t" { } }` contre
// `VBox "v" { Text "t" { } / a = 1 }`). L'entrelacement compte donc vraiment.
//
// ⚠️ LE RISQUE, et c'est pour ca que ce controle vient AVANT d'ecrire la couche :
//    dans une archive, les proprietes sont des ENTREES de l'objet et les enfants
//    vivent naturellement dans UNE entree de type tableau. Tous les enfants sont
//    alors groupes a une seule position -- et `b = 2` ne peut plus revenir APRES
//    l'enfant. `sourceOrder` sur les entrees ne suffirait pas.
//
// Ce que ce controle etablit : la trivia ayant ete posee sur le NOEUD (donc aussi
// sur chaque element de tableau) et non sur l'entree, un rang global suffit a
// reconstituer l'entrelacement. `NkArchive` n'a PAS besoin d'etre retouchee pour
// l'etape 4 -- c'est le travail de l'ecrivain, et il a ce qu'il lui faut.
static void T9_OrdreEntrelace() {
	printf("[T9] Ordre entrelace proprietes / enfants -- prerequis de l'etape 4\n");

	NkArchive v;
	v.SetInt32(NkStringView("a"), 1);
	v.SetInt32(NkStringView("b"), 2);

	NkVector<NkArchiveNode> kids;
	NkArchiveNode kid;
	NkArchive kidBody;
	kidBody.SetString(NkStringView("nom"), NkStringView("t"));
	kid.SetObject(kidBody);
	kids.PushBack(kid);
	v.SetNodeArray(NkStringView("children"), kids);

	// Le fichier disait : a (0), l'enfant (1), b (2).
	v.SetSourceOrder(NkStringView("a"), 0);
	v.SetSourceOrder(NkStringView("b"), 2);
	NkArchiveNode *ch = v.FindNode(NkStringView("children"));
	EXPECT_TRUE(ch != nullptr && ch->IsArray());
	if (ch && ch->IsArray()) {
		EXPECT_TRUE(!ch->array.Empty());
		if (!ch->array.Empty()) {
			ch->array[0].SetSourceOrder(1); // le rang vit sur l'ELEMENT
		}
	}

	// Un rang porte par un element de tableau se relit : c'est ce qui rend
	// l'entrelacement representable.
	if (ch && ch->IsArray() && !ch->array.Empty()) {
		EXPECT_TRUE(ch->array[0].SourceOrder() == 1);
	}
	EXPECT_TRUE(v.GetSourceOrder(NkStringView("a")) == 0);
	EXPECT_TRUE(v.GetSourceOrder(NkStringView("b")) == 2);

	// L'ecrivain fusionne les deux sources par rang. Ici, en miniature, la
	// boucle que la couche de syntaxe concrete devra ecrire.
	NkString out;
	const nk_int32 kEnfant = -2;
	for (nk_int32 rang = 0; rang < 3; ++rang) {
		bool ecrit = false;
		for (nk_size i = 0; i < v.Entries().Size() && !ecrit; ++i) {
			const NkArchiveEntry &e = v.Entries()[i];
			if (e.node.IsArray()) {
				continue; // le tableau n'est pas une propriete
			}
			if (e.node.SourceOrder() == rang) {
				out.Append(e.key);
				out.Append(" = ");
				out.Append(e.node.Lexeme());
				out.Append('\n');
				ecrit = true;
			}
		}
		if (ecrit || !ch || !ch->IsArray()) {
			continue;
		}
		for (nk_size k = 0; k < ch->array.Size(); ++k) {
			if (ch->array[k].SourceOrder() == rang) {
				out.Append("Text \"t\" { }");
				out.Append('\n');
				(void)kEnfant;
				break;
			}
		}
	}

	// L'attendu est ECRIT A LA MAIN : il ne vient pas du code teste.
	const NkString attendu("a = 1\nText \"t\" { }\nb = 2\n");
	EXPECT_STREQ(out, attendu);
}


// =============================================================================
// T10 -- L'IDENTITE D'UN NOEUD N'EST PAS UNE PROPRIETE (prerequis de l'etape 4)
// =============================================================================
// T9 a montre que l'ordre ENTRELACE proprietes/enfants est representable. Il
// reste la question que T9 ne pouvait pas voir, et elle decide de la couche :
//
//     VBox "v" { a = 1 }
//     ^^^^ ^^^
//
// Un noeud `.nkgui` s'ouvre sur DEUX jetons -- un type et un identifiant
// optionnel -- alors qu'une entree d'archive n'a qu'UNE cle. Si le type et
// l'identifiant deviennent des entrees ordinaires, plus rien ne les distingue
// d'une propriete : l'ecrivain rendrait
//
//     VBox "v" { $type = VBox / $id = v / a = 1 }
//
// c'est-a-dire le fichier ABIME, avec la meme cause que T0.
//
// ⚠️ LA CONCEPTION RETENUE, ET CE QUI LA REND SURE. Le type et l'identifiant
//    sont des entrees a CLE RESERVEE (`$type`, `$id`) que l'ecrivain consomme
//    pour composer l'en-tete du bloc au lieu de les emettre comme lignes.
//
//    Ca ne vaut que si une cle reservee ne peut JAMAIS entrer en collision avec
//    un vrai nom de propriete. Mesure : le lexeur `.nkgui` definit un
//    identifiant comme `[A-Za-z_][A-Za-z0-9_]*` (`NkGIsAlpha`,
//    `NkGuiFormat.h` l. 699) -- **`$` n'est pas un caractere d'identifiant**,
//    donc aucun fichier `.nkgui` valide ne peut nommer une propriete `$type`.
//
//    ⚠️ C'est une garantie EMPRUNTEE au lexeur, pas une garantie de l'archive.
//       Le jour ou quelqu'un ajoute `$` aux identifiants, cette couche casse en
//       silence. C'est pourquoi le canari vit LA-BAS, avec le lexeur, et pas
//       ici : `NKUIDesign --roundtrip-controles`, controle 21.
//
// ⚠️ POURQUOI PAS UN RANG NEGATIF plutot qu'une cle reservee. C'etait l'autre
//    candidat : marquer l'identite par `SetSourceOrder(-1)`. Il est ecarte pour
//    une raison mesurable -- `-1` est deja la valeur que porte toute entree
//    dont le rang n'a jamais ete pose (T2 : une propriete NEUVE va a la fin
//    justement parce qu'elle n'a pas de rang). Une propriete ajoutee par le
//    code serait donc prise pour l'identite du noeud. Deux sens pour une meme
//    valeur, c'est le motif qu'on retire partout ailleurs.
static void T10_IdentiteDuNoeud() {
	printf("[T10] L'identite d'un noeud n'est pas une propriete -- prerequis de l'etape 4\n");

	// Le noeud : VBox "v" { a = 1 / Text "t" { } / b = 2 }
	NkArchive v;
	v.SetString(NkStringView("$type"), NkStringView("VBox"));
	v.SetString(NkStringView("$id"), NkStringView("v"));
	v.SetInt32(NkStringView("a"), 1);
	v.SetInt32(NkStringView("b"), 2);

	NkVector<NkArchiveNode> kids;
	NkArchiveNode kid;
	NkArchive kidBody;
	kidBody.SetString(NkStringView("$type"), NkStringView("Text"));
	kidBody.SetString(NkStringView("$id"), NkStringView("t"));
	kidBody.SetSourceOrder(NkStringView("$type"), 0);
	kidBody.SetSourceOrder(NkStringView("$id"), 1);
	kid.SetObject(kidBody);
	kids.PushBack(kid);
	v.SetNodeArray(NkStringView("children"), kids);

	// ⚠️ LES CLES RESERVEES RECOIVENT UN RANG, ELLES AUSSI, ET C'EST LA CORRECTION
	//    D'UN CONTROLE QUI ETAIT VERT POUR RIEN. Premiere version : `$type` et
	//    `$id` n'avaient pas de rang. La boucle ne les atteignait donc jamais, et
	//    le corps etait propre SANS QUE LE TEST DE CLE RESERVEE SERVE A RIEN --
	//    mesure par la mutation H (`EstCleReservee` rendant toujours `false`) :
	//    121/121, VERTE.
	//
	//    C'est exactement le piege que le commentaire de ce controle decrivait
	//    deux paragraphes plus haut, et l'ecrire ne l'avait pas empeche. Un
	//    lecteur reel numerote ce qu'il lit ; l'identite d'un noeud a une place
	//    dans le fichier comme le reste. En lui donnant son rang, la SEULE chose
	//    qui la tient hors du corps redevient `EstCleReservee`, et la mutation H
	//    tue le controle.
	v.SetSourceOrder(NkStringView("$type"), 0);
	v.SetSourceOrder(NkStringView("$id"), 1);
	v.SetSourceOrder(NkStringView("a"), 2);
	v.SetSourceOrder(NkStringView("b"), 4);
	NkArchiveNode *ch = v.FindNode(NkStringView("children"));
	EXPECT_TRUE(ch != nullptr && ch->IsArray() && !ch->array.Empty());
	if (ch && ch->IsArray() && !ch->array.Empty()) {
		ch->array[0].SetSourceOrder(3);
	}

	// ── L'ECRIVAIN, en miniature : il CONSOMME les cles reservees ──────────
	// La boucle de T9, plus l'en-tete compose depuis $type / $id, et le saut
	// des cles reservees dans le corps.
	NkString out;
	out.Append(ArcString(v, "$type"));
	NkStringView id = ArcString(v, "$id");
	if (!id.Empty()) {
		out.Append(" ");
		out.Append('"');
		out.Append(id);
		out.Append('"');
	}
	out.Append(" {\n");

	for (nk_int32 rang = 0; rang < 5; ++rang) {
		bool ecrit = false;
		for (nk_size i = 0; i < v.Entries().Size() && !ecrit; ++i) {
			const NkArchiveEntry &e = v.Entries()[i];
			if (e.node.IsArray() || EstCleReservee(e.key)) {
				continue;
			}
			if (e.node.SourceOrder() == rang) {
				out.Append("  ");
				out.Append(e.key);
				out.Append(" = ");
				out.Append(e.node.Lexeme());
				out.Append('\n');
				ecrit = true;
			}
		}
		if (ecrit || !ch || !ch->IsArray()) {
			continue;
		}
		for (nk_size k = 0; k < ch->array.Size(); ++k) {
			if (ch->array[k].SourceOrder() != rang) {
				continue;
			}
			const NkArchive *corps = ch->array[k].object;
			EXPECT_TRUE(corps != nullptr);
			if (!corps) {
				break;
			}
			out.Append("  ");
			out.Append(ArcString(*corps, "$type"));
			NkStringView cid = ArcString(*corps, "$id");
			if (!cid.Empty()) {
				out.Append(" ");
				out.Append('"');
				out.Append(cid);
				out.Append('"');
			}
			out.Append(" { }\n");
			break;
		}
	}
	out.Append("}\n");

	// L'attendu est ECRIT A LA MAIN : il ne vient pas du code teste.
	const NkString attendu("VBox \"v\" {\n  a = 1\n  Text \"t\" { }\n  b = 2\n}\n");
	EXPECT_STREQ(out, attendu);

	// ⚠️ ET CE QUE L'EGALITE CI-DESSUS NE PEUT PAS VOIR -- avec la preuve que
	//    l'ecrire ne suffit pas. Elle serait tout aussi verte si l'ecrivain
	//    ignorait `$type`/`$id` pour une raison ACCIDENTELLE au lieu de les
	//    reconnaitre. C'est precisement ce qui est arrive : la premiere version
	//    de ce controle ne leur donnait pas de rang, et la mutation H
	//    (`EstCleReservee` toujours `false`) restait VERTE a 121/121.
	//
	//    Le paragraphe d'avertissement etait deja ecrit, mot pour mot, au-dessus
	//    du code fautif. **Nommer un piege ne le desamorce pas ; seule la
	//    mutation le fait.**
	//
	//    Les deux moities se mesurent donc separement, ET les cles portent un
	//    rang (voir plus haut) pour que le test de cle reservee soit la seule
	//    chose qui les retienne :
	//    (1) les cles reservees sont bien PRESENTES dans l'archive,
	//    (2) et le corps n'en emet aucune.
	EXPECT_TRUE(v.Has(NkStringView("$type")) && v.Has(NkStringView("$id")));
	EXPECT_TRUE(!Contient(out, "$type") && !Contient(out, "$id"));

	// Et l'identite se relit : un noeud sans identifiant n'est pas un noeud
	// sans type. `Text "t"` a les deux ; un cadre anonyme n'aurait que $type.
	NkArchive anonyme;
	anonyme.SetString(NkStringView("$type"), NkStringView("VBox"));
	EXPECT_TRUE(ArcString(anonyme, "$id").Empty());
	EXPECT_TRUE(!ArcString(anonyme, "$type").Empty());
}


// =============================================================================
// T11 -- LA SECTION INCONNUE, CONSERVEE TELLE QUELLE (prerequis de l'etape 4)
// =============================================================================
// Troisieme et dernier prerequis de representation, apres T9 (l'entrelacement)
// et T10 (l'identite). La regle de compatibilite ascendante de `.nkgui` v0.3 --
// celle que le controle 20b de `--roundtrip-controles` protege -- dit qu'une
// section introduite par une VERSION FUTURE doit etre relue et reemise **a
// l'octet pres**, commentaires interieurs compris, sans etre comprise.
//
// Ce n'est PAS de la trivia : la trivia entoure une valeur. Ici il n'y a pas de
// valeur -- c'est du contenu dont on ignore la structure.
//
// ⚠️ CE QUE LA MESURE A CHANGE PAR RAPPORT A CE QUE J'AVAIS PREVU. J'ai d'abord
//    ecrit ce controle en posant la tranche brute A LA FOIS comme valeur ET
//    comme litteral (`SetLiteral`), en pensant que c'etait le litteral qui la
//    rendait verbatim. **La mutation I -- `Lexeme()` ignorant le litteral et
//    rendant toujours la forme canonique -- tue 8 controles (T1, T3, T8...) mais
//    PAS celui-ci.**
//
//    Raison : la forme canonique d'un noeud CHAINE est la chaine elle-meme. Une
//    tranche brute n'a donc besoin d'AUCUN litteral, d'aucune trivia, d'aucun
//    mecanisme : elle se represente par un simple noeud chaine, et
//    `CanonicalLexeme()` la rend deja mot pour mot. Un mecanisme de moins a
//    ecrire dans la couche, et il a fallu une mutation pour s'en apercevoir.
static void T11_SectionInconnueVerbatim() {
	printf("[T11] Une section inconnue se conserve telle quelle -- prerequis de l'etape 4\n");

	// Une tranche EXACTE de fichier : plusieurs lignes, un commentaire dedans,
	// une tabulation, et des espaces multiples qu'aucun formateur ne produirait.
	const char *brut =
		"theme_futur {\n"
		"    # un commentaire que nous ne comprenons pas\n"
		"\tcouleur   =   #F79A28\n"
		"}";

	NkArchive ar;
	ar.SetString(NkStringView("$raw"), NkStringView(brut));
	NkArchiveNode *n = ar.FindNode(NkStringView("$raw"));
	EXPECT_TRUE(n != nullptr);
	if (!n) {
		return;
	}

	// 1. Le verbatim sort verbatim -- retours a la ligne, tabulation, espaces
	//    multiples et commentaire compris -- SANS QUE RIEN N'AIT ETE POSE.
	EXPECT_TRUE(!n->HasTrivia()); // aucune trivia : la representation est gratuite
	EXPECT_STREQ(NkString(n->Lexeme()), NkString(brut));
	EXPECT_STREQ(NkString(n->CanonicalLexeme()), NkString(brut));

	// 2. Elle survit a une COPIE PROFONDE : une section inconnue traverse les
	//    copies d'archive comme le reste.
	NkArchive copie = ar;
	NkArchiveNode *c = copie.FindNode(NkStringView("$raw"));
	EXPECT_TRUE(c != nullptr);
	if (c) {
		EXPECT_STREQ(NkString(c->Lexeme()), NkString(brut));
	}

	// 3. ⚠️ LA CONTRAINTE QUE CE CONTROLE POSE A LA COUCHE, et elle ne se corrige
	//    PAS dans `NkArchive`. Rien ne protege une tranche brute d'une
	//    modification : si la valeur est remplacee, la sortie change, et il
	//    n'existe aucune forme d'origine a laquelle revenir puisqu'une tranche
	//    brute n'a pas de forme canonique distincte de son texte.
	//
	//    >>> UNE SECTION INCONNUE NE DOIT JAMAIS PASSER PAR LE MODELE. <<<
	//
	//    Elle se lit dans l'archive et s'y reecrit sans que rien ne la touche.
	//    Le chemin `archive -> modele -> archive` n'a par construction rien a
	//    lui faire correspondre -- ce controle montre ce qu'il en couterait.
	NkArchive abime = ar;
	NkArchiveNode *a = abime.FindNode(NkStringView("$raw"));
	EXPECT_TRUE(a != nullptr);
	if (a) {
		a->value.text = NkString("autre chose");
		EXPECT_TRUE(NkString(a->Lexeme()) != NkString(brut));
	}

	// 4. La contre-epreuve du point 3, pour qu'il ne se lise pas comme une
	//    fatalite : l'original n'a pas bouge. La perte vient de la modification,
	//    pas de la representation.
	EXPECT_STREQ(NkString(n->Lexeme()), NkString(brut));

	// 5. La cle est reservee, donc l'ecrivain la reconnait comme il reconnait
	//    `$type` (T10) : une section brute ne s'emet pas en `cle = valeur`, elle
	//    s'emet telle quelle.
	EXPECT_TRUE(EstCleReservee(NkString("$raw")));

	// 6. ⚠️ ET LA MOITIE QUE LES CINQ PRECEDENTES NE VOIENT PAS : une tranche
	//    brute est du CONTENU, pas de la mise en forme. Elle doit donc rester
	//    visible pour qui ne demande pas la trivia -- au contraire de T5/T6, ou
	//    la trivia est invisible au JSON. Si elle etait rangee dans la trivia,
	//    elle disparaitrait du binaire natif et d'un export JSON sans que rien
	//    ne le signale.
	EXPECT_TRUE(ar.Has(NkStringView("$raw")));
	EXPECT_TRUE(n->kind == NkNodeKind::NK_NODE_SCALAR
				&& n->value.type == NkArchiveValueType::NK_VALUE_STRING);
}

// =============================================================================
// POINT D'ENTREE
// =============================================================================
int main() {
	// Sortie SANS TAMPON. Un banc qui peut tuer le processus (violation d'acces,
	// assertion, corruption de tas) perd tout son tampon au moment precis ou la
	// trace compte le plus -- on ne voit alors ni le dernier controle atteint ni
	// la ligne qui l'a tue. Mesure du 2026-08-22 : C5 plantait en 0xC0000005 et
	// n'imprimait RIEN, pas meme les controles deja passes.
	setvbuf(stdout, nullptr, _IONBF, 0);
	printf("=========================================================\n");
	printf(" SandboxNKArchive -- trivia, ordre du fichier, litteraux\n");
	printf("=========================================================\n\n");

	WireClasses();

	T0_EcartSansTrivia();
	T1_OctetPourOctet();
	T2_OrdreEtProprieteNeuve();
	T3_ValeurEditee();
	T4_JamaisVide();
	T5_AdditifJSON();
	T6_AdditifNKS1();
	T7_CycleDeVie();
	T8_GreffeRecursive();
	T9_OrdreEntrelace();
	T10_IdentiteDuNoeud();
	T11_SectionInconnueVerbatim();

	const int total = s_pass + s_fail;
	printf("\n---------------------------------------------------------\n");
	printf(" RESULTAT : %d / %d\n", s_pass, total);
	printf("---------------------------------------------------------\n");

	return (s_fail == 0) ? 0 : 1;
}

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================
