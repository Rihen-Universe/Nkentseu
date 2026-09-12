#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkFluidGrid.h — FUMÉE : fluide EULÉRIEN sur grille 3D (2026-09-05).
//
// MÉTHODE : Jos STAM, « Stable Fluids », SIGGRAPH 1999, p. 121-128 — advection
// SEMI-LAGRANGIENNE (inconditionnellement stable) + PROJECTION de pression
// (décomposition de Helmholtz-Hodge, § 2.3 : on retranche le gradient du champ
// de pression qui annule la divergence). Flottabilité et champs de fumée :
// Ronald FEDKIW, Jos STAM, Henrik Wann JENSEN, « Visual Simulation of Smoke »,
// SIGGRAPH 2001, p. 15-22 — équation (8) :
//     f_flottabilité = -alpha * s * ŷ + beta * (T - T_ambiante) * ŷ
// où s est la densité de fumée et T la température.
//
// ⚠️ CE QUE CE FICHIER N'EST PAS : `NkGrid3D` (NKRHI/Tools/Grid3D) est la GRILLE
// DE SOL DU VIEWPORT (lignes, axes, fondu) — un décor, pas un solveur. Le nom
// est le même, l'objet n'a rien à voir. Mesuré le 05/09 avant d'écrire ici.
//
// ⚠️ POURQUOI PAS LE NOYAU DE PROJECTION DU DFSPH : mesuré le 05/09 (§ « ce qui
// existe »). `NkSPHSolver` n'expose AUCUN solveur de Poisson réutilisable — sa
// « projection » DFSPH est une correction de vitesse par facteur alpha_i sur des
// listes de VOISINES, écrite en ligne dans `StepOnce`, sans matrice ni
// Laplacien. Ici la discrétisation est un Laplacien à 6 voisins sur grille
// régulière : c'est un AUTRE opérateur. Le partage sera le stockage GPU (SSBO),
// pas le noyau. Dit plutôt que supposé.
//
// GRILLE : COLOCALISEE (tout au centre de cellule, comme le code de référence de
// Stam), (nx+2) x (ny+2) x (nz+2) — une COUCHE DE BORD sur chaque face. La
// POPULATION que les témoins comptent est l'INTÉRIEUR : nx*ny*nz cellules. La
// résolution est pilotée par `cellSize` (m), jamais par un nombre figé.
//
// 🔴 CE QUE LA GRILLE COLOCALISEE COUTE, MESURE LE 05/09 — a lire avant de croire
// qu'un solveur de pression qui converge rend la divergence nulle.
// La projection resout lap(p) = -div avec le Laplacien a 6 voisins de PAS 1.
// Mais quand on retranche le gradient CENTRE, la divergence CENTREE de la
// vitesse corrigee fait apparaitre p[i+2] - 2p[i] + p[i-2] : un Laplacien de
// PAS 2. Les deux operateurs ne sont pas le meme -- la grille se decouple en
// sous-reseaux pair/impair (le mode « damier » classique). CONSEQUENCE MESUREE :
// on a force le solveur a converger 78 fois plus loin (residu max 3,175e-6 ->
// 4,075e-8 m/s, 35 -> 180 balayages, puis 4 000 balayages), et le rapport
// |div|*h / |u| n'a PAS bouge : 5,4084 % -> 5,4070 % -> 5,4070 %. Le temoin (b)
// est donc ROUGE par la DISCRETISATION, pas par le solveur, et une iteration de
// plus n'y changera rien. Le correctif est nomme et NON FAIT : la grille
// DECALEE (MAC) de Harlow & Welch, « Numerical Calculation of Time-Dependent
// Viscous Incompressible Flow of Fluid with Free Surface », Physics of Fluids 8,
// 1965, p. 2182-2189 -- ou la divergence et le Laplacien sont adjoints exacts.
// C'est la grille qu'utilise Fedkiw, Stam & Jensen 2001 ; le code de
// demonstration de Stam 1999, lui, est colocalise comme ici.
//
// 📌 LA BASCULE EST PRE-ENREGISTREE (2026-09-07) : seuils, controles d'instrument
// a refaire dans le nouveau repere, et liste des temoins qui DOIVENT changer de
// valeur -- tout est ecrit AVANT la premiere ligne de code, dans
// PLAN_GRILLE_MAC.md, a cote de ce fichier. Ce qui ne traverse pas un renvoi et
// doit donc etre lu ICI, ce sont les DEUX choses qui decident :
//   (1) LE CRITERE DECISIF. Sur une grille decalee, l'operateur de divergence et
//       le Laplacien de pression sont ADJOINTS EXACTS : la divergence residuelle
//       cesse d'avoir un plancher de discretisation et devient bornee par la
//       TOLERANCE. Donc la contre-epreuve du plancher DOIT CHANGER DE VERDICT --
//       resserrer la tolerance doit faire BOUGER le rapport, ce qu'elle ne fait
//       pas aujourd'hui. Seuil fixe d'avance : rapport(200,1e-4)/rapport(4000,
//       1e-8) >= 10, alors qu'il vaut 1,00013 (eps=0) et 1,00005 (eps=8) ici.
//   (2) CE QU'ELLE NE CORRIGE PAS : la MASSE. L'advection reste
//       semi-lagrangienne a interpolation trilineaire, et la perte de 45,9 % est
//       SPATIALE (mesure : diviser le pas de temps par 4 ne la change pas).
//       Promettre que la grille decalee la repare serait promettre ce qu'on ne
//       livre pas ; son correctif reste l'advection conservative en flux.
// (Hypothese ECARTEE en chemin : « c'est la couche collee aux parois ». Faux --
// mesure sur l'interieur STRICT, qui ne touche aucune paroi : 0,417 % contre
// 0,389 % sur tout l'interieur. Le defaut est partout, pas au bord.)
//
// ORDRE D'UN PAS (Stam 1999, § 2.2, adapté fumée par Fedkiw 2001) :
//   1. sources (densité, température, carburant)   — Emit*()
//   2. forces  : flottabilité (Fedkiw eq. 8) + gravité de la fumée froide
//   3. advection de la VITESSE (semi-lagrangienne)
//   4. PROJECTION (Poisson, Gauss-Seidel, résidu et itérations MESURÉS)
//   5. advection des SCALAIRES (densité, température, carburant)
//   6. dissipation
//
// ZÉRO STL : NkVector (NKContainers), fonctions de NKMath. Aucun std::.
// =============================================================================
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NKMath.h"
#include "NKMath/NkIForceField.h" // le CONTRAT du vent, partage avec le tissu et le SPH (newtons)

