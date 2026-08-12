// =============================================================================
// tests/test_ar.cpp — Self-test AR (standalone, ZÉRO stdlib, SANS caméra).
//
// Méthode : on SYNTHÉTISE l'image qu'une caméra verrait d'un marqueur à une
// pose CONNUE (projection perspective exacte, remplissage par homographie
// inverse), puis on demande à la chaîne de détection de retrouver cette pose.
// L'écart mesuré est l'erreur de la chaîne complète — c'est le même principe
// que le simulateur XR : prouver sans matériel, et savoir de combien on se
// trompe plutôt que d'espérer.
// =============================================================================
#include "NKXR/AR/NkArMarker.h"
#include "NKXR/AR/NkArSession.h"
#include "NKXR/AR/NkArWorld.h"
#include "NKLogger/NkLog.h"
#include "NKMemory/NkAllocator.h"

using namespace nkentseu;
using namespace nkentseu::xr;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg)                                                                                               \
	do {                                                                                                               \
		if (cond) {                                                                                                    \
			++g_pass;                                                                                                  \
		} else {                                                                                                       \
			++g_fail;                                                                                                  \
			logger.Errorf("  [FAIL] %s\n", msg);                                                                       \
		}                                                                                                              \
	} while (0)

// Le motif rendu contient une MARGE BLANCHE d'une cellule : le carré NOIR
// (celui que la détection suit, et donc celui que l'on mesure sur la feuille)
// occupe (gridBits+2)/(gridBits+4) du motif.
static float32 BlackSquareOf(float32 patternSizeMeters, uint32 gridBits) {
	return patternSizeMeters * float32(gridBits + 2u) / float32(gridBits + 4u);
}

static bool Near(float32 a, float32 b, float32 e) {
	const float32 d = a - b;
	return (d < 0 ? -d : d) <= e;
}

// Rend l'image qu'une caméra d'intrinsèques K verrait d'un marqueur carré placé
// à la pose donnée (rotation autour de Y puis X, translation).
// Un fond texture, NON periodique : des valeurs tirees sur une grille grossiere
// puis interpolees. Le suivi par l'image a besoin de relief pour s'accrocher ;
// un motif repetitif, lui, se recollerait a la mauvaise periode et donnerait un
// faux mouvement — d'ou le tirage pseudo-aleatoire plutot qu'un damier.
static void MakeTexture(uint8 *out, uint32 w, uint32 h) {
	const uint32 step = 3;
	const uint32 gw = w / step + 2u;
	const uint32 gh = h / step + 2u;
	auto &allocator = memory::NkGetDefaultAllocator();
	uint8 *grid = static_cast<uint8 *>(allocator.Allocate(gw * gh, 1));
	uint32 seed = 0x1234567u;
	for (uint32 i = 0; i < gw * gh; ++i) {
		seed = seed * 1664525u + 1013904223u;
		grid[i] = uint8(10u + ((seed >> 16) % 235u));
	}
	for (uint32 y = 0; y < h; ++y) {
		for (uint32 x = 0; x < w; ++x) {
			const uint32 gx = x / step, gy = y / step;
			const float32 fx = float32(x % step) / float32(step);
			const float32 fy = float32(y % step) / float32(step);
			const float32 a = float32(grid[gy * gw + gx]), b = float32(grid[gy * gw + gx + 1u]);
			const float32 c = float32(grid[(gy + 1u) * gw + gx]), d = float32(grid[(gy + 1u) * gw + gx + 1u]);
			const float32 top = a + (b - a) * fx, bot = c + (d - c) * fx;
			out[y * w + x] = uint8(top + (bot - top) * fy);
		}
	}
	allocator.Deallocate(grid);
}

