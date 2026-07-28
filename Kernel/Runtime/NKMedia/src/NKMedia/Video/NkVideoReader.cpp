// =============================================================================
// NKMedia/Video/NkVideoReader.cpp — implémentation (AVI/MJPEG d'abord).
// =============================================================================
#include "NKMedia/Video/NkVideoReader.h"
#include "NKMedia/Video/NkVideoWriter.h" // SelfTest

#include "NKImage/Core/NkImage.h"
#include "NKImage/Codecs/JPEG/NkJPEGCodec.h"
#include "NKMedia/Codecs/Video/H264/NkH264Decoder.h"
#include "NKMedia/Codecs/Video/VP8/NkVp8Decoder.h"
#include "NKMedia/Codecs/Video/VP9/NkVp9Decoder.h"
#include "NKMedia/Codecs/Video/HEVC/NkHevcDecoder.h"
#include "NKMedia/Codecs/Video/Mpeg2/NkMpeg2Decoder.h"
#include "NKMedia/Codecs/Video/Theora/NkTheoraDecoder.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKMemory/NKMemory.h"

#include <cstring>
#include <new>

namespace nkentseu {
	namespace media {

		namespace {

			inline uint32 RdU32LE(const uint8 *p) {
				return (uint32)p[0] | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24);
			}
			inline int32 RdI32LE(const uint8 *p) {
				return (int32)RdU32LE(p);
			}
			inline bool Tag(const uint8 *p, char a, char b, char c, char d) {
				return p[0] == (uint8)a && p[1] == (uint8)b && p[2] == (uint8)c && p[3] == (uint8)d;
			}
			// ISOBMFF (MP4/MOV) = big-endian.
			inline uint32 RdU32BE(const uint8 *p) {
				return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | ((uint32)p[2] << 8) | (uint32)p[3];
			}
			inline uint16 RdU16BE(const uint8 *p) {
				return (uint16)(((uint16)p[0] << 8) | (uint16)p[1]);
			}
			inline uint64 RdU64BE(const uint8 *p) {
				return ((uint64)RdU32BE(p) << 32) | (uint64)RdU32BE(p + 4);
			}

			enum class Backend { NONE, AVI, MOV, WEBM, TS, FLV, IVF, HEVC_ANNEXB, MPEG2_ES, OGV, SEQUENCE };
			enum class Codec { NONE, MJPEG, RAWRGB, H264, VP8, VP9, HEVC, MPEG2, THEORA };

			// Table 6-4 ISO/IEC 13818-2 : frame_rate_code (sequence header) -> cadence.
			double Mpeg2FrameRateFromCode(int32 code) {
				switch (code) {
					case 1:
						return 24000.0 / 1001.0;
					case 2:
						return 24.0;
					case 3:
						return 25.0;
					case 4:
						return 30000.0 / 1001.0;
					case 5:
						return 30.0;
					case 6:
						return 50.0;
					case 7:
						return 60000.0 / 1001.0;
					case 8:
						return 60.0;
				}
				return 0.0;
			}

			// ── Ogg — parse minimal d'une page (pour compter les paquets Theora + lire le
			// fps de l'en-tête d'identification ; le DÉCODAGE passe par NkTheoraDecoder qui
			// a son propre parseur Ogg complet). En-tête fixe 27 octets + table de lacing.
			struct OggPageView {
					uint32 serial = 0;
					uint8 headerType = 0;
					int32 nsegs = 0;
					const uint8 *segTable = nullptr;
					usize payloadOffset = 0;
					usize next = 0;
			};
			bool OggReadPage(const uint8 *d, usize n, usize pos, OggPageView &pg) {
				if (pos + 27 > n || d[pos] != 'O' || d[pos + 1] != 'g' || d[pos + 2] != 'g' ||
					d[pos + 3] != 'S')
					return false;
				pg.headerType = d[pos + 5];
				pg.serial = RdU32LE(d + pos + 14);
				pg.nsegs = (int32)d[pos + 26];
				if (pos + 27 + (usize)pg.nsegs > n)
					return false;
				pg.segTable = d + pos + 27;
				usize payload = 0;
				for (int32 i = 0; i < pg.nsegs; ++i)
					payload += pg.segTable[i];
				pg.payloadOffset = pos + 27 + (usize)pg.nsegs;
				if (pg.payloadOffset + payload > n)
					return false;
				pg.next = pg.payloadOffset + payload;
				return true;
			}

			// Détecte un flux élémentaire HEVC Annex-B brut (.265/.hevc) : start code
			// (00 00 01 / 00 00 00 01) suivi d'un en-tête NAL HEVC (2 octets) — on exige la
			// présence d'un VPS (type 32) ou d'un SPS (type 33) en couche de base pour
			// distinguer d'un ES H.264 (dont les octets d'en-tête NAL, 1 seul, ne donnent
			// jamais ces types HEVC), et de tout autre format. nal_unit_type = (byte0>>1)&0x3F,
			// nuh_layer_id = ((byte0&1)<<5)|(byte1>>3) — on n'accepte que la couche 0.
			bool LooksLikeHevcAnnexB(const uint8 *d, usize n) {
				usize p = 0;
				int32 checked = 0;
				while (p + 5 < n && checked < 256) {
					const bool sc3 = (d[p] == 0 && d[p + 1] == 0 && d[p + 2] == 1);
					const bool sc4 = (d[p] == 0 && d[p + 1] == 0 && d[p + 2] == 0 && d[p + 3] == 1);
					if (!sc3 && !sc4) {
						++p;
						continue;
					}
					const usize ns = sc4 ? p + 4 : p + 3;
					if (ns + 1 >= n)
						break;
					const uint8 b0 = d[ns], b1 = d[ns + 1];
					const int32 forbidden = b0 >> 7;
					const int32 type = (b0 >> 1) & 0x3F;
					const int32 layer = ((b0 & 1) << 5) | (b1 >> 3);
					if (forbidden == 0 && layer == 0 && (type == 32 || type == 33))
						return true;
					p = ns;
					++checked;
				}
				return false;
			}

			// Référence d'une image encodée dans le buffer fichier (offset+taille).
			struct FrameRef {
					usize offset = 0;
					usize size = 0;
			};

			// ── EBML (Matroska/WebM) — primitives et parseurs de PISTE VIDÉO ──────────────
			// Dupliquées ici (pas partagées avec NkMediaProbe/NkMediaDemux qui ont les leurs) par
			// cohérence avec le reste du fichier : NkVideoReader ne dépend d'aucun autre parseur.
			int32 EbmlReadVint(const uint8 *p, usize avail, uint64 &value, bool keepMarker) {
				if (avail == 0)
					return 0;
				uint8 b = p[0];
				int32 len = 0;
				uint8 mask = 0x80;
				for (int32 i = 0; i < 8; ++i) {
					if (b & mask) {
						len = i + 1;
						break;
					}
					mask >>= 1;
				}
				if (len == 0 || (usize)len > avail)
					return 0;
				uint64 v = keepMarker ? b : (uint64)(b & (0xFF >> len));
				for (int32 i = 1; i < len; ++i)
					v = (v << 8) | p[i];
				value = v;
				return len;
			}
			uint64 EbmlReadUint(const uint8 *p, usize len) {
				uint64 v = 0;
				for (usize i = 0; i < len && i < 8; ++i)
					v = (v << 8) | p[i];
				return v;
			}

			struct WebmVideoTrackInfo {
					int64 num = -1;
					int32 type = 0;
					NkString codecId;
					NkVector<nk_uint8> codecPriv;
					int32 width = 0, height = 0;
			};

			// Cherche la 1re piste VIDÉO (TrackType==1) dans Segment/Tracks/TrackEntry : numéro,
			// CodecID, CodecPrivate (= AVCDecoderConfigurationRecord pour V_MPEG4/ISO/AVC, MÊMES
			// octets que la boîte avcC ISOBMFF, sans le wrapper de boîte), dimensions.
			void WalkWebmTracks(const uint8 *base, usize start, usize end, WebmVideoTrackInfo *found,
								 WebmVideoTrackInfo *cur, int32 depth) {
				if (depth > 10 || (found && found->num >= 0))
					return;
				usize pos = start;
				while (pos < end) {
					uint64 id = 0, sz = 0;
					int32 il = EbmlReadVint(base + pos, end - pos, id, true);
					if (il <= 0)
						break;
					int32 sl = EbmlReadVint(base + pos + il, end - pos - il, sz, false);
					if (sl <= 0)
						break;
					const usize ds = pos + il + sl;
					const uint64 allOnes = (1ULL << (7 * sl)) - 1;
					const usize de = (sz == allOnes || ds + (usize)sz > end) ? end : ds + (usize)sz;
					if (id == 0x18538067ULL || id == 0x1654AE6BULL) { // Segment, Tracks
						WalkWebmTracks(base, ds, de, found, nullptr, depth + 1);
					} else if (id == 0xAEULL) { // TrackEntry
						WebmVideoTrackInfo t;
						WalkWebmTracks(base, ds, de, found, &t, depth + 1);
						if (t.type == 1 && t.num >= 0 && found && found->num < 0)
							*found = t;
					} else if (id == 0xE0ULL || id == 0xE1ULL) { // Video / Audio (sous TrackEntry)
						WalkWebmTracks(base, ds, de, found, cur, depth + 1);
					} else if (cur != nullptr) {
						if (id == 0x83ULL) { // TrackType
							cur->type = (int32)EbmlReadUint(base + ds, de - ds);
						} else if (id == 0xD7ULL) { // TrackNumber
							cur->num = (int64)EbmlReadUint(base + ds, de - ds);
						} else if (id == 0x86ULL) { // CodecID
							usize l = de - ds;
							if (l > 63)
								l = 63;
							char buf[64];
							for (usize i = 0; i < l; ++i)
								buf[i] = (char)base[ds + i];
							buf[l] = 0;
							cur->codecId = NkString(buf);
						} else if (id == 0x63A2ULL) { // CodecPrivate
							for (usize i = ds; i < de; ++i)
								cur->codecPriv.PushBack(base[i]);
						} else if (id == 0xB0ULL) { // PixelWidth
							cur->width = (int32)EbmlReadUint(base + ds, de - ds);
						} else if (id == 0xBAULL) { // PixelHeight
							cur->height = (int32)EbmlReadUint(base + ds, de - ds);
						}
					}
					pos = de;
				}
			}

			// Parcourt les Clusters et sort les SimpleBlock/Block de la piste vidéo (offset+taille
			// dans `outFrames`, horodatage réel en ms dans `outTs`, requis pour dériver `info.fps`
			// puisqu'EBML ne fournit pas d'équivalent direct à `stts` de l'ISOBMFF). Miroir de
			// `NkMediaDemux::WalkClusters` (audio) — pas de lacing géré (quasi jamais utilisé pour
			// la vidéo, c'est une optimisation de petits paquets audio).
			void WalkWebmVideoClusters(const uint8 *base, usize start, usize end, int64 videoNum,
									   uint64 clusterTs, NkVector<FrameRef> &outFrames,
									   NkVector<int64> &outTs, int32 depth) {
				if (depth > 10)
					return;
				usize pos = start;
				uint64 curClusterTs = clusterTs;
				while (pos < end) {
					uint64 id = 0, sz = 0;
					int32 il = EbmlReadVint(base + pos, end - pos, id, true);
					if (il <= 0)
						break;
					int32 sl = EbmlReadVint(base + pos + il, end - pos - il, sz, false);
					if (sl <= 0)
						break;
					const usize ds = pos + il + sl;
					const uint64 allOnes = (1ULL << (7 * sl)) - 1;
					const usize de = (sz == allOnes || ds + (usize)sz > end) ? end : ds + (usize)sz;
					if (id == 0x18538067ULL) { // Segment
						WalkWebmVideoClusters(base, ds, de, videoNum, curClusterTs, outFrames, outTs, depth + 1);
					} else if (id == 0x1F43B675ULL) { // Cluster
						WalkWebmVideoClusters(base, ds, de, videoNum, 0, outFrames, outTs, depth + 1);
					} else if (id == 0xE7ULL) { // Timestamp (du cluster)
						curClusterTs = EbmlReadUint(base + ds, de - ds);
					} else if (id == 0xA0ULL) { // BlockGroup
						WalkWebmVideoClusters(base, ds, de, videoNum, curClusterTs, outFrames, outTs, depth + 1);
					} else if (id == 0xA3ULL || id == 0xA1ULL) { // SimpleBlock / Block
						uint64 trackNum = 0;
						int32 tl = EbmlReadVint(base + ds, de - ds, trackNum, false);
						if (tl > 0 && ds + (usize)tl + 3 <= de && (int64)trackNum == videoNum) {
							const usize hp = ds + (usize)tl;
							const int16 rel = (int16)(((uint16)base[hp] << 8) | (uint16)base[hp + 1]);
							const usize frameStart = hp + 3;
							if (frameStart <= de && de > frameStart) {
								FrameRef fr;
								fr.offset = frameStart;
								fr.size = de - frameStart;
								outFrames.PushBack(fr);
								outTs.PushBack((int64)curClusterTs + (int64)rel);
							}
						}
					}
					pos = de;
				}
			}

			// ── MPEG Transport Stream (TS/M2TS) — découverte des PID ──────────────────────
			// Paquets fixes 188 octets (sync byte 0x47). M2TS/BDAV ajoute un préfixe de 4 octets
			// avant chaque paquet (192 octets de foulée) — détecté en cherchant le plus petit
			// stride pour lequel le sync byte retombe pile sur plusieurs paquets consécutifs.
			int32 DetectTsPacketSize(const uint8 *d, usize n) {
				const int32 strides[2] = {188, 192};
				for (int32 si = 0; si < 2; ++si) {
					const int32 stride = strides[si];
					const int32 syncOff = (stride == 192) ? 4 : 0;
					if ((usize)(syncOff + 188) > n)
						continue;
					const usize maxPkts = n / (usize)stride;
					const int32 need = maxPkts < 8 ? (int32)maxPkts : 8;
					if (need < 1)
						continue;
					bool good = true;
					for (int32 i = 0; i < need; ++i) {
						const usize p = (usize)i * (usize)stride + (usize)syncOff;
						if (p >= n || d[p] != 0x47) {
							good = false;
							break;
						}
					}
					if (good)
						return stride;
				}
				return 0;
			}

			// Cherche le PID du PMT en parsant le PAT (PID 0, table_id 0x00). Ne gère qu'un PAT
			// tenant dans UN SEUL paquet TS (cas quasi universel en pratique).
			int32 FindPmtPid(const uint8 *d, usize n, int32 pktSize, int32 syncOff) {
				for (usize p = (usize)syncOff; p + 188 <= n; p += (usize)pktSize) {
					if (d[p] != 0x47)
						continue;
					const int32 pid = (((int32)(d[p + 1] & 0x1F)) << 8) | d[p + 2];
					if (pid != 0)
						continue;
					if ((d[p + 1] & 0x40) == 0) // payload_unit_start_indicator
						continue;
					const int32 afc = (d[p + 3] >> 4) & 0x3;
					usize payload = p + 4;
					if (afc == 2)
						continue; // adaptation seule, pas de payload
					if (afc == 3) {
						const int32 al = d[payload];
						payload += 1 + (usize)al;
					}
					if (payload >= p + 188)
						continue;
					const usize sec = payload + 1 + (usize)d[payload]; // + pointer_field
					if (sec + 8 > p + 188 || d[sec] != 0x00)			// table_id PAT
						continue;
					const int32 sectionLen = (((int32)(d[sec + 1] & 0x0F)) << 8) | d[sec + 2];
					const usize secEnd = sec + 3 + (usize)sectionLen;
					usize q = sec + 8; // après table_id..last_section_number
					while (secEnd >= 4 && q + 4 <= secEnd - 4 && q + 4 <= p + 188) {
						const int32 progNum = ((int32)d[q] << 8) | d[q + 1];
						const int32 progPid = (((int32)(d[q + 2] & 0x1F)) << 8) | d[q + 3];
						if (progNum != 0)
							return progPid; // 1er programme (pas le NIT, program_number 0)
						q += 4;
					}
					break; // PAT trouvé mais aucun programme -> échec
				}
				return -1;
			}

