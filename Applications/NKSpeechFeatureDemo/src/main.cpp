// =============================================================================
// NKSpeechFeatureDemo — MFCC sur un VRAI fichier audio (validation bout-en-bout).
// -----------------------------------------------------------------------------
// Décode un fichier (mp3/wav/ogg/flac via NKAudio) → mono → NkAudioFeatures::MFCC.
// Prouve la chaîne « parole réelle → features » sur le corpus (Bassa/Bulu/ghomala').
//
//   NKSpeechFeatureDemo.exe <fichier_audio>
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKSpeech/NkAudioFeatures.h"
#include "NKAudio/NkAudio.h"
#include "NKContainers/Sequential/NkVector.h"

#include <cstdio>

using namespace nkentseu;

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage : NKSpeechFeatureDemo <fichier_audio>\n");
		return 2;
	}
	const char *path = argv[1];
	printf("=== NKSpeechFeatureDemo — MFCC sur fichier reel ===\n");
	printf("Fichier : %s\n", path);

	audio::AudioSample s = audio::AudioLoader::Load(path);
	if (!s.IsValid()) {
		printf("[ERREUR] Chargement/decodage echoue.\n");
		return 1;
	}
	printf("Decode : %d Hz | %d canal(aux) | %d frames | %.2f s\n", s.sampleRate, s.channels,
		   (int32)s.frameCount, s.GetDuration());

	// Downmix mono.
	const int32 ch = s.channels > 0 ? s.channels : 1;
	const int32 n = (int32)s.frameCount;
	NkVector<float32> mono;
	mono.Resize((uint64)n);
	for (int32 i = 0; i < n; ++i) {
		float32 acc = 0.0f;
		for (int32 c = 0; c < ch; ++c)
			acc += s.data[(uint64)i * (uint64)ch + (uint64)c];
		mono[(uint64)i] = acc / (float32)ch;
	}

	// MFCC au taux natif du fichier (le banc Mel s'adapte au sampleRate).
	ai::NkAudioFeatureConfig cfg;
	cfg.sampleRate = s.sampleRate;
	cfg.mfccCount = 13;
	cfg.melBands = 40;
	cfg.useDeltas = true;

	ai::NkFeatureMatrix mel = ai::NkAudioFeatures::LogMelSpectrogram(mono.Data(), n, cfg);
	ai::NkFeatureMatrix mfcc = ai::NkAudioFeatures::MFCC(mono.Data(), n, cfg);

	printf("log-Mel : %d frames x %d bandes\n", mel.frames, mel.dims);
	printf("MFCC    : %d frames x %d dims (13 base + delta + delta-delta)\n", mfcc.frames, mfcc.dims);

	// Petites stats de contrôle (moyenne/min/max des coefficients de base c1..c12 sur la 1re moitié).
	if (mfcc.frames > 0 && mfcc.dims >= 13) {
		double mean = 0.0;
		float32 mn = 1e30f, mx = -1e30f;
		int32 cnt = 0;
		for (int32 f = 0; f < mfcc.frames; ++f) {
			for (int32 k = 1; k < 13; ++k) {
				const float32 v = mfcc.data[(uint64)f * (uint64)mfcc.dims + (uint64)k];
				mean += v;
				if (v < mn)
					mn = v;
				if (v > mx)
					mx = v;
				++cnt;
			}
		}
		printf("MFCC c1..c12 : moyenne=%.3f min=%.3f max=%.3f (%d coeffs)\n", cnt ? mean / cnt : 0.0, mn, mx, cnt);
		// 1re frame (c0..c4) pour visualiser.
		printf("frame[0] c0..c4 :");
		for (int32 k = 0; k < 5 && k < mfcc.dims; ++k)
			printf(" %.2f", mfcc.data[(uint64)k]);
		printf("\n");
	}

	audio::AudioLoader::Free(s);
	printf("=== OK — features extraites de parole reelle ===\n");
	return 0;
}
