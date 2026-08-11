// ============================================================================
// main.cpp — NkRef, Étape 1 : des IMAGES sur la planche.
// ----------------------------------------------------------------------------
// GLUE fenêtre + boucle + entrées + rendu. Le modèle vit dans NkRefBoard.h
// (pur), la caméra dans NkRefView.h (pure) : c'est eux qu'on garde.
//
// Ce que fait l'Étape 1 (et rien de plus) :
//   - glisser-déposer des fichiers image → NkSprite posé au point de dépôt ;
//   - Ctrl+V : colle l'image du presse-papiers (GetClipboardImage, chantier
//     NKWindow de cette branche) sous le curseur ;
//   - clic = sélection (Ctrl = ajouter), glisser = déplacer la sélection,
//     glisser sur le vide = rectangle de sélection ;
//   - poignées d'échelle aux coins (ancre = coin opposé, échelle UNIFORME),
//     poignée de rotation au-dessus (Maj = crans de 15°) ;
//   - X / Y = miroir, Suppr = supprimer, PgUp/PgDn ou Ctrl+molette = ordre Z ;
//   - pan (clic milieu, Espace+glisser), zoom molette centré curseur, grille
//     adaptative, Début = origine — l'Étape 0 inchangée.
//
// Rendu en ESPACE ÉCRAN (la vue transforme elle-même, pas de SetView) : les
// sprites reçoivent position/échelle/rotation en pixels — un seul chemin de
// transformation, vérifiable au pixel sur les captures.
//
// L'ordre du tableau d'items EST l'ordre de profondeur ; la glue tient un
// tableau de NkTexture* ALIGNÉ SUR LES INDEX — toute opération qui réordonne
// ou supprime applique le même mouvement aux deux (cf. NkRefBoard.h).
// ============================================================================

#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEvent/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkDropEvent.h"
#include "NKLogger/NkLog.h"
#include "NKTime/NkTime.h"

#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Resources/NkTexture.h"
#include "NKCanvas/Renderer/Resources/NkSprite.h"

// Captures agent : dossier + numérotation + photographie de la fenêtre.
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKImage/NKImage.h"
#if defined(NKENTSEU_PLATFORM_WINDOWS)
#include <windows.h>
#endif

#include "NkRef/NkRefView.h"
#include "NkRef/NkRefBoard.h"

