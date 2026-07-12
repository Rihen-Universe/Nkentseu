// =============================================================================
// NKMediaTest — self-test du probe NKMedia + probe d'un fichier réel (optionnel).
//   NKMediaTest.exe            → self-test (détection conteneurs, vint EBML)
//   NKMediaTest.exe <fichier>  → démux d'en-tête : conteneur + pistes + codecs
// =============================================================================
#include "NKMedia/NkMediaProbe.h"
#include "NKMedia/NkMediaDemux.h"
#include "NKMedia/Codecs/Opus/NkOpusPacket.h"
#include "NKMedia/Codecs/Opus/NkOpusRange.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltLaplace.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltBands.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltMdct.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltEnergy.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltPvq.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltRate.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltAlloc.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltVq.h"
#include "NKMedia/Codecs/Aac/NkAacBitReader.h"
#include "NKMedia/Codecs/Aac/NkAacTables.h"
#include "NKMedia/Codecs/Aac/NkAacHuffman.h"
#include "NKMedia/Codecs/Aac/NkAacIcs.h"
#include "NKMedia/Codecs/Aac/NkAacDequant.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltDenorm.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltDeemphasis.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltDecoder.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltAntiCollapse.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltSplit.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltQuantBands.h"
#include "NKMedia/Codecs/Opus/Silk/NkSilkGains.h"
#include "NKMedia/Codecs/Opus/Silk/NkSilkLpc.h"
#include "NKMedia/Codecs/Opus/Silk/NkSilkNlsf.h"
#include "NKMedia/Codecs/Opus/Silk/NkSilkLtp.h"
#include "NKMedia/Codecs/Opus/Silk/NkSilkExcitation.h"
#include "NKMedia/Codecs/Opus/Silk/NkSilkFrameType.h"
#include "NKMedia/Codecs/Opus/Silk/NkSilkSynthesis.h"
#include "NKMedia/Codecs/Opus/Silk/NkSilkIndices.h"
#include "NKMedia/Codecs/Opus/Silk/NkSilkDecoder.h"
#include "NKMedia/Codecs/Opus/Silk/NkSilkTop.h"
#include "NKMedia/Codecs/Opus/Silk/NkSilkResampler.h"
#include "NKMedia/Codecs/Opus/NkOpusDecoder.h"
#include "NKMedia/Codecs/Opus/NkOpusFile.h"
#include "NKMedia/Codecs/Video/H264/NkH264Transform.h"
#include "NKMedia/Codecs/Video/H264/NkH264Cavlc.h"
#include "NKMedia/Codecs/Video/H264/NkH264Encoder.h"
#include "NKMedia/Video/NkVideoConverter.h"
#include "NKMedia/Video/NkVideoRecorder.h"
#include "NKMedia/Video/NkVideoWriter.h"
#include "NKMedia/Video/NkImageSequenceWriter.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NkFunctions.h"

#include <cstdio>

using namespace nkentseu;

static const char *TrackTypeName(media::NkMediaTrackType t) {
	switch (t) {
		case media::NkMediaTrackType::NK_AUDIO:
			return "audio";
		case media::NkMediaTrackType::NK_VIDEO:
			return "video";
		default:
			return "?";
	}
}

