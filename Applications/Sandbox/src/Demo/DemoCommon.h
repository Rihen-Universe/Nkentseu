#pragma once
// =============================================================================
// DemoCommon.h  — Shared utilities for the renderdemo entry point.
//
// Une demo expose deux fonctions :
//   void DemoXxx_Init  (DemoCtx& ctx);
//   void DemoXxx_Frame (DemoCtx& ctx, float32 dt);
//   void DemoXxx_Shutdown(DemoCtx& ctx);
//
// Le main.cpp s'occupe de creer fenetre/device/renderer puis appelle la demo
// selectionnee via --demo=N.
// =============================================================================
#include "../NkRenderer.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKLogger/NkLog.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKMath/NKMath.h"
#include <cstdio>

namespace nkentseu { namespace demo {

    using namespace nkentseu;
    using namespace nkentseu::math;
    using namespace nkentseu::renderer;

    // =========================================================================
    // Contexte partage entre main.cpp et les demos individuelles.
    // =========================================================================
    struct DemoCtx {
        NkIDevice*    device   = nullptr;
        NkRenderer*   renderer = nullptr;
        NkWindow*     window   = nullptr;
        NkGraphicsApi api      = NkGraphicsApi::NK_GFX_API_OPENGL;

        uint32        width    = 1280;
        uint32        height   = 720;
        float32       totalTime= 0.f;
        uint32        frame    = 0;

        // Etat utilisateur (la demo y stocke ses ressources)
        void*         userData = nullptr;
    };

    // =========================================================================
    // Helpers : parsing arguments
    // =========================================================================
    inline NkGraphicsApi ParseBackend(const NkVector<NkString>& args) {
        for (size_t i = 1; i < args.Size(); i++) {
            if (args[i] == "--backend=vulkan" || args[i] == "-bvk")  return NkGraphicsApi::NK_GFX_API_VULKAN;
            if (args[i] == "--backend=dx11"   || args[i] == "-bdx11") return NkGraphicsApi::NK_GFX_API_DX11;
            if (args[i] == "--backend=dx12"   || args[i] == "-bdx12") return NkGraphicsApi::NK_GFX_API_DX12;
            if (args[i] == "--backend=metal"  || args[i] == "-bmtl")  return NkGraphicsApi::NK_GFX_API_METAL;
            if (args[i] == "--backend=sw"     || args[i] == "-bsw")   return NkGraphicsApi::NK_GFX_API_SOFTWARE;
        }
        return NkGraphicsApi::NK_GFX_API_OPENGL;
    }

    inline int ParseDemo(const NkVector<NkString>& args, int defaultIdx = 0) {
        for (size_t i = 1; i < args.Size(); i++) {
            if (args[i].StartsWith("--demo=")) return atoi(args[i].SubStr(7).CStr());
        }
        return defaultIdx;
    }

    // =========================================================================
    // Format du nom de la demo (utilise par le main pour les logs)
    // =========================================================================
    inline const char* SubsystemFlagsToString(NkSubsystemFlags f, char* buf, size_t n) {
        // ecrit jusqu'a n octets une representation texte abregee
        size_t off = 0;
        auto Add = [&](const char* s) {
            while (*s && off + 1 < n) buf[off++] = *s++;
        };
        bool first = true;
        auto Sep = [&]() { if (!first) Add("|"); first = false; };
        if (NkHasFlag(f, NK_SS_RENDER2D))      { Sep(); Add("R2D"); }
        if (NkHasFlag(f, NK_SS_RENDER3D))      { Sep(); Add("R3D"); }
        if (NkHasFlag(f, NK_SS_TEXT))          { Sep(); Add("TEXT"); }
        if (NkHasFlag(f, NK_SS_UI))            { Sep(); Add("UI"); }
        if (NkHasFlag(f, NK_SS_SHADOW))        { Sep(); Add("SHADOW"); }
        if (NkHasFlag(f, NK_SS_POST_PROCESS))  { Sep(); Add("PP"); }
        if (NkHasFlag(f, NK_SS_VFX))           { Sep(); Add("VFX"); }
        if (NkHasFlag(f, NK_SS_ANIMATION))     { Sep(); Add("ANIM"); }
        if (NkHasFlag(f, NK_SS_OVERLAY))       { Sep(); Add("OVERLAY"); }
        if (NkHasFlag(f, NK_SS_SIMULATION))    { Sep(); Add("SIM"); }
        if (off == 0) Add("NONE");
        buf[off < n ? off : n - 1] = 0;
        return buf;
    }

    // =========================================================================
    // Interface d'une demo
    // =========================================================================
    using DemoInitFn     = bool(*)(DemoCtx&);
    using DemoFrameFn    = void(*)(DemoCtx&, float32);
    using DemoShutdownFn = void(*)(DemoCtx&);

    struct DemoEntry {
        const char*    name;
        const char*    description;
        DemoInitFn     init;
        DemoFrameFn    frame;
        DemoShutdownFn shutdown;
    };

}} // namespace nkentseu::demo
