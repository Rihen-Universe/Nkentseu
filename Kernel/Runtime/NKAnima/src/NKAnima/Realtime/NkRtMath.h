#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRtMath.h — LES OUTILS DE CALCUL de la physique d'animation EN TEMPS
// REEL (ressorts, IK des pieds, equilibre, ragdoll). Des fonctions en ligne, sans
// etat, sans allocation.
//
// ⚠️ Pourquoi pas NKMath::NkQuatf : les solveurs n'en ont besoin que de six
// operations, et ce module doit rester lisible a la lecture du code -- une
// convention de quaternion devinee est exactement le genre de defaut qui ne se
// voit qu'a l'ecran. Ici : (x, y, z, w), w reel, produit de Hamilton, rotation
// active v' = q v q*. Et pas Rig/NkRigMath.h : il appartient a l'atelier, qui
// quittera ce module (la scission) ; le temps reel, lui, reste.
//
// Matrices : celles de NKMath (colonnes : m00..m02 = axe X, m30..m32 =
// translation). Une rotation appliquee a une matrice MONDE tourne ses trois
// colonnes d'axes et son origine autour d'un pivot : l'echelle eventuelle des
// colonnes est gardee.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"

#include <cmath>

namespace nkentseu {
	namespace anim {
		namespace rt {

			using math::NkMat4f;
			using math::NkVec3f;

			// ── Vecteurs ───────────────────────────────────────────────────────
			inline NkVec3f V3(float32 x, float32 y, float32 z) {
				NkVec3f v;
				v.x = x;
				v.y = y;
				v.z = z;
				return v;
			}
			inline NkVec3f Add(const NkVec3f &a, const NkVec3f &b) {
				return V3(a.x + b.x, a.y + b.y, a.z + b.z);
			}
			inline NkVec3f Sub(const NkVec3f &a, const NkVec3f &b) {
				return V3(a.x - b.x, a.y - b.y, a.z - b.z);
			}
			inline NkVec3f Scale(const NkVec3f &a, float32 s) {
				return V3(a.x * s, a.y * s, a.z * s);
			}
			inline NkVec3f Madd(const NkVec3f &a, const NkVec3f &b, float32 s) {
				return V3(a.x + b.x * s, a.y + b.y * s, a.z + b.z * s);
			}
			inline float32 Dot(const NkVec3f &a, const NkVec3f &b) {
				return a.x * b.x + a.y * b.y + a.z * b.z;
			}
			inline NkVec3f Cross(const NkVec3f &a, const NkVec3f &b) {
				return V3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
			}
			inline float32 Length(const NkVec3f &a) {
				return std::sqrt(Dot(a, a));
			}
			inline float32 Distance(const NkVec3f &a, const NkVec3f &b) {
				return Length(Sub(a, b));
			}
			inline NkVec3f Normalize(const NkVec3f &a, const NkVec3f &repli) {
				const float32 l = Length(a);
				return l > 1e-9f ? Scale(a, 1.f / l) : repli;
			}
			inline NkVec3f Lerp(const NkVec3f &a, const NkVec3f &b, float32 t) {
				return V3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
			}
			/// Une direction quelconque perpendiculaire a `d` (unitaire).
			inline NkVec3f AnyPerpendicular(const NkVec3f &d) {
				const NkVec3f a = std::fabs(d.y) < 0.9f ? V3(0.f, 1.f, 0.f) : V3(1.f, 0.f, 0.f);
				return Normalize(Cross(d, a), V3(1.f, 0.f, 0.f));
			}
			inline bool IsFinite(const NkVec3f &a) {
				return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
			}
			inline float32 Clamp(float32 v, float32 lo, float32 hi) {
				return v < lo ? lo : (v > hi ? hi : v);
			}
			inline float32 SmoothStep(float32 e0, float32 e1, float32 x) {
				if (e1 <= e0)
					return x < e0 ? 0.f : 1.f;
				const float32 t = Clamp((x - e0) / (e1 - e0), 0.f, 1.f);
				return t * t * (3.f - 2.f * t);
			}

