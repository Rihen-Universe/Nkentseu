#pragma once
// =============================================================================
// NkMeshSystem.h  v2.0
// Système de mesh complet avec SOUS-MESHES, layouts custom, import multi-format.
//
// SOUS-MESHES (SubMesh) :
//   Un NkMesh contient 1..N sous-meshes. Chaque sous-mesh :
//     - Partage les mêmes vertex buffers du mesh parent
//     - A son propre range d'indices [firstIndex, indexCount]
//     - A son propre matériau par défaut (depuis l'import)
//     - Peut avoir ses propres bounds AABB
//   Exemples d'usage :
//     - Un personnage 3D : body + head + hands + clothing (sous-meshes séparés)
//     - Un véhicule : carrosserie + vitres + roues (transparences différentes)
//     - Un terrain sectionné : plusieurs zones de matériaux
//     - Un immeuble : façade + fenêtres + toit
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDescs.h"
#include "NKMath/NkVec.h"
#include "NKMath/NkMat4.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Associative/NkUnorderedMap.h"

namespace nkentseu {

    class NkMaterialSystem;
    class NkTextureLibrary;
    using NkMatId = uint64;
    static constexpr NkMatId NK_INVALID_MAT = 0;

    // =========================================================================
    // AABB
    // =========================================================================
    struct NkAABB {
        math::NkVec3 min = {-0.5f,-0.5f,-0.5f};
        math::NkVec3 max = { 0.5f, 0.5f, 0.5f};
        math::NkVec3 Center()  const { return (min+max)*0.5f; }
        math::NkVec3 Extents() const { return (max-min)*0.5f; }
        float32 Radius()       const { auto e=Extents(); return sqrtf(e.x*e.x+e.y*e.y+e.z*e.z); }
        void Expand(math::NkVec3 p) {
            min.x=fminf(min.x,p.x); min.y=fminf(min.y,p.y); min.z=fminf(min.z,p.z);
            max.x=fmaxf(max.x,p.x); max.y=fmaxf(max.y,p.y); max.z=fmaxf(max.z,p.z);
        }
        void Merge(const NkAABB& o) { Expand(o.min); Expand(o.max); }
        static NkAABB Empty() { return {{1e9f,1e9f,1e9f},{-1e9f,-1e9f,-1e9f}}; }
    };

    // =========================================================================
    // VertexLayout définition
    // =========================================================================
    enum class NkVertexSemantic : uint8 {
        NK_SEM_POSITION=0, NK_SEM_NORMAL, NK_SEM_TANGENT, NK_SEM_BITANGENT,
        NK_SEM_COLOR, NK_SEM_TEXCOORD, NK_SEM_JOINTS, NK_SEM_WEIGHTS,
        NK_SEM_CUSTOM
    };

    struct NkVertexElement {
        NkVertexSemantic semantic    = NkVertexSemantic::NK_SEM_CUSTOM;
        uint32           semanticIdx = 0;
        NkGPUFormat      format      = NkGPUFormat::NK_RGB32_FLOAT;
        uint32           offset      = 0;
        uint32           location    = 0;
        NkString         name;
        const char*      hlslSemantic= nullptr;
        uint32           hlslSemIdx  = 0;
        static uint32    FormatSize(NkGPUFormat f);
    };

    struct NkVertexLayoutDef {
        NkVector<NkVertexElement> elements;
        uint32                    stride = 0;
        uint32                    binding= 0;
        bool                      perInstance = false;
        NkVertexLayout ToRHI() const;
        const NkVertexElement* Find(NkVertexSemantic sem, uint32 idx=0) const;
        bool Has(NkVertexSemantic sem, uint32 idx=0) const;
    };

