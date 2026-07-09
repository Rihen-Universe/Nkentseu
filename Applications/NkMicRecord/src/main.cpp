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
#include "NKFileSystem/NkFile.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKLogger/NkLog.h"
#include "NKTime/NkChrono.h"
#include "NKTime/NkClock.h"

#include <cstdlib> // atoi/atof — parsing d'arguments seulement

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
	const int32 seconds = (argc > 1) ? (atoi(argv[1]) > 0 ? atoi(argv[1]) : 5) : 5;
	const char *outPath = (argc > 2) ? argv[2] : "nkmic_capture.wav";

	logger.Info("=== NkMicRecord — enregistrement micro {0}s -> {1} ===", seconds, outPath);

	// Périphériques disponibles.
	NkVector<audio::NkCaptureDeviceInfo> devs = audio::NkAudioCapture::EnumerateDevices();
	logger.Info("Peripheriques d'entree detectes : {0}", static_cast<int32>(devs.Size()));
	for (uint64 i = 0; i < devs.Size(); ++i)
		logger.Info("  [{0}] {1}{2}", static_cast<int32>(i), devs[i].name.CStr(),
					devs[i].isDefault ? " (defaut)" : "");

	audio::NkCaptureConfig cfg;
	cfg.sampleRate = 48000;
	cfg.channels = 1; // mono voix
	cfg.ringSeconds = seconds + 2;

	audio::NkAudioCapture cap;
	if (!cap.Open(cfg)) {
		logger.Error("Echec Open() — backend indisponible ou pas de micro. Backend : {0}", cap.BackendName());
		return 1;
	}
	logger.Info("Backend capture : {0} | {1} Hz | {2} canal(aux)", cap.BackendName(), cap.SampleRate(),
				cap.Channels());

	if (!cap.Start()) {
		logger.Error("Echec Start() de la capture.");
		cap.Close();
		return 1;
	}

	logger.Info(">>> PARLEZ MAINTENANT ({0}s)...", seconds);

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

	logger.Info("Frames capturees : {0} ({1} echantillons)", gotFrames, static_cast<int32>(recorded.Size()));

	if (recorded.Size() == 0) {
		logger.Error("Aucun echantillon capture — micro muet ou permission refusee.");
		return 1;
	}

	if (!WriteWav16(outPath, recorded, sr, ch)) {
		logger.Error("Echec d'ecriture du WAV : {0}", outPath);
		return 1;
	}

	logger.Info("=== OK — ecrit : {0} ({1} frames, {2} Hz, {3} canal) ===", outPath, gotFrames, sr, ch);
	return 0;
}
