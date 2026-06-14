/**
 * @File    NkMP3Codec.cpp
 * @Brief   Decodeur MP3 (MPEG-1/2 Layer 3) en style Nkentseu.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 *
 * @Acknowledgments
 *  Structure algorithmique et tables empruntees de minimp3 (CC0 public
 *  domain) par Lieven Moors, Martin J. Fiedler et Lion (github.com/lieff).
 *  https://github.com/lieff/minimp3
 *  Reecriture en style Nkentseu : types int32/float32, allocator memory::NkAlloc,
 *  logger NkLog, namespace nkentseu::audio::mp3, sans dependance externe et
 *  sans inclure de fichier minimp3 dans le projet. Branches SIMD retirees au
 *  profit du chemin scalaire portable (multi-platform).
 *
 * @MultiPlatform
 *  100% C++17 standard. Pas de header OS, pas d'#ifdef plateforme.
 *  Compile identique sur Windows / Linux / macOS / iOS / Android / Web.
 */

#include "NKAudio/Codecs/MP3/NkMP3Codec.h"
#include "NKMemory/NkAllocator.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cmath>

namespace nkentseu {
    namespace audio {
        namespace mp3 {

            // ════════════════════════════════════════════════════════════════
            //  Constantes du format MP3
            // ════════════════════════════════════════════════════════════════

            static constexpr int32 kMaxFreeFormatFrameSize     = 2304;
            static constexpr int32 kMaxFrameSyncMatchesNeeded  = 10;
            static constexpr int32 kMaxL3FramePayloadBytes     = kMaxFreeFormatFrameSize;
            static constexpr int32 kMaxBitReservoirBytes       = 511;
            static constexpr int32 kShortBlockType             = 2;
            static constexpr int32 kStopBlockType              = 3;
            static constexpr int32 kModesMask                  = 0xC0;
            static constexpr int32 kMonoMode                   = 0xC0;
            static constexpr int32 kBitsDequantizerOut         = -1;
            static constexpr int32 kMaxScf                     = (255 + kBitsDequantizerOut * 4 - 210);
            // BUG CRITIQUE precedemment : kMaxScfi etait hardcode a 45.
            // Formule correcte (minimp3) : (MAX_SCF + 3) & ~3 = (41 + 3) & ~3 = 44.
            // L'erreur de 1 sur kMaxScfi decalait les gains par 2^(1/4) ~ 1.19x
            // -> bruit melange a l'audio. Maintenant calcule depuis kMaxScf.
            static constexpr int32 kMaxScfi                    = (kMaxScf + 3) & ~3;

            // ════════════════════════════════════════════════════════════════
            //  Macros d'acces au header (4 octets MPEG)
            // ════════════════════════════════════════════════════════════════

            static NKENTSEU_INLINE int32 HdrIsMono(const uint8* h) noexcept {
                return (h[3] & 0xC0) == 0xC0;
            }
            static NKENTSEU_INLINE int32 HdrIsMs(const uint8* h) noexcept {
                return (h[3] & 0xE0) == 0x60;
            }
            static NKENTSEU_INLINE int32 HdrIsIntensity(const uint8* h) noexcept {
                return (h[3] & 0x10) != 0;
            }
            static NKENTSEU_INLINE int32 HdrTestMpeg1(const uint8* h) noexcept {
                return (h[1] & 0x08) != 0;
            }
            static NKENTSEU_INLINE int32 HdrTestNotMpeg25(const uint8* h) noexcept {
                return (h[1] & 0x10) != 0;
            }
            static NKENTSEU_INLINE int32 HdrTestIStereo(const uint8* h) noexcept {
                return (h[3] & 0x10) != 0;
            }
            static NKENTSEU_INLINE int32 HdrTestMsStereo(const uint8* h) noexcept {
                return (h[3] & 0x20) != 0;
            }
            static NKENTSEU_INLINE int32 HdrGetStereoMode(const uint8* h) noexcept {
                return (h[3] >> 6) & 3;
            }
            static NKENTSEU_INLINE int32 HdrGetStereoModeExt(const uint8* h) noexcept {
                return (h[3] >> 4) & 3;
            }
            static NKENTSEU_INLINE int32 HdrGetLayer(const uint8* h) noexcept {
                return (h[1] >> 1) & 3;
            }
            static NKENTSEU_INLINE int32 HdrGetBitrate(const uint8* h) noexcept {
                return h[2] >> 4;
            }
            static NKENTSEU_INLINE int32 HdrGetSampleRate(const uint8* h) noexcept {
                return (h[2] >> 2) & 3;
            }
            static NKENTSEU_INLINE int32 HdrIsFreeFormat(const uint8* h) noexcept {
                return (h[2] & 0xF0) == 0;
            }
            static NKENTSEU_INLINE int32 HdrIsCrc(const uint8* h) noexcept {
                return (h[1] & 1) == 0;
            }
            // BUG CRITIQUE precedemment : j'avais oublie le `* 3`. Ca selectionnait
            // les MAUVAIS layouts de scalefactor bands (kScfLong[1] au lieu de
            // kScfLong[5] pour MPEG-1 44.1kHz) -> audio decodable mais avec
            // mauvais scalefactors -> bruit melange a l'audio.
            // Formule : pour MPEG-1 ajoute 6, MPEG-2 ajoute 3, MPEG-2.5 ajoute 0.
            static NKENTSEU_INLINE int32 HdrGetMySampleRate(const uint8* h) noexcept {
                return HdrGetSampleRate(h)
                    + (((h[1] >> 3) & 1) + ((h[1] >> 4) & 1)) * 3;
            }

            static bool HdrValid(const uint8* h) noexcept {
                return h[0] == 0xFF
                    && ((h[1] & 0xF0) == 0xF0 || (h[1] & 0xFE) == 0xE2)
                    && (HdrGetLayer(h) != 0)
                    && (HdrGetBitrate(h) != 15)
                    && (HdrGetSampleRate(h) != 3);
            }

            static bool HdrCompare(const uint8* h1, const uint8* h2) noexcept {
                return HdrValid(h2)
                    && ((h1[1] ^ h2[1]) & 0xFE) == 0
                    && ((h1[2] ^ h2[2]) & 0x0C) == 0
                    && (HdrIsFreeFormat(h1) == HdrIsFreeFormat(h2));
            }

            // ════════════════════════════════════════════════════════════════
            //  Tables bitrate / sample rate
            // ════════════════════════════════════════════════════════════════

            static uint32 HdrBitrateKbps(const uint8* h) noexcept {
                static const uint8 halfrate[2][3][15] = {
                    { { 0,4,8,12,16,20,24,28,32,40,48,56,64,72,80 },
                      { 0,4,8,12,16,20,24,28,32,40,48,56,64,72,80 },
                      { 0,16,24,28,32,40,48,56,64,72,80,88,96,112,128 } },
                    { { 0,16,20,24,28,32,40,48,56,64,80,96,112,128,160 },
                      { 0,16,24,28,32,40,48,56,64,80,96,112,128,160,192 },
                      { 0,16,32,48,64,80,96,112,128,144,160,176,192,208,224 } }
                };
                return 2u * uint32(halfrate[HdrTestMpeg1(h) ? 1 : 0][HdrGetLayer(h) - 1][HdrGetBitrate(h)]);
            }

            static uint32 HdrSampleRateHz(const uint8* h) noexcept {
                static const uint32 g_hz[3] = { 44100, 48000, 32000 };
                return g_hz[HdrGetSampleRate(h)] >> int32((h[1] & 0x08) == 0 ? 1 : 0) >> int32((h[1] & 0x10) == 0 ? 1 : 0);
            }

            static uint32 HdrFrameSamples(const uint8* h) noexcept {
                return (HdrGetLayer(h) == 3 /* Layer I */) ? 384u
                       : ((HdrGetLayer(h) == 2 /* layer2 */) ? 1152u
                          : (1152u >> int32(!HdrTestMpeg1(h))));
            }

            static int32 HdrFrameBytes(const uint8* h, int32 freeFormatSize) noexcept {
                int32 frameBytes = int32(HdrFrameSamples(h) * HdrBitrateKbps(h) * 125u / HdrSampleRateHz(h));
                if (HdrGetLayer(h) == 3 /* Layer I */) {
                    frameBytes &= ~3; // slot align
                }
                return frameBytes ? frameBytes : freeFormatSize;
            }

            static int32 HdrPadding(const uint8* h) noexcept {
                return (h[2] & 0x2) ? (HdrGetLayer(h) == 3 /* Layer I */ ? 4 : 1) : 0;
            }

            // ════════════════════════════════════════════════════════════════
            //  Bit stream reader (lecture MSB-first)
            // ════════════════════════════════════════════════════════════════

            struct BitStream {
                const uint8* buf;
                int32 pos;
                int32 limit;

                void Init(const uint8* data, int32 bytes) noexcept {
                    buf = data; pos = 0; limit = bytes * 8;
                }

                uint32 GetBits(int32 n) noexcept {
                    uint32 next, cache = 0;
                    uint32 s = uint32(pos & 7);
                    int32 shl = n + int32(s);
                    const uint8* p = buf + (pos >> 3);
                    if ((pos += n) > limit) return 0;
                    next = uint32(*p++) & (255u >> s);
                    while ((shl -= 8) > 0) {
                        cache |= next << shl;
                        next = uint32(*p++);
                    }
                    return cache | (next >> uint32(-shl));
                }
            };

            // ════════════════════════════════════════════════════════════════
            //  Structure de granule L3 (Layer 3)
            // ════════════════════════════════════════════════════════════════

            struct L3GrInfo {
                const uint8* sfbtab;
                uint16 part_23_length;
                uint16 big_values;
                uint16 scalefac_compress;
                uint8  global_gain;
                uint8  block_type;
                uint8  mixed_block_flag;
                uint8  n_long_sfb;
                uint8  n_short_sfb;
                uint8  table_select[3];
                uint8  region_count[3];
                uint8  subblock_gain[3];
                uint8  preflag;
                uint8  scalefac_scale;
                uint8  count1_table;
                uint8  scfsi;
            };

