//
// NkArImu.cpp
// =============================================================================
// Description :
//   Accès aux capteurs de rotation. Android via ASensorManager ; ailleurs, un
//   refus honnête.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#include "NKXR/AR/NkArImu.h"
#include "NKLogger/NkLog.h"

#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__)
	#include <android/sensor.h>
#endif

namespace nkentseu {
	namespace xr {

#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__)

		namespace {
			// Cadence demandée : 200 Hz. Bien plus que les 30 images par seconde
			// de la caméra, et c'est voulu — c'est précisément ce qui permet de
			// suivre un geste vif que l'image, elle, verrait flou.
			constexpr int32 kSamplingPeriodUs = 5000;
			constexpr int32 kLooperId = 0x4E4B; // « NK »
		} // namespace

		bool NkArImu::Initialize() {
			// ASensorManager_getInstance et non ...ForPackage : cette dernière
			// n'existe qu'à partir d'Android 8, alors que l'application vise
			// Android 7. Elle est marquée obsolète mais reste fonctionnelle sur
			// toutes les versions — préférer une fonction qui marche partout à
			// une plus récente qui exclut des appareils.
			ASensorManager *manager = ASensorManager_getInstance();
			if (manager == nullptr) {
				logger.Warn("[NkArImu] Gestionnaire de capteurs indisponible.\n");
				return false;
			}
			const ASensor *gyro = ASensorManager_getDefaultSensor(manager, ASENSOR_TYPE_GYROSCOPE);
			const ASensor *accel = ASensorManager_getDefaultSensor(manager, ASENSOR_TYPE_ACCELEROMETER);
			if (gyro == nullptr && accel == nullptr) {
				logger.Warn("[NkArImu] Ni gyroscope ni accelerometre sur cet appareil.\n");
				return false;
			}
			ALooper *looper = ALooper_forThread();
			if (looper == nullptr) {
				looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
			}
			ASensorEventQueue *queue = ASensorManager_createEventQueue(manager, looper, kLooperId, nullptr, nullptr);
			if (queue == nullptr) {
				logger.Warn("[NkArImu] Creation de la file d'evenements refusee.\n");
				return false;
			}
			if (gyro != nullptr) {
				ASensorEventQueue_enableSensor(queue, gyro);
				ASensorEventQueue_setEventRate(queue, gyro, kSamplingPeriodUs);
			}
			if (accel != nullptr) {
				ASensorEventQueue_enableSensor(queue, accel);
				ASensorEventQueue_setEventRate(queue, accel, kSamplingPeriodUs);
			}
			mManager = manager;
			mQueue = queue;
			mGyro = gyro;
			mAccel = accel;
			mLastTimestamp = 0;
			mAvailable = true;
			logger.Infof("[NkArImu] Capteurs prets : gyroscope=%s accelerometre=%s\n", gyro ? "oui" : "non",
						 accel ? "oui" : "non");
			return true;
		}

		void NkArImu::Shutdown() {
			if (mQueue != nullptr && mManager != nullptr) {
				ASensorEventQueue *queue = static_cast<ASensorEventQueue *>(mQueue);
				if (mGyro != nullptr) {
					ASensorEventQueue_disableSensor(queue, static_cast<const ASensor *>(mGyro));
				}
				if (mAccel != nullptr) {
					ASensorEventQueue_disableSensor(queue, static_cast<const ASensor *>(mAccel));
				}
				ASensorManager_destroyEventQueue(static_cast<ASensorManager *>(mManager), queue);
			}
			mManager = nullptr;
			mQueue = nullptr;
			mGyro = nullptr;
			mAccel = nullptr;
			mAvailable = false;
		}

		NkArImuSample NkArImu::Poll() {
			NkArImuSample out;
			if (!mAvailable || mQueue == nullptr) {
				return out;
			}
			ASensorEventQueue *queue = static_cast<ASensorEventQueue *>(mQueue);
			ASensorEvent event;
			NkVec3f angle(0.f, 0.f, 0.f);
			while (ASensorEventQueue_getEvents(queue, &event, 1) > 0) {
				if (event.type == ASENSOR_TYPE_GYROSCOPE) {
					// Le gyroscope donne une VITESSE angulaire en rad/s ; c'est
					// l'intervalle entre deux mesures qui la transforme en angle.
					// L'horodatage vient du capteur lui-même, en nanosecondes :
					// s'en remettre à l'horloge de l'application introduirait la
					// gigue de la boucle de rendu dans une mesure qui n'en a pas.
					if (mLastTimestamp != 0) {
						const float32 dt = float32(double(event.timestamp - mLastTimestamp) * 1e-9);
						// Un intervalle absurde (mise en veille, capteur relancé)
						// ne doit pas se traduire par un bond de rotation.
						if (dt > 0.f && dt < 0.2f) {
							angle.x += event.vector.x * dt;
							angle.y += event.vector.y * dt;
							angle.z += event.vector.z * dt;
							++out.samples;
						}
					}
					mLastTimestamp = event.timestamp;
				}
				else if (event.type == ASENSOR_TYPE_ACCELEROMETER) {
					// À l'arrêt, l'accéléromètre ne mesure que la pesanteur —
					// donc la verticale, gratuitement. En mouvement il mesure
					// aussi le geste : c'est pourquoi cette valeur sert à
					// s'orienter, jamais à se déplacer.
					NkVec3f g(event.vector.x, event.vector.y, event.vector.z);
					const float32 len = g.Len();
					if (len > 0.001f) {
						out.gravity = g * (-1.f / len);
						out.hasGravity = true;
					}
				}
			}
			if (out.samples > 0) {
				// Petits angles : composer trois rotations d'axes séparés est
				// exact au second ordre près, et c'est très en deçà du bruit du
				// capteur sur cinq millisecondes.
				out.deltaRotation = NkQuatf::RotateZ(math::NkAngle::FromRad(angle.z)) *
									NkQuatf::RotateY(math::NkAngle::FromRad(angle.y)) *
									NkQuatf::RotateX(math::NkAngle::FromRad(angle.x));
				out.deltaRotation = out.deltaRotation.Normalized();
				out.valid = true;
			}
			else if (out.hasGravity) {
				out.valid = true;
			}
			return out;
		}

#else

		// Poste de travail : pas de capteurs. On le DIT plutôt que de rendre des
		// zéros, qu'un appelant confondrait avec « immobile ».
		bool NkArImu::Initialize() {
			mAvailable = false;
			return false;
		}

		void NkArImu::Shutdown() {}

		NkArImuSample NkArImu::Poll() {
			return NkArImuSample{};
		}

#endif

	} // namespace xr
} // namespace nkentseu
