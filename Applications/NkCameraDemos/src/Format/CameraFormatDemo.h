#pragma once
// =============================================================================
// CameraFormatDemo.h -- Demo 3 : verification cross-format ConvertToRGBA8.
//
// Genere quatre NkCameraFrame synthetiques (YUYV, NV12, YUV420 I420, MJPEG via
// NkJPEGCodec::Encode) puis les convertit en RGBA8 et les sauve en PNG. Les
// quatre resultats sont egalement affiches dans une grille 2x2. Pas besoin
// de webcam physique : utile pour valider les decodeurs sur CI / Web.
// =============================================================================

#include "NKWindow/Core/NkEntry.h"

namespace nkentseu {
    namespace cameradem {

        int RunCameraFormatDemo(const NkEntryState& state);

    } // namespace cameradem
} // namespace nkentseu
