// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkFluidGridRaymarch.cpp — marche de rayon dans une NkFluidGrid (Kajiya & Von
// Herzen 1984) et couleur du corps noir (Planck 1901 + CIE 1931 approchées par
// Wyman, Sloan & Shirley 2013 + sRGB IEC 61966-2-1). Voir l'en-tête.
// =============================================================================
#include "NkFluidGridRaymarch.h"
#include "NKTime/NkChrono.h" // en dernier (cf. NkVFXSystem.cpp : namespace `time`)

namespace nkentseu {
	namespace renderer {

		using namespace math;

		// =====================================================================
		// Intersection rayon / boîte alignée (méthode des dalles, « slab test »).
		// Rend faux si le rayon manque la boîte ou si elle est entièrement derrière.
		// =====================================================================
		static bool RayBox(const NkVec3f &o, const NkVec3f &d, const NkVec3f &bmin, const NkVec3f &bmax, float32 &t0,
						   float32 &t1) {
			float32 tmin = -1.0e30f, tmax = 1.0e30f;
			const float32 od[3] = {o.x, o.y, o.z};
			const float32 dd[3] = {d.x, d.y, d.z};
			const float32 lo[3] = {bmin.x, bmin.y, bmin.z};
			const float32 hi[3] = {bmax.x, bmax.y, bmax.z};
			for (uint32 a = 0; a < 3; ++a) {
				if (dd[a] > -1.0e-12f && dd[a] < 1.0e-12f) {
					if (od[a] < lo[a] || od[a] > hi[a])
						return false;
					continue;
				}
				const float32 inv = 1.f / dd[a];
				float32 ta = (lo[a] - od[a]) * inv;
				float32 tb = (hi[a] - od[a]) * inv;
				if (ta > tb) {
					const float32 s = ta;
					ta = tb;
					tb = s;
				}
				if (ta > tmin)
					tmin = ta;
				if (tb < tmax)
					tmax = tb;
				if (tmin > tmax)
					return false;
			}
			if (tmax <= 0.f)
				return false;
			t0 = (tmin > 0.f) ? tmin : 0.f;
			t1 = tmax;
			return true;
		}