int main(int argc, char **argv) {
	// Mode validation SILK : --silk <flux.webm> <sortie.pcm> (PCM s16le débit interne).
	if (argc >= 4 && NkString(argv[1]) == NkString("--silk")) {
		NkVector<nk_uint8> bytes;
		media::NkMediaInfo info;
		NkVector<media::NkMediaPacket> packets;
		if (!media::NkMediaDemux::ExtractAudioPacketsFile(argv[2], bytes, info, packets)) {
			printf("[ERREUR] demux %s\n", argv[2]);
			return 1;
		}
		// Config depuis le TOC du 1er paquet (SILK-only : config 0..11).
		const nk_uint8 toc = bytes[packets[0].offset];
		const int cfg = toc >> 3, bw = cfg / 4, fsz = cfg % 4;
		const int fs_kHz = (bw == 0) ? 8 : (bw == 1) ? 12 : 16;
		int nb_subfr = 4, nFrames = 1;
		if (fsz == 0) {
			nb_subfr = 2;
			nFrames = 1;
		} else if (fsz == 1) {
			nb_subfr = 4;
			nFrames = 1;
		} else if (fsz == 2) {
			nb_subfr = 4;
			nFrames = 2;
		} else {
			nb_subfr = 4;
			nFrames = 3;
		}
		// Option : 4e arg "48" → rééchantillonne vers 48 kHz (sortie Opus standard).
		const bool to48 = (argc >= 5 && NkString(argv[4]) == NkString("48"));
		media::NkSilkTop top;
		top.Init(fs_kHz, nb_subfr);
		media::NkSilkResampler rs;
		if (to48)
			rs.Init(fs_kHz * 1000, 48000);
		nk_int16 sMid[2] = {0, 0}; // tampon inter-trames (comme silk_Decode)
		NkVector<nk_int16> pcm;
		nk_int16 frameOut[960];
		for (uint64 p = 0; p < packets.Size(); ++p) {
			const nk_uint8 *pl = bytes.Data() + packets[p].offset;
			const uint32 sz = (uint32)packets[p].size;
			if (sz < 2)
				continue;
			media::NkOpusPacketInfo op;
			if (!media::NkOpusPacket::Parse(pl, sz, op))
				continue;
			for (int32 fr = 0; fr < op.frameCount; ++fr) {
				media::NkOpusRangeDecoder rd;
				rd.Init(pl + op.frames[fr].offset, (uint32)op.frames[fr].size);
				const int32 n = top.DecodePacket(rd, nFrames, frameOut);
				if (!to48) {
					for (int32 i = 0; i < n; ++i)
						pcm.PushBack(frameOut[i]);
				} else {
					// Prépend 2 échantillons tampon, rééchantillonne, sauve les 2 derniers.
					nk_int16 tmp[2 + 960];
					tmp[0] = sMid[0];
					tmp[1] = sMid[1];
					for (int32 i = 0; i < n; ++i)
						tmp[2 + i] = frameOut[i];
					sMid[0] = frameOut[n - 2];
					sMid[1] = frameOut[n - 1];
					nk_int16 out48[960 * 3 + 16];
					rs.Process(out48, &tmp[1], n);
					const int32 nOut = n * 48 / fs_kHz;
					for (int32 i = 0; i < nOut; ++i)
						pcm.PushBack(out48[i]);
				}
			}
		}
		FILE *f = fopen(argv[3], "wb");
		if (f) {
			fwrite(pcm.Data(), sizeof(nk_int16), pcm.Size(), f);
			fclose(f);
		}
		printf("[SILK] %d paquets -> %d echantillons @ %d kHz -> %s\n", (int)packets.Size(), (int)pcm.Size(),
			   to48 ? 48 : fs_kHz, argv[3]);
		return 0;
	}
	// Décodeur Opus complet (dispatch CELT/SILK) : --opus <flux.webm> <out.pcm> (48 kHz s16le).
	if (argc >= 4 && NkString(argv[1]) == NkString("--opus")) {
		NkVector<nk_uint8> bytes;
		media::NkMediaInfo info;
		NkVector<media::NkMediaPacket> packets;
		if (!media::NkMediaDemux::ExtractAudioPacketsFile(argv[2], bytes, info, packets))
			return 1;
		media::NkOpusDecoder dec;
		dec.Init(1);
		NkVector<nk_int16> pcm;
		nk_int16 out48[960 * 4];
		for (uint64 p = 0; p < packets.Size(); ++p) {
			const int32 n = dec.DecodePacket(bytes.Data() + packets[p].offset, (int32)packets[p].size, out48);
			for (int32 i = 0; i < n; ++i)
				pcm.PushBack(out48[i]);
		}
		FILE *f = fopen(argv[3], "wb");
		if (f) {
			fwrite(pcm.Data(), sizeof(nk_int16), pcm.Size(), f);
			fclose(f);
		}
		printf("[OPUS] %d paquets -> %d echantillons @ 48 kHz -> %s\n", (int)packets.Size(), (int)pcm.Size(), argv[3]);
		return 0;
	}
	// Fichier .opus (Ogg-Opus) complet : --oggopus <in.opus> <out.pcm> (48 kHz s16le mono).
	if (argc >= 4 && NkString(argv[1]) == NkString("--oggopus")) {
		NkVector<nk_int16> pcm;
		nk_int32 channels = 0;
		nk_int32 rate = 0;
		NkString err;
		if (!media::NkOpusFile::DecodeFile(argv[2], pcm, channels, rate, &err)) {
			printf("[OGGOPUS] ERREUR : %s\n", err.CStr());
			return 1;
		}
		FILE *f = fopen(argv[3], "wb");
		if (f) {
			fwrite(pcm.Data(), sizeof(nk_int16), pcm.Size(), f);
			fclose(f);
		}
		printf("[OGGOPUS] %s -> %d echantillons @ %d Hz (%d canal) -> %s\n", argv[2], (int)pcm.Size(), rate,
			   channels, argv[3]);
		return 0;
	}
	// Dump des paquets Opus bruts : --dumppkts <flux.webm> <out.pkts> ([u32 len][octets]*).
	if (argc >= 4 && NkString(argv[1]) == NkString("--dumppkts")) {
		NkVector<nk_uint8> bytes;
		media::NkMediaInfo info;
		NkVector<media::NkMediaPacket> packets;
		if (!media::NkMediaDemux::ExtractAudioPacketsFile(argv[2], bytes, info, packets))
			return 1;
		FILE *f = fopen(argv[3], "wb");
		for (uint64 p = 0; p < packets.Size(); ++p) {
			nk_uint32 len = (nk_uint32)packets[p].size;
			fwrite(&len, 4, 1, f);
			fwrite(bytes.Data() + packets[p].offset, 1, len, f);
		}
		fclose(f);
		printf("[DUMP] %d paquets -> %s\n", (int)packets.Size(), argv[3]);
		return 0;
	}
	if (argc >= 2) {
		media::NkMediaInfo info;
		if (!media::NkMediaProbe::ProbeFile(argv[1], info)) {
			printf("[ERREUR] probe echoue : %s\n", argv[1]);
			return 1;
		}
		printf("Fichier   : %s\n", argv[1]);
		printf("Conteneur : %s\n", info.ContainerName());
		printf("Pistes    : %d\n", (int)info.tracks.Size());
		for (uint64 i = 0; i < info.tracks.Size(); ++i) {
			const media::NkMediaTrack &t = info.tracks[i];
			printf("  [%d] %-5s codec=%-6s", (int)i, TrackTypeName(t.type), t.codec.CStr());
			if (t.type == media::NkMediaTrackType::NK_AUDIO)
				printf(" %d Hz, %d canal(aux)", t.sampleRate, t.channels);
			if (t.type == media::NkMediaTrackType::NK_VIDEO)
				printf(" %dx%d", t.width, t.height);
			printf("\n");
		}
		// Extraction des paquets audio.
		NkVector<nk_uint8> bytes;
		media::NkMediaInfo info2;
		NkVector<media::NkMediaPacket> packets;
		if (media::NkMediaDemux::ExtractAudioPacketsFile(argv[1], bytes, info2, packets)) {
			usize total = 0;
			for (uint64 i = 0; i < packets.Size(); ++i)
				total += packets[i].size;
			printf("Paquets audio : %d (total %d octets)\n", (int)packets.Size(), (int)total);
			if (packets.Size() > 0) {
				printf("  1er paquet  : offset=%d size=%d ts=%lldms\n", (int)packets[0].offset,
					   (int)packets[0].size, (long long)packets[0].timestampMs);
				const media::NkMediaPacket &last = packets[packets.Size() - 1];
				printf("  dernier     : ts=%lldms\n", (long long)last.timestampMs);
			}
			// Si Opus : parse le TOC du 1er paquet (étape 1 du décodeur Opus).
			const media::NkMediaTrack *au = info2.FirstAudio();
			if (au && au->codec == NkString("opus") && packets.Size() > 0) {
				media::NkOpusPacketInfo op;
				if (media::NkOpusPacket::Parse(bytes.Data() + packets[0].offset, packets[0].size, op)) {
					const char *modeName = op.mode == media::NkOpusMode::NK_SILK_ONLY ? "SILK"
										   : op.mode == media::NkOpusMode::NK_HYBRID ? "Hybrid" : "CELT";
					printf("  Opus TOC    : config=%d mode=%s %.1fms %s frames=%d\n", op.config, modeName,
						   op.frameSizeMs, op.stereo ? "stereo" : "mono", op.frameCount);
				}
			}
		} else {
			printf("Paquets audio : extraction non supportee pour ce conteneur.\n");
		}
		return 0;
	}

	printf("=== NKMediaTest — probe + demux conteneurs (headless) ===\n\n");
	int nbOk = 0, nbTotal = 0;
	{
		++nbTotal;
		const bool ok = media::NkMediaProbe::SelfTest();
		printf("[ %s ] NkMediaProbe : detection MP4/WebM/WAV/OGG/FLAC/MP3 + vint EBML\n", ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkMediaDemux::SelfTest();
		printf("[ %s ] NkMediaDemux : vint EBML + lecture big-endian\n", ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkOpusPacket::SelfTest();
		printf("[ %s ] NkOpusPacket : table config + decoupage trames codes 0-3\n", ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkOpusRange::SelfTest();
		printf("[ %s ] NkOpusRange : range coder aller-retour (icdf, bits bruts, uint, cdf)\n", ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkCeltLaplace::SelfTest();
		printf("[ %s ] NkCeltLaplace : coder de Laplace CELT aller-retour (energie grossiere)\n", ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkCeltBands::SelfTest();
		printf("[ %s ] NkCeltBands : 21 bandes CELT, bornes croissantes, echelle LM\n", ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkCeltMdct::SelfTest();
		printf("[ %s ] NkCeltMdct : MDCT/IMDCT + fenetre sinus, reconstruction TDAC parfaite\n", ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkCeltEnergy::SelfTest();
		printf("[ %s ] NkCeltEnergy : energie grossiere (Laplace) + fine (bits bruts) aller-retour, LM 0-3 intra/inter\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkCeltPvq::SelfTest();
		printf("[ %s ] NkCeltPvq : V(n,k), CWRS vecteur<->index exhaustif, aller-retour range coder\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkCeltRate::SelfTest();
		printf("[ %s ] NkCeltRate : bits<->pulses (coeur allocation), monotonie + aller-retour + budget\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkCeltAlloc::SelfTest();
		printf("[ %s ] NkCeltAlloc : band_allocation + bissection qualite + interp fine (bits/bande, budget monotone)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkCeltVq::SelfTest();
		printf("[ %s ] NkCeltVq : forme de bande (pulses->normalise->rotation), norme conservee\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkCeltDenorm::SelfTest();
		printf("[ %s ] NkCeltDenorm : denormalisation bandes (forme x energie), norme = 2^(E+eMean)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkCeltDeemphasis::SelfTest();
		printf("[ %s ] NkCeltDeemphasis : filtre sortie, aller-retour preemph/deemph + reponse impulsionnelle\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkCeltDecoder::SelfTest();
		printf("[ %s ] NkCeltDecoder : orchestration (flags ec) + trame silence -> PCM zero (assemblage en cours)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkCeltAntiCollapse::SelfTest();
		printf("[ %s ] NkCeltAntiCollapse : LCG + renormalise + bande effondree->bruit calibre, non-effondree intacte\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		bool ok = media::NkCeltSplit::SelfTest();
		// TellFrac : sanity vs Tell (prerequis budget par bande).
		{
			uint8 b[64];
			media::NkOpusRangeEncoder e;
			e.Init(b, 64);
			for (int i = 0; i < 20; ++i)
				e.Encode((uint32)(i % 7), (uint32)(i % 7) + 1, 8);
			e.Done();
			media::NkOpusRangeDecoder d;
			d.Init(b, 64);
			for (int i = 0; i < 5; ++i) {
				const uint32 fs = d.Decode(8);
				d.Update(fs, fs + 1, 8);
			}
			const int32 tell = d.Tell();
			const uint32 tf = d.TellFrac();
			// TellFrac en 1/8 bit ≈ Tell*8 (à moins d'un bit près).
			if ((int32)(tf >> 3) < tell - 1 || (int32)(tf >> 3) > tell + 1)
				ok = false;
		}
		printf("[ %s ] NkCeltSplit : haar1 (orthonormale+involutive) + compute_qn + TellFrac (prereq quant_all_bands)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkCeltQuantBands::SelfTest();
		printf("[ %s ] NkCeltQuantBands : decodage des bandes (compute_theta+quant_partition+folding), spectre fini\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkSilkGains::SelfTest();
		printf("[ %s ] NkSilkGains : gains SILK (index indep/cond/delta) aller-retour + log2lin + dequant monotone\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkSilkLpc::SelfTest();
		printf("[ %s ] NkSilkLpc : NLSF2A (cos LSF + convolution + LPC_fit) + inverse_pred_gain (stabilite)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkSilkNlsf::SelfTest();
		printf("[ %s ] NkSilkNlsf : codebook NB/MB+WB (index aller-retour) + reconstruction NLSF ordonnee -> LPC stable\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkSilkLtp::SelfTest();
		printf("[ %s ] NkSilkLtp : pitch lags (contour CB) bornes + gains LTP Q14 (VQ 8/16/32) + echelle\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkSilkExcitation::SelfTest();
		printf("[ %s ] NkSilkExcitation : shell-code pulses (rate/counts/shell/signes) aller-retour bit-exact\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkSilkFrameType::SelfTest();
		printf("[ %s ] NkSilkFrameType : type de trame (VAD/non-VAD) + interp NLSF + seed, aller-retour\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkSilkSynthesis::SelfTest();
		printf("[ %s ] NkSilkSynthesis : coeur DSP (excitation LCG + LTP + filtre LPC + gain) exact + deterministe\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkSilkIndices::SelfTest();
		printf("[ %s ] NkSilkIndices : assemblage decode_indices (type+gains+NLSF+pitch/LTP+seed) aller-retour\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkSilkDecoder::SelfTest();
		printf("[ %s ] NkSilkDecoder : decode_parameters (gains/LPC/pitch) + decode_frame (flux->PCM) deterministe\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkSilkTop::SelfTest();
		printf("[ %s ] NkSilkTop : top-level paquet (en-tete VAD/LBRR + boucle trames) -> PCM interne\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkSilkResampler::SelfTest();
		printf("[ %s ] NkSilkResampler : 16k->48k (up2 HQ + FIR frac), sortie bornee + deterministe\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkOpusDecoder::SelfTest();
		printf("[ %s ] NkOpusDecoder : dispatch TOC (CELT/SILK) -> 48 kHz (trame CELT silence 20ms=960)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkOpusFile::SelfTest();
		printf("[ %s ] NkOpusFile : Ogg-Opus forge (OpusHead/Tags/EOS) -> demux + decode + pre-skip + trim\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkAacBitReader::SelfTest();
		printf("[ %s ] NkAacBitReader : lecteur de bits MSB-first (round-trip + peek + align + overrun)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkAacTables::SelfTest();
		printf("[ %s ] NkAacTables : sample rates + swb_offset (monotone->1024/128) + fenetres sine/KBD (PR=1)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkAacHuffman::SelfTest();
		printf("[ %s ] NkAacHuffman : codebooks 1-11 + scalefactor (round-trip toutes entrees + signe + escape)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkAacIcs::SelfTest();
		printf("[ %s ] NkAacIcs : individual_channel_stream (ics_info+sections+scalefactors+spectral, round-trip)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkAacDequant::SelfTest();
		printf("[ %s ] NkAacDequant : iquant x^4/3 + gain 2^((sf-100)/4) + pulses + desentrelacement short\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkH264Transform::SelfTest();
		printf("[ %s ] NkH264Transform : transformee 4x4 entiere + Hadamard + quant/dequant QP (round-trip)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkH264Cavlc::SelfTest();
		printf("[ %s ] NkH264Cavlc : codage entropique residus (coeff_token/levels/total_zeros/run_before)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	{
		++nbTotal;
		const bool ok = media::NkH264Encoder::SelfTest();
		printf("[ %s ] NkH264Encoder : flux Annex-B SPS/PPS/IDR + macroblocs I_16x16 CAVLC (structurel)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	// Écrit un flux .h264 élémentaire ET un .mp4 cliquable (dégradé + boîte mobile + damier) pour ffmpeg.
	{
		const int32 W = 128, H = 96;
		NkVector<uint8> frame;
		frame.Resize((uint64)W * H * 3);
		auto fillFrame = [&](int32 f) {
			for (int32 y = 0; y < H; ++y)
				for (int32 x = 0; x < W; ++x) {
					uint8 *p = frame.Data() + ((uint64)y * W + x) * 3;
					p[0] = (uint8)((x * 2) & 0xFF); // R : dégradé horizontal (fond statique)
					p[1] = (uint8)((y * 2) & 0xFF); // G : dégradé vertical
					p[2] = 100;						// B constant
					const int32 boxx = 8 + f * 6;	// boîte mobile → mouvement (P-slices)
					if (x >= boxx && x < boxx + 28 && y >= 34 && y < 66) {
						p[0] = 240;
						p[1] = 230;
						p[2] = 40;
					}
					if (x >= W - 28 && y < 28) { // damier fin → intra I_4x4
						const uint8 tex = (uint8)(((x ^ y) & 1) ? 40 : 210);
						p[0] = tex;
						p[1] = tex;
						p[2] = tex;
					}
				}
		};

		const char *path = (argc >= 3) ? argv[2] : "h264_test.h264";
		media::NkH264Encoder enc;
		if (enc.Open(path, 128, 96, 25, 1, 26)) {
			enc.EnableReconDump("h264_recon.yuv"); // validation déblocage vs ffmpeg
			for (int32 f = 0; f < 12; ++f) {
				fillFrame(f);
				enc.WriteFrame(frame.Data(), media::NkVideoInputFormat::RGB24);
			}
			enc.Close();
			printf("[i] Flux H.264 ecrit : %s (12 trames 128x96, QP26)\n", path);
		} else {
			printf("[!] Ouverture %s impossible\n", path);
		}

		// Conteneur MP4 : vidéo H.264 + 2 pistes AUDIO (langues fr/en, bips distincts) + sous-titres.
		media::NkH264Encoder encMp4;
		if (encMp4.Open("h264_test.mp4", 128, 96, 25, 1, 26)) {
			const int32 rate = 44100, aPerFrame = rate / 25; // 1764 trames audio / trame vidéo
			const int32 aFr = encMp4.AddAudioTrack(rate, 1, "fre");	 // piste FR : bip 440 Hz
			const int32 aEn = encMp4.AddAudioTrack(rate, 1, "eng");	 // piste EN : bip 660 Hz
			// sous-titres : n'importe quelle langue, sans restriction (fr, en, ghomala' bbj, bassa bas)
			const int32 sFr = encMp4.AddSubtitleTrack("fre");
			const int32 sEn = encMp4.AddSubtitleTrack("eng");
			const int32 sGh = encMp4.AddSubtitleTrack("bbj"); // ghomala'
			const int32 sBa = encMp4.AddSubtitleTrack("bas"); // bassa
			NkVector<int16> t440, t660;
			t440.Resize((uint64)aPerFrame);
			t660.Resize((uint64)aPerFrame);
			int64 phase = 0;
			for (int32 f = 0; f < 12; ++f) {
				fillFrame(f);
				encMp4.WriteFrame(frame.Data(), media::NkVideoInputFormat::RGB24);
				for (int32 i = 0; i < aPerFrame; ++i, ++phase) {
					const double tt = (double)phase / rate;
					t440[(uint64)i] = (int16)(8000.0 * nkentseu::math::NkSin((float32)(6.2831853 * 440.0 * tt)));
					t660[(uint64)i] = (int16)(8000.0 * nkentseu::math::NkSin((float32)(6.2831853 * 660.0 * tt)));
				}
				encMp4.WriteAudioPcm(aFr, t440.Data(), (uint32)aPerFrame);
				encMp4.WriteAudioPcm(aEn, t660.Data(), (uint32)aPerFrame);
			}
			// sous-titres : 2 répliques (0-240ms, 240-480ms) dans chaque langue.
			encMp4.AddSubtitle(sFr, "Bonjour Nkentseu", 0, 240);
			encMp4.AddSubtitle(sFr, "Encodeur H.264 maison", 240, 240);
			encMp4.AddSubtitle(sEn, "Hello Nkentseu", 0, 240);
			encMp4.AddSubtitle(sEn, "Home-made H.264 encoder", 240, 240);
			encMp4.AddSubtitle(sGh, "Wa nkentseu", 0, 240); // ghomala' (exemple)
			encMp4.AddSubtitle(sGh, "Encodeur H.264", 240, 240);
			encMp4.AddSubtitle(sBa, "Mbolo Nkentseu", 0, 240); // bassa (exemple)
			encMp4.AddSubtitle(sBa, "Encodeur H.264", 240, 240);
			encMp4.Close();
			printf("[i] MP4 ecrit : h264_test.mp4 — video + 2 audio (fr/en) + 4 sous-titres (fr/en/bbj/bas)\n");
		} else {
			printf("[!] Ouverture h264_test.mp4 impossible\n");
		}
	}
	// Pipeline de conversion : (a) séquence PNG → H.264 MP4 ; (b) transcode MJPEG AVI → H.264 MP4.
	{
		const int32 W = 96, H = 64;
		NkVector<uint8> fr;
		fr.Resize((uint64)W * H * 3);
		auto fill = [&](int32 f) {
			for (int32 y = 0; y < H; ++y)
				for (int32 x = 0; x < W; ++x) {
					uint8 *p = fr.Data() + ((uint64)y * W + x) * 3;
					p[0] = (uint8)((x * 2 + f * 8) & 0xFF);
					p[1] = (uint8)((y * 3) & 0xFF);
					p[2] = (uint8)((80 + f * 10) & 0xFF);
				}
		};
		// (a) écrit une séquence PNG puis la convertit en MP4.
		{
			media::NkImageSequenceWriter seq;
			if (seq.Open(".", "cvframe", W, H, media::NkImageSeqFormat::PNG, 3)) {
				for (int32 f = 0; f < 10; ++f) {
					fill(f);
					seq.WriteFrame(fr.Data(), media::NkVideoInputFormat::RGB24);
				}
				seq.Close();
			}
			const int32 n =
				media::NkVideoConverter::ImageSequenceToVideo("cvframe_", 3, ".png", 1, 10, "conv_seq.mp4", 25, 1);
			printf("[i] Conversion sequence PNG -> conv_seq.mp4 : %d trames\n", n);
		}
		// (b) écrit un MJPEG AVI puis le transcode en H.264 MP4 (vrai décode→réencode).
		{
			media::NkVideoWriter avi;
			media::NkVideoConfig cfg;
			cfg.width = W;
			cfg.height = H;
			cfg.fpsNum = 25;
			cfg.fpsDen = 1;
			cfg.codec = media::NkVideoCodec::MJPEG;
			cfg.container = media::NkVideoContainer::AVI;
			if (avi.Open("conv_src.avi", cfg)) {
				for (int32 f = 0; f < 10; ++f) {
					fill(f);
					avi.WriteFrame(fr.Data(), media::NkVideoInputFormat::RGB24);
				}
				avi.Close();
			}
			const int32 n = media::NkVideoConverter::MjpegAviToVideo("conv_src.avi", "conv_trans.mp4");
			printf("[i] Transcode MJPEG AVI -> H.264 conv_trans.mp4 : %d trames\n", n);
		}
	}

	// Capture moteur → vidéo+audio : simule un framebuffer RGBA rendu + son + langues + sous-titres.
	{
		const int32 W = 128, H = 96, fps = 30, rate = 48000, aPerFrame = rate / fps;
		media::NkVideoRecorder rec;
		if (rec.Begin("engine_capture.mp4", W, H, fps, 1, 22)) {
			const int32 aFr = rec.AddAudio(rate, 1, "fre"); // 2 langues audio
			const int32 aEn = rec.AddAudio(rate, 1, "eng");
			const int32 sFr = rec.AddSubtitleTrack("fre"); // 2 langues sous-titres (dont ghomala')
			const int32 sGh = rec.AddSubtitleTrack("bbj");
			NkVector<uint8> fb;
			fb.Resize((uint64)W * H * 4); // framebuffer RGBA (comme une capture DX11)
			NkVector<int16> af, ae;
			af.Resize((uint64)aPerFrame);
			ae.Resize((uint64)aPerFrame);
			int64 ph = 0;
			for (int32 f = 0; f < 30; ++f) { // 1 seconde
				// "rendu" : dégradé animé + carré mobile (simule NKRenderer/NKCanvas)
				for (int32 y = 0; y < H; ++y)
					for (int32 x = 0; x < W; ++x) {
						uint8 *p = fb.Data() + ((uint64)y * W + x) * 4;
						p[0] = (uint8)((x * 2) & 0xFF);
						p[1] = (uint8)((y * 2 + f * 4) & 0xFF);
						p[2] = (uint8)((100 + f * 5) & 0xFF);
						p[3] = 255;
						const int32 bx = 4 + f * 4;
						if (x >= bx && x < bx + 20 && y >= 40 && y < 60) {
							p[0] = 255;
							p[1] = 220;
							p[2] = 0;
						}
					}
				rec.PushVideo(fb.Data(), media::NkVideoInputFormat::RGBA32); // trame capturée
				for (int32 i = 0; i < aPerFrame; ++i, ++ph) {
					const double t = (double)ph / rate;
					af[(uint64)i] = (int16)(8000.0 * nkentseu::math::NkSin((float32)(6.2831853 * 440.0 * t)));
					ae[(uint64)i] = (int16)(8000.0 * nkentseu::math::NkSin((float32)(6.2831853 * 660.0 * t)));
				}
				rec.PushAudio(aFr, af.Data(), (uint32)aPerFrame);
				rec.PushAudio(aEn, ae.Data(), (uint32)aPerFrame);
			}
			rec.AddSubtitle(sFr, "Rendu du moteur Nkentseu", 0, 500);
			rec.AddSubtitle(sFr, "Capture video + audio", 500, 500);
			rec.AddSubtitle(sGh, "Wa Nkentseu", 0, 500);
			rec.End();
			printf("[i] Capture moteur -> engine_capture.mp4 : %d trames (video+2 audio fr/en+2 sous-titres)\n",
				   rec.FrameCount());
		}
	}

	printf("\n=== Resultat : %d/%d suites OK ===\n", nbOk, nbTotal);
	return (nbOk == nbTotal) ? 0 : 1;
}
