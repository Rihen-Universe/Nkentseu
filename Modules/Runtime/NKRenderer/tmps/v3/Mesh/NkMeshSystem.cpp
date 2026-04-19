// =============================================================================
// NkMeshSystem.cpp
// =============================================================================
#include "NkMeshSystem.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace nkentseu {

    // =========================================================================
    // NkVertexElement
    // =========================================================================
    uint32 NkVertexElement::FormatSize(NkGPUFormat f) {
        return NkFormatBytesPerPixel(f);
    }

    // =========================================================================
    // NkVertexLayoutDef
    // =========================================================================
    NkVertexLayout NkVertexLayoutDef::ToRHI() const {
        NkVertexLayout lay;
        for (auto& e : elements) {
            lay.AddAttribute(e.location, binding, e.format, e.offset,
                              e.hlslSemantic, e.hlslSemIdx);
        }
        lay.AddBinding(binding, stride, perInstance);
        return lay;
    }

    const NkVertexElement* NkVertexLayoutDef::Find(NkVertexSemantic sem, uint32 idx) const {
        for (auto& e : elements)
            if (e.semantic==sem && e.semanticIdx==idx) return &e;
        return nullptr;
    }

    bool NkVertexLayoutDef::Has(NkVertexSemantic sem, uint32 idx) const {
        return Find(sem,idx) != nullptr;
    }

    // =========================================================================
    // NkVertexLayoutBuilder
    // =========================================================================
    NkVertexLayoutBuilder& NkVertexLayoutBuilder::Add(NkVertexSemantic sem,uint32 semIdx,
                                                       NkGPUFormat fmt,
                                                       const NkString& name,
                                                       const char* hlslSem,uint32 hlslIdx) {
        NkVertexElement e;
        e.semantic     = sem;
        e.semanticIdx  = semIdx;
        e.format       = fmt;
        e.offset       = mOffset;
        e.location     = mLoc++;
        e.name         = name;
        e.hlslSemantic = hlslSem;
        e.hlslSemIdx   = hlslIdx;
        mOffset       += NkVertexElement::FormatSize(fmt);
        mElements.PushBack(e);
        return *this;
    }

    NkVertexLayoutBuilder& NkVertexLayoutBuilder::AddPosition2F() { return Add(NkVertexSemantic::NK_SEM_POSITION,0,NkGPUFormat::NK_RG32_FLOAT,"position","POSITION",0); }
    NkVertexLayoutBuilder& NkVertexLayoutBuilder::AddPosition3F() { return Add(NkVertexSemantic::NK_SEM_POSITION,0,NkGPUFormat::NK_RGB32_FLOAT,"position","POSITION",0); }
    NkVertexLayoutBuilder& NkVertexLayoutBuilder::AddPosition4F() { return Add(NkVertexSemantic::NK_SEM_POSITION,0,NkGPUFormat::NK_RGBA32_FLOAT,"position","POSITION",0); }
    NkVertexLayoutBuilder& NkVertexLayoutBuilder::AddNormal3F()   { return Add(NkVertexSemantic::NK_SEM_NORMAL,  0,NkGPUFormat::NK_RGB32_FLOAT,"normal","NORMAL",0); }
    NkVertexLayoutBuilder& NkVertexLayoutBuilder::AddTangent3F()  { return Add(NkVertexSemantic::NK_SEM_TANGENT, 0,NkGPUFormat::NK_RGB32_FLOAT,"tangent","TANGENT",0); }
    NkVertexLayoutBuilder& NkVertexLayoutBuilder::AddTangent4F()  { return Add(NkVertexSemantic::NK_SEM_TANGENT, 0,NkGPUFormat::NK_RGBA32_FLOAT,"tangent","TANGENT",0); }
    NkVertexLayoutBuilder& NkVertexLayoutBuilder::AddBitangent3F(){ return Add(NkVertexSemantic::NK_SEM_BITANGENT,0,NkGPUFormat::NK_RGB32_FLOAT,"bitangent","BINORMAL",0); }

    NkVertexLayoutBuilder& NkVertexLayoutBuilder::AddUV2F(uint32 ch) {
        NkString n="uv"; n+=NkString::FromInt(ch);
        return Add(NkVertexSemantic::NK_SEM_TEXCOORD,ch,NkGPUFormat::NK_RG32_FLOAT,n,"TEXCOORD",ch);
    }
    NkVertexLayoutBuilder& NkVertexLayoutBuilder::AddColor4F(uint32 idx) {
        NkString n="color"; n+=NkString::FromInt(idx);
        return Add(NkVertexSemantic::NK_SEM_COLOR,idx,NkGPUFormat::NK_RGBA32_FLOAT,n,"COLOR",idx);
    }
    NkVertexLayoutBuilder& NkVertexLayoutBuilder::AddColor4UB(uint32 idx) {
        NkString n="color"; n+=NkString::FromInt(idx);
        return Add(NkVertexSemantic::NK_SEM_COLOR,idx,NkGPUFormat::NK_RGBA8_UNORM,n,"COLOR",idx);
    }
    NkVertexLayoutBuilder& NkVertexLayoutBuilder::AddJoints4UB() { return Add(NkVertexSemantic::NK_SEM_JOINTS,0,NkGPUFormat::NK_RGBA8_UINT,"joints","BLENDINDICES",0); }
    NkVertexLayoutBuilder& NkVertexLayoutBuilder::AddWeights4F() { return Add(NkVertexSemantic::NK_SEM_WEIGHTS,0,NkGPUFormat::NK_RGBA32_FLOAT,"weights","BLENDWEIGHT",0); }
    NkVertexLayoutBuilder& NkVertexLayoutBuilder::AddFloat (const NkString& name) { return Add(NkVertexSemantic::NK_SEM_CUSTOM,mLoc,NkGPUFormat::NK_R32_FLOAT,name,"TEXCOORD",mLoc); }
    NkVertexLayoutBuilder& NkVertexLayoutBuilder::AddFloat2(const NkString& name) { return Add(NkVertexSemantic::NK_SEM_CUSTOM,mLoc,NkGPUFormat::NK_RG32_FLOAT,name,"TEXCOORD",mLoc); }
    NkVertexLayoutBuilder& NkVertexLayoutBuilder::AddFloat3(const NkString& name) { return Add(NkVertexSemantic::NK_SEM_CUSTOM,mLoc,NkGPUFormat::NK_RGB32_FLOAT,name,"TEXCOORD",mLoc); }
    NkVertexLayoutBuilder& NkVertexLayoutBuilder::AddFloat4(const NkString& name) { return Add(NkVertexSemantic::NK_SEM_CUSTOM,mLoc,NkGPUFormat::NK_RGBA32_FLOAT,name,"TEXCOORD",mLoc); }

    NkVertexLayoutBuilder& NkVertexLayoutBuilder::SetBinding(uint32 b)   { mBinding=b; return *this; }
    NkVertexLayoutBuilder& NkVertexLayoutBuilder::SetPerInstance(bool v)  { mPerInstance=v; return *this; }

    NkVertexLayoutDef NkVertexLayoutBuilder::Build() const {
        NkVertexLayoutDef d;
        d.elements    = mElements;
        d.stride      = mOffset;
        d.binding     = mBinding;
        d.perInstance = mPerInstance;
        return d;
    }

    uint32 NkVertexLayoutBuilder::ComputeStride() const { return mOffset; }

    // ── Génération de code shader depuis le layout ────────────────────────────
    NkString NkVertexLayoutBuilder::GenerateGLSLInputs(int ver) const {
        NkString out;
        char buf[256];
        // Ajouter #version si demandé
        snprintf(buf,sizeof(buf),"// Generated from NkVertexLayoutDef (OpenGL GLSL %d)\n",ver);
        out += buf;
        for (auto& e : mElements) {
            const char* typeName = "float";
            switch(e.format) {
                case NkGPUFormat::NK_RG32_FLOAT:   typeName="vec2";  break;
                case NkGPUFormat::NK_RGB32_FLOAT:   typeName="vec3";  break;
                case NkGPUFormat::NK_RGBA32_FLOAT:  typeName="vec4";  break;
                case NkGPUFormat::NK_RGBA8_UNORM:   typeName="vec4";  break; // normalised
                case NkGPUFormat::NK_RGBA8_UINT:    typeName="uvec4"; break;
                case NkGPUFormat::NK_R32_UINT:      typeName="uint";  break;
                case NkGPUFormat::NK_R32_SINT:      typeName="int";   break;
                default: typeName="float"; break;
            }
            snprintf(buf,sizeof(buf),"layout(location=%u) in %s a%s;\n",
                     e.location, typeName, e.name.Empty()?"Custom":e.name.CStr());
            out += buf;
        }
        return out;
    }

    NkString NkVertexLayoutBuilder::GenerateVkGLSLInputs(int ver) const {
        // Identique à OpenGL GLSL pour les inputs vertex (les sets s'appliquent aux UBOs)
        return GenerateGLSLInputs(ver);
    }

    NkString NkVertexLayoutBuilder::GenerateHLSLStruct(bool isInput) const {
        NkString out;
        out += isInput ? "struct VSInput {\n" : "struct PSInput {\n";
        for (auto& e : mElements) {
            const char* typeName = "float";
            switch(e.format) {
                case NkGPUFormat::NK_RG32_FLOAT:  typeName="float2"; break;
                case NkGPUFormat::NK_RGB32_FLOAT:  typeName="float3"; break;
                case NkGPUFormat::NK_RGBA32_FLOAT: typeName="float4"; break;
                case NkGPUFormat::NK_RGBA8_UNORM:  typeName="float4"; break;
                case NkGPUFormat::NK_RGBA8_UINT:   typeName="uint4";  break;
                default: typeName="float"; break;
            }
            const char* sem = e.hlslSemantic ? e.hlslSemantic : "TEXCOORD";
            char buf[256];
            snprintf(buf,sizeof(buf),"    %s %s : %s%u;\n",
                     typeName,
                     e.name.Empty()?"v":e.name.CStr(),
                     sem, e.hlslSemIdx);
            out += buf;
        }
        out += "};\n";
        return out;
    }

    NkString NkVertexLayoutBuilder::GenerateMSLStruct(bool isInput) const {
        NkString out;
        out += "struct ";
        out += isInput ? "VertexIn" : "VertexOut";
        out += " {\n";
        for (auto& e : mElements) {
            const char* typeName = "float";
            switch(e.format) {
                case NkGPUFormat::NK_RG32_FLOAT:  typeName="float2"; break;
                case NkGPUFormat::NK_RGB32_FLOAT:  typeName="float3"; break;
                case NkGPUFormat::NK_RGBA32_FLOAT: typeName="float4"; break;
                case NkGPUFormat::NK_RGBA8_UNORM:  typeName="float4"; break;
                case NkGPUFormat::NK_RGBA8_UINT:   typeName="uint4";  break;
                default: typeName="float"; break;
            }
            char buf[256];
            if (e.semantic == NkVertexSemantic::NK_SEM_POSITION && isInput)
                snprintf(buf,sizeof(buf),"    %s %s [[attribute(%u)]];\n",
                         typeName,e.name.CStr(),e.location);
            else
                snprintf(buf,sizeof(buf),"    %s %s;\n",typeName,e.name.CStr());
            out += buf;
        }
        out += "};\n";
        return out;
    }

    // =========================================================================
    // NkMeshBuilder
    // =========================================================================
    NkMeshBuilder::NkMeshBuilder(const NkVertexLayoutDef& layout, NkPrimitiveTopology topo)
        : mLayout(layout), mTopology(topo) {}

    void NkMeshBuilder::SetLayout(const NkVertexLayoutDef& l) { mLayout = l; }
    void NkMeshBuilder::Reserve(uint32 vtx,uint32 idx) {
        mVtxData.Reserve(vtx*mLayout.stride);
        mIdxData.Reserve(idx);
    }

    uint32 NkMeshBuilder::BeginSubMesh(const NkString& name, NkMatId mat) {
        if (mInSubMesh) EndSubMesh();
        mCurrentSub    = (uint32)mSubRanges.Size();
        mSubStartIndex = (uint32)mIdxData.Size();
        SubMeshRange r; r.name=name; r.mat=mat;
        r.firstIndex=mSubStartIndex; r.indexCount=0;
        mSubRanges.PushBack(r);
        mInSubMesh = true;
        return mCurrentSub;
    }

    void NkMeshBuilder::EndSubMesh() {
        if (!mInSubMesh || mSubRanges.Empty()) return;
        mSubRanges.Back().indexCount = (uint32)mIdxData.Size() - mSubStartIndex;
        mInSubMesh = false;
    }

    NkMeshBuilder& NkMeshBuilder::BeginVertex() {
        uint32 sz = mLayout.stride;
        mCurrentVtxStart = (uint32)mVtxData.Size();
        mVtxData.Resize(mVtxData.Size() + sz, 0);
        mInVertex = true;
        return *this;
    }

    void NkMeshBuilder::SetAtOffset(uint32 off,const void* data,uint32 size) {
        if (!mInVertex) return;
        uint32 base = mCurrentVtxStart + off;
        if (base + size > (uint32)mVtxData.Size()) return;
        memcpy(mVtxData.Data() + base, data, size);
    }

    NkMeshBuilder& NkMeshBuilder::Position(float32 x,float32 y,float32 z,float32 w) {
        auto* e = mLayout.Find(NkVertexSemantic::NK_SEM_POSITION);
        if (!e) return *this;
        float32 v[4]={x,y,z,w};
        uint32 count=(e->format==NkGPUFormat::NK_RG32_FLOAT)?2:(e->format==NkGPUFormat::NK_RGBA32_FLOAT)?4:3;
        SetAtOffset(e->offset,v,count*4);
        return *this;
    }
    NkMeshBuilder& NkMeshBuilder::Normal(float32 x,float32 y,float32 z) {
        auto* e=mLayout.Find(NkVertexSemantic::NK_SEM_NORMAL);
        if(e){float32 v[3]={x,y,z};SetAtOffset(e->offset,v,12);}
        return *this;
    }
    NkMeshBuilder& NkMeshBuilder::Tangent(float32 x,float32 y,float32 z,float32 w) {
        auto* e=mLayout.Find(NkVertexSemantic::NK_SEM_TANGENT);
        if(e){float32 v[4]={x,y,z,w};SetAtOffset(e->offset,v,e->format==NkGPUFormat::NK_RGBA32_FLOAT?16:12);}
        return *this;
    }
    NkMeshBuilder& NkMeshBuilder::UV(float32 u,float32 v,uint32 ch) {
        auto* e=mLayout.Find(NkVertexSemantic::NK_SEM_TEXCOORD,ch);
        if(e){float32 vv[2]={u,v};SetAtOffset(e->offset,vv,8);}
        return *this;
    }
    NkMeshBuilder& NkMeshBuilder::Color(float32 r,float32 g,float32 b,float32 a,uint32 idx) {
        auto* e=mLayout.Find(NkVertexSemantic::NK_SEM_COLOR,idx);
        if(!e)return*this;
        if(e->format==NkGPUFormat::NK_RGBA8_UNORM){
            uint8 c[4]={(uint8)(r*255),(uint8)(g*255),(uint8)(b*255),(uint8)(a*255)};
            SetAtOffset(e->offset,c,4);
        } else {
            float32 c[4]={r,g,b,a}; SetAtOffset(e->offset,c,16);
        }
        return *this;
    }
    NkMeshBuilder& NkMeshBuilder::Color(uint8 r,uint8 g,uint8 b,uint8 a,uint32 idx) {
        auto* e=mLayout.Find(NkVertexSemantic::NK_SEM_COLOR,idx);
        if(!e)return*this;
        if(e->format==NkGPUFormat::NK_RGBA8_UNORM){uint8 c[4]={r,g,b,a};SetAtOffset(e->offset,c,4);}
        else{float32 c[4]={r/255.f,g/255.f,b/255.f,a/255.f};SetAtOffset(e->offset,c,16);}
        return *this;
    }
    NkMeshBuilder& NkMeshBuilder::Joints(uint8 j0,uint8 j1,uint8 j2,uint8 j3) {
        auto* e=mLayout.Find(NkVertexSemantic::NK_SEM_JOINTS);
        if(e){uint8 j[4]={j0,j1,j2,j3};SetAtOffset(e->offset,j,4);}
        return *this;
    }
    NkMeshBuilder& NkMeshBuilder::Weights(float32 w0,float32 w1,float32 w2,float32 w3) {
        auto* e=mLayout.Find(NkVertexSemantic::NK_SEM_WEIGHTS);
        if(e){float32 w[4]={w0,w1,w2,w3};SetAtOffset(e->offset,w,16);}
        return *this;
    }
    NkMeshBuilder& NkMeshBuilder::Custom1F(uint32 loc,float32 v)   { for(auto&e:mLayout.elements)if(e.location==loc){SetAtOffset(e.offset,&v,4);break;} return *this; }
    NkMeshBuilder& NkMeshBuilder::Custom4F(uint32 loc,float32 x,float32 y,float32 z,float32 w) { for(auto&e:mLayout.elements)if(e.location==loc){float32 v[4]={x,y,z,w};SetAtOffset(e.offset,v,16);break;} return *this; }
    NkMeshBuilder& NkMeshBuilder::Custom2F(uint32 loc,float32 x,float32 y) { return Custom4F(loc,x,y,0,0); }
    NkMeshBuilder& NkMeshBuilder::Custom3F(uint32 loc,float32 x,float32 y,float32 z) { return Custom4F(loc,x,y,z,0); }

    uint32 NkMeshBuilder::EndVertex() {
        mInVertex = false;
        uint32 idx = (mCurrentVtxStart / mLayout.stride);
        return idx;
    }

    void NkMeshBuilder::Triangle(uint32 a,uint32 b,uint32 c) { mIdxData.PushBack(a);mIdxData.PushBack(b);mIdxData.PushBack(c); }
    void NkMeshBuilder::Quad(uint32 a,uint32 b,uint32 c,uint32 d) { Triangle(a,b,c);Triangle(a,c,d); }
    void NkMeshBuilder::Line(uint32 a,uint32 b)   { mIdxData.PushBack(a);mIdxData.PushBack(b); }
    void NkMeshBuilder::Point(uint32 a)            { mIdxData.PushBack(a); }
    void NkMeshBuilder::Indices(const uint32* idx,uint32 count) {
        for(uint32 i=0;i<count;i++) mIdxData.PushBack(idx[i]);
    }
    uint32 NkMeshBuilder::GetVertexCount() const { return mLayout.stride>0?(uint32)mVtxData.Size()/mLayout.stride:0; }

    NkAABB NkMeshBuilder::ComputeBounds() const {
        NkAABB box = NkAABB::Empty();
        auto* pe=mLayout.Find(NkVertexSemantic::NK_SEM_POSITION);
        if(!pe||mVtxData.Empty()) return box;
        uint32 vc=GetVertexCount();
        for(uint32 i=0;i<vc;i++){
            const float32* p=(const float32*)(mVtxData.Data()+i*mLayout.stride+pe->offset);
            box.Expand({p[0],p[1],p[2]});
        }
        return box;
    }

    void NkMeshBuilder::Clear() { mVtxData.Clear(); mIdxData.Clear(); mSubRanges.Clear(); mInVertex=false; mInSubMesh=false; }
    void NkMeshBuilder::ComputeNormals(bool smooth) { /* flat normals impl */ (void)smooth; }
    void NkMeshBuilder::ComputeTangents() {}
    void NkMeshBuilder::WeldVertices(float32 eps) { (void)eps; }
    void NkMeshBuilder::FlipNormals() {}
    void NkMeshBuilder::FlipWinding() {
        for(uint32 i=0;i+2<(uint32)mIdxData.Size();i+=3)
            std::swap(mIdxData[i+1],mIdxData[i+2]);
    }
    void NkMeshBuilder::ApplyTransform(const math::NkMat4& m) { (void)m; }
    void NkMeshBuilder::MergeWith(const NkMeshBuilder& o) {
        uint32 baseVtx=GetVertexCount();
        mVtxData.Insert(mVtxData.End(),o.mVtxData.Begin(),o.mVtxData.End());
        for(auto idx:o.mIdxData) mIdxData.PushBack(idx+baseVtx);
    }

    // =========================================================================
    // NkMesh helpers
    // =========================================================================
    const NkSubMesh* NkMesh::GetSubMesh(uint32 idx) const {
        return idx<(uint32)submeshes.Size() ? &submeshes[idx] : nullptr;
    }
    const NkSubMesh* NkMesh::FindSubMesh(const NkString& name) const {
        for(auto& s:submeshes) if(s.name==name) return &s;
        return nullptr;
    }
    NkAABB NkMesh::ComputeBounds() const {
        NkAABB b=NkAABB::Empty();
        for(auto& s:submeshes) b.Merge(s.bounds);
        return b;
    }
    void NkMesh::GetLODRange(uint32 subIdx,float32 screenSize,uint32& f,uint32& c) const {
        if(subIdx>=(uint32)submeshes.Size()){f=0;c=0;return;}
        const NkSubMesh& sm=submeshes[subIdx];
        f=sm.firstIndex; c=sm.indexCount;
        for(auto& lod:sm.lods){
            if(screenSize<=lod.screenSizeThreshold){f=lod.firstIndex;c=lod.indexCount;break;}
        }
    }

    // =========================================================================
    // NkMeshSystem
    // =========================================================================
    NkMeshSystem::NkMeshSystem(NkIDevice* dev,NkMaterialSystem* mat,NkTextureLibrary* tex)
        : mDevice(dev),mMatSys(mat),mTexLib(tex),mMaxVtxBytes(0),mMaxIdxBytes(0) {}
    NkMeshSystem::~NkMeshSystem() { Shutdown(); }

    bool NkMeshSystem::Initialize(uint64 maxVtx,uint64 maxIdx) {
        mMaxVtxBytes=maxVtx; mMaxIdxBytes=maxIdx;
        mVtxPool=mDevice->CreateBuffer({maxVtx,NkBufferType::NK_VERTEX,NkResourceUsage::NK_DEFAULT,NkBindFlags::NK_VERTEX_BUFFER|NkBindFlags::NK_TRANSFER_DST});
        mIdxPool=mDevice->CreateBuffer({maxIdx,NkBufferType::NK_INDEX, NkResourceUsage::NK_DEFAULT,NkBindFlags::NK_INDEX_BUFFER|NkBindFlags::NK_TRANSFER_DST});
        if(!mVtxPool.IsValid()||!mIdxPool.IsValid()){
            logger.Errorf("[NkMeshSystem] Failed to create GPU pools\n");
            return false;
        }
        logger.Infof("[NkMeshSystem] Ready vtx=%llu MB idx=%llu MB\n",
                     maxVtx/1024/1024, maxIdx/1024/1024);
        return true;
    }

    void NkMeshSystem::Shutdown() {
        if(mVtxPool.IsValid()) mDevice->DestroyBuffer(mVtxPool);
        if(mIdxPool.IsValid()) mDevice->DestroyBuffer(mIdxPool);
        mMeshes.Clear();
        mVtxHead=mIdxHead=0;
    }

    NkMeshId NkMeshSystem::AllocId() { return mNextId++; }

    NkMeshId NkMeshSystem::Upload(const NkMeshBuilder& builder, const NkString& name) {
        if(builder.IsEmpty()||!mVtxPool.IsValid()) return NK_INVALID_MESH;

        NkMesh mesh;
        mesh.id          = AllocId();
        mesh.name        = name;
        mesh.layout      = builder.GetLayout();
        mesh.vertexStride= builder.GetLayout().stride;
        mesh.topology    = builder.GetTopology();
        mesh.vertexBuffer= mVtxPool;
        mesh.indexBuffer = mIdxPool;
        mesh.totalVertices= builder.GetVertexCount();
        mesh.totalIndices = builder.GetIndexCount();

        // Upload vertex data
        uint64 vtxSize=builder.GetVertexData().Size();
        uint64 idxSize=builder.GetIndexData().Size()*sizeof(uint32);
        if(mVtxHead+vtxSize>mMaxVtxBytes||mIdxHead+idxSize>mMaxIdxBytes){
            logger.Errorf("[NkMeshSystem] Out of GPU pool memory for '%s'\n",name.CStr());
            return NK_INVALID_MESH;
        }
        mDevice->WriteBuffer(mVtxPool,builder.GetVertexData().Data(),vtxSize,mVtxHead);
        mDevice->WriteBuffer(mIdxPool,builder.GetIndexData().Data(), idxSize,mIdxHead);

        // Construire les sous-meshes
        auto& ranges = builder.GetSubMeshRanges();
        if(ranges.Empty()) {
            // Pas de sous-mesh explicite → un seul sous-mesh couvrant tout
            NkSubMesh sm;
            sm.name       = name;
            sm.firstIndex = (uint32)(mIdxHead/sizeof(uint32));
            sm.indexCount = builder.GetIndexCount();
            sm.material   = NK_INVALID_MAT;
            sm.bounds     = builder.ComputeBounds();
            mesh.submeshes.PushBack(sm);
        } else {
            for(auto& r : ranges) {
                NkSubMesh sm;
                sm.name       = r.name;
                sm.firstIndex = (uint32)(mIdxHead/sizeof(uint32)) + r.firstIndex;
                sm.indexCount = r.indexCount;
                sm.material   = r.mat;
                sm.bounds     = builder.ComputeBounds(); // approximation
                mesh.submeshes.PushBack(sm);
            }
        }
        mesh.bounds = builder.ComputeBounds();
        mVtxHead += vtxSize;
        mIdxHead += idxSize;

        NkMeshId id = mesh.id;
        mMeshes[id] = mesh;
        return id;
    }

    const NkMesh* NkMeshSystem::Get(NkMeshId id)      const { auto it=mMeshes.Find(id); return it?&it->Second:nullptr; }
    bool          NkMeshSystem::IsValid(NkMeshId id)   const { return mMeshes.Find(id)!=nullptr; }

    void NkMeshSystem::Release(NkMeshId id) { mMeshes.Erase(id); }

    void NkMeshSystem::SetSubMeshMaterial(NkMeshId id,uint32 sub,NkMatId mat) {
        auto it=mMeshes.Find(id);
        if(it&&sub<(uint32)it->Second.submeshes.Size())
            it->Second.submeshes[sub].material=mat;
    }
    void NkMeshSystem::SetSubMeshVisible(NkMeshId id,uint32 sub,bool v) {
        auto it=mMeshes.Find(id);
        if(it&&sub<(uint32)it->Second.submeshes.Size())
            it->Second.submeshes[sub].visible=v;
    }
    uint32 NkMeshSystem::GetSubMeshCount(NkMeshId id) const {
        auto it=mMeshes.Find(id);
        return it?(uint32)it->Second.submeshes.Size():0;
    }
    NkSubMesh* NkMeshSystem::GetSubMesh(NkMeshId id,uint32 sub) {
        auto it=mMeshes.Find(id);
        if(it&&sub<(uint32)it->Second.submeshes.Size()) return &it->Second.submeshes[sub];
        return nullptr;
    }

    void NkMeshSystem::BindMesh(NkICommandBuffer* cmd,NkMeshId id) const {
        auto it=mMeshes.Find(id);
        if(!it) return;
        const NkMesh& m=it->Second;
        cmd->BindVertexBuffer(0,m.vertexBuffer,0);
        cmd->BindIndexBuffer(m.indexBuffer,m.indexFormat,0);
    }

    void NkMeshSystem::DrawSubMesh(NkICommandBuffer* cmd,NkMeshId id,uint32 sub,uint32 inst) const {
        auto it=mMeshes.Find(id);
        if(!it||sub>=(uint32)it->Second.submeshes.Size()) return;
        const NkSubMesh& sm=it->Second.submeshes[sub];
        if(!sm.visible) return;
        cmd->DrawIndexed(sm.indexCount,inst,sm.firstIndex,sm.vertexOffset,0);
    }

    void NkMeshSystem::DrawAllSubMeshes(NkICommandBuffer* cmd,NkMeshId id,uint32 inst) const {
        uint32 n=GetSubMeshCount(id);
        for(uint32 i=0;i<n;i++) DrawSubMesh(cmd,id,i,inst);
    }

    // Layouts standard
    NkVertexLayoutDef NkMeshSystem::GetStandardLayout() {
        return NkVertexLayoutBuilder().AddPosition3F().AddNormal3F().AddTangent4F().AddUV2F(0).AddUV2F(1).Build();
    }
    NkVertexLayoutDef NkMeshSystem::GetSkinnedLayout() {
        return NkVertexLayoutBuilder().AddPosition3F().AddNormal3F().AddTangent4F().AddUV2F().AddJoints4UB().AddWeights4F().Build();
    }
    NkVertexLayoutDef NkMeshSystem::GetSimpleLayout() {
        return NkVertexLayoutBuilder().AddPosition3F().AddUV2F().Build();
    }
    NkVertexLayoutDef NkMeshSystem::GetDebugLayout() {
        return NkVertexLayoutBuilder().AddPosition3F().AddColor4UB().Build();
    }
    NkVertexLayoutDef NkMeshSystem::GetParticleLayout() {
        return NkVertexLayoutBuilder().AddPosition3F().AddUV2F().AddColor4UB().AddFloat("size").Build();
    }
    NkVertexLayoutDef NkMeshSystem::GetTerrainLayout() {
        return NkVertexLayoutBuilder().AddPosition3F().AddNormal3F().AddUV2F().AddFloat4("splatWeights").Build();
    }

    uint32 NkMeshSystem::GetMeshCount() const { return (uint32)mMeshes.Size(); }
    uint64 NkMeshSystem::GetUsedVtxBytes() const { return mVtxHead; }
    uint64 NkMeshSystem::GetUsedIdxBytes() const { return mIdxHead; }

    // Primitives (version minimale — builder complet dans NkProceduralMesh.cpp)
    NkMeshId NkMeshSystem::GetScreenQuad() {
        static NkMeshId cached = NK_INVALID_MESH;
        if(IsValid(cached)) return cached;
        auto lay = GetSimpleLayout();
        NkMeshBuilder mb(lay);
        float32 p[4][5]={{-1,-1,0,0,1},{1,-1,0,1,1},{1,1,0,1,0},{-1,1,0,0,0}};
        for(int i=0;i<4;i++){mb.BeginVertex();mb.Position(p[i][0],p[i][1]);mb.UV(p[i][2],p[i][3]);mb.EndVertex();}
        mb.Quad(0,1,2,3);
        cached=Upload(mb,"ScreenQuad"); return cached;
    }

    NkMeshId NkMeshSystem::GetCube(const NkVertexLayoutDef* lay) {
        auto l=lay?*lay:GetStandardLayout();
        NkMeshBuilder mb(l);
        // 6 faces → 6 sous-meshes pour usage type "CubeFaces"
        struct Face{math::NkVec3 n;math::NkVec3 u,v;};
        Face faces[6]={
            {{0,0,1},{1,0,0},{0,1,0}},{{0,0,-1},{-1,0,0},{0,1,0}},
            {{0,1,0},{1,0,0},{0,0,-1}},{{0,-1,0},{1,0,0},{0,0,1}},
            {{1,0,0},{0,0,-1},{0,1,0}},{{-1,0,0},{0,0,1},{0,1,0}}
        };
        for(int fi=0;fi<6;fi++){
            mb.BeginSubMesh(NkString("Face")+NkString::FromInt(fi));
            auto& f=faces[fi];
            math::NkVec3 c={f.n.x,f.n.y,f.n.z};
            float32 uv[4][2]={{0,0},{1,0},{1,1},{0,1}};
            math::NkVec3 pts[4]={
                {c.x+(-f.u.x-f.v.x)*0.5f,c.y+(-f.u.y-f.v.y)*0.5f,c.z+(-f.u.z-f.v.z)*0.5f},
                {c.x+(f.u.x-f.v.x)*0.5f, c.y+(f.u.y-f.v.y)*0.5f, c.z+(f.u.z-f.v.z)*0.5f},
                {c.x+(f.u.x+f.v.x)*0.5f, c.y+(f.u.y+f.v.y)*0.5f, c.z+(f.u.z+f.v.z)*0.5f},
                {c.x+(-f.u.x+f.v.x)*0.5f,c.y+(-f.u.y+f.v.y)*0.5f,c.z+(-f.u.z+f.v.z)*0.5f}
            };
            uint32 base=(uint32)mb.GetVertexCount();
            for(int vi=0;vi<4;vi++){
                mb.BeginVertex();
                mb.Position(pts[vi].x,pts[vi].y,pts[vi].z);
                mb.Normal(f.n.x,f.n.y,f.n.z);
                mb.UV(uv[vi][0],uv[vi][1]);
                mb.EndVertex();
            }
            mb.Quad(base,base+1,base+2,base+3);
            mb.EndSubMesh();
        }
        return Upload(mb,"Cube");
    }

} // namespace nkentseu
