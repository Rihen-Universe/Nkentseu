// =============================================================================
// NkTerrainSable.cpp — NKRenderer / Mesh
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
#include "NkTerrainSable.h"

#include <math.h>

namespace nkentseu {
	namespace renderer {

		const char *NkSableStatutNom(NkSableStatut s) {
			switch (s) {
				case NkSableStatut::NK_OK:
					return "NK_OK";
				case NkSableStatut::NK_CHAMP_INVALIDE:
					return "NK_CHAMP_INVALIDE";
				case NkSableStatut::NK_DIMENSIONS_INCOMPATIBLES:
					return "NK_DIMENSIONS_INCOMPATIBLES";
				case NkSableStatut::NK_PARAMETRE_HORS_DOMAINE:
					return "NK_PARAMETRE_HORS_DOMAINE";
			}
			return "?";
		}

		// ─────────────────────────────────────────────────────────────────────
		//  LOIS
		// ─────────────────────────────────────────────────────────────────────

		float32 NkSableDeniveleMax(const NkTerrainSableParams &p, float32 pas) {
			if (pas <= 0.f)
				return 0.f;
			// 🔴 LE GARDE DE 90°, ET SA RAISON EST DANS L'EN-TÊTE.
			// `tanf` près de π/2 rend un nombre fini énorme dont le SIGNE n'est pas
			// garanti (π/2 n'est pas représentable en float32). Un dhMax négatif
			// inverserait le négatif de (s3) : le tas s'effondrerait complètement
			// là où l'on exige qu'il ne bouge pas. On coupe AVANT tanf.
			if (p.angleReposDeg >= 89.9f)
				return 3.402823466e+38f; // ~FLT_MAX : aucune paire ne sera jamais excédentaire
			if (p.angleReposDeg <= 0.f)
				return 0.f; // tout dénivelé est excédentaire : le tas s'aplatit totalement
			const float32 rad = p.angleReposDeg * 3.14159265358979323846f / 180.f;
			return pas * tanf(rad);
		}

		// ─────────────────────────────────────────────────────────────────────
		//  INITIALISATION
		// ─────────────────────────────────────────────────────────────────────

		static void SableAllouer(NkTerrainSable &c, uint32 N, uint32 M) {
			const uint32 n = N * M;
			c.N = N;
			c.M = M;
			c.base.Resize(n);
			c.depot.Resize(n);
			c.combine.Resize(n);
			c.deformabilite.Resize(n);
		}

		NkSableStatut NkSableInitDepuisNiveaux(const float32 *niveaux, uint32 N, uint32 M,
											   const NkTerrainParams &p, NkTerrainSable &out) {
			if (niveaux == nullptr || N < 2u || M < 2u)
				return NkSableStatut::NK_CHAMP_INVALIDE;
			if (p.pasX <= 0.f || p.pasZ <= 0.f)
				return NkSableStatut::NK_PARAMETRE_HORS_DOMAINE;

			SableAllouer(out, N, M);
			out.pasX = p.pasX;
			out.pasZ = p.pasZ;
			// MÊME origine que `NkTerrainDepuisNiveaux` (NkTerrainHeightMap.cpp:90-92).
			// Recopiée et non devinée : deux conventions d'origine feraient poser
			// l'ornière à côté de la roue, sans qu'aucun nombre ne le dise.
			out.x0 = p.centre ? -0.5f * (float32)(N - 1u) * p.pasX : 0.f;
			out.z0 = p.centre ? -0.5f * (float32)(M - 1u) * p.pasZ : 0.f;

			const uint32 n = N * M;
			for (uint32 k = 0; k < n; ++k) {
				out.base[k] = NkTerrainHauteurDepuisNiveau(p, niveaux[k]);
				out.depot[k] = 0.f;
				out.combine[k] = out.base[k]; // chemin zéro : recopie, pas addition
				out.deformabilite[k] = 1.f;
			}
			return NkSableStatut::NK_OK;
		}

