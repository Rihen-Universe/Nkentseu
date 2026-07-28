// =============================================================================
// NKMedia/Video/Containers/NkMovWriter.cpp — muxer QuickTime/MP4 (ISOBMFF) MJPEG.
// Boxes big-endian. ftyp + mdat (frames JPEG) + moov (mvhd/trak/.../stbl).
// =============================================================================
#include "NKMedia/Video/Containers/NkMovWriter.h"

namespace nkentseu {
	namespace media {

		namespace {
			// --- Assembleur d'octets big-endian (pour construire moov en mémoire). ---
			struct ByteBuf {
					NkVector<uint8> d;
					void u8(uint8 v) {
						d.PushBack(v);
					}
					void u16(uint16 v) {
						d.PushBack((uint8)(v >> 8));
						d.PushBack((uint8)(v & 0xFF));
					}
					void u32(uint32 v) {
						d.PushBack((uint8)(v >> 24));
						d.PushBack((uint8)((v >> 16) & 0xFF));
						d.PushBack((uint8)((v >> 8) & 0xFF));
						d.PushBack((uint8)(v & 0xFF));
					}
					void tag(char a, char b, char c, char e) {
						d.PushBack((uint8)a);
						d.PushBack((uint8)b);
						d.PushBack((uint8)c);
						d.PushBack((uint8)e);
					}
					void bytes(const uint8 *p, usize n) {
						for (usize i = 0; i < n; ++i)
							d.PushBack(p[i]);
					}
					void append(const ByteBuf &o) {
						for (uint64 i = 0; i < o.d.Size(); ++i)
							d.PushBack(o.d[i]);
					}
					usize size() const {
						return (usize)d.Size();
					}
			};

			// Enveloppe `payload` dans une box [size][type] et l'ajoute à `out`.
			void Box(ByteBuf &out, char a, char b, char c, char e, const ByteBuf &payload) {
				out.u32((uint32)(8 + payload.size()));
				out.tag(a, b, c, e);
				out.append(payload);
			}
		} // namespace

		bool NkMovWriter::Open(const char *path, int32 width, int32 height, int32 fpsNum, int32 fpsDen) {
			if (width <= 0 || height <= 0 || fpsDen <= 0 || fpsNum <= 0)
				return false;
			const uint32 mode =
				(uint32)NkFileMode::NK_WRITE | (uint32)NkFileMode::NK_BINARY | (uint32)NkFileMode::NK_TRUNCATE;
			if (!mFile.Open(path, (NkFileMode)mode))
				return false;
			mWidth = width;
			mHeight = height;
			mFpsNum = fpsNum;
			mFpsDen = fpsDen;
			mSizes.Clear();
			mOffsets.Clear();
			mAudioChunkOff.Clear();
			mAudioChunkFrames.Clear();
			mAudioFrames = 0;

			// ---- ftyp ----
			ByteBuf ftyp;
			ftyp.tag('q', 't', ' ', ' '); // major brand QuickTime
			ftyp.u32(0x00000200);		  // minor version
			ftyp.tag('q', 't', ' ', ' '); // compatible brand
			ByteBuf out;
			Box(out, 'f', 't', 'y', 'p', ftyp);
			mFile.Write(out.d.Data(), out.size());

			// ---- mdat (streaming) : on écrit l'en-tête, taille rapiécée à la fermeture. ----
			mMdatSizePos = mFile.Tell();
			uint8 hdr[8] = {0, 0, 0, 0, 'm', 'd', 'a', 't'};
			mFile.Write(hdr, 8);
			mMdatStart = mFile.Tell();
			return true;
		}

		bool NkMovWriter::WriteFrame(const uint8 *data, uint32 size, bool /*keyframe*/) {
			if (!mFile.IsOpen() || data == nullptr)
				return false;
			const nk_int64 off = mFile.Tell();
			mFile.Write(data, size);
			mOffsets.PushBack((uint32)off);
			mSizes.PushBack(size);
			return true;
		}

