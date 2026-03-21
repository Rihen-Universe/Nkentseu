#pragma once
// =============================================================================
// NkRHI_ML.h
// Interface ML/Deep Learning construite sur NkIDevice (RHI couche 1).
//
// Ce module prouve que le même device GPU qui rend vos images
// peut aussi entraîner des réseaux de neurones, sans quitter NkEngine.
//
// Opérations disponibles :
//   • MatMul (GEMM)           — matrix multiply A×B → C
//   • MatMulAdd (GEMM+bias)   — A×B + bias → C
//   • Conv2D                  — convolution 2D forward
//   • ReLU, GELU, Sigmoid, Tanh, Softmax — activations
//   • LayerNorm, BatchNorm    — normalisations
//   • Attention (Scaled Dot Product) — pour les Transformers
//   • AdamW Optimizer step    — mise à jour des poids
//   • MSE Loss, CrossEntropy  — fonctions de perte
//
// Tout tourne en GLSL compute shaders sur le device RHI existant.
// Pas de dépendance externe (pas de CUDA, pas de cuDNN).
// Compatible OpenGL 4.3+, Vulkan, DX12, Metal.
//
// Utilisation typique :
//   NkMLContext ml;
//   ml.Init(device);
//   NkTensor A = ml.CreateTensor({batch, in});
//   NkTensor B = ml.CreateTensor({in, out});
//   NkTensor C = ml.CreateTensor({batch, out});
//   ml.Upload(A, dataPtr);
//   ml.MatMul(A, B, C);
//   ml.ReLU(C, C);        // in-place
//   float loss = ml.MSELoss(C, target);
// =============================================================================
#include "../Core/NkIDevice.h"
#include <vector>
#include <string>
#include <functional>
#include <memory>

namespace nkentseu {

// =============================================================================
// TensorShape — dimensions d'un tenseur
// =============================================================================
struct NkTensorShape {
    std::vector<uint32> dims;   // [N, C, H, W] ou [batch, seq, d_model] etc.

    NkTensorShape() = default;
    NkTensorShape(std::initializer_list<uint32> d) : dims(d) {}
    explicit NkTensorShape(const std::vector<uint32>& d) : dims(d) {}

    uint32 NumElements() const {
        if (dims.empty()) return 0;
        uint32 n=1; for (auto d:dims) n*=d; return n;
    }
    uint32 NumBytes()    const { return NumElements()*sizeof(float); }
    uint32 Rank()        const { return (uint32)dims.size(); }
    uint32 operator[](uint32 i) const { return dims[i]; }

    bool operator==(const NkTensorShape& o) const { return dims==o.dims; }
    bool operator!=(const NkTensorShape& o) const { return dims!=o.dims; }

    std::string ToString() const {
        std::string s="[";
        for (uint32 i=0;i<dims.size();i++) {
            s+=std::to_string(dims[i]);
            if (i+1<dims.size()) s+=",";
        }
        return s+"]";
    }
};

// =============================================================================
// NkTensor — tenseur GPU (wrapper autour d'un NkBufferHandle)
// =============================================================================
struct NkTensor {
    NkBufferHandle  buffer;
    NkTensorShape   shape;
    bool            requiresGrad = false;
    NkBufferHandle  grad;        // buffer du gradient (si requiresGrad)
    std::string     name;

    bool IsValid() const { return buffer.IsValid(); }
    uint32 NumElements() const { return shape.NumElements(); }
    uint32 NumBytes()    const { return shape.NumBytes(); }

    // Shape helpers
    uint32 Rows()    const { return shape.Rank()>=2 ? shape[shape.Rank()-2] : 1; }
    uint32 Cols()    const { return shape.Rank()>=1 ? shape[shape.Rank()-1] : 1; }
    uint32 Batch()   const { return shape.Rank()>=3 ? shape[0] : 1; }
};

// =============================================================================
// NkMLContext — contexte ML/compute construit sur NkIDevice
// =============================================================================
class NkMLContext {
public:
    NkMLContext()  = default;
    ~NkMLContext() { if (mReady) Shutdown(); }

