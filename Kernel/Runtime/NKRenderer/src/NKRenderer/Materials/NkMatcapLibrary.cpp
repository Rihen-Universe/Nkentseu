// =============================================================================
// NkMatcapLibrary.cpp — generation procedurale des 30 matcaps
//
// Chaque pixel d'une boule matcap EST une normale en espace vue :
//     d  = (2u-1, 1-2v)              position dans le disque unite
//     nz = sqrt(1 - |d|^2)           hemisphere tournee vers la camera
//     n  = (d.x, d.y, nz)
// On evalue ensuite un petit modele d'eclairage analytique par matcap. Rien
// n'est echantillonne au rendu : tout le cout est paye une fois, au demarrage.
//
// POURQUOI PROCEDURAL PLUTOT QUE DES .EXR IMPORTES : les matcaps livrees avec
// Blender sont sous licence (GPL / CC), les redistribuer imposerait sa licence
// a ce depot. Les modeles ci-dessous reproduisent le ROLE de chaque matcap de
// Blender (une argile mate, un metal brosse, un rendu peau, un damier de
// controle des normales...) sans en copier les pixels.
//
// LIMITE HONNETE : une matcap peinte a la main par un artiste contient des
// details (reflets d'atelier, degrades non analytiques) qu'un modele ferme ne
// reproduit pas. Ces 30 matcaps sont bonnes pour lire une FORME — c'est leur
// travail en mode Solid — pas pour juger un materiau final. Le chargement de
// vraies matcaps depuis le disque reste possible : voir NkRender3D::SetMatcapTexture.
// =============================================================================
#include "NKRenderer/Materials/NkMatcapLibrary.h"

#include <math.h>

namespace nkentseu {
	namespace renderer {

		namespace {

			// ── Modele d'eclairage d'une boule ────────────────────────────────
			// kind selectionne les familles qui ne se decrivent pas par de simples
			// coefficients (damiers de controle, aplats toon, metal anisotrope).
			enum Kind : uint8 {
				K_STUDIO = 0,	 // diffus + speculaire + rim : le cas general
				K_HEMI,			 // degrade ciel/sol, tres mat (argiles)
				K_METAL,		 // horizon net + reflets durs
				K_TOON,			 // aplats quantifies
				K_CHECK_NORMAL,	 // damier sur la normale : controle du lissage
				K_CHECK_REFLECT, // damier sur la reflexion : controle des UV/reflets
				K_ANISO,		 // metal brosse (speculaire etire)
				K_SKIN			 // diffusion sous-surfacique approchee
			};

			struct Preset {
				const char *name;
				uint8 kind;
				float32 base[3];   // couleur diffuse
				float32 top[3];	   // teinte du haut (ciel)
				float32 bottom[3]; // teinte du bas (rebond du sol)
				float32 keyDir[3]; // direction de la lumiere principale
				float32 keyInt;	   // intensite diffuse
				float32 specInt;   // intensite speculaire
				float32 specPow;   // durete du speculaire
				float32 rimInt;	   // liseré de contour
				float32 rimPow;
				float32 rimCol[3];
				float32 ambient;  // plancher d'eclairement
				float32 bg;		  // fond de la tuile (hors du disque)
				float32 extra;	  // parametre libre : frequence damier, nb de paliers...
			};

