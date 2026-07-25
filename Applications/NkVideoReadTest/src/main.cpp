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
#include "NKMedia/Codecs/Video/VP9/NkVp9Decoder.h"
#include "NKMedia/Codecs/Video/HEVC/NkHevcDecoder.h"
#include "NKMedia/Codecs/Video/HEVC/NkHevcCabac.h"
#include "NKMedia/Audio/Containers/NkWavWriter.h"

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
		bool okVp9 = NkVp9Decoder::SelfTest();
		printf("  [ %s ] NkVp9Decoder::SelfTest (superframe + en-tete non compresse)\n", okVp9 ? "OK " : "KO");
		bool okHevc = NkHevcDecoder::SelfTest();
		printf("  [ %s ] NkHevcDecoder::SelfTest (NAL split en-tete 2 octets)\n", okHevc ? "OK " : "KO");
		bool okHevcCabac = NkHevcCabacState::SelfTest();
		printf("  [ %s ] NkHevcCabacState::SelfTest (tables init + formule + moteur known-answer)\n",
			   okHevcCabac ? "OK " : "KO");
		bool okWav = NkWavWriter::SelfTest();
		printf("  [ %s ] NkWavWriter::SelfTest (RIFF/WAVE round-trip)\n", okWav ? "OK " : "KO");
		bool all = ok && okH264 && okCavlc && okVp9 && okHevc && okHevcCabac && okWav;
		printf("=== %s ===\n", all ? "LECTURE VIDEO OPERATIONNELLE" : "ECHEC");
		return all ? 0 : 1;
	}

	// Mode VP8 sequence COMPLETE (cle + inter) : --vp8seq <fichier.ivf> <reference.yuv>
	// Decode TOUTES les images du flux via NkVp8DecodeFrame (etat persistant : contexte
	// d'entropie + tampons de reference) et compare CHAQUE image affichee a la reference
	// ffmpeg (I420 concatene). C'est la validation de bout en bout du decodeur VP8.
	if (argc >= 4 && strcmp(argv[1], "--vp8seq") == 0) {
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
		FILE *rf = fopen(argv[3], "rb");
		if (!rf) {
			printf("  [KO] reference introuvable : %s\n", argv[3]);
			fclose(f);
			return 1;
		}

		NkVp8DecoderState st;
		NkVp8Image img;
		int32 frameIdx = 0, shownIdx = 0, badFrames = 0;
		int64 grandDiff = 0, grandMax = 0;
		for (;;) {
			uint8 frameHdr[12];
			if (fread(frameHdr, 1, 12, f) != 12)
				break;
			const uint32 frameSize = (uint32)frameHdr[0] | ((uint32)frameHdr[1] << 8) |
									  ((uint32)frameHdr[2] << 16) | ((uint32)frameHdr[3] << 24);
			NkVector<uint8> frame;
			frame.Resize(frameSize);
			if (fread(frame.Data(), 1, frameSize, f) != frameSize) {
				printf("  [KO] frame %d tronquee\n", frameIdx);
				fclose(f);
				fclose(rf);
				return 1;
			}
			NkVp8FrameTag tag;
			const bool tagOk = NkVp8ParseFrameTag(frame.Data(), (usize)frame.Size(), tag);
			if (!NkVp8DecodeFrame(st, frame.Data(), (usize)frame.Size(), img)) {
				printf("  [KO] NkVp8DecodeFrame a echoue sur la frame %d (decodage #%d)\n",
					   frameIdx, frameIdx);
				fclose(f);
				fclose(rf);
				return 1;
			}
			++frameIdx;
			if (tagOk && !tag.showFrame)
				continue; // frame altref invisible : pas de sortie a comparer

			// Compare a la reference (I420 : Y puis U puis V).
			const int32 w = img.width, h = img.height;
			const int32 cw = (w + 1) / 2, ch2 = (h + 1) / 2;
			NkVector<uint8> ref;
			ref.Resize((uint64)(w * h + 2 * cw * ch2));
			if (fread(ref.Data(), 1, (size_t)ref.Size(), rf) != (usize)ref.Size()) {
				printf("  [KO] reference epuisee a l'image affichee %d\n", shownIdx);
				fclose(f);
				fclose(rf);
				return 1;
			}
			int64 nDiff = 0, maxD = 0;
			for (int32 y = 0; y < h; ++y)
				for (int32 x = 0; x < w; ++x) {
					const int32 a = img.Y()[(int64)y * img.yStride + x];
					const int32 b = ref[(int64)y * w + x];
					const int32 d = a > b ? a - b : b - a;
					if (d) {
						++nDiff;
						if (d > maxD)
							maxD = d;
					}
				}
			const uint8 *refU = ref.Data() + (int64)w * h;
			const uint8 *refV = refU + (int64)cw * ch2;
			for (int32 y = 0; y < ch2; ++y)
				for (int32 x = 0; x < cw; ++x) {
					const int32 du0 = img.U()[(int64)y * img.uvStride + x] - refU[(int64)y * cw + x];
					const int32 dv0 = img.V()[(int64)y * img.uvStride + x] - refV[(int64)y * cw + x];
					const int32 du = du0 < 0 ? -du0 : du0;
					const int32 dv = dv0 < 0 ? -dv0 : dv0;
					if (du) {
						++nDiff;
						if (du > maxD)
							maxD = du;
					}
					if (dv) {
						++nDiff;
						if (dv > maxD)
							maxD = dv;
					}
				}
			grandDiff += nDiff;
			if (maxD > grandMax)
				grandMax = maxD;
			if (nDiff == 0) {
				printf("  image %3d (%s) : BIT-EXACT\n", shownIdx,
					   (tagOk && tag.keyFrame) ? "CLE  " : "inter");
			} else {
				printf("  image %3d (%s) : %lld pixels differents (max %lld)\n", shownIdx,
					   (tagOk && tag.keyFrame) ? "CLE  " : "inter", (long long)nDiff,
					   (long long)maxD);
				++badFrames;
			}
			++shownIdx;
		}
		fclose(f);
		fclose(rf);
		printf("  TOTAL : %d images affichees, %d avec ecarts, %lld pixels differents (max %lld)\n",
			   shownIdx, badFrames, (long long)grandDiff, (long long)grandMax);
		printf("  [ %s ] sequence VP8 %s\n", badFrames == 0 ? "OK " : "KO",
			   badFrames == 0 ? "BIT-EXACTE vs ffmpeg" : "avec ecarts");
		return badFrames == 0 ? 0 : 1;
	}

	// Mode VP8 reconstruction : --vp8recon <fichier.ivf> <reference.yuv>
	// Decode la 1ere image (CLE) en pixels et la compare A LA REFERENCE ffmpeg (I420).
	// ⚠️ Le filtre de boucle n'est pas encore implemente : la comparaison n'a de sens
	// que sur un flux dont filterLevel == 0 (le harnais le signale sinon).
	if (argc >= 4 && strcmp(argv[1], "--vp8recon") == 0) {
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
		uint8 frameHdr[12];
		if (fread(frameHdr, 1, 12, f) != 12) {
			printf("  [KO] pas de frame\n");
			fclose(f);
			return 1;
		}
		const uint32 frameSize = (uint32)frameHdr[0] | ((uint32)frameHdr[1] << 8) |
								  ((uint32)frameHdr[2] << 16) | ((uint32)frameHdr[3] << 24);
		NkVector<uint8> frame;
		frame.Resize(frameSize);
		if (fread(frame.Data(), 1, frameSize, f) != frameSize) {
			printf("  [KO] frame tronquee\n");
			fclose(f);
			return 1;
		}
		fclose(f);

		NkVp8FrameTag tag;
		if (!NkVp8ParseFrameTag(frame.Data(), (usize)frame.Size(), tag) || !tag.keyFrame) {
			printf("  [KO] 1ere image absente ou non-cle\n");
			return 1;
		}
		NkVp8BoolDecoder bd(frame.Data() + tag.headerSize, (usize)tag.firstPartSize);
		NkVp8FrameContext fc;
		NkVp8Segmentation seg;
		NkVp8LoopFilterDeltas lfd;
		NkVp8FrameHeader hdr;
		NkVp8ParseCompressedHeader(bd, true, hdr, fc, seg, lfd);
		const int32 mbCols = (tag.width + 15) / 16, mbRows = (tag.height + 15) / 16;
		NkVector<NkVp8MbModeInfo> mbInfo;
		NkVp8DecodeKeyFrameModes(bd, hdr, seg, mbCols, mbRows, mbInfo);

		if (hdr.log2NbrOfDctPartitions != 0) {
			printf("  [KO] %d partitions de tokens, non gere\n", 1 << hdr.log2NbrOfDctPartitions);
			return 1;
		}
		const usize tokStart = tag.headerSize + (usize)tag.firstPartSize;
		NkVp8BoolDecoder tbd(frame.Data() + tokStart, (usize)frame.Size() - tokStart);
		NkVp8Image img;
		if (!NkVp8ReconstructKeyFrame(tbd, fc, hdr, lfd, mbInfo, tag.width, tag.height, img)) {
			printf("  [KO] NkVp8ReconstructKeyFrame a echoue\n");
			return 1;
		}
		printf("  reconstruit : %dx%d (filterLevel=%d filterType=%d sharpness=%d)\n", tag.width,
			   tag.height, hdr.filterLevel, hdr.filterType, hdr.sharpnessLevel);

		// Reference I420 : Y (w*h) puis U puis V (chacun w/2 * h/2, arrondi au superieur).
		FILE *rf = fopen(argv[3], "rb");
		if (!rf) {
			printf("  [KO] reference introuvable : %s\n", argv[3]);
			return 1;
		}
		const int32 cw = (tag.width + 1) / 2, ch = (tag.height + 1) / 2;
		NkVector<uint8> ref;
		ref.Resize((uint64)(tag.width * tag.height + 2 * cw * ch));
		const usize got = fread(ref.Data(), 1, (size_t)ref.Size(), rf);
		fclose(rf);
		if (got != (usize)ref.Size()) {
			printf("  [KO] reference tronquee (%llu / %llu)\n", (unsigned long long)got,
				   (unsigned long long)ref.Size());
			return 1;
		}

		int64 diffY = 0, diffU = 0, diffV = 0, maxDiff = 0;
		int32 shown = 0;
		for (int32 y = 0; y < tag.height; ++y)
			for (int32 x = 0; x < tag.width; ++x) {
				const int32 a = img.Y()[(int64)y * img.yStride + x];
				const int32 b = ref[(int64)y * tag.width + x];
				const int32 d = a > b ? a - b : b - a;
				if (d) {
					++diffY;
					if (d > maxDiff)
						maxDiff = d;
					if (shown < 24) {
						const NkVp8MbModeInfo &dmi =
							mbInfo[(uint64)(y / 16 + 1) * (mbCols + 1) + (x / 16 + 1)];
						printf("    diff Y (%3d,%3d) nous=%3d ref=%3d | MB(%d,%d) yMode=%d "
							   "bMode[%d]=%d skip=%d\n",
							   x, y, a, b, x / 16, y / 16, dmi.yMode,
							   ((y % 16) / 4) * 4 + (x % 16) / 4,
							   dmi.bModes[((y % 16) / 4) * 4 + (x % 16) / 4], dmi.skipCoeff);
						++shown;
					}
				}
			}
		const uint8 *refU = ref.Data() + (int64)tag.width * tag.height;
		const uint8 *refV = refU + (int64)cw * ch;
		for (int32 y = 0; y < ch; ++y)
			for (int32 x = 0; x < cw; ++x) {
				const int32 au = img.U()[(int64)y * img.uvStride + x], bu = refU[(int64)y * cw + x];
				const int32 av = img.V()[(int64)y * img.uvStride + x], bv = refV[(int64)y * cw + x];
				const int32 du = au > bu ? au - bu : bu - au;
				const int32 dv = av > bv ? av - bv : bv - av;
				if (du) {
					++diffU;
					if (du > maxDiff)
						maxDiff = du;
				}
				if (dv) {
					++diffV;
					if (dv > maxDiff)
						maxDiff = dv;
				}
			}
		const int64 totalPix = (int64)tag.width * tag.height + 2LL * cw * ch;
		const int64 totalDiff = diffY + diffU + diffV;
		printf("  vs reference ffmpeg : %lld / %lld pixels differents (Y=%lld U=%lld V=%lld), "
			   "ecart max=%lld\n",
			   (long long)totalDiff, (long long)totalPix, (long long)diffY, (long long)diffU,
			   (long long)diffV, (long long)maxDiff);
		if (totalDiff == 0) {
			printf("  [ OK  ] RECONSTRUCTION BIT-EXACTE vs ffmpeg\n");
			return 0;
		}
		printf("  [ KO ] ECARTS INATTENDUS (le filtre de boucle est desormais implemente)\n");
		return 1;
	}

	// Mode reconstruction VP9 INTER (brique 6) : --vp9inter <fichier.ivf> <ref_frame1.yuv>
	// Décode la trame 0 (clé, DecodeKeyFrame) PUIS la trame 1 (DecodeInterFrame, réfs
	// LAST=GOLDEN=ALTREF=trame clé — vrai juste après un refresh_frame_flags=0xFF, cas
	// normatif du tout premier P-frame) et compare la trame 1 pixel à pixel à la
	// référence ffmpeg (2e frame brute du flux). Pas de use_prev_frame_mvs (garanti
	// faux pour ce cas par la spec : last_frame_type==KEY_FRAME).
	if (argc >= 4 && strcmp(argv[1], "--vp9inter") == 0) {
		FILE *f = fopen(argv[2], "rb");
		if (!f) {
			printf("  [KO] fichier introuvable : %s\n", argv[2]);
			return 1;
		}
		uint8 ivfHdr[32];
		if (fread(ivfHdr, 1, 32, f) != 32 || memcmp(ivfHdr, "DKIF", 4) != 0) {
			printf("  [KO] pas un IVF\n");
			fclose(f);
			return 1;
		}
		NkVector<uint8> payloads[2];
		for (int32 fr = 0; fr < 2; ++fr) {
			uint8 frameHdr[12];
			if (fread(frameHdr, 1, 12, f) != 12) {
				printf("  [KO] IVF tronque (frame %d)\n", fr);
				fclose(f);
				return 1;
			}
			const uint32 sz = (uint32)frameHdr[0] | ((uint32)frameHdr[1] << 8) | ((uint32)frameHdr[2] << 16) |
							  ((uint32)frameHdr[3] << 24);
			payloads[fr].Resize(sz);
			if (fread(payloads[fr].Data(), 1, sz, f) != sz) {
				printf("  [KO] frame %d tronquee\n", fr);
				fclose(f);
				return 1;
			}
		}
		fclose(f);

		NkVp9EntropyState entropy;
		NkVp9Image keyImg;
		NkVp9Decoder::NkTileParseStats ts0;
		if (!NkVp9Decoder::DecodeKeyFrame(payloads[0].Data(), (usize)payloads[0].Size(), keyImg, entropy, &ts0,
										  true)) {
			printf("  [KO] DecodeKeyFrame (trame 0) a echoue\n");
			return 1;
		}
		printf("  trame 0 (cle) : %dx%d, %d blocs\n", keyImg.width, keyImg.height, ts0.blocks);

		const NkVp9Image *refs[3] = {&keyImg, &keyImg, &keyImg};
		NkVp9Image interImg;
		NkVp9Decoder::NkTileParseStats ts1;
		if (!NkVp9Decoder::DecodeInterFrame(payloads[1].Data(), (usize)payloads[1].Size(), refs, interImg, entropy,
											&ts1, true, false, nullptr)) {
			printf("  [KO] DecodeInterFrame (trame 1) a echoue (tiles=%d blocs=%d skip=%d eob=%lld overread=%d)\n",
				   ts1.tiles, ts1.blocks, ts1.skipBlocks, (long long)ts1.eobTotal, ts1.maxOverread);
			return 1;
		}
		printf("  trame 1 (inter) : %dx%d, %d blocs, eob=%lld\n", interImg.width, interImg.height, ts1.blocks,
			   (long long)ts1.eobTotal);

		FILE *rf = fopen(argv[3], "rb");
		if (!rf) {
			printf("  [KO] reference introuvable : %s\n", argv[3]);
			return 1;
		}
		const usize ySz = (usize)interImg.width * (usize)interImg.height;
		const usize uvSz = (usize)interImg.uvWidth * (usize)interImg.uvHeight;
		const usize frameBytes = ySz + 2 * uvSz;
		// La reference brute contient TOUTES les frames concatenees : on saute la
		// frame 0 (cle) pour comparer a la frame 1 (inter).
		if (fseek(rf, (long)frameBytes, SEEK_SET) != 0) {
			printf("  [KO] reference trop courte (seek frame 1)\n");
			fclose(rf);
			return 1;
		}
		NkVector<uint8> ref;
		ref.Resize(frameBytes);
		if (fread(ref.Data(), 1, frameBytes, rf) != frameBytes) {
			printf("  [KO] reference trop courte\n");
			fclose(rf);
			return 1;
		}
		fclose(rf);
		int64 diffs = 0;
		int32 maxDiff = 0, firstX = -1, firstY = -1, firstPlane = -1;
		int32 maxDiffX = -1, maxDiffY = -1, maxDiffPlane = -1;
		const uint8 *planes[3] = {interImg.y.Data(), interImg.u.Data(), interImg.v.Data()};
		const uint8 *refp[3] = {ref.Data(), ref.Data() + ySz, ref.Data() + ySz + uvSz};
		const int32 pw[3] = {interImg.width, interImg.uvWidth, interImg.uvWidth};
		const int32 ph[3] = {interImg.height, interImg.uvHeight, interImg.uvHeight};
		const int32 pstride[3] = {interImg.yStride, interImg.uvStride, interImg.uvStride};
		for (int32 p = 0; p < 3; ++p) {
			for (int32 yy = 0; yy < ph[p]; ++yy) {
				for (int32 xx = 0; xx < pw[p]; ++xx) {
					const int32 a = planes[p][yy * pstride[p] + xx];
					const int32 b = refp[p][yy * pw[p] + xx];
					const int32 d = a > b ? a - b : b - a;
					if (d) {
						++diffs;
						if (d > maxDiff) {
							maxDiff = d;
							maxDiffX = xx;
							maxDiffY = yy;
							maxDiffPlane = p;
						}
						if (firstX < 0) {
							firstX = xx;
							firstY = yy;
							firstPlane = p;
						}
					}
				}
			}
		}
		if (diffs == 0) {
			printf("  [ OK  ] trame INTER VP9 BIT-EXACTE vs ffmpeg (%dx%d)\n", interImg.width, interImg.height);
			return 0;
		}
		printf("  [ KO ] %lld pixels differents (maxdiff=%d @ plan=%d (%d,%d), premier plan=%d @ %d,%d)\n",
			   (long long)diffs, maxDiff, maxDiffPlane, maxDiffX, maxDiffY, firstPlane, firstX, firstY);
		FILE *of = fopen("vp9inter_out.yuv", "wb");
		if (of) {
			for (int32 p = 0; p < 3; ++p)
				for (int32 yy = 0; yy < ph[p]; ++yy)
					fwrite(planes[p] + (usize)yy * (usize)pstride[p], 1, (usize)pw[p], of);
			fclose(of);
			printf("  (image decodee ecrite dans vp9inter_out.yuv)\n");
		}
		return 1;
	}

	// Mode reconstruction VP9 MULTI-TRAMES : --vp9multi <fichier.ivf> <reference.yuv>
	// Décode TOUT le flux (clé + N inter, DPB 8 slots via refFrameIdx/refreshFrameFlags,
	// show_existing_frame géré) et compare chaque trame AFFICHÉE à la référence brute
	// ffmpeg (une image par trame montrée, dans l'ordre d'affichage). `usePrevFrameMvs`
	// toujours faux (aucune extraction de grille MV exposée par le décodeur pour l'instant
	// — limite documentée, cf. NkVp9Decoder.h).
	if (argc >= 4 && strcmp(argv[1], "--vp9multi") == 0) {
		FILE *f = fopen(argv[2], "rb");
		if (!f) {
			printf("  [KO] fichier introuvable : %s\n", argv[2]);
			return 1;
		}
		uint8 ivfHdr[32];
		if (fread(ivfHdr, 1, 32, f) != 32 || memcmp(ivfHdr, "DKIF", 4) != 0) {
			printf("  [KO] pas un IVF\n");
			fclose(f);
			return 1;
		}
		const uint16 ivfW = (uint16)(ivfHdr[12] | (ivfHdr[13] << 8));
		const uint16 ivfH = (uint16)(ivfHdr[14] | (ivfHdr[15] << 8));
		FILE *rf = fopen(argv[3], "rb");
		if (!rf) {
			printf("  [KO] reference introuvable : %s\n", argv[3]);
			fclose(f);
			return 1;
		}

		NkVp9EntropyState entropy;
		NkVp9Image slots[8];
		bool slotValid[8] = {false, false, false, false, false, false, false, false};

		auto CompareShown = [&](const NkVp9Image &img, int32 frameIdx, int32 frameType) -> bool {
			const usize ySz = (usize)img.width * (usize)img.height;
			const usize uvSz = (usize)img.uvWidth * (usize)img.uvHeight;
			const usize frameBytes = ySz + 2 * uvSz;
			NkVector<uint8> ref;
			ref.Resize(frameBytes);
			if (fread(ref.Data(), 1, frameBytes, rf) != frameBytes) {
				printf("  [KO] reference trop courte a la trame affichee %d\n", frameIdx);
				return false;
			}
			const uint8 *planes[3] = {img.y.Data(), img.u.Data(), img.v.Data()};
			const uint8 *refp[3] = {ref.Data(), ref.Data() + ySz, ref.Data() + ySz + uvSz};
			const int32 pw[3] = {img.width, img.uvWidth, img.uvWidth};
			const int32 ph[3] = {img.height, img.uvHeight, img.uvHeight};
			const int32 pstride[3] = {img.yStride, img.uvStride, img.uvStride};
			int64 diffs = 0;
			int32 maxDiff = 0;
			for (int32 p = 0; p < 3; ++p)
				for (int32 yy = 0; yy < ph[p]; ++yy)
					for (int32 xx = 0; xx < pw[p]; ++xx) {
						const int32 a = planes[p][yy * pstride[p] + xx];
						const int32 b = refp[p][yy * pw[p] + xx];
						const int32 d = a > b ? a - b : b - a;
						if (d) {
							++diffs;
							if (d > maxDiff)
								maxDiff = d;
						}
					}
			if (diffs != 0) {
				printf("  [KO] trame %d (type=%d) : %lld pixels differents (maxdiff=%d)\n", frameIdx, frameType,
					   (long long)diffs, maxDiff);
				return false;
			}
			return true;
		};

		// use_prev_frame_mvs (dec_api) : éligible si la trame PRÉCÉDENTE n'était pas
		// clé/intra-only, mêmes dimensions, et la trame COURANTE n'est pas error-resilient.
		bool prevEligibleBase = false; // "trame precedente n'etait pas intra/cle"
		int32 prevWidth = 0, prevHeight = 0;
		NkVector<NkVp9MvRef> mvGrid, nextMvGrid;

		int32 frameIdx = 0, decodedCount = 0, shownCount = 0, bitExactCount = 0;
		bool allOk = true;
		for (; allOk;) {
			uint8 frameHdr[12];
			if (fread(frameHdr, 1, 12, f) != 12)
				break;
			const uint32 payloadSize = (uint32)frameHdr[0] | ((uint32)frameHdr[1] << 8) |
									   ((uint32)frameHdr[2] << 16) | ((uint32)frameHdr[3] << 24);
			NkVector<uint8> payload;
			payload.Resize(payloadSize);
			if (fread(payload.Data(), 1, payloadSize, f) != payloadSize) {
				printf("  [KO] charge tronquee\n");
				allOk = false;
				break;
			}
			NkVp9Superframe sf;
			if (!NkVp9Decoder::ParseSuperframe(payload.Data(), (usize)payload.Size(), sf)) {
				printf("  [KO] superframe invalide\n");
				allOk = false;
				break;
			}
			for (int32 fr = 0; fr < sf.count && allOk; ++fr) {
				const uint8 *fdata = payload.Data() + sf.offsets[fr];
				const usize fsize = sf.sizes[fr];
				NkVp9FrameHeader hdr;
				if (!NkVp9Decoder::ParseUncompressedHeader(fdata, fsize, hdr, (int32)ivfW, (int32)ivfH)) {
					printf("  [KO] header invalide trame %d\n", frameIdx);
					allOk = false;
					break;
				}
				if (hdr.showExistingFrame) {
					const int32 slot = hdr.frameToShowMapIdx;
					if (slot < 0 || slot >= 8 || !slotValid[slot]) {
						printf("  [KO] show_existing_frame %d : slot %d non decode\n", frameIdx, slot);
						allOk = false;
						break;
					}
					++shownCount;
					if (CompareShown(slots[slot], frameIdx, -1))
						++bitExactCount;
					else
						allOk = false;
					++frameIdx;
					continue;
				}
				NkVp9Image img;
				NkVp9Decoder::NkTileParseStats stats;
				bool ok;
				bool isIntra = (hdr.frameType == kVp9KeyFrame || hdr.intraOnly);
				if (isIntra) {
					ok = NkVp9Decoder::DecodeKeyFrame(fdata, fsize, img, entropy, &stats, true);
				} else {
					const NkVp9Image *refs[3];
					bool refsOk = true;
					for (int32 i = 0; i < 3; ++i) {
						const int32 slot = hdr.refFrameIdx[i];
						if (slot < 0 || slot >= 8 || !slotValid[slot]) {
							refsOk = false;
							break;
						}
						refs[i] = &slots[slot];
					}
					if (!refsOk) {
						printf("  [KO] trame %d : reference(s) non decodee(s)\n", frameIdx);
						allOk = false;
						break;
					}
					const bool eligible = prevEligibleBase && !hdr.errorResilient && hdr.width == prevWidth &&
										  hdr.height == prevHeight;
					ok = NkVp9Decoder::DecodeInterFrame(fdata, fsize, refs, img, entropy, &stats, true, eligible,
														eligible ? mvGrid.Data() : nullptr, &nextMvGrid);
					if (ok)
						mvGrid = nextMvGrid;
				}
				if (!ok) {
					printf("  [KO] decode echoue trame %d (type=%d tiles=%d blocs=%d overread=%d)\n", frameIdx,
						   hdr.frameType, stats.tiles, stats.blocks, stats.maxOverread);
					allOk = false;
					break;
				}
				for (int32 s = 0; s < 8; ++s) {
					if (hdr.refreshFrameFlags & (1u << s)) {
						slots[s] = img;
						slotValid[s] = true;
					}
				}
				// use_prev_frame_mvs de la trame SUIVANTE (dec_api) : cette trame doit être
				// non intra/clé ET avoir été AFFICHÉE (cm->last_show_frame) — une trame
				// altref invisible casse l'éligibilité pour la trame qui la suit.
				prevEligibleBase = !isIntra && hdr.showFrame;
				prevWidth = hdr.width;
				prevHeight = hdr.height;
				++decodedCount;
				if (hdr.showFrame) {
					++shownCount;
					if (CompareShown(img, frameIdx, hdr.frameType))
						++bitExactCount;
					else
						allOk = false;
				}
				++frameIdx;
			}
		}
		fclose(f);
		fclose(rf);
		printf("  %d trames decodees, %d affichees, %d/%d bit-exactes vs ffmpeg\n", decodedCount, shownCount,
			   bitExactCount, shownCount);
		return (allOk && bitExactCount == shownCount && shownCount > 0) ? 0 : 1;
	}

	// Mode reconstruction VP9 (brique 4) : --vp9recon <fichier.ivf> <reference.yuv>
	// Décode la PREMIÈRE trame clé (prédiction intra + déquant + transformées) et la
	// compare pixel à pixel à la référence ffmpeg (I420). Objectif : BIT-EXACT.
	if (argc >= 4 && strcmp(argv[1], "--vp9recon") == 0) {
		FILE *f = fopen(argv[2], "rb");
		if (!f) {
			printf("  [KO] fichier introuvable : %s\n", argv[2]);
			return 1;
		}
		uint8 ivfHdr[32];
		if (fread(ivfHdr, 1, 32, f) != 32 || memcmp(ivfHdr, "DKIF", 4) != 0) {
			printf("  [KO] pas un IVF\n");
			fclose(f);
			return 1;
		}
		uint8 frameHdr[12];
		if (fread(frameHdr, 1, 12, f) != 12) {
			fclose(f);
			return 1;
		}
		const uint32 payloadSize = (uint32)frameHdr[0] | ((uint32)frameHdr[1] << 8) |
								   ((uint32)frameHdr[2] << 16) | ((uint32)frameHdr[3] << 24);
		NkVector<uint8> payload;
		payload.Resize(payloadSize);
		if (fread(payload.Data(), 1, payloadSize, f) != payloadSize) {
			fclose(f);
			return 1;
		}
		fclose(f);
		NkVp9Superframe sf;
		NkVp9Decoder::ParseSuperframe(payload.Data(), (usize)payload.Size(), sf);
		const bool noLf = (argc >= 5 && strcmp(argv[4], "nolf") == 0);
		NkVp9Image img;
		NkVp9EntropyState entropy;
		NkVp9Decoder::NkTileParseStats ts;
		if (!NkVp9Decoder::DecodeKeyFrame(payload.Data() + sf.offsets[0], sf.sizes[0], img, entropy, &ts,
										  !noLf)) {
			printf("  [KO] DecodeKeyFrame a echoue (tiles=%d blocs=%d)\n", ts.tiles, ts.blocks);
			return 1;
		}
		printf("  decode : %dx%d (uv %dx%d), %d blocs, eob=%lld\n", img.width, img.height,
			   img.uvWidth, img.uvHeight, ts.blocks, (long long)ts.eobTotal);

		FILE *rf = fopen(argv[3], "rb");
		if (!rf) {
			printf("  [KO] reference introuvable : %s\n", argv[3]);
			return 1;
		}
		const usize ySz = (usize)img.width * (usize)img.height;
		const usize uvSz = (usize)img.uvWidth * (usize)img.uvHeight;
		NkVector<uint8> ref;
		ref.Resize(ySz + 2 * uvSz);
		if (fread(ref.Data(), 1, ySz + 2 * uvSz, rf) != ySz + 2 * uvSz) {
			printf("  [KO] reference trop courte\n");
			fclose(rf);
			return 1;
		}
		fclose(rf);
		int64 diffs = 0;
		int32 maxDiff = 0, firstX = -1, firstY = -1, firstPlane = -1;
		const uint8 *planes[3] = {img.y.Data(), img.u.Data(), img.v.Data()};
		const uint8 *refs[3] = {ref.Data(), ref.Data() + ySz, ref.Data() + ySz + uvSz};
		const int32 pw[3] = {img.width, img.uvWidth, img.uvWidth};
		const int32 ph[3] = {img.height, img.uvHeight, img.uvHeight};
		const int32 pstride[3] = {img.yStride, img.uvStride, img.uvStride};
		for (int32 p = 0; p < 3; ++p) {
			for (int32 yy = 0; yy < ph[p]; ++yy) {
				for (int32 xx = 0; xx < pw[p]; ++xx) {
					const int32 a = planes[p][yy * pstride[p] + xx];
					const int32 b = refs[p][yy * pw[p] + xx];
					const int32 d = a > b ? a - b : b - a;
					if (d) {
						++diffs;
						if (d > maxDiff)
							maxDiff = d;
						if (firstX < 0) {
							firstX = xx;
							firstY = yy;
							firstPlane = p;
						}
					}
				}
			}
		}
		if (diffs == 0) {
			printf("  [ OK  ] image cle VP9 BIT-EXACTE vs ffmpeg (%dx%d)\n", img.width, img.height);
			return 0;
		}
		printf("  [ KO ] %lld pixels differents (maxdiff=%d, premier plan=%d @ %d,%d)\n",
			   (long long)diffs, maxDiff, firstPlane, firstX, firstY);
		// Dump de notre image pour cartographier les écarts (vp9recon_out.yuv).
		FILE *of = fopen("vp9recon_out.yuv", "wb");
		if (of) {
			for (int32 p = 0; p < 3; ++p)
				for (int32 yy = 0; yy < ph[p]; ++yy)
					fwrite(planes[p] + (usize)yy * (usize)pstride[p], 1, (usize)pw[p], of);
			fclose(of);
			printf("  (image decodee ecrite dans vp9recon_out.yuv)\n");
		}
		return 1;
	}

	// Mode diagnostic VP9 (brique 1) : --vp9header <fichier.ivf>
	// Parcourt TOUTES les frames IVF : découpe superframe (Annexe B) + en-tête non
	// compressé (§6.2) de chaque sous-trame. Vérifie : sync code/marker OK, dims des
	// trames clés == entête IVF, somme des tailles superframe == charge, headerSize
	// compressé dans les bornes. Statistiques par type de trame.
	if (argc >= 3 && strcmp(argv[1], "--vp9header") == 0) {
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
		if (strcmp(fourcc, "VP90") != 0)
			printf("  [!!] fourcc != VP90\n");

		int32 nPayloads = 0, nFrames = 0, nKey = 0, nInter = 0, nAltref = 0, nShowExisting = 0;
		int32 nSuperframes = 0, nCompOk = 0, nCompTotal = 0;
		int32 nContentOk = 0, nContentTotal = 0, nContentBlocks = 0;
		long long nContentEob = 0;
		bool allOk = true;
		for (;;) {
			uint8 frameHdr[12];
			if (fread(frameHdr, 1, 12, f) != 12)
				break;
			const uint32 payloadSize = (uint32)frameHdr[0] | ((uint32)frameHdr[1] << 8) |
									   ((uint32)frameHdr[2] << 16) | ((uint32)frameHdr[3] << 24);
			NkVector<uint8> payload;
			payload.Resize(payloadSize);
			if (fread(payload.Data(), 1, payloadSize, f) != payloadSize) {
				printf("  [KO] charge %d tronquee\n", nPayloads);
				allOk = false;
				break;
			}
			NkVp9Superframe sf;
			if (!NkVp9Decoder::ParseSuperframe(payload.Data(), (usize)payload.Size(), sf)) {
				printf("  [KO] superframe %d invalide\n", nPayloads);
				allOk = false;
				break;
			}
			if (sf.count > 1)
				++nSuperframes;
			for (int32 fr = 0; fr < sf.count; ++fr) {
				NkVp9FrameHeader h;
				if (!NkVp9Decoder::ParseUncompressedHeader(payload.Data() + sf.offsets[fr],
														   sf.sizes[fr], h, (int32)ivfW,
														   (int32)ivfH)) {
					printf("  [KO] header invalide (charge %d, sous-trame %d)\n", nPayloads, fr);
					allOk = false;
					continue;
				}
				++nFrames;
				if (h.showExistingFrame) {
					++nShowExisting;
				} else if (h.frameType == kVp9KeyFrame) {
					++nKey;
					if (h.width != (int32)ivfW || h.height != (int32)ivfH) {
						printf("  [KO] dims cle %dx%d != IVF %ux%u\n", h.width, h.height, ivfW, ivfH);
						allOk = false;
					}
				} else {
					++nInter;
					if (!h.showFrame)
						++nAltref;
				}
				if (!h.showExistingFrame &&
					(usize)h.uncompressedBytes + (usize)h.headerSizeBytes > sf.sizes[fr]) {
					printf("  [KO] headerSize hors bornes (charge %d, sous-trame %d : %d+%d > %u)\n",
						   nPayloads, fr, h.uncompressedBytes, h.headerSizeBytes,
						   (unsigned)sf.sizes[fr]);
					allOk = false;
				} else if (!h.showExistingFrame && h.headerSizeBytes > 0) {
					// Brique 2 : en-tête COMPRESSÉ (bool decoder + updates de probas).
					++nCompTotal;
					NkVp9FrameContext fc;
					NkVp9Decoder::InitDefaultFrameContext(fc);
					NkVp9CompressedHeader ch;
					if (NkVp9Decoder::ParseCompressedHeader(payload.Data() + sf.offsets[fr] +
																(usize)h.uncompressedBytes,
															(usize)h.headerSizeBytes, h, fc, ch)) {
						++nCompOk;
					} else {
						if (nCompTotal - nCompOk <= 4)
							printf("  [KO] en-tete compresse invalide (charge %d, sous-trame %d)\n",
								   nPayloads, fr);
						allOk = false;
					}
					// Brique 3 : CONTENU des trames clés/intra (partitions + modes + tokens),
					// critère = consommation exacte de chaque tile.
					if ((h.frameType == kVp9KeyFrame || h.intraOnly) && !h.showExistingFrame) {
						++nContentTotal;
						const usize hdrBytes = (usize)h.uncompressedBytes + (usize)h.headerSizeBytes;
						NkVp9Decoder::NkTileParseStats ts;
						if (NkVp9Decoder::ParseKeyFrameContent(
								payload.Data() + sf.offsets[fr] + hdrBytes, sf.sizes[fr] - hdrBytes,
								h, fc, ch, ts)) {
							++nContentOk;
							nContentBlocks += ts.blocks;
							nContentEob += ts.eobTotal;
						} else {
							if (nContentTotal - nContentOk <= 4)
								printf("  [KO] contenu cle invalide (charge %d, sous-trame %d : "
									   "tiles=%d blocs=%d overread=%d)\n",
									   nPayloads, fr, ts.tiles, ts.blocks, ts.maxOverread);
							allOk = false;
						}
					}
				}
				if (nFrames <= 6)
					printf("  trame %d : type=%s show=%d dims=%dx%d profil=%d cs=%d q=%d lf=%d "
						   "tiles=%dx%d hdrNC=%d hdrC=%d\n",
						   nFrames - 1,
						   h.showExistingFrame ? "SHOW_EXISTING"
											   : (h.frameType == kVp9KeyFrame ? "CLE" : "INTER"),
						   h.showFrame ? 1 : 0, h.width, h.height, h.profile, h.colorSpace,
						   h.baseQIdx, h.lfLevel, 1 << h.tileColsLog2, 1 << h.tileRowsLog2,
						   h.uncompressedBytes, h.headerSizeBytes);
			}
			++nPayloads;
		}
		fclose(f);
		printf("  charges=%d (superframes=%d) trames=%d : cles=%d inter=%d (altref invisibles=%d) "
			   "show_existing=%d\n",
			   nPayloads, nSuperframes, nFrames, nKey, nInter, nAltref, nShowExisting);
		printf("  en-tetes compresses : %d/%d parses sans erreur (bit marqueur + bornes bool)\n",
			   nCompOk, nCompTotal);
		printf("  contenu cles/intra : %d/%d trames (tiles consommees EXACTEMENT), %d blocs, "
			   "eob total=%lld\n",
			   nContentOk, nContentTotal, nContentBlocks, nContentEob);
		printf("  [ %s ] parsing en-tetes VP9\n", allOk ? "OK " : "KO");
		return allOk ? 0 : 1;
	}

	// Mode diagnostic TEMPORAIRE (cross-check independant) :
	// --hevcdump <fichier.265> <poc cible> <sortie.bin>
	// Ecrit [u32 dataByteOffset LE][u32 taille RBSP LE][RBSP dé-émulé] de la
	// PREMIERE slice P dont le POC calculé == poc cible, pour qu'un script
	// externe (Python) puisse reprendre le decodage CABAC EXACTEMENT au bon
	// octet sans avoir a reimplementer le parsing du slice header.
	if (argc >= 5 && strcmp(argv[1], "--hevcdump") == 0) {
		FILE *f = fopen(argv[2], "rb");
		if (!f) {
			printf("  [KO] fichier introuvable : %s\n", argv[2]);
			return 1;
		}
		fseek(f, 0, SEEK_END);
		long n = ftell(f);
		fseek(f, 0, SEEK_SET);
		NkVector<uint8> buf;
		buf.Resize((usize)n);
		if (fread(buf.Data(), 1, (size_t)n, f) != (size_t)n) {
			fclose(f);
			return 1;
		}
		fclose(f);
		const int32 targetPoc = atoi(argv[3]);
		NkVector<NkHevcNal> nals;
		NkHevcDecoder::SplitNalsAnnexB(buf.Data(), (usize)buf.Size(), nals);
		NkHevcSps sps;
		NkHevcPps pps;
		bool haveSps = false, havePps = false;
		int32 prevPocTid0 = 0;
		bool done = false;
		for (uint64 i = 0; i < nals.Size() && !done; ++i) {
			const NkHevcNal &nal = nals[i];
			const uint8 *nd = buf.Data() + nal.offset;
			if (nal.type == kHevcNalSps) {
				haveSps = NkHevcDecoder::ParseSps(nd, nal.size, sps);
				continue;
			}
			if (nal.type == kHevcNalPps) {
				havePps = NkHevcDecoder::ParsePps(nd, nal.size, pps);
				continue;
			}
			if (nal.type > 31 || !haveSps || !havePps)
				continue;
			NkHevcSliceHeader sh;
			if (!NkHevcDecoder::ParseSliceHeader(nd, nal.size, sps, pps, sh))
				continue;
			const bool isIdr = nal.type == kHevcNalIdrWRadl || nal.type == kHevcNalIdrNLp;
			const int32 poc =
				NkHevcDecoder::ComputePoc(sh.picOrderCntLsb, sps.log2MaxPocLsb, isIdr, prevPocTid0);
			prevPocTid0 = poc;
			if (poc != targetPoc || sh.sliceType != kHevcSliceP)
				continue;
			NkVector<uint8> rbsp;
			NkHevcDecoder::DeemulateRbsp(nd, nal.size, rbsp);
			FILE *out = fopen(argv[4], "wb");
			if (!out) {
				printf("  [KO] ecriture impossible : %s\n", argv[4]);
				return 1;
			}
			uint32 off = (uint32)sh.dataByteOffset, sz = (uint32)rbsp.Size();
			fwrite(&off, 4, 1, out);
			fwrite(&sz, 4, 1, out);
			fwrite(rbsp.Data(), 1, sz, out);
			fclose(out);
			printf("  [OK] poc=%d dataByteOffset=%u rbspSize=%u -> %s\n", poc, off, sz, argv[4]);
			done = true;
		}
		return done ? 0 : 1;
	}

	// Mode diagnostic HEVC (brique 1) : --hevcheader <fichier.265/.hevc Annex-B brut>
	// Decoupe les NALs (en-tete 2 octets), parse VPS/SPS/PPS et affiche dimensions/
	// profil/niveau/chroma/profondeur de bits — a comparer a `ffprobe` sur le meme
	// fichier pour validation (dimensions et profil doivent correspondre exactement).
	if (argc >= 3 && strcmp(argv[1], "--hevcheader") == 0) {
		FILE *f = fopen(argv[2], "rb");
		if (!f) {
			printf("  [KO] fichier introuvable : %s\n", argv[2]);
			return 1;
		}
		fseek(f, 0, SEEK_END);
		long n = ftell(f);
		fseek(f, 0, SEEK_SET);
		NkVector<uint8> buf;
		buf.Resize((usize)n);
		size_t got = fread(buf.Data(), 1, (size_t)n, f);
		fclose(f);
		if (got != (size_t)n) {
			printf("  [KO] lecture incomplete\n");
			return 1;
		}
		NkVector<NkHevcNal> nals;
		NkHevcDecoder::SplitNalsAnnexB(buf.Data(), (usize)buf.Size(), nals);
		printf("  %llu NALs (Annex-B, en-tete 2 octets)\n", (unsigned long long)nals.Size());
		int32 nVps = 0, nSps = 0, nPps = 0, nIdr = 0, nCra = 0, nTrail = 0, sliceHeaderCount = 0,
			  sliceParseFailures = 0;
		int32 prevPocTid0 = 0; // brique 9 : POC de la derniere image decodee (ComputePoc)
		bool haveSps = false, havePps = false;
		NkHevcSps sps;
		NkHevcPps pps;
		for (uint64 i = 0; i < nals.Size(); ++i) {
			const NkHevcNal &nal = nals[i];
			const uint8 *nd = buf.Data() + nal.offset;
			if (nal.type == kHevcNalVps) {
				++nVps;
				int32 vpsId;
				NkHevcProfileTierLevel ptl;
				if (NkHevcDecoder::ParseVps(nd, nal.size, vpsId, ptl))
					printf("  VPS id=%d profil=%d niveau=%d.%d\n", vpsId, ptl.generalProfileIdc,
						   ptl.generalLevelIdc / 30, (ptl.generalLevelIdc % 30) / 3);
			} else if (nal.type == kHevcNalSps) {
				++nSps;
				if (NkHevcDecoder::ParseSps(nd, nal.size, sps)) {
					haveSps = true;
					printf("  SPS id=%d %dx%d profil=%d niveau=%d.%d chroma=%d profondeur=%d/%d "
						   "fenetre_conformance=%d (l=%d r=%d h=%d b=%d) minCb=%d ctb=%d amp=%d "
						   "minTb=%d maxTb=%d maxTrafoIntra=%d maxTrafoInter=%d\n",
						   sps.spsId, sps.width, sps.height, sps.ptl.generalProfileIdc,
						   sps.ptl.generalLevelIdc / 30, (sps.ptl.generalLevelIdc % 30) / 3,
						   sps.chromaFormatIdc, sps.bitDepthLuma, sps.bitDepthChroma, sps.conformanceWindow,
						   sps.confWinLeft, sps.confWinRight, sps.confWinTop, sps.confWinBottom,
						   1 << sps.log2MinCbSizeY, 1 << (sps.log2MinCbSizeY + sps.log2DiffMaxMinCbSizeY),
						   sps.ampEnabled ? 1 : 0, 1 << sps.log2MinTbSizeY,
						   1 << (sps.log2MinTbSizeY + sps.log2DiffMaxMinTbSizeY),
						   sps.maxTransformHierarchyDepthIntra, sps.maxTransformHierarchyDepthInter);
				} else {
					printf("  [KO] SPS %llu : parsing echoue\n", (unsigned long long)i);
				}
			} else if (nal.type == kHevcNalPps) {
				++nPps;
				if (NkHevcDecoder::ParsePps(nd, nal.size, pps)) {
					havePps = true;
					printf("  PPS id=%d sps_id=%d tuiles=%d(%dx%d) sync_entropie=%d cu_qp_delta=%d "
						   "diffQgDepth=%d signHide=%d tSkip=%d tqBypass=%d mergeLvl=%d wp=%d\n",
						   pps.ppsId, pps.spsId, pps.tilesEnabled, pps.numTileColumnsMinus1 + 1,
						   pps.numTileRowsMinus1 + 1, pps.entropyCodingSyncEnabled, pps.cuQpDeltaEnabled,
						   pps.diffCuQpDeltaDepth, pps.signDataHiding ? 1 : 0,
						   pps.transformSkipEnabled ? 1 : 0, pps.transquantBypassEnabled ? 1 : 0,
						   pps.log2ParallelMergeLevel, pps.weightedPred ? 1 : 0);
				} else {
					printf("  [KO] PPS %llu : parsing echoue\n", (unsigned long long)i);
				}
			} else if (nal.type == kHevcNalIdrWRadl || nal.type == kHevcNalIdrNLp) {
				++nIdr;
			} else if (nal.type == kHevcNalCra) {
				++nCra;
			} else if (nal.type <= kHevcNalRaslR) {
				++nTrail;
			}
			// Slice headers (briques 2+3) : parse et affiche TOUTES les slices — champs
			// alignes sur la sortie de `ffmpeg -bsf:v trace_headers` (slice_type, poc_lsb,
			// num_negative/positive_pics, five_minus_max_num_merge_cand->max_merge,
			// slice_qp_delta->qp) pour comparaison directe avec l'oracle.
			if (nal.type <= 31 && haveSps && havePps) {
				NkHevcSliceHeader sh;
				if (NkHevcDecoder::ParseSliceHeader(nd, nal.size, sps, pps, sh)) {
					++sliceHeaderCount;
					static const char *kTypeNames[3] = {"B", "P", "I"};
					const char *typeName =
						(sh.sliceType >= 0 && sh.sliceType <= 2) ? kTypeNames[sh.sliceType] : "?";
					// Init CABAC réelle (brique 4) : contextes (§9.3.2.2) + moteur au début
					// de slice_data() dans le RBSP dé-émulé. Un flux valide exige
					// codIOffset < codIRange après l'init 9 bits (§9.3.1.2).
					NkVector<uint8> rbsp;
					NkHevcDecoder::DeemulateRbsp(nd, nal.size, rbsp);
					NkHevcCabacState cst;
					const int32 initType = NkHevcCabacState::InitTypeFor(sh.sliceType, sh.cabacInit);
					cst.Init(sh.sliceQp, initType);
					NkCabacEngine eng;
					eng.InitEngine(rbsp.Data(), (usize)rbsp.Size(), sh.dataByteOffset);
					const bool cabacOk =
						sh.dataByteOffset < (usize)rbsp.Size() && eng.codIOffset < eng.codIRange;
					if (!cabacOk)
						++sliceParseFailures;
					// Brique 9 : POC reel (pas juste picOrderCntLsb) + RefPicList0/1 en POC
					// (pas encore resolus en images -> brique DPB/NkVideoReader a venir).
					const int32 poc = NkHevcDecoder::ComputePoc(sh.picOrderCntLsb, sps.log2MaxPocLsb,
																 sh.isIdr, prevPocTid0);
					NkHevcRefPicLists refLists;
					NkHevcDecoder::BuildRefPicLists(sh.rps, poc, sh.numRefIdxL0Active, sh.numRefIdxL1Active,
													sh.sliceType == kHevcSliceB, refLists);
					char l0Buf[128] = "", l1Buf[128] = "";
					for (int32 k = 0; k < refLists.numL0; ++k)
						snprintf(l0Buf + strlen(l0Buf), sizeof(l0Buf) - strlen(l0Buf), "%s%d",
								 k ? "," : "", refLists.l0[k]);
					for (int32 k = 0; k < refLists.numL1; ++k)
						snprintf(l1Buf + strlen(l1Buf), sizeof(l1Buf) - strlen(l1Buf), "%s%d",
								 k ? "," : "", refLists.l1[k]);
					printf("  slice %2d : nal=%2d type=%s poc=%3d rps(neg=%d pos=%d) "
						   "tmvp=%d sao(y=%d c=%d) refs(l0=%d l1=%d) L0=[%s] L1=[%s] merge=%d qp=%d "
						   "entrees=%d data_off=%u cabac(init_type=%d offset9=%u %s)\n",
						   sliceHeaderCount, sh.nalType, typeName, poc, sh.rps.numNegativePics,
						   sh.rps.numPositivePics, sh.sliceTemporalMvpEnabled, sh.saoLuma, sh.saoChroma,
						   sh.numRefIdxL0Active, sh.numRefIdxL1Active, l0Buf, l1Buf, sh.maxNumMergeCand,
						   sh.sliceQp, sh.numEntryPointOffsets, (unsigned)sh.dataByteOffset, initType,
						   (unsigned)eng.codIOffset, cabacOk ? "ok" : "INVALIDE");
					prevPocTid0 = poc;
					// Brique 5 (I) + brique 8 (P/B) : decode STRUCTUREL complet du
					// slice_data (sao + quadtree + intra OU inter skip/merge/AMVP + residus
					// + WPP). Toute desynchronisation CABAC ferait echouer les
					// terminaisons -> echec compte comme KO. P/B = parse seul (pas de MC).
					{
						NkHevcSliceDataStats ds;
						if (NkHevcDecoder::ParseSliceDataIntra(nd, nal.size, sps, pps, sh, ds)) {
							printf("    slice_data %s : CTU=%d/%d rangees=%d CU=%d TU=%d "
								   "coeffs_nz=%lld qp_delta=%d ecart_entrees_max=%d octets\n",
								   typeName, ds.ctusParsed, ds.ctusParsed, ds.rows, ds.cuCount, ds.tuCount,
								   (long long)ds.nonZeroCoeffs, ds.qpDeltaCount, ds.maxSubsetDeviation);
						} else {
							++sliceParseFailures;
							printf("    [KO] slice_data %s : decode CTU echoue (desynchronisation)\n",
								   typeName);
						}
					}
				} else {
					++sliceParseFailures;
					printf("  [KO] slice NAL %llu (type=%d) : parsing echoue\n", (unsigned long long)i,
						   nal.type);
				}
			}
		}
		printf("  VPS=%d SPS=%d PPS=%d IDR=%d CRA=%d TRAIL/autres_slices=%d\n", nVps, nSps, nPps, nIdr, nCra,
			   nTrail);
		const bool ok =
			haveSps && sps.width > 0 && sps.height > 0 && sliceHeaderCount > 0 && sliceParseFailures == 0;
		printf("  slices parsees : %d (echecs=%d)\n", sliceHeaderCount, sliceParseFailures);
		printf("  [ %s ] parsing HEVC (briques 1-5+8 : NAL + VPS/SPS/PPS + slice header + "
			   "slice_data I/P/B complet : CTU/intra/inter skip-merge-AMVP/residus/WPP)\n",
			   ok ? "OK " : "KO");
		return ok ? 0 : 1;
	}

	// Mode validation HEVC (brique 6) : --hevcdecode <fichier.265> <ref.yuv>
	// Decode chaque slice I en PIXELS (prediction intra + dequant + transformees
	// inverses) et compare BIT-EXACT a la reference YUV produite par ffmpeg sur le
	// meme flux (yuv420p 8-bit ou yuv420p10le selon le SPS). Le flux doit etre
	// encode SANS deblocage ni SAO (les filtres en boucle = briques suivantes).
	if (argc >= 4 && strcmp(argv[1], "--hevcdecode") == 0) {
		FILE *f = fopen(argv[2], "rb");
		if (!f) {
			printf("  [KO] fichier introuvable : %s\n", argv[2]);
			return 1;
		}
		fseek(f, 0, SEEK_END);
		long n = ftell(f);
		fseek(f, 0, SEEK_SET);
		NkVector<uint8> buf;
		buf.Resize((usize)n);
		if (fread(buf.Data(), 1, (size_t)n, f) != (size_t)n) {
			fclose(f);
			printf("  [KO] lecture incomplete\n");
			return 1;
		}
		fclose(f);
		FILE *fr = fopen(argv[3], "rb");
		if (!fr) {
			printf("  [KO] reference introuvable : %s\n", argv[3]);
			return 1;
		}
		NkVector<NkHevcNal> nals;
		NkHevcDecoder::SplitNalsAnnexB(buf.Data(), (usize)buf.Size(), nals);
		NkHevcSps sps;
		NkHevcPps pps;
		bool haveSps = false, havePps = false;
		int32 frames = 0, framesOk = 0;
		int32 worstDiff = 0;
		for (uint64 i = 0; i < nals.Size(); ++i) {
			const NkHevcNal &nal = nals[i];
			const uint8 *nd = buf.Data() + nal.offset;
			if (nal.type == kHevcNalSps) {
				haveSps = NkHevcDecoder::ParseSps(nd, nal.size, sps);
			} else if (nal.type == kHevcNalPps) {
				havePps = NkHevcDecoder::ParsePps(nd, nal.size, pps);
			} else if (nal.type <= 31 && haveSps && havePps) {
				NkHevcSliceHeader sh;
				if (!NkHevcDecoder::ParseSliceHeader(nd, nal.size, sps, pps, sh) ||
					sh.sliceType != kHevcSliceI)
					continue;
				NkHevcFrame frame;
				NkHevcSliceDataStats ds;
				++frames;
				if (!NkHevcDecoder::DecodeSliceIntra(nd, nal.size, sps, pps, sh, frame, ds)) {
					printf("  trame %d : [KO] decode echoue\n", frames);
					continue;
				}
				// Lit la trame de reference (dimensions CROPPEES, 4:2:0, 8 ou 10 bits LE).
				const int32 cw = frame.cropW, ch = frame.cropH;
				const int32 ccw = cw >> 1, cch = ch >> 1;
				const int32 bytesPerSample = frame.bitDepth > 8 ? 2 : 1;
				const usize refSize = (usize)(cw * ch + 2 * ccw * cch) * bytesPerSample;
				NkVector<uint8> ref;
				ref.Resize(refSize);
				if (fread(ref.Data(), 1, refSize, fr) != refSize) {
					printf("  trame %d : [KO] reference trop courte\n", frames);
					break;
				}
				int32 maxDiff = 0;
				auto cmpPlane = [&](const nk_uint16 *plane, int32 stride, int32 w, int32 h,
									const uint8 *rp) {
					for (int32 y = 0; y < h; ++y)
						for (int32 x = 0; x < w; ++x) {
							const int32 ours = (int32)plane[y * stride + x];
							int32 theirs;
							if (bytesPerSample == 2)
								theirs = (int32)rp[(y * w + x) * 2] |
										 ((int32)rp[(y * w + x) * 2 + 1] << 8);
							else
								theirs = (int32)rp[y * w + x];
							const int32 d = ours > theirs ? ours - theirs : theirs - ours;
							if (d > maxDiff)
								maxDiff = d;
						}
				};
				const uint8 *rp = ref.Data();
				cmpPlane(frame.y.Data(), frame.lumaW, cw, ch, rp);
				rp += (usize)(cw * ch) * bytesPerSample;
				cmpPlane(frame.cb.Data(), frame.chromaW, ccw, cch, rp);
				rp += (usize)(ccw * cch) * bytesPerSample;
				cmpPlane(frame.cr.Data(), frame.chromaW, ccw, cch, rp);
				if (maxDiff > worstDiff)
					worstDiff = maxDiff;
				if (maxDiff == 0)
					++framesOk;
				printf("  trame %2d : %dx%d %d-bit CTU=%d coeffs=%lld maxdiff=%d %s\n", frames, cw, ch,
					   frame.bitDepth, ds.ctusParsed, (long long)ds.nonZeroCoeffs, maxDiff,
					   maxDiff == 0 ? "BIT-EXACT" : "[KO]");
			}
		}
		fclose(fr);
		const bool ok = frames > 0 && framesOk == frames;
		printf("  trames I decodees : %d, bit-exactes : %d/%d (pire ecart=%d)\n", frames, framesOk,
			   frames, worstDiff);
		printf("  [ %s ] decode HEVC intra vs ffmpeg (briques 6-7 : reconstruction + deblocage + SAO)\n",
			   ok ? "OK " : "KO");
		return ok ? 0 : 1;
	}

	// Mode validation HEVC (brique 11) : --hevcinter <fichier.265> <ref.yuv>
	// Decode la trame I PUIS TOUTES les trames P qui suivent, en chaine, avec un DPB
	// minimal (vecteur de trames deja decodees, recherche par POC) et les VRAIES
	// listes de reference (BuildRefPicLists, brique 9) resolues en pointeurs — donc
	// multi-reference reellement exerce des la 2e trame P (ref=2/3 dans le flux).
	// Compare en PIXELS chaque trame P a la reference ffmpeg (flux SANS deblocage/
	// SAO/ponderation explicite forcement, mais peut etre active). Position dans le
	// flux YUV brut retrouvee via le POC (ComputePoc), pas un index sequentiel.
	if (argc >= 4 && strcmp(argv[1], "--hevcinter") == 0) {
		FILE *f = fopen(argv[2], "rb");
		if (!f) {
			printf("  [KO] fichier introuvable : %s\n", argv[2]);
			return 1;
		}
		fseek(f, 0, SEEK_END);
		long n = ftell(f);
		fseek(f, 0, SEEK_SET);
		NkVector<uint8> buf;
		buf.Resize((usize)n);
		if (fread(buf.Data(), 1, (size_t)n, f) != (size_t)n) {
			fclose(f);
			printf("  [KO] lecture incomplete\n");
			return 1;
		}
		fclose(f);
		FILE *fr = fopen(argv[3], "rb");
		if (!fr) {
			printf("  [KO] reference introuvable : %s\n", argv[3]);
			return 1;
		}
		NkVector<NkHevcNal> nals;
		NkHevcDecoder::SplitNalsAnnexB(buf.Data(), (usize)buf.Size(), nals);
		NkHevcSps sps;
		NkHevcPps pps;
		bool haveSps = false, havePps = false;
		NkVector<NkHevcFrame> dpb;
		dpb.Reserve(256);
		int32 prevPocTid0 = 0;
		int32 pFrames = 0, pFramesOk = 0, worstDiff = 0;
		bool hardFail = false;
		auto findByPoc = [&](int32 poc) -> const NkHevcFrame * {
			for (uint64 k = 0; k < dpb.Size(); ++k)
				if (dpb[k].poc == poc)
					return &dpb[k];
			return nullptr;
		};
		for (uint64 i = 0; i < nals.Size() && !hardFail; ++i) {
			const NkHevcNal &nal = nals[i];
			const uint8 *nd = buf.Data() + nal.offset;
			if (nal.type == kHevcNalSps) {
				haveSps = NkHevcDecoder::ParseSps(nd, nal.size, sps);
				continue;
			}
			if (nal.type == kHevcNalPps) {
				havePps = NkHevcDecoder::ParsePps(nd, nal.size, pps);
				continue;
			}
			if (nal.type > 31 || !haveSps || !havePps)
				continue;
			NkHevcSliceHeader sh;
			if (!NkHevcDecoder::ParseSliceHeader(nd, nal.size, sps, pps, sh))
				continue;
			if (sh.sliceType == kHevcSliceB)
				continue; // bi-prediction : brique suivante — non attendu (bframes=0)
			const bool isIdr = nal.type == kHevcNalIdrWRadl || nal.type == kHevcNalIdrNLp;
			const int32 poc =
				NkHevcDecoder::ComputePoc(sh.picOrderCntLsb, sps.log2MaxPocLsb, isIdr, prevPocTid0);
			if (sh.sliceType == kHevcSliceI) {
				NkHevcFrame frame;
				frame.poc = poc;
				NkHevcSliceDataStats ds;
				if (!NkHevcDecoder::DecodeSliceIntra(nd, nal.size, sps, pps, sh, frame, ds)) {
					printf("  [KO] decode trame I echoue (poc=%d)\n", poc);
					hardFail = true;
					break;
				}
				printf("  trame I : poc=%d crop=%dx%d coded=%dx%d ctus=%d\n", poc, frame.cropW,
					   frame.cropH, frame.lumaW, frame.lumaH, ds.ctusParsed);
				prevPocTid0 = poc;
				dpb.PushBack(frame);
				continue;
			}
			if (sh.sliceType != kHevcSliceP)
				continue;
			NkHevcRefPicLists rpl;
			NkHevcDecoder::BuildRefPicLists(sh.rps, poc, sh.numRefIdxL0Active, 0, false, rpl);
			if (rpl.numL0 < sh.numRefIdxL0Active) {
				printf("  [KO] RefPicList0 incomplete (poc=%d)\n", poc);
				hardFail = true;
				break;
			}
			const NkHevcFrame *refsL0[16];
			bool resolveOk = true;
			for (int32 r = 0; r < sh.numRefIdxL0Active; ++r) {
				refsL0[r] = findByPoc(rpl.l0[r]);
				if (!refsL0[r])
					resolveOk = false;
			}
			if (!resolveOk) {
				printf("  [KO] resolution POC->trame echouee (poc=%d)\n", poc);
				hardFail = true;
				break;
			}
			NkHevcFrame frame;
			frame.poc = poc; // lu PAR DecodeSliceP (curPoc AMVP) : DOIT etre pose avant l'appel
			NkHevcSliceDataStats ds;
			if (!NkHevcDecoder::DecodeSliceP(nd, nal.size, sps, pps, sh, refsL0,
											 sh.numRefIdxL0Active, frame, ds)) {
				printf("  [KO] decode trame P echoue (poc=%d)\n", poc);
				hardFail = true;
				break;
			}
			prevPocTid0 = poc;
			++pFrames;
			const int32 cw = frame.cropW, ch = frame.cropH;
			const int32 ccw = cw >> 1, cch = ch >> 1;
			const usize frameSize = (usize)(cw * ch + 2 * ccw * cch);
			int32 maxDiff = -1;
			if (fseek(fr, (long)((usize)poc * frameSize), SEEK_SET) != 0) {
				printf("  [KO] seek reference echoue (poc=%d)\n", poc);
			} else {
				NkVector<uint8> ref;
				ref.Resize(frameSize);
				if (fread(ref.Data(), 1, frameSize, fr) != frameSize) {
					printf("  [KO] reference trop courte (poc=%d)\n", poc);
				} else {
					maxDiff = 0;
					auto cmpPlane = [&](const nk_uint16 *plane, int32 stride, int32 w, int32 h,
										const uint8 *rp) {
						for (int32 y = 0; y < h; ++y)
							for (int32 x = 0; x < w; ++x) {
								const int32 ours = (int32)plane[y * stride + x];
								const int32 theirs = (int32)rp[y * w + x];
								const int32 d = ours > theirs ? ours - theirs : theirs - ours;
								if (d > maxDiff)
									maxDiff = d;
							}
					};
					const uint8 *rp = ref.Data();
					cmpPlane(frame.y.Data(), frame.lumaW, cw, ch, rp);
					rp += (usize)(cw * ch);
					cmpPlane(frame.cb.Data(), frame.chromaW, ccw, cch, rp);
					rp += (usize)(ccw * cch);
					cmpPlane(frame.cr.Data(), frame.chromaW, ccw, cch, rp);
				}
			}
			if (maxDiff > worstDiff)
				worstDiff = maxDiff;
			if (maxDiff == 0)
				++pFramesOk;
			printf("  trame P : poc=%d refs=%d ctus=%d coeffs=%lld maxdiff=%d %s\n", poc,
				   sh.numRefIdxL0Active, ds.ctusParsed, (long long)ds.nonZeroCoeffs, maxDiff,
				   maxDiff == 0 ? "BIT-EXACT" : "[KO]");
			dpb.PushBack(frame);
		}
		fclose(fr);
		const bool ok = !hardFail && pFrames > 0 && pFramesOk == pFrames;
		printf("  trames P decodees : %d, bit-exactes : %d/%d (pire ecart=%d)\n", pFrames, pFramesOk,
			   pFrames, worstDiff);
		printf("  [ %s ] decode HEVC P multi-reference vs ffmpeg (brique 11 : MV merge/AMVP + mise a l'echelle, sans filtres en boucle)\n",
			   ok ? "OK " : "KO");
		return ok ? 0 : 1;
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

				// ── Brique 5 : residus (2eme partition) ──────────────────────────────
				// Les coefficients vivent dans une partition SEPAREE, qui commence juste
				// apres la 1ere. Ici on ne gere que le cas log2Partitions==0 (1 seule
				// partition de tokens) ; au-dela il y a des champs de taille 3 octets a
				// parser en tete, non implemente a ce stade.
				if (hdr.log2NbrOfDctPartitions == 0) {
					const usize tokStart = tag.headerSize + (usize)tag.firstPartSize;
					if (tokStart < (usize)frame.Size()) {
						const usize tokSize = (usize)frame.Size() - tokStart;
						NkVp8BoolDecoder tbd(frame.Data() + tokStart, tokSize);
						NkVp8ResidualStats st;
						if (NkVp8DecodeKeyFrameResiduals(tbd, fc, mbInfo, mbCols, mbRows, st)) {
							printf("  residus : MB decodes=%d sautes=%d | coeffs non nuls=%lld "
								   "eobTotal=%lld | plage coeff [%d..%d]\n",
								   st.decodedMbs, st.skippedMbs, (long long)st.nonZeroCoeffs,
								   (long long)st.eobTotal, st.minCoeff, st.maxCoeff);
							const bool mbCountOk = (st.decodedMbs + st.skippedMbs) == mbCols * mbRows;
							const bool tokConsumed = (tbd.pos == tokSize);
							const bool tokNoOverread = (tbd.overreadBytes <= 2);
							printf("  [ %s ] tous les MB traites (%d + %d = %d)\n",
								   mbCountOk ? "OK " : "KO", st.decodedMbs, st.skippedMbs,
								   mbCols * mbRows);
							printf("  [ %s ] partition de tokens consommee EXACTEMENT : %llu / %llu "
								   "octets (depassement=%d, <=2 normal)\n",
								   (tokConsumed && tokNoOverread) ? "OK " : "KO",
								   (unsigned long long)tbd.pos, (unsigned long long)tokSize,
								   tbd.overreadBytes);
							headerOk = headerOk && mbCountOk && tokConsumed && tokNoOverread;
						} else {
							printf("  [KO] NkVp8DecodeKeyFrameResiduals a echoue\n");
							headerOk = false;
						}
					}
				} else {
					printf("  (residus non testes : %d partitions de tokens, non gere a ce stade)\n",
						   1 << hdr.log2NbrOfDctPartitions);
				}
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
