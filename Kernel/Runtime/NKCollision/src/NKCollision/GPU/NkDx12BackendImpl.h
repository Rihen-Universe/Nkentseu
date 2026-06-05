#pragma once
// dx12_backend.h — DirectX 12 Compute collision backend
// Windows only. Requires: d3d12.lib, dxgi.lib, d3dcompiler.lib

#if defined(COL_ENABLE_DX12) && defined(_WIN32)
#include "../../include/collision/gpu_backend.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace col {

#define DX_CHECK(hr) do { if(FAILED(hr)) throw std::runtime_error(\
    "D3D12 error HRESULT=" + std::to_string(hr) + " at " __FILE__ ":" + std::to_string(__LINE__)); } while(0)

class DX12ComputeBackend : public ICollisionBackend {
public:
    DX12ComputeBackend() {
        createDevice();
        createCommandObjects();
        createRootSignature();
        createPipelines();
        counterBuffer_ = createBuffer(16, false);
    }
    ~DX12ComputeBackend() override {
        if(fenceEvent_) CloseHandle(fenceEvent_);
    }

    GPUBackend  type()  const override { return GPUBackend::DirectX12; }
    std::string name()  const override { return "DX12"; }

    GPUBuffer createBuffer(size_t bytes, bool readback) override {
        D3D12_HEAP_TYPE heapType = readback ? D3D12_HEAP_TYPE_READBACK : D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_STATES state = readback ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_FLAGS flags = readback ? D3D12_RESOURCE_FLAG_NONE : D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        D3D12_HEAP_PROPERTIES hp{heapType,D3D12_CPU_PAGE_PROPERTY_UNKNOWN,D3D12_MEMORY_POOL_UNKNOWN,1,1};
        D3D12_RESOURCE_DESC   rd{D3D12_RESOURCE_DIMENSION_BUFFER,0,(UINT64)bytes,1,1,1,
                                  DXGI_FORMAT_UNKNOWN,{1,0},D3D12_TEXTURE_LAYOUT_ROW_MAJOR,flags};

        ComPtr<ID3D12Resource> res;
        DX_CHECK(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&rd,state,nullptr,IID_PPV_ARGS(&res)));

        uint64_t handle = nextHandle_++;
        buffers_[handle] = {res, bytes, readback};