			// Cherche le PID vidéo en parsant le PMT du programme trouvé ci-dessus :
			// stream_type 0x1B (H.264/AVC) ou 0x02 (MPEG-2 Video) — le type trouvé est rendu
			// via `outStreamType` pour router vers le bon décodeur. Autres stream_types
			// (HEVC 0x24…) ignorés (pas branchés en TS).
			int32 FindVideoPid(const uint8 *d, usize n, int32 pktSize, int32 syncOff, int32 pmtPid,
							   int32 *outStreamType) {
				for (usize p = (usize)syncOff; p + 188 <= n; p += (usize)pktSize) {
					if (d[p] != 0x47)
						continue;
					const int32 pid = (((int32)(d[p + 1] & 0x1F)) << 8) | d[p + 2];
					if (pid != pmtPid)
						continue;
					if ((d[p + 1] & 0x40) == 0)
						continue;
					const int32 afc = (d[p + 3] >> 4) & 0x3;
					usize payload = p + 4;
					if (afc == 2)
						continue;
					if (afc == 3) {
						const int32 al = d[payload];
						payload += 1 + (usize)al;
					}
					if (payload >= p + 188)
						continue;
					const usize sec = payload + 1 + (usize)d[payload];
					if (sec + 12 > p + 188 || d[sec] != 0x02) // table_id PMT
						continue;
					const int32 sectionLen = (((int32)(d[sec + 1] & 0x0F)) << 8) | d[sec + 2];
					const usize secEnd = sec + 3 + (usize)sectionLen;
					const int32 programInfoLen = (((int32)(d[sec + 10] & 0x0F)) << 8) | d[sec + 11];
					usize q = sec + 12 + (usize)programInfoLen;
					while (secEnd >= 4 && q + 5 <= secEnd - 4 && q + 5 <= p + 188) {
						const int32 streamType = d[q];
						const int32 elemPid = (((int32)(d[q + 1] & 0x1F)) << 8) | d[q + 2];
						const int32 esInfoLen = (((int32)(d[q + 3] & 0x0F)) << 8) | d[q + 4];
						if (streamType == 0x1B || streamType == 0x02) { // H.264/AVC ou MPEG-2 Video
							if (outStreamType)
								*outStreamType = streamType;
							return elemPid;
						}
						q += 5 + (usize)esInfoLen;
					}
					break;
				}
				return -1;
			}

		} // namespace

		struct NkVideoReader::Impl {
				Backend backend = Backend::NONE;
				Codec codec = Codec::NONE;
				NkVector<nk_uint8> bytes;	// fichier complet (AVI/MOV)
				NkVector<FrameRef> frames;	// table des images (offset dans `bytes`)
				NkVector<NkString> seqPaths; // séquence : chemins des images triés
				NkVideoReaderInfo info;
				int32 cursor = 0;
				int32 bitCount = 24;			 // RAWRGB
				NkVector<nk_uint8> h264Sps, h264Pps; // H264 : SPS/PPS extraits de l'avcC
				int32 nalLenSize = 4;			 // taille du préfixe de longueur AVCC
				NkVector<bool> h264Keyframe;	 // parallèle à `frames` : image clé (IDR) en ordre de décodage
				NkVector<NkH264Frame> h264Dpb;	 // RefPicList0 : [0] = frame la plus récente (multi-réf)
				int32 h264PrevIndex = -2;		 // index de la dernière frame décodée (séquentialité)
				// Réordonnancement POC (les B se décodent dans le désordre → réaffichage par POC).
				// Clé de tri = POC GLOBAL monotone : gopBase (grand pas par IDR) + poc. Ainsi le POC
				// min du buffer est TOUJOURS la prochaine image à afficher, sans flush explicite.
				int32 lastPoc = 0;				  // POC de la dernière frame décodée (rempli par Decode)
				bool lastIsIdr = false;			  // la dernière frame décodée est un IDR (rempli par Decode)
				int32 h264DecodeCursor = 0;		  // prochain sample H264 à décoder (ordre bitstream)
				int64 h264GopBase = 0;			  // décalage de POC par GOP (incrémenté à chaque IDR)
				int32 h264ReorderMax = 4;		  // taille du buffer de réordonnancement (num B max)
				int32 h264OutCount = 0;			  // nombre d'images sorties (ordre d'affichage)
				NkVector<NkVideoFrame> h264Reorder;  // frames RGBA décodées, en attente d'affichage
				NkVector<nk_int64> h264ReorderKey;   // POC global parallèle

				// ── VP8 ─────────────────────────────────────────────────────────────
				// Pas de B-frames en VP8 : ordre de décodage == ordre d'affichage, le
				// curseur générique suffit. MAIS certains blocs sont des images altref
				// INVISIBLES (`show_frame == 0`) : décodées (elles mettent à jour les
				// références) mais jamais affichées. `vp8DisplayBlocks[i]` = indice du bloc
				// qui produit la i-ème image AFFICHÉE ; Decode(i) décode tous les blocs
				// depuis le précédent affiché (altref intermédiaires comprises).
				NkVp8DecoderState vp8State;
				NkVector<nk_int32> vp8DisplayBlocks;
				NkVector<bool> vp8Keyframe; // parallèle à vp8DisplayBlocks (image clé ?)
				int32 vp8PrevIndex = -2;	 // dernier index AFFICHÉ décodé (séquentialité)

				// ── VP9 ─────────────────────────────────────────────────────────────
				// Contrairement à VP8/H264, un bloc conteneur (bloc EBML SimpleBlock ou
				// trame IVF) peut contenir une SUPERFRAME VP9 (jusqu'à 8 sous-trames
				// concaténées, cf. Annexe B) : `frames[]` reste la table des blocs bruts,
				// mais l'unité de décodage VP9 est la SOUS-TRAME — `vp9Units[]` (une
				// entrée par sous-trame, en ordre de décodage) et `vp9DisplayUnits[]`
				// (indices dans vp9Units des sous-trames AFFICHÉES — show_frame OU
				// show_existing_frame —, en ordre d'affichage) sont donc distincts de
				// `frames`/`vp8DisplayBlocks`. Les champs d'en-tête utiles au décodage
				// (refFrameIdx/refreshFrameFlags/dims/errorResilient) sont mis en cache
				// à l'analyse (Vp9ScanUnits) pour ne jamais re-parser l'en-tête pendant
				// Decode(). Pas de réordonnancement d'affichage (comme VP8, contrairement
				// à H264) : ordre d'affichage = sous-suite monotone de l'ordre de décodage.
				struct Vp9Unit {
						int32 blockIdx = -1;			// index dans `frames`
						usize subOffset = 0, subSize = 0; // sous-région DANS ce bloc
						bool showFrame = false;
						bool showExistingFrame = false;
						int32 frameToShowMapIdx = 0;
						bool isKeyOrIntraOnly = false;
						bool errorResilient = false;
						int32 refFrameIdx[3] = {0, 0, 0};
						uint32 refreshFrameFlags = 0;
						int32 width = 0, height = 0;
				};
				NkVector<Vp9Unit> vp9Units;
				NkVector<nk_int32> vp9DisplayUnits;
				NkVp9EntropyState vp9Entropy;			// état persistant (frame_contexts + segmentation)
				NkVp9Image vp9RefSlots[8];				// DPB VP9 : 8 slots explicites (refresh_frame_flags)
				bool vp9SlotValid[8] = {false, false, false, false, false, false, false, false};
				int32 vp9PrevIndex = -2;				// dernier index AFFICHÉ décodé (séquentialité)
				bool vp9PrevEligibleBase = false;		// use_prev_frame_mvs : trame précédente non clé/intra
				int32 vp9PrevWidth = 0, vp9PrevHeight = 0;
				NkVector<NkVp9MvRef> vp9MvGrid;		// grille MV de la dernière trame (use_prev_frame_mvs)

				// ── HEVC/H.265 ───────────────────────────────────────────────────────
				// Le décodeur HEVC ne stocke AUCUN DPB (comme aucun de nos décodeurs) :
				// c'est le reader qui résout POC→pointeur d'image avant chaque appel. Les B
				// (bi-prédiction) imposent un réordonnancement décodage→affichage EXACT, ici
				// piloté par une clé d'affichage GLOBALE monotone précalculée par image
				// (`hevcGlobalKey[]` = gopBase par IDR + POC) : le POC min restant à décoder
				// borne quand on peut sortir la plus petite clé du buffer (pas d'heuristique
				// « profondeur B », cf. `hevcSuffixMinKey[]`). SPS/PPS/POC/type de slice sont
				// tous PRÉCALCULÉS à l'ouverture (HevcBuildPocTables) — Decode() n'a plus
				// qu'à résoudre les références dans `hevcDpb` et déquantifier/reconstruire.
				NkHevcSps hevcSps;
				NkHevcPps hevcPps;
				bool hevcHaveSps = false, hevcHavePps = false;
				int32 hevcNalLenSize = 4;			  // longueur préfixe NALU (MP4/MKV) ; 0 = Annex-B
				NkVector<nk_int32> hevcPoc;			  // POC intra-GOP par image (ordre décodage) — clé DPB
				NkVector<nk_int64> hevcGlobalKey;	  // clé d'affichage globale monotone (tri réordo)
				NkVector<nk_int64> hevcSuffixMinKey;  // min(hevcGlobalKey[i..fin]) — borne de sortie exacte
				NkVector<bool> hevcIsIdr;			  // image IDR (vide le DPB, POC repart à 0)
				NkVector<bool> hevcKeyframe;		  // IRAP (IDR/BLA/CRA) — point de resynchro SeekFrame
				NkVector<NkHevcFrame> hevcDpb;		  // DPB : images de référence décodées (résolues par POC)
				int32 hevcDecodeCursor = 0;			  // prochaine image à décoder (ordre bitstream)
				int32 hevcOutCount = 0;				  // images sorties (ordre d'affichage)
				NkVector<NkVideoFrame> hevcReorder;	  // RGBA décodées en attente d'affichage
				NkVector<nk_int64> hevcReorderKey;	  // clé d'affichage globale parallèle

				// ── MPEG-2 (ES .m2v ou piste vidéo TS stream_type 0x02) ─────────────
				// NkMpeg2Decoder::DecodeAll consomme le flux élémentaire ENTIER et rend
				// les images DÉJÀ réordonnancées (DPB + recul des B gérés par le
				// décodeur) : le reader décode tout à l'ouverture et garde les plans
				// YUV — Decode(index) n'est plus qu'une conversion RGBA, le curseur
				// générique (ReadFrame/SeekFrame/CurrentIndex) suffit, et le seek est
				// O(1) (pas de redécodage depuis le dernier I/GOP).
				NkVector<NkMpeg2Frame> mpeg2Frames; // toutes les images (ordre d'AFFICHAGE)

				// ── Theora (Ogg .ogv) ───────────────────────────────────────────────
				// Le décodeur consomme le conteneur Ogg ENTIER (Open + DecodeNextFrame
				// séquentiel ; pas de B en Theora : ordre décodage == ordre affichage
				// -> curseur générique). Retour arrière (SeekFrame) : ré-Open (reset
				// complet golden/previous) puis redécodage en avant depuis le début en
				// jetant les images intermédiaires (l'API ne repart pas d'une image
				// arbitraire). La piste audio (Vorbis) est ignorée par le décodeur
				// (il ne suit que le flux logique Theora).
				NkTheoraDecoder theoraDec;
				int32 theoraNextIndex = 0; // prochaine image que DecodeNextFrame produira

				const NkHevcFrame *HevcFindByPoc(int32 poc) const {
					for (uint64 k = 0; k < hevcDpb.Size(); ++k)
						if (hevcDpb[k].poc == poc)
							return &hevcDpb[k];
					return nullptr;
				}

				// Dimensions d'AFFICHAGE = pic_width/height - fenêtre de conformance
				// (×SubWidthC/SubHeightC, §7.4.3.2.1 ; 4:2:0 -> ×2 sur les deux axes).
				void HevcSetDimsFromSps() {
					const int32 subW =
						(hevcSps.chromaFormatIdc == 1 || hevcSps.chromaFormatIdc == 2) ? 2 : 1;
					const int32 subH = (hevcSps.chromaFormatIdc == 1) ? 2 : 1;
					info.width = hevcSps.width - subW * (hevcSps.confWinLeft + hevcSps.confWinRight);
					info.height = hevcSps.height - subH * (hevcSps.confWinTop + hevcSps.confWinBottom);
				}

				// Parse un HEVCDecoderConfigurationRecord (mêmes octets pour la boîte `hvcC`
				// ISOBMFF ET le `CodecPrivate` EBML V_MPEGH/ISO/HEVC) : lengthSizeMinusOne +
				// tableaux de NAL (VPS/SPS/PPS) regroupés par type. On n'extrait QUE les
				// structures SPS/PPS (le décodeur les prend en paramètre — pas besoin de
				// reconstruire un Annex-B ; le VPS n'est pas consommé par le décodeur).
				void ParseHvcCBytes(const uint8 *p, usize n) {
					if (n < 23)
						return;
					hevcNalLenSize = (p[21] & 3) + 1;
					const int32 numArrays = p[22];
					usize pos = 23;
					for (int32 a = 0; a < numArrays && pos + 3 <= n; ++a) {
						const int32 nalType = p[pos] & 0x3F;
						const int32 numNalus = ((int32)p[pos + 1] << 8) | p[pos + 2];
						pos += 3;
						for (int32 u = 0; u < numNalus && pos + 2 <= n; ++u) {
							const int32 len = ((int32)p[pos] << 8) | p[pos + 1];
							pos += 2;
							if (pos + (usize)len > n)
								return;
							if (nalType == 33) {
								NkHevcSps s;
								if (NkHevcDecoder::ParseSps(p + pos, (usize)len, s)) {
									hevcSps = s;
									hevcHaveSps = true;
								}
							} else if (nalType == 34) {
								NkHevcPps pp;
								if (NkHevcDecoder::ParsePps(p + pos, (usize)len, pp)) {
									hevcPps = pp;
									hevcHavePps = true;
								}
							}
							pos += (usize)len;
						}
					}
				}

				// Précalcule, image par image en ORDRE DE DÉCODAGE (sur `frames[]` = NAL de
				// slice VCL, un par image), le POC réel (§8.3.1, prevPocTid0 mis à jour
				// seulement par les images TemporalId==0 non-RASL/RADL/sous-couche-non-ref) et
				// une clé d'affichage GLOBALE monotone (gopBase incrémenté à chaque IDR + POC),
				// puis le min suffixe des clés (borne de sortie du réordonnancement). SPS/PPS
				// doivent déjà être résolus (`hevcSps`/`hevcPps`). Un en-tête de slice qui ne
				// parse pas (cas non géré) fait échouer l'ouverture proprement.
				bool HevcBuildPocTables() {
					const uint64 nf = frames.Size();
					if (nf == 0 || !hevcHaveSps || !hevcHavePps)
						return false;
					hevcPoc.Resize(nf);
					hevcGlobalKey.Resize(nf);
					hevcSuffixMinKey.Resize(nf);
					hevcIsIdr.Resize(nf);
					hevcKeyframe.Resize(nf);
					int32 prevPocTid0 = 0;
					int64 gopBase = 0;
					bool first = true;
					for (uint64 i = 0; i < nf; ++i) {
						const uint8 *nd = bytes.Data() + frames[i].offset;
						const usize nsz = frames[i].size;
						const int32 nalType = (nd[0] >> 1) & 0x3F;
						const int32 temporalId = (nd[1] & 0x7) - 1;
						NkHevcSliceHeader sh;
						if (!NkHevcDecoder::ParseSliceHeader(nd, nsz, hevcSps, hevcPps, sh))
							return false;
						const bool isIdr = (nalType == kHevcNalIdrWRadl || nalType == kHevcNalIdrNLp);
						const bool isIrap = (nalType >= 16 && nalType <= 23);
						const int32 poc = NkHevcDecoder::ComputePoc(sh.picOrderCntLsb, hevcSps.log2MaxPocLsb,
																	isIdr, prevPocTid0);
						if (isIdr && !first)
							gopBase += 1000000; // nouvelle clé de GOP : le GOP suivant s'affiche APRÈS
						hevcPoc[i] = poc;
						hevcIsIdr[i] = isIdr;
						hevcKeyframe[i] = isIrap;
						hevcGlobalKey[i] = gopBase + (int64)poc;
						const bool subNonRef = (nalType == kHevcNalTrailN || nalType == kHevcNalTsaN ||
												nalType == kHevcNalStsaN || nalType == kHevcNalRadlN ||
												nalType == kHevcNalRaslN);
						const bool raslRadl = (nalType == kHevcNalRadlN || nalType == kHevcNalRadlR ||
											   nalType == kHevcNalRaslN || nalType == kHevcNalRaslR);
						if (temporalId == 0 && !subNonRef && !raslRadl)
							prevPocTid0 = poc;
						first = false;
					}
					int64 mn = 0x7FFFFFFFFFFFFFFFLL;
					for (uint64 j = nf; j-- > 0;) {
						if (hevcGlobalKey[j] < mn)
							mn = hevcGlobalKey[j];
						hevcSuffixMinKey[j] = mn;
					}
					return true;
				}

