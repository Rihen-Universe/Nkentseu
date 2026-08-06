// =============================================================================
// NkQwen2Tokenizer.cpp — implémentation du BPE byte-level Qwen2 (NKInfer).
// Voir NkQwen2Tokenizer.h pour la spec complète (table GPT-2, approximation de
// la pré-tokenisation, garantie de round-trip) et Documentation/
// notes_etape4_qlora.md §2.1 pour le POURQUOI de ce module.
// =============================================================================
#include "NKInfer/NkQwen2Tokenizer.h"
#include "NKInfer/NkGGUFLoader.h"

namespace nkentseu {
	namespace ai {
		namespace infer {

			// ================= NkQwen2I64Map =========================================
			// Même schéma que data::NkI64Map (open addressing linéaire, capacité
			// puissance de 2) : la sonde linéaire suffit car la charge est bornée à
			// ~50 % à l'Init (capacité >= 2x le nombre d'entrées).
			static const int64 kQwenMapEmpty = (int64)0x8000000000000000LL;

			void NkQwen2I64Map::Init(int64 pow2) {
				keys.Clear();
				vals.Clear();
				keys.Reserve((nk_size)pow2);
				vals.Reserve((nk_size)pow2);
				for (int64 i = 0; i < pow2; ++i) {
					keys.PushBack(kQwenMapEmpty);
					vals.PushBack(0);
				}
				mask = pow2 - 1;
			}

			// Mélange multiplicatif FNV-prime (identique à NkI64Map::Hash) : disperse
			// les clés (idA<<32)|idB dont les bits bas se ressemblent beaucoup.
			static uint64 QwenMapHash(int64 k) {
				uint64 h = (uint64)k * 1099511628211ULL;
				h ^= h >> 29;
				h *= 1099511628211ULL;
				h ^= h >> 32;
				return h;
			}

			void NkQwen2I64Map::Put(int64 k, int64 v) {
				if (mask < 0)
					return;
				int64 s = (int64)(QwenMapHash(k) & (uint64)mask);
				while (keys[(nk_size)s] != kQwenMapEmpty && keys[(nk_size)s] != k)
					s = (s + 1) & mask;
				keys[(nk_size)s] = k;
				vals[(nk_size)s] = v;
			}

			int64 NkQwen2I64Map::Get(int64 k, int64 def) const {
				if (mask < 0)
					return def;
				int64 s = (int64)(QwenMapHash(k) & (uint64)mask);
				while (keys[(nk_size)s] != kQwenMapEmpty) {
					if (keys[(nk_size)s] == k)
						return vals[(nk_size)s];
					s = (s + 1) & mask;
				}
				return def;
			}

			// ================= Aides UTF-8 ===========================================
			// Les codepoints remappés de la table GPT-2 sont tous < 0x800 (max 323),
			// donc l'ENCODAGE n'a besoin que de 1-2 octets. Le DÉCODAGE, lui, doit
			// être général (jusqu'à 4 octets) : les tokens « added » d'un vocab réel
			// peuvent contenir n'importe quel codepoint.

			static int32 Utf8Encode(uint32 cp, char out[4]) {
				if (cp < 0x80u) {
					out[0] = (char)cp;
					return 1;
				}
				if (cp < 0x800u) {
					out[0] = (char)(0xC0u | (cp >> 6));
					out[1] = (char)(0x80u | (cp & 0x3Fu));
					return 2;
				}
				if (cp < 0x10000u) {
					out[0] = (char)(0xE0u | (cp >> 12));
					out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
					out[2] = (char)(0x80u | (cp & 0x3Fu));
					return 3;
				}
				out[0] = (char)(0xF0u | (cp >> 18));
				out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
				out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
				out[3] = (char)(0x80u | (cp & 0x3Fu));
				return 4;
			}