		void NkMovWriter::SetAudio(int32 sampleRate, int32 channels) {
			if (sampleRate > 0 && channels > 0) {
				mAudioRate = sampleRate;
				mAudioChannels = channels;
			} else {
				mAudioRate = 0;
				mAudioChannels = 0;
			}
		}

		bool NkMovWriter::WriteAudio(const int16 *interleaved, uint32 frames) {
			if (!mFile.IsOpen() || interleaved == nullptr || frames == 0)
				return false;
			if (mAudioRate <= 0 || mAudioChannels <= 0)
				return false; // SetAudio non appelé

			const nk_int64 off = mFile.Tell();
			// PCM s16 little-endian ('sowt') — sérialisation explicite, indépendante de l'hôte.
			const uint32 n = frames * (uint32)mAudioChannels;
			NkVector<uint8> bytes;
			bytes.Reserve((uint64)n * 2);
			for (uint32 i = 0; i < n; ++i) {
				const uint16 v = (uint16)interleaved[i];
				bytes.PushBack((uint8)(v & 0xFF));
				bytes.PushBack((uint8)((v >> 8) & 0xFF));
			}
			mFile.Write(bytes.Data(), (usize)bytes.Size());
			mAudioChunkOff.PushBack((uint32)off);
			mAudioChunkFrames.PushBack(frames);
			mAudioFrames += frames;
			return true;
		}

