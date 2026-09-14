#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkBuoyancy.h — L'EAU AGIT SUR LE CORPS : Archimède, le centre de carène et
// l'amortissement (2026-09-14). L'autre moitié du couplage ; la première est
// `NkWaterDisturbance.h`.
//
// Rodolf, deux fois : « les sphères ne réagissent pas à l'océan en mouvement ».
// Mesuré avant d'écrire : `NkRigidBody.h` porte déjà `ApplyForce`,
// `ApplyImpulse` et `ApplyForceAtPoint` — la PORTE existe et elle est propre.
// Ce qui manquait, c'est qui l'appelle avec une poussée d'eau : `grep -inE
// "archim|flott|buoy|immerg"` sur les 1005 lignes de `NKPhysics/tests/
// test_eau.cpp` ne rend qu'un commentaire sur l'arithmétique flottante.
//
// ── POURQUOI ICI, DANS NKMATH, ET PAS DANS NKPHYSICS ────────────────────────
// Exactement l'argument déjà écrit dans `NkIForceField.h` et `NkContactEvent.h` :
// NKRenderer ne dépend pas de NKPhysics, NKPhysics ne dépend pas de NKRenderer,
// et la seule couche que les deux consomment est la Foundation. Un volume
// immergé est de la géométrie. Ce fichier ne connaît donc AUCUN `NkRigidBody` :
// il prend un centre, un rayon, une masse et une vitesse, et rend une FORCE en
// newtons. L'appelant fait `body.ApplyForce(r.force)` — six lignes chez l'hôte.
//
// ── POURQUOI PAS `NkIForceField` ────────────────────────────────────────────
// Son contrat est `Force(position, time)` sur une particule PONCTUELLE. La
// poussée d'Archimède n'est pas ponctuelle : elle vaut rho g V où V est le
// volume IMMERGÉ du corps, et elle s'applique au centre de CARÈNE, qui n'est pas
// le centre de masse — c'est même ce qui fait le couple de redressement d'un
// bateau. Un corps branché sur `NkIForceField` recevrait la même force à dix
// mètres au-dessus de l'eau qu'à trois mètres dessous. On le NOMME au lieu de
// plier le contrat existant.
//
// ── LA LOI, ÉCRITE AVANT LE CODE ────────────────────────────────────────────
// Archimède : la poussée vaut le POIDS DU FLUIDE DÉPLACÉ, vers le haut.
//        F_poussee = rho_eau * g * V_immerge * (+Y)
// Le +Y est exact et n'est PAS la normale de la surface : Archimède est
// verticale par construction (elle intègre une pression hydrostatique, qui ne
// dépend que de la profondeur). Incliner la poussée sur la normale de la vague
// serait joli et faux ; ce qui fait vraiment cavaler un objet sur une vague,
// c'est la pente de la surface de pression, et on ne la modélise pas ici. Dit.
//
// ── LE VOLUME IMMERGÉ D'UNE SPHÈRE, DÉRIVÉ ET NON RECOPIÉ ───────────────────
// Sphère de rayon r centrée en 0, plan d'eau à la cote c (relative au centre,
// donc c = ys - cy), immergé = tout ce qui est SOUS le plan :
//        V(c) = INT_{-r}^{c} pi (r^2 - y^2) dy = pi ( r^2 c - c^3/3 + 2 r^3/3 )
// Contrôles immédiats : c = -r -> 0 ; c = +r -> 4 pi r^3 / 3. Les deux sont des
// témoins du banc, pas des évidences.
// Le MOMENT, pour le centre de carène :
//        M(c) = INT_{-r}^{c} y pi (r^2 - y^2) dy = pi ( r^2 c^2/2 - c^4/4 - r^4/4 )
//        y_carene = M / V   (relatif au centre de la sphère)
// Contrôle : c = +r donne M = 0, donc la carène est au centre — ce qui est la
// bonne réponse pour une sphère entièrement immergée.
//
// ── L'AMORTISSEMENT, ET CE QU'IL EST VRAIMENT ───────────────────────────────
// Un corps posé sur une eau parfaitement élastique OSCILLE POUR TOUJOURS :
// masse + ressort hydrostatique, sans perte. Il faut une perte, et c'est
// physique — un flotteur qui monte et descend RAYONNE des vagues, et cette
// énergie part. On la modélise par une traînée LINÉAIRE, pondérée par la
// fraction immergée :
//        F_trainee = - lambda * (V_immerge / V_total) * v
// ⚠️ `v` est la vitesse ABSOLUE, pas la vitesse relative à l'eau. C'est un choix,
// et il est nommé : l'amortissement de rayonnement s'oppose au mouvement dans le
// repère du fluide au repos au loin. Prendre la vitesse relative modéliserait la
// traînée visqueuse, qui est une AUTRE physique et qui ferait suivre la vague
// trop parfaitement — le banc ne mesurerait plus rien.
//
// ── CE QUI N'EST PAS FAIT, ET QUI EST NOMMÉ ─────────────────────────────────
//   * Une seule forme : la SPHÈRE. Une boîte, une coque quelconque demandent
//     l'échantillonnage de coque du D2 de `NKSimulation/docs/ROADMAP.md` — non
//     fait, et la fonction ne prétend pas le contraire en prenant une `NkShape`.
//   * Aucun couple : pour une sphère, la carène est sur la verticale du centre,
//     donc le bras de levier est nul et le couple est EXACTEMENT zéro. Le centre
//     de carène est tout de même RENDU, parce que c'est lui qui portera le
//     couple le jour où une autre forme arrivera.
//   * Aucune masse ajoutée (l'eau entraînée avec le corps), aucune force de
//     Froude-Krylov, aucun amortissement angulaire.
// =============================================================================
#include "NKMath/NkFunctions.h"
#include "NKMath/NkVec.h"
#include "NKMath/NkWaterSurface.h"