			// Décode le codepoint commençant à `p` (au plus `avail` octets). Renvoie
			// le nombre d'octets consommés, ou 0 si la séquence est invalide (le
			// caller recopie alors l'octet brut tel quel : jamais de perte de données,
			// c'est la condition du round-trip octet-à-octet).
			static int32 Utf8Decode(const char *p, int64 avail, uint32 &outCp) {
				if (avail <= 0)
					return 0;
				const uint8 c0 = (uint8)p[0];
				if (c0 < 0x80u) {
					outCp = c0;
					return 1;
				}
				int32 len = 0;
				uint32 cp = 0;
				if ((c0 & 0xE0u) == 0xC0u) {
					len = 2;
					cp = c0 & 0x1Fu;
				} else if ((c0 & 0xF0u) == 0xE0u) {
					len = 3;
					cp = c0 & 0x0Fu;
				} else if ((c0 & 0xF8u) == 0xF0u) {
					len = 4;
					cp = c0 & 0x07u;
				} else {
					return 0; // octet de continuation isolé ou en-tête invalide
				}
				if (avail < (int64)len)
					return 0;
				for (int32 i = 1; i < len; ++i) {
					const uint8 ci = (uint8)p[i];
					if ((ci & 0xC0u) != 0x80u)
						return 0;
					cp = (cp << 6) | (uint32)(ci & 0x3Fu);
				}
				outCp = cp;
				return len;
			}

			// ================= Tables octet<->codepoint (GPT-2) ======================
			void NkQwen2Tokenizer::BuildByteTables() {
				for (int32 i = 0; i < kCpTableSize; ++i)
					mCpToByte[i] = -1;
				// Spec bytes_to_unicode (GPT-2 encoder.py, reprise par Qwen2) : les
				// octets « imprimables » 33-126, 161-172, 174-255 se représentent
				// eux-mêmes ; chaque octet EXCLU reçoit 256+n, n croissant dans
				// l'ordre croissant des octets exclus (0..32 -> 256..288,
				// 127..160 -> 289..322 sauf 173, 173 -> 323... en fait 127..160 puis
				// 173, soit 68 exclus au total, codepoints 256..323).
				int32 n = 0;
				for (int32 b = 0; b < 256; ++b) {
					const bool keep = (b >= 33 && b <= 126) || (b >= 161 && b <= 172) || (b >= 174 && b <= 255);
					const int32 cp = keep ? b : (256 + n++);
					mByteToCp[b] = cp;
					mCpToByte[cp] = b;
				}
			}

			// ================= Table chaîne->id (FNV-1a 64, open addressing) =========
			static uint64 Fnv1a64(const char *data, int64 len) {
				uint64 h = 14695981039346656037ULL;
				for (int64 i = 0; i < len; ++i) {
					h ^= (uint64)(uint8)data[i];
					h *= 1099511628211ULL;
				}
				return h;
			}

			void NkQwen2Tokenizer::BuildStringToId() {
				// Capacité >= 2x le vocabulaire (charge <= 50 % : sonde linéaire courte).
				int64 cap = 2;
				while (cap < (int64)mTokens.Size() * 2)
					cap <<= 1;
				mHashSlots.Clear();
				mHashSlots.Reserve((nk_size)cap);
				for (int64 i = 0; i < cap; ++i)
					mHashSlots.PushBack(-1);
				mHashMask = cap - 1;
				for (int64 id = 0; id < (int64)mTokens.Size(); ++id) {
					const NkString &s = mTokens[(nk_size)id];
					int64 slot = (int64)(Fnv1a64(s.Data(), (int64)s.Size()) & (uint64)mHashMask);
					while (mHashSlots[(nk_size)slot] >= 0) {
						// Doublon EXACT dans le vocab : on garde le PREMIER id (les ids
						// suivants restent décodables via mTokens, simplement jamais
						// produits par Encode — comportement de llama.cpp).
						const NkString &other = mTokens[(nk_size)mHashSlots[(nk_size)slot]];
						if (other.Size() == s.Size() && other.Compare(s) == 0)
							break;
						slot = (slot + 1) & mHashMask;
					}
					if (mHashSlots[(nk_size)slot] < 0)
						mHashSlots[(nk_size)slot] = (int32)id;
				}
			}

