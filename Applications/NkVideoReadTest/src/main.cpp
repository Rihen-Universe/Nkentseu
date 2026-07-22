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
		bool all = ok && okH264 && okCavlc && okVp9;
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
		int32 nSuperframes = 0;
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
														   sf.sizes[fr], h)) {
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
		printf("  [ %s ] parsing en-tetes VP9\n", allOk ? "OK " : "KO");
		return allOk ? 0 : 1;
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
