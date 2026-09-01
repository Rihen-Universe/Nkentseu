// =============================================================================
// NkUnkenyComposants.h — les composants d'une scene 2D
//
// A QUOI SERT CE FICHIER
//   Il declare ce qu'une entite PEUT porter. Rien d'autre : pas de logique, pas
//   de dessin, pas de systeme. Ce sont des DONNEES, et elles doivent le rester —
//   c'est ce qui permet de les serialiser, de les inspecter et de les editer.
//
// ⚠️ LA REGLE QUI TIENT TOUT LE FICHIER
//   Un composant ne porte AUCUN type d'interface et AUCUN pointeur vers un
//   systeme. Test : *ce fichier compile-t-il sans NKGui et sans NKCanvas ?* Oui.
//   C'est ce qui a rendu possible le portage des panneaux de Nogee, et c'est ce
//   qui permettra a un editeur d'inspecter une scene sans la faire tourner.
//
// ⚠️ ET LA REGLE QUI EVITE LA DIVERGENCE
//   `NkCorps2D` ne stocke PAS la position : il stocke un `NkBodyId`. La position
//   d'un corps physique appartient au monde physique, une seule fois. Deux
//   copies — une dans le composant, une dans le solveur — divergent au premier
//   pas de simulation, et le defaut se presente comme « le sprite est a cote du
//   collisionneur ».
//   Le systeme de physique RECOPIE le resultat dans NkTransform2D apres chaque
//   pas ; c'est un sens unique, et il est ecrit.
//
// LE PLAN 2D
//   Unkeny travaille dans le plan XY. NKPhysics et NKCollision sont capables de
//   3D ; on y entre avec z = 0 et on n'expose que x, y et l'angle autour de Z.
//   Le pont fait la conversion en UN seul endroit (NkUnkenyPhysique).
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - une donnee portee par une entite -> ici, en struct simple
//   - un comportement                  -> un systeme, jamais un composant
//   - un besoin propre a UN jeu        -> chez ce jeu ; NkWorld accepte
//                                         n'importe quel type
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace unkeny {

		using math::NkVec2f;

		/// Position, rotation, echelle. C'est la VERITE d'affichage d'une entite.
		///
		/// ⚠️ Pas de matrice stockee : elle se recalcule au dessin. Une matrice
		/// en cache est une seconde source de verite, et il faut alors se rappeler
		/// de l'invalider — ce dont on ne se rappelle jamais.
		struct NkTransform2D {
				NkVec2f position{0.f, 0.f};
				float32 rotation = 0.f; ///< radians, sens trigonometrique
				NkVec2f echelle{1.f, 1.f};

				/// L'ordre est T * R * S : on tourne autour de l'ORIGINE de
				/// l'entite, pas autour du coin de l'ecran.
				NkVec2f VersMonde(const NkVec2f &local) const noexcept {
					const float32 c = math::NkCos(rotation);
					const float32 s = math::NkSin(rotation);
					const NkVec2f e(local.x * echelle.x, local.y * echelle.y);
					return NkVec2f(position.x + e.x * c - e.y * s, position.y + e.x * s + e.y * c);
				}
		};

		/// Un rectangle colore ou texture. `texId` a 0 = forme pleine, sans image.
		struct NkSprite2D {
				NkVec2f taille{1.f, 1.f}; ///< en unites de monde, avant echelle
				NkVec2f pivot{0.5f, 0.5f}; ///< 0,0 = coin haut-gauche ; 0.5,0.5 = centre
				uint32 couleur = 0xFFFFFFFFu;
				uint32 texId = 0;
				/// Region de l'atlas, en coordonnees normalisees. Tout l'atlas par defaut.
				NkVec2f uv0{0.f, 0.f};
				NkVec2f uv1{1.f, 1.f};
				int32 couche = 0; ///< ordre de dessin ; le plus grand passe devant
				bool visible = true;
		};

		/// Forme de collision, en donnees pures. Le pont la traduit en NkShape.
		enum class NkForme2D : uint8 { NK_CERCLE = 0, NK_BOITE, NK_CAPSULE };

		struct NkCollisionneur2D {
				NkForme2D forme = NkForme2D::NK_BOITE;
				NkVec2f demiTaille{0.5f, 0.5f}; ///< boite : demi-extents ; capsule : x = demi-longueur
				float32 rayon = 0.5f;			///< cercle et capsule
				NkVec2f decalage{0.f, 0.f};		///< par rapport au transform

				/// Couches et masque : deux entites n'entrent en collision que si
				/// chacune est dans le masque de l'autre. C'est ce qui permet aux
				/// balles du joueur d'ignorer le joueur sans code special.
				uint32 couche = 0x1u;
				uint32 masque = 0xFFFFFFFFu;

				/// Un declencheur detecte sans repousser. Une zone de fin de
				/// niveau, un ramassage, un capteur.
				bool declencheur = false;
		};

		enum class NkTypeCorps : uint8 {
			NK_STATIQUE = 0, ///< ne bouge jamais : murs, sol
			NK_CINEMATIQUE,	 ///< bouge, mais rien ne le pousse : plateforme mobile
			NK_DYNAMIQUE	 ///< subit forces et chocs
		};

		/// ⚠️ NE STOCKE PAS LA POSITION. Voir l'en-tete de fichier : elle
		/// appartient au monde physique, une seule fois.
		struct NkCorps2D {
				NkTypeCorps type = NkTypeCorps::NK_DYNAMIQUE;
				float32 masse = 1.f;
				float32 amortissementLineaire = 0.f;
				float32 amortissementAngulaire = 0.05f;
				float32 echelleGravite = 1.f;
				bool rotationBloquee = false; ///< un personnage de plateforme ne bascule pas

				/// Rempli par la scene a la creation. 0 = pas encore enregistre.
				/// ⚠️ Ne pas l'ecrire a la main : c'est le lien vers le solveur.
				uint32 corpsId = 0;
		};

		/// Un nom lisible. Sert a l'editeur, aux journaux et au debogage — pas au
		/// jeu : chercher une entite par son nom a chaque trame est un piege de
		/// performance ET de renommage.
		struct NkEtiquette {
				char nom[32] = {};
		};

		/// Vitesse imposee a la main, pour ce qui n'a pas besoin de physique.
		/// Un jeu de plateau, un menu qui glisse, un decor qui defile.
		struct NkVitesse2D {
				NkVec2f lineaire{0.f, 0.f};
				float32 angulaire = 0.f;
		};

	} // namespace unkeny
} // namespace nkentseu
