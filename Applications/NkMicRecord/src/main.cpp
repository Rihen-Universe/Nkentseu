// =============================================================================
// NkMicRecord — outil d'enregistrement micro → WAV (test réel de NkAudioCapture).
// -----------------------------------------------------------------------------
// Ouvre le micro par défaut (NKAudio, backend WASAPI sous Windows), enregistre
// `secondes` (défaut 5), écrit un WAV 16-bit PCM. À LANCER à la main pour tester
// la capture end-to-end (le ring buffer est déjà validé headless par NkAudioCapture::SelfTest).
//
//   NkMicRecord.exe [secondes] [sortie.wav]
//   ex : NkMicRecord.exe 5 ma_voix.wav
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKAudio/NkAudioCapture.h"
#include "NKAudio/NkDenoiser.h"
#include "NKAudio/NKAudio.h"
#include "NKFileSystem/NkFile.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKTime/NkChrono.h"
#include "NKTime/NkClock.h"

#include <cstdio>  // printf — sortie directe console
#include <cstdlib> // atoi — parsing d'arguments
#include <cmath>   // log10 — affichage dBFS diagnostic

using namespace nkentseu;

namespace {

	// Ajoute une valeur little-endian au buffer d'octets.
	void PushLE(NkVector<nk_uint8> &b, nk_uint32 v, int32 bytes) {
		for (int32 i = 0; i < bytes; ++i)
			b.PushBack(static_cast<nk_uint8>((v >> (8 * i)) & 0xFF));
	}

	void PushTag(NkVector<nk_uint8> &b, const char *tag) {
		for (int32 i = 0; i < 4; ++i)
			b.PushBack(static_cast<nk_uint8>(tag[i]));
	}

	// Encode des frames Float32 interleaved en WAV PCM 16-bit et écrit le fichier.
	bool WriteWav16(const char *path, const NkVector<float32> &samples, int32 sampleRate, int32 channels) {
		const nk_uint32 frameCount = channels > 0 ? static_cast<nk_uint32>(samples.Size()) / static_cast<nk_uint32>(channels) : 0;
		const nk_uint32 dataBytes = frameCount * static_cast<nk_uint32>(channels) * 2u; // 16-bit
		const nk_uint32 byteRate = static_cast<nk_uint32>(sampleRate) * static_cast<nk_uint32>(channels) * 2u;

		NkVector<nk_uint8> out;
		PushTag(out, "RIFF");
		PushLE(out, 36u + dataBytes, 4);
		PushTag(out, "WAVE");
		PushTag(out, "fmt ");
		PushLE(out, 16u, 4);								   // taille du chunk fmt
		PushLE(out, 1u, 2);									   // format = PCM
		PushLE(out, static_cast<nk_uint32>(channels), 2);
		PushLE(out, static_cast<nk_uint32>(sampleRate), 4);
		PushLE(out, byteRate, 4);
		PushLE(out, static_cast<nk_uint32>(channels) * 2u, 2); // block align
		PushLE(out, 16u, 2);								   // bits/sample
		PushTag(out, "data");
		PushLE(out, dataBytes, 4);

		// Float32 [-1,1] → int16, clampé.
		for (uint64 i = 0; i < samples.Size(); ++i) {
			float32 s = samples[i];
			s = s < -1.0f ? -1.0f : (s > 1.0f ? 1.0f : s);
			const int32 v = static_cast<int32>(s * 32767.0f);
			const nk_uint16 u = static_cast<nk_uint16>(static_cast<int16>(v));
			out.PushBack(static_cast<nk_uint8>(u & 0xFF));
			out.PushBack(static_cast<nk_uint8>((u >> 8) & 0xFF));
		}

		return NkFile::WriteAllBytes(path, out);
	}

} // namespace

