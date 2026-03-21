#pragma once
// =============================================================================
// NkDeviceFactory.h
// Factory RHI - cree le NkIDevice approprie a partir d'un NkIGraphicsContext.
// Point d'entree unique pour toute la couche RHI.
// =============================================================================
#include "NKRenderer/RHI/Core/NkIDevice.h"
#include <initializer_list>

namespace nkentseu {
class NkIGraphicsContext;

class NkDeviceFactory {
public:
    // Creer depuis un contexte graphique existant.
    static NkIDevice* Create(NkIGraphicsContext* ctx);

    // Creer en specifiant l'API explicitement.
    static NkIDevice* CreateForApi(NkGraphicsApi api,
                                   NkIGraphicsContext* ctx);

    // Creer avec chaine de fallback.
    static NkIDevice* CreateWithFallback(NkIGraphicsContext* ctx,
                                         std::initializer_list<NkGraphicsApi> order);

    // Verifier si une API supporte le RHI.
    static bool IsApiSupported(NkGraphicsApi api);

    // Detruire proprement.
    static void Destroy(NkIDevice*& device);
};

} // namespace nkentseu
