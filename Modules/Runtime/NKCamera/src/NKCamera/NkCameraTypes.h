#pragma once
// =============================================================================
// NkCameraTypes.h — Types communs au système de capture caméra physique
// VERSION CORRIGÉE ET COMPLÈTE
// =============================================================================

#include "NKWindow/Core/NkTypes.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace nkentseu{

    // ---------------------------------------------------------------------------
    // NkPixelFormat
    // ---------------------------------------------------------------------------

    inline const char* NkCameraPixelFormatToString(NkPixelFormat f) {
        switch (f) {
        case NkPixelFormat::NK_PIXEL_RGBA8:  return "RGBA8";
        case NkPixelFormat::NK_PIXEL_BGRA8:  return "BGRA8";
        case NkPixelFormat::NK_PIXEL_RGB8:   return "RGB8";
        case NkPixelFormat::NK_PIXEL_YUV420: return "YUV420";
        case NkPixelFormat::NK_PIXEL_NV12:   return "NV12";
        case NkPixelFormat::NK_PIXEL_YUYV:   return "YUYV";
        case NkPixelFormat::NK_PIXEL_MJPEG:  return "MJPEG";
        default: return "UNKNOWN";
        }
    }

    // ---------------------------------------------------------------------------
    // NkCameraFacing
    // ---------------------------------------------------------------------------
    enum class NkCameraFacing : NkU32 {
        NK_CAMERA_FACING_ANY      = 0,
        NK_CAMERA_FACING_FRONT,
        NK_CAMERA_FACING_BACK,
        NK_CAMERA_FACING_EXTERNAL,
    };

    // ---------------------------------------------------------------------------
    // NkCameraResolution
    // ---------------------------------------------------------------------------
    enum class NkCameraResolution : NkU32 {
        NK_CAM_RES_CUSTOM = 0,
        NK_CAM_RES_QVGA,   ///<  320×240
        NK_CAM_RES_VGA,    ///<  640×480
        NK_CAM_RES_HD,     ///<  1280×720
        NK_CAM_RES_FHD,    ///<  1920×1080
        NK_CAM_RES_4K,     ///<  3840×2160
    };

    inline void NkResolutionToSize(NkCameraResolution r, NkU32& w, NkU32& h) {
        switch (r) {
        case NkCameraResolution::NK_CAM_RES_QVGA: w=320;  h=240;  break;
        case NkCameraResolution::NK_CAM_RES_VGA:  w=640;  h=480;  break;
        case NkCameraResolution::NK_CAM_RES_HD:   w=1280; h=720;  break;
        case NkCameraResolution::NK_CAM_RES_FHD:  w=1920; h=1080; break;
        case NkCameraResolution::NK_CAM_RES_4K:   w=3840; h=2160; break;
        default:                                  w=640;  h=480;  break;
        }
    }

    // ---------------------------------------------------------------------------
    // NkCameraDevice
    // ---------------------------------------------------------------------------
    struct NkCameraDevice {
        NkU32          index  = 0;
        std::string    id;       ///< Identifiant OS unique (path Linux, GUID Win32, uniqueID iOS/macOS)
        std::string    name;     ///< Nom lisible
        NkCameraFacing facing = NkCameraFacing::NK_CAMERA_FACING_ANY;

        struct Mode {
            NkU32         width  = 0;
            NkU32         height = 0;
            NkU32         fps    = 30;
            NkPixelFormat format = NkPixelFormat::NK_PIXEL_RGBA8;
        };
        std::vector<Mode> modes;

        bool IsValid() const { return !id.empty(); }
        std::string ToString() const {
            return "Camera[" + std::to_string(index) + "] \"" + name
                + "\" facing=" + std::to_string((NkU32)facing)
                + " modes=" + std::to_string(modes.size());
        }
    };

    // ---------------------------------------------------------------------------
    // NkCameraConfig
    // ---------------------------------------------------------------------------
    struct NkCameraConfig {
        NkU32               deviceIndex      = 0;
        NkCameraResolution  preset           = NkCameraResolution::NK_CAM_RES_HD;
        NkU32               width            = 0;
        NkU32               height           = 0;
        NkU32               fps              = 30;
        NkPixelFormat       outputFormat     = NkPixelFormat::NK_PIXEL_RGBA8;
        NkCameraFacing      facing           = NkCameraFacing::NK_CAMERA_FACING_ANY;
        bool                flipHorizontal   = false;
        bool                autoFocus        = true;
        bool                autoExposure     = true;
        bool                autoWhiteBalance = true;

        void Resolve() {
            if (preset != NkCameraResolution::NK_CAM_RES_CUSTOM)
                NkResolutionToSize(preset, width, height);
            if (width  == 0) width  = 640;
            if (height == 0) height = 480;
            if (fps    == 0) fps    = 30;
        }
    };

    // ---------------------------------------------------------------------------
    // NkCameraFrame
    // ---------------------------------------------------------------------------
    struct NkCameraFrame {
        NkU32              width        = 0;
        NkU32              height       = 0;
        NkPixelFormat      format       = NkPixelFormat::NK_PIXEL_RGBA8;
        NkU64              timestampUs  = 0;
        NkU32              frameIndex   = 0;
        NkU32              stride       = 0;
        std::vector<NkU8>  data;

        bool IsValid() const { return width > 0 && height > 0 && !data.empty(); }

        /// Accès pixel RGBA8 (uniquement si format == NK_PIXEL_RGBA8)
        NkU32 GetPixelRGBA(NkU32 x, NkU32 y) const {
            if (x >= width || y >= height || format != NkPixelFormat::NK_PIXEL_RGBA8) return 0;
            const NkU8* p = data.data() + y * stride + x * 4;
            return ((NkU32)p[0] << 24) | ((NkU32)p[1] << 16)
                | ((NkU32)p[2] <<  8) |  (NkU32)p[3];
        }

        static NkU32 DefaultStride(NkU32 w, NkPixelFormat fmt) {
            switch (fmt) {
            case NkPixelFormat::NK_PIXEL_RGBA8:
            case NkPixelFormat::NK_PIXEL_BGRA8: return w * 4;
            case NkPixelFormat::NK_PIXEL_RGB8:  return w * 3;
            default: return w * 4;
            }
        }
    };

    // ---------------------------------------------------------------------------
    // NkPhotoCaptureResult
    // ---------------------------------------------------------------------------
    struct NkPhotoCaptureResult {
        bool           success  = false;
        std::string    errorMsg;
        NkCameraFrame  frame;
        std::string    savedPath;
        explicit operator bool() const { return success; }
    };

    // ---------------------------------------------------------------------------
    // NkVideoRecordConfig
    // ---------------------------------------------------------------------------
    struct NkVideoRecordConfig {
        enum class Mode : NkU32 {
            AUTO = 0,            // Choisit la meilleure voie (vidéo native, puis fallback)
            VIDEO_ONLY,          // Force un enregistrement vidéo (échec si indisponible)
            IMAGE_SEQUENCE_ONLY, // Force un enregistrement image par image
        };

        std::string outputPath;
        NkU32       bitrateBps      = 4000000;
        NkU32       audioSampleRate = 44100;
        bool        captureAudio    = false;
        std::string videoCodec      = "h264";
        std::string audioCodec      = "aac";
        std::string container       = "mp4";
        Mode        mode            = Mode::AUTO;
    };

    // ---------------------------------------------------------------------------
    // NkCameraState
    // ---------------------------------------------------------------------------
    enum class NkCameraState : NkU32 {
        NK_CAM_STATE_CLOSED    = 0,
        NK_CAM_STATE_OPENING,
        NK_CAM_STATE_STREAMING,
        NK_CAM_STATE_RECORDING,
        NK_CAM_STATE_PAUSED,
        NK_CAM_STATE_ERROR,
    };

    // ---------------------------------------------------------------------------
    // Callbacks
    // ---------------------------------------------------------------------------
    using NkFrameCallback       = std::function<void(const NkCameraFrame&)>;
    using NkCameraHotPlugCallback = std::function<void(const std::vector<NkCameraDevice>&)>;

    // ---------------------------------------------------------------------------
    // NkCameraOrientation — pour le mapping caméra virtuelle / caméra réelle
    // ---------------------------------------------------------------------------
    struct NkCameraOrientation {
        float yaw   = 0.f;  ///< Rotation autour de Y (gauche/droite), degrés
        float pitch = 0.f;  ///< Rotation autour de X (haut/bas), degrés
        float roll  = 0.f;  ///< Rotation autour de Z (inclinaison), degrés
        float accelX = 0.f, accelY = 0.f, accelZ = 0.f; ///< Accéléromètre (m/s²)
    };

} // namespace nkentseu
