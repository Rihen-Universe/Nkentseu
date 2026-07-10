// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltQuantBands.cpp — décodage des bandes CELT.
// Algorithme RFC 6716 (quant_all_bands/quant_band/quant_partition/compute_theta),
// réécrit à la sauce Nkentseu. Chemin DÉCODEUR MONO. Zero-STL.
// =============================================================================
#include "NKMedia/Codecs/Opus/Celt/NkCeltQuantBands.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltBands.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltSplit.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltRate.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltVq.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltAntiCollapse.h"
#include "NKMemory/NKMemory.h"

#include <cmath> // sqrtf

namespace nkentseu {
	namespace media {

		namespace {

			constexpr int32 BITRES = 3;
			constexpr int32 QTHETA_OFFSET = 4;
			constexpr int32 kNbEBands = 21;
			constexpr int32 kEffEBands = 21;

			// logN400 (constante du format RFC 6716).
			const int16 kLogN[21] = {0, 0, 0, 0, 0,  0,  0,  0,  8,  8, 8,
									 8, 16, 16, 16, 21, 21, 24, 29, 34, 36};
			// Table d'ordre pour le hadamard (stride 2/4/8/16), constante du format.
			const int32 kOrdery[] = {1,  0, 3,  0, 2,  1,  7, 0, 4, 3, 6,  1, 5, 2,
									 15, 0, 8,  7, 12, 3,  11, 4, 14, 1, 9, 6, 13, 2, 10, 5};
			const uint8 kBitInterleave[16] = {0, 1, 1, 1, 2, 3, 3, 3, 2, 3, 3, 3, 2, 3, 3, 3};
			const uint8 kBitDeinterleave[16] = {0x00, 0x03, 0x0C, 0x0F, 0x30, 0x33, 0x3C, 0x3F,
												0xC0, 0xC3, 0xCC, 0xCF, 0xF0, 0xF3, 0xFC, 0xFF};

			int32 IMax(int32 a, int32 b) {
				return a > b ? a : b;
			}
			int32 IMin(int32 a, int32 b) {
				return a < b ? a : b;
			}
			int32 FracMul16(int32 a, int32 b) {
				return (16384 + a * b) >> 15;
			}

			struct Ctx {
					NkOpusRangeDecoder *dec = nullptr;
					uint32 seed = 0;
					int32 spread = 2;
					int32 remainingBits = 0;
					int32 i = 0;
					int32 tfChange = 0;
			};

			struct SplitResult {
					int32 imid, iside, delta, itheta, qalloc;
			};

			void DeinterleaveHadamard(float32 *X, int32 N0, int32 stride, int32 hadamard, float32 *tmp) {
				const int32 N = N0 * stride;
				if (hadamard) {
					const int32 *ordery = kOrdery + stride - 2;
					for (int32 i = 0; i < stride; ++i)
						for (int32 j = 0; j < N0; ++j)
							tmp[ordery[i] * N0 + j] = X[j * stride + i];
				} else {
					for (int32 i = 0; i < stride; ++i)
						for (int32 j = 0; j < N0; ++j)
							tmp[i * N0 + j] = X[j * stride + i];
				}
				for (int32 k = 0; k < N; ++k)
					X[k] = tmp[k];
			}

			void InterleaveHadamard(float32 *X, int32 N0, int32 stride, int32 hadamard, float32 *tmp) {
				const int32 N = N0 * stride;
				if (hadamard) {
					const int32 *ordery = kOrdery + stride - 2;
					for (int32 i = 0; i < stride; ++i)
						for (int32 j = 0; j < N0; ++j)
							tmp[j * stride + i] = X[ordery[i] * N0 + j];
				} else {
					for (int32 i = 0; i < stride; ++i)
						for (int32 j = 0; j < N0; ++j)
							tmp[j * stride + i] = X[i * N0 + j];
				}
				for (int32 k = 0; k < N; ++k)
					X[k] = tmp[k];
			}

