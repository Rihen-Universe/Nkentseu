// =============================================================================
// NkGLTFLoader.cpp  — NKRenderer v4.0
//
// Loader glTF 2.0 from-scratch (MVP geometrie), zero-STL / NKMemory.
// Voir NkGLTFLoader.h pour le perimetre supporte / differe.
// =============================================================================
#include "NkGLTFLoader.h"

#include "NKLogger/NkLog.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/NkArchive.h"

#include <cmath>
#include <cstring>

namespace nkentseu {
    namespace renderer {

        namespace {

            // ── Codes glTF ────────────────────────────────────────────────────
            constexpr uint32 GLTF_GLB_MAGIC      = 0x46546C67u;   // 'glTF'
            constexpr uint32 GLTF_CHUNK_JSON     = 0x4E4F534Au;   // 'JSON'
            constexpr uint32 GLTF_CHUNK_BIN      = 0x004E4942u;   // 'BIN\0'

            constexpr int32 CT_BYTE           = 5120; // int8
            constexpr int32 CT_UNSIGNED_BYTE  = 5121; // uint8
            constexpr int32 CT_SHORT          = 5122; // int16
            constexpr int32 CT_UNSIGNED_SHORT = 5123; // uint16
            constexpr int32 CT_UNSIGNED_INT   = 5125; // uint32
            constexpr int32 CT_FLOAT          = 5126; // float32

            // Nombre de composantes selon le type glTF.
            uint32 TypeComponentCount(const NkString& t) {
                if (t == NkString("SCALAR")) return 1;
                if (t == NkString("VEC2"))   return 2;
                if (t == NkString("VEC3"))   return 3;
                if (t == NkString("VEC4"))   return 4;
                if (t == NkString("MAT2"))   return 4;
                if (t == NkString("MAT3"))   return 9;
                if (t == NkString("MAT4"))   return 16;
                return 0;
            }

            // Taille en octets d'une composante selon le componentType.
            uint32 ComponentByteSize(int32 ct) {
                switch (ct) {
                    case CT_BYTE:
                    case CT_UNSIGNED_BYTE:  return 1;
                    case CT_SHORT:
                    case CT_UNSIGNED_SHORT: return 2;
                    case CT_UNSIGNED_INT:
                    case CT_FLOAT:          return 4;
                    default:                return 0;
                }
            }

            // ── Buffer brut (un par glTF "buffers[]") ─────────────────────────
            struct RawBuffer {
                NkVector<uint8> data;
            };

            // ── Decodeur base64 zero-STL ──────────────────────────────────────
            // Decode les `len` caracteres b64 a partir de `src` dans `out`.
            // Tolerant : ignore whitespace, gere le padding '='.
            bool Base64Decode(const char* src, nk_size len, NkVector<uint8>& out) {
                auto val = [](char c) -> int {
                    if (c >= 'A' && c <= 'Z') return c - 'A';
                    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
                    if (c >= '0' && c <= '9') return c - '0' + 52;
                    if (c == '+') return 62;
                    if (c == '/') return 63;
                    return -1; // '=' ou invalide
                };
                uint32 acc = 0;
                int    bits = 0;
                for (nk_size i = 0; i < len; ++i) {
                    char c = src[i];
                    if (c == '=' ) break;
                    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
                    int v = val(c);
                    if (v < 0) continue; // ignore caractere inconnu
                    acc = (acc << 6) | (uint32)v;
                    bits += 6;
                    if (bits >= 8) {
                        bits -= 8;
                        out.PushBack((uint8)((acc >> bits) & 0xFFu));
                    }
                }
                return !out.Empty();
            }

            // ── Helpers de navigation NkArchive (JSON arbitraire) ─────────────
            // On manipule directement les NkArchiveNode bruts car glTF imbrique
            // objets et tableaux de facon arbitraire.

            const NkArchiveNode* ObjFind(const NkArchive& obj, const char* key) {
                return obj.FindNode(NkStringView(key));
            }

            // Recupere le tableau (NkArchiveNode array) pour une cle, ou nullptr.
            const NkVector<NkArchiveNode>* GetArrayNode(const NkArchive& obj, const char* key) {
                const NkArchiveNode* n = obj.FindNode(NkStringView(key));
                if (!n || !n->IsArray()) return nullptr;
                return &n->array;
            }

            // Lit un int depuis un noeud scalaire. Retourne def si absent/non-num.
            int64 NodeAsInt(const NkArchiveNode* n, int64 def) {
                if (!n || !n->IsScalar()) return def;
                const NkArchiveValue& v = n->value;
                if (v.IsInt())   return v.raw.i;
                if (v.IsUInt())  return (int64)v.raw.u;
                if (v.IsFloat()) return (int64)v.raw.f;
                // fallback : parse texte
                int64 out = def;
                if (!v.text.Empty()) {
                    NkString t = v.text;
                    if (t.ToInt64(out)) return out;
                }
                return def;
            }

            // Lit un int dans un objet via cle.
            int64 ObjGetInt(const NkArchive& obj, const char* key, int64 def) {
                return NodeAsInt(obj.FindNode(NkStringView(key)), def);
            }

            // Lit une string dans un objet via cle.
            bool ObjGetString(const NkArchive& obj, const char* key, NkString& out) {
                const NkArchiveNode* n = obj.FindNode(NkStringView(key));
                if (!n || !n->IsScalar() || !n->value.IsString()) return false;
                out = n->value.text;
                return true;
            }

            // Recupere l'objet imbrique a l'index `idx` d'un tableau d'objets.
            const NkArchive* ArrayObjAt(const NkVector<NkArchiveNode>& arr, nk_size idx) {
                if (idx >= arr.Size()) return nullptr;
                const NkArchiveNode& n = arr[idx];
                if (!n.IsObject()) return nullptr;
                return n.object;
            }

            // Forward : defini plus bas, utilise par ParseGLTFNodes.
            uint32 ObjGetFloatArray(const NkArchive& obj, const char* key,
                                    float32* dst, uint32 n);

            // Transforme une DIRECTION (w=0) par la 3x3 d'une NkMat4f column-major.
            // (NkMat4f::TransformVector retourne un Vec4 -> conversion ambigue ;
            //  on multiplie a la main, sans translation.)
            NkVec3f TransformDir3(const NkMat4f& m, const NkVec3f& d) {
                return {
                    m[0][0]*d.x + m[1][0]*d.y + m[2][0]*d.z,
                    m[0][1]*d.x + m[1][1]*d.y + m[2][1]*d.z,
                    m[0][2]*d.x + m[1][2]*d.y + m[2][2]*d.z
                };
            }

            // Compose la matrice locale d'un node a partir de ses TRS (ou matrix).
            // Partagee entre le baking de scene-graph (LoadGLTF) et EvaluateGLTFPose.
            NkMat4f NodeLocalMat(const NkGLTFNode& n) {
                if (n.hasMatrix) return n.matrix;
                NkQuatf q(n.rotation.x, n.rotation.y, n.rotation.z, n.rotation.w);
                NkMat4f R = static_cast<NkMat4f>(q);
                NkMat4f T = NkMat4f::Translate(n.translation);
                NkMat4f S = NkMat4f::Scale(n.scale);
                return T * R * S;
            }

            // Parse la section glTF `nodes[]` dans `outNodes` (TRS/matrix + mesh +
            // children). Reutilise par le baking scene-graph ET le skinning.
            void ParseGLTFNodes(const NkVector<NkArchiveNode>* nodesArr,
                                NkVector<NkGLTFNode>& outNodes) {
                outNodes.Clear();
                if (!nodesArr) return;
                outNodes.Resize(nodesArr->Size());
                for (nk_size ni = 0; ni < nodesArr->Size(); ++ni) {
                    const NkArchive* node = ArrayObjAt(*nodesArr, ni);
                    NkGLTFNode gn;
                    if (node) {
                        float32 m[16];
                        if (ObjGetFloatArray(*node, "matrix", m, 16) == 16) {
                            gn.hasMatrix = true;
                            gn.matrix    = NkMat4f(m);   // column-major
                        } else {
                            float32 t[3] = {0,0,0}, s[3] = {1,1,1}, r[4] = {0,0,0,1};
                            if (ObjGetFloatArray(*node, "translation", t, 3) == 3)
                                gn.translation = {t[0],t[1],t[2]};
                            if (ObjGetFloatArray(*node, "scale", s, 3) == 3)
                                gn.scale = {s[0],s[1],s[2]};
                            if (ObjGetFloatArray(*node, "rotation", r, 4) == 4)
                                gn.rotation = {r[0],r[1],r[2],r[3]};  // x,y,z,w
                        }
                        gn.mesh = (int32)ObjGetInt(*node, "mesh", -1);
                        const NkArchiveNode* ch = node->FindNode(NkStringView("children"));
                        if (ch && ch->IsArray()) {
                            for (nk_size ci = 0; ci < ch->array.Size(); ++ci)
                                gn.children.PushBack((int32)NodeAsInt(&ch->array[ci], -1));
                        }
                    }
                    outNodes[ni] = traits::NkMove(gn);
                }
            }