    bool Init   (NkIDevice* device);
    void Shutdown();
    bool IsReady() const { return mReady; }

    // =========================================================================
    // Gestion des tenseurs
    // =========================================================================
    NkTensor CreateTensor(const NkTensorShape& shape,
                           bool requiresGrad=false,
                           const char* name=nullptr);

    NkTensor CreateTensor(std::initializer_list<uint32> dims,
                           bool requiresGrad=false,
                           const char* name=nullptr) {
        return CreateTensor(NkTensorShape(dims), requiresGrad, name);
    }

    void DestroyTensor(NkTensor& t);

    // Upload CPU → GPU
    bool Upload(NkTensor& t, const float* data, uint32 count=0);
    // Download GPU → CPU
    bool Download(const NkTensor& t, float* out, uint32 count=0);

    // Initialisation des poids
    void FillZero (NkTensor& t);
    void FillOnes (NkTensor& t);
    void FillConst(NkTensor& t, float val);
    // Xavier / He initialization (important pour la convergence)
    void InitXavier(NkTensor& t);   // √(2/(fan_in+fan_out))
    void InitHe    (NkTensor& t);   // √(2/fan_in) — pour ReLU

    // =========================================================================
    // Opérations fondamentales (Forward pass)
    // =========================================================================

    // ── Algèbre linéaire ──────────────────────────────────────────────────────
    // C = A × B    (A: [M,K], B: [K,N], C: [M,N])
    void MatMul(const NkTensor& A, const NkTensor& B, NkTensor& C,
                bool transposeA=false, bool transposeB=false);

    // C = A × B + bias   (bias: [N] — broadcasted)
    void MatMulAdd(const NkTensor& A, const NkTensor& B,
                   const NkTensor& bias, NkTensor& C);

    // Batch MatMul  C[b] = A[b] × B[b]   (A,B,C: [batch, M/K, K/N])
    void BatchMatMul(const NkTensor& A, const NkTensor& B, NkTensor& C,
                      bool transposeA=false, bool transposeB=false);

    // C = alpha*A + beta*B
    void Add(const NkTensor& A, const NkTensor& B, NkTensor& C,
              float alpha=1.f, float beta=1.f);

    // C = A ⊙ B  (element-wise multiply)
    void Mul(const NkTensor& A, const NkTensor& B, NkTensor& C);

    // Transpose: B = A^T  (A: [M,N] → B: [N,M])
    void Transpose(const NkTensor& A, NkTensor& B);

    // ── Convolution ───────────────────────────────────────────────────────────
    struct Conv2DParams {
        uint32 strideH=1, strideW=1;
        uint32 padH=0, padW=0;
        uint32 dilationH=1, dilationW=1;
        uint32 groups=1;
    };
    // out = Conv2D(input, weight) + bias
    // input:  [N, C_in,  H,   W]
    // weight: [C_out, C_in/groups, kH, kW]
    // bias:   [C_out]
    // out:    [N, C_out, H_out, W_out]
    void Conv2D(const NkTensor& input, const NkTensor& weight,
                const NkTensor& bias,  NkTensor& output,
                const Conv2DParams& p={});

    // ── Activations ───────────────────────────────────────────────────────────
    void ReLU    (const NkTensor& in, NkTensor& out);
    void LeakyReLU(const NkTensor& in, NkTensor& out, float alpha=0.01f);
    void GELU    (const NkTensor& in, NkTensor& out);  // x*Φ(x) exact
    void Sigmoid (const NkTensor& in, NkTensor& out);
    void Tanh    (const NkTensor& in, NkTensor& out);
    void Softmax (const NkTensor& in, NkTensor& out, int32 axis=-1);
    void LogSoftmax(const NkTensor& in, NkTensor& out, int32 axis=-1);
    void Swish   (const NkTensor& in, NkTensor& out);  // x*sigmoid(x)
    void SiLU    (const NkTensor& in, NkTensor& out) { Swish(in,out); }

