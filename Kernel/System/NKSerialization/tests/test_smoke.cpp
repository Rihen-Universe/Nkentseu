// =============================================================================
// tests/test_smoke.cpp
// =============================================================================
// Tests fonctionnels standalone (sans framework de test externe).
// =============================================================================
#include <cstdio>
#include <cstring>
#include <cassert>

#include "NKSerialization/NkArchive.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/XML/NkXMLReader.h"
#include "NKSerialization/XML/NkXMLWriter.h"
#include "NKSerialization/YAML/NkYAMLReader.h"
#include "NKSerialization/YAML/NkYAMLWriter.h"
#include "NKSerialization/Binary/NkBinaryReader.h"
#include "NKSerialization/Binary/NkBinaryWriter.h"
#include "NKSerialization/Native/NkNativeFormat.h"
#include "NKSerialization/Asset/NkAssetMetadata.h"

using namespace nkentseu;

// ─── helpers ─────────────────────────────────────────────────────────────────
static int s_pass = 0;
static int s_fail = 0;

#define EXPECT_TRUE(expr) do { if (!(expr)) { \
    printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, #expr); ++s_fail; \
} else { ++s_pass; } } while(0)

#define EXPECT_EQ(a, b) EXPECT_TRUE((a) == (b))
#define EXPECT_STR(a, b) EXPECT_TRUE(strcmp((a),(b)) == 0)

// ─── Test 1 : NkArchive plat ──────────────────────────────────────────────────
static void TestArchiveFlat() {
    printf("[TEST] Archive flat set/get\n");
    NkArchive a;
    EXPECT_TRUE(a.SetString("name",  "Nkentseu"));
    EXPECT_TRUE(a.SetUInt64("build", 20260413ull));
    EXPECT_TRUE(a.SetBool("debug",   true));
    EXPECT_TRUE(a.SetFloat64("scale",1.25));
    EXPECT_TRUE(a.SetInt64("delta",  -99));

    NkString name;   EXPECT_TRUE(a.GetString("name", name));   EXPECT_STR(name.CStr(), "Nkentseu");
    nk_uint64 build; EXPECT_TRUE(a.GetUInt64("build", build)); EXPECT_EQ(build, 20260413ull);
    nk_bool debug;   EXPECT_TRUE(a.GetBool("debug", debug));   EXPECT_TRUE(debug);
    nk_float64 sc;   EXPECT_TRUE(a.GetFloat64("scale", sc));   EXPECT_TRUE(sc > 1.2 && sc < 1.3);
    nk_int64 delta;  EXPECT_TRUE(a.GetInt64("delta", delta));  EXPECT_EQ(delta, -99LL);

    EXPECT_TRUE(!a.GetString("missing", name));
    EXPECT_TRUE(a.Has("name"));
    EXPECT_TRUE(!a.Has("missing"));
    EXPECT_TRUE(a.Remove("debug"));
    EXPECT_TRUE(!a.Has("debug"));
}

// ─── Test 2 : Archive hiérarchique ───────────────────────────────────────────
static void TestArchiveHierarchical() {
    printf("[TEST] Archive hierarchical object/array\n");
    NkArchive root;

    NkArchive child;
    child.SetString("x", "hello");
    child.SetInt64("y", 42);
    root.SetObject("child", child);

    NkArchive out;
    EXPECT_TRUE(root.GetObject("child", out));
    NkString x; EXPECT_TRUE(out.GetString("x", x)); EXPECT_STR(x.CStr(), "hello");
    nk_int64 y; EXPECT_TRUE(out.GetInt64("y", y));  EXPECT_EQ(y, 42LL);

    // Array de scalaires
    NkVector<NkArchiveValue> arr;
    arr.PushBack(NkArchiveValue::FromInt64(1));
    arr.PushBack(NkArchiveValue::FromInt64(2));
    arr.PushBack(NkArchiveValue::FromInt64(3));
    root.SetArray("nums", arr);

    NkVector<NkArchiveValue> got;
    EXPECT_TRUE(root.GetArray("nums", got));
    EXPECT_EQ(got.Size(), 3u);
    nk_int64 v1 = 0; EXPECT_TRUE(NkString(got[0].text).ToInt64(v1)); EXPECT_EQ(v1, 1LL);
}

