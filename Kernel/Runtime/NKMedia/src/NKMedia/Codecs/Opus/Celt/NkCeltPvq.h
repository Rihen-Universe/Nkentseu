// =============================================================================
// NKMedia/Codecs/Opus/Celt/NkCeltPvq.h
// -----------------------------------------------------------------------------
// Décodeur Opus/CELT — PVQ / CWRS (libopus cwrs.c). Chaque bande code sa FORME
// spectrale comme un vecteur de `K` pulses signés sur `N` dimensions, énuméré de
// façon combinatoire (Combinatorial Warehouse-Rank-Select) : vecteur ↔ index dans
// [0, V(N,K)). Fournit encode/decode des pulses + le comptage V(N,K). Aller-retour
// (vecteur → index → vecteur) prouvé en self-test. Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Codecs/Opus/NkOpusRange.h"

namespace nkentseu {
	namespace media {

		struct NkCeltPvq {
			public:
				// Nombre de vecteurs de pulses de dimension n avec k pulses (V(n,k)).
				static uint32 V(int32 n, int32 k);

				// Décode un vecteur de pulses `y` (n dimensions, k pulses) depuis le range decoder.
				static void DecodePulses(NkOpusRangeDecoder &dec, int32 *y, int32 n, int32 k);
				// Encode un vecteur de pulses `y` (n, k) dans le range encoder.
				static void EncodePulses(NkOpusRangeEncoder &enc, const int32 *y, int32 n, int32 k);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
