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
//       "format": "unkeny.scene", "version": 1,
//       "config":   { physique, particules, gravite, pasFixe, pasMaxParTrame },
//       "camera":   "cx cy zoom rotation",
//       "entites":  [ { nom, transform, sprite{texture}, collisionneur,
//                       corps{etat}, mou, lumiere, emetteur,
//                       jeu{ NomComposant: "octets hex" } } ],
//       "particules": { reglages, corps[], p{...}, l{...} },
//       "eclairage":  { actif, ambiante, ombres, masqueOcculteurs, mode, maille }
//     }
//
//   ⚠️ « lumiere », « emetteur » et « eclairage » (2026-09-30) ne sont ECRITS
//   que s'ils existent (l'eclairage : s'il differe du defaut). Un fichier
//   d'avant se relit tel quel, et se reecrit a l'octet pres. La version reste
//   1 : un moteur plus ancien lit un fichier plus recent en ignorant ces cles
//   (ses lumieres disparaissent, rien ne casse).
//
// CE QUI EST SAUVE
//   - les composants d'Unkeny, un par un, champ par champ (lumieres et
//     emetteurs compris, 2026-09-30) ;
//   - l'ETAT des corps rigides (position, orientation, vitesses) ;
//   - tout le monde de particules (corps, particules, liens, reglages) ;
//   - les composants declares par NkScene::PhotographierAussi<T>("Nom") —
//     ceux du jeu comme NkAnimSprite2D — en octets. Les noms sont la cle :
//     un composant inconnu a la relecture est ignore, pas une erreur.
//
// ⚠️ CE QUI NE L'EST PAS, et pourquoi
//   - Les identifiants d'ENTITE. Ils sont refaits au chargement (comme par
//     Restaurer) ; un composant du jeu qui en stocke un pointera faux. Ceux
//     des CORPS MOUS, eux, sont stables et relies de nouveau.
//   - Les PIXELS des textures : on ecrit le NOM de la texture (NkTextures2D),
//     et on la retrouve au chargement par ce nom — deja enregistree, ou lue
//     depuis le disque. Sans NkTextures2D, le sprite revient sans image.
//   - Le generateur pseudo-aleatoire interne des particules (agitation
//     thermique) : une simulation rechargee est la meme, pas la meme au bit
//     pres dans ses trames suivantes.
//   - Les octets des composants "jeu" dependent de la plateforme (boutisme,
//     alignement) : un .nkscene se relit sur la meme famille de machines. Les
//     composants d'Unkeny, eux, sont ecrits champ par champ et voyagent.
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

		/// Version du format ecrit. Une lecture accepte toute version <= celle-ci.
		constexpr int32 NK_UNKENY_SCENE_VERSION = 1;

		bool NkSauverScene(NkScene &scene, NkArchive &sortie, const NkTextures2D *textures = nullptr);

		/// Remplace TOUTE la scene par celle de l'archive (la scene est
		/// re-initialisee avec la configuration du fichier). En cas d'echec, la
		/// scene est laissee intacte et `erreur` dit pourquoi.
		bool NkChargerScene(NkScene &scene, const NkArchive &entree, NkTextures2D *textures = nullptr,
							NkString *erreur = nullptr);

		bool NkSauverSceneJSON(NkScene &scene, NkString &json, const NkTextures2D *textures = nullptr);
		bool NkChargerSceneJSON(NkScene &scene, NkStringView json, NkTextures2D *textures = nullptr,
								NkString *erreur = nullptr);

		bool NkSauverSceneFichier(NkScene &scene, const char *chemin, const NkTextures2D *textures = nullptr);
		bool NkChargerSceneFichier(NkScene &scene, const char *chemin, NkTextures2D *textures = nullptr,
								   NkString *erreur = nullptr);

	} // namespace unkeny
} // namespace nkentseu