            // ════════════════════════════════════════════════════════════════
            //  Etat persistant du decodeur (entre frames)
            // ════════════════════════════════════════════════════════════════

            struct Mp3Decoder {
                float32 mdct_overlap[2][9 * 32];
                float32 qmf_state[15 * 2 * 32];
                int32   reserv;
                int32   free_format_bytes;
                uint8   header[4];
                uint8   reserv_buf[511];
            };

            struct Mp3Scratch {
                BitStream bs;
                uint8     maindata[kMaxBitReservoirBytes + kMaxL3FramePayloadBytes];
                L3GrInfo  gr_info[4];
                float32   grbuf[2][576];
                float32   scf[40];
                float32   syn[18 + 15][2 * 32];
                uint8     ist_pos[2][39];
            };

            // ════════════════════════════════════════════════════════════════
            //  Tables Huffman big_values (1842 entrees) - source minimp3 CC0
            // ════════════════════════════════════════════════════════════════

            static const int16 kHuffTabs[] = {
                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                785,785,785,785,784,784,784,784,513,513,513,513,513,513,513,513,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,
                -255,1313,1298,1282,785,785,785,785,784,784,784,784,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,290,288,
                -255,1313,1298,1282,769,769,769,769,529,529,529,529,529,529,529,529,528,528,528,528,528,528,528,528,512,512,512,512,512,512,512,512,290,288,
                -253,-318,-351,-367,785,785,785,785,784,784,784,784,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,819,818,547,547,275,275,275,275,561,560,515,546,289,274,288,258,
                -254,-287,1329,1299,1314,1312,1057,1057,1042,1042,1026,1026,784,784,784,784,529,529,529,529,529,529,529,529,769,769,769,769,768,768,768,768,563,560,306,306,291,259,
                -252,-413,-477,-542,1298,-575,1041,1041,784,784,784,784,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,-383,-399,1107,1092,1106,1061,849,849,789,789,1104,1091,773,773,1076,1075,341,340,325,309,834,804,577,577,532,532,516,516,832,818,803,816,561,561,531,531,515,546,289,289,288,258,
                -252,-429,-493,-559,1057,1057,1042,1042,529,529,529,529,529,529,529,529,784,784,784,784,769,769,769,769,512,512,512,512,512,512,512,512,-382,1077,-415,1106,1061,1104,849,849,789,789,1091,1076,1029,1075,834,834,597,581,340,340,339,324,804,833,532,532,832,772,818,803,817,787,816,771,290,290,290,290,288,258,
                -253,-349,-414,-447,-463,1329,1299,-479,1314,1312,1057,1057,1042,1042,1026,1026,785,785,785,785,784,784,784,784,769,769,769,769,768,768,768,768,-319,851,821,-335,836,850,805,849,341,340,325,336,533,533,579,579,564,564,773,832,578,548,563,516,321,276,306,291,304,259,
                -251,-572,-733,-830,-863,-879,1041,1041,784,784,784,784,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,-511,-527,-543,1396,1351,1381,1366,1395,1335,1380,-559,1334,1138,1138,1063,1063,1350,1392,1031,1031,1062,1062,1364,1363,1120,1120,1333,1348,881,881,881,881,375,374,359,373,343,358,341,325,791,791,1123,1122,-703,1105,1045,-719,865,865,790,790,774,774,1104,1029,338,293,323,308,-799,-815,833,788,772,818,803,816,322,292,307,320,561,531,515,546,289,274,288,258,
                -251,-525,-605,-685,-765,-831,-846,1298,1057,1057,1312,1282,785,785,785,785,784,784,784,784,769,769,769,769,512,512,512,512,512,512,512,512,1399,1398,1383,1367,1382,1396,1351,-511,1381,1366,1139,1139,1079,1079,1124,1124,1364,1349,1363,1333,882,882,882,882,807,807,807,807,1094,1094,1136,1136,373,341,535,535,881,775,867,822,774,-591,324,338,-671,849,550,550,866,864,609,609,293,336,534,534,789,835,773,-751,834,804,308,307,833,788,832,772,562,562,547,547,305,275,560,515,290,290,
                -252,-397,-477,-557,-622,-653,-719,-735,-750,1329,1299,1314,1057,1057,1042,1042,1312,1282,1024,1024,785,785,785,785,784,784,784,784,769,769,769,769,-383,1127,1141,1111,1126,1140,1095,1110,869,869,883,883,1079,1109,882,882,375,374,807,868,838,881,791,-463,867,822,368,263,852,837,836,-543,610,610,550,550,352,336,534,534,865,774,851,821,850,805,593,533,579,564,773,832,578,578,548,548,577,577,307,276,306,291,516,560,259,259,
                -250,-2107,-2507,-2764,-2909,-2974,-3007,-3023,1041,1041,1040,1040,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,-767,-1052,-1213,-1277,-1358,-1405,-1469,-1535,-1550,-1582,-1614,-1647,-1662,-1694,-1726,-1759,-1774,-1807,-1822,-1854,-1886,1565,-1919,-1935,-1951,-1967,1731,1730,1580,1717,-1983,1729,1564,-1999,1548,-2015,-2031,1715,1595,-2047,1714,-2063,1610,-2079,1609,-2095,1323,1323,1457,1457,1307,1307,1712,1547,1641,1700,1699,1594,1685,1625,1442,1442,1322,1322,-780,-973,-910,1279,1278,1277,1262,1276,1261,1275,1215,1260,1229,-959,974,974,989,989,-943,735,478,478,495,463,506,414,-1039,1003,958,1017,927,942,987,957,431,476,1272,1167,1228,-1183,1256,-1199,895,895,941,941,1242,1227,1212,1135,1014,1014,490,489,503,487,910,1013,985,925,863,894,970,955,1012,847,-1343,831,755,755,984,909,428,366,754,559,-1391,752,486,457,924,997,698,698,983,893,740,740,908,877,739,739,667,667,953,938,497,287,271,271,683,606,590,712,726,574,302,302,738,736,481,286,526,725,605,711,636,724,696,651,589,681,666,710,364,467,573,695,466,466,301,465,379,379,709,604,665,679,316,316,634,633,436,436,464,269,424,394,452,332,438,363,347,408,393,448,331,422,362,407,392,421,346,406,391,376,375,359,1441,1306,-2367,1290,-2383,1337,-2399,-2415,1426,1321,-2431,1411,1336,-2447,-2463,-2479,1169,1169,1049,1049,1424,1289,1412,1352,1319,-2495,1154,1154,1064,1064,1153,1153,416,390,360,404,403,389,344,374,373,343,358,372,327,357,342,311,356,326,1395,1394,1137,1137,1047,1047,1365,1392,1287,1379,1334,1364,1349,1378,1318,1363,792,792,792,792,1152,1152,1032,1032,1121,1121,1046,1046,1120,1120,1030,1030,-2895,1106,1061,1104,849,849,789,789,1091,1076,1029,1090,1060,1075,833,833,309,324,532,532,832,772,818,803,561,561,531,560,515,546,289,274,288,258,
                -250,-1179,-1579,-1836,-1996,-2124,-2253,-2333,-2413,-2477,-2542,-2574,-2607,-2622,-2655,1314,1313,1298,1312,1282,785,785,785,785,1040,1040,1025,1025,768,768,768,768,-766,-798,-830,-862,-895,-911,-927,-943,-959,-975,-991,-1007,-1023,-1039,-1055,-1070,1724,1647,-1103,-1119,1631,1767,1662,1738,1708,1723,-1135,1780,1615,1779,1599,1677,1646,1778,1583,-1151,1777,1567,1737,1692,1765,1722,1707,1630,1751,1661,1764,1614,1736,1676,1763,1750,1645,1598,1721,1691,1762,1706,1582,1761,1566,-1167,1749,1629,767,766,751,765,494,494,735,764,719,749,734,763,447,447,748,718,477,506,431,491,446,476,461,505,415,430,475,445,504,399,460,489,414,503,383,474,429,459,502,502,746,752,488,398,501,473,413,472,486,271,480,270,-1439,-1455,1357,-1471,-1487,-1503,1341,1325,-1519,1489,1463,1403,1309,-1535,1372,1448,1418,1476,1356,1462,1387,-1551,1475,1340,1447,1402,1386,-1567,1068,1068,1474,1461,455,380,468,440,395,425,410,454,364,467,466,464,453,269,409,448,268,432,1371,1473,1432,1417,1308,1460,1355,1446,1459,1431,1083,1083,1401,1416,1458,1445,1067,1067,1370,1457,1051,1051,1291,1430,1385,1444,1354,1415,1400,1443,1082,1082,1173,1113,1186,1066,1185,1050,-1967,1158,1128,1172,1097,1171,1081,-1983,1157,1112,416,266,375,400,1170,1142,1127,1065,793,793,1169,1033,1156,1096,1141,1111,1155,1080,1126,1140,898,898,808,808,897,897,792,792,1095,1152,1032,1125,1110,1139,1079,1124,882,807,838,881,853,791,-2319,867,368,263,822,852,837,866,806,865,-2399,851,352,262,534,534,821,836,594,594,549,549,593,593,533,533,848,773,579,579,564,578,548,563,276,276,577,576,306,291,516,560,305,305,275,259,
                -251,-892,-2058,-2620,-2828,-2957,-3023,-3039,1041,1041,1040,1040,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,-511,-527,-543,-559,1530,-575,-591,1528,1527,1407,1526,1391,1023,1023,1023,1023,1525,1375,1268,1268,1103,1103,1087,1087,1039,1039,1523,-604,815,815,815,815,510,495,509,479,508,463,507,447,431,505,415,399,-734,-782,1262,-815,1259,1244,-831,1258,1228,-847,-863,1196,-879,1253,987,987,748,-767,493,493,462,477,414,414,686,669,478,446,461,445,474,429,487,458,412,471,1266,1264,1009,1009,799,799,-1019,-1276,-1452,-1581,-1677,-1757,-1821,-1886,-1933,-1997,1257,1257,1483,1468,1512,1422,1497,1406,1467,1496,1421,1510,1134,1134,1225,1225,1466,1451,1374,1405,1252,1252,1358,1480,1164,1164,1251,1251,1238,1238,1389,1465,-1407,1054,1101,-1423,1207,-1439,830,830,1248,1038,1237,1117,1223,1148,1236,1208,411,426,395,410,379,269,1193,1222,1132,1235,1221,1116,976,976,1192,1162,1177,1220,1131,1191,963,963,-1647,961,780,-1663,558,558,994,993,437,408,393,407,829,978,813,797,947,-1743,721,721,377,392,844,950,828,890,706,706,812,859,796,960,948,843,934,874,571,571,-1919,690,555,689,421,346,539,539,944,779,918,873,932,842,903,888,570,570,931,917,674,674,-2575,1562,-2591,1609,-2607,1654,1322,1322,1441,1441,1696,1546,1683,1593,1669,1624,1426,1426,1321,1321,1639,1680,1425,1425,1305,1305,1545,1668,1608,1623,1667,1592,1638,1666,1320,1320,1652,1607,1409,1409,1304,1304,1288,1288,1664,1637,1395,1395,1335,1335,1622,1636,1394,1394,1319,1319,1606,1621,1392,1392,1137,1137,1137,1137,345,390,360,375,404,373,1047,-2751,-2767,-2783,1062,1121,1046,-2799,1077,-2815,1106,1061,789,789,1105,1104,263,355,310,340,325,354,352,262,339,324,1091,1076,1029,1090,1060,1075,833,833,788,788,1088,1028,818,818,803,803,561,561,531,531,816,771,546,546,289,274,288,258,
                -253,-317,-381,-446,-478,-509,1279,1279,-811,-1179,-1451,-1756,-1900,-2028,-2189,-2253,-2333,-2414,-2445,-2511,-2526,1313,1298,-2559,1041,1041,1040,1040,1025,1025,1024,1024,1022,1007,1021,991,1020,975,1019,959,687,687,1018,1017,671,671,655,655,1016,1015,639,639,758,758,623,623,757,607,756,591,755,575,754,559,543,543,1009,783,-575,-621,-685,-749,496,-590,750,749,734,748,974,989,1003,958,988,973,1002,942,987,957,972,1001,926,986,941,971,956,1000,910,985,925,999,894,970,-1071,-1087,-1102,1390,-1135,1436,1509,1451,1374,-1151,1405,1358,1480,1420,-1167,1507,1494,1389,1342,1465,1435,1450,1326,1505,1310,1493,1373,1479,1404,1492,1464,1419,428,443,472,397,736,526,464,464,486,457,442,471,484,482,1357,1449,1434,1478,1388,1491,1341,1490,1325,1489,1463,1403,1309,1477,1372,1448,1418,1433,1476,1356,1462,1387,-1439,1475,1340,1447,1402,1474,1324,1461,1371,1473,269,448,1432,1417,1308,1460,-1711,1459,-1727,1441,1099,1099,1446,1386,1431,1401,-1743,1289,1083,1083,1160,1160,1458,1445,1067,1067,1370,1457,1307,1430,1129,1129,1098,1098,268,432,267,416,266,400,-1887,1144,1187,1082,1173,1113,1186,1066,1050,1158,1128,1143,1172,1097,1171,1081,420,391,1157,1112,1170,1142,1127,1065,1169,1049,1156,1096,1141,1111,1155,1080,1126,1154,1064,1153,1140,1095,1048,-2159,1125,1110,1137,-2175,823,823,1139,1138,807,807,384,264,368,263,868,838,853,791,867,822,852,837,866,806,865,790,-2319,851,821,836,352,262,850,805,849,-2399,533,533,835,820,336,261,578,548,563,577,532,532,832,772,562,562,547,547,305,275,560,515,290,290,288,258
            };
            static const uint8 kHuffTab32[] = {
                130,162,193,209,44,28,76,140,9,9,9,9,9,9,9,9,190,254,222,238,126,94,157,157,109,61,173,205
            };
            static const uint8 kHuffTab33[] = {
                252,236,220,204,188,172,156,140,124,108,92,76,60,44,28,12
            };
            static const int16 kHuffTabIndex[2 * 16] = {
                0,32,64,98,0,132,180,218,292,364,426,538,648,746,0,1126,
                1460,1460,1460,1460,1460,1460,1460,1460,1842,1842,1842,1842,1842,1842,1842,1842
            };
            static const uint8 kHuffLinbits[] = {
                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,3,4,6,8,10,13,4,5,6,7,8,9,11,13
            };

