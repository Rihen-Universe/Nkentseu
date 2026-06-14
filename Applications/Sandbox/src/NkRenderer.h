#pragma once
// =============================================================================
// NkRenderer.h  — Facade publique pour les demos Sandbox
// Inclut toutes les API NKRenderer nécessaires aux demos.
// =============================================================================
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Materials/NkMaterial.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/Text/NkTextRenderer.h"
#include "NKRenderer/Tools/PostProcess/NkPostProcessStack.h"
#include "NKRenderer/Tools/VFX/NkVFXSystem.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Tools/Simulation/NkSimulationRenderer.h"
#include "NKRenderer/Tools/Animation/NkAnimationSystem.h"
#include "NKRenderer/Tools/Overlay/NkOverlayRenderer.h"

// Suppress Win32 GDI macros that collide with renderer method names
#ifdef DrawText
#  undef DrawText
#endif
#ifdef DrawTextCentered
#  undef DrawTextCentered
#endif

// Convenience using-declarations for demo code
namespace nkentseu {
    namespace renderer {
        // All types are in nkentseu::renderer — demos can use unqualified names
        // after `using namespace nkentseu::renderer;`
    }
}