            // Calcule la world matrix de chaque mesh (index dans meshes[]) en
            // parcourant le scene-graph en DFS depuis les racines de la scene.
            // worldChild = worldParent * NodeLocal(node). Pour chaque node ayant
            // mesh >= 0, enregistre meshWorld[node.mesh] (1er node gagne =
            // instancing non supporte). Defaut = identite si non reference.
            void ComputeMeshWorldMatrices(const NkArchive& root,
                                          const NkVector<NkGLTFNode>& nodes,
                                          uint32 meshCount,
                                          NkVector<NkMat4f>& meshWorld,
                                          NkVector<bool>&    meshAssigned) {
                meshWorld.Clear();    meshWorld.Resize(meshCount);
                meshAssigned.Clear(); meshAssigned.Resize(meshCount);
                for (uint32 i = 0; i < meshCount; ++i) {
                    meshWorld[i]    = NkMat4f::Identity();
                    meshAssigned[i] = false;
                }
                const uint32 nodeCount = (uint32)nodes.Size();
                if (nodeCount == 0) return;

                // Racines de scene : scenes[scene].nodes[] (scene = root.scene|0).
                NkVector<int32> roots;
                int32 sceneIdx = (int32)ObjGetInt(root, "scene", 0);
                const NkVector<NkArchiveNode>* scenesArr = GetArrayNode(root, "scenes");
                if (scenesArr && sceneIdx >= 0 && (uint32)sceneIdx < scenesArr->Size()) {
                    const NkArchive* sc = ArrayObjAt(*scenesArr, (nk_size)sceneIdx);
                    if (sc) {
                        const NkArchiveNode* sn = sc->FindNode(NkStringView("nodes"));
                        if (sn && sn->IsArray())
                            for (nk_size i = 0; i < sn->array.Size(); ++i)
                                roots.PushBack((int32)NodeAsInt(&sn->array[i], -1));
                    }
                }
                // Fallback : aucune scene -> tous les nodes non-enfants sont racines.
                if (roots.Empty()) {
                    NkVector<bool> isChild; isChild.Resize(nodeCount);
                    for (uint32 i = 0; i < nodeCount; ++i) isChild[i] = false;
                    for (uint32 i = 0; i < nodeCount; ++i)
                        for (uint32 c = 0; c < (uint32)nodes[i].children.Size(); ++c) {
                            int32 ci = nodes[i].children[c];
                            if (ci >= 0 && ci < (int32)nodeCount) isChild[ci] = true;
                        }
                    for (uint32 i = 0; i < nodeCount; ++i)
                        if (!isChild[i]) roots.PushBack((int32)i);
                }

                // DFS iteratif (pile) : evite la recursion (zero-STL friendly).
                struct Frame { int32 node; NkMat4f world; };
                NkVector<Frame> stack;
                NkVector<bool> visited; visited.Resize(nodeCount);
                for (uint32 i = 0; i < nodeCount; ++i) visited[i] = false;
                for (uint32 r = 0; r < (uint32)roots.Size(); ++r) {
                    int32 rn = roots[r];
                    if (rn < 0 || rn >= (int32)nodeCount) continue;
                    Frame f; f.node = rn; f.world = NodeLocalMat(nodes[(uint32)rn]);
                    stack.PushBack(f);
                    while (!stack.Empty()) {
                        Frame cur = stack[stack.Size() - 1];
                        stack.PopBack();
                        if (cur.node < 0 || cur.node >= (int32)nodeCount) continue;
                        if (visited[(uint32)cur.node]) continue; // garde anti-cycle
                        visited[(uint32)cur.node] = true;
                        const NkGLTFNode& nd = nodes[(uint32)cur.node];
                        int32 mi = nd.mesh;
                        if (mi >= 0 && (uint32)mi < meshCount) {
                            if (!meshAssigned[(uint32)mi]) {
                                meshWorld[(uint32)mi]    = cur.world;
                                meshAssigned[(uint32)mi] = true;
                            } else {
                                logger.Warnf("[NkGLTFLoader] instancing non supporte "
                                             "(mesh %d reference par plusieurs nodes, "
                                             "1er node retenu)\n", mi);
                            }
                        }
                        for (uint32 c = 0; c < (uint32)nd.children.Size(); ++c) {
                            int32 ci = nd.children[c];
                            if (ci < 0 || ci >= (int32)nodeCount) continue;
                            Frame cf;
                            cf.node  = ci;
                            cf.world = cur.world * NodeLocalMat(nodes[(uint32)ci]);
                            stack.PushBack(cf);
                        }
                    }
                }
            }

            // ── Description decodee d'un accessor / bufferView ────────────────
            struct AccessorInfo {
                bool   valid       = false;
                int32  componentType = 0;
                uint32 compCount   = 0;       // composantes par element (VEC3 -> 3)
                uint32 count       = 0;       // nombre d'elements
                bool   normalized  = false;
                nk_size byteOffset = 0;       // offset absolu dans le buffer
                uint32 stride      = 0;       // stride en octets entre elements
                int32  buffer      = -1;      // index du buffer source
            };

            // Resout un accessor (index) -> AccessorInfo absolu (offset/stride/buffer).
            AccessorInfo ResolveAccessor(const NkVector<NkArchiveNode>& accessors,
                                         const NkVector<NkArchiveNode>& bufferViews,
                                         int64 accIdx) {
                AccessorInfo info;
                if (accIdx < 0) return info;
                const NkArchive* acc = ArrayObjAt(accessors, (nk_size)accIdx);
                if (!acc) return info;

                int64 bvIdx       = ObjGetInt(*acc, "bufferView", -1);
                info.componentType = (int32)ObjGetInt(*acc, "componentType", 0);
                info.count        = (uint32)ObjGetInt(*acc, "count", 0);
                int64 accByteOff  = ObjGetInt(*acc, "byteOffset", 0);
                info.normalized   = (ObjGetInt(*acc, "normalized", 0) != 0);

                NkString typeStr;
                ObjGetString(*acc, "type", typeStr);
                info.compCount = TypeComponentCount(typeStr);

                if (info.compCount == 0 || info.count == 0 || bvIdx < 0) return info;

                const NkArchive* bv = ArrayObjAt(bufferViews, (nk_size)bvIdx);
                if (!bv) return info;

                info.buffer       = (int32)ObjGetInt(*bv, "buffer", -1);
                int64 bvByteOff   = ObjGetInt(*bv, "byteOffset", 0);
                int64 bvByteStride = ObjGetInt(*bv, "byteStride", 0);

                uint32 elemSize = ComponentByteSize(info.componentType) * info.compCount;
                info.stride     = (bvByteStride > 0) ? (uint32)bvByteStride : elemSize;
                info.byteOffset = (nk_size)(bvByteOff + accByteOff);
                info.valid      = (info.buffer >= 0);
                return info;
            }

            // Lit l'element `i`, composante `c`, d'un accessor float -> float32.
            // Gere la conversion depuis int normalise/non-normalise.
            float32 ReadAccessorFloat(const RawBuffer& buf, const AccessorInfo& a,
                                      uint32 i, uint32 c) {
                nk_size off = a.byteOffset + (nk_size)i * a.stride
                            + (nk_size)c * ComponentByteSize(a.componentType);
                if (off + ComponentByteSize(a.componentType) > buf.data.Size()) return 0.f;
                const uint8* p = buf.data.Data() + off;
                switch (a.componentType) {
                    case CT_FLOAT: {
                        float32 f; memcpy(&f, p, 4); return f;
                    }
                    case CT_UNSIGNED_BYTE: {
                        uint8 v = *p;
                        return a.normalized ? (float32)v / 255.f : (float32)v;
                    }
                    case CT_BYTE: {
                        int8 v = (int8)*p;
                        return a.normalized ? (float32)(v < -127 ? -1.f : v / 127.f) : (float32)v;
                    }
                    case CT_UNSIGNED_SHORT: {
                        uint16 v; memcpy(&v, p, 2);
                        return a.normalized ? (float32)v / 65535.f : (float32)v;
                    }
                    case CT_SHORT: {
                        int16 v; memcpy(&v, p, 2);
                        return a.normalized ? (float32)(v < -32767 ? -1.f : v / 32767.f) : (float32)v;
                    }
                    case CT_UNSIGNED_INT: {
                        uint32 v; memcpy(&v, p, 4); return (float32)v;
                    }
                    default: return 0.f;
                }
            }

