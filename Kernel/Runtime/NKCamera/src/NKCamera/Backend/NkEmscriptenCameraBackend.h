#pragma once
// =============================================================================
// NkEmscriptenCameraBackend.h — Capture caméra Web via getUserMedia
//
// Pipeline :
//   navigator.mediaDevices.getUserMedia({ video: { ... } })
//     -> <video> caché srcObject = stream
//     -> drawImage(video) sur <canvas> à fps cible
//     -> ctx.getImageData(...).data (Uint8ClampedArray RGBA)
//     -> Module.HEAPU8.set(...) vers buffer WASM
//     -> appel C export _nkcam_em_on_frame(w, h, ts)
//
// Limitations : pas de thread (single main loop), pas d'IMU, pas
// d'enregistrement vidéo natif (utiliser IMAGE_SEQUENCE_ONLY si besoin).
// HTTPS requis pour getUserMedia (sauf localhost).
// =============================================================================

#include "NKCamera/NKICameraBackend.h"

#ifdef __EMSCRIPTEN__
    #include <emscripten/emscripten.h>
    #include <emscripten/html5.h>
#endif

#include <mutex>
#include <vector>

namespace nkentseu {

    class NkEmscriptenCameraBackend : public NKICameraBackend {
        public:
            NkEmscriptenCameraBackend() { sInstance = this; }
            ~NkEmscriptenCameraBackend() override { Shutdown(); if (sInstance == this) sInstance = nullptr; }

            // -- Cycle de vie ----------------------------------------------------------
            bool Init() override {
                mLastError.Clear();
            #ifdef __EMSCRIPTEN__
                if (EmJsCheckSupport() == 0) {
                    mLastError = "navigator.mediaDevices.getUserMedia non disponible (HTTPS requis ?)";
                    return false;
                }
                EmJsSetup();
                return true;
            #else
                mLastError = "Backend Emscripten compile hors Emscripten";
                return false;
            #endif
            }

            void Shutdown() override { StopStreaming(); }

            // -- Énumération -----------------------------------------------------------
            NkVector<NkCameraDevice> EnumerateDevices() override {
                NkVector<NkCameraDevice> list;
            #ifdef __EMSCRIPTEN__
                int32 n = EmJsEnumerateCount();
                for (int32 i = 0; i < n; ++i) {
                    NkCameraDevice d;
                    d.index = (uint32)i;
                    char buf[256] = {0};
                    EmJsEnumerateName(i, buf, sizeof(buf));
                    d.name = NkString(buf);
                    d.id   = d.name;
                    // Modes par défaut : VGA / HD / FHD (JS ne révèle pas les modes
                    // tant que getUserMedia n'a pas négocié — c'est une limite Web).
                    NkCameraDevice::Mode mVGA{640,480,30,NkPixelFormat::NK_PIXEL_RGBA8};
                    NkCameraDevice::Mode mHD {1280,720,30,NkPixelFormat::NK_PIXEL_RGBA8};
                    NkCameraDevice::Mode mFHD{1920,1080,30,NkPixelFormat::NK_PIXEL_RGBA8};
                    d.modes.PushBack(mVGA);
                    d.modes.PushBack(mHD);
                    d.modes.PushBack(mFHD);
                    list.PushBack(d);
                }
            #endif
                return list;
            }

            void SetHotPlugCallback(NkCameraHotPlugCallback cb) override { mHotPlug = std::move(cb); }

            // -- Streaming -------------------------------------------------------------
            bool StartStreaming(const NkCameraConfig& configIn) override {
                NkCameraConfig cfg = configIn; cfg.Resolve();
            #ifdef __EMSCRIPTEN__
                {
                    std::lock_guard<std::mutex> lk(mMutex);
                    mFrameBuffer.assign((size_t)cfg.width * cfg.height * 4, 0);
                }
                mWidth  = cfg.width;
                mHeight = cfg.height;
                mFps    = cfg.fps;

                int32 ok = EmJsStart((int32)cfg.deviceIndex,
                                      (int32)cfg.width, (int32)cfg.height,
                                      (int32)cfg.fps,
                                      (int32)(intptr_t)mFrameBuffer.data(),
                                      (int32)mFrameBuffer.size());
                if (!ok) {
                    mState = NkCameraState::NK_CAM_STATE_ERROR;
                    mLastError = "getUserMedia: refus utilisateur ou device introuvable";
                    return false;
                }
                mState = NkCameraState::NK_CAM_STATE_STREAMING;
                return true;
            #else
                (void)cfg;
                mState = NkCameraState::NK_CAM_STATE_ERROR;
                return false;
            #endif
            }

            void StopStreaming() override {
            #ifdef __EMSCRIPTEN__
                EmJsStop();
            #endif
                mState = NkCameraState::NK_CAM_STATE_CLOSED;
                std::lock_guard<std::mutex> lk(mMutex);
                mHasFrame = false;
            }

            NkCameraState GetState() const override { return mState; }

            void SetFrameCallback(NkFrameCallback cb) override { mFrameCb = std::move(cb); }

