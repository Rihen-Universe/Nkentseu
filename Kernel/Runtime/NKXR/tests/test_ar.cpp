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

static bool Near(float32 a, float32 b, float32 e) {
	const float32 d = a - b;
	return (d < 0 ? -d : d) <= e;
}

// Rend l'image qu'une caméra d'intrinsèques K verrait d'un marqueur carré placé
// à la pose donnée (rotation autour de Y puis X, translation).
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
			CHECK(NkArPoseFromDetection(found[0], size, K, pose), "face : pose calculee");
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
			CHECK(NkArPoseFromDetection(found[0], size, K, pose), "incline : pose calculee");
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
		CHECK(small[0] == 0 && small[63] == 0, "motif : bordure noire");
	}

	// ── Cas 6 : la SESSION — suivi, perte, tolerance, formats couleur ───────
	{
		const int32 id = 0x2D;
		const float32 size = 0.20f;
		NkArSessionConfig cfg;
		cfg.markerSizeMeters = size;
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

	allocator.Deallocate(image);
	logger.Infof("=== NKXR AR self-test : %d OK, %d ECHECS ===\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
