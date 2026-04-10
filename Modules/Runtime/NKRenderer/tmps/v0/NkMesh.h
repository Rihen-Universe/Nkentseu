#pragma once
// =============================================================================
// NkMesh.h  —  Mesh RefCounted + variantes Static / Dynamic / Editor
// =============================================================================
#include "NKRenderer/Core/NkRef.h"
#include "NKRenderer/Core/NkRenderTypes.h"
#include "NKRenderer/Core/NkVertex.h"
#include "NKRenderer/Material/NkMaterial.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
namespace render {

class NkRenderDevice;

// =============================================================================
// Sub-mesh (section avec son propre matériau)
// =============================================================================
struct NkSubMesh {
    NkString      name;
    uint32        indexOffset  = 0;
    uint32        indexCount   = 0;
    int32         vertexBase   = 0;
    NkMaterialPtr material;
    NkPrimitive   primitive    = NkPrimitive::NK_TRIANGLES;
    NkAABB        aabb;

    static NkSubMesh Make(const char* n, uint32 off, uint32 count,
                           NkMaterialPtr mat={}, int32 base=0) {
        NkSubMesh s; s.name=n; s.indexOffset=off; s.indexCount=count;
        s.material=mat; s.vertexBase=base; return s;
    }
};

// =============================================================================
// Descripteur mesh générique
// =============================================================================
struct NkMeshDesc {
    NkString       name;
    NkPrimitive    primitive    = NkPrimitive::NK_TRIANGLES;

    // Vertex data (raw)
    const void*    vertexData   = nullptr;
    uint32         vertexCount  = 0;
    uint32         vertexStride = sizeof(NkVertex3D);
    NkVertexLayout vertexLayout;

    // Index data (raw)
    const void*    indexData    = nullptr;
    uint32         indexCount   = 0;
    NkIndexFormat  indexFormat  = NkIndexFormat::NK_UINT32;

    // Sub-meshes
    NkVector<NkSubMesh> subMeshes;

    // AABB global (calculé automatiquement si computeAABB=true)
    NkAABB         aabb;

    // Options
    bool           computeNormals  = false;
    bool           computeTangents = false;
    bool           computeAABB     = true;
    const char*    debugName       = nullptr;

    // ── Helpers typés ───────────────────────────────────────────────────────
    template<typename V = NkVertex3D>
    static NkMeshDesc FromBuffers(const V* verts, uint32 numV,
                                   const uint32* idx=nullptr, uint32 numI=0,
                                   const char* n=nullptr) {
        NkMeshDesc d;
        d.name=n?n:""; d.vertexData=verts; d.vertexCount=numV;
        d.vertexStride=sizeof(V); d.vertexLayout=V::Layout();
        d.indexData=idx; d.indexCount=numI;
        d.indexFormat=NkIndexFormat::NK_UINT32; d.debugName=n;
        return d;
    }
    template<typename V = NkVertex3D>
    static NkMeshDesc FromBuffers16(const V* verts, uint32 numV,
                                     const uint16* idx, uint32 numI,
                                     const char* n=nullptr) {
        NkMeshDesc d = FromBuffers<V>(verts,numV,nullptr,0,n);
        d.indexData=idx; d.indexCount=numI;
        d.indexFormat=NkIndexFormat::NK_UINT16; return d;
    }
    NkMeshDesc& AddSubMesh(NkSubMesh s) { subMeshes.PushBack(s); return *this; }
};

// =============================================================================
// Info runtime d'un mesh
// =============================================================================
struct NkMeshInfo {
    NkString  name;
    uint32    vertexCount  = 0;
    uint32    indexCount   = 0;
    uint32    subMeshCount = 0;
    NkAABB    aabb;
    uint64    gpuBytes     = 0;
    bool      isDynamic    = false;
};

// =============================================================================
// NkMesh — ressource GPU RefCounted
// =============================================================================
class NkMesh : public NkRefCounted {
public:
    const NkMeshInfo& Info()         const { return mInfo; }
    const NkString&   Name()         const { return mInfo.name; }
    uint32            VertexCount()  const { return mInfo.vertexCount; }
    uint32            IndexCount()   const { return mInfo.indexCount; }
    uint32            SubMeshCount() const { return (uint32)mSubMeshes.Size(); }
    const NkSubMesh&  SubMesh(uint32 i) const { return mSubMeshes[i]; }
    const NkAABB&     AABB()         const { return mInfo.aabb; }
    bool              IsDynamic()    const { return mInfo.isDynamic; }
    bool              HasIndex()     const { return mInfo.indexCount > 0; }
    uint64            GPUBytes()     const { return mInfo.gpuBytes; }