        GPUBuffer gb; gb.handle=(void*)(uintptr_t)handle; gb.byteSize=bytes; gb.backend=GPUBackend::DirectX12;
        return gb;
    }

    void destroyBuffer(GPUBuffer& gb) override {
        buffers_.erase((uint64_t)(uintptr_t)gb.handle); gb.handle=nullptr;
    }

    void uploadBuffer(GPUBuffer& dst, const void* src, size_t bytes) override {
        // Create upload heap buffer
        D3D12_HEAP_PROPERTIES hp{D3D12_HEAP_TYPE_UPLOAD,D3D12_CPU_PAGE_PROPERTY_UNKNOWN,D3D12_MEMORY_POOL_UNKNOWN,1,1};
        D3D12_RESOURCE_DESC   rd{D3D12_RESOURCE_DIMENSION_BUFFER,0,(UINT64)bytes,1,1,1,
                                  DXGI_FORMAT_UNKNOWN,{1,0},D3D12_TEXTURE_LAYOUT_ROW_MAJOR,D3D12_RESOURCE_FLAG_NONE};
        ComPtr<ID3D12Resource> upload;
        DX_CHECK(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&rd,
            D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload)));
        void* mapped=nullptr; upload->Map(0,nullptr,&mapped);
        std::memcpy(mapped,src,bytes); upload->Unmap(0,nullptr);

        auto& dstBuf = buf(dst);
        resetCmd();
        D3D12_RESOURCE_BARRIER b1=transition(dstBuf.resource.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST);
        cmdList_->ResourceBarrier(1,&b1);
        cmdList_->CopyBufferRegion(dstBuf.resource.Get(),0,upload.Get(),0,bytes);
        D3D12_RESOURCE_BARRIER b2=transition(dstBuf.resource.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmdList_->ResourceBarrier(1,&b2);
        execWait();
    }

    void downloadBuffer(void* dst, const GPUBuffer& src, size_t bytes) override {
        auto& srcBuf = buf(src);
        if(srcBuf.readback) {
            D3D12_RANGE r{0,bytes}; void* mapped=nullptr;
            srcBuf.resource->Map(0,&r,&mapped); std::memcpy(dst,mapped,bytes);
            srcBuf.resource->Unmap(0,nullptr); return;
        }
        D3D12_HEAP_PROPERTIES hp{D3D12_HEAP_TYPE_READBACK,D3D12_CPU_PAGE_PROPERTY_UNKNOWN,D3D12_MEMORY_POOL_UNKNOWN,1,1};
        D3D12_RESOURCE_DESC   rd{D3D12_RESOURCE_DIMENSION_BUFFER,0,(UINT64)bytes,1,1,1,
                                  DXGI_FORMAT_UNKNOWN,{1,0},D3D12_TEXTURE_LAYOUT_ROW_MAJOR,D3D12_RESOURCE_FLAG_NONE};
        ComPtr<ID3D12Resource> rb;
        device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&rd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&rb));
        resetCmd();
        D3D12_RESOURCE_BARRIER b1=transition(srcBuf.resource.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
        cmdList_->ResourceBarrier(1,&b1);
        cmdList_->CopyBufferRegion(rb.Get(),0,srcBuf.resource.Get(),0,bytes);
        D3D12_RESOURCE_BARRIER b2=transition(srcBuf.resource.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmdList_->ResourceBarrier(1,&b2);
        execWait();
        D3D12_RANGE r{0,bytes}; void* mapped=nullptr;
        rb->Map(0,&r,&mapped); std::memcpy(dst,mapped,bytes); rb->Unmap(0,nullptr);
    }

    uint32_t gpuBroadphase(const GPUBuffer& aabbsIn, uint32_t count,
                            GPUBuffer& pairsOut, uint32_t maxPairs) override {
        uint32_t zero=0; uploadBuffer(counterBuffer_,&zero,sizeof(uint32_t));
        if(!broadphasePSO_) return 0;
        resetCmd();
        cmdList_->SetComputeRootSignature(rootSig_.Get());
        cmdList_->SetPipelineState(broadphasePSO_.Get());
        uint32_t consts[2]={count,maxPairs};
        cmdList_->SetComputeRoot32BitConstants(0,2,consts,0);
        cmdList_->SetComputeRootUnorderedAccessView(1,buf(aabbsIn).resource->GetGPUVirtualAddress());
        cmdList_->SetComputeRootUnorderedAccessView(2,buf(pairsOut).resource->GetGPUVirtualAddress());
        cmdList_->SetComputeRootUnorderedAccessView(3,buf(counterBuffer_).resource->GetGPUVirtualAddress());
        D3D12_RESOURCE_BARRIER uavBarrier{D3D12_RESOURCE_BARRIER_TYPE_UAV,D3D12_RESOURCE_BARRIER_FLAG_NONE,{}};
        cmdList_->ResourceBarrier(1,&uavBarrier);
        cmdList_->Dispatch((count+63)/64,(count+63)/64,1);
        cmdList_->ResourceBarrier(1,&uavBarrier);
        execWait();
        uint32_t result=0; downloadBuffer(&result,counterBuffer_,sizeof(uint32_t));
        return std::min(result,maxPairs);
    }

    uint32_t gpuNarrowphase(const GPUBuffer& a, const GPUBuffer& p,
                             uint32_t pc, GPUBuffer& c, uint32_t mc) override {
        (void)a;(void)p;(void)pc;(void)c;(void)mc; return 0;
    }

    void sync()  override { waitGPU(); }
    void flush() override {}
    size_t   maxBufferSize()    const override { return size_t(2)<<30; }
    uint32_t warpSize()         const override { return 32; }
    uint32_t maxWorkgroupSize() const override { return 1024; }
    bool     supportsAtomics()  const override { return true; }

private:
    struct DXBuf { ComPtr<ID3D12Resource> resource; size_t size=0; bool readback=false; };

    ComPtr<ID3D12Device>               device_;
    ComPtr<ID3D12CommandQueue>         cmdQueue_;
    ComPtr<ID3D12CommandAllocator>     cmdAlloc_;
    ComPtr<ID3D12GraphicsCommandList>  cmdList_;
    ComPtr<ID3D12Fence>                fence_;
    HANDLE                             fenceEvent_=nullptr;
    uint64_t                           fenceVal_=0;
    ComPtr<ID3D12RootSignature>        rootSig_;
    ComPtr<ID3D12PipelineState>        broadphasePSO_;
    std::unordered_map<uint64_t,DXBuf> buffers_;
    uint64_t                           nextHandle_=1;
    GPUBuffer                          counterBuffer_;

    void createDevice() {
        ComPtr<IDXGIFactory4> factory;
        CreateDXGIFactory1(IID_PPV_ARGS(&factory));
        ComPtr<IDXGIAdapter1> adapter;
        factory->EnumAdapters1(0,&adapter);
        D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device_));
    }
    void createCommandObjects() {
        D3D12_COMMAND_QUEUE_DESC qd{}; qd.Type=D3D12_COMMAND_LIST_TYPE_COMPUTE;
        device_->CreateCommandQueue(&qd,IID_PPV_ARGS(&cmdQueue_));
        device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE,IID_PPV_ARGS(&cmdAlloc_));
        device_->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_COMPUTE,cmdAlloc_.Get(),nullptr,IID_PPV_ARGS(&cmdList_));
        cmdList_->Close();
        device_->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence_));
        fenceEvent_=CreateEvent(nullptr,FALSE,FALSE,nullptr);
    }
    void createRootSignature() {
        D3D12_ROOT_PARAMETER params[4]{};
        params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[0].Constants={0,0,2};
        for(int i=1;i<4;i++){
            params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;
            params[i].Descriptor.ShaderRegister=i-1;
        }
        D3D12_ROOT_SIGNATURE_DESC rsd{4,params,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> sig,err;
        D3D12SerializeRootSignature(&rsd,D3D_ROOT_SIGNATURE_VERSION_1,&sig,&err);
        if(sig) device_->CreateRootSignature(0,sig->GetBufferPointer(),sig->GetBufferSize(),IID_PPV_ARGS(&rootSig_));
    }
    void createPipelines() {
        static const char* src=R"(
RWStructuredBuffer<float4> aabbs:register(u0);
RWStructuredBuffer<uint2>  pairs:register(u1);
RWBuffer<uint>             cnt  :register(u2);
cbuffer C:register(b0){uint n;uint mp;}
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID){
    uint i=id.x,j=id.y; if(i>=n||j>=n||j<=i)return;
    float4 mi=aabbs[i*2],ma=aabbs[i*2+1],mj=aabbs[j*2],mja=aabbs[j*2+1];
    if(mi.x<=mja.x&&ma.x>=mj.x&&mi.y<=mja.y&&ma.y>=mj.y&&mi.z<=mja.z&&ma.z>=mj.z){
        uint idx;InterlockedAdd(cnt[0],1,idx);if(idx<mp){pairs[idx]=uint2(i,j);}}})";
        ComPtr<ID3DBlob> cs,err;
        if(SUCCEEDED(D3DCompile(src,strlen(src),nullptr,nullptr,nullptr,"main","cs_5_0",
            D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&cs,&err))) {
            D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};
            pd.pRootSignature=rootSig_.Get();
            pd.CS={cs->GetBufferPointer(),cs->GetBufferSize()};
            device_->CreateComputePipelineState(&pd,IID_PPV_ARGS(&broadphasePSO_));
        }
    }
    void resetCmd() { cmdAlloc_->Reset(); cmdList_->Reset(cmdAlloc_.Get(),nullptr); }
    void execWait() {
        cmdList_->Close();
        ID3D12CommandList* lists[]={cmdList_.Get()};
        cmdQueue_->ExecuteCommandLists(1,lists); waitGPU();
    }
    void waitGPU() {
        fenceVal_++;
        cmdQueue_->Signal(fence_.Get(),fenceVal_);
        if(fence_->GetCompletedValue()<fenceVal_){
            fence_->SetEventOnCompletion(fenceVal_,fenceEvent_);
            WaitForSingleObject(fenceEvent_,INFINITE);
        }
    }
    static D3D12_RESOURCE_BARRIER transition(ID3D12Resource* r,
        D3D12_RESOURCE_STATES b, D3D12_RESOURCE_STATES a) {
        D3D12_RESOURCE_BARRIER bar{D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,D3D12_RESOURCE_BARRIER_FLAG_NONE,{}};
        bar.Transition={r,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,b,a};
        return bar;
    }
    DXBuf& buf(const GPUBuffer& gb) { return buffers_.at((uint64_t)(uintptr_t)gb.handle); }
};

} // namespace col
#endif
