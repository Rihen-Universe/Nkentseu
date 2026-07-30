// =============================================================================
// NkDiffusion.cpp — implémentation du DDPM jouet (NKAI, Phase 6).
// Algorithme : Ho, Jain, Abbeel, « Denoising Diffusion Probabilistic Models »,
// NeurIPS 2020, arXiv:2006.11239 (cf NkDiffusion.h pour le détail des choix).
// =============================================================================
#include "NKGen/NkDiffusion.h"

#include <cmath>

namespace nkentseu {
	namespace ai {
		namespace gen {

			// -----------------------------------------------------------------
			// RNG gaussien local (LCG + Box-Muller), même schéma que les autres
			// bancs d'essai de ce module (ex. NKObjectGenTest::FillRandn) : pas de
			// dépendance supplémentaire, suffisant pour du bruit d'entraînement/
			// échantillonnage jouet. Remplit `t` (déjà alloué) de N(0,1) i.i.d.
			// -----------------------------------------------------------------
			static void FillGaussian(NkTensor &t, uint32 &s) {
				float *p = t.DataAs<float>();
				int64 n = NkShapeNumel(t.Shape());
				for (int64 i = 0; i < n; ++i) {
					s = s * 1664525u + 1013904223u;
					double u1 = (double)((s >> 8) & 0xFFFFu) / 65535.0;
					s = s * 1664525u + 1013904223u;
					double u2 = (double)((s >> 8) & 0xFFFFu) / 65535.0;
					if (u1 < 1e-7)
						u1 = 1e-7;
					p[i] = (float)(std::sqrt(-2.0 * std::log(u1)) * std::cos(6.2831853 * u2));
				}
			}

			// -------------------------------------------------------------------
			// NkDiffusionSchedule
			// -------------------------------------------------------------------
			NkDiffusionSchedule NkDiffusionSchedule::Linear(uint32 T, float betaStart, float betaEnd) {
				NkDiffusionSchedule s;
				s.T = T;
				s.betas.Resize(T);
				s.alphas.Resize(T);
				s.alphaBars.Resize(T);
				float acc = 1.0f;
				for (uint32 t = 0; t < T; ++t) {
					// Interpolation linéaire de β sur [betaStart, betaEnd] (Ho et al. 2020 §4).
					float frac = (T > 1) ? (float)t / (float)(T - 1) : 0.0f;
					float beta = betaStart + (betaEnd - betaStart) * frac;
					float alpha = 1.0f - beta;
					acc *= alpha;
					s.betas[t] = beta;
					s.alphas[t] = alpha;
					s.alphaBars[t] = acc;
				}
				return s;
			}

			// -------------------------------------------------------------------
			// Embedding sinusoidal du pas de temps (encodage positionnel Transformer,
			// Vaswani et al. 2017 — repris pour conditionner le réseau sur t).
			// -------------------------------------------------------------------
			void NkTimeEmbedding(uint32 t, uint32 dim, float *out) {
				for (uint32 i = 0; i < dim; ++i)
					out[i] = 0.0f;
				uint32 half = dim / 2u;
				for (uint32 i = 0; i < half; ++i) {
					double exponent = (2.0 * (double)i) / (double)dim;
					double freq = 1.0 / std::pow(10000.0, exponent);
					double angle = (double)t * freq;
					out[2u * i] = (float)std::sin(angle);
					out[2u * i + 1u] = (float)std::cos(angle);
				}
			}

			// -------------------------------------------------------------------
			// Embedding de classe (one-hot, fixe/non appris — cf NkDiffusion.h).
			// -------------------------------------------------------------------
			void NkClassEmbedding(uint32 c, uint32 numClasses, float *out) {
				for (uint32 i = 0; i < numClasses; ++i)
					out[i] = 0.0f;
				if (numClasses > 0u && c < numClasses)
					out[c] = 1.0f;
			}

			// -------------------------------------------------------------------
			// Processus direct (forward), forme fermée.
			// -------------------------------------------------------------------
			NkTensor NkDiffusionForward(const NkTensor &x0, const NkDiffusionSchedule &sched, const uint32 *tIdx,
										const NkTensor &eps) {
				NkTensor xt = NkTensor::Zeros(x0.Shape());
				const float *x0p = x0.DataAs<float>();
				const float *epsp = eps.DataAs<float>();
				float *xtp = xt.DataAs<float>();

				int64 B = x0.Shape()[0];
				int64 dataDim = (x0.Rank() > 1) ? x0.Shape()[1] : 1;

				for (int64 b = 0; b < B; ++b) {
					uint32 t = tIdx[b];
					float sqrtAlphaBar = std::sqrt(sched.alphaBars[t]);
					float sqrtOneMinusAlphaBar = std::sqrt(1.0f - sched.alphaBars[t]);
					for (int64 d = 0; d < dataDim; ++d) {
						int64 i = b * dataDim + d;
						xtp[i] = sqrtAlphaBar * x0p[i] + sqrtOneMinusAlphaBar * epsp[i];
					}
				}
				return xt;
			}

