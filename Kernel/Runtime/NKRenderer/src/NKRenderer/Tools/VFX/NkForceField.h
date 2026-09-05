#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkForceField.h — le VENT : une source de force externe commune (2026-09-05).
// ROADMAP_PRODUITS §6.2/6.3 : un champ de forces que TOUTES les familles lisent —
// particules ordinaires (CPU et noyau GPU), fluide SPH (CPU et GPU), et demain tissu,
// cheveux, herbe. Il vit sur l'émetteur (NkEmitterDesc::field) et s'ajoute à la gravité
// comme une ACCÉLÉRATION (force par unité de masse), là où la gravité est ajoutée.
//
// CONTRAT (05/09, tranché par délégation) : le champ rend des NEWTONS sur une particule ponctuelle ;
// CHAQUE CONSOMMATEUR DIVISE PAR LA MASSE DE SA PARTICULE (NkEmitterDesc::particleMass, explicite ;
// le SPH : sa masse calibrée Mass()). `strength` est donc une force (N), pas une accélération.
// Quatre formes : UNIFORM (F = dir · strength), VORTEX (tangentiel autour d'un axe,
// plein jusqu'au rayon puis décroissant en 1/r), TURBULENCE (bruit de valeur 3D, trois
// canaux), CURL (rotationnel d'un potentiel de bruit — à divergence nulle par
// construction : Bridson, Hourihan, Nordenstam, « Curl-Noise for Procedural Fluid Flow »,
// SIGGRAPH 2007 ; le rotationnel est pris par différences centrées, la divergence
// discrète est mesurée dans la sonde). Le bruit est un bruit de valeur à hachage par
// sinus (fract(sin(dot(p, k)) · 43758,5453)), le même en C++ et en NkSL : deux cibles,
// une formule ; ils diffèrent à epsilon (sin des deux côtés), dit.
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKMath/NkIForceField.h" // le CONTRAT partage avec le tissu (NKPhysics) : Force(position, temps) en newtons
#include <cmath>

namespace nkentseu {
	namespace renderer {

		enum class NkForceFieldType : uint8 { NONE, UNIFORM, VORTEX, TURBULENCE, CURL };

		struct NkForceField;
		inline NkVec3f NkEvalForceField(const NkForceField &f, NkVec3f pos, float32 t);

		// Le champ EST un math::NkIForceField (2026-09-05) : tissu, cheveux, herbe, particules et SPH lisent la
		// MÊME chose -- une force en newtons -- et chacun divise par la masse de sa particule.
		struct NkForceField : public math::NkIForceField {
				NkForceFieldType type = NkForceFieldType::NONE;
				float32 strength = 0.f;			   // force (N) pour UNIFORM/VORTEX ; amplitude (N) pour les bruits
				NkVec3f direction = {1.f, 0.f, 0.f}; // UNIFORM : direction ; VORTEX : centre
				NkVec3f axis = {0.f, 1.f, 0.f};		   // VORTEX : axe
				float32 radius = 1.f;				   // VORTEX : rayon du noyau plein
				float32 frequency = 1.f;			   // bruits : fréquence spatiale (1/m)
				float32 seed = 0.f;					   // bruits : graine
				float32 speed = 0.f;				   // bruits : dérive temporelle (le bruit avance de speed · t)

				NkVec3f Force(const NkVec3f &position, float32 time) const override {
					return NkEvalForceField(*this, position, time); // newtons
				}
		};

