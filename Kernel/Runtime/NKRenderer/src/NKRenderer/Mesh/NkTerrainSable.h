// =============================================================================
// NkTerrainSable.h — NKRenderer / Mesh
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// UNE ROUE PASSE, LE SOL GARDE SON ORNIÈRE — ET LE TAS S'EFFONDRE AU-DELÀ DE SON
// ANGLE DE REPOS. C'est la seule promesse de ce fichier. Banc : `NkSableCheck`.
//
// ── CE FICHIER EST L'AUTRE MOITIÉ, ET LES DEUX NE SE CONFONDENT PAS ────────
// `NkTerrainSplat` fait le sable COMME COUCHE : une couleur pondérée sur une
// surface QUI NE BOUGE PAS. Son en-tête (l. 49-52) nomme lui-même ce qui manquait
// — « le sable comme MATIÈRE GRANULAIRE [...] est un tout autre chantier ». Le
// voici. Les deux cohabitent : la couche dit de quoi le sol est fait, ce
// fichier-ci dit ce que le sol DEVIENT quand on marche dessus. Et l'un renseigne
// l'autre : c'est la couche ROCHE qui interdit le creusement
// (`NkSableDeformabiliteDepuisPoids`).
//
// ── LE SOL EST UN ÉTAT, PLUS UNE IMAGE ─────────────────────────────────────
// Aujourd'hui la hauteur EST la carte de hauteurs : `NkTerrainDepuisNiveaux`
// écrit `pos.y` une fois, puis le tableau des niveaux meurt. Il n'existe AUCUNE
// fonction `NkTerrainHauteur(x, z)` à interroger — contrairement à l'eau, dont
// `NkWaterEval` est une fonction pure de (x, z, t) rappelable à chaque image.
// C'est cette différence qui décide de tout le montage ci-dessous : on n'ajoute
// pas un terme à une formule, **on ajoute une mémoire à côté de l'image**.
//
//     base[k]     la hauteur que l'image dit, en unités du monde. JAMAIS écrite
//                 après l'initialisation. C'est la promesse de NkTerrainHeightMap,
//                 et ce fichier n'a pas le droit d'y toucher.
//     depot[k]    la déformation. 0 = sol intact. < 0 = creusé. > 0 = bourrelet.
//     combine[k]  base + depot, matérialisé, parce qu'un TIERS le pointe (voir
//                 ci-dessous) et qu'un tiers ne peut pas appeler une fonction.
//
// 🔴 LE NÉGATIF CAPITAL, ET POURQUOI IL EST OBTENU PAR CONSTRUCTION.
// « Sans aucune déformation, la hauteur vaut EXACTEMENT celle de l'image, AU
// BIT. » Une implémentation naïve écrirait `base + depot` partout et s'en
// remettrait à `x + 0.0f == x`. **C'est faux dans un cas** : en IEEE-754,
// `(-0.0f) + (+0.0f) = +0.0f`, un autre motif de bits — et `hauteurMin = -0.f`
// est représentable, `DepuisMinMax` peut le produire. Le critère « au bit »
// tomberait alors sur un montage correct, ce qui est exactement le défaut que la
// maison a déjà payé : « un attendu en dur se périme sans prévenir et se met à
// crier rouge sur un montage correct ».
// Donc : **quand `depot[k]` vaut exactement 0.f, la valeur de base est RECOPIÉE,
// elle ne traverse aucune addition.** Le zéro n'est pas un cas particulier de la
// somme, c'est un chemin distinct. Voir `NkSableHauteurCombinee`.
//
// ── CE QU'ON N'A PAS EU À ÉCRIRE : LA SURFACE REPOUSSE DÉJÀ LE CORPS ───────
// `NKCollision/NkColShapes.h:206` porte `NkShape::Heightfield3D(origin, cellX,
// cellZ, const float32 *heights, rows, cols)` et **conserve le pointeur tel
// quel** ; `NkColConcave.h:46` (`NkConvexVsHeightfield3D`) **relit ce tableau à
// chaque appel** pour en refaire deux triangles par cellule et les passer à
// GJK/EPA. Il suffit donc de lui donner `NkSableHauteurs(champ)` — c'est-à-dire
// `combine.Data()` — pour qu'un corps collisionne la surface DÉFORMÉE, sans une
// seule ligne de couplage dans ce fichier.
//
// 🔴 CONSÉQUENCE DURE DE CE POINTEUR NON-OWNANT : le champ **ne réalloue
// jamais** après `NkSableInit*`. `N`/`M` sont figés. Une fonction qui
// redimensionnerait le champ invaliderait en silence le collideur d'un tiers —
// il n'y a donc aucune fonction de redimensionnement, et il ne faut pas en
// ajouter une sans donner au collideur un moyen d'être prévenu.
//
// ── ET LA MISE À JOUR DU MAILLAGE PASSE PAR LA PORTE QUI EXISTE ────────────
// `NkMeshSystem::UpdateVertices` (NkMeshSystem.cpp:200) a sept appelants vivants.
// Sa garde est `if (!e || !e->dynamic) return false;` : **un maillage non déclaré
// dynamique refuse la mise à jour en rendant `false`**, sans message. Les sept
// appelants gardent tous un tableau `NkVertex3D` côté processeur et le
// repoussent ; `NkSableEcrireSommets` produit exactement ce tableau-là. On ne
// reconstruit PAS le `NkEditMesh` par image : `BuildFromPolygons` refait toute la
// demi-arête.
//
// ── CE QUE CE FICHIER NE FAIT PAS, ET C'EST NOMMÉ, PAS OUBLIÉ ──────────────
//  · Aucune simulation de grains individuels. Un champ de hauteur déformable
//    avec relaxation, c'est tout.
//  · Aucune érosion par l'eau, aucune poussière, aucun effacement des traces
//    avec le temps.
//  · Aucun écoulement continu sous gravité : la relaxation de l'angle de repos
//    est ITÉRATIVE ET LOCALE (quatre voisins), elle ne transporte pas de matière
//    sur de longues distances en une passe.
//  · Aucune compaction, aucune densité variable : le sable retiré réapparaît en
//    bourrelet à volume égal, il ne se tasse pas.
//  · Aucune déformation côté GPU. Le nuanceur de sommets GL de `Shaders/Terrain/`
//    déplace DÉJÀ Y depuis une carte de hauteurs en texture (dénoncé dans
//    `NkTerrainSplat.h`, note 3) : c'est un SECOND chemin, non arbitré. Activer
//    les deux déplacerait deux fois. On ne l'active pas.
//  · Un second passage EXACTEMENT au même endroit ne creuse pas davantage
//    (`NkSableEmpreinteCapsule` prend un minimum, il n'accumule pas) — c'est
//    délibéré, sinon un trajet fait de segments qui se recouvrent creuserait
//    proportionnellement au recouvrement, donc à la discrétisation. Le vrai
//    tassement progressif multi-passages n'est pas écrit.
// =============================================================================
#pragma once

