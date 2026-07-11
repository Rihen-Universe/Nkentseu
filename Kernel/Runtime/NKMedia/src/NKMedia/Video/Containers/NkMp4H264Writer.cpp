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

		namespace {
			// Code de langue ISO-639-2 (3 lettres minuscules) → 15 bits (5 bits/lettre, - 0x60).
			uint16 PackLang(const char *l) {
				if (!l || !l[0] || !l[1] || !l[2])
					return 0x55C4; // 'und'
				return (uint16)((((int32)l[0] - 0x60) << 10) | (((int32)l[1] - 0x60) << 5) | ((int32)l[2] - 0x60));
			}
		} // namespace

		int32 NkMp4H264Writer::AddAudioTrack(int32 sampleRate, int32 channels, const char *lang3) {
			if (mNumAudio >= kMaxAudio || sampleRate <= 0 || channels <= 0)
				return -1;
			const int32 idx = mNumAudio++;
			mAudio[idx].rate = sampleRate;
			mAudio[idx].channels = channels;
			mAudio[idx].lang = PackLang(lang3);
			return idx;
		}

		void NkMp4H264Writer::AppendAudioPcm(int32 trackIdx, const int16 *interleaved, uint32 frames) {
			if (trackIdx < 0 || trackIdx >= mNumAudio)
				return;
			AudioTrk &a = mAudio[trackIdx];
			const uint32 n = frames * (uint32)a.channels;
			for (uint32 i = 0; i < n; ++i) {
				const uint16 v = (uint16)interleaved[i];
				a.pcm.PushBack((uint8)(v & 0xFF)); // little-endian (sowt)
				a.pcm.PushBack((uint8)((v >> 8) & 0xFF));
			}
		}

		int32 NkMp4H264Writer::AddSubtitleTrack(const char *lang3) {
			if (mNumSubs >= kMaxSubs)
				return -1;
			const int32 idx = mNumSubs++;
			mSubs[idx].lang = PackLang(lang3);
			return idx;
		}

		void NkMp4H264Writer::AddSubtitle(int32 trackIdx, const char *utf8, uint32 startMs, uint32 durMs) {
			if (trackIdx < 0 || trackIdx >= mNumSubs || !utf8)
				return;
			SubTrk &s = mSubs[trackIdx];
			// trou avant le sous-titre → échantillon vide (texte de longueur 0).
			if (startMs > s.lastEndMs) {
				s.data.PushBack(0);
				s.data.PushBack(0);
				s.sizes.PushBack(2);
				s.durs.PushBack(startMs - s.lastEndMs);
			}
			uint32 len = 0;
			while (utf8[len])
				++len;
			s.data.PushBack((uint8)((len >> 8) & 0xFF)); // u16 longueur texte (big-endian)
			s.data.PushBack((uint8)(len & 0xFF));
			for (uint32 i = 0; i < len; ++i)
				s.data.PushBack((uint8)utf8[i]);
			s.sizes.PushBack(2 + len);
			s.durs.PushBack(durMs);
			s.lastEndMs = startMs + durMs;
		}

		bool NkMp4H264Writer::Close() {
			if (!mFile.IsOpen())
				return false;
			if (mSps.Size() < 4 || mPps.Size() == 0)
				return false; // avcC exige SPS (≥4 octets) + PPS

			const uint32 nSamples = (uint32)mSizes.Size();
			const uint32 movieTs = 1000; // échelle de temps du film (ms)

			// ---- écrit dans mdat (après la vidéo) : blocs PCM audio puis blocs sous-titres ----
			for (int32 t = 0; t < mNumAudio; ++t) {
				AudioTrk &a = mAudio[t];
				a.offset = (uint32)mFile.Tell();
				if (a.pcm.Size() > 0)
					mFile.Write(a.pcm.Data(), (usize)a.pcm.Size());
				a.frames = (uint32)(a.pcm.Size() / (2 * (uint64)a.channels));
			}
			for (int32 t = 0; t < mNumSubs; ++t) {
				SubTrk &s = mSubs[t];
				s.offset = (uint32)mFile.Tell();
				if (s.data.Size() > 0)
					mFile.Write(s.data.Data(), (usize)s.data.Size());
			}

			const nk_int64 mdatEnd = mFile.Tell();
			// Rapièce la taille de mdat (couvre vidéo + audio + sous-titres).
			const uint32 mdatSize = (uint32)(mdatEnd - mMdatSizePos);
			mFile.Seek(mMdatSizePos, NkSeekOrigin::NK_BEGIN);
			uint8 sz[4] = {(uint8)(mdatSize >> 24), (uint8)(mdatSize >> 16), (uint8)(mdatSize >> 8), (uint8)mdatSize};
			mFile.Write(sz, 4);
			mFile.Seek(mdatEnd, NkSeekOrigin::NK_BEGIN);

			// Durées de piste → échelle film ; durée du film = max de toutes les pistes.
			const uint32 vTrackTs = (uint32)mFpsNum;
			const uint32 vDurTrack = nSamples * (uint32)mFpsDen; // en échelle piste vidéo
			const uint32 vDurMovie = (uint32)((uint64)nSamples * (uint64)mFpsDen * movieTs / (uint64)mFpsNum);
			uint32 movieDur = vDurMovie;
			for (int32 t = 0; t < mNumAudio; ++t) {
				const uint32 d = (uint32)((uint64)mAudio[t].frames * movieTs / (uint64)mAudio[t].rate);
				if (d > movieDur)
					movieDur = d;
			}
			for (int32 t = 0; t < mNumSubs; ++t) {
				if (mSubs[t].lastEndMs > movieDur)
					movieDur = mSubs[t].lastEndMs;
			}

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

			// ---- pistes supplémentaires : audio (langues) puis sous-titres (tx3g) ----
			ByteBuf extraTraks;
			int32 trackId = 2;

			for (int32 t = 0; t < mNumAudio; ++t) {
				const AudioTrk &a = mAudio[t];
				const uint32 aDurMovie = (uint32)((uint64)a.frames * movieTs / (uint64)a.rate);
				ByteBuf aud; // sample entry 'sowt' (AudioSampleEntry v0)
				for (int i = 0; i < 6; ++i)
					aud.u8v(0);
				aud.u16(1);
				aud.u32(0);
				aud.u32(0);
				aud.u16((uint16)a.channels);
				aud.u16(16);
				aud.u16(0);
				aud.u16(0);
				aud.u32((uint32)a.rate << 16);
				ByteBuf sowt;
				Box(sowt, 's', 'o', 'w', 't', aud);
				ByteBuf astsd;
				astsd.u32(0);
				astsd.u32(1);
				astsd.append(sowt);
				ByteBuf astts;
				astts.u32(0);
				astts.u32(1);
				astts.u32(a.frames);
				astts.u32(1);
				ByteBuf astsc;
				astsc.u32(0);
				astsc.u32(1);
				astsc.u32(1);
				astsc.u32(a.frames);
				astsc.u32(1);
				ByteBuf astsz;
				astsz.u32(0);
				astsz.u32((uint32)(a.channels * 2));
				astsz.u32(a.frames);
				ByteBuf astco;
				astco.u32(0);
				astco.u32(1);
				astco.u32(a.offset);
				ByteBuf astbl;
				Box(astbl, 's', 't', 's', 'd', astsd);
				Box(astbl, 's', 't', 't', 's', astts);
				Box(astbl, 's', 't', 's', 'c', astsc);
				Box(astbl, 's', 't', 's', 'z', astsz);
				Box(astbl, 's', 't', 'c', 'o', astco);
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
				ByteBuf amdhd;
				amdhd.u32(0);
				amdhd.u32(0);
				amdhd.u32(0);
				amdhd.u32((uint32)a.rate);
				amdhd.u32(a.frames);
				amdhd.u16(a.lang); // langue ISO-639-2
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
				ByteBuf atkhd;
				atkhd.u32(0x00000007);
				atkhd.u32(0);
				atkhd.u32(0);
				atkhd.u32((uint32)trackId++);
				atkhd.u32(0);
				atkhd.u32(aDurMovie);
				atkhd.u32(0);
				atkhd.u32(0);
				atkhd.u16(0);	   // layer
				atkhd.u16(1);	   // alternate_group = 1 (les pistes audio sont des alternatives = langues)
				atkhd.u16(0x0100); // volume
				atkhd.u16(0);
				for (int i = 0; i < 9; ++i)
					atkhd.u32(mtx[i]);
				atkhd.u32(0);
				atkhd.u32(0);
				ByteBuf atrak;
				Box(atrak, 't', 'k', 'h', 'd', atkhd);
				Box(atrak, 'm', 'd', 'i', 'a', amdia);
				Box(extraTraks, 't', 'r', 'a', 'k', atrak);
			}

			for (int32 t = 0; t < mNumSubs; ++t) {
				const SubTrk &s = mSubs[t];
				const uint32 nSub = (uint32)s.sizes.Size();
				// sample entry 'tx3g' (TextSampleEntry) minimal + ftab.
				ByteBuf tx;
				for (int i = 0; i < 6; ++i)
					tx.u8v(0);
				tx.u16(1);	   // data_reference_index
				tx.u32(0);	   // displayFlags
				tx.u8v(1);	   // horizontal-justification (centre)
				tx.u8v(0xFF);  // vertical-justification (bas)
				tx.u8v(0);	   // background RGBA
				tx.u8v(0);
				tx.u8v(0);
				tx.u8v(0);
				tx.u16(0);	   // BoxRecord top/left/bottom/right
				tx.u16(0);
				tx.u16(0);
				tx.u16(0);
				tx.u16(0);	   // StyleRecord startChar
				tx.u16(0);	   // endChar
				tx.u16(1);	   // fontID
				tx.u8v(0);	   // faceStyleFlags
				tx.u8v(18);	   // fontSize
				tx.u8v(255);   // texte RGBA (blanc opaque)
				tx.u8v(255);
				tx.u8v(255);
				tx.u8v(255);
				ByteBuf ftab; // FontTableBox
				ftab.u16(1);  // entry count
				ftab.u16(1);  // fontID
				ftab.u8v(5);  // longueur nom
				ftab.u8v('S');
				ftab.u8v('e');
				ftab.u8v('r');
				ftab.u8v('i');
				ftab.u8v('f');
				Box(tx, 'f', 't', 'a', 'b', ftab);
				ByteBuf tx3g;
				Box(tx3g, 't', 'x', '3', 'g', tx);
				ByteBuf ststd;
				ststd.u32(0);
				ststd.u32(1);
				ststd.append(tx3g);
				// stts : une entrée par échantillon (durée en ms).
				ByteBuf ststts;
				ststts.u32(0);
				ststts.u32(nSub);
				for (uint32 i = 0; i < nSub; ++i) {
					ststts.u32(1);
					ststts.u32(s.durs[i]);
				}
				ByteBuf ststsc;
				ststsc.u32(0);
				ststsc.u32(1);
				ststsc.u32(1);
				ststsc.u32(nSub);
				ststsc.u32(1);
				ByteBuf ststsz;
				ststsz.u32(0);
				ststsz.u32(0); // tailles variables
				ststsz.u32(nSub);
				for (uint32 i = 0; i < nSub; ++i)
					ststsz.u32(s.sizes[i]);
				ByteBuf ststco;
				ststco.u32(0);
				ststco.u32(1);
				ststco.u32(s.offset);
				ByteBuf ststbl;
				Box(ststbl, 's', 't', 's', 'd', ststd);
				Box(ststbl, 's', 't', 't', 's', ststts);
				Box(ststbl, 's', 't', 's', 'c', ststsc);
				Box(ststbl, 's', 't', 's', 'z', ststsz);
				Box(ststbl, 's', 't', 'c', 'o', ststco);
				ByteBuf nmhd;
				nmhd.u32(0); // Null Media Header
				ByteBuf sdrefUrl;
				sdrefUrl.u32(0x00000001);
				ByteBuf sdref;
				sdref.u32(0);
				sdref.u32(1);
				Box(sdref, 'u', 'r', 'l', ' ', sdrefUrl);
				ByteBuf sdinf;
				Box(sdinf, 'd', 'r', 'e', 'f', sdref);
				ByteBuf sminf;
				Box(sminf, 'n', 'm', 'h', 'd', nmhd);
				Box(sminf, 'd', 'i', 'n', 'f', sdinf);
				Box(sminf, 's', 't', 'b', 'l', ststbl);
				ByteBuf smdhd;
				smdhd.u32(0);
				smdhd.u32(0);
				smdhd.u32(0);
				smdhd.u32(movieTs); // échelle = ms
				smdhd.u32(s.lastEndMs);
				smdhd.u16(s.lang);
				smdhd.u16(0);
				ByteBuf shdlr;
				shdlr.u32(0);
				shdlr.u32(0);
				shdlr.tag('t', 'e', 'x', 't');
				shdlr.u32(0);
				shdlr.u32(0);
				shdlr.u32(0);
				const char *shn = "SubtitleHandler";
				for (int i = 0; i < 15; ++i)
					shdlr.u8v((uint8)shn[i]);
				shdlr.u8v(0);
				ByteBuf smdia;
				Box(smdia, 'm', 'd', 'h', 'd', smdhd);
				Box(smdia, 'h', 'd', 'l', 'r', shdlr);
				Box(smdia, 'm', 'i', 'n', 'f', sminf);
				ByteBuf stkhd;
				stkhd.u32(0x00000007);
				stkhd.u32(0);
				stkhd.u32(0);
				stkhd.u32((uint32)trackId++);
				stkhd.u32(0);
				stkhd.u32(s.lastEndMs);
				stkhd.u32(0);
				stkhd.u32(0);
				stkhd.u16(0); // layer
				stkhd.u16(2); // alternate_group = 2 (sous-titres alternatifs = langues)
				stkhd.u16(0); // volume
				stkhd.u16(0);
				for (int i = 0; i < 9; ++i)
					stkhd.u32(mtx[i]);
				stkhd.u32((uint32)mWidth << 16);
				stkhd.u32((uint32)mHeight << 16);
				ByteBuf strak;
				Box(strak, 't', 'k', 'h', 'd', stkhd);
				Box(strak, 'm', 'd', 'i', 'a', smdia);
				Box(extraTraks, 't', 'r', 'a', 'k', strak);
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
			mvhd.u32((uint32)trackId); // next_track_id

			ByteBuf moov;
			Box(moov, 'm', 'v', 'h', 'd', mvhd);
			Box(moov, 't', 'r', 'a', 'k', trak);
			moov.append(extraTraks);

			ByteBuf out;
			Box(out, 'm', 'o', 'o', 'v', moov);
			mFile.Write(out.d.Data(), out.size());

			mFile.Close();
			return true;
		}

	} // namespace media
} // namespace nkentseu
