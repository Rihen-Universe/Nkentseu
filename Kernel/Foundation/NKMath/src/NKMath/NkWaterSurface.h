#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkWaterSurface.h — L'ÉTENDUE D'EAU : la houle de Gerstner, la profondeur, et
// ce que la profondeur pilote (2026-09-06).
// ROADMAP_PRODUITS.md §6.2 famille « hauteur d'eau » et §6.6 point 5.
// Question de Rodolf le 05/09 : « l'océan peut être un plan qui s'anime, mais
// comment gérer le splash, les fonds marins ? » — le splash est dans
// NkSplashLaw.h ; les fonds marins sont ICI, et la réponse tient en une ligne :
//
//        profondeur = hauteur_de_la_surface - hauteur_du_TERRAIN
//
// Le fond n'est pas de l'eau. C'est du terrain, et cette seule soustraction
// donne les trois choses qu'on voit : la COULEUR (Beer-Lambert, le rouge part le
// premier), l'ÉCUME près du rivage, et le CAMBREMENT des vagues qui ralentissent
// en arrivant sur la plage.
//
// ⚠️ CE QUE CE FICHIER EST, ET CE QU'IL N'EST PAS. C'est le MODÈLE : des
// fonctions de (x, z, t) vers une hauteur, une normale, une profondeur, une
// couleur. Il n'y a ici ni maillage, ni tampon, ni nuanceur — donc rien à
// « brancher » et rien à croire sur parole : chaque formule a un témoin qui la
// compare à sa loi physique. Le maillage et le rendu sont NOMMÉS comme non faits
// à la fin de cet en-tête.
//
// ── 0. LE DÉFERLEMENT : CE QUI EST PROMIS, ET CE QUI EST IMPOSSIBLE ICI ─────
// 🔴 UN CHAMP DE HAUTEUR NE PEUT PAS REPRÉSENTER UNE VAGUE QUI SE RETOURNE.
// Une surface qui se replie est MULTIVALUÉE — au-dessus d'un même (x, z) il y a
// deux nappes d'eau, la lèvre et le creux sous elle — et un `y = f(x, z)` n'en
// rend qu'une, par définition. Aucun réglage ne contourne ça : ce n'est pas une
// limite d'implémentation, c'est le type de l'objet. Ce fichier ne le promet
// donc pas, et rien ici ne doit laisser croire qu'il le fait.
//
// CE QUI EST LIVRÉ À LA PLACE : le DÉPLACEMENT HORIZONTAL de Gerstner (les
// « choppy waves » de Tessendorf). Il élargit les creux, aiguise les crêtes, et
// pousse jusqu'à l'AUTO-INTERSECTION du maillage — c'est-à-dire l'APPARENCE du
// déferlement, obtenue sans jamais quitter le champ de hauteur.
//
// ET ÇA SE MESURE, sans regarder : le JACOBIEN HORIZONTAL du déplacement,
//        J = det( d(P.x, P.z) / d(x, z) )
// vaut 1 quand rien ne bouge, décroît quand la crête se resserre, et PASSE SOUS
// ZÉRO exactement quand l'application se replie sur elle-même — donc quand la
// vague a brisé. C'est un NOMBRE et non un jugement à l'œil, et c'est lui qui
// pilote l'écume de déferlement (voir `NkWaterFoam`).
//
// ── 1. LA HOULE : somme de vagues de Gerstner ───────────────────────────────
// Une sinusoïde ordinaire fait des vagues symétriques : crêtes et creux ont la
// même forme, et l'océan ne ressemble pas à ça. Gerstner déplace le point AUSSI
// horizontalement, vers la crête : les crêtes se resserrent et pointent, les
// creux s'aplatissent et s'élargissent. C'est une solution exacte des équations
// d'Euler en profondeur infinie (Gerstner 1802 ; en informatique graphique :
// Fournier & Reeves, « A simple model of ocean waves », SIGGRAPH 1986, et la
// forme utilisée ici, Mark Finch, « Effective Water Simulation from Physical
// Models », GPU Gems chapitre 1, 2004).
//        P.x = x + SOMME( Q_i A_i D_i.x cos(w_i (D_i . (x,z)) + phi_i t) )
//        P.z = z + SOMME( Q_i A_i D_i.z cos(...) )
//        P.y =     SOMME( A_i sin(...) )
//   w_i = 2 pi / lambda_i (nombre d'onde), phi_i = w_i c_i (pulsation),
//   Q_i = cambrure dans [0, 1], divisée par (w_i A_i N) pour qu'à Q = 1 la crête
//   soit au bord du repli sur elle-même (une boucle est laide et non physique).
//
// NORMALE ANALYTIQUE, pas par différences finies. DEUX formules, et la mesure a
// tranché entre elles :
//   (a) celle de GPU Gems 1 (éq. 12), `NkWaterNormalFast` :
//        N.x = - SOMME( D_i.x  WA_i  C_i )
//        N.z = - SOMME( D_i.z  WA_i  C_i )
//        N.y = 1 - SOMME( Q_i  WA_i  S_i )      avec WA_i = w_i A_i
//       Elle est EXACTE pour UN train, et seulement du premier ordre pour une
//       SOMME : elle laisse tomber les termes croisés entre trains.
//   (b) le JACOBIEN COMPLET, ce que rend `NkWaterEval` par défaut : les deux
//       tangentes dP/dx et dP/dz sont dérivées analytiquement, terme à terme,
//       puis croisées. Toujours analytique — aucune différence finie, aucun
//       échantillonnage voisin — mais exacte pour une somme quelconque.
// ⚠️ CE QUI A DÉCIDÉ, ET C'EST UNE MESURE, PAS UN GOÛT (06/09, 8 trains, cambrure
// 0,7, 200 points) : (a) s'écarte des différences finies du VRAI champ de
// 4,275 degrés ; (b) reste sous 0,01 degré. Le témoin (o) compare (b), le témoin
// (o2) mesure l'écart de (a) et le DIT — il n'est pas là pour la condamner : à un
// seul train elle est juste, et c'est un produit vectoriel de moins par sommet.
//
// ── 2. LA DISPERSION : ce qui relie la vitesse à la longueur d'onde ─────────
// Théorie linéaire d'Airy, relation de dispersion pour une profondeur h :
//        omega^2 = g k tanh(k h),  k = 2 pi / lambda,  c = omega / k
// Ses deux limites sont les deux régimes qu'on voit :
//   * EAU PROFONDE (k h >> 1, tanh -> 1) :  c = sqrt(g / k) = sqrt(g lambda / 2 pi)
//     — les grandes vagues vont plus vite que les petites, c'est ce qui fait le
//     tri de la houle qui arrive d'une tempête lointaine.
//   * EAU PEU PROFONDE (k h << 1, tanh(x) -> x) : c = sqrt(g h)
//     — la vitesse ne dépend PLUS de la longueur d'onde, seulement du fond. Une
//     vague qui approche du rivage RALENTIT ; sa période étant conservée, sa
//     longueur d'onde raccourcit et son amplitude monte : elle se CAMBRE. C'est
//     exactement la question de Rodolf sur les fonds marins.
// Les deux limites sont vérifiées par les témoins (j) et (r), et la vitesse de
// phase y est MESURÉE en suivant une crête, pas recalculée depuis la formule.
//
// ── 3. LA COULEUR : Beer-Lambert, et le rouge s'en va le premier ────────────
//        T(d) = exp(-sigma d),  sigma en m^-1, d en mètres
// Coefficients de l'eau PURE, avec leur source et sa condition :
//   * 0,34 m^-1 à 650 nm (rouge) et 0,0044 m^-1 à 418 nm (le MINIMUM du spectre,
//     dans le bleu) sont les deux valeurs que j'ai pu sourcer le 06/09 chez
//     Pope & Fry, « Absorption spectrum (380-700 nm) of pure water. II.
//     Integrating cavity measurements », Applied Optics 36(33), 1997.
//   * ⚠️ La valeur du VERT (~550 nm) et celle du bleu à 450 nm, je ne les ai PAS
//     sourcées : les 0,06 et 0,01 m^-1 ci-dessous sont des ordres de grandeur
//     interpolés à la main entre les deux points ci-dessus. Je le dis au lieu de
//     laisser croire que les trois chiffres ont le même statut.
//   * Et c'est de l'eau PURE : une mer réelle porte du plancton et des
//     sédiments, qui absorbent surtout dans le bleu et font le vert des côtes.
//     Le paramètre existe (`absorption`), la valeur par défaut est l'eau pure.
// Le rapport 0,34 / 0,0044 = 77 est la raison pour laquelle tout devient bleu
// en profondeur : à 3 m il reste 36 % du rouge et 98,7 % du bleu.
//
// ── CE QUI N'EST PAS FAIT, ET QUI EST NOMMÉ ─────────────────────────────────
//   * LE MAILLAGE. Décision prise, non implémentée : GRILLE PROJETÉE (Claes
//     Johanson, « Real-time water rendering — introducing the projected grid
//     concept », mémoire, Lund 2004) plutôt qu'anneaux concentriques. Raison :
//     le nombre de sommets est CONSTANT et leur densité est uniforme À L'ÉCRAN,
//     donc le coût ne dépend ni de la distance de vue ni de la taille de
//     l'océan ; les anneaux concentriques, eux, dépensent des sommets au loin et
//     obligent à gérer les raccords de niveau de détail. Ce qu'elle coûte, dit :
//     la projection doit être bornée quand la caméra regarde l'horizon, et elle
//     ondule si on ne fige pas le plan de base.
//   * LE RENDU. Aucun nuanceur, aucun matériau branché. `NkWaterShade` rend la
//     couleur que le nuanceur devrait produire ; personne ne l'appelle encore.
//   * L'ÉCUME reste une VALEUR (0 à 1), pas une texture ni des particules — mais
//     elle est désormais un CHAMP : sa troisième source est le jacobien, donc
//     elle vaut zéro là où la surface ne se resserre pas. Une valeur par point
//     n'est pas une texture ; c'est ce qu'un nuanceur consomme pour en faire une.
//   * Le cambrement modifie la vitesse et l'amplitude. La vague NE SE RETOURNE
//     PAS ici, et ne le pourra jamais dans un champ de hauteur (§0) ; ce qui est
//     mesuré est le REPLI de l'application horizontale, par `jacobianXZ`.
// =============================================================================
#include "NKMath/NkFunctions.h"
#include "NKMath/NkVec.h"

