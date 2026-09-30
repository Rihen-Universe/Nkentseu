//
// test_hierarchy.cpp
// =============================================================================
// Description :
//   La topologie parent / enfant de NKECS (NKECS/Hierarchy/NkHierarchy.h).
//
// Caracteristiques :
//   - PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//     (h1) un lien pose se relit ; detacher rend la racine
//     (h2) les boucles sont refusees : soi-meme, et un parent descendant de
//          l'enfant ; le lien d'avant est garde tel quel
//     (h3) enfants directs et descendants (parents AVANT enfants)
//     (h4) une entite morte n'est le parent de personne : l'enfant se lit racine
//     (h5) la destruction recursive emporte toute la descendance, et elle seule
//     (h6) aucun plafond : 300 enfants sous un meme parent (Noge : 64)
//   - Standalone, sans framework externe (comme les deux autres suites).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include <cstdio>

#include "NKECS/Hierarchy/NkHierarchy.h"
#include "NKECS/World/NkWorld.h"

using namespace nkentseu;
using namespace nkentseu::ecs;

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

struct NkBancPoids {
		int32 valeur = 0;
};

static bool Contient(const NkVector<NkEntityId> &v, NkEntityId e) {
	for (nk_usize i = 0; i < v.Size(); ++i) {
		if (v[i] == e) {
			return true;
		}
	}
	return false;
}

static nk_usize Rang(const NkVector<NkEntityId> &v, NkEntityId e) {
	for (nk_usize i = 0; i < v.Size(); ++i) {
		if (v[i] == e) {
			return i;
		}
	}
	return v.Size();
}

// (h1)
static void Cas_h1() {
	NkWorld w;
	const NkEntityId a = w.CreateEntity();
	const NkEntityId b = w.CreateEntity();
	w.Add<NkBancPoids>(a);
	w.Add<NkBancPoids>(b);
	EXPECT_TRUE(!NkGetParent(w, b).IsValid());
	EXPECT_TRUE(NkSetParent(w, b, a));
	EXPECT_TRUE(NkGetParent(w, b) == a);
	EXPECT_TRUE(NkHierarchyDepth(w, b) == 1u);
	EXPECT_TRUE(NkIsDescendantOf(w, b, a));
	EXPECT_TRUE(!NkIsDescendantOf(w, a, b));
	EXPECT_TRUE(NkSetParent(w, b, NkEntityId::Invalid()));
	EXPECT_TRUE(!NkGetParent(w, b).IsValid() && !w.Has<NkParent>(b));
}

// (h2)
static void Cas_h2() {
	NkWorld w;
	const NkEntityId a = w.CreateEntity();
	const NkEntityId b = w.CreateEntity();
	const NkEntityId c = w.CreateEntity();
	EXPECT_TRUE(NkSetParent(w, b, a));
	EXPECT_TRUE(NkSetParent(w, c, b));
	EXPECT_TRUE(!NkSetParent(w, a, a));
	EXPECT_TRUE(!NkSetParent(w, a, c)); // a -> c -> b -> a : boucle
	EXPECT_TRUE(!NkGetParent(w, a).IsValid());
	EXPECT_TRUE(NkGetParent(w, c) == b);
	EXPECT_TRUE(NkHierarchyDepth(w, c) == 2u);
}

// (h3)
static void Cas_h3() {
	NkWorld w;
	const NkEntityId r = w.CreateEntity();
	const NkEntityId e1 = w.CreateEntity();
	const NkEntityId e2 = w.CreateEntity();
	const NkEntityId p1 = w.CreateEntity();
	const NkEntityId autre = w.CreateEntity();
	// Petit-enfant CREE avant son parent, pour que l'ordre de la requete
	// ne donne pas l'ordre de propagation par hasard.
	NkSetParent(w, p1, e1);
	NkSetParent(w, e1, r);
	NkSetParent(w, e2, r);
	NkVector<NkEntityId> enfants;
	NkCollectChildren(w, r, enfants);
	EXPECT_TRUE(enfants.Size() == 2u && Contient(enfants, e1) && Contient(enfants, e2));
	NkVector<NkEntityId> desc;
	NkCollectDescendants(w, r, desc);
	EXPECT_TRUE(desc.Size() == 3u && Contient(desc, p1) && !Contient(desc, autre) && !Contient(desc, r));
	EXPECT_TRUE(Rang(desc, e1) < Rang(desc, p1));
}

// (h4)
static void Cas_h4() {
	NkWorld w;
	const NkEntityId a = w.CreateEntity();
	const NkEntityId b = w.CreateEntity();
	w.Add<NkBancPoids>(a);
	NkSetParent(w, b, a);
	w.Destroy(a); // sans passer par la hierarchie
	EXPECT_TRUE(!NkGetParent(w, b).IsValid());
	EXPECT_TRUE(NkHierarchyDepth(w, b) == 0u);
	// Un nouvel occupant du meme index n'herite pas des enfants de l'ancien.
	const NkEntityId a2 = w.CreateEntity();
	EXPECT_TRUE(a2.index != a.index || a2.gen != a.gen);
	NkVector<NkEntityId> enfants;
	NkCollectChildren(w, a2, enfants);
	EXPECT_TRUE(enfants.Size() == 0u);
}

// (h5)
static void Cas_h5() {
	NkWorld w;
	const NkEntityId r = w.CreateEntity();
	const NkEntityId e = w.CreateEntity();
	const NkEntityId p = w.CreateEntity();
	const NkEntityId voisin = w.CreateEntity();
	NkSetParent(w, e, r);
	NkSetParent(w, p, e);
	NkDestroyRecursive(w, r);
	EXPECT_TRUE(!w.IsAlive(r) && !w.IsAlive(e) && !w.IsAlive(p));
	EXPECT_TRUE(w.IsAlive(voisin));
	EXPECT_TRUE(w.EntityCount() == 1u);
}

// (h6)
static void Cas_h6() {
	NkWorld w;
	const NkEntityId r = w.CreateEntity();
	for (int32 i = 0; i < 300; ++i) {
		const NkEntityId e = w.CreateEntity();
		NkSetParent(w, e, r);
	}
	NkVector<NkEntityId> enfants;
	NkCollectChildren(w, r, enfants);
	EXPECT_TRUE(enfants.Size() == 300u);
}

// ⚠️ UN CAS PAR FONCTION : un NkWorld pese lourd (tables d'archetypes), et six
//    mondes dans le meme cadre de pile debordaient la pile de 1 Mo en Debug
//    (0xC00000FD au demarrage, avant la premiere ligne ecrite).
int main() {
	printf("======================================================\n");
	printf("  NKECS - Hierarchie parent / enfant\n");
	printf("======================================================\n");

	Cas_h1();
	Cas_h2();
	Cas_h3();
	Cas_h4();
	Cas_h5();
	Cas_h6();

	printf("======================================================\n");
	printf("  Results: %d passed, %d failed\n", s_pass, s_fail);
	printf("======================================================\n");
	return s_fail > 0 ? 1 : 0;
}
