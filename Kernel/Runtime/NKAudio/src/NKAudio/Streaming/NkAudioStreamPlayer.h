#pragma once
/**
 * @File    NkAudioStreamPlayer.h
 * @Brief   Player audio streame : decoder thread + ring buffer + crossfade + loop.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - All Rights Reserved (see LICENSE)
 *
 * @Architecture
 *  Le AudioStreamPlayer maintient :
 *   - Un IAudioStream actif (la source audio).
 *   - Un ring buffer SPSC (Single Producer Single Consumer) float32.
 *   - Un thread worker qui appelle stream->ReadFrames() et ecrit dans le buffer.
 *   - Une methode ReadFrames() appelable depuis le thread audio (lock-free en
 *     lecture) qui pompe le buffer.
 *
 *  Le buffer fait typiquement 1-2 secondes audio (~88200 frames * 2 ch * 4 bytes
 *  = ~700 Ko pour stereo 44.1 kHz). Suffisant pour absorber les jitters du
 *  thread worker (decode CPU peut prendre plusieurs ms par chunk).
 *
 * @Usage
 *   AudioStreamPlayer player;
 *   player.Init(48000, 2, 88200); // sampleRate, channels, ring buffer frames
 *   IAudioStream* s = OpenAudioStream("music.flac");
 *   player.Play(s, true); // loop = true
 *   // ... dans le thread audio :
 *   player.ReadFrames(outBuf, nFrames);
 *   // ... a la fin :
 *   player.Stop();
 *   player.Shutdown();
 */