namespace nkentseu {
	namespace math {

		// ⚠️ PAS `NK_G` : ce nom est DEJA pris dans NKMath par la constante
		// gravitationnelle universelle. Deux `NK_G` de types differents, et le
		// compilateur l a dit tout de suite -- il aurait pu ne pas le dire.
		constexpr float32 NK_GRAVITE_NORMALE = 9.80665f; // m/s^2 (valeur normale, CGPM 1901)

		// Un train de vagues.
		struct NkGerstnerWave {
				float32 amplitude = 0.3f;	// m (demi-hauteur crête-creux)
				float32 wavelength = 10.f;	// m
				NkVec2f direction = {1.f, 0.f}; // unitaire, dans le plan XZ
				float32 steepness = 0.5f;	// Q dans [0, 1] : 0 = sinusoïde, 1 = crête au bord du repli
				float32 phase = 0.f;		// rad, décalage à t = 0
		};

		struct NkWaterParams {
				NkGerstnerWave waves[8];
				uint32 waveCount = 0;
				// Profondeur d'eau de RÉFÉRENCE pour la dispersion (m). <= 0 = eau
				// profonde (tanh -> 1). C'est cette valeur que le rivage fait varier.
				float32 depth = 0.f;
				float32 gravity = NK_GRAVITE_NORMALE;
		};

