// =============================================================================
// NKMedia/Codecs/Aac/NkAacTns.cpp — Temporal Noise Shaping (ISO 14496-3 §4.6.9).
// =============================================================================
#include "NKMedia/Codecs/Aac/NkAacTns.h"

namespace nkentseu {
	namespace media {

		namespace {
			constexpr int32 TNS_MAX_ORDER = 20;
			constexpr int32 EIGHT_SHORT = 2;

			// 4 tables de coefficients TNS (ISO), sélectionnées par 2·coef_compress + coef_res.
			const float32 kTnsCoef[4][16] = {
				// [0] compress=0, res=3
				{0.0f, 0.4338837391f, 0.7818314825f, 0.9749279122f, -0.9848077530f, -0.8660254038f, -0.6427876097f,
				 -0.3420201433f, -0.4338837391f, -0.7818314825f, -0.9749279122f, -0.9749279122f, -0.9848077530f,
				 -0.8660254038f, -0.6427876097f, -0.3420201433f},
				// [1] compress=0, res=4
				{0.0f, 0.2079116908f, 0.4067366431f, 0.5877852523f, 0.7431448255f, 0.8660254038f, 0.9510565163f,
				 0.9945218954f, -0.9957341763f, -0.9618256432f, -0.8951632914f, -0.7980172273f, -0.6736956436f,
				 -0.5264321629f, -0.3612416662f, -0.1837495178f},
				// [2] compress=1, res=3
				{0.0f, 0.4338837391f, -0.6427876097f, -0.3420201433f, 0.9749279122f, 0.7818314825f, -0.6427876097f,
				 -0.3420201433f, -0.4338837391f, -0.7818314825f, -0.6427876097f, -0.3420201433f, -0.7818314825f,
				 -0.4338837391f, -0.6427876097f, -0.3420201433f},
				// [3] compress=1, res=4
				{0.0f, 0.2079116908f, 0.4067366431f, 0.5877852523f, -0.6736956436f, -0.5264321629f, -0.3612416662f,
				 -0.1837495178f, 0.9945218954f, 0.9510565163f, 0.8660254038f, 0.7431448255f, -0.6736956436f,
				 -0.5264321629f, -0.3612416662f, -0.1837495178f}};

			// Bande max où le TNS s'applique (tns_sbf_max, Main/LC ; [long, court]).
			const uint8 kTnsMaxBands[12][2] = {{31, 9},  {31, 9},	{34, 10}, {40, 14}, {42, 14}, {51, 14},
											   {46, 14}, {46, 14}, {42, 14}, {42, 14}, {42, 14}, {39, 14}};

			int32 IMin(int32 a, int32 b) {
				return a < b ? a : b;
			}
			int32 IMax(int32 a, int32 b) {
				return a > b ? a : b;
			}

			// Décode les coefficients bruts en LPC (réflexion → direct). Renvoie l'ordre.
			void DecodeCoef(int32 order, int32 coefRes, int32 coefCompress, const uint8 *coef, float32 *a) {
				const int32 tableIndex = 2 * (coefCompress != 0) + (coefRes != 0);
				const float32 *tab = kTnsCoef[tableIndex];
				float32 tmp2[TNS_MAX_ORDER + 1];
				float32 b[TNS_MAX_ORDER + 1];
				for (int32 i = 0; i < order; ++i)
					tmp2[i] = tab[coef[i] & 15];
				a[0] = 1.0f;
				for (int32 m = 1; m <= order; ++m) {
					a[m] = tmp2[m - 1];
					for (int32 i = 1; i < m; ++i)
						b[i] = a[i] + a[m] * a[m - i];
					for (int32 i = 1; i < m; ++i)
						a[i] = b[i];
				}
			}

