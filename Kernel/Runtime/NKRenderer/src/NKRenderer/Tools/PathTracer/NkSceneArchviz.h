#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/PathTracer/NkSceneArchviz.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// =============================================================================
//  LA DESCRIPTION DE SCENE COMMUNE (09/11/2026, rendu-realiste)
// =============================================================================
//  Le traceur de chemins (NkTraceurChemins) et la reference Cycles lisent la MEME
//  description : on compare alors deux integrateurs, pas deux scenes.
//    - la geometrie : les .obj (et .mtl) exportes a cote des .nkgeo d'un assemblage
//      .nkscene de l'atelier (pieces `forme maillage fichier X.nkgeo origine x y z`) ;
//    - les matieres : `materiaux_scene.json` de l'archviz (bibliotheque nommee ;
//      attribution PAR NOM D'OBJET « document:piece » : d'abord `pieces`, puis la
//      premiere des `regles` [regex document, regex piece, matiere] qui convient) ;
//    - les lumieres : `lumieres_scene.json` (sources point / spot / ruban en lumens
//      et kelvins, liees a leur objet ; ambiances : soleil, ciel en degrade, EV).
//  Le document d'une piece : la ligne `scene X` de `<dossier>/<document>.nkscene`
//  est la ligne `o X` du .obj ; les sources d'un document sont decalees de l'origine
//  de sa piece dans l'assemblage.
//  UNITES : 1 unite = 1 lux x 1e-3 (soleil du soir 2 500 lux = 2,5, la valeur de
//  NK_SOLEIL de l'archviz) ; une source de F lumens a la puissance P = F x 1e-3 ;
//  point : I = P / 4 pi ; spot : I = P / (2 pi (1 - cos demi-angle)) ; ruban :
//  points isotropes tous les 25 cm au plus (flux par metre x longueur).
//  Aucune bibliotheque standard : JSON et expressions regulieres (le sous-ensemble
//  employe par l'archviz : ^ $ | ( ) . [..] \d, sans quantificateur) ecrits ici.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace renderer {

		struct NkScMateriau {
				char nom[48] = {0};
				float32 couleur[3] = {0.8f, 0.8f, 0.8f}; ///< albedo lineaire
				float32 rugosite = 0.8f;
				float32 metal = 0.f;
				float32 transmission = 0.f; ///< 1 : verre (dielectrique lisse, n = 1,5)
				float32 emission = 0.f;		///< radiance emise = couleur x emission (unites de scene)
				int32 classe = -1;			///< carte procedurale (NkMatieresProcedurales), -1 aucune
				float32 tuile = 1.f;		///< metres par tuile de la carte
				float32 lustre = 0.f;		///< (10/11) lustre des fibres (sheen) : 0 aucun, 1 velours
				float32 lustreRug = 0.4f;	///< rugosite du lustre (Charlie) : petite = reflet rasant serre
				// (10/11/2026) LA PEINTURE DE CARROSSERIE : les parametres du type NK_CAR_PAINT (gabarit
				// mTmplCarPaint), lus comme le temps reel les lit -- NKRenderer/Materials/NkPeintureCarrosserie.h
				float32 vernis = 0.f;			 ///< clearcoat : poids de la couche vernie, 0 aucune
				float32 vernisRug = 0.03f;		 ///< clearcoatRough : rugosite du vernis
				float32 paillettes = 0.f;		 ///< sheen de la carrosserie : quantite de paillettes, 0 aucune
				float32 paillettesDensite = 1.f; ///< anisotropy de la carrosserie : densite 0..1 (25 a 0,5 mm)
				// (11/11/2026, nkcraft-arbres) LA CARTE DE L'OBJET : une image RGBA lue aux UV DU MAILLAGE
				// (v = 0 en haut de l'image, la convention des primitives et de l'atelier), pas une carte
				// procedurale en metres. Ses couleurs MULTIPLIENT `couleur` (le temps reel fait de meme :
				// texture x teinte). Les feuilles detourees de NKCraft (Vegetation/NkArbre) en ont besoin :
				// sans elle le traceur rendait une carte de feuille comme un polygone plein.
				char carte[260] = {0};	   ///< vide : aucune
				float32 masque = 0.f;	   ///< seuil d'alpha de la carte : un texel plus transparent n'existe pas (0 : opaque)
				float32 translucide = 0.f; ///< part transmise en diffus vers l'autre face (feuille mince) : 0 aucune
		};

		struct NkScSource {
				int32 type = 0; ///< 0 point (isotrope), 1 spot
				float32 p[3] = {0.f, 0.f, 0.f};
				float32 dir[3] = {0.f, -1.f, 0.f}; ///< spot : axe
				float32 cosExt = -1.f;			   ///< spot : cosinus du demi-angle
				float32 rayon = 0.02f;			   ///< rayon de la source (ombres douces)
				float32 couleur[3] = {1.f, 1.f, 1.f};
				float32 intensite = 0.f; ///< intensite radiante (unites de scene / sr)
				float32 puissance = 0.f; ///< P = lumens x 1e-3 (echantillonnage)
				char objet[64] = {0};
		};

		/// (10/11/2026) une piece attribuee : son document, son groupe, l'index de sa matiere
		struct NkScAttribue {
				char doc[96] = {0};
				char piece[128] = {0};
				int32 mat = -1;
		};

		/// (10/11/2026) L'ATTRIBUTION DES MATIERES d'une scene de l'archviz (materiaux_scene.json :
		/// pieces nommees, puis regles re.search dans l'ordre, puis une matiere de repli par teinte du .mtl)
		/// et LES DOCUMENTS de l'assemblage (`o <scene>` d'une piece -> le .nkscene qui declare
		/// `scene <scene>`). UNE seule logique, lue par le traceur (NkScArchvizCharger) ET par le temps
		/// reel (NKCraft, mode realiste) : la meme matiere pour le meme objet des deux cotes.
		class NkScAttribution {
			public:
				NkScAttribution() = default;
				~NkScAttribution();
				NkScAttribution(const NkScAttribution &) = delete;
				NkScAttribution &operator=(const NkScAttribution &) = delete;
				/// lit les documents du dossier de `assemblage` et la bibliotheque `matieres` ; faux si rien
				/// n'a pu etre lu (une bibliotheque absente laisse les replis seuls)
				bool Ouvrir(const char *assemblage, const char *matieres);
				void Fermer();
				/// le document d'un objet `o <scene>` (le nom du .nkscene qui le declare) ; `doc` entre avec
				/// le document courant de la piece (vide au debut) : inconnu, il le garde, ou prend <scene>
				void Document(const char *scene, char *doc, int32 cap) const;
				/// l'index (dans `mats`) de la matiere d'une piece ; `kd` : la teinte du .mtl (le repli)
				int32 Matiere(const char *doc, const char *piece, const float32 *kd);
				/// l'index d'une matiere de la bibliotheque par son nom, -1 sinon
				int32 Index(const char *nom) const;
				/// la matiere de repli d'une teinte (creee au besoin ; sans compte)
				int32 Repli(const float32 *kd);

				NkVector<NkScMateriau> mats; ///< la bibliotheque, puis les replis crees
				int32 nBib = 0;
				int32 parNom = 0, parRegle = 0, parMtl = 0;
				bool jsonLu = false; ///< la bibliotheque a ete lue sans erreur

			private:
				struct Interne;
				Interne *mI = nullptr;
		};

		struct NkScArchviz {
				// geometrie (monde, metres, Y en haut) ; normales sortantes
				NkVector<float32> P;  ///< 3 par sommet
				NkVector<float32> N;  ///< 3 par sommet
				NkVector<float32> UV; ///< 2 par sommet (metres, projection en boite ; / tuile au rendu)
				NkVector<uint32> tri; ///< 3 indices par triangle
				NkVector<int32> triMat;
				NkVector<NkScMateriau> mats;
				NkVector<NkScSource> sources;
				// l'ambiance
				float32 soleilDir[3] = {0.f, 1.f, 0.f}; ///< VERS le soleil
				float32 soleilCouleur[3] = {1.f, 1.f, 1.f};
				float32 soleilE = 0.f; ///< eclairement (unites de scene) sur un plan face au soleil
				float32 cielZenith[3] = {0.25f, 0.45f, 0.85f};
				float32 cielHorizon[3] = {0.75f, 0.85f, 0.95f};
				float32 cielIntensite = 1.f;
				// (10/11/2026) LE CIEL EN TABLE lat-long (W x H, RGB a plat ; theta depuis le zenith,
				// phi autour de +y depuis +x vers +z : NkHosekCielTable). Present, il remplace le
				// degrade (x cielIntensite).
				NkVector<float32> cielTable;
				int32 cielW = 0, cielH = 0;
				float32 expositionEV = 10.f;
				float32 mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
				// le compte rendu de l'attribution (preuve : le meme compte que la reference Cycles)
				int32 piecesParNom = 0, piecesParRegle = 0, piecesParMtl = 0;
				// (10/11) les pieces attribuees, une entree par (document, groupe, matiere) : le banc des
				// matieres exactes les compare a celles du temps reel
				NkVector<NkScAttribue> attribues;
		};

		/// Charge la scene. `assemblage` : le .nkscene ; `matieres` / `lumieres` : les deux JSON ;
		/// `ambiance` : "soir" ou "jour" ; `terrain` : demi-cote du terrain d'herbe (0 : aucun).
		/// Ecrit le compte rendu sur la sortie. Rend faux si la geometrie manque.
		bool NkScArchvizCharger(NkScArchviz &s, const char *assemblage, const char *matieres, const char *lumieres,
								const char *ambiance, float32 terrain);

		/// (10/11/2026) LA ROTATION D'UNE PARTIE (`rotation rx ry rz`, degres) en matrice 3x3 (lignes) :
		/// R = Rz Ry Rx, la convention de l'atelier (HostSetNodeEuler) ; v' = R v + origine. Lue par le
		/// traceur et le temps reel (NKCraft, mode realiste). NK_ROTATION_MUTATION=1 : la rotation inverse
		/// (contre-epreuve de son banc).
		void NkScRotationEuler(const float32 deg[3], float32 R[9]);

		/// Recherche d'une expression reguliere (sous-ensemble ^ $ | ( ) . [..] \d) dans un texte
		/// (semantique de re.search). Expose pour son banc.
		bool NkScRegexCherche(const char *motif, const char *texte);

		/// Banc de la regex et du JSON (cas ecrits, attendus ecrits avant) ; rend le nombre d'echecs.
		int32 NkScArchvizBanc();

	} // namespace renderer
} // namespace nkentseu