			uint32 QuantBandN1(Ctx *ctx, float32 *X, float32 *lowbandOut) {
				int32 sign = 0;
				if (ctx->remainingBits >= (1 << BITRES)) {
					sign = (int32)ctx->dec->DecodeBits(1);
					ctx->remainingBits -= (1 << BITRES);
				}
				X[0] = sign ? -1.0f : 1.0f;
				if (lowbandOut)
					lowbandOut[0] = X[0]; // SHR32(.,4) = no-op float
				return 1;
			}

			SplitResult ComputeTheta(Ctx *ctx, int32 N, int32 *b, int32 B, int32 B0, int32 LM, int32 *fill) {
				const int32 pulseCap = kLogN[ctx->i] + LM * (1 << BITRES);
				const int32 offset = (pulseCap >> 1) - QTHETA_OFFSET;
				int32 qn = NkCeltSplit::ComputeQn(N, *b, offset, pulseCap, 0);
				const int32 tell = (int32)ctx->dec->TellFrac();
				int32 itheta = 0;
				if (qn != 1) {
					if (B0 > 1) {
						itheta = (int32)ctx->dec->DecodeUint((uint32)qn + 1); // pdf uniforme
					} else {
						// pdf triangulaire
						const int32 ft = ((qn >> 1) + 1) * ((qn >> 1) + 1);
						const int32 fm = (int32)ctx->dec->Decode((uint32)ft);
						int32 fl, fs;
						if (fm < ((qn >> 1) * ((qn >> 1) + 1) >> 1)) {
							itheta = ((int32)NkCeltSplit::Isqrt32(8u * (uint32)fm + 1u) - 1) >> 1;
							fs = itheta + 1;
							fl = itheta * (itheta + 1) >> 1;
						} else {
							itheta = (2 * (qn + 1) - (int32)NkCeltSplit::Isqrt32(8u * (uint32)(ft - fm - 1) + 1u)) >> 1;
							fs = qn + 1 - itheta;
							fl = ft - ((qn + 1 - itheta) * (qn + 2 - itheta) >> 1);
						}
						ctx->dec->Update((uint32)fl, (uint32)(fl + fs), (uint32)ft);
					}
					itheta = (int32)((uint32)itheta * 16384u / (uint32)qn);
				}
				const int32 qalloc = (int32)ctx->dec->TellFrac() - tell;
				*b -= qalloc;

				SplitResult r;
				if (itheta == 0) {
					r.imid = 32767;
					r.iside = 0;
					*fill &= (1 << B) - 1;
					r.delta = -16384;
				} else if (itheta == 16384) {
					r.imid = 0;
					r.iside = 32767;
					*fill &= ((1 << B) - 1) << B;
					r.delta = 16384;
				} else {
					r.imid = NkCeltSplit::BitexactCos(itheta);
					r.iside = NkCeltSplit::BitexactCos(16384 - itheta);
					r.delta = FracMul16((N - 1) << 7, NkCeltSplit::BitexactLog2Tan(r.iside, r.imid));
				}
				r.itheta = itheta;
				r.qalloc = qalloc;
				return r;
			}