            // ════════════════════════════════════════════════════════════════
            //  Power-law dequantization : pow43[i] = i^(4/3) signed
            // ════════════════════════════════════════════════════════════════

            static const float32 kPow43[129 + 16] = {
                0.f,-1.f,-2.519842f,-4.326749f,-6.349604f,-8.549880f,-10.902724f,-13.390518f,-16.000000f,-18.720754f,-21.544347f,-24.463781f,-27.473142f,-30.567351f,-33.741992f,-36.993181f,
                0.f,1.f,2.519842f,4.326749f,6.349604f,8.549880f,10.902724f,13.390518f,16.000000f,18.720754f,21.544347f,24.463781f,27.473142f,30.567351f,33.741992f,36.993181f,40.317474f,43.711787f,47.173345f,50.699631f,54.288352f,57.937408f,61.644865f,65.408941f,69.227979f,73.100443f,77.024898f,81.000000f,85.024491f,89.097188f,93.216975f,97.382800f,101.593667f,105.848633f,110.146801f,114.487321f,118.869381f,123.292209f,127.755065f,132.257246f,136.798076f,141.376907f,145.993119f,150.646117f,155.335327f,160.060199f,164.820202f,169.614826f,174.443577f,179.305980f,184.201575f,189.129918f,194.090580f,199.083145f,204.107210f,209.162385f,214.248292f,219.364564f,224.510845f,229.686789f,234.892058f,240.126328f,245.389280f,250.680604f,256.000000f,261.347174f,266.721841f,272.123723f,277.552547f,283.008049f,288.489971f,293.998060f,299.532071f,305.091761f,310.676898f,316.287249f,321.922592f,327.582707f,333.267377f,338.976394f,344.709550f,350.466646f,356.247482f,362.051866f,367.879608f,373.730522f,379.604427f,385.501143f,391.420496f,397.362314f,403.326427f,409.312672f,415.320884f,421.350905f,427.402579f,433.475750f,439.570269f,445.685987f,451.822757f,457.980436f,464.158883f,470.357960f,476.577530f,482.817459f,489.077615f,495.357868f,501.658090f,507.978156f,514.317941f,520.677324f,527.056184f,533.454404f,539.871867f,546.308458f,552.764065f,559.238575f,565.731879f,572.243870f,578.774440f,585.323483f,591.890898f,598.476581f,605.080431f,611.702349f,618.342238f,625.000000f,631.675540f,638.368763f,645.079578f
            };

            static float32 L3PowerLaw(int32 x) noexcept {
                float32 frac;
                int32 sign, mult = 256;
                if (x < 129) {
                    return kPow43[16 + x];
                }
                if (x < 1024) {
                    mult = 16;
                    x <<= 3;
                }
                sign = 2 * x & 64;
                frac = float32((x & 63) - sign) / float32((x & ~63) + sign);
                return kPow43[16 + ((x + sign) >> 6)]
                    * (1.0f + frac * ((4.0f / 3.0f) + frac * (2.0f / 9.0f))) * float32(mult);
            }

            // BUG CRITIQUE precedemment : ma version calculait y * 2^(+e/4)
            // alors que minimp3 calcule y * 2^(-e/4) (signe inverse).
            // Resultat : toute la dequantization etait fausse -> bruits etranges.
            // Fix : usage des constantes g_expfrac de minimp3 (= 2^-30 / 2^((i)/4)).
            // Note : L3_ldexp_q2 est toujours appelle avec exp_q2 >= 0 dans minimp3
            // (cf. usages dans L3_decode_scalefactors et L3_stereo_process).
            static float32 L3LdexpQ2(float32 y, int32 expQ2) noexcept {
                static const float32 kExpFrac[4] = {
                    9.31322575e-10f, 7.83145814e-10f, 6.58544508e-10f, 5.53767716e-10f
                };
                int32 e;
                do {
                    e = expQ2 > 30 * 4 ? 30 * 4 : expQ2;
                    y *= kExpFrac[e & 3] * float32(1u << (30 - (e >> 2)));
                } while ((expQ2 -= e) > 0);
                return y;
            }

            // ════════════════════════════════════════════════════════════════
            //  L3_read_side_info : parse les 17/32 octets de side info MP3
            //  Source : minimp3 (CC0), port direct en style Nkentseu
            // ════════════════════════════════════════════════════════════════