			// ── Quaternions (x, y, z, w) ───────────────────────────────────────
			struct NkRtQuat {
					float32 x = 0.f, y = 0.f, z = 0.f, w = 1.f;
			};
			inline NkRtQuat Q(float32 x, float32 y, float32 z, float32 w) {
				NkRtQuat q;
				q.x = x;
				q.y = y;
				q.z = z;
				q.w = w;
				return q;
			}
			inline NkRtQuat QIdentity() {
				return Q(0.f, 0.f, 0.f, 1.f);
			}
			inline NkRtQuat QMul(const NkRtQuat &a, const NkRtQuat &b) {
				return Q(a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
						 a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z);
			}
			inline NkRtQuat QConj(const NkRtQuat &q) {
				return Q(-q.x, -q.y, -q.z, q.w);
			}
			inline NkRtQuat QNormalize(const NkRtQuat &q) {
				const float32 l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
				return l > 1e-12f ? Q(q.x / l, q.y / l, q.z / l, q.w / l) : QIdentity();
			}
			inline NkVec3f QRotate(const NkRtQuat &q, const NkVec3f &v) {
				// v' = v + 2 u x (u x v + w v), u = (x, y, z)
				const NkVec3f u = V3(q.x, q.y, q.z);
				const NkVec3f t = Add(Cross(u, v), Scale(v, q.w));
				return Add(v, Scale(Cross(u, t), 2.f));
			}
			inline NkRtQuat QFromAxisAngle(const NkVec3f &axisUnit, float32 angle) {
				const float32 s = std::sin(angle * 0.5f);
				return Q(axisUnit.x * s, axisUnit.y * s, axisUnit.z * s, std::cos(angle * 0.5f));
			}
			/// La rotation MINIMALE qui porte la direction `u` sur la direction `v`.
			inline NkRtQuat QFromTo(const NkVec3f &u0, const NkVec3f &v0) {
				const NkVec3f u = Normalize(u0, V3(0.f, 1.f, 0.f));
				const NkVec3f v = Normalize(v0, u);
				const float32 d = Dot(u, v);
				if (d < -0.999999f) {
					return QFromAxisAngle(AnyPerpendicular(u), 3.14159265358979f);
				}
				const NkVec3f c = Cross(u, v);
				return QNormalize(Q(c.x, c.y, c.z, 1.f + d));
			}
			/// Exponentielle : un vecteur rotation (axe * angle) -> quaternion.
			inline NkRtQuat QFromRotVec(const NkVec3f &r) {
				const float32 a = Length(r);
				if (a < 1e-8f) {
					return QNormalize(Q(r.x * 0.5f, r.y * 0.5f, r.z * 0.5f, 1.f));
				}
				return QFromAxisAngle(Scale(r, 1.f / a), a);
			}
			/// Logarithme : quaternion -> vecteur rotation (chemin le plus court).
			inline NkVec3f QToRotVec(const NkRtQuat &q0) {
				NkRtQuat q = QNormalize(q0);
				if (q.w < 0.f) {
					q = Q(-q.x, -q.y, -q.z, -q.w);
				}
				const float32 s = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z);
				if (s < 1e-8f) {
					return V3(q.x * 2.f, q.y * 2.f, q.z * 2.f);
				}
				const float32 a = 2.f * std::atan2(s, q.w);
				return V3(q.x / s * a, q.y / s * a, q.z / s * a);
			}
			inline float32 QAngle(const NkRtQuat &q) {
				return Length(QToRotVec(q));
			}
			inline NkRtQuat QSlerp(const NkRtQuat &a, const NkRtQuat &b0, float32 t) {
				NkRtQuat b = b0;
				float32 d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
				if (d < 0.f) {
					d = -d;
					b = Q(-b.x, -b.y, -b.z, -b.w);
				}
				if (d > 0.9995f) {
					return QNormalize(Q(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
										a.w + (b.w - a.w) * t));
				}
				const float32 th = std::acos(Clamp(d, -1.f, 1.f));
				const float32 s = std::sin(th);
				const float32 wa = std::sin((1.f - t) * th) / s, wb = std::sin(t * th) / s;
				return Q(a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb, a.w * wa + b.w * wb);
			}
			/// Une fraction `t` de la rotation `q` (depuis l'identite).
			inline NkRtQuat QScale(const NkRtQuat &q, float32 t) {
				return QFromRotVec(Scale(QToRotVec(q), t));
			}

