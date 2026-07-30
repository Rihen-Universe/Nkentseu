// =============================================================================
// Noge/ECS/Systems/NkUISystem.cpp — système ECS UI in-game (HUD), CPU-only
// =============================================================================
// Étage 1 : NkUISystem (layout + liste de primitives abstraite NkUIDrawCmd).
// Étage 2 : replay backend-agnostique (NkUIDrawBackend).
// Étage 3 : NkUISoftwareCanvasBackend (NKCanvas Software + NKFont, headless).
// Cible de production future : backend renderer::NkRender2D (device NKRHI).
// =============================================================================
#include "NkUISystem.h"
#include "NKImage/Core/NkImage.h" // capture PNG optionnelle (codec réel NKImage)
#include "NKLogger/NkLog.h"
#include "NKMemory/NkAllocator.h"

namespace nkentseu {

	using namespace ecs;

	namespace {

		// NkColor4 (float [0..1]) -> octet [0..255], arrondi au plus proche.
		inline uint8 ToByte(float32 c) noexcept {
			const float32 v = c < 0.f ? 0.f : (c > 1.f ? 1.f : c);
			return static_cast<uint8>(v * 255.f + 0.5f);
		}

	} // namespace

	// =========================================================================
	// NkUISoftwareCanvasBackend
	// =========================================================================

	NkUISoftwareCanvasBackend::~NkUISoftwareCanvasBackend() noexcept {
		Shutdown();
	}

	// -------------------------------------------------------------------------
	bool NkUISoftwareCanvasBackend::Init(uint32 width, uint32 height, bool enableText) noexcept {
		if (width == 0u || height == 0u) {
			logger.Errorf("[NkUISoftwareCanvasBackend] Init(%u, %u) : dimensions invalides\n", width, height);
			return false;
		}

		// Framebuffer CPU 2D pur : pas de depth buffer (le HUD est peint dans
		// l'ordre des commandes, comme le 2D de NkSoftwareRenderer2D).
		mFB.depthEnabled = false;
		mFB.Resize(width, height);
		mReady = mFB.IsValid();

		// Texte : atlas CPU NKFont, police embarquée (aucun fichier externe).
		mTextReady = false;
		mFont = nullptr;
		mAtlasPixels = nullptr;
		if (enableText) {
			mFont = NkFontEmbedded::AddDefaultFont(mAtlas); // ProggyClean 13 px
			if (mFont && mAtlas.Build()) {
				nkft_uint8 *px = nullptr;
				mAtlas.GetTexDataAsAlpha8(&px, &mAtlasW, &mAtlasH);
				if (px && mAtlasW > 0 && mAtlasH > 0) {
					mAtlasPixels = px;
					mTextReady = true;
				}
			}
			if (!mTextReady) {
				logger.Warnf("[NkUISoftwareCanvasBackend] Police embarquée indisponible : texte HUD "
							 "désactivé (rects/panels/barres restent fonctionnels)\n");
				mFont = nullptr;
			}
		}

		return mReady;
	}

	// -------------------------------------------------------------------------
	void NkUISoftwareCanvasBackend::Shutdown() noexcept {
		mAtlas.Clear();
		mFont = nullptr;
		mAtlasPixels = nullptr;
		mAtlasW = mAtlasH = 0;
		mTextReady = false;
		mFB = NkSoftwareFramebuffer{};
		mReady = false;
	}

