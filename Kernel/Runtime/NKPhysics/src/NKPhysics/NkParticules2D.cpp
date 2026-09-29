// AUTEUR : Rihen
// =============================================================================
// NkParticules2D.cpp — le solveur (voir l'en-tete pour la methode et l'ordre)
//
// REGLAGES MESURES, PAS DEVINES (banc de Physic2D, 2026-09-29, avant le portage
// en metres — les grandeurs sans dimension sont inchangees) :
//   fluide    espacement 0,10 m, h = 2,2 s, rho0 calcule sur le reseau ideal ;
//             kPression 0,24 et kProche 0,07 : densite interieure 0,94 rho0
//             au repos (kProche 0,14 donnait 0,84 : l'eau "gonflait").
//   cohesion  n'agit que DANS la matiere (densite > 0,4 rho0) : sinon l'eau
//             tissait des fils qui pendaient dans le vide.
//   blob      Maxwell : plasticite 7/s -> s'affaisse en dome en ~5 s ; a 0,5/s
//             il restait une boule elastique 20 s (hauteur 79 -> 75 cm).
//   chaleur   agitation T x 0,7 sqrt(h) m/s : a T = 100 un cristal rompt
//             165 liaisons en 10 s et en refait (il est "fondu").
// =============================================================================
#include "NKPhysics/NkParticules2D.h"
#include "NKPhysics/NkPhysicsWorld.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {
	namespace physics {

		namespace {
			NK_FORCE_INLINE float32 Dot(const NkVec2f &a, const NkVec2f &b) noexcept {
				return a.x * b.x + a.y * b.y;
			}
			NK_FORCE_INLINE float32 Cross(const NkVec2f &a, const NkVec2f &b) noexcept {
				return a.x * b.y - a.y * b.x;
			}
			NK_FORCE_INLINE float32 Len2(const NkVec2f &v) noexcept {
				return v.x * v.x + v.y * v.y;
			}
			NK_FORCE_INLINE float32 Len(const NkVec2f &v) noexcept {
				return math::NkSqrt(v.x * v.x + v.y * v.y);
			}
			NK_FORCE_INLINE NkVec2f Perp(const NkVec2f &v) noexcept {
				return NkVec2f(-v.y, v.x);
			}
			NK_FORCE_INLINE NkVec2f Rot(const NkVec2f &v, float32 c, float32 s) noexcept {
				return NkVec2f(v.x * c - v.y * s, v.x * s + v.y * c);
			}
			NK_FORCE_INLINE float32 Clampf(float32 v, float32 a, float32 b) noexcept {
				return v < a ? a : (v > b ? b : v);
			}
			NK_FORCE_INLINE float32 Minf(float32 a, float32 b) noexcept {
				return a < b ? a : b;
			}
			NK_FORCE_INLINE float32 Maxf(float32 a, float32 b) noexcept {
				return a > b ? a : b;
			}
			/// Un NaN ne se compare egal a rien : le test ne depend pas d'une
			/// option de compilation (-ffast-math casse isnan).
			NK_FORCE_INLINE bool Fini(float32 v) noexcept {
				return v == v && v < 1.0e30f && v > -1.0e30f;
			}

			template <typename F>
			void PourPairesGrille(const NkVector<NkParticule2D> &ps, const NkVector<int32> &debut, const NkVector<int32> &fin,
								  const NkVector<uint32> &tri, int32 gx, int32 gy, const NkVec2f &origine, float32 cell,
								  F &&f) {
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

			bool SegmentsSeCroisent(const NkVec2f &p1, const NkVec2f &p2, const NkVec2f &q1, const NkVec2f &q2) noexcept {
				const NkVec2f r = p2 - p1;
				const NkVec2f s = q2 - q1;
				const float32 den = Cross(r, s);
				if (den > -1.0e-12f && den < 1.0e-12f) {
					return false;
				}
				const float32 t = Cross(q1 - p1, s) / den;
				const float32 u = Cross(q1 - p1, r) / den;
				return t >= 0.f && t <= 1.f && u >= 0.f && u <= 1.f;
			}
		} // namespace

		// =====================================================================
		// Materiaux
		// =====================================================================
		const char *NkMateriauP2DNom(NkMateriauP2D m) noexcept {
			switch (m) {
				case NkMateriauP2D::NK_BALLON:
					return "Ballon";
				case NkMateriauP2D::NK_BLOB:
					return "Blob";
				case NkMateriauP2D::NK_GELEE:
					return "Gelée";
				case NkMateriauP2D::NK_EAU:
					return "Eau";
				case NkMateriauP2D::NK_MIEL:
					return "Miel";
				case NkMateriauP2D::NK_SABLE:
					return "Sable";
				case NkMateriauP2D::NK_ATOMES:
					return "Atomes";
				case NkMateriauP2D::NK_TISSU:
					return "Tissu";
				case NkMateriauP2D::NK_CORDE:
					return "Corde";
				default:
					return "?";
			}
		}

		bool NkEstFluideP2D(NkMateriauP2D m) noexcept {
			return m == NkMateriauP2D::NK_EAU || m == NkMateriauP2D::NK_MIEL || m == NkMateriauP2D::NK_BLOB;
		}
		bool NkEstGrilleP2D(NkMateriauP2D m) noexcept {
			return m == NkMateriauP2D::NK_GELEE || m == NkMateriauP2D::NK_TISSU;
		}
		bool NkEstSoudableP2D(NkMateriauP2D m) noexcept {
			return m == NkMateriauP2D::NK_ATOMES || m == NkMateriauP2D::NK_BLOB;
		}

		float32 NkComplianceP2D(float32 raideur) noexcept {
			// A h ~ 1/480 s : a 0,5 le lien rattrape ~ un tiers de son ecart par
			// sous-pas, a 0,9 presque tout, a 1 tout. En s^2/kg : l'unite de
			// longueur n'y entre pas.
			const float32 s = 1.f - Clampf(raideur, 0.f, 1.f);
			return 2.0e-5f * s * s * s + 1.0e-10f;
		}

		float32 NkDensiteReseauP2D(float32 s, float32 h) noexcept {
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
					const float32 d = math::NkSqrt(x * x + y * y);
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
		NkParticules2D::NkParticules2D() {
			const float32 s = 0.10f;
			const float32 h = 2.2f * s;
			const float32 rho0 = NkDensiteReseauP2D(s, h);
			for (int32 i = 0; i < static_cast<int32>(NkMateriauP2D::NK_COUNT); ++i) {
				mFluide[i].h = h;
				mFluide[i].rho0 = rho0;
				mFluide[i].kPression = 0.20f;
				mFluide[i].kProche = 0.12f;
			}
			mFluide[static_cast<int32>(NkMateriauP2D::NK_EAU)].kPression = 0.24f;
			mFluide[static_cast<int32>(NkMateriauP2D::NK_EAU)].kProche = 0.07f;
			mFluide[static_cast<int32>(NkMateriauP2D::NK_MIEL)].kPression = 0.22f;
			mFluide[static_cast<int32>(NkMateriauP2D::NK_BLOB)].kPression = 0.18f;
		}

		float32 NkParticules2D::Alea() noexcept {
			uint32 x = mAlea;
			x ^= x << 13;
			x ^= x >> 17;
			x ^= x << 5;
			mAlea = x;
			return static_cast<float32>(x >> 8) * (2.f / 16777216.f) - 1.f;
		}

		void NkParticules2D::Vider() noexcept {
			particules.Clear();
			liens.Clear();
			corps.Clear();
			mSaisis.Clear();
			mSaisisDecalage.Clear();
			mRigides.Clear();
			rupturesTotal = 0;
			mCasseesPurgeables = 0;
			mGrilleValide = false;
			stats = NkStatsP2D{};
		}

		NkParamsFluide2D NkParticules2D::ParamsFluide(NkMateriauP2D m) const noexcept {
			return mFluide[static_cast<int32>(m)];
		}

		uint32 NkParticules2D::NouveauCorps(NkMateriauP2D mat) noexcept {
			NkCorpsP2D c;
			c.mat = mat;
			c.id = prochainId++;
			c.debut = static_cast<uint32>(particules.Size());
			c.nombre = 0;
			corps.PushBack(c);
			return static_cast<uint32>(corps.Size() - 1u);
		}

		uint32 NkParticules2D::AjouterParticule(uint32 ci, const NkVec2f &pos, float32 rayon) noexcept {
			NkParticule2D p;
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

		uint32 NkParticules2D::AjouterLien(uint32 a, uint32 b, NkGenreLien2D genre) noexcept {
			NkLien2D l;
			l.a = a;
			l.b = b;
			l.corps = particules[a].corps;
			l.repos = Len(particules[b].pos - particules[a].pos);
			l.reposInitial = l.repos;
			l.genre = genre;
			liens.PushBack(l);
			return static_cast<uint32>(liens.Size() - 1u);
		}

		bool NkParticules2D::Relier(uint32 a, uint32 b, float32 repos) noexcept {
			if (a == b) {
				return false;
			}
			NkParticule2D &pa = particules[a];
			NkParticule2D &pb = particules[b];
			if (pa.nbVoisins >= NK_P2D_MAX_VOISINS || pb.nbVoisins >= NK_P2D_MAX_VOISINS) {
				return false;
			}
			for (uint8 k = 0; k < pa.nbVoisins; ++k) {
				if (pa.voisins[k] == b) {
					return false;
				}
			}
			pa.voisins[pa.nbVoisins++] = b;
			pb.voisins[pb.nbVoisins++] = a;
			NkLien2D l;
			l.a = a;
			l.b = b;
			l.corps = pa.corps;
			l.repos = repos;
			l.reposInitial = repos;
			l.genre = NkGenreLien2D::NK_LIAISON;
			liens.PushBack(l);
			return true;
		}

		void NkParticules2D::RetirerVoisin(uint32 a, uint32 b) noexcept {
			NkParticule2D &pa = particules[a];
			for (uint8 k = 0; k < pa.nbVoisins; ++k) {
				if (pa.voisins[k] == b) {
					pa.voisins[k] = pa.voisins[--pa.nbVoisins];
					break;
				}
			}
			NkParticule2D &pb = particules[b];
			for (uint8 k = 0; k < pb.nbVoisins; ++k) {
				if (pb.voisins[k] == a) {
					pb.voisins[k] = pb.voisins[--pb.nbVoisins];
					break;
				}
			}
		}

		void NkParticules2D::FinaliserCorps(uint32 ci) noexcept {
			NkCorpsP2D &c = corps[ci];
			if (c.nombre == 0) {
				return;
			}
			NkVec2f centre(0.f, 0.f);
			float32 m = 0.f;
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				centre += particules[i].pos * particules[i].masse;
				m += particules[i].masse;
			}
			centre = centre / (m > 0.f ? m : 1.f);
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				particules[i].repos = particules[i].pos - centre;
			}
			if (c.mat == NkMateriauP2D::NK_BALLON) {
				float32 aire = 0.f;
				for (uint32 k = 0; k < c.nombre; ++k) {
					aire += Cross(particules[c.debut + k].pos, particules[c.debut + (k + 1) % c.nombre].pos);
				}
				c.aireRepos = aire * 0.5f;
			}
			RecalculerPlages();
		}

		void NkParticules2D::RecalculerPlages() noexcept {
			const uint32 nc = static_cast<uint32>(corps.Size());
			for (uint32 c = 0; c < nc; ++c) {
				corps[c].debut = NK_P2D_AUCUN;
				corps[c].nombre = 0;
				corps[c].lienDebut = NK_P2D_AUCUN;
				corps[c].lienNombre = 0;
			}
			for (uint32 i = 0; i < particules.Size(); ++i) {
				NkCorpsP2D &c = corps[particules[i].corps];
				if (c.debut == NK_P2D_AUCUN) {
					c.debut = i;
				}
				c.nombre++;
			}
			for (uint32 i = 0; i < liens.Size(); ++i) {
				const NkLien2D &l = liens[i];
				if (l.genre == NkGenreLien2D::NK_LIAISON) {
					continue;
				}
				NkCorpsP2D &c = corps[l.corps];
				if (c.lienDebut == NK_P2D_AUCUN) {
					c.lienDebut = i;
				}
				c.lienNombre++;
			}
			for (uint32 c = 0; c < nc; ++c) {
				if (corps[c].debut == NK_P2D_AUCUN) {
					corps[c].debut = static_cast<uint32>(particules.Size());
				}
				if (corps[c].lienDebut == NK_P2D_AUCUN) {
					corps[c].lienDebut = 0;
				}
			}
		}

		// =====================================================================
		// Grille spatiale : bornee par les PARTICULES (un monde de jeu n'a pas
		// de boite), tri par comptage, reconstruite a chaque sous-pas.
		// =====================================================================
		void NkParticules2D::ConstruireGrille() noexcept {
			const uint32 n = static_cast<uint32>(particules.Size());
			float32 rmax = 0.04f;
			float32 mnx = 1.0e30f, mny = 1.0e30f, mxx = -1.0e30f, mxy = -1.0e30f;
			for (uint32 i = 0; i < n; ++i) {
				const NkParticule2D &p = particules[i];
				rmax = Maxf(rmax, p.rayon);
				mnx = Minf(mnx, p.pos.x);
				mny = Minf(mny, p.pos.y);
				mxx = Maxf(mxx, p.pos.x);
				mxy = Maxf(mxy, p.pos.y);
			}
			// La cellule couvre a la fois un contact (2 rayons) et le rayon
			// d'interaction des fluides : 3 x 3 cellules suffisent toujours.
			mCell = Maxf(2.f * rmax + 0.01f, mFluide[0].h);
			// Un monde tres etendu grossit la cellule plutot que la memoire.
			while ((mxx - mnx) / mCell > 1000.f || (mxy - mny) / mCell > 1000.f) {
				mCell *= 2.f;
			}
			mOrigine = NkVec2f(mnx - mCell, mny - mCell);
			mGx = static_cast<int32>((mxx - mnx) / mCell) + 3;
			mGy = static_cast<int32>((mxy - mny) / mCell) + 3;
			const uint32 nCell = static_cast<uint32>(mGx * mGy);
			if (mCellDebut.Size() < nCell) {
				mCellDebut.Resize(nCell);
				mCellFin.Resize(nCell);
			}
			for (uint32 c = 0; c < nCell; ++c) {
				mCellDebut[c] = 0;
				mCellFin[c] = 0;
			}
			mTri.Resize(n);
			mCleTri.Resize(n);
			for (uint32 i = 0; i < n; ++i) {
				int32 cx = static_cast<int32>((particules[i].pos.x - mOrigine.x) / mCell);
				int32 cy = static_cast<int32>((particules[i].pos.y - mOrigine.y) / mCell);
				cx = cx < 0 ? 0 : (cx >= mGx ? mGx - 1 : cx);
				cy = cy < 0 ? 0 : (cy >= mGy ? mGy - 1 : cy);
				const uint32 cle = static_cast<uint32>(cy * mGx + cx);
				mCleTri[i] = cle;
				mCellFin[cle]++;
			}
			int32 somme = 0;
			for (uint32 c = 0; c < nCell; ++c) {
				const int32 k = mCellFin[c];
				mCellDebut[c] = somme;
				mCellFin[c] = somme; // curseur d'ecriture
				somme += k;
			}
			for (uint32 i = 0; i < n; ++i) {
				mTri[static_cast<uint32>(mCellFin[mCleTri[i]]++)] = i;
			}
			mGrilleValide = true;
			ConstruirePaires();
		}

		// La liste des paires PROCHES est construite une fois par sous-pas et
		// relue par toutes les passes : parcourir les cellules trois fois coutait
		// plus que les calculs eux-memes (mesure : 9,7 -> 5,4 ms, 1 315 particules).
		void NkParticules2D::ConstruirePaires() noexcept {
			const uint32 n = static_cast<uint32>(particules.Size());
			mEstFluide.Resize(n);
			for (uint32 i = 0; i < n; ++i) {
				mEstFluide[i] = NkEstFluideP2D(corps[particules[i].corps].mat) ? 1u : 0u;
			}
			mPaires.Clear();
			const float32 hF = mFluide[0].h;
			const NkParticule2D *ps = particules.Data();
			const uint8 *fl = mEstFluide.Data();
			PourPairesGrille(particules, mCellDebut, mCellFin, mTri, mGx, mGy, mOrigine, mCell, [&](uint32 i, uint32 j) {
				const NkParticule2D &a = ps[i];
				const NkParticule2D &b = ps[j];
				if (a.invMasse + b.invMasse <= 0.f) {
					return;
				}
				const bool ff = fl[i] && fl[j];
				if (!ff && a.corps == b.corps && !corps[a.corps].autoCollision) {
					return;
				}
				const float32 portee = ff ? hF * 1.1f : (a.rayon + b.rayon) * 1.25f;
				if (Len2(b.pos - a.pos) >= portee * portee) {
					return;
				}
				mPaires.PushBack(i);
				mPaires.PushBack(j);
			});
		}

		template <typename F> void NkParticules2D::PourPaires(F &&f) noexcept {
			const uint32 *pr = mPaires.Data();
			const uint32 np = static_cast<uint32>(mPaires.Size());
			for (uint32 k = 0; k < np; k += 2) {
				f(pr[k], pr[k + 1]);
			}
		}

		template <typename F>
		void NkParticules2D::PourCellules(float32 mnx, float32 mny, float32 mxx, float32 mxy, F &&f) noexcept {
			if (!mGrilleValide || mGx <= 0) {
				return;
			}
			int32 x0 = static_cast<int32>((mnx - mOrigine.x) / mCell);
			int32 y0 = static_cast<int32>((mny - mOrigine.y) / mCell);
			int32 x1 = static_cast<int32>((mxx - mOrigine.x) / mCell);
			int32 y1 = static_cast<int32>((mxy - mOrigine.y) / mCell);
			if (x1 < 0 || y1 < 0 || x0 >= mGx || y0 >= mGy) {
				return;
			}
			x0 = x0 < 0 ? 0 : x0;
			y0 = y0 < 0 ? 0 : y0;
			x1 = x1 >= mGx ? mGx - 1 : x1;
			y1 = y1 >= mGy ? mGy - 1 : y1;
			for (int32 y = y0; y <= y1; ++y) {
				for (int32 x = x0; x <= x1; ++x) {
					const int32 c = y * mGx + x;
					for (int32 k = mCellDebut[c]; k < mCellFin[c]; ++k) {
						f(mTri[static_cast<uint32>(k)]);
					}
				}
			}
		}

		// =====================================================================
		// Pas
		// =====================================================================
		void NkParticules2D::Pas(float32 dt, NkPhysicsWorld *rigides) noexcept {
			stats.ruptures = 0;
			stats.contacts = 0;
			stats.contactsRigides = 0;
			stats.soudures = 0;
			const int32 n = reglages.sousPas < 1 ? 1 : (reglages.sousPas > 40 ? 40 : reglages.sousPas);
			const float32 dtEff = dt * reglages.echelleTemps;
			const float32 h = dtEff / static_cast<float32>(n);
			if (h <= 0.f || particules.Size() == 0) {
				stats.energieCinetique = 0.f;
				return;
			}
			PreparerRigides(rigides);
			for (int32 k = 0; k < n; ++k) {
				SousPas(h, k);
			}
			Souder();
			if (mCasseesPurgeables > 512) {
				PurgerLiens();
			}

			// Ce qui est tombe hors du monde disparait.
			const NkLimites2D &L = reglages.limites;
			const float32 marge = L.actif ? (L.droite - L.gauche) * 0.5f : 0.f;
			auto dehors = [&](const NkVec2f &p) {
				if (!L.actif) {
					return p.y < -1000.f;
				}
				return p.y < L.bas - 6.f || p.x < L.gauche - marge || p.x > L.droite + marge;
			};
			bool horsMonde = false;
			for (uint32 i = 0; i < particules.Size() && !horsMonde; ++i) {
				horsMonde = dehors(particules[i].pos);
			}
			if (horsMonde) {
				NkVector<uint8> garder;
				garder.Resize(particules.Size());
				for (uint32 i = 0; i < particules.Size(); ++i) {
					garder[i] = dehors(particules[i].pos) ? 0u : 1u;
				}
				Compacter(garder);
			}

			float32 e = 0.f;
			for (uint32 i = 0; i < particules.Size(); ++i) {
				e += 0.5f * particules[i].masse * Len2(particules[i].vit);
			}
			stats.energieCinetique = e;
		}

		void NkParticules2D::SousPas(float32 h, int32 k) noexcept {
			Predire(h);
			ConstruireGrille();
			ResoudreSaisie();
			ResoudreLiens(h);
			ResoudrePression(h);
			ResoudreForme();
			ResoudreParticules();
			ResoudreRigides(h, h * static_cast<float32>(k + 1));
			ResoudreLimites();
			MajVitesses(h);
			Viscosite();
			AmortirLiens();
		}

		void NkParticules2D::Predire(float32 h) noexcept {
			const float32 amort = Clampf(1.f - reglages.amortAir * h, 0.f, 1.f);
			const float32 agitation = reglages.temperature * 0.7f * math::NkSqrt(h);
			for (uint32 i = 0; i < particules.Size(); ++i) {
				NkParticule2D &p = particules[i];
				p.prec = p.pos;
				if (p.epingle || p.invMasse == 0.f) {
					p.vit = NkVec2f(0.f, 0.f);
					continue;
				}
				const NkMateriauP2D mat = corps[p.corps].mat;
				NkVec2f a = reglages.gravite;
				// Le vent pousse ce qui a de la SURFACE plus que ce qui a du poids.
				const bool voile = mat == NkMateriauP2D::NK_TISSU || mat == NkMateriauP2D::NK_BALLON;
				a.x += reglages.vent * (voile ? 1.5f : 0.35f);
				p.vit += a * h;
				if (agitation > 0.f && (mat == NkMateriauP2D::NK_ATOMES || NkEstFluideP2D(mat))) {
					const float32 f = mat == NkMateriauP2D::NK_ATOMES ? 1.f : 0.5f;
					p.vit += NkVec2f(Alea(), Alea()) * (agitation * f);
				}
				p.vit *= amort;
				p.pos += p.vit * h;
			}
		}

		void NkParticules2D::ResoudreSaisie() noexcept {
			for (uint32 k = 0; k < mSaisis.Size(); ++k) {
				const uint32 i = mSaisis[k];
				if (i >= particules.Size()) {
					continue;
				}
				NkParticule2D &p = particules[i];
				const NkVec2f cible = mCibleSaisie + mSaisisDecalage[k];
				if (p.epingle) {
					p.pos = cible;
					p.prec = cible;
				} else {
					p.pos += (cible - p.pos) * 0.35f;
				}
			}
		}

		void NkParticules2D::ResoudreLiens(float32 h) noexcept {
			const float32 h2 = h * h;
			for (uint32 li = 0; li < liens.Size(); ++li) {
				NkLien2D &l = liens[li];
				if (l.casse) {
					continue;
				}
				NkParticule2D &pa = particules[l.a];
				NkParticule2D &pb = particules[l.b];
				const float32 w = pa.invMasse + pb.invMasse;
				if (w <= 0.f) {
					continue;
				}
				const NkVec2f d = pb.pos - pa.pos;
				const float32 len = Len(d);
				if (len < 1.0e-8f) {
					continue;
				}
				NkCorpsP2D &c = corps[l.corps];
				const float32 etirement = len / (l.repos > 1.0e-6f ? l.repos : 1.0e-6f);
				l.tension = etirement - 1.f;
				// Un ressort PLASTIQUE a une longueur de repos qui derive : sa rupture
				// se juge contre sa longueur de NAISSANCE, sinon il ne romprait jamais
				// et la matiere ne changerait jamais de voisines -- donc ne coulerait pas.
				const bool plastique =
					l.genre == NkGenreLien2D::NK_LIAISON && c.plasticite > 0.f && c.mat == NkMateriauP2D::NK_BLOB;
				const float32 etirementRupture = plastique ? len / Maxf(l.reposInitial, 1.0e-6f) : etirement;
				if (c.resistance > 0.f && etirementRupture > c.resistance) {
					l.casse = true;
					stats.ruptures++;
					rupturesTotal++;
					if (l.genre == NkGenreLien2D::NK_LIAISON) {
						RetirerVoisin(l.a, l.b);
					}
					if (!NkEstGrilleP2D(c.mat)) {
						mCasseesPurgeables++;
					}
					if (c.mat == NkMateriauP2D::NK_BALLON) {
						c.pression = 0.f; // creve : le gaz s'echappe
					}
					continue;
				}
				float32 facteur = 1.f;
				if (l.genre == NkGenreLien2D::NK_CISAILLEMENT) {
					facteur = 4.f;
				} else if (l.genre == NkGenreLien2D::NK_FLEXION) {
					facteur = 20.f;
				}
				const float32 alpha = NkComplianceP2D(c.raideur) * facteur / h2;
				const float32 C = len - l.repos;
				const float32 lambda = -C / (w + alpha);
				const NkVec2f corr = d * (lambda / len);
				pa.pos -= corr * pa.invMasse;
				pb.pos += corr * pb.invMasse;

				// Maxwell : le ressort OUBLIE peu a peu sa longueur au profit de
				// l'actuelle. Rapide, il est elastique (la gelee tremble) ; lent --
				// sous son propre poids -- il coule. Relaxation ~ 1 / plasticite s.
				if (plastique) {
					// Au-dela du SEUIL d'ecoulement (6 % d'ecart), la matiere cede vingt
					// fois plus vite : un choc S'ECRASE au lieu d'etre rendu en rebond.
					// Mesure, blob lache de 2,5 m (remontee du centre apres l'impact) :
					// sans seuil 0,31 -> 0,99 m (une balle) ; 12 % x 8 : 0,63 ; 6 % x 20 :
					// 0,40 (il tremble, comme une gelee) ; 3 % x 20 : 0,32 (il s'etale).
					// L'amortisseur le long des liens, lui, ne changeait presque rien.
					const float32 ecart = (len - l.repos) / Maxf(l.repos, 1.0e-6f);
					const float32 vitesse = (ecart > 0.06f || ecart < -0.06f) ? c.plasticite * 20.f : c.plasticite;
					l.repos += (len - l.repos) * Minf(vitesse * h, 1.f);
					l.repos = Clampf(l.repos, 0.6f * l.reposInitial, 2.2f * l.reposInitial);
				}
			}
		}

		void NkParticules2D::ResoudrePression(float32 h) noexcept {
			for (uint32 ci = 0; ci < corps.Size(); ++ci) {
				NkCorpsP2D &c = corps[ci];
				if (c.mat != NkMateriauP2D::NK_BALLON || c.nombre < 3 || c.pression <= 0.f) {
					continue;
				}
				const uint32 n = c.nombre;
				const uint32 d0 = c.debut;
				float32 aire = 0.f;
				for (uint32 k = 0; k < n; ++k) {
					aire += Cross(particules[d0 + k].pos, particules[d0 + (k + 1) % n].pos);
				}
				aire *= 0.5f;
				const float32 C = aire - c.aireRepos * c.pression;
				// Gradients de l'aire, calcules sur les positions AVANT correction.
				mTmpV2.Resize(n);
				float32 denom = 0.f;
				for (uint32 k = 0; k < n; ++k) {
					const NkVec2f &pp = particules[d0 + (k + n - 1) % n].pos;
					const NkVec2f &ps = particules[d0 + (k + 1) % n].pos;
					mTmpV2[k] = NkVec2f(0.5f * (ps.y - pp.y), 0.5f * (pp.x - ps.x));
					denom += particules[d0 + k].invMasse * Len2(mTmpV2[k]);
				}
				const float32 alpha = NkComplianceP2D(c.raideur) * 0.02f / (h * h);
				if (denom + alpha <= 1.0e-12f) {
					continue;
				}
				const float32 lambda = -C / (denom + alpha);
				for (uint32 k = 0; k < n; ++k) {
					particules[d0 + k].pos += mTmpV2[k] * (lambda * particules[d0 + k].invMasse);
				}
			}
		}

		void NkParticules2D::ResoudreForme() noexcept {
			for (uint32 ci = 0; ci < corps.Size(); ++ci) {
				const NkCorpsP2D &c = corps[ci];
				if (c.formeRaideur <= 0.f || c.nombre < 2) {
					continue;
				}
				NkVec2f centre(0.f, 0.f);
				float32 m = 0.f;
				for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
					centre += particules[i].pos * particules[i].masse;
					m += particules[i].masse;
				}
				centre = centre / (m > 0.f ? m : 1.f);
				float32 sa = 0.f;
				float32 sb = 0.f;
				for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
					const NkVec2f p = particules[i].pos - centre;
					const NkVec2f &q = particules[i].repos;
					sa += particules[i].masse * Dot(q, p);
					sb += particules[i].masse * Cross(q, p);
				}
				const float32 ang = math::NkAtan2(sb, sa);
				const float32 cs = math::NkCos(ang);
				const float32 sn = math::NkSin(ang);
				const float32 k = Clampf(c.formeRaideur, 0.f, 1.f);
				for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
					NkParticule2D &p = particules[i];
					if (p.invMasse == 0.f) {
						continue;
					}
					const NkVec2f but = centre + Rot(p.repos, cs, sn);
					p.pos += (but - p.pos) * k;
				}
			}
		}

		void NkParticules2D::ResoudreParticules() noexcept {
			const float32 hF = mFluide[0].h;
			const float32 hF2 = hF * hF;

			// --- 1. densites des fluides -------------------------------------
			for (uint32 i = 0; i < particules.Size(); ++i) {
				particules[i].densite = 0.f;
				particules[i].densiteProche = 0.f;
			}
			PourPaires([&](uint32 i, uint32 j) {
				if (!mEstFluide[i] || !mEstFluide[j]) {
					return;
				}
				NkParticule2D &a = particules[i];
				NkParticule2D &b = particules[j];
				const float32 r2 = Len2(b.pos - a.pos);
				if (r2 >= hF2) {
					return;
				}
				const float32 q = 1.f - math::NkSqrt(r2) / hF;
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
				NkParticule2D &a = particules[i];
				NkParticule2D &b = particules[j];
				const float32 w = a.invMasse + b.invMasse;
				if (w <= 0.f) {
					return;
				}
				const NkCorpsP2D &ca = corps[a.corps];
				const NkCorpsP2D &cb = corps[b.corps];
				NkVec2f d = b.pos - a.pos;
				float32 r2 = Len2(d);

				if (mEstFluide[i] && mEstFluide[j]) {
					if (r2 >= hF2) {
						return;
					}
					if (r2 < 1.0e-14f) {
						// Deux particules confondues : aucune direction n'existe pour
						// les ecarter, on en tire une au hasard.
						b.pos += NkVec2f(Alea(), Alea()) * 0.005f;
						return;
					}
					const float32 r = math::NkSqrt(r2);
					const float32 q = 1.f - r / hF;
					const NkParamsFluide2D &pa = mFluide[static_cast<int32>(ca.mat)];
					const NkParamsFluide2D &pb = mFluide[static_cast<int32>(cb.mat)];
					float32 Pa = pa.kPression * (a.densite - pa.rho0);
					float32 Pb = pb.kPression * (b.densite - pb.rho0);
					// La cohesion autorise une pression NEGATIVE (tension de surface),
					// mais seulement DANS la matiere : un filet isole n'attire plus.
					const float32 ta = a.densite > 0.40f * pa.rho0 ? ca.cohesion : 0.f;
					const float32 tb = b.densite > 0.40f * pb.rho0 ? cb.cohesion : 0.f;
					Pa = Maxf(Pa, -ta * pa.kPression * pa.rho0 * 0.5f);
					Pb = Maxf(Pb, -tb * pb.kPression * pb.rho0 * 0.5f);
					const float32 Pn = 0.5f * (pa.kProche * a.densiteProche + pb.kProche * b.densiteProche);
					const float32 D = (0.5f * (Pa + Pb) * q + Pn * q * q) * hF * 0.5f;
					const NkVec2f n = d / r;
					a.pos -= n * (D * a.invMasse / w);
					b.pos += n * (D * b.invMasse / w);
					return;
				}
				if (!collisions) {
					return;
				}
				const float32 rr = a.rayon + b.rayon;
				if (r2 >= rr * rr) {
					return;
				}
				if (r2 < 1.0e-14f) {
					d = NkVec2f(0.f, 1.0e-4f);
					r2 = 1.0e-8f;
				}
				const float32 r = math::NkSqrt(r2);
				const NkVec2f n = d / r;
				const float32 prof = rr - r;
				a.pos -= n * (prof * a.invMasse / w);
				b.pos += n * (prof * b.invMasse / w);
				++contacts;

				// Frottement de Coulomb en positions (Macklin 2014) : le glissement
				// relatif est annule tant qu'il reste sous mu * profondeur.
				const float32 mu = 0.5f * (ca.friction + cb.friction) * 1.2f;
				if (mu > 0.f) {
					const NkVec2f rel = (a.pos - a.prec) - (b.pos - b.prec);
					const NkVec2f tang = rel - n * Dot(rel, n);
					const float32 lt = Len(tang);
					if (lt > 1.0e-9f) {
						const float32 k = (lt < mu * prof) ? 1.f : Minf(mu * prof / lt, 1.f);
						a.pos -= tang * (k * a.invMasse / w);
						b.pos += tang * (k * b.invMasse / w);
					}
				}
			});
			stats.contacts += contacts;
		}

		// =====================================================================
		// Couplage avec NkPhysicsWorld
		// =====================================================================
		void NkParticules2D::PreparerRigides(NkPhysicsWorld *monde) noexcept {
			mRigides.Clear();
			if (monde == nullptr) {
				return;
			}
			const float32 dtPas = 1.f / 30.f; // marge de la boite : un pas lent
			NkVector<NkRigidBody> &corpsR = monde->Bodies();
			for (uint32 i = 0; i < corpsR.Size(); ++i) {
				NkRigidBody &b = corpsR[i];
				if ((b.flags & NK_BODY_TRIGGER) != 0u) {
					continue;
				}
				const collision::NkShape s = NkTransformShape(b.restShape, b.position, b.orientation);
				Rigide r;
				r.corps = &b;
				r.type = static_cast<uint8>(b.type);
				switch (s.type) {
					case collision::NkShapeType::NK_CIRCLE2D:
					case collision::NkShapeType::NK_SPHERE:
						r.forme = 0;
						r.centre = NkVec2f(s.p0.x, s.p0.y);
						r.rayon = s.radius;
						break;
					case collision::NkShapeType::NK_BOX2D:
						r.forme = 1;
						r.centre = NkVec2f(s.p0.x, s.p0.y);
						r.demi = NkVec2f(s.p1.x, s.p1.y);
						r.angle = s.rotation;
						break;
					case collision::NkShapeType::NK_CAPSULE2D:
					case collision::NkShapeType::NK_SEGMENT2D:
						r.forme = 2;
						r.a = NkVec2f(s.p0.x, s.p0.y);
						r.b = NkVec2f(s.p1.x, s.p1.y);
						r.rayon = s.type == collision::NkShapeType::NK_SEGMENT2D ? 0.f : s.radius;
						r.centre = (r.a + r.b) * 0.5f;
						break;
					default:
						continue; // formes 3D et polygones : non couples (dit dans l'en-tete)
				}
				r.posCorps = NkVec2f(b.position.x, b.position.y);
				const bool dyn = b.type == NkBodyType::DYNAMIC;
				const bool bouge = b.type != NkBodyType::STATIC;
				r.vitesse = bouge ? NkVec2f(b.linearVelocity.x, b.linearVelocity.y) : NkVec2f(0.f, 0.f);
				r.omega = bouge ? b.angularVelocity.z : 0.f;
				r.vitessePose = r.vitesse;
				r.omegaPose = r.omega;
				r.invMasse = dyn ? b.invMass : 0.f;
				r.invInertie = (dyn && (b.flags & NK_BODY_FIXED_ROT) == 0u) ? b.invInertiaDiag.z : 0.f;
				r.friction = b.material.dynamicFriction;
				r.rebond = b.material.restitution;
				// Boite englobante de la forme sur tout le pas, gonflee d'une
				// particule : c'est elle qui choisit les cellules a visiter.
				float32 ext = r.rayon;
				if (r.forme == 1) {
					ext = Len(r.demi);
				} else if (r.forme == 2) {
					ext = Len(r.b - r.a) * 0.5f + r.rayon;
				}
				const float32 deplacement = Len(r.vitesse) * dtPas + ext * (r.omega < 0.f ? -r.omega : r.omega) * dtPas;
				const float32 marge = ext + deplacement + 0.25f;
				r.mnx = r.centre.x - marge;
				r.mny = r.centre.y - marge;
				r.mxx = r.centre.x + marge;
				r.mxy = r.centre.y + marge;
				mRigides.PushBack(r);
			}
		}

		void NkParticules2D::ResoudreRigides(float32 h, float32 t) noexcept {
			if (mRigides.Size() == 0) {
				return;
			}
			for (uint32 ri = 0; ri < mRigides.Size(); ++ri) {
				Rigide &r = mRigides[ri];
				// Pose du corps a la FIN de ce sous-pas, extrapolee de sa vitesse :
				// un corps qui bouge ne laisse pas les particules le traverser entre
				// deux pas du solveur rigide.
				// ⚠️ La vitesse du DEBUT du pas, pas celle que les impulses viennent
				// de lui donner : le solveur rigide annulera ces impulses contre le
				// sol, et une pose extrapolee avec elles faisait s'enfoncer la
				// matiere de 3 cm dans une caisse immobile -- puis l'en ejectait a
				// 13 m/s au sous-pas suivant (le blob REBONDISSAIT sur la caisse).
				// Mais la vitesse du debut SEULE oublie que la matiere pousse le
				// corps : la quantite de mouvement fuyait de 10,8 % (temoin q6).
				// Compromis mesure : moitie debut, moitie apres impulses -- q6 a
				// 9,9 %, u3 (blob sur caisse) stable. La rotation garde la vitesse
				// du debut : c'est elle qui produisait les pics angulaires.
				const float32 dA = r.omegaPose * t;
				const float32 cA = math::NkCos(dA);
				const float32 sA = math::NkSin(dA);
				const NkVec2f pivot = r.posCorps + (r.vitessePose + (r.vitesse - r.vitessePose) * 0.5f) * t;
				auto pose = [&](const NkVec2f &p) { return pivot + Rot(p - r.posCorps, cA, sA); };
				const NkVec2f centre = pose(r.centre);
				const NkVec2f A = pose(r.a);
				const NkVec2f B = pose(r.b);
				const float32 angle = r.angle + dA;
				const float32 cB = math::NkCos(angle);
				const float32 sB = math::NkSin(angle);
				NkRigidBody *corpsR = static_cast<NkRigidBody *>(r.corps);

				r.contacts = 0;
				PourCellules(r.mnx, r.mny, r.mxx, r.mxy, [&](uint32 i) {
					NkParticule2D &p = particules[i];
					if (p.invMasse == 0.f) {
						return;
					}
					const NkCorpsP2D &cp = corps[p.corps];
					if (!cp.couplageRigide) {
						return;
					}
					// --- contact cercle (particule) / forme -----------------------
					NkVec2f n;
					float32 prof = 0.f;
					NkVec2f point;
					if (r.forme == 0) {
						const NkVec2f d = p.pos - centre;
						const float32 l2 = Len2(d);
						const float32 rr = r.rayon + p.rayon;
						if (l2 >= rr * rr) {
							return;
						}
						const float32 l = math::NkSqrt(l2);
						n = l > 1.0e-9f ? d / l : NkVec2f(0.f, 1.f);
						prof = rr - l;
						point = centre + n * r.rayon;
					} else if (r.forme == 1) {
						const NkVec2f loc = Rot(p.pos - centre, cB, -sB);
						const NkVec2f q(Clampf(loc.x, -r.demi.x, r.demi.x), Clampf(loc.y, -r.demi.y, r.demi.y));
						NkVec2f dl = loc - q;
						float32 l2 = Len2(dl);
						NkVec2f nl;
						if (l2 > 1.0e-14f) {
							if (l2 >= p.rayon * p.rayon) {
								return;
							}
							const float32 l = math::NkSqrt(l2);
							nl = dl / l;
							prof = p.rayon - l;
						} else {
							// Centre DANS la boite : on sort par la face la plus proche.
							const float32 fx = r.demi.x - (loc.x < 0.f ? -loc.x : loc.x);
							const float32 fy = r.demi.y - (loc.y < 0.f ? -loc.y : loc.y);
							if (fx < fy) {
								nl = NkVec2f(loc.x < 0.f ? -1.f : 1.f, 0.f);
								prof = fx + p.rayon;
							} else {
								nl = NkVec2f(0.f, loc.y < 0.f ? -1.f : 1.f);
								prof = fy + p.rayon;
							}
						}
						n = Rot(nl, cB, sB);
						point = centre + Rot(q, cB, sB);
					} else {
						const NkVec2f ab = B - A;
						const float32 l2ab = Len2(ab);
						const float32 u = l2ab > 1.0e-12f ? Clampf(Dot(p.pos - A, ab) / l2ab, 0.f, 1.f) : 0.f;
						const NkVec2f proche = A + ab * u;
						const NkVec2f d = p.pos - proche;
						const float32 l2 = Len2(d);
						const float32 rr = r.rayon + p.rayon;
						if (l2 >= rr * rr) {
							return;
						}
						const float32 l = math::NkSqrt(l2);
						n = l > 1.0e-9f ? d / l : Perp(ab / Maxf(math::NkSqrt(l2ab), 1.0e-9f));
						prof = rr - l;
						point = proche + n * r.rayon;
					}

					// --- reponse ------------------------------------------------
					// POSITION : la particule sort de la forme (contrainte d'inegalite
					// PBD ; le corps ne bouge pas en position -- il n'est deplace que par
					// son solveur, a partir de sa vitesse).
					// QUANTITE DE MOUVEMENT : l'echange se fait sur la vitesse RELATIVE
					// d'approche, impulse par impulse, en tenant a jour la vitesse du
					// corps. ⚠️ MESURE (2026-09-29) : convertir la profondeur en impulse
					// (lambda / h) donnait a une caisse de 20 kg, posee au sol, 11 m/s
					// vers le bas sous un blob de 64 kg -- la caisse ne "voit" pas le sol
					// ici --, elle s'enfoncait de 19 cm, rebondissait, et le blob
					// remontait de 70 cm avant d'ejecter la caisse a 10 m.
					const NkVec2f vP = (p.pos - p.prec) / h;
					const NkVec2f bras = point - pivot;
					const float32 rn = Cross(bras, n);
					// PARTAGE DE MASSE : N particules qui touchent le corps au meme
					// sous-pas le voient chacune N fois plus lourd. Sans lui, les
					// impulses sequentielles d'un seul passage se contredisaient (le bras
					// de levier change d'une particule a l'autre) : mesure, une caisse
					// sous un blob prenait +/- 11 rad/s d'une image a l'autre.
					const float32 partage = static_cast<float32>(r.contactsPrec > 1u ? r.contactsPrec : 1u);
					const float32 wB = (r.invMasse + rn * rn * r.invInertie) / partage;
					r.contacts++;
					const float32 wP = p.invMasse;
					const float32 lambda = prof / (wP + wB);
					p.pos += n * (lambda * wP);
					p.contact = true;
					p.normale = n;
					stats.contactsRigides++;

					// Frottement de la particule, relatif au point du corps.
					const NkVec2f vPoint = r.vitesse + Perp(bras) * r.omega;
					const float32 mu = 0.5f * (cp.friction + r.friction) * 1.5f;
					if (mu > 0.f) {
						const NkVec2f dep = (p.pos - p.prec) - vPoint * h;
						const NkVec2f tang = dep - n * Dot(dep, n);
						const float32 lt = Len(tang);
						if (lt > 1.0e-9f) {
							const float32 k = (lt < mu * prof) ? 1.f : Minf(mu * prof / lt, 1.f);
							p.pos -= tang * (k * wP / (wP + wB));
						}
					}

					if (corpsR != nullptr && r.invMasse > 0.f) {
						const NkVec2f vRel = vP - vPoint;
						const float32 vn = Dot(vRel, n);
						if (vn < 0.f) {
							const float32 jn = -vn / (wP + wB); // sur la particule, selon +n
							NkVec2f J = n * jn;
							const NkVec2f vt = vRel - n * vn;
							const float32 lvt = Len(vt);
							if (lvt > 1.0e-9f && mu > 0.f) {
								const float32 jt = Minf(lvt / (wP + wB), mu * jn);
								J -= vt * (jt / lvt);
							}
							// Le corps recoit l'oppose -- et on tient a jour SA vitesse
							// pour les particules suivantes (impulses sequentielles).
							const NkVec2f Jb = NkVec2f(-J.x, -J.y);
							NkApplyImpulseAtPoint(*corpsR, NkVec3f(Jb.x, Jb.y, 0.f), NkVec3f(point.x, point.y, 0.f));
							r.vitesse += Jb * r.invMasse;
							r.omega += Cross(bras, Jb) * r.invInertie;
							corpsR->flags &= ~static_cast<uint32>(NK_BODY_SLEEPING);
							corpsR->sleepTimer = 0.f;
						}
					}
				});
				r.contactsPrec = r.contacts;
			}
		}

		void NkParticules2D::Contact(NkParticule2D &p, const NkVec2f &n, float32 profondeur, float32 friction) noexcept {
			p.pos += n * profondeur;
			p.contact = true;
			p.normale = n;
			const float32 mu = friction * 1.5f;
			if (mu <= 0.f) {
				return;
			}
			const NkVec2f dep = p.pos - p.prec;
			const NkVec2f tang = dep - n * Dot(dep, n);
			const float32 lt = Len(tang);
			if (lt < 1.0e-9f) {
				return;
			}
			const float32 k = (lt < mu * profondeur) ? 1.f : Minf(mu * profondeur / lt, 1.f);
			p.pos -= tang * k;
		}

		void NkParticules2D::ResoudreLimites() noexcept {
			const NkLimites2D &L = reglages.limites;
			for (uint32 i = 0; i < particules.Size(); ++i) {
				NkParticule2D &p = particules[i];
				if (!L.actif) {
					continue;
				}
				if (p.invMasse == 0.f) {
					continue;
				}
				const float32 r = p.rayon;
				const float32 f = corps[p.corps].friction;
				const bool surSol = L.murs || (p.pos.x > L.gauche && p.pos.x < L.droite);
				if (surSol && p.pos.y < L.bas + r && p.prec.y > L.bas - r * 2.f) {
					Contact(p, NkVec2f(0.f, 1.f), L.bas + r - p.pos.y, f);
				}
				if (L.murs) {
					if (p.pos.x < L.gauche + r) {
						Contact(p, NkVec2f(1.f, 0.f), L.gauche + r - p.pos.x, f);
					}
					if (p.pos.x > L.droite - r) {
						Contact(p, NkVec2f(-1.f, 0.f), p.pos.x - (L.droite - r), f);
					}
				}
				if (L.plafond && p.pos.y > L.haut - r) {
					Contact(p, NkVec2f(0.f, -1.f), p.pos.y - (L.haut - r), f);
				}
			}
		}

		void NkParticules2D::MajVitesses(float32 h) noexcept {
			const float32 invH = 1.f / h;
			const float32 vMax = 80.f;
			for (uint32 i = 0; i < particules.Size(); ++i) {
				NkParticule2D &p = particules[i];
				if (p.epingle || p.invMasse == 0.f) {
					if (!p.saisie) {
						p.pos = p.prec;
					}
					p.vit = NkVec2f(0.f, 0.f);
					p.contact = false;
					continue;
				}
				NkVec2f v = (p.pos - p.prec) * invH;
				if (p.contact) {
					const float32 vnAvant = Dot(p.vit, p.normale);
					if (vnAvant < -0.8f) {
						const float32 vn = Dot(v, p.normale);
						v += p.normale * (-corps[p.corps].rebond * vnAvant - vn);
					}
				}
				const float32 l2 = Len2(v);
				if (l2 > vMax * vMax) {
					v *= vMax / math::NkSqrt(l2);
				}
				if (!Fini(v.x) || !Fini(v.y) || !Fini(p.pos.x) || !Fini(p.pos.y)) {
					// Garde-fou : une particule empoisonnee contaminerait ses
					// voisines en un sous-pas. On la ramene a son dernier etat sain.
					p.pos = (Fini(p.prec.x) && Fini(p.prec.y)) ? p.prec : NkVec2f(0.f, 1.f);
					v = NkVec2f(0.f, 0.f);
				}
				p.vit = v;
				p.contact = false;
			}
		}

		void NkParticules2D::Viscosite() noexcept {
			const float32 hF = mFluide[0].h;
			const float32 hF2 = hF * hF;
			const uint32 n = static_cast<uint32>(particules.Size());
			bool fluide = false;
			for (uint32 i = 0; i < n && !fluide; ++i) {
				fluide = mEstFluide[i] != 0;
			}
			if (!fluide) {
				return;
			}
			mTmpV2.Resize(n);
			mTmpF.Resize(n);
			for (uint32 i = 0; i < n; ++i) {
				mTmpV2[i] = NkVec2f(0.f, 0.f);
				mTmpF[i] = 0.f;
			}
			PourPaires([&](uint32 i, uint32 j) {
				if (!mEstFluide[i] || !mEstFluide[j]) {
					return;
				}
				const NkParticule2D &a = particules[i];
				const NkParticule2D &b = particules[j];
				const float32 r2 = Len2(b.pos - a.pos);
				if (r2 >= hF2) {
					return;
				}
				const float32 q = 1.f - math::NkSqrt(r2) / hF;
				const NkVec2f dv = b.vit - a.vit;
				mTmpV2[i] += dv * q;
				mTmpV2[j] -= dv * q;
				mTmpF[i] += q;
				mTmpF[j] += q;
			});
			for (uint32 i = 0; i < n; ++i) {
				if (mTmpF[i] <= 0.f) {
					continue;
				}
				NkParticule2D &p = particules[i];
				if (p.invMasse == 0.f) {
					continue;
				}
				// Moyenne ponderee : jamais plus que "prendre la vitesse du
				// voisinage", donc stable quelle que soit la viscosite choisie.
				const float32 c = Clampf(corps[p.corps].viscosite, 0.f, 0.5f);
				p.vit += mTmpV2[i] * (c / Maxf(mTmpF[i], 1.f));
			}
		}

		void NkParticules2D::AmortirLiens() noexcept {
			for (uint32 li = 0; li < liens.Size(); ++li) {
				const NkLien2D &l = liens[li];
				if (l.casse) {
					continue;
				}
				const float32 k = corps[l.corps].amortissement;
				if (k <= 0.f) {
					continue;
				}
				NkParticule2D &a = particules[l.a];
				NkParticule2D &b = particules[l.b];
				const float32 w = a.invMasse + b.invMasse;
				if (w <= 0.f) {
					continue;
				}
				const NkVec2f d = b.pos - a.pos;
				const float32 len = Len(d);
				if (len < 1.0e-8f) {
					continue;
				}
				const NkVec2f n = d / len;
				const float32 dv = Dot(b.vit - a.vit, n) * Clampf(k, 0.f, 1.f);
				a.vit += n * (dv * a.invMasse / w);
				b.vit -= n * (dv * b.invMasse / w);
			}
		}

		void NkParticules2D::Souder() noexcept {
			if (!reglages.soudure || !mGrilleValide || mTri.Size() != particules.Size()) {
				return;
			}
			PourPaires([&](uint32 i, uint32 j) {
				NkParticule2D &a = particules[i];
				NkParticule2D &b = particules[j];
				const NkCorpsP2D &ca = corps[a.corps];
				const NkCorpsP2D &cb = corps[b.corps];
				if (ca.mat != cb.mat || !NkEstSoudableP2D(ca.mat)) {
					return;
				}
				if (a.nbVoisins >= 6 || b.nbVoisins >= 6) {
					return;
				}
				const float32 s = 0.5f * (ca.espacement + cb.espacement);
				const float32 r2 = Len2(b.pos - a.pos);
				if (ca.mat == NkMateriauP2D::NK_ATOMES) {
					// Un atome ne se lie qu'a un voisin PROCHE et LENT : c'est ce qui
					// fait recristalliser un metal refroidi, et pas un gaz.
					if (r2 > (1.08f * s) * (1.08f * s) || Len2(a.vit - b.vit) > 0.7f * 0.7f) {
						return;
					}
					if (Relier(i, j, s)) {
						stats.soudures++;
					}
				} else {
					if (r2 > (1.2f * s) * (1.2f * s)) {
						return;
					}
					if (Relier(i, j, Clampf(math::NkSqrt(r2), 0.9f * s, 1.1f * s))) {
						stats.soudures++;
					}
				}
			});
		}

		void NkParticules2D::PurgerLiens() noexcept {
			uint32 w = 0;
			for (uint32 i = 0; i < liens.Size(); ++i) {
				const NkLien2D &l = liens[i];
				if (l.casse && !NkEstGrilleP2D(corps[l.corps].mat)) {
					continue;
				}
				liens[w++] = l;
			}
			liens.Resize(w);
			mCasseesPurgeables = 0;
			RecalculerPlages();
		}

		void NkParticules2D::Compacter(const NkVector<uint8> &garder) noexcept {
			const uint32 n = static_cast<uint32>(particules.Size());
			NkVector<uint32> remap;
			remap.Resize(n);
			uint32 w = 0;
			for (uint32 i = 0; i < n; ++i) {
				remap[i] = garder[i] ? w++ : NK_P2D_AUCUN;
			}
			for (uint32 i = 0; i < n; ++i) {
				if (remap[i] == NK_P2D_AUCUN) {
					continue;
				}
				NkParticule2D p = particules[i];
				uint8 nv = 0;
				for (uint8 k = 0; k < p.nbVoisins; ++k) {
					const uint32 v = remap[p.voisins[k]];
					if (v != NK_P2D_AUCUN) {
						p.voisins[nv++] = v;
					}
				}
				p.nbVoisins = nv;
				particules[remap[i]] = p;
			}
			particules.Resize(w);

			uint32 wl = 0;
			for (uint32 i = 0; i < liens.Size(); ++i) {
				NkLien2D l = liens[i];
				if (remap[l.a] == NK_P2D_AUCUN || remap[l.b] == NK_P2D_AUCUN) {
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
					remapC[c] = NK_P2D_AUCUN;
					continue;
				}
				remapC[c] = wc;
				corps[wc++] = corps[c];
			}
			corps.Resize(wc);
			for (uint32 i = 0; i < particules.Size(); ++i) {
				particules[i].corps = remapC[particules[i].corps];
			}
			for (uint32 i = 0; i < liens.Size(); ++i) {
				NkLien2D &l = liens[i];
				l.corps = remapC[l.corps] == NK_P2D_AUCUN ? particules[l.a].corps : remapC[l.corps];
			}

			SaisirFin();
			mGrilleValide = false;
			RecalculerPlages();
			// Un ballon ampute n'est plus un anneau : il se degonfle.
			for (uint32 c = 0; c < corps.Size(); ++c) {
				if (corps[c].mat == NkMateriauP2D::NK_BALLON && corps[c].nombre != static_cast<uint32>(corps[c].nx)) {
					corps[c].pression = 0.f;
				}
			}
		}

		// =====================================================================
		// Requetes
		// =====================================================================
		int32 NkParticules2D::ParticuleProche(const NkVec2f &p, float32 rayonMax) const noexcept {
			int32 meilleur = -1;
			float32 best = rayonMax * rayonMax;
			for (uint32 i = 0; i < particules.Size(); ++i) {
				const float32 r = particules[i].rayon;
				const float32 d2 = Len2(particules[i].pos - p) - r * r;
				if (d2 < best) {
					best = d2;
					meilleur = static_cast<int32>(i);
				}
			}
			return meilleur;
		}

		int32 NkParticules2D::IndexCorps(uint32 id) const noexcept {
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

		NkVec2f NkParticules2D::CentreCorps(uint32 ci) const noexcept {
			const NkCorpsP2D &c = corps[ci];
			NkVec2f s(0.f, 0.f);
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				s += particules[i].pos;
			}
			return c.nombre > 0 ? s / static_cast<float32>(c.nombre) : s;
		}

		NkVec2f NkParticules2D::VitesseCorps(uint32 ci) const noexcept {
			const NkCorpsP2D &c = corps[ci];
			NkVec2f s(0.f, 0.f);
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				s += particules[i].vit;
			}
			return c.nombre > 0 ? s / static_cast<float32>(c.nombre) : s;
		}

		void NkParticules2D::BoiteCorps(uint32 ci, NkVec2f &mn, NkVec2f &mx) const noexcept {
			const NkCorpsP2D &c = corps[ci];
			mn = NkVec2f(1.0e9f, 1.0e9f);
			mx = NkVec2f(-1.0e9f, -1.0e9f);
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				const NkParticule2D &p = particules[i];
				mn.x = Minf(mn.x, p.pos.x - p.rayon);
				mn.y = Minf(mn.y, p.pos.y - p.rayon);
				mx.x = Maxf(mx.x, p.pos.x + p.rayon);
				mx.y = Maxf(mx.y, p.pos.y + p.rayon);
			}
		}

		uint32 NkParticules2D::LiensActifs() const noexcept {
			uint32 n = 0;
			for (uint32 i = 0; i < liens.Size(); ++i) {
				n += liens[i].casse ? 0u : 1u;
			}
			return n;
		}

		uint32 NkParticules2D::LiensActifsDuCorps(uint32 ci) const noexcept {
			uint32 n = 0;
			for (uint32 i = 0; i < liens.Size(); ++i) {
				n += (!liens[i].casse && liens[i].corps == ci) ? 1u : 0u;
			}
			return n;
		}

		// =====================================================================
		// Editions
		// =====================================================================
		void NkParticules2D::SupprimerCorps(uint32 ci) noexcept {
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

		uint32 NkParticules2D::Gommer(const NkVec2f &p, float32 rayon) noexcept {
			NkVector<uint8> garder;
			garder.Resize(particules.Size());
			NkVector<uint8> entier;
			entier.Resize(corps.Size());
			for (uint32 c = 0; c < corps.Size(); ++c) {
				entier[c] = 0;
			}
			bool touche = false;
			for (uint32 i = 0; i < particules.Size(); ++i) {
				garder[i] = 1;
				const NkParticule2D &q = particules[i];
				if (Len2(q.pos - p) > (rayon + q.rayon) * (rayon + q.rayon)) {
					continue;
				}
				const NkMateriauP2D m = corps[q.corps].mat;
				if (NkEstFluideP2D(m) || m == NkMateriauP2D::NK_SABLE || m == NkMateriauP2D::NK_ATOMES) {
					garder[i] = 0;
				} else {
					entier[q.corps] = 1;
				}
				touche = true;
			}
			if (!touche) {
				return 0;
			}
			uint32 n = 0;
			for (uint32 i = 0; i < particules.Size(); ++i) {
				if (entier[particules[i].corps]) {
					garder[i] = 0;
				}
				n += garder[i] ? 0u : 1u;
			}
			Compacter(garder);
			return n;
		}

		uint32 NkParticules2D::Couper(const NkVec2f &a, const NkVec2f &b) noexcept {
			uint32 n = 0;
			for (uint32 i = 0; i < liens.Size(); ++i) {
				NkLien2D &l = liens[i];
				if (l.casse) {
					continue;
				}
				if (!SegmentsSeCroisent(particules[l.a].pos, particules[l.b].pos, a, b)) {
					continue;
				}
				l.casse = true;
				++n;
				NkCorpsP2D &c = corps[l.corps];
				if (l.genre == NkGenreLien2D::NK_LIAISON) {
					RetirerVoisin(l.a, l.b);
				}
				if (!NkEstGrilleP2D(c.mat)) {
					mCasseesPurgeables++;
				}
				if (c.mat == NkMateriauP2D::NK_BALLON) {
					c.pression = 0.f;
				}
			}
			rupturesTotal += n;
			return n;
		}

		void NkParticules2D::Explosion(const NkVec2f &c, float32 rayon, float32 vitesse) noexcept {
			for (uint32 i = 0; i < particules.Size(); ++i) {
				NkParticule2D &p = particules[i];
				if (p.invMasse == 0.f) {
					continue;
				}
				const NkVec2f d = p.pos - c;
				const float32 l = Len(d);
				if (l >= rayon) {
					continue;
				}
				const NkVec2f dir = l > 1.0e-6f ? d / l : NkVec2f(0.f, 1.f);
				const float32 att = 1.f - l / rayon;
				p.vit += dir * (vitesse * att * att);
			}
		}

		void NkParticules2D::Aimant(const NkVec2f &c, float32 rayon, float32 acceleration, float32 dt) noexcept {
			for (uint32 i = 0; i < particules.Size(); ++i) {
				NkParticule2D &p = particules[i];
				if (p.invMasse == 0.f) {
					continue;
				}
				const NkVec2f d = c - p.pos;
				const float32 l = Len(d);
				if (l >= rayon || l < 1.0e-5f) {
					continue;
				}
				const float32 att = 1.f - l / rayon;
				p.vit += d / l * (acceleration * att * dt);
				// Un aimant qui ne freine pas fait osciller la matiere autour de son
				// centre a l'infini : on dissipe un peu, pres du coeur.
				p.vit *= 1.f - 2.5f * att * dt;
			}
		}

		void NkParticules2D::Translater(uint32 ci, const NkVec2f &delta) noexcept {
			const NkCorpsP2D &c = corps[ci];
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				particules[i].pos += delta;
				particules[i].prec += delta;
			}
		}

		void NkParticules2D::AppliquerVitesse(uint32 ci, const NkVec2f &v) noexcept {
			const NkCorpsP2D &c = corps[ci];
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				if (particules[i].invMasse > 0.f) {
					particules[i].vit = v;
				}
			}
		}

		void NkParticules2D::BasculerEpingle(uint32 i) noexcept {
			if (i >= particules.Size()) {
				return;
			}
			NkParticule2D &p = particules[i];
			p.epingle = !p.epingle;
			p.invMasse = p.epingle ? 0.f : (p.masse > 0.f ? 1.f / p.masse : 0.f);
			p.vit = NkVec2f(0.f, 0.f);
		}

		void NkParticules2D::EpinglerCorps(uint32 ci, bool epingle) noexcept {
			const NkCorpsP2D &c = corps[ci];
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				NkParticule2D &p = particules[i];
				p.epingle = epingle;
				p.invMasse = epingle ? 0.f : (p.masse > 0.f ? 1.f / p.masse : 0.f);
				p.vit = NkVec2f(0.f, 0.f);
			}
		}

		void NkParticules2D::AppliquerMasse(uint32 ci) noexcept {
			const NkCorpsP2D &c = corps[ci];
			const float32 m = Maxf(c.masseParticule, 0.001f);
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				NkParticule2D &p = particules[i];
				p.masse = m;
				p.invMasse = p.epingle ? 0.f : 1.f / m;
			}
		}

		bool NkParticules2D::SaisirDebut(const NkVec2f &p, float32 rayon) noexcept {
			SaisirFin();
			for (uint32 i = 0; i < particules.Size(); ++i) {
				if (Len2(particules[i].pos - p) <= rayon * rayon) {
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
				NkParticule2D &q = particules[mSaisis[k]];
				// Decalage RESSERRE : on tient une poignee de matiere, pas un moule
				// rigide. Un fluide se laisse ainsi etirer.
				mSaisisDecalage[k] = (q.pos - p) * 0.6f;
				q.saisie = true;
			}
			mCibleSaisie = p;
			return true;
		}

		void NkParticules2D::SaisirVers(const NkVec2f &p) noexcept {
			mCibleSaisie = p;
		}

		void NkParticules2D::SaisirFin() noexcept {
			for (uint32 k = 0; k < mSaisis.Size(); ++k) {
				if (mSaisis[k] < particules.Size()) {
					particules[mSaisis[k]].saisie = false;
				}
			}
			mSaisis.Clear();
			mSaisisDecalage.Clear();
		}

	} // namespace physics
} // namespace nkentseu
