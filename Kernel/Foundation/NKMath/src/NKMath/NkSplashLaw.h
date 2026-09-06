#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkSplashLaw.h — la LOI d'éclaboussure : d'un contact publié aux gouttes qui
// en naissent (2026-09-06). ROADMAP_PRODUITS.md §6.6, palier 1.
//
// Pourquoi ICI et pas dans NKRenderer : ce fichier ne fabrique pas de
// particules, il fabrique des NOMBRES et des DIRECTIONS. L'adaptateur qui
// transforme un NkSplashDrop en NkParticleBirth tient en six lignes et vit chez
// le système d'effets (NKRenderer/Tools/VFX/NkSplashEmitter.h). Séparer les deux
// met la loi -- la seule chose qui puisse être fausse -- dans le module dont les
// tests sont les moins chers à construire, et la rend lisible par le tissu, la
// mousse ou l'audio le jour où ils la voudront.
//
// ⚠️ LA LOI RETENUE, ET LA CONTRADICTION QU'ELLE TRANCHE — à lire avant de
// changer un chiffre. ROADMAP_PRODUITS.md §6.6 dit « émission de particules à
// l'impact » et le brief du 05/09 demande deux choses qui ne peuvent pas être
// vraies ensemble : « nombre proportionnel à l'ÉNERGIE » (donc en v²) et
// « deux fois la vitesse -> environ deux fois plus de gouttes » (donc en v).
// J'ai retenu la loi LINÉAIRE, parce que c'est celle qui a un témoin écrit :
//
//        n(vn) = dropsPerRef * ( (vn - speedThreshold) / speedRef ) ^ exponent
//        avec exponent = 1 PAR DÉFAUT, et n = 0 tant que vn <= speedThreshold.
//
// `exponent` est un PARAMÈTRE, pas une constante : `exponent = 2` rejoue la loi
// en énergie, et le témoin (b3) mesure que l'instrument sait distinguer les deux
// (rapport ~2 contre ~4 en doublant la vitesse). Ce qui reste à trancher par
// Rodolf est nommé dans le rapport, pas caché dans une valeur par défaut.
//
// ARRIÈRE-PLAN, cité avec sa condition : Ihmsen, Akinci, Akinci, Teschner,
// « Unified spray, foam and air bubbles for particle-based fluids », The Visual
// Computer 28(6-8), 2012, génère la matière diffuse à partir de POTENTIELS
// normalisés-bornés, dont un potentiel d'énergie cinétique (donc quadratique en
// vitesse) multiplié par un potentiel d'air piégé. ⚠️ Je n'ai PAS pu ouvrir le
// PDF (le fichier rendu par cg.informatik.uni-freiburg.de n'était pas
// décodable le 06/09) : je cite le principe, PAS des numéros d'équation ni des
// constantes. Notre loi est plus simple et assumée comme telle : un seuil, une
// puissance, un plafond.
//
// ARRONDI STOCHASTIQUE. n est réel ; le nombre de gouttes est entier. Prendre
// floor() écrase tout ce qui est sous 1 goutte (un contact à 0,6 goutte n'en
// produirait jamais) et casse la proportionnalité près du seuil. Ici la partie
// fractionnaire est tirée par un HACHAGE DÉTERMINISTE de l'événement : sur un
// grand nombre de contacts la moyenne vaut n exactement, et deux exécutions du
// même banc rendent le même total (témoin (b5)).
// =============================================================================
#include "NKMath/NkContactEvent.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {
	namespace math {

		struct NkSplashParams {
				// Sous ce seuil d'impact, AUCUNE goutte (m/s). Le fluide au repos sur le
				// fond touche en permanence : sans seuil, il pleuvrait des gouttes.
				float32 speedThreshold = 0.5f;
				float32 speedRef = 1.f;		 // m/s : la vitesse qui vaut `dropsPerRef` gouttes
				float32 dropsPerRef = 4.f;	 // gouttes à (vn - seuil) = speedRef
				float32 exponent = 1.f;		 // 1 = linéaire (le témoin) ; 2 = en énergie
				uint32 maxDropsPerEvent = 32u; // plafond DIT : un impact ne peut pas noyer la file
				float32 coneHalfAngle = 0.6f;  // rad (~34°) autour de la réflexion
				float32 speedScale = 0.35f;	 // vitesse de la goutte = speedScale * |v| du contact
				float32 speedJitter = 0.3f;	 // +/- fraction sur cette vitesse
				float32 life = 1.2f;		 // s
				float32 offset = 0.01f;		 // m : la goutte naît à cette distance de la surface
				uint32 seed = 0x9E3779B9u;	 // graine du hachage (déterminisme)
		};

		// Une goutte à créer. L'adaptateur du système d'effets la recopie dans un
		// NkParticleBirth ; rien ici ne connaît les particules.
		struct NkSplashDrop {
				NkVec3f position = {0.f, 0.f, 0.f};
				NkVec3f velocity = {0.f, 0.f, 0.f};
				float32 life = 1.f;
		};

		// Hachage 32 bits (Wang / « integer hash », variante xorshift-multiply) : deux
		// entiers dedans, une valeur bien répartie dehors, sans état ni allocation.
		// Il n'y a PAS d'aléa ici : le même événement rend toujours les mêmes gouttes.
		NK_FORCE_INLINE uint32 NkSplashHash(uint32 a, uint32 b) noexcept {
			uint32 x = a * 0x9E3779B1u ^ (b + 0x85EBCA6Bu + (a << 6) + (a >> 2));
			x ^= x >> 16;
			x *= 0x7FEB352Du;
			x ^= x >> 15;
			x *= 0x846CA68Bu;
			x ^= x >> 16;
			return x;
		}

		// [0, 1)
		NK_FORCE_INLINE float32 NkSplashHash01(uint32 a, uint32 b) noexcept {
			return (float32)(NkSplashHash(a, b) >> 8) * (1.f / 16777216.f);
		}

		// Le nombre RÉEL de gouttes que la loi demande (avant arrondi) : c'est CE
		// chiffre que le témoin de proportionnalité doit lire, pas un compte entier.
		NK_FORCE_INLINE float32 NkSplashDropCountReal(const NkSplashParams &p, float32 normalSpeed) noexcept {
			const float32 over = normalSpeed - p.speedThreshold;
			if (over <= 0.f || p.speedRef <= 0.f)
				return 0.f;
			const float32 t = over / p.speedRef;
			float32 n = p.dropsPerRef * (p.exponent == 1.f ? t : NkPow(t, p.exponent));
			const float32 cap = (float32)p.maxDropsPerEvent;
			return n > cap ? cap : n;
		}

		// Le nombre ENTIER pour CET événement (arrondi stochastique déterministe).
		NK_FORCE_INLINE uint32 NkSplashDropCount(const NkSplashParams &p, const NkContactEvent &e) noexcept {
			const float32 n = NkSplashDropCountReal(p, e.normalSpeed);
			if (n <= 0.f)
				return 0u;
			const uint32 base = (uint32)n;
			const float32 frac = n - (float32)base;
			const uint32 bump = (NkSplashHash01(e.index ^ p.seed, (uint32)(e.time * 1000.f)) < frac) ? 1u : 0u;
			uint32 k = base + bump;
			if (k > p.maxDropsPerEvent)
				k = p.maxDropsPerEvent;
			return k;
		}

		// Écrit jusqu'à `maxOut` gouttes pour l'événement `e`. Rend le nombre écrit.
		// ⚠️ Si la loi en demande plus que `maxOut`, la différence est PERDUE : c'est
		// à l'appelant de la compter (NkSplashEmitter le fait).
		inline uint32 NkSplashEmit(const NkSplashParams &p, const NkContactEvent &e, NkSplashDrop *out,
								   uint32 maxOut) noexcept {
			const uint32 want = NkSplashDropCount(p, e);
			if (want == 0u || out == nullptr || maxOut == 0u)
				return 0u;
			const uint32 k = want < maxOut ? want : maxOut;

			// Réflexion de la vitesse incidente sur la normale : r = v - 2 (v.n) n.
			// e.velocity approche la surface (v.n < 0), donc r s'en éloigne.
			const NkVec3f n = e.normal;
			NkVec3f v = e.velocity;
			const float32 vlen = NkSqrt(v.x * v.x + v.y * v.y + v.z * v.z);
			NkVec3f r = n;
			if (vlen > 1e-6f) {
				const NkVec3f d = v * (1.f / vlen);
				const float32 dn = d.x * n.x + d.y * n.y + d.z * n.z;
				r = d - n * (2.f * dn);
				const float32 rl = NkSqrt(r.x * r.x + r.y * r.y + r.z * r.z);
				r = rl > 1e-6f ? r * (1.f / rl) : n;
			}
			// Une réflexion RASANTE renverrait les gouttes dans le solide dès qu'on
			// ouvre le cône : on la redresse vers la normale de la moitié de l'angle.
			// Dit, parce que c'est un choix, pas une loi.
			{
				NkVec3f mix = r + n;
				const float32 ml = NkSqrt(mix.x * mix.x + mix.y * mix.y + mix.z * mix.z);
				if (ml > 1e-6f)
					r = mix * (1.f / ml);
			}

			// Base orthonormée autour de r (Duff et al., « Building an Orthonormal
			// Basis, Revisited », JCGT 6(1), 2017 -- la version sans branche instable).
			const float32 sg = r.z >= 0.f ? 1.f : -1.f;
			const float32 a = -1.f / (sg + r.z);
			const float32 b = r.x * r.y * a;
			const NkVec3f t1 = {1.f + sg * r.x * r.x * a, sg * b, -sg * r.x};
			const NkVec3f t2 = {b, sg + r.y * r.y * a, -r.y};

			const float32 dropSpeed = p.speedScale * (vlen > 1e-6f ? vlen : e.normalSpeed);

			for (uint32 i = 0; i < k; ++i) {
				const uint32 h0 = NkSplashHash(e.index ^ p.seed, i * 2u + 1u);
				const uint32 h1 = NkSplashHash(e.index ^ (p.seed + 0x51u), i * 2u + 2u);
				const float32 u0 = (float32)(h0 >> 8) * (1.f / 16777216.f);
				const float32 u1 = (float32)(h1 >> 8) * (1.f / 16777216.f);
				// Direction uniforme DANS le cône (cos uniforme entre cos(theta) et 1)
				const float32 cosT = 1.f - u0 * (1.f - NkCos(p.coneHalfAngle));
				const float32 sinT = NkSqrt(NkMax(0.f, 1.f - cosT * cosT));
				const float32 phi = 6.2831853f * u1;
				const NkVec3f dir = r * cosT + t1 * (sinT * NkCos(phi)) + t2 * (sinT * NkSin(phi));

				const float32 jit = 1.f + p.speedJitter * (2.f * NkSplashHash01(e.index ^ 0xABCDu, i) - 1.f);
				out[i].position = e.position + n * p.offset;
				out[i].velocity = dir * (dropSpeed * jit);
				out[i].life = p.life;
			}
			return k;
		}

	} // namespace math
} // namespace nkentseu
