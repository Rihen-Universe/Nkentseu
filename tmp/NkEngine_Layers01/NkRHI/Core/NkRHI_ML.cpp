// =============================================================================
// NkRHI_ML.cpp — Implémentation NkMLContext
// =============================================================================
#include "NkRHI_ML.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cassert>
#include <random>
#include <algorithm>

namespace nkentseu {

// =============================================================================
bool NkMLContext::Init(NkIDevice* device) {
    if (!device || !device->IsValid()) return false;
    mDevice = device;

    if (!device->GetCaps().computeShaders) {
        printf("[NkML] Erreur: le device ne supporte pas les compute shaders\n");
        return false;
    }

    // Layout par défaut : 4 SSBOs (entrée A, entrée B, entrée C, sortie)
    NkDescriptorSetLayoutDesc layoutDesc;
    for (uint32 i=0;i<4;i++)
        layoutDesc.Add(i, NkDescriptorType::StorageBuffer, NkShaderStage::Compute);
    mDefaultLayout = mDevice->CreateDescriptorSetLayout(layoutDesc);

    // Buffer scratch pour les réductions (1 float32 = 4 bytes)
    mScratch = CreateTensor({1}, false, "_scratch");

    mReady = true;
    printf("[NkML] Initialisé sur: %s\n", device->GetCaps().vramBytes
           ? "GPU" : "device");
    printf("[NkML] Max group size: %u | Shared mem: %u KB\n",
           device->GetCaps().maxComputeGroupSizeX,
           device->GetCaps().maxComputeSharedMemory/1024);
    return true;
}

void NkMLContext::Shutdown() {
    Sync();
    // Détruire tous les pipelines
    for (auto& [k,v] : mPipelineCache) {
        mDevice->DestroyPipeline(const_cast<NkPipelineHandle&>(v.pipeline));
        mDevice->DestroyShader (const_cast<NkShaderHandle&>  (v.shader));
    }
    mPipelineCache.clear();
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

bool NkMLContext::Upload(NkTensor& t, const float* data, uint32 count) {
    uint32 n = count>0 ? count : t.NumElements();
    return mDevice->WriteBuffer(t.buffer, data, n*sizeof(float));
}

bool NkMLContext::Download(const NkTensor& t, float* out, uint32 count) {
    uint32 n = count>0 ? count : t.NumElements();
    return mDevice->ReadBuffer(t.buffer, out, n*sizeof(float));
}

void NkMLContext::FillZero(NkTensor& t) {
    auto* cmd=mDevice->CreateCommandBuffer();
    cmd->Begin(); cmd->ClearBuffer(t.buffer,0); cmd->End();
    mDevice->Submit(&cmd,1); mDevice->DestroyCommandBuffer(cmd);
}
void NkMLContext::FillOnes (NkTensor& t) { FillConst(t,1.f); }
void NkMLContext::FillConst(NkTensor& t, float val) {
    std::vector<float> v(t.NumElements(), val);
    Upload(t, v.data());
}

void NkMLContext::InitXavier(NkTensor& t) {
    uint32 fan_in  = t.shape.Rank()>=2 ? t.shape[t.shape.Rank()-2] : t.NumElements();
    uint32 fan_out = t.shape[t.shape.Rank()-1];
    float limit = sqrtf(6.f / float(fan_in+fan_out));
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(-limit, limit);
    std::vector<float> v(t.NumElements());
    for (auto& x:v) x=dist(rng);
    Upload(t, v.data());
}

void NkMLContext::InitHe(NkTensor& t) {
    uint32 fan_in = t.shape.Rank()>=2 ? t.shape[t.shape.Rank()-2] : t.NumElements();
    float std_dev = sqrtf(2.f / float(fan_in));
    std::mt19937 rng(std::random_device{}());
    std::normal_distribution<float> dist(0.f, std_dev);
    std::vector<float> v(t.NumElements());
    for (auto& x:v) x=dist(rng);
    Upload(t, v.data());
}

// =============================================================================
// Compile/cache un pipeline compute
// =============================================================================
NkPipelineHandle NkMLContext::GetOrCompilePipeline(const char* key,
                                                     const char* glslSrc) {
    auto it = mPipelineCache.find(key);
    if (it != mPipelineCache.end()) return it->second.pipeline;

    NkShaderDesc sd;
    sd.AddGLSL(NkShaderStage::Compute, glslSrc, "main");
    sd.debugName = key;

    NkShaderHandle sh = mDevice->CreateShader(sd);
    if (!sh.IsValid()) {
        printf("[NkML] Erreur compilation shader: %s\n", key);
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

    auto* cmd = mDevice->CreateCommandBuffer(NkCommandBufferType::Compute);
    cmd->Begin();
    cmd->BindComputePipeline(pipe);
    cmd->BindDescriptorSet(set, 0);
    // Push constants: M, N, K, alpha, transA, transB
    struct PC { int M,N,K; float alpha; int transA,transB; } pc{(int)M,(int)N,(int)K,1.f,(int)transA,(int)transB};
    cmd->PushConstants(NkShaderStage::Compute,0,sizeof(pc),&pc);
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

    auto* cmd = mDevice->CreateCommandBuffer(NkCommandBufferType::Compute);
    cmd->Begin();
    cmd->BindComputePipeline(pipe);
    cmd->BindDescriptorSet(set,0);
    struct PC2 { uint32 rows,cols; } pc{C.Rows(), C.Cols()};
    cmd->PushConstants(NkShaderStage::Compute,0,sizeof(pc),&pc);
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
    auto* cmd=dev->CreateCommandBuffer(NkCommandBufferType::Compute);
    cmd->Begin();
    cmd->BindComputePipeline(pipe);
    cmd->BindDescriptorSet(set,0);
    uint32 n=in.NumElements();
    cmd->PushConstants(NkShaderStage::Compute,0,sizeof(n),&n);
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
    auto* cmd=mDevice->CreateCommandBuffer(NkCommandBufferType::Compute);
    cmd->Begin(); cmd->BindComputePipeline(pipe); cmd->BindDescriptorSet(set,0);
    struct {uint32 n; float a;} pc{in.NumElements(),alpha};
    cmd->PushConstants(NkShaderStage::Compute,0,sizeof(pc),&pc);
    cmd->Dispatch((in.NumElements()+255)/256,1,1);
    cmd->UAVBarrier(out.buffer); cmd->End();
    mDevice->Submit(&cmd,1); mDevice->FreeDescriptorSet(set); mDevice->DestroyCommandBuffer(cmd);
}

// =============================================================================
// LayerNorm
// =============================================================================
void NkMLContext::LayerNorm(const NkTensor& in, const NkTensor& weight,
                             const NkTensor& bias2, NkTensor& out, float eps) {
    auto pipe=GetOrCompilePipeline("LayerNorm",NkMLShaders::LayerNorm());
    if (!pipe.IsValid()) return;
    // Layout 4 bindings
    NkDescriptorSetLayoutDesc ld;
    for(uint32 i=0;i<4;i++) ld.Add(i,NkDescriptorType::StorageBuffer,NkShaderStage::Compute);
    auto layout=mDevice->CreateDescriptorSetLayout(ld);
    auto set=mDevice->AllocateDescriptorSet(layout);
    mDevice->BindUniformBuffer(set,0,in.buffer);
    mDevice->BindUniformBuffer(set,1,weight.buffer);
    mDevice->BindUniformBuffer(set,2,bias2.buffer);
    mDevice->BindUniformBuffer(set,3,out.buffer);
    auto* cmd=mDevice->CreateCommandBuffer(NkCommandBufferType::Compute);
    cmd->Begin(); cmd->BindComputePipeline(pipe); cmd->BindDescriptorSet(set,0);
    uint32 cols=in.Cols(); uint32 rows=in.NumElements()/cols;
    struct {uint32 rows,cols; float eps;} pc{rows,cols,eps};
    cmd->PushConstants(NkShaderStage::Compute,0,sizeof(pc),&pc);
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
    for(uint32 i=0;i<4;i++) ld.Add(i,NkDescriptorType::StorageBuffer,NkShaderStage::Compute);
    auto layout=mDevice->CreateDescriptorSetLayout(ld);
    auto set=mDevice->AllocateDescriptorSet(layout);
    mDevice->BindUniformBuffer(set,0,param.buffer);
    mDevice->BindUniformBuffer(set,1,grad.buffer);
    mDevice->BindUniformBuffer(set,2,state.m.buffer);
    mDevice->BindUniformBuffer(set,3,state.v.buffer);

    float beta1t = powf(cfg.beta1, (float)state.t);
    float beta2t = powf(cfg.beta2, (float)state.t);
    struct { uint32 n; float lr,beta1,beta2,eps,wd,beta1t,beta2t; }
        pc{param.NumElements(), cfg.lr, cfg.beta1, cfg.beta2,
           cfg.eps, cfg.weightDecay, beta1t, beta2t};

    auto* cmd=mDevice->CreateCommandBuffer(NkCommandBufferType::Compute);
    cmd->Begin(); cmd->BindComputePipeline(pipe); cmd->BindDescriptorSet(set,0);
    cmd->PushConstants(NkShaderStage::Compute,0,sizeof(pc),&pc);
    cmd->Dispatch((param.NumElements()+255)/256,1,1);
    cmd->UAVBarrier(param.buffer); cmd->End();
    mDevice->Submit(&cmd,1); mDevice->FreeDescriptorSet(set);
    mDevice->DestroyDescriptorSetLayout(layout); mDevice->DestroyCommandBuffer(cmd);
}

// =============================================================================
// MSE Loss
// =============================================================================
float NkMLContext::MSELoss(const NkTensor& pred, const NkTensor& target) {
    FillZero(mScratch);
    auto pipe=GetOrCompilePipeline("MSELoss",NkMLShaders::MSELoss());
    if (!pipe.IsValid()) return 0.f;

    auto set=mDevice->AllocateDescriptorSet(mDefaultLayout);
    mDevice->BindUniformBuffer(set,0,pred.buffer);
    mDevice->BindUniformBuffer(set,1,target.buffer);
    mDevice->BindUniformBuffer(set,2,mScratch.buffer);

    uint32 n=pred.NumElements();
    auto* cmd=mDevice->CreateCommandBuffer(NkCommandBufferType::Compute);
    cmd->Begin(); cmd->BindComputePipeline(pipe); cmd->BindDescriptorSet(set,0);
    cmd->PushConstants(NkShaderStage::Compute,0,sizeof(n),&n);
    cmd->Dispatch((n+255)/256,1,1);
    cmd->UAVBarrier(mScratch.buffer); cmd->End();
    mDevice->Submit(&cmd,1);
    mDevice->FreeDescriptorSet(set); mDevice->DestroyCommandBuffer(cmd);

    float sum=0.f;
    Download(mScratch,&sum,1);
    return sum / float(n);
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
    auto* cmd=mDevice->CreateCommandBuffer(NkCommandBufferType::Compute);
    cmd->Begin(); cmd->BindComputePipeline(pipe); cmd->BindDescriptorSet(set,0);
    cmd->PushConstants(NkShaderStage::Compute,0,sizeof(pc),&pc);
    cmd->Dispatch((n+255)/256,1,1);
    cmd->UAVBarrier(gradLogits.buffer); cmd->End();
    mDevice->Submit(&cmd,1); mDevice->FreeDescriptorSet(set); mDevice->DestroyCommandBuffer(cmd);
}

// =============================================================================
// Utilitaires
// =============================================================================
void NkMLContext::Sync() { if (mDevice) mDevice->WaitIdle(); }

NkMLContext::TensorStats NkMLContext::ComputeStats(const NkTensor& t) {
    std::vector<float> v(t.NumElements());
    Download(t, v.data());
    float mn=v[0],mx=v[0],sum=0,sum2=0;
    for (auto x:v) { mn=std::min(mn,x); mx=std::max(mx,x); sum+=x; sum2+=x*x; }
    float mean=sum/v.size();
    float std=sqrtf(sum2/v.size()-mean*mean);
    return {mn,mx,mean,std};
}

void NkMLContext::PrintTensor(const NkTensor& t, uint32 maxElems,
                               const char* prefix) {
    std::vector<float> v(std::min(t.NumElements(),maxElems));
    mDevice->ReadBuffer(t.buffer, v.data(), v.size()*sizeof(float));
    printf("%s%s %s: [", prefix, t.name.c_str(), t.shape.ToString().c_str());
    for (uint32 i=0;i<v.size();i++) {
        printf("%.4f", v[i]);
        if (i+1<v.size()) printf(", ");
    }
    if (t.NumElements()>maxElems) printf(" ...");
    printf("]\n");
}

bool NkMLContext::AllClose(const NkTensor& A, const NkTensor& B,
                            float atol, float rtol) {
    if (A.shape != B.shape) return false;
    std::vector<float> va(A.NumElements()), vb(B.NumElements());
    Download(A,va.data()); Download(B,vb.data());
    for (uint32 i=0;i<va.size();i++) {
        float diff=fabsf(va[i]-vb[i]);
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
    auto* cmd=mDevice->CreateCommandBuffer(NkCommandBufferType::Compute);
    cmd->Begin(); cmd->BindComputePipeline(pipe); cmd->BindDescriptorSet(set,0);
    cmd->PushConstants(NkShaderStage::Compute,0,sizeof(n),&n);
    cmd->Dispatch((n+255)/256,1,1);
    cmd->UAVBarrier(gradIn.buffer); cmd->End();
    mDevice->Submit(&cmd,1); mDevice->FreeDescriptorSet(set); mDevice->DestroyCommandBuffer(cmd);
}

} // namespace nkentseu