			// 30 presets couvrant les familles de la bibliotheque de Blender.
			const Preset kPresets[NkMatcapLibrary::kCount] = {
				// ── Basiques (lecture de forme neutre) ────────────────────────
				{"basic_1", K_STUDIO, {0.78f, 0.78f, 0.80f}, {0.95f, 0.96f, 1.00f}, {0.28f, 0.27f, 0.26f},
				 {0.32f, 0.53f, 0.78f}, 0.62f, 0.35f, 28.f, 0.22f, 3.f, {0.85f, 0.90f, 1.00f}, 0.20f, 0.045f, 0.f},
				{"basic_2", K_STUDIO, {0.62f, 0.64f, 0.68f}, {0.88f, 0.92f, 1.00f}, {0.20f, 0.20f, 0.22f},
				 {-0.42f, 0.46f, 0.78f}, 0.66f, 0.28f, 18.f, 0.30f, 2.4f, {0.90f, 0.94f, 1.00f}, 0.17f, 0.045f, 0.f},
				{"basic_dark", K_STUDIO, {0.34f, 0.35f, 0.38f}, {0.60f, 0.63f, 0.72f}, {0.08f, 0.08f, 0.10f},
				 {0.28f, 0.50f, 0.82f}, 0.58f, 0.42f, 40.f, 0.34f, 3.4f, {0.70f, 0.78f, 1.00f}, 0.09f, 0.030f, 0.f},
				{"basic_side", K_STUDIO, {0.72f, 0.72f, 0.74f}, {0.90f, 0.92f, 0.98f}, {0.22f, 0.22f, 0.24f},
				 {0.86f, 0.18f, 0.48f}, 0.72f, 0.30f, 24.f, 0.18f, 3.f, {1.00f, 0.98f, 0.92f}, 0.14f, 0.045f, 0.f},

				// ── Ceramiques ────────────────────────────────────────────────
				{"ceramic_dark", K_STUDIO, {0.22f, 0.24f, 0.30f}, {0.52f, 0.58f, 0.72f}, {0.05f, 0.05f, 0.07f},
				 {0.30f, 0.55f, 0.78f}, 0.50f, 0.85f, 90.f, 0.30f, 4.f, {0.60f, 0.72f, 1.00f}, 0.07f, 0.028f, 0.f},
				{"ceramic_lightbulb", K_STUDIO, {0.92f, 0.90f, 0.84f}, {1.00f, 0.98f, 0.92f}, {0.42f, 0.40f, 0.38f},
				 {0.20f, 0.60f, 0.77f}, 0.55f, 0.75f, 70.f, 0.35f, 2.6f, {1.00f, 0.96f, 0.86f}, 0.28f, 0.055f, 0.f},

				// ── Damiers de controle ───────────────────────────────────────
				{"check_normal_y", K_CHECK_NORMAL, {0.85f, 0.85f, 0.88f}, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f},
				 {0.f, 0.f, 1.f}, 0.f, 0.f, 0.f, 0.f, 0.f, {0.f, 0.f, 0.f}, 0.f, 0.040f, 8.f},
				{"check_rim_light", K_CHECK_NORMAL, {0.90f, 0.90f, 0.92f}, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f},
				 {0.f, 0.f, 1.f}, 0.f, 0.f, 0.f, 0.55f, 2.2f, {1.00f, 1.00f, 1.00f}, 0.f, 0.045f, 12.f},
				{"check_rim_dark", K_CHECK_NORMAL, {0.30f, 0.30f, 0.33f}, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f},
				 {0.f, 0.f, 1.f}, 0.f, 0.f, 0.f, 0.60f, 2.2f, {0.75f, 0.85f, 1.00f}, 0.f, 0.025f, 12.f},
				{"reflection_check_horizontal", K_CHECK_REFLECT, {0.80f, 0.80f, 0.82f}, {0.f, 0.f, 0.f},
				 {0.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, 0.f, 0.f, 0.f, 0.f, 0.f, {0.f, 0.f, 0.f}, 0.f, 0.040f, 1.f},
				{"reflection_check_vertical", K_CHECK_REFLECT, {0.80f, 0.80f, 0.82f}, {0.f, 0.f, 0.f},
				 {0.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, 0.f, 0.f, 0.f, 0.f, 0.f, {0.f, 0.f, 0.f}, 0.f, 0.040f, 2.f},

				// ── Argiles (mates, degrade ciel/sol) ─────────────────────────
				{"clay_brown", K_HEMI, {0.58f, 0.40f, 0.28f}, {0.86f, 0.70f, 0.55f}, {0.14f, 0.09f, 0.06f},
				 {0.30f, 0.60f, 0.74f}, 0.55f, 0.06f, 8.f, 0.14f, 3.f, {0.90f, 0.75f, 0.60f}, 0.16f, 0.040f, 0.f},
				{"clay_muddy", K_HEMI, {0.44f, 0.42f, 0.34f}, {0.68f, 0.66f, 0.58f}, {0.11f, 0.10f, 0.09f},
				 {-0.30f, 0.62f, 0.72f}, 0.52f, 0.05f, 8.f, 0.12f, 3.f, {0.72f, 0.70f, 0.62f}, 0.14f, 0.035f, 0.f},
				{"clay_studio", K_HEMI, {0.72f, 0.68f, 0.64f}, {0.98f, 0.96f, 0.94f}, {0.24f, 0.22f, 0.21f},
				 {0.26f, 0.66f, 0.70f}, 0.58f, 0.10f, 14.f, 0.18f, 2.8f, {1.00f, 0.98f, 0.95f}, 0.20f, 0.050f, 0.f},
				{"matte_gray", K_HEMI, {0.66f, 0.66f, 0.66f}, {0.92f, 0.93f, 0.96f}, {0.20f, 0.20f, 0.21f},
				 {0.30f, 0.58f, 0.76f}, 0.60f, 0.05f, 8.f, 0.12f, 3.f, {0.90f, 0.92f, 1.00f}, 0.18f, 0.045f, 0.f},
				{"red_wax", K_HEMI, {0.66f, 0.20f, 0.16f}, {0.92f, 0.46f, 0.36f}, {0.16f, 0.04f, 0.04f},
				 {0.28f, 0.58f, 0.76f}, 0.56f, 0.12f, 16.f, 0.20f, 2.6f, {1.00f, 0.55f, 0.42f}, 0.14f, 0.038f, 0.f},

				// ── Pierres et resines ────────────────────────────────────────
				{"jade", K_SKIN, {0.16f, 0.52f, 0.38f}, {0.42f, 0.90f, 0.70f}, {0.03f, 0.14f, 0.10f},
				 {0.34f, 0.56f, 0.76f}, 0.50f, 0.55f, 60.f, 0.40f, 2.2f, {0.55f, 1.00f, 0.80f}, 0.10f, 0.030f, 0.55f},
				{"resin", K_SKIN, {0.82f, 0.70f, 0.48f}, {1.00f, 0.94f, 0.78f}, {0.24f, 0.18f, 0.10f},
				 {0.24f, 0.58f, 0.78f}, 0.48f, 0.65f, 80.f, 0.42f, 2.0f, {1.00f, 0.92f, 0.72f}, 0.16f, 0.045f, 0.60f},
				{"pearl", K_STUDIO, {0.88f, 0.86f, 0.90f}, {1.00f, 0.98f, 1.00f}, {0.34f, 0.32f, 0.38f},
				 {0.22f, 0.56f, 0.80f}, 0.48f, 0.70f, 46.f, 0.45f, 2.2f, {1.00f, 0.86f, 0.94f}, 0.26f, 0.055f, 0.f},
				{"skin", K_SKIN, {0.78f, 0.55f, 0.46f}, {1.00f, 0.80f, 0.70f}, {0.26f, 0.10f, 0.09f},
				 {0.28f, 0.56f, 0.78f}, 0.52f, 0.30f, 34.f, 0.34f, 2.4f, {1.00f, 0.62f, 0.52f}, 0.16f, 0.045f, 0.75f},
				{"glass_dark", K_METAL, {0.10f, 0.12f, 0.16f}, {0.40f, 0.48f, 0.62f}, {0.02f, 0.02f, 0.04f},
				 {0.30f, 0.52f, 0.80f}, 0.16f, 1.00f, 140.f, 0.62f, 2.6f, {0.70f, 0.84f, 1.00f}, 0.03f, 0.022f, 0.f},

				// ── Metaux ────────────────────────────────────────────────────
				{"metal_shiny", K_METAL, {0.72f, 0.74f, 0.78f}, {0.96f, 0.98f, 1.00f}, {0.10f, 0.11f, 0.14f},
				 {0.32f, 0.52f, 0.79f}, 0.30f, 1.00f, 110.f, 0.45f, 3.2f, {0.88f, 0.94f, 1.00f}, 0.06f, 0.030f, 0.f},
				{"metal_lead", K_METAL, {0.40f, 0.41f, 0.44f}, {0.66f, 0.68f, 0.74f}, {0.08f, 0.08f, 0.10f},
				 {0.30f, 0.54f, 0.78f}, 0.34f, 0.45f, 34.f, 0.30f, 3.f, {0.70f, 0.74f, 0.82f}, 0.07f, 0.028f, 0.f},
				{"metal_carpaint", K_METAL, {0.14f, 0.20f, 0.46f}, {0.45f, 0.60f, 1.00f}, {0.02f, 0.03f, 0.10f},
				 {0.30f, 0.54f, 0.78f}, 0.30f, 1.00f, 150.f, 0.55f, 2.6f, {0.80f, 0.90f, 1.00f}, 0.05f, 0.026f, 0.f},
				{"metal_anisotropic", K_ANISO, {0.62f, 0.63f, 0.66f}, {0.92f, 0.94f, 1.00f}, {0.10f, 0.10f, 0.12f},
				 {0.30f, 0.54f, 0.78f}, 0.30f, 0.90f, 60.f, 0.38f, 3.f, {0.86f, 0.90f, 1.00f}, 0.07f, 0.030f, 0.f},
				{"chrome", K_METAL, {0.86f, 0.88f, 0.92f}, {1.00f, 1.00f, 1.00f}, {0.06f, 0.07f, 0.10f},
				 {0.34f, 0.50f, 0.79f}, 0.22f, 1.00f, 180.f, 0.55f, 3.6f, {0.92f, 0.96f, 1.00f}, 0.05f, 0.030f, 0.f},
				{"gold", K_METAL, {0.90f, 0.68f, 0.24f}, {1.00f, 0.86f, 0.48f}, {0.16f, 0.10f, 0.02f},
				 {0.32f, 0.52f, 0.79f}, 0.34f, 0.95f, 90.f, 0.42f, 3.f, {1.00f, 0.88f, 0.50f}, 0.08f, 0.032f, 0.f},
				{"copper", K_METAL, {0.80f, 0.42f, 0.28f}, {1.00f, 0.66f, 0.48f}, {0.14f, 0.06f, 0.04f},
				 {0.32f, 0.52f, 0.79f}, 0.34f, 0.90f, 80.f, 0.42f, 3.f, {1.00f, 0.70f, 0.52f}, 0.08f, 0.032f, 0.f},

				// ── Stylise ───────────────────────────────────────────────────
				{"toon", K_TOON, {0.80f, 0.82f, 0.86f}, {1.00f, 1.00f, 1.00f}, {0.24f, 0.24f, 0.28f},
				 {0.30f, 0.55f, 0.78f}, 0.90f, 0.f, 0.f, 0.f, 0.f, {0.f, 0.f, 0.f}, 0.f, 0.045f, 4.f},
				{"blue_studio", K_STUDIO, {0.42f, 0.52f, 0.72f}, {0.72f, 0.84f, 1.00f}, {0.10f, 0.13f, 0.22f},
				 {0.30f, 0.55f, 0.78f}, 0.62f, 0.45f, 40.f, 0.30f, 2.8f, {0.80f, 0.90f, 1.00f}, 0.12f, 0.038f, 0.f},
			};

			inline float32 Clamp01(float32 x) {
				return x < 0.f ? 0.f : (x > 1.f ? 1.f : x);
			}

			inline float32 Dot3(const float32 *a, float32 x, float32 y, float32 z) {
				return a[0] * x + a[1] * y + a[2] * z;
			}

			// Evalue la couleur RGB d'un point de la boule pour le preset p.
			// (nx, ny, nz) est la normale en espace vue, deja normalisee.
			void Shade(const Preset &p, float32 nx, float32 ny, float32 nz, float32 *out) {
				// direction de la cle, normalisee une fois par pixel (cout negligeable
				// devant la lisibilite ; on ne genere l'atlas qu'au demarrage)
				const float32 kl = sqrtf(p.keyDir[0] * p.keyDir[0] + p.keyDir[1] * p.keyDir[1] +
										 p.keyDir[2] * p.keyDir[2]);
				const float32 inv = kl > 1e-6f ? 1.f / kl : 0.f;
				const float32 kx = p.keyDir[0] * inv, ky = p.keyDir[1] * inv, kz = p.keyDir[2] * inv;

				const float32 ndl = Clamp01(nx * kx + ny * ky + nz * kz);
				const float32 fres = powf(1.f - Clamp01(nz), p.rimPow > 0.f ? p.rimPow : 3.f);
				// melange ciel/sol par la composante verticale de la normale
				const float32 hemi = ny * 0.5f + 0.5f;

				switch (p.kind) {
					case K_CHECK_NORMAL: {
						// Damier plaque sur la SPHERE (coordonnees spheriques de la normale).
						// Sert a lire d'un coup d'oeil la continuite du lissage : une facette
						// mal soudee casse la grille de facon tres visible.
						const float32 u = atan2f(nx, nz) * (1.f / 6.2831853f) + 0.5f;
						const float32 v = asinf(ny < -1.f ? -1.f : (ny > 1.f ? 1.f : ny)) * (1.f / 3.14159265f) + 0.5f;
						const float32 f = p.extra > 0.f ? p.extra : 8.f;
						const int32 cu = (int32)floorf(u * f), cv = (int32)floorf(v * f);
						const float32 t = (((cu + cv) & 1) != 0) ? 1.f : 0.35f;
						for (int32 c = 0; c < 3; c++)
							out[c] = p.base[c] * t + p.rimInt * fres * p.rimCol[c];
						break;
					}
					case K_CHECK_REFLECT: {
						// Damier sur le vecteur REFLECHI (direction de vue = +Z) :
						//     r = 2 (n.v) n - v,  avec v = (0,0,1)
						// Le motif GLISSE sur la surface quand l'objet tourne, au lieu d'y
						// rester colle. C'est ce qui revele les cassures de normale et les
						// faces retournees qu'un damier plaque ne montre pas.
						const float32 rx = 2.f * nz * nx;
						const float32 ry = 2.f * nz * ny;
						const float32 rz = 2.f * nz * nz - 1.f;
						const float32 u = atan2f(rx, rz) * (1.f / 6.2831853f) + 0.5f;
						const float32 w = asinf(ry < -1.f ? -1.f : (ry > 1.f ? 1.f : ry)) * (1.f / 3.14159265f) + 0.5f;
						// extra = 1 -> bandes horizontales, 2 -> bandes verticales
						const float32 fu = (p.extra < 1.5f) ? 4.f : 16.f;
						const float32 fv = (p.extra < 1.5f) ? 16.f : 4.f;
						const int32 cu = (int32)floorf(u * fu), cv = (int32)floorf(w * fv);
						const float32 t = (((cu + cv) & 1) != 0) ? 0.92f : 0.26f;
						for (int32 c = 0; c < 3; c++)
							out[c] = p.base[c] * t;
						break;
					}
					case K_TOON: {
						// Aplats francs + liseré sombre au bord : la signature du rendu
						// cel-shading. On quantifie apres avoir remonte le contraste, sinon
						// les paliers se lisent comme des anneaux concentriques flous.
						const float32 steps = p.extra > 1.f ? p.extra : 4.f;
						const float32 lit = Clamp01((ndl - 0.12f) * 1.35f);
						float32 q = floorf(lit * steps + 0.001f) / (steps - 1.f);
						q = Clamp01(q) * 0.70f + 0.28f;
						const float32 edge = (nz < 0.34f) ? 0.35f : 1.f; // contour sombre
						for (int32 c = 0; c < 3; c++)
							out[c] = p.base[c] * q * edge;
						break;
					}
					case K_ANISO: {
						// Speculaire ETIRE : on comprime la composante horizontale de la
						// normale AVANT de renormaliser, ce qui allonge la tache dans cette
						// direction — signature du metal brosse. Sans la renormalisation, la
						// norme chute et le reflet disparait au lieu de s'etirer.
						float32 ax = nx * 0.16f, ay = ny, az = nz;
						const float32 al = sqrtf(ax * ax + ay * ay + az * az);
						const float32 ai = al > 1e-6f ? 1.f / al : 0.f;
						ax *= ai;
						ay *= ai;
						az *= ai;
						const float32 st = Clamp01(ax * kx + ay * ky + az * kz);
						const float32 spec = powf(st, p.specPow);
						const float32 amb = p.ambient;
						for (int32 c = 0; c < 3; c++) {
							const float32 env = p.bottom[c] + (p.top[c] - p.bottom[c]) * hemi;
							out[c] = amb * p.base[c] + p.keyInt * ndl * p.base[c] * env +
									 p.specInt * spec + p.rimInt * fres * p.rimCol[c];
						}
						break;
					}
					case K_SKIN: {
						// Approche de diffusion sous-surfacique : on decale le terme diffus
						// vers le rouge et on ajoute une transmission au bord (wrap lighting).
						const float32 wrap = Clamp01((nx * kx + ny * ky + nz * kz) * 0.5f + 0.5f);
						const float32 sss = powf(wrap, 1.6f) * (p.extra > 0.f ? p.extra : 0.6f);
						const float32 spec = powf(ndl, p.specPow);
						for (int32 c = 0; c < 3; c++) {
							const float32 env = p.bottom[c] + (p.top[c] - p.bottom[c]) * hemi;
							out[c] = p.ambient * p.base[c] + p.keyInt * ndl * p.base[c] +
									 sss * p.base[c] * env * 0.55f + p.specInt * spec +
									 p.rimInt * fres * p.rimCol[c];
						}
						break;
					}
					case K_METAL: {
						// Metal : peu de diffus, un horizon net (le "sol" se reflete dans la
						// moitie basse) et un speculaire dur.
						const float32 spec = powf(ndl, p.specPow);
						const float32 horiz = Clamp01((ny + 0.06f) * 6.f); // transition ciel/sol serree
						for (int32 c = 0; c < 3; c++) {
							const float32 env = p.bottom[c] + (p.top[c] - p.bottom[c]) * horiz;
							out[c] = p.ambient * p.base[c] + p.keyInt * ndl * p.base[c] + env * p.base[c] * 0.45f +
									 p.specInt * spec * p.base[c] + p.rimInt * fres * p.rimCol[c];
						}
						break;
					}
					case K_HEMI: {
						const float32 spec = p.specPow > 0.f ? powf(ndl, p.specPow) : 0.f;
						for (int32 c = 0; c < 3; c++) {
							const float32 env = p.bottom[c] + (p.top[c] - p.bottom[c]) * hemi;
							out[c] = p.ambient * p.base[c] + p.keyInt * ndl * p.base[c] * 0.55f + env * p.base[c] +
									 p.specInt * spec + p.rimInt * fres * p.rimCol[c];
						}
						break;
					}
					case K_STUDIO:
					default: {
						const float32 spec = p.specPow > 0.f ? powf(ndl, p.specPow) : 0.f;
						// remplissage venant du cote oppose a la cle : evite les noirs bouches
						const float32 fill = Clamp01(-nx * kx * 0.8f + ny * 0.25f + nz * 0.60f);
						for (int32 c = 0; c < 3; c++) {
							const float32 env = p.bottom[c] + (p.top[c] - p.bottom[c]) * hemi;
							out[c] = p.ambient * p.base[c] + p.keyInt * ndl * p.base[c] +
									 0.22f * fill * p.base[c] * env + p.specInt * spec +
									 p.rimInt * fres * p.rimCol[c];
						}
						break;
					}
				}
				for (int32 c = 0; c < 3; c++)
					out[c] = Clamp01(out[c]);
			}

			inline uint8 ToU8(float32 v) {
				// Encodage sRGB approche : les valeurs ci-dessus sont lineaires, la texture
				// est lue telle quelle par le shader. On applique la meme courbe que le reste
				// du moteur pour que les matcaps ne paraissent pas delavees.
				const float32 s = v <= 0.0031308f ? v * 12.92f : 1.055f * powf(v, 1.f / 2.4f) - 0.055f;
				const float32 c = s < 0.f ? 0.f : (s > 1.f ? 1.f : s);
				return (uint8)(c * 255.f + 0.5f);
			}

			// Ecrit la boule du preset id dans un bloc de largeur stride, a l'origine
			// (ox, oy), sur un cote de "diam" pixels.
			void RenderBall(int32 id, uint8 *dst, uint32 stride, uint32 ox, uint32 oy, uint32 diam) {
				const Preset &p = kPresets[id];
				const float32 bg = p.bg;
				for (uint32 y = 0; y < diam; y++) {
					for (uint32 x = 0; x < diam; x++) {
						// centre des texels : (x+0.5)/diam evite le demi-pixel de decalage
						// qui rendrait la boule asymetrique d'un pixel.
						const float32 nx = ((float32)x + 0.5f) / (float32)diam * 2.f - 1.f;
						const float32 ny = 1.f - ((float32)y + 0.5f) / (float32)diam * 2.f;
						const float32 r2 = nx * nx + ny * ny;
						float32 rgb[3];
						float32 a = 1.f;
						if (r2 >= 1.f) {
							rgb[0] = rgb[1] = rgb[2] = bg;
							a = 0.f;
						} else {
							Shade(p, nx, ny, sqrtf(1.f - r2), rgb);
							// antialiasing du contour sur ~1 texel : sans cela la silhouette
							// de la boule est crenelee et se voit sur les objets tres lisses.
							const float32 r = sqrtf(r2);
							const float32 e = 1.f / (float32)diam * 1.5f;
							if (r > 1.f - e) {
								const float32 t = (1.f - r) / e;
								for (int32 c = 0; c < 3; c++)
									rgb[c] = bg + (rgb[c] - bg) * t;
								a = t;
							}
						}
						uint8 *o = dst + ((uint64)(oy + y) * stride + (ox + x)) * 4;
						o[0] = ToU8(rgb[0]);
						o[1] = ToU8(rgb[1]);
						o[2] = ToU8(rgb[2]);
						o[3] = (uint8)(Clamp01(a) * 255.f + 0.5f);
					}
				}
			}

		} // namespace

