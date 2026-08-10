#pragma once
// =============================================================================
// NKXR.h — Include unique du runtime VR/AR/XR de Nkentseu (ZÉRO STL).
//
//   #include "NKXR/NKXR.h"
//   nkentseu::xr::NkXrSessionDesc desc;
//   desc.window = &window;                       // simulateur desktop
//   auto *session = nkentseu::xr::NkXrSession::Create(desc);
//   ... PollEvent → Begin → WaitFrame/BeginFrame/LocateViews/EndFrame ...
//   nkentseu::xr::NkXrSession::Destroy(session);
// =============================================================================
#include "NKXR/NkXrTypes.h"
#include "NKXR/NkXrPose.h"
#include "NKXR/NkXrSpace.h"
#include "NKXR/NkXrSwapchain.h"
#include "NKXR/NkXrLayer.h"
#include "NKXR/NkXrInput.h"
#include "NKXR/NKIXrBackend.h"
#include "NKXR/NkXrSession.h"
