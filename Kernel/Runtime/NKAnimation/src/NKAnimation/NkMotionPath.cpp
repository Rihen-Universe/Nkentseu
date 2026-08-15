// =============================================================================
// NKRenderer/Tools/Animation/NkMotionPath.cpp — animation par courbe (voir .h).
// =============================================================================
#include "NKAnimation/NkMotionPath.h"

namespace nkentseu {
	namespace anim {

		using math::NkVec3f;

		namespace {

			NkVec3f Lerp(const NkVec3f &a, const NkVec3f &b, float32 u) {
				return NkVec3f{a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u, a.z + (b.z - a.z) * u};
			}

			float32 Dist(const NkVec3f &a, const NkVec3f &b) {
				const float32 dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
				return math::NkSqrt(dx * dx + dy * dy + dz * dz);
			}

			NkVec3f Normalize(const NkVec3f &v) {
				const float32 l = math::NkSqrt(v.x * v.x + v.y * v.y + v.z * v.z);
				if (l < 1e-8f)
					return NkVec3f{0.f, 0.f, 1.f};
				return NkVec3f{v.x / l, v.y / l, v.z / l};
			}

			// Point de contrôle indexé, avec gestion ouvert/fermé (indices repliés ou clampés).
			NkVec3f Ctrl(const NkVector<NkVec3f> &p, int32 i, bool closed) {
				const int32 n = static_cast<int32>(p.Size());
				if (n == 0)
					return NkVec3f{0.f, 0.f, 0.f};
				if (closed) {
					int32 k = i % n;
					if (k < 0)
						k += n;
					return p[static_cast<uint64>(k)];
				}
				if (i < 0)
					i = 0;
				if (i > n - 1)
					i = n - 1;
				return p[static_cast<uint64>(i)];
			}

			// Nombre de segments (ouvert : n-1 ; fermé : n).
			int32 SegCount(const NkVector<NkVec3f> &p, bool closed) {
				const int32 n = static_cast<int32>(p.Size());
				if (n < 2)
					return 0;
				return closed ? n : n - 1;
			}

			// Catmull-Rom uniforme sur le segment [p1,p2] au paramètre local u.
			NkVec3f CatmullRom(const NkVec3f &p0, const NkVec3f &p1, const NkVec3f &p2, const NkVec3f &p3, float32 u) {
				const float32 u2 = u * u;
				const float32 u3 = u2 * u;
				NkVec3f r;
				r.x = 0.5f * ((2.f * p1.x) + (-p0.x + p2.x) * u + (2.f * p0.x - 5.f * p1.x + 4.f * p2.x - p3.x) * u2 +
							  (-p0.x + 3.f * p1.x - 3.f * p2.x + p3.x) * u3);
				r.y = 0.5f * ((2.f * p1.y) + (-p0.y + p2.y) * u + (2.f * p0.y - 5.f * p1.y + 4.f * p2.y - p3.y) * u2 +
							  (-p0.y + 3.f * p1.y - 3.f * p2.y + p3.y) * u3);
				r.z = 0.5f * ((2.f * p1.z) + (-p0.z + p2.z) * u + (2.f * p0.z - 5.f * p1.z + 4.f * p2.z - p3.z) * u2 +
							  (-p0.z + 3.f * p1.z - 3.f * p2.z + p3.z) * u3);
				return r;
			}

			// Dérivée (tangente non normalisée) du Catmull-Rom au paramètre local u.
			NkVec3f CatmullRomTangent(const NkVec3f &p0, const NkVec3f &p1, const NkVec3f &p2, const NkVec3f &p3,
									  float32 u) {
				const float32 u2 = u * u;
				NkVec3f r;
				r.x = 0.5f * ((-p0.x + p2.x) + 2.f * (2.f * p0.x - 5.f * p1.x + 4.f * p2.x - p3.x) * u +
							  3.f * (-p0.x + 3.f * p1.x - 3.f * p2.x + p3.x) * u2);
				r.y = 0.5f * ((-p0.y + p2.y) + 2.f * (2.f * p0.y - 5.f * p1.y + 4.f * p2.y - p3.y) * u +
							  3.f * (-p0.y + 3.f * p1.y - 3.f * p2.y + p3.y) * u2);
				r.z = 0.5f * ((-p0.z + p2.z) + 2.f * (2.f * p0.z - 5.f * p1.z + 4.f * p2.z - p3.z) * u +
							  3.f * (-p0.z + 3.f * p1.z - 3.f * p2.z + p3.z) * u2);
				return r;
			}