#include "NKAudio/Streaming/NkAudioStream.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace nkentseu {
	namespace audio {

		class NKENTSEU_AUDIO_API AudioStreamPlayer {
			public:
				AudioStreamPlayer() = default;
				~AudioStreamPlayer();

				/// Initialise le player avec la config sortie.
				/// @param sampleRate      Sample rate cible (= celui du backend audio).
				/// @param channels        Canaux sortie (1 ou 2).
				/// @param ringBufferFrames  Taille du ring buffer en frames (typiquement 88200 pour ~2s).
				bool Init(int32 sampleRate, int32 channels, int32 ringBufferFrames = 88200) noexcept;

				/// Stop, join le thread worker, libere les ressources.
				void Shutdown() noexcept;

				/// Demarre la lecture du stream donne. Le player prend possession du
				/// stream (le supprime via delete a Stop()/Shutdown()).
				/// Si un stream est deja en cours, il est arrete et remplace.
				/// @param loop  true = boucle infinie (seek(0) a la fin)
				bool Play(IAudioStream *stream, bool loop = false) noexcept;

				/// Arrete la lecture, libere le stream actuel.
				void Stop() noexcept;

				/// Pause / resume (le thread worker tourne toujours mais ne decode plus).
				void Pause() noexcept {
					mPaused = true;
				}

				void Resume() noexcept {
					mPaused = false;
				}

				/// Lit jusqu'a maxFrames frames depuis le ring buffer.
				/// Appel attendu depuis le thread audio - lock-free en lecture.
				/// Retourne le nombre de frames effectivement ecrites.
				int32 ReadFrames(float32 *outBuf, int32 maxFrames) noexcept;

				/// True si stream actif et pas EOF + en lecture (non-pause).
				bool IsPlaying() const noexcept {
					return mActive && !mPaused;
				}

				/// Volume scalaire applique a la sortie (1.0 = neutre).
				void SetVolume(float32 v) noexcept {
					mVolume = v;
				}

				float32 GetVolume() const noexcept {
					return mVolume;
				}

				/// Vitesse de lecture (1.0 = normale). Multiplie le ratio de reechantillonnage
				/// cote PRODUCTEUR (thread worker) : un changement prend effet une fois le ring
				/// buffer draine (~1-2s) SAUF si on force un flush (voir SeekContent/FlushRing).
				void SetSpeed(float32 speed) noexcept {
					mSpeed.store(speed > 0.01f ? speed : 0.01f, std::memory_order_relaxed);
				}

				float32 GetSpeed() const noexcept {
					return mSpeed.load(std::memory_order_relaxed);
				}

				/// Position de lecture en SECONDES DE CONTENU (pas le temps mixeur/ring look-ahead).
				/// Avancee par le thread AUDIO (cote consommateur, dans ReadFrames) au rythme de
				/// l'horloge reelle du backend x la vitesse courante -> utilisable comme horloge
				/// maitresse (ex. synchronisation video), contrairement a AudioEngine::
				/// GetPlaybackPosition qui renvoie toujours 0 pour une voix PlayProcedural.
				float64 GetPositionSeconds() const noexcept {
					return (float64)mContentMicros.load(std::memory_order_acquire) / 1000000.0;
				}

				/// Repositionne le CONTENU (stream + horloge de position) ; vide le ring buffer
				/// (les donnees deja decodees a l'ancienne position/vitesse sont invalidees).
				/// Thread-safe (verrouille comme Play/Stop) ; sans effet si aucun stream actif.
				void SeekContent(float32 seconds) noexcept;

				/// Vide le ring buffer SANS changer la position du stream sous-jacent (utile pour
				/// appliquer un changement de vitesse "immediatement" plutot qu'apres ~1-2s).
				void FlushRing() noexcept;

				/// True si le thread PRODUCTEUR est associe a un stream et n'a pas encore atteint
				/// sa fin (EOF sans boucle) OU si Stop() n'a pas ete appele. ⚠️ Passe a false DES
				/// que le decodage source est termine, MEME S'IL RESTE DU CONTENU DEJA DECODE
				/// dans le ring buffer (jusqu'a ~2s, le producteur decodant plus vite que le temps
				/// reel) : ne PAS l'utiliser seul pour decider "il n'y a plus rien a entendre" (ca
				/// coupe le son ~2s trop tot) — voir IsFinished() pour cette question-la.
				bool IsActive() const noexcept {
					return mActive.load(std::memory_order_relaxed);
				}

				/// True uniquement quand il n'y a VRAIMENT plus rien a jouer : producteur termine
				/// (IsActive()==false) ET ring buffer vide (aucune frame deja decodee en attente).
				/// C'est la bonne question pour une horloge de synchronisation externe (ex. video) :
				/// "puis-je encore faire confiance a GetPositionSeconds() ?" -> !IsFinished().
				bool IsFinished() const noexcept {
					if (mActive.load(std::memory_order_relaxed))
						return false;
					const int32 wPos = mWritePos.load(std::memory_order_acquire);
					const int32 rPos = mReadPos.load(std::memory_order_relaxed);
					return (wPos - rPos) <= 0;
				}

			private:
				void DecoderThreadProc();

				// Sortie config
				int32 mSampleRate = 0;
				int32 mChannels = 0;

				// Ring buffer SPSC
				float32 *mRingBuf = nullptr;
				int32 mRingFrames = 0;			 ///< Capacite en frames
				std::atomic<int32> mWritePos{0}; ///< Index frame producteur
				std::atomic<int32> mReadPos{0};	 ///< Index frame consommateur

				// Thread worker
				std::thread mThread;
				std::atomic<bool> mRunning{false};
				std::mutex mStreamMutex;	 ///< Protege mStream/mLoop changes
				std::condition_variable mCV; ///< Notifie quand le buffer a de la place

				// Stream actif
				IAudioStream *mStream = nullptr;
				bool mLoop = false;
				std::atomic<bool> mActive{false};
				std::atomic<bool> mPaused{false};

				// Volume
				std::atomic<float32> mVolume{1.0f};

				// Vitesse (multiplie le ratio de reechantillonnage cote producteur).
				std::atomic<float32> mSpeed{1.0f};

				// Position de CONTENU en microsecondes. Ecrivain UNIQUE = le thread audio
				// (dans ReadFrames) -> store/load simples suffisent (pas de CAS necessaire,
				// un seul thread modifie jamais cette valeur), lu par le thread appelant
				// (ex. la boucle video) via GetPositionSeconds().
				std::atomic<nk_int64> mContentMicros{0};
		};

	} // namespace audio
} // namespace nkentseu
