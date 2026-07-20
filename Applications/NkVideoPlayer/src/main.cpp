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
#include "NKAudio/NkAudio.h"
#include "NKAudio/Streaming/NkAudioStream.h"
#include "NKAudio/Streaming/NkAudioStreamPlayer.h"
#include "NKMemory/NkAllocator.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

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

// ─────────────────────────────────────────────────────────────────────────────
// Étalonnage couleur (color grade) appliqué à l'image RGBA décodée AVANT upload.
// NB : c'est un grade CPU intégré (presets). Le jour où NKRenderer expose des LUT
// .cube en 2D, on branchera ici une LUT à la place de ces presets (le point de
// câblage est le même : transformer les pixels juste avant frameTex.Update).
// ─────────────────────────────────────────────────────────────────────────────
enum class Grade : int32 { Neutral = 0, Cinema, Warm, Cool, Mono, Vivid, Count };

static const char *GradeName(Grade g) {
	switch (g) {
		case Grade::Cinema:
			return "Cinema teal/orange";
		case Grade::Warm:
			return "Chaud";
		case Grade::Cool:
			return "Froid";
		case Grade::Mono:
			return "N&B";
		case Grade::Vivid:
			return "Vif";
		default:
			return "Neutre";
	}
}

static inline nk_uint8 U8(float32 v) {
	return (nk_uint8)(v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v + 0.5f));
}

