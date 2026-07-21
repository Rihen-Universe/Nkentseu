// =============================================================================
// NKMedia/Video/NkVideoReader.cpp — implémentation (AVI/MJPEG d'abord).
// =============================================================================
#include "NKMedia/Video/NkVideoReader.h"
#include "NKMedia/Video/NkVideoWriter.h" // SelfTest

#include "NKImage/Core/NkImage.h"
#include "NKImage/Codecs/JPEG/NkJPEGCodec.h"
#include "NKMedia/Codecs/Video/H264/NkH264Decoder.h"
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

			enum class Backend { NONE, AVI, MOV, WEBM, SEQUENCE };
			enum class Codec { NONE, MJPEG, RAWRGB, H264 };

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
				// (pas de décodage, juste les en-têtes NAL). Partagé par ParseMov et ParseWebm.
				void ScanH264Keyframes() {
					h264Keyframe.Resize(frames.Size());
					for (uint64 i = 0; i < frames.Size(); ++i) {
						const uint8 *s = bytes.Data() + frames[i].offset;
						const usize sz = frames[i].size;
						bool idr = false;
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
					if (!isMjpeg && !isAvc)
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

					codec = isMjpeg ? Codec::MJPEG : Codec::H264;
					info.codec = NkString(isMjpeg ? "mjpeg" : "h264");
					info.container = NkString("mov");
					info.width = w;
					info.height = h;
					info.frameCount = (int32)frames.Size();
					info.fps = fps;
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
				bool ParseWebm() {
					const uint8 *d = bytes.Data();
					const usize n = (usize)bytes.Size();
					WebmVideoTrackInfo found;
					WalkWebmTracks(d, 0, n, &found, nullptr, 0);
					if (found.num < 0 || found.codecId.Empty())
						return false;
					if (!found.codecId.Contains("AVC") && !found.codecId.Contains("MPEG4"))
						return false; // VP8/VP9/AV1 etc. : pas de décodeur -> échec propre
					if (found.codecPriv.Size() < 7)
						return false;
					ParseAvcCBytes(found.codecPriv.Data(), (usize)found.codecPriv.Size());
					if (h264Sps.Size() == 0 || h264Pps.Size() == 0)
						return false;
					NkVector<int64> ts;
					WalkWebmVideoClusters(d, 0, n, found.num, 0, frames, ts, 0);
					if (frames.Size() == 0)
						return false;
					// fps dérivé des horodatages RÉELS des blocs (EBML n'a pas d'équivalent direct à
					// `stts` de l'ISOBMFF) ; repli 25 si dégénéré (un seul paquet, horodatages égaux…).
					double fps = 25.0;
					if (ts.Size() >= 2 && ts[ts.Size() - 1] > ts[0])
						fps = 1000.0 * (double)(ts.Size() - 1) / (double)(ts[ts.Size() - 1] - ts[0]);
					codec = Codec::H264;
					info.codec = NkString("h264");
					info.container = NkString("webm");
					info.width = found.width;
					info.height = found.height;
					info.frameCount = (int32)frames.Size();
					info.fps = fps;
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
						if (h264Sps.Size() == 0 || h264Pps.Size() == 0)
							return false;
						// Construit un flux Annex-B : SPS + PPS + NAL du sample (AVCC len-prefixé -> start codes).
						NkVector<nk_uint8> ab;
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
						bool isIdr = false;
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
						if (!NkH264Decoder::DecodeFrame(ab.Data(), (usize)ab.Size(), refs.Data(), (int32)refs.Size(), f))
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
