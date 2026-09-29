// =============================================================================
// NkPhysFabrique.cpp — fabrication des corps et des niveaux
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Physic2D/Physique/NkPhysFabrique.h"

namespace nkentseu {
	namespace physic2d {

		namespace {
			// clang-format off
			const NkInfoActeur kActeurs[static_cast<int32>(NkActeur::NK_A_COUNT)] = {
				{"Ballon",          "Anneau gonflé par un gaz. Crève si on le coupe.",       NkCategorie::NK_CAT_CORPS_MOUS,  NkMateriau::NK_BALLON, false, {232, 72, 60}},
				{"Blob visqueux",   "Coule comme une gelée, se coupe et se ressoude.",          NkCategorie::NK_CAT_CORPS_MOUS,  NkMateriau::NK_BLOB,   false, {124, 230, 76}},
				{"Slime géant",     "Un gros blob, plus mou et plus plastique.",               NkCategorie::NK_CAT_CORPS_MOUS,  NkMateriau::NK_BLOB,   false, {176, 96, 245}},
				{"Gelée",           "Tremble et revient toujours à sa forme.",                 NkCategorie::NK_CAT_CORPS_MOUS,  NkMateriau::NK_GELEE,  false, {255, 92, 172}},
				{"Caisse",          "Presque rigide : appariement de forme intégral.",          NkCategorie::NK_CAT_CORPS_MOUS,  NkMateriau::NK_CAISSE, false, {182, 126, 72}},
				{"Eau",             "Fluide peu visqueux. Maintenir pour verser.",             NkCategorie::NK_CAT_FLUIDES,     NkMateriau::NK_EAU,    true,  {48, 150, 255}},
				{"Miel",            "Fluide très visqueux. Maintenir pour verser.",            NkCategorie::NK_CAT_FLUIDES,     NkMateriau::NK_MIEL,   true,  {246, 172, 34}},
				{"Sable",           "Granulaire : forme des tas. Maintenir pour verser.",      NkCategorie::NK_CAT_FLUIDES,     NkMateriau::NK_SABLE,  true,  {228, 194, 130}},
				{"Bloc atomique",   "Réseau cristallin : se fracture, fond à la chaleur.",     NkCategorie::NK_CAT_STRUCTURES,  NkMateriau::NK_ATOMES, false, {70, 212, 255}},
				{"Disque atomique", "Cristal rond. Chauffez le monde pour le faire fondre.",   NkCategorie::NK_CAT_STRUCTURES,  NkMateriau::NK_ATOMES, false, {188, 204, 224}},
				{"Tissu",           "Grille de liens, déchirable à la main ou au couteau.",    NkCategorie::NK_CAT_STRUCTURES,  NkMateriau::NK_TISSU,  false, {66, 116, 226}},
				{"Corde",           "Chaîne lestée, accrochée en haut.",                       NkCategorie::NK_CAT_STRUCTURES,  NkMateriau::NK_CORDE,  false, {210, 176, 124}},
				{"Pont suspendu",   "Planches reliées entre deux points fixes.",               NkCategorie::NK_CAT_STRUCTURES,  NkMateriau::NK_CORDE,  false, {156, 110, 68}},
			};
			// clang-format on

			void Couleur(NkCorps &c, const uint8 rgb[3]) noexcept {
				c.couleur[0] = rgb[0];
				c.couleur[1] = rgb[1];
				c.couleur[2] = rgb[2];
			}

			/// Remplit un disque de rayon R en reseau hexagonal d'espacement s.
			template <typename F> void DisqueHex(const NkV2 &centre, float32 R, float32 s, F &&ajouter) {
				const float32 dy = s * 0.8660254f;
				const int32 n = static_cast<int32>(R / dy) + 1;
				for (int32 r = -n; r <= n; ++r) {
					const float32 y = static_cast<float32>(r) * dy;
					const float32 decal = (r & 1) ? s * 0.5f : 0.f;
					const int32 m = static_cast<int32>(R / s) + 1;
					for (int32 c = -m; c <= m; ++c) {
						const float32 x = static_cast<float32>(c) * s + decal;
						if (x * x + y * y <= R * R) {
							ajouter(centre + NkV2(x, y));
						}
					}
				}
			}

