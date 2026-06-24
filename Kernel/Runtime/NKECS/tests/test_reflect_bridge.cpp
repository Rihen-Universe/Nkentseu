// =============================================================================
// tests/test_reflect_bridge.cpp
// =============================================================================
// Test round-trip de la Phase 4 : reflexion des COMPOSANTS NKECS branchee sur
// NKReflection + NKSerialization (pont NkReflectBridge).
//
// Definit un composant Transform avec des champs reflechis (NK_REFLECT_*),
// l'enregistre via NkRegisterComponentReflection<Transform>(), serialise une
// instance vers une NkArchive -> JSON (string) -> archive reparsee, deserialise
// dans une instance vierge et verifie l'egalite des valeurs (round-trip).
//
// Verifie aussi :
//   - ComponentMeta.reflectClass / serialize / deserialize sont branches.
//   - L'API type-erased (SerializeComponentMeta) fonctionne.
//   - Le mapping NkFieldType -> NkTypeCategory.
//
// Standalone (sans framework de test externe). Zero-STL cote moteur.
// =============================================================================

#include <cstdio>
#include <cstring>

#include "NKECS/Core/NkTypeRegistry.h"
#include "NKECS/Reflect/NkReflect.h"
#include "NKECS/Reflect/NkReflectBridge.h"
#include "NKECS/Serialization/NkJsonSerialization.h"

#include "NKSerialization/NkArchive.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/JSON/NkJSONReader.h"

using namespace nkentseu;
using namespace nkentseu::ecs;
using namespace nkentseu::ecs::reflect;

// ─── helpers de test ─────────────────────────────────────────────────────────
static int s_pass = 0;
static int s_fail = 0;

#define EXPECT_TRUE(expr) do { if (!(expr)) { \
    printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, #expr); ++s_fail; \
} else { ++s_pass; } } while(0)

#define EXPECT_EQ(a, b) EXPECT_TRUE((a) == (b))

// =============================================================================
// COMPOSANT DE TEST
// =============================================================================
// Transform simple : position (3 floats) + scale (float) + nbChildren (uint32)
// + alive (bool). Champs scalaires -> (de)serialises par le pont generique.

struct Transform {
    float32 px;
    float32 py;
    float32 pz;
    float32 scale;
    uint32  nbChildren;
    bool    alive;
};

// Enregistrement ECS (nom lisible + ComponentMeta).
NK_COMPONENT(Transform)

// Description des champs reflechis (NKECS/Reflect). Types explicites.
NK_REFLECT_BEGIN(Transform)
    NK_FIELD_EX(px,         ::nkentseu::ecs::reflect::NkFieldType::Float32)
    NK_FIELD_EX(py,         ::nkentseu::ecs::reflect::NkFieldType::Float32)
    NK_FIELD_EX(pz,         ::nkentseu::ecs::reflect::NkFieldType::Float32)
    NK_FIELD_EX(scale,      ::nkentseu::ecs::reflect::NkFieldType::Float32)
    NK_FIELD_EX(nbChildren, ::nkentseu::ecs::reflect::NkFieldType::UInt32)
    NK_FIELD_EX(alive,      ::nkentseu::ecs::reflect::NkFieldType::Bool)
NK_REFLECT_END(Transform)

// =============================================================================
// TEST 1 : mapping NkFieldType -> NkTypeCategory
// =============================================================================
static void TestMapping() {
    printf("[TEST] FieldType -> TypeCategory mapping\n");
    using C = ::nkentseu::reflection::NkTypeCategory;
    EXPECT_TRUE(FieldTypeToCategory(NkFieldType::Float32) == C::NK_FLOAT32);
    EXPECT_TRUE(FieldTypeToCategory(NkFieldType::UInt32)  == C::NK_UINT32);
    EXPECT_TRUE(FieldTypeToCategory(NkFieldType::Bool)    == C::NK_BOOL);
    EXPECT_TRUE(FieldTypeToCategory(NkFieldType::String)  == C::NK_STRING);
    EXPECT_TRUE(FieldTypeToCategory(NkFieldType::Enum)    == C::NK_ENUM);
    EXPECT_TRUE(FieldTypeToCategory(NkFieldType::Vec3)    == C::NK_STRUCT);
    EXPECT_TRUE(FieldTypeIsScalar(NkFieldType::Float32));
    EXPECT_TRUE(!FieldTypeIsScalar(NkFieldType::Vec3));
}

