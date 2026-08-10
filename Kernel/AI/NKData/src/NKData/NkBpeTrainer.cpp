// =============================================================================
// NKData/NkBpeTrainer.cpp — implémentation de l'entraînement BPE à l'échelle.
// Voir NkBpeTrainer.h pour le POURQUOI (l'entraîneur historique est en
// O(fusions x octets) ; celui-ci travaille sur les mots uniques et tient ses
// comptes à jour au lieu de tout recalculer).
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKData/NkBpeTrainer.h"
#include "NKLogger/NkLog.h"

#include <cstdio> // FILE* — même choix que NKGpt/NKInfer pour l'I/O binaire

namespace nkentseu {
	namespace ai {
		namespace data {

			// =========================================================================
			// Outils communs
			// =========================================================================
			static const int64 kEmptyKey = (int64)0x8000000000000000LL;

			// Clé d'une paire ordonnée (a,b). 32 bits par identifiant : largement de quoi
			// loger un vocabulaire de 32 k (l'ancienne clé n'en réservait que 21).
			static inline int64 PairKey32(int32 a, int32 b) {
				return ((int64)a << 32) | (int64)(uint32)b;
			}

			static inline int32 PairA(int64 k) {
				return (int32)(k >> 32);
			}

			static inline int32 PairB(int64 k) {
				return (int32)(uint32)(k & 0xFFFFFFFFLL);
			}

			static inline uint64 MixHash(uint64 h) {
				h ^= h >> 33;
				h *= 0xFF51AFD7ED558CCDull;
				h ^= h >> 33;
				h *= 0xC4CEB9FE1A85EC53ull;
				h ^= h >> 33;
				return h;
			}

			static inline uint64 HashBytes(const char *p, int64 n) {
				uint64 h = 1469598103934665603ull; // FNV-1a
				for (int64 i = 0; i < n; ++i) {
					h ^= (uint64)(unsigned char)p[i];
					h *= 1099511628211ull;
				}
				return MixHash(h);
			}

			// =========================================================================
			// Table des paires : clé -> (compte, tête de chaîne d'occurrences, marque)
			// -------------------------------------------------------------------------
			// Adressage ouvert avec sondage linéaire. Aucune suppression : une paire dont
			// le compte retombe à zéro reste dans la table avec la valeur 0 (elle ne peut
			// de toute façon plus réapparaître — cf. la note dans la boucle de fusion).
			// =========================================================================
			namespace {

				struct PairTable {
						NkVector<int64> key;
						NkVector<int64> cnt;
						NkVector<int32> head;  // index+1 dans le pool de nœuds ; 0 = chaîne vide
						NkVector<int32> stamp; // mot+1 déjà enregistré pour cette paire ; 0 = aucun
						NkVector<int32> dmark; // fusion+1 ayant déjà signalé cette paire comme modifiée
						int64 mask = 0;
						int64 used = 0;

						void Init(int64 pow2) {
							key.Clear();
							cnt.Clear();
							head.Clear();
							stamp.Clear();
							dmark.Clear();
							key.Reserve((nk_size)pow2);
							cnt.Reserve((nk_size)pow2);
							head.Reserve((nk_size)pow2);
							stamp.Reserve((nk_size)pow2);
							dmark.Reserve((nk_size)pow2);
							for (int64 i = 0; i < pow2; ++i) {
								key.PushBack(kEmptyKey);
								cnt.PushBack(0);
								head.PushBack(0);
								stamp.PushBack(0);
								dmark.PushBack(0);
							}
							mask = pow2 - 1;
							used = 0;
						}

						int64 Find(int64 k) const {
							int64 s = (int64)(MixHash((uint64)k) & (uint64)mask);
							while (key[(nk_size)s] != kEmptyKey) {
								if (key[(nk_size)s] == k)
									return s;
								s = (s + 1) & mask;
							}
							return -1;
						}

