// =============================================================================
// NkTransformer.h — couches transformer (NKAI) : LayerNorm (affine), attention
// multi-têtes CAUSALE, bloc transformer, modèle GPT. Header-only (composent les
// primitives autograd : Matmul par lots, LayerNorm, SoftmaxCausal, GELU, Embedding,
// Permute, Reshape). Tout est GPU-résident si les paramètres résident sur GPU.
// =============================================================================
#pragma once

#include "NKNN/NkDense.h"
#include "NKNN/NkActivations.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKContainers/Sequential/NkVector.h"

#include <cmath>

namespace nkentseu {
    namespace ai {
        namespace nn {

            // ---- LayerNorm avec affine appris : y = LayerNorm(x)·γ + β (γ,β [1,d]) -----
            class NkLayerNorm {
            public:
                NkLayerNorm() = default;
                explicit NkLayerNorm(uint32 d)
                    : mGamma(NkVar::Leaf(NkTensor::Ones(NkShape{ (int64)1, (int64)d }), true)),
                      mBeta (NkVar::Leaf(NkTensor::Zeros(NkShape{ (int64)1, (int64)d }), true)) {}

                NkVar Forward(const NkVar& x) const {
                    const NkShape shp = x.Value().Shape();
                    const int64 d = shp[shp.Size() - 1];
                    const int64 rows = (d > 0) ? x.Value().Numel() / d : 0;
                    NkVar n = autograd::Reshape(autograd::LayerNorm(x), NkShape{ rows, d });
                    NkVar y = autograd::Add(autograd::Mul(n, mGamma), mBeta);   // broadcast [1,d]
                    return autograd::Reshape(y, shp);
                }
                void Parameters(NkVector<NkVar>& o) const { o.PushBack(mGamma); o.PushBack(mBeta); }
            private:
                NkVar mGamma, mBeta;
            };

            // ---- Attention multi-têtes CAUSALE (self-attention masquée) ----------------
            // x : [B, T, d] -> [B, T, d]. Projections Q,K,V,O (Dense d->d).
            class NkMultiHeadAttention {
            public:
                NkMultiHeadAttention() = default;
                NkMultiHeadAttention(uint32 dModel, uint32 nHeads, uint32 seed = 1u)
                    : mD(dModel), mH(nHeads),
                      mWq(dModel, dModel, seed + 1u), mWk(dModel, dModel, seed + 2u),
                      mWv(dModel, dModel, seed + 3u), mWo(dModel, dModel, seed + 4u) {}

                NkVar Forward(const NkVar& x) const {
                    const NkShape xs = x.Value().Shape();
                    const int64 B = xs[0], T = xs[1], d = xs[2];
                    const int64 h = mH, hd = d / h;
                    auto proj = [&](const NkDense& W) {
                        NkVar f = autograd::Reshape(x, NkShape{ B * T, d });
                        NkVar p = W.Forward(f);                              // [B*T, d]
                        p = autograd::Reshape(p, NkShape{ B, T, h, hd });
                        return autograd::Permute(p, NkShape{ 0, 2, 1, 3 });  // [B, h, T, hd]
                    };
                    NkVar Q = proj(mWq), K = proj(mWk), V = proj(mWv);
                    NkVar Kt = autograd::Permute(K, NkShape{ 0, 1, 3, 2 });  // [B, h, hd, T]
                    NkVar scores = autograd::Matmul(Q, Kt);                  // [B, h, T, T] (par lots)
                    scores = autograd::MulScalar(scores, 1.0 / std::sqrt((double)hd));
                    NkVar attn = autograd::SoftmaxCausal(scores);           // masque le futur
                    NkVar ctx = autograd::Matmul(attn, V);                  // [B, h, T, hd]
                    ctx = autograd::Permute(ctx, NkShape{ 0, 2, 1, 3 });    // [B, T, h, hd]
                    ctx = autograd::Reshape(ctx, NkShape{ B * T, d });
                    NkVar out = mWo.Forward(ctx);                           // [B*T, d]
                    return autograd::Reshape(out, NkShape{ B, T, d });
                }
                void Parameters(NkVector<NkVar>& o) const {
                    mWq.Parameters(o); mWk.Parameters(o); mWv.Parameters(o); mWo.Parameters(o);
                }
            private:
                uint32 mD = 0, mH = 0;
                NkDense mWq, mWk, mWv, mWo;
            };

            // Petit tenseur pseudo-aléatoire (LCG déterministe) pour initialiser les embeddings.
            static inline NkTensor RandnTensor(const NkShape& s, double scale, uint32 seed) {
                NkTensor t = NkTensor::Zeros(s);
                float* d = t.DataAs<float>(); const int64 n = t.Numel();
                uint32 st = seed * 2654435761u + 1u;
                for (int64 i = 0; i < n; ++i) {
                    st = st * 1664525u + 1013904223u;
                    double u = (double)((st >> 8) & 0xFFFFFFu) / 16777216.0;   // [0,1)
                    d[i] = (float)((u - 0.5) * 2.0 * scale);
                }
                return t;
            }