		NkSableStatut NkSableAdopterBaseDuMaillage(const NkEditMesh &mesh, NkTerrainSable &champ) {
			if (!champ.Valide())
				return NkSableStatut::NK_CHAMP_INVALIDE;
			const uint32 n = champ.Taille();
			if ((uint32)mesh.verts.Size() != n)
				return NkSableStatut::NK_DIMENSIONS_INCOMPATIBLES;
			for (uint32 k = 0; k < n; ++k) {
				// RECOPIE, pas recalcul : c'est tout l'objet de cette porte.
				champ.base[k] = mesh.verts[k].pos.y;
				champ.combine[k] = NkSableHauteurCombinee(champ.base[k], champ.depot[k]);
			}
			return NkSableStatut::NK_OK;
		}

		NkSableStatut NkSableDeformabiliteDepuisPoids(const NkTerrainPoids *poids, uint32 nbPoids,
													  NkTerrainSable &champ) {
			if (!champ.Valide())
				return NkSableStatut::NK_CHAMP_INVALIDE;
			if (poids == nullptr || nbPoids != champ.Taille())
				return NkSableStatut::NK_DIMENSIONS_INCOMPATIBLES;
			for (uint32 k = 0; k < nbPoids; ++k) {
				// NK_TERRAIN_ROCHE vaut 2, pas 0 — voir la correction écrite dans
				// l'en-tête. Une roche PURE (w = 1) donne exactement 0.f.
				float32 d = 1.f - poids[k].w[NK_TERRAIN_ROCHE];
				if (d < 0.f)
					d = 0.f;
				if (d > 1.f)
					d = 1.f;
				champ.deformabilite[k] = d;
			}
			return NkSableStatut::NK_OK;
		}

		NkSableStatut NkSableDeformabiliteUniforme(NkTerrainSable &champ, float32 valeur) {
			if (!champ.Valide())
				return NkSableStatut::NK_CHAMP_INVALIDE;
			if (valeur < 0.f || valeur > 1.f)
				return NkSableStatut::NK_PARAMETRE_HORS_DOMAINE;
			const uint32 n = champ.Taille();
			for (uint32 k = 0; k < n; ++k)
				champ.deformabilite[k] = valeur;
			return NkSableStatut::NK_OK;
		}

		NkSableStatut NkSableReinitialiser(NkTerrainSable &champ) {
			if (!champ.Valide())
				return NkSableStatut::NK_CHAMP_INVALIDE;
			const uint32 n = champ.Taille();
			for (uint32 k = 0; k < n; ++k) {
				champ.depot[k] = 0.f;
				champ.combine[k] = champ.base[k]; // recopie, pas addition
			}
			return NkSableStatut::NK_OK;
		}

		// ─────────────────────────────────────────────────────────────────────
		//  GÉOMÉTRIE DE L'EMPREINTE
		// ─────────────────────────────────────────────────────────────────────

		// Distance d'un point au SEGMENT [A,B], dans le plan XZ. Un segment et non
		// une droite : une trace de roue s'arrête là où la roue s'est arrêtée.
		static float32 DistanceAuSegment(float32 px, float32 pz, float32 ax, float32 az, float32 bx,
										 float32 bz) {
			const float32 vx = bx - ax, vz = bz - az;
			const float32 len2 = vx * vx + vz * vz;
			float32 t = 0.f;
			if (len2 > 0.f) {
				t = ((px - ax) * vx + (pz - az) * vz) / len2;
				if (t < 0.f)
					t = 0.f;
				if (t > 1.f)
					t = 1.f;
			}
			const float32 dx = px - (ax + t * vx), dz = pz - (az + t * vz);
			return sqrtf(dx * dx + dz * dz);
		}

		// Plage de cellules couvrant une boîte du monde, bornée à la grille.
		struct PlageCellules {
				uint32 i0, i1, j0, j1;
				bool vide;
		};

