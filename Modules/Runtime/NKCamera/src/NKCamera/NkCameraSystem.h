#pragma once
// =============================================================================
// NkCameraSystem.h â€” FaÃ§ade singleton de capture camÃ©ra + mapping virtuel
//
// MULTI-CAMÃ‰RAS : Chaque backend ouvre UNE camÃ©ra Ã  la fois (identifiÃ©e par
// config.deviceIndex). Pour capturer depuis plusieurs camÃ©ras simultanÃ©ment,
// utiliser NkMultiCamera (voir bas de fichier).
//
// ACCÃˆS Ã€ UNE CAMÃ‰RA PARMI PLUSIEURS :
//   auto devices = cam.EnumerateDevices();
//   // devices[0] = webcam principale
//   // devices[1] = deuxiÃ¨me webcam
//   NkCameraConfig cfg; cfg.deviceIndex = 1; // ouvrir la 2Ã¨me
//   cam.StartStreaming(cfg);
//
// CAMÃ‰RA VIRTUELLE MAPPÃ‰E :
//   cam.SetVirtualCameraMapping(true);
//   cam.SetVirtualCamera(myCamera2D);
//   // Chaque frame : si IMU disponible, mise Ã  jour automatique de myCamera2D
// =============================================================================

#include "INkCameraBackend.h"
#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)
    #include "NKCamera/Backend/NkWin32CameraBackend.h"
#elif defined(NKENTSEU_PLATFORM_MACOS)
    #include "NKCamera/Backend/NkCocoaCameraBackend.h"
#elif defined(NKENTSEU_PLATFORM_IOS)
    #include "NKCamera/Backend/NkUIKitCameraBackend.h"
#elif defined(NKENTSEU_PLATFORM_ANDROID)
    #include "NKCamera/Backend/NkAndroidCameraBackend.h"
#elif defined(NKENTSEU_PLATFORM_LINUX)
    #include "NKCamera/Backend/NkLinuxCameraBackend.h"
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    #include "NKCamera/Backend/NkEmscriptenCameraBackend.h"
#else
    #include "NKCamera/Backend/NkNoopCameraBackend.h"
#endif

#include <memory>
#include <mutex>
#include <queue>
#include <string>

namespace nkentseu
{
    #if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)
        using NkCameraBackend = NkWin32CameraBackend;
    #elif defined(NKENTSEU_PLATFORM_MACOS)
        using NkCameraBackend = NkCocoaCameraBackend;
    #elif defined(NKENTSEU_PLATFORM_IOS)
        using NkCameraBackend = NkUIKitCameraBackend;
    #elif defined(NKENTSEU_PLATFORM_ANDROID)
        using NkCameraBackend = NkAndroidCameraBackend;
    #elif defined(NKENTSEU_PLATFORM_LINUX)
        using NkCameraBackend = NkLinuxCameraBackend;
    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        using NkCameraBackend = NkEmscriptenCameraBackend;
    #else
        using NkCameraBackend = NkNoopCameraBackend;
    #endif

    // Forward dÃ©clarations
    class NkCamera2D;

    // ===========================================================================
    // NkCameraSystem â€” Une camÃ©ra physique Ã  la fois (multi via NkMultiCamera)
    // ===========================================================================

    class NkCameraSystem {
        public:
            // -----------------------------------------------------------------------
            // Singleton
            // -----------------------------------------------------------------------
            static NkCameraSystem& Instance()
            {
                static NkCameraSystem s;
                return s;
            }

            NkCameraSystem(const NkCameraSystem&)            = delete;
            NkCameraSystem& operator=(const NkCameraSystem&) = delete;

            // -----------------------------------------------------------------------
            // Cycle de vie (appelÃ© par NkSystem::Initialise / NkSystem::Close)
            // -----------------------------------------------------------------------
            bool Init();
            void Shutdown();
            bool IsReady() const { return mReady; }

            // -----------------------------------------------------------------------
            // Ã‰numÃ©ration de TOUS les pÃ©riphÃ©riques disponibles
            //
            // Retourne la liste complÃ¨te. Pour en choisir un, passer l'index dans
            // NkCameraConfig::deviceIndex. L'index correspond Ã  devices[i].index.
            // -----------------------------------------------------------------------
            NkVector<NkCameraDevice> EnumerateDevices();
            void SetHotPlugCallback(NkCameraHotPlugCallback cb);

            // -----------------------------------------------------------------------
            // Streaming â€” ouvrir LE pÃ©riphÃ©rique de config.deviceIndex
            //
            // Pour changer de camÃ©ra : StopStreaming() puis StartStreaming(newCfg)
            // -----------------------------------------------------------------------
            bool          StartStreaming(const NkCameraConfig& config = {});
            void          StopStreaming();
            NkCameraState GetState()    const;
            bool          IsStreaming() const;