            // Lit l'element `i` d'un accessor scalaire entier -> uint32 (indices).
            uint32 ReadAccessorIndex(const RawBuffer& buf, const AccessorInfo& a, uint32 i) {
                nk_size off = a.byteOffset + (nk_size)i * a.stride;
                uint32 cs = ComponentByteSize(a.componentType);
                if (off + cs > buf.data.Size()) return 0;
                const uint8* p = buf.data.Data() + off;
                switch (a.componentType) {
                    case CT_UNSIGNED_BYTE:  return (uint32)(*p);
                    case CT_UNSIGNED_SHORT: { uint16 v; memcpy(&v, p, 2); return (uint32)v; }
                    case CT_UNSIGNED_INT:   { uint32 v; memcpy(&v, p, 4); return v; }
                    case CT_SHORT:          { int16 v; memcpy(&v, p, 2); return (uint32)(v < 0 ? 0 : v); }
                    case CT_BYTE:           { int8 v = (int8)*p; return (uint32)(v < 0 ? 0 : v); }
                    default: return 0;
                }
            }

            inline uint32 PackRGBA8(uint8 r, uint8 g, uint8 b, uint8 a) {
                return ((uint32)a << 24) | ((uint32)b << 16) | ((uint32)g << 8) | r;
            }

            // Lit l'element `i` composante `c` d'un accessor entier (JOINTS_0) ->
            // uint32 brut (ubyte/ushort sans normalisation).
            uint32 ReadAccessorUInt(const RawBuffer& buf, const AccessorInfo& a,
                                    uint32 i, uint32 c) {
                nk_size off = a.byteOffset + (nk_size)i * a.stride
                            + (nk_size)c * ComponentByteSize(a.componentType);
                uint32 cs = ComponentByteSize(a.componentType);
                if (off + cs > buf.data.Size()) return 0;
                const uint8* p = buf.data.Data() + off;
                switch (a.componentType) {
                    case CT_UNSIGNED_BYTE:  return (uint32)(*p);
                    case CT_UNSIGNED_SHORT: { uint16 v; memcpy(&v, p, 2); return (uint32)v; }
                    case CT_UNSIGNED_INT:   { uint32 v; memcpy(&v, p, 4); return v; }
                    default: return 0;
                }
            }

            // Lit un accessor MAT4 (inverseBindMatrices) -> NkMat4f.
            // glTF stocke les matrices en COLUMN-MAJOR (16 floats col0..col3),
            // ce qui correspond au stockage interne de NkMat4f (constructeur
            // 16-args = column-major, cf. cast quaternion). On copie tel quel.
            NkMat4f ReadAccessorMat4(const RawBuffer& buf, const AccessorInfo& a, uint32 i) {
                float32 m[16];
                for (uint32 c = 0; c < 16; ++c) m[c] = ReadAccessorFloat(buf, a, i, c);
                // NkMat4f(const T*) prend 16 floats column-major (data[] lineaire).
                return NkMat4f(m);
            }

            // Resout l'URI d'un buffer : data-URI base64, ou fichier .bin relatif.
            bool ResolveBufferURI(const NkString& uri, const NkString& baseDir,
                                  RawBuffer& out) {
                // data URI : "data:application/octet-stream;base64,XXXX"
                if (uri.StartsWith("data:")) {
                    nk_size comma = uri.Find(',');
                    if (comma == NkString::npos) return false;
                    const char* b64 = uri.CStr() + comma + 1;
                    nk_size      blen = uri.Length() - comma - 1;
                    return Base64Decode(b64, blen, out.data);
                }
                // Fichier externe (URI relatif au .gltf). On joint manuellement
                // pour ne dependre que de l'API string (evite les overloads
                // NkPath::operator/ qui peuvent ne pas etre linkes selon l'app).
                NkString full = baseDir;
                if (!full.Empty() && !full.EndsWith('/') && !full.EndsWith('\\')) {
                    full.Append('/');
                }
                full.Append(uri);
                NkVector<uint8> bytes = NkFile::ReadAllBytes(full.CStr());
                if (bytes.Empty()) {
                    logger.Warnf("[NkGLTFLoader] buffer externe introuvable : %s\n",
                                 full.CStr());
                    return false;
                }
                out.data = traits::NkMove(bytes);
                return true;
            }

            // Parse le conteneur .glb : extrait le JSON + le chunk BIN (buffer[0]).
            bool ParseGLB(const NkVector<uint8>& file, NkString& outJson,
                          NkVector<uint8>& outBin) {
                if (file.Size() < 12) return false;
                const uint8* p = file.Data();
                uint32 magic, version, length;
                memcpy(&magic,   p + 0, 4);
                memcpy(&version, p + 4, 4);
                memcpy(&length,  p + 8, 4);
                if (magic != GLTF_GLB_MAGIC) return false;
                (void)version;

                nk_size pos = 12;
                bool haveJson = false;
                while (pos + 8 <= file.Size()) {
                    uint32 chunkLen, chunkType;
                    memcpy(&chunkLen,  p + pos + 0, 4);
                    memcpy(&chunkType, p + pos + 4, 4);
                    pos += 8;
                    if (pos + chunkLen > file.Size()) break;
                    if (chunkType == GLTF_CHUNK_JSON) {
                        outJson = NkString((const char*)(p + pos), (nk_size)chunkLen);
                        haveJson = true;
                    } else if (chunkType == GLTF_CHUNK_BIN) {
                        outBin.Resize((nk_size)chunkLen);
                        if (chunkLen > 0) memcpy(outBin.Data(), p + pos, chunkLen);
                    }
                    pos += chunkLen;
                    // padding 4 octets
                    while ((pos & 3u) != 0 && pos < file.Size()) ++pos;
                }
                return haveJson;
            }

            // ── Lecture d'un float depuis un objet via cle ────────────────────
            float32 ObjGetFloat(const NkArchive& obj, const char* key, float32 def) {
                const NkArchiveNode* n = obj.FindNode(NkStringView(key));
                if (!n || !n->IsScalar()) return def;
                const NkArchiveValue& v = n->value;
                if (v.IsFloat()) return (float32)v.raw.f;
                if (v.IsInt())   return (float32)v.raw.i;
                if (v.IsUInt())  return (float32)v.raw.u;
                if (!v.text.Empty()) {
                    float32 out = def;
                    if (v.text.ToFloat(out)) return out;
                }
                return def;
            }

            // Lit un tableau de floats (VEC[n]) depuis une cle ; remplit dst[0..n-1].
            // Retourne le nombre de composantes effectivement lues.
            uint32 ObjGetFloatArray(const NkArchive& obj, const char* key,
                                    float32* dst, uint32 n) {
                const NkArchiveNode* node = obj.FindNode(NkStringView(key));
                if (!node || !node->IsArray()) return 0;
                const NkVector<NkArchiveNode>& arr = node->array;
                uint32 c = 0;
                for (; c < n && c < (uint32)arr.Size(); ++c) {
                    const NkArchiveNode& e = arr[c];
                    if (!e.IsScalar()) { dst[c] = 0.f; continue; }
                    const NkArchiveValue& v = e.value;
                    if (v.IsFloat())      dst[c] = (float32)v.raw.f;
                    else if (v.IsInt())   dst[c] = (float32)v.raw.i;
                    else if (v.IsUInt())  dst[c] = (float32)v.raw.u;
                    else                  dst[c] = 0.f;
                }
                return c;
            }

            // Recupere l'index de texture d'un sous-objet (ex. "baseColorTexture")
            // d'un objet glTF, puis suit textures[idx].source -> images[].
            // Retourne -1 si absent. `obj` est le conteneur (material ou pbr block).
            int32 ResolveTextureImage(const NkArchive& obj, const char* texKey,
                                      const NkVector<NkArchiveNode>* textures) {
                const NkArchiveNode* texNode = obj.FindNode(NkStringView(texKey));
                if (!texNode || !texNode->IsObject() || !textures) return -1;
                int64 texIdx = ObjGetInt(*texNode->object, "index", -1);
                if (texIdx < 0) return -1;
                const NkArchive* tex = ArrayObjAt(*textures, (nk_size)texIdx);
                if (!tex) return -1;
                return (int32)ObjGetInt(*tex, "source", -1);
            }