		// Nombre d'onde k = 2 pi / lambda.
		NK_FORCE_INLINE float32 NkWaveNumber(float32 wavelength) noexcept {
			return wavelength > 1e-6f ? 6.2831853f / wavelength : 0.f;
		}

		// VITESSE DE PHASE d'Airy : c = sqrt( (g / k) tanh(k h) ).
		// h <= 0 -> eau profonde : c = sqrt(g / k) = sqrt(g lambda / 2 pi).
		NK_FORCE_INLINE float32 NkPhaseSpeed(float32 wavelength, float32 depth, float32 g = NK_GRAVITE_NORMALE) noexcept {
			const float32 k = NkWaveNumber(wavelength);
			if (k <= 0.f)
				return 0.f;
			if (depth <= 0.f)
				return NkSqrt(g / k);
			return NkSqrt((g / k) * NkTanh(k * depth));
		}

		// Ce qu'un point de la surface rend.
		struct NkWaterPoint {
				NkVec3f position = {0.f, 0.f, 0.f}; // le point DÉPLACÉ (x et z bougent : c'est Gerstner)
				NkVec3f normal = {0.f, 1.f, 0.f};	 // analytique EXACTE (jacobien complet), unitaire
				NkVec3f normalFast = {0.f, 1.f, 0.f}; // la forme de GPU Gems, pour comparaison (temoin (o2))
				// LE JACOBIEN HORIZONTAL, det( d(P.x, P.z) / d(x, z) ).
				// 1 = rien ne bouge · < 1 = la crête se resserre · <= 0 = l'application
				// se replie, la vague a BRISÉ.
				// ⚠️ Il n'est pas recalculé : les deux tangentes sont déjà dérivées
				// analytiquement pour la normale, et ce déterminant est leur mineur
				// horizontal. Le recalculer ailleurs créerait une seconde version qui
				// divergerait un jour de celle-ci — sans que rien ne le dise.
				// ── LA MATRICE, et pas seulement son déterminant ────────────────────
				// Inverser le déplacement demande de résoudre J · delta = r : le
				// déterminant seul n'y suffit pas. Ces quatre entrées sont les mêmes
				// tangentes, rangées :
				//     | jxx  jxz |     | dP.x/dx   dP.x/dz |
				//     | jzx  jzz |  =  | dP.z/dx   dP.z/dz |
				// Au repos c'est l'identité, donc jxx = jzz = 1 et jxz = jzx = 0.
				float32 jxx = 1.f, jxz = 0.f;
				float32 jzx = 0.f, jzz = 1.f;
				float32 jacobianXZ = 1.f;
		};

