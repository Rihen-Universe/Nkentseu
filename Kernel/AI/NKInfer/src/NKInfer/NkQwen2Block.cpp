// =============================================================================
// NkQwen2Block.cpp — voir NkQwen2Block.h pour la portée, l'architecture exacte
// et les sources citées.
// =============================================================================
#include "NKInfer/NkQwen2Block.h"
#include "NKTensor/NkTensorOps.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <initializer_list>

namespace nkentseu {
	namespace ai {
		namespace infer {

			namespace {

				// ---------------------------------------------------------------
				// Produit matriciel 2D CPU, écrit à la main : DÉLIBÉRÉMENT PAS
				// ops::Matmul, dont le dispatch bascule automatiquement vers le
				// GPU pour les grandes matrices (M·N·K >= 8e6 — cf NkTensorOps.cpp)
				// -- exactement la classe de tailles rencontrée ici (ex. projection
				// FFN 3584x18944). Ce jalon est CPU-only STRICT (cf NKInfer/ROADMAP.md :
				// contention Vulkan à éviter avec un entraînement GPU concurrent).
				// Même algorithme que ops::Matmul (ordre i-k-j, cache-friendly),
				// simplement sans AUCUNE branche de résidence GPU.
				// ---------------------------------------------------------------
				NkTensor CpuMatmul2D(const NkTensor &a, const NkTensor &b) {
					if (a.Rank() != 2 || b.Rank() != 2) {
						fprintf(stderr, "[NkQwen2Block] CpuMatmul2D : rang 2 requis (a=%u,b=%u)\n", a.Rank(), b.Rank());
						return NkTensor();
					}
					if (a.DType() != NkDType::NK_F32 || b.DType() != NkDType::NK_F32) {
						fprintf(stderr, "[NkQwen2Block] CpuMatmul2D : F32 requis\n");
						return NkTensor();
					}
					const int64 M = a.Shape()[0], K = a.Shape()[1];
					const int64 K2 = b.Shape()[0], N = b.Shape()[1];
					if (K != K2) {
						fprintf(stderr, "[NkQwen2Block] CpuMatmul2D : dimensions incompatibles (%lldx%lld * %lldx%lld)\n",
								(long long)M, (long long)K, (long long)K2, (long long)N);
						return NkTensor();
					}
					NkTensor ac = a.IsContiguous() ? a : a.Contiguous();
					NkTensor bc = b.IsContiguous() ? b : b.Contiguous();
					NkTensor out = NkTensor::Zeros(NkShape{M, N}, NkDType::NK_F32);
					const float *A = ac.DataAs<float>();
					const float *B = bc.DataAs<float>();
					float *C = out.DataAs<float>();
					for (int64 i = 0; i < M; ++i) {
						const float *Ai = A + i * K;
						float *Ci = C + i * N;
						for (int64 k = 0; k < K; ++k) {
							const float aik = Ai[k];
							if (aik == 0.0f)
								continue;
							const float *Bk = B + k * N;
							for (int64 j = 0; j < N; ++j)
								Ci[j] += aik * Bk[j];
						}
					}
					return out;
				}

				NkTensor Linear(const NkTensor &x, const NkTensor &w, const NkTensor &b) {
					NkTensor y = NkLinearNoBias(x, w);
					if (!y.IsValid())
						return y;
					if (b.IsValid())
						return ops::Add(y, b);
					return y;
				}

			} // namespace

