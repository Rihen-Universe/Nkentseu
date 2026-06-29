// =============================================================================
// GLTFLoaderTest.cpp — test standalone du loader glTF 2.0 (NkGLTFLoader).
//
// Verifie :
//   1) Un .gltf minimal (triangle) avec buffer en data URI base64.
//   2) Le sample reel Resources/Models/rubber_duck/scene.gltf (buffer .bin
//      externe) si present.
//   3) Le sample binaire Resources/Models/car.glb (.glb) si present.
//
// Pas de GPU : on appelle directement LoadGLTF (geometrie CPU pure).
//
// Cible jenga : `gltftest` (voir RendererSandbox.jenga).
// =============================================================================
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"

#include <cstdio>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::renderer;

static int g_fail = 0;

static void Check(bool cond, const char* msg) {
    std::printf("  [%s] %s\n", cond ? "OK " : "FAIL", msg);
    if (!cond) ++g_fail;
}

static bool ApproxEq(float a, float b, float eps = 1e-3f) {
    return std::fabs(a - b) <= eps;
}

// glTF minimal : 1 triangle (3 vertices), POSITION + indices, buffer base64.
// Le buffer binaire contient :
//   - 3 positions VEC3 f32 : (0,0,0) (1,0,0) (0,1,0)  -> 36 octets
//   - 3 indices u16        : 0,1,2                    -> 6 octets (+2 pad)
// Encodage base64 du buffer ci-dessous genere par script (voir commentaire).
static const char* kTriangleGLTF =
"{"
"  \"asset\": { \"version\": \"2.0\" },"
"  \"buffers\": [ {"
"     \"byteLength\": 44,"
"     \"uri\": \"data:application/octet-stream;base64,"
            "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIAAAA=\""
"  } ],"
"  \"bufferViews\": ["
"     { \"buffer\": 0, \"byteOffset\": 0,  \"byteLength\": 36, \"target\": 34962 },"
"     { \"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6,  \"target\": 34963 }"
"  ],"
"  \"accessors\": ["
"     { \"bufferView\": 0, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\","
"       \"min\": [0,0,0], \"max\": [1,1,0] },"
"     { \"bufferView\": 1, \"componentType\": 5123, \"count\": 3, \"type\": \"SCALAR\" }"
"  ],"
"  \"meshes\": [ { \"name\": \"tri\", \"primitives\": [ {"
"       \"attributes\": { \"POSITION\": 0 },"
"       \"indices\": 1, \"mode\": 4"
"  } ] } ]"
"}";

