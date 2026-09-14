// =============================================================================
// NkTerrainSplat.h — NKRenderer / Mesh
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// LA PENTE CHOISIT LA COUCHE : la roche sur le raide, la terre en bas, la neige
// en haut. Quatre poids par sommet, dont la somme vaut 1.
//
// ── ON ADOPTE UN NUANCEUR QUI EXISTE, ON N'EN ÉCRIT PAS UN ─────────────────
// `Resources/NKRenderer/Shaders/Terrain/GL/terrain.frag.gl.glsl` (42 lignes)
// implémente DÉJÀ le mélange 4 couches : quatre poids lus en RGBA d'une
// splatmap, quatre albédos, quatre normales tangentes, TBN, boucle de lumières.
// Ce fichier-ci produit exactement la donnée que ce nuanceur déclare. Il ne le
// remplace pas et ne le modifie pas.
//
// ⚠️ CE QUI EST MESURÉ ET CE QUI NE L'EST PAS — trois faits à ne pas perdre :
//
//  1. **Seuls GL et VK ont la splatmap.** Recensement de `tSplatmap` sur les
//     onze fichiers de `Resources/NKRenderer/Shaders/Terrain/` : GL x2, VK x2,
//     **DX11 x0, DX12 x0, MSL x0, NkSL x0**. DX11 (l.8-11) déclare
//     `tAlbedo/tNormal/tORM/tEmissive` — la signature du PBR générique. C'est le
//     fragment PBR standard portant un nom de fichier « terrain ».
//     Et les trois portent, DX11:17 / DX12:17 / MSL:12, la ligne
//         // Splatmap 4-layer
//     au-dessus d'une boucle de lumières qui ne mélange rien. Un commentaire qui
//     annonce ce que le code ne fait pas est du faux savoir armé : une recherche
//     de motif conclut « oui, DX11 l'a ».
//
//  2. **La copie qui fait foi est `Resources/`**, pas celle de l'arbre source.
//     `NkShaderLibrary.cpp:668` : `basePath = "Resources/NKRenderer/Shaders/"`.
//     Les deux copies ONT DIVERGÉ (4 fichiers) : `CameraUBO` a un champ
//     `iblStrength` de plus côté `Resources/`, et l'`ObjectUBO` Vulkan y est en
//     `set=1` contre `set=0` dans l'arbre source. Une disposition d'UBO qui
//     diffère d'un champ, c'est un nuanceur qui lit d'autres octets : sans
//     erreur et sans journal. La copie de l'arbre source est PÉRIMÉE.
//     Une troisième, `Resources/Shaders/NKRenderer/Shaders/Terrain/`, tient en
//     cinq fichiers d'UNE ligne de commentaire : un emplacement réservé.
//
//  3. **La paire GL a une collision de prise.** Le vertex déclare
//     `binding=4 tHeightmap` (GL vert l.8), le fragment `binding=4 tSplatmap`
//     (GL frag l.8). En OpenGL l'unité de texture est partagée par tout le
//     programme : deux textures sur l'unité 4, l'une gagne, rien ne le dit.
//     Et ce vertex DÉPLACE déjà Y depuis une heightmap en texture et recalcule
//     `vNormal` par gradient, ignorant `aNormal` (l.14-21) — alors que
//     `NkTerrainHeightMap` déplace au PROCESSEUR. Les deux ensemble déplacent
//     deux fois ; et avec `heightScale = 0`, `vNormal` vaut (0,1,0) partout,
//     donc la pente disparaît de l'éclairage. Deux conceptions du même terrain,
//     rien n'arbitre entre elles. NOMMÉ, pas corrigé : hors lot.
//
// ── CE QUE CE FICHIER NE FAIT PAS ───────────────────────────────────────────
// Aucune texture, aucun échantillonnage, aucune couleur de production. Et
// surtout : **le sable comme MATIÈRE GRANULAIRE** — des grains qui coulent, une
// trace de pneu qui creuse — est un tout autre chantier, bien plus gros. Ici le
// sable est une COUCHE, c'est-à-dire une couleur pondérée sur une surface qui ne
// bouge pas. Que personne ne croie l'autre livré.
//
// 🔄 MISE À JOUR DU 2026-09-14 — CET AUTRE CHANTIER EST ÉCRIT.
// La phrase ci-dessus était vraie à l'heure où elle a été posée. La laisser
// seule ferait de ce paragraphe exactement ce qu'il dénonce trois écrans plus
// haut — un commentaire qui ment sur le code — simplement dans l'autre sens.
// `NkTerrainSable.{h,cpp}` porte le champ de hauteur déformable et la relaxation
// à l'angle de repos ; banc `NkSableCheck`, 35 critères, 0 rouge.
// **Les deux ne se remplacent pas, et CELUI-CI RENSEIGNE L'AUTRE** : c'est le
// poids de `NK_TERRAIN_ROCHE` produit ici qui interdit le creusement là-bas
// (`NkSableDeformabiliteDepuisPoids` : `déformabilité = 1 - w[ROCHE]`).
// ⚠️ Et c'est bien la couche **2**. Un lot de travail a demandé « la roche,
// couche 0 dominante » : coder cet énoncé aurait rendu l'HERBE indéformable et
// la ROCHE molle, avec un banc tout vert. L'énumération ci-dessous fait foi, et
// elle ne vient pas de nous — elle vient des prises du nuanceur GL.
// =============================================================================
#pragma once

