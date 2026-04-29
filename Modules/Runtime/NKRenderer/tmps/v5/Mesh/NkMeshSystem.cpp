// =============================================================================
// NkMeshSystem.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkMeshSystem.h"
#include <cmath>
#include <cstring>

#ifndef NK_PI
#define NK_PI 3.14159265358979323846f
#endif

namespace nkentseu {
namespace renderer {

    // ── Vertex Layouts ────────────────────────────────────────────────────────
    NkVertexLayout NkVertexLayout::Default3D() {
        NkVertexLayout l;
        l.elements.PushBack({NkVertexAttr::NK_POSITION,  NkVertexFmt::NK_F3, 0});
        l.elements.PushBack({NkVertexAttr::NK_NORMAL,    NkVertexFmt::NK_F3, 1});
        l.elements.PushBack({NkVertexAttr::NK_TANGENT,   NkVertexFmt::NK_F3, 2});
        l.elements.PushBack({NkVertexAttr::NK_TEXCOORD0, NkVertexFmt::NK_F2, 3});
        l.elements.PushBack({NkVertexAttr::NK_TEXCOORD1, NkVertexFmt::NK_F2, 4});
        l.elements.PushBack({NkVertexAttr::NK_COLOR,     NkVertexFmt::NK_U8x4_NORM, 5});
        l.stride = sizeof(NkVertex3D);
        return l;
    }
    NkVertexLayout NkVertexLayout::Default2D() {
        NkVertexLayout l;
        l.elements.PushBack({NkVertexAttr::NK_POSITION,  NkVertexFmt::NK_F2, 0});
        l.elements.PushBack({NkVertexAttr::NK_TEXCOORD0, NkVertexFmt::NK_F2, 1});
        l.elements.PushBack({NkVertexAttr::NK_COLOR,     NkVertexFmt::NK_U8x4_NORM, 2});
        l.stride = sizeof(NkVertex2D);
        return l;
    }
    NkVertexLayout NkVertexLayout::Skinned() {
        NkVertexLayout l;
        l.elements.PushBack({NkVertexAttr::NK_POSITION,  NkVertexFmt::NK_F3, 0});
        l.elements.PushBack({NkVertexAttr::NK_NORMAL,    NkVertexFmt::NK_F3, 1});
        l.elements.PushBack({NkVertexAttr::NK_TANGENT,   NkVertexFmt::NK_F3, 2});
        l.elements.PushBack({NkVertexAttr::NK_TEXCOORD0, NkVertexFmt::NK_F2, 3});
        l.elements.PushBack({NkVertexAttr::NK_COLOR,     NkVertexFmt::NK_U8x4_NORM, 4});
        l.elements.PushBack({NkVertexAttr::NK_JOINTS,    NkVertexFmt::NK_U16x4, 5});
        l.elements.PushBack({NkVertexAttr::NK_WEIGHTS,   NkVertexFmt::NK_F4, 6});
        l.stride = sizeof(NkVertexSkinned);
        return l;
    }
    NkVertexLayout NkVertexLayout::Debug() {
        NkVertexLayout l;
        l.elements.PushBack({NkVertexAttr::NK_POSITION, NkVertexFmt::NK_F3, 0});
        l.elements.PushBack({NkVertexAttr::NK_COLOR,    NkVertexFmt::NK_U8x4_NORM, 1});
        l.stride = sizeof(NkVertexDebug);
        return l;
    }
    NkVertexLayout NkVertexLayout::Particle() {
        NkVertexLayout l;
        l.elements.PushBack({NkVertexAttr::NK_POSITION,  NkVertexFmt::NK_F3, 0});
        l.elements.PushBack({NkVertexAttr::NK_TEXCOORD0, NkVertexFmt::NK_F2, 1});
        l.elements.PushBack({NkVertexAttr::NK_COLOR,     NkVertexFmt::NK_U8x4_NORM, 2});
        l.stride = sizeof(NkVertexParticle);
        return l;
    }