			/// Relie toutes les paires du corps plus proches que `seuil`.
			void RelierVoisins(NkMonde &m, uint32 ci, float32 seuil, bool reposExact, float32 repos) noexcept {
				const NkCorps &c = m.corps[ci];
				const float32 s2 = seuil * seuil;
				for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
					for (uint32 j = i + 1; j < c.debut + c.nombre; ++j) {
						const NkV2 d = m.particules[j].pos - m.particules[i].pos;
						const float32 d2 = NkLen2(d);
						if (d2 < s2) {
							m.Relier(i, j, reposExact ? repos : std::sqrt(d2));
						}
					}
				}
			}

			/// Grille nx * ny, rangee par lignes. Les liens sont crees dans
			/// l'ordre que le RENDU relit : horizontaux, verticaux, puis le reste.
			uint32 Grille(NkMonde &m, uint32 ci, const NkV2 &origine, int32 nx, int32 ny, float32 s, float32 rayon,
						  bool descendante, bool flexion) noexcept {
				NkCorps &c = m.corps[ci];
				c.nx = nx;
				c.ny = ny;
				c.espacement = s;
				const uint32 base = static_cast<uint32>(m.particules.Size());
				for (int32 j = 0; j < ny; ++j) {
					for (int32 i = 0; i < nx; ++i) {
						const float32 y = descendante ? -static_cast<float32>(j) * s : static_cast<float32>(j) * s;
						m.AjouterParticule(ci, origine + NkV2(static_cast<float32>(i) * s, y), rayon);
					}
				}
				auto id = [&](int32 i, int32 j) { return base + static_cast<uint32>(j * nx + i); };
				for (int32 j = 0; j < ny; ++j) {
					for (int32 i = 0; i + 1 < nx; ++i) {
						m.AjouterLien(id(i, j), id(i + 1, j), NkGenreLien::NK_STRUCTURE);
					}
				}
				for (int32 j = 0; j + 1 < ny; ++j) {
					for (int32 i = 0; i < nx; ++i) {
						m.AjouterLien(id(i, j), id(i, j + 1), NkGenreLien::NK_STRUCTURE);
					}
				}
				for (int32 j = 0; j + 1 < ny; ++j) {
					for (int32 i = 0; i + 1 < nx; ++i) {
						m.AjouterLien(id(i, j), id(i + 1, j + 1), NkGenreLien::NK_CISAILLEMENT);
						m.AjouterLien(id(i + 1, j), id(i, j + 1), NkGenreLien::NK_CISAILLEMENT);
					}
				}
				if (flexion) {
					for (int32 j = 0; j < ny; ++j) {
						for (int32 i = 0; i + 2 < nx; ++i) {
							m.AjouterLien(id(i, j), id(i + 2, j), NkGenreLien::NK_FLEXION);
						}
					}
					for (int32 j = 0; j + 2 < ny; ++j) {
						for (int32 i = 0; i < nx; ++i) {
							m.AjouterLien(id(i, j), id(i, j + 2), NkGenreLien::NK_FLEXION);
						}
					}
				}
				return base;
			}

			/// Ouvre un corps fluide (eau, miel, sable) SANS particule.
			uint32 OuvrirFluide(NkMonde &m, NkActeur a) noexcept {
				const NkInfoActeur &info = NkActeurInfo(a);
				const uint32 ci = m.NouveauCorps(info.materiau, info.nom);
				NkCorps &c = m.corps[ci];
				Couleur(c, info.couleur);
				c.espacement = 10.f;
				c.rebond = 0.f;
				c.autoCollision = true;
				if (a == NkActeur::NK_A_EAU) {
					c.viscosite = 0.03f;
					c.cohesion = 0.30f; // mesure : sous 0.2 une flaque s'etale en film d'une particule
					c.friction = 0.02f;
				} else if (a == NkActeur::NK_A_MIEL) {
					c.viscosite = 0.42f;
					c.cohesion = 0.55f;
					c.friction = 0.35f;
				} else { // sable
					c.friction = 0.9f;
					c.espacement = 9.5f;
				}
				return ci;
			}

