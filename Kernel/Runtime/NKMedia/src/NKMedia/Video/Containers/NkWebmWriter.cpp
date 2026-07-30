// =============================================================================
// NKMedia/Video/Containers/NkWebmWriter.cpp — muxer WebM (EBML/Matroska).
// EBML : identifiants (IDs) et tailles (VINT) big-endian ; contenu des blocs
// vidéo/audio = octets bruts. Miroir écriture du démux ParseWebm de NkVideoReader.
// =============================================================================
#include "NKMedia/Video/Containers/NkWebmWriter.h"

namespace nkentseu {
	namespace media {

		namespace {
			// --- Identifiants EBML (Matroska/WebM). Repris à l'identique du démux. ---
			constexpr uint32 kIdEbmlHeader = 0x1A45DFA3;
			constexpr uint32 kIdEbmlVersion = 0x4286;
			constexpr uint32 kIdEbmlReadVersion = 0x42F7;
			constexpr uint32 kIdEbmlMaxIdLength = 0x42F2;
			constexpr uint32 kIdEbmlMaxSizeLength = 0x42F3;
			constexpr uint32 kIdDocType = 0x4282;
			constexpr uint32 kIdDocTypeVersion = 0x4287;
			constexpr uint32 kIdDocTypeReadVersion = 0x4285;

			constexpr uint32 kIdSegment = 0x18538067;
			constexpr uint32 kIdInfo = 0x1549A966;
			constexpr uint32 kIdTimestampScale = 0x2AD7B1;
			constexpr uint32 kIdMuxingApp = 0x4D80;
			constexpr uint32 kIdWritingApp = 0x5741;
			constexpr uint32 kIdDuration = 0x4489;

			constexpr uint32 kIdTracks = 0x1654AE6B;
			constexpr uint32 kIdTrackEntry = 0xAE;
			constexpr uint32 kIdTrackNumber = 0xD7;
			constexpr uint32 kIdTrackUid = 0x73C5;
			constexpr uint32 kIdTrackType = 0x83;
			constexpr uint32 kIdFlagLacing = 0x9C;
			constexpr uint32 kIdCodecId = 0x86;
			constexpr uint32 kIdCodecPrivate = 0x63A2;
			constexpr uint32 kIdCodecDelay = 0x56AA;
			constexpr uint32 kIdSeekPreRoll = 0x56BB;
			constexpr uint32 kIdVideo = 0xE0;
			constexpr uint32 kIdPixelWidth = 0xB0;
			constexpr uint32 kIdPixelHeight = 0xBA;
			constexpr uint32 kIdAudio = 0xE1;
			constexpr uint32 kIdSamplingFrequency = 0xB5;
			constexpr uint32 kIdChannels = 0x9F;

			constexpr uint32 kIdCluster = 0x1F43B675;
			constexpr uint32 kIdTimestamp = 0xE7;
			constexpr uint32 kIdSimpleBlock = 0xA3;

			// --- Assembleur EBML big-endian (construit un arbre en mémoire). ---
			struct Ebml {
					NkVector<uint8> d;

					void U8(uint8 v) {
						d.PushBack(v);
					}
					void Bytes(const uint8 *p, usize n) {
						for (usize i = 0; i < n; ++i)
							d.PushBack(p[i]);
					}
					void Append(const Ebml &o) {
						for (uint64 i = 0; i < o.d.Size(); ++i)
							d.PushBack(o.d[i]);
					}
					usize Size() const {
						return (usize)d.Size();
					}

					// Écrit un identifiant EBML : ses octets big-endian minimaux (l'ID
					// contient déjà son descripteur de longueur — on n'y touche pas).
					void Id(uint32 id) {
						if (id <= 0xFF)
							U8((uint8)id);
						else if (id <= 0xFFFF) {
							U8((uint8)(id >> 8));
							U8((uint8)id);
						} else if (id <= 0xFFFFFF) {
							U8((uint8)(id >> 16));
							U8((uint8)(id >> 8));
							U8((uint8)id);
						} else {
							U8((uint8)(id >> 24));
							U8((uint8)(id >> 16));
							U8((uint8)(id >> 8));
							U8((uint8)id);
						}
					}