// =============================================================================
// TEST 2 : enregistrement du pont + ComponentMeta branche
// =============================================================================
static void TestRegistration() {
    printf("[TEST] NkRegisterComponentReflection + ComponentMeta hooks\n");

    const reflection::NkClass* cls = NkRegisterComponentReflection<Transform>();
    EXPECT_TRUE(cls != nullptr);
    EXPECT_TRUE(cls->GetPropertyCount() == 6);

    // Idempotence : second appel = meme NkClass.
    const reflection::NkClass* cls2 = NkRegisterComponentReflection<Transform>();
    EXPECT_TRUE(cls == cls2);

    // ComponentMeta doit porter reflectClass + hooks.
    const ComponentMeta* meta = NkMetaOf<Transform>();
    EXPECT_TRUE(meta != nullptr);
    EXPECT_TRUE(meta->reflectClass == cls);
    EXPECT_TRUE(meta->serialize   != nullptr);
    EXPECT_TRUE(meta->deserialize != nullptr);

    // Les proprietes existent par nom.
    EXPECT_TRUE(cls->GetProperty("px")         != nullptr);
    EXPECT_TRUE(cls->GetProperty("scale")      != nullptr);
    EXPECT_TRUE(cls->GetProperty("nbChildren") != nullptr);
    EXPECT_TRUE(cls->GetProperty("alive")      != nullptr);

    // Offsets corrects (acces direct).
    const reflection::NkProperty* pScale = cls->GetProperty("scale");
    EXPECT_TRUE(pScale != nullptr && pScale->GetOffset() == offsetof(Transform, scale));
}

// =============================================================================
// TEST 3 : round-trip via JSON (type-safe)
// =============================================================================
static void TestRoundTrip() {
    printf("[TEST] round-trip composant via JSON (type-safe)\n");

    NkRegisterComponentReflection<Transform>();

    // 1. Instance source.
    Transform src;
    src.px = 1.5f; src.py = -2.5f; src.pz = 10.25f;
    src.scale = 3.0f; src.nbChildren = 42u; src.alive = true;

    // 2. Serialisation -> archive.
    NkArchive ar;
    EXPECT_TRUE(serialization::SerializeComponent(src, ar));
    EXPECT_TRUE(ar.Has("px"));
    EXPECT_TRUE(ar.Has("scale"));
    EXPECT_TRUE(ar.Has("nbChildren"));
    EXPECT_TRUE(ar.Has("alive"));

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

    // 5. Deserialisation -> instance vierge.
    Transform dst;
    dst.px = 0.f; dst.py = 0.f; dst.pz = 0.f;
    dst.scale = 0.f; dst.nbChildren = 0u; dst.alive = false;
    EXPECT_TRUE(serialization::DeserializeComponent(dst, ar2));

    // 6. Verification.
    EXPECT_TRUE(dst.px > 1.49f && dst.px < 1.51f);
    EXPECT_TRUE(dst.py > -2.51f && dst.py < -2.49f);
    EXPECT_TRUE(dst.pz > 10.24f && dst.pz < 10.26f);
    EXPECT_TRUE(dst.scale > 2.99f && dst.scale < 3.01f);
    EXPECT_EQ(dst.nbChildren, 42u);
    EXPECT_TRUE(dst.alive);
}

// =============================================================================
// TEST 4 : round-trip via l'API type-erased (ComponentMeta hooks)
// =============================================================================
static void TestTypeErased() {
    printf("[TEST] round-trip composant via ComponentMeta (type-erased)\n");

    NkRegisterComponentReflection<Transform>();
    const NkComponentId id = NkIdOf<Transform>();
    EXPECT_TRUE(serialization::ComponentHasReflection(id));

    const ComponentMeta* meta = NkTypeRegistry::Global().Get(id);

    Transform src;
    src.px = 7.0f; src.py = 8.0f; src.pz = 9.0f;
    src.scale = 1.25f; src.nbChildren = 3u; src.alive = false;

    NkArchive ar;
    EXPECT_TRUE(serialization::SerializeComponentMeta(meta, &src, ar));

    Transform dst;
    dst.px = 0.f; dst.py = 0.f; dst.pz = 0.f;
    dst.scale = 0.f; dst.nbChildren = 99u; dst.alive = true;
    EXPECT_TRUE(serialization::DeserializeComponentMeta(meta, &dst, ar));

    EXPECT_TRUE(dst.px > 6.99f && dst.px < 7.01f);
    EXPECT_TRUE(dst.scale > 1.24f && dst.scale < 1.26f);
    EXPECT_EQ(dst.nbChildren, 3u);
    EXPECT_TRUE(!dst.alive);
}

// =============================================================================
// main
// =============================================================================
int main() {
    printf("======================================================\n");
    printf("  NKECS Reflection Bridge Test Suite (Phase 4)\n");
    printf("======================================================\n");

    TestMapping();
    TestRegistration();
    TestRoundTrip();
    TestTypeErased();

    printf("======================================================\n");
    printf("  Results: %d passed, %d failed\n", s_pass, s_fail);
    printf("======================================================\n");
    return s_fail > 0 ? 1 : 0;
}