		// ── Évaluation CPU ────────────────────────────────────────────────────────
		namespace forcefield {
			inline float32 Fract(float32 x) {
				return x - floorf(x);
			}
			inline float32 Hash(float32 x, float32 y, float32 z, float32 seed) {
				const float32 d = x * 127.1f + y * 311.7f + z * 74.7f + seed;
				return Fract(sinf(d) * 43758.5453f);
			}
			// bruit de valeur 3D dans [0,1], interpolation trilinéaire lissée
			inline float32 Noise(float32 x, float32 y, float32 z, float32 seed) {
				const float32 ix = floorf(x), iy = floorf(y), iz = floorf(z);
				float32 fx = x - ix, fy = y - iy, fz = z - iz;
				fx = fx * fx * (3.f - 2.f * fx);
				fy = fy * fy * (3.f - 2.f * fy);
				fz = fz * fz * (3.f - 2.f * fz);
				const float32 c000 = Hash(ix, iy, iz, seed), c100 = Hash(ix + 1.f, iy, iz, seed);
				const float32 c010 = Hash(ix, iy + 1.f, iz, seed), c110 = Hash(ix + 1.f, iy + 1.f, iz, seed);
				const float32 c001 = Hash(ix, iy, iz + 1.f, seed), c101 = Hash(ix + 1.f, iy, iz + 1.f, seed);
				const float32 c011 = Hash(ix, iy + 1.f, iz + 1.f, seed), c111 = Hash(ix + 1.f, iy + 1.f, iz + 1.f, seed);
				const float32 x00 = c000 + (c100 - c000) * fx, x10 = c010 + (c110 - c010) * fx;
				const float32 x01 = c001 + (c101 - c001) * fx, x11 = c011 + (c111 - c011) * fx;
				const float32 y0 = x00 + (x10 - x00) * fy, y1 = x01 + (x11 - x01) * fy;
				return y0 + (y1 - y0) * fz;
			}
			// potentiel vectoriel : trois bruits centrés dans [-1,1]
			inline NkVec3f Potential(NkVec3f p, float32 seed) {
				return {2.f * Noise(p.x, p.y, p.z, seed) - 1.f, 2.f * Noise(p.x, p.y, p.z, seed + 17.f) - 1.f,
						2.f * Noise(p.x, p.y, p.z, seed + 43.f) - 1.f};
			}
			// rotationnel du potentiel par différences centrées (pas e)
			inline NkVec3f Curl(NkVec3f p, float32 seed, float32 e) {
				const NkVec3f dx1 = Potential({p.x + e, p.y, p.z}, seed), dx0 = Potential({p.x - e, p.y, p.z}, seed);
				const NkVec3f dy1 = Potential({p.x, p.y + e, p.z}, seed), dy0 = Potential({p.x, p.y - e, p.z}, seed);
				const NkVec3f dz1 = Potential({p.x, p.y, p.z + e}, seed), dz0 = Potential({p.x, p.y, p.z - e}, seed);
				const float32 inv = 1.f / (2.f * e);
				// curl = (dPz/dy - dPy/dz, dPx/dz - dPz/dx, dPy/dx - dPx/dy)
				return {(dy1.z - dy0.z) * inv - (dz1.y - dz0.y) * inv, (dz1.x - dz0.x) * inv - (dx1.z - dx0.z) * inv,
						(dx1.y - dx0.y) * inv - (dy1.x - dy0.x) * inv};
			}
		} // namespace forcefield

		// La FORCE (N) du champ en `pos` à l'instant `t` ; le consommateur divise par sa masse.
		inline NkVec3f NkEvalForceField(const NkForceField &f, NkVec3f pos, float32 t) {
			switch (f.type) {
				case NkForceFieldType::UNIFORM:
					return {f.direction.x * f.strength, f.direction.y * f.strength, f.direction.z * f.strength};
				case NkForceFieldType::VORTEX: {
					const NkVec3f r = {pos.x - f.direction.x, pos.y - f.direction.y, pos.z - f.direction.z};
					// tangentiel : axe x r
					NkVec3f tg = {f.axis.y * r.z - f.axis.z * r.y, f.axis.z * r.x - f.axis.x * r.z, f.axis.x * r.y - f.axis.y * r.x};
					const float32 l = sqrtf(tg.x * tg.x + tg.y * tg.y + tg.z * tg.z);
					if (l < 1e-6f)
						return {0.f, 0.f, 0.f};
					float32 fall = 1.f;
					if (l > f.radius && f.radius > 0.f)
						fall = f.radius / l;
					const float32 s = f.strength * fall / l;
					return {tg.x * s, tg.y * s, tg.z * s};
				}
				case NkForceFieldType::TURBULENCE: {
					const NkVec3f p = {pos.x * f.frequency + f.speed * t, pos.y * f.frequency, pos.z * f.frequency};
					const NkVec3f n = forcefield::Potential(p, f.seed);
					return {n.x * f.strength, n.y * f.strength, n.z * f.strength};
				}
				case NkForceFieldType::CURL: {
					const NkVec3f p = {pos.x * f.frequency + f.speed * t, pos.y * f.frequency, pos.z * f.frequency};
					const NkVec3f c = forcefield::Curl(p, f.seed, 0.01f);
					return {c.x * f.strength, c.y * f.strength, c.z * f.strength};
				}
				default:
					return {0.f, 0.f, 0.f};
			}
		}

		// Les quatre vec4 que les noyaux lisent (même encodage pour les deux stockages GPU) :
		// f0 = (type, strength, frequency, speed·t) ; f1 = (direction/centre xyz, radius) ; f2 = (axis xyz, seed) ;
		// f3 = (1/masse de la particule du consommateur, 0, 0, 0) -- le noyau multiplie la force par f3.x.
		inline void NkPackForceField(const NkForceField &f, float32 t, float32 particleMass, NkVec4f &f0, NkVec4f &f1,
									 NkVec4f &f2, NkVec4f &f3) {
			f0 = {(float32)(uint8)f.type, f.strength, f.frequency, f.speed * t};
			f1 = {f.direction.x, f.direction.y, f.direction.z, f.radius};
			f2 = {f.axis.x, f.axis.y, f.axis.z, f.seed};
			f3 = {particleMass > 1e-12f ? 1.f / particleMass : 0.f, 0.f, 0.f, 0.f};
		}

