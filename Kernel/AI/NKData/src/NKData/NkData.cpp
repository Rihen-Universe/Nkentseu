// =============================================================================
// NkData.cpp — implémentation des jeux de données et lots (NKAI, Phase 3).
// =============================================================================
#include "NKData/NkData.h"

#include <cstdio>
#include <cstring>
#include <cmath>

namespace nkentseu {
    namespace ai {
        namespace data {

            // =================================================================
            // NkDataset
            // =================================================================
            NkDataset::NkDataset(const NkTensor& features, const NkVector<int32>& labels,
                                 uint32 numClasses)
                : mY(labels), mNumClasses(numClasses) {
                NkTensor f = features.Contiguous();
                const NkShape& sh = f.Shape();
                if (sh.Size() >= 2) {
                    mX   = f;
                    mSize = (uint32)sh[0];
                    mDim  = (uint32)sh[1];
                }
            }

            // =================================================================
            // NkDataLoader
            // =================================================================
            NkDataLoader::NkDataLoader(const NkDataset& ds, uint32 batchSize,
                                       bool shuffle, uint32 seed)
                : mDs(ds), mBatchSize(batchSize ? batchSize : 1),
                  mShuffle(shuffle), mSeed(seed ? seed : 1u) {
                const uint32 n = mDs.Size();
                mOrder.Reserve(n);
                for (uint32 i = 0; i < n; ++i) mOrder.PushBack((int32)i);
                mNumBatches = (n + mBatchSize - 1) / mBatchSize;   // dernier lot partiel OK
                if (mShuffle) Shuffle();
            }

            void NkDataLoader::Shuffle() {
                if (!mShuffle) return;
                const uint32 n = mOrder.Size();
                uint32 s = mSeed;
                for (uint32 i = n; i > 1; --i) {          // Fisher-Yates
                    s = s * 1664525u + 1013904223u;
                    uint32 j = s % i;                     // 0..i-1
                    int32 tmp = mOrder[i - 1]; mOrder[i - 1] = mOrder[j]; mOrder[j] = tmp;
                }
                mSeed = s;                                // évolue -> permutation différente/époque
            }

            NkBatch NkDataLoader::GetBatch(uint32 index) const {
                NkBatch batch;
                const uint32 n = mDs.Size();
                const uint32 D = mDs.FeatureDim();
                const uint32 C = mDs.NumClasses();
                const uint32 start = index * mBatchSize;
                if (start >= n) return batch;                 // hors bornes -> lot vide
                uint32 B = mBatchSize;
                if (start + B > n) B = n - start;             // dernier lot partiel

                batch.size    = B;
                batch.inputs  = NkTensor::Zeros(NkShape{ (int64)B, (int64)D });
                batch.targets = NkTensor::Zeros(NkShape{ (int64)B, (int64)C });
                float*       inp = batch.inputs.DataAs<float>();
                float*       tgt = batch.targets.DataAs<float>();
                const float* Xp  = mDs.Features().DataAs<float>();   // [N, D] contigu

                for (uint32 k = 0; k < B; ++k) {
                    const int32 idx = mOrder[start + k];
                    memcpy(inp + (nk_size)k * D, Xp + (nk_size)idx * D, (nk_size)D * sizeof(float));
                    const int32 lbl = mDs.Labels()[(uint32)idx];
                    batch.labels.PushBack(lbl);
                    if (lbl >= 0 && (uint32)lbl < C)
                        tgt[(nk_size)k * C + (uint32)lbl] = 1.0f;
                }
                return batch;
            }

            // =================================================================
            // Générateur synthétique : amas 2D bien séparés sur un cercle.
            // =================================================================
            NkDataset MakeBlobs(uint32 numClasses, uint32 perClass, uint32 seed) {
                const uint32 N = numClasses * perClass;
                NkTensor X = NkTensor::Zeros(NkShape{ (int64)N, (int64)2 });
                NkVector<int32> Y; Y.Reserve(N);
                float* xp = X.DataAs<float>();
                uint32 s = seed ? seed : 1u;
                const double R = 3.0, TWO_PI = 6.283185307179586;
                for (uint32 c = 0; c < numClasses; ++c) {
                    const double a  = TWO_PI * (double)c / (double)(numClasses ? numClasses : 1);
                    const double cx = R * std::cos(a), cy = R * std::sin(a);
                    for (uint32 k = 0; k < perClass; ++k) {
                        const uint32 i = c * perClass + k;
                        s = s * 1664525u + 1013904223u;
                        double jx = ((double)((s >> 9) & 0x7FFFu) / 32767.0 - 0.5) * 1.0;
                        s = s * 1664525u + 1013904223u;
                        double jy = ((double)((s >> 9) & 0x7FFFu) / 32767.0 - 0.5) * 1.0;
                        xp[i * 2 + 0] = (float)(cx + jx);
                        xp[i * 2 + 1] = (float)(cy + jy);
                        Y.PushBack((int32)c);
                    }
                }
                return NkDataset(X, Y, numClasses);
            }

            // =================================================================
            // Chargeur MNIST (format IDX, big-endian).
            // =================================================================
            static bool ReadU32BE(FILE* f, uint32& out) {
                uint8 b[4];
                if (fread(b, 1, 4, f) != 4) return false;
                out = ((uint32)b[0] << 24) | ((uint32)b[1] << 16) |
                      ((uint32)b[2] << 8)  |  (uint32)b[3];
                return true;
            }

            NkDataset LoadMnist(const char* imagesPath, const char* labelsPath) {
                NkDataset invalid;
                FILE* fi = fopen(imagesPath, "rb");
                FILE* fl = fopen(labelsPath, "rb");
                if (!fi || !fl) { if (fi) fclose(fi); if (fl) fclose(fl); return invalid; }

                uint32 magicI = 0, nI = 0, rows = 0, cols = 0;
                uint32 magicL = 0, nL = 0;
                bool ok = ReadU32BE(fi, magicI) && ReadU32BE(fi, nI) &&
                          ReadU32BE(fi, rows)   && ReadU32BE(fi, cols) &&
                          ReadU32BE(fl, magicL) && ReadU32BE(fl, nL);
                if (!ok || magicI != 2051u || magicL != 2049u || nI != nL || nI == 0) {
                    fclose(fi); fclose(fl); return invalid;
                }

                const uint32 D = rows * cols;             // 784
                const nk_size total = (nk_size)nI * D;

                NkVector<uint8> pixels; pixels.Resize(total);
                if (fread(pixels.Data(), 1, total, fi) != total) { fclose(fi); fclose(fl); return invalid; }

                NkVector<uint8> lbytes; lbytes.Resize(nL);
                if (fread(lbytes.Data(), 1, nL, fl) != nL) { fclose(fi); fclose(fl); return invalid; }
                fclose(fi); fclose(fl);

                // Images -> tenseur [N, D] normalisé [0,1] ; labels -> int32.
                NkTensor X = NkTensor::Zeros(NkShape{ (int64)nI, (int64)D });
                float* xp = X.DataAs<float>();
                for (nk_size i = 0; i < total; ++i) xp[i] = (float)pixels[(uint32)i] / 255.0f;

                NkVector<int32> Y; Y.Reserve(nL);
                for (uint32 i = 0; i < nL; ++i) Y.PushBack((int32)lbytes[i]);

                return NkDataset(X, Y, 10u);
            }

        } // namespace data
    } // namespace ai
} // namespace nkentseu
