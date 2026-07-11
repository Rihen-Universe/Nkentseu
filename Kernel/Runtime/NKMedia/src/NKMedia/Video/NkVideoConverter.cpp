// =============================================================================
// NKMedia/Video/NkVideoConverter.cpp — pipeline de conversion vidéo from-scratch.
// Source (décodage) → RGB → Sink (réencodage). Réutilise NKImage + NkVideoWriter +
// NkH264Encoder. Sélection du format de sortie par extension.
// =============================================================================
#include "NKMedia/Video/NkVideoConverter.h"
#include "NKMedia/Video/NkVideoWriter.h"
#include "NKMedia/Codecs/Video/H264/NkH264Encoder.h"
#include "NKImage/Core/NkImage.h"
#include "NKFileSystem/NkFile.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace media {

		namespace {
			// Compare la fin de `path` à `ext` (insensible à la casse). ex EndsWith("a.MP4",".mp4").
			bool EndsWith(const char *path, const char *ext) {
				int32 lp = 0, le = 0;
				while (path[lp])
					++lp;
				while (ext[le])
					++le;
				if (le > lp)
					return false;
				for (int32 i = 0; i < le; ++i) {
					char a = path[lp - le + i], b = ext[i];
					if (a >= 'A' && a <= 'Z')
						a = (char)(a + 32);
					if (b >= 'A' && b <= 'Z')
						b = (char)(b + 32);
					if (a != b)
						return false;
				}
				return true;
			}

			NkVideoInputFormat FmtOf(int32 channels) {
				return channels == 4 ? NkVideoInputFormat::RGBA32 : NkVideoInputFormat::RGB24;
			}

			// Sink polymorphe (par extension) : H.264 (.mp4/.mov) ou NkVideoWriter (.avi/.m1v/.mpg).
			struct Sink {
					int32 kind = -1; // 0 = H.264, 1 = NkVideoWriter
					NkH264Encoder h264;
					NkVideoWriter vw;

					bool Open(const char *out, int32 w, int32 h, int32 fpsNum, int32 fpsDen, int32 quality) {
						if (EndsWith(out, ".mp4") || EndsWith(out, ".mov")) {
							kind = 0;
							return h264.Open(out, w, h, fpsNum, fpsDen, 26);
						}
						NkVideoConfig cfg;
						cfg.width = w;
						cfg.height = h;
						cfg.fpsNum = fpsNum;
						cfg.fpsDen = fpsDen;
						cfg.quality = quality;
						if (EndsWith(out, ".avi")) {
							cfg.codec = NkVideoCodec::MJPEG;
							cfg.container = NkVideoContainer::AVI;
						} else if (EndsWith(out, ".m1v") || EndsWith(out, ".mpg") || EndsWith(out, ".mpeg")) {
							cfg.codec = NkVideoCodec::MPEG1;
							cfg.container = NkVideoContainer::ELEMENTARY;
						} else {
							return false;
						}
						kind = 1;
						return vw.Open(out, cfg);
					}
					bool Write(const uint8 *rgb, NkVideoInputFormat fmt) {
						return kind == 0 ? h264.WriteFrame(rgb, fmt) : vw.WriteFrame(rgb, fmt);
					}
					bool Close() {
						return kind == 0 ? h264.Close() : vw.Close();
					}
			};

			// Construit "prefix + index (zéro-paddé sur digits) + suffix" dans `out` (max 512).
			void BuildName(char *out, const char *prefix, int32 digits, const char *suffix, int32 index) {
				int32 p = 0;
				for (int32 i = 0; prefix[i] && p < 500; ++i)
					out[p++] = prefix[i];
				char num[16];
				int32 nd = 0, v = index < 0 ? 0 : index;
				if (v == 0)
					num[nd++] = '0';
				while (v > 0) {
					num[nd++] = (char)('0' + v % 10);
					v /= 10;
				}
				for (int32 i = nd; i < digits && p < 500; ++i)
					out[p++] = '0';
				for (int32 i = nd - 1; i >= 0 && p < 500; --i)
					out[p++] = num[i];
				for (int32 i = 0; suffix[i] && p < 511; ++i)
					out[p++] = suffix[i];
				out[p] = 0;
			}

			inline uint32 Rd32LE(const uint8 *b) {
				return (uint32)b[0] | ((uint32)b[1] << 8) | ((uint32)b[2] << 16) | ((uint32)b[3] << 24);
			}
			inline bool Tag4(const uint8 *b, char a, char c, char d, char e) {
				return b[0] == (uint8)a && b[1] == (uint8)c && b[2] == (uint8)d && b[3] == (uint8)e;
			}
		} // namespace

		int32 NkVideoConverter::ImageSequenceToVideo(const char *prefix, int32 digits, const char *suffix, int32 first,
													 int32 count, const char *outPath, int32 fpsNum, int32 fpsDen,
													 int32 quality) {
			Sink sink;
			int32 written = 0;
			char path[512];
			for (int32 f = 0; f < count; ++f) {
				BuildName(path, prefix, digits, suffix, first + f);
				NkImage img;
				if (!img.LoadFromFile(path) || !img.IsValid())
					break; // fin de séquence
				const int32 w = img.Width(), h = img.Height(), ch = img.Channels();
				if (written == 0) {
					if (!sink.Open(outPath, w, h, fpsNum, fpsDen, quality))
						return 0;
				}
				if (!sink.Write(img.Pixels(), FmtOf(ch)))
					break;
				++written;
			}
			if (written > 0)
				sink.Close();
			return written;
		}

		int32 NkVideoConverter::MjpegAviToVideo(const char *aviPath, const char *outPath, int32 quality) {
			// Charge tout l'AVI en mémoire puis parcourt les chunks RIFF.
			NkFile in;
			if (!in.Open(aviPath, NkFileMode::NK_READ_BINARY))
				return 0;
			const usize sz = (usize)in.Size();
			if (sz < 64) {
				in.Close();
				return 0;
			}
			NkVector<uint8> buf;
			buf.Resize((uint64)sz);
			in.Read(buf.Data(), sz);
			in.Close();
			const uint8 *b = buf.Data();
			if (!Tag4(b, 'R', 'I', 'F', 'F') || !Tag4(b + 8, 'A', 'V', 'I', ' '))
				return 0;

			// fps depuis 'avih' (dwMicroSecPerFrame @ +0 des données).
			int32 fpsNum = 25, fpsDen = 1;
			for (usize i = 12; i + 8 < sz; ++i) {
				if (Tag4(b + i, 'a', 'v', 'i', 'h')) {
					const uint32 micros = Rd32LE(b + i + 8);
					if (micros > 0) {
						fpsNum = 1000000;
						fpsDen = (int32)micros;
					}
					break;
				}
			}

			// Trouve la LIST 'movi' puis itère ses chunks '00dc' = trames JPEG.
			usize moviData = 0, moviEnd = 0;
			for (usize i = 12; i + 12 < sz; ++i) {
				if (Tag4(b + i, 'L', 'I', 'S', 'T') && Tag4(b + i + 8, 'm', 'o', 'v', 'i')) {
					const uint32 listSize = Rd32LE(b + i + 4);
					moviData = i + 12;			  // après 'LIST'[4] size[4] 'movi'[4]
					moviEnd = i + 8 + listSize;	  // fin de la LIST
					if (moviEnd > sz)
						moviEnd = sz;
					break;
				}
			}
			if (moviData == 0)
				return 0;

			Sink sink;
			int32 written = 0;
			usize p = moviData;
			while (p + 8 <= moviEnd) {
				const uint32 ck = Rd32LE(b + p + 4); // taille du chunk
				const bool isFrame = Tag4(b + p, '0', '0', 'd', 'c');
				const usize dataOff = p + 8;
				if (isFrame && dataOff + ck <= sz) {
					NkImage img;
					if (img.LoadFromMemory(b + dataOff, (usize)ck) && img.IsValid()) {
						const int32 w = img.Width(), h = img.Height(), c = img.Channels();
						if (written == 0 && !sink.Open(outPath, w, h, fpsNum, fpsDen, quality))
							return 0;
						if (sink.Write(img.Pixels(), FmtOf(c)))
							++written;
					}
				}
				p = dataOff + ck + (ck & 1); // padding à l'octet pair
			}
			if (written > 0)
				sink.Close();
			return written;
		}

	} // namespace media
} // namespace nkentseu
