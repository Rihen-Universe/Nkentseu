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
		// Split en NALs (start codes). Collecte SPS/PPS + les slices (type 1/5) dans l'ordre.
		NkVector<nk_uint8> sps, pps;
		struct Slice {
				usize off, len;
		};
		NkVector<Slice> slices;
		{
			usize i = 0, prev = (usize)-1;
			auto isSC = [&](usize p) {
				return p + 2 < (usize)n && buf[p] == 0 && buf[p + 1] == 0 && buf[p + 2] == 1;
			};
			while (i + 2 < (usize)n) {
				if (isSC(i)) {
					const usize ns = i + 3;
					if (prev != (usize)-1) {
						usize e = i;
						while (e > prev && buf[e - 1] == 0)
							--e;
						const int32 t = buf[prev] & 0x1F;
						if (t == 7) {
							sps.Clear();
							for (usize k = prev; k < e; ++k)
								sps.PushBack(buf[k]);
						} else if (t == 8) {
							pps.Clear();
							for (usize k = prev; k < e; ++k)
								pps.PushBack(buf[k]);
						} else if (t == 5 || t == 1) {
							Slice s;
							s.off = prev;
							s.len = e - prev;
							slices.PushBack(s);
						}
					}
					prev = ns;
					i = ns;
				} else
					++i;
			}
			if (prev != (usize)-1) {
				const int32 t = buf[prev] & 0x1F;
				if (t == 5 || t == 1) {
					Slice s;
					s.off = prev;
					s.len = (usize)n - prev;
					slices.PushBack(s);
				}
			}
		}
		printf("  %d slices (SPS %llu o, PPS %llu o)\n", (int)slices.Size(), (unsigned long long)sps.Size(),
			   (unsigned long long)pps.Size());

		FILE *rf = (argc >= 4) ? fopen(argv[3], "rb") : nullptr;
		NkH264Frame cur;
		NkVector<NkH264Frame> dpb; // RefPicList0 : dpb[0] = image la plus recente
		uint64 totalDiffs = 0;
		for (uint64 si = 0; si < slices.Size(); ++si) {
			NkVector<nk_uint8> ab;
			auto sc = [&]() {
				ab.PushBack(0);
				ab.PushBack(0);
				ab.PushBack(0);
				ab.PushBack(1);
			};
			sc();
			for (uint64 k = 0; k < sps.Size(); ++k)
				ab.PushBack(sps[k]);
			sc();
			for (uint64 k = 0; k < pps.Size(); ++k)
				ab.PushBack(pps[k]);
			sc();
			for (usize k = 0; k < slices[si].len; ++k)
				ab.PushBack(buf[slices[si].off + k]);

			NkVector<const NkH264Frame *> refs;
			for (uint64 k = 0; k < dpb.Size(); ++k)
				refs.PushBack(&dpb[k]);
			if (!NkH264Decoder::DecodeFrame(ab.Data(), (usize)ab.Size(), refs.Data(), (int32)refs.Size(), cur)) {
				printf("  frame %d : [KO] DecodeFrame echoue (B / CABAC ?)\n", (int)si);
				break;
			}
			uint64 diffs = 0;
			double sse = 0.0;
			uint64 total = 0;
			if (rf) {
				const uint64 yN = (uint64)cur.cropW * cur.cropH;
				const uint64 cN = (uint64)(cur.cropW / 2) * (cur.cropH / 2);
				NkVector<nk_uint8> ref;
				ref.Resize(yN + 2 * cN);
				size_t got = fread(ref.Data(), 1, (size_t)(yN + 2 * cN), rf);
				(void)got;
				uint64 dY = 0, dC = 0;
				auto cmp = [&](const uint8 *dec, int32 dw, int32 w, int32 h, const uint8 *r, uint64 &dd) {
					for (int32 y = 0; y < h; ++y)
						for (int32 x = 0; x < w; ++x) {
							const int32 d = (int32)dec[(usize)y * dw + x] - (int32)r[(usize)y * w + x];
							if (d) {
								++diffs;
								++dd;
							}
							sse += (double)d * d;
							++total;
						}
				};
				cmp(cur.y.Data(), cur.lumaW, cur.cropW, cur.cropH, ref.Data(), dY);
				cmp(cur.cb.Data(), cur.chromaW, cur.cropW / 2, cur.cropH / 2, ref.Data() + yN, dC);
				cmp(cur.cr.Data(), cur.chromaW, cur.cropW / 2, cur.cropH / 2, ref.Data() + yN + cN, dC);
				// DEBUG : nb de diffs par bloc 4x4 (grille), 1er MB seulement.
				if (dY > 0 && cur.cropW <= 32 && cur.cropH <= 32) {
					const int32 bw = cur.cropW / 4, bh = cur.cropH / 4;
					printf("    diffs Y par bloc 4x4 (max diff) :\n");
					for (int32 by = 0; by < bh; ++by) {
						printf("      ");
						for (int32 bx = 0; bx < bw; ++bx) {
							int32 nd = 0, md = 0;
							for (int32 yy = 0; yy < 4; ++yy)
								for (int32 xx = 0; xx < 4; ++xx) {
									const int32 px = bx * 4 + xx, py = by * 4 + yy;
									const int32 dv = (int32)cur.y[(usize)py * cur.lumaW + px] -
													 (int32)ref[(usize)py * cur.cropW + px];
									if (dv) {
										++nd;
										const int32 a = dv < 0 ? -dv : dv;
										if (a > md)
											md = a;
									}
								}
							printf("%2d/%-3d ", nd, md);
						}
						printf("\n");
					}
				}
				const double psnr = (sse > 0.0) ? 10.0 * log10(255.0 * 255.0 * (double)total / sse) : 999.0;
				printf("  frame %d (%s) %dx%d : %llu diff (Y=%llu C=%llu), PSNR=%.2f %s\n", (int)si,
					   (si == 0 ? "IDR" : "P"), cur.cropW, cur.cropH, (unsigned long long)diffs,
					   (unsigned long long)dY, (unsigned long long)dC, psnr, diffs == 0 ? "(BIT-EXACT)" : "");
				totalDiffs += diffs;
			} else {
				printf("  frame %d (%s) : decode OK %dx%d\n", (int)si, (si == 0 ? "IDR" : "P"), cur.cropW, cur.cropH);
			}
			// La frame courante entre en TÊTE de la liste de références (plus récente d'abord).
			NkVector<NkH264Frame> newDpb;
			newDpb.PushBack(cur);
			for (uint64 k = 0; k < dpb.Size() && k < 15; ++k)
				newDpb.PushBack(dpb[k]);
			dpb = newDpb;
		}
		if (rf)
			fclose(rf);
		return (totalDiffs == 0) ? 0 : 2;
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