					// Nombre d'octets d'une taille/valeur VINT de longueur minimale
					// (valeur < 2^(7L)-1 ; le -1 réserve l'encodage « tout à un » = inconnu).
					static uint32 VintLen(uint64 v) {
						uint32 L = 1;
						while (L < 8 && v >= ((1ULL << (7 * L)) - 1))
							++L;
						return L;
					}
					// Écrit `v` en VINT de longueur `L` forcée (marqueur au bit 7*L).
					void VintFixed(uint64 v, uint32 L) {
						const uint64 enc = (1ULL << (7 * L)) | v;
						for (int32 i = (int32)L - 1; i >= 0; --i)
							U8((uint8)(enc >> (8 * (uint32)i)));
					}
					// Écrit une taille/valeur VINT de longueur minimale.
					void Size(uint64 v) {
						VintFixed(v, VintLen(v));
					}

					// --- Éléments typés (ID + taille + contenu). ---
					void ElemUint(uint32 id, uint64 val) {
						uint32 n = 1;
						while (n < 8 && (val >> (8 * n)) != 0)
							++n;
						Id(id);
						Size(n);
						for (int32 i = (int32)n - 1; i >= 0; --i)
							U8((uint8)(val >> (8 * (uint32)i)));
					}
					void ElemStr(uint32 id, const char *s) {
						usize n = 0;
						while (s[n] != 0)
							++n;
						Id(id);
						Size(n);
						for (usize i = 0; i < n; ++i)
							U8((uint8)s[i]);
					}
					void ElemFloat64(uint32 id, double val) {
						Id(id);
						Size(8);
						union {
								double dd;
								uint64 uu;
						} cv;
						cv.dd = val;
						for (int32 i = 7; i >= 0; --i)
							U8((uint8)(cv.uu >> (8 * (uint32)i)));
					}
					void ElemBin(uint32 id, const uint8 *p, usize n) {
						Id(id);
						Size(n);
						Bytes(p, n);
					}
					void ElemMaster(uint32 id, const Ebml &payload) {
						Id(id);
						Size(payload.Size());
						Append(payload);
					}
			};

			// Écrit un tampon d'octets sur le fichier.
			inline void WriteBuf(NkFile &f, const Ebml &e) {
				if (e.Size() > 0)
					f.Write(e.d.Data(), e.Size());
			}
		} // namespace

		void NkWebmWriter::StartCluster(int64 tsMs) {
			mCluster.Clear();
			Ebml ts;
			ts.ElemUint(kIdTimestamp, (uint64)(tsMs < 0 ? 0 : tsMs));
			for (uint64 i = 0; i < ts.d.Size(); ++i)
				mCluster.PushBack(ts.d[i]);
			mClusterBaseTs = tsMs;
			mClusterOpen = true;
		}

		void NkWebmWriter::FlushCluster() {
			if (!mClusterOpen)
				return;
			if (mCluster.Size() > 0) {
				Ebml hdr;
				hdr.Id(kIdCluster);
				hdr.Size((uint64)mCluster.Size());
				WriteBuf(mFile, hdr);
				mFile.Write(mCluster.Data(), (usize)mCluster.Size());
			}
			mCluster.Clear();
			mClusterOpen = false;
		}

		bool NkWebmWriter::AddBlock(uint64 trackNum, const uint8 *data, usize size, int64 timestampMs,
									bool keyframe) {
			if (!mFile.IsOpen() || data == nullptr || size == 0)
				return false;

			// Nouveau Cluster si : aucun ouvert, l'horodatage relatif déborderait le
			// champ int16 du SimpleBlock, ou l'on démarre une image-clé vidéo sur un
			// Cluster déjà rempli (pratique standard : chaque Cluster commence par une
			// image-clé → seekable). Un Cluster « vide » ne contient que son Timestamp.
			const bool videoKeyStart =
				keyframe && trackNum == kVideoTrack && mClusterOpen && mCluster.Size() > 3;
			int64 rel = mClusterOpen ? (timestampMs - mClusterBaseTs) : 0;
			const bool relOverflow = mClusterOpen && (rel > 32767 || rel < -32768);
			if (!mClusterOpen || videoKeyStart || relOverflow) {
				FlushCluster();
				StartCluster(timestampMs);
				rel = 0;
			}

			// SimpleBlock : [ID][taille][num piste VINT][int16 BE rel][flags][données].
			Ebml content;
			content.Size(trackNum); // le numéro de piste utilise le format VINT
			content.U8((uint8)(((uint16)(int16)rel >> 8) & 0xFF));
			content.U8((uint8)((uint16)(int16)rel & 0xFF));
			content.U8((uint8)(keyframe ? 0x80 : 0x00)); // bit 7 = image-clé, lacing = 0
			content.Bytes(data, size);

			Ebml blk;
			blk.Id(kIdSimpleBlock);
			blk.Size(content.Size());
			blk.Append(content);

			for (uint64 i = 0; i < blk.d.Size(); ++i)
				mCluster.PushBack(blk.d[i]);

			const int64 endTs = timestampMs;
			if (endTs > mMaxEndTsMs)
				mMaxEndTsMs = endTs;
			return true;
		}