				// Remplace `frames[]` (une entrée par ÉCHANTILLON MP4/MKV = access unit, NALU
				// longueur-préfixées) par une entrée par NAL de SLICE VCL (la 1re de chaque
				// échantillon), pointant DIRECTEMENT sur l'en-tête NAL dans `bytes` : le
				// décodeur prend ce NAL tel quel (pas de reconstruction Annex-B nécessaire —
				// il reçoit SPS/PPS en paramètre). Récupère aussi SPS/PPS en bande (hev1) si
				// le hvcC ne les portait pas. Retourne false si aucune slice trouvée.
				bool HevcSliceNalsFromSamples() {
					NkVector<FrameRef> out;
					for (uint64 i = 0; i < frames.Size(); ++i) {
						const usize base = frames[i].offset;
						const usize end = base + frames[i].size;
						usize p = base;
						bool tookSlice = false;
						while (p + (usize)hevcNalLenSize <= end) {
							uint32 len = 0;
							for (int32 k = 0; k < hevcNalLenSize; ++k)
								len = (len << 8) | bytes.Data()[p + k];
							p += (usize)hevcNalLenSize;
							if (p + len > end || len < 2)
								break;
							const int32 nalType = (bytes.Data()[p] >> 1) & 0x3F;
							if (nalType == 33 && !hevcHaveSps) {
								NkHevcSps s;
								if (NkHevcDecoder::ParseSps(bytes.Data() + p, len, s)) {
									hevcSps = s;
									hevcHaveSps = true;
								}
							} else if (nalType == 34 && !hevcHavePps) {
								NkHevcPps pp;
								if (NkHevcDecoder::ParsePps(bytes.Data() + p, len, pp)) {
									hevcPps = pp;
									hevcHavePps = true;
								}
							} else if (nalType <= 31 && !tookSlice) {
								FrameRef fr;
								fr.offset = p;
								fr.size = len;
								out.PushBack(fr);
								tookSlice = true;
							}
							p += len;
						}
					}
					if (out.Size() == 0)
						return false;
					frames = traits::NkMove(out);
					return true;
				}

				// --- Parse un flux élémentaire HEVC Annex-B brut (.265/.hevc) ---
				// Découpe tous les NAL (en-tête 2 octets), parse VPS/SPS/PPS en bande, puis
				// regroupe en IMAGES : une image commence à un NAL VCL avec
				// first_slice_segment_in_pic_flag=1 ; `frames[i]` pointe sur cette 1re slice VCL
				// (en-tête NAL inclus). Les slices dépendantes / 2e slices d'une même image
				// (x265 par défaut n'en émet pas) sont ignorées.
				bool ParseHevcAnnexB() {
					NkVector<NkHevcNal> nals;
					NkHevcDecoder::SplitNalsAnnexB(bytes.Data(), (usize)bytes.Size(), nals);
					for (uint64 i = 0; i < nals.Size(); ++i) {
						const uint8 *nd = bytes.Data() + nals[i].offset;
						if (nals[i].type == kHevcNalSps && !hevcHaveSps) {
							NkHevcSps s;
							if (NkHevcDecoder::ParseSps(nd, nals[i].size, s)) {
								hevcSps = s;
								hevcHaveSps = true;
							}
						} else if (nals[i].type == kHevcNalPps && !hevcHavePps) {
							NkHevcPps pp;
							if (NkHevcDecoder::ParsePps(nd, nals[i].size, pp)) {
								hevcPps = pp;
								hevcHavePps = true;
							}
						}
					}
					if (!hevcHaveSps || !hevcHavePps)
						return false;
					for (uint64 i = 0; i < nals.Size(); ++i) {
						if (nals[i].type > 31 || nals[i].layerId != 0)
							continue; // pas un NAL VCL de couche de base
						const uint8 *nd = bytes.Data() + nals[i].offset;
						NkHevcSliceHeader sh;
						if (!NkHevcDecoder::ParseSliceHeader(nd, nals[i].size, hevcSps, hevcPps, sh))
							continue;
						if (!sh.firstSliceSegmentInPic)
							continue; // 2e slice / slice dépendante de l'image courante
						FrameRef fr;
						fr.offset = nals[i].offset;
						fr.size = nals[i].size;
						frames.PushBack(fr);
					}
					if (frames.Size() == 0)
						return false;
					hevcNalLenSize = 0; // Annex-B : NAL déjà isolés dans `frames[]`
					codec = Codec::HEVC;
					info.codec = NkString("hevc");
					info.container = NkString("hevc");
					info.fps = 25.0; // Annex-B : pas de cadence en bande -> repli 25
					HevcSetDimsFromSps();
					if (!HevcBuildPocTables())
						return false;
					info.frameCount = (int32)frames.Size();
					return true;
				}

				// Découpe chaque bloc en sous-trames VP9 (ParseSuperframe) puis lit l'en-tête
				// non compressé de chacune (ParseUncompressedHeader, SANS décoder) pour
				// classer clé/inter, affichée/masquée, et mettre en cache les champs requis
				// au décodage. `info.width/height` doivent déjà être posés (guess pour les
				// trames à taille héritée d'une référence, normatif seulement pour l'inter).
				bool Vp9ScanUnits() {
					vp9Units.Clear();
					vp9DisplayUnits.Clear();
					for (uint64 i = 0; i < frames.Size(); ++i) {
						const uint8 *blockData = bytes.Data() + frames[i].offset;
						const usize blockSize = frames[i].size;
						NkVp9Superframe sf;
						if (!NkVp9Decoder::ParseSuperframe(blockData, blockSize, sf))
							return false;
						for (int32 fr = 0; fr < sf.count; ++fr) {
							NkVp9FrameHeader hdr;
							if (!NkVp9Decoder::ParseUncompressedHeader(blockData + sf.offsets[fr], sf.sizes[fr],
																	   hdr, info.width, info.height))
								return false;
							Vp9Unit u;
							u.blockIdx = (int32)i;
							u.subOffset = sf.offsets[fr];
							u.subSize = sf.sizes[fr];
							u.showFrame = hdr.showFrame;
							u.showExistingFrame = hdr.showExistingFrame;
							u.frameToShowMapIdx = hdr.frameToShowMapIdx;
							u.isKeyOrIntraOnly =
								!hdr.showExistingFrame && (hdr.frameType == kVp9KeyFrame || hdr.intraOnly);
							u.errorResilient = hdr.errorResilient;
							u.refFrameIdx[0] = hdr.refFrameIdx[0];
							u.refFrameIdx[1] = hdr.refFrameIdx[1];
							u.refFrameIdx[2] = hdr.refFrameIdx[2];
							u.refreshFrameFlags = hdr.refreshFrameFlags;
							u.width = hdr.width;
							u.height = hdr.height;
							vp9Units.PushBack(u);
							if (hdr.showFrame || hdr.showExistingFrame)
								vp9DisplayUnits.PushBack((int32)(vp9Units.Size() - 1));
						}
					}
					return vp9DisplayUnits.Size() > 0;
				}

				// Parse un AVCDecoderConfigurationRecord (mêmes octets pour la boîte `avcC` ISOBMFF
				// ET `CodecPrivate` EBML/Matroska V_MPEG4/ISO/AVC — seul le wrapper de boîte diffère,
				// absent côté EBML) : extrait SPS/PPS (1er jeu seulement, comme l'original) +
				// `nalLenSize`, et borne `h264ReorderMax` via `max_num_ref_frames` du SPS. Partagé par
				// ParseMov (MP4/MOV) et ParseWebm (MKV/WebM).
				void ParseAvcCBytes(const uint8 *p, usize n) {
					if (n < 7)
						return;
					nalLenSize = (p[4] & 3) + 1;
					usize pos = 5;
					const int32 numSps = p[pos] & 0x1F;
					++pos;
					for (int32 s = 0; s < numSps && pos + 2 <= n; ++s) {
						const int32 len = (int32)RdU16BE(p + pos);
						pos += 2;
						if (pos + (usize)len > n)
							break;
						if (s == 0)
							for (int32 i = 0; i < len; ++i)
								h264Sps.PushBack(p[pos + i]);
						pos += (usize)len;
					}
					if (pos < n) {
						const int32 numPps = p[pos];
						++pos;
						for (int32 s = 0; s < numPps && pos + 2 <= n; ++s) {
							const int32 len = (int32)RdU16BE(p + pos);
							pos += 2;
							if (pos + (usize)len > n)
								break;
							if (s == 0)
								for (int32 i = 0; i < len; ++i)
									h264Pps.PushBack(p[pos + i]);
							pos += (usize)len;
						}
					}
					NkH264Sps sps;
					if (h264Sps.Size() > 0 &&
						NkH264Decoder::ParseSps(h264Sps.Data(), (usize)h264Sps.Size(), sps) && sps.valid) {
						int32 rm = sps.numRefFrames;
						h264ReorderMax = rm < 1 ? 1 : (rm > 16 ? 16 : rm);
					}
				}

				// Index des images clés (IDR) en ORDRE DE DÉCODAGE, requis pour SeekFrame : un
				// échantillon AVCC peut contenir plusieurs NAL (longueur-préfixées, `nalLenSize`
				// octets) ; il est IDR si l'une d'elles a type=5. Scan une seule fois à l'ouverture
				// (pas de décodage, juste les en-têtes NAL). Partagé par ParseMov/ParseWebm (AVCC,
				// longueur-préfixé) ET ParseTs (Annex-B, start codes — `backend` DOIT être posé
				// AVANT cet appel pour ces deux conventions, contrairement à `nalLenSize`).
				void ScanH264Keyframes() {
					h264Keyframe.Resize(frames.Size());
					for (uint64 i = 0; i < frames.Size(); ++i) {
						const uint8 *s = bytes.Data() + frames[i].offset;
						const usize sz = frames[i].size;
						bool idr = false;
						if (backend == Backend::TS) {
							NkVector<NkH264Nal> nals;
							NkH264Decoder::SplitNalsAnnexB(s, sz, nals);
							for (uint64 k = 0; k < nals.Size(); ++k)
								if (nals[k].type == 5) {
									idr = true;
									break;
								}
						} else {
							usize p = 0;
							while (p + (usize)nalLenSize <= sz) {
								uint32 len = 0;
								for (int32 k = 0; k < nalLenSize; ++k)
									len = (len << 8) | s[p + k];
								p += (usize)nalLenSize;
								if (p + len > sz)
									break;
								if (len > 0 && (s[p] & 0x1F) == 5) {
									idr = true;
									break;
								}
								p += len;
							}
						}
						h264Keyframe[i] = idr;
					}
				}

				// --- Parse AVI : remplit info + frames ---
				bool ParseAvi() {
					const uint8 *d = bytes.Data();
					const usize n = (usize)bytes.Size();
					if (n < 12 || !Tag(d, 'R', 'I', 'F', 'F') || !Tag(d + 8, 'A', 'V', 'I', ' '))
						return false;

					uint32 comp = 0;
					int32 w = 0, h = 0, bc = 24;
					double fps = 0.0;
					bool haveVideoStrf = false;
					char lastStrh[4] = {0, 0, 0, 0};

					// Parcours récursif d'une région [start,end) de chunks RIFF.
					struct Walker {
							const uint8 *d;
							usize n;
							uint32 *comp;
							int32 *w, *h, *bc;
							double *fps;
							bool *haveVideoStrf;
							char *lastStrh;
							NkVector<FrameRef> *frames;

							void CollectMovi(usize start, usize end) {
								usize pos = start;
								while (pos + 8 <= end) {
									const uint8 *id = d + pos;
									uint32 sz = RdU32LE(d + pos + 4);
									usize body = pos + 8;
									if (body + sz > end)
										break;
									// image vidéo = '##dc' (compressé) ou '##db' (DIB brut)
									if (id[0] >= '0' && id[0] <= '9' && id[1] >= '0' && id[1] <= '9' && id[2] == 'd' &&
										(id[3] == 'c' || id[3] == 'b') && sz > 0) {
										FrameRef fr;
										fr.offset = body;
										fr.size = sz;
										frames->PushBack(fr);
									}
									pos = body + sz;
									pos = (pos + 1) & ~(usize)1;
								}
							}

							void Walk(usize start, usize end) {
								usize pos = start;
								while (pos + 8 <= end) {
									const uint8 *id = d + pos;
									uint32 sz = RdU32LE(d + pos + 4);
									usize body = pos + 8;
									if (body > end)
										break;
									usize avail = (body + sz <= end) ? sz : (end - body);

									if (Tag(id, 'L', 'I', 'S', 'T') && avail >= 4) {
										const uint8 *lt = d + body;
										if (Tag(lt, 'm', 'o', 'v', 'i'))
											CollectMovi(body + 4, body + avail);
										else if (Tag(lt, 'h', 'd', 'r', 'l') || Tag(lt, 's', 't', 'r', 'l'))
											Walk(body + 4, body + avail);
									} else if (Tag(id, 'a', 'v', 'i', 'h') && avail >= 4) {
										uint32 usPerFrame = RdU32LE(d + body);
										if (usPerFrame > 0)
											*fps = 1000000.0 / (double)usPerFrame;
									} else if (Tag(id, 's', 't', 'r', 'h') && avail >= 4) {
										lastStrh[0] = (char)id[0]; // (inutile) ; on lit fccType
										const uint8 *ft = d + body;
										lastStrh[0] = (char)ft[0];
										lastStrh[1] = (char)ft[1];
										lastStrh[2] = (char)ft[2];
										lastStrh[3] = (char)ft[3];
									} else if (Tag(id, 's', 't', 'r', 'f') && !*haveVideoStrf && avail >= 20 &&
											   lastStrh[0] == 'v' && lastStrh[1] == 'i' && lastStrh[2] == 'd' &&
											   lastStrh[3] == 's') {
										// BITMAPINFOHEADER
										*w = RdI32LE(d + body + 4);
										int32 hh = RdI32LE(d + body + 8);
										*h = (hh < 0) ? -hh : hh;
										*bc = (int32)((uint32)d[body + 14] | ((uint32)d[body + 15] << 8));
										*comp = RdU32LE(d + body + 16);
										*haveVideoStrf = true;
									}

									pos = body + sz;
									pos = (pos + 1) & ~(usize)1;
								}
							}
					};

					Walker wk;
					wk.d = d;
					wk.n = n;
					wk.comp = &comp;
					wk.w = &w;
					wk.h = &h;
					wk.bc = &bc;
					wk.fps = &fps;
					wk.haveVideoStrf = &haveVideoStrf;
					wk.lastStrh = lastStrh;
					wk.frames = &frames;
					wk.Walk(12, n);

					if (!haveVideoStrf || w <= 0 || h <= 0 || frames.Size() == 0)
						return false;

					// biCompression : 'MJPG' => MJPEG ; 0 => RGB brut.
					uint8 cb[4] = {(uint8)(comp & 0xFF), (uint8)((comp >> 8) & 0xFF), (uint8)((comp >> 16) & 0xFF),
								   (uint8)((comp >> 24) & 0xFF)};
					if (Tag(cb, 'M', 'J', 'P', 'G') || Tag(cb, 'm', 'j', 'p', 'g')) {
						codec = Codec::MJPEG;
						info.codec = NkString("mjpeg");
					} else if (comp == 0) {
						codec = Codec::RAWRGB;
						info.codec = NkString("rawrgb");
					} else {
						return false; // codec AVI non géré (ex. autre fourcc)
					}
					bitCount = bc;
					info.width = w;
					info.height = h;
					info.frameCount = (int32)frames.Size();
					info.fps = fps;
					info.container = NkString("avi");
					return true;
				}

				// Trouve le 1er box `t` (4 car) enfant direct dans [s,e). ps/pe = payload.
				static bool Box(const uint8 *d, usize s, usize e, const char *t, usize &ps, usize &pe) {
					usize p = s;
					while (p + 8 <= e) {
						uint64 bsz = RdU32BE(d + p);
						usize hdr = 8;
						if (bsz == 1) {
							if (p + 16 > e)
								break;
							bsz = RdU64BE(d + p + 8);
							hdr = 16;
						} else if (bsz == 0) {
							bsz = (uint64)(e - p);
						}
						if (bsz < hdr || p + (usize)bsz > e)
							break;
						if (Tag(d + p + 4, t[0], t[1], t[2], t[3])) {
							ps = p + hdr;
							pe = p + (usize)bsz;
							return true;
						}
						p += (usize)bsz;
					}
					return false;
				}

