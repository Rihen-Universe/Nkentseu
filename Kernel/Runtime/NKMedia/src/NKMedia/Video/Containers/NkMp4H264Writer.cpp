// =============================================================================
// NKMedia/Video/Containers/NkMp4H264Writer.cpp — muxer MP4 (ISOBMFF) H.264 (avc1).
// ftyp (isom) + mdat (NAL longueur-préfixées) + moov (avc1/avcC + stbl + stss).
// Boxes big-endian. Réutilise le motif ByteBuf/Box de NkMovWriter.
// =============================================================================
#include "NKMedia/Video/Containers/NkMp4H264Writer.h"

namespace nkentseu {
	namespace media {

		namespace {
			struct ByteBuf {
					NkVector<uint8> d;
					void u8v(uint8 v) {
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

			void Box(ByteBuf &out, char a, char b, char c, char e, const ByteBuf &payload) {
				out.u32((uint32)(8 + payload.size()));
				out.tag(a, b, c, e);
				out.append(payload);
			}
		} // namespace

		bool NkMp4H264Writer::Open(const char *path, int32 width, int32 height, int32 fpsNum, int32 fpsDen) {
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
			mSync.Clear();

			// ---- ftyp (isom + marques compatibles avc1) ----
			ByteBuf ftyp;
			ftyp.tag('i', 's', 'o', 'm');
			ftyp.u32(0x00000200);
			ftyp.tag('i', 's', 'o', 'm');
			ftyp.tag('i', 's', 'o', '2');
			ftyp.tag('a', 'v', 'c', '1');
			ftyp.tag('m', 'p', '4', '1');
			ByteBuf out;
			Box(out, 'f', 't', 'y', 'p', ftyp);
			mFile.Write(out.d.Data(), out.size());

			// ---- mdat (streaming) : taille rapiécée à la fermeture ----
			mMdatSizePos = mFile.Tell();
			uint8 hdr[8] = {0, 0, 0, 0, 'm', 'd', 'a', 't'};
			mFile.Write(hdr, 8);
			mMdatStart = mFile.Tell();
			return true;
		}

		void NkMp4H264Writer::SetSps(const uint8 *data, uint32 size) {
			mSps.Clear();
			for (uint32 i = 0; i < size; ++i)
				mSps.PushBack(data[i]);
		}
		void NkMp4H264Writer::SetPps(const uint8 *data, uint32 size) {
			mPps.Clear();
			for (uint32 i = 0; i < size; ++i)
				mPps.PushBack(data[i]);
		}

		bool NkMp4H264Writer::WriteSample(const uint8 *data, uint32 size, bool sync) {
			if (!mFile.IsOpen() || data == nullptr)
				return false;
			const nk_int64 off = mFile.Tell();
			mFile.Write(data, size);
			mOffsets.PushBack((uint32)off);
			mSizes.PushBack(size);
			mSync.PushBack(sync ? 1 : 0);
			return true;
		}

		void NkMp4H264Writer::SetAudio(int32 sampleRate, int32 channels) {
			mAudioRate = sampleRate;
			mAudioChannels = channels;
		}

		void NkMp4H264Writer::AppendAudioPcm(const int16 *interleaved, uint32 frames) {
			if (mAudioChannels <= 0)
				return;
			const uint32 n = frames * (uint32)mAudioChannels;
			for (uint32 i = 0; i < n; ++i) {
				const int16 v = interleaved[i];
				mAudioPcm.PushBack((uint8)((uint16)v & 0xFF)); // little-endian (sowt)
				mAudioPcm.PushBack((uint8)(((uint16)v >> 8) & 0xFF));
			}
		}