			int32 NkQwen2Tokenizer::FindTokenId(const NkString &s) const {
				if (mHashMask < 0)
					return -1;
				int64 slot = (int64)(Fnv1a64(s.Data(), (int64)s.Size()) & (uint64)mHashMask);
				while (mHashSlots[(nk_size)slot] >= 0) {
					// VÉRIFICATION de la chaîne au hit : l'égalité de hash/slot ne
					// prouve rien, seule l'égalité d'octets valide le résultat (pas de
					// collision silencieuse -> pas d'id faux).
					const NkString &cand = mTokens[(nk_size)mHashSlots[(nk_size)slot]];
					if (cand.Size() == s.Size() && cand.Compare(s) == 0)
						return mHashSlots[(nk_size)slot];
					slot = (slot + 1) & mHashMask;
				}
				return -1;
			}

			const NkString &NkQwen2Tokenizer::TokenString(int32 id) const {
				static NkString sEmpty;
				if (id < 0 || (nk_size)id >= mTokens.Size())
					return sEmpty;
				return mTokens[(nk_size)id];
			}

			// ================= Fusions ===============================================
			// Clé d'une paire : (idA<<32)|idB — les ids Qwen (< 152 064) tiennent
			// largement sur 32 bits, la clé est donc unique et positive.
			static int64 MergePairKey(int32 idA, int32 idB) {
				return (int64)(((uint64)(uint32)idA << 32) | (uint64)(uint32)idB);
			}

			void NkQwen2Tokenizer::BuildMerges(const NkVector<NkString> &merges) {
				int64 cap = 2;
				while (cap < (int64)merges.Size() * 2 + 8)
					cap <<= 1;
				mMergeRank.Init(cap);
				mMergeResult.Init(cap);
				mMergeCount = 0;
				mSkippedMerges = 0;
				for (int64 r = 0; r < (int64)merges.Size(); ++r) {
					const NkString &line = merges[(nk_size)r];
					// Séparateur = le PREMIER espace ASCII : les chaînes du vocab ne
					// contiennent jamais d'espace brut (0x20 est remappé en « Ġ »), le
					// découpage est donc sans ambiguïté.
					const NkString::SizeType sp = line.Find(' ');
					if (sp == NkString::npos || sp == 0 || sp + 1 >= line.Size()) {
						mSkippedMerges++;
						continue;
					}
					NkString a = line.SubStr(0, sp);
					NkString b = line.SubStr(sp + 1);
					const int32 idA = FindTokenId(a);
					const int32 idB = FindTokenId(b);
					// L'id du RÉSULTAT est retrouvé par lookup de la concaténation :
					// le fichier merges ne donne pas l'id, seul le vocab fait foi.
					NkString ab = a;
					ab.Append(b);
					const int32 idAB = FindTokenId(ab);
					if (idA < 0 || idB < 0 || idAB < 0) {
						mSkippedMerges++;
						continue;
					}
					const int64 key = MergePairKey(idA, idB);
					mMergeRank.Put(key, r);
					mMergeResult.Put(key, (int64)idAB);
					mMergeCount++;
				}
			}

			// ================= Tokens spéciaux =======================================
			void NkQwen2Tokenizer::ResolveSpecials() {
				// Cherchés PAR CHAÎNE (jamais par id supposé) : si un vocab de test ne
				// les contient pas, leur id reste -1 et EncodeWithSpecials == Encode.
				mEndOfTextId = FindTokenId(NkString("<|endoftext|>"));
				mImStartId = FindTokenId(NkString("<|im_start|>"));
				mImEndId = FindTokenId(NkString("<|im_end|>"));
			}

			// ================= Chargement ============================================
			bool NkQwen2Tokenizer::LoadFromLists(const NkVector<NkString> &tokens, const NkVector<NkString> &merges,
												 NkString *err) {
				if (tokens.Size() == 0) {
					if (err)
						*err = "NkQwen2Tokenizer : vocabulaire vide";
					return false;
				}
				mTokens.Clear();
				mTokens.Reserve(tokens.Size());
				for (nk_size i = 0; i < tokens.Size(); ++i)
					mTokens.PushBack(tokens[i]);

				BuildByteTables();
				BuildStringToId();

				// Token de base de chaque octet : la chaîne UTF-8 de son codepoint
				// remappé, cherchée dans le vocab. Sur un GGUF Qwen réel, les 256
				// existent toujours (c'est la définition du byte-level) ; -1 toléré
				// ici pour ne pas empêcher des vocabs de test partiels de charger.
				for (int32 b = 0; b < 256; ++b) {
					char buf[4];
					const int32 len = Utf8Encode((uint32)mByteToCp[b], buf);
					NkString s(buf, (NkString::SizeType)len);
					mByteTokenId[b] = FindTokenId(s);
				}

				BuildMerges(merges);
				ResolveSpecials();
				return true;
			}

