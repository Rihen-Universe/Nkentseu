#pragma once
// =============================================================================
// NkDeviceFactory.h
// Factory RHI - cree le NkIDevice approprie a partir d'un NkDeviceInitInfo.
// Point d'entree unique pour toute la couche RHI.
//
// Distinction compile-time vs runtime :
//   - IsApiSupported()    : compile-time — l'API est-elle compilée dans le binaire ?
//   - CreateAutoDetect()  : runtime      — teste chaque API sur le matériel courant.
//     La priorité par plateforme est : Windows → Vulkan > DX12 > DX11 > OpenGL > Software
//                                      macOS/iOS → Metal > OpenGL > Software
//                                      Linux/Android → Vulkan > OpenGL > Software
// =============================================================================
#include "NkIDevice.h"
#include "NKContainers/Sequential/NkVector.h"
#include <initializer_list>

namespace nkentseu {

class NkDeviceFactory {
public:
    // Creer depuis un bloc d'initialisation complet.
    static NkIDevice* Create(const NkDeviceInitInfo& init);

    // Creer en specifiant l'API explicitement.
    static NkIDevice* CreateForApi(NkGraphicsApi api, const NkDeviceInitInfo& init);

    // Creer avec chaine de fallback explicite.
    static NkIDevice* CreateWithFallback(const NkDeviceInitInfo& init,
                                         std::initializer_list<NkGraphicsApi> order);

    // Détection automatique à l'exécution : teste les APIs dans l'ordre optimal
    // pour la plateforme et retourne le premier device fonctionnel.
    // Modifie init.api avec l'API choisie pour que l'appelant sache ce qui a été sélectionné.
    // Retourne nullptr si aucune API ne fonctionne.
    static NkIDevice* CreateAutoDetect(NkDeviceInitInfo& init);

    // Retourne les APIs compilées dans ce binaire (détection compile-time uniquement).
    // Utile pour afficher les options disponibles dans un menu de configuration.
    static NkVector<NkGraphicsApi> GetSupportedApis();

    // Verifier si une API est compilée dans ce binaire (compile-time).
    // Ne garantit PAS que l'API fonctionne sur le matériel courant.
    static bool IsApiSupported(NkGraphicsApi api);

    // Detruire proprement.
    static void Destroy(NkIDevice*& device);
};

} // namespace nkentseu
