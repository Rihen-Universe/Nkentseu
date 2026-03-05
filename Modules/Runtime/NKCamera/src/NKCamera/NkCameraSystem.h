#pragma once
// =============================================================================
// NkCameraSystem.h — Façade singleton de capture caméra + mapping virtuel
//
// MULTI-CAMÉRAS : Chaque backend ouvre UNE caméra à la fois (identifiée par
// config.deviceIndex). Pour capturer depuis plusieurs caméras simultanément,
// utiliser NkMultiCamera (voir bas de fichier).
//
// ACCÈS À UNE CAMÉRA PARMI PLUSIEURS :
//   auto devices = cam.EnumerateDevices();
//   // devices[0] = webcam principale
//   // devices[1] = deuxième webcam
//   NkCameraConfig cfg; cfg.deviceIndex = 1; // ouvrir la 2ème
//   cam.StartStreaming(cfg);
//
// CAMÉRA VIRTUELLE MAPPÉE :
//   cam.SetVirtualCameraMapping(true);
//   cam.SetVirtualCamera(myCamera2D);
//   // Chaque frame : si IMU disponible, mise à jour automatique de myCamera2D
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
#elif defined(NKENTSEU_PLATFORM_WEB) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN) || defined(__EMSCRIPTEN__)
    #include "NKCamera/Backend/NkWASMCameraBackend.h"
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
    #elif defined(NKENTSEU_PLATFORM_WEB) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN) || defined(__EMSCRIPTEN__)
        using NkCameraBackend = NkWASMCameraBackend;
    #else
        using NkCameraBackend = NkNoopCameraBackend;
    #endif

    // Forward déclarations
    class NkCamera2D;

    // ===========================================================================
    // NkCameraSystem — Une caméra physique à la fois (multi via NkMultiCamera)
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
            // Cycle de vie (appelé par NkSystem::Initialise / NkSystem::Close)
            // -----------------------------------------------------------------------
            bool Init();
            void Shutdown();
            bool IsReady() const { return mReady; }

            // -----------------------------------------------------------------------
            // Énumération de TOUS les périphériques disponibles
            //
            // Retourne la liste complète. Pour en choisir un, passer l'index dans
            // NkCameraConfig::deviceIndex. L'index correspond à devices[i].index.
            // -----------------------------------------------------------------------
            std::vector<NkCameraDevice> EnumerateDevices();
            void SetHotPlugCallback(NkCameraHotPlugCallback cb);

            // -----------------------------------------------------------------------
            // Streaming — ouvrir LE périphérique de config.deviceIndex
            //
            // Pour changer de caméra : StopStreaming() puis StartStreaming(newCfg)
            // -----------------------------------------------------------------------
            bool          StartStreaming(const NkCameraConfig& config = {});
            void          StopStreaming();
            NkCameraState GetState()    const;
            bool          IsStreaming() const;

            void SetFrameCallback(NkFrameCallback cb);

            // Thread-safe — copie la dernière frame disponible
            bool GetLastFrame(NkCameraFrame& outFrame);

            // Queue thread-safe — recommandé dans la boucle principale
            void EnableFrameQueue(NkU32 maxQueueSize = 4);
            bool DrainFrameQueue(NkCameraFrame& outFrame);

            // -----------------------------------------------------------------------
            // Capture photo
            // -----------------------------------------------------------------------
            bool        CapturePhoto(NkPhotoCaptureResult& outResult);
            // API conservée pour compatibilité, mais l'écriture fichier est désactivée.
            std::string CapturePhotoToFile(const std::string& path = "");

            // -----------------------------------------------------------------------
            // Enregistrement vidéo
            // -----------------------------------------------------------------------
            bool  StartVideoRecord(const NkVideoRecordConfig& config = {});
            void  StopVideoRecord();
            bool  IsRecording()                 const;
            float GetRecordingDurationSeconds() const;

            // -----------------------------------------------------------------------
            // Contrôles caméra physique
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
            NkU32         GetWidth()     const;
            NkU32         GetHeight()    const;
            NkU32         GetFPS()       const;
            NkPixelFormat GetFormat()    const;
            std::string   GetLastError() const;
            NkU32         GetCurrentDeviceIndex() const { return mCurrentDeviceIndex; }

            INkCameraBackend* GetBackend() { return &mBackend; }
            const INkCameraBackend* GetBackend() const { return &mBackend; }

            // -----------------------------------------------------------------------
            // MAPPING CAMÉRA VIRTUELLE ← CAMÉRA RÉELLE (IMU)
            //
            // Quand activé, les données d'orientation IMU de la caméra physique
            // (gyroscope / accéléromètre) sont utilisées pour déplacer une NkCamera2D.
            //
            // Usage :
            //   cam.SetVirtualCameraTarget(&myCamera2D);   // lier la caméra 2D
            //   cam.SetVirtualCameraMapping(true);          // activer le mapping
            //   // Dans la boucle :
            //   cam.UpdateVirtualCamera(dt);                // appliquer les mouvements
            // -----------------------------------------------------------------------

            /// Lie une NkCamera2D cible dont la position/rotation sera pilotée par l'IMU
            void SetVirtualCameraTarget(NkCamera2D* cam2D);

            /// Active / désactive le mapping physique → virtuel
            void SetVirtualCameraMapping(bool enable);
            bool IsVirtualCameraMappingEnabled() const { return mVirtualMappingEnabled; }

            /// Paramètres de sensibilité du mapping
            struct VirtualCameraMapConfig {
                float yawSensitivity   = 1.0f;  ///< Sensibilité rotation gauche/droite
                float pitchSensitivity = 1.0f;  ///< Sensibilité rotation haut/bas
                float translationScale = 0.f;   ///< Translation (0 = rotation seulement)
                bool  invertX          = false;
                bool  invertY          = false;
                bool  smoothing        = true;
                float smoothFactor     = 0.15f; ///< Lerp de lissage (0.05 très lisse, 1.0 instantané)
            };

            void SetVirtualCameraMapConfig(const VirtualCameraMapConfig& cfg) { mMapConfig = cfg; }
            const VirtualCameraMapConfig& GetVirtualCameraMapConfig() const { return mMapConfig; }

            /// À appeler chaque frame (boucle principale)
            /// Met à jour la caméra virtuelle selon l'orientation IMU courante
            void UpdateVirtualCamera(float dt);

            /// Retourne l'orientation IMU courante (si supportée par le backend)
            bool GetCurrentOrientation(NkCameraOrientation& out) const;

            // -----------------------------------------------------------------------
            // Utilitaires statiques
            // -----------------------------------------------------------------------

            /// Convertit n'importe quel format de frame en RGBA8
            static bool ConvertToRGBA8(NkCameraFrame& frame);

            /// API conservée pour compatibilité (retourne false : I/O désactivé)
            static bool SaveFrameToFile(const NkCameraFrame& frame,
                                        const std::string& path,
                                        int quality = 90);

            /// Génère un chemin automatique pour sauvegarder une photo
            static std::string GenerateAutoPath(const std::string& prefix,
                                                const std::string& ext);

        private:
            NkCameraSystem() = default;

            void OnFrame(const NkCameraFrame& frame);

            // Backend
            NkCameraBackend mBackend;
            bool            mReady               = false;
            NkU32           mCurrentDeviceIndex  = 0;

            // Frame thread-safe
            mutable std::mutex  mFrameMutex;
            NkCameraFrame       mLastFrame;
            bool                mHasFrame    = false;
            NkFrameCallback     mUserCallback;

            // Queue optionnelle
            bool                         mQueueEnabled = false;
            NkU32                        mMaxQueueSize = 4;
            std::queue<NkCameraFrame>    mFrameQueue;
            mutable std::mutex           mQueueMutex;

            // Mapping caméra virtuelle
            NkCamera2D*          mVirtualCamera         = nullptr;
            bool                 mVirtualMappingEnabled = false;
            VirtualCameraMapConfig mMapConfig;

            // Orientation de référence (lors de l'activation du mapping)
            NkCameraOrientation  mRefOrientation {};
            bool                 mRefCaptured    = false;

            // Orientation lissée courante
            float mSmoothedYaw   = 0.f;
            float mSmoothedPitch = 0.f;
    };

    // ---------------------------------------------------------------------------
    // Raccourci global
    // ---------------------------------------------------------------------------
    inline NkCameraSystem& NkCamera() { return NkCameraSystem::Instance(); }


    // ===========================================================================
    // NkMultiCamera — Gérer PLUSIEURS caméras physiques simultanément
    //
    // Chaque NkMultiCamera::Stream encapsule un backend indépendant.
    // Utilisation :
    //   NkMultiCamera multi;
    //   auto& s0 = multi.Open(0, cfg0);  // caméra index 0
    //   auto& s1 = multi.Open(1, cfg1);  // caméra index 1
    //   NkCameraFrame f0, f1;
    //   s0.GetLastFrame(f0);
    //   s1.GetLastFrame(f1);
    // ===========================================================================

    class NkMultiCamera {
        public:
            // Un stream = une caméra physique ouverte
            class Stream {
                public:
                    explicit Stream(NkU32 deviceIndex);
                    ~Stream();

                    bool          Start(const NkCameraConfig& cfg);
                    void          Stop();
                    bool          GetLastFrame(NkCameraFrame& out);
                    bool          DrainFrame(NkCameraFrame& out);
                    void          EnableQueue(NkU32 sz = 4);
                    NkCameraState GetState()  const;
                    NkU32         DeviceIndex() const { return mDeviceIndex; }
                    std::string   GetLastError() const;

                    bool CapturePhotoToFile(const std::string& path = "");

                private:
                    void OnFrame(const NkCameraFrame& f);

                    NkU32                     mDeviceIndex;
                    NkCameraBackend           mBackend;
                    bool                      mBackendReady = false;
                    mutable std::mutex        mMutex;
                    NkCameraFrame             mLastFrame;
                    bool                      mHasFrame = false;
                    bool                      mQueueEnabled = false;
                    NkU32                     mMaxQueue = 4;
                    std::queue<NkCameraFrame> mQueue;
                    mutable std::mutex        mQueueMutex;
            };

            /// Ouvre la caméra d'index deviceIndex et démarre le streaming
            Stream& Open(NkU32 deviceIndex, const NkCameraConfig& config = {});

            /// Ferme une caméra par index
            void Close(NkU32 deviceIndex);

            /// Ferme toutes les caméras
            void CloseAll();

            /// Accès à un stream par index de device
            Stream* Get(NkU32 deviceIndex);

            NkU32 Count() const { return static_cast<NkU32>(mStreams.size()); }

        private:
            std::vector<std::unique_ptr<Stream>> mStreams;
    };

} // namespace nkentseu