namespace nkentseu {
	namespace renderer {

		// =====================================================================
		// Paramètres — la résolution vient de cellSize, pas d'un compte de cellules
		// =====================================================================
		struct NkFluidGridParams {
				// Domaine (m) et taille de cellule (m). nx = ceil((max.x-min.x)/cellSize), etc.
				math::NkVec3f boundsMin = {-0.5f, 0.f, -0.5f};
				math::NkVec3f boundsMax = {0.5f, 2.f, 0.5f};
				float32 cellSize = 0.05f;

				// FLOTTABILITÉ — Fedkiw, Stam & Jensen 2001, eq. (8).
				// buoyancyBeta <= 0 => BOUSSINESQ : beta = gravity / ambientTemperature,
				// c'est-à-dire l'accélération d'Archimède a = g (T - T_amb) / T_amb pour un
				// gaz parfait à pression constante. Le témoin (c) compare à CETTE loi.
				float32 ambientTemperature = 300.f; // K
				float32 gravity = 9.81f;			// m/s^2
				float32 buoyancyBeta = -1.f;		// <= 0 : Boussinesq (g / T_amb), en m/(s^2 K)
				float32 buoyancyAlpha = 0.f;		// poids de la densité (fumée qui pèse), m/s^2 par unité de densité

				// DISSIPATION (1/s) : d <- d * exp(-diss * dt). 0 = aucune (témoin de masse).
				float32 densityDissipation = 0.f;
				float32 temperatureDissipation = 0.f;

