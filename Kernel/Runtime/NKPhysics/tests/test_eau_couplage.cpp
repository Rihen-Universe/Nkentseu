// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_eau_couplage.cpp — LES CORPS ET L'EAU S'INFLUENCENT : les témoins des
// deux moitiés du couplage (2026-09-14).
//
// Rodolf a signalé DEUX fois que le couplage manque : « quand c'est calme on ne
// ressent pas l'impact du cube en mouvement sur l'eau, pareil pour les sphères
// quand l'océan est en mouvement ». Les deux sens manquaient, et c'est cohérent.
//
//   MOITIÉ A — l'eau agit sur le corps   -> NKMath/NkBuoyancy.h
//   MOITIÉ B — le corps agit sur l'eau   -> NKMath/NkWaterDisturbance.h
//
// ⚠️ POURQUOI ICI : `NKPhysics_Tests` lie déjà NKMath, il est DÉJÀ dans la liste
// `allow` de dutc/dute (`Nkentseu.jenga`), et `tests/**.cpp` le ramasse sans
// toucher au fichier jenga. Aucune cible neuve. C'est l'argument que
// `test_eau.cpp` a écrit lui-même le 06/09, et je le suis au lieu d'en inventer
// un autre.
//
// ── L'ORDRE DES TÉMOINS, ET POURQUOI IL COMMENCE PAR L'INSTRUMENT ───────────
// (i*) L'INSTRUMENT AVANT LE PREMIER CHIFFRE. Un compteur de différences de bits
//      qui rendrait zéro parce qu'il ne sait pas compter validerait tout. Il est
//      donc prouvé sur ZÉRO (deux évaluations identiques) ET sur du NON-ZÉRO
//      (deux instants différents) avant de servir. Et le volume immergé est
//      confronté à une QUADRATURE indépendante, pas à lui-même.
// (f*) MOITIÉ A, chacun avec son négatif.
// (p*) MOITIÉ B, dont le NÉGATIF CAPITAL (p1n) : sans corps, la surface vaut la
//      valeur analytique AU BIT. Sans lui, une perturbation qui fuit d'un ULP
//      serait invisible à l'œil et fausserait tout le reste.
// (m*) LES MUTATIONS : `NK_EAU_MUT=1|2|3` casse volontairement UNE chose et le
//      banc dit quels témoins rougissent. Une garde verte peut ne rien garder.
//
// ── LES IMAGES ──────────────────────────────────────────────────────────────
// `NK_EAU_IMAGES=<dossier>` fait écrire des BMP 24 bits : Rodolf doit VOIR
// l'avant et l'après, pas lire un chiffre vert. Rendus CPU, sans fenêtre ni
// device — le précédent est `Captures/fumee_colonne_2026-09-05.png`. Un
// `LISEZMOI.md` est écrit à côté et dit ce que chaque image prouve ET ce qu'elle
// ne prouve pas.
//
// Les chiffres partent sur stderr, comme `test_cloth.cpp` et `test_eau.cpp`, et
// pour la même raison mesurée : le puits console de NKLogger ne traverse pas un
// tuyau dans cet arbre.
// =============================================================================
#include "NKMath/NkBuoyancy.h"
#include "NKMath/NkFunctions.h"
#include "NKMath/NkWaterDisturbance.h"
#include "NKMath/NkWaterSurface.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::math;

namespace {

	int *gPass = nullptr;
	int *gFail = nullptr;
	// Le zero de reference pour les comparaisons AU BIT. Une constante, et non un
	// litteral dans l'appel : `memcmp` veut une adresse, et prendre celle d'un
	// temporaire est exactement le genre de detour qui finit par comparer autre
	// chose que ce qu'on croit.
	const float32 kZeroF = 0.f;
	int gMutation = 0; // NK_EAU_MUT
	const char *gImageDir = nullptr;

	void ECHECK(bool ok, const char *what) {
		if (ok) {
			++(*gPass);
			std::fprintf(stderr, "  [ok]    %s\n", what);
		} else {
			++(*gFail);
			std::fprintf(stderr, "  [ROUGE] %s\n", what);
		}
	}

	// ─────────────────────────────────────────────────────────────────────
	// L'INSTRUMENT : compter les DIFFÉRENCES DE BITS entre deux points d'eau.
	// `memcmp` champ par champ, jamais `==` : `NaN != NaN` et `-0.f == +0.f`
	// rendraient un comparateur flottant aveugle exactement là où on regarde.
	// ─────────────────────────────────────────────────────────────────────
	uint32 BitsDiff(const NkWaterPoint &a, const NkWaterPoint &b) noexcept {
		const float32 fa[] = {a.position.x, a.position.y, a.position.z, a.normal.x,	  a.normal.y,
							  a.normal.z,	a.jxx,		  a.jxz,		a.jzx,		  a.jzz,
							  a.jacobianXZ, a.tangent.x,  a.tangent.y,	a.tangent.z,  a.normalFast.x,
							  a.normalFast.y, a.normalFast.z};
		const float32 fb[] = {b.position.x, b.position.y, b.position.z, b.normal.x,	  b.normal.y,
							  b.normal.z,	b.jxx,		  b.jxz,		b.jzx,		  b.jzz,
							  b.jacobianXZ, b.tangent.x,  b.tangent.y,	b.tangent.z,  b.normalFast.x,
							  b.normalFast.y, b.normalFast.z};
		uint32 n = 0;
		for (uint32 i = 0; i < 17u; ++i)
			if (std::memcmp(&fa[i], &fb[i], sizeof(float32)) != 0)
				++n;
		return n;
	}

	// Une houle de référence, fabriquée une fois : les témoins qui la partagent
	// comparent la même chose. `steepness = 0` -> Q = 0 -> AUCUN déplacement
	// horizontal : la surface est une sinusoïde pure en y, ce qui rend la borne
	// analytique du flotteur écrivable AVANT la mesure.
	NkWaterParams HouleSinus(float32 amplitude, float32 longueur) noexcept {
		NkWaterParams p;
		p.waveCount = 1u;
		p.waves[0].amplitude = amplitude;
		p.waves[0].wavelength = longueur;
		p.waves[0].direction = {1.f, 0.f};
		p.waves[0].steepness = 0.f;
		p.waves[0].phase = 0.f;
		p.depth = 0.f; // eau profonde
		return p;
	}

	NkWaterParams HouleCalme() noexcept {
		NkWaterParams p;
		p.waveCount = 0u; // pas un seul train : la surface vaut 0 partout, au bit
		return p;
	}

