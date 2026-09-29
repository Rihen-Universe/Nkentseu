// AUTEUR : Rihen
// =============================================================================
// NkParticules2DFabrique.cpp
// =============================================================================
#include "NKPhysics/NkParticules2DFabrique.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {
	namespace physics {

		namespace {
			constexpr float32 kPi = 3.14159265358979f;

			float32 Len2(const NkVec2f &v) noexcept {
				return v.x * v.x + v.y * v.y;
			}

			/// Remplit un disque de rayon R en reseau hexagonal d'espacement s.
			template <typename F> void DisqueHex(const NkVec2f &centre, float32 R, float32 s, F &&ajouter) {
				const float32 dy = s * 0.8660254f;
				const int32 n = static_cast<int32>(R / dy) + 1;
				const int32 m = static_cast<int32>(R / s) + 1;
				for (int32 r = -n; r <= n; ++r) {
					const float32 y = static_cast<float32>(r) * dy;
					const float32 decal = (r & 1) ? s * 0.5f : 0.f;
					for (int32 c = -m; c <= m; ++c) {
						const float32 x = static_cast<float32>(c) * s + decal;
						if (x * x + y * y <= R * R) {
							ajouter(centre + NkVec2f(x, y));
						}
					}
				}
			}

			/// Relie toutes les paires du corps plus proches que `seuil`.
			void RelierVoisins(NkParticules2D &p, uint32 ci, float32 seuil, bool reposExact, float32 repos) noexcept {
				const NkCorpsP2D &c = p.corps[ci];
				const float32 s2 = seuil * seuil;
				for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
					for (uint32 j = i + 1; j < c.debut + c.nombre; ++j) {
						const float32 d2 = Len2(p.particules[j].pos - p.particules[i].pos);
						if (d2 < s2) {
							p.Relier(i, j, reposExact ? repos : math::NkSqrt(d2));
						}
					}
				}
			}

			/// Grille nx * ny, rangee par lignes. Les liens sont crees dans l'ordre
			/// que le RENDU relit : horizontaux, verticaux, puis le reste.
			uint32 Grille(NkParticules2D &p, uint32 ci, const NkVec2f &origine, int32 nx, int32 ny, float32 s,
						  float32 rayon, bool descendante, bool flexion) noexcept {
				NkCorpsP2D &c = p.corps[ci];
				c.nx = nx;
				c.ny = ny;
				c.espacement = s;
				const uint32 base = static_cast<uint32>(p.particules.Size());
				for (int32 j = 0; j < ny; ++j) {
					for (int32 i = 0; i < nx; ++i) {
						const float32 y = descendante ? -static_cast<float32>(j) * s : static_cast<float32>(j) * s;
						p.AjouterParticule(ci, origine + NkVec2f(static_cast<float32>(i) * s, y), rayon);
					}
				}
				auto id = [&](int32 i, int32 j) { return base + static_cast<uint32>(j * nx + i); };
				for (int32 j = 0; j < ny; ++j) {
					for (int32 i = 0; i + 1 < nx; ++i) {
						p.AjouterLien(id(i, j), id(i + 1, j), NkGenreLien2D::NK_STRUCTURE);
					}
				}
				for (int32 j = 0; j + 1 < ny; ++j) {
					for (int32 i = 0; i < nx; ++i) {
						p.AjouterLien(id(i, j), id(i, j + 1), NkGenreLien2D::NK_STRUCTURE);
					}
				}
				for (int32 j = 0; j + 1 < ny; ++j) {
					for (int32 i = 0; i + 1 < nx; ++i) {
						p.AjouterLien(id(i, j), id(i + 1, j + 1), NkGenreLien2D::NK_CISAILLEMENT);
						p.AjouterLien(id(i + 1, j), id(i, j + 1), NkGenreLien2D::NK_CISAILLEMENT);
					}
				}
				if (flexion) {
					for (int32 j = 0; j < ny; ++j) {
						for (int32 i = 0; i + 2 < nx; ++i) {
							p.AjouterLien(id(i, j), id(i + 2, j), NkGenreLien2D::NK_FLEXION);
						}
					}
					for (int32 j = 0; j + 2 < ny; ++j) {
						for (int32 i = 0; i < nx; ++i) {
							p.AjouterLien(id(i, j), id(i, j + 2), NkGenreLien2D::NK_FLEXION);
						}
					}
				}
				return base;
			}

			uint32 Ouvrir(NkParticules2D &p, NkPresetP2D preset) noexcept {
				NkCorpsP2D modele;
				NkAppliquerPresetP2D(modele, preset);
				const uint32 ci = p.NouveauCorps(modele.mat);
				const uint32 id = p.corps[ci].id;
				const uint32 debut = p.corps[ci].debut;
				p.corps[ci] = modele;
				p.corps[ci].id = id;
				p.corps[ci].debut = debut;
				p.corps[ci].nombre = 0;
				return ci;
			}
		} // namespace

		void NkAppliquerPresetP2D(NkCorpsP2D &c, NkPresetP2D preset) noexcept {
			c = NkCorpsP2D{};
			switch (preset) {
				case NkPresetP2D::NK_BALLON:
					c.mat = NkMateriauP2D::NK_BALLON;
					c.raideur = 0.95f;
					c.pression = 1.f;
					c.resistance = 2.4f;
					c.friction = 0.55f;
					c.rebond = 0.45f;
					c.autoCollision = false;
					break;
				case NkPresetP2D::NK_BLOB:
				case NkPresetP2D::NK_SLIME: {
					// Mesure (blob de 0,42 m au sol) : garde sa forme a l'impact,
					// tremble, puis s'affaisse en dome en ~5 s.
					const bool geant = preset == NkPresetP2D::NK_SLIME;
					c.mat = NkMateriauP2D::NK_BLOB;
					c.espacement = 0.10f;
					c.raideur = geant ? 0.30f : 0.40f;
					c.viscosite = geant ? 0.12f : 0.10f;
					c.cohesion = geant ? 0.45f : 0.60f;
					c.plasticite = geant ? 12.f : 7.f;
					c.resistance = 1.9f;
					c.friction = 0.25f;
					c.rebond = 0.05f;
					break;
				}
				case NkPresetP2D::NK_GELEE:
					c.mat = NkMateriauP2D::NK_GELEE;
					c.raideur = 0.55f;
					c.formeRaideur = 0.035f;
					c.friction = 0.6f;
					c.rebond = 0.25f;
					c.autoCollision = false;
					break;
				case NkPresetP2D::NK_EAU:
					c.mat = NkMateriauP2D::NK_EAU;
					c.espacement = 0.10f;
					c.viscosite = 0.03f;
					c.cohesion = 0.30f; // sous 0,2 une flaque s'etale en film d'une particule
					c.friction = 0.02f;
					c.rebond = 0.f;
					break;
				case NkPresetP2D::NK_MIEL:
					c.mat = NkMateriauP2D::NK_MIEL;
					c.espacement = 0.10f;
					c.viscosite = 0.42f;
					c.cohesion = 0.55f;
					c.friction = 0.35f;
					c.rebond = 0.f;
					break;
				case NkPresetP2D::NK_SABLE:
					c.mat = NkMateriauP2D::NK_SABLE;
					c.espacement = 0.095f;
					c.friction = 0.9f;
					c.rebond = 0.f;
					break;
				case NkPresetP2D::NK_CRISTAL:
					c.mat = NkMateriauP2D::NK_ATOMES;
					c.espacement = 0.14f;
					c.raideur = 0.93f;
					c.resistance = 1.30f;
					c.friction = 0.5f;
					c.rebond = 0.1f;
					break;
				case NkPresetP2D::NK_TISSU:
					c.mat = NkMateriauP2D::NK_TISSU;
					c.raideur = 0.97f;
					c.resistance = 1.6f;
					c.friction = 0.4f;
					c.rebond = 0.f;
					c.autoCollision = false;
					c.masseParticule = 0.5f;
					break;
				case NkPresetP2D::NK_CORDE:
					c.mat = NkMateriauP2D::NK_CORDE;
					c.raideur = 1.f;
					c.friction = 0.3f;
					c.rebond = 0.1f;
					c.autoCollision = false;
					break;
				case NkPresetP2D::NK_PONT:
					c.mat = NkMateriauP2D::NK_CORDE;
					c.raideur = 1.f;
					c.friction = 0.7f;
					c.rebond = 0.05f;
					c.autoCollision = false;
					c.masseParticule = 1.5f;
					break;
				default:
					break;
			}
		}

		int32 NkCreerBallonP2D(NkParticules2D &p, const NkVec2f &centre, float32 R) noexcept {
			const uint32 ci = Ouvrir(p, NkPresetP2D::NK_BALLON);
			// Arete ~ 2 rayons de particule : rien ne passe entre deux points.
			const int32 n = static_cast<int32>(2.f * kPi * R / 0.10f) < 12 ? 12 : static_cast<int32>(2.f * kPi * R / 0.10f);
			p.corps[ci].nx = n;
			const uint32 base = static_cast<uint32>(p.particules.Size());
			for (int32 k = 0; k < n; ++k) {
				const float32 t = 2.f * kPi * static_cast<float32>(k) / static_cast<float32>(n);
				p.AjouterParticule(ci, centre + NkVec2f(math::NkCos(t), math::NkSin(t)) * R, 0.052f);
			}
			for (int32 k = 0; k < n; ++k) {
				p.AjouterLien(base + static_cast<uint32>(k), base + static_cast<uint32>((k + 1) % n), NkGenreLien2D::NK_STRUCTURE);
			}
			p.FinaliserCorps(ci);
			return static_cast<int32>(ci);
		}

		int32 NkCreerBlobP2D(NkParticules2D &p, const NkVec2f &centre, float32 R, NkPresetP2D preset) noexcept {
			const uint32 ci = Ouvrir(p, preset == NkPresetP2D::NK_SLIME ? NkPresetP2D::NK_SLIME : NkPresetP2D::NK_BLOB);
			const float32 s = p.corps[ci].espacement;
			DisqueHex(centre, R, s, [&](const NkVec2f &q) { p.AjouterParticule(ci, q, s * 0.5f); });
			RelierVoisins(p, ci, s * 1.3f, false, 0.f);
			p.FinaliserCorps(ci);
			return static_cast<int32>(ci);
		}

		int32 NkCreerGeleeP2D(NkParticules2D &p, const NkVec2f &centre, int32 nx, int32 ny, float32 s) noexcept {
			const uint32 ci = Ouvrir(p, NkPresetP2D::NK_GELEE);
			const NkVec2f origine = centre - NkVec2f(static_cast<float32>(nx - 1) * s * 0.5f, static_cast<float32>(ny - 1) * s * 0.5f);
			Grille(p, ci, origine, nx, ny, s, s * 0.5f, false, true);
			p.FinaliserCorps(ci);
			return static_cast<int32>(ci);
		}

		int32 NkCreerFluideP2D(NkParticules2D &p, const NkVec2f &centre, float32 R, NkPresetP2D preset) noexcept {
			const int32 ci = NkOuvrirPinceauP2D(p, preset);
			if (ci < 0) {
				return -1;
			}
			const float32 s = p.corps[static_cast<uint32>(ci)].espacement;
			const float32 rayon = preset == NkPresetP2D::NK_SABLE ? 0.046f : s * 0.5f;
			DisqueHex(centre, R, s, [&](const NkVec2f &q) { p.AjouterParticule(static_cast<uint32>(ci), q, rayon); });
			p.FinaliserCorps(static_cast<uint32>(ci));
			return ci;
		}

		int32 NkOuvrirPinceauP2D(NkParticules2D &p, NkPresetP2D preset) noexcept {
			if (preset != NkPresetP2D::NK_EAU && preset != NkPresetP2D::NK_MIEL && preset != NkPresetP2D::NK_SABLE) {
				return -1;
			}
			return static_cast<int32>(Ouvrir(p, preset));
		}

		uint32 NkVerserP2D(NkParticules2D &p, uint32 ci, const NkVec2f &pos, float32 rayon, int32 n, uint32 &graine) noexcept {
			if (ci >= p.corps.Size() || ci + 1u != p.corps.Size()) {
				return 0;
			}
			NkCorpsP2D &c = p.corps[ci];
			if (c.debut + c.nombre != p.particules.Size()) {
				return 0;
			}
			auto alea = [&]() {
				uint32 x = graine == 0 ? 0x9E3779B9u : graine;
				x ^= x << 13;
				x ^= x >> 17;
				x ^= x << 5;
				graine = x;
				return static_cast<float32>(x >> 8) * (1.f / 16777216.f);
			};
			const float32 s = c.espacement;
			const float32 r = c.mat == NkMateriauP2D::NK_SABLE ? 0.046f : s * 0.5f;
			const float32 min2 = (s * 0.85f) * (s * 0.85f);
			uint32 ajoutes = 0;
			for (int32 k = 0; k < n * 3 && static_cast<int32>(ajoutes) < n; ++k) {
				const float32 ang = alea() * 2.f * kPi;
				const float32 d = math::NkSqrt(alea()) * rayon;
				const NkVec2f q = pos + NkVec2f(math::NkCos(ang), math::NkSin(ang)) * d;
				const NkLimites2D &L = p.reglages.limites;
				if (L.actif && (q.y < L.bas + r || q.y > L.haut - r || q.x < L.gauche + r || q.x > L.droite - r)) {
					continue;
				}
				// Pas de naissance DANS une autre particule : la pression
				// l'ejecterait a une vitesse absurde.
				bool libre = true;
				for (uint32 i = 0; i < p.particules.Size() && libre; ++i) {
					libre = Len2(p.particules[i].pos - q) >= min2;
				}
				if (!libre) {
					continue;
				}
				const uint32 id = p.AjouterParticule(ci, q, r);
				p.particules[id].vit = NkVec2f(0.f, -1.5f);
				++ajoutes;
			}
			return ajoutes;
		}

		int32 NkCreerCristalP2D(NkParticules2D &p, const NkVec2f &centre, bool disque) noexcept {
			const uint32 ci = Ouvrir(p, NkPresetP2D::NK_CRISTAL);
			const float32 s = p.corps[ci].espacement;
			if (!disque) {
				const int32 nx = 13;
				const int32 ny = 8;
				const float32 dy = s * 0.8660254f;
				const NkVec2f o = centre - NkVec2f(static_cast<float32>(nx - 1) * s * 0.5f, static_cast<float32>(ny - 1) * dy * 0.5f);
				for (int32 j = 0; j < ny; ++j) {
					const int32 n = (j & 1) ? nx - 1 : nx;
					const float32 decal = (j & 1) ? s * 0.5f : 0.f;
					for (int32 i = 0; i < n; ++i) {
						p.AjouterParticule(ci, o + NkVec2f(static_cast<float32>(i) * s + decal, static_cast<float32>(j) * dy), s * 0.45f);
					}
				}
			} else {
				DisqueHex(centre, 0.62f, s, [&](const NkVec2f &q) { p.AjouterParticule(ci, q, s * 0.45f); });
			}
			RelierVoisins(p, ci, s * 1.1f, true, s);
			p.FinaliserCorps(ci);
			return static_cast<int32>(ci);
		}

		int32 NkCreerTissuP2D(NkParticules2D &p, const NkVec2f &hautCentre, int32 nx, int32 ny, float32 s) noexcept {
			const uint32 ci = Ouvrir(p, NkPresetP2D::NK_TISSU);
			const uint32 base = Grille(p, ci, hautCentre - NkVec2f(static_cast<float32>(nx - 1) * s * 0.5f, 0.f), nx, ny, s, 0.04f,
									   true, true);
			for (int32 i = 0; i < nx; i += 5) {
				p.BasculerEpingle(base + static_cast<uint32>(i));
			}
			if (((nx - 1) % 5) != 0) {
				p.BasculerEpingle(base + static_cast<uint32>(nx - 1));
			}
			p.FinaliserCorps(ci);
			return static_cast<int32>(ci);
		}

		int32 NkCreerCordeP2D(NkParticules2D &p, const NkVec2f &haut, int32 n, float32 s) noexcept {
			const uint32 ci = Ouvrir(p, NkPresetP2D::NK_CORDE);
			p.corps[ci].espacement = s;
			p.corps[ci].nx = n;
			const uint32 base = static_cast<uint32>(p.particules.Size());
			for (int32 i = 0; i < n; ++i) {
				const bool lest = i == n - 1;
				const uint32 k = p.AjouterParticule(ci, haut - NkVec2f(0.f, static_cast<float32>(i) * s), lest ? 0.12f : 0.045f);
				if (lest) {
					p.particules[k].masse = 8.f;
					p.particules[k].invMasse = 1.f / 8.f;
				}
			}
			for (int32 i = 0; i + 1 < n; ++i) {
				p.AjouterLien(base + static_cast<uint32>(i), base + static_cast<uint32>(i + 1), NkGenreLien2D::NK_STRUCTURE);
			}
			p.BasculerEpingle(base);
			p.FinaliserCorps(ci);
			return static_cast<int32>(ci);
		}

		int32 NkCreerPontP2D(NkParticules2D &p, const NkVec2f &a, const NkVec2f &b) noexcept {
			const uint32 ci = Ouvrir(p, NkPresetP2D::NK_PONT);
			const NkVec2f ab = b - a;
			const float32 L = math::NkSqrt(Len2(ab));
			int32 n = static_cast<int32>(L / 0.15f) + 1;
			n = n < 3 ? 3 : n;
			p.corps[ci].espacement = L / static_cast<float32>(n - 1);
			p.corps[ci].nx = n;
			const uint32 base = static_cast<uint32>(p.particules.Size());
			for (int32 i = 0; i < n; ++i) {
				const float32 t = static_cast<float32>(i) / static_cast<float32>(n - 1);
				p.AjouterParticule(ci, a + ab * t, 0.075f);
			}
			for (int32 i = 0; i + 1 < n; ++i) {
				const uint32 l = p.AjouterLien(base + static_cast<uint32>(i), base + static_cast<uint32>(i + 1), NkGenreLien2D::NK_STRUCTURE);
				p.liens[l].repos *= 1.06f; // un peu de mou : un pont PEND
				p.liens[l].reposInitial = p.liens[l].repos;
			}
			p.BasculerEpingle(base);
			p.BasculerEpingle(base + static_cast<uint32>(n - 1));
			p.FinaliserCorps(ci);
			return static_cast<int32>(ci);
		}

	} // namespace physics
} // namespace nkentseu