		static PlageCellules PlageDe(const NkTerrainSable &c, float32 xmin, float32 xmax, float32 zmin,
									 float32 zmax) {
			PlageCellules r{0u, 0u, 0u, 0u, true};
			const float32 fi0 = (xmin - c.x0) / c.pasX;
			const float32 fi1 = (xmax - c.x0) / c.pasX;
			const float32 fj0 = (zmin - c.z0) / c.pasZ;
			const float32 fj1 = (zmax - c.z0) / c.pasZ;
			// Entièrement hors grille : on le dit, on ne clampe pas en silence. Un
			// clamp rendrait une empreinte plaquée sur le bord du terrain, ce qui
			// ressemble à un résultat.
			if (fi1 < 0.f || fj1 < 0.f || fi0 > (float32)(c.N - 1u) || fj0 > (float32)(c.M - 1u))
				return r;
			int32 i0 = (int32)floorf(fi0), i1 = (int32)ceilf(fi1);
			int32 j0 = (int32)floorf(fj0), j1 = (int32)ceilf(fj1);
			if (i0 < 0)
				i0 = 0;
			if (j0 < 0)
				j0 = 0;
			if (i1 > (int32)c.N - 1)
				i1 = (int32)c.N - 1;
			if (j1 > (int32)c.M - 1)
				j1 = (int32)c.M - 1;
			if (i0 > i1 || j0 > j1)
				return r;
			r.i0 = (uint32)i0;
			r.i1 = (uint32)i1;
			r.j0 = (uint32)j0;
			r.j1 = (uint32)j1;
			r.vide = false;
			return r;
		}

		NkSableStatut NkSableEmpreinteCapsule(NkTerrainSable &champ, const NkTerrainSableParams &p,
											  float32 ax, float32 az, float32 bx, float32 bz,
											  float32 demiLargeur, float32 profondeur,
											  float32 *outVolumeRetire, float32 *outVolumeRedepose) {
			if (outVolumeRetire)
				*outVolumeRetire = 0.f;
			if (outVolumeRedepose)
				*outVolumeRedepose = 0.f;
			if (!champ.Valide())
				return NkSableStatut::NK_CHAMP_INVALIDE;
			if (demiLargeur <= 0.f || p.largeurBourrelet < 1.f)
				return NkSableStatut::NK_PARAMETRE_HORS_DOMAINE;
			// Profondeur nulle : ce n'est pas une erreur, c'est une roue qui ne
			// s'enfonce pas. Le champ doit rester INTACT AU BIT — on sort avant
			// d'écrire quoi que ce soit.
			if (profondeur <= 0.f)
				return NkSableStatut::NK_OK;

			const float32 aireCellule = champ.pasX * champ.pasZ;
			const float32 rExt = demiLargeur * p.largeurBourrelet;

			// ── PASSE 1 : ON CREUSE ─────────────────────────────────────────
			const float32 xmin = (ax < bx ? ax : bx) - rExt;
			const float32 xmax = (ax > bx ? ax : bx) + rExt;
			const float32 zmin = (az < bz ? az : bz) - rExt;
			const float32 zmax = (az > bz ? az : bz) + rExt;
			const PlageCellules pl = PlageDe(champ, xmin, xmax, zmin, zmax);
			if (pl.vide)
				return NkSableStatut::NK_OK;

			float32 volumeRetire = 0.f;
			for (uint32 j = pl.j0; j <= pl.j1; ++j) {
				const float32 pz = champ.z0 + (float32)j * champ.pasZ;
				for (uint32 i = pl.i0; i <= pl.i1; ++i) {
					const uint32 k = j * champ.N + i;
					const float32 def = champ.deformabilite[k];
					if (def <= 0.f)
						continue; // roche : aucun creusement, et c'est le négatif de (s2)
					const float32 px = champ.x0 + (float32)i * champ.pasX;
					const float32 q = DistanceAuSegment(px, pz, ax, az, bx, bz) / demiLargeur;
					if (q > 1.f)
						continue;
					// `NkTerrainLisser` vient de NkTerrainSplat.h : le smoothstep du
					// dépôt, pas un second.
					const float32 creuse = profondeur * (1.f - NkTerrainLisser(q, 0.f, 1.f)) * def;
					if (creuse <= 0.f)
						continue;
					const float32 cible = -creuse;
					if (cible < champ.depot[k]) {
						volumeRetire += (champ.depot[k] - cible) * aireCellule;
						champ.depot[k] = cible;
					}
				}
			}

			// ── PASSE 2 : LE BOURRELET, À VOLUME ÉGAL ───────────────────────
			float32 volumeRedepose = 0.f;
			if (volumeRetire > 0.f && p.largeurBourrelet > 1.f) {
				// Somme des poids d'abord : on ne peut pas normaliser en un seul
				// parcours, et une normalisation approchée ferait fuir le volume.
				float32 sommePoids = 0.f;
				for (uint32 j = pl.j0; j <= pl.j1; ++j) {
					const float32 pz = champ.z0 + (float32)j * champ.pasZ;
					for (uint32 i = pl.i0; i <= pl.i1; ++i) {
						const uint32 k = j * champ.N + i;
						const float32 def = champ.deformabilite[k];
						if (def <= 0.f)
							continue;
						const float32 px = champ.x0 + (float32)i * champ.pasX;
						const float32 q = DistanceAuSegment(px, pz, ax, az, bx, bz) / demiLargeur;
						if (q <= 1.f || q > p.largeurBourrelet)
							continue;
						sommePoids += (1.f - NkTerrainLisser(q, 1.f, p.largeurBourrelet)) * def;
					}
				}
				if (sommePoids > 0.f) {
					const float32 hauteurParPoids = volumeRetire / (sommePoids * aireCellule);
					for (uint32 j = pl.j0; j <= pl.j1; ++j) {
						const float32 pz = champ.z0 + (float32)j * champ.pasZ;
						for (uint32 i = pl.i0; i <= pl.i1; ++i) {
							const uint32 k = j * champ.N + i;
							const float32 def = champ.deformabilite[k];
							if (def <= 0.f)
								continue;
							const float32 px = champ.x0 + (float32)i * champ.pasX;
							const float32 q = DistanceAuSegment(px, pz, ax, az, bx, bz) / demiLargeur;
							if (q <= 1.f || q > p.largeurBourrelet)
								continue;
							const float32 w = (1.f - NkTerrainLisser(q, 1.f, p.largeurBourrelet)) * def;
							const float32 ajout = w * hauteurParPoids;
							champ.depot[k] += ajout;
							volumeRedepose += ajout * aireCellule;
						}
					}
				}
				// sommePoids == 0 : l'anneau est entièrement de la roche. Le volume
				// est PERDU, et `outVolumeRedepose` restera à 0. On ne le cache pas :
				// un appelant qui compare les deux nombres le verra.
			}

			// Réécrire `combine` UNIQUEMENT sur la plage touchée : le reste du
			// terrain garde ses octets d'origine, il n'est pas réécrit à
			// l'identique. C'est ce qui rend le négatif capital insensible au
			// nombre d'empreintes posées ailleurs.
			for (uint32 j = pl.j0; j <= pl.j1; ++j)
				for (uint32 i = pl.i0; i <= pl.i1; ++i) {
					const uint32 k = j * champ.N + i;
					champ.combine[k] = NkSableHauteurCombinee(champ.base[k], champ.depot[k]);
				}

			if (outVolumeRetire)
				*outVolumeRetire = volumeRetire;
			if (outVolumeRedepose)
				*outVolumeRedepose = volumeRedepose;
			return NkSableStatut::NK_OK;
		}