		const char *NkMatcapLibrary::Name(int32 id) {
			if (id < 0 || id >= kCount)
				return "";
			return kPresets[id].name;
		}

		void NkMatcapLibrary::GenerateBall(int32 id, uint32 size, uint8 *dst) {
			if (!dst || size == 0)
				return;
			if (id < 0 || id >= kCount)
				id = 0;
			RenderBall(id, dst, size, 0, 0, size);
		}

		void NkMatcapLibrary::GenerateAtlas(uint8 *dst) {
			if (!dst)
				return;
			// Fond neutre partout d'abord : les tuiles inutilisees (kCols*kRows > kCount)
			// et les marges anti-bavure heritent de cette valeur.
			const uint64 n = (uint64)kAtlasW * kAtlasH;
			for (uint64 i = 0; i < n; i++) {
				dst[i * 4 + 0] = 10;
				dst[i * 4 + 1] = 10;
				dst[i * 4 + 2] = 12;
				dst[i * 4 + 3] = 0;
			}
			const uint32 inner = kTile - 2 * kPad;
			for (int32 id = 0; id < kCount; id++) {
				const uint32 cx = (uint32)id % kCols, cy = (uint32)id / kCols;
				RenderBall(id, dst, kAtlasW, cx * kTile + kPad, cy * kTile + kPad, inner);
			}
		}

		void NkMatcapLibrary::TileTransform(int32 id, float32 *outOffsetXY, float32 *outScaleXY) {
			if (id < 0 || id >= kCount)
				id = 0;
			const uint32 cx = (uint32)id % kCols, cy = (uint32)id / kCols;
			const float32 inner = (float32)(kTile - 2 * kPad);
			if (outOffsetXY) {
				outOffsetXY[0] = ((float32)(cx * kTile + kPad)) / (float32)kAtlasW;
				outOffsetXY[1] = ((float32)(cy * kTile + kPad)) / (float32)kAtlasH;
			}
			if (outScaleXY) {
				outScaleXY[0] = inner / (float32)kAtlasW;
				outScaleXY[1] = inner / (float32)kAtlasH;
			}
		}

	} // namespace renderer
} // namespace nkentseu
