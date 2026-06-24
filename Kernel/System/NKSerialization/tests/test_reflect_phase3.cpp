// =============================================================================
// tests/test_reflect_phase3.cpp
// =============================================================================
// Tests Phase 3 du chantier NKReflection (enum reflection + conteneurs +
// metadonnees d'edition + auto-link NkType->NkClass). Standalone (sans
// framework de test externe), sur le modele de test_reflect_serializer.cpp.
// =============================================================================

#include <cstdio>
#include <cstring>

#include "NKReflection/NkRegistry.h"
#include "NKReflection/NkEnumDescriptor.h"
#include "NKReflection/NkContainerTrait.h"
#include "NKReflection/NkReflectMeta.h"
#include "NKSerialization/NkArchive.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/Reflection/NkReflectSerializer.h"

using namespace nkentseu;
using namespace nkentseu::reflection;

// ─── helpers ─────────────────────────────────────────────────────────────────
static int s_pass = 0;
static int s_fail = 0;

#define EXPECT_TRUE(expr) do { if (!(expr)) { \
    printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, #expr); ++s_fail; \
} else { ++s_pass; } } while(0)

#define EXPECT_EQ(a, b) EXPECT_TRUE((a) == (b))
#define EXPECT_STR(a, b) EXPECT_TRUE((a) != nullptr && strcmp((a),(b)) == 0)

// =============================================================================
// TYPES DE TEST
// =============================================================================

// Enum reflechi (categorie NK_ENUM via DetermineCategory + descripteur).
enum class CreatureKind : nk_uint8 {
    Goblin = 0,
    Orc    = 3,
    Dragon = 7,
};
NKENTSEU_REFLECT_ENUM(CreatureKind, Goblin, Orc, Dragon)

// Sous-objet imbrique reflechi (NK_CLASS). PAS de SetClass manuel : on verifie
// que l'auto-link de NKENTSEU_REFLECT_CLASS suffit.
class Vec3P3 {
    NKENTSEU_REFLECT_CLASS(Vec3P3)
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
    Vec3P3() : x(0.0f), y(0.0f), z(0.0f) {}
};

// Classe principale : enum + conteneur NkVector<nk_int32> + objet imbrique +
// propriete transiente posee VIA FLAGS (NKENTSEU_PROPERTY_FLAGS).
class Hero {
    NKENTSEU_REFLECT_CLASS(Hero)
public:
    CreatureKind kind;
    NKENTSEU_REFLECT_PROPERTY(kind)
public:
    NkVector<nk_int32> scores;
    NKENTSEU_REFLECT_PROPERTY(scores)
public:
    Vec3P3 position;
    NKENTSEU_REFLECT_PROPERTY(position)
public:
    // Propriete transiente posee par FLAGS (et non a la main) : ne doit PAS
    // etre serialisee.
    NKENTSEU_PROPERTY_FLAGS(nk_int32, cacheToken, NK_REFLECT_TRANSIENT)
public:
    // Propriete avec metadonnees d'edition (range/tooltip/categorie).
    nk_float32 health;
    NKENTSEU_REFLECT_PROPERTY(health)
public:
    Hero() : kind(CreatureKind::Goblin), scores(), position(), cacheToken(0), health(0.0f) {}
};

// =============================================================================
// PREPARATION REFLEXION
// =============================================================================
static void WireMeta() {
    // Force l'instanciation des NkClass (et donc l'auto-link NkType->NkClass).
    const auto& heroClass = Hero::GetStaticClass();
    (void)Vec3P3::GetStaticClass();

    NkRegistry::Get().RegisterClass(&heroClass);

    // Pose des metadonnees d'edition sur 'health' (range/tooltip/categorie).
    NkProperty& healthProp =
        const_cast<NkProperty&>(*heroClass.GetProperty("health"));
    healthProp.SetRange(0.0f, 100.0f);
    healthProp.SetTooltip("Points de vie du heros");
    healthProp.SetCategory("Stats");
}

