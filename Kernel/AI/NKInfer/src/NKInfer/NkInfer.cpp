// =============================================================================
// NkInfer.cpp — inférence + persistance des poids (NKAI, Phase 3).
// =============================================================================
#include "NKInfer/NkInfer.h"
#include "NKTensor/NkTensorOps.h"

#include <cstdio>

namespace nkentseu {
    namespace ai {
        namespace infer {

            static const uint8  kMagic[4] = { 'N', 'K', 'M', 'D' };
            static const uint32 kVersion  = 1u;

            bool SaveParams(const char* path, const NkVector<NkVar>& params) {
                FILE* f = fopen(path, "wb");
                if (!f) return false;
                bool ok = fwrite(kMagic, 1, 4, f) == 4;
                uint32 version = kVersion, count = params.Size();
                ok = ok && fwrite(&version, sizeof(uint32), 1, f) == 1;
                ok = ok && fwrite(&count,   sizeof(uint32), 1, f) == 1;
                for (uint32 i = 0; ok && i < params.Size(); ++i) {
                    NkTensor v = params[i].Value().Contiguous();
                    const NkShape& sh = v.Shape();
                    uint32 rank = sh.Size();
                    ok = ok && fwrite(&rank, sizeof(uint32), 1, f) == 1;
                    for (uint32 d = 0; ok && d < rank; ++d) {
                        int64 dim = sh[d];
                        ok = ok && fwrite(&dim, sizeof(int64), 1, f) == 1;
                    }
                    int64 numel = NkShapeNumel(sh);
                    const float* p = v.DataAs<float>();
                    ok = ok && (numel == 0 ||
                                fwrite(p, sizeof(float), (size_t)numel, f) == (size_t)numel);
                }
                fclose(f);
                return ok;
            }

            bool LoadParams(const char* path, NkVector<NkVar>& params) {
                FILE* f = fopen(path, "rb");
                if (!f) return false;
                uint8 magic[4];
                uint32 version = 0, count = 0;
                bool ok = fread(magic, 1, 4, f) == 4 &&
                          magic[0] == kMagic[0] && magic[1] == kMagic[1] &&
                          magic[2] == kMagic[2] && magic[3] == kMagic[3] &&
                          fread(&version, sizeof(uint32), 1, f) == 1 &&
                          fread(&count,   sizeof(uint32), 1, f) == 1 &&
                          version == kVersion && count == params.Size();
                for (uint32 i = 0; ok && i < params.Size(); ++i) {
                    uint32 rank = 0;
                    ok = ok && fread(&rank, sizeof(uint32), 1, f) == 1;
                    NkShape shape;
                    for (uint32 d = 0; ok && d < rank; ++d) {
                        int64 dim = 0;
                        ok = ok && fread(&dim, sizeof(int64), 1, f) == 1;
                        shape.PushBack(dim);
                    }
                    // La forme lue doit correspondre au paramètre cible.
                    const NkShape& cur = params[i].Value().Shape();
                    if (!ok || shape.Size() != cur.Size()) { ok = false; break; }
                    for (uint32 d = 0; d < shape.Size(); ++d)
                        if (shape[d] != cur[d]) { ok = false; break; }
                    if (!ok) break;

                    int64 numel = NkShapeNumel(shape);
                    NkTensor t = NkTensor::Zeros(shape);
                    float* p = t.DataAs<float>();
                    ok = ok && (numel == 0 ||
                                fread(p, sizeof(float), (size_t)numel, f) == (size_t)numel);
                    if (ok) params[i].SetValue(t);
                }
                fclose(f);
                return ok;
            }

            NkVector<int32> Predict(const NkTensor& logits) {
                NkVector<int32> out;
                NkTensor lc = logits.Contiguous();
                const NkShape& sh = lc.Shape();
                const uint32 B = sh.Size() >= 1 ? (uint32)sh[0] : 0;
                const uint32 C = sh.Size() >= 2 ? (uint32)sh[1] : 0;
                if (B == 0 || C == 0) return out;
                const float* lp = lc.DataAs<float>();
                out.Reserve(B);
                for (uint32 i = 0; i < B; ++i) {
                    const float* row = lp + (nk_size)i * C;
                    uint32 best = 0; float bv = row[0];
                    for (uint32 c = 1; c < C; ++c) if (row[c] > bv) { bv = row[c]; best = c; }
                    out.PushBack((int32)best);
                }
                return out;
            }

            int32 PredictOne(const NkTensor& logits) {
                NkTensor lc = logits.Contiguous();
                const int64 n = NkShapeNumel(lc.Shape());
                if (n <= 0) return -1;
                const float* p = lc.DataAs<float>();
                int32 best = 0; float bv = p[0];
                for (int64 c = 1; c < n; ++c) if (p[c] > bv) { bv = p[c]; best = (int32)c; }
                return best;
            }

        } // namespace infer
    } // namespace ai
} // namespace nkentseu