			uint32 QuantPartition(Ctx *ctx, float32 *X, int32 N, int32 b, int32 B, float32 *lowband, int32 LM,
								  float32 gain, int32 fill) {
				const int32 B0 = B;
				uint32 cm = 0;

				const int32 count = NkCeltRate::Bits2Pulses(N, 1000000); // nb entrées cache
				const int32 cacheMax = count > 0 ? NkCeltRate::Pulses2Bits(N, count) - 1 : 0;

				if (LM != -1 && b > cacheMax + 12 && N > 2) {
					// --- SPLIT ---
					int32 n = N >> 1;
					float32 *Y = X + n;
					int32 lm2 = LM - 1;
					int32 f = fill;
					if (B == 1)
						f = (f & 1) | (f << 1);
					int32 b2 = (B + 1) >> 1;

					int32 bb = b;
					SplitResult sc = ComputeTheta(ctx, n, &bb, b2, B0, lm2, &f);
					const float32 mid = (1.0f / 32768.0f) * (float32)sc.imid;
					const float32 side = (1.0f / 32768.0f) * (float32)sc.iside;
					int32 delta = sc.delta;
					if (B0 > 1 && (sc.itheta & 0x3fff)) {
						if (sc.itheta > 8192)
							delta -= delta >> (4 - lm2);
						else
							delta = IMin(0, delta + (n << BITRES >> (5 - lm2)));
					}
					int32 mbits = IMax(0, IMin(bb, (bb - delta) / 2));
					int32 sbits = bb - mbits;
					ctx->remainingBits -= sc.qalloc;
					float32 *next2 = lowband ? lowband + n : nullptr;

					int32 rebalance = ctx->remainingBits;
					if (mbits >= sbits) {
						cm = QuantPartition(ctx, X, n, mbits, b2, lowband, lm2, gain * mid, f);
						rebalance = mbits - (rebalance - ctx->remainingBits);
						if (rebalance > 3 << BITRES && sc.itheta != 0)
							sbits += rebalance - (3 << BITRES);
						cm |= QuantPartition(ctx, Y, n, sbits, b2, next2, lm2, gain * side, f >> b2) << (B0 >> 1);
					} else {
						cm = QuantPartition(ctx, Y, n, sbits, b2, next2, lm2, gain * side, f >> b2) << (B0 >> 1);
						rebalance = sbits - (rebalance - ctx->remainingBits);
						if (rebalance > 3 << BITRES && sc.itheta != 16384)
							mbits += rebalance - (3 << BITRES);
						cm |= QuantPartition(ctx, X, n, mbits, b2, lowband, lm2, gain * mid, f);
					}
				} else {
					// --- PAS DE SPLIT ---
					int32 q = NkCeltRate::Bits2Pulses(N, b);
					int32 currBits = NkCeltRate::Pulses2Bits(N, q);
					ctx->remainingBits -= currBits;
					while (ctx->remainingBits < 0 && q > 0) {
						ctx->remainingBits += currBits;
						q--;
						currBits = NkCeltRate::Pulses2Bits(N, q);
						ctx->remainingBits -= currBits;
					}
					if (q != 0) {
						const int32 K = NkCeltRate::GetPulses(q);
						cm = NkCeltVq::AlgUnquant(*ctx->dec, X, N, K, ctx->spread, B, gain);
					} else {
						// folding / bruit
						const uint32 cmMask = (uint32)((1u << B) - 1u);
						fill &= (int32)cmMask;
						if (!fill) {
							for (int32 j = 0; j < N; ++j)
								X[j] = 0.0f;
						} else {
							if (lowband == nullptr) {
								for (int32 j = 0; j < N; ++j) {
									ctx->seed = NkCeltAntiCollapse::LcgRand(ctx->seed);
									X[j] = (float32)((int32)ctx->seed >> 20);
								}
								cm = cmMask;
							} else {
								for (int32 j = 0; j < N; ++j) {
									ctx->seed = NkCeltAntiCollapse::LcgRand(ctx->seed);
									const float32 tmp = (ctx->seed & 0x8000u) ? (1.0f / 256.0f) : -(1.0f / 256.0f);
									X[j] = lowband[j] + tmp;
								}
								cm = (uint32)fill;
							}
							NkCeltAntiCollapse::RenormaliseVector(X, N, gain);
						}
					}
				}
				return cm;
			}

