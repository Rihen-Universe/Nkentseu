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
		printf("[ %s ] NkCeltAlloc : band_allocation + bissection qualite (budget monotone) + tilt trim\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}
	printf("\n=== Resultat : %d/%d suites OK ===\n", nbOk, nbTotal);
	return (nbOk == nbTotal) ? 0 : 1;
}
