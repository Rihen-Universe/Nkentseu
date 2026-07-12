// =============================================================================
// NKMedia/NkMediaDemux.cpp — extraction des paquets audio (voir .h).
// MP4 : tables stbl (stsz/stco/co64/stsc/stts + mdhd). WebM : SimpleBlock/Cluster.
// =============================================================================
#include "NKMedia/NkMediaDemux.h"
#include "NKFileSystem/NkFile.h"

namespace nkentseu {
	namespace media {

		namespace {

			bool Tag4(const uint8 *p, const char *t) {
				return p[0] == (uint8)t[0] && p[1] == (uint8)t[1] && p[2] == (uint8)t[2] && p[3] == (uint8)t[3];
			}
			uint32 U32BE(const uint8 *p) {
				return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | ((uint32)p[2] << 8) | (uint32)p[3];
			}
			uint64 U64BE(const uint8 *p) {
				return ((uint64)U32BE(p) << 32) | (uint64)U32BE(p + 4);
			}

			// ---------------------------------------------------------------- MP4
			struct Mp4Tables {
					const uint8 *stsz = nullptr;
					usize stszLen = 0;
					const uint8 *stco = nullptr;
					usize stcoLen = 0;
					bool co64 = false;
					const uint8 *stsc = nullptr;
					usize stscLen = 0;
					const uint8 *stts = nullptr;
					usize sttsLen = 0;
					const uint8 *mdhd = nullptr;
					usize mdhdLen = 0;
					uint32 trackId = 0; // tkhd (pour le fMP4 fragmenté)
					bool found = false;
			};

			// 0 = inconnu, 1 = audio, 2 = video (handler courant du trak).
			void WalkMp4(const uint8 *base, usize start, usize end, int32 *handler, Mp4Tables *tab, Mp4Tables &result,
						 int32 depth) {
				if (depth > 8)
					return;
				usize pos = start;
				while (pos + 8 <= end) {
					uint64 size = (uint64)U32BE(base + pos);
					usize hdr = 8;
					if (size == 1) {
						if (pos + 16 > end)
							break;
						size = U64BE(base + pos + 8);
						hdr = 16;
					} else if (size == 0) {
						size = (uint64)(end - pos);
					}
					if (size < hdr || pos + (usize)size > end)
						break;
					const uint8 *type = base + pos + 4;
					const usize ps = pos + hdr;
					const usize pe = pos + (usize)size;

					if (Tag4(type, "moov") || Tag4(type, "udta")) {
						WalkMp4(base, ps, pe, handler, tab, result, depth + 1);
					} else if (Tag4(type, "trak")) {
						int32 h = 0;
						Mp4Tables t;
						WalkMp4(base, ps, pe, &h, &t, result, depth + 1);
						if (h == 1 && !result.found) {
							t.found = true;
							result = t;
						}
					} else if (Tag4(type, "mdia") || Tag4(type, "minf") || Tag4(type, "stbl")) {
						WalkMp4(base, ps, pe, handler, tab, result, depth + 1);
					} else if (Tag4(type, "hdlr") && handler != nullptr) {
						if (pe - ps >= 12) {
							const uint8 *h = base + ps + 8;
							if (Tag4(h, "soun"))
								*handler = 1;
							else if (Tag4(h, "vide"))
								*handler = 2;
						}
					} else if (tab != nullptr) {
						if (Tag4(type, "tkhd")) {
							if (pe - ps >= 24) {
								const uint8 ver = base[ps];
								tab->trackId = (ver == 1) ? U32BE(base + ps + 20) : U32BE(base + ps + 12);
							}
						} else if (Tag4(type, "stsz")) {
							tab->stsz = base + ps;
							tab->stszLen = pe - ps;
						} else if (Tag4(type, "stco")) {
							tab->stco = base + ps;
							tab->stcoLen = pe - ps;
							tab->co64 = false;
						} else if (Tag4(type, "co64")) {
							tab->stco = base + ps;
							tab->stcoLen = pe - ps;
							tab->co64 = true;
						} else if (Tag4(type, "stsc")) {
							tab->stsc = base + ps;
							tab->stscLen = pe - ps;
						} else if (Tag4(type, "stts")) {
							tab->stts = base + ps;
							tab->sttsLen = pe - ps;
						} else if (Tag4(type, "mdhd")) {
							tab->mdhd = base + ps;
							tab->mdhdLen = pe - ps;
						}
					}
					pos = pe;
				}
			}