		bool NkWebmWriter::Open(const char *path, const NkWebmConfig &config) {
			if (config.videoCodec == NkWebmVideoCodec::NONE || config.width <= 0 || config.height <= 0)
				return false;
			if (config.audioCodec == NkWebmAudioCodec::OPUS &&
				(config.audioCodecPrivate == nullptr || config.audioCodecPrivateSize < 8))
				return false;

			const uint32 mode =
				(uint32)NkFileMode::NK_WRITE | (uint32)NkFileMode::NK_BINARY | (uint32)NkFileMode::NK_TRUNCATE;
			if (!mFile.Open(path, (NkFileMode)mode))
				return false;

			mConfig = config;
			mVideoFrames = 0;
			mAudioFrames = 0;
			mMaxEndTsMs = 0;
			mCluster.Clear();
			mClusterOpen = false;

			// ---- EBML Header (DocType "webm") ----
			Ebml eh;
			eh.ElemUint(kIdEbmlVersion, 1);
			eh.ElemUint(kIdEbmlReadVersion, 1);
			eh.ElemUint(kIdEbmlMaxIdLength, 4);
			eh.ElemUint(kIdEbmlMaxSizeLength, 8);
			eh.ElemStr(kIdDocType, "webm");
			eh.ElemUint(kIdDocTypeVersion, 2);
			eh.ElemUint(kIdDocTypeReadVersion, 2);
			Ebml headerBox;
			headerBox.ElemMaster(kIdEbmlHeader, eh);
			WriteBuf(mFile, headerBox);

			// ---- Segment (taille inconnue à l'écriture → réservée 8 octets, rapiécée) ----
			Ebml segId;
			segId.Id(kIdSegment);
			WriteBuf(mFile, segId);
			mSegmentSizePos = mFile.Tell();
			Ebml segSize;
			segSize.VintFixed(0, 8); // placeholder VINT 8 octets
			WriteBuf(mFile, segSize);
			mSegmentDataStart = mFile.Tell();

			// ---- Info (TimestampScale = 1 ms, Duration placeholder rapiécée, apps) ----
			Ebml info;
			info.ElemUint(kIdTimestampScale, 1000000); // 1 ms en nanosecondes
			info.ElemStr(kIdMuxingApp, "NkWebmWriter");
			info.ElemStr(kIdWritingApp, "NkWebmWriter");
			// Duration : on repère l'offset (dans le tampon Info) des 8 octets du double.
			info.Id(kIdDuration);
			info.Size(8);
			const usize durOffsetInInfo = info.Size();
			{
				union {
						double dd;
						uint64 uu;
				} cv;
				cv.dd = 0.0;
				for (int32 i = 7; i >= 0; --i)
					info.U8((uint8)(cv.uu >> (8 * (uint32)i)));
			}
			Ebml infoBox;
			infoBox.Id(kIdInfo);
			infoBox.Size(info.Size());
			const nk_int64 infoHdrLen = (nk_int64)infoBox.Size(); // ID + taille (avant contenu)
			infoBox.Append(info);
			const nk_int64 infoStart = mFile.Tell();
			WriteBuf(mFile, infoBox);
			mDurationPos = infoStart + infoHdrLen + (nk_int64)durOffsetInInfo;

			// ---- Tracks ----
			Ebml tracks;

			// TrackEntry vidéo.
			{
				Ebml video;
				video.ElemUint(kIdPixelWidth, (uint64)config.width);
				video.ElemUint(kIdPixelHeight, (uint64)config.height);

				Ebml te;
				te.ElemUint(kIdTrackNumber, kVideoTrack);
				te.ElemUint(kIdTrackUid, kVideoTrack);
				te.ElemUint(kIdTrackType, 1); // vidéo
				te.ElemUint(kIdFlagLacing, 0);
				te.ElemStr(kIdCodecId, config.videoCodec == NkWebmVideoCodec::VP8 ? "V_VP8" : "V_VP9");
				te.ElemMaster(kIdVideo, video);
				tracks.ElemMaster(kIdTrackEntry, te);
			}

			// TrackEntry audio Opus (optionnel).
			if (config.audioCodec == NkWebmAudioCodec::OPUS) {
				Ebml audio;
				audio.ElemFloat64(kIdSamplingFrequency, (double)config.audioSampleRate);
				audio.ElemUint(kIdChannels, (uint64)(config.audioChannels > 0 ? config.audioChannels : 2));

				Ebml te;
				te.ElemUint(kIdTrackNumber, kAudioTrack);
				te.ElemUint(kIdTrackUid, kAudioTrack);
				te.ElemUint(kIdTrackType, 2); // audio
				te.ElemUint(kIdFlagLacing, 0);
				te.ElemStr(kIdCodecId, "A_OPUS");
				te.ElemBin(kIdCodecPrivate, config.audioCodecPrivate, config.audioCodecPrivateSize);
				// CodecDelay = pre-skip (échantillons à 48 kHz) converti en nanosecondes ;
				// SeekPreRoll = 80 ms (valeur canonique Opus). Le pre-skip est aux octets
				// 10..11 (little-endian) de l'OpusHead.
				if (config.audioCodecPrivateSize >= 12) {
					const uint32 preSkip = (uint32)config.audioCodecPrivate[10] |
										   ((uint32)config.audioCodecPrivate[11] << 8);
					const uint64 codecDelayNs = (uint64)preSkip * 1000000000ULL / 48000ULL;
					te.ElemUint(kIdCodecDelay, codecDelayNs);
				}
				te.ElemUint(kIdSeekPreRoll, 80000000ULL); // 80 ms
				te.ElemMaster(kIdAudio, audio);
				tracks.ElemMaster(kIdTrackEntry, te);
			}

			Ebml tracksBox;
			tracksBox.ElemMaster(kIdTracks, tracks);
			WriteBuf(mFile, tracksBox);

			return mFile.IsOpen();
		}