			uint32 QuantBand(Ctx *ctx, float32 *X, int32 N, int32 b, int32 B, float32 *lowband, int32 LM,
							 float32 *lowbandOut, float32 gain, float32 *lowbandScratch, int32 fill, float32 *tmp) {
				const int32 N0 = N;
				int32 N_B = N;
				const int32 B0i = B;
				int32 timeDivide = 0;
				int32 recombine = 0;
				const int32 longBlocks = (B0i == 1);
				uint32 cm = 0;
				int32 tfChange = ctx->tfChange;

				N_B = N_B / B;
				if (N == 1)
					return QuantBandN1(ctx, X, lowbandOut);

				if (tfChange > 0)
					recombine = tfChange;

				if (lowbandScratch && lowband && (recombine || ((N_B & 1) == 0 && tfChange < 0) || B0i > 1)) {
					for (int32 k = 0; k < N; ++k)
						lowbandScratch[k] = lowband[k];
					lowband = lowbandScratch;
				}

				for (int32 k = 0; k < recombine; ++k) {
					if (lowband)
						NkCeltSplit::Haar1(lowband, N >> k, 1 << k);
					fill = kBitInterleave[fill & 0xF] | kBitInterleave[(fill >> 4) & 0xF] << 2;
				}
				B >>= recombine;
				N_B <<= recombine;

				while ((N_B & 1) == 0 && tfChange < 0) {
					if (lowband)
						NkCeltSplit::Haar1(lowband, N_B, B);
					fill |= fill << B;
					B <<= 1;
					N_B >>= 1;
					timeDivide++;
					tfChange++;
				}
				const int32 B0 = B;
				const int32 N_B0 = N_B;

				if (B0 > 1 && lowband)
					DeinterleaveHadamard(lowband, N_B >> recombine, B0 << recombine, longBlocks, tmp);

				cm = QuantPartition(ctx, X, N, b, B, lowband, LM, gain, fill);

				// resynth (décodeur)
				if (B0 > 1)
					InterleaveHadamard(X, N_B >> recombine, B0 << recombine, longBlocks, tmp);
				N_B = N_B0;
				B = B0;
				for (int32 k = 0; k < timeDivide; ++k) {
					B >>= 1;
					N_B <<= 1;
					cm |= cm >> B;
					NkCeltSplit::Haar1(X, N_B, B);
				}
				for (int32 k = 0; k < recombine; ++k) {
					cm = kBitDeinterleave[cm & 0xF];
					NkCeltSplit::Haar1(X, N0 >> k, 1 << k);
				}
				B <<= recombine;

				if (lowbandOut) {
					const float32 nn = ::sqrtf((float32)N0);
					for (int32 j = 0; j < N0; ++j)
						lowbandOut[j] = nn * X[j];
				}
				cm &= (uint32)((1u << B) - 1u);
				return cm;
			}

		} // namespace