// =============================================================================
// TEST 1 : ENUM (descripteur + ToString/FromString + categorie)
// =============================================================================
static void TestEnumReflection() {
    printf("[TEST] Enum reflection\n");

    // Categorie NK_ENUM deduite par DetermineCategory.
    EXPECT_EQ(NkTypeOf<CreatureKind>().GetCategory(), NkTypeCategory::NK_ENUM);

    // Descripteur enregistre.
    const NkEnumDescriptor* d = NkEnumRegistry::Get().FindFor<CreatureKind>();
    EXPECT_TRUE(d != nullptr);
    if (d) {
        EXPECT_EQ(d->GetCount(), (nk_usize)3);
    }

    // ToString / FromString.
    EXPECT_STR(NkEnumToString<CreatureKind>(CreatureKind::Dragon), "Dragon");
    EXPECT_STR(NkEnumToString<CreatureKind>(CreatureKind::Orc), "Orc");

    CreatureKind parsed = CreatureKind::Goblin;
    EXPECT_TRUE(NkEnumFromString<CreatureKind>("Dragon", parsed));
    EXPECT_TRUE(parsed == CreatureKind::Dragon);

    // Nom inconnu -> echec.
    EXPECT_TRUE(!NkEnumFromString<CreatureKind>("Wyvern", parsed));
}

// =============================================================================
// TEST 2 : METADONNEES D'EDITION
// =============================================================================
static void TestEditMetadata() {
    printf("[TEST] Metadonnees d'edition\n");

    const auto& heroClass = Hero::GetStaticClass();
    const NkProperty* health = heroClass.GetProperty("health");
    EXPECT_TRUE(health != nullptr);
    if (!health) return;

    nk_float32 mn = -1.0f, mx = -1.0f;
    EXPECT_TRUE(health->GetRange(mn, mx));
    EXPECT_TRUE(mn == 0.0f && mx == 100.0f);
    EXPECT_TRUE(health->HasFlag(NK_REFLECT_RANGE));
    EXPECT_STR(health->GetTooltip(), "Points de vie du heros");
    EXPECT_STR(health->GetCategory(), "Stats");

    // Le flag transient pose VIA NKENTSEU_PROPERTY_FLAGS doit etre present et
    // unifie avec IsTransient().
    const NkProperty* cache = heroClass.GetProperty("cacheToken");
    EXPECT_TRUE(cache != nullptr);
    if (cache) {
        EXPECT_TRUE(cache->HasFlag(NK_REFLECT_TRANSIENT));
        EXPECT_TRUE(cache->IsTransient());
    }
}

// =============================================================================
// TEST 3 : CONTENEUR NkVector<nk_int32> round-trip
// =============================================================================
static void TestContainerRoundTrip() {
    printf("[TEST] Conteneur NkVector<nk_int32> round-trip\n");

    const auto& heroClass = Hero::GetStaticClass();
    const NkProperty* scoresProp = heroClass.GetProperty("scores");
    EXPECT_TRUE(scoresProp != nullptr);
    EXPECT_TRUE(scoresProp && scoresProp->IsContainer());

    Hero src;
    src.scores.PushBack(11);
    src.scores.PushBack(22);
    src.scores.PushBack(33);
    src.kind = CreatureKind::Orc;
    src.position.x = 1.0f; src.position.y = 2.0f; src.position.z = 3.0f;
    src.cacheToken = 999;   // transient
    src.health = 42.0f;

    NkArchive ar;
    EXPECT_TRUE(NkReflectSerializer::SerializeObject(src, ar));

    // Conteneur present, transient absent.
    EXPECT_TRUE(ar.Has("scores"));
    EXPECT_TRUE(!ar.Has("cacheToken"));

    // Deserialisation dans une instance vierge (round-trip dans le meme archive,
    // sans JSON pour preserver les tableaux scalaires bruts).
    Hero dst;
    EXPECT_TRUE(NkReflectSerializer::DeserializeObject(dst, ar));

    EXPECT_EQ(dst.scores.Size(), (nk_usize)3);
    if (dst.scores.Size() == 3) {
        EXPECT_EQ(dst.scores[0], 11);
        EXPECT_EQ(dst.scores[1], 22);
        EXPECT_EQ(dst.scores[2], 33);
    }
    // Objet imbrique round-trip (AUTO-LINK, aucun SetClass manuel).
    EXPECT_TRUE(dst.position.x == 1.0f && dst.position.y == 2.0f && dst.position.z == 3.0f);
}

