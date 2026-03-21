// =============================================================================
// NkDX12ComputeContext.cpp â€” DX12 Compute
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"
#include "NKLogger/NkLog.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#include "NkDX12ComputeContext.h"
#include "NKContainers/NkContainers.h"
#include <d3dcompiler.h>
#include <cstdio>

#if defined(MemoryBarrier)
#undef MemoryBarrier
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define NK_DX12C_LOG(...) logger.Infof("[NkDX12Compute] " __VA_ARGS__)
#define NK_DX12C_ERR(...) logger.Errorf("[NkDX12Compute] " __VA_ARGS__)
#define NK_DX12C_CHECK(hr,msg) do{if(FAILED(hr)){NK_DX12C_ERR(msg" 0x%08X\n",(unsigned)(hr));return;}}while(0)
#define NK_DX12C_CHECKB(hr,msg) do{if(FAILED(hr)){NK_DX12C_ERR(msg" 0x%08X\n",(unsigned)(hr));return{};}}while(0)

// Nombre max de UAV bindables (root signature simplifiÃ©e)
static constexpr UINT kMaxUAV = 8;

namespace nkentseu {

static bool ReadTextFile(const char* path, NkVector<char>& outText) {
    if (!path || !*path) {
        return false;
    }

    FILE* file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }

    const long fileSize = ftell(file);
    if (fileSize < 0) {
        fclose(file);
        return false;
    }
    rewind(file);

    outText.Resize((NkVector<char>::SizeType)fileSize + 1, '\0');
    const size_t readBytes = fread(outText.Data(), 1, (size_t)fileSize, file);
    fclose(file);
    if (readBytes != (size_t)fileSize) {
        outText.Clear();
        return false;
    }
    outText[(NkVector<char>::SizeType)fileSize] = '\0';
    return true;
}

NkDX12ComputeContext::~NkDX12ComputeContext() { if(mIsValid) Shutdown(); }

void NkDX12ComputeContext::InitFromGraphicsContext(NkIGraphicsContext* gfx) {
    if (!gfx || !gfx->IsValid()) return;
    auto* d = static_cast<NkDX12ContextData*>(gfx->GetNativeContextData());
    if (!d) return;

    mDevice       = d->device;
    mComputeQueue = d->computeCommandQueue ? d->computeCommandQueue : d->commandQueue;

    // Command objects dÃ©diÃ©s compute
    mDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&mAllocator));
    mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE,
        mAllocator.Get(), nullptr, IID_PPV_ARGS(&mCmdList));
    mCmdList->Close();

    // Fence
    mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence));
    mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    mFenceValue = 0;

    // Root signature simplifiÃ©e : kMaxUAV UAV (unbounded table)
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    range.NumDescriptors     = kMaxUAV;
    range.BaseShaderRegister = 0;
    range.RegisterSpace      = 0;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER param{};
    param.ParameterType    = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param.DescriptorTable.NumDescriptorRanges = 1;
    param.DescriptorTable.pDescriptorRanges   = &range;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.NumParameters = 1;
    rsDesc.pParameters   = &param;
    rsDesc.Flags         = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> blob, err;
    D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err);
    mDevice->CreateRootSignature(0, blob->GetBufferPointer(),
                                  blob->GetBufferSize(), IID_PPV_ARGS(&mRootSignature));

    mIsValid = true;
    NK_DX12C_LOG("Initialized from graphics context\n");
}

bool NkDX12ComputeContext::Init(const NkContextDesc&) {
    NK_DX12C_ERR("Standalone: use InitFromGraphicsContext\n"); return false;
}

void NkDX12ComputeContext::Shutdown() {
    WaitIdle();
    if (mFenceEvent) { CloseHandle(mFenceEvent); mFenceEvent = nullptr; }
    mRootSignature.Reset();
    mFence.Reset();
    mCmdList.Reset();
    mAllocator.Reset();
    mComputeQueue.Reset();
    mDevice.Reset();
    mIsValid = false;
}

bool NkDX12ComputeContext::IsValid() const { return mIsValid; }

// â”€â”€ Buffers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
struct DX12BufSet {
    ComPtr<ID3D12Resource> gpu;     // UPLOAD ou DEFAULT
    ComPtr<ID3D12Resource> readback;// Readback heap pour CPU read
    uint64 sizeBytes = 0;
    bool   cpuRead   = false;
};

