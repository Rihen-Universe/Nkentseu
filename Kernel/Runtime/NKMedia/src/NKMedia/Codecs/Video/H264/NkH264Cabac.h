// =============================================================================
// NKMedia/Codecs/Video/H264/NkH264Cabac.h
// -----------------------------------------------------------------------------
// Décodeur arithmétique binaire adaptatif au contexte (CABAC) — H.264 §9.3.
// C'est l'entropie des profils Main/High (la majorité des MP4 réels : téléphone,
// YouTube, export ffmpeg par défaut), par opposition au CAVLC baseline.
//
// Ce header fournit le MOTEUR (brique A) :
//   - NkCabacEngine : codIRange/codIOffset + curseur d'octets sur le RBSP.
//     InitEngine (§9.3.1.2), DecodeDecision (§9.3.3.2.1), DecodeBypass
//     (§9.3.3.2.3), DecodeTerminate (§9.3.3.2.4), RenormD (§9.3.3.2.2).
//   - NkCabacCtx : état d'un modèle de contexte (pStateIdx 0..63, valMPS 0/1).
//   - Tables de transition d'état rangeTabLPS[64][4], transIdxLPS[64],
//     transIdxMPS[64] (Tables 9-44 / 9-45).
//
// L'INITIALISATION des ~460 contextes depuis (m,n)+QP et le décodage des éléments
// de syntaxe (mb_type, résidus…) viennent dans les briques suivantes.
// Zero-STL, namespace nkentseu::media. From-scratch.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		// Un modèle de contexte : indice d'état de probabilité + symbole le plus probable.
		struct NkCabacCtx {
				uint8 pStateIdx = 0; // 0..63
				uint8 valMPS = 0;	 // 0 ou 1
		};

		// Table 9-44 : rangeTabLPS[pStateIdx][qCodIRangeIdx].
		static const uint8 kCabacRangeTabLPS[64][4] = {
			{128, 176, 208, 240}, {128, 167, 197, 227}, {128, 158, 187, 216}, {123, 150, 178, 205},
			{116, 142, 169, 195}, {111, 135, 160, 185}, {105, 128, 152, 175}, {100, 122, 144, 166},
			{95, 116, 137, 158},  {90, 110, 130, 150},  {85, 104, 123, 142},  {81, 99, 117, 135},
			{77, 94, 111, 128},	  {73, 89, 105, 122},	{69, 85, 100, 116},	  {66, 80, 95, 110},
			{62, 76, 90, 104},	  {59, 72, 86, 99},		{56, 69, 81, 94},	  {53, 65, 77, 89},
			{51, 62, 73, 85},	  {48, 59, 69, 80},		{46, 56, 66, 76},	  {43, 53, 63, 72},
			{41, 50, 59, 69},	  {39, 48, 56, 65},		{37, 45, 54, 62},	  {35, 43, 51, 59},
			{33, 41, 48, 56},	  {32, 39, 46, 53},		{30, 37, 43, 50},	  {28, 35, 41, 48},
			{27, 33, 39, 45},	  {26, 31, 37, 43},		{24, 30, 35, 41},	  {23, 28, 33, 39},
			{22, 27, 32, 37},	  {21, 26, 30, 35},		{20, 24, 29, 33},	  {19, 23, 27, 31},
			{18, 22, 26, 30},	  {17, 21, 25, 28},		{16, 20, 23, 27},	  {15, 19, 22, 25},
			{14, 18, 21, 24},	  {14, 17, 20, 23},		{13, 16, 19, 22},	  {12, 15, 18, 21},
			{12, 14, 17, 20},	  {11, 14, 16, 19},		{11, 13, 15, 18},	  {10, 12, 15, 17},
			{10, 12, 14, 16},	  {9, 11, 13, 15},		{9, 11, 12, 14},	  {8, 10, 12, 14},
			{8, 9, 11, 13},		  {7, 9, 11, 12},		{7, 9, 10, 12},		  {7, 8, 10, 11},
			{6, 8, 9, 11},		  {6, 7, 9, 10},		{6, 7, 8, 9},		  {2, 2, 2, 2},
		};

		// Table 9-45 : transitions d'état.
		static const uint8 kCabacTransIdxLPS[64] = {
			0,	0,	1,	2,	2,	4,	4,	5,	6,	7,	8,	9,	9,	11, 11, 12, 13, 13, 15, 15, 16, 16,
			18, 18, 19, 19, 21, 21, 23, 22, 23, 24, 24, 25, 26, 26, 27, 27, 28, 29, 29, 30, 30, 30,
			31, 32, 32, 33, 33, 33, 34, 34, 35, 35, 35, 36, 36, 36, 37, 37, 37, 38, 38, 63,
		};
		static const uint8 kCabacTransIdxMPS[64] = {
			1,	2,	3,	4,	5,	6,	7,	8,	9,	10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
			23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
			45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 62, 63,
		};

		// Moteur arithmétique CABAC : opère sur un RBSP dé-émulé, curseur en OCTETS.
		struct NkCabacEngine {
				const uint8 *data = nullptr;
				usize size = 0;	  // taille en octets
				usize bytePos = 0; // prochain octet à lire
				int32 bitPos = 0;  // bit courant dans l'octet (0..7, MSB-first) — usage interne renorm
				uint32 codIRange = 0;
				uint32 codIOffset = 0;

				// Lit UN bit MSB-first du flux (0 au-delà de la fin).
				uint32 ReadBit() {
					if (bytePos >= size)
						return 0;
					const uint32 b = (data[bytePos] >> (7 - bitPos)) & 1u;
					if (++bitPos == 8) {
						bitPos = 0;
						++bytePos;
					}
					return b;
				}

				// §9.3.1.2 : initialise le moteur. `startByte` = 1er octet du slice_data (aligné).
				void InitEngine(const uint8 *rbsp, usize nbytes, usize startByte) {
					data = rbsp;
					size = nbytes;
					bytePos = startByte;
					bitPos = 0;
					codIRange = 510;
					codIOffset = 0;
					for (int32 i = 0; i < 9; ++i)
						codIOffset = (codIOffset << 1) | ReadBit();
				}

				// §9.3.3.2.2 : renormalisation.
				void RenormD() {
					while (codIRange < 256) {
						codIRange <<= 1;
						codIOffset = (codIOffset << 1) | ReadBit();
					}
				}

				// §9.3.3.2.1 : décode un bin régulier avec le contexte `ctx` (mis à jour).
				uint32 DecodeDecision(NkCabacCtx &ctx) {
					const uint32 qIdx = (codIRange >> 6) & 3;
					const uint32 codIRangeLPS = kCabacRangeTabLPS[ctx.pStateIdx][qIdx];
					codIRange -= codIRangeLPS;
					uint32 binVal;
					if (codIOffset >= codIRange) {
						binVal = 1u - ctx.valMPS;
						codIOffset -= codIRange;
						codIRange = codIRangeLPS;
						if (ctx.pStateIdx == 0)
							ctx.valMPS = 1u - ctx.valMPS;
						ctx.pStateIdx = kCabacTransIdxLPS[ctx.pStateIdx];
					} else {
						binVal = ctx.valMPS;
						ctx.pStateIdx = kCabacTransIdxMPS[ctx.pStateIdx];
					}
					RenormD();
					return binVal;
				}

				// §9.3.3.2.3 : bin en contournement (équiprobable).
				uint32 DecodeBypass() {
					codIOffset = (codIOffset << 1) | ReadBit();
					if (codIOffset >= codIRange) {
						codIOffset -= codIRange;
						return 1;
					}
					return 0;
				}

				// §9.3.3.2.4 : bin de terminaison (end_of_slice_flag / I_PCM).
				uint32 DecodeTerminate() {
					codIRange -= 2;
					if (codIOffset >= codIRange)
						return 1; // pas de renorm : fin
					RenormD();
					return 0;
				}
		};

	} // namespace media
} // namespace nkentseu
