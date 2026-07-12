// =============================================================================
// NKTTSTrain — chargeur LJSpeech + (à venir) entraînement TTS acoustique.
//   Étape 1 (ce fichier) : PROUVER le pipeline de données.
//     metadata.csv (texte↔WAV) → lecteur WAV PCM16 → mel-spectrogramme (NkAudioFeatures).
//   Usage : NKTTSTrain.exe [<dossier LJSpeech-1.1>]   (défaut = VoiceDatasets/…)
// =============================================================================
#include "NKSpeech/NkAudioFeatures.h"
#include "NKSpeech/NkGriffinLim.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

// Lecteur WAV MINIMAL : RIFF/WAVE PCM16 → float mono [-1,1] + sampleRate.
static bool ReadWavMono(const char *path, NkVector<float32> &out, int32 &sampleRate) {
	FILE *f = fopen(path, "rb");
	if (!f)
		return false;
	auto u32 = [&]() { unsigned char b[4]; if (fread(b, 1, 4, f) != 4) return 0u; return (unsigned)b[0] | (b[1] << 8) | (b[2] << 16) | ((unsigned)b[3] << 24); };
	auto u16 = [&]() { unsigned char b[2]; if (fread(b, 1, 2, f) != 2) return 0u; return (unsigned)b[0] | (b[1] << 8); };
	char riff[4];
	if (fread(riff, 1, 4, f) != 4 || memcmp(riff, "RIFF", 4) != 0) { fclose(f); return false; }
	u32(); // taille
	char wave[4];
	if (fread(wave, 1, 4, f) != 4 || memcmp(wave, "WAVE", 4) != 0) { fclose(f); return false; }

	int32 channels = 1, bits = 16;
	sampleRate = 22050;
	bool haveFmt = false;
	// Parcourt les chunks jusqu'à "data".
	for (;;) {
		char id[4];
		if (fread(id, 1, 4, f) != 4) { fclose(f); return false; }
		unsigned sz = u32();
		if (memcmp(id, "fmt ", 4) == 0) {
			u16();							 // audioFormat (1 = PCM)
			channels = (int32)u16();
			sampleRate = (int32)u32();
			u32();							 // byteRate
			u16();							 // blockAlign
			bits = (int32)u16();
			haveFmt = true;
			if (sz > 16)
				fseek(f, (long)(sz - 16), SEEK_CUR);
		} else if (memcmp(id, "data", 4) == 0) {
			if (!haveFmt || bits != 16) { fclose(f); return false; } // on ne gère que PCM16
			const unsigned nSamplesTotal = sz / 2;			// int16
			const unsigned frames = nSamplesTotal / (unsigned)(channels > 0 ? channels : 1);
			out.Resize((uint64)frames);
			for (unsigned i = 0; i < frames; ++i) {
				int32 acc = 0;
				for (int32 c = 0; c < channels; ++c) {
					unsigned char b[2];
					if (fread(b, 1, 2, f) != 2) { out.Resize((uint64)i); fclose(f); return i > 0; }
					acc += (int16)(b[0] | (b[1] << 8));
				}
				out[(uint64)i] = (float32)acc / (float32)(channels * 32768);
			}
			fclose(f);
			return true;
		} else {
			fseek(f, (long)sz, SEEK_CUR); // chunk inconnu → saute
		}
	}
}

// Écrivain WAV MINIMAL : float mono [-1,1] → RIFF/WAVE PCM16.
static bool WriteWavMono(const char *path, const float32 *mono, int32 samples, int32 sampleRate) {
	FILE *f = fopen(path, "wb");
	if (!f)
		return false;
	auto w32 = [&](unsigned v) { unsigned char b[4] = { (unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24) }; fwrite(b, 1, 4, f); };
	auto w16 = [&](unsigned v) { unsigned char b[2] = { (unsigned char)v, (unsigned char)(v >> 8) }; fwrite(b, 1, 2, f); };
	const unsigned dataBytes = (unsigned)samples * 2u;
	fwrite("RIFF", 1, 4, f);
	w32(36u + dataBytes);
	fwrite("WAVE", 1, 4, f);
	fwrite("fmt ", 1, 4, f);
	w32(16u);						 // taille sous-chunk fmt
	w16(1u);						 // PCM
	w16(1u);						 // mono
	w32((unsigned)sampleRate);		 // sampleRate
	w32((unsigned)sampleRate * 2u);	 // byteRate (mono 16 bit)
	w16(2u);						 // blockAlign
	w16(16u);						 // bitsPerSample
	fwrite("data", 1, 4, f);
	w32(dataBytes);
	for (int32 i = 0; i < samples; ++i) {
		float32 s = mono[i];
		if (s > 1.0f) s = 1.0f;
		if (s < -1.0f) s = -1.0f;
		int32 v = (int32)(s * 32767.0f);
		w16((unsigned)(int16)v);
	}
	fclose(f);
	return true;
}

