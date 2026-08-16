// =============================================================================
// NkCameraSystem.cpp — Implémentation complète et fonctionnelle
// =============================================================================

#include "NkCameraSystem.h"
#include "NKCamera/NkCamera2D.h"
#include "NKImage/Core/NkImage.h"
#include "NKImage/Codecs/JPEG/NkJPEGCodec.h"
#include "NKLogger/NkLog.h"
#include "NKTime/NkSystemClock.h"

#include <ctime>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <cstdlib>

namespace nkentseu {

	// ===========================================================================
	// NkCameraSystem — Init / Shutdown
	// ===========================================================================

	bool NkCameraSystem::Init() {
		if (mReady)
			return true;
		if (!mBackend.Init()) {
			return false;
		}
		// Câbler le callback interne (thread de capture → OnFrame)
		mBackend.SetFrameCallback([this](const NkCameraFrame &f) { OnFrame(f); });
		mReady = true;
		return true;
	}

	void NkCameraSystem::Shutdown() {
		if (!mReady)
			return;
		mBackend.StopVideoRecord();
		mBackend.StopStreaming();
		mBackend.Shutdown();
		mReady = false;
		mRefCaptured = false;
		mVirtualCamera = nullptr;
	}

	// ===========================================================================
	// Énumération
	// ===========================================================================

	NkVector<NkCameraDevice> NkCameraSystem::EnumerateDevices() {
		if (!mReady)
			return {};
		return mBackend.EnumerateDevices();
	}

	void NkCameraSystem::SetHotPlugCallback(NkCameraHotPlugCallback cb) {
		if (mReady)
			mBackend.SetHotPlugCallback(traits::NkMove(cb));
	}

	// ===========================================================================
	// Streaming
	// ===========================================================================

	bool NkCameraSystem::StartStreaming(const NkCameraConfig &config) {
		if (!mReady)
			return false;
		NkCameraConfig cfg = config;
		cfg.Resolve();
		mCurrentDeviceIndex = cfg.deviceIndex;
		// Retenir la demande de miroir : elle voyage ensuite AVEC chaque trame,
		// jusqu'à la conversion qui l'applique. Sans ce relais, le champ
		// `flipHorizontal` de la configuration restait ce qu'il a été jusqu'au
		// 2026-08-15 — déclaré, réglable, et ignoré par tout le monde.
		mFlipHorizontal = cfg.flipHorizontal;
		// Recâbler le callback (peut avoir été écrasé lors d'un StopStreaming)
		mBackend.SetFrameCallback([this](const NkCameraFrame &f) { OnFrame(f); });
		return mBackend.StartStreaming(cfg);
	}

	void NkCameraSystem::StopStreaming() {
		if (mReady)
			mBackend.StopStreaming();
	}

	NkCameraState NkCameraSystem::GetState() const {
		return mReady ? mBackend.GetState() : NkCameraState::NK_CAM_STATE_CLOSED;
	}

	bool NkCameraSystem::IsStreaming() const {
		auto s = GetState();
		return s == NkCameraState::NK_CAM_STATE_STREAMING || s == NkCameraState::NK_CAM_STATE_RECORDING;
	}

	void NkCameraSystem::SetFrameCallback(NkFrameCallback cb) {
		threading::NkScopedLock<threading::NkMutex> lk(mFrameMutex);
		mUserCallback = traits::NkMove(cb);
	}

	bool NkCameraSystem::GetLastFrame(NkCameraFrame &out) {
		threading::NkScopedLock<threading::NkMutex> lk(mFrameMutex);
		if (!mHasFrame)
			return false;
		out = mLastFrame;
		return true;
	}

	void NkCameraSystem::EnableFrameQueue(uint32 maxSize) {
		threading::NkScopedLock<threading::NkMutex> lk(mQueueMutex);
		mQueueEnabled = true;
		mMaxQueueSize = maxSize;
	}

	bool NkCameraSystem::DrainFrameQueue(NkCameraFrame &out) {
		threading::NkScopedLock<threading::NkMutex> lk(mQueueMutex);
		if (mFrameQueue.Empty())
			return false;
		out = traits::NkMove(mFrameQueue.Back());
		while (!mFrameQueue.Empty())
			mFrameQueue.Pop();
		return true;
	}

	// ===========================================================================
	// Capture photo
	// ===========================================================================

	bool NkCameraSystem::CapturePhoto(NkPhotoCaptureResult &out) {
		if (!mReady) {
			out.success = false;
			out.errorMsg = "Camera not initialised";
			return false;
		}
		return mBackend.CapturePhoto(out);
	}

