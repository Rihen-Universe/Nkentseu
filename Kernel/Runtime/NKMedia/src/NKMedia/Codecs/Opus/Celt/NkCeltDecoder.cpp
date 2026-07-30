// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltDecoder.cpp — orchestration CELT (celt_decode).
// Chemin MONO complet : flags + coarse/fine energy + tf_decode + spread + caps +
// dynalloc + compute_allocation (décodage) + quant_all_bands + anti_collapse +
// finalise + denormalise + IMDCT CELT (DFT directe) + overlap-add + deemphasis.
// Algorithmes RFC 6716 réécrits à la sauce Nkentseu (aucun code importé). Zero-STL.
// =============================================================================
#include "NKMedia/Codecs/Opus/Celt/NkCeltDecoder.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltDeemphasis.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltEnergy.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltAlloc.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltQuantBands.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltAntiCollapse.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltDenorm.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltBands.h"
#include "NKMemory/NKMemory.h"

#include <cmath> // cosf, sinf

namespace nkentseu {
	namespace media {

		namespace {
			constexpr int32 BITRES = 3;
			constexpr int32 kNbEBands = 21;
			constexpr int32 kMaxLM = 3;
			constexpr int32 kOverlap = 120;
			constexpr int32 FINE_OFFSET = 21;
			constexpr int32 MAX_FINE_BITS = 8;
			constexpr float64 kPi = 3.14159265358979323846;
			constexpr float32 kMinEnergyDb = -28.0f;

			// Tables constantes du format RFC 6716.
			const uint8 kTapsetIcdf[3] = {2, 1, 0};
			const uint8 kSpreadIcdf[4] = {25, 23, 2, 0};
			const uint8 kTrimIcdf[11] = {126, 124, 119, 109, 87, 41, 19, 9, 4, 2, 0};
			const int16 kLogN[21] = {0, 0, 0, 0, 0,  0,  0,  0,  8,  8, 8,
									 8, 16, 16, 16, 21, 21, 24, 29, 34, 36};
			const int32 kTfSelect[4][8] = {
				{0, -1, 0, -1, 0, -1, 0, -1},
				{0, -1, 0, -2, 1, 0, 1, -1},
				{0, -2, 0, -3, 2, 0, 1, -1},
				{0, -2, 0, -3, 3, 0, 1, -1},
			};
			// cache.caps (libopus static_modes) : 21 bandes × 8 (2*LM+C-1).
			const uint8 kCacheCaps[168] = {
				224, 224, 224, 224, 224, 224, 224, 224, 160, 160, 160, 160, 185, 185, 185, 178, 178, 168, 134, 61,	37,
				224, 224, 224, 224, 224, 224, 224, 224, 240, 240, 240, 240, 207, 207, 207, 198, 198, 183, 144, 66,	40,
				160, 160, 160, 160, 160, 160, 160, 160, 185, 185, 185, 185, 193, 193, 193, 183, 183, 172, 138, 64,	38,
				240, 240, 240, 240, 240, 240, 240, 240, 207, 207, 207, 207, 204, 204, 204, 193, 193, 180, 143, 66,	40,
				185, 185, 185, 185, 185, 185, 185, 185, 193, 193, 193, 193, 193, 193, 193, 183, 183, 172, 138, 65,	39,
				207, 207, 207, 207, 207, 207, 207, 207, 204, 204, 204, 204, 201, 201, 201, 188, 188, 176, 141, 66,	40,
				193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 194, 194, 194, 184, 184, 173, 139, 65,	39,
				204, 204, 204, 204, 204, 204, 204, 204, 201, 201, 201, 201, 198, 198, 198, 187, 187, 175, 140, 66,	40};

			int32 IMax(int32 a, int32 b) {
				return a > b ? a : b;
			}
			int32 IMin(int32 a, int32 b) {
				return a < b ? a : b;
			}

			// --- tf_decode (celt_decoder.c) : résolution temps/fréquence par bande. ---
			void TfDecode(NkOpusRangeDecoder &dec, int32 start, int32 end, int32 isTransient, int32 *tfRes, int32 LM,
						  int32 lenBytes) {
				const uint32 budget0 = (uint32)lenBytes * 8u;
				uint32 budget = budget0;
				uint32 tell = (uint32)dec.Tell();
				int32 logp = isTransient ? 2 : 4;
				const int32 tfSelectRsv = (LM > 0 && tell + (uint32)logp + 1u <= budget) ? 1 : 0;
				budget -= (uint32)tfSelectRsv;
				int32 tfChanged = 0, curr = 0;
				for (int32 i = start; i < end; ++i) {
					if (tell + (uint32)logp <= budget) {
						curr ^= dec.DecodeBitLogp((uint32)logp);
						tell = (uint32)dec.Tell();
						tfChanged |= curr;
					}
					tfRes[i] = curr;
					logp = isTransient ? 4 : 5;
				}
				int32 tfSelect = 0;
				if (tfSelectRsv && kTfSelect[LM][4 * isTransient + 0 + tfChanged] !=
									   kTfSelect[LM][4 * isTransient + 2 + tfChanged])
					tfSelect = dec.DecodeBitLogp(1);
				for (int32 i = start; i < end; ++i)
					tfRes[i] = kTfSelect[LM][4 * isTransient + 2 * tfSelect + tfRes[i]];
			}

