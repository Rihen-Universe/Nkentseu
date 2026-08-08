// =============================================================================
// NkQwen2Sft.cpp — voir NkQwen2Sft.h pour le contrat (modèle multi-couches
// gelé + LoRA, CE masquée, boucle Adam sur adaptateurs seuls, ChatML).
// =============================================================================
#include "NKInfer/NkQwen2Sft.h"
#include "NKInfer/NkLora.h"

#include <cstdio>
#include <cstring>
#include <cmath>

namespace nkentseu {
	namespace ai {
		namespace infer {

			namespace {

				// Énumération UNIQUE des 7 emplacements d'adaptateur d'une couche —
				// partagée par Init/Step du trainer pour que l'ordre des feuilles
				// NkVar et celui des gradients ne puissent JAMAIS diverger (une
				// divergence silencieuse appliquerait le gradient d'une projection
				// aux poids d'une autre, sans erreur visible).
				void CollectSlots(NkQwen2LoraSet &set, NkQwen2LoraSetGrads &grads, NkLoraPair *outPairs[7],
								  NkLoraGrad *outGrads[7]) {
					outPairs[0] = &set.q;
					outPairs[1] = &set.k;
					outPairs[2] = &set.v;
					outPairs[3] = &set.o;
					outPairs[4] = &set.gate;
					outPairs[5] = &set.up;
					outPairs[6] = &set.down;
					outGrads[0] = &grads.q;
					outGrads[1] = &grads.k;
					outGrads[2] = &grads.v;
					outGrads[3] = &grads.o;
					outGrads[4] = &grads.gate;
					outGrads[5] = &grads.up;
					outGrads[6] = &grads.down;
				}

				// Gradient accumulé -> tenseur moyenné. Un gradient jamais alloué
				// (adaptateur présent mais jamais traversé — ne devrait pas
				// arriver) devient un zéro de la forme du paramètre : Adam avance
				// alors sans toucher ce paramètre.
				NkTensor MeanGradOrZeros(const NkTensor &g, const NkTensor &paramShapeLike, int64 count) {
					if (!g.IsValid())
						return NkTensor::Zeros(paramShapeLike.Shape(), NkDType::NK_F32);
					NkTensor out = g.Clone();
					float *p = out.DataAs<float>();
					const float inv = 1.0f / (float)count;
					const int64 n = out.Numel();
					for (int64 i = 0; i < n; ++i)
						p[i] *= inv;
					return out;
				}

				// Un pas Adam EN PLACE sur un tenseur de paramètres (formules en
				// tête de NkQwen2Sft.h — identiques à optim::NkAdam, réécrites
				// ici car NKInfer ne dépend pas de NKOptim, méthode additive).
				// `scale` porte le clipping global déjà décidé par l'appelant.
				void AdamStepInPlace(NkTensor &param, const NkTensor &grad, NkTensor &mMom, NkTensor &vMom, float32 lr,
									 float32 b1, float32 b2, float32 eps, int64 t, float32 scale) {
					if (!mMom.IsValid())
						mMom = NkTensor::Zeros(param.Shape(), NkDType::NK_F32);
					if (!vMom.IsValid())
						vMom = NkTensor::Zeros(param.Shape(), NkDType::NK_F32);
					float *pp = param.DataAs<float>();
					const float *gp = grad.DataAs<float>();
					float *mp = mMom.DataAs<float>();
					float *vp = vMom.DataAs<float>();
					const double bc1 = 1.0 - std::pow((double)b1, (double)t);
					const double bc2 = 1.0 - std::pow((double)b2, (double)t);
					const int64 n = param.Numel();
					for (int64 i = 0; i < n; ++i) {
						const double g = (double)gp[i] * (double)scale;
						mp[i] = (float)((double)b1 * (double)mp[i] + (1.0 - (double)b1) * g);
						vp[i] = (float)((double)b2 * (double)vp[i] + (1.0 - (double)b2) * g * g);
						const double mhat = (double)mp[i] / bc1;
						const double vhat = (double)vp[i] / bc2;
						pp[i] = (float)((double)pp[i] - (double)lr * mhat / (std::sqrt(vhat) + (double)eps));
					}
				}

				// Somme des carrés d'un tenseur (pour la norme L2 globale du clip).
				double SumSquares(const NkTensor &t) {
					if (!t.IsValid())
						return 0.0;
					const float *p = t.DataAs<float>();
					double s = 0.0;
					const int64 n = t.Numel();
					for (int64 i = 0; i < n; ++i)
						s += (double)p[i] * (double)p[i];
					return s;
				}

			} // namespace

