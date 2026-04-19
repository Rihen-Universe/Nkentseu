#pragma once
// =============================================================================
// NkMesh.h
// Système de mesh du NKRenderer.
//
// Types de mesh :
//   NkStaticMesh   — géométrie immuable (chargée une fois, GPU only)
//   NkDynamicMesh  — géométrie modifiable chaque frame (VBO uploadé dynamiquement)
//   NkEditableMesh — mesh éditable CPU (mode Blender : sommets, arêtes, faces)
//   NkProceduralMesh — génération procédurale (grilles, sphères, etc.)
//   NkSkinnedMesh  — mesh avec squelette pour l'animation
//
// Modes de rendu (comme Blender) :
//   Solid, Wireframe, Points (vertices), Mixed (Face+Edge+Vertex)
// =============================================================================
#include "NKRenderer/Core/NkRenderTypes.h"
#include "NKRenderer/Material/NkMaterial.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // Sous-mesh (section du mesh avec un matériau propre)
        // =============================================================================
        struct NkSubMesh {
            uint32      firstIndex   = 0;
            uint32      indexCount   = 0;
            uint32      firstVertex  = 0;
            uint32      vertexCount  = 0;
            uint32      materialIdx  = 0;
            NkString    name;
            // AABB local
            NkVec3f     aabbMin = {-0.5f,-0.5f,-0.5f};
            NkVec3f     aabbMax = { 0.5f, 0.5f, 0.5f};
        };

        // =============================================================================
        // Données de LOD (Level of Detail)
        // =============================================================================
        struct NkLODLevel {
            float           screenSizeThreshold = 1.f; // [0..1] taille écran en dessous de laquelle on utilise ce LOD
            NkVector<NkSubMesh> subMeshes;
            NkBufferHandle  vbo;
            NkBufferHandle  ibo;
            uint32          vertexCount = 0;
            uint32          indexCount  = 0;
        };

        // =============================================================================
        // AABB 3D
        // =============================================================================
        struct NkAABB {
            NkVec3f min = {-0.5f,-0.5f,-0.5f};
            NkVec3f max = { 0.5f, 0.5f, 0.5f};

            NkVec3f Center()  const { return {(min.x+max.x)*0.5f,(min.y+max.y)*0.5f,(min.z+max.z)*0.5f}; }
            NkVec3f Extents() const { return {(max.x-min.x)*0.5f,(max.y-min.y)*0.5f,(max.z-min.z)*0.5f}; }
            float   Radius()  const {
                NkVec3f e = Extents();
                return NkSqrt(e.x*e.x + e.y*e.y + e.z*e.z);
            }
            bool Contains(const NkVec3f& p) const {
                return p.x>=min.x&&p.x<=max.x && p.y>=min.y&&p.y<=max.y && p.z>=min.z&&p.z<=max.z;
            }
            NkAABB Merge(const NkAABB& o) const {
                return {
                    {NK_MIN(min.x,o.min.x), NK_MIN(min.y,o.min.y), NK_MIN(min.z,o.min.z)},
                    {NK_MAX(max.x,o.max.x), NK_MAX(max.y,o.max.y), NK_MAX(max.z,o.max.z)}
                };
            }
            void Expand(const NkVec3f& p) {
                min.x = NK_MIN(min.x,p.x); min.y = NK_MIN(min.y,p.y); min.z = NK_MIN(min.z,p.z);
                max.x = NK_MAX(max.x,p.x); max.y = NK_MAX(max.y,p.y); max.z = NK_MAX(max.z,p.z);
            }
        };

        // =============================================================================
        // NkMeshData — données CPU (utilisé pendant la construction)
        // =============================================================================
        struct NkMeshData {
            NkVector<NkVertex3D>  vertices;
            NkVector<uint32>      indices;
            NkVector<NkSubMesh>   subMeshes;
            NkVector<NkMaterial*> materials;
            NkMeshTopology        topology = NkMeshTopology::NK_TRIANGLES;
            NkAABB                aabb;

            // Recalculer les normales (si absentes ou incorrectes)
            void RecalculateNormals();
            // Recalculer les tangentes (TBN pour normal mapping)
            void RecalculateTangents();
            // Recalculer l'AABB
            void RecalculateAABB();
            // Optimiser (vertex deduplication, triangle cache)
            void Optimize();
            // Inverser les triangles (face culling)
            void FlipNormals();
            // Calculer les UVs sphériques
            void GenerateSphericalUV();
            // Calculer les UVs planaires
            void GeneratePlanarUV(int axis = 1);
        };

        // =============================================================================
        // NkStaticMesh — mesh GPU immuable
        // =============================================================================
        class NkStaticMesh {
            public:
                NkStaticMesh()  = default;
                ~NkStaticMesh() { Destroy(); }
                NkStaticMesh(const NkStaticMesh&) = delete;
                NkStaticMesh& operator=(const NkStaticMesh&) = delete;

                // ── Création ──────────────────────────────────────────────────────────────
                bool Create(NkIDevice* device, const NkMeshData& data);
                void Destroy();

                // ── Accesseurs ────────────────────────────────────────────────────────────
                bool              IsValid()       const { return mVBO.IsValid(); }
                NkMeshID          GetID()         const { return mID; }
                const NkString&   GetName()       const { return mName; }
                NkBufferHandle    GetVBO()        const { return mVBO; }
                NkBufferHandle    GetIBO()        const { return mIBO; }
                uint32            GetVertexCount()const { return mVertexCount; }
                uint32            GetIndexCount() const { return mIndexCount; }
                bool              HasIndices()    const { return mIBO.IsValid(); }
                const NkAABB&     GetAABB()       const { return mAABB; }
                const NkVector<NkSubMesh>&   GetSubMeshes()  const { return mSubMeshes; }
                const NkVector<NkMaterial*>& GetMaterials()  const { return mMaterials; }
                NkMaterial*       GetMaterial(uint32 idx) const;
                void              SetMaterial(uint32 idx, NkMaterial* mat);
                NkMeshTopology    GetTopology()   const { return mTopology; }
                const NkString&   GetSourcePath() const { return mSourcePath; }
                void              SetName(const char* n) { mName = n; }

                // LODs
                bool  HasLODs() const { return !mLODs.Empty(); }
                void  AddLOD(NkIDevice* device, const NkMeshData& data, float screenSizeThreshold);
                NkLODLevel* GetLOD(float screenSize);

                // Wireframe overlay buffer (edge indices)
                NkBufferHandle GetWireframeIBO() const { return mWireframeIBO; }
                bool           HasWireframe()    const { return mWireframeIBO.IsValid(); }
                void           BuildWireframe(NkIDevice* device);

                // Point cloud (vertex positions only)
                NkBufferHandle GetPointVBO() const { return mPointVBO; }
                bool           HasPointVBO() const { return mPointVBO.IsValid(); }
                void           BuildPointVBO(NkIDevice* device);

            private:
                NkIDevice*        mDevice      = nullptr;
                NkString          mName;
                NkString          mSourcePath;
                NkBufferHandle    mVBO;
                NkBufferHandle    mIBO;
                NkBufferHandle    mWireframeIBO; // edge pairs
                NkBufferHandle    mPointVBO;     // position seulement pour les points
                uint32            mVertexCount  = 0;
                uint32            mIndexCount   = 0;
                NkAABB            mAABB;
                NkMeshTopology    mTopology     = NkMeshTopology::NK_TRIANGLES;
                NkVector<NkSubMesh>   mSubMeshes;
                NkVector<NkMaterial*> mMaterials;
                NkVector<NkLODLevel>  mLODs;
                NkMeshID          mID;
                static uint64     sIDCounter;
        };

        // =============================================================================
        // NkDynamicMesh — mesh modifiable chaque frame
        // =============================================================================
        class NkDynamicMesh {
            public:
                NkDynamicMesh()  = default;
                ~NkDynamicMesh() { Destroy(); }
                NkDynamicMesh(const NkDynamicMesh&) = delete;
                NkDynamicMesh& operator=(const NkDynamicMesh&) = delete;

                bool Create(NkIDevice* device, uint32 maxVertices, uint32 maxIndices = 0);
                void Destroy();

                // Remplir les données (reset + upload)
                void Begin();
                void AddVertex(const NkVertex3D& v);
                void AddIndex(uint32 i);
                void AddTriangle(const NkVertex3D& a, const NkVertex3D& b, const NkVertex3D& c);
                void AddQuad(const NkVertex3D& a, const NkVertex3D& b,
                            const NkVertex3D& c, const NkVertex3D& d);
                void End(); // Upload vers GPU

                bool IsValid()          const { return mVBO.IsValid(); }
                NkBufferHandle GetVBO() const { return mVBO; }
                NkBufferHandle GetIBO() const { return mIBO; }
                uint32  GetVertexCount()const { return mCurrentVertex; }
                uint32  GetIndexCount() const { return mCurrentIndex; }
                bool    HasIndices()    const { return mIBO.IsValid() && mCurrentIndex > 0; }
                void    SetMaterial(NkMaterial* m) { mMaterial = m; }
                NkMaterial* GetMaterial() const    { return mMaterial; }
                NkMeshTopology GetTopology() const { return mTopology; }
                void SetTopology(NkMeshTopology t) { mTopology = t; }

            private:
                NkIDevice*     mDevice       = nullptr;
                NkBufferHandle mVBO;
                NkBufferHandle mIBO;
                uint32         mMaxVertices  = 0;
                uint32         mMaxIndices   = 0;
                uint32         mCurrentVertex= 0;
                uint32         mCurrentIndex = 0;
                NkVector<NkVertex3D> mCPUVerts;
                NkVector<uint32>     mCPUIndices;
                NkMaterial*    mMaterial     = nullptr;
                NkMeshTopology mTopology     = NkMeshTopology::NK_TRIANGLES;
        };

        // =============================================================================
        // NkEditableMesh — mesh éditable façon Blender (BMesh-like)
        // Vertices, Edges, Faces séparés pour édition et affichage sélectif
        // =============================================================================
        struct NkEditVertex {
            NkVec3f  position;
            NkVec3f  normal;
            NkVec2f  uv;
            NkColor4f color = {1,1,1,1};
            uint32   id;
            bool     selected = false;
            bool     hidden   = false;
        };

        struct NkEditEdge {
            uint32   v0, v1;    // indices dans mVertices
            uint32   id;
            bool     selected = false;
            bool     sharp    = false; // arête vive
            bool     seam     = false; // couture UV
            bool     hidden   = false;
        };

        struct NkEditFace {
            NkVector<uint32> vertexIndices;  // polygone (3+ sommets)
            uint32           materialIdx = 0;
            uint32           id;
            NkVec3f          normal;
            bool             selected = false;
            bool             hidden   = false;
        };

        class NkEditableMesh {
            public:
                NkEditableMesh()  = default;
                ~NkEditableMesh() = default;

                // ── Construction ──────────────────────────────────────────────────────────
                uint32 AddVertex(const NkVec3f& pos, const NkVec2f& uv = {0,0});
                uint32 AddEdge(uint32 v0, uint32 v1);
                uint32 AddFace(const NkVector<uint32>& verts, uint32 materialIdx = 0);
                uint32 AddTriangle(uint32 a, uint32 b, uint32 c, uint32 mat = 0);
                uint32 AddQuadFace(uint32 a, uint32 b, uint32 c, uint32 d, uint32 mat = 0);

                // ── Édition ───────────────────────────────────────────────────────────────
                void  MoveVertex(uint32 id, const NkVec3f& newPos);
                void  DeleteVertex(uint32 id);
                void  DeleteEdge(uint32 id);
                void  DeleteFace(uint32 id);

                // Sélection
                void  SelectVertex(uint32 id, bool s = true);
                void  SelectEdge(uint32 id,   bool s = true);
                void  SelectFace(uint32 id,   bool s = true);
                void  SelectAll(bool s = true);
                void  DeselectAll();

                // Extrusion (face → +profondeur)
                void  ExtrudeFace(uint32 faceId, const NkVec3f& offset);
                // Subdivision
                void  Subdivide(uint32 iterations = 1);
                // Inset (décaler une face vers l'intérieur)
                void  InsetFace(uint32 faceId, float amount);
                // Bevel (chanfrein)
                void  BevelEdge(uint32 edgeId, float amount, int segments = 1);
                // Merge
                void  MergeByDistance(float threshold = 0.001f);
                // Flip normals
                void  FlipNormals();
                void  RecalculateNormals();

                // ── Conversion vers NkMeshData ─────────────────────────────────────────
                NkMeshData ToMeshData() const;

                // ── Accesseurs ────────────────────────────────────────────────────────────
                const NkVector<NkEditVertex>& GetVertices() const { return mVertices; }
                const NkVector<NkEditEdge>&   GetEdges()    const { return mEdges; }
                const NkVector<NkEditFace>&   GetFaces()    const { return mFaces; }
                uint32 VertexCount() const { return (uint32)mVertices.Size(); }
                uint32 EdgeCount()   const { return (uint32)mEdges.Size(); }
                uint32 FaceCount()   const { return (uint32)mFaces.Size(); }
                bool IsEmpty()       const { return mVertices.Empty(); }

                // Statistiques sélection
                uint32 SelectedVertexCount() const;
                uint32 SelectedEdgeCount()   const;
                uint32 SelectedFaceCount()   const;

            private:
                NkVector<NkEditVertex> mVertices;
                NkVector<NkEditEdge>   mEdges;
                NkVector<NkEditFace>   mFaces;
                uint32                 mNextID = 0;

                uint32 FindOrAddEdge(uint32 v0, uint32 v1);
                void   TriangulateFace(const NkEditFace& f, NkVector<uint32>& outIndices) const;
        };

        // =============================================================================
        // NkSkinnedMesh — mesh avec squelette et skin weights
        // =============================================================================
        struct NkBone {
            NkString   name;
            int32      parent       = -1;    // index du bone parent (-1 = racine)
            NkMat4f    bindPose;             // pose de repos (model space)
            NkMat4f    inverseBindPose;      // inverse de la pose de repos
            NkVec3f    headPos;
            NkVec3f    tailPos;
            float      roll         = 0.f;
        };

        struct NkPose {
            NkString                name;
            NkVector<NkMat4f>       localTransforms; // un par bone, espace local
        };

        class NkSkinnedMesh : public NkStaticMesh {
            public:
                NkSkinnedMesh()  = default;
                ~NkSkinnedMesh() { DestroySkinnedData(); }

                bool CreateSkinned(NkIDevice* device, const NkMeshData& data,
                                const NkVector<NkBone>& skeleton);
                void DestroySkinnedData();

                // Calculer les matrices de skin pour une pose donnée
                void ComputeSkinMatrices(const NkPose& pose, NkVector<NkMat4f>& outMats) const;

                // Upload les matrices vers le GPU (SSBO binding 13)
                void UploadSkinMatrices(NkIDevice* device, const NkVector<NkMat4f>& mats);

                const NkVector<NkBone>&  GetSkeleton() const { return mSkeleton; }
                uint32                   BoneCount()   const { return (uint32)mSkeleton.Size(); }
                NkBufferHandle           GetSkinSSBO() const { return mSkinSSBO; }

            private:
                NkVector<NkBone>  mSkeleton;
                NkBufferHandle    mSkinSSBO;  // SSBO contenant les matrices skinning
        };

        // =============================================================================
        // NkProceduralMesh — générateurs de géométries standard
        // =============================================================================
        namespace NkProceduralMesh {
            NkMeshData Plane    (float width=1.f, float depth=1.f, uint32 res=1);
            NkMeshData Cube     (float size=1.f);
            NkMeshData Sphere   (float radius=0.5f, uint32 stacks=16, uint32 slices=32);
            NkMeshData Capsule  (float radius=0.5f, float height=1.f, uint32 segments=16);
            NkMeshData Cylinder (float radius=0.5f, float height=1.f, uint32 segments=32);
            NkMeshData Cone     (float radius=0.5f, float height=1.f, uint32 segments=32);
            NkMeshData Torus    (float major=0.5f,  float minor=0.2f,  uint32 major_seg=32, uint32 minor_seg=16);
            NkMeshData Arrow    (float length=1.f,  float radius=0.05f);
            NkMeshData Grid     (float width=10.f,  float depth=10.f, uint32 divisions=10);
            // Terrain (heightmap)
            NkMeshData Terrain  (const float* heightmap, uint32 w, uint32 d, float scale=1.f, float heightScale=1.f);
            // Billboard (quad toujours face caméra)
            NkMeshData Billboard(float width=1.f, float height=1.f);
            // Skybox
            NkMeshData Skybox   (float size=500.f);
        }

        // =============================================================================
        // NkMeshLibrary — cache de meshes
        // =============================================================================
        class NkMeshLibrary {
            public:
                static NkMeshLibrary& Get();

                void Init(NkIDevice* device);
                void Shutdown();

                NkStaticMesh* Load(const char* path); // via NkModelLoader
                NkStaticMesh* Find(const NkString& name) const;
                void          Register(NkStaticMesh* mesh);

                // Primitives built-in (générées à la demande et cachées)
                NkStaticMesh* GetCube();
                NkStaticMesh* GetSphere();
                NkStaticMesh* GetPlane();
                NkStaticMesh* GetCapsule();
                NkStaticMesh* GetCylinder();
                NkStaticMesh* GetCone();
                NkStaticMesh* GetTorus();
                NkStaticMesh* GetArrow();
                NkStaticMesh* GetSkybox();

            private:
                NkMeshLibrary() = default;
                NkIDevice*  mDevice = nullptr;
                NkUnorderedMap<NkString, NkStaticMesh*> mByPath;
                NkStaticMesh* mBuiltins[16] = {};
                enum {
                    NK_CUBE = 0,
                    NK_SPHERE,
                    NK_PLANE,
                    NK_CAPSULE,
                    NK_CYLINDER,
                    NK_CONE,
                    NK_TORUS,
                    NK_ARROW,
                    NK_SKYBOX
                };

                NkStaticMesh* CreateBuiltin(uint32 type);
        };

    } // namespace renderer
} // namespace nkentseu
