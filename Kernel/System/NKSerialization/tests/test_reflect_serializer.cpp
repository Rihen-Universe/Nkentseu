// =============================================================================
// tests/test_reflect_serializer.cpp
// =============================================================================
// Test round-trip du pont Reflection <-> Serialization (NkReflectSerializer).
// Declare des classes reflechies, serialise une instance vers un NkArchive,
// passe par JSON (string), puis deserialise dans une instance vierge et verifie
// l'egalite des valeurs. Standalone (sans framework de test externe).
// =============================================================================

#include <cstdio>
#include <cstring>

#include "NKReflection/NkRegistry.h"
#include "NKSerialization/NkArchive.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/Reflection/NkReflectSerializer.h"

using namespace nkentseu;

// ─── helpers ─────────────────────────────────────────────────────────────────
static int s_pass = 0;
static int s_fail = 0;

#define EXPECT_TRUE(expr) do { if (!(expr)) { \
    printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, #expr); ++s_fail; \
} else { ++s_pass; } } while(0)

#define EXPECT_EQ(a, b) EXPECT_TRUE((a) == (b))
#define EXPECT_STR(a, b) EXPECT_TRUE(strcmp((a),(b)) == 0)

// =============================================================================
// CLASSES REFLECHIES DE TEST
// =============================================================================

// Note : les macros NKENTSEU_REFLECT_CLASS / NKENTSEU_PROPERTY terminent en
// section private. On declare donc les membres explicitement, puis on les
// reflechit via NKENTSEU_REFLECT_PROPERTY, en re-ouvrant public a chaque fois.

// Sous-objet imbrique reflechi (categorie NK_CLASS avec NkClass associe).
class Vec3Like {
    NKENTSEU_REFLECT_CLASS(Vec3Like)
public:
    nk_float32 x;
    NKENTSEU_REFLECT_PROPERTY(x)
public:
    nk_float32 y;
    NKENTSEU_REFLECT_PROPERTY(y)
public:
    nk_float32 z;
    NKENTSEU_REFLECT_PROPERTY(z)
public:
    Vec3Like() : x(0.0f), y(0.0f), z(0.0f) {}
};

// Classe principale : primitifs + string + bool + entier (proxy d'enum) + objet
// imbrique reflechi + une propriete transiente qui NE doit PAS etre serialisee.
class Entity {
    NKENTSEU_REFLECT_CLASS(Entity)
public:
    nk_int32 health;
    NKENTSEU_REFLECT_PROPERTY(health)
public:
    nk_int64 score;
    NKENTSEU_REFLECT_PROPERTY(score)
public:
    nk_float32 speed;
    NKENTSEU_REFLECT_PROPERTY(speed)
public:
    nk_float64 ratio;
    NKENTSEU_REFLECT_PROPERTY(ratio)
public:
    nk_bool alive;
    NKENTSEU_REFLECT_PROPERTY(alive)
public:
    nk_uint32 kind;   // proxy d'enumeration (int)
    NKENTSEU_REFLECT_PROPERTY(kind)
public:
    NkString name;
    NKENTSEU_REFLECT_PROPERTY(name)
public:
    Vec3Like position;
    NKENTSEU_REFLECT_PROPERTY(position)
public:
    // Propriete transiente : enregistree manuellement avec le flag NK_TRANSIENT
    // dans WireReflection (la macro NKENTSEU_PROPERTY ne porte pas de flags).
    nk_int32 cacheToken;

    Entity()
        : health(0), score(0), speed(0.0f), ratio(0.0)
        , alive(false), kind(0), name(), position(), cacheToken(0) {}
};

// =============================================================================
// LIAISON DE TYPE NKType -> NkClass (necessaire pour les objets imbriques)
// =============================================================================
// Pour qu'une propriete de type Vec3Like soit reconnue comme NK_CLASS avec un
// NkClass associe, on lie le NkType<Vec3Like> a son NkClass. On le fait une
// fois au demarrage du test (idempotent).