			int32 CreerFluide(NkMonde &m, NkActeur a, const NkV2 &pos, float32 R) noexcept {
				const uint32 ci = OuvrirFluide(m, a);
				const float32 s = m.corps[ci].espacement;
				const float32 rayon = a == NkActeur::NK_A_SABLE ? 4.6f : 5.f;
				DisqueHex(pos, R, s, [&](const NkV2 &p) { m.AjouterParticule(ci, p, rayon); });
				m.FinaliserCorps(ci);
				return static_cast<int32>(ci);
			}

			int32 CreerBlob(NkMonde &m, NkActeur a, const NkV2 &pos, float32 R) noexcept {
				const NkInfoActeur &info = NkActeurInfo(a);
				const uint32 ci = m.NouveauCorps(NkMateriau::NK_BLOB, a == NkActeur::NK_A_SLIME ? "Slime" : "Blob");
				NkCorps &c = m.corps[ci];
				Couleur(c, info.couleur);
				c.espacement = 10.f;
				const bool geant = a == NkActeur::NK_A_SLIME;
				// Reglages MESURES au banc (blob de 42 cm pose au sol) : il garde
				// sa forme a l'impact, tremble, puis s'affaisse en dome en ~5 s.
				c.raideur = geant ? 0.30f : 0.40f;
				c.viscosite = geant ? 0.12f : 0.10f;
				c.cohesion = geant ? 0.45f : 0.60f;
				c.plasticite = geant ? 12.f : 7.f;
				c.resistance = 1.9f;
				c.friction = 0.25f;
				c.rebond = 0.05f;
				DisqueHex(pos, R, c.espacement, [&](const NkV2 &p) { m.AjouterParticule(ci, p, 5.f); });
				RelierVoisins(m, ci, c.espacement * 1.3f, false, 0.f);
				m.FinaliserCorps(ci);
				return static_cast<int32>(ci);
			}

			int32 CreerAtomes(NkMonde &m, NkActeur a, const NkV2 &pos) noexcept {
				const NkInfoActeur &info = NkActeurInfo(a);
				const uint32 ci = m.NouveauCorps(NkMateriau::NK_ATOMES, a == NkActeur::NK_A_ATOMES_BLOC ? "Cristal" : "Disque");
				NkCorps &c = m.corps[ci];
				Couleur(c, info.couleur);
				const float32 s = 14.f;
				c.espacement = s;
				c.raideur = 0.93f;
				c.resistance = 1.30f;
				c.friction = 0.5f;
				c.rebond = 0.1f;
				c.autoCollision = true;
				if (a == NkActeur::NK_A_ATOMES_BLOC) {
					const int32 nx = 13;
					const int32 ny = 8;
					const float32 dy = s * 0.8660254f;
					const NkV2 o = pos - NkV2(static_cast<float32>(nx - 1) * s * 0.5f, static_cast<float32>(ny - 1) * dy * 0.5f);
					for (int32 j = 0; j < ny; ++j) {
						const int32 n = (j & 1) ? nx - 1 : nx;
						for (int32 i = 0; i < n; ++i) {
							const float32 decal = (j & 1) ? s * 0.5f : 0.f;
							m.AjouterParticule(ci, o + NkV2(static_cast<float32>(i) * s + decal, static_cast<float32>(j) * dy), s * 0.45f);
						}
					}
				} else {
					DisqueHex(pos, 62.f, s, [&](const NkV2 &p) { m.AjouterParticule(ci, p, s * 0.45f); });
				}
				RelierVoisins(m, ci, s * 1.1f, true, s);
				m.FinaliserCorps(ci);
				return static_cast<int32>(ci);
			}
		} // namespace

		const NkInfoActeur &NkActeurInfo(NkActeur a) noexcept {
			const int32 i = static_cast<int32>(a);
			return kActeurs[(i >= 0 && i < static_cast<int32>(NkActeur::NK_A_COUNT)) ? i : 0];
		}

		const char *NkCategorieNom(NkCategorie c) noexcept {
			switch (c) {
				case NkCategorie::NK_CAT_CORPS_MOUS:
					return "Corps mous";
				case NkCategorie::NK_CAT_FLUIDES:
					return "Fluides";
				case NkCategorie::NK_CAT_STRUCTURES:
					return "Structures";
				default:
					return "?";
			}
		}

