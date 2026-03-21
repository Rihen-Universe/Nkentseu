// =============================================================================
// NkRHI_ML.cpp — Implémentation NkMLContext
// =============================================================================
#include "NKRHI/Core/NkML.h"
#include "NKLogger/NkLog.h"
#include "NKCore/NkMacros.h"
#include "NKContainers/Sequential/NkVector.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cassert>
#include <random>

namespace nkentseu {

// =============================================================================
bool NkMLContext::Init(NkIDevice* device) {
    if (!device || !device->IsValid()) return false;
    mDevice = device;

    if (!device->GetCaps().computeShaders) {
        logger_src.Infof("[NkML] Erreur: le device ne supporte pas les compute shaders\n");
        return false;
    }

    // Layout par défaut : 4 SSBOs (entrée A, entrée B, entrée C, sortie)
    NkDescriptorSetLayoutDesc layoutDesc;
    for (uint32 i=0;i<4;i++)
        layoutDesc.Add(i, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_COMPUTE);
    mDefaultLayout = mDevice->CreateDescriptorSetLayout(layoutDesc);

    // Buffer scratch pour les réductions (1 float32 = 4 bytes)
    mScratch = CreateTensor({1}, false, "_scratch");

    mReady = true;
    logger_src.Infof("[NkML] Initialisé sur: %s\n", device->GetCaps().vramBytes
           ? "GPU" : "device");
    logger_src.Infof("[NkML] Max group size: %u | Shared mem: %u KB\n",
           device->GetCaps().maxComputeGroupSizeX,
           device->GetCaps().maxComputeSharedMemory/1024);
    return true;
}

void NkMLContext::Shutdown() {
    Sync();
    // Détruire tous les pipelines
    mPipelineCache.ForEach([this](const NkString&, PipelineCache& v) {
        mDevice->DestroyPipeline(const_cast<NkPipelineHandle&>(v.pipeline));
        mDevice->DestroyShader (const_cast<NkShaderHandle&>  (v.shader));
    });
    mPipelineCache.Clear();
    if (mDefaultLayout.IsValid())
        mDevice->DestroyDescriptorSetLayout(mDefaultLayout);
    DestroyTensor(mScratch);
    mReady  = false;
    mDevice = nullptr;
}

// =============================================================================
// Gestion des tenseurs
// =============================================================================
NkTensor NkMLContext::CreateTensor(const NkTensorShape& shape,
                                    bool requiresGrad, const char* name) {
    NkTensor t;
    t.shape        = shape;
    t.requiresGrad = requiresGrad;
    t.name         = name ? name : "";

    auto desc = NkBufferDesc::Storage(shape.NumBytes(), false);
    desc.debugName = name;
    t.buffer = mDevice->CreateBuffer(desc);

    if (requiresGrad) {
        auto gdesc = NkBufferDesc::Storage(shape.NumBytes(), false);
        t.grad = mDevice->CreateBuffer(gdesc);
        // Init gradient à zéro
        auto* cmd = mDevice->CreateCommandBuffer();
        cmd->Begin();
        cmd->ClearBuffer(t.grad, 0);
        cmd->End();
        mDevice->Submit(&cmd, 1);
        mDevice->DestroyCommandBuffer(cmd);
    }
    return t;
}

void NkMLContext::DestroyTensor(NkTensor& t) {
    if (t.buffer.IsValid()) mDevice->DestroyBuffer(t.buffer);
    if (t.grad.IsValid())   mDevice->DestroyBuffer(t.grad);
    t = NkTensor{};
}

bool NkMLContext::Upload(NkTensor& t, const float32* data, uint32 count) {
    uint32 n = count>0 ? count : t.NumElements();
    return mDevice->WriteBuffer(t.buffer, data, n*sizeof(float32));
}

bool NkMLContext::Download(const NkTensor& t, float32* out, uint32 count) {
    uint32 n = count>0 ? count : t.NumElements();
    return mDevice->ReadBuffer(t.buffer, out, n*sizeof(float32));
}

void NkMLContext::FillZero(NkTensor& t) {
    auto* cmd=mDevice->CreateCommandBuffer();
    cmd->Begin(); cmd->ClearBuffer(t.buffer,0); cmd->End();
    mDevice->Submit(&cmd,1); mDevice->DestroyCommandBuffer(cmd);
}
void NkMLContext::FillOnes (NkTensor& t) { FillConst(t,1.f); }
void NkMLContext::FillConst(NkTensor& t, float32 val) {
    NkVector<float32> v; v.Resize(t.NumElements());
    for (usize i=0; i<v.Size(); i++) v[i]=val;
    Upload(t, v.Data());
}

void NkMLContext::InitXavier(NkTensor& t) {
    uint32 fan_in  = t.shape.Rank()>=2 ? t.shape[t.shape.Rank()-2] : t.NumElements();
    uint32 fan_out = t.shape[t.shape.Rank()-1];
    float32 limit = sqrtf(6.f / float32(fan_in+fan_out));
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float32> dist(-limit, limit);
    NkVector<float32> v; v.Resize(t.NumElements());
    for (usize i=0; i<v.Size(); i++) v[i]=dist(rng);
    Upload(t, v.Data());
}

void NkMLContext::InitHe(NkTensor& t) {
    uint32 fan_in = t.shape.Rank()>=2 ? t.shape[t.shape.Rank()-2] : t.NumElements();
    float32 std_dev = sqrtf(2.f / float32(fan_in));
    std::mt19937 rng(std::random_device{}());
    std::normal_distribution<float32> dist(0.f, std_dev);
    NkVector<float32> v; v.Resize(t.NumElements());
    for (usize i=0; i<v.Size(); i++) v[i]=dist(rng);
    Upload(t, v.Data());
}

// =============================================================================
// Compile/cache un pipeline compute
// =============================================================================
NkPipelineHandle NkMLContext::GetOrCompilePipeline(const char* key,
                                                     const char* glslSrc) {
    auto* cp = mPipelineCache.Find(key);
    if (cp) return cp->pipeline;

    NkShaderDesc sd;
    sd.AddGLSL(NkShaderStage::NK_COMPUTE, glslSrc, "main");
    sd.debugName = key;

    NkShaderHandle sh = mDevice->CreateShader(sd);
    if (!sh.IsValid()) {
        logger_src.Infof("[NkML] Erreur compilation shader: %s\n", key);
        return {};
    }

    NkComputePipelineDesc cpd; cpd.shader=sh; cpd.debugName=key;
    NkPipelineHandle pipe = mDevice->CreateComputePipeline(cpd);

    mPipelineCache[key] = {sh, pipe};
    return pipe;
}

// =============================================================================
// Dispatch 1D (n éléments, groupSize threads par groupe)
// =============================================================================
void NkMLContext::Dispatch1D(NkICommandBuffer* cmd, NkPipelineHandle pipe,
                               uint32 n, uint32 groupSize) {
    cmd->BindComputePipeline(pipe);
    uint32 groups = (n + groupSize - 1) / groupSize;
    cmd->Dispatch(groups, 1, 1);
}

// =============================================================================
// MatMul A×B → C
// =============================================================================
void NkMLContext::MatMul(const NkTensor& A, const NkTensor& B,
                          NkTensor& C, bool transA, bool transB) {
    uint32 M = transA ? A.Cols() : A.Rows();
    uint32 K = transA ? A.Rows() : A.Cols();
    uint32 N = transB ? B.Rows() : B.Cols();

    auto pipe = GetOrCompilePipeline("MatMul", NkMLShaders::MatMul());
    if (!pipe.IsValid()) return;

    auto set = mDevice->AllocateDescriptorSet(mDefaultLayout);
    mDevice->BindUniformBuffer(set,0,A.buffer);
    mDevice->BindUniformBuffer(set,1,B.buffer);
    mDevice->BindUniformBuffer(set,2,C.buffer);

    auto* cmd = mDevice->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
    cmd->Begin();
    cmd->BindComputePipeline(pipe);
    cmd->BindDescriptorSet(set, 0);
    // Push constants: M, N, K, alpha, transA, transB
    struct PC { int M,N,K; float32 alpha; int transA,transB; } pc{(int)M,(int)N,(int)K,1.f,(int)transA,(int)transB};
    cmd->PushConstants(NkShaderStage::NK_COMPUTE,0,sizeof(pc),&pc);
    cmd->Dispatch((N+15)/16, (M+15)/16, 1);
    // Barrier: compute write → compute read
    cmd->UAVBarrier(C.buffer);
    cmd->End();

    mDevice->Submit(&cmd, 1);
    mDevice->FreeDescriptorSet(set);
    mDevice->DestroyCommandBuffer(cmd);
}

// =============================================================================
// MatMulAdd A×B + bias → C
// =============================================================================
void NkMLContext::MatMulAdd(const NkTensor& A, const NkTensor& B,
                             const NkTensor& bias, NkTensor& C) {
    // Réutilise MatMul puis Add
    MatMul(A, B, C);

    // Add bias (broadcast sur les colonnes)
    auto pipe = GetOrCompilePipeline("AddBias", R"(
#version 430
layout(local_size_x=256) in;
layout(std430,binding=0) buffer C    { float c[]; };
layout(std430,binding=1) readonly buffer Bias { float bias[]; };
uniform uint rows, cols;
void main(){
    uint i=gl_GlobalInvocationID.x;
    if(i<rows*cols) c[i]+=bias[i%cols];
}
)");
    if (!pipe.IsValid()) return;

    auto set = mDevice->AllocateDescriptorSet(mDefaultLayout);
    mDevice->BindUniformBuffer(set,0,C.buffer);
    mDevice->BindUniformBuffer(set,1,bias.buffer);

    auto* cmd = mDevice->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
    cmd->Begin();
    cmd->BindComputePipeline(pipe);
    cmd->BindDescriptorSet(set,0);
    struct PC2 { uint32 rows,cols; } pc{C.Rows(), C.Cols()};
    cmd->PushConstants(NkShaderStage::NK_COMPUTE,0,sizeof(pc),&pc);
    uint32 n=C.NumElements();
    cmd->Dispatch((n+255)/256,1,1);
    cmd->UAVBarrier(C.buffer);
    cmd->End();
    mDevice->Submit(&cmd,1);
    mDevice->FreeDescriptorSet(set);
    mDevice->DestroyCommandBuffer(cmd);
}

// =============================================================================
// Activations (template pour les cas simples : 2 SSBOs + uniform n)
// =============================================================================
static void RunUnary(NkMLContext* ml, NkIDevice* dev, NkDescSetHandle layout,
                      const char* key, const char* src,
                      const NkTensor& in, NkTensor& out) {
    auto pipe = ml->GetOrCompilePipeline(key, src);
    if (!pipe.IsValid()) return;
    auto set = dev->AllocateDescriptorSet(layout);
    dev->BindUniformBuffer(set,0,in.buffer);
    dev->BindUniformBuffer(set,1,out.buffer);
    auto* cmd=dev->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
    cmd->Begin();
    cmd->BindComputePipeline(pipe);
    cmd->BindDescriptorSet(set,0);
    uint32 n=in.NumElements();
    cmd->PushConstants(NkShaderStage::NK_COMPUTE,0,sizeof(n),&n);
    cmd->Dispatch((n+255)/256,1,1);
    cmd->UAVBarrier(out.buffer);
    cmd->End();
    dev->Submit(&cmd,1);
    dev->FreeDescriptorSet(set);
    dev->DestroyCommandBuffer(cmd);
}

void NkMLContext::ReLU    (const NkTensor& in, NkTensor& out) {
    RunUnary(this,mDevice,mDefaultLayout,"ReLU",NkMLShaders::ReLU(),in,out);
}
void NkMLContext::GELU    (const NkTensor& in, NkTensor& out) {
    RunUnary(this,mDevice,mDefaultLayout,"GELU",NkMLShaders::GELU(),in,out);
}
void NkMLContext::Sigmoid (const NkTensor& in, NkTensor& out) {
    RunUnary(this,mDevice,mDefaultLayout,"Sigmoid",NkMLShaders::Sigmoid(),in,out);
}
void NkMLContext::Tanh    (const NkTensor& in, NkTensor& out) {
    RunUnary(this,mDevice,mDefaultLayout,"Tanh", R"(
#version 430
layout(local_size_x=256) in;
layout(std430,binding=0) readonly buffer In{float x[];};
layout(std430,binding=1) writeonly buffer Out{float y[];};
uniform uint n; void main(){uint i=gl_GlobalInvocationID.x;if(i<n)y[i]=tanh(x[i]);}
)",in,out);
}
void NkMLContext::Swish(const NkTensor& in, NkTensor& out) {
    RunUnary(this,mDevice,mDefaultLayout,"Swish",R"(
#version 430
layout(local_size_x=256) in;
layout(std430,binding=0) readonly buffer In{float x[];};
layout(std430,binding=1) writeonly buffer Out{float y[];};
uniform uint n; void main(){uint i=gl_GlobalInvocationID.x;if(i<n){float v=x[i];y[i]=v/(1.0+exp(-v));}}
)",in,out);
}
void NkMLContext::LeakyReLU(const NkTensor& in, NkTensor& out, float alpha) {
    auto pipe=GetOrCompilePipeline("LeakyReLU",R"(
#version 430
layout(local_size_x=256) in;
layout(std430,binding=0) readonly buffer In{float x[];};
layout(std430,binding=1) writeonly buffer Out{float y[];};
uniform uint n; uniform float alpha;
void main(){uint i=gl_GlobalInvocationID.x;if(i<n)y[i]=x[i]>0?x[i]:alpha*x[i];}
)");
    if (!pipe.IsValid()) return;
    auto set=mDevice->AllocateDescriptorSet(mDefaultLayout);
    mDevice->BindUniformBuffer(set,0,in.buffer);
    mDevice->BindUniformBuffer(set,1,out.buffer);
    auto* cmd=mDevice->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
    cmd->Begin(); cmd->BindComputePipeline(pipe); cmd->BindDescriptorSet(set,0);
    struct {uint32 n; float32 a;} pc{in.NumElements(),alpha};
    cmd->PushConstants(NkShaderStage::NK_COMPUTE,0,sizeof(pc),&pc);
    cmd->Dispatch((in.NumElements()+255)/256,1,1);
    cmd->UAVBarrier(out.buffer); cmd->End();
    mDevice->Submit(&cmd,1); mDevice->FreeDescriptorSet(set); mDevice->DestroyCommandBuffer(cmd);
}

// =============================================================================
// LayerNorm
// =============================================================================
void NkMLContext::LayerNorm(const NkTensor& in, const NkTensor& weight,
                             const NkTensor& bias2, NkTensor& out, float32 eps) {
    auto pipe=GetOrCompilePipeline("LayerNorm",NkMLShaders::LayerNorm());
    if (!pipe.IsValid()) return;
    // Layout 4 bindings
    NkDescriptorSetLayoutDesc ld;
    for(uint32 i=0;i<4;i++) ld.Add(i,NkDescriptorType::NK_STORAGE_BUFFER,NkShaderStage::NK_COMPUTE);
    auto layout=mDevice->CreateDescriptorSetLayout(ld);
    auto set=mDevice->AllocateDescriptorSet(layout);
    mDevice->BindUniformBuffer(set,0,in.buffer);
    mDevice->BindUniformBuffer(set,1,weight.buffer);
    mDevice->BindUniformBuffer(set,2,bias2.buffer);
    mDevice->BindUniformBuffer(set,3,out.buffer);
    auto* cmd=mDevice->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
    cmd->Begin(); cmd->BindComputePipeline(pipe); cmd->BindDescriptorSet(set,0);
    uint32 cols=in.Cols(); uint32 rows=in.NumElements()/cols;
    struct {uint32 rows,cols; float32 eps;} pc{rows,cols,eps};
    cmd->PushConstants(NkShaderStage::NK_COMPUTE,0,sizeof(pc),&pc);
    cmd->Dispatch(rows,1,1); // un workgroup par ligne
    cmd->UAVBarrier(out.buffer); cmd->End();
    mDevice->Submit(&cmd,1); mDevice->FreeDescriptorSet(set);
    mDevice->DestroyDescriptorSetLayout(layout); mDevice->DestroyCommandBuffer(cmd);
}

// =============================================================================
// AdamW step
// =============================================================================
NkMLContext::AdamState NkMLContext::CreateAdamState(const NkTensor& param) {
    AdamState s;
    s.m = CreateTensor(param.shape, false, "_adam_m");
    s.v = CreateTensor(param.shape, false, "_adam_v");
    FillZero(s.m); FillZero(s.v);
    s.t = 0;
    return s;
}
void NkMLContext::DestroyAdamState(AdamState& s) {
    DestroyTensor(s.m); DestroyTensor(s.v); s.t=0;
}

void NkMLContext::AdamWStep(NkTensor& param, const NkTensor& grad,
                             AdamState& state, const AdamConfig& cfg) {
    state.t++;
    auto pipe=GetOrCompilePipeline("AdamW",NkMLShaders::AdamWStep());
    if (!pipe.IsValid()) return;

    NkDescriptorSetLayoutDesc ld;
    for(uint32 i=0;i<4;i++) ld.Add(i,NkDescriptorType::NK_STORAGE_BUFFER,NkShaderStage::NK_COMPUTE);
    auto layout=mDevice->CreateDescriptorSetLayout(ld);
    auto set=mDevice->AllocateDescriptorSet(layout);
    mDevice->BindUniformBuffer(set,0,param.buffer);
    mDevice->BindUniformBuffer(set,1,grad.buffer);
    mDevice->BindUniformBuffer(set,2,state.m.buffer);
    mDevice->BindUniformBuffer(set,3,state.v.buffer);

    float32 beta1t = powf(cfg.beta1, (float32)state.t);
    float32 beta2t = powf(cfg.beta2, (float32)state.t);
    struct { uint32 n; float32 lr,beta1,beta2,eps,wd,beta1t,beta2t; }
        pc{param.NumElements(), cfg.lr, cfg.beta1, cfg.beta2,
           cfg.eps, cfg.weightDecay, beta1t, beta2t};

    auto* cmd=mDevice->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
    cmd->Begin(); cmd->BindComputePipeline(pipe); cmd->BindDescriptorSet(set,0);
    cmd->PushConstants(NkShaderStage::NK_COMPUTE,0,sizeof(pc),&pc);
    cmd->Dispatch((param.NumElements()+255)/256,1,1);
    cmd->UAVBarrier(param.buffer); cmd->End();
    mDevice->Submit(&cmd,1); mDevice->FreeDescriptorSet(set);
    mDevice->DestroyDescriptorSetLayout(layout); mDevice->DestroyCommandBuffer(cmd);
}

// =============================================================================
// MSE Loss
// =============================================================================
float32 NkMLContext::MSELoss(const NkTensor& pred, const NkTensor& target) {
    FillZero(mScratch);
    auto pipe=GetOrCompilePipeline("MSELoss",NkMLShaders::MSELoss());
    if (!pipe.IsValid()) return 0.f;

    auto set=mDevice->AllocateDescriptorSet(mDefaultLayout);
    mDevice->BindUniformBuffer(set,0,pred.buffer);
    mDevice->BindUniformBuffer(set,1,target.buffer);
    mDevice->BindUniformBuffer(set,2,mScratch.buffer);

    uint32 n=pred.NumElements();
    auto* cmd=mDevice->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
    cmd->Begin(); cmd->BindComputePipeline(pipe); cmd->BindDescriptorSet(set,0);
    cmd->PushConstants(NkShaderStage::NK_COMPUTE,0,sizeof(n),&n);
    cmd->Dispatch((n+255)/256,1,1);
    cmd->UAVBarrier(mScratch.buffer); cmd->End();
    mDevice->Submit(&cmd,1);
    mDevice->FreeDescriptorSet(set); mDevice->DestroyCommandBuffer(cmd);

    float32 sum=0.f;
    Download(mScratch,&sum,1);
    return sum / float32(n);
}

// =============================================================================
// Backward — SoftmaxCrossEntropy
// =============================================================================
void NkMLContext::SoftmaxCrossEntropyBackward(const NkTensor& logits,
                                               const NkTensor& labels,
                                               NkTensor& gradLogits) {
    auto pipe=GetOrCompilePipeline("CEBackward",NkMLShaders::CrossEntropyBackward());
    if (!pipe.IsValid()) return;
    auto set=mDevice->AllocateDescriptorSet(mDefaultLayout);
    mDevice->BindUniformBuffer(set,0,logits.buffer);
    mDevice->BindUniformBuffer(set,1,labels.buffer);
    mDevice->BindUniformBuffer(set,2,gradLogits.buffer);
    uint32 n=logits.NumElements();
    uint32 nc=logits.Cols();
    uint32 bs=logits.Rows();
    struct{uint32 n,nc,bs;}pc{n,nc,bs};
    auto* cmd=mDevice->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
    cmd->Begin(); cmd->BindComputePipeline(pipe); cmd->BindDescriptorSet(set,0);
    cmd->PushConstants(NkShaderStage::NK_COMPUTE,0,sizeof(pc),&pc);
    cmd->Dispatch((n+255)/256,1,1);
    cmd->UAVBarrier(gradLogits.buffer); cmd->End();
    mDevice->Submit(&cmd,1); mDevice->FreeDescriptorSet(set); mDevice->DestroyCommandBuffer(cmd);
}

// =============================================================================
// Utilitaires
// =============================================================================
void NkMLContext::Sync() { if (mDevice) mDevice->WaitIdle(); }

NkMLContext::TensorStats NkMLContext::ComputeStats(const NkTensor& t) {
    NkVector<float32> v; v.Resize(t.NumElements());
    Download(t, v.Data());
    float32 mn=v[0],mx=v[0],sum=0,sum2=0;
    for (usize i=0; i<v.Size(); i++) {
        float32 x=v[i];
        mn=NK_MIN(mn,x); mx=NK_MAX(mx,x); sum+=x; sum2+=x*x;
    }
    float32 mean=sum/v.Size();
    float32 stddev=sqrtf(sum2/v.Size()-mean*mean);
    return {mn,mx,mean,stddev};
}

void NkMLContext::PrintTensor(const NkTensor& t, uint32 maxElems,
                               const char* prefix) {
    uint32 n = NK_MIN(t.NumElements(), maxElems);
    NkVector<float32> v; v.Resize(n);
    mDevice->ReadBuffer(t.buffer, v.Data(), v.Size()*sizeof(float32));
    logger_src.Infof("%s%s %s: [", prefix, t.name.CStr(), t.shape.ToString().CStr());
    for (uint32 i=0;i<v.Size();i++) {
        logger_src.Infof("%.4f", v[i]);
        if (i+1<v.Size()) logger_src.Infof(", ");
    }
    if (t.NumElements()>maxElems) logger_src.Infof(" ...");
    logger_src.Infof("]\n");
}

bool NkMLContext::AllClose(const NkTensor& A, const NkTensor& B,
                            float32 atol, float32 rtol) {
    if (A.shape != B.shape) return false;
    NkVector<float32> va; va.Resize(A.NumElements());
    NkVector<float32> vb; vb.Resize(B.NumElements());
    Download(A,va.Data()); Download(B,vb.Data());
    for (uint32 i=0;i<va.Size();i++) {
        float32 diff=fabsf(va[i]-vb[i]);
        if (diff > atol + rtol*fabsf(vb[i])) return false;
    }
    return true;
}

NkTensor NkMLContext::Reshape(const NkTensor& t, const NkTensorShape& newShape) {
    assert(t.shape.NumElements() == newShape.NumElements());
    NkTensor r = t;
    r.shape = newShape;
    // Même buffer — pas de copie
    return r;
}

void NkMLContext::Copy(const NkTensor& src, NkTensor& dst) {
    NkBufferCopyRegion r{0,0,src.NumBytes()};
    auto* cmd=mDevice->CreateCommandBuffer();
    cmd->Begin(); cmd->CopyBuffer(src.buffer,dst.buffer,r); cmd->End();
    mDevice->Submit(&cmd,1); mDevice->DestroyCommandBuffer(cmd);
}

// MatMulBackward
void NkMLContext::MatMulBackward(const NkTensor& A, const NkTensor& B,
                                  const NkTensor& gradC,
                                  NkTensor& gradA, NkTensor& gradB) {
    // gradA = gradC × B^T
    MatMul(gradC, B, gradA, false, true);
    // gradB = A^T × gradC
    MatMul(A, gradC, gradB, true, false);
}

// ReLUBackward
void NkMLContext::ReLUBackward(const NkTensor& out, const NkTensor& gradOut,
                                NkTensor& gradIn) {
    auto pipe=GetOrCompilePipeline("ReLUBackward",R"(
#version 430
layout(local_size_x=256) in;
layout(std430,binding=0) readonly buffer Out{float y[];};
layout(std430,binding=1) readonly buffer GradOut{float go[];};
layout(std430,binding=2) writeonly buffer GradIn{float gi[];};
uniform uint n;
void main(){uint i=gl_GlobalInvocationID.x;if(i<n)gi[i]=y[i]>0?go[i]:0.0;}
)");
    if (!pipe.IsValid()) return;
    auto set=mDevice->AllocateDescriptorSet(mDefaultLayout);
    mDevice->BindUniformBuffer(set,0,out.buffer);
    mDevice->BindUniformBuffer(set,1,gradOut.buffer);
    mDevice->BindUniformBuffer(set,2,gradIn.buffer);
    uint32 n=out.NumElements();
    auto* cmd=mDevice->CreateCommandBuffer(NkCommandBufferType::NK_COMPUTE);
    cmd->Begin(); cmd->BindComputePipeline(pipe); cmd->BindDescriptorSet(set,0);
    cmd->PushConstants(NkShaderStage::NK_COMPUTE,0,sizeof(n),&n);
    cmd->Dispatch((n+255)/256,1,1);
    cmd->UAVBarrier(gradIn.buffer); cmd->End();
    mDevice->Submit(&cmd,1); mDevice->FreeDescriptorSet(set); mDevice->DestroyCommandBuffer(cmd);
}

} // namespace nkentseu
