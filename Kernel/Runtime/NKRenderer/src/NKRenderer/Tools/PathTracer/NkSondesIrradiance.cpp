// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    NKRenderer/Tools/PathTracer/NkSondesIrradiance.cpp
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// Les sondes d'irradiance cuites par le traceur : voir NkSondesIrradiance.h.

#include "NKRenderer/Tools/PathTracer/NkSondesIrradiance.h"
#include "NKThreading/NkThreadPool.h"
#include "NKCore/Text/NkSortie.h"
#include "NKMath/NkFunctions.h"
#include "NKTime/NkChrono.h"
#include "NKPlatform/NkEnv.h"

namespace nkentseu {
	namespace renderer {

		namespace {
			constexpr float32 kPiSd = 3.14159265358979f;
			constexpr int32 kSdLimiteTexture = 2048; // bord d'une texture 3D (D3D11 : 2048)

			// le lobe de la visibilite : max(0, n.w)^32, par cinq carres
			inline float32 Lobe32(float32 c) {
				if (c <= 0.80f) // 0,8^32 = 8e-4 : negligeable
					return 0.f;
				const float32 c2 = c * c, c4 = c2 * c2, c8 = c4 * c4, c16 = c8 * c8;
				return c16 * c16;
			}

			struct Hasard {
					uint64 s;
					uint32 Suivant() {
						const uint64 old = s;
						s = old * 6364136223846793005ULL + 1442695040888963407ULL;
						const uint32 x = (uint32)(((old >> 18u) ^ old) >> 27u);
						const uint32 r = (uint32)(old >> 59u);
						return (x >> r) | (x << ((0u - r) & 31u));
					}
					float32 F() {
						return (float32)(Suivant() >> 8) * (1.f / 16777216.f);
					}
			};
			inline uint64 Melanger(uint64 z) {
				z += 0x9E3779B97F4A7C15ULL;
				z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
				z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
				return z ^ (z >> 31);
			}

			// Les directions d'une sonde : la spirale de Fibonacci sur la sphere (repartition
			// reguliere), tournee au hasard pour chaque sonde (rotation uniforme de Shoemake) : les
			// sondes voisines ne partagent pas les memes trous.
			void Directions(int32 n, uint64 graine, float32 *dirs) {
				Hasard h{Melanger(graine)};
				const float32 u1 = h.F(), u2 = h.F(), u3 = h.F();
				const float32 a = math::NkSqrt(1.f - u1), b = math::NkSqrt(u1);
				const float32 qx = a * math::NkSin(2.f * kPiSd * u2), qy = a * math::NkCos(2.f * kPiSd * u2);
				const float32 qz = b * math::NkSin(2.f * kPiSd * u3), qw = b * math::NkCos(2.f * kPiSd * u3);
				const float32 orAngle = 2.39996322972865f; // pi (3 - racine de 5)
				for (int32 k = 0; k < n; ++k) {
					const float32 y = 1.f - (2.f * (float32)k + 1.f) / (float32)n;
					const float32 r = math::NkSqrt(y > 1.f || y < -1.f ? 0.f : 1.f - y * y);
					const float32 ph = orAngle * (float32)k;
					const float32 vx = r * math::NkCos(ph), vy = y, vz = r * math::NkSin(ph);
					// v' = v + 2w (q x v) + 2 q x (q x v)
					const float32 cx = qy * vz - qz * vy, cy = qz * vx - qx * vz, cz = qx * vy - qy * vx;
					const float32 ccx = qy * cz - qz * cy, ccy = qz * cx - qx * cz, ccz = qx * cy - qy * cx;
					dirs[k * 3] = vx + 2.f * (qw * cx + ccx);
					dirs[k * 3 + 1] = vy + 2.f * (qw * cy + ccy);
					dirs[k * 3 + 2] = vz + 2.f * (qw * cz + ccz);
				}
			}

			inline usize Index(const NkSdVolume &v, int32 x, int32 y, int32 z) {
				return (((usize)z * (usize)v.H + (usize)y) * (usize)v.W + (usize)x) * 4u;
			}