		int32 NkCreerPont(NkMonde &m, const NkV2 &a, const NkV2 &b) noexcept {
			const NkInfoActeur &info = NkActeurInfo(NkActeur::NK_A_PONT);
			const uint32 ci = m.NouveauCorps(NkMateriau::NK_CORDE, "Pont");
			NkCorps &c = m.corps[ci];
			Couleur(c, info.couleur);
			c.raideur = 1.f;
			c.friction = 0.7f;
			c.rebond = 0.05f;
			c.autoCollision = false;
			c.masseParticule = 1.5f;
			const float32 L = NkLen(b - a);
			int32 n = static_cast<int32>(L / 15.f) + 1;
			n = n < 3 ? 3 : n;
			c.espacement = L / static_cast<float32>(n - 1);
			c.nx = n;
			const uint32 base = static_cast<uint32>(m.particules.Size());
			for (int32 i = 0; i < n; ++i) {
				const float32 t = static_cast<float32>(i) / static_cast<float32>(n - 1);
				m.AjouterParticule(ci, a + (b - a) * t, 7.5f);
			}
			for (int32 i = 0; i + 1 < n; ++i) {
				const uint32 l = m.AjouterLien(base + static_cast<uint32>(i), base + static_cast<uint32>(i + 1),
											   NkGenreLien::NK_STRUCTURE);
				m.liens[l].repos *= 1.06f; // un peu de mou : un pont PEND
				m.liens[l].reposInitial = m.liens[l].repos;
			}
			m.BasculerEpingle(base);
			m.BasculerEpingle(base + static_cast<uint32>(n - 1));
			m.FinaliserCorps(ci);
			return static_cast<int32>(ci);
		}