    // Mise à jour d'un mesh dynamique
    // Ne change PAS le nombre de vertices alloués — respecter les limites initiales.
    bool UpdateVertices(const void* data, uint32 count);
    bool UpdateIndices (const void* data, uint32 count, NkIndexFormat fmt=NkIndexFormat::NK_UINT32);

    // Accès interne RHI
    uint64 RHIVBOId()   const { return mRHIVBOId; }
    uint64 RHIIBOId()   const { return mRHIIBOId; }
    NkIndexFormat IndexFmt() const { return mIndexFmt; }

protected:
    friend class NkRenderDevice;
    NkMesh() = default;
    virtual void Destroy() override;

    NkMeshInfo           mInfo;
    NkVector<NkSubMesh>  mSubMeshes;
    uint64               mRHIVBOId  = 0;
    uint64               mRHIIBOId  = 0;
    NkIndexFormat        mIndexFmt  = NkIndexFormat::NK_UINT32;
    uint32               mMaxVerts  = 0;
    uint32               mMaxIdx    = 0;
    NkRenderDevice*      mDevice    = nullptr;
};

using NkMeshPtr = NkRef<NkMesh>;

// =============================================================================
// NkModel — collection de meshes + hiérarchie de nœuds
// =============================================================================
struct NkModelNode {
    NkString          name;
    NkTransform3D     localTransform;
    int32             parentIdx = -1;   // -1 = racine
    NkVector<uint32>  meshIndices;      // Indices dans NkModel::mMeshes
    NkVector<uint32>  childIndices;
};

class NkModel : public NkRefCounted {
public:
    uint32           MeshCount()  const { return (uint32)mMeshes.Size(); }
    uint32           NodeCount()  const { return (uint32)mNodes.Size(); }
    NkMeshPtr        GetMesh(uint32 i)  const { return mMeshes[i]; }
    const NkModelNode& GetNode(uint32 i)const { return mNodes[i]; }
    const NkAABB&    AABB()            const { return mAABB; }
    const NkString&  Name()            const { return mName; }

protected:
    friend class NkRenderDevice;
    NkModel() = default;
    virtual void Destroy() override;

    NkString              mName;
    NkVector<NkMeshPtr>   mMeshes;
    NkVector<NkModelNode> mNodes;
    NkAABB                mAABB;
    NkRenderDevice*       mDevice = nullptr;
};

using NkModelPtr = NkRef<NkModel>;

// =============================================================================
// Descripteurs spécialisés
// =============================================================================

// Static mesh — données immuables
struct NkStaticMeshDesc : public NkMeshDesc {
    bool uploadNow = true;

    static NkStaticMeshDesc From(NkMeshDesc d) {
        NkStaticMeshDesc s; static_cast<NkMeshDesc&>(s)=d; return s;
    }
};

// Dynamic mesh — mise à jour chaque frame
struct NkDynamicMeshDesc {
    NkString       name;
    uint32         maxVertices  = 0;
    uint32         maxIndices   = 0;
    uint32         vertexStride = sizeof(NkVertex3D);
    NkVertexLayout vertexLayout;
    NkIndexFormat  indexFormat  = NkIndexFormat::NK_UINT32;
    NkPrimitive    primitive    = NkPrimitive::NK_TRIANGLES;
    const char*    debugName    = nullptr;

    template<typename V=NkVertex3D>
    static NkDynamicMeshDesc Create(uint32 maxV, uint32 maxI=0, const char* n=nullptr) {
        NkDynamicMeshDesc d;
        d.name=n?n:""; d.maxVertices=maxV; d.maxIndices=maxI;
        d.vertexStride=sizeof(V); d.vertexLayout=V::Layout();
        d.indexFormat = (maxV>65535)?NkIndexFormat::NK_UINT32:NkIndexFormat::NK_UINT16;
        d.debugName=n; return d;
    }
};

// =============================================================================
// NkMeshPrimitives — géométrie procédurale
// =============================================================================
class NkMeshPrimitives {
public:
    // ── 3D ──────────────────────────────────────────────────────────────────
    static NkMeshDesc Cube    (float32 size=1.f, const char* n="Cube");
    static NkMeshDesc Sphere  (float32 r=.5f, uint32 stacks=24, uint32 slices=32, const char* n="Sphere");
    static NkMeshDesc Plane   (float32 sx=1.f, float32 sz=1.f, uint32 subdX=1, uint32 subdZ=1, const char* n="Plane");
    static NkMeshDesc Capsule (float32 r=.5f, float32 h=1.f, uint32 seg=16, const char* n="Capsule");
    static NkMeshDesc Cylinder(float32 r=.5f, float32 h=1.f, uint32 seg=16, const char* n="Cylinder");
    static NkMeshDesc Cone    (float32 r=.5f, float32 h=1.f, uint32 seg=16, const char* n="Cone");
    static NkMeshDesc Torus   (float32 R=1.f, float32 r=.25f, uint32 segs=32, uint32 sides=16, const char* n="Torus");
    static NkMeshDesc FullscreenQuad(const char* n="FSQuad");
    static NkMeshDesc SkyboxCube    (const char* n="Skybox");
    static NkMeshDesc Arrow         (float32 len=1.f, const char* n="Arrow");
    static NkMeshDesc Grid          (uint32 cellsX=10, uint32 cellsZ=10, float32 cellSz=1.f, const char* n="Grid");
    static NkMeshDesc Icosphere     (float32 r=.5f, uint32 subdivisions=3, const char* n="Icosphere");