				// PROJECTION : Gauss-Seidel SUR-RELAXÉ (SOR). On sort dès que le résidu max
				// passe sous `pressureTolerance * résidu de départ` ; le nombre d'itérations
				// réellement faites et le résidu final sont dits dans les stats.
				// MESURÉ le 05/09 : sans sur-relaxation (omega = 1), 80 balayages laissaient
				// 0,90 % de divergence résiduelle sur 40^3 — le témoin (b) rougissait à cause
				// du SOLVEUR, pas de la formulation.
				uint32 pressureIterations = 400;
				float32 pressureTolerance = 1.0e-3f; // RELATIF au résidu de départ
				// <= 1 ou >= 2 : la valeur optimale théorique 2/(1 + sin(pi/N)) est utilisée
				// (Press & al., « Numerical Recipes » 3e éd., eq. 20.5.19), N = plus grande dimension.
				float32 pressureOmega = -1.f;
				// Le résidu n'est mesuré que tous les N balayages : le mesurer à chaque
				// balayage DOUBLE le coût du solveur (mesuré).
				uint32 residualCheckEvery = 8;
				// DEMARRAGE A CHAUD : on repart du champ de pression du pas precedent au lieu
				// de zero (comme le kappa du DFSPH). MESURE le 05/09 : 493 -> 63 balayages
				// moyens pour la meme tolerance. Le residu de reference reste celui de p = 0,
				// donc le critere ne devient pas plus facile parce qu'on part de plus pres.
				bool pressureWarmStart = true;
				// BACKTRACE : ordre 2 (point milieu, RK2) au lieu de l'ordre 1 de Stam.
				// MESURE le 05/09 : l'ordre 1 perdait 45 a 51 % de la masse en 500 pas dans un
				// panache (0,1 % seulement en translation pure) -- l'erreur en dt^2 |grad u|^2
				// du point de depart, pas les parois : c'est le cas T du banc qui l'a tranche.
				bool advectRK2 = true;
				// CORRECTION MacCORMACK des SCALAIRES (Selle, Fedkiw, Kim, Liu & Rossignac,
				// « An Unconditionally Stable MacCormack Method », Journal of Scientific
				// Computing 35, 2008, p. 350-371) : on advecte en avant, puis en arriere, et
				// on retranche la MOITIE de l'erreur d'aller-retour, avec le limiteur aux
				// bornes du pochoir d'interpolation (sans quoi le schema n'est pas borne).
				// C'est le correctif que la mesure DESIGNE : la perte de masse ne bouge pas
				// quand on divise le pas de temps par 4 (-51,6 / -51,4 / -51,2 %), donc elle
				// n'est pas temporelle -- c'est la diffusion de l'interpolation trilineaire.
				// DEFAUT : FAUX, et c'est la MESURE qui a tranche, pas le gout. Sur la meme
				// scene et les memes temoins (05/09) :
				//   sans MacCormack : masse -45,9 % / transport 0,070 cellule / divergence 0,39 %
				//   avec MacCormack : masse +25,8 % / transport 1,331 cellule / divergence 0,56 %
				// Le limiteur ecrete le front, donc le barycentre RETARDE : le seul temoin
				// qui etait VERT le devient ROUGE, contre une erreur de masse qui change de
				// signe sans changer d'ordre de grandeur. On garde le schema disponible et
				// mesure, eteint par defaut, jusqu'a une advection vraiment conservative.
				bool advectMacCormack = false;

				// CONFINEMENT DE VORTICITE — Fedkiw, Stam & Jensen, « Visual Simulation of
				// Smoke », SIGGRAPH 2001, § 4, eq. (9)-(11). C'est ce qui fait d'un jet un
				// PANACHE : l'advection semi-lagrangienne DISSIPE la vorticite (c'est le
				// prix de sa stabilite inconditionnelle), et sans elle la colonne monte
				// tout droit. Le confinement la REINJECTE la ou elle est deja, sans en
				// creer ailleurs :
				//     omega  = rot(u)                          (eq. 9)
				//     N      = grad|omega| / | grad|omega| |   (eq. 10, normalise)
				//     f_conf = epsilon * h * (N x omega)       (eq. 11)
				// Le facteur h rend l'effet independant de la resolution (Fedkiw, § 4 :
				// « to ensure that as h -> 0 the physically correct solution is obtained »).
				// UNITE : f_conf est une ACCELERATION (m/s^2) — Fedkiw ecrit l'equation de
				// quantite de mouvement a densite unite ; epsilon est sans dimension.
				//
				// 0 = eteint (defaut). NEGATIF = force INVERSEE : ce n'est pas un reglage,
				// c'est la MUTATION du banc — elle doit faire CHUTER l'enstrophie et donc
				// rougir le temoin. Sans elle, le critere « l'enstrophie augmente » ne
				// prouverait que « du code tourne », pas que le SIGNE du produit vectoriel
				// est le bon — et c'est l'erreur la plus probable de cette formule.
				float32 vorticityConfinement = 0.f;

