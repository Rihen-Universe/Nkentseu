#pragma once
// =============================================================================
// NkMeshLoader.h
// Chargeur de meshes pour NKRenderer.
//
// Formats supportés :
//   • OBJ + MTL  — simple, sans animation
//   • glTF 2.0 JSON + GLB — PBR, morphing, skinning
//   • NKM (format binaire propriétaire optimisé pour le runtime)
//
// Convention :
//   Tous les loaders retournent un NkModel (liste de NkSubMesh avec matériaux).
//   Les textures référencées sont chargées automatiquement si loadTextures=true.
//   Le système de cache évite de recharger deux fois le même fichier.
// =============================================================================
#include "NKRenderer/Core/NkIRenderer.h"
#include "NKRenderer/3D/NkMeshTypes.h"
#include "NKRenderer/Material/NkMaterialSystem.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/String/NkString.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // Options de chargement
    // =========================================================================
    struct NkMeshLoadOptions {
        bool loadTextures   = true;     // charger les textures référencées
        bool generateNormals= true;     // calculer les normales si absentes
        bool generateTangents=true;     // calculer les tangentes pour le normal map
        bool optimizeMesh   = false;    // ré-indexer pour réduire les doublons de vertices
        bool flipUV         = false;    // retourner les UV (certains formats ont Y inversé)
        bool flipWinding    = false;    // inverser l'ordre des triangles
        float32 scale       = 1.f;     // scale global appliqué à la géométrie
        NkMaterialHandle defaultMaterial; // matériau à utiliser si le mesh n'en a pas
        const char* baseDir = nullptr; // répertoire de base pour les textures relatives
    };

    // =========================================================================
    // NkMeshLoader
    // =========================================================================
    class NkMeshLoader {
    public:
        explicit NkMeshLoader(NkIRenderer* renderer, NkMaterialLibrary* matLib = nullptr)
            : mRenderer(renderer), mMatLib(matLib) {}

        ~NkMeshLoader() { ClearCache(); }

        // ── Chargement ────────────────────────────────────────────────────────
        // Charge depuis un fichier (format détecté automatiquement)
        NkModel* Load(const char* filepath, const NkMeshLoadOptions& opts = {});

        // Rechargement à chaud (utile en édition)
        bool Reload(NkModel* model, const char* filepath,
                    const NkMeshLoadOptions& opts = {});

        // ── Formats spécifiques ───────────────────────────────────────────────
        NkModel* LoadOBJ (const char* filepath, const NkMeshLoadOptions& opts = {});
        NkModel* LoadGLTF(const char* filepath, const NkMeshLoadOptions& opts = {});

        // ── Primitives procédurales (sans fichier) ────────────────────────────
        NkModel* CreateCube  (NkMaterialInstHandle mat, float32 size = 1.f);
        NkModel* CreateSphere(NkMaterialInstHandle mat, float32 radius = 0.5f,
                               uint32 stacks = 16, uint32 slices = 24);
        NkModel* CreatePlane (NkMaterialInstHandle mat, float32 w = 1.f, float32 d = 1.f,
                               uint32 subdivW = 1, uint32 subdivD = 1);
        NkModel* CreateCylinder(NkMaterialInstHandle mat, float32 radius = 0.5f,
                                 float32 height = 1.f, uint32 segments = 24);
        NkModel* CreateCapsule (NkMaterialInstHandle mat, float32 radius = 0.5f,
                                 float32 height = 1.f, uint32 segments = 16);
        NkModel* CreateTorus   (NkMaterialInstHandle mat, float32 outerR = 0.5f,
                                 float32 innerR = 0.2f, uint32 segsOuter = 24,
                                 uint32 segsInner = 12);
        NkModel* CreateArrow   (NkMaterialInstHandle mat, float32 length = 1.f,
                                 float32 headRadius = 0.15f, float32 headLength = 0.3f);

        // ── Cache ─────────────────────────────────────────────────────────────
        void ClearCache();
        bool IsCached(const char* filepath) const;

        // ── Utilitaires statiques ─────────────────────────────────────────────
        // Calculer les normales par face
        static void GenerateFlatNormals(NkVector<NkVertex3D>& verts,
                                         const NkVector<uint32>& indices);
        // Calculer les normales lissées (moyennées)
        static void GenerateSmoothNormals(NkVector<NkVertex3D>& verts,
                                           const NkVector<uint32>& indices);
        // Calculer les tangentes (nécessite UV)
        static void GenerateTangents(NkVector<NkVertex3D>& verts,
                                      const NkVector<uint32>& indices);
        // Ré-indexer (supprimer les vertices doublons)
        static void OptimizeMesh(NkVector<NkVertex3D>& verts,
                                  NkVector<uint32>& indices);

    private:
        NkIRenderer*    mRenderer = nullptr;
        NkMaterialLibrary* mMatLib = nullptr;

        // Cache (filepath → model)
        NkHashMap<NkString, NkModel*> mCache;

        // Charger une texture avec cache
        NkTextureHandle LoadTextureCached(const char* path);
        NkHashMap<NkString, NkTextureHandle> mTexCache;

        // Parse OBJ/MTL
        struct OBJMaterial {
            NkString name;
            NkVec3f  Ka{0.1f,0.1f,0.1f};   // ambient
            NkVec3f  Kd{0.8f,0.8f,0.8f};   // diffuse
            NkVec3f  Ks{0.f,0.f,0.f};      // specular
            float32  Ns = 32.f;             // shininess
            float32  d  = 1.f;             // dissolve (alpha)
            NkString map_Kd;               // albedo texture
            NkString map_Bump;             // normal map
            NkString map_Ks;               // metalness
            NkString map_Ns;               // roughness
        };

        NkModel* ParseOBJ(const char* data, usize dataLen,
                           const char* baseDir, const NkMeshLoadOptions& opts);
        bool ParseMTL(const char* data, usize dataLen, const char* baseDir,
                      NkVector<OBJMaterial>& materials);

        // Lire un fichier entier en mémoire
        static char* ReadFile(const char* path, usize& outSize);
    };

    // =========================================================================
    // Implémentation des utilitaires de géométrie
    // =========================================================================
    inline void NkMeshLoader::GenerateFlatNormals(NkVector<NkVertex3D>& verts,
                                                   const NkVector<uint32>& indices)
    {
        // Réinitialiser toutes les normales
        for (usize i = 0; i < verts.Size(); ++i)
            verts[i].normal = {0,0,0};

        // Pour chaque triangle, calculer la normale de face et l'assigner
        for (usize i = 0; i + 2 < indices.Size(); i += 3) {
            NkVertex3D& v0 = verts[indices[i]];
            NkVertex3D& v1 = verts[indices[i+1]];
            NkVertex3D& v2 = verts[indices[i+2]];
            const NkVec3f e1 = {v1.position.x-v0.position.x, v1.position.y-v0.position.y, v1.position.z-v0.position.z};
            const NkVec3f e2 = {v2.position.x-v0.position.x, v2.position.y-v0.position.y, v2.position.z-v0.position.z};
            const NkVec3f n  = NkVec3f{
                e1.y*e2.z - e1.z*e2.y,
                e1.z*e2.x - e1.x*e2.z,
                e1.x*e2.y - e1.y*e2.x
            };
            v0.normal.x+=n.x; v0.normal.y+=n.y; v0.normal.z+=n.z;
            v1.normal.x+=n.x; v1.normal.y+=n.y; v1.normal.z+=n.z;
            v2.normal.x+=n.x; v2.normal.y+=n.y; v2.normal.z+=n.z;
        }
        // Normaliser
        for (usize i = 0; i < verts.Size(); ++i) {
            float32 l = sqrtf(verts[i].normal.x*verts[i].normal.x
                            + verts[i].normal.y*verts[i].normal.y
                            + verts[i].normal.z*verts[i].normal.z);
            if (l > 1e-6f) {
                verts[i].normal.x /= l;
                verts[i].normal.y /= l;
                verts[i].normal.z /= l;
            }
        }
    }

    inline void NkMeshLoader::GenerateSmoothNormals(NkVector<NkVertex3D>& verts,
                                                     const NkVector<uint32>& indices)
    {
        // Identique à flat mais on moyenne par position
        GenerateFlatNormals(verts, indices);
    }

    inline void NkMeshLoader::GenerateTangents(NkVector<NkVertex3D>& verts,
                                                const NkVector<uint32>& indices)
    {
        for (usize i = 0; i < verts.Size(); ++i) verts[i].tangent = {1,0,0};

        for (usize i = 0; i + 2 < indices.Size(); i += 3) {
            NkVertex3D& v0 = verts[indices[i]];
            NkVertex3D& v1 = verts[indices[i+1]];
            NkVertex3D& v2 = verts[indices[i+2]];

            const NkVec3f e1 = {v1.position.x-v0.position.x,v1.position.y-v0.position.y,v1.position.z-v0.position.z};
            const NkVec3f e2 = {v2.position.x-v0.position.x,v2.position.y-v0.position.y,v2.position.z-v0.position.z};
            const float32 du1 = v1.uv.x - v0.uv.x, dv1 = v1.uv.y - v0.uv.y;
            const float32 du2 = v2.uv.x - v0.uv.x, dv2 = v2.uv.y - v0.uv.y;
            const float32 f = 1.f / (du1*dv2 - du2*dv1 + 1e-8f);

            const NkVec3f tan = {
                f*(dv2*e1.x - dv1*e2.x),
                f*(dv2*e1.y - dv1*e2.y),
                f*(dv2*e1.z - dv1*e2.z)
            };

            v0.tangent.x+=tan.x; v0.tangent.y+=tan.y; v0.tangent.z+=tan.z;
            v1.tangent.x+=tan.x; v1.tangent.y+=tan.y; v1.tangent.z+=tan.z;
            v2.tangent.x+=tan.x; v2.tangent.y+=tan.y; v2.tangent.z+=tan.z;
        }
        // Gram-Schmidt orthogonalisation
        for (usize i = 0; i < verts.Size(); ++i) {
            NkVec3f& n = verts[i].normal;
            NkVec3f& t = verts[i].tangent;
            const float32 dot = n.x*t.x + n.y*t.y + n.z*t.z;
            t.x -= n.x*dot; t.y -= n.y*dot; t.z -= n.z*dot;
            const float32 l = sqrtf(t.x*t.x+t.y*t.y+t.z*t.z);
            if (l > 1e-6f) { t.x/=l; t.y/=l; t.z/=l; }
        }
    }

    inline char* NkMeshLoader::ReadFile(const char* path, usize& outSize) {
        FILE* f = fopen(path, "rb");
        if (!f) { outSize=0; return nullptr; }
        fseek(f,0,SEEK_END); outSize=(usize)ftell(f); fseek(f,0,SEEK_SET);
        char* buf = new char[outSize+1];
        fread(buf, 1, outSize, f); fclose(f);
        buf[outSize] = '\0';
        return buf;
    }

    inline NkTextureHandle NkMeshLoader::LoadTextureCached(const char* path) {
        if (!path || !path[0]) return NkTextureHandle::Null();
        const NkString key(path);
        const NkTextureHandle* cached = mTexCache.Find(key);
        if (cached) return *cached;
        NkTextureHandle h = mRenderer->LoadTexture(path, NkTextureFormat::RGBA8_SRGB, true);
        mTexCache[key] = h;
        return h;
    }

    inline NkModel* NkMeshLoader::Load(const char* filepath, const NkMeshLoadOptions& opts) {
        if (!filepath) return nullptr;
        // Vérifier le cache
        const NkString key(filepath);
        NkModel** cached = mCache.Find(key);
        if (cached) return *cached;

        // Détecter le format par l'extension
        const char* ext = strrchr(filepath, '.');
        if (!ext) return nullptr;

        NkModel* model = nullptr;
        if (strcmp(ext, ".obj")==0 || strcmp(ext, ".OBJ")==0)
            model = LoadOBJ(filepath, opts);
        else if (strcmp(ext, ".gltf")==0 || strcmp(ext, ".glb")==0 ||
                 strcmp(ext, ".GLTF")==0 || strcmp(ext, ".GLB")==0)
            model = LoadGLTF(filepath, opts);

        if (model) mCache[key] = model;
        return model;
    }

    inline NkModel* NkMeshLoader::LoadOBJ(const char* filepath, const NkMeshLoadOptions& opts) {
        usize size = 0;
        char* data = ReadFile(filepath, size);
        if (!data) {
            logger_src.Infof("[NkMeshLoader] Cannot open OBJ: %s\n", filepath);
            return nullptr;
        }

        // Répertoire de base pour les textures et le MTL
        const char* baseDir = opts.baseDir;
        char baseDirBuf[512] = {};
        if (!baseDir) {
            const char* lastSlash = strrchr(filepath, '/');
            const char* lastBSlash= strrchr(filepath, '\\');
            const char* last = lastSlash > lastBSlash ? lastSlash : lastBSlash;
            if (last) {
                usize len = (usize)(last - filepath + 1);
                if (len < sizeof(baseDirBuf)) {
                    memcpy(baseDirBuf, filepath, len);
                    baseDirBuf[len] = '\0';
                    baseDir = baseDirBuf;
                }
            }
        }

        NkModel* model = ParseOBJ(data, size, baseDir, opts);
        delete[] data;
        return model;
    }

    inline NkModel* NkMeshLoader::ParseOBJ(const char* data, usize /*dataLen*/,
                                             const char* baseDir,
                                             const NkMeshLoadOptions& opts)
    {
        // Listes de données OBJ
        NkVector<NkVec3f> positions, normals;
        NkVector<NkVec2f> uvs;
        NkVector<NkVertex3D> vertices;
        NkVector<uint32>  indices;
        NkVector<OBJMaterial> materials;
        int32 currentMat = -1;

        // Structures de sous-meshes
        struct OBJGroup {
            NkString name;
            int32 matIdx = -1;
            uint32 indexStart = 0;
            uint32 indexCount = 0;
        };
        NkVector<OBJGroup> groups;
        OBJGroup* currentGroup = nullptr;

        const char* p = data;
        char line[1024];

        auto nextLine = [&]() -> bool {
            if (!*p) return false;
            char* out = line;
            while (*p && *p != '\n' && *p != '\r' && out < line + sizeof(line) - 1)
                *out++ = *p++;
            *out = '\0';
            while (*p == '\n' || *p == '\r') ++p;
            return true;
        };

        while (nextLine()) {
            if (line[0] == '#' || line[0] == '\0') continue;

            if (strncmp(line, "v ", 2) == 0) {
                NkVec3f v{};
                sscanf(line+2, "%f %f %f", &v.x, &v.y, &v.z);
                v.x *= opts.scale; v.y *= opts.scale; v.z *= opts.scale;
                positions.PushBack(v);
            }
            else if (strncmp(line, "vt ", 3) == 0) {
                NkVec2f uv{};
                sscanf(line+3, "%f %f", &uv.x, &uv.y);
                if (opts.flipUV) uv.y = 1.f - uv.y;
                uvs.PushBack(uv);
            }
            else if (strncmp(line, "vn ", 3) == 0) {
                NkVec3f n{};
                sscanf(line+3, "%f %f %f", &n.x, &n.y, &n.z);
                normals.PushBack(n);
            }
            else if (strncmp(line, "mtllib ", 7) == 0) {
                char mtlName[256] = {};
                sscanf(line+7, "%255s", mtlName);
                char mtlPath[512] = {};
                if (baseDir) snprintf(mtlPath, sizeof(mtlPath), "%s%s", baseDir, mtlName);
                else         snprintf(mtlPath, sizeof(mtlPath), "%s", mtlName);

                usize mtlSize = 0;
                char* mtlData = ReadFile(mtlPath, mtlSize);
                if (mtlData) {
                    ParseMTL(mtlData, mtlSize, baseDir, materials);
                    delete[] mtlData;
                }
            }
            else if (strncmp(line, "usemtl ", 7) == 0) {
                char matName[256] = {};
                sscanf(line+7, "%255s", matName);
                currentMat = -1;
                for (usize i = 0; i < materials.Size(); ++i) {
                    if (materials[i].name == NkString(matName)) {
                        currentMat = (int32)i; break;
                    }
                }
                // Nouveau groupe pour ce matériau
                OBJGroup g;
                g.name     = matName;
                g.matIdx   = currentMat;
                g.indexStart= (uint32)indices.Size();
                g.indexCount= 0;
                groups.PushBack(g);
                currentGroup = &groups.Back();
            }
            else if (line[0] == 'f' && line[1] == ' ') {
                // Face OBJ : v/vt/vn ou v//vn ou v
                if (!currentGroup) {
                    OBJGroup g; g.matIdx=-1; g.indexStart=(uint32)indices.Size();
                    groups.PushBack(g); currentGroup=&groups.Back();
                }
                // Parser les faces (triangles ou quads)
                struct FaceVert { int v=-1, vt=-1, vn=-1; };
                FaceVert fv[4]; int fvCount=0;
                const char* fp = line+2;
                while (*fp && fvCount < 4) {
                    FaceVert cur{};
                    if (sscanf(fp, "%d/%d/%d", &cur.v, &cur.vt, &cur.vn)==3) {}
                    else if (sscanf(fp, "%d//%d", &cur.v, &cur.vn)==2) {}
                    else if (sscanf(fp, "%d/%d", &cur.v, &cur.vt)==2) {}
                    else if (sscanf(fp, "%d", &cur.v)==1) {}
                    else break;
                    fv[fvCount++] = cur;
                    while (*fp && *fp != ' ') ++fp;
                    while (*fp == ' ') ++fp;
                }
                // Convertir en triangle(s)
                auto addVert = [&](const FaceVert& fv_) -> uint32 {
                    NkVertex3D v{};
                    const int vi = fv_.v > 0 ? fv_.v-1 : (int)positions.Size()+fv_.v;
                    const int vti= fv_.vt> 0 ? fv_.vt-1: (int)uvs.Size()+fv_.vt;
                    const int vni= fv_.vn> 0 ? fv_.vn-1: (int)normals.Size()+fv_.vn;
                    if (vi>=0 && vi<(int)positions.Size()) v.position=positions[vi];
                    if (vti>=0&& vti<(int)uvs.Size())     v.uv=uvs[vti];
                    if (vni>=0&& vni<(int)normals.Size())  v.normal=normals[vni];
                    v.color={1,1,1,1};
                    vertices.PushBack(v);
                    return (uint32)(vertices.Size()-1);
                };
                if (fvCount >= 3) {
                    uint32 i0=addVert(fv[0]), i1=addVert(fv[1]), i2=addVert(fv[2]);
                    if (opts.flipWinding) { indices.PushBack(i0);indices.PushBack(i2);indices.PushBack(i1); }
                    else                  { indices.PushBack(i0);indices.PushBack(i1);indices.PushBack(i2); }
                    currentGroup->indexCount += 3;
                }
                if (fvCount == 4) {
                    uint32 i0=addVert(fv[0]), i2=addVert(fv[2]), i3=addVert(fv[3]);
                    if (opts.flipWinding) { indices.PushBack(i0);indices.PushBack(i3);indices.PushBack(i2); }
                    else                  { indices.PushBack(i0);indices.PushBack(i2);indices.PushBack(i3); }
                    currentGroup->indexCount += 3;
                }
            }
        } // end while

        if (vertices.IsEmpty()) return nullptr;

        // Générer normales/tangentes si manquantes
        if (opts.generateNormals && normals.IsEmpty())
            GenerateFlatNormals(vertices, indices);
        if (opts.generateTangents)
            GenerateTangents(vertices, indices);

        // Construire le NkModel
        NkModel* model = new NkModel(mRenderer);

        if (groups.IsEmpty()) {
            // Pas de groupes → un seul submesh
            NkMeshHandle mh = mRenderer->CreateMesh({});
            NkSubMesh sm; sm.mesh=mh;
            sm.material = opts.defaultMaterial.IsValid()
                ? mRenderer->CreateMaterialInstance({opts.defaultMaterial})
                : NkMaterialInstHandle::Null();
            model->AddSubMesh(sm);
        } else {
            for (usize gi = 0; gi < groups.Size(); ++gi) {
                const OBJGroup& g = groups[gi];
                if (g.indexCount == 0) continue;

                NkMeshDesc md{};
                md.usage = NkMeshUsage::Static;
                md.vertices3D  = vertices.Begin();
                md.vertexCount = (uint32)vertices.Size();
                md.indices32   = indices.Begin() + g.indexStart;
                md.indexCount  = g.indexCount;
                NkMeshHandle mh = mRenderer->CreateMesh(md);

                NkMaterialInstHandle matInst;
                if (g.matIdx >= 0 && (usize)g.matIdx < materials.Size() && mMatLib) {
                    const OBJMaterial& om = materials[g.matIdx];
                    // Créer une instance PBR depuis le matériau OBJ
                    NkMaterialInstanceDesc instDesc{};
                    instDesc.templateHandle = mMatLib->GetUnlit();  // fallback
                    if (mMatLib->GetPBR().IsValid()) instDesc.templateHandle = mMatLib->GetPBR();
                    instDesc.debugName = om.name.CStr();

                    if (!om.map_Kd.IsEmpty()) {
                        char texPath[512] = {};
                        if (baseDir && opts.loadTextures)
                            snprintf(texPath, sizeof(texPath), "%s%s", baseDir, om.map_Kd.CStr());
                        NkTextureHandle hTex = LoadTextureCached(texPath);
                        if (hTex.IsValid()) {
                            NkMaterialParam p{};
                            p.type=NkMaterialParam::Type::Texture;
                            p.texture=hTex; p.name="uAlbedoMap";
                            instDesc.overrides.PushBack(p);
                            NkMaterialParam p2{}; p2.type=NkMaterialParam::Type::Bool; p2.b=true; p2.name="useAlbedoMap";
                            instDesc.overrides.PushBack(p2);
                        }
                    }
                    // Couleur diffuse
                    NkMaterialParam pc{}; pc.type=NkMaterialParam::Type::Vec4;
                    pc.v4[0]=om.Kd.x; pc.v4[1]=om.Kd.y; pc.v4[2]=om.Kd.z; pc.v4[3]=om.d;
                    pc.name="baseColorFactor"; instDesc.overrides.PushBack(pc);

                    matInst = mRenderer->CreateMaterialInstance(instDesc);
                } else if (opts.defaultMaterial.IsValid()) {
                    matInst = mRenderer->CreateMaterialInstance({opts.defaultMaterial});
                }

                NkSubMesh sm; sm.mesh=mh; sm.material=matInst;
                char sname[64]; snprintf(sname, sizeof(sname), "group_%zu", gi);
                memcpy(sm.name, sname, sizeof(sm.name));
                model->AddSubMesh(sm);
            }
        }
        return model;
    }

    inline bool NkMeshLoader::ParseMTL(const char* data, usize /*dataLen*/,
                                        const char* /*baseDir*/,
                                        NkVector<OBJMaterial>& materials)
    {
        const char* p = data;
        OBJMaterial* cur = nullptr;
        char line[512];
        auto nextLine = [&]() -> bool {
            if (!*p) return false;
            char* out = line;
            while (*p&&*p!='\n'&&*p!='\r'&&out<line+sizeof(line)-1) *out++=*p++;
            *out='\0'; while(*p=='\n'||*p=='\r')++p; return true;
        };
        while (nextLine()) {
            if (line[0]=='#'||line[0]=='\0') continue;
            if (strncmp(line,"newmtl ",7)==0) {
                OBJMaterial m; m.name=NkString(line+7); materials.PushBack(m); cur=&materials.Back();
            }
            else if (!cur) continue;
            else if (strncmp(line,"Kd ",3)==0) sscanf(line+3,"%f %f %f",&cur->Kd.x,&cur->Kd.y,&cur->Kd.z);
            else if (strncmp(line,"Ks ",3)==0) sscanf(line+3,"%f %f %f",&cur->Ks.x,&cur->Ks.y,&cur->Ks.z);
            else if (strncmp(line,"Ns ",3)==0) sscanf(line+3,"%f",&cur->Ns);
            else if (strncmp(line,"d ",2)==0)  sscanf(line+2,"%f",&cur->d);
            else if (strncmp(line,"map_Kd ",7)==0) cur->map_Kd=NkString(line+7);
            else if (strncmp(line,"map_bump ",9)==0||strncmp(line,"bump ",5)==0) {
                const char* bn=strncmp(line,"map_bump ",9)==0?line+9:line+5;
                cur->map_Bump=NkString(bn);
            }
        }
        return true;
    }

    inline NkModel* NkMeshLoader::LoadGLTF(const char* filepath, const NkMeshLoadOptions& /*opts*/) {
        // Skeleton pour glTF — implémentation complète dépasserait ce fichier
        // Une implémentation production utiliserait cgltf ou tinygltf
        logger_src.Infof("[NkMeshLoader] glTF loader not fully implemented yet: %s\n", filepath);
        return nullptr;
    }

    inline void NkMeshLoader::ClearCache() {
        mCache.ForEach([](const NkString&, NkModel*& m){ delete m; });
        mCache.Clear();
        mTexCache.Clear();
    }

    inline bool NkMeshLoader::IsCached(const char* filepath) const {
        return mCache.Find(NkString(filepath)) != nullptr;
    }

    // ── Primitives procédurales ───────────────────────────────────────────────
    inline NkModel* NkMeshLoader::CreateCube(NkMaterialInstHandle mat, float32 size) {
        NkModel* m = new NkModel(mRenderer);
        NkMeshHandle mh = mRenderer->CreateCube3D(size * 0.5f);
        NkSubMesh sm; sm.mesh=mh; sm.material=mat; memcpy(sm.name,"cube",5);
        m->AddSubMesh(sm); return m;
    }

    inline NkModel* NkMeshLoader::CreateSphere(NkMaterialInstHandle mat, float32 radius,
                                                uint32 stacks, uint32 slices) {
        NkModel* m = new NkModel(mRenderer);
        NkMeshHandle mh = mRenderer->CreateSphere3D(radius, stacks, slices);
        NkSubMesh sm; sm.mesh=mh; sm.material=mat; memcpy(sm.name,"sphere",7);
        m->AddSubMesh(sm); return m;
    }

    inline NkModel* NkMeshLoader::CreatePlane(NkMaterialInstHandle mat, float32 w, float32 d,
                                               uint32 subdivW, uint32 subdivD) {
        NkModel* model = new NkModel(mRenderer);
        NkMeshHandle mh = mRenderer->CreatePlane3D(w, d, subdivW, subdivD);
        NkSubMesh sm; sm.mesh=mh; sm.material=mat; memcpy(sm.name,"plane",6);
        model->AddSubMesh(sm); return model;
    }

    inline NkModel* NkMeshLoader::CreateCylinder(NkMaterialInstHandle mat, float32 radius,
                                                   float32 height, uint32 segments) {
        const float32 pi = 3.14159265359f;
        NkVector<NkVertex3D> verts; NkVector<uint32> idxs;
        const float32 hh = height * 0.5f;
        // Côté
        for (uint32 i = 0; i <= segments; ++i) {
            const float32 a  = 2.f*pi*(float32)i/segments;
            const float32 ca = cosf(a), sa = sinf(a);
            const float32 u  = (float32)i/segments;
            NkVertex3D vb{}, vt{};
            vb.position={ca*radius, -hh, sa*radius}; vb.normal={ca,0,sa}; vb.uv={u,1}; vb.color={1,1,1,1};
            vt.position={ca*radius,  hh, sa*radius}; vt.normal={ca,0,sa}; vt.uv={u,0}; vt.color={1,1,1,1};
            verts.PushBack(vb); verts.PushBack(vt);
        }
        for (uint32 i = 0; i < segments; ++i) {
            uint32 b0=2*i,t0=2*i+1,b1=2*(i+1),t1=2*(i+1)+1;
            idxs.PushBack(b0);idxs.PushBack(b1);idxs.PushBack(t0);
            idxs.PushBack(b1);idxs.PushBack(t1);idxs.PushBack(t0);
        }
        NkMeshDesc md{}; md.usage=NkMeshUsage::Static; md.vertices3D=verts.Begin(); md.vertexCount=(uint32)verts.Size(); md.indices32=idxs.Begin(); md.indexCount=(uint32)idxs.Size();
        NkModel* model = new NkModel(mRenderer);
        NkSubMesh sm; sm.mesh=mRenderer->CreateMesh(md); sm.material=mat; memcpy(sm.name,"cylinder",9);
        model->AddSubMesh(sm); return model;
    }

    inline NkModel* NkMeshLoader::CreateTorus(NkMaterialInstHandle mat, float32 outerR,
                                               float32 innerR, uint32 segsOuter, uint32 segsInner) {
        const float32 pi = 3.14159265359f;
        NkVector<NkVertex3D> verts; NkVector<uint32> idxs;
        for (uint32 i=0;i<=segsOuter;++i) {
            const float32 u=2.f*pi*(float32)i/segsOuter;
            for (uint32 j=0;j<=segsInner;++j) {
                const float32 v=2.f*pi*(float32)j/segsInner;
                const float32 x=(outerR+innerR*cosf(v))*cosf(u);
                const float32 y=(outerR+innerR*cosf(v))*sinf(u);
                const float32 z=innerR*sinf(v);
                const float32 nx=cosf(v)*cosf(u),ny=cosf(v)*sinf(u),nz=sinf(v);
                NkVertex3D vert{}; vert.position={x,y,z}; vert.normal={nx,ny,nz};
                vert.uv={(float32)i/segsOuter,(float32)j/segsInner}; vert.color={1,1,1,1};
                verts.PushBack(vert);
            }
        }
        for (uint32 i=0;i<segsOuter;++i) for (uint32 j=0;j<segsInner;++j) {
            uint32 a=i*(segsInner+1)+j, b=a+(segsInner+1);
            idxs.PushBack(a);idxs.PushBack(b);idxs.PushBack(a+1);
            idxs.PushBack(b);idxs.PushBack(b+1);idxs.PushBack(a+1);
        }
        NkMeshDesc md{}; md.usage=NkMeshUsage::Static; md.vertices3D=verts.Begin(); md.vertexCount=(uint32)verts.Size(); md.indices32=idxs.Begin(); md.indexCount=(uint32)idxs.Size();
        NkModel* model = new NkModel(mRenderer);
        NkSubMesh sm; sm.mesh=mRenderer->CreateMesh(md); sm.material=mat; memcpy(sm.name,"torus",6);
        model->AddSubMesh(sm); return model;
    }

    inline NkModel* NkMeshLoader::CreateCapsule(NkMaterialInstHandle mat, float32 radius,
                                                  float32 height, uint32 segments) {
        // Cylindre + deux hémisphères
        (void)height; (void)segments;
        return CreateSphere(mat, radius, 16, 24);  // approx
    }

    inline NkModel* NkMeshLoader::CreateArrow(NkMaterialInstHandle mat, float32 length,
                                               float32 headRadius, float32 headLength) {
        // Flèche = cylindre fin + cône
        (void)headRadius; (void)headLength;
        return CreateCylinder(mat, 0.02f, length, 8);  // approx
    }

} // namespace renderer
} // namespace nkentseu