    // =========================================================================
    // NkVertexLayoutBuilder — construction fluente
    // =========================================================================
    class NkVertexLayoutBuilder {
    public:
        NkVertexLayoutBuilder& AddPosition2F();
        NkVertexLayoutBuilder& AddPosition3F();
        NkVertexLayoutBuilder& AddPosition4F();
        NkVertexLayoutBuilder& AddNormal3F();
        NkVertexLayoutBuilder& AddTangent3F();
        NkVertexLayoutBuilder& AddTangent4F();
        NkVertexLayoutBuilder& AddBitangent3F();
        NkVertexLayoutBuilder& AddUV2F(uint32 ch=0);
        NkVertexLayoutBuilder& AddUV3F(uint32 ch=0);
        NkVertexLayoutBuilder& AddColor3F(uint32 idx=0);
        NkVertexLayoutBuilder& AddColor4F(uint32 idx=0);
        NkVertexLayoutBuilder& AddColor4UB(uint32 idx=0);
        NkVertexLayoutBuilder& AddJoints4UB();
        NkVertexLayoutBuilder& AddJoints4US();
        NkVertexLayoutBuilder& AddWeights4F();
        NkVertexLayoutBuilder& AddWeights4HF();
        NkVertexLayoutBuilder& AddFloat (const NkString& name="");
        NkVertexLayoutBuilder& AddFloat2(const NkString& name="");
        NkVertexLayoutBuilder& AddFloat3(const NkString& name="");
        NkVertexLayoutBuilder& AddFloat4(const NkString& name="");
        NkVertexLayoutBuilder& AddInt   (const NkString& name="");
        NkVertexLayoutBuilder& AddUInt  (const NkString& name="");
        NkVertexLayoutBuilder& SetBinding(uint32 b);
        NkVertexLayoutBuilder& SetPerInstance(bool v);
        NkVertexLayoutDef Build() const;
        uint32 ComputeStride()   const;

        // Génère le code GLSL des inputs depuis ce layout
        NkString GenerateGLSLInputs    (int glslVersion=460) const;
        NkString GenerateVkGLSLInputs  (int glslVersion=460) const;
        NkString GenerateHLSLStruct    (bool isInput=true)   const;
        NkString GenerateMSLStruct     (bool isInput=true)   const;
        // Génère un vertex shader minimal passthrough
        NkString GeneratePassthroughVert(NkShaderBackend backend) const;

    private:
        NkVector<NkVertexElement> mElements;
        uint32 mOffset=0, mBinding=0, mLoc=0;
        bool   mPerInstance=false;
        NkVertexLayoutBuilder& Add(NkVertexSemantic sem,uint32 semIdx,NkGPUFormat fmt,
                                    const NkString& name="",const char* hlslSem=nullptr,uint32 hlslIdx=0);
    };

    // =========================================================================
    // ID Mesh
    // =========================================================================
    using NkMeshId = uint64;
    static constexpr NkMeshId NK_INVALID_MESH = 0;

    // =========================================================================
    // SOUS-MESH — range d'indices + matériau au sein d'un NkMesh
    // =========================================================================
    struct NkSubMesh {
        NkString    name;
        uint32      firstIndex  = 0;       // offset dans l'index buffer du mesh parent
        uint32      indexCount  = 0;
        int32       vertexOffset= 0;       // base vertex pour les indices
        NkMatId     material    = NK_INVALID_MAT;  // matériau par défaut
        NkAABB      bounds;
        bool        visible     = true;
        bool        castShadow  = true;
        bool        receiveShadow= true;
        // LOD pour ce sous-mesh (niveaux d'indices alternatifs)
        struct LOD {
            uint32 firstIndex = 0;
            uint32 indexCount = 0;
            float32 screenSizeThreshold = 0.f;
        };
        NkVector<LOD> lods;  // lods[0] = LOD0 (haute résolution)
    };

    // =========================================================================
    // MESH GPU — ensemble de vertex/index buffers + liste de sous-meshes
    // =========================================================================
    struct NkMesh {
        NkMeshId            id            = NK_INVALID_MESH;
        NkString            name;

        // GPU Buffers
        NkBufferHandle      vertexBuffer;
        NkBufferHandle      indexBuffer;
        NkVertexLayoutDef   layout;

        // Géométrie globale
        uint32              totalVertices = 0;
        uint32              totalIndices  = 0;
        uint32              vertexStride  = 0;
        NkIndexFormat       indexFormat   = NkIndexFormat::NK_UINT32;
        NkPrimitiveTopology topology      = NkPrimitiveTopology::NK_TRIANGLE_LIST;
        NkAABB              bounds;        // AABB englobant tous les sous-meshes

        // Sous-meshes
        NkVector<NkSubMesh> submeshes;

        // Skinning
        bool                hasSkin       = false;
        uint32              boneCount     = 0;

        // Morph targets
        NkBufferHandle      morphBuffer;   // positions alternativas empilées
        uint32              morphCount    = 0;