            bool GetLastFrame(NkCameraFrame& out) override {
                std::lock_guard<std::mutex> lk(mMutex);
                if (!mHasFrame) return false;
                out = mLastFrame;
                return true;
            }

            // -- Photo / vidéo ---------------------------------------------------------
            bool CapturePhoto(NkPhotoCaptureResult& out) override {
                std::lock_guard<std::mutex> lk(mMutex);
                if (!mHasFrame) { out.success = false; out.errorMsg = "no frame yet"; return false; }
                out.frame = mLastFrame;
                out.success = true;
                return true;
            }

            bool CapturePhotoToFile(const NkString&) override {
                mLastError = "Emscripten ne supporte pas l'ecriture fichier directe — "
                              "utiliser SaveFrameToFile + FS.syncfs ou download.";
                return false;
            }

            // Pas de recording vidéo natif Web (MediaRecorder API hors scope ici).
            // L'appelant utilisera NkCameraSystem en mode IMAGE_SEQUENCE_ONLY si besoin.
            bool  StartVideoRecord(const NkVideoRecordConfig&) override { return false; }
            void  StopVideoRecord() override {}
            bool  IsRecording() const override { return false; }
            float GetRecordingDurationSeconds() const override { return 0.f; }

            // -- Info session ----------------------------------------------------------
            uint32         GetWidth()     const override { return mWidth; }
            uint32         GetHeight()    const override { return mHeight; }
            uint32         GetFPS()       const override { return mFps; }
            NkPixelFormat GetFormat()    const override { return NkPixelFormat::NK_PIXEL_RGBA8; }
            NkString   GetLastError() const override { return mLastError; }

            // -- Pump interne appelé par JS (export C) ---------------------------------
            void EmOnFrame(uint32 w, uint32 h, uint64 tsUs) {
                NkCameraFrame f;
                f.width  = w;
                f.height = h;
                f.format = NkPixelFormat::NK_PIXEL_RGBA8;
                f.stride = w * 4;
                f.timestampUs = tsUs;
                {
                    std::lock_guard<std::mutex> lk(mMutex);
                    const usize need = (usize)w * h * 4;
                    if (mFrameBuffer.size() < need) return;
                    f.data.Resize((uint32)need);
                    std::memcpy(f.data.Data(), mFrameBuffer.data(), need);
                    f.frameIndex = ++mFrameCounter;
                    mLastFrame   = f;
                    mHasFrame    = true;
                }
                if (mFrameCb) mFrameCb(f);
            }

            static NkEmscriptenCameraBackend* sInstance;

        private:
        #ifdef __EMSCRIPTEN__
            // ---- Pont JS <-> C ---------------------------------------------------
            static int32 EmJsCheckSupport();
            static void  EmJsSetup();
            static int32 EmJsEnumerateCount();
            static void  EmJsEnumerateName(int32 idx, char* out, int32 cap);
            static int32 EmJsStart(int32 deviceIdx, int32 w, int32 h, int32 fps,
                                    int32 bufPtr, int32 bufSize);
            static void  EmJsStop();
        #endif

            NkCameraState           mState     = NkCameraState::NK_CAM_STATE_CLOSED;
            NkString                mLastError;
            NkFrameCallback         mFrameCb;
            NkCameraHotPlugCallback mHotPlug;

            uint32 mWidth = 0, mHeight = 0, mFps = 0;
            mutable std::mutex mMutex;
            std::vector<uint8> mFrameBuffer;
            NkCameraFrame      mLastFrame;
            bool               mHasFrame    = false;
            uint32             mFrameCounter = 0;
    };

    // Définition du singleton de routage JS → C
    inline NkEmscriptenCameraBackend* NkEmscriptenCameraBackend::sInstance = nullptr;

#ifdef __EMSCRIPTEN__

    // ===========================================================================
    // Pont JS (EM_JS) + export C (EMSCRIPTEN_KEEPALIVE)
    // ===========================================================================

    EM_JS(int, nkentseu_emcam_supported, (), {
        return (navigator && navigator.mediaDevices && navigator.mediaDevices.getUserMedia) ? 1 : 0;
    });

    EM_JS(void, nkentseu_emcam_setup, (), {
        if (Module.nkcam) return;
        Module.nkcam = {
            stream:    null,
            video:     document.createElement('video'),
            canvas:    document.createElement('canvas'),
            ctx:       null,
            intervalId:0,
            bufPtr:    0,
            bufSize:   0,
            w:         0,
            h:         0,
            devices:   []
        };
        Module.nkcam.video.autoplay = true;
        Module.nkcam.video.playsInline = true;
        Module.nkcam.video.muted = true;
        Module.nkcam.ctx = Module.nkcam.canvas.getContext('2d', { willReadFrequently: true });
        // Lazy enumerate devices (peut échouer si permission pas accordée).
        navigator.mediaDevices.enumerateDevices().then((devs) => {
            Module.nkcam.devices = devs.filter(d => d.kind === 'videoinput');
        }).catch(() => { Module.nkcam.devices = []; });
    });