			uint32 U24BE(const uint8 *p) {
				return ((uint32)p[0] << 16) | ((uint32)p[1] << 8) | (uint32)p[2];
			}

			// fMP4 : parcourt les moof/traf/trun pour la piste `trackId` audio.
			bool DemuxMp4Fragmented(const uint8 *base, usize size, uint32 trackId, uint32 timescale,
									NkVector<NkMediaPacket> &out) {
				out.Clear();
				uint64 tick = 0;
				usize pos = 0;
				while (pos + 8 <= size) {
					uint64 bsz = (uint64)U32BE(base + pos);
					usize hdr = 8;
					if (bsz == 1) {
						if (pos + 16 > size)
							break;
						bsz = U64BE(base + pos + 8);
						hdr = 16;
					} else if (bsz == 0) {
						bsz = (uint64)(size - pos);
					}
					if (bsz < hdr || pos + (usize)bsz > size)
						break;
					if (Tag4(base + pos + 4, "moof")) {
						const usize moofStart = pos;
						const usize moofEnd = pos + (usize)bsz;
						// parcourt traf.
						usize p = pos + hdr;
						while (p + 8 <= moofEnd) {
							uint64 s2 = (uint64)U32BE(base + p);
							usize h2 = 8;
							if (s2 == 1) {
								s2 = U64BE(base + p + 8);
								h2 = 16;
							} else if (s2 == 0)
								s2 = (uint64)(moofEnd - p);
							if (s2 < h2 || p + (usize)s2 > moofEnd)
								break;
							if (Tag4(base + p + 4, "traf")) {
								// lit tfhd + truns.
								uint32 tfTrack = 0, defSize = 0, defDur = 0;
								uint64 baseOff = moofStart;
								bool haveBase = false;
								const usize trafStart = p + h2, trafEnd = p + (usize)s2;
								usize q = trafStart;
								// 1re passe : tfhd.
								while (q + 8 <= trafEnd) {
									uint64 s3 = (uint64)U32BE(base + q);
									usize h3 = 8;
									if (s3 == 1) {
										s3 = U64BE(base + q + 8);
										h3 = 16;
									} else if (s3 == 0)
										s3 = (uint64)(trafEnd - q);
									if (s3 < h3 || q + (usize)s3 > trafEnd)
										break;
									if (Tag4(base + q + 4, "tfhd")) {
										const uint8 *pl = base + q + h3;
										const uint32 flags = U24BE(pl + 1);
										tfTrack = U32BE(pl + 4);
										usize o = 8;
										if (flags & 0x000001) {
											baseOff = U64BE(pl + o);
											haveBase = true;
											o += 8;
										}
										if (flags & 0x000002)
											o += 4;
										if (flags & 0x000008) {
											defDur = U32BE(pl + o);
											o += 4;
										}
										if (flags & 0x000010) {
											defSize = U32BE(pl + o);
											o += 4;
										}
										if (!haveBase) // default-base-is-moof (0x020000) ou fallback navigateur
											baseOff = moofStart;
									}
									q += s3;
								}
								if (tfTrack == trackId) {
									// 2e passe : truns.
									q = trafStart;
									while (q + 8 <= trafEnd) {
										uint64 s3 = (uint64)U32BE(base + q);
										usize h3 = 8;
										if (s3 == 1) {
											s3 = U64BE(base + q + 8);
											h3 = 16;
										} else if (s3 == 0)
											s3 = (uint64)(trafEnd - q);
										if (s3 < h3 || q + (usize)s3 > trafEnd)
											break;
										if (Tag4(base + q + 4, "trun")) {
											const uint8 *pl = base + q + h3;
											const uint32 flags = U24BE(pl + 1);
											const uint32 count = U32BE(pl + 4);
											usize o = 8;
											int32 dataOff = 0;
											if (flags & 0x000001) {
												dataOff = (int32)U32BE(pl + o);
												o += 4;
											}
											if (flags & 0x000004)
												o += 4;
											uint64 sampleOff = baseOff + (uint64)(int64)dataOff;
											for (uint32 i = 0; i < count; ++i) {
												uint32 dur = defDur, sz = defSize;
												if (flags & 0x000100) {
													dur = U32BE(pl + o);
													o += 4;
												}
												if (flags & 0x000200) {
													sz = U32BE(pl + o);
													o += 4;
												}
												if (flags & 0x000400)
													o += 4;
												if (flags & 0x000800)
													o += 4;
												if (sz == 0 || sampleOff + sz > (uint64)size)
													break;
												NkMediaPacket pk;
												pk.offset = (usize)sampleOff;
												pk.size = (usize)sz;
												pk.timestampMs = (int64)(tick * 1000ULL / (uint64)timescale);
												out.PushBack(pk);
												sampleOff += sz;
												tick += dur;
											}
										}
										q += s3;
									}
								}
							}
							p += s2;
						}
					}
					pos += (usize)bsz;
				}
				return out.Size() > 0;
			}