        bool IsValid() const { return vertexBuffer.IsValid() && !submeshes.Empty(); }

        // Helpers
        const NkSubMesh*    GetSubMesh(uint32 idx) const;
        const NkSubMesh*    FindSubMesh(const NkString& name) const;
        uint32              GetSubMeshCount() const { return (uint32)submeshes.Size(); }
        NkAABB              ComputeBounds()   const;

        // LOD helper : retourne le bon firstIndex/indexCount selon la distance
        void GetLODRange(uint32 submeshIdx, float32 screenSize,
                          uint32& outFirst, uint32& outCount) const;
    };

    // =========================================================================
    // NkMeshBuilder — construction vertex-par-vertex avec sous-meshes
    // =========================================================================
    class NkMeshBuilder {
    public:
        explicit NkMeshBuilder(const NkVertexLayoutDef& layout,
                                NkPrimitiveTopology topo=NkPrimitiveTopology::NK_TRIANGLE_LIST);
        NkMeshBuilder() = default;
        void SetLayout(const NkVertexLayoutDef& layout);

        // ── Sous-meshes ────────────────────────────────────────────────────────
        uint32 BeginSubMesh(const NkString& name="", NkMatId mat=NK_INVALID_MAT);
        void   EndSubMesh();
        uint32 GetCurrentSubMesh() const { return mCurrentSub; }

        // ── Vertices ──────────────────────────────────────────────────────────
        NkMeshBuilder& BeginVertex();
        NkMeshBuilder& Position (float32 x,float32 y,float32 z=0.f,float32 w=1.f);
        NkMeshBuilder& Normal   (float32 x,float32 y,float32 z);
        NkMeshBuilder& Tangent  (float32 x,float32 y,float32 z,float32 w=1.f);
        NkMeshBuilder& UV       (float32 u,float32 v,uint32 ch=0);
        NkMeshBuilder& Color    (float32 r,float32 g,float32 b,float32 a=1.f,uint32 idx=0);
        NkMeshBuilder& Color    (uint8 r,uint8 g,uint8 b,uint8 a=255,uint32 idx=0);
        NkMeshBuilder& Joints   (uint8 j0,uint8 j1,uint8 j2,uint8 j3);
        NkMeshBuilder& Weights  (float32 w0,float32 w1,float32 w2,float32 w3);
        NkMeshBuilder& Custom1F (uint32 loc,float32 v);
        NkMeshBuilder& Custom2F (uint32 loc,float32 x,float32 y);
        NkMeshBuilder& Custom3F (uint32 loc,float32 x,float32 y,float32 z);
        NkMeshBuilder& Custom4F (uint32 loc,float32 x,float32 y,float32 z,float32 w);
        uint32         EndVertex();

        // ── Primitives ────────────────────────────────────────────────────────
        void Triangle (uint32 a,uint32 b,uint32 c);
        void Quad     (uint32 a,uint32 b,uint32 c,uint32 d);
        void Line     (uint32 a,uint32 b);
        void Point    (uint32 a);
        void Indices  (const uint32* idx,uint32 count);

        // ── Post-process ──────────────────────────────────────────────────────
        void ComputeNormals   (bool smooth=true);
        void ComputeTangents  ();
        void FlipNormals      ();
        void FlipWinding      ();
        void WeldVertices     (float32 eps=1e-5f);
        void Optimize         ();
        void Subdivide        (uint32 level=1,bool catmullClark=false);
        void ApplyTransform   (const math::NkMat4& m);
        void MergeWith        (const NkMeshBuilder& other);

        // ── Accesseurs ────────────────────────────────────────────────────────
        const NkVector<uint8>&    GetVertexData()   const { return mVtxData; }
        const NkVector<uint32>&   GetIndexData()    const { return mIdxData; }
        const NkVertexLayoutDef&  GetLayout()       const { return mLayout; }
        NkPrimitiveTopology        GetTopology()     const { return mTopology; }
        uint32                     GetVertexCount()  const;
        uint32                     GetIndexCount()   const { return (uint32)mIdxData.Size(); }
        bool                       IsEmpty()         const { return mVtxData.Empty(); }
        NkAABB                     ComputeBounds()   const;