int main(int argc, char **argv) {
	printf("=== NKTTSTrain — chargeur LJSpeech (etape 1 : pipeline de donnees) ===\n\n");

	const char *dir = (argc >= 2) ? argv[1] : "D:/Projets/2026/Nkentseu/VoiceDatasets/LJSpeech/LJSpeech-1.1";
	char metaPath[1024];
	snprintf(metaPath, sizeof(metaPath), "%s/metadata.csv", dir);
	FILE *meta = fopen(metaPath, "r");
	if (!meta) {
		printf("[ERREUR] metadata.csv introuvable : %s\n", metaPath);
		printf("         (le dataset est-il extrait ? passe le dossier LJSpeech-1.1 en argument)\n");
		return 1;
	}

	NkAudioFeatureConfig cfg;
	cfg.sampleRate = 22050; // LJSpeech
	cfg.melBands = 80;		// standard TTS
	cfg.useDeltas = false;

	int32 shown = 0, ok = 0;
	int64 totalFrames = 0;
	NkVector<float32> firstAudio; // premier clip retenu pour le round-trip vocodeur
	int32 firstSr = 22050;
	char firstId[64] = { 0 };
	char line[8192];
	while (fgets(line, sizeof(line), meta) && shown < 5) {
		// Format : "LJ001-0001|Transcription brute|Transcription normalisee"
		char *p1 = strchr(line, '|');
		if (!p1)
			continue;
		*p1 = '\0';
		const char *id = line;
		char *p2 = strchr(p1 + 1, '|');
		const char *text = (p2 ? p2 + 1 : p1 + 1);
		// retire le \n final du texte
		char textBuf[4096];
		snprintf(textBuf, sizeof(textBuf), "%s", text);
		for (char *c = textBuf; *c; ++c)
			if (*c == '\n' || *c == '\r') { *c = '\0'; break; }

		char wavPath[1200];
		snprintf(wavPath, sizeof(wavPath), "%s/wavs/%s.wav", dir, id);
		NkVector<float32> audio;
		int32 sr = 0;
		++shown;
		if (!ReadWavMono(wavPath, audio, sr)) {
			printf("  [%s] WAV illisible (%s)\n", id, wavPath);
			continue;
		}
		NkFeatureMatrix mel = NkAudioFeatures::LogMelSpectrogram(audio.Data(), (int32)audio.Size(), cfg);
		++ok;
		totalFrames += mel.frames;
		if (firstAudio.Size() == 0) {
			firstAudio = audio;
			firstSr = sr;
			snprintf(firstId, sizeof(firstId), "%s", id);
		}
		printf("  [%s] %.2fs @ %d Hz -> mel %d trames x %d bandes | texte(%d) : \"%.60s%s\"\n",
			   id, (double)audio.Size() / (double)sr, sr, mel.frames, mel.dims, (int)strlen(textBuf), textBuf,
			   strlen(textBuf) > 60 ? "..." : "");
	}
	fclose(meta);

	printf("\n  [ %s ] %d/%d echantillons charges (WAV -> mel-spectrogramme)\n", (ok == shown && ok > 0) ? "OK " : "KO",
		   ok, shown);

	// -------------------------------------------------------------------------
	// ETAPE 2 — VOCODEUR sur VRAIE VOIX : clip reel -> STFT magnitude -> Griffin-Lim
	//   -> onde re-synthetisee. Valide la moitie « vocodeur » du TTS sur voix humaine
	//   (aucun modele appris ici : c'est la reconstruction de phase pure). Ecrit 2 WAV
	//   (original + reconstruit) pour une ecoute A/B.
	// -------------------------------------------------------------------------
	if (firstAudio.Size() > 0) {
		printf("\n=== Etape 2 — vocodeur Griffin-Lim sur vraie voix [%s] ===\n", firstId);
		NkGriffinLimConfig gl;
		gl.fftSize = 1024;
		gl.hopSize = 256;
		gl.iterations = 150;

		NkMagSpectrogram mag = NkGriffinLim::Magnitude(firstAudio.Data(), (int32)firstAudio.Size(), gl);
		printf("  STFT magnitude : %d trames x %d bins (fft=%d, hop=%d)\n", mag.frames, mag.bins, gl.fftSize, gl.hopSize);

		NkVector<float32> recon = NkGriffinLim::Reconstruct(mag, gl);

		// JUSTESSE DU CONTENU = erreur sur la MAGNITUDE reconstruite (le buzz de phase
		// Griffin-Lim gonfle l'energie mais NE change PAS le contenu spectral = la voix).
		// On recalcule |STFT| de la reconstruction et on compare a la cible (trames internes).
		{
			NkMagSpectrogram magR = NkGriffinLim::Magnitude(recon.Data(), (int32)recon.Size(), gl);
			int32 mf = (magR.frames < mag.frames) ? magR.frames : mag.frames;
			double num = 0.0, den2 = 0.0;
			for (int32 f = 2; f < mf - 2; ++f) // saute les bords peu recouverts
				for (int32 k = 0; k < mag.bins; ++k) {
					double a = mag.data[(nk_size)f * mag.bins + k];
					double b = magR.data[(nk_size)f * magR.bins + k];
					num += (a - b) * (a - b);
					den2 += a * a;
				}
			double magErr = (den2 > 0.0) ? sqrt(num / den2) : 1.0;
			printf("  erreur magnitude reconstruite = %.2f%% (contenu vocal ; <~15%% = voix correcte)\n", magErr * 100.0);
		}

		// Energie relative (original vs reconstruit) sur la longueur commune.
		int32 n = (int32)((recon.Size() < firstAudio.Size()) ? recon.Size() : firstAudio.Size());
		double eOrig = 0.0, eRec = 0.0;
		for (int32 i = 0; i < n; ++i) {
			eOrig += (double)firstAudio[(uint64)i] * firstAudio[(uint64)i];
			eRec += (double)recon[(uint64)i] * recon[(uint64)i];
		}
		double ratio = (eOrig > 0.0) ? (eRec / eOrig) : 0.0;
		printf("  reconstruit : %d echantillons (%.2fs), energie brute ratio rec/orig = %.4f\n",
			   (int)recon.Size(), (double)recon.Size() / (double)firstSr, ratio);

		// Rendu ECOUTABLE : normalise en RMS a une cible confortable, puis SOFT-CLIP tanh
		// (ecrase les pics transitoires du buzz Griffin-Lim SANS ecraser le corps de la voix).
		double rms = 0.0;
		for (uint64 i = 0; i < recon.Size(); ++i)
			rms += (double)recon[i] * recon[i];
		rms = (recon.Size() > 0) ? sqrt(rms / (double)recon.Size()) : 0.0;
		const double targetRms = 0.20;
		double gain = (rms > 1e-9) ? (targetRms / rms) : 1.0;
		for (uint64 i = 0; i < recon.Size(); ++i) {
			double v = (double)recon[i] * gain;
			recon[i] = (float32)tanh(v); // corps ~lineaire (tanh(0.2)~0.197), pics -> ~±1
		}
		printf("  rendu ecoutable : RMS %.4f -> cible %.2f (gain %.2f) + soft-clip tanh\n", rms, targetRms, gain);

		bool w1 = WriteWavMono("nktts_ljspeech_orig.wav", firstAudio.Data(), (int32)firstAudio.Size(), firstSr);
		bool w2 = WriteWavMono("nktts_ljspeech_recon.wav", recon.Data(), (int32)recon.Size(), firstSr);
		printf("  ecrit : nktts_ljspeech_orig.wav (%s) + nktts_ljspeech_recon.wav (%s)\n",
			   w1 ? "OK" : "KO", w2 ? "OK" : "KO");
		printf("  -> ECOUTE A/B : la reconstruction doit sonner comme la vraie locutrice (voix, prosodie).\n");
	}

	printf("\n=== Pipeline de donnees LJSpeech %s ===\n", (ok > 0) ? "OPERATIONNEL" : "EN ECHEC");
	return (ok > 0) ? 0 : 1;
}