			// x[T,K] · W[N,K]^T -> [T,N], SANS matérialiser W^T (cf NkQwen2Block.h) :
			// `x` accédé ligne par ligne, `W` aussi (produit scalaire direct par ligne
			// de sortie) -- les DEUX opérandes restent dans leur disposition mémoire
			// d'origine, cache-friendly, contrairement à CpuMatmul2D(x, w.Transpose(0,1))
			// qui forcerait une copie élément-par-élément de W entier via
			// NkTensor::Contiguous() générique (prohibitif pour lm_head : 152064 lignes).
			NkTensor NkLinearNoBias(const NkTensor &x, const NkTensor &w) {
				if (x.Rank() != 2 || w.Rank() != 2 || x.Shape()[1] != w.Shape()[1]) {
					fprintf(stderr, "[NkQwen2Block] NkLinearNoBias : formes incompatibles (x=[%lld,%lld], w=[%lld,%lld])\n",
							x.Rank() > 0 ? (long long)x.Shape()[0] : 0, x.Rank() > 1 ? (long long)x.Shape()[1] : 0,
							w.Rank() > 0 ? (long long)w.Shape()[0] : 0, w.Rank() > 1 ? (long long)w.Shape()[1] : 0);
					return NkTensor();
				}
				if (x.DType() != NkDType::NK_F32 || w.DType() != NkDType::NK_F32) {
					fprintf(stderr, "[NkQwen2Block] NkLinearNoBias : F32 requis\n");
					return NkTensor();
				}
				NkTensor xc = x.IsContiguous() ? x : x.Contiguous();
				NkTensor wc = w.IsContiguous() ? w : w.Contiguous();
				const int64 T = xc.Shape()[0], K = xc.Shape()[1], N = wc.Shape()[0];
				NkTensor out = NkTensor::Empty(NkShape{T, N}, NkDType::NK_F32);
				const float *X = xc.DataAs<float>();
				const float *W = wc.DataAs<float>();
				float *C = out.DataAs<float>();
				for (int64 t = 0; t < T; ++t) {
					const float *xt = X + t * K;
					float *ct = C + t * N;
					for (int64 n = 0; n < N; ++n) {
						const float *wn = W + n * K;
						float acc = 0.0f;
						for (int64 k = 0; k < K; ++k)
							acc += xt[k] * wn[k];
						ct[n] = acc;
					}
				}
				return out;
			}

			bool NkQwen2LayerWeights::IsValid(const NkQwen2Config &cfg) const {
				const int64 d = cfg.dModel, qd = (int64)cfg.nHeads * cfg.headDim, kvd = (int64)cfg.nKVHeads * cfg.headDim;
				auto shapeIs = [](const NkTensor &t, std::initializer_list<int64> dims) {
					if (!t.IsValid() || t.Rank() != (uint32)dims.size())
						return false;
					uint32 i = 0;
					for (int64 dim : dims) {
						if (t.Shape()[i] != dim)
							return false;
						++i;
					}
					return true;
				};
				return shapeIs(attnNorm, {d}) && shapeIs(wq, {qd, d}) && shapeIs(bq, {qd}) && shapeIs(wk, {kvd, d}) &&
					   shapeIs(bk, {kvd}) && shapeIs(wv, {kvd, d}) && shapeIs(bv, {kvd}) && shapeIs(wo, {d, qd}) &&
					   shapeIs(ffnNorm, {d}) && shapeIs(wGate, {(int64)cfg.ffnDim, d}) &&
					   shapeIs(wUp, {(int64)cfg.ffnDim, d}) && shapeIs(wDown, {d, (int64)cfg.ffnDim});
			}

			NkTensor NkRMSNorm(const NkTensor &x, const NkTensor &weight, float32 eps) {
				if (!x.IsValid() || !weight.IsValid() || x.DType() != NkDType::NK_F32 ||
					weight.DType() != NkDType::NK_F32) {
					fprintf(stderr, "[NkQwen2Block] NkRMSNorm : tenseurs invalides ou dtype != F32\n");
					return NkTensor();
				}
				const uint32 r = x.Rank();
				if (r != 1 && r != 2) {
					fprintf(stderr, "[NkQwen2Block] NkRMSNorm : rang 1 ou 2 attendu (reçu %u)\n", r);
					return NkTensor();
				}
				const int64 d = x.Shape()[r - 1];
				if (weight.Numel() != d) {
					fprintf(stderr, "[NkQwen2Block] NkRMSNorm : weight (%lld) != dernier axe de x (%lld)\n",
							(long long)weight.Numel(), (long long)d);
					return NkTensor();
				}
				const int64 T = (r == 2) ? x.Shape()[0] : 1;
				NkTensor xc = x.IsContiguous() ? x : x.Contiguous();
				NkTensor wc = weight.IsContiguous() ? weight : weight.Contiguous();
				NkTensor out = NkTensor::Empty(x.Shape(), NkDType::NK_F32);
				const float *xp = xc.DataAs<float>();
				const float *wp = wc.DataAs<float>();
				float *op = out.DataAs<float>();
				for (int64 t = 0; t < T; ++t) {
					const float *row = xp + t * d;
					double ss = 0.0;
					for (int64 i = 0; i < d; ++i)
						ss += (double)row[i] * (double)row[i];
					const double ms = ss / (double)d;
					const float inv = (float)(1.0 / std::sqrt(ms + (double)eps));
					float *orow = op + t * d;
					for (int64 i = 0; i < d; ++i)
						orow[i] = row[i] * inv * wp[i];
				}
				return out;
			}