		// La surface en (x, z) à l'instant t. `depthOverride` (>= 0) remplace
		// params.depth : c'est par lui que le rivage fait ralentir les vagues.
		inline NkWaterPoint NkWaterEval(const NkWaterParams &p, float32 x, float32 z, float32 t,
										float32 depthOverride = -1.f) noexcept {
			NkWaterPoint o;
			o.position = {x, 0.f, z};
			const float32 h = depthOverride >= 0.f ? depthOverride : p.depth;
			// Les deux TANGENTES, dérivées terme à terme (jamais échantillonnées).
			NkVec3f dPdx = {1.f, 0.f, 0.f}, dPdz = {0.f, 0.f, 1.f};
			// La forme rapide de GPU Gems, calculée en même temps : elle ne coûte rien
			// de plus et le témoin (o2) la lit.
			float32 fx = 0.f, fz = 0.f, fy = 1.f;
			for (uint32 i = 0; i < p.waveCount && i < 8u; ++i) {
				const NkGerstnerWave &w = p.waves[i];
				const float32 k = NkWaveNumber(w.wavelength);
				if (k <= 0.f || w.amplitude == 0.f)
					continue;
				const float32 c = NkPhaseSpeed(w.wavelength, h, p.gravity);
				const float32 omega = k * c;
				// Q normalisé : à steepness = 1, la crête est juste au bord du repli.
				const float32 denom = k * w.amplitude * (float32)(p.waveCount ? p.waveCount : 1u);
				const float32 Q = denom > 1e-9f ? w.steepness / denom : 0.f;
				const float32 dx = w.direction.x, dz = w.direction.y;
				const float32 theta = k * (dx * x + dz * z) - omega * t + w.phase;
				const float32 S = NkSin(theta), C = NkCos(theta);
				const float32 A = w.amplitude, QAk = Q * A * k, Ak = A * k;
				o.position.x += Q * A * dx * C;
				o.position.z += Q * A * dz * C;
				o.position.y += A * S;
				// d/dx : theta derive de k dx ; d/dz : de k dz
				dPdx.x -= QAk * dx * dx * S;
				dPdx.y += Ak * dx * C;
				dPdx.z -= QAk * dz * dx * S;
				dPdz.x -= QAk * dx * dz * S;
				dPdz.y += Ak * dz * C;
				dPdz.z -= QAk * dz * dz * S;
				fx -= dx * Ak * C;
				fz -= dz * Ak * C;
				fy -= Q * Ak * S;
			}
			// LE JACOBIEN HORIZONTAL, pris sur les tangentes qu'on vient de dériver :
			//     | dP.x/dx   dP.x/dz |     | dPdx.x   dPdz.x |
			//     | dP.z/dx   dP.z/dz |  =  | dPdx.z   dPdz.z |
			// Au repos les tangentes valent (1,0,0) et (0,0,1) : J = 1 EXACTEMENT, et
			// pas « à peu près » — c'est ce que le contrôle négatif (y1) vérifie.
			o.jxx = dPdx.x; // dP.x/dx
			o.jxz = dPdz.x; // dP.x/dz
			o.jzx = dPdx.z; // dP.z/dx
			o.jzz = dPdz.z; // dP.z/dz
			// ⚠️ LE DÉTERMINANT EST TIRÉ DES QUATRE CHAMPS, et non recalculé depuis les
			// tangentes : deux expressions équivalentes finiraient un jour par ne plus
			// l'être, et rien ne le dirait. Une seule source.
			o.jacobianXZ = o.jxx * o.jzz - o.jxz * o.jzx;

			NkVec3f n = dPdz.Cross(dPdx); // (0,0,1) x (1,0,0) = (0,1,0) au repos : +Y
			const float32 l = NkSqrt(n.x * n.x + n.y * n.y + n.z * n.z);
			o.normal = l > 1e-9f ? n * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
			o.normalFast = NkVec3f{fx, fy, fz};
			const float32 lf = NkSqrt(fx * fx + fy * fy + fz * fz);
			if (lf > 1e-9f)
				o.normalFast = o.normalFast * (1.f / lf);
			return o;
		}

