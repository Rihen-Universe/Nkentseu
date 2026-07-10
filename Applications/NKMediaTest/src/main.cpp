// =============================================================================
// NKMediaTest — self-test du probe NKMedia + probe d'un fichier réel (optionnel).
//   NKMediaTest.exe            → self-test (détection conteneurs, vint EBML)
//   NKMediaTest.exe <fichier>  → démux d'en-tête : conteneur + pistes + codecs
// =============================================================================
#include "NKMedia/NkMediaProbe.h"

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
		return 0;
	}

	printf("=== NKMediaTest — probe conteneurs (headless) ===\n\n");
	const bool ok = media::NkMediaProbe::SelfTest();
	printf("[ %s ] NkMediaProbe : detection MP4/WebM/WAV/OGG/FLAC/MP3 + vint EBML\n", ok ? "OK " : "FAIL");
	printf("\n=== Resultat : %d/1 suites OK ===\n", ok ? 1 : 0);
	return ok ? 0 : 1;
}
