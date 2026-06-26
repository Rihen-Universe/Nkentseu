// =============================================================================
// tests/test_entity_serialization.cpp
// =============================================================================
// Finition : SERIALISATION NIVEAU ENTITE / MONDE (NKECS).
//   - SerializeEntity / DeserializeEntity : une entite porteuse de plusieurs
//     composants reflechis -> archive (sous-objet par composant) -> JSON ->
//     archive -> entite vierge, verifie les valeurs.
//   - SerializeWorld / DeserializeWorld : plusieurs entites -> archive -> monde
//     reconstruit.
//
// Standalone (sans framework de test externe). Zero-STL cote moteur.
// =============================================================================

#include <cstdio>

#include "NKECS/Core/NkTypeRegistry.h"
#include "NKECS/Reflect/NkReflect.h"
#include "NKECS/Reflect/NkReflectBridge.h"
#include "NKECS/Serialization/NkJsonSerialization.h"
#include "NKECS/Serialization/NkEntitySerialization.h"
#include "NKECS/World/NkWorld.h"

#include "NKSerialization/NkArchive.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/JSON/NkJSONReader.h"

using namespace nkentseu;
using namespace nkentseu::ecs;
using namespace nkentseu::ecs::reflect;

static int s_pass = 0;
static int s_fail = 0;
#define EXPECT_TRUE(expr) do { if (!(expr)) { \
    printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, #expr); ++s_fail; \
} else { ++s_pass; } } while(0)
#define EXPECT_EQ(a, b) EXPECT_TRUE((a) == (b))

// =============================================================================
// COMPOSANTS DE TEST (deux composants reflechis)
// =============================================================================
struct Transform {
    float32 px, py, pz;
    float32 scale;
};
NK_COMPONENT(Transform)
NK_REFLECT_BEGIN(Transform)
    NK_FIELD_EX(px,    ::nkentseu::ecs::reflect::NkFieldType::Float32)
    NK_FIELD_EX(py,    ::nkentseu::ecs::reflect::NkFieldType::Float32)
    NK_FIELD_EX(pz,    ::nkentseu::ecs::reflect::NkFieldType::Float32)
    NK_FIELD_EX(scale, ::nkentseu::ecs::reflect::NkFieldType::Float32)
NK_REFLECT_END(Transform)

struct Health {
    int32 current;
    int32 max;
    bool  invincible;
};
NK_COMPONENT(Health)
NK_REFLECT_BEGIN(Health)
    NK_FIELD_EX(current,    ::nkentseu::ecs::reflect::NkFieldType::Int32)
    NK_FIELD_EX(max,        ::nkentseu::ecs::reflect::NkFieldType::Int32)
    NK_FIELD_EX(invincible, ::nkentseu::ecs::reflect::NkFieldType::Bool)
NK_REFLECT_END(Health)

static void RegisterAll() {
    NkRegisterComponentReflection<Transform>();
    NkRegisterComponentReflection<Health>();
}

// =============================================================================
// TEST 1 : entite avec 2 composants -> JSON -> entite vierge
// =============================================================================
static void TestEntityRoundTrip() {
    printf("[TEST] SerializeEntity / DeserializeEntity (2 composants)\n");
    RegisterAll();

    NkWorld world;

    // Entite source.
    const NkEntityId e = world.CreateEntity();
    Transform t; t.px = 1.5f; t.py = -2.5f; t.pz = 10.25f; t.scale = 3.0f;
    Health    h; h.current = 75; h.max = 100; h.invincible = true;
    world.Add<Transform>(e, t);
    world.Add<Health>(e, h);

    // Serialise l'entite -> archive (un sous-objet par composant).
    NkArchive ar;
    EXPECT_TRUE(serialization::SerializeEntity(world, e, ar));
    EXPECT_TRUE(ar.Has("Transform"));
    EXPECT_TRUE(ar.Has("Health"));

    // archive -> JSON -> archive.
    NkString json;
    EXPECT_TRUE(NkJSONWriter::WriteArchive(ar, json, true));
    printf("    JSON:\n%s\n", json.CStr());
    NkArchive ar2;
    NkString err;
    EXPECT_TRUE(NkJSONReader::ReadArchive(json.View(), ar2, &err));
    if (!err.Empty()) printf("    parse err: %s\n", err.CStr());

    // Nouvelle entite VIERGE (sans composant) -> deserialise.
    const NkEntityId e2 = world.CreateEntity();
    EXPECT_TRUE(serialization::DeserializeEntity(world, e2, ar2));

    // Les composants doivent maintenant exister avec les bonnes valeurs.
    EXPECT_TRUE(world.Has<Transform>(e2));
    EXPECT_TRUE(world.Has<Health>(e2));

    const Transform* t2 = world.Get<Transform>(e2);
    const Health*    h2 = world.Get<Health>(e2);
    EXPECT_TRUE(t2 != nullptr);
    EXPECT_TRUE(h2 != nullptr);
    if (t2) {
        EXPECT_TRUE(t2->px > 1.49f && t2->px < 1.51f);
        EXPECT_TRUE(t2->py > -2.51f && t2->py < -2.49f);
        EXPECT_TRUE(t2->pz > 10.24f && t2->pz < 10.26f);
        EXPECT_TRUE(t2->scale > 2.99f && t2->scale < 3.01f);
    }
    if (h2) {
        EXPECT_EQ(h2->current, 75);
        EXPECT_EQ(h2->max, 100);
        EXPECT_TRUE(h2->invincible);
    }
}

