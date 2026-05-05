#ifndef NKENTSEU_EVENT_NKGAMEPADEVENT_H
#define NKENTSEU_EVENT_NKGAMEPADEVENT_H

#pragma once

// =============================================================================
// Fichier : NkGamepadEvent.h
// =============================================================================
// Description :
//   Définition complète de la hiérarchie des événements pour les manettes de 
//   jeu et joysticks au sein du moteur NkEntseu.
//
// Catégorie : NK_CAT_GAMEPAD
//
// Responsabilités :
//   - Fournir un système d'événements unifié pour tous les types de manettes.
//   - Abstraire les différences entre les layouts Xbox/PlayStation/Nintendo.
//   - Transporter les métadonnées des périphériques connectés.
//   - Supporter les fonctionnalités avancées (rumble, batterie, gyro...).
//
// Enumerations définies :
//   - NkGamepadType  : Famille de manette (Xbox, PlayStation, Nintendo...)
//   - NkGamepadButton : Boutons avec layout universel Xbox-compatible
//   - NkGamepadAxis   : Axes analogiques (sticks, gâchettes, D-pad)
//
// Structures définies :
//   - NkGamepadInfo : Métadonnées complètes d'une manette connectée
//
// Classes définies :
//   - NkGamepadEvent              : Classe de base abstraite
//   - NkGamepadConnectEvent       : Connexion d'une manette
//   - NkGamepadDisconnectEvent    : Déconnexion d'une manette
//   - NkGamepadButtonPressEvent   : Enfoncement d'un bouton
//   - NkGamepadButtonReleaseEvent : Relâchement d'un bouton
//   - NkGamepadAxisEvent          : Modification d'un axe analogique
//   - NkGamepadRumbleEvent        : Commande de vibration haptique (sortant)
//   - NkGamepadBatteryEvent       : Changement du niveau de batterie
//
// Auteur : Équipe NkEntseu
// Version : 1.0.0
// Date : 2026
// =============================================================================

    #include "NkEvent.h"
    #include "NkMouseEvent.h"
    #include "NKContainers/String/NkStringUtils.h"
    #include "NKMemory/NkUtils.h"

    #include <string>

    namespace nkentseu {
        // =========================================================================
        // Section : Énumérations pour l'identification des manettes
        // =========================================================================

        // -------------------------------------------------------------------------
        // Énumération : NkGamepadType
        // -------------------------------------------------------------------------
        // Description :
        //   Classification des familles de manettes pour l'adaptation des 
        //   prompts visuels et des comportements spécifiques.
        // Valeurs :
        //   - NK_GP_TYPE_XBOX        : Manettes Microsoft (360, One, Series X|S)
        //   - NK_GP_TYPE_PLAYSTATION : Manettes Sony (DualShock 3/4, DualSense)
        //   - NK_GP_TYPE_NINTENDO    : Manettes Nintendo (Joy-Con, Pro Controller)
        //   - NK_GP_TYPE_STEAM       : Steam Controller et compatibles
        //   - NK_GP_TYPE_STADIA      : Manette Google Stadia
        //   - NK_GP_TYPE_GENERIC     : Périphérique HID générique non identifié
        //   - NK_GP_TYPE_MOBILE      : Manettes mobiles (iOS MFi, Android)
        // Utilisation :
        //   - Sélection des icônes de boutons dans l'interface utilisateur.
        //   - Adaptation des schémas de contrôle par plateforme.
        // -------------------------------------------------------------------------
        enum class NkGamepadType : uint32 {
            NK_GP_TYPE_UNKNOWN = 0,
            NK_GP_TYPE_XBOX,
            NK_GP_TYPE_PLAYSTATION,
            NK_GP_TYPE_NINTENDO,
            NK_GP_TYPE_STEAM,
            NK_GP_TYPE_STADIA,
            NK_GP_TYPE_GENERIC,
            NK_GP_TYPE_MOBILE,
            NK_GP_TYPE_MAX
        };

        // -------------------------------------------------------------------------
        // Fonction : NkGamepadTypeToString
        // -------------------------------------------------------------------------
        // Description :
        //   Convertit une valeur NkGamepadType en chaîne de caractères lisible.
        // Paramètres :
        //   - t : Valeur de l'énumération à convertir.
        // Retour :
        //   Pointeur vers une chaîne statique décrivant le type de manette.
        // Notes :
        //   - Fonction noexcept : garantie de ne pas lever d'exception.
        //   - Retourne "Unknown" pour toute valeur non reconnue.
        // -------------------------------------------------------------------------
        inline const char* NkGamepadTypeToString(NkGamepadType t) noexcept {
            switch (t) {
                case NkGamepadType::NK_GP_TYPE_XBOX:
                    return "Xbox";
                case NkGamepadType::NK_GP_TYPE_PLAYSTATION:
                    return "PlayStation";
                case NkGamepadType::NK_GP_TYPE_NINTENDO:
                    return "Nintendo";
                case NkGamepadType::NK_GP_TYPE_STEAM:
                    return "Steam";
                case NkGamepadType::NK_GP_TYPE_STADIA:
                    return "Stadia";
                case NkGamepadType::NK_GP_TYPE_GENERIC:
                    return "Generic";
                case NkGamepadType::NK_GP_TYPE_MOBILE:
                    return "Mobile";
                default:
                    return "Unknown";
            }
        }

        // -------------------------------------------------------------------------
        // Énumération : NkGamepadButton
        // -------------------------------------------------------------------------
        // Description :
        //   Layout universel de boutons basé sur le standard Xbox pour une 
        //   abstraction multi-plateforme cohérente.
        // Conventions de nommage :
        //   - Directions cardinales : SOUTH=A/Cross, EAST=B/Circle, etc.
        //   - LB/RB : Bumpers gauche/droit (boutons d'épaule digitaux)
        //   - LT/RT : Triggers/gâchettes (avec variantes digitale/analogique)
        //   - L3/R3 : Clicks des sticks analogiques gauche/droit
        // Correspondances multi-plateformes :
        //   - Xbox : A/B/X/Y, LB/RB, LT/RT, View/Menu, Xbox button
        //   - PlayStation : Cross/Circle/Square/Triangle, L1/R1, L2/R2, Share/Options, PS button
        //   - Nintendo : B/A/Y/X, L/R, ZL/ZR, -/+, Capture, Home button
        // -------------------------------------------------------------------------
        enum class NkGamepadButton : uint32 {
            NK_GP_UNKNOWN = 0,
            NK_GP_SOUTH,
            NK_GP_EAST,
            NK_GP_WEST,
            NK_GP_NORTH,
            NK_GP_LB,
            NK_GP_RB,
            NK_GP_LT_DIGITAL,
            NK_GP_RT_DIGITAL,
            NK_GP_LSTICK,
            NK_GP_RSTICK,
            NK_GP_DPAD_UP,
            NK_GP_DPAD_DOWN,
            NK_GP_DPAD_LEFT,
            NK_GP_DPAD_RIGHT,
            NK_GP_START,
            NK_GP_BACK,
            NK_GP_GUIDE,
            NK_GP_TOUCHPAD,
            NK_GP_CAPTURE,
            NK_GP_MIC,
            NK_GP_PADDLE_1,
            NK_GP_PADDLE_2,
            NK_GP_PADDLE_3,
            NK_GP_PADDLE_4,
            NK_GAMEPAD_BUTTON_MAX
        };

        // -------------------------------------------------------------------------
        // Fonction : NkGamepadButtonToString
        // -------------------------------------------------------------------------
        // Description :
        //   Convertit une valeur NkGamepadButton en chaîne lisible avec aliases.
        // Paramètres :
        //   - b : Valeur de l'énumération à convertir.
        // Retour :
        //   Pointeur vers une chaîne statique avec labels multi-plateformes.
        // Exemple :
        //   NK_GP_SOUTH → "South(A/Cross)" pour affichage UI cohérent.
        // -------------------------------------------------------------------------
        inline const char* NkGamepadButtonToString(NkGamepadButton b) noexcept {
            switch (b) {
                case NkGamepadButton::NK_GP_SOUTH:
                    return "South(A/Cross)";
                case NkGamepadButton::NK_GP_EAST:
                    return "East(B/Circle)";
                case NkGamepadButton::NK_GP_WEST:
                    return "West(X/Square)";
                case NkGamepadButton::NK_GP_NORTH:
                    return "North(Y/Triangle)";
                case NkGamepadButton::NK_GP_LB:
                    return "LB/L1";
                case NkGamepadButton::NK_GP_RB:
                    return "RB/R1";
                case NkGamepadButton::NK_GP_LT_DIGITAL:
                    return "LT";
                case NkGamepadButton::NK_GP_RT_DIGITAL:
                    return "RT";
                case NkGamepadButton::NK_GP_LSTICK:
                    return "L3";
                case NkGamepadButton::NK_GP_RSTICK:
                    return "R3";
                case NkGamepadButton::NK_GP_DPAD_UP:
                    return "DUp";
                case NkGamepadButton::NK_GP_DPAD_DOWN:
                    return "DDown";
                case NkGamepadButton::NK_GP_DPAD_LEFT:
                    return "DLeft";
                case NkGamepadButton::NK_GP_DPAD_RIGHT:
                    return "DRight";
                case NkGamepadButton::NK_GP_START:
                    return "Start";
                case NkGamepadButton::NK_GP_BACK:
                    return "Back";
                case NkGamepadButton::NK_GP_GUIDE:
                    return "Guide";
                case NkGamepadButton::NK_GP_TOUCHPAD:
                    return "Touchpad";
                case NkGamepadButton::NK_GP_CAPTURE:
                    return "Capture";
                case NkGamepadButton::NK_GP_MIC:
                    return "Mic";
                case NkGamepadButton::NK_GP_PADDLE_1:
                    return "Paddle1";
                case NkGamepadButton::NK_GP_PADDLE_2:
                    return "Paddle2";
                case NkGamepadButton::NK_GP_PADDLE_3:
                    return "Paddle3";
                case NkGamepadButton::NK_GP_PADDLE_4:
                    return "Paddle4";
                default:
                    return "Unknown";
            }
        }

        // -------------------------------------------------------------------------
        // Énumération : NkGamepadAxis
        // -------------------------------------------------------------------------
        // Description :
        //   Identification des axes analogiques avec conventions de valeurs.
        // Conventions de valeurs :
        //   - Sticks (LX/LY/RX/RY) : [-1.0, +1.0], centre à 0.0
        //     * LX/RX : -1=gauche, +1=droite
        //     * LY/RY : -1=bas, +1=haut (convention HID : Y+ vers le haut)
        //   - Gâchettes (LT/RT) : [0.0, +1.0], 0=relâchée, 1=enfoncée à fond
        //   - D-Pad analogique : [-1.0, +1.0] ou valeurs discrètes selon hardware
        // Notes :
        //   - La zone morte (deadzone) est appliquée en amont par la couche d'entrée.
        //   - Les valeurs sont déjà normalisées et filtrées avant émission.
        // -------------------------------------------------------------------------
        enum class NkGamepadAxis : uint32 {
            NK_GP_AXIS_LX = 0,
            NK_GP_AXIS_LY,
            NK_GP_AXIS_RX,
            NK_GP_AXIS_RY,
            NK_GP_AXIS_LT,
            NK_GP_AXIS_RT,
            NK_GP_AXIS_DPAD_X,
            NK_GP_AXIS_DPAD_Y,
            NK_GAMEPAD_AXIS_MAX
        };

        // -------------------------------------------------------------------------
        // Fonction : NkGamepadAxisToString
        // -------------------------------------------------------------------------
        // Description :
        //   Convertit une valeur NkGamepadAxis en chaîne de caractères lisible.
        // Paramètres :
        //   - a : Valeur de l'énumération à convertir.
        // Retour :
        //   Pointeur vers une chaîne statique décrivant l'axe analogique.
        // Utilisation :
        //   - Debug logging, profiling, affichage des bindings dans les options.
        // -------------------------------------------------------------------------
        inline const char* NkGamepadAxisToString(NkGamepadAxis a) noexcept {
            switch (a) {
                case NkGamepadAxis::NK_GP_AXIS_LX:
                    return "LeftX";
                case NkGamepadAxis::NK_GP_AXIS_LY:
                    return "LeftY";
                case NkGamepadAxis::NK_GP_AXIS_RX:
                    return "RightX";
                case NkGamepadAxis::NK_GP_AXIS_RY:
                    return "RightY";
                case NkGamepadAxis::NK_GP_AXIS_LT:
                    return "TriggerLeft";
                case NkGamepadAxis::NK_GP_AXIS_RT:
                    return "TriggerRight";
                case NkGamepadAxis::NK_GP_AXIS_DPAD_X:
                    return "DPadX";
                case NkGamepadAxis::NK_GP_AXIS_DPAD_Y:
                    return "DPadY";
                default:
                    return "Unknown";
            }
        }

        // =========================================================================
        // Section : Structures de métadonnées des manettes
        // =========================================================================

        // -------------------------------------------------------------------------
        // Structure : NkGamepadInfo
        // -------------------------------------------------------------------------
        // Description :
        //   Conteneur complet des métadonnées d'une manette connectée.
        //   Centralise toutes les informations d'identification et de capacités.
        // Champs d'identification :
        //   - index     : Indice du joueur (0 = joueur 1, 1 = joueur 2, etc.)
        //   - id        : Identifiant opaque système (GUID SDL, chemin /dev/input...)
        //   - name      : Nom lisible fourni par le périphérique ou le pilote
        //   - type      : Famille de manette détectée (NkGamepadType)
        //   - vendorId/productId : Identifiants USB pour reconnaissance matérielle
        // Champs de capacités :
        //   - numButtons/numAxes : Nombre de contrôles physiques disponibles
        //   - hasRumble          : Support des moteurs de vibration classiques
        //   - hasTriggerRumble   : Support de la vibration dans les gâchettes
        //   - hasTouchpad        : Présence d'un pavé tactile (DualShock/DualSense)
        //   - hasGyro            : Présence de capteurs de mouvement (gyro/accéléro)
        //   - hasLED             : LED de couleur programmable pour feedback visuel
        //   - hasBattery         : Alimentation par batterie (vs connexion filaire)
        // Notes :
        //   - Les tableaux id/name sont initialisés à zéro au constructeur.
        //   - L'index joueur peut changer si les manettes sont reconnectées.
        // -------------------------------------------------------------------------
        struct NkGamepadInfo {
            uint32 index = 0;
            char id[128] = {};
            char name[128] = {};
            NkGamepadType type = NkGamepadType::NK_GP_TYPE_UNKNOWN;
            uint16 vendorId = 0;
            uint16 productId = 0;
            uint32 numButtons = 0;
            uint32 numAxes = 0;
            bool hasRumble = false;
            bool hasTriggerRumble = false;
            bool hasTouchpad = false;
            bool hasGyro = false;
            bool hasLED = false;
            bool hasBattery = false;

            NkGamepadInfo() {
                memory::NkMemSet(id, 0, sizeof(id));
                memory::NkMemSet(name, 0, sizeof(name));
            }
        };

        // =========================================================================
        // Section : Hiérarchie des classes d'événements manette
        // =========================================================================

        // -------------------------------------------------------------------------
        // Classe : NkGamepadEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Classe de base abstraite pour tous les événements de manette.
        //   Centralise l'index du joueur source pour tous les événements dérivés.
        // Héritage :
        //   - Public : NkEvent
        // Catégorie :
        //   - NK_CAT_GAMEPAD (via macro NK_EVENT_CATEGORY_FLAGS)
        // Fonctionnalités :
        //   - Fournit un accès constant à l'index du joueur via GetGamepadIndex().
        //   - Propage l'identifiant de fenêtre source à la classe parente NkEvent.
        // Utilisation :
        //   Ne pas instancier directement. Utiliser les classes dérivées.
        // -------------------------------------------------------------------------
        class NkGamepadEvent : public NkEvent {
            public:
                NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_GAMEPAD)

                uint32 GetGamepadIndex() const noexcept {
                    return mGamepadIndex;
                }

            protected:
                explicit NkGamepadEvent(
                    uint32 index,
                    uint64 windowId = 0
                ) noexcept
                    : NkEvent(windowId)
                    , mGamepadIndex(index) {
                }

                uint32 mGamepadIndex = 0;
        };

        // -------------------------------------------------------------------------
        // Classe : NkGamepadConnectEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lorsqu'une manette est détectée et initialisée.
        // Déclenchement :
        //   - Connexion USB/Bluetooth d'une manette supportée.
        //   - Ré-initialisation après reprise de veille ou reconnexion.
        // Données transportées :
        //   - Structure NkGamepadInfo complète pour identification et configuration.
        // Utilisation typique :
        //   - Attribution d'un slot joueur (player 1, player 2...).
        //   - Chargement de profils de configuration spécifiques par type de manette.
        //   - Mise à jour de l'interface pour afficher les prompts appropriés.
        // -------------------------------------------------------------------------
        class NkGamepadConnectEvent final : public NkGamepadEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_GAMEPAD_CONNECT)

                explicit NkGamepadConnectEvent(
                    const NkGamepadInfo& info,
                    uint64 windowId = 0
                ) noexcept
                    : NkGamepadEvent(info.index, windowId)
                    , mInfo(info) {
                }

                NkEvent* Clone() const override {
                    return new NkGamepadConnectEvent(*this);
                }

                NkString ToString() const override {
                    return NkString::Fmt(
                        "GamepadConnect(#{0} \"{1}\" {2})",
                        mInfo.index,
                        mInfo.name,
                        NkGamepadTypeToString(mInfo.type)
                    );
                }

                const NkGamepadInfo& GetInfo() const noexcept {
                    return mInfo;
                }

            private:
                NkGamepadInfo mInfo;
        };

        // -------------------------------------------------------------------------
        // Classe : NkGamepadDisconnectEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lorsqu'une manette est déconnectée ou devient indisponible.
        // Déclenchement :
        //   - Débranchement physique ou perte de connexion sans fil.
        //   - Mise en veille de la manette ou du système hôte.
        //   - Batterie faible entraînant une extinction automatique.
        // Action requise :
        //   - Libérer le slot joueur associé si nécessaire.
        //   - Mettre à jour l'interface utilisateur (masquer les prompts...).
        //   - Sauvegarder l'état de jeu si la déconnexion est critique.
        // -------------------------------------------------------------------------
        class NkGamepadDisconnectEvent final : public NkGamepadEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_GAMEPAD_DISCONNECT)

                NkGamepadDisconnectEvent(
                    uint32 index,
                    uint64 windowId = 0
                ) noexcept
                    : NkGamepadEvent(index, windowId) {
                }

                NkEvent* Clone() const override {
                    return new NkGamepadDisconnectEvent(*this);
                }

                NkString ToString() const override {
                    return NkString::Fmt(
                        "GamepadDisconnect(#{0})",
                        mGamepadIndex
                    );
                }
        };

        // -------------------------------------------------------------------------
        // Classe : NkGamepadButtonEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Classe de base interne pour les événements de boutons de manette.
        //   Factorise les données communes aux événements press/release.
        // Données transportées :
        //   - button      : Identifiant du bouton dans le layout universel.
        //   - state       : État du bouton (NK_PRESSED / NK_RELEASED).
        //   - analogValue : Valeur analogique [0,1] pour les gâchettes mappées en bouton.
        // Notes :
        //   - Classe protégée : ne pas instancier directement.
        //   - Utiliser NkGamepadButtonPressEvent ou NkGamepadButtonReleaseEvent.
        //   - analogValue permet de gérer le seuil de déclenchement des triggers.
        // -------------------------------------------------------------------------
        class NkGamepadButtonEvent : public NkGamepadEvent {
            public:
                NkGamepadButton GetButton() const noexcept {
                    return mButton;
                }

                NkButtonState GetState() const noexcept {
                    return mState;
                }

                float32 GetAnalogValue() const noexcept {
                    return mAnalogValue;
                }

            protected:
                NkGamepadButtonEvent(
                    uint32 index,
                    NkGamepadButton button,
                    NkButtonState state,
                    float32 analogValue,
                    uint64 windowId
                ) noexcept
                    : NkGamepadEvent(index, windowId)
                    , mButton(button)
                    , mState(state)
                    , mAnalogValue(analogValue) {
                }

                NkGamepadButton mButton = NkGamepadButton::NK_GP_UNKNOWN;
                NkButtonState mState = NkButtonState::NK_RELEASED;
                float32 mAnalogValue = 0.f;
        };

        // -------------------------------------------------------------------------
        // Classe : NkGamepadButtonPressEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lorsqu'un bouton de manette est enfoncé.
        // Déclenchement :
        //   - Appui physique sur un bouton de la manette.
        //   - Activation logicielle simulée (pour tests, macros ou accessibilité).
        // Données transportées :
        //   - Identifiant du bouton, état NK_PRESSED, valeur analogique optionnelle.
        // Utilisation typique :
        //   - Déclenchement d'actions de jeu (saut, tir, interaction...).
        //   - Début d'une séquence de contrôle (ex: combo, charge d'attaque...).
        // Notes :
        //   - Pour les gâchettes, analogValue indique le niveau d'enfoncement.
        //   - analogValue = 1.0 par défaut pour les boutons digitaux.
        // -------------------------------------------------------------------------
        class NkGamepadButtonPressEvent final : public NkGamepadButtonEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_GAMEPAD_BUTTON_PRESSED)

                NkGamepadButtonPressEvent(
                    uint32 index,
                    NkGamepadButton button,
                    float32 analogValue = 1.f,
                    uint64 windowId = 0
                ) noexcept
                    : NkGamepadButtonEvent(
                        index,
                        button,
                        NkButtonState::NK_PRESSED,
                        analogValue,
                        windowId
                    ) {
                }

                NkEvent* Clone() const override {
                    return new NkGamepadButtonPressEvent(*this);
                }

                NkString ToString() const override {
                    return NkString::Fmt(
                        "GamepadButtonPress(#{0} {1})",
                        mGamepadIndex,
                        NkGamepadButtonToString(mButton)
                    );
                }
        };

        // -------------------------------------------------------------------------
        // Classe : NkGamepadButtonReleaseEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lorsqu'un bouton de manette est relâché.
        // Déclenchement :
        //   - Relâchement physique d'un bouton de la manette.
        //   - Désactivation logicielle simulée.
        // Données transportées :
        //   - Identifiant du bouton, état NK_RELEASED.
        // Utilisation typique :
        //   - Fin d'une action continue (ex: visée, accélération...).
        //   - Détection de combos basés sur le timing des relâchements.
        // Notes :
        //   - analogValue est fixé à 0.0 pour les événements de relâchement.
        // -------------------------------------------------------------------------
        class NkGamepadButtonReleaseEvent final : public NkGamepadButtonEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_GAMEPAD_BUTTON_RELEASED)

                NkGamepadButtonReleaseEvent(
                    uint32 index,
                    NkGamepadButton button,
                    uint64 windowId = 0
                ) noexcept
                    : NkGamepadButtonEvent(
                        index,
                        button,
                        NkButtonState::NK_RELEASED,
                        0.f,
                        windowId
                    ) {
                }

                NkEvent* Clone() const override {
                    return new NkGamepadButtonReleaseEvent(*this);
                }

                NkString ToString() const override {
                    return NkString::Fmt(
                        "GamepadButtonRelease(#{0} {1})",
                        mGamepadIndex,
                        NkGamepadButtonToString(mButton)
                    );
                }
        };

        // -------------------------------------------------------------------------
        // Classe : NkGamepadAxisEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lorsqu'un axe analogique de manette change de valeur.
        //   Transporte la valeur courante, précédente et la variation instantanée.
        // Conventions de valeurs :
        //   - Sticks (LX/LY/RX/RY) : [-1.0, +1.0], centre à 0.0
        //   - Gâchettes (LT/RT) : [0.0, +1.0], 0=relâchée, 1=enfoncée
        //   - D-Pad analogique : [-1.0, +1.0] ou valeurs discrètes selon hardware
        // Traitement appliqué :
        //   - Zone morte (deadzone) appliquée en amont : si |value| < deadzone → 0.0
        //   - Valeurs déjà normalisées et filtrées avant émission de l'événement.
        // Données transportées :
        //   - axis      : Identifiant de l'axe dans l'énumération NkGamepadAxis.
        //   - value     : Valeur courante après application de la deadzone.
        //   - prevValue : Valeur précédente pour calcul de delta.
        //   - delta     : Variation instantanée (value - prevValue).
        //   - deadzone  : Seuil de zone morte utilisé pour le filtrage.
        // Utilisation typique :
        //   - Contrôle continu de mouvement, de caméra ou de paramètre.
        //   - Détection de seuils, de zones mortes ou de courbes de réponse.
        // -------------------------------------------------------------------------
        class NkGamepadAxisEvent final : public NkGamepadEvent {
            public:
                static constexpr float32 DEFAULT_DEADZONE = 0.08f;

                NkGamepadAxisEvent(
                    uint32 index,
                    NkGamepadAxis axis,
                    float32 value,
                    float32 prevValue,
                    float32 deadzone = DEFAULT_DEADZONE,
                    uint64 windowId = 0
                ) noexcept
                    : NkGamepadEvent(index, windowId)
                    , mAxis(axis)
                    , mValue(value)
                    , mPrevValue(prevValue)
                    , mDelta(value - prevValue)
                    , mDeadzone(deadzone) {
                }

                NkEvent* Clone() const override {
                    return new NkGamepadAxisEvent(*this);
                }

                NkString ToString() const override {
                    return NkString::Fmt(
                        "GamepadAxis(#{0} {1}={2:.3} delta={3:.3})",
                        mGamepadIndex,
                        NkGamepadAxisToString(mAxis),
                        mValue,
                        mDelta
                    );
                }

                NkGamepadAxis GetAxis() const noexcept {
                    return mAxis;
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

                float32 GetDeadzone() const noexcept {
                    return mDeadzone;
                }

                bool IsInDeadzone() const noexcept {
                    return mValue > -mDeadzone && mValue < mDeadzone;
                }

            private:
                NkGamepadAxis mAxis = NkGamepadAxis::NK_GP_AXIS_LX;
                float32 mValue = 0.f;
                float32 mPrevValue = 0.f;
                float32 mDelta = 0.f;
                float32 mDeadzone = DEFAULT_DEADZONE;
        };

        // -------------------------------------------------------------------------
        // Classe : NkGamepadRumbleEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Commande sortante pour déclencher une vibration haptique sur une manette.
        //   Cet événement est émis par l'application et consommé par le backend d'entrée.
        // Type d'événement :
        //   - Sortant (outgoing) : l'application → le système d'entrée → le hardware.
        //   - Ne pas écouter cet événement pour du gameplay, mais pour du logging/debug.
        // Paramètres de vibration :
        //   - motorLow    : Intensité moteur basse fréquence [0,1] (effets larges, poignée gauche)
        //   - motorHigh   : Intensité moteur haute fréquence [0,1] (détails fins, poignée droite)
        //   - triggerLeft : Intensité vibration gâchette gauche [0,1] (DualSense/Xbox Elite)
        //   - triggerRight: Intensité vibration gâchette droite [0,1]
        //   - durationMs  : Durée de l'effet en millisecondes (0 = infini jusqu'à stop)
        // Utilisation typique :
        //   - Feedback haptique pour impacts, explosions, collisions.
        //   - Effets immersifs : tension de corde d'arc, résistance de gâchette...
        //   - Indications discrètes : direction d'une menace, proximité d'un objet...
        // Notes :
        //   - IsStop() retourne true si toutes les intensités sont à 0.0.
        //   - durationMs = 0 signifie "maintenir jusqu'au prochain appel avec IsStop()=true".
        // -------------------------------------------------------------------------
        class NkGamepadRumbleEvent final : public NkGamepadEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_GAMEPAD_RUMBLE)

                NkGamepadRumbleEvent(
                    uint32 index,
                    float32 motorLow = 0.f,
                    float32 motorHigh = 0.f,
                    float32 triggerLeft = 0.f,
                    float32 triggerRight = 0.f,
                    uint32 durationMs = 100,
                    uint64 windowId = 0
                ) noexcept
                    : NkGamepadEvent(index, windowId)
                    , mMotorLow(motorLow)
                    , mMotorHigh(motorHigh)
                    , mTriggerLeft(triggerLeft)
                    , mTriggerRight(triggerRight)
                    , mDurationMs(durationMs) {
                }

                NkEvent* Clone() const override {
                    return new NkGamepadRumbleEvent(*this);
                }

                NkString ToString() const override {
                    return NkString::Fmt(
                        "GamepadRumble(#{0} lo={1:.3} hi={2:.3} {3}ms)",
                        mGamepadIndex,
                        mMotorLow,
                        mMotorHigh,
                        mDurationMs
                    );
                }

                float32 GetMotorLow() const noexcept {
                    return mMotorLow;
                }

                float32 GetMotorHigh() const noexcept {
                    return mMotorHigh;
                }

                float32 GetTriggerLeft() const noexcept {
                    return mTriggerLeft;
                }

                float32 GetTriggerRight() const noexcept {
                    return mTriggerRight;
                }

                uint32 GetDurationMs() const noexcept {
                    return mDurationMs;
                }

                bool IsStop() const noexcept {
                    return mMotorLow == 0.f
                        && mMotorHigh == 0.f
                        && mTriggerLeft == 0.f
                        && mTriggerRight == 0.f;
                }

            private:
                float32 mMotorLow = 0.f;
                float32 mMotorHigh = 0.f;
                float32 mTriggerLeft = 0.f;
                float32 mTriggerRight = 0.f;
                uint32 mDurationMs = 100;
        };

        // -------------------------------------------------------------------------
        // Classe : NkGamepadBatteryEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lorsque le niveau de batterie d'une manette sans fil change.
        //   Permet d'avertir l'utilisateur et d'adapter le comportement du jeu.
        // Niveaux de batterie :
        //   - level = -1.0 : Manette filaire ou niveau inconnu (IsWired() = true)
        //   - level = [0.0, 1.0] : Proportion de charge restante (0% à 100%)
        // États associés :
        //   - isCharging : true si la manette est actuellement en charge via câble.
        //   - IsCritical() : Retourne true si level < 10% (alerte batterie faible).
        // Utilisation typique :
        //   - Affichage d'une notification ou d'un indicateur UI de batterie.
        //   - Adaptation des effets haptiques pour économiser la batterie.
        //   - Sauvegarde automatique en cas de batterie critique.
        // Notes :
        //   - Cet événement peut être émis périodiquement ou uniquement au changement.
        //   - Le backend d'entrée détermine la fréquence de mise à jour.
        // -------------------------------------------------------------------------
        class NkGamepadBatteryEvent final : public NkGamepadEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_GAMEPAD_CONNECT)

                NkGamepadBatteryEvent(
                    uint32 index,
                    float32 level = -1.f,
                    bool isCharging = false,
                    uint64 windowId = 0
                ) noexcept
                    : NkGamepadEvent(index, windowId)
                    , mLevel(level)
                    , mIsCharging(isCharging) {
                }

                NkEvent* Clone() const override {
                    return new NkGamepadBatteryEvent(*this);
                }

                NkString ToString() const override {
                    NkString level = (mLevel < 0.f)
                        ? NkString("wired")
                        : NkString::Fmt(
                            "{0}%",
                            static_cast<int>(mLevel * 100)
                        );
                    return NkString::Fmt(
                        "GamepadBattery(#{0} {1}{2})",
                        mGamepadIndex,
                        level,
                        mIsCharging ? " charging" : ""
                    );
                }

                float32 GetLevel() const noexcept {
                    return mLevel;
                }

                bool IsCharging() const noexcept {
                    return mIsCharging;
                }

                bool IsWired() const noexcept {
                    return mLevel < 0.f;
                }

                bool IsCritical() const noexcept {
                    return mLevel >= 0.f && mLevel < 0.1f;
                }

            private:
                float32 mLevel = -1.f;
                bool mIsCharging = false;
        };

    } // namespace nkentseu

