// =============================================================================
// NkUSDALoader.cpp — NKRenderer (Mesh/) — USD ASCII (.usda), zero-STL.
// =============================================================================
#include "NkUSDALoader.h"
#include "NkMeshLoaderUtil.h"

#include "NKLogger/NkLog.h"
#include "NKFileSystem/NkFile.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace renderer {

        namespace {
            using namespace meshutil;

            // Cherche `name` suivi (apres espaces) de '=' puis '[' ; renvoie le
            // contenu entre '[' et ']' matching. [s,e) ou false.
            bool FindArray(const char* bs, const char* be, const char* name,
                           const char*& s, const char*& e) {
                usize nl = 0; while (name[nl]) ++nl;
                for (const char* p = bs; p + nl <= be; ++p) {
                    bool match = true;
                    for (usize i = 0; i < nl; ++i) if (p[i] != name[i]) { match = false; break; }
                    if (!match) continue;
                    // bord gauche : doit etre un debut de token (espace/[/:/début)
                    if (p > bs) { char c = p[-1]; if (!(c==' '||c=='\t'||c=='\n'||c=='\r'||c==']'||c=='[')) continue; }
                    const char* q = p + nl;
                    while (q < be && (*q==' '||*q=='\t'||*q=='\r'||*q=='\n')) ++q;
                    if (q >= be || *q != '=') continue;
                    ++q;
                    while (q < be && (*q==' '||*q=='\t'||*q=='\r'||*q=='\n')) ++q;
                    if (q >= be || *q != '[') continue;
                    ++q; s = q;
                    int32 depth = 1;
                    while (q < be && depth > 0) { if (*q=='[') ++depth; else if (*q==']') --depth; if (depth==0) break; ++q; }
                    e = q;
                    return true;
                }
                return false;
            }

            void NumsF(const char* p, const char* end, NkVector<float64>& out) {
                for (;;) { while (p<end && !((*p>='0'&&*p<='9')||*p=='-'||*p=='+'||*p=='.')) ++p;
                    if (p>=end) break; out.PushBack(ParseDouble(p,end)); }
            }
            void NumsI(const char* p, const char* end, NkVector<int64>& out) {
                for (;;) { while (p<end && !((*p>='0'&&*p<='9')||*p=='-'||*p=='+')) ++p;
                    if (p>=end) break; out.PushBack(ParseInt64(p,end)); }
            }

            // Extrait un bloc Mesh [bs,be) (interieur des accolades).
            void ExtractMeshBlock(const char* bs, const char* be, NkGLTFMeshData& out) {
                const char *s, *e;
                NkVector<float64> points, normals, st;
                NkVector<int64>   fvi, fvc;
                if (FindArray(bs, be, "points", s, e))            NumsF(s, e, points);
                if (FindArray(bs, be, "faceVertexIndices", s, e)) NumsI(s, e, fvi);
                if (FindArray(bs, be, "faceVertexCounts", s, e))  NumsI(s, e, fvc);
                if (FindArray(bs, be, "normals", s, e))           NumsF(s, e, normals);
                if (FindArray(bs, be, "primvars:st", s, e))       NumsF(s, e, st);

                if (points.Empty() || fvi.Empty()) return;
                int32 nPoints  = (int32)(points.Size() / 3);
                int32 nCorners = (int32)fvi.Size();
                int32 nNrm = (int32)(normals.Size() / 3);
                int32 nSt  = (int32)(st.Size() / 2);
                bool nrmPerPoint  = (nNrm == nPoints);
                bool nrmPerCorner = (nNrm == nCorners);
                bool stPerPoint   = (nSt == nPoints);
                bool stPerCorner  = (nSt == nCorners);

                uint32 baseV = (uint32)out.vertices.Size();
                uint32 subStart = (uint32)out.indices.Size();
                uint32 white = PackRGBA(255,255,255);
                bool anyNormal = false;

                auto emit = [&](int32 corner) -> uint32 {
                    int64 pi = fvi[(NkVector<int64>::SizeType)corner];
                    NkVertex3D v{};
                    if (pi >= 0 && pi*3+2 < (int64)points.Size())
                        v.pos = { (float32)points[(NkVector<float64>::SizeType)(pi*3+0)],
                                  (float32)points[(NkVector<float64>::SizeType)(pi*3+1)],
                                  (float32)points[(NkVector<float64>::SizeType)(pi*3+2)] };
                    int64 ni = nrmPerCorner ? corner : (nrmPerPoint ? pi : -1);
                    if (ni >= 0 && ni*3+2 < (int64)normals.Size()) {
                        v.normal = { (float32)normals[(NkVector<float64>::SizeType)(ni*3+0)],
                                     (float32)normals[(NkVector<float64>::SizeType)(ni*3+1)],
                                     (float32)normals[(NkVector<float64>::SizeType)(ni*3+2)] };
                        anyNormal = true;
                    }
                    int64 ti = stPerCorner ? corner : (stPerPoint ? pi : -1);
                    if (ti >= 0 && ti*2+1 < (int64)st.Size())
                        v.uv = { (float32)st[(NkVector<float64>::SizeType)(ti*2+0)],
                                 1.f - (float32)st[(NkVector<float64>::SizeType)(ti*2+1)] };
                    v.tangent={0.f,0.f,0.f}; v.uv2=v.uv; v.color=white;
                    out.vertices.PushBack(v);
                    return (uint32)out.vertices.Size() - 1 - baseV;
                };

                int32 corner = 0;
                if (!fvc.Empty()) {
                    for (uint32 f = 0; f < (uint32)fvc.Size(); ++f) {
                        int32 vc = (int32)fvc[(NkVector<int64>::SizeType)f];
                        if (corner + vc > nCorners) break;
                        uint32 fan[256]; int32 fn = 0;
                        for (int32 k = 0; k < vc && fn < 256; ++k) fan[fn++] = emit(corner + k);
                        for (int32 k = 1; k + 1 < fn; ++k) {
                            out.indices.PushBack(fan[0]); out.indices.PushBack(fan[k]); out.indices.PushBack(fan[k+1]);
                        }
                        corner += vc;
                    }
                } else {   // pas de counts : suppose des triangles
                    for (int32 c = 0; c + 2 < nCorners; c += 3) {
                        uint32 a=emit(c),b=emit(c+1),cc=emit(c+2);
                        out.indices.PushBack(a); out.indices.PushBack(b); out.indices.PushBack(cc);
                    }
                }

                uint32 idxCount = (uint32)out.indices.Size() - subStart;
                if (idxCount == 0) return;
                if (!anyNormal) {
                    for (uint32 vi=baseV; vi<(uint32)out.vertices.Size(); ++vi) out.vertices[vi].normal={0.f,0.f,0.f};
                    for (uint32 ii=subStart; ii+2<subStart+idxCount; ii+=3) {
                        uint32 a=out.indices[ii]+baseV,b=out.indices[ii+1]+baseV,cc=out.indices[ii+2]+baseV;
                        NkVec3f p0=out.vertices[a].pos,p1=out.vertices[b].pos,p2=out.vertices[cc].pos;
                        NkVec3f e1={p1.x-p0.x,p1.y-p0.y,p1.z-p0.z}, e2={p2.x-p0.x,p2.y-p0.y,p2.z-p0.z};
                        NkVec3f n={e1.y*e2.z-e1.z*e2.y,e1.z*e2.x-e1.x*e2.z,e1.x*e2.y-e1.y*e2.x};
                        out.vertices[a].normal={out.vertices[a].normal.x+n.x,out.vertices[a].normal.y+n.y,out.vertices[a].normal.z+n.z};
                        out.vertices[b].normal={out.vertices[b].normal.x+n.x,out.vertices[b].normal.y+n.y,out.vertices[b].normal.z+n.z};
                        out.vertices[cc].normal={out.vertices[cc].normal.x+n.x,out.vertices[cc].normal.y+n.y,out.vertices[cc].normal.z+n.z};
                    }
                    for (uint32 vi=baseV; vi<(uint32)out.vertices.Size(); ++vi){
                        NkVec3f& n=out.vertices[vi].normal; float32 l=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
                        if(l>1e-8f){n.x/=l;n.y/=l;n.z/=l;} else n={0.f,1.f,0.f};
                    }
                }
                NkSubMesh sm; sm.firstIndex=subStart; sm.indexCount=idxCount; sm.baseVertex=baseV;
                out.subMeshes.PushBack(sm); out.subMeshMaterial.PushBack(-1);
            }
        } // namespace

        bool LoadUSDA(const NkString& path, NkGLTFMeshData& out) {
            NkString text = NkFile::ReadAllText(path.CStr());
            if (text.Empty()) {
                NkLog::Instance().Warnf("[NkUSDALoader] fichier introuvable/vide : %s", path.CStr());
                return false;
            }
            const char* p   = text.CStr();
            const char* end = p + text.Size();
            if (!(text.Size() >= 5 && p[0]=='#' && p[1]=='u' && p[2]=='s' && p[3]=='d' && p[4]=='a')) {
                NkLog::Instance().Warnf("[NkUSDALoader] entete '#usda' absent (USDC binaire non supporte) : %s", path.CStr());
                return false;
            }

            // Chaque "def Mesh" ... { ... } (brace-matching).
            uint32 meshCount = 0;
            const char* kw = "def Mesh";
            for (const char* q = p; q + 8 <= end; ++q) {
                bool m = true; for (int32 i = 0; i < 8; ++i) if (q[i] != kw[i]) { m = false; break; }
                if (!m) continue;
                const char* b = q + 8;
                // jusqu'a '{' (le nom + d'eventuelles metadata (...) peuvent etre
                // sur la meme ligne OU la suivante -> on saute tout jusqu'a '{').
                while (b < end && *b != '{') ++b;
                if (b >= end) continue;
                ++b; const char* bs = b; int32 depth = 1;
                while (b < end && depth > 0) { if (*b=='{') ++depth; else if (*b=='}') --depth; if (depth==0) break; ++b; }
                ExtractMeshBlock(bs, b, out); ++meshCount;
                q = b;   // continue apres ce bloc
            }

            if (out.vertices.Empty() || out.indices.Empty()) {
                NkLog::Instance().Warnf("[NkUSDALoader] aucune geometrie exploitable : %s", path.CStr());
                return false;
            }

            // upAxis (metadata header). USD defaut = Y. "Z" -> conversion.
            bool zUp = false;
            // recherche simple de upAxis = "Z"
            for (const char* q = p; q + 7 <= end; ++q) {
                if (q[0]=='u'&&q[1]=='p'&&q[2]=='A'&&q[3]=='x'&&q[4]=='i'&&q[5]=='s') {
                    const char* r = q + 6;
                    while (r < end && (*r==' '||*r=='\t'||*r=='=')) ++r;
                    if (r < end && (*r=='"'||*r=='\'')) { ++r; if (r<end && (*r=='Z'||*r=='z')) zUp = true; }
                    break;
                }
            }
            if (zUp) for (uint32 i = 0; i < (uint32)out.vertices.Size(); ++i) {
                NkVertex3D& v = out.vertices[i];
                v.pos    = { v.pos.x, v.pos.z, -v.pos.y };
                v.normal = { v.normal.x, v.normal.z, -v.normal.y };
            }

            ComputeBounds(out);
            out.debugName = path;
            NkLog::Instance().Infof("[NkUSDALoader] OK '%s' : %u meshes, %u verts, %u indices, %u sous-meshes%s",
                                    path.CStr(), meshCount, (uint32)out.vertices.Size(),
                                    (uint32)out.indices.Size(), (uint32)out.subMeshes.Size(),
                                    zUp ? " (Z->Y)" : "");
            return true;
        }

    } // namespace renderer
} // namespace nkentseu
