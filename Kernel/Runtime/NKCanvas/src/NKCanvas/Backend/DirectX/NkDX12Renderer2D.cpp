// =============================================================================
// NkDX12Renderer2D.cpp — DirectX 12 2D renderer
// Root signature: root constants (16 floats = projection), descriptor table
// (SRV t0), static sampler (s0). No CBV heap needed.
// =============================================================================
#include "NkDX12Renderer2D.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)

#include "NKCanvas/Renderer/Resources/NkTexture.h"
#include "NKCanvas/Renderer/Resources/NkTextureBackend.h"
#include "NKCanvas/Renderer/Resources/NkShaderBackend.h"
#include "NKCanvas/Renderer/Targets/NkRenderTextureBackend.h"
#include "NKCanvas/Core/NkNativeContextAccess.h"
#include "NkDirectXContextData.h"
#include "NKLogger/NkLog.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMemory/NkAllocator.h"
#include <new>  // placement new

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

// #include <d3dx12.h>

#define NK_DX12_2D_LOG(...) logger.Infof("[NkDX12-2D] " __VA_ARGS__)
#define NK_DX12_2D_ERR(...) logger.Errorf("[NkDX12-2D] " __VA_ARGS__)
#define NK_DX12_2D_CHECK(hr,msg) do{if(FAILED(hr)){NK_DX12_2D_ERR(msg " 0x%08X",(unsigned)(hr));return false;}}while(0)

namespace nkentseu {
    namespace renderer {

        // ── HLSL ─────────────────────────────────────────────────────────────────────
        static const char* kDX12_2D_HLSL = R"(
        struct Proj { float4x4 proj; };
        ConstantBuffer<Proj> u_CB : register(b0);

        Texture2D    t_Texture : register(t0);
        SamplerState s_Sampler : register(s0);

        struct VSIn  { float2 pos : POSITION; float2 uv : TEXCOORD0; float4 col : COLOR; };
        struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; float4 col : COLOR; };

        VSOut VS(VSIn v) {
            VSOut o;
            o.pos = mul(u_CB.proj, float4(v.pos, 0.f, 1.f));
            o.uv  = v.uv;
            o.col = v.col;
            return o;
        }

