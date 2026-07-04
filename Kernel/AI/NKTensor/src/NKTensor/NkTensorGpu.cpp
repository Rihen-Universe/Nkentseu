// =============================================================================
// NkTensorGpu.cpp — implémentation du contexte GPU de NKTensor.
// Réutilise le chemin compute PROUVÉ (cf. Applications/NkComputeNkSL) :
//   NkSL -> GLSL-Vulkan -> (glslang/SPIRV-Cross) -> HLSL/SPIRV/MSL -> pipeline compute.
// =============================================================================
#include "NKTensor/NkTensorGpu.h"
#include "NKTensor/NkTensor.h"   // pour ToGPU/ToCPU + NkTensorInternal (construction GPU)

#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKSL/Compiler/NkSLCompiler.h"
#include "NKSL/ShaderConvert/NkShaderConvert.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
    namespace ai {

        // ---- État interne (pimpl) : tout NKRHI/NKSL confiné ici -----------------
        struct NkTensorGpu::Impl {
            NkIDevice*  device  = nullptr;
            bool        tried   = false;
            const char* backend = "none";

            NkUnorderedMap<uint64, NkBufferHandle> buffers;   // id opaque -> handle
            uint64 nextId = 1;

            struct Kernel {
                NkString         name;
                NkShaderHandle   shader;
                NkPipelineHandle pipe;
                NkDescSetHandle  layout;
                NkBufferHandle   params;   // UBO 16o persistant (pas de churn par dispatch)
            };
            NkVector<Kernel> kernels;   // cache par nom (peu d'entrées -> linéaire)

            // Compile (ou récupère du cache) un kernel NkSL compute.
            // nBuffers storage buffers (bindings 0..n-1) + 1 UBO au binding uboBinding.
            Kernel* GetOrCompile(const char* name, const NkString& nksl,
                                 uint32 nBuffers, uint32 uboBinding) {
                for (uint32 i = 0; i < kernels.Size(); i++)
                    if (kernels[i].name == name) return &kernels[i];

                NkSLCompiler slc;
                NkSLCompileResult gl = slc.Compile(nksl, NkSLStage::NK_COMPUTE,
                                                   NkSLTarget::NK_GLSL_VULKAN);
                if (!gl.success) {
                    logger_src.Errorf("[NkTensorGpu] NkSL->GLSL KO (%s)\n", name);
                    return nullptr;
                }

                // GLSL-Vulkan -> source du backend courant.
                NkShaderDesc sd; sd.debugName = name;
                const NkGraphicsApi api = device->GetApi();
                if (api == NkGraphicsApi::NK_GFX_API_DX11 || api == NkGraphicsApi::NK_GFX_API_DX12) {
                    uint32 sm = (api == NkGraphicsApi::NK_GFX_API_DX12) ? 60u : 50u;
                    NkShaderConvertResult hl = NkShaderConverter::GlslToHlsl(gl.source, NkSLStage::NK_COMPUTE, sm, name);
                    if (!hl.success) { logger_src.Errorf("[NkTensorGpu] GLSL->HLSL KO (%s): %s\n", name, hl.errors.CStr()); return nullptr; }
                    sd.AddHLSL(NkShaderStage::NK_COMPUTE, hl.source.CStr(), "main");
                } else if (api == NkGraphicsApi::NK_GFX_API_VULKAN) {
                    NkShaderConvertResult sp = NkShaderConverter::GlslToSpirv(gl.source, NkSLStage::NK_COMPUTE, name);
                    if (!sp.success) { logger_src.Errorf("[NkTensorGpu] GLSL->SPIRV KO (%s)\n", name); return nullptr; }
                    sd.AddSPIRV(NkShaderStage::NK_COMPUTE, sp.binary.Data(), (uint64)sp.binary.Size());
                } else if (api == NkGraphicsApi::NK_GFX_API_METAL) {
                    NkShaderConvertResult ms = NkShaderConverter::GlslToMsl(gl.source, NkSLStage::NK_COMPUTE, name);
                    if (!ms.success) { logger_src.Errorf("[NkTensorGpu] GLSL->MSL KO (%s)\n", name); return nullptr; }
                    sd.AddMSL(NkShaderStage::NK_COMPUTE, ms.source.CStr(), "main");
                } else {
                    logger_src.Errorf("[NkTensorGpu] API compute non supportée (%s)\n", name);
                    return nullptr;
                }

                NkShaderHandle sh = device->CreateShader(sd);
                if (!sh.IsValid()) { logger_src.Errorf("[NkTensorGpu] CreateShader KO (%s)\n", name); return nullptr; }
                NkComputePipelineDesc cpd; cpd.shader = sh; cpd.debugName = name;
                NkPipelineHandle pipe = device->CreateComputePipeline(cpd);
                if (!pipe.IsValid()) { logger_src.Errorf("[NkTensorGpu] Pipeline KO (%s)\n", name); return nullptr; }

                NkDescriptorSetLayoutDesc ld;
                for (uint32 i = 0; i < nBuffers; i++)
                    ld.Add(i, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
                ld.Add(uboBinding, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_COMPUTE);
                NkDescSetHandle layout = device->CreateDescriptorSetLayout(ld);

                Kernel k; k.name = name; k.shader = sh; k.pipe = pipe; k.layout = layout;
                k.params = device->CreateBuffer(NkBufferDesc::Uniform(16)); // persistant
                kernels.PushBack(k);
                logger_src.Infof("[NkTensorGpu] kernel '%s' compilé (%s)\n", name, NkGraphicsApiName(api));
                return &kernels[kernels.Size() - 1];
            }

            NkBufferHandle Handle(uint64 id) {
                auto* h = buffers.Find(id);
                return h ? *h : NkBufferHandle{};
            }
        };

        // ---- Cycle de vie -------------------------------------------------------
        NkTensorGpu& NkTensorGpu::Get() {
            static NkTensorGpu inst;
            return inst;
        }

        NkTensorGpu::~NkTensorGpu() { Shutdown(); }

        bool NkTensorGpu::EnsureInit() {
            if (!mImpl) mImpl = new Impl();
            if (mImpl->tried) return mImpl->device != nullptr;
            mImpl->tried = true;

            // Device compute headless : on tente DX11 puis DX12 (Windows), sinon
            // l'auto-détection choisit selon la plateforme.
            const NkGraphicsApi tryOrder[] = {
                NkGraphicsApi::NK_GFX_API_DX11,
                NkGraphicsApi::NK_GFX_API_DX12,
                NkGraphicsApi::NK_GFX_API_VULKAN,
                NkGraphicsApi::NK_GFX_API_METAL,
            };
            for (NkGraphicsApi api : tryOrder) {
                NkDeviceInitInfo di; di.api = api;   // pas de surface -> headless
                di.context.software.threading = true;
                NkIDevice* dev = NkDeviceFactory::Create(di);
                if (dev && dev->IsValid() && dev->GetCaps().computeShaders) {
                    mImpl->device  = dev;
                    mImpl->backend = NkGraphicsApiName(api);
                    logger_src.Infof("[NkTensorGpu] device compute: %s\n", mImpl->backend);
                    return true;
                }
                if (dev) NkDeviceFactory::Destroy(dev);
            }
            logger_src.Infof("[NkTensorGpu] aucun device compute GPU disponible (CPU only)\n");
            return false;
        }

        void NkTensorGpu::Shutdown() {
            if (!mImpl) return;
            if (mImpl->device) {
                for (uint32 i = 0; i < mImpl->kernels.Size(); i++) {
                    mImpl->device->DestroyPipeline(mImpl->kernels[i].pipe);
                    mImpl->device->DestroyShader(mImpl->kernels[i].shader);
                    mImpl->device->DestroyDescriptorSetLayout(mImpl->kernels[i].layout);
                    if (mImpl->kernels[i].params.IsValid())
                        mImpl->device->DestroyBuffer(mImpl->kernels[i].params);
                }
                mImpl->buffers.ForEach([this](const uint64&, NkBufferHandle& h) {
                    mImpl->device->DestroyBuffer(h);
                });
                NkDeviceFactory::Destroy(mImpl->device);
                mImpl->device = nullptr;
            }
            delete mImpl; mImpl = nullptr;
        }

        bool        NkTensorGpu::IsAvailable() { return EnsureInit(); }
        const char* NkTensorGpu::BackendName() { EnsureInit(); return mImpl ? mImpl->backend : "none"; }

        // ---- Buffers ------------------------------------------------------------
        uint64 NkTensorGpu::CreateBuffer(nk_size bytes) {
            if (!EnsureInit()) return 0;
            NkBufferHandle h = mImpl->device->CreateBuffer(NkBufferDesc::Storage(bytes, false));
            if (!h.IsValid()) return 0;
            uint64 id = mImpl->nextId++;
            mImpl->buffers.Insert(id, h);
            return id;
        }
        void NkTensorGpu::DestroyBuffer(uint64 id) {
            if (!mImpl || !mImpl->device || id == 0) return;
            auto* h = mImpl->buffers.Find(id);
            if (h) { mImpl->device->DestroyBuffer(*h); mImpl->buffers.Erase(id); }
        }
        bool NkTensorGpu::Upload(uint64 id, const void* data, nk_size bytes) {
            if (!mImpl || !mImpl->device) return false;
            NkBufferHandle h = mImpl->Handle(id); if (!h.IsValid()) return false;
            return mImpl->device->WriteBuffer(h, data, bytes);
        }
        bool NkTensorGpu::Download(uint64 id, void* out, nk_size bytes) {
            if (!mImpl || !mImpl->device) return false;
            NkBufferHandle h = mImpl->Handle(id); if (!h.IsValid()) return false;
            return mImpl->device->ReadBuffer(h, out, bytes);
        }

        // ---- Dispatch helpers ---------------------------------------------------
        static void BindSSBO(NkIDevice* dev, NkDescSetHandle set, uint32 binding, NkBufferHandle buf) {
            NkDescriptorWrite w{};
            w.set = set; w.binding = binding;
            w.type = NkDescriptorType::NK_STORAGE_BUFFER; w.buffer = buf;
            dev->UpdateDescriptorSets(&w, 1);
        }

        bool NkTensorGpu::RunBinary(const char* name, const NkString& nkslSrc,
                                    uint64 a, uint64 b, uint64 c, uint32 count) {
            if (!EnsureInit()) return false;
            Impl* d = mImpl;
            Impl::Kernel* k = d->GetOrCompile(name, nkslSrc, /*nBuffers*/3, /*ubo*/3);
            if (!k) return false;
            NkBufferHandle ha = d->Handle(a), hb = d->Handle(b), hc = d->Handle(c);
            if (!ha.IsValid() || !hb.IsValid() || !hc.IsValid()) return false;

            struct P { uint32 count; } p{ count };
            d->device->WriteBuffer(k->params, &p, sizeof(p));

            NkDescSetHandle set = d->device->AllocateDescriptorSet(k->layout);
            BindSSBO(d->device, set, 0, ha);
            BindSSBO(d->device, set, 1, hb);
            BindSSBO(d->device, set, 2, hc);
            d->device->BindUniformBuffer(set, 3, k->params);

            auto* cmd = d->device->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
            cmd->Begin();
            cmd->BindComputePipeline(k->pipe);
            cmd->BindDescriptorSet(set, 0);
            cmd->Dispatch((count + 63) / 64, 1, 1);
            cmd->UAVBarrier(hc);
            cmd->End();
            d->device->Submit(&cmd, 1);
            d->device->WaitIdle();   // flush avant le Download (ReadBuffer synchronise aussi via Map)

            d->device->FreeDescriptorSet(set);
            d->device->DestroyCommandBuffer(cmd);
            return true;
        }

        bool NkTensorGpu::RunUnary(const char* name, const NkString& nkslSrc,
                                   uint64 a, uint64 b, uint32 count) {
            if (!EnsureInit()) return false;
            Impl* d = mImpl;
            Impl::Kernel* k = d->GetOrCompile(name, nkslSrc, /*nBuffers*/2, /*ubo*/2);
            if (!k) return false;
            NkBufferHandle ha = d->Handle(a), hb = d->Handle(b);
            if (!ha.IsValid() || !hb.IsValid()) return false;

            struct P { uint32 count; } p{ count };
            d->device->WriteBuffer(k->params, &p, sizeof(p));

            NkDescSetHandle set = d->device->AllocateDescriptorSet(k->layout);
            BindSSBO(d->device, set, 0, ha);
            BindSSBO(d->device, set, 1, hb);
            d->device->BindUniformBuffer(set, 2, k->params);

            auto* cmd = d->device->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
            cmd->Begin();
            cmd->BindComputePipeline(k->pipe);
            cmd->BindDescriptorSet(set, 0);
            cmd->Dispatch((count + 63) / 64, 1, 1);
            cmd->UAVBarrier(hb);
            cmd->End();
            d->device->Submit(&cmd, 1);
            d->device->WaitIdle();   // flush avant le Download (ReadBuffer synchronise aussi via Map)

            d->device->FreeDescriptorSet(set);
            d->device->DestroyCommandBuffer(cmd);
            return true;
        }

        // MatMul : buffers 0,1,2 (A,B,C) + UBO { uint M,N,K } binding 3.
        // Dispatch 1D (index plat) : chaque thread calcule un élément C[idx]. On
        // évite le workgroup 2D (course intermittente observée sur WARP headless).
        static const char* kMatMulNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;
@binding(set=0, binding=3) uniform Dims { uint M; uint N; uint K; } d;

layout(local_size_x = 64) in;

@stage(compute)
@entry
void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < d.M * d.N) {
        uint row = idx / d.N;
        uint col = idx - row * d.N;
        float acc = 0.0;
        for (uint k = 0u; k < d.K; k = k + 1u) {
            acc = acc + A.data[row * d.K + k] * B.data[k * d.N + col];
        }
        C.data[idx] = acc;
    }
}
)NKSL";

        bool NkTensorGpu::RunMatMul(uint64 a, uint64 b, uint64 c, uint32 M, uint32 N, uint32 K) {
            if (!EnsureInit()) return false;
            Impl* d = mImpl;
            Impl::Kernel* k = d->GetOrCompile("matmul", NkString(kMatMulNkSL), /*nBuffers*/3, /*ubo*/3);
            if (!k) return false;
            NkBufferHandle ha = d->Handle(a), hb = d->Handle(b), hc = d->Handle(c);
            if (!ha.IsValid() || !hb.IsValid() || !hc.IsValid()) return false;

            struct P { uint32 M, N, K, pad; } p{ M, N, K, 0 };
            d->device->WriteBuffer(k->params, &p, sizeof(p));

            NkDescSetHandle set = d->device->AllocateDescriptorSet(k->layout);
            BindSSBO(d->device, set, 0, ha);
            BindSSBO(d->device, set, 1, hb);
            BindSSBO(d->device, set, 2, hc);
            d->device->BindUniformBuffer(set, 3, k->params);

            auto* cmd = d->device->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
            cmd->Begin();
            cmd->BindComputePipeline(k->pipe);
            cmd->BindDescriptorSet(set, 0);
            cmd->Dispatch((M * N + 63) / 64, 1, 1);   // 1D : un thread par élément C
            cmd->UAVBarrier(hc);
            cmd->End();
            d->device->Submit(&cmd, 1);
            d->device->WaitIdle();   // flush avant le Download (ReadBuffer synchronise aussi via Map)

            d->device->FreeDescriptorSet(set);
            d->device->DestroyCommandBuffer(cmd);
            return true;
        }

        // =====================================================================
        // Intégration au niveau tenseur : construction GPU + transferts CPU<->GPU.
        // NkTensorInternal est ami de NkTensor -> accès aux membres privés.
        // =====================================================================
        struct NkTensorInternal {
            static NkTensor MakeGpu(const NkShape& shape, NkDType dtype, uint64 gpuBuf) {
                NkTensor t;
                t.mStorage = NkTensorStorage::Allocate(0);   // pas de data CPU
                t.mStorage->gpuBuffer = gpuBuf;
                t.mShape   = shape;
                t.mStrides = NkContiguousStrides(shape);
                t.mDType   = dtype;
                t.mDevice  = NkDevice::NK_GPU;
                t.mOffset  = 0;
                return t;
            }
            static uint64 GpuBuffer(const NkTensor& t) {
                return t.mStorage ? t.mStorage->gpuBuffer : 0;
            }
        };

        // Kernel élémentaire add (mêmes bindings que RunBinary attend).
        static const char* kAddNkSL = R"NKSL(