    // ── MeshSystem ────────────────────────────────────────────────────────────
    NkMeshSystem::~NkMeshSystem() { Shutdown(); }

    bool NkMeshSystem::Init(NkIDevice* device) {
        mDevice = device;
        BuildPrimitives();
        return true;
    }

    void NkMeshSystem::Shutdown() { ReleaseAll(); }

    NkMeshHandle NkMeshSystem::Alloc(const MeshEntry& e) {
        NkMeshHandle h{mNextId++};
        mMeshes.Insert(h.id, e);
        return h;
    }

    NkMeshHandle NkMeshSystem::Create(const NkMeshDesc& desc) {
        MeshEntry e;
        e.layout      = desc.layout;
        e.vertexCount = desc.vertexCount;
        e.indexCount  = desc.indexCount;
        e.subMeshes   = desc.subMeshes;
        e.bounds      = desc.bounds;
        e.dynamic     = desc.dynamic;
        e.debugName   = desc.debugName;

        NkBufferDesc vbd;
        vbd.size   = desc.vertexCount * desc.layout.stride;
        vbd.usage  = desc.dynamic ? NkBufferUsage::NK_DYNAMIC : NkBufferUsage::NK_STATIC;
        vbd.type   = NkBufferType::NK_VERTEX;
        vbd.data   = desc.vertices;
        e.vbo = mDevice->CreateBuffer(vbd);

        if (desc.indices && desc.indexCount > 0) {
            NkBufferDesc ibd;
            ibd.size   = desc.indexCount * sizeof(uint32);
            ibd.usage  = desc.dynamic ? NkBufferUsage::NK_DYNAMIC : NkBufferUsage::NK_STATIC;
            ibd.type   = NkBufferType::NK_INDEX;
            ibd.data   = desc.indices;
            e.ibo = mDevice->CreateBuffer(ibd);
        }

        // Si pas de sous-meshes, créer un sous-mesh qui couvre tout
        if (e.subMeshes.Empty()) {
            NkSubMesh sm;
            sm.name       = desc.debugName;
            sm.firstIndex = 0;
            sm.indexCount = desc.indexCount;
            sm.bounds     = desc.bounds;
            e.subMeshes.PushBack(sm);
        }
        return Alloc(e);
    }

    NkMeshHandle NkMeshSystem::Import(const NkString& path, bool importMat) {
        // TODO: GLTF2 via micro_gltf / assimp
        // Retourner un cube placeholder pour l'instant
        return GetCube();
    }

    bool NkMeshSystem::UpdateVertices(NkMeshHandle h, const void* data, uint32 count) {
        auto* e = mMeshes.Find(h.id);
        if (!e || !e->dynamic) return false;
        return mDevice->UpdateBuffer(e->vbo, data, count * e->layout.stride);
    }

    bool NkMeshSystem::UpdateIndices(NkMeshHandle h, const uint32* data, uint32 count) {
        auto* e = mMeshes.Find(h.id);
        if (!e || !e->dynamic) return false;
        return mDevice->UpdateBuffer(e->ibo, data, count * sizeof(uint32));
    }

    void NkMeshSystem::Release(NkMeshHandle& h) {
        auto* e = mMeshes.Find(h.id);
        if (!e) return;
        if (e->vbo.IsValid()) mDevice->DestroyBuffer(e->vbo);
        if (e->ibo.IsValid()) mDevice->DestroyBuffer(e->ibo);
        mMeshes.Remove(h.id);
        h = NkMeshHandle::Null();
    }

    void NkMeshSystem::ReleaseAll() {
        for (auto& [id, e] : mMeshes) {
            if (e.vbo.IsValid()) mDevice->DestroyBuffer(e.vbo);
            if (e.ibo.IsValid()) mDevice->DestroyBuffer(e.ibo);
        }
        mMeshes.Clear();
    }