		void NkCeltQuantBands::QuantAllBands(NkOpusRangeDecoder &dec, int32 start, int32 end, float32 *X_,
											 uint8 *collapseMasks, const int32 *pulses, int32 shortBlocks, int32 spread,
											 const int32 *tfRes, int32 totalBits, int32 balance, int32 LM,
											 int32 codedBands, uint32 *seed) {
			const int16 *eBands = NkCeltBands::Eband5ms();
			const int32 M = 1 << LM;
			const int32 B = shortBlocks ? M : 1;
			const int32 normOffset = M * (int32)eBands[start];
			const int32 normLen = M * (int32)eBands[kNbEBands] - normOffset;

			float32 *norm = (float32 *)memory::NkAlloc((size_t)(normLen > 0 ? normLen : 1) * sizeof(float32));
			float32 *tmp = (float32 *)memory::NkAlloc((size_t)256 * sizeof(float32)); // scratch hadamard
			float32 *lowbandScratch = X_ + M * (int32)eBands[kEffEBands - 1];

			Ctx ctx;
			ctx.dec = &dec;
			ctx.seed = *seed;
			ctx.spread = spread;

			int32 lowbandOffset = 0;
			int32 updateLowband = 1;

			for (int32 i = start; i < end; ++i) {
				ctx.i = i;
				const int32 last = (i == end - 1);
				float32 *X = X_ + M * (int32)eBands[i];
				const int32 N = M * (int32)eBands[i + 1] - M * (int32)eBands[i];
				const int32 tell = (int32)dec.TellFrac();
				if (i != start)
					balance -= tell;
				const int32 remainingBits = totalBits - tell - 1;
				ctx.remainingBits = remainingBits;

				int32 b;
				if (i <= codedBands - 1) {
					const int32 currBalance = balance / IMin(3, codedBands - i);
					b = IMax(0, IMin(16383, IMin(remainingBits + 1, pulses[i] + currBalance)));
				} else {
					b = 0;
				}

				if ((M * (int32)eBands[i] - N >= M * (int32)eBands[start] || i == start + 1) &&
					(updateLowband || lowbandOffset == 0))
					lowbandOffset = i;
				// special_hybrid_folding : no-op en CELT pur (start=0)

				ctx.tfChange = tfRes[i];
				float32 *scratch = last ? nullptr : lowbandScratch;

				int32 effectiveLowband = -1;
				uint32 xcm;
				if (lowbandOffset != 0 && (spread != 3 || B > 1 || ctx.tfChange < 0)) {
					effectiveLowband = IMax(0, M * (int32)eBands[lowbandOffset] - normOffset - N);
					int32 foldStart = lowbandOffset;
					while (M * (int32)eBands[--foldStart] > effectiveLowband + normOffset)
						;
					int32 foldEnd = lowbandOffset - 1;
					while (++foldEnd < i && M * (int32)eBands[foldEnd] < effectiveLowband + normOffset + N)
						;
					xcm = 0;
					int32 fi = foldStart;
					do {
						xcm |= collapseMasks[fi];
					} while (++fi < foldEnd);
				} else {
					xcm = (uint32)((1u << B) - 1u);
				}

				float32 *lowbandPtr = (effectiveLowband != -1) ? (norm + effectiveLowband) : nullptr;
				float32 *lowbandOut = last ? nullptr : (norm + M * (int32)eBands[i] - normOffset);

				xcm = QuantBand(&ctx, X, N, b, B, lowbandPtr, LM, lowbandOut, 1.0f, scratch, (int32)xcm, tmp);

				collapseMasks[i] = (uint8)xcm;
				balance += pulses[i] + tell;
				updateLowband = (b > (N << BITRES));
			}
			*seed = ctx.seed;

			memory::NkFree(norm);
			memory::NkFree(tmp);
		}

		bool NkCeltQuantBands::SelfTest() {
			// Test structurel : décode une trame avec allocation nulle (toutes bandes fold/clear) →
			// spectre fini, pas de crash. La validation bit-exacte réelle = harnais NKOpusRef vs ffmpeg.
			bool ok = true;
			const int32 LM = 3;
			const int16 *eBands = NkCeltBands::Eband5ms();
			const int32 M = 1 << LM;
			const int32 total = M * (int32)eBands[kNbEBands];
			float32 *X = (float32 *)memory::NkAlloc((size_t)total * sizeof(float32));
			uint8 masks[21];
			int32 pulses[21];
			int32 tf[21];
			for (int32 i = 0; i < 21; ++i) {
				pulses[i] = 0;
				tf[i] = 0;
				masks[i] = 0;
			}
			for (int32 i = 0; i < total; ++i)
				X[i] = 0.0f;

			uint8 buf[64];
			NkOpusRangeEncoder enc;
			enc.Init(buf, 64);
			enc.EncodeBits(0, 1); // un peu de données
			enc.Done();

			NkOpusRangeDecoder dec;
			dec.Init(buf, (uint32)(enc.RangeBytes() > 0 ? enc.RangeBytes() : 1));
			uint32 seed = 0;
			NkCeltQuantBands::QuantAllBands(dec, 0, 21, X, masks, pulses, 0, 2, tf, 64 * 8, 0, LM, 21, &seed);

			for (int32 i = 0; i < total; ++i) {
				const float32 v = X[i];
				if (!(v == v) || v > 1e9f || v < -1e9f) { // NaN/inf
					ok = false;
					break;
				}
			}
			memory::NkFree(X);
			return ok;
		}

	} // namespace media
} // namespace nkentseu
