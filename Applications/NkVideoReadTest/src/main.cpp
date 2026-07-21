// =============================================================================
// NkVideoReadTest — valide la LECTURE vidéo NKMedia (NkVideoReader).
//   Sans argument : self-test (écrit un AVI MJPEG puis le relit et vérifie).
//   Avec un chemin : ouvre le fichier, affiche les infos et décode toutes les images
//   (mesure une somme de contrôle par image pour prouver le décodage frame par frame).
// =============================================================================
#include "NKMedia/Video/NkVideoReader.h"
#include "NKMedia/Codecs/Video/H264/NkH264Decoder.h"
#include "NKMedia/Codecs/Video/H264/NkH264Cavlc.h"
#include "NKMedia/Codecs/Video/VP8/NkVp8Decoder.h"
#include "NKMedia/Codecs/Video/VP8/NkVp8BoolDecoder.h"

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

	// Mode diagnostic VP8 (chantier en cours) : --vp8header <fichier.ivf>
	// Lit le frame tag (en-tête non compressé) + démarre le décodeur booléen sur la 1ère
	// partition pour décoder color_space/clamping_type (2 premiers bits du header compressé,
	// §9.2) — validation minimale que la brique 1 (décodeur booléen) tourne sans planter.
	// IVF = conteneur brut minimal (pas de démuxage NKMedia requis) : entête 32 octets
	// ("DKIF"+version+header_size+fourcc+w+h+timebase+num_frames) puis, par frame,
	// taille(4 LE)+timestamp(8 LE)+données.
	if (argc >= 3 && strcmp(argv[1], "--vp8header") == 0) {
		FILE *f = fopen(argv[2], "rb");
		if (!f) {
			printf("  [KO] fichier introuvable : %s\n", argv[2]);
			return 1;
		}
		uint8 ivfHdr[32];
		if (fread(ivfHdr, 1, 32, f) != 32 || memcmp(ivfHdr, "DKIF", 4) != 0) {
			printf("  [KO] pas un fichier IVF valide\n");
			fclose(f);
			return 1;
		}
		char fourcc[5] = {(char)ivfHdr[8], (char)ivfHdr[9], (char)ivfHdr[10], (char)ivfHdr[11], 0};
		const uint16 ivfW = (uint16)(ivfHdr[12] | (ivfHdr[13] << 8));
		const uint16 ivfH = (uint16)(ivfHdr[14] | (ivfHdr[15] << 8));
		printf("  IVF : fourcc=%s dims=%ux%u\n", fourcc, ivfW, ivfH);
		uint8 frameHdr[12];
		if (fread(frameHdr, 1, 12, f) != 12) {
			printf("  [KO] pas de frame\n");
			fclose(f);
			return 1;
		}
		const uint32 frameSize = (uint32)frameHdr[0] | ((uint32)frameHdr[1] << 8) | ((uint32)frameHdr[2] << 16) |
								  ((uint32)frameHdr[3] << 24);
		NkVector<uint8> frame;
		frame.Resize(frameSize);
		if (fread(frame.Data(), 1, frameSize, f) != frameSize) {
			printf("  [KO] frame tronquee\n");
			fclose(f);
			return 1;
		}
		fclose(f);

		NkVp8FrameTag tag;
		if (!NkVp8ParseFrameTag(frame.Data(), (usize)frame.Size(), tag)) {
			printf("  [KO] NkVp8ParseFrameTag a echoue\n");
			return 1;
		}
		printf("  frame tag : keyFrame=%d version=%d showFrame=%d firstPartSize=%u dims=%dx%d "
			   "hscale=%d vscale=%d headerSize=%llu\n",
			   tag.keyFrame ? 1 : 0, tag.version, tag.showFrame ? 1 : 0, tag.firstPartSize, tag.width,
			   tag.height, tag.horizScale, tag.vertScale, (unsigned long long)tag.headerSize);

		const bool dimsOk = (tag.width == ivfW && tag.height == ivfH);
		printf("  [ %s ] dimensions frame tag == entete IVF (%dx%d vs %ux%u)\n", dimsOk ? "OK " : "KO",
			   tag.width, tag.height, ivfW, ivfH);

		bool headerOk = true;
		if (tag.keyFrame && tag.headerSize + tag.firstPartSize <= (usize)frame.Size()) {
			NkVp8BoolDecoder bd(frame.Data() + tag.headerSize, (usize)tag.firstPartSize);
			NkVp8FrameContext fc;
			NkVp8Segmentation seg;
			NkVp8LoopFilterDeltas lfDeltas;
			NkVp8FrameHeader hdr;
			NkVp8ParseCompressedHeader(bd, tag.keyFrame, hdr, fc, seg, lfDeltas);
			printf("  en-tete compresse : colorSpace=%d clamping=%d segEnabled=%d filterType=%d "
				   "filterLevel=%d sharpness=%d lfDeltaEnabled=%d log2Partitions=%d baseQ=%d "
				   "y1dcDQ=%d y2dcDQ=%d y2acDQ=%d uvdcDQ=%d uvacDQ=%d refreshEntropy=%d "
				   "refreshLast=%d mbNoSkip=%d probSkipFalse=%d\n",
				   hdr.colorSpace, hdr.clampingType, hdr.segmentationEnabled ? 1 : 0, hdr.filterType,
				   hdr.filterLevel, hdr.sharpnessLevel, hdr.lfDeltaEnabled ? 1 : 0,
				   hdr.log2NbrOfDctPartitions, hdr.baseQIndex, hdr.y1dcDeltaQ, hdr.y2dcDeltaQ,
				   hdr.y2acDeltaQ, hdr.uvdcDeltaQ, hdr.uvacDeltaQ, hdr.refreshEntropyProbs ? 1 : 0,
				   hdr.refreshLastFrame ? 1 : 0, hdr.mbNoSkipCoeff ? 1 : 0, hdr.probSkipFalse);
			// Sanity structurelle (pas encore de decodage complet a comparer bit-a-bit) :
			// plages valides + position finale du decodeur booleen a l'interieur (pas au-dela)
			// de la 1ere partition, avec une marge pour le pre-chargement 2 octets a l'init.
			const bool rangesOk = hdr.log2NbrOfDctPartitions >= 0 && hdr.log2NbrOfDctPartitions <= 3 &&
								   hdr.baseQIndex >= 0 && hdr.baseQIndex <= 127 && hdr.filterLevel >= 0 &&
								   hdr.filterLevel <= 63 && hdr.sharpnessLevel >= 0 && hdr.sharpnessLevel <= 7;
			const bool posOk = bd.pos <= (usize)tag.firstPartSize + 2;
			headerOk = rangesOk && posOk;
			printf("  [ %s ] plages valides (log2Partitions/baseQ/filterLevel/sharpness)\n",
				   rangesOk ? "OK " : "KO");
			printf("  [ %s ] position decodeur booleen (%llu) <= firstPartSize (%u) + marge\n",
				   posOk ? "OK " : "KO", (unsigned long long)bd.pos, tag.firstPartSize);

			// ── Brique 4 : modes de tous les macroblocs (image cle) ──────────────────
			const int32 mbCols = (tag.width + 15) / 16;
			const int32 mbRows = (tag.height + 15) / 16;
			NkVector<NkVp8MbModeInfo> mbInfo;
			if (NkVp8DecodeKeyFrameModes(bd, hdr, seg, mbCols, mbRows, mbInfo)) {
				const int32 stride = mbCols + 1;
				int32 yModeCount[5] = {0, 0, 0, 0, 0};
				int32 uvModeCount[4] = {0, 0, 0, 0};
				int32 bModeCount[10] = {};
				int32 nSkip = 0, nBad = 0, nBpred = 0;
				for (int32 r = 0; r < mbRows; ++r) {
					for (int32 c = 0; c < mbCols; ++c) {
						const NkVp8MbModeInfo &mi = mbInfo[(uint64)(r + 1) * stride + (c + 1)];
						if (mi.yMode > 4 || mi.uvMode > 3)
							++nBad;
						else {
							++yModeCount[mi.yMode];
							++uvModeCount[mi.uvMode];
						}
						if (mi.skipCoeff)
							++nSkip;
						if (mi.yMode == kVp8MbBPred) {
							++nBpred;
							for (int32 b = 0; b < 16; ++b) {
								if (mi.bModes[b] > 9)
									++nBad;
								else
									++bModeCount[mi.bModes[b]];
							}
						}
					}
				}
				printf("  modes MB (%dx%d = %d MB) : yMode DC=%d V=%d H=%d TM=%d B_PRED=%d | "
					   "uvMode DC=%d V=%d H=%d TM=%d | skip=%d\n",
					   mbCols, mbRows, mbCols * mbRows, yModeCount[0], yModeCount[1], yModeCount[2],
					   yModeCount[3], yModeCount[4], uvModeCount[0], uvModeCount[1], uvModeCount[2],
					   uvModeCount[3], nSkip);
				if (nBpred > 0) {
					printf("  sous-modes 4x4 (%d MB en B_PRED) : DC=%d TM=%d VE=%d HE=%d LD=%d "
						   "RD=%d VR=%d VL=%d HD=%d HU=%d\n",
						   nBpred, bModeCount[0], bModeCount[1], bModeCount[2], bModeCount[3],
						   bModeCount[4], bModeCount[5], bModeCount[6], bModeCount[7], bModeCount[8],
						   bModeCount[9]);
				}
				// CONTROLE LE PLUS FORT disponible a ce stade (pas encore de pixels a comparer
				// a une reference) : le decodage de l'en-tete + des 99 MB doit consommer la 1ere
				// partition ENTIEREMENT et SANS deborder. Un desalignement du bitstream (champ
				// mal ordonne, mauvais arbre, table de probas fausse) ferait diverger la
				// consommation et raterait la borne. ⚠️ `bd.pos` seul ne suffit PAS (plafonne a
				// `size`) -> on verifie AUSSI `overreadBytes`, dont 1-2 est normal (le decodeur
				// booleen lit structurellement ~2 octets d'avance).
				const bool modesValidOk = (nBad == 0);
				const bool fullyConsumed = (bd.pos == (usize)tag.firstPartSize);
				const bool noWildOverread = (bd.overreadBytes <= 2);
				printf("  [ %s ] tous les modes dans des plages valides (%d invalides)\n",
					   modesValidOk ? "OK " : "KO", nBad);
				printf("  [ %s ] 1ere partition consommee EXACTEMENT : %llu / %u octets "
					   "(depassement=%d, <=2 normal)\n",
					   (fullyConsumed && noWildOverread) ? "OK " : "KO", (unsigned long long)bd.pos,
					   tag.firstPartSize, bd.overreadBytes);
				headerOk = headerOk && modesValidOk && fullyConsumed && noWildOverread;
			} else {
				printf("  [KO] NkVp8DecodeKeyFrameModes a echoue\n");
				headerOk = false;
			}
		}
		return (dimsOk && headerOk) ? 0 : 1;
	}

	// Mode validation SeekFrame H264 : --seektest <fichier.mp4/mov> <index affichage>
	// Valide le fix 2026-07-21 (SeekFrame était un no-op silencieux pour H264, voir ROADMAP
	// NKMedia). Méthode : décode SÉQUENTIELLEMENT de 0 jusqu'à index+marge (référence sûre),
	// PUIS ré-ouvre un lecteur frais et appelle SeekFrame(index) + un seul ReadFrame — compare
	// la somme de contrôle obtenue à la fenêtre séquentielle [index-6, index+6] (le seek est une
	// APPROXIMATION assumée : ordre décodage ≈ ordre affichage, exact aux limites de GOP).
	if (argc >= 4 && strcmp(argv[1], "--seektest") == 0) {
		const char *path = argv[2];
		const int32 target = atoi(argv[3]);
		const int32 margin = 6;
		NkVector<uint64> seqSums;
		int32 seqBase = target - margin < 0 ? 0 : target - margin;
		{
			NkVideoReader rd;
			if (!rd.Open(path)) {
				printf("  [KO] impossible d'ouvrir : %s\n", path);
				return 1;
			}
			NkVideoFrame fr;
			int32 idx = 0;
			while (idx <= target + margin && rd.ReadFrame(fr)) {
				if (idx >= seqBase) {
					uint64 s = 0;
					for (uint64 i = 0; i < fr.rgba.Size(); ++i)
						s += fr.rgba[i];
					seqSums.PushBack(s);
				}
				++idx;
			}
		}
		NkVideoReader rd2;
		if (!rd2.Open(path) || !rd2.SeekFrame(target)) {
			printf("  [KO] Open/SeekFrame(%d) a echoue\n", target);
			return 1;
		}
		// SeekFrame se contente de repositionner sur l'IDR précédente (voir implémentation) : il
		// faut ensuite redécoder EN AVANT jusqu'à la cible, exactement comme le fait la boucle de
		// rattrapage du lecteur (Applications/NkVideoPlayer). On plafonne les itérations pour ne
		// jamais boucler indéfiniment si `CurrentIndex()` ne progresse pas comme attendu.
		NkVideoFrame fr2;
		int32 guard = 0;
		bool first = true; // garantit au moins UN ReadFrame (cas target=0 : CurrentIndex() y est déjà)
		while ((first || rd2.CurrentIndex() < target) && guard < target + margin + 1000) {
			if (!rd2.ReadFrame(fr2)) {
				printf("  [KO] ReadFrame (rattrapage post-seek) a echoue avant d'atteindre %d\n", target);
				return 1;
			}
			first = false;
			++guard;
		}
		uint64 seekSum = 0;
		for (uint64 i = 0; i < fr2.rgba.Size(); ++i)
			seekSum += fr2.rgba[i];
		int32 bestOffset = 999;
		for (uint64 k = 0; k < seqSums.Size(); ++k) {
			if (seqSums[k] == seekSum) {
				const int32 off = (int32)((int64)(seqBase + (int32)k) - (int64)target);
				const int32 absOff = off < 0 ? -off : off;
				const int32 absBest = bestOffset < 0 ? -bestOffset : bestOffset;
				if (absOff < absBest)
					bestOffset = off;
			}
		}
		if (bestOffset == 999)
			printf("  [KO] SeekFrame(%d) : aucune correspondance dans [%d,%d]\n", target, seqBase, target + margin);
		else
			printf("  [%s] SeekFrame(%d) -> retombe sur l'image affichage %d (offset %+d)\n",
				   bestOffset == 0 ? "OK " : "OK~", target, target + bestOffset, bestOffset);
		return bestOffset == 999 ? 1 : 0;
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
		// â  Avec des B-frames, l'ordre de DECODAGE n'est PAS l'ordre d'AFFICHAGE. Le YUV de
		// reference (ffmpeg) est en ordre d'AFFICHAGE : on compare donc chaque image decodee a
		// ref[POC/2 - pocBase/2], et non a la i-eme image du fichier. On charge tout le YUV pour
		// pouvoir adresser n'importe quelle position.
		NkVector<nk_uint8> allRef;
		int32 pocBase = -1;
		if (rf) {
			fseek(rf, 0, SEEK_END);
			const long rn = ftell(rf);
			fseek(rf, 0, SEEK_SET);
			allRef.Resize((uint64)rn);
			size_t gg = fread(allRef.Data(), 1, (size_t)rn, rf);
			(void)gg;
		}
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
				const uint64 frameBytes = yN + 2 * cN;
				// Index d'AFFICHAGE derive du POC (il avance de 2 par image en codage trame).
				if (pocBase < 0)
					pocBase = cur.poc;
				const int64 disp = (int64)(cur.poc - pocBase) / 2;
				const uint64 off = (uint64)disp * frameBytes;
				if (disp < 0 || off + frameBytes > allRef.Size()) {
					printf("  frame %d : POC=%d hors du YUV de reference (%llu images)\n", (int)si, cur.poc,
						   (unsigned long long)(allRef.Size() / frameBytes));
					continue;
				}
				const nk_uint8 *ref = allRef.Data() + off;
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
				cmp(cur.y.Data(), cur.lumaW, cur.cropW, cur.cropH, ref, dY);
				cmp(cur.cb.Data(), cur.chromaW, cur.cropW / 2, cur.cropH / 2, ref + yN, dC);
				cmp(cur.cr.Data(), cur.chromaW, cur.cropW / 2, cur.cropH / 2, ref + yN + cN, dC);
				// DEBUG : liste les premiers pixels CHROMA divergents (plan, x, y, nous vs ref).
				if (dC > 0 && dY == 0) {
					int32 shown = 0;
					for (int32 comp = 0; comp < 2 && shown < 8; ++comp) {
						const nk_uint8 *dec = comp == 0 ? cur.cb.Data() : cur.cr.Data();
						const nk_uint8 *rf = ref + yN + (uint64)comp * cN;
						const int32 cw = cur.cropW / 2, ch = cur.cropH / 2;
						for (int32 y = 0; y < ch && shown < 8; ++y)
							for (int32 x = 0; x < cw && shown < 8; ++x) {
								const int32 a = dec[(usize)y * cur.chromaW + x], b = rf[(usize)y * cw + x];
								if (a != b) {
									printf("    chroma%s (%d,%d) mb(%d,%d) nous=%d ref=%d\n", comp ? "Cr" : "Cb",
										   x, y, x / 8, y / 8, a, b);
									++shown;
								}
							}
					}
				}
				// DEBUG : localise le PREMIER MB 16x16 (ordre raster) dont un pixel luma differe.
				if (dY > 0) {
					const int32 mbw = (cur.cropW + 15) / 16, mbh = (cur.cropH + 15) / 16;
					for (int32 mby = 0; mby < mbh; ++mby) {
						for (int32 mbx = 0; mbx < mbw; ++mbx) {
							int32 nd = 0, mxd = 0;
							for (int32 yy = 0; yy < 16; ++yy)
								for (int32 xx = 0; xx < 16; ++xx) {
									const int32 pxx = mbx * 16 + xx, pyy = mby * 16 + yy;
									if (pxx >= cur.cropW || pyy >= cur.cropH)
										continue;
									const int32 dv = (int32)cur.y[(usize)pyy * cur.lumaW + pxx] -
													 (int32)ref[(usize)pyy * cur.cropW + pxx];
									if (dv) {
										++nd;
										const int32 a = dv < 0 ? -dv : dv;
										if (a > mxd)
											mxd = a;
									}
								}
							if (nd > 0) {
								printf("    1er MB divergent : (%d,%d) [addr %d] nd=%d maxdiff=%d\n", mbx, mby,
									   mby * mbw + mbx, nd, mxd);
								mby = mbh;
								break;
							}
						}
					}
				}
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
				printf("  dec#%d -> aff#%d (POC=%d%s) %dx%d : %llu diff (Y=%llu C=%llu), PSNR=%.2f %s\n", (int)si,
					   (int)disp, cur.poc, cur.isReference ? "" : ", non-ref", cur.cropW, cur.cropH,
					   (unsigned long long)diffs, (unsigned long long)dY, (unsigned long long)dC, psnr,
					   diffs == 0 ? "(BIT-EXACT)" : "");
				totalDiffs += diffs;
			} else {
				printf("  dec#%d (POC=%d%s) : decode OK %dx%d\n", (int)si, cur.poc,
					   cur.isReference ? "" : ", non-ref", cur.cropW, cur.cropH);
			}
			// La frame courante entre en TÊTE de la liste de références (plus récente d'abord).
			// ⚠️ SEULES les images de référence entrent dans le DPB : une B non-référencée ne doit
			// pas y figurer (sinon l'état POC « image de référence précédente » est faussé).
			if (cur.isReference) {
				NkVector<NkH264Frame> newDpb;
				newDpb.PushBack(cur);
				for (uint64 k = 0; k < dpb.Size() && k < 15; ++k)
					newDpb.PushBack(dpb[k]);
				dpb = newDpb;
			}
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

	// Argument optionnel : une référence RGBA (ordre d'AFFICHAGE) pour valider le réordonnancement.
	FILE *refRgba = (argc >= 3) ? fopen(argv[2], "rb") : nullptr;

	int32 count = 0;
	uint64 globalSum = 0;
	uint64 maxPixDiff = 0, totBadFrames = 0;
	NkVideoFrame fr;
	while (rd.ReadFrame(fr)) {
		uint64 s = 0;
		for (uint64 i = 0; i < fr.rgba.Size(); ++i)
			s += fr.rgba[i];
		globalSum += s;
		if (refRgba) {
			// Compare pixel à pixel à la frame de MÊME position dans la référence (ordre affichage).
			// On tolère un petit écart (la conversion YUV->RGBA diffère de celle de ffmpeg) : ce qui
			// est validé ici est l'ORDRE, pas le bit-exact RGBA.
			NkVector<nk_uint8> ref;
			ref.Resize(fr.rgba.Size());
			size_t got = fread(ref.Data(), 1, (size_t)fr.rgba.Size(), refRgba);
			if (got == fr.rgba.Size()) {
				uint64 bad = 0, mx = 0;
				for (uint64 i = 0; i < fr.rgba.Size(); ++i) {
					const int32 d = (int32)fr.rgba[i] - (int32)ref[i];
					const uint64 a = (uint64)(d < 0 ? -d : d);
					if (a > 6)
						++bad; // au-delà de la tolérance de conversion
					if (a > mx)
						mx = a;
				}
				if (mx > maxPixDiff)
					maxPixDiff = mx;
				if (bad > fr.rgba.Size() / 20)
					++totBadFrames; // > 5% de pixels franchement différents = mauvais ORDRE
			}
		}
		if (count < 3 || (count % 30) == 0)
			printf("    frame %4d : %dx%d  t=%lldms  checksum=%llu\n", fr.index, fr.width, fr.height,
				   (long long)fr.timestampMs, (unsigned long long)s);
		++count;
	}
	if (refRgba) {
		fclose(refRgba);
		printf("  validation ordre : maxPixDiff=%llu, frames mal ordonnees=%llu/%d\n",
			   (unsigned long long)maxPixDiff, (unsigned long long)totBadFrames, count);
	}
	printf("  %d images decodees (somme globale=%llu)\n", count, (unsigned long long)globalSum);
	printf("=== %s ===\n", count > 0 ? "LECTURE OK" : "AUCUNE IMAGE");
	return count > 0 ? 0 : 1;
}
