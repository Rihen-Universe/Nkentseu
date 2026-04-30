#ifndef NKENTSEU_EVENT_NKGENERICHIDMAPPER_H
#define NKENTSEU_EVENT_NKGENERICHIDMAPPER_H

#pragma once

// =============================================================================
// Fichier : NkGenericHidMapper.h
// =============================================================================
// Description :
//   Mapper léger et générique pour relier des indices HID physiques 
//   (boutons, axes) à des indices logiques applicatifs.
//
// Objectifs :
//   - Permettre à l'utilisateur de définir un mapping personnalisé par deviceId.
//   - Rester indépendant du backend graphique ou de la plateforme.
//   - Être utilisable directement depuis les callbacks d'événements HID.
//   - Fournir des transformations d'axes (scale, offset, deadzone, invert).
//
// Fonctionnalités :
//   - Mapping bouton physique → bouton logique (ou désactivation).
//   - Mapping axe physique → axe logique avec transformations paramétrables.
//   - Résolution à la volée depuis les événements NkHidButtonEvent/NkHidAxisEvent.
//   - Gestion multi-périphériques via un conteneur interne par deviceId.
//
// Thread-safety :
//   - Non thread-safe par défaut. Synchronisation externe requise si accès 
//     concurrent depuis plusieurs threads.
//
// Auteur : Équipe NkEntseu
// Version : 1.0.0
// Date : 2026
// =============================================================================

    #include "NkGenericHidEvent.h"
    #include "NKMath/NkFunctions.h"

    #include <algorithm>
    #include <cmath>

    namespace nkentseu {
        // =========================================================================
        // Section : Constantes et types de configuration du mapper HID
        // =========================================================================

        // -------------------------------------------------------------------------
        // Constante : NK_HID_UNMAPPED
        // -------------------------------------------------------------------------
        // Description :
        //   Valeur sentinelle indiquant qu'un indice physique n'est pas mappé 
        //   à un indice logique (désactivation explicite ou absence de mapping).
        // Utilisation :
        //   - Comparaison avec les résultats de ResolveButton/ResolveAxis.
        //   - Configuration de DisableButton/DisableAxis pour ignorer un contrôle.
        // -------------------------------------------------------------------------
        inline constexpr uint32 NK_HID_UNMAPPED = 0xFFFFFFFFu;

        // -------------------------------------------------------------------------
        // Structure : NkHidAxisBinding
        // -------------------------------------------------------------------------
        // Description :
        //   Conteneur de paramètres de transformation pour un axe HID mappé.
        //   Appliqué lors de la résolution d'un axe physique vers un axe logique.
        // Paramètres :
        //   - logicalAxis : Indice logique cible (NK_HID_UNMAPPED = désactivé).
        //   - scale       : Facteur de multiplication appliqué à la valeur brute.
        //   - offset      : Valeur additive appliquée après le scaling.
        //   - deadzone    : Seuil en dessous duquel la valeur est ramenée à zéro.
        //   - invert      : Inversion du signe de la valeur avant transformations.
        // Ordre d'application :
        //   1. Inversion (si invert=true) : v = -v
        //   2. Scaling : v = v * scale
        //   3. Offset : v = v + offset
        //   4. Deadzone : si |v| < deadzone alors v = 0
        //   5. Clamping : v = clamp(v, -1.0, +1.0)
        // -------------------------------------------------------------------------
        struct NkHidAxisBinding {
            uint32 logicalAxis = NK_HID_UNMAPPED;
            float32 scale = 1.f;
            float32 offset = 0.f;
            float32 deadzone = 0.f;
            bool invert = false;
        };

        // =========================================================================
        // Section : Classe principale NkGenericHidMapper
        // =========================================================================

        // -------------------------------------------------------------------------
        // Classe : NkGenericHidMapper
        // -------------------------------------------------------------------------
        // Description :
        //   Mapper générique pour la traduction des contrôles HID physiques 
        //   vers des identifiants logiques applicatifs.
        // Responsabilités :
        //   - Stockage des mappings par périphérique (deviceId).
        //   - Résolution des boutons et axes avec transformations optionnelles.
        //   - Interface simple pour configuration et interrogation des mappings.
        // Utilisation typique :
        //   - Instancier un mapper global ou par contexte d'entrée.
        //   - Configurer les mappings au chargement d'un profil périphérique.
        //   - Appeler MapButtonEvent/MapAxisEvent dans les callbacks HID.
        // Notes :
        //   - Les mappings non configurés retournent l'indice physique inchangé.
        //   - La désactivation explicite retourne NK_HID_UNMAPPED.
        // -------------------------------------------------------------------------
        class NkGenericHidMapper {
            public:
                // -----------------------------------------------------------------
                // Méthode : Clear
                // -----------------------------------------------------------------
                // Description :
                //   Supprime tous les mappings de tous les périphériques.
                //   Réinitialise le mapper à son état vierge.
                // Utilisation :
                //   - Réinitialisation complète entre sessions ou tests.
                //   - Libération mémoire avant destruction (optionnel).
                // -----------------------------------------------------------------
                void Clear() noexcept {
                    mDevices.Clear();
                }

                // -----------------------------------------------------------------
                // Méthode : ClearDevice
                // -----------------------------------------------------------------
                // Description :
                //   Supprime tous les mappings pour un périphérique spécifique.
                // Paramètres :
                //   - deviceId : Identifiant du périphérique à réinitialiser.
                // Utilisation :
                //   - Nettoyage lors de la déconnexion d'un périphérique.
                //   - Rechargement dynamique de configuration par device.
                // -----------------------------------------------------------------
                void ClearDevice(uint64 deviceId) noexcept {
                    mDevices.Erase(deviceId);
                }

                // -----------------------------------------------------------------
                // Méthode : SetButtonMap
                // -----------------------------------------------------------------
                // Description :
                //   Associe un bouton physique à un bouton logique pour un device.
                // Paramètres :
                //   - deviceId       : Identifiant du périphérique cible.
                //   - physicalButton : Index du bouton physique (0-based).
                //   - logicalButton  : Index logique applicatif cible.
                // Notes :
                //   - Écrase tout mapping précédent pour ce bouton physique.
                //   - Utiliser DisableButton pour désactiver un bouton.
                // -----------------------------------------------------------------
                void SetButtonMap(
                    uint64 deviceId,
                    uint32 physicalButton,
                    uint32 logicalButton
                ) {
                    mDevices[deviceId].buttonMap[physicalButton] = logicalButton;
                }

                // -----------------------------------------------------------------
                // Méthode : DisableButton
                // -----------------------------------------------------------------
                // Description :
                //   Désactive explicitement un bouton physique pour un device.
                // Paramètres :
                //   - deviceId       : Identifiant du périphérique cible.
                //   - physicalButton : Index du bouton physique à désactiver.
                // Effet :
                //   - ResolveButton retournera NK_HID_UNMAPPED pour ce bouton.
                //   - MapButtonEvent retournera false, ignorant l'événement.
                // -----------------------------------------------------------------
                void DisableButton(
                    uint64 deviceId,
                    uint32 physicalButton
                ) {
                    mDevices[deviceId].buttonMap[physicalButton] = NK_HID_UNMAPPED;
                }

                // -----------------------------------------------------------------
                // Méthode : SetAxisMap
                // -----------------------------------------------------------------
                // Description :
                //   Configure le mapping d'un axe physique vers un axe logique 
                //   avec transformations optionnelles (scale, offset, deadzone...).
                // Paramètres :
                //   - deviceId      : Identifiant du périphérique cible.
                //   - physicalAxis  : Index de l'axe physique (0-based).
                //   - logicalAxis   : Index logique applicatif cible.
                //   - invert        : Inverser le signe de la valeur (par défaut: false).
                //   - scale         : Facteur de multiplication (par défaut: 1.0).
                //   - deadzone      : Seuil de zone morte (par défaut: 0.0).
                //   - offset        : Décalage additif (par défaut: 0.0).
                // Notes :
                //   - Les transformations sont appliquées dans l'ordre défini 
                //     dans la documentation de NkHidAxisBinding.
                //   - Utiliser DisableAxis pour désactiver un axe.
                // -----------------------------------------------------------------
                void SetAxisMap(
                    uint64 deviceId,
                    uint32 physicalAxis,
                    uint32 logicalAxis,
                    bool invert = false,
                    float32 scale = 1.f,
                    float32 deadzone = 0.f,
                    float32 offset = 0.f
                ) {
                    NkHidAxisBinding b;
                    b.logicalAxis = logicalAxis;
                    b.invert = invert;
                    b.scale = scale;
                    b.deadzone = deadzone;
                    b.offset = offset;
                    mDevices[deviceId].axisMap[physicalAxis] = b;
                }

                // -----------------------------------------------------------------
                // Méthode : DisableAxis
                // -----------------------------------------------------------------
                // Description :
                //   Désactive explicitement un axe physique pour un device.
                // Paramètres :
                //   - deviceId     : Identifiant du périphérique cible.
                //   - physicalAxis : Index de l'axe physique à désactiver.
                // Effet :
                //   - ResolveAxis retournera false pour cet axe.
                //   - MapAxisEvent retournera false, ignorant l'événement.
                // -----------------------------------------------------------------
                void DisableAxis(
                    uint64 deviceId,
                    uint32 physicalAxis
                ) {
                    NkHidAxisBinding b;
                    b.logicalAxis = NK_HID_UNMAPPED;
                    mDevices[deviceId].axisMap[physicalAxis] = b;
                }

                // -----------------------------------------------------------------
                // Méthode : ResolveButton
                // -----------------------------------------------------------------
                // Description :
                //   Résout un indice de bouton physique en indice logique.
                // Paramètres :
                //   - deviceId       : Identifiant du périphérique source.
                //   - physicalButton : Index du bouton physique à résoudre.
                // Retour :
                //   - Indice logique mappé, ou physicalButton si non configuré,
                //     ou NK_HID_UNMAPPED si explicitement désactivé.
                // Notes :
                //   - Fonction noexcept : garantie de ne pas lever d'exception.
                //   - Retourne l'indice physique inchangé si aucun mapping n'existe.
                // -----------------------------------------------------------------
                uint32 ResolveButton(
                    uint64 deviceId,
                    uint32 physicalButton
                ) const noexcept {
                    const DeviceMapping* dm = FindDevice(deviceId);
                    if (!dm) {
                        return physicalButton;
                    }

                    const uint32* val = dm->buttonMap.Find(physicalButton);
                    if (!val) {
                        return physicalButton;
                    }
                    return *val;
                }

                // -----------------------------------------------------------------
                // Méthode : ResolveAxis
                // -----------------------------------------------------------------
                // Description :
                //   Résout un axe physique en axe logique avec transformations.
                // Paramètres :
                //   - deviceId      : Identifiant du périphérique source.
                //   - physicalAxis  : Index de l'axe physique à résoudre.
                //   - rawValue      : Valeur brute de l'axe (normalisée [-1,1]).
                //   - outLogicalAxis : [OUT] Indice logique résultant.
                //   - outValue       : [OUT] Valeur transformée et clampée.
                // Retour :
                //   - true si l'axe est actif et mappé (ou fallback par défaut).
                //   - false si l'axe est explicitement désactivé (NK_HID_UNMAPPED).
                // Transformations appliquées (si mapping configuré) :
                //   1. Inversion : v = -v (si invert=true)
                //   2. Scaling : v = v * scale
                //   3. Offset : v = v + offset
                //   4. Deadzone : si |v| < deadzone alors v = 0
                //   5. Clamping : v = clamp(v, -1.0, +1.0)
                // -----------------------------------------------------------------
                bool ResolveAxis(
                    uint64 deviceId,
                    uint32 physicalAxis,
                    float32 rawValue,
                    uint32& outLogicalAxis,
                    float32& outValue
                ) const noexcept {
                    const DeviceMapping* dm = FindDevice(deviceId);
                    if (!dm) {
                        outLogicalAxis = physicalAxis;
                        outValue = math::NkClamp(rawValue, -1.f, 1.f);
                        return true;
                    }

                    const NkHidAxisBinding* b = dm->axisMap.Find(physicalAxis);
                    if (!b) {
                        outLogicalAxis = physicalAxis;
                        outValue = math::NkClamp(rawValue, -1.f, 1.f);
                        return true;
                    }

                    if (b->logicalAxis == NK_HID_UNMAPPED) {
                        return false;
                    }

                    float32 v = rawValue;
                    if (b->invert) {
                        v = -v;
                    }
                    v = (v * b->scale) + b->offset;
                    if (math::NkFabs(v) < b->deadzone) {
                        v = 0.f;
                    }
                    outLogicalAxis = b->logicalAxis;
                    outValue = math::NkClamp(v, -1.f, 1.f);
                    return true;
                }

                // -----------------------------------------------------------------
                // Méthode : MapButtonEvent
                // -----------------------------------------------------------------
                // Description :
                //   Résout un événement de bouton HID en indices logiques.
                // Paramètres :
                //   - ev              : Événement NkHidButtonEvent source.
                //   - outLogicalButton : [OUT] Indice logique du bouton.
                //   - outState         : [OUT] État du bouton (pressed/released).
                // Retour :
                //   - true si le bouton est actif et mappé.
                //   - false si le bouton est désactivé (NK_HID_UNMAPPED).
                // Utilisation :
                //   - Appeler dans le callback de NkHidButtonPressEvent/ReleaseEvent.
                //   - Ignorer l'événement si la méthode retourne false.
                // -----------------------------------------------------------------
                bool MapButtonEvent(
                    const NkHidButtonEvent& ev,
                    uint32& outLogicalButton,
                    NkButtonState& outState
                ) const noexcept {
                    outLogicalButton = ResolveButton(ev.GetDeviceId(), ev.GetButtonIndex());
                    if (outLogicalButton == NK_HID_UNMAPPED) {
                        return false;
                    }
                    outState = ev.GetState();
                    return true;
                }

                // -----------------------------------------------------------------
                // Méthode : MapAxisEvent
                // -----------------------------------------------------------------
                // Description :
                //   Résout un événement d'axe HID en indice et valeur logiques.
                // Paramètres :
                //   - ev            : Événement NkHidAxisEvent source.
                //   - outLogicalAxis : [OUT] Indice logique de l'axe.
                //   - outValue       : [OUT] Valeur transformée et clampée.
                // Retour :
                //   - true si l'axe est actif et mappé (ou fallback par défaut).
                //   - false si l'axe est explicitement désactivé.
                // Utilisation :
                //   - Appeler dans le callback de NkHidAxisEvent.
                //   - Ignorer l'événement si la méthode retourne false.
                // -----------------------------------------------------------------
                bool MapAxisEvent(
                    const NkHidAxisEvent& ev,
                    uint32& outLogicalAxis,
                    float32& outValue
                ) const noexcept {
                    return ResolveAxis(
                        ev.GetDeviceId(),
                        ev.GetAxisIndex(),
                        ev.GetValue(),
                        outLogicalAxis,
                        outValue
                    );
                }

            private:
                // -----------------------------------------------------------------
                // Structure interne : DeviceMapping
                // -----------------------------------------------------------------
                // Description :
                //   Conteneur des mappings pour un périphérique unique.
                //   Regroupe les maps de boutons et d'axes pour ce device.
                // Membres :
                //   - buttonMap : Association physicalButton → logicalButton.
                //   - axisMap   : Association physicalAxis → NkHidAxisBinding.
                // Notes :
                //   - Structure privée : usage interne au mapper uniquement.
                //   - Utilise NkUnorderedMap pour un accès O(1) moyen.
                // -----------------------------------------------------------------
                struct DeviceMapping {
                    NkUnorderedMap<uint32, uint32> buttonMap;
                    NkUnorderedMap<uint32, NkHidAxisBinding> axisMap;
                };

                // -----------------------------------------------------------------
                // Méthode interne : FindDevice
                // -----------------------------------------------------------------
                // Description :
                //   Recherche et retourne un pointeur constant vers le mapping 
                //   d'un périphérique, ou nullptr si non trouvé.
                // Paramètres :
                //   - deviceId : Identifiant du périphérique à rechercher.
                // Retour :
                //   - Pointeur constant vers DeviceMapping si trouvé.
                //   - nullptr si le deviceId n'existe pas dans le conteneur.
                // Notes :
                //   - Méthode const noexcept : accès en lecture seule sécurisé.
                //   - Utilise Find() de NkUnorderedMap pour éviter une copie.
                // -----------------------------------------------------------------
                const DeviceMapping* FindDevice(uint64 deviceId) const noexcept {
                    return mDevices.Find(deviceId);
                }

                // -----------------------------------------------------------------
                // Membre : mDevices
                // -----------------------------------------------------------------
                // Description :
                //   Conteneur principal associant chaque deviceId à ses mappings.
                // Type :
                //   NkUnorderedMap<uint64, DeviceMapping>
                // Accès :
                //   - Lecture/écriture via les méthodes publiques du mapper.
                //   - Recherche via FindDevice() pour les opérations const.
                // Notes :
                //   - Clé : deviceId (uint64) fournie par le système HID.
                //   - Valeur : DeviceMapping contenant buttonMap et axisMap.
                // -----------------------------------------------------------------
                NkUnorderedMap<uint64, DeviceMapping> mDevices;
        };

    } // namespace nkentseu

