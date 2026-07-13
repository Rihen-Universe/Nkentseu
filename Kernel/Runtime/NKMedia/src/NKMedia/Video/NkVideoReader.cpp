// =============================================================================
// NKMedia/Video/NkVideoReader.cpp — implémentation (AVI/MJPEG d'abord).
// =============================================================================
#include "NKMedia/Video/NkVideoReader.h"
#include "NKMedia/Video/NkVideoWriter.h" // SelfTest

#include "NKImage/Core/NkImage.h"
#include "NKImage/Codecs/JPEG/NkJPEGCodec.h"
#include "NKFileSystem/NkFile.h"
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

			enum class Backend { NONE, AVI, MOV, SEQUENCE };
			enum class Codec { NONE, MJPEG, RAWRGB, H264 };

			// Référence d'une image encodée dans le buffer fichier (offset+taille).
			struct FrameRef {
					usize offset = 0;
					usize size = 0;
			};

		} // namespace

		struct NkVideoReader::Impl {
				Backend backend = Backend::NONE;
				Codec codec = Codec::NONE;
				NkVector<nk_uint8> bytes;	// fichier complet (AVI/MOV)
				NkVector<FrameRef> frames;	// table des images (offset dans `bytes`)
				NkVideoReaderInfo info;
				int32 cursor = 0;
				int32 bitCount = 24; // RAWRGB

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

				// Décode l'image `index` en RGBA dans `out`.
				bool Decode(int32 index, NkVideoFrame &out) {
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
			return mImpl ? mImpl->cursor : -1;
		}

		bool NkVideoReader::Open(const char *path) {
			if (!mImpl || !path)
				return false;
			Close();

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

			// ISOBMFF (....ftyp) => MOV/MP4 : lecture vidéo à venir (itération suivante).
			if (mImpl->bytes.Size() >= 12 && Tag(d + 4, 'f', 't', 'y', 'p')) {
				// TODO(next) : démux piste vidéo ISOBMFF + MJPEG ; H264 = décodeur dédié.
				return false;
			}

			return false;
		}

		bool NkVideoReader::ReadFrame(NkVideoFrame &out) {
			if (!IsOpen())
				return false;
			if (mImpl->cursor >= (int32)mImpl->frames.Size())
				return false;
			if (!mImpl->Decode(mImpl->cursor, out))
				return false;
			out.timestampMs = (mImpl->info.fps > 0.0) ? (int64)((double)mImpl->cursor * 1000.0 / mImpl->info.fps) : 0;
			mImpl->cursor++;
			return true;
		}

		bool NkVideoReader::SeekFrame(int32 index) {
			if (!IsOpen() || index < 0 || index >= (int32)mImpl->frames.Size())
				return false;
			mImpl->cursor = index;
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
