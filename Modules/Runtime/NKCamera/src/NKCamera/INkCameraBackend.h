#pragma once
// =============================================================================
// INkCameraBackend.h — Contrat backend de capture caméra physique
// VERSION CORRIGÉE — sélection multi-caméras propre
// =============================================================================

#include "NkCameraTypes.h"

namespace nkentseu {

    class INkCameraBackend {
        public:
            virtual ~INkCameraBackend() = default;

            // -----------------------------------------------------------------------
            // Cycle de vie
            // -----------------------------------------------------------------------
            virtual bool Init()     = 0;
            virtual void Shutdown() = 0;

            // -----------------------------------------------------------------------
            // Énumération — retourne TOUS les périphériques disponibles
            // -----------------------------------------------------------------------
            virtual std::vector<NkCameraDevice> EnumerateDevices() = 0;

            /// Callback hot-plug (brancher/débrancher une webcam USB)
            virtual void SetHotPlugCallback(NkCameraHotPlugCallback cb) = 0;

            // -----------------------------------------------------------------------
            // Session — ouvrir LE périphérique identifié par config.deviceIndex
            // -----------------------------------------------------------------------
            virtual bool StartStreaming(const NkCameraConfig& config) = 0;
            virtual void StopStreaming()  = 0;
            virtual NkCameraState GetState() const = 0;

            virtual void SetFrameCallback(NkFrameCallback cb)    = 0;
            virtual bool GetLastFrame(NkCameraFrame& outFrame)   = 0;

            // -----------------------------------------------------------------------
            // Capture photo
            // -----------------------------------------------------------------------
            virtual bool CapturePhoto(NkPhotoCaptureResult& out) = 0;
            virtual bool CapturePhotoToFile(const std::string& path) = 0;

            // -----------------------------------------------------------------------
            // Enregistrement vidéo
            // -----------------------------------------------------------------------
            virtual bool  StartVideoRecord(const NkVideoRecordConfig& config) = 0;
            virtual void  StopVideoRecord()  = 0;
            virtual bool  IsRecording()      const = 0;
            virtual float GetRecordingDurationSeconds() const = 0;

            // -----------------------------------------------------------------------
            // Contrôles (implémentation optionnelle — retourne false si non supporté)
            // -----------------------------------------------------------------------
            virtual bool SetAutoFocus       (bool e)          { (void)e; return false; }
            virtual bool SetAutoExposure    (bool e)          { (void)e; return false; }
            virtual bool SetAutoWhiteBalance(bool e)          { (void)e; return false; }
            virtual bool SetZoom            (float level)     { (void)level; return false; }
            virtual bool SetFlash           (bool e)          { (void)e; return false; }
            virtual bool SetTorch           (bool e)          { (void)e; return false; }
            virtual bool SetFocusPoint      (float x, float y){ (void)x;(void)y; return false; }

            // -----------------------------------------------------------------------
            // Informations session
            // -----------------------------------------------------------------------
            virtual NkU32         GetWidth()     const = 0;
            virtual NkU32         GetHeight()    const = 0;
            virtual NkU32         GetFPS()       const = 0;
            virtual NkPixelFormat GetFormat()    const = 0;
            virtual std::string   GetLastError() const = 0;

            // -----------------------------------------------------------------------
            // Orientation IMU (optionnel — mobile/XR uniquement)
            // -----------------------------------------------------------------------
            virtual bool GetOrientation(NkCameraOrientation& out) { (void)out; return false; }
    };

} // namespace nkentseu