				// Borne de sécurité, dite si elle mord.
				float32 maxSpeed = 100.f;

				// ── VENT (2026-09-06) : le champ de force EXTERNE, commun a tout le depot.
				// CONTRAT (math::NkIForceField, NKMath) : Force(position, temps) rend des
				// NEWTONS sur une particule PONCTUELLE ; CHAQUE CONSOMMATEUR DIVISE PAR LA
				// MASSE DE SA PARTICULE. On suit la convention, on ne la reinvente pas.
				// Ici la « particule » est une cellule, et sa masse est DECLAREE, pas
				// derivee : la densite de cette grille est une concentration de fumee, pas
				// une masse volumique en kg/m^3 — en tirer des kilogrammes serait inventer
				// une unite. `fieldParticleMass` joue donc exactement le role de
				// `NkEmitterDesc::particleMass` (kg, explicite), et le temoin le verifie :
				// doubler la masse DOIT diviser l'acceleration par deux.
				const math::NkIForceField *field = nullptr; // EMPRUNTE, jamais possede
				float32 fieldParticleMass = 1.f;			// kg
				bool fieldEnabled = true;					// faux -> le temoin du vent DOIT rougir

				// ── INTERRUPTEURS DE MUTATION (les témoins DOIVENT rougir) ──────────
				bool projectionEnabled = true; // faux -> témoin (b) divergence rouge
				bool advectionEnabled = true;  // faux -> témoin (d) transport rouge
				bool buoyancyEnabled = true;   // faux -> témoin (c) rouge

				// ── COMBUSTION (palier ③) : 0 = éteinte, la grille reste de la fumée pure.
				// Taux de réaction (1/s) : carburant consommé par seconde à concentration 1.
				float32 burnRate = 0.f;
				// Chaleur dégagée par unité de carburant brûlé (K par unité de carburant).
				float32 heatPerFuel = 0.f;
				// Fumée (suie) produite par unité de carburant brûlé.
				float32 sootPerFuel = 0.f;
				// Refroidissement de la flamme vers l'ambiante (1/s) — Fedkiw 2001 § 5 utilise
				// un refroidissement en (T/T_max)^4 ; ici, exponentiel simple, DIT tel quel.
				float32 coolingRate = 0.f;
				float32 fuelDissipation = 0.f;
		};

		// =====================================================================
		// Ce que le dernier pas a MESURÉ — les témoins lisent ici, pas une impression.
		// La population comptée est écrite dans chaque champ.
		// =====================================================================
		struct NkFluidGridStats {
				uint32 cellsTotal = 0;	  // (nx+2)(ny+2)(nz+2), bords compris
				uint32 cellsInterior = 0; // nx*ny*nz — LA population de tous les témoins

				// Masse = somme(densité) * cellSize^3 sur l'INTÉRIEUR (unité : densité x m^3)
				float32 mass = 0.f;
				float32 heat = 0.f; // somme(T - T_amb) * cellSize^3, intérieur
				float32 fuel = 0.f; // somme(carburant) * cellSize^3, intérieur

				// DIVERGENCE, en m/s (div * cellSize), moyenne des |.|.
				// DEUX POPULATIONS, et il faut les distinguer -- c'est mesure, pas suppose :
				//  - « intérieur » = les nx*ny*nz cellules interieures. La couche COLLEE aux
				//    parois y est comprise, or sa divergence apparente vient de la CONVENTION
				//    de bord (le fantome porte -u_normal), pas d'un residu du solveur : quand
				//    on force le solveur a converger 78 fois plus loin (residu 3,2e-6 ->
				//    4,1e-8), ce rapport ne bouge PAS (5,408 % -> 5,407 %). Mesure du 05/09.
				//  - « strict » = les cellules qui ne touchent AUCUNE paroi, (nx-2)(ny-2)(nz-2).
				//    C'est la population sur laquelle la projection agit vraiment, et c'est
				//    celle que le temoin (b) juge. Les deux chiffres sont publies.
				float32 divBeforeMean = 0.f, divBeforeMax = 0.f;
				float32 divAfterMean = 0.f, divAfterMax = 0.f;
				float32 velocityMean = 0.f; // |u| moyen sur l'intérieur (m/s)
				float32 divRatio = 0.f;		// divAfterMean / velocityMean (sans unité)
				uint32 cellsStrict = 0;		// (nx-2)(ny-2)(nz-2)
				float32 divAfterMeanStrict = 0.f, divAfterMaxStrict = 0.f;
				float32 velocityMeanStrict = 0.f;
				float32 divRatioStrict = 0.f;