			bool DemuxMp4(const uint8 *base, usize size, NkVector<NkMediaPacket> &out) {
				Mp4Tables t;
				int32 h = 0;
				WalkMp4(base, 0, size, &h, nullptr, t, 0);
				if (!t.found)
					return false;

				// Timescale (mdhd) — utile aussi pour le fMP4.
				uint32 ts0 = 1000;
				if (t.mdhd && t.mdhdLen >= 24) {
					const uint8 ver = t.mdhd[0];
					ts0 = (ver == 1) ? U32BE(t.mdhd + 20) : U32BE(t.mdhd + 12);
					if (ts0 == 0)
						ts0 = 1000;
				}

				// stbl classique manquant/vide → tenter le fMP4 (moof/traf/trun).
				bool classicOk = t.stsz && t.stco && t.stsc && U32BE(t.stsz + 8) > 0;
				if (!classicOk)
					return DemuxMp4Fragmented(base, size, t.trackId, ts0, out);

				// mdhd : timescale.
				uint32 timescale = 1000;
				if (t.mdhd && t.mdhdLen >= 24) {
					const uint8 ver = t.mdhd[0];
					timescale = (ver == 1) ? U32BE(t.mdhd + 20) : U32BE(t.mdhd + 12);
					if (timescale == 0)
						timescale = 1000;
				}

				// stsz.
				if (t.stszLen < 12)
					return false;
				const uint32 sampleSize = U32BE(t.stsz + 4);
				const uint32 sampleCount = U32BE(t.stsz + 8);
				if (sampleCount == 0)
					return false;
				auto SzAt = [&](uint32 i) -> uint32 {
					if (sampleSize != 0)
						return sampleSize;
					const usize o = 12 + (usize)i * 4;
					return (o + 4 <= t.stszLen) ? U32BE(t.stsz + o) : 0;
				};

				// stco / co64.
				if (t.stcoLen < 8)
					return false;
				const uint32 numChunks = U32BE(t.stco + 4);
				auto ChunkOff = [&](uint32 c) -> uint64 {
					if (t.co64) {
						const usize o = 8 + (usize)c * 8;
						return (o + 8 <= t.stcoLen) ? U64BE(t.stco + o) : 0;
					}
					const usize o = 8 + (usize)c * 4;
					return (o + 4 <= t.stcoLen) ? (uint64)U32BE(t.stco + o) : 0;
				};

				// stsc : (first_chunk, samples_per_chunk, sdi) — samples par chunk c (1-based).
				if (t.stscLen < 8)
					return false;
				const uint32 stscCount = U32BE(t.stsc + 4);
				auto SamplesPerChunk = [&](uint32 chunk1) -> uint32 { // chunk1 = 1-based
					uint32 spc = 0;
					for (uint32 i = 0; i < stscCount; ++i) {
						const usize o = 8 + (usize)i * 12;
						if (o + 12 > t.stscLen)
							break;
						const uint32 first = U32BE(t.stsc + o);
						if (first <= chunk1)
							spc = U32BE(t.stsc + o + 4);
						else
							break;
					}
					return spc;
				};

				// stts : durées (ticks) par sample, pour l'horodatage.
				const uint32 sttsCount = t.stts && t.sttsLen >= 8 ? U32BE(t.stts + 4) : 0;

				out.Clear();
				uint32 sampleIdx = 0;
				uint64 tick = 0;
				uint32 sttsEntry = 0, sttsRem = 0, sttsDelta = 0;
				auto NextDelta = [&]() -> uint32 {
					if (sttsCount == 0)
						return 0;
					while (sttsRem == 0 && sttsEntry < sttsCount) {
						const usize o = 8 + (usize)sttsEntry * 8;
						if (o + 8 > t.sttsLen)
							return 0;
						sttsRem = U32BE(t.stts + o);
						sttsDelta = U32BE(t.stts + o + 4);
						++sttsEntry;
						if (sttsRem == 0)
							continue;
					}
					if (sttsRem > 0) {
						--sttsRem;
						return sttsDelta;
					}
					return 0;
				};

				for (uint32 c = 0; c < numChunks && sampleIdx < sampleCount; ++c) {
					const uint32 spc = SamplesPerChunk(c + 1);
					uint64 off = ChunkOff(c);
					for (uint32 s = 0; s < spc && sampleIdx < sampleCount; ++s) {
						const uint32 sz = SzAt(sampleIdx);
						if (off + sz > (uint64)size)
							break;
						NkMediaPacket pk;
						pk.offset = (usize)off;
						pk.size = (usize)sz;
						pk.timestampMs = (int64)(tick * 1000ULL / (uint64)timescale);
						out.PushBack(pk);
						off += sz;
						tick += NextDelta();
						++sampleIdx;
					}
				}
				return out.Size() > 0;
			}

