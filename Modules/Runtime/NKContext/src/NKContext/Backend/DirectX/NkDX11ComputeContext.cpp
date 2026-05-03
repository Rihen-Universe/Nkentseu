// =============================================================================
// NkDX11ComputeContext.cpp â€” Compute DX11 (CS_5_0, Structured Buffers, UAV)
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKLogger/NkLog.h"


#if defined(NKENTSEU_PLATFORM_WINDOWS)
#include "NkDX11ComputeContext.h"
#include "NKContainers/NkContainers.h"
#include <d3dcompiler.h>
#include <cstdio>

#if defined(MemoryBarrier)
#undef MemoryBarrier
#endif

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")

#define NK_DX11C_LOG(...) logger.Infof("[NkDX11Compute] " __VA_ARGS__)
#define NK_DX11C_ERR(...) logger.Errorf("[NkDX11Compute] " __VA_ARGS__)
#define NK_DX11C_CHECK(hr,msg) do { if(FAILED(hr)){NK_DX11C_ERR(msg " hr=0x%08X\n",(unsigned)(hr));return;} } while(0)
#define NK_DX11C_CHECKB(hr,msg) do { if(FAILED(hr)){NK_DX11C_ERR(msg " hr=0x%08X\n",(unsigned)(hr));return {};} } while(0)

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

NkDX11ComputeContext::~NkDX11ComputeContext() { if(mIsValid) Shutdown(); }

void NkDX11ComputeContext::InitFromGraphicsContext(NkIGraphicsContext* gfx) {
    if (!gfx || !gfx->IsValid()) return;
    auto* d = static_cast<NkDX11ContextData*>(gfx->GetNativeContextData());
    if (!d) return;
    mDevice  = d->device;
    mContext = d->context;
    mIsValid = true;
    NK_DX11C_LOG("Initialized from graphics context\n");
}

bool NkDX11ComputeContext::Init(const NkContextDesc&) {
    NK_DX11C_ERR("Standalone DX11 compute: use InitFromGraphicsContext\n");
    return false;
}

void NkDX11ComputeContext::Shutdown() {
    WaitIdle();
    mDevice.Reset();
    mContext.Reset();
    mIsValid = false;
}

bool NkDX11ComputeContext::IsValid() const { return mIsValid; }

// â”€â”€ Buffers (Structured Buffer + UAV + Staging readback) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
struct DX11BufSet {
    ComPtr<ID3D11Buffer>              buf;
    ComPtr<ID3D11UnorderedAccessView> uav;
    ComPtr<ID3D11Buffer>              staging; // pour readback CPU
    uint64 sizeBytes = 0;
    bool   cpuRead   = false;
};

NkComputeBuffer NkDX11ComputeContext::CreateBuffer(const NkComputeBufferDesc& desc) {
    NkComputeBuffer out;
    if (!mIsValid || desc.sizeBytes == 0) return out;

    auto* bs = new DX11BufSet();
    bs->sizeBytes = desc.sizeBytes;
    bs->cpuRead   = desc.cpuReadable;

    // Structured buffer GPU
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth           = (UINT)desc.sizeBytes;
    bd.Usage               = D3D11_USAGE_DEFAULT;
    bd.BindFlags           = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    bd.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bd.StructureByteStride = 4; // uint/float de base â€” l'utilisateur gÃ¨re le layout dans HLSL

    D3D11_SUBRESOURCE_DATA initData{desc.initialData, 0, 0};
    HRESULT hr = mDevice->CreateBuffer(&bd,
        desc.initialData ? &initData : nullptr, &bs->buf);
    if (FAILED(hr)) { NK_DX11C_ERR("CreateBuffer failed 0x%08X\n",(unsigned)hr); delete bs; return out; }

    // UAV
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format             = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension      = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = (UINT)(desc.sizeBytes / 4);
    hr = mDevice->CreateUnorderedAccessView(bs->buf.Get(), &uavDesc, &bs->uav);
    if (FAILED(hr)) { NK_DX11C_ERR("CreateUAV failed\n"); delete bs; return out; }

    // Staging pour readback
    if (desc.cpuReadable) {
        D3D11_BUFFER_DESC sd{};
        sd.ByteWidth = (UINT)desc.sizeBytes;
        sd.Usage     = D3D11_USAGE_STAGING;
        sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        mDevice->CreateBuffer(&sd, nullptr, &bs->staging);
    }

    out.handle    = bs;
    out.sizeBytes = desc.sizeBytes;
    out.valid     = true;
    return out;
}

void NkDX11ComputeContext::DestroyBuffer(NkComputeBuffer& buf) {
    if (!buf.valid) return;
    delete static_cast<DX11BufSet*>(buf.handle);
    buf = NkComputeBuffer{};
}