	// ─────────────────────────────────────────────────────────────────────
	// ÉCRITURE BMP 24 bits — aucune dépendance (NKPhysics_Tests ne lie pas
	// NKImage). Lignes du BAS vers le HAUT et remplissage à 4 octets : les deux
	// pièges du format, écrits ici pour qu'ils ne soient pas re-payés ailleurs.
	// ─────────────────────────────────────────────────────────────────────
	bool EcrireBMP(const char *chemin, const unsigned char *rgb, uint32 w, uint32 h) {
		std::FILE *f = std::fopen(chemin, "wb");
		if (f == nullptr)
			return false;
		const uint32 pad = (4u - ((w * 3u) & 3u)) & 3u;
		const uint32 dataSize = (w * 3u + pad) * h;
		const uint32 fileSize = 54u + dataSize;
		unsigned char hdr[54];
		std::memset(hdr, 0, sizeof(hdr));
		hdr[0] = 'B';
		hdr[1] = 'M';
		std::memcpy(hdr + 2, &fileSize, 4);
		const uint32 offset = 54u;
		std::memcpy(hdr + 10, &offset, 4);
		const uint32 dib = 40u;
		std::memcpy(hdr + 14, &dib, 4);
		std::memcpy(hdr + 18, &w, 4);
		std::memcpy(hdr + 22, &h, 4);
		const uint16 planes = 1u, bpp = 24u;
		std::memcpy(hdr + 26, &planes, 2);
		std::memcpy(hdr + 28, &bpp, 2);
		std::memcpy(hdr + 34, &dataSize, 4);
		std::fwrite(hdr, 1, 54, f);
		const unsigned char zero[3] = {0, 0, 0};
		for (uint32 y = 0; y < h; ++y) {
			const uint32 src = (h - 1u - y) * w * 3u; // BMP : la première ligne du fichier est le BAS
			for (uint32 x = 0; x < w; ++x) {
				const unsigned char bgr[3] = {rgb[src + x * 3u + 2u], rgb[src + x * 3u + 1u],
											  rgb[src + x * 3u + 0u]};
				std::fwrite(bgr, 1, 3, f);
			}
			if (pad != 0u)
				std::fwrite(zero, 1, pad, f);
		}
		std::fclose(f);
		return true;
	}

	// La palette : charte Rihen. Creux -> pétrole #0A555F, repos -> blanc cassé,
	// bosse -> orange #F79A28. L'ÉCHELLE est un paramètre et elle est IMPRIMÉE :
	// deux images de la même série la partagent, sinon « avant » et « après » ne
	// se comparent pas — ils se ressemblent.
	void Couleur(float32 y, float32 echelle, unsigned char *out) noexcept {
		float32 u = y / (echelle > 1e-9f ? echelle : 1.f);
		u = NkClamp(u, -1.f, 1.f);
		const float32 petrole[3] = {10.f, 85.f, 95.f};
		const float32 blanc[3] = {238.f, 240.f, 238.f};
		const float32 orange[3] = {247.f, 154.f, 40.f};
		for (uint32 c = 0; c < 3u; ++c) {
			const float32 v = u < 0.f ? blanc[c] + (petrole[c] - blanc[c]) * (-u)
									  : blanc[c] + (orange[c] - blanc[c]) * u;
			out[c] = (unsigned char)NkClamp(v, 0.f, 255.f);
		}
	}

	// Vue de DESSUS d'un carré de `etendue` mètres centré sur l'origine.
	void ImageDessus(const char *nom, const NkWaterParams &houle, const NkWaterDisturbance *champ,
					 float32 t, float32 etendue, float32 echelle) {
		if (gImageDir == nullptr)
			return;
		const uint32 N = 480u;
		unsigned char *px = (unsigned char *)std::malloc((nk_size)N * N * 3u);
		if (px == nullptr)
			return;
		for (uint32 j = 0; j < N; ++j) {
			const float32 z = -etendue * 0.5f + etendue * ((float32)j + 0.5f) / (float32)N;
			for (uint32 i = 0; i < N; ++i) {
				const float32 x = -etendue * 0.5f + etendue * ((float32)i + 0.5f) / (float32)N;
				const float32 y = NkWaterHeight(houle, x, z, t, -1.f, champ);
				Couleur(y, echelle, px + ((nk_size)j * N + i) * 3u);
			}
		}
		char chemin[512];
		std::snprintf(chemin, sizeof(chemin), "%s/%s", gImageDir, nom);
		const bool ok = EcrireBMP(chemin, px, N, N);
		std::free(px);
		std::fprintf(stderr, "     image %-28s %s  (echelle +/- %.3f m, etendue %.1f m, t = %.2f s)\n",
					 nom, ok ? "ECRITE" : "ECHEC", (double)echelle, (double)etendue, (double)t);
	}

	// Un tracé de courbes : `nbCourbes` séries de `n` valeurs, axes et zéro.
	void ImageCourbes(const char *nom, const float32 *series, uint32 nbCourbes, uint32 n, float32 ymin,
					  float32 ymax, const char *legende) {
		if (gImageDir == nullptr || n < 2u)
			return;
		const uint32 W = 900u, H = 380u;
		unsigned char *px = (unsigned char *)std::malloc((nk_size)W * H * 3u);
		if (px == nullptr)
			return;
		std::memset(px, 250, (nk_size)W * H * 3u);
		// L'axe zéro : sans lui, « le corps suit la vague » n'a pas de référence.
		if (ymax > ymin) {
			const uint32 y0 = (uint32)((ymax - 0.f) / (ymax - ymin) * (float32)(H - 1u));
			if (y0 < H)
				for (uint32 x = 0; x < W; ++x) {
					px[((nk_size)y0 * W + x) * 3u + 0u] = 200;
					px[((nk_size)y0 * W + x) * 3u + 1u] = 200;
					px[((nk_size)y0 * W + x) * 3u + 2u] = 200;
				}
		}
		const unsigned char cols[3][3] = {{10, 85, 95}, {247, 154, 40}, {120, 120, 130}};
		for (uint32 c = 0; c < nbCourbes && c < 3u; ++c) {
			for (uint32 i = 0; i < n; ++i) {
				const uint32 x = (uint32)((float32)i / (float32)(n - 1u) * (float32)(W - 1u));
				float32 v = series[(nk_size)c * n + i];
				v = NkClamp(v, ymin, ymax);
				const uint32 y = (uint32)((ymax - v) / (ymax - ymin) * (float32)(H - 1u));
				for (int32 dy = -1; dy <= 1; ++dy) {
					const int32 yy = (int32)y + dy;
					if (yy < 0 || yy >= (int32)H)
						continue;
					unsigned char *p = px + ((nk_size)yy * W + x) * 3u;
					p[0] = cols[c][0];
					p[1] = cols[c][1];
					p[2] = cols[c][2];
				}
			}
		}
		char chemin[512];
		std::snprintf(chemin, sizeof(chemin), "%s/%s", gImageDir, nom);
		const bool ok = EcrireBMP(chemin, px, W, H);
		std::free(px);
		std::fprintf(stderr, "     image %-28s %s  [%s]  y dans [%.3f ; %.3f]\n", nom,
					 ok ? "ECRITE" : "ECHEC", legende, (double)ymin, (double)ymax);
	}