		int32 NkCreerActeur(NkMonde &m, NkActeur a, const NkV2 &posDemandee) noexcept {
			const NkInfoActeur &info = NkActeurInfo(a);
			// On pose DANS la boite : un acteur ne nait pas dans un mur.
			const float32 demiL = m.reglages.largeur * 0.5f;
			NkV2 pos(NkClampf(posDemandee.x, -demiL + 60.f, demiL - 60.f),
					 NkClampf(posDemandee.y, 60.f, m.reglages.hauteur - 60.f));

			switch (a) {
				case NkActeur::NK_A_BALLON: {
					const uint32 ci = m.NouveauCorps(NkMateriau::NK_BALLON, "Ballon");
					NkCorps &c = m.corps[ci];
					Couleur(c, info.couleur);
					const float32 R = 55.f;
					const int32 n = static_cast<int32>(2.f * NK_PI * R / 10.f);
					c.nx = n;
					c.raideur = 0.95f;
					c.pression = 1.f;
					c.resistance = 2.4f;
					c.friction = 0.55f;
					c.rebond = 0.45f;
					c.autoCollision = false;
					const uint32 base = static_cast<uint32>(m.particules.Size());
					for (int32 k = 0; k < n; ++k) {
						const float32 t = 2.f * NK_PI * static_cast<float32>(k) / static_cast<float32>(n);
						m.AjouterParticule(ci, pos + NkV2(std::cos(t), std::sin(t)) * R, 5.2f);
					}
					for (int32 k = 0; k < n; ++k) {
						m.AjouterLien(base + static_cast<uint32>(k), base + static_cast<uint32>((k + 1) % n),
									  NkGenreLien::NK_STRUCTURE);
					}
					m.FinaliserCorps(ci);
					return static_cast<int32>(ci);
				}
				case NkActeur::NK_A_BLOB:
					return CreerBlob(m, a, pos, 42.f);
				case NkActeur::NK_A_SLIME:
					return CreerBlob(m, a, pos, 78.f);
				case NkActeur::NK_A_GELEE: {
					const uint32 ci = m.NouveauCorps(NkMateriau::NK_GELEE, "Gelée");
					NkCorps &c = m.corps[ci];
					Couleur(c, info.couleur);
					c.raideur = 0.55f;
					c.formeRaideur = 0.035f;
					c.friction = 0.6f;
					c.rebond = 0.25f;
					c.autoCollision = false;
					const int32 nx = 9;
					const int32 ny = 7;
					const float32 s = 13.f;
					Grille(m, ci, pos - NkV2(static_cast<float32>(nx - 1) * s * 0.5f, static_cast<float32>(ny - 1) * s * 0.5f), nx,
						   ny, s, 6.5f, false, true);
					m.FinaliserCorps(ci);
					return static_cast<int32>(ci);
				}
				case NkActeur::NK_A_CAISSE: {
					const uint32 ci = m.NouveauCorps(NkMateriau::NK_CAISSE, "Caisse");
					NkCorps &c = m.corps[ci];
					Couleur(c, info.couleur);
					c.raideur = 1.f;
					c.formeRaideur = 1.f;
					c.friction = 0.7f;
					c.rebond = 0.1f;
					c.masseParticule = 2.f;
					c.autoCollision = false;
					const int32 n = 4;
					const float32 s = 22.f;
					Grille(m, ci, pos - NkV2(s * 1.5f, s * 1.5f), n, n, s, 11.f, false, false);
					m.FinaliserCorps(ci);
					return static_cast<int32>(ci);
				}
				case NkActeur::NK_A_EAU:
				case NkActeur::NK_A_MIEL:
				case NkActeur::NK_A_SABLE:
					return CreerFluide(m, a, pos, 22.f);
				case NkActeur::NK_A_ATOMES_BLOC:
				case NkActeur::NK_A_ATOMES_DISQUE:
					return CreerAtomes(m, a, pos);
				case NkActeur::NK_A_TISSU: {
					const uint32 ci = m.NouveauCorps(NkMateriau::NK_TISSU, "Tissu");
					NkCorps &c = m.corps[ci];
					Couleur(c, info.couleur);
					c.raideur = 0.97f;
					c.resistance = 1.6f;
					c.friction = 0.4f;
					c.rebond = 0.f;
					c.autoCollision = false;
					c.masseParticule = 0.5f;
					const int32 nx = 26;
					const int32 ny = 18;
					const float32 s = 11.f;
					NkV2 haut = pos + NkV2(0.f, static_cast<float32>(ny - 1) * s * 0.5f);
					haut.y = NkMinf(haut.y, m.reglages.hauteur - 10.f);
					const uint32 base = Grille(m, ci, haut - NkV2(static_cast<float32>(nx - 1) * s * 0.5f, 0.f), nx, ny, s,
											   4.f, true, true);
					for (int32 i = 0; i < nx; i += 5) {
						m.BasculerEpingle(base + static_cast<uint32>(i));
					}
					m.BasculerEpingle(base + static_cast<uint32>(nx - 1));
					m.FinaliserCorps(ci);
					return static_cast<int32>(ci);
				}
				case NkActeur::NK_A_CORDE: {
					const uint32 ci = m.NouveauCorps(NkMateriau::NK_CORDE, "Corde");
					NkCorps &c = m.corps[ci];
					Couleur(c, info.couleur);
					c.raideur = 1.f;
					c.friction = 0.3f;
					c.rebond = 0.1f;
					c.autoCollision = false;
					const int32 n = 34;
					const float32 s = 9.f;
					c.espacement = s;
					c.nx = n;
					const NkV2 haut(pos.x, NkMinf(pos.y + 150.f, m.reglages.hauteur - 10.f));
					const uint32 base = static_cast<uint32>(m.particules.Size());
					for (int32 i = 0; i < n; ++i) {
						const bool lest = i == n - 1;
						const uint32 p = m.AjouterParticule(ci, haut - NkV2(0.f, static_cast<float32>(i) * s), lest ? 12.f : 4.5f);
						if (lest) {
							m.particules[p].masse = 8.f;
							m.particules[p].invMasse = 1.f / 8.f;
						}
					}
					for (int32 i = 0; i + 1 < n; ++i) {
						m.AjouterLien(base + static_cast<uint32>(i), base + static_cast<uint32>(i + 1), NkGenreLien::NK_STRUCTURE);
					}
					m.BasculerEpingle(base);
					m.FinaliserCorps(ci);
					return static_cast<int32>(ci);
				}
				case NkActeur::NK_A_PONT:
					return NkCreerPont(m, pos - NkV2(170.f, 0.f), pos + NkV2(170.f, 0.f));
				default:
					return -1;
			}
		}