            // ---- Bloc transformer : pré-LN → attention → +résiduel → pré-LN → MLP → +résiduel
            class NkTransformerBlock {
            public:
                NkTransformerBlock() = default;
                NkTransformerBlock(uint32 d, uint32 h, uint32 seed = 1u)
                    : mLn1(d), mAttn(d, h, seed + 10u), mLn2(d),
                      mFc1(d, 4u * d, seed + 20u), mFc2(4u * d, d, seed + 30u), mD(d) {}

                NkVar Forward(const NkVar& x) const {
                    const NkShape xs = x.Value().Shape();
                    const int64 B = xs[0], T = xs[1], d = xs[2];
                    NkVar a  = mAttn.Forward(mLn1.Forward(x));       // attention (pré-LN)
                    NkVar x1 = autograd::Add(x, a);                 // résiduel
                    NkVar n  = mLn2.Forward(x1);
                    NkVar f  = autograd::Reshape(n, NkShape{ B * T, d });
                    NkVar hid = autograd::Gelu(mFc1.Forward(f));    // [B*T, 4d]
                    NkVar mo = autograd::Reshape(mFc2.Forward(hid), NkShape{ B, T, d });
                    return autograd::Add(x1, mo);                   // résiduel
                }
                void Parameters(NkVector<NkVar>& o) const {
                    mLn1.Parameters(o); mAttn.Parameters(o); mLn2.Parameters(o);
                    mFc1.Parameters(o); mFc2.Parameters(o);
                }
            private:
                NkLayerNorm mLn1, mLn2; NkMultiHeadAttention mAttn; NkDense mFc1, mFc2; uint32 mD = 0;
            };

            // ---- Modèle GPT (char-level) : embeddings + blocs + LN final + tête LM --------
            class NkGPT {
            public:
                NkGPT() = default;
                NkGPT(uint32 vocab, uint32 dModel, uint32 nHeads, uint32 nLayers, uint32 maxT, uint32 seed = 1u)
                    : mVocab(vocab), mD(dModel), mMaxT(maxT),
                      mTokEmb(NkVar::Leaf(RandnTensor(NkShape{ (int64)vocab, (int64)dModel }, 0.02, seed + 1u), true)),
                      mPosEmb(NkVar::Leaf(RandnTensor(NkShape{ (int64)maxT,  (int64)dModel }, 0.02, seed + 2u), true)),
                      mLnf(dModel), mHead(dModel, vocab, seed + 3u) {
                    for (uint32 l = 0; l < nLayers; ++l)
                        mBlocks.PushBack(NkTransformerBlock(dModel, nHeads, seed + 100u + l * 17u));
                }

                // tokens : [B, T] d'ids (f32). Renvoie les logits [B*T, vocab].
                NkVar Forward(const NkTensor& tokens) const {
                    const int64 B = tokens.Shape()[0], T = tokens.Shape()[1];
                    NkVar te = autograd::Embedding(mTokEmb, tokens);          // [B,T,d]
                    NkTensor posIdx = NkTensor::Zeros(NkShape{ B, T });
                    { float* p = posIdx.DataAs<float>(); for (int64 b=0;b<B;++b) for (int64 t=0;t<T;++t) p[b*T+t]=(float)t; }
                    NkVar pe = autograd::Embedding(mPosEmb, posIdx);          // [B,T,d]
                    NkVar x = autograd::Add(te, pe);
                    for (uint32 l = 0; l < mBlocks.Size(); ++l) x = mBlocks[l].Forward(x);
                    x = mLnf.Forward(x);
                    NkVar f = autograd::Reshape(x, NkShape{ B * T, (int64)mD });
                    return mHead.Forward(f);                                  // [B*T, vocab]
                }
                void Parameters(NkVector<NkVar>& o) const {
                    o.PushBack(mTokEmb); o.PushBack(mPosEmb);
                    for (uint32 l = 0; l < mBlocks.Size(); ++l) mBlocks[l].Parameters(o);
                    mLnf.Parameters(o); mHead.Parameters(o);
                }
                uint32 Vocab() const { return mVocab; }
                uint32 MaxT()  const { return mMaxT; }
            private:
                uint32 mVocab = 0, mD = 0, mMaxT = 0;
                NkVar mTokEmb, mPosEmb;
                NkVector<NkTransformerBlock> mBlocks;
                NkLayerNorm mLnf; NkDense mHead;
            };

        } // namespace nn
    } // namespace ai
} // namespace nkentseu