    // ── Normalisations ────────────────────────────────────────────────────────
    // LayerNorm : normalise sur la dernière dimension
    // out = (in - mean) / sqrt(var + eps) * weight + bias
    void LayerNorm(const NkTensor& in,
                   const NkTensor& weight, const NkTensor& bias,
                   NkTensor& out, float eps=1e-5f);

    void BatchNorm(const NkTensor& in,
                   const NkTensor& weight, const NkTensor& bias,
                   const NkTensor& runningMean, const NkTensor& runningVar,
                   NkTensor& out, float eps=1e-5f, bool training=false);

    void RMSNorm(const NkTensor& in, const NkTensor& weight,
                  NkTensor& out, float eps=1e-5f);

    // ── Attention (Transformer) ───────────────────────────────────────────────
    // Scaled Dot-Product Attention:
    // Attention(Q,K,V) = softmax(Q×K^T / √d_k) × V
    // Q,K,V : [batch, heads, seq, d_head]
    // out   : [batch, heads, seq, d_head]
    void ScaledDotProductAttention(const NkTensor& Q, const NkTensor& K,
                                    const NkTensor& V, NkTensor& out,
                                    float scale=0.f,
                                    const NkTensor* mask=nullptr);

    // ── Embedding ─────────────────────────────────────────────────────────────
    // Lookup : out[i] = table[indices[i]]
    // table:   [vocab_size, embed_dim]
    // indices: [batch, seq] (uint32)
    void EmbeddingLookup(const NkTensor& table,
                          const NkTensor& indices,
                          NkTensor& out);

    // ── Dropout (inference = no-op) ───────────────────────────────────────────
    void Dropout(const NkTensor& in, NkTensor& out,
                  float rate=0.1f, bool training=false);

    // ── Pooling ───────────────────────────────────────────────────────────────
    void MaxPool2D(const NkTensor& in, NkTensor& out,
                    uint32 kH, uint32 kW, uint32 strideH, uint32 strideW);
    void AvgPool2D(const NkTensor& in, NkTensor& out,
                    uint32 kH, uint32 kW, uint32 strideH, uint32 strideW);
    void GlobalAvgPool(const NkTensor& in, NkTensor& out); // [N,C,H,W]→[N,C]

    // ─── Reshape / Slice ─────────────────────────────────────────────────────
    // Reshape ne copie pas — change juste le shape (doit avoir même NumElements)
    NkTensor Reshape(const NkTensor& t, const NkTensorShape& newShape);
    void     Copy   (const NkTensor& src, NkTensor& dst);

    // =========================================================================
    // Fonctions de perte
    // =========================================================================
    // Retourne la loss scalaire sur CPU (téléchargement implicite)
    float MSELoss       (const NkTensor& pred, const NkTensor& target);
    float MAELoss       (const NkTensor& pred, const NkTensor& target);
    float CrossEntropy  (const NkTensor& logits, const NkTensor& labels);
    float BCELoss       (const NkTensor& pred, const NkTensor& target);
    float CosineSimilarity(const NkTensor& A, const NkTensor& B);

    // =========================================================================
    // Backward pass (gradients)
    // =========================================================================
    // Gradient de ReLU : dout = din * (out > 0)
    void ReLUBackward(const NkTensor& out, const NkTensor& gradOut,
                       NkTensor& gradIn);

    // Gradient de MatMul :
    //   gradA = gradC × B^T
    //   gradB = A^T × gradC
    void MatMulBackward(const NkTensor& A,    const NkTensor& B,
                         const NkTensor& gradC,
                         NkTensor& gradA, NkTensor& gradB);