// =============================================================================
// TEST 2 : monde (plusieurs entites) -> JSON -> monde reconstruit
// =============================================================================
static void TestWorldRoundTrip() {
    printf("[TEST] SerializeWorld / DeserializeWorld\n");
    RegisterAll();

    NkWorld src;

    // 3 entites avec compositions differentes.
    const NkEntityId a = src.CreateEntity();
    { Transform t; t.px = 1; t.py = 2; t.pz = 3; t.scale = 1; src.Add<Transform>(a, t); }

    const NkEntityId b = src.CreateEntity();
    { Transform t; t.px = 4; t.py = 5; t.pz = 6; t.scale = 2; src.Add<Transform>(b, t);
      Health h; h.current = 30; h.max = 50; h.invincible = false; src.Add<Health>(b, h); }

    const NkEntityId c = src.CreateEntity();
    { Health h; h.current = 10; h.max = 10; h.invincible = true; src.Add<Health>(c, h); }

    EXPECT_EQ(src.EntityCount(), 3u);

    // Serialise tout le monde.
    NkArchive ar;
    EXPECT_TRUE(serialization::SerializeWorld(src, ar));
    EXPECT_TRUE(ar.Has("entities"));

    NkString json;
    EXPECT_TRUE(NkJSONWriter::WriteArchive(ar, json, true));
    printf("    World JSON (tronque):\n%.600s\n    ...\n", json.CStr());

    NkArchive ar2;
    NkString err;
    EXPECT_TRUE(NkJSONReader::ReadArchive(json.View(), ar2, &err));

    // Reconstruit dans un monde vierge.
    NkWorld dst;
    EXPECT_TRUE(serialization::DeserializeWorld(dst, ar2));
    EXPECT_EQ(dst.EntityCount(), 3u);

    // Compte les compositions par requete (independant des ids).
    int transforms = 0, healths = 0, both = 0;
    dst.Query<Transform>().ForEach([&](NkEntityId, Transform&) { ++transforms; });
    dst.Query<Health>().ForEach([&](NkEntityId, Health&) { ++healths; });
    dst.Query<Transform, Health>().ForEach([&](NkEntityId, Transform&, Health&) { ++both; });
    EXPECT_EQ(transforms, 2);  // a, b
    EXPECT_EQ(healths, 2);     // b, c
    EXPECT_EQ(both, 1);        // b

    // Verifie une valeur precise : l'entite Transform-only doit avoir px=1.
    bool foundPx1 = false, foundBoth = false;
    dst.Query<Transform>().ForEach([&](NkEntityId, Transform& t) {
        if (t.px > 0.99f && t.px < 1.01f) foundPx1 = true;
    });
    dst.Query<Transform, Health>().ForEach([&](NkEntityId, Transform& t, Health& h) {
        if (t.px > 3.99f && t.px < 4.01f && h.current == 30 && h.max == 50) foundBoth = true;
    });
    EXPECT_TRUE(foundPx1);
    EXPECT_TRUE(foundBoth);
}

int main() {
    printf("======================================================\n");
    printf("  NKECS - Serialisation niveau ENTITE / MONDE\n");
    printf("======================================================\n");

    TestEntityRoundTrip();
    TestWorldRoundTrip();

    printf("======================================================\n");
    printf("  Results: %d passed, %d failed\n", s_pass, s_fail);
    printf("======================================================\n");
    return s_fail > 0 ? 1 : 0;
}
