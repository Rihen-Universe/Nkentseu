//
// NkArWorld.cpp
// =============================================================================
// Description :
//   Localisation de la caméra dans le monde depuis un marqueur connu, et
//   extension de la carte par les marqueurs vus en même temps.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#include "NKXR/AR/NkArWorld.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace xr {

		void NkArWorld::Reset() {
			mMap.Clear();
			mAnchors.Clear();
			mCameraInWorld = NkXrPose::Identity();
			mHasOrigin = false;
			mLocalizedThisFrame = false;
			mFramesSinceLocalized = 0;
			mNextHandle = 1;
		}

		const NkArMapEntry *NkArWorld::FindMapEntry(int32 id) const {
			for (nk_size i = 0; i < mMap.Size(); ++i) {
				if (mMap[i].id == id) {
					return &mMap[i];
				}
			}
			return nullptr;
		}

		bool NkArWorld::Update(const NkArSession &session) {
			mLocalizedThisFrame = false;
			const NkVector<NkArTrackedMarker> &tracked = session.GetTracked();

			// ── 1) Origine : le PREMIER marqueur vu définit le monde ─────────
			// Choix assumé : il devient l'origine, sa pose dans le monde est
			// l'identité. Tout le reste s'exprime par rapport à lui — c'est ce
			// qui rend le repère STABLE et indépendant de la caméra.
			if (!mHasOrigin) {
				for (nk_size i = 0; i < tracked.Size(); ++i) {
					if (!tracked[i].visibleThisFrame) {
						continue;
					}
					NkArMapEntry origin;
					origin.id = tracked[i].id;
					origin.poseInWorld = NkXrPose::Identity();
					origin.isOrigin = true;
					origin.observations = 1;
					mMap.PushBack(origin);
					mHasOrigin = true;
					logger.Infof("[NkArWorld] Origine du monde posée sur le marqueur %d.\n", origin.id);
					break;
				}
			}
			if (!mHasOrigin) {
				++mFramesSinceLocalized;
				return false;
			}

			// ── 2) Localiser la CAMÉRA depuis un marqueur connu ──────────────
			// La détection donne T_camera_marqueur (le marqueur dans le repère
			// de l'objectif). On veut l'inverse du raisonnement :
			//   T_monde_camera = T_monde_marqueur * inverse(T_camera_marqueur)
			// Le plus GRAND marqueur à l'image est retenu : plus il occupe de
			// pixels, plus sa pose est précise — un marqueur lointain
            // entraînerait tout le monde dans son erreur.
			const NkArTrackedMarker *best = nullptr;
			const NkArMapEntry *bestEntry = nullptr;
			for (nk_size i = 0; i < tracked.Size(); ++i) {
				if (!tracked[i].visibleThisFrame) {
					continue;
				}
				const NkArMapEntry *entry = FindMapEntry(tracked[i].id);
				if (entry == nullptr) {
					continue;
				}
				if (best == nullptr || tracked[i].pose.position.LenSq() < best->pose.position.LenSq()) {
					best = &tracked[i];
					bestEntry = entry;
				}
			}
			if (best != nullptr) {
				mCameraInWorld = bestEntry->poseInWorld * best->pose.Inverted();
				mLocalizedThisFrame = true;
				mFramesSinceLocalized = 0;
			}
			else {
				++mFramesSinceLocalized;
			}

			// ── 3) Étendre la carte ──────────────────────────────────────────
			// Un marqueur INCONNU vu alors que la caméra est localisée entre
			// dans la carte : T_monde_nouveau = T_monde_camera * T_camera_nouveau.
			// De proche en proche, la pièce devient repérable — c'est ce qui
			// permet de se déplacer d'un marqueur à l'autre sans perdre la scène.
			if (mConfig.extendMap && mLocalizedThisFrame) {
				for (nk_size i = 0; i < tracked.Size(); ++i) {
					const NkArTrackedMarker &marker = tracked[i];
					if (!marker.visibleThisFrame || FindMapEntry(marker.id) != nullptr) {
						continue;
					}
					// Trop petit à l'image = pose imprécise : l'inscrire
					// propagerait l'erreur à tout ce qui sera posé ensuite.
					if (marker.pose.position.Len() > 0.f && marker.framesTracked < 3u) {
						continue; // laisser le lissage se stabiliser
					}
					NkArMapEntry entry;
					entry.id = marker.id;
					entry.poseInWorld = mCameraInWorld * marker.pose;
					entry.observations = 1;
					mMap.PushBack(entry);
					logger.Infof("[NkArWorld] Marqueur %d ajouté à la carte (%u connus).\n", entry.id,
								 uint32(mMap.Size()));
				}
			}
			return mLocalizedThisFrame;
		}

		uint32 NkArWorld::Place(const NkXrPose &poseInWorld) {
			NkArAnchor anchor;
			anchor.handle = mNextHandle;
			++mNextHandle;
			anchor.poseInWorld = poseInWorld;
			mAnchors.PushBack(anchor);
			return anchor.handle;
		}

		uint32 NkArWorld::PlaceInFrontOfCamera(float32 metersAhead) {
			if (!mHasOrigin) {
				return 0; // sans monde, « devant » ne veut rien dire
			}
			// « Devant » = -Z dans le repère caméra, transporté dans le monde.
			NkXrPose local;
			local.position = NkVec3f(0.f, 0.f, -metersAhead);
			return Place(mCameraInWorld * local);
		}

		bool NkArWorld::Remove(uint32 handle) {
			for (nk_size i = 0; i < mAnchors.Size(); ++i) {
				if (mAnchors[i].handle == handle) {
					mAnchors[i] = mAnchors[mAnchors.Size() - 1u];
					mAnchors.PopBack();
					return true;
				}
			}
			return false;
		}

		void NkArWorld::RemoveAll() {
			mAnchors.Clear();
		}

		bool NkArWorld::ToCamera(const NkXrPose &poseInWorld, NkXrPose &outPose) const {
			if (!mHasOrigin) {
				return false;
			}
			// Le passage inverse : du monde vers l'objectif. La pose de caméra
			// utilisée peut dater (aucun marqueur en vue) — c'est délibéré, et
			// c'est à l'appelant de le DIRE à l'utilisateur via IsLocalizedNow().
			outPose = mCameraInWorld.Inverted() * poseInWorld;
			return true;
		}

		bool NkArWorld::GetAnchorInCamera(uint32 handle, NkXrPose &outPose) const {
			for (nk_size i = 0; i < mAnchors.Size(); ++i) {
				if (mAnchors[i].handle == handle) {
					return ToCamera(mAnchors[i].poseInWorld, outPose);
				}
			}
			return false;
		}

	} // namespace xr
} // namespace nkentseu
