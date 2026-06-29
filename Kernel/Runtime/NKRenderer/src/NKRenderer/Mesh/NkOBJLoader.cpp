// =============================================================================
// NkOBJLoader.cpp — NKRenderer (Mesh/)  — Wavefront OBJ + MTL, zero-STL.
// Voir NkOBJLoader.h pour le perimetre.
// =============================================================================
#include "NkOBJLoader.h"
#include "NkMeshLoaderUtil.h"

#include "NKLogger/NkLog.h"
#include "NKFileSystem/NkFile.h"
#include "NKContainers/Associative/NkHashMap.h"

namespace nkentseu {
    namespace renderer {

        namespace {
            using namespace meshutil;

            // Dossier parent d'un chemin (sans le separateur final). "" si aucun.
            NkString DirOf(const NkString& path) {
                int32 cut = -1;
                for (int32 i = 0; i < (int32)path.Size(); ++i) {
                    char c = path[(NkString::SizeType)i];
                    if (c == '/' || c == '\\') cut = i;
                }
                NkString d;
                for (int32 i = 0; i < cut; ++i) d.Append(path[(NkString::SizeType)i]);
                return d;
            }

            NkString JoinPath(const NkString& dir, const NkString& name) {
                if (dir.Empty()) return name;
                NkString full = dir;
                if (!full.EndsWith('/') && !full.EndsWith('\\')) full.Append('/');
                full.Append(name);
                return full;
            }

            // Compare le debut d'une ligne a un mot-cle suivi d'un espace/tab.
            bool LineIs(const char* p, const char* end, const char* kw) {
                while (*kw) { if (p >= end || *p != *kw) return false; ++p; ++kw; }
                return (p >= end) || IsWS(*p);
            }

            // Index OBJ 1-based/negatif -> 0-based. count = nb d'elements deja lus.
            int32 ResolveIndex(int32 raw, int32 count) {
                if (raw > 0) return raw - 1;
                if (raw < 0) return count + raw;   // -1 = dernier
                return -1;                          // 0 = absent
            }

            // ── Parse le .mtl : remplit out.materials + out.images, et la table
            //    name->index. Charge les textures relativement a `dir`. ──────────
            void ParseMTL(const NkString& mtlPath, const NkString& dir,
                          NkGLTFMeshData& out, NkVector<NkString>& matNames) {
                NkString text = NkFile::ReadAllText(mtlPath.CStr());
                if (text.Empty()) {
                    NkLog::Instance().Warnf("[NkOBJLoader] .mtl introuvable/vide : %s", mtlPath.CStr());
                    return;
                }
                const char* p   = text.CStr();
                const char* end = p + text.Size();
                NkGLTFMaterial* cur = nullptr;

                auto loadTex = [&](const NkString& name) -> int32 {
                    NkString full = JoinPath(dir, name);
                    NkGLTFImage img;
                    if (img.decoded.Load(full.CStr(), 4)) {
                        img.valid = img.decoded.IsValid();
                        img.uri   = name;
                        out.images.PushBack(static_cast<NkGLTFImage&&>(img));
                        return (int32)out.images.Size() - 1;
                    }
                    NkLog::Instance().Warnf("[NkOBJLoader] texture introuvable : %s", full.CStr());
                    return -1;
                };

                while (p < end) {
                    SkipHSpace(p, end);
                    if (p < end && *p == '#') { SkipToEOL(p, end); if (p < end) ++p; continue; }

                    if (LineIs(p, end, "newmtl")) {
                        p += 6;
                        NkString nm; ReadToken(p, end, nm);
                        NkGLTFMaterial m; m.name = nm;
                        m.metallicFactor = 0.f;          // OBJ/MTL = surfaces non-PBR -> dielectrique
                        m.roughnessFactor = 0.8f;
                        out.materials.PushBack(static_cast<NkGLTFMaterial&&>(m));
                        matNames.PushBack(nm);
                        cur = &out.materials[(NkVector<NkGLTFMaterial>::SizeType)out.materials.Size() - 1];
                    } else if (cur && LineIs(p, end, "Kd")) {
                        p += 2;
                        cur->baseColorFactor.x = ParseFloat(p, end);
                        cur->baseColorFactor.y = ParseFloat(p, end);
                        cur->baseColorFactor.z = ParseFloat(p, end);
                    } else if (cur && LineIs(p, end, "Ke")) {
                        p += 2;
                        cur->emissiveFactor.x = ParseFloat(p, end);
                        cur->emissiveFactor.y = ParseFloat(p, end);
                        cur->emissiveFactor.z = ParseFloat(p, end);
                    } else if (cur && LineIs(p, end, "Ns")) {
                        p += 2;
                        float32 ns = ParseFloat(p, end);       // shininess Phong [0..1000]
                        if (ns < 0.f) ns = 0.f; if (ns > 1000.f) ns = 1000.f;
                        cur->roughnessFactor = std::sqrt(2.f / (ns + 2.f)); // approx GGX
                        if (cur->roughnessFactor > 1.f) cur->roughnessFactor = 1.f;
                    } else if (cur && (LineIs(p, end, "d"))) {
                        p += 1; cur->baseColorFactor.w = ParseFloat(p, end);
                    } else if (cur && LineIs(p, end, "Tr")) {
                        p += 2; cur->baseColorFactor.w = 1.f - ParseFloat(p, end);
                    } else if (cur && LineIs(p, end, "map_Kd")) {
                        p += 6; NkString nm; ReadToken(p, end, nm);
                        cur->baseColorImage = loadTex(nm);
                    } else if (cur && LineIs(p, end, "map_Ke")) {
                        p += 6; NkString nm; ReadToken(p, end, nm);
                        cur->emissiveImage = loadTex(nm);
                    } else if (cur && (LineIs(p, end, "map_Bump") || LineIs(p, end, "bump") || LineIs(p, end, "norm"))) {
                        // saute jusqu'au dernier token de la ligne (le nom de fichier)
                        SkipToEOL(p, end);
                        const char* ls = p; // p est sur '\n'; recule pour relire... simplifie:
                        (void)ls;
                        // (Normal map differee : map_Bump peut avoir des options -bm etc.)
                        // On laisse normalImage=-1 pour le MVP.
                    }
                    SkipToEOL(p, end);
                    if (p < end) ++p;
                }
            }
        } // namespace