			// Éclate t∈[0,1] en (segment, u local).
			void SplitT(const NkVector<NkVec3f> &p, bool closed, float32 t, int32 &seg, float32 &u) {
				const int32 sc = SegCount(p, closed);
				if (sc <= 0) {
					seg = 0;
					u = 0.f;
					return;
				}
				float32 tt = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
				const float32 f = tt * static_cast<float32>(sc);
				seg = static_cast<int32>(f);
				if (seg >= sc)
					seg = sc - 1;
				u = f - static_cast<float32>(seg);
			}

		} // namespace

		NkVec3f NkMotionCurve::SamplePosition(float32 t) const {
			const int32 n = static_cast<int32>(points.Size());
			if (n == 0)
				return NkVec3f{0.f, 0.f, 0.f};
			if (n == 1)
				return points[0];
			int32 seg;
			float32 u;
			SplitT(points, closed, t, seg, u);
			const NkVec3f p0 = Ctrl(points, seg - 1, closed);
			const NkVec3f p1 = Ctrl(points, seg, closed);
			const NkVec3f p2 = Ctrl(points, seg + 1, closed);
			const NkVec3f p3 = Ctrl(points, seg + 2, closed);
			return CatmullRom(p0, p1, p2, p3, u);
		}

		NkVec3f NkMotionCurve::SampleTangent(float32 t) const {
			const int32 n = static_cast<int32>(points.Size());
			if (n < 2)
				return NkVec3f{0.f, 0.f, 1.f};
			int32 seg;
			float32 u;
			SplitT(points, closed, t, seg, u);
			const NkVec3f p0 = Ctrl(points, seg - 1, closed);
			const NkVec3f p1 = Ctrl(points, seg, closed);
			const NkVec3f p2 = Ctrl(points, seg + 1, closed);
			const NkVec3f p3 = Ctrl(points, seg + 2, closed);
			return Normalize(CatmullRomTangent(p0, p1, p2, p3, u));
		}

		float32 NkMotionCurve::Length(int32 samplesPerSeg) const {
			const int32 sc = SegCount(points, closed);
			if (sc <= 0 || samplesPerSeg < 1)
				return 0.f;
			const int32 total = sc * samplesPerSeg;
			float32 len = 0.f;
			NkVec3f prev = SamplePosition(0.f);
			for (int32 i = 1; i <= total; ++i) {
				const float32 t = static_cast<float32>(i) / static_cast<float32>(total);
				const NkVec3f cur = SamplePosition(t);
				len += Dist(prev, cur);
				prev = cur;
			}
			return len;
		}

		float32 NkMotionCurve::DistanceToT(float32 dist, int32 samplesPerSeg) const {
			const int32 sc = SegCount(points, closed);
			if (sc <= 0 || samplesPerSeg < 1)
				return 0.f;
			const int32 total = sc * samplesPerSeg;
			const float32 full = Length(samplesPerSeg);
			if (full < 1e-8f)
				return 0.f;
			// Replie/clampe la distance.
			float32 d = dist;
			if (closed) {
				d = d - full * math::NkFloor(d / full);
				if (d < 0.f)
					d += full;
			} else {
				d = d < 0.f ? 0.f : (d > full ? full : d);
			}
			// Marche le long de l'arc jusqu'à atteindre d.
			float32 acc = 0.f;
			NkVec3f prev = SamplePosition(0.f);
			for (int32 i = 1; i <= total; ++i) {
				const float32 t = static_cast<float32>(i) / static_cast<float32>(total);
				const NkVec3f cur = SamplePosition(t);
				const float32 seg = Dist(prev, cur);
				if (acc + seg >= d) {
					const float32 tPrev = static_cast<float32>(i - 1) / static_cast<float32>(total);
					const float32 frac = seg > 1e-8f ? (d - acc) / seg : 0.f;
					return tPrev + (t - tPrev) * frac;
				}
				acc += seg;
				prev = cur;
			}
			return 1.f;
		}

		NkVec3f NkMotionCurve::SampleByDistance(float32 dist, int32 samplesPerSeg) const {
			return SamplePosition(DistanceToT(dist, samplesPerSeg));
		}

		void NkPathFollow::Reset() {
			mDistance = 0.f;
			mDir = 1;
		}