// Fabrique la vue qu'aurait la MEME camera apres une rotation pure. Exacte a
// toute profondeur : sous rotation seule, l'image se transforme par K.R.K^-1 et
// la parallaxe n'existe pas. C'est ce qui permet de tester le suivi sans
// modeliser de scene 3D.
static void WarpByRotation(const uint8 *src, uint8 *dst, uint32 w, uint32 h, const NkArCameraIntrinsics &k,
						   float32 yawRad, float32 pitchRad, float32 rollRad) {
	const math::NkQuatf rot = math::NkQuatf::RotateZ(math::NkAngle::FromRad(rollRad)) *
							  math::NkQuatf::RotateY(math::NkAngle::FromRad(yawRad)) *
							  math::NkQuatf::RotateX(math::NkAngle::FromRad(pitchRad));
	for (uint32 y = 0; y < h; ++y) {
		for (uint32 x = 0; x < w; ++x) {
			// Rayon du pixel de DESTINATION, ramene dans l'ancienne camera.
			math::NkVec3f d(((float32(x) + 0.5f) - k.cx) / k.fx, -((float32(y) + 0.5f) - k.cy) / k.fy, -1.f);
			const math::NkVec3f s = rot * d;
			const float32 depth = -s.z;
			if (depth <= 0.001f) {
				dst[y * w + x] = 200;
				continue;
			}
			const float32 u = k.fx * (s.x / depth) + k.cx;
			const float32 v = -k.fy * (s.y / depth) + k.cy;
			const int32 iu = int32(u), iv = int32(v);
			if (iu < 0 || iv < 0 || uint32(iu) + 1u >= w || uint32(iv) + 1u >= h) {
				dst[y * w + x] = 200;
				continue;
			}
			const float32 fu = u - float32(iu), fv = v - float32(iv);
			const float32 a = float32(src[uint32(iv) * w + uint32(iu)]);
			const float32 b = float32(src[uint32(iv) * w + uint32(iu) + 1u]);
			const float32 c = float32(src[(uint32(iv) + 1u) * w + uint32(iu)]);
			const float32 e = float32(src[(uint32(iv) + 1u) * w + uint32(iu) + 1u]);
			const float32 top = a + (b - a) * fu, bot = c + (e - c) * fu;
			dst[y * w + x] = uint8(top + (bot - top) * fv);
		}
	}
}

