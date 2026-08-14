//
// NkArImu.h
// =============================================================================
// Description :
//   La centrale inertielle : mesurer la rotation de l'appareil par ses CAPTEURS
//   plutôt que par l'image.
//
// Pourquoi ce fichier existe :
//   Tout le travail fait sur `NkArFlow` — retrouver la rotation en comparant
//   deux images — n'existe que parce qu'une webcam de bureau n'a pas de
//   capteurs. Un téléphone, lui, porte un gyroscope : il donne la rotation
//   directement, à deux cents mesures par seconde, sans avoir besoin de la
//   moindre texture, et sans se laisser tromper par quelqu'un qui traverse le
//   champ. Sur téléphone, mesurer vaut mieux que deviner.
//
// Ce que ce module apporte, et ce qu'il n'apporte pas :
//   - il donne la ROTATION, et elle est excellente ;
//   - il ne donne PAS la translation. L'accéléromètre mesure une accélération,
//     dont il faudrait intégrer deux fois le signal pour obtenir une position :
//     le moindre biais devient alors des mètres d'erreur en quelques secondes.
//     Marcher restera donc non suivi tant qu'il n'y aura pas de SLAM.
//   - il donne en revanche la VERTICALE (la pesanteur mesurée à l'arrêt), qui
//     est un cadeau : elle donne gratuitement la normale du sol, dont la
//     détection de plans aura besoin.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_XR_NKARIMU_H__
#define __NKENTSEU_XR_NKARIMU_H__

#include "NKXR/NkXrPose.h"

namespace nkentseu {
	namespace xr {

		struct NkArImuSample {
			bool valid = false;
			/// Rotation accumulée depuis la dernière lecture, dans le repère de
			/// l'appareil. C'est un INCRÉMENT : l'appelant le compose, il ne le
			/// remplace pas.
			NkQuatf deltaRotation{};
			/// Direction de la pesanteur dans le repère de l'appareil, normalisée.
			/// Utile telle quelle : elle dit où est le bas, donc l'inclinaison
			/// réelle de l'appareil, sans aucune image.
			NkVec3f gravity{ 0.f, -1.f, 0.f };
			bool hasGravity = false;
			uint32 samples = 0; ///< Mesures consommées depuis le dernier appel.
		};

		// ── La centrale inertielle ───────────────────────────────────────────
		// Implémentée sur Android (ASensorManager) ; ailleurs, elle rend
		// poliment « pas disponible » — une application peut donc l'appeler
		// partout sans se garder elle-même.
		class NkArImu {
			public:
				bool Initialize();
				void Shutdown();
				bool IsAvailable() const { return mAvailable; }

				/// Consomme les mesures arrivées depuis l'appel précédent et rend
				/// la rotation cumulée sur cet intervalle. À appeler une fois par
				/// image : les mesures arrivent beaucoup plus vite que les images,
				/// et les cumuler ici évite d'en perdre.
				NkArImuSample Poll();

			private:
				void *mManager = nullptr;
				void *mQueue = nullptr;
				const void *mGyro = nullptr;
				const void *mAccel = nullptr;
				int64 mLastTimestamp = 0;
				bool mAvailable = false;
		};

	} // namespace xr
} // namespace nkentseu

#endif // __NKENTSEU_XR_NKARIMU_H__
