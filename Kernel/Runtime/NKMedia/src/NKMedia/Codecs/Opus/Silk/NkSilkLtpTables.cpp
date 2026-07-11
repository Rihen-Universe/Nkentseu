// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkLtpTables.cpp
// Tables pitch/LTP — GÉNÉRÉES depuis libopus (pitch_est_tables.c, tables_LTP.c,
// tables_other.c). Contenu numérique bit-exact (vérifié par diff). Accès via
// NkSilkLtpTables::*. Ne pas éditer à la main.
// =============================================================================
#include "NkSilkLtp.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {
		namespace {

#define PE_MAX_NB_SUBFR 4
#define PE_NB_CBKS_STAGE2_EXT 11
#define PE_NB_CBKS_STAGE2_10MS 3
#define PE_NB_CBKS_STAGE3_MAX 34
#define PE_NB_CBKS_STAGE3_10MS 12
#define NB_LTP_CBKS 3

static const int8 kSilk_CB_lags_stage2_10_ms[ PE_MAX_NB_SUBFR >> 1][ PE_NB_CBKS_STAGE2_10MS ] =
{
    {0, 1, 0},
    {0, 0, 1}
};

static const int8 kSilk_CB_lags_stage3_10_ms[ PE_MAX_NB_SUBFR >> 1 ][ PE_NB_CBKS_STAGE3_10MS ] =
{
    { 0, 0, 1,-1, 1,-1, 2,-2, 2,-2, 3,-3},
    { 0, 1, 0, 1,-1, 2,-1, 2,-2, 3,-2, 3}
};

static const int8 kSilk_CB_lags_stage2[ PE_MAX_NB_SUBFR ][ PE_NB_CBKS_STAGE2_EXT ] =
{
    {0, 2,-1,-1,-1, 0, 0, 1, 1, 0, 1},
    {0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0},
    {0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0},
    {0,-1, 2, 1, 0, 1, 1, 0, 0,-1,-1}
};

static const int8 kSilk_CB_lags_stage3[ PE_MAX_NB_SUBFR ][ PE_NB_CBKS_STAGE3_MAX ] =
{
    {0, 0, 1,-1, 0, 1,-1, 0,-1, 1,-2, 2,-2,-2, 2,-3, 2, 3,-3,-4, 3,-4, 4, 4,-5, 5,-6,-5, 6,-7, 6, 5, 8,-9},
    {0, 0, 1, 0, 0, 0, 0, 0, 0, 0,-1, 1, 0, 0, 1,-1, 0, 1,-1,-1, 1,-1, 2, 1,-1, 2,-2,-2, 2,-2, 2, 2, 3,-3},
    {0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1,-1, 1, 0, 0, 2, 1,-1, 2,-1,-1, 2,-1, 2, 2,-1, 3,-2,-2,-2, 3},
    {0, 1, 0, 0, 1, 0, 1,-1, 2,-1, 2,-1, 2, 3,-2, 3,-2,-2, 4, 4,-3, 5,-3,-4, 6,-4, 6, 5,-5, 8,-6,-5,-7, 9}
};

static const int8 kSilk_LTP_gain_vq_0[8][5] =
{
{
         4,      6,     24,      7,      5
},
{
         0,      0,      2,      0,      0
},
{
        12,     28,     41,     13,     -4
},
{
        -9,     15,     42,     25,     14
},
{
         1,     -2,     62,     41,     -9
},
{
       -10,     37,     65,     -4,      3
},
{
        -6,      4,     66,      7,     -8
},
{
        16,     14,     38,     -3,     33
}
};

static const int8 kSilk_LTP_gain_vq_1[16][5] =
{
{
        13,     22,     39,     23,     12
},
{
        -1,     36,     64,     27,     -6
},
{
        -7,     10,     55,     43,     17
},
{
         1,      1,      8,      1,      1
},
{
         6,    -11,     74,     53,     -9
},
{
       -12,     55,     76,    -12,      8
},
{
        -3,      3,     93,     27,     -4
},
{
        26,     39,     59,      3,     -8
},
{
         2,      0,     77,     11,      9
},
{
        -8,     22,     44,     -6,      7
},
{
        40,      9,     26,      3,      9
},
{
        -7,     20,    101,     -7,      4
},
{
         3,     -8,     42,     26,      0
},
{
       -15,     33,     68,      2,     23
},
{
        -2,     55,     46,     -2,     15
},
{
         3,     -1,     21,     16,     41
}
};

static const int8 kSilk_LTP_gain_vq_2[32][5] =
{
{
        -6,     27,     61,     39,      5
},
{
       -11,     42,     88,      4,      1
},
{
        -2,     60,     65,      6,     -4
},
{
        -1,     -5,     73,     56,      1
},
{
        -9,     19,     94,     29,     -9
},
{
         0,     12,     99,      6,      4
},
{
         8,    -19,    102,     46,    -13
},
{
         3,      2,     13,      3,      2
},
{
         9,    -21,     84,     72,    -18
},
{
       -11,     46,    104,    -22,      8
},
{
        18,     38,     48,     23,      0
},
{
       -16,     70,     83,    -21,     11
},
{
         5,    -11,    117,     22,     -8
},
{
        -6,     23,    117,    -12,      3
},
{
         3,     -8,     95,     28,      4
},
{
       -10,     15,     77,     60,    -15
},
{
        -1,      4,    124,      2,     -4
},
{
         3,     38,     84,     24,    -25
},
{
         2,     13,     42,     13,     31
},
{
        21,     -4,     56,     46,     -1
},
{
        -1,     35,     79,    -13,     19
},
{
        -7,     65,     88,     -9,    -14
},
{
        20,      4,     81,     49,    -29
},
{
        20,      0,     75,      3,    -17
},
{
         5,     -9,     44,     92,     -8
},
{
         1,     -3,     22,     69,     31
},
{
        -6,     95,     41,    -12,      5
},
{
        39,     67,     16,     -4,      1
},
{
         0,     -6,    120,     55,    -36
},
{
       -13,     44,    122,      4,    -24
},
{
        81,      5,     11,      3,      7
},
{
         2,      0,      9,     10,     88
}
};

			static const int8 *const kSilk_LTP_vq_ptrs_Q7[NB_LTP_CBKS] = {
				&kSilk_LTP_gain_vq_0[0][0], &kSilk_LTP_gain_vq_1[0][0], &kSilk_LTP_gain_vq_2[0][0]};
			static const int8 kSilk_LTP_vq_sizes[NB_LTP_CBKS] = {8, 16, 32};
			static const int16 kSilk_LTPScales_table_Q14[3] = {15565, 12288, 8192};

		} // namespace (anonyme)

		// ── Accès aux tables ────────────────────────────────────────────────────
		const int8 *NkSilkLtpTables::CbLags(int32 Fs_kHz, int32 nb_subfr, int32 *cbk_size) {
			if (Fs_kHz == 8) {
				if (nb_subfr == PE_MAX_NB_SUBFR) {
					*cbk_size = PE_NB_CBKS_STAGE2_EXT;
					return &kSilk_CB_lags_stage2[0][0];
				}
				*cbk_size = PE_NB_CBKS_STAGE2_10MS;
				return &kSilk_CB_lags_stage2_10_ms[0][0];
			}
			if (nb_subfr == PE_MAX_NB_SUBFR) {
				*cbk_size = PE_NB_CBKS_STAGE3_MAX;
				return &kSilk_CB_lags_stage3[0][0];
			}
			*cbk_size = PE_NB_CBKS_STAGE3_10MS;
			return &kSilk_CB_lags_stage3_10_ms[0][0];
		}
		const int8 *NkSilkLtpTables::LtpVq(int32 perIndex) {
			return kSilk_LTP_vq_ptrs_Q7[perIndex];
		}
		int32 NkSilkLtpTables::LtpVqSize(int32 perIndex) {
			return kSilk_LTP_vq_sizes[perIndex];
		}
		int16 NkSilkLtpTables::LtpScale(int32 idx) {
			return kSilk_LTPScales_table_Q14[idx];
		}

	} // namespace media
} // namespace nkentseu