// =============================================================================
// TEST 4 : ENUM round-trip via serializer + JSON (nom symbolique)
// =============================================================================
static void TestEnumSerializeJSON() {
    printf("[TEST] Enum serialise comme NOM + round-trip JSON\n");

    Hero src;
    src.kind = CreatureKind::Dragon;
    src.position.x = 9.0f; src.position.y = 8.0f; src.position.z = 7.0f;
    src.health = 75.0f;

    NkArchive ar;
    EXPECT_TRUE(NkReflectSerializer::SerializeObject(src, ar));

    // L'enum doit etre stocke comme STRING "Dragon".
    NkString kindStr;
    EXPECT_TRUE(ar.GetString("kind", kindStr));
    EXPECT_STR(kindStr.CStr(), "Dragon");

    // Round-trip JSON (l'enum et l'objet imbrique survivent ; le conteneur est
    // vide ici donc pas concerne par la limite JSON des tableaux).
    NkString json;
    EXPECT_TRUE(NkJSONWriter::WriteArchive(ar, json, true));
    printf("    JSON:\n%s\n", json.CStr());

    NkArchive ar2;
    NkString err;
    EXPECT_TRUE(NkJSONReader::ReadArchive(json.View(), ar2, &err));
    if (!err.Empty()) printf("    parse err: %s\n", err.CStr());

    Hero dst;
    dst.kind = CreatureKind::Goblin;
    EXPECT_TRUE(NkReflectSerializer::DeserializeObject(dst, ar2));

    EXPECT_TRUE(dst.kind == CreatureKind::Dragon);
    EXPECT_TRUE(dst.position.x == 9.0f && dst.position.y == 8.0f && dst.position.z == 7.0f);
}

// =============================================================================
// TEST 5 : helpers de conteneur (descripteur direct)
// =============================================================================
static void TestContainerDescriptor() {
    printf("[TEST] NkContainerDescriptor direct\n");

    const NkContainerDescriptor* d = NkContainerOf<NkVector<nk_int32>>();
    EXPECT_TRUE(d != nullptr && d->IsValid());
    EXPECT_TRUE(d && d->elementType &&
                d->elementType->GetCategory() == NkTypeCategory::NK_INT32);

    NkVector<nk_int32> v;
    void* cont = &v;
    EXPECT_EQ(d->GetCount(cont), (nk_usize)0);

    void* e0 = d->PushBackDefault(cont);
    EXPECT_TRUE(e0 != nullptr);
    *static_cast<nk_int32*>(e0) = 1234;
    EXPECT_EQ(d->GetCount(cont), (nk_usize)1);
    EXPECT_EQ(v[0], 1234);

    d->Clear(cont);
    EXPECT_EQ(d->GetCount(cont), (nk_usize)0);

    // Type non-conteneur : trait false.
    EXPECT_TRUE(!NkContainerTrait<nk_int32>::IsContainer);
    EXPECT_TRUE(NkContainerTrait<NkVector<nk_int32>>::IsContainer);
}

// =============================================================================
// main
// =============================================================================
int main() {
    printf("======================================================\n");
    printf("  NKReflection Phase 3 Test Suite\n");
    printf("======================================================\n");

    WireMeta();

    TestEnumReflection();
    TestEditMetadata();
    TestContainerRoundTrip();
    TestEnumSerializeJSON();
    TestContainerDescriptor();

    printf("======================================================\n");
    printf("  Results: %d passed, %d failed\n", s_pass, s_fail);
    printf("======================================================\n");
    return s_fail > 0 ? 1 : 0;
}