		bool NkWebmWriter::AddVideoFrame(const uint8 *data, usize size, int64 timestampMs,
										 bool isKeyframe) {
			if (!AddBlock(kVideoTrack, data, size, timestampMs, isKeyframe))
				return false;
			++mVideoFrames;
			return true;
		}

		bool NkWebmWriter::AddAudioFrame(const uint8 *data, usize size, int64 timestampMs) {
			if (mConfig.audioCodec != NkWebmAudioCodec::OPUS)
				return false;
			// Les trames Opus sont toutes « image-clé » côté conteneur.
			if (!AddBlock(kAudioTrack, data, size, timestampMs, true))
				return false;
			++mAudioFrames;
			return true;
		}

		bool NkWebmWriter::Finalize() {
			if (!mFile.IsOpen())
				return false;

			FlushCluster();
			const nk_int64 fileEnd = mFile.Tell();

			// Rapièce Duration (double, en unités de TimestampScale = ms). On approxime
			// la durée par le dernier horodatage rencontré + une image (souvent suffisant
			// pour les lecteurs ; ffprobe recalcule de toute façon depuis les paquets).
			{
				const double durationMs = (double)(mMaxEndTsMs > 0 ? mMaxEndTsMs : 0);
				union {
						double dd;
						uint64 uu;
				} cv;
				cv.dd = durationMs;
				uint8 fb[8];
				for (int32 i = 7; i >= 0; --i)
					fb[7 - i] = (uint8)(cv.uu >> (8 * (uint32)i));
				mFile.Seek(mDurationPos, NkSeekOrigin::NK_BEGIN);
				mFile.Write(fb, 8);
			}

			// Rapièce la taille du Segment (VINT 8 octets fixe).
			{
				const uint64 segLen = (uint64)(fileEnd - mSegmentDataStart);
				Ebml sz;
				sz.VintFixed(segLen, 8);
				mFile.Seek(mSegmentSizePos, NkSeekOrigin::NK_BEGIN);
				mFile.Write(sz.d.Data(), sz.Size());
			}

			mFile.Seek(fileEnd, NkSeekOrigin::NK_BEGIN);
			mFile.Close();
			return true;
		}

		// -----------------------------------------------------------------------------
		// SelfTest : écrit un WebM VP9 minimal (2 paquets factices), relit les octets et
		// re-parse l'EBML pour vérifier la structure. Indépendant de tout lecteur externe.
		// -----------------------------------------------------------------------------
		namespace {
			// Lecteur VINT EBML minimal (copie de la logique de NkVideoReader::EbmlReadVint).
			int32 SelfReadVint(const uint8 *p, usize avail, uint64 &value, bool keepMarker) {
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
			uint64 SelfReadUint(const uint8 *p, usize len) {
				uint64 v = 0;
				for (usize i = 0; i < len && i < 8; ++i)
					v = (v << 8) | p[i];
				return v;
			}
		} // namespace

