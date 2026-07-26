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
#include "NKMath/NKMath.h"		  // math::NkExp (softmax des logits par trame)

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

		// =====================================================================
		// Décodage CTC par recherche en faisceau (beam search).
		// -----------------------------------------------------------------------
		// Règle standard (algorithme "prefix beam search" popularisé par
		// Hannun, « Sequence Modeling with CTC », 2017 — https://distill.pub/2017/ctc/,
		// section « Beam Search » ; formalisé originellement par Graves et al.,
		// « Connectionist Temporal Classification », ICML 2006) : on maintient un
		// ensemble borné (le "faisceau") de PRÉFIXES DÉCODÉS (suites de symboles,
		// blanc/répétitions déjà retirés), chacun avec deux probabilités disjointes :
		//   - pb  = P(les chemins CTC menant à ce préfixe et se terminant par un blanc)
		//   - pnb = P(... se terminant par une répétition du dernier symbole du préfixe)
		// À chaque trame, chaque hypothèse est étendue par chaque symbole du vocabulaire
		// et les préfixes identiques obtenus par des chemins différents sont FUSIONNÉS
		// (sommés) — c'est ce qui distingue le beam search CTC d'un beam search générique
		// (sans cette fusion, on ferait juste du glouton avec k candidats indépendants).
		// Seules les `beamWidth` meilleures hypothèses (par pb+pnb) sont conservées.
		// =====================================================================

		// Une hypothèse du faisceau : préfixe déjà "collapsé" (sans blanc ni répétition
		// consécutive) + les deux probabilités disjointes pb/pnb (voir ci-dessus).
		struct NkCTCBeamHyp {
				NkVector<int32> seq;
				double pb = 0.0;
				double pnb = 0.0;
		};

		namespace detail {

			inline bool NkSameIntSeq(const NkVector<int32> &a, const NkVector<int32> &b) {
				if (a.Size() != b.Size())
					return false;
				for (uint32 i = 0; i < a.Size(); ++i)
					if (a[i] != b[i])
						return false;
				return true;
			}

			// Recherche linéaire du préfixe `seq` dans le faisceau courant : le faisceau
			// reste petit (beamWidth, typiquement 5-20) donc O(beamWidth) par appel est
			// largement suffisant ici (pas besoin d'une table de hachage dédiée).
			inline int32 NkFindHyp(const NkVector<NkCTCBeamHyp> &beam, const NkVector<int32> &seq) {
				for (uint32 i = 0; i < beam.Size(); ++i)
					if (NkSameIntSeq(beam[i].seq, seq))
						return (int32)i;
				return -1;
			}

			// Ajoute `p` à pb (si isBlankBranch) ou pnb du candidat `seq` dans `cand`,
			// en le créant s'il n'existe pas encore (fusion des préfixes identiques).
			inline void NkAccumulate(NkVector<NkCTCBeamHyp> &cand, const NkVector<int32> &seq, double p, bool isBlankBranch) {
				int32 idx = NkFindHyp(cand, seq);
				if (idx < 0) {
					NkCTCBeamHyp nh;
					nh.seq = seq;
					cand.PushBack(nh);
					idx = (int32)cand.Size() - 1;
				}
				if (isBlankBranch)
					cand[(nk_size)idx].pb += p;
				else
					cand[(nk_size)idx].pnb += p;
			}

		} // namespace detail

		// Exécute le beam search CTC et renvoie le FAISCEAU FINAL complet (toutes les
		// hypothèses survivantes, pas seulement la meilleure) — factorisé hors de
		// `NkCTCBeamSearchDecode` pour être réutilisé par `NkCTCBeamSearchNBest` (le
		// RE-SCORING par modèle de langue, ci-dessous et dans NkLangModel.h, a besoin de
		// PLUSIEURS hypothèses candidates, pas d'une seule déjà tranchée par le score
		// acoustique seul). Code de boucle inchangé par rapport à la version d'origine.
		inline NkVector<NkCTCBeamHyp> NkCTCBeamSearchRun(const NkTensor &logits /*[T,1,V]*/, int32 blank, int32 beamWidth) {
			NkTensor lc = logits.ToCPU().Contiguous();
			const NkShape &sh = lc.Shape();
			const int64 T = sh[0], V = sh[2];
			const float *lp = lc.DataAs<float>();
			if (beamWidth < 1)
				beamWidth = 1;

			// Faisceau initial : préfixe vide, entièrement "en blanc" (pb=1).
			NkVector<NkCTCBeamHyp> beam;
			{
				NkCTCBeamHyp h0;
				h0.pb = 1.0;
				h0.pnb = 0.0;
				beam.PushBack(h0);
			}

			NkVector<double> probs;
			probs.Resize((nk_size)V);

			for (int64 t = 0; t < T; ++t) {
				const float *row = lp + t * V; // B=1

				// Softmax stable de la trame : les logits ne sont PAS déjà des
				// probabilités (contrairement au glouton, le beam search a besoin de
				// vraies probabilités pour sommer plusieurs chemins).
				float mx = row[0];
				for (int64 k = 1; k < V; ++k)
					if (row[k] > mx)
						mx = row[k];
				double sum = 0.0;
				for (int64 k = 0; k < V; ++k) {
					const double e = (double)math::NkExp((float64)(row[k] - mx));
					probs[(nk_size)k] = e;
					sum += e;
				}
				const double invSum = (sum > 0.0) ? 1.0 / sum : 0.0;
				for (int64 k = 0; k < V; ++k)
					probs[(nk_size)k] *= invSum;

				NkVector<NkCTCBeamHyp> cand; // candidats de ce pas, préfixes fusionnés

				for (uint32 h = 0; h < beam.Size(); ++h) {
					const NkCTCBeamHyp &hyp = beam[h];
					const double pTotal = hyp.pb + hyp.pnb;
					if (pTotal <= 0.0)
						continue;

					for (int64 k = 0; k < V; ++k) {
						const double p = probs[(nk_size)k];
						if (p <= 0.0)
							continue;

						if ((int32)k == blank) {
							// Blanc : le préfixe ne change pas ; il "libère" la prochaine
							// répétition (règle CTC standard).
							detail::NkAccumulate(cand, hyp.seq, p * pTotal, /*isBlankBranch=*/true);
						} else {
							const bool repeatsLast = !hyp.seq.IsEmpty() && hyp.seq.Back() == (int32)k;
							if (repeatsLast) {
								// (a) Pas de blanc entre les deux occurrences : le chemin
								//     collapse SANS ajouter de symbole (répétition consommée).
								detail::NkAccumulate(cand, hyp.seq, p * hyp.pnb, /*isBlankBranch=*/false);
								// (b) Un blanc séparait déjà : nouvelle occurrence -> préfixe étendu.
								NkVector<int32> ext = hyp.seq;
								ext.PushBack((int32)k);
								detail::NkAccumulate(cand, ext, p * hyp.pb, /*isBlankBranch=*/false);
							} else {
								// Symbole différent du dernier du préfixe : extension normale.
								NkVector<int32> ext = hyp.seq;
								ext.PushBack((int32)k);
								detail::NkAccumulate(cand, ext, p * pTotal, /*isBlankBranch=*/false);
							}
						}
					}
				}

				// Garde les `beamWidth` meilleures hypothèses (par pb+pnb). Sélection
				// partielle O(beamWidth * |cand|) : |cand| <= beamWidth*V reste petit
				// pour ce module (vocabulaires de quelques dizaines de symboles), pas
				// besoin d'un tri générique.
				NkVector<uint8> used;
				used.Resize((nk_size)cand.Size());
				for (uint32 i = 0; i < cand.Size(); ++i)
					used[i] = 0;
				NkVector<NkCTCBeamHyp> next;
				const uint32 keep = ((uint32)beamWidth < cand.Size()) ? (uint32)beamWidth : cand.Size();
				for (uint32 sel = 0; sel < keep; ++sel) {
					int32 best = -1;
					double bestScore = -1.0;
					for (uint32 i = 0; i < cand.Size(); ++i) {
						if (used[i])
							continue;
						const double score = cand[i].pb + cand[i].pnb;
						if (score > bestScore) {
							bestScore = score;
							best = (int32)i;
						}
					}
					if (best < 0)
						break;
					used[(nk_size)best] = 1;
					next.PushBack(cand[(nk_size)best]);
				}
				beam = next;
				if (beam.IsEmpty()) {
					// Sécurité (ne devrait pas arriver : la branche blanc a toujours p>0
					// puisque softmax(row) > 0 partout) — évite un faisceau vide.
					NkCTCBeamHyp h0;
					h0.pb = 1.0;
					h0.pnb = 0.0;
					beam.PushBack(h0);
				}
			}
			return beam;
		}

		// Décodage CTC par recherche en faisceau. `beamWidth` >= 1 (1 se comporte comme
		// le glouton, mais reste correct car les préfixes identiques sont fusionnés à
		// chaque pas — ce n'est PAS strictement le même code que NkCTCGreedyDecode).
		// `logits` : [T,1,V], symboles 0..V-1 dont `blank`. Résultat dans `out`.
		inline void NkCTCBeamSearchDecode(const NkTensor &logits /*[T,1,V]*/, int32 blank, int32 beamWidth, NkVector<int32> &out) {
			NkVector<NkCTCBeamHyp> beam = NkCTCBeamSearchRun(logits, blank, beamWidth);

			// Meilleure hypothèse finale = plus forte probabilité totale pb+pnb.
			uint32 best = 0;
			double bestScore = beam[0].pb + beam[0].pnb;
			for (uint32 i = 1; i < beam.Size(); ++i) {
				const double score = beam[i].pb + beam[i].pnb;
				if (score > bestScore) {
					bestScore = score;
					best = i;
				}
			}
			out = beam[(nk_size)best].seq;
		}

		// =====================================================================
		// N-best + RE-SCORING par modèle de langue n-gram (brique 3 de la Phase 8,
		// « Lexique/décodage »).
		// -----------------------------------------------------------------------
		// Le beam search CTC ci-dessus ne score les hypothèses QUE par la
		// vraisemblance ACOUSTIQUE (pb+pnb) : il ignore complètement la plausibilité
		// LINGUISTIQUE d'une suite de symboles. `NkCTCBeamSearchNBest` expose le
		// faisceau final complet (au lieu d'une seule hypothèse déjà tranchée) pour
		// permettre un RE-SCORING externe ; `NkRescoreWithLM` combine ce score
		// acoustique avec un modèle de langue n-gram (`NkNgramLM`, NkLangModel.h) par
		// interpolation log-linéaire — technique standard dite « shallow fusion » /
		// « N-best rescoring » (sources détaillées en tête de NkLangModel.h : Hannun
		// et al., Deep Speech, arXiv:1412.5567, 2014 ; Graves & Jaitly, ICML 2014).
		// =====================================================================

		// Une hypothèse du N-best final : préfixe + score ACOUSTIQUE pur en log
		// (log(pb+pnb) du beam search CTC, AVANT tout re-scoring linguistique).
		struct NkCTCNBestHyp {
				NkVector<int32> seq;
				double acousticLogProb = 0.0;
		};

		// Renvoie jusqu'à `beamWidth` hypothèses finales (le faisceau complet),
		// triées par score acoustique décroissant. Contrairement à
		// `NkCTCBeamSearchDecode`, ne choisit PAS la meilleure : c'est au
		// re-scoring (ou à l'appelant) de trancher.
		inline void NkCTCBeamSearchNBest(const NkTensor &logits /*[T,1,V]*/, int32 blank, int32 beamWidth,
										  NkVector<NkCTCNBestHyp> &outNBest) {
			NkVector<NkCTCBeamHyp> beam = NkCTCBeamSearchRun(logits, blank, beamWidth);
			outNBest.Clear();
			for (uint32 i = 0; i < beam.Size(); ++i) {
				const double pTotal = beam[i].pb + beam[i].pnb;
				NkCTCNBestHyp h;
				h.seq = beam[i].seq;
				h.acousticLogProb = (pTotal > 0.0) ? (double)math::NkLog((float64)pTotal) : -1.0e9;
				outNBest.PushBack(h);
			}
			// Tri par score acoustique décroissant (N petit : beamWidth typiquement
			// <= quelques dizaines -> tri par sélection, pas besoin de mieux).
			for (uint32 i = 0; i < outNBest.Size(); ++i) {
				uint32 best = i;
				for (uint32 j = i + 1; j < outNBest.Size(); ++j)
					if (outNBest[j].acousticLogProb > outNBest[best].acousticLogProb)
						best = j;
				if (best != i) {
					NkCTCNBestHyp tmp = outNBest[i];
					outNBest[i] = outNBest[best];
					outNBest[best] = tmp;
				}
			}
		}

		// RE-SCORE la liste N-best en combinant score acoustique et score du modèle de
		// langue : score(c) = acousticLogProb(c) + lmWeight * avgLogProb_LM(c), où
		// avgLogProb_LM = LM.LogProb(c) / longueur(c) (log-probabilité MOYENNE PAR
		// SYMBOLE, PAS la somme brute) — shallow fusion log-linéaire (cf. sources en
		// tête de NkLangModel.h), mais avec NORMALISATION PAR LONGUEUR.
		// ⚠️ Pourquoi la normalisation est nécessaire (constaté EMPIRIQUEMENT en
		// construisant ce test, cf. NKASRTest) : une probabilité de séquence n-gramme
		// BRUTE est un PRODUIT de termes <= 1, donc décroît mécaniquement avec la
		// longueur — un LM même bien entraîné préfère presque toujours, à tort, une
		// hypothèse plus COURTE (biais de longueur classique, documenté ex. dans la
		// littérature NMT/ASR sous le nom de « length normalization », voir Wu et al.,
		// « Google's Neural Machine Translation System », arXiv:1609.08144, 2016,
		// section « length normalization » — et c'est précisément pour compenser ce
		// biais que Hannun et al. (Deep Speech, arXiv:1412.5567, 2014) ajoutent un
		// terme bonus par mot β·|c| à leur score, cf. NkLangModel.h). Diviser par la
		// longueur (plutôt que d'ajouter un bonus fixe, qui peut sur-corriger et
		// favoriser des hypothèses artificiellement longues, cf. limite constatée et
		// documentée dans NKASRTest) neutralise ce biais sans créer de sur-correction.
		// Renvoie la séquence gagnante dans `out` ; false si `nbest` est vide.
		template <typename TLangModel>
		inline bool NkRescoreWithLM(const NkVector<NkCTCNBestHyp> &nbest, const TLangModel &lm, double lmWeight,
									NkVector<int32> &out) {
			if (nbest.IsEmpty())
				return false;
			auto avgLm = [&](const NkVector<int32> &seq) {
				const double len = seq.IsEmpty() ? 1.0 : (double)seq.Size();
				return lm.LogProb(seq) / len;
			};
			uint32 best = 0;
			double bestScore = nbest[0].acousticLogProb + lmWeight * avgLm(nbest[0].seq);
			for (uint32 i = 1; i < nbest.Size(); ++i) {
				const double score = nbest[i].acousticLogProb + lmWeight * avgLm(nbest[i].seq);
				if (score > bestScore) {
					bestScore = score;
					best = i;
				}
			}
			out = nbest[(nk_size)best].seq;
			return true;
		}

	} // namespace ai
} // namespace nkentseu