        float4 PS(VSOut v) : SV_TARGET {
            return t_Texture.Sample(s_Sampler, v.uv) * v.col;
        }
        )";

        // =============================================================================
        // Registry globale pour le dispatch NkTextureBackend.
        // Les 5 callbacks sont statiques (pas de `this`), donc l'etat partage
        // (device, cmdQueue, srvHeap, ...) vit dans une struct file-scope. Capture
        // dans Initialize(), purge dans Shutdown(). Pas thread-safe : tout doit
        // s'executer sur le thread du contexte GPU.
        // =============================================================================
        struct NkDX12TextureEntry {
            ID3D12Resource* resource    = nullptr;  // tex2D dans DEFAULT heap
            ID3D12Resource* uploadHeap  = nullptr;  // upload heap conserve pour Update
            uint32          srvIndex    = 0;        // slot dans gDX12Registry.srvHeap
            NkTextureFilter filter      = NkTextureFilter::NK_LINEAR;
            NkTextureWrap   wrap        = NkTextureWrap::NK_CLAMP;
            uint32          width       = 0;
            uint32          height      = 0;
            uint32          uploadRowPitch = 0;     // pitch aligne 256 (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)
            bool            alive       = false;    // false apres Delete (slot recyclable)
        };

        static struct {
            ID3D12Device*              device        = nullptr;
            ID3D12CommandQueue*        cmdQueue      = nullptr;
            ID3D12DescriptorHeap*      srvHeap       = nullptr;  // CBV_SRV_UAV heap (du renderer)
            uint32                     srvHeapStride = 0;
            uint32                     srvHeapMaxSlots = 0;
            uint32*                    nextSrvSlot   = nullptr;  // ref sur mNextSRVSlot du renderer

            // Command list + allocator + fence dedies aux uploads de textures.
            // Ne reutilise PAS le cmdList du contexte (qui appartient au frame
            // courant et n'est ferme qu'a Present).
            ID3D12CommandAllocator*    uploadAllocator = nullptr;
            ID3D12GraphicsCommandList* uploadCmdList   = nullptr;
            ID3D12Fence*               uploadFence     = nullptr;
            HANDLE                     uploadEvent     = nullptr;
            uint64                     uploadFenceVal  = 0;

            NkVector<NkDX12TextureEntry*> entries;    // index = id-1 (id=0 reserve "invalide")
        } gDX12Registry;

        // -----------------------------------------------------------------------------
        // Helpers internes a la registry (anonymes pour eviter polluer la classe)
        // -----------------------------------------------------------------------------
        namespace {

            inline D3D12_CPU_DESCRIPTOR_HANDLE NkDX12_RegCPUHandle(uint32 slot) {
                D3D12_CPU_DESCRIPTOR_HANDLE h = gDX12Registry.srvHeap->GetCPUDescriptorHandleForHeapStart();
                h.ptr += (uint64)slot * gDX12Registry.srvHeapStride;
                return h;
            }

            // Soumet uploadCmdList sur cmdQueue puis attend la completion via fence.
            // CRITIQUE : sans cette attente, le DEFAULT heap ne contiendrait pas
            // encore les pixels au moment du premier draw qui reference la SRV.
            void NkDX12_FlushUpload() {
                if (!gDX12Registry.uploadCmdList || !gDX12Registry.cmdQueue) return;
                gDX12Registry.uploadCmdList->Close();
                ID3D12CommandList* lists[] = { gDX12Registry.uploadCmdList };
                gDX12Registry.cmdQueue->ExecuteCommandLists(1, lists);
                const uint64 v = ++gDX12Registry.uploadFenceVal;
                gDX12Registry.cmdQueue->Signal(gDX12Registry.uploadFence, v);
                if (gDX12Registry.uploadFence->GetCompletedValue() < v) {
                    gDX12Registry.uploadFence->SetEventOnCompletion(v, gDX12Registry.uploadEvent);
                    ::WaitForSingleObject(gDX12Registry.uploadEvent, INFINITE);
                }
                // Reset pour la prochaine commande
                gDX12Registry.uploadAllocator->Reset();
                gDX12Registry.uploadCmdList->Reset(gDX12Registry.uploadAllocator, nullptr);
            }

            // Recupere l'entree par id (id == index+1, id=0 => nullptr).
            inline NkDX12TextureEntry* NkDX12_GetEntry(uint32 id) {
                if (id == 0) return nullptr;
                const uint32 idx = id - 1;
                if (idx >= gDX12Registry.entries.Size()) return nullptr;
                NkDX12TextureEntry* e = gDX12Registry.entries[idx];
                return (e && e->alive) ? e : nullptr;
            }

        } // anonymous namespace

        // -----------------------------------------------------------------------------
        // NkTextureBackend::Create
        // -----------------------------------------------------------------------------
        uint32 NkDX12Renderer2D::CreateDX12Texture(uint32 w, uint32 h, const uint8* rgba) {
            if (!gDX12Registry.device || w == 0 || h == 0) return 0;
            if (gDX12Registry.nextSrvSlot &&
                *gDX12Registry.nextSrvSlot >= gDX12Registry.srvHeapMaxSlots) {
                NK_DX12_2D_ERR("SRV heap full (max=%u)", gDX12Registry.srvHeapMaxSlots);
                return 0;
            }

            ID3D12Device* dev = gDX12Registry.device;

            // 1) Texture DEFAULT heap (COPY_DEST initial)
            D3D12_HEAP_PROPERTIES hpDef{ D3D12_HEAP_TYPE_DEFAULT };
            D3D12_RESOURCE_DESC td{};
            td.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            td.Width              = w;
            td.Height             = h;
            td.DepthOrArraySize   = 1;
            td.MipLevels          = 1;
            td.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
            td.SampleDesc.Count   = 1;
            td.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            td.Flags              = D3D12_RESOURCE_FLAG_NONE;

            ID3D12Resource* tex = nullptr;
            if (FAILED(dev->CreateCommittedResource(&hpDef, D3D12_HEAP_FLAG_NONE, &td,
                       D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&tex)))) {
                NK_DX12_2D_ERR("CreateCommittedResource(texture) failed");
                return 0;
            }

            // 2) Layout pour buffer d'upload (pitch aligne 256)
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
            UINT   numRows       = 0;
            UINT64 rowSizeBytes  = 0;
            UINT64 totalBytes    = 0;
            dev->GetCopyableFootprints(&td, 0, 1, 0, &footprint, &numRows, &rowSizeBytes, &totalBytes);

            // 3) Upload heap (taille = totalBytes)
            D3D12_HEAP_PROPERTIES hpUp{ D3D12_HEAP_TYPE_UPLOAD };
            D3D12_RESOURCE_DESC ud{};
            ud.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
            ud.Width            = totalBytes;
            ud.Height           = 1;
            ud.DepthOrArraySize = 1;
            ud.MipLevels        = 1;
            ud.SampleDesc.Count = 1;
            ud.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            ID3D12Resource* upload = nullptr;
            if (FAILED(dev->CreateCommittedResource(&hpUp, D3D12_HEAP_FLAG_NONE, &ud,
                       D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload)))) {
                tex->Release();
                NK_DX12_2D_ERR("CreateCommittedResource(upload) failed");
                return 0;
            }

            // 4) Map + memcpy row-by-row avec pitch aligne (si rgba != nullptr)
            if (rgba) {
                void* mapped = nullptr;
                if (SUCCEEDED(upload->Map(0, nullptr, &mapped))) {
                    uint8* dst = (uint8*)mapped + footprint.Offset;
                    const uint32 srcRowBytes = w * 4;
                    const uint32 dstRowPitch = footprint.Footprint.RowPitch;
                    for (uint32 row = 0; row < h; ++row) {
                        memcpy(dst + row * dstRowPitch, rgba + row * srcRowBytes, srcRowBytes);
                    }
                    upload->Unmap(0, nullptr);
                }
            }

            // 5) Copy upload -> tex puis transition COPY_DEST -> PIXEL_SHADER_RESOURCE
            D3D12_TEXTURE_COPY_LOCATION dst{};
            dst.pResource         = tex;
            dst.Type              = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.SubresourceIndex  = 0;

            D3D12_TEXTURE_COPY_LOCATION src{};
            src.pResource         = upload;
            src.Type              = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint   = footprint;

            gDX12Registry.uploadCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource   = tex;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            gDX12Registry.uploadCmdList->ResourceBarrier(1, &barrier);

            // 6) Flush synchronous (fence wait) — sinon premier draw lit du noir
            NkDX12_FlushUpload();

            // 7) Allocation du slot SRV (compteur partage avec le renderer)
            uint32 srvSlot = (*gDX12Registry.nextSrvSlot)++;
            D3D12_SHADER_RESOURCE_VIEW_DESC srvd{};
            srvd.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
            srvd.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvd.Texture2D.MipLevels     = 1;
            dev->CreateShaderResourceView(tex, &srvd, NkDX12_RegCPUHandle(srvSlot));

            // 8) Entree registry (recycle un slot mort si possible)
            NkDX12TextureEntry* entry = nullptr;
            uint32 idx = 0;
            for (; idx < gDX12Registry.entries.Size(); ++idx) {
                if (gDX12Registry.entries[idx] && !gDX12Registry.entries[idx]->alive) {
                    entry = gDX12Registry.entries[idx];
                    break;
                }
            }
            if (!entry) {
                void* mem = nkentseu::memory::NkAlloc(sizeof(NkDX12TextureEntry));
                if (!mem) {
                    tex->Release();
                    upload->Release();
                    NK_DX12_2D_ERR("NkAlloc(NkDX12TextureEntry) failed");
                    return 0;
                }
                entry = new(mem) NkDX12TextureEntry();
                gDX12Registry.entries.PushBack(entry);
                idx = gDX12Registry.entries.Size() - 1;
            }
            entry->resource       = tex;
            entry->uploadHeap     = upload;
            entry->srvIndex       = srvSlot;
            entry->filter         = NkTextureFilter::NK_LINEAR;
            entry->wrap           = NkTextureWrap::NK_CLAMP;
            entry->width          = w;
            entry->height         = h;
            entry->uploadRowPitch = footprint.Footprint.RowPitch;
            entry->alive          = true;

            return idx + 1;  // id != 0
        }

        // -----------------------------------------------------------------------------
        // NkTextureBackend::Update — sub-region upload
        // -----------------------------------------------------------------------------
        void NkDX12Renderer2D::UpdateDX12Texture(uint32 id, uint32 x, uint32 y,
                                                  uint32 w, uint32 h, const uint8* rgba) {
            NkDX12TextureEntry* e = NkDX12_GetEntry(id);
            if (!e || !rgba || w == 0 || h == 0) return;
            if (x + w > e->width || y + h > e->height) {
                NK_DX12_2D_ERR("UpdateDX12Texture: region out of bounds");
                return;
            }

            // Recree un upload buffer dimensionne pour le sous-rect (e->uploadHeap
            // peut etre trop petit ou re-utilise par une autre Update). Plus simple
            // et correct que de reutiliser uploadHeap initial.
            ID3D12Device* dev = gDX12Registry.device;

            D3D12_RESOURCE_DESC td{};
            td.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            td.Width              = w;
            td.Height             = h;
            td.DepthOrArraySize   = 1;
            td.MipLevels          = 1;
            td.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
            td.SampleDesc.Count   = 1;

            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
            UINT   numRows = 0;
            UINT64 rowSize = 0;
            UINT64 total   = 0;
            dev->GetCopyableFootprints(&td, 0, 1, 0, &footprint, &numRows, &rowSize, &total);

            D3D12_HEAP_PROPERTIES hpUp{ D3D12_HEAP_TYPE_UPLOAD };
            D3D12_RESOURCE_DESC ud{};
            ud.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
            ud.Width            = total;
            ud.Height           = 1;
            ud.DepthOrArraySize = 1;
            ud.MipLevels        = 1;
            ud.SampleDesc.Count = 1;
            ud.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            ID3D12Resource* upload = nullptr;
            if (FAILED(dev->CreateCommittedResource(&hpUp, D3D12_HEAP_FLAG_NONE, &ud,
                       D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload)))) {
                NK_DX12_2D_ERR("UpdateDX12Texture: upload alloc failed");
                return;
            }

            void* mapped = nullptr;
            if (SUCCEEDED(upload->Map(0, nullptr, &mapped))) {
                uint8* dst = (uint8*)mapped + footprint.Offset;
                const uint32 srcRowBytes = w * 4;
                const uint32 dstRowPitch = footprint.Footprint.RowPitch;
                for (uint32 row = 0; row < h; ++row) {
                    memcpy(dst + row * dstRowPitch, rgba + row * srcRowBytes, srcRowBytes);
                }
                upload->Unmap(0, nullptr);
            }

            // Transition texture PIXEL_SHADER_RESOURCE -> COPY_DEST
            D3D12_RESOURCE_BARRIER toCopy{};
            toCopy.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            toCopy.Transition.pResource   = e->resource;
            toCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            toCopy.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
            toCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            gDX12Registry.uploadCmdList->ResourceBarrier(1, &toCopy);

            // CopyTextureRegion vers (x, y, 0)
            D3D12_TEXTURE_COPY_LOCATION dst{};
            dst.pResource        = e->resource;
            dst.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.SubresourceIndex = 0;

            D3D12_TEXTURE_COPY_LOCATION src{};
            src.pResource        = upload;
            src.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint  = footprint;

            gDX12Registry.uploadCmdList->CopyTextureRegion(&dst, x, y, 0, &src, nullptr);

            // Transition retour COPY_DEST -> PIXEL_SHADER_RESOURCE
            D3D12_RESOURCE_BARRIER toRead{};
            toRead.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            toRead.Transition.pResource   = e->resource;
            toRead.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            toRead.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            toRead.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            gDX12Registry.uploadCmdList->ResourceBarrier(1, &toRead);

            NkDX12_FlushUpload();

            // upload buffer n'est plus utile apres le flush (fence wait)
            upload->Release();
        }

        // -----------------------------------------------------------------------------
        // NkTextureBackend::Destroy
        // -----------------------------------------------------------------------------
        void NkDX12Renderer2D::DeleteDX12Texture(uint32 id) {
            NkDX12TextureEntry* e = NkDX12_GetEntry(id);
            if (!e) return;
            if (e->resource)   { e->resource->Release();   e->resource = nullptr; }
            if (e->uploadHeap) { e->uploadHeap->Release(); e->uploadHeap = nullptr; }
            // Le slot SRV reste alloue (allocateur croissant simple, pas de free-list).
            e->alive = false;
        }

        // -----------------------------------------------------------------------------
        // NkTextureBackend::SetFilter — NO-OP en DX12
        // -----------------------------------------------------------------------------
        void NkDX12Renderer2D::SetDX12TextureFilter(uint32 id, NkTextureFilter f) {
            // La root signature utilise un sampler unique (LINEAR + CLAMP) lie via
            // mSamplerHeap dans BeginBackend(). Pour supporter du per-texture
            // filter/wrap il faudrait un sampler heap multi-slots + un slot par
            // entree. Non implemente : on stocke la valeur pour information.
            NkDX12TextureEntry* e = NkDX12_GetEntry(id);
            if (e) e->filter = f;
        }

        // -----------------------------------------------------------------------------
        // NkTextureBackend::SetWrap — NO-OP en DX12 (cf. SetFilter)
        // -----------------------------------------------------------------------------
        void NkDX12Renderer2D::SetDX12TextureWrap(uint32 id, NkTextureWrap w) {
            NkDX12TextureEntry* e = NkDX12_GetEntry(id);
            if (e) e->wrap = w;
        }

        // =============================================================================
        bool NkDX12Renderer2D::Initialize(NkIGraphicsContext* ctx) {
            if (mIsValid) return false;
            if (!ctx || ctx->GetApi() != NkGraphicsApi::NK_GFX_API_DX12) {
                NK_DX12_2D_ERR("Requires a DX12 context");
                return false;
            }
            mCtx = ctx;

            NkDX12ContextData* d = NkNativeContext::DX12(ctx);
            if (!d || !d->device) { NK_DX12_2D_ERR("Invalid DX12 context data"); return false; }
            mDevice  = d->device;
            mCmdList = d->cmdList;

            if (!CreateDescriptorHeap()) return false;
            if (!CreateSampler())        return false;
            if (!CreateRootSignature())  return false;
            if (!CreatePSOs())           return false;
            if (!CreateBuffers())        return false;
            // NB : CreateWhiteTexture() est volontairement DIFFEREE apres l'init
            // de la registry d'upload (gDX12Registry.uploadCmdList) ci-dessous —
            // sinon elle se rabattait sur le cmdList du contexte (ferme a l'init)
            // -> "API cannot be called on a closed command list" + corruption ->
            // toutes les BeginFrame::Reset echouaient -> ecran vide.

            NkContextInfo info = ctx->GetInfo();
            const uint32 W = info.windowWidth  > 0 ? info.windowWidth  : 800;
            const uint32 H = info.windowHeight > 0 ? info.windowHeight : 600;
            mDefaultView.center = { W * 0.5f, H * 0.5f };
            mDefaultView.size   = { (float)W, (float)H };
            mCurrentView        = mDefaultView;
            mViewport           = { 0, 0, (int32)W, (int32)H };

            float proj[16];
            mCurrentView.ToProjectionMatrix(proj);
            UploadProjection(proj);

            // ── Enregistrement dispatch table NkTexture ──────────────────────────
            // Capture les handles GPU dans la registry globale (les 5 callbacks
            // sont statiques et n'ont pas acces a `this`). On cree aussi un
            // command allocator + cmdList + fence dedies aux uploads de textures
            // pour ne pas perturber le frame courant du contexte.
            {
                gDX12Registry.device          = mDevice.Get();
                gDX12Registry.cmdQueue        = d->commandQueue.Get();
                gDX12Registry.srvHeap         = mSRVHeap.Get();
                gDX12Registry.srvHeapStride   = mSRVDescSize;
                gDX12Registry.srvHeapMaxSlots = kMaxSRVSlots;
                gDX12Registry.nextSrvSlot     = &mNextSRVSlot;

                ID3D12CommandAllocator*    alloc = nullptr;
                ID3D12GraphicsCommandList* list  = nullptr;
                ID3D12Fence*               fence = nullptr;

                if (FAILED(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                           IID_PPV_ARGS(&alloc)))) {
                    NK_DX12_2D_ERR("Upload allocator failed");
                    return false;
                }
                if (FAILED(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                           alloc, nullptr, IID_PPV_ARGS(&list)))) {
                    alloc->Release();
                    NK_DX12_2D_ERR("Upload cmd list failed");
                    return false;
                }
                // CreateCommandList ouvre la liste -> deja prete a recevoir des cmds
                if (FAILED(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                           IID_PPV_ARGS(&fence)))) {
                    list->Release(); alloc->Release();
                    NK_DX12_2D_ERR("Upload fence failed");
                    return false;
                }
                HANDLE evt = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
                if (!evt) {
                    fence->Release(); list->Release(); alloc->Release();
                    NK_DX12_2D_ERR("Upload event failed");
                    return false;
                }

                gDX12Registry.uploadAllocator = alloc;
                gDX12Registry.uploadCmdList   = list;
                gDX12Registry.uploadFence     = fence;
                gDX12Registry.uploadEvent     = evt;
                gDX12Registry.uploadFenceVal  = 0;

                NkTextureBackend backend{};
                backend.Create    = &NkDX12Renderer2D::CreateDX12Texture;
                backend.Update    = &NkDX12Renderer2D::UpdateDX12Texture;
                backend.Destroy   = &NkDX12Renderer2D::DeleteDX12Texture;
                backend.SetFilter = &NkDX12Renderer2D::SetDX12TextureFilter;
                backend.SetWrap   = &NkDX12Renderer2D::SetDX12TextureWrap;
                NkTextureSetBackend(backend);
            }

            // Texture blanche 1x1 (default des quads non textures). DOIT etre
            // creee APRES la registry d'upload ci-dessus : elle passe par la
            // command list d'upload dediee (gDX12Registry.uploadCmdList), pas par
            // le cmdList du contexte (ferme a l'init).
            if (!CreateWhiteTexture())   return false;

            // NkShader sur DX12 : compile HLSL + reconstruction du PSO (root
            // signature partagee, mais pipeline state inclut les shaders).
            // Implementation differee — stub installe pour preserver la
            // consistance API.
            NkShaderInstallUnsupportedBackend("DX12");
            NkRenderTextureInstallUnsupportedBackend("DX12");

            mIsValid = true;
            NK_DX12_2D_LOG("Initialized");
            return true;
        }

        // =============================================================================
        bool NkDX12Renderer2D::CreateDescriptorHeap() {
            // SRV heap (shader visible)
            D3D12_DESCRIPTOR_HEAP_DESC hd{};
            hd.NumDescriptors = kMaxSRVSlots;
            hd.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            hd.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            NK_DX12_2D_CHECK(mDevice->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&mSRVHeap)), "CreateSRVHeap");
            mSRVDescSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            // Sampler heap (shader visible, 1 slot)
            D3D12_DESCRIPTOR_HEAP_DESC sd{};
            sd.NumDescriptors = 1;
            sd.Type  = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            sd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            NK_DX12_2D_CHECK(mDevice->CreateDescriptorHeap(&sd, IID_PPV_ARGS(&mSamplerHeap)), "CreateSamplerHeap");

            return true;
        }

        // =============================================================================
        bool NkDX12Renderer2D::CreateSampler() {
            D3D12_SAMPLER_DESC sd{};
            sd.Filter   = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            sd.AddressU = sd.AddressV = sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sd.MaxLOD   = D3D12_FLOAT32_MAX;
            mDevice->CreateSampler(&sd, mSamplerHeap->GetCPUDescriptorHandleForHeapStart());
            return true;
        }

        // =============================================================================
        bool NkDX12Renderer2D::CreateRootSignature() {
            // Slot 0: root constants (16 × float = proj)
            // Slot 1: descriptor table → SRV range (t0)
            // Slot 2: descriptor table → sampler range (s0)
            D3D12_ROOT_PARAMETER params[3]{};

            params[0].ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            params[0].Constants.ShaderRegister = 0;
            params[0].Constants.RegisterSpace  = 0;
            params[0].Constants.Num32BitValues = 16;
            params[0].ShaderVisibility         = D3D12_SHADER_VISIBILITY_VERTEX;

            D3D12_DESCRIPTOR_RANGE srvRange{};
            srvRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            srvRange.NumDescriptors                    = 1;
            srvRange.BaseShaderRegister                = 0;
            srvRange.RegisterSpace                     = 0;
            srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
            params[1].ParameterType                    = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            params[1].DescriptorTable.NumDescriptorRanges = 1;
            params[1].DescriptorTable.pDescriptorRanges   = &srvRange;
            params[1].ShaderVisibility                 = D3D12_SHADER_VISIBILITY_PIXEL;

            D3D12_DESCRIPTOR_RANGE samplerRange{};
            samplerRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
            samplerRange.NumDescriptors                    = 1;
            samplerRange.BaseShaderRegister                = 0;
            samplerRange.RegisterSpace                     = 0;
            samplerRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
            params[2].ParameterType                        = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            params[2].DescriptorTable.NumDescriptorRanges  = 1;
            params[2].DescriptorTable.pDescriptorRanges    = &samplerRange;
            params[2].ShaderVisibility                     = D3D12_SHADER_VISIBILITY_PIXEL;

            D3D12_ROOT_SIGNATURE_DESC rsDesc{};
            rsDesc.NumParameters = 3;
            rsDesc.pParameters   = params;
            rsDesc.Flags         = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

            ComPtr<ID3DBlob> blob, errBlob;
            HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errBlob);
            if (FAILED(hr)) {
                NK_DX12_2D_ERR("Serialize root sig: %s",
                    errBlob ? (char*)errBlob->GetBufferPointer() : "?");
                return false;
            }
            NK_DX12_2D_CHECK(mDevice->CreateRootSignature(0,
                blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&mRootSig)),
                "CreateRootSig");
            return true;
        }

        // =============================================================================
        bool NkDX12Renderer2D::MakePSO(D3D12_BLEND_DESC blendDesc,
                                        ComPtr<ID3D12PipelineState>& out,
                                        ID3DBlob* vsBlob, ID3DBlob* psBlob) {
            D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,      0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,      0,  8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                {"COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,    0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            };

            D3D12_RASTERIZER_DESC rast{};
            rast.FillMode = D3D12_FILL_MODE_SOLID;
            rast.CullMode = D3D12_CULL_MODE_NONE;
            rast.DepthClipEnable = FALSE;

            D3D12_DEPTH_STENCIL_DESC dss{};
            dss.DepthEnable    = FALSE;
            dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
            psoDesc.pRootSignature        = mRootSig.Get();
            psoDesc.VS                    = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
            psoDesc.PS                    = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
            psoDesc.BlendState            = blendDesc;
            psoDesc.SampleMask            = UINT_MAX;
            psoDesc.RasterizerState       = rast;
            psoDesc.DepthStencilState     = dss;
            psoDesc.InputLayout           = { inputLayout, 3 };
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            psoDesc.NumRenderTargets      = 1;
            psoDesc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
            psoDesc.DSVFormat             = DXGI_FORMAT_UNKNOWN;
            psoDesc.SampleDesc.Count      = 1;

            return SUCCEEDED(mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&out)));
        }

        // =============================================================================
        bool NkDX12Renderer2D::CreatePSOs() {
            ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;
            HRESULT hr = D3DCompile(kDX12_2D_HLSL, strlen(kDX12_2D_HLSL),
                                    nullptr, nullptr, nullptr, "VS", "vs_5_1", 0, 0, &vsBlob, &errBlob);
            if (FAILED(hr)) { NK_DX12_2D_ERR("VS: %s", errBlob ? (char*)errBlob->GetBufferPointer() : "?"); return false; }
            errBlob.Reset();
            hr = D3DCompile(kDX12_2D_HLSL, strlen(kDX12_2D_HLSL),
                            nullptr, nullptr, nullptr, "PS", "ps_5_1", 0, 0, &psBlob, &errBlob);
            if (FAILED(hr)) { NK_DX12_2D_ERR("PS: %s", errBlob ? (char*)errBlob->GetBufferPointer() : "?"); return false; }

            // Helper to build a D3D12_BLEND_DESC for one render target
            auto MakeBlendDesc = [](D3D12_BLEND src, D3D12_BLEND dst,
                                    D3D12_BLEND srcA, D3D12_BLEND dstA,
                                    bool enable) -> D3D12_BLEND_DESC {
                D3D12_BLEND_DESC bd{};
                bd.RenderTarget[0].BlendEnable           = enable ? TRUE : FALSE;
                bd.RenderTarget[0].SrcBlend              = src;
                bd.RenderTarget[0].DestBlend             = dst;
                bd.RenderTarget[0].BlendOp               = D3D12_BLEND_OP_ADD;
                bd.RenderTarget[0].SrcBlendAlpha         = srcA;
                bd.RenderTarget[0].DestBlendAlpha        = dstA;
                bd.RenderTarget[0].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
                bd.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
                return bd;
            };

            bool ok = true;
            ok &= MakePSO(MakeBlendDesc(D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA,
                                        D3D12_BLEND_ONE, D3D12_BLEND_INV_SRC_ALPHA, true),
                        mPSOAlpha, vsBlob.Get(), psBlob.Get());
            ok &= MakePSO(MakeBlendDesc(D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_ONE,
                                        D3D12_BLEND_ONE, D3D12_BLEND_ONE, true),
                        mPSOAdd, vsBlob.Get(), psBlob.Get());
            ok &= MakePSO(MakeBlendDesc(D3D12_BLEND_DEST_COLOR, D3D12_BLEND_ZERO,
                                        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, true),
                        mPSOMul, vsBlob.Get(), psBlob.Get());
            ok &= MakePSO(MakeBlendDesc(D3D12_BLEND_ONE, D3D12_BLEND_ZERO,
                                        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, false),
                        mPSONone, vsBlob.Get(), psBlob.Get());
            return ok;
        }

        // =============================================================================
        bool NkDX12Renderer2D::CreateBuffers() {
            const UINT64 vbSize = (UINT64)kMaxVertices * sizeof(NkVertex2D);
            const UINT64 ibSize = (UINT64)kMaxIndices  * sizeof(uint32);

            D3D12_HEAP_PROPERTIES hp{ D3D12_HEAP_TYPE_UPLOAD };
            auto MakeBuf = [&](UINT64 sz, ComPtr<ID3D12Resource>& out) -> bool {
                D3D12_RESOURCE_DESC rd{};
                rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
                rd.Width     = sz; rd.Height = rd.DepthOrArraySize = rd.MipLevels = 1;
                rd.SampleDesc = {1,0};
                rd.Layout    = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
                rd.Flags     = D3D12_RESOURCE_FLAG_NONE;
                return SUCCEEDED(mDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE,
                    &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&out)));
            };

            if (!MakeBuf(vbSize, mVB)) { NK_DX12_2D_ERR("CreateVB"); return false; }
            if (!MakeBuf(ibSize, mIB)) { NK_DX12_2D_ERR("CreateIB"); return false; }

            mVB->Map(0, nullptr, &mVBMap);
            mIB->Map(0, nullptr, &mIBMap);

            mVBView.BufferLocation = mVB->GetGPUVirtualAddress();
            mVBView.SizeInBytes    = (UINT)vbSize;
            mVBView.StrideInBytes  = (UINT)sizeof(NkVertex2D);

            mIBView.BufferLocation = mIB->GetGPUVirtualAddress();
            mIBView.SizeInBytes    = (UINT)ibSize;
            mIBView.Format         = DXGI_FORMAT_R32_UINT;

            return true;
        }

        // =============================================================================
        bool NkDX12Renderer2D::CreateWhiteTexture() {
            // 1×1 white texture in upload heap (static, never updated)
            D3D12_HEAP_PROPERTIES hp{ D3D12_HEAP_TYPE_DEFAULT };
            D3D12_RESOURCE_DESC rd{};
            rd.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            rd.Width = rd.Height  = 1;
            rd.DepthOrArraySize   = rd.MipLevels = 1;
            rd.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
            rd.SampleDesc.Count   = 1;
            rd.Flags              = D3D12_RESOURCE_FLAG_NONE;
            NK_DX12_2D_CHECK(mDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE,
                &rd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mWhiteTex)),
                "CreateWhiteTex");

            // Upload via staging
            D3D12_HEAP_PROPERTIES uhp{ D3D12_HEAP_TYPE_UPLOAD };
            D3D12_RESOURCE_DESC urd{};
            urd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            // RowPitch d'une copie buffer->texture DOIT etre multiple de 256
            // (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT). Le staging fait donc 256 octets
            // (on n'ecrit que les 4 premiers = le pixel blanc).
            urd.Width = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
            urd.Height = urd.DepthOrArraySize = urd.MipLevels = 1;
            urd.SampleDesc = {1,0}; urd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            ComPtr<ID3D12Resource> staging;
            mDevice->CreateCommittedResource(&uhp, D3D12_HEAP_FLAG_NONE, &urd,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&staging));

            const uint32 white = 0xFFFFFFFFu;
            void* mapped = nullptr;
            staging->Map(0, nullptr, &mapped);
            memcpy(mapped, &white, 4);
            staging->Unmap(0, nullptr);

            D3D12_TEXTURE_COPY_LOCATION dst = {};
            dst.pResource = mWhiteTex.Get();
            dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.SubresourceIndex = 0;

            D3D12_TEXTURE_COPY_LOCATION src = {};
            src.pResource = staging.Get();
            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint.Offset = 0;
            src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            src.PlacedFootprint.Footprint.Width = 1;
            src.PlacedFootprint.Footprint.Height = 1;
            src.PlacedFootprint.Footprint.Depth = 1;
            src.PlacedFootprint.Footprint.RowPitch = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT; // 256, requis

            // One-shot copy via la command list d'upload DEDIEE (ouverte), PAS le
            // cmdList du contexte (ferme a l'init -> "API cannot be called on a
            // closed command list" + corruption de la liste). NkDX12_FlushUpload()
            // Close+Execute+attend la fence : le `staging` local reste vivant
            // jusqu'au retour de fonction (donc apres la completion GPU).
            gDX12Registry.uploadCmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            {
                D3D12_RESOURCE_BARRIER b{};
                b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                b.Transition.pResource   = mWhiteTex.Get();
                b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                b.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                gDX12Registry.uploadCmdList->ResourceBarrier(1, &b);
            }
            NkDX12_FlushUpload();   // Close + Execute + attend la fence GPU

            // Create SRV
            mWhiteSRVSlot = mNextSRVSlot++;
            D3D12_SHADER_RESOURCE_VIEW_DESC srvd{};
            srvd.Format                    = DXGI_FORMAT_R8G8B8A8_UNORM;
            srvd.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvd.Shader4ComponentMapping   = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvd.Texture2D.MipLevels       = 1;
            mDevice->CreateShaderResourceView(mWhiteTex.Get(), &srvd, GetCPUHandle(mWhiteSRVSlot));

            return true;
        }

        // =============================================================================
        void NkDX12Renderer2D::TransitionResource(ID3D12GraphicsCommandList4* cmd,
                                                ID3D12Resource* res,
                                                D3D12_RESOURCE_STATES from,
                                                D3D12_RESOURCE_STATES to) {
            D3D12_RESOURCE_BARRIER b{};
            b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b.Transition.pResource   = res;
            b.Transition.StateBefore = from;
            b.Transition.StateAfter  = to;
            b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            cmd->ResourceBarrier(1, &b);
        }

        // =============================================================================
        D3D12_GPU_DESCRIPTOR_HANDLE NkDX12Renderer2D::GetGPUHandle(uint32 slot) const {
            D3D12_GPU_DESCRIPTOR_HANDLE h = mSRVHeap->GetGPUDescriptorHandleForHeapStart();
            h.ptr += (uint64)slot * mSRVDescSize;
            return h;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE NkDX12Renderer2D::GetCPUHandle(uint32 slot) const {
            D3D12_CPU_DESCRIPTOR_HANDLE h = mSRVHeap->GetCPUDescriptorHandleForHeapStart();
            h.ptr += (uint64)slot * mSRVDescSize;
            return h;
        }

        // =============================================================================
        uint32 NkDX12Renderer2D::GetOrCreateSRVSlot(const NkTexture* tex) {
            if (!tex || !tex->IsValid()) return mWhiteSRVSlot;
            // Avec le dispatch NkTextureBackend, le slot SRV a deja ete cree
            // par CreateDX12Texture (registry globale, cf. plus haut). tex->GetGPUId()
            // est l'id renvoye par Create == index+1 dans gDX12Registry.entries.
            const uint32 id = tex->GetGPUId();
            if (id != 0 && id - 1 < gDX12Registry.entries.Size()) {
                NkDX12TextureEntry* e = gDX12Registry.entries[id - 1];
                if (e && e->alive) return e->srvIndex;
            }
            // Compat : si la texture a ete remplie en direct par un autre chemin
            // (mHandle = ID3D12Resource*), on alloue un SRV a la volee. Conserve
            // pour eviter de casser d'eventuels usages externes.
            for (const auto& cached : mTexSRVCache) {
                if (cached.texture == tex) return cached.slot;
            }
            if (mNextSRVSlot >= kMaxSRVSlots) return mWhiteSRVSlot;
            ID3D12Resource* res = static_cast<ID3D12Resource*>(tex->GetHandle());
            if (!res) return mWhiteSRVSlot;

            uint32 slot = mNextSRVSlot++;
            D3D12_SHADER_RESOURCE_VIEW_DESC srvd{};
            srvd.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
            srvd.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvd.Texture2D.MipLevels     = 1;
            mDevice->CreateShaderResourceView(res, &srvd, GetCPUHandle(slot));

            TexSRVEntry e{ tex, slot };
            mTexSRVCache.PushBack(e);
            return slot;
        }

        // =============================================================================
        void NkDX12Renderer2D::Shutdown() {
            if (!mIsValid) return;
            if (mVBMap) { mVB->Unmap(0, nullptr); mVBMap = nullptr; }
            if (mIBMap) { mIB->Unmap(0, nullptr); mIBMap = nullptr; }

            // Vider la registry NkTexture : release des ressources, des objets
            // helper d'upload et desenregistrement du dispatch.
            {
                // Reset dispatch a "vide" : les Destroy ulterieurs de NkTexture
                // sont alors no-op (callbacks null).
                NkTextureBackend empty{};
                NkTextureSetBackend(empty);

                for (uint32 i = 0; i < gDX12Registry.entries.Size(); ++i) {
                    NkDX12TextureEntry* e = gDX12Registry.entries[i];
                    if (!e) continue;
                    if (e->resource)   e->resource->Release();
                    if (e->uploadHeap) e->uploadHeap->Release();
                    e->~NkDX12TextureEntry();
                    nkentseu::memory::NkFree(e);
                }
                gDX12Registry.entries.Clear();

                if (gDX12Registry.uploadCmdList)  { gDX12Registry.uploadCmdList->Release();  gDX12Registry.uploadCmdList  = nullptr; }
                if (gDX12Registry.uploadAllocator){ gDX12Registry.uploadAllocator->Release();gDX12Registry.uploadAllocator= nullptr; }
                if (gDX12Registry.uploadFence)    { gDX12Registry.uploadFence->Release();    gDX12Registry.uploadFence    = nullptr; }
                if (gDX12Registry.uploadEvent)    { ::CloseHandle(gDX12Registry.uploadEvent);gDX12Registry.uploadEvent    = nullptr; }

                gDX12Registry.device          = nullptr;
                gDX12Registry.cmdQueue        = nullptr;
                gDX12Registry.srvHeap         = nullptr;
                gDX12Registry.srvHeapStride   = 0;
                gDX12Registry.srvHeapMaxSlots = 0;
                gDX12Registry.nextSrvSlot     = nullptr;
                gDX12Registry.uploadFenceVal  = 0;
            }

            mIsValid = false;
            NK_DX12_2D_LOG("Shutdown");
        }

        // =============================================================================
        void NkDX12Renderer2D::Clear(const NkColor2D& col) {
            NkDX12ContextData* d = NkNativeContext::DX12(mCtx);
            if (!d || !d->cmdList) return;
            math::NkColorF cf = col.ToColorF();
            float fc[4] = { cf.r, cf.g, cf.b, cf.a };
            const uint32 frame = d->currentBackBuffer;
            d->cmdList->ClearRenderTargetView(d->rtvHandles[frame], fc, 0, nullptr);
        }

        // =============================================================================
        void NkDX12Renderer2D::BeginBackend() {
            NkDX12ContextData* d = NkNativeContext::DX12(mCtx);
            if (!d || !d->cmdList) return;

            // Demarre la frame cote CONTEXTE : WaitForFence + Reset allocator/cmdList
            // + barrier PRESENT->RENDER_TARGET + bind RTV/DSV + clear defaut + viewport.
            // INDISPENSABLE en DX12 : sans ca le back buffer reste en etat PRESENT et
            // le cmdList n'est jamais reset -> les drawcalls sont invisibles. (Le
            // EndFrame = barrier RENDER_TARGET->PRESENT + Close + Execute est appele
            // dans EndBackend ; le Present() par NkRenderWindow::Display.)
            if (mCtx) mCtx->BeginFrame();

            mCmdList = d->cmdList;

            // Le PSO 2D a DSVFormat=UNKNOWN (pas de depth). BeginFrame() a bind
            // RTV+DSV ; on REBIND la RTV SEULE (DSV null) pour matcher le PSO. Sinon
            // mismatch RTV/DSV au moment du draw -> cmdList en erreur -> Close echoue.
            mCmdList->OMSetRenderTargets(1, &d->rtvHandles[d->currentBackBuffer], FALSE, nullptr);

            ID3D12DescriptorHeap* heaps[] = { mSRVHeap.Get(), mSamplerHeap.Get() };
            mCmdList->SetDescriptorHeaps(2, heaps);
            mCmdList->SetGraphicsRootSignature(mRootSig.Get());
            mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            mCmdList->IASetVertexBuffers(0, 1, &mVBView);
            mCmdList->IASetIndexBuffer(&mIBView);
            // Sampler: always the same
            mCmdList->SetGraphicsRootDescriptorTable(2,
                mSamplerHeap->GetGPUDescriptorHandleForHeapStart());
        }

        // =============================================================================
        void NkDX12Renderer2D::EndBackend() {
            // Termine la frame cote contexte : barrier RENDER_TARGET->PRESENT + Close
            // du cmdList + ExecuteCommandLists. Le Present() est ensuite appele par
            // NkRenderWindow::Display via context->Present().
            if (mCtx) mCtx->EndFrame();
        }

        // =============================================================================
        void NkDX12Renderer2D::SubmitBatches(const NkBatchGroup* groups, uint32 groupCount,
                                            const NkVertex2D* verts, uint32 vCount,
                                            const uint32*     idx,   uint32 iCount) {
            if (!mIsValid || !mCmdList || !vCount || !iCount) return;

            memcpy(mVBMap, verts, vCount * sizeof(NkVertex2D));
            memcpy(mIBMap, idx,   iCount * sizeof(uint32));

            // Viewport + scissor
            D3D12_VIEWPORT vp{
                (float)mViewport.left, (float)mViewport.top,
                (float)mViewport.width, (float)mViewport.height,
                0.f, 1.f
            };
            // Scissor : plein viewport par defaut ; si un clip est actif, on le
            // restreint (intersection avec le viewport — D3D12_RECT {l,t,r,b},
            // origine haut-gauche, donc pas de flip Y).
            D3D12_RECT scissor{ mViewport.left, mViewport.top,
                                mViewport.left + mViewport.width,
                                mViewport.top  + mViewport.height };
            if (mHasClip) {
                const int32 vx1 = mViewport.left + mViewport.width;
                const int32 vy1 = mViewport.top  + mViewport.height;
                int32 l = mClipRect.x > mViewport.left ? mClipRect.x : mViewport.left;
                int32 t = mClipRect.y > mViewport.top  ? mClipRect.y : mViewport.top;
                int32 r = (mClipRect.x + mClipRect.width)  < vx1 ? (mClipRect.x + mClipRect.width)  : vx1;
                int32 b = (mClipRect.y + mClipRect.height) < vy1 ? (mClipRect.y + mClipRect.height) : vy1;
                if (r < l) r = l;
                if (b < t) b = t;
                scissor.left = l; scissor.top = t; scissor.right = r; scissor.bottom = b;
            }
            mCmdList->RSSetViewports(1, &vp);
            mCmdList->RSSetScissorRects(1, &scissor);

            // Root constants: projection (16 × float)
            mCmdList->SetGraphicsRoot32BitConstants(0, 16, mProjection, 0);

            ID3D12PipelineState* currentPSO = nullptr;
            for (uint32 g = 0; g < groupCount; ++g) {
                const auto& group = groups[g];

                ID3D12PipelineState* pso = nullptr;
                switch (group.blendMode) {
                    case NkBlendMode::NK_ADD:      pso = mPSOAdd.Get();   break;
                    case NkBlendMode::NK_MULTIPLY: pso = mPSOMul.Get();   break;
                    case NkBlendMode::NK_NONE:     pso = mPSONone.Get();  break;
                    default:                    pso = mPSOAlpha.Get(); break;
                }
                if (pso != currentPSO) {
                    mCmdList->SetPipelineState(pso);
                    currentPSO = pso;
                }

                uint32 srvSlot = GetOrCreateSRVSlot(group.texture);
                mCmdList->SetGraphicsRootDescriptorTable(1, GetGPUHandle(srvSlot));
                mCmdList->DrawIndexedInstanced(group.indexCount, 1, group.indexStart, 0, 0);
            }
        }

        // =============================================================================
        void NkDX12Renderer2D::UploadProjection(const float32 proj[16]) {
            memcpy(mProjection, proj, 64);
        }

    } // namespace renderer
} // namespace nkentseu
#endif // WINDOWS