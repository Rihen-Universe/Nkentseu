#pragma once
// =============================================================================
// NkDeviceFactory.h
// Factory RHI - cree le NkIDevice approprie a partir d'un NkDeviceInitInfo.
// Point d'entree unique pour toute la couche RHI.
// =============================================================================
#include "NkIDevice.h"
#include <initializer_list>

namespace nkentseu {

class NkDeviceFactory {
public:
    // Creer depuis un bloc d'initialisation complet.
    static NkIDevice* Create(const NkDeviceInitInfo& init);

    // Creer en specifiant l'API explicitement.
    static NkIDevice* CreateForApi(NkGraphicsApi api, const NkDeviceInitInfo& init);

    // Creer avec chaine de fallback.
    static NkIDevice* CreateWithFallback(const NkDeviceInitInfo& init, std::initializer_list<NkGraphicsApi> order);

    // Verifier si une API supporte le RHI.
    static bool IsApiSupported(NkGraphicsApi api);

    // Detruire proprement.
    static void Destroy(NkIDevice*& device);
};

} // namespace nkentseu
