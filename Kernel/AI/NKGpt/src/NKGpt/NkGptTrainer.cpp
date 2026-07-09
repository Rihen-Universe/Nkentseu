// =============================================================================
// NKGpt/NkGptTrainer.cpp — implémentation de l'entraîneur GPT réutilisable
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKGpt/NkGptTrainer.h"
#include "NKOptim/NkOptim.h"
#include "NKTensor/NkTensorGpu.h"
#include "NKTime/NkChrono.h"
#include "NKMath/NkFunctions.h" // NkExp, NkCos (au lieu de <math.h>)
#include "NKLogger/NkLog.h"		// logger (macro) : status + texte généré (console + fichiers)

using namespace nkentseu;
using namespace nkentseu::ai;	// nn::, optim::, autograd::
using namespace nkentseu::math; // NkExp, NkCos

namespace nkentseu {
	namespace ai {
		namespace gpt {

			NkGptTrainer::NkGptTrainer(const NkGptConfig &cfg) : mCfg(cfg) {
			}

			NkGptTrainer::~NkGptTrainer() {
				delete mGpt;
			}

			double NkGptTrainer::NextRand() {
				mRng = mRng * 6364136223846793005ull + 1442695040888963407ull;
				return (double)((mRng >> 11) & 0xFFFFFFFFFFFFFull) / (double)(1ull << 52);
			}

			// texts (par langue) -> mLangData/mLangMask, avec le BPE courant. Masque la question des
			// blocs "Question:/Réponse:" (instruction-tuning : seule la réponse compte dans la loss).
			nk_size NkGptTrainer::EncodeCorpus(const NkVector<NkString> &texts) {
				const NkStringView marker("Réponse: ");
				mLangData.Resize((nk_size)texts.Size());
				mLangMask.Resize((nk_size)texts.Size());
				nk_size totalTok = 0;
				for (int64 li = 0; li < (int64)texts.Size(); ++li) {
					const NkString &txt = texts[(nk_size)li];
					const bool isQa = txt.Find(marker) != NkString::npos;
					if (!isQa) {
						NkVector<int32> ids;
						mBpe.Encode(txt, ids);
						for (int64 k = 0; k < (int64)ids.Size(); ++k) {
							mLangData[(nk_size)li].PushBack((float)ids[(nk_size)k]);
							mLangMask[(nk_size)li].PushBack(1.f);
						}
					} else {
						const nk_size sz = txt.Size();
						nk_size pos = 0;
						while (pos < sz) {
							const nk_size be = txt.Find("\n\n", pos);
							const nk_size blen = (be == NkString::npos) ? (sz - pos) : (be - pos);
							NkString block = txt.SubStr(pos, blen);
							pos = (be == NkString::npos) ? sz : be + 2;
							if (block.Size() == 0)
								continue;
							const nk_size mp = block.Find(marker);
							NkString qPart = (mp == NkString::npos) ? block : block.SubStr(0, mp + marker.Size());
							NkVector<int32> qIds;
							mBpe.Encode(qPart, qIds);
							for (int64 k = 0; k < (int64)qIds.Size(); ++k) {
								mLangData[(nk_size)li].PushBack((float)qIds[(nk_size)k]);
								mLangMask[(nk_size)li].PushBack(0.f);
							}
							if (mp != NkString::npos) {
								NkString aPart = block.SubStr(mp + marker.Size());
								if (aPart.Size() > 0) {
									NkVector<int32> aIds;
									mBpe.Encode(aPart, aIds);
									for (int64 k = 0; k < (int64)aIds.Size(); ++k) {
										mLangData[(nk_size)li].PushBack((float)aIds[(nk_size)k]);
										mLangMask[(nk_size)li].PushBack(1.f);
									}
								}
							}
							NkVector<int32> sepIds;
							mBpe.Encode(NkString("\n\n"), sepIds);
							for (int64 k = 0; k < (int64)sepIds.Size(); ++k) {
								mLangData[(nk_size)li].PushBack((float)sepIds[(nk_size)k]);
								mLangMask[(nk_size)li].PushBack(0.f);
							}
						}
					}
					totalTok += mLangData[(nk_size)li].Size();
				}
				return totalTok;
			}