            // Decode une image glTF (entry de images[]) en RGBA8 dans out.decoded.
            // Sources possibles :
            //   - "uri" data:...;base64,XXX     -> base64 -> NKImage::LoadFromMemory
            //   - "uri" fichier relatif         -> NkFile::ReadAllBytes -> decode
            //   - "bufferView" (.glb / embarque)-> slice du buffer -> decode
            void DecodeGLTFImage(const NkArchive& imgObj, const NkString& baseDir,
                                 const NkVector<RawBuffer>& buffers,
                                 const NkVector<NkArchiveNode>* bufferViews,
                                 NkGLTFImage& out) {
                NkString uri;
                if (ObjGetString(imgObj, "uri", uri) && !uri.Empty()) {
                    out.uri = uri;
                    if (uri.StartsWith("data:")) {
                        // data URI base64 (image embarquee dans le .gltf)
                        nk_size comma = uri.Find(',');
                        if (comma == NkString::npos) return;
                        NkVector<uint8> bytes;
                        if (!Base64Decode(uri.CStr() + comma + 1,
                                          uri.Length() - comma - 1, bytes)) return;
                        if (out.decoded.LoadFromMemory(bytes.Data(),
                                                       (usize)bytes.Size(), 4)) {
                            out.valid = out.decoded.IsValid();
                        }
                    } else {
                        // Fichier externe relatif au .gltf
                        NkString full = baseDir;
                        if (!full.Empty() && !full.EndsWith('/') && !full.EndsWith('\\'))
                            full.Append('/');
                        full.Append(uri);
                        if (out.decoded.Load(full.CStr(), 4)) {
                            out.valid = out.decoded.IsValid();
                        } else {
                            logger.Warnf("[NkGLTFLoader] image introuvable : %s\n", full.CStr());
                        }
                    }
                    return;
                }

                // bufferView (.glb embarque) : on slice les octets puis on decode.
                int64 bvIdx = ObjGetInt(imgObj, "bufferView", -1);
                if (bvIdx < 0 || !bufferViews) return;
                const NkArchive* bv = ArrayObjAt(*bufferViews, (nk_size)bvIdx);
                if (!bv) return;
                int32 bufIdx   = (int32)ObjGetInt(*bv, "buffer", -1);
                int64 byteOff  = ObjGetInt(*bv, "byteOffset", 0);
                int64 byteLen  = ObjGetInt(*bv, "byteLength", 0);
                if (bufIdx < 0 || (uint32)bufIdx >= buffers.Size() || byteLen <= 0) return;
                const RawBuffer& buf = buffers[(uint32)bufIdx];
                if ((nk_size)(byteOff + byteLen) > buf.data.Size()) return;
                if (out.decoded.LoadFromMemory(buf.data.Data() + byteOff,
                                               (usize)byteLen, 4)) {
                    out.valid = out.decoded.IsValid();
                }
            }

        } // namespace anonyme