@binding(set=0, binding=0) buffer BufA { float data[]; } A;
@binding(set=0, binding=1) buffer BufB { float data[]; } B;
@binding(set=0, binding=2) buffer BufC { float data[]; } C;
@binding(set=0, binding=3) uniform Params { uint count; } pc;
layout(local_size_x = 64) in;
@stage(compute)
@entry
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i < pc.count) { C.data[i] = A.data[i] + B.data[i]; }
}
)NKSL";

        NkTensor NkGpuAdd(const NkTensor& a, const NkTensor& b) {
            NkTensor ga = (a.Device() == NkDevice::NK_GPU) ? a : a.ToGPU();
            NkTensor gb = (b.Device() == NkDevice::NK_GPU) ? b : b.ToGPU();
            if (!ga.IsValid() || !gb.IsValid()) return NkTensor{};
            if (ga.Numel() != gb.Numel()) return NkTensor{};   // v1 : mêmes formes (pas de broadcast GPU)
            const int64 n = ga.Numel();
            const nk_size bytes = (nk_size)n * NkDTypeSize(ga.DType());
            uint64 cbuf = NkTensorGpu::Get().CreateBuffer(bytes);
            if (!cbuf) return NkTensor{};
            NkTensorGpu::Get().RunBinary("add", NkString(kAddNkSL),
                                         NkTensorInternal::GpuBuffer(ga),
                                         NkTensorInternal::GpuBuffer(gb),
                                         cbuf, (uint32)n);
            return NkTensorInternal::MakeGpu(ga.Shape(), ga.DType(), cbuf);
        }

        NkTensor NkGpuMatmul(const NkTensor& a, const NkTensor& b) {
            NkTensor ga = (a.Device() == NkDevice::NK_GPU) ? a : a.ToGPU();
            NkTensor gb = (b.Device() == NkDevice::NK_GPU) ? b : b.ToGPU();
            if (!ga.IsValid() || !gb.IsValid()) return NkTensor{};
            if (ga.Rank() != 2 || gb.Rank() != 2) return NkTensor{};
            const int64 M = ga.Shape()[0], K = ga.Shape()[1];
            const int64 K2 = gb.Shape()[0], N = gb.Shape()[1];
            if (K != K2) return NkTensor{};
            NkShape outShape; outShape.PushBack(M); outShape.PushBack(N);
            const nk_size bytes = (nk_size)(M * N) * NkDTypeSize(ga.DType());
            uint64 cbuf = NkTensorGpu::Get().CreateBuffer(bytes);
            if (!cbuf) return NkTensor{};
            NkTensorGpu::Get().RunMatMul(NkTensorInternal::GpuBuffer(ga),
                                         NkTensorInternal::GpuBuffer(gb),
                                         cbuf, (uint32)M, (uint32)N, (uint32)K);
            return NkTensorInternal::MakeGpu(outShape, ga.DType(), cbuf);
        }

        NkTensor NkTensor::ToGPU() const {
            if (mDevice == NkDevice::NK_GPU) return *this;
            if (!NkTensorGpu::Get().IsAvailable()) return NkTensor{};
            NkTensor cont = Contiguous();
            const nk_size bytes = (nk_size)cont.Numel() * NkDTypeSize(cont.DType());
            uint64 buf = NkTensorGpu::Get().CreateBuffer(bytes);
            if (!buf) return NkTensor{};
            NkTensorGpu::Get().Upload(buf, cont.RawData(), bytes);
            return NkTensorInternal::MakeGpu(cont.Shape(), cont.DType(), buf);
        }

        NkTensor NkTensor::ToCPU() const {
            if (mDevice == NkDevice::NK_CPU) return *this;
            NkTensor out = NkTensor::Empty(mShape, mDType, NkDevice::NK_CPU);
            const nk_size bytes = (nk_size)Numel() * NkDTypeSize(mDType);
            NkTensorGpu::Get().Download(mStorage->gpuBuffer, out.RawData(), bytes);
            return out;
        }

    } // namespace ai
} // namespace nkentseu
