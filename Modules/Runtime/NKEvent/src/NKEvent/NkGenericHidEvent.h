#ifndef NKENTSEU_EVENT_NKGENERICHIDEVENT_H
#define NKENTSEU_EVENT_NKGENERICHIDEVENT_H

#pragma once

// =============================================================================
// Fichier : NkGenericHidEvent.h
// =============================================================================
// Description :
//   Définition de la hiérarchie des événements pour les périphériques HID 
//   génériques ne relevant pas des catégories spécialisées (clavier, souris, 
//   manette, tactile) au sein du moteur NkEntseu.
//
// Catégorie : NK_CAT_GENERIC_HID
//
// Responsabilités :
//   - Fournir un système d'événements pour les périphériques HID non standard.
//   - Exposer les métadonnées des périphériques connectés (VID/PID, usage...).
//   - Transporter les données brutes HID pour l'interprétation personnalisée.
//
// Périphériques couverts :
//   - Volants de course, pédales, leviers de vitesse
//   - Joysticks de simulation de vol et spatiale
//   - Tablettes graphiques et stylets professionnels
//   - Contrôleurs MIDI et interfaces audio
//   - Périphériques médicaux et industriels spécialisés
//
// Enumerations définies :
//   - NkHidUsagePage : Pages d'usage USB HID (top-level)
//
// Structures définies :
//   - NkHidDeviceInfo : Métadonnées complètes d'un périphérique HID
//
// Classes définies :
//   - NkGenericHidEvent        : Classe de base abstraite
//   - NkHidConnectEvent        : Connexion d'un périphérique HID
//   - NkHidDisconnectEvent     : Déconnexion d'un périphérique HID
//   - NkHidButtonPressEvent    : Enfoncement d'un bouton HID
//   - NkHidButtonReleaseEvent  : Relâchement d'un bouton HID
//   - NkHidAxisEvent           : Modification d'un axe analogique HID
//   - NkHidRawInputEvent       : Rapport HID brut pour traitement personnalisé
//
// Auteur : Équipe NkEntseu
// Version : 1.0.0
// Date : 2026
// =============================================================================

    #include "NkEvent.h"
    #include "NkMouseEvent.h"
    #include "NKMemory/NkUtils.h"
    #include "NKContainers/String/NkStringUtils.h"
    #include "NKCore/NkTraits.h"

    namespace nkentseu {
        // =========================================================================
        // Section : Énumérations et types HID génériques
        // =========================================================================

        // -------------------------------------------------------------------------
        // Énumération : NkHidUsagePage
        // -------------------------------------------------------------------------
        // Description :
        //   Pages d'usage USB HID de premier niveau pour l'identification des 
        //   catégories fonctionnelles des périphériques connectés.
        // Référence :
        //   USB HID Usage Tables Specification v1.3+
        // Utilisation :
        //   - Filtrage des périphériques par catégorie fonctionnelle.
        //   - Interprétation sémantique des contrôles rapportés.
        // -------------------------------------------------------------------------
        enum class NkHidUsagePage : uint16 {
            NK_HID_PAGE_UNDEFINED     = 0x00,
            NK_HID_PAGE_GENERIC       = 0x01,
            NK_HID_PAGE_SIMULATION    = 0x02,
            NK_HID_PAGE_VR            = 0x03,
            NK_HID_PAGE_SPORT         = 0x04,
            NK_HID_PAGE_GAME          = 0x05,
            NK_HID_PAGE_KEYBOARD      = 0x07,
            NK_HID_PAGE_LED           = 0x08,
            NK_HID_PAGE_BUTTON        = 0x09,
            NK_HID_PAGE_DIGITIZER     = 0x0D,
            NK_HID_PAGE_CONSUMER      = 0x0C,
            NK_HID_PAGE_SENSOR        = 0x20,
            NK_HID_PAGE_VENDOR_FIRST  = 0xFF00,
            NK_HID_PAGE_VENDOR_LAST   = 0xFFFF
        };

        // -------------------------------------------------------------------------
        // Structure : NkHidDeviceInfo
        // -------------------------------------------------------------------------
        // Description :
        //   Conteneur de métadonnées pour un périphérique HID connecté.
        //   Centralise toutes les informations d'identification et de capacités.
        // Champs principaux :
        //   - deviceId    : Identifiant opaque unique attribué par le système.
        //   - name/path   : Nom lisible et chemin système du périphérique.
        //   - vendorId/productId : Identifiants USB pour reconnaissance matérielle.
        //   - usagePage/usageId  : Identifiants HID pour interprétation fonctionnelle.
        //   - numButtons/numAxes/numHats : Capacités déclarées du périphérique.
        //   - isWireless  : Indicateur de connexion sans fil.
        // Notes :
        //   - Les tableaux name/path sont initialisés à zéro au constructeur.
        //   - L'identifiant deviceId reste stable pendant la session de connexion.
        // -------------------------------------------------------------------------
        struct NkHidDeviceInfo {
            uint64         deviceId   = 0;
            char          name[128]  = {};
            char          path[256]  = {};
            uint16         vendorId   = 0;
            uint16         productId  = 0;
            uint16         usagePage  = 0;
            uint16         usageId    = 0;
            uint32         numButtons = 0;
            uint32         numAxes    = 0;
            uint32         numHats    = 0;
            bool          isWireless = false;

            NkHidDeviceInfo() {
                memory::NkMemSet(name, 0, sizeof(name));
                memory::NkMemSet(path, 0, sizeof(path));
            }
        };

        // =========================================================================
        // Section : Hiérarchie des classes d'événements HID génériques
        // =========================================================================

        // -------------------------------------------------------------------------
        // Classe : NkGenericHidEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Classe de base abstraite pour tous les événements de périphériques 
        //   HID ne relevant pas des catégories d'entrée standard.
        // Héritage :
        //   - Public : NkEvent
        // Catégorie :
        //   - NK_CAT_GENERIC_HID (via macro NK_EVENT_CATEGORY_FLAGS)
        // Fonctionnalités :
        //   - Centralise l'identifiant du périphérique source pour tous les 
        //     événements dérivés.
        //   - Fournit un accès constant à deviceId via GetDeviceId().
        // Utilisation :
        //   Ne pas instancier directement. Utiliser les classes dérivées.
        // -------------------------------------------------------------------------
        class NkGenericHidEvent : public NkEvent {
            public:
                NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_GENERIC_HID)

                uint64 GetDeviceId() const noexcept {
                    return mDeviceId;
                }

            protected:
                NkGenericHidEvent(
                    uint64 deviceId,
                    uint64 windowId = 0
                ) noexcept
                    : NkEvent(windowId)
                    , mDeviceId(deviceId) {
                }

                uint64 mDeviceId = 0;
        };

        // -------------------------------------------------------------------------
        // Classe : NkHidConnectEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lorsqu'un périphérique HID générique est détecté, 
        //   initialisé et prêt à recevoir des commandes.
        // Déclenchement :
        //   - Détection Plug & Play (USB/BT) d'un périphérique supporté.
        //   - Ré-initialisation après reprise de veille ou reconnexion.
        // Données transportées :
        //   - Structure NkHidDeviceInfo complète pour identification et 
        //     configuration du périphérique.
        // Utilisation typique :
        //   - Enregistrement du périphérique dans le gestionnaire d'entrées.
        //   - Chargement de profils de configuration spécifiques (mapping...).
        // -------------------------------------------------------------------------
        class NkHidConnectEvent final : public NkGenericHidEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_GENERIC_CONNECT)

                explicit NkHidConnectEvent(
                    const NkHidDeviceInfo& info,
                    uint64 windowId = 0
                ) noexcept
                    : NkGenericHidEvent(info.deviceId, windowId)
                    , mInfo(info) {
                }

                NkEvent* Clone() const override {
                    return new NkHidConnectEvent(*this);
                }

                NkString ToString() const override {
                    return NkString::Fmt(
                        "HidConnect({0} VID={1} PID={2})",
                        mInfo.name,
                        mInfo.vendorId,
                        mInfo.productId
                    );
                }

                const NkHidDeviceInfo& GetInfo() const noexcept {
                    return mInfo;
                }

            private:
                NkHidDeviceInfo mInfo;
        };

        // -------------------------------------------------------------------------
        // Classe : NkHidDisconnectEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lorsqu'un périphérique HID générique est déconnecté 
        //   ou devient indisponible.
        // Déclenchement :
        //   - Débranchement physique ou perte de connexion sans fil.
        //   - Mise en veille du périphérique ou du système hôte.
        //   - Erreur de communication ou retrait forcé par le pilote.
        // Action requise :
        //   - Libérer les ressources associées au périphérique (mapping, états...).
        //   - Mettre à jour l'interface utilisateur si nécessaire.
        // -------------------------------------------------------------------------
        class NkHidDisconnectEvent final : public NkGenericHidEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_GENERIC_DISCONNECT)

                NkHidDisconnectEvent(
                    uint64 deviceId,
                    uint64 windowId = 0
                ) noexcept
                    : NkGenericHidEvent(deviceId, windowId) {
                }

                NkEvent* Clone() const override {
                    return new NkHidDisconnectEvent(*this);
                }

                NkString ToString() const override {
                    return NkString::Fmt(
                        "HidDisconnect(device={0})",
                        mDeviceId
                    );
                }
        };

        // -------------------------------------------------------------------------
        // Classe : NkHidButtonEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Classe de base interne pour les événements de boutons HID.
        //   Factorise les données communes aux événements press/release.
        // Données transportées :
        //   - buttonIndex : Index logique du bouton dans le périphérique (0-based).
        //   - state       : État du bouton (NK_PRESSED / NK_RELEASED).
        //   - usagePage/usageId : Identifiants HID standards pour interprétation.
        // Notes :
        //   - Classe protégée : ne pas instancier directement.
        //   - Utiliser NkHidButtonPressEvent ou NkHidButtonReleaseEvent.
        // -------------------------------------------------------------------------
        class NkHidButtonEvent : public NkGenericHidEvent {
            public:
                uint32 GetButtonIndex() const noexcept {
                    return mButtonIndex;
                }

                NkButtonState GetState() const noexcept {
                    return mState;
                }

                uint16 GetUsagePage() const noexcept {
                    return mUsagePage;
                }

                uint16 GetUsageId() const noexcept {
                    return mUsageId;
                }

            protected:
                NkHidButtonEvent(
                    uint64 deviceId,
                    uint32 buttonIndex,
                    NkButtonState state,
                    uint16 usagePage,
                    uint16 usageId,
                    uint64 windowId
                ) noexcept
                    : NkGenericHidEvent(deviceId, windowId)
                    , mButtonIndex(buttonIndex)
                    , mState(state)
                    , mUsagePage(usagePage)
                    , mUsageId(usageId) {
                }

                uint32 mButtonIndex = 0;
                NkButtonState mState = NkButtonState::NK_RELEASED;
                uint16 mUsagePage = 0;
                uint16 mUsageId = 0;
        };

        // -------------------------------------------------------------------------
        // Classe : NkHidButtonPressEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lorsqu'un bouton HID est enfoncé.
        // Déclenchement :
        //   - Appui physique sur un bouton du périphérique.
        //   - Activation logicielle simulée (pour tests ou macros).
        // Données transportées :
        //   - Index du bouton, identifiants HID, état NK_PRESSED.
        // Utilisation typique :
        //   - Déclenchement d'actions de jeu ou d'interface.
        //   - Début d'une séquence de contrôle (ex: freinage, tir...).
        // -------------------------------------------------------------------------
        class NkHidButtonPressEvent final : public NkHidButtonEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_GENERIC_BUTTON_PRESSED)

                NkHidButtonPressEvent(
                    uint64 deviceId,
                    uint32 buttonIndex,
                    uint16 usagePage = 0,
                    uint16 usageId = 0,
                    uint64 windowId = 0
                ) noexcept
                    : NkHidButtonEvent(
                        deviceId,
                        buttonIndex,
                        NkButtonState::NK_PRESSED,
                        usagePage,
                        usageId,
                        windowId
                    ) {
                }

                NkEvent* Clone() const override {
                    return new NkHidButtonPressEvent(*this);
                }

                NkString ToString() const override {
                    return NkString::Fmt(
                        "HidButtonPress(dev={0} btn={1})",
                        mDeviceId,
                        mButtonIndex
                    );
                }
        };

        // -------------------------------------------------------------------------
        // Classe : NkHidButtonReleaseEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lorsqu'un bouton HID est relâché.
        // Déclenchement :
        //   - Relâchement physique d'un bouton du périphérique.
        //   - Désactivation logicielle simulée.
        // Données transportées :
        //   - Index du bouton, identifiants HID, état NK_RELEASED.
        // Utilisation typique :
        //   - Fin d'une action continue (ex: accélération, visée...).
        //   - Détection de combos ou de séquences temporelles.
        // -------------------------------------------------------------------------
        class NkHidButtonReleaseEvent final : public NkHidButtonEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_GENERIC_BUTTON_RELEASED)

                NkHidButtonReleaseEvent(
                    uint64 deviceId,
                    uint32 buttonIndex,
                    uint16 usagePage = 0,
                    uint16 usageId = 0,
                    uint64 windowId = 0
                ) noexcept
                    : NkHidButtonEvent(
                        deviceId,
                        buttonIndex,
                        NkButtonState::NK_RELEASED,
                        usagePage,
                        usageId,
                        windowId
                    ) {
                }

                NkEvent* Clone() const override {
                    return new NkHidButtonReleaseEvent(*this);
                }

                NkString ToString() const override {
                    return NkString::Fmt(
                        "HidButtonRelease(dev={0} btn={1})",
                        mDeviceId,
                        mButtonIndex
                    );
                }
        };

        // -------------------------------------------------------------------------
        // Classe : NkHidAxisEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lorsqu'un axe analogique HID change de valeur.
        //   Transporte à la fois la valeur normalisée et la valeur brute.
        // Normalisation :
        //   - Axes bidirectionnels (joystick, volant) : [-1.0, +1.0]
        //   - Axes unidirectionnels (pédales, gâchettes) : [0.0, +1.0]
        // Données transportées :
        //   - axisIndex : Index logique de l'axe dans le périphérique.
        //   - value/prevValue : Valeurs normalisée actuelle et précédente.
        //   - delta : Variation instantanée (value - prevValue).
        //   - rawValue : Valeur brute entière fournie par le pilote HID.
        //   - usagePage/usageId : Identifiants HID pour interprétation.
        // Utilisation typique :
        //   - Contrôle continu de mouvement, de force ou de paramètre.
        //   - Détection de seuils, de zones mortes ou de courbes de réponse.
        // -------------------------------------------------------------------------
        class NkHidAxisEvent final : public NkGenericHidEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_GENERIC_AXIS_MOTION)

                NkHidAxisEvent(
                    uint64 deviceId,
                    uint32 axisIndex,
                    float32 value,
                    float32 prevValue,
                    int32 rawValue = 0,
                    uint16 usagePage = 0,
                    uint16 usageId = 0,
                    uint64 windowId = 0
                ) noexcept
                    : NkGenericHidEvent(deviceId, windowId)
                    , mAxisIndex(axisIndex)
                    , mValue(value)
                    , mPrevValue(prevValue)
                    , mDelta(value - prevValue)
                    , mRawValue(rawValue)
                    , mUsagePage(usagePage)
                    , mUsageId(usageId) {
                }

                NkEvent* Clone() const override {
                    return new NkHidAxisEvent(*this);
                }

                NkString ToString() const override {
                    return NkString::Fmt(
                        "HidAxis(dev={0} axis={1} val={2:.3} delta={3:.3})",
                        mDeviceId,
                        mAxisIndex,
                        mValue,
                        mDelta
                    );
                }

                uint32 GetAxisIndex() const noexcept {
                    return mAxisIndex;
                }

                float32 GetValue() const noexcept {
                    return mValue;
                }

                float32 GetPrevValue() const noexcept {
                    return mPrevValue;
                }

                float32 GetDelta() const noexcept {
                    return mDelta;
                }

                int32 GetRawValue() const noexcept {
                    return mRawValue;
                }

                uint16 GetUsagePage() const noexcept {
                    return mUsagePage;
                }

                uint16 GetUsageId() const noexcept {
                    return mUsageId;
                }

            private:
                uint32 mAxisIndex = 0;
                float32 mValue = 0.f;
                float32 mPrevValue = 0.f;
                float32 mDelta = 0.f;
                int32 mRawValue = 0;
                uint16 mUsagePage = 0;
                uint16 mUsageId = 0;
        };

        // -------------------------------------------------------------------------
        // Classe : NkHidRawInputEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement transportant un rapport HID brut non interprété.
        //   Permet le traitement personnalisé de périphériques non standard.
        // Cas d'usage :
        //   - Tablettes graphiques avec données de pression/tilt complexes.
        //   - Périphériques médicaux avec protocoles propriétaires.
        //   - Matériel industriel avec formats de rapport spécifiques.
        // Données transportées :
        //   - reportId : Identifiant du rapport HID (0 si unique/non utilisé).
        //   - data     : Vecteur de bytes contenant le rapport complet.
        // Notes importantes :
        //   - L'interprétation des données nécessite la connaissance du 
        //     descripteur HID du périphérique (vendorId + productId).
        //   - Les données sont copiées lors de la construction de l'événement.
        //   - Utiliser GetBytes() et GetSize() pour un accès efficace.
        // -------------------------------------------------------------------------
        class NkHidRawInputEvent final : public NkGenericHidEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_GENERIC_BUTTON_PRESSED)

                NkHidRawInputEvent(
                    uint64 deviceId,
                    uint8 reportId,
                    NkVector<uint8> data,
                    uint64 windowId = 0
                )
                    : NkGenericHidEvent(deviceId, windowId)
                    , mReportId(reportId)
                    , mData(traits::NkMove(data)) {
                }

                NkEvent* Clone() const override {
                    return new NkHidRawInputEvent(*this);
                }

                NkString ToString() const override {
                    return NkString::Fmt(
                        "HidRawInput(dev={0} reportId={1} size={2}B)",
                        mDeviceId,
                        mReportId,
                        mData.Size()
                    );
                }

                uint8 GetReportId() const noexcept {
                    return mReportId;
                }

                const NkVector<uint8>& GetData() const noexcept {
                    return mData;
                }

                const uint8* GetBytes() const noexcept {
                    return mData.Data();
                }

                uint32 GetSize() const noexcept {
                    return static_cast<uint32>(mData.Size());
                }

            private:
                uint8 mReportId = 0;
                NkVector<uint8> mData;
        };

    } // namespace nkentseu

