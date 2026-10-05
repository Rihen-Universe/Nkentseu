#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Rig/ — le rig 3D : armature editable (tete/queue/roulis), rig automatique, document du rig.
// -----------------------------------------------------------------------------
// FICHIER: NKAnima/Rig/NkRigMath.h
// DESCRIPTION: les QUELQUES calculs de vecteurs dont le rig, la peau et les
//   formes ont besoin, ecrits composante par composante.
//
// ⚠️ POURQUOI PAS LES OPERATEURS DE NKMath DIRECTEMENT. Ils existent (et sont
//    employes quand ils suffisent), mais deux pieges ont deja coute ici :
//    `NkMat4f m;` est une matrice NULLE (pas l'identite), et les angles passent
//    par `NkAngle` dont l'unite se lit mal a l'appel. Les fonctions ci-dessous
//    disent leur unite (radians) et ne construisent jamais de matrice implicite.
// -----------------------------------------------------------------------------

#include "NKMath/NKMath.h"

#include <cmath>

namespace nkentseu {
	namespace anim {
		namespace rigm {

			using math::NkMat4f;
			using math::NkVec3f;

			inline NkVec3f V(float32 x, float32 y, float32 z) {
				NkVec3f v;
				v.x = x;
				v.y = y;
				v.z = z;
				return v;
			}
			inline NkVec3f Add(const NkVec3f &a, const NkVec3f &b) {
				return V(a.x + b.x, a.y + b.y, a.z + b.z);
			}
			inline NkVec3f Sub(const NkVec3f &a, const NkVec3f &b) {
				return V(a.x - b.x, a.y - b.y, a.z - b.z);
			}
			inline NkVec3f Mul(const NkVec3f &a, float32 s) {
				return V(a.x * s, a.y * s, a.z * s);
			}
			inline NkVec3f Lerp(const NkVec3f &a, const NkVec3f &b, float32 t) {
				return V(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
			}
			inline float32 Dot(const NkVec3f &a, const NkVec3f &b) {
				return a.x * b.x + a.y * b.y + a.z * b.z;
			}
			inline NkVec3f Cross(const NkVec3f &a, const NkVec3f &b) {
				return V(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
			}
			inline float32 Len(const NkVec3f &a) {
				return std::sqrt(Dot(a, a));
			}
			inline float32 Dist(const NkVec3f &a, const NkVec3f &b) {
				return Len(Sub(a, b));
			}
			/// Normalise ; un vecteur nul rend `repli`.
			inline NkVec3f Norm(const NkVec3f &a, const NkVec3f &repli = V(0.f, 1.f, 0.f)) {
				const float32 l = Len(a);
				return l > 1e-12f ? Mul(a, 1.f / l) : repli;
			}
			/// Le miroir par le plan x = 0 (la symetrie X du rig).
			inline NkVec3f MiroirX(const NkVec3f &a) {
				return V(-a.x, a.y, a.z);
			}

			/// Rotation de `v` autour de l'axe UNITAIRE `k`, angle en RADIANS (Rodrigues).
			inline NkVec3f Tourner(const NkVec3f &v, const NkVec3f &k, float32 angle) {
				const float32 c = std::cos(angle), s = std::sin(angle);
				const NkVec3f kxv = Cross(k, v);
				const float32 kv = Dot(k, v);
				return V(v.x * c + kxv.x * s + k.x * kv * (1.f - c), v.y * c + kxv.y * s + k.y * kv * (1.f - c),
						 v.z * c + kxv.z * s + k.z * kv * (1.f - c));
			}

			/// Une matrice depuis ses trois axes et son origine (colonnes, comme NkMat4f).
			inline NkMat4f Repere(const NkVec3f &x, const NkVec3f &y, const NkVec3f &z, const NkVec3f &o) {
				NkMat4f m = NkMat4f::Identity();
				m.m00 = x.x;
				m.m01 = x.y;
				m.m02 = x.z;
				m.m10 = y.x;
				m.m11 = y.y;
				m.m12 = y.z;
				m.m20 = z.x;
				m.m21 = z.y;
				m.m22 = z.z;
				m.m30 = o.x;
				m.m31 = o.y;
				m.m32 = o.z;
				return m;
			}
			inline NkVec3f AxeX(const NkMat4f &m) {
				return V(m.m00, m.m01, m.m02);
			}
			inline NkVec3f AxeY(const NkMat4f &m) {
				return V(m.m10, m.m11, m.m12);
			}
			inline NkVec3f AxeZ(const NkMat4f &m) {
				return V(m.m20, m.m21, m.m22);
			}
			inline NkVec3f Origine(const NkMat4f &m) {
				return V(m.m30, m.m31, m.m32);
			}
			/// Un point par une matrice affine (w = 1).
			inline NkVec3f Point(const NkMat4f &m, const NkVec3f &p) {
				return V(m.m00 * p.x + m.m10 * p.y + m.m20 * p.z + m.m30, m.m01 * p.x + m.m11 * p.y + m.m21 * p.z + m.m31,
						 m.m02 * p.x + m.m12 * p.y + m.m22 * p.z + m.m32);
			}
			/// Une direction par une matrice (w = 0).
			inline NkVec3f Direction(const NkMat4f &m, const NkVec3f &d) {
				return V(m.m00 * d.x + m.m10 * d.y + m.m20 * d.z, m.m01 * d.x + m.m11 * d.y + m.m21 * d.z,
						 m.m02 * d.x + m.m12 * d.y + m.m22 * d.z);
			}
			/// La symetrie X d'une matrice : S * M * S, S = diag(-1, 1, 1). Une
			/// ROTATION reste une rotation (det inchange) : c'est ce qui rend le repere
			/// d'un os miroir coherent avec l'os de depart.
			inline NkMat4f MiroirMatriceX(const NkMat4f &m) {
				NkMat4f r = m;
				// S*M*S : la ligne 0 et la colonne 0 changent de signe (l'element 00 deux fois).
				r.m01 = -m.m01;
				r.m02 = -m.m02;
				r.m03 = -m.m03;
				r.m10 = -m.m10;
				r.m20 = -m.m20;
				r.m30 = -m.m30;
				return r;
			}

			/// Le quaternion (x, y, z, w) de la partie ROTATION d'une matrice (axes
			/// renormalises : une echelle ne fausse pas l'angle). Shepperd.
			inline void Quaternion(const NkMat4f &m, float32 &qx, float32 &qy, float32 &qz, float32 &qw) {
				const NkVec3f ax = Norm(AxeX(m), V(1.f, 0.f, 0.f)), ay = Norm(AxeY(m)), az = Norm(AxeZ(m), V(0.f, 0.f, 1.f));
				// r_ij : ligne i, colonne j (les colonnes sont les axes).
				const float32 r00 = ax.x, r10 = ax.y, r20 = ax.z;
				const float32 r01 = ay.x, r11 = ay.y, r21 = ay.z;
				const float32 r02 = az.x, r12 = az.y, r22 = az.z;
				const float32 tr = r00 + r11 + r22;
				if (tr > 0.f) {
					const float32 s = std::sqrt(tr + 1.f) * 2.f;
					qw = 0.25f * s;
					qx = (r21 - r12) / s;
					qy = (r02 - r20) / s;
					qz = (r10 - r01) / s;
				} else if (r00 > r11 && r00 > r22) {
					const float32 s = std::sqrt(1.f + r00 - r11 - r22) * 2.f;
					qw = (r21 - r12) / s;
					qx = 0.25f * s;
					qy = (r01 + r10) / s;
					qz = (r02 + r20) / s;
				} else if (r11 > r22) {
					const float32 s = std::sqrt(1.f + r11 - r00 - r22) * 2.f;
					qw = (r02 - r20) / s;
					qx = (r01 + r10) / s;
					qy = 0.25f * s;
					qz = (r12 + r21) / s;
				} else {
					const float32 s = std::sqrt(1.f + r22 - r00 - r11) * 2.f;
					qw = (r10 - r01) / s;
					qx = (r02 + r20) / s;
					qy = (r12 + r21) / s;
					qz = 0.25f * s;
				}
			}

			/// Distance d'un point a un segment [a, b], et le parametre du point le plus proche.
			inline float32 DistanceSegment(const NkVec3f &p, const NkVec3f &a, const NkVec3f &b, float32 *tOut = nullptr) {
				const NkVec3f ab = Sub(b, a);
				const float32 l2 = Dot(ab, ab);
				float32 t = l2 > 1e-12f ? Dot(Sub(p, a), ab) / l2 : 0.f;
				t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
				if (tOut != nullptr) {
					*tOut = t;
				}
				return Dist(p, Add(a, Mul(ab, t)));
			}

		} // namespace rigm
	} // namespace anim
} // namespace nkentseu
