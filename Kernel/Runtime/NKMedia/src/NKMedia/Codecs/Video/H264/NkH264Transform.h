// =============================================================================
// NKMedia/Codecs/Video/H264/NkH264Transform.h
// -----------------------------------------------------------------------------
// H.264 (ISO 14496-10) — transformée entière 4×4 (cœur DCT-like, sans multiplications
// irrationnelles) + transformée de Hadamard 4×4 (coefficients DC de l'Intra_16×16) +
// quantification/déquantification pilotées par QP (tables normAdjust/MF du standard).
// Réécrit à la sauce Nkentseu. Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		struct NkH264Transform {
			public:
				// Cœur : transformée 4×4 directe/inverse (blocs 16 coeffs, raster).
				static void Forward4x4(const int32 in[16], int32 out[16]);
				static void Inverse4x4(const int32 in[16], int32 out[16]); // inclut (·+32)>>6

				// Hadamard 4×4 (DC de l'Intra_16×16 / chroma).
				static void HadamardForward4x4(const int32 in[16], int32 out[16]);
				static void HadamardInverse4x4(const int32 in[16], int32 out[16]);

				// Quantifie un bloc 4×4 (résidu transformé) → niveaux `lvl`. `qp` 0..51. `intra` change l'arrondi.
				static void Quant4x4(const int32 coef[16], int32 lvl[16], int32 qp, bool intra);
				// Déquantifie les niveaux → coefficients (avant transformée inverse).
				// `w` (optionnel) = liste de quantification 4×4 en ordre RASTER (16 valeurs, profil High
				// avec scaling matrices custom) ; nullptr = liste PLATE (weightScale=16, chemin prouvé).
				static void Dequant4x4(const int32 lvl[16], int32 coef[16], int32 qp, const uint8 *w = nullptr);

				// Quant/dequant du DC (après Hadamard), pour l'Intra_16×16 luma (nDC=16) / chroma (nDC=4).
				// `w00` = weightScale de la position (0,0) de la liste (16 = plat).
				static void QuantDC(const int32 dc[16], int32 lvl[16], int32 n, int32 qp, bool intra);
				static void DequantDC(const int32 lvl[16], int32 dc[16], int32 n, int32 qp, int32 w00 = 16);

				// Transformée 8×8 (profil High). Inverse §8.5.13.2 (blocs 64 coeffs raster, inclut (·+32)>>6)
				// et déquantification 8×8 §8.5.13.1 (scaling lists FLAT = défaut x264).
				static void Inverse8x8(const int32 in[64], int32 out[64]);
				// `w` (optionnel) = liste 8×8 en ordre RASTER (64 valeurs) ; nullptr = plate (weightScale=16).
				static void Dequant8x8(const int32 lvl[64], int32 coef[64], int32 qp, const uint8 *w = nullptr);

				// Chroma 4:2:0 : transformée DC 2×2 (Hadamard, directe = inverse), quant/dequant DC chroma
				// (échelle spec 8.5.11 : facteur ×16, décalage >>5), et table de QP chroma (qPi → QPc).
				static void Hadamard2x2(const int32 in[4], int32 out[4]);
				static void QuantChromaDC(const int32 f[4], int32 lvl[4], int32 qp, bool intra);
				static void DequantChromaDC(const int32 g[4], int32 dc[4], int32 qp, int32 w00 = 16);
				static int32 ChromaQp(int32 lumaQp);

				static bool SelfTest();
		};

	} // namespace media
} // namespace nkentseu
