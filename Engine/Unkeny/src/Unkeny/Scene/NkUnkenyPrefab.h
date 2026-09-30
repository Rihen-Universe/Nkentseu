//
// NkUnkenyPrefab.h
// =============================================================================
// Description :
//   Les PREFABS d'Unkeny : un assemblage d'entites (une racine et toute sa
//   descendance) qu'on enregistre (.nkprefab), qu'on pose autant de fois qu'on
//   veut, et dont chaque copie — une INSTANCE — suit les changements du modele
//   tout en gardant ce qu'on y a change a la main (ses SURCHARGES).
//
// Caracteristiques :
//   - Un prefab est une liste de NOEUDS, chacun la photo d'une entite
//     (NkScene::NkPhotoEntite) ; le noeud a un NUMERO stable d'une version du
//     prefab a l'autre, c'est ce qui relie un noeud d'instance a son modele.
//   - Une instance porte NkInstancePrefab2D sur chacune de ses entites : quel
//     prefab, quel noeud, quelle racine d'instance. Il est photographie et
//     sauve (le prefab par son NOM, la racine par son identite).
//   - LES SURCHARGES NE SE DECLARENT PAS : elles se VOIENT. A la mise a jour,
//     chaque champ est compare a l'ANCIENNE version du modele : identique, il
//     prend la nouvelle valeur ; different, c'est que quelqu'un l'a change sur
//     cette instance, et il est garde. Rien a marquer dans l'inspecteur, rien a
//     oublier de marquer.
//   - Champ par champ pour tout ce qui est decrit (NkUnkenyChamps.h : sprite,
//     collisionneur, corps, nom, local, et les composants du jeu declares avec
//     leurs champs) ; composant entier pour un composant du jeu non decrit.
//
// ⚠️ CE QUI N'EST PAS DANS UN PREFAB, et pourquoi
//   - La MATIERE d'un corps mou (ses particules) : elle vit dans le monde de
//     particules, pas dans un composant — meme regle que « Dupliquer ».
//   - La position et la rotation de la RACINE d'une instance : c'est la ou on
//     l'a posee. Son echelle, elle, suit le modele.
//   - Un prefab dans un prefab : un noeud qui etait lui-meme une instance
//     d'un autre prefab entre dans le nouveau comme de simples entites.
//
// ⚠️ POURQUOI PAS LE NkPrefab DE NOGE (la regle « chercher qui porte deja »)
//   Noge a un NkPrefab (Noge/ECS/Prefab) : 3D (NkTransform, NkSceneComponent),
//   lie a NkGameObject, et Unkeny ne depend pas de Noge (NKCanvas et NKRenderer
//   sont exclusifs). Ses surcharges sont un TODO (SetOverride n'ecrit rien).
//   Celui-ci reprend sa forme (modele / instance / registre) et comble ce qui
//   manquait : l'instanciation des composants, les surcharges, la propagation.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYPREFAB_H__
#define __NKENTSEU_UNKENY_NKUNKENYPREFAB_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

namespace nkentseu {
	namespace unkeny {

		struct NkRessourcesScene;

		/// Porte par chaque entite d'une instance. DONNEES SEULEMENT. Declare a la
		/// photo par NkScene::Init, avec ses champs : une scene relue retrouve ses
		/// instances meme si aucun prefab n'a encore ete touche.
		struct NkInstancePrefab2D {
				uint32 prefab = 0;			///< identifiant NkPrefabs2D (ecrit par NOM)
				uint32 noeud = 0;			///< numero du noeud dans le prefab
				ecs::NkEntityId racine;		///< la racine de CETTE instance (ecrite par identite)
		};

		/// La place d'un composant photographie dans NkPhotoEntite::extra, au
		/// moment ou le prefab a ete fait : c'est ce qui permet de le poser dans
		/// une scene qui a declare ses composants dans un autre ordre.
		struct NkDispositionPrefab {
				NkString nom; ///< vide : composant sans nom (apparie par rang et taille)
				uint32 taille = 0;
		};

