// =============================================================================
// NKTrain/NkCheckpoint.cpp — checkpoint générique modèle+optimiseur (NKAI).
// Même pattern d'E/S que NKInfer (NKMD) et NKGpt (NKGP) : FILE* C, tenseur =
// rang+dims+floats. Ici GÉNÉRALISÉ (aucune dépendance à un modèle précis).
// =============================================================================
#include "NKTrain/NkCheckpoint.h"
#include "NKLogger/NkLog.h"

#include <cstdio>

namespace nkentseu {
	namespace ai {
		namespace train {

			static const char kMagic[4] = {'N', 'K', 'T', 'C'};
			static const uint32 kVersion = 1u;

			static int64 ShapeNumel(const NkShape &sh) {
				int64 n = 1;
				for (uint32 i = 0; i < sh.Size(); ++i)
					n *= sh[i];
				return n;
			}

			// Écrit un tenseur (rang + dims + floats) après passage sur CPU contigu.
			static bool WriteTensor(FILE *f, const NkTensor &src) {
				NkTensor v = src.ToCPU().Contiguous();
				const NkShape &sh = v.Shape();
				uint32 rank = sh.Size();
				bool ok = fwrite(&rank, sizeof(uint32), 1, f) == 1;
				for (uint32 d = 0; ok && d < rank; ++d) {
					int64 dim = sh[d];
					ok = fwrite(&dim, sizeof(int64), 1, f) == 1;
				}
				int64 numel = ShapeNumel(sh);
				const float *p = v.DataAs<float>();
				ok = ok && (numel == 0 || fwrite(p, sizeof(float), (size_t)numel, f) == (size_t)numel);
				return ok;
			}

			// Lit un tenseur (rang + dims + floats) dans un NkTensor CPU neuf.
			static bool ReadTensor(FILE *f, NkTensor &out) {
				uint32 rank = 0;
				if (fread(&rank, sizeof(uint32), 1, f) != 1 || rank > 8)
					return false;
				NkShape shape;
				for (uint32 d = 0; d < rank; ++d) {
					int64 dim = 0;
					if (fread(&dim, sizeof(int64), 1, f) != 1)
						return false;
					shape.PushBack(dim);
				}
				int64 numel = ShapeNumel(shape);
				out = NkTensor::Zeros(shape);
				float *p = out.DataAs<float>();
				return numel == 0 || fread(p, sizeof(float), (size_t)numel, f) == (size_t)numel;
			}

			// Saute un tenseur sans le charger (rang + dims + floats).
			static bool SkipTensor(FILE *f) {
				uint32 rank = 0;
				if (fread(&rank, sizeof(uint32), 1, f) != 1 || rank > 8)
					return false;
				int64 numel = 1;
				for (uint32 d = 0; d < rank; ++d) {
					int64 dim = 0;
					if (fread(&dim, sizeof(int64), 1, f) != 1)
						return false;
					numel *= dim;
				}
				return fseek(f, (long)(numel * (int64)sizeof(float)), SEEK_CUR) == 0;
			}

			// Lit l'en-tête (magic+version+paramCount), positionne le curseur juste avant
			// le premier tenseur de poids. Renvoie -1 en cas d'échec, sinon paramCount.
			static int64 ReadHeader(FILE *f) {
				char magic[4];
				uint32 ver = 0, count = 0;
				if (fread(magic, 1, 4, f) != 4 || magic[0] != kMagic[0] || magic[1] != kMagic[1] ||
					magic[2] != kMagic[2] || magic[3] != kMagic[3])
					return -1;
				if (fread(&ver, sizeof(uint32), 1, f) != 1 || ver != kVersion)
					return -1;
				if (fread(&count, sizeof(uint32), 1, f) != 1)
					return -1;
				return (int64)count;
			}

			// Saute les `count` tenseurs de poids puis lit le flag hasOpt. Positionne le
			// curseur juste après ce flag (début du bloc opt si hasOpt=1). -1 si erreur,
			// sinon 0/1.
			static int32 SkipWeightsReadOptFlag(FILE *f, int64 count) {
				for (int64 i = 0; i < count; ++i)
					if (!SkipTensor(f))
						return -1;
				uint8 hasOpt = 0;
				if (fread(&hasOpt, 1, 1, f) != 1)
					return -1;
				return (int32)hasOpt;
			}