        // ── API publique ──────────────────────────────────────────────────────
        bool LoadGLTF(const NkString& path, NkGLTFMeshData& out) {
            NkPath p(path);
            NkString baseDir = p.GetDirectory();

            // 1) Lire le fichier
            NkVector<uint8> file = NkFile::ReadAllBytes(path.CStr());
            if (file.Empty()) {
                logger.Warnf("[NkGLTFLoader] fichier introuvable ou vide : %s\n", path.CStr());
                return false;
            }

            // 2) Detecter .glb vs .gltf
            NkString json;
            NkVector<uint8> glbBin;
            bool isGLB = false;
            if (file.Size() >= 4) {
                uint32 magic; memcpy(&magic, file.Data(), 4);
                isGLB = (magic == GLTF_GLB_MAGIC);
            }
            if (isGLB) {
                if (!ParseGLB(file, json, glbBin)) {
                    logger.Warnf("[NkGLTFLoader] GLB invalide : %s\n", path.CStr());
                    return false;
                }
            } else {
                json = NkString((const char*)file.Data(), file.Size());
            }

            // 3) Parser le JSON
            NkArchive root;
            NkString err;
            if (!NkJSONReader::ReadArchive(json.View(), root, &err)) {
                logger.Warnf("[NkGLTFLoader] JSON invalide (%s) : %s\n", err.CStr(), path.CStr());
                return false;
            }

            const NkVector<NkArchiveNode>* accessors   = GetArrayNode(root, "accessors");
            const NkVector<NkArchiveNode>* bufferViews = GetArrayNode(root, "bufferViews");
            const NkVector<NkArchiveNode>* buffersArr  = GetArrayNode(root, "buffers");
            const NkVector<NkArchiveNode>* meshes      = GetArrayNode(root, "meshes");

            if (!meshes || meshes->Empty()) {
                logger.Warnf("[NkGLTFLoader] aucun mesh dans : %s\n", path.CStr());
                return false;
            }
            if (!accessors || !bufferViews) {
                logger.Warnf("[NkGLTFLoader] accessors/bufferViews manquants : %s\n", path.CStr());
                return false;
            }

            // 4) Resoudre tous les buffers
            NkVector<RawBuffer> buffers;
            uint32 bufCount = buffersArr ? (uint32)buffersArr->Size() : 0;
            buffers.Resize(bufCount);
            for (uint32 b = 0; b < bufCount; ++b) {
                const NkArchive* buf = ArrayObjAt(*buffersArr, b);
                if (!buf) continue;
                NkString uri;
                if (ObjGetString(*buf, "uri", uri) && !uri.Empty()) {
                    if (!ResolveBufferURI(uri, baseDir, buffers[b])) {
                        logger.Warnf("[NkGLTFLoader] buffer %u non resolu\n", b);
                    }
                } else if (isGLB && b == 0) {
                    // buffer[0] sans uri -> chunk BIN du .glb
                    buffers[b].data = traits::NkMove(glbBin);
                }
            }

            // 5) Iterer meshes -> primitives
            out.vertices.Clear();
            out.indices.Clear();
            out.subMeshes.Clear();
            out.subMeshMaterial.Clear();
            out.materials.Clear();
            out.skinnedVertices.Clear();
            out.skinJoints.Clear();
            out.inverseBind.Clear();
            out.nodes.Clear();
            out.animations.Clear();
            out.isSkinned = false;
            out.skinRootNode = -1;
            out.bounds = NkAABB::Empty();
            out.debugName = p.GetFileNameWithoutExtension();

            // ── Scene-graph : parse nodes[] + calcule la world matrix de chaque
            //    mesh AVANT l'assemblage, pour baker les transforms de node dans
            //    les vertices statiques (corrige les modeles couches/decales).
            //    out.nodes est rempli ici et reutilise par la section skinning.
            ParseGLTFNodes(GetArrayNode(root, "nodes"), out.nodes);
            const uint32 meshCount = (uint32)meshes->Size();
            NkVector<NkMat4f> meshWorld;        // world matrix par index de mesh
            NkVector<bool>    meshAssigned;     // true si un node reference le mesh
            ComputeMeshWorldMatrices(root, out.nodes, meshCount, meshWorld, meshAssigned);
            // Normal matrix = inverse-transpose de la 3x3 (gere le scale non-uniforme).
            NkVector<NkMat4f> meshNormalMat; meshNormalMat.Resize(meshCount);
            for (uint32 i = 0; i < meshCount; ++i)
                meshNormalMat[i] = meshWorld[i].Inverse().Transpose();

            uint32 droppedPrims = 0;

            for (nk_size mi = 0; mi < meshes->Size(); ++mi) {
                // World/normal matrix de CE mesh (identite si non reference).
                const NkMat4f& meshW  = meshWorld[(uint32)mi];
                const NkMat4f& meshNM = meshNormalMat[(uint32)mi];
                const bool     bakeMesh = meshAssigned[(uint32)mi];
                const NkArchive* mesh = ArrayObjAt(*meshes, mi);
                if (!mesh) continue;
                NkString meshName;
                ObjGetString(*mesh, "name", meshName);

                const NkArchiveNode* primsNode = ObjFind(*mesh, "primitives");
                if (!primsNode || !primsNode->IsArray()) continue;
                const NkVector<NkArchiveNode>& prims = primsNode->array;

                for (nk_size pi = 0; pi < prims.Size(); ++pi) {
                    if (!prims[pi].IsObject()) continue;
                    const NkArchive& prim = *prims[pi].object;

                    // mode : seul TRIANGLES (4) ou non-specifie (defaut 4) supporte
                    int64 mode = ObjGetInt(prim, "mode", 4);
                    if (mode != 4) {
                        ++droppedPrims;
                        logger.Warnf("[NkGLTFLoader] primitive mode=%lld non supporte (skip)\n",
                                     (long long)mode);
                        continue;
                    }

                    const NkArchiveNode* attrNode = ObjFind(prim, "attributes");
                    if (!attrNode || !attrNode->IsObject()) { ++droppedPrims; continue; }
                    const NkArchive& attrs = *attrNode->object;

                    int64 posIdx = ObjGetInt(attrs, "POSITION", -1);
                    if (posIdx < 0) { ++droppedPrims; continue; }

                    AccessorInfo aPos = ResolveAccessor(*accessors, *bufferViews, posIdx);
                    if (!aPos.valid || aPos.componentType != CT_FLOAT || aPos.compCount != 3) {
                        ++droppedPrims;
                        logger.Warnf("[NkGLTFLoader] POSITION accessor invalide (skip)\n");
                        continue;
                    }
                    if (aPos.buffer < 0 || (uint32)aPos.buffer >= buffers.Size()) { ++droppedPrims; continue; }
                    const RawBuffer& posBuf = buffers[(uint32)aPos.buffer];

                    uint32 vcount = aPos.count;

                    AccessorInfo aNrm = ResolveAccessor(*accessors, *bufferViews, ObjGetInt(attrs, "NORMAL", -1));
                    AccessorInfo aTan = ResolveAccessor(*accessors, *bufferViews, ObjGetInt(attrs, "TANGENT", -1));
                    AccessorInfo aUV0 = ResolveAccessor(*accessors, *bufferViews, ObjGetInt(attrs, "TEXCOORD_0", -1));
                    AccessorInfo aUV1 = ResolveAccessor(*accessors, *bufferViews, ObjGetInt(attrs, "TEXCOORD_1", -1));
                    AccessorInfo aCol = ResolveAccessor(*accessors, *bufferViews, ObjGetInt(attrs, "COLOR_0", -1));
                    // Skinning : JOINTS_0 (uvec4 ubyte/ushort), WEIGHTS_0 (vec4 float/normalized).
                    AccessorInfo aJoint = ResolveAccessor(*accessors, *bufferViews, ObjGetInt(attrs, "JOINTS_0", -1));
                    AccessorInfo aWeight= ResolveAccessor(*accessors, *bufferViews, ObjGetInt(attrs, "WEIGHTS_0", -1));
                    bool primSkinned = aJoint.valid && aWeight.valid
                                    && aJoint.compCount >= 4 && aWeight.compCount >= 4;

                    auto bufOf = [&](const AccessorInfo& a) -> const RawBuffer* {
                        if (!a.valid || a.buffer < 0 || (uint32)a.buffer >= buffers.Size()) return nullptr;
                        return &buffers[(uint32)a.buffer];
                    };
                    const RawBuffer* nrmBuf = bufOf(aNrm);
                    const RawBuffer* tanBuf = bufOf(aTan);
                    const RawBuffer* uv0Buf = bufOf(aUV0);
                    const RawBuffer* uv1Buf = bufOf(aUV1);
                    const RawBuffer* colBuf = bufOf(aCol);
                    const RawBuffer* jointBuf  = primSkinned ? bufOf(aJoint)  : nullptr;
                    const RawBuffer* weightBuf = primSkinned ? bufOf(aWeight) : nullptr;
                    if (!jointBuf || !weightBuf) primSkinned = false;
                    if (primSkinned) out.isSkinned = true;

                    uint32 baseVertex = (uint32)out.vertices.Size();
                    NkAABB subBounds  = NkAABB::Empty();

                    // Bake la world matrix du node UNIQUEMENT pour les primitives
                    // STATIQUES. Pour les primitives SKINNEES, glTF ignore le node
                    // transform du mesh (le skinning gere l'espace via les joint
                    // matrices) -> on laisse les positions locales.
                    const bool bake = bakeMesh && !primSkinned;

                    // ── Vertices ──────────────────────────────────────────────
                    for (uint32 vi = 0; vi < vcount; ++vi) {
                        NkVertex3D v;
                        v.pos = {
                            ReadAccessorFloat(posBuf, aPos, vi, 0),
                            ReadAccessorFloat(posBuf, aPos, vi, 1),
                            ReadAccessorFloat(posBuf, aPos, vi, 2)
                        };
                        if (bake) v.pos = meshW * v.pos;   // world = M * vec4(pos,1)
                        subBounds.Expand(v.pos);

                        if (nrmBuf && aNrm.compCount >= 3) {
                            v.normal = {
                                ReadAccessorFloat(*nrmBuf, aNrm, vi, 0),
                                ReadAccessorFloat(*nrmBuf, aNrm, vi, 1),
                                ReadAccessorFloat(*nrmBuf, aNrm, vi, 2)
                            };
                            if (bake) {
                                // normal' = normalize(normalMatrix * normal)
                                v.normal = TransformDir3(meshNM, v.normal).Normalized();
                            }
                        } else {
                            v.normal = {0.f, 0.f, 0.f}; // calcule plus bas si absent
                        }

                        if (tanBuf && aTan.compCount >= 3) {
                            v.tangent = {
                                ReadAccessorFloat(*tanBuf, aTan, vi, 0),
                                ReadAccessorFloat(*tanBuf, aTan, vi, 1),
                                ReadAccessorFloat(*tanBuf, aTan, vi, 2)
                            };
                            if (bake) {
                                // Tangente = direction -> world matrix (3x3), pas la
                                // normal matrix (la tangente vit dans l'espace tangent).
                                v.tangent = TransformDir3(meshW, v.tangent).Normalized();
                            }
                        } else {
                            v.tangent = {1.f, 0.f, 0.f};
                        }

                        if (uv0Buf && aUV0.compCount >= 2) {
                            v.uv = {
                                ReadAccessorFloat(*uv0Buf, aUV0, vi, 0),
                                ReadAccessorFloat(*uv0Buf, aUV0, vi, 1)
                            };
                        } else {
                            v.uv = {0.f, 0.f};
                        }

                        if (uv1Buf && aUV1.compCount >= 2) {
                            v.uv2 = {
                                ReadAccessorFloat(*uv1Buf, aUV1, vi, 0),
                                ReadAccessorFloat(*uv1Buf, aUV1, vi, 1)
                            };
                        } else {
                            v.uv2 = {0.f, 0.f};
                        }

                        if (colBuf && aCol.compCount >= 3) {
                            float32 r = ReadAccessorFloat(*colBuf, aCol, vi, 0);
                            float32 g = ReadAccessorFloat(*colBuf, aCol, vi, 1);
                            float32 b = ReadAccessorFloat(*colBuf, aCol, vi, 2);
                            float32 a = (aCol.compCount >= 4) ? ReadAccessorFloat(*colBuf, aCol, vi, 3) : 1.f;
                            // Les COLOR_0 float sont en [0,1] ; les int normalises
                            // sont deja ramenes a [0,1] par ReadAccessorFloat.
                            auto clamp01 = [](float32 x){ return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); };
                            v.color = PackRGBA8((uint8)(clamp01(r) * 255.f + 0.5f),
                                                (uint8)(clamp01(g) * 255.f + 0.5f),
                                                (uint8)(clamp01(b) * 255.f + 0.5f),
                                                (uint8)(clamp01(a) * 255.f + 0.5f));
                        } else {
                            v.color = PackRGBA8(255, 255, 255, 255);
                        }

                        out.vertices.PushBack(v);

                        // ── Skinning : construit le vertex skinne parallele ───
                        // boneIdx stockes en float (cf. NkVertexSkinned), poids
                        // normalises (somme = 1) pour eviter les artefacts si le
                        // fichier n'est pas exactement normalise.
                        if (primSkinned) {
                            NkVertexSkinned sv;
                            // Copie la partie NkVertex3D deja remplie.
                            static_cast<NkVertex3D&>(sv) = v;
                            uint32 j0 = ReadAccessorUInt(*jointBuf, aJoint, vi, 0);
                            uint32 j1 = ReadAccessorUInt(*jointBuf, aJoint, vi, 1);
                            uint32 j2 = ReadAccessorUInt(*jointBuf, aJoint, vi, 2);
                            uint32 j3 = ReadAccessorUInt(*jointBuf, aJoint, vi, 3);
                            float32 w0 = ReadAccessorFloat(*weightBuf, aWeight, vi, 0);
                            float32 w1 = ReadAccessorFloat(*weightBuf, aWeight, vi, 1);
                            float32 w2 = ReadAccessorFloat(*weightBuf, aWeight, vi, 2);
                            float32 w3 = ReadAccessorFloat(*weightBuf, aWeight, vi, 3);
                            float32 ws = w0 + w1 + w2 + w3;
                            if (ws > 1e-6f) { w0/=ws; w1/=ws; w2/=ws; w3/=ws; }
                            else            { w0 = 1.f; w1 = w2 = w3 = 0.f; }
                            sv.boneIdx[0]=(float32)j0; sv.boneIdx[1]=(float32)j1;
                            sv.boneIdx[2]=(float32)j2; sv.boneIdx[3]=(float32)j3;
                            sv.boneWeight[0]=w0; sv.boneWeight[1]=w1;
                            sv.boneWeight[2]=w2; sv.boneWeight[3]=w3;
                            out.skinnedVertices.PushBack(sv);
                        } else if (out.isSkinned) {
                            // Primitive non-skinnee dans un mesh skinne : pousse un
                            // vertex skinne neutre (bone 0, poids 1) pour garder
                            // skinnedVertices parallele a vertices.
                            NkVertexSkinned sv;
                            static_cast<NkVertex3D&>(sv) = v;
                            sv.boneIdx[0]=sv.boneIdx[1]=sv.boneIdx[2]=sv.boneIdx[3]=0.f;
                            sv.boneWeight[0]=1.f; sv.boneWeight[1]=sv.boneWeight[2]=sv.boneWeight[3]=0.f;
                            out.skinnedVertices.PushBack(sv);
                        }
                    }

                    // ── Indices ───────────────────────────────────────────────
                    uint32 firstIndex = (uint32)out.indices.Size();
                    int64  idxAcc     = ObjGetInt(prim, "indices", -1);
                    uint32 idxCount   = 0;

                    if (idxAcc >= 0) {
                        AccessorInfo aIdx = ResolveAccessor(*accessors, *bufferViews, idxAcc);
                        const RawBuffer* idxBuf = bufOf(aIdx);
                        if (aIdx.valid && idxBuf && aIdx.compCount == 1) {
                            for (uint32 ii = 0; ii < aIdx.count; ++ii) {
                                uint32 idx = ReadAccessorIndex(*idxBuf, aIdx, ii);
                                if (idx >= vcount) idx = 0; // garde anti out-of-bounds
                                out.indices.PushBack(idx);
                            }
                            idxCount = aIdx.count;
                        }
                    }
                    if (idxCount == 0) {
                        // Pas d'indices -> sequentiel (0,1,2,...)
                        for (uint32 ii = 0; ii < vcount; ++ii) out.indices.PushBack(ii);
                        idxCount = vcount;
                    }

                    // ── Normales calculees par-face si absentes ───────────────
                    if (!nrmBuf) {
                        // Accumuler les normales de face sur chaque vertex de la
                        // primitive, puis normaliser.
                        for (uint32 ii = 0; ii + 2 < idxCount; ii += 3) {
                            uint32 i0 = out.indices[firstIndex + ii + 0] + baseVertex;
                            uint32 i1 = out.indices[firstIndex + ii + 1] + baseVertex;
                            uint32 i2 = out.indices[firstIndex + ii + 2] + baseVertex;
                            NkVec3f p0 = out.vertices[i0].pos;
                            NkVec3f p1 = out.vertices[i1].pos;
                            NkVec3f p2 = out.vertices[i2].pos;
                            NkVec3f e1 = {p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
                            NkVec3f e2 = {p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};
                            NkVec3f n  = {
                                e1.y * e2.z - e1.z * e2.y,
                                e1.z * e2.x - e1.x * e2.z,
                                e1.x * e2.y - e1.y * e2.x
                            };
                            out.vertices[i0].normal = { out.vertices[i0].normal.x + n.x, out.vertices[i0].normal.y + n.y, out.vertices[i0].normal.z + n.z };
                            out.vertices[i1].normal = { out.vertices[i1].normal.x + n.x, out.vertices[i1].normal.y + n.y, out.vertices[i1].normal.z + n.z };
                            out.vertices[i2].normal = { out.vertices[i2].normal.x + n.x, out.vertices[i2].normal.y + n.y, out.vertices[i2].normal.z + n.z };
                        }
                        for (uint32 vi = 0; vi < vcount; ++vi) {
                            NkVec3f& n = out.vertices[baseVertex + vi].normal;
                            float32 len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
                            if (len > 1e-8f) { n.x /= len; n.y /= len; n.z /= len; }
                            else             { n = {0.f, 1.f, 0.f}; }
                        }
                        // Mesh SKINNE : les skinnedVertices ont ete remplis AVANT ce
                        // calcul (normale=0 car NORMAL absent, ex. Fox) -> recopier
                        // la normale par-face, sinon eclairage=0 -> mesh NOIR.
                        if (out.isSkinned
                            && out.skinnedVertices.Size() == out.vertices.Size()) {
                            for (uint32 vi = 0; vi < vcount; ++vi)
                                out.skinnedVertices[baseVertex + vi].normal =
                                    out.vertices[baseVertex + vi].normal;
                        }
                    }

                    // ── SubMesh ───────────────────────────────────────────────
                    NkSubMesh sm;
                    if (!meshName.Empty()) sm.name = meshName;
                    else                   sm.name = NkString("primitive");
                    sm.firstIndex = firstIndex;
                    sm.indexCount = idxCount;
                    sm.baseVertex = baseVertex;
                    sm.bounds     = subBounds;
                    // sm.material (NkMatHandle template) reste invalide : le pont
                    // mesh->renderer assigne des NkMatInstHandle via subMeshMaterial.
                    out.subMeshes.PushBack(sm);

                    // Index materiau glTF de cette primitive (-1 = aucun).
                    out.subMeshMaterial.PushBack((int32)ObjGetInt(prim, "material", -1));

                    out.bounds.Merge(subBounds);
                }
            }

            if (out.vertices.Empty() || out.subMeshes.Empty()) {
                logger.Warnf("[NkGLTFLoader] aucune geometrie chargee depuis : %s (prims drop=%u)\n",
                             path.CStr(), droppedPrims);
                return false;
            }

            // 6) Materiaux + textures + images (PBR metallic-roughness glTF 2.0)
            //    Decode tardif : on n'a besoin des images que si des materiaux
            //    les referencent. On decode TOUTES les images presentes (simple)
            //    et on map les index dans NkGLTFMaterial.
            {
                const NkVector<NkArchiveNode>* materialsArr = GetArrayNode(root, "materials");
                const NkVector<NkArchiveNode>* texturesArr  = GetArrayNode(root, "textures");
                const NkVector<NkArchiveNode>* imagesArr    = GetArrayNode(root, "images");

                // Decode toutes les images en RGBA8.
                if (imagesArr) {
                    out.images.Resize(imagesArr->Size());
                    for (nk_size ii = 0; ii < imagesArr->Size(); ++ii) {
                        const NkArchive* imgObj = ArrayObjAt(*imagesArr, ii);
                        if (!imgObj) continue;
                        DecodeGLTFImage(*imgObj, baseDir, buffers, bufferViews, out.images[ii]);
                        if (out.images[ii].valid) {
                            logger.Info("[NkGLTFLoader] image[{0}] decodee : {1}x{2}\n",
                                        (uint32)ii, (uint32)out.images[ii].decoded.Width(),
                                        (uint32)out.images[ii].decoded.Height());
                        } else {
                            // Decode echoue : le codec NKImage n'a pas su lire
                            // ces octets (ex. JPEG embarque non supporte). La
                            // NkTexHandle restera invalide -> material sans
                            // albedoMap -> texture blanche par defaut (couleurs
                            // fausses). Trace explicite pour diagnostic.
                            logger.Warnf("[NkGLTFLoader] image[%u] DECODE ECHOUE "
                                         "(uri='%s', codec NKImage)\n",
                                         (uint32)ii, out.images[ii].uri.CStr());
                        }
                    }
                }

                // Parse les materiaux.
                if (materialsArr) {
                    for (nk_size mi = 0; mi < materialsArr->Size(); ++mi) {
                        const NkArchive* mat = ArrayObjAt(*materialsArr, mi);
                        NkGLTFMaterial gm;
                        if (mat) {
                            ObjGetString(*mat, "name", gm.name);
                            gm.doubleSided = (ObjGetInt(*mat, "doubleSided", 0) != 0);

                            // alphaMode : OPAQUE / MASK / BLEND
                            NkString alphaMode;
                            if (ObjGetString(*mat, "alphaMode", alphaMode)) {
                                if (alphaMode == NkString("MASK"))  gm.alphaMode = 1;
                                else if (alphaMode == NkString("BLEND")) gm.alphaMode = 2;
                            }
                            gm.alphaCutoff = ObjGetFloat(*mat, "alphaCutoff", 0.5f);

                            // emissiveFactor (VEC3)
                            {
                                float32 e[3] = {0.f, 0.f, 0.f};
                                if (ObjGetFloatArray(*mat, "emissiveFactor", e, 3) == 3)
                                    gm.emissiveFactor = {e[0], e[1], e[2]};
                            }
                            gm.emissiveImage   = ResolveTextureImage(*mat, "emissiveTexture",  texturesArr);

                            // normalTexture { index, scale }
                            {
                                const NkArchiveNode* nt = mat->FindNode(NkStringView("normalTexture"));
                                if (nt && nt->IsObject()) {
                                    gm.normalScale = ObjGetFloat(*nt->object, "scale", 1.f);
                                }
                            }
                            gm.normalImage     = ResolveTextureImage(*mat, "normalTexture",    texturesArr);

                            // occlusionTexture { index, strength }
                            {
                                const NkArchiveNode* ot = mat->FindNode(NkStringView("occlusionTexture"));
                                if (ot && ot->IsObject()) {
                                    gm.occlusionStrength = ObjGetFloat(*ot->object, "strength", 1.f);
                                }
                            }
                            gm.occlusionImage  = ResolveTextureImage(*mat, "occlusionTexture", texturesArr);

                            // pbrMetallicRoughness { baseColorFactor, metallicFactor,
                            //                        roughnessFactor, baseColorTexture,
                            //                        metallicRoughnessTexture }
                            const NkArchiveNode* pbrNode = mat->FindNode(NkStringView("pbrMetallicRoughness"));
                            if (pbrNode && pbrNode->IsObject()) {
                                const NkArchive& pbr = *pbrNode->object;
                                float32 bc[4] = {1.f, 1.f, 1.f, 1.f};
                                if (ObjGetFloatArray(pbr, "baseColorFactor", bc, 4) >= 3)
                                    gm.baseColorFactor = {bc[0], bc[1], bc[2], bc[3]};
                                gm.metallicFactor  = ObjGetFloat(pbr, "metallicFactor",  1.f);
                                gm.roughnessFactor = ObjGetFloat(pbr, "roughnessFactor", 1.f);
                                gm.baseColorImage         = ResolveTextureImage(pbr, "baseColorTexture",         texturesArr);
                                gm.metallicRoughnessImage = ResolveTextureImage(pbr, "metallicRoughnessTexture", texturesArr);
                            }
                        }
                        out.materials.PushBack(gm);
                    }
                }
            }

            // 7) Skinning : nodes[] (scene graph), skins[] (joints + inverseBind),
            //    animations[] (samplers TRS). Necessaire pour evaluer la pose.
            if (out.isSkinned) {
                const NkVector<NkArchiveNode>* nodesArr = GetArrayNode(root, "nodes");
                const NkVector<NkArchiveNode>* skinsArr = GetArrayNode(root, "skins");
                const NkVector<NkArchiveNode>* animArr  = GetArrayNode(root, "animations");

                // ── Nodes (TRS ou matrix + mesh + children) ──────────────────
                // Deja parses avant la boucle meshes (pour le baking scene-graph) ;
                // on re-parse seulement si vide (defensif).
                if (out.nodes.Empty() && nodesArr) {
                    ParseGLTFNodes(nodesArr, out.nodes);
                }

                // ── Skin (premier skin uniquement pour le MVP) ───────────────
                if (skinsArr && !skinsArr->Empty()) {
                    const NkArchive* skin = ArrayObjAt(*skinsArr, 0);
                    if (skin) {
                        out.skinRootNode = (int32)ObjGetInt(*skin, "skeleton", -1);
                        const NkArchiveNode* jn = skin->FindNode(NkStringView("joints"));
                        if (jn && jn->IsArray()) {
                            for (nk_size ji = 0; ji < jn->array.Size(); ++ji)
                                out.skinJoints.PushBack((int32)NodeAsInt(&jn->array[ji], -1));
                        }
                        int64 ibmAcc = ObjGetInt(*skin, "inverseBindMatrices", -1);
                        if (ibmAcc >= 0) {
                            AccessorInfo aIBM = ResolveAccessor(*accessors, *bufferViews, ibmAcc);
                            const RawBuffer* ibmBuf = (aIBM.valid && aIBM.buffer >= 0
                                                       && (uint32)aIBM.buffer < buffers.Size())
                                                     ? &buffers[(uint32)aIBM.buffer] : nullptr;
                            if (ibmBuf && aIBM.compCount == 16) {
                                for (uint32 k = 0; k < aIBM.count; ++k)
                                    out.inverseBind.PushBack(ReadAccessorMat4(*ibmBuf, aIBM, k));
                            }
                        }
                        // Si pas d'inverseBind fournies : identite (bind pose = repos).
                        if (out.inverseBind.Empty()) {
                            for (uint32 k = 0; k < (uint32)out.skinJoints.Size(); ++k)
                                out.inverseBind.PushBack(NkMat4f::Identity());
                        }
                    }
                }

                // ── Animations (samplers + channels) ─────────────────────────
                if (animArr) {
                    for (nk_size ai = 0; ai < animArr->Size(); ++ai) {
                        const NkArchive* anim = ArrayObjAt(*animArr, ai);
                        if (!anim) continue;
                        NkGLTFAnimation ga;
                        ObjGetString(*anim, "name", ga.name);

                        const NkArchiveNode* samplersN = anim->FindNode(NkStringView("samplers"));
                        const NkArchiveNode* channelsN = anim->FindNode(NkStringView("channels"));
                        if (!samplersN || !samplersN->IsArray()
                         || !channelsN || !channelsN->IsArray()) continue;
                        const NkVector<NkArchiveNode>& samplers = samplersN->array;

                        for (nk_size ci = 0; ci < channelsN->array.Size(); ++ci) {
                            const NkArchiveNode& chNode = channelsN->array[ci];
                            if (!chNode.IsObject()) continue;
                            const NkArchive& ch = *chNode.object;
                            int64 sampIdx = ObjGetInt(ch, "sampler", -1);
                            const NkArchiveNode* tgt = ch.FindNode(NkStringView("target"));
                            if (sampIdx < 0 || !tgt || !tgt->IsObject()) continue;

                            NkGLTFAnimChannel gc;
                            gc.node = (int32)ObjGetInt(*tgt->object, "node", -1);
                            NkString pathStr;
                            ObjGetString(*tgt->object, "path", pathStr);
                            if      (pathStr == NkString("translation")) gc.path = NkGLTFPath::TRANSLATION;
                            else if (pathStr == NkString("rotation"))    gc.path = NkGLTFPath::ROTATION;
                            else if (pathStr == NkString("scale"))       gc.path = NkGLTFPath::SCALE;
                            else                                         gc.path = NkGLTFPath::WEIGHTS;
                            if (gc.path == NkGLTFPath::WEIGHTS) continue; // morph non gere

                            const NkArchive* samp = ArrayObjAt(samplers, (nk_size)sampIdx);
                            if (!samp) continue;
                            NkString interpStr;
                            ObjGetString(*samp, "interpolation", interpStr);
                            if      (interpStr == NkString("STEP"))        gc.interp = NkGLTFInterp::STEP;
                            else if (interpStr == NkString("CUBICSPLINE")) gc.interp = NkGLTFInterp::CUBICSPLINE;
                            else                                           gc.interp = NkGLTFInterp::LINEAR;

                            int64 inAcc  = ObjGetInt(*samp, "input", -1);
                            int64 outAcc = ObjGetInt(*samp, "output", -1);
                            AccessorInfo aIn  = ResolveAccessor(*accessors, *bufferViews, inAcc);
                            AccessorInfo aOut = ResolveAccessor(*accessors, *bufferViews, outAcc);
                            const RawBuffer* inBuf = (aIn.valid && aIn.buffer >= 0
                                                      && (uint32)aIn.buffer < buffers.Size())
                                                    ? &buffers[(uint32)aIn.buffer] : nullptr;
                            const RawBuffer* outBuf = (aOut.valid && aOut.buffer >= 0
                                                       && (uint32)aOut.buffer < buffers.Size())
                                                     ? &buffers[(uint32)aOut.buffer] : nullptr;
                            if (!inBuf || !outBuf) continue;

                            uint32 nk = aIn.count;
                            // CUBICSPLINE : output a 3x les valeurs (in/val/out tangents).
                            // On lit seulement la valeur centrale -> traite comme LINEAR.
                            uint32 stride = (gc.interp == NkGLTFInterp::CUBICSPLINE) ? 3 : 1;
                            uint32 valOff = (gc.interp == NkGLTFInterp::CUBICSPLINE) ? 1 : 0;
                            for (uint32 k = 0; k < nk; ++k) {
                                gc.times.PushBack(ReadAccessorFloat(*inBuf, aIn, k, 0));
                                uint32 oi = k * stride + valOff;
                                NkVec4f val{0,0,0, gc.path==NkGLTFPath::ROTATION?1.f:0.f};
                                val.x = ReadAccessorFloat(*outBuf, aOut, oi, 0);
                                val.y = ReadAccessorFloat(*outBuf, aOut, oi, 1);
                                val.z = ReadAccessorFloat(*outBuf, aOut, oi, 2);
                                if (aOut.compCount >= 4)
                                    val.w = ReadAccessorFloat(*outBuf, aOut, oi, 3);
                                gc.values.PushBack(val);
                                if (gc.times[k] > ga.duration) ga.duration = gc.times[k];
                            }
                            if (gc.interp == NkGLTFInterp::CUBICSPLINE)
                                gc.interp = NkGLTFInterp::LINEAR;
                            ga.channels.PushBack(traits::NkMove(gc));
                        }
                        out.animations.PushBack(traits::NkMove(ga));
                    }
                }

                logger.Info("[NkGLTFLoader] SKINNED : {0} joints, {1} inverseBind, "
                            "{2} nodes, {3} animations, {4} skinnedVerts\n",
                            (uint32)out.skinJoints.Size(), (uint32)out.inverseBind.Size(),
                            (uint32)out.nodes.Size(), (uint32)out.animations.Size(),
                            (uint32)out.skinnedVertices.Size());
            }

            logger.Info("[NkGLTFLoader] '{0}' : {1} vertices, {2} indices, {3} submeshes, {4} materials, {5} images\n",
                        path.CStr(), (uint32)out.vertices.Size(),
                        (uint32)out.indices.Size(), (uint32)out.subMeshes.Size(),
                        (uint32)out.materials.Size(), (uint32)out.images.Size());
            // Bounds finales (apres baking scene-graph) : utile pour verifier que
            // l'orientation des modeles est correcte (axe long, mise a l'endroit).
            logger.Info("[NkGLTFLoader] bounds (world-baked) min=({0}, {1}, {2}) "
                        "max=({3}, {4}, {5})\n",
                        out.bounds.min.x, out.bounds.min.y, out.bounds.min.z,
                        out.bounds.max.x, out.bounds.max.y, out.bounds.max.z);
            return true;
        }