#include "NkTerrainSplat.h"

namespace nkentseu {
	namespace renderer {

		// ── POURQUOI UN STATUT NOMMÉ ────────────────────────────────────────
		// Même raison que `NkTerrainStatut` : un `false` rendrait « le champ
		// n'est pas initialisé » indiscernable de « tes dimensions ne collent
		// pas au maillage », et l'appelant retomberait sur « ça n'a pas marché ».
		enum class NkSableStatut : uint8 {
			NK_OK = 0,
			NK_CHAMP_INVALIDE,			  ///< N < 2, M < 2, ou tableaux non alloués
			NK_DIMENSIONS_INCOMPATIBLES,  ///< le maillage / le tableau n'a pas N*M éléments
			NK_PARAMETRE_HORS_DOMAINE,	  ///< pas <= 0, angle hors [0,90], aire <= 0…
		};

		const char *NkSableStatutNom(NkSableStatut s);

		// ═══════════════════════════════════════════════════════════════════
		//  LES PARAMÈTRES DU LIT GRANULAIRE
		// ═══════════════════════════════════════════════════════════════════
		struct NkTerrainSableParams {
				// ── PORTANCE ET RAIDEUR : DEUX GRANDEURS, PAS UNE ───────────
				// Un seul « coefficient d'enfoncement » ne saurait pas dire la
				// différence entre « le sol tient » et « le sol cède lentement ».
				// Ici la loi est écrite, et exposée plus bas pour qu'un banc
				// calcule l'attendu SANS redemander au champ ce qu'il vérifie :
				//
				//     pression p  = masse * gravite / aireContact      [Pa]
				//     enfoncement = 0                       si p <= portance
				//                 = (p - portance) / raideur  sinon    [m]
				//
				// `portance` est la pression que le lit porte sans céder ;
				// `raideur` est la pression supplémentaire par mètre d'enfoncement.
				// Ordres de grandeur d'un sable sec meuble : portance quelques
				// kPa, raideur quelques dizaines de kPa/m. ⚠️ NON TRANCHÉ : les
				// valeurs exactes sont les yeux de Rodolf, pas une mesure.
				float32 portance = 6000.f;	  // Pa
				float32 raideur = 60000.f;	  // Pa/m
				float32 gravite = 9.81f;	  // m/s²

