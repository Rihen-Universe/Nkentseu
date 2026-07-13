// =============================================================================
// NKMedia/Codecs/Aac/NkAacDequant.cpp — déquantification + scaling (ISO 14496-3).
// =============================================================================
#include "NKMedia/Codecs/Aac/NkAacDequant.h"

#include <cmath> // powf (x^(4/3), 2^…) — fonctions math C

namespace nkentseu {
	namespace media {

		namespace {
			constexpr int32 NOISE_HCB = 13;
			constexpr int32 INTENSITY_HCB2 = 14;
			constexpr int32 INTENSITY_HCB = 15;

			// iquant : sign(q)·|q|^(4/3).
			inline float32 IQuant(int32 q) {
				if (q == 0)
					return 0.0f;
				const float32 a = ::powf((float32)(q < 0 ? -q : q), 4.0f / 3.0f);
				return q < 0 ? -a : a;
			}
		} // namespace

		void NkAacDequant::Apply(const NkAacIcs &ics, float32 *specOut) {
			// Copie locale des coefficients quantifiés (pour appliquer les pulses).
			int32 q[NkAacIcs::kFrameLen];
			for (int32 i = 0; i < NkAacIcs::kFrameLen; ++i)
				q[i] = ics.specQuant[i];

			// 1) Pulses (fenêtres longues uniquement).
			if (ics.pulseDataPresent && ics.windowSequence != 2) {
				const int32 startOff = (int32)ics.swbOffset[ics.pulse.pulseStartSfb];
				const int32 maxOff = (int32)ics.swbOffset[ics.numSwb];
				int32 k = startOff < maxOff ? startOff : maxOff;
				for (int32 i = 0; i <= (int32)ics.pulse.numberPulse; ++i) {
					k += (int32)ics.pulse.pulseOffset[i];
					if (k >= NkAacIcs::kFrameLen)
						break;
					if (q[k] > 0)
						q[k] += (int32)ics.pulse.pulseAmp[i];
					else
						q[k] -= (int32)ics.pulse.pulseAmp[i];
				}
			}

			for (int32 i = 0; i < NkAacIcs::kFrameLen; ++i)
				specOut[i] = 0.0f;

			// 2-4) Déquant + scaling + désentrelacement (cf. quant_to_spec).
			int32 kk = 0;			   // index linéaire dans le layout groupé
			int32 gindex = 0;		   // base du groupe dans la sortie désentrelacée
			const int32 winInc = (int32)ics.swbOffset[ics.numSwb]; // longueur de fenêtre (1024/128)
			for (int32 g = 0; g < ics.numWindowGroups; ++g) {
				int32 j = 0;
				for (int32 sfb = 0; sfb < ics.numSwb; ++sfb) {
					const int32 width = (int32)ics.swbOffset[sfb + 1] - (int32)ics.swbOffset[sfb];
					const int32 sf = (sfb < ics.maxSfb) ? (int32)ics.scaleFactors[g][sfb] : 0;
					const int32 cb = (sfb < ics.maxSfb) ? (int32)ics.sfbCb[g][sfb] : 0;
					// Bandes intensity/PNS : 0 ici (traitées par la brique stéréo/bruit).
					const bool special = (cb == INTENSITY_HCB || cb == INTENSITY_HCB2 || cb == NOISE_HCB);
					const float32 scf = special ? 0.0f : ::powf(2.0f, 0.25f * (float32)(sf - 100));
					for (int32 win = 0; win < ics.windowGroupLength[g]; ++win) {
						const int32 wa = gindex + win * winInc + j;
						for (int32 bin = 0; bin < width; ++bin) {
							const int32 x = (kk < NkAacIcs::kFrameLen) ? q[kk] : 0;
							++kk;
							const int32 dst = wa + bin;
							if (dst < NkAacIcs::kFrameLen)
								specOut[dst] = IQuant(x) * scf;
						}
					}
					j += width;
				}
				gindex += winInc * ics.windowGroupLength[g];
			}
		}