	NkString NkCameraSystem::CapturePhotoToFile(const NkString &path) {
		if (!mReady)
			return "";

		NkPhotoCaptureResult res;
		if (!mBackend.CapturePhoto(res)) {
			// Fallback : dernière frame du queue/buffer si le backend ne sait
			// pas capturer à la demande (cas de certains backends streaming-only).
			if (!GetLastFrame(res.frame) || !res.frame.IsValid())
				return "";
		}

		NkString outPath = path;
		if (outPath.Empty())
			outPath = GenerateAutoPath("photo", "png");

		if (!SaveFrameToFile(res.frame, outPath, 90))
			return "";
		return outPath;
	}

	// ===========================================================================
	// Enregistrement vidéo
	// ===========================================================================

	bool NkCameraSystem::StartVideoRecord(const NkVideoRecordConfig &config) {
		if (!mReady)
			return false;
		NkVideoRecordConfig cfg = config;
		if (cfg.outputPath.Empty())
			cfg.outputPath = GenerateAutoPath("video", cfg.container);

		// Mode IMAGE_SEQUENCE_ONLY : pris en charge par NkCameraSystem
		// (cross-platform via NKImage). outputPath sert de préfixe
		// ou de dossier — on ajoute "_NNNNNN.ext" pour chaque frame.
		if (cfg.mode == NkVideoRecordConfig::Mode::IMAGE_SEQUENCE_ONLY) {
			threading::NkScopedLock<threading::NkMutex> lk(mFrameMutex);
			mImageSequenceActive = true;
			mImageSequenceDir = cfg.outputPath; // préfixe complet attendu
			// Extension par défaut PNG (lossless). L'utilisateur peut forcer
			// jpg via cfg.videoCodec == "jpg" ou cfg.container == "jpg".
			mImageSequenceExt = (cfg.container == "jpg" || cfg.videoCodec == "jpg") ? NkString("jpg") : NkString("png");
			mImageSequenceQuality = 90;
			mImageSequenceIndex = 0;
			mImageSequenceStartUs = (uint64)NkSystemClock::UnixMilliseconds() * 1000ULL;
			return true;
		}

		// Mode VIDEO_ONLY / AUTO : déléguer au backend natif (H.264, etc.)
		bool ok = mBackend.StartVideoRecord(cfg);
		if (!ok && cfg.mode == NkVideoRecordConfig::Mode::AUTO) {
			// Fallback transparent : si le backend natif refuse en AUTO,
			// on bascule en séquence d'images.
			NkVideoRecordConfig fallback = cfg;
			fallback.mode = NkVideoRecordConfig::Mode::IMAGE_SEQUENCE_ONLY;
			return StartVideoRecord(fallback);
		}
		return ok;
	}

	void NkCameraSystem::StopVideoRecord() {
		if (!mReady)
			return;
		{
			threading::NkScopedLock<threading::NkMutex> lk(mFrameMutex);
			mImageSequenceActive = false;
			mImageSequenceIndex = 0;
		}
		mBackend.StopVideoRecord();
	}

	bool NkCameraSystem::IsRecording() const {
		if (!mReady)
			return false;
		{
			threading::NkScopedLock<threading::NkMutex> lk(mFrameMutex);
			if (mImageSequenceActive)
				return true;
		}
		return mBackend.IsRecording();
	}

	float NkCameraSystem::GetRecordingDurationSeconds() const {
		if (!mReady)
			return 0.f;
		{
			threading::NkScopedLock<threading::NkMutex> lk(mFrameMutex);
			if (mImageSequenceActive) {
				uint64 nowUs = (uint64)NkSystemClock::UnixMilliseconds() * 1000ULL;
				return float((nowUs - mImageSequenceStartUs) / 1000000.0);
			}
		}
		return mBackend.GetRecordingDurationSeconds();
	}

	// ===========================================================================
	// Contrôles
	// ===========================================================================

	bool NkCameraSystem::SetAutoFocus(bool v) {
		return mReady && mBackend.SetAutoFocus(v);
	}

	bool NkCameraSystem::SetAutoExposure(bool v) {
		return mReady && mBackend.SetAutoExposure(v);
	}

	bool NkCameraSystem::SetAutoWhiteBalance(bool v) {
		return mReady && mBackend.SetAutoWhiteBalance(v);
	}

	bool NkCameraSystem::SetZoom(float v) {
		return mReady && mBackend.SetZoom(v);
	}