NkComputeBuffer NkDX12ComputeContext::CreateBuffer(const NkComputeBufferDesc& desc) {
    NkComputeBuffer out;
    if (!mIsValid || desc.sizeBytes == 0) return out;

    auto* bs = new DX12BufSet();
    bs->sizeBytes = desc.sizeBytes;
    bs->cpuRead   = desc.cpuReadable;

    // Heap UPLOAD (CPU writable)
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = desc.cpuWritable ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width     = desc.sizeBytes;
    rd.Height    = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
    rd.SampleDesc = {1,0};
    rd.Layout    = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    rd.Flags     = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    if (hp.Type == D3D12_HEAP_TYPE_UPLOAD)
        rd.Flags = D3D12_RESOURCE_FLAG_NONE; // UPLOAD ne supporte pas UAV

    D3D12_RESOURCE_STATES initState = (hp.Type == D3D12_HEAP_TYPE_UPLOAD)
        ? D3D12_RESOURCE_STATE_GENERIC_READ
        : D3D12_RESOURCE_STATE_COMMON;

    HRESULT hr = mDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE,
        &rd, initState, nullptr, IID_PPV_ARGS(&bs->gpu));
    if (FAILED(hr)) { NK_DX12C_ERR("CreateBuffer failed\n"); delete bs; return out; }

    if (desc.cpuReadable) {
        D3D12_HEAP_PROPERTIES rhp{D3D12_HEAP_TYPE_READBACK};
        D3D12_RESOURCE_DESC rrd = rd;
        rrd.Flags = D3D12_RESOURCE_FLAG_NONE;
        mDevice->CreateCommittedResource(&rhp, D3D12_HEAP_FLAG_NONE,
            &rrd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&bs->readback));
    }

    if (desc.initialData && desc.cpuWritable) {
        void* mapped = nullptr;
        bs->gpu->Map(0, nullptr, &mapped);
        if (mapped) {
            memcpy(mapped, desc.initialData, (size_t)desc.sizeBytes);
            bs->gpu->Unmap(0, nullptr);
        }
    }

    out.handle    = bs;
    out.sizeBytes = desc.sizeBytes;
    out.valid     = true;
    return out;
}

void NkDX12ComputeContext::DestroyBuffer(NkComputeBuffer& buf) {
    if (!buf.valid) return;
    delete static_cast<DX12BufSet*>(buf.handle);
    buf = NkComputeBuffer{};
}

bool NkDX12ComputeContext::WriteBuffer(NkComputeBuffer& buf,
    const void* data, uint64 bytes, uint64 offset) {
    if (!buf.valid || !data) return false;
    auto* bs = static_cast<DX12BufSet*>(buf.handle);
    void* mapped = nullptr;
    D3D12_RANGE range{(SIZE_T)offset, (SIZE_T)(offset+bytes)};
    if (FAILED(bs->gpu->Map(0, &range, &mapped))) return false;
    memcpy((uint8*)mapped + offset, data, (size_t)bytes);
    bs->gpu->Unmap(0, &range);
    return true;
}

bool NkDX12ComputeContext::ReadBuffer(const NkComputeBuffer& buf,
    void* outData, uint64 bytes, uint64 offset) {
    if (!buf.valid || !outData) return false;
    auto* bs = static_cast<DX12BufSet*>(buf.handle);
    if (!bs->readback) return false;

    // Copy GPU â†’ readback via command list
    mAllocator->Reset();
    mCmdList->Reset(mAllocator.Get(), nullptr);
    mCmdList->CopyResource(bs->readback.Get(), bs->gpu.Get());
    mCmdList->Close();
    ID3D12CommandList* lists[] = {mCmdList.Get()};
    mComputeQueue->ExecuteCommandLists(1, lists);
    WaitIdle();

    void* mapped = nullptr;
    D3D12_RANGE readRange{(SIZE_T)offset, (SIZE_T)(offset+bytes)};
    if (FAILED(bs->readback->Map(0, &readRange, &mapped))) return false;
    memcpy(outData, (uint8*)mapped + offset, (size_t)bytes);
    D3D12_RANGE noWrite{0,0};
    bs->readback->Unmap(0, &noWrite);
    return true;
}

// â”€â”€ Shaders HLSL â†’ DXIL â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
struct DX12ShaderBlob { ComPtr<ID3DBlob> blob; };