			// Le bord d'une carte (repli octaedrique) : chaque texel de bord recopie le texel interieur
			// qui lui est voisin SUR LA SPHERE, pour que le filtrage bilineaire raccorde les aretes.
			void Bords(NkSdVolume &v, int32 x0, int32 y0, int32 z) {
				const int32 T = kSdTexels;
				auto copie = [&](int32 bx, int32 by, int32 ix, int32 iy) {
					const usize d = Index(v, x0 + bx, y0 + by, z), s = Index(v, x0 + ix, y0 + iy, z);
					for (int32 q = 0; q < 4; ++q)
						v.texels[d + (usize)q] = v.texels[s + (usize)q];
				};
				for (int32 i = 1; i <= T; ++i) {
					copie(i, 0, T + 1 - i, 1);
					copie(i, T + 1, T + 1 - i, T);
					copie(0, i, 1, T + 1 - i);
					copie(T + 1, i, T, T + 1 - i);
				}
				copie(0, 0, T, T);
				copie(T + 1, 0, 1, T);
				copie(0, T + 1, T, 1);
				copie(T + 1, T + 1, 1, 1);
			}
		} // namespace

		// ── l'octaedre ─────────────────────────────────────────────────────────────────
		void NkSdOctEncoder(const float32 d[3], float32 p[2]) {
			const float32 ax = d[0] < 0.f ? -d[0] : d[0], ay = d[1] < 0.f ? -d[1] : d[1], az = d[2] < 0.f ? -d[2] : d[2];
			const float32 s = ax + ay + az > 1e-20f ? ax + ay + az : 1.f;
			float32 px = d[0] / s, pz = d[2] / s;
			if (d[1] < 0.f) {
				const float32 sx = px >= 0.f ? 1.f : -1.f, sz = pz >= 0.f ? 1.f : -1.f;
				const float32 apx = px < 0.f ? -px : px, apz = pz < 0.f ? -pz : pz;
				px = (1.f - apz) * sx;
				pz = (1.f - apx) * sz;
			}
			p[0] = px;
			p[1] = pz;
		}
		void NkSdOctDecoder(const float32 p[2], float32 d[3]) {
			const float32 apx = p[0] < 0.f ? -p[0] : p[0], apz = p[1] < 0.f ? -p[1] : p[1];
			float32 x = p[0], y = 1.f - apx - apz, z = p[1];
			if (y < 0.f) {
				const float32 sx = x >= 0.f ? 1.f : -1.f, sz = z >= 0.f ? 1.f : -1.f;
				x = (1.f - apz) * sx;
				z = (1.f - apx) * sz;
			}
			const float32 l = math::NkSqrt(x * x + y * y + z * z);
			d[0] = x / l;
			d[1] = y / l;
			d[2] = z / l;
		}

		// ── les demi-flottants ─────────────────────────────────────────────────────────
		uint16 NkSdVersDemi(float32 f) {
			union {
					float32 f;
					uint32 u;
			} c;
			c.f = f;
			const uint32 u = c.u;
			const uint32 signe = (u >> 16) & 0x8000u;
			const uint32 ef = (u >> 23) & 0xFFu;
			uint32 m = u & 0x7FFFFFu;
			if (ef == 0xFFu) // infini, ou NaN
				return (uint16)(signe | 0x7C00u | (m ? 0x200u : 0u));
			const int32 e = (int32)ef - 127 + 15;
			if (e >= 31)
				return (uint16)(signe | 0x7BFFu); // borne a 65 504
			if (e <= 0) {						  // sous-normal
				if (e < -10)
					return (uint16)signe;
				m |= 0x800000u;
				const uint32 dec = (uint32)(14 - e);
				uint32 hm = m >> dec;
				const uint32 reste = m & ((1u << dec) - 1u), moitie = 1u << (dec - 1u);
				if (reste > moitie || (reste == moitie && (hm & 1u)))
					++hm;
				return (uint16)(signe | hm);
			}
			uint32 h = signe | ((uint32)e << 10) | (m >> 13);
			const uint32 reste = m & 0x1FFFu;
			if (reste > 0x1000u || (reste == 0x1000u && (h & 1u)))
				++h; // la retenue passe dans l'exposant : c'est voulu
			if ((h & 0x7FFFu) >= 0x7C00u)
				h = signe | 0x7BFFu;
			return (uint16)h;
		}
		float32 NkSdDepuisDemi(uint16 h) {
			const uint32 s = (uint32)(h & 0x8000u) << 16;
			const uint32 e = ((uint32)h >> 10) & 0x1Fu, m = (uint32)h & 0x3FFu;
			if (e == 0) {
				const float32 v = (float32)m * (1.f / 16777216.f);
				return s ? -v : v;
			}
			union {
					float32 f;
					uint32 u;
			} c;
			c.u = e == 31 ? (s | 0x7F800000u | (m << 13)) : (s | ((e - 15u + 127u) << 23) | (m << 13));
			return c.f;
		}