	// =====================================================================
	// (i) L'INSTRUMENT, AVANT LE PREMIER CHIFFRE
	// =====================================================================
	void TemoinInstrument() {
		std::fprintf(stderr, "-- (i) l'instrument, prouve AVANT de mesurer --\n");

		// (i1) LE COMPTEUR DE BITS SUR ZERO. S'il ne sait pas rendre 0, tout le
		// reste est du bruit vert.
		const NkWaterParams h = HouleSinus(0.5f, 25.f);
		const NkWaterPoint a = NkWaterEval(h, 1.3f, -0.7f, 2.5f);
		const NkWaterPoint b = NkWaterEval(h, 1.3f, -0.7f, 2.5f);
		const uint32 zero = BitsDiff(a, b);
		std::fprintf(stderr, "     compteur sur deux evaluations IDENTIQUES : %u champs differents\n",
					 zero);
		ECHECK(zero == 0u, "(i1) LE ZERO DU COMPTEUR : deux evaluations identiques -> 0 difference");

		// (i2) ET SUR DU NON-ZERO. Un compteur qui rend toujours 0 passerait (i1).
		const NkWaterPoint c = NkWaterEval(h, 1.3f, -0.7f, 2.5f + 0.001f);
		const uint32 nz = BitsDiff(a, c);
		std::fprintf(stderr, "     compteur sur deux instants distants de 1 ms  : %u champs differents\n",
					 nz);
		ECHECK(nz > 0u, "(i2) LE COMPTEUR MORD : deux instants differents -> au moins une difference");

		// (i3) LE VOLUME IMMERGE CONTRE UNE QUADRATURE INDEPENDANTE. On ne compare
		// pas la formule a elle-meme : on tranche la sphere en 200 000 disques.
		{
			const float32 r = 0.5f, cote = -0.13f;
			const uint32 N = 200000u;
			float64 somme = 0.0;
			const float64 dy = ((float64)cote - (float64)(-r)) / (float64)N;
			for (uint32 i = 0; i < N; ++i) {
				const float64 y = (float64)(-r) + ((float64)i + 0.5) * dy;
				somme += 3.14159265358979 * ((float64)r * r - y * y) * dy;
			}
			const float32 formule = NkSphereVolumeBelow(r, cote);
			const float32 ecart = NkFabs(formule - (float32)somme) / (float32)somme;
			std::fprintf(stderr,
						 "     r = %.3f m, cote = %.3f m : formule %.9f m3, quadrature %.9f m3, "
						 "ecart relatif %.3e\n",
						 (double)r, (double)cote, (double)formule, somme, (double)ecart);
			ECHECK(ecart < 1e-5f, "(i3) le volume immerge tient contre une QUADRATURE independante");
		}

		// (i4) LES DEUX BORNES, qui ne sont pas des evidences : hors de l'eau -> 0,
		// noyee -> 4 pi r^3 / 3.
		{
			const float32 r = 0.5f;
			const float32 v0 = NkSphereVolumeBelow(r, -r);
			const float32 v1 = NkSphereVolumeBelow(r, r);
			const float32 attendu = 4.f / 3.f * 3.1415926535f * r * r * r;
			std::fprintf(stderr, "     V(c=-r) = %.9f (attendu 0)   V(c=+r) = %.9f (attendu %.9f)\n",
						 (double)v0, (double)v1, (double)attendu);
			ECHECK(v0 == 0.f && NkFabs(v1 - attendu) < 1e-6f,
				   "(i4) les deux bornes du volume : affleurement 0, sphere noyee 4 pi r^3 / 3");
		}

		// (i5) LE CENTRE DE CARENE D'UNE SPHERE NOYEE EST SON CENTRE. Un moment non
		// nul la trahirait, et rien d'autre ne le dirait.
		{
			const float32 r = 0.5f;
			const float32 m = NkSphereMomentBelow(r, r);
			std::fprintf(stderr, "     moment de la sphere entierement immergee : %.9e (attendu 0)\n",
						 (double)m);
			ECHECK(m == 0.f, "(i5) sphere noyee -> moment nul -> carene AU centre");
		}

		// (i6) LA CONDITION D'ESSAI ELLE-MEME : la mer « calme » l'est-elle ? Un
		// banc a deja mesure une pente de 10 degres sur un sol plat.
		{
			const NkWaterParams calme = HouleCalme();
			float32 pire = 0.f;
			for (uint32 i = 0; i < 500u; ++i) {
				const float32 x = -25.f + 0.1f * (float32)i;
				const float32 y = NkWaterHeight(calme, x, 0.37f * (float32)i, 1.234f * (float32)i);
				if (NkFabs(y) > pire)
					pire = NkFabs(y);
			}
			std::fprintf(stderr, "     mer calme, 500 points et 500 instants : |y| max = %.9e\n",
						 (double)pire);
			ECHECK(pire == 0.f, "(i6) la mer calme du banc est plate A ZERO EXACT, pas 'presque'");
		}
	}

	// =====================================================================
	// MOITIE A — L'EAU AGIT SUR LE CORPS
	// =====================================================================

	// Un flotteur integre a la main (Euler semi-implicite). Pourquoi pas
	// `NkPhysicsWorld` : il ferait entrer la detection de collision, le solveur de
	// contacts et le sommeil dans une mesure qui ne porte que sur UNE force. La
	// porte `NkRigidBody::ApplyForce` existe et elle est nommee dans
	// `NkBuoyancy.h` ; ce banc mesure la FORCE, pas le cablage.
	struct Flotteur {
			NkVec3f position = {0.f, 0.f, 0.f};
			NkVec3f vitesse = {0.f, 0.f, 0.f};
			float32 rayon = 0.5f;
			float32 masse = 1.f;
	};

	void PasFlotteur(Flotteur &f, const NkBuoyancyParams &bp, const NkWaterParams &houle,
					 const NkWaterDisturbance *champ, float32 t, float32 dt) {
		const NkBuoyancyResult r =
			NkBuoyancySphere(bp, houle, f.position, f.rayon, f.vitesse, t, champ);
		const NkVec3f poids = {0.f, -f.masse * bp.gravity, 0.f};
		const NkVec3f acc = (r.force + poids) * (1.f / f.masse);
		f.vitesse = f.vitesse + acc * dt;
		f.position = f.position + f.vitesse * dt;
	}

