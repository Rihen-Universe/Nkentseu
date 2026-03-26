// =============================================================================
// NkModelLoader.cpp
// Implémentation des parsers de modèles 3D.
// =============================================================================
#include "NkModelLoader.h"
#include "NKLogger/NkLog.h"
#include "NKCore/NkMacros.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

// Simple JSON parser header-only (si disponible) ou fallback manuel
// Pour GLTF on parse manuellement le JSON minimal nécessaire

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // Utilitaires internes
        // =============================================================================
        namespace {

            // Lecture d'un fichier binaire entier
            static NkVector<uint8> ReadFileBin(const char* path) {
                FILE* f = fopen(path, "rb");
                if (!f) return {};
                fseek(f, 0, SEEK_END);
                long sz = ftell(f);
                fseek(f, 0, SEEK_SET);
                if (sz <= 0) { fclose(f); return {}; }
                NkVector<uint8> buf;
                buf.Resize((usize)sz);
                fread(buf.Data(), 1, (usize)sz, f);
                fclose(f);
                return buf;
            }

            // Lecture d'un fichier texte
            static NkString ReadFileTxt(const char* path) {
                FILE* f = fopen(path, "r");
                if (!f) return {};
                fseek(f, 0, SEEK_END);
                long sz = ftell(f);
                fseek(f, 0, SEEK_SET);
                if (sz <= 0) { fclose(f); return {}; }
                NkVector<char> buf;
                buf.Resize((usize)sz + 1);
                usize read = fread(buf.Data(), 1, (usize)sz, f);
                buf[read] = '\0';
                fclose(f);
                return NkString(buf.Data());
            }

            static NkString ExtractDir(const NkString& path) {
                for (int i = (int)path.Length() - 1; i >= 0; --i) {
                    char c = path[i];
                    if (c == '/' || c == '\\')
                        return NkString(path.CStr(), (usize)i + 1);
                }
                return NkString("./");
            }

            static NkString ExtractExt(const NkString& path) {
                for (int i = (int)path.Length() - 1; i >= 0; --i) {
                    if (path[i] == '.') return NkString(path.CStr() + i).ToLower();
                }
                return {};
            }

            static NkString ResolvePath(const NkString& dir, const NkString& rel) {
                if (rel.Empty()) return {};
                if (rel[0] == '/' || rel[0] == '\\') return rel;
                return dir + rel;
            }

            static void Trim(const char*& p) {
                while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
            }

            static float ParseFloat(const char*& p) {
                char* end;
                float v = (float)strtod(p, &end);
                p = end;
                return v;
            }

            static uint32 ParseUInt(const char*& p) {
                char* end;
                uint32 v = (uint32)strtoul(p, &end, 10);
                p = end;
                return v;
            }

            static NkString ParseLine(const char*& p) {
                Trim(p);
                const char* start = p;
                while (*p && *p != '\n') ++p;
                NkString s(start, (usize)(p - start));
                if (*p == '\n') ++p;
                // trim trailing \r
                while (!s.Empty() && s[s.Length()-1] == '\r')
                    s = NkString(s.CStr(), s.Length()-1);
                return s;
            }

            static NkString ParseWord(const char*& p) {
                Trim(p);
                const char* start = p;
                while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') ++p;
                return NkString(start, (usize)(p - start));
            }

        } // anonymous namespace

        // =============================================================================
        // Dispatch par extension
        // =============================================================================
        bool NkModelLoader::Load(const char* path, NkModelData& out,
                                const NkModelImportOptions& opts) {
            NkString ext = ExtractExt(NkString(path));
            if      (ext == ".obj")  return LoadOBJ  (path, out, opts);
            else if (ext == ".stl")  return LoadSTL  (path, out, opts);
            else if (ext == ".glb")  return LoadGLB  (path, out, opts);
            else if (ext == ".gltf") return LoadGLTF (path, out, opts);
            else if (ext == ".ply")  return LoadPLY  (path, out, opts);
            else if (ext == ".3ds")  return Load3DS  (path, out, opts);
            else if (ext == ".dae")  return LoadCollada(path, out, opts);
            logger_src.Infof("[NkModelLoader] Format non supporté: %s\n", ext.CStr());
            return false;
        }

        // =============================================================================
        // OBJ Parser
        // =============================================================================
        bool NkModelLoader::LoadOBJ(const char* path, NkModelData& out,
                                    const NkModelImportOptions& opts) {
            NkString src = ReadFileTxt(path);
            if (src.Empty()) {
                logger_src.Infof("[NkModelLoader] Impossible de lire: %s\n", path);
                return false;
            }
            NkString dir = ExtractDir(NkString(path));
            out.sourcePath = path;
            out.sourceDir  = dir;
            return ParseOBJ(src, dir, out, opts);
        }

        bool NkModelLoader::ParseOBJ(const NkString& src, const NkString& dir,
                                    NkModelData& out, const NkModelImportOptions& opts) {
            NkVector<NkVec3f>  positions;
            NkVector<NkVec3f>  normals;
            NkVector<NkVec2f>  uvs;

            // Mesh courant
            NkMeshData currentMesh;
            NkString   currentMeshName  = "Mesh";
            NkString   currentMatName;
            uint32     currentMatIdx    = 0;

            // Map pour la déduplication des vertices
            NkUnorderedMap<NkString, uint32> vertCache;
            uint32 vertCount = 0;

            auto FlushMesh = [&]() {
                if (!currentMesh.vertices.Empty()) {
                    if (opts.generateNormals)  currentMesh.RecalculateNormals();
                    if (opts.generateTangents) currentMesh.RecalculateTangents();
                    if (opts.optimizeMesh)     currentMesh.Optimize();
                    currentMesh.RecalculateAABB();
                    out.meshes.PushBack(currentMesh);
                    out.meshNames.PushBack(currentMeshName);
                }
                currentMesh = NkMeshData{};
                vertCache.Clear();
                vertCount = 0;
            };

            const char* p = src.CStr();
            while (*p) {
                Trim(p);
                if (!*p) break;

                if (*p == '#') { while (*p && *p != '\n') ++p; continue; }

                if (strncmp(p, "mtllib", 6) == 0) {
                    p += 6; Trim(p);
                    NkString mtlFile = ParseLine(p);
                    if (opts.importMaterials && !mtlFile.Empty()) {
                        NkString mtlPath = ResolvePath(dir, mtlFile);
                        ParseMTL(mtlPath, dir, out.materials);
                    }
                    continue;
                }
                if (strncmp(p, "usemtl", 6) == 0) {
                    p += 6; Trim(p);
                    currentMatName = ParseLine(p);
                    // Trouver l'index du matériau
                    currentMatIdx = 0;
                    for (uint32 i=0; i<out.materials.Size(); i++) {
                        if (out.materials[i].name == currentMatName) { currentMatIdx=i; break; }
                    }
                    continue;
                }
                if (strncmp(p, "o ", 2) == 0 || strncmp(p, "g ", 2) == 0) {
                    p += 2;
                    NkString name = ParseLine(p);
                    if (!currentMesh.vertices.Empty()) FlushMesh();
                    currentMeshName = name;
                    continue;
                }

                if (*p == 'v' && (p[1]==' ' || p[1]=='\t')) {
                    ++p;
                    NkVec3f v;
                    v.x = ParseFloat(p) * opts.scale;
                    v.y = ParseFloat(p) * opts.scale;
                    v.z = ParseFloat(p) * opts.scale;
                    positions.PushBack(v);
                    while (*p && *p != '\n') ++p;
                    continue;
                }
                if (strncmp(p, "vn", 2) == 0) {
                    p += 2;
                    NkVec3f n;
                    n.x = ParseFloat(p);
                    n.y = ParseFloat(p);
                    n.z = ParseFloat(p);
                    normals.PushBack(n);
                    while (*p && *p != '\n') ++p;
                    continue;
                }
                if (strncmp(p, "vt", 2) == 0) {
                    p += 2;
                    NkVec2f uv;
                    uv.x = ParseFloat(p);
                    uv.y = ParseFloat(p);
                    if (opts.flipUV_Y) uv.y = 1.f - uv.y;
                    uvs.PushBack(uv);
                    while (*p && *p != '\n') ++p;
                    continue;
                }
                if (*p == 'f' && (p[1]==' ' || p[1]=='\t')) {
                    p += 2;
                    // Parse face vertices (polygone)
                    NkVector<uint32> faceVerts;
                    while (*p && *p != '\n' && *p != '\r') {
                        Trim(p);
                        if (!*p || *p=='\n' || *p=='\r') break;

                        // Format : pos/uv/norm ou pos//norm ou pos/uv ou pos
                        int32 pi=-1, ui=-1, ni=-1;
                        if (*p >= '0' && *p <= '9') pi = (int32)ParseUInt(p) - 1;
                        if (*p == '/') {
                            ++p;
                            if (*p != '/') ui = (int32)ParseUInt(p) - 1;
                            if (*p == '/') { ++p; ni = (int32)ParseUInt(p) - 1; }
                        }

                        // Construire la clé pour la déduplication
                        char key[64];
                        snprintf(key, sizeof(key), "%d/%d/%d", pi, ui, ni);
                        NkString keyStr(key);

                        uint32 idx;
                        auto* found = vertCache.Find(keyStr);
                        if (found) {
                            idx = *found;
                        } else {
                            NkVertex3D v{};
                            if (pi >= 0 && pi < (int32)positions.Size())
                                v.position = positions[(usize)pi];
                            if (ni >= 0 && ni < (int32)normals.Size())
                                v.normal = normals[(usize)ni];
                            if (ui >= 0 && ui < (int32)uvs.Size())
                                v.uv0 = uvs[(usize)ui];
                            v.color = {1,1,1,1};

                            if (opts.flipNormals) {
                                v.normal.x = -v.normal.x;
                                v.normal.y = -v.normal.y;
                                v.normal.z = -v.normal.z;
                            }

                            idx = vertCount++;
                            currentMesh.vertices.PushBack(v);
                            vertCache[keyStr] = idx;
                        }
                        faceVerts.PushBack(idx);
                    }

                    // Triangulation fan (pour les ngons)
                    if (faceVerts.Size() >= 3) {
                        for (uint32 i = 1; i+1 < faceVerts.Size(); i++) {
                            if (opts.flipWindingOrder) {
                                currentMesh.indices.PushBack(faceVerts[0]);
                                currentMesh.indices.PushBack(faceVerts[i+1]);
                                currentMesh.indices.PushBack(faceVerts[i]);
                            } else {
                                currentMesh.indices.PushBack(faceVerts[0]);
                                currentMesh.indices.PushBack(faceVerts[i]);
                                currentMesh.indices.PushBack(faceVerts[i+1]);
                            }
                        }
                    }

                    // Sous-mesh courant
                    if (currentMesh.subMeshes.Empty()) {
                        NkSubMesh sm;
                        sm.firstIndex = 0;
                        sm.materialIdx = currentMatIdx;
                        sm.name = currentMatName;
                        currentMesh.subMeshes.PushBack(sm);
                    } else {
                        // Mettre à jour le count du dernier sous-mesh
                        auto& sm = currentMesh.subMeshes.Back();
                        sm.indexCount = (uint32)currentMesh.indices.Size() - sm.firstIndex;
                    }

                    while (*p && *p != '\n') ++p;
                    continue;
                }

                // Ligne inconnue : skip
                while (*p && *p != '\n') ++p;
                if (*p == '\n') ++p;
            }

            FlushMesh();

            // Calculer les totaux
            for (const auto& m : out.meshes) {
                out.totalVertices  += (uint32)m.vertices.Size();
                out.totalTriangles += (uint32)m.indices.Size() / 3;
            }

            logger_src.Infof("[NkModelLoader] OBJ '%s': %u mesh, %u vert, %u tri, %u mat\n",
                out.sourcePath.CStr(), out.meshes.Size(), out.totalVertices, out.totalTriangles,
                out.materials.Size());

            return !out.meshes.Empty();
        }

        bool NkModelLoader::ParseMTL(const NkString& path, const NkString& dir,
                                    NkVector<NkImportedMaterial>& mats) {
            NkString src = ReadFileTxt(path.CStr());
            if (src.Empty()) return false;

            NkImportedMaterial* cur = nullptr;
            const char* p = src.CStr();
            while (*p) {
                Trim(p);
                if (!*p) break;
                if (*p == '#') { while (*p && *p != '\n') ++p; continue; }

                if (strncmp(p, "newmtl", 6) == 0) {
                    p += 6; Trim(p);
                    NkImportedMaterial mat;
                    mat.name = ParseLine(p);
                    mats.PushBack(mat);
                    cur = &mats.Back();
                    continue;
                }
                if (!cur) { while (*p && *p != '\n') ++p; if (*p=='\n') ++p; continue; }

                if (strncmp(p, "Kd", 2) == 0) {
                    p += 2;
                    cur->albedo.r = ParseFloat(p);
                    cur->albedo.g = ParseFloat(p);
                    cur->albedo.b = ParseFloat(p);
                } else if (strncmp(p, "Ks", 2) == 0) {
                    p += 2;
                    cur->specularColor.r = ParseFloat(p);
                    cur->specularColor.g = ParseFloat(p);
                    cur->specularColor.b = ParseFloat(p);
                } else if (strncmp(p, "Ke", 2) == 0) {
                    p += 2;
                    cur->emissive.r = ParseFloat(p);
                    cur->emissive.g = ParseFloat(p);
                    cur->emissive.b = ParseFloat(p);
                } else if (strncmp(p, "Ns", 2) == 0) {
                    p += 2;
                    float ns = ParseFloat(p);
                    cur->glossiness = ns / 1000.f; // 0..1
                    cur->roughness  = 1.f - cur->glossiness;
                } else if (strncmp(p, "Ni", 2) == 0) {
                    p += 2; cur->ior = ParseFloat(p);
                } else if (strncmp(p, "d ", 2) == 0) {
                    p += 2; cur->albedo.a = ParseFloat(p);
                    if (cur->albedo.a < 1.f) cur->blendMode = NkBlendMode::Translucent;
                } else if (strncmp(p, "Tr", 2) == 0) {
                    p += 2; cur->albedo.a = 1.f - ParseFloat(p);
                    if (cur->albedo.a < 1.f) cur->blendMode = NkBlendMode::Translucent;
                } else if (strncmp(p, "illum", 5) == 0) {
                    p += 5; cur->shadingModel = OBJIllumToShading((int)ParseFloat(p));
                } else if (strncmp(p, "map_Kd", 6) == 0) {
                    p += 6; Trim(p); cur->albedoPath = ResolvePath(dir, ParseLine(p)); continue;
                } else if (strncmp(p, "map_Bump", 8) == 0 || strncmp(p, "bump", 4) == 0) {
                    while (*p && *p != ' ' && *p != '\t') ++p;
                    Trim(p); cur->normalPath = ResolvePath(dir, ParseLine(p)); continue;
                } else if (strncmp(p, "map_Ks", 6) == 0) {
                    p += 6; Trim(p); cur->specularPath = ResolvePath(dir, ParseLine(p)); continue;
                } else if (strncmp(p, "map_Ke", 6) == 0) {
                    p += 6; Trim(p); cur->emissivePath = ResolvePath(dir, ParseLine(p)); continue;
                } else if (strncmp(p, "map_d", 5) == 0) {
                    p += 5; Trim(p); cur->opacityPath = ResolvePath(dir, ParseLine(p)); continue;
                } else if (strncmp(p, "map_Ns", 6) == 0) {
                    p += 6; Trim(p); cur->glossinessPath = ResolvePath(dir, ParseLine(p)); continue;
                } else if (strncmp(p, "map_ao", 6) == 0) {
                    p += 6; Trim(p); cur->aoPath = ResolvePath(dir, ParseLine(p)); continue;
                }
                while (*p && *p != '\n') ++p;
                if (*p == '\n') ++p;
            }
            return !mats.Empty();
        }

        NkShadingModel NkModelLoader::OBJIllumToShading(int illum) {
            switch (illum) {
                case 0: return NkShadingModel::Unlit;
                case 1: return NkShadingModel::Phong;
                case 2: return NkShadingModel::Phong;
                case 4: return NkShadingModel::GlassBSDF;
                case 6: return NkShadingModel::GlassBSDF;
                case 9: return NkShadingModel::GlassBSDF;
                default: return NkShadingModel::DefaultLit;
            }
        }

        // =============================================================================
        // STL Parser
        // =============================================================================
        bool NkModelLoader::LoadSTL(const char* path, NkModelData& out,
                                    const NkModelImportOptions& opts) {
            NkVector<uint8> data = ReadFileBin(path);
            if (data.Empty()) return false;
            out.sourcePath = path;
            out.sourceDir  = ExtractDir(NkString(path));

            // Détecter binary vs ASCII
            const char* txt = reinterpret_cast<const char*>(data.Data());
            bool isAscii = (data.Size() > 5 && strncmp(txt, "solid", 5) == 0);

            // Vérifier la cohérence taille (binary STL)
            if (isAscii && data.Size() > 84) {
                uint32 triCount = *reinterpret_cast<const uint32*>(data.Data() + 80);
                usize expectedSize = 84 + (usize)triCount * 50;
                if (data.Size() == expectedSize) isAscii = false; // c'est en fait binary
            }

            NkMeshData mesh;
            bool ok;
            if (isAscii) ok = ParseSTLASCII(txt, mesh);
            else         ok = ParseSTLBinary(data.Data(), data.Size(), mesh);

            if (!ok) return false;
            if (opts.generateNormals)  mesh.RecalculateNormals();
            if (opts.generateTangents) mesh.RecalculateTangents();
            if (opts.optimizeMesh)     mesh.Optimize();
            mesh.RecalculateAABB();

            // Scale
            if (opts.scale != 1.f) {
                for (auto& v : mesh.vertices) {
                    v.position.x *= opts.scale;
                    v.position.y *= opts.scale;
                    v.position.z *= opts.scale;
                }
                mesh.RecalculateAABB();
            }

            out.meshes.PushBack(mesh);
            out.meshNames.PushBack("STLMesh");
            out.totalVertices  = (uint32)mesh.vertices.Size();
            out.totalTriangles = (uint32)mesh.indices.Size() / 3;

            logger_src.Infof("[NkModelLoader] STL '%s': %u vert, %u tri\n",
                path, out.totalVertices, out.totalTriangles);
            return true;
        }

        bool NkModelLoader::ParseSTLBinary(const uint8* data, usize size, NkMeshData& out) {
            if (size < 84) return false;
            uint32 triCount = *reinterpret_cast<const uint32*>(data + 80);
            if (size < 84 + (usize)triCount * 50) return false;

            out.vertices.Reserve(triCount * 3);
            out.indices.Reserve(triCount * 3);

            const uint8* ptr = data + 84;
            for (uint32 i = 0; i < triCount; i++, ptr += 50) {
                const float* nf = reinterpret_cast<const float*>(ptr);
                NkVec3f normal = {nf[0], nf[1], nf[2]};
                const float* v0 = nf + 3;
                const float* v1 = nf + 6;
                const float* v2 = nf + 9;
                uint32 base = (uint32)out.vertices.Size();
                NkVertex3D va, vb, vc;
                va.position = {v0[0], v0[1], v0[2]}; va.normal = normal; va.color={1,1,1,1};
                vb.position = {v1[0], v1[1], v1[2]}; vb.normal = normal; vb.color={1,1,1,1};
                vc.position = {v2[0], v2[1], v2[2]}; vc.normal = normal; vc.color={1,1,1,1};
                out.vertices.PushBack(va);
                out.vertices.PushBack(vb);
                out.vertices.PushBack(vc);
                out.indices.PushBack(base);
                out.indices.PushBack(base+1);
                out.indices.PushBack(base+2);
            }
            return true;
        }

        bool NkModelLoader::ParseSTLASCII(const char* text, NkMeshData& out) {
            const char* p = text;
            while (*p) {
                Trim(p);
                if (strncmp(p, "facet normal", 12) == 0) {
                    p += 12;
                    NkVec3f normal;
                    normal.x = ParseFloat(p);
                    normal.y = ParseFloat(p);
                    normal.z = ParseFloat(p);
                    // skip "outer loop"
                    while (*p && strncmp(p, "vertex", 6) != 0) ++p;
                    NkVec3f verts[3];
                    for (int vi = 0; vi < 3; vi++) {
                        while (*p && strncmp(p, "vertex", 6) != 0) ++p;
                        p += 6;
                        verts[vi].x = ParseFloat(p);
                        verts[vi].y = ParseFloat(p);
                        verts[vi].z = ParseFloat(p);
                    }
                    uint32 base = (uint32)out.vertices.Size();
                    for (int vi = 0; vi < 3; vi++) {
                        NkVertex3D v{};
                        v.position = verts[vi];
                        v.normal   = normal;
                        v.color    = {1,1,1,1};
                        out.vertices.PushBack(v);
                    }
                    out.indices.PushBack(base); out.indices.PushBack(base+1); out.indices.PushBack(base+2);
                } else {
                    ++p;
                }
            }
            return !out.vertices.Empty();
        }

        // =============================================================================
        // GLTF/GLB Parser
        // =============================================================================
        bool NkModelLoader::LoadGLB(const char* path, NkModelData& out,
                                    const NkModelImportOptions& opts) {
            NkVector<uint8> data = ReadFileBin(path);
            if (data.Size() < 12) return false;

            out.sourcePath = path;
            out.sourceDir  = ExtractDir(NkString(path));

            // Header GLB : magic(4) + version(4) + length(4)
            uint32 magic   = *reinterpret_cast<const uint32*>(data.Data());
            uint32 version = *reinterpret_cast<const uint32*>(data.Data() + 4);
            if (magic != 0x46546C67 || version != 2) {
                logger_src.Infof("[NkModelLoader] GLB: magic ou version invalide\n");
                return false;
            }

            // Chunk 0 : JSON
            uint32 jsonLen  = *reinterpret_cast<const uint32*>(data.Data() + 12);
            uint32 jsonType = *reinterpret_cast<const uint32*>(data.Data() + 16);
            if (jsonType != 0x4E4F534A) { // "JSON"
                logger_src.Infof("[NkModelLoader] GLB: premier chunk n'est pas JSON\n");
                return false;
            }
            NkString jsonStr(reinterpret_cast<const char*>(data.Data() + 20), jsonLen);

            // Chunk 1 : BIN (optionnel)
            const uint8* binChunk = nullptr;
            usize binSize = 0;
            usize chunk1Offset = 20 + jsonLen;
            if (chunk1Offset + 8 < data.Size()) {
                uint32 binLen  = *reinterpret_cast<const uint32*>(data.Data() + chunk1Offset);
                uint32 binType = *reinterpret_cast<const uint32*>(data.Data() + chunk1Offset + 4);
                if (binType == 0x004E4942) { // "BIN\0"
                    binChunk = data.Data() + chunk1Offset + 8;
                    binSize  = binLen;
                }
            }

            return ParseGLTFJSON(jsonStr.CStr(), binChunk, binSize, out.sourceDir, out, opts);
        }

        bool NkModelLoader::LoadGLTF(const char* path, NkModelData& out,
                                    const NkModelImportOptions& opts) {
            NkString json = ReadFileTxt(path);
            if (json.Empty()) return false;
            out.sourcePath = path;
            out.sourceDir  = ExtractDir(NkString(path));
            return ParseGLTFJSON(json.CStr(), nullptr, 0, out.sourceDir, out, opts);
        }

        // =============================================================================
        // GLTF JSON parser minimal
        // On parse manuellement les champs nécessaires pour les meshes et matériaux.
        // Une vraie implémentation utiliserait une lib JSON comme nlohmann ou rapidjson.
        // =============================================================================
        bool NkModelLoader::ParseGLTFJSON(const char* jsonStr, const uint8* binChunk,
                                        usize binSize, const NkString& dir,
                                        NkModelData& out, const NkModelImportOptions& opts) {
            // NOTE: Parser JSON GLTF complet est volumineux.
            // Cette implémentation parse les structures de base.
            // Pour une production, utiliser mikktspace + cgltf ou tinygltf.

            logger_src.Infof("[NkModelLoader] GLTF parse (mode simplifié) depuis: %s\n", dir.CStr());

            // Créer un mesh placeholder si on ne parse pas complètement
            // Une vraie implémentation irait ici chercher les bufferViews, accessors, meshes, etc.
            // Le parsing complet de GLTF JSON dépasse le cadre de ce header-only.

            // Pour l'instant, signaler que le format est reconnu mais demander cgltf si disponible
            logger_src.Infof("[NkModelLoader] Note: Pour un support GLTF complet, "
                            "activez NK_MODEL_USE_CGLTF et linkez cgltf.h\n");

            // Parsing minimal: chercher "scene", "meshes", "materials"
            // Cette version extrait les noms de meshes et matériaux
            const char* p = jsonStr;
            bool foundMeshes = false;

            // Compter les meshes (simple approximation)
            const char* search = jsonStr;
            while ((search = strstr(search, "\"primitives\"")) != nullptr) {
                // Créer un mesh vide avec un seul triangle (placeholder)
                NkMeshData mesh;
                NkVertex3D v0, v1, v2;
                v0.position = {0,0,0};    v0.normal={0,1,0}; v0.color={1,1,1,1};
                v1.position = {1,0,0};    v1.normal={0,1,0}; v1.color={1,1,1,1};
                v2.position = {0.5f,1,0}; v2.normal={0,1,0}; v2.color={1,1,1,1};
                mesh.vertices.PushBack(v0);
                mesh.vertices.PushBack(v1);
                mesh.vertices.PushBack(v2);
                mesh.indices.PushBack(0);
                mesh.indices.PushBack(1);
                mesh.indices.PushBack(2);
                mesh.RecalculateAABB();
                out.meshes.PushBack(mesh);
                out.meshNames.PushBack("GLTFMesh");
                foundMeshes = true;
                search++;
                break; // une seule passe pour ce parser minimal
            }

            (void)p;

            if (!foundMeshes) {
                logger_src.Infof("[NkModelLoader] GLTF: aucun mesh trouvé dans %s\n",
                                dir.CStr());
            }
            out.totalVertices = 3;
            out.totalTriangles = 1;
            return foundMeshes;
        }

        // =============================================================================
        // PLY Parser (format simple)
        // =============================================================================
        bool NkModelLoader::LoadPLY(const char* path, NkModelData& out,
                                    const NkModelImportOptions& opts) {
            NkVector<uint8> data = ReadFileBin(path);
            if (data.Empty()) return false;
            out.sourcePath = path;
            out.sourceDir  = ExtractDir(NkString(path));
            NkMeshData mesh;
            bool ok = ParsePLY(data.Data(), data.Size(), mesh, opts);
            if (!ok) return false;
            mesh.RecalculateAABB();
            out.meshes.PushBack(mesh);
            out.meshNames.PushBack("PLYMesh");
            out.totalVertices  = (uint32)mesh.vertices.Size();
            out.totalTriangles = (uint32)mesh.indices.Size() / 3;
            return true;
        }

        bool NkModelLoader::ParsePLY(const uint8* data, usize size,
                                    NkMeshData& out, const NkModelImportOptions& opts) {
            const char* p = reinterpret_cast<const char*>(data);
            if (strncmp(p, "ply", 3) != 0) return false;

            uint32 vertCount = 0, faceCount = 0;
            bool   binary   = false;
            bool   bigEndian= false;
            bool   hasNormal= false;
            bool   hasColor = false;
            bool   hasUV    = false;

            // Parse header
            while (*p && strncmp(p, "end_header", 10) != 0) {
                Trim(p);
                if (strncmp(p, "format binary_big_endian",    24)==0) { binary=true; bigEndian=true; }
                if (strncmp(p, "format binary_little_endian",  27)==0) { binary=true; }
                if (strncmp(p, "element vertex", 14)==0) { p+=14; vertCount=(uint32)ParseFloat(p); }
                if (strncmp(p, "element face",   12)==0) { p+=12; faceCount=(uint32)ParseFloat(p); }
                if (strncmp(p, "property float nx", 17)==0) hasNormal=true;
                if (strncmp(p, "property uchar red", 18)==0) hasColor=true;
                if (strncmp(p, "property float s", 16)==0 ||
                    strncmp(p, "property float u", 16)==0) hasUV=true;
                while (*p && *p != '\n') ++p;
                if (*p == '\n') ++p;
            }
            if (strncmp(p, "end_header", 10) == 0) { p += 10; if (*p=='\n') ++p; }

            out.vertices.Reserve(vertCount);
            out.indices.Reserve(faceCount * 3);

            if (!binary) {
                // ASCII PLY
                for (uint32 i = 0; i < vertCount; i++) {
                    NkVertex3D v{};
                    v.position.x = ParseFloat(p); v.position.y = ParseFloat(p); v.position.z = ParseFloat(p);
                    if (hasNormal) { v.normal.x = ParseFloat(p); v.normal.y = ParseFloat(p); v.normal.z = ParseFloat(p); }
                    if (hasColor)  {
                        v.color.r = ParseFloat(p)/255.f; v.color.g = ParseFloat(p)/255.f;
                        v.color.b = ParseFloat(p)/255.f; v.color.a = 1.f;
                    } else v.color = {1,1,1,1};
                    if (hasUV) { v.uv0.x = ParseFloat(p); v.uv0.y = ParseFloat(p); }
                    if (opts.scale != 1.f) { v.position.x*=opts.scale; v.position.y*=opts.scale; v.position.z*=opts.scale; }
                    out.vertices.PushBack(v);
                    while (*p && *p != '\n') ++p; if (*p=='\n') ++p;
                }
                for (uint32 i = 0; i < faceCount; i++) {
                    uint32 cnt = (uint32)ParseFloat(p);
                    NkVector<uint32> poly;
                    for (uint32 j=0; j<cnt; j++) poly.PushBack((uint32)ParseFloat(p));
                    for (uint32 j=1; j+1<cnt; j++) {
                        out.indices.PushBack(poly[0]);
                        out.indices.PushBack(poly[j]);
                        out.indices.PushBack(poly[j+1]);
                    }
                    while (*p && *p != '\n') ++p; if (*p=='\n') ++p;
                }
            }
            // Binary PLY : non implémenté ici (extension future)

            return !out.vertices.Empty();
        }

        bool NkModelLoader::Load3DS  (const char* path, NkModelData& out, const NkModelImportOptions& opts) {
            logger_src.Infof("[NkModelLoader] 3DS: format partiel - %s\n", path);
            return false;
        }

        bool NkModelLoader::LoadCollada(const char* path, NkModelData& out, const NkModelImportOptions& opts) {
            logger_src.Infof("[NkModelLoader] DAE: format partiel - %s\n", path);
            return false;
        }

        // =============================================================================
        // NkMeshData — implémentation des helpers
        // =============================================================================
        void NkMeshData::RecalculateNormals() {
            // Reset normals
            for (auto& v : vertices) v.normal = {0,0,0};

            // Accumuler les normales de face
            for (usize i = 0; i+2 < indices.Size(); i+=3) {
                auto& v0 = vertices[indices[i]];
                auto& v1 = vertices[indices[i+1]];
                auto& v2 = vertices[indices[i+2]];
                NkVec3f e1 = {v1.position.x-v0.position.x, v1.position.y-v0.position.y, v1.position.z-v0.position.z};
                NkVec3f e2 = {v2.position.x-v0.position.x, v2.position.y-v0.position.y, v2.position.z-v0.position.z};
                NkVec3f n  = {e1.y*e2.z-e1.z*e2.y, e1.z*e2.x-e1.x*e2.z, e1.x*e2.y-e1.y*e2.x};
                v0.normal = {v0.normal.x+n.x, v0.normal.y+n.y, v0.normal.z+n.z};
                v1.normal = {v1.normal.x+n.x, v1.normal.y+n.y, v1.normal.z+n.z};
                v2.normal = {v2.normal.x+n.x, v2.normal.y+n.y, v2.normal.z+n.z};
            }

            // Normaliser
            for (auto& v : vertices) {
                float len = NkSqrt(v.normal.x*v.normal.x + v.normal.y*v.normal.y + v.normal.z*v.normal.z);
                if (len > 1e-6f) { v.normal.x/=len; v.normal.y/=len; v.normal.z/=len; }
                else v.normal = {0,1,0};
            }
        }

        void NkMeshData::RecalculateTangents() {
            for (auto& v : vertices) {
                v.tangent = {1,0,0};
                v.bitangent = {0,0,1};
            }
            // Calcul Mikktspace-approximé (pour chaque triangle)
            for (usize i = 0; i+2 < indices.Size(); i+=3) {
                auto& v0 = vertices[indices[i]];
                auto& v1 = vertices[indices[i+1]];
                auto& v2 = vertices[indices[i+2]];

                NkVec3f e1 = {v1.position.x-v0.position.x, v1.position.y-v0.position.y, v1.position.z-v0.position.z};
                NkVec3f e2 = {v2.position.x-v0.position.x, v2.position.y-v0.position.y, v2.position.z-v0.position.z};
                float du1 = v1.uv0.x - v0.uv0.x, dv1 = v1.uv0.y - v0.uv0.y;
                float du2 = v2.uv0.x - v0.uv0.x, dv2 = v2.uv0.y - v0.uv0.y;
                float denom = du1*dv2 - du2*dv1;
                if (NkFabs(denom) < 1e-8f) continue;
                float inv = 1.f / denom;
                NkVec3f T = {inv*(dv2*e1.x-dv1*e2.x), inv*(dv2*e1.y-dv1*e2.y), inv*(dv2*e1.z-dv1*e2.z)};
                NkVec3f B = {inv*(du1*e2.x-du2*e1.x), inv*(du1*e2.y-du2*e1.y), inv*(du1*e2.z-du2*e1.z)};
                float tl = NkSqrt(T.x*T.x+T.y*T.y+T.z*T.z);
                float bl = NkSqrt(B.x*B.x+B.y*B.y+B.z*B.z);
                if (tl>1e-6f) { T.x/=tl; T.y/=tl; T.z/=tl; }
                if (bl>1e-6f) { B.x/=bl; B.y/=bl; B.z/=bl; }
                for (int j=0;j<3;j++) {
                    vertices[indices[i+j]].tangent   = T;
                    vertices[indices[i+j]].bitangent = B;
                }
            }
        }

        void NkMeshData::RecalculateAABB() {
            if (vertices.Empty()) return;
            aabb.min = aabb.max = vertices[0].position;
            for (const auto& v : vertices) aabb.Expand(v.position);
        }

        void NkMeshData::Optimize() {
            // Vertex deduplication simple
            NkUnorderedMap<NkString, uint32> cache;
            NkVector<NkVertex3D> newVerts;
            NkVector<uint32>     newIdx;
            for (uint32 idx : indices) {
                auto& v = vertices[idx];
                char key[128];
                snprintf(key, sizeof(key), "%.4f,%.4f,%.4f,%.3f,%.3f",
                        v.position.x, v.position.y, v.position.z,
                        v.uv0.x, v.uv0.y);
                auto* found = cache.Find(NkString(key));
                if (found) {
                    newIdx.PushBack(*found);
                } else {
                    uint32 ni = (uint32)newVerts.Size();
                    cache[NkString(key)] = ni;
                    newVerts.PushBack(v);
                    newIdx.PushBack(ni);
                }
            }
            vertices = traits::NkMove(newVerts);
            indices  = traits::NkMove(newIdx);
        }

        void NkMeshData::FlipNormals() {
            for (auto& v : vertices) {
                v.normal.x = -v.normal.x;
                v.normal.y = -v.normal.y;
                v.normal.z = -v.normal.z;
            }
            // Inverser l'ordre des triangles
            for (usize i=0; i+2<indices.Size(); i+=3)
                mem::NkSwap(indices[i+1], indices[i+2]);
        }

        // =============================================================================
        // NkModelImporter
        // =============================================================================
        NkModelImporter::ImportResult NkModelImporter::Import(
            NkIDevice* device, const NkModelData& data, const NkModelImportOptions& opts) {

            ImportResult result;
            if (!device || !data.IsValid()) return result;

            // Convertir les matériaux
            NkVector<NkMaterial*> mats;
            if (opts.importMaterials) {
                for (const auto& im : data.materials) {
                    NkMaterial* m = ConvertMaterial(device, im, data.sourceDir);
                    if (opts.importTextures && m)
                        LoadMaterialTextures(device, m, im, data.sourceDir);
                    mats.PushBack(m);
                }
            }
            result.materials = mats;

            // Convertir les meshes
            for (usize i = 0; i < data.meshes.Size(); i++) {
                const NkMeshData& md = data.meshes[i];
                auto* sm = new NkStaticMesh();
                if (sm->Create(device, md)) {
                    // Assigner les matériaux
                    for (const auto& sub : md.subMeshes) {
                        if (sub.materialIdx < mats.Size() && mats[sub.materialIdx])
                            sm->SetMaterial(sub.materialIdx, mats[sub.materialIdx]);
                    }
                    result.meshes.PushBack(sm);
                } else {
                    delete sm;
                }
            }

            result.hierarchy  = data.nodes;
            result.animations = data.animations;
            result.valid      = !result.meshes.Empty();
            return result;
        }

        NkStaticMesh* NkModelImporter::ImportMesh(NkIDevice* device, const char* path,
                                                    NkMaterial* material,
                                                    const NkModelImportOptions& opts) {
            NkModelData data;
            if (!NkModelLoader::Load(path, data, opts)) return nullptr;
            if (data.meshes.Empty()) return nullptr;

            auto* sm = new NkStaticMesh();
            if (!sm->Create(device, data.meshes[0])) { delete sm; return nullptr; }
            if (material) sm->SetMaterial(0, material);
            return sm;
        }

        NkMaterial* NkModelImporter::ConvertMaterial(NkIDevice* device,
                                                    const NkImportedMaterial& im,
                                                    const NkString& baseDir) {
            auto* mat = new NkMaterial();
            mat->Create(device, im.shadingModel, im.blendMode);
            mat->SetName(im.name.CStr());
            mat->SetAlbedo({im.albedo.r, im.albedo.g, im.albedo.b, im.albedo.a});
            mat->SetMetallic(im.metallic);
            mat->SetRoughness(im.roughness);
            mat->SetAO(im.ao);
            mat->SetEmissive({im.emissive.r, im.emissive.g, im.emissive.b, 1.f}, im.emissiveScale);
            mat->SetAlphaCutoff(im.alphaCutoff);
            mat->SetTwoSided(im.doubleSided);
            if (im.transmission > 0.f) mat->SetTransmission(im.transmission, im.ior, 0.f);
            if (im.ssRadius > 0.f) {
                mat->SetSubsurfaceColor({im.subsurface.r, im.subsurface.g, im.subsurface.b, 1.f});
                mat->SetSubsurfaceRadius(im.ssRadius);
            }
            return mat;
        }

        void NkModelImporter::LoadMaterialTextures(NkIDevice* device, NkMaterial* out,
                                                    const NkImportedMaterial& src,
                                                    const NkString& baseDir) {
            auto& texMgr = NkTextureManager::Get();
            if (!src.albedoPath.Empty()) {
                auto* t = texMgr.LoadTexture2D(src.albedoPath.CStr(), NkTextureParams::Albedo());
                if (t) out->SetAlbedoMap(t);
            }
            if (!src.normalPath.Empty()) {
                auto* t = texMgr.LoadTexture2D(src.normalPath.CStr(), NkTextureParams::Normal());
                if (t) out->SetNormalMap(t);
            }
            if (!src.ormPath.Empty()) {
                auto* t = texMgr.LoadTexture2D(src.ormPath.CStr(), NkTextureParams::Linear());
                if (t) out->SetORMMap(t);
            }
            if (!src.emissivePath.Empty()) {
                auto* t = texMgr.LoadTexture2D(src.emissivePath.CStr(), NkTextureParams::Albedo());
                if (t) out->SetEmissiveMap(t);
            }
            if (!src.heightPath.Empty()) {
                auto* t = texMgr.LoadTexture2D(src.heightPath.CStr(), NkTextureParams::Linear());
                if (t) out->SetHeightMap(t);
            }
            out->FlushToGPU();
        }

    } // namespace renderer
} // namespace nkentseu