			// --------------------------------------------------------------- WebM
			int32 ReadVint(const uint8 *p, usize avail, uint64 &value, bool keepMarker) {
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
				uint64 v = keepMarker ? (uint64)b : (uint64)(b & (0xFF >> len));
				for (int32 i = 1; i < len; ++i)
					v = (v << 8) | p[i];
				value = v;
				return len;
			}
			uint64 EbmlUint(const uint8 *p, usize len) {
				uint64 v = 0;
				for (usize i = 0; i < len && i < 8; ++i)
					v = (v << 8) | p[i];
				return v;
			}

			// Trouve le TrackNumber de la 1re piste audio (TrackType==2).
			void FindAudioTrack(const uint8 *base, usize start, usize end, int64 &audioNum, int32 &curType,
								int64 &curNum, int32 depth) {
				if (depth > 10)
					return;
				usize pos = start;
				while (pos < end && audioNum < 0) {
					uint64 id = 0, sz = 0;
					int32 il = ReadVint(base + pos, end - pos, id, true);
					if (il <= 0)
						break;
					int32 sl = ReadVint(base + pos + il, end - pos - il, sz, false);
					if (sl <= 0)
						break;
					const usize ds = pos + il + sl;
					uint64 allOnes = (1ULL << (7 * sl)) - 1;
					usize de = (sz == allOnes || ds + (usize)sz > end) ? end : ds + (usize)sz;

					if (id == 0x18538067ULL || id == 0x1654AE6BULL) { // Segment, Tracks
						FindAudioTrack(base, ds, de, audioNum, curType, curNum, depth + 1);
					} else if (id == 0xAEULL) { // TrackEntry
						int32 tt = 0;
						int64 tn = -1;
						FindAudioTrack(base, ds, de, audioNum, tt, tn, depth + 1);
						if (tt == 2 && tn >= 0 && audioNum < 0)
							audioNum = tn;
					} else if (id == 0x83ULL) { // TrackType
						curType = (int32)EbmlUint(base + ds, de - ds);
					} else if (id == 0xD7ULL) { // TrackNumber
						curNum = (int64)EbmlUint(base + ds, de - ds);
					}
					pos = de;
				}
			}

