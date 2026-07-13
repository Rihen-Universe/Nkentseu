/**
 * @File    NkAudioStreamPlayer.cpp
 * @Brief   Implementation du player streaming.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 */

#include "NKAudio/Streaming/NkAudioStreamPlayer.h"
#include "NKMemory/NkAllocator.h"
#include "NKLogger/NkLog.h"
#include <chrono>
#include <cstring>

namespace nkentseu {
	namespace audio {

		// ────────────────────────────────────────────────────────────────────
		//  Helpers ring buffer SPSC (mono-producer / mono-consumer lock-free).
		//  L'invariant : writePos >= readPos toujours. La taille disponible
		//  pour lecture = writePos - readPos. La taille libre en ecriture =
		//  ringFrames - 1 - (writePos - readPos). On laisse 1 frame de marge
		//  pour distinguer "plein" de "vide".
		// ────────────────────────────────────────────────────────────────────

		static NKENTSEU_INLINE int32 FramesAvailable(int32 wPos, int32 rPos, int32 capacity) noexcept {
			return wPos - rPos;
		}

		static NKENTSEU_INLINE int32 FramesFree(int32 wPos, int32 rPos, int32 capacity) noexcept {
			return capacity - 1 - (wPos - rPos);
		}

		// ════════════════════════════════════════════════════════════════════
		//  AudioStreamPlayer
		// ════════════════════════════════════════════════════════════════════

		AudioStreamPlayer::~AudioStreamPlayer() {
			Shutdown();
		}

		bool AudioStreamPlayer::Init(int32 sampleRate, int32 channels, int32 ringBufferFrames) noexcept {
			if (mRunning.load())
				return true; // deja initialise
			if (sampleRate <= 0 || channels <= 0 || ringBufferFrames <= 0)
				return false;

			mSampleRate = sampleRate;
			mChannels = channels;
			mRingFrames = ringBufferFrames;

			usize ringBytes = usize(mRingFrames) * usize(mChannels) * sizeof(float32);
			mRingBuf = static_cast<float32 *>(memory::NkAlloc(ringBytes, nullptr, sizeof(float32)));
			if (!mRingBuf) {
				logger.Error("[StreamPlayer] Echec allocation ring buffer ({0} octets)", ringBytes);
				return false;
			}
			::memset(mRingBuf, 0, ringBytes);

			mWritePos = 0;
			mReadPos = 0;
			mActive = false;
			mPaused = false;
			mRunning = true;

			mThread = std::thread([this] { DecoderThreadProc(); });
			logger.Info("[StreamPlayer] Init OK : {0} Hz, {1} ch, ring buffer {2} frames.", sampleRate, channels,
						ringBufferFrames);
			return true;
		}

		void AudioStreamPlayer::Shutdown() noexcept {
			if (!mRunning.load())
				return;
			mRunning = false;
			mActive = false;
			mCV.notify_all();
			if (mThread.joinable())
				mThread.join();

			// Cleanup
			{
				std::lock_guard<std::mutex> lock(mStreamMutex);
				if (mStream) {
					memory::NkGetDefaultAllocator().Delete(mStream);
					mStream = nullptr;
				}
			}
			if (mRingBuf) {
				memory::NkFree(mRingBuf, nullptr);
				mRingBuf = nullptr;
			}
			mRingFrames = 0;
		}

		bool AudioStreamPlayer::Play(IAudioStream *stream, bool loop) noexcept {
			if (!mRunning.load() || !stream)
				return false;
			std::lock_guard<std::mutex> lock(mStreamMutex);

			// Remplace le stream actuel (delete l'ancien)
			if (mStream) {
				memory::NkGetDefaultAllocator().Delete(mStream);
				mStream = nullptr;
			}
			mStream = stream;
			mLoop = loop;

			// Reset le ring buffer
			mWritePos = 0;
			mReadPos = 0;
			mActive = true;
			mPaused = false;
			mCV.notify_all();

			logger.Info("[StreamPlayer] Play : {0} Hz, {1} ch, loop={2}", stream->GetSampleRate(),
						stream->GetChannels(), loop ? 1 : 0);
			return true;
		}

		void AudioStreamPlayer::Stop() noexcept {
			std::lock_guard<std::mutex> lock(mStreamMutex);
			mActive = false;
			if (mStream) {
				memory::NkGetDefaultAllocator().Delete(mStream);
				mStream = nullptr;
			}
			mWritePos = 0;
			mReadPos = 0;
		}

