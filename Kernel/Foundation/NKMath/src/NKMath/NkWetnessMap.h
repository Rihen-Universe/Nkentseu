#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkWetnessMap.h — la CARTE DE MOUILLAGE d'une surface, et ce que le mouillage
// fait à la lumière (2026-09-06). ROADMAP_PRODUITS.md §6.6, paliers 2 et 3.
// Demande explicite de Rodolf le 05/09 au soir : « n'oublie pas l'effet mouillé ».
//
// ── ESPACE : UV DE L'OBJET, pas grille monde. Choix, et ses raisons ──────────
// Les deux ont été pesés :
//   * grille monde (une texture au-dessus du terrain, en XZ) : simple à écrire
//     depuis des contacts, aucun dépliage nécessaire, mais elle NE SUIT PAS
//     l'objet. Une nappe mouillée qu'on soulève redeviendrait sèche, un mur ne
//     pourrait pas être mouillé du tout (deux points du mur ont le même XZ), et
//     le palier 3 (« le tissu mouillé est plus lourd ») serait impossible.
//   * UV de l'objet : la carte VOYAGE avec la surface. C'est le seul espace où
//     « ce tissu-là est mouillé » veut dire quelque chose quand il bouge, et
//     c'est déjà l'espace où le nuanceur PBR échantillonne ses textures : une
//     entrée matériau de plus, aucun repère nouveau.
// Ce que le choix coûte, dit : il faut une paramétrisation. Pour un plan et pour
// une nappe de tissu (une grille) elle est immédiate ; pour un maillage importé
// c'est le dépliage UV, qui existe ailleurs dans le dépôt et n'est pas de ce lot.
// Le pont entre le monde et l'UV est un PROJECTEUR (NkPlanarProjector ici) : les
// contacts arrivent en mètres, la carte s'écrit en UV.
//
// MÉMOIRE, mesurée et non estimée : la carte est un tableau de float32, donc
// 4 octets par texel -- 256 x 256 = 262 144 octets (256 Kio) par surface.
// `Bytes()` le rend. Le float32 est un choix de BANC (les témoins comparent des
// écarts de quelques millièmes) ; la texture envoyée au GPU est en R8, soit
// 65 536 octets pour la même carte -- un quart de bit de bruit sur un canal qui
// pilote une rugosité, c'est invisible et c'est dit.
//
// ── SÉCHAGE : décroissance exponentielle ────────────────────────────────────
//        w(t + dt) = w(t) * exp(-dt / tau),  tau = NkWetMaterial::dryingTime (s)
// tau est la constante de temps : après tau il reste 1/e ~ 37 % du mouillage,
// après 3 tau il en reste 5 %. ⚠️ Les valeurs de tau par matière sont des
// constantes de DIRECTION ARTISTIQUE, pas une mesure de séchage réel : je les
// donne avec cet avertissement, et l'ORDRE est ce qui compte (le sable sèche
// vite, le cuir lentement -- consigne de Rodolf). Le témoin ne vérifie pas
// « 20 secondes », il vérifie que le sable est sec avant le cuir.
//
// ── CE QUE LE MOUILLAGE FAIT À LA LUMIÈRE : NkApplyWetness ──────────────────
// Formule retenue = celle de Sébastien Lagarde, « Water drop 3b - Physically
// based wet surfaces » (seblagarde.wordpress.com, 14/04/2013), relue le 06/09,
// dans ses termes exacts (glossiness, porosity dans [0, 1]) :
//        factor  = lerp(1, 0.2, Porosity)
//        Diffuse = Diffuse * lerp(1.0, factor, WetLevel)
//        Gloss   = lerp(1.0, Gloss, lerp(1, factor, 0.5 * WetLevel))
// Le 0,5 est de lui, et sa raison est écrite dans le billet : « to limit the
// specular boost ». Notre matériau parle en RUGOSITÉ (rugosite = 1 - gloss), et
// la conversion est algébrique, pas une seconde formule :
//        gloss' = 1 - t * (1 - gloss)  avec t = 1 - 0.5 * wet * (1 - factor)
//   =>   rugosite' = t * rugosite = rugosite * (1 - 0.4 * porosite * wet)
// Les trois effets demandés y sont : albédo assombri (jusqu'à x0,2 à porosité 1),
// rugosité qui descend, et donc réflexion spéculaire resserrée et plus vive.
//
// ⚠️ CE QUE LAGARDE NE FAIT PAS, et que j'ajoute EN LE DISANT : il écrit
// explicitement que le changement d'indice de réfraction « is not visible enough
// in game to be taken into account » et ne pose pas de F0 d'eau. Le brief
// demande une « réflexion spéculaire renforcée » : `waterLayer` (0 par défaut,
// donc Lagarde pur) fait glisser F0 vers celui de l'eau. Ce F0 n'est pas
// mémorisé, il est CALCULÉ par Fresnel à incidence normale pour n = 1,33 :
//        F0 = ((n - 1) / (n + 1))^2 = (0,33 / 2,33)^2 = 0,02006...
// =============================================================================
#include "NKMath/NkFunctions.h"
#include "NKMath/NkVec.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace math {

		// Ce qu'une MATIÈRE fait de l'eau qu'elle reçoit.
		struct NkWetMaterial {
				// tau du séchage (s). ⚠️ Direction artistique, pas une mesure -- voir l'en-tête.
				// Ordre voulu (Rodolf) : sable vite, cuir lentement.
				float32 dryingTime = 20.f;
				// Porosité [0, 1] : 0 = verre/métal poli (l'eau glisse, presque aucun
				// assombrissement), 1 = sable/béton (l'eau entre, l'albédo tombe à x0,2).
				float32 porosity = 1.f;
				// Combien un contact dépose, par unité de vitesse d'approche (1/(m/s)).
				float32 absorption = 1.f;
				// Gain de masse à saturation, en fraction de la masse sèche (tissu).
				// Ordre de grandeur usuel du textile ; c'est un paramètre, pas une constante.
				float32 saturatedMassGain = 0.4f;
				// Facteur multiplicatif de la compliance XPBD à saturation : > 1 = le
				// tissu MOUILLÉ CÈDE davantage (il tombe, il colle) ; < 1 = il raidit.
				float32 saturatedComplianceGain = 3.f;
				// Mélange vers le F0 de l'eau (0 = Lagarde pur, 1 = pellicule d'eau franche).
				float32 waterLayer = 0.f;
		};

		// Quelques matières nommées. Constantes de DIRECTION ARTISTIQUE (en-tête) ;
		// ce qui est garanti est l'ORDRE des temps de séchage, pas leur valeur.
		NK_FORCE_INLINE NkWetMaterial NkWetSand() noexcept {
			NkWetMaterial m;
			m.dryingTime = 20.f;
			m.porosity = 1.f;
			return m;
		}
		NK_FORCE_INLINE NkWetMaterial NkWetLeather() noexcept {
			NkWetMaterial m;
			m.dryingTime = 300.f; // 15 x le sable : le cuir garde l'eau
			m.porosity = 0.6f;
			return m;
		}
		NK_FORCE_INLINE NkWetMaterial NkWetCloth() noexcept {
			NkWetMaterial m;
			m.dryingTime = 120.f;
			m.porosity = 0.85f;
			m.saturatedMassGain = 0.6f;
			return m;
		}
		NK_FORCE_INLINE NkWetMaterial NkWetStone() noexcept {
			NkWetMaterial m;
			m.dryingTime = 60.f;
			m.porosity = 0.35f;
			return m;
		}

		// Ce que le nuanceur applique. Le CPU (témoins) et NkSL calculent la MÊME chose :
		// cette fonction est la référence, le .nksl la recopie ligne pour ligne.
		struct NkWetShading {
				NkVec3f albedo = {1.f, 1.f, 1.f};
				float32 roughness = 1.f;
				float32 f0 = 0.04f;
		};

		NK_FORCE_INLINE NkWetShading NkApplyWetness(const NkVec3f &albedoDry, float32 roughnessDry, float32 f0Dry,
													float32 wet, const NkWetMaterial &mat) noexcept {
			const float32 w = NkClamp(wet, 0.f, 1.f);
			const float32 porosity = NkClamp(mat.porosity, 0.f, 1.f);
			// Lagarde 3b, mot pour mot
			const float32 factor = 1.f + (0.2f - 1.f) * porosity; // lerp(1, 0.2, porosity)
			const float32 dif = 1.f + (factor - 1.f) * w;		  // lerp(1, factor, WetLevel)
			const float32 t = 1.f + (factor - 1.f) * (0.5f * w);  // lerp(1, factor, 0.5 * WetLevel)
			NkWetShading o;
			o.albedo = albedoDry * dif;
			o.roughness = NkClamp(roughnessDry * t, 0.f, 1.f);
			// Pellicule d'eau (ajout assumé, hors Lagarde) : F0 de l'eau par Fresnel, n = 1,33.
			const float32 nWater = 1.33f;
			const float32 f0Water = ((nWater - 1.f) / (nWater + 1.f)) * ((nWater - 1.f) / (nWater + 1.f));
			const float32 k = NkClamp(mat.waterLayer, 0.f, 1.f) * w;
			o.f0 = f0Dry + (f0Water - f0Dry) * k;
			return o;
		}

		// =====================================================================
		// La carte : un canal accumulé en espace UV [0, 1]^2, qui sèche.
		// =====================================================================
		class NkWetnessMap {
			public:
				NkWetMaterial material;

				void Create(uint32 width, uint32 height) {
					mW = width ? width : 1u;
					mH = height ? height : 1u;
					mData.Resize((size_t)mW * (size_t)mH, 0.f);
					mDry = true;
				}
				void Clear() {
					for (size_t i = 0; i < mData.Size(); ++i)
						mData[i] = 0.f;
					mDry = true;
				}

				uint32 Width() const noexcept {
					return mW;
				}
				uint32 Height() const noexcept {
					return mH;
				}
				// Mémoire de la carte CPU (float32). La texture GPU équivalente en R8
				// vaut le quart : Bytes() / 4.
				uint32 Bytes() const noexcept {
					return (uint32)(mData.Size() * sizeof(float32));
				}
				bool IsDry() const noexcept {
					return mDry;
				}

				// Dépose `amount` de mouillage autour de (u, v), rayon en UV, profil
				// linéaire (1 au centre, 0 au bord). Le canal sature à 1.
				void Splat(float32 u, float32 v, float32 radiusUV, float32 amount) {
					if (mData.Size() == 0 || amount <= 0.f)
						return;
					const float32 r = radiusUV > 0.f ? radiusUV : (1.f / (float32)mW);
					const int32 x0 = (int32)NkFloor((u - r) * (float32)mW);
					const int32 x1 = (int32)NkCeil((u + r) * (float32)mW);
					const int32 y0 = (int32)NkFloor((v - r) * (float32)mH);
					const int32 y1 = (int32)NkCeil((v + r) * (float32)mH);
					for (int32 y = y0; y <= y1; ++y) {
						if (y < 0 || y >= (int32)mH)
							continue;
						const float32 fy = ((float32)y + 0.5f) / (float32)mH;
						for (int32 x = x0; x <= x1; ++x) {
							if (x < 0 || x >= (int32)mW)
								continue;
							const float32 fx = ((float32)x + 0.5f) / (float32)mW;
							const float32 dx = fx - u, dy = fy - v;
							const float32 d = NkSqrt(dx * dx + dy * dy);
							if (d > r)
								continue;
							const float32 fall = 1.f - d / r;
							float32 &c = mData[(size_t)y * (size_t)mW + (size_t)x];
							c += amount * fall;
							if (c > 1.f)
								c = 1.f;
						}
					}
					mDry = false;
				}

				// Séchage : w *= exp(-dt / tau). MUTATION : `enabled = false` ne sèche
				// pas -- le témoin (d4) doit rougir.
				void Dry(float32 dt, bool enabled = true) {
					if (!enabled || dt <= 0.f || mData.Size() == 0)
						return;
					const float32 tau = material.dryingTime > 1e-6f ? material.dryingTime : 1e-6f;
					const float32 k = NkExp(-dt / tau);
					bool allDry = true;
					for (size_t i = 0; i < mData.Size(); ++i) {
						mData[i] *= k;
						if (mData[i] < 1e-4f)
							mData[i] = 0.f;
						else
							allDry = false;
					}
					mDry = allDry;
				}

				// Échantillonnage bilinéaire, bords pincés (un contact au bord d'une
				// nappe ne doit pas boucler de l'autre côté).
				float32 Sample(float32 u, float32 v) const noexcept {
					if (mData.Size() == 0)
						return 0.f;
					const float32 fx = NkClamp(u, 0.f, 1.f) * (float32)mW - 0.5f;
					const float32 fy = NkClamp(v, 0.f, 1.f) * (float32)mH - 0.5f;
					int32 x0 = (int32)NkFloor(fx), y0 = (int32)NkFloor(fy);
					const float32 tx = fx - (float32)x0, ty = fy - (float32)y0;
					const int32 x1 = Clampi(x0 + 1, 0, (int32)mW - 1), y1 = Clampi(y0 + 1, 0, (int32)mH - 1);
					x0 = Clampi(x0, 0, (int32)mW - 1);
					y0 = Clampi(y0, 0, (int32)mH - 1);
					const float32 a = AtI(x0, y0), b = AtI(x1, y0), c = AtI(x0, y1), d = AtI(x1, y1);
					return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
				}

				float32 At(uint32 x, uint32 y) const noexcept {
					return mData[(size_t)y * (size_t)mW + (size_t)x];
				}
				float32 Mean() const noexcept {
					if (mData.Size() == 0)
						return 0.f;
					float32 s = 0.f;
					for (size_t i = 0; i < mData.Size(); ++i)
						s += mData[i];
					return s / (float32)mData.Size();
				}
				float32 Max() const noexcept {
					float32 m = 0.f;
					for (size_t i = 0; i < mData.Size(); ++i)
						if (mData[i] > m)
							m = mData[i];
					return m;
				}
				// Somme du canal : c'est ce qui pilote la MASSE (une nappe deux fois
				// plus mouillée est deux fois plus lourde en eau).
				float32 Sum() const noexcept {
					float32 s = 0.f;
					for (size_t i = 0; i < mData.Size(); ++i)
						s += mData[i];
					return s;
				}
				const float32 *Data() const noexcept {
					return mData.Data();
				}
				float32 *Data() noexcept {
					return mData.Data();
				}

			private:
				static int32 Clampi(int32 v, int32 lo, int32 hi) noexcept {
					return v < lo ? lo : (v > hi ? hi : v);
				}
				float32 AtI(int32 x, int32 y) const noexcept {
					return mData[(size_t)y * (size_t)mW + (size_t)x];
				}
				NkVector<float32> mData;
				uint32 mW = 0, mH = 0;
				bool mDry = true;
		};

		// =====================================================================
		// Le pont monde -> UV : projection PLANAIRE sur une boîte alignée.
		// C'est le projecteur des surfaces horizontales (sable, sol, plan d'eau) ;
		// une nappe de tissu n'en a pas besoin (sa grille EST son UV).
		// =====================================================================
		class NkPlanarProjector {
			public:
				NkVec3f origin = {0.f, 0.f, 0.f}; // coin (u = 0, v = 0)
				NkVec3f du = {1.f, 0.f, 0.f};	  // le vecteur qui va de u = 0 à u = 1
				NkVec3f dv = {0.f, 0.f, 1.f};	  // idem pour v

				// Faux si le point tombe hors du carreau (le contact n'écrit rien).
				bool Project(const NkVec3f &p, float32 &u, float32 &v) const noexcept {
					const NkVec3f d = p - origin;
					const float32 lu = du.x * du.x + du.y * du.y + du.z * du.z;
					const float32 lv = dv.x * dv.x + dv.y * dv.y + dv.z * dv.z;
					if (lu <= 1e-12f || lv <= 1e-12f)
						return false;
					u = (d.x * du.x + d.y * du.y + d.z * du.z) / lu;
					v = (d.x * dv.x + d.y * dv.y + d.z * dv.z) / lv;
					return u >= 0.f && u <= 1.f && v >= 0.f && v <= 1.f;
				}
		};

	} // namespace math
} // namespace nkentseu