				// --- Parse MOV/MP4 (ISOBMFF) : piste vidéo -> table des samples + info ---
				bool ParseMov() {
					const uint8 *d = bytes.Data();
					const usize n = (usize)bytes.Size();
					usize moovS = 0, moovE = 0;
					if (!Box(d, 0, n, "moov", moovS, moovE))
						return false;

					usize stsdS = 0, stsdE = 0, stszS = 0, stszE = 0, stcoS = 0, stcoE = 0, stscS = 0, stscE = 0;
					usize sttsS = 0, sttsE = 0;
					bool co64 = false, found = false;
					uint32 timescale = 0;

					usize p = moovS;
					while (p + 8 <= moovE && !found) {
						uint64 bsz = RdU32BE(d + p);
						usize hdr = 8;
						if (bsz == 1) {
							if (p + 16 > moovE)
								break;
							bsz = RdU64BE(d + p + 8);
							hdr = 16;
						} else if (bsz == 0)
							bsz = (uint64)(moovE - p);
						if (bsz < hdr || p + (usize)bsz > moovE)
							break;
						if (Tag(d + p + 4, 't', 'r', 'a', 'k')) {
							const usize trS = p + hdr, trE = p + (usize)bsz;
							usize mdiaS, mdiaE;
							if (Box(d, trS, trE, "mdia", mdiaS, mdiaE)) {
								usize hS, hE;
								bool isVid = Box(d, mdiaS, mdiaE, "hdlr", hS, hE) && (hE - hS >= 12) &&
											 Tag(d + hS + 8, 'v', 'i', 'd', 'e');
								if (isVid) {
									usize mdhdS, mdhdE;
									if (Box(d, mdiaS, mdiaE, "mdhd", mdhdS, mdhdE)) {
										uint8 ver = d[mdhdS];
										if (ver == 1 && mdhdE - mdhdS >= 28)
											timescale = RdU32BE(d + mdhdS + 20);
										else if (mdhdE - mdhdS >= 16)
											timescale = RdU32BE(d + mdhdS + 12);
									}
									usize minfS, minfE, stblS, stblE;
									if (Box(d, mdiaS, mdiaE, "minf", minfS, minfE) &&
										Box(d, minfS, minfE, "stbl", stblS, stblE)) {
										usize a, b;
										if (Box(d, stblS, stblE, "stsd", a, b)) {
											stsdS = a;
											stsdE = b;
										}
										if (Box(d, stblS, stblE, "stsz", a, b)) {
											stszS = a;
											stszE = b;
										}
										if (Box(d, stblS, stblE, "stco", a, b)) {
											stcoS = a;
											stcoE = b;
											co64 = false;
										} else if (Box(d, stblS, stblE, "co64", a, b)) {
											stcoS = a;
											stcoE = b;
											co64 = true;
										}
										if (Box(d, stblS, stblE, "stsc", a, b)) {
											stscS = a;
											stscE = b;
										}
										if (Box(d, stblS, stblE, "stts", a, b)) {
											sttsS = a;
											sttsE = b;
										}
										found = (stsdS && stszS && stcoS && stscS);
									}
								}
							}
						}
						p += (usize)bsz;
					}
					if (!found)
						return false;

					// stsd -> codec + dimensions (VisualSampleEntry).
					if (stsdE - stsdS < 16)
						return false;
					const usize entS = stsdS + 8; // saute version/flags/entryCount
					if (entS + 36 > stsdE)
						return false;
					const uint8 *etype = d + entS + 4;
					int32 w = (int32)RdU16BE(d + entS + 32);
					int32 h = (int32)RdU16BE(d + entS + 34);
					const bool isMjpeg = Tag(etype, 'm', 'j', 'p', 'a') || Tag(etype, 'j', 'p', 'e', 'g') ||
										 Tag(etype, 'M', 'J', 'P', 'G');
					const bool isAvc = Tag(etype, 'a', 'v', 'c', '1') || Tag(etype, 'a', 'v', 'c', '3');
					const bool isHevc = Tag(etype, 'h', 'v', 'c', '1') || Tag(etype, 'h', 'e', 'v', '1');
					if (!isMjpeg && !isAvc && !isHevc)
						return false;

					// H264 : extrait SPS/PPS de la box avcC (enfant de l'entrée avc1, après 78 octets fixes).
					if (isAvc) {
						const uint32 entrySize = RdU32BE(d + entS);
						const usize avc1End = entS + (usize)entrySize;
						usize acS, acE;
						if (entS + 8 + 78 <= avc1End && Box(d, entS + 8 + 78, avc1End, "avcC", acS, acE) &&
							acE - acS >= 7)
							ParseAvcCBytes(d + acS, acE - acS);
					}
					// HEVC : extrait SPS/PPS de la box hvcC (même position que avcC, après 78 octets).
					if (isHevc) {
						const uint32 entrySize = RdU32BE(d + entS);
						const usize hvEnd = entS + (usize)entrySize;
						usize hcS, hcE;
						if (entS + 8 + 78 <= hvEnd && Box(d, entS + 8 + 78, hvEnd, "hvcC", hcS, hcE) &&
							hcE - hcS >= 23)
							ParseHvcCBytes(d + hcS, hcE - hcS);
					}

					// Assemble la table des samples via stsz + stco/co64 + stsc.
					if (stszE - stszS < 12 || stcoE - stcoS < 8 || stscE - stscS < 8)
						return false;
					const uint32 sampleSize = RdU32BE(d + stszS + 4);
					const uint32 sampleCount = RdU32BE(d + stszS + 8);
					const uint32 chunkCount = RdU32BE(d + stcoS + 4);
					const uint32 stscCount = RdU32BE(d + stscS + 4);
					if (sampleCount == 0 || chunkCount == 0 || stscCount == 0)
						return false;

					auto sizeOf = [&](uint32 i) -> uint32 {
						if (sampleSize)
							return sampleSize;
						const usize off = stszS + 12 + (usize)i * 4;
						return (off + 4 <= stszE) ? RdU32BE(d + off) : 0;
					};
					auto chunkOffset = [&](uint32 c1) -> uint64 { // c1 = index de chunk 1-based
						const usize base = stcoS + 8;
						if (co64) {
							const usize o = base + (usize)(c1 - 1) * 8;
							return (o + 8 <= stcoE) ? RdU64BE(d + o) : 0;
						}
						const usize o = base + (usize)(c1 - 1) * 4;
						return (o + 4 <= stcoE) ? (uint64)RdU32BE(d + o) : 0;
					};

					uint32 sampleIdx = 0;
					for (uint32 e = 0; e < stscCount && sampleIdx < sampleCount; ++e) {
						const usize eo = stscS + 8 + (usize)e * 12;
						if (eo + 12 > stscE)
							break;
						const uint32 firstChunk = RdU32BE(d + eo);
						const uint32 spc = RdU32BE(d + eo + 4);
						const uint32 nextFirst =
							(e + 1 < stscCount && eo + 12 + 12 <= stscE) ? RdU32BE(d + eo + 12) : (chunkCount + 1);
						for (uint32 c = firstChunk; c < nextFirst && sampleIdx < sampleCount; ++c) {
							if (c < 1 || c > chunkCount)
								break;
							uint64 off = chunkOffset(c);
							for (uint32 s = 0; s < spc && sampleIdx < sampleCount; ++s) {
								const uint32 sz = sizeOf(sampleIdx);
								if (off + sz <= (uint64)n && sz > 0) {
									FrameRef fr;
									fr.offset = (usize)off;
									fr.size = sz;
									frames.PushBack(fr);
								}
								off += sz;
								++sampleIdx;
							}
						}
					}
					if (frames.Size() == 0)
						return false;

					// fps via stts + timescale.
					double fps = 0.0;
					if (sttsS && timescale > 0 && sttsE - sttsS >= 8) {
						const uint32 nent = RdU32BE(d + sttsS + 4);
						uint64 total = 0, nsamp = 0;
						for (uint32 i = 0; i < nent; ++i) {
							const usize o = sttsS + 8 + (usize)i * 8;
							if (o + 8 > sttsE)
								break;
							const uint32 cnt = RdU32BE(d + o);
							const uint32 dl = RdU32BE(d + o + 4);
							total += (uint64)cnt * (uint64)dl;
							nsamp += cnt;
						}
						if (total > 0)
							fps = (double)timescale * (double)nsamp / (double)total;
					}

					codec = isMjpeg ? Codec::MJPEG : (isHevc ? Codec::HEVC : Codec::H264);
					info.codec = NkString(isMjpeg ? "mjpeg" : (isHevc ? "hevc" : "h264"));
					info.container = NkString("mov");
					info.width = w;
					info.height = h;
					info.frameCount = (int32)frames.Size();
					info.fps = fps;
					if (codec == Codec::HEVC) {
						// `frames[]` = échantillons (AU) : les remplacer par les NAL de slice VCL,
						// puis précalculer POC + clés d'affichage (réordonnancement B).
						if (!hevcHaveSps || !hevcHavePps || !HevcSliceNalsFromSamples())
							return false;
						HevcSetDimsFromSps();
						if (!HevcBuildPocTables())
							return false;
						info.frameCount = (int32)frames.Size();
						return true;
					}
					if (codec == Codec::H264)
						ScanH264Keyframes();
					// Dimensions manquantes + MJPEG : on décode la 1re image pour les obtenir.
					if ((w <= 0 || h <= 0) && codec == Codec::MJPEG) {
						NkVideoFrame f0;
						if (Decode(0, f0)) {
							info.width = f0.width;
							info.height = f0.height;
						}
					}
					return true;
				}

				// --- Parse WebM/Matroska (EBML) : piste vidéo H264 uniquement pour l'instant ---
				// VP8/VP9/AV1 (les codecs vidéo natifs les plus courants en WebM) échouent proprement
				// (pas de décodeur) — un H264-en-MKV (rips courants) se lit via le décodeur existant,
				// AUCUN changement requis côté décodage (CodecPrivate EBML = mêmes octets que avcC).
				// Scan des blocs VP8 : repère les images AFFICHÉES (`show_frame`) et les clés
				// via le frame tag non compressé de chaque bloc. `frames` reste la liste de
				// TOUS les blocs (altref invisibles comprises, à décoder mais pas afficher).
				bool Vp8ScanBlocks() {
					vp8DisplayBlocks.Clear();
					vp8Keyframe.Clear();
					for (uint64 i = 0; i < frames.Size(); ++i) {
						NkVp8FrameTag tag;
						if (!NkVp8ParseFrameTag(bytes.Data() + frames[i].offset, frames[i].size,
												 tag))
							return false;
						if (tag.showFrame) {
							vp8DisplayBlocks.PushBack((int32)i);
							vp8Keyframe.PushBack(tag.keyFrame);
						}
					}
					return vp8DisplayBlocks.Size() > 0;
				}

				bool ParseWebm() {
					const uint8 *d = bytes.Data();
					const usize n = (usize)bytes.Size();
					WebmVideoTrackInfo found;
					WalkWebmTracks(d, 0, n, &found, nullptr, 0);
					if (found.num < 0 || found.codecId.Empty())
						return false;
					const bool isVp8 = found.codecId.Contains("VP8");
					const bool isVp9 = found.codecId.Contains("VP9");
					const bool isHevc = found.codecId.Contains("HEVC") || found.codecId.Contains("MPEGH");
					if (!isVp8 && !isVp9 && !isHevc && !found.codecId.Contains("AVC") &&
						!found.codecId.Contains("MPEG4"))
						return false; // AV1 etc. : pas de décodeur -> échec propre
					if (isHevc) {
						if (found.codecPriv.Size() >= 23)
							ParseHvcCBytes(found.codecPriv.Data(), (usize)found.codecPriv.Size());
						// SPS/PPS peuvent aussi arriver en bande (hev1) -> résolus dans
						// HevcSliceNalsFromSamples ci-dessous ; échec propre si toujours absents.
					} else if (!isVp8 && !isVp9) {
						if (found.codecPriv.Size() < 7)
							return false;
						ParseAvcCBytes(found.codecPriv.Data(), (usize)found.codecPriv.Size());
						if (h264Sps.Size() == 0 || h264Pps.Size() == 0)
							return false;
					}
					NkVector<int64> ts;
					WalkWebmVideoClusters(d, 0, n, found.num, 0, frames, ts, 0);
					if (frames.Size() == 0)
						return false;
					// fps dérivé des horodatages RÉELS des blocs (EBML n'a pas d'équivalent direct à
					// `stts` de l'ISOBMFF) ; repli 25 si dégénéré (un seul paquet, horodatages égaux…).
					double fps = 25.0;
					if (ts.Size() >= 2 && ts[ts.Size() - 1] > ts[0])
						fps = 1000.0 * (double)(ts.Size() - 1) / (double)(ts[ts.Size() - 1] - ts[0]);
					info.container = NkString("webm");
					info.width = found.width;
					info.height = found.height;
					info.fps = fps;
					if (isHevc) {
						codec = Codec::HEVC;
						info.codec = NkString("hevc");
						if (!HevcSliceNalsFromSamples() || !hevcHaveSps || !hevcHavePps)
							return false;
						HevcSetDimsFromSps();
						if (!HevcBuildPocTables())
							return false;
						info.frameCount = (int32)frames.Size();
					} else if (isVp8) {
						codec = Codec::VP8;
						info.codec = NkString("vp8");
						if (!Vp8ScanBlocks())
							return false;
						info.frameCount = (int32)vp8DisplayBlocks.Size();
					} else if (isVp9) {
						codec = Codec::VP9;
						info.codec = NkString("vp9");
						if (!Vp9ScanUnits())
							return false;
						info.frameCount = (int32)vp9DisplayUnits.Size();
					} else {
						codec = Codec::H264;
						info.codec = NkString("h264");
						info.frameCount = (int32)frames.Size();
						ScanH264Keyframes();
					}
					return true;
				}

				// --- Parse IVF : conteneur brut minimal (DKIF), VP8 (VP80) ou VP9 (VP90) ---
				bool ParseIvf() {
					const uint8 *d = bytes.Data();
					const usize n = (usize)bytes.Size();
					if (n < 32 || d[0] != 'D' || d[1] != 'K' || d[2] != 'I' || d[3] != 'F')
						return false;
					const bool isVp8 = Tag(d + 8, 'V', 'P', '8', '0');
					const bool isVp9 = Tag(d + 8, 'V', 'P', '9', '0');
					if (!isVp8 && !isVp9)
						return false; // AV01… : pas de décodeur -> échec propre
					const int32 w = (int32)(d[12] | (d[13] << 8));
					const int32 h = (int32)(d[14] | (d[15] << 8));
					const uint32 rate = RdU32LE(d + 16);  // framerate numerator
					const uint32 scale = RdU32LE(d + 20); // framerate denominator
					usize pos = 32;
					while (pos + 12 <= n) {
						const uint32 sz = RdU32LE(d + pos);
						if (pos + 12 + sz > n)
							break;
						FrameRef fr;
						fr.offset = pos + 12;
						fr.size = sz;
						frames.PushBack(fr);
						pos += 12 + (usize)sz;
					}
					if (frames.Size() == 0)
						return false;
					info.container = NkString("ivf");
					info.width = w;
					info.height = h;
					info.fps = (scale > 0) ? ((double)rate / (double)scale) : 25.0;
					if (isVp9) {
						if (!Vp9ScanUnits())
							return false;
						codec = Codec::VP9;
						info.codec = NkString("vp9");
						info.frameCount = (int32)vp9DisplayUnits.Size();
					} else {
						if (!Vp8ScanBlocks())
							return false;
						codec = Codec::VP8;
						info.codec = NkString("vp8");
						info.frameCount = (int32)vp8DisplayBlocks.Size();
					}
					return true;
				}