			NkTensor NkSiLU(const NkTensor &x) {
				if (!x.IsValid() || x.DType() != NkDType::NK_F32) {
					fprintf(stderr, "[NkQwen2Block] NkSiLU : tenseur invalide ou dtype != F32\n");
					return NkTensor();
				}
				NkTensor xc = x.IsContiguous() ? x : x.Contiguous();
				NkTensor out = NkTensor::Empty(x.Shape(), NkDType::NK_F32);
				const int64 n = xc.Numel();
				const float *xp = xc.DataAs<float>();
				float *op = out.DataAs<float>();
				for (int64 i = 0; i < n; ++i) {
					const float v = xp[i];
					op[i] = (float)((double)v / (1.0 + std::exp(-(double)v)));
				}
				return out;
			}

			void NkApplyRoPE(NkTensor &x, int32 posOffset, float32 freqBase) {
				if (!x.IsValid() || x.DType() != NkDType::NK_F32 || x.Rank() != 3 || !x.IsContiguous()) {
					fprintf(stderr, "[NkQwen2Block] NkApplyRoPE : tenseur [H,T,headDim] f32 contigu requis\n");
					return;
				}
				const int64 H = x.Shape()[0], T = x.Shape()[1], Dh = x.Shape()[2];
				if ((Dh % 2) != 0)
					return;
				const int64 half = Dh / 2;
				float *p = x.DataAs<float>();
				for (int64 h = 0; h < H; ++h) {
					for (int64 t = 0; t < T; ++t) {
						const double pos = (double)(posOffset + t);
						float *row = p + (h * T + t) * Dh;
						for (int64 i = 0; i < half; ++i) {
							const double theta = pos * std::pow((double)freqBase, -2.0 * (double)i / (double)Dh);
							const double c = std::cos(theta), s = std::sin(theta);
							const float x1 = row[i], x2 = row[i + half];
							row[i] = (float)((double)x1 * c - (double)x2 * s);
							row[i + half] = (float)((double)x1 * s + (double)x2 * c);
						}
					}
				}
			}

			// Reshape [T, nH*headDim] -> [nH, T, headDim] (permute+contiguous).
			static NkTensor SplitHeadsAndPermute(const NkTensor &x, int64 T, int64 nH, int64 headDim) {
				NkTensor r = x.Reshape(NkShape{T, nH, headDim});
				if (!r.IsValid())
					return r;
				return r.Permute(NkShape{1, 0, 2}).Contiguous();
			}

