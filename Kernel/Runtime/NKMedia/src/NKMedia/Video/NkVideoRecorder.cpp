// =============================================================================
// NKMedia/Video/NkVideoRecorder.cpp — enregistreur A/V (capture moteur → MP4).
// Facade au-dessus de NkH264Encoder : vidéo H.264 + N pistes audio (langues) +
// N pistes sous-titres. Gère le retournement vertical des framebuffers bottom-up.
// =============================================================================
#include "NKMedia/Video/NkVideoRecorder.h"

namespace nkentseu {
	namespace media {

		namespace {
			inline int32 BppOf(NkVideoInputFormat f) {
				return f == NkVideoInputFormat::RGBA32 ? 4 : 3;
			}
		} // namespace

		bool NkVideoRecorder::Begin(const char *path, int32 width, int32 height, int32 fpsNum, int32 fpsDen,
									int32 qp) {
			if (mOpen)
				return false;
			mWidth = width;
			mHeight = height;
			// Un GOP raisonnable pour de la capture temps réel (IDR régulières = seek/robustesse).
			if (!mEnc.Open(path, width, height, fpsNum, fpsDen, qp, 60))
				return false;
			mOpen = true;
			return true;
		}

		int32 NkVideoRecorder::AddAudio(int32 sampleRate, int32 channels, const char *lang3) {
			return mOpen ? mEnc.AddAudioTrack(sampleRate, channels, lang3) : -1;
		}

		int32 NkVideoRecorder::AddSubtitleTrack(const char *lang3) {
			return mOpen ? mEnc.AddSubtitleTrack(lang3) : -1;
		}

		void NkVideoRecorder::AddSubtitle(int32 trackIdx, const char *utf8, uint32 startMs, uint32 durMs) {
			if (mOpen)
				mEnc.AddSubtitle(trackIdx, utf8, startMs, durMs);
		}

		bool NkVideoRecorder::PushVideo(const uint8 *pixels, NkVideoInputFormat fmt, bool flipVertical) {
			if (!mOpen || !pixels)
				return false;
			if (!flipVertical)
				return mEnc.WriteFrame(pixels, fmt);
			// Retourne verticalement (framebuffer bottom-up type OpenGL) dans un buffer temporaire.
			const int32 bpp = BppOf(fmt);
			const uint64 stride = (uint64)mWidth * bpp;
			mFlip.Resize(stride * (uint64)mHeight);
			for (int32 y = 0; y < mHeight; ++y) {
				const uint8 *src = pixels + (uint64)(mHeight - 1 - y) * stride;
				uint8 *dst = mFlip.Data() + (uint64)y * stride;
				for (uint64 i = 0; i < stride; ++i)
					dst[i] = src[i];
			}
			return mEnc.WriteFrame(mFlip.Data(), fmt);
		}

		void NkVideoRecorder::PushAudio(int32 trackIdx, const int16 *interleaved, uint32 frames) {
			if (mOpen)
				mEnc.WriteAudioPcm(trackIdx, interleaved, frames);
		}

		bool NkVideoRecorder::End() {
			if (!mOpen)
				return false;
			mEnc.Close();
			mOpen = false;
			return true;
		}

	} // namespace media
} // namespace nkentseu