    // ── 2D (z=0) ────────────────────────────────────────────────────────────
    static NkMeshDesc Quad2D  (float32 w=1.f, float32 h=1.f, const char* n="Quad2D");
    static NkMeshDesc Circle2D(float32 r=.5f, uint32 seg=32, const char* n="Circle2D");
};

// =============================================================================
// Implémentations inline des primitives
// =============================================================================
inline NkMeshDesc NkMeshPrimitives::FullscreenQuad(const char* n) {
    static const NkVertex3D verts[4] = {
        {-1,-1, 0,  0,0,-1, 1,0,0,  0,0},
        { 1,-1, 0,  0,0,-1, 1,0,0,  1,0},
        { 1, 1, 0,  0,0,-1, 1,0,0,  1,1},
        {-1, 1, 0,  0,0,-1, 1,0,0,  0,1},
    };
    static const uint32 idx[6] = {0,1,2, 0,2,3};
    return NkMeshDesc::FromBuffers(verts,4,idx,6,n);
}

inline NkMeshDesc NkMeshPrimitives::Cube(float32 sz, const char* n) {
    const float h = sz * 0.5f;
    struct Face { float vx[4][3]; float uv[4][2]; float nx,ny,nz; };
    static const Face faces[6] = {
        {{{h,-h,h},{h,h,h},{-h,h,h},{-h,-h,h}}, {{1,0},{1,1},{0,1},{0,0}},  0, 0, 1},
        {{{-h,-h,-h},{-h,h,-h},{h,h,-h},{h,-h,-h}},{{1,0},{1,1},{0,1},{0,0}},0, 0,-1},
        {{{-h,-h,-h},{h,-h,-h},{h,-h,h},{-h,-h,h}},{{0,1},{1,1},{1,0},{0,0}},0,-1, 0},
        {{{-h,h,h},{h,h,h},{h,h,-h},{-h,h,-h}},{{0,1},{1,1},{1,0},{0,0}},    0, 1, 0},
        {{{-h,-h,-h},{-h,-h,h},{-h,h,h},{-h,h,-h}},{{0,0},{1,0},{1,1},{0,1}},-1,0,0},
        {{{h,-h,h},{h,-h,-h},{h,h,-h},{h,h,h}},{{0,0},{1,0},{1,1},{0,1}},    1, 0, 0},
    };
    static const int quad[6]={0,1,2,0,2,3};
    NkVector<NkVertex3D> v; v.Reserve(36);
    NkVector<uint32>     i; i.Reserve(36);
    for(auto& f:faces) {
        uint32 base=(uint32)v.Size();
        for(int q=0;q<4;++q) {
            NkVertex3D vtx; vtx.px=f.vx[q][0]; vtx.py=f.vx[q][1]; vtx.pz=f.vx[q][2];
            vtx.nx=f.nx; vtx.ny=f.ny; vtx.nz=f.nz;
            vtx.u=f.uv[q][0]; vtx.v=f.uv[q][1];
            v.PushBack(vtx);
        }
        for(int q:quad) i.PushBack(base+q);
    }
    return NkMeshDesc::FromBuffers(v.Data(),(uint32)v.Size(),i.Data(),(uint32)i.Size(),n);
}

inline NkMeshDesc NkMeshPrimitives::Sphere(float32 radius, uint32 stacks, uint32 slices, const char* n) {
    NkVector<NkVertex3D> v;
    NkVector<uint32>     i;
    const float pi = NK_PI_F;

    for(uint32 s=0;s<=stacks;++s) {
        const float phi = pi * s / stacks;
        for(uint32 sl=0;sl<=slices;++sl) {
            const float theta = 2.f*pi*sl/slices;
            const float x=NkSin(phi)*NkCos(theta);
            const float y=NkCos(phi);
            const float z=NkSin(phi)*NkSin(theta);
            NkVertex3D vtx;
            vtx.px=x*radius; vtx.py=y*radius; vtx.pz=z*radius;
            vtx.nx=x; vtx.ny=y; vtx.nz=z;
            vtx.u=(float)sl/slices; vtx.v=(float)s/stacks;
            v.PushBack(vtx);
        }
    }
    for(uint32 s=0;s<stacks;++s) {
        for(uint32 sl=0;sl<slices;++sl) {
            uint32 a=s*(slices+1)+sl;
            uint32 b=a+slices+1;
            i.PushBack(a); i.PushBack(b); i.PushBack(a+1);
            i.PushBack(b); i.PushBack(b+1); i.PushBack(a+1);
        }
    }
    return NkMeshDesc::FromBuffers(v.Data(),(uint32)v.Size(),i.Data(),(uint32)i.Size(),n);
}

inline NkMeshDesc NkMeshPrimitives::Plane(float32 sx, float32 sz,
                                            uint32 subdX, uint32 subdZ, const char* n) {
    NkVector<NkVertex3D> v;
    NkVector<uint32>     i;
    const float hx=sx*.5f, hz=sz*.5f;
    for(uint32 r=0;r<=subdZ;++r) {
        for(uint32 c=0;c<=subdX;++c) {
            float x = -hx + (float)c/subdX * sx;
            float z = -hz + (float)r/subdZ * sz;
            NkVertex3D vtx; vtx.px=x; vtx.py=0; vtx.pz=z;
            vtx.nx=0; vtx.ny=1; vtx.nz=0;
            vtx.u=(float)c/subdX; vtx.v=(float)r/subdZ;
            v.PushBack(vtx);
        }
    }
    for(uint32 r=0;r<subdZ;++r) {
        for(uint32 c=0;c<subdX;++c) {
            uint32 a=r*(subdX+1)+c;
            i.PushBack(a); i.PushBack(a+1); i.PushBack(a+subdX+1);
            i.PushBack(a+1); i.PushBack(a+subdX+2); i.PushBack(a+subdX+1);
        }
    }
    return NkMeshDesc::FromBuffers(v.Data(),(uint32)v.Size(),i.Data(),(uint32)i.Size(),n);
}

inline NkMeshDesc NkMeshPrimitives::Quad2D(float32 w, float32 h, const char* n) {
    const float hw=w*.5f, hh=h*.5f;
    static NkVertex3D verts[4];
    verts[0]={-hw,-hh,0, 0,0,-1, 1,0,0, 0,0};
    verts[1]={ hw,-hh,0, 0,0,-1, 1,0,0, 1,0};
    verts[2]={ hw, hh,0, 0,0,-1, 1,0,0, 1,1};
    verts[3]={-hw, hh,0, 0,0,-1, 1,0,0, 0,1};
    static const uint32 idx[6]={0,1,2,0,2,3};
    return NkMeshDesc::FromBuffers(verts,4,idx,6,n);
}

inline NkMeshDesc NkMeshPrimitives::Grid(uint32 cellsX, uint32 cellsZ, float32 cellSz, const char* n) {
    NkVector<NkVertexDebug> v;
    NkVector<uint32>        i;
    const float hx=cellsX*cellSz*.5f, hz=cellsZ*cellSz*.5f;
    NkColorF col={0.5f,0.5f,0.5f,1.f};
    // Lignes X
    for(uint32 r=0;r<=cellsZ;++r) {
        float z=-hz+(float)r*cellSz;
        uint32 base=(uint32)v.Size();
        v.PushBack({-hx,0.f,z,col}); v.PushBack({hx,0.f,z,col});
        i.PushBack(base); i.PushBack(base+1);
    }
    // Lignes Z
    for(uint32 c=0;c<=cellsX;++c) {
        float x=-hx+(float)c*cellSz;
        uint32 base=(uint32)v.Size();
        v.PushBack({x,0.f,-hz,col}); v.PushBack({x,0.f,hz,col});
        i.PushBack(base); i.PushBack(base+1);
    }
    NkMeshDesc d;
    d.name=n?n:"Grid"; d.primitive=NkPrimitive::NK_LINES;
    d.vertexData=v.Data(); d.vertexCount=(uint32)v.Size();
    d.vertexStride=sizeof(NkVertexDebug); d.vertexLayout=NkVertexDebug::Layout();
    d.indexData=i.Data(); d.indexCount=(uint32)i.Size();
    d.debugName=n;
    return d;
}

} // namespace render
} // namespace nkentseu