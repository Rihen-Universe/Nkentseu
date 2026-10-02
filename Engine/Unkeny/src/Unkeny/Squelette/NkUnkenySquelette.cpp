//
// NkUnkenySquelette.cpp — le squelette 2D d'Unkeny (voir l'en-tete).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
//

#include "Unkeny/Squelette/NkUnkenySquelette.h"

#include "NKECS/World/NkWorld.h"
#include "NKLogger/NkLog.h"
#include "NKPhysics/NkParticules2D.h"
#include "NKPhysics/NkParticules2DFabrique.h"
#include "Unkeny/Scene/NkUnkenyActif.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

		using math::NkMat4f;

		namespace {
			constexpr float32 kPi = 3.14159265358979f;

			void CopierNom(char *dst, uint32 taille, const char *src) noexcept {
				uint32 i = 0;
				if (src != nullptr) {
					for (; i + 1u < taille && src[i] != '\0'; ++i) {
						dst[i] = src[i];
					}
				}
				for (; i < taille; ++i) {
					dst[i] = '\0';
				}
			}

			NkVec2f Transformer(const NkMat4f &m, const NkVec2f &p) noexcept {
				return NkVec2f(m.m00 * p.x + m.m10 * p.y + m.m30, m.m01 * p.x + m.m11 * p.y + m.m31);
			}

			float32 AngleMonde(const NkMat4f &m) noexcept {
				return std::atan2(m.m01, m.m00);
			}

			float32 Echelle(const NkMat4f &m) noexcept {
				const float32 e = std::sqrt(m.m00 * m.m00 + m.m01 * m.m01);
				return e > 1e-6f ? e : 1.f;
			}

			/// LA FK, celle de NKAnima (os dans l'ordre des indices : parent avant enfant).
			void Monde(const NkSquelette2D &s, bool pose, NkMat4f *monde) noexcept {
				int32 parents[NK_SQUELETTE2D_OS_MAX];
				NkMat4f local[NK_SQUELETTE2D_OS_MAX];
				const uint32 n = s.nbOs <= NK_SQUELETTE2D_OS_MAX ? s.nbOs : NK_SQUELETTE2D_OS_MAX;
				for (uint32 j = 0; j < n; ++j) {
					const int32 p = s.os[j].parent;
					parents[j] = (p >= 0 && static_cast<uint32>(p) < j) ? p : -1;
					local[j] = anim::NkBone2DToMatrix(pose ? NkOsPose2D(s.os[j]) : NkOsRepos2D(s.os[j]));
				}
				anim::NkForwardKinematics(parents, nullptr, n, local, monde);
			}

			float32 DistanceSegment(const NkVec2f &p, const NkVec2f &a, const NkVec2f &b) noexcept {
				const float32 abx = b.x - a.x, aby = b.y - a.y;
				const float32 l2 = abx * abx + aby * aby;
				float32 k = l2 > 1e-12f ? ((p.x - a.x) * abx + (p.y - a.y) * aby) / l2 : 0.f;
				k = k < 0.f ? 0.f : (k > 1.f ? 1.f : k);
				const float32 dx = a.x + abx * k - p.x, dy = a.y + aby * k - p.y;
				return std::sqrt(dx * dx + dy * dy);
			}

			/// Les poids d'un sommet en table (un par os).
			void Table(const NkMaillage2D &m, uint32 i, float32 *parOs) noexcept {
				for (uint32 j = 0; j < NK_SQUELETTE2D_OS_MAX; ++j) {
					parOs[j] = 0.f;
				}
				for (uint32 k = 0; k < NK_MAILLAGE2D_OS; ++k) {
					const uint8 o = m.os[i * NK_MAILLAGE2D_OS + k];
					const uint16 w = m.poids[i * NK_MAILLAGE2D_OS + k];
					if (o != NK_MAILLAGE2D_SANS_OS && o < NK_SQUELETTE2D_OS_MAX && w > 0u) {
						parOs[o] += static_cast<float32>(w) / static_cast<float32>(NK_MAILLAGE2D_POIDS_PLEIN);
					}
				}
			}

			/// Les DOUBLES d'une couture recoivent les memes poids (la moyenne) : sans
			/// cela, une couture s'OUVRE des qu'un os plie (les deux cotes ne suivent
			/// pas la meme matrice).
			void UnifierDoubles(NkMaillage2D &m) noexcept {
				uint8 fait[NK_MAILLAGE2D_SOMMETS_MAX] = {};
				for (uint32 i = 0; i < m.nbSommets; ++i) {
					if (fait[i] != 0u) {
						continue;
					}
					uint8 d[NK_MAILLAGE2D_SOMMETS_MAX];
					const uint32 n = NkDoublesSommet2D(m, i, d, NK_MAILLAGE2D_SOMMETS_MAX);
					if (n < 2u) {
						fait[i] = 1u;
						continue;
					}
					float32 somme[NK_SQUELETTE2D_OS_MAX] = {};
					for (uint32 k = 0; k < n; ++k) {
						float32 t[NK_SQUELETTE2D_OS_MAX];
						Table(m, d[k], t);
						for (uint32 j = 0; j < NK_SQUELETTE2D_OS_MAX; ++j) {
							somme[j] += t[j] / static_cast<float32>(n);
						}
						fait[d[k]] = 1u;
					}
					for (uint32 k = 0; k < n; ++k) {
						NkPoserPoidsSommet2D(m, d[k], somme);
					}
				}
			}

			/// Les os d'une chaine molle (le premier, puis le PREMIER enfant de chacun).
			uint32 OsDeLaChaine(const NkSquelette2D &s, const NkChaineMolle2D &c, uint32 *sortie) noexcept {
				if (c.premier < 0 || static_cast<uint32>(c.premier) >= s.nbOs || c.nombre == 0u) {
					return 0u;
				}
				uint32 n = 0;
				int32 o = c.premier;
				while (o >= 0 && n < c.nombre && n < NK_SQUELETTE2D_OS_MAX) {
					sortie[n++] = static_cast<uint32>(o);
					int32 suivant = -1;
					for (uint32 j = static_cast<uint32>(o) + 1u; j < s.nbOs; ++j) {
						if (s.os[j].parent == o) {
							suivant = static_cast<int32>(j);
							break;
						}
					}
					o = suivant;
				}
				return n;
			}

			/// Le point que l'IK tire pour l'os `mid` : la tete de son premier enfant, ou son bout.
			NkVec2f Effecteur(const NkSquelette2D &s, uint32 mid) noexcept {
				for (uint32 j = mid + 1u; j < s.nbOs; ++j) {
					if (s.os[j].parent == static_cast<int32>(mid)) {
						return NkVec2f(s.os[j].px, s.os[j].py);
					}
				}
				return NkVec2f(s.os[mid].longueur, 0.f);
			}
		} // namespace

		// =====================================================================
		// LECTURE
		// =====================================================================
		int32 NkSqueletteTrouverOs(const NkSquelette2D &s, const char *nom) noexcept {
			if (nom == nullptr || nom[0] == '\0') {
				return -1;
			}
			for (uint32 j = 0; j < s.nbOs; ++j) {
				if (std::strncmp(s.os[j].nom, nom, sizeof(s.os[j].nom)) == 0) {
					return static_cast<int32>(j);
				}
			}
			return -1;
		}

		anim::NkBone2D NkOsRepos2D(const NkOs2D &o) noexcept {
			anim::NkBone2D b;
			b.x = o.x;
			b.y = o.y;
			b.angle = o.angle;
			b.sx = o.ex;
			b.sy = o.ey;
			return b;
		}

		anim::NkBone2D NkOsPose2D(const NkOs2D &o) noexcept {
			anim::NkBone2D b;
			b.x = o.px;
			b.y = o.py;
			b.angle = o.pangle;
			b.sx = o.pex;
			b.sy = o.pey;
			return b;
		}

		void NkOsPoserRepos2D(NkOs2D &o, const anim::NkBone2D &b) noexcept {
			o.x = b.x;
			o.y = b.y;
			o.angle = b.angle;
			o.ex = b.sx;
			o.ey = b.sy;
		}

		void NkOsPoserPose2D(NkOs2D &o, const anim::NkBone2D &b) noexcept {
			o.px = b.x;
			o.py = b.y;
			o.pangle = b.angle;
			o.pex = b.sx;
			o.pey = b.sy;
		}

		void NkSqueletteMondeRepos(const NkSquelette2D &s, NkMat4f *monde) noexcept {
			Monde(s, false, monde);
		}

		void NkSqueletteMondePose(const NkSquelette2D &s, NkMat4f *monde) noexcept {
			Monde(s, true, monde);
		}

		NkVec2f NkSqueletteTete(const NkMat4f *monde, uint32 j) noexcept {
			return NkVec2f(monde[j].m30, monde[j].m31);
		}

		NkVec2f NkSqueletteQueue(const NkSquelette2D &s, const NkMat4f *monde, uint32 j) noexcept {
			return Transformer(monde[j], NkVec2f(j < s.nbOs ? s.os[j].longueur : 0.f, 0.f));
		}

		int32 NkSqueletteOsSous(const NkSquelette2D &s, const NkVec2f &p, float32 rayon, bool pose) noexcept {
			NkMat4f monde[NK_SQUELETTE2D_OS_MAX];
			Monde(s, pose, monde);
			int32 meilleur = -1;
			float32 dMin = rayon;
			for (uint32 j = 0; j < s.nbOs; ++j) {
				const float32 d = DistanceSegment(p, NkSqueletteTete(monde, j), NkSqueletteQueue(s, monde, j));
				if (d <= dMin) {
					dMin = d;
					meilleur = static_cast<int32>(j);
				}
			}
			return meilleur;
		}

		bool NkSqueletteEnPose(const NkSquelette2D &s) noexcept {
			for (uint32 j = 0; j < s.nbOs; ++j) {
				const NkOs2D &o = s.os[j];
				if (std::fabs(o.px - o.x) > 1e-5f || std::fabs(o.py - o.y) > 1e-5f ||
					std::fabs(anim::NkWrapAngle(o.pangle - o.angle)) > 1e-5f || std::fabs(o.pex - o.ex) > 1e-5f ||
					std::fabs(o.pey - o.ey) > 1e-5f) {
					return true;
				}
			}
			return false;
		}

		// =====================================================================
		// LA PEAU
		// =====================================================================
		void NkSqueletteMatricesPeau(const NkSquelette2D &s, NkMat4f *peau) noexcept {
			NkMat4f repos[NK_SQUELETTE2D_OS_MAX];
			Monde(s, false, repos);
			Monde(s, true, peau);
			for (uint32 j = 0; j < s.nbOs; ++j) {
				peau[j] = peau[j] * repos[j].Inverse();
			}
		}

		uint32 NkMaillageDeformer2D(const NkSquelette2D *s, const NkMaillage2D &m, NkVec2f *sortie) noexcept {
			if (s == nullptr || s->nbOs == 0u) {
				for (uint32 i = 0; i < m.nbSommets; ++i) {
					sortie[i] = m.positions[i];
				}
				return m.nbSommets;
			}
			NkMat4f peau[NK_SQUELETTE2D_OS_MAX];
			NkSqueletteMatricesPeau(*s, peau);
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				const NkVec2f &p = m.positions[i];
				float32 somme = 0.f;
				NkVec2f q(0.f, 0.f);
				for (uint32 k = 0; k < NK_MAILLAGE2D_OS; ++k) {
					const uint8 o = m.os[i * NK_MAILLAGE2D_OS + k];
					const uint16 w = m.poids[i * NK_MAILLAGE2D_OS + k];
					if (o == NK_MAILLAGE2D_SANS_OS || o >= s->nbOs || w == 0u) {
						continue;
					}
					const float32 f = static_cast<float32>(w);
					const NkVec2f r = Transformer(peau[o], p);
					q = NkVec2f(q.x + r.x * f, q.y + r.y * f);
					somme += f;
				}
				sortie[i] = somme > 0.f ? NkVec2f(q.x / somme, q.y / somme) : p;
			}
			return m.nbSommets;
		}

		uint32 NkSommetsPonderes2D(const NkMaillage2D &m) noexcept {
			uint32 n = 0;
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				bool pondere = false;
				for (uint32 k = 0; k < NK_MAILLAGE2D_OS && !pondere; ++k) {
					pondere = m.os[i * NK_MAILLAGE2D_OS + k] != NK_MAILLAGE2D_SANS_OS && m.poids[i * NK_MAILLAGE2D_OS + k] > 0u;
				}
				n += pondere ? 1u : 0u;
			}
			return n;
		}

		void NkEmplacementsParties2D(const NkSquelette2D *s, const NkMaillage2D &m, int32 *ordre, bool *visible) noexcept {
			for (uint32 k = 0; k < NK_MAILLAGE2D_PARTIES_MAX; ++k) {
				ordre[k] = m.parties[k].ordre;
				visible[k] = true;
			}
			if (s == nullptr) {
				return;
			}
			for (uint32 e = 0; e < s->nbEmplacements; ++e) {
				const NkEmplacement2D &x = s->emplacements[e];
				for (uint32 a = 0; a < NK_SQUELETTE2D_ATTACHES_MAX; ++a) {
					const int32 p = x.attaches[a];
					if (p < 0 || static_cast<uint32>(p) >= m.nbParties) {
						continue;
					}
					const bool montree = static_cast<int32>(a) == x.attache;
					visible[p] = montree;
					if (montree) {
						ordre[p] = x.ordre;
					}
				}
			}
		}

		// =====================================================================
		// LES POIDS
		// =====================================================================
		float32 NkPoidsSommet2D(const NkMaillage2D &m, uint32 i, uint32 os) noexcept {
			if (i >= m.nbSommets) {
				return 0.f;
			}
			float32 w = 0.f;
			for (uint32 k = 0; k < NK_MAILLAGE2D_OS; ++k) {
				if (m.os[i * NK_MAILLAGE2D_OS + k] == os) {
					w += static_cast<float32>(m.poids[i * NK_MAILLAGE2D_OS + k]) / static_cast<float32>(NK_MAILLAGE2D_POIDS_PLEIN);
				}
			}
			return w;
		}

		void NkPoserPoidsSommet2D(NkMaillage2D &m, uint32 i, const float32 *parOs) noexcept {
			if (i >= NK_MAILLAGE2D_SOMMETS_MAX) {
				return;
			}
			// Les quatre plus forts.
			int32 choisi[NK_MAILLAGE2D_OS] = {-1, -1, -1, -1};
			for (uint32 k = 0; k < NK_MAILLAGE2D_OS; ++k) {
				float32 meilleur = 1e-4f;
				for (uint32 j = 0; j < NK_SQUELETTE2D_OS_MAX; ++j) {
					bool pris = false;
					for (uint32 q = 0; q < k; ++q) {
						pris = pris || choisi[q] == static_cast<int32>(j);
					}
					if (!pris && parOs[j] > meilleur) {
						meilleur = parOs[j];
						choisi[k] = static_cast<int32>(j);
					}
				}
			}
			float32 somme = 0.f;
			for (uint32 k = 0; k < NK_MAILLAGE2D_OS; ++k) {
				somme += choisi[k] >= 0 ? parOs[choisi[k]] : 0.f;
			}
			uint32 total = 0;
			int32 fort = -1;
			for (uint32 k = 0; k < NK_MAILLAGE2D_OS; ++k) {
				const uint32 c = i * NK_MAILLAGE2D_OS + k;
				if (choisi[k] < 0 || somme <= 0.f) {
					m.os[c] = NK_MAILLAGE2D_SANS_OS;
					m.poids[c] = 0u;
					continue;
				}
				const uint32 q = static_cast<uint32>(std::floor(parOs[choisi[k]] / somme * static_cast<float32>(NK_MAILLAGE2D_POIDS_PLEIN)));
				m.os[c] = static_cast<uint8>(choisi[k]);
				m.poids[c] = static_cast<uint16>(q > NK_MAILLAGE2D_POIDS_PLEIN ? NK_MAILLAGE2D_POIDS_PLEIN : q);
				total += m.poids[c];
				if (fort < 0) {
					fort = static_cast<int32>(k); // le premier choisi est le plus fort
				}
			}
			// L'arrondi va au plus fort : la somme fait EXACTEMENT le plein.
			if (fort >= 0 && total < NK_MAILLAGE2D_POIDS_PLEIN) {
				m.poids[i * NK_MAILLAGE2D_OS + static_cast<uint32>(fort)] =
					static_cast<uint16>(m.poids[i * NK_MAILLAGE2D_OS + static_cast<uint32>(fort)] + (NK_MAILLAGE2D_POIDS_PLEIN - total));
			}
		}

		void NkNormaliserPoids2D(NkMaillage2D &m) noexcept {
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				float32 t[NK_SQUELETTE2D_OS_MAX];
				Table(m, i, t);
				NkPoserPoidsSommet2D(m, i, t);
			}
			UnifierDoubles(m);
		}

		uint32 NkPeindrePoids2D(NkMaillage2D &m, uint32 os, const NkVec2f &centre, float32 rayon, float32 force, bool retirer) noexcept {
			if (os >= NK_SQUELETTE2D_OS_MAX || rayon <= 0.f) {
				return 0u;
			}
			uint32 touches = 0;
			force = force < 0.f ? 0.f : (force > 1.f ? 1.f : force);
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				const float32 dx = m.positions[i].x - centre.x, dy = m.positions[i].y - centre.y;
				const float32 d = std::sqrt(dx * dx + dy * dy);
				if (d > rayon) {
					continue;
				}
				const float32 f = force * (1.f - d / rayon);
				float32 t[NK_SQUELETTE2D_OS_MAX];
				Table(m, i, t);
				const float32 avant = t[os];
				const float32 apres = retirer ? avant * (1.f - f) : avant + f * (1.f - avant);
				float32 autres = 0.f;
				for (uint32 j = 0; j < NK_SQUELETTE2D_OS_MAX; ++j) {
					autres += j != os ? t[j] : 0.f;
				}
				// Les autres os se partagent ce qui reste, dans leurs proportions.
				const float32 reste = 1.f - apres;
				for (uint32 j = 0; j < NK_SQUELETTE2D_OS_MAX; ++j) {
					if (j != os) {
						t[j] = autres > 1e-6f ? t[j] * reste / autres : 0.f;
					}
				}
				t[os] = apres;
				NkPoserPoidsSommet2D(m, i, t);
				++touches;
			}
			UnifierDoubles(m);
			return touches;
		}

		void NkAutoPoidsDistance2D(const NkSquelette2D &s, NkMaillage2D &m) noexcept {
			if (s.nbOs == 0u) {
				NkMaillageSansOs2D(m);
				return;
			}
			NkMat4f monde[NK_SQUELETTE2D_OS_MAX];
			Monde(s, false, monde);
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				float32 t[NK_SQUELETTE2D_OS_MAX] = {};
				for (uint32 j = 0; j < s.nbOs; ++j) {
					const float32 d = DistanceSegment(m.positions[i], NkSqueletteTete(monde, j), NkSqueletteQueue(s, monde, j));
					t[j] = 1.f / (d * d + 1e-4f);
				}
				NkPoserPoidsSommet2D(m, i, t);
			}
			UnifierDoubles(m);
		}

		void NkAutoPoidsChaleur2D(const NkSquelette2D &s, NkMaillage2D &m) noexcept {
			if (s.nbOs == 0u || m.nbSommets == 0u) {
				NkMaillageSansOs2D(m);
				return;
			}
			NkMat4f monde[NK_SQUELETTE2D_OS_MAX];
			Monde(s, false, monde);
			const uint32 n = m.nbSommets;
			// L'os le plus proche de chaque sommet, et la distance (la « vue » de Pinocchio).
			int32 proche[NK_MAILLAGE2D_SOMMETS_MAX];
			float32 dist[NK_MAILLAGE2D_SOMMETS_MAX];
			float32 dOs[NK_MAILLAGE2D_SOMMETS_MAX][NK_SQUELETTE2D_OS_MAX];
			for (uint32 i = 0; i < n; ++i) {
				proche[i] = 0;
				dist[i] = 1e30f;
				for (uint32 j = 0; j < s.nbOs; ++j) {
					dOs[i][j] = DistanceSegment(m.positions[i], NkSqueletteTete(monde, j), NkSqueletteQueue(s, monde, j));
					if (dOs[i][j] < dist[i]) {
						dist[i] = dOs[i][j];
						proche[i] = static_cast<int32>(j);
					}
				}
			}
			// Le graphe : les aretes des triangles, ponderees 1/longueur^2 ; les doubles
			// d'une couture sont SOUDES (une arete tres raide : la chaleur passe).
			struct Voisin {
					uint8 j;
					float32 a;
			};
			static constexpr uint32 kVoisinsMax = 16u;
			Voisin voisins[NK_MAILLAGE2D_SOMMETS_MAX][kVoisinsMax];
			uint8 nbVoisins[NK_MAILLAGE2D_SOMMETS_MAX] = {};
			float32 longueurMoyenne = 0.f;
			uint32 nbAretes = 0;
			auto Relier = [&](uint32 a, uint32 b, float32 poids) {
				for (uint32 k = 0; k < nbVoisins[a]; ++k) {
					if (voisins[a][k].j == b) {
						return;
					}
				}
				if (nbVoisins[a] < kVoisinsMax) {
					voisins[a][nbVoisins[a]++] = Voisin{static_cast<uint8>(b), poids};
				}
			};
			for (uint32 t = 0; t < m.nbTriangles; ++t) {
				for (uint32 e = 0; e < 3u; ++e) {
					const uint32 a = m.triangles[t * 3u + e], b = m.triangles[t * 3u + (e + 1u) % 3u];
					const float32 dx = m.positions[a].x - m.positions[b].x, dy = m.positions[a].y - m.positions[b].y;
					const float32 l2 = dx * dx + dy * dy;
					longueurMoyenne += std::sqrt(l2);
					++nbAretes;
					const float32 w = 1.f / (l2 > 1e-8f ? l2 : 1e-8f);
					Relier(a, b, w);
					Relier(b, a, w);
				}
			}
			longueurMoyenne = nbAretes > 0u ? longueurMoyenne / static_cast<float32>(nbAretes) : 0.1f;
			const float32 soudure = 100.f / (longueurMoyenne * longueurMoyenne);
			for (uint32 i = 0; i < n; ++i) {
				uint8 d[NK_MAILLAGE2D_SOMMETS_MAX];
				const uint32 nd = NkDoublesSommet2D(m, i, d, NK_MAILLAGE2D_SOMMETS_MAX);
				for (uint32 k = 0; k < nd; ++k) {
					if (d[k] != i) {
						Relier(i, d[k], soudure);
					}
				}
			}
			// H : la chaleur de l'os le plus proche, en 1/d^2 (bornee pres de l'os).
			const float32 dMin = 0.25f * longueurMoyenne;
			float32 H[NK_MAILLAGE2D_SOMMETS_MAX];
			for (uint32 i = 0; i < n; ++i) {
				const float32 d = dist[i] > dMin ? dist[i] : dMin;
				H[i] = 1.f / (d * d);
			}
			// Une diffusion par os : (sum a_ij + H_i) w_i = sum a_ij w_j + H_i p_i (Gauss-Seidel).
			float32 w[NK_SQUELETTE2D_OS_MAX][NK_MAILLAGE2D_SOMMETS_MAX];
			for (uint32 j = 0; j < s.nbOs; ++j) {
				for (uint32 i = 0; i < n; ++i) {
					// p_i : 1 si cet os est le plus proche (ou a egalite a 1 % pres).
					w[j][i] = dOs[i][j] <= dist[i] * 1.01f + 1e-6f ? 1.f : 0.f;
				}
				for (uint32 it = 0; it < 120u; ++it) {
					for (uint32 i = 0; i < n; ++i) {
						const float32 p = dOs[i][j] <= dist[i] * 1.01f + 1e-6f ? 1.f : 0.f;
						float32 num = H[i] * p, den = H[i];
						for (uint32 k = 0; k < nbVoisins[i]; ++k) {
							num += voisins[i][k].a * w[j][voisins[i][k].j];
							den += voisins[i][k].a;
						}
						w[j][i] = den > 0.f ? num / den : p;
					}
				}
			}
			for (uint32 i = 0; i < n; ++i) {
				float32 t[NK_SQUELETTE2D_OS_MAX] = {};
				for (uint32 j = 0; j < s.nbOs; ++j) {
					t[j] = w[j][i] > 0.01f ? w[j][i] : 0.f;
				}
				bool vide = true;
				for (uint32 j = 0; j < s.nbOs; ++j) {
					vide = vide && t[j] <= 0.f;
				}
				if (vide) {
					t[proche[i]] = 1.f;
				}
				NkPoserPoidsSommet2D(m, i, t);
			}
			UnifierDoubles(m);
		}

		uint32 NkPoidsParParties2D(const NkSquelette2D &s, NkMaillage2D &m) noexcept {
			uint32 apparies = 0;
			for (uint32 k = 0; k < m.nbParties; ++k) {
				const int32 o = NkSqueletteTrouverOs(s, m.parties[k].nom);
				if (o < 0) {
					continue;
				}
				++apparies;
				for (uint32 i = 0; i < m.nbSommets; ++i) {
					if (m.partieSommet[i] != k) {
						continue;
					}
					float32 t[NK_SQUELETTE2D_OS_MAX] = {};
					t[o] = 1.f;
					NkPoserPoidsSommet2D(m, i, t);
				}
			}
			UnifierDoubles(m);
			return apparies;
		}

		void NkPoidsSansOs2D(NkMaillage2D &m, uint32 j) noexcept {
			for (uint32 c = 0; c < NK_MAILLAGE2D_SOMMETS_MAX * NK_MAILLAGE2D_OS; ++c) {
				if (m.os[c] == NK_MAILLAGE2D_SANS_OS) {
					continue;
				}
				if (m.os[c] == j) {
					m.os[c] = NK_MAILLAGE2D_SANS_OS;
					m.poids[c] = 0u;
				} else if (m.os[c] > j) {
					m.os[c] = static_cast<uint8>(m.os[c] - 1u);
				}
			}
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				float32 t[NK_SQUELETTE2D_OS_MAX];
				Table(m, i, t);
				NkPoserPoidsSommet2D(m, i, t);
			}
		}

		// =====================================================================
		// EDITION
		// =====================================================================
		int32 NkSqueletteAjouterOs(NkSquelette2D &s, const char *nom, int32 parent, const NkVec2f &tete, const NkVec2f &queue) noexcept {
			if (s.nbOs >= NK_SQUELETTE2D_OS_MAX || parent >= static_cast<int32>(s.nbOs) || parent < -1) {
				return -1;
			}
			const uint32 j = s.nbOs;
			NkOs2D o;
			char genere[16];
			if (nom == nullptr || nom[0] == '\0' || NkSqueletteTrouverOs(s, nom) >= 0) {
				// Un nom LIBRE : « Os 7 », sinon « Os 7b »...
				uint32 k = j + 1u;
				do {
					std::snprintf(genere, sizeof(genere), "Os %u", k++);
				} while (NkSqueletteTrouverOs(s, genere) >= 0);
				nom = genere;
			}
			CopierNom(o.nom, sizeof(o.nom), nom);
			o.parent = static_cast<int8>(parent);
			s.os[j] = o;
			s.nbOs = static_cast<uint8>(j + 1u);
			NkSqueletteReposTeteQueue(s, j, tete, queue);
			return static_cast<int32>(j);
		}

		void NkSqueletteReposTeteQueue(NkSquelette2D &s, uint32 j, const NkVec2f &tete, const NkVec2f &queue) noexcept {
			if (j >= s.nbOs) {
				return;
			}
			NkMat4f monde[NK_SQUELETTE2D_OS_MAX];
			Monde(s, false, monde);
			const float32 dx = queue.x - tete.x, dy = queue.y - tete.y;
			const float32 longueur = std::sqrt(dx * dx + dy * dy);
			anim::NkBone2D voulu;
			voulu.x = tete.x;
			voulu.y = tete.y;
			voulu.angle = longueur > 1e-6f ? std::atan2(dy, dx) : AngleMonde(monde[j]);
			NkMat4f local = anim::NkBone2DToMatrix(voulu);
			const int32 p = s.os[j].parent;
			float32 echelle = 1.f;
			if (p >= 0) {
				local = monde[p].Inverse() * local;
				echelle = Echelle(monde[p]);
			}
			anim::NkBone2D b = anim::NkBone2DFromMatrix(local);
			b.sx = s.os[j].ex;
			b.sy = s.os[j].ey;
			NkOsPoserRepos2D(s.os[j], b);
			s.os[j].longueur = longueur / (echelle * (s.os[j].ex > 1e-6f ? s.os[j].ex : 1.f));
			NkOsPoserPose2D(s.os[j], b);
		}

		bool NkSqueletteRetirerOs(NkSquelette2D &s, NkMaillage2D *m, uint32 j) noexcept {
			if (j >= s.nbOs) {
				return false;
			}
			NkMat4f repos[NK_SQUELETTE2D_OS_MAX], pose[NK_SQUELETTE2D_OS_MAX];
			Monde(s, false, repos);
			Monde(s, true, pose);
			const int32 pj = s.os[j].parent;
			// Les enfants passent au parent, A LEUR PLACE (repos et pose).
			for (uint32 c = j + 1u; c < s.nbOs; ++c) {
				if (s.os[c].parent != static_cast<int32>(j)) {
					continue;
				}
				const NkMat4f lr = pj >= 0 ? repos[pj].Inverse() * repos[c] : repos[c];
				const NkMat4f lp = pj >= 0 ? pose[pj].Inverse() * pose[c] : pose[c];
				NkOsPoserRepos2D(s.os[c], anim::NkBone2DFromMatrix(lr));
				NkOsPoserPose2D(s.os[c], anim::NkBone2DFromMatrix(lp));
				s.os[c].parent = static_cast<int8>(pj);
			}
			for (uint32 c = j; c + 1u < s.nbOs; ++c) {
				s.os[c] = s.os[c + 1u];
			}
			s.os[s.nbOs - 1u] = NkOs2D();
			s.nbOs = static_cast<uint8>(s.nbOs - 1u);
			for (uint32 c = 0; c < s.nbOs; ++c) {
				if (s.os[c].parent > static_cast<int32>(j)) {
					s.os[c].parent = static_cast<int8>(s.os[c].parent - 1);
				}
			}
			for (uint32 e = 0; e < s.nbEmplacements; ++e) {
				int8 &o = s.emplacements[e].os;
				o = o == static_cast<int32>(j) ? static_cast<int8>(pj) : (o > static_cast<int32>(j) ? static_cast<int8>(o - 1) : o);
			}
			// Une chaine ou une IK qui passe par l'os s'en va ; les autres suivent les indices.
			for (uint32 k = 0; k < s.nbChaines;) {
				NkChaineMolle2D &c = s.chaines[k];
				if (c.premier == static_cast<int32>(j)) {
					for (uint32 q = k; q + 1u < s.nbChaines; ++q) {
						s.chaines[q] = s.chaines[q + 1u];
					}
					s.chaines[--s.nbChaines] = NkChaineMolle2D();
					continue;
				}
				if (c.premier > static_cast<int32>(j)) {
					c.premier = static_cast<int8>(c.premier - 1);
				}
				++k;
			}
			for (uint32 k = 0; k < s.nbIK;) {
				NkContrainteIK2D &c = s.ik[k];
				if (c.os == static_cast<int32>(j) || c.cible == static_cast<int32>(j)) {
					for (uint32 q = k; q + 1u < s.nbIK; ++q) {
						s.ik[q] = s.ik[q + 1u];
					}
					s.ik[--s.nbIK] = NkContrainteIK2D();
					continue;
				}
				c.os = c.os > static_cast<int32>(j) ? static_cast<int8>(c.os - 1) : c.os;
				c.cible = c.cible > static_cast<int32>(j) ? static_cast<int8>(c.cible - 1) : c.cible;
				++k;
			}
			if (m != nullptr) {
				NkPoidsSansOs2D(*m, j);
			}
			return true;
		}

		void NkSqueletteRetourRepos(NkSquelette2D &s) noexcept {
			for (uint32 j = 0; j < s.nbOs; ++j) {
				NkOsPoserPose2D(s.os[j], NkOsRepos2D(s.os[j]));
			}
			for (uint32 e = 0; e < s.nbEmplacements; ++e) {
				s.emplacements[e].attache = s.emplacements[e].attacheRepos;
				s.emplacements[e].ordre = s.emplacements[e].ordreRepos;
			}
		}

		uint32 NkSqueletteSymetrie(NkSquelette2D &s, uint32 j) noexcept {
			if (j >= s.nbOs) {
				return 0u;
			}
			// L'os et ses descendants, dans l'ordre des indices (parents d'abord).
			bool dans[NK_SQUELETTE2D_OS_MAX] = {};
			dans[j] = true;
			for (uint32 c = j + 1u; c < s.nbOs; ++c) {
				dans[c] = s.os[c].parent >= 0 && dans[s.os[c].parent];
			}
			const uint32 n0 = s.nbOs;
			uint32 touches = 0;
			for (uint32 c = j; c < n0; ++c) {
				if (!dans[c]) {
					continue;
				}
				const NkString miroir = anim::NkMirrorBoneName(NkString(s.os[c].nom));
				if (miroir.Empty()) {
					continue; // un os sans G ni D n'a pas de jumeau
				}
				NkMat4f monde[NK_SQUELETTE2D_OS_MAX];
				Monde(s, false, monde);
				const NkVec2f t = NkSqueletteTete(monde, c), q = NkSqueletteQueue(s, monde, c);
				const NkVec2f tm(-t.x, t.y), qm(-q.x, q.y);
				const int32 existe = NkSqueletteTrouverOs(s, miroir.CStr());
				if (existe >= 0) {
					NkSqueletteReposTeteQueue(s, static_cast<uint32>(existe), tm, qm);
				} else {
					// Le parent du jumeau : le jumeau du parent s'il existe, sinon le meme.
					int32 parent = s.os[c].parent;
					if (parent >= 0) {
						const NkString pm = anim::NkMirrorBoneName(NkString(s.os[parent].nom));
						const int32 jp = pm.Empty() ? -1 : NkSqueletteTrouverOs(s, pm.CStr());
						if (jp >= 0) {
							parent = jp;
						}
					}
					if (NkSqueletteAjouterOs(s, miroir.CStr(), parent, tm, qm) < 0) {
						break;
					}
				}
				++touches;
			}
			return touches;
		}

		bool NkSqueletteModele(NkSquelette2D &s, anim::NkSkeleton2DTemplate t, const NkVec2f &lo, const NkVec2f &hi) noexcept {
			anim::NkSkeleton2D sk;
			if (!anim::NkMakeSkeleton2D(t, lo, hi, sk)) {
				return false;
			}
			return NkSqueletteDepuisNKAnima(s, sk, nullptr);
		}

		bool NkSqueletteIK2D(NkSquelette2D &s, uint32 mid, const NkVec2f &effecteur, const NkVec2f &cible, bool coudePositif,
							 float32 melange) noexcept {
			if (mid >= s.nbOs || s.os[mid].parent < 0) {
				return false;
			}
			const uint32 haut = static_cast<uint32>(s.os[mid].parent);
			NkMat4f monde[NK_SQUELETTE2D_OS_MAX];
			Monde(s, true, monde);
			anim::NkBone2D bh = NkOsPose2D(s.os[haut]), bm = NkOsPose2D(s.os[mid]);
			const bool ok = anim::NkApplyTwoBoneIK2DWorld(monde[haut], monde[mid], bh, bm, effecteur, cible, coudePositif, melange);
			NkOsPoserPose2D(s.os[haut], bh);
			NkOsPoserPose2D(s.os[mid], bm);
			return ok;
		}

		bool NkSqueletteCoudePositif2D(const NkSquelette2D &s, uint32 mid, const NkVec2f &effecteur) noexcept {
			if (mid >= s.nbOs || s.os[mid].parent < 0) {
				return true;
			}
			NkMat4f monde[NK_SQUELETTE2D_OS_MAX];
			Monde(s, true, monde);
			return anim::NkTwoBoneBendIsPositive2D(monde[s.os[mid].parent], monde[mid], effecteur);
		}

		int32 NkSqueletteAjouterEmplacement(NkSquelette2D &s, const char *nom, int32 os, int32 partie) noexcept {
			if (s.nbEmplacements >= NK_SQUELETTE2D_EMPLACEMENTS_MAX || os >= static_cast<int32>(s.nbOs)) {
				return -1;
			}
			NkEmplacement2D e;
			CopierNom(e.nom, sizeof(e.nom), nom != nullptr && nom[0] != '\0' ? nom : "Emplacement");
			e.os = static_cast<int8>(os);
			e.attaches[0] = static_cast<int8>(partie);
			s.emplacements[s.nbEmplacements] = e;
			return static_cast<int32>(s.nbEmplacements++);
		}

		int32 NkSqueletteAjouterChaine(NkSquelette2D &s, uint32 premier, uint32 nombre) noexcept {
			if (s.nbChaines >= NK_SQUELETTE2D_CHAINES_MAX || premier >= s.nbOs || nombre == 0u) {
				return -1;
			}
			NkChaineMolle2D c;
			c.premier = static_cast<int8>(premier);
			c.nombre = static_cast<uint8>(nombre > NK_SQUELETTE2D_OS_MAX ? NK_SQUELETTE2D_OS_MAX : nombre);
			uint32 os[NK_SQUELETTE2D_OS_MAX];
			c.nombre = static_cast<uint8>(OsDeLaChaine(s, c, os));
			s.chaines[s.nbChaines] = c;
			return static_cast<int32>(s.nbChaines++);
		}

		// =====================================================================
		// NKANIMA
		// =====================================================================
		namespace {
			NkString NomPartie(const NkMaillage2D *m, int32 p) {
				if (m != nullptr && p >= 0 && static_cast<uint32>(p) < m->nbParties && m->parties[p].nom[0] != '\0') {
					return NkString(m->parties[p].nom);
				}
				char b[24];
				std::snprintf(b, sizeof(b), "Partie %d", static_cast<int>(p));
				return NkString(b);
			}

			int32 PartieDeNom(const NkMaillage2D *m, const NkString &nom) {
				if (m != nullptr) {
					for (uint32 k = 0; k < m->nbParties; ++k) {
						if (nom == NkString(m->parties[k].nom)) {
							return static_cast<int32>(k);
						}
					}
				}
				int p = -1;
				if (std::sscanf(nom.CStr(), "Partie %d", &p) == 1) {
					return p;
				}
				return -1;
			}
		} // namespace

		bool NkSqueletteVersNKAnima(const NkSquelette2D &s, const NkMaillage2D *m, anim::NkSkeleton2D &out) {
			out.Clear();
			for (uint32 j = 0; j < s.nbOs; ++j) {
				if (out.AddBone(s.os[j].nom, s.os[j].parent, NkOsRepos2D(s.os[j]), s.os[j].longueur) < 0) {
					return false;
				}
			}
			for (uint32 e = 0; e < s.nbEmplacements; ++e) {
				const NkEmplacement2D &x = s.emplacements[e];
				anim::NkSlot2DDef d;
				d.name = NkString(x.nom);
				d.bone = x.os;
				for (uint32 a = 0; a < NK_SQUELETTE2D_ATTACHES_MAX; ++a) {
					if (x.attaches[a] >= 0) {
						d.attachments.PushBack(NomPartie(m, x.attaches[a]));
					}
				}
				d.attachment = x.attacheRepos;
				d.order = x.ordreRepos;
				out.slots.PushBack(d);
			}
			return out.Validate();
		}

		bool NkSqueletteDepuisNKAnima(NkSquelette2D &s, const anim::NkSkeleton2D &in, const NkMaillage2D *m) {
			const uint32 n = in.Count();
			if (n > NK_SQUELETTE2D_OS_MAX) {
				logger.Warn("[unkeny] squelette 2D : {0} os, le composant en porte {1}", n, NK_SQUELETTE2D_OS_MAX);
				return false;
			}
			// L'ordre du composant : parents AVANT enfants (l'ordre topologique de NKAnima).
			int32 nouveau[NK_SQUELETTE2D_OS_MAX];
			uint32 ancien[NK_SQUELETTE2D_OS_MAX];
			const bool topo = static_cast<uint32>(in.skeleton.topo.Size()) == n;
			for (uint32 k = 0; k < n; ++k) {
				ancien[k] = topo ? in.skeleton.topo[k] : k;
				nouveau[ancien[k]] = static_cast<int32>(k);
			}
			NkSquelette2D r;
			r.osVisibles = s.osVisibles;
			std::memcpy(r.source, s.source, sizeof(r.source));
			for (uint32 k = 0; k < n; ++k) {
				const uint32 j = ancien[k];
				NkOs2D &o = r.os[k];
				CopierNom(o.nom, sizeof(o.nom), in.skeleton.bones[j].name);
				const int32 p = in.Parent(j);
				o.parent = static_cast<int8>(p >= 0 ? nouveau[p] : -1);
				NkOsPoserRepos2D(o, in.BindLocal2D(j));
				NkOsPoserPose2D(o, in.BindLocal2D(j));
				o.longueur = j < static_cast<uint32>(in.lengths.Size()) ? in.lengths[j] : 0.f;
			}
			r.nbOs = static_cast<uint8>(n);
			for (uint32 e = 0; e < static_cast<uint32>(in.slots.Size()) && e < NK_SQUELETTE2D_EMPLACEMENTS_MAX; ++e) {
				const anim::NkSlot2DDef &d = in.slots[e];
				NkEmplacement2D &x = r.emplacements[e];
				CopierNom(x.nom, sizeof(x.nom), d.name.CStr());
				x.os = static_cast<int8>(d.bone >= 0 && static_cast<uint32>(d.bone) < n ? nouveau[d.bone] : -1);
				for (uint32 a = 0; a < static_cast<uint32>(d.attachments.Size()) && a < NK_SQUELETTE2D_ATTACHES_MAX; ++a) {
					x.attaches[a] = static_cast<int8>(PartieDeNom(m, d.attachments[a]));
				}
				x.attache = x.attacheRepos = static_cast<int8>(d.attachment);
				x.ordre = x.ordreRepos = static_cast<int16>(d.order);
				r.nbEmplacements = static_cast<uint8>(e + 1u);
			}
			s = r;
			return true;
		}

		bool NkSauverSquelette2D(const NkSquelette2D &s, const NkMaillage2D *m, const char *chemin) {
			anim::NkSkeleton2D sk;
			if (chemin == nullptr || !NkSqueletteVersNKAnima(s, m, sk)) {
				return false;
			}
			return sk.SaveBinary(NkString(chemin));
		}

		bool NkChargerSquelette2D(NkSquelette2D &s, const NkMaillage2D *m, const char *chemin) {
			anim::NkSkeleton2D sk;
			if (chemin == nullptr || !sk.LoadBinary(NkString(chemin))) {
				return false;
			}
			return NkSqueletteDepuisNKAnima(s, sk, m);
		}

		uint32 NkSqueletteAppliquerPose(NkSquelette2D &s, const anim::NkAnimPose &pose) noexcept {
			uint32 ecrits = 0;
			const anim::NkAnimationClip *clip = pose.skeleton;
			if (clip != nullptr) {
				for (uint32 j = 0; j < static_cast<uint32>(pose.bones.Size()); ++j) {
					if (j < static_cast<uint32>(clip->boneTracks.Size()) && clip->boneTracks[j].Empty()) {
						continue; // le clip ne cle pas cet os : il garde sa pose
					}
					const int32 o = NkSqueletteOsDuClip(s, *clip, j);
					if (o < 0) {
						continue;
					}
					anim::NkBone2D b = anim::NkBone2DFromTRS(pose.bones[j]);
					NkOsPoserPose2D(s.os[o], b);
					++ecrits;
				}
			}
			for (uint32 k = 0; k < static_cast<uint32>(pose.props.Size()); ++k) {
				const anim::NkPropValue &v = pose.props[k];
				NkString nom;
				bool ordre = false;
				if (!v.target.Empty() || v.weight <= 0.5f || !anim::NkParseSlot2DProperty(v.property, nom, ordre)) {
					continue;
				}
				for (uint32 e = 0; e < s.nbEmplacements; ++e) {
					if (nom == NkString(s.emplacements[e].nom)) {
						const int32 x = static_cast<int32>(std::floor(v.value.x + 0.5f));
						if (ordre) {
							s.emplacements[e].ordre = static_cast<int16>(x);
						} else {
							s.emplacements[e].attache = static_cast<int8>(x);
						}
					}
				}
			}
			return ecrits;
		}

		int32 NkSqueletteOsDuClip(const NkSquelette2D &s, const anim::NkAnimationClip &clip, uint32 j) noexcept {
			const char *nom = nullptr;
			if (j < static_cast<uint32>(clip.jointNames.Size()) && !clip.jointNames[j].Empty()) {
				nom = clip.jointNames[j].CStr();
			} else if (j < static_cast<uint32>(clip.boneTracks.Size())) {
				nom = clip.boneTracks[j].name.CStr();
			}
			return NkSqueletteTrouverOs(s, nom);
		}

		uint32 NkSqueletteCle(const NkSquelette2D &s, anim::NkAnimationClip &clip, float32 t, const uint8 *choisis, anim::NkInterpMode interp) {
			if (s.nbOs == 0u) {
				return 0u;
			}
			// Le clip connait-il CE squelette (memes noms, meme ordre) ? Sinon : ses pistes
			// sont reprises PAR NOM sur un clip prepare pour lui.
			bool pareil = clip.skeletalLocal && clip.boneCount == s.nbOs && static_cast<uint32>(clip.jointNames.Size()) == s.nbOs;
			for (uint32 j = 0; pareil && j < s.nbOs; ++j) {
				pareil = clip.jointNames[j] == NkString(s.os[j].nom);
			}
			if (!pareil) {
				anim::NkSkeleton2D sk;
				if (!NkSqueletteVersNKAnima(s, nullptr, sk)) {
					return 0u;
				}
				anim::NkAnimationClip neuf = clip;
				neuf.boneTracks.Clear();
				neuf.boneCount = 0;
				neuf.jointNames.Clear();
				neuf.jointParent.Clear();
				neuf.jointInverseBind.Clear();
				neuf.jointTopo.Clear();
				neuf.ResizeBones(s.nbOs);
				for (uint32 j = 0; j < s.nbOs; ++j) {
					for (uint32 a = 0; a < static_cast<uint32>(clip.boneTracks.Size()); ++a) {
						if (NkSqueletteOsDuClip(s, clip, a) == static_cast<int32>(j)) {
							neuf.boneTracks[j] = clip.boneTracks[a];
						}
					}
				}
				sk.PrepareClip(neuf, true);
				clip = neuf;
			}
			uint32 n = 0;
			for (uint32 j = 0; j < s.nbOs; ++j) {
				if (choisis != nullptr && choisis[j] == 0u) {
					continue;
				}
				anim::NkAddBoneKey2D(clip, j, t, NkOsPose2D(s.os[j]), interp);
				++n;
			}
			return n;
		}

		NkString NkSqueletteCheminOs(const NkSquelette2D &s, uint32 j) {
			if (j >= s.nbOs) {
				return NkString();
			}
			uint32 pile[NK_SQUELETTE2D_OS_MAX];
			uint32 n = 0;
			for (int32 o = static_cast<int32>(j); o >= 0 && n < NK_SQUELETTE2D_OS_MAX; o = s.os[o].parent) {
				pile[n++] = static_cast<uint32>(o);
			}
			NkString chemin;
			for (int32 k = static_cast<int32>(n) - 1; k >= 0; --k) {
				chemin.Append(s.os[pile[k]].nom);
				if (k > 0) {
					chemin.Append("/");
				}
			}
			return chemin;
		}

		// =====================================================================
		// EN JEU : les chaines molles et les IK
		// =====================================================================
		namespace {
			/// La chaine en particules, a la place de ses os (repere du MONDE).
			bool ConstruireChaine(NkScene &scene, ecs::NkEntityId id, const NkTransform2D &t, NkSquelette2D &s, NkChaineMolle2D &c) {
				physics::NkParticules2D *pw = scene.Particules();
				uint32 os[NK_SQUELETTE2D_OS_MAX];
				const uint32 n = OsDeLaChaine(s, c, os);
				if (pw == nullptr || n == 0u) {
					return false;
				}
				NkMat4f monde[NK_SQUELETTE2D_OS_MAX];
				Monde(s, true, monde);
				const uint32 ci = pw->NouveauCorps(physics::NkMateriauP2D::NK_CORDE);
				{
					physics::NkCorpsP2D &corps = pw->corps[ci];
					const uint32 idCorps = corps.id, debutCorps = corps.debut;
					physics::NkAppliquerPresetP2D(corps, physics::NkPresetP2D::NK_CORDE);
					corps.id = idCorps;
					corps.debut = debutCorps;
					corps.nombre = 0;
					corps.raideur = math::NkClamp(c.raideur, 0.01f, 1.f);
					corps.amortissement = math::NkClamp(c.amortissement, 0.f, 1.f);
					corps.masseParticule = c.masse > 0.001f ? c.masse : 0.001f;
					corps.autoCollision = false;
					corps.couplageRigide = c.decor;
					corps.formeRaideur = 0.f;
					corps.resistance = 0.f;
				}
				const uint32 debut = static_cast<uint32>(pw->particules.Size());
				const float32 rayon = 0.03f;
				for (uint32 k = 0; k < n; ++k) {
					pw->AjouterParticule(ci, t.VersMonde(NkSqueletteTete(monde, os[k])), rayon);
				}
				pw->AjouterParticule(ci, t.VersMonde(NkSqueletteQueue(s, monde, os[n - 1u])), rayon);
				for (uint32 k = 0; k < n; ++k) {
					pw->AjouterLien(debut + k, debut + k + 1u, physics::NkGenreLien2D::NK_STRUCTURE);
				}
				// La FLEXION (saut d'un maillon) : une echarpe se plie, une corde pend.
				if (c.flexion > 0.f) {
					for (uint32 k = 0; k + 2u <= n; ++k) {
						pw->AjouterLien(debut + k, debut + k + 2u, physics::NkGenreLien2D::NK_FLEXION);
					}
				}
				pw->BasculerEpingle(debut); // le maillon de tete est TENU par l'os parent
				pw->AppliquerMasse(ci);
				pw->FinaliserCorps(ci);
				// Ni dessinee par le rendu des corps mous, ni orpheline : ses contacts vont a l'entite.
				pw->corps[ci].utilisateur = NK_CORPS_MOU_DE_MAILLAGE | id.Pack();
				c.corps = pw->corps[ci].id;
				return true;
			}
		} // namespace

		void NkSquelettesAvantPasFixe(NkScene &scene, float32 dt) {
			(void)dt;
			physics::NkParticules2D *pw = scene.Particules();
			if (pw == nullptr) {
				return;
			}
			ecs::NkWorld &w = scene.Monde();
			NkVector<ecs::NkEntityId> ids;
			w.Query<NkTransform2D, NkSquelette2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &, NkSquelette2D &s) {
				if (s.nbChaines > 0u) {
					ids.PushBack(id);
				}
			});
			for (uint32 k = 0; k < static_cast<uint32>(ids.Size()); ++k) {
				const ecs::NkEntityId id = ids[k];
				NkSquelette2D *s = w.Get<NkSquelette2D>(id);
				const NkTransform2D *t = w.Get<NkTransform2D>(id);
				if (s == nullptr || t == nullptr || !NkEntiteActive(w, id)) {
					continue;
				}
				NkMat4f monde[NK_SQUELETTE2D_OS_MAX];
				Monde(*s, true, monde);
				for (uint32 c = 0; c < s->nbChaines; ++c) {
					NkChaineMolle2D &ch = s->chaines[c];
					if (ch.corps == 0u || pw->IndexCorps(ch.corps) < 0) {
						ch.corps = 0u;
						ConstruireChaine(scene, id, *t, *s, ch);
						continue;
					}
					// Le maillon de tete SUIT l'os qui porte la chaine (cinematique).
					const int32 ci = pw->IndexCorps(ch.corps);
					const physics::NkCorpsP2D &corps = pw->corps[static_cast<uint32>(ci)];
					uint32 os[NK_SQUELETTE2D_OS_MAX];
					if (OsDeLaChaine(*s, ch, os) == 0u || corps.nombre == 0u) {
						continue;
					}
					// La TRAINEE de l'air sur les maillons libres.
					const float32 frein = math::NkClamp(1.f - ch.trainee * dt, 0.f, 1.f);
					for (uint32 q = 1; q < corps.nombre; ++q) {
						physics::NkParticule2D &p = pw->particules[corps.debut + q];
						p.vit = NkVec2f(p.vit.x * frein, p.vit.y * frein);
					}
					physics::NkParticule2D &tete = pw->particules[corps.debut];
					const NkVec2f cible = t->VersMonde(NkSqueletteTete(monde, os[0]));
					tete.vit = dt > 0.f ? NkVec2f((cible.x - tete.pos.x) / dt, (cible.y - tete.pos.y) / dt) : NkVec2f(0.f, 0.f);
					tete.pos = cible;
					tete.prec = cible;
				}
			}
		}

		void NkSquelettesApresAnimation(NkScene &scene, float32 dt) {
			(void)dt;
			ecs::NkWorld &w = scene.Monde();
			const physics::NkParticules2D *pw = scene.Particules();
			w.Query<NkTransform2D, NkSquelette2D>().ForEach([&](ecs::NkEntityId id, NkTransform2D &t, NkSquelette2D &s) {
				if (!NkEntiteActive(w, id)) {
					return;
				}
				// 1. Les IK en jeu : la poignee (un os) tire la chaine de deux os.
				for (uint32 k = 0; k < s.nbIK; ++k) {
					const NkContrainteIK2D &c = s.ik[k];
					if (c.os < 0 || c.cible < 0 || static_cast<uint32>(c.os) >= s.nbOs || static_cast<uint32>(c.cible) >= s.nbOs) {
						continue;
					}
					NkMat4f monde[NK_SQUELETTE2D_OS_MAX];
					Monde(s, true, monde);
					NkSqueletteIK2D(s, static_cast<uint32>(c.os), Effecteur(s, static_cast<uint32>(c.os)),
									NkSqueletteTete(monde, static_cast<uint32>(c.cible)), c.coudePositif, c.melange);
				}
				// 2. Les chaines molles : chaque os pointe vers le maillon suivant.
				if (pw == nullptr) {
					return;
				}
				const float32 cr = math::NkCos(-t.rotation), sr = math::NkSin(-t.rotation);
				const float32 ex = t.echelle.x != 0.f ? t.echelle.x : 1.f, ey = t.echelle.y != 0.f ? t.echelle.y : 1.f;
				auto VersLocal = [&](const NkVec2f &p) {
					const float32 x = p.x - t.position.x, y = p.y - t.position.y;
					return NkVec2f((x * cr - y * sr) / ex, (x * sr + y * cr) / ey);
				};
				for (uint32 c = 0; c < s.nbChaines; ++c) {
					const NkChaineMolle2D &ch = s.chaines[c];
					const int32 ci = ch.corps != 0u ? pw->IndexCorps(ch.corps) : -1;
					if (ci < 0) {
						continue;
					}
					const physics::NkCorpsP2D &corps = pw->corps[static_cast<uint32>(ci)];
					uint32 os[NK_SQUELETTE2D_OS_MAX];
					const uint32 n = OsDeLaChaine(s, ch, os);
					if (n == 0u || corps.nombre < n + 1u) {
						continue;
					}
					for (uint32 k = 0; k < n; ++k) {
						NkMat4f monde[NK_SQUELETTE2D_OS_MAX];
						Monde(s, true, monde);
						const NkVec2f a = VersLocal(pw->particules[corps.debut + k].pos);
						const NkVec2f b = VersLocal(pw->particules[corps.debut + k + 1u].pos);
						const float32 voulu = std::atan2(b.y - a.y, b.x - a.x);
						const int32 p = s.os[os[k]].parent;
						const float32 parent = p >= 0 ? AngleMonde(monde[p]) : 0.f;
						s.os[os[k]].pangle = anim::NkWrapAngle(voulu - parent);
					}
				}
			});
		}

		void NkSqueletteDetruirePhysique(NkScene &scene, NkSquelette2D &s) noexcept {
			physics::NkParticules2D *pw = scene.Particules();
			for (uint32 c = 0; c < NK_SQUELETTE2D_CHAINES_MAX; ++c) {
				if (s.chaines[c].corps != 0u && pw != nullptr) {
					const int32 ci = pw->IndexCorps(s.chaines[c].corps);
					if (ci >= 0) {
						pw->SupprimerCorps(static_cast<uint32>(ci));
					}
				}
			}
			NkSqueletteOublierPhysique(s);
		}

		void NkSqueletteOublierPhysique(NkSquelette2D &s) noexcept {
			for (uint32 c = 0; c < NK_SQUELETTE2D_CHAINES_MAX; ++c) {
				s.chaines[c].corps = 0u;
			}
		}

		// =====================================================================
		// LA DESCRIPTION (sauvegarde, photo, prefab)
		// =====================================================================
		namespace {
#define NK_SQUELETTE_OS(champ, genre) NK_UNKENY_CHAMP_TABLEAU(NkSquelette2D, os, NkOs2D, champ, genre, "os." #champ)
#define NK_SQUELETTE_EMPL(champ, genre)                                                                                \
	NK_UNKENY_CHAMP_TABLEAU(NkSquelette2D, emplacements, NkEmplacement2D, champ, genre, "emplacements." #champ)
#define NK_SQUELETTE_CHAINE(champ, genre)                                                                              \
	NK_UNKENY_CHAMP_TABLEAU(NkSquelette2D, chaines, NkChaineMolle2D, champ, genre, "chaines." #champ)
#define NK_SQUELETTE_IK(champ, genre) NK_UNKENY_CHAMP_TABLEAU(NkSquelette2D, ik, NkContrainteIK2D, champ, genre, "ik." #champ)
			/// Les attaches d'un emplacement : 4 octets par emplacement, un champ par rang.
#define NK_SQUELETTE_ATTACHE(rang)                                                                                     \
	NkChampSauve {                                                                                                     \
		"emplacements.attaches" #rang, NkTypeChamp::NK_I8,                                                             \
			static_cast<uint32>(offsetof(NkSquelette2D, emplacements) + offsetof(NkEmplacement2D, attaches) + rang),   \
			1u, NK_SQUELETTE2D_EMPLACEMENTS_MAX, static_cast<uint32>(sizeof(NkEmplacement2D)), false                   \
	}

			const NkChampSauve kChampsSquelette[] = {
				NK_UNKENY_CHAMP(NkSquelette2D, nbOs, NkTypeChamp::NK_U8),
				NK_UNKENY_CHAMP(NkSquelette2D, nbEmplacements, NkTypeChamp::NK_U8),
				NK_UNKENY_CHAMP(NkSquelette2D, nbChaines, NkTypeChamp::NK_U8),
				NK_UNKENY_CHAMP(NkSquelette2D, nbIK, NkTypeChamp::NK_U8),
				NK_UNKENY_CHAMP(NkSquelette2D, osVisibles, NkTypeChamp::NK_BOOL),
				NK_UNKENY_CHAMP(NkSquelette2D, source, NkTypeChamp::NK_TEXTE),
				NK_SQUELETTE_OS(nom, NkTypeChamp::NK_TEXTE),
				NK_SQUELETTE_OS(parent, NkTypeChamp::NK_I8),
				NK_SQUELETTE_OS(x, NkTypeChamp::NK_F32),
				NK_SQUELETTE_OS(y, NkTypeChamp::NK_F32),
				NK_SQUELETTE_OS(angle, NkTypeChamp::NK_F32),
				NK_SQUELETTE_OS(longueur, NkTypeChamp::NK_F32),
				NK_SQUELETTE_OS(ex, NkTypeChamp::NK_F32),
				NK_SQUELETTE_OS(ey, NkTypeChamp::NK_F32),
				NK_SQUELETTE_OS(px, NkTypeChamp::NK_F32),
				NK_SQUELETTE_OS(py, NkTypeChamp::NK_F32),
				NK_SQUELETTE_OS(pangle, NkTypeChamp::NK_F32),
				NK_SQUELETTE_OS(pex, NkTypeChamp::NK_F32),
				NK_SQUELETTE_OS(pey, NkTypeChamp::NK_F32),
				NK_SQUELETTE_EMPL(nom, NkTypeChamp::NK_TEXTE),
				NK_SQUELETTE_EMPL(os, NkTypeChamp::NK_I8),
				NK_SQUELETTE_ATTACHE(0),
				NK_SQUELETTE_ATTACHE(1),
				NK_SQUELETTE_ATTACHE(2),
				NK_SQUELETTE_ATTACHE(3),
				NK_SQUELETTE_EMPL(attache, NkTypeChamp::NK_I8),
				NK_SQUELETTE_EMPL(attacheRepos, NkTypeChamp::NK_I8),
				NK_SQUELETTE_EMPL(ordre, NkTypeChamp::NK_I16),
				NK_SQUELETTE_EMPL(ordreRepos, NkTypeChamp::NK_I16),
				NK_SQUELETTE_CHAINE(premier, NkTypeChamp::NK_I8),
				NK_SQUELETTE_CHAINE(nombre, NkTypeChamp::NK_U8),
				NK_SQUELETTE_CHAINE(decor, NkTypeChamp::NK_BOOL),
				NK_SQUELETTE_CHAINE(raideur, NkTypeChamp::NK_F32),
				NK_SQUELETTE_CHAINE(flexion, NkTypeChamp::NK_F32),
				NK_SQUELETTE_CHAINE(amortissement, NkTypeChamp::NK_F32),
				NK_SQUELETTE_CHAINE(masse, NkTypeChamp::NK_F32),
				NK_SQUELETTE_CHAINE(trainee, NkTypeChamp::NK_F32),
				NK_SQUELETTE_IK(os, NkTypeChamp::NK_I8),
				NK_SQUELETTE_IK(cible, NkTypeChamp::NK_I8),
				NK_SQUELETTE_IK(coudePositif, NkTypeChamp::NK_BOOL),
				NK_SQUELETTE_IK(melange, NkTypeChamp::NK_F32),
			};
#undef NK_SQUELETTE_OS
#undef NK_SQUELETTE_EMPL
#undef NK_SQUELETTE_CHAINE
#undef NK_SQUELETTE_IK
#undef NK_SQUELETTE_ATTACHE
		} // namespace

		const NkChampSauve *NkChampsSquelette2D(uint32 &nombre) noexcept {
			nombre = static_cast<uint32>(sizeof(kChampsSquelette) / sizeof(kChampsSquelette[0]));
			return kChampsSquelette;
		}

	} // namespace unkeny
} // namespace nkentseu