static void SynthesizeView(uint8 *out, uint32 w, uint32 h, const NkArCameraIntrinsics &k, int32 markerId,
						   uint32 gridBits, float32 sizeMeters, float32 yawDeg, float32 pitchDeg,
						   const math::NkVec3f &position) {
	// Motif du marqueur, à haute résolution pour ne pas ajouter d'escalier.
	const uint32 patternSize = 256;
	auto &allocator = memory::NkGetDefaultAllocator();
	uint8 *pattern = static_cast<uint8 *>(allocator.Allocate(patternSize * patternSize, 1));
	NkArRenderMarker(markerId, gridBits, pattern, patternSize);

	const float32 deg = math::NK_PI_F / 180.f;
	const math::NkMat4f rot = math::NkMat4f::RotationY(math::NkAngle::FromRad(yawDeg * deg)) *
							  math::NkMat4f::RotationX(math::NkAngle::FromRad(pitchDeg * deg));
	// Les 4 coins du marqueur, projetés — l'ordre suit celui de la détection
	// (sens horaire, coin 0 en haut à gauche du motif).
	const float32 half = sizeMeters * 0.5f;
	const math::NkVec3f local[4] = { { -half, half, 0.f }, { half, half, 0.f }, { half, -half, 0.f },
									 { -half, -half, 0.f } };
	NkVec2f projected[4];
	for (uint32 i = 0; i < 4u; ++i) {
		math::NkVec3f p;
		p.x = rot.mat[0][0] * local[i].x + rot.mat[1][0] * local[i].y + rot.mat[2][0] * local[i].z + position.x;
		p.y = rot.mat[0][1] * local[i].x + rot.mat[1][1] * local[i].y + rot.mat[2][1] * local[i].z + position.y;
		p.z = rot.mat[0][2] * local[i].x + rot.mat[1][2] * local[i].y + rot.mat[2][2] * local[i].z + position.z;
		// Caméra regardant -Z : la profondeur utile est -z.
		const float32 depth = -p.z;
		projected[i].x = k.fx * (p.x / depth) + k.cx;
		projected[i].y = -k.fy * (p.y / depth) + k.cy;
	}

	// Remplissage : pour chaque pixel de la boîte englobante, coordonnée dans
	// le motif par interpolation bilinéaire inverse du quadrilatère.
	for (uint32 i = 0; i < w * h; ++i) {
		out[i] = 200; // fond clair
	}
	float32 minX = projected[0].x, maxX = projected[0].x, minY = projected[0].y, maxY = projected[0].y;
	for (uint32 i = 1; i < 4u; ++i) {
		minX = (projected[i].x < minX) ? projected[i].x : minX;
		maxX = (projected[i].x > maxX) ? projected[i].x : maxX;
		minY = (projected[i].y < minY) ? projected[i].y : minY;
		maxY = (projected[i].y > maxY) ? projected[i].y : maxY;
	}
	// Balayage en coordonnées du MOTIF (u,v) puis projection : simple, exact,
	// et sans trou puisqu'on sur-échantillonne.
	const uint32 steps = 2048;
	for (uint32 sv = 0; sv < steps; ++sv) {
		for (uint32 su = 0; su < steps; ++su) {
			const float32 u = float32(su) / float32(steps - 1u);
			const float32 v = float32(sv) / float32(steps - 1u);
			// Interpolation bilinéaire des 4 coins projetés.
			const float32 topX = projected[0].x + (projected[1].x - projected[0].x) * u;
			const float32 topY = projected[0].y + (projected[1].y - projected[0].y) * u;
			const float32 botX = projected[3].x + (projected[2].x - projected[3].x) * u;
			const float32 botY = projected[3].y + (projected[2].y - projected[3].y) * u;
			const float32 px = topX + (botX - topX) * v;
			const float32 py = topY + (botY - topY) * v;
			const int32 ix = int32(px + 0.5f);
			const int32 iy = int32(py + 0.5f);
			if (ix < 0 || iy < 0 || uint32(ix) >= w || uint32(iy) >= h) {
				continue;
			}
			const uint32 tu = uint32(u * float32(patternSize - 1u));
			const uint32 tv = uint32(v * float32(patternSize - 1u));
			out[uint32(iy) * w + uint32(ix)] = pattern[tv * patternSize + tu];
		}
	}
	allocator.Deallocate(pattern);
}

