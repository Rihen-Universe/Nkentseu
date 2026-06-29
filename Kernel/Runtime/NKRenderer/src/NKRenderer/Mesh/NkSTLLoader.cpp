// =============================================================================
// NkSTLLoader.cpp — NKRenderer (Mesh/) — STL binaire + ASCII, zero-STL.
// =============================================================================
#include "NkSTLLoader.h"
#include "NkMeshLoaderUtil.h"

#include "NKLogger/NkLog.h"
#include "NKFileSystem/NkFile.h"

namespace nkentseu {
    namespace renderer {

        namespace {
            using namespace meshutil;

            void EmitTri(NkGLTFMeshData& out, const NkVec3f& a, const NkVec3f& b,
                         const NkVec3f& c, NkVec3f n) {
                float32 len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
                if (len < 1e-8f) {                       // normale absente -> calcul
                    NkVec3f e1 = {b.x - a.x, b.y - a.y, b.z - a.z};
                    NkVec3f e2 = {c.x - a.x, c.y - a.y, c.z - a.z};
                    n = {e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
                    len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
                }
                if (len > 1e-8f) { n.x /= len; n.y /= len; n.z /= len; } else n = {0.f, 1.f, 0.f};

                uint32 white = PackRGBA(255, 255, 255);
                const NkVec3f tri[3] = {a, b, c};
                for (int32 i = 0; i < 3; ++i) {
                    NkVertex3D v{};
                    v.pos = tri[i]; v.normal = n; v.tangent = {0.f, 0.f, 0.f};
                    v.uv = {0.f, 0.f}; v.uv2 = {0.f, 0.f}; v.color = white;
                    out.indices.PushBack((uint32)out.vertices.Size());
                    out.vertices.PushBack(v);
                }
            }

            bool LoadBinary(const NkVector<nk_uint8>& buf, NkGLTFMeshData& out) {
                if (buf.Size() < 84) return false;
                const uint8* d = buf.Data();
                uint32 n = ReadU32LE(d + 80);
                if ((uint64)84 + (uint64)50 * n != (uint64)buf.Size()) return false; // pas binaire
                const uint8* t = d + 84;
                for (uint32 i = 0; i < n; ++i, t += 50) {
                    NkVec3f nrm = {ReadF32LE(t),      ReadF32LE(t + 4),  ReadF32LE(t + 8)};
                    NkVec3f a   = {ReadF32LE(t + 12), ReadF32LE(t + 16), ReadF32LE(t + 20)};
                    NkVec3f b   = {ReadF32LE(t + 24), ReadF32LE(t + 28), ReadF32LE(t + 32)};
                    NkVec3f c   = {ReadF32LE(t + 36), ReadF32LE(t + 40), ReadF32LE(t + 44)};
                    EmitTri(out, a, b, c, nrm);
                }
                return true;
            }

            bool LoadAscii(const char* p, const char* end, NkGLTFMeshData& out) {
                NkVec3f n{}, vtx[3]; int32 vcount = 0;
                NkString tok;
                while (p < end) {
                    SkipWS(p, end);
                    if (p >= end) break;
                    ReadToken(p, end, tok);
                    if (tok == NkString("facet")) {
                        ReadToken(p, end, tok);           // "normal"
                        n.x = ParseFloat(p, end); n.y = ParseFloat(p, end); n.z = ParseFloat(p, end);
                        vcount = 0;
                    } else if (tok == NkString("vertex")) {
                        if (vcount < 3) {
                            vtx[vcount].x = ParseFloat(p, end);
                            vtx[vcount].y = ParseFloat(p, end);
                            vtx[vcount].z = ParseFloat(p, end);
                            ++vcount;
                        }
                    } else if (tok == NkString("endfacet")) {
                        if (vcount == 3) EmitTri(out, vtx[0], vtx[1], vtx[2], n);
                    } else {
                        SkipToEOL(p, end);                // solid/outer/loop/endloop/endsolid/...
                    }
                }
                return !out.vertices.Empty();
            }
        } // namespace

        bool LoadSTL(const NkString& path, NkGLTFMeshData& out) {
            NkVector<nk_uint8> buf = NkFile::ReadAllBytes(path.CStr());
            if (buf.Empty()) {
                NkLog::Instance().Warnf("[NkSTLLoader] fichier introuvable/vide : %s", path.CStr());
                return false;
            }
            // Detection binaire par taille exacte (robuste meme si header = "solid").
            bool isBinary = false;
            if (buf.Size() >= 84) {
                uint32 n = ReadU32LE(buf.Data() + 80);
                if ((uint64)84 + (uint64)50 * n == (uint64)buf.Size()) isBinary = true;
            }

            bool ok = false;
            if (isBinary) ok = LoadBinary(buf, out);
            else          ok = LoadAscii((const char*)buf.Data(), (const char*)buf.Data() + buf.Size(), out);

            if (!ok || out.vertices.Empty()) {
                NkLog::Instance().Warnf("[NkSTLLoader] aucune geometrie dans %s", path.CStr());
                return false;
            }
            NkSubMesh sm; sm.firstIndex = 0; sm.indexCount = (uint32)out.indices.Size();
            out.subMeshes.PushBack(sm); out.subMeshMaterial.PushBack(-1);
            ComputeBounds(out);
            out.debugName = path;
            NkLog::Instance().Infof("[NkSTLLoader] OK '%s' (%s) : %u triangles, %u verts",
                                    path.CStr(), isBinary ? "binaire" : "ascii",
                                    (uint32)out.indices.Size() / 3, (uint32)out.vertices.Size());
            return true;
        }

    } // namespace renderer
} // namespace nkentseu