		int32 AudioStreamPlayer::ReadFrames(float32 *outBuf, int32 maxFrames) noexcept {
			if (!mRunning.load() || !mActive.load() || mPaused.load() || maxFrames <= 0) {
				::memset(outBuf, 0, usize(maxFrames) * usize(mChannels) * sizeof(float32));
				return 0;
			}
			const int32 wPos = mWritePos.load(std::memory_order_acquire);
			const int32 rPos = mReadPos.load(std::memory_order_relaxed);
			int32 avail = FramesAvailable(wPos, rPos, mRingFrames);
			if (avail <= 0) {
				// Buffer vide : remplir avec du silence (le worker est en retard)
				::memset(outBuf, 0, usize(maxFrames) * usize(mChannels) * sizeof(float32));
				return 0;
			}
			const int32 nFrames = (maxFrames < avail) ? maxFrames : avail;

			// Copie depuis le ring buffer (modulo mRingFrames)
			const float32 vol = mVolume.load();
			int32 rIdx = rPos % mRingFrames;
			int32 left = nFrames;
			float32 *dst = outBuf;
			while (left > 0) {
				int32 chunk = (rIdx + left <= mRingFrames) ? left : (mRingFrames - rIdx);
				const float32 *src = mRingBuf + usize(rIdx) * usize(mChannels);
				if (vol == 1.0f) {
					::memcpy(dst, src, usize(chunk) * usize(mChannels) * sizeof(float32));
				} else {
					const int32 cnt = chunk * mChannels;
					for (int32 i = 0; i < cnt; ++i)
						dst[i] = src[i] * vol;
				}
				dst += usize(chunk) * usize(mChannels);
				rIdx = (rIdx + chunk) % mRingFrames;
				left -= chunk;
			}
			mReadPos.store(rPos + nFrames, std::memory_order_release);
			mCV.notify_all(); // reveille le worker s'il attend

			// Si on a lu moins que demande, remplir le reste avec du silence
			if (nFrames < maxFrames) {
				const int32 remain = maxFrames - nFrames;
				::memset(dst, 0, usize(remain) * usize(mChannels) * sizeof(float32));
			}
			return nFrames;
		}

		// ────────────────────────────────────────────────────────────────────
		//  Decoder thread : alimente le ring buffer en arriere-plan
		// ────────────────────────────────────────────────────────────────────