#endif // NKENTSEU_EVENT_NKGENERICHIDMAPPER_H

// =============================================================================
// Section : Exemples d'utilisation (à titre informatif - en commentaire)
// =============================================================================

/*
// -----------------------------------------------------------------------------
// Exemple 1 : Configuration initiale d'un mapper pour un volant de course
// -----------------------------------------------------------------------------
void ConfigureRacingWheelMapper(NkGenericHidMapper& mapper, uint64 wheelDeviceId) {
    // Mapping des boutons principaux
    mapper.SetButtonMap(wheelDeviceId, 0, 0);  // Bouton physique 0 → Action "A"
    mapper.SetButtonMap(wheelDeviceId, 1, 1);  // Bouton physique 1 → Action "B"
    mapper.SetButtonMap(wheelDeviceId, 2, 2);  // Bouton physique 2 → Action "X"
    mapper.SetButtonMap(wheelDeviceId, 3, 3);  // Bouton physique 3 → Action "Y"

    // Désactivation des boutons non utilisés
    mapper.DisableButton(wheelDeviceId, 10);  // Bouton menu système
    mapper.DisableButton(wheelDeviceId, 11);  // Bouton pairing Bluetooth

    // Mapping des axes avec transformations
    // Axe 0 : Volant (rotation X) avec deadzone et inversion
    mapper.SetAxisMap(
        wheelDeviceId,
        0,              // physicalAxis: rotation du volant
        0,              // logicalAxis: steering input
        true,           // invert: inversion pour orientation cohérente
        1.0f,           // scale: pas de modification d'amplitude
        0.05f,          // deadzone: 5% de zone morte centrale
        0.0f            // offset: pas de décalage
    );

    // Axe 1 : Accélérateur (pédale droite) sans inversion, avec offset
    mapper.SetAxisMap(
        wheelDeviceId,
        1,              // physicalAxis: pédale accélérateur
        1,              // logicalAxis: throttle input
        false,          // invert: pas d'inversion (0=repos, 1=à fond)
        1.0f,           // scale: amplitude normale
        0.02f,          // deadzone: 2% pour éviter les dérives
        0.0f            // offset: pas de décalage
    );

    // Axe 2 : Frein (pédale gauche) avec courbe de réponse plus sensible
    mapper.SetAxisMap(
        wheelDeviceId,
        2,              // physicalAxis: pédale de frein
        2,              // logicalAxis: brake input
        false,          // invert: pas d'inversion
        1.2f,           // scale: amplification pour sensibilité accrue
        0.03f,          // deadzone: 3% de zone morte
        0.0f            // offset: pas de décalage
    );
}

// -----------------------------------------------------------------------------
// Exemple 2 : Callback de traitement d'un événement bouton HID
// -----------------------------------------------------------------------------
void OnHidButtonEvent(const NkHidButtonEvent& event, NkGenericHidMapper& mapper) {
    uint32 logicalButton = 0;
    NkButtonState state = NkButtonState::NK_RELEASED;

    // Résolution du mapping
    if (!mapper.MapButtonEvent(event, logicalButton, state)) {
        // Bouton désactivé ou non mappé : ignorer l'événement
        return;
    }

    // Dispatch vers le système d'entrée logique
    switch (logicalButton) {
        case 0: // Action "A"
            if (state == NkButtonState::NK_PRESSED) {
                // PlayerInput::Confirm();
            }
            break;
        case 1: // Action "B"
            if (state == NkButtonState::NK_PRESSED) {
                // PlayerInput::Cancel();
            }
            break;
        case 2: // Action "X"
            if (state == NkButtonState::NK_PRESSED) {
                // PlayerInput::Interact();
            }
            break;
        case 3: // Action "Y"
            if (state == NkButtonState::NK_PRESSED) {
                // PlayerInput::Jump();
            }
            break;
        default:
            // Bouton logique non reconnu : gestion générique
            // InputDispatcher::DispatchGenericButton(logicalButton, state);
            break;
    }
}

// -----------------------------------------------------------------------------
// Exemple 3 : Callback de traitement d'un événement axe HID
// -----------------------------------------------------------------------------
void OnHidAxisEvent(const NkHidAxisEvent& event, NkGenericHidMapper& mapper) {
    uint32 logicalAxis = 0;
    float32 mappedValue = 0.f;

    // Résolution du mapping avec transformations
    if (!mapper.MapAxisEvent(event, logicalAxis, mappedValue)) {
        // Axe désactivé : ignorer l'événement
        return;
    }

    // Application d'un filtre de lissage optionnel (ex: lissage exponentiel)
    // static float32 smoothedValues[MAX_LOGICAL_AXES] = {};
    // constexpr float32 SMOOTH_FACTOR = 0.1f;
    // smoothedValues[logicalAxis] = math::NkLerp(
    //     smoothedValues[logicalAxis],
    //     mappedValue,
    //     SMOOTH_FACTOR
    // );
    // const float32 finalValue = smoothedValues[logicalAxis];

    const float32 finalValue = mappedValue; // Sans lissage pour l'exemple

    // Dispatch vers le contrôleur approprié
    switch (logicalAxis) {
        case 0: // Steering / Direction
            // VehicleController::SetSteering(finalValue);
            break;
        case 1: // Throttle / Accélération
            // VehicleController::SetThrottle(finalValue);
            break;
        case 2: // Brake / Freinage
            // VehicleController::SetBrake(finalValue);
            break;
        case 3: // Clutch / Embrayage
            // VehicleController::SetClutch(finalValue);
            break;
        default:
            // Axe générique : dispatch vers système de mapping avancé
            // AdvancedInputMapper::DispatchAxis(logicalAxis, finalValue);
            break;
    }
}

// -----------------------------------------------------------------------------
// Exemple 4 : Gestion dynamique de la connexion/déconnexion de périphériques
// -----------------------------------------------------------------------------
void OnHidConnectEvent(const NkHidConnectEvent& event, NkGenericHidMapper& globalMapper) {
    const auto& info = event.GetInfo();
    const auto deviceId = event.GetDeviceId();

    // Détection du type de périphérique par VID/PID ou usagePage
    if (info.usagePage == static_cast<uint16>(NkHidUsagePage::NK_HID_PAGE_SIMULATION)) {
        // Configuration automatique pour périphérique de simulation
        ConfigureRacingWheelMapper(globalMapper, deviceId);
        NK_LOG_INFO("Mapper configuré pour périphérique simulation: {}", info.name);
    } else if (info.vendorId == 0x046D && info.productId == 0xC262) {
        // Configuration spécifique pour un modèle connu (ex: tablette Wacom)
        // ConfigureTabletMapper(globalMapper, deviceId);
        NK_LOG_INFO("Mapper configuré pour tablette: {}", info.name);
    } else {
        // Fallback : mapping identité (physical = logical)
        NK_LOG_DEBUG("Utilisation du mapping par défaut pour: {}", info.name);
    }
}

void OnHidDisconnectEvent(const NkHidDisconnectEvent& event, NkGenericHidMapper& globalMapper) {
    const auto deviceId = event.GetDeviceId();

    // Nettoyage des mappings pour libérer la mémoire
    globalMapper.ClearDevice(deviceId);
    NK_LOG_DEBUG("Mappings nettoyés pour device déconnecté: {}", deviceId);
}

// -----------------------------------------------------------------------------
// Exemple 5 : Rechargement dynamique de configuration depuis un fichier
// -----------------------------------------------------------------------------
bool LoadMapperConfigFromFile(NkGenericHidMapper& mapper, const char* configPath) {
    // Pseudo-code de chargement de configuration JSON/XML
    // auto config = ConfigParser::Load(configPath);
    // if (!config.IsValid()) {
    //     NK_LOG_ERROR("Échec du chargement de la configuration: {}", configPath);
    //     return false;
    // }

    // for (const auto& deviceConfig : config.GetDeviceConfigs()) {
    //     const uint64 deviceId = deviceConfig.GetDeviceId();
    //     
    //     // Chargement des mappings de boutons
    //     for (const auto& btnMap : deviceConfig.GetButtonMappings()) {
    //         if (btnMap.IsDisabled()) {
    //             mapper.DisableButton(deviceId, btnMap.GetPhysicalIndex());
    //         } else {
    //             mapper.SetButtonMap(
    //                 deviceId,
    //                 btnMap.GetPhysicalIndex(),
    //                 btnMap.GetLogicalIndex()
    //             );
    //         }
    //     }
    //     
    //     // Chargement des mappings d'axes
    //     for (const auto& axisMap : deviceConfig.GetAxisMappings()) {
    //         if (axisMap.IsDisabled()) {
    //             mapper.DisableAxis(deviceId, axisMap.GetPhysicalIndex());
    //         } else {
    //             mapper.SetAxisMap(
    //                 deviceId,
    //                 axisMap.GetPhysicalIndex(),
    //                 axisMap.GetLogicalIndex(),
    //                 axisMap.GetInvert(),
    //                 axisMap.GetScale(),
    //                 axisMap.GetDeadzone(),
    //                 axisMap.GetOffset()
    //             );
    //         }
    //     }
    // }

    // NK_LOG_INFO("Configuration du mapper chargée depuis: {}", configPath);
    return true;
}

// -----------------------------------------------------------------------------
// Exemple 6 : Test unitaire simple du mapper
// -----------------------------------------------------------------------------
void TestGenericHidMapper() {
    NkGenericHidMapper mapper;
    constexpr uint64 TEST_DEVICE_ID = 9999;

    // Configuration de test
    mapper.SetButtonMap(TEST_DEVICE_ID, 0, 10);  // Physical 0 → Logical 10
    mapper.DisableButton(TEST_DEVICE_ID, 5);     // Physical 5 → Désactivé

    mapper.SetAxisMap(
        TEST_DEVICE_ID,
        0,           // physicalAxis
        20,          // logicalAxis
        false,       // invert
        2.0f,        // scale: double l'amplitude
        0.1f,        // deadzone: 10%
        0.5f         // offset: +0.5
    );

    // Test de résolution de bouton
    NK_ASSERT(mapper.ResolveButton(TEST_DEVICE_ID, 0) == 10);      // Mappé
    NK_ASSERT(mapper.ResolveButton(TEST_DEVICE_ID, 1) == 1);       // Non mappé → identité
    NK_ASSERT(mapper.ResolveButton(TEST_DEVICE_ID, 5) == NK_HID_UNMAPPED); // Désactivé

    // Test de résolution d'axe avec transformations
    uint32 logicalAxis = 0;
    float32 outValue = 0.f;

    // Valeur dans la deadzone → doit être clampée à 0
    bool result = mapper.ResolveAxis(TEST_DEVICE_ID, 0, 0.02f, logicalAxis, outValue);
    NK_ASSERT(result == true);
    NK_ASSERT(logicalAxis == 20);
    NK_ASSERT(outValue == 0.f);  // Deadzone appliquée

    // Valeur normale avec scale et offset
    result = mapper.ResolveAxis(TEST_DEVICE_ID, 0, 0.3f, logicalAxis, outValue);
    NK_ASSERT(result == true);
    NK_ASSERT(logicalAxis == 20);
    // Calcul attendu: ((0.3 * 2.0) + 0.5) = 1.1 → clampé à 1.0
    NK_ASSERT(outValue == 1.0f);

    // Test d'axe désactivé
    mapper.DisableAxis(TEST_DEVICE_ID, 0);
    result = mapper.ResolveAxis(TEST_DEVICE_ID, 0, 0.5f, logicalAxis, outValue);
    NK_ASSERT(result == false);  // Axe désactivé → retourne false

    NK_LOG_INFO("Tests du NkGenericHidMapper passés avec succès");
}

// -----------------------------------------------------------------------------
// Exemple 7 : Intégration dans une boucle de jeu principale
// -----------------------------------------------------------------------------
class GameInputHandler {
public:
    void Initialize() {
        // Création du mapper global
        mHidMapper = std::make_unique<NkGenericHidMapper>();

        // Chargement de la configuration utilisateur
        // LoadMapperConfigFromFile(*mHidMapper, "config/hid_mappings.json");

        // Enregistrement des listeners d'événements
        // auto& eventMgr = Engine::GetEventManager();
        // eventMgr.Subscribe<NkHidButtonPressEvent>(
        //     [this](const NkHidButtonPressEvent& e) {
        //         OnButtonEvent(e);
        //     }
        // );
        // eventMgr.Subscribe<NkHidAxisEvent>(
        //     [this](const NkHidAxisEvent& e) {
        //         OnAxisEvent(e);
        //     }
        // );
    }

    void Shutdown() {
        // Nettoyage des ressources
        if (mHidMapper) {
            mHidMapper->Clear();
            mHidMapper.reset();
        }
    }

private:
    void OnButtonEvent(const NkHidButtonEvent& event) {
        if (!mHidMapper) {
            return;
        }

        uint32 logicalButton = 0;
        NkButtonState state = NkButtonState::NK_RELEASED;

        if (mHidMapper->MapButtonEvent(event, logicalButton, state)) {
            // Traitement de l'entrée logique
            // ProcessLogicalButton(logicalButton, state);
        }
    }

    void OnAxisEvent(const NkHidAxisEvent& event) {
        if (!mHidMapper) {
            return;
        }

        uint32 logicalAxis = 0;
        float32 mappedValue = 0.f;

        if (mHidMapper->MapAxisEvent(event, logicalAxis, mappedValue)) {
            // Traitement de l'entrée logique
            // ProcessLogicalAxis(logicalAxis, mappedValue);
        }
    }

    std::unique_ptr<NkGenericHidMapper> mHidMapper;
};
*/