	bool NkCameraSystem::SetFlash(bool v) {
		return mReady && mBackend.SetFlash(v);
	}

	bool NkCameraSystem::SetTorch(bool v) {
		return mReady && mBackend.SetTorch(v);
	}

	bool NkCameraSystem::SetFocusPoint(float x, float y) {
		return mReady && mBackend.SetFocusPoint(x, y);
	}

	// ===========================================================================
	// Informations session
	// ===========================================================================

	uint32 NkCameraSystem::GetWidth() const {
		return mReady ? mBackend.GetWidth() : 0;
	}

	uint32 NkCameraSystem::GetHeight() const {
		return mReady ? mBackend.GetHeight() : 0;
	}

	uint32 NkCameraSystem::GetFPS() const {
		return mReady ? mBackend.GetFPS() : 0;
	}

	NkPixelFormat NkCameraSystem::GetFormat() const {
		return mReady ? mBackend.GetFormat() : NkPixelFormat::NK_PIXEL_UNKNOWN;
	}

	NkString NkCameraSystem::GetLastError() const {
		return mReady ? mBackend.GetLastError() : "Camera system not initialised";
	}

	// ===========================================================================
	// Callback interne — reçoit chaque frame du thread de capture
	// ===========================================================================

	void NkCameraSystem::OnFrame(const NkCameraFrame &frame) {
		bool doSequence = false;
		NkString seqDir, seqExt;
		uint32 seqIdx = 0;
		int32 seqQ = 90;

		// Mettre à jour la dernière frame et appeler le callback utilisateur
		{
			threading::NkScopedLock<threading::NkMutex> lk(mFrameMutex);
			mLastFrame = frame;
			mLastFrame.flipHorizontal = mFlipHorizontal;
			mHasFrame = true;
			if (mUserCallback) {
				// Le rappel utilisateur reçoit la même demande de miroir que
				// `GetLastFrame` : deux chemins qui livreraient des images
				// différentes seraient un piège, pas une souplesse.
				NkCameraFrame copie = frame;
				copie.flipHorizontal = mFlipHorizontal;
				mUserCallback(copie);
			}

			if (mImageSequenceActive) {
				doSequence = true;
				seqDir = mImageSequenceDir;
				seqExt = mImageSequenceExt;
				seqIdx = mImageSequenceIndex++;
				seqQ = mImageSequenceQuality;
			}
		}
		// Queue
		if (mQueueEnabled) {
			threading::NkScopedLock<threading::NkMutex> lk(mQueueMutex);
			if (mFrameQueue.Size() >= mMaxQueueSize)
				mFrameQueue.Pop();
			mFrameQueue.Push(frame);
		}

		// Mode IMAGE_SEQUENCE_ONLY : sauve hors lock (I/O potentiellement lent)
		if (doSequence) {
			// Composition directe : plus de tampon `char[24]` intermédiaire, donc
			// plus de troncature possible si l'index venait à grandir.
			NkString path = seqDir + NkString::Fmtf("_%06u.", seqIdx) + seqExt;
			(void)SaveFrameToFile(frame, path, seqQ);
		}
	}

	// ===========================================================================
	// MAPPING CAMÉRA VIRTUELLE ← CAMÉRA PHYSIQUE (IMU)
	// ===========================================================================

	void NkCameraSystem::SetVirtualCameraTarget(NkCamera2D *cam2D) {
		mVirtualCamera = cam2D;
		mRefCaptured = false; // réinitialiser la référence
	}

	void NkCameraSystem::SetVirtualCameraMapping(bool enable) {
		mVirtualMappingEnabled = enable;
		if (enable)
			mRefCaptured = false; // prendre une nouvelle référence
	}

	bool NkCameraSystem::GetCurrentOrientation(NkCameraOrientation &out) const {
		if (!mReady)
			return false;
		return const_cast<NkCameraBackend &>(mBackend).GetOrientation(out);
	}

