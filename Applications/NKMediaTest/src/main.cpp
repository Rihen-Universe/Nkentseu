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
#include "NKMedia/Codecs/Opus/Celt/NkCeltDenorm.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltDeemphasis.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltDecoder.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltAntiCollapse.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltSplit.h"
#include "NKMedia/Codecs/Opus/Celt/NkCeltQuantBands.h"
#include "NKMedia/Codecs/Video/H264/NkH264Transform.h"
#include "NKMedia/Codecs/Video/H264/NkH264Cavlc.h"
#include "NKMedia/Codecs/Video/H264/NkH264Encoder.h"
#include "NKContainers/Sequential/NkVector.h"

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
	// Écrit un vrai flux .h264 (dégradé animé) pour validation externe par ffmpeg.
	{
		const char *path = (argc >= 3) ? argv[2] : "h264_test.h264";
		media::NkH264Encoder enc;
		if (enc.Open(path, 128, 96, 25, 1, 26)) {
			enc.EnableReconDump("h264_recon.yuv"); // validation déblocage vs ffmpeg
			const int32 W = 128, H = 96;
			NkVector<uint8> frame;
			frame.Resize((uint64)W * H * 3);
			for (int32 f = 0; f < 12; ++f) {
				for (int32 y = 0; y < H; ++y)
					for (int32 x = 0; x < W; ++x) {
						uint8 *p = frame.Data() + ((uint64)y * W + x) * 3;
						p[0] = (uint8)((x * 2) & 0xFF); // R : dégradé horizontal (fond statique)
						p[1] = (uint8)((y * 2) & 0xFF); // G : dégradé vertical
						p[2] = 100;						// B constant
						// boîte qui se déplace → mouvement pour les P-slices (MC)
						const int32 boxx = 8 + f * 6;
						if (x >= boxx && x < boxx + 28 && y >= 34 && y < 66) {
							p[0] = 240;
							p[1] = 230;
							p[2] = 40;
						}
						// damier fin (coin) → force aussi de l'intra I_4x4
						if (x >= W - 28 && y < 28) {
							const uint8 tex = (uint8)(((x ^ y) & 1) ? 40 : 210);
							p[0] = tex;
							p[1] = tex;
							p[2] = tex;
						}
					}
				enc.WriteFrame(frame.Data(), media::NkVideoInputFormat::RGB24);
			}
			enc.Close();
			printf("[i] Flux H.264 ecrit : %s (12 trames 128x96, QP26) — a valider : ffmpeg -i %s out.png\n", path,
				   path);
		} else {
			printf("[!] Ouverture %s impossible (ecriture flux H.264 sautee)\n", path);
		}
	}
	printf("\n=== Resultat : %d/%d suites OK ===\n", nbOk, nbTotal);
	return (nbOk == nbTotal) ? 0 : 1;
}
