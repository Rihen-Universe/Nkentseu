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
	EXPECT_TRUE(dst.children.Size() == 3);
	if (dst.children.Size() == 3) {
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
	EXPECT_TRUE(dst.points.Size() == 3);
	if (dst.points.Size() == 3) {
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
	EXPECT_TRUE(nodeArchives.Size() == 3);
	if (nodeArchives.Size() == 3) {
		EXPECT_TRUE(nodeArchives[0].Has("children"));
		EXPECT_TRUE(nodeArchives[0].Has("width"));
		EXPECT_TRUE(nodeArchives[0].Has("prov"));
	}

	DocRoot dst;
	EXPECT_TRUE(NkReflectSerializer::DeserializeObject(dst, ar));

	EXPECT_TRUE(dst.nodes.Size() == 3);
	EXPECT_TRUE(dst.metrics.Size() == 2);

	if (dst.nodes.Size() == 3) {
		// Le conteneur imbrique du premier noeud.
		EXPECT_TRUE(dst.nodes[0].children.Size() == 2);
		if (dst.nodes[0].children.Size() == 2) {
			EXPECT_TRUE(dst.nodes[0].children[0] == 1);
			EXPECT_TRUE(dst.nodes[0].children[1] == 2);
		}
		// Le conteneur VIDE du deuxieme -- vide n'est pas absent.
		EXPECT_TRUE(dst.nodes[1].children.Size() == 0);
		// Le conteneur d'un seul element du troisieme.
		EXPECT_TRUE(dst.nodes[2].children.Size() == 1);
		if (dst.nodes[2].children.Size() == 1) {
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
	EXPECT_TRUE(nodeArchives.Size() == 3);

	if (nodeArchives.Size() == 3) {
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
	EXPECT_TRUE(dst.nodes.Size() == 3);
	if (dst.nodes.Size() == 3) {
		EXPECT_TRUE(ProvEqual(src.nodes[1].prov, dst.nodes[1].prov));
	}
}

// =============================================================================
// POINT D'ENTREE
// =============================================================================
int main() {
	printf("=========================================================\n");
	printf(" SandboxNKSerialization -- pont Reflection <-> Archive\n");
	printf("=========================================================\n\n");

	WireClasses();

	C1_ScalarContainer();
	C2_ObjectContainer();
	C3_NestedContainerInObjectArray();
	C4_ProvenanceNeverOmitted();

	const int total = s_pass + s_fail;
	printf("\n---------------------------------------------------------\n");
	printf(" RESULTAT : %d / %d\n", s_pass, total);
	printf("---------------------------------------------------------\n");

	return (s_fail == 0) ? 0 : 1;
}

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================
