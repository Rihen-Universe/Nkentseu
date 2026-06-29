// =============================================================================
// NkFBXLoader.cpp — NKRenderer (Mesh/) — FBX binaire (sous-ensemble geometrie).
// Voir NkFBXLoader.h. Zero-STL. Inflate des arrays via NkDeflate (NKImage).
// =============================================================================
#include "NkFBXLoader.h"
#include "NkMeshLoaderUtil.h"

#include "NKLogger/NkLog.h"
#include "NKFileSystem/NkFile.h"
#include "NKImage/Core/NkImage.h"        // NkDeflate::Decompress (zlib)
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace renderer {

        namespace {
            using namespace meshutil;

            struct FbxProp {
                char    type = 0;
                float64 scalar = 0.0;
                NkVector<float64> arrF;   // arrays f/d (en f64)
                NkVector<int64>   arrI;   // arrays i/l/b
                NkString          str;    // S/R
            };
            struct FbxNode {
                NkString name;
                NkVector<FbxProp> props;
                NkVector<FbxNode> children;
                const FbxNode* Find(const char* n) const {
                    for (uint32 i = 0; i < (uint32)children.Size(); ++i)
                        if (children[(NkVector<FbxNode>::SizeType)i].name == NkString(n))
                            return &children[(NkVector<FbxNode>::SizeType)i];
                    return nullptr;
                }
            };

            struct Cur { const uint8* p; const uint8* end; bool ok = true; };
            uint8  RU8 (Cur& c) { if (c.p >= c.end) { c.ok = false; return 0; } return *c.p++; }
            uint32 RU32(Cur& c) { if (c.p + 4 > c.end) { c.ok = false; return 0; } uint32 v = ReadU32LE(c.p); c.p += 4; return v; }
            uint64 RU64(Cur& c) { if (c.p + 8 > c.end) { c.ok = false; return 0; } uint64 v = ReadU64LE(c.p); c.p += 8; return v; }

            bool ReadProp(Cur& c, FbxProp& pr) {
                char t = (char)RU8(c); pr.type = t;
                switch (t) {
                    case 'Y': { if (c.p + 2 > c.end) { c.ok = false; return false; } pr.scalar = (float64)(int16)ReadU16LE(c.p); c.p += 2; } break;
                    case 'C': { pr.scalar = (float64)RU8(c); } break;
                    case 'I': { pr.scalar = (float64)(int32)RU32(c); } break;
                    case 'F': { if (c.p + 4 > c.end) { c.ok = false; return false; } pr.scalar = (float64)ReadF32LE(c.p); c.p += 4; } break;
                    case 'D': { if (c.p + 8 > c.end) { c.ok = false; return false; } pr.scalar = ReadF64LE(c.p); c.p += 8; } break;
                    case 'L': { pr.scalar = (float64)(int64)RU64(c); } break;
                    case 'S': case 'R': {
                        uint32 len = RU32(c);
                        if (c.p + len > c.end) { c.ok = false; return false; }
                        if (t == 'S') for (uint32 i = 0; i < len; ++i) pr.str.Append((char)c.p[i]);
                        c.p += len;
                    } break;
                    case 'f': case 'd': case 'l': case 'i': case 'b': {
                        uint32 arrLen = RU32(c); uint32 enc = RU32(c); uint32 cmpLen = RU32(c);
                        if (!c.ok) return false;
                        uint32 elemSz = (t == 'd' || t == 'l') ? 8 : (t == 'b') ? 1 : 4;
                        uint64 rawSz  = (uint64)arrLen * elemSz;
                        const uint8* data = nullptr; NkVector<uint8> tmp;
                        if (enc == 1) {
                            if (c.p + cmpLen > c.end) { c.ok = false; return false; }
                            tmp.Resize((NkVector<uint8>::SizeType)rawSz);
                            usize written = 0;
                            if (!NkDeflate::Decompress(c.p, cmpLen, tmp.Data(), (usize)rawSz, written)
                                || written < rawSz) { c.ok = false; return false; }
                            c.p += cmpLen; data = tmp.Data();
                        } else {
                            if (c.p + rawSz > c.end) { c.ok = false; return false; }
                            data = c.p; c.p += rawSz;
                        }
                        const uint8* d = data;
                        for (uint32 i = 0; i < arrLen; ++i) {
                            if      (t == 'f') { pr.arrF.PushBack((float64)ReadF32LE(d)); d += 4; }
                            else if (t == 'd') { pr.arrF.PushBack(ReadF64LE(d)); d += 8; }
                            else if (t == 'i') { pr.arrI.PushBack((int64)(int32)ReadU32LE(d)); d += 4; }
                            else if (t == 'l') { pr.arrI.PushBack((int64)ReadU64LE(d)); d += 8; }
                            else               { pr.arrI.PushBack((int64)d[0]); d += 1; }
                        }
                    } break;
                    default: c.ok = false; return false;
                }
                return c.ok;
            }

            bool ParseNode(Cur& c, const uint8* base, bool is64, FbxNode& node, bool& isNull) {
                isNull = false;
                uint64 endOff   = is64 ? RU64(c) : (uint64)RU32(c);
                uint64 numProps = is64 ? RU64(c) : (uint64)RU32(c);
                uint64 propLen  = is64 ? RU64(c) : (uint64)RU32(c); (void)propLen;
                uint8  nameLen  = RU8(c);
                if (!c.ok) return false;
                if (endOff == 0 && numProps == 0 && nameLen == 0) { isNull = true; return true; }
                for (uint32 i = 0; i < nameLen; ++i) node.name.Append((char)RU8(c));
                for (uint64 i = 0; i < numProps; ++i) {
                    FbxProp pr;
                    if (!ReadProp(c, pr)) return false;
                    node.props.PushBack(static_cast<FbxProp&&>(pr));
                }
                while (c.p < base + endOff && c.ok) {
                    FbxNode child; bool childNull = false;
                    if (!ParseNode(c, base, is64, child, childNull)) return false;
                    if (childNull) break;
                    node.children.PushBack(static_cast<FbxNode&&>(child));
                }
                if (endOff > 0) c.p = base + endOff;     // realignement
                return c.ok;
            }

            // Premier prop array f64 / i64 d'un noeud.
            const NkVector<float64>* ArrF(const FbxNode* n) {
                if (!n) return nullptr;
                for (uint32 i = 0; i < (uint32)n->props.Size(); ++i)
                    if (!n->props[(NkVector<FbxProp>::SizeType)i].arrF.Empty())
                        return &n->props[(NkVector<FbxProp>::SizeType)i].arrF;
                return nullptr;
            }
            const NkVector<int64>* ArrI(const FbxNode* n) {
                if (!n) return nullptr;
                for (uint32 i = 0; i < (uint32)n->props.Size(); ++i)
                    if (!n->props[(NkVector<FbxProp>::SizeType)i].arrI.Empty())
                        return &n->props[(NkVector<FbxProp>::SizeType)i].arrI;
                return nullptr;
            }
            NkString StrOf(const FbxNode* n) {
                if (n) for (uint32 i = 0; i < (uint32)n->props.Size(); ++i)
                    if (!n->props[(NkVector<FbxProp>::SizeType)i].str.Empty())
                        return n->props[(NkVector<FbxProp>::SizeType)i].str;
                return NkString("");
            }

            // ── Extrait une Geometry FBX dans out (un sous-mesh). ─────────────
            void ExtractGeometry(const FbxNode& geo, NkGLTFMeshData& out) {
                const NkVector<float64>* verts = ArrF(geo.Find("Vertices"));
                const NkVector<int64>*   pvi   = ArrI(geo.Find("PolygonVertexIndex"));
                if (!verts || !pvi || verts->Size() < 3) return;

                // Normales
                const FbxNode* leN = geo.Find("LayerElementNormal");
                const NkVector<float64>* norms = leN ? ArrF(leN->Find("Normals")) : nullptr;
                const NkVector<int64>*   nIdx  = leN ? ArrI(leN->Find("NormalsIndex")) : nullptr;
                NkString nMap = leN ? StrOf(leN->Find("MappingInformationType")) : NkString("");
                NkString nRef = leN ? StrOf(leN->Find("ReferenceInformationType")) : NkString("");
                bool nByPV = !(nMap == NkString("ByControlPoint") || nMap == NkString("ByVertice") || nMap == NkString("ByVertex"));
                bool nIdxToDir = (nRef == NkString("IndexToDirect") || nRef == NkString("Index"));

                // UV
                const FbxNode* leU = geo.Find("LayerElementUV");
                const NkVector<float64>* uvs  = leU ? ArrF(leU->Find("UV")) : nullptr;
                const NkVector<int64>*   uIdx = leU ? ArrI(leU->Find("UVIndex")) : nullptr;
                NkString uMap = leU ? StrOf(leU->Find("MappingInformationType")) : NkString("");
                NkString uRef = leU ? StrOf(leU->Find("ReferenceInformationType")) : NkString("");
                bool uByPV = !(uMap == NkString("ByControlPoint") || uMap == NkString("ByVertice") || uMap == NkString("ByVertex"));
                bool uIdxToDir = (uRef == NkString("IndexToDirect") || uRef == NkString("Index"));

                uint32 subStart = (uint32)out.indices.Size();
                uint32 baseV    = (uint32)out.vertices.Size();
                bool   anyNormal = false;
                uint32 white = PackRGBA(255, 255, 255);

                auto ctrlCount = (int64)(verts->Size() / 3);

                // emet un vertex pour le coin (ctrl, pvPos) et renvoie son index local (depuis baseV)
                auto emit = [&](int64 ctrl, int64 pvPos) -> uint32 {
                    NkVertex3D v{};
                    if (ctrl >= 0 && ctrl < ctrlCount) {
                        v.pos = { (float32)(*verts)[(NkVector<float64>::SizeType)(ctrl * 3 + 0)],
                                  (float32)(*verts)[(NkVector<float64>::SizeType)(ctrl * 3 + 1)],
                                  (float32)(*verts)[(NkVector<float64>::SizeType)(ctrl * 3 + 2)] };
                    }
                    // normale
                    if (norms) {
                        int64 ni = nByPV ? pvPos : ctrl;
                        if (nIdxToDir && nIdx && ni >= 0 && ni < (int64)nIdx->Size())
                            ni = (*nIdx)[(NkVector<int64>::SizeType)ni];
                        if (ni >= 0 && ni * 3 + 2 < (int64)norms->Size()) {
                            v.normal = { (float32)(*norms)[(NkVector<float64>::SizeType)(ni * 3 + 0)],
                                         (float32)(*norms)[(NkVector<float64>::SizeType)(ni * 3 + 1)],
                                         (float32)(*norms)[(NkVector<float64>::SizeType)(ni * 3 + 2)] };
                            anyNormal = true;
                        }
                    }
                    // uv
                    if (uvs) {
                        int64 ui = uByPV ? pvPos : ctrl;
                        if (uIdxToDir && uIdx && ui >= 0 && ui < (int64)uIdx->Size())
                            ui = (*uIdx)[(NkVector<int64>::SizeType)ui];
                        if (ui >= 0 && ui * 2 + 1 < (int64)uvs->Size()) {
                            float32 uu = (float32)(*uvs)[(NkVector<float64>::SizeType)(ui * 2 + 0)];
                            float32 vv = (float32)(*uvs)[(NkVector<float64>::SizeType)(ui * 2 + 1)];
                            v.uv = { uu, 1.f - vv };       // FBX origine bas-gauche -> flip V
                        }
                    }
                    v.tangent = {0.f, 0.f, 0.f}; v.uv2 = v.uv; v.color = white;
                    out.vertices.PushBack(v);
                    return (uint32)out.vertices.Size() - 1;
                };

                // parcourt PolygonVertexIndex, triangulation fan par polygone
                uint32 poly[256]; int32 polyN = 0;
                for (uint32 k = 0; k < (uint32)pvi->Size(); ++k) {
                    int64 raw = (*pvi)[(NkVector<int64>::SizeType)k];
                    bool last = raw < 0;
                    int64 ctrl = last ? (~raw) : raw;     // dernier index = ~idx
                    if (polyN < 256) poly[polyN++] = emit(ctrl, (int64)k);
                    if (last) {
                        for (int32 i = 1; i + 1 < polyN; ++i) {
                            out.indices.PushBack(poly[0] - baseV);
                            out.indices.PushBack(poly[i] - baseV);
                            out.indices.PushBack(poly[i + 1] - baseV);
                        }
                        polyN = 0;
                    }
                }

                uint32 idxCount = (uint32)out.indices.Size() - subStart;
                if (idxCount == 0) return;
                if (!anyNormal) {
                    // normales lissees locales a ce sous-mesh
                    for (uint32 vi = baseV; vi < (uint32)out.vertices.Size(); ++vi)
                        out.vertices[vi].normal = {0.f, 0.f, 0.f};
                    for (uint32 ii = subStart; ii + 2 < subStart + idxCount; ii += 3) {
                        uint32 a = out.indices[ii] + baseV, b = out.indices[ii + 1] + baseV, cc = out.indices[ii + 2] + baseV;
                        NkVec3f p0 = out.vertices[a].pos, p1 = out.vertices[b].pos, p2 = out.vertices[cc].pos;
                        NkVec3f e1 = {p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
                        NkVec3f e2 = {p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};
                        NkVec3f n  = {e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
                        out.vertices[a].normal  = {out.vertices[a].normal.x + n.x,  out.vertices[a].normal.y + n.y,  out.vertices[a].normal.z + n.z};
                        out.vertices[b].normal  = {out.vertices[b].normal.x + n.x,  out.vertices[b].normal.y + n.y,  out.vertices[b].normal.z + n.z};
                        out.vertices[cc].normal = {out.vertices[cc].normal.x + n.x, out.vertices[cc].normal.y + n.y, out.vertices[cc].normal.z + n.z};
                    }
                    for (uint32 vi = baseV; vi < (uint32)out.vertices.Size(); ++vi) {
                        NkVec3f& n = out.vertices[vi].normal;
                        float32 len = std::sqrt(n.x*n.x + n.y*n.y + n.z*n.z);
                        if (len > 1e-8f) { n.x/=len; n.y/=len; n.z/=len; } else n = {0.f,1.f,0.f};
                    }
                }

                NkSubMesh sm; sm.firstIndex = subStart; sm.indexCount = idxCount; sm.baseVertex = baseV;
                out.subMeshes.PushBack(sm);
                out.subMeshMaterial.PushBack(-1);
            }

            // ════════════════════════════════════════════════════════════════
            //  FBX ASCII (texte) -> meme arbre FbxNode que le binaire.
            //  Syntaxe : Name: prop, prop, "string" { enfants }
            //            Vertices: *N { a: 1,2,3,... }   (arrays)
            // ════════════════════════════════════════════════════════════════
            void SkipWSC(const char*& p, const char* end) {            // ws + ';' comment
                for (;;) {
                    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
                    if (p < end && *p == ';') { while (p < end && *p != '\n') ++p; continue; }
                    break;
                }
            }
            // Lit le nom d'un noeud (jusqu'a ':'). false si pas de ':'.
            bool ReadAName(const char*& p, const char* end, NkString& name) {
                SkipWSC(p, end);
                const char* s = p;
                while (p < end && *p != ':' && *p != '{' && *p != '}' && *p != '\n') ++p;
                if (p >= end || *p != ':') return false;
                const char* e = p; while (e > s && (e[-1] == ' ' || e[-1] == '\t')) --e;
                name.Clear(); if (e > s) name.Append(s, (NkString::SizeType)(e - s));
                ++p;                                                   // consomme ':'
                return !name.Empty();
            }
            // Corps d'array `{ a: nombres }` : detecte float/int via '.'/'e'.
            void ReadArrayBody(const char*& p, const char* end, FbxProp& pr) {
                bool isFloat = false;
                for (const char* q = p; q < end && *q != '}'; ++q)
                    if (*q == '.' || *q == 'e' || *q == 'E') { isFloat = true; break; }
                pr.type = isFloat ? 'd' : 'i';
                while (p < end && *p != '}') {
                    while (p < end && *p != '}' &&
                           !((*p >= '0' && *p <= '9') || *p == '-' || *p == '+' || *p == '.')) ++p;
                    if (p >= end || *p == '}') break;
                    if (isFloat) pr.arrF.PushBack(ParseDouble(p, end));
                    else         pr.arrI.PushBack(ParseInt64(p, end));
                }
            }
            bool ParseANode(const char*& p, const char* end, FbxNode& node) {
                if (!ReadAName(p, end, node.name)) return false;
                bool hasArray = false;
                for (;;) {
                    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r')) ++p;
                    if (p >= end) break;
                    char ch = *p;
                    if (ch == '{' || ch == '}' || ch == '\n') break;
                    if (ch == '"') {
                        ++p; const char* s = p; while (p < end && *p != '"') ++p;
                        FbxProp pr; pr.type = 'S'; if (p > s) pr.str.Append(s, (NkString::SizeType)(p - s));
                        if (p < end) ++p;
                        node.props.PushBack(static_cast<FbxProp&&>(pr));
                    } else if (ch == '*') {
                        ++p; ParseInt64(p, end); hasArray = true;       // compteur ignore
                    } else if ((ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.') {
                        FbxProp pr; pr.type = 'D'; pr.scalar = ParseDouble(p, end);
                        node.props.PushBack(static_cast<FbxProp&&>(pr));
                    } else {
                        const char* s = p;
                        while (p < end && *p != ',' && *p != '{' && *p != '\n' && *p != ' ' && *p != '\t') ++p;
                        FbxProp pr; pr.type = 'S'; if (p > s) pr.str.Append(s, (NkString::SizeType)(p - s));
                        node.props.PushBack(static_cast<FbxProp&&>(pr));
                    }
                    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r')) ++p;
                    if (p < end && *p == ',') { ++p; continue; }
                    break;
                }
                SkipWSC(p, end);
                if (p < end && *p == '{') {
                    ++p;
                    if (hasArray) {
                        FbxProp pr; ReadArrayBody(p, end, pr);
                        node.props.PushBack(static_cast<FbxProp&&>(pr));
                        SkipWSC(p, end); if (p < end && *p == '}') ++p;
                    } else {
                        for (;;) {
                            SkipWSC(p, end);
                            if (p >= end) break;
                            if (*p == '}') { ++p; break; }
                            const char* save = p;
                            FbxNode child;
                            if (!ParseANode(p, end, child)) {
                                while (p < end && *p != '\n' && *p != '}') ++p;
                                if (p < end && *p == '\n') ++p;
                                if (p <= save && p < end) ++p;
                                continue;
                            }
                            node.children.PushBack(static_cast<FbxNode&&>(child));
                            if (p <= save && p < end) ++p;
                        }
                    }
                }
                return true;
            }
            void ParseAsciiRoots(const char* p, const char* end, NkVector<FbxNode>& roots) {
                for (;;) {
                    SkipWSC(p, end);
                    if (p >= end) break;
                    if (*p == '}') { ++p; continue; }
                    const char* save = p;
                    FbxNode n;
                    if (!ParseANode(p, end, n)) { if (p < end) ++p; continue; }
                    if (!n.name.Empty()) roots.PushBack(static_cast<FbxNode&&>(n));
                    if (p <= save && p < end) ++p;
                }
            }

            // ── Tronc commun (binaire ET ascii) : extrait la geometrie + UpAxis.
            bool FinishFBX(NkVector<FbxNode>& roots, uint32 version, bool ascii,
                           NkGLTFMeshData& out, const NkString& path) {
                uint32 geoCount = 0;
                for (uint32 i = 0; i < (uint32)roots.Size(); ++i) {
                    if (!(roots[(NkVector<FbxNode>::SizeType)i].name == NkString("Objects"))) continue;
                    const FbxNode& objs = roots[(NkVector<FbxNode>::SizeType)i];
                    for (uint32 j = 0; j < (uint32)objs.children.Size(); ++j) {
                        const FbxNode& ch = objs.children[(NkVector<FbxNode>::SizeType)j];
                        if (ch.name == NkString("Geometry")) { ExtractGeometry(ch, out); ++geoCount; }
                    }
                }
                if (out.vertices.Empty() || out.indices.Empty()) {
                    NkLog::Instance().Warnf("[NkFBXLoader] aucune geometrie exploitable dans %s", path.CStr());
                    return false;
                }
                // UpAxis : GlobalSettings/Properties70/P "UpAxis" (1=Y, 2=Z).
                int32 upAxis = 1;
                for (uint32 i = 0; i < (uint32)roots.Size(); ++i) {
                    if (!(roots[(NkVector<FbxNode>::SizeType)i].name == NkString("GlobalSettings"))) continue;
                    const FbxNode* p70 = roots[(NkVector<FbxNode>::SizeType)i].Find("Properties70");
                    if (!p70) break;
                    for (uint32 j = 0; j < (uint32)p70->children.Size(); ++j) {
                        const FbxNode& pn = p70->children[(NkVector<FbxNode>::SizeType)j];
                        if (!(pn.name == NkString("P")) || pn.props.Size() < 5) continue;
                        if (pn.props[0].str == NkString("UpAxis"))
                            upAxis = (int32)pn.props[(NkVector<FbxProp>::SizeType)4].scalar;
                    }
                    break;
                }
                if (getenv("NK_FBX_ZUP")) upAxis = 2;     // cf. header : geometrie Z-up mal etiquetee
                if (upAxis == 2) {
                    for (uint32 i = 0; i < (uint32)out.vertices.Size(); ++i) {
                        NkVertex3D& v = out.vertices[i];
                        v.pos    = { v.pos.x,    v.pos.z,    -v.pos.y };
                        v.normal = { v.normal.x, v.normal.z, -v.normal.y };
                    }
                }
                ComputeBounds(out);
                out.debugName = path;
                NkLog::Instance().Infof("[NkFBXLoader] OK '%s' (%s v%u) : %u geometries, %u verts, %u indices, %u sous-meshes",
                                        path.CStr(), ascii ? "ascii" : "binaire", version, geoCount,
                                        (uint32)out.vertices.Size(), (uint32)out.indices.Size(),
                                        (uint32)out.subMeshes.Size());
                return true;
            }
        } // namespace

        bool LoadFBX(const NkString& path, NkGLTFMeshData& out) {
            NkVector<nk_uint8> buf = NkFile::ReadAllBytes(path.CStr());
            if (buf.Size() < 27) {
                NkLog::Instance().Warnf("[NkFBXLoader] fichier introuvable/trop court : %s", path.CStr());
                return false;
            }
            const uint8* base = buf.Data();
            const char* magic = "Kaydara FBX Binary";
            bool isBinary = true;
            for (int32 i = 0; magic[i]; ++i) {
                if ((char)base[i] != magic[i]) { isBinary = false; break; }
            }

            NkVector<FbxNode> roots;
            uint32 version = 0;

            if (isBinary) {
                version = ReadU32LE(base + 23);
                bool is64 = (version >= 7500);
                Cur c{ base + 27, base + buf.Size(), true };
                while (c.p + (is64 ? 25 : 13) <= c.end && c.ok) {
                    const uint8* save = c.p;
                    FbxNode n; bool isNull = false;
                    if (!ParseNode(c, base, is64, n, isNull)) break;
                    if (isNull) break;
                    if (n.name.Empty() && n.children.Empty() && n.props.Empty()) break;
                    roots.PushBack(static_cast<FbxNode&&>(n));
                    if (c.p <= save) break;     // securite anti-boucle
                }
            } else {
                // FBX ASCII : commence par un commentaire ';' ou 'FBXHeaderExtension'.
                ParseAsciiRoots((const char*)base, (const char*)base + buf.Size(), roots);
                // Version : FBXHeaderExtension/FBXVersion (optionnel, pour le log).
                for (uint32 i = 0; i < (uint32)roots.Size(); ++i) {
                    const FbxNode& r = roots[(NkVector<FbxNode>::SizeType)i];
                    if (!(r.name == NkString("FBXHeaderExtension"))) continue;
                    const FbxNode* fv = r.Find("FBXVersion");
                    if (fv && !fv->props.Empty()) version = (uint32)fv->props[0].scalar;
                    break;
                }
            }

            if (roots.Empty()) {
                NkLog::Instance().Warnf("[NkFBXLoader] arbre vide (ni binaire ni ascii valide) : %s", path.CStr());
                return false;
            }
            return FinishFBX(roots, version, !isBinary, out, path);
        }

    } // namespace renderer
} // namespace nkentseu