static void TestTriangleDataURI() {
    std::printf("== Test 1 : triangle .gltf (data URI base64) ==\n");

    // Ecrit le .gltf dans le dossier temp pour avoir un vrai path fichier.
    NkPath tmp = NkPath::GetTempDirectory();
    NkString path = tmp.ToString();
    if (!path.Empty() && !path.EndsWith('/') && !path.EndsWith('\\')) path.Append('/');
    path.Append("nk_gltf_triangle_test.gltf");

    if (!NkFile::WriteAllText(path.CStr(), kTriangleGLTF)) {
        Check(false, "ecriture du .gltf temporaire");
        return;
    }

    NkGLTFMeshData data;
    bool ok = LoadGLTF(path, data);
    Check(ok, "LoadGLTF retourne true");
    if (!ok) { NkFile::Delete(path.CStr()); return; }

    Check(data.vertices.Size() == 3, "3 vertices");
    Check(data.indices.Size() == 3, "3 indices");
    Check(data.subMeshes.Size() == 1, "1 submesh");

    if (data.vertices.Size() == 3) {
        const NkVertex3D& v0 = data.vertices[0];
        const NkVertex3D& v1 = data.vertices[1];
        const NkVertex3D& v2 = data.vertices[2];
        Check(ApproxEq(v0.pos.x, 0.f) && ApproxEq(v0.pos.y, 0.f) && ApproxEq(v0.pos.z, 0.f),
              "v0 == (0,0,0)");
        Check(ApproxEq(v1.pos.x, 1.f) && ApproxEq(v1.pos.y, 0.f) && ApproxEq(v1.pos.z, 0.f),
              "v1 == (1,0,0)");
        Check(ApproxEq(v2.pos.x, 0.f) && ApproxEq(v2.pos.y, 1.f) && ApproxEq(v2.pos.z, 0.f),
              "v2 == (0,1,0)");
        // NORMAL absente -> calculee par-face : (0,0,1) pour ce triangle CCW XY.
        Check(ApproxEq(v0.normal.z, 1.f) || ApproxEq(v0.normal.z, -1.f),
              "normale calculee (|z|=1) car NORMAL absente");
        // COLOR absente -> blanc.
        Check(v0.color == 0xFFFFFFFFu, "couleur defaut = blanc");
    }
    if (data.indices.Size() == 3) {
        Check(data.indices[0] == 0 && data.indices[1] == 1 && data.indices[2] == 2,
              "indices == [0,1,2]");
    }
    Check(ApproxEq(data.bounds.min.x, 0.f) && ApproxEq(data.bounds.min.y, 0.f)
       && ApproxEq(data.bounds.max.x, 1.f) && ApproxEq(data.bounds.max.y, 1.f),
          "bounds = ([0,0,0]..[1,1,0])");

    NkFile::Delete(path.CStr());
}

static void TestRealSample(const char* relPath, const char* label) {
    std::printf("== Test : %s ==\n", label);
    if (!NkFile::Exists(relPath)) {
        std::printf("  [SKIP] introuvable : %s\n", relPath);
        return;
    }
    NkGLTFMeshData data;
    bool ok = LoadGLTF(NkString(relPath), data);
    Check(ok, "LoadGLTF retourne true");
    if (!ok) return;
    Check(data.vertices.Size() > 0, "vertices > 0");
    Check(data.indices.Size() > 0, "indices > 0");
    Check(data.subMeshes.Size() > 0, "submeshes > 0");
    // bounds non degenere
    bool boundsOk = data.bounds.max.x >= data.bounds.min.x
                 && data.bounds.max.y >= data.bounds.min.y
                 && data.bounds.max.z >= data.bounds.min.z;
    Check(boundsOk, "bounds valide (max >= min)");
    std::printf("    -> %u vertices, %u indices, %u submeshes, bounds=[%.3f,%.3f,%.3f]..[%.3f,%.3f,%.3f]\n",
                (unsigned)data.vertices.Size(), (unsigned)data.indices.Size(),
                (unsigned)data.subMeshes.Size(),
                data.bounds.min.x, data.bounds.min.y, data.bounds.min.z,
                data.bounds.max.x, data.bounds.max.y, data.bounds.max.z);
}

// Construit un .glb triangle valide en memoire (header + chunk JSON + chunk BIN)
// pour exercer le chemin binaire .glb (buffer[0] sans uri -> chunk BIN).
static void PushU32LE(NkVector<uint8>& v, uint32 x) {
    v.PushBack((uint8)(x & 0xFF));
    v.PushBack((uint8)((x >> 8) & 0xFF));
    v.PushBack((uint8)((x >> 16) & 0xFF));
    v.PushBack((uint8)((x >> 24) & 0xFF));
}