// ─── Test 3 : Chemin hiérarchique ────────────────────────────────────────────
static void TestArchivePath() {
    printf("[TEST] Archive path set/get\n");
    NkArchive root;
    NkArchiveNode node(NkArchiveValue::FromString("deep_value"));
    EXPECT_TRUE(root.SetPath("a.b.c", node));

    const NkArchiveNode* found = root.GetPath("a.b.c");
    EXPECT_TRUE(found != nullptr);
    EXPECT_TRUE(found->IsScalar());
    EXPECT_STR(found->value.text.CStr(), "deep_value");

    // Meta-données
    EXPECT_TRUE(root.SetMeta("author", "Nkentseu Engine"));
    NkString author;
    EXPECT_TRUE(root.GetMeta("author", author));
    EXPECT_STR(author.CStr(), "Nkentseu Engine");
}

// ─── Test 4 : JSON round-trip ─────────────────────────────────────────────────
static void TestJSONRoundTrip() {
    printf("[TEST] JSON round-trip (flat)\n");
    NkArchive src;
    src.SetString("name",  "Nkentseu");
    src.SetUInt64("build", 20260413ull);
    src.SetBool("ok",      true);
    src.SetFloat64("pi",   3.14159265358979);

    NkString json;
    EXPECT_TRUE(NkJSONWriter::WriteArchive(src, json, true));
    EXPECT_TRUE(!json.Empty());

    NkArchive dst;
    NkString err;
    EXPECT_TRUE(NkJSONReader::ReadArchive(json.View(), dst, &err));
    EXPECT_TRUE(err.Empty());

    NkString name; dst.GetString("name", name); EXPECT_STR(name.CStr(), "Nkentseu");
    nk_uint64 b = 0; dst.GetUInt64("build", b); EXPECT_EQ(b, 20260413ull);
    nk_bool ok = false; dst.GetBool("ok", ok); EXPECT_TRUE(ok);
    nk_float64 pi = 0; dst.GetFloat64("pi", pi); EXPECT_TRUE(pi > 3.1415 && pi < 3.1416);
}

// ─── Test 5 : JSON nested round-trip ─────────────────────────────────────────
static void TestJSONNested() {
    printf("[TEST] JSON round-trip (nested)\n");
    NkArchive src;
    NkArchive inner;
    inner.SetString("key", "value");
    inner.SetInt64("num", 777);
    src.SetObject("inner", inner);
    src.SetString("top", "level");

    NkString json;
    EXPECT_TRUE(NkJSONWriter::WriteArchive(src, json, true));

    NkArchive dst;
    NkString err;
    EXPECT_TRUE(NkJSONReader::ReadArchive(json.View(), dst, &err));
    if (!err.Empty()) printf("    err: %s\n", err.CStr());

    NkString top; dst.GetString("top", top); EXPECT_STR(top.CStr(), "level");
    NkArchive dstInner; EXPECT_TRUE(dst.GetObject("inner", dstInner));
    NkString key; dstInner.GetString("key", key); EXPECT_STR(key.CStr(), "value");
    nk_int64 num = 0; dstInner.GetInt64("num", num); EXPECT_EQ(num, 777LL);
}

// ─── Test 6 : XML round-trip ──────────────────────────────────────────────────
static void TestXMLRoundTrip() {
    printf("[TEST] XML round-trip\n");
    NkArchive src;
    src.SetString("title", "Test XML");
    src.SetInt64("value", -123);

    NkString xml;
    EXPECT_TRUE(NkXMLWriter::WriteArchive(src, xml, true));

    NkArchive dst;
    NkString err;
    EXPECT_TRUE(NkXMLReader::ReadArchive(xml.View(), dst, &err));

    NkString title; dst.GetString("title", title); EXPECT_STR(title.CStr(), "Test XML");
    nk_int64 v = 0; dst.GetInt64("value", v); EXPECT_EQ(v, -123LL);
}