				// --- Parse TS/M2TS (MPEG Transport Stream) : piste vidéo H264 uniquement ---
				// Contrairement à ParseMov/ParseWebm (samples AVCC contigus dans le fichier), les
				// données d'une image H264 sont ÉPARPILLÉES sur plusieurs paquets TS de 188 octets
				// (payload utile ~184 octets/paquet) : il faut les RÉASSEMBLER. `bytes` (le fichier
				// TS brut) est donc REMPLACÉ par le flux Annex-B reconstruit (une entrée `frames[i]`
				// = un paquet PES complet, SPS/PPS PRÉFIXÉS depuis le dernier jeu vu en flux si ce
				// paquet ne les répète pas lui-même — même logique que MOV/WebM qui les stockent hors
				// bande, sauf qu'ici SPS/PPS sont EN BANDE, normal pour du TS/broadcast).
				bool ParseTs() {
					const uint8 *d = bytes.Data();
					const usize n = (usize)bytes.Size();
					const int32 pktSize = DetectTsPacketSize(d, n);
					if (pktSize == 0)
						return false;
					const int32 syncOff = (pktSize == 192) ? 4 : 0;
					const int32 pmtPid = FindPmtPid(d, n, pktSize, syncOff);
					if (pmtPid < 0)
						return false;
					int32 streamType = 0;
					const int32 vpid = FindVideoPid(d, n, pktSize, syncOff, pmtPid, &streamType);
					if (vpid < 0)
						return false; // pas de piste H264/MPEG-2 (HEVC non branché) -> échec propre

					// ── MPEG-2 Video (stream_type 0x02) : réassemble l'ES complet (PES headers
					// retirés, payloads concaténés — pas de découpage par image nécessaire :
					// NkMpeg2Decoder::DecodeAll consomme le flux entier), puis MÊME chemin que
					// le .m2v (FinishMpeg2Es : fps du sequence header + décodage complet).
					if (streamType == 0x02) {
						NkVector<nk_uint8> esBytes;
						NkVector<nk_uint8> curPes;
						bool havePes = false;
						auto flushPesM2 = [&]() {
							// PES vidéo : 00 00 01 <stream_id E0..EF>, en-tête 9 + PES_header_data_length.
							if (curPes.Size() >= 9 && curPes[0] == 0 && curPes[1] == 0 && curPes[2] == 1 &&
								curPes[3] >= 0xE0 && curPes[3] <= 0xEF) {
								const usize esStart = 9 + (usize)curPes[8];
								for (usize k = esStart; k < curPes.Size(); ++k)
									esBytes.PushBack(curPes[k]);
							}
							curPes.Resize(0);
						};
						for (usize p = (usize)syncOff; p + 188 <= n; p += (usize)pktSize) {
							if (d[p] != 0x47)
								continue;
							const int32 pid = (((int32)(d[p + 1] & 0x1F)) << 8) | d[p + 2];
							if (pid != vpid)
								continue;
							const bool pusi = (d[p + 1] & 0x40) != 0;
							const int32 afc = (d[p + 3] >> 4) & 0x3;
							usize payload = p + 4;
							if (afc == 2)
								continue; // adaptation seule, pas de payload
							if (afc == 3) {
								const int32 al = d[payload];
								payload += 1 + (usize)al;
							}
							if (payload > p + 188)
								continue;
							if (pusi) {
								flushPesM2();
								havePes = true;
							}
							if (havePes)
								for (usize i = payload; i < p + 188; ++i)
									curPes.PushBack(d[i]);
						}
						flushPesM2();
						if (esBytes.Size() == 0)
							return false;
						bytes = traits::NkMove(esBytes); // remplace les paquets TS bruts par l'ES
						backend = Backend::TS;
						return FinishMpeg2Es(bytes.Data(), (usize)bytes.Size(), "ts");
					}

					NkVector<nk_uint8> outBytes;
					NkVector<nk_uint8> curPes;
					NkVector<nk_uint8> cachedSps, cachedPps;
					bool havePes = false;

					auto flushPes = [&]() {
						if (curPes.Size() < 9 || !(curPes[0] == 0 && curPes[1] == 0 && curPes[2] == 1)) {
							curPes.Resize(0);
							return;
						}
						const usize hdrDataLen = curPes[8]; // PES_header_data_length
						const usize esStart = 9 + hdrDataLen;
						if (esStart >= curPes.Size()) {
							curPes.Resize(0);
							return;
						}
						const uint8 *es = curPes.Data() + esStart;
						const usize esSize = curPes.Size() - esStart;
						NkVector<NkH264Nal> nals;
						NkH264Decoder::SplitNalsAnnexB(es, esSize, nals);
						bool hasSps = false, hasPps = false, hasSlice = false;
						for (uint64 i = 0; i < nals.Size(); ++i) {
							if (nals[i].type == 7) {
								hasSps = true;
								cachedSps.Resize(0);
								for (usize k = 0; k < nals[i].size; ++k)
									cachedSps.PushBack(es[nals[i].offset + k]);
							} else if (nals[i].type == 8) {
								hasPps = true;
								cachedPps.Resize(0);
								for (usize k = 0; k < nals[i].size; ++k)
									cachedPps.PushBack(es[nals[i].offset + k]);
							} else if (nals[i].type == 1 || nals[i].type == 5) {
								hasSlice = true;
							}
						}
						if (!hasSlice) { // paquet audio/AUD/SEI seul -> rien à décoder ici
							curPes.Resize(0);
							return;
						}
						const usize frameStart = outBytes.Size();
						auto appendStartCode = [&]() {
							outBytes.PushBack(0);
							outBytes.PushBack(0);
							outBytes.PushBack(0);
							outBytes.PushBack(1);
						};
						if (!hasSps && cachedSps.Size() > 0) {
							appendStartCode();
							for (uint64 k = 0; k < cachedSps.Size(); ++k)
								outBytes.PushBack(cachedSps[k]);
						}
						if (!hasPps && cachedPps.Size() > 0) {
							appendStartCode();
							for (uint64 k = 0; k < cachedPps.Size(); ++k)
								outBytes.PushBack(cachedPps[k]);
						}
						for (usize k = esStart; k < curPes.Size(); ++k)
							outBytes.PushBack(curPes[k]);
						FrameRef fr;
						fr.offset = frameStart;
						fr.size = outBytes.Size() - frameStart;
						frames.PushBack(fr);
						curPes.Resize(0);
					};

					for (usize p = (usize)syncOff; p + 188 <= n; p += (usize)pktSize) {
						if (d[p] != 0x47)
							continue;
						const int32 pid = (((int32)(d[p + 1] & 0x1F)) << 8) | d[p + 2];
						if (pid != vpid)
							continue;
						const bool pusi = (d[p + 1] & 0x40) != 0;
						const int32 afc = (d[p + 3] >> 4) & 0x3;
						usize payload = p + 4;
						if (afc == 2)
							continue; // adaptation seule, pas de payload
						if (afc == 3) {
							const int32 al = d[payload];
							payload += 1 + (usize)al;
						}
						if (payload > p + 188)
							continue;
						if (pusi) {
							flushPes();
							havePes = true;
						}
						if (havePes)
							for (usize i = payload; i < p + 188; ++i)
								curPes.PushBack(d[i]);
					}
					flushPes();
					if (frames.Size() == 0)
						return false;

					bytes = traits::NkMove(outBytes); // remplace les paquets TS bruts par l'ES assemblé
					backend = Backend::TS;			   // AVANT ScanH264Keyframes (convention Annex-B)
					codec = Codec::H264;
					info.codec = NkString("h264");
					info.container = NkString("ts");
					// Dimensions via le dernier SPS vu (TS n'a pas d'équivalent PixelWidth/Height).
					NkH264Sps sps;
					if (cachedSps.Size() > 0 && NkH264Decoder::ParseSps(cachedSps.Data(), (usize)cachedSps.Size(), sps) &&
						sps.valid) {
						info.width = sps.width;
						info.height = sps.height;
					}
					info.frameCount = (int32)frames.Size();
					// Pas d'horodatage simplement exploitable (PCR/PTS 90 kHz, wrap 33 bits) sans
					// démuxage complet du PES : repli sur 25 fps, cohérent avec l'estimation MOV/WebM
					// en l'absence de métadonnée exacte.
					info.fps = 25.0;
					ScanH264Keyframes();
					return true;
				}

				// Termine l'ouverture d'un flux élémentaire MPEG-2 (.m2v direct OU ES
				// réassemblé depuis un TS) : fps/dimensions depuis le sequence header
				// (00 00 01 B3), puis décodage COMPLET via NkMpeg2Decoder::DecodeAll (le
				// décodeur gère le DPB + le réordonnancement des B et rend les images en
				// ORDRE D'AFFICHAGE). `containerName` = "m2v" ou "ts".
				bool FinishMpeg2Es(const uint8 *es, usize esSize, const char *containerName) {
					double fps = 0.0;
					int32 seqW = 0, seqH = 0;
					for (usize p = 0; p + 8 <= esSize; ++p) {
						if (es[p] == 0 && es[p + 1] == 0 && es[p + 2] == 1 && es[p + 3] == 0xB3) {
							seqW = ((int32)es[p + 4] << 4) | (es[p + 5] >> 4);
							seqH = (((int32)es[p + 5] & 0x0F) << 8) | es[p + 6];
							fps = Mpeg2FrameRateFromCode(es[p + 7] & 0x0F);
							break;
						}
					}
					char errmsg[256] = {0};
					if (!NkMpeg2Decoder::DecodeAll(es, esSize, mpeg2Frames, errmsg, sizeof(errmsg)) &&
						mpeg2Frames.Size() == 0)
						return false; // échec dur sans aucune image (ex. flux entrelacé refusé)
					if (mpeg2Frames.Size() == 0)
						return false;
					codec = Codec::MPEG2;
					info.codec = NkString("mpeg2");
					info.container = NkString(containerName);
					info.width = mpeg2Frames[0].width > 0 ? mpeg2Frames[0].width : seqW;
					info.height = mpeg2Frames[0].height > 0 ? mpeg2Frames[0].height : seqH;
					info.frameCount = (int32)mpeg2Frames.Size();
					info.fps = fps > 0.0 ? fps : 25.0;
					return true;
				}

				// --- Parse un flux élémentaire MPEG-2 Video brut (.m2v) ---
				bool ParseMpeg2Es() {
					return FinishMpeg2Es(bytes.Data(), (usize)bytes.Size(), "m2v");
				}

				// --- Parse Ogg/Theora (.ogv) ---
				// NkTheoraDecoder consomme le conteneur Ogg ENTIER (Open + boucle
				// DecodeNextFrame) et ignore les autres flux logiques (Vorbis…). Le reader
				// ne re-démuxe donc PAS les pages pour décoder — il fait seulement un
				// parcours Ogg minimal pour connaître `frameCount` à l'avance (nombre de
				// paquets DONNÉES du flux Theora = paquets terminés par le lacing, moins
				// les 3 en-têtes) et lire la cadence FRN/FRD (en-tête d'identification,
				// big-endian, offsets 22/26).
				bool ParseOgv() {
					const uint8 *d = bytes.Data();
					const usize n = (usize)bytes.Size();
					if (!NkTheoraDecoder::Probe(d, n))
						return false; // Ogg sans flux Theora (ex. .ogg audio pur) -> échec propre
					NkString err;
					if (!theoraDec.Open(d, n, &err))
						return false;
					// Passe 1 : serial du flux Theora + fps depuis l'en-tête d'identification
					// (1er paquet de la page BOS : 0x80 "theora" … FRN@22 FRD@26, big-endian).
					uint32 serial = 0;
					bool haveSerial = false;
					double fps = 0.0;
					{
						usize pos = 0;
						OggPageView pg;
						while (pos < n && OggReadPage(d, n, pos, pg)) {
							if ((pg.headerType & 0x02) != 0 && pg.next - pg.payloadOffset >= 30) {
								const uint8 *pl = d + pg.payloadOffset;
								if (pl[0] == 0x80 && pl[1] == 't' && pl[2] == 'h' && pl[3] == 'e' &&
									pl[4] == 'o' && pl[5] == 'r' && pl[6] == 'a') {
									serial = pg.serial;
									haveSerial = true;
									const uint32 frn = RdU32BE(pl + 22);
									const uint32 frd = RdU32BE(pl + 26);
									if (frn > 0 && frd > 0)
										fps = (double)frn / (double)frd;
									break;
								}
							}
							pos = pg.next;
						}
					}
					if (!haveSerial)
						return false;
					// Passe 2 : compte les paquets du flux (un paquet se termine à une valeur
					// de lacing < 255 ; un paquet multi-pages n'est compté qu'une fois, à sa
					// terminaison). Paquets données = total - 3 en-têtes (0x80/0x81/0x82).
					int32 packetCount = 0;
					{
						usize pos = 0;
						OggPageView pg;
						while (pos < n && OggReadPage(d, n, pos, pg)) {
							if (pg.serial == serial)
								for (int32 i = 0; i < pg.nsegs; ++i)
									if (pg.segTable[i] < 255)
										++packetCount;
							pos = pg.next;
						}
					}
					if (packetCount <= 3)
						return false;
					codec = Codec::THEORA;
					info.codec = NkString("theora");
					info.container = NkString("ogv");
					info.width = theoraDec.PictureWidth();
					info.height = theoraDec.PictureHeight();
					info.frameCount = packetCount - 3;
					info.fps = fps > 0.0 ? fps : 25.0;
					theoraNextIndex = 0;
					return true;
				}

				// --- Parse FLV (Flash Video) : piste vidéo H264 uniquement ---
				// Conteneur par TAGS séquentiels (pas de table d'échantillons). ⭐ H264-en-FLV
				// utilise EXACTEMENT le même AVCDecoderConfigurationRecord (`AVCPacketType==0`) et
				// le même format NALU longueur-préfixé (`AVCPacketType==1`) que la boîte `avcC`
				// ISOBMFF -> `ParseAvcCBytes` + le chemin de décodage AVCC existant (comme MOV) sont
				// réutilisés SANS AUCUN CHANGEMENT (contrairement à TS qui est Annex-B en bande).
				bool ParseFlv() {
					const uint8 *d = bytes.Data();
					const usize n = (usize)bytes.Size();
					if (n < 13 || !(d[0] == 'F' && d[1] == 'L' && d[2] == 'V'))
						return false;
					const uint32 headerSize = RdU32BE(d + 5);
					if ((usize)headerSize + 4 > n)
						return false;
					usize pos = (usize)headerSize + 4; // header + 1er PreviousTagSize (toujours 0)
					NkVector<int64> ts;
					while (pos + 11 <= n) {
						const uint8 tagType = d[pos];
						const uint32 dataSize = ((uint32)d[pos + 1] << 16) | ((uint32)d[pos + 2] << 8) | d[pos + 3];
						const uint32 tsLow = ((uint32)d[pos + 4] << 16) | ((uint32)d[pos + 5] << 8) | d[pos + 6];
						const uint32 tsExt = d[pos + 7];
						const int64 timestampMs = (int64)(((uint32)tsExt << 24) | tsLow);
						const usize dataStart = pos + 11;
						if (dataStart + (usize)dataSize > n)
							break;
						if (tagType == 9 && dataSize >= 5) { // TagType 9 = vidéo
							const uint8 codecId = d[dataStart] & 0x0F;
							if (codecId == 7) { // AVC/H264
								const uint8 pktType = d[dataStart + 1];
								if (pktType == 0) { // AVCDecoderConfigurationRecord (SPS/PPS)
									ParseAvcCBytes(d + dataStart + 5, (usize)dataSize - 5);
								} else if (pktType == 1 && dataSize > 5) { // NALU(s), AVCC longueur-préfixé
									FrameRef fr;
									fr.offset = dataStart + 5;
									fr.size = (usize)dataSize - 5;
									frames.PushBack(fr);
									ts.PushBack(timestampMs);
								}
							}
						}
						pos = dataStart + (usize)dataSize + 4; // + PreviousTagSize suivant
					}
					if (frames.Size() == 0 || h264Sps.Size() == 0 || h264Pps.Size() == 0)
						return false;
					double fps = 25.0;
					if (ts.Size() >= 2 && ts[ts.Size() - 1] > ts[0])
						fps = 1000.0 * (double)(ts.Size() - 1) / (double)(ts[ts.Size() - 1] - ts[0]);
					codec = Codec::H264;
					info.codec = NkString("h264");
					info.container = NkString("flv");
					info.frameCount = (int32)frames.Size();
					info.fps = fps;
					NkH264Sps sps;
					if (h264Sps.Size() > 0 &&
						NkH264Decoder::ParseSps(h264Sps.Data(), (usize)h264Sps.Size(), sps) && sps.valid) {
						info.width = sps.width;
						info.height = sps.height;
					}
					backend = Backend::FLV; // avant ScanH264Keyframes (par cohérence, AVCC ici)
					ScanH264Keyframes();
					return true;
				}