		// ── la grille ──────────────────────────────────────────────────────────────────
		bool NkSdPlacer(const float32 mn[3], const float32 mx[3], float32 pasMin, int32 budget, NkSdGrille &g) {
			if (pasMin <= 0.f || budget < 8)
				return false;
			float32 e[3];
			for (int32 a = 0; a < 3; ++a) {
				e[a] = mx[a] - mn[a];
				if (!(e[a] >= 0.f))
					return false;
			}
			float32 pas = pasMin;
			for (int32 essai = 0; essai < 200; ++essai) {
				int32 n[3];
				for (int32 a = 0; a < 3; ++a) {
					n[a] = (int32)(e[a] / pas) + 2; // une sonde de plus : la grille couvre la boite
					if (n[a] < 2)
						n[a] = 2;
				}
				const int64 total = (int64)n[0] * (int64)n[1] * (int64)n[2];
				if (total <= (int64)budget && n[0] * kSdTuile <= kSdLimiteTexture && n[2] * kSdTuile <= kSdLimiteTexture &&
					n[1] * 2 <= kSdLimiteTexture) {
					g.pas = pas;
					g.nx = n[0];
					g.ny = n[1];
					g.nz = n[2];
					// centree sur la boite
					for (int32 a = 0; a < 3; ++a)
						g.origine[a] = 0.5f * (mn[a] + mx[a]) - 0.5f * pas * (float32)(n[a] - 1);
					return true;
				}
				pas *= 1.08f;
			}
			return false;
		}

		void NkSdTexel(const NkSdVolume &v, int32 ix, int32 iy, int32 iz, int32 tranche, int32 tx, int32 ty, float32 out[4]) {
			const usize i = Index(v, ix * kSdTuile + tx, iz * kSdTuile + ty, iy * 2 + tranche);
			for (int32 q = 0; q < 4; ++q)
				out[q] = NkSdDepuisDemi(v.texels[i + (usize)q]);
		}