			// --- init_caps (rate.c) : plafond de bits par bande. ---
			void InitCaps(int32 *cap, int32 LM, int32 C, const int16 *eBands) {
				for (int32 i = 0; i < kNbEBands; ++i) {
					const int32 N = ((int32)eBands[i + 1] - (int32)eBands[i]) << LM;
					cap[i] = ((int32)kCacheCaps[kNbEBands * (2 * LM + C - 1) + i] + 64) * C * N >> 2;
				}
			}

			// LOG2_FRAC_TABLE (rate.c) : coût en 1/8 bit du paramètre intensity selon le nombre
			// de bandes candidates.
			const uint8 kLog2FracTable[24] = {0,  8,  13, 16, 19, 21, 23, 24, 26, 27, 28, 29,
											  30, 31, 32, 32, 33, 34, 34, 35, 36, 36, 37, 37};

			// --- compute_allocation (décodage, rate.c interp_bits2pulses) — mono + stéréo. ---
			// Renvoie codedBands ; remplit pulses (budget PVQ frac), fineQuant, finePriority ; met à jour
			// balance ; en stéréo (C==2) décode aussi intensity et dualStereo.
			int32 ComputeAllocation(NkOpusRangeDecoder &dec, int32 start, int32 end, const int32 *offsets,
									const int32 *cap, int32 allocTrim, int32 totalFrac, int32 LM, int32 C,
									const int16 *eBands, int32 *pulses, int32 *fineQuant, int32 *finePriority,
									int32 *balanceOut, int32 *intensityOut, int32 *dualStereoOut) {
				const int32 allocFloor = C << BITRES;
				const int32 ALLOC_STEPS = 6;
				const int32 logM = LM << BITRES;
				const int32 stereo = (C == 2) ? 1 : 0;

				int32 total = IMax(totalFrac, 0);
				int32 skipStart = start;
				const int32 skipRsv = total >= (1 << BITRES) ? (1 << BITRES) : 0;
				total -= skipRsv;
				// Réservation des paramètres intensity + dual stereo (C==2 seulement).
				int32 intensityRsv = 0, dualStereoRsv = 0;
				if (C == 2) {
					intensityRsv = (int32)kLog2FracTable[end - start];
					if (intensityRsv > total) {
						intensityRsv = 0;
					} else {
						total -= intensityRsv;
						dualStereoRsv = total >= (1 << BITRES) ? (1 << BITRES) : 0;
						total -= dualStereoRsv;
					}
				}

				int32 bits1[kNbEBands] = {0}, bits2[kNbEBands] = {0}, thresh[kNbEBands] = {0}, trimOff[kNbEBands] = {0};
				NkCeltAlloc::BuildInterp(start, end, offsets, cap, allocTrim, total, C, LM, bits1, bits2, thresh,
										 trimOff);
				if (offsets)
					for (int32 j = start; j < end; ++j)
						if (offsets[j] > 0)
							skipStart = j;

				int32 bits[kNbEBands] = {0};

				// Bissection sur le coefficient d'interpolation.
				int32 lo = 0, hi = 1 << ALLOC_STEPS;
				for (int32 i = 0; i < ALLOC_STEPS; ++i) {
					const int32 mid = (lo + hi) >> 1;
					int32 psum = 0, done = 0;
					for (int32 j = end; j-- > start;) {
						const int32 tmp = bits1[j] + (int32)(((int64)mid * bits2[j]) >> ALLOC_STEPS);
						if (tmp >= thresh[j] || done) {
							done = 1;
							psum += IMin(tmp, cap[j]);
						} else if (tmp >= allocFloor)
							psum += allocFloor;
					}
					if (psum > total)
						hi = mid;
					else
						lo = mid;
				}
				int32 psum = 0, done = 0;
				for (int32 j = end; j-- > start;) {
					int32 tmp = bits1[j] + (int32)(((int64)lo * bits2[j]) >> ALLOC_STEPS);
					if (tmp < thresh[j] && !done)
						tmp = (tmp >= allocFloor) ? allocFloor : 0;
					else
						done = 1;
					tmp = IMin(tmp, cap[j]);
					bits[j] = tmp;
					psum += tmp;
				}

				// Décision de bandes ignorées (skip), depuis la fin.
				int32 codedBands = end;
				for (;; codedBands--) {
					const int32 j = codedBands - 1;
					if (j <= skipStart) {
						total += skipRsv;
						break;
					}
					int32 left = total - psum;
					const int32 den0 = (int32)eBands[codedBands] - (int32)eBands[start];
					int32 percoeff = den0 > 0 ? left / den0 : 0;
					left -= den0 * percoeff;
					const int32 rem = IMax(left - ((int32)eBands[j] - (int32)eBands[start]), 0);
					const int32 bandWidth = (int32)eBands[codedBands] - (int32)eBands[j];
					int32 bandBits = bits[j] + percoeff * bandWidth + rem;
					if (bandBits >= IMax(thresh[j], allocFloor + (1 << BITRES))) {
						if (dec.DecodeBitLogp(1))
							break;
						psum += 1 << BITRES;
						bandBits -= 1 << BITRES;
					}
					// Récupère les bits de cette bande + réajuste la réservation intensity
					// (moins de bandes candidates → coût plus faible).
					psum -= bits[j] + intensityRsv;
					if (intensityRsv > 0)
						intensityRsv = (int32)kLog2FracTable[j - start];
					psum += intensityRsv;
					if (bandBits >= allocFloor) {
						psum += allocFloor;
						bits[j] = allocFloor;
					} else {
						bits[j] = 0;
					}
				}

				// Décodage des paramètres intensity et dual stereo.
				int32 intensity = 0, dualStereo = 0;
				if (intensityRsv > 0)
					intensity = start + (int32)dec.DecodeUint((uint32)(codedBands + 1 - start));
				if (intensity <= start) {
					total += dualStereoRsv;
					dualStereoRsv = 0;
				}
				if (dualStereoRsv > 0)
					dualStereo = dec.DecodeBitLogp(1);
				if (intensityOut)
					*intensityOut = intensity;
				if (dualStereoOut)
					*dualStereoOut = dualStereo;

				// Répartition des bits restants.
				int32 left = total - psum;
				const int32 den1 = (int32)eBands[codedBands] - (int32)eBands[start];
				int32 percoeff = den1 > 0 ? left / den1 : 0;
				left -= den1 * percoeff;
				for (int32 j = start; j < codedBands; ++j)
					bits[j] += percoeff * ((int32)eBands[j + 1] - (int32)eBands[j]);
				for (int32 j = start; j < codedBands; ++j) {
					const int32 tmp = IMin(left, (int32)eBands[j + 1] - (int32)eBands[j]);
					bits[j] += tmp;
					left -= tmp;
				}

				// Split PVQ / énergie fine + priorité + rééquilibrage.
				int32 balance = 0;
				int32 j = start;
				for (; j < codedBands; ++j) {
					const int32 N0 = (int32)eBands[j + 1] - (int32)eBands[j];
					const int32 N = N0 << LM;
					const int32 bit = bits[j] + balance;
					int32 excess;
					if (N > 1) {
						excess = IMax(bit - cap[j], 0);
						bits[j] = bit - excess;
						// Compense le degré de liberté supplémentaire du split stéréo (l'angle θ).
						const int32 den = C * N + ((C == 2 && N > 2 && !dualStereo && j < intensity) ? 1 : 0);
						const int32 NClogN = den * ((int32)kLogN[j] + logM);
						int32 offset = (NClogN >> 1) - den * FINE_OFFSET;
						if (N == 2)
							offset += den << BITRES >> 2;
						if (bits[j] + offset < den * 2 << BITRES)
							offset += NClogN >> 2;
						else if (bits[j] + offset < den * 3 << BITRES)
							offset += NClogN >> 3;
						int32 eb = IMax(0, bits[j] + offset + (den << (BITRES - 1)));
						eb = (eb / den) >> BITRES;
						if (C * eb > (bits[j] >> BITRES))
							eb = bits[j] >> stereo >> BITRES;
						eb = IMin(eb, MAX_FINE_BITS);
						fineQuant[j] = eb;
						finePriority[j] = (eb * (den << BITRES) >= bits[j] + offset) ? 1 : 0;
						bits[j] -= C * eb << BITRES;
					} else {
						excess = IMax(0, bit - (C << BITRES));
						bits[j] = bit - excess;
						fineQuant[j] = 0;
						finePriority[j] = 1;
					}
					if (excess > 0) {
						const int32 extraFine = IMin(excess >> BITRES, MAX_FINE_BITS - fineQuant[j]);
						fineQuant[j] += extraFine;
						const int32 extraBits = extraFine * C << BITRES;
						finePriority[j] = (extraBits >= excess - balance) ? 1 : 0;
						excess -= extraBits;
					}
					balance = excess;
				}
				*balanceOut = balance;
				for (; j < end; ++j) {
					fineQuant[j] = bits[j] >> stereo >> BITRES;
					bits[j] = 0;
					finePriority[j] = (fineQuant[j] < 1) ? 1 : 0;
				}
				for (int32 k = start; k < end; ++k)
					pulses[k] = bits[k];
				return codedBands;
			}