		float32 NkPathFollow::SampleScale(float32 t) const {
			const int32 n = static_cast<int32>(scaleProfile.Size());
			if (n == 0)
				return 1.0f;
			const float32 tt = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
			if (n == 1)
				return scaleProfile[0].y;
			// Recherche linéaire du segment (peu de clés en pratique).
			for (int32 i = 1; i < n; ++i) {
				if (tt <= scaleProfile[static_cast<uint64>(i)].x) {
					const math::NkVec2f a = scaleProfile[static_cast<uint64>(i - 1)];
					const math::NkVec2f b = scaleProfile[static_cast<uint64>(i)];
					const float32 span = b.x - a.x;
					const float32 u = span > 1e-6f ? (tt - a.x) / span : 0.f;
					return a.y + (b.y - a.y) * u;
				}
			}
			return scaleProfile[static_cast<uint64>(n - 1)].y;
		}

		// Remplit la cible (T+R+S) au paramètre t.
		NkPathFollow::NkPathTarget NkPathFollow::SampleAtDistance(float32 dist) const {
			NkPathTarget out;
			const float32 t = curve.DistanceToT(dist);
			out.position = curve.SamplePosition(t);
			out.forward = curve.SampleTangent(t);
			if (orientToPath)
				out.rotation = math::NkQuatf::LookAt(out.forward, up);
			const float32 s = SampleScale(t);
			out.scale = NkVec3f{baseScale.x * s, baseScale.y * s, baseScale.z * s};
			return out;
		}

		NkPathFollow::NkPathTarget NkPathFollow::Advance(float32 dt) {
			NkPathTarget out;
			const float32 full = curve.Length();
			if (full < 1e-8f) {
				out.position = curve.SamplePosition(0.f);
				out.forward = curve.SampleTangent(0.f);
				return out;
			}
			mDistance += speed * dt * static_cast<float32>(mDir);

			switch (loop) {
				case NkPathLoopMode::NK_ONCE:
					if (mDistance >= full) {
						mDistance = full;
						out.finished = true;
					} else if (mDistance < 0.f) {
						mDistance = 0.f;
					}
					break;
				case NkPathLoopMode::NK_LOOP:
					mDistance = mDistance - full * math::NkFloor(mDistance / full);
					if (mDistance < 0.f)
						mDistance += full;
					break;
				case NkPathLoopMode::NK_PINGPONG:
					if (mDistance > full) {
						mDistance = full - (mDistance - full);
						mDir = -1;
					} else if (mDistance < 0.f) {
						mDistance = -mDistance;
						mDir = 1;
					}
					break;
			}

			const float32 t = curve.DistanceToT(mDistance);
			out.position = curve.SamplePosition(t);
			out.forward = curve.SampleTangent(t);
			if (orientToPath)
				out.rotation = math::NkQuatf::LookAt(out.forward, up);
			const float32 s = SampleScale(t);
			out.scale = NkVec3f{baseScale.x * s, baseScale.y * s, baseScale.z * s};
			return out;
		}

