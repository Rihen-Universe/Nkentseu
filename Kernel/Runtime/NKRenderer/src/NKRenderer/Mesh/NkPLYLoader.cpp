// =============================================================================
// NkPLYLoader.cpp — NKRenderer (Mesh/) — PLY ascii + binary_little_endian.
// =============================================================================
#include "NkPLYLoader.h"
#include "NkMeshLoaderUtil.h"

#include "NKLogger/NkLog.h"
#include "NKFileSystem/NkFile.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace renderer {

        namespace {
            using namespace meshutil;

            enum PlyType : uint8 { PT_CHAR, PT_UCHAR, PT_SHORT, PT_USHORT,
                                   PT_INT, PT_UINT, PT_FLOAT, PT_DOUBLE, PT_NONE };

            PlyType ParseType(const NkString& s) {
                if (s == NkString("char")   || s == NkString("int8"))    return PT_CHAR;
                if (s == NkString("uchar")  || s == NkString("uint8"))   return PT_UCHAR;
                if (s == NkString("short")  || s == NkString("int16"))   return PT_SHORT;
                if (s == NkString("ushort") || s == NkString("uint16"))  return PT_USHORT;
                if (s == NkString("int")    || s == NkString("int32"))   return PT_INT;
                if (s == NkString("uint")   || s == NkString("uint32"))  return PT_UINT;
                if (s == NkString("float")  || s == NkString("float32")) return PT_FLOAT;
                if (s == NkString("double") || s == NkString("float64")) return PT_DOUBLE;
                return PT_NONE;
            }
            uint32 TypeSize(PlyType t) {
                switch (t) {
                    case PT_CHAR: case PT_UCHAR:   return 1;
                    case PT_SHORT: case PT_USHORT: return 2;
                    case PT_INT: case PT_UINT: case PT_FLOAT: return 4;
                    case PT_DOUBLE:                return 8;
                    default:                       return 0;
                }
            }
            bool IsFloatType(PlyType t) { return t == PT_FLOAT || t == PT_DOUBLE; }

            struct PlyProp {
                NkString name;
                PlyType  type      = PT_NONE;   // type scalaire (ou type des elements si liste)
                bool     isList    = false;
                PlyType  countType = PT_NONE;
            };
            struct PlyElem {
                NkString name;
                uint32   count = 0;
                NkVector<PlyProp> props;
            };

            // Lit une valeur scalaire binaire LE comme f64.
            float64 ReadBinF64(const uint8*& d, const uint8* end, PlyType t) {
                if (d + TypeSize(t) > end) return 0.0;
                float64 r = 0.0;
                switch (t) {
                    case PT_CHAR:   r = (float64)(int8)d[0]; break;
                    case PT_UCHAR:  r = (float64)d[0]; break;
                    case PT_SHORT:  r = (float64)(int16)ReadU16LE(d); break;
                    case PT_USHORT: r = (float64)ReadU16LE(d); break;
                    case PT_INT:    r = (float64)(int32)ReadU32LE(d); break;
                    case PT_UINT:   r = (float64)ReadU32LE(d); break;
                    case PT_FLOAT:  r = (float64)ReadF32LE(d); break;
                    case PT_DOUBLE: r = ReadF64LE(d); break;
                    default: break;
                }
                d += TypeSize(t);
                return r;
            }

            // Index des proprietes interessantes du vertex.
            struct VtxMap { int32 x=-1,y=-1,z=-1,nx=-1,ny=-1,nz=-1,r=-1,g=-1,b=-1,a=-1,s=-1,t=-1; };

            void BuildVtxMap(const PlyElem& e, VtxMap& vm) {
                for (int32 i = 0; i < (int32)e.props.Size(); ++i) {
                    const NkString& n = e.props[(NkVector<PlyProp>::SizeType)i].name;
                    if      (n == NkString("x"))  vm.x = i;  else if (n == NkString("y"))  vm.y = i;
                    else if (n == NkString("z"))  vm.z = i;  else if (n == NkString("nx")) vm.nx = i;
                    else if (n == NkString("ny")) vm.ny = i; else if (n == NkString("nz")) vm.nz = i;
                    else if (n == NkString("red"))   vm.r = i; else if (n == NkString("green")) vm.g = i;
                    else if (n == NkString("blue"))  vm.b = i; else if (n == NkString("alpha")) vm.a = i;
                    else if (n == NkString("s") || n == NkString("u") || n == NkString("texture_u")) vm.s = i;
                    else if (n == NkString("t") || n == NkString("v") || n == NkString("texture_v")) vm.t = i;
                }
            }

            void StoreVertex(NkGLTFMeshData& out, const VtxMap& vm,
                             const NkVector<PlyProp>& props, const float64* vals) {
                NkVertex3D v{};
                v.pos = { vm.x >= 0 ? (float32)vals[vm.x] : 0.f,
                          vm.y >= 0 ? (float32)vals[vm.y] : 0.f,
                          vm.z >= 0 ? (float32)vals[vm.z] : 0.f };
                v.normal = { vm.nx >= 0 ? (float32)vals[vm.nx] : 0.f,
                             vm.ny >= 0 ? (float32)vals[vm.ny] : 0.f,
                             vm.nz >= 0 ? (float32)vals[vm.nz] : 0.f };
                v.tangent = {0.f, 0.f, 0.f};
                v.uv  = { vm.s >= 0 ? (float32)vals[vm.s] : 0.f,
                          vm.t >= 0 ? (float32)vals[vm.t] : 0.f };
                v.uv2 = v.uv;
                auto chan = [&](int32 idx) -> uint8 {
                    if (idx < 0) return 255;
                    float64 x = vals[idx];
                    if (IsFloatType(props[(NkVector<PlyProp>::SizeType)idx].type)) x *= 255.0;
                    if (x < 0) x = 0; if (x > 255) x = 255;
                    return (uint8)(x + 0.5);
                };
                v.color = PackRGBA(chan(vm.r), chan(vm.g), chan(vm.b), vm.a >= 0 ? chan(vm.a) : 255);
                out.vertices.PushBack(v);
            }
        } // namespace

        bool LoadPLY(const NkString& path, NkGLTFMeshData& out) {
            NkVector<nk_uint8> buf = NkFile::ReadAllBytes(path.CStr());
            if (buf.Size() < 4) {
                NkLog::Instance().Warnf("[NkPLYLoader] fichier introuvable/vide : %s", path.CStr());
                return false;
            }
            const char* p   = (const char*)buf.Data();
            const char* end = p + buf.Size();
            if (!(p[0] == 'p' && p[1] == 'l' && p[2] == 'y')) {
                NkLog::Instance().Warnf("[NkPLYLoader] magic 'ply' absent : %s", path.CStr());
                return false;
            }

            // ── Parse l'en-tete (toujours ASCII) ─────────────────────────────
            bool ascii = true, bigEndian = false;
            NkVector<PlyElem> elems;
            NkString tok;
            const char* hp = p;
            while (hp < end) {
                SkipWS(hp, end);
                ReadToken(hp, end, tok);
                if (tok == NkString("format")) {
                    ReadToken(hp, end, tok);
                    if (tok == NkString("ascii")) ascii = true;
                    else { ascii = false; bigEndian = (tok == NkString("binary_big_endian")); }
                } else if (tok == NkString("element")) {
                    PlyElem e; ReadToken(hp, end, e.name);
                    e.count = (uint32)ParseInt64(hp, end);
                    elems.PushBack(static_cast<PlyElem&&>(e));
                } else if (tok == NkString("property")) {
                    if (elems.Empty()) { SkipToEOL(hp, end); continue; }
                    PlyProp pr;
                    ReadToken(hp, end, tok);
                    if (tok == NkString("list")) {
                        pr.isList = true;
                        NkString ct, et; ReadToken(hp, end, ct); ReadToken(hp, end, et);
                        pr.countType = ParseType(ct); pr.type = ParseType(et);
                    } else {
                        pr.type = ParseType(tok);
                    }
                    ReadToken(hp, end, pr.name);
                    elems[(NkVector<PlyElem>::SizeType)elems.Size() - 1].props.PushBack(static_cast<PlyProp&&>(pr));
                } else if (tok == NkString("end_header")) {
                    SkipToEOL(hp, end); if (hp < end) ++hp;  // saute le '\n'
                    break;
                } else {
                    SkipToEOL(hp, end);                       // comment / obj_info / ...
                }
            }
            if (bigEndian) {
                NkLog::Instance().Warnf("[NkPLYLoader] binary_big_endian non supporte : %s", path.CStr());
                return false;
            }

            bool anyNormal = false;
            const char*  ap = hp;                 // curseur ASCII
            const uint8* bp = (const uint8*)hp;   // curseur binaire

            for (uint32 ei = 0; ei < (uint32)elems.Size(); ++ei) {
                const PlyElem& e = elems[(NkVector<PlyElem>::SizeType)ei];
                bool isVertex = (e.name == NkString("vertex"));
                bool isFace   = (e.name == NkString("face"));
                VtxMap vm;
                if (isVertex) { BuildVtxMap(e, vm); if (vm.nx >= 0) anyNormal = true; }

                for (uint32 r = 0; r < e.count; ++r) {
                    if (isVertex) {
                        float64 vals[64]; int32 np = (int32)e.props.Size(); if (np > 64) np = 64;
                        for (int32 pi = 0; pi < np; ++pi) {
                            const PlyProp& pr = e.props[(NkVector<PlyProp>::SizeType)pi];
                            if (pr.isList) {  // rare sur vertex : on saute
                                int64 cnt = ascii ? ParseInt64WS(ap, end) : (int64)ReadBinF64(bp, (const uint8*)end, pr.countType);
                                for (int64 k = 0; k < cnt; ++k) { if (ascii) ParseDoubleWS(ap, end); else ReadBinF64(bp, (const uint8*)end, pr.type); }
                                vals[pi] = 0.0;
                            } else {
                                vals[pi] = ascii ? ParseDoubleWS(ap, end) : ReadBinF64(bp, (const uint8*)end, pr.type);
                            }
                        }
                        StoreVertex(out, vm, e.props, vals);
                    } else if (isFace) {
                        // Une property liste = vertex_indices. Les autres props scalaires sont sautees.
                        for (int32 pi = 0; pi < (int32)e.props.Size(); ++pi) {
                            const PlyProp& pr = e.props[(NkVector<PlyProp>::SizeType)pi];
                            if (pr.isList) {
                                int64 cnt = ascii ? ParseInt64WS(ap, end) : (int64)ReadBinF64(bp, (const uint8*)end, pr.countType);
                                int32 idx[64]; int32 nn = 0;
                                for (int64 k = 0; k < cnt; ++k) {
                                    int32 vi = ascii ? (int32)ParseInt64WS(ap, end) : (int32)ReadBinF64(bp, (const uint8*)end, pr.type);
                                    if (nn < 64) idx[nn++] = vi;
                                }
                                for (int32 k = 1; k + 1 < nn; ++k) {       // fan
                                    out.indices.PushBack((uint32)idx[0]);
                                    out.indices.PushBack((uint32)idx[k]);
                                    out.indices.PushBack((uint32)idx[k + 1]);
                                }
                            } else {
                                if (ascii) ParseDoubleWS(ap, end); else ReadBinF64(bp, (const uint8*)end, pr.type);
                            }
                        }
                    } else {
                        // Element inconnu : on saute ses donnees.
                        for (int32 pi = 0; pi < (int32)e.props.Size(); ++pi) {
                            const PlyProp& pr = e.props[(NkVector<PlyProp>::SizeType)pi];
                            if (pr.isList) {
                                int64 cnt = ascii ? ParseInt64WS(ap, end) : (int64)ReadBinF64(bp, (const uint8*)end, pr.countType);
                                for (int64 k = 0; k < cnt; ++k) { if (ascii) ParseDoubleWS(ap, end); else ReadBinF64(bp, (const uint8*)end, pr.type); }
                            } else { if (ascii) ParseDoubleWS(ap, end); else ReadBinF64(bp, (const uint8*)end, pr.type); }
                        }
                    }
                }
            }

            if (out.vertices.Empty()) {
                NkLog::Instance().Warnf("[NkPLYLoader] aucun vertex dans %s", path.CStr());
                return false;
            }
            // Maillage sans faces (nuage de points) : on genere des indices points->triangles
            // degeneres n'a pas de sens ; on laisse indices vide -> un seul sous-mesh "points".
            if (out.indices.Empty()) {
                for (uint32 i = 0; i < (uint32)out.vertices.Size(); ++i) out.indices.PushBack(i);
            }
            if (!anyNormal) GenerateSmoothNormals(out);

            NkSubMesh sm; sm.firstIndex = 0; sm.indexCount = (uint32)out.indices.Size();
            out.subMeshes.PushBack(sm); out.subMeshMaterial.PushBack(-1);
            ComputeBounds(out);
            out.debugName = path;
            NkLog::Instance().Infof("[NkPLYLoader] OK '%s' (%s) : %u verts, %u indices",
                                    path.CStr(), ascii ? "ascii" : "binaire",
                                    (uint32)out.vertices.Size(), (uint32)out.indices.Size());
            return true;
        }

    } // namespace renderer
} // namespace nkentseu