				// Garde-fou d'enfoncement. Sans lui, une masse absurde ferait un
				// puits traversant le terrain, et l'image ne dirait pas pourquoi.
				float32 enfoncementMax = 0.5f; // m

				// ── L'ANGLE DE REPOS — CE QUI SÉPARE DU SABLE D'UNE PÂTE ────
				// Une pâte à modeler tient n'importe quelle pente. Du sable non :
				// au-delà de son angle de repos, le tas S'EFFONDRE. 30-35° pour du
				// sable sec ; 33 par défaut.
				float32 angleReposDeg = 33.f;

				// Fraction de l'excès transférée à chaque itération et par paire
				// de voisins. 1.0 nivellerait la paire d'un coup, mais chaque
				// cellule a QUATRE voisins : quatre transferts pleins sur la même
				// cellule dans la même itération la feraient osciller. 0.5 est la
				// valeur stable usuelle ; elle est exposée pour être mutée.
				float32 facteurRelaxation = 0.5f;

				// ── LE BOURRELET, EN FRACTION ET JAMAIS EN UNITÉS ───────────
				// Rayon extérieur du bourrelet, en multiple de la demi-largeur de
				// l'empreinte. Une valeur en mètres se périmerait dès qu'on change
				// la taille de la roue. 1 = pas de bourrelet du tout (le volume
				// retiré disparaîtrait : la conservation deviendrait invérifiable,
				// c'est pourquoi le défaut est > 1).
				float32 largeurBourrelet = 1.8f;
		};

		// ── LA LOI D'ENFONCEMENT, EXPOSÉE ───────────────────────────────────
		// Publique et `inline` pour la même raison que
		// `NkTerrainHauteurDepuisNiveau` : un banc doit calculer l'attendu avec
		// EXACTEMENT cette arithmétique, sans la recopier.
		inline float32 NkSableEnfoncement(const NkTerrainSableParams &p, float32 masseKg,
										  float32 aireContact) {
			if (aireContact <= 0.f || p.raideur <= 0.f || masseKg <= 0.f)
				return 0.f;
			const float32 pression = masseKg * p.gravite / aireContact;
			if (pression <= p.portance)
				return 0.f;
			float32 d = (pression - p.portance) / p.raideur;
			if (d > p.enfoncementMax)
				d = p.enfoncementMax;
			return d;
		}

		// ── LA LOI DU TALUS, EXPOSÉE ────────────────────────────────────────
		// Dénivelé maximal toléré entre deux cellules voisines distantes de `pas` :
		//     dhMax = pas * tan(angleRepos)
		// Au-delà, la relaxation transfère de la matière du haut vers le bas.
		//
		// 🔴 LE PIÈGE DE 90° EST RÉEL ET IL EST À L'ENVERS DE L'INTUITION.
		// Le négatif exigé est « angle de repos à 90° -> le tas NE s'effondre
		// PAS ». On serait tenté d'écrire `pas * tanf(rad(90))` et de laisser
		// l'infini faire le travail. Or π/2 n'est pas représentable en float32 :
		// `tanf(1.5707964f)` rend un nombre fini ÉNORME **et de signe non garanti**
		// selon la bibliothèque mathématique. Un dhMax négatif rendrait TOUTE
		// paire excédentaire : le négatif ne se contenterait pas d'échouer, il
		// s'inverserait — le tas s'effondrerait totalement là où on exige qu'il
		// ne bouge pas. Le garde est donc explicite, dans le .cpp, et il est
		// nommé ici pour que personne ne le retire en le croyant décoratif.
		float32 NkSableDeniveleMax(const NkTerrainSableParams &p, float32 pas);

		// ═══════════════════════════════════════════════════════════════════
		//  LE CHAMP
		// ═══════════════════════════════════════════════════════════════════
		struct NkTerrainSable {
				uint32 N = 0, M = 0;		 ///< figés après l'init (voir l'en-tête)
				float32 pasX = 1.f, pasZ = 1.f;
				float32 x0 = 0.f, z0 = 0.f;	 ///< coin (i=0, j=0), convention de NkTerrainHeightMap

