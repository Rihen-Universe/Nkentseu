//
// NkColRay2D.h
// =============================================================================
// Description :
//   Lancer de rayon EXACT contre chaque forme 2D (boite orientee, capsule,
//   segment, triangle, polygone convexe, chaine). Avant ce fichier, le seul
//   rayon 2D exact etait celui du cercle : toute autre forme etait touchee sur
//   sa boite ENGLOBANTE (NkWorld::Raycast2D, garde sous le nom Raycast2DBoites).
//
// Caracteristiques :
//   - Fonctions libres, en-tete seul, zero STL, zero allocation.
//   - Convention de toutes les fonctions : `hit.t` est la distance le long de
//     `ray.dir` (supposee UNITAIRE), `hit.normal` la normale SORTANTE de la
//     forme au point touche, `hit.point` le point. Rend false au-dela de maxT.
//   - Un rayon qui PART dans un polygone (boite, triangle, polygone) le touche a
//     t = 0, normale opposee au rayon : c'est ce qu'attend un test « suis-je
//     dans un mur ». ⚠️ Le cercle existant (NkRayCircle2D) rend, lui, le point
//     de SORTIE : son comportement est garde tel quel.
//
// Algorithmes implementes :
//   - Cyrus-Beck (1978) pour les polygones convexes, orientation detectee
//   - Plaques (« slabs ») dans le repere local pour la boite orientee
//   - Capsule = rectangle du segment + deux cercles, plus petit t retenu
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_NKCOLLISION_NKCOLRAY2D_H__
#define __NKENTSEU_NKCOLLISION_NKCOLRAY2D_H__

#include "NKCollision/NkColTests.h"

namespace nkentseu {
	namespace collision {

		/// Rayon contre un polygone convexe donne par ses sommets (z ignore),
		/// dans n'importe quel sens de parcours.
		inline bool NkRayPolygon2D(const NkRay2D &ray, const NkVec2f *v, uint32 n, NkRayHit2D &hit) noexcept {
			if (v == nullptr || n < 3u) {
				return false;
			}
			float32 aire = 0.f;
			for (uint32 i = 0; i < n; ++i) {
				const NkVec2f &a = v[i];
				const NkVec2f &b = v[(i + 1u) % n];
				aire += a.x * b.y - a.y * b.x;
			}
			// Sens horaire : la normale sortante est de l'autre cote de l'arete.
			const float32 sens = aire < 0.f ? -1.f : 1.f;
			float32 tEntree = 0.f;
			float32 tSortie = ray.maxT;
			NkVec2f normale(-ray.dir.x, -ray.dir.y);
			for (uint32 i = 0; i < n; ++i) {
				const NkVec2f &a = v[i];
				const NkVec2f &b = v[(i + 1u) % n];
				const NkVec2f e = b - a;
				NkVec2f nrm(e.y * sens, -e.x * sens);
				const float32 l = NkLen(nrm);
				if (l < 1.0e-12f) {
					continue; // arete degeneree
				}
				nrm = nrm * (1.f / l);
				// Demi-plan interieur : dot(nrm, x - a) <= 0.
				const float32 num = nrm.Dot(a - ray.origin);
				const float32 den = nrm.Dot(ray.dir);
				if (den > -1.0e-12f && den < 1.0e-12f) {
					if (num < 0.f) {
						return false; // parallele et DEHORS de cette arete
					}
					continue;
				}
				const float32 t = num / den;
				if (den < 0.f) {
					if (t > tEntree) {
						tEntree = t;
						normale = nrm;
					}
				} else if (t < tSortie) {
					tSortie = t;
				}
				if (tEntree > tSortie) {
					return false;
				}
			}
			if (tEntree > ray.maxT) {
				return false;
			}
			hit.hit = true;
			hit.t = tEntree;
			hit.point = ray.origin + ray.dir * tEntree;
			hit.normal = normale;
			return true;
		}