			// --- unquant_energy_finalise (quant_bands.c) : consomme les bits restants. ---
			void EnergyFinalise(NkOpusRangeDecoder &dec, float32 *oldEBands, const int32 *fineQuant,
								const int32 *finePriority, int32 start, int32 end, int32 bitsLeft, int32 C) {
				for (int32 prio = 0; prio < 2; ++prio) {
					for (int32 i = start; i < end && bitsLeft >= C; ++i) {
						if (fineQuant[i] >= MAX_FINE_BITS || finePriority[i] != prio)
							continue;
						for (int32 c = 0; c < C; ++c) {
							const int32 q2 = (int32)dec.DecodeBits(1);
							const float32 offset = ((float32)q2 - 0.5f) * (float32)(1 << (14 - fineQuant[i] - 1)) *
												   (1.0f / 16384.0f);
							oldEBands[i + c * kNbEBands] += offset;
							bitsLeft--;
						}
					}
				}
			}

			// Fenêtre MDCT CELT (identique au repli TDAC de l'IMDCT) : window(i) =
			// sin(π/2 · sin²(π/2 · (i+.5)/overlap)). Utilisée par le crossfade du post-filtre.
			inline float32 CeltWindow(int32 i, int32 overlap) {
				const double s = ::sin(0.5 * kPi * ((double)i + 0.5) / (double)overlap);
				return (float32)::sin(0.5 * kPi * s * s);
			}