        // ─────────────────────────────────────────────────────────────────────
        // Evaluation de pose squelettique
        // ─────────────────────────────────────────────────────────────────────
        namespace {

            // Interpole un canal au temps t (LINEAR + STEP ; lerp/slerp simple).
            NkVec4f SampleChannel(const NkGLTFAnimChannel& ch, float32 t) {
                if (ch.times.Empty())
                    return (ch.path == NkGLTFPath::ROTATION) ? NkVec4f{0,0,0,1}
                         : (ch.path == NkGLTFPath::SCALE)    ? NkVec4f{1,1,1,0}
                                                             : NkVec4f{0,0,0,0};
                uint32 n = (uint32)ch.times.Size();
                if (t <= ch.times[0])     return ch.values[0];
                if (t >= ch.times[n-1])   return ch.values[n-1];
                uint32 hi = 1;
                while (hi < n && ch.times[hi] <= t) ++hi;
                uint32 lo = hi - 1;
                float32 dt = ch.times[hi] - ch.times[lo];
                float32 a  = (dt > 1e-8f) ? (t - ch.times[lo]) / dt : 0.f;
                const NkVec4f& A = ch.values[lo];
                const NkVec4f& B = ch.values[hi];
                if (ch.interp == NkGLTFInterp::STEP) return A;
                if (ch.path == NkGLTFPath::ROTATION) {
                    // slerp (nlerp suffisante visuellement, plus simple/robuste).
                    float32 d = A.x*B.x + A.y*B.y + A.z*B.z + A.w*B.w;
                    NkVec4f Bc = B; if (d < 0.f) { Bc = {-B.x,-B.y,-B.z,-B.w}; }
                    NkVec4f q{ A.x+(Bc.x-A.x)*a, A.y+(Bc.y-A.y)*a,
                               A.z+(Bc.z-A.z)*a, A.w+(Bc.w-A.w)*a };
                    float32 len = sqrtf(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
                    if (len > 1e-8f) { q.x/=len; q.y/=len; q.z/=len; q.w/=len; }
                    else             { q = {0,0,0,1}; }
                    return q;
                }
                return { A.x+(B.x-A.x)*a, A.y+(B.y-A.y)*a, A.z+(B.z-A.z)*a, A.w+(B.w-A.w)*a };
            }

            // Compose la matrice locale d'un node a partir de ses TRS (ou matrix).
            NkMat4f NodeLocal(const NkGLTFNode& n) {
                if (n.hasMatrix) return n.matrix;
                NkQuatf q(n.rotation.x, n.rotation.y, n.rotation.z, n.rotation.w);
                NkMat4f R = static_cast<NkMat4f>(q);
                NkMat4f T = NkMat4f::Translate(n.translation);
                NkMat4f S = NkMat4f::Scale(n.scale);
                return T * R * S;
            }

        } // namespace anonyme