#include "NkTerrainHeightMap.h"

namespace nkentseu {
	namespace renderer {

		// ── LES QUATRE COUCHES — NON TRANCHÉ ────────────────────────────────
		// Cet ordre n'est pas inventé : il est DÉJÀ SUR DISQUE, en commentaire
		// des prises du nuanceur qui fait foi (GL frag l.9-12) —
		//   tAlbedo0 // Grass . tAlbedo1 // Dirt . tAlbedo2 // Rock . tAlbedo3 // Snow
		// On l'adopte pour ne pas créer un second vocabulaire qu'il faudrait
		// ensuite réconcilier avec celui-là.
		// ⚠️ NON TRANCHÉ : ces quatre matières, leur ordre, et toute couleur
		// d'aperçu sont les yeux de Rodolf. Adopté ≠ décidé.
		enum NkTerrainCouche : uint8 {
			NK_TERRAIN_HERBE = 0, // Grass
			NK_TERRAIN_TERRE = 1, // Dirt — c'est ici que « le sable » atterrit, COMME COUCHE
			NK_TERRAIN_ROCHE = 2, // Rock
			NK_TERRAIN_NEIGE = 3, // Snow
		};

		// Un jeu de quatre poids. `w[NK_TERRAIN_*]`, somme = 1.
		struct NkTerrainPoids {
				float32 w[4] = {1.f, 0.f, 0.f, 0.f};

				float32 Somme() const {
					return w[0] + w[1] + w[2] + w[3];
				}
				uint8 Dominante() const {
					uint8 d = 0;
					for (uint8 k = 1; k < 4; ++k)
						if (w[k] > w[d])
							d = k;
					return d;
				}
		};

		struct NkTerrainSplatParams {
				// ── LES DEUX SEULS SEUILS ABSOLUS, ET C'EST JUSTIFIÉ ────────
				// Un ANGLE est absolu : il ne dépend d'aucune échelle de
				// terrain, contrairement à une altitude. 25° / 45° peuvent donc
				// être écrits en dur sans se périmer.
				float32 penteRoche0 = 25.f; // en deçà : aucune roche
				float32 penteRoche1 = 45.f; // au-delà : roche pure

				// ── LES ALTITUDES SONT DES FRACTIONS, JAMAIS DES UNITÉS ─────
				// Un seuil en unités de monde se périmerait dès qu'on change
				// `hauteurParNiveau`, et se mettrait à crier rouge sur un
				// montage correct. Ce sont des fractions de l'étendue RÉELLE
				// (hMin..hMax) du terrain mesuré.
				float32 fracTerre0 = 0.05f; // fin du règne de la terre : début de transition
				float32 fracTerre1 = 0.25f; // au-delà : plus de terre
				float32 fracNeige0 = 0.75f; // début de la neige
				float32 fracNeige1 = 0.95f; // au-delà : neige pleine
		};