		/// Un modele.
		struct NkPrefab2D {
				NkString nom;		   ///< la cle ; le chemin du .nkprefab pour un prefab enregistre
				uint32 revision = 0;   ///< +1 a chaque mise a jour
				uint32 prochainNoeud = 1;
				/// [0] = la racine. `uid` = numero de noeud, `parentUid` = noeud parent
				/// (0 pour la racine), `local` = place dans le parent. Parents AVANT
				/// enfants. Les champs NK_ENTITE y portent des NUMEROS DE NOEUD.
				NkVector<NkScene::NkPhotoEntite> noeuds;
				NkVector<NkDispositionPrefab> disposition;
		};

		/// Le registre des prefabs d'une session : nom <-> identifiant, comme
		/// NkTextures2D et NkSons2D (« meme nom, meme identifiant »).
		class NkPrefabs2D {
			public:
				/// Fait un prefab de `racine` et de toute sa descendance, sous `nom`.
				/// Un nom deja pris est REMPLACE comme par MettreAJour (les instances
				/// suivent). Rend l'identifiant (jamais 0), ou 0 si `racine` est morte.
				uint32 Creer(NkScene &scene, ecs::NkEntityId racine, const char *nom);

				/// Pose une instance, sa racine a `position`. Rend la racine.
				ecs::NkEntityId Instancier(NkScene &scene, uint32 prefab, const NkVec2f &position);

				/// La nouvelle version du modele est `source` (une instance qu'on a
				/// retouchee, ou n'importe quelle entite) ; TOUTES les instances de
				/// `scene` suivent, leurs surcharges gardees. Rend le nombre
				/// d'instances mises a jour.
				uint32 MettreAJour(NkScene &scene, uint32 prefab, ecs::NkEntityId source);

				/// Les champs de `entite` (un noeud d'instance) qui different du
				/// modele actuel : ses surcharges, « Composant.champ ».
				void Surcharges(NkScene &scene, ecs::NkEntityId entite, NkVector<NkString> &out);

				/// Les racines des instances de `prefab` dans `scene`.
				void Instances(NkScene &scene, uint32 prefab, NkVector<ecs::NkEntityId> &out);

				/// Ecrit le prefab dans `chemin` (.nkprefab, JSON « unkeny.prefab »).
				/// `scene` : celle dont les composants ont ete declares (leurs noms).
				bool Enregistrer(NkScene &scene, uint32 prefab, const char *chemin, const NkRessourcesScene &ressources);
				/// Lit un .nkprefab. Son NOM est `chemin`. Un prefab deja connu sous
				/// ce nom est REMPLACE, et les instances de `scene` suivent : c'est
				/// « le fichier a change, les niveaux se mettent a jour ».
				uint32 Charger(NkScene &scene, const char *chemin, const NkRessourcesScene &ressources,
							   NkString *erreur = nullptr);
				bool ChargerJSON(NkScene &scene, const char *nom, NkStringView json, const NkRessourcesScene &ressources,
								 uint32 &id, NkString *erreur = nullptr);
				bool EnregistrerJSON(NkScene &scene, uint32 prefab, NkString &json, const NkRessourcesScene &ressources);

				const NkPrefab2D *Prefab(uint32 id) const noexcept {
					return id != 0u && id <= mPrefabs.Size() ? &mPrefabs[id - 1u] : nullptr;
				}
				uint32 Trouver(const char *nom) const noexcept;
				const char *Nom(uint32 id) const noexcept {
					const NkPrefab2D *p = Prefab(id);
					return p != nullptr ? p->nom.CStr() : "";
				}
				uint32 Nombre() const noexcept {
					return static_cast<uint32>(mPrefabs.Size());
				}

			private:
				/// La photo de `racine` et de sa descendance, en noeuds. Chaque entite
				/// capturee DEVIENT un noeud d'instance de `idPrefab` (la source d'un
				/// prefab en est la premiere instance).
				bool Capturer(NkScene &scene, ecs::NkEntityId racine, uint32 idPrefab, const NkPrefab2D *ancien,
							  NkPrefab2D &out);
				uint32 Propager(NkScene &scene, uint32 prefab, const NkPrefab2D &ancien);
				NkVector<NkPrefab2D> mPrefabs;
		};

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYPREFAB_H__