						// Trouve ou crée la case de `k`. Peut faire grossir la table : tout
						// index de case obtenu avant un appel à Insert doit être considéré
						// comme périmé.
						int64 Insert(int64 k) {
							if ((used + 1) * 10 > (mask + 1) * 7)
								Grow();
							int64 s = (int64)(MixHash((uint64)k) & (uint64)mask);
							while (key[(nk_size)s] != kEmptyKey) {
								if (key[(nk_size)s] == k)
									return s;
								s = (s + 1) & mask;
							}
							key[(nk_size)s] = k;
							cnt[(nk_size)s] = 0;
							head[(nk_size)s] = 0;
							stamp[(nk_size)s] = 0;
							dmark[(nk_size)s] = 0;
							++used;
							return s;
						}

						void Grow() {
							const int64 oldCap = mask + 1;
							NkVector<int64> ok = key;
							NkVector<int64> oc = cnt;
							NkVector<int32> oh = head;
							NkVector<int32> os = stamp;
							NkVector<int32> od = dmark;
							Init(oldCap * 2);
							for (int64 i = 0; i < oldCap; ++i) {
								if (ok[(nk_size)i] == kEmptyKey)
									continue;
								int64 s = (int64)(MixHash((uint64)ok[(nk_size)i]) & (uint64)mask);
								while (key[(nk_size)s] != kEmptyKey)
									s = (s + 1) & mask;
								key[(nk_size)s] = ok[(nk_size)i];
								cnt[(nk_size)s] = oc[(nk_size)i];
								head[(nk_size)s] = oh[(nk_size)i];
								stamp[(nk_size)s] = os[(nk_size)i];
								dmark[(nk_size)s] = od[(nk_size)i];
								++used;
							}
						}
				};

				// ---- Tas binaire max à invalidation paresseuse ---------------------------
				// On empile (clé, compte) à chaque hausse de compte. Une baisse n'enlève
				// rien : l'entrée périmée est simplement rejetée au dépilement, quand on
				// constate que le compte réel ne correspond plus. Départage déterministe :
				// à compte égal, la plus petite clé passe devant.
				static inline bool HeapBetter(int64 c1, int64 k1, int64 c2, int64 k2) {
					if (c1 != c2)
						return c1 > c2;
					return k1 < k2;
				}

				struct PairHeap {
						NkVector<int64> hk, hc;

						void Push(int64 k, int64 c) {
							hk.PushBack(k);
							hc.PushBack(c);
							int64 i = (int64)hk.Size() - 1;
							while (i > 0) {
								const int64 p = (i - 1) / 2;
								if (HeapBetter(hc[(nk_size)i], hk[(nk_size)i], hc[(nk_size)p], hk[(nk_size)p])) {
									const int64 tk = hk[(nk_size)i];
									const int64 tc = hc[(nk_size)i];
									hk[(nk_size)i] = hk[(nk_size)p];
									hc[(nk_size)i] = hc[(nk_size)p];
									hk[(nk_size)p] = tk;
									hc[(nk_size)p] = tc;
									i = p;
								} else
									break;
							}
						}

						bool Pop(int64 &k, int64 &c) {
							const int64 n = (int64)hk.Size();
							if (n == 0)
								return false;
							k = hk[(nk_size)0];
							c = hc[(nk_size)0];
							hk[(nk_size)0] = hk[(nk_size)(n - 1)];
							hc[(nk_size)0] = hc[(nk_size)(n - 1)];
							hk.Resize((nk_size)(n - 1));
							hc.Resize((nk_size)(n - 1));
							const int64 m = n - 1;
							int64 i = 0;
							for (;;) {
								const int64 l = 2 * i + 1, r = 2 * i + 2;
								int64 best = i;
								if (l < m && HeapBetter(hc[(nk_size)l], hk[(nk_size)l], hc[(nk_size)best], hk[(nk_size)best]))
									best = l;
								if (r < m && HeapBetter(hc[(nk_size)r], hk[(nk_size)r], hc[(nk_size)best], hk[(nk_size)best]))
									best = r;
								if (best == i)
									break;
								const int64 tk = hk[(nk_size)i];
								const int64 tc = hc[(nk_size)i];
								hk[(nk_size)i] = hk[(nk_size)best];
								hc[(nk_size)i] = hc[(nk_size)best];
								hk[(nk_size)best] = tk;
								hc[(nk_size)best] = tc;
								i = best;
							}
							return true;
						}
				};