			bool NkQwen2Tokenizer::LoadFromGGUF(const char *path, NkString *err) {
				NkVector<NkString> tokens;
				if (!NkGGUFReadFullStringArray(path, "tokenizer.ggml.tokens", tokens)) {
					if (err)
						*err = "NkQwen2Tokenizer : lecture de tokenizer.ggml.tokens échouée";
					return false;
				}
				NkVector<NkString> merges;
				if (!NkGGUFReadFullStringArray(path, "tokenizer.ggml.merges", merges)) {
					if (err)
						*err = "NkQwen2Tokenizer : lecture de tokenizer.ggml.merges échouée";
					return false;
				}
				return LoadFromLists(tokens, merges, err);
			}

			// ================= Pré-tokenisation ======================================
			// Classement PAR OCTET (cf en-tête du .h pour la justification UTF-8).
			static bool IsSpaceByte(uint8 c) {
				return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
			}
			static bool IsLetterByte(uint8 c) {
				return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80;
			}
			static bool IsDigitByte(uint8 c) {
				return c >= '0' && c <= '9';
			}

			// Longueur d'une contraction anglaise commençant à p[0] == '\'' (0 si
			// aucune) : 's 't 'm 'd (2 octets) ; 're 've 'll (3 octets). Minuscules
			// uniquement, comme la regex GPT-2 d'origine (non insensible à la casse).
			static int64 ContractionLen(const char *p, int64 avail) {
				if (avail < 2 || p[0] != '\'')
					return 0;
				const char c1 = p[1];
				if (avail >= 3) {
					const char c2 = p[2];
					if ((c1 == 'r' && c2 == 'e') || (c1 == 'v' && c2 == 'e') || (c1 == 'l' && c2 == 'l'))
						return 3;
				}
				if (c1 == 's' || c1 == 't' || c1 == 'm' || c1 == 'd')
					return 2;
				return 0;
			}

