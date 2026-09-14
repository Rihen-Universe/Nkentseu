#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkWaterDisturbance.h — L'ÉTAT QUI MANQUAIT À L'EAU (2026-09-14).
//
// Rodolf, deux fois : « quand c'est calme on ne ressent pas l'impact du cube en
// mouvement sur l'eau ». Ouvert `NkWaterSurface.h` : `NkWaterEval` est une
// FONCTION PURE de (x, z, t). Un corps ne peut pas influencer une formule — il
// n'y a rien à perturber. Ce fichier est ce « rien » : le SEUL état de la
// surface d'eau, ajouté à côté de l'évaluateur analytique, jamais dedans.
//
// ── CE QUE CE FICHIER EST, ET CE QU'IL N'EST PAS ────────────────────────────
// 🔴 CE N'EST PAS UNE SIMULATION DE FLUIDE. L'eau reste ANALYTIQUE : la houle de
// Gerstner n'est ni remplacée ni recalculée. On AJOUTE un terme de hauteur, et
// c'est tout. Le solveur de grille eulérien existe déjà (NkFluidGrid, pour le
// feu et la fumée) et coûte ~170 ms par image : ce n'est pas lui qu'on veut ici,
// et le dire évite qu'un lecteur pressé branche le mauvais.
// 🔴 CE N'EST PAS NON PLUS UN MODÈLE DE SILLAGE DE KELVIN. Un vrai sillage de
// navire est un cône à 19°28' qui vient de la dispersion des ondes de gravité
// (Kelvin 1887) ; ici les rides sont ISOTROPES. C'est nommé, pas caché.
//
// ── LA FORME, ET POURQUOI ELLE EST À SUPPORT COMPACT ────────────────────────
// Une source dépose sur la surface la cloche
//
//        h(r) = a (1 - s)^2      avec  s = r^2 / R^2,  et  h = 0 dès que s >= 1
//
// Le support COMPACT (nul EXACTEMENT au-delà de R, pas « négligeable ») n'est
// pas un choix d'esthétique, c'est la condition du contrôle négatif capital :
// **sans aucun corps, la surface doit valoir la valeur analytique AU BIT.** Une
// gaussienne, qui ne s'annule jamais, ferait FUIR la perturbation sur toute la
// mer — d'un ULP, donc invisible à l'œil et fatal à la mesure.
//
// ── L'AMPLITUDE N'EST PAS UN RÉGLAGE : ELLE SORT DU VOLUME DÉPLACÉ ──────────
// L'intégrale de la cloche sur le plan se calcule à la main (u = r/R) :
//
//        INT h dA = a INT_0^R (1-r^2/R^2)^2 2 pi r dr = a pi R^2 / 3
//
// On veut que le creux contienne EXACTEMENT le volume déplacé V. Donc
//
//        a = -3 V / (pi R^2)                              (le signe : ça CREUSE)
//
// C'est ce qui rend l'attendu de chaque témoin DÉRIVABLE des paramètres du banc
// au lieu d'être recopié : le creux à 1 m d'un corps se calcule sur une feuille.
//
// ── DEUX NATURES DE SOURCE, ET C'EST LA DIFFÉRENCE QUE RODOLF VOIT ──────────
//   * LA CARÈNE (`SetHull`) — le corps est LÀ, maintenant. Elle ne vieillit pas,
//     ne s'amortit pas, et son propriétaire la déplace ou la retire. C'est elle
//     qui fait qu'un corps POSÉ creuse l'eau autour de lui.
//   * LA RIDE (`Shed`) — le trou que le corps a VACANCÉ en avançant. Elle a un
//     âge, elle s'étale (rayon R(age) = R + spread*age) et elle s'amortit
//     (exp(-damping*age)). C'est elle qui fait la TRACE DERRIÈRE le corps —
//     et, comme rien n'est lâché DEVANT, l'amont et l'aval cessent d'être égaux.
//     L'étalement DILUE sans créer : à amortissement nul, l'intégrale de la ride
//     reste -V quel que soit son âge, puisque a decroît en 1/R(age)^2.
//
// ── L'AMORTISSEMENT N'EST PAS FACULTATIF ────────────────────────────────────
// Sans lui la mer se remplit de rides éternelles et le coût monte sans fin.
// `Prune` retire les rides dont le facteur exp(-damping*age) est passé sous un
// plancher DIT. Une ride à amortissement NUL n'est jamais retirée — c'est
// délibéré, c'est le contrôle négatif de (p3), et la fonction le dit plutôt que
// de « protéger » l'appelant en douce.
//
// ── CE QUI N'EST PAS FAIT, ET QUI EST NOMMÉ ─────────────────────────────────
//   * Aucune propagation : une ride s'étale et s'éteint sur place, elle ne
//     porte pas d'onde qui voyage avec une célérité d'Airy. Le faire demanderait
//     un ÉTAT DE CHAMP (une grille), c'est-à-dire exactement ce qu'on refuse ici.
//   * Aucune réflexion sur les berges, aucune interaction entre rides.
//   * Aucune écume, aucune goutte : les gouttes sont dans `NkSplashLaw.h`, qui
//     consomme un `NkContactEvent` — un fichier voisin, pas un concurrent.
// =============================================================================
#include "NKMath/NkFunctions.h"
#include "NKMath/NkVec.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace math {

		// Une source de perturbation. Deux natures, distinguées par `persistent`.
		struct NkWaterSource {
				NkVec2f center = {0.f, 0.f}; // (x, z) en mètres
				// Volume DÉPLACÉ, en m^3. > 0 creuse (c'est le cas d'un corps) ;
				// < 0 bombe (une source qui pousse l'eau vers le haut).
				float32 volume = 0.f;
				float32 radius = 1.f;  // R en mètres. Au-delà : contribution NULLE, pas « petite ».
				float32 birth = 0.f;   // s, instant du lâcher (ignoré si persistent)
				float32 damping = 0.f; // 1/s. 0 = ne s'éteint JAMAIS (contrôle négatif de (p3))
				float32 spread = 0.f;  // m/s d'étalement du rayon. 0 = la ride reste sur place.
				// Identifiant du propriétaire, pour `SetHull`/`RemoveHull`. 0 = aucun.
				uint32 owner = 0u;
				// true = CARÈNE (le corps est là) ; false = RIDE (elle vieillit).
				bool persistent = false;
		};

		// Ce qu'un point de la surface reçoit d'une perturbation : la hauteur AJOUTÉE
		// et les deux pentes qu'elle introduit. Les pentes sont analytiques, dérivées
		// terme à terme de la même cloche — jamais des différences finies, pour la
		// raison déjà écrite dans `NkWaterSurface.h` : une seconde version divergerait.
		struct NkWaterDisturbanceSample {
				float32 height = 0.f; // m, à AJOUTER à la hauteur de Gerstner
				float32 dydx = 0.f;	  // d(height)/dx
				float32 dydz = 0.f;	  // d(height)/dz
		};

		// L'amplitude au CENTRE d'une source, à l'instant `time`. Exposée plutôt
		// qu'enfouie dans la boucle : un témoin doit pouvoir refaire le calcul sans
		// recopier la formule — une sonde qui recopie finit par mesurer autre chose.
		//   a = -3 V / (pi R(age)^2) * exp(-damping * age)
		NK_FORCE_INLINE float32 NkWaterSourceAmplitude(const NkWaterSource &s, float32 time) noexcept {
			float32 R = s.radius;
			float32 decay = 1.f;
			if (!s.persistent) {
				const float32 age = time - s.birth;
				if (age < 0.f)
					return 0.f; // pas encore née : rien, et surtout pas « un peu »
				if (s.spread != 0.f)
					R = s.radius + s.spread * age;
				if (s.damping != 0.f)
					decay = NkExp(-s.damping * age);
			}
			if (R <= 1e-6f)
				return 0.f;
			return -3.f * s.volume * decay / (3.1415926535f * R * R);
		}

		// Le rayon EFFECTIF d'une source à l'instant `time` (il grandit pour une ride
		// qui s'étale). Au-delà, la source ne contribue RIEN.
		NK_FORCE_INLINE float32 NkWaterSourceRadius(const NkWaterSource &s, float32 time) noexcept {
			if (s.persistent || s.spread == 0.f)
				return s.radius;
			const float32 age = time - s.birth;
			return age > 0.f ? s.radius + s.spread * age : s.radius;
		}

		// LA CONTRIBUTION D'UNE SEULE SOURCE. Rendue séparément pour que les témoins
		// éprouvent la cloche AVANT d'éprouver la somme : un banc qui ne sait pas
		// mesurer une source ne saura pas expliquer un total faux.
		NK_FORCE_INLINE NkWaterDisturbanceSample NkWaterSourceSample(const NkWaterSource &s, float32 x,
																	 float32 z, float32 time) noexcept {
			NkWaterDisturbanceSample o;
			const float32 R = NkWaterSourceRadius(s, time);
			if (R <= 1e-6f)
				return o;
			const float32 dx = x - s.center.x, dz = z - s.center.y;
			const float32 invR2 = 1.f / (R * R);
			const float32 ss = (dx * dx + dz * dz) * invR2;
			if (ss >= 1.f)
				return o; // SUPPORT COMPACT : zéro EXACT, et c'est le coeur du négatif capital
			const float32 a = NkWaterSourceAmplitude(s, time);
			if (a == 0.f)
				return o;
			const float32 oneMinus = 1.f - ss;
			o.height = a * oneMinus * oneMinus;
			// d/dx (1-s)^2 = -2(1-s) * ds/dx, et ds/dx = 2 dx / R^2
			const float32 k = -4.f * a * oneMinus * invR2;
			o.dydx = k * dx;
			o.dydz = k * dz;
			return o;
		}

		// =====================================================================
		// L'ÉTAT. Possédé par l'HÔTE (une scène, un banc), passé par POINTEUR à
		// l'évaluateur. Pas un membre de `NkWaterParams` : ces paramètres-là sont
		// recopiés PAR VALEUR dans deux composants ECS et dans
		// `NkWaterMeshParams` — un état qui se duplique à chaque image est un état
		// qu'on perd. Un seul propriétaire, tout le monde le lit.
		// =====================================================================
		class NkWaterDisturbance {
			public:
				NkWaterDisturbance() = default;

				// Aucune allocation en régime : on réserve une fois.
				void Reserve(uint32 n) {
					mSources.Reserve(n);
					mCapacity = n;
				}

				void Clear() noexcept {
					mSources.Clear();
					mDropped = 0u;
					mShed = 0u;
					mPruned = 0u;
				}

				uint32 Count() const noexcept {
					return (uint32)mSources.Size();
				}
				// Rides REFUSÉES faute de place. La file DIT ce qu'elle perd : un
				// `Shed` qui rendrait vrai sans rien déposer serait un mensonge.
				uint32 Dropped() const noexcept {
					return mDropped;
				}
				uint32 ShedTotal() const noexcept {
					return mShed;
				}
				uint32 PrunedTotal() const noexcept {
					return mPruned;
				}
				const NkWaterSource *Data() const noexcept {
					return mSources.Data();
				}

				// ── LA CARÈNE ────────────────────────────────────────────────────
				// Le corps `owner` est LÀ : il creuse `volume` m^3 sur un rayon `radius`.
				// Rappelée à chaque pas avec la position courante ; elle ne vieillit pas.
				// `owner` doit être NON NUL : zéro est la valeur « aucun propriétaire »,
				// et l'accepter ferait fusionner toutes les carènes sans propriétaire en
				// une seule, en silence.
				bool SetHull(uint32 owner, const NkVec2f &center, float32 radius, float32 volume) {
					if (owner == 0u)
						return false;
					for (nk_size i = 0; i < mSources.Size(); ++i) {
						NkWaterSource &s = mSources[i];
						if (s.persistent && s.owner == owner) {
							s.center = center;
							s.radius = radius;
							s.volume = volume;
							return true;
						}
					}
					if (mCapacity != 0u && (uint32)mSources.Size() >= mCapacity) {
						++mDropped;
						return false;
					}
					NkWaterSource s;
					s.center = center;
					s.radius = radius;
					s.volume = volume;
					s.owner = owner;
					s.persistent = true;
					mSources.PushBack(s);
					return true;
				}

				// Le corps est parti (détruit, sorti de l'eau). Rend vrai s'il y était.
				bool RemoveHull(uint32 owner) noexcept {
					for (nk_size i = 0; i < mSources.Size(); ++i) {
						if (mSources[i].persistent && mSources[i].owner == owner) {
							RemoveAt(i);
							return true;
						}
					}
					return false;
				}

				// ── LA RIDE ──────────────────────────────────────────────────────
				// Le trou que le corps vient de quitter. Rend FAUX quand la file est
				// pleine, et `Dropped()` monte.
				// `owner` : le corps qui l'a lachee. Il sert a l'EXCLURE de sa propre
				// perturbation (cf. `Sample`) ; 0 = personne, la ride agit sur tout le
				// monde.
				bool Shed(const NkVec2f &center, float32 radius, float32 volume, float32 time,
						  float32 damping, float32 spread = 0.f, uint32 owner = 0u) {
					if (mCapacity != 0u && (uint32)mSources.Size() >= mCapacity) {
						++mDropped;
						return false;
					}
					NkWaterSource s;
					s.center = center;
					s.radius = radius;
					s.volume = volume;
					s.birth = time;
					s.damping = damping;
					s.spread = spread;
					s.owner = owner;
					s.persistent = false;
					mSources.PushBack(s);
					++mShed;
					return true;
				}

				// Retire les rides éteintes. `floorRatio` est le facteur exp(-d*age)
				// sous lequel une ride ne compte plus. Une ride à amortissement NUL
				// n'est JAMAIS retirée : c'est voulu, c'est le négatif de (p3), et ça
				// se voit dans le compteur au lieu d'être corrigé en douce.
				uint32 Prune(float32 time, float32 floorRatio = 1e-3f) noexcept {
					uint32 n = 0u;
					nk_size i = 0;
					while (i < mSources.Size()) {
						const NkWaterSource &s = mSources[i];
						if (!s.persistent && s.damping > 0.f) {
							const float32 age = time - s.birth;
							if (age > 0.f && NkExp(-s.damping * age) < floorRatio) {
								RemoveAt(i);
								++n;
								++mPruned;
								continue;
							}
						}
						++i;
					}
					return n;
				}

				// LA SOMME. Zéro source -> un échantillon EXACTEMENT nul (les trois
				// champs à +0.0f), ce dont l'évaluateur se sert pour ne rien toucher.
				//
				// ── 🔴 `excludeOwner`, ET POURQUOI IL EXISTE ────────────────────────
				// UN CORPS NE S'ENFONCE PAS DANS SON PROPRE CREUX. Sans cette porte, la
				// flottabilité interroge la surface à l'aplomb du corps, y trouve le
				// bassin que le corps lui-même y a creusé, en déduit moins d'eau, donc
				// moins de poussée — et le corps descend, ce qui creuse davantage. Une
				// boucle de réaction positive, et rien ne l'aurait dite : le corps
				// aurait simplement « coulé un peu », ce qu'on aurait mis sur le compte
				// de la masse. Le rendu, lui, n'exclut RIEN (`excludeOwner = 0`) : le
				// creux doit se VOIR, c'est tout l'objet du lot.
				// ⚠️ Conséquence assumée : un corps ne sent pas non plus SA PROPRE
				// trace. Pour ce modèle c'est le comportement voulu ; le jour où deux
				// corps devront s'influencer, ils le feront — leurs propriétaires
				// diffèrent.
				NkWaterDisturbanceSample Sample(float32 x, float32 z, float32 time,
												uint32 excludeOwner = 0u) const noexcept {
					NkWaterDisturbanceSample o;
					for (nk_size i = 0; i < mSources.Size(); ++i) {
						if (excludeOwner != 0u && mSources[i].owner == excludeOwner)
							continue;
						const NkWaterDisturbanceSample s = NkWaterSourceSample(mSources[i], x, z, time);
						if (s.height != 0.f)
							o.height += s.height;
						if (s.dydx != 0.f)
							o.dydx += s.dydx;
						if (s.dydz != 0.f)
							o.dydz += s.dydz;
					}
					return o;
				}

				// La hauteur seule, pour un appelant qui n'a que faire des pentes.
				float32 Height(float32 x, float32 z, float32 time,
							   uint32 excludeOwner = 0u) const noexcept {
					return Sample(x, z, time, excludeOwner).height;
				}

			private:
				void RemoveAt(nk_size i) noexcept {
					// Ordre NON conservé : la somme est commutative, et un décalage de
					// tout le tableau coûterait à chaque ride éteinte.
					const nk_size last = mSources.Size() - 1;
					if (i != last)
						mSources[i] = mSources[last];
					mSources.PopBack();
				}

				NkVector<NkWaterSource> mSources;
				uint32 mCapacity = 0u; // 0 = pas de plafond (la file grandit)
				uint32 mDropped = 0u;
				uint32 mShed = 0u;
				uint32 mPruned = 0u;
		};

		// =====================================================================
		// LE LÂCHEUR : il transforme un corps qui BOUGE en rides derrière lui.
		//
		// Pourquoi une classe et pas un appel direct à `Shed` : lâcher une ride à
		// chaque pas de temps remplirait la file en une seconde et rendrait la
		// densité de rides dépendante du PAS DE TEMPS — donc une eau qui change
		// d'aspect quand on change de fréquence de simulation. On lâche tous les
		// `stepDistance` MÈTRES PARCOURUS, ce qui ne dépend que de la trajectoire.
		// =====================================================================
		// 🔴 LA NORMALISATION PAR LE RECOUVREMENT, ET C'EST UNE MESURE QUI L'A IMPOSÉE.
		// Première version : chaque ride emportait le volume déplacé V ENTIER. Le banc
		// a mesuré, derrière un corps de 0,5 m à 3 m/s, un creux de **-0,134 m** là où
		// la carène elle-même ne creuse que **-0,0905 m** au centre — le sillage était
		// plus profond que le bateau. La cause n'est pas le noyau : c'est que deux
		// rides consécutives sont lâchées tous les `stepDistance` = 0,5 m alors que
		// leur rayon vaut R = 1,49 m, donc **six rides se recouvrent** en tout point et
		// leurs volumes s'additionnent. Le nombre de recouvrantes vaut 2R/stepDistance,
		// et diviser par lui rend au sillage un volume déplacé de l'ordre de V au lieu
		// de N fois V :
		//
		//        volume de la ride = V * gain * stepDistance / (2 R)
		//
		// Ce n'est donc PAS un facteur ajusté à l'oeil après coup : c'est le facteur
		// qui annule le double comptage, et il disparaît si l'on lâche les rides à
		// `stepDistance = 2R` (aucun recouvrement). Le témoin (p2d) mesure la borne
		// qui aurait dû crier dès le premier jour : le sillage ne creuse pas plus que
		// la carène.
		struct NkWaterWakeParams {
				// Fraction du trou vacancé qu'une ride emporte, APRÈS normalisation par
				// le recouvrement. 1 = tout. C'est un RÉGLAGE d'apparence, pas une loi,
				// et c'est le SEUL de cette structure : dit comme tel.
				float32 gain = 1.f;
				float32 damping = 0.8f;	 // 1/s
				float32 spread = 0.6f;	 // m/s d'étalement
				float32 stepDistance = 0.5f; // m parcourus entre deux lâchers
		};

		// Suit UN corps. `owner` doit être non nul.
		class NkWaterWake {
			public:
				void Reset(const NkVec2f &position) noexcept {
					mLast = position;
					mStarted = true;
					mTravel = 0.f;
				}

				// À appeler une fois par pas, avec la position (x,z) du corps, son rayon
				// de flottaison et son volume immergé. Rend le nombre de rides lâchées.
				uint32 Update(NkWaterDisturbance &field, uint32 owner, const NkVec2f &position,
							  float32 radius, float32 volume, float32 time,
							  const NkWaterWakeParams &p) {
					field.SetHull(owner, position, radius, volume);
					if (!mStarted) {
						Reset(position);
						return 0u;
					}
					const float32 dx = position.x - mLast.x, dz = position.y - mLast.y;
					const float32 d = NkSqrt(dx * dx + dz * dz);
					mTravel += d;
					mLast = position;
					if (p.stepDistance <= 1e-6f || mTravel < p.stepDistance)
						return 0u;
					uint32 n = 0u;
					// LA NORMALISATION PAR LE RECOUVREMENT (cf. le bloc au-dessus de
					// `NkWaterWakeParams`) : sans elle, six rides superposées creusent
					// six fois le volume déplacé et le sillage devient plus profond que
					// la carène — mesuré, pas redouté. Bornée à 1 : on ne CRÉE jamais
					// plus de volume qu'un lâcher sans recouvrement.
					const float32 chevauchement =
						radius > 1e-6f ? NkMin(1.f, p.stepDistance / (2.f * radius)) : 1.f;
					const float32 vRide = volume * p.gain * chevauchement;
					// Une boucle, et non un seul lâcher : un corps très rapide doit
					// laisser autant de rides qu'il a parcouru de pas, sinon sa trace
					// s'éclaircit quand il accélère — l'inverse de ce qu'on voit.
					while (mTravel >= p.stepDistance) {
						mTravel -= p.stepDistance;
						if (field.Shed(position, radius, vRide, time, p.damping, p.spread, owner))
							++n;
						else
							break; // file pleine : elle l'a déjà compté, on n'insiste pas
					}
					return n;
				}

			private:
				NkVec2f mLast = {0.f, 0.f};
				bool mStarted = false;
				float32 mTravel = 0.f;
		};

	} // namespace math
} // namespace nkentseu
