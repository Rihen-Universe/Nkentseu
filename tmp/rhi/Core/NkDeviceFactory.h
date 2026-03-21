#pragma once
// =============================================================================
// NkDeviceFactory.h
// Factory RHI — crée le NkIDevice approprié à partir d'un NkSurfaceDesc.
// Point d'entrée unique pour toute la couche RHI.
// =============================================================================
#include "NkIDevice.h"
#include "NKContext/Core/NkSurfaceDesc.h"
#include <initializer_list>

namespace nkentseu {

    class NkDeviceFactory {
        public:
            // ── Créer en spécifiant l'API explicitement ───────────────────────────────
            static NkIDevice* CreateForApi(NkGraphicsApi api, NkSurfaceDesc* surface);

            // ── Créer avec chaîne de fallback ─────────────────────────────────────────
            // Essaie chaque API dans l'ordre, retourne le premier qui réussit
            static NkIDevice* CreateWithFallback(NkSurfaceDesc* surface, std::initializer_list<NkGraphicsApi> order);

            // ── Vérifier si une API supporte le RHI ──────────────────────────────────
            static bool IsApiSupported(NkGraphicsApi api);

            // ── Détruire proprement ───────────────────────────────────────────────────
            static void Destroy(NkIDevice*& device);
    };

} // namespace nkentseu
