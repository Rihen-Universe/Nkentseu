// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Engine/Noge/tests/test_prefab.cpp
// =============================================================================
// Les composants d'un NkPrefab de Noge sont-ils POSES sur l'entite instanciee ?
// Jusqu'au 2026-09-29, non : NkPrefab::Instantiate les desserialisait dans un
// tampon puis le jetait (TODO « dispatcher generique manquant »), et
// WithComponent<T> appelait un SerializeComponent declare mais jamais defini.
//
// PRE-ENREGISTREMENT :
//   (np1) WithComponent<T>(valeurs) : le prefab porte le composant, sous son nom
//   (np2) Instantiate : l'entite porte le composant, AUX MEMES VALEURS
//   (np3) un composant decrit a la main en JSON (fichier edite) est pose aussi
//   (np4) un nom de composant inconnu ne pose rien et ne casse rien
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKECS/Reflect/NkReflect.h"
#include "NKECS/Reflect/NkReflectBridge.h"
#include "Noge/ECS/Prefab/NkPrefab.h"

using namespace nkentseu;
// Les macros NK_FIELD* nomment les drapeaux sans espace de noms (comme les
// suites de NKECS).
using namespace nkentseu::ecs::reflect;

struct NkBancPrefabVie {
		int32 pv;
		float32 vitesse;
};
NK_COMPONENT(NkBancPrefabVie)
NK_REFLECT_BEGIN(NkBancPrefabVie)
NK_FIELD_EX(pv, ::nkentseu::ecs::reflect::NkFieldType::Int32)
NK_FIELD_EX(vitesse, ::nkentseu::ecs::reflect::NkFieldType::Float32)
NK_REFLECT_END(NkBancPrefabVie)

namespace {
	void Enregistrer() {
		ecs::reflect::NkRegisterComponentReflection<NkBancPrefabVie>();
	}
} // namespace

TEST_CASE(NogePrefab, ComposantsPosesAInstanciation) {
	Enregistrer();
	NkPrefab orc("Orc");
	orc.WithComponent<NkBancPrefabVie>(50, 2.5f);
	// (np1)
	ASSERT_TRUE(orc.HasComponent("NkBancPrefabVie"));

	// (np2)
	ecs::NkWorld monde;
	const ecs::NkEntityId e = orc.Instantiate(monde, "orc1");
	const NkBancPrefabVie *v = monde.Get<NkBancPrefabVie>(e);
	ASSERT_NOT_NULL(v);
	ASSERT_EQUAL(50, static_cast<int>(v->pv));
	ASSERT_NEAR(2.5f, v->vitesse, 0.0001f);
}

TEST_CASE(NogePrefab, ComposantDecritEnJson) {
	Enregistrer();
	// (np3) Le chemin d'un prefab relu d'un fichier : le JSON du composant, tel
	// qu'un humain l'a ecrit.
	NkPrefab gob("Gobelin");
	gob.components[NkString("NkBancPrefabVie")] = NkPrefabComponentData("NkBancPrefabVie", "{\"pv\": 7, \"vitesse\": 1.5}");
	ecs::NkWorld monde;
	const ecs::NkEntityId e = gob.Instantiate(monde, nullptr);
	const NkBancPrefabVie *v = monde.Get<NkBancPrefabVie>(e);
	ASSERT_NOT_NULL(v);
	ASSERT_EQUAL(7, static_cast<int>(v->pv));
	ASSERT_NEAR(1.5f, v->vitesse, 0.0001f);

	// (np4)
	NkPrefab inconnu("Inconnu");
	inconnu.components[NkString("NkCeComposantNExistePas")] = NkPrefabComponentData("NkCeComposantNExistePas", "{}");
	const ecs::NkEntityId f = inconnu.Instantiate(monde, nullptr);
	ASSERT_TRUE(monde.IsAlive(f));
	ASSERT_TRUE(monde.Get<NkBancPrefabVie>(f) == nullptr);
}
