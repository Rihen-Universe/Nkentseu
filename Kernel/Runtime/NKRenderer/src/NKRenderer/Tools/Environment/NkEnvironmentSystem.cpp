// =============================================================================
// NkEnvironmentSystem.cpp  — NKRenderer v5.0
//
// D.2d : IBL prefiltering CPU au startup avec cache disque.
//   - BRDF LUT 256x256 RG8     : split-sum integration (Karis 2013) via Hammersley
//   - Irradiance cubemap 32x32 : convolution cos-weighted hemisphere (Lambert)
//   - Prefilter cubemap 128x128 (5 mips) : importance sampling GGX par roughness
//
// Cache disque (nk_ibl_cache.bin par defaut) : premiere execution ~0.5-2s, suivantes <50ms.
// Invalidation automatique si les parametres sky ou les tailles changent (hash FNV-32).
// =============================================================================
#include "NkEnvironmentSystem.h"
#include "NKThreading/NkThreadPool.h"
#include "NKLogger/NkLog.h"
#include "NKFileSystem/NkPath.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"		   // Phase N v0 : lire le .hdr pour LoadFromHDR
#include "NKImage/Core/NkImage.h"		   // Phase N v0 : type NkImage
#include "NKImage/Codecs/HDR/NkHDRCodec.h" // Phase N v0 : decoder Radiance .hdr
#include "NkIBLCompute.h"				   // Phase N v1 : convolutions GPU (compute)
#include "NKTime/NkChrono.h"			   // timings GPU vs CPU
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace nkentseu {
	namespace renderer {

		// ── Cache disque IBL ─────────────────────────────────────────────────────
		// Format : magic(4) + version(4) + hash(4) + irrSize(4) + prefSize(4)
		//        + prefMips(4) + lutSize(4)
		//        + LUT data (lutSize*lutSize*2)
		//        + Irr data (6 * irrSize*irrSize*4)
		//        + Pref data (sum_mip 6*(prefSize>>mip)^2*4)
		static constexpr uint32 kIBLMagic = 0x4E4B4942u; // 'NKIB'
		static constexpr uint32 kIBLVersion = 2u;

		static uint32 IBLHash(const NkVec3f &sky, const NkVec3f &hor, const NkVec3f &gnd, uint32 irrSz, uint32 prefSz,
							  uint32 prefM, uint32 lutSz) {
			uint32 h = 0x811c9dc5u;
			auto mix = [&](uint32 v) { h = (h ^ v) * 0x01000193u; };
			uint32 b;
			auto mf = [&](float v) {
				memcpy(&b, &v, 4);
				mix(b);
			};
			mf(sky.x);
			mf(sky.y);
			mf(sky.z);
			mf(hor.x);
			mf(hor.y);
			mf(hor.z);
			mf(gnd.x);
			mf(gnd.y);
			mf(gnd.z);
			mix(irrSz);
			mix(prefSz);
			mix(prefM);
			mix(lutSz);
			mix(kIBLVersion);
			return h;
		}

		// Hash du ciel COMPLET. Tout parametre qui change l'image DOIT entrer
		// ici : le cache disque est indexe par ce nombre, et un parametre oublie
		// ferait resservir un ciel perime en silence — le pire des defauts,
		// puisque le reglage semble simplement « sans effet ».
		static uint32 IBLHashSky(const NkSkyParams &P, uint32 irrSz, uint32 prefSz, uint32 prefM, uint32 lutSz) {
			uint32 h = IBLHash(P.skyTop, P.horizon, P.ground, irrSz, prefSz, prefM, lutSz);
			auto mix = [&](uint32 v) { h = (h ^ v) * 0x01000193u; };
			uint32 b;
			auto mf = [&](float v) {
				memcpy(&b, &v, 4);
				mix(b);
			};
			mix((uint32)P.model);
			mf(P.sunDirection.x);
			mf(P.sunDirection.y);
			mf(P.sunDirection.z);
			mf(P.turbidity);
			mix(P.sunDisc ? 1u : 0u);
			mf(P.sunIntensity);
			mf(P.sunColor.x);
			mf(P.sunColor.y);
			mf(P.sunColor.z);
			mix(P.clouds ? 1u : 0u);
			mf(P.cloudCoverage);
			mf(P.cloudDensity);
			mf(P.cloudScale);
			mf(P.cloudColor.x);
			mf(P.cloudColor.y);
			mf(P.cloudColor.z);
			return h;
		}

		static NkPath IBLCachePath(const char *dir, uint32 hash) {
			char buf[64];
			snprintf(buf, sizeof(buf), "nk_ibl_%08x.bin", hash);
			NkPath cacheDir;
			if (dir && dir[0]) {
				cacheDir = NkPath(dir) / "ibl";
			} else {
				cacheDir = NkPath::GetExecutableDirectory() / "cache" / "ibl";
			}
			NkDirectory::CreateRecursive(cacheDir);
			return cacheDir / buf;
		}

		// Charge le cache et uploade directement sur le device.
		// Retourne true si le cache est valide et a ete uploade.
		static bool TryLoadIBLCache(const NkPath &path, uint32 hash, NkIDevice *device, NkTextureHandle brdfLUT,
									NkTextureHandle irr, NkTextureHandle pref, uint32 irrSz, uint32 prefSz,
									uint32 prefM, uint32 lutSz) {
			FILE *f = fopen(path.CStr(), "rb");
			if (!f) {
				logger.Info("[IBL] Cache miss (fichier absent) : {0}\n", path.CStr());
				return false;
			}

			uint32 hdr[7];
			if (fread(hdr, 4, 7, f) != 7) {
				fclose(f);
				return false;
			}
			if (hdr[0] != kIBLMagic || hdr[1] != kIBLVersion || hdr[2] != hash || hdr[3] != irrSz || hdr[4] != prefSz ||
				hdr[5] != prefM || hdr[6] != lutSz) {
				logger.Info("[IBL] Cache invalide (hash ou tailles differentes) : {0}\n", path.CStr());
				logger.Info("[IBL]   magic={0:#x} ver={1} hash={2:#x} irr={3} pref={4} mips={5} lut={6}\n", hdr[0],
							hdr[1], hdr[2], hdr[3], hdr[4], hdr[5], hdr[6]);
				fclose(f);
				return false;
			}

			// LUT
			{
				std::vector<uint8_t> buf(lutSz * lutSz * 2);
				if (fread(buf.data(), 1, buf.size(), f) != buf.size()) {
					fclose(f);
					return false;
				}
				device->WriteTexture(brdfLUT, buf.data());
			}
			// Irradiance (6 faces)
			for (uint32 face = 0; face < 6; face++) {
				std::vector<uint8_t> buf(irrSz * irrSz * 4);
				if (fread(buf.data(), 1, buf.size(), f) != buf.size()) {
					fclose(f);
					return false;
				}
				device->WriteTextureRegion(irr, buf.data(), 0, 0, 0, irrSz, irrSz, 1, 0, face);
			}
			// Prefilter (mips x 6 faces)
			for (uint32 mip = 0; mip < prefM; mip++) {
				uint32 mipSz = prefSz >> mip;
				if (mipSz < 1)
					mipSz = 1;
				for (uint32 face = 0; face < 6; face++) {
					std::vector<uint8_t> buf(mipSz * mipSz * 4);
					if (fread(buf.data(), 1, buf.size(), f) != buf.size()) {
						fclose(f);
						return false;
					}
					device->WriteTextureRegion(pref, buf.data(), 0, 0, 0, mipSz, mipSz, 1, mip, face);
				}
			}
			fclose(f);
			logger.Info("[IBL] Cache charge (hit) : {0}\n", path.CStr());
			return true;
		}

		static void SaveIBLCache(const NkPath &path, uint32 hash, uint32 irrSz, uint32 prefSz, uint32 prefM,
								 uint32 lutSz, const uint8_t *lutData, const std::vector<std::vector<uint8_t>> &irrData,
								 const std::vector<std::vector<std::vector<uint8_t>>> &prefData) {
			FILE *f = fopen(path.CStr(), "wb");
			if (!f) {
				logger.Errorf("[IBL] Impossible d'ecrire le cache : {0} (verifier les droits d'ecriture)\n",
							  path.CStr());
				return;
			}
			uint32 hdr[7] = {kIBLMagic, kIBLVersion, hash, irrSz, prefSz, prefM, lutSz};
			fwrite(hdr, 4, 7, f);
			fwrite(lutData, 1, lutSz * lutSz * 2, f);
			for (uint32 face = 0; face < 6; face++)
				fwrite(irrData[face].data(), 1, irrData[face].size(), f);
			for (uint32 mip = 0; mip < prefM; mip++)
				for (uint32 face = 0; face < 6; face++)
					fwrite(prefData[mip][face].data(), 1, prefData[mip][face].size(), f);
			fclose(f);
			logger.Info("[IBL] Cache sauvegarde : {0}\n", path.CStr());
		}

		// ── Helpers cubemap directions (convention OpenGL / Vulkan-equivalente) ──
		// Reconstruit la direction 3D normalisee depuis (face, u, v ∈ [-1,1]).
		static inline void CubemapFaceUVToDir(uint32 face, float u, float v, float &dx, float &dy, float &dz) {
			switch (face) {
				case 0:
					dx = 1.f;
					dy = -v;
					dz = -u;
					break; // +X
				case 1:
					dx = -1.f;
					dy = -v;
					dz = u;
					break; // -X
				case 2:
					dx = u;
					dy = 1.f;
					dz = v;
					break; // +Y (sky)
				case 3:
					dx = u;
					dy = -1.f;
					dz = -v;
					break; // -Y (ground)
				case 4:
					dx = u;
					dy = -v;
					dz = 1.f;
					break; // +Z
				case 5:
					dx = -u;
					dy = -v;
					dz = -1.f;
					break; // -Z
				default:
					dx = 0.f;
					dy = 1.f;
					dz = 0.f;
					break;
			}
			float l = std::sqrt(dx * dx + dy * dy + dz * dz);
			if (l > 1e-6f) {
				dx /= l;
				dy /= l;
				dz /= l;
			}
		}

		// ── Sample HDR equirect (Phase N v0) ────────────────────────────────────
		// Mappe une direction monde (dx,dy,dz) -> UV equirect [0,1] -> pixel
		// RGB96F dans l'image HDR. Nearest neighbor (bilinear future v1).
		// Convention : Y = up, X = "vers l'observateur" a phi=0, atan2 sur (dz, dx).
		static inline NkVec3f SampleEquirect(float dx, float dy, float dz, const NkImage &hdr) {
			// Spherical coords : phi = atan2(dz, dx) in [-π, π], theta = asin(dy)
			const float invPI = 0.31830988618379f;
			const float inv2PI = 0.15915494309189f;
			float phi = std::atan2(dz, dx);
			float theta = std::asin(NkClamp(dy, -1.f, 1.f));
			float u = phi * inv2PI + 0.5f;
			float v = 0.5f - theta * invPI;

			const int32 w = hdr.Width();
			const int32 h = hdr.Height();
			if (w <= 0 || h <= 0 || !hdr.Pixels())
				return {0.f, 0.f, 0.f};

			// Nearest neighbor : px=floor(u*w), py=floor(v*h), clamp.
			int32 px = (int32)(u * (float)w);
			int32 py = (int32)(v * (float)h);
			if (px < 0)
				px = 0;
			else if (px >= w)
				px = w - 1;
			if (py < 0)
				py = 0;
			else if (py >= h)
				py = h - 1;

			const NkImagePixelFormat fmt = hdr.Format();
			const uint8 *row = hdr.RowPtr(py);
			if (fmt == NkImagePixelFormat::NK_RGB96F) {
				const float *p = (const float *)row + (size_t)px * 3;
				return {p[0], p[1], p[2]};
			} else if (fmt == NkImagePixelFormat::NK_RGBA128F) {
				const float *p = (const float *)row + (size_t)px * 4;
				return {p[0], p[1], p[2]};
			}
			return {0.f, 0.f, 0.f};
		}

		// ── Sky gradient procedural (notre "environment input") ─────────────────
		// Gradient ciel haut -> horizon -> sol selon dir.y.
		static inline NkVec3f SampleSkyGradient(float dx, float dy, float /*dz*/, const NkVec3f &skyTop,
												const NkVec3f &horizon, const NkVec3f &ground) {
			(void)dx;
			float t = dy; // [-1, 1]
			NkVec3f c;
			if (t >= 0.f) {
				c.x = horizon.x * (1.f - t) + skyTop.x * t;
				c.y = horizon.y * (1.f - t) + skyTop.y * t;
				c.z = horizon.z * (1.f - t) + skyTop.z * t;
			} else {
				float k = -t;
				c.x = horizon.x * (1.f - k) + ground.x * k;
				c.y = horizon.y * (1.f - k) + ground.y * k;
				c.z = horizon.z * (1.f - k) + ground.z * k;
			}
			return c;
		}

		static inline float NkSkyClamp(float v, float a, float b) {
			return v < a ? a : (v > b ? b : v);
		}

		// ── CIEL PHYSIQUE — Preetham, Shirley & Smits (1999) ────────────────────
		// « A Practical Analytic Model for Daylight ». Modele ANALYTIQUE de
		// diffusion atmospherique : pas de simulation, une formule fermee. Il
		// donne le bleu profond au zenith, le blanchiment vers l'horizon et les
		// teintes chaudes au couchant SANS qu'on ait a les regler — elles sortent
		// de la position du soleil et de la turbidite de l'air.
		//
		// Fonction de Perez : F(theta, gamma) = (1 + A e^(B/cos theta))
		//                                     * (1 + C e^(D gamma) + E cos^2 gamma)
		//   theta = angle zenithal du rayon, gamma = angle rayon-soleil.
		// Les cinq coefficients sont affines en turbidite, un jeu par canal
		// (Y luminance, x et y chromaticite CIE).
		static inline float NkPerez(float cosTheta, float gamma, float cosGamma, const float c[5]) {
			const float ct = cosTheta < 0.01f ? 0.01f : cosTheta; // pole du modele a theta=90
			const float a = 1.f + c[0] * std::exp(c[1] / ct);
			const float b = 1.f + c[2] * std::exp(c[3] * gamma) + c[4] * cosGamma * cosGamma;
			return a * b;
		}

		// CIE xyY -> RGB lineaire (primaires sRGB, blanc D65).
		static inline NkVec3f NkxyYToRGB(float x, float y, float Y) {
			if (y < 1e-4f)
				return {0.f, 0.f, 0.f};
			const float X = x * (Y / y);
			const float Z = (1.f - x - y) * (Y / y);
			const float r = 3.2406f * X - 1.5372f * Y - 0.4986f * Z;
			const float g = -0.9689f * X + 1.8758f * Y + 0.0415f * Z;
			const float b = 0.0557f * X - 0.2040f * Y + 1.0570f * Z;
			return {r < 0.f ? 0.f : r, g < 0.f ? 0.f : g, b < 0.f ? 0.f : b};
		}

		static NkVec3f SampleSkyPhysical(float dx, float dy, float dz, const NkSkyParams &P) {
			// Direction VERS le soleil = oppose de sa direction de propagation
			// (meme convention que NkLightDesc::direction, cf. l'en-tete).
			float sx = -P.sunDirection.x, sy = -P.sunDirection.y, sz = -P.sunDirection.z;
			const float sl = std::sqrt(sx * sx + sy * sy + sz * sz);
			if (sl > 1e-6f) {
				sx /= sl;
				sy /= sl;
				sz /= sl;
			} else {
				sx = 0.f;
				sy = 1.f;
				sz = 0.f;
			}

			const float T = NkSkyClamp(P.turbidity, 1.f, 10.f);
			const float cosTheta = dy;
			const float cosThetaS = NkSkyClamp(sy, -1.f, 1.f);
			const float thetaS = std::acos(cosThetaS);
			const float cosGamma = NkSkyClamp(dx * sx + dy * sy + dz * sz, -1.f, 1.f);
			const float gamma = std::acos(cosGamma);

			const float cY[5] = {0.1787f * T - 1.4630f, -0.3554f * T + 0.4275f, -0.0227f * T + 5.3251f,
								 0.1206f * T - 2.5771f, -0.0670f * T + 0.3703f};
			const float cx[5] = {-0.0193f * T - 0.2592f, -0.0665f * T + 0.0008f, -0.0004f * T + 0.2125f,
								 -0.0641f * T - 0.8989f, -0.0033f * T + 0.0452f};
			const float cy[5] = {-0.0167f * T - 0.2608f, -0.0950f * T + 0.0092f, -0.0079f * T + 0.2102f,
								 -0.0441f * T - 1.6537f, -0.0109f * T + 0.0529f};

			const float t2 = thetaS * thetaS, t3 = t2 * thetaS;
			const float chi = (4.f / 9.f - T / 120.f) * (3.14159265f - 2.f * thetaS);
			float Yz = (4.0453f * T - 4.9710f) * std::tan(chi) - 0.2155f * T + 2.4192f;
			if (Yz < 0.f)
				Yz = 0.f;
			const float xz = T * T * (0.00166f * t3 - 0.00375f * t2 + 0.00209f * thetaS) +
							 T * (-0.02903f * t3 + 0.06377f * t2 - 0.03202f * thetaS + 0.00394f) +
							 (0.11693f * t3 - 0.21196f * t2 + 0.06052f * thetaS + 0.25886f);
			const float yz = T * T * (0.00275f * t3 - 0.00610f * t2 + 0.00317f * thetaS) +
							 T * (-0.04214f * t3 + 0.08970f * t2 - 0.04153f * thetaS + 0.00516f) +
							 (0.15346f * t3 - 0.26756f * t2 + 0.06670f * thetaS + 0.26688f);

			// Normalisation par la valeur au zenith (theta = 0, gamma = thetaS).
			const float dY = NkPerez(1.f, thetaS, cosThetaS, cY);
			const float dXx = NkPerez(1.f, thetaS, cosThetaS, cx);
			const float dYy = NkPerez(1.f, thetaS, cosThetaS, cy);
			const float ct = cosTheta < 0.f ? 0.f : cosTheta;
			const float Yv = Yz * NkPerez(ct, gamma, cosGamma, cY) / (dY < 1e-4f ? 1e-4f : dY);
			const float xv = xz * NkPerez(ct, gamma, cosGamma, cx) / (dXx < 1e-4f ? 1e-4f : dXx);
			const float yv = yz * NkPerez(ct, gamma, cosGamma, cy) / (dYy < 1e-4f ? 1e-4f : dYy);

			// Yz est en kcd/m2 : sans remise a l'echelle le ciel sature tout. Le
			// facteur ci-dessous amene un ciel clair de turbidite 2,5 autour de
			// 1,0 au zenith, ce qui se marie avec le reste du rendu.
			const float kScale = 0.06f;
			NkVec3f c = NkxyYToRGB(xv, yv, Yv * kScale * P.sunIntensity);

			// DISQUE SOLAIRE. Sans lui le ciel physique n'a aucune source
			// visible, ce qui saute aux yeux des qu'on regarde vers le soleil.
			// Rayon apparent reel ~0,27 deg ; on l'elargit un peu et on adoucit
			// le bord, sinon a la resolution de la cubemap il crenele.
			if (P.sunDisc && cosTheta > -0.05f && gamma < 0.030f) {
				float k = 1.f - NkSkyClamp((gamma - 0.009f) / (0.030f - 0.009f), 0.f, 1.f);
				k = k * k;
				const float s = 12.f * P.sunIntensity * k;
				c.x += s;
				// La TEINTE choisie s'applique ICI, sur le disque. La legere derive
				// vers le chaud (0,97 / 0,90) reste : c'est la couleur propre d'un
				// soleil vu a travers l'atmosphere ; la teinte vient par-dessus.
				c.x += s * P.sunColor.x;
				c.y += s * 0.97f * P.sunColor.y;
				c.z += s * 0.90f * P.sunColor.z;
			}

			// SOUS L'HORIZON le modele n'est pas defini (le terme e^(B/cos theta)
			// diverge). On y pose la couleur de sol, avec un fondu court pour
			// eviter une couture nette a l'horizon exact.
			if (dy < 0.f) {
				const float k = NkSkyClamp(-dy / 0.10f, 0.f, 1.f);
				c.x = c.x * (1.f - k) + P.ground.x * k;
				c.y = c.y * (1.f - k) + P.ground.y * k;
				c.z = c.z * (1.f - k) + P.ground.z * k;
			}
			return c;
		}

		// ── NUAGES PROCEDURAUX ──────────────────────────────────────────────────
		// Bruit de valeur + fBm. Le hachage est ENTIER et deterministe : le meme
		// ciel doit se regenerer a l'identique d'une session a l'autre, sans quoi
		// le cache disque servirait une image differente de celle qu'on vient de
		// calculer.
		static inline float NkSkyHash3(int32 x, int32 y, int32 z) {
			uint32 n = (uint32)(x * 374761393) + (uint32)(y * 668265263) + (uint32)(z * 1274126177);
			n = (n ^ (n >> 13)) * 1274126177u;
			n = n ^ (n >> 16);
			return (float)(n & 0x00FFFFFFu) / (float)0x00FFFFFF;
		}

		static inline float NkSkyValueNoise(float x, float y, float z) {
			const float fx = std::floor(x), fy = std::floor(y), fz = std::floor(z);
			const int32 ix = (int32)fx, iy = (int32)fy, iz = (int32)fz;
			float tx = x - fx, ty = y - fy, tz = z - fz;
			// Lissage de Hermite : sans lui les cellules du bruit se voient.
			tx = tx * tx * (3.f - 2.f * tx);
			ty = ty * ty * (3.f - 2.f * ty);
			tz = tz * tz * (3.f - 2.f * tz);
			auto L = [](float a, float b, float t) { return a + (b - a) * t; };
			const float c000 = NkSkyHash3(ix, iy, iz), c100 = NkSkyHash3(ix + 1, iy, iz);
			const float c010 = NkSkyHash3(ix, iy + 1, iz), c110 = NkSkyHash3(ix + 1, iy + 1, iz);
			const float c001 = NkSkyHash3(ix, iy, iz + 1), c101 = NkSkyHash3(ix + 1, iy, iz + 1);
			const float c011 = NkSkyHash3(ix, iy + 1, iz + 1), c111 = NkSkyHash3(ix + 1, iy + 1, iz + 1);
			return L(L(L(c000, c100, tx), L(c010, c110, tx), ty), L(L(c001, c101, tx), L(c011, c111, tx), ty), tz);
		}

		static inline float NkSkyFbm(float x, float y, float z, int32 octaves) {
			float sum = 0.f, amp = 0.5f, norm = 0.f;
			for (int32 i = 0; i < octaves; i++) {
				sum += amp * NkSkyValueNoise(x, y, z);
				norm += amp;
				x *= 2.02f;
				y *= 2.02f;
				z *= 2.02f;
				amp *= 0.5f;
			}
			return norm > 0.f ? sum / norm : 0.f;
		}

		static inline NkVec3f ApplyClouds(NkVec3f sky, float dx, float dy, float dz, const NkSkyParams &P) {
			if (!P.clouds || dy <= 0.001f)
				return sky; // pas de nuage sous l'horizon
			// PROJECTION SUR UN PLAN a altitude constante : diviser par dy fait
			// que les nuages s'ecrasent et s'etirent en approchant de l'horizon,
			// comme dans la realite. Sans cette division ils garderaient la meme
			// taille apparente partout, et le ciel ressemblerait a une boule
			// texturee plutot qu'a une voute.
			const float s = P.cloudScale <= 0.f ? 1.f : P.cloudScale;
			const float inv = 1.f / (dy < 0.05f ? 0.05f : dy);
			const float px = dx * inv * s;
			const float pz = dz * inv * s;
			const float n = NkSkyFbm(px, pz, 0.5f, 5);

			// La COUVERTURE est un SEUIL sur le bruit, pas une opacite : a 0 il
			// ne reste rien, a 1 le ciel est plein. C'est ce qui donne des nuages
			// qui naissent et grossissent, au lieu d'un voile uniforme qui se
			// contenterait de foncer.
			const float cov = NkSkyClamp(P.cloudCoverage, 0.f, 1.f);
			if (cov <= 0.f)
				return sky;
			float d = (n - (1.f - cov)) / (cov < 1e-3f ? 1e-3f : cov);
			d = NkSkyClamp(d, 0.f, 1.f);
			d *= (P.cloudDensity < 0.f ? 0.f : P.cloudDensity);
			d *= NkSkyClamp(dy / 0.12f, 0.f, 1.f); // la projection degenere a l'horizon
			d = NkSkyClamp(d, 0.f, 1.f);

			// Les nuages sont ECLAIRES par le ciel qu'ils masquent : on melange
			// vers leur couleur propre ponderee par la luminosite locale du ciel.
			// Un blanc pur sur un ciel sombre trahirait le collage.
			const float lit = NkSkyClamp((sky.x + sky.y + sky.z) * 0.5f, 0.25f, 1.5f);
			const NkVec3f cc{P.cloudColor.x * lit, P.cloudColor.y * lit, P.cloudColor.z * lit};
			return {sky.x + (cc.x - sky.x) * d, sky.y + (cc.y - sky.y) * d, sky.z + (cc.z - sky.z) * d};
		}

		// Point d'entree UNIQUE du ciel procedural : modele de base puis nuages.
		// Les quatre sites d'echantillonnage (cubemap visible, irradiance et les
		// deux du prefilter) passent par ici — sans quoi le ciel qu'on VOIT et
		// celui qui ECLAIRE finiraient par diverger.
		static inline NkVec3f SampleSkyModel(float dx, float dy, float dz, const NkSkyParams &P) {
			const NkVec3f c = (P.model == NkSkyModel::NK_SKY_PHYSICAL)
								  ? SampleSkyPhysical(dx, dy, dz, P)
								  : SampleSkyGradient(dx, dy, dz, P.skyTop, P.horizon, P.ground);
			return ApplyClouds(c, dx, dy, dz, P);
		}

		// ── Hammersley sequence (low-discrepancy 2D) ────────────────────────────
		// Van der Corput radical inverse base 2.
		static inline float RadicalInverseVdC(uint32 bits) {
			bits = (bits << 16u) | (bits >> 16u);
			bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
			bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
			bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
			bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
			return float(bits) * 2.3283064365386963e-10f;
		}

		static inline void Hammersley(uint32 i, uint32 N, float &x, float &y) {
			x = float(i) / float(N);
			y = RadicalInverseVdC(i);
		}

		// ── Construction d'un repere TBN orthonorme depuis N ─────────────────────
		static inline void BuildTBN(float Nx, float Ny, float Nz, float &Tx, float &Ty, float &Tz, float &Bx, float &By,
									float &Bz) {
			float ax = std::fabs(Ny) < 0.999f ? 0.f : 1.f;
			float ay = std::fabs(Ny) < 0.999f ? 1.f : 0.f;
			float az = 0.f;
			// T = normalize(cross(up, N))
			Tx = ay * Nz - az * Ny;
			Ty = az * Nx - ax * Nz;
			Tz = ax * Ny - ay * Nx;
			float l = std::sqrt(Tx * Tx + Ty * Ty + Tz * Tz);
			if (l > 1e-6f) {
				Tx /= l;
				Ty /= l;
				Tz /= l;
			}
			// B = cross(N, T)
			Bx = Ny * Tz - Nz * Ty;
			By = Nz * Tx - Nx * Tz;
			Bz = Nx * Ty - Ny * Tx;
		}

		// ── Importance sample GGX dans le repere de N ───────────────────────────
		// xi ∈ [0,1)^2, retourne la half-vector H en world space.
		static inline void ImportanceSampleGGX(float xiX, float xiY, float roughness, float Nx, float Ny, float Nz,
											   float &Hx, float &Hy, float &Hz) {
			float a = roughness * roughness;
			float phi = 6.28318530718f * xiX;
			float cosTheta = std::sqrt((1.f - xiY) / (1.f + (a * a - 1.f) * xiY));
			float sinTheta = std::sqrt(std::fmax(0.f, 1.f - cosTheta * cosTheta));

			float lx = sinTheta * std::cos(phi);
			float ly = sinTheta * std::sin(phi);
			float lz = cosTheta;

			// World-space via TBN
			float Tx, Ty, Tz, Bx, By, Bz;
			BuildTBN(Nx, Ny, Nz, Tx, Ty, Tz, Bx, By, Bz);
			Hx = Tx * lx + Bx * ly + Nx * lz;
			Hy = Ty * lx + By * ly + Ny * lz;
			Hz = Tz * lx + Bz * ly + Nz * lz;
			float l = std::sqrt(Hx * Hx + Hy * Hy + Hz * Hz);
			if (l > 1e-6f) {
				Hx /= l;
				Hy /= l;
				Hz /= l;
			}
		}

		// ── G_Smith pour BRDF LUT (Karis variation k=a/2) ───────────────────────
		static inline float G_Schlick(float cosT, float k) {
			return cosT / (cosT * (1.f - k) + k);
		}

		static inline float G_Smith_IBL(float NoV, float NoL, float roughness) {
			float a = roughness;
			float k = (a * a) / 2.f;
			return G_Schlick(NoV, k) * G_Schlick(NoL, k);
		}

		// ── Integrate split-sum BRDF (Karis 2013) ───────────────────────────────
		// Retourne (scale, bias) avec F0_scale = 1-(1-VoH)^5*(1-Fc) etc.
		static inline void IntegrateBRDF(float NoV, float roughness, uint32 numSamples, float &outScale,
										 float &outBias) {
			// V dans le plan XZ avec V.z = NoV (frame ou N=Z).
			float Vx = std::sqrt(std::fmax(0.f, 1.f - NoV * NoV));
			float Vy = 0.f;
			float Vz = NoV;

			float A = 0.f, B = 0.f;
			for (uint32 i = 0; i < numSamples; i++) {
				float xiX, xiY;
				Hammersley(i, numSamples, xiX, xiY);
				float Hx, Hy, Hz;
				ImportanceSampleGGX(xiX, xiY, roughness, 0.f, 0.f, 1.f, Hx, Hy, Hz);

				// L = reflect(-V, H) = 2*(V·H)*H - V
				float VoH = Vx * Hx + Vy * Hy + Vz * Hz;
				float Lx = 2.f * VoH * Hx - Vx;
				float Ly = 2.f * VoH * Hy - Vy;
				float Lz = 2.f * VoH * Hz - Vz;

				float NoL = std::fmax(Lz, 0.f);
				float NoH = std::fmax(Hz, 0.f);
				VoH = std::fmax(VoH, 0.f);

				if (NoL > 0.f) {
					float G = G_Smith_IBL(NoV, NoL, roughness);
					float G_Vis = (G * VoH) / std::fmax(NoH * NoV, 1e-6f);
					float Fc = std::pow(1.f - VoH, 5.f);
					A += (1.f - Fc) * G_Vis;
					B += Fc * G_Vis;
				}
			}
			outScale = A / float(numSamples);
			outBias = B / float(numSamples);
		}

		NkEnvironmentSystem::~NkEnvironmentSystem() {
			Shutdown();
		}

		bool NkEnvironmentSystem::Init(NkIDevice *device, const NkEnvironmentConfig &cfg) {
			mDevice = device;
			mCfg = cfg;

			// ── Cree les textures GPU avec leurs tailles finales ────────────────
			const uint32 irrSize = mCfg.irradianceSize > 0 ? mCfg.irradianceSize : 32;
			const uint32 prefSize = mCfg.prefilterSize > 0 ? mCfg.prefilterSize : 128;
			const uint32 prefMips = mCfg.prefilterMips > 0 ? mCfg.prefilterMips : 5;
			const uint32 lutSize = mCfg.brdfLUTSize > 0 ? mCfg.brdfLUTSize : 256;

			{
				auto td = NkTextureDesc::Cubemap(irrSize, NkGPUFormat::NK_RGBA8_UNORM, 1);
				td.debugName = "EnvIrradiance";
				mIrradiance = mDevice->CreateTexture(td);
			}
			{
				auto td = NkTextureDesc::Cubemap(prefSize, NkGPUFormat::NK_RGBA8_UNORM, prefMips);
				td.debugName = "EnvPrefilter";
				mPrefilter = mDevice->CreateTexture(td);
			}
			{
				auto td = NkTextureDesc::Tex2D(lutSize, lutSize, NkGPUFormat::NK_RG8_UNORM, 1);
				td.debugName = "BRDFLUT";
				mBrdfLUT = mDevice->CreateTexture(td);
			}
			// Phase N v1 : cubemap dedie skybox HDR brut (RGBA32F, mip 0).
			// Format full-float pour preserver les valeurs > 1.0 (sans
			// Reinhard tonemap) — c'est ce qui rend le sky HDR "vivant".
			{
				auto td = NkTextureDesc::Cubemap(prefSize, NkGPUFormat::NK_RGBA32_FLOAT, 1);
				td.debugName = "SkyEnvCube";
				mSkyEnvCube = mDevice->CreateTexture(td);
			}

			mEnvSampler = mDevice->CreateSampler(NkSamplerDesc::Clamp());
			mLutSampler = mDevice->CreateSampler(NkSamplerDesc::Clamp());

			// Phase N v0 : dispatch selon la source choisie par l'app.
			//   PROCEDURAL : gradient sky parametrable (default, retro-compat)
			//   HDR_FILE   : charge un .hdr equirect 360 depuis cfg.hdrPath
			//   NONE       : pas d'auto-load, l'app appellera Load*() manuellement
			switch (mCfg.source) {
				case NkEnvSource::NK_ENV_PROCEDURAL:
					LoadProcedural(mCfg.skyTop, mCfg.horizon, mCfg.ground);
					break;
				case NkEnvSource::NK_ENV_HDR_FILE:
					if (!mCfg.hdrPath.Empty()) {
						if (!LoadFromHDR(mCfg.hdrPath)) {
							// Fallback procedural si echec du chargement
							logger.Warnf("[NkEnvironmentSystem] HDR load failed (%s), "
										 "fallback procedural sky\n",
										 mCfg.hdrPath.CStr());
							LoadProcedural(mCfg.skyTop, mCfg.horizon, mCfg.ground);
						}
					} else {
						logger.Warnf("[NkEnvironmentSystem] source=HDR_FILE mais hdrPath vide, "
									 "fallback procedural sky\n");
						LoadProcedural(mCfg.skyTop, mCfg.horizon, mCfg.ground);
					}
					break;
				case NkEnvSource::NK_ENV_NONE:
					// L'app appellera LoadProcedural ou LoadFromHDR plus tard.
					// Les textures restent vides (valeur GPU par defaut = noir).
					break;
			}

			return mIrradiance.IsValid() && mPrefilter.IsValid() && mBrdfLUT.IsValid();
		}

		// Ancienne signature CONSERVEE : elle se ramene au modele degrade, donc
		// tous les appels existants produisent exactement le meme ciel qu'avant.
		void NkEnvironmentSystem::LoadProcedural(const NkVec3f &skyTop, const NkVec3f &horizon, const NkVec3f &ground) {
			NkSkyParams p;
			p.model = NkSkyModel::NK_SKY_GRADIENT;
			p.skyTop = skyTop;
			p.horizon = horizon;
			p.ground = ground;
			LoadProceduralEx(p);
		}

		void NkEnvironmentSystem::LoadProceduralEx(const NkSkyParams &P) {
			if (!mDevice)
				return;
			// Alias locaux : le corps ci-dessous n'a pas change de forme, seuls
			// les points d'echantillonnage passent desormais par SampleSkyModel.
			const NkVec3f &skyTop = P.skyTop;
			const NkVec3f &horizon = P.horizon;
			const NkVec3f &ground = P.ground;
			(void)skyTop;
			(void)horizon;
			(void)ground;

			const uint32 irrSize = mCfg.irradianceSize > 0 ? mCfg.irradianceSize : 32;
			const uint32 prefSize = mCfg.prefilterSize > 0 ? mCfg.prefilterSize : 128;
			const uint32 prefMips = mCfg.prefilterMips > 0 ? mCfg.prefilterMips : 5;
			const uint32 lutSize = mCfg.brdfLUTSize > 0 ? mCfg.brdfLUTSize : 256;

			auto &pool = ::nkentseu::threading::NkThreadPool::GetGlobal();

			// ── Phase N v1 : skybox cubemap procedural (gradient direct) ────
			// En mode PROCEDURAL on n'a pas de "vrai HDR" — on sample le
			// gradient sky directement sans tonemap (les valeurs sont deja
			// dans [0,1]). Genere TOUJOURS avant le cache check IBL.
			if (mSkyEnvCube.IsValid()) {
				std::vector<std::vector<float>> skyData(6);
				auto skyFaceWork = [&](uint32 face) {
					auto &buf = skyData[face];
					buf.assign(prefSize * prefSize * 4, 0.f);
					for (uint32 y = 0; y < prefSize; y++) {
						for (uint32 x = 0; x < prefSize; x++) {
							float u = ((float)x + 0.5f) / (float)prefSize * 2.f - 1.f;
							float v = ((float)y + 0.5f) / (float)prefSize * 2.f - 1.f;
							float Nx, Ny, Nz;
							CubemapFaceUVToDir(face, u, v, Nx, Ny, Nz);
							NkVec3f s = SampleSkyModel(Nx, Ny, Nz, P);
							uint32 idx = (y * prefSize + x) * 4;
							buf[idx + 0] = s.x;
							buf[idx + 1] = s.y;
							buf[idx + 2] = s.z;
							buf[idx + 3] = 1.f;
						}
					}
				};
				pool.ParallelFor(6, [&](nk_size f) { skyFaceWork((uint32)f); }, 1);
				pool.Join();
				for (uint32 f = 0; f < 6; f++)
					mDevice->WriteTextureRegion(mSkyEnvCube, skyData[f].data(), 0, 0, 0, prefSize, prefSize, 1, 0, f);
			}

			// ── Cache disque ────────────────────────────────────────────────────
			uint32 hash = IBLHashSky(P, irrSize, prefSize, prefMips, lutSize);
			if (mCfg.enableCache) {
				auto path = IBLCachePath(mCfg.cacheDir, hash);
				if (TryLoadIBLCache(path, hash, mDevice, mBrdfLUT, mIrradiance, mPrefilter, irrSize, prefSize, prefMips,
									lutSize)) {
					return; // charge depuis cache : aucun calcul CPU
				}
			}

			// ── BRDF LUT ────────────────────────────────────────────────────────
			// 32 samples suffisent pour un gradient sky sans hautes frequences.
			std::vector<uint8_t> lutData(lutSize * lutSize * 2);
			if (mBrdfLUT.IsValid()) {
				const uint32 N = 32;
				pool.ParallelFor(
					lutSize,
					[&](nk_size yi) {
						uint32 y = (uint32)yi;
						float roughness = (float(y) + 0.5f) / float(lutSize);
						for (uint32 x = 0; x < lutSize; x++) {
							float NoV = (float(x) + 0.5f) / float(lutSize);
							float A = 0.f, B = 0.f;
							IntegrateBRDF(NoV, roughness, N, A, B);
							uint32 idx = (y * lutSize + x) * 2;
							lutData[idx + 0] = uint8_t(NkClamp(A, 0.f, 1.f) * 255.f);
							lutData[idx + 1] = uint8_t(NkClamp(B, 0.f, 1.f) * 255.f);
						}
					},
					/*grainSize=*/8);
				pool.Join();
				mDevice->WriteTexture(mBrdfLUT, lutData.data());
			}

			// ── Irradiance convolution ──────────────────────────────────────────
			// 4 strates × 16 azimuts = 64 samples : suffisant pour ciel gradient.
			std::vector<std::vector<uint8_t>> irrData(6);
			if (mIrradiance.IsValid()) {
				auto irrFaceWork = [&](uint32 face) {
					auto &buf = irrData[face];
					buf.assign(irrSize * irrSize * 4, 0);
					const float kPI = 3.14159265358979f;
					const uint32 nTheta = 4;
					const uint32 nPhi = 16;
					const float dTheta = 0.5f * kPI / float(nTheta);
					const float dPhi = 2.0f * kPI / float(nPhi);

					for (uint32 y = 0; y < irrSize; y++) {
						for (uint32 x = 0; x < irrSize; x++) {
							float u = ((float)x + 0.5f) / (float)irrSize * 2.f - 1.f;
							float v = ((float)y + 0.5f) / (float)irrSize * 2.f - 1.f;
							float Nx, Ny, Nz;
							CubemapFaceUVToDir(face, u, v, Nx, Ny, Nz);
							float Tx, Ty, Tz, Bx, By, Bz;
							BuildTBN(Nx, Ny, Nz, Tx, Ty, Tz, Bx, By, Bz);
							float Cx = 0.f, Cy = 0.f, Cz = 0.f;
							uint32 nSamp = 0;
							for (uint32 ti = 0; ti < nTheta; ti++) {
								float theta = (float(ti) + 0.5f) * dTheta;
								float sT = std::sin(theta), cT = std::cos(theta);
								for (uint32 pi = 0; pi < nPhi; pi++) {
									float phi = (float(pi) + 0.5f) * dPhi;
									float sP = std::sin(phi), cP = std::cos(phi);
									float lx = sT * cP, ly = sT * sP, lz = cT;
									float Wx = Tx * lx + Bx * ly + Nx * lz;
									float Wy = Ty * lx + By * ly + Ny * lz;
									float Wz = Tz * lx + Bz * ly + Nz * lz;
									NkVec3f s = SampleSkyModel(Wx, Wy, Wz, P);
									float w = cT * sT;
									Cx += s.x * w;
									Cy += s.y * w;
									Cz += s.z * w;
									nSamp++;
								}
							}
							float scale = kPI / float(nSamp);
							Cx *= scale;
							Cy *= scale;
							Cz *= scale;
							uint32 idx = (y * irrSize + x) * 4;
							buf[idx + 0] = (uint8_t)(NkClamp(Cx, 0.f, 1.f) * 255.f);
							buf[idx + 1] = (uint8_t)(NkClamp(Cy, 0.f, 1.f) * 255.f);
							buf[idx + 2] = (uint8_t)(NkClamp(Cz, 0.f, 1.f) * 255.f);
							buf[idx + 3] = 255;
						}
					}
				};
				pool.ParallelFor(6, [&](nk_size f) { irrFaceWork((uint32)f); }, 1);
				pool.Join();
				for (uint32 f = 0; f < 6; f++)
					mDevice->WriteTextureRegion(mIrradiance, irrData[f].data(), 0, 0, 0, irrSize, irrSize, 1, 0, f);
			}

			// ── Prefilter GGX par mip ───────────────────────────────────────────
			// 16 samples : qualite correcte pour ciel sans hautes frequences.
			std::vector<std::vector<std::vector<uint8_t>>> prefData(prefMips, std::vector<std::vector<uint8_t>>(6));
			if (mPrefilter.IsValid()) {
				const uint32 numSamples = 16;
				for (uint32 mip = 0; mip < prefMips; mip++) {
					uint32 mipSize = prefSize >> mip;
					if (mipSize < 1)
						mipSize = 1;
					float roughness = (prefMips > 1) ? float(mip) / float(prefMips - 1) : 0.f;
					auto &mipBufs = prefData[mip];

					auto prefFaceWork = [&](uint32 face) {
						auto &buf = mipBufs[face];
						buf.assign(mipSize * mipSize * 4, 0);
						for (uint32 y = 0; y < mipSize; y++) {
							for (uint32 x = 0; x < mipSize; x++) {
								float u = ((float)x + 0.5f) / (float)mipSize * 2.f - 1.f;
								float v = ((float)y + 0.5f) / (float)mipSize * 2.f - 1.f;
								float Nx, Ny, Nz;
								CubemapFaceUVToDir(face, u, v, Nx, Ny, Nz);
								float Vx = Nx, Vy = Ny, Vz = Nz;
								if (roughness < 1e-3f) {
									NkVec3f s = SampleSkyModel(Nx, Ny, Nz, P);
									uint32 idx = (y * mipSize + x) * 4;
									buf[idx + 0] = (uint8_t)(NkClamp(s.x, 0.f, 1.f) * 255.f);
									buf[idx + 1] = (uint8_t)(NkClamp(s.y, 0.f, 1.f) * 255.f);
									buf[idx + 2] = (uint8_t)(NkClamp(s.z, 0.f, 1.f) * 255.f);
									buf[idx + 3] = 255;
									continue;
								}
								float Cx = 0.f, Cy = 0.f, Cz = 0.f, sumW = 0.f;
								for (uint32 i = 0; i < numSamples; i++) {
									float xiX, xiY;
									Hammersley(i, numSamples, xiX, xiY);
									float Hx, Hy, Hz;
									ImportanceSampleGGX(xiX, xiY, roughness, Nx, Ny, Nz, Hx, Hy, Hz);
									float VoH = Vx * Hx + Vy * Hy + Vz * Hz;
									float Lx = 2.f * VoH * Hx - Vx;
									float Ly = 2.f * VoH * Hy - Vy;
									float Lz = 2.f * VoH * Hz - Vz;
									float NoL = std::fmax(Nx * Lx + Ny * Ly + Nz * Lz, 0.f);
									if (NoL > 0.f) {
										NkVec3f s = SampleSkyModel(Lx, Ly, Lz, P);
										Cx += s.x * NoL;
										Cy += s.y * NoL;
										Cz += s.z * NoL;
										sumW += NoL;
									}
								}
								float inv = (sumW > 1e-6f) ? 1.f / sumW : 0.f;
								Cx *= inv;
								Cy *= inv;
								Cz *= inv;
								uint32 idx = (y * mipSize + x) * 4;
								buf[idx + 0] = (uint8_t)(NkClamp(Cx, 0.f, 1.f) * 255.f);
								buf[idx + 1] = (uint8_t)(NkClamp(Cy, 0.f, 1.f) * 255.f);
								buf[idx + 2] = (uint8_t)(NkClamp(Cz, 0.f, 1.f) * 255.f);
								buf[idx + 3] = 255;
							}
						}
					};
					pool.ParallelFor(6, [&](nk_size f) { prefFaceWork((uint32)f); }, 1);
					pool.Join();
					for (uint32 f = 0; f < 6; f++)
						mDevice->WriteTextureRegion(mPrefilter, mipBufs[f].data(), 0, 0, 0, mipSize, mipSize, 1, mip,
													f);
				}
			}

			// ── Sauvegarde du cache pour les prochains lancements ───────────────
			if (mCfg.enableCache) {
				auto path = IBLCachePath(mCfg.cacheDir, hash);
				SaveIBLCache(path, hash, irrSize, prefSize, prefMips, lutSize, lutData.data(), irrData, prefData);
			}
		}

		// ── Phase N v0 : LoadFromHDR ────────────────────────────────────────
		// Charge un .hdr equirect 360 et l'utilise comme source pour les
		// convolutions irradiance + prefilter. CPU-side (future v1 = compute
		// shader GPU). Hash de cache = path + tailles config (pas le mtime
		// pour cette v0 — clear cache manuel si on swap le .hdr).
		bool NkEnvironmentSystem::LoadFromHDR(const NkString &path) {
			if (!mDevice)
				return false;
			if (path.Empty()) {
				logger.Warnf("[NkEnvironmentSystem] LoadFromHDR : path vide\n");
				return false;
			}

			// Lire les bytes du fichier
			auto bytes = NkFile::ReadAllBytes(path.CStr());
			if (bytes.Empty()) {
				logger.Warnf("[NkEnvironmentSystem] LoadFromHDR : impossible de lire '%s'\n", path.CStr());
				return false;
			}

			// Decode HDR (Radiance RGBE) -> NkImage RGB96F
			NkImage *hdr = NkHDRCodec::Decode(bytes.Data(), bytes.Size());
			if (!hdr || !hdr->IsValid()) {
				logger.Warnf("[NkEnvironmentSystem] LoadFromHDR : decode HDR echoue '%s'\n", path.CStr());
				if (hdr)
					hdr->Free();
				return false;
			}
			if (!hdr->IsHDR()) {
				logger.Warnf("[NkEnvironmentSystem] LoadFromHDR : format non-HDR '%s' (format=%d)\n", path.CStr(),
							 (int)hdr->Format());
				hdr->Free();
				return false;
			}
			logger.Infof("[NkEnvironmentSystem] LoadFromHDR : '%s' (%dx%d, %s)\n", path.CStr(), hdr->Width(),
						 hdr->Height(), hdr->Format() == NkImagePixelFormat::NK_RGB96F ? "RGB96F" : "RGBA128F");

			const uint32 irrSize = mCfg.irradianceSize > 0 ? mCfg.irradianceSize : 32;
			const uint32 prefSize = mCfg.prefilterSize > 0 ? mCfg.prefilterSize : 128;
			const uint32 prefMips = mCfg.prefilterMips > 0 ? mCfg.prefilterMips : 5;
			const uint32 lutSize = mCfg.brdfLUTSize > 0 ? mCfg.brdfLUTSize : 256;

			// ── Cache disque : hash sur path + tailles (pas le contenu) ─────
			// Suffit pour eviter de re-calculer si on relance avec le meme
			// HDR + memes tailles. Pour invalider, clear le fichier manuellement.
			uint32 hash = 0x811c9dc5u;
			auto mix = [&](uint32 v) { hash = (hash ^ v) * 0x01000193u; };
			for (uint32 i = 0; i < path.Size(); ++i)
				mix((uint32)(uint8)path[i]);
			mix(irrSize);
			mix(prefSize);
			mix(prefMips);
			mix(lutSize);
			mix(kIBLVersion);
			mix(0x48445201u); // marker "HDR1" pour ne pas collisionner avec LoadProcedural

			auto &pool = ::nkentseu::threading::NkThreadPool::GetGlobal();

			// ── Phase N v1 : skybox cubemap HDR brut (sans Reinhard) ────────
			// Genere TOUJOURS (avant le cache check IBL) car ce cubemap n'est
			// pas serialise dans nk_ibl_*.bin (regeneration ~10ms, negligeable).
			// Mirror direct du HDR equirect sur les 6 faces du cube en RGBA32F.
			if (mSkyEnvCube.IsValid()) {
				std::vector<std::vector<float>> skyData(6);
				auto skyFaceWork = [&](uint32 face) {
					auto &buf = skyData[face];
					buf.assign(prefSize * prefSize * 4, 0.f);
					for (uint32 y = 0; y < prefSize; y++) {
						for (uint32 x = 0; x < prefSize; x++) {
							float u = ((float)x + 0.5f) / (float)prefSize * 2.f - 1.f;
							float v = ((float)y + 0.5f) / (float)prefSize * 2.f - 1.f;
							float Nx, Ny, Nz;
							CubemapFaceUVToDir(face, u, v, Nx, Ny, Nz);
							NkVec3f s = SampleEquirect(Nx, Ny, Nz, *hdr);
							uint32 idx = (y * prefSize + x) * 4;
							buf[idx + 0] = s.x;
							buf[idx + 1] = s.y;
							buf[idx + 2] = s.z;
							buf[idx + 3] = 1.f;
						}
					}
				};
				pool.ParallelFor(6, [&](nk_size f) { skyFaceWork((uint32)f); }, 1);
				pool.Join();
				for (uint32 f = 0; f < 6; f++)
					mDevice->WriteTextureRegion(mSkyEnvCube, skyData[f].data(), 0, 0, 0, prefSize, prefSize, 1, 0, f);
			}

			if (mCfg.enableCache) {
				auto cpath = IBLCachePath(mCfg.cacheDir, hash);
				if (TryLoadIBLCache(cpath, hash, mDevice, mBrdfLUT, mIrradiance, mPrefilter, irrSize, prefSize,
									prefMips, lutSize)) {
					hdr->Free();
					return true;
				}
			}

			// ── BRDF LUT (identique a LoadProcedural — universel) ───────────
			std::vector<uint8_t> lutData(lutSize * lutSize * 2);
			if (mBrdfLUT.IsValid()) {
				const uint32 N = 32;
				pool.ParallelFor(
					lutSize,
					[&](nk_size yi) {
						uint32 y = (uint32)yi;
						float roughness = (float(y) + 0.5f) / float(lutSize);
						for (uint32 x = 0; x < lutSize; x++) {
							float NoV = (float(x) + 0.5f) / float(lutSize);
							float A = 0.f, B = 0.f;
							IntegrateBRDF(NoV, roughness, N, A, B);
							uint32 idx = (y * lutSize + x) * 2;
							lutData[idx + 0] = uint8_t(NkClamp(A, 0.f, 1.f) * 255.f);
							lutData[idx + 1] = uint8_t(NkClamp(B, 0.f, 1.f) * 255.f);
						}
					},
					/*grainSize=*/8);
				pool.Join();
				mDevice->WriteTexture(mBrdfLUT, lutData.data());
			}

			// ── Phase N v1 : convolutions sur GPU (compute) ─────────────────
			// Remplit les MÊMES buffers que le chemin CPU (packing RGBA8
			// identique) → upload et cache disque inchangés. Sur le moindre
			// échec (backend sans compute, kernel KO), gpuDone reste false et
			// les blocs CPU ci-dessous s'exécutent comme avant.
			std::vector<std::vector<uint8_t>> irrData(6);
			std::vector<std::vector<std::vector<uint8_t>>> prefData(prefMips, std::vector<std::vector<uint8_t>>(6));
			bool gpuDone = false;

			const char *envGpu = std::getenv("NK_IBL_GPU");
			// DX11 : NK_IBL_VERIFY montre maxDiff=175/255 sur ~0.8% des texels
			// (fxc cs_5_0 ; GL/VK/DX12 sont a 5/255 pres) — comme le resultat
			// alimente le cache disque, DX11 reste sur le CPU par defaut.
			// NK_IBL_GPU=1 force le GPU (debug), NK_IBL_GPU=0 force le CPU.
			const bool dx11 = mDevice->GetApi() == NkGraphicsApi::NK_GFX_API_DX11;
			const bool envForceOn = envGpu != nullptr && envGpu[0] == '1';
			const bool envForceOff = envGpu != nullptr && envGpu[0] == '0';
			const bool wantGpu = mCfg.gpuConvolution && !envForceOff && (!dx11 || envForceOn) &&
								 mIrradiance.IsValid() && mPrefilter.IsValid();
			if (wantGpu) {
				const float64 tGpu0 = ::nkentseu::NkChrono::Now().nanoseconds;

				// Equirect → RGBA float32 contigu (SSBO source du kernel).
				const uint32 hw = (uint32)hdr->Width();
				const uint32 hh = (uint32)hdr->Height();
				const NkImagePixelFormat hfmt = hdr->Format();
				std::vector<float> rgba((size_t)hw * hh * 4, 0.f);
				for (uint32 yy = 0; yy < hh; ++yy) {
					const uint8 *row = hdr->RowPtr((int32)yy);
					float *dst = rgba.data() + (size_t)yy * hw * 4;
					if (hfmt == NkImagePixelFormat::NK_RGB96F) {
						const float *s = (const float *)row;
						for (uint32 xx = 0; xx < hw; ++xx) {
							dst[xx * 4 + 0] = s[xx * 3 + 0];
							dst[xx * 4 + 1] = s[xx * 3 + 1];
							dst[xx * 4 + 2] = s[xx * 3 + 2];
							dst[xx * 4 + 3] = 1.f;
						}
					} else { // NK_RGBA128F (garanti par le check format plus haut)
						std::memcpy(dst, row, (size_t)hw * 4 * sizeof(float));
					}
				}

				NkIBLCompute gpu;
				const bool gpuInit = gpu.Init(mDevice);
				const float64 tCompile = ::nkentseu::NkChrono::Now().nanoseconds;
				if (gpuInit && gpu.UploadHDR(rgba.data(), hw, hh)) {
					bool ok = true;

					uint8_t *irrPtr[6];
					for (uint32 f = 0; f < 6; ++f) {
						irrData[f].assign((size_t)irrSize * irrSize * 4, 0);
						irrPtr[f] = irrData[f].data();
					}
					ok = gpu.ConvolveIrradiance(irrSize, irrPtr);

					for (uint32 mip = 0; ok && mip < prefMips; ++mip) {
						uint32 mipSize = prefSize >> mip;
						if (mipSize < 1)
							mipSize = 1;
						const float roughness = (prefMips > 1) ? float(mip) / float(prefMips - 1) : 0.f;
						uint8_t *prefPtr[6];
						for (uint32 f = 0; f < 6; ++f) {
							prefData[mip][f].assign((size_t)mipSize * mipSize * 4, 0);
							prefPtr[f] = prefData[mip][f].data();
						}
						ok = gpu.PrefilterMip(mipSize, roughness, /*numSamples=*/32, prefPtr);
					}

					if (ok) {
						gpuDone = true;
						const float64 now = ::nkentseu::NkChrono::Now().nanoseconds;
						const float64 compileMs = (tCompile - tGpu0) / 1.0e6;
						const float64 convMs = (now - tCompile) / 1.0e6;
						logger.Infof("[IBL] Convolutions GPU : %.1f ms (compile kernels %.1f ms one-shot + "
									 "convolution %.1f ms) — irr %ux%u + prefilter %ux%u x%u mips\n",
									 compileMs + convMs, compileMs, convMs, irrSize, irrSize, prefSize, prefSize,
									 prefMips);
					} else {
						logger.Warnf("[IBL] Convolution GPU echouee — fallback CPU\n");
					}
				}
			}

			// NK_IBL_VERIFY=1 : garde une copie du resultat GPU, force le
			// recalcul CPU ci-dessous, puis loggue l'ecart max (validation
			// numerique headless, backend par backend).
			std::vector<std::vector<uint8_t>> irrGpuCopy;
			std::vector<std::vector<std::vector<uint8_t>>> prefGpuCopy;
			const char *envVerify = std::getenv("NK_IBL_VERIFY");
			const bool verify = gpuDone && envVerify != nullptr && envVerify[0] == '1';
			if (verify) {
				irrGpuCopy = irrData;
				prefGpuCopy = prefData;
				gpuDone = false; // ré-exécute le CPU dans irrData/prefData
			}

			// ── Irradiance convolution (CPU, fallback / verify) ─────────────
			// Lambert weighted hemisphere, 4 strates × 16 azimuts = 64 samples
			const float64 tCpu0 = ::nkentseu::NkChrono::Now().nanoseconds;
			if (mIrradiance.IsValid() && !gpuDone) {
				auto irrFaceWork = [&](uint32 face) {
					auto &buf = irrData[face];
					buf.assign(irrSize * irrSize * 4, 0);
					const float kPI = 3.14159265358979f;
					const uint32 nTheta = 4;
					const uint32 nPhi = 16;
					const float dTheta = 0.5f * kPI / float(nTheta);
					const float dPhi = 2.0f * kPI / float(nPhi);
					for (uint32 y = 0; y < irrSize; y++) {
						for (uint32 x = 0; x < irrSize; x++) {
							float u = ((float)x + 0.5f) / (float)irrSize * 2.f - 1.f;
							float v = ((float)y + 0.5f) / (float)irrSize * 2.f - 1.f;
							float Nx, Ny, Nz;
							CubemapFaceUVToDir(face, u, v, Nx, Ny, Nz);
							float Tx, Ty, Tz, Bx, By, Bz;
							BuildTBN(Nx, Ny, Nz, Tx, Ty, Tz, Bx, By, Bz);
							float Cx = 0.f, Cy = 0.f, Cz = 0.f;
							uint32 nSamp = 0;
							for (uint32 ti = 0; ti < nTheta; ti++) {
								float theta = (float(ti) + 0.5f) * dTheta;
								float sT = std::sin(theta), cT = std::cos(theta);
								for (uint32 pi = 0; pi < nPhi; pi++) {
									float phi = (float(pi) + 0.5f) * dPhi;
									float sP = std::sin(phi), cP = std::cos(phi);
									float lx = sT * cP, ly = sT * sP, lz = cT;
									float Wx = Tx * lx + Bx * ly + Nx * lz;
									float Wy = Ty * lx + By * ly + Ny * lz;
									float Wz = Tz * lx + Bz * ly + Nz * lz;
									NkVec3f s = SampleEquirect(Wx, Wy, Wz, *hdr);
									float w = cT * sT;
									Cx += s.x * w;
									Cy += s.y * w;
									Cz += s.z * w;
									nSamp++;
								}
							}
							float scale = kPI / float(nSamp);
							Cx *= scale;
							Cy *= scale;
							Cz *= scale;
							// Clamp + tonemap simple Reinhard pour eviter saturation
							// (HDR vrai a des valeurs > 1.0 qui clamperaient en blanc).
							Cx = Cx / (1.f + Cx);
							Cy = Cy / (1.f + Cy);
							Cz = Cz / (1.f + Cz);
							uint32 idx = (y * irrSize + x) * 4;
							buf[idx + 0] = (uint8_t)(NkClamp(Cx, 0.f, 1.f) * 255.f);
							buf[idx + 1] = (uint8_t)(NkClamp(Cy, 0.f, 1.f) * 255.f);
							buf[idx + 2] = (uint8_t)(NkClamp(Cz, 0.f, 1.f) * 255.f);
							buf[idx + 3] = 255;
						}
					}
				};
				pool.ParallelFor(6, [&](nk_size f) { irrFaceWork((uint32)f); }, 1);
				pool.Join();
			}
			if (mIrradiance.IsValid()) {
				for (uint32 f = 0; f < 6; f++)
					mDevice->WriteTextureRegion(mIrradiance, irrData[f].data(), 0, 0, 0, irrSize, irrSize, 1, 0, f);
			}

			// ── Prefilter GGX par mip (CPU, fallback / verify) ──────────────
			// 32 samples : qualite correcte pour HDR a hautes frequences (vs
			// 16 pour gradient procedural). Garde le mip 0 mirror du HDR.
			if (mPrefilter.IsValid() && !gpuDone) {
				const uint32 numSamples = 32;
				for (uint32 mip = 0; mip < prefMips; mip++) {
					uint32 mipSize = prefSize >> mip;
					if (mipSize < 1)
						mipSize = 1;
					float roughness = (prefMips > 1) ? float(mip) / float(prefMips - 1) : 0.f;
					auto &mipBufs = prefData[mip];

					auto prefFaceWork = [&](uint32 face) {
						auto &buf = mipBufs[face];
						buf.assign(mipSize * mipSize * 4, 0);
						for (uint32 y = 0; y < mipSize; y++) {
							for (uint32 x = 0; x < mipSize; x++) {
								float u = ((float)x + 0.5f) / (float)mipSize * 2.f - 1.f;
								float v = ((float)y + 0.5f) / (float)mipSize * 2.f - 1.f;
								float Nx, Ny, Nz;
								CubemapFaceUVToDir(face, u, v, Nx, Ny, Nz);
								float Vx = Nx, Vy = Ny, Vz = Nz;
								if (roughness < 1e-3f) {
									NkVec3f s = SampleEquirect(Nx, Ny, Nz, *hdr);
									s.x = s.x / (1.f + s.x);
									s.y = s.y / (1.f + s.y);
									s.z = s.z / (1.f + s.z);
									uint32 idx = (y * mipSize + x) * 4;
									buf[idx + 0] = (uint8_t)(NkClamp(s.x, 0.f, 1.f) * 255.f);
									buf[idx + 1] = (uint8_t)(NkClamp(s.y, 0.f, 1.f) * 255.f);
									buf[idx + 2] = (uint8_t)(NkClamp(s.z, 0.f, 1.f) * 255.f);
									buf[idx + 3] = 255;
									continue;
								}
								float Cx = 0.f, Cy = 0.f, Cz = 0.f, sumW = 0.f;
								for (uint32 i = 0; i < numSamples; i++) {
									float xiX, xiY;
									Hammersley(i, numSamples, xiX, xiY);
									float Hx, Hy, Hz;
									ImportanceSampleGGX(xiX, xiY, roughness, Nx, Ny, Nz, Hx, Hy, Hz);
									float VoH = Vx * Hx + Vy * Hy + Vz * Hz;
									float Lx = 2.f * VoH * Hx - Vx;
									float Ly = 2.f * VoH * Hy - Vy;
									float Lz = 2.f * VoH * Hz - Vz;
									float NoL = std::fmax(Nx * Lx + Ny * Ly + Nz * Lz, 0.f);
									if (NoL > 0.f) {
										NkVec3f s = SampleEquirect(Lx, Ly, Lz, *hdr);
										Cx += s.x * NoL;
										Cy += s.y * NoL;
										Cz += s.z * NoL;
										sumW += NoL;
									}
								}
								float inv = (sumW > 1e-6f) ? 1.f / sumW : 0.f;
								Cx *= inv;
								Cy *= inv;
								Cz *= inv;
								// Reinhard tonemap
								Cx = Cx / (1.f + Cx);
								Cy = Cy / (1.f + Cy);
								Cz = Cz / (1.f + Cz);
								uint32 idx = (y * mipSize + x) * 4;
								buf[idx + 0] = (uint8_t)(NkClamp(Cx, 0.f, 1.f) * 255.f);
								buf[idx + 1] = (uint8_t)(NkClamp(Cy, 0.f, 1.f) * 255.f);
								buf[idx + 2] = (uint8_t)(NkClamp(Cz, 0.f, 1.f) * 255.f);
								buf[idx + 3] = 255;
							}
						}
					};
					pool.ParallelFor(6, [&](nk_size f) { prefFaceWork((uint32)f); }, 1);
					pool.Join();
				}
			}
			if (mPrefilter.IsValid()) {
				for (uint32 mip = 0; mip < prefMips; mip++) {
					uint32 mipSize = prefSize >> mip;
					if (mipSize < 1)
						mipSize = 1;
					for (uint32 f = 0; f < 6; f++)
						mDevice->WriteTextureRegion(mPrefilter, prefData[mip][f].data(), 0, 0, 0, mipSize, mipSize, 1,
													mip, f);
				}
			}

			if (!gpuDone) {
				const float64 cpuMs = (::nkentseu::NkChrono::Now().nanoseconds - tCpu0) / 1.0e6;
				logger.Infof("[IBL] Convolutions CPU : %.1f ms\n", cpuMs);
			}

			// ── NK_IBL_VERIFY=1 : ecart GPU vs CPU (octets RGBA8) ───────────
			if (verify) {
				uint32 maxDiff = 0;
				uint64 nDiff = 0;
				uint64 nTotal = 0;
				auto compare = [&](const std::vector<uint8_t> &a, const std::vector<uint8_t> &b) {
					const size_t n = a.size() < b.size() ? a.size() : b.size();
					for (size_t i = 0; i < n; ++i) {
						const uint32 d = (uint32)(a[i] > b[i] ? a[i] - b[i] : b[i] - a[i]);
						if (d > maxDiff)
							maxDiff = d;
						if (d != 0)
							++nDiff;
						++nTotal;
					}
				};
				for (uint32 f = 0; f < 6; ++f)
					compare(irrGpuCopy[f], irrData[f]);
				for (uint32 mip = 0; mip < prefMips; ++mip)
					for (uint32 f = 0; f < 6; ++f)
						compare(prefGpuCopy[mip][f], prefData[mip][f]);
				logger.Infof("[IBL] VERIFY GPU vs CPU : maxDiff=%u/255, octets differents=%llu/%llu (%.3f%%)\n",
							 maxDiff, (unsigned long long)nDiff, (unsigned long long)nTotal,
							 nTotal ? 100.0 * (float64)nDiff / (float64)nTotal : 0.0);
			}

			// ── Sauvegarde du cache ─────────────────────────────────────────
			// Note v1 : mSkyEnvCube n'est PAS dans le cache (genere avant le
			// cache check, regenere a chaque boot ~10ms negligeable).
			if (mCfg.enableCache) {
				auto cpath = IBLCachePath(mCfg.cacheDir, hash);
				SaveIBLCache(cpath, hash, irrSize, prefSize, prefMips, lutSize, lutData.data(), irrData, prefData);
			}

			hdr->Free();
			return true;
		}

		void NkEnvironmentSystem::Shutdown() {
			if (mLutSampler.IsValid()) {
				mDevice->DestroySampler(mLutSampler);
				mLutSampler = {};
			}
			if (mEnvSampler.IsValid()) {
				mDevice->DestroySampler(mEnvSampler);
				mEnvSampler = {};
			}
			if (mBrdfLUT.IsValid()) {
				mDevice->DestroyTexture(mBrdfLUT);
				mBrdfLUT = {};
			}
			if (mSkyEnvCube.IsValid()) {
				mDevice->DestroyTexture(mSkyEnvCube);
				mSkyEnvCube = {};
			}
			if (mPrefilter.IsValid()) {
				mDevice->DestroyTexture(mPrefilter);
				mPrefilter = {};
			}
			if (mIrradiance.IsValid()) {
				mDevice->DestroyTexture(mIrradiance);
				mIrradiance = {};
			}
		}

	} // namespace renderer
} // namespace nkentseu