			// ---- Modèle -----------------------------------------------------------

			bool NkQwen2SftModel::IsValid() const {
				if (!cfg.IsValid() || layers.Size() == 0 || lora.Size() != layers.Size())
					return false;
				if (!embedding.IsValid() || embedding.Rank() != 2 || embedding.DType() != NkDType::NK_F32 ||
					embedding.Shape()[1] != cfg.dModel || embedding.Shape()[0] <= 0)
					return false;
				if (!finalNorm.IsValid() || finalNorm.Numel() != cfg.dModel)
					return false;
				if (!lmHead.IsValid() || lmHead.Rank() != 2 || lmHead.Shape()[1] != cfg.dModel ||
					lmHead.Shape()[0] <= 0)
					return false;
				for (nk_size k = 0; k < layers.Size(); ++k)
					if (!layers[k].IsValid(cfg))
						return false;
				return true;
			}

			// ---- Embedding gelé ---------------------------------------------------

			NkTensor NkQwen2SftEmbedLookup(const NkTensor &embedding, const NkVector<int32> &ids) {
				if (!embedding.IsValid() || embedding.Rank() != 2 || embedding.DType() != NkDType::NK_F32 ||
					ids.Size() == 0) {
					fprintf(stderr, "[NkQwen2Sft] EmbedLookup : embedding [V,d] f32 + ids non vides requis\n");
					return NkTensor();
				}
				const int64 V = embedding.Shape()[0], d = embedding.Shape()[1];
				const int64 T = (int64)ids.Size();
				NkTensor ec = embedding.IsContiguous() ? embedding : embedding.Contiguous();
				NkTensor out = NkTensor::Empty(NkShape{T, d}, NkDType::NK_F32);
				const float *ep = ec.DataAs<float>();
				float *op = out.DataAs<float>();
				for (int64 t = 0; t < T; ++t) {
					const int64 id = (int64)ids[(nk_size)t];
					if (id < 0 || id >= V) {
						fprintf(stderr, "[NkQwen2Sft] EmbedLookup : id %lld hors [0,%lld)\n", (long long)id,
								(long long)V);
						return NkTensor();
					}
					// Copie de ligne (jamais une vue) : la sortie doit vivre sa vie
					// indépendamment de la matrice d'embedding.
					std::memcpy(op + t * d, ep + id * d, (usize)d * sizeof(float));
				}
				return out;
			}

			// ---- Forward complet --------------------------------------------------

			NkTensor NkQwen2SftForward(const NkQwen2SftModel &m, const NkVector<int32> &ids, NkQwen2SftSaved &saved) {
				if (!m.IsValid()) {
					fprintf(stderr, "[NkQwen2Sft] Forward : modèle invalide\n");
					return NkTensor();
				}
				NkTensor x = NkQwen2SftEmbedLookup(m.embedding, ids);
				if (!x.IsValid())
					return NkTensor();
				const nk_size n = m.layers.Size();
				saved.layers.Clear();
				saved.layers.Resize(n);
				for (nk_size k = 0; k < n; ++k) {
					// Chaînage vertical : la sortie de la couche k est l'entrée de
					// la couche k+1 ; chaque couche sauvegarde SES activations.
					x = NkQwen2LayerForwardTrain(m.cfg, m.layers[k], m.lora[k], x, saved.layers[k]);
					if (!x.IsValid()) {
						fprintf(stderr, "[NkQwen2Sft] Forward : échec couche %u\n", (uint32)k);
						return NkTensor();
					}
				}
				saved.xFinal = x; // tenseur frais produit par la dernière couche
				NkTensor xn = NkRMSNorm(x, m.finalNorm, m.cfg.rmsEps);
				if (!xn.IsValid())
					return NkTensor();
				// lm_head gelé, [V,d] : logits = xn·Wᵀ ligne par ligne (aucune
				// transposée matérialisée — piège n°4 des notes).
				return NkLinearNoBias(xn, m.lmHead);
			}

			// ---- Cross-entropie masquée -------------------------------------------