			// --- POST-FILTRE CELT (comb_filter, celt.c) : filtre en peigne accordé sur le
			// pitch qui renforce la périodicité. `x` peut = `y` (in-place). Lit l'historique
			// x[i-T] EN AMONT de x[0] (présent dans le buffer glissant). Sur `overlap`
			// échantillons, fond l'ancien filtre (T0,g0,tapset0) vers le nouveau (T1,g1,tapset1)
			// via la fenêtre ; le reste utilise le filtre constant (T1,g1). Port du chemin float
			// (SHL32 = no-op, MULT_COEF* = multiplication). Sans lui, la sortie CELT est ~2.8×
			// trop faible (le deemphasis, LTI, amplifie moins un signal non enrichi en basses).
			void CombFilter(float32 *y, const float32 *x, int32 T0, int32 T1, int32 N, float32 g0, float32 g1,
							int32 tapset0, int32 tapset1, int32 overlap) {
				static const float32 kCombGains[3][3] = {
					{0.3066406250f, 0.2170410156f, 0.1296386719f},
					{0.4638671875f, 0.2680664062f, 0.0f},
					{0.7998046875f, 0.1000976562f, 0.0f}};
				const int32 kMinPeriod = 15; // COMBFILTER_MINPERIOD
				if (g0 == 0.0f && g1 == 0.0f) {
					if (x != y)
						for (int32 i = 0; i < N; ++i)
							y[i] = x[i];
					return;
				}
				if (T0 < kMinPeriod)
					T0 = kMinPeriod;
				if (T1 < kMinPeriod)
					T1 = kMinPeriod;
				const float32 g00 = g0 * kCombGains[tapset0][0], g01 = g0 * kCombGains[tapset0][1],
							  g02 = g0 * kCombGains[tapset0][2];
				const float32 g10 = g1 * kCombGains[tapset1][0], g11 = g1 * kCombGains[tapset1][1],
							  g12 = g1 * kCombGains[tapset1][2];
				float32 x1 = x[-T1 + 1], x2 = x[-T1], x3 = x[-T1 - 1], x4 = x[-T1 - 2];
				int32 ov = overlap;
				if (g0 == g1 && T0 == T1 && tapset0 == tapset1)
					ov = 0; // filtre inchangé → pas de crossfade
				int32 i = 0;
				for (; i < ov; ++i) {
					const float32 x0 = x[i - T1 + 2];
					const float32 fw = CeltWindow(i, overlap);
					const float32 f = fw * fw;
					y[i] = x[i] + (1.0f - f) * g00 * x[i - T0] +
						   (1.0f - f) * g01 * (x[i - T0 + 1] + x[i - T0 - 1]) +
						   (1.0f - f) * g02 * (x[i - T0 + 2] + x[i - T0 - 2]) + f * g10 * x2 +
						   f * g11 * (x1 + x3) + f * g12 * (x0 + x4);
					x4 = x3;
					x3 = x2;
					x2 = x1;
					x1 = x0;
				}
				if (g1 == 0.0f) {
					if (x != y)
						for (; i < N; ++i)
							y[i] = x[i];
					return;
				}
				// Partie à filtre constant (comb_filter_const).
				for (; i < N; ++i)
					y[i] = x[i] + g10 * x[i - T1] + g11 * (x[i - T1 + 1] + x[i - T1 - 1]) +
						   g12 * (x[i - T1 + 2] + x[i - T1 - 2]);
			}