	void TemoinFlottabilite() {
		std::fprintf(stderr, "-- (f) MOITIE A : l'eau agit sur le corps --\n");

		const float32 rayon = 0.5f;
		const float32 densiteEau = (gMutation == 3) ? 0.f : 1000.f; // MUTATION 3 : plus de poussee
		const float32 densiteCorps = 400.f;						   // moins dense que l'eau -> il flotte
		const float32 vTotal = 4.f / 3.f * 3.1415926535f * rayon * rayon * rayon;
		const float32 masse = densiteCorps * vTotal;

		// ── L'ATTENDU, ECRIT AVANT LA MESURE ET DERIVE ──────────────────────
		// Archimede : a l'equilibre, poussee = poids, donc rho g V = m g, donc
		//        V_immerge = masse / densite_eau
		// C'est un enonce PHYSIQUE, pas une relecture du code. On resout ici la
		// cote `c` qui donne ce volume, par bissection, avec NOTRE propre boucle.
		const float32 vAttendu = masse / 1000.f; // la vraie eau, pas la mutee
		float32 cEq = 0.f;
		{
			float32 lo = -rayon, hi = rayon;
			for (uint32 i = 0; i < 80u; ++i) {
				const float32 mid = 0.5f * (lo + hi);
				if (NkSphereVolumeBelow(rayon, mid) < vAttendu)
					lo = mid;
				else
					hi = mid;
			}
			cEq = 0.5f * (lo + hi);
		}
		const float32 yEq = -cEq; // sur eau calme (ys = 0), le centre est a -cEq
		const float32 aFlot = NkSphereWaterlineRadius(rayon, cEq);
		const float32 raideur = 1000.f * NK_GRAVITE_NORMALE * 3.1415926535f * aFlot * aFlot;
		const float32 omega0 = NkSqrt(raideur / masse);
		const float32 zeta = 0.25f; // choisi, et donc NOMME : il fixe l'amortissement
		const float32 lambda = 2.f * zeta * NkSqrt(raideur * masse);
		std::fprintf(stderr,
					 "     r = %.3f m, rho_corps = %.0f, m = %.4f kg, V_total = %.6f m3\n"
					 "     ATTENDU AVANT MESURE : V_immerge = m/rho = %.6f m3 -> cote c = %.6f m\n"
					 "       -> centre d'equilibre y = %.6f m ; rayon de flottaison a = %.4f m\n"
					 "       -> raideur k = rho g pi a^2 = %.1f N/m ; omega0 = %.4f rad/s "
					 "(periode propre %.4f s)\n"
					 "       -> zeta = %.3f -> lambda = 2 zeta sqrt(k m) = %.2f N.s/m\n",
					 (double)rayon, (double)densiteCorps, (double)masse, (double)vTotal,
					 (double)vAttendu, (double)cEq, (double)yEq, (double)aFlot, (double)raideur,
					 (double)omega0, (double)(6.2831853f / omega0), (double)zeta, (double)lambda);

		NkBuoyancyParams bp;
		bp.waterDensity = densiteEau;
		bp.linearDamping = lambda;

		const NkWaterParams calme = HouleCalme();
		const float32 dt = 1.f / 480.f;
		const uint32 pas = (uint32)(20.f / dt);

		// ── (f1) UN CORPS MOINS DENSE REMONTE ET SE STABILISE ────────────────
		{
			Flotteur f;
			f.rayon = rayon;
			f.masse = masse;
			f.position = {0.f, 2.f, 0.f}; // lache 2 m au-dessus : il doit TOMBER puis remonter
			float32 t = 0.f;
			float32 plusBas = 1e9f;
			for (uint32 i = 0; i < pas; ++i) {
				PasFlotteur(f, bp, calme, nullptr, t, dt);
				t += dt;
				if (f.position.y < plusBas)
					plusBas = f.position.y;
			}
			const float32 ecart = NkFabs(f.position.y - yEq);
			std::fprintf(stderr,
						 "     lache a y = +2,000 m ; apres %u pas (%.1f s) : y = %.6f m "
						 "(attendu %.6f), ecart %.3e m ; point le plus bas %.4f m\n",
						 pas, (double)(dt * (float32)pas), (double)f.position.y, (double)yEq,
						 (double)ecart, (double)plusBas);
			ECHECK(ecart < 1e-3f,
				   "(f1) un corps moins dense REMONTE et se stabilise a l'enfoncement d'Archimede");
			ECHECK(plusBas < yEq,
				   "(f1b) il a bien PLONGE sous son equilibre avant d'y revenir (ce n'est pas un "
				   "corps pose a la main sur sa reponse)");
		}

		// ── (f1n) LE NEGATIF : UN CORPS PLUS DENSE COULE ─────────────────────
		{
			const float32 masseLourde = 2000.f * vTotal;
			float32 bidon = 0.f;
			const bool aUnEquilibre = NkBuoyancyEquilibriumSphere(rayon, masseLourde, 1000.f, bidon);
			ECHECK(!aUnEquilibre,
				   "(f1n-a) plus dense que l'eau : la recherche d'equilibre REFUSE au lieu d'en "
				   "inventer un");

			Flotteur f;
			f.rayon = rayon;
			f.masse = masseLourde;
			f.position = {0.f, 0.f, 0.f};
			float32 t = 0.f;
			float32 yAvant = 0.f;
			bool monotone = true;
			for (uint32 i = 0; i < pas; ++i) {
				PasFlotteur(f, bp, calme, nullptr, t, dt);
				t += dt;
				if (i > 240u && f.position.y > yAvant)
					monotone = false;
				yAvant = f.position.y;
			}
			std::fprintf(stderr,
						 "     corps a 2000 kg/m3 lache a y = 0 : apres %.1f s, y = %.3f m, "
						 "vitesse %.3f m/s, descente monotone = %s\n",
						 (double)(dt * (float32)pas), (double)f.position.y, (double)f.vitesse.y,
						 monotone ? "oui" : "NON");
			ECHECK(f.position.y < -10.f && f.vitesse.y < 0.f && monotone,
				   "(f1n-b) un corps PLUS dense coule, ne se stabilise pas, et descend encore");
		}

		// ── (f2) IL SUIT LA VAGUE ────────────────────────────────────────────
		// L'ATTENDU EST UNE BORNE, ECRITE AVANT, ET DERIVEE. Autour de son
		// equilibre, le flotteur est un oscillateur masse-ressort-amortisseur
		// EXCITE PAR SA BASE (la surface). Sa reponse en deplacement vaut
		//        Y / Ys = 1 / (1 - r^2 + 2 i zeta r),   r = omega / omega0
		// donc l'ECART au mouvement de la surface vaut
		//        |Y - Ys| / |Ys| = r sqrt(r^2 + 4 zeta^2) / sqrt((1-r^2)^2 + 4 zeta^2 r^2)
		// Tout y sort des parametres ci-dessus ; rien n'est recopie.
		{
			const float32 amplitude = 0.4f, longueur = 40.f;
			const NkWaterParams houle = HouleSinus(amplitude, longueur);
			const float32 celerite = NkPhaseSpeed(longueur, 0.f, NK_GRAVITE_NORMALE);
			const float32 omega = 6.2831853f * celerite / longueur;
			const float32 periode = 6.2831853f / omega;
			const float32 rr = omega / omega0;
			const float32 num = rr * NkSqrt(rr * rr + 4.f * zeta * zeta);
			const float32 den =
				NkSqrt((1.f - rr * rr) * (1.f - rr * rr) + 4.f * zeta * zeta * rr * rr);
			const float32 borneLineaire = amplitude * num / den;
			// La marge : le modele lineaire suppose une raideur CONSTANTE, or le
			// volume de calotte est cubique en enfoncement. Le facteur est DIT, pas
			// ajuste apres coup.
			const float32 borne = 1.5f * borneLineaire;
			std::fprintf(stderr,
						 "     houle : A = %.3f m, L = %.1f m -> c = %.4f m/s, omega = %.4f rad/s, "
						 "periode %.4f s\n"
						 "     ATTENDU AVANT MESURE : r = omega/omega0 = %.4f ; ecart <= A r "
						 "sqrt(r^2+4z^2)/sqrt((1-r^2)^2+4z^2r^2) = %.5f m ; borne retenue "
						 "(x1,5 pour la non-linearite) = %.5f m\n",
						 (double)amplitude, (double)longueur, (double)celerite, (double)omega,
						 (double)periode, (double)rr, (double)borneLineaire, (double)borne);

			Flotteur f;
			f.rayon = rayon;
			f.masse = masse;
			f.position = {0.f, yEq, 0.f};
			float32 t = 0.f;
			const uint32 nPas = (uint32)(8.f * periode / dt);
			const uint32 depuis = (uint32)(6.f * periode / dt); // on juge les 2 dernieres periodes
			float32 pire = 0.f;
			float32 amplitudeCorps = 0.f, yMin = 1e9f, yMax = -1e9f;
			// Les deux courbes pour l'image : la surface et le corps.
			const uint32 nCourbe = 900u;
			float32 *courbes = (float32 *)std::malloc(sizeof(float32) * 2u * nCourbe);
			const uint32 divisor = nPas / nCourbe + 1u;
			uint32 ecrit = 0u;
			for (uint32 i = 0; i < nPas; ++i) {
				PasFlotteur(f, bp, houle, nullptr, t, dt);
				t += dt;
				const float32 ys = NkWaterHeight(houle, f.position.x, f.position.z, t);
				if (courbes != nullptr && (i % divisor) == 0u && ecrit < nCourbe) {
					courbes[ecrit] = ys;
					courbes[nCourbe + ecrit] = f.position.y;
					++ecrit;
				}
				if (i >= depuis) {
					const float32 e = NkFabs(f.position.y - (ys + yEq));
					if (e > pire)
						pire = e;
					if (f.position.y < yMin)
						yMin = f.position.y;
					if (f.position.y > yMax)
						yMax = f.position.y;
				}
			}
			amplitudeCorps = 0.5f * (yMax - yMin);
			std::fprintf(stderr,
						 "     MESURE sur les 2 dernieres periodes : ecart max au (surface + "
						 "equilibre) = %.5f m ; demi-amplitude du corps %.4f m (la houle : %.4f m)\n",
						 (double)pire, (double)amplitudeCorps, (double)amplitude);
			ECHECK(pire <= borne, "(f2) le corps SUIT la vague, sous la borne ecrite AVANT");
			ECHECK(amplitudeCorps > 0.5f * amplitude,
				   "(f2b) et il la suit VRAIMENT : son amplitude depasse la moitie de celle de la "
				   "houle (un corps immobile passerait (f2) si la borne etait large)");
			if (courbes != nullptr) {
				ImageCourbes("eau_07_flotteur_suit_la_houle.bmp", courbes, 2u, ecrit,
							 -amplitude * 1.6f, amplitude * 1.6f,
							 "petrole = surface, orange = centre du flotteur");
				std::free(courbes);
			}

			// ── (f2n) LE NEGATIF : SUR EAU CALME, IL N'Y A RIEN A SUIVRE ─────
			// Deux enonces, et ils n'ont pas le meme statut : la condition d'essai
			// est BIT-EXACTE, la reponse du corps ne peut pas l'etre (la force nette
			// a l'equilibre n'est pas exactement zero en flottant). On le dit au
			// lieu de faire semblant.
			Flotteur g;
			g.rayon = rayon;
			g.masse = masse;
			g.position = {0.f, yEq, 0.f};
			float32 tg = 0.f;
			bool surfaceBitExacte = true;
			float32 excursion = 0.f;
			for (uint32 i = 0; i < nPas; ++i) {
				PasFlotteur(g, bp, calme, nullptr, tg, dt);
				tg += dt;
				const float32 ys = NkWaterHeight(calme, g.position.x, g.position.z, tg);
				if (std::memcmp(&ys, &kZeroF, sizeof(float32)) != 0)
					surfaceBitExacte = false;
				const float32 e = NkFabs(g.position.y - yEq);
				if (e > excursion)
					excursion = e;
			}
			std::fprintf(stderr,
						 "     CONTRE-EPREUVE eau calme : surface a 0 AU BIT sur %u pas = %s ; "
						 "excursion du corps %.3e m\n",
						 nPas, surfaceBitExacte ? "oui" : "NON", (double)excursion);
			ECHECK(surfaceBitExacte,
				   "(f2n-a) eau calme : la surface que le corps suivrait vaut ZERO AU BIT a chaque "
				   "pas");
			ECHECK(excursion < 1e-4f,
				   "(f2n-b) eau calme : le corps reste a une hauteur constante (a 1e-4 m ; PAS au "
				   "bit, et c'est dit : la force nette a l'equilibre n'est pas exactement nulle en "
				   "flottant)");
		}
	}

