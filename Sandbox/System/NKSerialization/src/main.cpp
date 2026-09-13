// =============================================================================
// Sandbox/System/NKSerialization/src/main.cpp
// Banc EXECUTABLE du pont NKReflection <-> NKSerialization.
//
// POURQUOI CE FICHIER EXISTE
// --------------------------
// `Kernel/System/NKSerialization/tests/` contient trois bancs deja ecrits
// (test_reflect_phase3.cpp, test_reflect_objcontainer.cpp,
// test_reflect_phase5.cpp) declares en `test()` dans le .jenga. La politique du
// workspace (`disableunittestexecution`) les COMPILE SANS JAMAIS LES EXECUTER :
// `jenga test --project NKSerialization` repond noir sur blanc
// "Unit-test execution is disabled by workspace policy", et aucun .exe
// NKSerialization n'existe sous Build/. Le code de la Phase 3 (conteneurs)
// existait donc sans qu'aucune preuve n'ait jamais tourne.
//
// Ce banc est une APPLICATION CONSOLE -- meme raison et meme convention que
// Applications/NKGuiDrawTest, qui documente ce choix.
//
// CE QU'IL COUVRE
// ---------------
//  C1  conteneur de scalaires        NkVector<nk_int32>
//  C2  conteneur d'objets reflechis  NkVector<Point{x,y}>
//  C3  NOUVEAU -- conteneur d'objets dont les objets portent EUX-MEMES un
//      conteneur, sur trois niveaux. C'est la forme exacte de NkUIDocument
//      (NkVector<NkUINode> ou chaque NkUINode porte NkVector<int32> children),
//      et AUCUN banc existant ne la couvre : test_reflect_objcontainer.cpp
//      s'arrete a Polygon{ NkString; NkVector<Point{x,y}> }, dont les elements
//      n'ont pas de conteneur.
//  C4  NOUVEAU -- la provenance survit MEME QUAND ELLE VAUT LE DEFAUT.
//      Regle reprise de NkComponentInstance::Save : une etiquette absente se
//      relit comme "humain, non verifie", ce qui est le defaut -- donc
//      indiscernable d'une etiquette perdue.
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
static int s_debt = 0;