static void ApplyGrade(nk_uint8 *px, uint64 pixelCount, Grade g) {
	if (g == Grade::Neutral)
		return;
	for (uint64 i = 0; i < pixelCount; ++i) {
		float32 r = (float32)px[i * 4 + 0];
		float32 gr = (float32)px[i * 4 + 1];
		float32 b = (float32)px[i * 4 + 2];
		const float32 luma = 0.299f * r + 0.587f * gr + 0.114f * b;
		switch (g) {
			case Grade::Cinema: {
				// Contraste filmique doux + ombres froides / hautes lumières chaudes.
				const float32 t = (luma / 255.0f - 0.5f) * 2.0f; // [-1..1]
				r = (r - 128.0f) * 1.08f + 128.0f;
				gr = (gr - 128.0f) * 1.05f + 128.0f;
				b = (b - 128.0f) * 1.08f + 128.0f;
				r *= 1.0f + 0.10f * t;	// chaud dans les clairs
				b *= 1.0f - 0.10f * t;	// froid dans les sombres
				r = luma + (r - luma) * 0.92f;
				gr = luma + (gr - luma) * 0.92f;
				b = luma + (b - luma) * 0.92f;
				break;
			}
			case Grade::Warm:
				r *= 1.10f;
				b *= 0.90f;
				break;
			case Grade::Cool:
				r *= 0.90f;
				b *= 1.10f;
				break;
			case Grade::Mono:
				r = gr = b = luma;
				break;
			case Grade::Vivid:
				r = luma + (r - luma) * 1.35f;
				gr = luma + (gr - luma) * 1.35f;
				b = luma + (b - luma) * 1.35f;
				r = (r - 128.0f) * 1.10f + 128.0f;
				gr = (gr - 128.0f) * 1.10f + 128.0f;
				b = (b - 128.0f) * 1.10f + 128.0f;
				break;
			default:
				break;
		}
		px[i * 4 + 0] = U8(r);
		px[i * 4 + 1] = U8(gr);
		px[i * 4 + 2] = U8(b);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// LUT 3D .cube (format Adobe/DaVinci Resolve) : etalonnage professionnel « cinema ».
// Un fichier .cube contient LUT_3D_SIZE N puis N^3 lignes « R G B » in [0,1], R variant
// le plus vite (r + g*N + b*N*N). On applique par interpolation trilineaire.
// ─────────────────────────────────────────────────────────────────────────────
struct NkCubeLut {
		int32 size = 0;
		NkVector<float32> data; // 3 * size^3 (RGB entrelaces)
		bool IsValid() const {
			return size >= 2 && data.Size() == (uint64)size * size * size * 3;
		}
};

// Parse minimal d'un .cube (ignore commentaires #, TITLE, DOMAIN_*). 3D uniquement.
static bool LoadCubeLut(const char *path, NkCubeLut &lut) {
	FILE *f = fopen(path, "rb");
	if (!f)
		return false;
	lut.size = 0;
	lut.data.Clear();
	char line[256];
	int32 expected = 0;
	while (fgets(line, sizeof(line), f)) {
		// Ignore espaces initiaux.
		const char *p = line;
		while (*p == ' ' || *p == '\t')
			++p;
		if (*p == '#' || *p == '\r' || *p == '\n' || *p == 0)
			continue;
		if (strncmp(p, "LUT_3D_SIZE", 11) == 0) {
			int n = 0;
			if (sscanf(p + 11, "%d", &n) == 1 && n >= 2 && n <= 128) {
				lut.size = n;
				expected = n * n * n * 3;
				lut.data.Clear();
			}
			continue;
		}
		// Lignes de meta connues -> ignorer.
		if ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z'))
			continue;
		// Sinon : triplet R G B.
		float r = 0, g = 0, b = 0;
		if (sscanf(p, "%f %f %f", &r, &g, &b) == 3) {
			lut.data.PushBack((float32)r);
			lut.data.PushBack((float32)g);
			lut.data.PushBack((float32)b);
		}
	}
	fclose(f);
	(void)expected;
	return lut.IsValid();
}

static inline float32 LerpF(float32 a, float32 b, float32 t) {
	return a + (b - a) * t;
}

// Applique la LUT 3D (trilineaire) a l'image RGBA in place.
static void ApplyCubeLut(nk_uint8 *px, uint64 pixelCount, const NkCubeLut &lut) {
	if (!lut.IsValid())
		return;
	const int32 N = lut.size;
	const float32 scale = (float32)(N - 1);
	for (uint64 i = 0; i < pixelCount; ++i) {
		const float32 R = px[i * 4 + 0] / 255.0f;
		const float32 G = px[i * 4 + 1] / 255.0f;
		const float32 B = px[i * 4 + 2] / 255.0f;
		const float32 fr = R * scale, fg = G * scale, fb = B * scale;
		int32 r0 = (int32)fr, g0 = (int32)fg, b0 = (int32)fb;
		if (r0 >= N - 1)
			r0 = N - 2;
		if (g0 >= N - 1)
			g0 = N - 2;
		if (b0 >= N - 1)
			b0 = N - 2;
		if (r0 < 0)
			r0 = 0;
		if (g0 < 0)
			g0 = 0;
		if (b0 < 0)
			b0 = 0;
		const float32 dr = fr - r0, dg = fg - g0, db = fb - b0;
		auto sample = [&](int32 r, int32 g, int32 b, float32 &oR, float32 &oG, float32 &oB) {
			const uint64 idx = ((uint64)b * N * N + (uint64)g * N + (uint64)r) * 3;
			oR = lut.data[idx + 0];
			oG = lut.data[idx + 1];
			oB = lut.data[idx + 2];
		};
		float32 c000R, c000G, c000B, c100R, c100G, c100B, c010R, c010G, c010B, c110R, c110G, c110B;
		float32 c001R, c001G, c001B, c101R, c101G, c101B, c011R, c011G, c011B, c111R, c111G, c111B;
		sample(r0, g0, b0, c000R, c000G, c000B);
		sample(r0 + 1, g0, b0, c100R, c100G, c100B);
		sample(r0, g0 + 1, b0, c010R, c010G, c010B);
		sample(r0 + 1, g0 + 1, b0, c110R, c110G, c110B);
		sample(r0, g0, b0 + 1, c001R, c001G, c001B);
		sample(r0 + 1, g0, b0 + 1, c101R, c101G, c101B);
		sample(r0, g0 + 1, b0 + 1, c011R, c011G, c011B);
		sample(r0 + 1, g0 + 1, b0 + 1, c111R, c111G, c111B);
		// Interpolation trilineaire.
		const float32 oR = LerpF(LerpF(LerpF(c000R, c100R, dr), LerpF(c010R, c110R, dr), dg),
								 LerpF(LerpF(c001R, c101R, dr), LerpF(c011R, c111R, dr), dg), db);
		const float32 oG = LerpF(LerpF(LerpF(c000G, c100G, dr), LerpF(c010G, c110G, dr), dg),
								 LerpF(LerpF(c001G, c101G, dr), LerpF(c011G, c111G, dr), dg), db);
		const float32 oB = LerpF(LerpF(LerpF(c000B, c100B, dr), LerpF(c010B, c110B, dr), dg),
								 LerpF(LerpF(c001B, c101B, dr), LerpF(c011B, c111B, dr), dg), db);
		px[i * 4 + 0] = U8(oR * 255.0f);
		px[i * 4 + 1] = U8(oG * 255.0f);
		px[i * 4 + 2] = U8(oB * 255.0f);
	}
}

int nkmain(const NkEntryState &state) {
	// ── Arguments : [1] = média vidéo, [2] éventuel = audio, --step N, --lut fichier.cube
	NkString videoPath, audioPath, lutPath;
	int32 stepArg = 0; // 0 => défaut = ~1 seconde (calculé plus bas d'après le fps)
	for (uint64 i = 1; i < state.args.Size(); ++i) {
		const NkString &a = state.args[i];
		if ((a == "--step" || a == "-s") && i + 1 < state.args.Size()) {
			stepArg = 0;
			const char *v = state.args[++i].CStr();
			for (uint64 k = 0; v[k] >= '0' && v[k] <= '9'; ++k)
				stepArg = stepArg * 10 + (v[k] - '0');
		} else if ((a == "--lut" || a == "-l") && i + 1 < state.args.Size()) {
			lutPath = state.args[++i];
		} else if (LooksLikeAudio(a))
			audioPath = a;
		else if (videoPath.Empty())
			videoPath = a;
	}
	if (videoPath.Empty()) {
		logger.Error("[NkVideoPlayer] usage : NkVideoPlayer <video> [audio] [--step N] [--lut fichier.cube]");
		return -1;
	}

	// Charge la LUT .cube si fournie (etalonnage cinema professionnel, active par 'U').
	NkCubeLut lut;
	bool lutLoaded = false;
	if (!lutPath.Empty()) {
		lutLoaded = LoadCubeLut(lutPath.CStr(), lut);
		if (lutLoaded)
			logger.Info("[NkVideoPlayer] LUT chargee : {0} (taille {1})", lutPath.CStr(), lut.size);
		else
			logger.Warn("[NkVideoPlayer] LUT illisible/invalide : {0}", lutPath.CStr());
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

	// ── 5) Audio STREAMÉ (jamais tout en RAM) : piste DU CONTENEUR (MP4/MOV/WebM,
	//    AAC décodé from-scratch par paquets) sinon fichier externe optionnel
	//    (WAV/FLAC/MP3/OGG). AudioStreamPlayer (ring buffer + thread décodeur,
	//    déjà livré dans NKAudio) alimente une voix procédurale ; c'est SA propre
	//    horloge de contenu (GetPositionSeconds, indépendante de l'AudioEngine —
	//    AudioEngine::GetPlaybackPosition renvoie toujours 0 pour une voix
	//    procédurale) qui sert d'HORLOGE MAÎTRESSE à la vidéo.
	audio::AudioEngine &engine = audio::AudioEngine::Instance();
	audio::AudioStreamPlayer streamPlayer;
	audio::AudioHandle audioHandle{};
	bool audioOn = false;
	{
		audio::IAudioStream *astream = audio::OpenAudioStream(videoPath.CStr()); // piste du conteneur
		if (!astream && !audioPath.Empty())
			astream = audio::OpenAudioStream(audioPath.CStr()); // fichier externe (WAV/FLAC/MP3/OGG)
		if (astream) {
			audio::AudioEngineConfig acfg;
			const bool engineUp = engine.Initialize(acfg);
			const bool playerUp =
				engineUp && streamPlayer.Init(engine.GetSampleRate(), engine.GetChannels(), 88200);
			// Play() ne prend possession d'astream QUE s'il renvoie true (sinon `astream`
			// reste nôtre à libérer — voir AudioStreamPlayer::Play/Shutdown).
			const bool played = playerUp && streamPlayer.Play(astream, /*loop=*/false);
			if (played) {
				audio::VoiceParams vp;
				vp.bus = "Music";
				audioHandle = engine.PlayProcedural(
					[&streamPlayer](float32 *buf, int32 frames, int32 /*channels*/) {
						streamPlayer.ReadFrames(buf, frames);
					},
					vp);
				audioOn = true;
				logger.Info("[NkVideoPlayer] audio streamé : {0} Hz, {1} canal(aux), ~{2:.1f}s (source {3} Hz)",
							engine.GetSampleRate(), engine.GetChannels(),
							(double)astream->GetFrameCount() / (double)math::NkMax(1, astream->GetSampleRate()),
							astream->GetSampleRate());
			} else {
				memory::NkGetDefaultAllocator().Delete(astream);
				if (playerUp)
					streamPlayer.Shutdown();
				if (engineUp)
					engine.Shutdown();
			}
		}
	}

	// ── 6) Boucle : cadence au fps, décode -> (grade) -> upload -> dessine ─────
	//   CONTRÔLES : Espace=pause/lecture · S=stop(retour début) · L=boucle on/off
	//               →/←=avancer/reculer d'un PAS · ↑/↓=vitesse · C=étalonnage couleur · U=LUT
	//   Le PAS de saut est --step N images (défaut ≈ 1 seconde). LUT via --lut fichier.cube.
	bool running = true;
	auto &events = NkEvents();
	events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });

	const int32 step = (stepArg > 0) ? stepArg : (int32)(fps + 0.5f); // pas de saut (images)
	media::NkVideoFrame fr;
	NkVector<nk_uint8> rawFrame; // copie brute (non étalonnée) de l'image courante
	int32 rawW = 0, rawH = 0;
	bool haveFrame = false;
	bool ended = false;
	bool paused = false;
	bool loop = true;
	float32 speed = 1.0f;
	Grade grade = Grade::Neutral;
	bool lutOn = lutLoaded; // si une LUT est fournie, active par defaut ; bascule avec 'U'

	// Applique preset d'etalonnage PUIS LUT .cube (si active) sur un buffer RGBA.
	auto applyLook = [&](nk_uint8 *px, uint64 pixelCount) {
		ApplyGrade(px, pixelCount, grade);
		if (lutOn && lutLoaded)
			ApplyCubeLut(px, pixelCount, lut);
	};
	// Décode/étalonne/upload l'image fraîchement lue dans `fr`.
	auto pushFrame = [&]() {
		rawW = fr.width;
		rawH = fr.height;
		rawFrame = fr.rgba; // conserve la version brute pour un ré-étalonnage à la volée
		applyLook(fr.rgba.Data(), (uint64)fr.width * fr.height);
		frameTex.Update(fr.rgba.Data(), (uint32)fr.width, (uint32)fr.height, 0, 0);
		haveFrame = true;
	};
	// Ré-applique l'étalonnage sur l'image courante (ex. changement de grade/LUT en pause).
	auto regrade = [&]() {
		if (!haveFrame || rawFrame.Empty())
			return;
		fr.rgba = rawFrame;
		applyLook(fr.rgba.Data(), (uint64)rawW * rawH);
		frameTex.Update(fr.rgba.Data(), (uint32)rawW, (uint32)rawH, 0, 0);
	};
	// Va à l'image `target` (accès aléatoire : MJPEG/AVI/MOV/séquences ; no-op si non supporté).
	auto seekTo = [&](int32 tgt) {
		if (vin.frameCount > 0) {
			if (tgt < 0)
				tgt = 0;
			if (tgt > vin.frameCount - 1)
				tgt = vin.frameCount - 1;
		} else if (tgt < 0)
			tgt = 0;
		if (reader.SeekFrame(tgt) && reader.ReadFrame(fr)) {
			pushFrame();
			ended = false;
		}
	};

	// Cale l'audio streamé sur une position vidéo (secondes) — seek/stop/boucle.
	// Une seule opération pour les deux anciens cas (redémarrer au bouclage ==
	// simplement re-seeker à 0) : SeekContent repositionne le stream SOUS-JACENT
	// (donc reste valide même pour un fichier externe non-conteneur) + vide le
	// ring buffer + réinitialise l'horloge de contenu (GetPositionSeconds).
	auto syncAudioTo = [&](float32 seconds) {
		if (!audioOn)
			return;
		streamPlayer.SeekContent(seconds < 0.0f ? 0.0f : seconds);
	};

	// ── Horloge média ──────────────────────────────────────────────────────────
	// L'AUDIO est l'horloge maîtresse : la vidéo affiche l'image que le son a
	// atteinte (mediaClock = position de la voix). Sans audio, horloge cumulée.
	float64 mediaClock = 0.0;
	float32 titleAcc = 1.0f;
	uint32 lastW = 0, lastH = 0;
	NkClock clock;
	bool firstFrame = true;

	while (running && window.IsOpen()) {
		const float32 dt = clock.Tick().delta;
		titleAcc += dt;

		// ── Entrées clavier ───────────────────────────────────────────────────
		while (NkEvent *ev = events.PollEvent()) {
			if (auto *k = ev->As<NkKeyPressEvent>()) {
				switch (k->GetKey()) {
					case NkKey::NK_ESCAPE:
						running = false;
						break;
					case NkKey::NK_SPACE:
						paused = !paused;
						if (audioOn) {
							if (paused)
								engine.Pause(audioHandle);
							else
								engine.Resume(audioHandle);
						}
						break;
					case NkKey::NK_S: // stop -> retour au début, en pause
						seekTo(0);
						paused = true;
						mediaClock = 0.0;
						if (audioOn) {
							syncAudioTo(0.0f);
							engine.Pause(audioHandle);
						}
						break;
					case NkKey::NK_L: // boucle on/off
						loop = !loop;
						break;
					case NkKey::NK_RIGHT: // avancer d'un pas
						seekTo(reader.CurrentIndex() + step);
						mediaClock = (float64)reader.CurrentIndex() / (float64)fps;
						syncAudioTo((float32)mediaClock);
						break;
					case NkKey::NK_LEFT: // reculer d'un pas
						seekTo(reader.CurrentIndex() - step);
						mediaClock = (float64)reader.CurrentIndex() / (float64)fps;
						syncAudioTo((float32)mediaClock);
						break;
					case NkKey::NK_UP: // plus vite
						speed = math::NkMin(speed * 1.25f, 4.0f);
						if (audioOn) {
							streamPlayer.SetSpeed(speed);
							streamPlayer.FlushRing(); // applique la vitesse immediatement (sinon
													  // ~1-2s de contenu deja bufferise a l'ancienne vitesse)
						}
						break;
					case NkKey::NK_DOWN: // plus lent
						speed = math::NkMax(speed / 1.25f, 0.1f);
						if (audioOn) {
							streamPlayer.SetSpeed(speed);
							streamPlayer.FlushRing();
						}
						break;
					case NkKey::NK_C: // cycle étalonnage couleur
						grade = (Grade)(((int32)grade + 1) % (int32)Grade::Count);
						regrade();
						break;
					case NkKey::NK_U: // active/desactive la LUT .cube (si chargee)
						if (lutLoaded) {
							lutOn = !lutOn;
							regrade();
						}
						break;
					default:
						break;
				}
			}
		}

		// ── Avance temporelle (horloge maîtresse = AUDIO si présent) ───────────
		if (!paused && !ended) {
			// AudioStreamPlayer::GetPositionSeconds() = horloge de CONTENU tenue par le
			// thread audio lui-même (AudioEngine::GetPlaybackPosition renverrait toujours 0
			// pour une voix procédurale). ⚠️ On teste !IsFinished() (PAS IsActive()) : le
			// producteur passe IsActive()=false dès qu'il a fini de DÉCODER, alors qu'il peut
			// encore rester jusqu'à ~2s de contenu déjà décodé dans le ring — IsActive() seul
			// ferait décrocher la vidéo ~2s avant la fin réelle du son. Si le flux audio est
			// VRAIMENT plus court que la vidéo (IsFinished()==true), on repasse en horloge cumulée.
			if (audioOn && !streamPlayer.IsFinished())
				mediaClock = streamPlayer.GetPositionSeconds();
			else
				mediaClock += (float64)(dt * speed);
			// L'image que l'horloge a atteinte ; on rattrape par BUDGET DE TEMPS (pas un
			// nombre fixe de décodes) et on ne pousse (upload texture + draw) que la
			// DERNIÈRE image atteinte dans la rafale — jamais les intermédiaires.
			// ⚠️ Un décodeur H264 from-scratch (scalaire, non-SIMD) n'a PAS de marge de
			// temps réel garantie sur du contenu réel (mesuré : ~8-24 fps selon le profil/
			// résolution, contre les 24-30 fps habituels d'une source — voir ROADMAP NKMedia
			// "Performance décodage"). Un cap fixe à 4 décodes/tick faisait DIVERGER l'écart
			// vidéo/audio sans limite (chaque frame décodée ET affichée coûte plus cher que
			// le budget d'une frame réelle -> le retard grandit à chaque tick, "vidéo hyper
			// lente"). Ici : on décode en rafale bornée par le TEMPS (adapte automatiquement
			// à la vitesse réelle du décodeur, quelle qu'elle soit) et on n'affiche QUE la
			// dernière image atteinte (économise l'upload/dessin des images intermédiaires,
			// qui de toute façon seraient immédiatement remplacées) -> dégradation aussi
			// gracieuse que le débit de décodage le permet structurellement, au lieu de
			// diverger. Ne RÉSOUT PAS un décodeur structurellement trop lent (il faudrait
			// l'accélérer), mais évite le pire.
			const int32 targetIdx = (int32)(mediaClock * (float64)fps);
			bool wrapped = false;
			bool havePendingFrame = false; // décodée dans cette rafale, pas encore affichée
			NkChrono burstTimer;
			while ((firstFrame || reader.CurrentIndex() < targetIdx) &&
				   burstTimer.Elapsed().ToMilliseconds() < 150.0) {
				if (reader.ReadFrame(fr)) {
					havePendingFrame = true;
					firstFrame = false;
				} else if (loop) {
					if (reader.SeekFrame(0) && reader.ReadFrame(fr)) {
						havePendingFrame = true;
						mediaClock = 0.0;
						syncAudioTo(0.0f); // re-seek le stream + vide le ring + reset l'horloge
						wrapped = true;
					} else {
						ended = true; // pas de rembobinage possible -> fige la dernière image
					}
					break;
				} else {
					paused = true; // fin sans boucle -> fige sur la dernière image
					if (audioOn)
						engine.Pause(audioHandle);
					break;
				}
			}
			if (havePendingFrame)
				pushFrame(); // upload + dessin UNE SEULE FOIS, avec la dernière image de la rafale
			(void)wrapped;
		}

		// ── Resize éventuel ─────────────────────────────────────────────────────
		const math::NkVec2u sz = target.GetSize();
		if (sz.x != lastW || sz.y != lastH) {
			if (lastW != 0 && sz.x > 0 && sz.y > 0)
				target.OnResize(sz.x, sz.y);
			lastW = sz.x;
			lastH = sz.y;
		}
		const float32 W = (float32)sz.x, H = (float32)sz.y;

		// Letterbox (préserve le ratio).
		const float32 s = math::NkMin(W / (float32)vidW, H / (float32)vidH);
		sprite.SetScale(s, s);
		sprite.SetPosition((W - (float32)vidW * s) * 0.5f, (H - (float32)vidH * s) * 0.5f);

		// ── HUD dans la barre de titre (~5x/s) ──────────────────────────────────
		if (titleAcc >= 0.2f) {
			titleAcc = 0.0f;
			const int32 idx = reader.CurrentIndex();
			const char *lutTxt = lutLoaded ? (lutOn ? "LUT:on" : "LUT:off") : "LUT:-";
			window.SetTitle(NkString::Format("NkVideoPlayer  %s  img %d/%d  x%.2f  [%s]  %s  boucle:%s  (%s)",
											 paused ? "PAUSE" : "LECTURE", idx,
											 vin.frameCount > 0 ? vin.frameCount : -1, (double)speed, GradeName(grade),
											 lutTxt, loop ? "on" : "off", videoPath.CStr()));
		}

		// ── Rendu ───────────────────────────────────────────────────────────────
		target.Clear(NkColor2D{0, 0, 0, 255});
		if (haveFrame)
			target.Draw(static_cast<const NkDrawable &>(sprite));
		target.Display();
	}

	// ── Libération ────────────────────────────────────────────────────────────
	// streamPlayer.Shutdown() joint le thread décodeur et libère le IAudioStream
	// qu'il possède (ContainerAudioStream/MemoryStream/WavStream) ; l'ordre importe :
	// arrêter l'engine (qui appelle le callback procédural depuis son thread audio)
	// AVANT de détruire streamPlayer, pour ne jamais mixer un stream en cours de
	// destruction.
	if (audioOn) {
		engine.Shutdown();
		streamPlayer.Shutdown();
	}
	window.Close();
	logger.Info("[NkVideoPlayer] fin.");
	return 0;
}