    uint32 NkMeshSystem::GetSubMeshCount(NkMeshHandle h) const {
        auto* e = mMeshes.Find(h.id);
        return e ? (uint32)e->subMeshes.Size() : 0;
    }
    const NkSubMesh& NkMeshSystem::GetSubMesh(NkMeshHandle h, uint32 idx) const {
        static NkSubMesh sNull;
        auto* e = mMeshes.Find(h.id);
        if (!e || idx >= (uint32)e->subMeshes.Size()) return sNull;
        return e->subMeshes[idx];
    }
    bool NkMeshSystem::SetSubMeshMaterial(NkMeshHandle h, uint32 idx, NkMatHandle m) {
        auto* e = mMeshes.Find(h.id);
        if (!e || idx >= (uint32)e->subMeshes.Size()) return false;
        e->subMeshes[idx].material = m;
        return true;
    }
    const NkAABB& NkMeshSystem::GetBounds(NkMeshHandle h) const {
        static NkAABB sNull;
        auto* e = mMeshes.Find(h.id);
        return e ? e->bounds : sNull;
    }
    NkBufferHandle NkMeshSystem::GetVBO(NkMeshHandle h) const {
        auto* e = mMeshes.Find(h.id); return e ? e->vbo : NkBufferHandle{};
    }
    NkBufferHandle NkMeshSystem::GetIBO(NkMeshHandle h) const {
        auto* e = mMeshes.Find(h.id); return e ? e->ibo : NkBufferHandle{};
    }

    // ── Draw ──────────────────────────────────────────────────────────────────
    void NkMeshSystem::BindMesh(NkICommandBuffer* cmd, NkMeshHandle h) {
        auto* e = mMeshes.Find(h.id);
        if (!e) return;
        cmd->BindVertexBuffer(e->vbo, e->layout.stride);
        if (e->ibo.IsValid()) cmd->BindIndexBuffer(e->ibo, NkIndexType::NK_UINT32);
    }

    void NkMeshSystem::DrawSubMesh(NkICommandBuffer* cmd, NkMeshHandle h,
                                     uint32 subIdx, uint32 instances) {
        auto* e = mMeshes.Find(h.id);
        if (!e || subIdx >= (uint32)e->subMeshes.Size()) return;
        const auto& sm = e->subMeshes[subIdx];
        if (!sm.visible) return;
        if (e->ibo.IsValid())
            cmd->DrawIndexed(sm.indexCount, instances, sm.firstIndex, sm.baseVertex, 0);
        else
            cmd->Draw(sm.indexCount, instances, sm.firstIndex, 0);
    }

    void NkMeshSystem::DrawAll(NkICommandBuffer* cmd, NkMeshHandle h, uint32 instances) {
        auto* e = mMeshes.Find(h.id);
        if (!e) return;
        for (uint32 i = 0; i < (uint32)e->subMeshes.Size(); i++)
            DrawSubMesh(cmd, h, i, instances);
    }

    // ── Primitives ────────────────────────────────────────────────────────────
    inline uint32 PackColor(uint8 r, uint8 g, uint8 b, uint8 a=255) {
        return ((uint32)a<<24)|((uint32)b<<16)|((uint32)g<<8)|r;
    }