		// =====================================================================
		// Le rendu
		// =====================================================================
		void NkFluidRaymarchRender(const NkFluidGrid &grid, const NkFluidRaymarchParams &p, NkVector<uint8> &rgba,
								   NkFluidRaymarchStats &stats, NkVector<float32> *maxTempOut) {
			const int64 t0ns = ::nkentseu::NkChrono::Now().nanoseconds;
			stats = NkFluidRaymarchStats();

			const uint32 W = p.width, H = p.height;
			rgba.Resize((usize)W * (usize)H * 4u, 0);
			if (maxTempOut)
				maxTempOut->Resize((usize)W * (usize)H, 0.f);
			if (W == 0 || H == 0)
				return;

			const NkVec3f bmin = grid.Params().boundsMin;
			const NkVec3f bmax = grid.Params().boundsMax;
			const float32 ds = (p.stepSize > 0.f) ? p.stepSize : (grid.CellSize() * 0.5f);
			const float32 dsL = ds * ((p.shadowStepFactor > 0.f) ? p.shadowStepFactor : 1.f);
			const float32 sigmaT = p.absorption + p.scattering;
			const float32 albedoScat = (sigmaT > 0.f) ? (p.scattering / sigmaT) : 0.f;
			const NkVec3f L = p.lightDir.Normalized();

			// Repère caméra
			const NkVec3f fwd = (p.cameraTarget - p.cameraPos).Normalized();
			NkVec3f right = fwd.Cross(p.up);
			if (right.Dot(right) < 1.0e-12f)
				right = {1.f, 0.f, 0.f};
			right.Normalize();
			const NkVec3f upv = right.Cross(fwd).Normalized();
			const float32 tanHalf = NkTan(0.5f * p.fovDegrees * 3.14159265f / 180.f);
			const float32 aspect = (float32)W / (float32)H;

			const uint8 bgR = (uint8)(NkClamp(p.background.x, 0.f, 1.f) * 255.f + 0.5f);
			const uint8 bgG = (uint8)(NkClamp(p.background.y, 0.f, 1.f) * 255.f + 0.5f);
			const uint8 bgB = (uint8)(NkClamp(p.background.z, 0.f, 1.f) * 255.f + 0.5f);

			for (uint32 py = 0; py < H; ++py) {
				for (uint32 px = 0; px < W; ++px) {
					++stats.rays;
					const float32 sx = (2.f * (((float32)px + 0.5f) / (float32)W) - 1.f) * tanHalf * aspect;
					const float32 sy = (1.f - 2.f * (((float32)py + 0.5f) / (float32)H)) * tanHalf;
					const NkVec3f dir = (fwd + right * sx + upv * sy).Normalized();

					float32 ta = 0.f, tb = 0.f;
					NkVec3f color = {0.f, 0.f, 0.f};
					float32 T = 1.f;
					float32 tempMax = 0.f;

					if (RayBox(p.cameraPos, dir, bmin, bmax, ta, tb)) {
						++stats.raysHit;
						for (float32 t = ta + 0.5f * ds; t < tb && T > p.transmittanceCutoff; t += ds) {
							const NkVec3f q = p.cameraPos + dir * t;
							const float32 dens = grid.SampleDensityWorld(q);
							const float32 temp = p.emission ? grid.SampleTemperatureWorld(q) : 0.f;
							if (temp > tempMax)
								tempMax = temp;
							++stats.samples;

							const bool emet = p.emission && temp > p.emissionMinTemperature;
							if (dens <= 1.0e-6f && !emet)
								continue;

							const float32 st = dens * sigmaT;
							const float32 alpha = 1.f - NkExp(-st * ds);

							// Éclairage : ambiante + lumière atténuée par une SECONDE marche.
							float32 shadow = 1.f;
							if (p.shadowMarch && dens > 1.0e-6f) {
								float32 la = 0.f, lb = 0.f;
								if (RayBox(q, L, bmin, bmax, la, lb)) {
									float32 tau = 0.f;
									const float32 lmax =
										(p.shadowMaxDistance > 0.f && p.shadowMaxDistance < lb) ? p.shadowMaxDistance : lb;
									for (float32 s = 0.5f * dsL; s < lmax; s += dsL) {
										tau += grid.SampleDensityWorld(q + L * s) * sigmaT * dsL;
										++stats.shadowSamples;
										if (tau > 8.f)
											break;
									}
									shadow = NkExp(-tau);
								}
							}

							if (alpha > 0.f) {
								const NkVec3f lit = {p.ambient.x + p.lightColor.x * shadow,
													 p.ambient.y + p.lightColor.y * shadow,
													 p.ambient.z + p.lightColor.z * shadow};
								const float32 w = T * alpha * albedoScat;
								color.x += w * p.albedo.x * lit.x;
								color.y += w * p.albedo.y * lit.y;
								color.z += w * p.albedo.z * lit.z;
							}

							// ÉMISSION (feu) : couleur du corps noir a la temperature locale,
							// pesee par la radiance relative de Stefan-Boltzmann.
							if (emet) {
								const NkVec3f bb = NkBlackBodyColor(temp);
								float32 e = p.emissionStrength *
											NkBlackBodyRelativeRadiance(temp, p.emissionMinTemperature) * ds;
								if (p.emissionUsesDensity)
									e *= dens;
								color.x += T * e * bb.x;
								color.y += T * e * bb.y;
								color.z += T * e * bb.z;
							}

							T *= (1.f - alpha);
						}
					}

					const float32 r = color.x + T * p.background.x;
					const float32 g = color.y + T * p.background.y;
					const float32 b = color.z + T * p.background.z;
					const usize o = ((usize)py * (usize)W + (usize)px) * 4u;
					// Le fond est écrit tel quel quand rien n'a été rencontré : le
					// contrôle négatif du banc compte les pixels qui en diffèrent.
					if (T >= 1.f && color.x == 0.f && color.y == 0.f && color.z == 0.f) {
						rgba[o + 0] = bgR;
						rgba[o + 1] = bgG;
						rgba[o + 2] = bgB;
					} else {
						rgba[o + 0] = (uint8)(NkClamp(r, 0.f, 1.f) * 255.f + 0.5f);
						rgba[o + 1] = (uint8)(NkClamp(g, 0.f, 1.f) * 255.f + 0.5f);
						rgba[o + 2] = (uint8)(NkClamp(b, 0.f, 1.f) * 255.f + 0.5f);
					}
					rgba[o + 3] = 255;
					if (maxTempOut)
						(*maxTempOut)[(usize)py * (usize)W + (usize)px] = tempMax;
				}
			}
			stats.ms = (float32)((::nkentseu::NkChrono::Now().nanoseconds - t0ns) / 1.0e6);
		}

