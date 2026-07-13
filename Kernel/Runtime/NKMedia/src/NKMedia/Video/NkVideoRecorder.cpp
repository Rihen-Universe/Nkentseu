// =============================================================================
// NKMedia/Video/NkVideoRecorder.cpp — enregistreur A/V threadé (capture moteur → MP4).
// La boucle de rendu pousse les trames dans une file (rapide) ; un thread de fond les
// encode en H.264 → l'application reste fluide malgré le coût de l'encodage.
// =============================================================================
#include "NKMedia/Video/NkVideoRecorder.h"
#include <cstring> // memcpy

namespace nkentseu {
	namespace media {

		namespace {
			inline int32 BppOf(NkVideoInputFormat f) {
				return f == NkVideoInputFormat::RGBA32 ? 4 : 3;
			}
		} // namespace

		bool NkVideoRecorder::Begin(const char *path, int32 width, int32 height, int32 fpsNum, int32 fpsDen,
									int32 qp, int32 maxQueuedFrames, NkRecorderCodec codec, int32 mjpegQuality) {
			if (mOpen)
				return false;
			mWidth = width;
			mHeight = height;
			mPushed = 0;
			mHead = 0;
			mQueue.Clear();
			mAudioCh.Clear();
			mMaxQueuedVideo = maxQueuedFrames > 0 ? maxQueuedFrames : 32;
			mPendingVideo = 0;
			mDropped = 0;
			mEncodedVideo = 0;
			mEncodeFpsEma = 0.0;
			mLastEncodeNs = 0;
			mUseMjpeg = (codec == NkRecorderCodec::MJPEG);
			mMjpegQuality = (mjpegQuality < 1) ? 1 : (mjpegQuality > 100 ? 100 : mjpegQuality);
			if (mUseMjpeg) {
				// MJPEG : chaque trame = JPEG (codec NKImage), conteneur MOV/MP4, vidéo seule.
				NkVideoConfig cfg;
				cfg.width = width;
				cfg.height = height;
				cfg.fpsNum = fpsNum;
				cfg.fpsDen = fpsDen;
				cfg.codec = NkVideoCodec::MJPEG;
				cfg.container = NkVideoContainer::MOV;
				cfg.quality = mMjpegQuality;
				if (!mMjpegWriter.Open(path, cfg))
					return false;
			} else {
				// GOP raisonnable pour de la capture temps réel (IDR régulières = seek/robustesse).
				if (!mEnc.Open(path, width, height, fpsNum, fpsDen, qp, 60))
					return false;
			}
			mOpen = true;
			mWorker.Start([this](void *) { WorkerLoop(); }); // thread d'encodage
			return true;
		}

		int32 NkVideoRecorder::AddAudio(int32 sampleRate, int32 channels, const char *lang3) {
			if (!mOpen || mUseMjpeg)
				return -1; // MJPEG = vidéo seule (pas d'audio)
			const int32 idx = mEnc.AddAudioTrack(sampleRate, channels, lang3);
			if (idx >= 0)
				mAudioCh.PushBack(channels); // indexé par l'index de piste audio (ordre d'ajout)
			return idx;
		}

		int32 NkVideoRecorder::AddSubtitleTrack(const char *lang3) {
			return (mOpen && !mUseMjpeg) ? mEnc.AddSubtitleTrack(lang3) : -1;
		}

		bool NkVideoRecorder::PushVideo(const uint8 *pixels, NkVideoInputFormat fmt, bool flipVertical) {
			if (!mOpen || !pixels)
				return false;

			// File BORNEE : si l'encodeur est en retard (file pleine), on ABANDONNE cette
			// trame (drop-newest) au lieu de gonfler la memoire. Test AVANT le memcpy (cher).
			mMutex.Lock();
			if (mPendingVideo >= mMaxQueuedVideo) {
				++mDropped;
				mMutex.Unlock();
				return true; // temps reel : trame droppee (voir DroppedFrames())
			}
			++mPendingVideo; // reserve un emplacement
			mMutex.Unlock();
			const int32 bpp = BppOf(fmt);
			const uint64 stride = (uint64)mWidth * bpp;
			const uint64 total = stride * (uint64)mHeight;
			Item it;
			it.type = 0;
			it.fmt = fmt;
			it.data.Resize(total);
			uint8 *dst = it.data.Data();
			if (!flipVertical) {
				memcpy(dst, pixels, (size_t)total);
			} else {
				for (int32 y = 0; y < mHeight; ++y)
					memcpy(dst + (uint64)y * stride, pixels + (uint64)(mHeight - 1 - y) * stride, (size_t)stride);
			}
			mMutex.Lock();
			mQueue.PushBack(static_cast<Item &&>(it));
			mMutex.Unlock();
			(void)mSem.Release();
			++mPushed;
			return true;
		}