			double NkQwen2SftMaskedCE(const NkTensor &logits, const NkVector<int32> &targets,
									  const NkVector<float32> &mask, NkTensor *dLogits, int64 *outActive) {
				if (!logits.IsValid() || logits.Rank() != 2 || logits.DType() != NkDType::NK_F32 ||
					(int64)targets.Size() != logits.Shape()[0] || targets.Size() != mask.Size()) {
					fprintf(stderr, "[NkQwen2Sft] MaskedCE : logits [T,V] + targets/mask de taille T requis\n");
					return -1.0;
				}
				const int64 T = logits.Shape()[0], V = logits.Shape()[1];
				NkTensor lc = logits.IsContiguous() ? logits : logits.Contiguous();
				const float *lp = lc.DataAs<float>();

				// Passe 1 : nombre de lignes actives — la normalisation par les
				// lignes ACTIVES (et pas par T) est le cœur du patch 03_LOSS_MASKING.
				int64 active = 0;
				for (nk_size t = 0; t < mask.Size(); ++t)
					if (mask[t] != 0.0f)
						++active;
				if (outActive)
					*outActive = active;
				float *dp = nullptr;
				if (dLogits) {
					// Zeros : les lignes masquées gardent un gradient EXACTEMENT nul.
					*dLogits = NkTensor::Zeros(NkShape{T, V}, NkDType::NK_F32);
					dp = dLogits->DataAs<float>();
				}
				if (active == 0)
					return 0.0; // rien à apprendre : perte ET gradient nuls

				double loss = 0.0;
				const double invActive = 1.0 / (double)active;
				for (int64 t = 0; t < T; ++t) {
					if (mask[(nk_size)t] == 0.0f)
						continue; // ligne de PROMPT : ni perte, ni gradient
					const int64 tgt = (int64)targets[(nk_size)t];
					if (tgt < 0 || tgt >= V) {
						fprintf(stderr, "[NkQwen2Sft] MaskedCE : cible %lld hors [0,%lld)\n", (long long)tgt,
								(long long)V);
						return -1.0;
					}
					const float *row = lp + t * V;
					// Softmax stable en double (max soustrait) : la perte sert de
					// référence aux différences finies, elle doit être propre.
					double mx = (double)row[0];
					for (int64 c = 1; c < V; ++c)
						if ((double)row[c] > mx)
							mx = (double)row[c];
					double sum = 0.0;
					for (int64 c = 0; c < V; ++c)
						sum += std::exp((double)row[c] - mx);
					const double logZ = mx + std::log(sum);
					loss += logZ - (double)row[tgt];
					if (dp) {
						float *drow = dp + t * V;
						const double invSum = 1.0 / sum;
						for (int64 c = 0; c < V; ++c)
							drow[c] = (float)(std::exp((double)row[c] - mx) * invSum * invActive);
						drow[tgt] -= (float)invActive; // (softmax − onehot)/actives
					}
				}
				return loss * invActive;
			}

			// ---- Backward complet -------------------------------------------------

			bool NkQwen2SftBackward(const NkQwen2SftModel &m, const NkQwen2SftSaved &saved, const NkTensor &dLogits,
									NkVector<NkQwen2LoraSetGrads> &grads) {
				if (!m.IsValid() || !saved.xFinal.IsValid() || saved.layers.Size() != m.layers.Size() ||
					!dLogits.IsValid() || dLogits.Rank() != 2 || dLogits.Shape()[1] != m.lmHead.Shape()[0]) {
					fprintf(stderr, "[NkQwen2Sft] Backward : modèle/saved/dLogits incohérents (Forward oublié ?)\n");
					return false;
				}
				const nk_size n = m.layers.Size();
				if (grads.Size() == 0)
					grads.Resize(n);
				if (grads.Size() != n) {
					fprintf(stderr, "[NkQwen2Sft] Backward : grads de taille inattendue\n");
					return false;
				}
				NkTensor dLc = dLogits.IsContiguous() ? dLogits : dLogits.Contiguous();
				// lm_head gelé : seul dX traverse — dXn[T,d] = dLogits[T,V]·W[V,d].
				NkTensor dXn = NkCpuMatmulAB(dLc, m.lmHead);
				if (!dXn.IsValid())
					return false;
				// RMSNorm final gelé (pas de dWeight, comme dans les couches).
				NkTensor dX = NkRMSNormBackward(saved.xFinal, m.finalNorm, m.cfg.rmsEps, dXn);
				if (!dX.IsValid())
					return false;
				// Couches en ordre INVERSE : le dX de la couche k+1 nourrit le
				// backward de la couche k — la traversée verticale du jalon 3.
				for (int64 k = (int64)n - 1; k >= 0; --k) {
					NkTensor dPrev;
					if (!NkQwen2LayerBackward(m.cfg, m.layers[(nk_size)k], m.lora[(nk_size)k],
											  saved.layers[(nk_size)k], dX, dPrev, grads[(nk_size)k])) {
						fprintf(stderr, "[NkQwen2Sft] Backward : échec couche %lld\n", (long long)k);
						return false;
					}
					dX = dPrev;
				}
				// dX sous la couche 0 : abandonné (embedding gelé).
				return true;
			}

