// =============================================================================
// NkTerrainHeightMap.h — NKRenderer / Mesh
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// UNE IMAGE DE HAUTEURS DEVIENT UN TERRAIN DONT CHAQUE SOMMET EST À LA HAUTEUR
// QUE L'IMAGE DIT. C'est la seule promesse de ce fichier, et elle est mesurable
// au pixel : banc `NkTerrainCheck`.
//
// ── POURQUOI DES FONCTIONS LIBRES, ET PAS UNE CLASSE `NkTerrain` ────────────
// Consigne écrite dans `NkEditMesh.h` (l. 11-18, procès-verbal du 2026-07-26) :
// une SECONDE structure demi-arête (`NkHalfEdgeMesh`) avait été écrite en
// parallèle de `NkEditMesh`, jamais implémentée, jamais incluse, et supprimée
// avec cette phrase — « si ces opérations sont voulues un jour, les implémenter
// comme FONCTIONS LIBRES opérant sur renderer::NkEditMesh, et NON via une
// structure demi-arête concurrente ». Une classe `NkTerrain` portant sa propre
// grille de sommets serait exactement la faute déjà payée : un terrain est un
// MAILLAGE COMME UN AUTRE, produit par une fonction. Il hérite donc gratuitement
// de tout ce que `NkEditMesh` sait déjà faire (sélection, extrusion, modifieurs,
// undo, export) au lieu d'en redemander une copie.
//
// ── UN PIXEL = UN SOMMET ────────────────────────────────────────────────────
// C'est le seul découpage où la promesse ci-dessus se VÉRIFIE au pixel. Toute
// autre densité obligerait à interpoler, donc à inventer des hauteurs que
// l'image n'a jamais dites — et le critère central deviendrait invérifiable.
// (Une densité découplée viendra avec les niveaux de détail ; elle devra alors
//  NOMMER son interpolation, pas la subir.)
//
// ── CE QUE CE FICHIER NE FAIT PAS ───────────────────────────────────────────
// Niveaux de détail, pavage GPU, quadtree, mélange de textures par pente,
// érosion, sable, terre déformable. Tous nommés, aucun écrit.
// Le mélange par pente a déjà son point d'entrée sur disque et il est ORPHELIN :
// `Shaders/Terrain/` (23 dossiers de nuanceurs, celui-là n'a ni gabarit de
// matériau dans `NkMaterialSystem` ni aucun appelant dans tout le dépôt) annonce
// « splatmap 4-layer PBR » et son corps de fragment est un talon.
// =============================================================================
#pragma once

#include "NkEditMesh.h"

namespace nkentseu {

	class NkImage; // NKImage/Core/NkImage.h — inclus par le .cpp

	namespace renderer {

		// ── Pourquoi un statut nommé plutôt qu'un `bool` ────────────────────
		// Le refus 1x1 ci-dessous est un RÉSULTAT, pas une panne. Un `false` le
		// rendrait indiscernable d'une image absente ou d'un canal manquant, et
		// l'appelant retomberait sur « ça n'a pas marché ».
		enum class NkTerrainStatut : uint8 {
			NK_OK = 0,
			NK_IMAGE_INVALIDE,	  ///< pixels absents, largeur ou hauteur <= 0
			NK_IMAGE_TROP_PETITE, ///< N < 2 ou M < 2 — voir la note ci-dessous
			NK_CANAL_ABSENT,	  ///< `canal` >= nombre de canaux de l'image
			NK_FORMAT_INCONNU,	  ///< NkImagePixelFormat non traité
		};

		const char *NkTerrainStatutNom(NkTerrainStatut s);

		// ── LE REFUS 1x1 EST UN CHOIX, ET VOICI SA RAISON ───────────────────
		// Une image N x M donne (N-1)(M-1) quads. Pour N = M = 1 cela fait ZÉRO.
		// Fabriquer « un quad quand même » demanderait de poser QUATRE sommets
		// alors que l'image n'en dit qu'UN : trois hauteurs sur quatre ne
		// seraient dites par rien. Ce serait violer la promesse du fichier dans
		// le fichier lui-même. On refuse, et le refus porte un nom.
		// La règle est donc : N >= 2 ET M >= 2. Une bande Nx1 tombe dessous, et
		// pour la même raison : elle n'a aucune deuxième rangée à relier.

		struct NkTerrainParams {
				// Pas du quadrillage, en unités du monde, par pixel.
				float32 pasX = 1.f;
				float32 pasZ = 1.f;

				// ── LOI DE HAUTEUR ──────────────────────────────────────────
				//     hauteur = hauteurMin + niveau * hauteurParNiveau
				//
				// ⚠️ POURQUOI UN PAS PAR NIVEAU, ET PAS UN `hauteurMax`.
				// Avec (max - min) / 255, aucune hauteur attendue n'est
				// exactement représentable : 1/255 est périodique en base 2.
				// Le négatif « terrain noir => plat, écart 0 AU BIT » deviendrait
				// alors inatteignable et devrait être remplacé par un seuil —
				// c'est-à-dire qu'on perdrait exactement le test qui mord. Avec
				// un pas par niveau puissance de deux (1/32, 1/64…), TOUTES les
				// hauteurs attendues sont exactes en float32.
				// `DepuisMinMax` reste là pour l'usage courant, qui pense en
				// min/max et se moque du bit.
				float32 hauteurMin = 0.f;
				float32 hauteurParNiveau = 1.f / 255.f;

				// ── UN CANAL NOMMÉ, PAS UNE LUMINANCE ───────────────────────
				// Une luminance 0.2126R + 0.7152G + 0.0722B rendrait l'attendu
				// non calculable au bit. Un canal est une LECTURE ; une
				// luminance est un CALCUL, et un calcul se glisse entre ce que
				// l'image dit et ce que le sommet vaut.
				int32 canal = 0;