        // Sous-meshes définis
        struct SubMeshRange {
            NkString name; NkMatId mat; uint32 firstIndex; uint32 indexCount;
        };
        const NkVector<SubMeshRange>& GetSubMeshRanges() const { return mSubRanges; }

        void Clear();
        void Reserve(uint32 vtx,uint32 idx);

    private:
        NkVertexLayoutDef   mLayout;
        NkPrimitiveTopology mTopology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
        NkVector<uint8>     mVtxData;
        NkVector<uint32>    mIdxData;
        uint32              mCurrentVtxStart = 0;
        bool                mInVertex        = false;

        uint32              mCurrentSub      = 0;
        uint32              mSubStartIndex   = 0;
        NkVector<SubMeshRange> mSubRanges;
        bool                mInSubMesh       = false;

        void SetAtOffset(uint32 off,const void* data,uint32 size);
    };

    // =========================================================================
    // Résultat de chargement
    // =========================================================================
    struct NkMeshLoadResult {
        bool success = false;
        NkString errors;
        NkVector<NkMeshId>   meshIds;     // tous les meshes chargés (avec sous-meshes)
        NkVector<NkString>   meshNames;
        // Hiérarchie de scène
        struct SceneNode {
            NkString         name;
            math::NkMat4     localTransform;
            int32            meshIndex = -1;    // -1 = pas de mesh
            int32            skinIndex = -1;
            NkVector<uint32> children;
        };
        NkVector<SceneNode>  nodes;
        // Animations
        struct AnimClipRef {
            NkString name;
            float32  duration = 0.f;
        };
        NkVector<AnimClipRef> animations;
        NkAABB               sceneBounds;

        // Helpers
        NkMeshId FindMesh(const NkString& name) const;
        NkVector<NkMeshId> GetAllMeshIds() const { return meshIds; }
    };

    // =========================================================================
    // NkMeshSystem — gestionnaire principal
    // =========================================================================
    class NkMeshSystem {
    public:
        explicit NkMeshSystem(NkIDevice* device, NkMaterialSystem* matSys=nullptr,
                               NkTextureLibrary* texLib=nullptr);
        ~NkMeshSystem();

        bool Initialize(uint64 maxVtxBytes=256ull*1024*1024,
                         uint64 maxIdxBytes= 64ull*1024*1024);
        void Shutdown();

        // ── Upload ────────────────────────────────────────────────────────────
        NkMeshId Upload(const NkMeshBuilder& builder, const NkString& name="mesh");
        NkMeshId UploadRaw(const NkVertexLayoutDef& layout,
                            const void* vtx, uint32 vtxCount,
                            const uint32* idx, uint32 idxCount,
                            const NkString& name="mesh");
        NkMeshId UploadMultiSub(const NkMeshBuilder& builder,
                                 const NkString& name="multimesh"); // idem Upload, mais préserve les sous-meshes

        // ── Mise à jour dynamique ─────────────────────────────────────────────
        bool UpdateVertices(NkMeshId id, const void* vtx, uint32 count, uint32 firstVtx=0);

        // ── Import ────────────────────────────────────────────────────────────
        NkMeshLoadResult LoadOBJ (const NkString& path, const struct NkImportOptions& opts={});
        NkMeshLoadResult LoadGLTF(const NkString& path, const struct NkImportOptions& opts={});
        NkMeshLoadResult LoadGLB (const NkString& path, const struct NkImportOptions& opts={});
        NkMeshLoadResult Load    (const NkString& path, const struct NkImportOptions& opts={});

        // ── Primitives pré-construites ────────────────────────────────────────
        NkMeshId GetQuad       (const NkVertexLayoutDef* lay=nullptr);
        NkMeshId GetCube       (const NkVertexLayoutDef* lay=nullptr);
        NkMeshId GetSphere     (uint32 subdivs=4,const NkVertexLayoutDef* lay=nullptr);
        NkMeshId GetIcosphere  (uint32 subdivs=3,const NkVertexLayoutDef* lay=nullptr);
        NkMeshId GetPlane      (uint32 subX=1,uint32 subZ=1,const NkVertexLayoutDef* lay=nullptr);
        NkMeshId GetCylinder   (uint32 slices=32,const NkVertexLayoutDef* lay=nullptr);
        NkMeshId GetCapsule    (uint32 rings=8,const NkVertexLayoutDef* lay=nullptr);
        NkMeshId GetScreenQuad ();
        NkMeshId GetSkyboxCube ();
        // Primitive multi-sous-meshes (ex: cube avec 6 sous-meshes = 6 faces)
        NkMeshId GetCubeFaces  ();   // 6 sous-meshes (1 par face)