            void SetFrameCallback(NkFrameCallback cb);

            // Thread-safe â€” copie la derniÃ¨re frame disponible
            bool GetLastFrame(NkCameraFrame& outFrame);

            // Queue thread-safe â€” recommandÃ© dans la boucle principale
            void EnableFrameQueue(uint32 maxQueueSize = 4);
            bool DrainFrameQueue(NkCameraFrame& outFrame);

            // -----------------------------------------------------------------------
            // Capture photo
            // -----------------------------------------------------------------------
            bool        CapturePhoto(NkPhotoCaptureResult& outResult);
            // API conservÃ©e pour compatibilitÃ©, mais l'Ã©criture fichier est dÃ©sactivÃ©e.
            NkString CapturePhotoToFile(const NkString& path = "");

            // -----------------------------------------------------------------------
            // Enregistrement vidÃ©o
            // -----------------------------------------------------------------------
            bool  StartVideoRecord(const NkVideoRecordConfig& config = {});
            void  StopVideoRecord();
            bool  IsRecording()                 const;
            float GetRecordingDurationSeconds() const;

            // -----------------------------------------------------------------------
            // ContrÃ´les camÃ©ra physique
            // -----------------------------------------------------------------------
            bool SetAutoFocus       (bool e);
            bool SetAutoExposure    (bool e);
            bool SetAutoWhiteBalance(bool e);
            bool SetZoom            (float level);   ///< 1.0 = aucun zoom
            bool SetFlash           (bool e);
            bool SetTorch           (bool e);
            bool SetFocusPoint      (float normX, float normY);

            // -----------------------------------------------------------------------
            // Informations session courante
            // -----------------------------------------------------------------------
            uint32         GetWidth()     const;
            uint32         GetHeight()    const;
            uint32         GetFPS()       const;
            NkPixelFormat GetFormat()    const;
            NkString   GetLastError() const;
            uint32         GetCurrentDeviceIndex() const { return mCurrentDeviceIndex; }

            INkCameraBackend* GetBackend() { return &mBackend; }
            const INkCameraBackend* GetBackend() const { return &mBackend; }

            // -----------------------------------------------------------------------
            // MAPPING CAMÃ‰RA VIRTUELLE â† CAMÃ‰RA RÃ‰ELLE (IMU)
            //
            // Quand activÃ©, les donnÃ©es d'orientation IMU de la camÃ©ra physique
            // (gyroscope / accÃ©lÃ©romÃ¨tre) sont utilisÃ©es pour dÃ©placer une NkCamera2D.
            //
            // Usage :
            //   cam.SetVirtualCameraTarget(&myCamera2D);   // lier la camÃ©ra 2D
            //   cam.SetVirtualCameraMapping(true);          // activer le mapping
            //   // Dans la boucle :
            //   cam.UpdateVirtualCamera(dt);                // appliquer les mouvements
            // -----------------------------------------------------------------------

            /// Lie une NkCamera2D cible dont la position/rotation sera pilotÃ©e par l'IMU
            void SetVirtualCameraTarget(NkCamera2D* cam2D);

            /// Active / dÃ©sactive le mapping physique â†’ virtuel
            void SetVirtualCameraMapping(bool enable);
            bool IsVirtualCameraMappingEnabled() const { return mVirtualMappingEnabled; }

            /// ParamÃ¨tres de sensibilitÃ© du mapping
            struct VirtualCameraMapConfig {
                float yawSensitivity   = 1.0f;  ///< SensibilitÃ© rotation gauche/droite
                float pitchSensitivity = 1.0f;  ///< SensibilitÃ© rotation haut/bas
                float translationScale = 0.f;   ///< Translation (0 = rotation seulement)
                bool  invertX          = false;
                bool  invertY          = false;
                bool  smoothing        = true;
                float smoothFactor     = 0.15f; ///< Lerp de lissage (0.05 trÃ¨s lisse, 1.0 instantanÃ©)
            };

            void SetVirtualCameraMapConfig(const VirtualCameraMapConfig& cfg) { mMapConfig = cfg; }
            const VirtualCameraMapConfig& GetVirtualCameraMapConfig() const { return mMapConfig; }

            /// Ã€ appeler chaque frame (boucle principale)
            /// Met Ã  jour la camÃ©ra virtuelle selon l'orientation IMU courante
            void UpdateVirtualCamera(float dt);

            /// Retourne l'orientation IMU courante (si supportÃ©e par le backend)
            bool GetCurrentOrientation(NkCameraOrientation& out) const;