				// ---- Table mot -> index (adressage ouvert) --------------------------------
				struct WordTable {
						NkVector<int64> hash; // empreinte du mot ; kEmptyKey = case libre
						NkVector<int32> idx;  // index dans le tableau des mots
						int64 mask = 0, used = 0;

						void Init(int64 pow2) {
							hash.Clear();
							idx.Clear();
							hash.Reserve((nk_size)pow2);
							idx.Reserve((nk_size)pow2);
							for (int64 i = 0; i < pow2; ++i) {
								hash.PushBack(kEmptyKey);
								idx.PushBack(-1);
							}
							mask = pow2 - 1;
							used = 0;
						}
				};

			} // namespace

			// Découpe le texte en tranches qui ne coupent AUCUN mot : on ne tranche
			// qu'après un '\n' suivi d'un caractère non blanc. Dans les deux modes de
			// pré-tokenisation un mot ne franchit jamais une telle frontière, donc
			// découper ou non donne rigoureusement les mêmes mots. Sans cela, le simple
			// fait de pré-tokeniser 25 Mo d'un coup construirait des millions de NkString
			// simultanées.
			static int64 SafeCut(const NkString &t, int64 target) {
				const char *p = t.Data();
				const int64 n = (int64)t.Size();
				if (target >= n)
					return n;
				int64 i = target;
				while (i < n) {
					if (p[i] == '\n' && i + 1 < n) {
						const unsigned char c = (unsigned char)p[i + 1];
						if (c != ' ' && c != '\n' && c != '\t' && c != '\r')
							return i + 1;
					}
					++i;
				}
				return n;
			}