		bool NkMp4H264Writer::Close() {
			if (!mFile.IsOpen())
				return false;
			if (mSps.Size() < 4 || mPps.Size() == 0)
				return false; // avcC exige SPS (≥4 octets) + PPS

			const uint32 nSamples = (uint32)mSizes.Size();

			// ---- audio : écrit le bloc PCM contigu dans mdat (après la vidéo) ----
			const bool hasAudio = mAudioChannels > 0 && mAudioRate > 0 && mAudioPcm.Size() > 0;
			uint32 audioOffset = 0, audioFrames = 0;
			if (hasAudio) {
				audioOffset = (uint32)mFile.Tell();
				mFile.Write(mAudioPcm.Data(), (usize)mAudioPcm.Size());
				audioFrames = (uint32)(mAudioPcm.Size() / (2 * (uint64)mAudioChannels));
			}

			const nk_int64 mdatEnd = mFile.Tell();
			// Rapièce la taille de mdat (couvre vidéo + audio).
			const uint32 mdatSize = (uint32)(mdatEnd - mMdatSizePos);
			mFile.Seek(mMdatSizePos, NkSeekOrigin::NK_BEGIN);
			uint8 sz[4] = {(uint8)(mdatSize >> 24), (uint8)(mdatSize >> 16), (uint8)(mdatSize >> 8), (uint8)mdatSize};
			mFile.Write(sz, 4);
			mFile.Seek(mdatEnd, NkSeekOrigin::NK_BEGIN);

			// Échelle de temps du film = 1000 (ms) ; durées de piste converties dedans.
			const uint32 movieTs = 1000;
			const uint32 vTrackTs = (uint32)mFpsNum;
			const uint32 vDurTrack = nSamples * (uint32)mFpsDen; // en échelle piste vidéo
			const uint32 vDurMovie = (uint32)((uint64)nSamples * (uint64)mFpsDen * movieTs / (uint64)mFpsNum);
			const uint32 aDurMovie =
				hasAudio ? (uint32)((uint64)audioFrames * movieTs / (uint64)mAudioRate) : 0;
			const uint32 movieDur = vDurMovie > aDurMovie ? vDurMovie : aDurMovie;

			// ---- avcC (AVCDecoderConfigurationRecord) ----
			ByteBuf avcc;
			avcc.u8v(1);			// configurationVersion
			avcc.u8v(mSps[1]);		// AVCProfileIndication (profile_idc)
			avcc.u8v(mSps[2]);		// profile_compatibility (constraints)
			avcc.u8v(mSps[3]);		// AVCLevelIndication (level_idc)
			avcc.u8v(0xFF);			// 6 bits réservés + lengthSizeMinusOne = 3 (NAL sur 4 octets)
			avcc.u8v(0xE1);			// 3 bits réservés + numOfSequenceParameterSets = 1
			avcc.u16((uint16)mSps.Size());
			avcc.bytes(mSps.Data(), (usize)mSps.Size());
			avcc.u8v(1); // numOfPictureParameterSets
			avcc.u16((uint16)mPps.Size());
			avcc.bytes(mPps.Data(), (usize)mPps.Size());

			// ---- stsd → avc1 (VisualSampleEntry) + avcC ----
			ByteBuf visual;
			for (int i = 0; i < 6; ++i)
				visual.u8v(0);	  // reserved
			visual.u16(1);		  // data_reference_index
			visual.u16(0);		  // pre_defined
			visual.u16(0);		  // reserved
			visual.u32(0);		  // pre_defined[3]
			visual.u32(0);
			visual.u32(0);
			visual.u16((uint16)mWidth);
			visual.u16((uint16)mHeight);
			visual.u32(0x00480000); // horizresolution 72dpi
			visual.u32(0x00480000); // vertresolution
			visual.u32(0);			// reserved
			visual.u16(1);			// frame_count
			const char *cname = "Nkentseu H264";
			visual.u8v((uint8)13);
			for (int i = 0; i < 31; ++i)
				visual.u8v(i < 13 ? (uint8)cname[i] : 0);
			visual.u16(0x0018); // depth
			visual.u16(0xFFFF); // pre_defined = -1
			Box(visual, 'a', 'v', 'c', 'C', avcc);
			ByteBuf avc1;
			Box(avc1, 'a', 'v', 'c', '1', visual);
			ByteBuf stsd;
			stsd.u32(0); // version+flags
			stsd.u32(1); // entry count
			stsd.append(avc1);

			// stts : nSamples de durée mFpsDen (échelle piste vidéo = mFpsNum).
			ByteBuf stts;
			stts.u32(0);
			stts.u32(1);
			stts.u32(nSamples);
			stts.u32((uint32)mFpsDen);

			// stsc : un échantillon par chunk.
			ByteBuf stsc;
			stsc.u32(0);
			stsc.u32(1);
			stsc.u32(1);
			stsc.u32(1);
			stsc.u32(1);

			// stsz : taille par échantillon.
			ByteBuf stsz;
			stsz.u32(0);
			stsz.u32(0);
			stsz.u32(nSamples);
			for (uint32 i = 0; i < nSamples; ++i)
				stsz.u32(mSizes[i]);

			// stco : offsets.
			ByteBuf stco;
			stco.u32(0);
			stco.u32(nSamples);
			for (uint32 i = 0; i < nSamples; ++i)
				stco.u32(mOffsets[i]);

			// stss : échantillons synchro (IDR), numéros 1-based.
			ByteBuf syncList;
			uint32 nSync = 0;
			for (uint32 i = 0; i < nSamples; ++i)
				if (mSync[i]) {
					syncList.u32(i + 1);
					++nSync;
				}
			ByteBuf stss;
			stss.u32(0);
			stss.u32(nSync);
			stss.append(syncList);

			ByteBuf stbl;
			Box(stbl, 's', 't', 's', 'd', stsd);
			Box(stbl, 's', 't', 't', 's', stts);
			Box(stbl, 's', 't', 's', 'c', stsc);
			Box(stbl, 's', 't', 's', 'z', stsz);
			Box(stbl, 's', 't', 'c', 'o', stco);
			if (nSync > 0 && nSync < nSamples)
				Box(stbl, 's', 't', 's', 's', stss); // sinon toutes synchro → box omise

			// ---- minf ----
			ByteBuf vmhd;
			vmhd.u32(0x00000001);
			vmhd.u16(0);
			vmhd.u16(0);
			vmhd.u16(0);
			vmhd.u16(0);
			ByteBuf drefUrl;
			drefUrl.u32(0x00000001);
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
			mdhd.u32(0);
			mdhd.u32(0);
			mdhd.u32(0);
			mdhd.u32(vTrackTs);
			mdhd.u32(vDurTrack);
			mdhd.u16(0x55C4); // langue (und)
			mdhd.u16(0);
			ByteBuf hdlr;
			hdlr.u32(0);
			hdlr.u32(0);
			hdlr.tag('v', 'i', 'd', 'e');
			hdlr.u32(0);
			hdlr.u32(0);
			hdlr.u32(0);
			const char *hn = "VideoHandler";
			for (int i = 0; i < 12; ++i)
				hdlr.u8v((uint8)hn[i]);
			hdlr.u8v(0);
			ByteBuf mdia;
			Box(mdia, 'm', 'd', 'h', 'd', mdhd);
			Box(mdia, 'h', 'd', 'l', 'r', hdlr);
			Box(mdia, 'm', 'i', 'n', 'f', minf);

			// ---- tkhd ----
			const uint32 mtx[9] = {0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};
			ByteBuf tkhd;
			tkhd.u32(0x00000007);
			tkhd.u32(0);
			tkhd.u32(0);
			tkhd.u32(1); // track_id = 1 (vidéo)
			tkhd.u32(0);
			tkhd.u32(vDurMovie);
			tkhd.u32(0);
			tkhd.u32(0);
			tkhd.u16(0);
			tkhd.u16(0);
			tkhd.u16(0);
			tkhd.u16(0);
			for (int i = 0; i < 9; ++i)
				tkhd.u32(mtx[i]);
			tkhd.u32((uint32)mWidth << 16);
			tkhd.u32((uint32)mHeight << 16);
			ByteBuf trak;
			Box(trak, 't', 'k', 'h', 'd', tkhd);
			Box(trak, 'm', 'd', 'i', 'a', mdia);

			// ---- piste audio (LPCM 'sowt') si présente ----
			ByteBuf audioTrak;
			if (hasAudio) {
				// sample entry 'sowt' (AudioSampleEntry v0)
				ByteBuf aud;
				for (int i = 0; i < 6; ++i)
					aud.u8v(0);
				aud.u16(1); // data_reference_index
				aud.u32(0);
				aud.u32(0); // reserved
				aud.u16((uint16)mAudioChannels);
				aud.u16(16); // bits par échantillon
				aud.u16(0);	 // pre_defined
				aud.u16(0);	 // reserved
				aud.u32((uint32)mAudioRate << 16);
				ByteBuf sowt;
				Box(sowt, 's', 'o', 'w', 't', aud);
				ByteBuf astsd;
				astsd.u32(0);
				astsd.u32(1);
				astsd.append(sowt);
				// stts : audioFrames de durée 1 (échelle = mAudioRate)
				ByteBuf astts;
				astts.u32(0);
				astts.u32(1);
				astts.u32(audioFrames);
				astts.u32(1);
				// stsc : tout en 1 chunk
				ByteBuf astsc;
				astsc.u32(0);
				astsc.u32(1);
				astsc.u32(1);
				astsc.u32(audioFrames);
				astsc.u32(1);
				// stsz : taille constante = canaux*2
				ByteBuf astsz;
				astsz.u32(0);
				astsz.u32((uint32)(mAudioChannels * 2));
				astsz.u32(audioFrames);
				// stco : offset du bloc audio
				ByteBuf astco;
				astco.u32(0);
				astco.u32(1);
				astco.u32(audioOffset);
				ByteBuf astbl;
				Box(astbl, 's', 't', 's', 'd', astsd);
				Box(astbl, 's', 't', 't', 's', astts);
				Box(astbl, 's', 't', 's', 'c', astsc);
				Box(astbl, 's', 't', 's', 'z', astsz);
				Box(astbl, 's', 't', 'c', 'o', astco);
				// smhd + dinf + stbl → minf
				ByteBuf smhd;
				smhd.u32(0);
				smhd.u16(0);
				smhd.u16(0);
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
				// mdhd + hdlr(soun) + minf → mdia
				ByteBuf amdhd;
				amdhd.u32(0);
				amdhd.u32(0);
				amdhd.u32(0);
				amdhd.u32((uint32)mAudioRate);
				amdhd.u32(audioFrames);
				amdhd.u16(0x55C4);
				amdhd.u16(0);
				ByteBuf ahdlr;
				ahdlr.u32(0);
				ahdlr.u32(0);
				ahdlr.tag('s', 'o', 'u', 'n');
				ahdlr.u32(0);
				ahdlr.u32(0);
				ahdlr.u32(0);
				const char *ahn = "SoundHandler";
				for (int i = 0; i < 12; ++i)
					ahdlr.u8v((uint8)ahn[i]);
				ahdlr.u8v(0);
				ByteBuf amdia;
				Box(amdia, 'm', 'd', 'h', 'd', amdhd);
				Box(amdia, 'h', 'd', 'l', 'r', ahdlr);
				Box(amdia, 'm', 'i', 'n', 'f', aminf);
				// tkhd audio (track_id=2, largeur/hauteur 0, volume plein)
				ByteBuf atkhd;
				atkhd.u32(0x00000007);
				atkhd.u32(0);
				atkhd.u32(0);
				atkhd.u32(2);
				atkhd.u32(0);
				atkhd.u32(aDurMovie);
				atkhd.u32(0);
				atkhd.u32(0);
				atkhd.u16(0);
				atkhd.u16(0);
				atkhd.u16(0x0100); // volume
				atkhd.u16(0);
				for (int i = 0; i < 9; ++i)
					atkhd.u32(mtx[i]);
				atkhd.u32(0);
				atkhd.u32(0); // largeur/hauteur = 0 (audio)
				Box(audioTrak, 't', 'k', 'h', 'd', atkhd);
				Box(audioTrak, 'm', 'd', 'i', 'a', amdia);
			}

			// ---- mvhd ----
			ByteBuf mvhd;
			mvhd.u32(0);
			mvhd.u32(0);
			mvhd.u32(0);
			mvhd.u32(movieTs);
			mvhd.u32(movieDur);
			mvhd.u32(0x00010000);
			mvhd.u16(0x0100);
			mvhd.u16(0);
			mvhd.u32(0);
			mvhd.u32(0);
			for (int i = 0; i < 9; ++i)
				mvhd.u32(mtx[i]);
			for (int i = 0; i < 6; ++i)
				mvhd.u32(0);
			mvhd.u32(hasAudio ? 3 : 2); // next_track_id

			ByteBuf moov;
			Box(moov, 'm', 'v', 'h', 'd', mvhd);
			Box(moov, 't', 'r', 'a', 'k', trak);
			if (hasAudio)
				Box(moov, 't', 'r', 'a', 'k', audioTrak);

			ByteBuf out;
			Box(out, 'm', 'o', 'o', 'v', moov);
			mFile.Write(out.d.Data(), out.size());

			mFile.Close();
			return true;
		}

	} // namespace media
} // namespace nkentseu
