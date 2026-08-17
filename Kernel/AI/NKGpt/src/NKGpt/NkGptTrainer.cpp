// =============================================================================
// NKGpt/NkGptTrainer.cpp — implémentation de l'entraîneur GPT réutilisable
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKGpt/NkGptTrainer.h"
#include "NKData/NkBpeTrainer.h" // tokenizer pré-entraîné (LoadBpe) + encodeur à mémo
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
				delete mLlama;
			}

			double NkGptTrainer::NextRand() {
				mRng = mRng * 6364136223846793005ull + 1442695040888963407ull;
				return (double)((mRng >> 11) & 0xFFFFFFFFFFFFFull) / (double)(1ull << 52);
			}

			// texts (par langue) -> mLangData/mLangMask, avec le BPE courant. Masque la question des
			// blocs "Question:/Réponse:" (instruction-tuning : seule la réponse compte dans la loss).
			nk_size NkGptTrainer::EncodeCorpus(const NkVector<NkString> &texts) {
				const NkString markerStr = mCfg.qaMarker.Empty() ? NkString("Réponse: ") : mCfg.qaMarker;
				const NkStringView marker(markerStr.CStr());
				// Encodeur à mémo : sur un corpus de plusieurs dizaines de Mo, le même mot
				// est ré-encodé des millions de fois pour un résultat identique. Sans le
				// mémo, l'encodage du corpus coûte plus cher que plusieurs pas
				// d'entraînement. Résultat strictement identique (vérifié par NKBpeTest).
				data::NkBpeEncoder enc(mBpe);
				mLangData.Resize((nk_size)texts.Size());
				mLangMask.Resize((nk_size)texts.Size());
				mLangStarts.Resize((nk_size)texts.Size());
				nk_size totalTok = 0;
				for (int64 li = 0; li < (int64)texts.Size(); ++li) {
					const NkString &txt = texts[(nk_size)li];
					const bool isQa = txt.Find(marker) != NkString::npos;
					if (!isQa) {
						NkVector<int32> ids;
						enc.Encode(txt, ids);
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

							// On note OÙ commence ce bloc. Le lot pourra alors démarrer sa
							// fenêtre ici plutôt qu'au milieu de nulle part, et voir
							// l'exemple ENTIER. Voir mLangStarts dans l'en-tête pour ce que
							// coûtait l'absence de cette note.
							if ((nk_size)li < mLangStarts.Size())
								mLangStarts[(nk_size)li].PushBack((int64)mLangData[(nk_size)li].Size());

							// Un bloc peut contenir PLUSIEURS tours, pas un seul. On alterne
							// donc : tout ce qui va jusqu'au marqueur de réponse INCLUS est
							// masqué (c'est la question), tout ce qui suit jusqu'au tour
							// suivant compte dans la perte (c'est la réponse).
							// SANS cette alternance, la version précédente comptait dans la
							// perte TOUT ce qui suit la première réponse — donc les questions
							// suivantes de l'utilisateur : le modèle apprendrait à les écrire
							// à sa place. Sur un corpus à un seul tour, ce code fait
							// exactement ce que faisait l'ancien (la boucle ne tourne qu'une
							// fois), la non-régression est donc structurelle.
							auto emettre = [&](const NkString &s, float masque) {
								if (s.Size() == 0)
									return;
								NkVector<int32> ids;
								enc.Encode(s, ids);
								for (int64 k = 0; k < (int64)ids.Size(); ++k) {
									mLangData[(nk_size)li].PushBack((float)ids[(nk_size)k]);
									mLangMask[(nk_size)li].PushBack(masque);
								}
							};
							const nk_size blen2 = block.Size();
							// ⚠️ UN CORPUS PEUT ÊTRE MIXTE. Dès qu'un seul « Reponse: »
							// figurait dans le fichier, TOUT le texte passait par le chemin
							// question/réponse — et un paragraphe de prose, dépourvu du
							// marqueur, était intégralement MASQUÉ. Constaté en assemblant
							// l'identité et Wikipédia : 0,075 % des positions comptaient
							// dans la perte, et le premier pas rendait une perte de ZÉRO.
							// Un bloc sans marqueur est de la PROSE : il compte en entier.
							if (block.Find(markerStr.CStr()) == NkString::npos) {
								emettre(block, 1.f);
								NkVector<int32> sepP;
								enc.Encode(NkString("\n\n"), sepP);
								for (int64 k = 0; k < (int64)sepP.Size(); ++k) {
									mLangData[(nk_size)li].PushBack((float)sepP[(nk_size)k]);
									mLangMask[(nk_size)li].PushBack(1.f);
								}
								continue;
							}
							nk_size bp = 0;
							while (bp < blen2) {
								const nk_size mp = block.Find(markerStr.CStr(), bp);
								if (mp == NkString::npos) {
									emettre(block.SubStr(bp), 0.f); // question sans réponse
									break;
								}
								emettre(block.SubStr(bp, mp + marker.Size() - bp), 0.f);
								bp = mp + marker.Size();
								// Le tour suivant commence par un « Question: » EN DÉBUT DE
								// LIGNE — chercher « Question: » seul le trouverait aussi au
								// milieu d'une réponse qui en parle.
								const nk_size nq = block.Find("\nQuestion:", bp);
								const nk_size fin = (nq == NkString::npos) ? blen2 : nq;
								if (fin > bp)
									emettre(block.SubStr(bp, fin - bp), 1.f);
								bp = fin;
							}
							NkVector<int32> sepIds;
							enc.Encode(NkString("\n\n"), sepIds);
							for (int64 k = 0; k < (int64)sepIds.Size(); ++k) {
								mLangData[(nk_size)li].PushBack((float)sepIds[(nk_size)k]);
								mLangMask[(nk_size)li].PushBack(0.f);
							}
						}
					}
					totalTok += mLangData[(nk_size)li].Size();
				}

				// Le masquage question/réponse repose sur DEUX conditions muettes : que le
				// marqueur figure tel quel dans le texte, et que les blocs soient séparés
				// par une ligne vide en « \n\n » (un corpus Windows en « \r\n\r\n » n'en
				// produit qu'un seul, énorme). Si l'une des deux manque, l'entraînement se
				// déroule normalement — sur la mauvaise cible. On mesure donc la part
				// réellement masquée au lieu de la supposer.
				{
					nk_size actifs = 0, total = 0;
					for (int64 li = 0; li < (int64)mLangMask.Size(); ++li)
						for (int64 k = 0; k < (int64)mLangMask[(nk_size)li].Size(); ++k) {
							++total;
							if (mLangMask[(nk_size)li][(nk_size)k] != 0.f)
								++actifs;
						}
					if (mCfg.verbose && total > 0)
						logger.Info("Masquage (marqueur « {0} ») : {1}% des positions comptent dans la perte.",
									markerStr.CStr(), 100.0 * (double)actifs / (double)total);
				}

				// Split held-out : réserve la QUEUE de chaque langue au val (jamais vue à l'entraînement).
				mValData.Clear();
				mValMask.Clear();
				if (mCfg.valFrac > 0.f && mCfg.valFrac < 0.9f) {
					mValData.Resize((nk_size)texts.Size());
					mValMask.Resize((nk_size)texts.Size());
					for (int64 li = 0; li < (int64)texts.Size(); ++li) {
						NkVector<float> &dd = mLangData[(nk_size)li];
						NkVector<float> &mm = mLangMask[(nk_size)li];
						const int64 N = (int64)dd.Size();
						const int64 nVal = (int64)((double)N * (double)mCfg.valFrac);
						if (nVal <= mT || N - nVal <= mT)
							continue; // pas assez de tokens pour découper proprement
						const int64 cut = N - nVal; // val = queue [cut, N)
						for (int64 k = cut; k < N; ++k) {
							mValData[(nk_size)li].PushBack(dd[(nk_size)k]);
							mValMask[(nk_size)li].PushBack((k < (int64)mm.Size()) ? mm[(nk_size)k] : 1.f);
						}
						dd.Resize((nk_size)cut);
						mm.Resize((nk_size)cut);
					}
					nk_size valTok = 0;
					for (int64 li = 0; li < (int64)mValData.Size(); ++li)
						valTok += mValData[(nk_size)li].Size();
					if (mCfg.verbose)
						logger.Info("Validation held-out : {0} tokens réservés ({1}% de queue par langue, jamais vus "
									"à l'entraînement).",
									(unsigned long long)valTok, (double)(mCfg.valFrac * 100.f));
					// Les fenetres ne peuvent PAS etre construites ici : mT n'est affecte
					// qu'apres cet appel dans le chemin « depart de zero ». L'appel est
					// donc pose dans Prepare, une fois les dimensions connues.
				}
				return totalTok;
			}

			// Tire UNE FOIS les fenetres de validation, puis n'y touche plus.
			//
			// Flux aleatoire LOCAL, germe constant : ni --graine ni le deroulement de
			// l'entrainement ne peuvent le decaler. Deux entrainements quelconques
			// evaluent donc exactement le meme texte, ce qui rend leurs pertes
            // comparables terme a terme.
			//
			// Repartition PROPORTIONNELLE a la taille de chaque langue : prendre autant
			// de fenetres partout sur-representerait une langue peu presente, et la perte
			// de validation ne refleterait plus le corpus.
			void NkGptTrainer::ConstruireFenetresVal() {
				mValFenetres.Clear();
				const int nL = (int)mValData.Size();
				if (nL <= 0 || mT <= 0)
					return;

				int64 total = 0;
				for (int li = 0; li < nL; ++li)
					total += (int64)mValData[(nk_size)li].Size();
				if (total <= 0)
					return;

				// Assez de fenetres pour que la mesure ne depende pas du tirage, assez peu
				// pour que l'evaluation reste breve : forward seul, sans gradient.
				const int voulu = mCfg.valFenetres > 0 ? mCfg.valFenetres : 192;
				uint64 rng = 0xD1B54A32D192ED03ull; // flux SEPARE de mRng, volontairement

				for (int li = 0; li < nL; ++li) {
					const int64 N = (int64)mValData[(nk_size)li].Size();
					if (N <= mT + 1)
						continue; // trop court pour une seule fenetre
					int part = (int)(((double)N / (double)total) * (double)voulu + 0.5);
					if (part < 1)
						part = 1; // une langue presente compte au moins une fois
					for (int k = 0; k < part; ++k) {
						rng = rng * 6364136223846793005ull + 1442695040888963407ull;
						const double u = (double)((rng >> 11) & 0xFFFFFFFFFFFFFull) / (double)(1ull << 52);
						FenetreVal f;
						f.langue = li;
						f.decalage = (int64)(u * (double)(N - mT - 1));
						mValFenetres.PushBack(f);
					}
				}
				// Empreinte du jeu de fenetres, ECRITE AU JOURNAL.
				//
				// « Les deux entrainements ont bien vu le meme texte » est une promesse
				// qu'on ne peut pas tenir sur parole : il suffit d'un argument different
				// pour qu'elle devienne fausse sans que rien ne le signale. L'empreinte
				// rend la promesse CONSTATABLE — deux journaux qui portent le meme nombre
				// se comparent, deux journaux qui different se rejettent.
				uint64 emp = 1469598103934665603ull; // FNV-1a 64
				for (nk_size i = 0; i < mValFenetres.Size(); ++i) {
					const uint64 v = ((uint64)(uint32)mValFenetres[i].langue << 40) ^ (uint64)mValFenetres[i].decalage;
					for (int o = 0; o < 8; ++o) {
						emp ^= (v >> (o * 8)) & 0xFFull;
						emp *= 1099511628211ull;
					}
				}
				// Rendue en HEXADECIMAL, et le formatage est fait a la main : le
				// journal rend un entier non signe comme signe, et une empreinte
				// affichee « -7720886274511702334 » se lit comme une anomalie alors
				// qu'elle est juste. Seize chiffres hexadecimaux ne posent pas la question.
				char hex[17];
				for (int i = 0; i < 16; ++i) {
					const unsigned d = (unsigned)((emp >> ((15 - i) * 4)) & 0xFull);
					hex[i] = (char)(d < 10 ? ('0' + d) : ('a' + (d - 10)));
				}
				hex[16] = '\0';
				if (mCfg.verbose)
					logger.Info("Validation figée : {0} fenêtres de {1} tokens ({2} tokens évalués), "
								"empreinte {3} — identiques à chaque évaluation et d'un entraînement à "
								"l'autre (comparer cette empreinte entre journaux).",
								(unsigned long long)mValFenetres.Size(), (long long)mT,
								(unsigned long long)((int64)mValFenetres.Size() * mT), hex);
			}

			// Lot de validation numero `indexLot` pris dans le jeu fige.
			// Meme mise en forme que MakeBatchIdxFrom (tag de langue en tete, cible
			// decalee d'un token), mais le decalage vient de mValFenetres au lieu du hasard.
			void NkGptTrainer::MakeBatchValFige(int indexLot, NkTensor &x, NkTensor &targetIdx) {
				NkShape xs;
				xs.PushBack(mB);
				xs.PushBack(mT);
				x = NkTensor::Zeros(xs);
				targetIdx = NkTensor::Full(NkShape{mB * mT}, -1.0);
				float *xp = x.DataAs<float>();
				float *tp = targetIdx.DataAs<float>();
				const int64 base = (int64)indexLot * mB;
				for (int64 b = 0; b < mB; ++b) {
					const int64 idx = base + b;
					if (idx >= (int64)mValFenetres.Size())
						break; // dernier lot incomplet : les lignes restantes restent masquees (-1)
					const FenetreVal &f = mValFenetres[(nk_size)idx];
					const NkVector<float> &dd = mValData[(nk_size)f.langue];
					const NkVector<float> &mk = mValMask[(nk_size)f.langue];
					const bool hasMask = (mk.Size() == dd.Size());
					const int64 off = f.decalage;
					xp[b * mT + 0] = (float)(mNByte + f.langue);
					if (!hasMask || mk[(nk_size)off] != 0.f)
						tp[b * mT + 0] = dd[(nk_size)off];
					for (int64 t = 1; t < mT; ++t) {
						xp[b * mT + t] = dd[(nk_size)(off + t - 1)];
						if (!hasMask || mk[(nk_size)(off + t)] != 0.f)
							tp[b * mT + t] = dd[(nk_size)(off + t)];
					}
				}
			}

			// Lot d'entraînement (échantillonné depuis mLangData).
			// Une fenêtre sur deux démarre au DÉBUT d'un bloc, l'autre au hasard.
			//
			// Les deux sont nécessaires, pour des raisons opposées. Démarrer au début
			// d'un bloc est le SEUL moyen de voir un exemple structuré en entier —
			// sans quoi le modèle apprend à répondre depuis un contexte AMPUTÉ, donc
			// à inventer une phrase qui ne s'y trouve pas. Mais ne faire QUE cela lui
			// apprendrait que tout commence à la position zéro d'une fenêtre, ce qui
			// est faux dès qu'on lui parle vraiment : le hasard garde cette souplesse.
			int64 NkGptTrainer::ChoisirDecalage(int li, int64 N) {
				// ⚠️ LE FLUX ALEATOIRE SE CONSOMME INCONDITIONNELLEMENT.
				//
				// La version precedente ecrivait :
				//     auDebut = A && B && (NextRand() < 0.5);
				// Le && COURT-CIRCUITE : quand B est faux (aucun debut de bloc
				// enregistre), NextRand() n'etait pas appele. Et les deux chemins ne
				// tiraient pas le meme nombre de fois. Le NOMBRE DE TIRAGES dependait
				// donc du CONTENU du corpus.
				//
				// Consequence : « meme graine » ne designait plus « meme course » des
				// que le corpus changeait — c'est le contenu qui decidait de la position
				// dans le flux. Deux regimes, meme declares proprement, produisaient deux
				// UNIVERS differents et restaient incomparables.
				//
				// REGLE GENERALE : un flux pseudo-aleatoire ne se consomme JAMAIS sous
				// condition des donnees. Sinon la reproductibilite devient fonction du
				// fichier, et « meme graine, resultat different » redevient possible —
				// ce qui a deja coute une chasse cette semaine.
				//
				// On tire donc TOUJOURS trois fois, et on jette ce qui ne sert pas. Seul
				// le MODE change entre deux regimes, plus l'univers entier.
				const double rMode = NextRand();  // choix du mode
				const double rBloc = NextRand();  // index du bloc
				const double rUnif = NextRand();  // decalage uniforme (et repli)

				const int64 maxOff = N - mT - 1;
				if (maxOff <= 0)
					return 0;

				const bool blocsDispo =
					((nk_size)li < mLangStarts.Size()) && (mLangStarts[(nk_size)li].Size() > 0);
				// Mode DECLARE, plus deduit d'un Find sur le contenu : voir mCfg.debutsDeBloc.
				const bool modeBlocs = (mCfg.debutsDeBloc < 0) ? blocsDispo : (mCfg.debutsDeBloc != 0);
				const bool auDebut = modeBlocs && blocsDispo && (rMode < 0.5);
				if (!auDebut)
					return (int64)(rUnif * (double)(N - mT));

				const NkVector<int64> &st = mLangStarts[(nk_size)li];
				nk_size k = (nk_size)(rBloc * (double)st.Size());
				if (k >= st.Size())
					k = st.Size() - 1;
				const int64 off = st[k];
				// La validation retire la QUEUE des donnees : un debut de bloc peut tomber
				// au-dela. Le rabattre ferait converger tous ces cas vers la MEME fenetre,
				// sur-representee en silence. On reprend le tirage uniforme deja consomme.
				if (off < 0 || off > maxOff)
					return (int64)(rUnif * (double)(N - mT));
				return off;
			}

			void NkGptTrainer::MakeBatch(NkTensor &x, NkTensor &oneHot) {
				MakeBatchFrom(mLangData, mLangMask, x, oneHot);
			}

			// Lot : x[B,T], cible one-hot [B*T, V] depuis `data`/`mask`. Tag de langue en tête ; round-robin.
			void NkGptTrainer::MakeBatchFrom(const NkVector<NkVector<float>> &data, const NkVector<NkVector<float>> &mask,
											 NkTensor &x, NkTensor &oneHot) {
				NkShape xs;
				xs.PushBack(mB);
				xs.PushBack(mT);
				x = NkTensor::Zeros(xs);
				oneHot = NkTensor::Zeros(NkShape{mB * mT, (int64)mV});
				float *xp = x.DataAs<float>();
				float *op = oneHot.DataAs<float>();
				const int nL = (int)data.Size();
				for (int64 b = 0; b < mB; ++b) {
					const int li = nL > 0 ? (int)(b % nL) : 0;
					const NkVector<float> &dd = data[(nk_size)li];
					const bool hasMask = ((nk_size)li < mask.Size()) && (mask[(nk_size)li].Size() == dd.Size());
					const int64 N = (int64)dd.Size();
					if (N <= mT)
						continue;
					const int64 off = ChoisirDecalage(li, N);
					xp[b * mT + 0] = (float)(mNByte + li);
					if (!hasMask || mask[(nk_size)li][(nk_size)off] != 0.f)
						op[(b * mT + 0) * mV + (int)dd[(nk_size)off]] = 1.f;
					for (int64 t = 1; t < mT; ++t) {
						xp[b * mT + t] = dd[(nk_size)(off + t - 1)];
						if (!hasMask || mask[(nk_size)li][(nk_size)(off + t)] != 0.f)
							op[(b * mT + t) * mV + (int)dd[(nk_size)(off + t)]] = 1.f;
					}
				}
			}

			// Lot d'entraînement à cible INDICES (depuis mLangData).
			void NkGptTrainer::MakeBatchIdx(NkTensor &x, NkTensor &targetIdx) {
				MakeBatchIdxFrom(mLangData, mLangMask, x, targetIdx);
			}

			// Lot : x[B,T] + cible targetIdx[B*T] (id de la classe par position ; -1 = masquée/inactive).
			// Même échantillonnage que MakeBatchFrom, mais sans matérialiser le one-hot [B*T,V].
			void NkGptTrainer::MakeBatchIdxFrom(const NkVector<NkVector<float>> &data,
												const NkVector<NkVector<float>> &mask, NkTensor &x,
												NkTensor &targetIdx) {
				NkShape xs;
				xs.PushBack(mB);
				xs.PushBack(mT);
				x = NkTensor::Zeros(xs);
				targetIdx = NkTensor::Full(NkShape{mB * mT}, -1.0); // -1 par défaut = masquée
				float *xp = x.DataAs<float>();
				float *tp = targetIdx.DataAs<float>();
				const int nL = (int)data.Size();
				for (int64 b = 0; b < mB; ++b) {
					const int li = nL > 0 ? (int)(b % nL) : 0;
					const NkVector<float> &dd = data[(nk_size)li];
					const bool hasMask = ((nk_size)li < mask.Size()) && (mask[(nk_size)li].Size() == dd.Size());
					const int64 N = (int64)dd.Size();
					if (N <= mT)
						continue; // ligne entièrement masquée (targetIdx reste -1)
					const int64 off = ChoisirDecalage(li, N);
					xp[b * mT + 0] = (float)(mNByte + li);
					if (!hasMask || mask[(nk_size)li][(nk_size)off] != 0.f)
						tp[b * mT + 0] = dd[(nk_size)off];
					for (int64 t = 1; t < mT; ++t) {
						xp[b * mT + t] = dd[(nk_size)(off + t - 1)];
						if (!hasMask || mask[(nk_size)li][(nk_size)(off + t)] != 0.f)
							tp[b * mT + t] = dd[(nk_size)(off + t)];
					}
				}
			}

			// Perte moyenne sur le held-out (forward seul, aucun gradient). -1 si pas de val.
			//
			// Parcourt TOUT le jeu fige, a chaque appel. Pas de parametre « nombre de
			// lots » : evaluer tantot 4 lots tantot 8 ferait varier la mesure pour une
			// raison etrangere au modele. La quantite se regle une fois, a la
			// construction du jeu (--valfenetres).
			double NkGptTrainer::EvaluateVal() {
				if (mValData.Size() == 0 || mValFenetres.Size() == 0)
					return -1.0;
				double sum = 0.0;
				int cnt = 0;
				const int nLots = (int)((mValFenetres.Size() + (nk_size)mB - 1) / (nk_size)mB);
				for (int i = 0; i < nLots; ++i) {
					NkTensor x, tgt;
					MakeBatchValFige(i, x, tgt);
					NkVar logits = mLlama ? mLlama->Forward(mUseGpu ? x.ToGPU() : x)
                                     : mGpt->Forward(mUseGpu ? x.ToGPU() : x);
					NkVar loss = autograd::SoftmaxCrossEntropyIndexed(logits, NkVar::Leaf(tgt, false));
					sum += loss.Value().ToCPU().GetItem(NkShape{(int64)0});
					++cnt;
				}
				return cnt > 0 ? sum / (double)cnt : -1.0;
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
					// Le checkpoint « NKGP » ne transporte PAS le mode de pré-tokenisation
					// (format antérieur à son existence). Un modèle entraîné avec un
					// découpage mots/ponctuation et rechargé sans cette information
					// encoderait ses invites autrement qu'à l'entraînement — sans erreur
					// visible, juste des réponses incohérentes. Le fichier tokenizer, lui,
					// le porte : quand il est fourni, il fait autorité.
					if (!mCfg.bpePath.Empty()) {
						Bpe fromFile;
						if (!data::LoadBpe(mCfg.bpePath.CStr(), fromFile)) {
							logger.Info("Tokenizer illisible : {0}", mCfg.bpePath.CStr());
							return false;
						}
						if (fromFile.merges.Size() != mBpe.merges.Size()) {
							logger.Info("Tokenizer et checkpoint incompatibles : {0} fusions contre {1}.",
										(unsigned long long)fromFile.merges.Size(),
										(unsigned long long)mBpe.merges.Size());
							return false;
						}
						mBpe.pretok = fromFile.pretok;
						if (V)
							logger.Info("Mode de pre-tokenisation repris du tokenizer : {0}.", mBpe.pretok);
					}
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
					if (!mCfg.bpePath.Empty()) {
						// Tokenizer pré-entraîné : on ne le ré-apprend pas. C'est ce qui rend
						// praticable un vocabulaire de 16 k et, surtout, ce qui garantit qu'un
						// entraînement repris plus tard découpe le texte EXACTEMENT pareil.
						if (!data::LoadBpe(mCfg.bpePath.CStr(), mBpe)) {
							logger.Info("Tokenizer illisible : {0}", mCfg.bpePath.CStr());
							return false;
						}
						if (V)
							logger.Info("Tokenizer charge : {0} ({1} fusions, mode pretok {2}).", mCfg.bpePath.CStr(),
										(unsigned long long)mBpe.merges.Size(), mBpe.pretok);
					} else {
						if (V)
							logger.Info("Entraînement du tokenizer BPE ({0} fusions cible)...", mCfg.merges);
						TrainBpe(texts, mCfg.merges, mBpe);
					}
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
						// Le message d'origine disait QUE ça ne correspondait pas, jamais
						// QUOI — et ces tags viennent du NOM DU FICHIER, pas de son
						// contenu. Un corpus français rebaptisé se voyait donc refusé
						// sans qu'on puisse deviner pourquoi (une demi-heure perdue le
						// 2026-08-10). Dire les deux listes rend la correction évidente.
						NkString attendus;
						for (nk_size i = 0; i < mLangs.Size(); ++i) {
							if (i)
								attendus.Append(", ");
							attendus.Append(mLangs[i]);
						}
						NkString trouves;
						for (nk_size i = 0; i < langs2.Size(); ++i) {
							if (i)
								trouves.Append(", ");
							trouves.Append(langs2[i]);
						}
						logger.Info("Reprise IMPOSSIBLE : le checkpoint attend les tags [{0}], le corpus fourni "
									"donne [{1}].",
									attendus.CStr(), trouves.CStr());
						logger.Info("Ces tags sont deduits du NOM DU FICHIER, pas de son contenu : renommer le "
									"corpus pour qu'il porte le meme prefixe (par exemple fr_...) suffit "
									"generalement. Repartir de zero sans --load est l'autre issue.");
						return false;
					}
					const nk_size totalTok = EncodeCorpus(texts);
					if (V)
						logger.Info("Reprise : corpus ré-encodé avec le BPE du checkpoint ({0} tokens BPE, {1} tags).",
									(unsigned long long)totalTok, (int)mLangs.Size());
				}

				// Le regime d'echantillonnage est DIT, toujours. Un regime deduit qu'on
				// ne journalise pas se decouvre en comparant deux courses incomparables.
				if (V) {
					nk_size blocs = 0;
					for (nk_size i = 0; i < mLangStarts.Size(); ++i)
						blocs += mLangStarts[i].Size();
					const bool actif = (mCfg.debutsDeBloc < 0) ? (blocs > 0) : (mCfg.debutsDeBloc != 0);
					logger.Info("Echantillonnage : {0} ({1} debuts de bloc recenses) — reglage `debutsDeBloc` = {2}.",
								actif ? "1 fenetre sur 2 demarre sur un debut de bloc" : "decalage UNIFORME pur",
								(unsigned long long)blocs, mCfg.debutsDeBloc);
				}

				// Jeu de validation fige : ICI, et pas dans EncodeCorpus, parce que la
				// taille de fenetre mT n'est connue qu'apres. Place a l'interieur
				// d'EncodeCorpus, la construction sortait sans rien faire et SANS RIEN
				// DIRE — le seul indice etait une ligne de journal absente.
				ConstruireFenetresVal();
				if (mValData.Size() > 0 && mValFenetres.Size() == 0) {
					// Une validation demandee qui n'evalue rien renverrait -1 a chaque
					// appel, et une courbe vide se lit comme « pas encore mesure » plutot
					// que comme une panne. On le dit donc franchement.
					logger.Info("ATTENTION : validation demandee mais AUCUNE fenetre construite (held-out trop "
								"court pour T={0} ?). Les pertes de validation seront absentes.",
								(long long)mT);
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
				//
				// Exactement UN des deux modèles est construit. NkLlamaLM ne prend pas
				// `mT` : sans table de positions, il n'a aucune longueur maximale
				// inscrite dans ses poids — c'est précisément ce qui le rend
				// extensible après coup.
				if (mCfg.architectureLlama) {
					mLlama = new nn::NkLlamaLM((uint32)mV, (uint32)mD, (uint32)mH, (uint32)mL,
											   mCfg.seedPoids, mCfg.weightTying);
					mLlama->Parameters(mParams);
					if (V)
						logger.Info("Architecture : NkLlamaLM (RMSNorm + RoPE + SwiGLU), graine {0}, "
									"tables liees = {1}",
									(long long)mCfg.seedPoids, mCfg.weightTying ? "OUI" : "non");
				} else {
					mGpt = new nn::NkGPT((uint32)mV, (uint32)mD, (uint32)mH, (uint32)mL, (uint32)mT,
										 mCfg.seedPoids);
					mGpt->Parameters(mParams);
					if (V)
						logger.Info("Architecture : NkGPT (LayerNorm + positions apprises + GELU), graine {0}",
									(long long)mCfg.seedPoids);
				}
				// Compte RÉEL des paramètres, depuis mParams — pas la formule théorique
				// calculée en amont par l'appelant, qui ignore le weight tying et
				// afficherait le même chiffre dans les deux cas. C'est ce compte-ci qui
				// dit si les tables sont VRAIMENT liées.
				//
				// Il attrape aussi l'erreur inverse : si mTokEmb était poussé DEUX FOIS
				// dans la liste, le total le montrerait (et l'optimiseur l'avancerait au
				// double du taux demandé, ce qui se lirait comme « le tying dégrade »).
				if (V) {
					int64 total = 0;
					for (nk_size i = 0; i < mParams.Size(); ++i)
						total += mParams[i].Value().Numel();
					logger.Info("Paramètres RÉELS : {0} tenseurs, {1} valeurs ({2} Mo de poids).",
								(unsigned long long)mParams.Size(), (long long)total,
								(double)total * 4.0 / 1048576.0);
				}

				if (!mCfg.loadPath.Empty()) {
					if (!LoadCheckpointWeights(mCfg.loadPath.CStr(), mParams)) {
						logger.Info("Poids du checkpoint incompatibles avec les dims.");
						return false;
					}
					if (V)
						logger.Info("Poids rechargés ({0} tenseurs).", mParams.Size());
				}
				// Reprise : tente de restaurer l'état optimiseur (moments Adam + pas). Absent (v3) =>
				// reprise des poids seuls (le warmup redémarrera). Moments gardés sur CPU jusqu'à Fit().
				if (mCfg.resume && !mCfg.loadPath.Empty()) {
					if (LoadCheckpointOptState(mCfg.loadPath.CStr(), mOptM, mOptV, mResumeStep)) {
						mHasOptState = true;
						if (V)
							logger.Info("État optimiseur repris : pas {0}, {1} moments (reprise parfaite).",
										(long long)mResumeStep, (unsigned long long)mOptM.Size());
					} else if (V)
						logger.Info("Checkpoint sans état optimiseur (v3) : reprise des poids seuls.");
				}
				if (mUseGpu)
					for (uint32 i = 0; i < mParams.Size(); ++i)
						mParams[i].SetValue(mParams[i].Value().ToGPU());
				return true;
			}

			NkString NkGptTrainer::Generate(const NkString &sd, int nToks, double temp, int langIdx) {
				NkSampleParams sp;
				sp.temperature = temp; // compat : température seule (top-k/top-p désactivés)
				return Generate(sd, nToks, sp, langIdx);
			}

			NkString NkGptTrainer::Generate(const NkString &sd, int nToks, const NkSampleParams &sp, int langIdx) {
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

				// Chemin RAPIDE (KV-cache) : possible si tout le décodage tient dans la fenêtre T
				// (positions absolues valides, math identique à Forward sur le préfixe complet).
				// ⚠️ NkLlamaLM n'a PAS de décodage incrémental : le chemin à cache
				// clé/valeur lui est fermé, il retraite tout le contexte à chaque
				// token. Correct mais coûteux — et c'est un manque ASSUMÉ tant que
				// l'architecture n'est pas retenue : écrire `ForwardStep` avec RoPE
				// demande de passer l'index ABSOLU du token, et un index faux ne
				// planterait pas, il dégraderait en silence.
				const bool canCache = !mUseGpu && !mLlama && ((int64)ctx.Size() + (int64)nToks) <= mT;
				if (canCache) {
					nn::NkKVCache cache;
					cache.Reset((uint32)mL);
					NkVar last;
					// Amorce : passe chaque token du contexte (le dernier logits sert au 1er tirage).
					for (nk_size i = 0; i < ctx.Size(); ++i) {
						NkTensor tok = NkTensor::Full(NkShape{(int64)1, (int64)1}, (double)ctx[i]);
						last = mGpt->ForwardStep(tok, cache);
					}
					for (int i = 0; i < nToks; ++i) {
						NkTensor lc = last.Value().ToCPU().Contiguous();
						const int next = NkSampleToken(lc.DataAs<float>(), mNByte, sp, mRng);
						ctx.PushBack((int32)next);
						out.Append(mBpe.Decode(next));
						NkTensor tok = NkTensor::Full(NkShape{(int64)1, (int64)1}, (double)next);
						last = mGpt->ForwardStep(tok, cache);
					}
					return out;
				}

				// Chemin GÉNÉRIQUE (fenêtre glissante) : recalcule le préfixe (tronqué à T) à chaque
				// pas. Nécessaire pour GPU ou pour un décodage plus long que la fenêtre.
				for (int i = 0; i < nToks; ++i) {
					int64 len = (int64)ctx.Size();
					if (len > mT)
						len = mT;
					NkTensor tok = NkTensor::Zeros(NkShape{(int64)1, len});
					float *tp = tok.DataAs<float>();
					for (int64 t = 0; t < len; ++t)
						tp[t] = (float)ctx[(nk_size)((int64)ctx.Size() - len + t)];
					NkVar logits = mLlama ? mLlama->Forward(mUseGpu ? tok.ToGPU() : tok)
                                 : mGpt->Forward(mUseGpu ? tok.ToGPU() : tok);
					NkTensor lc = logits.Value().ToCPU().Contiguous();
					const float *lp = lc.DataAs<float>() + (len - 1) * mV;
					const int next = NkSampleToken(lp, mNByte, sp, mRng);
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

			void NkGptTrainer::FillMeta(GptMeta &meta) const {
				meta.V = mV;
				meta.d = (int32)mD;
				meta.H = (int32)mH;
				meta.L = (int32)mL;
				meta.T = (int32)mT;
				meta.langs = mLangs;
				for (int64 i = 0; i < (int64)mBpe.merges.Size(); ++i)
					meta.merges.PushBack(mBpe.merges[(nk_size)i]);
			}

			bool NkGptTrainer::Save(const char *path) {
				GptMeta meta;
				FillMeta(meta);
				return SaveCheckpoint(path, meta, mParams);
			}

			void NkGptTrainer::Fit() {
				if (!mGpt && !mLlama)
					return;
				const bool V = mCfg.verbose;
				int STEPS = mCfg.steps; // ajusté plus bas si un horizon absolu est donné
				const int ACCUM = mCfg.accum;
				const float peakLr = mCfg.lr;
				const int WARMUP = (mCfg.warmup >= 0) ? mCfg.warmup : (STEPS / 20);
				const double kPi = 3.14159265358979323846;
				const float minLrRatio = 0.1f;
				const int SAVEEVERY = mCfg.saveEvery;
				const bool hasSave = !mCfg.savePath.Empty();

				optim::NkAdam adam(mParams, peakLr, 0.9f, 0.999f, 1e-8f, /*weightDecay=AdamW*/ 0.01f);

				// Reprise parfaite : restaure les moments Adam + le compteur de pas => pas de warmup ni de
				// pic de perte, le schedule reprend là où il s'était arrêté. `base` = pas déjà effectués.
				int64 base = 0;
				if (mHasOptState) {
					adam.SetMoments(mOptM, mOptV);
					adam.SetStepCount(mResumeStep);
					// `base` ne sert QU'AU CALENDRIER du pas d'apprentissage. Le
					// laisser à zéro pour une nouvelle phase redonne le warmup et le
					// pic demandé, sans rien perdre des moments d'Adam.
					base = mCfg.freshSchedule ? 0 : mResumeStep;
					mOptM.Clear();
					mOptV.Clear();
				}

				// Horizon ABSOLU : c'est ICI, et nulle part ailleurs, qu'on peut le
				// convertir en nombre de pas — `base` vient d'être lu du checkpoint,
				// seule source qui survive à une coupure de courant. Un relanceur
				// extérieur, lui, devrait deviner.
				if (mCfg.horizon > 0) {
					const int64 restant = mCfg.horizon - base;
					STEPS = (restant > 0) ? (int)restant : 0;
					if (V) {
						if (STEPS > 0)
							logger.Info("   Horizon absolu {0} : {1} pas restants (deja effectues : {2}).",
										(long long)mCfg.horizon, (long long)STEPS, (long long)base);
						else
							logger.Info("   Horizon absolu {0} DEJA ATTEINT (pas global {1}) : rien a faire.",
										(long long)mCfg.horizon, (long long)base);
					}
					if (STEPS == 0)
						return; // idempotent : relancer un run fini ne le prolonge pas
				}
				const int64 totalHorizon = base + (int64)STEPS;

				// Métadonnées de sauvegarde (dims + BPE + langues) : construites une seule fois.
				GptMeta saveMeta;
				FillMeta(saveMeta);

				if (V) {
					logger.Info("-- Entraînement ({0} pas) --", STEPS);
					if (base > 0)
						logger.Info("   Reprise au pas global {0} (schedule continué, sans warmup).", (long long)base);
					else if (mHasOptState)
						logger.Info("   NOUVELLE PHASE : poids et moments repris, calendrier d'apprentissage NEUF "
									"(warmup {0} pas -> pic {1}).",
									WARMUP, (double)peakLr);
					if (ACCUM > 1)
						logger.Info("   Accumulation de gradient : {0} micro-lots -> batch effectif = {1}", ACCUM,
									(long long)(mB * ACCUM));
					logger.Info("   LR schedule : warmup {0} pas -> pic {1} -> cosine (plancher {2}%) ; checkpoint "
								"tous les {3} pas",
								WARMUP, (double)peakLr, (double)(minLrRatio * 100), SAVEEVERY);
				}
				mEma = 0;
				NkChrono chrono;
				// Fenetre de profil : ARMEE APRES LE PREMIER PAS. Le pas 1 compile les
				// noyaux (NkSL -> SPIR-V -> pipeline) et paie la relecture de controle
				// « sortie non nulle » de chaque noyau : l'y inclure attribuerait un
				// cout de demarrage a des operations qui ne le paient qu'une fois.
				NkChrono chronoProfil;
				int64 pasProfiles = 0;
				for (int s = 1; s <= STEPS; ++s) {
					const int64 g = base + (int64)s; // pas global (pour le schedule)
					float lr;
					if (WARMUP > 0 && g <= (int64)WARMUP)
						lr = peakLr * (float)g / (float)WARMUP;
					else {
						const double prog =
							(totalHorizon > WARMUP) ? (double)(g - WARMUP) / (double)(totalHorizon - WARMUP) : 1.0;
						const double cosv = 0.5 * (1.0 + NkCos(kPi * prog));
						lr = (float)(peakLr * (minLrRatio + (1.0 - minLrRatio) * cosv));
					}
					adam.SetLearningRate(lr);
					adam.ZeroGrad();
					double lv = 0.0;
					// ⚠️ NE JAMAIS DIVISER PAR UNE CONSTANTE. La version precedente faisait
					// `lv += perte / ACCUM` : un micro-lot SANS position contributive rend une
					// perte de 0, et ce 0 entrait dans la moyenne comme une mesure valide.
					//
					// Constate le 14 aout 2026 : un run a rendu 2,61397 la ou son jumeau (meme
					// config, meme graine) rendait 10,4482. Rapport 0,250184 — un QUART a
					// 0,07 % pres : UN seul micro-lot sur quatre avait somme quelque chose, et
					// on divisait quand meme par quatre. Au pas suivant, ZERO micro-lot, donc
					// perte nulle. C'est de l'arithmetique, pas du stochastique.
					//
					// Le danger depasse ce cas : a un quart de remplissage la perte tombe a
					// 2,61 et saute aux yeux, mais a neuf dixiemes elle baisse de 10 % — et une
					// perte 10 % plus basse ne ressemble pas a un defaut, elle ressemble a un
					// PROGRES. Un run peut donc etre contamine pendant des milliers de pas en
					// presentant une courbe meilleure.
					//
					// Moyenne ponderee par les positions reellement sommees : elle redonne la
					// vraie perte moyenne, quel que soit le remplissage des micro-lots.
					double sommePonderee = 0.0;
					int64 posTotal = 0;
					int microVides = 0;
					for (int m = 0; m < ACCUM; ++m) {
						NkTensor x, tgt;
						MakeBatchIdx(x, tgt); // cible = indices [B*T] (pas de one-hot dense)
						NkVar logits = mLlama ? mLlama->Forward(mUseGpu ? x.ToGPU() : x)
                                     : mGpt->Forward(mUseGpu ? x.ToGPU() : x);
						// tgt reste sur CPU (minuscule ; le forward et le backward la lisent en CPU).
						NkVar loss = autograd::SoftmaxCrossEntropyIndexed(logits, NkVar::Leaf(tgt, false));
						NkVar scaled = (ACCUM > 1) ? autograd::MulScalar(loss, 1.0 / (double)ACCUM) : loss;
						scaled.Backward();

						// Positions contributives comptees sur la CIBLE elle-meme : independant
						// du chemin (GPU ou repli CPU) et de la structure interne du noeud.
						int64 actifs = 0;
						{
							NkTensor tc = tgt.Contiguous();
							const float *tp = tc.DataAs<float>();
							for (int64 i = 0; i < tc.Numel(); ++i)
								if (tp[i] >= 0.f)
									++actifs;
						}
						if (actifs == 0)
							++microVides;
						sommePonderee += loss.Value().ToCPU().GetItem(NkShape{(int64)0}) * (double)actifs;
						posTotal += actifs;
					}
					lv = (posTotal > 0) ? sommePonderee / (double)posTotal : 0.0;

					// ---- FILET 0 : le lot etait-il seulement REMPLI ? ----
					// Journalise TOUJOURS, pas seulement en cas de panne : un compteur qu'on
					// ne lit qu'apres coup ne previent de rien.
					{
						const int64 attendu = (int64)ACCUM * mB * mT;
						const double taux = (attendu > 0) ? (double)posTotal / (double)attendu : 0.0;
						if (s <= 3 || microVides > 0 || taux < 0.999)
							logger.Info("   [lot] pas {0} : {1} positions sommees sur {2} attendues "
										"({3}%), micro-lots vides = {4}",
										(long long)s, (long long)posTotal, (long long)attendu, taux * 100.0,
										microVides);
						if (microVides > 0) {
							logger.Info("*** ARRET au pas {0} : {1} micro-lot(s) sur {2} n'ont somme AUCUNE "
										"position. La perte affichee serait divisee d'autant sans que rien "
										"ne le signale. ***",
										(long long)s, microVides, ACCUM);
							break;
						}
					}

					// ---- FILET 0bis : la perte INITIALE n'a qu'une valeur licite ----
					// Un modele fraichement initialise ne sait rien : il repartit sa masse
					// uniformement sur le vocabulaire, donc -log(1/V) = ln(V). Tout ecart
					// notable au premier pas denonce un lot mal rempli, une cible decalee ou
					// une initialisation hors norme — AVANT que des heures ne soient depensees.
					if (s == 1) {
						const double attendue = math::NkLog((double)mV);
						// ⚠️ SEUIL ASYMETRIQUE, ET C'EST MESURE, PAS SUPPOSE.
						//
						// Premiere version : |perte - ln(V)| < 0,05, symetrique. Elle a arrete
						// CINQ runs SAINS sur cinq (14 aout 2026). L'exces au-dessus de ln(V)
						// est en effet STRUCTUREL et croit avec la largeur : +0,008 a d=384,
						// +0,024 a d=512, +0,051 a d=640 — la table d'embedding n'est pas nulle
						// a l'initialisation, donc les logits ne le sont pas non plus et la
						// perte depasse legerement l'uniforme. Un seuil symetrique arretait donc
						// precisement les configurations les plus larges, celles qu'on mesure.
						//
						// Les deux cotes n'ont PAS le meme statut :
						//  - EN DESSOUS de ln(V) : IMPOSSIBLE. On ne fait pas mieux que
						//    l'uniforme sans avoir appris. Le run fautif etait a -7,78.
						//  - AU-DESSUS : anomalie d'initialisation, tolerable jusqu'a un point.
						const double margeBasse = 0.02; // sous ln(V) : impossibilite
						const double margeHaute = 0.50; // au-dessus : init hors norme
						if (lv < attendue - margeBasse) {
							logger.Info("*** ARRET au pas 1 : perte = {0}, INFERIEURE a ln(V) = {1}. Un modele "
										"non entraine ne peut PAS faire mieux que l'uniforme : le lot est mal "
										"rempli, ou des lignes n'ont pas ete calculees. ***",
										lv, attendue);
							break;
						}
						if (lv > attendue + margeHaute) {
							logger.Info("*** ARRET au pas 1 : perte = {0}, trop au-dessus de ln(V) = {1} "
										"(marge {2}). Initialisation hors norme. ***",
										lv, attendue, margeHaute);
							break;
						}
						logger.Info("   [init] perte au pas 1 = {0}, ln(V) = {1}, exces {2} : conforme.", lv,
									attendue, lv - attendue);
					}
					adam.Step();
					mEma = (s == 1) ? lv : 0.98 * mEma + 0.02 * lv;
					if (s == 1) {
						NkTensorGpu::ProfilRaz(true);
						chronoProfil.Reset();
						pasProfiles = 0;
					} else
						++pasProfiles;

					// ---- FILET 1 : la perte est-elle seulement UNE PERTE ? ----
					// L'entropie croisée vaut -log(p) avec p strictement inférieur à 1 :
					// elle est STRICTEMENT POSITIVE par construction. Zéro, une valeur
					// négative ou un NaN ne sont donc pas des pertes basses, ce sont des
					// impossibilités — la marque d'un calcul qui n'a pas eu lieu.
					//
					// POURQUOI CE FILET EXISTE EN PLUS DE CELUI D'EN DESSOUS. Mesuré le
					// 2026-08-10 à B=24 : la perte part correctement de 9,71785 (soit
					// ln(16385), la valeur du hasard), puis tombe à 0 EXACTEMENT au 25e
					// pas. Le contrôle des 30 pas ci-dessous a alors SALUÉ cette chute
					// comme « une baisse de 100 %, l'entrainement calcule reellement »,
					// et le run a paru 2,5 fois plus rapide que la normale. Un garde-fou
					// qui rassure à tort est pire que pas de garde-fou du tout : il fait
					// tourner des heures dans le vide en affichant que tout va bien.
					if (!(lv > 0.0) || lv != lv || lv > 1e30) {
						logger.Info("*** ARRET au pas {0} : perte = {1}, ce qui est IMPOSSIBLE. ***", (long long)s,
									lv);
						logger.Info("*** Une entropie croisee est strictement positive ; zero, negatif ou NaN "
									"signifie que le calcul GPU ne produit plus rien. Defauts GPU signales : "
									"{0}. Reduire --B (et augmenter --accum a lot effectif egal). ***",
									(long long)NkTensorGpu::DefautCount());
						break;
					}

					// ---- FILET 2 : est-ce que ça apprend, vraiment ? ----
					// Un calcul GPU qui échoue en silence (allocation refusée, lot trop
					// grand) laisse la perte EXACTEMENT à sa valeur initiale pendant que
					// le run paraît plus rapide que jamais — constaté le 2026-08-09, et
					// rien dans le journal ne le disait. Une perte qui n'a pas bougé
					// d'un millième après 30 pas n'est pas un entraînement lent : c'est
					// un entraînement qui n'a pas lieu. On arrête plutôt que de brasser
					// du vide pendant des heures.
					if (s == 1)
						mPerteInitiale = lv;
					if (s == 30 && mPerteInitiale > 0.0) {
						// ⚠️ LA VALEUR ABSOLUE, et non l'écart signé.
						//
						// Ce filet cherche une perte FIGÉE — le symptôme d'un calcul qui
						// n'a pas lieu. Une perte qui MONTE est un mouvement : le calcul
						// se fait, et une hausse au début d'une phase nouvelle, pendant
						// la montée en puissance du pas d'apprentissage, est normale.
						//
						// Mesuré le 2026-08-10 : une phase legitime est morte au pas 30
						// sur « la perte n'a pas bouge (3,531 -> 3,97963, soit -12,7 %) ».
						// L'ecart signé était négatif, donc inférieur au seuil — et le
						// run a été tué pour avoir trop appris dans le mauvais sens. Un
						// garde-fou qui tue ce qu'il devait protéger est pire qu'aucun.
						const double ecart = (mPerteInitiale - lv) / mPerteInitiale;
						const double bouge = (ecart < 0.0) ? -ecart : ecart;
						const int64 defauts = NkTensorGpu::DefautCount();
						if (bouge < 0.001 || defauts > 0) {
							logger.Info("*** ARRET : apres 30 pas la perte n'a pas bouge ({0} -> {1}, soit {2}%). "
										"Defauts GPU signales : {3}. ***",
										mPerteInitiale, lv, bouge * 100.0, (long long)defauts);
							logger.Info("*** Le calcul n'a tres probablement PAS lieu. Causes connues : lot trop "
										"grand pour la carte (essayer --B plus petit avec --accum plus grand a lot "
										"effectif egal), ou memoire video insuffisante. ***");
							break;
						}
						if (V)
							logger.Info("  [controle] la perte a BOUGE de {0}% en 30 pas ({1} -> {2}) — "
										"l'entrainement calcule reellement.",
										ecart * 100.0, mPerteInitiale, lv);
						if (V)
							logger.Info("  [mesure] {0} operations GPU en 30 pas = {1} par pas — a {2} s/pas, "
										"le cout fixe par operation vaut {3} ms.",
										(long long)NkTensorGpu::OpCount(), (long long)(NkTensorGpu::OpCount() / 30),
										chrono.Elapsed().seconds / 30.0,
										(NkTensorGpu::OpCount() > 0)
											? (chrono.Elapsed().seconds * 1000.0) / (double)NkTensorGpu::OpCount()
											: 0.0);
					}
					if (V && (s % 25 == 0 || s == 1))
						logger.Info("  pas {0} : perte = {1}  (moy. {2})  lr={3}", s, lv, mEma, (double)lr);
					// Pic de VRAM releve APRES quelques pas complets : le premier pas
					// n'a pas encore materialise l'etat de l'optimiseur (moments Adam),
					// donc un releve precoce sous-estime l'occupation reelle — et
					// sous-estimer la memoire est la facon la plus couteuse de se
					// tromper (la configuration ne tient pas, on le decouvre tard).
					if (s == 3 && mCfg.verbose) {
						const double pic = (double)NkTensorGpu::VramPic() / 1048576.0;
						const double picCalc = (double)NkTensorGpu::VramPicCalcul() / 1048576.0;
						const double viv = (double)NkTensorGpu::VramVivante() / 1048576.0;
						logger.Info("VRAM suivie apres {0} pas : PIC physique {1} Mo (calcul seul {2} Mo), "
									"vivante {3} Mo. (nos tampons SEULEMENT : ni pilote, ni fragmentation "
									"— plancher de l'occupation reelle, pas son total ; le pic physique "
									"compte ce que la reserve retient.)",
									(long long)s, pic, picCalc, viv);
					}

					if (mCfg.valEvery > 0 && mValData.Size() > 0 && s % mCfg.valEvery == 0) {
						const double vl = EvaluateVal();
						if (V && vl >= 0.0)
							logger.Info("  [val] pas {0} : perte val = {1}  (train {2})", s, vl, mEma);
					}
					if (V && mCfg.sampleEvery > 0 && s % mCfg.sampleEvery == 0) {
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
						const bool sv = SaveCheckpoint(mCfg.savePath.CStr(), saveMeta, mParams, &adam.FirstMoments(),
													   &adam.SecondMoments(), adam.StepCount());
						if (sv && V)
							logger.Info("  [checkpoint pas {0} (global {1}) -> {2}]", s, (long long)g,
										mCfg.savePath.CStr());
					}
				}
				if (V)
					logger.Info("Entraînement terminé en {0} s ({1}).", chrono.Elapsed().seconds,
								mUseGpu ? "GPU-résident" : "CPU");
				if (V && mUseGpu && pasProfiles > 0)
					NkTensorGpu::ProfilRapport(chronoProfil.Elapsed().seconds, pasProfiles);

				if (mValData.Size() > 0) {
					const double vl = EvaluateVal();
					if (V && vl >= 0.0)
						logger.Info("Perte VALIDATION finale (held-out) : {0}  (train moy. {1}) — écart = "
									"généralisation vs mémorisation.",
									vl, mEma);
				}

				if (hasSave) {
					const bool sv = SaveCheckpoint(mCfg.savePath.CStr(), saveMeta, mParams, &adam.FirstMoments(),
												   &adam.SecondMoments(), adam.StepCount());
					if (sv) {
						if (V)
							logger.Info("Modèle sauvegardé (avec état optimiseur, pas global {0}) : {1}",
										(long long)adam.StepCount(), mCfg.savePath.CStr());
					} else
						logger.Info("Échec de la sauvegarde : {0}", mCfg.savePath.CStr());
				}
			}

		} // namespace gpt
	} // namespace ai
} // namespace nkentseu