			// --- IMDCT CELT (clt_mdct_backward, DFT directe à la place du kiss_fft). ---
			// Reproduit exactement pré-rotation → FFT → post-rotation → repli TDAC fenêtré.
			// `in` : N2 coefficients (accès avec `stride`) ; `out` : écrit out[0..N2+overlap/2).
			// Le recouvrement inter-trame se fait via out[0..overlap/2) = queue de la trame
			// précédente (déjà présente dans le buffer glissant), mélangée par le repli.
			void MdctBackward(const float32 *in, float32 *out, int32 shift, int32 stride) {
				const int32 Nfull = 1920 >> shift;
				const int32 N2 = Nfull >> 1;
				const int32 N4 = Nfull >> 2;
				const int32 ov = kOverlap;
				const int32 base = ov >> 1;
				// trig(i) = cos(2π(i+.125)/Nfull).
				auto Trig = [Nfull](int32 i) -> float32 {
					return (float32)::cos(2.0 * kPi * ((double)i + 0.125) / (double)Nfull);
				};

				// Pré-rotation (ordre naturel, DFT directe → pas de bit-reverse).
				float32 fr[480], fi[480];
				for (int32 i = 0; i < N4; ++i) {
					const float32 x1 = in[stride * (2 * i)];
					const float32 x2 = in[stride * (N2 - 1 - 2 * i)];
					const float32 ti = Trig(i);
					const float32 tn = Trig(N4 + i);
					const float32 yr = x2 * ti + x1 * tn;
					const float32 yi = x1 * ti - x2 * tn;
					fr[i] = yi; // swap re/imag (on utilise une FFT et non une IFFT)
					fi[i] = yr;
				}
				// FFT directe de taille N4 : F[k] = Σ f[n] · e^{-2πi nk/N4}.
				float32 Fr[480], Fi[480];
				for (int32 k = 0; k < N4; ++k) {
					double sr = 0.0, si = 0.0;
					for (int32 n = 0; n < N4; ++n) {
						const double ang = -2.0 * kPi * (double)n * (double)k / (double)N4;
						const double c = ::cos(ang), s = ::sin(ang);
						sr += (double)fr[n] * c - (double)fi[n] * s;
						si += (double)fr[n] * s + (double)fi[n] * c;
					}
					Fr[k] = (float32)sr;
					Fi[k] = (float32)si;
				}
				// Écrit F dans out (paires complexes) à partir de base.
				for (int32 k = 0; k < N4; ++k) {
					out[base + 2 * k] = Fr[k];
					out[base + 2 * k + 1] = Fi[k];
				}
				// Post-rotation + dé-mélange depuis les deux bouts.
				{
					float32 *yp0 = out + base;
					float32 *yp1 = out + base + N2 - 2;
					for (int32 i = 0; i < ((N4 + 1) >> 1); ++i) {
						float32 re = yp0[1], im = yp0[0];
						float32 t0 = Trig(i), t1 = Trig(N4 + i);
						float32 yr = re * t0 + im * t1;
						float32 yi = re * t1 - im * t0;
						float32 re2 = yp1[1], im2 = yp1[0];
						yp0[0] = yr;
						yp1[1] = yi;
						t0 = Trig(N4 - i - 1);
						t1 = Trig(N2 - i - 1);
						yr = re2 * t0 + im2 * t1;
						yi = re2 * t1 - im2 * t0;
						yp1[0] = yr;
						yp0[1] = yi;
						yp0 += 2;
						yp1 -= 2;
					}
				}
				// Repli TDAC fenêtré sur les deux bords → out[0..overlap).
				// window(i) = sin(π/2 · sin²(π/2 · (i+.5)/overlap)).
				auto Win = [ov](int32 i) -> float32 {
					const double s = ::sin(0.5 * kPi * ((double)i + 0.5) / (double)ov);
					return (float32)::sin(0.5 * kPi * s * s);
				};
				{
					float32 *xp1 = out + ov - 1;
					float32 *yp1 = out;
					for (int32 i = 0; i < ov / 2; ++i) {
						const float32 x1 = *xp1;
						const float32 x2 = *yp1;
						const float32 w1 = Win(i);
						const float32 w2 = Win(ov - 1 - i);
						*yp1++ = x2 * w2 - x1 * w1;
						*xp1-- = x2 * w1 + x1 * w2;
					}
				}
			}
		} // namespace