			void NkQwen2Tokenizer::PreTokenize(const NkString &text, NkVector<NkString> &outSegments) {
				const char *p = text.Data();
				const int64 n = (int64)text.Size();
				int64 i = 0;
				while (i < n) {
					const uint8 c = (uint8)p[i];

					// 1) Contractions anglaises (prioritaires, comme dans la regex).
					const int64 cl = ContractionLen(p + i, n - i);
					if (cl > 0) {
						outSegments.PushBack(NkString(p + i, (NkString::SizeType)cl));
						i += cl;
						continue;
					}

					// 2) Lettres (sans préfixe espace) : mot entier.
					if (IsLetterByte(c)) {
						int64 j = i + 1;
						while (j < n && IsLetterByte((uint8)p[j]))
							j++;
						outSegments.PushBack(NkString(p + i, (NkString::SizeType)(j - i)));
						i = j;
						continue;
					}

					// 3) Chiffres (sans préfixe espace) : 1 à 3 chiffres MAX (borne
					// Qwen \p{N}{1,3} — un nombre long devient plusieurs tokens).
					if (IsDigitByte(c)) {
						int64 j = i;
						int64 k = 0;
						while (j < n && IsDigitByte((uint8)p[j]) && k < 3) {
							j++;
							k++;
						}
						outSegments.PushBack(NkString(p + i, (NkString::SizeType)(j - i)));
						i = j;
						continue;
					}

					// 4) UN espace suivi de lettres/chiffres/ponctuation : l'espace
					// part avec le token suivant (« Ġmot », signature du BPE GPT-2).
					if (c == ' ' && i + 1 < n && !IsSpaceByte((uint8)p[i + 1])) {
						const uint8 nxt = (uint8)p[i + 1];
						int64 j = i + 1;
						if (IsLetterByte(nxt)) {
							while (j < n && IsLetterByte((uint8)p[j]))
								j++;
						} else if (IsDigitByte(nxt)) {
							int64 k = 0;
							while (j < n && IsDigitByte((uint8)p[j]) && k < 3) {
								j++;
								k++;
							}
						} else {
							// Ponctuation : suite d'octets ni espace, ni lettre, ni
							// chiffre (l'apostrophe d'une contraction est déjà happée
							// par le cas 1 au prochain tour si elle démarre le token).
							while (j < n && !IsSpaceByte((uint8)p[j]) && !IsLetterByte((uint8)p[j]) &&
								   !IsDigitByte((uint8)p[j]))
								j++;
						}
						outSegments.PushBack(NkString(p + i, (NkString::SizeType)(j - i)));
						i = j;
						continue;
					}

					// 5) Suite d'espaces. Règle GPT-2 `\s+(?!\S)` : si la suite est
					// suivie de texte ET se termine par un espace simple, ce DERNIER
					// espace est laissé au token suivant (il redevient le préfixe
					// « espace optionnel » du cas 4 au prochain tour).
					if (IsSpaceByte(c)) {
						int64 j = i;
						while (j < n && IsSpaceByte((uint8)p[j]))
							j++;
						if (j < n && (uint8)p[j - 1] == ' ' && j - i > 1) {
							outSegments.PushBack(NkString(p + i, (NkString::SizeType)(j - 1 - i)));
							i = j - 1;
						} else if (j < n && (uint8)p[j - 1] == ' ' && j - i == 1) {
							// Espace isolé suivi d'un non-espace : déjà couvert par le
							// cas 4 — on ne peut arriver ici que si le suivant est un
							// espace (contradiction) ; garde-fou : émettre l'espace.
							outSegments.PushBack(NkString(p + i, 1));
							i = j;
						} else {
							outSegments.PushBack(NkString(p + i, (NkString::SizeType)(j - i)));
							i = j;
						}
						continue;
					}

					// 6) Ponctuation (sans préfixe espace).
					{
						int64 j = i;
						while (j < n && !IsSpaceByte((uint8)p[j]) && !IsLetterByte((uint8)p[j]) &&
							   !IsDigitByte((uint8)p[j]))
							j++;
						outSegments.PushBack(NkString(p + i, (NkString::SizeType)(j - i)));
						i = j;
					}
				}
			}

			// ================= Encode ================================================
			bool NkQwen2Tokenizer::EncodeSegment(const char *bytes, int64 len, NkVector<int32> &outIds) const {
				// Symboles initiaux : un token de base par OCTET du segment (jamais
				// d'inconnu : c'est la propriété byte-level).
				NkVector<int32> seq;
				seq.Reserve((nk_size)len);
				for (int64 i = 0; i < len; ++i) {
					const int32 id = mByteTokenId[(uint8)bytes[i]];
					if (id < 0)
						return false; // vocab incomplet (impossible sur un GGUF réel)
					seq.PushBack(id);
				}

				// Fusions gloutonnes PAR RANG MINIMAL : à chaque tour, on cherche la
				// paire adjacente dont la fusion a le plus petit rang appris, on la
				// remplace par l'id de sa concaténation, et on recommence jusqu'à ce
				// qu'aucune paire ne soit fusionnable. C'est l'algorithme de référence
				// du BPE inférence (même boucle que data::NkBpe::EncodeWord, mais avec
				// l'id de résultat lu dans mMergeResult au lieu de 256+rang : ici le
				// vocab n'est pas ordonné par rang de fusion).
				const int64 kNoRank = 0x7FFFFFFFFFFFFFFFLL;
				while (seq.Size() >= 2) {
					int64 bestRank = kNoRank;
					int64 bestPos = -1;
					for (int64 i = 0; i + 1 < (int64)seq.Size(); ++i) {
						const int64 r = mMergeRank.Get(MergePairKey(seq[(nk_size)i], seq[(nk_size)(i + 1)]), kNoRank);
						if (r < bestRank) {
							bestRank = r;
							bestPos = i;
						}
					}
					if (bestPos < 0)
						break;
					const int64 merged =
						mMergeResult.Get(MergePairKey(seq[(nk_size)bestPos], seq[(nk_size)(bestPos + 1)]), -1);
					if (merged < 0)
						break; // incohérence rank/result : ne jamais boucler à l'infini
					seq[(nk_size)bestPos] = (int32)merged;
					for (int64 j = bestPos + 1; j + 1 < (int64)seq.Size(); ++j)
						seq[(nk_size)j] = seq[(nk_size)(j + 1)];
					seq.Resize((nk_size)(seq.Size() - 1));
				}

				for (nk_size i = 0; i < seq.Size(); ++i)
					outIds.PushBack(seq[i]);
				return true;
			}

