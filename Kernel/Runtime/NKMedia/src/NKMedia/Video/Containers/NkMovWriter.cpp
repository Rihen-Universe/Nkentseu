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

			// ---- mvhd ----
			ByteBuf mvhd;
			mvhd.u32(0);
			mvhd.u32(0);
			mvhd.u32(0);		 // creation/modification
			mvhd.u32(timescale); // timescale
			mvhd.u32(duration);	 // duration
			mvhd.u32(0x00010000); // preferred rate
			mvhd.u16(0x0100);	 // preferred volume
			mvhd.u16(0);
			mvhd.u32(0);
			mvhd.u32(0); // reserved
			for (int i = 0; i < 9; ++i)
				mvhd.u32(mtx[i]);
			for (int i = 0; i < 6; ++i)
				mvhd.u32(0); // pre-defined
			mvhd.u32(2);	 // next track id

			ByteBuf moov;
			Box(moov, 'm', 'v', 'h', 'd', mvhd);
			Box(moov, 't', 'r', 'a', 'k', trak);

			ByteBuf out;
			Box(out, 'm', 'o', 'o', 'v', moov);
			mFile.Write(out.d.Data(), out.size());

			mFile.Close();
			return true;
		}

	} // namespace media
} // namespace nkentseu