		bool NkMovWriter::Close() {
			if (!mFile.IsOpen())
				return false;

			const nk_int64 mdatEnd = mFile.Tell();
			const uint32 nSamples = (uint32)mSizes.Size();

			// Rapièce la taille de mdat.
			const uint32 mdatSize = (uint32)(mdatEnd - mMdatSizePos);
			mFile.Seek(mMdatSizePos, NkSeekOrigin::NK_BEGIN);
			uint8 sz[4] = {(uint8)(mdatSize >> 24), (uint8)(mdatSize >> 16), (uint8)(mdatSize >> 8), (uint8)mdatSize};
			mFile.Write(sz, 4);
			mFile.Seek(mdatEnd, NkSeekOrigin::NK_BEGIN);

			// Échelle de temps = fpsNum ; durée d'un échantillon = fpsDen ; durée totale = nSamples*fpsDen.
			const uint32 timescale = (uint32)mFpsNum;
			const uint32 sampleDur = (uint32)mFpsDen;
			const uint32 duration = nSamples * sampleDur;

			// ---- stbl children ----
			// stsd : une entrée 'jpeg' (VisualSampleEntry).
			ByteBuf visual;
			for (int i = 0; i < 6; ++i)
				visual.u8(0);	  // reserved
			visual.u16(1);		  // data_reference_index
			visual.u16(0);		  // version
			visual.u16(0);		  // revision
			visual.u32(0);		  // vendor
			visual.u32(0);		  // temporal quality
			visual.u32(512);	  // spatial quality
			visual.u16((uint16)mWidth);
			visual.u16((uint16)mHeight);
			visual.u32(0x00480000); // horiz res 72dpi
			visual.u32(0x00480000); // vert res
			visual.u32(0);			// data size
			visual.u16(1);			// frame count
			// compressor name (pascal string, 32 octets)
			const char *cname = "Motion JPEG";
			visual.u8((uint8)11);
			for (int i = 0; i < 31; ++i)
				visual.u8(i < 11 ? (uint8)cname[i] : 0);
			visual.u16(24);		// depth
			visual.u16(0xFFFF); // color table id
			ByteBuf jpegEntry;
			Box(jpegEntry, 'j', 'p', 'e', 'g', visual);
			ByteBuf stsd;
			stsd.u32(0);		 // version+flags
			stsd.u32(1);		 // entry count
			stsd.append(jpegEntry);

			// stts : nSamples avec durée sampleDur.
			ByteBuf stts;
			stts.u32(0);
			stts.u32(1); // one entry
			stts.u32(nSamples);
			stts.u32(sampleDur);

			// stsc : tout dans un seul chunk (first_chunk=1, samples_per_chunk=nSamples, desc=1).
			ByteBuf stsc;
			stsc.u32(0);
			stsc.u32(1);
			stsc.u32(1);
			stsc.u32(nSamples ? nSamples : 1);
			stsc.u32(1);

			// stsz : taille par échantillon.
			ByteBuf stsz;
			stsz.u32(0);
			stsz.u32(0); // sample_size=0 → tailles individuelles
			stsz.u32(nSamples);
			for (uint32 i = 0; i < nSamples; ++i)
				stsz.u32(mSizes[i]);

			// stco : offset de chaque échantillon (un chunk par échantillon → offsets = ceux enregistrés).
			ByteBuf stco;
			stco.u32(0);
			stco.u32(nSamples);
			for (uint32 i = 0; i < nSamples; ++i)
				stco.u32(mOffsets[i]);
			// NB : stsc dit "tout en 1 chunk" mais on liste chaque offset → on met stsc à 1 sample/chunk.
			// Corrige stsc pour 1 échantillon par chunk (cohérent avec stco par-échantillon).
			stsc.d.Clear();
			stsc.u32(0);
			stsc.u32(1);
			stsc.u32(1);
			stsc.u32(1);
			stsc.u32(1);

			ByteBuf stbl;
			Box(stbl, 's', 't', 's', 'd', stsd);
			Box(stbl, 's', 't', 't', 's', stts);
			Box(stbl, 's', 't', 's', 'c', stsc);
			Box(stbl, 's', 't', 's', 'z', stsz);
			Box(stbl, 's', 't', 'c', 'o', stco);

			// ---- minf ----
			ByteBuf vmhd;
			vmhd.u32(0x00000001); // version+flags (1)
			vmhd.u16(0);		  // graphics mode
			vmhd.u16(0);
			vmhd.u16(0);
			vmhd.u16(0); // opcolor
			ByteBuf drefUrl;
			drefUrl.u32(0x00000001); // 'url ' flags = self-contained
			ByteBuf dref;
			dref.u32(0);
			dref.u32(1);
			Box(dref, 'u', 'r', 'l', ' ', drefUrl);
			ByteBuf dinf;
			Box(dinf, 'd', 'r', 'e', 'f', dref);
			ByteBuf minf;
			Box(minf, 'v', 'm', 'h', 'd', vmhd);
			Box(minf, 'd', 'i', 'n', 'f', dinf);
			Box(minf, 's', 't', 'b', 'l', stbl);

			// ---- mdia ----
			ByteBuf mdhd;
			mdhd.u32(0);		 // version+flags
			mdhd.u32(0);		 // creation
			mdhd.u32(0);		 // modification
			mdhd.u32(timescale); // timescale
			mdhd.u32(duration);	 // duration
			mdhd.u16(0x55C4);	 // language (und)
			mdhd.u16(0);		 // quality
			ByteBuf hdlr;
			hdlr.u32(0);
			hdlr.u32(0);				  // component type
			hdlr.tag('v', 'i', 'd', 'e'); // component subtype
			hdlr.u32(0);
			hdlr.u32(0);
			hdlr.u32(0);
			hdlr.u8(0); // name (empty pascal)
			ByteBuf mdia;
			Box(mdia, 'm', 'd', 'h', 'd', mdhd);
			Box(mdia, 'h', 'd', 'l', 'r', hdlr);
			Box(mdia, 'm', 'i', 'n', 'f', minf);

			// ---- tkhd ----
			ByteBuf tkhd;
			tkhd.u32(0x00000007); // flags: enabled|in movie|in preview
			tkhd.u32(0);
			tkhd.u32(0);		  // creation/modification
			tkhd.u32(1);		  // track id
			tkhd.u32(0);		  // reserved
			tkhd.u32(duration);	  // duration (movie timescale = same here)
			tkhd.u32(0);
			tkhd.u32(0);   // reserved
			tkhd.u16(0);   // layer
			tkhd.u16(0);   // alternate group
			tkhd.u16(0);   // volume
			tkhd.u16(0);   // reserved
			// matrix (identity)
			const uint32 mtx[9] = {0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};
			for (int i = 0; i < 9; ++i)
				tkhd.u32(mtx[i]);
			tkhd.u32((uint32)mWidth << 16);	 // width (16.16)
			tkhd.u32((uint32)mHeight << 16); // height (16.16)
			ByteBuf trak;
			Box(trak, 't', 'k', 'h', 'd', tkhd);
			Box(trak, 'm', 'd', 'i', 'a', mdia);

			// ---- trak audio PCM 'sowt' (optionnel) ----
			const bool hasAudio = (mAudioRate > 0 && mAudioChannels > 0 && mAudioChunkOff.Size() > 0);
			// Durée audio dans l'échelle du FILM (timescale vidéo).
			const uint32 aDurMovie =
				hasAudio ? (uint32)(mAudioFrames * (uint64)timescale / (uint64)mAudioRate) : 0;
			const uint32 movieDur = (aDurMovie > duration) ? aDurMovie : duration;
			ByteBuf atrak;
			if (hasAudio) {
				const uint32 nChunks = (uint32)mAudioChunkOff.Size();

				// stsd : AudioSampleEntry v0 'sowt' (PCM s16 LE).
				ByteBuf aud;
				for (int i = 0; i < 6; ++i)
					aud.u8(0);						// reserved
				aud.u16(1);							// data_reference_index
				aud.u16(0);							// version
				aud.u16(0);							// revision
				aud.u32(0);							// vendor
				aud.u16((uint16)mAudioChannels);	// channel count
				aud.u16(16);						// sample size (bits)
				aud.u16(0);							// compression id
				aud.u16(0);							// packet size
				aud.u32((uint32)mAudioRate << 16);	// sample rate (16.16)
				ByteBuf sowt;
				Box(sowt, 's', 'o', 'w', 't', aud);
				ByteBuf astsd;
				astsd.u32(0);
				astsd.u32(1);
				astsd.append(sowt);

				// stts : mAudioFrames échantillons de durée 1 (échelle piste = sampleRate).
				ByteBuf astts;
				astts.u32(0);
				astts.u32(1);
				astts.u32((uint32)mAudioFrames);
				astts.u32(1);

				// stsc : trames par chunk — une entrée par CHANGEMENT (runs compactés).
				ByteBuf astscEntries;
				uint32 nStscEntries = 0;
				for (uint32 i = 0; i < nChunks; ++i) {
					if (i == 0 || mAudioChunkFrames[i] != mAudioChunkFrames[i - 1]) {
						astscEntries.u32(i + 1);			   // first_chunk (1-based)
						astscEntries.u32(mAudioChunkFrames[i]); // samples_per_chunk
						astscEntries.u32(1);				   // sample_description_index
						++nStscEntries;
					}
				}
				ByteBuf astsc;
				astsc.u32(0);
				astsc.u32(nStscEntries);
				astsc.append(astscEntries);

				// stsz : taille constante = un bloc PCM (channels*2 octets).
				ByteBuf astsz;
				astsz.u32(0);
				astsz.u32((uint32)(mAudioChannels * 2));
				astsz.u32((uint32)mAudioFrames);

				// stco : offset de chaque chunk audio.
				ByteBuf astco;
				astco.u32(0);
				astco.u32(nChunks);
				for (uint32 i = 0; i < nChunks; ++i)
					astco.u32(mAudioChunkOff[i]);

				ByteBuf astbl;
				Box(astbl, 's', 't', 's', 'd', astsd);
				Box(astbl, 's', 't', 't', 's', astts);
				Box(astbl, 's', 't', 's', 'c', astsc);
				Box(astbl, 's', 't', 's', 'z', astsz);
				Box(astbl, 's', 't', 'c', 'o', astco);

				// minf audio : smhd + dinf + stbl.
				ByteBuf smhd;
				smhd.u32(0); // version+flags
				smhd.u16(0); // balance
				smhd.u16(0); // reserved
				ByteBuf adrefUrl;
				adrefUrl.u32(0x00000001);
				ByteBuf adref;
				adref.u32(0);
				adref.u32(1);
				Box(adref, 'u', 'r', 'l', ' ', adrefUrl);
				ByteBuf adinf;
				Box(adinf, 'd', 'r', 'e', 'f', adref);
				ByteBuf aminf;
				Box(aminf, 's', 'm', 'h', 'd', smhd);
				Box(aminf, 'd', 'i', 'n', 'f', adinf);
				Box(aminf, 's', 't', 'b', 'l', astbl);

				// mdia audio : mdhd (échelle = sampleRate) + hdlr 'soun' + minf.
				ByteBuf amdhd;
				amdhd.u32(0);
				amdhd.u32(0);
				amdhd.u32(0);
				amdhd.u32((uint32)mAudioRate);
				amdhd.u32((uint32)mAudioFrames);
				amdhd.u16(0x55C4); // language (und)
				amdhd.u16(0);
				ByteBuf ahdlr;
				ahdlr.u32(0);
				ahdlr.u32(0);
				ahdlr.tag('s', 'o', 'u', 'n');
				ahdlr.u32(0);
				ahdlr.u32(0);
				ahdlr.u32(0);
				ahdlr.u8(0); // name (empty pascal)
				ByteBuf amdia;
				Box(amdia, 'm', 'd', 'h', 'd', amdhd);
				Box(amdia, 'h', 'd', 'l', 'r', ahdlr);
				Box(amdia, 'm', 'i', 'n', 'f', aminf);

				// tkhd audio (durée en échelle film, volume plein).
				ByteBuf atkhd;
				atkhd.u32(0x00000007);
				atkhd.u32(0);
				atkhd.u32(0);
				atkhd.u32(2); // track id
				atkhd.u32(0);
				atkhd.u32(aDurMovie);
				atkhd.u32(0);
				atkhd.u32(0);
				atkhd.u16(0);	   // layer
				atkhd.u16(0);	   // alternate group
				atkhd.u16(0x0100); // volume
				atkhd.u16(0);
				for (int i = 0; i < 9; ++i)
					atkhd.u32(mtx[i]);
				atkhd.u32(0); // width
				atkhd.u32(0); // height
				Box(atrak, 't', 'k', 'h', 'd', atkhd);
				Box(atrak, 'm', 'd', 'i', 'a', amdia);
			}

			// ---- mvhd ----
			ByteBuf mvhd;
			mvhd.u32(0);
			mvhd.u32(0);
			mvhd.u32(0);		 // creation/modification
			mvhd.u32(timescale); // timescale
			mvhd.u32(movieDur);	 // duration (max de toutes les pistes)
			mvhd.u32(0x00010000); // preferred rate
			mvhd.u16(0x0100);	 // preferred volume
			mvhd.u16(0);
			mvhd.u32(0);
			mvhd.u32(0); // reserved
			for (int i = 0; i < 9; ++i)
				mvhd.u32(mtx[i]);
			for (int i = 0; i < 6; ++i)
				mvhd.u32(0);				 // pre-defined
			mvhd.u32(hasAudio ? 3u : 2u);	 // next track id

			ByteBuf moov;
			Box(moov, 'm', 'v', 'h', 'd', mvhd);
			Box(moov, 't', 'r', 'a', 'k', trak);
			if (hasAudio)
				Box(moov, 't', 'r', 'a', 'k', atrak);

			ByteBuf out;
			Box(out, 'm', 'o', 'o', 'v', moov);
			mFile.Write(out.d.Data(), out.size());

			mFile.Close();
			return true;
		}

	} // namespace media
} // namespace nkentseu
