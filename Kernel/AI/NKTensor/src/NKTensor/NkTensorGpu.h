// =============================================================================
// NkTensorGpu.h — contexte GPU de NKTensor (Jalon 3).
//
// Encapsule le chemin compute PROUVÉ : kernel écrit en NkSL -> GLSL-Vulkan ->
// (glslang -> SPIR-V -> SPIRV-Cross) -> HLSL/SPIRV/MSL -> pipeline compute NKRHI.
// Le tenseur ne connaît pas NKRHI : il ne manipule que des handles opaques (uint64)
// de buffers GPU gérés ici. Device headless (compute-only, sans fenêtre).
//
// Tout est derrière un pimpl : NkTensor.h/.cpp restent CPU-only et sans dépendance
// NKRHI ; seul NkTensorGpu.cpp tire NKRHI + NKSL.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace ai {

        class NkTensor;   // forward (défini dans NkTensor.h)

        class NkTensorGpu {
        public:
            // Singleton paresseux. Le device GPU n'est créé qu'au premier usage.
            static NkTensorGpu& Get();

            // Un device compute headless est-il disponible sur cette machine ?
            bool        IsAvailable();
            const char* BackendName();   // "DirectX 11" / "Vulkan" / … / "none"

            // ---- Buffers GPU (stockage des tenseurs) ---------------------------
            // Retourne un id opaque (>0) ou 0 en échec. Le buffer est un storage
            // buffer (SSBO / UAV) utilisable en compute et relisible par le CPU.
            uint64 CreateBuffer(nk_size bytes);
            void   DestroyBuffer(uint64 id);
            bool   Upload  (uint64 id, const void* data, nk_size bytes);
            bool   Download(uint64 id, void* out,        nk_size bytes);

            // ---- Kernels ------------------------------------------------------
            // Élémentaire binaire : C = f(A, B) sur `count` éléments f32.
            // Le kernel NkSL doit déclarer buffers 0,1,2 (A,B,C) + UBO binding 3
            // { uint count }, workgroup local_size_x=64. Compilé et mis en cache par nom.
            bool RunBinary(const char* name, const NkString& nkslSrc,
                           uint64 a, uint64 b, uint64 c, uint32 count);

            // Élémentaire unaire : B = f(A) sur `count` éléments f32.
            // Kernel : buffers 0,1 (A,B) + UBO binding 2 { uint count }.
            bool RunUnary(const char* name, const NkString& nkslSrc,
                          uint64 a, uint64 b, uint32 count);

            // Unaire avec scalaire : B = f(A, s). Kernel : buffers 0,1 + UBO binding 2
            // { uint count; float s; }. Pour mulscalar / addscalar / step.
            bool RunUnaryScalar(const char* name, const NkString& nkslSrc,
                                uint64 a, uint64 b, uint32 count, float s);

            // MatMul C[M,N] = A[M,K] · B[K,N] (row-major, f32).
            // Kernel : buffers 0,1,2 (A,B,C) + UBO binding 3 { uint M,N,K },
            // workgroup 16x16.
            bool RunMatMul(uint64 a, uint64 b, uint64 c, uint32 M, uint32 N, uint32 K);

            // Réduction segmentée : vue [outer, reduce, inner], sortie [outer, inner]
            // (out[o,i] = reduce_r in[o*reduce*inner + r*inner + i]). Couvre toutes les
            // réductions d'axe (axe 0 : outer=1 ; dernier axe : inner=1 ; globale :
            // outer=inner=1). Kernel : buffers 0,1 (A,B) + UBO binding 2 { uint outer,
            // reduce, inner }, un thread par élément de sortie.
            bool RunReduce(const char* name, const NkString& nkslSrc,
                           uint64 a, uint64 out, uint32 outer, uint32 reduce, uint32 inner);

            // Matérialisation contiguë d'une vue strided (gather par strides). Chaque
            // thread lit un élément de sortie à src = offset + Σ coord_d · stride_d.
            // Couvre permute/transpose sur GPU (rang ≤ 8). shape/strides en ÉLÉMENTS.
            bool RunGather(uint64 in, uint64 out, uint32 rank, uint32 offset,
                           const uint32* shape, const uint32* strides, uint32 count);

            // im2col / col2im GPU. `p` = 12 uints {B,Cin,H,W,kH,kW,stride,pad,outH,outW,K,M}.
            // Kernel : buffers 0,1 (A,B) + UBO binding 2. `count` = nb de threads (éléments
            // de sortie). Utilisé par NkGpuIm2Col / NkGpuCol2Im.
            bool RunConvOp(const char* name, const NkString& nkslSrc,
                           uint64 in, uint64 out, const uint32* p12, uint32 count);

            // Générique 3 buffers (a,b,c) + UBO {12 uints} binding 3. Pour maxpool
            // (fwd : in/out/arg ; bwd : grad/arg/dx).
            bool RunOp3(const char* name, const NkString& nkslSrc,
                        uint64 a, uint64 b, uint64 c, const uint32* p12, uint32 count);

            // Pas d'Adam FUSÉ : un seul dispatch met à jour param/m/v EN PLACE.
            // Buffers 0,1,2,3 = param(rw), grad(r), m(rw), v(rw) + UBO binding 4.
            // `wd` = weight decay découplé (0 -> Adam ; >0 -> AdamW).
            bool RunAdam(uint64 param, uint64 grad, uint64 m, uint64 v, uint32 count,
                         float lr, float b1, float b2, float eps, float b1t, float b2t, float wd);

            void Shutdown();  // libère device + pipelines (appelé à l'arrêt)

        private:
            NkTensorGpu() = default;
            ~NkTensorGpu();
            NkTensorGpu(const NkTensorGpu&) = delete;
            NkTensorGpu& operator=(const NkTensorGpu&) = delete;

            struct Impl;
            Impl* mImpl = nullptr;
            bool  EnsureInit();
        };

        // Ops GPU (appelées par ops::Add / ops::Matmul quand un opérande est sur GPU).
        // Déplacent au besoin les opérandes sur GPU ; renvoient un tenseur device=GPU.
        // Renvoient un tenseur invalide si le GPU est indisponible.
        NkTensor NkGpuAdd   (const NkTensor& a, const NkTensor& b);   // élémentaire (mêmes formes)
        NkTensor NkGpuMatmul(const NkTensor& a, const NkTensor& b);   // a[M,K] · b[K,N] -> [M,N]
        // Matmul par LOTS : a[batch,M,K] · b[batch,K,N] -> [batch,M,N] (attention transformer).
        NkTensor NkGpuBatchedMatmul(const NkTensor& a, const NkTensor& b);
        // Broadcast d'un vecteur vec[C] sur le DERNIER axe de big[..,C] (biais Dense, affine
        // LayerNorm) — reste sur GPU. out[i] = big[i] (op) vec[i%C].
        NkTensor NkGpuAddBroadcastRow(const NkTensor& big, const NkTensor& vec);
        NkTensor NkGpuMulBroadcastRow(const NkTensor& big, const NkTensor& vec);
        NkTensor NkGpuMul    (const NkTensor& a, const NkTensor& b);  // élémentaire A ⊙ B
        NkTensor NkGpuSub    (const NkTensor& a, const NkTensor& b);  // élémentaire A − B
        NkTensor NkGpuRelu   (const NkTensor& a);                     // max(A,0)
        NkTensor NkGpuSigmoid(const NkTensor& a);                     // 1/(1+e^−A)
        NkTensor NkGpuTanh   (const NkTensor& a);                     // tanh(A)
        NkTensor NkGpuMulScalar(const NkTensor& a, double s);         // A · s
        NkTensor NkGpuAddScalar(const NkTensor& a, double s);         // A + s
        NkTensor NkGpuStep     (const NkTensor& a);                   // (A>0)?1:0  (masque ReLU')
        NkTensor NkGpuDiv      (const NkTensor& a, const NkTensor& b);// A / B (élémentaire)
        NkTensor NkGpuSqrt     (const NkTensor& a);                   // sqrt(A)  (Adam résident)
        NkTensor NkGpuGelu     (const NkTensor& a);                   // GELU(A) (tanh-approx)
        NkTensor NkGpuGeluBackward(const NkTensor& x, const NkTensor& grad);

        // Embedding : table[vocab,d], indices (ids en f32) -> lignes rassemblées ; backward =
        // scatter-add (gather par ligne de table, sans course).
        NkTensor NkGpuEmbedding(const NkTensor& table, const NkTensor& idx);
        NkTensor NkGpuEmbeddingBackward(const NkTensor& grad, const NkTensor& idx, int64 vocab, int64 d);

        // Pas d'Adam FUSÉ (1 seul dispatch) sur des tenseurs GPU : met à jour param, m, v
        // EN PLACE. Renvoie false si l'un n'est pas résident GPU (l'appelant reprend la
        // voie ops::). b1t = 1−β1ᵗ, b2t = 1−β2ᵗ (correction de biais, calculée côté CPU).
        bool NkGpuAdamStep(const NkTensor& param, const NkTensor& grad,
                           const NkTensor& m, const NkTensor& v,
                           float lr, float b1, float b2, float eps, float b1t, float b2t, float wd);

        // Réductions GPU (kind : 0=Sum, 1=Mean, 2=Max). Opèrent sur un tenseur GPU
        // contigu et renvoient un tenseur GPU. Argmax reste CPU (sortie i64).
        NkTensor NkGpuReduceAll (const NkTensor& a, int kind);              // -> {1}
        NkTensor NkGpuReduceAxis(const NkTensor& a, uint32 axis, int kind); // -> shape sans l'axe

        // Rend contigu un tenseur GPU strided (permute/transpose) sans repasser CPU.
        // Renvoie un tenseur GPU contigu de même forme. Rang > 8 -> repli CPU.
        NkTensor NkGpuContiguous(const NkTensor& t);

        // Convolution comme réarrangement mémoire, sur GPU (conv résidente).
        // NkGpuIm2Col : x GPU [B,Cin,H,W] -> col GPU [M=B·outH·outW, K=Cin·kH·kW].
        // NkGpuCol2Im : col GPU [M,K] -> dx GPU [B,Cin,H,W] (accumulation, formulation
        // gather sans atomics : un thread par élément de dx).
        NkTensor NkGpuIm2Col(const NkTensor& x, int64 kH, int64 kW, int64 stride, int64 pad,
                             int64 outH, int64 outW);
        NkTensor NkGpuCol2Im(const NkTensor& col, int64 B, int64 Cin, int64 H, int64 W,
                             int64 kH, int64 kW, int64 stride, int64 pad, int64 outH, int64 outW);

        NkTensor NkGpuExp(const NkTensor& a);                         // exp(A) élémentaire

        // Suréchantillonnage nearest ×2 : [B,C,H,W] -> [B,C,2H,2W] (fwd) ; backward = somme
        // des 4 sorties par entrée.
        NkTensor NkGpuUpsample2x(const NkTensor& x);
        NkTensor NkGpuUpsample2xBackward(const NkTensor& grad, int64 B, int64 C, int64 H, int64 W);

        // Convolution transposée 2D (upsampling appris). x[B,Cin,H,W], w[Cin,Cout,kH,kW]
        // -> y[B,Cout,outH,outW]. Formulations gather (sans course).
        NkTensor NkGpuConvTranspose2D(const NkTensor& x, const NkTensor& w, int64 stride, int64 pad);
        NkTensor NkGpuConvTranspose2DBackwardX(const NkTensor& grad, const NkTensor& w,
            int64 B, int64 Cin, int64 H, int64 W, int64 Cout, int64 kH, int64 kW,
            int64 stride, int64 pad, int64 outH, int64 outW);
        NkTensor NkGpuConvTranspose2DBackwardW(const NkTensor& x, const NkTensor& grad,
            int64 B, int64 Cin, int64 H, int64 W, int64 Cout, int64 kH, int64 kW,
            int64 stride, int64 pad, int64 outH, int64 outW);

        // Convolution 3D (voxels). x[B,Cin,D,H,W], w[Cout,Cin,kD,kH,kW]. Gather (sans course).
        NkTensor NkGpuConv3D(const NkTensor& x, const NkTensor& w, int64 stride, int64 pad);
        NkTensor NkGpuConv3DBackwardX(const NkTensor& grad, const NkTensor& w, const NkTensor& x, int64 stride, int64 pad);
        NkTensor NkGpuConv3DBackwardW(const NkTensor& grad, const NkTensor& x, const NkTensor& w, int64 stride, int64 pad);
        // Convolution transposée 3D (upsampling voxels). w[Cin,Cout,kD,kH,kW].
        NkTensor NkGpuConvTranspose3D(const NkTensor& x, const NkTensor& w, int64 stride, int64 pad);
        NkTensor NkGpuConvTranspose3DBackwardX(const NkTensor& grad, const NkTensor& w, const NkTensor& x, int64 stride, int64 pad);
        NkTensor NkGpuConvTranspose3DBackwardW(const NkTensor& grad, const NkTensor& x, const NkTensor& w, int64 stride, int64 pad);

        // LayerNorm sur le dernier axe (γ=1,β=0) : y=(x−μ)/√(var+ε). fwd + bwd (recalcul depuis x).
        NkTensor NkGpuLayerNormStd(const NkTensor& x);
        NkTensor NkGpuLayerNormStdBackward(const NkTensor& x, const NkTensor& grad);

        // Softmax sur le DERNIER axe (stable). + backward (dx=y⊙(dy−Σ dy⊙y)) + variante CAUSALE
        // (masque les positions futures : dernier axe [.., T, T], requête = row % T).
        NkTensor NkGpuSoftmaxRows(const NkTensor& x);
        NkTensor NkGpuSoftmaxBackward(const NkTensor& y, const NkTensor& grad);
        NkTensor NkGpuSoftmaxCausal(const NkTensor& x);
        // Max-pooling 2D GPU. Forward : renvoie la sortie [B,C,oH,oW] et remplit `argOut`
        // (indices HW du max, en f32). Backward : redistribue le grad vers les argmax.
        NkTensor NkGpuMaxPool2D(const NkTensor& x, int64 kernel, int64 stride, NkTensor& argOut);
        NkTensor NkGpuMaxPool2DBackward(const NkTensor& grad, const NkTensor& arg,
                                        int64 B, int64 C, int64 H, int64 W,
                                        int64 outH, int64 outW, int64 kernel, int64 stride);

    } // namespace ai
} // namespace nkentseu