// ─── Test 7 : YAML round-trip ─────────────────────────────────────────────────
static void TestYAMLRoundTrip() {
    printf("[TEST] YAML round-trip\n");
    NkArchive src;
    src.SetString("name", "engine");
    src.SetUInt64("version", 2026u);
    src.SetBool("release", false);

    NkString yaml;
    EXPECT_TRUE(NkYAMLWriter::WriteArchive(src, yaml));

    NkArchive dst;
    NkString err;
    EXPECT_TRUE(NkYAMLReader::ReadArchive(yaml.View(), dst, &err));

    NkString name; dst.GetString("name", name); EXPECT_STR(name.CStr(), "engine");
    nk_uint64 v = 0; dst.GetUInt64("version", v); EXPECT_EQ(v, 2026ull);
    nk_bool rel = true; dst.GetBool("release", rel); EXPECT_TRUE(!rel);
}

// ─── Test 8 : Binary round-trip ───────────────────────────────────────────────
static void TestBinaryRoundTrip() {
    printf("[TEST] Binary round-trip\n");
    NkArchive src;
    src.SetInt64("a", -42);
    src.SetString("b", "hello binary");

    NkVector<nk_uint8> bytes;
    EXPECT_TRUE(NkBinaryWriter::WriteArchive(src, bytes));
    EXPECT_TRUE(bytes.Size() > 0u);

    NkArchive dst;
    NkString err;
    EXPECT_TRUE(NkBinaryReader::ReadArchive(bytes.Data(), bytes.Size(), dst, &err));
    EXPECT_TRUE(err.Empty());

    nk_int64 a = 0; dst.GetInt64("a", a); EXPECT_EQ(a, -42LL);
    NkString b; dst.GetString("b", b); EXPECT_STR(b.CStr(), "hello binary");
}

// ─── Test 9 : NkNative round-trip ────────────────────────────────────────────
static void TestNativeRoundTrip() {
    printf("[TEST] NkNative round-trip\n");
    NkArchive src;
    src.SetBool("flag", true);
    src.SetInt64("count", 1234567890LL);
    src.SetFloat64("ratio", 0.123456789012345);
    src.SetString("label", "native test");
    src.SetUInt64("big", 0xDEADBEEFCAFEBABEull);

    NkVector<nk_uint8> data;
    EXPECT_TRUE(native::NkNativeWriter::WriteArchive(src, data));
    EXPECT_TRUE(data.Size() >= 12u);

    NkArchive dst;
    NkString err;
    EXPECT_TRUE(native::NkNativeReader::ReadArchive(data.Data(), data.Size(), dst, &err));
    if (!err.Empty()) printf("    err: %s\n", err.CStr());

    nk_bool flag = false; dst.GetBool("flag", flag); EXPECT_TRUE(flag);
    nk_int64 cnt = 0; dst.GetInt64("count", cnt); EXPECT_EQ(cnt, 1234567890LL);
    nk_float64 ratio = 0; dst.GetFloat64("ratio", ratio); EXPECT_TRUE(ratio > 0.12 && ratio < 0.13);
    NkString lbl; dst.GetString("label", lbl); EXPECT_STR(lbl.CStr(), "native test");
    nk_uint64 big = 0; dst.GetUInt64("big", big); EXPECT_EQ(big, 0xDEADBEEFCAFEBABEull);
}

// ─── Test 10 : NkNative nested ───────────────────────────────────────────────
static void TestNativeNested() {
    printf("[TEST] NkNative nested object\n");
    NkArchive root;
    NkArchive child;
    child.SetString("name", "child_node");
    child.SetUInt64("id", 42ull);
    root.SetObject("child", child);
    root.SetString("root_key", "root_val");

    NkVector<nk_uint8> data;
    EXPECT_TRUE(native::NkNativeWriter::WriteArchive(root, data));

    NkArchive out;
    NkString err;
    EXPECT_TRUE(native::NkNativeReader::ReadArchive(data.Data(), data.Size(), out, &err));
    if (!err.Empty()) printf("    err: %s\n", err.CStr());

    NkString rk; out.GetString("root_key", rk); EXPECT_STR(rk.CStr(), "root_val");
    NkArchive childOut; EXPECT_TRUE(out.GetObject("child", childOut));
    NkString cn; childOut.GetString("name", cn); EXPECT_STR(cn.CStr(), "child_node");
    nk_uint64 cid = 0; childOut.GetUInt64("id", cid); EXPECT_EQ(cid, 42ull);
}

