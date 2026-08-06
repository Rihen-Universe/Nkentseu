// =============================================================================
// NkQKGpuShaderCommon.h — fragments NkSL PARTAGÉS par les noyaux K-quants GPU.
//
// POURQUOI CE FICHIER EXISTE
// --------------------------
// Q4_K et Q6_K décodent tous deux une super-échelle stockée en fp16, et cette
// conversion doit être la TRANSCRIPTION EXACTE de NkF16BitsToF32 (NKTensor/
// NkFp16.h) — branche dénormale comprise — sans quoi la comparaison bit-à-bit
// avec la référence CPU (NkGGUFDequantizeRaw) n'a plus de sens. Recopier ce
// décodeur dans chaque module créerait exactement le risque que le dépôt a déjà
// payé ailleurs : DEUX transcriptions du même algorithme qui divergent au fil
// des retouches, et un écart qu'on met des heures à localiser.
//
// Le fragment est une CHAÎNE de source NkSL, pas du C++ : il est concaténé au
// noyau appelant. Il ne suppose rien du reste du shader (aucun buffer, aucun
// uniforme) — c'est une fonction pure sur des bits.
//
// Zéro STL.
// =============================================================================
#pragma once

namespace nkentseu {
	namespace ai {
		namespace infer {

			// -----------------------------------------------------------------
			// Décodage fp16 -> f32, en ENTIERS.
			//
			// On n'utilise PAS `unpackHalf2x16` : sa traduction HLSL/MSL n'est pas
			// fiable sur toute la chaîne NkSL -> SPIR-V -> SPIRV-Cross, alors que
			// des décalages et des masques passent partout à l'identique.
			//
			// Les masques sont écrits en HEXADÉCIMAL : `0x8000u` (bit de signe),
			// `0x1Fu` (5 bits d'exposant), `0x3FFu` (10 bits de mantisse),
			// `0x7F800000u` (exposant f32 tout à 1). En décimal — 32768u, 31u,
			// 1023u, 2139095040u — la relecture contre la spec IEEE-754 est
			// impossible à l'œil. Le lexer NkSL accepte 0x/0b depuis 2026-08-05 ;
			// c'était la raison d'être de cet ajout.
			//
			// Les constantes qui sont des NOMBRES et non des motifs de bits restent
			// en décimal : 113 (= 127 - 15 + 1, recentrage d'exposant), 112
			// (= 127 - 15), 23 et 13 (positions de champs). Les écrire en hexa les
			// rendrait moins lisibles, pas plus.
			// -----------------------------------------------------------------
			inline constexpr const char *kNkQKGpuF16Helper = R"NKSL(
float nkqkF16(uint h) {
    uint sign = (h & 0x8000u) << 16u;
    uint e = (h >> 10u) & 0x1Fu;
    uint m = h & 0x3FFu;
    uint bits = sign;
    if (e == 0u) {
        if (m != 0u) {
            uint mm = m;
            uint sh = 0u;
            // Normalisation d'un dénormal : décaler jusqu'au bit 10. Dix tours
            // suffisent (la mantisse fait 10 bits) et BORNENT la boucle — un GPU
            // n'aime pas les while dont il ne peut pas prouver la terminaison.
            for (uint i = 0u; i < 10u; i = i + 1u) {
                if ((mm & 0x400u) == 0u) {
                    mm = mm << 1u;
                    sh = sh + 1u;
                }
            }
            bits = sign | ((113u - sh) << 23u) | ((mm & 0x3FFu) << 13u);
        }
    } else {
        if (e == 0x1Fu) {
            bits = sign | 0x7F800000u | (m << 13u);
        } else {
            bits = sign | ((e + 112u) << 23u) | (m << 13u);
        }
    }
    return uintBitsToFloat(bits);
}
)NKSL";

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