				uint32 pressureIters = 0;		 // balayages SOR RÉELLEMENT faits
				float32 pressureResidual = 0.f;	 // résidu max final du Poisson (m/s)
				float32 pressureResidual0 = 0.f; // résidu max de départ (p = 0), m/s
				float32 pressureOmega = 0.f;	 // sur-relaxation réellement appliquée
				bool pressureCapHit = false;	 // la borne d'itérations a mordu

				// VORTICITE (2026-09-06), mesuree sur l'INTERIEUR STRICT — la meme
				// population que la divergence stricte, et pour la meme raison : la couche
				// collee aux parois porte une vorticite de CONVENTION (le fantome tangentiel
				// est recopie, le normal inverse), pas une vorticite du fluide.
				//   enstrophy      = somme |omega|^2 * h^3   (unite : m^3/s^2)
				//   vorticityMean  = |omega| moyen           (1/s)
				float32 enstrophy = 0.f;
				float32 vorticityMean = 0.f, vorticityMax = 0.f;
				// Acceleration moyenne REELLEMENT ajoutee par le confinement au dernier pas
				// (m/s^2), et par le vent. Un parametre non honore est pire qu'un parametre
				// absent : ces deux chiffres disent que la force est PARTIE.
				float32 confinementAccelMean = 0.f;
				float32 windAccelMean = 0.f;

				float32 maxSpeed = 0.f;
				float32 maxTemperature = 0.f;
				uint32 speedClamped = 0; // cellules bornées par maxSpeed (0 attendu)
				uint32 nanCount = 0;	 // NaN/Inf trouvés (0 attendu)
				float32 ms = 0.f;
		};

		// =====================================================================
		// La grille
		// =====================================================================
		class NkFluidGrid final {
			public:
				NkFluidGrid() = default;
				~NkFluidGrid() = default;

				bool Init(const NkFluidGridParams &p);
				void Reset(); // remet les champs à zéro, garde l'allocation

				void Step(float32 dt);

				// ── Sources (coordonnées MONDE) ─────────────────────────────────
				// Sphère de rayon r : ajoute densité/température/carburant, additif.
				void EmitSphere(const math::NkVec3f &centerWorld, float32 radius, float32 density,
								float32 temperature, float32 fuel = 0.f);
				// Impose une vitesse uniforme à TOUT l'intérieur (témoin d'advection).
				void SetUniformVelocity(const math::NkVec3f &v);

				// ── Lecture (témoins et rendu) ──────────────────────────────────
				uint32 Nx() const { return mNx; }
				uint32 Ny() const { return mNy; }
				uint32 Nz() const { return mNz; }
				float32 CellSize() const { return mParams.cellSize; }
				const NkFluidGridParams &Params() const { return mParams; }
				NkFluidGridParams &Params() { return mParams; }
				const NkFluidGridStats &Stats() const { return mStats; }

				// Index dans les tableaux (i,j,k dans [0, N+1] : 0 et N+1 = bord).
				uint32 Idx(uint32 i, uint32 j, uint32 k) const {
					return i + (mNx + 2) * (j + (mNy + 2) * k);
				}

				const float32 *Density() const { return mDensity.Data(); }
				float32 *Density() { return mDensity.Data(); }
				const float32 *Temperature() const { return mTemperature.Data(); }
				float32 *Temperature() { return mTemperature.Data(); }
				const float32 *Fuel() const { return mFuel.Data(); }
				const float32 *VelocityX() const { return mU.Data(); }
				const float32 *VelocityY() const { return mV.Data(); }
				const float32 *VelocityZ() const { return mW.Data(); }