        // ── Layouts standard ──────────────────────────────────────────────────
        static NkVertexLayoutDef GetStandardLayout ();
        static NkVertexLayoutDef GetSkinnedLayout  ();
        static NkVertexLayoutDef GetSimpleLayout   ();
        static NkVertexLayoutDef GetDebugLayout    ();
        static NkVertexLayoutDef GetParticleLayout ();
        static NkVertexLayoutDef GetTerrainLayout  ();
        static NkVertexLayoutDef GetSplineLayout   ();

        // ── Accès ─────────────────────────────────────────────────────────────
        const NkMesh* Get(NkMeshId id) const;
        bool          IsValid(NkMeshId id) const;
        void          Release(NkMeshId id);

        // ── Sous-meshes ────────────────────────────────────────────────────────
        // Matériaux des sous-meshes
        void         SetSubMeshMaterial(NkMeshId id,uint32 subIdx,NkMatId mat);
        void         SetSubMeshVisible (NkMeshId id,uint32 subIdx,bool v);
        uint32       GetSubMeshCount   (NkMeshId id) const;
        NkSubMesh*   GetSubMesh        (NkMeshId id,uint32 subIdx);
        // Créer un LOD pour un sous-mesh
        bool         AddSubMeshLOD(NkMeshId id,uint32 subIdx,
                                    const uint32* indices,uint32 count,
                                    float32 screenSizeThreshold);

        // ── Bind helper ────────────────────────────────────────────────────────
        void BindMesh   (NkICommandBuffer* cmd,NkMeshId id) const;
        void DrawSubMesh(NkICommandBuffer* cmd,NkMeshId id,uint32 subIdx,
                          uint32 instanceCount=1) const;
        void DrawSubMeshLOD(NkICommandBuffer* cmd,NkMeshId id,uint32 subIdx,
                             float32 screenSize,uint32 instanceCount=1) const;
        void DrawAllSubMeshes(NkICommandBuffer* cmd,NkMeshId id,
                               uint32 instanceCount=1) const;

        // ── Stats ──────────────────────────────────────────────────────────────
        uint32 GetMeshCount()     const;
        uint64 GetUsedVtxBytes()  const;
        uint64 GetUsedIdxBytes()  const;

    private:
        NkIDevice*         mDevice;
        NkMaterialSystem*  mMatSys;
        NkTextureLibrary*  mTexLib;
        uint64             mMaxVtxBytes, mMaxIdxBytes;
        NkMeshId           mNextId = 1;

        // Pool GPU
        NkBufferHandle     mVtxPool, mIdxPool;
        uint64             mVtxHead=0, mIdxHead=0;

        NkUnorderedMap<NkMeshId, NkMesh>  mMeshes;
        NkUnorderedMap<uint64,   NkMeshId> mPrimCache;

        NkMeshId AllocId();
        NkMeshId BuildAndUpload(NkMeshBuilder& mb, const NkString& name);
        void     BuildAABB(NkMesh& mesh, const NkMeshBuilder& builder);

        // Importeurs internes
        bool ParseOBJ (const NkString& path, const struct NkImportOptions&, NkMeshLoadResult&);
        bool ParseGLTF(const NkString& path, const struct NkImportOptions&, NkMeshLoadResult&, bool binary);
    };

    // =========================================================================
    // Options d'import
    // =========================================================================
    struct NkImportOptions {
        const NkVertexLayoutDef* targetLayout   = nullptr;
        bool   computeNormals    = true;
        bool   computeTangents   = true;
        bool   weldVertices      = true;
        bool   optimizeMesh      = true;
        bool   generateLODs      = false;
        uint32 lodCount          = 4;
        float32 lodRatio         = 0.5f;
        bool   importMaterials   = true;
        bool   importSkins       = true;
        bool   importAnimations  = true;
        bool   mergeSubmeshes    = false;   // fusionner les sous-meshes (1 seul draw call)
        float32 scale            = 1.f;
        bool   flipZ             = false;
        bool   flipUVY           = false;
        NkString textureBasePath;
    };

} // namespace nkentseu
