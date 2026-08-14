// =============================================================================
// NKAnimPhysics/NkBalance.cpp — implémentation (voir NkBalance.h)
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKAnimPhysics/NkBalance.h"

namespace nkentseu {
	namespace renderer {

		using math::NkVec2f;
		using math::NkVec3f;

		// --- petites primitives vectorielles (composante par composante, sans dépendance API) ---
		static inline float32 Dot3(const NkVec3f &a, const NkVec3f &b) {
			return a.x * b.x + a.y * b.y + a.z * b.z;
		}

		static inline NkVec3f Cross3(const NkVec3f &a, const NkVec3f &b) {
			return NkVec3f{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
		}

		static inline NkVec3f Normalize3(const NkVec3f &a) {
			const float32 l2 = Dot3(a, a);
			if (l2 <= 1e-12f)
				return NkVec3f{0.f, 0.f, 0.f};
			const float32 inv = 1.0f / math::NkSqrt(l2);
			return NkVec3f{a.x * inv, a.y * inv, a.z * inv};
		}

		// Base tangente orthonormée (u,v) du plan de normale n.
		static void PlaneBasis(const NkVec3f &nIn, NkVec3f &u, NkVec3f &v) {
			NkVec3f n = Normalize3(nIn);
			if (Dot3(n, n) <= 1e-12f)
				n = NkVec3f{0.f, 1.f, 0.f};
			// vecteur d'aide non parallèle à n
			NkVec3f helper = (math::NkAbs(n.x) < 0.9f) ? NkVec3f{1.f, 0.f, 0.f} : NkVec3f{0.f, 0.f, 1.f};
			u = Normalize3(Cross3(helper, n));
			v = Cross3(n, u); // déjà unitaire (n,u orthonormés)
		}

		static inline NkVec2f ProjectTo(const NkVec3f &p, const NkVec3f &u, const NkVec3f &v) {
			return NkVec2f{Dot3(p, u), Dot3(p, v)};
		}

		// cross 2D orienté : (a-o) × (b-o).
		static inline float32 Cross2(const NkVec2f &o, const NkVec2f &a, const NkVec2f &b) {
			return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
		}

		// Tri croissant (x puis y) par insertion (petits comptes).
		static void SortByXY(NkVector<NkVec2f> &p) {
			for (int64 i = 1; i < (int64)p.Size(); ++i) {
				NkVec2f key = p[(nk_size)i];
				int64 j = i - 1;
				while (j >= 0 && (p[(nk_size)j].x > key.x ||
								  (p[(nk_size)j].x == key.x && p[(nk_size)j].y > key.y))) {
					p[(nk_size)(j + 1)] = p[(nk_size)j];
					--j;
				}
				p[(nk_size)(j + 1)] = key;
			}
		}

		// Enveloppe convexe 2D (Andrew monotone chain), sortie CCW.
		static void ConvexHull(NkVector<NkVec2f> pts, NkVector<NkVec2f> &hull) {
			hull.Clear();
			const int64 n = (int64)pts.Size();
			if (n < 3) {
				for (int64 i = 0; i < n; ++i)
					hull.PushBack(pts[(nk_size)i]);
				return;
			}
			SortByXY(pts);
			NkVector<NkVec2f> h;
			// bas
			for (int64 i = 0; i < n; ++i) {
				while (h.Size() >= 2 &&
					   Cross2(h[h.Size() - 2], h[h.Size() - 1], pts[(nk_size)i]) <= 0.f)
					h.PopBack();
				h.PushBack(pts[(nk_size)i]);
			}
			// haut
			const int64 lower = (int64)h.Size() + 1;
			for (int64 i = n - 2; i >= 0; --i) {
				while ((int64)h.Size() >= lower &&
					   Cross2(h[h.Size() - 2], h[h.Size() - 1], pts[(nk_size)i]) <= 0.f)
					h.PopBack();
				h.PushBack(pts[(nk_size)i]);
			}
			// retire le dernier (= premier dupliqué)
			for (int64 i = 0; i + 1 < (int64)h.Size(); ++i)
				hull.PushBack(h[(nk_size)i]);
		}

		// Distance signée d'un point q à un SEGMENT [a,b] (toujours >= 0 en magnitude ; renvoyée
		// négative car un segment n'a pas d'aire → jamais "dedans" au sens strict).
		static float32 DistToSegment(const NkVec2f &q, const NkVec2f &a, const NkVec2f &b) {
			const float32 dx = b.x - a.x, dy = b.y - a.y;
			const float32 len2 = dx * dx + dy * dy;
			float32 t = 0.f;
			if (len2 > 1e-12f)
				t = ((q.x - a.x) * dx + (q.y - a.y) * dy) / len2;
			if (t < 0.f)
				t = 0.f;
			if (t > 1.f)
				t = 1.f;
			const float32 px = a.x + t * dx, py = a.y + t * dy;
			const float32 ex = q.x - px, ey = q.y - py;
			return math::NkSqrt(ex * ex + ey * ey);
		}

		NkBalanceResult NkBalance::EvaluateStatic(const NkVec3f &com, const NkVec3f *supportPts, int32 count,
												  const NkVec3f &groundNormal) {
			NkBalanceResult r;
			NkVec3f u, v;
			PlaneBasis(groundNormal, u, v);
			const NkVec2f q = ProjectTo(com, u, v);
			r.comOnPlane = q;

			if (!supportPts || count <= 0) {
				r.balanced = false;
				r.margin = -1e30f;
				r.supportCount = 0;
				return r;
			}

			NkVector<NkVec2f> proj;
			proj.Reserve((nk_size)count);
			for (int32 i = 0; i < count; ++i)
				proj.PushBack(ProjectTo(supportPts[i], u, v));

			// Cas dégénérés.
			if (count == 1) {
				const float32 ex = q.x - proj[0].x, ey = q.y - proj[0].y;
				const float32 d = math::NkSqrt(ex * ex + ey * ey);
				r.supportCount = 1;
				r.margin = -d;
				r.balanced = (d < 1e-4f);
				return r;
			}

			NkVector<NkVec2f> hull;
			ConvexHull(proj, hull);
			r.supportCount = (int32)hull.Size();

			if (hull.Size() < 3) {
				// segment (ou points colinéaires) : pas d'aire → marge = -distance au segment.
				const NkVec2f a = hull[0];
				const NkVec2f b = hull[hull.Size() - 1];
				const float32 d = DistToSegment(q, a, b);
				r.margin = -d;
				r.balanced = (d < 1e-4f);
				return r;
			}

			// Polygone convexe CCW : marge = min sur les arêtes de la distance signée (>0 = à gauche/dedans).
			const int64 m = (int64)hull.Size();
			float32 minSigned = 1e30f;
			for (int64 i = 0; i < m; ++i) {
				const NkVec2f a = hull[(nk_size)i];
				const NkVec2f b = hull[(nk_size)((i + 1) % m)];
				const float32 ex = b.x - a.x, ey = b.y - a.y;
				const float32 len = math::NkSqrt(ex * ex + ey * ey);
				if (len <= 1e-9f)
					continue;
				const float32 signedDist = ((b.x - a.x) * (q.y - a.y) - (b.y - a.y) * (q.x - a.x)) / len;
				if (signedDist < minSigned)
					minSigned = signedDist;
			}
			r.margin = minSigned;
			r.balanced = (minSigned >= 0.f);
			return r;
		}

		NkVec2f NkBalance::TipDirection(const NkVec3f &comVelocity, const NkVec3f &groundNormal) {
			NkVec3f u, v;
			PlaneBasis(groundNormal, u, v);
			NkVec2f d = ProjectTo(comVelocity, u, v);
			const float32 l2 = d.x * d.x + d.y * d.y;
			if (l2 <= 1e-10f)
				return NkVec2f{0.f, 0.f};
			const float32 inv = 1.0f / math::NkSqrt(l2);
			return NkVec2f{d.x * inv, d.y * inv};
		}

		bool NkBalance::SelfTest() {
			const float32 eps = 1e-3f;
			const NkVec3f up{0.f, 1.f, 0.f};

			// Support = carré 2×2 au sol (y=0) centré sur l'origine (4 coins).
			NkVec3f square[4] = {NkVec3f{-1.f, 0.f, -1.f}, NkVec3f{1.f, 0.f, -1.f}, NkVec3f{1.f, 0.f, 1.f},
								 NkVec3f{-1.f, 0.f, 1.f}};

			// 1) COM au-dessus du centre -> équilibré, marge ~1 (distance au bord le plus proche).
			{
				NkBalanceResult r = NkBalance::EvaluateStatic(NkVec3f{0.f, 1.7f, 0.f}, square, 4, up);
				if (!r.balanced)
					return false;
				if (r.margin < 1.f - eps || r.margin > 1.f + eps)
					return false;
				if (r.supportCount != 4)
					return false;
			}

			// 2) COM au-delà du bord (x=2) -> déséquilibré, marge négative (~-1).
			{
				NkBalanceResult r = NkBalance::EvaluateStatic(NkVec3f{2.f, 1.7f, 0.f}, square, 4, up);
				if (r.balanced)
					return false;
				if (r.margin > 0.f)
					return false;
			}

			// 3) COM pile sur un bord (x=1) -> marge ~0, limite d'équilibre.
			{
				NkBalanceResult r = NkBalance::EvaluateStatic(NkVec3f{1.f, 1.7f, 0.f}, square, 4, up);
				if (r.margin > eps || r.margin < -eps)
					return false;
			}

			// 4) Deux pieds (segment) : COM sur la ligne -> ~équilibré ; à côté -> déséquilibré.
			{
				NkVec3f feet[2] = {NkVec3f{-0.5f, 0.f, 0.f}, NkVec3f{0.5f, 0.f, 0.f}};
				NkBalanceResult on = NkBalance::EvaluateStatic(NkVec3f{0.f, 1.f, 0.f}, feet, 2, up);
				NkBalanceResult off = NkBalance::EvaluateStatic(NkVec3f{0.f, 1.f, 0.6f}, feet, 2, up);
				if (!on.balanced)
					return false; // COM sur le segment
				if (off.balanced)
					return false; // COM à 0.6 m de côté
				if (off.margin > -0.5f)
					return false; // ~ -0.6
			}

			// 5) TipDirection : vitesse horizontale -> vecteur unité (base-indépendant) ;
			//    vitesse purement verticale -> {0,0}.
			{
				NkVec2f d = NkBalance::TipDirection(NkVec3f{3.f, 5.f, 0.f}, up); // composante sol non nulle
				const float32 len = math::NkSqrt(d.x * d.x + d.y * d.y);
				if (len < 1.f - eps || len > 1.f + eps)
					return false;
				NkVec2f z = NkBalance::TipDirection(NkVec3f{0.f, 9.f, 0.f}, up); // vertical pur
				if (z.x != 0.f || z.y != 0.f)
					return false;
			}

			return true;
		}

	} // namespace renderer
} // namespace nkentseu