            static const uint8 kScfLong[8][23] = {
                { 6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54,0 },
                { 12,12,12,12,12,12,16,20,24,28,32,40,48,56,64,76,90,2,2,2,2,2,0 },
                { 6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54,0 },
                { 6,6,6,6,6,6,8,10,12,14,16,18,22,26,32,38,46,54,62,70,76,36,0 },
                { 6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54,0 },
                { 4,4,4,4,4,4,6,6,8,8,10,12,16,20,24,28,34,42,50,54,76,158,0 },
                { 4,4,4,4,4,4,6,6,6,8,10,12,16,18,22,28,34,40,46,54,54,192,0 },
                { 4,4,4,4,4,4,6,6,8,10,12,16,20,24,30,38,46,56,68,84,102,26,0 }
            };
            static const uint8 kScfShort[8][40] = {
                { 4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,30,30,30,40,40,40,18,18,18,0 },
                { 8,8,8,8,8,8,8,8,8,12,12,12,16,16,16,20,20,20,24,24,24,28,28,28,36,36,36,2,2,2,2,2,2,2,2,2,26,26,26,0 },
                { 4,4,4,4,4,4,4,4,4,6,6,6,6,6,6,8,8,8,10,10,10,14,14,14,18,18,18,26,26,26,32,32,32,42,42,42,18,18,18,0 },
                { 4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,32,32,32,44,44,44,12,12,12,0 },
                { 4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,30,30,30,40,40,40,18,18,18,0 },
                { 4,4,4,4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,22,22,22,30,30,30,56,56,56,0 },
                { 4,4,4,4,4,4,4,4,4,4,4,4,6,6,6,6,6,6,10,10,10,12,12,12,14,14,14,16,16,16,20,20,20,26,26,26,66,66,66,0 },
                { 4,4,4,4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,12,12,12,16,16,16,20,20,20,26,26,26,34,34,34,42,42,42,12,12,12,0 }
            };
            static const uint8 kScfMixed[8][40] = {
                { 6,6,6,6,6,6,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,30,30,30,40,40,40,18,18,18,0 },
                { 12,12,12,4,4,4,8,8,8,12,12,12,16,16,16,20,20,20,24,24,24,28,28,28,36,36,36,2,2,2,2,2,2,2,2,2,26,26,26,0 },
                { 6,6,6,6,6,6,6,6,6,6,6,6,8,8,8,10,10,10,14,14,14,18,18,18,26,26,26,32,32,32,42,42,42,18,18,18,0 },
                { 6,6,6,6,6,6,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,32,32,32,44,44,44,12,12,12,0 },
                { 6,6,6,6,6,6,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,30,30,30,40,40,40,18,18,18,0 },
                { 4,4,4,4,4,4,6,6,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,22,22,22,30,30,30,56,56,56,0 },
                { 4,4,4,4,4,4,6,6,4,4,4,6,6,6,6,6,6,10,10,10,12,12,12,14,14,14,16,16,16,20,20,20,26,26,26,66,66,66,0 },
                { 4,4,4,4,4,4,6,6,4,4,4,6,6,6,8,8,8,12,12,12,16,16,16,20,20,20,26,26,26,34,34,34,42,42,42,12,12,12,0 }
            };

            static int32 L3ReadSideInfo(BitStream& bs, L3GrInfo* gr, const uint8* hdr) noexcept {
                uint32 tables, scfsi = 0;
                int32 main_data_begin, part_23_sum = 0;
                int32 sr_idx = HdrGetMySampleRate(hdr); sr_idx -= (sr_idx != 0);
                int32 gr_count = HdrIsMono(hdr) ? 1 : 2;

                if (HdrTestMpeg1(hdr)) {
                    gr_count *= 2;
                    main_data_begin = int32(bs.GetBits(9));
                    scfsi = bs.GetBits(7 + gr_count);
                } else {
                    main_data_begin = int32(bs.GetBits(8 + gr_count) >> gr_count);
                }

                do {
                    if (HdrIsMono(hdr)) scfsi <<= 4;
                    gr->part_23_length = uint16(bs.GetBits(12));
                    part_23_sum += gr->part_23_length;
                    gr->big_values = uint16(bs.GetBits(9));
                    if (gr->big_values > 288) return -1;
                    gr->global_gain = uint8(bs.GetBits(8));
                    gr->scalefac_compress = uint16(bs.GetBits(HdrTestMpeg1(hdr) ? 4 : 9));
                    gr->sfbtab = kScfLong[sr_idx];
                    gr->n_long_sfb  = 22;
                    gr->n_short_sfb = 0;
                    if (bs.GetBits(1)) {
                        gr->block_type = uint8(bs.GetBits(2));
                        if (!gr->block_type) return -1;
                        gr->mixed_block_flag = uint8(bs.GetBits(1));
                        gr->region_count[0] = 7;
                        gr->region_count[1] = 255;
                        if (gr->block_type == kShortBlockType) {
                            scfsi &= 0x0F0F;
                            if (!gr->mixed_block_flag) {
                                gr->region_count[0] = 8;
                                gr->sfbtab = kScfShort[sr_idx];
                                gr->n_long_sfb = 0;
                                gr->n_short_sfb = 39;
                            } else {
                                gr->sfbtab = kScfMixed[sr_idx];
                                gr->n_long_sfb = HdrTestMpeg1(hdr) ? 8 : 6;
                                gr->n_short_sfb = 30;
                            }
                        }
                        tables = bs.GetBits(10);
                        tables <<= 5;
                        gr->subblock_gain[0] = uint8(bs.GetBits(3));
                        gr->subblock_gain[1] = uint8(bs.GetBits(3));
                        gr->subblock_gain[2] = uint8(bs.GetBits(3));
                    } else {
                        gr->block_type = 0;
                        gr->mixed_block_flag = 0;
                        tables = bs.GetBits(15);
                        gr->region_count[0] = uint8(bs.GetBits(4));
                        gr->region_count[1] = uint8(bs.GetBits(3));
                        gr->region_count[2] = 255;
                    }
                    gr->table_select[0] = uint8(tables >> 10);
                    gr->table_select[1] = uint8((tables >> 5) & 31);
                    gr->table_select[2] = uint8((tables) & 31);
                    gr->preflag = HdrTestMpeg1(hdr) ? uint8(bs.GetBits(1)) : uint8(gr->scalefac_compress >= 500);
                    gr->scalefac_scale = uint8(bs.GetBits(1));
                    gr->count1_table   = uint8(bs.GetBits(1));
                    gr->scfsi = uint8((scfsi >> 12) & 15);
                    scfsi <<= 4;
                    gr++;
                } while (--gr_count);

                if (part_23_sum + bs.pos > bs.limit + main_data_begin * 8) return -1;
                return main_data_begin;
            }

            // ════════════════════════════════════════════════════════════════
            //  L3_read_scalefactors + L3_decode_scalefactors
            // ════════════════════════════════════════════════════════════════

            static void L3ReadScalefactors(uint8* scf, uint8* ist_pos, const uint8* scf_size,
                                            const uint8* scf_count, BitStream& bs, int32 scfsi) noexcept {
                int32 i, k;
                for (i = 0; i < 4 && scf_count[i]; i++, scfsi *= 2) {
                    int32 cnt = scf_count[i];
                    if (scfsi & 8) {
                        ::memcpy(scf, ist_pos, cnt);
                    } else {
                        int32 bits = scf_size[i];
                        if (!bits) {
                            ::memset(scf, 0, cnt);
                            ::memset(ist_pos, 0, cnt);
                        } else {
                            int32 max_scf = (scfsi < 0) ? (1 << bits) - 1 : -1;
                            for (k = 0; k < cnt; k++) {
                                int32 s = int32(bs.GetBits(bits));
                                ist_pos[k] = uint8(s == max_scf ? -1 : s);
                                scf[k] = uint8(s);
                            }
                        }
                    }
                    ist_pos += cnt;
                    scf += cnt;
                }
                scf[0] = scf[1] = scf[2] = 0;
            }

            static void L3DecodeScalefactors(const uint8* hdr, uint8* ist_pos, BitStream& bs,
                                              const L3GrInfo* gr, float32* scf, int32 ch) noexcept {
                static const uint8 kScfPartitions[3][28] = {
                    { 6,5,5, 5,6,5,5,5,6,5, 7,3,11,10,0,0, 7, 7, 7,0, 6, 6,6,3, 8, 8,5,0 },
                    { 8,9,6,12,6,9,9,9,6,9,12,6,15,18,0,0, 6,15,12,0, 6,12,9,6, 6,18,9,0 },
                    { 9,9,6,12,9,9,9,9,9,9,12,6,18,18,0,0,12,12,12,0,12, 9,9,6,15,12,9,0 }
                };
                const uint8* scf_partition = kScfPartitions[!!gr->n_short_sfb + !gr->n_long_sfb];
                uint8 scf_size[4], iscf[40];
                int32 i, scf_shift = gr->scalefac_scale + 1, gain_exp, scfsi = gr->scfsi;
                float32 gain;

                if (HdrTestMpeg1(hdr)) {
                    static const uint8 kScfcDecode[16] = { 0,1,2,3, 12,5,6,7, 9,10,11,13, 14,15,18,19 };
                    int32 part = kScfcDecode[gr->scalefac_compress];
                    scf_size[1] = scf_size[0] = uint8(part >> 2);
                    scf_size[3] = scf_size[2] = uint8(part & 3);
                } else {
                    static const uint8 kScfMod[6 * 4] = { 5,5,4,4,5,5,4,1,4,3,1,1,5,6,6,1,4,4,4,1,4,3,1,1 };
                    int32 k, modprod, sfc, ist = HdrTestIStereo(hdr) && ch;
                    sfc = gr->scalefac_compress >> ist;
                    for (k = ist * 3 * 4; sfc >= 0; sfc -= modprod, k += 4) {
                        for (modprod = 1, i = 3; i >= 0; i--) {
                            scf_size[i] = uint8(sfc / modprod % kScfMod[k + i]);
                            modprod *= kScfMod[k + i];
                        }
                    }
                    scf_partition += k;
                    scfsi = -16;
                }
                L3ReadScalefactors(iscf, ist_pos, scf_size, scf_partition, bs, scfsi);

                if (gr->n_short_sfb) {
                    int32 sh = 3 - scf_shift;
                    for (i = 0; i < gr->n_short_sfb; i += 3) {
                        iscf[gr->n_long_sfb + i + 0] += uint8(gr->subblock_gain[0] << sh);
                        iscf[gr->n_long_sfb + i + 1] += uint8(gr->subblock_gain[1] << sh);
                        iscf[gr->n_long_sfb + i + 2] += uint8(gr->subblock_gain[2] << sh);
                    }
                } else if (gr->preflag) {
                    static const uint8 kPreamp[10] = { 1,1,1,1,2,2,3,3,3,2 };
                    for (i = 0; i < 10; i++) iscf[11 + i] += kPreamp[i];
                }

                gain_exp = gr->global_gain + kBitsDequantizerOut * 4 - 210 - (HdrIsMs(hdr) ? 2 : 0);
                gain = L3LdexpQ2(float32(1 << (kMaxScfi / 4)), kMaxScfi - gain_exp);
                for (i = 0; i < int32(gr->n_long_sfb + gr->n_short_sfb); i++) {
                    scf[i] = L3LdexpQ2(gain, iscf[i] << scf_shift);
                }
            }