			// =========================================================================
			// Entraînement
			// =========================================================================
			bool TrainBpeFast(const NkVector<NkString> &texts, const NkBpeTrainConfig &cfg, NkBpe &out,
							  NkBpeTrainStats *stats) {
				NkBpeTrainStats st;
				const int32 targetMerges = cfg.targetVocab - 256;
				if (targetMerges <= 0) {
					logger.Info("BPE : vocabulaire cible {0} <= 256, rien à apprendre.", cfg.targetVocab);
					return false;
				}

				// ---- 1. Mots uniques et leurs fréquences ------------------------------
				WordTable wt;
				wt.Init(1 << 16);
				NkVector<NkString> words;
				NkVector<int64> freq;

				for (int64 ti = 0; ti < (int64)texts.Size(); ++ti) {
					const NkString &txt = texts[(nk_size)ti];
					const int64 n = (int64)txt.Size();
					st.totalBytes += n;
					int64 pos = 0;
					while (pos < n) {
						const int64 end = SafeCut(txt, pos + (1 << 20));
						NkString chunk = txt.SubStr((nk_size)pos, (nk_size)(end - pos));
						pos = end;
						NkVector<NkString> chunkWords;
						NkBpe::PreTokMode(chunk, cfg.pretok, chunkWords);
						for (int64 wi = 0; wi < (int64)chunkWords.Size(); ++wi) {
							const NkString &w = chunkWords[(nk_size)wi];
							const int64 h = (int64)HashBytes(w.Data(), (int64)w.Size());
							// Insertion / recherche.
							if ((wt.used + 1) * 10 > (wt.mask + 1) * 7) {
								// Agrandissement : on réinsère depuis `words` (source de vérité).
								WordTable nt;
								nt.Init((wt.mask + 1) * 2);
								for (int64 k = 0; k < (int64)words.Size(); ++k) {
									const NkString &ww = words[(nk_size)k];
									const int64 hh = (int64)HashBytes(ww.Data(), (int64)ww.Size());
									int64 s = (int64)(MixHash((uint64)hh) & (uint64)nt.mask);
									while (nt.hash[(nk_size)s] != kEmptyKey)
										s = (s + 1) & nt.mask;
									nt.hash[(nk_size)s] = hh;
									nt.idx[(nk_size)s] = (int32)k;
									++nt.used;
								}
								wt = nt;
							}
							int64 s = (int64)(MixHash((uint64)h) & (uint64)wt.mask);
							int32 found = -1;
							while (wt.hash[(nk_size)s] != kEmptyKey) {
								if (wt.hash[(nk_size)s] == h) {
									const int32 wid = wt.idx[(nk_size)s];
									if (words[(nk_size)wid] == w) {
										found = wid;
										break;
									}
								}
								s = (s + 1) & wt.mask;
							}
							if (found >= 0)
								freq[(nk_size)found] += 1;
							else {
								wt.hash[(nk_size)s] = h;
								wt.idx[(nk_size)s] = (int32)words.Size();
								++wt.used;
								words.PushBack(w);
								freq.PushBack(1);
							}
							++st.wordOccur;
						}
					}
				}

				const int64 nWords = (int64)words.Size();
				st.uniqueWords = nWords;
				if (nWords == 0) {
					logger.Info("BPE : corpus vide.");
					return false;
				}

				// ---- 2. Mots -> suites de symboles, à plat ----------------------------
				// Chaque mot occupe une tranche de taille FIXE (sa longueur d'origine) ;
				// les fusions ne font que raccourcir la suite, elle est donc réécrite sur
				// place et `wLen` porte la longueur courante.
				NkVector<int32> sym;
				NkVector<int64> wOff;
				NkVector<int32> wLen;
				wOff.Reserve((nk_size)nWords);
				wLen.Reserve((nk_size)nWords);
				{
					int64 total = 0;
					for (int64 i = 0; i < nWords; ++i)
						total += (int64)words[(nk_size)i].Size();
					sym.Reserve((nk_size)total);
					st.initialSymbols = total;
				}
				for (int64 i = 0; i < nWords; ++i) {
					const NkString &w = words[(nk_size)i];
					wOff.PushBack((int64)sym.Size());
					const char *p = w.Data();
					const int64 n = (int64)w.Size();
					for (int64 k = 0; k < n; ++k)
						sym.PushBack((int32)(unsigned char)p[k]);
					wLen.PushBack((int32)n);
				}

				if (cfg.verbose)
					logger.Info("BPE : {0} octets -> {1} mots ({2} distincts), {3} symboles.",
								(long long)st.totalBytes, (long long)st.wordOccur, (long long)nWords,
								(long long)st.initialSymbols);

				// ---- 3. Comptes de paires + index paire -> mots ------------------------
				PairTable pt;
				{
					int64 cap = 1 << 16;
					while (cap < st.initialSymbols)
						cap <<= 1;
					pt.Init(cap);
				}
				PairHeap heap;
				NkVector<int32> nodeWord, nodeNext; // pool de chaînage
				const int64 maxNodes = st.initialSymbols * 2 + (1 << 21);

				// Enregistre `w` dans la chaîne de la paire de case `slot` (sans doublon
				// consécutif — le compactage se charge des doublons résiduels).
				auto Register = [&](int64 slot, int32 w) {
					if (pt.stamp[(nk_size)slot] == w + 1)
						return;
					pt.stamp[(nk_size)slot] = w + 1;
					nodeWord.PushBack(w);
					nodeNext.PushBack(pt.head[(nk_size)slot]);
					pt.head[(nk_size)slot] = (int32)nodeWord.Size(); // index+1
				};

				// Reconstruit l'index paire -> mots depuis zéro. Les chaînes ne sont jamais
				// purgées au fil de l'eau (un mot y reste inscrit même s'il a perdu la
				// paire, ce qui est sans effet sur le résultat mais consomme de la
				// mémoire) : ce compactage périodique borne cette consommation.
				auto RebuildChains = [&]() {
					for (int64 i = 0; i <= pt.mask; ++i) {
						pt.head[(nk_size)i] = 0;
						pt.stamp[(nk_size)i] = 0;
					}
					nodeWord.Clear();
					nodeNext.Clear();
					for (int64 w = 0; w < nWords; ++w) {
						const int64 off = wOff[(nk_size)w];
						const int32 len = wLen[(nk_size)w];
						for (int32 i = 0; i + 1 < len; ++i) {
							const int64 k = PairKey32(sym[(nk_size)(off + i)], sym[(nk_size)(off + i + 1)]);
							const int64 s = pt.Insert(k);
							Register(s, (int32)w);
						}
					}
					++st.compactions;
				};

				for (int64 w = 0; w < nWords; ++w) {
					const int64 off = wOff[(nk_size)w];
					const int32 len = wLen[(nk_size)w];
					const int64 f = freq[(nk_size)w];
					for (int32 i = 0; i + 1 < len; ++i) {
						const int64 k = PairKey32(sym[(nk_size)(off + i)], sym[(nk_size)(off + i + 1)]);
						const int64 s = pt.Insert(k);
						pt.cnt[(nk_size)s] += f;
						Register(s, (int32)w);
					}
				}
				for (int64 i = 0; i <= pt.mask; ++i)
					if (pt.key[(nk_size)i] != kEmptyKey && pt.cnt[(nk_size)i] > 0)
						heap.Push(pt.key[(nk_size)i], pt.cnt[(nk_size)i]);

				// ---- 4. Boucle de fusion ----------------------------------------------
				out.merges.Clear();
				NkVector<int32> touched;			// mots concernés par la fusion courante
				NkVector<int32> wordMark;			// dernière fusion ayant vu ce mot (dédoublonnage)
				wordMark.Reserve((nk_size)nWords);
				for (int64 i = 0; i < nWords; ++i)
					wordMark.PushBack(-1);
				NkVector<int64> dirty; // paires dont le compte a monté pendant la fusion courante

				for (int32 m = 0; m < targetMerges; ++m) {
					// Meilleure paire encore valide.
					int64 bestKey = 0, bestCnt = 0;
					bool found = false;
					for (;;) {
						int64 k = 0, c = 0;
						if (!heap.Pop(k, c))
							break;
						const int64 s = pt.Find(k);
						const int64 live = (s < 0) ? 0 : pt.cnt[(nk_size)s];
						if (live != c) {
							// Entrée périmée. On ne la jette PAS sèchement : une paire dont le
							// compte a seulement baissé (sans jamais remonter) n'a plus aucune
							// entrée dans le tas, et disparaîtrait définitivement alors qu'elle
							// est encore fusionnable. On la réinscrit à sa valeur réelle.
							if (live > 0)
								heap.Push(k, live);
							continue;
						}
						bestKey = k;
						bestCnt = c;
						found = true;
						break;
					}
					if (!found || bestCnt < cfg.minPairFreq) {
						if (cfg.verbose)
							logger.Info("BPE : arrêt à {0} fusions (plus aucune paire de fréquence >= {1}).",
										(long long)out.merges.Size(), (long long)cfg.minPairFreq);
						break;
					}

					// Vérification par force brute (tests) : la comptabilité incrémentale
					// dit-elle EXACTEMENT ce que dirait un recomptage complet, et la paire
					// retenue est-elle vraiment de compte maximal ?
					if (m < cfg.verifyMerges) {
						PairTable ref;
						ref.Init(pt.mask + 1);
						for (int64 w = 0; w < nWords; ++w) {
							const int64 off = wOff[(nk_size)w];
							const int32 len = wLen[(nk_size)w];
							const int64 f = freq[(nk_size)w];
							for (int32 i = 0; i + 1 < len; ++i) {
								const int64 s = ref.Insert(PairKey32(sym[(nk_size)(off + i)], sym[(nk_size)(off + i + 1)]));
								ref.cnt[(nk_size)s] += f;
							}
						}
						int64 trueMax = 0;
						bool agree = true;
						for (int64 i = 0; i <= ref.mask; ++i) {
							if (ref.key[(nk_size)i] == kEmptyKey)
								continue;
							if (ref.cnt[(nk_size)i] > trueMax)
								trueMax = ref.cnt[(nk_size)i];
							const int64 s = pt.Find(ref.key[(nk_size)i]);
							const int64 live = (s < 0) ? 0 : pt.cnt[(nk_size)s];
							if (live != ref.cnt[(nk_size)i])
								agree = false;
						}
						for (int64 i = 0; i <= pt.mask; ++i) {
							if (pt.key[(nk_size)i] == kEmptyKey || pt.cnt[(nk_size)i] == 0)
								continue;
							const int64 s = ref.Find(pt.key[(nk_size)i]);
							const int64 refc = (s < 0) ? 0 : ref.cnt[(nk_size)s];
							if (refc != pt.cnt[(nk_size)i])
								agree = false;
						}
						if (bestCnt != trueMax)
							agree = false;
						++st.verifyChecked;
						if (!agree) {
							++st.verifyFailed;
							logger.Info("BPE VERIF : DESACCORD a la fusion {0} (choisi {1}, max reel {2}).", m,
										(long long)bestCnt, (long long)trueMax);
						}
					}

					const int32 a = PairA(bestKey), b = PairB(bestKey);
					NkMerge mg;
					mg.a = a;
					mg.b = b;
					out.merges.PushBack(mg);
					const int32 newId = 256 + (int32)(out.merges.Size() - 1);
					st.lastPairFreq = bestCnt;

					// Mots concernés : la chaîne peut contenir des doublons et des mots qui
					// ne portent plus la paire — les deux sont sans conséquence (un mot sans
					// la paire est réécrit à l'identique, donc son bilan de comptes est nul).
					touched.Clear();
					dirty.Clear();
					{
						const int64 s = pt.Find(bestKey);
						int32 node = (s < 0) ? 0 : pt.head[(nk_size)s];
						while (node != 0) {
							const int32 w = nodeWord[(nk_size)(node - 1)];
							if (wordMark[(nk_size)w] != m) {
								wordMark[(nk_size)w] = m;
								touched.PushBack(w);
							}
							node = nodeNext[(nk_size)(node - 1)];
						}
						if (s >= 0)
							pt.head[(nk_size)s] = 0; // la paire est consommée : chaîne inutile
					}

					for (int64 ti = 0; ti < (int64)touched.Size(); ++ti) {
						const int32 w = touched[(nk_size)ti];
						const int64 off = wOff[(nk_size)w];
						const int32 len = wLen[(nk_size)w];
						const int64 f = freq[(nk_size)w];

						// Retire toutes les paires de l'ANCIENNE suite, puis ajoute toutes
						// celles de la NOUVELLE. Plus coûteux que de ne toucher qu'au
						// voisinage de chaque occurrence, mais manifestement correct : aucun
						// raisonnement sur « le voisin de gauche a-t-il déjà été fusionné ».
						for (int32 i = 0; i + 1 < len; ++i) {
							const int64 s = pt.Insert(PairKey32(sym[(nk_size)(off + i)], sym[(nk_size)(off + i + 1)]));
							pt.cnt[(nk_size)s] -= f;
						}

						int32 wr = 0;
						for (int32 i = 0; i < len;) {
							if (i + 1 < len && sym[(nk_size)(off + i)] == a && sym[(nk_size)(off + i + 1)] == b) {
								sym[(nk_size)(off + wr)] = newId;
								++wr;
								i += 2;
							} else {
								sym[(nk_size)(off + wr)] = sym[(nk_size)(off + i)];
								++wr;
								i += 1;
							}
						}
						wLen[(nk_size)w] = wr;

						for (int32 i = 0; i + 1 < wr; ++i) {
							const int64 k = PairKey32(sym[(nk_size)(off + i)], sym[(nk_size)(off + i + 1)]);
							const int64 s = pt.Insert(k);
							pt.cnt[(nk_size)s] += f;
							// Une paire peut être incrémentée par des centaines de mots dans une
							// même fusion : on ne l'empile qu'UNE fois, à sa valeur finale, une
							// fois tous les mots traités. Sans cela le tas grossirait comme le
							// nombre total d'incréments, pas comme le nombre de paires.
							if (pt.dmark[(nk_size)s] != m + 1) {
								pt.dmark[(nk_size)s] = m + 1;
								dirty.PushBack(k);
							}
							Register(s, w);
						}
					}

					for (int64 di = 0; di < (int64)dirty.Size(); ++di) {
						const int64 s = pt.Find(dirty[(nk_size)di]);
						if (s >= 0 && pt.cnt[(nk_size)s] > 0)
							heap.Push(dirty[(nk_size)di], pt.cnt[(nk_size)s]);
					}

					if ((int64)nodeWord.Size() > maxNodes)
						RebuildChains();

					if (cfg.verbose && ((m + 1) % 2000 == 0))
						logger.Info("BPE : {0}/{1} fusions (derniere frequence {2}).", m + 1, targetMerges,
									(long long)bestCnt);
				}

				st.merges = (int64)out.merges.Size();
				st.finalVocab = 256 + st.merges;
				for (int64 w = 0; w < nWords; ++w)
					st.finalSymbolsWeighted += (int64)wLen[(nk_size)w] * freq[(nk_size)w];
				out.pretok = cfg.pretok;
				out.BuildVocabRank();
				if (stats)
					*stats = st;
				if (cfg.verbose)
					logger.Info("BPE : {0} fusions apprises, vocabulaire {1} (mode pretok {2}, {3} compactages).",
								(long long)st.merges, (long long)st.finalVocab, cfg.pretok, (long long)st.compactions);
				return st.merges > 0;
			}

