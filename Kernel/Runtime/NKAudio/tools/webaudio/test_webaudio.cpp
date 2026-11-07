// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_webaudio.cpp — LE TEMOIN DU DORSAL AUDIO WEB (tools/banc_webaudio.sh)
// Une page Emscripten qui fait exactement ce que fait le dorsal : NkWebAudio_JSInit au demarrage (donc SANS geste),
// avec le MEME JavaScript (src/NKAudio/NkWebAudioJs.h). Elle joue un la a 440 Hz : le navigateur du banc est
// lance avec --mute-audio, aucun son ne sort.
// =============================================================================
#include "../../src/NKAudio/NkWebAudioJs.h"
#include <math.h>

static double sPhase = 0.0;

// Le meme point d'entree que NkAudioBackends.cpp (stereo entrelace).
extern "C" EMSCRIPTEN_KEEPALIVE void NkWebAudioFillBuffer(int frames, float *out) {
	for (int i = 0; i < frames; ++i) {
		const float v = 0.2f * (float)sin(sPhase);
		sPhase += 2.0 * 3.14159265358979 * 440.0 / 48000.0;
		out[i * 2] = v;
		out[i * 2 + 1] = v;
	}
}

int main() {
	const int sr = NkWebAudio_JSInit(48000, 2, 2048, NK_WEBAUDIO_DEBLOCAGE);
	return sr > 0 ? 0 : 1; // le runtime reste vivant (NO_EXIT_RUNTIME)
}