    // Gradient de LayerNorm
    void LayerNormBackward(const NkTensor& in, const NkTensor& weight,
                            const NkTensor& gradOut, float eps,
                            NkTensor& gradIn, NkTensor& gradWeight,
                            NkTensor& gradBias);

    // Gradient de SoftmaxCrossEntropy (fusionné pour la stabilité numérique)
    // grad[i] = (softmax[i] - label[i]) / batch_size
    void SoftmaxCrossEntropyBackward(const NkTensor& logits,
                                      const NkTensor& labels,
                                      NkTensor& gradLogits);

    // =========================================================================
    // Optimiseurs
    // =========================================================================
    struct AdamConfig {
        float lr      = 1e-3f;
        float beta1   = 0.9f;
        float beta2   = 0.999f;
        float eps     = 1e-8f;
        float weightDecay = 0.01f; // AdamW weight decay
    };

    // État de l'optimiseur (momentum, variance) — un par paramètre
    struct AdamState {
        NkTensor m;   // premier moment (momentum)
        NkTensor v;   // deuxième moment (variance)
        uint32   t=0; // pas de temps (pour la correction du biais)
    };

    AdamState CreateAdamState(const NkTensor& param);
    void      DestroyAdamState(AdamState& state);

    // Un step AdamW : met à jour param en place avec son gradient
    // param -= lr * m_hat / (sqrt(v_hat) + eps) + lr*wd*param
    void AdamWStep(NkTensor& param, const NkTensor& grad,
                    AdamState& state, const AdamConfig& cfg);

    // SGD avec momentum
    struct SGDConfig { float lr=0.01f, momentum=0.9f, weightDecay=0.f; };
    struct SGDState  { NkTensor velocity; };
    SGDState CreateSGDState(const NkTensor& param);
    void     SGDStep(NkTensor& param, const NkTensor& grad,
                      SGDState& state, const SGDConfig& cfg);

    // =========================================================================
    // Utilitaires
    // =========================================================================
    // Synchronise le GPU (attend la fin de tous les compute shaders)
    void Sync();

    // Statistiques sur un tenseur (min, max, mean, std)
    struct TensorStats { float min, max, mean, std; };
    TensorStats ComputeStats(const NkTensor& t);

    // Affiche les premières valeurs d'un tenseur (pour debug)
    void PrintTensor(const NkTensor& t, uint32 maxElems=16,
                      const char* prefix="");

    // Vérifie que deux tenseurs sont proches (pour les tests unitaires)
    bool AllClose(const NkTensor& A, const NkTensor& B,
                   float atol=1e-4f, float rtol=1e-4f);

private:
    // ── Compile un shader compute GLSL ────────────────────────────────────────
    NkPipelineHandle GetOrCompilePipeline(const char* key, const char* glslSrc);

    // ── Dispatch helper (calcule les groupes depuis le nombre d'éléments) ─────
    void Dispatch1D(NkICommandBuffer* cmd, NkPipelineHandle pipe,
                     uint32 n, uint32 groupSize=256);

    // ── Bind un tenseur comme SSBO sur un binding donné ──────────────────────
    void BindTensor(NkDescSetHandle set, uint32 binding, const NkTensor& t);

    NkIDevice*  mDevice = nullptr;
    bool        mReady  = false;

    // Cache des pipelines compilés (key=nom de l'opération)
    struct PipelineCache {
        NkShaderHandle   shader;
        NkPipelineHandle pipeline;
    };
    NkUnorderedMap<std::string, PipelineCache> mPipelineCache;

    // Layout des descriptors (2 SSBO input + 1 SSBO output par défaut)
    NkDescSetHandle mDefaultLayout;