		// La même évaluation en NkSL : à concaténer au préambule d'un noyau qui déclare un
		// bloc uniforme `Field { vec4 f0; vec4 f1; vec4 f2; vec4 f3; } wf;` ; nkForceField rend l'ACCELERATION (force x f3.x).
		inline const char *NkForceFieldNkSL() {
			return R"NKSL(
float ffHash(vec3 v, float seed) {
    float d = v.x * 127.1 + v.y * 311.7 + v.z * 74.7 + seed;
    return fract(sin(d) * 43758.5453);
}
float ffNoise(vec3 p, float seed) {
    vec3 i = floor(p);
    vec3 f = p - i;
    f = f * f * (vec3(3.0, 3.0, 3.0) - f * 2.0);
    float c000 = ffHash(i, seed);
    float c100 = ffHash(i + vec3(1.0, 0.0, 0.0), seed);
    float c010 = ffHash(i + vec3(0.0, 1.0, 0.0), seed);
    float c110 = ffHash(i + vec3(1.0, 1.0, 0.0), seed);
    float c001 = ffHash(i + vec3(0.0, 0.0, 1.0), seed);
    float c101 = ffHash(i + vec3(1.0, 0.0, 1.0), seed);
    float c011 = ffHash(i + vec3(0.0, 1.0, 1.0), seed);
    float c111 = ffHash(i + vec3(1.0, 1.0, 1.0), seed);
    float x00 = c000 + (c100 - c000) * f.x;
    float x10 = c010 + (c110 - c010) * f.x;
    float x01 = c001 + (c101 - c001) * f.x;
    float x11 = c011 + (c111 - c011) * f.x;
    float y0 = x00 + (x10 - x00) * f.y;
    float y1 = x01 + (x11 - x01) * f.y;
    return y0 + (y1 - y0) * f.z;
}
vec3 ffPotential(vec3 p, float seed) {
    return vec3(2.0 * ffNoise(p, seed) - 1.0, 2.0 * ffNoise(p, seed + 17.0) - 1.0, 2.0 * ffNoise(p, seed + 43.0) - 1.0);
}
vec3 ffCurl(vec3 p, float seed, float e) {
    vec3 dx1 = ffPotential(p + vec3(e, 0.0, 0.0), seed);
    vec3 dx0 = ffPotential(p - vec3(e, 0.0, 0.0), seed);
    vec3 dy1 = ffPotential(p + vec3(0.0, e, 0.0), seed);
    vec3 dy0 = ffPotential(p - vec3(0.0, e, 0.0), seed);
    vec3 dz1 = ffPotential(p + vec3(0.0, 0.0, e), seed);
    vec3 dz0 = ffPotential(p - vec3(0.0, 0.0, e), seed);
    float inv = 1.0 / (2.0 * e);
    return vec3((dy1.z - dy0.z) * inv - (dz1.y - dz0.y) * inv, (dz1.x - dz0.x) * inv - (dx1.z - dx0.z) * inv, (dx1.y - dx0.y) * inv - (dy1.x - dy0.x) * inv);
}
vec3 nkForceField(vec3 pos) {
    vec3 a = vec3(0.0, 0.0, 0.0);
    float ty = wf.f0.x;
    float st = wf.f0.y;
    if (ty > 0.5 && ty < 1.5) {
        a = vec3(wf.f1.x, wf.f1.y, wf.f1.z) * st;
    }
    if (ty > 1.5 && ty < 2.5) {
        vec3 r = pos - vec3(wf.f1.x, wf.f1.y, wf.f1.z);
        vec3 ax = vec3(wf.f2.x, wf.f2.y, wf.f2.z);
        vec3 tg = vec3(ax.y * r.z - ax.z * r.y, ax.z * r.x - ax.x * r.z, ax.x * r.y - ax.y * r.x);
        float l = length(tg);
        if (l > 0.000001) {
            float fall = 1.0;
            if (l > wf.f1.w && wf.f1.w > 0.0) { fall = wf.f1.w / l; }
            a = tg * (st * fall / l);
        }
    }
    if (ty > 2.5 && ty < 3.5) {
        vec3 p = vec3(pos.x * wf.f0.z + wf.f0.w, pos.y * wf.f0.z, pos.z * wf.f0.z);
        a = ffPotential(p, wf.f2.w) * st;
    }
    if (ty > 3.5) {
        vec3 p = vec3(pos.x * wf.f0.z + wf.f0.w, pos.y * wf.f0.z, pos.z * wf.f0.z);
        a = ffCurl(p, wf.f2.w, 0.01) * st;
    }
    return a * wf.f3.x;
}
)NKSL";
		}

	} // namespace renderer
} // namespace nkentseu