			// Lot : x[B,T], cible one-hot [B*T, V]. Séquence préfixée du tag de langue ; round-robin.
			void NkGptTrainer::MakeBatch(NkTensor &x, NkTensor &oneHot) {
				NkShape xs;
				xs.PushBack(mB);
				xs.PushBack(mT);
				x = NkTensor::Zeros(xs);
				oneHot = NkTensor::Zeros(NkShape{mB * mT, (int64)mV});
				float *xp = x.DataAs<float>();
				float *op = oneHot.DataAs<float>();
				const int nL = (int)mLangData.Size();
				for (int64 b = 0; b < mB; ++b) {
					const int li = nL > 0 ? (int)(b % nL) : 0;
					const NkVector<float> &dd = mLangData[(nk_size)li];
					const bool hasMask =
						((nk_size)li < mLangMask.Size()) && (mLangMask[(nk_size)li].Size() == dd.Size());
					const int64 N = (int64)dd.Size();
					if (N <= mT)
						continue;
					const int64 off = (int64)(NextRand() * (double)(N - mT));
					xp[b * mT + 0] = (float)(mNByte + li);
					if (!hasMask || mLangMask[(nk_size)li][(nk_size)off] != 0.f)
						op[(b * mT + 0) * mV + (int)dd[(nk_size)off]] = 1.f;
					for (int64 t = 1; t < mT; ++t) {
						xp[b * mT + t] = dd[(nk_size)(off + t - 1)];
						if (!hasMask || mLangMask[(nk_size)li][(nk_size)(off + t)] != 0.f)
							op[(b * mT + t) * mV + (int)dd[(nk_size)(off + t)]] = 1.f;
					}
				}
			}

			bool NkGptTrainer::Prepare() {
				mUseGpu = NkTensorGpu::Get().IsAvailable();
				const bool V = mCfg.verbose;
				mB = mCfg.B;

				if (!mCfg.loadPath.Empty()) {
					GptMeta meta;
					if (!LoadCheckpointMeta(mCfg.loadPath.CStr(), meta)) {
						logger.Info("Checkpoint illisible ou format obsolète (attendu BPE v3) : {0}",
									mCfg.loadPath.CStr());
						return false;
					}
					mV = meta.V;
					mD = meta.d;
					mH = meta.H;
					mL = meta.L;
					mT = meta.T;
					mLangs = meta.langs;
					for (int64 i = 0; i < (int64)meta.merges.Size(); ++i)
						mBpe.merges.PushBack(meta.merges[(nk_size)i]);
					mBpe.BuildVocabRank();
					mNByte = mBpe.Base();
					if (V)
						logger.Info(
							"Modèle chargé : {0} (V={1}, T={2}, d={3}, têtes={4}, couches={5}, {6} fusions BPE)",
							mCfg.loadPath.CStr(), mV, (long long)mT, (long long)mD, (long long)mH, (long long)mL,
							(unsigned long long)mBpe.merges.Size());
				} else {
					NkVector<NkString> texts;
					if (!mCfg.corpusFile.Empty()) {
						if (V)
							logger.Info("Corpus : fichier unique {0}", mCfg.corpusFile.CStr());
						mLangs.PushBack(LangOf(mCfg.corpusFile));
						texts.PushBack(LoadCorpus(mCfg.corpusFile.CStr(), mCfg.maxChars));
					} else {
						if (V)
							logger.Info("Corpus : dossier {0} (équilibré par langue, cap total {1})",
										mCfg.corpusDir.CStr(), (unsigned long long)mCfg.maxChars);
						LoadCorpusByLang(mCfg.corpusDir, mCfg.maxChars, mLangs, texts);
					}
					nk_size totalChars = 0;
					for (int64 i = 0; i < (int64)texts.Size(); ++i)
						totalChars += texts[(nk_size)i].Size();
					if (totalChars < 1000) {
						logger.Info("Corpus introuvable/trop court.");
						return false;
					}
					if (V)
						logger.Info("Entraînement du tokenizer BPE ({0} fusions cible)...", mCfg.merges);
					TrainBpe(texts, mCfg.merges, mBpe);
					mNByte = mBpe.Base();
					mV = mNByte + (int)mLangs.Size();
					const nk_size totalTok = EncodeCorpus(texts);
					if (V)
						logger.Info("Corpus : {0} car. -> {1} tokens BPE ; {2} tokens (256 + {3} fusions) + {4} tags = "
									"vocab {5}.",
									(unsigned long long)totalChars, (unsigned long long)totalTok, mNByte,
									(unsigned long long)mBpe.merges.Size(), (int)mLangs.Size(), mV);
					mT = mCfg.T;
					mD = mCfg.d;
					mH = mCfg.H;
					mL = mCfg.L;
					if (V)
						logger.Info(
							"Modèle GPT : T={0}, d={1}, têtes={2}, couches={3}, batch={4}  (AdamW, GPU-résident)",
							(long long)mT, (long long)mD, (long long)mH, (long long)mL, (long long)mB);
				}

				// Reprise : ré-encoder le corpus avec le BPE du checkpoint (mêmes tags requis).
				if (mCfg.resume) {
					NkVector<NkString> texts;
					NkVector<NkString> langs2;
					if (!mCfg.corpusFile.Empty()) {
						langs2.PushBack(LangOf(mCfg.corpusFile));
						texts.PushBack(LoadCorpus(mCfg.corpusFile.CStr(), mCfg.maxChars));
					} else {
						LoadCorpusByLang(mCfg.corpusDir, mCfg.maxChars, langs2, texts);
					}
					bool langsOk = (langs2.Size() == mLangs.Size());
					for (int64 i = 0; langsOk && i < (int64)mLangs.Size(); ++i)
						if (!(langs2[(nk_size)i] == mLangs[(nk_size)i]))
							langsOk = false;
					if (!langsOk) {
						logger.Info("Reprise IMPOSSIBLE : les tags du corpus ne correspondent pas au checkpoint.");
						return false;
					}
					const nk_size totalTok = EncodeCorpus(texts);
					if (V)
						logger.Info("Reprise : corpus ré-encodé avec le BPE du checkpoint ({0} tokens BPE, {1} tags).",
									(unsigned long long)totalTok, (int)mLangs.Size());
				}

				// Langue de génération demandée.
				mGenLang = -1;
				if (!mCfg.genLang.Empty())
					for (int64 i = 0; i < (int64)mLangs.Size(); ++i)
						if (mLangs[(nk_size)i] == mCfg.genLang) {
							mGenLang = (int)i;
							break;
						}

				// Construction du modèle + (chargement des poids | init aléatoire).
				mGpt = new nn::NkGPT((uint32)mV, (uint32)mD, (uint32)mH, (uint32)mL, (uint32)mT, 1234u);
				mGpt->Parameters(mParams);
				if (!mCfg.loadPath.Empty()) {
					if (!LoadCheckpointWeights(mCfg.loadPath.CStr(), mParams)) {
						logger.Info("Poids du checkpoint incompatibles avec les dims.");
						return false;
					}
					if (V)
						logger.Info("Poids rechargés ({0} tenseurs).", mParams.Size());
				}
				if (mUseGpu)
					for (uint32 i = 0; i < mParams.Size(); ++i)
						mParams[i].SetValue(mParams[i].Value().ToGPU());
				return true;
			}