		void NkCeltDecoder::Init(int32 C) {
			mC = (C < 1) ? 1 : (C > kMaxChannels ? kMaxChannels : C);
			for (int32 i = 0; i < kNumBands * kMaxChannels; ++i) {
				// libopus : l'état est zéro-initialisé (OPUS_CLEAR) → oldBandE = 0 ; seuls
				// oldLogE/oldLogE2 sont mis à -28 dB. Le -28 sur oldBandE ne s'applique qu'aux
				// trames SILENCE (géré séparément). Mettre -28 ici décalait TOUTES les énergies
				// des trames inter de coef·(-9) (ex. -4.5 dB à LM=3) → sortie ~22× fausse.
				mOldEBands[i] = 0.0f;
				mOldLogE[i] = kMinEnergyDb;
				mOldLogE2[i] = kMinEnergyDb;
			}
			for (int32 c = 0; c < kMaxChannels; ++c)
				mPreemphMem[c] = 0.0f;
			for (int32 i = 0; i < (kDecBufSize + kOverlap) * kMaxChannels; ++i)
				mDecodeMem[i] = 0.0f;
			mRng = 0;
			mPostfilterPeriod = mPostfilterPeriodOld = 0;
			mPostfilterGain = mPostfilterGainOld = 0.0f;
			mPostfilterTapset = mPostfilterTapsetOld = 0;
			mInit = true;
		}

		bool NkCeltDecoder::DecodeFrame(const uint8 *data, int32 len, int32 LM, float32 *pcm, NkFrameFlags *outFlags,
										int32 endBand) {
			if (data == nullptr || len <= 0)
				return false;
			if (endBand < 1 || endBand > kNumBands)
				endBand = kNumBands;
			NkOpusRangeDecoder dec;
			dec.Init(data, (uint32)len);
			return DecodeShared(dec, len, LM, 0, endBand, pcm, false, outFlags);
		}

