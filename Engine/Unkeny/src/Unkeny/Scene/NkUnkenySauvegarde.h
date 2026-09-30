// =============================================================================
// NkUnkenySauvegarde.h — ecrire une scene dans un fichier, et la relire
//
// A QUOI SERT CE FICHIER
//   Un niveau fait a l'editeur, une partie sauvegardee, une simulation qu'on
//   reprend : tout ce qu'une NkScene porte passe par un fichier .nkscene et en
//   revient a l'identique.
//
// LE FORMAT : JSON, par NKSerialization (NkArchive + NkJSONWriter/Reader)
//   Lisible, comparable dans git, editable a la main pour une retouche. Les
//   GRANDS tableaux (positions de 1 000 particules) sont ecrits en une chaine
//   de nombres separes par des espaces, "%.9g" — la precision qui rend un
//   float32 a l'identique. Un objet JSON par particule aurait decuple le
//   fichier sans rien rendre de plus lisible.
//
//     {
//       "format": "unkeny.scene", "version": 2, "prochainUid": N,
//       "config":   { physique, particules, gravite, pasFixe, pasMaxParTrame },
//       "camera":   "cx cy zoom rotation",
//       "entites":  [ { uid, parent, local, nom, transform, sprite{texture},
//                       collisionneur, corps{etat}, mou, lumiere, emetteur,
//                       jeu{ NomComposant: { champ: valeur... } | "octets hex" } } ],
//       "particules": { reglages, corps[], p{...}, l{...} },
//       "eclairage":  { actif, ambiante, ombres, masqueOcculteurs, mode, maille }
//     }
//
// LES VERSIONS — une lecture accepte toute version <= NK_UNKENY_SCENE_VERSION
//   1  (2026-09) : pas d'identite, pas de hierarchie ; tous les composants
//      "jeu" en OCTETS hex. RELUE A L'IDENTIQUE, par le meme code qu'avant :
//      les entites y recoivent les identites 1..N dans l'ordre du fichier.
//   2  (2026-09-29) : "uid", "parent" (uid du parent), "local" ; un composant
//      "jeu" DECRIT (PhotographierAussi<T>(nom, champs, n)) est un OBJET, champ
//      par champ — un champ absent garde sa valeur par defaut, un champ inconnu
//      est ignore : on relit une sauvegarde faite avant l'ajout d'un champ, et
//      sur une autre ABI. Un composant NON decrit reste en octets (inchange).
//
//   ⚠️ « lumiere », « emetteur » et « eclairage » (2026-09-30) ne sont ECRITS
//   que s'ils existent (l'eclairage : s'il differe du defaut), champ par champ,
//   et se LISENT dans les deux versions : un fichier sans eux se relit tel
//   quel et se reecrit a l'octet pres. Un moteur qui ne les connait pas les
//   ignore (ses lumieres disparaissent, rien ne casse).
//
// CE QUI EST SAUVE
//   - les composants d'Unkeny, un par un, champ par champ (lumieres et
//     emetteurs compris, 2026-09-30) ;
//   - l'ETAT des corps rigides (position, orientation, vitesses) ;
//   - tout le monde de particules (corps, particules, liens, reglages) ;
//   - l'IDENTITE de chaque entite et sa HIERARCHIE (v2) ;
//   - les composants declares par NkScene::PhotographierAussi<T>("Nom") — les
//     noms sont la cle : un composant inconnu a la relecture est ignore.
//   - Les REFERENCES des composants decrits voyagent par ce qui ne change pas :
//     une entite par son identite, un son / une texture / un prefab par son
//     NOM (NkRessourcesScene). Sans la ressource a l'ecriture, le numero de
//     session est ecrit, comme avant.
//
// ⚠️ CE QUI NE L'EST PAS, et pourquoi
//   - Les POIGNEES d'entite (ecs::NkEntityId) : refaites au chargement. Ce qui
//     les reference passe par l'identite (ci-dessus). Les corps mous gardent
//     leurs identifiants.
//   - Les PIXELS des textures : on ecrit le NOM de la texture (NkTextures2D),
//     et on la retrouve au chargement par ce nom — deja enregistree, ou lue
//     depuis le disque. Sans NkTextures2D, le sprite revient sans image.
//   - Le generateur pseudo-aleatoire interne des particules (agitation
//     thermique) : une simulation rechargee est la meme, pas la meme au bit
//     pres dans ses trames suivantes.
//   - Les octets d'un composant "jeu" NON decrit dependent de la plateforme
//     (boutisme, alignement) : ceux-la se relisent sur la meme famille de
//     machines seulement. Decrire le composant leve la limite.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#pragma once

#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringView.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

namespace nkentseu {
	class NkArchive;
	namespace unkeny {

		class NkTextures2D;
		class NkSons2D;
		class NkPrefabs2D;

		/// Version du format ecrit. Une lecture accepte toute version <= celle-ci.
		/// 2 depuis le 2026-09-29 (identites, hierarchie, composants champ par champ).
		constexpr int32 NK_UNKENY_SCENE_VERSION = 2;

		/// Les registres qui donnent un NOM a un identifiant de session. Chacun est
		/// facultatif : absent a l'ecriture, le numero est ecrit ; absent a la
		/// relecture, le nom ne se resout pas (0, et c'est dit).
		struct NkRessourcesScene {
				NkTextures2D *textures = nullptr;
				NkSons2D *sons = nullptr;
				NkPrefabs2D *prefabs = nullptr;
		};

		bool NkSauverScene(NkScene &scene, NkArchive &sortie, const NkTextures2D *textures = nullptr);
		bool NkSauverScene(NkScene &scene, NkArchive &sortie, const NkRessourcesScene &ressources);

		/// Remplace TOUTE la scene par celle de l'archive (la scene est
		/// re-initialisee avec la configuration du fichier). En cas d'echec, la
		/// scene est laissee intacte et `erreur` dit pourquoi.
		bool NkChargerScene(NkScene &scene, const NkArchive &entree, NkTextures2D *textures = nullptr,
							NkString *erreur = nullptr);
		bool NkChargerScene(NkScene &scene, const NkArchive &entree, const NkRessourcesScene &ressources,
							NkString *erreur = nullptr);

		bool NkSauverSceneJSON(NkScene &scene, NkString &json, const NkTextures2D *textures = nullptr);
		bool NkSauverSceneJSON(NkScene &scene, NkString &json, const NkRessourcesScene &ressources);
		bool NkChargerSceneJSON(NkScene &scene, NkStringView json, NkTextures2D *textures = nullptr,
								NkString *erreur = nullptr);
		bool NkChargerSceneJSON(NkScene &scene, NkStringView json, const NkRessourcesScene &ressources,
								NkString *erreur = nullptr);

		bool NkSauverSceneFichier(NkScene &scene, const char *chemin, const NkTextures2D *textures = nullptr);
		bool NkSauverSceneFichier(NkScene &scene, const char *chemin, const NkRessourcesScene &ressources);
		bool NkChargerSceneFichier(NkScene &scene, const char *chemin, NkTextures2D *textures = nullptr,
								   NkString *erreur = nullptr);
		bool NkChargerSceneFichier(NkScene &scene, const char *chemin, const NkRessourcesScene &ressources,
								   NkString *erreur = nullptr);

	} // namespace unkeny
} // namespace nkentseu