// Une DETTE CONNUE : un controle qui doit etre rouge aujourd'hui, et dont on
// sait pourquoi. Il s'imprime, il se compte a part, et il ne fait PAS echouer le
// banc -- sinon la baseline devient « 59/60 », on s'habitue au rouge, et le jour
// ou un VRAI defaut apparait plus personne ne le distingue du 60e.
// S'il se met a PASSER, c'est un evenement : la dette est reglee, et le banc le
// dit pour qu'on vienne retirer la marque.
#define EXPECT_KNOWN_DEBT(expr, why) \
	do { \
		if (!(expr)) { \
			printf("  DETTE [%s:%d] %s\n         -> %s\n", __FILE__, __LINE__, #expr, why); \
			++s_debt; \
		} else { \
			printf("  DETTE REGLEE [%s:%d] %s -- retirer la marque\n", __FILE__, __LINE__, #expr); \
			++s_pass; \
		} \
	} while (0)

#define EXPECT_TRUE(expr)                                                                                              \
	do {                                                                                                               \
		if (!(expr)) {                                                                                                 \
			printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, #expr);                                                  \
			++s_fail;                                                                                                  \
		} else {                                                                                                       \
			++s_pass;                                                                                                  \
		}                                                                                                              \
	} while (0)

// =============================================================================
// C1 / C2 -- les formes deja couvertes par les bancs qui n'ont jamais tourne
// =============================================================================

struct Point {
		NKENTSEU_REFLECT_CLASS(Point)
	public:
		NKENTSEU_PROPERTY(nk_float32, x)
	public:
		NKENTSEU_PROPERTY(nk_float32, y)
	public:
};

struct Polygon {
		NKENTSEU_REFLECT_CLASS(Polygon)
	public:
		NKENTSEU_PROPERTY(NkString, name)
	public:
		NKENTSEU_PROPERTY(NkVector<Point>, points)
	public:
};

// =============================================================================
// C3 / C4 -- la forme de NkUIDocument
//
// Calquee champ par champ sur Applications/NKUIDesign/src/NKUIDesign/Document.h :
//   NkUIDocument { NkVector<NkUINode> nodes; NkVector<NkDocMetric> metrics; }
//   NkUINode     { NkString component; NkString label; NkVector<int32> children;
//                  NkSizeDecl width/height; NkProvenance prov; }
//   NkProvenance { auteur; verifiee; corrigee; origine }
//
// L'imbrication qui compte : DocRoot.nodes est un conteneur d'OBJETS, et chacun
// de ces objets porte a son tour un conteneur (children) ET un objet imbrique
// (prov, width, height). Trois niveaux.
// =============================================================================

struct SizeDecl {
		NKENTSEU_REFLECT_CLASS(SizeDecl)
	public:
		NKENTSEU_PROPERTY(nk_int32, mode)
	public:
		NKENTSEU_PROPERTY(nk_float32, value)
	public:
		NKENTSEU_PROPERTY(nk_float32, minValue)
	public:
		NKENTSEU_PROPERTY(nk_float32, maxValue)
	public:
};

struct Provenance {
		NKENTSEU_REFLECT_CLASS(Provenance)
	public:
		NKENTSEU_PROPERTY(NkString, author)
	public:
		NKENTSEU_PROPERTY(nk_bool, verified)
	public:
		NKENTSEU_PROPERTY(nk_bool, corrected)
	public:
		NKENTSEU_PROPERTY(NkString, origin)
	public:
};

struct DocNode {
		NKENTSEU_REFLECT_CLASS(DocNode)
	public:
		NKENTSEU_PROPERTY(NkString, component)
	public:
		NKENTSEU_PROPERTY(NkString, label)
	public:
		// Le conteneur DANS l'element du conteneur : le coeur de C3.
		NKENTSEU_PROPERTY(NkVector<nk_int32>, children)
	public:
		NKENTSEU_PROPERTY(SizeDecl, width)
	public:
		NKENTSEU_PROPERTY(SizeDecl, height)
	public:
		NKENTSEU_PROPERTY(Provenance, prov)
	public:
};

struct DocMetric {
		NKENTSEU_REFLECT_CLASS(DocMetric)
	public:
		NKENTSEU_PROPERTY(NkString, name)
	public:
		NKENTSEU_PROPERTY(nk_float32, value)
	public:
};

struct DocRoot {
		NKENTSEU_REFLECT_CLASS(DocRoot)
	public:
		NKENTSEU_PROPERTY(NkString, name)
	public:
		NKENTSEU_PROPERTY(NkVector<DocNode>, nodes)
	public:
		NKENTSEU_PROPERTY(NkVector<DocMetric>, metrics)
	public:
};

// =============================================================================
// C5 -- LA FORME REELLE DES TYPES DU KIT : `const char *`
// =============================================================================
// NkSizeDecl et NkLayoutDecl (NKEditorKit/Components/NkComponentLayout.h), que
// NkUINode porte DIRECTEMENT, ne sont PAS faits de NkString :
//
//     struct NkSizeDecl {
//         NkSizeMode  mode;
//         float32     value;
//         const char *valueMetric = "";   <-- ICI
//         float32     minVal, maxVal;
//     };
//
// Ce sont des types de COMPILATION : leurs chaines sont des litteraux, et le
// type ne possede rien. KitSize ci-dessous en est le calque exact.
//
// ⚠️ Ce controle refute une affirmation que j'ai ecrite moi-meme (Q2 §6) :
//    « les pointeurs restent non geres, et c'est SANS EFFET ici : NkUIDocument
//    est plat, zero pointeur ». C'etait vrai de NkUINode DIRECTEMENT et faux
//    TRANSITIVEMENT -- NkUINode porte deux NkSizeDecl et un NkLayoutDecl, qui
//    portent quatre `const char *` a eux trois.
struct KitSize {
		NKENTSEU_REFLECT_CLASS(KitSize)
	public:
		NKENTSEU_PROPERTY(nk_int32, mode)
	public:
		NKENTSEU_PROPERTY(nk_float32, value)
	public:
		// Le membre qui coince. Categorie NK_POINTER, pas NK_STRING.
		// `= ""` comme dans le vrai NkSizeDecl : sans lui le pointeur est
		// indetermine et le banc mourait en 0xC0000005 -- un defaut de MON
		// calque, pas du pont. Le calque doit etre fidele, sinon il mesure autre
		// chose que ce qu'il pretend mesurer.
		const char *valueMetric = "";
		NKENTSEU_REFLECT_PROPERTY(valueMetric)
	public:
		NKENTSEU_PROPERTY(nk_float32, minVal)
	public:
};

static void C5_ConstCharStarDuKit() {
	printf("[C5] `const char *` du kit : ce que le pont en fait reellement\n");

	// 1. Le fait de typage, avant toute serialisation.
	const NkClass &cls = KitSize::GetStaticClass();
	const NkProperty *pm = cls.GetProperty("valueMetric");
	EXPECT_TRUE(pm != nullptr);
	if (pm) {
		// C'est LA mesure : le kit ecrit une chaine, la reflexion voit un pointeur.
		EXPECT_TRUE(pm->GetType().GetCategory() == NkTypeCategory::NK_POINTER);
		EXPECT_TRUE(pm->GetType().GetCategory() != NkTypeCategory::NK_STRING);
	}

	KitSize src;
	src.mode = 2;
	src.value = 320.0f;
	src.valueMetric = "largeur_palette"; // un nom de metrique, comme dans le kit
	src.minVal = 120.0f;

	NkArchive ar;
	const nk_bool wrote = NkReflectSerializer::SerializeObject(src, ar);

	// 2. Ce qui arrive vraiment. On n'AFFIRME pas le resultat a l'avance : on le
	//    constate et on l'imprime, parce que c'est precisement ce qui etait
	//    inconnu. Les champs qui ne sont PAS des pointeurs, eux, doivent passer.
	printf("      SerializeObject -> %s\n", wrote ? "true" : "false");
	printf("      cle 'valueMetric' presente dans l'archive : %s\n", ar.Has(NkStringView("valueMetric")) ? "oui" : "NON");

	// ⚠️ LE CONTROLE QUI VERROUILLE LA CORRECTION DU 2026-08-22.
	// Avant : SerializeObject rendait `true` en ayant omis le champ. Un repli
	// qui preserve `success` n'est pas un repli, c'est un mensonge. Maintenant
	// il rend `false` -- et l'appelant peut enfin savoir.
	EXPECT_TRUE(wrote == false);

	// Et il ecrit quand meme TOUT CE QU'IL PEUT : une archive partielle vaut
	// mieux que rien, du moment que le retour dit la verite. Ces trois-la sont
	// donc presentes MALGRE le `false`.
	EXPECT_TRUE(ar.Has(NkStringView("mode")));
	EXPECT_TRUE(ar.Has(NkStringView("value")));
	EXPECT_TRUE(ar.Has(NkStringView("minVal")));

	// Le temoin de non-regression : un type SANS pointeur doit continuer a rendre
	// `true`. C'est ce qui prouve que la correction n'a pas transforme le retour
	// en « false des qu'on respire ».
	{
		Point sain;
		sain.x = 1.0f;
		sain.y = 2.0f;
		NkArchive arSain;
		EXPECT_TRUE(NkReflectSerializer::SerializeObject(sain, arSain));
		Point relu;
		EXPECT_TRUE(NkReflectSerializer::DeserializeObject(relu, arSain));
	}

	// 3. LE CONTROLE QUI TRANCHE, et il doit etre ROUGE tant que la dette des
	//    pointeurs n'est pas reglee : le nom de metrique doit survivre a
	//    l'aller-retour. Sans lui, `width = extensible min 120 metrique
	//    "largeur_palette"` se relit sans sa metrique -- et une taille qui perd
	//    sa metrique se resout au NOMBRE, silencieusement.
	KitSize dst;
	const nk_bool read = NkReflectSerializer::DeserializeObject(dst, ar);
	printf("      DeserializeObject -> %s\n", read ? "true" : "false");

	// Cote LECTURE la regle differe, et ce n'est pas un oubli : une cle ABSENTE
	// est legitime (champ optionnel, document d'une version anterieure). Ici
	// `valueMetric` n'a jamais ete ecrite, donc il n'y a rien a perdre a la
	// relecture -- `true` est la bonne reponse.
	EXPECT_TRUE(read == true);

	const char *m = dst.valueMetric;
	const bool metricSurvived = (m != nullptr) && (NkString(m) == NkString("largeur_palette"));
	printf("      valueMetric apres aller-retour : <<%s>>\n", m ? m : "(nul)");
	EXPECT_KNOWN_DEBT(metricSurvived,
					  "dette des pointeurs : NkReflectSerializer ne gere pas const char*. "
					  "Il le DIT desormais (SerializeObject rend false), mais la metrique "
					  "est toujours perdue : il faut un proprietaire de la chaine.");
}

// ---------------------------------------------------------------------------
// Enregistrement des NkClass (auto-link NkType -> NkClass).
// ---------------------------------------------------------------------------
static void WireClasses() {
	(void)Point::GetStaticClass();
	(void)Polygon::GetStaticClass();
	(void)SizeDecl::GetStaticClass();
	(void)Provenance::GetStaticClass();
	(void)DocNode::GetStaticClass();
	(void)DocMetric::GetStaticClass();
	(void)DocRoot::GetStaticClass();
	(void)KitSize::GetStaticClass();
}

// ---------------------------------------------------------------------------
// Fabrique du document temoin.
// ---------------------------------------------------------------------------
static DocRoot MakeWitnessDocument() {
	DocRoot doc;
	doc.name = NkString("temoin");

	// Noeud 0 : la racine, deux enfants.
	DocNode root;
	root.component = NkString("content_browser");
	root.label = NkString("Explorateur");
	root.children.PushBack(1);
	root.children.PushBack(2);
	root.width.mode = 1;
	root.width.value = 320.0f;
	root.width.minValue = 120.0f;
	root.width.maxValue = 900.0f;
	root.height.mode = 2;
	root.height.value = 0.0f;
	root.height.minValue = 0.0f;
	root.height.maxValue = 0.0f;
	root.prov.author = NkString("rodolf");
	root.prov.verified = true;
	root.prov.corrected = false;
	root.prov.origin = NkString("main");

	// Noeud 1 : une feuille SANS enfant -- le conteneur vide doit survivre.
	DocNode leafA;
	leafA.component = NkString("tree_view");
	leafA.label = NkString("Arbre");
	leafA.width.mode = 0;
	leafA.width.value = 100.0f;
	leafA.height.mode = 0;
	leafA.height.value = 24.0f;
	// prov laissee AU DEFAUT sur ce noeud : c'est ce que C4 verifie.

	// Noeud 2 : une feuille avec un seul enfant.
	DocNode leafB;
	leafB.component = NkString("Button");
	leafB.label = NkString("Valider");
	leafB.children.PushBack(3);
	leafB.width.mode = 1;
	leafB.width.value = 88.0f;
	leafB.height.mode = 1;
	leafB.height.value = 32.0f;
	leafB.prov.author = NkString("nkai");
	leafB.prov.verified = false;
	leafB.prov.corrected = true;
	leafB.prov.origin = NkString("generation");

	doc.nodes.PushBack(root);
	doc.nodes.PushBack(leafA);
	doc.nodes.PushBack(leafB);

	DocMetric m0;
	m0.name = NkString("espacement.serre");
	m0.value = 4.0f;
	DocMetric m1;
	m1.name = NkString("marge.large");
	m1.value = 16.0f;
	doc.metrics.PushBack(m0);
	doc.metrics.PushBack(m1);

	return doc;
}

static nk_bool SizeEqual(const SizeDecl &a, const SizeDecl &b) {
	return a.mode == b.mode && a.value == b.value && a.minValue == b.minValue && a.maxValue == b.maxValue;
}

static nk_bool ProvEqual(const Provenance &a, const Provenance &b) {
	return a.author == b.author && a.verified == b.verified && a.corrected == b.corrected && a.origin == b.origin;
}

static nk_bool DocEqual(const DocRoot &a, const DocRoot &b) {
	if (!(a.name == b.name)) {
		return false;
	}
	if (a.nodes.Size() != b.nodes.Size() || a.metrics.Size() != b.metrics.Size()) {
		return false;
	}
	for (nk_size i = 0; i < a.nodes.Size(); ++i) {
		const DocNode &x = a.nodes[(nk_uint32)i];
		const DocNode &y = b.nodes[(nk_uint32)i];
		if (!(x.component == y.component) || !(x.label == y.label)) {
			return false;
		}
		if (x.children.Size() != y.children.Size()) {
			return false;
		}
		for (nk_size k = 0; k < x.children.Size(); ++k) {
			if (x.children[(nk_uint32)k] != y.children[(nk_uint32)k]) {
				return false;
			}
		}
		if (!SizeEqual(x.width, y.width) || !SizeEqual(x.height, y.height)) {
			return false;
		}
		if (!ProvEqual(x.prov, y.prov)) {
			return false;
		}
	}
	for (nk_size i = 0; i < a.metrics.Size(); ++i) {
		if (!(a.metrics[(nk_uint32)i].name == b.metrics[(nk_uint32)i].name)) {
			return false;
		}
		if (a.metrics[(nk_uint32)i].value != b.metrics[(nk_uint32)i].value) {
			return false;
		}
	}
	return true;
}

// =============================================================================
// C1 -- conteneur de scalaires
// =============================================================================
static void C1_ScalarContainer() {
	printf("[C1] Conteneur de scalaires NkVector<nk_int32>\n");

	const NkContainerDescriptor *d = NkContainerOf<NkVector<nk_int32>>();
	EXPECT_TRUE(d != nullptr && d->IsValid());
	EXPECT_TRUE(d && d->elementType && d->elementType->GetCategory() == NkTypeCategory::NK_INT32);

	DocNode src;
	src.component = NkString("x");
	src.children.PushBack(7);
	src.children.PushBack(8);
	src.children.PushBack(9);

	NkArchive ar;
	EXPECT_TRUE(NkReflectSerializer::SerializeObject(src, ar));
	EXPECT_TRUE(ar.Has("children"));

	DocNode dst;
	EXPECT_TRUE(NkReflectSerializer::DeserializeObject(dst, ar));
	// RELATION, pas compte fige : ce qu'on teste est « la destination a autant
	// d'elements que la source », pas « la destination en a trois ». Le trois est
	// un accident du temoin ; le jour ou on lui ajoute un element, un compte fige
	// tomberait et serait « repare » en changeant 3 en 4, sans etre relu.
	// ⚠️ Et la relation seule ne suffit pas : si la source etait vide, 0 == 0
	// passerait. D'ou l'ancrage de non-vacuite juste avant.
	EXPECT_TRUE(!src.children.Empty());
	EXPECT_TRUE(dst.children.Size() == src.children.Size());
	if (!src.children.Empty() && dst.children.Size() == src.children.Size()) {
		EXPECT_TRUE(dst.children[0] == 7);
		EXPECT_TRUE(dst.children[1] == 8);
		EXPECT_TRUE(dst.children[2] == 9);
	}
}

// =============================================================================
// C2 -- conteneur d'objets reflechis (elements SANS conteneur)
// =============================================================================
static void C2_ObjectContainer() {
	printf("[C2] Conteneur d'objets NkVector<Point>\n");

	Polygon src;
	src.name = NkString("triangle");
	Point p0;
	p0.x = 0.0f;
	p0.y = 0.0f;
	Point p1;
	p1.x = 10.0f;
	p1.y = 0.0f;
	Point p2;
	p2.x = 5.0f;
	p2.y = 8.0f;
	src.points.PushBack(p0);
	src.points.PushBack(p1);
	src.points.PushBack(p2);

	NkArchive ar;
	EXPECT_TRUE(NkReflectSerializer::SerializeReflected(&Polygon::GetStaticClass(), &src, ar));
	EXPECT_TRUE(ar.Has("points"));

	Polygon dst;
	EXPECT_TRUE(NkReflectSerializer::DeserializeReflected(&Polygon::GetStaticClass(), &dst, ar));
	EXPECT_TRUE(!src.points.Empty());
	EXPECT_TRUE(dst.points.Size() == src.points.Size());
	if (!src.points.Empty() && dst.points.Size() == src.points.Size()) {
		EXPECT_TRUE(dst.points[2].x == 5.0f && dst.points[2].y == 8.0f);
	}
}

// =============================================================================
// C3 -- NOUVEAU : conteneur d'objets QUI PORTENT EUX-MEMES un conteneur.
//       La forme exacte de NkUIDocument. Aucun banc existant ne la couvre.
// =============================================================================
static void C3_NestedContainerInObjectArray() {
	printf("[C3] Document en memoire -> archive -> document en memoire (3 niveaux)\n");

	const DocRoot src = MakeWitnessDocument();

	NkArchive ar;
	EXPECT_TRUE(NkReflectSerializer::SerializeObject(src, ar));
	EXPECT_TRUE(ar.Has("nodes"));
	EXPECT_TRUE(ar.Has("metrics"));

	// Le conteneur imbrique doit etre PRESENT dans l'element serialise.
	NkVector<NkArchive> nodeArchives;
	EXPECT_TRUE(ar.GetObjectArray(NkStringView("nodes"), nodeArchives));
	EXPECT_TRUE(!src.nodes.Empty());
	EXPECT_TRUE(nodeArchives.Size() == src.nodes.Size());
	if (!src.nodes.Empty() && nodeArchives.Size() == src.nodes.Size()) {
		EXPECT_TRUE(nodeArchives[0].Has("children"));
		EXPECT_TRUE(nodeArchives[0].Has("width"));
		EXPECT_TRUE(nodeArchives[0].Has("prov"));
	}

	DocRoot dst;
	EXPECT_TRUE(NkReflectSerializer::DeserializeObject(dst, ar));

	EXPECT_TRUE(!src.metrics.Empty());
	EXPECT_TRUE(dst.nodes.Size() == src.nodes.Size());
	EXPECT_TRUE(dst.metrics.Size() == src.metrics.Size());

	if (!src.nodes.Empty() && dst.nodes.Size() == src.nodes.Size()) {
		// Le conteneur imbrique du premier noeud.
		EXPECT_TRUE(!src.nodes[0].children.Empty());
		EXPECT_TRUE(dst.nodes[0].children.Size() == src.nodes[0].children.Size());
		if (!src.nodes[0].children.Empty() && dst.nodes[0].children.Size() == src.nodes[0].children.Size()) {
			EXPECT_TRUE(dst.nodes[0].children[0] == 1);
			EXPECT_TRUE(dst.nodes[0].children[1] == 2);
		}
		// Le conteneur VIDE du deuxieme -- vide n'est pas absent.
		// ⚠️ ICI le compte fige est le BON choix, et c'est le seul du banc :
		// ce conteneur ne doit PAS grandir, sa vacuite EST la propriete testee.
		// Ecrire == src.nodes[1].children.Size() perdrait le sens.
		EXPECT_TRUE(dst.nodes[1].children.Size() == 0);
		// Le conteneur d'un seul element du troisieme.
		EXPECT_TRUE(dst.nodes[2].children.Size() == src.nodes[2].children.Size());
		if (!src.nodes[2].children.Empty() && dst.nodes[2].children.Size() == src.nodes[2].children.Size()) {
			EXPECT_TRUE(dst.nodes[2].children[0] == 3);
		}
		// L'objet imbrique DANS l'element du conteneur.
		EXPECT_TRUE(dst.nodes[0].width.value == 320.0f);
		EXPECT_TRUE(dst.nodes[0].width.maxValue == 900.0f);
		EXPECT_TRUE(dst.nodes[2].height.value == 32.0f);
	}

	// Et l'egalite complete, provenance comprise.
	EXPECT_TRUE(DocEqual(src, dst));
}

// =============================================================================
// C4 -- NOUVEAU : la provenance survit MEME QUAND ELLE VAUT LE DEFAUT.
//
// Regle de NkComponentInstance::Save, reprise telle quelle : "Meme quand elle
// vaut le defaut. Une etiquette absente serait relue comme humain/non verifie
// -- ce qui est le defaut, donc indiscernable d'une etiquette perdue."
//
// Le noeud 1 du temoin a une provenance laissee au defaut. Le controle exige
// que le bloc `prov` soit ECRIT quand meme, pas omis parce qu'il est vide.
// =============================================================================
static void C4_ProvenanceNeverOmitted() {
	printf("[C4] Provenance ecrite meme au defaut (jamais omise)\n");

	const DocRoot src = MakeWitnessDocument();

	NkArchive ar;
	EXPECT_TRUE(NkReflectSerializer::SerializeObject(src, ar));

	NkVector<NkArchive> nodeArchives;
	EXPECT_TRUE(ar.GetObjectArray(NkStringView("nodes"), nodeArchives));
	EXPECT_TRUE(!src.nodes.Empty());
	EXPECT_TRUE(nodeArchives.Size() == src.nodes.Size());

	if (!src.nodes.Empty() && nodeArchives.Size() == src.nodes.Size()) {
		// Le noeud 1 a une provenance entierement par defaut.
		EXPECT_TRUE(nodeArchives[1].Has("prov"));
		NkArchive prov;
		EXPECT_TRUE(nodeArchives[1].GetObject(NkStringView("prov"), prov));
		// Les quatre champs doivent etre presents, y compris les faux/vides.
		EXPECT_TRUE(prov.Has("author"));
		EXPECT_TRUE(prov.Has("verified"));
		EXPECT_TRUE(prov.Has("corrected"));
		EXPECT_TRUE(prov.Has("origin"));
	}

	// Et au retour, un defaut ecrit doit se relire comme un defaut, pas comme
	// une absence : le noeud 1 doit etre EGAL a la source.
	DocRoot dst;
	EXPECT_TRUE(NkReflectSerializer::DeserializeObject(dst, ar));
	EXPECT_TRUE(dst.nodes.Size() == src.nodes.Size());
	if (!src.nodes.Empty() && dst.nodes.Size() == src.nodes.Size()) {
		EXPECT_TRUE(ProvEqual(src.nodes[1].prov, dst.nodes[1].prov));
	}
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
	printf(" SandboxNKSerialization -- pont Reflection <-> Archive\n");
	printf("=========================================================\n\n");

	WireClasses();

	C1_ScalarContainer();
	C2_ObjectContainer();
	C3_NestedContainerInObjectArray();
	C4_ProvenanceNeverOmitted();
	C5_ConstCharStarDuKit();

	const int total = s_pass + s_fail;
	if (s_debt > 0) {
		printf("\n  %d dette(s) connue(s) -- rouge attendu, n echoue pas le banc.\n", s_debt);
	}
	printf("\n---------------------------------------------------------\n");
	printf(" RESULTAT : %d / %d\n", s_pass, total);
	printf("---------------------------------------------------------\n");

	return (s_fail == 0) ? 0 : 1;
}

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================