            // ════════════════════════════════════════════════════════════════
            //  L3_huffman : decode des coefficients de frequence (576 par granule)
            // ════════════════════════════════════════════════════════════════

            static void L3Huffman(float32* dst, BitStream& bs, const L3GrInfo* gr_info,
                                   const float32* scf, int32 layer3gr_limit) noexcept {
                float32 one = 0.0f;
                int32 ireg = 0, big_val_cnt = gr_info->big_values;
                const uint8* sfb = gr_info->sfbtab;
                const uint8* bs_next_ptr = bs.buf + bs.pos / 8;
                uint32 bs_cache = (((uint32(bs_next_ptr[0]) * 256u + bs_next_ptr[1]) * 256u
                                  + bs_next_ptr[2]) * 256u + bs_next_ptr[3]) << (bs.pos & 7);
                int32 pairs_to_decode, np, bs_sh = (bs.pos & 7) - 8;
                bs_next_ptr += 4;

#define PEEK_BITS(n)  (bs_cache >> (32 - (n)))
#define FLUSH_BITS(n) { bs_cache <<= (n); bs_sh += (n); }
#define CHECK_BITS    while (bs_sh >= 0) { bs_cache |= uint32(*bs_next_ptr++) << bs_sh; bs_sh -= 8; }
#define BSPOS         ((bs_next_ptr - bs.buf) * 8 - 24 + bs_sh)

                while (big_val_cnt > 0) {
                    int32 tab_num = gr_info->table_select[ireg];
                    int32 sfb_cnt = gr_info->region_count[ireg++];
                    const int16* codebook = kHuffTabs + kHuffTabIndex[tab_num];
                    int32 linbits = kHuffLinbits[tab_num];
                    if (linbits) {
                        do {
                            np = *sfb++ / 2;
                            pairs_to_decode = (big_val_cnt < np) ? big_val_cnt : np;
                            one = *scf++;
                            do {
                                int32 j, w = 5;
                                int32 leaf = codebook[PEEK_BITS(w)];
                                while (leaf < 0) {
                                    FLUSH_BITS(w);
                                    w = leaf & 7;
                                    leaf = codebook[PEEK_BITS(w) - (leaf >> 3)];
                                }
                                FLUSH_BITS(leaf >> 8);
                                for (j = 0; j < 2; j++, dst++, leaf >>= 4) {
                                    int32 lsb = leaf & 0x0F;
                                    if (lsb == 15) {
                                        lsb += PEEK_BITS(linbits);
                                        FLUSH_BITS(linbits);
                                        CHECK_BITS;
                                        *dst = one * L3PowerLaw(lsb)
                                            * ((int32(bs_cache) < 0) ? -1.0f : 1.0f);
                                    } else {
                                        *dst = kPow43[16 + lsb - 16 * int32(bs_cache >> 31)] * one;
                                    }
                                    FLUSH_BITS(lsb ? 1 : 0);
                                }
                                CHECK_BITS;
                            } while (--pairs_to_decode);
                        } while ((big_val_cnt -= np) > 0 && --sfb_cnt >= 0);
                    } else {
                        do {
                            np = *sfb++ / 2;
                            pairs_to_decode = (big_val_cnt < np) ? big_val_cnt : np;
                            one = *scf++;
                            do {
                                int32 j, w = 5;
                                int32 leaf = codebook[PEEK_BITS(w)];
                                while (leaf < 0) {
                                    FLUSH_BITS(w);
                                    w = leaf & 7;
                                    leaf = codebook[PEEK_BITS(w) - (leaf >> 3)];
                                }
                                FLUSH_BITS(leaf >> 8);
                                for (j = 0; j < 2; j++, dst++, leaf >>= 4) {
                                    int32 lsb = leaf & 0x0F;
                                    *dst = kPow43[16 + lsb - 16 * int32(bs_cache >> 31)] * one;
                                    FLUSH_BITS(lsb ? 1 : 0);
                                }
                                CHECK_BITS;
                            } while (--pairs_to_decode);
                        } while ((big_val_cnt -= np) > 0 && --sfb_cnt >= 0);
                    }
                }

                for (np = 1 - big_val_cnt;; dst += 4) {
                    const uint8* codebook_count1 = gr_info->count1_table ? kHuffTab33 : kHuffTab32;
                    int32 leaf = codebook_count1[PEEK_BITS(4)];
                    if (!(leaf & 8)) {
                        leaf = codebook_count1[(leaf >> 3) + (bs_cache << 4 >> (32 - (leaf & 3)))];
                    }
                    FLUSH_BITS(leaf & 7);
                    if (BSPOS > layer3gr_limit) break;
#define RELOAD_SCALEFACTOR  if (!--np) { np = *sfb++/2; if (!np) break; one = *scf++; }
#define DEQ_COUNT1(s) if (leaf & (128 >> (s))) { dst[s] = (int32(bs_cache) < 0) ? -one : one; FLUSH_BITS(1) }
                    RELOAD_SCALEFACTOR;
                    DEQ_COUNT1(0);
                    DEQ_COUNT1(1);
                    RELOAD_SCALEFACTOR;
                    DEQ_COUNT1(2);
                    DEQ_COUNT1(3);
                    CHECK_BITS;
                }
                bs.pos = layer3gr_limit;
#undef PEEK_BITS
#undef FLUSH_BITS
#undef CHECK_BITS
#undef BSPOS
#undef RELOAD_SCALEFACTOR
#undef DEQ_COUNT1
            }

            // ════════════════════════════════════════════════════════════════
            //  L3 stereo : midside + intensity
            // ════════════════════════════════════════════════════════════════

            static void L3MidsideStereo(float32* left, int32 n) noexcept {
                float32* right = left + 576;
                for (int32 i = 0; i < n; i++) {
                    float32 a = left[i], b = right[i];
                    left[i] = a + b;
                    right[i] = a - b;
                }
            }

            static void L3IntensityStereoBand(float32* left, int32 n, float32 kl, float32 kr) noexcept {
                for (int32 i = 0; i < n; i++) {
                    left[i + 576] = left[i] * kr;
                    left[i] = left[i] * kl;
                }
            }

            static void L3StereoTopBand(const float32* right, const uint8* sfb, int32 nbands,
                                         int32 max_band[3]) noexcept {
                max_band[0] = max_band[1] = max_band[2] = -1;
                for (int32 i = 0; i < nbands; i++) {
                    for (int32 k = 0; k < sfb[i]; k += 2) {
                        if (right[k] != 0 || right[k + 1] != 0) { max_band[i % 3] = i; break; }
                    }
                    right += sfb[i];
                }
            }

            static void L3StereoProcess(float32* left, const uint8* ist_pos, const uint8* sfb,
                                         const uint8* hdr, int32 max_band[3], int32 mpeg2_sh) noexcept {
                static const float32 kPan[7 * 2] = {
                    0,1, 0.21132487f,0.78867513f, 0.36602540f,0.63397460f, 0.5f,0.5f,
                    0.63397460f,0.36602540f, 0.78867513f,0.21132487f, 1,0
                };
                uint32 max_pos = HdrTestMpeg1(hdr) ? 7 : 64;
                for (uint32 i = 0; sfb[i]; i++) {
                    uint32 ipos = ist_pos[i];
                    if (int32(i) > max_band[i % 3] && ipos < max_pos) {
                        float32 kl, kr, s = HdrTestMsStereo(hdr) ? 1.41421356f : 1.0f;
                        if (HdrTestMpeg1(hdr)) {
                            kl = kPan[2 * ipos];
                            kr = kPan[2 * ipos + 1];
                        } else {
                            kl = 1.0f;
                            kr = L3LdexpQ2(1.0f, int32((ipos + 1) >> 1) << mpeg2_sh);
                            if (ipos & 1) { kl = kr; kr = 1.0f; }
                        }
                        L3IntensityStereoBand(left, sfb[i], kl * s, kr * s);
                    } else if (HdrTestMsStereo(hdr)) {
                        L3MidsideStereo(left, sfb[i]);
                    }
                    left += sfb[i];
                }
            }

            static void L3IntensityStereo(float32* left, uint8* ist_pos, const L3GrInfo* gr,
                                           const uint8* hdr) noexcept {
                int32 max_band[3], n_sfb = gr->n_long_sfb + gr->n_short_sfb;
                int32 max_blocks = gr->n_short_sfb ? 3 : 1;
                L3StereoTopBand(left + 576, gr->sfbtab, n_sfb, max_band);
                if (gr->n_long_sfb) {
                    int32 m = max_band[0];
                    if (max_band[1] > m) m = max_band[1];
                    if (max_band[2] > m) m = max_band[2];
                    max_band[0] = max_band[1] = max_band[2] = m;
                }
                for (int32 i = 0; i < max_blocks; i++) {
                    int32 default_pos = HdrTestMpeg1(hdr) ? 3 : 0;
                    int32 itop = n_sfb - max_blocks + i;
                    int32 prev = itop - max_blocks;
                    ist_pos[itop] = uint8(max_band[i] >= prev ? default_pos : ist_pos[prev]);
                }
                L3StereoProcess(left, ist_pos, gr->sfbtab, hdr, max_band, gr[1].scalefac_compress & 1);
            }

            // ════════════════════════════════════════════════════════════════
            //  L3_reorder + L3_antialias
            // ════════════════════════════════════════════════════════════════

            static void L3Reorder(float32* grbuf, float32* scratch, const uint8* sfb) noexcept {
                int32 i, len;
                float32* src = grbuf;
                float32* dst = scratch;
                for (; 0 != (len = *sfb); sfb += 3, src += 2 * len) {
                    for (i = 0; i < len; i++, src++) {
                        *dst++ = src[0 * len];
                        *dst++ = src[1 * len];
                        *dst++ = src[2 * len];
                    }
                }
                ::memcpy(grbuf, scratch, usize(dst - scratch) * sizeof(float32));
            }

