#pragma once
// =============================================================================
// NkMeshSystem.h  — NKRenderer v4.0  (Mesh/)
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/Associative/NkHashMap.h"

namespace nkentseu {
    namespace renderer {

        // =========================================================================
        // Vertex layout
        // =========================================================================
        enum class NkVertexAttr : uint8 {
            NK_POSITION=0, NK_NORMAL, NK_TANGENT, NK_BITANGENT,
            NK_COLOR, NK_TEXCOORD0, NK_TEXCOORD1,
            NK_JOINTS, NK_WEIGHTS, NK_CUSTOM,
        };
        enum class NkVertexFmt : uint8 {
            NK_F1, NK_F2, NK_F3, NK_F4,
            NK_U8x4_NORM,   // color
            NK_U16x4,       // joints
        };
        struct NkVertexElement { NkVertexAttr attr; NkVertexFmt fmt; uint32 location; };

        struct NkVertexLayout {
            NkVector<NkVertexElement> elements;
            uint32 stride = 0;

            static NkVertexLayout Default3D();
            static NkVertexLayout Default2D();
            static NkVertexLayout Skinned();
            static NkVertexLayout Debug();
            static NkVertexLayout Particle();

            NkString GenerateGLSL(uint32 version=460) const;
            NkString GenerateHLSL(bool dx12=false)    const;
            NkString GenerateMSL()                     const;
        };

        // =========================================================================
        // Sub-mesh + LOD
        // =========================================================================
        struct NkSubMeshLOD { uint32 firstIndex; uint32 indexCount; float32 screenSize; };
        struct NkSubMesh {
            NkString        name;
            uint32          firstIndex   = 0;
            uint32          indexCount   = 0;
            uint32          baseVertex   = 0;
            NkMatHandle     material;
            NkAABB          bounds;
            bool            visible      = true;
            bool            castShadow   = true;
            NkVector<NkSubMeshLOD> lods;
        };

        // =========================================================================
        // NkMeshDesc
        // =========================================================================
        struct NkMeshDesc {
            NkVertexLayout      layout;
            const void*         vertices    = nullptr;
            uint32              vertexCount = 0;
            const uint32*       indices     = nullptr;
            uint32              indexCount  = 0;
            NkVector<NkSubMesh> subMeshes;
            bool                dynamic     = false;
            // keepCPU : conserve une copie CPU des vertices/indices dans le mesh
            // (modele Blender : le CPU est l'autorite, le GPU n'est qu'un cache).
            // Necessaire UNIQUEMENT pour ce qui touche les vertices cote CPU :
            // Edit Mode / modification de topologie, skinning CPU (fallback),
            // mesh de collision/raycast, generation procedurale. PAS necessaire
            // pour le rendu, ni le skinning/morphing/rigging GPU (bones+poids en
            // buffers GPU, deformation dans le vertex shader -> le CPU ne touche
            // jamais les vertices). Defaut = false (moteur de jeu : evite de
            // dupliquer des centaines de Mo de geometrie statique en RAM).
            // NkMeshDesc::Simple() l'active (petits meshes procéduraux/éditables) ;
            // Import (gros assets) le laisse a false.
            bool                keepCPU     = false;
            NkAABB              bounds;
            NkString            debugName;

            static NkMeshDesc Simple(const NkVertexLayout& l, const void* v, uint32 vc, const uint32* i, uint32 ic);
        };

        // =========================================================================
        // NkMeshSystem
        // =========================================================================
        class NkMeshSystem {
            public:
                NkMeshSystem() = default;
                ~NkMeshSystem();

                bool Init(NkIDevice* device);
                void Shutdown();

                NkMeshHandle Create(const NkMeshDesc& desc);
                NkMeshHandle Import(const NkString& path, bool importMaterials=true);

                bool UpdateVertices(NkMeshHandle h, const void* data, uint32 count);
                bool UpdateIndices (NkMeshHandle h, const uint32* data, uint32 count);