				// Charge un fichier image (PNG/JPEG/BMP/TGA…) -> RGBA via NKImage.
				static bool LoadImageFile(const char *path, int32 index, NkVideoFrame &out) {
					NkImage img;
					if (!img.Load(path, 4))
						return false;
					const int32 w = img.Width(), h = img.Height(), ch = img.Channels();
					const uint8 *px = img.Pixels();
					if (w <= 0 || h <= 0 || !px)
						return false;
					out.width = w;
					out.height = h;
					out.rgba.Resize((uint64)w * (uint64)h * 4u);
					uint8 *o = out.rgba.Data();
					for (int32 i = 0; i < w * h; ++i) {
						const uint8 *s = px + (usize)i * (usize)ch;
						o[i * 4 + 0] = s[0];
						o[i * 4 + 1] = (ch >= 2) ? s[1] : s[0];
						o[i * 4 + 2] = (ch >= 3) ? s[2] : s[0];
						o[i * 4 + 3] = (ch >= 4) ? s[3] : 255;
					}
					out.index = index;
					return true;
				}

				// Extension image reconnue (insensible à la casse).
				static bool HasImageExt(const char *s) {
					usize n = 0;
					while (s[n])
						++n;
					auto ends = [&](const char *e) {
						usize m = 0;
						while (e[m])
							++m;
						if (m > n)
							return false;
						for (usize i = 0; i < m; ++i) {
							char a = s[n - m + i];
							if (a >= 'A' && a <= 'Z')
								a = (char)(a - 'A' + 'a');
							if (a != e[i])
								return false;
						}
						return true;
					};
					return ends(".png") || ends(".jpg") || ends(".jpeg") || ends(".bmp") || ends(".tga") ||
						   ends(".qoi") || ends(".ppm");
				}

