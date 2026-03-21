#pragma once
// =============================================================================
// NkDeviceFactory.h
// Factory RHI — crée le NkIDevice approprié à partir d'un NkIGraphicsContext.
// Point d'entrée unique pour toute la couche RHI.
// =============================================================================
#include "NkIDevice.h"
#include "../../NkFinal_work/Core/NkIGraphicsContext.h"
#include <initializer_list>

namespace nkentseu {

class NkDeviceFactory {
public:
    // ── Créer depuis un contexte graphique existant ───────────────────────────
    // Le device partage le device natif du contexte (pas de recréation GPU)
    static NkIDevice* Create(NkIGraphicsContext* ctx);

    // ── Créer en spécifiant l'API explicitement ───────────────────────────────
    static NkIDevice* CreateForApi(NkGraphicsApi api,
                                    NkIGraphicsContext* ctx);

    // ── Créer avec chaîne de fallback ─────────────────────────────────────────
    // Essaie chaque API dans l'ordre, retourne le premier qui réussit
    static NkIDevice* CreateWithFallback(NkIGraphicsContext* ctx,
                                          std::initializer_list<NkGraphicsApi> order);

    // ── Vérifier si une API supporte le RHI ──────────────────────────────────
    static bool IsApiSupported(NkGraphicsApi api);

    // ── Détruire proprement ───────────────────────────────────────────────────
    static void Destroy(NkIDevice*& device);
};

} // namespace nkentseu