                // Phase M.6 : update partiel du VBO. Utile pour Dynamic Paint
                // ou animation procedurale ou seul un sous-ensemble des vertices
                // change. firstVertex = index du 1er vertex a mettre a jour ;
                // count = nombre de vertices a uploader depuis `data`.
                // Le mesh doit avoir ete cree avec dynamic=true.
                bool UpdateVerticesRange(NkMeshHandle h, const void* data,
                                         uint32 firstVertex, uint32 count);

                void Release(NkMeshHandle& h);
                void ReleaseAll();

                uint32           GetSubMeshCount(NkMeshHandle h) const;
                const NkSubMesh& GetSubMesh(NkMeshHandle h, uint32 idx) const;
                bool             SetSubMeshMaterial(NkMeshHandle h, uint32 idx, NkMatHandle m);
                const NkAABB&    GetBounds(NkMeshHandle h) const;

                // Primitives built-in
                NkMeshHandle GetCube();
                NkMeshHandle GetSphere(uint32 stacks=32, uint32 slices=32);
                NkMeshHandle GetIcosphere(uint32 subdivisions=3);
                NkMeshHandle GetPlane(uint32 divX=1, uint32 divY=1);
                NkMeshHandle GetQuad();
                NkMeshHandle GetCylinder(uint32 segs=32);
                NkMeshHandle GetCone(uint32 segs=32);
                NkMeshHandle GetCapsule(uint32 segs=32);

                // Draw (appelé par NkRender3D)
                void BindMesh   (NkICommandBuffer* cmd, NkMeshHandle h);
                void DrawSubMesh(NkICommandBuffer* cmd, NkMeshHandle h,
                                uint32 subIdx, uint32 instances=1);
                void DrawAll    (NkICommandBuffer* cmd, NkMeshHandle h, uint32 instances=1);

                NkBufferHandle GetVBO(NkMeshHandle h) const;
                NkBufferHandle GetIBO(NkMeshHandle h) const;

                // ── Acces CPU (modele Blender : le mesh CPU est l'autorite) ─────────
                // Disponible uniquement si le mesh a ete cree avec keepCPU=true.
                // Retourne nullptr / 0 sinon. Ne fait AUCUN readback GPU.
                const void*   GetVertices   (NkMeshHandle h) const; // pointeur vers NkVertex3D[...]
                uint32        GetVertexCount (NkMeshHandle h) const;
                uint32        GetVertexStride(NkMeshHandle h) const;
                const uint32* GetIndices    (NkMeshHandle h) const;
                uint32        GetIndexCount  (NkMeshHandle h) const;
                bool          HasCPUData     (NkMeshHandle h) const;

            private:
                struct MeshEntry {
                    NkBufferHandle      vbo, ibo;
                    NkVertexLayout      layout;
                    uint32              vertexCount=0, indexCount=0;
                    NkVector<NkSubMesh> subMeshes;
                    NkAABB              bounds;
                    bool                dynamic=false;
                    NkString            debugName;
                    // Copie CPU optionnelle (keepCPU) : autorite mesh cote CPU.
                    NkVector<uint8>     cpuVerts;   // vertexCount * layout.stride octets
                    NkVector<uint32>    cpuIdx;     // indexCount indices
                };

                NkIDevice*                     mDevice = nullptr;
                NkHashMap<uint64, MeshEntry>   mMeshes;
                uint64                         mNextId = 1;

                // Cached primitives
                NkMeshHandle mCube, mSphere, mIcosphere, mPlane, mQuad, mCylinder, mCone, mCapsule;
                bool         mPrimitivesBuilt = false;

                void BuildPrimitives();
                NkMeshHandle Alloc(const MeshEntry& e);
                void BuildSphereData(uint32 stacks, uint32 slices, NkVector<NkVertex3D>& verts, NkVector<uint32>& idx);
                void BuildIcosphereData(uint32 subs, NkVector<NkVertex3D>& verts, NkVector<uint32>& idx);
        };

    } // namespace renderer
} // namespace nkentseu