static void WireReflection() {
    // Force l'instanciation des NkClass + enregistrement.
    const auto& entityClass = Entity::GetStaticClass();
    const auto& vecClass    = Vec3Like::GetStaticClass();

    // Lie NkType(Vec3Like) -> NkClass(Vec3Like) pour la recursion d'objet.
    const_cast<reflection::NkType&>(reflection::NkTypeOf<Vec3Like>())
        .SetClass(&vecClass);
    const_cast<reflection::NkType&>(reflection::NkTypeOf<Entity>())
        .SetClass(&entityClass);

    // Enregistre la propriete transiente "cacheToken" avec le flag NK_TRANSIENT.
    // Statique : creee une seule fois ; AddProperty deduplique par nom.
    static reflection::NkProperty s_cacheTokenProp(
        "cacheToken",
        reflection::NkTypeOf<nk_int32>(),
        offsetof(Entity, cacheToken),
        static_cast<nk_uint32>(reflection::NkPropertyFlags::NK_TRANSIENT));
    const_cast<reflection::NkClass&>(entityClass).AddProperty(&s_cacheTokenProp);

    reflection::NkRegistry::Get().RegisterClass(&entityClass);
    reflection::NkRegistry::Get().RegisterClass(&vecClass);
}

// =============================================================================
// TEST
// =============================================================================
static void TestReflectRoundTrip() {
    printf("[TEST] NkReflectSerializer round-trip (via JSON)\n");

    WireReflection();

    // 1. Instance source remplie.
    Entity src;
    src.health = 137;
    src.score  = 9876543210LL;
    src.speed  = 4.25f;
    src.ratio  = 0.123456789;
    src.alive  = true;
    src.kind   = 7u;
    src.name   = NkString("Hero");
    src.position.x = 1.5f;
    src.position.y = -2.5f;
    src.position.z = 10.0f;
    src.cacheToken = 99999; // transient : ne doit PAS survivre au round-trip.

    // 2. Serialisation reflechie -> archive.
    NkArchive ar;
    EXPECT_TRUE(NkReflectSerializer::SerializeObject(src, ar));

    // La propriete transiente ne doit pas etre presente.
    EXPECT_TRUE(!ar.Has("cacheToken"));
    // Les autres doivent etre presentes.
    EXPECT_TRUE(ar.Has("health"));
    EXPECT_TRUE(ar.Has("name"));
    EXPECT_TRUE(ar.Has("position"));

    // 3. Archive -> JSON string.
    NkString json;
    EXPECT_TRUE(NkJSONWriter::WriteArchive(ar, json, true));
    EXPECT_TRUE(!json.Empty());
    printf("    JSON:\n%s\n", json.CStr());

    // 4. JSON string -> archive reparsee.
    NkArchive ar2;
    NkString err;
    EXPECT_TRUE(NkJSONReader::ReadArchive(json.View(), ar2, &err));
    if (!err.Empty()) printf("    parse err: %s\n", err.CStr());

    // 5. Deserialisation reflechie -> instance vierge.
    Entity dst;
    dst.health = 0; dst.score = 0; dst.speed = 0.0f; dst.ratio = 0.0;
    dst.alive = false; dst.kind = 0; dst.name = NkString();
    dst.position.x = 0.0f; dst.position.y = 0.0f; dst.position.z = 0.0f;
    dst.cacheToken = -1;

    EXPECT_TRUE(NkReflectSerializer::DeserializeObject(dst, ar2));

    // 6. Verification round-trip.
    EXPECT_EQ(dst.health, 137);
    EXPECT_EQ(dst.score, 9876543210LL);
    EXPECT_TRUE(dst.speed > 4.24f && dst.speed < 4.26f);
    EXPECT_TRUE(dst.ratio > 0.123 && dst.ratio < 0.124);
    EXPECT_TRUE(dst.alive);
    EXPECT_EQ(dst.kind, 7u);
    EXPECT_STR(dst.name.CStr(), "Hero");

    // Objet imbrique.
    EXPECT_TRUE(dst.position.x > 1.49f && dst.position.x < 1.51f);
    EXPECT_TRUE(dst.position.y > -2.51f && dst.position.y < -2.49f);
    EXPECT_TRUE(dst.position.z > 9.99f && dst.position.z < 10.01f);

    // Transient : doit rester a sa valeur d'init (non touchee par la deser).
    EXPECT_EQ(dst.cacheToken, -1);
}

// =============================================================================
// main
// =============================================================================
int main() {
    printf("======================================================\n");
    printf("  NKReflectSerializer Test Suite (Phase 2)\n");
    printf("======================================================\n");

    TestReflectRoundTrip();

    printf("======================================================\n");
    printf("  Results: %d passed, %d failed\n", s_pass, s_fail);
    printf("======================================================\n");
    return s_fail > 0 ? 1 : 0;
}