			NkString NkGptTrainer::Generate(const NkString &sd, int nToks, double temp, int langIdx) {
				NkVector<int32> ctx;
				if (langIdx >= 0 && langIdx < (int)mLangs.Size())
					ctx.PushBack((int32)(mNByte + langIdx));
				NkVector<int32> seedIds;
				mBpe.Encode(sd, seedIds);
				for (int64 i = 0; i < (int64)seedIds.Size(); ++i)
					ctx.PushBack(seedIds[(nk_size)i]);
				if (ctx.Size() == 0)
					ctx.PushBack(0);
				NkString out = sd;
				NkVector<float> logitBuf;
				logitBuf.Resize((nk_size)mV);
				for (int i = 0; i < nToks; ++i) {
					int64 len = (int64)ctx.Size();
					if (len > mT)
						len = mT;
					NkTensor tok = NkTensor::Zeros(NkShape{(int64)1, len});
					float *tp = tok.DataAs<float>();
					for (int64 t = 0; t < len; ++t)
						tp[t] = (float)ctx[(nk_size)((int64)ctx.Size() - len + t)];
					NkVar logits = mGpt->Forward(mUseGpu ? tok.ToGPU() : tok);
					NkTensor lc = logits.Value().ToCPU().Contiguous();
					const float *lp = lc.DataAs<float>() + (len - 1) * mV;
					double mx = -1e30;
					for (int v = 0; v < mNByte; ++v)
						if (lp[v] > mx)
							mx = lp[v];
					double sum = 0;
					for (int v = 0; v < mV; ++v) {
						if (v >= mNByte) {
							logitBuf[(nk_size)v] = 0.f;
							continue;
						}
						const double e = NkExp((lp[v] - mx) / temp);
						logitBuf[(nk_size)v] = (float)e;
						sum += e;
					}
					double r = NextRand() * sum, acc = 0;
					int next = 0;
					for (int v = 0; v < mNByte; ++v) {
						acc += logitBuf[(nk_size)v];
						if (acc >= r) {
							next = v;
							break;
						}
					}
					ctx.PushBack((int32)next);
					out.Append(mBpe.Decode(next));
				}
				return out;
			}

			void NkGptTrainer::GenerateFinal() {
				logger.Info("=== TEXTE GÉNÉRÉ (amorce « {0} », {1} tokens, temp 0.8) ===", mCfg.seed.CStr(),
							mCfg.genLen);
				if (mLangs.Size() <= 1)
					logger.Info("{0}", Generate(mCfg.seed, mCfg.genLen, 0.8, mLangs.Size() == 0 ? -1 : 0).CStr());
				else
					for (int li = 0; li < (int)mLangs.Size(); ++li)
						logger.Info("[{0}] {1}", mLangs[(nk_size)li].CStr(),
									Generate(mCfg.seed, mCfg.genLen, 0.8, li).CStr());
				logger.Info("=========================================================");
			}