	void NkCameraSystem::UpdateVirtualCamera(float dt) {
		(void)dt;
		if (!mVirtualMappingEnabled || !mVirtualCamera || !mReady)
			return;

		NkCameraOrientation orient;
		if (!mBackend.GetOrientation(orient))
			return;

		// Capturer la pose de référence au premier appel
		if (!mRefCaptured) {
			mRefOrientation = orient;
			mSmoothedYaw = 0.f;
			mSmoothedPitch = 0.f;
			mRefCaptured = true;
			return;
		}

		// Différence par rapport à la référence
		float deltaYaw = orient.yaw - mRefOrientation.yaw;
		float deltaPitch = orient.pitch - mRefOrientation.pitch;

		// Inversion optionnelle
		if (mMapConfig.invertX)
			deltaYaw = -deltaYaw;
		if (mMapConfig.invertY)
			deltaPitch = -deltaPitch;

		// Appliquer la sensibilité
		float targetYaw = deltaYaw * mMapConfig.yawSensitivity;
		float targetPitch = deltaPitch * mMapConfig.pitchSensitivity;

		// Lissage par interpolation (si activé)
		if (mMapConfig.smoothing) {
			float f = mMapConfig.smoothFactor;
			mSmoothedYaw += (targetYaw - mSmoothedYaw) * f;
			mSmoothedPitch += (targetPitch - mSmoothedPitch) * f;
		} else {
			mSmoothedYaw = targetYaw;
			mSmoothedPitch = targetPitch;
		}

		// Appliquer à la caméra virtuelle :
		// yaw   → translation horizontale (panoramique horizontal)
		// pitch → translation verticale   (panoramique vertical)
		// roll  → rotation de la caméra   (si souhaité)
		float panX = mSmoothedYaw * mMapConfig.translationScale;
		float panY = mSmoothedPitch * mMapConfig.translationScale;

		if (mMapConfig.translationScale > 0.f) {
			// Mode translation : déplacer la caméra dans l'espace monde
			mVirtualCamera->SetPosition(panX, panY);
		} else {
			// Mode rotation seulement : utiliser la rotation de la caméra virtuelle
			// (yaw → rotation cam2D, car en 2D le seul axe de rotation est Z)
			mVirtualCamera->SetRotation(mSmoothedYaw + orient.roll);
		}
	}

	// ===========================================================================
	// Conversions de format
	// ===========================================================================

	// La formule de déquantification YUV se choisit sur la PLAGE, jamais sur le
	// format. Déduire la plage du format « marchait » tant qu'un seul producteur
	// émettait chaque format : c'était juste par coïncidence de producteurs, pas
	// par construction, et le second producteur l'aurait cassé en silence.
	//
	// Quand la plage n'est PAS déclarée, on ne devine pas : on le DIT, une fois
	// par format, et on applique un repli explicitement historique — celui qui
	// reproduit exactement le comportement d'avant, pour que la migration ne
	// change rien à l'écran. Le repli est une compatibilité assumée, pas une
	// mesure ; c'est pour ça qu'il s'accompagne d'un avertissement.
	static bool NkFrameUsesVideoRange(const NkCameraFrame &frame) {
		if (frame.range == NkColorRange::NK_COLOR_RANGE_VIDEO)
			return true;
		if (frame.range == NkColorRange::NK_COLOR_RANGE_FULL)
			return false;

		// Repli historique : I420 était décodé en plage pleine, les autres en
		// plage réduite. On le conserve à l'identique, et on le signale.
		const bool legacyVideoRange = (frame.format != NkPixelFormat::NK_PIXEL_YUV420);

		// Un avertissement par format, pas un par image : un journal qui se
		// répète soixante fois par seconde n'est plus lu, donc ne protège plus.
		static bool warned[8] = { false, false, false, false, false, false, false, false };
		const uint32 slot = (uint32)frame.format & 7u;
		if (!warned[slot]) {
			warned[slot] = true;
			logger.Warnf("[NkCamera] Plage de couleur NON DECLAREE pour %s : repli historique = %s. "
			             "Le producteur doit renseigner NkCameraFrame::range (cf. NkColorRange).",
			             NkCameraPixelFormatToString(frame.format),
			             legacyVideoRange ? "REDUITE" : "PLEINE");
		}
		return legacyVideoRange;
	}

	// Miroir horizontal — délégué à NKImage, qui sait déjà le faire.
	//
	// La première version, écrite le 2026-08-15, retournait les octets à la main.
	// Elle marchait, et c'était le défaut : `NkImage::FlipHorizontal()` existe, et
	// `NkImage::Wrap` donne une vue NON PROPRIÉTAIRE sur un tampon existant —
	// donc aucune copie, aucune allocation, et le retournement n'est plus écrit
	// deux fois dans le dépôt.
	// *« Toujours utiliser Nkentseu »* ne se voit dans aucun `grep` : un helper
	// local qui double le Kernel ne casse rien, ne prévient personne, et se
	// contente d'exister.
	static void NkMiroirRGBA8(NkCameraFrame &frame) {
		NkImage *vue = NkImage::Wrap(frame.data.Data(), (int32)frame.width, (int32)frame.height,
									 NkImagePixelFormat::NK_RGBA32);
		if (vue == nullptr)
			return;
		vue->FlipHorizontal();
		vue->Free(); // vue non propriétaire : libère le descripteur, pas les pixels
	}