	// =====================================================================
	// MOITIE B — LE CORPS AGIT SUR L'EAU
	// =====================================================================
	void TemoinPerturbation() {
		std::fprintf(stderr, "-- (p) MOITIE B : le corps agit sur l'eau --\n");

		const float32 rayon = 0.5f;
		const float32 vTotal = 4.f / 3.f * 3.1415926535f * rayon * rayon * rayon;
		const float32 masse = 400.f * vTotal;
		const float32 vDeplace = masse / 1000.f; // Archimede : le volume qu'il deplace
		float32 cEq = 0.f;
		NkBuoyancyEquilibriumSphere(rayon, masse, 1000.f, cEq);
		const float32 aFlot = NkSphereWaterlineRadius(rayon, cEq);
		// L'ETENDUE du creux. C'est un REGLAGE, pas une loi : le creux reel autour
		// d'une carene depend du nombre de Froude et de la forme. 3 x le rayon de
		// flottaison est un choix, et le voici nomme.
		const float32 etalement = 3.f;
		const float32 R = etalement * aFlot;
		// L'amplitude au centre, DERIVEE : a = -3 V / (pi R^2). Ecrite ici parce
		// que c'est l'ORDRE DE GRANDEUR attendu, pas le critere fin -- le critere
		// fin, c'est le volume (p1c), qui ne depend pas de la forme de la cloche.
		const float32 aCentre = -3.f * vDeplace / (3.1415926535f * R * R);
		std::fprintf(stderr,
					 "     le corps deplace V = m/rho = %.6f m3 ; rayon de flottaison a = %.4f m ; "
					 "etalement x%.1f -> R = %.4f m\n"
					 "     ATTENDU AVANT MESURE : creux au centre a = -3V/(pi R^2) = %.6f m "
					 "(NEGATIF, ordre 1e-2 m)\n",
					 (double)vDeplace, (double)aFlot, (double)etalement, (double)R, (double)aCentre);

		const NkWaterParams calme = HouleCalme();

		// ── (p1) UN CORPS POSE CREUSE LA SURFACE AUTOUR DE LUI ───────────────
		{
			NkWaterDisturbance champ;
			champ.Reserve(256u);
			if (gMutation != 1) // MUTATION 1 : on ne pose PAS la carene
				champ.SetHull(1u, NkVec2f{0.f, 0.f}, R, vDeplace);

			const float32 sans = NkWaterHeight(calme, 1.f, 0.f, 0.f);
			const float32 avec = NkWaterHeight(calme, 1.f, 0.f, 0.f, -1.f, &champ);
			std::fprintf(stderr,
						 "     hauteur a 1,0 m du corps : SANS corps %.9f m, AVEC corps %.6f m, "
						 "difference %.6f m\n",
						 (double)sans, (double)avec, (double)(avec - sans));
			ECHECK(avec < sans, "(p1a) le corps POSE CREUSE la surface a 1 m : la hauteur BAISSE");
			ECHECK(NkFabs(avec) > 1e-3f && NkFabs(avec) < 1e-1f,
				   "(p1b) et le creux est de l'ordre de grandeur du volume deplace (entre 1 mm et "
				   "10 cm)");

			// (p1c) LE CRITERE QUI NE DEPEND PAS DE LA FORME DE LA CLOCHE :
			// l'integrale du creux sur le plan vaut le volume DEPLACE, au signe
			// pres. C'est Archimede, pas une relecture du noyau.
			{
				const float32 demi = 2.f * R;
				const uint32 N = 900u;
				const float32 h = 2.f * demi / (float32)N;
				float64 integrale = 0.0;
				for (uint32 j = 0; j < N; ++j) {
					const float32 z = -demi + ((float32)j + 0.5f) * h;
					for (uint32 i = 0; i < N; ++i) {
						const float32 x = -demi + ((float32)i + 0.5f) * h;
						integrale += (float64)champ.Height(x, z, 0.f) * (float64)h * (float64)h;
					}
				}
				const float64 attendu = gMutation == 1 ? 0.0 : -(float64)vDeplace;
				const float64 err = attendu != 0.0 ? NkFabs(integrale - attendu) / NkFabs(attendu)
												   : NkFabs(integrale);
				std::fprintf(stderr,
							 "     quadrature %ux%u sur [-2R, 2R] : integrale du creux = %.8f m3, "
							 "attendu -V = %.8f m3, ecart relatif %.3e\n",
							 N, N, integrale, attendu, err);
				ECHECK(gMutation == 1 ? (integrale == 0.0) : (err < 2e-3),
					   "(p1c) le creux CONTIENT EXACTEMENT le volume deplace (Archimede, pas la "
					   "forme de la cloche)");
			}

			ImageDessus("eau_01_calme_sans_corps.bmp", calme, nullptr, 0.f, 8.f, 0.05f);
			ImageDessus("eau_02_calme_corps_pose.bmp", calme, &champ, 0.f, 8.f, 0.05f);
		}

		// ── (p1n) LE NEGATIF CAPITAL : SANS CORPS, AU BIT ────────────────────
		{
			const NkWaterParams houle = HouleSinus(0.6f, 18.f);
			NkWaterDisturbance vide;
			vide.Reserve(16u);
			NkWaterDisturbance loin;
			loin.Reserve(16u);
			loin.SetHull(1u, NkVec2f{1000.f, 1000.f}, R, vDeplace); // hors du support compact
			uint32 diffVide = 0u, diffLoin = 0u, points = 0u;
			for (uint32 k = 0; k < 7u; ++k) {
				const float32 t = 0.37f * (float32)k;
				for (uint32 j = 0; j < 61u; ++j) {
					const float32 z = -15.f + 0.5f * (float32)j;
					for (uint32 i = 0; i < 61u; ++i) {
						const float32 x = -15.f + 0.5f * (float32)i;
						const NkWaterPoint a = NkWaterEval(houle, x, z, t);
						const NkWaterPoint b = NkWaterEval(houle, x, z, t, -1.f, &vide);
						const NkWaterPoint c = NkWaterEval(houle, x, z, t, -1.f, &loin);
						diffVide += BitsDiff(a, b);
						diffLoin += BitsDiff(a, c);
						++points;
					}
				}
			}
			std::fprintf(stderr,
						 "     %u points x 17 champs = %u comparaisons de bits\n"
						 "       perturbation VIDE            : %u champs differents\n"
						 "       source a 1000 m (hors support): %u champs differents\n",
						 points, points * 17u, diffVide, diffLoin);
			ECHECK(diffVide == 0u,
				   "(p1n-a) NEGATIF CAPITAL : sans aucun corps, la surface vaut la valeur "
				   "ANALYTIQUE AU BIT");
			ECHECK(diffLoin == 0u,
				   "(p1n-b) et une source HORS de son support compact ne fuit pas d'un seul ULP");
			ECHECK(vide.Count() == 0u && vide.Dropped() == 0u && vide.ShedTotal() == 0u,
				   "(p1n-c) le ZERO des compteurs de l'etat : une perturbation neuve est vide et "
				   "le dit");
		}

		// ── (p2) UN CORPS QUI BOUGE LAISSE UNE TRACE DERRIERE LUI ────────────
		// L'ATTENDU, ECRIT AVANT ET DERIVE DE LA GEOMETRIE : a D = 2,0 m du corps,
		// on est HORS du support de sa carene (R = %.3f m < 2,0 m), donc l'AMONT
		// vaut EXACTEMENT 0 -- il n'y a rien devant. L'AVAL, lui, porte les rides
		// lachees, donc il est STRICTEMENT NEGATIF.
		{
			NkWaterDisturbance champ;
			champ.Reserve(512u);
			NkWaterWake sillage;
			NkWaterWakeParams wp;
			wp.gain = 1.f;
			wp.damping = (gMutation == 2) ? 0.f : 0.8f; // MUTATION 2 : plus d'amortissement
			wp.spread = 0.6f;
			wp.stepDistance = 0.5f;

			const float32 vitesse = 3.f, dt = 1.f / 120.f;
			float32 x = -10.f, t = 0.f;
			sillage.Reset(NkVec2f{x, 0.f});
			uint32 laches = 0u;
			while (x < 6.f) {
				x += vitesse * dt;
				t += dt;
				laches += sillage.Update(champ, 1u, NkVec2f{x, 0.f}, R, vDeplace, t, wp);
				champ.Prune(t);
			}
			const float32 amont = champ.Height(x + 2.f, 0.f, t);
			const float32 aval = champ.Height(x - 2.f, 0.f, t);
			std::fprintf(stderr,
						 "     corps a %.1f m/s de x=-10 a x=%.2f (t = %.3f s) : %u rides lachees, "
						 "%u encore vivantes, %u eteintes, %u perdues\n"
						 "     AMONT (x+2) = %.9f m   |   AVAL (x-2) = %.9f m\n",
						 (double)vitesse, (double)x, (double)t, laches, champ.Count(),
						 champ.PrunedTotal(), champ.Dropped(), (double)amont, (double)aval);
			ECHECK(amont == 0.f,
				   "(p2a) DEVANT le corps, a 2 m (hors du support de sa carene), la surface est "
				   "intacte A ZERO EXACT");
			ECHECK(aval < 0.f, "(p2b) DERRIERE lui, a la MEME distance, l'eau est CREUSEE : la trace");
			ECHECK(laches > 0u && champ.Count() > 0u,
				   "(p2c) des rides ont vraiment ete lachees, et il en reste");

			ImageDessus("eau_03_calme_sillage.bmp", calme, &champ, t, 24.f, 0.02f);
			const NkWaterParams houle = HouleSinus(0.25f, 12.f);
			ImageDessus("eau_04_houle_seule.bmp", houle, nullptr, t, 24.f, 0.30f);
			ImageDessus("eau_05_houle_et_sillage.bmp", houle, &champ, t, 24.f, 0.30f);
		}

		// ── (p2n) LE NEGATIF : CORPS IMMOBILE -> AMONT ET AVAL EGAUX AU BIT ──
		// On echantillonne a +/-1,0 m, DANS le support de la carene : a +/-2 m les
		// deux vaudraient zero et l'egalite serait vide de sens.
		{
			NkWaterDisturbance champ;
			champ.Reserve(512u);
			NkWaterWake sillage;
			NkWaterWakeParams wp;
			const float32 dt = 1.f / 120.f;
			float32 t = 0.f;
			sillage.Reset(NkVec2f{0.f, 0.f});
			for (uint32 i = 0; i < 600u; ++i) {
				t += dt;
				sillage.Update(champ, 1u, NkVec2f{0.f, 0.f}, R, vDeplace, t, wp);
			}
			const float32 amont = champ.Height(1.f, 0.f, t);
			const float32 aval = champ.Height(-1.f, 0.f, t);
			const bool egauxAuBit = std::memcmp(&amont, &aval, sizeof(float32)) == 0;
			std::fprintf(stderr,
						 "     CONTRE-EPREUVE corps IMMOBILE, 600 pas : %u rides lachees ; "
						 "amont(+1) = %.9f, aval(-1) = %.9f, egaux au bit = %s\n",
						 champ.ShedTotal(), (double)amont, (double)aval, egauxAuBit ? "oui" : "NON");
			ECHECK(champ.ShedTotal() == 0u,
				   "(p2n-a) un corps qui ne bouge pas ne lache AUCUNE ride");
			ECHECK(egauxAuBit && amont != 0.f,
				   "(p2n-b) amont et aval sont EGAUX AU BIT (et non nuls : on mesure bien quelque "
				   "chose)");
		}

		// ── (p3) LA PERTURBATION S'AMORTIT ───────────────────────────────────
		// LA LOI, ECRITE AVANT : sans etalement, l'amplitude decroit en
		//        h(age) / h(0) = exp(-damping * age)
		{
			const float32 tau = (gMutation == 2) ? 0.f : 0.5f;
			NkWaterDisturbance champ;
			champ.Reserve(8u);
			champ.Shed(NkVec2f{0.f, 0.f}, R, vDeplace, 0.f, tau, 0.f); // spread = 0 : loi PURE
			const float32 h0 = champ.Height(0.f, 0.f, 0.f);
			bool loiTenue = true;
			std::fprintf(stderr, "     ride lachee a t=0, amortissement %.3f /s, sans etalement :\n",
						 (double)tau);
			for (uint32 k = 1; k <= 4u; ++k) {
				const float32 age = (float32)k;
				const float32 h = champ.Height(0.f, 0.f, age);
				const float32 attendu = NkExp(-tau * age);
				const float32 mesure = h / h0;
				const float32 err = NkFabs(mesure - attendu);
				if (err > 1e-5f)
					loiTenue = false;
				std::fprintf(stderr, "       age %.0f s : rapport mesure %.6f, attendu %.6f, ecart %.2e\n",
							 (double)age, (double)mesure, (double)attendu, (double)err);
			}
			if (gMutation == 2) {
				ECHECK(!loiTenue || NkFabs(champ.Height(0.f, 0.f, 4.f) / h0 - 1.f) < 1e-7f,
					   "(p3) MUTATION 2 : amortissement NUL -> l'amplitude ne decroit PAS (le "
					   "critere de (p3) ne mesurerait plus rien)");
			} else {
				ECHECK(loiTenue, "(p3) la perturbation S'AMORTIT selon exp(-damping * age), la loi "
								 "ecrite AVANT");
			}

			// (p3n) LE NEGATIF : amortissement nul -> rapport 1,0 AU BIT.
			NkWaterDisturbance eternel;
			eternel.Reserve(8u);
			eternel.Shed(NkVec2f{0.f, 0.f}, R, vDeplace, 0.f, 0.f, 0.f);
			const float32 e0 = eternel.Height(0.f, 0.f, 0.f);
			const float32 e9 = eternel.Height(0.f, 0.f, 9999.f);
			const bool identique = std::memcmp(&e0, &e9, sizeof(float32)) == 0;
			std::fprintf(stderr,
						 "     CONTRE-EPREUVE amortissement NUL : h(0) = %.9f, h(9999 s) = %.9f, "
						 "identiques au bit = %s ; pruned = %u\n",
						 (double)e0, (double)e9, identique ? "oui" : "NON", eternel.PrunedTotal());
			eternel.Prune(9999.f);
			ECHECK(identique && e0 != 0.f,
				   "(p3n-a) amortissement NUL : l'amplitude ne bouge PAS d'un bit en 9999 s (sans "
				   "quoi (p3) ne mesurerait rien)");
			ECHECK(eternel.Count() == 1u,
				   "(p3n-b) et `Prune` ne la retire PAS : une ride eternelle reste, et ca se voit "
				   "dans le compteur au lieu d'etre corrige en douce");
		}

		// ── (p3b) L'ETALEMENT DILUE, IL NE CREE PAS ──────────────────────────
		// A amortissement nul, une ride qui s'etale garde son VOLUME : c'est la
		// consequence de a = -3V/(pi R(age)^2), et ca se verifie par quadrature.
		{
			NkWaterDisturbance champ;
			champ.Reserve(8u);
			champ.Shed(NkVec2f{0.f, 0.f}, 1.f, vDeplace, 0.f, 0.f, 0.5f);
			bool conserve = true;
			for (uint32 k = 0; k <= 4u; k += 2u) {
				const float32 age = (float32)k;
				const float32 Rage = 1.f + 0.5f * age;
				const float32 demi = 1.3f * Rage;
				const uint32 N = 700u;
				const float32 h = 2.f * demi / (float32)N;
				float64 integrale = 0.0;
				for (uint32 j = 0; j < N; ++j) {
					const float32 z = -demi + ((float32)j + 0.5f) * h;
					for (uint32 i = 0; i < N; ++i) {
						const float32 x = -demi + ((float32)i + 0.5f) * h;
						integrale += (float64)champ.Height(x, z, age) * (float64)h * (float64)h;
					}
				}
				const float64 err = NkFabs(integrale + (float64)vDeplace) / (float64)vDeplace;
				if (err > 3e-3)
					conserve = false;
				std::fprintf(stderr,
							 "       age %.0f s, R = %.2f m : integrale %.8f m3 (attendu %.8f), "
							 "ecart %.2e\n",
							 (double)age, (double)Rage, integrale, -(double)vDeplace, err);
			}
			ECHECK(conserve,
				   "(p3b) une ride qui s'etale DILUE son creux sans en creer : son volume reste -V");
		}

		// ── (p4) LA FILE DIT CE QU'ELLE PERD ─────────────────────────────────
		{
			NkWaterDisturbance champ;
			champ.Reserve(8u);
			uint32 acceptees = 0u;
			for (uint32 i = 0; i < 20u; ++i)
				if (champ.Shed(NkVec2f{(float32)i, 0.f}, 1.f, 0.1f, 0.f, 1.f, 0.f))
					++acceptees;
			std::fprintf(stderr,
						 "     20 rides proposees sur une file de 8 : %u acceptees, %u perdues, "
						 "%u vivantes\n",
						 acceptees, champ.Dropped(), champ.Count());
			ECHECK(acceptees == 8u && champ.Dropped() == 12u && champ.Count() == 8u,
				   "(p4a) la file est BORNEE et DIT ce qu'elle perd (8 acceptees, 12 refusees)");
			const uint32 retirees = champ.Prune(20.f);
			ECHECK(retirees == 8u && champ.Count() == 0u,
				   "(p4b) `Prune` retire les rides eteintes : la mer ne se remplit pas de rides "
				   "eternelles");
			ECHECK(!champ.SetHull(0u, NkVec2f{0.f, 0.f}, 1.f, 1.f),
				   "(p4c) une carene sans proprietaire est REFUSEE (sinon toutes fusionneraient en "
				   "silence)");
		}
	}