			bool NkGptTrainer::Save(const char *path) {
				GptMeta meta;
				meta.V = mV;
				meta.d = (int32)mD;
				meta.H = (int32)mH;
				meta.L = (int32)mL;
				meta.T = (int32)mT;
				meta.langs = mLangs;
				for (int64 i = 0; i < (int64)mBpe.merges.Size(); ++i)
					meta.merges.PushBack(mBpe.merges[(nk_size)i]);
				return SaveCheckpoint(path, meta, mParams);
			}

			void NkGptTrainer::Fit() {
				if (!mGpt)
					return;
				const bool V = mCfg.verbose;
				const int STEPS = mCfg.steps;
				const int ACCUM = mCfg.accum;
				const float peakLr = mCfg.lr;
				const int WARMUP = (mCfg.warmup >= 0) ? mCfg.warmup : (STEPS / 20);
				const double kPi = 3.14159265358979323846;
				const float minLrRatio = 0.1f;
				const int SAVEEVERY = mCfg.saveEvery;
				const bool hasSave = !mCfg.savePath.Empty();

				optim::NkAdam adam(mParams, peakLr, 0.9f, 0.999f, 1e-8f, /*weightDecay=AdamW*/ 0.01f);

				if (V) {
					logger.Info("-- Entraînement ({0} pas) --", STEPS);
					if (ACCUM > 1)
						logger.Info("   Accumulation de gradient : {0} micro-lots -> batch effectif = {1}", ACCUM,
									(long long)(mB * ACCUM));
					logger.Info("   LR schedule : warmup {0} pas -> pic {1} -> cosine (plancher {2}%) ; checkpoint "
								"tous les {3} pas",
								WARMUP, (double)peakLr, (double)(minLrRatio * 100), SAVEEVERY);
				}
				mEma = 0;
				NkChrono chrono;
				for (int s = 1; s <= STEPS; ++s) {
					float lr;
					if (WARMUP > 0 && s <= WARMUP)
						lr = peakLr * (float)s / (float)WARMUP;
					else {
						const double prog = (STEPS > WARMUP) ? (double)(s - WARMUP) / (double)(STEPS - WARMUP) : 1.0;
						const double cosv = 0.5 * (1.0 + NkCos(kPi * prog));
						lr = (float)(peakLr * (minLrRatio + (1.0 - minLrRatio) * cosv));
					}
					adam.SetLearningRate(lr);
					adam.ZeroGrad();
					double lv = 0.0;
					for (int m = 0; m < ACCUM; ++m) {
						NkTensor x, oneHot;
						MakeBatch(x, oneHot);
						NkVar logits = mGpt->Forward(mUseGpu ? x.ToGPU() : x);
						NkVar loss = autograd::SoftmaxCrossEntropy(
							logits, NkVar::Leaf(mUseGpu ? oneHot.ToGPU() : oneHot, false));
						NkVar scaled = (ACCUM > 1) ? autograd::MulScalar(loss, 1.0 / (double)ACCUM) : loss;
						scaled.Backward();
						lv += loss.Value().ToCPU().GetItem(NkShape{(int64)0}) / (double)ACCUM;
					}
					adam.Step();
					mEma = (s == 1) ? lv : 0.98 * mEma + 0.02 * lv;
					if (V && (s % 25 == 0 || s == 1))
						logger.Info("  pas {0} : perte = {1}  (moy. {2})  lr={3}", s, lv, mEma, (double)lr);
					if (V && s % 100 == 0) {
						logger.Info("    --- échantillons (pas {0}) ---", s);
						if (mLangs.Size() == 0)
							logger.Info("    {0}", Generate(mCfg.seed, 100, 0.8, -1).CStr());
						else
							for (int li = 0; li < (int)mLangs.Size(); ++li)
								logger.Info("    [{0}] {1}", mLangs[(nk_size)li].CStr(),
											Generate(mCfg.seed, 80, 0.8, li).CStr());
						logger.Info("    ---------------------------");
					}
					if (SAVEEVERY > 0 && hasSave && s % SAVEEVERY == 0) {
						if (Save(mCfg.savePath.CStr()) && V)
							logger.Info("  [checkpoint pas {0} -> {1}]", s, mCfg.savePath.CStr());
					}
				}
				if (V)
					logger.Info("Entraînement terminé en {0} s ({1}).", chrono.Elapsed().seconds,
								mUseGpu ? "GPU-résident" : "CPU");

				if (hasSave) {
					if (Save(mCfg.savePath.CStr())) {
						if (V)
							logger.Info("Modèle sauvegardé : {0}", mCfg.savePath.CStr());
					} else
						logger.Info("Échec de la sauvegarde : {0}", mCfg.savePath.CStr());
				}
			}

		} // namespace gpt
	} // namespace ai
} // namespace nkentseu