		// ── la cuisson ─────────────────────────────────────────────────────────────────
		bool NkSdCuire(const NkSdGrille &g, const NkSdReglages &r, NkSdVolume &v, NkSdStats *st) {
			const NkElapsedTime t0 = NkChrono::Now();
			if (g.nx < 2 || g.ny < 2 || g.nz < 2 || r.rayons < 16)
				return false;
			v.grille = g;
			v.W = g.nx * kSdTuile;
			v.H = g.nz * kSdTuile;
			v.D = g.ny * 2;
			v.texels.Resize((usize)v.W * (usize)v.H * (usize)v.D * 4u);
			// les directions des texels (centres), une fois
			const int32 T = kSdTexels;
			float32 dirTexel[kSdTexels * kSdTexels * 3];
			for (int32 ty = 0; ty < T; ++ty)
				for (int32 tx = 0; tx < T; ++tx) {
					const float32 p[2] = {((float32)tx + 0.5f) / (float32)T * 2.f - 1.f, ((float32)ty + 0.5f) / (float32)T * 2.f - 1.f};
					NkSdOctDecoder(p, dirTexel + (ty * T + tx) * 3);
				}
			const float32 dmax = 2.f * g.pas * 1.7320508f;
			const int32 n = r.rayons;
			// CONTRE-EPREUVE du banc : NK_SONDES_MUTATION=1 oublie le cosinus de l'irradiance (le four
			// blanc rend alors 2 pi au lieu de pi : le banc doit rougir)
			const char *mu = env::GetEnvVar("NK_SONDES_MUTATION");
			const bool mutation = mu && mu[0] == '1';
			const int32 total = g.nx * g.ny * g.nz;
			NkVector<uint64> rayonsSonde;
			NkVector<uint8> eteinte;
			rayonsSonde.Resize((usize)total);
			eteinte.Resize((usize)total);
			// les tampons de travail : alloues ICI, une fois, par lots de sondes (aucune allocation
			// dans les fils)
			const int32 lot = 256;
			NkVector<float32> tDirs, tRad, tDist;
			NkVector<uint8> tDos, tEchec;
			tDirs.Resize((usize)lot * (usize)n * 3u);
			tRad.Resize((usize)lot * (usize)n * 3u);
			tDist.Resize((usize)lot * (usize)n);
			tDos.Resize((usize)lot * (usize)n);
			tEchec.Resize((usize)lot);
			int32 debut = 0;
			auto sonde = [&](nk_size jj) {
				const int32 j = (int32)jj, k = debut + j;
				if (k >= total)
					return;
				const int32 ix = k % g.nx, iy = (k / g.nx) % g.ny, iz = k / (g.nx * g.ny);
				const float32 o[3] = {g.origine[0] + g.pas * (float32)ix, g.origine[1] + g.pas * (float32)iy,
									  g.origine[2] + g.pas * (float32)iz};
				float32 *dirs = tDirs.Data() + (usize)j * (usize)n * 3u;
				float32 *rad = tRad.Data() + (usize)j * (usize)n * 3u;
				float32 *dist = tDist.Data() + (usize)j * (usize)n;
				uint8 *dos = tDos.Data() + (usize)j * (usize)n;
				Directions(n, (uint64)k * 0x9E3779B97F4A7C15ULL + 7u, dirs);
				uint64 ry = 0;
				if (!NkTcRayonsSonde(o, dirs, n, Melanger((uint64)k + 0xA5A5ULL), dmax, r.traceur, rad, dist, dos, &ry)) {
					tEchec[(usize)j] = 1u;
					return;
				}
				rayonsSonde[(usize)k] = ry;
				int32 nDos = 0;
				for (int32 i = 0; i < n; ++i)
					nDos += dos[(usize)i] ? 1 : 0;
				const bool active = (float32)nDos < r.seuilDos * (float32)n;
				eteinte[(usize)k] = active ? 0u : 1u;
				const float32 kE = 4.f * kPiSd / (float32)n;
				for (int32 t = 0; t < T * T; ++t) {
					const float32 *nt = dirTexel + t * 3;
					float32 E[3] = {0.f, 0.f, 0.f}, sw = 0.f, sd = 0.f, sd2 = 0.f;
					for (int32 i = 0; i < n; ++i) {
						const float32 *w = dirs + i * 3;
						const float32 c = nt[0] * w[0] + nt[1] * w[1] + nt[2] * w[2];
						if (c <= 0.f)
							continue;
						const float32 ce = mutation ? 1.f : c;
						E[0] += rad[(usize)i * 3] * ce;
						E[1] += rad[(usize)i * 3 + 1] * ce;
						E[2] += rad[(usize)i * 3 + 2] * ce;
						const float32 lw = Lobe32(c);
						if (lw > 0.f) {
							const float32 d = dist[(usize)i];
							sw += lw;
							sd += lw * d;
							sd2 += lw * d * d;
						}
					}
					const float32 mu = sw > 0.f ? sd / sw : dmax, mu2 = sw > 0.f ? sd2 / sw : dmax * dmax;
					const int32 tx = t % T, ty = t / T;
					const usize ii = Index(v, ix * kSdTuile + 1 + tx, iz * kSdTuile + 1 + ty, iy * 2);
					v.texels[ii] = NkSdVersDemi(E[0] * kE);
					v.texels[ii + 1] = NkSdVersDemi(E[1] * kE);
					v.texels[ii + 2] = NkSdVersDemi(E[2] * kE);
					v.texels[ii + 3] = NkSdVersDemi(active ? 1.f : 0.f);
					const usize iv = Index(v, ix * kSdTuile + 1 + tx, iz * kSdTuile + 1 + ty, iy * 2 + 1);
					v.texels[iv] = NkSdVersDemi(mu);
					v.texels[iv + 1] = NkSdVersDemi(mu2);
					v.texels[iv + 2] = 0;
					v.texels[iv + 3] = NkSdVersDemi(1.f);
				}
				Bords(v, ix * kSdTuile, iz * kSdTuile, iy * 2);
				Bords(v, ix * kSdTuile, iz * kSdTuile, iy * 2 + 1);
			};
			threading::NkThreadPool &pool = threading::NkThreadPool::GetGlobal();
			for (debut = 0; debut < total; debut += lot) {
				pool.ParallelFor((nk_size)lot, sonde, (nk_size)4);
				pool.Join();
				for (int32 j = 0; j < lot; ++j)
					if (tEchec[(usize)j])
						return false;
			}
			uint64 ry = 0;
			int32 nEteintes = 0;
			for (int32 k = 0; k < total; ++k) {
				ry += rayonsSonde[(usize)k];
				nEteintes += eteinte[(usize)k] ? 1 : 0;
			}
			const float64 ms = (NkChrono::Now() - t0).ToMilliseconds();
			if (st) {
				st->ms = ms;
				st->rayons = ry;
				st->sondes = total;
				st->eteintes = nEteintes;
			}
			NkPrintfSortie("[sondes] CUISSON : %dx%dx%d sondes (pas %.2f m, origine %.2f %.2f %.2f), %d rayons/sonde, %d eteinte(s) ; "
						   "%.0f ms, %.1f Mrayons ; volume %dx%dx%d RGBA16F (%.1f Mo)\n",
						   (int)g.nx, (int)g.ny, (int)g.nz, (double)g.pas, (double)g.origine[0], (double)g.origine[1],
						   (double)g.origine[2], (int)n, (int)nEteintes, ms, (double)ry * 1e-6, (int)v.W, (int)v.H, (int)v.D,
						   (double)v.texels.Size() * 2.0 / (1024.0 * 1024.0));
			return true;
		}