				// ── LA VITESSE AU CENTRE D'UNE CELLULE ──────────────────────────
				// ⚠️ C'EST LE CHEMIN QUE LA BASCULE MAC REND DANGEREUX, et c'est
				// pour ça qu'il porte un nom AVANT d'avoir un contenu. Tout ce qui
				// lit la vitesse au centre d'une cellule — flottabilité, vorticité,
				// statistiques, rendu, vent — devra passer par ici.
				//   La grille est DECALEE : u vit sur les FACES, et cette fonction rend
				//   la MOYENNE DES DEUX FACES qui bordent la cellule. MESURE du 12/09,
				//   a code d'essai IDENTIQUE A L'OCTET PRES : le controle (m1) est passe
				//   de ROUGE (1,250e-01 puis 9,375e-02, quand la grille etait encore
				//   colocalisee) a VERT (1,192e-07 puis 4,773e-08). C'est CE PASSAGE qui
				//   est le verdict -- un temoin ne vert n'aurait rien dit du tout.
				// CONVENTION DE FACE, ecrite ici parce que c'est elle qui decide de
				// tout : la face x d'indice i est la face GAUCHE de la cellule
				// (i,j,k), a x = boundsMin.x + (i-1)*h ; le centre de la cellule
				// reste a (i-0.5)*h. La face DROITE de la cellule i est donc la face
				// GAUCHE de la cellule i+1 : UNE case pour UNE face, jamais deux --
				// c'est exactement ce qui rend la divergence COMPACTE et adjointe
				// exacte du Laplacien de pression. L'allocation ne change pas.
				void VelocityAtCenter(uint32 i, uint32 j, uint32 k, float32 &ux, float32 &uy, float32 &uz) const;

				// Échantillonnage trilinéaire d'un champ en coordonnées MONDE (rendu).
				float32 SampleDensityWorld(const math::NkVec3f &p) const;
				float32 SampleTemperatureWorld(const math::NkVec3f &p) const;

				// Barycentres pondérés (témoins de transport et de flottabilité), en MONDE.
				// Rendent false et ne touchent pas `out` si le poids total est nul.
				bool DensityCentroid(math::NkVec3f &out) const;
				bool TemperatureCentroid(math::NkVec3f &out) const; // pondéré par (T - T_amb) positif

				// Vitesse verticale moyenne pondérée par (T - T_amb) positif (témoin (c)).
				float32 HotVerticalVelocity() const;

				// Totaux sur l'INTÉRIEUR (les témoins les relisent hors d'un pas).
				float32 TotalMass() const;
				// Masse portee par la COUCHE INTERIEURE COLLEE AUX PAROIS (les six faces).
				// C'est le compteur qui dit si un temoin de conservation est encore VALIDE :
				// l'advection semi-lagrangienne DETRUIT la masse qui touche une paroi (voir .cpp).
				float32 WallLayerMass() const;
				float32 TotalHeat() const;
				float32 TotalFuel() const;

				// Le beta de flottabilité RÉELLEMENT appliqué (Boussinesq si params <= 0).
				float32 EffectiveBeta() const;

				// ── VORTICITE (2026-09-06) ──────────────────────────────────────
				// Renseignes par le dernier Step, sur l'INTERIEUR STRICT.
				float32 Enstrophy() const { return mStats.enstrophy; }
				float32 VorticityMean() const { return mStats.vorticityMean; }

				// RAYON DE GIRATION HORIZONTAL de la densite dans la tranche horizontale la
				// plus proche de `worldY` (m). C'est LA quantite qui separe un jet d'un
				// panache : la racine de la moyenne des carres des distances au barycentre
				// de la tranche, ponderee par la densite. Un jet mince a un petit rayon,
				// un panache qui tournoie s'etale.
				//  - `sliceMassOut` : la masse de la tranche. Il FAUT la publier : comparer
				//    deux rayons sans comparer les masses comparerait deux populations.
				//  - `rowOut` : l'indice j reellement lu (la tranche n'est pas exactement a
				//    worldY, elle est a la rangee la plus proche).
				// Rend false et ne touche a rien si la tranche est vide.
				bool PlumeRadius(float32 worldY, float32 &radiusOut, float32 &sliceMassOut, uint32 &rowOut) const;

