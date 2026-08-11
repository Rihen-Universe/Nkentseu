// ============================================================================
// main.cpp — NkRef, Étape 0 : le canevas infini NU.
// ----------------------------------------------------------------------------
// GLUE fenêtre + boucle + entrées + grille. La caméra 2D vit dans NkRefView.h
// (pure) : c'est elle qu'on garde, ce fichier restera mince. Aucune image à ce
// stade — l'Étape 0 se limite, volontairement, à « se promener dans le vide » :
// pan (clic MILIEU, ou ESPACE + glisser gauche), zoom molette CENTRÉ SOUS LE
// CURSEUR, grille adaptative, Début (Home) = retour à l'origine.
//
// Le rendu est fait en ESPACE ÉCRAN (la vue transforme les coordonnées
// elle-même, pas de SetView) : pour une grille de lignes c'est équivalent, et
// ça ne dépend pas du comportement de la vue du backend — on adoptera SetView
// à l'Étape 1 (sprites), quand on pourra le VÉRIFIER à l'œil sur les images.
// ============================================================================

#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEvent/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKLogger/NkLog.h"
#include "NKTime/NkTime.h"

#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"

// Captures agent : dossier + numérotation + photographie de la fenêtre.
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKImage/NKImage.h"
#if defined(NKENTSEU_PLATFORM_WINDOWS)
#include <windows.h>
#endif

#include "NkRef/NkRefView.h"

#include <cstdio>  // snprintf (chemins, titre — glue, OK)
#include <cstdlib> // getenv/atoi (crochets d'agent, comme le modeleur)

using namespace nkentseu;
using namespace nkentseu::renderer;

namespace {