			// Parcourt les Clusters et sort les SimpleBlock/Block de la piste audio.
			void WalkClusters(const uint8 *base, usize start, usize end, int64 audioNum, uint64 clusterTs,
							  NkVector<NkMediaPacket> &out, int32 depth) {
				if (depth > 10)
					return;
				usize pos = start;
				uint64 curClusterTs = clusterTs;
				while (pos < end) {
					uint64 id = 0, sz = 0;
					int32 il = ReadVint(base + pos, end - pos, id, true);
					if (il <= 0)
						break;
					int32 sl = ReadVint(base + pos + il, end - pos - il, sz, false);
					if (sl <= 0)
						break;
					const usize ds = pos + il + sl;
					uint64 allOnes = (1ULL << (7 * sl)) - 1;
					usize de = (sz == allOnes || ds + (usize)sz > end) ? end : ds + (usize)sz;

					if (id == 0x18538067ULL) { // Segment
						WalkClusters(base, ds, de, audioNum, curClusterTs, out, depth + 1);
					} else if (id == 0x1F43B675ULL) { // Cluster
						WalkClusters(base, ds, de, audioNum, 0, out, depth + 1);
					} else if (id == 0xE7ULL) { // Timestamp (du cluster)
						curClusterTs = EbmlUint(base + ds, de - ds);
					} else if (id == 0xA0ULL) { // BlockGroup
						WalkClusters(base, ds, de, audioNum, curClusterTs, out, depth + 1);
					} else if (id == 0xA3ULL || id == 0xA1ULL) { // SimpleBlock / Block
						uint64 trackNum = 0;
						int32 tl = ReadVint(base + ds, de - ds, trackNum, false);
						if (tl > 0 && ds + (usize)tl + 3 <= de && (int64)trackNum == audioNum) {
							const usize hp = ds + (usize)tl;
							const int16 rel = (int16)(((uint16)base[hp] << 8) | (uint16)base[hp + 1]);
							const uint8 flags = base[hp + 2];
							const usize frameStart = hp + 3;
							const int32 lacing = (flags >> 1) & 0x03;
							if (frameStart <= de) {
								NkMediaPacket pk;
								pk.offset = frameStart;
								pk.size = de - frameStart; // sans lacing = 1 frame (cas Opus/WebM)
								pk.timestampMs = (int64)curClusterTs + (int64)rel;
								if (lacing == 0 && pk.size > 0)
									out.PushBack(pk);
								else if (pk.size > 0)
									out.PushBack(pk); // laced : v1 = bloc entier (approx), à raffiner
							}
						}
					}
					pos = de;
				}
			}

			// ---------------------------------------------------------------
			// OGG — reconstruction des paquets du premier flux audio.
			// Une page = "OggS" + version + headerType + granule(le64) +
			// serial(le32) + seq + crc + nsegs + table de lacing, puis le
			// payload (segments contigus). Un lacing < 255 termine un paquet.
			// Les paquets renvoyés incluent les EN-TÊTES du codec (OpusHead/
			// OpusTags ou headers Vorbis) : le décodeur aval les consomme.
			// Limite V1 : un paquet à cheval sur deux pages (continuation)
			// n'est pas contigu dans le buffer source → il est ignoré (rare :
			// il faudrait un paquet audio > ~64 Ko).
			// ---------------------------------------------------------------

