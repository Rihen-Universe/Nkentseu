// -----------------------------------------------------------------------------
// FICHIER: Unkeny/Scene/NkUnkenyActif.cpp
// DESCRIPTION: L'activite des entites (NkUnkenyActif.h) : allumer, eteindre,
//              et mettre le solveur et la matiere en accord.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Unkeny/Scene/NkUnkenyScene.h"

namespace nkentseu {
	namespace unkeny {

		bool NkScene::Activer(ecs::NkEntityId id, bool actif) {
			if (!mMonde.IsAlive(id)) {
				return false;
			}
			// Le composant est GARDE rallume (actif = vrai) : une instance de
			// prefab qui rallume ce que son prefab eteint le dit explicitement.
			NkActif2D a;
			a.actif = actif;
			if (mMonde.Has<NkActif2D>(id)) {
				mMonde.Set<NkActif2D>(id, a);
			} else {
				mMonde.Add<NkActif2D>(id, a);
			}
			AppliquerActivite();
			return true;
		}

		void NkScene::AppliquerActivite() {
			// ── Les corps RIGIDES : un corps eteint SORT du solveur (il n'y touche
			//    plus rien, rien ne le touche), un corps rallume y RENTRE, immobile,
			//    la ou est son transform. ⚠️ Releve PUIS agit : ajouter ou changer un
			//    composant pendant une requete la rendrait invalide.
			if (mPhysique != nullptr) {
				NkVector<ecs::NkEntityId> aEteindre;
				NkVector<ecs::NkEntityId> aRallumer;
				mMonde.Query<NkCorps2D>().ForEach([&](ecs::NkEntityId id, NkCorps2D &c) {
					const bool actif = EstActive(id);
					if (!actif && c.corpsId != physics::NK_INVALID_BODY) {
						aEteindre.PushBack(id);
					} else if (actif && c.corpsId == physics::NK_INVALID_BODY && mMonde.Has<NkCollisionneur2D>(id)) {
						aRallumer.PushBack(id);
					}
				});
				for (uint32 i = 0; i < aEteindre.Size(); ++i) {
					NkCorps2D *c = mMonde.Get<NkCorps2D>(aEteindre[i]);
					if (c == nullptr) {
						continue;
					}
					mCorpsEteints.Insert(aEteindre[i].Pack(), c->corpsId);
					mPhysique->DestroyBody(c->corpsId);
					c->corpsId = physics::NK_INVALID_BODY;
				}
				for (uint32 i = 0; i < aRallumer.Size(); ++i) {
					const ecs::NkEntityId id = aRallumer[i];
					const NkCorps2D *c = mMonde.Get<NkCorps2D>(id);
					if (c == nullptr) {
						continue;
					}
					NkCorps2D copie = *c;
					copie.corpsId = physics::NK_INVALID_BODY;
					if (!AjouterCorps(id, copie)) {
						continue;
					}
					// Les attaches des particules designaient l'ANCIEN corps : elles
					// suivent le neuf (meme geste que RefaireEntites).
					const physics::NkBodyId *ancien = mCorpsEteints.Find(id.Pack());
					const NkCorps2D *nc = mMonde.Get<NkCorps2D>(id);
					if (ancien != nullptr && nc != nullptr && mParticules != nullptr && *ancien != physics::NK_INVALID_BODY) {
						const physics::NkBodyId a = *ancien;
						const physics::NkBodyId n = nc->corpsId;
						mParticules->RemapperRigides(&a, &n, 1u);
					}
					mCorpsEteints.Erase(id.Pack());
				}
			}
			// ── La MATIERE : un corps mou eteint est gele sur place (NKPhysics,
			//    NkCorpsP2D::actif) -- sa matiere n'est pas detruite.
			if (mParticules != nullptr) {
				physics::NkParticules2D *p = mParticules;
				mMonde.Query<NkCorpsMou2D>().ForEach([&](ecs::NkEntityId id, NkCorpsMou2D &m) {
					const int32 ci = p->IndexCorps(m.corpsId);
					if (ci >= 0) {
						p->corps[static_cast<uint32>(ci)].actif = EstActive(id);
					}
				});
			}
		}

	} // namespace unkeny
} // namespace nkentseu
