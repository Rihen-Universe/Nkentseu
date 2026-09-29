// =============================================================================
// NkPhysMonde.cpp — le solveur
//
// L'ORDRE D'UN SOUS-PAS, ET POURQUOI CET ORDRE
//   1. Predire     v += a h ; x* = x + v h           (forces, chaleur, vent)
//   2. Grille      une seule construction par sous-pas, relue par 5 passes
//   3. Saisie      la souris tire AVANT les contraintes : elles corrigent
//   4. Liens       XPBD, avec rupture et plasticite
//   5. Pression    le ballon
//   6. Forme       gelee, caisse
//   7. Particules  densite des fluides, puis relaxation + collisions
//   8. Statiques   EN DERNIER : rien ne doit finir dans un mur
//   9. Vitesses    v = (x - x_prec) / h, restitution sur les statiques
//  10. Viscosite   XSPH, sur les vitesses deja calculees
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Physic2D/Physique/NkPhysMonde.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace physic2d {

		// =====================================================================
		// Materiaux
		// =====================================================================
		const char *NkMateriauNom(NkMateriau m) noexcept {
			switch (m) {
				case NkMateriau::NK_BALLON:
					return "Ballon";
				case NkMateriau::NK_BLOB:
					return "Blob";
				case NkMateriau::NK_GELEE:
					return "Gelée";
				case NkMateriau::NK_EAU:
					return "Eau";
				case NkMateriau::NK_MIEL:
					return "Miel";
				case NkMateriau::NK_SABLE:
					return "Sable";
				case NkMateriau::NK_ATOMES:
					return "Atomes";
				case NkMateriau::NK_CAISSE:
					return "Caisse";
				case NkMateriau::NK_TISSU:
					return "Tissu";
				case NkMateriau::NK_CORDE:
					return "Corde";
				default:
					return "?";
			}
		}

		bool NkEstFluide(NkMateriau m) noexcept {
			return m == NkMateriau::NK_EAU || m == NkMateriau::NK_MIEL || m == NkMateriau::NK_BLOB;
		}

		bool NkEstGrille(NkMateriau m) noexcept {
			return m == NkMateriau::NK_GELEE || m == NkMateriau::NK_CAISSE || m == NkMateriau::NK_TISSU;
		}

		bool NkEstSoudable(NkMateriau m) noexcept {
			return m == NkMateriau::NK_ATOMES || m == NkMateriau::NK_BLOB;
		}

		float32 NkCompliance(float32 raideur) noexcept {
			// Echelle choisie pour h ~ 1/480 s : a 0.5 le lien rattrape environ
			// un tiers de son ecart par sous-pas, a 0.9 presque tout, a 1 tout.
			const float32 s = 1.f - NkClampf(raideur, 0.f, 1.f);
			return 2.0e-5f * s * s * s + 1.0e-10f;
		}

		float32 NkDensiteReseau(float32 s, float32 h) noexcept {
			float32 rho = 0.f;
			const float32 dy = s * 0.8660254f;
			const int32 n = static_cast<int32>(h / dy) + 2;
			for (int32 r = -n; r <= n; ++r) {
				const float32 decal = (r & 1) ? s * 0.5f : 0.f;
				for (int32 c = -n - 1; c <= n + 1; ++c) {
					if (r == 0 && c == 0) {
						continue;
					}
					const float32 x = static_cast<float32>(c) * s + decal;
					const float32 y = static_cast<float32>(r) * dy;
					const float32 d = std::sqrt(x * x + y * y);
					if (d < h) {
						const float32 q = 1.f - d / h;
						rho += q * q;
					}
				}
			}
			return rho;
		}

		// =====================================================================
		// Construction
		// =====================================================================
		NkMonde::NkMonde() {
			const float32 s = 10.f;
			const float32 h = 2.2f * s;
			const float32 rho0 = NkDensiteReseau(s, h);
			for (int32 i = 0; i < static_cast<int32>(NkMateriau::NK_COUNT); ++i) {
				mFluide[i].h = h;
				mFluide[i].rho0 = rho0;
				mFluide[i].kPression = 0.20f;
				mFluide[i].kProche = 0.12f;
			}
			mFluide[static_cast<int32>(NkMateriau::NK_EAU)].kPression = 0.24f;
			mFluide[static_cast<int32>(NkMateriau::NK_EAU)].kProche = 0.07f;
			mFluide[static_cast<int32>(NkMateriau::NK_MIEL)].kPression = 0.22f;
			mFluide[static_cast<int32>(NkMateriau::NK_BLOB)].kPression = 0.18f;
			mFluide[static_cast<int32>(NkMateriau::NK_BLOB)].kProche = 0.12f;
		}

		void NkMonde::Vider() noexcept {
			particules.Clear();
			liens.Clear();
			corps.Clear();
			obstacles.Clear();
			mSaisis.Clear();
			mSaisisDecalage.Clear();
			rupturesTotal = 0;
			mCasseesPurgeables = 0;
			mGrilleValide = false;
			for (int32 i = 0; i < static_cast<int32>(NkMateriau::NK_COUNT); ++i) {
				compteurNoms[i] = 0;
			}
			stats = NkStatsPas{};
		}

		NkParamsFluide NkMonde::ParamsFluide(NkMateriau m) const noexcept {
			return mFluide[static_cast<int32>(m)];
		}

		uint32 NkMonde::NouveauCorps(NkMateriau mat, const char *prefixeNom) noexcept {
			NkCorps c;
			c.mat = mat;
			c.id = prochainId++;
			c.debut = static_cast<uint32>(particules.Size());
			c.nombre = 0;
			const uint32 n = ++compteurNoms[static_cast<int32>(mat)];
			std::snprintf(c.nom, sizeof(c.nom), "%s_%u", prefixeNom != nullptr ? prefixeNom : NkMateriauNom(mat),
						  static_cast<unsigned>(n));
			corps.PushBack(c);
			return static_cast<uint32>(corps.Size() - 1u);
		}

		uint32 NkMonde::AjouterParticule(uint32 ci, const NkV2 &pos, float32 rayon) noexcept {
			NkParticule p;
			p.pos = pos;
			p.prec = pos;
			p.rayon = rayon;
			p.corps = ci;
			p.masse = corps[ci].masseParticule;
			p.invMasse = p.masse > 0.f ? 1.f / p.masse : 0.f;
			particules.PushBack(p);
			corps[ci].nombre++;
			return static_cast<uint32>(particules.Size() - 1u);
		}

		uint32 NkMonde::AjouterLien(uint32 a, uint32 b, NkGenreLien genre) noexcept {
			NkLien l;
			l.a = a;
			l.b = b;
			l.corps = particules[a].corps;
			l.repos = NkLen(particules[b].pos - particules[a].pos);
			l.reposInitial = l.repos;
			l.genre = genre;
			liens.PushBack(l);
			return static_cast<uint32>(liens.Size() - 1u);
		}

		bool NkMonde::Relier(uint32 a, uint32 b, float32 repos) noexcept {
			if (a == b) {
				return false;
			}
			NkParticule &pa = particules[a];
			NkParticule &pb = particules[b];
			if (pa.nbVoisins >= NK_PHYS_MAX_VOISINS || pb.nbVoisins >= NK_PHYS_MAX_VOISINS) {
				return false;
			}
			for (uint8 k = 0; k < pa.nbVoisins; ++k) {
				if (pa.voisins[k] == b) {
					return false;
				}
			}
			pa.voisins[pa.nbVoisins++] = b;
			pb.voisins[pb.nbVoisins++] = a;
			NkLien l;
			l.a = a;
			l.b = b;
			l.corps = pa.corps;
			l.repos = repos;
			l.reposInitial = repos;
			l.genre = NkGenreLien::NK_LIAISON;
			liens.PushBack(l);
			return true;
		}

		void NkMonde::RetirerVoisin(uint32 a, uint32 b) noexcept {
			NkParticule &pa = particules[a];
			for (uint8 k = 0; k < pa.nbVoisins; ++k) {
				if (pa.voisins[k] == b) {
					pa.voisins[k] = pa.voisins[--pa.nbVoisins];
					break;
				}
			}
			NkParticule &pb = particules[b];
			for (uint8 k = 0; k < pb.nbVoisins; ++k) {
				if (pb.voisins[k] == a) {
					pb.voisins[k] = pb.voisins[--pb.nbVoisins];
					break;
				}
			}
		}

		void NkMonde::FinaliserCorps(uint32 ci) noexcept {
			NkCorps &c = corps[ci];
			if (c.nombre == 0) {
				return;
			}
			// Positions de repos relatives au centre de masse de repos.
			NkV2 centre;
			float32 m = 0.f;
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				centre += particules[i].pos * particules[i].masse;
				m += particules[i].masse;
			}
			centre = centre / (m > 0.f ? m : 1.f);
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				particules[i].repos = particules[i].pos - centre;
			}
			// Aire de l'anneau (sens trigonometrique : positive).
			if (c.mat == NkMateriau::NK_BALLON) {
				float32 aire = 0.f;
				for (uint32 k = 0; k < c.nombre; ++k) {
					const NkV2 &p0 = particules[c.debut + k].pos;
					const NkV2 &p1 = particules[c.debut + (k + 1) % c.nombre].pos;
					aire += NkCross(p0, p1);
				}
				c.aireRepos = aire * 0.5f;
			}
			RecalculerPlages();
		}

		void NkMonde::RecalculerPlages() noexcept {
			const uint32 nc = static_cast<uint32>(corps.Size());
			for (uint32 c = 0; c < nc; ++c) {
				corps[c].debut = NK_PHYS_AUCUN;
				corps[c].nombre = 0;
				corps[c].lienDebut = NK_PHYS_AUCUN;
				corps[c].lienNombre = 0;
			}
			for (uint32 i = 0; i < particules.Size(); ++i) {
				NkCorps &c = corps[particules[i].corps];
				if (c.debut == NK_PHYS_AUCUN) {
					c.debut = i;
				}
				c.nombre++;
			}
			for (uint32 i = 0; i < liens.Size(); ++i) {
				const NkLien &l = liens[i];
				if (l.genre == NkGenreLien::NK_LIAISON) {
					continue;
				}
				NkCorps &c = corps[l.corps];
				if (c.lienDebut == NK_PHYS_AUCUN) {
					c.lienDebut = i;
				}
				c.lienNombre++;
			}
			for (uint32 c = 0; c < nc; ++c) {
				if (corps[c].debut == NK_PHYS_AUCUN) {
					corps[c].debut = 0;
				}
				if (corps[c].lienDebut == NK_PHYS_AUCUN) {
					corps[c].lienDebut = 0;
				}
			}
		}

		// =====================================================================
		// Grille spatiale (tri par comptage, reconstruite a chaque sous-pas)
		// =====================================================================
		int32 NkMonde::CelluleX(float32 x) const noexcept {
			int32 i = static_cast<int32>((x - mOrigine.x) / mCell);
			return i < 0 ? 0 : (i >= mGx ? mGx - 1 : i);
		}
		int32 NkMonde::CelluleY(float32 y) const noexcept {
			int32 i = static_cast<int32>((y - mOrigine.y) / mCell);
			return i < 0 ? 0 : (i >= mGy ? mGy - 1 : i);
		}

		void NkMonde::ConstruireGrille() noexcept {
			float32 rmax = 4.f;
			for (uint32 i = 0; i < particules.Size(); ++i) {
				rmax = NkMaxf(rmax, particules[i].rayon);
			}
			// La cellule couvre a la fois un contact (2 rayons) et le rayon
			// d'interaction des fluides : 3x3 cellules suffisent toujours.
			mCell = NkMaxf(2.f * rmax + 1.f, mFluide[0].h);
			const float32 marge = mCell * 2.f;
			mOrigine = NkV2(-reglages.largeur * 0.5f - marge, -marge);
			mGx = static_cast<int32>((reglages.largeur + 2.f * marge) / mCell) + 1;
			mGy = static_cast<int32>((reglages.hauteur + 2.f * marge) / mCell) + 1;
			const uint32 nCell = static_cast<uint32>(mGx * mGy);
			if (mCellDebut.Size() != nCell) {
				mCellDebut.Resize(nCell);
				mCellFin.Resize(nCell);
			}
			for (uint32 c = 0; c < nCell; ++c) {
				mCellDebut[c] = 0;
				mCellFin[c] = 0;
			}
			const uint32 n = static_cast<uint32>(particules.Size());
			mTri.Resize(n);
			mCleTri.Resize(n);
			for (uint32 i = 0; i < n; ++i) {
				const uint32 cle =
					static_cast<uint32>(CelluleY(particules[i].pos.y) * mGx + CelluleX(particules[i].pos.x));
				mCleTri[i] = cle;
				mCellFin[cle]++;
			}
			int32 somme = 0;
			for (uint32 c = 0; c < nCell; ++c) {
				const int32 k = mCellFin[c];
				mCellDebut[c] = somme;
				mCellFin[c] = somme; // sert de curseur d'ecriture
				somme += k;
			}
			for (uint32 i = 0; i < n; ++i) {
				mTri[static_cast<uint32>(mCellFin[mCleTri[i]]++)] = i;
			}
			mGrilleValide = true;
			ConstruirePaires();
		}

		// Parcourt chaque paire (i, j > i) dont les cellules se touchent.
		template <typename F> static inline void NkPourPaires(const NkVector<NkParticule> &ps,
															   const NkVector<int32> &debut, const NkVector<int32> &fin,
															   const NkVector<uint32> &tri, int32 gx, int32 gy,
															   const NkV2 &origine, float32 cell, F &&f) {
			const uint32 n = static_cast<uint32>(ps.Size());
			for (uint32 i = 0; i < n; ++i) {
				int32 cx = static_cast<int32>((ps[i].pos.x - origine.x) / cell);
				int32 cy = static_cast<int32>((ps[i].pos.y - origine.y) / cell);
				cx = cx < 0 ? 0 : (cx >= gx ? gx - 1 : cx);
				cy = cy < 0 ? 0 : (cy >= gy ? gy - 1 : cy);
				const int32 y0 = cy > 0 ? cy - 1 : 0;
				const int32 y1 = cy < gy - 1 ? cy + 1 : gy - 1;
				const int32 x0 = cx > 0 ? cx - 1 : 0;
				const int32 x1 = cx < gx - 1 ? cx + 1 : gx - 1;
				for (int32 y = y0; y <= y1; ++y) {
					for (int32 x = x0; x <= x1; ++x) {
						const int32 c = y * gx + x;
						for (int32 k = debut[c]; k < fin[c]; ++k) {
							const uint32 j = tri[static_cast<uint32>(k)];
							if (j > i) {
								f(i, j);
							}
						}
					}
				}
			}
		}

		// La liste des paires PROCHES est construite une fois par sous-pas et
		// relue par toutes les passes : parcourir les cellules trois fois
		// coutait plus que les calculs eux-memes.
		void NkMonde::ConstruirePaires() noexcept {
			const uint32 n = static_cast<uint32>(particules.Size());
			mEstFluide.Resize(n);
			for (uint32 i = 0; i < n; ++i) {
				mEstFluide[i] = NkEstFluide(corps[particules[i].corps].mat) ? 1u : 0u;
			}
			mPaires.Clear();
			const float32 hF = mFluide[0].h;
			const NkParticule *ps = particules.Data();
			const uint8 *fl = mEstFluide.Data();
			NkPourPaires(particules, mCellDebut, mCellFin, mTri, mGx, mGy, mOrigine, mCell, [&](uint32 i, uint32 j) {
				const NkParticule &a = ps[i];
				const NkParticule &b = ps[j];
				if (a.invMasse + b.invMasse <= 0.f) {
					return;
				}
				const bool ff = fl[i] && fl[j];
				if (!ff && a.corps == b.corps && !corps[a.corps].autoCollision) {
					return;
				}
				const float32 portee = ff ? hF * 1.1f : (a.rayon + b.rayon) * 1.25f;
				if (NkLen2(b.pos - a.pos) >= portee * portee) {
					return;
				}
				mPaires.PushBack(i);
				mPaires.PushBack(j);
			});
		}

		template <typename F> void NkMonde::PourPaires(F &&f) noexcept {
			const uint32 *pr = mPaires.Data();
			const uint32 np = static_cast<uint32>(mPaires.Size());
			for (uint32 k = 0; k < np; k += 2) {
				f(pr[k], pr[k + 1]);
			}
		}

		// =====================================================================
		// Pas
		// =====================================================================
		void NkMonde::Pas(float32 dt) noexcept {
			stats.ruptures = 0;
			stats.contacts = 0;
			stats.soudures = 0;
			const int32 n = reglages.sousPas < 1 ? 1 : (reglages.sousPas > 40 ? 40 : reglages.sousPas);
			const float32 h = dt * reglages.echelleTemps / static_cast<float32>(n);
			if (h <= 0.f || particules.Size() == 0) {
				stats.energieCinetique = 0.f;
				return;
			}
			for (int32 k = 0; k < n; ++k) {
				SousPas(h);
			}
			Souder();
			if (mCasseesPurgeables > 512) {
				PurgerLiens();
			}

			// Ce qui est tombe hors du monde (murs coupes) disparait.
			bool horsMonde = false;
			for (uint32 i = 0; i < particules.Size(); ++i) {
				const NkV2 &p = particules[i].pos;
				if (p.y < -600.f || p.x < -reglages.largeur || p.x > reglages.largeur) {
					horsMonde = true;
					break;
				}
			}
			if (horsMonde) {
				NkVector<uint8> garder;
				garder.Resize(particules.Size());
				for (uint32 i = 0; i < particules.Size(); ++i) {
					const NkV2 &p = particules[i].pos;
					garder[i] = (p.y < -600.f || p.x < -reglages.largeur || p.x > reglages.largeur) ? 0u : 1u;
				}
				Compacter(garder);
			}

			float32 e = 0.f;
			for (uint32 i = 0; i < particules.Size(); ++i) {
				e += 0.5f * particules[i].masse * NkLen2(particules[i].vit);
			}
			stats.energieCinetique = e * 1.0e-4f; // cm^2 -> m^2 : des joules pour une masse en kg
		}

		void NkMonde::SousPas(float32 h) noexcept {
			Predire(h);
			ConstruireGrille();
			ResoudreSaisie();
			ResoudreLiens(h);
			ResoudrePression(h);
			ResoudreForme();
			ResoudreParticules();
			ResoudreStatiques();
			MajVitesses(h);
			Viscosite();
		}

		void NkMonde::Predire(float32 h) noexcept {
			const float32 amort = NkClampf(1.f - reglages.amortAir * h, 0.f, 1.f);
			const float32 agitation = reglages.temperature * 70.f * std::sqrt(h);
			for (uint32 i = 0; i < particules.Size(); ++i) {
				NkParticule &p = particules[i];
				p.prec = p.pos;
				if (p.epingle || p.invMasse == 0.f) {
					p.vit = NkV2();
					continue;
				}
				const NkMateriau mat = corps[p.corps].mat;
				NkV2 a = reglages.gravite;
				// Le vent pousse ce qui a de la SURFACE plus que ce qui a du poids.
				const bool voile = mat == NkMateriau::NK_TISSU || mat == NkMateriau::NK_BALLON;
				a.x += reglages.vent * (voile ? 1.5f : 0.35f);
				p.vit += a * h;
				if (agitation > 0.f && (mat == NkMateriau::NK_ATOMES || NkEstFluide(mat))) {
					const float32 f = mat == NkMateriau::NK_ATOMES ? 1.f : 0.5f;
					p.vit += NkV2(mAlea.Signe(), mAlea.Signe()) * (agitation * f);
				}
				p.vit *= amort;
				p.pos += p.vit * h;
			}
		}

		void NkMonde::ResoudreSaisie() noexcept {
			for (uint32 k = 0; k < mSaisis.Size(); ++k) {
				const uint32 i = mSaisis[k];
				if (i >= particules.Size()) {
					continue;
				}
				NkParticule &p = particules[i];
				const NkV2 cible = mCibleSaisie + mSaisisDecalage[k];
				if (p.epingle) {
					p.pos = cible;
					p.prec = cible;
				} else {
					p.pos += (cible - p.pos) * 0.35f;
				}
			}
		}

		void NkMonde::ResoudreLiens(float32 h) noexcept {
			const float32 h2 = h * h;
			for (uint32 li = 0; li < liens.Size(); ++li) {
				NkLien &l = liens[li];
				if (l.casse) {
					continue;
				}
				NkParticule &pa = particules[l.a];
				NkParticule &pb = particules[l.b];
				const float32 w = pa.invMasse + pb.invMasse;
				if (w <= 0.f) {
					continue;
				}
				const NkV2 d = pb.pos - pa.pos;
				const float32 len = NkLen(d);
				if (len < 1.0e-6f) {
					continue;
				}
				NkCorps &c = corps[l.corps];
				const float32 etirement = len / (l.repos > 1.0e-4f ? l.repos : 1.0e-4f);
				l.tension = etirement - 1.f;
				// Un ressort PLASTIQUE a une longueur de repos qui derive : sa rupture
				// se juge contre sa longueur de NAISSANCE, sinon il ne romprait jamais
				// et la matiere ne pourrait pas changer de voisins -- donc pas couler.
				const bool plastique = l.genre == NkGenreLien::NK_LIAISON && c.plasticite > 0.f && c.mat == NkMateriau::NK_BLOB;
				const float32 etirementRupture = plastique ? len / NkMaxf(l.reposInitial, 1.0e-4f) : etirement;
				if (c.resistance > 0.f && etirementRupture > c.resistance) {
					l.casse = true;
					stats.ruptures++;
					rupturesTotal++;
					if (l.genre == NkGenreLien::NK_LIAISON) {
						RetirerVoisin(l.a, l.b);
					}
					if (!NkEstGrille(c.mat)) {
						mCasseesPurgeables++;
					}
					if (c.mat == NkMateriau::NK_BALLON) {
						c.pression = 0.f; // creve : le gaz s'echappe
					}
					continue;
				}
				float32 facteur = 1.f;
				if (l.genre == NkGenreLien::NK_CISAILLEMENT) {
					facteur = 4.f;
				} else if (l.genre == NkGenreLien::NK_FLEXION) {
					facteur = 20.f;
				}
				const float32 alpha = NkCompliance(c.raideur) * facteur / h2;
				const float32 C = len - l.repos;
				const float32 lambda = -C / (w + alpha);
				const NkV2 corr = d * (lambda / len);
				pa.pos -= corr * pa.invMasse;
				pb.pos += corr * pb.invMasse;

				// Plasticite du blob (modele de Maxwell) : chaque ressort OUBLIE
				// peu a peu sa longueur de repos au profit de sa longueur actuelle.
				// Un mouvement rapide le trouve elastique (la gelee tremble) ; une
				// charge lente -- son propre poids -- le fait couler. Le temps de
				// relaxation vaut ~ 1 / plasticite secondes.
				if (l.genre == NkGenreLien::NK_LIAISON && c.plasticite > 0.f && c.mat == NkMateriau::NK_BLOB) {
					const float32 ecart = len - l.repos;
					l.repos += ecart * NkMinf(c.plasticite * h, 1.f);
					l.repos = NkClampf(l.repos, 0.6f * l.reposInitial, 2.2f * l.reposInitial);
				}
			}
		}

		void NkMonde::ResoudrePression(float32 h) noexcept {
			for (uint32 ci = 0; ci < corps.Size(); ++ci) {
				NkCorps &c = corps[ci];
				if (c.mat != NkMateriau::NK_BALLON || c.nombre < 3 || c.pression <= 0.f) {
					continue;
				}
				const uint32 n = c.nombre;
				const uint32 d0 = c.debut;
				float32 aire = 0.f;
				for (uint32 k = 0; k < n; ++k) {
					aire += NkCross(particules[d0 + k].pos, particules[d0 + (k + 1) % n].pos);
				}
				aire *= 0.5f;
				const float32 C = aire - c.aireRepos * c.pression;
				// Gradients calcules sur les positions AVANT correction.
				float32 denom = 0.f;
				NkVector<NkV2> &g = mTmpV2;
				g.Resize(n);
				for (uint32 k = 0; k < n; ++k) {
					const NkV2 &pp = particules[d0 + (k + n - 1) % n].pos;
					const NkV2 &ps = particules[d0 + (k + 1) % n].pos;
					g[k] = NkV2(0.5f * (ps.y - pp.y), 0.5f * (pp.x - ps.x));
					denom += particules[d0 + k].invMasse * NkLen2(g[k]);
				}
				const float32 alpha = NkCompliance(c.raideur) * 0.02f / (h * h);
				if (denom + alpha <= 1.0e-8f) {
					continue;
				}
				const float32 lambda = -C / (denom + alpha);
				for (uint32 k = 0; k < n; ++k) {
					particules[d0 + k].pos += g[k] * (lambda * particules[d0 + k].invMasse);
				}
			}
		}

		void NkMonde::ResoudreForme() noexcept {
			for (uint32 ci = 0; ci < corps.Size(); ++ci) {
				const NkCorps &c = corps[ci];
				if (c.formeRaideur <= 0.f || c.nombre < 2) {
					continue;
				}
				NkV2 centre;
				float32 m = 0.f;
				for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
					centre += particules[i].pos * particules[i].masse;
					m += particules[i].masse;
				}
				centre = centre / (m > 0.f ? m : 1.f);
				float32 sa = 0.f;
				float32 sb = 0.f;
				for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
					const NkV2 p = particules[i].pos - centre;
					const NkV2 &q = particules[i].repos;
					sa += particules[i].masse * NkDot(q, p);
					sb += particules[i].masse * NkCross(q, p);
				}
				const float32 ang = std::atan2(sb, sa);
				const float32 cs = std::cos(ang);
				const float32 sn = std::sin(ang);
				const float32 k = NkClampf(c.formeRaideur, 0.f, 1.f);
				for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
					NkParticule &p = particules[i];
					if (p.invMasse == 0.f) {
						continue;
					}
					const NkV2 but = centre + NkRotation(p.repos, cs, sn);
					p.pos += (but - p.pos) * k;
				}
			}
		}

		void NkMonde::ResoudreParticules() noexcept {
			const float32 hF = mFluide[0].h;
			const float32 hF2 = hF * hF;

			// --- 1. densites des fluides -------------------------------------
			for (uint32 i = 0; i < particules.Size(); ++i) {
				particules[i].densite = 0.f;
				particules[i].densiteProche = 0.f;
			}
			PourPaires([&](uint32 i, uint32 j) {
				NkParticule &a = particules[i];
				NkParticule &b = particules[j];
				if (!mEstFluide[i] || !mEstFluide[j]) {
					return;
				}
				const NkV2 d = b.pos - a.pos;
				const float32 r2 = NkLen2(d);
				if (r2 >= hF2) {
					return;
				}
				const float32 q = 1.f - std::sqrt(r2) / hF;
				const float32 q2 = q * q;
				a.densite += q2;
				b.densite += q2;
				a.densiteProche += q2 * q;
				b.densiteProche += q2 * q;
			});

			// --- 2. relaxation des fluides + collisions ------------------------
			const bool collisions = reglages.collisions;
			uint32 contacts = 0;
			PourPaires([&](uint32 i, uint32 j) {
				NkParticule &a = particules[i];
				NkParticule &b = particules[j];
				const float32 w = a.invMasse + b.invMasse;
				if (w <= 0.f) {
					return;
				}
				const NkCorps &ca = corps[a.corps];
				const NkCorps &cb = corps[b.corps];
				const bool fa = mEstFluide[i] != 0;
				const bool fb = mEstFluide[j] != 0;
				NkV2 d = b.pos - a.pos;
				float32 r2 = NkLen2(d);

				if (fa && fb) {
					if (r2 >= hF2 || r2 < 1.0e-10f) {
						if (r2 < 1.0e-10f) {
							// Deux particules confondues : on les separe au hasard,
							// sinon aucune direction n'existe pour les ecarter.
							b.pos += NkV2(mAlea.Signe(), mAlea.Signe()) * 0.5f;
						}
						return;
					}
					const float32 r = std::sqrt(r2);
					const float32 q = 1.f - r / hF;
					const NkParamsFluide &pa = mFluide[static_cast<int32>(ca.mat)];
					const NkParamsFluide &pb = mFluide[static_cast<int32>(cb.mat)];
					float32 Pa = pa.kPression * (a.densite - pa.rho0);
					float32 Pb = pb.kPression * (b.densite - pb.rho0);
					// La cohesion autorise une pression NEGATIVE : la surface tire
					// sur ses voisines, c'est la tension de surface.
					// Seulement DANS la matiere : une particule isolee (un filet, une
					// gouttelette) n'attire plus, sinon l'eau tisserait des fils qui
					// pendent dans le vide (instabilite de traction des SPH).
					const float32 ta = a.densite > 0.40f * pa.rho0 ? ca.cohesion : 0.f;
					const float32 tb = b.densite > 0.40f * pb.rho0 ? cb.cohesion : 0.f;
					Pa = NkMaxf(Pa, -ta * pa.kPression * pa.rho0 * 0.5f);
					Pb = NkMaxf(Pb, -tb * pb.kPression * pb.rho0 * 0.5f);
					const float32 Pn = 0.5f * (pa.kProche * a.densiteProche + pb.kProche * b.densiteProche);
					const float32 D = (0.5f * (Pa + Pb) * q + Pn * q * q) * hF * 0.5f;
					const NkV2 n = d / r;
					a.pos -= n * (D * a.invMasse / w);
					b.pos += n * (D * b.invMasse / w);
					return;
				}
				if (!collisions) {
					return;
				}
				if (a.corps == b.corps && !ca.autoCollision) {
					return;
				}
				const float32 rr = a.rayon + b.rayon;
				if (r2 >= rr * rr) {
					return;
				}
				// Les deux bouts d'un lien de corde ne se repoussent pas.
				if (r2 < 1.0e-10f) {
					d = NkV2(0.f, 0.01f);
					r2 = 1.0e-4f;
				}
				const float32 r = std::sqrt(r2);
				const NkV2 n = d / r;
				const float32 prof = rr - r;
				a.pos -= n * (prof * a.invMasse / w);
				b.pos += n * (prof * b.invMasse / w);
				++contacts;

				// Frottement de Coulomb en positions (Macklin 2014) : on annule
				// le glissement relatif tant qu'il reste sous mu * profondeur.
				const float32 mu = 0.5f * (ca.friction + cb.friction) * 1.2f;
				if (mu > 0.f) {
					const NkV2 rel = (a.pos - a.prec) - (b.pos - b.prec);
					const NkV2 tang = rel - n * NkDot(rel, n);
					const float32 lt = NkLen(tang);
					if (lt > 1.0e-7f) {
						const float32 k = (lt < mu * prof) ? 1.f : NkMinf(mu * prof / lt, 1.f);
						a.pos -= tang * (k * a.invMasse / w);
						b.pos += tang * (k * b.invMasse / w);
					}
				}
			});
			stats.contacts += contacts;
		}

		void NkMonde::Contact(NkParticule &p, const NkV2 &n, float32 profondeur, float32 friction) noexcept {
			p.pos += n * profondeur;
			p.contact = true;
			p.normale = n;
			const float32 mu = friction * 1.5f;
			if (mu <= 0.f) {
				return;
			}
			const NkV2 dep = p.pos - p.prec;
			const NkV2 tang = dep - n * NkDot(dep, n);
			const float32 lt = NkLen(tang);
			if (lt < 1.0e-7f) {
				return;
			}
			const float32 k = (lt < mu * profondeur) ? 1.f : NkMinf(mu * profondeur / lt, 1.f);
			p.pos -= tang * k;
		}

		void NkMonde::ResoudreStatiques() noexcept {
			const float32 demiL = reglages.largeur * 0.5f;
			const float32 H = reglages.hauteur;
			for (uint32 i = 0; i < particules.Size(); ++i) {
				NkParticule &p = particules[i];
				p.contact = false;
				if (p.invMasse == 0.f) {
					continue;
				}
				const NkCorps &c = corps[p.corps];
				const float32 r = p.rayon;
				const float32 f = c.friction;
				const bool surSol = reglages.murs || (p.pos.x > -demiL && p.pos.x < demiL);
				if (surSol && p.pos.y < r && p.prec.y > -r * 2.f) {
					Contact(p, NkV2(0.f, 1.f), r - p.pos.y, f);
				}
				if (reglages.murs) {
					if (p.pos.x < -demiL + r) {
						Contact(p, NkV2(1.f, 0.f), -demiL + r - p.pos.x, f);
					}
					if (p.pos.x > demiL - r) {
						Contact(p, NkV2(-1.f, 0.f), p.pos.x - (demiL - r), f);
					}
					if (p.pos.y > H - r) {
						Contact(p, NkV2(0.f, -1.f), p.pos.y - (H - r), f);
					}
				}
				for (uint32 k = 0; k < obstacles.Size(); ++k) {
					const NkObstacle &o = obstacles[k];
					const NkV2 ab = o.b - o.a;
					const float32 l2 = NkLen2(ab);
					float32 t = l2 > 1.0e-6f ? NkDot(p.pos - o.a, ab) / l2 : 0.f;
					t = NkClampf(t, 0.f, 1.f);
					const NkV2 proche = o.a + ab * t;
					const NkV2 d = p.pos - proche;
					const float32 dist2 = NkLen2(d);
					const float32 rr = o.rayon + r;
					if (dist2 >= rr * rr) {
						continue;
					}
					const float32 dist = std::sqrt(dist2);
					NkV2 n = dist > 1.0e-6f ? d / dist : NkPerp(ab / NkMaxf(std::sqrt(l2), 1.0e-6f));
					if (dist <= 1.0e-6f && l2 <= 1.0e-6f) {
						n = NkV2(0.f, 1.f);
					}
					Contact(p, n, rr - dist, f);
				}
			}
		}

		void NkMonde::MajVitesses(float32 h) noexcept {
			const float32 invH = 1.f / h;
			const float32 vMax = 8000.f;
			for (uint32 i = 0; i < particules.Size(); ++i) {
				NkParticule &p = particules[i];
				if (p.epingle || p.invMasse == 0.f) {
					if (!p.saisie) {
						p.pos = p.prec;
					}
					p.vit = NkV2();
					continue;
				}
				NkV2 v = (p.pos - p.prec) * invH;
				if (p.contact) {
					const float32 vnAvant = NkDot(p.vit, p.normale);
					if (vnAvant < -80.f) {
						const float32 vn = NkDot(v, p.normale);
						v += p.normale * (-corps[p.corps].rebond * vnAvant - vn);
					}
				}
				const float32 l2 = NkLen2(v);
				if (l2 > vMax * vMax) {
					v *= vMax / std::sqrt(l2);
				}
				if (!NkFini(v.x) || !NkFini(v.y) || !NkFini(p.pos.x) || !NkFini(p.pos.y)) {
					// Garde-fou : une particule empoisonnee contaminerait ses
					// voisines en un sous-pas. On la ramene a son dernier etat sain.
					p.pos = NkFini(p.prec.x) && NkFini(p.prec.y) ? p.prec : NkV2(0.f, 100.f);
					v = NkV2();
				}
				p.vit = v;
			}
		}

		void NkMonde::Viscosite() noexcept {
			const float32 hF = mFluide[0].h;
			const float32 hF2 = hF * hF;
			const uint32 n = static_cast<uint32>(particules.Size());
			NkVector<NkV2> &acc = mTmpV2;
			acc.Resize(n);
			NkVector<float32> &poids = mTmpF;
			poids.Resize(n);
			for (uint32 i = 0; i < n; ++i) {
				acc[i] = NkV2();
				poids[i] = 0.f;
			}
			bool fluide = false;
			for (uint32 i = 0; i < corps.Size(); ++i) {
				if (NkEstFluide(corps[i].mat) && corps[i].nombre > 0) {
					fluide = true;
					break;
				}
			}
			if (!fluide) {
				return;
			}
			PourPaires([&](uint32 i, uint32 j) {
				const NkParticule &a = particules[i];
				const NkParticule &b = particules[j];
				if (!mEstFluide[i] || !mEstFluide[j]) {
					return;
				}
				const float32 r2 = NkLen2(b.pos - a.pos);
				if (r2 >= hF2) {
					return;
				}
				const float32 q = 1.f - std::sqrt(r2) / hF;
				const NkV2 dv = b.vit - a.vit;
				acc[i] += dv * q;
				acc[j] -= dv * q;
				poids[i] += q;
				poids[j] += q;
			});
			for (uint32 i = 0; i < n; ++i) {
				if (poids[i] <= 0.f) {
					continue;
				}
				NkParticule &p = particules[i];
				if (p.invMasse == 0.f) {
					continue;
				}
				const float32 c = NkClampf(corps[p.corps].viscosite, 0.f, 0.5f);
				// Moyenne ponderee : jamais plus que "prendre la vitesse du
				// voisinage", donc stable quelle que soit la viscosite choisie.
				p.vit += acc[i] * (c / NkMaxf(poids[i], 1.f));
			}
		}

		void NkMonde::Souder() noexcept {
			if (!reglages.soudure || !mGrilleValide || mTri.Size() != particules.Size()) {
				return;
			}
			PourPaires([&](uint32 i, uint32 j) {
				NkParticule &a = particules[i];
				NkParticule &b = particules[j];
				const NkCorps &ca = corps[a.corps];
				const NkCorps &cb = corps[b.corps];
				if (ca.mat != cb.mat || !NkEstSoudable(ca.mat)) {
					return;
				}
				if (a.nbVoisins >= 6 || b.nbVoisins >= 6) {
					return;
				}
				const float32 s = 0.5f * (ca.espacement + cb.espacement);
				const float32 r2 = NkLen2(b.pos - a.pos);
				if (ca.mat == NkMateriau::NK_ATOMES) {
					// Un atome ne se lie qu'a un voisin PROCHE et LENT : c'est ce
					// qui fait recristalliser un metal refroidi, et pas un gaz.
					if (r2 > (1.08f * s) * (1.08f * s) || NkLen2(a.vit - b.vit) > 70.f * 70.f) {
						return;
					}
					if (Relier(i, j, s)) {
						stats.soudures++;
					}
				} else {
					if (r2 > (1.2f * s) * (1.2f * s)) {
						return;
					}
					const float32 r = std::sqrt(r2);
					if (Relier(i, j, NkClampf(r, 0.9f * s, 1.1f * s))) {
						stats.soudures++;
					}
				}
			});
		}

		void NkMonde::PurgerLiens() noexcept {
			uint32 w = 0;
			for (uint32 i = 0; i < liens.Size(); ++i) {
				const NkLien &l = liens[i];
				if (l.casse && !NkEstGrille(corps[l.corps].mat)) {
					continue;
				}
				liens[w++] = l;
			}
			liens.Resize(w);
			mCasseesPurgeables = 0;
			RecalculerPlages();
		}

		void NkMonde::Compacter(const NkVector<uint8> &garder) noexcept {
			const uint32 n = static_cast<uint32>(particules.Size());
			NkVector<uint32> remap;
			remap.Resize(n);
			uint32 w = 0;
			for (uint32 i = 0; i < n; ++i) {
				remap[i] = garder[i] ? w++ : NK_PHYS_AUCUN;
			}
			// Particules (l'ordre est conserve : les corps restent contigus).
			for (uint32 i = 0; i < n; ++i) {
				if (remap[i] == NK_PHYS_AUCUN) {
					continue;
				}
				NkParticule p = particules[i];
				uint8 nv = 0;
				for (uint8 k = 0; k < p.nbVoisins; ++k) {
					const uint32 v = remap[p.voisins[k]];
					if (v != NK_PHYS_AUCUN) {
						p.voisins[nv++] = v;
					}
				}
				p.nbVoisins = nv;
				particules[remap[i]] = p;
			}
			particules.Resize(w);

			// Liens.
			uint32 wl = 0;
			for (uint32 i = 0; i < liens.Size(); ++i) {
				NkLien l = liens[i];
				if (remap[l.a] == NK_PHYS_AUCUN || remap[l.b] == NK_PHYS_AUCUN) {
					continue;
				}
				l.a = remap[l.a];
				l.b = remap[l.b];
				liens[wl++] = l;
			}
			liens.Resize(wl);

			// Corps devenus vides.
			NkVector<uint32> compte;
			compte.Resize(corps.Size());
			for (uint32 c = 0; c < corps.Size(); ++c) {
				compte[c] = 0;
			}
			for (uint32 i = 0; i < particules.Size(); ++i) {
				compte[particules[i].corps]++;
			}
			NkVector<uint32> remapC;
			remapC.Resize(corps.Size());
			uint32 wc = 0;
			for (uint32 c = 0; c < corps.Size(); ++c) {
				if (compte[c] == 0) {
					remapC[c] = NK_PHYS_AUCUN;
					continue;
				}
				remapC[c] = wc;
				corps[wc++] = corps[c];
			}
			corps.Resize(wc);
			for (uint32 i = 0; i < particules.Size(); ++i) {
				particules[i].corps = remapC[particules[i].corps];
			}
			uint32 wl2 = 0;
			for (uint32 i = 0; i < liens.Size(); ++i) {
				NkLien l = liens[i];
				if (remapC[l.corps] == NK_PHYS_AUCUN) {
					l.corps = particules[l.a].corps;
				} else {
					l.corps = remapC[l.corps];
				}
				liens[wl2++] = l;
			}
			liens.Resize(wl2);

			// Saisie : les index ont bouge.
			SaisirFin();
			mGrilleValide = false;
			RecalculerPlages();
			// Un ballon ampute n'est plus un anneau : il se degonfle.
			for (uint32 c = 0; c < corps.Size(); ++c) {
				if (corps[c].mat == NkMateriau::NK_BALLON && corps[c].nombre != static_cast<uint32>(corps[c].nx)) {
					corps[c].pression = 0.f;
				}
			}
		}

		// =====================================================================
		// Requetes
		// =====================================================================
		int32 NkMonde::ParticuleProche(const NkV2 &p, float32 rayonMax) const noexcept {
			int32 meilleur = -1;
			float32 best = rayonMax * rayonMax;
			for (uint32 i = 0; i < particules.Size(); ++i) {
				if (!corps[particules[i].corps].visible) {
					continue;
				}
				const float32 r = particules[i].rayon;
				const float32 d2 = NkLen2(particules[i].pos - p) - r * r;
				if (d2 < best) {
					best = d2;
					meilleur = static_cast<int32>(i);
				}
			}
			return meilleur;
		}

		int32 NkMonde::IndexCorps(uint32 id) const noexcept {
			if (id == 0) {
				return -1;
			}
			for (uint32 c = 0; c < corps.Size(); ++c) {
				if (corps[c].id == id) {
					return static_cast<int32>(c);
				}
			}
			return -1;
		}

		NkV2 NkMonde::CentreCorps(uint32 ci) const noexcept {
			const NkCorps &c = corps[ci];
			NkV2 s;
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				s += particules[i].pos;
			}
			return c.nombre > 0 ? s / static_cast<float32>(c.nombre) : s;
		}

		void NkMonde::BoiteCorps(uint32 ci, NkV2 &mn, NkV2 &mx) const noexcept {
			const NkCorps &c = corps[ci];
			mn = NkV2(1.0e9f, 1.0e9f);
			mx = NkV2(-1.0e9f, -1.0e9f);
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				const NkParticule &p = particules[i];
				mn.x = NkMinf(mn.x, p.pos.x - p.rayon);
				mn.y = NkMinf(mn.y, p.pos.y - p.rayon);
				mx.x = NkMaxf(mx.x, p.pos.x + p.rayon);
				mx.y = NkMaxf(mx.y, p.pos.y + p.rayon);
			}
		}

		uint32 NkMonde::LiensActifs() const noexcept {
			uint32 n = 0;
			for (uint32 i = 0; i < liens.Size(); ++i) {
				n += liens[i].casse ? 0u : 1u;
			}
			return n;
		}

		// =====================================================================
		// Editions
		// =====================================================================
		void NkMonde::SupprimerCorps(uint32 ci) noexcept {
			if (ci >= corps.Size()) {
				return;
			}
			NkVector<uint8> garder;
			garder.Resize(particules.Size());
			for (uint32 i = 0; i < particules.Size(); ++i) {
				garder[i] = particules[i].corps == ci ? 0u : 1u;
			}
			Compacter(garder);
		}

		uint32 NkMonde::Gommer(const NkV2 &p, float32 rayon) noexcept {
			uint32 n = 0;
			// Obstacles.
			for (uint32 k = 0; k < obstacles.Size();) {
				const NkObstacle &o = obstacles[k];
				const NkV2 ab = o.b - o.a;
				const float32 l2 = NkLen2(ab);
				const float32 t = l2 > 1.0e-6f ? NkClampf(NkDot(p - o.a, ab) / l2, 0.f, 1.f) : 0.f;
				if (NkLen(p - (o.a + ab * t)) < rayon + o.rayon) {
					obstacles.Erase(obstacles.Begin() + k);
					++n;
				} else {
					++k;
				}
			}
			// Particules.
			NkVector<uint8> garder;
			garder.Resize(particules.Size());
			NkVector<uint8> corpsEntier;
			corpsEntier.Resize(corps.Size());
			for (uint32 c = 0; c < corps.Size(); ++c) {
				corpsEntier[c] = 0;
			}
			bool touche = false;
			for (uint32 i = 0; i < particules.Size(); ++i) {
				garder[i] = 1;
				const NkParticule &q = particules[i];
				if (NkLen2(q.pos - p) > (rayon + q.rayon) * (rayon + q.rayon)) {
					continue;
				}
				const NkMateriau m = corps[q.corps].mat;
				if (NkEstFluide(m) || m == NkMateriau::NK_SABLE || m == NkMateriau::NK_ATOMES) {
					garder[i] = 0;
				} else {
					corpsEntier[q.corps] = 1;
				}
				touche = true;
			}
			if (!touche) {
				return n;
			}
			for (uint32 i = 0; i < particules.Size(); ++i) {
				if (corpsEntier[particules[i].corps]) {
					garder[i] = 0;
				}
				n += garder[i] ? 0u : 1u;
			}
			Compacter(garder);
			return n;
		}

		static bool NkSegmentsSeCroisent(const NkV2 &p1, const NkV2 &p2, const NkV2 &q1, const NkV2 &q2) noexcept {
			const NkV2 r = p2 - p1;
			const NkV2 s = q2 - q1;
			const float32 den = NkCross(r, s);
			if (den > -1.0e-9f && den < 1.0e-9f) {
				return false;
			}
			const float32 t = NkCross(q1 - p1, s) / den;
			const float32 u = NkCross(q1 - p1, r) / den;
			return t >= 0.f && t <= 1.f && u >= 0.f && u <= 1.f;
		}

		uint32 NkMonde::Couper(const NkV2 &a, const NkV2 &b) noexcept {
			uint32 n = 0;
			for (uint32 i = 0; i < liens.Size(); ++i) {
				NkLien &l = liens[i];
				if (l.casse) {
					continue;
				}
				if (!NkSegmentsSeCroisent(particules[l.a].pos, particules[l.b].pos, a, b)) {
					continue;
				}
				l.casse = true;
				++n;
				NkCorps &c = corps[l.corps];
				if (l.genre == NkGenreLien::NK_LIAISON) {
					RetirerVoisin(l.a, l.b);
				}
				if (!NkEstGrille(c.mat)) {
					mCasseesPurgeables++;
				}
				if (c.mat == NkMateriau::NK_BALLON) {
					c.pression = 0.f;
				}
			}
			rupturesTotal += n;
			return n;
		}

		void NkMonde::Explosion(const NkV2 &c, float32 rayon, float32 vitesse) noexcept {
			for (uint32 i = 0; i < particules.Size(); ++i) {
				NkParticule &p = particules[i];
				if (p.invMasse == 0.f) {
					continue;
				}
				const NkV2 d = p.pos - c;
				const float32 l = NkLen(d);
				if (l >= rayon) {
					continue;
				}
				const NkV2 dir = l > 1.0e-4f ? d / l : NkV2(0.f, 1.f);
				const float32 att = 1.f - l / rayon;
				p.vit += dir * (vitesse * att * att);
			}
		}

		void NkMonde::Aimant(const NkV2 &c, float32 rayon, float32 acceleration, float32 dt) noexcept {
			for (uint32 i = 0; i < particules.Size(); ++i) {
				NkParticule &p = particules[i];
				if (p.invMasse == 0.f) {
					continue;
				}
				const NkV2 d = c - p.pos;
				const float32 l = NkLen(d);
				if (l >= rayon || l < 1.0e-3f) {
					continue;
				}
				const float32 att = 1.f - l / rayon;
				p.vit += d / l * (acceleration * att * dt);
				// Un aimant qui ne freine pas fait osciller la matiere a l'infini
				// autour de son centre : on dissipe un peu, pres du coeur.
				p.vit *= 1.f - 2.5f * att * dt;
			}
		}

		void NkMonde::Translater(uint32 ci, const NkV2 &delta) noexcept {
			const NkCorps &c = corps[ci];
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				particules[i].pos += delta;
				particules[i].prec += delta;
			}
		}

		void NkMonde::AppliquerVitesse(uint32 ci, const NkV2 &v) noexcept {
			const NkCorps &c = corps[ci];
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				if (particules[i].invMasse > 0.f) {
					particules[i].vit = v;
				}
			}
		}

		void NkMonde::BasculerEpingle(uint32 i) noexcept {
			if (i >= particules.Size()) {
				return;
			}
			NkParticule &p = particules[i];
			p.epingle = !p.epingle;
			p.invMasse = p.epingle ? 0.f : (p.masse > 0.f ? 1.f / p.masse : 0.f);
			p.vit = NkV2();
		}

		void NkMonde::EpinglerCorps(uint32 ci, bool epingle) noexcept {
			const NkCorps &c = corps[ci];
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				NkParticule &p = particules[i];
				p.epingle = epingle;
				p.invMasse = epingle ? 0.f : (p.masse > 0.f ? 1.f / p.masse : 0.f);
				p.vit = NkV2();
			}
		}

		void NkMonde::AppliquerMasse(uint32 ci) noexcept {
			const NkCorps &c = corps[ci];
			const float32 m = NkMaxf(c.masseParticule, 0.01f);
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				NkParticule &p = particules[i];
				p.masse = m;
				p.invMasse = p.epingle ? 0.f : 1.f / m;
			}
		}

		bool NkMonde::SaisirDebut(const NkV2 &p, float32 rayon) noexcept {
			SaisirFin();
			for (uint32 i = 0; i < particules.Size(); ++i) {
				if (!corps[particules[i].corps].visible) {
					continue;
				}
				if (NkLen2(particules[i].pos - p) <= rayon * rayon) {
					mSaisis.PushBack(i);
				}
			}
			if (mSaisis.Size() == 0) {
				const int32 k = ParticuleProche(p, rayon * 2.5f);
				if (k < 0) {
					return false;
				}
				mSaisis.PushBack(static_cast<uint32>(k));
			}
			mSaisisDecalage.Resize(mSaisis.Size());
			for (uint32 k = 0; k < mSaisis.Size(); ++k) {
				NkParticule &q = particules[mSaisis[k]];
				// Le decalage est RESSERRE : on tient une poignee de matiere,
				// pas un moule rigide. Un fluide se laisse ainsi etirer.
				mSaisisDecalage[k] = (q.pos - p) * 0.6f;
				q.saisie = true;
			}
			mCibleSaisie = p;
			return true;
		}

		void NkMonde::SaisirVers(const NkV2 &p) noexcept {
			mCibleSaisie = p;
		}

		void NkMonde::SaisirFin() noexcept {
			for (uint32 k = 0; k < mSaisis.Size(); ++k) {
				if (mSaisis[k] < particules.Size()) {
					particules[mSaisis[k]].saisie = false;
				}
			}
			mSaisis.Clear();
			mSaisisDecalage.Clear();
		}

	} // namespace physic2d
} // namespace nkentseu