		// Interpolation lissée (smoothstep) — écrite ici et pas ailleurs pour
		// que l'attendu du banc se calcule avec EXACTEMENT cette fonction.
		inline float32 NkTerrainLisser(float32 x, float32 a, float32 b) {
			if (b - a <= 0.f)
				return x >= b ? 1.f : 0.f;
			float32 t = (x - a) / (b - a);
			if (t < 0.f)
				t = 0.f;
			if (t > 1.f)
				t = 1.f;
			return t * t * (3.f - 2.f * t);
		}

		// ── LA LOI, EXPOSÉE — et POURQUOI ELLE NE SE NORMALISE PAS ──────────
		//     s      = lisser(angle, penteRoche0, penteRoche1)
		//     wRoche = s                        reste  = 1 - s
		//     wNeige = reste * fNeige           reste2 = reste - wNeige
		//     wTerre = reste2 * fTerre          wHerbe = reste2 - wTerre
		// Somme = s + reste*fNeige + reste2 = s + reste = 1, par une chaîne de
		// SOUSTRACTIONS, et chaque poids est >= 0 (tous les facteurs sont dans
		// [0,1]).
		//
		// ⚠️ IL N'Y A PAS DE DIVISION FINALE PAR LA SOMME, ET C'EST DÉLIBÉRÉ.
		// Une normalisation terminale rendrait le contrôle « somme = 1 »
		// INTESTABLE : la propriété serait garantie deux fois — par la
		// construction ET par la correction — donc le contrôle ne pourrait
		// jamais rougir. « Une propriété garantie deux fois est une propriété
		// dont l'échec est masqué. » Elle est garantie UNE fois, par
		// l'arithmétique, et le banc mesure si l'arithmétique tient en float32.
		//
		// `hMin`/`hMax` sont l'étendue MESURÉE du terrain. Si elle est nulle
		// (terrain plat), les deux transitions s'effondrent : règle posée,
		// fNeige = 0 et fTerre = 1, donc wTerre = reste.
		NkTerrainPoids NkTerrainPoidsDepuisPente(float32 angleDeg, float32 hauteur, float32 hMin,
												 float32 hMax, const NkTerrainSplatParams &p);

		// Calcule les poids de CHAQUE sommet du maillage, dans l'ordre des
		// sommets — donc `out[k]` correspond au pixel (k % N, k / N).
		// L'angle vient de la normale du sommet : acos(clamp(n.y, -1, 1)).
		// `hMin`/`hMax` sont mesurés sur le maillage lui-même : un seuil dérivé
		// du terrain qu'il juge, jamais recopié.
		NkTerrainStatut NkTerrainPoidsDuMaillage(const NkEditMesh &mesh, const NkTerrainSplatParams &p,
												 NkVector<NkTerrainPoids> &out);

		// ── LA SPLATMAP, AU FORMAT QUE LE NUANCEUR DÉCLARE ──────────────────
		// RGBA8 : R = herbe, G = terre, B = roche, A = neige — l'ordre de
		// `splat.r/g/b/a` dans `terrain.frag.gl.glsl` l.23-26.
		// ⚠️ La quantification sur 8 bits fait perdre la somme exacte : quatre
		// arrondis indépendants ne somment plus à 255 au bit. C'est une
		// propriété du FORMAT, pas de la loi — et c'est précisément pourquoi le
		// nuanceur renormalise à la lecture (l.20-21) et pourquoi le contrôle
		// « somme = 1 » se mesure sur les poids FLOTTANTS, en amont.
		// `outSomme255Max`, quand il est fourni, reçoit le plus grand écart à
		// 255 constaté : l'erreur de quantification, mesurée au lieu d'être
		// supposée.
		NkTerrainStatut NkTerrainSplatmapDepuisPoids(const NkTerrainPoids *poids, uint32 N, uint32 M,
													 NkImage &out, uint32 *outSomme255Max = nullptr);

	} // namespace renderer
} // namespace nkentseu