			NkTensor NkQwen2LayerForward(const NkQwen2Config &cfg, const NkQwen2LayerWeights &w, const NkTensor &x,
										 NkKVCacheLayer &cache) {
				if (!cfg.IsValid()) {
					fprintf(stderr, "[NkQwen2Block] NkQwen2LayerForward : NkQwen2Config invalide\n");
					return NkTensor();
				}
				if (!w.IsValid(cfg)) {
					fprintf(stderr, "[NkQwen2Block] NkQwen2LayerForward : NkQwen2LayerWeights incohérents avec cfg\n");
					return NkTensor();
				}
				if (!x.IsValid() || x.DType() != NkDType::NK_F32 || x.Rank() != 2 || x.Shape()[1] != cfg.dModel) {
					fprintf(stderr, "[NkQwen2Block] NkQwen2LayerForward : x doit être [T,%d] f32\n", cfg.dModel);
					return NkTensor();
				}

				const int64 T = x.Shape()[0];
				const int64 d = cfg.dModel;
				const int64 headDim = cfg.headDim;
				const int64 nHeads = cfg.nHeads;
				const int64 nKVHeads = cfg.nKVHeads;
				const int64 groupSize = nHeads / nKVHeads;
				const int32 posOffset = cache.Length();

				// ---- Attention (pré-norme) ------------------------------------
				NkTensor xn = NkRMSNorm(x, w.attnNorm, cfg.rmsEps);
				if (!xn.IsValid())
					return NkTensor();

				NkTensor Q2 = Linear(xn, w.wq, w.bq); // [T, nHeads*headDim]
				NkTensor K2 = Linear(xn, w.wk, w.bk); // [T, nKVHeads*headDim]
				NkTensor V2 = Linear(xn, w.wv, w.bv); // [T, nKVHeads*headDim]
				if (!Q2.IsValid() || !K2.IsValid() || !V2.IsValid())
					return NkTensor();

				NkTensor Q = SplitHeadsAndPermute(Q2, T, nHeads, headDim);	  // [nHeads,T,headDim]
				NkTensor K = SplitHeadsAndPermute(K2, T, nKVHeads, headDim); // [nKVHeads,T,headDim]
				NkTensor V = SplitHeadsAndPermute(V2, T, nKVHeads, headDim); // [nKVHeads,T,headDim]
				if (!Q.IsValid() || !K.IsValid() || !V.IsValid())
					return NkTensor();

				NkApplyRoPE(Q, posOffset, cfg.ropeFreqBase);
				NkApplyRoPE(K, posOffset, cfg.ropeFreqBase); // V : PAS de RoPE (valeurs, pas positions)

				NkKVCacheAppend(cache, K, V);
				const int64 Ttot = cache.Length();
				const NkTensor &Kall = cache.k; // [nKVHeads,Ttot,headDim]
				const NkTensor &Vall = cache.v;

				NkTensor ctx = NkTensor::Empty(NkShape{nHeads, T, headDim}, NkDType::NK_F32);
				float *ctxP = ctx.DataAs<float>();
				const float invSqrtHd = (float)(1.0 / std::sqrt((double)headDim));

				for (int64 qh = 0; qh < nHeads; ++qh) {
					const int64 kvh = qh / groupSize;
					NkTensor Qh = Q.Slice(0, qh, qh + 1, 1).Reshape(NkShape{T, headDim});
					NkTensor Kh = Kall.Slice(0, kvh, kvh + 1, 1).Reshape(NkShape{Ttot, headDim});
					NkTensor Vh = Vall.Slice(0, kvh, kvh + 1, 1).Reshape(NkShape{Ttot, headDim});
					if (!Qh.IsValid() || !Kh.IsValid() || !Vh.IsValid())
						return NkTensor();

					NkTensor scores = CpuMatmul2D(Qh, Kh.Transpose(0, 1)); // [T,Ttot]
					if (!scores.IsValid())
						return NkTensor();
					float *sp = scores.DataAs<float>();

					// Masque causal + softmax par ligne (t local -> position absolue
					// posOffset+t ; les colonnes > posOffset+t sont interdites).
					for (int64 t = 0; t < T; ++t) {
						float *row = sp + t * Ttot;
						const int64 lastAllowed = posOffset + t; // colonne max autorisée (inclus)
						float mx = -INFINITY;
						for (int64 c = 0; c <= lastAllowed && c < Ttot; ++c) {
							row[c] *= invSqrtHd;
							if (row[c] > mx)
								mx = row[c];
						}
						double sum = 0.0;
						for (int64 c = 0; c <= lastAllowed && c < Ttot; ++c) {
							const double e = std::exp((double)(row[c] - mx));
							row[c] = (float)e;
							sum += e;
						}
						const double invSum = sum > 0.0 ? 1.0 / sum : 0.0;
						for (int64 c = 0; c <= lastAllowed && c < Ttot; ++c)
							row[c] = (float)(row[c] * invSum);
						for (int64 c = lastAllowed + 1; c < Ttot; ++c)
							row[c] = 0.0f; // futur : masqué (poids nul)
					}

					NkTensor ctxH = CpuMatmul2D(scores, Vh); // [T,headDim]
					if (!ctxH.IsValid())
						return NkTensor();
					const float *ctxHp = ctxH.DataAs<float>();
					std::memcpy(ctxP + qh * T * headDim, ctxHp, (usize)(T * headDim) * sizeof(float));
				}

				NkTensor ctxT = ctx.Permute(NkShape{1, 0, 2}).Contiguous().Reshape(NkShape{T, d}); // [T,d]
				if (!ctxT.IsValid())
					return NkTensor();
				NkTensor attnOut = NkLinearNoBias(ctxT, w.wo); // pas de biais (cf sources)
				if (!attnOut.IsValid())
					return NkTensor();
				NkTensor x1 = ops::Add(x, attnOut); // résiduel

				// ---- MLP SwiGLU (pré-norme) ------------------------------------
				NkTensor xn2 = NkRMSNorm(x1, w.ffnNorm, cfg.rmsEps);
				if (!xn2.IsValid())
					return NkTensor();
				NkTensor gate = NkSiLU(NkLinearNoBias(xn2, w.wGate)); // [T,ffnDim]
				NkTensor up = NkLinearNoBias(xn2, w.wUp);			 // [T,ffnDim]
				if (!gate.IsValid() || !up.IsValid())
					return NkTensor();
				NkTensor hidden = ops::Mul(gate, up);
				NkTensor down = NkLinearNoBias(hidden, w.wDown); // [T,d]
				if (!down.IsValid())
					return NkTensor();

				return ops::Add(x1, down); // résiduel
			}

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