	// Le fichier d'accompagnement des images : ce que chacune prouve, ET ce
	// qu'elle ne prouve pas. Une image sans cette phrase-la se fait croire.
	void EcrireLisezMoi() {
		if (gImageDir == nullptr)
			return;
		char chemin[512];
		std::snprintf(chemin, sizeof(chemin), "%s/LISEZMOI.md", gImageDir);
		std::FILE *f = std::fopen(chemin, "wb");
		if (f == nullptr)
			return;
		std::fprintf(
			f,
			"# Les corps et l'eau s'influencent — les images (2026-09-14)\n\n"
			"AUTEUR : TEUGUIA TADJUIDJE Rodolf Sederis — Rihen\n\n"
			"Rendus **CPU**, sans fenetre ni device, produits par\n"
			"`NKPhysics_Tests` (`test_eau_couplage.cpp`). Ce ne sont PAS des captures du\n"
			"moteur : elles montrent la SURFACE telle que `NkWaterEval` la rend, donc\n"
			"exactement ce que le producteur de maillage recevra. Elles ne prouvent rien\n"
			"sur le rendu GPU, l'eclairage, ni les nuanceurs.\n\n"
			"Palette : **petrole #0A555F = creux**, blanc = repos, **orange #F79A28 =\n"
			"bosse**. L'echelle est ecrite dans le journal du banc, et les images d'une\n"
			"meme serie la PARTAGENT — sans quoi « avant » et « apres » se ressemblent au\n"
			"lieu de se comparer.\n\n"
			"| image | ce qu'elle prouve | ce qu'elle ne prouve pas |\n"
			"|---|---|---|\n"
			"| `eau_01_calme_sans_corps.bmp` | la mer calme est **uniformement au repos** "
			"— c'est le ZERO de l'instrument, a regarder AVANT les autres | rien d'autre : "
			"une image plate ne dit pas que le code marche |\n"
			"| `eau_02_calme_corps_pose.bmp` | le corps **pose** creuse un bassin autour de "
			"lui (tache petrole) : moitie B, cas statique | que le creux ait la bonne forme "
			"physique — le temoin (p1c) verifie son VOLUME, pas sa silhouette |\n"
			"| `eau_03_calme_sillage.bmp` | le corps **en mouvement** laisse une trace "
			"DERRIERE lui et rien devant : moitie B, cas dynamique | que ce soit un sillage "
			"de Kelvin — les rides sont ISOTROPES, le cone a 19deg28' n'est pas modelise |\n"
			"| `eau_04_houle_seule.bmp` | la houle de Gerstner **seule**, inchangee | — |\n"
			"| `eau_05_houle_et_sillage.bmp` | la trace **s'ajoute** a la houle sans la "
			"remplacer : compare-la a la 04, meme echelle, meme instant | que la trace "
			"interagisse avec la houle (elle ne le fait pas : c'est une somme) |\n"
			"| `eau_07_flotteur_suit_la_houle.bmp` | moitie A : **petrole = la surface**, "
			"**orange = le centre du flotteur**. Les deux courbes montent et descendent "
			"ensemble | que le retard soit exact — le temoin (f2) le borne, l'image le "
			"montre |\n\n"
			"⚠️ **Ce qui manque encore, et qui est nomme** : aucune de ces images ne sort "
			"du chemin GPU. Le branchement au producteur de maillage (`NkWaterMeshBuilder`)\n"
			"et a la sonde ocean de `Demo3D` est l'etape suivante ; tant qu'elle n'est pas\n"
			"faite, l'eau que Rodolf voit a l'ecran ne porte pas encore ces creux.\n");
		std::fclose(f);
		std::fprintf(stderr, "     LISEZMOI.md ecrit dans %s\n", gImageDir);
	}

} // namespace

int RunCouplageTests(int &pass, int &fail) {
	gPass = &pass;
	gFail = &fail;
	const char *mut = std::getenv("NK_EAU_MUT");
	gMutation = mut != nullptr ? std::atoi(mut) : 0;
	gImageDir = std::getenv("NK_EAU_IMAGES");
	const int avant = fail;
	std::fprintf(stderr, "=== LES CORPS ET L'EAU S'INFLUENCENT : les deux moities du couplage ===\n");
	if (gMutation != 0)
		std::fprintf(stderr,
					 "*** MUTATION %d ACTIVE : 1 = carene non posee, 2 = amortissement nul, "
					 "3 = poussee nulle. On ATTEND des rouges, et on regarde LESQUELS. ***\n",
					 gMutation);
	if (gImageDir != nullptr)
		std::fprintf(stderr, "*** images demandees dans : %s ***\n", gImageDir);
	TemoinInstrument();
	TemoinFlottabilite();
	TemoinPerturbation();
	EcrireLisezMoi();
	std::fprintf(stderr, "=== couplage : %d rouge(s) sur ce bloc ===\n", fail - avant);
	return fail;
}
