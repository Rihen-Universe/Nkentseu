// =============================================================================
// NKSpeech/NkAsrModel.h — modèle acoustique ASR (BiGRU + CTC), header-only.
//
// Assemble les briques livrées (Option A.2) en un modèle acoustique from-scratch :
//   features MFCC (NkAudioFeatures) → GRU BIDIRECTIONNEL (avant + arrière) → tête
//   linéaire par trame → logits [T,1,V] → perte CTC (autograd::CTCLoss) et décodage
//   glouton. Petit vocabulaire de symboles (+ blanc), entraînable par NKOptim.
//
// Header-only VOLONTAIREMENT : dépend de NKNN/NKAutograd, que la lib NKSpeech (qui
// ne fait que l'extraction de features) ne lie pas — seuls les consommateurs qui ont
// déjà la pile NN (apps/tests) l'incluent. Namespace nkentseu::ai.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKNN/NkNN.h"			  // NkGRUCell, NkDense, ZeroState, GRURunSeq, StackTime
#include "NKAutograd/NkVar.h"	  // NkVar, autograd::*
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {

		// Modèle acoustique : GRU bidirectionnel + tête linéaire, sortie = logits CTC.
		// La bidirectionnalité (contexte passé ET futur) aide l'alignement CTC.
		class NkASRModel {
			public:
				NkASRModel() = default;

				// inputDim = dimension des features (MFCC), hidden = taille cachée d'un sens,
				// vocab = nb de symboles Y COMPRIS le blanc CTC.
				NkASRModel(int32 inputDim, int32 hidden, int32 vocab, uint32 seed = 1u)
					: mIn(inputDim), mH(hidden), mV(vocab), mFwd((uint32)inputDim, (uint32)hidden, seed + 10u),
					  mBwd((uint32)inputDim, (uint32)hidden, seed + 20u), mHeadF((uint32)hidden, (uint32)vocab, seed + 30u),
					  mHeadB((uint32)hidden, (uint32)vocab, seed + 40u) {
				}

				// `frames` : une trame [1, inputDim] par pas de temps. Renvoie les logits
				// empilés [T, 1, vocab] (prêts pour autograd::CTCLoss).
				NkVar Forward(const NkVector<NkVar> &frames) const {
					const uint32 T = frames.Size();
					// Sens AVANT.
					NkVector<NkVar> hf = nn::GRURunSeq(mFwd, frames, nn::ZeroState(1, (uint32)mH));
					// Sens ARRIÈRE : dérouler sur la séquence inversée puis réaligner sur t.
					NkVector<NkVar> rev;
					for (int32 t = (int32)T - 1; t >= 0; --t)
						rev.PushBack(frames[(nk_size)t]);
					NkVector<NkVar> hbRev = nn::GRURunSeq(mBwd, rev, nn::ZeroState(1, (uint32)mH));
					// Tête par trame : logit[t] = HeadF(hf[t]) + HeadB(hb[t]).
					NkVector<NkVar> logits;
					for (uint32 t = 0; t < T; ++t) {
						NkVar back = hbRev[(nk_size)(T - 1 - t)]; // réalignement
						NkVar lt = autograd::Add(mHeadF.Forward(hf[(nk_size)t]), mHeadB.Forward(back));
						logits.PushBack(lt);
					}
					return nn::StackTime(logits); // [T,1,V]
				}

				void Parameters(NkVector<NkVar> &o) const {
					mFwd.Parameters(o);
					mBwd.Parameters(o);
					mHeadF.Parameters(o);
					mHeadB.Parameters(o);
				}

				int32 Vocab() const {
					return mV;
				}

			private:
				int32 mIn = 0, mH = 0, mV = 0;
				nn::NkGRUCell mFwd, mBwd;
				nn::NkDense mHeadF, mHeadB;
		};

		// Décodage CTC glouton : argmax par trame → collapse répétitions → retire le blanc.
		inline void NkCTCGreedyDecode(const NkTensor &logits /*[T,1,V]*/, int32 blank, NkVector<int32> &out) {
			NkTensor lc = logits.ToCPU().Contiguous();
			const NkShape &sh = lc.Shape();
			const int64 T = sh[0], V = sh[2];
			const float *lp = lc.DataAs<float>();
			int32 prev = -1;
			for (int64 t = 0; t < T; ++t) {
				const float *row = lp + t * V; // B=1
				int32 arg = 0;
				for (int64 k = 1; k < V; ++k)
					if (row[k] > row[arg])
						arg = (int32)k;
				if (arg != prev && arg != blank)
					out.PushBack(arg);
				prev = arg;
			}
		}

	} // namespace ai
} // namespace nkentseu