			// ---- Formatage ChatML -------------------------------------------------

			bool NkQwen2SftFormatChatML(const NkQwen2Tokenizer &tok, const NkString &question, const NkString &answer,
										NkQwen2SftExample &out, NkString *err) {
				out.tokens.Clear();
				out.lossMask.Clear();
				if (!tok.IsLoaded() || tok.ImStartId() < 0 || tok.ImEndId() < 0) {
					if (err)
						*err = NkString("tokenizer sans <|im_start|>/<|im_end|> : ChatML impossible");
					return false;
				}
				// Partie PROMPT (masque 0) : gabarit + question + ouverture de la
				// réponse. Le \n après « assistant » fait partie du gabarit : le
				// modèle ne doit pas être noté sur sa capacité à le prédire.
				NkString prompt("<|im_start|>user\n");
				prompt += question;
				prompt += "<|im_end|>\n<|im_start|>assistant\n";
				// Partie RÉPONSE (masque 1) : la réponse ET le <|im_end|> qui la
				// clôt — apprendre à FINIR fait partie de la réponse (« token de
				// fin » du patch 03_LOSS_MASKING).
				NkString resp(answer);
				resp += "<|im_end|>";

				NkVector<int32> promptIds, respIds;
				if (!tok.EncodeWithSpecials(prompt, promptIds) || !tok.EncodeWithSpecials(resp, respIds)) {
					if (err)
						*err = NkString("échec d'encodage ChatML");
					return false;
				}
				for (nk_size i = 0; i < promptIds.Size(); ++i) {
					out.tokens.PushBack(promptIds[i]);
					out.lossMask.PushBack(0.0f);
				}
				for (nk_size i = 0; i < respIds.Size(); ++i) {
					out.tokens.PushBack(respIds[i]);
					out.lossMask.PushBack(1.0f);
				}
				return true;
			}

			// ---- Trainer ----------------------------------------------------------

			bool NkQwen2SftTrainer::Init(NkQwen2SftModel *model, float32 lr) {
				if (!model || !model->IsValid()) {
					fprintf(stderr, "[NkQwen2Sft] Trainer::Init : modèle invalide\n");
					return false;
				}
				mModel = model;
				mPairPtrs.Clear();
				mGrads.Clear();
				mGrads.Resize(model->lora.Size());
				mM.Clear();
				mV.Clear();
				mAccumCount = 0;
				mT = 0;
				mLr = lr;
				for (nk_size k = 0; k < model->lora.Size(); ++k) {
					NkLoraPair *pairs[7];
					NkLoraGrad *gr[7];
					CollectSlots(model->lora[k], mGrads[k], pairs, gr);
					for (int32 j = 0; j < 7; ++j) {
						if (!pairs[j]->IsValid())
							continue;
						mPairPtrs.PushBack(pairs[j]);
						// Moments alloués paresseusement au premier Step (formes
						// prises sur le paramètre) : deux entrées par paire, A
						// puis B — ordre verrouillé par CollectSlots.
						mM.PushBack(NkTensor());
						mM.PushBack(NkTensor());
						mV.PushBack(NkTensor());
						mV.PushBack(NkTensor());
					}
				}
				if (mPairPtrs.Size() == 0) {
					fprintf(stderr, "[NkQwen2Sft] Trainer::Init : aucun adaptateur LoRA valide\n");
					return false;
				}
				return true;
			}

			bool NkQwen2SftTrainer::SplitExample(const NkQwen2SftExample &ex, NkVector<int32> &inputs,
												 NkVector<int32> &targets, NkVector<float32> &mask) const {
				const nk_size n = ex.tokens.Size();
				if (n < 2 || ex.lossMask.Size() != n)
					return false;
				bool anyActive = false;
				for (nk_size t = 0; t + 1 < n; ++t) {
					inputs.PushBack(ex.tokens[t]);
					targets.PushBack(ex.tokens[t + 1]);
					// Le masque suit la CIBLE : la position t est notée si le token
					// qu'elle doit prédire (t+1) appartient à la réponse.
					mask.PushBack(ex.lossMask[t + 1]);
					if (ex.lossMask[t + 1] != 0.0f)
						anyActive = true;
				}
				return anyActive;
			}