		// ── le banc ────────────────────────────────────────────────────────────────────
		int32 NkSdBanc() {
			int32 echecs = 0;
			auto verdict = [&](bool ok, const char *quoi, float64 mesure, float64 attendu) {
				NkPrintfSortie("[sondes-banc] %-44s mesure %.5f attendu %.5f : %s\n", quoi, mesure, attendu, ok ? "VERT" : "ROUGE");
				if (!ok)
					++echecs;
			};
			// 1. l'octaedre : aller-retour sur des directions au hasard, et centres de texels
			{
				Hasard h{12345u};
				float32 pire = 0.f;
				for (int32 i = 0; i < 2000; ++i) {
					const float32 z = 1.f - 2.f * h.F(), ph = 2.f * kPiSd * h.F(), r = math::NkSqrt(1.f - z * z);
					const float32 d[3] = {r * math::NkCos(ph), z, r * math::NkSin(ph)};
					float32 p[2], d2[3];
					NkSdOctEncoder(d, p);
					NkSdOctDecoder(p, d2);
					const float32 e = (d[0] - d2[0]) * (d[0] - d2[0]) + (d[1] - d2[1]) * (d[1] - d2[1]) + (d[2] - d2[2]) * (d[2] - d2[2]);
					if (e > pire)
						pire = e;
				}
				verdict(pire < 1e-10f, "octaedre : aller-retour (ecart^2 max)", (float64)pire, 0.0);
				// les bords : (1, v) et (1, -v) designent la meme direction
				const float32 a[2] = {1.f, 0.4f}, b[2] = {1.f, -0.4f};
				float32 da[3], db[3];
				NkSdOctDecoder(a, da);
				NkSdOctDecoder(b, db);
				const float32 e = (da[0] - db[0]) * (da[0] - db[0]) + (da[1] - db[1]) * (da[1] - db[1]) + (da[2] - db[2]) * (da[2] - db[2]);
				verdict(e < 1e-10f, "octaedre : arete repliee (miroir)", (float64)e, 0.0);
			}
			// 2. les demi-flottants
			{
				const float32 vals[7] = {0.f, 1.f, -2.5f, 3.14159265f, 65504.f, 1e-5f, 0.333333f};
				float32 pire = 0.f;
				for (int32 i = 0; i < 7; ++i) {
					const float32 r = NkSdDepuisDemi(NkSdVersDemi(vals[i]));
					const float32 ref = vals[i] < 0.f ? -vals[i] : vals[i];
					const float32 e = (r - vals[i] < 0.f ? vals[i] - r : r - vals[i]) / (ref > 1e-3f ? ref : 1e-3f);
					if (e > pire)
						pire = e;
				}
				verdict(pire < 1e-3f, "demi-flottants : erreur relative max", (float64)pire, 0.0);
				verdict(NkSdDepuisDemi(NkSdVersDemi(1e9f)) == 65504.f, "demi-flottants : borne a 65504",
						(float64)NkSdDepuisDemi(NkSdVersDemi(1e9f)), 65504.0);
			}
			// une scene : un ciel uniforme de radiance 1, sans soleil ni source, et un mur facultatif
			auto preparer = [&](bool mur) -> bool {
				NkScArchviz s;
				auto sommet = [&](float32 x, float32 y, float32 z, float32 nx, float32 ny, float32 nz) {
					s.P.PushBack(x);
					s.P.PushBack(y);
					s.P.PushBack(z);
					s.N.PushBack(nx);
					s.N.PushBack(ny);
					s.N.PushBack(nz);
					s.UV.PushBack(0.f);
					s.UV.PushBack(0.f);
				};
				NkScMateriau m;
				m.couleur[0] = m.couleur[1] = m.couleur[2] = 0.5f;
				m.rugosite = 1.f;
				s.mats.PushBack(m);
				if (mur) {
					// le plan x = 0,3, normale +x (la sonde de l'origine en voit l'ARRIERE)
					sommet(0.3f, -50.f, -50.f, 1.f, 0.f, 0.f);
					sommet(0.3f, 50.f, -50.f, 1.f, 0.f, 0.f);
					sommet(0.3f, 50.f, 50.f, 1.f, 0.f, 0.f);
					sommet(0.3f, -50.f, 50.f, 1.f, 0.f, 0.f);
				} else {
					// un grain de geometrie tres loin (le traceur veut au moins un triangle)
					sommet(5000.f, 5000.f, 5000.f, 0.f, 1.f, 0.f);
					sommet(5000.01f, 5000.f, 5000.f, 0.f, 1.f, 0.f);
					sommet(5000.f, 5000.f, 5000.01f, 0.f, 1.f, 0.f);
					sommet(5000.01f, 5000.f, 5000.01f, 0.f, 1.f, 0.f);
				}
				const uint32 q[6] = {0, 1, 2, 0, 2, 3};
				for (int32 i = 0; i < 6; ++i)
					s.tri.PushBack(q[i]);
				s.triMat.PushBack(0);
				s.triMat.PushBack(0);
				s.soleilE = 0.f;
				for (int32 a = 0; a < 3; ++a) {
					s.cielZenith[a] = 1.f;
					s.cielHorizon[a] = 1.f;
					s.mn[a] = -50.f;
					s.mx[a] = 50.f;
				}
				s.cielIntensite = 1.f;
				NkTcStats st;
				return NkTcPreparer(s, nullptr, &st);
			};
			NkSdReglages r;
			r.rayons = 1024;
			// 3. le four blanc : E(n) = pi partout ; la visibilite vaut la borne (aucun impact)
			if (preparer(false)) {
				NkSdGrille g;
				g.pas = 1.f;
				NkSdVolume v;
				NkSdStats st;
				if (NkSdCuire(g, r, v, &st)) {
					float32 pire = 0.f, pireVis = 0.f, actives = 1.f;
					const float32 dmax = 2.f * 1.7320508f;
					for (int32 k = 0; k < 8; ++k)
						for (int32 t = 0; t < kSdTexels * kSdTexels; ++t) {
							float32 a[4], b[4];
							NkSdTexel(v, k & 1, (k >> 1) & 1, (k >> 2) & 1, 0, 1 + t % kSdTexels, 1 + t / kSdTexels, a);
							NkSdTexel(v, k & 1, (k >> 1) & 1, (k >> 2) & 1, 1, 1 + t % kSdTexels, 1 + t / kSdTexels, b);
							for (int32 c = 0; c < 3; ++c) {
								const float32 e = a[c] / kPiSd - 1.f;
								const float32 ae = e < 0.f ? -e : e;
								if (ae > pire)
									pire = ae;
							}
							const float32 ev = b[0] / dmax - 1.f;
							const float32 aev = ev < 0.f ? -ev : ev;
							if (aev > pireVis)
								pireVis = aev;
							if (a[3] < 0.5f)
								actives = 0.f;
						}
					verdict(pire < 0.03f, "four blanc : |E/pi - 1| max (8 sondes x 64)", (float64)pire, 0.0);
					verdict(pireVis < 0.01f, "four blanc : visibilite = borne (ecart relatif)", (float64)pireVis, 0.0);
					verdict(actives > 0.5f, "four blanc : toutes les sondes actives", (float64)actives, 1.0);
				} else
					verdict(false, "four blanc : cuisson", 0.0, 1.0);
			} else
				verdict(false, "four blanc : preparation", 0.0, 1.0);
			// 4. le mur x = 0,3 (albedo 0,5) : la sonde (0,0,0) en voit l'arriere (eteinte), la sonde
			// (1,0,0) l'avant : vers -x, un mur de radiance 0,5 (E = 0,5 pi) a 0,7 m ; vers +x le ciel
			if (preparer(true)) {
				NkSdGrille g;
				g.pas = 1.f;
				NkSdVolume v;
				NkSdStats st;
				if (NkSdCuire(g, r, v, &st)) {
					// le texel dont la direction est la plus proche de -x, et de +x
					int32 tm = 0, tp = 0;
					float32 bm = -2.f, bp = -2.f;
					for (int32 t = 0; t < kSdTexels * kSdTexels; ++t) {
						const float32 p[2] = {((float32)(t % kSdTexels) + 0.5f) / (float32)kSdTexels * 2.f - 1.f,
											  ((float32)(t / kSdTexels) + 0.5f) / (float32)kSdTexels * 2.f - 1.f};
						float32 d[3];
						NkSdOctDecoder(p, d);
						if (-d[0] > bm) {
							bm = -d[0];
							tm = t;
						}
						if (d[0] > bp) {
							bp = d[0];
							tp = t;
						}
					}
					float32 a0[4], am[4], ap[4], vm[4];
					NkSdTexel(v, 0, 0, 0, 0, 1, 1, a0);
					NkSdTexel(v, 1, 0, 0, 0, 1 + tm % kSdTexels, 1 + tm / kSdTexels, am);
					NkSdTexel(v, 1, 0, 0, 0, 1 + tp % kSdTexels, 1 + tp / kSdTexels, ap);
					NkSdTexel(v, 1, 0, 0, 1, 1 + tm % kSdTexels, 1 + tm / kSdTexels, vm);
					verdict(a0[3] < 0.5f, "mur : la sonde de dos est eteinte", (float64)a0[3], 0.0);
					verdict(am[3] > 0.5f, "mur : la sonde de face est active", (float64)am[3], 1.0);
					const float32 em = am[1] / kPiSd;
					verdict(em > 0.45f && em < 0.56f, "mur : E(-x)/pi (mur de radiance 0,5)", (float64)em, 0.5);
					const float32 ep = ap[1] / kPiSd;
					verdict(ep > 0.90f && ep < 1.10f, "mur : E(+x)/pi (le ciel, un peu de mur)", (float64)ep, 1.0);
					verdict(vm[0] > 0.66f && vm[0] < 0.80f, "mur : distance moyenne vers -x (0,7 m)", (float64)vm[0], 0.7);
				} else
					verdict(false, "mur : cuisson", 0.0, 1.0);
			} else
				verdict(false, "mur : preparation", 0.0, 1.0);
			NkPrintfSortie("[sondes-banc] %s : %d echec(s)\n", echecs ? "ROUGE" : "VERT", (int)echecs);
			return echecs;
		}

	} // namespace renderer
} // namespace nkentseu
