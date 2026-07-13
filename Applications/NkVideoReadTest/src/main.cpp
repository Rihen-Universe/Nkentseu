// =============================================================================
// NkVideoReadTest — valide la LECTURE vidéo NKMedia (NkVideoReader).
//   Sans argument : self-test (écrit un AVI MJPEG puis le relit et vérifie).
//   Avec un chemin : ouvre le fichier, affiche les infos et décode toutes les images
//   (mesure une somme de contrôle par image pour prouver le décodage frame par frame).
// =============================================================================
#include "NKMedia/Video/NkVideoReader.h"
#include "NKMedia/Codecs/Video/H264/NkH264Decoder.h"
#include "NKMedia/Codecs/Video/H264/NkH264Cavlc.h"

#include <cstdio>
#include <cstring>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::media;

int main(int argc, char **argv) {
	printf("=== NkVideoReadTest — lecture video NKMedia ===\n\n");

	if (argc < 2) {
		printf("  [self-test] ecrire AVI MJPEG -> relire -> verifier...\n");
		bool ok = NkVideoReader::SelfTest();
		printf("  [ %s ] NkVideoReader::SelfTest (AVI MJPEG round-trip)\n", ok ? "OK " : "KO");
		bool okH264 = NkH264Decoder::SelfTest();
		printf("  [ %s ] NkH264Decoder::SelfTest (NAL split + SPS + PPS + slice header I)\n", okH264 ? "OK " : "KO");
		bool okCavlc = NkH264Cavlc::SelfTest();
		printf("  [ %s ] NkH264Cavlc::SelfTest (encode->decode round-trip)\n", okCavlc ? "OK " : "KO");
		bool all = ok && okH264 && okCavlc;
		printf("=== %s ===\n", all ? "LECTURE VIDEO OPERATIONNELLE" : "ECHEC");
		return all ? 0 : 1;
	}

	// Mode décodeur H264 intra : --decode264 <fichier.264> [ref.yuv]
	if (argc >= 3 && strcmp(argv[1], "--decode264") == 0) {
		FILE *f = fopen(argv[2], "rb");
		if (!f) {
			printf("  [KO] fichier introuvable : %s\n", argv[2]);
			return 1;
		}
		fseek(f, 0, SEEK_END);
		long n = ftell(f);
		fseek(f, 0, SEEK_SET);
		NkVector<nk_uint8> buf;
		buf.Resize((uint64)n);
		size_t rd2 = fread(buf.Data(), 1, (size_t)n, f);
		fclose(f);
		(void)rd2;
		NkH264Frame fr;
		if (!NkH264Decoder::DecodeIdrFrame(buf.Data(), (usize)n, fr)) {
			printf("  [KO] DecodeIdrFrame a echoue (CABAC/inter/multi-slice non geres ?)\n");
			return 1;
		}
		printf("  decode IDR : luma %dx%d (crop %dx%d), chroma %dx%d\n", fr.lumaW, fr.lumaH, fr.cropW, fr.cropH,
			   fr.chromaW, fr.chromaH);
		if (argc >= 4) {
			FILE *rf = fopen(argv[3], "rb");
			if (rf) {
				const uint64 yN = (uint64)fr.cropW * fr.cropH;
				const uint64 cN = (uint64)(fr.cropW / 2) * (fr.cropH / 2);
				NkVector<nk_uint8> ref;
				ref.Resize(yN + 2 * cN);
				size_t got = fread(ref.Data(), 1, (size_t)(yN + 2 * cN), rf);
				fclose(rf);
				(void)got;
				// Compare (crop) : Y, puis U, V.
				uint64 diffs = 0, total = 0;
				double sse = 0.0;
				auto cmpPlane = [&](const uint8 *dec, int32 dw, int32 w, int32 h, const uint8 *r) {
					for (int32 y = 0; y < h; ++y)
						for (int32 x = 0; x < w; ++x) {
							int32 a = dec[(usize)y * dw + x], b = r[(usize)y * w + x];
							int32 d = a - b;
							if (d)
								++diffs;
							sse += (double)d * d;
							++total;
						}
				};
				cmpPlane(fr.y.Data(), fr.lumaW, fr.cropW, fr.cropH, ref.Data());
				cmpPlane(fr.cb.Data(), fr.chromaW, fr.cropW / 2, fr.cropH / 2, ref.Data() + yN);
				cmpPlane(fr.cr.Data(), fr.chromaW, fr.cropW / 2, fr.cropH / 2, ref.Data() + yN + cN);
				double psnr = (sse > 0.0) ? 10.0 * log10(255.0 * 255.0 * (double)total / sse) : 999.0;
				printf("  vs ref.yuv : %llu/%llu pixels differents, PSNR=%.2f dB %s\n", (unsigned long long)diffs,
					   (unsigned long long)total, psnr, (diffs == 0) ? "(BIT-EXACT !)" : "");
				return (diffs == 0) ? 0 : 2;
			}
		}
		return 0;
	}

	const char *path = argv[1];
	NkVideoReader rd;
	if (!rd.Open(path)) {
		printf("  [KO] impossible d'ouvrir/lire : %s\n", path);
		printf("       (formats lus : AVI MJPEG/RGB ; MOV/MP4-MJPEG et H264 = a venir)\n");
		return 1;
	}
	const NkVideoReaderInfo &in = rd.Info();
	printf("  ouvert : %s\n", path);
	printf("  conteneur=%s codec=%s  %dx%d  %.2f fps  frames=%d\n", in.container.CStr(), in.codec.CStr(), in.width,
		   in.height, in.fps, in.frameCount);

	int32 count = 0;
	uint64 globalSum = 0;
	NkVideoFrame fr;
	while (rd.ReadFrame(fr)) {
		uint64 s = 0;
		for (uint64 i = 0; i < fr.rgba.Size(); ++i)
			s += fr.rgba[i];
		globalSum += s;
		if (count < 3 || (count % 30) == 0)
			printf("    frame %4d : %dx%d  t=%lldms  checksum=%llu\n", fr.index, fr.width, fr.height,
				   (long long)fr.timestampMs, (unsigned long long)s);
		++count;
	}
	printf("  %d images decodees (somme globale=%llu)\n", count, (unsigned long long)globalSum);
	printf("=== %s ===\n", count > 0 ? "LECTURE OK" : "AUCUNE IMAGE");
	return count > 0 ? 0 : 1;
}