			bool SaveCheckpoint(const char *path, const NkVector<NkVar> &params, const optim::NkAdam *opt,
								const NkTrainState *state) {
				FILE *f = fopen(path, "wb");
				if (!f)
					return false;
				bool ok = fwrite(kMagic, 1, 4, f) == 4;
				uint32 ver = kVersion;
				ok = ok && fwrite(&ver, sizeof(uint32), 1, f) == 1;
				uint32 count = params.Size();
				ok = ok && fwrite(&count, sizeof(uint32), 1, f) == 1;
				for (uint32 i = 0; ok && i < params.Size(); ++i)
					ok = ok && WriteTensor(f, params[i].Value());

				const bool hasOpt = (opt != nullptr) && opt->FirstMoments().Size() == params.Size() &&
									 opt->SecondMoments().Size() == params.Size();
				uint8 optFlag = hasOpt ? 1u : 0u;
				ok = ok && fwrite(&optFlag, 1, 1, f) == 1;
				if (ok && hasOpt) {
					int64 step = opt->StepCount();
					ok = fwrite(&step, sizeof(int64), 1, f) == 1;
					const NkVector<NkTensor> &m = opt->FirstMoments();
					const NkVector<NkTensor> &v = opt->SecondMoments();
					for (uint32 i = 0; ok && i < m.Size(); ++i)
						ok = ok && WriteTensor(f, m[i]);
					for (uint32 i = 0; ok && i < v.Size(); ++i)
						ok = ok && WriteTensor(f, v[i]);
				}

				const bool hasState = state != nullptr;
				uint8 stateFlag = hasState ? 1u : 0u;
				ok = ok && fwrite(&stateFlag, 1, 1, f) == 1;
				if (ok && hasState) {
					ok = fwrite(&state->epoch, sizeof(int64), 1, f) == 1;
					ok = ok && fwrite(&state->globalStep, sizeof(int64), 1, f) == 1;
					ok = ok && fwrite(&state->bestMetric, sizeof(double), 1, f) == 1;
					ok = ok && fwrite(&state->badEpochs, sizeof(int32), 1, f) == 1;
				}
				fclose(f);
				if (!ok)
					logger.Info("SaveCheckpoint : échec d'écriture ({0})", path);
				return ok;
			}

			bool LoadCheckpointWeights(const char *path, NkVector<NkVar> &params) {
				FILE *f = fopen(path, "rb");
				if (!f)
					return false;
				int64 count = ReadHeader(f);
				bool ok = count >= 0 && (uint32)count == params.Size();
				for (uint32 i = 0; ok && i < params.Size(); ++i) {
					NkTensor t;
					ok = ReadTensor(f, t);
					if (!ok)
						break;
					const NkShape &cur = params[i].Value().Shape();
					const NkShape &got = t.Shape();
					if (got.Size() != cur.Size()) {
						ok = false;
						break;
					}
					for (uint32 d = 0; d < got.Size(); ++d)
						if (got[d] != cur[d]) {
							ok = false;
							break;
						}
					if (ok)
						params[i].SetValue(t);
				}
				fclose(f);
				return ok;
			}

			bool LoadCheckpointOptState(const char *path, const NkVector<NkVar> &params, optim::NkAdam &opt) {
				FILE *f = fopen(path, "rb");
				if (!f)
					return false;
				int64 count = ReadHeader(f);
				if (count < 0 || (uint32)count != params.Size()) {
					fclose(f);
					return false;
				}
				int32 hasOpt = SkipWeightsReadOptFlag(f, count);
				if (hasOpt != 1) {
					fclose(f);
					return false;
				}
				int64 step = 0;
				bool ok = fread(&step, sizeof(int64), 1, f) == 1;
				NkVector<NkTensor> m, v;
				for (int64 i = 0; ok && i < count; ++i) {
					NkTensor t;
					ok = ReadTensor(f, t);
					if (ok)
						m.PushBack(t);
				}
				for (int64 i = 0; ok && i < count; ++i) {
					NkTensor t;
					ok = ReadTensor(f, t);
					if (ok)
						v.PushBack(t);
				}
				fclose(f);
				ok = ok && m.Size() == (uint32)count && v.Size() == (uint32)count;
				if (ok) {
					opt.SetMoments(m, v);
					opt.SetStepCount(step);
				}
				return ok;
			}

			bool LoadCheckpointTrainState(const char *path, NkTrainState &state) {
				FILE *f = fopen(path, "rb");
				if (!f)
					return false;
				int64 count = ReadHeader(f);
				if (count < 0) {
					fclose(f);
					return false;
				}
				int32 hasOpt = SkipWeightsReadOptFlag(f, count);
				if (hasOpt < 0) {
					fclose(f);
					return false;
				}
				if (hasOpt == 1) {
					int64 step = 0;
					bool ok = fread(&step, sizeof(int64), 1, f) == 1;
					for (int64 i = 0; ok && i < 2 * count; ++i) // saute m puis v
						ok = SkipTensor(f);
					if (!ok) {
						fclose(f);
						return false;
					}
				}
				uint8 hasState = 0;
				bool ok = fread(&hasState, 1, 1, f) == 1;
				if (!ok || hasState == 0) {
					fclose(f);
					return false;
				}
				ok = fread(&state.epoch, sizeof(int64), 1, f) == 1;
				ok = ok && fread(&state.globalStep, sizeof(int64), 1, f) == 1;
				ok = ok && fread(&state.bestMetric, sizeof(double), 1, f) == 1;
				ok = ok && fread(&state.badEpochs, sizeof(int32), 1, f) == 1;
				fclose(f);
				return ok;
			}

			bool CheckpointHasOptState(const char *path) {
				FILE *f = fopen(path, "rb");
				if (!f)
					return false;
				int64 count = ReadHeader(f);
				if (count < 0) {
					fclose(f);
					return false;
				}
				int32 hasOpt = SkipWeightsReadOptFlag(f, count);
				fclose(f);
				return hasOpt == 1;
			}

		} // namespace train
	} // namespace ai
} // namespace nkentseu