		int32 NkOuvrirPinceau(NkMonde &m, NkActeur a) noexcept {
			if (!NkActeurInfo(a).pinceau) {
				return -1;
			}
			return static_cast<int32>(OuvrirFluide(m, a));
		}

		uint32 NkVerser(NkMonde &m, uint32 ci, const NkV2 &pos, float32 rayon, int32 n, NkAlea &alea) noexcept {
			if (ci >= m.corps.Size() || ci + 1u != m.corps.Size()) {
				return 0;
			}
			NkCorps &c = m.corps[ci];
			if (c.debut + c.nombre != m.particules.Size()) {
				return 0;
			}
			const float32 s = c.espacement;
			const float32 r = c.mat == NkMateriau::NK_SABLE ? 4.6f : 5.f;
			uint32 ajoutes = 0;
			for (int32 k = 0; k < n * 3 && static_cast<int32>(ajoutes) < n; ++k) {
				const float32 ang = alea.Uniforme() * 2.f * NK_PI;
				const float32 d = std::sqrt(alea.Uniforme()) * rayon;
				const NkV2 p = pos + NkV2(std::cos(ang), std::sin(ang)) * d;
				if (p.y < r || p.y > m.reglages.hauteur - r || p.x < -m.reglages.largeur * 0.5f + r ||
					p.x > m.reglages.largeur * 0.5f - r) {
					continue;
				}
				// Pas de naissance DANS une autre particule : la pression
				// l'ejecterait a une vitesse absurde.
				bool libre = true;
				const float32 min2 = (s * 0.85f) * (s * 0.85f);
				for (uint32 i = 0; i < m.particules.Size(); ++i) {
					if (NkLen2(m.particules[i].pos - p) < min2) {
						libre = false;
						break;
					}
				}
				if (!libre) {
					continue;
				}
				const uint32 id = m.AjouterParticule(ci, p, r);
				m.particules[id].vit = NkV2(0.f, -150.f);
				++ajoutes;
			}
			return ajoutes;
		}

		const char *NkNiveauNom(int32 i) noexcept {
			switch (i) {
				case 0:
					return "Démo · Tout à la fois";
				case 1:
					return "Bac à slime";
				case 2:
					return "Cristaux et chaleur";
				case 3:
					return "Tissus, cordes et ponts";
				default:
					return "Niveau vide";
			}
		}