	// -------------------------------------------------------------------------
	void NkUISoftwareCanvasBackend::Begin(const uint8 clearColor[4]) noexcept {
		if (mReady)
			mFB.Clear(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
	}

	// -------------------------------------------------------------------------
	void NkUISoftwareCanvasBackend::DrawRect(float32 x, float32 y, float32 w, float32 h,
											 const NkColor4 &c) noexcept {
		if (!mReady || w <= 0.f || h <= 0.f || c.a <= 0.f)
			return;
		mFB.FillRect(static_cast<int32>(x + 0.5f), static_cast<int32>(y + 0.5f), static_cast<int32>(w + 0.5f),
					 static_cast<int32>(h + 0.5f), ToByte(c.r), ToByte(c.g), ToByte(c.b), ToByte(c.a));
	}

	// -------------------------------------------------------------------------
	float32 NkUISoftwareCanvasBackend::TextWidth(const char *text) const noexcept {
		return (mTextReady && text) ? mFont->CalcTextSizeX(text) : 0.f;
	}

	float32 NkUISoftwareCanvasBackend::TextLineHeight() const noexcept {
		return mTextReady ? mFont->lineAdvance : 0.f;
	}

	float32 NkUISoftwareCanvasBackend::TextAscent() const noexcept {
		return mTextReady ? mFont->ascent : 0.f;
	}

	// -------------------------------------------------------------------------
	void NkUISoftwareCanvasBackend::DrawTextLine(float32 x, float32 baselineY, const char *text, const NkColor4 &c,
												 float32 maxX) noexcept {
		if (!mReady || !mTextReady || !text || !*text || c.a <= 0.f)
			return;

		const uint8 cr = ToByte(c.r);
		const uint8 cg = ToByte(c.g);
		const uint8 cb = ToByte(c.b);
		const float32 colA = c.a < 0.f ? 0.f : (c.a > 1.f ? 1.f : c.a);

		const char *p = text;
		const char *end = text;
		while (*end)
			++end;

		float32 penX = x;
		while (p < end) {
			const NkFontCodepoint cp = NkFont::DecodeUTF8(&p, end);
			if (cp == 0u)
				break;
			const NkFontGlyph *g = mFont->FindGlyph(cp);
			if (!g)
				continue;
			// Quad du glyphe relatif à la baseline (même convention que
			// NkGuiDrawList::AddText — le consommateur NKGui réel de NKFont).
			const float32 gx0 = penX + g->x0;
			const float32 gy0 = baselineY + g->y0;
			const float32 gx1 = penX + g->x1;
			const float32 gy1 = baselineY + g->y1;
			if (gx1 > maxX)
				break; // troncature simple au rect
			if (g->visible && gx1 > gx0 && gy1 > gy0) {
				// Resample nearest de l'atlas alpha8 -> DrawPoint blendé du
				// framebuffer Software (primitive réelle NKCanvas).
				const float32 invW = 1.f / (gx1 - gx0);
				const float32 invH = 1.f / (gy1 - gy0);
				const int32 iy0 = static_cast<int32>(math::NkFloor(gy0));
				const int32 iy1 = static_cast<int32>(math::NkCeil(gy1));
				const int32 ix0 = static_cast<int32>(math::NkFloor(gx0));
				const int32 ix1 = static_cast<int32>(math::NkCeil(gx1));
				for (int32 iy = iy0; iy < iy1; ++iy) {
					const float32 v = (static_cast<float32>(iy) + 0.5f - gy0) * invH;
					if (v < 0.f || v >= 1.f)
						continue;
					const float32 texV = g->v0 + v * (g->v1 - g->v0);
					int32 ty = static_cast<int32>(texV * static_cast<float32>(mAtlasH));
					if (ty < 0)
						ty = 0;
					if (ty >= mAtlasH)
						ty = mAtlasH - 1;
					for (int32 ix = ix0; ix < ix1; ++ix) {
						const float32 u = (static_cast<float32>(ix) + 0.5f - gx0) * invW;
						if (u < 0.f || u >= 1.f)
							continue;
						const float32 texU = g->u0 + u * (g->u1 - g->u0);
						int32 tx = static_cast<int32>(texU * static_cast<float32>(mAtlasW));
						if (tx < 0)
							tx = 0;
						if (tx >= mAtlasW)
							tx = mAtlasW - 1;
						const uint8 a8 = mAtlasPixels[static_cast<usize>(ty) * static_cast<usize>(mAtlasW) +
													  static_cast<usize>(tx)];
						if (a8 == 0u)
							continue;
						const uint8 outA = static_cast<uint8>(static_cast<float32>(a8) * colA + 0.5f);
						mFB.DrawPoint(ix, iy, cr, cg, cb, outA, 0.f, true);
					}
				}
			}
			penX += g->advanceX;
		}
	}

	// -------------------------------------------------------------------------
	bool NkUISoftwareCanvasBackend::SavePNG(const char *path) const noexcept {
		if (!mReady || !path)
			return false;

		NkImage img;
		if (!img.Create(mFB.width, mFB.height, math::NkColor(0u, 0u, 0u, 255u), 4))
			return false;

		// GetPixel résout le swizzle octet-natif (BGRA Windows / RGBA ailleurs)
		// vers du RGBA logique — copie pixel à pixel vers l'image RGBA8.
		NkSoftwareFramebuffer &fb = const_cast<NkSoftwareFramebuffer &>(mFB);
		for (uint32 y = 0; y < mFB.height; ++y) {
			uint8 *row = img.RowPtr(static_cast<int32>(y));
			for (uint32 x = 0; x < mFB.width; ++x) {
				const math::NkColor c = fb.GetPixel(x, y);
				row[x * 4u + 0u] = c.r;
				row[x * 4u + 1u] = c.g;
				row[x * 4u + 2u] = c.b;
				row[x * 4u + 3u] = c.a;
			}
		}
		return img.SavePNG(path);
	}

	// =========================================================================
	// NkUISystem
	// =========================================================================

	NkUISystem::~NkUISystem() noexcept {
		Shutdown();
	}

	// -------------------------------------------------------------------------
	bool NkUISystem::Init(uint32 width, uint32 height, bool enableText) noexcept {
		Shutdown();

		NkUISoftwareCanvasBackend *sw = memory::NkGetDefaultAllocator().New<NkUISoftwareCanvasBackend>();
		if (!sw)
			return false;
		if (!sw->Init(width, height, enableText)) {
			memory::NkGetDefaultAllocator().Delete(sw);
			return false;
		}
		mOwnedSoftware = sw;
		mBackend = sw;
		return true;
	}

	// -------------------------------------------------------------------------
	void NkUISystem::SetBackend(NkUIDrawBackend *backend) noexcept {
		if (mOwnedSoftware) {
			memory::NkGetDefaultAllocator().Delete(mOwnedSoftware);
			mOwnedSoftware = nullptr;
		}
		mBackend = backend;
	}

	// -------------------------------------------------------------------------
	void NkUISystem::Shutdown() noexcept {
		if (mOwnedSoftware) {
			memory::NkGetDefaultAllocator().Delete(mOwnedSoftware);
			mOwnedSoftware = nullptr;
		}
		mBackend = nullptr;
		mCmds.Clear();
		mTextArena.Clear();
	}

	// -------------------------------------------------------------------------
	void NkUISystem::ComputeRect(const NkRectTransform &rt, float32 refW, float32 refH, float32 &outX, float32 &outY,
								 float32 &outW, float32 &outH) noexcept {
		const float32 aMinX = rt.anchor.minX * refW;
		const float32 aMaxX = rt.anchor.maxX * refW;
		const float32 aMinY = rt.anchor.minY * refH;
		const float32 aMaxY = rt.anchor.maxY * refH;

		// Axe X.
		if (rt.anchor.minX == rt.anchor.maxX) {
			outW = rt.sizeDelta.x;
			outX = aMinX + rt.anchoredPos.x - rt.pivot.x * outW;
		} else {
			// Étirement entre les deux ancres ; sizeDelta.x = ajustement de taille.
			outW = (aMaxX - aMinX) + rt.sizeDelta.x;
			outX = aMinX + rt.anchoredPos.x - rt.pivot.x * rt.sizeDelta.x;
		}

		// Axe Y (0 = haut, comme le framebuffer).
		if (rt.anchor.minY == rt.anchor.maxY) {
			outH = rt.sizeDelta.y;
			outY = aMinY + rt.anchoredPos.y - rt.pivot.y * outH;
		} else {
			outH = (aMaxY - aMinY) + rt.sizeDelta.y;
			outY = aMinY + rt.anchoredPos.y - rt.pivot.y * rt.sizeDelta.y;
		}
	}

	// -------------------------------------------------------------------------
	void NkUISystem::PushRect(float32 x, float32 y, float32 w, float32 h, const NkColor4 &c) noexcept {
		if (w <= 0.f || h <= 0.f || c.a <= 0.f)
			return;
		NkUIDrawCmd cmd;
		cmd.kind = NkUIDrawCmd::Kind::Rect;
		cmd.x = x;
		cmd.y = y;
		cmd.w = w;
		cmd.h = h;
		cmd.color = c;
		mCmds.PushBack(cmd);
	}

	// -------------------------------------------------------------------------
	void NkUISystem::PushStroke(float32 x, float32 y, float32 w, float32 h, float32 t, const NkColor4 &c) noexcept {
		if (t <= 0.f || w <= 0.f || h <= 0.f)
			return;
		if (t * 2.f >= w || t * 2.f >= h) { // bordure plus large que le rect
			PushRect(x, y, w, h, c);
			return;
		}
		PushRect(x, y, w, t, c);					// haut
		PushRect(x, y + h - t, w, t, c);			// bas
		PushRect(x, y + t, t, h - 2.f * t, c);		// gauche
		PushRect(x + w - t, y + t, t, h - 2.f * t, c); // droite
	}

	// -------------------------------------------------------------------------
	void NkUISystem::PushText(float32 x, float32 baselineY, const char *text, const NkColor4 &c,
							  float32 maxX) noexcept {
		if (!text || !*text || c.a <= 0.f)
			return;
		NkUIDrawCmd cmd;
		cmd.kind = NkUIDrawCmd::Kind::Text;
		cmd.x = x;
		cmd.y = baselineY;
		cmd.w = maxX;
		cmd.h = 0.f;
		cmd.color = c;
		cmd.textOffset = static_cast<uint32>(mTextArena.Size());
		for (const char *p = text; *p; ++p)
			mTextArena.PushBack(*p);
		mTextArena.PushBack('\0');
		mCmds.PushBack(cmd);
	}

	// -------------------------------------------------------------------------
	void NkUISystem::Execute(NkWorld &world, float32 dt) {
		if (!mBackend)
			return; // Init()/SetBackend() pas encore appelé — même convention
					// que NkAudioSystem (no-op tant que la ressource manque).

		mCmds.Clear();
		mTextArena.Clear();

		const float32 fbW = static_cast<float32>(mBackend->Width());
		const float32 fbH = static_cast<float32>(mBackend->Height());

		// 1) Référentiel : premier NkCanvas ScreenSpace visible.
		float32 refW = fbW;
		float32 refH = fbH;
		bool canvasVisible = true;
		bool canvasFound = false;
		world.Query<NkCanvas>().ForEach([&](NkEntityId, NkCanvas &cv) {
			if (canvasFound || cv.mode != NkCanvasMode::ScreenSpace)
				return;
			canvasFound = true;
			canvasVisible = cv.visible;
			if (cv.scaleWithScreen && cv.referenceWidth > 0.f && cv.referenceHeight > 0.f) {
				refW = cv.referenceWidth;
				refH = cv.referenceHeight;
				cv.scaleFactor = fbW / cv.referenceWidth;
			}
		});

		const float32 sx = fbW / refW;
		const float32 sy = fbH / refH;

		// 2) Layout + PRODUCTION DE PRIMITIVES par entité
		//    (Panel < Image < ProgressBar < Texte au sein d'une entité).
		if (canvasVisible) {
			world.Query<NkRectTransform>().ForEach([&](NkEntityId id, NkRectTransform &rt) {
				float32 rx, ry, rw, rh;
				ComputeRect(rt, refW, refH, rx, ry, rw, rh);

				// Passage référence canvas -> pixels framebuffer, écrit dans le
				// composant (contrat documenté par NkRectTransform : "mis à jour
				// par NkUILayoutSystem" — ce système en tient le rôle).
				rt.rectX = rx * sx;
				rt.rectY = ry * sy;
				rt.rectW = rw * sx;
				rt.rectH = rh * sy;

				if (!rt.visible)
					return;

				// ── NkUIPanel : fond + bordure ────────────────────────────
				if (const NkUIPanel *panel = world.Get<NkUIPanel>(id)) {
					if (panel->visible) {
						PushRect(rt.rectX, rt.rectY, rt.rectW, rt.rectH, panel->backgroundColor);
						PushStroke(rt.rectX, rt.rectY, rt.rectW, rt.rectH, panel->borderWidth * sx,
								   panel->borderColor);
					}
				}

				// ── NkUIImage : rect de couleur unie (texture non blittée) ─
				if (const NkUIImage *img = world.Get<NkUIImage>(id)) {
					if (img->visible) {
						float32 fx = rt.rectX, fy = rt.rectY, fw = rt.rectW, fh = rt.rectH;
						if (img->imageType == NkImageType::Filled) {
							const float32 t =
								img->fillAmount < 0.f ? 0.f : (img->fillAmount > 1.f ? 1.f : img->fillAmount);
							if (img->fillMethod == NkFillMethod::Vertical) {
								fh = rt.rectH * t;
								fy = rt.rectY + (rt.rectH - fh); // remplit du bas vers le haut
							} else {
								fw = rt.rectW * t; // Horizontal (Radial* : fallback horizontal)
							}
						}
						PushRect(fx, fy, fw, fh, img->color);
					}
				}

				// ── NkUIProgressBar : fond + remplissage animé ────────────
				if (NkUIProgressBar *pb = world.Get<NkUIProgressBar>(id)) {
					if (pb->visible) {
						// Animation smooth optionnelle (displayedValue -> value).
						if (pb->animate) {
							const float32 delta = pb->value - pb->displayedValue;
							const float32 step = pb->animSpeed * dt;
							if (delta > step)
								pb->displayedValue += step;
							else if (delta < -step)
								pb->displayedValue -= step;
							else
								pb->displayedValue = pb->value;
						} else {
							pb->displayedValue = pb->value;
						}
						const float32 t = pb->displayedValue < 0.f
											  ? 0.f
											  : (pb->displayedValue > 1.f ? 1.f : pb->displayedValue);

						PushRect(rt.rectX, rt.rectY, rt.rectW, rt.rectH, pb->backgroundColor);
						if (pb->vertical) {
							const float32 fh = rt.rectH * t;
							PushRect(rt.rectX, rt.rectY + (rt.rectH - fh), rt.rectW, fh, pb->fillColor);
						} else if (pb->reverse) {
							const float32 fw = rt.rectW * t;
							PushRect(rt.rectX + (rt.rectW - fw), rt.rectY, fw, rt.rectH, pb->fillColor);
						} else {
							PushRect(rt.rectX, rt.rectY, rt.rectW * t, rt.rectH, pb->fillColor);
						}
					}
				}

				// ── NkUIText : ligne de texte alignée (métriques backend) ──
				if (const NkUIText *txt = world.Get<NkUIText>(id)) {
					if (txt->visible && mBackend->TextSupported() && txt->text[0] != '\0') {
						const float32 tw = mBackend->TextWidth(txt->text);
						const float32 lh = mBackend->TextLineHeight();
						const float32 asc = mBackend->TextAscent();

						float32 tx = rt.rectX;
						if (txt->alignH == NkTextAlign::Center)
							tx = rt.rectX + (rt.rectW - tw) * 0.5f;
						else if (txt->alignH == NkTextAlign::Right)
							tx = rt.rectX + rt.rectW - tw;

						float32 baseline = rt.rectY + asc; // Top
						if (txt->alignV == NkTextAlign::Middle)
							baseline = rt.rectY + (rt.rectH - lh) * 0.5f + asc;
						else if (txt->alignV == NkTextAlign::Bottom)
							baseline = rt.rectY + rt.rectH - lh + asc;

						PushText(tx, baseline, txt->text, txt->color, rt.rectX + rt.rectW);
					}
				}
			});
		}

		// 3) REPLAY backend-agnostique : la même liste sera consommée demain
		//    par un backend renderer::NkRender2D (device NKRHI) sans changer
		//    une ligne des étapes 1-2.
		mBackend->Begin(mClear);
		for (nk_usize i = 0; i < mCmds.Size(); ++i) {
			const NkUIDrawCmd &cmd = mCmds[i];
			if (cmd.kind == NkUIDrawCmd::Kind::Rect) {
				mBackend->DrawRect(cmd.x, cmd.y, cmd.w, cmd.h, cmd.color);
			} else {
				mBackend->DrawTextLine(cmd.x, cmd.y, CommandText(cmd), cmd.color, cmd.w);
			}
		}
	}

} // namespace nkentseu
