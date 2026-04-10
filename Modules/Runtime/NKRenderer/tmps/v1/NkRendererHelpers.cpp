// =============================================================================
// NkRendererHelpers.cpp
// Implémentation des méthodes inline non triviales :
//   NkShaderDesc builders
//   NkIRenderer shortcuts (CreateQuad2D, CreateSphere3D…)
//   NkRenderer2D implementation
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkIRenderer.h"
#include "NKRenderer/2D/NkRenderer2D.h"
#include <cmath>

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // NkShaderDesc builders
    // =========================================================================
    static NkShaderStageDesc MakeStage(NkShaderStageType s, NkShaderBackend b,
                                       const char* src, const char* entry,
                                       bool isFile=false)
    {
        NkShaderStageDesc d{};
        d.stage      = s;
        d.backend    = b;
        d.source     = NkString(src ? src : "");
        d.entryPoint = NkString(entry ? entry : "main");
        d.isFilePath = isFile;
        return d;
    }

    NkShaderDesc& NkShaderDesc::AddGL(NkShaderStageType s, const char* src, const char* e)
    { stages.PushBack(MakeStage(s, NkShaderBackend::OpenGL, src, e)); return *this; }

    NkShaderDesc& NkShaderDesc::AddVK(NkShaderStageType s, const char* src, const char* e)
    { stages.PushBack(MakeStage(s, NkShaderBackend::Vulkan, src, e)); return *this; }

    NkShaderDesc& NkShaderDesc::AddDX11(NkShaderStageType s, const char* src, const char* e)
    { stages.PushBack(MakeStage(s, NkShaderBackend::DX11, src, e)); return *this; }

    NkShaderDesc& NkShaderDesc::AddDX12(NkShaderStageType s, const char* src, const char* e)
    { stages.PushBack(MakeStage(s, NkShaderBackend::DX12, src, e)); return *this; }

    NkShaderDesc& NkShaderDesc::AddGLFile(NkShaderStageType s, const char* p, const char* e)
    { stages.PushBack(MakeStage(s, NkShaderBackend::OpenGL, p, e, true)); return *this; }

    NkShaderDesc& NkShaderDesc::AddVKFile(NkShaderStageType s, const char* p, const char* e)
    { stages.PushBack(MakeStage(s, NkShaderBackend::Vulkan, p, e, true)); return *this; }

    NkShaderDesc& NkShaderDesc::AddDX11File(NkShaderStageType s, const char* p, const char* e)
    { stages.PushBack(MakeStage(s, NkShaderBackend::DX11, p, e, true)); return *this; }

    NkShaderDesc& NkShaderDesc::AddDX12File(NkShaderStageType s, const char* p, const char* e)
    { stages.PushBack(MakeStage(s, NkShaderBackend::DX12, p, e, true)); return *this; }

    // =========================================================================
    // NkIRenderer — shortcuts géométrie procédurale
    // =========================================================================
    NkMeshHandle NkIRenderer::CreateQuad2D(float32 w, float32 h) {
        const float32 hw = w * 0.5f, hh = h * 0.5f;
        NkVertex2D verts[4] = {
            {{-hw, -hh}, {0,0}, {1,1,1,1}},
            {{ hw, -hh}, {1,0}, {1,1,1,1}},
            {{-hw,  hh}, {0,1}, {1,1,1,1}},
            {{ hw,  hh}, {1,1}, {1,1,1,1}},
        };
        uint32 idx[6] = {0,1,2, 1,3,2};
        NkMeshDesc d{};
        d.usage       = NkMeshUsage::Static;
        d.vertices2D  = verts;
        d.vertexCount = 4;
        d.indices32   = idx;
        d.indexCount  = 6;
        d.debugName   = "nk/quad2d";
        return CreateMesh(d);
    }

    NkMeshHandle NkIRenderer::CreateCube3D(float32 half) {
        const float P = half, N = -half;
        struct Face { float vx[4][3]; float uv[4][2]; float nx,ny,nz; };
        static const Face faces[6] = {
            {{{P,N,P},{P,P,P},{N,P,P},{N,N,P}}, {{1,0},{1,1},{0,1},{0,0}},  0, 0, 1},
            {{{N,N,N},{N,P,N},{P,P,N},{P,N,N}}, {{1,0},{1,1},{0,1},{0,0}},  0, 0,-1},
            {{{N,N,N},{P,N,N},{P,N,P},{N,N,P}}, {{0,1},{1,1},{1,0},{0,0}},  0,-1, 0},
            {{{N,P,P},{P,P,P},{P,P,N},{N,P,N}}, {{0,1},{1,1},{1,0},{0,0}},  0, 1, 0},
            {{{N,N,N},{N,N,P},{N,P,P},{N,P,N}}, {{0,0},{1,0},{1,1},{0,1}}, -1, 0, 0},
            {{{P,N,P},{P,N,N},{P,P,N},{P,P,P}}, {{0,0},{1,0},{1,1},{0,1}},  1, 0, 0},
        };
        static const int idx[6] = {0,1,2,0,2,3};
        NkVector<NkVertex3D> verts; verts.Reserve(36);
        for (const auto& f : faces) {
            for (int i : idx) {
                NkVertex3D v{};
                v.position = {f.vx[i][0], f.vx[i][1], f.vx[i][2]};
                v.normal   = {f.nx, f.ny, f.nz};
                v.uv       = {f.uv[i][0], f.uv[i][1]};
                v.color    = {1,1,1,1};
                verts.PushBack(v);
            }
        }
        NkMeshDesc d{};
        d.usage       = NkMeshUsage::Static;
        d.vertices3D  = verts.Begin();
        d.vertexCount = 36;
        d.debugName   = "nk/cube3d";
        return CreateMesh(d);
    }

    NkMeshHandle NkIRenderer::CreateSphere3D(float32 radius, uint32 stacks, uint32 slices) {
        const float32 pi = 3.14159265359f;
        NkVector<NkVertex3D> verts;
        NkVector<uint32>     idxs;
        verts.Reserve((stacks+1)*(slices+1));

        for (uint32 i=0; i<=stacks; ++i) {
            const float32 phi = pi * (float32)i / stacks;
            for (uint32 j=0; j<=slices; ++j) {
                const float32 theta = 2.f*pi * (float32)j / slices;
                const float32 x = sinf(phi)*cosf(theta);
                const float32 y = cosf(phi);
                const float32 z = sinf(phi)*sinf(theta);
                NkVertex3D v{};
                v.position = {x*radius, y*radius, z*radius};
                v.normal   = {x, y, z};
                v.uv       = {(float32)j/slices, (float32)i/stacks};
                v.color    = {1,1,1,1};
                verts.PushBack(v);
            }
        }
        for (uint32 i=0; i<stacks; ++i) {
            for (uint32 j=0; j<slices; ++j) {
                const uint32 a = i*(slices+1)+j;
                const uint32 b = a+slices+1;
                idxs.PushBack(a);   idxs.PushBack(b);   idxs.PushBack(a+1);
                idxs.PushBack(b);   idxs.PushBack(b+1); idxs.PushBack(a+1);
            }
        }
        NkMeshDesc d{};
        d.usage       = NkMeshUsage::Static;
        d.vertices3D  = verts.Begin();
        d.vertexCount = (uint32)verts.Size();
        d.indices32   = idxs.Begin();
        d.indexCount  = (uint32)idxs.Size();
        d.debugName   = "nk/sphere3d";
        return CreateMesh(d);
    }

    NkMeshHandle NkIRenderer::CreatePlane3D(float32 width, float32 depth,
                                             uint32 subdivW, uint32 subdivD)
    {
        const uint32 cols = subdivW + 1;
        const uint32 rows = subdivD + 1;
        NkVector<NkVertex3D> verts; verts.Reserve(cols * rows);
        NkVector<uint32>     idxs;  idxs.Reserve(subdivW * subdivD * 6);

        for (uint32 r=0; r<rows; ++r) {
            for (uint32 c=0; c<cols; ++c) {
                const float32 u = (float32)c / subdivW;
                const float32 v = (float32)r / subdivD;
                NkVertex3D vert{};
                vert.position = {(u-0.5f)*width, 0.f, (v-0.5f)*depth};
                vert.normal   = {0,1,0};
                vert.uv       = {u, v};
                vert.color    = {1,1,1,1};
                verts.PushBack(vert);
            }
        }
        for (uint32 r=0; r<subdivD; ++r) {
            for (uint32 c=0; c<subdivW; ++c) {
                const uint32 a = r*cols+c, b=a+cols;
                idxs.PushBack(a);   idxs.PushBack(a+1); idxs.PushBack(b);
                idxs.PushBack(a+1); idxs.PushBack(b+1); idxs.PushBack(b);
            }
        }
        NkMeshDesc d{};
        d.usage       = NkMeshUsage::Static;
        d.vertices3D  = verts.Begin();
        d.vertexCount = (uint32)verts.Size();
        d.indices32   = idxs.Begin();
        d.indexCount  = (uint32)idxs.Size();
        d.debugName   = "nk/plane3d";
        return CreateMesh(d);
    }

    NkTextureHandle NkIRenderer::LoadTexture(const char* filePath,
                                              NkTextureFormat fmt,
                                              bool generateMips)
    {
        // Utilise NkImage pour charger le fichier
        NkImage* img = NkImage::LoadSTB(filePath, 4);
        if (!img || !img->IsValid()) {
            if (img) img->Free();
            return NkTextureHandle::Null();
        }

        NkTextureDesc d = NkTextureDesc::Tex2D(
            (uint32)img->Width(), (uint32)img->Height(), fmt,
            generateMips ? 0 : 1);
        d.initialData = img->Pixels();
        d.rowPitch    = (uint32)img->Stride();
        d.debugName   = filePath;

        NkTextureHandle h = CreateTexture(d);
        if (h.IsValid() && generateMips) GenerateMipmaps(h);

        img->Free();
        return h;
    }

    // =========================================================================
    // NkRenderer2D
    // =========================================================================
    bool NkRenderer2D::Init(uint32 viewW, uint32 viewH) {
        mViewport = {(float32)viewW, (float32)viewH};
        NkCameraDesc2D d{};
        d.viewW = viewW;
        d.viewH = viewH;
        d.zoom  = 1.f;
        mCamera = mRenderer->CreateCamera2D(d);
        return mCamera.IsValid();
    }

    void NkRenderer2D::Destroy() {
        if (mCamera.IsValid()) mRenderer->DestroyCamera(mCamera);
    }

    void NkRenderer2D::Begin(NkRendererCommand& cmd) {
        mCmd = &cmd;
        mTransformStack.top = 0;
        mTransformStack.transforms[0] = NkMat3f::Identity();
    }

    void NkRenderer2D::End() {
        mCmd = nullptr;
    }

    void NkRenderer2D::SetCamera(NkVec2f position, float32 zoom, float32 rotation) {
        NkCameraDesc2D d{};
        d.viewW    = (uint32)mViewport.x;
        d.viewH    = (uint32)mViewport.y;
        d.position = position;
        d.zoom     = zoom;
        d.rotation = rotation;
        mRenderer->UpdateCamera2D(mCamera, d);
    }

    void NkRenderer2D::SetViewport(uint32 w, uint32 h) {
        mViewport = {(float32)w, (float32)h};
        NkCameraDesc2D d{};
        d.viewW = w; d.viewH = h;
        mRenderer->UpdateCamera2D(mCamera, d);
    }

    void NkRenderer2D::DrawSprite(NkTextureHandle tex,
                                   NkVec2f center, NkVec2f size,
                                   NkVec4f color, float32 rotation,
                                   NkVec4f /*uvRect*/,
                                   float32 depth, uint32 layer)
    {
        if (!mCmd) return;
        mCmd->DrawSprite(tex, center, size, color, rotation, depth, layer);
    }

    void NkRenderer2D::DrawSpriteCorner(NkTextureHandle tex, NkVec2f topLeft,
                                         NkVec2f size, NkVec4f color,
                                         float32 depth, uint32 layer)
    {
        DrawSprite(tex, {topLeft.x + size.x*0.5f, topLeft.y + size.y*0.5f},
                   size, color, 0.f, {0,0,1,1}, depth, layer);
    }

    void NkRenderer2D::FillRect(NkVec2f pos, NkVec2f size, NkVec4f color,
                                 float32 depth, uint32 layer)
    {
        if (mCmd) mCmd->DrawRect2D(pos, size, color, depth, layer);
    }

    void NkRenderer2D::DrawRect(NkVec2f pos, NkVec2f size, NkVec4f color,
                                 float32 thickness, float32 depth, uint32 layer)
    {
        if (mCmd) mCmd->DrawRectOutline2D(pos, size, color, thickness, depth, layer);
    }

    void NkRenderer2D::FillCircle(NkVec2f center, float32 radius, NkVec4f color,
                                   uint32 segments, float32 depth, uint32 layer)
    {
        if (mCmd) mCmd->DrawCircle2D(center, radius, color, segments, depth, layer);
    }

    void NkRenderer2D::DrawCircle(NkVec2f center, float32 radius, NkVec4f color,
                                   float32 /*thickness*/, uint32 segs,
                                   float32 depth, uint32 layer)
    {
        if (mCmd) mCmd->DrawCircle2D(center, radius, color, segs, depth, layer);
    }

    void NkRenderer2D::DrawLine(NkVec2f start, NkVec2f end, NkVec4f color,
                                 float32 thickness, float32 depth, uint32 layer)
    {
        if (mCmd) mCmd->DrawLine2D(start, end, color, thickness, depth, layer);
    }

    void NkRenderer2D::DrawArrow(NkVec2f start, NkVec2f end, NkVec4f color,
                                  float32 thickness, float32 headSize,
                                  float32 depth, uint32 layer)
    {
        if (!mCmd) return;
        mCmd->DrawLine2D(start, end, color, thickness, depth, layer);
        // Tête de flèche (deux lignes)
        const float32 dx = end.x - start.x, dy = end.y - start.y;
        const float32 len = sqrtf(dx*dx + dy*dy);
        if (len < 1e-4f) return;
        const float32 ux = dx/len, uy = dy/len;
        const float32 px = -uy, py = ux;
        const NkVec2f p1 = {end.x - ux*headSize + px*headSize*0.4f,
                             end.y - uy*headSize + py*headSize*0.4f};
        const NkVec2f p2 = {end.x - ux*headSize - px*headSize*0.4f,
                             end.y - uy*headSize - py*headSize*0.4f};
        mCmd->DrawLine2D(end, p1, color, thickness, depth, layer);
        mCmd->DrawLine2D(end, p2, color, thickness, depth, layer);
    }

    void NkRenderer2D::FillTriangle(NkVec2f a, NkVec2f b, NkVec2f c,
                                     NkVec4f color, float32 depth, uint32 layer)
    {
        if (!mCmd) return;
        // Triangle via 3 lignes (approximation — pour un vrai fill il faudrait
        // un mesh dynamique) ou DrawRect2D d'une boîte englobante
        mCmd->DrawLine2D(a, b, color, 1.f, depth, layer);
        mCmd->DrawLine2D(b, c, color, 1.f, depth, layer);
        mCmd->DrawLine2D(c, a, color, 1.f, depth, layer);
    }

    void NkRenderer2D::DrawText(NkFontHandle font, const char* text,
                                 NkVec2f position, float32 size,
                                 NkVec4f color, float32 maxWidth, uint32 layer)
    {
        if (mCmd) mCmd->DrawText(font, text, position, size, color, maxWidth, layer);
    }

    NkVec2f NkRenderer2D::MeasureText(NkFontHandle font, const char* text,
                                       float32 size) const
    {
        // Approximation simple : 0.6 * size par caractère (à remplacer par le vrai glyphe)
        if (!text) return {0,0};
        uint32 len = 0;
        for (const char* p=text; *p; ++p) if ((*p & 0xC0) != 0x80) ++len;
        return {len * size * 0.6f, size};
    }

    void NkRenderer2D::PushTransform(NkMat3f transform) {
        if (mTransformStack.top < 31) {
            mTransformStack.top++;
            mTransformStack.transforms[mTransformStack.top] = transform;
        }
    }

    void NkRenderer2D::PopTransform() {
        if (mTransformStack.top > 0) mTransformStack.top--;
    }

    NkMat3f NkRenderer2D::GetCurrentTransform() const {
        return mTransformStack.transforms[mTransformStack.top];
    }

} // namespace renderer
} // namespace nkentseu