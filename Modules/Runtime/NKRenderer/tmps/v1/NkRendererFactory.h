#pragma once
// =============================================================================
// NkRendererFactory.h
// Fabrique le renderer concret adapté à l'API graphique du NkIDevice fourni.
// Point d'entrée unique — l'application n'inclut pas les implémentations.
// =============================================================================
#include "NkIRenderer.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkGraphicsApi.h"

namespace nkentseu {
namespace renderer {

    class NkRendererFactory {
    public:
        // Créer le renderer adapté à l'API du device fourni.
        // Le renderer NE possède PAS le device — sa durée de vie est gérée
        // par l'appelant.
        static NkIRenderer* Create(NkIDevice* device);

        // Créer pour une API spécifique (override, principalement pour les tests).
        static NkIRenderer* CreateForApi(NkIDevice* device, NkGraphicsApi api);

        // Détruire proprement (appelle Shutdown puis delete).
        static void Destroy(NkIRenderer*& renderer);
    };

} // namespace renderer
} // namespace nkentseu