            static void L3Antialias(float32* grbuf, int32 nbands) noexcept {
                static const float32 kAa[2][8] = {
                    { 0.85749293f,0.88174200f,0.94962865f,0.98331459f,
                      0.99551782f,0.99916056f,0.99989920f,0.99999316f },
                    { 0.51449576f,0.47173197f,0.31337745f,0.18191320f,
                      0.09457419f,0.04096558f,0.01419856f,0.00369997f }
                };
                for (; nbands > 0; nbands--, grbuf += 18) {
                    for (int32 i = 0; i < 8; i++) {
                        float32 u = grbuf[18 + i];
                        float32 d = grbuf[17 - i];
                        grbuf[18 + i] = u * kAa[0][i] - d * kAa[1][i];
                        grbuf[17 - i] = u * kAa[1][i] + d * kAa[0][i];
                    }
                }
            }

            // ════════════════════════════════════════════════════════════════
            //  L3 IMDCT
            // ════════════════════════════════════════════════════════════════

            static void L3Dct3_9(float32* y) noexcept {
                float32 s0, s1, s2, s3, s4, s5, s6, s7, s8, t0, t2, t4;
                s0 = y[0]; s2 = y[2]; s4 = y[4]; s6 = y[6]; s8 = y[8];
                t0 = s0 + s6 * 0.5f;
                s0 -= s6;
                t4 = (s4 + s2) * 0.93969262f;
                t2 = (s8 + s2) * 0.76604444f;
                s6 = (s4 - s8) * 0.17364818f;
                s4 += s8 - s2;
                s2 = s0 - s4 * 0.5f;
                y[4] = s4 + s0;
                s8 = t0 - t2 + s6;
                s0 = t0 - t4 + t2;
                s4 = t0 + t4 - s6;
                s1 = y[1]; s3 = y[3]; s5 = y[5]; s7 = y[7];
                s3 *= 0.86602540f;
                t0 = (s5 + s1) * 0.98480775f;
                t4 = (s5 - s7) * 0.34202014f;
                t2 = (s1 + s7) * 0.64278761f;
                s1 = (s1 - s5 - s7) * 0.86602540f;
                s5 = t0 - s3 - t2;
                s7 = t4 - s3 - t0;
                s3 = t4 + s3 - t2;
                y[0] = s4 - s7;
                y[1] = s2 + s1;
                y[2] = s0 - s3;
                y[3] = s8 + s5;
                y[5] = s8 - s5;
                y[6] = s0 + s3;
                y[7] = s2 - s1;
                y[8] = s4 + s7;
            }

            static void L3Imdct36(float32* grbuf, float32* overlap, const float32* window, int32 nbands) noexcept {
                static const float32 kTwid9[18] = {
                    0.73727734f,0.79335334f,0.84339145f,0.88701083f,0.92387953f,0.95371695f,
                    0.97629601f,0.99144486f,0.99904822f,0.67559021f,0.60876143f,0.53729961f,
                    0.46174861f,0.38268343f,0.30070580f,0.21643961f,0.13052619f,0.04361938f
                };
                for (int32 j = 0; j < nbands; j++, grbuf += 18, overlap += 9) {
                    float32 co[9], si[9];
                    co[0] = -grbuf[0];
                    si[0] = grbuf[17];
                    for (int32 i = 0; i < 4; i++) {
                        si[8 - 2 * i] = grbuf[4 * i + 1] - grbuf[4 * i + 2];
                        co[1 + 2 * i] = grbuf[4 * i + 1] + grbuf[4 * i + 2];
                        si[7 - 2 * i] = grbuf[4 * i + 4] - grbuf[4 * i + 3];
                        co[2 + 2 * i] = -(grbuf[4 * i + 3] + grbuf[4 * i + 4]);
                    }
                    L3Dct3_9(co);
                    L3Dct3_9(si);
                    si[1] = -si[1]; si[3] = -si[3]; si[5] = -si[5]; si[7] = -si[7];
                    for (int32 i = 0; i < 9; i++) {
                        float32 ovl = overlap[i];
                        float32 sum = co[i] * kTwid9[9 + i] + si[i] * kTwid9[0 + i];
                        overlap[i] = co[i] * kTwid9[0 + i] - si[i] * kTwid9[9 + i];
                        grbuf[i]      = ovl * window[0 + i] - sum * window[9 + i];
                        grbuf[17 - i] = ovl * window[9 + i] + sum * window[0 + i];
                    }
                }
            }

            static void L3Idct3(float32 x0, float32 x1, float32 x2, float32* dst) noexcept {
                float32 m1 = x1 * 0.86602540f;
                float32 a1 = x0 - x2 * 0.5f;
                dst[1] = x0 + x2;
                dst[0] = a1 + m1;
                dst[2] = a1 - m1;
            }

            static void L3Imdct12(float32* x, float32* dst, float32* overlap) noexcept {
                static const float32 kTwid3[6] = {
                    0.79335334f,0.92387953f,0.99144486f,
                    0.60876143f,0.38268343f,0.13052619f
                };
                float32 co[3], si[3];
                L3Idct3(-x[0], x[6] + x[3], x[12] + x[9], co);
                L3Idct3(x[15], x[12] - x[9], x[6] - x[3], si);
                si[1] = -si[1];
                for (int32 i = 0; i < 3; i++) {
                    float32 ovl = overlap[i];
                    float32 sum = co[i] * kTwid3[3 + i] + si[i] * kTwid3[0 + i];
                    overlap[i] = co[i] * kTwid3[0 + i] - si[i] * kTwid3[3 + i];
                    dst[i]     = ovl * kTwid3[2 - i] - sum * kTwid3[5 - i];
                    dst[5 - i] = ovl * kTwid3[5 - i] + sum * kTwid3[2 - i];
                }
            }

            static void L3ImdctShort(float32* grbuf, float32* overlap, int32 nbands) noexcept {
                for (; nbands > 0; nbands--, overlap += 9, grbuf += 18) {
                    float32 tmp[18];
                    ::memcpy(tmp, grbuf, sizeof(tmp));
                    ::memcpy(grbuf, overlap, 6 * sizeof(float32));
                    L3Imdct12(tmp,     grbuf + 6,  overlap + 6);
                    L3Imdct12(tmp + 1, grbuf + 12, overlap + 6);
                    L3Imdct12(tmp + 2, overlap,    overlap + 6);
                }
            }

            static void L3ChangeSign(float32* grbuf) noexcept {
                grbuf += 18;
                for (int32 b = 0; b < 32; b += 2, grbuf += 36) {
                    for (int32 i = 1; i < 18; i += 2) grbuf[i] = -grbuf[i];
                }
            }

            static void L3ImdctGr(float32* grbuf, float32* overlap, uint32 block_type, uint32 n_long_bands) noexcept {
                static const float32 kMdctWindow[2][18] = {
                    { 0.99904822f,0.99144486f,0.97629601f,0.95371695f,0.92387953f,
                      0.88701083f,0.84339145f,0.79335334f,0.73727734f,0.04361938f,
                      0.13052619f,0.21643961f,0.30070580f,0.38268343f,0.46174861f,
                      0.53729961f,0.60876143f,0.67559021f },
                    { 1,1,1,1,1,1,0.99144486f,0.92387953f,0.79335334f,
                      0,0,0,0,0,0,0.13052619f,0.38268343f,0.60876143f }
                };
                if (n_long_bands) {
                    L3Imdct36(grbuf, overlap, kMdctWindow[0], n_long_bands);
                    grbuf += 18 * n_long_bands;
                    overlap += 9 * n_long_bands;
                }
                if (block_type == kShortBlockType) {
                    L3ImdctShort(grbuf, overlap, 32 - n_long_bands);
                } else {
                    L3Imdct36(grbuf, overlap, kMdctWindow[block_type == kStopBlockType], 32 - n_long_bands);
                }
            }

            // ════════════════════════════════════════════════════════════════
            //  Bit reservoir : save + restore main_data entre frames
            // ════════════════════════════════════════════════════════════════

            static void L3SaveReservoir(Mp3Decoder* h, Mp3Scratch* s) noexcept {
                int32 pos = (s->bs.pos + 7) / 8;
                int32 remains = s->bs.limit / 8 - pos;
                if (remains > kMaxBitReservoirBytes) {
                    pos += remains - kMaxBitReservoirBytes;
                    remains = kMaxBitReservoirBytes;
                }
                if (remains > 0) {
                    ::memmove(h->reserv_buf, s->maindata + pos, remains);
                }
                h->reserv = remains;
            }

            static int32 L3RestoreReservoir(Mp3Decoder* h, BitStream* bs, Mp3Scratch* s,
                                             int32 main_data_begin) noexcept {
                int32 frame_bytes = (bs->limit - bs->pos) / 8;
                int32 bytes_have = (h->reserv < main_data_begin) ? h->reserv : main_data_begin;
                ::memcpy(s->maindata,
                         h->reserv_buf + (h->reserv > main_data_begin ? h->reserv - main_data_begin : 0),
                         (h->reserv < main_data_begin) ? h->reserv : main_data_begin);
                ::memcpy(s->maindata + bytes_have, bs->buf + bs->pos / 8, frame_bytes);
                s->bs.Init(s->maindata, bytes_have + frame_bytes);
                return h->reserv >= main_data_begin ? 1 : 0;
            }

            // ════════════════════════════════════════════════════════════════
            //  L3_decode : orchestre toutes les etapes pour un set de granules
            // ════════════════════════════════════════════════════════════════