		// ── LA PRÉIMAGE : quel point AU REPOS se déplace jusqu'à (tx, tz) ? ────────
		//
		// POURQUOI ELLE, ET PAS UN REMBOURRAGE. La grille couvre R au repos et affiche
		// D(R) ; pour garantir D(R) ⊇ V il faut construire R ⊇ D⁻¹(V). Élargir R
		// « partout » fait payer sur toute la surface une correction due sur une
		// bande, et c'est mesuré : +20 % d'étendue ne rendait que 0,741 de couverture
		// tout en faisant passer le gâchis de 0,20 à 0,52 — au-delà de son seuil.
		// La préimage, elle, ne rajoute aucun sommet : elle les REFORME.
		//
		// Gerstner ne s'inverse pas analytiquement, mais son jacobien est exact et
		// déjà dérivé, donc Newton converge en quelques pas :
		//        W(n+1) = W(n) − J⁻¹ ( D(W(n)) − T )
		// Le point de départ est la cible elle-même : le déplacement est une petite
		// perturbation de l'identité, pas une transformation quelconque.
		//
		// 🔴 SON DOMAINE, ÉCRIT AVANT LE CODE — comme la limite du champ de hauteur.
		// `J > 0` EST la condition d'inversibilité locale, et J vaut 1 − raideur pour
		// un train (mesuré par (y2)). Au-delà de raideur 1, la préimage n'est plus
		// UNIQUE : ce n'est pas qu'on ne sait pas la calculer, c'est qu'il y en a
		// plusieurs. La fonction REFUSE alors, au lieu d'en rendre une au hasard.
		//
		// ⚠️ ET ELLE REFUSE AUSSI QUAND ELLE N'A PAS CONVERGÉ. Un solveur qui rend
		// silencieusement un résultat non convergé est pire que pas de solveur : il
		// déplace l'erreur au lieu de la signaler. `iterations` est rendu dans tous
		// les cas, pour que l'appelant puisse mesurer la convergence au lieu de la
		// croire.
		// 🔴 LA TOLÉRANCE EST RELATIVE À LA MAGNITUDE, ET C'EST UNE MESURE QUI L'IMPOSE.
		// Première version : un seuil ABSOLU de 1e-4 m. Il ne pouvait pas être atteint.
		// Mesuré sur les trois poses, le résidu plafonnait à 1,22e-04 et 2,44e-04 m,
		// avec les 24 itérations systématiquement consommées — or l'étendue s'étale
		// jusqu'à ~2·10³ m, et 1 ULP de float32 y vaut EXACTEMENT 2,44e-04 m. Les
		// résidus étaient donc à 1 et 2 ULP : Newton avait convergé aussi loin que
		// l'arithmétique le permet, et le critère exigeait mieux que ce que le type
		// peut représenter. C'est la faute déjà payée ailleurs dans ce dépôt — un
		// critère écrit d'avance sur un instrument trop grossier est faux d'avance.
		// On demande donc quelques ULP RELATIFS, ce qui est l'énoncé sensé, au lieu de
		// desserrer le seuil absolu au jugé.
		NK_FORCE_INLINE bool NkWaterInverseXZ(const NkWaterParams &p, float32 tx, float32 tz,
											  float32 t, float32 &outX, float32 &outZ,
											  uint32 &iterations, float32 toleranceRelative = 1e-6f,
											  uint32 maxIter = 24u) noexcept {
			float32 x = tx, z = tz;
			iterations = 0u;
			outX = x;
			outZ = z;
			// L'échelle de la cible fixe le grain atteignable. Le plancher absolu tient
			// le cas où la cible est proche de l'origine, où le relatif s'effondrerait.
			const float32 magnitude = NkFabs(tx) + NkFabs(tz);
			const float32 tolerance = NkMax(1e-5f, toleranceRelative * magnitude);
			for (uint32 n = 0; n < maxIter; ++n) {
				const NkWaterPoint w = NkWaterEval(p, x, z, t);
				const float32 rx = w.position.x - tx, rz = w.position.z - tz;
				iterations = n + 1u;
				if (rx * rx + rz * rz <= tolerance * tolerance) {
					outX = x;
					outZ = z;
					return true;
				}
				if (NkFabs(w.jacobianXZ) < 1e-6f) {
					outX = x; // plus inversible ici : la surface se replie
					outZ = z;
					return false;
				}
				const float32 inv = 1.f / w.jacobianXZ;
				const float32 dx = (w.jzz * rx - w.jxz * rz) * inv;
				const float32 dz = (-w.jzx * rx + w.jxx * rz) * inv;
				x -= dx;
				z -= dz;
			}
			outX = x;
			outZ = z;
			return false; // pas convergé, et ça se DIT
		}

