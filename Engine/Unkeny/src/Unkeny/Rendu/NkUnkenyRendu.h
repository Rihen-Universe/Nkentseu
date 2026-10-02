// =============================================================================
// NkUnkenyRendu.h — dessiner une scene 2D
//
// A QUOI SERT CE FICHIER
//   Il parcourt les entites qui ont un transform et un sprite, et il les
//   dessine a travers la camera. C'est tout : il ne modifie aucun composant.
//
// ⚠️ L'ORDRE DE DESSIN EST UNE DONNEE, PAS UN HASARD
//   NKECS n'offre AUCUNE garantie sur l'ordre d'iteration des archetypes — et
//   c'est normal, ce n'est pas son travail. Un rendu qui dessine dans l'ordre
//   de la requete produit donc un empilement qui CHANGE quand on ajoute un
//   composant a une entite. Le defaut se presente comme « le personnage passe
//   parfois derriere le decor », un coup sur deux, sans rien qui l'explique.
//   On trie donc explicitement par `NkSprite2D::couche`. Le tri coute ; ne pas
//   trier coute plus cher, et plus tard.
//
// ⚠️ ET LE HORS-CHAMP N'EST PAS DESSINE
//   Une scene de mille entites dont trente sont visibles ne doit pas emettre
//   mille quads. Le test se fait sur la ZONE VISIBLE de la camera, en monde —
//   pas apres conversion en pixels, ce qui reviendrait a payer la conversion
//   pour rien.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - un type de dessin (ligne, cercle, texte de scene) -> ici
//   - un effet propre a un jeu                          -> chez le jeu, apres
//                                                          l'appel a Dessiner
// =============================================================================
#pragma once

#include "NKGui/Core/NkGuiContext.h"
#include "Unkeny/Maillage/NkUnkenyMaillage.h"
#include "Unkeny/Scene/NkUnkenyFormes.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

namespace nkentseu {
	namespace unkeny {
		struct NkSquelette2D; // Squelette/NkUnkenySquelette.h (R30)

		struct NkStatsRendu {
				int32 entitesVues = 0;	  ///< entites avec sprite visible
				int32 entitesDessinees = 0; ///< celles qui ont passe le hors-champ
		};

		/// Dessine tous les sprites de la scene, tries par couche.
		/// Rend de quoi mesurer : sans compteur, « pourquoi c'est lent » n'a pas
		/// de reponse, et « le culling marche-t-il » non plus.
		/// (2026-10-01) Et les FORMES 2D (NkRenduForme2D), dans le MEME tri : une
		/// forme de couche 2 passe devant un sprite de couche 1, et derriere un
		/// de couche 3. Elles comptent dans les statistiques comme les sprites.
		NkStatsRendu NkDessinerScene(nkgui::NkGuiDrawList &dl, NkScene &scene);

		/// Une forme 2D (NkUnkenyFormes.h) sous le transform `t`, a travers la
		/// camera : remplissage (eventail si convexe, triangulation NkEarcutVers
		/// si elle peut etre concave), contour, opacite. C'est le dessin de
		/// NkDessinerScene, offert a qui veut une forme hors d'une scene.
		void NkDessinerRenduForme2D(nkgui::NkGuiDrawList &dl, const NkVue2D &camera, const NkTransform2D &t,
									const NkRenduForme2D &f);

		/// (2026-10-02, R31) Un MAILLAGE 2D dont les sommets sont DEJA en monde
		/// (NkMaillagePositionsMonde) : la texture deformee, une couleur par sommet
		/// (multipliee par la teinte), les parties dans leur ordre de dessin. C'est
		/// le dessin de NkDessinerScene, offert a la fenetre d'edition.
		/// (R30) `squelette` (facultatif) : ses EMPLACEMENTS cachent les attaches non
		/// montrees et donnent son ordre a celle qui l'est (NkEmplacementsParties2D).
		void NkDessinerMaillage2D(nkgui::NkGuiDrawList &dl, const NkVue2D &camera, const NkMaillage2D &m,
								  const NkVec2f *monde, const NkSquelette2D *squelette = nullptr);

		/// La meme forme en VIGNETTE, inscrite dans le rectangle `r` (en pixels) :
		/// les icones du panneau « Placer des acteurs ».
		void NkDessinerFormeVignette(nkgui::NkGuiDrawList &dl, const NkRenduForme2D &f, const nkgui::NkRect &r);

		/// ⚠️ TOUTES LES COULEURS D'UNKENY SONT EN 0xRRGGBBAA — l'alpha en
		/// QUEUE. Ecrite en ARGB par habitude, `0x20FFFFFF` ne donne pas un
		/// blanc a 12 % : elle donne un CYAN OPAQUE, et `0x8000FF00` donne du
		/// transparent pur. Les deux se compilent, les deux « marchent », et
		/// seule l'image le dit — defaut trouve sur une capture le 2026-09-01.
		/// La convention est celle de NkSprite2D::couleur : elle ne change pas
		/// d'un fichier a l'autre.

		/// Contours des collisionneurs, en superposition. C'est l'outil qui rend
		/// un defaut de physique VISIBLE : un sprite decale de son collisionneur
		/// se cherche pendant des heures sans lui, et se voit en une seconde
		/// avec.
		void NkDessinerCollisionneurs(nkgui::NkGuiDrawList &dl, NkScene &scene, uint32 couleur = 0x00E07AC0u);

		/// Les FORMES pleines : chaque entite a collisionneur dessinee depuis sa
		/// forme (boite, cercle, capsule), rotation comprise — ce qu'on voit est
		/// exactement ce qui touche. Les corps DYNAMIQUES prennent la couleur de
		/// leur sprite et des details qui disent le mouvement (le reflet et le
		/// repere d'une balle qui roule, le cadre d'une caisse) ; les autres sont
		/// du decor. Un corps dont le sprite est TEXTURE et visible est laisse a
		/// NkDessinerScene.
		///
		/// Ajoute le 2026-09-29, depuis la demo Physic2D : NkDessinerScene ne
		/// dessine que des sprites carres (une balle y serait un carre) et
		/// NkDessinerCollisionneurs que des contours. L'editeur en avait besoin
		/// a son tour.
		struct NkOptionsFormes {
				uint32 decor = 0x606472FFu;		  ///< RGBA des statiques et cinematiques
				uint32 decorBord = 0x969CACFFu;
				/// Couleur propre d'un decor (le sol plus sombre que les obstacles).
				/// Rendre 0 garde `decor`. Facultatif.
				uint32 (*couleurDecor)(ecs::NkWorld &monde, ecs::NkEntityId id, void *donnees) = nullptr;
				void *donnees = nullptr;
		};
		void NkDessinerFormes(nkgui::NkGuiDrawList &dl, NkScene &scene, const NkOptionsFormes &options = NkOptionsFormes());

		/// Une grille de reperage en coordonnees de MONDE. Elle dit ou est
		/// l'origine et quelle taille fait une unite — les deux questions qu'on
		/// se pose devant une scene qui ne s'affiche pas ou l'on croyait.
		void NkDessinerGrille(nkgui::NkGuiDrawList &dl, const NkVue2D &camera, float32 pas = 1.f,
							  uint32 couleur = 0xFFFFFF1Eu, uint32 couleurAxes = 0xFFFFFF5Au);

	} // namespace unkeny
} // namespace nkentseu
