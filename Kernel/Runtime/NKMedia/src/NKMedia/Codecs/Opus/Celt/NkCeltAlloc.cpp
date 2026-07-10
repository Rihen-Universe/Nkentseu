// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltAlloc.cpp — allocation CELT (clt_compute_allocation).
// =============================================================================
#include "NKMedia/Codecs/Opus/Celt/NkCeltAlloc.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltBands.h"

namespace nkentseu {
	namespace media {

		namespace {
			constexpr int32 BITRES = 3;

			int32 IMax(int32 a, int32 b) {
				return a > b ? a : b;
			}
			int32 IMin(int32 a, int32 b) {
				return a < b ? a : b;
			}

			// band_allocation (libopus modes.c) : 11 lignes (qualité) × 21 bandes.
			const uint8 kBandAllocation[11 * 21] = {
				0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,	  0,	0,	 0,
				90,  80,  75,  69,  63,  56,  49,  40,  34,  29,  20,  18,  10,  0,   0,   0,   0,   0,	  0,	0,	 0,
				110, 100, 90,  84,  78,  71,  65,  58,  51,  45,  39,  32,  26,  20,  12,  0,   0,   0,	  0,	0,	 0,
				118, 110, 103, 93,  86,  80,  75,  70,  65,  59,  53,  47,  40,  31,  23,  15,  4,   0,	  0,	0,	 0,
				126, 119, 112, 104, 95,  89,  83,  78,  72,  66,  60,  54,  47,  39,  32,  25,  17,  12,  1,	0,	 0,
				134, 127, 120, 114, 103, 97,  91,  85,  78,  72,  66,  60,  54,  47,  41,  35,  29,  23,  16,	10,	 1,
				144, 137, 130, 124, 113, 107, 101, 95,  87,  81,  75,  69,  63,  56,  50,  44,  38,  33,  27,	23,	 15,
				158, 151, 144, 138, 127, 121, 115, 109, 100, 94,  88,  82,  76,  69,  63,  57,  51,  47,  41,	37,	 29,
				178, 171, 164, 158, 147, 141, 135, 129, 120, 114, 108, 102, 96,  89,  83,  77,  71,  67,  61,	57,	 49,
				216, 209, 202, 196, 185, 179, 173, 167, 158, 152, 146, 140, 134, 127, 121, 115, 109, 105, 99,	95,	 87,
				255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
		} // namespace

		const uint8 *NkCeltAlloc::BandAllocation() {
			return kBandAllocation;
		}

		int32 NkCeltAlloc::BuildInterp(int32 start, int32 end, const int32 *offsets, const int32 *cap, int32 allocTrim,
									   int32 total, int32 C, int32 LM, int32 *bits1, int32 *bits2, int32 *thresh,
									   int32 *trimOffset) {
			const int16 *eBands = NkCeltBands::Eband5ms();
			const int32 nbEBands = kNumBands;
			if (total < 0)
				total = 0;

			// Seuils + tilt (trim) par bande.
			for (int32 j = start; j < end; ++j) {
				const int32 width = (int32)eBands[j + 1] - (int32)eBands[j];
				thresh[j] = IMax(C << BITRES, (3 * width << LM << BITRES) >> 4);
				int64 to = (int64)C * width * (allocTrim - 5 - LM) * (end - j - 1) * ((int64)1 << (LM + BITRES)) >> 6;
				if (((int64)width << LM) == 1)
					to -= (int64)(C << BITRES);
				trimOffset[j] = (int32)to;
			}

			// Bissection sur la qualité : plus haute ligne `lo` dont l'allocation tient dans `total`.
			int32 lo = 1;
			int32 hi = kNumQuality - 1;
			do {
				int32 done = 0;
				int64 psum = 0;
				const int32 mid = (lo + hi) >> 1;
				for (int32 j = end; j-- > start;) {
					const int32 width = (int32)eBands[j + 1] - (int32)eBands[j];
					int64 bitsj = ((int64)C * width * kBandAllocation[mid * nbEBands + j] << LM) >> 2;
					if (bitsj > 0)
						bitsj = IMax(0, (int32)(bitsj + trimOffset[j]));
					if (offsets)
						bitsj += offsets[j];
					if (bitsj >= thresh[j] || done) {
						done = 1;
						psum += IMin((int32)bitsj, cap[j]);
					} else if (bitsj >= (C << BITRES)) {
						psum += (C << BITRES);
					}
				}
				if (psum > total)
					hi = mid - 1;
				else
					lo = mid + 1;
			} while (lo <= hi);
			hi = lo--;

			// Construit bits1 (ligne lo) et bits2 (delta vers ligne hi).
			for (int32 j = start; j < end; ++j) {
				const int32 width = (int32)eBands[j + 1] - (int32)eBands[j];
				int32 b1 = (int32)(((int64)C * width * kBandAllocation[lo * nbEBands + j] << LM) >> 2);
				int32 b2 = (hi >= kNumQuality)
							   ? cap[j]
							   : (int32)(((int64)C * width * kBandAllocation[hi * nbEBands + j] << LM) >> 2);
				if (b1 > 0)
					b1 = IMax(0, b1 + trimOffset[j]);
				if (b2 > 0)
					b2 = IMax(0, b2 + trimOffset[j]);
				if (lo > 0 && offsets)
					b1 += offsets[j];
				if (offsets)
					b2 += offsets[j];
				b2 = IMax(0, b2 - b1);
				bits1[j] = b1;
				bits2[j] = b2;
			}
			return lo;
		}