#endif // NKENTSEU_EVENT_NKGENERICHIDEVENT_H

// =============================================================================
// Section : Exemples d'utilisation (à titre informatif - en commentaire)
// =============================================================================

/*
// -----------------------------------------------------------------------------
// Exemple 1 : Gestion de la connexion d'un périphérique HID
// -----------------------------------------------------------------------------
void OnHidDeviceConnected(const NkHidConnectEvent& event) {
    // Récupération des informations du périphérique
    const auto& info = event.GetInfo();
    const auto deviceId = event.GetDeviceId();

    // Journalisation de la connexion
    NK_LOG_INFO(
        "Périphérique HID connecté : {} (VID:0x{:04X} PID:0x{:04X})",
        info.name,
        info.vendorId,
        info.productId
    );

    // Enregistrement dans le gestionnaire d'entrées
    // HidManager::RegisterDevice(deviceId, info);

    // Chargement d'un profil de configuration si disponible
    // auto profile = ConfigLoader::LoadHidProfile(info.vendorId, info.productId);
    // if (profile.IsValid()) {
    //     HidManager::ApplyMapping(deviceId, profile);
    // }
}

// -----------------------------------------------------------------------------
// Exemple 2 : Gestion de la déconnexion d'un périphérique HID
// -----------------------------------------------------------------------------
void OnHidDeviceDisconnected(const NkHidDisconnectEvent& event) {
    const auto deviceId = event.GetDeviceId();

    // Libération des ressources associées au périphérique
    // HidManager::UnregisterDevice(deviceId);

    // Mise à jour de l'interface utilisateur
    // UIManager::NotifyDeviceRemoved(deviceId);

    NK_LOG_WARN("Périphérique HID déconnecté : device={}", deviceId);
}

// -----------------------------------------------------------------------------
// Exemple 3 : Traitement des boutons HID (volant, joystick...)
// -----------------------------------------------------------------------------
void OnHidButtonPressed(const NkHidButtonPressEvent& event) {
    const auto deviceId = event.GetDeviceId();
    const auto buttonIndex = event.GetButtonIndex();
    const auto usagePage = event.GetUsagePage();
    const auto usageId = event.GetUsageId();

    // Interprétation selon la page d'usage HID
    if (usagePage == static_cast<uint16>(NkHidUsagePage::NK_HID_PAGE_SIMULATION)) {
        // Bouton de simulation (volant, avion...)
        switch (usageId) {
            case 0xC5: // Brake
                // VehicleController::ApplyBrake(deviceId, 1.0f);
                break;
            case 0xC4: // Accelerator
                // VehicleController::ApplyThrottle(deviceId, 1.0f);
                break;
            default:
                // Gestion générique des boutons de simulation
                // InputMapper::DispatchSimButton(deviceId, buttonIndex, true);
                break;
        }
    } else {
        // Gestion générique pour autres pages d'usage
        // InputMapper::DispatchGenericButton(deviceId, buttonIndex, true);
    }
}

void OnHidButtonReleased(const NkHidButtonReleaseEvent& event) {
    const auto deviceId = event.GetDeviceId();
    const auto buttonIndex = event.GetButtonIndex();

    // Relâchement du bouton : action inverse ou arrêt
    // InputMapper::DispatchGenericButton(deviceId, buttonIndex, false);
}

// -----------------------------------------------------------------------------
// Exemple 4 : Traitement des axes analogiques HID
// -----------------------------------------------------------------------------
void OnHidAxisChanged(const NkHidAxisEvent& event) {
    const auto deviceId = event.GetDeviceId();
    const auto axisIndex = event.GetAxisIndex();
    const auto value = event.GetValue();
    const auto delta = event.GetDelta();
    const auto rawValue = event.GetRawValue();

    // Application d'une zone morte pour éviter les dérives
    constexpr float DEAD_ZONE = 0.05f;
    if (std::abs(value) < DEAD_ZONE) {
        return; // Ignorer les petites variations autour du centre
    }

    // Application d'une courbe de réponse non-linéaire si configurée
    // float adjustedValue = ResponseCurve::Apply(value, deviceId, axisIndex);

    // Dispatch vers le contrôleur approprié
    // switch (axisIndex) {
    //     case 0: // Steering / Rotation X
    //         VehicleController::SetSteering(deviceId, adjustedValue);
    //         break;
    //     case 1: // Throttle / Rotation Y
    //         VehicleController::SetThrottle(deviceId, adjustedValue);
    //         break;
    //     case 2: // Brake / Rotation Z
    //         VehicleController::SetBrake(deviceId, adjustedValue);
    //         break;
    //     default:
    //         // Axes génériques ou non mappés
    //         InputMapper::DispatchGenericAxis(deviceId, axisIndex, adjustedValue);
    //         break;
    // }

    // Logging de débogage pour calibration
    #if NK_DEBUG
    if (std::abs(delta) > 0.1f) {
        NK_LOG_DEBUG(
            "Axis change: dev={} axis={} val={:.3f} delta={:.3f} raw={}",
            deviceId, axisIndex, value, delta, rawValue
        );
    }
    #endif
}

// -----------------------------------------------------------------------------
// Exemple 5 : Traitement des rapports HID bruts (périphériques non standard)
// -----------------------------------------------------------------------------
void OnHidRawInputReceived(const NkHidRawInputEvent& event) {
    const auto deviceId = event.GetDeviceId();
    const auto reportId = event.GetReportId();
    const auto* data = event.GetBytes();
    const auto size = event.GetSize();

    // Récupération des informations du périphérique pour interprétation
    // const auto* deviceInfo = HidManager::GetDeviceInfo(deviceId);
    // if (!deviceInfo) {
    //     return; // Périphérique non reconnu, ignorer
    // }

    // Interprétation selon VID/PID ou usagePage
    // if (deviceInfo->vendorId == 0x046D && deviceInfo->productId == 0xC262) {
    //     // Exemple : Tablette graphique Wacom spécifique
    //     ParseWacomReport(data, size, reportId);
    // } else if (deviceInfo->usagePage == static_cast<uint16>(NkHidUsagePage::NK_HID_PAGE_SENSOR)) {
    //     // Périphérique avec capteurs (accéléromètre, gyro...)
    //     ParseSensorReport(data, size, reportId);
    // } else {
    //     // Fallback : traitement générique ou logging
    //     NK_LOG_DEBUG(
    //         "Raw HID report: dev={} reportId={} size={}B data=[{}]",
    //         deviceId, reportId, size,
    //         NkStringUtils::BytesToHex(data, std::min(size, 16u))
    //     );
    // }
}

// -----------------------------------------------------------------------------
// Exemple 6 : Filtrage des périphériques par page d'usage HID
// -----------------------------------------------------------------------------
bool IsSimulationDevice(const NkHidDeviceInfo& info) {
    // Vérification de la page d'usage principale
    if (info.usagePage == static_cast<uint16>(NkHidUsagePage::NK_HID_PAGE_SIMULATION)) {
        return true;
    }

    // Vérification secondaire par nom ou capacités
    if (info.numAxes >= 3 && info.numButtons >= 4) {
        // Périphérique avec suffisamment de contrôles pour la simulation
        return true;
    }

    return false;
}

// -----------------------------------------------------------------------------
// Exemple 7 : Enregistrement des listeners d'événements HID génériques
// -----------------------------------------------------------------------------
void RegisterGenericHidEventListeners(NkEventManager& eventManager) {
    // Connexion / Déconnexion de périphériques
    eventManager.Subscribe<NkHidConnectEvent>(
        [](const NkHidConnectEvent& e) {
            OnHidDeviceConnected(e);
        }
    );

    eventManager.Subscribe<NkHidDisconnectEvent>(
        [](const NkHidDisconnectEvent& e) {
            OnHidDeviceDisconnected(e);
        }
    );

    // Événements de boutons
    eventManager.Subscribe<NkHidButtonPressEvent>(
        [](const NkHidButtonPressEvent& e) {
            OnHidButtonPressed(e);
        }
    );

    eventManager.Subscribe<NkHidButtonReleaseEvent>(
        [](const NkHidButtonReleaseEvent& e) {
            OnHidButtonReleased(e);
        }
    );

    // Événements d'axes analogiques
    eventManager.Subscribe<NkHidAxisEvent>(
        [](const NkHidAxisEvent& e) {
            OnHidAxisChanged(e);
        }
    );

    // Rapports bruts pour périphériques non standard
    eventManager.Subscribe<NkHidRawInputEvent>(
        [](const NkHidRawInputEvent& e) {
            OnHidRawInputReceived(e);
        }
    );
}

// -----------------------------------------------------------------------------
// Exemple 8 : Création et dispatch manuel d'événements (tests/debug)
// -----------------------------------------------------------------------------
void DispatchTestHidEvents(NkEventManager& eventManager) {
    // Simulation de connexion d'un volant Logitech G29
    NkHidDeviceInfo wheelInfo;
    wheelInfo.deviceId = 1001;
    wheelInfo.vendorId = 0x046D;
    wheelInfo.productId = 0xC24F;
    wheelInfo.usagePage = static_cast<uint16>(NkHidUsagePage::NK_HID_PAGE_SIMULATION);
    wheelInfo.numAxes = 3;
    wheelInfo.numButtons = 14;
    wheelInfo.numHats = 1;
    strcpy_s(wheelInfo.name, "Logitech G29 Racing Wheel");

    NkHidConnectEvent connectEvent(wheelInfo);
    eventManager.Dispatch(connectEvent);

    // Simulation d'appui sur le bouton "X" (index 0)
    NkHidButtonPressEvent buttonEvent(
        1001,   // deviceId
        0,      // buttonIndex
        0x09,   // usagePage: Button
        0x01,   // usageId: Button 1
        0       // windowId
    );
    eventManager.Dispatch(buttonEvent);

    // Simulation de rotation du volant (axe 0, valeur -0.75)
    NkHidAxisEvent axisEvent(
        1001,   // deviceId
        0,      // axisIndex: steering
        -0.75f, // value: tourné à gauche
        0.0f,   // prevValue: centre
        -32768, // rawValue: valeur brute 16-bit signed
        0x01,   // usagePage: Generic Desktop
        0x30,   // usageId: X axis
        0       // windowId
    );
    eventManager.Dispatch(axisEvent);
}
*/