			double NkQwen2SftTrainer::AccumulateExample(const NkQwen2SftExample &ex) {
				if (!mModel)
					return -1.0;
				NkVector<int32> inputs, targets;
				NkVector<float32> mask;
				if (!SplitExample(ex, inputs, targets, mask)) {
					fprintf(stderr, "[NkQwen2Sft] AccumulateExample : exemple trop court ou sans cible active\n");
					return -1.0;
				}
				NkQwen2SftSaved saved;
				NkTensor logits = NkQwen2SftForward(*mModel, inputs, saved);
				if (!logits.IsValid())
					return -1.0;
				NkTensor dLogits;
				const double L = NkQwen2SftMaskedCE(logits, targets, mask, &dLogits, nullptr);
				if (L < 0.0)
					return -1.0;
				if (!NkQwen2SftBackward(*mModel, saved, dLogits, mGrads))
					return -1.0;
				++mAccumCount;
				return L;
			}

			double NkQwen2SftTrainer::EvaluateExample(const NkQwen2SftExample &ex) const {
				if (!mModel)
					return -1.0;
				NkVector<int32> inputs, targets;
				NkVector<float32> mask;
				if (!SplitExample(ex, inputs, targets, mask))
					return -1.0;
				NkQwen2SftSaved saved;
				NkTensor logits = NkQwen2SftForward(*mModel, inputs, saved);
				if (!logits.IsValid())
					return -1.0;
				return NkQwen2SftMaskedCE(logits, targets, mask, nullptr, nullptr);
			}

			bool NkQwen2SftTrainer::Step() {
				if (!mModel || mAccumCount == 0) {
					fprintf(stderr, "[NkQwen2Sft] Step : rien d'accumulé\n");
					return false;
				}
				// Collecte des gradients MOYENNÉS — dans le MÊME ordre
				// d'énumération que Init (CollectSlots partagé : une divergence
				// appliquerait le gradient d'une projection à une autre).
				NkVector<NkTensor> meanGrads; // 2 par paire : A puis B
				nk_size p = 0;
				for (nk_size k = 0; k < mModel->lora.Size(); ++k) {
					NkLoraPair *pairs[7];
					NkLoraGrad *gr[7];
					CollectSlots(mModel->lora[k], mGrads[k], pairs, gr);
					for (int32 j = 0; j < 7; ++j) {
						if (!pairs[j]->IsValid())
							continue;
						if (p >= mPairPtrs.Size() || mPairPtrs[p] != pairs[j]) {
							fprintf(stderr, "[NkQwen2Sft] Step : ordre d'énumération corrompu\n");
							return false;
						}
						meanGrads.PushBack(MeanGradOrZeros(gr[j]->dA, pairs[j]->A, mAccumCount));
						meanGrads.PushBack(MeanGradOrZeros(gr[j]->dB, pairs[j]->B, mAccumCount));
						++p;
					}
				}
				// Clipping par norme L2 GLOBALE (tous adaptateurs comme un seul
				// vecteur) : un facteur d'échelle unique préserve la direction.
				float32 scale = 1.0f;
				if (mClipNorm > 0.0f) {
					double ss = 0.0;
					for (nk_size i = 0; i < meanGrads.Size(); ++i)
						ss += SumSquares(meanGrads[i]);
					const double norm = std::sqrt(ss);
					if (norm > (double)mClipNorm)
						scale = (float32)((double)mClipNorm / (norm + 1e-12));
				}
				++mT; // correction de biais : t commence à 1
				for (nk_size i = 0; i < mPairPtrs.Size(); ++i) {
					AdamStepInPlace(mPairPtrs[i]->A, meanGrads[(nk_size)(2 * i)], mM[(nk_size)(2 * i)],
									mV[(nk_size)(2 * i)], mLr, mB1, mB2, mEps, mT, scale);
					AdamStepInPlace(mPairPtrs[i]->B, meanGrads[(nk_size)(2 * i + 1)], mM[(nk_size)(2 * i + 1)],
									mV[(nk_size)(2 * i + 1)], mLr, mB1, mB2, mEps, mT, scale);
				}
				// Remise à zéro de l'accumulation : des NkLoraGrad frais (les
				// tenseurs seront réalloués à zéro au premier +=).
				for (nk_size k = 0; k < mGrads.Size(); ++k)
					mGrads[k] = NkQwen2LoraSetGrads();
				mAccumCount = 0;
				return true;
			}

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