namespace nkentseu {
	namespace math {

		struct NkBuoyancyParams {
				// kg/m^3. 1000 = eau douce à 4 degC. L'eau de mer est ~1025.
				float32 waterDensity = 1000.f;
				float32 gravity = NK_GRAVITE_NORMALE;
				// N.s/m : l'amortissement de rayonnement, pondéré par la fraction
				// immergée. 0 = un flotteur qui oscille sans fin (et c'est un contrôle
				// négatif légitime, pas un bug).
				float32 linearDamping = 0.f;
		};

		struct NkBuoyancyResult {
				float32 surfaceY = 0.f;		 // m, hauteur de l'eau à (x, z) du corps
				float32 submersion = 0.f;	 // m, d = ys - (cy - r), borné à [0, 2r]
				float32 immersedVolume = 0.f; // m^3
				float32 totalVolume = 0.f;	 // m^3
				NkVec3f centerOfBuoyancy = {0.f, 0.f, 0.f}; // monde
				NkVec3f buoyancy = {0.f, 0.f, 0.f};		   // N, poussée seule
				NkVec3f drag = {0.f, 0.f, 0.f};			   // N, traînée seule
				NkVec3f force = {0.f, 0.f, 0.f};		   // N, la somme (ce qu'on applique)
				NkVec3f normal = {0.f, 1.f, 0.f};		   // normale de la surface à (x, z)
		};

		// Volume d'une sphère de rayon `r` sous un plan à la cote `c` RELATIVE au
		// centre. Hors de [-r, +r], la réponse est 0 ou la sphère entière — et c'est
		// borné ICI plutôt que chez chaque appelant.
		NK_FORCE_INLINE float32 NkSphereVolumeBelow(float32 r, float32 c) noexcept {
			if (r <= 0.f)
				return 0.f;
			if (c <= -r)
				return 0.f;
			if (c >= r)
				return 4.1887902f * r * r * r; // 4 pi / 3
			return 3.1415926535f * (r * r * c - c * c * c / 3.f + 2.f * r * r * r / 3.f);
		}

		// Le moment vertical de la partie immergée, RELATIF au centre de la sphère.
		// Rendu séparément du volume pour que le banc puisse éprouver l'un sans
		// l'autre : un centre de carène faux avec un volume juste est un défaut
		// qu'un seul chiffre confondrait.
		NK_FORCE_INLINE float32 NkSphereMomentBelow(float32 r, float32 c) noexcept {
			if (r <= 0.f)
				return 0.f;
			if (c <= -r)
				return 0.f;
			if (c >= r)
				return 0.f; // sphère entière : le moment autour du centre est nul
			const float32 r2 = r * r;
			return 3.1415926535f * (r2 * c * c * 0.5f - c * c * c * c * 0.25f - r2 * r2 * 0.25f);
		}

		// LE VOLUME IMMERGÉ EXPRIMÉ EN ENFONCEMENT, la forme dont le banc a besoin :
		// `d` est mesuré depuis le BAS de la sphère (0 = elle affleure, 2r = noyée).
		NK_FORCE_INLINE float32 NkSphereImmersedVolume(float32 r, float32 d) noexcept {
			return NkSphereVolumeBelow(r, d - r);
		}