		bool NkWebmWriter::SelfTest() {
			const char *path = "nkwebmwriter_selftest.webm";
			const int32 W = 64, H = 48;

			// Deux « paquets » VP9 factices (le muxer ne décode pas : n'importe quels octets).
			uint8 pkt0[16];
			uint8 pkt1[24];
			for (int32 i = 0; i < 16; ++i)
				pkt0[i] = (uint8)(0xA0 + i);
			for (int32 i = 0; i < 24; ++i)
				pkt1[i] = (uint8)(0x10 + i);

			NkWebmConfig cfg;
			cfg.videoCodec = NkWebmVideoCodec::VP9;
			cfg.width = W;
			cfg.height = H;

			NkWebmWriter wr;
			if (!wr.Open(path, cfg))
				return false;
			if (!wr.AddVideoFrame(pkt0, sizeof(pkt0), 0, true))
				return false;
			if (!wr.AddVideoFrame(pkt1, sizeof(pkt1), 33, false))
				return false;
			if (!wr.Finalize())
				return false;

			NkVector<uint8> bytes = NkFile::ReadAllBytes(path);
			const uint8 *d = bytes.Data();
			const usize n = (usize)bytes.Size();
			if (n < 32)
				return false;

			// L'en-tête EBML débute par 0x1A 0x45 0xDF 0xA3.
			if (d[0] != 0x1A || d[1] != 0x45 || d[2] != 0xDF || d[3] != 0xA3)
				return false;

			// Parcours EBML : on descend dans les masters (EBML Header, Segment, Info,
			// Tracks, TrackEntry, Video, Cluster) et on collecte les champs à vérifier.
			bool sawSegment = false, sawInfo = false, sawTracks = false;
			bool sawVideoTrack = false, sawCluster = false;
			int32 foundW = 0, foundH = 0;
			int32 blockCount = 0, keyBlocks = 0;
			bool codecOk = false;

			// Pile explicite [start,end) pour éviter la récursion.
			struct Range {
					usize pos, end;
			};
			Range stack[32];
			int32 sp = 0;
			stack[sp++] = {0, n};

			while (sp > 0) {
				Range &r = stack[sp - 1];
				if (r.pos >= r.end) {
					--sp;
					continue;
				}
				uint64 id = 0, sz = 0;
				int32 il = SelfReadVint(d + r.pos, r.end - r.pos, id, true);
				if (il <= 0) {
					--sp;
					continue;
				}
				int32 sl = SelfReadVint(d + r.pos + il, r.end - r.pos - il, sz, false);
				if (sl <= 0) {
					--sp;
					continue;
				}
				const usize ds = r.pos + il + sl;
				const uint64 allOnes = (1ULL << (7 * sl)) - 1;
				const usize de = (sz == allOnes || ds + (usize)sz > r.end) ? r.end : ds + (usize)sz;

				bool descend = false;
				if (id == kIdEbmlHeader || id == kIdSegment || id == kIdInfo || id == kIdTracks ||
					id == kIdTrackEntry || id == kIdVideo || id == kIdCluster) {
					descend = true;
					if (id == kIdSegment)
						sawSegment = true;
					if (id == kIdInfo)
						sawInfo = true;
					if (id == kIdTracks)
						sawTracks = true;
					if (id == kIdCluster)
						sawCluster = true;
				} else if (id == kIdTrackType) {
					if (SelfReadUint(d + ds, de - ds) == 1)
						sawVideoTrack = true;
				} else if (id == kIdCodecId) {
					if (de - ds == 5 && d[ds] == 'V' && d[ds + 1] == '_' && d[ds + 2] == 'V' &&
						d[ds + 3] == 'P' && d[ds + 4] == '9')
						codecOk = true;
				} else if (id == kIdPixelWidth) {
					foundW = (int32)SelfReadUint(d + ds, de - ds);
				} else if (id == kIdPixelHeight) {
					foundH = (int32)SelfReadUint(d + ds, de - ds);
				} else if (id == kIdSimpleBlock) {
					uint64 trackNum = 0;
					int32 tl = SelfReadVint(d + ds, de - ds, trackNum, false);
					if (tl > 0 && ds + (usize)tl + 3 <= de && trackNum == kVideoTrack) {
						const uint8 flags = d[ds + (usize)tl + 2];
						++blockCount;
						if (flags & 0x80)
							++keyBlocks;
					}
				}

				// Passe à l'élément frère.
				r.pos = de;
				if (descend && sp < 32)
					stack[sp++] = {ds, de};
			}

			return sawSegment && sawInfo && sawTracks && sawVideoTrack && sawCluster && codecOk &&
				   foundW == W && foundH == H && blockCount == 2 && keyBlocks == 1;
		}

	} // namespace media
} // namespace nkentseu