			// =========================================================================
			// Persistance « NKBP » v1
			// =========================================================================
			bool SaveBpe(const char *path, const NkBpe &bpe) {
				FILE *f = fopen(path, "wb");
				if (!f)
					return false;
				const char magic[4] = {'N', 'K', 'B', 'P'};
				const int32 version = 1;
				const int32 pretok = bpe.pretok;
				const int32 n = (int32)bpe.merges.Size();
				bool ok = fwrite(magic, 1, 4, f) == 4;
				ok = ok && fwrite(&version, sizeof(int32), 1, f) == 1;
				ok = ok && fwrite(&pretok, sizeof(int32), 1, f) == 1;
				ok = ok && fwrite(&n, sizeof(int32), 1, f) == 1;
				for (int32 i = 0; ok && i < n; ++i) {
					const int32 a = bpe.merges[(nk_size)i].a;
					const int32 b = bpe.merges[(nk_size)i].b;
					ok = ok && fwrite(&a, sizeof(int32), 1, f) == 1;
					ok = ok && fwrite(&b, sizeof(int32), 1, f) == 1;
				}
				fclose(f);
				return ok;
			}

			bool LoadBpe(const char *path, NkBpe &bpe) {
				FILE *f = fopen(path, "rb");
				if (!f)
					return false;
				char magic[4] = {0, 0, 0, 0};
				int32 version = 0, pretok = 0, n = 0;
				bool ok = fread(magic, 1, 4, f) == 4;
				ok = ok && magic[0] == 'N' && magic[1] == 'K' && magic[2] == 'B' && magic[3] == 'P';
				ok = ok && fread(&version, sizeof(int32), 1, f) == 1 && version == 1;
				ok = ok && fread(&pretok, sizeof(int32), 1, f) == 1;
				ok = ok && fread(&n, sizeof(int32), 1, f) == 1 && n >= 0 && n < (1 << 22);
				if (!ok) {
					fclose(f);
					return false;
				}
				bpe.merges.Clear();
				for (int32 i = 0; i < n; ++i) {
					int32 a = 0, b = 0;
					if (fread(&a, sizeof(int32), 1, f) != 1 || fread(&b, sizeof(int32), 1, f) != 1) {
						ok = false;
						break;
					}
					NkMerge mg;
					mg.a = a;
					mg.b = b;
					bpe.merges.PushBack(mg);
				}
				fclose(f);
				if (!ok)
					return false;
				bpe.pretok = pretok;
				bpe.BuildVocabRank();
				return true;
			}

