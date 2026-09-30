//
// NkUnkenyHierarchie.cpp
// =============================================================================
// Description :
//   Les methodes de NkScene qui tiennent l'IDENTITE STABLE des entites et leur
//   HIERARCHIE (rattacher, detacher, propager le monde des enfants).
//
// Caracteristiques :
//   - La topologie passe par NKECS (ecs::NkSetParent et sa garde anti-boucle) :
//     rien n'est reecrit ici de ce que NKECS porte.
//   - Propagation par PROFONDEUR croissante : un parent est toujours a jour
//     quand on calcule ses enfants, quel que soit l'ordre des archetypes.
//   - « Qui gagne » : un monde ecrit par quelqu'un d'autre depuis la derniere
//     propagation (gizmo, TeleporterEntite, NkVitesse2D, physique) est un GESTE,
//     il est garde et le local suit ; sinon le monde suit le parent.
//
// Algorithmes implementes :
//   - Tri par insertion sur la profondeur (stable ; quelques centaines
//     d'enfants dans une scene d'editeur, et deja presque tries d'une trame a
//     l'autre)
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Unkeny/Scene/NkUnkenyScene.h"

#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace unkeny {

		// =====================================================================
		// Identite
		// =====================================================================
		uint64 NkScene::Uid(ecs::NkEntityId id) const noexcept {
			if (!mMonde.IsAlive(id)) {
				return 0u;
			}
			const NkIdentite2D *x = mMonde.Get<NkIdentite2D>(id);
			return x != nullptr ? x->uid : 0u;
		}

		uint64 NkScene::AssurerUid(ecs::NkEntityId id) {
			if (!mMonde.IsAlive(id)) {
				return 0u;
			}
			if (const NkIdentite2D *x = mMonde.Get<NkIdentite2D>(id)) {
				if (x->uid != 0u) {
					return x->uid;
				}
			}
			NkIdentite2D ident;
			ident.uid = mProchainUid++;
			mMonde.Add<NkIdentite2D>(id, ident);
			mCacheUid.Insert(ident.uid, id);
			return ident.uid;
		}

		void NkScene::RefaireCacheUid() {
			mCacheUid.Clear();
			mMonde.Query<NkIdentite2D>().ForEach([this](ecs::NkEntityId id, NkIdentite2D &x) {
				if (x.uid != 0u) {
					mCacheUid.Insert(x.uid, id);
				}
			});
		}

		ecs::NkEntityId NkScene::EntiteParUid(uint64 uid) {
			if (uid == 0u) {
				return ecs::NkEntityId::Invalid();
			}
			// Le cache n'est JAMAIS cru sur parole : l'entite doit vivre et porter
			// encore cet uid. Une entree perimee (entite detruite par Monde()
			// directement) fait refaire le cache une fois.
			for (int32 essai = 0; essai < 2; ++essai) {
				if (const ecs::NkEntityId *e = mCacheUid.Find(uid)) {
					if (Uid(*e) == uid) {
						return *e;
					}
				}
				if (essai == 0) {
					RefaireCacheUid();
				}
			}
			return ecs::NkEntityId::Invalid();
		}

		// =====================================================================
		// Hierarchie
		// =====================================================================
		ecs::NkEntityId NkScene::Parent(ecs::NkEntityId id) const noexcept {
			return ecs::NkGetParent(mMonde, id);
		}

		void NkScene::Enfants(ecs::NkEntityId id, NkVector<ecs::NkEntityId> &out) {
			ecs::NkCollectChildren(mMonde, id, out);
		}

		void NkScene::Descendants(ecs::NkEntityId id, NkVector<ecs::NkEntityId> &out) {
			ecs::NkCollectDescendants(mMonde, id, out);
		}

		const NkTransform2D *NkScene::Local(ecs::NkEntityId id) const noexcept {
			if (!Parent(id).IsValid()) {
				return nullptr;
			}
			const NkLocal2D *l = mMonde.Get<NkLocal2D>(id);
			return l != nullptr ? &l->local : nullptr;
		}

		bool NkScene::Rattacher(ecs::NkEntityId enfant, ecs::NkEntityId parent, bool garderMonde) {
			if (!mMonde.IsAlive(enfant) || !mMonde.IsAlive(parent) || !mMonde.Has<NkTransform2D>(enfant) ||
				!mMonde.Has<NkTransform2D>(parent)) {
				return false;
			}
			// La garde anti-boucle est celle de NKECS : soi-meme, ou un parent qui
			// descend de l'enfant.
			if (!ecs::NkSetParent(mMonde, enfant, parent)) {
				logger.Warn("[unkeny] Rattacher refuse : l'entite serait son propre ancetre");
				return false;
			}
			// Les deux doivent avoir une identite : c'est elle que le fichier ecrit.
			AssurerUid(enfant);
			AssurerUid(parent);
			const NkTransform2D p = *mMonde.Get<NkTransform2D>(parent);
			NkTransform2D &m = *mMonde.Get<NkTransform2D>(enfant);
			NkLocal2D l;
			l.parentProduit = p;
			if (garderMonde) {
				l.local = NkDecomposer2D(p, m);
				l.produit = m; // le monde n'a pas bouge : rien a recalculer
			} else {
				l.local = m;
				NkTransform2D nouveau = NkComposer2D(p, l.local);
				if (mMonde.Has<NkCorps2D>(enfant) && !MeneParPhysique(enfant)) {
					nouveau.rotation = PorterCorps(enfant, nouveau);
				}
				m = nouveau;
				l.produit = nouveau;
			}
			mMonde.Add<NkLocal2D>(enfant, l);
			// Les descendants de l'enfant suivent s'il a bouge.
			PropagerHierarchie();
			return true;
		}

		bool NkScene::Detacher(ecs::NkEntityId enfant, bool garderMonde) {
			if (!mMonde.IsAlive(enfant)) {
				return false;
			}
			if (!mMonde.Has<ecs::NkParent>(enfant)) {
				mMonde.Remove<NkLocal2D>(enfant);
				return true; // deja une racine
			}
			if (!garderMonde) {
				if (const NkLocal2D *l = mMonde.Get<NkLocal2D>(enfant)) {
					NkTransform2D nouveau = l->local;
					if (mMonde.Has<NkCorps2D>(enfant) && !MeneParPhysique(enfant)) {
						nouveau.rotation = PorterCorps(enfant, nouveau);
					}
					*mMonde.Get<NkTransform2D>(enfant) = nouveau;
				}
			}
			ecs::NkSetParent(mMonde, enfant, ecs::NkEntityId::Invalid());
			mMonde.Remove<NkLocal2D>(enfant);
			if (!garderMonde) {
				PropagerHierarchie();
			}
			return true;
		}

		bool NkScene::PoserLocal(ecs::NkEntityId enfant, const NkTransform2D &local) {
			const ecs::NkEntityId parent = Parent(enfant);
			NkLocal2D *l = mMonde.Get<NkLocal2D>(enfant);
			if (!parent.IsValid() || l == nullptr) {
				return false;
			}
			const NkTransform2D p = *mMonde.Get<NkTransform2D>(parent);
			NkTransform2D nouveau = NkComposer2D(p, local);
			if (mMonde.Has<NkCorps2D>(enfant) && !MeneParPhysique(enfant)) {
				nouveau.rotation = PorterCorps(enfant, nouveau);
			}
			// ⚠️ Relire le composant apres PorterCorps : aucune migration
			//    d'archetype n'a lieu ici, mais la regle est de ne pas garder de
			//    pointeur de composant a travers un appel qui pourrait en faire une.
			l = mMonde.Get<NkLocal2D>(enfant);
			l->local = local;
			l->produit = nouveau;
			l->parentProduit = p;
			*mMonde.Get<NkTransform2D>(enfant) = nouveau;
			PropagerHierarchie();
			return true;
		}

		void NkScene::DetruireAvecEnfants(ecs::NkEntityId id) {
			if (!mMonde.IsAlive(id)) {
				return;
			}
			NkVector<ecs::NkEntityId> tous;
			Descendants(id, tous);
			// Les feuilles d'abord : chaque Detruire trouve des enfants deja partis.
			for (uint32 i = static_cast<uint32>(tous.Size()); i > 0u; --i) {
				Detruire(tous[i - 1u]);
			}
			Detruire(id);
		}

		void NkScene::PropagerHierarchie() {
			struct NkEnfant {
					ecs::NkEntityId id;
					uint32 profondeur = 0;
			};
			NkVector<NkEnfant> enfants;
			NkVector<ecs::NkEntityId> orphelins;
			const ecs::NkWorld &monde = mMonde;
			mMonde.Query<ecs::NkParent>().ForEach([&](ecs::NkEntityId id, ecs::NkParent &) {
				if (!ecs::NkGetParent(monde, id).IsValid()) {
					orphelins.PushBack(id); // parent detruit sans passer par la scene
					return;
				}
				NkEnfant e;
				e.id = id;
				e.profondeur = ecs::NkHierarchyDepth(monde, id);
				enfants.PushBack(e);
			});
			// Hors de l'iteration : retirer un composant deplace les lignes.
			for (uint32 i = 0; i < orphelins.Size(); ++i) {
				mMonde.Remove<ecs::NkParent>(orphelins[i]);
				mMonde.Remove<NkLocal2D>(orphelins[i]);
			}
			if (enfants.Size() == 0u) {
				return;
			}
			for (uint32 i = 1; i < enfants.Size(); ++i) {
				const NkEnfant x = enfants[i];
				uint32 j = i;
				while (j > 0u && enfants[j - 1u].profondeur > x.profondeur) {
					enfants[j] = enfants[j - 1u];
					--j;
				}
				enfants[j] = x;
			}

			for (uint32 i = 0; i < enfants.Size(); ++i) {
				const ecs::NkEntityId id = enfants[i].id;
				const ecs::NkEntityId parent = ecs::NkGetParent(mMonde, id);
				const NkTransform2D *tp = mMonde.Get<NkTransform2D>(parent);
				NkTransform2D *tm = mMonde.Get<NkTransform2D>(id);
				if (tp == nullptr || tm == nullptr) {
					continue;
				}
				const NkTransform2D p = *tp;
				NkLocal2D *l = mMonde.Get<NkLocal2D>(id);
				if (l == nullptr) {
					// Un lien pose par NKECS directement : on part d'ou il est.
					NkLocal2D neuf;
					neuf.local = NkDecomposer2D(p, *tm);
					neuf.produit = *tm;
					neuf.parentProduit = p;
					mMonde.Add<NkLocal2D>(id, neuf);
					continue;
				}
				if (MeneParPhysique(id)) {
					// La physique mene : le local SUIT le monde, rien n'est ecrit.
					l->local = NkDecomposer2D(p, *tm);
					l->produit = *tm;
					l->parentProduit = p;
					continue;
				}
				const bool geste = !NkMemeTransform2D(*tm, l->produit);
				const bool parentBouge = !NkMemeTransform2D(p, l->parentProduit);
				if (geste) {
					// Le geste a ete fait dans le repere du parent TEL QU'IL ETAIT.
					l->local = NkDecomposer2D(l->parentProduit, *tm);
				}
				if (!parentBouge) {
					// Rien a porter : le monde reste EXACTEMENT celui du geste (le
					// recomposer y ajouterait l'arrondi de l'aller-retour).
					l->produit = *tm;
					continue;
				}
				NkTransform2D nouveau = NkComposer2D(p, l->local);
				if (mMonde.Has<NkCorps2D>(id)) {
					nouveau.rotation = PorterCorps(id, nouveau);
				}
				l = mMonde.Get<NkLocal2D>(id);
				tm = mMonde.Get<NkTransform2D>(id);
				*tm = nouveau;
				l->produit = nouveau;
				l->parentProduit = p;
			}
		}

	} // namespace unkeny
} // namespace nkentseu