	static bool NkConvertToRGBA8Impl(NkCameraFrame &frame) {
		if (frame.format == NkPixelFormat::NK_PIXEL_RGBA8)
			return true;
		uint32 w = frame.width, h = frame.height;
		NkVector<uint8> out;
		out.Resize(w * h * 4);

		if (frame.format == NkPixelFormat::NK_PIXEL_BGRA8) {
			for (uint32 i = 0; i < w * h; ++i) {
				out[i * 4 + 0] = frame.data[i * 4 + 2];
				out[i * 4 + 1] = frame.data[i * 4 + 1];
				out[i * 4 + 2] = frame.data[i * 4 + 0];
				out[i * 4 + 3] = frame.data[i * 4 + 3];
			}
			frame.data = traits::NkMove(out);
			frame.format = NkPixelFormat::NK_PIXEL_RGBA8;
			frame.stride = w * 4;
			return true;
		}

		if (frame.format == NkPixelFormat::NK_PIXEL_RGB8) {
			for (uint32 i = 0; i < w * h; ++i) {
				out[i * 4 + 0] = frame.data[i * 3 + 0];
				out[i * 4 + 1] = frame.data[i * 3 + 1];
				out[i * 4 + 2] = frame.data[i * 3 + 2];
				out[i * 4 + 3] = 255;
			}
			frame.data = traits::NkMove(out);
			frame.format = NkPixelFormat::NK_PIXEL_RGBA8;
			frame.stride = w * 4;
			return true;
		}

		if (frame.format == NkPixelFormat::NK_PIXEL_YUYV) {
			// YUYV packed: Y0 U0 Y1 V0
			const bool videoRange = NkFrameUsesVideoRange(frame);
			for (uint32 i = 0; i < w * h / 2; ++i) {
				float y0 = (float)frame.data[i * 4 + 0];
				float cb = (float)frame.data[i * 4 + 1] - 128.f;
				float y1 = (float)frame.data[i * 4 + 2];
				float cr = (float)frame.data[i * 4 + 3] - 128.f;
				auto cl = [](float v) -> uint8 { return (uint8)(v < 0 ? 0 : v > 255 ? 255 : v); };
				if (videoRange) {
					y0 -= 16.f;
					y1 -= 16.f;
					out[i * 8 + 0] = cl(y0 * 1.164f + cr * 1.596f);
					out[i * 8 + 1] = cl(y0 * 1.164f - cb * 0.391f - cr * 0.813f);
					out[i * 8 + 2] = cl(y0 * 1.164f + cb * 2.018f);
					out[i * 8 + 4] = cl(y1 * 1.164f + cr * 1.596f);
					out[i * 8 + 5] = cl(y1 * 1.164f - cb * 0.391f - cr * 0.813f);
					out[i * 8 + 6] = cl(y1 * 1.164f + cb * 2.018f);
				} else {
					out[i * 8 + 0] = cl(y0 + 1.402f * cr);
					out[i * 8 + 1] = cl(y0 - 0.344136f * cb - 0.714136f * cr);
					out[i * 8 + 2] = cl(y0 + 1.772f * cb);
					out[i * 8 + 4] = cl(y1 + 1.402f * cr);
					out[i * 8 + 5] = cl(y1 - 0.344136f * cb - 0.714136f * cr);
					out[i * 8 + 6] = cl(y1 + 1.772f * cb);
				}
				out[i * 8 + 3] = 255;
				out[i * 8 + 7] = 255;
			}
			frame.data = traits::NkMove(out);
			frame.format = NkPixelFormat::NK_PIXEL_RGBA8;
			frame.stride = w * 4;
			return true;
		}

		if (frame.format == NkPixelFormat::NK_PIXEL_MJPEG) {
			// Décodage MJPEG via NkJPEGCodec (baseline DCT JFIF/Exif).
			// Sortie codec : NK_RGB24 ou NK_GRAY8 — on convertit en RGBA8 ici.
			NkImage *img = NkJPEGCodec::Decode(frame.data.Data(), (usize)frame.data.Size());
			if (!img)
				return false;

			uint32 iw = (uint32)img->Width();
			uint32 ih = (uint32)img->Height();
			int32 channels = img->Channels();
			const uint8 *src = img->Pixels();
			int32 srcStride = img->Stride();

			out.Resize(iw * ih * 4);
			if (channels == 3) {
				for (uint32 y = 0; y < ih; ++y) {
					const uint8 *row = src + (usize)y * srcStride;
					uint8 *dst = out.Data() + (usize)y * iw * 4;
					for (uint32 x = 0; x < iw; ++x) {
						dst[x * 4 + 0] = row[x * 3 + 0];
						dst[x * 4 + 1] = row[x * 3 + 1];
						dst[x * 4 + 2] = row[x * 3 + 2];
						dst[x * 4 + 3] = 255;
					}
				}
			} else if (channels == 1) {
				for (uint32 y = 0; y < ih; ++y) {
					const uint8 *row = src + (usize)y * srcStride;
					uint8 *dst = out.Data() + (usize)y * iw * 4;
					for (uint32 x = 0; x < iw; ++x) {
						uint8 g = row[x];
						dst[x * 4 + 0] = g;
						dst[x * 4 + 1] = g;
						dst[x * 4 + 2] = g;
						dst[x * 4 + 3] = 255;
					}
				}
			} else {
				img->Free();
				return false;
			}
			// Dimensions JPEG peuvent différer du header annoncé : on resync.
			frame.width = iw;
			frame.height = ih;
			img->Free();
			frame.data = traits::NkMove(out);
			frame.format = NkPixelFormat::NK_PIXEL_RGBA8;
			frame.stride = iw * 4;
			return true;
		}

		if (frame.format == NkPixelFormat::NK_PIXEL_NV12) {
			const uint8 *Y = frame.data.Data();
			const uint8 *UV = frame.data.Data() + w * h;
			const bool videoRange = NkFrameUsesVideoRange(frame);
			for (uint32 row = 0; row < h; ++row) {
				for (uint32 col = 0; col < w; ++col) {
					float y = (float)Y[row * w + col];
					float u = (float)UV[(row / 2) * (w) + (col & ~1u)] - 128.f;
					float v = (float)UV[(row / 2) * (w) + (col & ~1u) + 1] - 128.f;
					float r, g, b;
					if (videoRange) {
						y -= 16.f;
						r = y * 1.164f + v * 1.596f;
						g = y * 1.164f - u * 0.391f - v * 0.813f;
						b = y * 1.164f + u * 2.018f;
					} else {
						r = y + 1.402f * v;
						g = y - 0.344136f * u - 0.714136f * v;
						b = y + 1.772f * u;
					}
					uint32 idx = (row * w + col) * 4;
					out[idx + 0] = (uint8)(r < 0 ? 0 : r > 255 ? 255 : r);
					out[idx + 1] = (uint8)(g < 0 ? 0 : g > 255 ? 255 : g);
					out[idx + 2] = (uint8)(b < 0 ? 0 : b > 255 ? 255 : b);
					out[idx + 3] = 255;
				}
			}
			frame.data = traits::NkMove(out);
			frame.format = NkPixelFormat::NK_PIXEL_RGBA8;
			frame.stride = w * 4;
			return true;
		}

		if (frame.format == NkPixelFormat::NK_PIXEL_YUV420) {
			// I420 planar : Y (w*h) + U (w/2 * h/2) + V (w/2 * h/2)
			// C'est le format produit par V4L2 V4L2_PIX_FMT_YUV420 et Android
			// YUV_420_888 (mappé sur I420 quand pixel stride U=V=1). Pour NV21
			// (U/V inversés) l'appelant peut swap les plans U et V avant appel.
			const uint32 ySize = frame.width * frame.height;
			const uint32 uvW = frame.width / 2;
			const uint32 uvH = frame.height / 2;
			const uint32 uvSize = uvW * uvH;
			if (frame.data.Size() < ySize + 2 * uvSize)
				return false;

			const uint8 *Y = frame.data.Data();
			const uint8 *U = Y + ySize;
			const uint8 *V = U + uvSize;
			// La formule vient de la PLAGE déclarée par le producteur, plus du
			// format. En plage pleine — ce qu'émet Android YUV_420_888, et ce
			// qu'il déclare désormais — appliquer la formule de plage réduite
			// (retrancher 16, multiplier par 1,164) étirerait un signal déjà
			// étendu : clairs saturés, sombres bouchés, image délavée. C'est ce
			// que montraient les captures de Rihen, qu'il a résumé d'un mot :
			// « pas coloré réaliste ».
			//   plage pleine  (BT.601 dite « JPEG ») : R = Y + 1,402·(V−128) …
			//   plage réduite (BT.601 dite « TV »)   : R = 1,164·(Y−16) + 1,596·(V−128) …
			const bool videoRange = NkFrameUsesVideoRange(frame);
			for (uint32 row = 0; row < frame.height; ++row) {
				for (uint32 col = 0; col < frame.width; ++col) {
					float y = (float)Y[row * frame.width + col];
					float u = (float)U[(row / 2) * uvW + (col / 2)] - 128.f;
					float v = (float)V[(row / 2) * uvW + (col / 2)] - 128.f;
					float r, g, b;
					if (videoRange) {
						y -= 16.f;
						r = y * 1.164f + v * 1.596f;
						g = y * 1.164f - u * 0.391f - v * 0.813f;
						b = y * 1.164f + u * 2.018f;
					} else {
						r = y + 1.402f * v;
						g = y - 0.344136f * u - 0.714136f * v;
						b = y + 1.772f * u;
					}
					uint32 idx = (row * frame.width + col) * 4;
					out[idx + 0] = (uint8)(r < 0 ? 0 : r > 255 ? 255 : r);
					out[idx + 1] = (uint8)(g < 0 ? 0 : g > 255 ? 255 : g);
					out[idx + 2] = (uint8)(b < 0 ? 0 : b > 255 ? 255 : b);
					out[idx + 3] = 255;
				}
			}
			frame.data = traits::NkMove(out);
			frame.format = NkPixelFormat::NK_PIXEL_RGBA8;
			frame.stride = frame.width * 4;
			return true;
		}

		return false;
	}