// ─── Test 11 : AssetId ───────────────────────────────────────────────────────
static void TestAssetId() {
    printf("[TEST] NkAssetId generate / toString / fromString\n");
    NkAssetId id1 = NkAssetId::Generate();
    NkAssetId id2 = NkAssetId::Generate();
    EXPECT_TRUE(id1.IsValid());
    EXPECT_TRUE(id2.IsValid());
    EXPECT_TRUE(id1 != id2);

    NkString str = id1.ToString();
    EXPECT_TRUE(str.Length() == 35u); // {16chars-16chars}
    EXPECT_TRUE(str[0] == '{');

    NkAssetId parsed = NkAssetId::FromString(str.View());
    EXPECT_TRUE(parsed == id1);
}

// ─── Test 12 : AssetMetadata sérialisation ───────────────────────────────────
static void TestAssetMetadataSerialization() {
    printf("[TEST] NkAssetMetadata serialize/deserialize\n");
    NkAssetMetadata meta;
    meta.id          = NkAssetId::Generate();
    meta.assetPath   = NkAssetPath("/Game/Meshes/Cube");
    meta.type        = NkAssetType::StaticMesh;
    meta.typeName    = NkString("UStaticMesh");
    meta.assetVersion = 3u;
    meta.AddTag("renderable");
    meta.AddTag("lod0");
    meta.properties.SetString("lod_count", "3");
    NkAssetId depId = NkAssetId::Generate();
    meta.AddDependency(depId, "/Game/Materials/Mat0");

    NkArchive archive;
    EXPECT_TRUE(meta.Serialize(archive));

    NkAssetMetadata meta2;
    EXPECT_TRUE(meta2.Deserialize(archive));

    EXPECT_TRUE(meta2.id == meta.id);
    EXPECT_STR(meta2.assetPath.path.CStr(), "/Game/Meshes/Cube");
    EXPECT_STR(meta2.assetPath.name.CStr(), "Cube");
    EXPECT_EQ(meta2.type, NkAssetType::StaticMesh);
    EXPECT_EQ(meta2.assetVersion, 3u);
    EXPECT_EQ(meta2.tags.Size(), 2u);
    EXPECT_TRUE(meta2.HasTag("renderable"));
    EXPECT_TRUE(meta2.HasTag("lod0"));
    EXPECT_EQ(meta2.dependencies.Size(), 1u);
    EXPECT_TRUE(meta2.dependencies[0].id == depId);
    EXPECT_STR(meta2.dependencies[0].path.path.CStr(), "/Game/Materials/Mat0");

    NkString lodCount;
    EXPECT_TRUE(meta2.properties.GetString("lod_count", lodCount));
    EXPECT_STR(lodCount.CStr(), "3");
}

// ─── Test 13 : AssetIO (écriture/lecture fichier) ────────────────────────────
static void TestAssetIO() {
    printf("[TEST] NkAssetIO write/read file\n");

    NkAssetMetadata meta;
    meta.id        = NkAssetId::Generate();
    meta.assetPath = NkAssetPath("/Game/Textures/Albedo");
    meta.type      = NkAssetType::Texture2D;
    meta.typeName  = NkString("UTexture2D");
    meta.AddTag("albedo");

    // Payload fictif (2 KB de données)
    NkVector<nk_uint8> payload(2048u);
    for (nk_size i = 0; i < 2048u; ++i) payload[i] = static_cast<nk_uint8>(i & 0xFF);

    const char* tmpPath = "/tmp/test_asset.nkasset";
    NkString err;
    EXPECT_TRUE(NkAssetIO::Write(tmpPath, meta, payload.Data(), payload.Size(), &err));
    if (!err.Empty()) printf("    Write err: %s\n", err.CStr());

    // Relire uniquement les métadonnées
    NkAssetMetadata meta2;
    EXPECT_TRUE(NkAssetIO::ReadMetadata(tmpPath, meta2, &err));
    if (!err.Empty()) printf("    ReadMeta err: %s\n", err.CStr());
    EXPECT_TRUE(meta2.id == meta.id);
    EXPECT_STR(meta2.assetPath.path.CStr(), "/Game/Textures/Albedo");
    EXPECT_EQ(meta2.payloadSize, 2048u);

    // Relire le payload
    NkVector<nk_uint8> payload2;
    EXPECT_TRUE(NkAssetIO::ReadPayload(tmpPath, payload2, &err));
    if (!err.Empty()) printf("    ReadPayload err: %s\n", err.CStr());
    EXPECT_EQ(payload2.Size(), 2048u);
    if (payload2.Size() == 2048u) {
        EXPECT_EQ(payload2[0], 0u);
        EXPECT_EQ(payload2[255], 255u);
        EXPECT_EQ(payload2[256], 0u);
    }
}

