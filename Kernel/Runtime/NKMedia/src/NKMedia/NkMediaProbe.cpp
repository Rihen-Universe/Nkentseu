// =============================================================================
// NKMedia/NkMediaProbe.cpp — détection conteneur + démux d'en-tête (voir .h).
// Parseurs from-scratch : ISOBMFF (MP4) + EBML (Matroska/WebM).
// =============================================================================
#include "NKMedia/NkMediaProbe.h"
#include "NKMemory/NKMemory.h"
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
			uint16 U16BE(const uint8 *p) {
				return (uint16)(((uint16)p[0] << 8) | (uint16)p[1]);
			}
			uint64 U64BE(const uint8 *p) {
				return ((uint64)U32BE(p) << 32) | (uint64)U32BE(p + 4);
			}

			// ---- Codec fourcc/id → nom court ----
			NkString Mp4Codec(const uint8 *fourcc) {
				if (Tag4(fourcc, "mp4a"))
					return NkString("aac");
				if (Tag4(fourcc, "Opus") || Tag4(fourcc, "opus"))
					return NkString("opus");
				if (Tag4(fourcc, "avc1") || Tag4(fourcc, "avc3"))
					return NkString("h264");
				if (Tag4(fourcc, "hvc1") || Tag4(fourcc, "hev1"))
					return NkString("h265");
				if (Tag4(fourcc, "vp08"))
					return NkString("vp8");
				if (Tag4(fourcc, "vp09"))
					return NkString("vp9");
				if (Tag4(fourcc, "mp4v"))
					return NkString("mpeg4");
				if (Tag4(fourcc, ".mp3") || Tag4(fourcc, "mp3 "))
					return NkString("mp3");
				if (Tag4(fourcc, "twos") || Tag4(fourcc, "sowt") || Tag4(fourcc, "lpcm"))
					return NkString("pcm");
				char b[5] = {(char)fourcc[0], (char)fourcc[1], (char)fourcc[2], (char)fourcc[3], 0};
				return NkString(b);
			}

			// =====================================================================
			//  ISOBMFF (MP4)
			// =====================================================================
			void ParseStsd(const uint8 *p, usize len, NkMediaTrackType tt, NkMediaInfo &info) {
				if (len < 8)
					return;
				// version/flags (4) + entry_count (4), puis 1re sample entry : [size u32][fourcc 4].
				usize off = 8;
				if (off + 8 > len)
					return;
				const uint8 *entry = p + off;
				const uint8 *fourcc = entry + 4;
				NkMediaTrack tr;
				tr.type = tt;
				tr.codec = Mp4Codec(fourcc);
				// Champs spécifiques : audio (mp4a-like) ou vidéo (avc1-like).
				const usize eLen = (usize)U32BE(entry);
				const uint8 *body = entry + 8; // après size+fourcc
				const usize bodyLen = (eLen >= 8 && off + eLen <= len) ? (eLen - 8) : (len - off - 8);
				if (tt == NkMediaTrackType::NK_AUDIO && bodyLen >= 20) {
					// 6 réservés + 2 data_ref + 8 (version/rev/vendor) + 2 channelcount + 2 samplesize
					// + 2 predef + 2 reserved + 4 samplerate(16.16).
					tr.channels = (int32)U16BE(body + 16);
					tr.sampleRate = (int32)U16BE(body + 24); // partie entière du 16.16
				} else if (tt == NkMediaTrackType::NK_VIDEO && bodyLen >= 32) {
					// 6 réservés + 2 data_ref + 16 (predef/reserved) + 2 width + 2 height.
					tr.width = (int32)U16BE(body + 24);
					tr.height = (int32)U16BE(body + 26);
				}
				info.tracks.PushBack(tr);
			}

			// Walk récursif des boîtes. `tt` = handler courant de la piste (null hors trak).
			void WalkMp4(const uint8 *base, usize start, usize end, NkMediaTrackType *tt, NkMediaInfo &info,
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
					const usize pstart = pos + hdr;
					const usize pend = pos + (usize)size;

					if (Tag4(type, "moov") || Tag4(type, "udta")) {
						WalkMp4(base, pstart, pend, tt, info, depth + 1);
					} else if (Tag4(type, "trak")) {
						NkMediaTrackType local = NkMediaTrackType::NK_UNKNOWN;
						WalkMp4(base, pstart, pend, &local, info, depth + 1);
					} else if (Tag4(type, "mdia") || Tag4(type, "minf") || Tag4(type, "stbl")) {
						WalkMp4(base, pstart, pend, tt, info, depth + 1);
					} else if (Tag4(type, "hdlr") && tt != nullptr) {
						// version/flags (4) + predefined (4) + handler_type (4).
						if (pend - pstart >= 12) {
							const uint8 *h = base + pstart + 8;
							if (Tag4(h, "soun"))
								*tt = NkMediaTrackType::NK_AUDIO;
							else if (Tag4(h, "vide"))
								*tt = NkMediaTrackType::NK_VIDEO;
						}
					} else if (Tag4(type, "stsd") && tt != nullptr) {
						ParseStsd(base + pstart, pend - pstart, *tt, info);
					}
					pos = pend;
				}
			}

			// =====================================================================
			//  EBML (Matroska / WebM)
			// =====================================================================
			// Lit un vint. `keepMarker`=true pour les IDs (garde les bits de longueur),
			// false pour les tailles (retire le bit marqueur). Renvoie len (octets) ou 0.
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
				uint64 v;
				if (keepMarker) {
					v = b;
				} else {
					v = (uint64)(b & (0xFF >> len)); // retire le bit marqueur
				}
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
			double EbmlFloat(const uint8 *p, usize len) {
				if (len == 4) {
					uint32 u = U32BE(p);
					float f;
					// copie bit-à-bit
					uint8 *dst = (uint8 *)&f;
					dst[0] = (uint8)(u & 0xFF);
					dst[1] = (uint8)((u >> 8) & 0xFF);
					dst[2] = (uint8)((u >> 16) & 0xFF);
					dst[3] = (uint8)((u >> 24) & 0xFF);
					return (double)f;
				}
				if (len == 8) {
					uint64 u = U64BE(p);
					double d;
					uint8 *dst = (uint8 *)&d;
					for (int32 i = 0; i < 8; ++i)
						dst[i] = (uint8)((u >> (8 * i)) & 0xFF);
					return d;
				}
				return 0.0;
			}

			NkString WebmCodec(const uint8 *p, usize len) {
				char buf[32];
				usize n = len < 31 ? len : 31;
				for (usize i = 0; i < n; ++i)
					buf[i] = (char)p[i];
				buf[n] = 0;
				NkString id(buf);
				if (id == NkString("A_OPUS"))
					return NkString("opus");
				if (id == NkString("A_VORBIS"))
					return NkString("vorbis");
				if (id.Contains("A_AAC"))
					return NkString("aac");
				if (id.Contains("A_PCM"))
					return NkString("pcm");
				if (id == NkString("A_MPEG/L3"))
					return NkString("mp3");
				if (id == NkString("V_VP8"))
					return NkString("vp8");
				if (id == NkString("V_VP9"))
					return NkString("vp9");
				if (id.Contains("V_MPEG4") || id.Contains("AVC"))
					return NkString("h264");
				return id;
			}

			// Parcourt un conteneur EBML. Récursif dans les éléments maîtres pertinents.
			void WalkEbml(const uint8 *base, usize start, usize end, NkMediaInfo &info, NkMediaTrack *cur,
						  int32 depth) {
				if (depth > 10)
					return;
				usize pos = start;
				while (pos < end) {
					uint64 id = 0, size = 0;
					int32 idl = ReadVint(base + pos, end - pos, id, true);
					if (idl <= 0)
						break;
					int32 sl = ReadVint(base + pos + idl, end - pos - idl, size, false);
					if (sl <= 0)
						break;
					const usize dstart = pos + idl + sl;
					// Taille inconnue (tous bits à 1) → jusqu'à la fin (Segment souvent).
					uint64 allOnes = (1ULL << (7 * sl)) - 1;
					usize dend;
					if (size == allOnes || dstart + (usize)size > end)
						dend = end;
					else
						dend = dstart + (usize)size;

					if (id == 0x18538067ULL || id == 0x1654AE6BULL) { // Segment, Tracks
						WalkEbml(base, dstart, dend, info, nullptr, depth + 1);
					} else if (id == 0xAEULL) { // TrackEntry
						NkMediaTrack tr;
						WalkEbml(base, dstart, dend, info, &tr, depth + 1);
						info.tracks.PushBack(tr);
					} else if (id == 0xE1ULL || id == 0xE0ULL) { // Audio / Video (sous TrackEntry)
						WalkEbml(base, dstart, dend, info, cur, depth + 1);
					} else if (cur != nullptr) {
						const uint8 *d = base + dstart;
						const usize dl = dend - dstart;
						if (id == 0x83ULL) { // TrackType
							const uint64 t = EbmlUint(d, dl);
							cur->type = (t == 2) ? NkMediaTrackType::NK_AUDIO
												 : (t == 1) ? NkMediaTrackType::NK_VIDEO : NkMediaTrackType::NK_UNKNOWN;
						} else if (id == 0x86ULL) { // CodecID
							cur->codec = WebmCodec(d, dl);
						} else if (id == 0xB5ULL) { // SamplingFrequency (float)
							cur->sampleRate = (int32)EbmlFloat(d, dl);
						} else if (id == 0x9FULL) { // Channels
							cur->channels = (int32)EbmlUint(d, dl);
						} else if (id == 0xB0ULL) { // PixelWidth
							cur->width = (int32)EbmlUint(d, dl);
						} else if (id == 0xBAULL) { // PixelHeight
							cur->height = (int32)EbmlUint(d, dl);
						}
					}
					pos = dend;
				}
			}

		} // namespace

		const char *NkMediaInfo::ContainerName() const {
			switch (container) {
				case NkMediaContainer::NK_MP4:
					return "MP4";
				case NkMediaContainer::NK_WEBM:
					return "WebM";
				case NkMediaContainer::NK_WAV:
					return "WAV";
				case NkMediaContainer::NK_OGG:
					return "OGG";
				case NkMediaContainer::NK_MP3:
					return "MP3";
				case NkMediaContainer::NK_FLAC:
					return "FLAC";
				default:
					return "Unknown";
			}
		}

		const NkMediaTrack *NkMediaInfo::FirstAudio() const {
			for (uint64 i = 0; i < tracks.Size(); ++i)
				if (tracks[i].type == NkMediaTrackType::NK_AUDIO)
					return &tracks[i];
			return nullptr;
		}
		const NkMediaTrack *NkMediaInfo::FirstVideo() const {
			for (uint64 i = 0; i < tracks.Size(); ++i)
				if (tracks[i].type == NkMediaTrackType::NK_VIDEO)
					return &tracks[i];
			return nullptr;
		}

		NkMediaContainer NkMediaProbe::DetectContainer(const uint8 *data, usize size) {
			if (data == nullptr)
				return NkMediaContainer::NK_UNKNOWN;
			if (size >= 12 && Tag4(data + 4, "ftyp"))
				return NkMediaContainer::NK_MP4;
			if (size >= 4 && data[0] == 0x1A && data[1] == 0x45 && data[2] == 0xDF && data[3] == 0xA3)
				return NkMediaContainer::NK_WEBM;
			if (size >= 12 && Tag4(data, "RIFF") && Tag4(data + 8, "WAVE"))
				return NkMediaContainer::NK_WAV;
			if (size >= 4 && Tag4(data, "OggS"))
				return NkMediaContainer::NK_OGG;
			if (size >= 4 && Tag4(data, "fLaC"))
				return NkMediaContainer::NK_FLAC;
			if (size >= 3 && data[0] == 'I' && data[1] == 'D' && data[2] == '3')
				return NkMediaContainer::NK_MP3;
			if (size >= 2 && data[0] == 0xFF && (data[1] & 0xE0) == 0xE0)
				return NkMediaContainer::NK_MP3;
			return NkMediaContainer::NK_UNKNOWN;
		}

		bool NkMediaProbe::Probe(const uint8 *data, usize size, NkMediaInfo &out) {
			out.container = DetectContainer(data, size);
			out.tracks.Clear();
			switch (out.container) {
				case NkMediaContainer::NK_MP4:
					WalkMp4(data, 0, size, nullptr, out, 0);
					return true;
				case NkMediaContainer::NK_WEBM:
					WalkEbml(data, 0, size, out, nullptr, 0);
					return true;
				case NkMediaContainer::NK_WAV: {
					// En-tête WAV minimal : fmt chunk (channels, sampleRate).
					if (size >= 24 && Tag4(data + 12, "fmt ")) {
						NkMediaTrack tr;
						tr.type = NkMediaTrackType::NK_AUDIO;
						tr.codec = NkString("pcm");
						tr.channels = (int32)U16BE(data + 22); // little-endian en réalité...
						// WAV est little-endian : relire correctement.
						tr.channels = (int32)((uint16)data[22] | ((uint16)data[23] << 8));
						tr.sampleRate = (int32)((uint32)data[24] | ((uint32)data[25] << 8) |
												((uint32)data[26] << 16) | ((uint32)data[27] << 24));
						out.tracks.PushBack(tr);
					}
					return true;
				}
				case NkMediaContainer::NK_OGG: {
					// Parcours des pages BOS (début de flux) : identifie les
					// codecs Opus/Vorbis et leurs paramètres d'en-tête.
					usize pos = 0;
					while (pos + 27 <= size) {
						const uint8 *h = data + pos;
						if (!Tag4(h, "OggS") || h[4] != 0)
							break;
						const uint8 headerType = h[5];
						const uint8 nsegs = h[26];
						if (pos + 27 + nsegs > size)
							break;
						usize payload = 0;
						for (uint8 i = 0; i < nsegs; ++i)
							payload += h[27 + i];
						const uint8 *pl = h + 27 + nsegs;
						if (pos + 27 + nsegs + payload > size)
							break;
						if ((headerType & 0x02) == 0)
							break; // fin de la zone BOS
						if (payload >= 19 && pl[0] == 'O' && pl[1] == 'p' && pl[2] == 'u' && pl[3] == 's' &&
							pl[4] == 'H' && pl[5] == 'e' && pl[6] == 'a' && pl[7] == 'd') {
							NkMediaTrack tr;
							tr.type = NkMediaTrackType::NK_AUDIO;
							tr.codec = NkString("opus");
							tr.channels = (int32)pl[9];
							tr.sampleRate = 48000; // sortie décodeur Opus (le taux
												   // d'entrée original est pl[12..15])
							out.tracks.PushBack(tr);
						} else if (payload >= 30 && pl[0] == 0x01 && pl[1] == 'v' && pl[2] == 'o' && pl[3] == 'r' &&
								   pl[4] == 'b' && pl[5] == 'i' && pl[6] == 's') {
							NkMediaTrack tr;
							tr.type = NkMediaTrackType::NK_AUDIO;
							tr.codec = NkString("vorbis");
							tr.channels = (int32)pl[11];
							tr.sampleRate = (int32)((uint32)pl[12] | ((uint32)pl[13] << 8) | ((uint32)pl[14] << 16) |
													((uint32)pl[15] << 24));
							out.tracks.PushBack(tr);
						}
						pos += 27 + nsegs + payload;
					}
					return true;
				}
				case NkMediaContainer::NK_UNKNOWN:
					return false;
				default:
					return true; // conteneur reconnu, démux détaillé à venir (MP3/FLAC)
			}
		}

		bool NkMediaProbe::ProbeFile(const char *path, NkMediaInfo &out) {
			NkVector<nk_uint8> bytes = NkFile::ReadAllBytes(path);
			if (bytes.Size() == 0)
				return false;
			return Probe(bytes.Data(), (usize)bytes.Size(), out);
		}

		bool NkMediaProbe::SelfTest() {
			bool ok = true;

			// Détection de conteneur sur des en-têtes synthétiques.
			{
				uint8 mp4[16] = {0, 0, 0, 0x18, 'f', 't', 'y', 'p', 'i', 's', 'o', 'm', 0, 0, 0, 1};
				if (DetectContainer(mp4, 16) != NkMediaContainer::NK_MP4)
					ok = false;
			}
			{
				uint8 webm[8] = {0x1A, 0x45, 0xDF, 0xA3, 0x01, 0x00, 0x00, 0x00};
				if (DetectContainer(webm, 8) != NkMediaContainer::NK_WEBM)
					ok = false;
			}
			{
				uint8 wav[16] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' '};
				if (DetectContainer(wav, 16) != NkMediaContainer::NK_WAV)
					ok = false;
			}
			{
				uint8 ogg[4] = {'O', 'g', 'g', 'S'};
				if (DetectContainer(ogg, 4) != NkMediaContainer::NK_OGG)
					ok = false;
			}
			{
				uint8 flac[4] = {'f', 'L', 'a', 'C'};
				if (DetectContainer(flac, 4) != NkMediaContainer::NK_FLAC)
					ok = false;
			}
			{
				uint8 id3[3] = {'I', 'D', '3'};
				if (DetectContainer(id3, 3) != NkMediaContainer::NK_MP3)
					ok = false;
			}

			// Vint EBML : tailles connues.
			{
				uint8 v1[1] = {0x81}; // len 1, valeur 1
				uint64 val = 0;
				if (ReadVint(v1, 1, val, false) != 1 || val != 1)
					ok = false;
				uint8 v2[2] = {0x40, 0x02}; // len 2, valeur 2
				if (ReadVint(v2, 2, val, false) != 2 || val != 2)
					ok = false;
				// ID (keepMarker) : 0x9F reste 0x9F.
				if (ReadVint(v1, 1, val, true) != 1 || val != 0x81)
					ok = false;
			}

			return ok;
		}

	} // namespace media
} // namespace nkentseu