	bool NkCameraSystem::ConvertToRGBA8(NkCameraFrame &frame) {
		if (!NkConvertToRGBA8Impl(frame))
			return false;

		// Le miroir est DEMANDÉ par la configuration et porté par la trame ; il
		// s'applique ICI, une fois l'image en RGBA8 — donc quel que soit le
		// format d'origine, sans dupliquer le retournement dans les huit
		// branches de conversion.
		//
		// `flipHorizontal` retombe à faux après coup : convertir deux fois la
		// même trame ne doit pas la retourner deux fois. C'est le genre de
		// double application qui ne se voit pas — une image miroitée deux fois
		// est identique à l'originale, et on conclut que le réglage ne marche
		// pas alors qu'il marche trop.
		if (frame.flipHorizontal) {
			NkMiroirRGBA8(frame);
			frame.flipHorizontal = false;
		}
		return true;
	}

	bool NkCameraSystem::SaveFrameToFile(const NkCameraFrame &frame, const NkString &path, int quality) {
		if (!frame.IsValid())
			return false;
		if (path.Empty())
			return false;

		// Convertir vers RGBA8 si nécessaire (sur une copie pour ne pas
		// muter la frame d'entrée — l'appelant peut vouloir la conserver).
		NkCameraFrame copy = frame;
		if (copy.format != NkPixelFormat::NK_PIXEL_RGBA8) {
			if (!ConvertToRGBA8(copy))
				return false;
		}

		// Wrap les pixels dans un NkImage non-propriétaire et délègue à NKImage
		// qui détecte le format depuis l'extension (.png / .jpg / .bmp / .tga
		// / .qoi / .gif / .ppm / .webp).
		NkImage *img = NkImage::Wrap(const_cast<uint8 *>(copy.data.Data()), (int32)copy.width, (int32)copy.height,
									 NkImagePixelFormat::NK_RGBA32, (int32)copy.stride);
		if (!img)
			return false;

		bool ok = img->Save(path.CStr(), quality);
		img->Free();
		return ok;
	}