NkComputeShader NkDX12ComputeContext::CreateShaderFromSource(
    const char* src, const char* entry) {
    NkComputeShader s;
    ComPtr<ID3DBlob> blob, err;
    HRESULT hr = D3DCompile(src, strlen(src), nullptr, nullptr, nullptr,
        entry, "cs_5_1", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &err);
    if (FAILED(hr)) {
        NK_DX12C_ERR("HLSL compile: %s\n",
            err ? (char*)err->GetBufferPointer() : "unknown");
        return s;
    }
    auto* b = new DX12ShaderBlob(); b->blob = blob;
    s.handle = b; s.valid = true;
    return s;
}

NkComputeShader NkDX12ComputeContext::CreateShaderFromFile(
    const char* path, const char* entry) {
    NkVector<char> src;
    if (!ReadTextFile(path, src)) {
        NK_DX12C_ERR("Cannot open: %s\n", path);
        return {};
    }
    return CreateShaderFromSource(src.Data(), entry);
}

void NkDX12ComputeContext::DestroyShader(NkComputeShader& s) {
    if (!s.valid) return;
    delete static_cast<DX12ShaderBlob*>(s.handle);
    s = NkComputeShader{};
}

// â”€â”€ Pipeline â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
NkComputePipeline NkDX12ComputeContext::CreatePipeline(const NkComputeShader& s) {
    NkComputePipeline p;
    if (!s.valid) return p;
    auto* b = static_cast<DX12ShaderBlob*>(s.handle);

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.CS             = {b->blob->GetBufferPointer(), b->blob->GetBufferSize()};
    ComPtr<ID3D12PipelineState> pso;
    if (FAILED(mDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pso)))) {
        NK_DX12C_ERR("CreateComputePipelineState failed\n"); return p;
    }
    p.handle = pso.Detach();
    p.valid  = true;
    return p;
}

void NkDX12ComputeContext::DestroyPipeline(NkComputePipeline& p) {
    if (!p.valid) return;
    static_cast<ID3D12PipelineState*>(p.handle)->Release();
    p = NkComputePipeline{};
}

void NkDX12ComputeContext::BindPipeline(const NkComputePipeline& p) {
    if (!p.valid) return;
    // StockÃ© pour Dispatch
    (void)p;
}

void NkDX12ComputeContext::BindBuffer(uint32 /*slot*/, NkComputeBuffer& /*buf*/) {
    // En DX12, les UAV sont bindÃ©s via descriptor heap
    // ImplÃ©mentation complÃ¨te nÃ©cessite un SRV/UAV heap compute
}

void NkDX12ComputeContext::Dispatch(uint32 gx, uint32 gy, uint32 gz) {
    mAllocator->Reset();
    mCmdList->Reset(mAllocator.Get(), nullptr);
    mCmdList->SetComputeRootSignature(mRootSignature.Get());
    mCmdList->Dispatch(gx, gy, gz);
    mCmdList->Close();
    ID3D12CommandList* lists[] = {mCmdList.Get()};
    mComputeQueue->ExecuteCommandLists(1, lists);
    // Signal fence
    ++mFenceValue;
    mComputeQueue->Signal(mFence.Get(), mFenceValue);
}

void NkDX12ComputeContext::WaitIdle() {
    if (!mIsValid || !mFenceEvent) return;
    if (mFence->GetCompletedValue() < mFenceValue) {
        mFence->SetEventOnCompletion(mFenceValue, mFenceEvent);
        WaitForSingleObjectEx(mFenceEvent, INFINITE, FALSE);
    }
}

void NkDX12ComputeContext::MemoryBarrier() {
    // UAV barrier via ResourceBarrier dans le command list
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type  = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.UAV.pResource = nullptr; // all resources
    mCmdList->ResourceBarrier(1, &barrier);
}

NkGraphicsApi NkDX12ComputeContext::GetApi()              const { return NkGraphicsApi::NK_API_DIRECTX12; }
uint32 NkDX12ComputeContext::GetMaxGroupSizeX()           const { return 1024; }
uint32 NkDX12ComputeContext::GetMaxGroupSizeY()           const { return 1024; }
uint32 NkDX12ComputeContext::GetMaxGroupSizeZ()           const { return 64; }
uint64 NkDX12ComputeContext::GetSharedMemoryBytes()       const { return 32768; }
bool   NkDX12ComputeContext::SupportsAtomics()            const { return true; }
bool   NkDX12ComputeContext::SupportsFloat64()            const { return false; }

} // namespace nkentseu
#endif // WINDOWS

