// =============================================================================
// Noge/ECS/Systems/NkAudioSystem.cpp — pont ECS -> NKAudio (moteur audio AAA)
// =============================================================================
#include "NkAudioSystem.h"
#include "NKMemory/NkAllocator.h"

namespace nkentseu {

	using namespace ecs;
	using namespace audio;

	// -------------------------------------------------------------------------
	NkAudioSystem::~NkAudioSystem() noexcept {
		Shutdown();
	}

	// -------------------------------------------------------------------------
	bool NkAudioSystem::Init(const AudioEngineConfig &cfg) noexcept {
		EnsureBackendsRegistered();

		AudioEngine &engine = Engine();
		if (engine.IsInitialized()) {
			// Déjà démarré par un autre appelant (ou un précédent Init()) : on
			// s'y raccroche sans reconfigurer, et on ne le coupera pas depuis
			// Shutdown() (mOwnsEngine reste tel quel).
			return true;
		}

		if (!engine.Initialize(cfg)) {
			return false;
		}
		mOwnsEngine = true;
		return true;
	}

	// -------------------------------------------------------------------------
	void NkAudioSystem::Shutdown() noexcept {
		AudioEngine &engine = Engine();
		if (mOwnsEngine && engine.IsInitialized()) {
			engine.StopAll();
			engine.Shutdown();
		}
		mOwnsEngine = false;

		for (nk_usize i = 0; i < mClips.Size(); ++i) {
			ClipEntry *entry = mClips[i];
			if (entry) {
				AudioLoader::Free(entry->sample);
				memory::NkGetDefaultAllocator().Delete(entry);
			}
		}
		mClips.Clear();
		mVoices.Clear();
	}

	// -------------------------------------------------------------------------
	const AudioSample *NkAudioSystem::AcquireClip(const NkString &path) noexcept {
		if (path.Length() == 0) {
			return nullptr;
		}

		for (nk_usize i = 0; i < mClips.Size(); ++i) {
			if (mClips[i] && mClips[i]->path == path) {
				return &mClips[i]->sample;
			}
		}

		AudioSample loaded = AudioLoader::Load(path.CStr());
		if (!loaded.IsValid()) {
			return nullptr;
		}

		ClipEntry *entry = memory::NkGetDefaultAllocator().New<ClipEntry>();
		entry->path = path;
		entry->sample = loaded;
		mClips.PushBack(entry);
		return &entry->sample;
	}

	// -------------------------------------------------------------------------
	NkAudioSystem::VoiceEntry *NkAudioSystem::FindVoice(NkEntityId entity) noexcept {
		for (nk_usize i = 0; i < mVoices.Size(); ++i) {
			if (mVoices[i].entity == entity) {
				return &mVoices[i];
			}
		}
		return nullptr;
	}

	// -------------------------------------------------------------------------
	VoiceParams NkAudioSystem::BuildVoiceParams(const NkAudioSourceComponent &src, const NkTransform *tf) noexcept {
		VoiceParams vp;
		vp.volume = src.volume;
		vp.pitch = src.pitch;
		vp.looping = src.loop;
		vp.source3d.positional = src.spatialize;
		vp.source3d.minDistance = src.minDistance;
		vp.source3d.maxDistance = src.maxDistance;
		if (tf) {
			vp.source3d.position[0] = tf->localPosition.x;
			vp.source3d.position[1] = tf->localPosition.y;
			vp.source3d.position[2] = tf->localPosition.z;
		}
		return vp;
	}

	// -------------------------------------------------------------------------
	AudioHandle NkAudioSystem::PlaySource(NkWorld &world, NkEntityId entity) noexcept {
		NkAudioSourceComponent *src = world.Get<NkAudioSourceComponent>(entity);
		if (!src) {
			return AUDIO_HANDLE_INVALID;
		}

		const AudioSample *clip = AcquireClip(src->clipPath);
		if (!clip) {
			return AUDIO_HANDLE_INVALID;
		}

		const NkTransform *tf = world.Get<NkTransform>(entity);
		const VoiceParams vp = BuildVoiceParams(*src, tf);

		AudioHandle h = Engine().Play(*clip, vp);
		if (h.IsValid()) {
			src->playing = true;
			if (VoiceEntry *v = FindVoice(entity)) {
				v->handle = h;
			} else {
				mVoices.PushBack(VoiceEntry{entity, h});
			}
		}
		return h;
	}