	NkString NkCameraSystem::GenerateAutoPath(const NkString &prefix, const NkString &ext) {
		// Horodatage par NKTime, plus par la bibliotheque C. `StampCompact`
		// rend faux si l'heure est illisible et laisse alors une chaine VIDE :
		// on substitue explicitement une valeur reconnaissable plutot que de
		// laisser passer un nom a moitie forme, qui se collisionnerait en
		// silence avec le suivant.
		char ts[16] = {};
		if (!NkSystemClock::StampCompact(ts, sizeof(ts)))
			return prefix + "_00000000_000000." + ext;

		return prefix + "_" + NkString(ts) + "." + ext;
	}

	// ===========================================================================
	// NkMultiCamera::Stream
	// ===========================================================================

	NkMultiCamera::Stream::Stream(uint32 idx) : mDeviceIndex(idx) {
		mBackendReady = mBackend.Init();
		if (mBackendReady)
			mBackend.SetFrameCallback([this](const NkCameraFrame &f) { OnFrame(f); });
	}

	NkMultiCamera::Stream::~Stream() {
		Stop();
		if (mBackendReady)
			mBackend.Shutdown();
	}

	bool NkMultiCamera::Stream::Start(const NkCameraConfig &cfgIn) {
		if (!mBackendReady)
			return false;
		NkCameraConfig cfg = cfgIn;
		cfg.deviceIndex = mDeviceIndex;
		cfg.Resolve();
		mBackend.SetFrameCallback([this](const NkCameraFrame &f) { OnFrame(f); });
		return mBackend.StartStreaming(cfg);
	}

