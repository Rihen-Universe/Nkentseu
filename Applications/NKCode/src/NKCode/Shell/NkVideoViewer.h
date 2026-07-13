#pragma once
// =============================================================================
// NkVideoViewer.h — Lecteur VIDEO de NKCode (onglet media, mediaKind == 2).
//   Ouvre un fichier video via NKMedia (NkVideoReader : AVI/MOV/MP4 MJPEG,
//   sequences d'images PNG/JPEG, H264 tout-intra), decode chaque image en RGBA8,
//   l'uploade dans une texture backend (via le shell) et l'affiche en LETTERBOX
//   dans l'onglet. Transport : play/pause, stop, boucle, vitesse, +/- 1 pas.
//
//   Le decodage se fait DANS le rendu de l'onglet ACTIF (cadence au fps de la
//   video via ctx.input.dt) -> l'onglet inactif ne decode pas (pause naturelle).
//
//   Honnetete : MJPEG (AVI/MOV), sequences d'images et H264 TOUT-INTRA se lisent
//   entierement ; un MP4 H264 avec P/B-frames n'affiche que sa 1re image (le
//   decodeur H264 est intra). Le message le signale clairement.
// @Author  Rihen
// =============================================================================
#include "NKGui/NKGui.h"
#include "NKMedia/Video/NkVideoReader.h"
#include "NKEditorKit/NkEditorShell.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkFormat.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKCode/Project/NkCodeState.h" // OpenFile, StrEq

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu::nkgui;

		// ── Une video chargee (persistante tant que l'appli vit) ──────────────────
		struct NkVideoClip {
				NkString path;
				media::NkVideoReader *reader = nullptr; // decodeur (non copiable -> heap)
				media::NkVideoReaderInfo info;
				media::NkVideoFrame frame; // image courante decodee (RGBA8)
				bool opened = false, ok = false;
				bool openOk = false; // Open() a reussi (conteneur lisible) meme si aucune image decodable
				bool haveFrame = false;
				uint32 texId = 0;	 // texture backend (allouee au 1er upload, reutilisee)
				int32 texW = 0, texH = 0;
				float32 fps = 25.f;
				int32 frameCount = -1;
				int32 index = -1;	 // index de l'image affichee
				bool playing = false;
				bool loop = true;
				bool ended = false;	 // fin atteinte (pas de boucle)
				float32 speed = 1.f;
				float32 acc = 0.f;	 // accumulateur temporel (secondes)
				bool intraOnly = false; // ReadFrame s'est arrete tot (H264 P/B) -> 1re image seule
		};

		inline NkVector<NkVideoClip *> &NkVideoClips() {
			static NkVector<NkVideoClip *> v;
			return v;
		}

		// Registre par chemin (pointeurs stables : allocation dediee par video).
		inline NkVideoClip *NkVideoClipFor(const NkString &path) {
			auto &v = NkVideoClips();
			for (usize i = 0; i < v.Size(); ++i)
				if (StrEq(v[i]->path.CStr(), path.CStr()))
					return v[i];
			NkVideoClip *c = new NkVideoClip();
			c->path = path;
			v.PushBack(c);
			return c;
		}

		// Uploade `c->frame` dans la texture (alloue au 1er appel, met a jour ensuite).
		inline void NkVideoUpload(editorkit::NkEditorShell *shell, NkVideoClip *c) {
			if (!shell || c->frame.rgba.Empty() || c->frame.width <= 0 || c->frame.height <= 0)
				return;
			const uint8 *px = c->frame.rgba.Data();
			if (c->texId == 0 || c->texW != c->frame.width || c->texH != c->frame.height) {
				c->texId = shell->UploadRGBA(px, c->frame.width, c->frame.height);
				c->texW = c->frame.width;
				c->texH = c->frame.height;
			} else {
				shell->UpdateRGBA(c->texId, px, c->frame.width, c->frame.height);
			}
			c->haveFrame = c->texId != 0;
		}

		inline NkString NkVideoFmtTime(float32 s) { // m:ss
			if (s < 0.f)
				s = 0.f;
			const int32 t = (int32)(s + 0.0001f);
			return NkPrintf("%d:%02d", t / 60, t % 60);
		}
		inline NkString NkVideoFmtSec(int32 t) { // m:ss depuis des secondes ENTIERES
			if (t < 0)
				t = 0;
			return NkPrintf("%d:%02d", t / 60, t % 60);
		}

		// ── Viewer : decode + dessine l'image + transport dans `r` ────────────────
		inline void DrawVideoViewer(NkGuiContext &ctx, editorkit::NkEditorShell *shell, OpenFile &f, const NkRect &r) {
			NkGuiDrawList &dl = ctx.DL();
			const NkGuiFont *font = ctx.font;
			const float32 lh = (font && font->Valid()) ? font->LineHeight() : 16.f;
			const float32 asc = (font && font->Valid()) ? font->Ascent() : 12.f;
			const NkVec2 mp = ctx.input.mousePos;
			dl.PushClipRect(r, true);
			dl.AddRectFilled(r, NkColor{18, 20, 24, 255});

			auto centered = [&](const char *msg, const NkColor &col) {
				if (!font || !font->Valid())
					return;
				const float32 tw = font->MeasureWidth(msg);
				dl.AddText(font->Face(), font->TexId(), {r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f + asc}, msg,
						   col);
			};

			NkVideoClip *c = NkVideoClipFor(f.path.ToString());

			// ── Ouverture + 1re image UNE fois ──
			if (!c->opened) {
				c->opened = true;
				c->reader = new media::NkVideoReader();
				c->openOk = c->reader->Open(f.path.ToString().CStr());
				if (c->openOk) {
					c->info = c->reader->Info();
					c->fps = c->info.fps > 1.0 ? (float32)c->info.fps : 25.f;
					c->frameCount = c->info.frameCount;
					if (c->reader->ReadFrame(c->frame)) {
						c->index = c->frame.index; // index de l'image REELLEMENT decodee (cursor a deja avance)
						NkVideoUpload(shell, c);
						c->playing = false; // affiche la 1re image, attend le play de l'utilisateur
						c->acc = 0.f;
						c->ok = true;
					} else {
						c->ok = false; // conteneur lisible mais aucune image decodable (H264 inter)
					}
				}
			}
			if (!c->ok) {
				if (c->openOk) // ouvert : on connait le codec -> message precis + honnete
					centered(NkPrintf("Codec %s non decode (ex. H264 inter P/B) - image indisponible",
									  c->info.codec.CStr())
								 .CStr(),
							 NkColor{220, 170, 90, 255});
				else
					centered("Format video non pris en charge (MP4 fragmente / codec non gere)",
							 NkColor{240, 120, 110, 255});
				dl.PopClipRect();
				return;
			}

			// ── Zone image (au-dessus de la barre de transport) ──
			const float32 ctrlH = 46.f, infoH = lh + 10.f;
			const NkRect view = {r.x, r.y, r.w, r.h - ctrlH - infoH};

			// ── Avance temporelle (cadence au fps * vitesse) ──
			float32 dt = ctx.input.dt;
			if (dt <= 0.f || dt > 0.25f)
				dt = 1.f / 60.f; // garde-fou (onglet revenu au premier plan / hoquet)
			const float32 frameDur = 1.f / (c->fps * (c->speed > 0.01f ? c->speed : 0.01f));
			if (c->playing && !c->ended) {
				c->acc += dt;
				if (c->acc > frameDur * 4.f)
					c->acc = 0.f; // evite le rattrapage explosif
				while (c->acc >= frameDur) {
					c->acc -= frameDur;
					if (c->reader->ReadFrame(c->frame)) {
						c->index = c->frame.index; // index de l'image REELLEMENT decodee (cursor a deja avance)
						NkVideoUpload(shell, c);
					} else if (c->loop) {
						if (c->reader->SeekFrame(0) && c->reader->ReadFrame(c->frame)) {
							c->index = c->frame.index; // index de l'image REELLEMENT decodee (cursor a deja avance)
							NkVideoUpload(shell, c);
						} else {
							c->ended = true;
							if (c->index <= 0)
								c->intraOnly = true; // n'a jamais depasse la 1re image
							break;
						}
					} else {
						c->playing = false; // fin sans boucle -> fige la derniere image
						if (c->index <= 0)
							c->intraOnly = true;
						break;
					}
				}
			}

			// ── Dessine l'image (letterbox, ratio preserve) ──
			if (c->haveFrame && c->texId && c->texW > 0 && c->texH > 0) {
				const float32 sx = view.w / (float32)c->texW, sy = view.h / (float32)c->texH;
				const float32 s = sx < sy ? sx : sy;
				const float32 dw = c->texW * s, dh = c->texH * s;
				const NkRect disp = {view.x + (view.w - dw) * 0.5f, view.y + (view.h - dh) * 0.5f, dw, dh};
				dl.AddImage(c->texId, disp, {0.f, 0.f}, {1.f, 1.f}, NkColor{255, 255, 255, 255});
			} else {
				dl.PushClipRect(view, true);
				centered("...", ctx.theme.textDisabled);
				dl.PopClipRect();
			}

			// ── Barre de transport ──
			const float32 cy = r.y + r.h - infoH - ctrlH;
			const NkRect ctrl = {r.x, cy, r.w, ctrlH};
			dl.AddRectFilled(ctrl, NkColor{20, 22, 26, 255});
			const float32 by = ctrl.y + ctrlH * 0.5f;

			// Play / Pause (cercle).
			const float32 btnR = 15.f, bx = ctrl.x + 22.f + btnR;
			const NkVec2 bc = {bx, by};
			const bool overBtn = (mp.x - bx) * (mp.x - bx) + (mp.y - by) * (mp.y - by) <= btnR * btnR;
			dl.AddCircleFilled(bc, btnR, overBtn ? NkColor{15, 115, 213, 255} : NkColor{40, 46, 54, 255});
			if (c->playing) { // pause (deux barres)
				dl.AddRectFilled({bx - 5.f, by - 6.f, 3.5f, 12.f}, NkColor{235, 240, 245, 255});
				dl.AddRectFilled({bx + 2.f, by - 6.f, 3.5f, 12.f}, NkColor{235, 240, 245, 255});
			} else { // play (triangle)
				dl.AddTriangleFilled({bx - 4.f, by - 6.f}, {bx - 4.f, by + 6.f}, {bx + 7.f, by},
									 NkColor{235, 240, 245, 255});
			}
			if (overBtn && ctx.input.mouseClicked[0]) {
				if (c->ended) { // relance depuis le debut
					if (c->reader->SeekFrame(0) && c->reader->ReadFrame(c->frame)) {
						c->index = c->frame.index; // index de l'image REELLEMENT decodee (cursor a deja avance)
						NkVideoUpload(shell, c);
					}
					c->ended = false;
					c->acc = 0.f;
					c->playing = true;
				} else
					c->playing = !c->playing;
			}

			// Stop (retour a la 1re image, en pause).
			const NkRect stopB = {bx + btnR + 14.f, by - 9.f, 18.f, 18.f};
			const bool overStop = NkGuiRectContains(stopB, mp);
			dl.AddRectFilled({stopB.x + 3.f, stopB.y + 3.f, 12.f, 12.f},
							 overStop ? NkColor{200, 90, 90, 255} : NkColor{120, 128, 138, 255}, 2.f);
			if (overStop && ctx.input.mouseClicked[0]) {
				if (c->reader->SeekFrame(0) && c->reader->ReadFrame(c->frame)) {
					c->index = c->frame.index; // index de l'image REELLEMENT decodee (cursor a deja avance)
					NkVideoUpload(shell, c);
				}
				c->playing = false;
				c->ended = false;
				c->acc = 0.f;
			}

			// Boucle (toggle).
			const NkRect loopB = {stopB.x + stopB.w + 12.f, by - 10.f, 20.f, 20.f};
			const bool overLoop = NkGuiRectContains(loopB, mp);
			dl.AddRectFilled(loopB, c->loop ? NkColor{15, 115, 213, 255}
											: (overLoop ? NkColor{54, 60, 70, 255} : NkColor{40, 46, 54, 255}), 4.f);
			{
				const NkColor ic = c->loop ? NkColor{240, 245, 250, 255} : NkColor{150, 158, 168, 255};
				dl.AddRect({loopB.x + 4.f, loopB.y + 6.f, 10.f, 8.f}, ic, 1.5f);
				dl.AddTriangleFilled({loopB.x + 12.f, loopB.y + 3.f}, {loopB.x + 12.f, loopB.y + 9.f},
									 {loopB.x + 17.f, loopB.y + 6.f}, ic);
			}
			if (overLoop && ctx.input.mouseClicked[0])
				c->loop = !c->loop;

			// Temps ecoule / total / restant + image courante (l'index avance a chaque frame decodee).
			// On travaille en SECONDES ENTIERES et on derive le restant = total - ecoule, sinon les
			// arrondis independants (floor) desynchronisent l'affichage (2.6 + 3.4 -> 2 + 3 != 6).
			if (font && font->Valid()) {
				const float32 elapsed = c->fps > 0.f ? (float32)c->index / c->fps : 0.f;
				NkString tt;
				if (c->frameCount > 0 && c->fps > 0.f) {
					const int32 totalS = (int32)((float32)c->frameCount / c->fps + 0.5f);
					int32 elapS = (int32)(elapsed + 0.5f);
					if (elapS > totalS)
						elapS = totalS;
					const int32 remainS = totalS - elapS;
					tt = NkPrintf("%s / %s   -%s   img %d/%d", NkVideoFmtSec(elapS).CStr(),
								  NkVideoFmtSec(totalS).CStr(), NkVideoFmtSec(remainS).CStr(), c->index + 1,
								  c->frameCount);
				} else {
					tt = NkPrintf("%s   img %d", NkVideoFmtSec((int32)(elapsed + 0.5f)).CStr(), c->index + 1);
				}
				dl.AddText(font->Face(), font->TexId(), {loopB.x + loopB.w + 14.f, by - lh * 0.5f + asc}, tt.CStr(),
						   ctx.theme.text);
			}

			// Pas precedent / suivant (seek d'un pas ~1s).
			const int32 step = (int32)(c->fps + 0.5f);
			auto seekTo = [&](int32 tgt) {
				if (c->frameCount > 0) {
					if (tgt < 0)
						tgt = 0;
					if (tgt > c->frameCount - 1)
						tgt = c->frameCount - 1;
				} else if (tgt < 0)
					tgt = 0;
				if (c->reader->SeekFrame(tgt) && c->reader->ReadFrame(c->frame)) {
					c->index = c->frame.index; // index de l'image REELLEMENT decodee (cursor a deja avance)
					NkVideoUpload(shell, c);
					c->ended = false;
				}
			};
			if (font && font->Valid()) {
				const float32 px0 = loopB.x + loopB.w + 232.f;
				const NkRect prevB = {px0, by - 11.f, 24.f, 22.f};
				const NkRect nextB = {px0 + 30.f, by - 11.f, 24.f, 22.f};
				const bool oPrev = NkGuiRectContains(prevB, mp), oNext = NkGuiRectContains(nextB, mp);
				dl.AddRectFilled(prevB, oPrev ? NkColor{54, 60, 70, 255} : NkColor{40, 46, 54, 255}, 4.f);
				dl.AddRectFilled(nextB, oNext ? NkColor{54, 60, 70, 255} : NkColor{40, 46, 54, 255}, 4.f);
				const NkColor ac = NkColor{220, 226, 234, 255};
				// |< et >|
				dl.AddRectFilled({prevB.x + 6.f, prevB.y + 5.f, 2.f, 12.f}, ac);
				dl.AddTriangleFilled({prevB.x + 17.f, prevB.y + 5.f}, {prevB.x + 17.f, prevB.y + 17.f},
									 {prevB.x + 10.f, prevB.y + 11.f}, ac);
				dl.AddTriangleFilled({nextB.x + 7.f, nextB.y + 5.f}, {nextB.x + 7.f, nextB.y + 17.f},
									 {nextB.x + 14.f, nextB.y + 11.f}, ac);
				dl.AddRectFilled({nextB.x + 16.f, nextB.y + 5.f, 2.f, 12.f}, ac);
				if (oPrev && ctx.input.mouseClicked[0])
					seekTo(c->index - step);
				if (oNext && ctx.input.mouseClicked[0])
					seekTo(c->index + step);
			}

			// Vitesse : [-] x.xx [+].
			if (font && font->Valid()) {
				const float32 sx0 = loopB.x + loopB.w + 328.f;
				const NkRect sMinus = {sx0, by - 11.f, 20.f, 22.f};
				const NkRect sPlus = {sx0 + 76.f, by - 11.f, 20.f, 22.f};
				const bool oM = NkGuiRectContains(sMinus, mp), oP = NkGuiRectContains(sPlus, mp);
				dl.AddRectFilled(sMinus, oM ? NkColor{54, 60, 70, 255} : NkColor{40, 46, 54, 255}, 4.f);
				dl.AddRectFilled(sPlus, oP ? NkColor{54, 60, 70, 255} : NkColor{40, 46, 54, 255}, 4.f);
				dl.AddText(font->Face(), font->TexId(),
						   {sMinus.x + 10.f - font->MeasureWidth("-") * 0.5f, by - lh * 0.5f + asc}, "-", ctx.theme.text);
				dl.AddText(font->Face(), font->TexId(),
						   {sPlus.x + 10.f - font->MeasureWidth("+") * 0.5f, by - lh * 0.5f + asc}, "+", ctx.theme.text);
				dl.AddText(font->Face(), font->TexId(), {sMinus.x + 27.f, by - lh * 0.5f + asc},
						   NkPrintf("%.2fx", c->speed).CStr(), ctx.theme.text);
				if (oM && ctx.input.mouseClicked[0])
					c->speed = c->speed / 1.25f < 0.1f ? 0.1f : c->speed / 1.25f;
				if (oP && ctx.input.mouseClicked[0])
					c->speed = c->speed * 1.25f > 4.0f ? 4.0f : c->speed * 1.25f;
			}

			// Note d'honnetete : H264 P/B non decode au-dela de la 1re image.
			if (c->intraOnly && font && font->Valid()) {
				const char *w = "H264 inter (P/B) non decode : 1re image seule";
				dl.AddText(font->Face(), font->TexId(),
						   {ctrl.x + ctrl.w - font->MeasureWidth(w) - 16.f, by - lh * 0.5f + asc}, w,
						   NkColor{220, 170, 90, 255});
			}

			// ── Barre d'info (nom, format, dims, fps) ──
			if (font && font->Valid()) {
				const NkRect bar = {r.x, r.y + r.h - infoH, r.w, infoH};
				dl.AddRectFilled(bar, NkColor{16, 18, 22, 235});
				const NkString info =
					NkPrintf("%s      %s/%s  %d x %d  %.0f fps      %d Ko", f.Name().CStr(), c->info.container.CStr(),
							 c->info.codec.CStr(), c->info.width, c->info.height, (double)c->fps,
							 (int32)(f.mediaSize / 1024));
				dl.AddText(font->Face(), font->TexId(), {bar.x + 12.f, bar.y + 5.f + asc}, info.CStr(), ctx.theme.text);
			}
			dl.PopClipRect();
		}

	} // namespace nkcode
} // namespace nkentseu
