// =============================================================================
// NkAudioPlayer — lecteur audio de RÉFÉRENCE avec VISUALISATION (NKCanvas + NKAudio).
// -----------------------------------------------------------------------------
// Le patron CORRECT pour lire un fichier audio vers les haut-parleurs, ET l'afficher :
//   1. AudioEngine::Instance().Initialize()  -> ouvre le device + thread de mixage
//   2. AudioLoader::Load(path)               -> décode WAV/MP3/OGG/FLAC/Opus en AudioSample
//   3. engine.Play(sample, params)           -> lance une voix (bus "Music")
//   4. boucle : GetPlaybackPosition -> tête de lecture sur la FORME D'ONDE dessinée
//   5. engine.Shutdown()
//
// La forme d'onde (enveloppe min/max par colonne de pixels) est pré-calculée UNE fois,
// puis dessinée chaque frame via NKCanvas (portable OpenGL/Vulkan/DX/Software). La partie
// déjà jouée est colorée différemment ; une barre verticale marque la position courante.
//
// ⚠️ NOTE (agent NKCode) : le mixage NKAudio convertit le TAUX du fichier vers celui du
// device (fix mixBuffer/format-device). Un lecteur n'a RIEN à faire côté rééchantillonnage.
//
//   CONTRÔLES : Espace = pause/lecture · Échap = quitter
//   Usage : NkAudioPlayer <fichier audio>
// =============================================================================
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NKMath.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Core/NkRenderer2DTypes.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"

#include "NKAudio/NkAudio.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

NKENTSEU_DEFINE_APP_DATA(([]() {
	NkAppData d{};
	d.appName = "NkAudioPlayer";
	d.appVersion = "1.0.0";
	return d;
})());

// Une colonne de forme d'onde : min et max de l'amplitude (mono) sur l'intervalle.
struct WavePeak {
		float32 lo = 0.0f;
		float32 hi = 0.0f;
};

// Pré-calcule `cols` colonnes d'enveloppe min/max sur tout l'échantillon (canaux moyennés).
static void ComputePeaks(const audio::AudioSample &s, int32 cols, NkVector<WavePeak> &out) {
	out.Clear();
	if (!s.data || s.frameCount <= 0 || cols <= 0)
		return;
	out.Resize((uint64)cols);
	const int32 ch = s.channels > 0 ? s.channels : 1;
	for (int32 c = 0; c < cols; ++c) {
		const uint64 f0 = (uint64)((double)c / cols * (double)s.frameCount);
		uint64 f1 = (uint64)((double)(c + 1) / cols * (double)s.frameCount);
		if (f1 <= f0)
			f1 = f0 + 1;
		if (f1 > (uint64)s.frameCount)
			f1 = (uint64)s.frameCount;
		float32 lo = 1.0f, hi = -1.0f;
		for (uint64 f = f0; f < f1; ++f) {
			float32 m = 0.0f;
			for (int32 k = 0; k < ch; ++k)
				m += s.data[f * (uint64)ch + (uint64)k];
			m /= (float32)ch;
			if (m < lo)
				lo = m;
			if (m > hi)
				hi = m;
		}
		if (hi < lo) {
			lo = 0.0f;
			hi = 0.0f;
		}
		out[(uint64)c].lo = lo;
		out[(uint64)c].hi = hi;
	}
}

// Ajoute un quad plein (2 triangles) au tampon de vertices, couleur unie.
static void PushQuad(NkVector<NkVertex> &v, float32 x0, float32 y0, float32 x1, float32 y1, const NkColor2D &col) {
	NkVertex a{x0, y0, 0, 0, col.r, col.g, col.b, col.a};
	NkVertex b{x1, y0, 0, 0, col.r, col.g, col.b, col.a};
	NkVertex c{x1, y1, 0, 0, col.r, col.g, col.b, col.a};
	NkVertex d{x0, y1, 0, 0, col.r, col.g, col.b, col.a};
	v.PushBack(a);
	v.PushBack(b);
	v.PushBack(c);
	v.PushBack(a);
	v.PushBack(c);
	v.PushBack(d);
}

