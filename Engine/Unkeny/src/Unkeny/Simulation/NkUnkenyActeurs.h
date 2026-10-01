// =============================================================================
// NkUnkenyActeurs.h — le catalogue des acteurs de SIMULATION
//
// A QUOI SERT CE FICHIER
//   Poser dans une NkScene un ballon, un blob visqueux, de l'eau, un cristal,
//   un tissu, une corde, un pont, une caisse… en UN appel, avec des reglages
//   mesures (NKPhysics/NkParticules2DFabrique). C'est le vocabulaire commun a
//   la demo Physic2D et a UnkenyEditor : sans lui, chacun aurait sa liste, et
//   un blob de l'editeur ne serait pas celui de la demo.
//
// POURQUOI IL EST DANS LE MOTEUR — decision du 2026-09-29
//   Il vivait dans Applications/Physic2D. L'editeur en avait besoin pour sa
//   palette « Placer des acteurs » : le copier aurait fait deux catalogues,
//   l'inclure depuis une application aurait fait dependre l'editeur d'une
//   demo. Il descend donc d'un etage. Ce qui reste a la demo : la BOITE de ses
//   niveaux, son marqueur de decor, la composition des niveaux.
//
// UN ACTEUR = UNE ENTITE
//   corps mou / fluide / atomes -> NkScene::CreerCorpsMou (NkCorpsMou2D)
//   caisse, balle               -> NkScene::AjouterCorps (corps rigide)
//   obstacle                    -> corps STATIQUE a collisionneur capsule
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#pragma once

#include "NKMath/NKMath.h"
#include "Unkeny/Scene/NkUnkenyFormes.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

namespace nkentseu {
	namespace unkeny {

		class NkTextures2D;
		class NkSons2D;

		enum class NkActeurSim : uint8 {
			NK_BALLON = 0,
			NK_BLOB,
			NK_SLIME,
			NK_GELEE,
			NK_EAU,
			NK_MIEL,
			NK_SABLE,
			NK_CRISTAL,
			NK_DISQUE,
			NK_TISSU,
			NK_CORDE,
			NK_PONT,
			NK_CAISSE, ///< corps RIGIDE (NkPhysicsWorld)
			NK_BALLE,  ///< corps RIGIDE (NkPhysicsWorld)
			NK_COUNT
		};

		/// Les rubriques d'une palette (celles de « Placer des acteurs » d'UE5).
		enum class NkCategorieActeur : uint8 { NK_CORPS_MOUS = 0, NK_FLUIDES, NK_ATOMES, NK_TISSUS, NK_RIGIDES, NK_COUNT };
		const char *NkCategorieActeurNom(NkCategorieActeur c) noexcept;

		struct NkInfoActeurSim {
				const char *nom;
				const char *description;
				uint32 couleur; ///< RGBA
				bool pinceau;	///< maintenir verse de la matiere (fluides)
				bool rigide;
				bool trace;		///< se pose en GLISSANT d'un point a un autre (pont)
				NkCategorieActeur categorie;
		};
		const NkInfoActeurSim &NkActeurSimInfo(NkActeurSim a) noexcept;

		/// Ce que les acteurs empruntent au rendu et au son. Tout est facultatif :
		/// un identifiant nul donne un aplat (dessine par son collisionneur) ou
		/// le silence.
		struct NkRessourcesSim {
				uint32 texCaisse = 0;
				uint32 texBalle = 0;
				uint32 sonPose = 0;
				uint32 sonExplosion = 0;
				uint32 sonCoupe = 0;
		};
		/// Fabrique, PAR PROGRAMME, les textures (caisse en planches, balle a
		/// quartiers) et les bruitages (pose, explosion, coupe). Aucun fichier :
		/// rien a copier dans un APK. `textures` ou `sons` peut etre nul.
		void NkCreerRessourcesSim(NkRessourcesSim &sortie, NkTextures2D *textures, NkSons2D *sons);

		/// Pose un acteur en `pos`. Rend son entite (invalide si refuse, ou si la
		/// scene n'a pas de particules alors qu'il en faut).
		/// `plafond` : hauteur maximale de l'ACCROCHE d'un tissu ou d'une corde
		/// (ils pendent sous leur point d'attache, pose au-dessus du clic).
		ecs::NkEntityId NkPoserActeurSim(NkScene &scene, NkActeurSim a, const NkVec2f &pos,
										 const NkRessourcesSim *ressources = nullptr, float32 plafond = 1.0e30f);

		/// La MATIERE seule d'un acteur non rigide : le corps de particules, sans
		/// entite. Rend son index (ou -1). Pour AttacherCorpsMou sur une entite
		/// qui existe deja (« Ajouter un composant » d'un editeur).
		int32 NkCreerMatiereSim(NkScene &scene, NkActeurSim a, const NkVec2f &pos, float32 plafond = 1.0e30f);

		/// Pinceau de fluide : un corps VIDE, pret a recevoir NkVerserSim.
		ecs::NkEntityId NkOuvrirPinceauSim(NkScene &scene, NkActeurSim a);
		/// Verse jusqu'a `n` particules dans le corps de l'entite `e`. false si ce
		/// corps n'est plus le dernier cree (il faut en rouvrir un).
		bool NkVerserSim(NkScene &scene, ecs::NkEntityId e, const NkVec2f &pos, int32 n, uint32 &graine);
		/// Pont de planches entre deux points fixes.
		ecs::NkEntityId NkPoserPontSim(NkScene &scene, const NkVec2f &a, const NkVec2f &b);
		/// Paroi statique (capsule) de a a b ; a == b donne une cheville ronde.
		ecs::NkEntityId NkPoserObstacleSim(NkScene &scene, const NkVec2f &a, const NkVec2f &b, float32 rayon);

		// --- Les FORMES 2D (2026-10-01, NkUnkenyFormes.h) ----------------------
		/// Pose une forme en `pos` : l'entite (nommee `nom`, ou le nom du genre),
		/// sa NkRenduForme2D et, si `collisionneur`, le collisionneur ASSORTI
		/// (NkCollisionneurDepuisForme). `corps` >= 0 : un corps rigide de ce
		/// NkTypeCorps (la scene doit avoir sa physique). Rend l'entite.
		ecs::NkEntityId NkPoserForme2D(NkScene &scene, const NkRenduForme2D &f, const NkVec2f &pos, const char *nom = nullptr,
									   bool collisionneur = true, int32 corps = -1);
		/// Refait le collisionneur de l'entite depuis sa forme (couche, masque et
		/// « declencheur » gardes ; corps rigide refait s'il y en a un). Faux si
		/// l'entite n'a pas de forme.
		bool NkRefaireCollisionneurForme(NkScene &scene, ecs::NkEntityId id);

		/// Les noms sont numerotes par type (Blob_visqueux_3). Remettre a zero au
		/// chargement d'un niveau garde des noms courts.
		void NkRemettreNomsSim() noexcept;
		/// Le nom suivant pour ce type (« Blob_visqueux_4 ») — pour qui fabrique un
		/// acteur a la main et veut la meme numerotation que le catalogue.
		void NkNommerActeurSim(char *sortie, usize taille, NkActeurSim a) noexcept;

	} // namespace unkeny
} // namespace nkentseu