	// -------------------------------------------------------------------------
	void NkAudioSystem::StopSource(NkWorld &world, NkEntityId entity, float32 fadeOutTime) noexcept {
		if (VoiceEntry *v = FindVoice(entity)) {
			if (v->handle.IsValid()) {
				Engine().Stop(v->handle, fadeOutTime);
			}
			v->handle = AUDIO_HANDLE_INVALID;
		}
		if (NkAudioSourceComponent *src = world.Get<NkAudioSourceComponent>(entity)) {
			src->playing = false;
		}
	}

	// -------------------------------------------------------------------------
	AudioHandle NkAudioSystem::VoiceOf(NkEntityId entity) const noexcept {
		for (nk_usize i = 0; i < mVoices.Size(); ++i) {
			if (mVoices[i].entity == entity) {
				return mVoices[i].handle;
			}
		}
		return AUDIO_HANDLE_INVALID;
	}

	// -------------------------------------------------------------------------
	void NkAudioSystem::Execute(NkWorld &world, float32 /*dt*/) {
		AudioEngine &engine = Engine();
		if (!engine.IsInitialized()) {
			return; // Init() pas encore appelé -- rien à faire (cohérent avec les
					// autres ponts ECS qui ignorent tant que leur ressource n'est
					// pas liée, ex. NkParticleSystem sans mRenderer).
		}

		// 1) Listener : première entité valide (NkAudioListenerComponent + NkTransform).
		world.Query<NkAudioListenerComponent, NkTransform>().ForEach(
			[&](NkEntityId, NkAudioListenerComponent &listener, NkTransform &tf) {
				engine.SetListenerPosition(tf.localPosition.x, tf.localPosition.y, tf.localPosition.z);
				engine.SetMasterVolume(listener.volume);
			});

		// 2) Sources.
		world.Query<NkAudioSourceComponent>().ForEach([&](NkEntityId id, NkAudioSourceComponent &src) {
			const NkTransform *tf = world.Get<NkTransform>(id);

			// (a) Démarrage automatique -- déclencheur "one-shot" : playOnStart
			// lance la lecture UNE SEULE FOIS (sémantique standard façon Unity),
			// il n'est PAS une consigne "auto-replay dès que la voix se libère".
			// On le consomme donc immédiatement, que le Play() réussisse ou non
			// (sinon un chemin de clip invalide retenterait le chargement à
			// chaque frame indéfiniment).
			if (src.playOnStart && !src.playing) {
				src.playOnStart = false;
				if (const AudioSample *clip = AcquireClip(src.clipPath)) {
					const VoiceParams vp = BuildVoiceParams(src, tf);
					AudioHandle h = engine.Play(*clip, vp);
					if (h.IsValid()) {
						src.playing = true;
						if (VoiceEntry *v = FindVoice(id)) {
							v->handle = h;
						} else {
							mVoices.PushBack(VoiceEntry{id, h});
						}
					}
				}
			}

			// (b) Suivi 3D + volume/pitch dynamiques + resynchronisation d'état.
			VoiceEntry *voice = FindVoice(id);
			if (voice && voice->handle.IsValid()) {
				if (src.spatialize && tf) {
					engine.SetSourcePosition(voice->handle, tf->localPosition.x, tf->localPosition.y,
											  tf->localPosition.z);
				}
				engine.SetVolume(voice->handle, src.volume);
				engine.SetPitch(voice->handle, src.pitch);

				const bool stillPlaying = engine.IsPlaying(voice->handle);
				src.playing = stillPlaying;
				if (!stillPlaying) {
					// Voix recyclée par le pool (FINISHED) : prête pour un futur Play().
					voice->handle = AUDIO_HANDLE_INVALID;
				}
			}
		});
	}

} // namespace nkentseu