		// ── LA POUSSÉE SUR UNE SPHÈRE ────────────────────────────────────────────
		// `waves` + `disturbance` : la MÊME surface que celle qui est peinte. C'est
		// la condition écrite dans `NkWaterSurface.h` — deux surfaces, et le corps
		// flotterait à côté de sa propre trace.
		inline NkBuoyancyResult NkBuoyancySphere(const NkBuoyancyParams &bp, const NkWaterParams &waves,
												 const NkVec3f &center, float32 radius,
												 const NkVec3f &velocity, float32 time,
												 const NkWaterDisturbance *disturbance = nullptr,
												 float32 baseY = 0.f) noexcept {
			NkBuoyancyResult o;
			o.totalVolume = 4.1887902f * radius * radius * radius;
			// ⚠️ La surface est évaluée à l'aplomb du CENTRE (x, z) du corps, sur le
			// point de repos — pas sur le point DÉPLACÉ de Gerstner. C'est une
			// approximation, et la voici nommée : Gerstner bouge aussi en x et z, donc
			// la nappe au-dessus du corps n'est pas exactement celle de son (x, z) de
			// repos. `NkWaterInverseXZ` sait inverser ce déplacement ; l'appeler à
			// chaque pas pour chaque corps coûterait jusqu'à 24 évaluations de houle,
			// et l'écart est du second ordre en cambrure. Non fait, et dit.
			const NkWaterPoint w = NkWaterEval(waves, center.x, center.z, time, -1.f, disturbance);
			o.surfaceY = baseY + w.position.y;
			o.normal = w.normal;

			const float32 c = o.surfaceY - center.y; // cote de l'eau relative au centre
			o.submersion = NkClamp(c + radius, 0.f, 2.f * radius);
			o.immersedVolume = NkSphereVolumeBelow(radius, c);
			if (o.immersedVolume <= 0.f) {
				o.centerOfBuoyancy = center; // hors de l'eau : pas de carène, on rend le centre
				return o;					 // force nulle : l'appelant n'ajoute RIEN
			}
			const float32 m = NkSphereMomentBelow(radius, c);
			o.centerOfBuoyancy = NkVec3f{center.x, center.y + m / o.immersedVolume, center.z};

			o.buoyancy = NkVec3f{0.f, bp.waterDensity * bp.gravity * o.immersedVolume, 0.f};
			if (bp.linearDamping != 0.f && o.totalVolume > 0.f) {
				const float32 f = o.immersedVolume / o.totalVolume;
				o.drag = velocity * (-bp.linearDamping * f);
			}
			o.force = o.buoyancy + o.drag;
			return o;
		}

		// ── L'ENFONCEMENT D'ÉQUILIBRE, PAR BISSECTION ────────────────────────────
		// À l'équilibre, poussée = poids : rho g V = m g, donc V = m / rho. On
		// cherche la cote `c` telle que `NkSphereVolumeBelow(r, c) == m / rho`.
		//
		// ⚠️ CE QUE CETTE FONCTION EST : un OUTIL, pas un attendu de témoin. Un banc
		// qui s'en servirait pour fabriquer son attendu comparerait la bibliothèque à
		// elle-même. L'énoncé indépendant est « V_immerge = masse / densite_eau »,
		// qui vient d'Archimède ; le banc pose CET énoncé et résout lui-même.
		// V est strictement croissant en c sur [-r, r] (sa dérivée pi(r^2-c^2) est
		// positive), donc la bissection converge sans ambiguïté.
		// Rend faux si le corps est plus dense que l'eau : il n'y a PAS d'équilibre,
		// et rendre `c = r` en silence serait affirmer qu'il flotte au ras.
		inline bool NkBuoyancyEquilibriumSphere(float32 radius, float32 mass, float32 waterDensity,
												float32 &outC, uint32 iterations = 60u) noexcept {
			outC = 0.f;
			if (radius <= 0.f || waterDensity <= 0.f || mass < 0.f)
				return false;
			const float32 vNeeded = mass / waterDensity;
			const float32 vTotal = 4.1887902f * radius * radius * radius;
			if (vNeeded > vTotal)
				return false; // plus dense que l'eau : il coule, et ça se DIT
			float32 lo = -radius, hi = radius;
			for (uint32 i = 0; i < iterations; ++i) {
				const float32 mid = 0.5f * (lo + hi);
				if (NkSphereVolumeBelow(radius, mid) < vNeeded)
					lo = mid;
				else
					hi = mid;
			}
			outC = 0.5f * (lo + hi);
			return true;
		}

		// ── LE RAYON DE FLOTTAISON ───────────────────────────────────────────────
		// Le cercle que la sphère découpe dans le plan d'eau : a = sqrt(r^2 - c^2).
		// C'est lui qui donne la RAIDEUR hydrostatique k = rho g pi a^2 (le banc en a
		// besoin pour écrire sa borne AVANT la mesure) et c'est lui qui dimensionne
		// la carène que le corps creuse dans `NkWaterDisturbance`.
		NK_FORCE_INLINE float32 NkSphereWaterlineRadius(float32 radius, float32 c) noexcept {
			if (radius <= 0.f)
				return 0.f;
			const float32 a2 = radius * radius - c * c;
			return a2 > 0.f ? NkSqrt(a2) : 0.f;
		}

	} // namespace math
} // namespace nkentseu