			// =========================================================================
			// Encodeur à mémo
			// =========================================================================
			NkBpeEncoder::NkBpeEncoder(const NkBpe &bpe) : mBpe(bpe) {
				const int64 cap = 1 << 16;
				for (int64 i = 0; i < cap; ++i) {
					mKey.PushBack(NkString());
					mUsed.PushBack(0);
					mOff.PushBack(0);
					mLen.PushBack(0);
				}
				mMask = cap - 1;
			}

			void NkBpeEncoder::Grow() {
				NkVector<NkString> ok = mKey;
				NkVector<uint8> ou = mUsed;
				NkVector<int64> oo = mOff;
				NkVector<int32> ol = mLen;
				const int64 oldCap = mMask + 1;
				mKey.Clear();
				mUsed.Clear();
				mOff.Clear();
				mLen.Clear();
				for (int64 i = 0; i < oldCap * 2; ++i) {
					mKey.PushBack(NkString());
					mUsed.PushBack(0);
					mOff.PushBack(0);
					mLen.PushBack(0);
				}
				mMask = oldCap * 2 - 1;
				mCount = 0;
				for (int64 i = 0; i < oldCap; ++i) {
					if (!ou[(nk_size)i])
						continue;
					const NkString &w = ok[(nk_size)i];
					int64 s = (int64)(MixHash(HashBytes(w.Data(), (int64)w.Size())) & (uint64)mMask);
					while (mUsed[(nk_size)s])
						s = (s + 1) & mMask;
					mKey[(nk_size)s] = w;
					mUsed[(nk_size)s] = 1;
					mOff[(nk_size)s] = oo[(nk_size)i];
					mLen[(nk_size)s] = ol[(nk_size)i];
					++mCount;
				}
			}