		bool NkCeltDecoder::DecodeShared(NkOpusRangeDecoder &dec, int32 len, int32 LM, int32 start, int32 end,
										 float32 *pcm, bool accum, NkFrameFlags *outFlags) {
			if (!mInit)
				Init(mC);
			if (pcm == nullptr || LM < 0 || LM > 3)
				return false;

			const int32 C = mC;
			const int32 M = 1 << LM;
			const int32 N = M * kShortMdctSize; // = shortMdctSize<<LM
			const int32 totalBitsRaw = len * 8;
			const int16 *eBands = NkCeltBands::Eband5ms();

			// --- SILENCE ---
			bool silence;
			const int32 tell0 = dec.Tell();
			if (tell0 >= totalBitsRaw)
				silence = true;
			else if (tell0 == 1)
				silence = dec.DecodeBitLogp(15) != 0;
			else
				silence = false;

			bool transient = false;
			bool intra = false;

			// Params du post-filtre décodés cette trame (0 = inactif). Appliqués après synthèse.
			int32 pfPitch = 0;
			float32 pfGain = 0.0f;
			int32 pfTapset = 0;

			if (!silence) {
				// Post-filtre (comb filter pitch). Uniquement en CELT-only (start==0) ; en
				// hybride il n'est pas transmis. On décode ET on applique (cf. CombFilter plus bas).
				if (start == 0 && dec.Tell() + 16 <= totalBitsRaw) {
					if (dec.DecodeBitLogp(1)) {
						const uint32 octave = dec.DecodeUint(6);
						pfPitch = (int32)((16u << octave) + dec.DecodeBits(4 + (uint32)octave) - 1u);
						const int32 qg = (int32)dec.DecodeBits(3);
						if (dec.Tell() + 2 <= totalBitsRaw)
							pfTapset = (int32)dec.DecodeIcdf(kTapsetIcdf, 2);
						pfGain = 0.09375f * (float32)(qg + 1); // QCONST16(.09375,15)*(qg+1)
					}
				}
				if (LM > 0 && dec.Tell() + 3 <= totalBitsRaw)
					transient = dec.DecodeBitLogp(3) != 0;
				if (dec.Tell() + 3 <= totalBitsRaw)
					intra = dec.DecodeBitLogp(3) != 0;
			}

			if (outFlags) {
				outFlags->silence = silence;
				outFlags->transient = transient;
				outFlags->intra = intra;
			}

			const int32 shortBlocks = transient ? M : 0;

			// Spectre normalisé + masques de collapse.
			float32 *X = (float32 *)memory::NkAlloc((size_t)C * N * sizeof(float32));
			uint8 collapseMasks[kNumBands * kMaxChannels];
			for (int32 i = 0; i < C * N; ++i)
				X[i] = 0.0f;
			for (int32 i = 0; i < kNumBands * kMaxChannels; ++i)
				collapseMasks[i] = 0;

			if (!silence) {
				// Énergie grossière.
				NkCeltEnergy::UnquantCoarse(dec, mOldEBands, kNumBands, start, end, intra, C, LM);

				// tf_decode.
				int32 tfRes[kNumBands] = {0};
				TfDecode(dec, start, end, transient ? 1 : 0, tfRes, LM, len);

				// Spread.
				int32 spread = 2; // SPREAD_NORMAL
				if (dec.Tell() + 4 <= totalBitsRaw)
					spread = dec.DecodeIcdf(kSpreadIcdf, 5);

				// Caps.
				int32 cap[kNumBands];
				InitCaps(cap, LM, C, eBands);

				// Dynalloc (boosts).
				int32 offsets[kNumBands] = {0};
				int32 dynallocLogp = 6;
				int32 totalBitsFrac = totalBitsRaw << BITRES;
				uint32 tellFrac = dec.TellFrac();
				for (int32 i = start; i < end; ++i) {
					const int32 width = C * ((int32)eBands[i + 1] - (int32)eBands[i]) << LM;
					const int32 quanta = IMin(width << BITRES, IMax(6 << BITRES, width));
					int32 loopLogp = dynallocLogp;
					int32 boost = 0;
					while ((int32)tellFrac + (loopLogp << BITRES) < totalBitsFrac && boost < cap[i]) {
						const int32 flag = dec.DecodeBitLogp((uint32)loopLogp);
						tellFrac = dec.TellFrac();
						if (!flag)
							break;
						boost += quanta;
						totalBitsFrac -= quanta;
						loopLogp = 1;
					}
					offsets[i] = boost;
					if (boost > 0)
						dynallocLogp = IMax(2, dynallocLogp - 1);
				}

				// Trim.
				int32 allocTrim = 5;
				if ((int32)tellFrac + (6 << BITRES) <= totalBitsFrac)
					allocTrim = dec.DecodeIcdf(kTrimIcdf, 7);

				// Réservation anti-collapse + budget d'allocation.
				int32 bits = (totalBitsRaw << BITRES) - (int32)dec.TellFrac() - 1;
				const int32 antiCollapseRsv =
					(transient && LM >= 2 && bits >= ((LM + 2) << BITRES)) ? (1 << BITRES) : 0;
				bits -= antiCollapseRsv;

				// Allocation (décodage) ; en stéréo décode aussi intensity + dual stereo.
				int32 pulses[kNumBands] = {0}, fineQuant[kNumBands] = {0}, finePriority[kNumBands] = {0};
				int32 balance = 0, intensity = 0, dualStereo = 0;
				const int32 codedBands = ComputeAllocation(dec, start, end, offsets, cap, allocTrim, bits, LM, C,
														   eBands, pulses, fineQuant, finePriority, &balance,
														   &intensity, &dualStereo);

				// Énergie fine.
				NkCeltEnergy::UnquantFine(dec, mOldEBands, kNumBands, fineQuant, start, end, C);

				// Décodage des bandes (forme spectrale). En stéréo, Y = plan du canal 1.
				NkCeltQuantBands::QuantAllBands(dec, start, end, X, (C == 2) ? (X + N) : nullptr, collapseMasks,
												pulses, shortBlocks, spread, dualStereo, intensity, tfRes,
												(totalBitsRaw << BITRES) - antiCollapseRsv, balance, LM, codedBands,
												&mRng);

				// Anti-collapse.
				int32 antiCollapseOn = 0;
				if (antiCollapseRsv > 0)
					antiCollapseOn = (int32)dec.DecodeBits(1);

				// Finalise (bits restants → énergie fine).
				EnergyFinalise(dec, mOldEBands, fineQuant, finePriority, start, end, len * 8 - dec.Tell(), C);

				if (antiCollapseOn) {
					// Ordre normatif : bandes en boucle externe, canaux en interne (le LCG mRng
					// est séquentiel). Pour C==1, libopus prend le MAX de l'historique des deux
					// canaux (trace d'un éventuel passé stéréo) — notre canal 1 vaut -28 constant,
					// le max avec lui plafonne l'historique par le bas exactement pareil.
					for (int32 i = start; i < end; ++i) {
						const int32 N0 = (int32)eBands[i + 1] - (int32)eBands[i];
						for (int32 c = 0; c < C; ++c) {
							float32 prev1 = mOldLogE[c * kNumBands + i];
							float32 prev2 = mOldLogE2[c * kNumBands + i];
							if (C == 1) {
								prev1 = prev1 > mOldLogE[kNumBands + i] ? prev1 : mOldLogE[kNumBands + i];
								prev2 = prev2 > mOldLogE2[kNumBands + i] ? prev2 : mOldLogE2[kNumBands + i];
							}
							mRng = NkCeltAntiCollapse::ApplyBand(X + c * N + M * (int32)eBands[i], N0, LM,
																 collapseMasks[i * C + c],
																 mOldEBands[c * kNumBands + i], prev1, prev2,
																 pulses[i], mRng);
						}
					}
				}
			}

			if (silence) {
				for (int32 i = 0; i < C * kNumBands; ++i)
					mOldEBands[i] = kMinEnergyDb;
			}

			// --- Synthèse : dénormalise → IMDCT → overlap-add (buffer glissant) → deemphasis. ---
			const int32 B = transient ? M : 1;
			const int32 NB = transient ? kShortMdctSize : N;
			const int32 shift = transient ? kMaxLM : (kMaxLM - LM);

			float32 *freq = (float32 *)memory::NkAlloc((size_t)N * sizeof(float32));

			for (int32 c = 0; c < C; ++c) {
				float32 *mem = mDecodeMem + c * (kDecBufSize + kOverlap);
				// Glissement du buffer vers la gauche de N.
				for (int32 i = 0; i < kDecBufSize - N + kOverlap; ++i)
					mem[i] = mem[i + N];
				float32 *outSyn = mem + (kDecBufSize - N);

				if (silence) {
					for (int32 i = 0; i < N; ++i)
						outSyn[i] = 0.0f;
				} else {
					// Le repère MDCT couvre N = shortMdctSize<<LM bins, mais les bandes ne couvrent que
					// eBands[nb]<<LM ; on met tout le tampon à zéro d'abord (les bins hauts non codés = 0).
					for (int32 i = 0; i < N; ++i)
						freq[i] = 0.0f;
					NkCeltDenorm::DenormaliseBands(X + c * N, freq, mOldEBands + c * kNumBands, start, end, LM);
					for (int32 b = 0; b < B; ++b)
						MdctBackward(&freq[b], outSyn + NB * b, shift, B);
				}

				// Post-filtre (comb filter) : 1er sous-bloc avec l'ancien filtre → crossfade vers le
				// filtre courant ; le reste avec le nouveau pitch/gain de cette trame. In-place sur
				// outSyn (l'historique x[i-T] est en amont dans le buffer glissant). No-op si gains nuls
				// (hybride + trames sans post-filtre) → n'affecte pas le chemin hybride bit-exact.
				CombFilter(outSyn, outSyn, mPostfilterPeriodOld, mPostfilterPeriod, kShortMdctSize,
						   mPostfilterGainOld, mPostfilterGain, mPostfilterTapsetOld, mPostfilterTapset, kOverlap);
				if (LM != 0)
					CombFilter(outSyn + kShortMdctSize, outSyn + kShortMdctSize, mPostfilterPeriod, pfPitch,
							   N - kShortMdctSize, mPostfilterGain, pfGain, mPostfilterTapset, pfTapset, kOverlap);

				// Deemphasis → PCM interleaved.
				float32 *pcmC = (float32 *)memory::NkAlloc((size_t)N * sizeof(float32));
				NkCeltDeemphasis::Apply(outSyn, pcmC, N, NkCeltDeemphasis::kPreemphCoef48k, &mPreemphMem[c]);
				for (int32 i = 0; i < N; ++i)
					if (accum) pcm[i * C + c] += pcmC[i] * (1.0f / 32768.0f); else pcm[i * C + c] = pcmC[i] * (1.0f / 32768.0f); // SCALEOUT + accum (hybride)
				memory::NkFree(pcmC);
			}

			// Bascule l'état du post-filtre : courant → old, décodé → courant (cf. celt_decode).
			mPostfilterPeriodOld = mPostfilterPeriod;
			mPostfilterGainOld = mPostfilterGain;
			mPostfilterTapsetOld = mPostfilterTapset;
			mPostfilterPeriod = pfPitch;
			mPostfilterGain = pfGain;
			mPostfilterTapset = pfTapset;
			if (LM != 0) {
				mPostfilterPeriodOld = mPostfilterPeriod;
				mPostfilterGainOld = mPostfilterGain;
				mPostfilterTapsetOld = mPostfilterTapset;
			}

			// Historique énergie pour l'anti-collapse de la trame suivante.
			for (int32 i = 0; i < C * kNumBands; ++i) {
				mOldLogE2[i] = mOldLogE[i];
				mOldLogE[i] = mOldEBands[i];
			}

			memory::NkFree(freq);
			memory::NkFree(X);
			return true;
		}

