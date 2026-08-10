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
						: mGamma(NkVar::Leaf(NkTensor::Ones(NkShape{(int64)1, (int64)d}), true)),
						  mBeta(NkVar::Leaf(NkTensor::Zeros(NkShape{(int64)1, (int64)d}), true)) {
					}

					NkVar Forward(const NkVar &x) const {
						const NkShape shp = x.Value().Shape();
						const int64 d = shp[shp.Size() - 1];
						const int64 rows = (d > 0) ? x.Value().Numel() / d : 0;
						NkVar n = autograd::Reshape(autograd::LayerNorm(x), NkShape{rows, d});
						NkVar y = autograd::Add(autograd::Mul(n, mGamma), mBeta); // broadcast [1,d]
						return autograd::Reshape(y, shp);
					}

					void Parameters(NkVector<NkVar> &o) const {
						o.PushBack(mGamma);
						o.PushBack(mBeta);
					}

				private:
					NkVar mGamma, mBeta;
			};

			// ---- Attention multi-têtes CAUSALE (self-attention masquée) ----------------
			// x : [B, T, d] -> [B, T, d]. Projections Q,K,V,O (Dense d->d).
			class NkMultiHeadAttention {
				public:
					NkMultiHeadAttention() = default;

					NkMultiHeadAttention(uint32 dModel, uint32 nHeads, uint32 seed = 1u)
						: mD(dModel), mH(nHeads), mWq(dModel, dModel, seed + 1u), mWk(dModel, dModel, seed + 2u),
						  mWv(dModel, dModel, seed + 3u), mWo(dModel, dModel, seed + 4u) {
					}

					NkVar Forward(const NkVar &x) const {
						const NkShape xs = x.Value().Shape();
						const int64 B = xs[0], T = xs[1], d = xs[2];
						const int64 h = mH, hd = d / h;
						auto proj = [&](const NkDense &W) {
							NkVar f = autograd::Reshape(x, NkShape{B * T, d});
							NkVar p = W.Forward(f); // [B*T, d]
							p = autograd::Reshape(p, NkShape{B, T, h, hd});
							return autograd::Permute(p, NkShape{0, 2, 1, 3}); // [B, h, T, hd]
						};
						NkVar Q = proj(mWq), K = proj(mWk), V = proj(mWv);
						NkVar Kt = autograd::Permute(K, NkShape{0, 1, 3, 2}); // [B, h, hd, T]
						NkVar scores = autograd::Matmul(Q, Kt);				  // [B, h, T, T] (par lots)
						scores = autograd::MulScalar(scores, 1.0 / std::sqrt((double)hd));
						NkVar attn = autograd::SoftmaxCausal(scores);	   // masque le futur
						NkVar ctx = autograd::Matmul(attn, V);			   // [B, h, T, hd]
						ctx = autograd::Permute(ctx, NkShape{0, 2, 1, 3}); // [B, T, h, hd]
						ctx = autograd::Reshape(ctx, NkShape{B * T, d});
						NkVar out = mWo.Forward(ctx); // [B*T, d]
						return autograd::Reshape(out, NkShape{B, T, d});
					}

					// Un pas de décodage INCRÉMENTAL avec KV-cache. `x` = [B,1,d] (le token
				// courant seul). `kC`/`vC` = caches [B,h,Tpast,hd] de CETTE couche, étendus
				// en place. La requête voit toutes les clés passées (softmax simple).
				NkVar ForwardStep(const NkVar &x, NkTensor &kC, NkTensor &vC) const {
					const NkShape xs = x.Value().Shape();
					const int64 B = xs[0], d = xs[2];
					const int64 h = mH, hd = d / h;
					auto proj = [&](const NkDense &W) {
						NkVar f = autograd::Reshape(x, NkShape{B * 1, d});
						NkVar p = W.Forward(f);
						p = autograd::Reshape(p, NkShape{B, (int64)1, h, hd});
						return autograd::Permute(p, NkShape{0, 2, 1, 3}); // [B, h, 1, hd]
					};
					NkVar Q = proj(mWq), K = proj(mWk), V = proj(mWv);
					kC = kC.IsValid() ? CatTimeAxis(kC, K.Value()) : K.Value().Contiguous();
					vC = vC.IsValid() ? CatTimeAxis(vC, V.Value()) : V.Value().Contiguous();
					NkVar Kc = NkVar::Leaf(kC, false), Vc = NkVar::Leaf(vC, false);
					NkVar Kt = autograd::Permute(Kc, NkShape{0, 1, 3, 2}); // [B,h,hd,Ttot]
					NkVar scores = autograd::Matmul(Q, Kt);				  // [B,h,1,Ttot]
					scores = autograd::MulScalar(scores, 1.0 / std::sqrt((double)hd));
					NkVar attn = autograd::Softmax(scores);
					NkVar ctx = autograd::Matmul(attn, Vc); // [B,h,1,hd]
					ctx = autograd::Permute(ctx, NkShape{0, 2, 1, 3});
					ctx = autograd::Reshape(ctx, NkShape{B * 1, d});
					NkVar out = mWo.Forward(ctx);
					return autograd::Reshape(out, NkShape{B, (int64)1, d});
				}

				void Parameters(NkVector<NkVar> &o) const {
						mWq.Parameters(o);
						mWk.Parameters(o);
						mWv.Parameters(o);
						mWo.Parameters(o);
					}

				private:
					// Concatène [B,h,Ta,hd] ⊕ [B,h,Tb,hd] sur l'axe TEMPS (2).
					static NkTensor CatTimeAxis(const NkTensor &a, const NkTensor &b) {
						NkTensor ca = a.Contiguous(), cb = b.Contiguous();
						const NkShape &sa = ca.Shape();
						const NkShape &sb = cb.Shape();
						const int64 B = sa[0], H = sa[1], Ta = sa[2], hd = sa[3], Tb = sb[2];
						NkTensor out = NkTensor::Zeros(NkShape{B, H, Ta + Tb, hd});
						const float *pa = ca.DataAs<float>();
						const float *pb = cb.DataAs<float>();
						float *po = out.DataAs<float>();
						for (int64 bb = 0; bb < B; ++bb)
							for (int64 hh = 0; hh < H; ++hh) {
								const int64 obase = ((bb * H + hh) * (Ta + Tb)) * hd;
								const int64 abase = ((bb * H + hh) * Ta) * hd;
								const int64 bbase = ((bb * H + hh) * Tb) * hd;
								for (int64 i = 0; i < Ta * hd; ++i)
									po[obase + i] = pa[abase + i];
								for (int64 i = 0; i < Tb * hd; ++i)
									po[obase + Ta * hd + i] = pb[bbase + i];
							}
						return out;
					}

					uint32 mD = 0, mH = 0;
					NkDense mWq, mWk, mWv, mWo;
			};

			// Petit tenseur pseudo-aléatoire (LCG déterministe) pour initialiser les embeddings.
			static inline NkTensor RandnTensor(const NkShape &s, double scale, uint32 seed) {
				NkTensor t = NkTensor::Zeros(s);
				float *d = t.DataAs<float>();
				const int64 n = t.Numel();
				uint32 st = seed * 2654435761u + 1u;
				for (int64 i = 0; i < n; ++i) {
					st = st * 1664525u + 1013904223u;
					double u = (double)((st >> 8) & 0xFFFFFFu) / 16777216.0; // [0,1)
					d[i] = (float)((u - 0.5) * 2.0 * scale);
				}
				return t;
			}

			// ---- Bloc transformer : pré-LN → attention → +résiduel → pré-LN → MLP → +résiduel
			class NkTransformerBlock {
				public:
					NkTransformerBlock() = default;

					NkTransformerBlock(uint32 d, uint32 h, uint32 seed = 1u)
						: mLn1(d), mAttn(d, h, seed + 10u), mLn2(d), mFc1(d, 4u * d, seed + 20u),
						  mFc2(4u * d, d, seed + 30u), mD(d) {
					}

					NkVar Forward(const NkVar &x) const {
						const NkShape xs = x.Value().Shape();
						const int64 B = xs[0], T = xs[1], d = xs[2];
						NkVar a = mAttn.Forward(mLn1.Forward(x)); // attention (pré-LN)
						NkVar x1 = autograd::Add(x, a);			  // résiduel
						NkVar n = mLn2.Forward(x1);
						NkVar f = autograd::Reshape(n, NkShape{B * T, d});
						NkVar hid = autograd::Gelu(mFc1.Forward(f)); // [B*T, 4d]
						NkVar mo = autograd::Reshape(mFc2.Forward(hid), NkShape{B, T, d});
						return autograd::Add(x1, mo); // résiduel
					}

					// Identique à Forward, mais rend AUSSI les activations cachées du MLP
					// ([B·T, 4d], après GELU). Additif : Forward n'est pas touché.
					// Motif : comparer deux réseaux par ce que leurs unités FONT, et non
					// par ce à quoi leurs poids ressemblent, demande de pouvoir observer
					// ces activations. Recopier le calcul dans l'application le ferait
					// diverger du modèle au premier changement ; on l'expose donc ici.
					NkVar ForwardCapture(const NkVar &x, NkTensor *mlpCache) const {
						const NkShape xs = x.Value().Shape();
						const int64 B = xs[0], T = xs[1], d = xs[2];
						NkVar a = mAttn.Forward(mLn1.Forward(x));
						NkVar x1 = autograd::Add(x, a);
						NkVar n = mLn2.Forward(x1);
						NkVar f = autograd::Reshape(n, NkShape{B * T, d});
						NkVar hid = autograd::Gelu(mFc1.Forward(f)); // [B*T, 4d]
						if (mlpCache)
							*mlpCache = hid.Value().ToCPU().Contiguous().Clone();
						NkVar mo = autograd::Reshape(mFc2.Forward(hid), NkShape{B, T, d});
						return autograd::Add(x1, mo);
					}

					// Un pas incrémental (KV-cache) : x = [B,1,d], caches K/V de cette couche.
					NkVar ForwardStep(const NkVar &x, NkTensor &kC, NkTensor &vC) const {
						const NkShape xs = x.Value().Shape();
						const int64 B = xs[0], d = xs[2];
						NkVar a = mAttn.ForwardStep(mLn1.Forward(x), kC, vC);
						NkVar x1 = autograd::Add(x, a);
						NkVar n = mLn2.Forward(x1);
						NkVar f = autograd::Reshape(n, NkShape{B * 1, d});
						NkVar hid = autograd::Gelu(mFc1.Forward(f));
						NkVar mo = autograd::Reshape(mFc2.Forward(hid), NkShape{B, (int64)1, d});
						return autograd::Add(x1, mo);
					}

					void Parameters(NkVector<NkVar> &o) const {
						mLn1.Parameters(o);
						mAttn.Parameters(o);
						mLn2.Parameters(o);
						mFc1.Parameters(o);
						mFc2.Parameters(o);
					}

				private:
					NkLayerNorm mLn1, mLn2;
					NkMultiHeadAttention mAttn;
					NkDense mFc1, mFc2;
					uint32 mD = 0;
			};

				// Cache clé/valeur pour le décodage INCRÉMENTAL : K et V par couche
				// ([B,h,Tpast,hd]). `len` = nombre de tokens déjà traités (= position absolue
				// du prochain token pour l'embedding positionnel).
				struct NkKVCache {
						NkVector<NkTensor> k, v;
						int64 len = 0;

						void Reset(uint32 nLayers) {
							k.Clear();
							v.Clear();
							k.Resize((nk_size)nLayers);
							v.Resize((nk_size)nLayers);
							len = 0;
						}
				};

			// ---- Modèle GPT (char-level) : embeddings + blocs + LN final + tête LM --------
			class NkGPT {
				public:
					NkGPT() = default;

					NkGPT(uint32 vocab, uint32 dModel, uint32 nHeads, uint32 nLayers, uint32 maxT, uint32 seed = 1u)
						: mVocab(vocab), mD(dModel), mMaxT(maxT),
						  mTokEmb(
							  NkVar::Leaf(RandnTensor(NkShape{(int64)vocab, (int64)dModel}, 0.02, seed + 1u), true)),
						  mPosEmb(NkVar::Leaf(RandnTensor(NkShape{(int64)maxT, (int64)dModel}, 0.02, seed + 2u), true)),
						  mLnf(dModel), mHead(dModel, vocab, seed + 3u) {
						for (uint32 l = 0; l < nLayers; ++l)
							mBlocks.PushBack(NkTransformerBlock(dModel, nHeads, seed + 100u + l * 17u));
					}

					// tokens : [B, T] d'ids (f32). Renvoie les logits [B*T, vocab].
					NkVar Forward(const NkTensor &tokens) const {
						const int64 B = tokens.Shape()[0], T = tokens.Shape()[1];
						NkVar te = autograd::Embedding(mTokEmb, tokens); // [B,T,d]
						NkTensor posIdx = NkTensor::Zeros(NkShape{B, T});
						{
							float *p = posIdx.DataAs<float>();
							for (int64 b = 0; b < B; ++b)
								for (int64 t = 0; t < T; ++t)
									p[b * T + t] = (float)t;
						}
						NkVar pe = autograd::Embedding(mPosEmb, posIdx); // [B,T,d]
						NkVar x = autograd::Add(te, pe);
						for (uint32 l = 0; l < mBlocks.Size(); ++l)
							x = mBlocks[l].Forward(x);
						x = mLnf.Forward(x);
						NkVar f = autograd::Reshape(x, NkShape{B * T, (int64)mD});
						return mHead.Forward(f); // [B*T, vocab]
					}

					// Identique à Forward, mais collecte en plus les ACTIVATIONS :
					//   `residus`  : le flux résiduel [B·T, d] à l'entrée du 1er bloc puis
					//                à la sortie de chacun (donc nLayers+1 relevés) ;
					//   `mlpCache` : les activations cachées du MLP [B·T, 4d] par bloc.
					// Additif : `Forward` est inchangé, et rien de ceci n'entre dans le
					// graphe de gradient — ce sont des copies CPU, pour observation.
					NkVar ForwardCapture(const NkTensor &tokens, NkVector<NkTensor> *residus,
										 NkVector<NkTensor> *mlpCache) const {
						const int64 B = tokens.Shape()[0], T = tokens.Shape()[1];
						NkVar te = autograd::Embedding(mTokEmb, tokens);
						NkTensor posIdx = NkTensor::Zeros(NkShape{B, T});
						{
							float *p = posIdx.DataAs<float>();
							for (int64 b = 0; b < B; ++b)
								for (int64 t = 0; t < T; ++t)
									p[b * T + t] = (float)t;
						}
						NkVar pe = autograd::Embedding(mPosEmb, posIdx);
						NkVar x = autograd::Add(te, pe);
						auto releve = [&](const NkVar &v) {
							if (residus)
								residus->PushBack(v.Value().ToCPU().Contiguous().Clone());
						};
						releve(x);
						for (uint32 l = 0; l < mBlocks.Size(); ++l) {
							NkTensor h;
							x = mBlocks[l].ForwardCapture(x, mlpCache ? &h : nullptr);
							if (mlpCache)
								mlpCache->PushBack(h);
							releve(x);
						}
						x = mLnf.Forward(x);
						NkVar f = autograd::Reshape(x, NkShape{B * T, (int64)mD});
						return mHead.Forward(f);
					}

					// Décodage INCRÉMENTAL avec KV-cache : traite UN token (id) à la position
					// absolue `cache.len`, met à jour les caches, renvoie les logits [B, vocab]
					// de ce token. Mathématiquement identique à Forward sur le préfixe complet,
					// mais en O(T) par token au lieu de O(T²). `tokenId` = [B,1].
					NkVar ForwardStep(const NkTensor &tokenId, NkKVCache &cache) const {
						if (cache.k.Size() != mBlocks.Size())
							cache.Reset((uint32)mBlocks.Size());
						const int64 B = tokenId.Shape()[0];
						NkVar te = autograd::Embedding(mTokEmb, tokenId); // [B,1,d]
						NkTensor posIdx = NkTensor::Full(NkShape{B, (int64)1}, (double)cache.len);
						NkVar pe = autograd::Embedding(mPosEmb, posIdx); // [B,1,d]
						NkVar x = autograd::Add(te, pe);
						for (uint32 l = 0; l < mBlocks.Size(); ++l)
							x = mBlocks[l].ForwardStep(x, cache.k[(nk_size)l], cache.v[(nk_size)l]);
						x = mLnf.Forward(x);
						NkVar f = autograd::Reshape(x, NkShape{B * 1, (int64)mD});
						cache.len += 1;
						return mHead.Forward(f); // [B, vocab]
					}

					void Parameters(NkVector<NkVar> &o) const {
						o.PushBack(mTokEmb);
						o.PushBack(mPosEmb);
						for (uint32 l = 0; l < mBlocks.Size(); ++l)
							mBlocks[l].Parameters(o);
						mLnf.Parameters(o);
						mHead.Parameters(o);
					}

					uint32 Vocab() const {
						return mVocab;
					}

					uint32 MaxT() const {
						return mMaxT;
					}

				private:
					uint32 mVocab = 0, mD = 0, mMaxT = 0;
					NkVar mTokEmb, mPosEmb;
					NkVector<NkTransformerBlock> mBlocks;
					NkLayerNorm mLnf;
					NkDense mHead;
			};

		} // namespace nn
	} // namespace ai
} // namespace nkentseu