		// ---------------------------------------------------------------------------
		bool NkAacDequant::SelfTest() {
			// Cas fenêtre longue synthétique : 1 bande de largeur 4, sf variable,
			// quant connus → vérifie iquant·gain.
			auto approx = [](float32 a, float32 b) -> bool {
				const float32 d = a - b;
				return (d > -1e-3f && d < 1e-3f);
			};

			// sf = 100 → gain 1. quant = [3, -2, 1, 0].
			{
				NkAacIcs ics;
				ics.windowSequence = 0;
				ics.numWindows = 1;
				ics.numWindowGroups = 1;
				ics.windowGroupLength[0] = 1;
				ics.numSwb = 1;
				ics.maxSfb = 1;
				ics.swbOffset[0] = 0;
				ics.swbOffset[1] = 4;
				ics.sfbCb[0][0] = 1;
				ics.scaleFactors[0][0] = 100;
				ics.specQuant[0] = 3;
				ics.specQuant[1] = -2;
				ics.specQuant[2] = 1;
				ics.specQuant[3] = 0;
				float32 spec[NkAacIcs::kFrameLen];
				NkAacDequant::Apply(ics, spec);
				if (!approx(spec[0], ::powf(3.0f, 4.0f / 3.0f)))
					return false;
				if (!approx(spec[1], -::powf(2.0f, 4.0f / 3.0f)))
					return false;
				if (!approx(spec[2], 1.0f) || !approx(spec[3], 0.0f))
					return false;
			}

			// sf = 108 → gain 2^((108-100)/4) = 2^2 = 4.
			{
				NkAacIcs ics;
				ics.windowSequence = 0;
				ics.numWindows = 1;
				ics.numWindowGroups = 1;
				ics.windowGroupLength[0] = 1;
				ics.numSwb = 1;
				ics.maxSfb = 1;
				ics.swbOffset[0] = 0;
				ics.swbOffset[1] = 2;
				ics.sfbCb[0][0] = 5;
				ics.scaleFactors[0][0] = 108;
				ics.specQuant[0] = 1;
				ics.specQuant[1] = 8;
				float32 spec[NkAacIcs::kFrameLen];
				NkAacDequant::Apply(ics, spec);
				if (!approx(spec[0], 4.0f)) // 1^(4/3)=1, ×4
					return false;
				if (!approx(spec[1], ::powf(8.0f, 4.0f / 3.0f) * 4.0f)) // 16×4=64
					return false;
			}

			// Désentrelacement court : 2 groupes, winInc=4, vérifie les positions de sortie.
			{
				NkAacIcs ics;
				ics.windowSequence = 2;
				ics.numWindows = 8;
				ics.numWindowGroups = 2;
				ics.windowGroupLength[0] = 1;
				ics.windowGroupLength[1] = 1;
				ics.numSwb = 1;
				ics.maxSfb = 1;
				ics.swbOffset[0] = 0;
				ics.swbOffset[1] = 4; // winInc = 4
				ics.sfbCb[0][0] = 1;
				ics.sfbCb[1][0] = 1;
				ics.scaleFactors[0][0] = 100;
				ics.scaleFactors[1][0] = 100;
				// groupe 0 : coeffs 0..3 ; groupe 1 : coeffs 4..7 (layout groupé).
				for (int32 i = 0; i < 8; ++i)
					ics.specQuant[i] = i + 1; // 1..8
				float32 spec[NkAacIcs::kFrameLen];
				NkAacDequant::Apply(ics, spec);
				// groupe 0 → sortie [0..3], groupe 1 → gindex=4 → sortie [4..7].
				if (!approx(spec[0], 1.0f) || !approx(spec[4], ::powf(5.0f, 4.0f / 3.0f)))
					return false;
			}

			// Pulses : sf=100, quant[2]=3, pulse ajoute 5 à la position 2 → quant 8.
			{
				NkAacIcs ics;
				ics.windowSequence = 0;
				ics.numWindows = 1;
				ics.numWindowGroups = 1;
				ics.windowGroupLength[0] = 1;
				ics.numSwb = 1;
				ics.maxSfb = 1;
				ics.swbOffset[0] = 0;
				ics.swbOffset[1] = 4;
				ics.sfbCb[0][0] = 1;
				ics.scaleFactors[0][0] = 100;
				ics.specQuant[2] = 3;
				ics.pulseDataPresent = true;
				ics.pulse.numberPulse = 0;	 // 1 pulse
				ics.pulse.pulseStartSfb = 0; // swbOffset[0]=0
				ics.pulse.pulseOffset[0] = 2;
				ics.pulse.pulseAmp[0] = 5;
				float32 spec[NkAacIcs::kFrameLen];
				NkAacDequant::Apply(ics, spec);
				if (!approx(spec[2], ::powf(8.0f, 4.0f / 3.0f))) // 3+5=8
					return false;
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