		void NkVideoRecorder::PushAudio(int32 trackIdx, const int16 *interleaved, uint32 frames) {
			if (!mOpen || !interleaved || frames == 0)
				return;
			if (trackIdx < 0 || trackIdx >= (int32)mAudioCh.Size())
				return;
			const uint64 bytes = (uint64)frames * (uint64)mAudioCh[trackIdx] * 2;
			Item it;
			it.type = 1;
			it.track = trackIdx;
			it.frames = frames;
			it.data.Resize(bytes);
			memcpy(it.data.Data(), interleaved, (size_t)bytes);
			mMutex.Lock();
			mQueue.PushBack(static_cast<Item &&>(it));
			mMutex.Unlock();
			(void)mSem.Release();
		}

		void NkVideoRecorder::AddSubtitle(int32 trackIdx, const char *utf8, uint32 startMs, uint32 durMs) {
			if (!mOpen || !utf8)
				return;
			uint32 len = 0;
			while (utf8[len])
				++len;
			Item it;
			it.type = 3;
			it.track = trackIdx;
			it.startMs = startMs;
			it.durMs = durMs;
			it.data.Resize((uint64)len + 1);
			memcpy(it.data.Data(), utf8, (size_t)len + 1); // inclut le \0
			mMutex.Lock();
			mQueue.PushBack(static_cast<Item &&>(it));
			mMutex.Unlock();
			(void)mSem.Release();
		}

		void NkVideoRecorder::WorkerLoop() {
			for (;;) {
				mSem.Acquire();
				Item item;
				bool has = false;
				mMutex.Lock();
				if (mHead < mQueue.Size()) {
					item = static_cast<Item &&>(mQueue[mHead]);
					++mHead;
					if (mHead >= mQueue.Size()) { // file drainée → on remet à zéro (borne la mémoire)
						mQueue.Clear();
						mHead = 0;
					}
					has = true;
				}
				mMutex.Unlock();
				if (!has)
					continue;
				if (item.type == 2)
					break; // stop
				else if (item.type == 0) {
					if (mUseMjpeg)
						mMjpegWriter.WriteFrame(item.data.Data(), item.fmt);
					else
						mEnc.WriteFrame(item.data.Data(), item.fmt);
					const int64 nowNs = NkChrono::Now().ToNanoseconds();
					mMutex.Lock();
					if (mPendingVideo > 0)
						--mPendingVideo;
					++mEncodedVideo;
					if (mLastEncodeNs != 0 && nowNs > mLastEncodeNs) {
						const double dt = (double)(nowNs - mLastEncodeNs) * 1e-9;
						const double inst = dt > 0.0 ? 1.0 / dt : 0.0;
						mEncodeFpsEma = (mEncodeFpsEma <= 0.0) ? inst : (0.9 * mEncodeFpsEma + 0.1 * inst);
					}
					mLastEncodeNs = nowNs;
					mMutex.Unlock();
				}
				else if (item.type == 1 && !mUseMjpeg)
					mEnc.WriteAudioPcm(item.track, reinterpret_cast<const int16 *>(item.data.Data()), item.frames);
				else if (item.type == 3 && !mUseMjpeg)
					mEnc.AddSubtitle(item.track, reinterpret_cast<const char *>(item.data.Data()), item.startMs,
									 item.durMs);
			}
		}

		int32 NkVideoRecorder::QueueDepth() {
			mMutex.Lock();
			const int32 d = mPendingVideo;
			mMutex.Unlock();
			return d;
		}

		uint64 NkVideoRecorder::DroppedFrames() {
			mMutex.Lock();
			const uint64 d = mDropped;
			mMutex.Unlock();
			return d;
		}

		double NkVideoRecorder::EncodeFps() {
			mMutex.Lock();
			const double v = mEncodeFpsEma;
			mMutex.Unlock();
			return v;
		}

		bool NkVideoRecorder::End() {
			if (!mOpen)
				return false;
			Item stop;
			stop.type = 2;
			mMutex.Lock();
			mQueue.PushBack(static_cast<Item &&>(stop));
			mMutex.Unlock();
			(void)mSem.Release();
			if (mWorker.Joinable())
				mWorker.Join();
			if (mUseMjpeg)
				mMjpegWriter.Close();
			else
				mEnc.Close();
			mOpen = false;
			return true;
		}

	} // namespace media
} // namespace nkentseu
