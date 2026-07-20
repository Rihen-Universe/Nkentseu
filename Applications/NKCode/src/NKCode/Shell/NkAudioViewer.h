#pragma once
// =============================================================================
// NkAudioViewer.h — Lecteur AUDIO de NKCode (onglet média, mediaKind == 3).
//   Charge un fichier audio via NKAudio (WAV/MP3/OGG/FLAC/Opus), dessine sa
//   FORME D'ONDE (enveloppe min/max par colonne), et le joue via l'AudioEngine
//   natif (WASAPI/CoreAudio/ALSA/Oboe) : play/pause, seek (clic sur l'onde),
//   tête de lecture, temps courant/total, volume.
//
//   Le sample décodé (et son buffer) est conservé vivant dans un registre par
//   chemin — l'AudioEngine référence ce buffer pendant la lecture.
// @Author  Rihen
// =============================================================================
#include "NKGui/NKGui.h"
#include "NKAudio/NkAudio.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkFormat.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKCode/Project/NkCodeState.h" // OpenFile, StrEq

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu::nkgui;
		using namespace nkentseu::audio;

		// ── Une piste chargée (persistante tant que l'appli vit) ──────────────────
		struct NkAudioClip {
				NkString path;
				AudioSample sample;			  // buffer possédé par NKAudio (gardé vivant)
				bool loaded = false, ok = false;
				NkVector<float32> peakMin, peakMax; // enveloppe par colonne de pixels
				int32 peakW = 0;
				float32 dur = 0.f;
				float32 pos = 0.f;			  // position (secondes) : lecture OU point de seek
				AudioHandle handle = AUDIO_HANDLE_INVALID;
				bool started = false;		  // une voix existe (en lecture ou pause)
				bool loop = false;			  // relit en boucle
				float32 volume = 1.f;
				float32 speed = 1.f; // vitesse de lecture (multiplie le pitch)
				int32 srcRate = 0;   // frequence native (avant resample) - diag
				uint64 lastSeenTick = 0;	  // dernier frame ou le viewer a ete dessine (onglet actif)
		};

		inline NkVector<NkAudioClip *> &NkAudioClips() {
			static NkVector<NkAudioClip *> v;
			return v;
		}
		inline bool &NkAudioEngineTried() {
			static bool b = false;
			return b;
		}
		inline bool &NkAudioEngineOk() {
			static bool b = false;
			return b;
		}
		// Compteur de frames (avance chaque frame) + option « arreter a la sortie de l'onglet ».
		inline uint64 &NkAudioTick() {
			static uint64 t = 0;
			return t;
		}
		inline bool &NkAudioAutoStop() { // OPTION (defaut : oui)
			static bool b = true;
			return b;
		}

		// Registre par chemin (pointeurs stables : allocation dédiée par piste).
		inline NkAudioClip *NkAudioClipFor(const NkString &path) {
			auto &v = NkAudioClips();
			for (usize i = 0; i < v.Size(); ++i)
				if (StrEq(v[i]->path.CStr(), path.CStr()))
					return v[i];
			NkAudioClip *c = new NkAudioClip();
			c->path = path;
			v.PushBack(c);
			return c;
		}

		// À appeler CHAQUE FRAME avec le chemin de l'onglet ACTIF : si l'option est
		// active, arrête toute piste dont l'onglet n'est plus au premier plan
		// (chemin différent de l'onglet actif) — robuste, sans compteur de frames.
		inline void NkAudioStopInactive(const char *activePath) {
			if (!NkAudioAutoStop() || !NkAudioEngineOk())
				return;
			auto &v = NkAudioClips();
			for (usize i = 0; i < v.Size(); ++i) {
				NkAudioClip *c = v[i];
				if (c->started && (!activePath || !activePath[0] || !StrEq(c->path.CStr(), activePath))) {
					AudioEngine::Instance().Stop(c->handle, 0.f);
					c->started = false;
				}
			}
		}

		// Enveloppe min/max sur `W` colonnes (mix des canaux). Recalcul seulement
		// quand la largeur change nettement (scan O(frames)).
		inline void NkAudioComputePeaks(NkAudioClip *c, int32 W) {
			c->peakMin.Clear();
			c->peakMax.Clear();
			c->peakW = W;
			if (!c->ok || W <= 0 || !c->sample.data)
				return;
			const float32 *d = c->sample.data;
			const usize frames = c->sample.frameCount;
			const int32 ch = c->sample.channels > 0 ? c->sample.channels : 1;
			for (int32 x = 0; x < W; ++x) {
				usize f0 = (usize)((uint64)x * (uint64)frames / (uint64)W);
				usize f1 = (usize)((uint64)(x + 1) * (uint64)frames / (uint64)W);
				if (f1 <= f0)
					f1 = f0 + 1;
				if (f1 > frames)
					f1 = frames;
				float32 mn = 1.f, mx = -1.f;
				for (usize fr = f0; fr < f1; ++fr) {
					float32 s = 0.f;
					for (int32 k = 0; k < ch; ++k)
						s += d[fr * (usize)ch + (usize)k];
					s /= (float32)ch;
					if (s < mn)
						mn = s;
					if (s > mx)
						mx = s;
				}
				if (mn > mx) {
					mn = 0.f;
					mx = 0.f;
				}
				c->peakMin.PushBack(mn);
				c->peakMax.PushBack(mx);
			}
		}

		inline NkString NkAudioFmtTime(float32 s) { // m:ss
			if (s < 0.f)
				s = 0.f;
			const int32 t = (int32)(s + 0.0001f);
			return NkPrintf("%d:%02d", t / 60, t % 60);
		}

		// ── Viewer : dessine onde + transport dans `r` ────────────────────────────
		inline void DrawAudioViewer(NkGuiContext &ctx, OpenFile &f, const NkRect &r) {
			NkGuiDrawList &dl = ctx.DL();
			const NkGuiFont *font = ctx.font;
			const float32 lh = (font && font->Valid()) ? font->LineHeight() : 16.f;
			const float32 asc = (font && font->Valid()) ? font->Ascent() : 12.f;
			const NkVec2 mp = ctx.input.mousePos;
			dl.PushClipRect(r, true);
			dl.AddRectFilled(r, NkColor{24, 26, 30, 255});

			auto centered = [&](const char *msg, const NkColor &col) {
				if (!font || !font->Valid())
					return;
				const float32 tw = font->MeasureWidth(msg);
				dl.AddText(font->Face(), font->TexId(), {r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f + asc}, msg,
						   col);
			};

			NkAudioClip *c = NkAudioClipFor(f.path.ToString());

			// Moteur audio : tentative d'init UNE fois.
			if (!NkAudioEngineTried()) {
				NkAudioEngineTried() = true;
				AudioEngineConfig cfg;
				NkAudioEngineOk() = AudioEngine::Instance().Initialize(cfg);
			}
			const bool engineOk = NkAudioEngineOk();

			// Décodage UNE fois. Le moteur NKAudio convertit le taux à la volée par voix
			// (mixage), donc on garde le sample à sa fréquence native — pas de resample ici.
			if (!c->loaded) {
				c->loaded = true;
				c->sample = AudioLoader::Load(f.path.ToString().CStr());
				c->ok = c->sample.IsValid();
				c->srcRate = c->ok ? c->sample.sampleRate : 0;
				c->dur = c->ok ? c->sample.GetDuration() : 0.f;
			}
			if (!c->ok) {
				centered("Impossible de decoder l'audio", NkColor{240, 120, 110, 255});
				dl.PopClipRect();
				return;
			}

			// ── État de lecture (dérivé du moteur) ──
			bool isPlaying = c->started && engineOk && AudioEngine::Instance().IsPlaying(c->handle);
			const bool isPaused = c->started && engineOk && AudioEngine::Instance().IsPaused(c->handle);
			if (c->started && !isPlaying && !isPaused) { // lecture terminée
				c->started = false;
				if (c->pos >= c->dur - 0.06f)
					c->pos = 0.f;
			}
			if (isPlaying)
				c->pos = AudioEngine::Instance().GetPlaybackPosition(c->handle);

			// ── Disposition : onde en haut, transport en bas ──
			const float32 pad = 18.f, infoH = lh + 10.f, ctrlH = 46.f;
			const NkRect wave = {r.x + pad, r.y + pad, r.w - 2.f * pad, r.h - infoH - ctrlH - 2.f * pad};

			// Recalcule l'enveloppe si la largeur a changé (>2 px).
			const int32 W = (int32)wave.w;
			if (W > 0 && (c->peakW < W - 2 || c->peakW > W + 2))
				NkAudioComputePeaks(c, W);

			// Onde : axe médian + colonnes min/max ; partie jouée en accent.
			const float32 midY = wave.y + wave.h * 0.5f, halfH = wave.h * 0.5f - 2.f;
			dl.AddRectFilled({wave.x, midY, wave.w, 1.f}, NkColor{60, 66, 74, 255});
			const float32 playX = c->dur > 0.f ? wave.x + (c->pos / c->dur) * wave.w : wave.x;
			const int32 nCol = (int32)c->peakMax.Size();
			for (int32 x = 0; x < nCol; ++x) {
				const float32 mn = c->peakMin[x], mx = c->peakMax[x];
				const float32 y0 = midY - mx * halfH, y1 = midY - mn * halfH;
				const float32 cx = wave.x + (float32)x;
				const bool played = cx <= playX;
				dl.AddRectFilled({cx, y0, 1.f, (y1 - y0) > 1.f ? (y1 - y0) : 1.f},
								 played ? NkColor{88, 166, 255, 255} : NkColor{92, 100, 112, 255});
			}
			// Tête de lecture.
			dl.AddRectFilled({playX - 0.5f, wave.y, 1.5f, wave.h}, NkColor{247, 154, 40, 255});

			// Seek : clic (ou glisser) sur l'onde -> position.
			const bool overWave = NkGuiRectContains(wave, mp);
			static const void *s_seek = nullptr;
			if (overWave && ctx.input.mouseClicked[0])
				s_seek = c;
			if (s_seek == c) {
				if (ctx.input.mouseDown[0]) {
					float32 t = (mp.x - wave.x) / (wave.w > 1.f ? wave.w : 1.f);
					t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
					c->pos = t * c->dur;
					if (c->started && engineOk)
						AudioEngine::Instance().SetPlaybackPosition(c->handle, c->pos);
				} else
					s_seek = nullptr;
			}

			// ── Transport (bouton play/pause + stop + temps) ──
			const float32 cy = r.y + r.h - infoH - ctrlH;
			const NkRect ctrl = {r.x, cy, r.w, ctrlH};
			dl.AddRectFilled(ctrl, NkColor{20, 22, 26, 255});
			const float32 btnR = 15.f, bx = ctrl.x + 22.f + btnR, by = ctrl.y + ctrlH * 0.5f;
			const NkVec2 bc = {bx, by};
			const bool overBtn = (mp.x - bx) * (mp.x - bx) + (mp.y - by) * (mp.y - by) <= btnR * btnR;
			dl.AddCircleFilled(bc, btnR, overBtn ? NkColor{15, 115, 213, 255} : NkColor{40, 46, 54, 255});
			if (isPlaying) { // icône pause (deux barres)
				dl.AddRectFilled({bx - 5.f, by - 6.f, 3.5f, 12.f}, NkColor{235, 240, 245, 255});
				dl.AddRectFilled({bx + 2.f, by - 6.f, 3.5f, 12.f}, NkColor{235, 240, 245, 255});
			} else { // icône play (triangle)
				dl.AddTriangleFilled({bx - 4.f, by - 6.f}, {bx - 4.f, by + 6.f}, {bx + 7.f, by},
									 NkColor{235, 240, 245, 255});
			}
			if (overBtn && ctx.input.mouseClicked[0] && engineOk) {
				if (isPlaying)
					AudioEngine::Instance().Pause(c->handle);
				else if (isPaused)
					AudioEngine::Instance().Resume(c->handle);
				else { // (re)démarre une voix à la position courante
					VoiceParams p;
					p.volume = c->volume;
					p.pitch = c->speed; // sample deja a la freq du device -> pitch = vitesse pure
					p.startOffset = (c->pos < c->dur - 0.05f) ? c->pos : 0.f;
					p.looping = c->loop;
					p.bus = "Music";
					c->handle = AudioEngine::Instance().Play(c->sample, p);
					c->started = true;
					isPlaying = true;
				}
			}

			// Stop.
			const NkRect stopB = {bx + btnR + 14.f, by - 9.f, 18.f, 18.f};
			const bool overStop = NkGuiRectContains(stopB, mp);
			dl.AddRectFilled({stopB.x + 3.f, stopB.y + 3.f, 12.f, 12.f},
							 overStop ? NkColor{200, 90, 90, 255} : NkColor{120, 128, 138, 255}, 2.f);
			if (overStop && ctx.input.mouseClicked[0] && engineOk) {
				if (c->started)
					AudioEngine::Instance().Stop(c->handle, 0.f);
				c->started = false;
				c->pos = 0.f;
			}

			// Boucle (relit en boucle) — toggle.
			const NkRect loopB = {stopB.x + stopB.w + 12.f, by - 10.f, 20.f, 20.f};
			const bool overLoop = NkGuiRectContains(loopB, mp);
			dl.AddRectFilled(loopB, c->loop ? NkColor{15, 115, 213, 255}
				                              : (overLoop ? NkColor{54, 60, 70, 255} : NkColor{40, 46, 54, 255}), 4.f);
			{
				const NkColor ic = c->loop ? NkColor{240, 245, 250, 255} : NkColor{150, 158, 168, 255};
				dl.AddRect({loopB.x + 4.f, loopB.y + 6.f, 10.f, 8.f}, ic, 1.5f); // anneau
				dl.AddTriangleFilled({loopB.x + 12.f, loopB.y + 3.f}, {loopB.x + 12.f, loopB.y + 9.f},
					                     {loopB.x + 17.f, loopB.y + 6.f}, ic); // fleche
			}
			if (overLoop && ctx.input.mouseClicked[0]) {
				c->loop = !c->loop;
				if (c->started && engineOk)
					AudioEngine::Instance().SetLooping(c->handle, c->loop);
			}
			// Temps courant / total.
			if (font && font->Valid()) {
				const NkString tt = NkAudioFmtTime(c->pos) + NkString(" / ") + NkAudioFmtTime(c->dur);
				dl.AddText(font->Face(), font->TexId(), {loopB.x + loopB.w + 14.f, by - lh * 0.5f + asc}, tt.CStr(),
						   ctx.theme.text);
			}

			// Vitesse de lecture : [-] x.xx [+] (multiplie le pitch de la voix).
			if (engineOk && font && font->Valid()) {
				const float32 sx0 = loopB.x + loopB.w + 132.f;
				const NkRect sMinus = {sx0, by - 11.f, 20.f, 22.f};
				const NkRect sPlus = {sx0 + 76.f, by - 11.f, 20.f, 22.f};
				const bool oM = NkGuiRectContains(sMinus, mp), oP = NkGuiRectContains(sPlus, mp);
				dl.AddRectFilled(sMinus, oM ? NkColor{54, 60, 70, 255} : NkColor{40, 46, 54, 255}, 4.f);
				dl.AddRectFilled(sPlus, oP ? NkColor{54, 60, 70, 255} : NkColor{40, 46, 54, 255}, 4.f);
				dl.AddText(font->Face(), font->TexId(), {sMinus.x + 10.f - font->MeasureWidth("-") * 0.5f, by - lh * 0.5f + asc}, "-", ctx.theme.text);
				dl.AddText(font->Face(), font->TexId(), {sPlus.x + 10.f - font->MeasureWidth("+") * 0.5f, by - lh * 0.5f + asc}, "+", ctx.theme.text);
				dl.AddText(font->Face(), font->TexId(), {sMinus.x + 27.f, by - lh * 0.5f + asc}, NkPrintf("%.2fx", c->speed).CStr(), ctx.theme.text);
				if (oM && ctx.input.mouseClicked[0]) {
					c->speed = c->speed - 0.1f < 0.25f ? 0.25f : c->speed - 0.1f;
					if (c->started) AudioEngine::Instance().SetPitch(c->handle, c->speed);
				}
				if (oP && ctx.input.mouseClicked[0]) {
					c->speed = c->speed + 0.1f > 3.0f ? 3.0f : c->speed + 0.1f;
					if (c->started) AudioEngine::Instance().SetPitch(c->handle, c->speed);
				}
			}
			// OPTION : arreter la lecture quand on quitte l'onglet (toggle).
			if (engineOk && font && font->Valid()) {
				const char *lbl = "arret a la sortie de l'onglet";
				const float32 lw = font->MeasureWidth(lbl);
				const NkRect cb = {ctrl.x + ctrl.w - lw - 36.f, by - 8.f, 16.f, 16.f};
				const bool overCb = NkGuiRectContains(cb, mp);
				dl.AddRectFilled(cb, NkAudioAutoStop() ? NkColor{15, 115, 213, 255}
					                                       : (overCb ? NkColor{54, 60, 70, 255} : NkColor{40, 46, 54, 255}), 3.f);
				if (NkAudioAutoStop()) {
					dl.AddLine({cb.x + 3.f, cb.y + 8.f}, {cb.x + 6.5f, cb.y + 12.f}, NkColor{255, 255, 255, 255}, 2.f);
					dl.AddLine({cb.x + 6.5f, cb.y + 12.f}, {cb.x + 13.f, cb.y + 4.f}, NkColor{255, 255, 255, 255}, 2.f);
				}
				dl.AddText(font->Face(), font->TexId(), {cb.x + cb.w + 6.f, by - lh * 0.5f + asc}, lbl, ctx.theme.textDisabled);
				if (overCb && ctx.input.mouseClicked[0])
					NkAudioAutoStop() = !NkAudioAutoStop();
			}
			// Message si le moteur audio n'a pas pu démarrer.
			if (!engineOk && font && font->Valid()) {
				const char *w = "moteur audio indisponible (onde seule)";
				dl.AddText(font->Face(), font->TexId(), {ctrl.x + ctrl.w - font->MeasureWidth(w) - 16.f, by - lh * 0.5f + asc},
						   w, NkColor{220, 170, 90, 255});
			}

			// ── Barre d'info (nom, format, durée) ──
			if (font && font->Valid()) {
				const NkRect bar = {r.x, r.y + r.h - infoH, r.w, infoH};
				dl.AddRectFilled(bar, NkColor{16, 18, 22, 235});
				const int32 erInfo = engineOk ? AudioEngine::Instance().GetSampleRate() : 0;
				const char *bk = engineOk ? AudioEngine::Instance().GetBackendName() : "-";
				const NkString info = NkPrintf("%s      %d -> %d Hz (%s)  %d ch      %d Ko      %s", f.Name().CStr(),
											   c->srcRate, erInfo, bk, c->sample.channels, (int32)(f.mediaSize / 1024),
											   NkAudioFmtTime(c->dur).CStr());
				dl.AddText(font->Face(), font->TexId(), {bar.x + 12.f, bar.y + 5.f + asc}, info.CStr(), ctx.theme.text);
			}
			dl.PopClipRect();
		}

	} // namespace nkcode
} // namespace nkentseu