		bool NkMotionCurve::SelfTest() {
			bool ok = true;

			// --- Test 1 : la courbe PASSE par ses points de contrôle (ouverte). ---
			{
				NkMotionCurve c;
				c.points.PushBack(NkVec3f{0.f, 0.f, 0.f});
				c.points.PushBack(NkVec3f{1.f, 1.f, 0.f});
				c.points.PushBack(NkVec3f{2.f, 0.f, 0.f});
				c.points.PushBack(NkVec3f{3.f, 1.f, 0.f});
				const NkVec3f p0 = c.SamplePosition(0.f);
				const NkVec3f p1 = c.SamplePosition(1.f);
				if (Dist(p0, c.points[0]) > 1e-4f)
					ok = false; // début = 1er point
				if (Dist(p1, c.points[3]) > 1e-4f)
					ok = false; // fin = dernier point
			}

			// --- Test 2 : ligne droite → longueur = distance des extrémités, tangente = +X. ---
			{
				NkMotionCurve c;
				c.points.PushBack(NkVec3f{0.f, 0.f, 0.f});
				c.points.PushBack(NkVec3f{1.f, 0.f, 0.f});
				c.points.PushBack(NkVec3f{2.f, 0.f, 0.f});
				c.points.PushBack(NkVec3f{3.f, 0.f, 0.f});
				const float32 len = c.Length();
				if (math::NkAbs(len - 3.0f) > 1e-2f)
					ok = false;
				const NkVec3f tg = c.SampleTangent(0.5f);
				if (math::NkAbs(tg.x - 1.0f) > 1e-3f || math::NkAbs(tg.y) > 1e-3f)
					ok = false;
				// À mi-distance (1.5), position ≈ x=1.5.
				const NkVec3f mid = c.SampleByDistance(1.5f);
				if (math::NkAbs(mid.x - 1.5f) > 2e-2f)
					ok = false;
			}

			// --- Test 3 : path-follow — avance à vitesse constante, loop revient au début. ---
			{
				NkPathFollow pf;
				pf.curve.points.PushBack(NkVec3f{0.f, 0.f, 0.f});
				pf.curve.points.PushBack(NkVec3f{1.f, 0.f, 0.f});
				pf.curve.points.PushBack(NkVec3f{2.f, 0.f, 0.f});
				pf.curve.points.PushBack(NkVec3f{3.f, 0.f, 0.f});
				pf.speed = 1.0f;
				pf.loop = NkPathLoopMode::NK_LOOP;
				// après 1.5 s à vitesse 1 → distance 1.5 → x≈1.5.
				NkPathFollow::NkPathTarget tg{};
				for (int i = 0; i < 15; ++i)
					tg = pf.Advance(0.1f);
				if (math::NkAbs(tg.position.x - 1.5f) > 5e-2f)
					ok = false;
				if (math::NkAbs(tg.forward.x - 1.0f) > 1e-2f)
					ok = false;
				// longueur totale 3 → après encore 2 s (total 3.5) → loop → distance 0.5 → x≈0.5.
				for (int i = 0; i < 20; ++i)
					tg = pf.Advance(0.1f);
				if (math::NkAbs(tg.position.x - 0.5f) > 8e-2f)
					ok = false;
			}

			// --- Test 4 : NK_ONCE s'arrête à la fin (finished). ---
			{
				NkPathFollow pf;
				pf.curve.points.PushBack(NkVec3f{0.f, 0.f, 0.f});
				pf.curve.points.PushBack(NkVec3f{1.f, 0.f, 0.f});
				pf.curve.points.PushBack(NkVec3f{2.f, 0.f, 0.f});
				pf.speed = 2.0f;
				pf.loop = NkPathLoopMode::NK_ONCE;
				NkPathFollow::NkPathTarget tg{};
				for (int i = 0; i < 20; ++i)
					tg = pf.Advance(0.1f); // 20*0.1*2 = 4 unités > longueur 2
				if (!tg.finished)
					ok = false;
				if (math::NkAbs(tg.position.x - 2.0f) > 5e-2f)
					ok = false; // clampé au dernier point
			}

			// --- Test 5 : TRANSFORM COMPLET — ÉCHELLE (profil) + ROTATION (valide, constante sur droite). ---
			{
				NkPathFollow pf;
				pf.curve.points.PushBack(NkVec3f{0.f, 0.f, 0.f});
				pf.curve.points.PushBack(NkVec3f{1.f, 0.f, 0.f});
				pf.curve.points.PushBack(NkVec3f{2.f, 0.f, 0.f});
				pf.curve.points.PushBack(NkVec3f{3.f, 0.f, 0.f});
				// échelle 1 au début → 3 à la fin.
				pf.scaleProfile.PushBack(math::NkVec2f{0.f, 1.f});
				pf.scaleProfile.PushBack(math::NkVec2f{1.f, 3.f});

				const NkPathFollow::NkPathTarget a = pf.SampleAtDistance(0.f);   // t=0 → scale 1
				const NkPathFollow::NkPathTarget m = pf.SampleAtDistance(1.5f);  // t≈0.5 → scale ≈2
				const NkPathFollow::NkPathTarget b = pf.SampleAtDistance(3.f);   // t=1 → scale 3
				if (math::NkAbs(a.scale.x - 1.0f) > 5e-2f)
					ok = false;
				if (math::NkAbs(m.scale.x - 2.0f) > 1.5e-1f)
					ok = false;
				if (math::NkAbs(b.scale.x - 3.0f) > 5e-2f)
					ok = false;

				// rotation valide (normalisée) et constante sur une droite.
				const float32 qn = math::NkSqrt(m.rotation.x * m.rotation.x + m.rotation.y * m.rotation.y +
												m.rotation.z * m.rotation.z + m.rotation.w * m.rotation.w);
				if (math::NkAbs(qn - 1.0f) > 1e-2f)
					ok = false;
				const float32 dq = math::NkAbs(a.rotation.x - m.rotation.x) + math::NkAbs(a.rotation.y - m.rotation.y) +
								   math::NkAbs(a.rotation.z - m.rotation.z) + math::NkAbs(a.rotation.w - m.rotation.w);
				if (dq > 1e-2f)
					ok = false; // même direction (droite) → même rotation
			}

			return ok;
		}

	} // namespace anim
} // namespace nkentseu
