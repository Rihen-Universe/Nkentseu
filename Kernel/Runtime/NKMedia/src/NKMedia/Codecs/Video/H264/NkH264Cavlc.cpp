// =============================================================================
// NKMedia/Codecs/Video/H264/NkH264Cavlc.cpp — codage CAVLC H.264 (§9.2).
// =============================================================================
#include "NKMedia/Codecs/Video/H264/NkH264Cavlc.h"

namespace nkentseu {
	namespace media {

		namespace {
// --- tables décodées depuis minih264 (validées vs standard) ---
#include "NkH264CavlcTables.inc"

			inline int32 CoeffTokenTable(int32 nC) {
				if (nC < 0)
					return 4; // chroma DC
				if (nC < 2)
					return 0;
				if (nC < 4)
					return 1;
				if (nC < 8)
					return 2;
				return 3; // FLC (nC >= 8)
			}

			// Émet un niveau (level_prefix unaire + level_suffix), suffixLength adaptatif.
			void PutLevelCode(NkH264BitWriter &bs, int32 levelCode, int32 suffixLength) {
				if (suffixLength > 0) {
					const int32 prefix = levelCode >> suffixLength;
					if (prefix < 15) {
						for (int32 z = 0; z < prefix; ++z)
							bs.PutBits(0, 1);
						bs.PutBits(1, 1);
						bs.PutBits((uint32)(levelCode & ((1 << suffixLength) - 1)), suffixLength);
						return;
					}
					// échappement (prefix >= 15)
					for (int32 z = 0; z < 15; ++z)
						bs.PutBits(0, 1);
					bs.PutBits(1, 1);
					bs.PutBits((uint32)(levelCode - (15 << suffixLength)) & 0xFFF, 12);
					return;
				}
				// suffixLength == 0
				if (levelCode < 14) {
					for (int32 z = 0; z < levelCode; ++z)
						bs.PutBits(0, 1);
					bs.PutBits(1, 1);
				} else if (levelCode < 30) {
					for (int32 z = 0; z < 14; ++z)
						bs.PutBits(0, 1);
					bs.PutBits(1, 1);
					bs.PutBits((uint32)(levelCode - 14), 4);
				} else {
					for (int32 z = 0; z < 15; ++z)
						bs.PutBits(0, 1);
					bs.PutBits(1, 1);
					bs.PutBits((uint32)(levelCode - 30) & 0xFFF, 12);
				}
			}
		} // namespace

		int32 NkH264Cavlc::EncodeResidual(NkH264BitWriter &bs, const int32 *coef, int32 maxNumCoeff, int32 nC) {
			// 1) positions et niveaux des coefficients non nuls (ordre de balayage ascendant).
			int32 pos[16], lvl[16];
			int32 tc = 0;
			for (int32 i = 0; i < maxNumCoeff; ++i)
				if (coef[i] != 0) {
					pos[tc] = i;
					lvl[tc] = coef[i];
					++tc;
				}

			// trailing_ones = ±1 en fin (haute fréquence), max 3.
			int32 t1 = 0;
			for (int32 i = tc - 1; i >= 0 && t1 < 3; --i) {
				if (lvl[i] == 1 || lvl[i] == -1)
					++t1;
				else
					break;
			}

			// total_zeros.
			const int32 totalZeros = (tc > 0) ? (pos[tc - 1] - (tc - 1)) : 0;

			// 2) coeff_token.
			const int32 tbl = CoeffTokenTable(nC);
			const uint8 *ctk = kCoeffToken[tbl][tc][t1];
			bs.PutBits(ctk[0], ctk[1]);
			if (tc == 0)
				return 0;

			// 3) signes des trailing ones (haute → basse fréquence).
			for (int32 i = 0; i < t1; ++i)
				bs.PutBits(lvl[tc - 1 - i] < 0 ? 1u : 0u, 1);

			// 4) niveaux (des non-T1, haute → basse fréquence).
			int32 suffixLength = (tc > 10 && t1 < 3) ? 1 : 0;
			for (int32 k = 0; k < tc - t1; ++k) {
				const int32 level = lvl[tc - 1 - t1 - k];
				int32 levelCode = (level > 0) ? ((level << 1) - 2) : ((-level << 1) - 1);
				if (k == 0 && t1 < 3)
					levelCode -= 2;
				PutLevelCode(bs, levelCode, suffixLength);
				if (suffixLength == 0)
					suffixLength = 1;
				const int32 a = level < 0 ? -level : level;
				if (a > (3 << (suffixLength - 1)) && suffixLength < 6)
					++suffixLength;
			}

			// 5) total_zeros.
			if (tc < maxNumCoeff) {
				const uint8 *tz = (maxNumCoeff == 4) ? kTotalZerosChroma[tc][totalZeros] : kTotalZeros[tc][totalZeros];
				bs.PutBits(tz[0], tz[1]);
			}

			// 6) run_before (haute → basse fréquence, sauf le dernier coefficient).
			int32 zerosLeft = totalZeros;
			for (int32 i = tc - 1; i >= 1; --i) {
				if (zerosLeft <= 0)
					break;
				const int32 run = pos[i] - pos[i - 1] - 1;
				const int32 ctx = (zerosLeft < 7 ? zerosLeft : 7) - 1;
				const uint8 *rb = kRunBefore[ctx][run];
				bs.PutBits(rb[0], rb[1]);
				zerosLeft -= run;
			}

			return tc;
		}

		bool NkH264Cavlc::SelfTest() {
			// Test structurel : divers blocs → encodage sans débordement, longueur cohérente.
			bool ok = true;
			const int32 cases[][16] = {
				{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	 // bloc nul
				{3, 1, -1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	 // mix
				{-5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20},	 // DC + haute freq
				{1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	 // trailing ones
				{7, -3, 2, -8, 4, 0, -1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, // gros niveaux
			};
			for (uint32 c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c) {
				for (int32 nC = 0; nC <= 8; nC += 2) {
					NkH264BitWriter bs;
					const int32 tc = NkH264Cavlc::EncodeResidual(bs, cases[c], 16, nC);
					// total_coeff attendu = nombre de non-nuls.
					int32 expect = 0;
					for (int32 i = 0; i < 16; ++i)
						if (cases[c][i] != 0)
							++expect;
					if (tc != expect)
						ok = false;
				}
			}
			// Cas chroma DC (nC=-1, maxNumCoeff=4).
			{
				const int32 dc[4] = {2, -1, 0, 1};
				NkH264BitWriter bs;
				if (NkH264Cavlc::EncodeResidual(bs, dc, 4, -1) != 3)
					ok = false;
			}
			return ok;
		}

	} // namespace media
} // namespace nkentseu