int main(int argc, char **argv) {
	// Mode décodage : NkMicRecord --decode <in.(wav|mp3|ogg|flac|opus)> <out.wav>
	// Charge via AudioLoader (auto-détection du format, dont Ogg-Opus) et
	// réécrit en WAV 16-bit — test end-to-end des codecs NKAudio.
	if (argc >= 4 && NkString(argv[1]) == NkString("--decode")) {
		audio::AudioSample smp = audio::AudioLoader::Load(argv[2]);
		if (!smp.IsValid()) {
			printf("[DECODE] ERREUR : chargement impossible : %s\n", argv[2]);
			return 1;
		}
		const bool ok = audio::AudioLoader::SaveWAV(argv[3], smp);
		printf("[DECODE] %s -> %llu frames @ %d Hz (%d canal/aux) -> %s (%s)\n", argv[2],
			   static_cast<unsigned long long>(smp.frameCount), smp.sampleRate, smp.channels, argv[3],
			   ok ? "OK" : "ECHEC ecriture");
		audio::AudioLoader::Free(smp);
		return ok ? 0 : 1;
	}

	int32 seconds = 5;
	const char *outPath = "nkmic_capture.wav";
	bool denoise = true; // débruitage + normalisation par défaut (corrige bruit + volume bas)

	// Parsing simple : [secondes] [sortie.wav] [--raw]
	int posArg = 0;
	for (int a = 1; a < argc; ++a) {
		if (argv[a][0] == '-') {
			if (argv[a][1] == '-' && argv[a][2] == 'r') // --raw
				denoise = false;
		} else if (posArg == 0) {
			const int v = atoi(argv[a]);
			if (v > 0)
				seconds = v;
			++posArg;
		} else if (posArg == 1) {
			outPath = argv[a];
			++posArg;
		}
	}

	printf("=== NkMicRecord — enregistrement micro %ds -> %s (%s) ===\n", seconds, outPath,
		   denoise ? "debruitage+normalisation ON" : "brut (--raw)");

	// Périphériques disponibles.
	NkVector<audio::NkCaptureDeviceInfo> devs = audio::NkAudioCapture::EnumerateDevices();
	printf("Peripheriques d'entree detectes : %d\n", static_cast<int32>(devs.Size()));
	for (uint64 i = 0; i < devs.Size(); ++i)
		printf("  [%d] %s%s\n", static_cast<int32>(i), devs[i].name.CStr(), devs[i].isDefault ? " (defaut)" : "");

	audio::NkCaptureConfig cfg;
	cfg.sampleRate = 48000;
	cfg.channels = 1; // mono voix
	cfg.ringSeconds = seconds + 2;

	audio::NkAudioCapture cap;
	if (!cap.Open(cfg)) {
		printf("[ERREUR] Open() a echoue — backend indisponible ou pas de micro. Backend : %s\n", cap.BackendName());
		return 1;
	}
	printf("Backend capture : %s | %d Hz | %d canal(aux)\n", cap.BackendName(), cap.SampleRate(), cap.Channels());

	if (!cap.Start()) {
		printf("[ERREUR] Start() de la capture a echoue.\n");
		cap.Close();
		return 1;
	}

	printf(">>> PARLEZ MAINTENANT (%ds)...\n", seconds);
	fflush(stdout);

	NkVector<float32> recorded;
	const int32 sr = cap.SampleRate();
	const int32 ch = cap.Channels();
	const int32 totalFrames = sr * seconds;

	NkVector<float32> tmp;
	tmp.Resize(static_cast<uint64>(4096 * ch));

	int32 gotFrames = 0;
	NkClock clock;
	while (gotFrames < totalFrames) {
		const int32 n = cap.Read(tmp.Data(), 4096);
		if (n > 0) {
			for (int32 i = 0; i < n * ch; ++i)
				recorded.PushBack(tmp[static_cast<uint64>(i)]);
			gotFrames += n;
		} else {
			NkChrono::Sleep(static_cast<int64>(3)); // laisse le ring se remplir (3 ms)
		}
		// Garde-fou anti-blocage : au pire 2× la durée cible.
		if (clock.Tick().total > static_cast<float32>(seconds) * 2.0f + 1.0f)
			break;
	}

	cap.Stop();
	cap.Close();

	printf("Frames capturees : %d (%d echantillons)\n", gotFrames, static_cast<int32>(recorded.Size()));

	if (recorded.Size() == 0) {
		printf("[ERREUR] Aucun echantillon capture — micro muet ou permission refusee.\n");
		return 1;
	}

	// Mesure de crête brute (diagnostic « volume trop bas »).
	float32 rawPeak = 0.f;
	for (uint64 i = 0; i < recorded.Size(); ++i) {
		const float32 av = recorded[i] < 0.f ? -recorded[i] : recorded[i];
		if (av > rawPeak)
			rawPeak = av;
	}
	printf("Crete brute : %.4f (%.1f dBFS)\n", rawPeak, rawPeak > 1e-6f ? 20.f * (float32)log10(rawPeak) : -120.f);

	// Débruitage + normalisation (soustraction spectrale + gate + auto-gain).
	NkVector<float32> *toWrite = &recorded;
	NkVector<float32> clean;
	if (denoise) {
		printf("Debruitage (soustraction spectrale + gate) + normalisation...\n");
		audio::NkDenoiseOptions opt; // valeurs par défaut : voix
		if (audio::NkDenoiser::Process(recorded.Data(), gotFrames, ch, sr, clean, opt)) {
			toWrite = &clean;
			float32 cp = 0.f;
			for (uint64 i = 0; i < clean.Size(); ++i) {
				const float32 av = clean[i] < 0.f ? -clean[i] : clean[i];
				if (av > cp)
					cp = av;
			}
			printf("Crete apres traitement : %.4f (%.1f dBFS)\n", cp, cp > 1e-6f ? 20.f * (float32)log10(cp) : -120.f);
		} else {
			printf("[WARN] Debruitage echoue — ecriture du brut.\n");
		}
	}

	if (!WriteWav16(outPath, *toWrite, sr, ch)) {
		printf("[ERREUR] Ecriture du WAV a echoue : %s\n", outPath);
		return 1;
	}

	printf("=== OK — ecrit : %s (%d frames, %d Hz, %d canal) ===\n", outPath, gotFrames, sr, ch);
	return 0;
}