			inline uint32 OggU32LE(const uint8 *p) {
				return (uint32)p[0] | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24);
			}

			inline uint64 OggU64LE(const uint8 *p) {
				return (uint64)OggU32LE(p) | ((uint64)OggU32LE(p + 4) << 32);
			}

			struct OggPage {
					uint8 headerType = 0;
					int64 granule = -1;
					uint32 serial = 0;
					usize payloadOffset = 0; // offset absolu du payload dans le buffer
					usize payloadSize = 0;
					uint8 nsegs = 0;
					const uint8 *segTable = nullptr;
					usize next = 0; // offset absolu de la page suivante
			};

			// Parse la page à `pos`. Renvoie false si en-tête invalide/tronqué.
			bool OggParsePage(const uint8 *base, usize size, usize pos, OggPage &pg) {
				if (pos + 27 > size)
					return false;
				const uint8 *h = base + pos;
				if (h[0] != 'O' || h[1] != 'g' || h[2] != 'g' || h[3] != 'S' || h[4] != 0)
					return false;
				pg.headerType = h[5];
				pg.granule = (int64)OggU64LE(h + 6);
				pg.serial = OggU32LE(h + 14);
				pg.nsegs = h[26];
				if (pos + 27 + pg.nsegs > size)
					return false;
				pg.segTable = h + 27;
				pg.payloadOffset = pos + 27 + pg.nsegs;
				usize payload = 0;
				for (uint8 i = 0; i < pg.nsegs; ++i)
					payload += pg.segTable[i];
				if (pg.payloadOffset + payload > size)
					return false;
				pg.payloadSize = payload;
				pg.next = pg.payloadOffset + payload;
				return true;
			}

			bool DemuxOgg(const uint8 *base, usize size, NkVector<NkMediaPacket> &out) {
				out.Clear();

				// 1) Choix du flux : premier BOS dont le payload commence par
				//    "OpusHead" ; à défaut, premier BOS audio-compatible.
				uint32 serial = 0;
				bool haveSerial = false;
				bool haveFallback = false;
				uint32 fallbackSerial = 0;
				usize pos = 0;
				OggPage pg;
				while (pos < size && OggParsePage(base, size, pos, pg)) {
					if ((pg.headerType & 0x02) != 0 && pg.payloadSize >= 8) {
						const uint8 *pl = base + pg.payloadOffset;
						if (pl[0] == 'O' && pl[1] == 'p' && pl[2] == 'u' && pl[3] == 's' && pl[4] == 'H' &&
							pl[5] == 'e' && pl[6] == 'a' && pl[7] == 'd') {
							serial = pg.serial;
							haveSerial = true;
							break;
						}
						if (!haveFallback) {
							fallbackSerial = pg.serial;
							haveFallback = true;
						}
					} else if ((pg.headerType & 0x02) == 0) {
						break; // fin de la zone BOS
					}
					pos = pg.next;
				}
				if (!haveSerial) {
					if (!haveFallback)
						return false;
					serial = fallbackSerial;
				}

				// 2) Reconstruction des paquets du flux choisi.
				usize pktStart = 0;
				usize pktSize = 0;
				bool pktOpen = false;
				bool pktBroken = false; // paquet démarré sur une page précédente (non contigu)
				pos = 0;
				while (pos < size && OggParsePage(base, size, pos, pg)) {
					if (pg.serial != serial) {
						pos = pg.next;
						continue;
					}
					// Continuation d'un paquet de la page précédente : le
					// payload n'est pas contigu (en-tête de page au milieu) —
					// on marque le paquet comme perdu (limite V1 documentée).
					if (pktOpen && (pg.headerType & 0x01) != 0) {
						pktBroken = true;
					} else if (!pktOpen && (pg.headerType & 0x01) != 0) {
						// Continuation orpheline : sauter les segments jusqu'à
						// la fin du paquet en cours.
						pktBroken = true;
						pktOpen = true;
						pktStart = pg.payloadOffset;
						pktSize = 0;
					}

					usize segPos = pg.payloadOffset;
					int32 lastCompleteInPage = -1;
					for (uint8 i = 0; i < pg.nsegs; ++i) {
						const uint8 lace = pg.segTable[i];
						if (!pktOpen) {
							pktOpen = true;
							pktBroken = false;
							pktStart = segPos;
							pktSize = 0;
						}
						pktSize += lace;
						segPos += lace;
						if (lace < 255) {
							// Fin de paquet.
							if (!pktBroken && pktSize > 0) {
								NkMediaPacket pk;
								pk.offset = pktStart;
								pk.size = pktSize;
								pk.timestampMs = 0;
								pk.granule = -1;
								out.PushBack(pk);
								lastCompleteInPage = (int32)out.Size() - 1;
							}
							pktOpen = false;
							pktBroken = false;
						}
					}
					// granulepos de la page = position (échantillons) de fin du
					// dernier paquet COMPLET de la page.
					if (lastCompleteInPage >= 0 && pg.granule >= 0)
						out[(usize)lastCompleteInPage].granule = pg.granule;

					pos = pg.next;
				}
				return out.Size() > 0;
			}

