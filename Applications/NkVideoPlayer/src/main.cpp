// =============================================================================
// NkVideoPlayer — lecteur vidéo de RÉFÉRENCE, MULTI-PLATEFORME.
// -----------------------------------------------------------------------------
// Montre le patron CORRECT pour AFFICHER une vidéo à l'écran, portable sur tous
// les OS supportés (pas de GDI/Win32 : on passe par NKCanvas, qui rend via
// OpenGL/Vulkan/DX/Software selon la plateforme) :
//
//   1. NkWindow          -> fenêtre native (Win/Linux/macOS/…)
//   2. NkRenderWindow    -> cible de rendu 2D NKCanvas (backend OpenGL par défaut)
//   3. NkVideoReader     -> décode chaque image en RGBA (AVI/MOV/MP4 · MJPEG/H264 · séquences)
//   4. NkTexture         -> reçoit les pixels RGBA de l'image courante (Update)
//   5. NkSprite          -> affiche la texture, mise à l'échelle pour remplir la fenêtre
//   6. boucle cadencée au fps de la vidéo (Clear -> Draw -> Display)
//
// Optionnel : un 2e argument audio est joué en parallèle via NKAudio (démo A/V).
//
//   Usage : NkVideoPlayer <video.avi|mov|mp4|dossier_images> [audio.wav|mp3|ogg]
//
// ⚠️ À l'attention de l'agent NKCode : ce fichier N'UTILISE QUE des API publiques
// (NKCanvas + NKMedia + NKAudio). Le cœur — ouvrir, décoder image par image en
// RGBA, uploader dans une texture, dessiner — est directement transposable dans
// un panneau/onglet de l'IDE (remplacer NkRenderWindow par la cible de rendu de
// l'éditeur ; le reste est identique).
// =============================================================================
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkColor.h"
#include "NKMath/NKMath.h"
#include "NKContainers/String/NkString.h"

#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Renderer/Resources/NkTexture.h"
#include "NKCanvas/Renderer/Resources/NkSprite.h"

#include "NKMedia/Video/NkVideoReader.h"
#include "NKAudio/NKAudio.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

NKENTSEU_DEFINE_APP_DATA(([]() {
	NkAppData d{};
	d.appName = "NkVideoPlayer";
	d.appVersion = "1.0.0";
	return d;
})());

// Petit helper : le chemin se termine-t-il par une extension audio ?
static bool LooksLikeAudio(const NkString &p) {
	const char *ext[] = {".wav", ".mp3", ".ogg", ".flac", ".opus", ".oga"};
	const char *c = p.CStr();
	uint64 n = 0;
	while (c[n])
		++n;
	for (const char *e : ext) {
		uint64 el = 0;
		while (e[el])
			++el;
		if (n < el)
			continue;
		bool match = true;
		for (uint64 i = 0; i < el; ++i) {
			char a = c[n - el + i];
			if (a >= 'A' && a <= 'Z')
				a = (char)(a - 'A' + 'a');
			if (a != e[i]) {
				match = false;
				break;
			}
		}
		if (match)
			return true;
	}
	return false;
}

