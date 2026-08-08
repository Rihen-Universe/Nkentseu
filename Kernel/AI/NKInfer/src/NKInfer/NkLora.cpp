// =============================================================================
// NkLora.cpp — voir NkLora.h pour le principe, les dérivées et les sources.
// =============================================================================
#include "NKInfer/NkLora.h"
#include "NKInfer/NkQwen2Block.h" // NkLinearNoBias (x·Wᵀ CPU strict)

#include <cstdio>
#include <cmath>

namespace nkentseu {
	namespace ai {
		namespace infer {

			// ---- Produits matriciels CPU stricts ---------------------------------

			NkTensor NkCpuMatmulAB(const NkTensor &a, const NkTensor &b) {
				if (!a.IsValid() || !b.IsValid() || a.Rank() != 2 || b.Rank() != 2 || a.Shape()[1] != b.Shape()[0] ||
					a.DType() != NkDType::NK_F32 || b.DType() != NkDType::NK_F32) {
					fprintf(stderr, "[NkLora] NkCpuMatmulAB : formes/dtype incompatibles\n");
					return NkTensor();
				}
				NkTensor ac = a.IsContiguous() ? a : a.Contiguous();
				NkTensor bc = b.IsContiguous() ? b : b.Contiguous();
				const int64 M = ac.Shape()[0], K = ac.Shape()[1], N = bc.Shape()[1];
				NkTensor out = NkTensor::Zeros(NkShape{M, N}, NkDType::NK_F32);
				const float *A = ac.DataAs<float>();
				const float *B = bc.DataAs<float>();
				float *C = out.DataAs<float>();
				// Ordre i-k-j (cache-friendly), avec saut des a[i,k] nuls : les
				// gradients causaux (colonnes futures masquées) sont pleins de zéros
				// structurels — les sauter est gratuit et strictement équivalent.
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

			NkTensor NkCpuMatmulATB(const NkTensor &a, const NkTensor &b) {
				if (!a.IsValid() || !b.IsValid() || a.Rank() != 2 || b.Rank() != 2 || a.Shape()[0] != b.Shape()[0] ||
					a.DType() != NkDType::NK_F32 || b.DType() != NkDType::NK_F32) {
					fprintf(stderr, "[NkLora] NkCpuMatmulATB : formes/dtype incompatibles\n");
					return NkTensor();
				}
				NkTensor ac = a.IsContiguous() ? a : a.Contiguous();
				NkTensor bc = b.IsContiguous() ? b : b.Contiguous();
				const int64 K = ac.Shape()[0], M = ac.Shape()[1], N = bc.Shape()[1];
				NkTensor out = NkTensor::Zeros(NkShape{M, N}, NkDType::NK_F32);
				const float *A = ac.DataAs<float>();
				const float *B = bc.DataAs<float>();
				float *C = out.DataAs<float>();
				// out[m,n] = Σ_k a[k,m]·b[k,n]. Boucle k EXTERNE : a et b sont lus
				// ligne par ligne dans leur disposition d'origine (aucune transposée
				// matérialisée — piège n°4 des notes QLoRA).
				for (int64 k = 0; k < K; ++k) {
					const float *Ak = A + k * M;
					const float *Bk = B + k * N;
					for (int64 m = 0; m < M; ++m) {
						const float akm = Ak[m];
						if (akm == 0.0f)
							continue;
						float *Cm = C + m * N;
						for (int64 n = 0; n < N; ++n)
							Cm[n] += akm * Bk[n];
					}
				}
				return out;
			}

			// ---- RNG reproductible ------------------------------------------------

			NkLoraRng::NkLoraRng(uint64 seed) {
				// Un état nul serait un point fixe du xorshift : on le décale par la
				// constante de hachage doré (même graine 0 -> suite non dégénérée).
				state = seed ? seed : 0x9E3779B97F4A7C15ull;
			}

			uint64 NkLoraRng::NextU64() {
				// xorshift64* (Vigna) : période 2^64-1, suffisant pour des inits de test.
				uint64 x = state;
				x ^= x >> 12;
				x ^= x << 25;
				x ^= x >> 27;
				state = x;
				return x * 0x2545F4914F6CDD1Dull;
			}

			float64 NkLoraRng::NextUniform() {
				// 53 bits de mantisse -> uniforme sur (0,1] (jamais 0 : Box-Muller
				// prend log(u), qui exploserait en 0).
				const uint64 bits = (NextU64() >> 11) + 1ull; // 1 .. 2^53
				return (float64)bits * (1.0 / 9007199254740992.0);
			}

			float32 NkLoraRng::NextGaussian() {
				if (mHasSpare) {
					mHasSpare = false;
					return (float32)mSpare;
				}
				// Box-Muller : deux uniformes -> deux gaussiennes indépendantes.
				const float64 u1 = NextUniform();
				const float64 u2 = NextUniform();
				const float64 m = std::sqrt(-2.0 * std::log(u1));
				const float64 twoPi = 6.28318530717958647692;
				mSpare = m * std::sin(twoPi * u2);
				mHasSpare = true;
				return (float32)(m * std::cos(twoPi * u2));
			}

			// ---- NkLoraPair -------------------------------------------------------

			NkLoraPair NkLoraPair::Create(int32 outFeatures, int32 inFeatures, int32 r, float32 alpha, float32 sigma,
										  NkLoraRng &rng) {
				NkLoraPair p;
				if (outFeatures <= 0 || inFeatures <= 0 || r <= 0) {
					fprintf(stderr, "[NkLora] Create : dimensions invalides (out=%d,in=%d,r=%d)\n", outFeatures,
							inFeatures, r);
					return p;
				}
				p.r = r;
				p.alpha = alpha;
				p.A = NkTensor::Empty(NkShape{(int64)r, (int64)inFeatures}, NkDType::NK_F32);
				float *ap = p.A.DataAs<float>();
				const int64 nA = p.A.Numel();
				for (int64 i = 0; i < nA; ++i)
					ap[i] = sigma * rng.NextGaussian();
				// B = 0 : delta initial EXACTEMENT nul (la sortie reste celle du socle).
				p.B = NkTensor::Zeros(NkShape{(int64)outFeatures, (int64)r}, NkDType::NK_F32);
				return p;
			}

			NkTensor NkLoraForwardDelta(const NkLoraPair &p, const NkTensor &x) {
				if (!p.IsValid() || !x.IsValid() || x.Rank() != 2 || x.Shape()[1] != p.A.Shape()[1]) {
					fprintf(stderr, "[NkLora] ForwardDelta : paire ou entrée invalide\n");
					return NkTensor();
				}
				// u = x·Aᵀ [T,r] ; delta = (u·Bᵀ)·scale [T,out]. NkLinearNoBias couvre
				// exactement l'orientation x[T,K]·W[N,K]ᵀ des deux produits.
				NkTensor u = NkLinearNoBias(x, p.A);
				if (!u.IsValid())
					return NkTensor();
				NkTensor delta = NkLinearNoBias(u, p.B);
				if (!delta.IsValid())
					return NkTensor();
				const float scale = p.Scale();
				float *dp = delta.DataAs<float>();
				const int64 n = delta.Numel();
				for (int64 i = 0; i < n; ++i)
					dp[i] *= scale;
				return delta;
			}

			// Accumule src (+=) dans dst (mêmes formes contiguës). Alloue dst à zéro
			// si invalide — sémantique « += depuis zéro » des gradients.
			static bool AccumInto(NkTensor &dst, const NkTensor &src) {
				if (!src.IsValid())
					return false;
				if (!dst.IsValid())
					dst = NkTensor::Zeros(src.Shape(), NkDType::NK_F32);
				if (dst.Numel() != src.Numel()) {
					fprintf(stderr, "[NkLora] AccumInto : formes incompatibles\n");
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

			bool NkLoraBackward(const NkLoraPair &p, const NkTensor &x, const NkTensor &dY, NkLoraGrad &g,
								NkTensor *dXAccum) {
				if (!p.IsValid() || !x.IsValid() || !dY.IsValid() || x.Rank() != 2 || dY.Rank() != 2 ||
					x.Shape()[0] != dY.Shape()[0] || x.Shape()[1] != p.A.Shape()[1] ||
					dY.Shape()[1] != p.B.Shape()[0]) {
					fprintf(stderr, "[NkLora] Backward : formes incompatibles\n");
					return false;
				}
				const float scale = p.Scale();

				// u = x·Aᵀ recalculé (checkpointing absorbé : coût négligeable vs stockage).
				NkTensor u = NkLinearNoBias(x, p.A); // [T,r]
				if (!u.IsValid())
					return false;

				// dB = scale · dYᵀ·u.
				NkTensor dB = NkCpuMatmulATB(dY, u); // [out,r]
				if (!dB.IsValid())
					return false;
				{
					float *bp = dB.DataAs<float>();
					const int64 n = dB.Numel();
					for (int64 i = 0; i < n; ++i)
						bp[i] *= scale;
				}

				// du = scale · dY·B.
				NkTensor du = NkCpuMatmulAB(dY, p.B); // [T,r]
				if (!du.IsValid())
					return false;
				{
					float *up = du.DataAs<float>();
					const int64 n = du.Numel();
					for (int64 i = 0; i < n; ++i)
						up[i] *= scale;
				}

				// dA = duᵀ·x ; dX += du·A.
				NkTensor dA = NkCpuMatmulATB(du, x); // [r,in]
				if (!dA.IsValid())
					return false;

				if (!AccumInto(g.dA, dA) || !AccumInto(g.dB, dB))
					return false;
				if (dXAccum) {
					NkTensor dX = NkCpuMatmulAB(du, p.A); // [T,in]
					if (!dX.IsValid() || !AccumInto(*dXAccum, dX))
						return false;
				}
				return true;
			}

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
