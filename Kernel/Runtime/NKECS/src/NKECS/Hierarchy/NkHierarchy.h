//
// NkHierarchy.h
// =============================================================================
// Description :
//   La hierarchie parent / enfant d'un NkWorld, reduite a sa TOPOLOGIE : qui est
//   l'enfant de qui. Aucune transformation ici — un moteur 2D et un moteur 3D ne
//   composent pas leurs repères de la meme facon, et chacun le fait chez lui
//   (Unkeny : NkUnkenyHierarchie ; Noge : NkSceneGraph).
//
// Caracteristiques :
//   - UNE seule verite : le lien `NkParent` porte par l'ENFANT. Les enfants d'une
//     entite se DEDUISENT (requete), ils ne sont stockes nulle part. Une liste
//     d'enfants tenue a cote du lien parent est une seconde copie de la meme
//     relation, et deux copies divergent au premier Destroy qui oublie l'une.
//   - Aucun plafond : ni nombre d'enfants, ni profondeur (la garde anti-boucle
//     s'arrete a kMaxHierarchyDepth pour qu'un lien corrompu ne fige pas le jeu).
//   - Copiable bit a bit : NkParent passe dans une photo d'etat sans code.
//   - Un lien vers une entite MORTE se lit comme « racine » (IsAlive, generation) :
//     detruire un parent par NkWorld::Destroy ne fait planter personne.
//
// ⚠️ D'OU VIENT NkParent — la regle « chercher qui porte deja »
//   Noge avait deja ce composant, a l'identique (`ecs::NkParent { entity }`,
//   Noge/ECS/Components/Core/NkTransform.h). Il DESCEND ici pour que tout moteur
//   sur NKECS partage le meme lien ; Noge l'inclut desormais depuis ce fichier,
//   sans changement de nom, de champ ni de disposition.
//   Son `NkChildren` (64 enfants au plus) reste chez Noge : c'est SON cache,
//   entretenu par NkSceneGraph::SetParent. Les fonctions ci-dessous ne le
//   connaissent pas — un monde Noge passe par NkSceneGraph, pas par elles.
//
// Algorithmes implementes :
//   - Garde anti-cycle par remontee de la chaine des parents (O(profondeur))
//   - Enfants et descendants par requete sur NkParent (O(n) par appel : le prix
//     de la verite unique, assume ; un moteur qui en a besoin a chaque trame
//     trie une fois par profondeur, cf. Unkeny)
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_NKECS_NKHIERARCHY_H__
#define __NKENTSEU_NKECS_NKHIERARCHY_H__

#include "NKECS/NkECSDefines.h"
#include "NKECS/World/NkWorld.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ecs {

		/// Profondeur au-dela de laquelle une chaine de parents est tenue pour
		/// corrompue (boucle ecrite a la main dans le composant). Aucune scene
		/// reelle n'en approche ; c'est un garde-fou, pas une limite de design.
		static constexpr uint32 kMaxHierarchyDepth = 1024u;

		// =====================================================================
		// NkParent — le lien, porte par l'ENFANT. Invalid() = racine.
		// =====================================================================
		struct NkParent {
				NkEntityId entity = NkEntityId::Invalid(); ///< Invalid() = racine
		};
		NK_COMPONENT(NkParent)

		/// Le parent VIVANT de `e`, ou Invalid() (racine, ou parent detruit).
		inline NkEntityId NkGetParent(const NkWorld &world, NkEntityId e) noexcept {
			const NkParent *p = world.Get<NkParent>(e);
			if (p == nullptr || !world.IsAlive(p->entity)) {
				return NkEntityId::Invalid();
			}
			return p->entity;
		}

		/// `e` est-il `ancestor`, ou l'un de ses descendants ? (e == ancestor : vrai)
		inline bool NkIsDescendantOf(const NkWorld &world, NkEntityId e, NkEntityId ancestor) noexcept {
			NkEntityId cur = e;
			for (uint32 d = 0; d <= kMaxHierarchyDepth && world.IsAlive(cur); ++d) {
				if (cur == ancestor) {
					return true;
				}
				cur = NkGetParent(world, cur);
			}
			return false;
		}

		/// Nombre de parents vivants au-dessus de `e` (0 = racine).
		inline uint32 NkHierarchyDepth(const NkWorld &world, NkEntityId e) noexcept {
			uint32 d = 0;
			NkEntityId cur = NkGetParent(world, e);
			while (cur.IsValid() && d < kMaxHierarchyDepth) {
				++d;
				cur = NkGetParent(world, cur);
			}
			return d;
		}

		/// Rattache `child` a `parent`. `parent` Invalid() = detacher (racine).
		/// Refuse — rend false sans rien changer — une entite morte, un lien de
		/// soi a soi, et tout lien qui fermerait une boucle (parent descendant
		/// de child). C'est la garde que NkSceneGraph::SetParent n'avait pas.
		inline bool NkSetParent(NkWorld &world, NkEntityId child, NkEntityId parent) noexcept {
			if (!world.IsAlive(child)) {
				return false;
			}
			if (!parent.IsValid()) {
				if (world.Has<NkParent>(child)) {
					world.Remove<NkParent>(child);
				}
				return true;
			}
			if (!world.IsAlive(parent) || parent == child || NkIsDescendantOf(world, parent, child)) {
				return false;
			}
			NkParent lien;
			lien.entity = parent;
			world.Add<NkParent>(child, lien);
			return true;
		}

		/// Les enfants DIRECTS vivants de `parent`, dans l'ordre de la requete.
		inline void NkCollectChildren(NkWorld &world, NkEntityId parent, NkVector<NkEntityId> &out) noexcept {
			out.Clear();
			world.Query<NkParent>().ForEach([&](NkEntityId id, NkParent &p) {
				if (p.entity == parent) {
					out.PushBack(id);
				}
			});
		}

		/// Tous les descendants vivants de `root` (sans lui), parents AVANT
		/// enfants : l'ordre qu'il faut pour propager une transformation.
		inline void NkCollectDescendants(NkWorld &world, NkEntityId root, NkVector<NkEntityId> &out) noexcept {
			out.Clear();
			NkVector<NkEntityId> niveau;
			NkCollectChildren(world, root, niveau);
			NkVector<NkEntityId> suivant;
			NkVector<NkEntityId> enfants;
			for (uint32 d = 0; d < kMaxHierarchyDepth && niveau.Size() > 0u; ++d) {
				suivant.Clear();
				for (nk_usize i = 0; i < niveau.Size(); ++i) {
					out.PushBack(niveau[i]);
					NkCollectChildren(world, niveau[i], enfants);
					for (nk_usize k = 0; k < enfants.Size(); ++k) {
						suivant.PushBack(enfants[k]);
					}
				}
				niveau = suivant;
			}
		}

		/// Detruit `root` ET toute sa descendance, feuilles d'abord.
		/// ⚠️ Pas pendant une iteration de Query : NkWorld::Destroy deplace les
		///    lignes d'archetype (meme regle que Destroy).
		inline void NkDestroyRecursive(NkWorld &world, NkEntityId root) noexcept {
			if (!world.IsAlive(root)) {
				return;
			}
			NkVector<NkEntityId> tous;
			NkCollectDescendants(world, root, tous);
			for (nk_usize i = tous.Size(); i > 0u; --i) {
				world.Destroy(tous[i - 1u]);
			}
			world.Destroy(root);
		}

	} // namespace ecs
} // namespace nkentseu

#endif // __NKENTSEU_NKECS_NKHIERARCHY_H__