		/// Boite orientee : centre, demi-extents, angle (radians).
		inline bool NkRayOBB2D(const NkRay2D &ray, const NkVec2f &c, const NkVec2f &h, float32 angle, NkRayHit2D &hit) noexcept {
			const float32 co = math::NkCos(angle);
			const float32 si = math::NkSin(angle);
			const NkVec2f d0 = ray.origin - c;
			// Repere local : rotation de -angle.
			const NkVec2f o(d0.x * co + d0.y * si, -d0.x * si + d0.y * co);
			const NkVec2f dir(ray.dir.x * co + ray.dir.y * si, -ray.dir.x * si + ray.dir.y * co);
			float32 tmin = 0.f;
			float32 tmax = ray.maxT;
			NkVec2f nl(-dir.x, -dir.y); // depart dedans : oppose au rayon
			for (int32 i = 0; i < 2; ++i) {
				const float32 oi = i == 0 ? o.x : o.y;
				const float32 di = i == 0 ? dir.x : dir.y;
				const float32 hi = i == 0 ? h.x : h.y;
				if (di > -1.0e-12f && di < 1.0e-12f) {
					if (oi < -hi || oi > hi) {
						return false;
					}
					continue;
				}
				const float32 inv = 1.f / di;
				float32 t1 = (-hi - oi) * inv;
				float32 t2 = (hi - oi) * inv;
				float32 signe = -1.f; // on entre par la face NEGATIVE
				if (t1 > t2) {
					const float32 t = t1;
					t1 = t2;
					t2 = t;
					signe = 1.f;
				}
				if (t1 > tmin) {
					tmin = t1;
					nl = i == 0 ? NkVec2f(signe, 0.f) : NkVec2f(0.f, signe);
				}
				if (t2 < tmax) {
					tmax = t2;
				}
				if (tmin > tmax) {
					return false;
				}
			}
			hit.hit = true;
			hit.t = tmin;
			hit.point = ray.origin + ray.dir * tmin;
			hit.normal = NkVec2f(nl.x * co - nl.y * si, nl.x * si + nl.y * co);
			return true;
		}

		/// Segment d'epaisseur nulle : la normale regarde l'origine du rayon.
		inline bool NkRaySegment2D(const NkRay2D &ray, const NkVec2f &a, const NkVec2f &b, NkRayHit2D &hit) noexcept {
			const NkVec2f s = b - a;
			const float32 den = ray.dir.x * s.y - ray.dir.y * s.x;
			if (den > -1.0e-12f && den < 1.0e-12f) {
				return false; // parallele : un segment n'a pas d'epaisseur a toucher
			}
			const NkVec2f q = a - ray.origin;
			const float32 t = (q.x * s.y - q.y * s.x) / den;
			const float32 u = (q.x * ray.dir.y - q.y * ray.dir.x) / den;
			if (t < 0.f || t > ray.maxT || u < 0.f || u > 1.f) {
				return false;
			}
			NkVec2f n(-s.y, s.x);
			const float32 l = NkLen(n);
			n = l > 1.0e-12f ? n * (1.f / l) : NkVec2f(0.f, 1.f);
			if (n.Dot(ray.dir) > 0.f) {
				n = n * -1.f;
			}
			hit.hit = true;
			hit.t = t;
			hit.point = ray.origin + ray.dir * t;
			hit.normal = n;
			return true;
		}