static void TestTriangleGLB() {
    std::printf("== Test 2 : triangle .glb (binaire, chunk BIN) ==\n");

    // BIN : 3 positions VEC3 f32 + 3 indices u16 (+2 pad -> aligne 4).
    NkVector<uint8> bin;
    float pos[9] = {0,0,0, 2,0,0, 0,2,0};
    for (int i = 0; i < 9; ++i) {
        uint8 b[4]; std::memcpy(b, &pos[i], 4);
        for (int k = 0; k < 4; ++k) bin.PushBack(b[k]);
    }
    uint16 idx[3] = {0,1,2};
    for (int i = 0; i < 3; ++i) {
        uint8 b[2]; std::memcpy(b, &idx[i], 2);
        bin.PushBack(b[0]); bin.PushBack(b[1]);
    }
    while ((bin.Size() & 3u) != 0) bin.PushBack(0); // pad chunk a 4 octets

    NkString json =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":42}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36,\"target\":34962},"
        "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6,\"target\":34963}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
        "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
        "\"meshes\":[{\"name\":\"glbtri\",\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1,\"mode\":4}]}]}";
    NkVector<uint8> jsonBytes;
    for (nk_size i = 0; i < json.Length(); ++i) jsonBytes.PushBack((uint8)json.CStr()[i]);
    while ((jsonBytes.Size() & 3u) != 0) jsonBytes.PushBack((uint8)' '); // pad espaces

    NkVector<uint8> glb;
    uint32 total = 12 + 8 + (uint32)jsonBytes.Size() + 8 + (uint32)bin.Size();
    PushU32LE(glb, 0x46546C67u);   // 'glTF'
    PushU32LE(glb, 2u);            // version
    PushU32LE(glb, total);         // length
    PushU32LE(glb, (uint32)jsonBytes.Size());
    PushU32LE(glb, 0x4E4F534Au);   // 'JSON'
    for (nk_size i = 0; i < jsonBytes.Size(); ++i) glb.PushBack(jsonBytes[i]);
    PushU32LE(glb, (uint32)bin.Size());
    PushU32LE(glb, 0x004E4942u);   // 'BIN\0'
    for (nk_size i = 0; i < bin.Size(); ++i) glb.PushBack(bin[i]);

    NkPath tmp = NkPath::GetTempDirectory();
    NkString path = tmp.ToString();
    if (!path.Empty() && !path.EndsWith('/') && !path.EndsWith('\\')) path.Append('/');
    path.Append("nk_gltf_triangle_test.glb");
    if (!NkFile::WriteAllBytes(path.CStr(), glb)) {
        Check(false, "ecriture du .glb temporaire");
        return;
    }

    NkGLTFMeshData data;
    bool ok = LoadGLTF(path, data);
    Check(ok, "LoadGLTF(.glb) retourne true");
    if (ok) {
        Check(data.vertices.Size() == 3, "3 vertices");
        Check(data.indices.Size() == 3, "3 indices");
        if (data.vertices.Size() == 3) {
            Check(ApproxEq(data.vertices[1].pos.x, 2.f), "v1.x == 2 (decode BIN)");
            Check(ApproxEq(data.vertices[2].pos.y, 2.f), "v2.y == 2 (decode BIN)");
        }
        Check(ApproxEq(data.bounds.max.x, 2.f) && ApproxEq(data.bounds.max.y, 2.f),
              "bounds max == (2,2,0)");
    }
    NkFile::Delete(path.CStr());
}

int main(int /*argc*/, char** /*argv*/) {
    std::printf("\n=== NkGLTFLoader test ===\n");

    TestTriangleDataURI();
    TestTriangleGLB();

    // Sample reel : resolu relativement au cwd. Jenga lance depuis la racine du
    // repo en general ; on tente aussi un prefixe relatif au binaire.
    const char* duck1 = "Resources/Models/rubber_duck/scene.gltf";
    const char* duck2 = "../../Resources/Models/rubber_duck/scene.gltf";
    TestRealSample(NkFile::Exists(duck1) ? duck1 : duck2, "rubber_duck/scene.gltf (.bin externe)");

    // Note : Resources/Models/car.glb est un placeholder vide (primitives [{}]
    // sans accessors) -> le loader le rejette volontairement. Pas teste ici.

    std::printf("\n=== Resultat : %s (%d echec(s)) ===\n",
                g_fail == 0 ? "SUCCES" : "ECHEC", g_fail);
    return g_fail == 0 ? 0 : 1;
}
