// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkIndicesTables.cpp
// Tables d'index SILK (pitch/LTP/uniform) — GÉNÉRÉES depuis libopus
// (tables_pitch_lag.c, tables_LTP.c, tables_other.c). Bit-exact (vérifié).
// =============================================================================
#include "NkSilkIndices.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

const uint8 kSilk_pitch_lag_iCDF[32] = {
       253,    250,    244,    233,    212,    182,    150,    131,
       120,    110,     98,     85,     72,     60,     49,     40,
        32,     25,     19,     15,     13,     11,      9,      8,
         7,      6,      5,      4,      3,      2,      1,      0
};

const uint8 kSilk_pitch_delta_iCDF[21] = {
       210,    208,    206,    203,    199,    193,    183,    168,
       142,    104,     74,     52,     37,     27,     20,     14,
        10,      6,      4,      2,      0
};

const uint8 kSilk_pitch_contour_iCDF[34] = {
       223,    201,    183,    167,    152,    138,    124,    111,
        98,     88,     79,     70,     62,     56,     50,     44,
        39,     35,     31,     27,     24,     21,     18,     16,
        14,     12,     10,      8,      6,      4,      3,      2,
         1,      0
};

const uint8 kSilk_pitch_contour_NB_iCDF[11] = {
       188,    176,    155,    138,    119,     97,     67,     43,
        26,     10,      0
};

const uint8 kSilk_pitch_contour_10_ms_iCDF[12] = {
       165,    119,     80,     61,     47,     35,     27,     20,
        14,      9,      4,      0
};

const uint8 kSilk_pitch_contour_10_ms_NB_iCDF[3] = {
       113,     63,      0
};

const uint8 kSilk_LTP_per_index_iCDF[3] = {
       179,     99,      0
};

const uint8 kSilk_LTP_gain_iCDF_0[8] = {
        71,     56,     43,     30,     21,     12,      6,      0
};

const uint8 kSilk_LTP_gain_iCDF_1[16] = {
       199,    165,    144,    124,    109,     96,     84,     71,
        61,     51,     42,     32,     23,     15,      8,      0
};

const uint8 kSilk_LTP_gain_iCDF_2[32] = {
       241,    225,    211,    199,    187,    175,    164,    153,
       142,    132,    123,    114,    105,     96,     88,     80,
        72,     64,     57,     50,     44,     38,     33,     29,
        24,     20,     16,     12,      9,      5,      2,      0
};

	const uint8 kSilk_LTPscale_iCDF[3] = {128, 64, 0};
	const uint8 kSilk_uniform4_iCDF[4] = {192, 128, 64, 0};
	const uint8 kSilk_uniform6_iCDF[6] = {213, 171, 128, 85, 43, 0};
	const uint8 kSilk_uniform8_iCDF[8] = {224, 192, 160, 128, 96, 64, 32, 0};
	const uint8 *const kSilk_LTP_gain_iCDF_ptrs[3] = {kSilk_LTP_gain_iCDF_0, kSilk_LTP_gain_iCDF_1,
													  kSilk_LTP_gain_iCDF_2};

	} // namespace media
} // namespace nkentseu