int nkmain(const NkEntryState &state) {
	NkString path;
	for (uint64 i = 1; i < state.args.Size(); ++i)
		if (path.Empty())
			path = state.args[i];
	if (path.Empty()) {
		logger.Error("[NkAudioPlayer] usage : NkAudioPlayer <fichier.wav|mp3|ogg|flac|opus>");
		return -1;
	}

	// ── 1) Moteur audio ───────────────────────────────────────────────────────
	audio::AudioEngine &engine = audio::AudioEngine::Instance();
	audio::AudioEngineConfig cfg;
	if (!engine.Initialize(cfg)) {
		logger.Error("[NkAudioPlayer] AudioEngine::Initialize a echoue (device indisponible ?)");
		return -2;
	}
	logger.Info("[NkAudioPlayer] device reel : {0} Hz, {1} canaux", engine.GetSampleRate(), engine.GetChannels());

	// ── 2) Charge/decode le fichier ───────────────────────────────────────────
	audio::AudioSample sample = audio::AudioLoader::Load(path.CStr());
	if (!sample.IsValid()) {
		logger.Error("[NkAudioPlayer] impossible de charger/decoder : {0}", path.CStr());
		engine.Shutdown();
		return -3;
	}
	const double durSec = (sample.sampleRate > 0) ? (double)sample.frameCount / (double)sample.sampleRate : 0.0;
	logger.Info("[NkAudioPlayer] {0} : {1} frames, {2} Hz, {3} canaux -> {4}s", path.CStr(),
				(int)sample.frameCount, sample.sampleRate, sample.channels, (float32)durSec);

	// ── 3) Fenetre + cible de rendu 2D ────────────────────────────────────────
	NkWindowConfig wcfg;
	wcfg.title = NkString::Format("NkAudioPlayer - %s", path.CStr());
	wcfg.width = 1000;
	wcfg.height = 340;
	wcfg.centered = true;
	wcfg.resizable = true;
	NkWindow window;
	if (!window.Create(wcfg)) {
		logger.Error("[NkAudioPlayer] echec creation fenetre");
		engine.Shutdown();
		audio::AudioLoader::Free(sample);
		return -4;
	}
	NkContextDesc desc;
	desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;
	NkRenderWindow target(window, desc);
	if (!target.IsValid()) {
		logger.Error("[NkAudioPlayer] echec NkRenderWindow");
		window.Close();
		engine.Shutdown();
		audio::AudioLoader::Free(sample);
		return -5;
	}

	// ── 4) Joue + prepare la forme d'onde ─────────────────────────────────────
	audio::VoiceParams vp;
	vp.bus = "Music";
	vp.volume = 1.0f;
	audio::AudioHandle handle = engine.Play(sample, vp);

	NkVector<WavePeak> peaks;
	int32 peakCols = 0; // recalcule si la largeur change
	NkVector<NkVertex> verts;

	// Palette (pétrole / orange Rihen).
	const NkColor2D kBg{16, 22, 26, 255};
	const NkColor2D kWaveUnplayed{10, 85, 95, 255};	 // #0A555F
	const NkColor2D kWavePlayed{247, 154, 40, 255};	 // #F79A28
	const NkColor2D kAxis{40, 52, 58, 255};
	const NkColor2D kHead{235, 240, 242, 255};

	bool running = true;
	bool paused = false;
	auto &events = NkEvents();
	events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });

	while (running && window.IsOpen()) {
		while (NkEvent *ev = events.PollEvent()) {
			if (auto *k = ev->As<NkKeyPressEvent>()) {
				if (k->GetKey() == NkKey::NK_ESCAPE)
					running = false;
				else if (k->GetKey() == NkKey::NK_SPACE) {
					paused = !paused;
					if (paused)
						engine.Pause(handle);
					else
						engine.Resume(handle);
				}
			}
		}

		const math::NkVec2u sz = target.GetSize();
		static uint32 lastW = 0, lastH = 0;
		if (sz.x != lastW || sz.y != lastH) {
			if (lastW != 0 && sz.x > 0 && sz.y > 0)
				target.OnResize(sz.x, sz.y);
			lastW = sz.x;
			lastH = sz.y;
		}
		const float32 W = (float32)sz.x, H = (float32)sz.y;
		const int32 cols = (int32)W;

		// (Re)calcule les pics si la largeur a change.
		if (cols != peakCols && cols > 0) {
			ComputePeaks(sample, cols, peaks);
			peakCols = cols;
		}

		// Position de lecture -> fraction [0..1].
		const float32 pos = engine.GetPlaybackPosition(handle);
		const float32 frac = (durSec > 0.0) ? math::NkClamp((float32)(pos / durSec), 0.0f, 1.0f) : 0.0f;
		const int32 headCol = (int32)(frac * (float32)cols);

		// ── Construit la geometrie de la forme d'onde ─────────────────────────
		const float32 midY = H * 0.5f;
		const float32 amp = H * 0.42f; // demi-hauteur max
		verts.Clear();
		// Ligne d'axe centrale.
		PushQuad(verts, 0.0f, midY - 1.0f, W, midY + 1.0f, kAxis);
		// Barres d'enveloppe (une colonne = un quad vertical lo..hi).
		for (int32 c = 0; c < (int32)peaks.Size() && c < cols; ++c) {
			const float32 x0 = (float32)c;
			const float32 x1 = x0 + 1.0f;
			float32 yhi = midY - peaks[(uint64)c].hi * amp;
			float32 ylo = midY - peaks[(uint64)c].lo * amp;
			if (ylo - yhi < 1.0f) { // garantit >= 1 px meme sur le silence
				yhi = midY - 0.5f;
				ylo = midY + 0.5f;
			}
			PushQuad(verts, x0, yhi, x1, ylo, (c <= headCol) ? kWavePlayed : kWaveUnplayed);
		}
		// Tete de lecture.
		const float32 hx = frac * W;
		PushQuad(verts, hx - 1.0f, 0.0f, hx + 1.0f, H, kHead);

		// ── Rendu ─────────────────────────────────────────────────────────────
		target.Clear(kBg);
		if (!verts.Empty())
			target.Draw(verts.Data(), (uint32)verts.Size(), NkPrimitiveType::NK_TRIANGLES);
		target.Display();

		// HUD titre (~5x/s).
		static float32 tacc = 1.0f;
		tacc += 0.016f;
		if (tacc >= 0.2f) {
			tacc = 0.0f;
			window.SetTitle(NkString::Format("NkAudioPlayer  %s  %.1fs / %.1fs  %s", paused ? "PAUSE" : "LECTURE",
											 (double)pos, durSec, path.CStr()));
		}

		if (!engine.IsPlaying(handle) && !paused)
			running = false; // fin naturelle
	}

	engine.Shutdown();
	audio::AudioLoader::Free(sample);
	window.Close();
	logger.Info("[NkAudioPlayer] fin.");
	return 0;
}
