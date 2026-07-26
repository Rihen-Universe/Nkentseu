// =============================================================================
// NKData/NkTokenizer.cpp — implémentation du tokenizer BPE générique (NKAI).
// Déplacé/généralisé depuis NKGpt/NkGptCore.cpp (algorithme inchangé, aucune
// dépendance GPT : pure tokenisation texte <-> identifiants).
// =============================================================================
#include "NKData/NkTokenizer.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace ai {
		namespace data {

			// ================= NkI64Map ===================================================
			static const int64 kEmpty = (int64)0x8000000000000000LL;

			void NkI64Map::Init(int64 pow2) {
				keys.Clear();
				vals.Clear();
				keys.Reserve((nk_size)pow2);
				vals.Reserve((nk_size)pow2);
				for (int64 i = 0; i < pow2; ++i) {
					keys.PushBack(kEmpty);
					vals.PushBack(0);
				}
				mask = pow2 - 1;
				bestKey = -1;
				bestVal = 0;
			}

			void NkI64Map::Reset() {
				for (int64 i = 0; i <= mask; ++i) {
					keys[(nk_size)i] = kEmpty;
					vals[(nk_size)i] = 0;
				}
				bestKey = -1;
				bestVal = 0;
			}

			uint64 NkI64Map::Hash(int64 k) {
				uint64 h = (uint64)k * 1099511628211ULL;
				h ^= h >> 29;
				h *= 1099511628211ULL;
				h ^= h >> 32;
				return h;
			}

			void NkI64Map::Add(int64 k, int64 w) {
				int64 s = (int64)(Hash(k) & (uint64)mask);
				while (keys[(nk_size)s] != kEmpty && keys[(nk_size)s] != k)
					s = (s + 1) & mask;
				if (keys[(nk_size)s] == kEmpty) {
					keys[(nk_size)s] = k;
					vals[(nk_size)s] = w;
				} else
					vals[(nk_size)s] += w;
				int64 nv = vals[(nk_size)s];
				if (nv > bestVal) {
					bestVal = nv;
					bestKey = k;
				}
			}

			int64 NkI64Map::Get(int64 k, int64 def) const {
				int64 s = (int64)(Hash(k) & (uint64)mask);
				while (keys[(nk_size)s] != kEmpty) {
					if (keys[(nk_size)s] == k)
						return vals[(nk_size)s];
					s = (s + 1) & mask;
				}
				return def;
			}

			static int64 PairKey(int32 a, int32 b) {
				return ((int64)a << 21) | (int64)b;
			}

			// ================= NkBpe =======================================================
			void NkBpe::BuildVocabRank() {
				vocab.Clear();
				for (int b = 0; b < 256; ++b) {
					NkString s;
					s.Append((char)b);
					vocab.PushBack(s);
				}
				int64 cap = 2;
				while (cap < (int64)(merges.Size() * 2 + 8))
					cap <<= 1;
				rank.Init(cap);
				for (int64 i = 0; i < (int64)merges.Size(); ++i) {
					NkString t = vocab[(nk_size)merges[(nk_size)i].a];
					t.Append(vocab[(nk_size)merges[(nk_size)i].b]);
					vocab.PushBack(t);
					rank.Add(PairKey(merges[(nk_size)i].a, merges[(nk_size)i].b), i);
				}
			}

			void NkBpe::PreTok(const NkString &text, NkVector<NkString> &words) {
				const char *p = text.Data();
				int64 n = (int64)text.Size();
				NkString cur;
				for (int64 i = 0; i < n; ++i) {
					unsigned char c = (unsigned char)p[i];
					if (c == ' ' || c == '\n' || c == '\t' || c == '\r') {
						if (cur.Size() > 0)
							words.PushBack(cur);
						cur = NkString();
						cur.Append((char)c);
					} else
						cur.Append((char)c);
				}
				if (cur.Size() > 0)
					words.PushBack(cur);
			}

			void NkBpe::EncodeWord(const NkString &w, NkVector<int32> &out) const {
				NkVector<int32> seq;
				const char *p = w.Data();
				int64 n = (int64)w.Size();
				for (int64 i = 0; i < n; ++i)
					seq.PushBack((int32)(unsigned char)p[i]);
				while (seq.Size() >= 2) {
					int64 bestRank = 0x7fffffffLL;
					int64 bestPos = -1;
					for (int64 i = 0; i + 1 < (int64)seq.Size(); ++i) {
						int64 r = rank.Get(PairKey(seq[(nk_size)i], seq[(nk_size)(i + 1)]), 0x7fffffffLL);
						if (r < bestRank) {
							bestRank = r;
							bestPos = i;
						}
					}
					if (bestPos < 0)
						break;
					seq[(nk_size)bestPos] = 256 + (int32)bestRank;
					for (int64 j = bestPos + 1; j + 1 < (int64)seq.Size(); ++j)
						seq[(nk_size)j] = seq[(nk_size)(j + 1)];
					seq.Resize((nk_size)(seq.Size() - 1));
				}
				for (int64 i = 0; i < (int64)seq.Size(); ++i)
					out.PushBack(seq[(nk_size)i]);
			}

			void NkBpe::Encode(const NkString &text, NkVector<int32> &out) const {
				NkVector<NkString> words;
				PreTok(text, words);
				for (int64 i = 0; i < (int64)words.Size(); ++i)
					EncodeWord(words[(nk_size)i], out);
			}

			void TrainBpe(const NkVector<NkString> &texts, int nMerges, NkBpe &bpe) {
				const int32 SEP = -1;
				const int64 CAP = 800000;
				NkVector<int32> flat;
				for (int64 ti = 0; ti < (int64)texts.Size() && (int64)flat.Size() < CAP; ++ti) {
					NkVector<NkString> words;
					NkBpe::PreTok(texts[(nk_size)ti], words);
					for (int64 wi = 0; wi < (int64)words.Size(); ++wi) {
						const NkString &w = words[(nk_size)wi];
						const char *p = w.Data();
						int64 n = (int64)w.Size();
						for (int64 i = 0; i < n; ++i)
							flat.PushBack((int32)(unsigned char)p[i]);
						flat.PushBack(SEP);
						if ((int64)flat.Size() >= CAP)
							break;
					}
				}
				NkI64Map pc;
				int64 cap = 1;
				while (cap < (1 << 19))
					cap <<= 1;
				pc.Init(cap);
				for (int m = 0; m < nMerges; ++m) {
					pc.Reset();
					int64 N = (int64)flat.Size();
					for (int64 i = 0; i + 1 < N; ++i) {
						int32 a = flat[(nk_size)i], b = flat[(nk_size)(i + 1)];
						if (a == SEP || b == SEP)
							continue;
						pc.Add(PairKey(a, b), 1);
					}
					if (pc.bestKey < 0 || pc.bestVal < 2)
						break;
					int32 a = (int32)(pc.bestKey >> 21), b = (int32)(pc.bestKey & ((1 << 21) - 1));
					NkMerge mg;
					mg.a = a;
					mg.b = b;
					bpe.merges.PushBack(mg);
					int32 newId = 256 + (int32)(bpe.merges.Size() - 1);
					int64 w = 0;
					for (int64 r = 0; r < N;) {
						if (r + 1 < N && flat[(nk_size)r] == a && flat[(nk_size)(r + 1)] == b) {
							flat[(nk_size)w] = newId;
							++w;
							r += 2;
						} else {
							flat[(nk_size)w] = flat[(nk_size)r];
							++w;
							r += 1;
						}
					}
					flat.Resize((nk_size)w);
					if ((m + 1) % 200 == 0)
						logger.Info("  BPE : {0}/{1} fusions...", m + 1, nMerges);
				}
				bpe.BuildVocabRank();
			}

			NkString DecodeAll(const NkBpe &bpe, const NkVector<int32> &ids) {
				NkString out;
				for (int64 i = 0; i < (int64)ids.Size(); ++i) {
					int32 id = ids[(nk_size)i];
					if (id >= 0 && (nk_size)id < bpe.vocab.Size())
						out.Append(bpe.Decode(id));
				}
				return out;
			}

		} // namespace data
	} // namespace ai
} // namespace nkentseu
