// =============================================================================
// NkCameraSystem.cpp — Implémentation complète et fonctionnelle
// =============================================================================

#include "NkCameraSystem.h"
#include "NKCamera/NkCamera2D.h"
#include "NKImage/Core/NkImage.h"
#include "NKImage/Codecs/JPEG/NkJPEGCodec.h"

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

    bool NkCameraSystem::Init()
    {
        if (mReady) return true;
        if (!mBackend.Init()) {
            return false;
        }
        // Câbler le callback interne (thread de capture → OnFrame)
        mBackend.SetFrameCallback([this](const NkCameraFrame& f){ OnFrame(f); });
        mReady = true;
        return true;
    }

    void NkCameraSystem::Shutdown()
    {
        if (!mReady) return;
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

    NkVector<NkCameraDevice> NkCameraSystem::EnumerateDevices()
    {
        if (!mReady) return {};
        return mBackend.EnumerateDevices();
    }

    void NkCameraSystem::SetHotPlugCallback(NkCameraHotPlugCallback cb)
    {
        if (mReady) mBackend.SetHotPlugCallback(std::move(cb));
    }

    // ===========================================================================
    // Streaming
    // ===========================================================================

    bool NkCameraSystem::StartStreaming(const NkCameraConfig& config)
    {
        if (!mReady) return false;
        NkCameraConfig cfg = config;
        cfg.Resolve();
        mCurrentDeviceIndex = cfg.deviceIndex;
        // Recâbler le callback (peut avoir été écrasé lors d'un StopStreaming)
        mBackend.SetFrameCallback([this](const NkCameraFrame& f){ OnFrame(f); });
        return mBackend.StartStreaming(cfg);
    }

    void NkCameraSystem::StopStreaming()
    {
        if (mReady) mBackend.StopStreaming();
    }

    NkCameraState NkCameraSystem::GetState() const
    {
        return mReady ? mBackend.GetState() : NkCameraState::NK_CAM_STATE_CLOSED;
    }

    bool NkCameraSystem::IsStreaming() const
    {
        auto s = GetState();
        return s == NkCameraState::NK_CAM_STATE_STREAMING
            || s == NkCameraState::NK_CAM_STATE_RECORDING;
    }

    void NkCameraSystem::SetFrameCallback(NkFrameCallback cb)
    {
        std::lock_guard<std::mutex> lk(mFrameMutex);
        mUserCallback = std::move(cb);
    }

    bool NkCameraSystem::GetLastFrame(NkCameraFrame& out)
    {
        std::lock_guard<std::mutex> lk(mFrameMutex);
        if (!mHasFrame) return false;
        out = mLastFrame;
        return true;
    }

    void NkCameraSystem::EnableFrameQueue(uint32 maxSize)
    {
        std::lock_guard<std::mutex> lk(mQueueMutex);
        mQueueEnabled = true;
        mMaxQueueSize = maxSize;
    }

    bool NkCameraSystem::DrainFrameQueue(NkCameraFrame& out)
    {
        std::lock_guard<std::mutex> lk(mQueueMutex);
        if (mFrameQueue.empty()) return false;
        out = std::move(mFrameQueue.back());
        while (!mFrameQueue.empty()) mFrameQueue.pop();
        return true;
    }

    // ===========================================================================
    // Capture photo
    // ===========================================================================

    bool NkCameraSystem::CapturePhoto(NkPhotoCaptureResult& out)
    {
        if (!mReady) { out.success = false; out.errorMsg = "Camera not initialised"; return false; }
        return mBackend.CapturePhoto(out);
    }

    NkString NkCameraSystem::CapturePhotoToFile(const NkString& path)
    {
        if (!mReady) return "";

        NkPhotoCaptureResult res;
        if (!mBackend.CapturePhoto(res)) {
            // Fallback : dernière frame du queue/buffer si le backend ne sait
            // pas capturer à la demande (cas de certains backends streaming-only).
            if (!GetLastFrame(res.frame) || !res.frame.IsValid()) return "";
        }

        NkString outPath = path;
        if (outPath.Empty()) outPath = GenerateAutoPath("photo", "png");

        if (!SaveFrameToFile(res.frame, outPath, 90)) return "";
        return outPath;
    }

    // ===========================================================================
    // Enregistrement vidéo
    // ===========================================================================

    bool NkCameraSystem::StartVideoRecord(const NkVideoRecordConfig& config)
    {
        if (!mReady) return false;
        NkVideoRecordConfig cfg = config;
        if (cfg.outputPath.Empty())
            cfg.outputPath = GenerateAutoPath("video", cfg.container);

        // Mode IMAGE_SEQUENCE_ONLY : pris en charge par NkCameraSystem
        // (cross-platform via NKImage). outputPath sert de préfixe
        // ou de dossier — on ajoute "_NNNNNN.ext" pour chaque frame.
        if (cfg.mode == NkVideoRecordConfig::Mode::IMAGE_SEQUENCE_ONLY) {
            std::lock_guard<std::mutex> lk(mFrameMutex);
            mImageSequenceActive  = true;
            mImageSequenceDir     = cfg.outputPath; // préfixe complet attendu
            // Extension par défaut PNG (lossless). L'utilisateur peut forcer
            // jpg via cfg.videoCodec == "jpg" ou cfg.container == "jpg".
            mImageSequenceExt = (cfg.container == "jpg" || cfg.videoCodec == "jpg")
                                 ? NkString("jpg") : NkString("png");
            mImageSequenceQuality = 90;
            mImageSequenceIndex   = 0;
            mImageSequenceStartUs = (uint64)std::time(nullptr) * 1000000ULL;
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

    void  NkCameraSystem::StopVideoRecord()
    {
        if (!mReady) return;
        {
            std::lock_guard<std::mutex> lk(mFrameMutex);
            mImageSequenceActive = false;
            mImageSequenceIndex  = 0;
        }
        mBackend.StopVideoRecord();
    }

    bool  NkCameraSystem::IsRecording()  const
    {
        if (!mReady) return false;
        {
            std::lock_guard<std::mutex> lk(mFrameMutex);
            if (mImageSequenceActive) return true;
        }
        return mBackend.IsRecording();
    }

    float NkCameraSystem::GetRecordingDurationSeconds() const
    {
        if (!mReady) return 0.f;
        {
            std::lock_guard<std::mutex> lk(mFrameMutex);
            if (mImageSequenceActive) {
                uint64 nowUs = (uint64)std::time(nullptr) * 1000000ULL;
                return float((nowUs - mImageSequenceStartUs) / 1000000.0);
            }
        }
        return mBackend.GetRecordingDurationSeconds();
    }

    // ===========================================================================
    // Contrôles
    // ===========================================================================

    bool NkCameraSystem::SetAutoFocus       (bool v) { return mReady && mBackend.SetAutoFocus(v); }
    bool NkCameraSystem::SetAutoExposure    (bool v) { return mReady && mBackend.SetAutoExposure(v); }
    bool NkCameraSystem::SetAutoWhiteBalance(bool v) { return mReady && mBackend.SetAutoWhiteBalance(v); }
    bool NkCameraSystem::SetZoom            (float v){ return mReady && mBackend.SetZoom(v); }
    bool NkCameraSystem::SetFlash           (bool v) { return mReady && mBackend.SetFlash(v); }
    bool NkCameraSystem::SetTorch           (bool v) { return mReady && mBackend.SetTorch(v); }
    bool NkCameraSystem::SetFocusPoint  (float x, float y)
    { return mReady && mBackend.SetFocusPoint(x, y); }

    // ===========================================================================
    // Informations session
    // ===========================================================================

    uint32         NkCameraSystem::GetWidth()     const { return mReady ? mBackend.GetWidth()  : 0; }
    uint32         NkCameraSystem::GetHeight()    const { return mReady ? mBackend.GetHeight() : 0; }
    uint32         NkCameraSystem::GetFPS()       const { return mReady ? mBackend.GetFPS()    : 0; }
    NkPixelFormat NkCameraSystem::GetFormat()    const
    { return mReady ? mBackend.GetFormat() : NkPixelFormat::NK_PIXEL_UNKNOWN; }
    NkString   NkCameraSystem::GetLastError() const
    { return mReady ? mBackend.GetLastError() : "Camera system not initialised"; }

    // ===========================================================================
    // Callback interne — reçoit chaque frame du thread de capture
    // ===========================================================================

    void NkCameraSystem::OnFrame(const NkCameraFrame& frame)
    {
        bool   doSequence = false;
        NkString seqDir, seqExt;
        uint32  seqIdx = 0;
        int32   seqQ   = 90;

        // Mettre à jour la dernière frame et appeler le callback utilisateur
        {
            std::lock_guard<std::mutex> lk(mFrameMutex);
            mLastFrame = frame;
            mHasFrame  = true;
            if (mUserCallback) mUserCallback(frame);

            if (mImageSequenceActive) {
                doSequence = true;
                seqDir = mImageSequenceDir;
                seqExt = mImageSequenceExt;
                seqIdx = mImageSequenceIndex++;
                seqQ   = mImageSequenceQuality;
            }
        }
        // Queue
        if (mQueueEnabled) {
            std::lock_guard<std::mutex> lk(mQueueMutex);
            if (mFrameQueue.size() >= mMaxQueueSize) mFrameQueue.pop();
            mFrameQueue.push(frame);
        }

        // Mode IMAGE_SEQUENCE_ONLY : sauve hors lock (I/O potentiellement lent)
        if (doSequence) {
            char tail[24] = {};
            std::snprintf(tail, sizeof(tail), "_%06u.", seqIdx);
            NkString path = seqDir + NkString(tail) + seqExt;
            (void)SaveFrameToFile(frame, path, seqQ);
        }
    }

    // ===========================================================================
    // MAPPING CAMÉRA VIRTUELLE ← CAMÉRA PHYSIQUE (IMU)
    // ===========================================================================

    void NkCameraSystem::SetVirtualCameraTarget(NkCamera2D* cam2D)
    {
        mVirtualCamera = cam2D;
        mRefCaptured   = false; // réinitialiser la référence
    }

    void NkCameraSystem::SetVirtualCameraMapping(bool enable)
    {
        mVirtualMappingEnabled = enable;
        if (enable) mRefCaptured = false; // prendre une nouvelle référence
    }

    bool NkCameraSystem::GetCurrentOrientation(NkCameraOrientation& out) const
    {
        if (!mReady) return false;
        return const_cast<NkCameraBackend&>(mBackend).GetOrientation(out);
    }

    void NkCameraSystem::UpdateVirtualCamera(float dt)
    {
        (void)dt;
        if (!mVirtualMappingEnabled || !mVirtualCamera || !mReady) return;

        NkCameraOrientation orient;
        if (!mBackend.GetOrientation(orient)) return;

        // Capturer la pose de référence au premier appel
        if (!mRefCaptured) {
            mRefOrientation  = orient;
            mSmoothedYaw     = 0.f;
            mSmoothedPitch   = 0.f;
            mRefCaptured     = true;
            return;
        }

        // Différence par rapport à la référence
        float deltaYaw   = orient.yaw   - mRefOrientation.yaw;
        float deltaPitch = orient.pitch - mRefOrientation.pitch;

        // Inversion optionnelle
        if (mMapConfig.invertX) deltaYaw   = -deltaYaw;
        if (mMapConfig.invertY) deltaPitch = -deltaPitch;

        // Appliquer la sensibilité
        float targetYaw   = deltaYaw   * mMapConfig.yawSensitivity;
        float targetPitch = deltaPitch * mMapConfig.pitchSensitivity;

        // Lissage par interpolation (si activé)
        if (mMapConfig.smoothing) {
            float f = mMapConfig.smoothFactor;
            mSmoothedYaw   += (targetYaw   - mSmoothedYaw)   * f;
            mSmoothedPitch += (targetPitch - mSmoothedPitch) * f;
        } else {
            mSmoothedYaw   = targetYaw;
            mSmoothedPitch = targetPitch;
        }

        // Appliquer à la caméra virtuelle :
        // yaw   → translation horizontale (panoramique horizontal)
        // pitch → translation verticale   (panoramique vertical)
        // roll  → rotation de la caméra   (si souhaité)
        float panX = mSmoothedYaw   * mMapConfig.translationScale;
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

    bool NkCameraSystem::ConvertToRGBA8(NkCameraFrame& frame)
    {
        if (frame.format == NkPixelFormat::NK_PIXEL_RGBA8) return true;
        uint32 w = frame.width, h = frame.height;
        NkVector<uint8> out;
        out.Resize(w * h * 4);

        if (frame.format == NkPixelFormat::NK_PIXEL_BGRA8) {
            for (uint32 i = 0; i < w * h; ++i) {
                out[i*4+0] = frame.data[i*4+2];
                out[i*4+1] = frame.data[i*4+1];
                out[i*4+2] = frame.data[i*4+0];
                out[i*4+3] = frame.data[i*4+3];
            }
            frame.data = std::move(out);
            frame.format = NkPixelFormat::NK_PIXEL_RGBA8;
            frame.stride = w * 4;
            return true;
        }

        if (frame.format == NkPixelFormat::NK_PIXEL_RGB8) {
            for (uint32 i = 0; i < w * h; ++i) {
                out[i*4+0] = frame.data[i*3+0];
                out[i*4+1] = frame.data[i*3+1];
                out[i*4+2] = frame.data[i*3+2];
                out[i*4+3] = 255;
            }
            frame.data = std::move(out);
            frame.format = NkPixelFormat::NK_PIXEL_RGBA8;
            frame.stride = w * 4;
            return true;
        }

        if (frame.format == NkPixelFormat::NK_PIXEL_YUYV) {
            // YUYV packed: Y0 U0 Y1 V0
            for (uint32 i = 0; i < w * h / 2; ++i) {
                float y0 = (float)frame.data[i*4+0] - 16.f;
                float cb = (float)frame.data[i*4+1] - 128.f;
                float y1 = (float)frame.data[i*4+2] - 16.f;
                float cr = (float)frame.data[i*4+3] - 128.f;
                auto cl = [](float v) -> uint8 {
                    return (uint8)(v < 0 ? 0 : v > 255 ? 255 : v);
                };
                out[i*8+0] = cl(y0*1.164f + cr*1.596f);
                out[i*8+1] = cl(y0*1.164f - cb*0.391f - cr*0.813f);
                out[i*8+2] = cl(y0*1.164f + cb*2.018f);
                out[i*8+3] = 255;
                out[i*8+4] = cl(y1*1.164f + cr*1.596f);
                out[i*8+5] = cl(y1*1.164f - cb*0.391f - cr*0.813f);
                out[i*8+6] = cl(y1*1.164f + cb*2.018f);
                out[i*8+7] = 255;
            }
            frame.data = std::move(out);
            frame.format = NkPixelFormat::NK_PIXEL_RGBA8;
            frame.stride = w * 4;
            return true;
        }

        if (frame.format == NkPixelFormat::NK_PIXEL_MJPEG) {
            // Décodage MJPEG via NkJPEGCodec (baseline DCT JFIF/Exif).
            // Sortie codec : NK_RGB24 ou NK_GRAY8 — on convertit en RGBA8 ici.
            NkImage* img = NkJPEGCodec::Decode(frame.data.Data(),
                                                (usize)frame.data.Size());
            if (!img) return false;

            uint32 iw = (uint32)img->Width();
            uint32 ih = (uint32)img->Height();
            int32  channels = img->Channels();
            const uint8* src = img->Pixels();
            int32 srcStride = img->Stride();

            out.Resize(iw * ih * 4);
            if (channels == 3) {
                for (uint32 y = 0; y < ih; ++y) {
                    const uint8* row = src + (usize)y * srcStride;
                    uint8* dst = out.Data() + (usize)y * iw * 4;
                    for (uint32 x = 0; x < iw; ++x) {
                        dst[x*4+0] = row[x*3+0];
                        dst[x*4+1] = row[x*3+1];
                        dst[x*4+2] = row[x*3+2];
                        dst[x*4+3] = 255;
                    }
                }
            } else if (channels == 1) {
                for (uint32 y = 0; y < ih; ++y) {
                    const uint8* row = src + (usize)y * srcStride;
                    uint8* dst = out.Data() + (usize)y * iw * 4;
                    for (uint32 x = 0; x < iw; ++x) {
                        uint8 g = row[x];
                        dst[x*4+0] = g; dst[x*4+1] = g; dst[x*4+2] = g; dst[x*4+3] = 255;
                    }
                }
            } else {
                img->Free();
                return false;
            }
            // Dimensions JPEG peuvent différer du header annoncé : on resync.
            frame.width  = iw;
            frame.height = ih;
            img->Free();
            frame.data = std::move(out);
            frame.format = NkPixelFormat::NK_PIXEL_RGBA8;
            frame.stride = iw * 4;
            return true;
        }

        if (frame.format == NkPixelFormat::NK_PIXEL_NV12) {
            const uint8* Y  = frame.data.Data();
            const uint8* UV = frame.data.Data() + w * h;
            for (uint32 row = 0; row < h; ++row) {
                for (uint32 col = 0; col < w; ++col) {
                    float y  = (float)Y[row * w + col] - 16.f;
                    float u  = (float)UV[(row/2)*(w) + (col & ~1u)]     - 128.f;
                    float v  = (float)UV[(row/2)*(w) + (col & ~1u) + 1] - 128.f;
                    float r  = y * 1.164f + v * 1.596f;
                    float g  = y * 1.164f - u * 0.391f - v * 0.813f;
                    float b  = y * 1.164f + u * 2.018f;
                    uint32 idx = (row * w + col) * 4;
                    out[idx+0] = (uint8)(r < 0 ? 0 : r > 255 ? 255 : r);
                    out[idx+1] = (uint8)(g < 0 ? 0 : g > 255 ? 255 : g);
                    out[idx+2] = (uint8)(b < 0 ? 0 : b > 255 ? 255 : b);
                    out[idx+3] = 255;
                }
            }
            frame.data = std::move(out);
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
            const uint32 uvW   = frame.width  / 2;
            const uint32 uvH   = frame.height / 2;
            const uint32 uvSize = uvW * uvH;
            if (frame.data.Size() < ySize + 2 * uvSize) return false;

            const uint8* Y = frame.data.Data();
            const uint8* U = Y + ySize;
            const uint8* V = U + uvSize;
            for (uint32 row = 0; row < frame.height; ++row) {
                for (uint32 col = 0; col < frame.width; ++col) {
                    float y = (float)Y[row * frame.width + col]    - 16.f;
                    float u = (float)U[(row/2) * uvW + (col/2)]    - 128.f;
                    float v = (float)V[(row/2) * uvW + (col/2)]    - 128.f;
                    float r = y * 1.164f + v * 1.596f;
                    float g = y * 1.164f - u * 0.391f - v * 0.813f;
                    float b = y * 1.164f + u * 2.018f;
                    uint32 idx = (row * frame.width + col) * 4;
                    out[idx+0] = (uint8)(r < 0 ? 0 : r > 255 ? 255 : r);
                    out[idx+1] = (uint8)(g < 0 ? 0 : g > 255 ? 255 : g);
                    out[idx+2] = (uint8)(b < 0 ? 0 : b > 255 ? 255 : b);
                    out[idx+3] = 255;
                }
            }
            frame.data = std::move(out);
            frame.format = NkPixelFormat::NK_PIXEL_RGBA8;
            frame.stride = frame.width * 4;
            return true;
        }

        return false;
    }

    bool NkCameraSystem::SaveFrameToFile(const NkCameraFrame& frame,
                                        const NkString& path, int quality)
    {
        if (!frame.IsValid()) return false;
        if (path.Empty())     return false;

        // Convertir vers RGBA8 si nécessaire (sur une copie pour ne pas
        // muter la frame d'entrée — l'appelant peut vouloir la conserver).
        NkCameraFrame copy = frame;
        if (copy.format != NkPixelFormat::NK_PIXEL_RGBA8) {
            if (!ConvertToRGBA8(copy)) return false;
        }

        // Wrap les pixels dans un NkImage non-propriétaire et délègue à NKImage
        // qui détecte le format depuis l'extension (.png / .jpg / .bmp / .tga
        // / .qoi / .gif / .ppm / .webp).
        NkImage* img = NkImage::Wrap(
            const_cast<uint8*>(copy.data.Data()),
            (int32)copy.width,
            (int32)copy.height,
            NkImagePixelFormat::NK_RGBA32,
            (int32)copy.stride
        );
        if (!img) return false;

        bool ok = img->Save(path.CStr(), quality);
        img->Free();
        return ok;
    }

    NkString NkCameraSystem::GenerateAutoPath(const NkString& prefix,
                                                const NkString& ext)
    {
        const std::time_t t = std::time(nullptr);

        std::tm tmBuf {};
    #if defined(_WIN32)
        localtime_s(&tmBuf, &t);
    #else
        localtime_r(&t, &tmBuf);
    #endif

        char ts[32] = {};
        if (std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &tmBuf) == 0)
            std::snprintf(ts, sizeof(ts), "00000000_000000");

        return prefix + "_" + NkString(ts) + "." + ext;
    }

    // ===========================================================================
    // NkMultiCamera::Stream
    // ===========================================================================

    NkMultiCamera::Stream::Stream(uint32 idx) : mDeviceIndex(idx)
    {
        mBackendReady = mBackend.Init();
        if (mBackendReady)
            mBackend.SetFrameCallback([this](const NkCameraFrame& f){ OnFrame(f); });
    }

    NkMultiCamera::Stream::~Stream()
    {
        Stop();
        if (mBackendReady) mBackend.Shutdown();
    }

    bool NkMultiCamera::Stream::Start(const NkCameraConfig& cfgIn)
    {
        if (!mBackendReady) return false;
        NkCameraConfig cfg = cfgIn;
        cfg.deviceIndex = mDeviceIndex;
        cfg.Resolve();
        mBackend.SetFrameCallback([this](const NkCameraFrame& f){ OnFrame(f); });
        return mBackend.StartStreaming(cfg);
    }

    void NkMultiCamera::Stream::Stop()
    {
        if (!mBackendReady) return;
        mBackend.StopVideoRecord();
        mBackend.StopStreaming();
    }

    void NkMultiCamera::Stream::OnFrame(const NkCameraFrame& f)
    {
        { std::lock_guard<std::mutex> lk(mMutex); mLastFrame = f; mHasFrame = true; }
        if (mQueueEnabled) {
            std::lock_guard<std::mutex> lk(mQueueMutex);
            if (mQueue.size() >= mMaxQueue) mQueue.pop();
            mQueue.push(f);
        }
    }

    bool NkMultiCamera::Stream::GetLastFrame(NkCameraFrame& out)
    {
        std::lock_guard<std::mutex> lk(mMutex);
        if (!mHasFrame) return false;
        out = mLastFrame; return true;
    }

    bool NkMultiCamera::Stream::DrainFrame(NkCameraFrame& out)
    {
        std::lock_guard<std::mutex> lk(mQueueMutex);
        if (mQueue.empty()) return false;
        out = std::move(mQueue.back());
        while (!mQueue.empty()) mQueue.pop();
        return true;
    }

    void NkMultiCamera::Stream::EnableQueue(uint32 sz)
    {
        std::lock_guard<std::mutex> lk(mQueueMutex);
        mQueueEnabled = true; mMaxQueue = sz;
    }

    NkCameraState NkMultiCamera::Stream::GetState() const
    { return mBackendReady ? mBackend.GetState() : NkCameraState::NK_CAM_STATE_CLOSED; }

    NkString NkMultiCamera::Stream::GetLastError() const
    { return mBackendReady ? mBackend.GetLastError() : "camera backend init failed"; }

    bool NkMultiCamera::Stream::CapturePhotoToFile(const NkString& path)
    {
        if (!mBackendReady) return false;

        NkPhotoCaptureResult res;
        if (!mBackend.CapturePhoto(res)) {
            if (!GetLastFrame(res.frame) || !res.frame.IsValid()) return false;
        }

        NkString outPath = path;
        if (outPath.Empty())
            outPath = NkCameraSystem::GenerateAutoPath(
                NkString::Fmt("photo_cam{0}", mDeviceIndex), "png");

        return NkCameraSystem::SaveFrameToFile(res.frame, outPath, 90);
    }

    // ===========================================================================
    // NkMultiCamera
    // ===========================================================================

    NkMultiCamera::Stream& NkMultiCamera::Open(uint32 deviceIndex,
                                                const NkCameraConfig& config)
    {
        // Vérifier si déjà ouvert
        for (auto& s : mStreams)
            if (s->DeviceIndex() == deviceIndex)
                return *s;

        auto s = std::make_unique<Stream>(deviceIndex);
        s->Start(config);
        mStreams.PushBack(std::move(s));
        return *mStreams.Back();
    }

    void NkMultiCamera::Close(uint32 deviceIndex)
    {
        for (usize i = 0; i < mStreams.Size(); ) {
            if (mStreams[i]->DeviceIndex() == deviceIndex)
                mStreams.Erase(mStreams.begin() + i);
            else
                ++i;
        }
    }

    void NkMultiCamera::CloseAll() { mStreams.Clear(); }

    NkMultiCamera::Stream* NkMultiCamera::Get(uint32 deviceIndex)
    {
        for (auto& s : mStreams)
            if (s->DeviceIndex() == deviceIndex) return s.get();
        return nullptr;
    }

} // namespace nkentseu