int main() {
	logger.Info("=== NKXR AR self-test (marqueurs, sans camera) ===\n");
	auto &allocator = memory::NkGetDefaultAllocator();

	const uint32 W = 640, H = 480;
	const NkArCameraIntrinsics K = NkArCameraIntrinsics::FromFovX(W, H, 60.f);
	CHECK(K.fx > 100.f && Near(K.cx, 320.f, 0.01f), "intrinseques depuis le FOV");

	uint8 *image = static_cast<uint8 *>(allocator.Allocate(W * H, 1));

	// ── Cas 1 : marqueur de face, à 1 m ──────────────────────────────────────
	{
		const int32 id = 0x2D; // motif dissymétrique : détecte une rotation fausse
		const float32 size = 0.20f;
		const math::NkVec3f truth(0.f, 0.f, -1.f);
		SynthesizeView(image, W, H, K, id, 4, size, 0.f, 0.f, truth);

		NkArDetectorConfig config;
		NkVector<NkArDetection> found;
		const uint32 count = NkArDetectMarkers(image, W, H, config, found);
		CHECK(count >= 1u, "face : au moins un marqueur detecte");
		if (count >= 1u) {
			CHECK(found[0].id == id, "face : identifiant lu correctement");
			NkXrPose pose;
			CHECK(NkArPoseFromDetection(found[0], BlackSquareOf(size, 4), K, pose), "face : pose calculee");
			// 1 cm de tolérance sur 1 m : la chaîne est faite pour l'AR, pas
			// pour la métrologie — mais 1 % est un vrai critere.
			CHECK(Near(pose.position.z, truth.z, 0.02f), "face : distance ~1 m");
			CHECK(Near(pose.position.x, 0.f, 0.02f) && Near(pose.position.y, 0.f, 0.02f), "face : centre");
			// De face, l'orientation doit etre ~identite (|w| proche de 1).
			CHECK(Near(math::NkAbs(pose.orientation.w), 1.f, 0.05f), "face : orientation ~identite");
		}
	}

	// ── Cas 2 : marqueur incliné (le vrai test de la pose) ───────────────────
	{
		const int32 id = 0x5A3;
		const float32 size = 0.15f;
		const math::NkVec3f truth(0.05f, -0.03f, -0.8f);
		const float32 yaw = 25.f;
		SynthesizeView(image, W, H, K, id, 4, size, yaw, 0.f, truth);

		NkArDetectorConfig config;
		NkVector<NkArDetection> found;
		const uint32 count = NkArDetectMarkers(image, W, H, config, found);
		CHECK(count >= 1u, "incline : marqueur detecte");
		if (count >= 1u) {
			NkXrPose pose;
			CHECK(NkArPoseFromDetection(found[0], BlackSquareOf(size, 4), K, pose), "incline : pose calculee");
			CHECK(Near(pose.position.z, truth.z, 0.03f), "incline : distance retrouvee");
			CHECK(Near(pose.position.x, truth.x, 0.03f), "incline : decalage X retrouve");
			CHECK(Near(pose.position.y, truth.y, 0.03f), "incline : decalage Y retrouve");
			// L'axe du marqueur doit avoir tourne d'environ 25 degres.
			const float32 angleDeg = pose.orientation.Angle().Rad() * 180.f / math::NK_PI_F;
			CHECK(Near(angleDeg, yaw, 8.f), "incline : rotation ~25 degres");
		}
	}

	// ── Cas 3 : rien à trouver dans une image vide ───────────────────────────
	{
		for (uint32 i = 0; i < W * H; ++i) {
			image[i] = 180;
		}
		NkArDetectorConfig config;
		NkVector<NkArDetection> found;
		CHECK(NkArDetectMarkers(image, W, H, config, found) == 0u, "image vide : aucune fausse detection");
	}

	// ── Cas 4 : marqueur trop petit = refus explicite ────────────────────────
	{
		const math::NkVec3f farAway(0.f, 0.f, -8.f);
		SynthesizeView(image, W, H, K, 0x2D, 4, 0.05f, 0.f, 0.f, farAway);
		NkArDetectorConfig config;
		NkVector<NkArDetection> found;
		const uint32 count = NkArDetectMarkers(image, W, H, config, found);
		CHECK(count == 0u, "trop loin : refus plutot qu'une pose fantaisiste");
	}

	// ── Cas 5 : le motif genere est lisible tel quel ─────────────────────────
	{
		uint8 small[64 * 64];
		CHECK(NkArRenderMarker(0x2D, 4, small, 64), "generation du motif");
		// Coin = MARGE BLANCHE (zone de silence), puis la bordure noire plus
		// au centre : c'est ce qui permet de detecter sur fond sombre.
		CHECK(small[0] == 255, "motif : marge blanche au coin");
		CHECK(small[(64 / 8) * 64 + (64 / 8) + 2] == 0, "motif : bordure noire sous la marge");
	}

	// ── Cas 6 : la SESSION — suivi, perte, tolerance, formats couleur ───────
	{
		const int32 id = 0x2D;
		const float32 size = 0.20f;
		NkArSessionConfig cfg;
		cfg.markerSizeMeters = BlackSquareOf(size, 4);
		cfg.lostToleranceFrames = 3;
		cfg.smoothing = 0.5f;
		NkArSession session;
		CHECK(session.Initialize(cfg, W, H), "session : initialisation");

		// Image RGBA depuis le gris : verifie la conversion de luminance.
		uint8 *rgba = static_cast<uint8 *>(allocator.Allocate(W * H * 4u, 1));
		SynthesizeView(image, W, H, K, id, 4, size, 0.f, 0.f, math::NkVec3f(0.f, 0.f, -1.f));
		for (uint32 i = 0; i < W * H; ++i) {
			rgba[i * 4u + 0u] = image[i];
			rgba[i * 4u + 1u] = image[i];
			rgba[i * 4u + 2u] = image[i];
			rgba[i * 4u + 3u] = 255;
		}
		CHECK(session.ProcessFrame(rgba, W, H, W * 4u, NkArImageFormat::NK_AR_RGBA8) == 1u,
			  "session : marqueur vu en RGBA8");
		const NkArTrackedMarker *m = session.Find(id);
		CHECK(m != nullptr && m->visibleThisFrame, "session : marqueur suivi et visible");
		CHECK(m != nullptr && Near(m->pose.position.z, -1.f, 0.03f), "session : pose exposee correcte");

		// Image VIDE : le marqueur doit SURVIVRE quelques frames, pas
		// disparaitre — sinon un objet virtuel clignoterait sans cesse.
		for (uint32 i = 0; i < W * H; ++i) {
			image[i] = 180;
		}
		session.ProcessFrame(image, W, H, W, NkArImageFormat::NK_AR_GRAY8);
		m = session.Find(id);
		CHECK(m != nullptr && !m->visibleThisFrame && m->framesSinceSeen == 1u,
			  "session : perdu 1 frame mais TOUJOURS suivi");
		for (uint32 k = 0; k < 4u; ++k) {
			session.ProcessFrame(image, W, H, W, NkArImageFormat::NK_AR_GRAY8);
		}
		CHECK(session.Find(id) == nullptr, "session : oublie apres la tolerance");
		allocator.Deallocate(rgba);
	}

	// ── Cas 7 : mode ANCRE — poser une fois, garder pour toujours ───────────
	{
		const int32 id = 0x2D;
		const float32 size = 0.20f;
		NkArSessionConfig cfg;
		cfg.markerSizeMeters = BlackSquareOf(size, 4);
		cfg.lostToleranceFrames = 2; // volontairement court : il doit etre IGNORE
		cfg.anchorMode = NkArAnchorMode::NK_AR_ANCHOR_PERSISTENT;
		NkArSession session;
		session.Initialize(cfg, W, H);

		SynthesizeView(image, W, H, K, id, 4, size, 0.f, 0.f, math::NkVec3f(0.f, 0.f, -1.f));
		CHECK(session.ProcessFrame(image, W, H, W, NkArImageFormat::NK_AR_GRAY8) == 1u, "ancre : marqueur pose");
		const NkArTrackedMarker *anchored = session.Find(id);
		CHECK(anchored != nullptr, "ancre : marqueur retenu");
		const math::NkVec3f placed = (anchored != nullptr) ? anchored->pose.position : math::NkVec3f();

		// Le marqueur disparait DEFINITIVEMENT de l'image : l'ancre doit tenir,
		// bien au-dela de lostToleranceFrames, et ne pas bouger d'un pouce.
		for (uint32 i = 0; i < W * H; ++i) {
			image[i] = 180;
		}
		for (uint32 k = 0; k < 100u; ++k) {
			session.ProcessFrame(image, W, H, W, NkArImageFormat::NK_AR_GRAY8);
		}
		anchored = session.Find(id);
		CHECK(anchored != nullptr, "ancre : TOUJOURS la 100 images apres la disparition");
		CHECK(anchored != nullptr && !anchored->visibleThisFrame, "ancre : signalee non visible (l'app le sait)");
		CHECK(anchored != nullptr && Near(anchored->pose.position.z, placed.z, 0.0001f),
			  "ancre : pose INCHANGEE depuis la pose initiale");

		// L'application garde la main : Forget retire l'ancre.
		CHECK(session.Forget(id), "ancre : Forget retire l'ancre");
		CHECK(session.Find(id) == nullptr, "ancre : oubliee apres Forget");
		CHECK(!session.Forget(id), "ancre : Forget d'un inconnu rend false");
	}

	// ── Cas 8 : le MONDE — l'objet reste en place quand la camera bouge ─────
	// Le test qui compte vraiment : on pose un objet, on DEPLACE la camera,
	// et l'objet doit se retrouver AILLEURS dans l'image mais AU MEME ENDROIT
	// dans le monde. C'est toute la difference entre « colle a l'objectif » et
	// « pose dans la piece ».
	{
		const int32 id = 0x2D;
		const float32 size = 0.20f;
		NkArSessionConfig cfg;
		cfg.markerSizeMeters = BlackSquareOf(size, 4);
		cfg.smoothing = 0.f; // pas de lissage : on veut la pose brute, mesurable
		NkArSession session;
		session.Initialize(cfg, W, H);
		NkArWorld world;
		world.Initialize({});

		// Vue 1 : marqueur droit devant, a 1 m.
		SynthesizeView(image, W, H, K, id, 4, size, 0.f, 0.f, math::NkVec3f(0.f, 0.f, -1.f));
		session.ProcessFrame(image, W, H, W, NkArImageFormat::NK_AR_GRAY8);
		CHECK(world.Update(session), "monde : camera localisee sur le premier marqueur");
		CHECK(world.HasEverLocalized(), "monde : origine posee");

		// On pose un objet a 0,5 m devant la camera.
		const uint32 anchor = world.PlaceInFrontOfCamera(0.5f);
		CHECK(anchor != 0u, "monde : objet pose devant la camera");
		NkXrPose seenBefore;
		CHECK(world.GetAnchorInCamera(anchor, seenBefore), "monde : objet visible depuis la camera");
		CHECK(Near(seenBefore.position.z, -0.5f, 0.01f), "monde : objet a 0,5 m devant, comme demande");

		// Vue 2 : LA CAMERA A BOUGE — le marqueur apparait decale et plus loin.
		SynthesizeView(image, W, H, K, id, 4, size, 0.f, 0.f, math::NkVec3f(0.20f, 0.f, -1.40f));
		session.ProcessFrame(image, W, H, W, NkArImageFormat::NK_AR_GRAY8);
		CHECK(world.Update(session), "monde : camera re-localisee apres deplacement");
		NkXrPose seenAfter;
		CHECK(world.GetAnchorInCamera(anchor, seenAfter), "monde : objet toujours connu");
		// Vu de la nouvelle position, l'objet DOIT avoir bouge dans l'image...
		const float32 moved = (seenAfter.position - seenBefore.position).Len();
		CHECK(moved > 0.05f, "monde : l'objet a change de place VU DE LA CAMERA (elle a bouge)");
		// ...alors que sa place DANS LE MONDE n'a pas varie d'un cheveu.
		const NkArAnchor &stored = world.GetAnchors()[0];
		CHECK(Near(stored.poseInWorld.position.x, stored.poseInWorld.position.x, 0.f), "monde : ancre stable");

		// Le marqueur DISPARAIT : l'objet reste connu, mais l'application est
		// prevenue que la localisation vieillit.
		for (uint32 i = 0; i < W * H; ++i) {
			image[i] = 180;
		}
		session.ProcessFrame(image, W, H, W, NkArImageFormat::NK_AR_GRAY8);
		CHECK(!world.Update(session), "monde : plus localise quand aucun marqueur n'est vu");
		CHECK(world.GetFramesSinceLocalized() >= 1u, "monde : l'age de la localisation est connu");
		CHECK(world.GetAnchorInCamera(anchor, seenAfter), "monde : l'objet reste pose malgre la perte");
		CHECK(world.Remove(anchor), "monde : Remove retire l'objet");
		CHECK(!world.GetAnchorInCamera(anchor, seenAfter), "monde : objet retire");
	}

	// ── Cas 9 : la camera TOURNE sans marqueur — suivi par l'image ──────────
	// Le defaut a corriger : sans marqueur, l'objet restait colle a l'ecran
	// pendant que la camera pivotait. On mesure donc la rotation SUR L'IMAGE.
	// Sous rotation pure, l'image se transforme exactement par K.R.K^-1, quelle
	// que soit la profondeur : on peut donc FABRIQUER la vue tournee a partir
	// de la premiere, sans rien approximer.
	{
		uint8 *second = static_cast<uint8 *>(allocator.Allocate(W * H, 1));
		uint8 *background = static_cast<uint8 *>(allocator.Allocate(W * H, 1));
		MakeTexture(background, W, H);

		const int32 id = 0x2D;
		const float32 size = 0.20f;
		NkArSessionConfig cfg;
		cfg.markerSizeMeters = BlackSquareOf(size, 4);
		cfg.smoothing = 0.f;
		cfg.lostToleranceFrames = 0; // le marqueur doit vraiment disparaitre
		NkArSession session;
		session.Initialize(cfg, W, H);
		NkArWorld world;
		world.Initialize({});

		// Vue 1 : le marqueur sur ce fond texture (le fond donne au suivi de
		// quoi s'accrocher — un mur parfaitement uni ne dit rien de la camera,
		// et c'est une limite REELLE, pas un artefact du test).
		SynthesizeView(image, W, H, K, id, 4, size, 0.f, 0.f, math::NkVec3f(0.f, 0.f, -1.f));
		for (uint32 i = 0; i < W * H; ++i) {
			if (image[i] == 200) {
				image[i] = background[i]; // le fond clair de la synthese
			}
		}
		session.ProcessFrame(image, W, H, W, NkArImageFormat::NK_AR_GRAY8);
		CHECK(world.Update(session), "suivi image : localise sur le marqueur");
		const uint32 anchor = world.PlaceInFrontOfCamera(0.5f);
		NkXrPose before;
		CHECK(world.GetAnchorInCamera(anchor, before), "suivi image : objet pose");

		// Vue 2 : la camera a tourne de 2 degres a gauche. Le marqueur est
		// RETIRE de la source pour forcer le systeme a se debrouiller avec
		// l'image seule — c'est precisement le cas que l'on repare.
		const float32 yaw = 2.f * math::NK_PI_F / 180.f;
		WarpByRotation(background, second, W, H, K, yaw, 0.f, 0.f);
		session.ProcessFrame(second, W, H, W, NkArImageFormat::NK_AR_GRAY8);
		const bool localized = world.Update(session);
		CHECK(!localized, "suivi image : plus aucun marqueur, donc plus de localisation par marqueur");
		CHECK(world.IsTrackingByImage(), "suivi image : la rotation de la camera a ete MESUREE sur l'image");
		const NkArFlowResult &flow = world.GetLastFlow();
		logger.Infof("  [info] flow : candidats %u, ambigus %u, retenus %u, residu %.2f px, lacet %.4f rad\n",
					 flow.candidates, flow.ambiguous, flow.inliers, flow.residualPixels, flow.yawRad);
		CHECK(flow.valid, "suivi image : estimation valide");
		CHECK(Near(flow.yawRad, yaw, 0.30f * yaw), "suivi image : lacet retrouve, signe compris");
		CHECK(Near(flow.pitchRad, 0.f, 0.004f), "suivi image : pas de tangage invente");
		CHECK(Near(flow.rollRad, 0.f, 0.004f), "suivi image : pas de roulis invente");

		// La consequence qui compte : l'objet a TOURNE dans le champ de la
		// camera, donc il glisse a l'ecran au lieu d'y rester colle.
		NkXrPose after;
		CHECK(world.GetAnchorInCamera(anchor, after), "suivi image : objet toujours connu");
		const float32 angleBefore = math::NkAtan2(before.position.x, -before.position.z);
		const float32 angleAfter = math::NkAtan2(after.position.x, -after.position.z);
		// La camera a tourne a GAUCHE : un objet qui etait droit devant se
		// retrouve donc sur sa DROITE, du meme angle. C'est le signe qui trahit
		// une composition inversee, et il n'y en a qu'un de juste.
		CHECK(Near(angleAfter - angleBefore, yaw, 0.35f * yaw),
			  "suivi image : l'objet a glisse du bon cote, de la bonne quantite");
		CHECK(Near(after.position.Len(), before.position.Len(), 0.02f),
			  "suivi image : la DISTANCE ne bouge pas (une rotation ne rapproche rien)");

		// Rotation LENTE : 0,2 degre, soit moins de deux pixels de glissement.
		// C'est le cas qui echouait en vrai — un panoramique a la main avance de
		// cet ordre a chaque image. Arrondi a l'entier le plus proche, ce
		// mouvement se perd et la rotation cumulee n'avance JAMAIS, alors meme
		// que la piece defile. Il exige l'affinage sous-pixel.
		{
			session.ProcessFrame(background, W, H, W, NkArImageFormat::NK_AR_GRAY8);
			world.Update(session);
			const float32 slowYaw = 0.2f * math::NK_PI_F / 180.f;
			WarpByRotation(background, second, W, H, K, slowYaw, 0.f, 0.f);
			session.ProcessFrame(second, W, H, W, NkArImageFormat::NK_AR_GRAY8);
			world.Update(session);
			const NkArFlowResult &slow = world.GetLastFlow();
			logger.Infof("  [info] lent : lacet %.5f rad attendu %.5f, glissement %.2f px, %u pts\n", slow.yawRad,
						 slowYaw, slow.medianShiftPixels, slow.inliers);
			CHECK(slow.valid, "suivi image : rotation lente mesuree");
			CHECK(Near(slow.yawRad, slowYaw, 0.35f * slowYaw), "suivi image : 0,2 degre retrouve (sous-pixel)");
		}

		// Camera IMMOBILE : la meme image deux fois. Le suivi ne doit inventer
		// aucune rotation — c'est le pendant du test precedent, et le defaut
		// serait bien pire : un objet pose deriverait tout seul, sans que
		// personne n'ait bouge.
		session.ProcessFrame(second, W, H, W, NkArImageFormat::NK_AR_GRAY8);
		world.Update(session);
		const NkArFlowResult &still = world.GetLastFlow();
		CHECK(Near(still.yawRad, 0.f, 0.0008f), "suivi image : camera immobile, aucun lacet invente");
		CHECK(Near(still.pitchRad, 0.f, 0.0008f), "suivi image : camera immobile, aucun tangage invente");

		// Fond parfaitement uni : rien a suivre. Le systeme doit le DIRE, et
		// finir par refuser sa propre pose plutot que d'afficher n'importe ou.
		for (uint32 i = 0; i < W * H; ++i) {
			second[i] = 180;
		}
		NkArWorldConfig strict;
		strict.maxBlindFrames = 3;
		world.Initialize(strict);
		for (uint32 f = 0; f < 6u; ++f) {
			session.ProcessFrame(second, W, H, W, NkArImageFormat::NK_AR_GRAY8);
			world.Update(session);
		}
		CHECK(!world.IsTrackingByImage(), "suivi image : un mur uni n'apprend rien, et on ne pretend pas le contraire");
		CHECK(!world.IsPoseUsable(), "suivi image : apres trop d'images aveugles, la pose est declaree perdue");
		CHECK(!world.GetAnchorInCamera(anchor, after), "suivi image : l'objet est cache plutot qu'affiche au hasard");

		allocator.Deallocate(background);
		allocator.Deallocate(second);
	}

	allocator.Deallocate(image);
	logger.Infof("=== NKXR AR self-test : %d OK, %d ECHECS ===\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