    void NkMeshSystem::BuildPrimitives() {
        if (mPrimitivesBuilt) return;

        // ── CUBE ──────────────────────────────────────────────────────────────
        {
            NkVertex3D verts[24]; uint32 idx[36];
            // 6 faces × 4 verts, normals, uvs
            const NkVec3f n[6] = {{0,0,1},{0,0,-1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}};
            const NkVec3f t[6] = {{1,0,0},{-1,0,0},{0,0,-1},{0,0,1},{1,0,0},{1,0,0}};
            const NkVec3f p[8] = {
                {-0.5f,-0.5f, 0.5f},{0.5f,-0.5f, 0.5f},{0.5f,0.5f, 0.5f},{-0.5f,0.5f, 0.5f},
                {-0.5f,-0.5f,-0.5f},{0.5f,-0.5f,-0.5f},{0.5f,0.5f,-0.5f},{-0.5f,0.5f,-0.5f}
            };
            const int fi[6][4] = {{1,0,3,2},{4,5,6,7},{0,4,7,3},{5,1,2,6},{3,7,6,2},{0,1,5,4}};
            NkVec2f uvs[4] = {{0,1},{1,1},{1,0},{0,0}};
            uint32 white = PackColor(255,255,255);
            for (int f=0;f<6;f++) {
                for (int v=0;v<4;v++) {
                    auto& vt = verts[f*4+v];
                    vt.pos=p[fi[f][v]]; vt.normal=n[f]; vt.tangent=t[f];
                    vt.uv=uvs[v]; vt.uv2={0,0}; vt.color=white;
                }
                uint32 b=f*4;
                idx[f*6+0]=b; idx[f*6+1]=b+1; idx[f*6+2]=b+2;
                idx[f*6+3]=b; idx[f*6+4]=b+2; idx[f*6+5]=b+3;
            }
            NkMeshDesc d = NkMeshDesc::Simple(NkVertexLayout::Default3D(), verts,24,idx,36);
            d.debugName="Cube"; d.bounds=NkAABB::Unit();
            mCube = Create(d);
        }

        // ── PLANE ─────────────────────────────────────────────────────────────
        {
            NkVertex3D verts[4]; uint32 idx[6];
            verts[0]={{-0.5f,0,0.5f},{0,1,0},{1,0,0},{0,1},{0,1},PackColor(255,255,255)};
            verts[1]={{ 0.5f,0,0.5f},{0,1,0},{1,0,0},{1,1},{1,1},PackColor(255,255,255)};
            verts[2]={{ 0.5f,0,-0.5f},{0,1,0},{1,0,0},{1,0},{1,0},PackColor(255,255,255)};
            verts[3]={{-0.5f,0,-0.5f},{0,1,0},{1,0,0},{0,0},{0,0},PackColor(255,255,255)};
            idx[0]=0;idx[1]=2;idx[2]=1; idx[3]=0;idx[4]=3;idx[5]=2;
            NkMeshDesc d=NkMeshDesc::Simple(NkVertexLayout::Default3D(),verts,4,idx,6);
            d.debugName="Plane";
            d.bounds={{-0.5f,0,-0.5f},{0.5f,0,0.5f}};
            mPlane=Create(d);
        }

        // ── QUAD (NDC, pour fullscreen post-process) ──────────────────────────
        {
            NkVertex3D verts[4]; uint32 idx[6];
            verts[0]={{-1,-1,0},{0,0,1},{1,0,0},{0,0},{0,0},PackColor(255,255,255)};
            verts[1]={{ 1,-1,0},{0,0,1},{1,0,0},{1,0},{1,0},PackColor(255,255,255)};
            verts[2]={{ 1, 1,0},{0,0,1},{1,0,0},{1,1},{1,1},PackColor(255,255,255)};
            verts[3]={{-1, 1,0},{0,0,1},{1,0,0},{0,1},{0,1},PackColor(255,255,255)};
            idx[0]=0;idx[1]=1;idx[2]=2; idx[3]=0;idx[4]=2;idx[5]=3;
            NkMeshDesc d=NkMeshDesc::Simple(NkVertexLayout::Default3D(),verts,4,idx,6);
            d.debugName="NDCQuad";
            mQuad=Create(d);
        }

        // ── SPHERE ────────────────────────────────────────────────────────────
        {
            NkVector<NkVertex3D> v; NkVector<uint32> i;
            BuildSphereData(32,32,v,i);
            NkMeshDesc d=NkMeshDesc::Simple(NkVertexLayout::Default3D(),
                                             v.Data(),(uint32)v.Size(),
                                             i.Data(),(uint32)i.Size());
            d.debugName="Sphere"; d.bounds=NkAABB::Unit();
            mSphere=Create(d);
        }

        // ── ICOSPHERE ─────────────────────────────────────────────────────────
        {
            NkVector<NkVertex3D> v; NkVector<uint32> i;
            BuildIcosphereData(3,v,i);
            NkMeshDesc d=NkMeshDesc::Simple(NkVertexLayout::Default3D(),
                                             v.Data(),(uint32)v.Size(),
                                             i.Data(),(uint32)i.Size());
            d.debugName="Icosphere"; d.bounds=NkAABB::Unit();
            mIcosphere=Create(d);
        }

        // ── CYLINDER ─────────────────────────────────────────────────────────
        {
            const uint32 segs=32;
            NkVector<NkVertex3D> v; NkVector<uint32> idx;
            uint32 white=PackColor(255,255,255);
            for(uint32 s=0;s<=segs;s++){
                float32 a=2.f*NK_PI*s/segs;
                float32 c=cosf(a),sn=sinf(a);
                NkVertex3D top,bot;
                top.pos={c*0.5f,0.5f,sn*0.5f}; top.normal={c,0,sn};
                top.tangent={-sn,0,c}; top.uv={(float32)s/segs,0}; top.color=white;
                bot.pos={c*0.5f,-0.5f,sn*0.5f}; bot.normal={c,0,sn};
                bot.tangent={-sn,0,c}; bot.uv={(float32)s/segs,1}; bot.color=white;
                v.PushBack(top); v.PushBack(bot);
            }
            for(uint32 s=0;s<segs;s++){
                uint32 b=s*2;
                idx.PushBack(b);idx.PushBack(b+1);idx.PushBack(b+2);
                idx.PushBack(b+1);idx.PushBack(b+3);idx.PushBack(b+2);
            }
            NkMeshDesc d=NkMeshDesc::Simple(NkVertexLayout::Default3D(),
                                             v.Data(),(uint32)v.Size(),
                                             idx.Data(),(uint32)idx.Size());
            d.debugName="Cylinder";
            d.bounds={{-0.5f,-0.5f,-0.5f},{0.5f,0.5f,0.5f}};
            mCylinder=Create(d);
        }

        // ── CONE ─────────────────────────────────────────────────────────────
        {
            const uint32 segs=32;
            NkVector<NkVertex3D> v; NkVector<uint32> idx;
            uint32 white=PackColor(255,255,255);
            NkVertex3D tip; tip.pos={0,0.5f,0}; tip.normal={0,1,0}; tip.color=white;
            v.PushBack(tip);
            for(uint32 s=0;s<=segs;s++){
                float32 a=2*NK_PI*s/segs;
                NkVertex3D b; b.pos={cosf(a)*0.5f,-0.5f,sinf(a)*0.5f};
                b.normal={cosf(a),0.5f,sinf(a)}; b.color=white;
                b.uv={(float32)s/segs,1.f};
                v.PushBack(b);
            }
            for(uint32 s=0;s<segs;s++){
                idx.PushBack(0); idx.PushBack(s+1); idx.PushBack(s+2);
            }
            NkMeshDesc d=NkMeshDesc::Simple(NkVertexLayout::Default3D(),
                                             v.Data(),(uint32)v.Size(),
                                             idx.Data(),(uint32)idx.Size());
            d.debugName="Cone";
            d.bounds={{-0.5f,-0.5f,-0.5f},{0.5f,0.5f,0.5f}};
            mCone=Create(d);
        }

        // ── CAPSULE = CYLINDRE + 2 demi-sphères ───────────────────────────────
        // (simplifié — utilise un cylindre pour l'instant)
        mCapsule = mCylinder;

        mPrimitivesBuilt = true;
    }