    EM_JS(int, nkentseu_emcam_enum_count, (), {
        return Module.nkcam ? Module.nkcam.devices.length : 0;
    });

    EM_JS(void, nkentseu_emcam_enum_name, (int idx, int outPtr, int cap), {
        if (!Module.nkcam) { return; }
        var d = Module.nkcam.devices[idx];
        var name = d && d.label ? d.label : ('Camera ' + idx);
        // stringToUTF8 termine par '\0'.
        stringToUTF8(name, outPtr, cap);
    });

    EM_JS(int, nkentseu_emcam_start, (int devIdx, int w, int h, int fps,
                                       int bufPtr, int bufSize), {
        if (!Module.nkcam) return 0;
        var nk = Module.nkcam;
        nk.bufPtr  = bufPtr;
        nk.bufSize = bufSize;
        nk.w       = w;
        nk.h       = h;
        nk.canvas.width  = w;
        nk.canvas.height = h;
        var constraints = {
            video: {
                width:  { ideal: w },
                height: { ideal: h }
            }
        };
        var devs = nk.devices || [];
        if (devs[devIdx] && devs[devIdx].deviceId) {
            constraints.video.deviceId = { exact: devs[devIdx].deviceId };
        }
        return Asyncify.handleAsync(async () => {
            try {
                var stream = await navigator.mediaDevices.getUserMedia(constraints);
                nk.stream = stream;
                nk.video.srcObject = stream;
                await nk.video.play();
                // Réajuster canvas aux dimensions réelles si différentes
                if (nk.video.videoWidth > 0) {
                    nk.canvas.width  = nk.video.videoWidth;
                    nk.canvas.height = nk.video.videoHeight;
                    nk.w = nk.video.videoWidth;
                    nk.h = nk.video.videoHeight;
                }
                // Pump : drawImage + getImageData + invoke C export
                var interval = Math.max(1, Math.floor(1000 / fps));
                nk.intervalId = setInterval(function() {
                    if (!nk.video.videoWidth) return;
                    nk.ctx.drawImage(nk.video, 0, 0, nk.canvas.width, nk.canvas.height);
                    var img = nk.ctx.getImageData(0, 0, nk.canvas.width, nk.canvas.height);
                    var need = nk.canvas.width * nk.canvas.height * 4;
                    if (need <= nk.bufSize) {
                        Module.HEAPU8.set(img.data, nk.bufPtr);
                        var tsUs = Math.floor(performance.now() * 1000);
                        Module._nkentseu_emcam_on_frame(nk.canvas.width, nk.canvas.height,
                                                         tsUs >>> 0, (tsUs / 0x100000000) >>> 0);
                    }
                }, interval);
                return 1;
            } catch (e) {
                console.error('[NkEmscriptenCameraBackend] getUserMedia error:', e);
                return 0;
            }
        });
    });

    EM_JS(void, nkentseu_emcam_stop, (), {
        if (!Module.nkcam) return;
        var nk = Module.nkcam;
        if (nk.intervalId) { clearInterval(nk.intervalId); nk.intervalId = 0; }
        if (nk.stream) {
            nk.stream.getTracks().forEach(function(t) { t.stop(); });
            nk.stream = null;
        }
        nk.video.srcObject = null;
    });

    // Définitions des trampolines C++ → JS appelables depuis l'objet
    inline int32 NkEmscriptenCameraBackend::EmJsCheckSupport()       { return nkentseu_emcam_supported(); }
    inline void  NkEmscriptenCameraBackend::EmJsSetup()              { nkentseu_emcam_setup(); }
    inline int32 NkEmscriptenCameraBackend::EmJsEnumerateCount()     { return nkentseu_emcam_enum_count(); }
    inline void  NkEmscriptenCameraBackend::EmJsEnumerateName(int32 idx, char* out, int32 cap) {
        nkentseu_emcam_enum_name(idx, (int)(intptr_t)out, cap);
    }
    inline int32 NkEmscriptenCameraBackend::EmJsStart(int32 d, int32 w, int32 h, int32 fps,
                                                       int32 bufPtr, int32 bufSize) {
        return nkentseu_emcam_start(d, w, h, fps, bufPtr, bufSize);
    }
    inline void  NkEmscriptenCameraBackend::EmJsStop()               { nkentseu_emcam_stop(); }

    // Export C appelé par le pump setInterval JS. Doit être visible à l'édition
    // de lien via EMSCRIPTEN_KEEPALIVE.
    extern "C" EMSCRIPTEN_KEEPALIVE
    void nkentseu_emcam_on_frame(int w, int h, unsigned int tsLow, unsigned int tsHigh) {
        if (!NkEmscriptenCameraBackend::sInstance) return;
        uint64 ts = ((uint64)tsHigh << 32) | (uint64)tsLow;
        NkEmscriptenCameraBackend::sInstance->EmOnFrame((uint32)w, (uint32)h, ts);
    }

#endif // __EMSCRIPTEN__

} // namespace nkentseu