        bool EvaluateGLTFPose(const NkGLTFMeshData& data, int32 animIdx,
                              float32 t, NkVector<NkMat4f>& outBones) {
            if (!data.isSkinned || data.skinJoints.Empty()) return false;

            uint32 nodeCount = (uint32)data.nodes.Size();
            if (nodeCount == 0) return false;

            // 1) Transforms LOCALES = TRS statiques, surchargees par l'animation.
            NkVector<NkMat4f> local; local.Resize(nodeCount);
            for (uint32 i = 0; i < nodeCount; ++i)
                local[i] = NodeLocal(data.nodes[i]);

            if (animIdx >= 0 && animIdx < (int32)data.animations.Size()) {
                const NkGLTFAnimation& anim = data.animations[(uint32)animIdx];
                // Temps boucle sur la duree.
                float32 dur = anim.duration > 1e-4f ? anim.duration : 1.f;
                float32 tt  = t - floorf(t / dur) * dur;
                // Recompose chaque node anime depuis ses TRS (override par canal).
                // On part des TRS statiques puis on applique les canaux.
                NkVector<NkVec3f> trans; trans.Resize(nodeCount);
                NkVector<NkVec4f> rot;   rot.Resize(nodeCount);
                NkVector<NkVec3f> scl;   scl.Resize(nodeCount);
                NkVector<bool>    hasM;  hasM.Resize(nodeCount);
                for (uint32 i = 0; i < nodeCount; ++i) {
                    trans[i] = data.nodes[i].translation;
                    rot[i]   = data.nodes[i].rotation;
                    scl[i]   = data.nodes[i].scale;
                    hasM[i]  = data.nodes[i].hasMatrix;
                }
                for (uint32 c = 0; c < (uint32)anim.channels.Size(); ++c) {
                    const NkGLTFAnimChannel& ch = anim.channels[c];
                    if (ch.node < 0 || ch.node >= (int32)nodeCount) continue;
                    NkVec4f v = SampleChannel(ch, tt);
                    if      (ch.path == NkGLTFPath::TRANSLATION) trans[ch.node] = {v.x,v.y,v.z};
                    else if (ch.path == NkGLTFPath::ROTATION)    rot[ch.node]   = v;
                    else if (ch.path == NkGLTFPath::SCALE)       scl[ch.node]   = {v.x,v.y,v.z};
                }
                for (uint32 i = 0; i < nodeCount; ++i) {
                    if (hasM[i]) { local[i] = data.nodes[i].matrix; continue; }
                    NkQuatf q(rot[i].x, rot[i].y, rot[i].z, rot[i].w);
                    local[i] = NkMat4f::Translate(trans[i])
                             * static_cast<NkMat4f>(q)
                             * NkMat4f::Scale(scl[i]);
                }
            }

            // 2) Transforms GLOBALES par parcours hierarchique (parent -> enfants).
            NkVector<NkMat4f> global; global.Resize(nodeCount);
            NkVector<int32>   parent; parent.Resize(nodeCount);
            for (uint32 i = 0; i < nodeCount; ++i) parent[i] = -1;
            for (uint32 i = 0; i < nodeCount; ++i)
                for (uint32 c = 0; c < (uint32)data.nodes[i].children.Size(); ++c) {
                    int32 ci = data.nodes[i].children[c];
                    if (ci >= 0 && ci < (int32)nodeCount) parent[ci] = (int32)i;
                }
            // Resolution iterative (ordre topologique simple : on itere jusqu'a
            // ce que tous les globals soient calcules ; profondeur faible).
            NkVector<bool> done; done.Resize(nodeCount);
            for (uint32 i = 0; i < nodeCount; ++i) done[i] = false;
            bool progressed = true;
            uint32 guard = 0;
            while (progressed && guard++ < nodeCount + 2) {
                progressed = false;
                for (uint32 i = 0; i < nodeCount; ++i) {
                    if (done[i]) continue;
                    int32 p = parent[i];
                    if (p < 0)          { global[i] = local[i]; done[i] = true; progressed = true; }
                    else if (done[p])   { global[i] = global[p] * local[i]; done[i] = true; progressed = true; }
                }
            }
            for (uint32 i = 0; i < nodeCount; ++i) if (!done[i]) global[i] = local[i];

            // 3) Joint matrices = globalTransform(joint) * inverseBind(joint).
            uint32 jc = (uint32)data.skinJoints.Size();
            outBones.Clear();
            outBones.Resize(jc);
            for (uint32 j = 0; j < jc; ++j) {
                int32 nodeIdx = data.skinJoints[j];
                NkMat4f g = (nodeIdx >= 0 && nodeIdx < (int32)nodeCount)
                          ? global[nodeIdx] : NkMat4f::Identity();
                NkMat4f ib = (j < (uint32)data.inverseBind.Size())
                          ? data.inverseBind[j] : NkMat4f::Identity();
                outBones[j] = g * ib;
            }
            return true;
        }

    } // namespace renderer
} // namespace nkentseu
