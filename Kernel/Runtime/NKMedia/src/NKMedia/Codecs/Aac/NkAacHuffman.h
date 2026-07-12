// =============================================================================
// NKMedia/Codecs/Aac/NkAacHuffman.h
// -----------------------------------------------------------------------------
// Décodage Huffman AAC-LC (ISO/IEC 14496-3) : spectral_data (codebooks 1-11, par
// paires ou quadruplets, signés ou non + échappement du codebook 11) et
// scale_factor_data (codebook 0, différentiel). S'appuie sur les tables
// reconstruites dans NkAacHuffmanTables.cpp. Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMedia/Codecs/Aac/NkAacBitReader.h"

namespace nkentseu {
	namespace media {

		// Une entrée de codebook : codeword `cw` de longueur `len` bits (MSB-first) →
		// valeurs `v` (2 ou 4 selon le codebook ; magnitudes pour les codebooks non signés).
		struct NkAacHcbEntry {
				uint8 len;	 // longueur du codeword en bits (1..19)
				uint32 cw;	 // codeword (MSB-first), jusqu'à 19 bits (cb11)
				int8 v[4];
		};

		// Un codebook : `entries` triées par (len, cw), `count` entrées, `dim` valeurs par
		// codeword (1 = scalefactor, 2 = paire, 4 = quadruplet).
		struct NkAacHcbBook {
				const NkAacHcbEntry *entries;
				int32 count;
				int32 dim;
		};

		// Index 0 = codebook des facteurs d'échelle ; 1..11 = codebooks spectraux.
		extern const NkAacHcbBook kAacHcbBooks[12];

		struct NkAacHuffman {
			public:
				// Décode un codeword du codebook spectral `cb` (1..11) → `out` (dim valeurs
				// signées ; bits de signe des codebooks non signés + échappement cb11 appliqués).
				static void DecodeSpectral(NkAacBitReader &br, int32 cb, int32 *out);

				// Décode un facteur d'échelle différentiel → valeur brute 0..120
				// (l'appelant soustrait l'offset 60).
				static int32 DecodeScaleFactor(NkAacBitReader &br);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
