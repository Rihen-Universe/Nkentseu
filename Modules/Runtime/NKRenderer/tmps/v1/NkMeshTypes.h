#pragma once
// =============================================================================
// NkMeshTypes.h
// Wrappers haut niveau pour les types de mesh.
//
// NkStaticMesh   — géométrie immutable (upload une fois, GPU read-only)
// NkDynamicMesh  — géométrie mise à jour chaque frame (streaming, particules)
// NkEditorMesh   — géométrie CPU-side lisible/modifiable (picking, déformation)
// NkModel        — ensemble de submeshes avec des matériaux associés
// =============================================================================
#include "NKRenderer/Core/NkIRenderer.h"

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // NkStaticMesh — géométrie immutable
    // =========================================================================
    class NkStaticMesh {
    public:
        explicit NkStaticMesh(NkIRenderer* renderer) : mRenderer(renderer) {}
        ~NkStaticMesh() { Destroy(); }

        NkStaticMesh(const NkStaticMesh&) = delete;
        NkStaticMesh& operator=(const NkStaticMesh&) = delete;

        // Créer depuis des données CPU (upload unique)
        bool Create(const NkVertex3D* vertices, uint32 vertexCount,
                    const uint32* indices=nullptr, uint32 indexCount=0,
                    NkPrimitiveMode mode=NkPrimitiveMode::Triangles,
                    const char* debugName=nullptr)
        {
            Destroy();
            NkMeshDesc desc{};
            desc.usage       = NkMeshUsage::Static;
            desc.mode        = mode;
            desc.idxFmt      = NkIndexFormat::UInt32;
            desc.vertices3D  = vertices;
            desc.vertexCount = vertexCount;
            desc.indices32   = indices;
            desc.indexCount  = indexCount;
            desc.debugName   = debugName;
            mHandle = mRenderer->CreateMesh(desc);
            return mHandle.IsValid();
        }

        // Créer depuis des vertices 2D
        bool Create2D(const NkVertex2D* vertices, uint32 vertexCount,
                      const uint32* indices=nullptr, uint32 indexCount=0,
                      const char* debugName=nullptr)
        {
            Destroy();
            NkMeshDesc desc{};
            desc.usage       = NkMeshUsage::Static;
            desc.idxFmt      = NkIndexFormat::UInt32;
            desc.vertices2D  = vertices;
            desc.vertexCount = vertexCount;
            desc.indices32   = indices;
            desc.indexCount  = indexCount;
            desc.debugName   = debugName;
            mHandle = mRenderer->CreateMesh(desc);
            return mHandle.IsValid();
        }

        void Destroy() {
            if (mHandle.IsValid()) {
                mRenderer->DestroyMesh(mHandle);
            }
        }

        NkMeshHandle GetHandle() const { return mHandle; }
        bool IsValid() const { return mHandle.IsValid(); }

    private:
        NkIRenderer* mRenderer = nullptr;
        NkMeshHandle mHandle;
    };

    // =========================================================================
    // NkDynamicMesh — géométrie mise à jour fréquemment
    // =========================================================================
    class NkDynamicMesh {
    public:
        explicit NkDynamicMesh(NkIRenderer* renderer) : mRenderer(renderer) {}
        ~NkDynamicMesh() { Destroy(); }

        NkDynamicMesh(const NkDynamicMesh&) = delete;
        NkDynamicMesh& operator=(const NkDynamicMesh&) = delete;

        // Créer avec capacité initiale (buffer GPU pré-alloué)
        bool Create(uint32 maxVertices, uint32 maxIndices=0,
                    NkPrimitiveMode mode=NkPrimitiveMode::Triangles,
                    bool is2D=false, const char* debugName=nullptr)
        {
            Destroy();
            mMaxVertices = maxVertices;
            mMaxIndices  = maxIndices;
            mIs2D        = is2D;

            // Pré-allouer avec des données nulles pour réserver le buffer GPU
            NkMeshDesc desc{};
            desc.usage       = NkMeshUsage::Dynamic;
            desc.mode        = mode;
            desc.idxFmt      = NkIndexFormat::UInt32;
            desc.vertexCount = 0;
            desc.indexCount  = 0;
            desc.debugName   = debugName;

            if (is2D) {
                mScratch2D.Resize(maxVertices);
                desc.vertices2D = mScratch2D.Begin();
            } else {
                mScratch3D.Resize(maxVertices);
                desc.vertices3D = mScratch3D.Begin();
            }
            if (maxIndices > 0) {
                mScratchIdx.Resize(maxIndices);
                desc.indices32  = mScratchIdx.Begin();
            }

            mHandle = mRenderer->CreateMesh(desc);
            return mHandle.IsValid();
        }

        // Mise à jour (appelée avant chaque draw)
        bool Update(const NkVertex3D* vertices, uint32 vertexCount,
                    const uint32* indices=nullptr, uint32 indexCount=0)
        {
            if (!mHandle.IsValid()) return false;
            return mRenderer->UpdateMesh(mHandle, vertices, vertexCount,
                                         indices, indexCount);
        }

        bool Update2D(const NkVertex2D* vertices, uint32 vertexCount,
                      const uint32* indices=nullptr, uint32 indexCount=0)
        {
            if (!mHandle.IsValid()) return false;
            return mRenderer->UpdateMesh(mHandle, vertices, vertexCount,
                                         indices, indexCount);
        }

        // Accès au buffer scratch (pour modification CPU directe)
        NkVertex3D* BeginUpdate3D(uint32 maxVerts) {
            mScratch3D.Resize(maxVerts > mMaxVertices ? mMaxVertices : maxVerts);
            return mScratch3D.Begin();
        }
        NkVertex2D* BeginUpdate2D(uint32 maxVerts) {
            mScratch2D.Resize(maxVerts > mMaxVertices ? mMaxVertices : maxVerts);
            return mScratch2D.Begin();
        }
        uint32* BeginUpdateIndices(uint32 maxIdx) {
            mScratchIdx.Resize(maxIdx > mMaxIndices ? mMaxIndices : maxIdx);
            return mScratchIdx.Begin();
        }

        bool EndUpdate(uint32 actualVertices, uint32 actualIndices=0) {
            if (!mHandle.IsValid()) return false;
            const void* v = mIs2D ? (const void*)mScratch2D.Begin()
                                  : (const void*)mScratch3D.Begin();
            const void* i = actualIndices > 0 ? (const void*)mScratchIdx.Begin() : nullptr;
            return mRenderer->UpdateMesh(mHandle, v, actualVertices, i, actualIndices);
        }

        void Destroy() {
            if (mHandle.IsValid()) mRenderer->DestroyMesh(mHandle);
            mScratch2D.Clear();
            mScratch3D.Clear();
            mScratchIdx.Clear();
        }

        NkMeshHandle GetHandle() const { return mHandle; }
        bool IsValid() const { return mHandle.IsValid(); }
        uint32 GetMaxVertices() const { return mMaxVertices; }

    private:
        NkIRenderer*      mRenderer   = nullptr;
        NkMeshHandle      mHandle;
        uint32            mMaxVertices= 0;
        uint32            mMaxIndices = 0;
        bool              mIs2D       = false;
        NkVector<NkVertex3D> mScratch3D;
        NkVector<NkVertex2D> mScratch2D;
        NkVector<uint32>     mScratchIdx;
    };

    // =========================================================================
    // NkEditorMesh — géométrie lisible/modifiable côté CPU
    // =========================================================================
    struct NkEditorVertex {
        NkVec3f  position;
        NkVec3f  normal;
        NkVec3f  tangent;
        NkVec2f  uv;
        NkVec4f  color = {1.f, 1.f, 1.f, 1.f};
        uint32   index = 0;   // index d'origine dans le mesh (pour le picking)
    };

    class NkEditorMesh {
    public:
        explicit NkEditorMesh(NkIRenderer* renderer) : mRenderer(renderer) {}
        ~NkEditorMesh() { Destroy(); }

        bool Create(const NkVertex3D* vertices, uint32 vCount,
                    const uint32* indices, uint32 iCount,
                    const char* debugName=nullptr)
        {
            Destroy();
            mVertices.Resize(vCount);
            for (uint32 i = 0; i < vCount; ++i) {
                auto& e = mVertices[i];
                e.position = vertices[i].position;
                e.normal   = vertices[i].normal;
                e.tangent  = vertices[i].tangent;
                e.uv       = vertices[i].uv;
                e.color    = vertices[i].color;
                e.index    = i;
            }
            mIndices.Resize(iCount);
            for (uint32 i = 0; i < iCount; ++i) mIndices[i] = indices[i];

            // Upload initial
            UploadToGPU(debugName);
            return mHandle.IsValid();
        }

        // Modification d'un vertex (marque le mesh comme dirty)
        void SetVertexPosition(uint32 idx, NkVec3f pos) {
            if (idx < mVertices.Size()) {
                mVertices[idx].position = pos;
                mDirty = true;
            }
        }

        void SetVertexColor(uint32 idx, NkVec4f color) {
            if (idx < mVertices.Size()) {
                mVertices[idx].color = color;
                mDirty = true;
            }
        }

        // Recalcul des normales par face
        void RecalculateNormals();

        // Recalcul des tangentes (nécessite des UV)
        void RecalculateTangents();

        // Upload vers le GPU si dirty
        bool Flush() {
            if (!mDirty || !mHandle.IsValid()) return true;
            // Convertir NkEditorVertex → NkVertex3D pour l'upload
            NkVector<NkVertex3D> v3;
            v3.Resize(mVertices.Size());
            for (usize i = 0; i < mVertices.Size(); ++i) {
                v3[i].position = mVertices[i].position;
                v3[i].normal   = mVertices[i].normal;
                v3[i].tangent  = mVertices[i].tangent;
                v3[i].uv       = mVertices[i].uv;
                v3[i].color    = mVertices[i].color;
            }
            mRenderer->UpdateMesh(mHandle, v3.Begin(), (uint32)v3.Size(),
                                  mIndices.Begin(), (uint32)mIndices.Size());
            mDirty = false;
            return true;
        }

        // Picking : retourne l'index du triangle le plus proche du rayon (ou -1)
        int32 RaycastTriangle(NkVec3f origin, NkVec3f direction,
                              float32& outT) const;

        const NkVector<NkEditorVertex>& GetVertices() const { return mVertices; }
        const NkVector<uint32>&         GetIndices()  const { return mIndices; }
        NkMeshHandle                    GetHandle()   const { return mHandle; }
        bool IsValid() const { return mHandle.IsValid(); }

        void Destroy() {
            if (mHandle.IsValid()) mRenderer->DestroyMesh(mHandle);
            mVertices.Clear();
            mIndices.Clear();
        }

    private:
        void UploadToGPU(const char* debugName) {
            NkVector<NkVertex3D> v3;
            v3.Resize(mVertices.Size());
            for (usize i = 0; i < mVertices.Size(); ++i) {
                v3[i].position = mVertices[i].position;
                v3[i].normal   = mVertices[i].normal;
                v3[i].tangent  = mVertices[i].tangent;
                v3[i].uv       = mVertices[i].uv;
                v3[i].color    = mVertices[i].color;
            }
            NkMeshDesc desc{};
            desc.usage       = NkMeshUsage::Editor;
            desc.vertices3D  = v3.Begin();
            desc.vertexCount = (uint32)v3.Size();
            desc.indices32   = mIndices.Begin();
            desc.indexCount  = (uint32)mIndices.Size();
            desc.debugName   = debugName;
            mHandle = mRenderer->CreateMesh(desc);
        }

        NkIRenderer*           mRenderer = nullptr;
        NkMeshHandle           mHandle;
        NkVector<NkEditorVertex> mVertices;
        NkVector<uint32>         mIndices;
        bool                   mDirty = false;
    };

    // =========================================================================
    // NkSubMesh — un sous-mesh avec son matériau
    // =========================================================================
    struct NkSubMesh {
        NkMeshHandle         mesh;
        NkMaterialInstHandle material;
        NkVec3f              boundsMin = {-1,-1,-1};
        NkVec3f              boundsMax = {  1, 1, 1};
        bool                 visible   = true;
        char                 name[64]  = {};
    };

    // =========================================================================
    // NkModel — ensemble de submeshes
    // =========================================================================
    class NkModel {
    public:
        explicit NkModel(NkIRenderer* renderer) : mRenderer(renderer) {}
        ~NkModel() { Destroy(); }

        NkModel(const NkModel&) = delete;
        NkModel& operator=(const NkModel&) = delete;

        // Ajouter un sous-mesh
        void AddSubMesh(const NkSubMesh& sub) {
            mSubMeshes.PushBack(sub);
            UpdateBounds();
        }

        // Accès
        const NkVector<NkSubMesh>& GetSubMeshes() const { return mSubMeshes; }
        NkSubMesh*       GetSubMesh(uint32 idx) {
            return (idx < mSubMeshes.Size()) ? &mSubMeshes[idx] : nullptr;
        }

        // AABB monde (avant transform)
        NkVec3f GetBoundsMin() const { return mBoundsMin; }
        NkVec3f GetBoundsMax() const { return mBoundsMax; }

        // Visibilité globale
        void SetVisible(bool v) { mVisible = v; }
        bool IsVisible() const  { return mVisible; }

        uint32 GetSubMeshCount() const { return (uint32)mSubMeshes.Size(); }

        // Émettre tous les draw calls vers le command
        void Draw(NkRendererCommand& cmd, const NkMat4f& transform,
                  bool castShadow=true, bool receiveShadow=true,
                  uint32 layer=0) const
        {
            if (!mVisible) return;
            for (usize i = 0; i < mSubMeshes.Size(); ++i) {
                const NkSubMesh& s = mSubMeshes[i];
                if (!s.visible || !s.mesh.IsValid() || !s.material.IsValid()) continue;
                cmd.Draw3D(s.mesh, s.material, transform,
                           castShadow, receiveShadow, layer);
            }
        }

        void Destroy() {
            mSubMeshes.Clear();
        }

    private:
        void UpdateBounds() {
            if (mSubMeshes.IsEmpty()) return;
            mBoundsMin = mSubMeshes[0].boundsMin;
            mBoundsMax = mSubMeshes[0].boundsMax;
            for (usize i = 1; i < mSubMeshes.Size(); ++i) {
                const auto& s = mSubMeshes[i];
                mBoundsMin.x = NkMin(mBoundsMin.x, s.boundsMin.x);
                mBoundsMin.y = NkMin(mBoundsMin.y, s.boundsMin.y);
                mBoundsMin.z = NkMin(mBoundsMin.z, s.boundsMin.z);
                mBoundsMax.x = NkMax(mBoundsMax.x, s.boundsMax.x);
                mBoundsMax.y = NkMax(mBoundsMax.y, s.boundsMax.y);
                mBoundsMax.z = NkMax(mBoundsMax.z, s.boundsMax.z);
            }
        }

        NkIRenderer*     mRenderer = nullptr;
        NkVector<NkSubMesh> mSubMeshes;
        NkVec3f          mBoundsMin = {-1,-1,-1};
        NkVec3f          mBoundsMax = { 1, 1, 1};
        bool             mVisible   = true;
    };

    // =========================================================================
    // NkCamera2D / NkCamera3D — wrappers caméra pratiques
    // =========================================================================
    class NkCamera2D {
    public:
        explicit NkCamera2D(NkIRenderer* renderer)
            : mRenderer(renderer) {}
        ~NkCamera2D() { Destroy(); }

        bool Create(uint32 viewW, uint32 viewH) {
            NkCameraDesc2D d{};
            d.viewW = viewW; d.viewH = viewH;
            d.zoom = 1.f;
            mHandle = mRenderer->CreateCamera2D(d);
            mDesc   = d;
            return mHandle.IsValid();
        }

        void SetPosition(NkVec2f pos) { mDesc.position=pos; Upload(); }
        void SetZoom(float32 z)       { mDesc.zoom=z;        Upload(); }
        void SetRotation(float32 deg) { mDesc.rotation=deg;  Upload(); }
        void SetViewport(uint32 w, uint32 h) { mDesc.viewW=w; mDesc.viewH=h; Upload(); }

        void Pan(NkVec2f delta)       { mDesc.position.x+=delta.x; mDesc.position.y+=delta.y; Upload(); }
        void Zoom(float32 factor)     { mDesc.zoom=NkMax(0.01f, mDesc.zoom*factor); Upload(); }

        NkVec2f ScreenToWorld(NkVec2f screenPx) const {
            const float32 hw = mDesc.viewW * 0.5f / mDesc.zoom;
            const float32 hh = mDesc.viewH * 0.5f / mDesc.zoom;
            return {
                mDesc.position.x + (screenPx.x / mDesc.viewW * 2.f - 1.f) * hw,
                mDesc.position.y + (screenPx.y / mDesc.viewH * 2.f - 1.f) * hh
            };
        }

        NkVec2f WorldToScreen(NkVec2f worldPt) const {
            const float32 hw = mDesc.viewW * 0.5f / mDesc.zoom;
            const float32 hh = mDesc.viewH * 0.5f / mDesc.zoom;
            return {
                ((worldPt.x - mDesc.position.x) / hw + 1.f) * 0.5f * mDesc.viewW,
                ((worldPt.y - mDesc.position.y) / hh + 1.f) * 0.5f * mDesc.viewH
            };
        }

        NkCameraHandle GetHandle() const { return mHandle; }
        const NkCameraDesc2D& GetDesc() const { return mDesc; }

        void Destroy() { if (mHandle.IsValid()) mRenderer->DestroyCamera(mHandle); }

    private:
        void Upload() { if (mHandle.IsValid()) mRenderer->UpdateCamera2D(mHandle, mDesc); }
        NkIRenderer*  mRenderer = nullptr;
        NkCameraHandle mHandle;
        NkCameraDesc2D mDesc;
    };

    class NkCamera3D {
    public:
        explicit NkCamera3D(NkIRenderer* renderer)
            : mRenderer(renderer) {}
        ~NkCamera3D() { Destroy(); }

        bool Create(const NkCameraDesc3D& desc={}) {
            mHandle = mRenderer->CreateCamera3D(desc);
            mDesc   = desc;
            return mHandle.IsValid();
        }

        void LookAt(NkVec3f eye, NkVec3f target, NkVec3f up={0,1,0}) {
            mDesc.position=eye; mDesc.target=target; mDesc.up=up; Upload();
        }
        void SetFOV   (float32 deg) { mDesc.fovY=deg;       Upload(); }
        void SetAspect(float32 a)   { mDesc.aspect=a;       Upload(); }
        void SetClip  (float32 n, float32 f) { mDesc.nearPlane=n; mDesc.farPlane=f; Upload(); }

        // Orbit autour du target
        void Orbit(float32 yawDelta, float32 pitchDelta) {
            (void)yawDelta; (void)pitchDelta; /* TODO : implémentation mathématique */
            Upload();
        }

        NkMat4f GetViewMatrix()       const { return mRenderer->GetViewMatrix(mHandle); }
        NkMat4f GetProjectionMatrix() const { return mRenderer->GetProjectionMatrix(mHandle); }
        NkMat4f GetViewProjection()   const { return mRenderer->GetViewProjectionMatrix(mHandle); }

        NkCameraHandle GetHandle() const { return mHandle; }
        const NkCameraDesc3D& GetDesc() const { return mDesc; }

        void Destroy() { if (mHandle.IsValid()) mRenderer->DestroyCamera(mHandle); }

    private:
        void Upload() { if (mHandle.IsValid()) mRenderer->UpdateCamera3D(mHandle, mDesc); }
        NkIRenderer*   mRenderer = nullptr;
        NkCameraHandle mHandle;
        NkCameraDesc3D mDesc;
    };

} // namespace renderer
} // namespace nkentseu