				// --- Parse une SÉQUENCE d'images (dossier) : chemins triés + dims de la 1re ---
				bool ParseSequence(const char *dir) {
					NkVector<NkString> files =
						NkDirectory::GetFiles(dir, "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
					for (uint64 i = 0; i < files.Size(); ++i)
						if (HasImageExt(files[i].CStr()))
							seqPaths.PushBack(files[i]);
					if (seqPaths.Size() == 0)
						return false;
					// Tri lexicographique (frame_001, frame_002, …).
					for (uint64 i = 0; i + 1 < seqPaths.Size(); ++i) {
						uint64 mn = i;
						for (uint64 j = i + 1; j < seqPaths.Size(); ++j)
							if (strcmp(seqPaths[j].CStr(), seqPaths[mn].CStr()) < 0)
								mn = j;
						if (mn != i) {
							NkString t = seqPaths[i];
							seqPaths[i] = seqPaths[mn];
							seqPaths[mn] = t;
						}
					}
					NkVideoFrame f0;
					if (!LoadImageFile(seqPaths[0].CStr(), 0, f0))
						return false;
					info.width = f0.width;
					info.height = f0.height;
					info.frameCount = (int32)seqPaths.Size();
					info.fps = 30.0; // débit par défaut (inconnu pour une séquence)
					info.codec = NkString("image");
					info.container = NkString("sequence");
					return true;
				}

				// Décode l'image `index` en RGBA dans `out`.
				bool Decode(int32 index, NkVideoFrame &out) {
					if (backend == Backend::SEQUENCE) {
						if (index < 0 || index >= (int32)seqPaths.Size())
							return false;
						return LoadImageFile(seqPaths[(uint64)index].CStr(), index, out);
					}

					// ── MPEG-2 : toutes les images sont DÉJÀ décodées (ordre d'affichage,
					// FinishMpeg2Es) — il ne reste que la conversion YUV -> RGBA. Accès
					// direct par index : Seek O(1), pas de contrainte de séquentialité.
					// (Placé AVANT le garde-fou `frames[]` : ce codec ne remplit pas la
					// table des FrameRef, comme le chemin SEQUENCE.)
					if (codec == Codec::MPEG2) {
						if (index < 0 || index >= (int32)mpeg2Frames.Size())
							return false;
						const NkMpeg2Frame &f = mpeg2Frames[(uint64)index];
						const int32 w = f.width, h = f.height;
						const int32 cw = f.dispChromaW, chh = f.dispChromaH;
						if (w <= 0 || h <= 0 || cw <= 0 || chh <= 0)
							return false;
						// YUV -> RGBA (BT.601 limited-range, chroma nearest) — même conversion
						// que les chemins H264/VP8/VP9/HEVC. Décalages chroma dérivés des dims
						// d'affichage (4:2:0 : les deux axes ; 4:2:2 : horizontal seulement).
						const int32 sx = (cw < w) ? 1 : 0, sy = (chh < h) ? 1 : 0;
						out.width = w;
						out.height = h;
						out.rgba.Resize((uint64)w * (uint64)h * 4u);
						uint8 *o = out.rgba.Data();
						for (int32 y = 0; y < h; ++y)
							for (int32 x = 0; x < w; ++x) {
								const int32 Y = f.y[(usize)y * (usize)f.lumaW + (usize)x];
								const int32 U =
									f.cb[(usize)(y >> sy) * (usize)f.chromaW + (usize)(x >> sx)];
								const int32 V =
									f.cr[(usize)(y >> sy) * (usize)f.chromaW + (usize)(x >> sx)];
								const int32 C = Y - 16, D = U - 128, E = V - 128;
								auto cl = [](int32 v) -> uint8 {
									return (uint8)(v < 0 ? 0 : (v > 255 ? 255 : v));
								};
								const usize oi = ((usize)y * (usize)w + (usize)x) * 4;
								o[oi + 0] = cl((298 * C + 409 * E + 128) >> 8);
								o[oi + 1] = cl((298 * C - 100 * D - 208 * E + 128) >> 8);
								o[oi + 2] = cl((298 * C + 516 * D + 128) >> 8);
								o[oi + 3] = 255;
							}
						out.index = index;
						return true;
					}

					// ── Theora : décodage strictement séquentiel via NkTheoraDecoder (pas de
					// B : ordre décodage == ordre affichage). Saut en AVANT : décoder en
					// jetant les images intermédiaires. Retour ARRIÈRE : ré-Open (reset
					// complet de l'état golden/previous du décodeur, qui ne sait pas
					// repartir d'une image arbitraire) puis redécodage depuis le début.
					// (Placé AVANT le garde-fou `frames[]`, comme MPEG-2.)
					if (codec == Codec::THEORA) {
						if (index < 0 || index >= info.frameCount)
							return false;
						if (index != theoraNextIndex) {
							if (index < theoraNextIndex) {
								if (!theoraDec.Open(bytes.Data(), (usize)bytes.Size()))
									return false;
								theoraNextIndex = 0;
							}
							NkTheoraFrame skipf;
							while (theoraNextIndex < index) {
								if (!theoraDec.DecodeNextFrame(skipf))
									return false;
								++theoraNextIndex;
							}
						}
						NkTheoraFrame f;
						if (!theoraDec.DecodeNextFrame(f))
							return false;
						++theoraNextIndex;
						const int32 w = f.width, h = f.height;
						const int32 cw = f.chromaWidth, chh = f.chromaHeight;
						if (w <= 0 || h <= 0 || cw <= 0 || chh <= 0)
							return false;
						// YUV -> RGBA (BT.601 limited-range, chroma nearest) — même conversion
						// que les autres codecs. Plans rognés à la picture region, stride = dims
						// affichées. Décalages chroma dérivés (gère 4:2:0 / 4:2:2 / 4:4:4).
						const int32 sx = (cw < w) ? 1 : 0, sy = (chh < h) ? 1 : 0;
						out.width = w;
						out.height = h;
						out.rgba.Resize((uint64)w * (uint64)h * 4u);
						uint8 *o = out.rgba.Data();
						for (int32 y = 0; y < h; ++y)
							for (int32 x = 0; x < w; ++x) {
								const int32 Y = f.y[(usize)y * (usize)w + (usize)x];
								const int32 U = f.cb[(usize)(y >> sy) * (usize)cw + (usize)(x >> sx)];
								const int32 V = f.cr[(usize)(y >> sy) * (usize)cw + (usize)(x >> sx)];
								const int32 C = Y - 16, D = U - 128, E = V - 128;
								auto cl = [](int32 v) -> uint8 {
									return (uint8)(v < 0 ? 0 : (v > 255 ? 255 : v));
								};
								const usize oi = ((usize)y * (usize)w + (usize)x) * 4;
								o[oi + 0] = cl((298 * C + 409 * E + 128) >> 8);
								o[oi + 1] = cl((298 * C - 100 * D - 208 * E + 128) >> 8);
								o[oi + 2] = cl((298 * C + 516 * D + 128) >> 8);
								o[oi + 3] = 255;
							}
						out.index = index;
						return true;
					}

					if (index < 0 || index >= (int32)frames.Size())
						return false;
					const FrameRef &fr = frames[(uint64)index];
					const uint8 *src = bytes.Data() + fr.offset;

					if (codec == Codec::MJPEG) {
						NkImage *img = NkJPEGCodec::Decode(src, fr.size);
						if (!img)
							return false;
						const int32 w = img->Width(), h = img->Height(), ch = img->Channels();
						const uint8 *px = img->Pixels();
						if (w <= 0 || h <= 0 || !px) {
							img->Free();
							return false;
						}
						out.width = w;
						out.height = h;
						out.rgba.Resize((uint64)w * (uint64)h * 4u);
						uint8 *o = out.rgba.Data();
						for (int32 i = 0; i < w * h; ++i) {
							const uint8 *s = px + (usize)i * (usize)ch;
							o[i * 4 + 0] = s[0];
							o[i * 4 + 1] = (ch >= 2) ? s[1] : s[0];
							o[i * 4 + 2] = (ch >= 3) ? s[2] : s[0];
							o[i * 4 + 3] = (ch >= 4) ? s[3] : 255;
						}
						img->Free();
						out.index = index;
						return true;
					}

					if (codec == Codec::RAWRGB) {
						// DIB bottom-up, BGR(A), lignes alignées sur 4 octets.
						const int32 w = info.width, h = info.height;
						const int32 bpp = bitCount / 8;
						if (bpp < 3)
							return false;
						const int32 rowSize = ((w * bpp + 3) / 4) * 4;
						if (fr.size < (usize)rowSize * (usize)h)
							return false;
						out.width = w;
						out.height = h;
						out.rgba.Resize((uint64)w * (uint64)h * 4u);
						uint8 *o = out.rgba.Data();
						for (int32 y = 0; y < h; ++y) {
							const uint8 *row = src + (usize)(h - 1 - y) * (usize)rowSize; // bottom-up
							for (int32 x = 0; x < w; ++x) {
								const uint8 *s = row + (usize)x * (usize)bpp;
								o[(y * w + x) * 4 + 0] = s[2]; // R (BGR)
								o[(y * w + x) * 4 + 1] = s[1]; // G
								o[(y * w + x) * 4 + 2] = s[0]; // B
								o[(y * w + x) * 4 + 3] = (bpp >= 4) ? s[3] : 255;
							}
						}
						out.index = index;
						return true;
					}

					if (codec == Codec::H264) {
						// Deux conventions selon le conteneur : MOV/WebM stockent SPS/PPS HORS BANDE
						// (avcC/CodecPrivate) et les échantillons en AVCC longueur-préfixé -> il faut
						// construire l'Annex-B (start codes + SPS/PPS + NAL). TS assemble DÉJÀ un
						// Annex-B complet et autonome par image dans ParseTs (SPS/PPS EN BANDE comme
						// tout flux broadcast) -> `frames[i]` est directement utilisable telle quelle.
						NkVector<nk_uint8> ab; // MOV/WebM seulement (TS n'en a pas besoin)
						const uint8 *annexB;
						usize annexBSize;
						bool isIdr = false;
						if (backend == Backend::TS) {
							annexB = src;
							annexBSize = fr.size;
							NkVector<NkH264Nal> nals;
							NkH264Decoder::SplitNalsAnnexB(annexB, annexBSize, nals);
							for (uint64 i = 0; i < nals.Size(); ++i)
								if (nals[i].type == 5) {
									isIdr = true;
									break;
								}
						} else {
							if (h264Sps.Size() == 0 || h264Pps.Size() == 0)
								return false;
							auto sc = [&]() {
								ab.PushBack(0);
								ab.PushBack(0);
								ab.PushBack(0);
								ab.PushBack(1);
							};
							sc();
							for (uint64 i = 0; i < h264Sps.Size(); ++i)
								ab.PushBack(h264Sps[i]);
							sc();
							for (uint64 i = 0; i < h264Pps.Size(); ++i)
								ab.PushBack(h264Pps[i]);
							const usize n = fr.size;
							usize p = 0;
							while (p + (usize)nalLenSize <= n) {
								uint32 len = 0;
								for (int32 i = 0; i < nalLenSize; ++i)
									len = (len << 8) | src[p + i];
								p += (usize)nalLenSize;
								if (p + len > n)
									break;
								if (len > 0 && (src[p] & 0x1F) == 5)
									isIdr = true; // NAL de type 5 = slice IDR (début de GOP)
								sc();
								for (uint32 i = 0; i < len; ++i)
									ab.PushBack(src[p + i]);
								p += len;
							}
							annexB = ab.Data();
							annexBSize = ab.Size();
						}
						lastIsIdr = isIdr;

						// Décodage séquentiel MULTI-RÉFÉRENCE : RefPicList0 = les dernières frames décodées
						// (h264Dpb[0] = la plus récente). Si on saute (non séquentiel), on repart d'une IDR.
						// ⚠️ Un IDR VIDE le DPB : les images du GOP précédent ne sont plus des références
						// (sinon les P/B du nouveau GOP prédiraient depuis de mauvaises images).
						const bool sequential = (index == h264PrevIndex + 1 && h264PrevIndex >= 0);
						if (!sequential || isIdr)
							h264Dpb.Clear();
						NkVector<const NkH264Frame *> refs;
						for (uint64 k = 0; k < h264Dpb.Size(); ++k)
							refs.PushBack(&h264Dpb[k]);
						NkH264Frame f;
						if (!NkH264Decoder::DecodeFrame(annexB, annexBSize, refs.Data(), (int32)refs.Size(), f))
							return false; // IDR requise en tête / cas non géré
						lastPoc = f.poc; // pour le réordonnancement d'affichage
						h264PrevIndex = index;

						// YUV 4:2:0 -> RGBA (BT.601 limited-range, chroma nearest). Fait AVANT le
						// stockage DPB ci-dessous (qui DEPLACE f) : lit encore f.y/f.cb/f.cr valides ici.
						const int32 w = f.cropW, h = f.cropH;
						out.width = w;
						out.height = h;
						out.rgba.Resize((uint64)w * (uint64)h * 4u);
						uint8 *o = out.rgba.Data();
						for (int32 y = 0; y < h; ++y)
							for (int32 x = 0; x < w; ++x) {
								const int32 Y = f.y[(usize)y * f.lumaW + x];
								const int32 U = f.cb[(usize)(y / 2) * f.chromaW + (x / 2)];
								const int32 V = f.cr[(usize)(y / 2) * f.chromaW + (x / 2)];
								const int32 C = Y - 16, D = U - 128, E = V - 128;
								auto cl = [](int32 v) -> uint8 { return (uint8)(v < 0 ? 0 : (v > 255 ? 255 : v)); };
								const usize oi = ((usize)y * (usize)w + (usize)x) * 4;
								o[oi + 0] = cl((298 * C + 409 * E + 128) >> 8);
								o[oi + 1] = cl((298 * C - 100 * D - 208 * E + 128) >> 8);
								o[oi + 2] = cl((298 * C + 516 * D + 128) >> 8);
								o[oi + 3] = 255;
							}
						// ⚠️ SEULES les images de RÉFÉRENCE entrent dans le DPB (nal_ref_idc != 0).
						// Une B non-référencée ne doit PAS y figurer (sinon l'état POC / le mouvement
						// co-localisé de la frame suivante serait faussé).
						// PERF : DEPLACE (traits::NkMove) au lieu de COPIER f + les ~15 anciennes entrées
						// -> évite de dupliquer les plans pixel (y/cb/cr) + les grilles de mouvement de
						// CHAQUE image du DPB à CHAQUE frame décodée. Coût mesuré avant fix : ~80ms/frame
						// et CROISSANT avec le nombre de frames (heap churn ~380 Ko x jusqu'à 16 entrées,
						// DEUX FOIS -- une fois en construisant newDpb, une fois en l'assignant), largement
						// dominant devant le décodage H264 lui-même (~5-6ms/frame) : ~5x de débit gagné sur
						// un vrai film (mesuré). f n'est plus utilisée après ce point (YUV extrait ci-dessus).
						if (f.isReference) {
							NkVector<NkH264Frame> newDpb;
							newDpb.PushBack(traits::NkMove(f));
							for (uint64 k = 0; k < h264Dpb.Size() && k < 15; ++k)
								newDpb.PushBack(traits::NkMove(h264Dpb[k]));
							h264Dpb = traits::NkMove(newDpb);
						}

						out.index = index;
						return true;
					}

					if (codec == Codec::VP8) {
						if (index < 0 || index >= (int32)vp8DisplayBlocks.Size())
							return false;
						// Séquentiel : décoder les blocs depuis le dernier affiché (les altref
						// invisibles intermédiaires mettent à jour les références). Saut : on
						// repart de la dernière image CLÉ affichée <= index (l'état inter
						// dépend de toute la chaîne depuis la clé).
						int32 startDisplay = vp8PrevIndex + 1;
						if (index != vp8PrevIndex + 1) {
							int32 kf = index;
							while (kf > 0 && !vp8Keyframe[(uint64)kf])
								kf = kf - 1;
							startDisplay = kf;
							vp8State = NkVp8DecoderState(); // repart d'un état vierge
						}
						const int32 firstBlock =
							(startDisplay > 0) ? vp8DisplayBlocks[(uint64)(startDisplay - 1)] + 1 : 0;
						NkVp8Image img;
						for (int32 b = firstBlock; b <= vp8DisplayBlocks[(uint64)index]; ++b) {
							if (!NkVp8DecodeFrame(vp8State, bytes.Data() + frames[(uint64)b].offset,
												   frames[(uint64)b].size, img))
								return false;
						}
						vp8PrevIndex = index;

						// YUV 4:2:0 -> RGBA (BT.601 limited-range, chroma nearest) — même
						// conversion que le chemin H264.
						const int32 w = img.width, h = img.height;
						out.width = w;
						out.height = h;
						out.rgba.Resize((uint64)w * (uint64)h * 4u);
						uint8 *o = out.rgba.Data();
						for (int32 y = 0; y < h; ++y)
							for (int32 x = 0; x < w; ++x) {
								const int32 Y = img.Y()[(int64)y * img.yStride + x];
								const int32 U = img.U()[(int64)(y / 2) * img.uvStride + (x / 2)];
								const int32 V = img.V()[(int64)(y / 2) * img.uvStride + (x / 2)];
								const int32 C = Y - 16, D = U - 128, E = V - 128;
								auto cl = [](int32 v) -> uint8 {
									return (uint8)(v < 0 ? 0 : (v > 255 ? 255 : v));
								};
								const usize oi = ((usize)y * (usize)w + (usize)x) * 4;
								o[oi + 0] = cl((298 * C + 409 * E + 128) >> 8);
								o[oi + 1] = cl((298 * C - 100 * D - 208 * E + 128) >> 8);
								o[oi + 2] = cl((298 * C + 516 * D + 128) >> 8);
								o[oi + 3] = 255;
							}
						out.index = index;
						return true;
					}

					if (codec == Codec::VP9) {
						if (index < 0 || index >= (int32)vp9DisplayUnits.Size())
							return false;
						// Séquentiel : continuer juste après la dernière sous-trame AFFICHÉE
						// décodée (les altref invisibles/masquées intermédiaires mettent à
						// jour les références). Saut : repartir de la dernière sous-trame
						// CLÉ/intra-only <= la cible (état inter dépendant de toute la
						// chaîne) — reset complet de l'état persistant (entropie, DPB 8
						// slots, éligibilité use_prev_frame_mvs).
						int32 startUnit;
						if (index == vp9PrevIndex + 1 && vp9PrevIndex >= 0) {
							startUnit = vp9DisplayUnits[(uint64)vp9PrevIndex] + 1;
						} else {
							const int32 targetForReset = vp9DisplayUnits[(uint64)index];
							int32 kf = 0;
							for (int32 u = 0; u <= targetForReset; ++u)
								if (vp9Units[(uint64)u].isKeyOrIntraOnly)
									kf = u;
							startUnit = kf;
							vp9Entropy = NkVp9EntropyState();
							for (int32 s = 0; s < 8; ++s)
								vp9SlotValid[s] = false;
							vp9PrevEligibleBase = false;
							vp9MvGrid.Resize(0);
						}
						const int32 targetUnit = vp9DisplayUnits[(uint64)index];
						NkVp9Image img;
						for (int32 u = startUnit; u <= targetUnit; ++u) {
							const Vp9Unit &vu = vp9Units[(uint64)u];
							if (vu.showExistingFrame) {
								// Réaffiche un slot déjà décodé : PAS de nouvelle décode, et
								// n'affecte ni l'éligibilité use_prev_frame_mvs ni les slots
								// (vp9_decoder.c : last_show_frame/swap sautés si
								// show_existing_frame).
								if (vu.frameToShowMapIdx < 0 || vu.frameToShowMapIdx >= 8 ||
									!vp9SlotValid[vu.frameToShowMapIdx])
									return false;
								img = vp9RefSlots[vu.frameToShowMapIdx];
								continue;
							}
							const uint8 *fdata = bytes.Data() + frames[(uint64)vu.blockIdx].offset + vu.subOffset;
							const usize fsize = vu.subSize;
							NkVp9Image decoded;
							bool ok;
							if (vu.isKeyOrIntraOnly) {
								ok = NkVp9Decoder::DecodeKeyFrame(fdata, fsize, decoded, vp9Entropy);
							} else {
								const NkVp9Image *refs[3];
								bool refsOk = true;
								for (int32 i = 0; i < 3; ++i) {
									const int32 slot = vu.refFrameIdx[i];
									if (slot < 0 || slot >= 8 || !vp9SlotValid[slot]) {
										refsOk = false;
										break;
									}
									refs[i] = &vp9RefSlots[slot];
								}
								if (!refsOk)
									return false;
								const bool eligible = vp9PrevEligibleBase && !vu.errorResilient &&
													  vu.width == vp9PrevWidth && vu.height == vp9PrevHeight;
								NkVector<NkVp9MvRef> nextMvGrid;
								ok = NkVp9Decoder::DecodeInterFrame(fdata, fsize, refs, decoded, vp9Entropy,
																	nullptr, true, eligible,
																	eligible ? vp9MvGrid.Data() : nullptr,
																	&nextMvGrid);
								if (ok)
									vp9MvGrid = traits::NkMove(nextMvGrid);
							}
							if (!ok)
								return false;
							img = decoded; // copie pour affichage AVANT le déplacement ci-dessous
							int32 lastSlot = -1;
							for (int32 s = 0; s < 8; ++s)
								if (vu.refreshFrameFlags & (1u << s))
									lastSlot = s;
							for (int32 s = 0; s < 8; ++s) {
								if (vu.refreshFrameFlags & (1u << s)) {
									vp9RefSlots[s] = (s == lastSlot) ? traits::NkMove(decoded) : decoded;
									vp9SlotValid[s] = true;
								}
							}
							// use_prev_frame_mvs (dec_api) exige aussi que la trame précédente ait
							// été AFFICHÉE (cm->last_show_frame) — une altref invisible casse
							// l'éligibilité de la trame qui la suit (piège déjà rencontré et
							// documenté dans le harnais --vp9multi de NkVideoReadTest).
							vp9PrevEligibleBase = !vu.isKeyOrIntraOnly && vu.showFrame;
							vp9PrevWidth = vu.width;
							vp9PrevHeight = vu.height;
						}
						vp9PrevIndex = index;

						const int32 w = img.width, h = img.height;
						if (w <= 0 || h <= 0)
							return false;
						// YUV 4:2:0 -> RGBA (BT.601 limited-range, chroma nearest) — même
						// conversion que les chemins H264/VP8.
						out.width = w;
						out.height = h;
						out.rgba.Resize((uint64)w * (uint64)h * 4u);
						uint8 *o = out.rgba.Data();
						for (int32 y = 0; y < h; ++y)
							for (int32 x = 0; x < w; ++x) {
								const int32 Y = img.y.Data()[(usize)y * (usize)img.yStride + (usize)x];
								const int32 U =
									img.u.Data()[(usize)(y >> 1) * (usize)img.uvStride + (usize)(x >> 1)];
								const int32 V =
									img.v.Data()[(usize)(y >> 1) * (usize)img.uvStride + (usize)(x >> 1)];
								const int32 C = Y - 16, D = U - 128, E = V - 128;
								auto cl = [](int32 v) -> uint8 {
									return (uint8)(v < 0 ? 0 : (v > 255 ? 255 : v));
								};
								const usize oi = ((usize)y * (usize)w + (usize)x) * 4;
								o[oi + 0] = cl((298 * C + 409 * E + 128) >> 8);
								o[oi + 1] = cl((298 * C - 100 * D - 208 * E + 128) >> 8);
								o[oi + 2] = cl((298 * C + 516 * D + 128) >> 8);
								o[oi + 3] = 255;
							}
						out.index = index;
						return true;
					}

					if (codec == Codec::HEVC) {
						// `frames[index]` = NAL de slice VCL (en-tête 2 octets inclus). Le décodeur
						// ne stocke aucun DPB : on résout les références (POC->pointeur) dans
						// `hevcDpb`. POC/type sont PRÉCALCULÉS (HevcBuildPocTables). Appelé en
						// ORDRE DE DÉCODAGE (ordre bitstream) par ReadFrame -> `hevcDpb` évolue
						// correctement (comme le DPB H.264).
						const uint8 *nalPtr = bytes.Data() + fr.offset;
						const usize nalSize = fr.size;
						const int32 nalType = (nalPtr[0] >> 1) & 0x3F;
						NkHevcSliceHeader sh;
						if (!NkHevcDecoder::ParseSliceHeader(nalPtr, nalSize, hevcSps, hevcPps, sh))
							return false;
						const int32 poc = hevcPoc[(uint64)index];
						NkHevcFrame frame;
						frame.poc = poc; // lu par DecodeSliceP/B (curPoc AMVP) : posé AVANT l'appel
						NkHevcSliceDataStats ds;
						bool decOk = false;
						if (sh.sliceType == kHevcSliceI) {
							decOk = NkHevcDecoder::DecodeSliceIntra(nalPtr, nalSize, hevcSps, hevcPps, sh,
																	frame, ds);
						} else {
							const bool isB = (sh.sliceType == kHevcSliceB);
							NkHevcRefPicLists rpl;
							NkHevcDecoder::BuildRefPicLists(sh.rps, poc, sh.numRefIdxL0Active,
															sh.numRefIdxL1Active, isB, rpl);
							if (rpl.numL0 < sh.numRefIdxL0Active ||
								(isB && rpl.numL1 < sh.numRefIdxL1Active))
								return false;
							const NkHevcFrame *refsL0[16];
							const NkHevcFrame *refsL1[16];
							for (int32 r = 0; r < sh.numRefIdxL0Active; ++r) {
								refsL0[r] = HevcFindByPoc(rpl.l0[r]);
								if (!refsL0[r])
									return false;
							}
							if (isB)
								for (int32 r = 0; r < sh.numRefIdxL1Active; ++r) {
									refsL1[r] = HevcFindByPoc(rpl.l1[r]);
									if (!refsL1[r])
										return false;
								}
							decOk = isB ? NkHevcDecoder::DecodeSliceB(nalPtr, nalSize, hevcSps, hevcPps, sh,
																	  refsL0, sh.numRefIdxL0Active, refsL1,
																	  sh.numRefIdxL1Active, frame, ds)
										: NkHevcDecoder::DecodeSliceP(nalPtr, nalSize, hevcSps, hevcPps, sh,
																	  refsL0, sh.numRefIdxL0Active, frame, ds);
						}
						if (!decOk)
							return false;

						// YUV 4:2:0 (uint16, 8 bits) -> RGBA (BT.601 limited-range, chroma nearest)
						// — même conversion que les chemins H264/VP8/VP9. Fait AVANT le stockage
						// DPB ci-dessous (qui DÉPLACE `frame`), lit encore frame.y/cb/cr valides ici.
						const int32 w = frame.cropW, h = frame.cropH;
						out.width = w;
						out.height = h;
						out.rgba.Resize((uint64)w * (uint64)h * 4u);
						uint8 *o = out.rgba.Data();
						for (int32 y = 0; y < h; ++y)
							for (int32 x = 0; x < w; ++x) {
								const int32 Y = frame.y[(usize)y * (usize)frame.lumaW + (usize)x];
								const int32 U =
									frame.cb[(usize)(y >> 1) * (usize)frame.chromaW + (usize)(x >> 1)];
								const int32 V =
									frame.cr[(usize)(y >> 1) * (usize)frame.chromaW + (usize)(x >> 1)];
								const int32 C = Y - 16, D = U - 128, E = V - 128;
								auto cl = [](int32 v) -> uint8 {
									return (uint8)(v < 0 ? 0 : (v > 255 ? 255 : v));
								};
								const usize oi = ((usize)y * (usize)w + (usize)x) * 4;
								o[oi + 0] = cl((298 * C + 409 * E + 128) >> 8);
								o[oi + 1] = cl((298 * C - 100 * D - 208 * E + 128) >> 8);
								o[oi + 2] = cl((298 * C + 516 * D + 128) >> 8);
								o[oi + 3] = 255;
							}

						// isReference d'après le type NAL (TRAIL_N/TSA_N/STSA_N/RADL_N/RASL_N =
						// non-référence, jamais gardés au DPB). Éviction DPB par le RPS de la
						// slice courante (§8.3.2 : le RPS énumère l'ENSEMBLE des images à
						// conserver) : on garde les POC listés, on ajoute la courante si ref.
						const bool nonRef = (nalType == kHevcNalTrailN || nalType == kHevcNalTsaN ||
											 nalType == kHevcNalStsaN || nalType == kHevcNalRadlN ||
											 nalType == kHevcNalRaslN);
						frame.isReference = !nonRef;
						{
							int32 keep[64];
							int32 nk = 0;
							for (int32 k = 0; k < sh.rps.numNegativePics && nk < 64; ++k)
								keep[nk++] = poc + sh.rps.deltaPocS0[k];
							for (int32 k = 0; k < sh.rps.numPositivePics && nk < 64; ++k)
								keep[nk++] = poc + sh.rps.deltaPocS1[k];
							NkVector<NkHevcFrame> nd;
							for (uint64 di = 0; di < hevcDpb.Size(); ++di) {
								bool keepIt = false;
								for (int32 k = 0; k < nk; ++k)
									if (hevcDpb[di].poc == keep[k]) {
										keepIt = true;
										break;
									}
								if (keepIt)
									nd.PushBack(traits::NkMove(hevcDpb[di]));
							}
							hevcDpb = traits::NkMove(nd);
							if (frame.isReference)
								hevcDpb.PushBack(traits::NkMove(frame)); // NkMove : plans+MV LOURDS
						}
						out.index = index;
						return true;
					}

					return false;
				}
		};

		NkVideoReader::NkVideoReader() {
			mImpl = (Impl *)memory::NkAlloc(sizeof(Impl));
			if (mImpl)
				new (mImpl) Impl();
		}

		NkVideoReader::~NkVideoReader() {
			if (mImpl) {
				mImpl->~Impl();
				memory::NkFree(mImpl);
				mImpl = nullptr;
			}
		}

		void NkVideoReader::Close() {
			if (!mImpl)
				return;
			mImpl->~Impl();
			new (mImpl) Impl();
		}

		bool NkVideoReader::IsOpen() const {
			return mImpl && mImpl->backend != Backend::NONE;
		}

		const NkVideoReaderInfo &NkVideoReader::Info() const {
			return mImpl->info;
		}

		int32 NkVideoReader::CurrentIndex() const {
			if (!mImpl)
				return -1;
			// ⚠️ Le chemin H264 (ReadFrame ci-dessus) réordonne par POC et avance
			// `h264OutCount` (ordre d'AFFICHAGE), PAS `cursor` (qui reste figé à sa valeur de
			// SeekFrame pour ce codec) -> un appelant qui compare CurrentIndex() à une cible
			// (ex. rattrapage vidéo/audio de NkVideoPlayer) ne voyait JAMAIS de progression et
			// bouclait donc à chaque tick jusqu'à épuiser son budget de temps, même une fois
			// rattrapé (cause réelle de la lenteur perçue, indépendante du débit du décodeur).
			if (mImpl->codec == Codec::H264)
				return mImpl->h264OutCount;
			if (mImpl->codec == Codec::HEVC)
				return mImpl->hevcOutCount;
			return mImpl->cursor;
		}

