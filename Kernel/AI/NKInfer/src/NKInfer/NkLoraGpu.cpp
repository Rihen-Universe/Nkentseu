// =============================================================================
// NkLoraGpu.cpp — voir NkLoraGpu.h pour les formules, la justification des huit
// tampons et la spécification complète du format NKLA.
// =============================================================================
#include "NKInfer/NkLoraGpu.h"
#include "NKInfer/NkQKGpuBackward.h"

#include "NKTensor/NkTensorGpu.h"
#include "NKFileSystem/NkFile.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace ai {
		namespace infer {

			namespace {

				void SetErr(NkString *e, const char *msg) {
					if (e)
						*e = NkString(msg);
				}

				uint64 NewBuf(uint64 bytes, uint64 *acc) {
					const uint64 alloc = (bytes + 15ull) & ~15ull;
					uint64 id = NkTensorGpu::Get().CreateBuffer((nk_size)alloc);
					if (id != 0 && acc)
						*acc += alloc;
					return id;
				}

				// FNV-1a 64. Elle ne prétend pas être cryptographique : elle attrape
				// la troncature et la corruption d'octets, ce qui est exactement le
				// risque ici (disque plein, copie interrompue). Sans elle, un fichier
				// tronqué se recharge silencieusement et l'entraînement repart d'un
				// état faux — précisément les heures que la sauvegarde protège.
				constexpr uint64 kFnvOffset = 14695981039346656037ull;

				void FnvUpdate(uint64 &h, const void *data, uint64 n) {
					const uint8 *p = (const uint8 *)data;
					for (uint64 i = 0; i < n; ++i) {
						h ^= (uint64)p[i];
						h *= 1099511628211ull;
					}
				}

				// TOUT ce qui part sur le disque passe par ce Writer : l'empreinte ne
				// peut donc pas « oublier » un champ ajouté plus tard.
				struct Writer {
						NkFile *f = nullptr;
						uint64 hash = kFnvOffset;
						uint64 written = 0;
						bool ok = true;

						void Put(const void *data, uint64 n) {
							if (!ok || n == 0)
								return;
							if (f->Write(data, (usize)n) != (usize)n) {
								ok = false;
								return;
							}
							FnvUpdate(hash, data, n);
							written += n;
						}
						void PutU32(uint32 v) {
							Put(&v, 4);
						}
						void PutI32(int32 v) {
							Put(&v, 4);
						}
						void PutF32(float32 v) {
							Put(&v, 4);
						}
						void PutI64(int64 v) {
							Put(&v, 8);
						}
						void PutU64(uint64 v) {
							Put(&v, 8);
						}
				};

				struct Reader {
						NkFile *f = nullptr;
						uint64 hash = kFnvOffset;
						bool ok = true;

						void Get(void *data, uint64 n) {
							if (!ok || n == 0)
								return;
							if (f->Read(data, (usize)n) != (usize)n) {
								ok = false;
								return;
							}
							FnvUpdate(hash, data, n);
						}
						uint32 GetU32() {
							uint32 v = 0;
							Get(&v, 4);
							return v;
						}
						int32 GetI32() {
							int32 v = 0;
							Get(&v, 4);
							return v;
						}
						float32 GetF32() {
							float32 v = 0.0f;
							Get(&v, 4);
							return v;
						}
						int64 GetI64() {
							int64 v = 0;
							Get(&v, 8);
							return v;
						}
						uint64 GetU64() {
							uint64 v = 0;
							Get(&v, 8);
							return v;
						}
				};

			} // namespace

			NkLoraGpuPair *NkLoraGpuSet::At(int32 i) {
				switch (i) {
					case 0:
						return &q;
					case 1:
						return &k;
					case 2:
						return &v;
					case 3:
						return &o;
					case 4:
						return &gate;
					case 5:
						return &up;
					case 6:
						return &down;
					default:
						return nullptr;
				}
			}

			const NkLoraGpuPair *NkLoraGpuSet::At(int32 i) const {
				return const_cast<NkLoraGpuSet *>(this)->At(i);
			}

			const char *NkLoraGpuSet::Name(int32 i) {
				static const char *kNames[7] = {"q", "k", "v", "o", "gate", "up", "down"};
				return (i >= 0 && i < 7) ? kNames[i] : "?";
			}

			bool NkLoraGpuCreate(NkLoraGpuPair &p, int32 outFeatures, int32 inFeatures, int32 r, float32 alpha,
								 float32 sigma, NkLoraRng &rng, bool training, uint64 *outAllocatedBytes,
								 NkString *err) {
				NkLoraGpuRelease(p);
				if (outFeatures <= 0 || inFeatures <= 0 || r <= 0) {
					SetErr(err, "NkLoraGpuCreate : dimensions invalides");
					return false;
				}
				p.r = r;
				p.inF = inFeatures;
				p.outF = outFeatures;
				p.alpha = alpha;
				const uint64 nA = (uint64)r * (uint64)inFeatures;
				const uint64 nB = (uint64)outFeatures * (uint64)r;
				const uint64 F = sizeof(float32);

				p.A = NewBuf(nA * F, outAllocatedBytes);
				p.B = NewBuf(nB * F, outAllocatedBytes);
				if (p.A == 0 || p.B == 0) {
					SetErr(err, "NkLoraGpuCreate : CreateBuffer (A/B) a échoué");
					NkLoraGpuRelease(p);
					return false;
				}
				// A ~ N(0, sigma) tirée du MÊME RNG que le jalon 2 (xorshift64* +
				// Box-Muller) : à graine égale, un adaptateur CPU et un adaptateur
				// GPU partent des mêmes nombres, donc les deux chemins restent
				// comparables si on veut un jour les confronter.
				NkVector<float32> host;
				host.Resize((NkVector<float32>::SizeType)nA);
				for (uint64 i = 0; i < nA; ++i)
					host[(NkVector<float32>::SizeType)i] = rng.NextGaussian() * sigma;
				if (!NkTensorGpu::Get().Upload(p.A, host.Data(), (nk_size)(nA * F))) {
					SetErr(err, "NkLoraGpuCreate : Upload de A a échoué");
					NkLoraGpuRelease(p);
					return false;
				}
				if (!NkGpuZeroBuffer(p.B, nB * F, err)) {
					NkLoraGpuRelease(p);
					return false;
				}
				if (training) {
					uint64 *slots[6] = {&p.dA, &p.dB, &p.mA, &p.mB, &p.vA, &p.vB};
					const uint64 sizes[6] = {nA, nB, nA, nB, nA, nB};
					for (int i = 0; i < 6; ++i) {
						*slots[i] = NewBuf(sizes[i] * F, outAllocatedBytes);
						if (*slots[i] == 0 || !NkGpuZeroBuffer(*slots[i], sizes[i] * F, err)) {
							SetErr(err, "NkLoraGpuCreate : allocation des tampons d'entraînement a échoué");
							NkLoraGpuRelease(p);
							return false;
						}
					}
				}
				return true;
			}

			void NkLoraGpuRelease(NkLoraGpuPair &p) {
				NkTensorGpu &gpu = NkTensorGpu::Get();
				uint64 *slots[8] = {&p.A, &p.B, &p.dA, &p.dB, &p.mA, &p.mB, &p.vA, &p.vB};
				for (int i = 0; i < 8; ++i) {
					if (*slots[i] != 0) {
						gpu.DestroyBuffer(*slots[i]);
						*slots[i] = 0;
					}
				}
				p.r = 0;
				p.inF = 0;
				p.outF = 0;
				p.alpha = 0.0f;
			}

			bool NkLoraGpuForward(const NkLoraGpuPair &p, uint64 xBuf, uint64 yBuf, int64 T, uint64 uBuf,
								  NkString *err) {
				if (!p.IsValid() || xBuf == 0 || yBuf == 0 || uBuf == 0 || T <= 0) {
					SetErr(err, "NkLoraGpuForward : arguments invalides");
					return false;
				}
				// u = x·Aᵀ  ([T,in] × [r,in]ᵀ -> [T,r])
				if (!NkGpuMatmulABt(xBuf, p.A, uBuf, T, p.r, p.inF, 1.0f, false, err))
					return false;
				// y += scale · u·Bᵀ — l'échelle ET l'accumulation sont FUSÉES dans le
				// noyau : pas de tampon [T,out] intermédiaire, qui coûterait 9,7 Mo
				// par projection de FFN à T=128 et une passe mémoire de plus.
				return NkGpuMatmulABt(uBuf, p.B, yBuf, T, p.outF, p.r, p.Scale(), true, err);
			}

			bool NkLoraGpuBackward(const NkLoraGpuPair &p, uint64 xBuf, uint64 dyBuf, int64 T, uint64 uBuf,
								   uint64 duBuf, uint64 dxBuf, NkString *err) {
				if (!p.IsValid() || !p.HasGrads() || xBuf == 0 || dyBuf == 0 || uBuf == 0 || duBuf == 0 || T <= 0) {
					SetErr(err, "NkLoraGpuBackward : arguments invalides (paire sans gradients ?)");
					return false;
				}
				const float32 s = p.Scale();
				// u recalculé (cf NkLora.h : moins cher que de le garder vivant).
				if (!NkGpuMatmulABt(xBuf, p.A, uBuf, T, p.r, p.inF, 1.0f, false, err))
					return false;
				// dB += scale · dYᵀ·u   ([T,out]ᵀ × [T,r] -> [out,r])
				if (!NkGpuMatmulAtB(dyBuf, uBuf, p.dB, p.outF, p.r, T, s, true, err))
					return false;
				// du = scale · dY·B     ([T,out] × [out,r] -> [T,r])
				if (!NkGpuMatmulAB(dyBuf, p.B, duBuf, T, p.r, p.outF, s, false, err))
					return false;
				// dA += duᵀ·x           ([T,r]ᵀ × [T,in] -> [r,in])
				if (!NkGpuMatmulAtB(duBuf, xBuf, p.dA, p.r, p.inF, T, 1.0f, true, err))
					return false;
				// dX += du·A            ([T,r] × [r,in] -> [T,in])
				if (dxBuf != 0) {
					if (!NkGpuMatmulAB(duBuf, p.A, dxBuf, T, p.inF, p.r, 1.0f, true, err))
						return false;
				}
				return true;
			}

			bool NkLoraGpuZeroGrads(const NkLoraGpuPair &p, NkString *err) {
				if (!p.HasGrads()) {
					SetErr(err, "NkLoraGpuZeroGrads : paire sans gradients");
					return false;
				}
				const uint64 F = sizeof(float32);
				return NkGpuZeroBuffer(p.dA, (uint64)p.NumelA() * F, err) &&
					   NkGpuZeroBuffer(p.dB, (uint64)p.NumelB() * F, err);
			}

			bool NkLoraGpuAdamStep(const NkLoraGpuPair &p, float32 lr, float32 b1, float32 b2, float32 eps, float32 b1t,
								   float32 b2t, NkString *err) {
				if (!p.IsValid() || !p.HasGrads() || p.mA == 0 || p.vA == 0) {
					SetErr(err, "NkLoraGpuAdamStep : paire sans état Adam");
					return false;
				}
				// RunAdam de NKTensor : UN dispatch met à jour param/m/v en place.
				// wd = 0 -> Adam pur (pas AdamW) : les seuls paramètres sont les
				// adaptateurs, et une décroissance de poids sur B (initialisé à 0)
				// tirerait l'adaptateur vers l'inaction — l'inverse du but.
				NkTensorGpu &gpu = NkTensorGpu::Get();
				if (!gpu.RunAdam(p.A, p.dA, p.mA, p.vA, (uint32)p.NumelA(), lr, b1, b2, eps, b1t, b2t, 0.0f)) {
					SetErr(err, "NkLoraGpuAdamStep : RunAdam sur A a échoué");
					return false;
				}
				if (!gpu.RunAdam(p.B, p.dB, p.mB, p.vB, (uint32)p.NumelB(), lr, b1, b2, eps, b1t, b2t, 0.0f)) {
					SetErr(err, "NkLoraGpuAdamStep : RunAdam sur B a échoué");
					return false;
				}
				return true;
			}

			// =====================================================================
			// Persistance NKLA
			// =====================================================================
			bool NkLoraGpuSaveNKLA(const char *path, const NkVector<NkLoraGpuSet> &sets, int32 rank, float32 alpha,
								   int32 dModel, int32 ffnDim, int32 qDim, int32 kvDim, NkLoraGpuNklaInfo *outInfo,
								   NkString *err, const NkLoraTrainState *train) {
				if (!path || sets.Size() == 0) {
					SetErr(err, "NkLoraGpuSaveNKLA : chemin ou ensemble vide");
					return false;
				}
				// Un fichier de reprise n'a de sens que si les moments existent : sans
				// tampons d'entraînement, il n'y a rien à relire. On le REFUSE plutôt
				// que d'écrire des zéros qui feraient diverger la reprise.
				const uint32 flags = train ? (uint32)NK_NKLA_TRAIN_STATE : 0u;
				if (flags && (!sets[0].q.IsValid() || !sets[0].q.HasGrads())) {
					SetErr(err, "NkLoraGpuSaveNKLA : état de reprise demandé sur des adaptateurs sans tampons "
								"d'entraînement");
					return false;
				}
				NkFile f(path, NkFileMode::NK_WRITE_BINARY);
				if (!f.IsOpen()) {
					SetErr(err, "NkLoraGpuSaveNKLA : ouverture en écriture impossible");
					return false;
				}
				Writer w;
				w.f = &f;
				w.Put("NKLA", 4);
				w.PutU32(flags ? 2u : 1u);
				w.PutU32((uint32)sets.Size());
				w.PutU32((uint32)NkLoraGpuSet::kCount);
				w.PutU32((uint32)rank);
				w.PutF32(alpha);
				w.PutI32(dModel);
				w.PutI32(ffnDim);
				w.PutI32(qDim);
				w.PutI32(kvDim);
				w.PutU32(flags);
				w.PutU32(0);

				NkTensorGpu &gpu = NkTensorGpu::Get();
				NkVector<float32> host;
				uint64 params = 0;
				for (uint32 l = 0; l < sets.Size() && w.ok; ++l) {
					for (int32 i = 0; i < NkLoraGpuSet::kCount && w.ok; ++i) {
						const NkLoraGpuPair *p = sets[l].At(i);
						if (!p || !p->IsValid()) {
							w.PutU32(0);
							w.PutI32(0);
							w.PutI32(0);
							continue;
						}
						w.PutU32(1);
						w.PutI32(p->outF);
						w.PutI32(p->inF);
						// A, B puis — si reprise — les deux moments d'Adam. L'ORDRE est
						// celui du format : le lecteur le suit à l'identique, et les
						// tailles se déduisent des formes déjà écrites.
						const uint64 nA = (uint64)p->NumelA(), nB = (uint64)p->NumelB();
						const uint64 counts[6] = {nA, nB, nA, nB, nA, nB};
						const uint64 bufs[6] = {p->A, p->B, p->mA, p->mB, p->vA, p->vB};
						const int nBuf = flags ? 6 : 2;
						for (int s = 0; s < nBuf && w.ok; ++s) {
							host.Resize((NkVector<float32>::SizeType)counts[s]);
							if (!gpu.Download(bufs[s], host.Data(), (nk_size)(counts[s] * sizeof(float32)))) {
								SetErr(err, "NkLoraGpuSaveNKLA : relecture d'un adaptateur depuis le GPU a échoué");
								return false;
							}
							w.Put(host.Data(), counts[s] * sizeof(float32));
							// Seuls A et B sont des PARAMÈTRES : compter les moments
							// gonflerait un chiffre que l'utilisateur lit comme la
							// taille du modèle appris.
							if (s < 2)
								params += counts[s];
						}
					}
				}
				// Le bloc d'état vient APRÈS les poids : il est court, et le mettre en
				// queue permet de lire un fichier de reprise comme un fichier
				// d'adaptateurs en s'arrêtant simplement plus tôt.
				if (flags && train) {
					w.PutI64(train->step);
					w.PutI64(train->corpusPos);
					w.PutI64(train->epoch);
					w.PutU64(train->rngState);
					w.PutF32(train->lr);
					w.PutF32(train->beta1);
					w.PutF32(train->beta2);
					w.PutF32(train->eps);
				}
				if (!w.ok) {
					SetErr(err, "NkLoraGpuSaveNKLA : écriture interrompue (disque plein ?)");
					return false;
				}
				const uint64 h = w.hash;
				if (f.Write(&h, sizeof(h)) != sizeof(h)) {
					SetErr(err, "NkLoraGpuSaveNKLA : écriture de l'empreinte a échoué");
					return false;
				}
				f.Flush();
				if (outInfo) {
					outInfo->version = flags ? 2u : 1u;
					outInfo->flags = flags;
					outInfo->layerCount = sets.Size();
					outInfo->projCount = (uint32)NkLoraGpuSet::kCount;
					outInfo->rank = (uint32)rank;
					outInfo->alpha = alpha;
					outInfo->dModel = dModel;
					outInfo->ffnDim = ffnDim;
					outInfo->qDim = qDim;
					outInfo->kvDim = kvDim;
					outInfo->paramCount = params;
					outInfo->fileBytes = w.written + sizeof(h);
				}
				return true;
			}

			bool NkLoraGpuInspectNKLA(const char *path, NkLoraGpuNklaInfo &out, NkString *err) {
				NkFile f(path, NkFileMode::NK_READ_BINARY);
				if (!f.IsOpen()) {
					SetErr(err, "NkLoraGpuInspectNKLA : ouverture impossible");
					return false;
				}
				char magic[4] = {0, 0, 0, 0};
				Reader rd;
				rd.f = &f;
				rd.Get(magic, 4);
				if (!rd.ok || magic[0] != 'N' || magic[1] != 'K' || magic[2] != 'L' || magic[3] != 'A') {
					SetErr(err, "NkLoraGpuInspectNKLA : magie NKLA absente");
					return false;
				}
				out.version = rd.GetU32();
				out.layerCount = rd.GetU32();
				out.projCount = rd.GetU32();
				out.rank = rd.GetU32();
				out.alpha = rd.GetF32();
				out.dModel = rd.GetI32();
				out.ffnDim = rd.GetI32();
				out.qDim = rd.GetI32();
				out.kvDim = rd.GetI32();
				out.flags = rd.GetU32();
				rd.GetU32();
				if (!rd.ok) {
					SetErr(err, "NkLoraGpuInspectNKLA : en-tête tronqué");
					return false;
				}
				return true;
			}

			bool NkLoraGpuLoadNKLA(const char *path, NkVector<NkLoraGpuSet> &sets, NkLoraGpuNklaInfo *outInfo,
								   NkString *err, NkLoraTrainState *outTrain) {
				NkFile f(path, NkFileMode::NK_READ_BINARY);
				if (!f.IsOpen()) {
					SetErr(err, "NkLoraGpuLoadNKLA : ouverture impossible");
					return false;
				}
				Reader rd;
				rd.f = &f;
				char magic[4] = {0, 0, 0, 0};
				rd.Get(magic, 4);
				if (!rd.ok || magic[0] != 'N' || magic[1] != 'K' || magic[2] != 'L' || magic[3] != 'A') {
					SetErr(err, "NkLoraGpuLoadNKLA : magie NKLA absente");
					return false;
				}
				NkLoraGpuNklaInfo info;
				info.version = rd.GetU32();
				info.layerCount = rd.GetU32();
				info.projCount = rd.GetU32();
				info.rank = rd.GetU32();
				info.alpha = rd.GetF32();
				info.dModel = rd.GetI32();
				info.ffnDim = rd.GetI32();
				info.qDim = rd.GetI32();
				info.kvDim = rd.GetI32();
				info.flags = rd.GetU32();
				rd.GetU32();
				if (!rd.ok || (info.version != 1 && info.version != 2)) {
					SetErr(err, "NkLoraGpuLoadNKLA : version inconnue ou en-tête tronqué");
					return false;
				}
				// Un drapeau qu'on ne connaît pas veut dire que le fichier porte des
				// données dont on ignore la taille : on ne saurait plus où lire la
				// suite. REFUSER vaut mieux que lire de travers.
				if (info.flags & ~(uint32)NK_NKLA_TRAIN_STATE) {
					SetErr(err, "NkLoraGpuLoadNKLA : drapeaux inconnus (fichier écrit par une version plus récente)");
					return false;
				}
				// L'état de reprise exige des tampons d'entraînement : sans eux, il n'y
				// a nulle part où remettre les moments.
				const bool wantTrain = (outTrain != nullptr) && (info.flags & NK_NKLA_TRAIN_STATE) != 0u;
				const bool hasTrain = (info.flags & NK_NKLA_TRAIN_STATE) != 0u;
				if (wantTrain && (!sets[0].q.IsValid() || !sets[0].q.HasGrads())) {
					SetErr(err, "NkLoraGpuLoadNKLA : reprise demandée sur des adaptateurs sans tampons d'entraînement");
					return false;
				}
				if (info.layerCount != sets.Size() || info.projCount != (uint32)NkLoraGpuSet::kCount) {
					SetErr(err, "NkLoraGpuLoadNKLA : le fichier ne décrit pas ce modèle (couches/projections)");
					return false;
				}

				NkTensorGpu &gpu = NkTensorGpu::Get();
				NkVector<float32> host;
				uint64 params = 0;
				for (uint32 l = 0; l < sets.Size(); ++l) {
					for (int32 i = 0; i < NkLoraGpuSet::kCount; ++i) {
						const uint32 present = rd.GetU32();
						const int32 outF = rd.GetI32();
						const int32 inF = rd.GetI32();
						if (!rd.ok) {
							SetErr(err, "NkLoraGpuLoadNKLA : fichier tronqué (descripteur de projection)");
							return false;
						}
						NkLoraGpuPair *p = sets[l].At(i);
						if (present == 0) {
							if (p && p->IsValid()) {
								SetErr(err,
									   "NkLoraGpuLoadNKLA : le modèle attend un adaptateur que le fichier n'a pas");
								return false;
							}
							continue;
						}
						if (!p || !p->IsValid() || p->outF != outF || p->inF != inF || p->r != (int32)info.rank) {
							SetErr(err, "NkLoraGpuLoadNKLA : formes d'adaptateur incompatibles avec le modèle courant");
							return false;
						}
						const uint64 nA = (uint64)p->NumelA(), nB = (uint64)p->NumelB();
						const uint64 counts[6] = {nA, nB, nA, nB, nA, nB};
						const uint64 bufs[6] = {p->A, p->B, p->mA, p->mB, p->vA, p->vB};
						// Les moments présents dans le fichier sont TOUJOURS lus : le
						// flux est séquentiel, les sauter désalignerait tout ce qui
						// suit. En revanche ils ne sont téléversés que si la reprise
						// est demandée — charger un fichier de reprise pour une simple
						// inférence reste donc légitime et sans effet de bord.
						const int nBuf = hasTrain ? 6 : 2;
						for (int s = 0; s < nBuf; ++s) {
							host.Resize((NkVector<float32>::SizeType)counts[s]);
							rd.Get(host.Data(), counts[s] * sizeof(float32));
							if (!rd.ok) {
								SetErr(err, "NkLoraGpuLoadNKLA : fichier tronqué (données d'adaptateur)");
								return false;
							}
							if ((s < 2 || wantTrain)
								&& !gpu.Upload(bufs[s], host.Data(), (nk_size)(counts[s] * sizeof(float32)))) {
								SetErr(err, "NkLoraGpuLoadNKLA : Upload vers le GPU a échoué");
								return false;
							}
							if (s < 2)
								params += counts[s];
						}
						p->alpha = info.alpha;
					}
				}
				// Bloc d'état : lu dès qu'il est présent (il entre dans l'empreinte),
				// recopié seulement si l'appelant l'a demandé.
				if (hasTrain) {
					NkLoraTrainState ts;
					ts.step = rd.GetI64();
					ts.corpusPos = rd.GetI64();
					ts.epoch = rd.GetI64();
					ts.rngState = rd.GetU64();
					ts.lr = rd.GetF32();
					ts.beta1 = rd.GetF32();
					ts.beta2 = rd.GetF32();
					ts.eps = rd.GetF32();
					if (!rd.ok) {
						SetErr(err, "NkLoraGpuLoadNKLA : fichier tronqué (état de reprise)");
						return false;
					}
					if (outTrain)
						*outTrain = ts;
				}
				uint64 stored = 0;
				if (f.Read(&stored, sizeof(stored)) != sizeof(stored)) {
					SetErr(err, "NkLoraGpuLoadNKLA : empreinte absente (fichier tronqué)");
					return false;
				}
				if (stored != rd.hash) {
					// REFUS, pas avertissement : un adaptateur corrompu appliqué au
					// socle donne un modèle silencieusement faux — exactement ce que
					// la sauvegarde était censée empêcher.
					SetErr(err, "NkLoraGpuLoadNKLA : empreinte FNV incorrecte — fichier corrompu");
					return false;
				}
				info.paramCount = params;
				if (outInfo)
					*outInfo = info;
				return true;
			}

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