		int32 NkCeltAlloc::InterpFine(int32 start, int32 end, const int32 *bits1, const int32 *bits2,
									  const int32 *thresh, const int32 *cap, int32 total, int32 C, int32 *bits) {
			const int32 ALLOC_STEPS = 6;
			const int32 allocFloor = C << BITRES;

			// Bissection sur le coefficient d'interpolation lo/hi ∈ [0, 1<<ALLOC_STEPS].
			int32 lo = 0;
			int32 hi = 1 << ALLOC_STEPS;
			for (int32 i = 0; i < ALLOC_STEPS; ++i) {
				const int32 mid = (lo + hi) >> 1;
				int64 psum = 0;
				int32 done = 0;
				for (int32 j = end; j-- > start;) {
					const int32 tmp = bits1[j] + (int32)(((int64)mid * bits2[j]) >> ALLOC_STEPS);
					if (tmp >= thresh[j] || done) {
						done = 1;
						psum += IMin(tmp, cap[j]);
					} else if (tmp >= allocFloor) {
						psum += allocFloor;
					}
				}
				if (psum > total)
					hi = mid;
				else
					lo = mid;
			}

			// Budget final par bande au coefficient `lo`.
			int64 psum = 0;
			int32 done = 0;
			for (int32 j = end; j-- > start;) {
				int32 tmp = bits1[j] + (int32)(((int64)lo * bits2[j]) >> ALLOC_STEPS);
				if (tmp < thresh[j] && !done) {
					tmp = (tmp >= allocFloor) ? allocFloor : 0;
				} else {
					done = 1;
				}
				tmp = IMin(tmp, cap[j]);
				bits[j] = tmp;
				psum += tmp;
			}
			return (int32)psum;
		}

		bool NkCeltAlloc::SelfTest() {
			bool ok = true;
			const int32 start = 0, end = kNumBands, C = 1, LM = 3;
			int32 bits1[21], bits2[21], thresh[21], trim[21], cap[21];
			for (int32 i = 0; i < 21; ++i)
				cap[i] = 1 << 20; // plafond large

			// 1) Qualité monotone croissante avec le budget.
			int32 prevLo = -1;
			const int32 budgets[6] = {50, 200, 800, 2000, 6000, 20000};
			for (int32 bi = 0; bi < 6; ++bi) {
				const int32 lo = NkCeltAlloc::BuildInterp(start, end, nullptr, cap, 5, budgets[bi], C, LM, bits1, bits2,
														  thresh, trim);
				if (lo < prevLo)
					ok = false; // plus de budget → qualité ≥
				prevLo = lo;
				if (lo < 0 || lo > kNumQuality - 1)
					ok = false; // lo=0 valide (budget trop faible même pour la ligne 1)
				// bits1 non négatifs.
				for (int32 j = start; j < end; ++j)
					if (bits1[j] < 0 || bits2[j] < 0)
						ok = false;
			}

			// 2) Trim : un trim plus grand donne plus de bits aux basses bandes (tilt).
			{
				int32 b1a[21], b2a[21], thA[21], trA[21];
				int32 b1b[21], b2b[21], thB[21], trB[21];
				NkCeltAlloc::BuildInterp(start, end, nullptr, cap, 0, 4000, C, LM, b1a, b2a, thA, trA);
				NkCeltAlloc::BuildInterp(start, end, nullptr, cap, 10, 4000, C, LM, b1b, b2b, thB, trB);
				// trimOffset de la 1re bande doit être plus grand avec trim=10 qu'avec trim=0.
				if (!(trB[0] > trA[0]))
					ok = false;
			}

			// 3) InterpFine : budget par bande cohérent (somme ≤ total) et monotone avec le budget.
			{
				int32 prevSum = -1;
				const int32 budgets2[5] = {200, 800, 2000, 6000, 20000};
				for (int32 bi = 0; bi < 5; ++bi) {
					const int32 total = budgets2[bi];
					NkCeltAlloc::BuildInterp(start, end, nullptr, cap, 5, total, C, LM, bits1, bits2, thresh, trim);
					int32 perBand[21];
					const int32 sum = NkCeltAlloc::InterpFine(start, end, bits1, bits2, thresh, cap, total, C, perBand);
					if (sum > total)
						ok = false; // ne doit pas dépasser le budget
					if (sum < prevSum)
						ok = false; // plus de budget → au moins autant alloué
					prevSum = sum;
					for (int32 j = start; j < end; ++j)
						if (perBand[j] < 0)
							ok = false;
				}
			}

			return ok;
		}

	} // namespace media
} // namespace nkentseu
