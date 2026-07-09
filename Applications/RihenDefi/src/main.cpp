// =============================================================================
// RihenDefi — Portage Nkentseu du script Pygame « Défi Multi-plateforme »
// Équivalent de : ordre de mission/semaine2/script.py
// =============================================================================
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkColor.h"
#include "NKMath/NKMath.h"
#include "NKContainers/String/NkString.h"
#include "NKImage/Core/NkImage.h"

#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Renderer/Shapes/NkCircleShape.h"
#include "NKCanvas/Renderer/Resources/NkTexture.h"
#include "NKCanvas/Renderer/Resources/NkSprite.h"
#include "NKCanvas/Renderer/Resources/NkFont.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

NKENTSEU_DEFINE_APP_DATA(([]() {
	NkAppData d{};
	d.appName = "RihenDefi";
	d.appVersion = "1.0.0";
	return d;
})());

// Couleurs de la balle (comme ball_colors dans le script Python).
static const NkColor2D kBallColors[] = {
	{255, 0, 0, 255},	// rouge
	{0, 255, 0, 255},	// vert
	{0, 0, 255, 255},	// bleu
	{255, 165, 0, 255}, // orange
	{255, 0, 255, 255}, // magenta
};
static constexpr int32 kBallColorCount = 5;

// Sprite jaune 64×64 si sprite.png est introuvable (fallback Pygame).
static void MakeYellowSpriteImage(NkImage &img) {
	img.Create(64, 64, math::NkColor{0, 0, 0, 0}, 4);
	img.FillRect(0, 0, 64, 64, math::NkColor{255, 255, 0, 255});
}

static bool TryLoadFont(nkentseu::renderer::NkFont &font, NkIRenderer2D &renderer) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
	if (font.LoadFromFile(renderer, "C:/Windows/Fonts/arial.ttf"))
		return true;
#endif
	return font.LoadFromFile(renderer, "assets/arial.ttf");
}

nkentseu::renderer::NkSprite sprite;

