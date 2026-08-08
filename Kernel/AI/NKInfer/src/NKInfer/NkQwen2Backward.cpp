// =============================================================================
// NkQwen2Backward.cpp — voir NkQwen2Backward.h pour le contrat, les dérivées
// et la justification du backward manuel hors-graphe.
// =============================================================================
#include "NKInfer/NkQwen2Backward.h"
#include "NKTensor/NkTensorOps.h"

#include <cstdio>
#include <cstring>
#include <cmath>

namespace nkentseu {
	namespace ai {
		namespace infer {

			namespace {

				// ---------------------------------------------------------------
				// Produit matriciel 2D CPU — COPIE À L'IDENTIQUE de CpuMatmul2D de
				// NkQwen2Block.cpp (fonction interne là-bas, non exportée ; on ne
				// modifie PAS le module d'inférence validé — méthode additive,
				// piège n°7 des notes QLoRA). L'identité du code garantit que le
				// forward d'entraînement produit les MÊMES flottants que le
				// forward d'inférence (même ordre de somme, même saut des zéros).
				// ---------------------------------------------------------------
				NkTensor CpuMatmul2D(const NkTensor &a, const NkTensor &b) {
					if (a.Rank() != 2 || b.Rank() != 2) {
						fprintf(stderr, "[NkQwen2Backward] CpuMatmul2D : rang 2 requis (a=%u,b=%u)\n", a.Rank(),
								b.Rank());
						return NkTensor();
					}
					if (a.DType() != NkDType::NK_F32 || b.DType() != NkDType::NK_F32) {
						fprintf(stderr, "[NkQwen2Backward] CpuMatmul2D : F32 requis\n");
						return NkTensor();
					}
					const int64 M = a.Shape()[0], K = a.Shape()[1];
					const int64 K2 = b.Shape()[0], N = b.Shape()[1];
					if (K != K2) {
						fprintf(stderr, "[NkQwen2Backward] CpuMatmul2D : dimensions incompatibles\n");
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

				// Projection avec biais optionnel — même composition que dans
				// NkQwen2Block.cpp (NkLinearNoBias + ops::Add broadcast).
				NkTensor Linear(const NkTensor &x, const NkTensor &w, const NkTensor &b) {
					NkTensor y = NkLinearNoBias(x, w);
					if (!y.IsValid())
						return y;
					if (b.IsValid())
						return ops::Add(y, b);
					return y;
				}

				// Reshape [T, nH*headDim] -> [nH, T, headDim] — copie du helper
				// interne de NkQwen2Block.cpp (mêmes opérations -> mêmes octets).
				NkTensor SplitHeadsAndPermute(const NkTensor &x, int64 T, int64 nH, int64 headDim) {
					NkTensor r = x.Reshape(NkShape{T, nH, headDim});
					if (!r.IsValid())
						return r;
					return r.Permute(NkShape{1, 0, 2}).Contiguous();
				}

				// dst += src (mêmes nombres d'éléments, f32 contigus).
				bool AccumInPlace(NkTensor &dst, const NkTensor &src) {
					if (!dst.IsValid() || !src.IsValid() || dst.Numel() != src.Numel()) {
						fprintf(stderr, "[NkQwen2Backward] AccumInPlace : tenseurs incompatibles\n");
						return false;
					}
					NkTensor sc = src.IsContiguous() ? src : src.Contiguous();
					float *dp = dst.DataAs<float>();
					const float *sp = sc.DataAs<float>();
					const int64 n = dst.Numel();
					for (int64 i = 0; i < n; ++i)
						dp[i] += sp[i];
					return true;
				}

				// [nH,T,hd] -> [T,nH*hd] : inverse du découpage en têtes appliqué
				// aux GRADIENTS (dQ/dK/dV vers l'espace plat des projections).
				NkTensor MergeHeadsToFlat(const NkTensor &x) {
					const int64 H = x.Shape()[0], T = x.Shape()[1], hd = x.Shape()[2];
					NkTensor out = NkTensor::Empty(NkShape{T, H * hd}, NkDType::NK_F32);
					const float *xp = x.DataAs<float>();
					float *op = out.DataAs<float>();
					for (int64 h = 0; h < H; ++h)
						for (int64 t = 0; t < T; ++t)
							std::memcpy(op + t * H * hd + h * hd, xp + (h * T + t) * hd, (usize)hd * sizeof(float));
					return out;
				}

				// [T,nH*hd] -> [nH,T,hd] : redécoupe le gradient de ctxT en têtes.
				NkTensor SplitFlatToHeads(const NkTensor &x, int64 nH, int64 hd) {
					const int64 T = x.Shape()[0];
					NkTensor out = NkTensor::Empty(NkShape{nH, T, hd}, NkDType::NK_F32);
					const float *xp = x.DataAs<float>();
					float *op = out.DataAs<float>();
					for (int64 h = 0; h < nH; ++h)
						for (int64 t = 0; t < T; ++t)
							std::memcpy(op + (h * T + t) * hd, xp + t * nH * hd + h * hd, (usize)hd * sizeof(float));
					return out;
				}

			} // namespace

			// ---- Briques isolées --------------------------------------------------

			NkTensor NkRMSNormBackward(const NkTensor &x, const NkTensor &weight, float32 eps, const NkTensor &dY) {
				if (!x.IsValid() || !weight.IsValid() || !dY.IsValid() || x.DType() != NkDType::NK_F32 ||
					x.Numel() != dY.Numel()) {
					fprintf(stderr, "[NkQwen2Backward] NkRMSNormBackward : tenseurs invalides\n");
					return NkTensor();
				}
				const uint32 r = x.Rank();
				if (r != 1 && r != 2) {
					fprintf(stderr, "[NkQwen2Backward] NkRMSNormBackward : rang 1 ou 2 attendu\n");
					return NkTensor();
				}
				const int64 d = x.Shape()[r - 1];
				if (weight.Numel() != d) {
					fprintf(stderr, "[NkQwen2Backward] NkRMSNormBackward : weight incompatible\n");
					return NkTensor();
				}
				const int64 T = (r == 2) ? x.Shape()[0] : 1;
				NkTensor xc = x.IsContiguous() ? x : x.Contiguous();
				NkTensor wc = weight.IsContiguous() ? weight : weight.Contiguous();
				NkTensor dc = dY.IsContiguous() ? dY : dY.Contiguous();
				NkTensor out = NkTensor::Empty(x.Shape(), NkDType::NK_F32);
				const float *xp = xc.DataAs<float>();
				const float *wp = wc.DataAs<float>();
				const float *gp = dc.DataAs<float>();
				float *op = out.DataAs<float>();
				for (int64 t = 0; t < T; ++t) {
					const float *row = xp + t * d;
					const float *grow = gp + t * d;
					// Mêmes accumulations en double que le forward (NkRMSNorm) :
					// ss = Σx², dot = Σ w_i·x_i·dy_i.
					double ss = 0.0, dot = 0.0;
					for (int64 i = 0; i < d; ++i) {
						ss += (double)row[i] * (double)row[i];
						dot += (double)wp[i] * (double)row[i] * (double)grow[i];
					}
					const double ms = ss / (double)d;
					const double inv = 1.0 / std::sqrt(ms + (double)eps);
					const double inv3OverD = inv * inv * inv / (double)d;
					float *orow = op + t * d;
					// dx_j = inv·w_j·dy_j − (inv³/d)·x_j·dot (dérivée fermée, cf .h).
					for (int64 j = 0; j < d; ++j)
						orow[j] = (float)(inv * (double)wp[j] * (double)grow[j] - inv3OverD * (double)row[j] * dot);
				}
				return out;
			}

			void NkApplyRoPEBackward(NkTensor &dx, int32 posOffset, float32 freqBase) {
				if (!dx.IsValid() || dx.DType() != NkDType::NK_F32 || dx.Rank() != 3 || !dx.IsContiguous()) {
					fprintf(stderr, "[NkQwen2Backward] NkApplyRoPEBackward : [H,T,headDim] f32 contigu requis\n");
					return;
				}
				const int64 H = dx.Shape()[0], T = dx.Shape()[1], Dh = dx.Shape()[2];
				if ((Dh % 2) != 0)
					return;
				const int64 half = Dh / 2;
				float *p = dx.DataAs<float>();
				for (int64 h = 0; h < H; ++h) {
					for (int64 t = 0; t < T; ++t) {
						const double pos = (double)(posOffset + t);
						float *row = p + (h * T + t) * Dh;
						for (int64 i = 0; i < half; ++i) {
							// Rotation orthogonale : le backward est Rᵀ = R(−θ),
							// soit le forward avec sin(θ) changé de signe.
							const double theta = pos * std::pow((double)freqBase, -2.0 * (double)i / (double)Dh);
							const double c = std::cos(theta), s = std::sin(theta);
							const float d1 = row[i], d2 = row[i + half];
							row[i] = (float)((double)d1 * c + (double)d2 * s);
							row[i + half] = (float)(-(double)d1 * s + (double)d2 * c);
						}
					}
				}
			}

			bool NkSwiGLUBackward(const NkTensor &g, const NkTensor &u, const NkTensor &dH, NkTensor &dG, NkTensor &dU) {
				if (!g.IsValid() || !u.IsValid() || !dH.IsValid() || g.Numel() != u.Numel() ||
					g.Numel() != dH.Numel() || g.DType() != NkDType::NK_F32) {
					fprintf(stderr, "[NkQwen2Backward] NkSwiGLUBackward : formes incompatibles\n");
					return false;
				}
				NkTensor gc = g.IsContiguous() ? g : g.Contiguous();
				NkTensor uc = u.IsContiguous() ? u : u.Contiguous();
				NkTensor hc = dH.IsContiguous() ? dH : dH.Contiguous();
				dG = NkTensor::Empty(g.Shape(), NkDType::NK_F32);
				dU = NkTensor::Empty(g.Shape(), NkDType::NK_F32);
				const float *gp = gc.DataAs<float>();
				const float *up = uc.DataAs<float>();
				const float *hp = hc.DataAs<float>();
				float *dgp = dG.DataAs<float>();
				float *dup = dU.DataAs<float>();
				const int64 n = gc.Numel();
				for (int64 i = 0; i < n; ++i) {
					// σ et silu en double, comme NkSiLU (mêmes conventions numériques).
					const double gv = (double)gp[i];
					const double sig = 1.0 / (1.0 + std::exp(-gv));
					const double silu = gv * sig;
					const double dsilu = sig * (1.0 + gv * (1.0 - sig)); // silu'(g)
					dgp[i] = (float)((double)hp[i] * (double)up[i] * dsilu);
					dup[i] = (float)((double)hp[i] * silu);
				}
				return true;
			}

			NkTensor NkGQAAttentionForward(const NkTensor &Q, const NkTensor &K, const NkTensor &V, NkTensor *outProbs) {
				if (!Q.IsValid() || !K.IsValid() || !V.IsValid() || Q.Rank() != 3 || K.Rank() != 3 || V.Rank() != 3 ||
					Q.Shape()[1] != K.Shape()[1] || Q.Shape()[2] != K.Shape()[2] || K.Shape()[0] != V.Shape()[0] ||
					(Q.Shape()[0] % K.Shape()[0]) != 0) {
					fprintf(stderr, "[NkQwen2Backward] NkGQAAttentionForward : formes incompatibles\n");
					return NkTensor();
				}
				const int64 nH = Q.Shape()[0], T = Q.Shape()[1], hd = Q.Shape()[2];
				const int64 nKV = K.Shape()[0];
				const int64 groupSize = nH / nKV;
				const float invSqrtHd = (float)(1.0 / std::sqrt((double)hd));

				NkTensor ctx = NkTensor::Empty(NkShape{nH, T, hd}, NkDType::NK_F32);
				float *ctxP = ctx.DataAs<float>();
				if (outProbs)
					*outProbs = NkTensor::Empty(NkShape{nH, T, T}, NkDType::NK_F32);

				for (int64 qh = 0; qh < nH; ++qh) {
					const int64 kvh = qh / groupSize;
					NkTensor Qh = Q.Slice(0, qh, qh + 1, 1).Reshape(NkShape{T, hd});
					NkTensor Kh = K.Slice(0, kvh, kvh + 1, 1).Reshape(NkShape{T, hd});
					NkTensor Vh = V.Slice(0, kvh, kvh + 1, 1).Reshape(NkShape{T, hd});
					if (!Qh.IsValid() || !Kh.IsValid() || !Vh.IsValid())
						return NkTensor();

					// Mêmes opérations, dans le même ordre, que la boucle
					// d'attention de NkQwen2LayerForward (posOffset=0, Ttot=T).
					NkTensor scores = CpuMatmul2D(Qh, Kh.Transpose(0, 1)); // [T,T]
					if (!scores.IsValid())
						return NkTensor();
					float *sp = scores.DataAs<float>();
					for (int64 t = 0; t < T; ++t) {
						float *row = sp + t * T;
						const int64 lastAllowed = t; // position 0 : la requête t voit les clés <= t
						float mx = -INFINITY;
						for (int64 c = 0; c <= lastAllowed; ++c) {
							row[c] *= invSqrtHd;
							if (row[c] > mx)
								mx = row[c];
						}
						double sum = 0.0;
						for (int64 c = 0; c <= lastAllowed; ++c) {
							const double e = std::exp((double)(row[c] - mx));
							row[c] = (float)e;
							sum += e;
						}
						const double invSum = sum > 0.0 ? 1.0 / sum : 0.0;
						for (int64 c = 0; c <= lastAllowed; ++c)
							row[c] = (float)(row[c] * invSum);
						for (int64 c = lastAllowed + 1; c < T; ++c)
							row[c] = 0.0f; // futur masqué
					}
					if (outProbs)
						std::memcpy(outProbs->DataAs<float>() + qh * T * T, sp, (usize)(T * T) * sizeof(float));

					NkTensor ctxH = CpuMatmul2D(scores, Vh); // [T,hd]
					if (!ctxH.IsValid())
						return NkTensor();
					std::memcpy(ctxP + qh * T * hd, ctxH.DataAs<float>(), (usize)(T * hd) * sizeof(float));
				}
				return ctx;
			}

			bool NkGQAAttentionBackward(const NkTensor &Q, const NkTensor &K, const NkTensor &V, const NkTensor &probs,
										const NkTensor &dCtx, NkTensor &dQ, NkTensor &dK, NkTensor &dV) {
				if (!Q.IsValid() || !K.IsValid() || !V.IsValid() || !probs.IsValid() || !dCtx.IsValid() ||
					Q.Rank() != 3 || probs.Rank() != 3 || dCtx.Numel() != Q.Numel()) {
					fprintf(stderr, "[NkQwen2Backward] NkGQAAttentionBackward : formes incompatibles\n");
					return false;
				}
				const int64 nH = Q.Shape()[0], T = Q.Shape()[1], hd = Q.Shape()[2];
				const int64 nKV = K.Shape()[0];
				const int64 groupSize = nH / nKV;
				const float invSqrtHd = (float)(1.0 / std::sqrt((double)hd));

				dQ = NkTensor::Zeros(NkShape{nH, T, hd}, NkDType::NK_F32);
				dK = NkTensor::Zeros(NkShape{nKV, T, hd}, NkDType::NK_F32);
				dV = NkTensor::Zeros(NkShape{nKV, T, hd}, NkDType::NK_F32);
				float *dQp = dQ.DataAs<float>();
				float *dKp = dK.DataAs<float>();
				float *dVp = dV.DataAs<float>();

				for (int64 qh = 0; qh < nH; ++qh) {
					const int64 kvh = qh / groupSize;
					NkTensor Qh = Q.Slice(0, qh, qh + 1, 1).Reshape(NkShape{T, hd});
					NkTensor Kh = K.Slice(0, kvh, kvh + 1, 1).Reshape(NkShape{T, hd});
					NkTensor Vh = V.Slice(0, kvh, kvh + 1, 1).Reshape(NkShape{T, hd});
					NkTensor P = probs.Slice(0, qh, qh + 1, 1).Reshape(NkShape{T, T});
					NkTensor dCtxH = dCtx.Slice(0, qh, qh + 1, 1).Reshape(NkShape{T, hd});
					if (!Qh.IsValid() || !Kh.IsValid() || !Vh.IsValid() || !P.IsValid() || !dCtxH.IsValid())
						return false;

					// ctxH = P·Vh  =>  dP = dCtxH·Vhᵀ ; dVh += Pᵀ·dCtxH.
					NkTensor dP = NkLinearNoBias(dCtxH, Vh);	 // [T,T]
					NkTensor dVh = NkCpuMatmulATB(P, dCtxH); // [T,hd]
					if (!dP.IsValid() || !dVh.IsValid())
						return false;

					// Softmax causal backward (échelle 1/√hd remontée vers les
					// scores bruts) : ds_c = invSqrt·P_c·(dP_c − Σ P·dP).
					NkTensor dS = NkTensor::Zeros(NkShape{T, T}, NkDType::NK_F32);
					const float *pp = P.DataAs<float>();
					const float *dpp = dP.DataAs<float>();
					float *dsp = dS.DataAs<float>();
					for (int64 t = 0; t < T; ++t) {
						const float *prow = pp + t * T;
						const float *dprow = dpp + t * T;
						float *dsrow = dsp + t * T;
						double dot = 0.0;
						for (int64 c = 0; c <= t; ++c)
							dot += (double)prow[c] * (double)dprow[c];
						for (int64 c = 0; c <= t; ++c)
							dsrow[c] = (float)((double)invSqrtHd * (double)prow[c] * ((double)dprow[c] - dot));
						// c > t : colonnes du futur, gradient structurellement nul
						// (déjà zéro via Zeros).
					}

					// scores = Q·Kᵀ  =>  dQh = dS·Kh ; dKh += dSᵀ·Qh.
					NkTensor dQh = NkCpuMatmulAB(dS, Kh);	// [T,hd]
					NkTensor dKh = NkCpuMatmulATB(dS, Qh); // [T,hd]
					if (!dQh.IsValid() || !dKh.IsValid())
						return false;

					std::memcpy(dQp + qh * T * hd, dQh.DataAs<float>(), (usize)(T * hd) * sizeof(float));
					// GQA : les têtes Q du groupe ACCUMULENT sur leur tête K/V.
					const float *dkh = dKh.DataAs<float>();
					const float *dvh = dVh.DataAs<float>();
					float *dkDst = dKp + kvh * T * hd;
					float *dvDst = dVp + kvh * T * hd;
					for (int64 i = 0; i < T * hd; ++i) {
						dkDst[i] += dkh[i];
						dvDst[i] += dvh[i];
					}
				}
				return true;
			}

			// ---- Couche complète --------------------------------------------------

			NkTensor NkQwen2LayerForwardTrain(const NkQwen2Config &cfg, const NkQwen2LayerWeights &w,
											  const NkQwen2LoraSet &lora, const NkTensor &x, NkQwen2TrainSaved &saved) {
				if (!cfg.IsValid() || !w.IsValid(cfg)) {
					fprintf(stderr, "[NkQwen2Backward] ForwardTrain : cfg/poids invalides\n");
					return NkTensor();
				}
				if (!x.IsValid() || x.DType() != NkDType::NK_F32 || x.Rank() != 2 || x.Shape()[1] != cfg.dModel) {
					fprintf(stderr, "[NkQwen2Backward] ForwardTrain : x doit être [T,%d] f32\n", cfg.dModel);
					return NkTensor();
				}
				const int64 T = x.Shape()[0];
				const int64 d = cfg.dModel;
				const int64 headDim = cfg.headDim;
				const int64 nHeads = cfg.nHeads;
				const int64 nKVHeads = cfg.nKVHeads;

				// Clone défensif : entre ForwardTrain et Backward, l'appelant peut
				// réutiliser son buffer d'entrée (piège n°1 : copie = vue partagée).
				saved.x = x.Clone();

				// ---- Attention (pré-norme) ------------------------------------
				saved.xn1 = NkRMSNorm(saved.x, w.attnNorm, cfg.rmsEps);
				if (!saved.xn1.IsValid())
					return NkTensor();

				NkTensor Q2 = Linear(saved.xn1, w.wq, w.bq); // [T,nH*hd]
				NkTensor K2 = Linear(saved.xn1, w.wk, w.bk); // [T,nKV*hd]
				NkTensor V2 = Linear(saved.xn1, w.wv, w.bv); // [T,nKV*hd]
				if (!Q2.IsValid() || !K2.IsValid() || !V2.IsValid())
					return NkTensor();
				// Deltas LoRA : ajoutés APRÈS le socle (y = W₀x+b + delta) — chemin
				// strictement identique à l'inférence quand la paire est absente.
				if (lora.q.IsValid())
					Q2 = ops::Add(Q2, NkLoraForwardDelta(lora.q, saved.xn1));
				if (lora.k.IsValid())
					K2 = ops::Add(K2, NkLoraForwardDelta(lora.k, saved.xn1));
				if (lora.v.IsValid())
					V2 = ops::Add(V2, NkLoraForwardDelta(lora.v, saved.xn1));

				NkTensor Q = SplitHeadsAndPermute(Q2, T, nHeads, headDim);
				NkTensor K = SplitHeadsAndPermute(K2, T, nKVHeads, headDim);
				NkTensor V = SplitHeadsAndPermute(V2, T, nKVHeads, headDim);
				if (!Q.IsValid() || !K.IsValid() || !V.IsValid())
					return NkTensor();

				NkApplyRoPE(Q, 0, cfg.ropeFreqBase);
				NkApplyRoPE(K, 0, cfg.ropeFreqBase); // V : pas de RoPE
				saved.Q = Q;
				saved.K = K;
				saved.V = V;

				NkTensor ctx = NkGQAAttentionForward(Q, K, V, &saved.probs); // [nH,T,hd]
				if (!ctx.IsValid())
					return NkTensor();

				saved.ctxT = ctx.Permute(NkShape{1, 0, 2}).Contiguous().Reshape(NkShape{T, d});
				if (!saved.ctxT.IsValid())
					return NkTensor();
				NkTensor attnOut = NkLinearNoBias(saved.ctxT, w.wo);
				if (!attnOut.IsValid())
					return NkTensor();
				if (lora.o.IsValid())
					attnOut = ops::Add(attnOut, NkLoraForwardDelta(lora.o, saved.ctxT));
				saved.x1 = ops::Add(saved.x, attnOut); // résiduel

				// ---- MLP SwiGLU (pré-norme) -----------------------------------
				saved.xn2 = NkRMSNorm(saved.x1, w.ffnNorm, cfg.rmsEps);
				if (!saved.xn2.IsValid())
					return NkTensor();
				saved.g = NkLinearNoBias(saved.xn2, w.wGate); // PRÉ-SiLU [T,ffn]
				saved.u = NkLinearNoBias(saved.xn2, w.wUp);
				if (!saved.g.IsValid() || !saved.u.IsValid())
					return NkTensor();
				if (lora.gate.IsValid())
					saved.g = ops::Add(saved.g, NkLoraForwardDelta(lora.gate, saved.xn2));
				if (lora.up.IsValid())
					saved.u = ops::Add(saved.u, NkLoraForwardDelta(lora.up, saved.xn2));

				saved.h = ops::Mul(NkSiLU(saved.g), saved.u); // silu(g)⊙u
				NkTensor down = NkLinearNoBias(saved.h, w.wDown);
				if (!down.IsValid())
					return NkTensor();
				if (lora.down.IsValid())
					down = ops::Add(down, NkLoraForwardDelta(lora.down, saved.h));

				return ops::Add(saved.x1, down); // résiduel
			}

			bool NkQwen2LayerBackward(const NkQwen2Config &cfg, const NkQwen2LayerWeights &w, const NkQwen2LoraSet &lora,
									  const NkQwen2TrainSaved &saved, const NkTensor &dOut, NkTensor &dX,
									  NkQwen2LoraSetGrads &grads) {
				if (!cfg.IsValid() || !saved.x.IsValid() || !saved.xn1.IsValid() || !saved.probs.IsValid() ||
					!saved.h.IsValid() || !dOut.IsValid() || dOut.Numel() != saved.x.Numel()) {
					fprintf(stderr, "[NkQwen2Backward] Backward : activations/dOut invalides (ForwardTrain oublié ?)\n");
					return false;
				}
				const int64 headDim = cfg.headDim;
				const int64 nHeads = cfg.nHeads;
				const int64 nKVHeads = cfg.nKVHeads;
				NkTensor dOutC = dOut.IsContiguous() ? dOut : dOut.Contiguous();

				// ---- résiduel final : y = x1 + down(h) ------------------------
				// dDown = dOut ; la part directe vers x1 est ajoutée plus bas.

				// ---- down (socle gelé + LoRA) ---------------------------------
				NkTensor dH = NkCpuMatmulAB(dOutC, w.wDown); // [T,ffn] = dY·W
				if (!dH.IsValid())
					return false;
				if (lora.down.IsValid() && !NkLoraBackward(lora.down, saved.h, dOutC, grads.down, &dH))
					return false;

				// ---- SwiGLU ---------------------------------------------------
				NkTensor dG, dU;
				if (!NkSwiGLUBackward(saved.g, saved.u, dH, dG, dU))
					return false;

				// ---- gate / up ------------------------------------------------
				NkTensor dXn2 = NkCpuMatmulAB(dG, w.wGate); // [T,d]
				if (!dXn2.IsValid() || !AccumInPlace(dXn2, NkCpuMatmulAB(dU, w.wUp)))
					return false;
				if (lora.gate.IsValid() && !NkLoraBackward(lora.gate, saved.xn2, dG, grads.gate, &dXn2))
					return false;
				if (lora.up.IsValid() && !NkLoraBackward(lora.up, saved.xn2, dU, grads.up, &dXn2))
					return false;

				// ---- RMSNorm ffn + résiduel : x1 reçoit la branche MLP ET dOut -
				NkTensor dX1 = NkRMSNormBackward(saved.x1, w.ffnNorm, cfg.rmsEps, dXn2);
				if (!dX1.IsValid() || !AccumInPlace(dX1, dOutC))
					return false;

				// ---- projection O (dAttnOut = dX1) ----------------------------
				NkTensor dCtxT = NkCpuMatmulAB(dX1, w.wo); // [T,nH*hd]
				if (!dCtxT.IsValid())
					return false;
				if (lora.o.IsValid() && !NkLoraBackward(lora.o, saved.ctxT, dX1, grads.o, &dCtxT))
					return false;

				// ---- attention GQA --------------------------------------------
				NkTensor dCtx = SplitFlatToHeads(dCtxT, nHeads, headDim); // [nH,T,hd]
				NkTensor dQ, dK, dV;
				if (!NkGQAAttentionBackward(saved.Q, saved.K, saved.V, saved.probs, dCtx, dQ, dK, dV))
					return false;

				// ---- RoPE inverse (rotation orthogonale, angle opposé) --------
				NkApplyRoPEBackward(dQ, 0, cfg.ropeFreqBase);
				NkApplyRoPEBackward(dK, 0, cfg.ropeFreqBase);

				// ---- projections Q/K/V (socle gelé : dX seulement + LoRA) -----
				NkTensor dQ2 = MergeHeadsToFlat(dQ); // [T,nH*hd]
				NkTensor dK2 = MergeHeadsToFlat(dK); // [T,nKV*hd]
				NkTensor dV2 = MergeHeadsToFlat(dV);
				(void)nKVHeads;
				NkTensor dXn1 = NkCpuMatmulAB(dQ2, w.wq); // [T,d]
				if (!dXn1.IsValid() || !AccumInPlace(dXn1, NkCpuMatmulAB(dK2, w.wk)) ||
					!AccumInPlace(dXn1, NkCpuMatmulAB(dV2, w.wv)))
					return false;
				if (lora.q.IsValid() && !NkLoraBackward(lora.q, saved.xn1, dQ2, grads.q, &dXn1))
					return false;
				if (lora.k.IsValid() && !NkLoraBackward(lora.k, saved.xn1, dK2, grads.k, &dXn1))
					return false;
				if (lora.v.IsValid() && !NkLoraBackward(lora.v, saved.xn1, dV2, grads.v, &dXn1))
					return false;

				// ---- RMSNorm attn + résiduel d'entrée : x apparaît deux fois --
				dX = NkRMSNormBackward(saved.x, w.attnNorm, cfg.rmsEps, dXn1);
				if (!dX.IsValid() || !AccumInPlace(dX, dX1))
					return false;
				return true;
			}

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