		NkSableStatut NkSablePassageRoue(NkTerrainSable &champ, const NkTerrainSableParams &p,
										 float32 ax, float32 az, float32 bx, float32 bz,
										 float32 demiLargeur, float32 longueurContact, float32 masseKg,
										 float32 *outEnfoncement, float32 *outVolumeRetire,
										 float32 *outVolumeRedepose) {
			if (outEnfoncement)
				*outEnfoncement = 0.f;
			if (demiLargeur <= 0.f || longueurContact <= 0.f)
				return NkSableStatut::NK_PARAMETRE_HORS_DOMAINE;
			// L'APPELANT NE DONNE PAS LA PROFONDEUR : elle est dérivée de la masse
			// et de l'aire de contact. C'est ce qui rend (s2) vérifiable — un banc
			// recalcule le même nombre avec la même fonction publique.
			const float32 aireContact = 2.f * demiLargeur * longueurContact;
			const float32 d = NkSableEnfoncement(p, masseKg, aireContact);
			if (outEnfoncement)
				*outEnfoncement = d;
			return NkSableEmpreinteCapsule(champ, p, ax, az, bx, bz, demiLargeur, d, outVolumeRetire,
										   outVolumeRedepose);
		}

		// ─────────────────────────────────────────────────────────────────────
		//  LE TALUS
		// ─────────────────────────────────────────────────────────────────────