            static void L3Decode(Mp3Decoder* h, Mp3Scratch* s, L3GrInfo* gr_info, int32 nch) noexcept {
                for (int32 ch = 0; ch < nch; ch++) {
                    int32 layer3gr_limit = s->bs.pos + gr_info[ch].part_23_length;
                    L3DecodeScalefactors(h->header, s->ist_pos[ch], s->bs, gr_info + ch, s->scf, ch);
                    L3Huffman(s->grbuf[ch], s->bs, gr_info + ch, s->scf, layer3gr_limit);
                }
                if (HdrTestIStereo(h->header)) {
                    L3IntensityStereo(s->grbuf[0], s->ist_pos[1], gr_info, h->header);
                } else if (HdrIsMs(h->header)) {
                    L3MidsideStereo(s->grbuf[0], 576);
                }
                for (int32 ch = 0; ch < nch; ch++, gr_info++) {
                    int32 aa_bands = 31;
                    int32 n_long_bands = (gr_info->mixed_block_flag ? 2 : 0)
                                       << int32(HdrGetMySampleRate(h->header) == 2);
                    if (gr_info->n_short_sfb) {
                        aa_bands = n_long_bands - 1;
                        L3Reorder(s->grbuf[ch] + n_long_bands * 18, s->syn[0],
                                  gr_info->sfbtab + gr_info->n_long_sfb);
                    }
                    L3Antialias(s->grbuf[ch], aa_bands);
                    L3ImdctGr(s->grbuf[ch], h->mdct_overlap[ch], gr_info->block_type, n_long_bands);
                    L3ChangeSign(s->grbuf[ch]);
                }
            }

            // ════════════════════════════════════════════════════════════════
            //  mp3d_DCT_II : DCT-II 32-point pour la synthesis filter bank
            //  Version scalaire (sans SIMD)
            // ════════════════════════════════════════════════════════════════

            static void Mp3dDCT2(float32* grbuf, int32 n) noexcept {
                static const float32 kSec[24] = {
                    10.19000816f,0.50060302f,0.50241929f,3.40760851f,0.50547093f,0.52249861f,
                    2.05778098f,0.51544732f,0.56694406f,1.48416460f,0.53104258f,0.64682180f,
                    1.16943991f,0.55310392f,0.78815460f,0.97256821f,0.58293498f,1.06067765f,
                    0.83934963f,0.62250412f,1.72244716f,0.74453628f,0.67480832f,5.10114861f
                };
                for (int32 k = 0; k < n; k++) {
                    float32 t[4][8];
                    float32* x;
                    float32* y = grbuf + k;
                    int32 i;
                    for (x = t[0], i = 0; i < 8; i++, x++) {
                        float32 x0 = y[i * 18];
                        float32 x1 = y[(15 - i) * 18];
                        float32 x2 = y[(16 + i) * 18];
                        float32 x3 = y[(31 - i) * 18];
                        float32 t0 = x0 + x3;
                        float32 t1 = x1 + x2;
                        float32 t2 = (x1 - x2) * kSec[3 * i + 0];
                        float32 t3 = (x0 - x3) * kSec[3 * i + 1];
                        x[0]  = t0 + t1;
                        x[8]  = (t0 - t1) * kSec[3 * i + 2];
                        x[16] = t3 + t2;
                        x[24] = (t3 - t2) * kSec[3 * i + 2];
                    }
                    for (x = t[0], i = 0; i < 4; i++, x += 8) {
                        float32 x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3],
                                x4 = x[4], x5 = x[5], x6 = x[6], x7 = x[7], xt;
                        xt = x0 - x7; x0 += x7;
                        x7 = x1 - x6; x1 += x6;
                        x6 = x2 - x5; x2 += x5;
                        x5 = x3 - x4; x3 += x4;
                        x4 = x0 - x3; x0 += x3;
                        x3 = x1 - x2; x1 += x2;
                        x[0] = x0 + x1;
                        x[4] = (x0 - x1) * 0.70710677f;
                        x5 = x5 + x6;
                        x6 = (x6 + x7) * 0.70710677f;
                        x7 = x7 + xt;
                        x3 = (x3 + x4) * 0.70710677f;
                        x5 -= x7 * 0.198912367f;
                        x7 += x5 * 0.382683432f;
                        x5 -= x7 * 0.198912367f;
                        x0 = xt - x6; xt += x6;
                        x[1] = (xt + x7) * 0.50979561f;
                        x[2] = (x4 + x3) * 0.54119611f;
                        x[3] = (x0 - x5) * 0.60134488f;
                        x[5] = (x0 + x5) * 0.89997619f;
                        x[6] = (x4 - x3) * 1.30656302f;
                        x[7] = (xt - x7) * 2.56291556f;
                    }
                    // ATTENTION : version scalaire (avec facteurs 0.5f sur les sous-bandes
                    // 1 et 3). La version SIMD de minimp3 omet ces 0.5f, ce qui produit
                    // un decodage 2x trop fort sur ces sous-bandes (bruits / clipping).
                    for (i = 0; i < 7; i++, y += 4 * 18) {
                        y[0 * 18] = t[0][i];
                        y[1 * 18] = 0.5f * (t[2][i] + t[3][i] + t[3][i + 1]);
                        y[2 * 18] = t[1][i] + t[1][i + 1];
                        y[3 * 18] = 0.5f * (t[3][i] + t[2][i + 1] + t[3][i + 1]);
                    }
                    y[0 * 18] = t[0][7];
                    y[1 * 18] = 0.5f * (t[2][7] + t[3][7]);
                    y[2 * 18] = t[1][7];
                    y[3 * 18] = t[3][7];
                }
            }

            // ════════════════════════════════════════════════════════════════
            //  PCM clip helper
            // ════════════════════════════════════════════════════════════════

            static NKENTSEU_INLINE int16 Mp3dScalePcm(float32 sample) noexcept {
                if (sample >= 32766.5f)  return  32767;
                if (sample <= -32767.5f) return -32768;
                int16 s = int16(sample + 0.5f);
                s -= int16(s < 0);
                return s;
            }

            // ════════════════════════════════════════════════════════════════
            //  mp3d_synth_pair + mp3d_synth : polyphase synthesis QMF
            //  Source : minimp3 (CC0), version scalaire
            // ════════════════════════════════════════════════════════════════

            static void Mp3dSynthPair(int16* pcm, int32 nch, const float32* z) noexcept {
                float32 a;
                a  = (z[14 * 64] - z[ 0 * 64]) * 29;
                a += (z[ 1 * 64] + z[13 * 64]) * 213;
                a += (z[12 * 64] - z[ 2 * 64]) * 459;
                a += (z[ 3 * 64] + z[11 * 64]) * 2037;
                a += (z[10 * 64] - z[ 4 * 64]) * 5153;
                a += (z[ 5 * 64] + z[ 9 * 64]) * 6574;
                a += (z[ 8 * 64] - z[ 6 * 64]) * 37489;
                a +=  z[ 7 * 64]               * 75038;
                pcm[0] = Mp3dScalePcm(a);
                z += 2;
                a  = z[14 * 64] * 104;
                a += z[12 * 64] * 1567;
                a += z[10 * 64] * 9727;
                a += z[ 8 * 64] * 64019;
                a += z[ 6 * 64] * -9975;
                a += z[ 4 * 64] * -45;
                a += z[ 2 * 64] * 146;
                a += z[ 0 * 64] * -5;
                pcm[16 * nch] = Mp3dScalePcm(a);
            }

            static void Mp3dSynth(float32* xl, int16* dstl, int32 nch, float32* lins) noexcept {
                float32* xr = xl + 576 * (nch - 1);
                int16* dstr = dstl + (nch - 1);
                static const float32 kGwin[] = {
                    -1,26,-31,208,218,401,-519,2063,2000,4788,-5517,7134,5959,35640,-39336,74992,
                    -1,24,-35,202,222,347,-581,2080,1952,4425,-5879,7640,5288,33791,-41176,74856,
                    -1,21,-38,196,225,294,-645,2087,1893,4063,-6237,8092,4561,31947,-43006,74630,
                    -1,19,-41,190,227,244,-711,2085,1822,3705,-6589,8492,3776,30112,-44821,74313,
                    -1,17,-45,183,228,197,-779,2075,1739,3351,-6935,8840,2935,28289,-46617,73908,
                    -1,16,-49,176,228,153,-848,2057,1644,3004,-7271,9139,2037,26482,-48390,73415,
                    -2,14,-53,169,227,111,-919,2032,1535,2663,-7597,9389,1082,24694,-50137,72835,
                    -2,13,-58,161,224,72,-991,2001,1414,2330,-7910,9592,70,22929,-51853,72169,
                    -2,11,-63,154,221,36,-1064,1962,1280,2006,-8209,9750,-998,21189,-53534,71420,
                    -2,10,-68,147,215,2,-1137,1919,1131,1692,-8491,9863,-2122,19478,-55178,70590,
                    -3,9,-73,139,208,-29,-1210,1870,970,1388,-8755,9935,-3300,17799,-56778,69679,
                    -3,8,-79,132,200,-57,-1283,1817,794,1095,-8998,9966,-4533,16155,-58333,68692,
                    -4,7,-85,125,189,-83,-1356,1759,605,814,-9219,9959,-5818,14548,-59838,67629,
                    -4,7,-91,117,177,-106,-1428,1698,402,545,-9416,9916,-7154,12980,-61289,66494,
                    -5,6,-97,111,163,-127,-1498,1634,185,288,-9585,9838,-8540,11455,-62684,65290
                };
                float32* zlin = lins + 15 * 64;
                const float32* w = kGwin;

                zlin[4 * 15]     = xl[18 * 16];
                zlin[4 * 15 + 1] = xr[18 * 16];
                zlin[4 * 15 + 2] = xl[0];
                zlin[4 * 15 + 3] = xr[0];

                zlin[4 * 31]     = xl[1 + 18 * 16];
                zlin[4 * 31 + 1] = xr[1 + 18 * 16];
                zlin[4 * 31 + 2] = xl[1];
                zlin[4 * 31 + 3] = xr[1];

                Mp3dSynthPair(dstr,         nch, lins + 4 * 15 + 1);
                Mp3dSynthPair(dstr + 32*nch,nch, lins + 4 * 15 + 64 + 1);
                Mp3dSynthPair(dstl,         nch, lins + 4 * 15);
                Mp3dSynthPair(dstl + 32*nch,nch, lins + 4 * 15 + 64);

                for (int32 i = 14; i >= 0; i--) {
                    float32 a[4], b[4];
                    zlin[4 * i]     = xl[18 * (31 - i)];
                    zlin[4 * i + 1] = xr[18 * (31 - i)];
                    zlin[4 * i + 2] = xl[1 + 18 * (31 - i)];
                    zlin[4 * i + 3] = xr[1 + 18 * (31 - i)];
                    zlin[4 * (i + 16) + 0] = xl[1 + 18 * (1 + i)];
                    zlin[4 * (i + 16) + 1] = xr[1 + 18 * (1 + i)];
                    zlin[4 * (i - 16) + 2] = xl[18 * (1 + i)];
                    zlin[4 * (i - 16) + 3] = xr[18 * (1 + i)];

#define LOAD(k) float32 w0 = *w++; float32 w1 = *w++; float32* vz = &zlin[4*i - (k)*64]; float32* vy = &zlin[4*i - (15-(k))*64];
#define S0(k) { int32 j; LOAD(k); for (j = 0; j < 4; j++) { b[j]  = vz[j]*w1 + vy[j]*w0; a[j]  = vz[j]*w0 - vy[j]*w1; } }
#define S1(k) { int32 j; LOAD(k); for (j = 0; j < 4; j++) { b[j] += vz[j]*w1 + vy[j]*w0; a[j] += vz[j]*w0 - vy[j]*w1; } }
#define S2(k) { int32 j; LOAD(k); for (j = 0; j < 4; j++) { b[j] += vz[j]*w1 + vy[j]*w0; a[j] += vy[j]*w1 - vz[j]*w0; } }
                    S0(0) S2(1) S1(2) S2(3) S1(4) S2(5) S1(6) S2(7)
#undef LOAD
#undef S0
#undef S1
#undef S2
                    dstr[(15 - i) * nch] = Mp3dScalePcm(a[1]);
                    dstr[(17 + i) * nch] = Mp3dScalePcm(b[1]);
                    dstl[(15 - i) * nch] = Mp3dScalePcm(a[0]);
                    dstl[(17 + i) * nch] = Mp3dScalePcm(b[0]);
                    dstr[(47 - i) * nch] = Mp3dScalePcm(a[3]);
                    dstr[(49 + i) * nch] = Mp3dScalePcm(b[3]);
                    dstl[(47 - i) * nch] = Mp3dScalePcm(a[2]);
                    dstl[(49 + i) * nch] = Mp3dScalePcm(b[2]);
                }
            }