// ─── Test 14 : AssetRegistry ─────────────────────────────────────────────────
static void TestAssetRegistry() {
    printf("[TEST] NkAssetRegistry save/load\n");

    NkAssetRegistry reg;

    NkAssetRecord r1;
    r1.id        = NkAssetId::Generate();
    r1.assetPath = NkAssetPath("/Game/Meshes/Cube");
    r1.type      = NkAssetType::StaticMesh;
    r1.typeName  = NkString("UStaticMesh");
    r1.diskPath  = NkString("/Content/Meshes/Cube.nkasset");
    reg.Register(r1);

    NkAssetRecord r2;
    r2.id        = NkAssetId::Generate();
    r2.assetPath = NkAssetPath("/Game/Materials/M_Metal");
    r2.type      = NkAssetType::Material;
    r2.typeName  = NkString("UMaterial");
    r2.diskPath  = NkString("/Content/Materials/M_Metal.nkasset");
    reg.Register(r2);

    EXPECT_EQ(reg.Count(), 2u);

    const char* regPath = "/tmp/test_registry.nkreg";
    NkString err;
    EXPECT_TRUE(reg.SaveRegistry(regPath, &err));
    if (!err.Empty()) printf("    Save err: %s\n", err.CStr());

    NkAssetRegistry reg2;
    EXPECT_TRUE(reg2.LoadRegistry(regPath, &err));
    if (!err.Empty()) printf("    Load err: %s\n", err.CStr());

    EXPECT_EQ(reg2.Count(), 2u);

    const NkAssetRecord* found = reg2.FindByPath("/Game/Meshes/Cube");
    EXPECT_TRUE(found != nullptr);
    if (found) {
        EXPECT_TRUE(found->id == r1.id);
        EXPECT_EQ(found->type, NkAssetType::StaticMesh);
    }

    NkVector<const NkAssetRecord*> meshes;
    reg2.GetByType(NkAssetType::StaticMesh, meshes);
    EXPECT_EQ(meshes.Size(), 1u);
}

// ─── Test 15 : Archive Merge ─────────────────────────────────────────────────
static void TestArchiveMerge() {
    printf("[TEST] Archive merge\n");
    NkArchive a, b;
    a.SetString("shared", "from_a");
    a.SetInt64("only_a", 1);
    b.SetString("shared", "from_b");
    b.SetInt64("only_b", 2);

    // Merge avec overwrite=false : shared reste "from_a"
    a.Merge(b, false);
    NkString shared; a.GetString("shared", shared); EXPECT_STR(shared.CStr(), "from_a");
    nk_int64 ob = 0; a.GetInt64("only_b", ob); EXPECT_EQ(ob, 2LL);

    // Merge avec overwrite=true : shared devient "from_b"
    a.Merge(b, true);
    a.GetString("shared", shared); EXPECT_STR(shared.CStr(), "from_b");
}

// =============================================================================
// main
// =============================================================================
int main() {
    printf("======================================================\n");
    printf("  NKSerialization Test Suite\n");
    printf("======================================================\n");

    TestArchiveFlat();
    TestArchiveHierarchical();
    TestArchivePath();
    TestJSONRoundTrip();
    TestJSONNested();
    TestXMLRoundTrip();
    TestYAMLRoundTrip();
    TestBinaryRoundTrip();
    TestNativeRoundTrip();
    TestNativeNested();
    TestAssetId();
    TestAssetMetadataSerialization();
    TestAssetIO();
    TestAssetRegistry();
    TestArchiveMerge();

    printf("======================================================\n");
    printf("  Results: %d passed, %d failed\n", s_pass, s_fail);
    printf("======================================================\n");
    return s_fail > 0 ? 1 : 0;
}