        bool LoadOBJ(const NkString& path, NkGLTFMeshData& out) {
            NkString text = NkFile::ReadAllText(path.CStr());
            if (text.Empty()) {
                NkLog::Instance().Warnf("[NkOBJLoader] fichier introuvable/vide : %s", path.CStr());
                return false;
            }
            const NkString dir = DirOf(path);

            NkVector<NkVec3f> positions;
            NkVector<NkVec2f> texcoords;
            NkVector<NkVec3f> normals;
            NkVector<NkString> matNames;             // parallele a out.materials
            NkHashMap<uint64, uint32> vcache;        // (vi,ti,ni) packe -> index vertex

            bool anyNormalRef = false;
            int32 curMat = -1;                       // index materiau courant (-1 = aucun)

            // Sous-mesh courant
            bool   haveSub   = false;
            uint32 subStart  = 0;
            int32  subMat    = -1;

            auto closeSub = [&]() {
                if (!haveSub) return;
                uint32 cnt = (uint32)out.indices.Size() - subStart;
                if (cnt == 0) return;               // sous-mesh vide -> ignore
                NkSubMesh sm;
                sm.firstIndex = subStart;
                sm.indexCount = cnt;
                sm.baseVertex = 0;
                out.subMeshes.PushBack(sm);
                out.subMeshMaterial.PushBack(subMat);
            };

            const char* p   = text.CStr();
            const char* end = p + text.Size();

            while (p < end) {
                SkipHSpace(p, end);
                if (p >= end) break;
                char c0 = *p;

                if (c0 == '#' || c0 == '\n') { SkipToEOL(p, end); if (p < end) ++p; continue; }

                if (c0 == 'v' && (p + 1 < end) && (p[1] == ' ' || p[1] == '\t')) {
                    p += 1; NkVec3f v;
                    v.x = ParseFloat(p, end); v.y = ParseFloat(p, end); v.z = ParseFloat(p, end);
                    positions.PushBack(v);
                } else if (c0 == 'v' && (p + 1 < end) && p[1] == 't') {
                    p += 2; NkVec2f t;
                    t.x = ParseFloat(p, end); t.y = ParseFloat(p, end);
                    texcoords.PushBack(t);
                } else if (c0 == 'v' && (p + 1 < end) && p[1] == 'n') {
                    p += 2; NkVec3f n;
                    n.x = ParseFloat(p, end); n.y = ParseFloat(p, end); n.z = ParseFloat(p, end);
                    normals.PushBack(n);
                } else if (c0 == 'f' && (p + 1 < end) && (p[1] == ' ' || p[1] == '\t')) {
                    p += 1;
                    // Ouvre un sous-mesh si necessaire (sur changement de materiau).
                    if (!haveSub || subMat != curMat) {
                        closeSub();
                        haveSub = true; subStart = (uint32)out.indices.Size(); subMat = curMat;
                    }
                    // Parse tous les corners de la face -> triangulation en fan.
                    uint32 faceIdx[64]; int32 faceN = 0;
                    while (faceN < 64) {
                        SkipHSpace(p, end);
                        if (p >= end || *p == '\n') break;
                        if (!((*p >= '0' && *p <= '9') || *p == '-' || *p == '+')) break;
                        int32 rv = ParseInt(p, end), rt = 0, rn = 0;
                        if (p < end && *p == '/') {
                            ++p;
                            if (p < end && *p != '/') rt = ParseInt(p, end);
                            if (p < end && *p == '/') { ++p; rn = ParseInt(p, end); }
                        }
                        int32 vi = ResolveIndex(rv, (int32)positions.Size());
                        int32 ti = ResolveIndex(rt, (int32)texcoords.Size());
                        int32 ni = ResolveIndex(rn, (int32)normals.Size());
                        if (ni >= 0) anyNormalRef = true;
                        if (vi < 0 || vi >= (int32)positions.Size()) continue;

                        uint64 key = ((uint64)((uint32)(vi + 1) & 0x1FFFFFu) << 42)
                                   | ((uint64)((uint32)(ti + 1) & 0x1FFFFFu) << 21)
                                   | ((uint64)((uint32)(ni + 1) & 0x1FFFFFu));
                        uint32 outIdx;
                        if (uint32* found = vcache.Find(key)) {
                            outIdx = *found;
                        } else {
                            NkVertex3D vert{};
                            vert.pos = positions[(NkVector<NkVec3f>::SizeType)vi];
                            vert.normal = (ni >= 0 && ni < (int32)normals.Size())
                                        ? normals[(NkVector<NkVec3f>::SizeType)ni] : NkVec3f{0.f, 0.f, 0.f};
                            vert.tangent = {0.f, 0.f, 0.f};   // fallback shader
                            if (ti >= 0 && ti < (int32)texcoords.Size()) {
                                NkVec2f tc = texcoords[(NkVector<NkVec2f>::SizeType)ti];
                                vert.uv = {tc.x, 1.f - tc.y};  // OBJ origine bas-gauche -> flip V
                            } else vert.uv = {0.f, 0.f};
                            vert.uv2 = vert.uv;
                            vert.color = PackRGBA(255, 255, 255);
                            out.vertices.PushBack(vert);
                            outIdx = (uint32)out.vertices.Size() - 1;
                            vcache.Insert(key, outIdx);
                        }
                        faceIdx[faceN++] = outIdx;
                    }
                    // Fan : (0,i,i+1)
                    for (int32 i = 1; i + 1 < faceN; ++i) {
                        out.indices.PushBack(faceIdx[0]);
                        out.indices.PushBack(faceIdx[i]);
                        out.indices.PushBack(faceIdx[i + 1]);
                    }
                } else if (LineIs(p, end, "usemtl")) {
                    p += 6; NkString nm; ReadToken(p, end, nm);
                    curMat = -1;
                    for (uint32 i = 0; i < (uint32)matNames.Size(); ++i)
                        if (matNames[i] == nm) { curMat = (int32)i; break; }
                } else if (LineIs(p, end, "mtllib")) {
                    p += 6; NkString nm; ReadToken(p, end, nm);
                    ParseMTL(JoinPath(dir, nm), dir, out, matNames);
                }
                SkipToEOL(p, end);
                if (p < end) ++p;
            }
            closeSub();

            if (out.vertices.Empty() || out.indices.Empty()) {
                NkLog::Instance().Warnf("[NkOBJLoader] aucune geometrie dans %s", path.CStr());
                return false;
            }
            // Si le fichier ne fournit aucune normale, on les calcule (lissees).
            if (!anyNormalRef) GenerateSmoothNormals(out);

            // Au moins un sous-mesh (fallback : tout).
            if (out.subMeshes.Empty()) {
                NkSubMesh sm; sm.firstIndex = 0; sm.indexCount = (uint32)out.indices.Size();
                out.subMeshes.PushBack(sm); out.subMeshMaterial.PushBack(-1);
            }

            ComputeBounds(out);
            out.debugName = path;

            NkLog::Instance().Infof("[NkOBJLoader] OK '%s' : %u verts, %u indices, %u sous-meshes, %u materiaux, %u images",
                          path.CStr(), (uint32)out.vertices.Size(), (uint32)out.indices.Size(),
                          (uint32)out.subMeshes.Size(), (uint32)out.materials.Size(),
                          (uint32)out.images.Size());
            return true;
        }

    } // namespace renderer
} // namespace nkentseu
