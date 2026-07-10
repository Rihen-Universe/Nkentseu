// =============================================================================
// NKMedia/Codecs/Video/Mpeg1/NkMpeg1Tables.cpp — tables MPEG-1 (ISO 11172-2).
// =============================================================================
#include "NKMedia/Codecs/Video/Mpeg1/NkMpeg1Tables.h"

namespace nkentseu {
	namespace media {

		namespace {
			const uint16 kIntraMatrix[64] = {8,  16, 19, 22, 26, 27, 29, 34, 16, 16, 22, 24, 27, 29, 34, 37,
											 19, 22, 26, 27, 29, 34, 34, 38, 22, 22, 26, 27, 29, 34, 37, 40,
											 22, 26, 27, 29, 32, 35, 40, 48, 26, 27, 29, 32, 35, 40, 48, 58,
											 26, 27, 29, 34, 38, 46, 56, 69, 27, 29, 35, 38, 46, 56, 69, 83};

			const uint8 kZigZag[64] = {0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
									   12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
									   35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
									   58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

			const uint16 kDcLumCode[12] = {0x4, 0x0, 0x1, 0x5, 0x6, 0xe, 0x1e, 0x3e, 0x7e, 0xfe, 0x1fe, 0x1ff};
			const uint8 kDcLumBits[12] = {3, 2, 2, 3, 3, 4, 5, 6, 7, 8, 9, 9};
			const uint16 kDcChromaCode[12] = {0x0, 0x1, 0x2, 0x6, 0xe, 0x1e, 0x3e, 0x7e, 0xfe, 0x1fe, 0x3fe, 0x3ff};
			const uint8 kDcChromaBits[12] = {2, 2, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10};

			// VLC AC (run/level) — {code, bits}. Indices 0..110 = couples, 111 = escape, 112 = EOB.
			const uint16 kAcVlc[113][2] = {
				{0x3, 2},	{0x4, 4},	{0x5, 5},	{0x6, 7},  {0x26, 8},  {0x21, 8},  {0xa, 10},  {0x1d, 12},
				{0x18, 12}, {0x13, 12}, {0x10, 12}, {0x1a, 13}, {0x19, 13}, {0x18, 13}, {0x17, 13}, {0x1f, 14},
				{0x1e, 14}, {0x1d, 14}, {0x1c, 14}, {0x1b, 14}, {0x1a, 14}, {0x19, 14}, {0x18, 14}, {0x17, 14},
				{0x16, 14}, {0x15, 14}, {0x14, 14}, {0x13, 14}, {0x12, 14}, {0x11, 14}, {0x10, 14}, {0x18, 15},
				{0x17, 15}, {0x16, 15}, {0x15, 15}, {0x14, 15}, {0x13, 15}, {0x12, 15}, {0x11, 15}, {0x10, 15},
				{0x3, 3},	{0x6, 6},	{0x25, 8},	{0xc, 10}, {0x1b, 12}, {0x16, 13}, {0x15, 13}, {0x1f, 15},
				{0x1e, 15}, {0x1d, 15}, {0x1c, 15}, {0x1b, 15}, {0x1a, 15}, {0x19, 15}, {0x13, 16}, {0x12, 16},
				{0x11, 16}, {0x10, 16}, {0x5, 4},	{0x4, 7},  {0xb, 10},  {0x14, 12}, {0x14, 13}, {0x7, 5},
				{0x24, 8},	{0x1c, 12}, {0x13, 13}, {0x6, 5},  {0xf, 10},  {0x12, 12}, {0x7, 6},   {0x9, 10},
				{0x12, 13}, {0x5, 6},	{0x1e, 12}, {0x14, 16}, {0x4, 6},   {0x15, 12}, {0x7, 7},   {0x11, 12},
				{0x5, 7},	{0x11, 13}, {0x27, 8},	{0x10, 13}, {0x23, 8},  {0x1a, 16}, {0x22, 8},   {0x19, 16},
				{0x20, 8},	{0x18, 16}, {0xe, 10},	{0x17, 16}, {0xd, 10},  {0x16, 16}, {0x8, 10},   {0x15, 16},
				{0x1f, 12}, {0x1a, 12}, {0x19, 12}, {0x17, 12}, {0x16, 12}, {0x1f, 13}, {0x1e, 13}, {0x1d, 13},
				{0x1c, 13}, {0x1b, 13}, {0x1f, 16}, {0x1e, 16}, {0x1d, 16}, {0x1c, 16}, {0x1b, 16},
				{0x1, 6}, // escape
				{0x2, 2}, // EOB
			};

			const int8 kAcLevel[111] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
										20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
										39, 40, 1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
										18, 1,  2,  3,  4,  5,  1,  2,  3,  4,  1,  2,  3,  1,  2,  3,  1,  2,  3,
										1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  2,  1,  1,  1,
										1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1};

			const int8 kAcRun[111] = {0,	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	0,
									  0,	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
									  1,	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4,	4,
									  5,	5, 5, 6, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15,
									  15, 16, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
		} // namespace

		const uint16 *NkMpeg1Tables::IntraMatrix() {
			return kIntraMatrix;
		}
		const uint8 *NkMpeg1Tables::ZigZag() {
			return kZigZag;
		}
		const uint16 *NkMpeg1Tables::DcLumCode() {
			return kDcLumCode;
		}
		const uint8 *NkMpeg1Tables::DcLumBits() {
			return kDcLumBits;
		}
		const uint16 *NkMpeg1Tables::DcChromaCode() {
			return kDcChromaCode;
		}
		const uint8 *NkMpeg1Tables::DcChromaBits() {
			return kDcChromaBits;
		}
		const uint16 (*NkMpeg1Tables::AcVlc())[2] {
			return kAcVlc;
		}
		const int8 *NkMpeg1Tables::AcRun() {
			return kAcRun;
		}
		const int8 *NkMpeg1Tables::AcLevel() {
			return kAcLevel;
		}

		int32 NkMpeg1Tables::FindAc(int32 run, int32 absLevel) {
			for (int32 i = 0; i < kAcCount; ++i)
				if ((int32)kAcRun[i] == run && (int32)kAcLevel[i] == absLevel)
					return i;
			return -1;
		}

	} // namespace media
} // namespace nkentseu