		// La hauteur seule (les témoins de dispersion suivent une crête là-dessus).
		NK_FORCE_INLINE float32 NkWaterHeight(const NkWaterParams &p, float32 x, float32 z, float32 t,
											  float32 depthOverride = -1.f) noexcept {
			return NkWaterEval(p, x, z, t, depthOverride).position.y;
		}

		// =====================================================================
		// LE FOND. `profondeur = hauteur de la surface - hauteur du terrain`.
		// Négative = le terrain SORT de l'eau (une plage, un rocher). C'est le
		// seul endroit du fichier où le terrain entre : il n'est pas modélisé
		// ici, il est FOURNI (une carte de hauteur, la forme Heightfield de
		// NKCollision, ou une fonction).
		// =====================================================================
		NK_FORCE_INLINE float32 NkWaterDepth(float32 surfaceY, float32 terrainY) noexcept {
			return surfaceY - terrainY;
		}

		struct NkWaterOptics {
				// Coefficients d'absorption (m^-1) par canal. Défaut : eau pure, avec
				// l'avertissement de l'en-tête sur le statut de chaque chiffre.
				NkVec3f absorption = {0.34f, 0.06f, 0.01f};
				NkVec3f deepColor = {0.02f, 0.08f, 0.14f}; // ce qu'on voit quand tout est absorbé
				// Écume : au-dessus du fond quand la profondeur passe sous ce seuil (m)...
				float32 shoreDepth = 0.6f;
				// ...et sur les crêtes au-dessus de cette hauteur (m).
				float32 crestHeight = 0.35f;
				// ── L'ÉCUME DE DÉFERLEMENT ──────────────────────────────────────
				// Seuil de JACOBIEN sous lequel l'eau est considérée comme en train de
				// briser. J = 1 au repos, 0 au repli sur soi.
				// ⚠️ Ce seuil n'est pas de la même nature que les deux ci-dessus. Ceux-là
				// portent sur une PROFONDEUR et une HAUTEUR — des accidents de position,
				// vrais même sur une mer d'huile. Celui-ci porte sur la DÉFORMATION : il
				// ne se déclenche que là où la surface se resserre réellement, et c'est
				// ce qui fait de l'écume un CHAMP au lieu d'une valeur allumée partout.
				float32 breakJacobian = 0.5f;
		};