		float32 NkSablePenteMaxDeg(const NkTerrainSable &champ) {
			if (!champ.Valide())
				return 0.f;
			float32 tanMax = 0.f;
			for (uint32 j = 0; j < champ.M; ++j) {
				for (uint32 i = 0; i < champ.N; ++i) {
					const uint32 k = j * champ.N + i;
					const float32 h = NkSableHauteurCombinee(champ.base[k], champ.depot[k]);
					if (i + 1u < champ.N) {
						const uint32 k2 = k + 1u;
						const float32 h2 = NkSableHauteurCombinee(champ.base[k2], champ.depot[k2]);
						const float32 t = fabsf(h2 - h) / champ.pasX;
						if (t > tanMax)
							tanMax = t;
					}
					if (j + 1u < champ.M) {
						const uint32 k2 = k + champ.N;
						const float32 h2 = NkSableHauteurCombinee(champ.base[k2], champ.depot[k2]);
						const float32 t = fabsf(h2 - h) / champ.pasZ;
						if (t > tanMax)
							tanMax = t;
					}
				}
			}
			return atanf(tanMax) * 180.f / 3.14159265358979323846f;
		}

		float32 NkSableVolumeDepot(const NkTerrainSable &champ) {
			if (!champ.Valide())
				return 0.f;
			// Somme en float64 : le volume est une somme de N*M termes de signes
			// opposés (le creux et le bourrelet s'annulent presque exactement).
			// En float32 l'annulation catastrophique donnerait un résidu du même
			// ordre que ce qu'on cherche à mesurer, et le critère de conservation
			// mesurerait alors l'arithmétique du banc, pas le code.
			float64 s = 0.0;
			const uint32 n = champ.Taille();
			for (uint32 k = 0; k < n; ++k)
				s += (float64)champ.depot[k];
			return (float32)(s * (float64)champ.pasX * (float64)champ.pasZ);
		}

		uint32 NkSableRelaxer(NkTerrainSable &champ, const NkTerrainSableParams &p, uint32 iterationsMax,
							  float32 *outPenteMaxDeg) {
			if (!champ.Valide()) {
				if (outPenteMaxDeg)
					*outPenteMaxDeg = 0.f;
				return 0u;
			}
			const float32 dhMaxX = NkSableDeniveleMax(p, champ.pasX);
			const float32 dhMaxZ = NkSableDeniveleMax(p, champ.pasZ);
			const float32 fac = p.facteurRelaxation * 0.5f;

			const uint32 n = champ.Taille();
			NkVector<float32> delta;
			delta.Resize(n);

			uint32 iterations = 0;
			for (uint32 it = 0; it < iterationsMax; ++it) {
				for (uint32 k = 0; k < n; ++k)
					delta[k] = 0.f;
				bool bouge = false;

				// JACOBI : on LIT la hauteur courante, on ÉCRIT dans `delta`.
				for (uint32 j = 0; j < champ.M; ++j) {
					for (uint32 i = 0; i < champ.N; ++i) {
						const uint32 k = j * champ.N + i;
						const float32 h = NkSableHauteurCombinee(champ.base[k], champ.depot[k]);
						const float32 defK = champ.deformabilite[k];
						// Deux voisins seulement (droite, bas) : chaque paire est
						// ainsi visitée UNE fois. La visiter deux fois doublerait le
						// transfert sans que rien ne le dise.
						for (int32 axe = 0; axe < 2; ++axe) {
							uint32 k2;
							float32 dhMax;
							if (axe == 0) {
								if (i + 1u >= champ.N)
									continue;
								k2 = k + 1u;
								dhMax = dhMaxX;
							} else {
								if (j + 1u >= champ.M)
									continue;
								k2 = k + champ.N;
								dhMax = dhMaxZ;
							}
							const float32 h2 = NkSableHauteurCombinee(champ.base[k2], champ.depot[k2]);
							const float32 dh = h2 - h;
							const float32 a = dh < 0.f ? -dh : dh;
							if (a <= dhMax)
								continue;
							const float32 def = champ.deformabilite[k2] < defK ? champ.deformabilite[k2]
																			  : defK;
							if (def <= 0.f)
								continue; // la roche ne coule pas
							const float32 transfert = (a - dhMax) * fac * def;
							if (transfert <= 0.f)
								continue;
							// Du HAUT vers le BAS. Ce qu'on retire d'un côté, on
							// l'ajoute de l'autre : le volume est conservé par
							// construction, terme à terme.
							if (dh > 0.f) {
								delta[k2] -= transfert;
								delta[k] += transfert;
							} else {
								delta[k] -= transfert;
								delta[k2] += transfert;
							}
							bouge = true;
						}
					}
				}

				++iterations;
				if (!bouge)
					break;
				for (uint32 k = 0; k < n; ++k) {
					if (delta[k] != 0.f) {
						champ.depot[k] += delta[k];
						champ.combine[k] = NkSableHauteurCombinee(champ.base[k], champ.depot[k]);
					}
				}
			}

			if (outPenteMaxDeg)
				*outPenteMaxDeg = NkSablePenteMaxDeg(champ);
			return iterations;
		}

