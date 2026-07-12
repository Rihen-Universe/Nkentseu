// =============================================================================
// NKSpeechTest — tests headless du module de parole NKSpeech (NKAI).
// AUCUN device GPU : pur CPU. Sortie via printf (sortie directe console).
// =============================================================================
#include "NKSpeech/NkAudioFeatures.h"
#include "NKSpeech/NkGriffinLim.h"
#include "NKSpeech/NkVoiceSynth.h"

#include <cstdio>

using namespace nkentseu;

// Écrit un WAV PCM16 mono (pour ÉCOUTER la synthèse). RIFF little-endian.
static void WriteWavPcm16(const char *path, const float *samples, int n, int sampleRate) {
	FILE *fp = fopen(path, "wb");
	if (!fp)
		return;
	auto u32 = [&](unsigned v) { fputc(v & 255, fp); fputc((v >> 8) & 255, fp); fputc((v >> 16) & 255, fp); fputc((v >> 24) & 255, fp); };
	auto u16 = [&](unsigned v) { fputc(v & 255, fp); fputc((v >> 8) & 255, fp); };
	const int dataBytes = n * 2;
	fwrite("RIFF", 1, 4, fp); u32(36 + dataBytes); fwrite("WAVE", 1, 4, fp);
	fwrite("fmt ", 1, 4, fp); u32(16); u16(1); u16(1);           // PCM, mono
	u32(sampleRate); u32(sampleRate * 2); u16(2); u16(16);        // byte rate, block align, bits
	fwrite("data", 1, 4, fp); u32(dataBytes);
	for (int i = 0; i < n; ++i) {
		float v = samples[i];
		if (v > 1.0f) v = 1.0f;
		if (v < -1.0f) v = -1.0f;
		int s = (int)(v * 32767.0f);
		u16((unsigned)(s & 0xFFFF));
	}
	fclose(fp);
}

int main() {
	printf("=== NKSpeechTest — parole from-scratch (NKAI, headless) ===\n\n");

	int nbOk = 0, nbTotal = 0;

	{
		++nbTotal;
		const bool ok = ai::NkAudioFeatures::SelfTest();
		printf("[ %s ] NkAudioFeatures : MFCC/log-Mel (sinus 1kHz -> bon canal Mel, deterministe, silence fini)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}

	{
		++nbTotal;
		const bool ok = ai::NkGriffinLim::SelfTest();
		printf("[ %s ] NkGriffinLim : vocodeur (spectrogramme magnitude -> onde, phase iterative ; "
			   "magnitude reconstruite fidele + energie preservee)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}

	{
		++nbTotal;
		const bool ok = ai::NkVoiceSynth::SelfTest();
		printf("[ %s ] NkVoiceSynth : synthese par formants (voyelle 'a' -> energie autour de F1/F2)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}

	// Synthese AUDIBLE : ecrit un WAV "a e i o u" (a ecouter).
	{
		NkVector<ai::NkPhone> seq;
		const char voyelles[5] = {'a', 'e', 'i', 'o', 'u'};
		for (int i = 0; i < 5; ++i) {
			seq.PushBack(ai::NkVoiceSynth::Vowel(voyelles[i], 260.0f));
			ai::NkPhone sil;
			sil.gain = 0.0f;
			sil.durationMs = 60.0f;
			seq.PushBack(sil); // petite pause entre voyelles
		}
		ai::NkVoiceSynthConfig cfg;
		NkVector<float32> wav = ai::NkVoiceSynth::Synthesize(seq, cfg);
		if (wav.Size() > 0) {
			WriteWavPcm16("nkvoice_aeiou.wav", wav.Data(), (int)wav.Size(), cfg.sampleRate);
			printf("       -> ecrit nkvoice_aeiou.wav (%d echantillons, %d Hz) : a ecouter !\n",
				   (int)wav.Size(), cfg.sampleRate);
		}
	}

	printf("\n=== Resultat : %d/%d suites OK ===\n", nbOk, nbTotal);
	return (nbOk == nbTotal) ? 0 : 1;
}