#endif // NKENTSEU_EVENT_NKGAMEPADEVENT_H

// =============================================================================
// Section : Exemples d'utilisation (à titre informatif - en commentaire)
// =============================================================================

/*
// -----------------------------------------------------------------------------
// Exemple 1 : Gestion de la connexion d'une manette
// -----------------------------------------------------------------------------
void OnGamepadConnected(const NkGamepadConnectEvent& event) {
    // Récupération des informations de la manette
    const auto& info = event.GetInfo();
    const auto playerIndex = event.GetGamepadIndex();

    // Journalisation de la connexion
    NK_LOG_INFO(
        "Manette connectée : Joueur #{} - {} ({})",
        playerIndex,
        info.name,
        NkGamepadTypeToString(info.type)
    );

    // Attribution d'un profil de contrôle selon le type de manette
    // ControlProfile profile;
    // if (info.type == NkGamepadType::NK_GP_TYPE_PLAYSTATION) {
    //     profile = ControlProfile::LoadPlayStationLayout();
    // } else {
    //     profile = ControlProfile::LoadDefaultLayout();
    // }
    // PlayerInput::AssignProfile(playerIndex, profile);

    // Mise à jour de l'interface pour afficher les bons prompts
    // UIManager::UpdateGamepadPrompts(info.type);

    // Activation des fonctionnalités avancées si supportées
    // if (info.hasGyro) {
    //     MotionControls::Enable(playerIndex, true);
    // }
    // if (info.hasTriggerRumble) {
    //     Haptics::EnableTriggerFeedback(playerIndex, true);
    // }
}

// -----------------------------------------------------------------------------
// Exemple 2 : Gestion de la déconnexion d'une manette
// -----------------------------------------------------------------------------
void OnGamepadDisconnected(const NkGamepadDisconnectEvent& event) {
    const auto playerIndex = event.GetGamepadIndex();

    // Libération du slot joueur
    // PlayerInput::ReleaseSlot(playerIndex);

    // Mise en pause si la déconnexion est critique (mode solo)
    // if (GameMode::IsSinglePlayer() && playerIndex == 0) {
    //     Game::PauseWithMessage("Manette déconnectée");
    // }

    // Mise à jour de l'interface
    // UIManager::HideGamepadPrompts(playerIndex);

    NK_LOG_WARN("Manette déconnectée : Joueur #{}", playerIndex);
}

// -----------------------------------------------------------------------------
// Exemple 3 : Traitement des boutons de manette
// -----------------------------------------------------------------------------
void OnGamepadButtonPressed(const NkGamepadButtonPressEvent& event) {
    const auto playerIndex = event.GetGamepadIndex();
    const auto button = event.GetButton();
    const auto analogValue = event.GetAnalogValue();

    // Mapping des actions selon le bouton pressé
    switch (button) {
        case NkGamepadButton::NK_GP_SOUTH:  // A / Cross
            // PlayerActions::Jump(playerIndex);
            break;

        case NkGamepadButton::NK_GP_EAST:  // B / Circle
            // PlayerActions::Cancel(playerIndex);
            break;

        case NkGamepadButton::NK_GP_WEST:  // X / Square
            // PlayerActions::Interact(playerIndex);
            break;

        case NkGamepadButton::NK_GP_NORTH:  // Y / Triangle
            // PlayerActions::SpecialAbility(playerIndex);
            break;

        case NkGamepadButton::NK_GP_LT_DIGITAL:
        case NkGamepadButton::NK_GP_RT_DIGITAL:
            // Gestion des gâchettes avec valeur analogique
            if (analogValue > 0.5f) {
                // PlayerActions::Aim(playerIndex);  // Seuil à 50% d'enfoncement
            }
            break;

        case NkGamepadButton::NK_GP_GUIDE:
            // Ouverture du menu système / pause
            // UIManager::ToggleSystemMenu(playerIndex);
            break;

        default:
            // Bouton non géré : dispatch vers système de mapping personnalisé
            // CustomBindingSystem::Dispatch(playerIndex, button, true);
            break;
    }
}

void OnGamepadButtonReleased(const NkGamepadButtonReleaseEvent& event) {
    const auto playerIndex = event.GetGamepadIndex();
    const auto button = event.GetButton();

    // Actions de relâchement si nécessaire
    switch (button) {
        case NkGamepadButton::NK_GP_LT_DIGITAL:
        case NkGamepadButton::NK_GP_RT_DIGITAL:
            // PlayerActions::StopAiming(playerIndex);
            break;

        default:
            // CustomBindingSystem::Dispatch(playerIndex, button, false);
            break;
    }
}

// -----------------------------------------------------------------------------
// Exemple 4 : Traitement des axes analogiques
// -----------------------------------------------------------------------------
void OnGamepadAxisChanged(const NkGamepadAxisEvent& event) {
    const auto playerIndex = event.GetGamepadIndex();
    const auto axis = event.GetAxis();
    const auto value = event.GetValue();
    const auto delta = event.GetDelta();

    // Ignorer les variations dans la zone morte (déjà filtré mais double-check)
    if (event.IsInDeadzone()) {
        return;
    }

    // Application d'une courbe de réponse non-linéaire pour les sticks
    float32 adjustedValue = value;
    if (axis == NkGamepadAxis::NK_GP_AXIS_LX ||
        axis == NkGamepadAxis::NK_GP_AXIS_LY ||
        axis == NkGamepadAxis::NK_GP_AXIS_RX ||
        axis == NkGamepadAxis::NK_GP_AXIS_RY) {
        // Courbe exponentielle pour plus de précision au centre
        // adjustedValue = math::NkSign(value) * math::NkPow(math::NkFabs(value), 1.5f);
    }

    // Dispatch vers le contrôleur approprié
    switch (axis) {
        case NkGamepadAxis::NK_GP_AXIS_LX:
            // PlayerMovement::SetHorizontalInput(playerIndex, adjustedValue);
            break;

        case NkGamepadAxis::NK_GP_AXIS_LY:
            // PlayerMovement::SetVerticalInput(playerIndex, adjustedValue);
            break;

        case NkGamepadAxis::NK_GP_AXIS_RX:
            // CameraControl::SetHorizontalLook(playerIndex, adjustedValue);
            break;

        case NkGamepadAxis::NK_GP_AXIS_RY:
            // CameraControl::SetVerticalLook(playerIndex, adjustedValue);
            break;

        case NkGamepadAxis::NK_GP_AXIS_LT:
            // VehicleControl::SetBrake(playerIndex, adjustedValue);
            break;

        case NkGamepadAxis::NK_GP_AXIS_RT:
            // VehicleControl::SetThrottle(playerIndex, adjustedValue);
            break;

        default:
            // Axe non géré : logging de débogage
            #if NK_DEBUG
            NK_LOG_DEBUG(
                "Axis unmapped: player={} axis={} value={:.3f}",
                playerIndex,
                NkGamepadAxisToString(axis),
                adjustedValue
            );
            #endif
            break;
    }
}

// -----------------------------------------------------------------------------
// Exemple 5 : Déclenchement de vibrations haptiques (événement sortant)
// -----------------------------------------------------------------------------
void TriggerImpactHaptics(uint32 playerIndex, float32 intensity) {
    // Vibration courte et intense pour un impact
    NkGamepadRumbleEvent rumble(
        playerIndex,
        0.0f,           // motorLow: pas de basse fréquence
        intensity,      // motorHigh: haute fréquence pour l'impact
        0.0f,           // triggerLeft: pas de vibration de gâchette
        0.0f,           // triggerRight
        150             // durationMs: 150ms pour un effet court
    );
    // Engine::GetEventManager().Dispatch(rumble);
}

void TriggerContinuousRumble(uint32 playerIndex, float32 lowFreq, float32 highFreq) {
    // Vibration continue pour un effet prolongé (ex: moteur, tension...)
    NkGamepadRumbleEvent rumble(
        playerIndex,
        lowFreq,        // motorLow: intensité basse fréquence
        highFreq,       // motorHigh: intensité haute fréquence
        0.0f,
        0.0f,
        0               // durationMs: 0 = maintenir jusqu'à appel Stop
    );
    // Engine::GetEventManager().Dispatch(rumble);
}

void StopAllRumble(uint32 playerIndex) {
    // Arrêt de toutes les vibrations pour un joueur
    NkGamepadRumbleEvent stop(playerIndex);  // Toutes intensités à 0.0 par défaut
    NK_ASSERT(stop.IsStop());  // Vérification que l'événement est bien un "stop"
    // Engine::GetEventManager().Dispatch(stop);
}

// -----------------------------------------------------------------------------
// Exemple 6 : Gestion des notifications de batterie
// -----------------------------------------------------------------------------
void OnGamepadBatteryChanged(const NkGamepadBatteryEvent& event) {
    const auto playerIndex = event.GetGamepadIndex();
    const auto level = event.GetLevel();

    if (event.IsWired()) {
        // Manette filaire : masquer l'indicateur de batterie
        // UIManager::HideBatteryIndicator(playerIndex);
        return;
    }

    // Mise à jour de l'indicateur UI de batterie
    // UIManager::UpdateBatteryIndicator(playerIndex, level, event.IsCharging());

    // Notification en cas de batterie faible
    if (event.IsCritical() && !event.IsCharging()) {
        // UIManager::ShowNotification(
        //     playerIndex,
        //     "Batterie faible - Veuillez recharger votre manette",
        //     NotificationPriority::HIGH
        // );

        // Adaptation des effets haptiques pour économiser la batterie
        // Haptics::ReduceIntensity(playerIndex, 0.5f);  // Réduire de 50%
    }

    // Logging pour débogage
    #if NK_DEBUG
    NK_LOG_DEBUG(
        "Battery update: player={} level={}% charging={}",
        playerIndex,
        static_cast<int>(level * 100),
        event.IsCharging() ? "yes" : "no"
    );
    #endif
}

// -----------------------------------------------------------------------------
// Exemple 7 : Enregistrement des listeners d'événements manette
// -----------------------------------------------------------------------------
void RegisterGamepadEventListeners(NkEventManager& eventManager) {
    // Connexion / Déconnexion
    eventManager.Subscribe<NkGamepadConnectEvent>(
        [](const NkGamepadConnectEvent& e) {
            OnGamepadConnected(e);
        }
    );

    eventManager.Subscribe<NkGamepadDisconnectEvent>(
        [](const NkGamepadDisconnectEvent& e) {
            OnGamepadDisconnected(e);
        }
    );

    // Événements de boutons
    eventManager.Subscribe<NkGamepadButtonPressEvent>(
        [](const NkGamepadButtonPressEvent& e) {
            OnGamepadButtonPressed(e);
        }
    );

    eventManager.Subscribe<NkGamepadButtonReleaseEvent>(
        [](const NkGamepadButtonReleaseEvent& e) {
            OnGamepadButtonReleased(e);
        }
    );

    // Événements d'axes analogiques
    eventManager.Subscribe<NkGamepadAxisEvent>(
        [](const NkGamepadAxisEvent& e) {
            OnGamepadAxisChanged(e);
        }
    );

    // Notifications de batterie
    eventManager.Subscribe<NkGamepadBatteryEvent>(
        [](const NkGamepadBatteryEvent& e) {
            OnGamepadBatteryChanged(e);
        }
    );

    // Note : NkGamepadRumbleEvent est un événement sortant, pas besoin de listener
}

// -----------------------------------------------------------------------------
// Exemple 8 : Création et dispatch manuel d'événements (tests/debug)
// -----------------------------------------------------------------------------
void DispatchTestGamepadEvents(NkEventManager& eventManager) {
    constexpr uint32 TEST_PLAYER_INDEX = 0;

    // Simulation de connexion d'une manette Xbox
    NkGamepadInfo xboxInfo;
    xboxInfo.index = TEST_PLAYER_INDEX;
    xboxInfo.type = NkGamepadType::NK_GP_TYPE_XBOX;
    xboxInfo.vendorId = 0x045E;
    xboxInfo.productId = 0x028E;
    xboxInfo.numButtons = 16;
    xboxInfo.numAxes = 6;
    xboxInfo.hasRumble = true;
    xboxInfo.hasBattery = true;
    strcpy_s(xboxInfo.name, "Xbox Wireless Controller");
    strcpy_s(xboxInfo.id, "xinput0");

    NkGamepadConnectEvent connectEvent(xboxInfo);
    eventManager.Dispatch(connectEvent);

    // Simulation d'appui sur le bouton A (SOUTH)
    NkGamepadButtonPressEvent buttonEvent(
        TEST_PLAYER_INDEX,
        NkGamepadButton::NK_GP_SOUTH,
        1.0f,  // analogValue
        0      // windowId
    );
    eventManager.Dispatch(buttonEvent);

    // Simulation de mouvement du stick gauche (LX = 0.5, LY = -0.3)
    NkGamepadAxisEvent axisEventX(
        TEST_PLAYER_INDEX,
        NkGamepadAxis::NK_GP_AXIS_LX,
        0.5f,   // value: légèrement à droite
        0.0f,   // prevValue: centre
        0.08f,  // deadzone
        0       // windowId
    );
    eventManager.Dispatch(axisEventX);

    NkGamepadAxisEvent axisEventY(
        TEST_PLAYER_INDEX,
        NkGamepadAxis::NK_GP_AXIS_LY,
        -0.3f,  // value: légèrement vers le bas (Y- = bas)
        0.0f,   // prevValue
        0.08f,
        0
    );
    eventManager.Dispatch(axisEventY);

    // Simulation de vibration d'impact
    NkGamepadRumbleEvent rumbleEvent(
        TEST_PLAYER_INDEX,
        0.0f,   // motorLow
        0.8f,   // motorHigh: vibration intense
        0.0f,
        0.0f,
        200,    // 200ms de durée
        0
    );
    eventManager.Dispatch(rumbleEvent);

    // Simulation de notification de batterie faible
    NkGamepadBatteryEvent batteryEvent(
        TEST_PLAYER_INDEX,
        0.08f,   // 8% de batterie
        false,   // pas en charge
        0
    );
    NK_ASSERT(batteryEvent.IsCritical());  // Vérification que l'alerte est bien critique
    eventManager.Dispatch(batteryEvent);
}
*/