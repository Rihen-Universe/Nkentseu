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

		int32 NkH264Cavlc::DecodeResidual(NkH264BitReader &bs, int32 *coefOut, int32 maxNumCoeff, int32 nC) {
			for (int32 i = 0; i < maxNumCoeff; ++i)
				coefOut[i] = 0;

			// 1) coeff_token -> total_coeff + trailing_ones.
			const int32 tbl = CoeffTokenTable(nC);
			int32 totalCoeff = 0, trailingOnes = 0;
			if (tbl == 3) { // nC >= 8 : FLC 6 bits
				const uint32 code = bs.U(6);
				if (code == 3) {
					totalCoeff = 0;
					trailingOnes = 0;
				} else {
					totalCoeff = (int32)(code >> 2) + 1;
					trailingOnes = (int32)(code & 3);
				}
			} else {
				bool found = false;
				for (int32 tc = 0; tc <= maxNumCoeff && !found; ++tc) {
					const int32 t1max = (tc < 3) ? tc : 3;
					for (int32 t1 = 0; t1 <= t1max && !found; ++t1) {
						const uint8 *e = kCoeffToken[tbl][tc][t1];
						if (e[1] == 0)
							continue;
						if (bs.Peek(e[1]) == (uint32)e[0]) {
							bs.Skip(e[1]);
							totalCoeff = tc;
							trailingOnes = t1;
							found = true;
						}
					}
				}
				if (!found)
					return 0;
			}
			if (totalCoeff == 0)
				return 0;

			// 2) niveaux : d'abord les trailing ones (±1), puis les autres (prefix/suffix adaptatif).
			int32 levelVal[16];
			for (int32 i = 0; i < trailingOnes; ++i)
				levelVal[i] = (bs.U1() == 1) ? -1 : 1;

			int32 suffixLength = (totalCoeff > 10 && trailingOnes < 3) ? 1 : 0;
			for (int32 k = 0; k < totalCoeff - trailingOnes; ++k) {
				int32 levelPrefix = 0;
				while (!bs.Eof() && bs.U1() == 0)
					++levelPrefix;
				int32 levelSuffixSize = suffixLength;
				if (levelPrefix == 14 && suffixLength == 0)
					levelSuffixSize = 4;
				else if (levelPrefix >= 15)
					levelSuffixSize = levelPrefix - 3;
				const int32 levelSuffix = (levelSuffixSize > 0) ? (int32)bs.U(levelSuffixSize) : 0;
				const int32 pfx = (levelPrefix < 15) ? levelPrefix : 15;
				int32 levelCode = (pfx << suffixLength) + levelSuffix;
				if (levelPrefix >= 15 && suffixLength == 0)
					levelCode += 15;
				if (levelPrefix >= 16)
					levelCode += (1 << (levelPrefix - 3)) - 4096;
				if (k == 0 && trailingOnes < 3)
					levelCode += 2;
				const int32 level = ((levelCode & 1) == 0) ? ((levelCode + 2) >> 1) : (-((levelCode + 1) >> 1));
				levelVal[trailingOnes + k] = level;
				if (suffixLength == 0)
					suffixLength = 1;
				const int32 a = (level < 0) ? -level : level;
				if (a > (3 << (suffixLength - 1)) && suffixLength < 6)
					++suffixLength;
			}

			// 3) total_zeros.
			int32 totalZeros = 0;
			if (totalCoeff < maxNumCoeff) {
				const int32 maxTz = maxNumCoeff - totalCoeff;
				bool ftz = false;
				for (int32 tz = 0; tz <= maxTz && !ftz; ++tz) {
					const uint8 *e = (maxNumCoeff == 4) ? kTotalZerosChroma[totalCoeff][tz] : kTotalZeros[totalCoeff][tz];
					if (e[1] == 0)
						continue;
					if (bs.Peek(e[1]) == (uint32)e[0]) {
						bs.Skip(e[1]);
						totalZeros = tz;
						ftz = true;
					}
				}
			}

			// 4) run_before -> positions.
			int32 runVal[16];
			int32 zerosLeft = totalZeros;
			for (int32 i = 0; i < totalCoeff - 1; ++i) {
				if (zerosLeft > 0) {
					const int32 ctx = (zerosLeft < 7 ? zerosLeft : 7) - 1;
					int32 run = 0;
					bool fr = false;
					for (int32 r = 0; r < 15 && !fr; ++r) {
						const uint8 *e = kRunBefore[ctx][r];
						if (e[1] == 0)
							continue;
						if (bs.Peek(e[1]) == (uint32)e[0]) {
							bs.Skip(e[1]);
							run = r;
							fr = true;
						}
					}
					runVal[i] = run;
					zerosLeft -= run;
				} else {
					runVal[i] = 0;
				}
			}
			runVal[totalCoeff - 1] = (zerosLeft > 0) ? zerosLeft : 0;

			// 5) reconstruction dans l'ordre de balayage (spec 9.2.4).
			int32 coeffNum = -1;
			for (int32 i = totalCoeff - 1; i >= 0; --i) {
				coeffNum += runVal[i] + 1;
				if (coeffNum >= 0 && coeffNum < maxNumCoeff)
					coefOut[coeffNum] = levelVal[i];
			}
			return totalCoeff;
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

			// ROUND-TRIP : encode -> decode -> les coefficients doivent être IDENTIQUES.
			for (uint32 c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c) {
				for (int32 nC = 0; nC <= 8; nC += 2) {
					NkH264BitWriter bw;
					NkH264Cavlc::EncodeResidual(bw, cases[c], 16, nC);
					NkVector<uint8> bytes;
					bw.Finalize(bytes);
					NkH264BitReader br(bytes.Data(), (usize)bytes.Size());
					int32 dec[16];
					NkH264Cavlc::DecodeResidual(br, dec, 16, nC);
					for (int32 i = 0; i < 16; ++i)
						if (dec[i] != cases[c][i])
							ok = false;
				}
			}
			// Round-trip chroma DC + AC 15.
			{
				const int32 dc[4] = {2, -1, 0, 1};
				NkH264BitWriter bw;
				NkH264Cavlc::EncodeResidual(bw, dc, 4, -1);
				NkVector<uint8> bytes;
				bw.Finalize(bytes);
				NkH264BitReader br(bytes.Data(), (usize)bytes.Size());
				int32 dec[4];
				NkH264Cavlc::DecodeResidual(br, dec, 4, -1);
				for (int32 i = 0; i < 4; ++i)
					if (dec[i] != dc[i])
						ok = false;
			}
			return ok;
		}

	} // namespace media
} // namespace nkentseu