		NkSableStatut NkSablePoserCone(NkTerrainSable &champ, float32 cx, float32 cz, float32 rayon,
									   float32 hauteur) {
			if (!champ.Valide())
				return NkSableStatut::NK_CHAMP_INVALIDE;
			if (rayon <= 0.f)
				return NkSableStatut::NK_PARAMETRE_HORS_DOMAINE;
			const PlageCellules pl = PlageDe(champ, cx - rayon, cx + rayon, cz - rayon, cz + rayon);
			if (pl.vide)
				return NkSableStatut::NK_OK;
			for (uint32 j = pl.j0; j <= pl.j1; ++j) {
				const float32 pz = champ.z0 + (float32)j * champ.pasZ;
				for (uint32 i = pl.i0; i <= pl.i1; ++i) {
					const uint32 k = j * champ.N + i;
					const float32 px = champ.x0 + (float32)i * champ.pasX;
					const float32 dx = px - cx, dz = pz - cz;
					const float32 r = sqrtf(dx * dx + dz * dz);
					if (r >= rayon)
						continue;
					// Cône EXACT (linéaire), pas un lissage : la pente initiale vaut
					// alors `atan(hauteur / rayon)` partout, donc le banc SAIT ce
					// qu'il a posé avant de mesurer.
					champ.depot[k] += hauteur * (1.f - r / rayon);
					champ.combine[k] = NkSableHauteurCombinee(champ.base[k], champ.depot[k]);
				}
			}
			return NkSableStatut::NK_OK;
		}

		// ─────────────────────────────────────────────────────────────────────
		//  RENDRE VISIBLE
		// ─────────────────────────────────────────────────────────────────────

		NkSableStatut NkSableEcrireSommets(const NkTerrainSable &champ, NkVertex3D *sommets,
										   uint32 nbSommets) {
			if (!champ.Valide())
				return NkSableStatut::NK_CHAMP_INVALIDE;
			if (sommets == nullptr || nbSommets != champ.Taille())
				return NkSableStatut::NK_DIMENSIONS_INCOMPATIBLES;
			for (uint32 k = 0; k < nbSommets; ++k)
				sommets[k].pos.y = NkSableHauteurCombinee(champ.base[k], champ.depot[k]);
			return NkSableStatut::NK_OK;
		}

		NkSableStatut NkSableAppliquerAuMaillage(const NkTerrainSable &champ, NkEditMesh &mesh) {
			if (!champ.Valide())
				return NkSableStatut::NK_CHAMP_INVALIDE;
			const uint32 n = champ.Taille();
			if ((uint32)mesh.verts.Size() != n)
				return NkSableStatut::NK_DIMENSIONS_INCOMPATIBLES;
			for (uint32 k = 0; k < n; ++k)
				mesh.verts[k].pos.y = NkSableHauteurCombinee(champ.base[k], champ.depot[k]);
			// Sans ce recalcul, l'ornière serait creusée mais éclairée comme un sol
			// plat — c'est-à-dire invisible sur l'image, alors que les nombres
			// seraient justes. C'est exactement la famille de défaut que ce lot doit
			// rendre impossible.
			mesh.RecomputeNormals();
			return NkSableStatut::NK_OK;
		}

	} // namespace renderer
} // namespace nkentseu