			// -------------------------------------------------------------------
			// NkDiffusion — réseau ε_θ(x_t, t).
			// -------------------------------------------------------------------
			NkDiffusion::NkDiffusion(uint32 dataDim, uint32 timeEmbedDim, uint32 hidden, uint32 seed, uint32 numClasses)
				: mL1(dataDim + timeEmbedDim + numClasses, hidden, seed + 1u), mL2(hidden, hidden, seed + 17u),
				  mL3(hidden, dataDim, seed + 31u), mDataDim(dataDim), mTimeEmbedDim(timeEmbedDim), mHidden(hidden),
				  mNumClasses(numClasses) {
			}

			NkVar NkDiffusion::PredictNoise(const NkVar &xtWithTime) const {
				NkVar h = nn::Relu(mL1.Forward(xtWithTime));
				h = nn::Relu(mL2.Forward(h));
				return mL3.Forward(h); // pas d'activation finale : le bruit n'est pas borné
			}

			NkTensor NkDiffusion::BuildModelInput(const NkTensor &xt, const uint32 *tIdx, const uint32 *classIdx) const {
				int64 B = xt.Shape()[0];
				uint32 cols = mDataDim + mTimeEmbedDim + mNumClasses;
				NkTensor input = NkTensor::Zeros(NkShape{B, (int64)cols});
				const float *xp = xt.DataAs<float>();
				float *ip = input.DataAs<float>();

				for (int64 b = 0; b < B; ++b) {
					// point bruité (dataDim premières colonnes)
					for (uint32 d = 0; d < mDataDim; ++d)
						ip[b * cols + d] = xp[b * mDataDim + d];
					// embedding temporel (colonnes suivantes) — concaténation manuelle,
					// cf commentaire de NkDiffusion.h (pas d'op Concat "axe features").
					NkTimeEmbedding(tIdx[b], mTimeEmbedDim, ip + b * cols + mDataDim);
					// embedding de classe one-hot (dernières colonnes, si conditionné) —
					// même schéma manuel que le temps (cf commentaire de fichier).
					if (mNumClasses > 0u) {
						uint32 c = classIdx ? classIdx[b] : 0u;
						NkClassEmbedding(c, mNumClasses, ip + b * cols + mDataDim + mTimeEmbedDim);
					}
				}
				return input;
			}

			void NkDiffusion::Parameters(NkVector<NkVar> &out) const {
				mL1.Parameters(out);
				mL2.Parameters(out);
				mL3.Parameters(out);
			}

			NkTensor NkDiffusion::Sample(uint32 count, const NkDiffusionSchedule &sched, uint32 rngSeed,
										  const uint32 *classIdx) const {
				NkTensor x = NkTensor::Zeros(NkShape{(int64)count, (int64)mDataDim});
				uint32 rng = rngSeed;
				FillGaussian(x, rng); // x_T ~ N(0,1) : point de départ = bruit PUR.

				NkVector<uint32> tIdx((NkVector<uint32>::SizeType)count, 0u);

				for (int32 ti = (int32)sched.T - 1; ti >= 0; --ti) {
					uint32 t = (uint32)ti;
					for (uint32 b = 0; b < count; ++b)
						tIdx[b] = t;

					NkTensor modelIn = BuildModelInput(x, tIdx.Data(), classIdx);
					NkVar xin = NkVar::Leaf(modelIn, false);
					NkTensor epsPred = PredictNoise(xin).Value().Contiguous();

					float alpha = sched.alphas[t];
					float alphaBar = sched.alphaBars[t];
					float beta = sched.betas[t];
					float coefX = 1.0f / std::sqrt(alpha);
					float coefEps = beta / std::sqrt(1.0f - alphaBar);
					float sigma = std::sqrt(beta); // variance "large" (Ho et al. 2020, Algorithme 2)

					NkTensor noiseZ = NkTensor::Zeros(x.Shape());
					if (t > 0)
						FillGaussian(noiseZ, rng); // z = 0 au dernier pas (t=0), sinon N(0,1)

					float *xp = x.DataAs<float>();
					const float *ep = epsPred.DataAs<float>();
					const float *zp = noiseZ.DataAs<float>();
					int64 n = NkShapeNumel(x.Shape());
					for (int64 i = 0; i < n; ++i) {
						float mean = coefX * (xp[i] - coefEps * ep[i]);
						xp[i] = mean + sigma * zp[i];
					}
				}
				return x;
			}

		} // namespace gen
	} // namespace ai
} // namespace nkentseu