				// L'instant que le champ de force voit (s). Avance de dt a chaque Step.
				float32 FieldTime() const { return mFieldTime; }

			private:
				void Allocate();
				void AddSources(float32 dt);
				void AddBuoyancy(float32 dt);
				void Combust(float32 dt);
				// omega = rot(u) par differences centrees, + |omega|, bords recopies.
				// TOUJOURS appelee (le confinement en a besoin, les temoins aussi) : elle
				// coute une passe contre les dizaines de balayages du Poisson.
				void ComputeVorticity();
				void AddVorticityConfinement(float32 dt); // Fedkiw 2001, eq. (11)
				void AddWind(float32 dt);				  // NkIForceField : newtons / masse declaree
				// La VITESSE au point de GRILLE (gx,gy,gz), interpolee depuis les FACES.
				// gx est en unites de CENTRE (le centre de la cellule i vaut i) ; comme
				// u vit a i-0,5, on passe gx+0,5 a Trilinear, qui indexe les centres.
				// C'est LE decalage de la grille decalee, et il tient en trois additions.
				void VelocityAtGrid(const NkVector<float32> &fu, const NkVector<float32> &fv,
									const NkVector<float32> &fw, float32 gx, float32 gy, float32 gz, float32 &vx,
									float32 &vy, float32 &vz) const;
				// Position de depart (coordonnees de GRILLE) d'ou vient ce qui arrive au
				// point (gx,gy,gz). Prend des FLOTTANTS : sur grille decalee le point de
				// depart d'une face n'est pas un centre de cellule.
				void BacktraceAt(float32 gx, float32 gy, float32 gz, float32 dt0, const NkVector<float32> &fu,
								 const NkVector<float32> &fv, const NkVector<float32> &fw, float32 &x, float32 &y,
								 float32 &z) const;
				// Les bords de VITESSE sur grille decalee. Rien a voir avec SetBoundary :
				// la composante NORMALE n'est plus « inversee dans un fantome », elle est
				// imposee NULLE SUR la face de paroi, exactement la ou la paroi est.
				void SetVelocityWalls();
				// Un aller SEUL, semi-lagrangien. `sens` vaut +1 (remonter le temps) ou -1.
				void AdvectSemiLagrangien(NkVector<float32> &dst, const NkVector<float32> &src, float32 dt, int32 bnd,
										  float32 sens);
				// Bornes du pochoir trilineaire autour d'une position (limiteur MacCormack).
				void TrilinearBornes(const NkVector<float32> &f, float32 x, float32 y, float32 z, float32 &mn,
									 float32 &mx) const;
				void AdvectScalar(NkVector<float32> &dst, const NkVector<float32> &src, float32 dt, int32 bnd);
				void AdvectVelocity(float32 dt);
				void Project(float32 dt);
				void SetBoundary(int32 b, NkVector<float32> &f);
				float32 Trilinear(const NkVector<float32> &f, float32 x, float32 y, float32 z) const;
				// `strict` : ne compte que les cellules qui ne touchent aucune paroi.
				void MeasureDivergence(float32 &meanOut, float32 &maxOut, bool strict) const;
				void MeasureVelocity();

				NkFluidGridParams mParams;
				NkFluidGridStats mStats;
				uint32 mNx = 0, mNy = 0, mNz = 0; // cellules INTÉRIEURES
				uint32 mCount = 0;				  // (nx+2)(ny+2)(nz+2)
				bool mReady = false;

				NkVector<float32> mU, mV, mW;	  // vitesse (m/s), centre de cellule
				NkVector<float32> mU0, mV0, mW0;  // tampons d'advection / travail
				NkVector<float32> mDensity, mDensity0;
				NkVector<float32> mTemperature, mTemperature0;
				NkVector<float32> mFuel, mFuel0;
				NkVector<float32> mPressure, mDivergence;
				NkVector<float32> mScratchA, mScratchB;			   // aller-retour de MacCormack
				NkVector<float32> mOmegaX, mOmegaY, mOmegaZ, mOmegaMag; // rot(u) et son module
				float32 mFieldTime = 0.f;						   // l'instant vu par le champ de force (s)
		};

	} // namespace renderer
} // namespace nkentseu
