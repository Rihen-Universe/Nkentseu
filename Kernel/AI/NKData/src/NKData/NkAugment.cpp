// =============================================================================
// NKData/NkAugment.cpp — split train/val/test + augmentation (NKAI).
// =============================================================================
#include "NKData/NkAugment.h"

#include <cstring>
#include <cmath>

namespace nkentseu {
	namespace ai {
		namespace data {

			// Rassemble les exemples de `ds` désignés par `idx` en un nouveau NkDataset.
			static NkDataset Gather(const NkDataset &ds, const NkVector<int32> &idx) {
				const uint32 D = ds.FeatureDim();
				NkTensor X = NkTensor::Zeros(NkShape{(int64)idx.Size(), (int64)D});
				float *xp = X.DataAs<float>();
				const float *src = ds.Features().Contiguous().DataAs<float>();
				NkVector<int32> Y;
				Y.Reserve(idx.Size());
				for (uint32 i = 0; i < idx.Size(); ++i) {
					const int32 id = idx[i];
					memcpy(xp + (nk_size)i * D, src + (nk_size)id * D, (nk_size)D * sizeof(float));
					Y.PushBack(ds.Labels()[(uint32)id]);
				}
				return NkDataset(X, Y, ds.NumClasses());
			}

			NkSplit SplitDataset(const NkDataset &ds, double trainFrac, double valFrac, double testFrac, uint32 seed) {
				NkSplit out;
				const uint32 N = ds.Size();
				NkVector<int32> order;
				order.Reserve(N);
				for (uint32 i = 0; i < N; ++i)
					order.PushBack((int32)i);

				// Fisher-Yates déterministe (même LCG que NkDataLoader::Shuffle).
				uint32 s = seed ? seed : 1u;
				for (uint32 i = N; i > 1; --i) {
					s = s * 1664525u + 1013904223u;
					const uint32 j = s % i;
					const int32 tmp = order[i - 1];
					order[i - 1] = order[j];
					order[j] = tmp;
				}

				double total = trainFrac + valFrac + testFrac;
				if (total <= 0.0)
					total = 1.0;
				uint32 nTrain = (uint32)((double)N * trainFrac / total + 0.5);
				uint32 nVal = (uint32)((double)N * valFrac / total + 0.5);
				if (nTrain > N)
					nTrain = N;
				if (nTrain + nVal > N)
					nVal = N - nTrain;
				const uint32 nTest = N - nTrain - nVal;

				NkVector<int32> ti, vi, tei;
				ti.Reserve(nTrain);
				vi.Reserve(nVal);
				tei.Reserve(nTest);
				for (uint32 i = 0; i < nTrain; ++i)
					ti.PushBack(order[i]);
				for (uint32 i = 0; i < nVal; ++i)
					vi.PushBack(order[nTrain + i]);
				for (uint32 i = 0; i < nTest; ++i)
					tei.PushBack(order[nTrain + nVal + i]);

				out.train = Gather(ds, ti);
				out.val = Gather(ds, vi);
				out.test = Gather(ds, tei);
				return out;
			}

			NkDataset ConcatDatasets(const NkDataset &a, const NkDataset &b) {
				if (!a.IsValid())
					return b;
				if (!b.IsValid() || b.FeatureDim() != a.FeatureDim() || b.NumClasses() != a.NumClasses())
					return a;
				const uint32 D = a.FeatureDim();
				const uint32 Na = a.Size(), Nb = b.Size();
				NkTensor X = NkTensor::Zeros(NkShape{(int64)(Na + Nb), (int64)D});
				float *xp = X.DataAs<float>();
				const float *ap = a.Features().Contiguous().DataAs<float>();
				const float *bp = b.Features().Contiguous().DataAs<float>();
				memcpy(xp, ap, (nk_size)Na * D * sizeof(float));
				memcpy(xp + (nk_size)Na * D, bp, (nk_size)Nb * D * sizeof(float));
				NkVector<int32> Y;
				Y.Reserve(Na + Nb);
				for (uint32 i = 0; i < Na; ++i)
					Y.PushBack(a.Labels()[i]);
				for (uint32 i = 0; i < Nb; ++i)
					Y.PushBack(b.Labels()[i]);
				return NkDataset(X, Y, a.NumClasses());
			}

			NkDataset AugmentFlipHorizontal(const NkDataset &ds, uint32 rows, uint32 cols) {
				if (!ds.IsValid() || rows * cols != ds.FeatureDim())
					return ds;
				const uint32 N = ds.Size(), D = ds.FeatureDim();
				NkTensor X = NkTensor::Zeros(NkShape{(int64)N, (int64)D});
				float *xp = X.DataAs<float>();
				const float *src = ds.Features().Contiguous().DataAs<float>();
				for (uint32 i = 0; i < N; ++i) {
					for (uint32 r = 0; r < rows; ++r) {
						for (uint32 c = 0; c < cols; ++c) {
							const nk_size srcAt = (nk_size)i * D + (nk_size)r * cols + c;
							const nk_size dstAt = (nk_size)i * D + (nk_size)r * cols + (cols - 1 - c);
							xp[dstAt] = src[srcAt];
						}
					}
				}
				NkVector<int32> Y;
				Y.Reserve(N);
				for (uint32 i = 0; i < N; ++i)
					Y.PushBack(ds.Labels()[i]);
				return NkDataset(X, Y, ds.NumClasses());
			}

			// LCG [0,1) déterministe (même famille que NkDataLoader/MakeBlobs).
			static double NextUniform(uint32 &s) {
				s = s * 1664525u + 1013904223u;
				return (double)((s >> 9) & 0x7FFFu) / 32768.0; // [0,1)
			}

			NkDataset AugmentGaussianNoise(const NkDataset &ds, float stddev, uint32 seed) {
				if (!ds.IsValid())
					return ds;
				const uint32 N = ds.Size(), D = ds.FeatureDim();
				NkTensor X = NkTensor::Zeros(NkShape{(int64)N, (int64)D});
				float *xp = X.DataAs<float>();
				const float *src = ds.Features().Contiguous().DataAs<float>();
				uint32 s = seed ? seed : 1u;
				const nk_size total = (nk_size)N * D;
				for (nk_size i = 0; i < total; ++i) {
					// Box-Muller : deux uniformes -> une gaussienne standard.
					double u1 = NextUniform(s);
					double u2 = NextUniform(s);
					if (u1 < 1e-12)
						u1 = 1e-12;
					const double z = std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * 3.14159265358979323846 * u2);
					xp[i] = src[i] + (float)(z * (double)stddev);
				}
				NkVector<int32> Y;
				Y.Reserve(N);
				for (uint32 i = 0; i < N; ++i)
					Y.PushBack(ds.Labels()[i]);
				return NkDataset(X, Y, ds.NumClasses());
			}

			NkVector<int32> AugmentTokenDropout(const NkVector<int32> &ids, float dropProb, uint32 seed) {
				NkVector<int32> out;
				uint32 s = seed ? seed : 1u;
				const int64 n = (int64)ids.Size();
				out.Reserve((nk_size)n);
				for (int64 i = 0; i < n; ++i) {
					const double u = NextUniform(s);
					if (u >= (double)dropProb)
						out.PushBack(ids[(nk_size)i]);
				}
				if (out.Size() == 0 && n > 0)
					out.PushBack(ids[0]); // jamais de séquence vide en sortie si l'entrée ne l'était pas
				return out;
			}

		} // namespace data
	} // namespace ai
} // namespace nkentseu