int nkmain(const NkEntryState &state) {
	(void)state;

	// ── Étape 1 : fenêtre 1180×620 redimensionnable (pygame.display.set_mode) ──
	NkWindowConfig cfg;
	cfg.title = "Rihen - Défi Multi-plateforme";
	cfg.width = 1180;
	cfg.height = 620;
	cfg.centered = true;
	cfg.resizable = true;

	NkWindow window;
	if (!window.Create(cfg)) {
		logger.Error("[RihenDefi] echec creation fenetre");
		return -1;
	}

	// ── Étape 2 : cible de rendu 2D (NKCanvas) ───────────────────────────────
	NkContextDesc desc;
	desc.api = NkGraphicsApi::NK_GFX_API_OPENGL; // backend le plus stable
	NkRenderWindow target(window, desc);
	if (!target.IsValid()) {
		logger.Error("[RihenDefi] echec NkRenderWindow");
		window.Close();
		return -2;
	}

	// ── Étape 3 : charger le sprite (pygame.image.load) ──────────────────────
	NkImage spriteImg;
	if (!spriteImg.Load("assets/sprite.png")) {
		logger.Warn("[RihenDefi] sprite.png introuvable -> carre jaune 64x64");
		MakeYellowSpriteImage(spriteImg);
	}

	NkTexture spriteTex;
	if (!spriteTex.LoadFromImage(*target.GetRenderer(), spriteImg)) {
		logger.Error("[RihenDefi] echec upload texture sprite");
		window.Close();
		return -3;
	}
	// spriteTex.SetSmooth(true);

	// sprite.SetTexture(spriteTex);

	// C'est après cette ligne que vos commandes fonctionneront :
	sprite.SetScale(1.f, 1.f); // 64×64 comme pygame.transform.scale(..., (64, 64))

	// ── Étape 4 : police + textes (pygame.font.SysFont) ─────────────────────
	nkentseu::renderer::NkFont font;
	const bool hasFont = TryLoadFont(font, *target.GetRenderer());

	NkText titleText(font, "Rihen Multi-platform Game", 24);
	titleText.SetFillColor(NkColor2D::White);
	titleText.SetPosition({20.f, 20.f});

	NkText fpsText(font, "FPS: 0", 24);
	fpsText.SetFillColor(NkColor2D::White);
	fpsText.SetPosition({20.f, 55.f});

	// ── Étape 5 : balle rebondissante (pygame.draw.circle) ───────────────────
	static constexpr float32 kBallRadius = 25.f;
	static constexpr float32 kBallSpeed = 300.f; // px/s (≈ 5 px/frame à 60 FPS)

	float32 ballX = 1180.f * 0.5f;
	float32 ballY = 620.f * 0.5f;
	float32 ballVx = kBallSpeed * (nkentseu::math::NkRand.NextUInt32(2) == 0 ? 1.f : -1.f);
	float32 ballVy = kBallSpeed * (nkentseu::math::NkRand.NextUInt32(2) == 0 ? 1.f : -1.f);

	int32 colorIndex = 0;

	NkCircleShape ball(kBallRadius);
	ball.SetOrigin({kBallRadius, kBallRadius});
	ball.SetFillColor(kBallColors[colorIndex]);

	// ── Étape 6 : boucle principale ───────────────────────────────────────────
	bool running = true;
	auto &events = NkEvents();
	events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });

	NkClock clock;
	uint32 lastW = 0, lastH = 0;
	float32 fpsSmoothed = 60.f;

	while (running && window.IsOpen()) {
		const float32 dt = clock.Tick().delta;
		if (dt > 0.f) {
			const float32 instantFps = 1.f / dt;
			fpsSmoothed = fpsSmoothed * 0.9f + instantFps * 0.1f;
		}

		// Événements (pygame.event.get)
		while (NkEvent *ev = events.PollEvent()) {
			if (auto *press = ev->As<NkMouseButtonPressEvent>()) {
				if (press->IsLeft()) {
					colorIndex = (colorIndex + 1) % kBallColorCount;
					ball.SetFillColor(kBallColors[colorIndex]);
				}
			}
		}

		// Taille courante (gère le resize comme pygame.VIDEORESIZE)
		const math::NkVec2u sz = target.GetSize();
		if (sz.x != lastW || sz.y != lastH) {
			if (lastW != 0 && sz.x > 0 && sz.y > 0)
				target.OnResize(sz.x, sz.y);
			lastW = sz.x;
			lastH = sz.y;
		}
		const float32 W = static_cast<float32>(sz.x);
		const float32 H = static_cast<float32>(sz.y);

		// Mise à jour physique de la balle
		ballX += ballVx * dt;
		ballY += ballVy * dt;

		if (ballX - kBallRadius < 0.f) {
			ballX = kBallRadius;
			ballVx *= -1.f;
		} else if (ballX + kBallRadius > W) {
			ballX = W - kBallRadius;
			ballVx *= -1.f;
		}
		if (ballY - kBallRadius < 0.f) {
			ballY = kBallRadius;
			ballVy *= -1.f;
		} else if (ballY + kBallRadius > H) {
			ballY = H - kBallRadius;
			ballVy *= -1.f;
		}

		ball.SetPosition({ballX, ballY});

		// Position du sprite : centré en X, à 1/3 de la hauteur
		sprite.SetPosition({(W - 64.f) * 0.5f, H / 3.f - 32.f});

		// ── Rendu (screen.fill + blit + draw.circle + flip) ───────────────────
		target.Clear(NkColor2D{30, 30, 40, 255}); // BACKGROUND_COLOR

		target.Draw(static_cast<const NkDrawable &>(sprite));
		target.Draw(ball);

		if (hasFont) {
			fpsText.SetString(NkString::Format("FPS: %d", (int)fpsSmoothed).CStr());
			target.Draw(static_cast<const nkentseu::renderer::NkIDrawable2D &>(titleText));
			target.Draw(static_cast<const nkentseu::renderer::NkIDrawable2D &>(fpsText));
		}

		target.Display(); // pygame.display.flip()
	}

	window.Close();
	return 0;
}