		// Beer-Lambert par canal : T = exp(-sigma d). d < 0 (terrain émergé) -> 1.
		NK_FORCE_INLINE NkVec3f NkBeerLambert(const NkVec3f &sigma, float32 depth) noexcept {
			const float32 d = depth > 0.f ? depth : 0.f;
			return NkVec3f{NkExp(-sigma.x * d), NkExp(-sigma.y * d), NkExp(-sigma.z * d)};
		}

		// La couleur vue à travers `depth` mètres d'eau au-dessus de `bottomColor`.
		NK_FORCE_INLINE NkVec3f NkWaterShade(const NkWaterOptics &o, const NkVec3f &bottomColor,
											 float32 depth) noexcept {
			const NkVec3f T = NkBeerLambert(o.absorption, depth);
			return NkVec3f{bottomColor.x * T.x + o.deepColor.x * (1.f - T.x),
						   bottomColor.y * T.y + o.deepColor.y * (1.f - T.y),
						   bottomColor.z * T.z + o.deepColor.z * (1.f - T.z)};
		}

		// L'écume : 0 à 1. TROIS sources, et le maximum des trois (elles se
		// superposent au rivage, là où la vague déferle sur le sable).
		//   * le RIVAGE : la profondeur passe sous `shoreDepth` ;
		//   * les CRÊTES : la hauteur dépasse `crestHeight` ;
		//   * le DÉFERLEMENT : le jacobien passe sous `breakJacobian`.
		//
		// ⚠️ LE JACOBIEN EST UN PARAMÈTRE PAR DÉFAUT, ET CE N'EST PAS UN DÉTAIL DE
		// CONFORT. Les appelants existants passent trois arguments ; avec `1.f` — la
		// valeur au repos — la troisième source ne contribue jamais, et leur résultat
		// est INCHANGÉ AU BIT. Un paramètre ajouté sans défaut neutre aurait changé
		// en silence ce que mesurent des témoins écrits pour autre chose.
		NK_FORCE_INLINE float32 NkWaterFoam(const NkWaterOptics &o, float32 depth, float32 surfaceY,
											float32 jacobian = 1.f) noexcept {
			float32 f = 0.f;
			if (o.shoreDepth > 1e-6f && depth < o.shoreDepth)
				f = NkClamp(1.f - depth / o.shoreDepth, 0.f, 1.f);
			if (o.crestHeight > 1e-6f && surfaceY > o.crestHeight) {
				const float32 c = NkClamp((surfaceY - o.crestHeight) / o.crestHeight, 0.f, 1.f);
				if (c > f)
					f = c;
			}
			if (o.breakJacobian > 1e-6f && jacobian < o.breakJacobian) {
				const float32 b = NkClamp((o.breakJacobian - jacobian) / o.breakJacobian, 0.f, 1.f);
				if (b > f)
					f = b;
			}
			return f;
		}

	} // namespace math
} // namespace nkentseu