		bool NkVideoReader::Open(const char *path) {
			if (!mImpl || !path)
				return false;
			Close();

			// Dossier => séquence d'images.
			if (NkDirectory::Exists(path)) {
				if (mImpl->ParseSequence(path)) {
					mImpl->backend = Backend::SEQUENCE;
					mImpl->cursor = 0;
					return true;
				}
				return false;
			}

			mImpl->bytes = NkFile::ReadAllBytes(path);
			if (mImpl->bytes.Size() < 12)
				return false;
			const uint8 *d = mImpl->bytes.Data();

			// AVI (RIFF....AVI )
			if (Tag(d, 'R', 'I', 'F', 'F') && Tag(d + 8, 'A', 'V', 'I', ' ')) {
				if (mImpl->ParseAvi()) {
					mImpl->backend = Backend::AVI;
					mImpl->cursor = 0;
					return true;
				}
				return false;
			}

			// ISOBMFF (....ftyp) => MOV/MP4 : démux piste vidéo (MJPEG décodé ; H264 = métadonnées
			// seules pour l'instant, décodeur dédié à venir -> ReadFrame renverra false).
			if (mImpl->bytes.Size() >= 12 && Tag(d + 4, 'f', 't', 'y', 'p')) {
				if (mImpl->ParseMov()) {
					mImpl->backend = Backend::MOV;
					mImpl->cursor = 0;
					return true;
				}
				return false;
			}

			// EBML (Matroska/WebM), magie 0x1A45DFA3 : piste vidéo H264 uniquement pour l'instant
			// (VP8/VP9/AV1 -> ParseWebm échoue proprement, pas de décodeur).
			if (mImpl->bytes.Size() >= 4 && d[0] == 0x1A && d[1] == 0x45 && d[2] == 0xDF && d[3] == 0xA3) {
				if (mImpl->ParseWebm()) {
					mImpl->backend = Backend::WEBM;
					mImpl->cursor = 0;
					return true;
				}
				return false;
			}

			// FLV (Flash Video), magie "FLV" : piste vidéo H264 uniquement (Sorenson/VP6/Screen
			// Video -> ParseFlv échoue proprement, pas de décodeur).
			if (mImpl->bytes.Size() >= 13 && d[0] == 'F' && d[1] == 'L' && d[2] == 'V') {
				if (mImpl->ParseFlv()) {
					mImpl->cursor = 0; // backend déjà posé par ParseFlv
					return true;
				}
				return false;
			}

			// IVF (magie "DKIF") : conteneur brut minimal, VP8 géré (VP9/AV1 -> échec propre).
			if (mImpl->bytes.Size() >= 32 && d[0] == 'D' && d[1] == 'K' && d[2] == 'I' &&
				d[3] == 'F') {
				if (mImpl->ParseIvf()) {
					mImpl->backend = Backend::IVF;
					mImpl->cursor = 0;
					return true;
				}
				return false;
			}

			// Ogg (magie "OggS") : flux logique Theora (piste Vorbis ignorée). Un Ogg
			// audio pur (sans Theora) -> ParseOgv échoue proprement (Probe négatif).
			if (mImpl->bytes.Size() >= 27 && d[0] == 'O' && d[1] == 'g' && d[2] == 'g' && d[3] == 'S') {
				if (mImpl->ParseOgv()) {
					mImpl->backend = Backend::OGV;
					mImpl->cursor = 0;
					return true;
				}
				return false;
			}

			// Flux élémentaire MPEG-2 Video brut (.m2v) : start code de sequence header
			// (00 00 01 B3) en tête de fichier. Décodé ENTIÈREMENT à l'ouverture
			// (NkMpeg2Decoder::DecodeAll rend les images en ordre d'affichage).
			if (mImpl->bytes.Size() >= 8 && d[0] == 0x00 && d[1] == 0x00 && d[2] == 0x01 &&
				d[3] == 0xB3) {
				if (mImpl->ParseMpeg2Es()) {
					mImpl->backend = Backend::MPEG2_ES;
					mImpl->cursor = 0;
					return true;
				}
				return false;
			}

			// TS/M2TS : paquets 188 (ou 192 avec préfixe M2TS) de sync byte 0x47. Pistes vidéo
			// H264 (stream_type 0x1B) et MPEG-2 (0x02) ; HEVC -> ParseTs échoue proprement.
			if (DetectTsPacketSize(d, mImpl->bytes.Size()) != 0) {
				if (mImpl->ParseTs()) {
					mImpl->cursor = 0; // backend déjà posé par ParseTs (avant ScanH264Keyframes)
					return true;
				}
				return false;
			}

				// Flux élémentaire HEVC/H.265 Annex-B brut (.265/.hevc) : start code + en-tête NAL
				// HEVC 2 octets, distingué d'un ES H.264 par la présence d'un VPS/SPS HEVC (type
				// 32/33). Placé en dernier : aucun magic de conteneur ci-dessus n'est masqué.
				if (LooksLikeHevcAnnexB(d, mImpl->bytes.Size())) {
					if (mImpl->ParseHevcAnnexB()) {
						mImpl->backend = Backend::HEVC_ANNEXB;
						mImpl->cursor = 0;
						return true;
					}
					return false;
				}

			return false;
		}

		bool NkVideoReader::ReadFrame(NkVideoFrame &out) {
			if (!IsOpen())
				return false;
			Impl *m = mImpl;

			// ── Chemin H.264 : réordonnancement POC (décodage bitstream → affichage POC) ──
			// Les B se décodent dans le désordre ; on bufferise et on ressort par POC croissant.
			// Le tri utilise un POC GLOBAL monotone (gopBase + poc) : l'IDR d'un nouveau GOP a poc=0
			// mais un gopBase plus grand, donc il s'affiche APRÈS tout le GOP précédent. Le POC min
			// du buffer est ainsi toujours la prochaine image à afficher, sans flush explicite.
			if (m->codec == Codec::H264) {
				const int32 count = m->info.frameCount;
				for (;;) {
					// Sortir une image si le buffer est assez rempli (ou plus rien à décoder).
					const bool noMoreInput = (m->h264DecodeCursor >= count);
					const bool canOutput =
						m->h264Reorder.Size() > 0 &&
						((int32)m->h264Reorder.Size() > m->h264ReorderMax || noMoreInput);
					if (canOutput) {
						uint64 best = 0;
						for (uint64 k = 1; k < m->h264ReorderKey.Size(); ++k)
							if (m->h264ReorderKey[k] < m->h264ReorderKey[best])
								best = k;
						// PERF : DEPLACE (traits::NkMove) au lieu de COPIER — `out` et chaque élément
						// décalé pendant la compaction embarquent le buffer RGBA COMPLET de l'image
						// (~1 Mo à 720x360) ; une copie par assignation ici (au lieu d'un move) refaisait
						// jusqu'à ~4-5 copies de 1 Mo PAR IMAGE SORTIE (trouvé par échantillonnage gdb :
						// 6/8 relevés de pile du thread principal étaient dans NkVector<uint8>::operator=
						// appelé DIRECTEMENT depuis ReadFrame — même famille de bug que le DPB de
						// NkH264Decoder, ici sur le buffer de réordonnancement POC).
						out = traits::NkMove(m->h264Reorder[best]);
						// Retirer l'élément `best` (compaction).
						for (uint64 k = best + 1; k < m->h264Reorder.Size(); ++k) {
							m->h264Reorder[k - 1] = traits::NkMove(m->h264Reorder[k]);
							m->h264ReorderKey[k - 1] = m->h264ReorderKey[k];
						}
						m->h264Reorder.Resize(m->h264Reorder.Size() - 1);
						m->h264ReorderKey.Resize(m->h264ReorderKey.Size() - 1);
						out.index = m->h264OutCount; // ordre d'AFFICHAGE
						out.timestampMs =
							(m->info.fps > 0.0) ? (int64)((double)m->h264OutCount * 1000.0 / m->info.fps) : 0;
						++m->h264OutCount;
						return true;
					}
					if (noMoreInput)
						return false; // buffer vide et plus d'entrée -> fin
					// Décoder le prochain sample (ordre bitstream).
					NkVideoFrame f;
					const int32 idx = m->h264DecodeCursor;
					if (!m->Decode(idx, f)) {
						++m->h264DecodeCursor;
						continue; // sample non décodable -> on saute (ex. cas non géré)
					}
					if (m->lastIsIdr && idx != 0)
						m->h264GopBase += 1000000; // nouveau GOP -> POC repart, on décale la clé globale
					f.timestampMs = 0;			   // recalculé à la sortie (ordre d'affichage)
					m->h264Reorder.PushBack(traits::NkMove(f)); // f inutilisée après (évite une copie de ~1 Mo)
					m->h264ReorderKey.PushBack(m->h264GopBase + (int64)m->lastPoc);
					++m->h264DecodeCursor;
				}
			}

			// ── Chemin HEVC : réordonnancement décodage→affichage par clé POC globale ──
			// Les B (bi-prédiction) se décodent dans le désordre. On bufferise les images RGBA
			// décodées et on sort la plus petite clé d'affichage dès qu'aucune image restant à
			// décoder ne peut avoir une clé plus petite (min suffixe précalculé) — réordonnancement
			// EXACT, sans heuristique de profondeur B. `Decode` est appelé en ORDRE DÉCODAGE (donc
			// le DPB HEVC évolue correctement, comme pour H264).
			if (m->codec == Codec::HEVC) {
				const int32 count = m->info.frameCount;
				for (;;) {
					const bool noMoreInput = (m->hevcDecodeCursor >= count);
					int64 minRemaining = 0x7FFFFFFFFFFFFFFFLL;
					if (!noMoreInput)
						minRemaining = m->hevcSuffixMinKey[(uint64)m->hevcDecodeCursor];
					uint64 best = 0;
					bool canOutput = false;
					if (m->hevcReorder.Size() > 0) {
						for (uint64 k = 1; k < m->hevcReorderKey.Size(); ++k)
							if (m->hevcReorderKey[k] < m->hevcReorderKey[best])
								best = k;
						if (noMoreInput || m->hevcReorderKey[best] < minRemaining)
							canOutput = true;
					}
					if (canOutput) {
						// NkMove : `out` et les éléments décalés embarquent le buffer RGBA complet.
						out = traits::NkMove(m->hevcReorder[best]);
						for (uint64 k = best + 1; k < m->hevcReorder.Size(); ++k) {
							m->hevcReorder[k - 1] = traits::NkMove(m->hevcReorder[k]);
							m->hevcReorderKey[k - 1] = m->hevcReorderKey[k];
						}
						m->hevcReorder.Resize(m->hevcReorder.Size() - 1);
						m->hevcReorderKey.Resize(m->hevcReorderKey.Size() - 1);
						out.index = m->hevcOutCount; // ordre d'AFFICHAGE
						out.timestampMs = (m->info.fps > 0.0)
											? (int64)((double)m->hevcOutCount * 1000.0 / m->info.fps)
											: 0;
						++m->hevcOutCount;
						return true;
					}
					if (noMoreInput)
						return false; // buffer vide et plus d'entrée -> fin
					NkVideoFrame f;
					const int32 idx = m->hevcDecodeCursor;
					if (!m->Decode(idx, f)) {
						++m->hevcDecodeCursor;
						continue; // image non décodable -> on saute
					}
					f.timestampMs = 0;
					m->hevcReorder.PushBack(traits::NkMove(f));
					m->hevcReorderKey.PushBack(m->hevcGlobalKey[(uint64)idx]);
					++m->hevcDecodeCursor;
				}
			}

			// ── Autres codecs (MJPEG, RAWRGB, séquences) : ordre décodage == affichage ──
			if (m->cursor >= m->info.frameCount)
				return false;
			if (!m->Decode(m->cursor, out))
				return false;
			out.timestampMs = (m->info.fps > 0.0) ? (int64)((double)m->cursor * 1000.0 / m->info.fps) : 0;
			m->cursor++;
			return true;
		}

		bool NkVideoReader::SeekFrame(int32 index) {
			if (!IsOpen() || index < 0 || index >= mImpl->info.frameCount)
				return false;
			Impl *m = mImpl;
			// ⚠️ H264 : `cursor` seul (ci-dessous) ne veut RIEN dire pour ce codec — le chemin de
			// lecture (ReadFrame) utilise un état de réordonnancement POC séparé qu'il faut
			// repositionner explicitement, sinon SeekFrame est un no-op silencieux (bug trouvé et
			// documenté 2026-07-21, voir ROADMAP).
			if (m->codec == Codec::H264) {
				// Approximation ordre-décodage ≈ ordre-affichage (exacte aux limites de GOP ; l'écart
				// ailleurs = au plus `h264ReorderMax` B en attente, quelques images) : cherche la
				// DERNIÈRE image clé (IDR) à un index de décodage <= `index`, repart de là, puis
				// redécode en avant — coût normal pour tout lecteur H264 (distance au GOP précédent).
				int32 kf = 0;
				for (int32 i = 0; i <= index && i < (int32)m->h264Keyframe.Size(); ++i)
					if (m->h264Keyframe[(uint64)i])
						kf = i;
				m->h264DecodeCursor = kf;
				m->h264OutCount = kf; // exact à la limite de GOP (l'IDR est la 1re image affichée de son GOP)
				m->h264Reorder.Resize(0);
				m->h264ReorderKey.Resize(0);
				m->lastIsIdr = false; // redécouvert au 1er ReadFrame (décodera l'IDR -> vide h264Dpb)
			}
			if (m->codec == Codec::HEVC) {
				// Comme H264 : repart de la DERNIÈRE image IRAP (IDR/CRA/BLA) à un index de décodage
				// <= `index` (approximation ordre-décodage ≈ ordre-affichage), vide le DPB + le buffer
				// de réordonnancement, puis redécode en avant.
				int32 kf = 0;
				for (int32 i = 0; i <= index && i < (int32)m->hevcKeyframe.Size(); ++i)
					if (m->hevcKeyframe[(uint64)i])
						kf = i;
				m->hevcDecodeCursor = kf;
				m->hevcOutCount = kf; // exact à la limite de GOP (l'IRAP est la 1re image affichée)
				m->hevcReorder.Resize(0);
				m->hevcReorderKey.Resize(0);
				m->hevcDpb.Clear(); // les références du GOP précédent ne valent plus après resynchro
			}
			m->cursor = index;
			return true;
		}

		bool NkVideoReader::SelfTest() {
			// 1) Écrit un petit AVI MJPEG via NkVideoWriter (motif RGBA simple).
			const int32 W = 64, H = 48, N = 8;
			const char *path = "nkvideoreader_selftest.avi";
			{
				NkVideoWriter wr;
				NkVideoConfig cfg;
				cfg.width = W;
				cfg.height = H;
				cfg.fpsNum = 10;
				cfg.fpsDen = 1;
				cfg.codec = NkVideoCodec::MJPEG;
				cfg.container = NkVideoContainer::AVI;
				cfg.quality = 92;
				if (!wr.Open(path, cfg))
					return false;
				NkVector<nk_uint8> frame;
				frame.Resize((uint64)W * (uint64)H * 4u);
				for (int32 f = 0; f < N; ++f) {
					for (int32 i = 0; i < W * H; ++i) {
						frame[(uint64)i * 4 + 0] = (uint8)((i + f * 16) & 0xFF);
						frame[(uint64)i * 4 + 1] = (uint8)((i * 2 + f * 8) & 0xFF);
						frame[(uint64)i * 4 + 2] = (uint8)((f * 32) & 0xFF);
						frame[(uint64)i * 4 + 3] = 255;
					}
					if (!wr.WriteFrame(frame.Data(), NkVideoInputFormat::RGBA32))
						return false;
				}
				wr.Close();
			}

			// 2) Relit l'AVI et vérifie dimensions + nombre d'images + pixels plausibles.
			NkVideoReader rd;
			if (!rd.Open(path))
				return false;
			if (rd.Info().width != W || rd.Info().height != H)
				return false;
			int32 count = 0;
			bool nonZero = false;
			NkVideoFrame fr;
			while (rd.ReadFrame(fr)) {
				if (fr.width != W || fr.height != H || fr.rgba.Size() != (uint64)W * (uint64)H * 4u)
					return false;
				for (uint64 i = 0; i < fr.rgba.Size(); ++i)
					if (fr.rgba[i] != 0) {
						nonZero = true;
						break;
					}
				++count;
			}
			return count == N && nonZero;
		}

	} // namespace media
} // namespace nkentseu