		void NkChargerNiveau(NkMonde &m, int32 i) noexcept {
			const NkReglages garde = m.reglages;
			m.Vider();
			m.reglages = NkReglages{};
			m.reglages.sousPas = garde.sousPas;
			auto obstacle = [&](NkV2 a, NkV2 b, float32 r) {
				NkObstacle o;
				o.a = a;
				o.b = b;
				o.rayon = r;
				m.obstacles.PushBack(o);
			};

			switch (i) {
				case 0: {
					obstacle(NkV2(-1150.f, 760.f), NkV2(-640.f, 560.f), 10.f); // rampe
					obstacle(NkV2(150.f, 0.f), NkV2(150.f, 190.f), 9.f);	   // bac a eau
					obstacle(NkV2(470.f, 0.f), NkV2(470.f, 190.f), 9.f);
					obstacle(NkV2(-520.f, 0.f), NkV2(-520.f, 330.f), 12.f); // piliers du pont
					obstacle(NkV2(-120.f, 0.f), NkV2(-120.f, 330.f), 12.f);
					obstacle(NkV2(620.f, 480.f), NkV2(980.f, 480.f), 10.f); // etagere
					obstacle(NkV2(-40.f, 760.f), NkV2(-40.f, 760.f), 22.f); // chevilles
					obstacle(NkV2(80.f, 700.f), NkV2(80.f, 700.f), 22.f);
					NkCreerPont(m, NkV2(-520.f, 345.f), NkV2(-120.f, 345.f));
					CreerFluide(m, NkActeur::NK_A_EAU, NkV2(310.f, 110.f), 95.f);
					NkCreerActeur(m, NkActeur::NK_A_BALLON, NkV2(-1050.f, 900.f));
					CreerBlob(m, NkActeur::NK_A_BLOB, NkV2(-330.f, 800.f), 44.f);
					CreerBlob(m, NkActeur::NK_A_SLIME, NkV2(300.f, 900.f), 70.f);
					NkCreerActeur(m, NkActeur::NK_A_GELEE, NkV2(-300.f, 1150.f));
					NkCreerActeur(m, NkActeur::NK_A_CAISSE, NkV2(760.f, 560.f));
					NkCreerActeur(m, NkActeur::NK_A_TISSU, NkV2(920.f, 1040.f));
					NkCreerActeur(m, NkActeur::NK_A_ATOMES_BLOC, NkV2(-860.f, 60.f));
					NkCreerActeur(m, NkActeur::NK_A_CORDE, NkV2(-60.f, 1100.f));
					break;
				}
				case 1: {
					obstacle(NkV2(-700.f, 420.f), NkV2(-300.f, 120.f), 12.f); // entonnoir
					obstacle(NkV2(300.f, 120.f), NkV2(700.f, 420.f), 12.f);
					obstacle(NkV2(-300.f, 120.f), NkV2(-300.f, 0.f), 12.f);
					obstacle(NkV2(300.f, 120.f), NkV2(300.f, 0.f), 12.f);
					CreerBlob(m, NkActeur::NK_A_SLIME, NkV2(-350.f, 900.f), 80.f);
					CreerBlob(m, NkActeur::NK_A_BLOB, NkV2(0.f, 1150.f), 45.f);
					CreerBlob(m, NkActeur::NK_A_BLOB, NkV2(250.f, 800.f), 40.f);
					CreerBlob(m, NkActeur::NK_A_SLIME, NkV2(520.f, 1150.f), 60.f);
					CreerFluide(m, NkActeur::NK_A_MIEL, NkV2(-850.f, 600.f), 70.f);
					NkCreerActeur(m, NkActeur::NK_A_BALLON, NkV2(900.f, 700.f));
					NkCreerActeur(m, NkActeur::NK_A_GELEE, NkV2(0.f, 600.f));
					break;
				}
				case 2: {
					NkCreerActeur(m, NkActeur::NK_A_ATOMES_BLOC, NkV2(-700.f, 60.f));
					NkCreerActeur(m, NkActeur::NK_A_ATOMES_BLOC, NkV2(-700.f, 400.f));
					NkCreerActeur(m, NkActeur::NK_A_ATOMES_DISQUE, NkV2(0.f, 80.f));
					NkCreerActeur(m, NkActeur::NK_A_ATOMES_DISQUE, NkV2(0.f, 500.f));
					NkCreerActeur(m, NkActeur::NK_A_ATOMES_BLOC, NkV2(650.f, 60.f));
					obstacle(NkV2(400.f, 700.f), NkV2(900.f, 700.f), 10.f);
					NkCreerActeur(m, NkActeur::NK_A_CAISSE, NkV2(650.f, 900.f));
					break;
				}
				case 3: {
					NkCreerActeur(m, NkActeur::NK_A_TISSU, NkV2(-700.f, 1000.f));
					NkCreerActeur(m, NkActeur::NK_A_TISSU, NkV2(-250.f, 1000.f));
					NkCreerActeur(m, NkActeur::NK_A_CORDE, NkV2(250.f, 1050.f));
					NkCreerActeur(m, NkActeur::NK_A_CORDE, NkV2(400.f, 1050.f));
					NkCreerActeur(m, NkActeur::NK_A_CORDE, NkV2(550.f, 1050.f));
					obstacle(NkV2(-900.f, 0.f), NkV2(-900.f, 300.f), 12.f);
					obstacle(NkV2(-300.f, 0.f), NkV2(-300.f, 300.f), 12.f);
					NkCreerPont(m, NkV2(-900.f, 315.f), NkV2(-300.f, 315.f));
					NkCreerActeur(m, NkActeur::NK_A_BALLON, NkV2(-600.f, 600.f));
					NkCreerActeur(m, NkActeur::NK_A_CAISSE, NkV2(-750.f, 700.f));
					NkCreerActeur(m, NkActeur::NK_A_GELEE, NkV2(-450.f, 750.f));
					break;
				}
				default:
					break;
			}
		}

	} // namespace physic2d
} // namespace nkentseu