#include <cstdio>  // snprintf (chemins, titre — glue, OK)
#include <cstdlib> // getenv/atoi (crochets d'agent, comme le modeleur)
#include <cstring> // strchr/strncpy (parsing des crochets)

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
	// l'interface arme déjà — DROP/CLICK/MOVE appellent les MÊMES fonctions
	// que le glisser-déposer et la souris. Si personne ne les pose, rien ne
	// change.
	//   NK_AGENT_SHOT="n[,n...]"                  captures aux frames n…
	//   NK_AGENT_EXIT=n                           sortie propre à la frame >= n
	//   NK_AGENT_PAN="n:dx:dy"                    pan de (dx,dy) pixels
	//   NK_AGENT_ZOOM="n:facteur:px:py"           zoom au pixel (px,py)
	//   NK_AGENT_DROP="n:px:py:chemin[;...]"      dépose une image au pixel
	//   NK_AGENT_CLICK="n:px:py[;...]"            clic gauche de sélection
	//   NK_AGENT_MOVE="n:dx:dy"                   déplace la sélection (pixels)
	struct NkAgentHooks {
			static constexpr int32 kMax = 8;
			int32 shotFrames[kMax] = {};
			int32 shotCount = 0;
			int32 exitFrame = -1;
			int32 panFrame = -1;
			float32 panDx = 0.0f, panDy = 0.0f;
			int32 zoomFrame = -1;
			float32 zoomFactor = 1.0f, zoomPx = 0.0f, zoomPy = 0.0f;

			struct Drop {
					int32 frame = -1;
					float32 px = 0.0f, py = 0.0f;
					char path[260] = {};
			};
			Drop drops[kMax];
			int32 dropCount = 0;

			struct Click {
					int32 frame = -1;
					float32 px = 0.0f, py = 0.0f;
			};
			Click clicks[kMax];
			int32 clickCount = 0;

			int32 moveFrame = -1;
			float32 moveDx = 0.0f, moveDy = 0.0f;

			void Read() {
				if (const char *v = std::getenv("NK_AGENT_SHOT")) {
					const char *p = v;
					while (*p && shotCount < kMax) {
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
				// Le CHEMIN peut contenir des ':' (D:\...) : on coupe NOUS-MÊMES
				// après le 3e deux-points, sscanf s'y tromperait.
				if (const char *v = std::getenv("NK_AGENT_DROP")) {
					const char *tok = v;
					while (tok && *tok && dropCount < kMax) {
						const char *end = std::strchr(tok, ';');
						Drop &d = drops[dropCount];
						int f = 0;
						float px = 0, py = 0;
						const char *c1 = std::strchr(tok, ':');
						const char *c2 = c1 ? std::strchr(c1 + 1, ':') : nullptr;
						const char *c3 = c2 ? std::strchr(c2 + 1, ':') : nullptr;
						if (c3 && std::sscanf(tok, "%d:%f:%f", &f, &px, &py) == 3) {
							d.frame = (int32)f;
							d.px = (float32)px;
							d.py = (float32)py;
							const char *p0 = c3 + 1;
							const usize len = end ? (usize)(end - p0) : std::strlen(p0);
							const usize n = len < sizeof(d.path) - 1 ? len : sizeof(d.path) - 1;
							std::memcpy(d.path, p0, n);
							d.path[n] = '\0';
							++dropCount;
						}
						tok = end ? end + 1 : nullptr;
					}
				}
				if (const char *v = std::getenv("NK_AGENT_CLICK")) {
					const char *tok = v;
					while (tok && *tok && clickCount < kMax) {
						const char *end = std::strchr(tok, ';');
						int f = 0;
						float px = 0, py = 0;
						if (std::sscanf(tok, "%d:%f:%f", &f, &px, &py) == 3) {
							clicks[clickCount].frame = (int32)f;
							clicks[clickCount].px = (float32)px;
							clicks[clickCount].py = (float32)py;
							++clickCount;
						}
						tok = end ? end + 1 : nullptr;
					}
				}
				if (const char *v = std::getenv("NK_AGENT_MOVE")) {
					int f = 0;
					float dx = 0, dy = 0;
					if (std::sscanf(v, "%d:%f:%f", &f, &dx, &dy) == 3) {
						moveFrame = (int32)f;
						moveDx = (float32)dx;
						moveDy = (float32)dy;
					}
				}
			}
	};

	// Ce que la souris est en train de faire. Un seul mode à la fois : c'est
	// ce qui rend le canevas prévisible (pas de drag+zoom+rect simultanés).
	enum class NkMode { Idle, Pan, DragItems, ScaleCorner, Rotate, RectSelect };

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
	cfg.dropEnabled = true; // glisser-déposer OLE complet (Enter/Leave/File)
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
	nkref::NkRefBoard board;
	// Textures GPU, INDEX ALIGNÉ sur board.items (cf. en-tête du fichier).
	NkVector<NkTexture *> textures;
	auto &alloc = memory::NkGetDefaultAllocator();

	NkAgentHooks agent;
	agent.Read();
	int32 agentFrame = 0;

	auto &events = NkEvents();
	math::NkVec2u lastSize = target.GetSize();

	NkMode mode = NkMode::Idle;
	math::NkVec2f mousePix{640.0f, 400.0f}; // dernière position curseur (client)
	math::NkVec2f rectStartPix{0.0f, 0.0f}; // départ du rectangle de sélection
	bool rectAdditive = false;
	int32 grabCorner = -1;					 // coin d'échelle saisi (0..3)
	math::NkVec2f grabAnchorWorld{0, 0};	 // coin OPPOSÉ (ancre d'échelle)
	math::NkVec2f grabCenterWorld{0, 0};	 // centre de l'item à la prise
	float32 grabScale = 1.0f;				 // échelle à la prise
	float32 grabDist = 1.0f;				 // distance ancre→souris à la prise
	float32 grabRotDeg = 0.0f;				 // rotation à la prise
	float32 grabAngleDeg = 0.0f;			 // angle centre→souris à la prise
	bool homeWasDown = false;
	int32 titleCooldown = 0;
	bool running = true;

	// ── Les ACTIONS, partagées entre la souris et les crochets d'agent ──────
	// (un seul chemin : le crochet appelle exactement ce que l'événement appelle)

	// Charge une image et la pose centrée sur `world`. Retourne l'index (-1 si échec).
	auto addImageFromFile = [&](const char *path, const math::NkVec2f &world) -> int32 {
		NkImage img;
		if (!img.Load(path)) {
			logger.Warn("[NkRef] image illisible : %s", path);
			return -1;
		}
		NkTexture *tex = alloc.New<NkTexture>();
		if (!tex || !tex->LoadFromImage(*target.GetRenderer(), img)) {
			logger.Warn("[NkRef] upload GPU echoue : %s", path);
			if (tex)
				alloc.Delete(tex);
			return -1;
		}
		board.AddItem(world, img.Width(), img.Height(), NkString(path));
		textures.PushBack(tex);
		logger.Info("[NkRef] image posee : %s (%ux%u)", path, img.Width(), img.Height());
		return (int32)board.items.Size() - 1;
	};

	// Colle l'image du presse-papiers centrée sur `world`.
	auto pasteClipboard = [&](const math::NkVec2f &world) {
		NkClipboardImage clip;
		if (!window.GetClipboardImage(clip)) {
			logger.Info("[NkRef] presse-papiers : pas d'image");
			return;
		}
		NkImage img;
		if (!img.Create(clip.width, clip.height, math::NkColor(0, 0, 0, 0), 4))
			return;
		uint8 *dst = img.Pixels();
		const uint8 *src = clip.pixels.Data();
		const usize total = (usize)clip.width * clip.height * 4u;
		for (usize i = 0; i < total; ++i)
			dst[i] = src[i];
		NkTexture *tex = alloc.New<NkTexture>();
		if (!tex || !tex->LoadFromImage(*target.GetRenderer(), img)) {
			if (tex)
				alloc.Delete(tex);
			return;
		}
		board.AddItem(world, clip.width, clip.height, NkString());
		textures.PushBack(tex);
		logger.Info("[NkRef] collage presse-papiers : %ux%u", clip.width, clip.height);
	};

	// Clic gauche de sélection au pixel (px,py) — la partie « choisir » du
	// press ; le démarrage du drag reste dans le gestionnaire souris.
	auto selectAtPixel = [&](float32 px, float32 py, bool additive) -> int32 {
		const math::NkVec2u sz = target.GetSize();
		const math::NkVec2f w = view.PixelToWorld({px, py}, {(float32)sz.x, (float32)sz.y});
		const int32 hit = board.HitTest(w);
		if (hit >= 0) {
			// Re-cliquer un item DÉJÀ sélectionné sans Ctrl ne désélectionne
			// pas les autres : c'est ce qui permet de déplacer un groupe.
			if (!board.items[(usize)hit].selected)
				board.Select(hit, additive);
			else
				board.active = hit;
		} else if (!additive) {
			board.ClearSelection();
		}
		return hit;
	};

	auto moveSelectionByPixels = [&](float32 dx, float32 dy) {
		board.MoveSelected({dx / view.zoom, dy / view.zoom});
	};

	auto removeSelected = [&]() {
		NkVector<nkref::int32> removed;
		board.RemoveSelected(removed);
		for (int32 k = (int32)removed.Size() - 1; k >= 0; --k) {
			const usize idx = (usize)removed[(usize)k];
			alloc.Delete(textures[idx]);
			textures.RemoveAt(idx);
		}
	};

	auto reorderActive = [&](bool up) {
		const int32 old = board.active;
		const int32 other = board.RaiseActive(up);
		if (other >= 0 && old >= 0) {
			NkTexture *tmp = textures[(usize)old];
			textures[(usize)old] = textures[(usize)other];
			textures[(usize)other] = tmp;
		}
	};

	// Position pixel de la poignée de rotation de l'item actif (au-dessus du
	// milieu du bord haut, à 26 px écran — constant quel que soit le zoom).
	auto rotationHandlePix = [&](const nkref::NkRefItem &it, const math::NkVec2f &vp) -> math::NkVec2f {
		math::NkVec2f c[4];
		nkref::NkRefBoard::Corners(it, c);
		const math::NkVec2f topMid = view.WorldToPixel({(c[0].x + c[1].x) * 0.5f, (c[0].y + c[1].y) * 0.5f}, vp);
		const math::NkVec2f center = view.WorldToPixel(it.pos, vp);
		math::NkVec2f dir{topMid.x - center.x, topMid.y - center.y};
		const float32 len = math::NkSqrt(dir.x * dir.x + dir.y * dir.y);
		if (len > 0.001f) {
			dir.x /= len;
			dir.y /= len;
		} else {
			dir = {0.0f, -1.0f};
		}
		return {topMid.x + dir.x * 26.0f, topMid.y + dir.y * 26.0f};
	};

	while (running && window.IsOpen()) {
		const bool spaceDown = events.GetInputState().keyboard.IsKeyPressed(NkKey::NK_SPACE);
		const math::NkVec2u szNow = target.GetSize();
		const math::NkVec2f vp{(float32)szNow.x, (float32)szNow.y};

		// ── Événements ──────────────────────────────────────────────────────
		while (NkEvent *ev = events.PollEvent()) {
			if (ev->Is<NkWindowCloseEvent>()) {
				running = false;
				break;
			}

			if (auto *mw = ev->As<NkMouseWheelVerticalEvent>()) {
				if (mw->GetModifiers().ctrl) {
					// Ctrl+molette = ordre de profondeur de l'item actif.
					reorderActive(mw->GetDeltaY() > 0.0);
				} else {
					const float32 factor = math::NkPow(1.15f, (float32)mw->GetDeltaY());
					view.ZoomAtPixel(factor, {(float32)mw->GetX(), (float32)mw->GetY()}, vp);
				}
			}

			if (auto *mb = ev->As<NkMouseButtonPressEvent>()) {
				const float32 px = (float32)mb->GetX(), py = (float32)mb->GetY();
				mousePix = {px, py};
				if (mb->IsMiddle() || (mb->IsLeft() && spaceDown)) {
					mode = NkMode::Pan;
				} else if (mb->IsLeft()) {
					const bool ctrl = mb->GetModifiers().ctrl;
					// 1) Les poignées de l'item actif ont priorité sur tout.
					bool onHandle = false;
					if (board.active >= 0 && board.active < (int32)board.items.Size()) {
						const nkref::NkRefItem &it = board.items[(usize)board.active];
						math::NkVec2f c[4];
						nkref::NkRefBoard::Corners(it, c);
						for (int32 k = 0; k < 4 && !onHandle; ++k) {
							const math::NkVec2f hp = view.WorldToPixel(c[k], vp);
							if (math::NkAbs(hp.x - px) <= 7.0f && math::NkAbs(hp.y - py) <= 7.0f) {
								mode = NkMode::ScaleCorner;
								grabCorner = k;
								grabAnchorWorld = c[(k + 2) % 4]; // le coin OPPOSÉ ne bouge pas
								grabCenterWorld = it.pos;
								grabScale = it.scale;
								const math::NkVec2f mw2 = view.PixelToWorld({px, py}, vp);
								const float32 ddx = mw2.x - grabAnchorWorld.x, ddy = mw2.y - grabAnchorWorld.y;
								grabDist = math::NkSqrt(ddx * ddx + ddy * ddy);
								if (grabDist < 0.0001f)
									grabDist = 0.0001f;
								onHandle = true;
							}
						}
						if (!onHandle) {
							const math::NkVec2f rh = rotationHandlePix(it, vp);
							if (math::NkAbs(rh.x - px) <= 8.0f && math::NkAbs(rh.y - py) <= 8.0f) {
								mode = NkMode::Rotate;
								grabRotDeg = it.rotationDeg;
								const math::NkVec2f mw2 = view.PixelToWorld({px, py}, vp);
								grabAngleDeg = math::NkAtan2(mw2.y - it.pos.y, mw2.x - it.pos.x) *
											   (180.0f / math::NK_PI_F);
								onHandle = true;
							}
						}
					}
					// 2) Sinon : sélection d'item, ou rectangle sur le vide.
					if (!onHandle) {
						const int32 hit = selectAtPixel(px, py, ctrl);
						if (hit >= 0) {
							mode = NkMode::DragItems;
						} else {
							mode = NkMode::RectSelect;
							rectStartPix = {px, py};
							rectAdditive = ctrl;
						}
					}
				}
			}

			if (auto *mr = ev->As<NkMouseButtonReleaseEvent>()) {
				if (mode == NkMode::RectSelect && mr->IsLeft()) {
					const math::NkVec2f a = view.PixelToWorld(rectStartPix, vp);
					const math::NkVec2f b = view.PixelToWorld(mousePix, vp);
					board.SelectInRect(a, b, rectAdditive);
				}
				if (mr->IsMiddle() || mr->IsLeft())
					mode = NkMode::Idle;
			}

			if (auto *mm = ev->As<NkMouseMoveEvent>()) {
				const float32 px = (float32)mm->GetX(), py = (float32)mm->GetY();
				mousePix = {px, py};
				const float32 dx = (float32)mm->GetDeltaX(), dy = (float32)mm->GetDeltaY();
				switch (mode) {
					case NkMode::Pan:
						view.PanByPixels(dx, dy);
						break;
					case NkMode::DragItems:
						moveSelectionByPixels(dx, dy);
						break;
					case NkMode::ScaleCorner:
						if (board.active >= 0 && board.active < (int32)board.items.Size()) {
							nkref::NkRefItem &it = board.items[(usize)board.active];
							const math::NkVec2f mw2 = view.PixelToWorld({px, py}, vp);
							const float32 ddx = mw2.x - grabAnchorWorld.x, ddy = mw2.y - grabAnchorWorld.y;
							const float32 dist = math::NkSqrt(ddx * ddx + ddy * ddy);
							float32 s = grabScale * (dist / grabDist);
							if (s < 0.01f)
								s = 0.01f;
							if (s > 100.0f)
								s = 100.0f;
							// L'ancre (coin opposé) reste FIXE : le centre suit
							// linéairement le rapport d'échelle.
							const float32 r = s / grabScale;
							it.scale = s;
							it.pos = {grabAnchorWorld.x + (grabCenterWorld.x - grabAnchorWorld.x) * r,
									  grabAnchorWorld.y + (grabCenterWorld.y - grabAnchorWorld.y) * r};
						}
						break;
					case NkMode::Rotate:
						if (board.active >= 0 && board.active < (int32)board.items.Size()) {
							nkref::NkRefItem &it = board.items[(usize)board.active];
							const math::NkVec2f mw2 = view.PixelToWorld({px, py}, vp);
							const float32 a = math::NkAtan2(mw2.y - it.pos.y, mw2.x - it.pos.x) *
											  (180.0f / math::NK_PI_F);
							float32 rot = grabRotDeg + (a - grabAngleDeg);
							if (mm->GetModifiers().shift) { // Maj = crans de 15°
								rot = (float32)((int32)(rot / 15.0f + (rot >= 0 ? 0.5f : -0.5f))) * 15.0f;
							}
							it.rotationDeg = rot;
						}
						break;
					default:
						break;
				}
			}

			if (auto *kp = ev->As<NkKeyPressEvent>()) {
				const NkKey k = kp->GetKey();
				const bool ctrl = kp->GetModifiers().ctrl;
				if (k == NkKey::NK_DELETE) {
					removeSelected();
				} else if (k == NkKey::NK_X && board.HasSelection()) {
					board.MirrorSelected(true);
				} else if (k == NkKey::NK_Y && board.HasSelection()) {
					board.MirrorSelected(false);
				} else if (k == NkKey::NK_PAGE_UP) {
					reorderActive(true);
				} else if (k == NkKey::NK_PAGE_DOWN) {
					reorderActive(false);
				} else if (k == NkKey::NK_V && ctrl) {
					pasteClipboard(view.PixelToWorld(mousePix, vp));
				}
			}

			if (auto *df = ev->As<NkDropFileEvent>()) {
				// Chaque fichier au point de dépôt, en cascade de 24 px pour
				// que plusieurs fichiers ne s'empilent pas exactement.
				const math::NkVec2f base = view.PixelToWorld(
					{(float32)df->data.x, (float32)df->data.y}, vp);
				for (uint32 i = 0; i < df->data.Count(); ++i) {
					const float32 off = (float32)i * 24.0f / view.zoom;
					addImageFromFile(df->data.paths[i].CStr(), {base.x + off, base.y + off});
				}
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

		// ── Crochets d'agent : mêmes fonctions que la souris ────────────────
		if (agent.panFrame > 0 && agentFrame == agent.panFrame)
			view.PanByPixels(agent.panDx, agent.panDy);
		if (agent.zoomFrame > 0 && agentFrame == agent.zoomFrame)
			view.ZoomAtPixel(agent.zoomFactor, {agent.zoomPx, agent.zoomPy}, vp);
		for (int32 i = 0; i < agent.dropCount; ++i) {
			if (agent.drops[i].frame > 0 && agentFrame == agent.drops[i].frame)
				addImageFromFile(agent.drops[i].path,
								 view.PixelToWorld({agent.drops[i].px, agent.drops[i].py}, vp));
		}
		for (int32 i = 0; i < agent.clickCount; ++i) {
			if (agent.clicks[i].frame > 0 && agentFrame == agent.clicks[i].frame)
				selectAtPixel(agent.clicks[i].px, agent.clicks[i].py, false);
		}
		if (agent.moveFrame > 0 && agentFrame == agent.moveFrame)
			moveSelectionByPixels(agent.moveDx, agent.moveDy);

		// ── Rendu : fond + grille + images + sélection, en espace écran ─────
		target.Clear(NkColor2D{20, 22, 25, 255});
		NkRenderer2D &r = target.GetRenderer2D();

		const float32 spacing = view.GridSpacing(32.0f);
		const math::NkVec2f wMin = view.PixelToWorld({0.0f, 0.0f}, vp);
		const math::NkVec2f wMax = view.PixelToWorld(vp, vp);
		const NkColor2D minor{255, 255, 255, 10};
		const NkColor2D major{255, 255, 255, 26};
		const int64 ix0 = (int64)math::NkFloor(wMin.x / spacing);
		const int64 ix1 = (int64)math::NkCeil(wMax.x / spacing);
		for (int64 i = ix0; i <= ix1; ++i) {
			const float32 xPix = view.WorldToPixel({(float32)i * spacing, 0.0f}, vp).x;
			r.DrawLine({xPix, 0.0f}, {xPix, vp.y}, (i % 8) == 0 ? major : minor, 1.0f);
		}
		const int64 iy0 = (int64)math::NkFloor(wMin.y / spacing);
		const int64 iy1 = (int64)math::NkCeil(wMax.y / spacing);
		for (int64 i = iy0; i <= iy1; ++i) {
			const float32 yPix = view.WorldToPixel({0.0f, (float32)i * spacing}, vp).y;
			r.DrawLine({0.0f, yPix}, {vp.x, yPix}, (i % 8) == 0 ? major : minor, 1.0f);
		}
		const math::NkVec2f origin = view.WorldToPixel({0.0f, 0.0f}, vp);
		const NkColor2D axis{247, 154, 40, 70};
		if (origin.x >= 0.0f && origin.x <= vp.x)
			r.DrawLine({origin.x, 0.0f}, {origin.x, vp.y}, axis, 1.0f);
		if (origin.y >= 0.0f && origin.y <= vp.y)
			r.DrawLine({0.0f, origin.y}, {vp.x, origin.y}, axis, 1.0f);

		// Les images, du fond vers le dessus (l'ordre du tableau).
		for (usize i = 0; i < board.items.Size(); ++i) {
			const nkref::NkRefItem &it = board.items[i];
			NkTexture *tex = textures[i];
			if (!tex || !tex->IsValid())
				continue;
			NkSprite sp(*tex);
			sp.SetOrigin({(float32)it.texW * 0.5f, (float32)it.texH * 0.5f});
			sp.SetPosition(view.WorldToPixel(it.pos, vp));
			sp.SetRotation(it.rotationDeg);
			sp.SetScale({it.scale * view.zoom, it.scale * view.zoom});
			sp.SetFlipX(it.mirrorX);
			sp.SetFlipY(it.mirrorY);
			// NkSprite hérite de NkIDrawable2D ET NkDrawable → on lève
			// l'ambiguïté de Draw() en ciblant le chemin NkDrawable.
			target.Draw(static_cast<const NkDrawable &>(sp));
		}

		// Contours de sélection + poignées (par-dessus les images).
		const NkColor2D selCol{46, 140, 153, 255};	  // sélectionné
		const NkColor2D activeCol{247, 154, 40, 255}; // actif (porte les poignées)
		for (usize i = 0; i < board.items.Size(); ++i) {
			const nkref::NkRefItem &it = board.items[i];
			if (!it.selected)
				continue;
			math::NkVec2f c[4];
			nkref::NkRefBoard::Corners(it, c);
			math::NkVec2f p[4];
			for (int32 k = 0; k < 4; ++k)
				p[k] = view.WorldToPixel(c[k], vp);
			const NkColor2D col = ((int32)i == board.active) ? activeCol : selCol;
			for (int32 k = 0; k < 4; ++k)
				r.DrawLine(p[k], p[(k + 1) % 4], col, 1.5f);
			if ((int32)i == board.active) {
				for (int32 k = 0; k < 4; ++k)
					r.DrawFilledRect({p[k].x - 4.0f, p[k].y - 4.0f, 8.0f, 8.0f}, activeCol);
				const math::NkVec2f topMid{(p[0].x + p[1].x) * 0.5f, (p[0].y + p[1].y) * 0.5f};
				const math::NkVec2f rh = rotationHandlePix(it, vp);
				r.DrawLine(topMid, rh, activeCol, 1.5f);
				r.DrawCircle(rh, 5.0f, activeCol, 24);
			}
		}

		// Rectangle de sélection en cours.
		if (mode == NkMode::RectSelect) {
			const float32 x0 = rectStartPix.x < mousePix.x ? rectStartPix.x : mousePix.x;
			const float32 y0 = rectStartPix.y < mousePix.y ? rectStartPix.y : mousePix.y;
			const float32 w = math::NkAbs(mousePix.x - rectStartPix.x);
			const float32 h = math::NkAbs(mousePix.y - rectStartPix.y);
			const NkColor2D rimCol{46, 140, 153, 180};
			r.DrawFilledRect({x0, y0, w, h}, NkColor2D{46, 140, 153, 30});
			r.DrawLine({x0, y0}, {x0 + w, y0}, rimCol, 1.0f);
			r.DrawLine({x0 + w, y0}, {x0 + w, y0 + h}, rimCol, 1.0f);
			r.DrawLine({x0 + w, y0 + h}, {x0, y0 + h}, rimCol, 1.0f);
			r.DrawLine({x0, y0 + h}, {x0, y0}, rimCol, 1.0f);
		}

		target.Display();

		if (--titleCooldown <= 0) {
			char t[96];
			std::snprintf(t, sizeof(t), "NkRef - %d%% - %d image%s", (int)(view.zoom * 100.0f + 0.5f),
						  (int)board.items.Size(), board.items.Size() > 1 ? "s" : "");
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

		NkChrono::Sleep(8.0);
	}

	// Libération des textures AVANT la mort du contexte GL (target vit encore).
	for (usize i = 0; i < textures.Size(); ++i)
		alloc.Delete(textures[i]);
	textures.Clear();

	// PAS de window.Close() explicite ici : `target` (déclaré APRÈS `window`)
	// doit mourir AVANT la fenêtre — glXDestroyContext sur un Display X11 déjà
	// fermé segfaultait sous WSLg (backtrace _XSend/XInitExtension, vécu le
	// 2026-08-11). L'ordre de destruction C++ inverse fait exactement ça, et
	// ~NkWindow appelle Close() lui-même.
	return 0;
}
