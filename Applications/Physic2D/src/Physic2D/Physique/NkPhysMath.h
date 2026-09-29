// =============================================================================
// NkPhysMath.h — le vecteur 2D et le hasard du moteur Physic2D
//
// POURQUOI UN VECTEUR A NOUS, et pas math::NkVec2f
//   Le moteur fait des millions d'operations par seconde sur des paires de
//   particules. Il lui faut un type TRIVIAL, sans surprise d'alias ni de
//   conversion implicite, dont chaque operateur est visible d'un coup d'oeil.
//   Le passage vers NKGui (NkVec2) se fait en UN seul endroit : le rendu.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include <cmath>

namespace nkentseu {
	namespace physic2d {

		struct NkV2 {
				float32 x = 0.f;
				float32 y = 0.f;

				constexpr NkV2() noexcept = default;
				constexpr NkV2(float32 ax, float32 ay) noexcept : x(ax), y(ay) {
				}

				constexpr NkV2 operator+(const NkV2 &o) const noexcept {
					return NkV2(x + o.x, y + o.y);
				}
				constexpr NkV2 operator-(const NkV2 &o) const noexcept {
					return NkV2(x - o.x, y - o.y);
				}
				constexpr NkV2 operator-() const noexcept {
					return NkV2(-x, -y);
				}
				constexpr NkV2 operator*(float32 s) const noexcept {
					return NkV2(x * s, y * s);
				}
				constexpr NkV2 operator/(float32 s) const noexcept {
					return NkV2(x / s, y / s);
				}
				NkV2 &operator+=(const NkV2 &o) noexcept {
					x += o.x;
					y += o.y;
					return *this;
				}
				NkV2 &operator-=(const NkV2 &o) noexcept {
					x -= o.x;
					y -= o.y;
					return *this;
				}
				NkV2 &operator*=(float32 s) noexcept {
					x *= s;
					y *= s;
					return *this;
				}
		};

		constexpr NkV2 operator*(float32 s, const NkV2 &v) noexcept {
			return NkV2(v.x * s, v.y * s);
		}
		constexpr float32 NkDot(const NkV2 &a, const NkV2 &b) noexcept {
			return a.x * b.x + a.y * b.y;
		}
		/// Produit vectoriel 2D (composante z). > 0 : b est a GAUCHE de a.
		constexpr float32 NkCross(const NkV2 &a, const NkV2 &b) noexcept {
			return a.x * b.y - a.y * b.x;
		}
		constexpr float32 NkLen2(const NkV2 &v) noexcept {
			return v.x * v.x + v.y * v.y;
		}
		inline float32 NkLen(const NkV2 &v) noexcept {
			return std::sqrt(v.x * v.x + v.y * v.y);
		}
		/// Rotation de +90 degres (sens trigonometrique).
		constexpr NkV2 NkPerp(const NkV2 &v) noexcept {
			return NkV2(-v.y, v.x);
		}
		inline NkV2 NkRotation(const NkV2 &v, float32 c, float32 s) noexcept {
			return NkV2(v.x * c - v.y * s, v.x * s + v.y * c);
		}
		constexpr float32 NkClampf(float32 v, float32 a, float32 b) noexcept {
			return v < a ? a : (v > b ? b : v);
		}
		constexpr float32 NkMinf(float32 a, float32 b) noexcept {
			return a < b ? a : b;
		}
		constexpr float32 NkMaxf(float32 a, float32 b) noexcept {
			return a > b ? a : b;
		}
		constexpr float32 NkLerpf(float32 a, float32 b, float32 t) noexcept {
			return a + (b - a) * t;
		}
		/// true si le nombre est fini. Un NaN ne se compare egal a rien, pas
		/// meme a lui-meme : c'est le test le moins cher qui ne depende pas
		/// d'une option de compilation (-ffast-math casse std::isnan).
		inline bool NkFini(float32 v) noexcept {
			return v == v && v < 1.0e30f && v > -1.0e30f;
		}

		/// Hasard DETERMINISTE (xorshift32). Deux executions avec la meme graine
		/// donnent la meme simulation : c'est ce qui rend le banc reproductible.
		struct NkAlea {
				uint32 etat = 0x9E3779B9u;

				uint32 Suivant() noexcept {
					uint32 x = etat;
					x ^= x << 13;
					x ^= x >> 17;
					x ^= x << 5;
					etat = x;
					return x;
				}
				/// [0, 1)
				float32 Uniforme() noexcept {
					return static_cast<float32>(Suivant() >> 8) * (1.f / 16777216.f);
				}
				/// [-1, 1)
				float32 Signe() noexcept {
					return Uniforme() * 2.f - 1.f;
				}
		};

		constexpr float32 NK_PI = 3.14159265358979f;

	} // namespace physic2d
} // namespace nkentseu