		// =====================================================================
		// CORPS NOIR — rien n'est recopié d'une table de couleurs : tout est CALCULÉ.
		// =====================================================================

		// Constantes SI EXACTES depuis la révision 2019 du Système international :
		// h = 6,626 070 15e-34 J.s ; c = 2,997 924 58e8 m/s ; k = 1,380 649e-23 J/K.
		static const float64 kPlanckH = 6.62607015e-34;
		static const float64 kLightC = 2.99792458e8;
		static const float64 kBoltzK = 1.380649e-23;

		// Loi de PLANCK (1901) : radiance spectrale d'un corps noir, W.m^-3.sr^-1.
		// En float64 : lambda^5 vaut ~3e-32 dans le visible, un float32 s'y écrase.
		static float64 PlanckRadiance(float64 lambdaMeters, float64 kelvin) {
			if (lambdaMeters <= 0.0 || kelvin <= 0.0)
				return 0.0;
			const float64 l5 = lambdaMeters * lambdaMeters * lambdaMeters * lambdaMeters * lambdaMeters;
			const float64 a = 2.0 * kPlanckH * kLightC * kLightC / l5;
			const float64 x = kPlanckH * kLightC / (lambdaMeters * kBoltzK * kelvin);
			// exp d'un float64 : on passe par NkExp en float32 quand c'est sûr, sinon
			// on borne — au-delà de x = 80 le dénominateur est exp(x) à 1e-35 près.
			if (x > 80.0)
				return a * (float64)NkExp(-(float32)NkMin(x, 300.0));
			const float64 e = (float64)NkExp((float32)x);
			return a / (e - 1.0);
		}

		// Gaussienne par morceaux de WYMAN, SLOAN & SHIRLEY (JCGT 2013), eq. (2) :
		// deux écarts-types, un de chaque côté du sommet.
		static float64 PieceGauss(float64 x, float64 mu, float64 s1, float64 s2) {
			const float64 t = (x - mu) * ((x < mu) ? (1.0 / s1) : (1.0 / s2));
			return (float64)NkExp((float32)(-0.5 * t * t));
		}

		// Fonctions colorimétriques CIE 1931 2°, ajustement MULTI-LOBES de
		// Wyman, Sloan & Shirley (JCGT 2013), eq. (3) : lambda en NANOMÈTRES.
		static void CieXYZBar(float64 nm, float64 &xb, float64 &yb, float64 &zb) {
			xb = 1.056 * PieceGauss(nm, 599.8, 37.9, 31.0) + 0.362 * PieceGauss(nm, 442.0, 16.0, 26.7) -
				 0.065 * PieceGauss(nm, 501.1, 20.4, 26.2);
			yb = 0.821 * PieceGauss(nm, 568.8, 46.9, 40.5) + 0.286 * PieceGauss(nm, 530.9, 16.3, 31.1);
			zb = 1.217 * PieceGauss(nm, 437.0, 11.8, 36.0) + 0.681 * PieceGauss(nm, 459.0, 26.0, 13.8);
		}

		// Encodage gamma sRGB — IEC 61966-2-1.
		static float32 SrgbEncode(float64 c) {
			if (c <= 0.0)
				return 0.f;
			if (c >= 1.0)
				return 1.f;
			if (c <= 0.0031308)
				return (float32)(12.92 * c);
			return (float32)(1.055 * (float64)NkPow((float32)c, 1.f / 2.4f) - 0.055);
		}

		// XYZ (D65) -> sRGB linéaire — matrice de la norme IEC 61966-2-1.
		static void XyzToLinearSrgb(float64 X, float64 Y, float64 Z, float64 &r, float64 &g, float64 &b) {
			r = 3.2406 * X - 1.5372 * Y - 0.4986 * Z;
			g = -0.9689 * X + 1.8758 * Y + 0.0415 * Z;
			b = 0.0557 * X - 0.2040 * Y + 1.0570 * Z;
		}

