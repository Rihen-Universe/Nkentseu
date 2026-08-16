// =============================================================================
// bench_ar_flow.cpp — Ce que vaut `NkArImageFlow`, mesuré séparément.
//
// POURQUOI : les 748 lignes de `NkArFlow` tournent sur le téléphone depuis le
// 13 août, mais **n'ont jamais été mesurées seules**. Elles sont pourtant ce
// qui tient la scène entre deux marqueurs — donc ce qu'un étudiant verra en
// premier quand il détournera la caméra du marqueur. On ne peut pas promettre
// ce qu'on n'a pas mesuré.
//
// LE PROTOCOLE, et sa limite écrite avant ses chiffres :
//   - image RÉELLE prise par la caméra du téléphone (`nkar_diag_gris.png`,
//     1280×720) — vraie texture, vrai bruit de capteur, vrai éclairage ;
//   - rotations SIMULÉES par décalage ENTIER de pixels. Sous le modèle
//     sténopé, une rotation de lacet θ déplace l'image de Δx = fx·tan(θ) ;
//     on impose donc Δx entier et on en déduit la vérité θ = atan(Δx/fx).
//   - le suivi ne voit que deux images ; on compare ce qu'il rend à ce qu'on
//     a imposé.
//
// ⚠️ CE QUE CE BANC NE MESURE PAS, et il faut le lire avant les résultats :
//   un décalage entier est le cas le PLUS FAVORABLE — pas de rééchantillonnage,
//   donc pas de flou d'interpolation. Le monde réel ajoute le flou de bougé,
//   l'obturateur déroulant, les changements d'éclairage et la parallaxe d'une
//   translation. **Les chiffres ci-dessous sont une BORNE SUPÉRIEURE de la
//   précision, pas une performance de terrain.**
// =============================================================================

#include "NKXR/AR/NkArFlow.h"
#include "NKImage/Core/NkImage.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::xr;

namespace {

	// Décale l'image de dx pixels vers la droite ; la bande découverte est
	// remplie par recopie du bord, ce qui n'introduit aucune texture inventée.
	void DecalerX(const uint8 *src, uint8 *dst, uint32 w, uint32 h, int32 dx) {
		for (uint32 y = 0; y < h; ++y) {
			const uint8 *ls = src + (usize)y * w;
			uint8 *ld = dst + (usize)y * w;
			for (uint32 x = 0; x < w; ++x) {
				int32 sx = (int32)x - dx;
				if (sx < 0)
					sx = 0;
				if (sx >= (int32)w)
					sx = (int32)w - 1;
				ld[x] = ls[sx];
			}
		}
	}

} // namespace

int main() {
	NkImage img;
	if (!img.LoadFromFile("nkar_diag_gris.png") || img.Width() == 0) {
		std::printf("ECHEC : image de reference introuvable (nkar_diag_gris.png)\n");
		return 2;
	}
	const uint32 w = (uint32)img.Width();
	const uint32 h = (uint32)img.Height();
	const uint32 canaux = (uint32)img.Channels();

	NkVector<uint8> gris;
	gris.Resize((usize)w * h);
	const uint8 *px = img.Pixels();
	for (usize i = 0; i < (usize)w * h; ++i) {
		if (canaux >= 3)
			gris[i] = (uint8)(((uint32)px[i * canaux] * 77 + (uint32)px[i * canaux + 1] * 150 +
							   (uint32)px[i * canaux + 2] * 29) >> 8);
		else
			gris[i] = px[i * canaux];
	}

	// Intrinsèques mesurées sur ce téléphone le 13/08 (calibration de Zhang,
	// erreur de reprojection 1,83 px). Utiliser un champ supposé fausserait la
	// conversion pixels -> angle, donc la vérité elle-même.
	NkArCameraIntrinsics k;
	k.fx = 918.9f;
	k.fy = 923.5f;
	k.cx = (float32)w * 0.5f;
	k.cy = (float32)h * 0.5f;

	std::printf("Image reelle %ux%u, fx=%.1f (calibration mesuree du 13/08)\n\n", w, h, k.fx);
	std::printf("  decalage    verite    mesure    erreur   points   residu   valide\n");
	std::printf("  (pixels)     (deg)     (deg)     (deg)                (px)\n");

	const int32 decalages[] = { 1, 2, 4, 8, 16, 32, 48, 64, 96, 128 };
	int echecs = 0;
	int valides = 0;

	NkVector<uint8> decalee;
	decalee.Resize((usize)w * h);

	for (int32 dx : decalages) {
		NkArImageFlow flux;
		NkArFlowConfig cfg;
		flux.Initialize(cfg);

		// 1re image : la référence. 2e : la même, décalée d'une valeur connue.
		(void)flux.Track(gris.Data(), w, h, k);
		DecalerX(gris.Data(), decalee.Data(), w, h, dx);
		const NkArFlowResult r = flux.Track(decalee.Data(), w, h, k);

		// SIGNE — corrigé le 2026-08-17, après m'être trompé une première fois.
		// Le contenu décalé vers la DROITE signifie que la caméra a tourné vers
		// la GAUCHE (tournez la tête à gauche : la scène défile à droite). Or
		// `NkArFlowResult::yawRad` déclare « tourner à gauche > 0 ». La vérité
		// est donc POSITIVE, et mon premier signe était faux — pas le code.
		const float32 veriteDeg = atanf((float32)dx / k.fx) * 180.f / 3.14159265f;
		const float32 mesureDeg = r.yawRad * 180.f / 3.14159265f;
		const float32 erreurDeg = r.valid ? (mesureDeg - veriteDeg) : 0.f;

		std::printf("  %8d  %8.3f  %8.3f  %+8.3f  %7u  %7.2f   %s\n", dx, veriteDeg,
					r.valid ? mesureDeg : 0.f, r.valid ? erreurDeg : 0.f, r.inliers, r.residualPixels,
					r.valid ? "oui" : "NON");

		if (r.valid) {
			++valides;
			// Tolérance : 15 % de l'angle imposé, plancher à 0,05 deg pour les
			// tout petits décalages où la quantification domine.
			const float32 tol = (veriteDeg < 0.f ? -veriteDeg : veriteDeg) * 0.15f + 0.05f;
			const float32 err = erreurDeg < 0.f ? -erreurDeg : erreurDeg;
			if (err > tol)
				++echecs;
		}
	}

	std::printf("\n%d/%d decalages suivis, %d hors tolerance (15%% + 0,05 deg).\n", valides,
				(int)(sizeof(decalages) / sizeof(decalages[0])), echecs);
	std::printf("RAPPEL : decalages ENTIERS, donc cas le plus favorable. Le terrain ajoute\n");
	std::printf("le flou de bouge, l'obturateur deroulant et la parallaxe. BORNE SUPERIEURE.\n");
	return 0;
}