			int64 NkBpeEncoder::Slot(const NkString &w) {
				if ((mCount + 1) * 10 > (mMask + 1) * 7)
					Grow();
				int64 s = (int64)(MixHash(HashBytes(w.Data(), (int64)w.Size())) & (uint64)mMask);
				while (mUsed[(nk_size)s]) {
					if (mKey[(nk_size)s] == w)
						return s;
					s = (s + 1) & mMask;
				}
				return s; // case libre où insérer
			}

			void NkBpeEncoder::Encode(const NkString &text, NkVector<int32> &out) {
				NkVector<NkString> words;
				NkBpe::PreTokMode(text, mBpe.pretok, words);
				for (int64 wi = 0; wi < (int64)words.Size(); ++wi) {
					const NkString &w = words[(nk_size)wi];
					const int64 s = Slot(w);
					if (mUsed[(nk_size)s]) {
						++mHits;
						const int64 off = mOff[(nk_size)s];
						const int32 len = mLen[(nk_size)s];
						for (int32 k = 0; k < len; ++k)
							out.PushBack(mIds[(nk_size)(off + k)]);
						continue;
					}
					++mMisses;
					NkVector<int32> ids;
					mBpe.EncodeWord(w, ids);
					mKey[(nk_size)s] = w;
					mUsed[(nk_size)s] = 1;
					mOff[(nk_size)s] = (int64)mIds.Size();
					mLen[(nk_size)s] = (int32)ids.Size();
					++mCount;
					for (int64 k = 0; k < (int64)ids.Size(); ++k) {
						mIds.PushBack(ids[(nk_size)k]);
						out.PushBack(ids[(nk_size)k]);
					}
				}
			}

		} // namespace data
	} // namespace ai
} // namespace nkentseu
