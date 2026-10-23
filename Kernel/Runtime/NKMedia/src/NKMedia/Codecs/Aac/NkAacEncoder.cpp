// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKMedia/Codecs/Aac/NkAacEncoder.cpp — le codeur AAC-LC minimal (voir NkAacEncoder.h).
// Transcription du prototype prouve par ffmpeg (tools/prouver_aac_codeur.py, 23/10) : memes formules, memes
// conventions (MDCT x2 de la norme, repliement TDAC, DCT-IV : pre-rotation exp(-i pi (n + 1/4) / M), post-rotation
// exp(-i pi k / M) ; livre 11 : mot de code, PUIS les bits de signe, PUIS les echappements -- l'ordre du decodeur).
// =============================================================================
#include "NKMedia/Codecs/Aac/NkAacEncoder.h"
#include "NKMedia/Codecs/Aac/NkAacHuffman.h"
#include "NKMedia/Codecs/Aac/NkAacTables.h"
#include "NKMedia/Video/NkBitWriter.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {
	namespace media {

		namespace {
			constexpr int32 kN = 2048;
			constexpr int32 kM = 1024;
			constexpr int32 kH = 512;
			constexpr float64 kPi = 3.14159265358979323846;
			constexpr float32 kQMax = 165000.0f; ///< au-dela, une valeur quantifiee depasserait 8191 (echappement)

			float32 gFen[kN];
			float32 gPreRe[kH], gPreIm[kH], gPostRe[kH], gPostIm[kH];
			float32 gTwRe[kH / 2], gTwIm[kH / 2];
			int32 gRev[kH];
			float32 gPas[256]; ///< 2^(-(sf - 100) / 4)
			uint8 gCbLen[17][17];
			uint32 gCbCw[17][17];
			uint8 gSfLen = 0;
			uint32 gSfCw = 0;
			bool gPret = false;

			void Preparer() {
				if (gPret)
					return;
				NkAacTables::BuildWindow(gFen, kN, false); // sinus : celle du decodeur
				for (int32 n = 0; n < kH; ++n) {
					const float64 a = kPi * ((float64)n + 0.25) / (float64)kM;
					gPreRe[n] = (float32)math::NkCos(a);
					gPreIm[n] = (float32)-math::NkSin(a);
					const float64 b = kPi * (float64)n / (float64)kM;
					gPostRe[n] = (float32)math::NkCos(b);
					gPostIm[n] = (float32)-math::NkSin(b);
				}
				for (int32 k = 0; k < kH / 2; ++k) {
					const float64 a = 2.0 * kPi * (float64)k / (float64)kH;
					gTwRe[k] = (float32)math::NkCos(a);
					gTwIm[k] = (float32)-math::NkSin(a);
				}
				for (int32 i = 0; i < kH; ++i) { // 9 bits
					int32 r = 0;
					for (int32 b = 0; b < 9; ++b)
						if (i & (1 << b))
							r |= 1 << (8 - b);
					gRev[i] = r;
				}
				// 2^(-1/4) par multiplications : aucun pow
				const float64 quart = 1.0 / math::NkSqrt(math::NkSqrt(2.0));
				float64 p = 1.0;
				for (int32 sf = 100; sf < 256; ++sf) {
					gPas[sf] = (float32)p;
					p *= quart;
				}
				p = 1.0;
				for (int32 sf = 99; sf >= 0; --sf) {
					p /= quart;
					gPas[sf] = (float32)p;
				}
				// le livre 11 et le code du facteur d'echelle « delta 0 » (indice 60), lus dans les tables du decodeur
				for (int32 a = 0; a < 17; ++a)
					for (int32 b = 0; b < 17; ++b)
						gCbLen[a][b] = 0;
				const NkAacHcbBook &cb = kAacHcbBooks[11];
				for (int32 i = 0; i < cb.count; ++i) {
					const NkAacHcbEntry &e = cb.entries[i];
					if (e.v[0] >= 0 && e.v[0] < 17 && e.v[1] >= 0 && e.v[1] < 17) {
						gCbLen[e.v[0]][e.v[1]] = e.len;
						gCbCw[e.v[0]][e.v[1]] = e.cw;
					}
				}
				const NkAacHcbBook &sf = kAacHcbBooks[0];
				for (int32 i = 0; i < sf.count; ++i)
					if (sf.entries[i].v[0] == 60) {
						gSfLen = sf.entries[i].len;
						gSfCw = sf.entries[i].cw;
					}
				gPret = true;
			}

			void Fft512(float32 *re, float32 *im) {
				for (int32 i = 0; i < kH; ++i)
					if (gRev[i] > i) {
						const float32 tr = re[i], ti = im[i];
						re[i] = re[gRev[i]], im[i] = im[gRev[i]];
						re[gRev[i]] = tr, im[gRev[i]] = ti;
					}
				for (int32 len = 2; len <= kH; len <<= 1) {
					const int32 pas = kH / len;
					for (int32 debut = 0; debut < kH; debut += len)
						for (int32 j = 0; j < len / 2; ++j) {
							const float32 wr = gTwRe[j * pas], wi = gTwIm[j * pas];
							const int32 a = debut + j, b = a + len / 2;
							const float32 xr = re[b] * wr - im[b] * wi, xi = re[b] * wi + im[b] * wr;
							re[b] = re[a] - xr, im[b] = im[a] - xi;
							re[a] += xr, im[a] += xi;
						}
				}
			}

			/// X[k] = f * somme x[n] w[n] cos(pi/M (n + 1/2 + M/2)(k + 1/2)), f = 2 (la norme)
			void Mdct(const float32 *x, float32 *X, float32 f) {
				float32 v[kM];
				for (int32 n = 0; n < kM / 2; ++n)
					v[n] = -x[3 * kM / 2 - 1 - n] * gFen[3 * kM / 2 - 1 - n] - x[3 * kM / 2 + n] * gFen[3 * kM / 2 + n];
				for (int32 n = kM / 2; n < kM; ++n)
					v[n] = x[n - kM / 2] * gFen[n - kM / 2] - x[3 * kM / 2 - 1 - n] * gFen[3 * kM / 2 - 1 - n];
				float32 re[kH], im[kH];
				for (int32 n = 0; n < kH; ++n) {
					const float32 a = v[2 * n], b = v[kM - 1 - 2 * n];
					re[n] = a * gPreRe[n] - b * gPreIm[n];
					im[n] = a * gPreIm[n] + b * gPreRe[n];
				}
				Fft512(re, im);
				for (int32 k = 0; k < kH; ++k) {
					const float32 yr = re[k] * gPostRe[k] - im[k] * gPostIm[k];
					const float32 yi = re[k] * gPostIm[k] + im[k] * gPostRe[k];
					X[2 * k] = f * yr;
					X[kM - 1 - 2 * k] = -f * yi;
				}
			}

			void Quantifier(const float32 *c, int32 n, int32 sf, int32 *q) {
				const float32 g = gPas[sf];
				for (int32 i = 0; i < n; ++i) {
					const float32 a = (c[i] < 0.f ? -c[i] : c[i]) * g;
					int32 m = (int32)(math::NkSqrt(a) * math::NkSqrt(math::NkSqrt(a)) + 0.4054f);
					if (m > 8191)
						m = 8191;
					q[i] = c[i] < 0.f ? -m : m;
				}
			}

			int32 Spectre(NkBitWriter *w, const int32 *q, int32 n) {
				int32 total = 0;
				for (int32 i = 0; i + 1 < n; i += 2) {
					const int32 y = q[i], z = q[i + 1];
					const int32 ay = y < 0 ? -y : y, az = z < 0 ? -z : z;
					const int32 iy = ay > 16 ? 16 : ay, iz = az > 16 ? 16 : az;
					total += gCbLen[iy][iz];
					if (w)
						w->PutBits(gCbCw[iy][iz], gCbLen[iy][iz]);
					const int32 v2[2] = {y, z};
					for (int32 d = 0; d < 2; ++d)
						if (v2[d] != 0) {
							++total;
							if (w)
								w->PutBits(v2[d] < 0 ? 1u : 0u, 1);
						}
					for (int32 d = 0; d < 2; ++d) {
						const int32 a = v2[d] < 0 ? -v2[d] : v2[d];
						if (a < 16)
							continue;
						int32 e = 4;
						while ((a >> (e + 1)) != 0)
							++e;
						total += (e - 4) + 1 + e;
						if (w) {
							if (e > 4)
								w->PutBits((1u << (e - 4)) - 1u, e - 4);
							w->PutBits(0, 1);
							w->PutBits((uint32)(a - (1 << e)), e);
						}
					}
				}
				return total;
			}

			/// Un individual_channel_stream (common_window : sans ics_info) ; rend le nombre de bits.
			int32 Canal(NkBitWriter *w, const int32 *q, int32 sf, int32 maxSfb, const uint16 *swb) {
				int32 total = 8;
				if (w)
					w->PutBits((uint32)sf, 8); // global_gain
				uint8 livre[64];
				for (int32 b = 0; b < maxSfb; ++b) {
					livre[b] = 0;
					for (int32 i = swb[b]; i < swb[b + 1]; ++i)
						if (q[i] != 0) {
							livre[b] = 11;
							break;
						}
				}
				for (int32 b = 0; b < maxSfb;) { // section_data
					const uint8 cb = livre[b];
					int32 n = 1;
					while (b + n < maxSfb && livre[b + n] == cb)
						++n;
					total += 4;
					if (w)
						w->PutBits(cb, 4);
					int32 r = n;
					while (r >= 31) {
						total += 5;
						if (w)
							w->PutBits(31, 5);
						r -= 31;
					}
					total += 5;
					if (w)
						w->PutBits((uint32)r, 5);
					b += n;
				}
				for (int32 b = 0; b < maxSfb; ++b) // scale_factor_data : delta 0 (le facteur = global_gain)
					if (livre[b]) {
						total += gSfLen;
						if (w)
							w->PutBits(gSfCw, gSfLen);
					}
				total += 3; // pulse, tns, gain_control : absents
				if (w) {
					w->PutBits(0, 1);
					w->PutBits(0, 1);
					w->PutBits(0, 1);
				}
				for (int32 b = 0; b < maxSfb; ++b)
					if (livre[b])
						total += Spectre(w, q + swb[b], swb[b + 1] - swb[b]);
				return total;
			}
		} // namespace

		int32 NkAacEncoder::AudioSpecificConfig(int32 sampleRate, uint8 out[2]) {
			const int32 idx = NkAacTables::SampleRateIndex(sampleRate);
			if (idx < 0)
				return 0;
			// objet 2 (AAC LC) sur 5 bits, indice de frequence sur 4, 2 canaux sur 4, puis 3 bits nuls
			const uint32 v = (2u << 11) | ((uint32)idx << 7) | (2u << 3);
			out[0] = (uint8)(v >> 8);
			out[1] = (uint8)(v & 0xFF);
			return 2;
		}

		bool NkAacEncoder::Open(int32 sampleRate, int32 debit) {
			Preparer();
			mSfIndex = NkAacTables::SampleRateIndex(sampleRate);
			if (sampleRate != 44100 || mSfIndex < 0 || debit <= 0 || !gSfLen)
				return false;
			mSwb = NkAacTables::SwbOffsetLong(mSfIndex);
			const int32 nb = NkAacTables::NumSwbLong(mSfIndex);
			// jusqu'a ~16 kHz (743 / 1024 de la bande) : au-dela, peu pour la voix, beaucoup de bits
			mMaxSfb = 0;
			while (mMaxSfb < nb && mSwb[mMaxSfb + 1] <= 744)
				++mMaxSfb;
			mBudget = (int32)((int64)debit * 1024 / sampleRate);
			mOuvert = true;
			return true;
		}

		bool NkAacEncoder::EncodeFrame(const float32 *gauche, const float32 *droite, NkVector<uint8> &trame) {
			trame.Clear();
			if (!mOuvert || !gauche || !droite)
				return false;
			float32 mid[kN], cote[kN];
			for (int32 i = 0; i < kN; ++i) {
				mid[i] = (gauche[i] + droite[i]) * 0.5f;
				cote[i] = (gauche[i] - droite[i]) * 0.5f;
			}
			const float32 f = mutationSansFacteurDeux ? 1.f : 2.f;
			Mdct(mid, mM, f);
			Mdct(cote, mS, f);
			const int32 haut = mSwb[mMaxSfb];
			for (int32 k = haut; k < kM; ++k)
				mM[k] = mS[k] = 0.f;
			float32 pic = 1e-9f;
			for (int32 k = 0; k < haut; ++k) {
				const float32 a = mM[k] < 0.f ? -mM[k] : mM[k], b = mS[k] < 0.f ? -mS[k] : mS[k];
				pic = a > pic ? a : pic;
				pic = b > pic ? b : pic;
			}
			// le plus petit facteur qui garde chaque valeur sous 8191, puis le plus fin qui tient dans le budget
			int32 bas = 0;
			while (bas < 255 && pic * gPas[bas] > kQMax)
				++bas;
			const int32 entete = 3 + 4 + 1 + (1 + 2 + 1 + 6 + 1) + 2 + 3;
			int32 hautSf = 255;
			while (bas < hautSf) {
				const int32 sf = (bas + hautSf) / 2;
				Quantifier(mM, haut, sf, mQ);
				int32 n = entete + Canal(nullptr, mQ, sf, mMaxSfb, mSwb);
				Quantifier(mS, haut, sf, mQ);
				n += Canal(nullptr, mQ, sf, mMaxSfb, mSwb);
				if (n <= mBudget)
					hautSf = sf;
				else
					bas = sf + 1;
			}
			const int32 sf = bas;
			mDernierFacteur = sf;
			NkBitWriter w;
			w.PutBits(1, 3); // ID_CPE
			w.PutBits(0, 4); // element_instance_tag
			w.PutBits(1, 1); // common_window
			w.PutBits(0, 1); // ics_reserved_bit
			w.PutBits(0, 2); // ONLY_LONG_SEQUENCE
			w.PutBits(0, 1); // fenetre sinus
			w.PutBits((uint32)mMaxSfb, 6);
			w.PutBits(0, 1); // predictor_data_present
			w.PutBits(2, 2); // ms_mask_present = 2 : M/S sur toutes les bandes
			Quantifier(mM, haut, sf, mQ);
			for (int32 k = haut; k < kM; ++k)
				mQ[k] = 0;
			Canal(&w, mQ, sf, mMaxSfb, mSwb);
			Quantifier(mS, haut, sf, mQ);
			Canal(&w, mQ, sf, mMaxSfb, mSwb);
			w.PutBits(7, 3); // ID_END
			w.AlignByteZero();
			trame.Resize(w.Size());
			for (usize i = 0; i < w.Size(); ++i)
				trame[i] = w.Data()[i];
			return true;
		}

		float64 NkAacEncoder::SelfTestMdct() {
			Preparer();
			float32 x[kN], X[kM];
			uint32 graine = 12345u;
			for (int32 n = 0; n < kN; ++n) {
				graine = graine * 1103515245u + 12345u;
				x[n] = (float32)((int32)((graine >> 16) & 0x7FFF) - 16384);
			}
			Mdct(x, X, 2.f);
			float64 ecart = 0.0, ref = 1e-9;
			for (int32 k = 0; k < kM; k += 37) { // un echantillon des sorties : la formule directe coute N par sortie
				float64 s = 0.0;
				for (int32 n = 0; n < kN; ++n)
					s += (float64)x[n] * (float64)gFen[n] *
						 math::NkCos(kPi / (float64)kM * ((float64)n + 0.5 + (float64)kM / 2.0) * ((float64)k + 0.5));
				s *= 2.0;
				const float64 d = s - (float64)X[k];
				ecart = (d < 0 ? -d : d) > ecart ? (d < 0 ? -d : d) : ecart;
				ref = (s < 0 ? -s : s) > ref ? (s < 0 ? -s : s) : ref;
			}
			return ecart / ref;
		}
	} // namespace media
} // namespace nkentseu
