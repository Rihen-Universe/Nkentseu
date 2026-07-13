// =============================================================================
// NkAudioPlayer — lecteur audio de RÉFÉRENCE (usage propre de NKAudio).
// -----------------------------------------------------------------------------
// Le patron CORRECT pour lire un fichier audio vers les haut-parleurs :
//   1. AudioEngine::Instance().Initialize()  -> ouvre le device (WASAPI) + thread de mixage
//   2. AudioLoader::Load(path)               -> décode WAV/MP3/OGG/FLAC/Opus en AudioSample
//   3. engine.Play(sample, params)           -> lance une voix (bus "Music")
//   4. boucle : IsPlaying(handle) + GetPlaybackPosition (progression)
//   5. engine.Shutdown()
//
// ⚠️ NOTE (à l'attention de l'agent NKCode) : le mixage NKAudio convertit le TAUX
// d'échantillonnage du fichier vers celui du device (fix cdba6d5c/adac762e). Un lecteur
// n'a donc RIEN à faire côté rééchantillonnage : Load -> Play suffit. Si l'ancien lecteur
// jouait trop vite/aigu, c'était CE bug (corrigé) — un simple pull suffit.
//
//   Usage : NkAudioPlayer.exe <fichier audio> [--maxsec N]
// =============================================================================
#include "NKAudio/NKAudio.h"
#include "NKTime/NkChrono.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

using namespace nkentseu;
using namespace nkentseu::audio;

int main(int argc, char **argv) {
	printf("=== NkAudioPlayer — lecteur audio de reference (NKAudio) ===\n\n");
	if (argc < 2) {
		printf("  usage : NkAudioPlayer.exe <fichier.wav|mp3|ogg|flac|opus> [--maxsec N]\n");
		return 1;
	}
	const char *path = argv[1];
	float32 maxSec = 0.0f; // 0 = jusqu'à la fin
	for (int32 i = 2; i < argc; ++i)
		if (strcmp(argv[i], "--maxsec") == 0 && i + 1 < argc)
			maxSec = (float32)atof(argv[++i]);

	// 1) Moteur audio : ouvre le device + démarre le mixage.
	AudioEngine &engine = AudioEngine::Instance();
	AudioEngineConfig cfg; // backend AUTO (WASAPI sous Windows), 48000 Hz par défaut
	if (!engine.Initialize(cfg)) {
		printf("  [KO] AudioEngine::Initialize a echoue (device audio indisponible ?)\n");
		return 1;
	}
	printf("  moteur (demande) : %d Hz, %d canaux\n", cfg.sampleRate, cfg.channels);
	printf("  device (REEL)    : %d Hz, %d canaux  <-- taux cible du reechantillonnage\n", engine.GetSampleRate(),
		   engine.GetChannels());

	// 2) Charge et décode le fichier.
	AudioSample sample = AudioLoader::Load(path);
	if (!sample.IsValid()) {
		printf("  [KO] impossible de charger/decoder : %s\n", path);
		engine.Shutdown();
		return 1;
	}
	const double durSec = (sample.sampleRate > 0) ? (double)sample.frameCount / (double)sample.sampleRate : 0.0;
	printf("  fichier : %s\n", path);
	printf("  audio   : %lld frames, %d Hz, %d canaux -> %.2f s\n", (long long)sample.frameCount, sample.sampleRate,
		   sample.channels, durSec);
	printf("  (le moteur convertit %d Hz -> %d Hz automatiquement)\n\n", sample.sampleRate, cfg.sampleRate);

	// 3) Joue (bus Music).
	VoiceParams vp;
	vp.bus = "Music";
	vp.volume = 1.0f;
	const AudioHandle h = engine.Play(sample, vp);
	printf("  >>> lecture en cours...\n");

	// 4) Suivi : barre de progression tant que la voix joue (tick 100 ms).
	double elapsed = 0.0;
	while (engine.IsPlaying(h)) {
		const double pos = (double)engine.GetPlaybackPosition(h);
		const int32 pct = (durSec > 0.0) ? (int32)(pos * 100.0 / durSec) : 0;
		printf("\r  %5.1fs / %5.1fs  [", pos, durSec);
		for (int32 b = 0; b < 30; ++b)
			printf("%c", (b < pct * 30 / 100) ? '#' : '-');
		printf("] %3d%%", pct);
		fflush(stdout);
		NkChrono::Sleep((int64)100);
		elapsed += 0.1;
		if (maxSec > 0.0f && elapsed >= (double)maxSec)
			break;
	}
	printf("\n\n  <<< lecture terminee.\n");

	// 5) Libère.
	engine.Shutdown();
	AudioLoader::Free(sample);
	printf("=== fin ===\n");
	return 0;
}
