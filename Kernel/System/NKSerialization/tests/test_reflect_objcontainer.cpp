// =============================================================================
// tests/test_reflect_objcontainer.cpp
// =============================================================================
// Finition Phase 3 (report) : conteneurs d'OBJETS reflechis.
//   - NkArchive::SetObjectArray (ajoute en complement de GetObjectArray).
//   - NkReflectSerializer : une propriete NkVector<SousObjet NK_CLASS> est
//     serialisee en object-array (chaque element = sous-objet recursif) et
//     re-desserialisee a l'identique.
//
// Round-trip : Polygon { NkString name; NkVector<Point{x,y}> points; } ->
// archive -> JSON -> archive reparsee -> Polygon vierge, verifie egalite.
//
// Standalone, zero-STL cote moteur.
// =============================================================================

#include <cstdio>

#include "NKReflection/NkRegistry.h"
#include "NKSerialization/NkArchive.h"
#include "NKSerialization/Reflection/NkReflectSerializer.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/JSON/NkJSONReader.h"

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

using namespace nkentseu;
using namespace nkentseu::reflection;

static int s_pass = 0;
static int s_fail = 0;
#define EXPECT_TRUE(expr) do { if (!(expr)) { \
    printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, #expr); ++s_fail; \
} else { ++s_pass; } } while(0)

// =============================================================================
// SOUS-OBJET REFLECHI : Point { x, y }
// =============================================================================
struct Point {
    NKENTSEU_REFLECT_CLASS(Point)
public:
    NKENTSEU_PROPERTY(nk_float32, x)
public:
    NKENTSEU_PROPERTY(nk_float32, y)
public:
};

// =============================================================================
// OBJET CONTENEUR : Polygon { name; NkVector<Point> points }
// =============================================================================
struct Polygon {
    NKENTSEU_REFLECT_CLASS(Polygon)
public:
    NKENTSEU_PROPERTY(NkString, name)
public:
    NKENTSEU_PROPERTY(NkVector<Point>, points)
public:
};

// =============================================================================
// TEST 1 : NkArchive SetObjectArray / GetObjectArray (round-trip direct)
// =============================================================================
static void TestArchiveObjectArray() {
    printf("[TEST] NkArchive::SetObjectArray / GetObjectArray\n");

    NkVector<NkArchive> arr;
    for (int i = 0; i < 3; ++i) {
        NkArchive o;
        o.SetInt32(NkStringView("v"), i * 10);
        arr.PushBack(o);
    }

    NkArchive root;
    EXPECT_TRUE(root.SetObjectArray(NkStringView("items"), arr));

    NkVector<NkArchive> out;
    EXPECT_TRUE(root.GetObjectArray(NkStringView("items"), out));
    EXPECT_TRUE(out.Size() == 3);
    for (nk_size i = 0; i < out.Size(); ++i) {
        nk_int32 v = -1;
        EXPECT_TRUE(out[i].GetInt32(NkStringView("v"), v));
        EXPECT_TRUE(v == static_cast<nk_int32>(i) * 10);
    }
}

// =============================================================================
// TEST 2 : round-trip d'un conteneur d'objets reflechis via JSON
// =============================================================================
static void TestObjectContainerRoundTrip() {
    printf("[TEST] NkVector<Point> round-trip (object-array via reflexion)\n");

    // Force l'enregistrement des NkClass (auto-link via GetStaticClass).
    (void)Point::GetStaticClass();
    (void)Polygon::GetStaticClass();

    Polygon src;
    src.name = NkString("triangle");
    Point p0; p0.x = 0.0f;  p0.y = 0.0f;
    Point p1; p1.x = 10.0f; p1.y = 0.0f;
    Point p2; p2.x = 5.0f;  p2.y = 8.0f;
    src.points.PushBack(p0);
    src.points.PushBack(p1);
    src.points.PushBack(p2);

    // Serialisation -> archive.
    NkArchive ar;
    EXPECT_TRUE(NkReflectSerializer::SerializeReflected(&Polygon::GetStaticClass(), &src, ar));
    EXPECT_TRUE(ar.Has("name"));
    EXPECT_TRUE(ar.Has("points"));

    // L'entree "points" doit etre un tableau d'objets.
    NkVector<NkArchive> rawPts;
    EXPECT_TRUE(ar.GetObjectArray(NkStringView("points"), rawPts));
    EXPECT_TRUE(rawPts.Size() == 3);

    // Archive -> JSON -> archive (round-trip texte complet).
    NkString json;
    EXPECT_TRUE(NkJSONWriter::WriteArchive(ar, json, true));
    EXPECT_TRUE(!json.Empty());
    printf("    JSON:\n%s\n", json.CStr());

    NkArchive ar2;
    NkString err;
    EXPECT_TRUE(NkJSONReader::ReadArchive(json.View(), ar2, &err));
    if (!err.Empty()) printf("    parse err: %s\n", err.CStr());

    // Deserialisation -> Polygon vierge.
    Polygon dst;
    EXPECT_TRUE(NkReflectSerializer::DeserializeReflected(&Polygon::GetStaticClass(), &dst, ar2));

    // Verification.
    EXPECT_TRUE(dst.name == NkString("triangle"));
    EXPECT_TRUE(dst.points.Size() == 3);
    if (dst.points.Size() == 3) {
        EXPECT_TRUE(dst.points[0].x > -0.01f && dst.points[0].x < 0.01f);
        EXPECT_TRUE(dst.points[1].x > 9.99f && dst.points[1].x < 10.01f);
        EXPECT_TRUE(dst.points[2].x > 4.99f && dst.points[2].x < 5.01f);
        EXPECT_TRUE(dst.points[2].y > 7.99f && dst.points[2].y < 8.01f);
    }
}

int main() {
    printf("======================================================\n");
    printf("  NKSerialization - Conteneurs d'objets reflechis\n");
    printf("======================================================\n");

    TestArchiveObjectArray();
    TestObjectContainerRoundTrip();

    printf("======================================================\n");
    printf("  Results: %d passed, %d failed\n", s_pass, s_fail);
    printf("======================================================\n");
    return s_fail > 0 ? 1 : 0;
}
