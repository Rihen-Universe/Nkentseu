// =============================================================================
// NkUnkenyFormes.h — les FORMES 2D (le « mesh renderer » d'Unkeny)
//
// A QUOI SERT CE FICHIER
//   Demande de Rihen (01/10) : « je ne peux pas ajouter de mesh renderer comme
//   des cercles, des polygones, des triangles ». Il declare la FORME DESSINEE
//   d'une entite -- rectangle (et carre), cercle, ellipse, triangle, etoile,
//   polygone regulier, capsule, ligne / chaine, polygone libre -- avec son
//   remplissage, son contour, ses coins arrondis et son opacite ; puis la
//   geometrie qui en sort (contour, sommets) et le COLLISIONNEUR ASSORTI.
//
// ⚠️ CE QUI EXISTAIT, ET POURQUOI CE N'EN EST PAS UNE COPIE
//   - NkSprite2D : un QUAD (couleur ou image). Une balle y etait un carre.
//   - NkDessinerFormes (Rendu/) : le dessin d'un COLLISIONNEUR (boite, cercle,
//     capsule), aux couleurs du decor ou du sprite. C'est de la physique
//     rendue visible, pas une forme qu'on regle : ni contour, ni etoile, ni
//     arrondi, ni couche.
//   - NKGui (NkGuiDrawList) sait remplir un polygone CONVEXE, tracer une ligne
//     brisee, des cercles et des ellipses ; NKMath sait trianguler un contour
//     CONCAVE (NkEarcutVers, sans allocation). Le dessin s'appuie sur eux,
//     rien n'est reecrit (Rendu/NkUnkenyRendu.cpp).
//
// ⚠️ LA REGLE DES COMPOSANTS TIENT : ce fichier compile sans NKGui ni
//   NKCanvas. Des DONNEES et de la geometrie pure ; le dessin est dans Rendu/.
//
// LE COLLISIONNEUR ASSORTI (NkCollisionneurDepuisForme)
//   « Ce qu'on voit est ce qui touche », dans la mesure de ce que le solveur
//   sait faire, et quand il ne le sait pas, c'est DIT ici :
//     rectangle -> boite ; cercle -> cercle ; capsule -> capsule ;
//     triangle, polygone convexe <= 8 sommets -> polygone exact ;
//     contour concave (etoile, polygone libre, ligne epaisse), ellipse,
//     polygone de plus de 8 cotes :
//        DECOR (statique, cinematique, sans corps) -> CHAINE fermee : exacte ;
//        DYNAMIQUE -> enveloppe convexe reduite a 8 sommets (NKCollision ne
//        lit que 8 sommets d'un polygone, et ne fait pas tomber un concave).
//   Les coins arrondis ne sont pas suivis par le collisionneur (ecart d'au
//   plus le rayon de l'arrondi, aux coins).
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#pragma once

#include "Unkeny/Scene/NkUnkenyChamps.h"
#include "Unkeny/Scene/NkUnkenyComposants.h"

namespace nkentseu {
	namespace unkeny {

		/// Les genres de forme. AJOUTES A LA FIN : un fichier porte le numero.
		enum class NkGenreForme2D : uint8 {
			NK_RECTANGLE = 0, ///< et le carre : un rectangle de cotes egaux
			NK_CERCLE,
			NK_ELLIPSE,
			NK_TRIANGLE,
			NK_ETOILE,			  ///< `branches`, `rayonInterieur`
			NK_POLYGONE_REGULIER, ///< `cotes`
			NK_CAPSULE,
			NK_LIGNE,			  ///< ligne / chaine ouverte : `points`, `epaisseur`
			NK_POLYGONE_LIBRE,	  ///< contour ferme quelconque : `points`
			NK_COUNT
		};

		/// Les points d'une ligne ou d'un polygone libre.
		static constexpr uint32 NK_FORME_POINTS_MAX = 32u;
		/// Les points d'un contour DESSINE (arrondis et courbes tessellees).
		static constexpr uint32 NK_FORME_CONTOUR_MAX = 512u;

		/// La forme dessinee d'une entite. Centree sur l'origine de l'entite et
		/// inscrite dans `taille` (sauf ligne et polygone libre : leurs points).
		/// ⚠️ Les couleurs sont en 0xRRGGBBAA, comme NkSprite2D (NkUnkenyRendu.h).
		struct NkRenduForme2D {
				NkGenreForme2D genre = NkGenreForme2D::NK_RECTANGLE;
				NkVec2f taille{1.f, 1.f}; ///< m, la boite de la forme, avant echelle
				uint32 remplissage = 0x4C9BE8FFu;
				bool rempli = true;
				uint32 couleurContour = 0x16212EFFu;
				float32 epaisseurContour = 0.f; ///< m ; 0 = pas de contour
				/// m, le rayon des COINS (rectangle, triangle, polygones, etoile).
				/// Borne a la moitie de l'arete la plus courte d'un coin.
				float32 arrondi = 0.f;
				float32 opacite = 1.f;		  ///< 0..1, multiplie remplissage et contour
				uint8 cotes = 6;			  ///< polygone regulier, 3..32
				uint8 branches = 5;			  ///< etoile, 3..16
				float32 rayonInterieur = 0.45f; ///< etoile : fraction du rayon exterieur
				float32 epaisseur = 0.12f;	  ///< m, le trait d'une ligne
				int32 couche = 0;			  ///< ordre de dessin, partage avec les sprites
				bool visible = true;
				/// L'editeur REFAIT le collisionneur quand la forme change (taille,
				/// genre, cotes...). Faux : le collisionneur se regle a la main.
				bool collisionSuit = true;
				uint8 nbPoints = 0;
				/// Ligne et polygone libre, en m dans le repere de l'entite.
				NkVec2f points[NK_FORME_POINTS_MAX] = {};
		};