		// Intégration d'un spectre quelconque sur le visible, pas de 1 nm.
		// `mode` : 0 = Planck a `kelvin`, 1 = spectre d'ÉNERGIE ÉGALE (contrôle).
		static void SpectrumToXYZ(float64 kelvin, int32 mode, float64 &X, float64 &Y, float64 &Z) {
			X = Y = Z = 0.0;
			for (int32 nm = 360; nm <= 830; ++nm) {
				const float64 s = (mode == 1) ? 1.0 : PlanckRadiance((float64)nm * 1.0e-9, kelvin);
				float64 xb, yb, zb;
				CieXYZBar((float64)nm, xb, yb, zb);
				X += s * xb;
				Y += s * yb;
				Z += s * zb;
			}
		}

		// Exposée pour le banc : la chromaticité CIE xy d'un corps noir.
		// (Déclarée ici et pas dans l'en-tête : c'est un instrument de contrôle.)
		void NkBlackBodyChromaticity(float32 kelvin, float32 &x, float32 &y) {
			float64 X, Y, Z;
			SpectrumToXYZ((float64)kelvin, 0, X, Y, Z);
			const float64 s = X + Y + Z;
			x = (s > 0.0) ? (float32)(X / s) : 0.f;
			y = (s > 0.0) ? (float32)(Y / s) : 0.f;
		}

		// Le contrôle du chemin colorimétrique SEUL : un spectre d'énergie égale doit
		// tomber sur le point blanc E, x = y = 1/3, quelles que soient les gaussiennes.
		void NkEqualEnergyChromaticity(float32 &x, float32 &y) {
			float64 X, Y, Z;
			SpectrumToXYZ(0.0, 1, X, Y, Z);
			const float64 s = X + Y + Z;
			x = (s > 0.0) ? (float32)(X / s) : 0.f;
			y = (s > 0.0) ? (float32)(Y / s) : 0.f;
		}

		NkVec3f NkBlackBodyColor(float32 kelvin) {
			float64 X, Y, Z;
			SpectrumToXYZ((float64)kelvin, 0, X, Y, Z);
			const float64 s = X + Y + Z;
			if (s <= 0.0)
				return {0.f, 0.f, 0.f};
			// Normalisation en chromaticité : on garde la TEINTE, la luminance est
			// portée à part (Stefan-Boltzmann). Y = 1 puis remise à l'échelle sur le max.
			X /= Y;
			Z /= Y;
			float64 r, g, b;
			XyzToLinearSrgb(X, 1.0, Z, r, g, b);
			// Une chromaticité de corps noir chaud sort du triangle sRGB : on ramène
			// les négatifs a zéro (désaturation minimale), c'est dit.
			if (r < 0.0)
				r = 0.0;
			if (g < 0.0)
				g = 0.0;
			if (b < 0.0)
				b = 0.0;
			float64 m = r;
			if (g > m)
				m = g;
			if (b > m)
				m = b;
			if (m <= 0.0)
				return {0.f, 0.f, 0.f};
			r /= m;
			g /= m;
			b /= m;
			return {SrgbEncode(r), SrgbEncode(g), SrgbEncode(b)};
		}

		float32 NkBlackBodyRelativeRadiance(float32 kelvin, float32 reference) {
			// STEFAN-BOLTZMANN : la puissance rayonnée va comme T^4.
			if (reference <= 0.f || kelvin <= 0.f)
				return 0.f;
			const float32 r = kelvin / reference;
			return r * r * r * r;
		}

		float32 NkWienPeakWavelength(float32 kelvin) {
			// Loi du déplacement de WIEN, b = 2,897 771 955e-3 m.K (CODATA 2018).
			return (kelvin > 0.f) ? (2.897771955e-3f / kelvin) : 0.f;
		}

		// Le maximum de Planck TROUVÉ PAR BALAYAGE (et non par la formule de Wien) :
		// c'est le contrôle croisé de l'implémentation de Planck elle-même.
		float32 NkPlanckPeakWavelengthScanned(float32 kelvin, float32 nmMin, float32 nmMax, float32 nmStep) {
			float64 best = -1.0;
			float32 bestNm = 0.f;
			for (float32 nm = nmMin; nm <= nmMax; nm += nmStep) {
				const float64 v = PlanckRadiance((float64)nm * 1.0e-9, (float64)kelvin);
				if (v > best) {
					best = v;
					bestNm = nm;
				}
			}
			return bestNm * 1.0e-9f;
		}

	} // namespace renderer
} // namespace nkentseu