			// ── Matrices (colonnes NKMath) ─────────────────────────────────────
			inline NkVec3f Origin(const NkMat4f &m) {
				return V3(m.m30, m.m31, m.m32);
			}
			inline void SetOrigin(NkMat4f &m, const NkVec3f &p) {
				m.m30 = p.x;
				m.m31 = p.y;
				m.m32 = p.z;
			}
			inline NkVec3f AxisX(const NkMat4f &m) {
				return V3(m.m00, m.m01, m.m02);
			}
			inline NkVec3f AxisY(const NkMat4f &m) {
				return V3(m.m10, m.m11, m.m12);
			}
			inline NkVec3f AxisZ(const NkMat4f &m) {
				return V3(m.m20, m.m21, m.m22);
			}
			inline void SetAxes(NkMat4f &m, const NkVec3f &x, const NkVec3f &y, const NkVec3f &z) {
				m.m00 = x.x;
				m.m01 = x.y;
				m.m02 = x.z;
				m.m10 = y.x;
				m.m11 = y.y;
				m.m12 = y.z;
				m.m20 = z.x;
				m.m21 = z.y;
				m.m22 = z.z;
			}
			/// Un point du repere local de `m`, en monde.
			inline NkVec3f TransformPoint(const NkMat4f &m, const NkVec3f &p) {
				return V3(m.m00 * p.x + m.m10 * p.y + m.m20 * p.z + m.m30, m.m01 * p.x + m.m11 * p.y + m.m21 * p.z + m.m31,
						  m.m02 * p.x + m.m12 * p.y + m.m22 * p.z + m.m32);
			}
			/// Une direction du repere local de `m`, en monde.
			inline NkVec3f TransformDir(const NkMat4f &m, const NkVec3f &d) {
				return V3(m.m00 * d.x + m.m10 * d.y + m.m20 * d.z, m.m01 * d.x + m.m11 * d.y + m.m21 * d.z,
						  m.m02 * d.x + m.m12 * d.y + m.m22 * d.z);
			}
			/// Tourne la matrice MONDE `m` (axes ET origine) de `q` autour du point `pivot`.
			inline void RotateAbout(NkMat4f &m, const NkRtQuat &q, const NkVec3f &pivot) {
				SetAxes(m, QRotate(q, AxisX(m)), QRotate(q, AxisY(m)), QRotate(q, AxisZ(m)));
				SetOrigin(m, Add(pivot, QRotate(q, Sub(Origin(m), pivot))));
			}
			/// La rotation des axes de `m` (colonnes normalisees : l'echelle est ignoree).
			inline NkRtQuat QFromMat(const NkMat4f &m) {
				const NkVec3f x = Normalize(AxisX(m), V3(1.f, 0.f, 0.f));
				NkVec3f y = AxisY(m);
				y = Normalize(Sub(y, Scale(x, Dot(x, y))), AnyPerpendicular(x));
				const NkVec3f z = Cross(x, y);
				const float32 r00 = x.x, r11 = y.y, r22 = z.z;
				const float32 tr = r00 + r11 + r22;
				NkRtQuat q;
				if (tr > 0.f) {
					const float32 s = std::sqrt(tr + 1.f) * 2.f;
					q = Q((y.z - z.y) / s, (z.x - x.z) / s, (x.y - y.x) / s, 0.25f * s);
				} else if (r00 > r11 && r00 > r22) {
					const float32 s = std::sqrt(1.f + r00 - r11 - r22) * 2.f;
					q = Q(0.25f * s, (y.x + x.y) / s, (z.x + x.z) / s, (y.z - z.y) / s);
				} else if (r11 > r22) {
					const float32 s = std::sqrt(1.f + r11 - r00 - r22) * 2.f;
					q = Q((y.x + x.y) / s, 0.25f * s, (z.y + y.z) / s, (z.x - x.z) / s);
				} else {
					const float32 s = std::sqrt(1.f + r22 - r00 - r11) * 2.f;
					q = Q((z.x + x.z) / s, (z.y + y.z) / s, 0.25f * s, (x.y - y.x) / s);
				}
				return QNormalize(q);
			}
			/// Les longueurs des trois colonnes d'axes (l'echelle par axe).
			inline NkVec3f AxisScales(const NkMat4f &m) {
				return V3(Length(AxisX(m)), Length(AxisY(m)), Length(AxisZ(m)));
			}
			/// Une matrice depuis une rotation, une echelle par axe et une origine.
			inline NkMat4f MatFrom(const NkRtQuat &q, const NkVec3f &scale, const NkVec3f &origin) {
				NkMat4f m = NkMat4f::Identity();
				SetAxes(m, Scale(QRotate(q, V3(1.f, 0.f, 0.f)), scale.x), Scale(QRotate(q, V3(0.f, 1.f, 0.f)), scale.y),
						Scale(QRotate(q, V3(0.f, 0.f, 1.f)), scale.z));
				SetOrigin(m, origin);
				return m;
			}
			/// Le melange de deux transformations (rotation : slerp, origine et echelle :
			/// lineaire). `t` = 0 : a ; 1 : b.
			inline NkMat4f BlendMat(const NkMat4f &a, const NkMat4f &b, float32 t) {
				if (t <= 0.f)
					return a;
				if (t >= 1.f)
					return b;
				return MatFrom(QSlerp(QFromMat(a), QFromMat(b), t), Lerp(AxisScales(a), AxisScales(b), t),
							   Lerp(Origin(a), Origin(b), t));
			}

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