    void NkMeshSystem::BuildSphereData(uint32 stacks, uint32 slices,
                                        NkVector<NkVertex3D>& v, NkVector<uint32>& idx) {
        uint32 white=PackColor(255,255,255);
        for(uint32 i=0;i<=stacks;i++){
            float32 phi=NK_PI*i/stacks;
            for(uint32 j=0;j<=slices;j++){
                float32 theta=2*NK_PI*j/slices;
                float32 x=sinf(phi)*cosf(theta);
                float32 y=cosf(phi);
                float32 z=sinf(phi)*sinf(theta);
                NkVertex3D vt;
                vt.pos={x*0.5f,y*0.5f,z*0.5f};
                vt.normal={x,y,z};
                vt.tangent={-sinf(theta),0,cosf(theta)};
                vt.uv={(float32)j/slices,1.f-(float32)i/stacks};
                vt.color=white;
                v.PushBack(vt);
            }
        }
        for(uint32 i=0;i<stacks;i++)
            for(uint32 j=0;j<slices;j++){
                uint32 b=i*(slices+1)+j;
                idx.PushBack(b);idx.PushBack(b+slices+1);idx.PushBack(b+1);
                idx.PushBack(b+1);idx.PushBack(b+slices+1);idx.PushBack(b+slices+2);
            }
    }