bool NkDX11ComputeContext::WriteBuffer(NkComputeBuffer& buf,
    const void* data, uint64 bytes, uint64 offset) {
    if (!buf.valid || !data) return false;
    auto* bs = static_cast<DX11BufSet*>(buf.handle);
    D3D11_BOX box{(UINT)offset, 0, 0, (UINT)(offset+bytes), 1, 1};
    mContext->UpdateSubresource(bs->buf.Get(), 0, &box, data, 0, 0);
    return true;
}

bool NkDX11ComputeContext::ReadBuffer(const NkComputeBuffer& buf,
    void* outData, uint64 bytes, uint64 offset) {
    if (!buf.valid || !outData) return false;
    auto* bs = static_cast<DX11BufSet*>(buf.handle);
    if (!bs->staging) return false;
    mContext->CopyResource(bs->staging.Get(), bs->buf.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = mContext->Map(bs->staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) return false;
    memcpy(outData, (uint8*)mapped.pData + offset, (size_t)bytes);
    mContext->Unmap(bs->staging.Get(), 0);
    return true;
}

// â”€â”€ Shaders HLSL â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
NkComputeShader NkDX11ComputeContext::CreateShaderFromSource(
    const char* src, const char* entry) {
    NkComputeShader s;
    ComPtr<ID3DBlob> blob, err;
    HRESULT hr = D3DCompile(src, strlen(src), nullptr, nullptr, nullptr,
        entry, "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &err);
    if (FAILED(hr)) {
        NK_DX11C_ERR("HLSL compile: %s\n",
            err ? (char*)err->GetBufferPointer() : "unknown");
        return s;
    }
    ComPtr<ID3D11ComputeShader> cs;
    hr = mDevice->CreateComputeShader(blob->GetBufferPointer(),
                                       blob->GetBufferSize(), nullptr, &cs);
    if (FAILED(hr)) { NK_DX11C_ERR("CreateComputeShader failed\n"); return s; }
    s.handle = cs.Detach();
    s.valid  = true;
    return s;
}

NkComputeShader NkDX11ComputeContext::CreateShaderFromFile(
    const char* path, const char* entry) {
    NkVector<char> src;
    if (!ReadTextFile(path, src)) {
        NK_DX11C_ERR("Cannot open: %s\n", path);
        return {};
    }
    return CreateShaderFromSource(src.Data(), entry);
}

void NkDX11ComputeContext::DestroyShader(NkComputeShader& s) {
    if (!s.valid) return;
    static_cast<ID3D11ComputeShader*>(s.handle)->Release();
    s = NkComputeShader{};
}

// â”€â”€ Pipeline = juste le shader en DX11 â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
NkComputePipeline NkDX11ComputeContext::CreatePipeline(const NkComputeShader& s) {
    NkComputePipeline p;
    if (!s.valid) return p;
    p.handle = s.handle; // En DX11, le "pipeline" = le CS
    p.valid  = true;
    return p;
}
void NkDX11ComputeContext::DestroyPipeline(NkComputePipeline& p) { p = {}; }

void NkDX11ComputeContext::BindPipeline(const NkComputePipeline& p) {
    if (!p.valid) return;
    mContext->CSSetShader(static_cast<ID3D11ComputeShader*>(p.handle), nullptr, 0);
}

void NkDX11ComputeContext::BindBuffer(uint32 slot, NkComputeBuffer& buf) {
    if (!buf.valid) return;
    auto* bs = static_cast<DX11BufSet*>(buf.handle);
    ID3D11UnorderedAccessView* uavs[] = {bs->uav.Get()};
    mContext->CSSetUnorderedAccessViews(slot, 1, uavs, nullptr);
}

void NkDX11ComputeContext::Dispatch(uint32 gx, uint32 gy, uint32 gz) {
    mContext->Dispatch(gx, gy, gz);
}

void NkDX11ComputeContext::WaitIdle() {
    // DX11 est synchrone par dÃ©faut â€” Flush suffit
    mContext->Flush();
}

void NkDX11ComputeContext::MemoryBarrier() {
    // DX11 gÃ¨re les barriÃ¨res automatiquement entre Dispatch/Map
}

NkGraphicsApi NkDX11ComputeContext::GetApi()              const { return NkGraphicsApi::NK_GFX_API_D3D11; }
uint32 NkDX11ComputeContext::GetMaxGroupSizeX()           const { return 1024; }
uint32 NkDX11ComputeContext::GetMaxGroupSizeY()           const { return 1024; }
uint32 NkDX11ComputeContext::GetMaxGroupSizeZ()           const { return 64; }
uint64 NkDX11ComputeContext::GetSharedMemoryBytes()       const { return 32768; }
bool   NkDX11ComputeContext::SupportsAtomics()            const { return true; }
bool   NkDX11ComputeContext::SupportsFloat64()            const { return false; }

} // namespace nkentseu
#endif // WINDOWS