				NkVector<float32> base;		 ///< hauteur du monde dite par l'image — JAMAIS réécrite
				NkVector<float32> depot;	 ///< déformation ; 0.f exactement = intact
				NkVector<float32> combine;	 ///< base (+ depot) — c'est CE tableau que le collideur pointe

				// ── LA DÉFORMABILITÉ, ET POURQUOI ELLE EST CONTINUE ────────
				// 1 = sable pur, 0 = roche indéformable. Pas un booléen : la
				// splatmap donne des poids continus, et un seuil binaire ferait
				// apparaître un escalier là où la matière change graduellement.
				// La roche PURE donne exactement 0.f, donc « aucun creusement »
				// reste mesurable au bit.
				NkVector<float32> deformabilite;

				uint32 Taille() const {
					return N * M;
				}
				bool Valide() const {
					return N >= 2u && M >= 2u && base.Size() == (usize)(N * M) &&
						   depot.Size() == (usize)(N * M) && combine.Size() == (usize)(N * M) &&
						   deformabilite.Size() == (usize)(N * M);
				}
		};

		// ── LA COMBINAISON, ET SON CHEMIN ZÉRO DISTINCT ─────────────────────
		// Voir le 🔴 de l'en-tête : le zéro ne traverse aucune addition.
		inline float32 NkSableHauteurCombinee(float32 base, float32 depot) {
			return (depot == 0.f) ? base : (base + depot);
		}

		// Hauteur combinée d'une cellule. `i` = colonne (0..N-1), `j` = ligne.
		inline float32 NkSableHauteur(const NkTerrainSable &c, uint32 i, uint32 j) {
			const uint32 k = j * c.N + i;
			return NkSableHauteurCombinee(c.base[k], c.depot[k]);
		}

		// Le tableau que `NkShape::Heightfield3D` doit recevoir. Nommé plutôt que
		// laissé à `combine.Data()` : c'est un CONTRAT (non-ownant, non réalloué),
		// et un contrat se nomme.
		inline const float32 *NkSableHauteurs(const NkTerrainSable &c) {
			return c.combine.Data();
		}

		// ═══════════════════════════════════════════════════════════════════
		//  INITIALISATION
		// ═══════════════════════════════════════════════════════════════════

		// Depuis les niveaux bruts, avec la MÊME loi et les MÊMES conventions
		// d'origine que `NkTerrainDepuisNiveaux` (x0/z0 centrés ou non).
		NkSableStatut NkSableInitDepuisNiveaux(const float32 *niveaux, uint32 N, uint32 M,
											   const NkTerrainParams &p, NkTerrainSable &out);

		// ── ADOPTER LA BASE DU MAILLAGE, ET POURQUOI CETTE PORTE EXISTE ─────
		// `NkSableInitDepuisNiveaux` RECALCULE `hauteurMin + niveau * pas`. Le
		// compilateur a le droit de fusionner cette expression en un FMA dans une
		// unité de compilation et pas dans l'autre : les deux résultats
		// différeraient alors d'un ULP, et le négatif capital tomberait pour une
		// raison qui n'a rien à voir avec le sable. Cette fonction RECOPIE les
		// hauteurs du maillage au lieu de les recalculer — la base devient
		// bit-identique par construction, quelle que soit la façon dont le
		// maillage a été produit. C'est le chemin que le banc emprunte.
		// Le maillage doit avoir exactement N*M sommets, dans l'ordre des pixels.
		NkSableStatut NkSableAdopterBaseDuMaillage(const NkEditMesh &mesh, NkTerrainSable &champ);

		// ── LA ROCHE N'EST PAS DÉFORMABLE ───────────────────────────────────
		// `deformabilite[k] = 1 - poids[k].w[NK_TERRAIN_ROCHE]`.
		//
		// ⚠️ CORRECTION D'UN ÉNONCÉ DU CANAL, ET ELLE COMPTE. Le lot demande
		// « corps posé sur de la ROCHE (couche 0 dominante) -> aucun creusement ».
		// **La roche n'est pas la couche 0.** `NkTerrainSplat.h` l. 68-73 :
		// `NK_TERRAIN_HERBE = 0`, `NK_TERRAIN_TERRE = 1`, `NK_TERRAIN_ROCHE = 2`,
		// `NK_TERRAIN_NEIGE = 3` — et cet ordre n'est pas inventé, il vient des
		// commentaires de prises du nuanceur GL qui fait foi. Coder « couche 0 »
		// aurait rendu l'HERBE indéformable et la ROCHE molle, avec un banc vert.
		NkSableStatut NkSableDeformabiliteDepuisPoids(const NkTerrainPoids *poids, uint32 nbPoids,
													  NkTerrainSable &champ);