		/// Le nom affiche d'un genre (« Rectangle », « Étoile »...).
		const char *NkNomGenreForme2D(NkGenreForme2D g) noexcept;

		/// Une forme prete a poser : taille, couleurs, cotes, points d'exemple.
		NkRenduForme2D NkFormeParDefaut(NkGenreForme2D g) noexcept;

		/// Les SOMMETS de la forme (coins vifs, sans arrondi ni courbe), repere de
		/// l'entite, avant echelle. Cercle, ellipse, capsule : un contour
		/// tessele. `ferme` est faux pour une ligne. Rend leur nombre.
		uint32 NkSommetsForme2D(const NkRenduForme2D &f, NkVec2f *sortie, uint32 capacite, bool &ferme) noexcept;

		/// Le contour DESSINE : arrondis compris. Rend le nombre de points.
		uint32 NkContourForme2D(const NkRenduForme2D &f, NkVec2f *sortie, uint32 capacite, bool &ferme) noexcept;

		/// Vrai si le contour dessine peut etre CONCAVE (il faut alors le
		/// trianguler au lieu de l'eventail d'un convexe).
		bool NkFormePeutEtreConcave(const NkRenduForme2D &f) noexcept;

		/// Distance SIGNEE d'un point (monde) a la forme dessinee, negative
		/// dedans ; une ligne compte son epaisseur. Le test de la prise au clic.
		float32 NkDistanceRenduForme2D(const NkTransform2D &t, const NkRenduForme2D &f, const NkVec2f &p) noexcept;

		/// La demi-boite englobante du contour, repere de l'entite (sans echelle).
		NkVec2f NkDemiBoiteForme2D(const NkRenduForme2D &f) noexcept;

		/// Le COLLISIONNEUR ASSORTI (voir l'en-tete). Ne touche QUE la geometrie
		/// de `c` (forme, tailles, decalage, rotation, sommets) : sa couche, son
		/// masque et « declencheur » sont gardes. `dynamique` : l'entite porte un
		/// corps dynamique (le concave devient alors son enveloppe convexe).
		bool NkCollisionneurDepuisForme(const NkRenduForme2D &f, bool dynamique, NkCollisionneur2D &c) noexcept;

		/// Le collisionneur depuis le CONTOUR D'UN SPRITE : l'enveloppe convexe des
		/// pixels dont l'alpha depasse `seuil`, reduite a 8 sommets, dans le
		/// repere du sprite (taille, pivot, region UV). Une image sans transparence
		/// donne sa boite. Faux si l'image est vide ou absente.
		bool NkCollisionneurDepuisSprite(const NkSprite2D &s, const uint8 *rgba, int32 w, int32 h, NkCollisionneur2D &c,
										 uint8 seuil = 128u) noexcept;

		/// Le CONTOUR d'un collisionneur, en MONDE (rotations de l'entite et
		/// propre, decalage) : ce que la physique touche, tel qu'on le trace
		/// (surcouche « Collisionneurs », poignees de l'editeur, occulteurs).
		/// Cercle et capsule : tesseles. `ferme` faux pour une chaine ouverte.
		uint32 NkContourCollisionneur2D(const NkTransform2D &t, const NkCollisionneur2D &c, NkVec2f *sortie,
										uint32 capacite, bool &ferme, uint32 segmentsCercle = 32u) noexcept;

		// --- La geometrie partagee ---------------------------------------------
		/// L'enveloppe convexe (chaine monotone d'Andrew), sens TRIGONOMETRIQUE.
		uint32 NkEnveloppeConvexe2D(const NkVec2f *pts, uint32 n, NkVec2f *sortie, uint32 capacite) noexcept;
		/// Retire des sommets d'un convexe jusqu'a `max`, chaque fois celui qui
		/// emporte le moins d'aire. Rend le nouveau nombre.
		uint32 NkReduireConvexe2D(NkVec2f *pts, uint32 n, uint32 max) noexcept;
		/// Le contour est-il convexe (dans un sens ou dans l'autre) ?
		bool NkEstConvexe2D(const NkVec2f *pts, uint32 n) noexcept;
		/// Aire signee (positive = sens trigonometrique).
		float32 NkAireSignee2D(const NkVec2f *pts, uint32 n) noexcept;

		/// Les champs du composant (sauvegarde, photo, fusion des prefabs).
		const NkChampSauve *NkChampsRenduForme2D(uint32 &nombre) noexcept;

	} // namespace unkeny
} // namespace nkentseu