int nkmain(const NkEntryState &state) {
	// ── Arguments : [1] = média vidéo, [2] éventuel = audio ───────────────────
	NkString videoPath, audioPath;
	for (uint64 i = 1; i < state.args.Size(); ++i) {
		const NkString &a = state.args[i];
		if (LooksLikeAudio(a))
			audioPath = a;
		else if (videoPath.Empty())
			videoPath = a;
	}
	if (videoPath.Empty()) {
		logger.Error("[NkVideoPlayer] usage : NkVideoPlayer <video> [audio]");
		return -1;
	}

	// ── 1) Ouvre la vidéo (décodage image par image en RGBA) ──────────────────
	media::NkVideoReader reader;
	if (!reader.Open(videoPath.CStr())) {
		logger.Error("[NkVideoPlayer] impossible d'ouvrir/lire : {0}", videoPath.CStr());
		return -2;
	}
	const media::NkVideoReaderInfo &vin = reader.Info();
	const int32 vidW = vin.width > 0 ? vin.width : 1;
	const int32 vidH = vin.height > 0 ? vin.height : 1;
	const float32 fps = vin.fps > 1.0f ? vin.fps : 25.0f;
	logger.Info("[NkVideoPlayer] {0} : {1}/{2} {3}x{4} {5}fps frames={6}", videoPath.CStr(), vin.container.CStr(),
				vin.codec.CStr(), vidW, vidH, (double)fps, vin.frameCount);

	// ── 2) Fenêtre dimensionnée sur la vidéo (bornée à 1600x900) ──────────────
	float32 winScale = 1.0f;
	if (vidW > 1600 || vidH > 900)
		winScale = math::NkMin(1600.0f / (float32)vidW, 900.0f / (float32)vidH);
	NkWindowConfig cfg;
	cfg.title = NkString::Format("NkVideoPlayer - %s", videoPath.CStr());
	cfg.width = (uint32)math::NkMax(160.0f, (float32)vidW * winScale);
	cfg.height = (uint32)math::NkMax(90.0f, (float32)vidH * winScale);
	cfg.centered = true;
	cfg.resizable = true;
	NkWindow window;
	if (!window.Create(cfg)) {
		logger.Error("[NkVideoPlayer] echec creation fenetre");
		return -3;
	}

	// ── 3) Cible de rendu 2D NKCanvas (backend portable) ──────────────────────
	NkContextDesc desc;
	desc.api = NkGraphicsApi::NK_GFX_API_OPENGL; // le plus stable multi-OS
	NkRenderWindow target(window, desc);
	if (!target.IsValid()) {
		logger.Error("[NkVideoPlayer] echec NkRenderWindow");
		window.Close();
		return -4;
	}

	// ── 4) Texture RGBA de la taille vidéo + sprite d'affichage ───────────────
	NkTexture frameTex;
	if (!frameTex.Create(*target.GetRenderer(), (uint32)vidW, (uint32)vidH, NkColor2D{0, 0, 0, 255})) {
		logger.Error("[NkVideoPlayer] echec creation texture {0}x{1}", vidW, vidH);
		window.Close();
		return -5;
	}
	NkSprite sprite(frameTex);

	// ── 5) Audio optionnel (démo A/V, via NKAudio) ────────────────────────────
	audio::AudioEngine &engine = audio::AudioEngine::Instance();
	audio::AudioSample audioSample;
	bool audioOn = false;
	if (!audioPath.Empty()) {
		audio::AudioEngineConfig acfg;
		if (engine.Initialize(acfg)) {
			audioSample = audio::AudioLoader::Load(audioPath.CStr());
			if (audioSample.IsValid()) {
				audio::VoiceParams vp;
				vp.bus = "Music";
				engine.Play(audioSample, vp);
				audioOn = true;
				logger.Info("[NkVideoPlayer] audio : {0} ({1} Hz)", audioPath.CStr(), audioSample.sampleRate);
			} else {
				engine.Shutdown();
			}
		}
	}

	// ── 6) Boucle : cadence au fps, décode -> upload -> dessine ───────────────
	bool running = true;
	auto &events = NkEvents();
	events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });

	const float32 frameDur = 1.0f / fps;
	float32 acc = frameDur; // force la 1re image immédiatement
	media::NkVideoFrame fr;
	bool haveFrame = false;
	bool ended = false;
	NkClock clock;

	while (running && window.IsOpen()) {
		const float32 dt = clock.Tick().delta;
		acc += dt;

		// Événements (fermeture, Échap, Espace = pause).
		static bool paused = false;
		while (NkEvent *ev = events.PollEvent()) {
			if (auto *k = ev->As<NkKeyPressEvent>()) {
				if (k->GetKey() == NkKey::NK_ESCAPE)
					running = false;
				else if (k->GetKey() == NkKey::NK_SPACE)
					paused = !paused;
			}
		}

		// Avance d'une image si le temps de l'image est écoulé.
		if (!paused && acc >= frameDur && !ended) {
			acc -= frameDur;
			if (reader.ReadFrame(fr)) {
				frameTex.Update(fr.rgba.Data(), (uint32)fr.width, (uint32)fr.height, 0, 0);
				haveFrame = true;
			} else {
				// Fin : reboucle (les séquences/MJPEG supportent SeekFrame(0)).
				if (reader.SeekFrame(0)) {
					if (audioOn) {
						// resynchronise l'audio depuis le début.
						engine.StopAll();
						audio::VoiceParams vp;
						vp.bus = "Music";
						engine.Play(audioSample, vp);
					}
				} else {
					ended = true; // pas de rembobinage possible -> fige la dernière image
				}
			}
		}

		// Resize éventuel de la fenêtre.
		const math::NkVec2u sz = target.GetSize();
		static uint32 lastW = 0, lastH = 0;
		if (sz.x != lastW || sz.y != lastH) {
			if (lastW != 0 && sz.x > 0 && sz.y > 0)
				target.OnResize(sz.x, sz.y);
			lastW = sz.x;
			lastH = sz.y;
		}
		const float32 W = (float32)sz.x, H = (float32)sz.y;

		// Mise à l'échelle en préservant le ratio (letterbox).
		const float32 s = math::NkMin(W / (float32)vidW, H / (float32)vidH);
		sprite.SetScale(s, s);
		sprite.SetPosition((W - (float32)vidW * s) * 0.5f, (H - (float32)vidH * s) * 0.5f);

		// Rendu.
		target.Clear(NkColor2D{0, 0, 0, 255});
		if (haveFrame)
			target.Draw(static_cast<const NkDrawable &>(sprite));
		target.Display();
	}

	// ── Libération ────────────────────────────────────────────────────────────
	if (audioOn) {
		engine.Shutdown();
		audio::AudioLoader::Free(audioSample);
	}
	window.Close();
	logger.Info("[NkVideoPlayer] fin.");
	return 0;
}