		// Pose une déformabilité uniforme (bancs, sols d'une seule matière).
		NkSableStatut NkSableDeformabiliteUniforme(NkTerrainSable &champ, float32 valeur);

		// Remet `depot` à zéro EXACT et recalcule `combine`. Le sol redevient
		// l'image, au bit.
		NkSableStatut NkSableReinitialiser(NkTerrainSable &champ);

		// ═══════════════════════════════════════════════════════════════════
		//  UN CORPS CREUSE
		// ═══════════════════════════════════════════════════════════════════

		// ── UNE SEULE FORME D'EMPREINTE : LA CAPSULE ────────────────────────
		// Un appui statique et une trace de roue sont la MÊME chose à un degré de
		// liberté près : la capsule (segment A->B, demi-largeur `demiLargeur`).
		// A == B redonne le disque. Deux fonctions auraient divergé au premier
		// correctif ; il n'y en a qu'une, et `NkSableEmpreinte` est un appel.
		//
		// Profil de creusement, en distance NORMALISÉE q = dist(P, segment) /
		// demiLargeur :
		//     q <= 1 : creuse = profondeur * (1 - NkTerrainLisser(q, 0, 1))
		//     q >  1 : rien
		// `NkTerrainLisser` est le smoothstep DÉJÀ écrit dans `NkTerrainSplat.h` —
		// réutilisé et non recopié, précisément pour que l'attendu d'un banc se
		// calcule avec la même fonction que le code.
		//
		// 🔴 LE CREUSEMENT PREND UN MINIMUM, IL N'ACCUMULE PAS.
		//     depot[k] <- min(depot[k], -creuse)
		// La profondeur est comptée DEPUIS LA BASE, jamais depuis la surface
		// courante. Sans ce minimum, un trajet découpé en segments qui se
		// recouvrent creuserait proportionnellement au recouvrement — c'est-à-dire
		// que le résultat dépendrait de la DISCRÉTISATION de l'appelant, et pas de
		// la physique. Le prix est nommé dans l'en-tête : un second passage au
		// même endroit ne creuse pas davantage.
		//
		// ── LA MATIÈRE RETIRÉE REVIENT EN BOURRELET, À VOLUME ÉGAL ─────────
		// Deuxième passe, dans l'anneau 1 < q <= largeurBourrelet, de poids
		// `1 - NkTerrainLisser(q, 1, largeurBourrelet)` (1 au bord de l'empreinte,
		// 0 au bord extérieur), pondéré par la déformabilité. Le volume ajouté
		// vaut le volume retiré : c'est ce qui fait les deux crêtes de part et
		// d'autre d'une ornière, et c'est un critère mesurable
		// (`NkSableVolumeDepot` avant / après).
		// Si l'anneau est entièrement de la roche, la somme des poids est nulle :
		// le volume est alors PERDU, et la fonction le dit en rendant le volume
		// effectivement redéposé dans `outVolumeRedepose`. On ne le cache pas.
		//
		// `outVolumeRetire` / `outVolumeRedepose` : m³, facultatifs.
		NkSableStatut NkSableEmpreinteCapsule(NkTerrainSable &champ, const NkTerrainSableParams &p,
											  float32 ax, float32 az, float32 bx, float32 bz,
											  float32 demiLargeur, float32 profondeur,
											  float32 *outVolumeRetire = nullptr,
											  float32 *outVolumeRedepose = nullptr);

		// Appui statique : la capsule dégénérée.
		inline NkSableStatut NkSableEmpreinte(NkTerrainSable &champ, const NkTerrainSableParams &p,
											  float32 cx, float32 cz, float32 rayon, float32 profondeur,
											  float32 *outVolumeRetire = nullptr,
											  float32 *outVolumeRedepose = nullptr) {
			return NkSableEmpreinteCapsule(champ, p, cx, cz, cx, cz, rayon, profondeur, outVolumeRetire,
										   outVolumeRedepose);
		}