		/// Capsule : segment [a, b] gonfle de r.
		inline bool NkRayCapsule2D(const NkRay2D &ray, const NkVec2f &a, const NkVec2f &b, float32 r, NkRayHit2D &hit) noexcept {
			bool trouve = false;
			NkRayHit2D meilleur;
			meilleur.t = ray.maxT;
			const NkVec2f ab = b - a;
			const float32 l = NkLen(ab);
			if (l > 1.0e-9f) {
				const float32 angle = math::NkAtan2(ab.y, ab.x);
				NkRayHit2D h;
				if (NkRayOBB2D(ray, (a + b) * 0.5f, NkVec2f(l * 0.5f, r), angle, h) && h.t <= meilleur.t) {
					// Le rectangle ne compte que par ses FLANCS : un coin du rectangle
					// est dans l'arrondi, que les cercles traitent exactement.
					const float32 u = (h.point - a).Dot(ab) / (l * l);
					if (u >= 0.f && u <= 1.f) {
						meilleur = h;
						trouve = true;
					}
				}
			}
			const NkVec2f bouts[2] = {a, b};
			for (int32 k = 0; k < 2; ++k) {
				NkRayHit2D h;
				// Depart DANS le cercle : NkRayCircle2D rend la sortie, pas le choc.
				const NkVec2f d = ray.origin - bouts[k];
				if (d.Dot(d) <= r * r) {
					h.hit = true;
					h.t = 0.f;
					h.point = ray.origin;
					h.normal = NkVec2f(-ray.dir.x, -ray.dir.y);
					meilleur = h;
					trouve = true;
					continue;
				}
				if (NkRayCircle2D(ray, bouts[k], r, h) && h.t < meilleur.t) {
					meilleur = h;
					trouve = true;
				}
			}
			if (trouve) {
				hit = meilleur;
			}
			return trouve;
		}

		/// Rayon contre UNE forme 2D quelconque. Les formes sans surface (point)
		/// ou composees (compound 2D) rendent false : l'appelant decide.
		inline bool NkRayShape2D(const NkRay2D &ray, const NkShape &s, NkRayHit2D &hit) noexcept {
			switch (s.type) {
				case NkShapeType::NK_CIRCLE2D:
					return NkRayCircle2D(ray, NkVec2f(s.p0.x, s.p0.y), s.radius, hit);
				case NkShapeType::NK_BOX2D:
					return NkRayOBB2D(ray, NkVec2f(s.p0.x, s.p0.y), NkVec2f(s.p1.x, s.p1.y), s.rotation, hit);
				case NkShapeType::NK_SEGMENT2D:
					return NkRaySegment2D(ray, NkVec2f(s.p0.x, s.p0.y), NkVec2f(s.p1.x, s.p1.y), hit);
				case NkShapeType::NK_CAPSULE2D:
					return NkRayCapsule2D(ray, NkVec2f(s.p0.x, s.p0.y), NkVec2f(s.p1.x, s.p1.y), s.radius, hit);
				case NkShapeType::NK_TRIANGLE2D:
				case NkShapeType::NK_POLYGON2D: {
					if (s.verts == nullptr || s.vertCount < 3u) {
						return false;
					}
					// 16 sommets suffisent : NkColClip n'en lit deja que 8.
					NkVec2f v[16];
					const uint32 n = s.vertCount < 16u ? s.vertCount : 16u;
					for (uint32 i = 0; i < n; ++i) {
						v[i] = NkVec2f(s.verts[i].x, s.verts[i].y);
					}
					return NkRayPolygon2D(ray, v, n, hit);
				}
				case NkShapeType::NK_CHAIN2D: {
					bool trouve = false;
					NkRayHit2D meilleur;
					meilleur.t = ray.maxT;
					for (uint32 i = 0; s.verts != nullptr && i + 1u < s.vertCount; ++i) {
						NkRayHit2D h;
						if (NkRaySegment2D(ray, NkVec2f(s.verts[i].x, s.verts[i].y), NkVec2f(s.verts[i + 1u].x, s.verts[i + 1u].y), h) &&
							h.t < meilleur.t) {
							meilleur = h;
							trouve = true;
						}
					}
					if (trouve) {
						hit = meilleur;
					}
					return trouve;
				}
				default:
					return false;
			}
		}

	} // namespace collision
} // namespace nkentseu

#endif // __NKENTSEU_NKCOLLISION_NKCOLRAY2D_H__