			bool NkQwen2Tokenizer::Encode(const NkString &text, NkVector<int32> &outIds) const {
				if (!IsLoaded())
					return false;
				NkVector<NkString> segments;
				PreTokenize(text, segments);
				for (nk_size s = 0; s < segments.Size(); ++s) {
					// Le remappage octet->codepoint est déjà encapsulé dans
					// mByteTokenId (l'id de base d'un octet EST l'id de la chaîne
					// remappée de cet octet) : EncodeSegment reçoit les octets BRUTS.
					const NkString &seg = segments[s];
					if (!EncodeSegment(seg.Data(), (int64)seg.Size(), outIds))
						return false;
				}
				return true;
			}

			bool NkQwen2Tokenizer::EncodeWithSpecials(const NkString &text, NkVector<int32> &outIds) const {
				if (!IsLoaded())
					return false;
				// Les 3 spéciaux ChatML connus, seulement s'ils existent dans CE vocab.
				const char *names[3] = {"<|endoftext|>", "<|im_start|>", "<|im_end|>"};
				const int32 ids[3] = {mEndOfTextId, mImStartId, mImEndId};

				NkString::SizeType pos = 0;
				while (pos < text.Size()) {
					// Prochaine occurrence LITTÉRALE d'un spécial (la plus proche).
					NkString::SizeType bestAt = NkString::npos;
					int32 bestIdx = -1;
					for (int32 k = 0; k < 3; ++k) {
						if (ids[k] < 0)
							continue;
						const NkString::SizeType at = text.Find(names[k], pos);
						if (at != NkString::npos && (bestAt == NkString::npos || at < bestAt)) {
							bestAt = at;
							bestIdx = k;
						}
					}
					if (bestIdx < 0) {
						// Plus aucun spécial : le reste passe par le BPE normal.
						if (!Encode(text.SubStr(pos), outIds))
							return false;
						return true;
					}
					if (bestAt > pos) {
						if (!Encode(text.SubStr(pos, bestAt - pos), outIds))
							return false;
					}
					outIds.PushBack(ids[bestIdx]);
					NkString name(names[bestIdx]);
					pos = bestAt + name.Size();
				}
				return true;
			}

			// ================= Decode ================================================
			NkString NkQwen2Tokenizer::DecodeOne(int32 id) const {
				NkString out;
				if (id < 0 || (nk_size)id >= mTokens.Size())
					return out; // id hors bornes ignoré (robustesse, comme DecodeAll)
				const NkString &tok = mTokens[(nk_size)id];
				const char *p = tok.Data();
				const int64 n = (int64)tok.Size();
				int64 i = 0;
				while (i < n) {
					uint32 cp = 0;
					const int32 len = Utf8Decode(p + i, n - i, cp);
					if (len <= 0) {
						// Séquence UTF-8 invalide dans le vocab : octet recopié tel quel
						// (aucune perte — condition du round-trip).
						out.Append(p[i]);
						i += 1;
						continue;
					}
					if (cp < (uint32)kCpTableSize && mCpToByte[cp] >= 0) {
						// Codepoint de la table byte-level : dé-remappage vers l'octet
						// d'origine (ex. « Ġ » U+0120 -> espace 0x20).
						out.Append((char)(uint8)mCpToByte[cp]);
					} else {
						// Codepoint hors table (token spécial, token « added ») :
						// recopié tel quel, en UTF-8 d'origine.
						out.Append(p + i, (NkString::SizeType)len);
					}
					i += len;
				}
				return out;
			}

			NkString NkQwen2Tokenizer::Decode(const NkVector<int32> &ids) const {
				NkString out;
				for (nk_size i = 0; i < ids.Size(); ++i)
					out.Append(DecodeOne(ids[i]));
				return out;
			}

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