            // -----------------------------------------------------------------------
            // Utilitaires statiques
            // -----------------------------------------------------------------------

            /// Convertit n'importe quel format de frame en RGBA8
            static bool ConvertToRGBA8(NkCameraFrame& frame);

            /// API conservÃ©e pour compatibilitÃ© (retourne false : I/O dÃ©sactivÃ©)
            static bool SaveFrameToFile(const NkCameraFrame& frame,
                                        const NkString& path,
                                        int quality = 90);

            /// GÃ©nÃ¨re un chemin automatique pour sauvegarder une photo
            static NkString GenerateAutoPath(const NkString& prefix,
                                                const NkString& ext);

        private:
            NkCameraSystem() = default;

            void OnFrame(const NkCameraFrame& frame);

            // Backend
            NkCameraBackend mBackend;
            bool            mReady               = false;
            uint32           mCurrentDeviceIndex  = 0;

            // Frame thread-safe
            mutable std::mutex  mFrameMutex;
            NkCameraFrame       mLastFrame;
            bool                mHasFrame    = false;
            NkFrameCallback     mUserCallback;

            // Queue optionnelle
            bool                         mQueueEnabled = false;
            uint32                        mMaxQueueSize = 4;
            std::queue<NkCameraFrame>    mFrameQueue;
            mutable std::mutex           mQueueMutex;

            // Mapping camÃ©ra virtuelle
            NkCamera2D*          mVirtualCamera         = nullptr;
            bool                 mVirtualMappingEnabled = false;
            VirtualCameraMapConfig mMapConfig;

            // Orientation de rÃ©fÃ©rence (lors de l'activation du mapping)
            NkCameraOrientation  mRefOrientation {};
            bool                 mRefCaptured    = false;

            // Orientation lissÃ©e courante
            float mSmoothedYaw   = 0.f;
            float mSmoothedPitch = 0.f;
    };

    // ---------------------------------------------------------------------------
    // Raccourci global
    // ---------------------------------------------------------------------------
    inline NkCameraSystem& NkCamera() { return NkCameraSystem::Instance(); }


    // ===========================================================================
    // NkMultiCamera â€” GÃ©rer PLUSIEURS camÃ©ras physiques simultanÃ©ment
    //
    // Chaque NkMultiCamera::Stream encapsule un backend indÃ©pendant.
    // Utilisation :
    //   NkMultiCamera multi;
    //   auto& s0 = multi.Open(0, cfg0);  // camÃ©ra index 0
    //   auto& s1 = multi.Open(1, cfg1);  // camÃ©ra index 1
    //   NkCameraFrame f0, f1;
    //   s0.GetLastFrame(f0);
    //   s1.GetLastFrame(f1);
    // ===========================================================================

    class NkMultiCamera {
        public:
            // Un stream = une camÃ©ra physique ouverte
            class Stream {
                public:
                    explicit Stream(uint32 deviceIndex);
                    ~Stream();

                    bool          Start(const NkCameraConfig& cfg);
                    void          Stop();
                    bool          GetLastFrame(NkCameraFrame& out);
                    bool          DrainFrame(NkCameraFrame& out);
                    void          EnableQueue(uint32 sz = 4);
                    NkCameraState GetState()  const;
                    uint32         DeviceIndex() const { return mDeviceIndex; }
                    NkString   GetLastError() const;

                    bool CapturePhotoToFile(const NkString& path = "");

                private:
                    void OnFrame(const NkCameraFrame& f);

                    uint32                     mDeviceIndex;
                    NkCameraBackend           mBackend;
                    bool                      mBackendReady = false;
                    mutable std::mutex        mMutex;
                    NkCameraFrame             mLastFrame;
                    bool                      mHasFrame = false;
                    bool                      mQueueEnabled = false;
                    uint32                     mMaxQueue = 4;
                    std::queue<NkCameraFrame> mQueue;
                    mutable std::mutex        mQueueMutex;
            };

            /// Ouvre la camÃ©ra d'index deviceIndex et dÃ©marre le streaming
            Stream& Open(uint32 deviceIndex, const NkCameraConfig& config = {});

            /// Ferme une camÃ©ra par index
            void Close(uint32 deviceIndex);

            /// Ferme toutes les camÃ©ras
            void CloseAll();

            /// AccÃ¨s Ã  un stream par index de device
            Stream* Get(uint32 deviceIndex);

            uint32 Count() const { return static_cast<uint32>(mStreams.Size()); }

        private:
            NkVector<std::unique_ptr<Stream>> mStreams;
    };

} // namespace nkentseu