	// ── CAPTURES ────────────────────────────────────────────────────────────
	// Premier chemin LIBRE captures/<prefixe>_NNN.png : numérotation simple,
	// sans horloge — l'ordre des fichiers EST l'ordre des prises (même
	// convention que le modeleur).
	bool NkNextCapturePath(const char *prefix, char *out, int32 cap) {
		NkDirectory::CreateRecursive("captures");
		for (int32 i = 1; i < 1000; ++i) {
			std::snprintf(out, (size_t)cap, "captures/%s_%03d.png", prefix, (int)i);
			if (!NkFile::Exists(out))
				return true;
		}
		return false;
	}

#if defined(NKENTSEU_PLATFORM_WINDOWS)
	// Photographie de TOUTE la fenêtre par l'OS (PrintWindow) : indépendante du
	// backend graphique — NkRenderWindow::Capture ne lit que DX11 et nous
	// tournons en OpenGL. Recopié du modeleur (le chemin qui a fait ses preuves).
	bool NkCaptureWholeWindow(NkWindow &win, const char *path) {
		const NkSurfaceDesc sd = win.GetSurfaceDesc();
		HWND hwnd = sd.hwnd;
		if (!hwnd)
			return false;
		RECT rc{};
		if (!GetWindowRect(hwnd, &rc))
			return false;
		const int32 w = rc.right - rc.left, h = rc.bottom - rc.top;
		if (w <= 0 || h <= 0)
			return false;
		HDC hdcWin = GetWindowDC(hwnd);
		HDC hdcMem = CreateCompatibleDC(hdcWin);
		BITMAPINFO bi{};
		bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi.bmiHeader.biWidth = w;
		bi.bmiHeader.biHeight = -h; // négatif = origine en HAUT (ordre des lignes PNG)
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;
		void *bits = nullptr;
		HBITMAP hbmp = CreateDIBSection(hdcMem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
		bool ok = false;
		if (hbmp) {
			HGDIOBJ old = SelectObject(hdcMem, hbmp);
			ok = PrintWindow(hwnd, hdcMem, 2 /*PW_RENDERFULLCONTENT*/) != 0;
			if (!ok)
				ok = BitBlt(hdcMem, 0, 0, w, h, hdcWin, 0, 0, SRCCOPY | CAPTUREBLT) != 0;
			if (ok && bits) {
				NkImage img;
				ok = img.Create((uint32)w, (uint32)h, math::NkColor(0, 0, 0, 255), 4);
				if (ok) {
					const uint8 *src = (const uint8 *)bits;
					uint8 *dst = img.Pixels();
					for (int32 i = 0; i < w * h; ++i) { // BGRA -> RGBA, alpha opaque
						dst[i * 4 + 0] = src[i * 4 + 2];
						dst[i * 4 + 1] = src[i * 4 + 1];
						dst[i * 4 + 2] = src[i * 4 + 0];
						dst[i * 4 + 3] = 255;
					}
					ok = img.Save(path);
				}
			}
			SelectObject(hdcMem, old);
			DeleteObject(hbmp);
		}
		DeleteDC(hdcMem);
		ReleaseDC(hwnd, hdcWin);
		return ok;
	}
#else
	bool NkCaptureWholeWindow(NkWindow &, const char *) {
		std::printf("[NkRef] capture : pas encore portee sur cette plateforme\n");
		return false;
	}
#endif

	// ── CROCHETS D'AGENT ────────────────────────────────────────────────────
	// Même doctrine que le modeleur : les crochets n'ARMENT que ce que
	// l'interface arme déjà — NK_AGENT_PAN/ZOOM appellent les MÊMES méthodes
	// de NkRefView que la souris. Si personne ne les pose, rien ne change.
	//   NK_AGENT_SHOT="n[,n2,...]"        captures aux frames n, n2… (fenêtre entière)
	//   NK_AGENT_EXIT=n                   sortie propre à la frame >= n
	//   NK_AGENT_PAN="n:dx:dy"            à la frame n, pan de (dx,dy) pixels
	//   NK_AGENT_ZOOM="n:facteur:px:py"   à la frame n, zoom au pixel (px,py)
	struct NkAgentHooks {
			static constexpr int32 kMaxShots = 8;
			int32 shotFrames[kMaxShots] = {};
			int32 shotCount = 0;
			int32 exitFrame = -1;
			int32 panFrame = -1;
			float32 panDx = 0.0f, panDy = 0.0f;
			int32 zoomFrame = -1;
			float32 zoomFactor = 1.0f, zoomPx = 0.0f, zoomPy = 0.0f;

			void Read() {
				if (const char *v = std::getenv("NK_AGENT_SHOT")) {
					// liste « 10,30,50 » — plusieurs prises pour comparer
					// avant/après pan et zoom par diff pixel.
					const char *p = v;
					while (*p && shotCount < kMaxShots) {
						shotFrames[shotCount++] = (int32)std::atoi(p);
						while (*p && *p != ',')
							++p;
						if (*p == ',')
							++p;
					}
				}
				if (const char *v = std::getenv("NK_AGENT_EXIT"))
					exitFrame = (int32)std::atoi(v);
				if (const char *v = std::getenv("NK_AGENT_PAN")) {
					int f = 0;
					float dx = 0, dy = 0;
					if (std::sscanf(v, "%d:%f:%f", &f, &dx, &dy) == 3) {
						panFrame = (int32)f;
						panDx = (float32)dx;
						panDy = (float32)dy;
					}
				}
				if (const char *v = std::getenv("NK_AGENT_ZOOM")) {
					int f = 0;
					float fac = 1, px = 0, py = 0;
					if (std::sscanf(v, "%d:%f:%f:%f", &f, &fac, &px, &py) == 4) {
						zoomFrame = (int32)f;
						zoomFactor = (float32)fac;
						zoomPx = (float32)px;
						zoomPy = (float32)py;
					}
				}
			}
	};

} // namespace

NKENTSEU_DEFINE_APP_DATA(([]() {
	NkAppData d{};
	d.appName = "NkRef";
	d.appVersion = "0.1.0";
	return d;
})());

int nkmain(const NkEntryState &state) {
	(void)state;

	NkWindowConfig cfg;
	cfg.title = "NkRef";
	cfg.width = 1280;
	cfg.height = 800;
	cfg.centered = true;
	cfg.resizable = true;
	NkWindow window(cfg);
	if (!window.IsOpen()) {
		logger.Error("[NkRef] creation fenetre echouee");
		return -1;
	}

	NkContextDesc desc;
	desc.api = NkGraphicsApi::NK_GFX_API_OPENGL; // seul backend Renderer2D validé à ce jour
	NkRenderWindow target(window, desc);
	if (!target.IsValid()) {
		logger.Error("[NkRef] cible de rendu invalide");
		return -2;
	}

	nkref::NkRefView view;

	NkAgentHooks agent;
	agent.Read();
	int32 agentFrame = 0;

	auto &events = NkEvents();
	math::NkVec2u lastSize = target.GetSize();

	bool panning = false;	   // pan en cours (milieu, ou espace+gauche)
	bool homeWasDown = false;  // front montant de la touche Début
	int32 titleCooldown = 0;   // maj du titre 1 frame sur 15 (pas chaque frame)
	bool running = true;

	while (running && window.IsOpen()) {
		// ── Événements ──────────────────────────────────────────────────────
		const bool spaceDown = events.GetInputState().keyboard.IsKeyPressed(NkKey::NK_SPACE);

		while (NkEvent *ev = events.PollEvent()) {
			if (ev->Is<NkWindowCloseEvent>()) {
				running = false;
				break;
			}

			if (auto *mw = ev->As<NkMouseWheelVerticalEvent>()) {
				// La molette porte SA position : c'est elle l'ancre du zoom.
				const math::NkVec2u sz = target.GetSize();
				const float32 factor = math::NkPow(1.15f, (float32)mw->GetDeltaY());
				view.ZoomAtPixel(factor, {(float32)mw->GetX(), (float32)mw->GetY()},
								 {(float32)sz.x, (float32)sz.y});
			}

			if (auto *mb = ev->As<NkMouseButtonPressEvent>()) {
				if (mb->IsMiddle() || (mb->IsLeft() && spaceDown))
					panning = true;
			}
			if (auto *mr = ev->As<NkMouseButtonReleaseEvent>()) {
				if (mr->IsMiddle() || mr->IsLeft())
					panning = false;
			}

			if (auto *mm = ev->As<NkMouseMoveEvent>()) {
				if (panning) // deltas déjà calculés par NKEvent
					view.PanByPixels((float32)mm->GetDeltaX(), (float32)mm->GetDeltaY());
			}
		}
		if (!running || !window.IsOpen())
			break;

		// Début (Home) = revenir à l'origine (front montant seulement).
		const bool homeDown = events.GetInputState().keyboard.IsKeyPressed(NkKey::NK_HOME);
		if (homeDown && !homeWasDown)
			view.Reset();
		homeWasDown = homeDown;

		// ── Resize (garde-fou WM_SIZE au démarrage, piège documenté) ────────
		const math::NkVec2u sz = target.GetSize();
		if (sz.x != lastSize.x || sz.y != lastSize.y) {
			if (lastSize.x != 0 && sz.x > 0 && sz.y > 0)
				target.OnResize(sz.x, sz.y);
			lastSize = sz;
		}

		// ── Crochets d'agent : mêmes méthodes que la souris ─────────────────
		const math::NkVec2f vp{(float32)sz.x, (float32)sz.y};
		if (agent.panFrame > 0 && agentFrame == agent.panFrame)
			view.PanByPixels(agent.panDx, agent.panDy);
		if (agent.zoomFrame > 0 && agentFrame == agent.zoomFrame)
			view.ZoomAtPixel(agent.zoomFactor, {agent.zoomPx, agent.zoomPy}, vp);

		// ── Rendu : fond + grille adaptative + axes, en espace écran ────────
		target.Clear(NkColor2D{20, 22, 25, 255});
		NkRenderer2D &r = target.GetRenderer2D();

		const float32 spacing = view.GridSpacing(32.0f); // ≥ 32 px entre lignes
		const math::NkVec2f wMin = view.PixelToWorld({0.0f, 0.0f}, vp);
		const math::NkVec2f wMax = view.PixelToWorld(vp, vp);
		const NkColor2D minor{255, 255, 255, 10};
		const NkColor2D major{255, 255, 255, 26};

		// Verticales. Index ENTIER pour décider mineure/majeure (1 sur 8) :
		// un fmod sur float dériverait loin de l'origine.
		const int64 ix0 = (int64)math::NkFloor(wMin.x / spacing);
		const int64 ix1 = (int64)math::NkCeil(wMax.x / spacing);
		for (int64 i = ix0; i <= ix1; ++i) {
			const float32 xPix = view.WorldToPixel({(float32)i * spacing, 0.0f}, vp).x;
			const bool isMajor = (i % 8) == 0;
			r.DrawLine({xPix, 0.0f}, {xPix, vp.y}, isMajor ? major : minor, 1.0f);
		}
		// Horizontales.
		const int64 iy0 = (int64)math::NkFloor(wMin.y / spacing);
		const int64 iy1 = (int64)math::NkCeil(wMax.y / spacing);
		for (int64 i = iy0; i <= iy1; ++i) {
			const float32 yPix = view.WorldToPixel({0.0f, (float32)i * spacing}, vp).y;
			const bool isMajor = (i % 8) == 0;
			r.DrawLine({0.0f, yPix}, {vp.x, yPix}, isMajor ? major : minor, 1.0f);
		}

		// Les axes de l'origine, en orange discret (repère de la planche vide) :
		// c'est le seul point fixe du monde — sans lui, impossible de « rentrer ».
		const math::NkVec2f origin = view.WorldToPixel({0.0f, 0.0f}, vp);
		const NkColor2D axis{247, 154, 40, 70};
		if (origin.x >= 0.0f && origin.x <= vp.x)
			r.DrawLine({origin.x, 0.0f}, {origin.x, vp.y}, axis, 1.0f);
		if (origin.y >= 0.0f && origin.y <= vp.y)
			r.DrawLine({0.0f, origin.y}, {vp.x, origin.y}, axis, 1.0f);

		target.Display();

		// Titre = zoom courant (pas de police à l'Étape 0, le titre suffit).
		if (--titleCooldown <= 0) {
			char t[64];
			std::snprintf(t, sizeof(t), "NkRef - %d%%", (int)(view.zoom * 100.0f + 0.5f));
			window.SetTitle(t);
			titleCooldown = 15;
		}

		// ── Crochets d'agent : capture et sortie à la frame demandée ────────
		++agentFrame;
		for (int32 s = 0; s < agent.shotCount; ++s) {
			if (agent.shotFrames[s] > 0 && agentFrame == agent.shotFrames[s]) {
				char capPath[256];
				if (NkNextCapturePath("nkref", capPath, (int32)sizeof(capPath))) {
					const bool okCap = NkCaptureWholeWindow(window, capPath);
					std::printf("[NkRef] capture -> %s : %s\n", capPath, okCap ? "ecrite" : "ECHEC");
				}
			}
		}
		if (agent.exitFrame > 0 && agentFrame >= agent.exitFrame)
			running = false; // >= : même si une frame saute, on sort toujours

		NkChrono::Sleep(8.0); // ~120 Hz max — le canevas vide n'a pas besoin de plus
	}

	// PAS de window.Close() explicite ici : `target` (déclaré APRÈS `window`)
	// doit mourir AVANT la fenêtre — glXDestroyContext sur un Display X11 déjà
	// fermé segfaultait sous WSLg (backtrace _XSend/XInitExtension, vécu le
	// 2026-08-11). L'ordre de destruction C++ inverse fait exactement ça, et
	// ~NkWindow appelle Close() lui-même.
	return 0;
}