			bool DemuxWebm(const uint8 *base, usize size, NkVector<NkMediaPacket> &out) {
				int64 audioNum = -1;
				int32 t = 0;
				int64 n = -1;
				FindAudioTrack(base, 0, size, audioNum, t, n, 0);
				if (audioNum < 0)
					return false;
				out.Clear();
				WalkClusters(base, 0, size, audioNum, 0, out, 0);
				return out.Size() > 0;
			}

		} // namespace

		bool NkMediaDemux::ExtractAudioPackets(const uint8 *data, usize size, const NkMediaInfo &info,
											   NkVector<NkMediaPacket> &out) {
			if (data == nullptr || size == 0)
				return false;
			switch (info.container) {
				case NkMediaContainer::NK_MP4:
					return DemuxMp4(data, size, out);
				case NkMediaContainer::NK_WEBM:
					return DemuxWebm(data, size, out);
				case NkMediaContainer::NK_OGG:
					// Paquets du premier flux (Opus prioritaire), EN-TÊTES de
					// codec inclus (OpusHead/OpusTags en paquets 0 et 1).
					return DemuxOgg(data, size, out);
				default:
					return false; // WAV/MP3/FLAC : lecture via NKAudio existant
			}
		}

		bool NkMediaDemux::ExtractAudioPacketsFile(const char *path, NkVector<nk_uint8> &outBytes,
												   NkMediaInfo &outInfo, NkVector<NkMediaPacket> &out) {
			outBytes = NkFile::ReadAllBytes(path);
			if (outBytes.Size() == 0)
				return false;
			if (!NkMediaProbe::Probe(outBytes.Data(), (usize)outBytes.Size(), outInfo))
				return false;
			return ExtractAudioPackets(outBytes.Data(), (usize)outBytes.Size(), outInfo, out);
		}

		bool NkMediaDemux::SelfTest() {
			bool ok = true;
			// Vint : longueurs/valeurs connues (mêmes helpers que le démux WebM).
			{
				uint8 v[2] = {0x40, 0x02};
				uint64 val = 0;
				if (ReadVint(v, 2, val, false) != 2 || val != 2)
					ok = false;
				uint8 s[1] = {0x81};
				if (ReadVint(s, 1, val, false) != 1 || val != 1)
					ok = false;
			}
			// U32BE / U64BE.
			{
				uint8 b[8] = {0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02};
				if (U32BE(b) != 256u)
					ok = false;
				if (U64BE(b) != ((256ULL << 32) | 2ULL))
					ok = false;
			}
			return ok;
		}

	} // namespace media
} // namespace nkentseu
