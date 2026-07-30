// =============================================================================
// NKData/NkSequence.cpp — vocabulaire mot-à-mot + padding de séquences (NKAI).
// =============================================================================
#include "NKData/NkSequence.h"

namespace nkentseu {
	namespace ai {
		namespace data {

			// Définitions hors-classe (ODR-use : ces constantes sont passées par référence à
			// NkHashMap::Insert/etc. — un `static const` non-constexpr en a besoin en C++17).
			const int32 NkVocab::kPadId;
			const int32 NkVocab::kUnkId;

			// Découpe sur les espaces/'\n'/'\t'/'\r' — mots SANS le séparateur (à la
			// différence de `NkBpe::PreTok`, qui garde le séparateur collé au mot suivant
			// pour la convention BPE octet-à-octet).
			static void SplitWords(const NkString &text, NkVector<NkString> &words) {
				const char *p = text.Data();
				int64 n = (int64)text.Size();
				NkString cur;
				for (int64 i = 0; i < n; ++i) {
					unsigned char c = (unsigned char)p[i];
					if (c == ' ' || c == '\n' || c == '\t' || c == '\r') {
						if (cur.Size() > 0) {
							words.PushBack(cur);
							cur = NkString();
						}
					} else {
						cur.Append((char)c);
					}
				}
				if (cur.Size() > 0)
					words.PushBack(cur);
			}

			NkVocab::NkVocab() {
				NkString pad("<pad>");
				NkString unk("<unk>");
				mIdToWord.PushBack(pad);
				mIdToWord.PushBack(unk);
				mWordToId.Insert(pad, kPadId);
				mWordToId.Insert(unk, kUnkId);
			}

			void NkVocab::BuildFromTexts(const NkVector<NkString> &texts, int32 minFreq) {
				NkHashMap<NkString, int32> freq;
				for (int64 ti = 0; ti < (int64)texts.Size(); ++ti) {
					NkVector<NkString> words;
					SplitWords(texts[(nk_size)ti], words);
					for (int64 wi = 0; wi < (int64)words.Size(); ++wi) {
						const NkString &w = words[(nk_size)wi];
						int32 *f = freq.Find(w);
						if (f)
							++(*f);
						else
							freq.Insert(w, 1);
					}
				}

				// Collecte (mot, fréquence) filtrée par minFreq.
				NkVector<NkString> wArr;
				NkVector<int32> fArr;
				for (const auto &e : freq) {
					if (e.Second >= minFreq) {
						wArr.PushBack(e.First);
						fArr.PushBack(e.Second);
					}
				}

				// Tri par fréquence DÉCROISSANTE (insertion sort : vocabulaire compact
				// attendu, priorité à la simplicité/déterminisme plutôt qu'à l'échelle).
				// Départage à fréquence égale par ordre alphabétique -> vocabulaire
				// reproductible d'un run à l'autre (l'ordre de NkHashMap ne l'est pas).
				for (int64 i = 1; i < (int64)wArr.Size(); ++i) {
					NkString w = wArr[(nk_size)i];
					int32 f = fArr[(nk_size)i];
					int64 j = i - 1;
					while (j >= 0 && (fArr[(nk_size)j] < f || (fArr[(nk_size)j] == f && wArr[(nk_size)j] > w))) {
						wArr[(nk_size)(j + 1)] = wArr[(nk_size)j];
						fArr[(nk_size)(j + 1)] = fArr[(nk_size)j];
						--j;
					}
					wArr[(nk_size)(j + 1)] = w;
					fArr[(nk_size)(j + 1)] = f;
				}

				for (int64 i = 0; i < (int64)wArr.Size(); ++i) {
					const NkString &w = wArr[(nk_size)i];
					if (mWordToId.Contains(w))
						continue;
					const int32 id = (int32)mIdToWord.Size();
					mIdToWord.PushBack(w);
					mWordToId.Insert(w, id);
				}
			}

			int32 NkVocab::IdOf(const NkString &word) const {
				const int32 *id = mWordToId.Find(word);
				return id ? *id : kUnkId;
			}

			const NkString &NkVocab::WordOf(int32 id) const {
				if (id >= 0 && (nk_size)id < mIdToWord.Size())
					return mIdToWord[(nk_size)id];
				return mIdToWord[(nk_size)kUnkId];
			}

			void NkVocab::Encode(const NkString &text, NkVector<int32> &outIds) const {
				NkVector<NkString> words;
				SplitWords(text, words);
				for (int64 i = 0; i < (int64)words.Size(); ++i)
					outIds.PushBack(IdOf(words[(nk_size)i]));
			}

			NkString NkVocab::DecodeAll(const NkVector<int32> &ids) const {
				NkString out;
				for (int64 i = 0; i < (int64)ids.Size(); ++i) {
					if (i > 0)
						out.Append(' ');
					out.Append(WordOf(ids[(nk_size)i]));
				}
				return out;
			}

			// =================================================================
			// PadSequences
			// =================================================================
			NkSeqBatch PadSequences(const NkVector<NkVector<int32>> &seqs, int32 padId, int32 maxLen) {
				NkSeqBatch out;
				const uint32 B = seqs.Size();
				int64 T = maxLen;
				if (T <= 0) {
					T = 0;
					for (uint32 i = 0; i < B; ++i)
						if ((int64)seqs[i].Size() > T)
							T = (int64)seqs[i].Size();
				}
				if (T <= 0)
					T = 1; // évite un tenseur [B,0] dégénéré même si toutes les séquences sont vides

				out.ids = NkTensor::Zeros(NkShape{(int64)B, T});
				out.mask = NkTensor::Zeros(NkShape{(int64)B, T});
				float *idp = out.ids.DataAs<float>();
				float *mkp = out.mask.DataAs<float>();
				for (uint32 b = 0; b < B; ++b) {
					const NkVector<int32> &s = seqs[b];
					const int64 realLen = (int64)s.Size();
					const int64 kept = (realLen < T) ? realLen : T; // tronque si trop long
					for (int64 t = 0; t < T; ++t) {
						const nk_size at = (nk_size)b * (nk_size)T + (nk_size)t;
						if (t < kept) {
							idp[at] = (float)s[(nk_size)t];
							mkp[at] = 1.0f;
						} else {
							idp[at] = (float)padId;
							mkp[at] = 0.0f;
						}
					}
					out.lengths.PushBack((int32)kept);
				}
				return out;
			}

		} // namespace data
	} // namespace ai
} // namespace nkentseu