            static void Mp3dSynthGranule(float32* qmf_state, float32* grbuf, int32 nbands, int32 nch,
                                          int16* pcm, float32* lins) noexcept {
                for (int32 i = 0; i < nch; i++) {
                    Mp3dDCT2(grbuf + 576 * i, nbands);
                }
                ::memcpy(lins, qmf_state, sizeof(float32) * 15 * 64);
                for (int32 i = 0; i < nbands; i += 2) {
                    Mp3dSynth(grbuf + i, pcm + 32 * nch * i, nch, lins + i * 64);
                }
                if (nch == 1) {
                    for (int32 i = 0; i < 15 * 64; i += 2) lins[i] = lins[i + 1];
                }
                ::memcpy(qmf_state, lins + nbands * 64, sizeof(float32) * 15 * 64);
            }

        } // namespace mp3

        // ════════════════════════════════════════════════════════════════════
        //  Point d'entree NkMP3Codec::Decode
        //
        //  v1.2 : decodeur complet. Skip ID3v2 (debut) + ID3v1 (fin), sync sur
        //  la premiere frame MPEG valide, decode chaque granule Layer 3 via
        //  L3Decode + Mp3dSynthGranule, convertit int16 -> float32 normalise
        //  [-1, 1] dans outBuf. Layer 1 et Layer 2 sont silencieusement
        //  skippes (cf. test HdrGetLayer == 1 dans la boucle plus bas).
        // ════════════════════════════════════════════════════════════════════

        AudioSample NkMP3Codec::Decode(const uint8* data, usize size,
                                       memory::NkAllocator* allocator) noexcept {
            using namespace mp3;
            AudioSample empty{};
            if (!data || size < 4) return empty;

            // Skip ID3v2 / ID3v1
            usize pos = 0;
            if (size >= 10 && data[0] == 'I' && data[1] == 'D' && data[2] == '3') {
                uint32 tagSize = (uint32(data[6]) << 21) | (uint32(data[7]) << 14)
                               | (uint32(data[8]) <<  7) |  uint32(data[9]);
                pos = 10 + usize(tagSize);
                if (data[5] & 0x10) pos += 10;
                if (pos >= size) return empty;
            }
            usize endPos = size;
            if (size >= 128) {
                const uint8* tag = data + size - 128;
                if (tag[0] == 'T' && tag[1] == 'A' && tag[2] == 'G') endPos = size - 128;
            }

            // Find first valid frame
            const uint8* p = data + pos;
            const uint8* end = data + endPos;
            while (p + 4 <= end) {
                if (HdrValid(p)) {
                    int32 fb = HdrFrameBytes(p, 0) + HdrPadding(p);
                    if (fb > 0 && p + fb + 4 <= end && HdrCompare(p, p + fb)) break;
                }
                ++p;
            }
            if (p + 4 > end) { logger.Error("[MP3] Pas de sync valide trouve."); return empty; }

            uint32 sampleRate = HdrSampleRateHz(p);
            int32 channels = HdrIsMono(p) ? 1 : 2;

            // Count frames for output size estimation
            nk_int64 totalFrames = 0;
            const uint8* fp = p;
            while (fp + 4 <= end) {
                if (!HdrValid(fp)) break;
                int32 fb = HdrFrameBytes(fp, 0) + HdrPadding(fp);
                if (fb <= 0 || fp + fb > end) break;
                totalFrames += nk_int64(HdrFrameSamples(fp));
                fp += fb;
            }
            if (totalFrames <= 0) return empty;

            // Allocate output buffer (float32 interleaved)
            usize totalSamples = usize(totalFrames) * usize(channels);
            float32* outBuf = static_cast<float32*>(
                memory::NkAlloc(totalSamples * sizeof(float32), allocator, sizeof(float32)));
            if (!outBuf) { logger.Error("[MP3] Alloc echec ({0} samples).", totalSamples); return empty; }
            ::memset(outBuf, 0, totalSamples * sizeof(float32));

            // Allocate decoder state + scratch (heap pour eviter 32 KB de stack)
            Mp3Decoder* dec = static_cast<Mp3Decoder*>(
                memory::NkAlloc(sizeof(Mp3Decoder), allocator, alignof(Mp3Decoder)));
            Mp3Scratch* scratch = static_cast<Mp3Scratch*>(
                memory::NkAlloc(sizeof(Mp3Scratch), allocator, alignof(Mp3Scratch)));
            if (!dec || !scratch) {
                if (dec) memory::NkFree(dec, allocator);
                if (scratch) memory::NkFree(scratch, allocator);
                memory::NkFree(outBuf, allocator);
                logger.Error("[MP3] Alloc decoder state echec.");
                return empty;
            }
            ::memset(dec, 0, sizeof(Mp3Decoder));
            ::memset(scratch, 0, sizeof(Mp3Scratch));

            // Decode frame par frame
            nk_int64 framePos = 0;
            int16 pcm[1152 * 2]; // max samples per frame (MPEG-1) x 2 ch
            fp = p;
            while (fp + 4 <= end && framePos < totalFrames) {
                if (!HdrValid(fp)) { ++fp; continue; }
                int32 fb = HdrFrameBytes(fp, 0) + HdrPadding(fp);
                if (fb <= 0 || fp + fb > end) break;

                ::memcpy(dec->header, fp, 4);
                BitStream bsFrame;
                bsFrame.Init(fp + 4, fb - 4);
                if (HdrIsCrc(fp)) bsFrame.GetBits(16);

                if (HdrGetLayer(fp) == 1 /* Layer 3 */) {
                    int32 main_data_begin = L3ReadSideInfo(bsFrame, scratch->gr_info, fp);
                    if (main_data_begin < 0 || bsFrame.pos > bsFrame.limit) {
                        // Bad frame, reset state
                        ::memset(dec, 0, sizeof(Mp3Decoder));
                    } else {
                        int32 ok = L3RestoreReservoir(dec, &bsFrame, scratch, main_data_begin);
                        if (ok) {
                            int32 ngran = HdrTestMpeg1(fp) ? 2 : 1;
                            int16* pcmPtr = pcm;
                            for (int32 igr = 0; igr < ngran; igr++, pcmPtr += 576 * channels) {
                                ::memset(scratch->grbuf[0], 0, 576 * 2 * sizeof(float32));
                                L3Decode(dec, scratch, scratch->gr_info + igr * channels, channels);
                                Mp3dSynthGranule(dec->qmf_state, scratch->grbuf[0], 18,
                                                  channels, pcmPtr, scratch->syn[0]);
                            }
                            // Convert int16 PCM to float32 normalized
                            int32 frameSamps = HdrFrameSamples(fp);
                            int32 outSamples = frameSamps * channels;
                            float32* dst = outBuf + usize(framePos) * usize(channels);
                            for (int32 i = 0; i < outSamples; ++i) {
                                dst[i] = float32(pcm[i]) * (1.0f / 32768.0f);
                            }
                            framePos += frameSamps;
                        }
                        L3SaveReservoir(dec, scratch);
                    }
                }
                fp += fb;
            }

            memory::NkFree(scratch, allocator);
            memory::NkFree(dec, allocator);

            logger.Info("[MP3] Decode OK : {0} frames, {1} Hz, {2} canaux.",
                        framePos, sampleRate, channels);

            AudioSample r;
            r.data       = outBuf;
            r.frameCount = usize(framePos);
            r.sampleRate = int32(sampleRate);
            r.channels   = channels;
            r.format     = AudioFormat::MP3;
            r.mAllocator = allocator;
            return r;
        }

    } // namespace audio
} // namespace nkentseu
