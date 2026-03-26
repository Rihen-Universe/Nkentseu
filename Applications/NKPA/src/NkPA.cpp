/**
 * @File    NkPA.cpp
 * @Brief   Point d'entree — NKPA Procedural Animation.
 *          12 especes animees par IK (poisson, requin, anguille, meduse,
 *          serpent, chenille, mille-pattes, ver, lezard, tortue, chat, oiseau).
 *
 *  Argument optionnel : --api=<software|opengl|vulkan|dx11|dx12>
 *  Defaut : software
 */

#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkEntry.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NkPAApp.h"

#include <cstring>

using namespace nkentseu;
using namespace nkentseu::nkpa;

// ─── Lecture de l'API depuis les arguments de ligne de commande ───────────────

static NkGraphicsApi ParseApiArg(const NkEntryState& state) {
    for (usize i = 0; i < state.args.Size(); ++i) {
        const char* arg = state.args[i].CStr();
        if (!arg) continue;

        // Accepte --api=xxx ou -api=xxx
        const char* val = nullptr;
        if (::strncmp(arg, "--api=", 6) == 0)       val = arg + 6;
        else if (::strncmp(arg, "-api=",  5) == 0)  val = arg + 5;
        if (!val) continue;

        if (::strcmp(val, "opengl")   == 0) return NkGraphicsApi::NK_API_OPENGL;
        if (::strcmp(val, "vulkan")   == 0) return NkGraphicsApi::NK_API_VULKAN;
        if (::strcmp(val, "dx11")     == 0) return NkGraphicsApi::NK_API_DIRECTX11;
        if (::strcmp(val, "dx12")     == 0) return NkGraphicsApi::NK_API_DIRECTX12;
        if (::strcmp(val, "metal")    == 0) return NkGraphicsApi::NK_API_METAL;
        if (::strcmp(val, "software") == 0) return NkGraphicsApi::NK_API_SOFTWARE;
    }
    return NkGraphicsApi::NK_API_SOFTWARE;
}

int nkmain(const NkEntryState& state) {
    NkGraphicsApi api = ParseApiArg(state);

    NkPAApp app;
    if (!app.Init(1280, 720, api)) return 1;
    app.Run();
    app.Shutdown();
    return 0;
}