		// ── LA ROUE ─────────────────────────────────────────────────────────
		// L'enfoncement n'est pas donné : il est CALCULÉ par `NkSableEnfoncement`
		// à partir de la masse portée par la roue et de son aire de contact —
		// `aireContact = 2 * demiLargeur * longueurContact`. L'appelant ne peut
		// donc pas inventer une profondeur ; c'est ce qui rend (s2) dérivable.
		// `outEnfoncement`, quand il est fourni, reçoit la profondeur retenue :
		// un banc doit pouvoir confronter le nombre utilisé à celui qu'il a
		// calculé lui-même, et non le supposer.
		NkSableStatut NkSablePassageRoue(NkTerrainSable &champ, const NkTerrainSableParams &p,
										 float32 ax, float32 az, float32 bx, float32 bz,
										 float32 demiLargeur, float32 longueurContact, float32 masseKg,
										 float32 *outEnfoncement = nullptr,
										 float32 *outVolumeRetire = nullptr,
										 float32 *outVolumeRedepose = nullptr);

		// ═══════════════════════════════════════════════════════════════════
		//  LE TALUS NATUREL
		// ═══════════════════════════════════════════════════════════════════

		// ── RELAXATION DE L'ANGLE DE REPOS ──────────────────────────────────
		// Quatre voisins. Pour chaque paire, si |h_haut - h_bas| > dhMax, on
		// transfère `(excès) * 0.5 * facteurRelaxation * min(def_i, def_j)` du
		// haut vers le bas.
		//
		// ⚠️ JACOBI, PAS GAUSS-SEIDEL. Les deltas sont accumulés dans un tampon
		// et appliqués À LA FIN de l'itération. En place, le résultat dépendrait
		// de l'ORDRE de parcours de la grille : un tas parfaitement symétrique
		// s'effondrerait de biais, et personne ne saurait dire si c'est la
		// physique ou la boucle. Le prix est une passe de mémoire de plus.
		// Bénéfice second, et il est mesurable : chaque transfert retire x d'un
		// côté et ajoute x de l'autre, donc **le volume est conservé par
		// construction** — la relaxation ne crée ni ne détruit de sable.
		//
		// S'arrête dès qu'une itération ne trouve plus aucun excès. Rend le
		// NOMBRE D'ITÉRATIONS RÉELLEMENT EXÉCUTÉES : un banc qui lirait 0 saurait
		// que rien n'a bougé, au lieu de croire à une convergence immédiate.
		// `outPenteMaxDeg` reçoit la pente maximale APRÈS relaxation.
		uint32 NkSableRelaxer(NkTerrainSable &champ, const NkTerrainSableParams &p, uint32 iterationsMax,
							  float32 *outPenteMaxDeg = nullptr);

		// ── MESURER LA PENTE, ET MESURER LA CONDITION D'ESSAI ───────────────
		// Pente maximale de la surface COMBINÉE, en degrés, sur les paires de
		// voisins. C'est ce qui juge (s3) — et c'est aussi ce qui doit être
		// mesuré sur le terrain NU avant de poser le tas : « un banc a mesuré une
		// pente de 10 degrés sur un sol parfaitement plat ».
		float32 NkSablePenteMaxDeg(const NkTerrainSable &champ);

		// Volume total du dépôt (m³, signé : négatif si on a plus creusé que
		// déposé). `somme(depot[k]) * pasX * pasZ`.
		float32 NkSableVolumeDepot(const NkTerrainSable &champ);

		// Pose un cône de sable de hauteur `hauteur` et de rayon `rayon` centré en
		// (cx, cz). Sert au négatif de (s3) : un cône plus raide que l'angle de
		// repos DOIT s'effondrer. La pente initiale est `atan(hauteur / rayon)` —
		// dérivable, donc le banc sait ce qu'il a posé avant de mesurer.
		NkSableStatut NkSablePoserCone(NkTerrainSable &champ, float32 cx, float32 cz, float32 rayon,
									   float32 hauteur);

		// ═══════════════════════════════════════════════════════════════════
		//  RENDRE VISIBLE
		// ═══════════════════════════════════════════════════════════════════

		// Réécrit le seul `.y` des sommets d'un tableau de N*M `NkVertex3D` — le
		// tableau que les sept appelants de `NkMeshSystem::UpdateVertices`
		// gardent côté processeur. Ne touche ni X, ni Z, ni UV, ni couleur.
		NkSableStatut NkSableEcrireSommets(const NkTerrainSable &champ, NkVertex3D *sommets,
										   uint32 nbSommets);

		// Réécrit le `.y` des sommets du maillage éditable puis recalcule les
		// normales (sans quoi l'ornière serait creusée mais éclairée comme un
		// sol plat — c'est-à-dire invisible).
		NkSableStatut NkSableAppliquerAuMaillage(const NkTerrainSable &champ, NkEditMesh &mesh);

	} // namespace renderer
} // namespace nkentseu