    // Buffer scratch pour les réductions (loss, stats)
    NkTensor mScratch;
};

// =============================================================================
// Shaders GLSL compute pour chaque opération (dans des namespaces séparés)
// =============================================================================
namespace NkMLShaders {

// ── MatMul (tuile 16×16 pour la localité cache) ───────────────────────────────
inline const char* MatMul() { return R"(
#version 430
layout(local_size_x=16, local_size_y=16) in;

layout(std430, binding=0) readonly  buffer A { float a[]; };
layout(std430, binding=1) readonly  buffer B { float b[]; };
layout(std430, binding=2) writeonly buffer C { float c[]; };

uniform int M, N, K;
uniform float alpha;   // scalaire de A×B
uniform int transA, transB;

shared float tileA[16][16];
shared float tileB[16][16];

void main() {
    uint row = gl_GlobalInvocationID.y;
    uint col = gl_GlobalInvocationID.x;
    float sum = 0.0;

    for (int t=0; t<(K+15)/16; t++) {
        // Charger les tuiles en shared memory
        uint aRow = transA==0 ? row          : t*16+gl_LocalInvocationID.x;
        uint aCol = transA==0 ? t*16+gl_LocalInvocationID.x : row;
        uint bRow = transB==0 ? t*16+gl_LocalInvocationID.y : col;
        uint bCol = transB==0 ? col          : t*16+gl_LocalInvocationID.y;

        tileA[gl_LocalInvocationID.y][gl_LocalInvocationID.x] =
            (aRow<uint(M) && aCol<uint(K)) ? a[aRow*K+aCol] : 0.0;
        tileB[gl_LocalInvocationID.y][gl_LocalInvocationID.x] =
            (bRow<uint(K) && bCol<uint(N)) ? b[bRow*N+bCol] : 0.0;

        barrier();
        for (int k=0; k<16; k++) sum += tileA[gl_LocalInvocationID.y][k]
                                       * tileB[k][gl_LocalInvocationID.x];
        barrier();
    }
    if (row<uint(M) && col<uint(N))
        c[row*N+col] = alpha * sum;
}
)"; }

// ── MatMulAdd (GEMM + bias) ───────────────────────────────────────────────────
inline const char* MatMulAdd() { return R"(
#version 430
layout(local_size_x=256) in;
layout(std430,binding=0) readonly  buffer A    { float a[]; };
layout(std430,binding=1) readonly  buffer B    { float b[]; };
layout(std430,binding=2) readonly  buffer Bias { float bias[]; };
layout(std430,binding=3) writeonly buffer C    { float c[]; };
uniform int M,N,K;
shared float tileA[16][16];
shared float tileB[16][16];
void main(){
    uint row=gl_GlobalInvocationID.y, col=gl_GlobalInvocationID.x;
    float sum=0;
    for(int t=0;t<(K+15)/16;t++){
        tileA[gl_LocalInvocationID.y][gl_LocalInvocationID.x]=
            (row<uint(M)&&t*16+gl_LocalInvocationID.x<uint(K))?a[row*K+t*16+gl_LocalInvocationID.x]:0;
        tileB[gl_LocalInvocationID.y][gl_LocalInvocationID.x]=
            (t*16+gl_LocalInvocationID.y<uint(K)&&col<uint(N))?b[(t*16+gl_LocalInvocationID.y)*N+col]:0;
        barrier();
        for(int k=0;k<16;k++) sum+=tileA[gl_LocalInvocationID.y][k]*tileB[k][gl_LocalInvocationID.x];
        barrier();
    }
    if(row<uint(M)&&col<uint(N)) c[row*N+col]=sum+(col<uint(N)?bias[col]:0);
}
)"; }

// ── ReLU ─────────────────────────────────────────────────────────────────────
inline const char* ReLU() { return R"(
#version 430
layout(local_size_x=256) in;
layout(std430,binding=0) readonly  buffer In  { float x[]; };
layout(std430,binding=1) writeonly buffer Out { float y[]; };
uniform uint n;
void main(){ uint i=gl_GlobalInvocationID.x; if(i<n) y[i]=max(0.0,x[i]); }
)"; }

// ── GELU (exact) ──────────────────────────────────────────────────────────────
inline const char* GELU() { return R"(
#version 430
layout(local_size_x=256) in;
layout(std430,binding=0) readonly  buffer In  { float x[]; };
layout(std430,binding=1) writeonly buffer Out { float y[]; };
uniform uint n;
// GELU exact : x * 0.5 * (1 + erf(x/sqrt(2)))
void main(){
    uint i=gl_GlobalInvocationID.x;
    if(i<n){ float v=x[i]; y[i]=v*0.5*(1.0+erf(v*0.70710678)); }
}
)"; }

// ── Sigmoid ───────────────────────────────────────────────────────────────────
inline const char* Sigmoid() { return R"(
#version 430
layout(local_size_x=256) in;
layout(std430,binding=0) readonly  buffer In  { float x[]; };
layout(std430,binding=1) writeonly buffer Out { float y[]; };
uniform uint n;
void main(){ uint i=gl_GlobalInvocationID.x; if(i<n) y[i]=1.0/(1.0+exp(-x[i])); }
)"; }

// ── Softmax (réduction par ligne) ────────────────────────────────────────────
inline const char* Softmax() { return R"(
#version 430
layout(local_size_x=256) in;
layout(std430,binding=0) readonly  buffer In  { float x[]; };
layout(std430,binding=1) writeonly buffer Out { float y[]; };
uniform uint rows, cols;
shared float smax, ssum;
void main(){
    uint row=gl_WorkGroupID.x;
    if(row>=rows) return;
    uint base=row*cols;
    // Pass 1 : max (pour stabilité numérique)
    float lmax=-1e30;
    for(uint c=gl_LocalInvocationID.x;c<cols;c+=256) lmax=max(lmax,x[base+c]);
    // Réduction dans le warp
    for(int s=128;s>0;s>>=1) { barrier(); float v=subgroupShuffle(lmax,gl_LocalInvocationID.x^uint(s)); lmax=max(lmax,v); }
    if(gl_LocalInvocationID.x==0) smax=lmax;
    barrier();
    // Pass 2 : sum exp
    float lsum=0;
    for(uint c=gl_LocalInvocationID.x;c<cols;c+=256) lsum+=exp(x[base+c]-smax);
    for(int s=128;s>0;s>>=1){ barrier(); lsum+=subgroupShuffle(lsum,gl_LocalInvocationID.x^uint(s)); }
    if(gl_LocalInvocationID.x==0) ssum=lsum;
    barrier();
    // Pass 3 : normalise
    for(uint c=gl_LocalInvocationID.x;c<cols;c+=256) y[base+c]=exp(x[base+c]-smax)/ssum;
}
)"; }

// ── LayerNorm ─────────────────────────────────────────────────────────────────
inline const char* LayerNorm() { return R"(
#version 430
layout(local_size_x=256) in;
layout(std430,binding=0) readonly  buffer In     { float x[]; };
layout(std430,binding=1) readonly  buffer Weight { float w[]; };
layout(std430,binding=2) readonly  buffer Bias2  { float b[]; };
layout(std430,binding=3) writeonly buffer Out    { float y[]; };
uniform uint rows, cols;
uniform float eps;
shared float smean, svar;
void main(){
    uint row=gl_WorkGroupID.x; if(row>=rows) return;
    uint base=row*cols;
    float lmean=0,lvar=0;
    for(uint c=gl_LocalInvocationID.x;c<cols;c+=256) lmean+=x[base+c];
    // reduce mean
    for(int s=128;s>0;s>>=1){ barrier(); lmean+=subgroupShuffle(lmean,gl_LocalInvocationID.x^uint(s)); }
    if(gl_LocalInvocationID.x==0) smean=lmean/float(cols);
    barrier();
    for(uint c=gl_LocalInvocationID.x;c<cols;c+=256){ float d=x[base+c]-smean; lvar+=d*d; }
    for(int s=128;s>0;s>>=1){ barrier(); lvar+=subgroupShuffle(lvar,gl_LocalInvocationID.x^uint(s)); }
    if(gl_LocalInvocationID.x==0) svar=lvar/float(cols);
    barrier();
    float invstd=1.0/sqrt(svar+eps);
    for(uint c=gl_LocalInvocationID.x;c<cols;c+=256)
        y[base+c]=(x[base+c]-smean)*invstd*w[c]+b[c];
}
)"; }

// ── AdamW step ────────────────────────────────────────────────────────────────
inline const char* AdamWStep() { return R"(
#version 430
layout(local_size_x=256) in;
layout(std430,binding=0) buffer Param { float param[]; };
layout(std430,binding=1) readonly buffer Grad { float grad[]; };
layout(std430,binding=2) buffer M1    { float m[]; };  // premier moment
layout(std430,binding=3) buffer V1    { float v[]; };  // deuxième moment
uniform uint n;
uniform float lr, beta1, beta2, eps, wd, beta1t, beta2t; // beta1^t, beta2^t
void main(){
    uint i=gl_GlobalInvocationID.x; if(i>=n) return;
    float g=grad[i];
    // Weight decay (AdamW : appliqué sur param avant la mise à jour Adam)
    param[i]*=(1.0-lr*wd);
    // Moments
    m[i]=beta1*m[i]+(1.0-beta1)*g;
    v[i]=beta2*v[i]+(1.0-beta2)*g*g;
    // Bias correction
    float mh=m[i]/(1.0-beta1t);
    float vh=v[i]/(1.0-beta2t);
    param[i]-=lr*mh/(sqrt(vh)+eps);
}
)"; }

// ── MSE Loss (réduction) ─────────────────────────────────────────────────────
inline const char* MSELoss() { return R"(
#version 430
layout(local_size_x=256) in;
layout(std430,binding=0) readonly  buffer Pred   { float pred[]; };
layout(std430,binding=1) readonly  buffer Target { float tgt[]; };
layout(std430,binding=2) buffer    Result        { float result[]; };
uniform uint n;
shared float ssum[256];
void main(){
    uint i=gl_GlobalInvocationID.x;
    float d=i<n ? pred[i]-tgt[i] : 0.0;
    ssum[gl_LocalInvocationID.x]=d*d;
    barrier();
    for(uint s=128;s>0;s>>=1){ if(gl_LocalInvocationID.x<s) ssum[gl_LocalInvocationID.x]+=ssum[gl_LocalInvocationID.x+s]; barrier(); }
    if(gl_LocalInvocationID.x==0) atomicAdd(result[0],ssum[0]);
}
)"; }

// ── SoftmaxCrossEntropy Backward ─────────────────────────────────────────────
inline const char* CrossEntropyBackward() { return R"(
#version 430
layout(local_size_x=256) in;
layout(std430,binding=0) readonly  buffer Logits { float logits[]; };
layout(std430,binding=1) readonly  buffer Labels { float labels[]; };
layout(std430,binding=2) writeonly buffer GradOut{ float grad[]; };
uniform uint n, num_classes, batch_size;
void main(){
    uint i=gl_GlobalInvocationID.x; if(i>=n) return;
    uint b=i/num_classes, c=i%num_classes;
    // Softmax en ligne (simplifié — en pratique on pré-calcule)
    float mx=logits[b*num_classes];
    for(uint j=0;j<num_classes;j++) mx=max(mx,logits[b*num_classes+j]);
    float s=0;
    for(uint j=0;j<num_classes;j++) s+=exp(logits[b*num_classes+j]-mx);
    float p=exp(logits[i]-mx)/s;
    grad[i]=(p-labels[i])/float(batch_size);
}
)"; }

} // namespace NkMLShaders

} // namespace nkentseu