				// Centre le terrain sur l'origine (sinon le coin (0,0) de
				// l'image est à x=0, z=0).
				bool centre = true;

				// Ombrage lissé (Face::smooth = 1) : un terrain facetté n'est
				// pas ce qu'on veut voir, et `TriangulateShaded` dédoublerait
				// les coins pour le rendre. Cf. la note d'exactitude dans le .cpp.
				bool lisse = true;

				// Usage courant : min/max, au prix du bit (voir ci-dessus).
				static NkTerrainParams DepuisMinMax(float32 hMin, float32 hMax, float32 pasX = 1.f,
													float32 pasZ = 1.f) {
					NkTerrainParams p;
					p.pasX = pasX;
					p.pasZ = pasZ;
					p.hauteurMin = hMin;
					p.hauteurParNiveau = (hMax - hMin) / 255.f;
					return p;
				}
		};

		// ── LA LOI, EXPOSÉE ─────────────────────────────────────────────────
		// Elle est publique pour qu'un appelant — et surtout un banc — puisse
		// calculer l'attendu SANS redemander au maillage ce qu'il est justement
		// en train de vérifier.
		inline float32 NkTerrainHauteurDepuisNiveau(const NkTerrainParams &p, float32 niveau) {
			return p.hauteurMin + niveau * p.hauteurParNiveau;
		}

		// ── COMBIEN DE SOMMETS, COMBIEN DE TRIANGLES ────────────────────────
		// Écrites comme des fonctions et non comme un commentaire : un appelant
		// qui dimensionne un tampon doit obtenir le MÊME nombre que celui que
		// le maillage produira, sans le recopier.
		inline uint32 NkTerrainNbSommets(uint32 N, uint32 M) {
			return N * M;
		}
		inline uint32 NkTerrainNbQuads(uint32 N, uint32 M) {
			return (N < 2u || M < 2u) ? 0u : (N - 1u) * (M - 1u);
		}
		inline uint32 NkTerrainNbTriangles(uint32 N, uint32 M) {
			return 2u * NkTerrainNbQuads(N, M);
		}
		// ── ARÊTES : MON ATTENDU ÉTAIT FAUX, ET LA MESURE L'A DIT ───────────
		// J'avais écrit, avant de mesurer :
		//     N*(M-1) + M*(N-1) + (N-1)*(M-1)     « verticales + horizontales
		//                                            + une diagonale par quad »
		// Mesuré sur 9x9 : 144 arêtes, pas 208. Sur 6x10 : 104, pas 149. Sur
		// 5x5 : 40, pas 56. Les trois écarts sont EXACTEMENT (N-1)(M-1), c'est-
		// à-dire mes diagonales.
		//
		// La prémisse était fausse, pas le calcul : **ce maillage est fait de
		// QUADS, pas de triangles.** `NkEditMesh` porte des n-gons ; la diagonale
		// n'apparaît qu'à la TRIANGULATION, qui est un cache d'affichage et ne
		// crée aucune arête dans la structure. Compter les diagonales, c'était
		// compter les arêtes d'un maillage que le moteur ne construit pas.
		// La formule ci-dessous est celle du quadrillage de quads.
		inline uint32 NkTerrainNbAretes(uint32 N, uint32 M) {
			if (N < 2u || M < 2u)
				return 0u;
			return N * (M - 1u) + M * (N - 1u);
		}
		// Ce que l'attendu faux désignait vraiment : le nombre d'arêtes du
		// maillage TRIANGULÉ. Gardé, nommé, et distinct — un jour quelqu'un
		// voudra ce nombre-là, et il ne doit pas le confondre avec l'autre.
		inline uint32 NkTerrainNbAretesTriangulees(uint32 N, uint32 M) {
			if (N < 2u || M < 2u)
				return 0u;
			return NkTerrainNbAretes(N, M) + (N - 1u) * (M - 1u);
		}

		// ── LECTURE DES NIVEAUX ─────────────────────────────────────────────
		// Séparée de la construction, et c'est délibéré : un banc doit pouvoir
		// confronter L'IMAGE au maillage sans re-décoder l'image lui-même, avec
		// les mêmes conventions de canal et de ligne.
		// Sortie : `outNiveaux[j * outN + i]`, ligne 0 = ligne du HAUT de l'image.
		// Formats entiers -> 0..255. Formats flottants -> la valeur brute, sans
		// plafond (c'est le seul chemin qui échappe aux 256 paliers du 8 bits ;
		// NKImage n'a pas de gris 16 bits entier — voir la feuille de route).
		NkTerrainStatut NkTerrainLireNiveaux(const NkImage &img, const NkTerrainParams &p,
											 NkVector<float32> &outNiveaux, uint32 &outN, uint32 &outM);

		// ── LA PORTE ────────────────────────────────────────────────────────
		// Construit `out` par `NkEditMesh::BuildFromPolygons` (quads, n-gons),
		// puis pose l'ombrage et recalcule les normales.
		// `out` est entièrement remplacé. En cas de statut != NK_OK, `out` n'est
		// PAS touché : un maillage à moitié construit se confond avec un
		// maillage construit.
		NkTerrainStatut NkTerrainDepuisHeightMap(const NkImage &img, const NkTerrainParams &p, NkEditMesh &out);

		// Variante sans image : les niveaux sont déjà en main (génération
		// procédurale, sortie d'un autre étage). Même géométrie, mêmes formules.
		NkTerrainStatut NkTerrainDepuisNiveaux(const float32 *niveaux, uint32 N, uint32 M,
											   const NkTerrainParams &p, NkEditMesh &out);

	} // namespace renderer
} // namespace nkentseu