		bool NkCeltDecoder::SelfTest() {
			bool ok = true;
			const uint32 CAP = 256;
			uint8 *buf = (uint8 *)memory::NkAlloc(CAP);
			if (!buf)
				return false;

			// Trame de SILENCE : bit de silence = 1.
			NkOpusRangeEncoder enc;
			enc.Init(buf, CAP);
			enc.EncodeBitLogp(1, 15);
			enc.Done();
			const int32 len = (int32)enc.RangeBytes();

			const int32 LM = 3;
			const int32 N = (1 << LM) * kShortMdctSize;
			const int32 C = 1;

			NkCeltDecoder decoder;
			decoder.Init(C);
			float32 *pcm = (float32 *)memory::NkAlloc((size_t)N * C * sizeof(float32));
			NkCeltDecoder::NkFrameFlags flags;
			const bool okDec = decoder.DecodeFrame(buf, len > 0 ? len : 1, LM, pcm, &flags);
			if (!okDec)
				ok = false;
			if (!flags.silence)
				ok = false;
			for (int32 i = 0; i < N * C; ++i)
				if (pcm[i] < -1e-6f || pcm[i] > 1e-6f)
					ok = false;

			memory::NkFree(buf);
			memory::NkFree(pcm);
			return ok;
		}

	} // namespace media
} // namespace nkentseu