		void AudioStreamPlayer::DecoderThreadProc() {
			constexpr int32 kChunkFrames = 1024; // decode par paquets de 1024 frames
			// Buffer scratch pour conversion / mismatch channels
			float32 *scratch = static_cast<float32 *>(
				memory::NkAlloc(usize(kChunkFrames) * 8 * sizeof(float32), nullptr, sizeof(float32)));
			// Buffer de sortie du rééchantillonnage (taux natif -> taux du device).
			constexpr int32 kResCap = kChunkFrames + 16;
			float32 *resBuf = static_cast<float32 *>(
				memory::NkAlloc(usize(kResCap) * 8 * sizeof(float32), nullptr, sizeof(float32)));
			if (!scratch || !resBuf) {
				logger.Error("[StreamPlayer] Allocation scratch buffer echec.");
				if (scratch)
					memory::NkFree(scratch, nullptr);
				if (resBuf)
					memory::NkFree(resBuf, nullptr);
				return;
			}
			// État du rééchantillonneur linéaire (persistant entre les chunks pour éviter les clics).
			double resFrac = 0.0;
			float32 resPrev[8] = {0, 0, 0, 0, 0, 0, 0, 0};

			while (mRunning.load()) {
				if (!mActive.load() || mPaused.load()) {
					std::unique_lock<std::mutex> lk(mStreamMutex);
					mCV.wait_for(lk, std::chrono::milliseconds(50));
					continue;
				}

				// Y a-t-il assez de place dans le ring buffer ?
				int32 wPos = mWritePos.load(std::memory_order_relaxed);
				int32 rPos = mReadPos.load(std::memory_order_acquire);
				int32 freeFrames = FramesFree(wPos, rPos, mRingFrames);
				if (freeFrames < kResCap) {
					// Attendre que le consommateur libere de la place
					std::unique_lock<std::mutex> lk(mStreamMutex);
					mCV.wait_for(lk, std::chrono::milliseconds(10));
					continue;
				}

				// Recupere le stream (sous lock pour eviter race avec Play/Stop)
				IAudioStream *stream = nullptr;
				bool loop = false;
				int32 streamCh = 0;
					int32 streamRate = 0;
				{
					std::lock_guard<std::mutex> lock(mStreamMutex);
					stream = mStream;
					loop = mLoop;
					if (stream) {
						streamCh = stream->GetChannels();
						streamRate = stream->GetSampleRate();
					}
				}
				if (!stream) {
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
					continue;
				}

				// Decode kChunkFrames frames
				if (streamCh <= 0 || streamCh > 8) {
					mActive = false;
					continue;
				}
				const double ratio = (streamRate > 0) ? (double)streamRate / (double)mSampleRate : 1.0;
				const bool doResample = (streamRate > 0 && streamRate != mSampleRate);
				// Frames SOURCE a lire pour produire ~kChunkFrames de sortie (upsampling => moins).
				int32 srcToRead = kChunkFrames;
				if (doResample && ratio < 1.0) {
					srcToRead = (int32)((double)kChunkFrames * ratio) + 2;
					if (srcToRead > kChunkFrames) srcToRead = kChunkFrames;
					if (srcToRead < 2) srcToRead = 2;
				}
				int32 nRead = stream->ReadFrames(scratch, srcToRead);
				if (nRead == 0) {
					if (loop) {
						stream->Seek(0);
						resFrac = 0.0; // reset resampler (evite un clic a la boucle)
						for (int32 c = 0; c < 8; ++c) resPrev[c] = 0.0f;
						continue;
					} else {
						mActive = false;
						continue;
					}
				}
				
				// Reechantillonnage lineaire natif -> device (etat persistant resFrac/resPrev).
				const float32 *outSrc = scratch;
				int32 outFrames = nRead;
				if (doResample) {
					double pos = resFrac;
					int32 of = 0;
					while (of < kResCap) {
						int32 i = (int32)pos;
						if ((double)i > pos) i -= 1; // vrai floor (pos peut etre negatif)
						if (i > nRead - 2) break;   // il faut l'echantillon i+1 <= nRead-1
						const double f = pos - (double)i;
						for (int32 c = 0; c < streamCh; ++c) {
							const float32 a = (i < 0) ? resPrev[c] : scratch[usize(i) * usize(streamCh) + c];
							const float32 bb = scratch[usize(i + 1) * usize(streamCh) + c];
							resBuf[usize(of) * usize(streamCh) + c] = a + (bb - a) * (float32)f;
						}
						pos += ratio;
						++of;
					}
					resFrac = pos - (double)nRead; // origine decalee pour le prochain chunk
					for (int32 c = 0; c < streamCh; ++c)
						resPrev[c] = scratch[usize(nRead - 1) * usize(streamCh) + c];
					outSrc = resBuf;
					outFrames = of;
				}
				if (outFrames <= 0)
					continue;
				
				// Conversion canaux (mono<->stereo) + ecriture ring buffer.
				int32 wIdx = wPos % mRingFrames;
				int32 left = outFrames;
				const float32 *src = outSrc;
				while (left > 0) {
					int32 chunk = (wIdx + left <= mRingFrames) ? left : (mRingFrames - wIdx);
					float32 *dst = mRingBuf + usize(wIdx) * usize(mChannels);
					if (streamCh == mChannels) {
						::memcpy(dst, src, usize(chunk) * usize(mChannels) * sizeof(float32));
					} else if (streamCh == 1 && mChannels == 2) {
						for (int32 i = 0; i < chunk; ++i) {
							dst[i * 2 + 0] = src[i];
							dst[i * 2 + 1] = src[i];
						}
					} else if (streamCh == 2 && mChannels == 1) {
						for (int32 i = 0; i < chunk; ++i)
							dst[i] = (src[i * 2 + 0] + src[i * 2 + 1]) * 0.5f;
					} else {
						::memset(dst, 0, usize(chunk) * usize(mChannels) * sizeof(float32));
					}
					src += usize(chunk) * usize(streamCh);
					wIdx = (wIdx + chunk) % mRingFrames;
					left -= chunk;
				}
				mWritePos.store(wPos + outFrames, std::memory_order_release);
			}

			memory::NkFree(scratch, nullptr);
			logger.Info("[StreamPlayer] Decoder thread shutdown.");
		}

	} // namespace audio
} // namespace nkentseu