			// Filtre tout-pôle : y[n] = x[n] − Σ_{j=1}^{order} lpc[j]·y[n−j], en place, incrément `inc`.
			void ArFilter(float32 *spectrum, int32 size, int32 inc, const float32 *lpc, int32 order) {
				float32 state[2 * TNS_MAX_ORDER] = {0};
				int32 stateIndex = 0;
				for (int32 i = 0; i < size; ++i) {
					float32 y = 0.0f;
					for (int32 j = 0; j < order; ++j)
						y += state[stateIndex + j] * lpc[j + 1];
					y = *spectrum - y;
					--stateIndex;
					if (stateIndex < 0)
						stateIndex = order - 1;
					state[stateIndex] = state[stateIndex + order] = y;
					*spectrum = y;
					spectrum += inc;
				}
			}
		} // namespace

		void NkAacTns::Apply(const NkAacIcs &ics, int32 sfIndex, float32 *spec) {
			if (!ics.tnsDataPresent)
				return;
			const int32 nshort = NkAacIcs::kFrameLen / 8; // 128
			const int32 isShort = (ics.windowSequence == EIGHT_SHORT) ? 1 : 0;
			const int32 maxBand = (sfIndex >= 0 && sfIndex < 12) ? (int32)kTnsMaxBands[sfIndex][isShort] : ics.numSwb;
			const int32 swbMax = (int32)ics.swbOffset[ics.numSwb];

			for (int32 w = 0; w < ics.numWindows; ++w) {
				int32 bottom = ics.numSwb;
				for (int32 f = 0; f < (int32)ics.tns.nFilt[w] && f < 4; ++f) {
					const int32 top = bottom;
					bottom = IMax(top - (int32)ics.tns.length[w][f], 0);
					const int32 order = IMin((int32)ics.tns.order[w][f], TNS_MAX_ORDER);
					if (order == 0)
						continue;
					float32 lpc[TNS_MAX_ORDER + 1];
					DecodeCoef(order, (int32)ics.tns.coefRes[w], (int32)ics.tns.coefCompress[w][f], ics.tns.coef[w][f],
							   lpc);

					int32 start = IMin(bottom, maxBand);
					start = IMin(start, ics.maxSfb);
					start = IMin((int32)ics.swbOffset[start], swbMax);
					int32 end = IMin(top, maxBand);
					end = IMin(end, ics.maxSfb);
					end = IMin((int32)ics.swbOffset[end], swbMax);
					const int32 size = end - start;
					if (size <= 0)
						continue;

					int32 inc = 1;
					if (ics.tns.direction[w][f]) {
						inc = -1;
						start = end - 1;
					}
					ArFilter(&spec[w * nshort + start], size, inc, lpc, order);
				}
			}
		}

		// ---------------------------------------------------------------------------
		bool NkAacTns::SelfTest() {
			auto approx = [](float32 a, float32 b) -> bool {
				const float32 d = a - b;
				return d > -1e-4f && d < 1e-4f;
			};
			// 1) Filtre AR ordre 1, lpc[1]=0.5 sur une impulsion → réponse géométrique :
			//    y[0]=1, y[n] = -0.5·y[n-1] → 1, -0.5, 0.25, -0.125, ...
			{
				float32 lpc[2] = {1.0f, 0.5f};
				float32 s[6] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
				ArFilter(s, 6, 1, lpc, 1);
				float32 exp = 1.0f;
				for (int32 i = 0; i < 6; ++i) {
					if (!approx(s[i], exp))
						return false;
					exp *= -0.5f;
				}
			}
			// 2) DecodeCoef ordre 1 : a[1] = tns_coef[table][coef], a[0]=1.
			{
				float32 lpc[2];
				uint8 coef[1] = {2};
				DecodeCoef(1, 0, 0, coef, lpc); // table [0], coef=2 → 0.7818314825
				if (!approx(lpc[0], 1.0f) || !approx(lpc[1], 0.7818314825f))
					return false;
			}
			// 3) TNS non présent → spectre inchangé.
			{
				NkAacIcs ics;
				ics.tnsDataPresent = false;
				float32 spec[8] = {1, 2, 3, 4, 5, 6, 7, 8};
				NkAacTns::Apply(ics, 4, spec);
				for (int32 i = 0; i < 8; ++i)
					if (!approx(spec[i], (float32)(i + 1)))
						return false;
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
