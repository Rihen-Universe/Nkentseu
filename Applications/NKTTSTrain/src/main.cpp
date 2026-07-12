// =============================================================================
// NKTTSTrain — chargeur LJSpeech + (à venir) entraînement TTS acoustique.
//   Étape 1 (ce fichier) : PROUVER le pipeline de données.
//     metadata.csv (texte↔WAV) → lecteur WAV PCM16 → mel-spectrogramme (NkAudioFeatures).
//   Usage : NKTTSTrain.exe [<dossier LJSpeech-1.1>]   (défaut = VoiceDatasets/…)
// =============================================================================
#include "NKSpeech/NkAudioFeatures.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

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
		printf("  [%s] %.2fs @ %d Hz -> mel %d trames x %d bandes | texte(%d) : \"%.60s%s\"\n",
			   id, (double)audio.Size() / (double)sr, sr, mel.frames, mel.dims, (int)strlen(textBuf), textBuf,
			   strlen(textBuf) > 60 ? "..." : "");
	}
	fclose(meta);

	printf("\n  [ %s ] %d/%d echantillons charges (WAV -> mel-spectrogramme)\n", (ok == shown && ok > 0) ? "OK " : "KO",
		   ok, shown);
	printf("=== Pipeline de donnees LJSpeech %s ===\n", (ok > 0) ? "OPERATIONNEL" : "EN ECHEC");
	return (ok > 0) ? 0 : 1;
}