	void NkMultiCamera::Stream::Stop() {
		if (!mBackendReady)
			return;
		mBackend.StopVideoRecord();
		mBackend.StopStreaming();
	}

	void NkMultiCamera::Stream::OnFrame(const NkCameraFrame &f) {
		{
			threading::NkScopedLock<threading::NkMutex> lk(mMutex);
			mLastFrame = f;
			mHasFrame = true;
		}
		if (mQueueEnabled) {
			threading::NkScopedLock<threading::NkMutex> lk(mQueueMutex);
			if (mQueue.Size() >= mMaxQueue)
				mQueue.Pop();
			mQueue.Push(f);
		}
	}

	bool NkMultiCamera::Stream::GetLastFrame(NkCameraFrame &out) {
		threading::NkScopedLock<threading::NkMutex> lk(mMutex);
		if (!mHasFrame)
			return false;
		out = mLastFrame;
		return true;
	}

	bool NkMultiCamera::Stream::DrainFrame(NkCameraFrame &out) {
		threading::NkScopedLock<threading::NkMutex> lk(mQueueMutex);
		if (mQueue.Empty())
			return false;
		out = traits::NkMove(mQueue.Back());
		while (!mQueue.Empty())
			mQueue.Pop();
		return true;
	}

	void NkMultiCamera::Stream::EnableQueue(uint32 sz) {
		threading::NkScopedLock<threading::NkMutex> lk(mQueueMutex);
		mQueueEnabled = true;
		mMaxQueue = sz;
	}

	NkCameraState NkMultiCamera::Stream::GetState() const {
		return mBackendReady ? mBackend.GetState() : NkCameraState::NK_CAM_STATE_CLOSED;
	}

	NkString NkMultiCamera::Stream::GetLastError() const {
		return mBackendReady ? mBackend.GetLastError() : "camera backend init failed";
	}

	bool NkMultiCamera::Stream::CapturePhotoToFile(const NkString &path) {
		if (!mBackendReady)
			return false;

		NkPhotoCaptureResult res;
		if (!mBackend.CapturePhoto(res)) {
			if (!GetLastFrame(res.frame) || !res.frame.IsValid())
				return false;
		}

		NkString outPath = path;
		if (outPath.Empty())
			outPath = NkCameraSystem::GenerateAutoPath(NkString::Fmt("photo_cam{0}", mDeviceIndex), "png");

		return NkCameraSystem::SaveFrameToFile(res.frame, outPath, 90);
	}

	// ===========================================================================
	// NkMultiCamera
	// ===========================================================================

	NkMultiCamera::Stream &NkMultiCamera::Open(uint32 deviceIndex, const NkCameraConfig &config) {
		// Vérifier si déjà ouvert
		for (auto &s : mStreams)
			if (s->DeviceIndex() == deviceIndex)
				return *s;

		auto s = memory::NkMakeUnique<Stream>(deviceIndex);
		s->Start(config);
		mStreams.PushBack(traits::NkMove(s));
		return *mStreams.Back();
	}

	void NkMultiCamera::Close(uint32 deviceIndex) {
		for (usize i = 0; i < mStreams.Size();) {
			if (mStreams[i]->DeviceIndex() == deviceIndex)
				mStreams.Erase(mStreams.begin() + i);
			else
				++i;
		}
	}

	void NkMultiCamera::CloseAll() {
		mStreams.Clear();
	}

	NkMultiCamera::Stream *NkMultiCamera::Get(uint32 deviceIndex) {
		for (auto &s : mStreams)
			if (s->DeviceIndex() == deviceIndex)
				return s.Get();
		return nullptr;
	}

} // namespace nkentseu
