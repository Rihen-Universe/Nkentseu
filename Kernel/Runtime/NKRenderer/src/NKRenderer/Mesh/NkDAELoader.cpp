// =============================================================================
// NkDAELoader.cpp — NKRenderer (Mesh/) — COLLADA .dae (XML), zero-STL.
// =============================================================================
#include "NkDAELoader.h"
#include "NkMeshLoaderUtil.h"

#include "NKLogger/NkLog.h"
#include "NKFileSystem/NkFile.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace renderer {

        namespace {
            using namespace meshutil;

            // ── Mini DOM XML ─────────────────────────────────────────────────
            struct XmlAttr { NkString name, value; };
            struct XmlNode {
                NkString name;
                NkVector<XmlAttr> attrs;
                NkString text;
                NkVector<XmlNode> children;

                const NkString* Attr(const char* n) const {
                    for (uint32 i = 0; i < (uint32)attrs.Size(); ++i)
                        if (attrs[(NkVector<XmlAttr>::SizeType)i].name == NkString(n))
                            return &attrs[(NkVector<XmlAttr>::SizeType)i].value;
                    return nullptr;
                }
                const XmlNode* Child(const char* n) const {
                    for (uint32 i = 0; i < (uint32)children.Size(); ++i)
                        if (children[(NkVector<XmlNode>::SizeType)i].name == NkString(n))
                            return &children[(NkVector<XmlNode>::SizeType)i];
                    return nullptr;
                }
            };

            bool IsNameChar(char c) {
                return !(c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
                         c == '>' || c == '/' || c == '=' || c == '<');
            }

            // Parse l'element a partir de p (qui pointe juste APRES le '<').
            bool ParseElement(const char*& p, const char* end, XmlNode& node);

            // Saute commentaires/PI/doctype au point courant ('<' deja vu si besoin).
            // Retourne true si quelque chose a ete saute.
            bool SkipMisc(const char*& p, const char* end) {
                if (p + 4 <= end && p[0] == '<' && p[1] == '!' && p[2] == '-' && p[3] == '-') {
                    p += 4;
                    while (p + 3 <= end && !(p[0] == '-' && p[1] == '-' && p[2] == '>')) ++p;
                    p += 3; return true;
                }
                if (p + 2 <= end && p[0] == '<' && (p[1] == '?' || p[1] == '!')) {
                    p += 2; while (p < end && *p != '>') ++p; if (p < end) ++p; return true;
                }
                return false;
            }

            bool ParseElement(const char*& p, const char* end, XmlNode& node) {
                // nom
                const char* s = p;
                while (p < end && IsNameChar(*p)) ++p;
                node.name.Clear(); if (p > s) node.name.Append(s, (NkString::SizeType)(p - s));
                // attributs
                for (;;) {
                    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
                    if (p >= end) return false;
                    if (*p == '/' || *p == '>') break;
                    const char* as = p;
                    while (p < end && IsNameChar(*p)) ++p;
                    XmlAttr at; if (p > as) at.name.Append(as, (NkString::SizeType)(p - as));
                    while (p < end && (*p == ' ' || *p == '=' )) ++p;
                    if (p < end && (*p == '"' || *p == '\'')) {
                        char q = *p; ++p; const char* vs = p;
                        while (p < end && *p != q) ++p;
                        if (p > vs) at.value.Append(vs, (NkString::SizeType)(p - vs));
                        if (p < end) ++p;
                    }
                    if (!at.name.Empty()) node.attrs.PushBack(static_cast<XmlAttr&&>(at));
                }
                if (p < end && *p == '/') {           // <name .../>
                    p += 1; if (p < end && *p == '>') ++p;
                    return true;
                }
                if (p < end && *p == '>') ++p;        // fin du tag ouvrant
                // contenu
                for (;;) {
                    const char* ts = p;
                    while (p < end && *p != '<') ++p;  // texte
                    if (p > ts) {
                        for (const char* c = ts; c < p; ++c)
                            if (!(*c == '\n' || *c == '\r' || *c == '\t')) { node.text.Append(*c); }
                    }
                    if (p >= end) break;
                    // ici *p == '<'
                    if (p + 1 < end && p[1] == '/') {  // tag fermant </name>
                        p += 2; while (p < end && *p != '>') ++p; if (p < end) ++p;
                        break;
                    }
                    if (SkipMisc(p, end)) continue;
                    ++p;                                // consomme '<'
                    XmlNode child;
                    if (!ParseElement(p, end, child)) break;
                    node.children.PushBack(static_cast<XmlNode&&>(child));
                }
                return true;
            }

            bool ParseXML(const char* p, const char* end, XmlNode& root) {
                while (p < end) {
                    while (p < end && *p != '<') ++p;
                    if (p >= end) return false;
                    if (SkipMisc(p, end)) continue;
                    ++p;                                // consomme '<'
                    return ParseElement(p, end, root);
                }
                return false;
            }

            // ── Helpers d'extraction ─────────────────────────────────────────
            NkString StripHash(const NkString& s) {
                if (!s.Empty() && s[0] == '#') return s.SubStr(1);
                return s;
            }
            void ParseFloats(const NkString& text, NkVector<float64>& out) {
                const char* p = text.CStr(); const char* end = p + text.Size();
                for (;;) { SkipWS(p, end); if (p >= end) break;
                    if (!((*p >= '0' && *p <= '9') || *p == '-' || *p == '+' || *p == '.')) { ++p; continue; }
                    out.PushBack(ParseDouble(p, end)); }
            }
            void ParseInts(const NkString& text, NkVector<int64>& out) {
                const char* p = text.CStr(); const char* end = p + text.Size();
                for (;;) { SkipWS(p, end); if (p >= end) break;
                    if (!((*p >= '0' && *p <= '9') || *p == '-' || *p == '+')) { ++p; continue; }
                    out.PushBack(ParseInt64(p, end)); }
            }

            struct Source { NkString id; NkVector<float64> data; int32 stride = 1; };

            // Extrait un <mesh> dans out (un sous-mesh par triangles/polylist).
            void ExtractMesh(const XmlNode& mesh, NkGLTFMeshData& out) {
                // sources
                NkVector<Source> sources;
                for (uint32 i = 0; i < (uint32)mesh.children.Size(); ++i) {
                    const XmlNode& sn = mesh.children[(NkVector<XmlNode>::SizeType)i];
                    if (!(sn.name == NkString("source"))) continue;
                    Source src; if (auto* id = sn.Attr("id")) src.id = *id;
                    if (const XmlNode* fa = sn.Child("float_array")) ParseFloats(fa->text, src.data);
                    if (const XmlNode* tc = sn.Child("technique_common"))
                        if (const XmlNode* ac = tc->Child("accessor"))
                            if (auto* st = ac->Attr("stride")) { int32 v=0; if (st->ToInt(v)) src.stride = v; }
                    sources.PushBack(static_cast<Source&&>(src));
                }
                auto findSrc = [&](const NkString& id) -> const Source* {
                    NkString want = StripHash(id);
                    for (uint32 i = 0; i < (uint32)sources.Size(); ++i)
                        if (sources[(NkVector<Source>::SizeType)i].id == want) return &sources[(NkVector<Source>::SizeType)i];
                    return nullptr;
                };
                // <vertices id=...> -> POSITION source
                NkString verticesId, posSrcFromVerts;
                if (const XmlNode* vn = mesh.Child("vertices")) {
                    if (auto* id = vn->Attr("id")) verticesId = *id;
                    for (uint32 i = 0; i < (uint32)vn->children.Size(); ++i) {
                        const XmlNode& in = vn->children[(NkVector<XmlNode>::SizeType)i];
                        if (in.name == NkString("input"))
                            if (auto* sem = in.Attr("semantic")) if (*sem == NkString("POSITION"))
                                if (auto* src = in.Attr("source")) posSrcFromVerts = *src;
                    }
                }

                uint32 white = PackRGBA(255, 255, 255);

                // Traite chaque primitive triangles/polylist.
                for (uint32 ci = 0; ci < (uint32)mesh.children.Size(); ++ci) {
                    const XmlNode& prim = mesh.children[(NkVector<XmlNode>::SizeType)ci];
                    bool isTri  = (prim.name == NkString("triangles"));
                    bool isPoly = (prim.name == NkString("polylist"));
                    if (!isTri && !isPoly) continue;

                    // inputs
                    const Source* posSrc = nullptr; int32 posOff = -1, posStride = 3;
                    const Source* nrmSrc = nullptr; int32 nrmOff = -1, nrmStride = 3;
                    const Source* uvSrc  = nullptr; int32 uvOff  = -1, uvStride  = 2;
                    int32 maxOff = 0;
                    for (uint32 i = 0; i < (uint32)prim.children.Size(); ++i) {
                        const XmlNode& in = prim.children[(NkVector<XmlNode>::SizeType)i];
                        if (!(in.name == NkString("input"))) continue;
                        auto* sem = in.Attr("semantic"); auto* src = in.Attr("source");
                        auto* off = in.Attr("offset");
                        int32 o = 0; if (off) off->ToInt(o); if (o > maxOff) maxOff = o;
                        if (!sem || !src) continue;
                        if (*sem == NkString("VERTEX")) {
                            const Source* ps = findSrc(posSrcFromVerts);
                            posSrc = ps; posOff = o; if (ps) posStride = ps->stride;
                        } else if (*sem == NkString("NORMAL")) {
                            nrmSrc = findSrc(*src); nrmOff = o; if (nrmSrc) nrmStride = nrmSrc->stride;
                        } else if (*sem == NkString("TEXCOORD")) {
                            uvSrc = findSrc(*src); uvOff = o; if (uvSrc) uvStride = uvSrc->stride;
                        }
                    }
                    int32 perCorner = maxOff + 1;
                    if (!posSrc || perCorner <= 0) continue;

                    NkVector<int64> pidx;
                    if (const XmlNode* pn = prim.Child("p")) ParseInts(pn->text, pidx);
                    if (pidx.Empty()) continue;

                    NkVector<int64> vcount;
                    if (isPoly) if (const XmlNode* vc = prim.Child("vcount")) ParseInts(vc->text, vcount);

                    uint32 subStart = (uint32)out.indices.Size();
                    uint32 baseV    = (uint32)out.vertices.Size();
                    bool anyNormal  = false;

                    int32 numCorners = (int32)(pidx.Size() / (uint32)perCorner);
                    auto emit = [&](int32 corner) -> uint32 {
                        NkVertex3D v{};
                        int64 pi = pidx[(NkVector<int64>::SizeType)(corner * perCorner + posOff)];
                        if (pi >= 0 && pi * posStride + 2 < (int64)posSrc->data.Size())
                            v.pos = { (float32)posSrc->data[(NkVector<float64>::SizeType)(pi*posStride+0)],
                                      (float32)posSrc->data[(NkVector<float64>::SizeType)(pi*posStride+1)],
                                      (float32)posSrc->data[(NkVector<float64>::SizeType)(pi*posStride+2)] };
                        if (nrmSrc && nrmOff >= 0) {
                            int64 ni = pidx[(NkVector<int64>::SizeType)(corner * perCorner + nrmOff)];
                            if (ni >= 0 && ni * nrmStride + 2 < (int64)nrmSrc->data.Size()) {
                                v.normal = { (float32)nrmSrc->data[(NkVector<float64>::SizeType)(ni*nrmStride+0)],
                                             (float32)nrmSrc->data[(NkVector<float64>::SizeType)(ni*nrmStride+1)],
                                             (float32)nrmSrc->data[(NkVector<float64>::SizeType)(ni*nrmStride+2)] };
                                anyNormal = true;
                            }
                        }
                        if (uvSrc && uvOff >= 0) {
                            int64 ui = pidx[(NkVector<int64>::SizeType)(corner * perCorner + uvOff)];
                            if (ui >= 0 && ui * uvStride + 1 < (int64)uvSrc->data.Size())
                                v.uv = { (float32)uvSrc->data[(NkVector<float64>::SizeType)(ui*uvStride+0)],
                                         1.f - (float32)uvSrc->data[(NkVector<float64>::SizeType)(ui*uvStride+1)] };
                        }
                        v.tangent = {0.f,0.f,0.f}; v.uv2 = v.uv; v.color = white;
                        out.vertices.PushBack(v);
                        return (uint32)out.vertices.Size() - 1 - baseV;
                    };

                    if (isPoly && !vcount.Empty()) {
                        int32 corner = 0;
                        for (uint32 pgi = 0; pgi < (uint32)vcount.Size(); ++pgi) {
                            int32 vc = (int32)vcount[(NkVector<int64>::SizeType)pgi];
                            if (corner + vc > numCorners) break;
                            uint32 fan[256]; int32 fn = 0;
                            for (int32 k = 0; k < vc && fn < 256; ++k) fan[fn++] = emit(corner + k);
                            for (int32 k = 1; k + 1 < fn; ++k) {
                                out.indices.PushBack(fan[0]); out.indices.PushBack(fan[k]); out.indices.PushBack(fan[k+1]);
                            }
                            corner += vc;
                        }
                    } else {   // triangles : 3 corners consecutifs
                        for (int32 c = 0; c + 2 < numCorners; c += 3) {
                            uint32 a = emit(c), b = emit(c+1), cc = emit(c+2);
                            out.indices.PushBack(a); out.indices.PushBack(b); out.indices.PushBack(cc);
                        }
                    }

                    uint32 idxCount = (uint32)out.indices.Size() - subStart;
                    if (idxCount == 0) continue;
                    if (!anyNormal) {
                        for (uint32 vi = baseV; vi < (uint32)out.vertices.Size(); ++vi)
                            out.vertices[vi].normal = {0.f,0.f,0.f};
                        for (uint32 ii = subStart; ii + 2 < subStart + idxCount; ii += 3) {
                            uint32 a = out.indices[ii]+baseV, b = out.indices[ii+1]+baseV, cc = out.indices[ii+2]+baseV;
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
            }
        } // namespace

        bool LoadDAE(const NkString& path, NkGLTFMeshData& out) {
            NkString text = NkFile::ReadAllText(path.CStr());
            if (text.Empty()) {
                NkLog::Instance().Warnf("[NkDAELoader] fichier introuvable/vide : %s", path.CStr());
                return false;
            }
            XmlNode root;
            if (!ParseXML(text.CStr(), text.CStr() + text.Size(), root) || !(root.name == NkString("COLLADA"))) {
                NkLog::Instance().Warnf("[NkDAELoader] XML/COLLADA invalide : %s", path.CStr());
                return false;
            }

            // library_geometries/geometry/mesh
            uint32 meshCount = 0;
            if (const XmlNode* libGeo = root.Child("library_geometries")) {
                for (uint32 i = 0; i < (uint32)libGeo->children.Size(); ++i) {
                    const XmlNode& g = libGeo->children[(NkVector<XmlNode>::SizeType)i];
                    if (!(g.name == NkString("geometry"))) continue;
                    if (const XmlNode* mesh = g.Child("mesh")) { ExtractMesh(*mesh, out); ++meshCount; }
                }
            }
            if (out.vertices.Empty() || out.indices.Empty()) {
                NkLog::Instance().Warnf("[NkDAELoader] aucune geometrie exploitable : %s", path.CStr());
                return false;
            }

            // up_axis : asset/up_axis (Y_UP defaut, Z_UP -> conversion)
            bool zUp = false;
            if (const XmlNode* asset = root.Child("asset"))
                if (const XmlNode* up = asset->Child("up_axis"))
                    zUp = (up->text == NkString("Z_UP"));
            if (zUp) for (uint32 i = 0; i < (uint32)out.vertices.Size(); ++i) {
                NkVertex3D& v = out.vertices[i];
                v.pos    = { v.pos.x, v.pos.z, -v.pos.y };
                v.normal = { v.normal.x, v.normal.z, -v.normal.y };
            }

            ComputeBounds(out);
            out.debugName = path;
            NkLog::Instance().Infof("[NkDAELoader] OK '%s' : %u meshes, %u verts, %u indices, %u sous-meshes%s",
                                    path.CStr(), meshCount, (uint32)out.vertices.Size(),
                                    (uint32)out.indices.Size(), (uint32)out.subMeshes.Size(),
                                    zUp ? " (Z_UP->Y)" : "");
            return true;
        }

    } // namespace renderer
} // namespace nkentseu