    void NkMeshSystem::BuildIcosphereData(uint32 subs,
                                           NkVector<NkVertex3D>& v, NkVector<uint32>& idx) {
        // Icosahedron base
        float32 phi=(1.f+sqrtf(5.f))*0.5f;
        float32 d=1.f/sqrtf(1.f+phi*phi);
        float32 p=phi*d;
        NkVec3f base[12]={
            {-d,p,0},{d,p,0},{-d,-p,0},{d,-p,0},
            {0,-d,p},{0,d,p},{0,-d,-p},{0,d,-p},
            {p,0,-d},{p,0,d},{-p,0,-d},{-p,0,d}
        };
        uint32 baseFaces[20][3]={
            {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
            {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
            {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},
            {4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}
        };
        uint32 white=PackColor(255,255,255);
        for(auto& p3:base){
            float32 l=sqrtf(p3.x*p3.x+p3.y*p3.y+p3.z*p3.z);
            NkVertex3D vt;
            vt.pos={p3.x/l*0.5f,p3.y/l*0.5f,p3.z/l*0.5f};
            vt.normal={p3.x/l,p3.y/l,p3.z/l};
            vt.uv={(atan2f(p3.z,p3.x)/(2*NK_PI)+0.5f),
                   (asinf(p3.y/l)/NK_PI+0.5f)};
            vt.color=white;
            v.PushBack(vt);
        }
        for(auto& f:baseFaces){
            idx.PushBack(f[0]);idx.PushBack(f[1]);idx.PushBack(f[2]);
        }
        // TODO: subdivisions
        (void)subs;
    }

    NkMeshDesc NkMeshDesc::Simple(const NkVertexLayout& l,
                                    const void* verts, uint32 vc,
                                    const uint32* inds,  uint32 ic) {
        NkMeshDesc d;
        d.layout=l; d.vertices=verts; d.vertexCount=vc;
        d.indices=inds; d.indexCount=ic;
        return d;
    }

    // ── Getters primitives ────────────────────────────────────────────────────
    NkMeshHandle NkMeshSystem::GetCube()                   { BuildPrimitives(); return mCube; }
    NkMeshHandle NkMeshSystem::GetSphere(uint32,uint32)    { BuildPrimitives(); return mSphere; }
    NkMeshHandle NkMeshSystem::GetIcosphere(uint32)        { BuildPrimitives(); return mIcosphere; }
    NkMeshHandle NkMeshSystem::GetPlane(uint32,uint32)     { BuildPrimitives(); return mPlane; }
    NkMeshHandle NkMeshSystem::GetQuad()                   { BuildPrimitives(); return mQuad; }
    NkMeshHandle NkMeshSystem::GetCylinder(uint32)         { BuildPrimitives(); return mCylinder; }
    NkMeshHandle NkMeshSystem::GetCone(uint32)             { BuildPrimitives(); return mCone; }
    NkMeshHandle NkMeshSystem::GetCapsule(uint32)          { BuildPrimitives(); return mCapsule; }

} // namespace renderer
} // namespace nkentseu
