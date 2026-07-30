#pragma once
// =============================================================================
// Noge/ECS/Systems/NkAudioSystem.h — pont ECS -> NKAudio (moteur audio AAA)
// =============================================================================
// Modèle identique à NkPhysicsSystem/NkParticleSystem : adaptateur ECS mince,
// ZÉRO réimplémentation. Traduit NkAudioSourceComponent / NkAudioListenerComponent
// (données ECS pures, Noge/ECS/Components/Audio/NkAudioComponents.h) en appels
// réels à audio::AudioEngine (singleton NKAudio, Kernel/Runtime/NKAudio) :
//   - AcquireClip() charge le clip (AudioLoader::Load) une seule fois par chemin
//     et le met en cache : le pointeur AudioSample DOIT rester valide tant que
//     la voix joue (AudioEngine::Play() le stocke non-owning, cf. Voice::sample
//     dans NkAudioEngineCore.cpp). Les entrées de cache sont individuellement
//     allouées via NKMemory (New/Delete) pour garder une adresse stable même
//     quand le NkVector de suivi grossit/réalloue (contrairement à un NkVector
//     de AudioSample par valeur, qui aurait invalidé les voix en cours).
//   - playOnStart / loop / spatialize / volume / pitch -> VoiceParams + Play().
//   - Position de la NkTransform de l'entité -> SetSourcePosition (si spatialize).
//   - NkAudioListenerComponent + sa NkTransform -> SetListenerPosition.
//   - Le booléen `playing` du composant est resynchronisé chaque frame depuis
//     AudioEngine::IsPlaying(handle) — l'API NKAudio réelle n'expose PAS de
//     `GetVoiceState(handle)` (contrairement à ce qu'un jalon informel pouvait
//     laisser supposer) ; IsPlaying()/IsPaused()/GetActiveVoices() sont les
//     équivalents réellement présents dans NkAudio.h, utilisés ici.
// =============================================================================

#include "NKECS/System/NkSystem.h"
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Audio/NkAudioComponents.h"
#include "NKAudio/NkAudio.h"
#include "NKAudio/NkAudioBackends.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

	class NkAudioSystem final : public ecs::NkSystem {
		public:
			NkAudioSystem() noexcept = default;
			~NkAudioSystem() noexcept override;

			NkAudioSystem(const NkAudioSystem &) = delete;
			NkAudioSystem &operator=(const NkAudioSystem &) = delete;

			/**
			 * @brief Initialise (ou récupère) le moteur audio NKAudio singleton.
			 *
			 * Idempotent : si `audio::AudioEngine::Instance()` est déjà initialisé
			 * par un autre appelant, cette méthode réussit sans reconfigurer le
			 * moteur, et Shutdown() ne le coupera pas non plus (ownership tracké
			 * par mOwnsEngine, même principe que le reste du moteur pour un
			 * singleton partagé).
			 */
			bool Init(const audio::AudioEngineConfig &cfg = audio::AudioEngineConfig{}) noexcept;

			/// Arrête le moteur (si CE système l'a démarré) + libère les clips en cache.
			void Shutdown() noexcept;

			[[nodiscard]] ecs::NkSystemDesc Describe() const override {
				return ecs::NkSystemDesc{}
					.Writes<ecs::NkAudioSourceComponent>()
					.Reads<ecs::NkAudioListenerComponent>()
					.Reads<ecs::NkTransform>()
					.InGroup(ecs::NkSystemGroup::PostUpdate)
					.Sequential()
					.Named("NkAudioSystem");
			}

			void Execute(ecs::NkWorld &world, float32 dt) override;

			/**
			 * @brief Déclenche explicitement la lecture du clip d'une entité.
			 *
			 * Équivalent concret du "NkAudioSource.Play()" du jalon : force la
			 * lecture même si `playOnStart` a déjà été consommé. Charge le clip si
			 * nécessaire (AcquireClip), construit les VoiceParams depuis le
			 * composant + la NkTransform de l'entité (si présente), appelle
			 * audio::AudioEngine::Play().
			 *
			 * @return Handle de voix NKAudio ; invalide si clip introuvable, entité
			 *         sans NkAudioSourceComponent, ou pool de voix plein.
			 */
			audio::AudioHandle PlaySource(ecs::NkWorld &world, ecs::NkEntityId entity) noexcept;

			/// Arrête la voix associée à l'entité (no-op si elle ne joue pas).
			void StopSource(ecs::NkWorld &world, ecs::NkEntityId entity, float32 fadeOutTime = 0.0f) noexcept;

			/// Handle de voix NKAudio actif pour une entité (invalide si aucune voix suivie).
			[[nodiscard]] audio::AudioHandle VoiceOf(ecs::NkEntityId entity) const noexcept;

			/// Accès direct au moteur (bus, HRTF, contrôles globaux...) — même schéma
			/// que NkPhysicsSystem::World().
			[[nodiscard]] audio::AudioEngine &Engine() noexcept {
				return audio::AudioEngine::Instance();
			}

		private:
			// Entrée de cache clip : allouée individuellement (NKMemory) pour une
			// adresse stable, référencée non-owning par les voix NKAudio actives.
			struct ClipEntry {
					NkString path;
					audio::AudioSample sample;
			};

			struct VoiceEntry {
					ecs::NkEntityId entity;
					audio::AudioHandle handle;
			};

			// Charge (ou récupère du cache) le clip désigné par `path`.
			// Retourne nullptr si le chemin est vide ou si le chargement échoue.
			[[nodiscard]] const audio::AudioSample *AcquireClip(const NkString &path) noexcept;

			[[nodiscard]] VoiceEntry *FindVoice(ecs::NkEntityId entity) noexcept;

			// Construit les VoiceParams NKAudio depuis le composant + la transform
			// optionnelle de l'entité.
			[[nodiscard]] static audio::VoiceParams BuildVoiceParams(const ecs::NkAudioSourceComponent &src,
																	  const ecs::NkTransform *tf) noexcept;

			NkVector<ClipEntry *> mClips;
			NkVector<VoiceEntry> mVoices;
			bool mOwnsEngine = false;
	};

} // namespace nkentseu
