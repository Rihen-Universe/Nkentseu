#ifndef NKENTSEU_EVENT_NKEVENTDISPATCHER_H
#define NKENTSEU_EVENT_NKEVENTDISPATCHER_H

#pragma once

// =============================================================================
// Fichier : NkEventDispatcher.h
// =============================================================================
// Description :
//   Trois outils complémentaires pour la gestion des événements et des entrées
//   au sein du moteur NkEntseu, offrant à la fois un modèle push (événements)
//   et pull (polling) pour une flexibilité maximale.
//
// Composants :
//
//   1. NkEventDispatcher — Routeur d'événements typés (modèle push)
//      - Analogue à EventBroker::Route<T> de l'ancien système.
//      - Permet de router un NkEvent* vers des handlers fortement typés.
//      - Macro NK_DISPATCH pour simplifier l'enregistrement des callbacks.
//
//   2. NkInputQuery — Interface de polling direct des entrées (modèle pull)
//      - Analogue à InputManager::IsKeyDown / MouseAxis de l'ancien système.
//      - Délègue exactement aux membres de NkEventState (NkEventState.h).
//      - Instance globale stateless : NkInput (accès direct sans injection).
//
//   3. NkActionManager / NkAxisManager — Système d'actions et axes nommés
//      - Inspirés d'ActionManager / AxisManager de l'ancien système.
//      - Adaptés au nouveau système d'événements avec NkInputCode générique.
//      - Support des modificateurs, répétition, et résolution dynamique.
//
// Correspondance avec l'ancien système :
//   EventBroker::Route<T>(fn)     → NkEventDispatcher::Dispatch<T>(fn)
//   REGISTER_CLIENT_EVENT(m)      → NK_DISPATCH(d, T, m)
//   InputManager::IsKeyDown()     → NkInput.IsKeyDown()
//   InputManager::IsMouseDown()   → NkInput.IsMouseDown()
//   InputManager::MousePosition   → NkInput.MouseX() / MouseY()
//   InputManager::MouseDelta      → NkInput.MouseDeltaX() / MouseY()
//   InputManager::GamepadAxis()   → NkInput.GamepadAxis()
//   ActionManager                 → NkActionManager
//   AxisManager                   → NkAxisManager
//
// Auteur : Équipe NkEntseu
// Version : 1.0.0
// Date : 2026
// =============================================================================

    #include "NkEvent.h"
    #include "NkEventState.h"
    #include "NkKeyboardEvent.h"
    #include "NkMouseEvent.h"
    #include "NkGamepadEvent.h"
    #include "NkGamepadSystem.h"
    #include "NKCore/NkTraits.h"

    namespace nkentseu {
        class NkEventSystem;
        class NkGamepadSystem;
    }

    namespace nkentseu {
        // =========================================================================
        // Section : Alias de type pour les handlers d'événements
        // =========================================================================

        // -------------------------------------------------------------------------
        // Alias : NkEventHandler
        // -------------------------------------------------------------------------
        // Description :
        //   Alias template pour les fonctions callback recevant un événement typé.
        //   Retourne bool : true = événement consommé, false = propagation continue.
        // Paramètre template :
        //   - T : Type d'événement dérivé de NkEvent.
        // Signature :
        //   bool handler(T& event)
        // Utilisation :
        //   - Définition des signatures attendues par NkEventDispatcher::Dispatch.
        //   - Compatibilité avec lambdas, fonctions libres et member functions.
        // -------------------------------------------------------------------------
        template<typename T>
        using NkEventHandler = NkFunction<bool(T&)>;

        // =========================================================================
        // Section : Classe NkEventDispatcher (routeur d'événements typés)
        // =========================================================================

        // -------------------------------------------------------------------------
        // Classe : NkEventDispatcher
        // -------------------------------------------------------------------------
        // Description :
        //   Wrappeur léger sur un pointeur NkEvent* permettant de router 
        //   l'événement vers des handlers fortement typés via Dispatch<T>().
        //   Implémente le pattern "type-safe event routing" sans RTTI coûteux.
        // Cycle de vie :
        //   - Créé temporairement dans OnEvent() pour chaque événement reçu.
        //   - Ne possède pas l'événement : référence externe uniquement.
        //   - Thread-safe : lecture seule de l'événement, pas de mutation interne.
        // Fonctionnalités :
        //   - Dispatch<T>() : Route vers handler si type correspond et non-handled.
        //   - GetEvent() : Accès au pointeur NkEvent* sous-jacent.
        //   - IsHandled() : Vérifie si l'événement a déjà été consommé.
        //   - GetEventType() : Retourne le type enum de l'événement courant.
        // Utilisation typique :
        //   @code
        //   void MyClass::OnEvent(NkEvent* ev) {
        //       NkEventDispatcher dispatcher(ev);
        //       NK_DISPATCH(dispatcher, NkKeyPressEvent, OnKeyPress);
        //       NK_DISPATCH(dispatcher, NkMouseButtonEvent, OnMouseButton);
        //   }
        //   @endcode
        // Notes :
        //   - static_assert garantit que T dérive bien de NkEvent à la compilation.
        //   - MarkHandled() appelé automatiquement si handler retourne true.
        // -------------------------------------------------------------------------
        class NkEventDispatcher {
            public:
                explicit NkEventDispatcher(NkEvent* ev) noexcept
                    : mEvent(ev) {
                }

                explicit NkEventDispatcher(NkEvent& ev) noexcept
                    : mEvent(&ev) {
                }

                template<typename T>
                bool Dispatch(NkEventHandler<T> handler) {
                    static_assert(
                        traits::NkIsBaseOf_v<NkEvent, T>,
                        "NkEventDispatcher::Dispatch — T must derive from NkEvent"
                    );
                    if (!mEvent || mEvent->IsHandled()) {
                        return false;
                    }
                    if (mEvent->GetType() != T::GetStaticType()) {
                        return false;
                    }

                    bool consumed = handler(static_cast<T&>(*mEvent));
                    if (consumed) {
                        mEvent->MarkHandled();
                    }
                    return consumed;
                }

                template<typename T, typename Fn>
                bool Dispatch(Fn&& fn) {
                    return Dispatch<T>(
                        NkEventHandler<T>(traits::NkForward<Fn>(fn))
                    );
                }

                NkEvent* GetEvent() const noexcept {
                    return mEvent;
                }

                bool IsHandled() const noexcept {
                    return mEvent && mEvent->IsHandled();
                }

                NkEventType::Value GetEventType() const noexcept {
                    return mEvent
                        ? mEvent->GetType()
                        : NkEventType::NK_NONE;
                }

            private:
                NkEvent* mEvent = nullptr;
        };

        // =========================================================================
        // Section : Macros de commodité pour l'enregistrement des handlers
        // =========================================================================

        // -------------------------------------------------------------------------
        // Macro : NK_DISPATCH
        // -------------------------------------------------------------------------
        // Description :
        //   Macro simplifiant l'enregistrement d'un handler member function
        //   pour un type d'événement spécifique via NkEventDispatcher.
        // Paramètres :
        //   - dispatcher_ : Instance de NkEventDispatcher à utiliser.
        //   - EventType_  : Type d'événement cible (dérivé de NkEvent).
        //   - method_     : Nom de la member function à appeler.
        // Signature attendue de method_ :
        //   bool method_(EventType_& e)
        // Expansion :
        //   Crée un lambda capturant 'this' qui appelle la méthode spécifiée.
        // Utilisation :
        //   @code
        //   void MyClass::OnEvent(NkEvent* ev) {
        //       NkEventDispatcher dispatcher(ev);
        //       NK_DISPATCH(dispatcher, NkKeyPressEvent, OnKeyPress);
        //   }
        //
        //   bool MyClass::OnKeyPress(NkKeyPressEvent& e) {
        //       // Traitement de l'événement...
        //       return true; // true = consommé, false = propagation
        //   }
        //   @endcode
        // -------------------------------------------------------------------------
        #define NK_DISPATCH(dispatcher_, EventType_, method_) \
            (dispatcher_).Dispatch<EventType_>( \
                [this](EventType_& e_) -> bool { \
                    return this->method_(e_); \
                } \
            )

        // -------------------------------------------------------------------------
        // Macro : NK_DISPATCH_FREE
        // -------------------------------------------------------------------------
        // Description :
        //   Macro pour l'enregistrement d'un handler libre (fonction ou lambda)
        //   sans capture de 'this', pour plus de flexibilité.
        // Paramètres :
        //   - dispatcher_ : Instance de NkEventDispatcher à utiliser.
        //   - EventType_  : Type d'événement cible (dérivé de NkEvent).
        //   - fn_         : Fonction, lambda ou functor à appeler.
        // Signature attendue de fn_ :
        //   bool fn_(EventType_& e)  // ou compatible via conversion
        // Utilisation :
        //   @code
        //   // Avec une fonction libre
        //   bool HandleKeyPress(NkKeyPressEvent& e) { /*...*/ return true; }
        //   NK_DISPATCH_FREE(dispatcher, NkKeyPressEvent, HandleKeyPress);
        //
        //   // Avec un lambda
        //   NK_DISPATCH_FREE(dispatcher, NkKeyPressEvent,
        //       [](NkKeyPressEvent& e) {
        //           // Traitement...
        //           return true;
        //       }
        //   );
        //   @endcode
        // -------------------------------------------------------------------------
        #define NK_DISPATCH_FREE(dispatcher_, EventType_, fn_) \
            (dispatcher_).Dispatch<EventType_>(fn_)

        // =========================================================================
        // Section : Classe NkInputQuery (polling direct des entrées)
        // =========================================================================

        // -------------------------------------------------------------------------
        // Classe : NkInputQuery
        // -------------------------------------------------------------------------
        // Description :
        //   Interface de polling direct pour interroger l'état courant des 
        //   périphériques d'entrée sans attendre les callbacks d'événements.
        //   Délègue exactement aux membres de NkEventState pour cohérence.
        // Architecture :
        //   - Instance globale stateless : NkInput (déclarée inline plus bas).
        //   - Injection de dépendance via SetEventSystem/SetGamepadSystem.
        //   - Fallback vers dummy state si systèmes non initialisés (safe par défaut).
        // Catégories d'entrées supportées :
        //   - Clavier : IsKeyDown(), IsKeyRepeated(), modificateurs, lastKey...
        //   - Souris  : Position, delta, raw delta, boutons, insideWindow...
        //   - Manette : IsGamepadDown(), GamepadAxis(), connexion, rumble...
        // Thread-safety :
        //   - Lecture seule : toutes les méthodes sont const et thread-safe.
        //   - Écriture réservée à NkEventSystem via les setters d'injection.
        // Utilisation typique :
        //   @code
        //   // Dans la boucle de jeu
        //   if (NkInput.IsKeyDown(NkKey::NK_SPACE)) {
        //       Player::Jump();
        //   }
        //
        //   // Contrôle de caméra FPS avec mouvement brut
        //   float yaw = -NkInput.MouseRawDeltaX() * 0.002f;
        //   Camera::RotateYaw(yaw);
        //
        //   // Input manette pour joueur 0
        //   if (NkInput.IsGamepadDown(0, NkGamepadButton::NK_GP_SOUTH)) {
        //       Player::Fire(0);
        //   }
        //   @endcode
        // Notes :
        //   - N'attend pas les événements : état snapshot au moment de l'appel.
        //   - Idéal pour le gameplay réactif où la latence callback est critique.
        // -------------------------------------------------------------------------
        class NkInputQuery {
            public:
                // -----------------------------------------------------------------
                // Section : Méthodes de polling clavier
                // -----------------------------------------------------------------
                // Description :
                //   Interrogation de l'état courant du clavier via NkKeyboardInputState.
                //   Toutes les méthodes sont const et noexcept pour performance.
                // -----------------------------------------------------------------

                bool IsKeyDown(NkKey key) const noexcept;

                bool IsKeyRepeated(NkKey key) const noexcept;

                bool IsCtrlDown() const noexcept;

                bool IsAltDown() const noexcept;

                bool IsShiftDown() const noexcept;

                bool IsSuperDown() const noexcept;

                NkKey LastKey() const noexcept;

                NkScancode LastScancode() const noexcept;

                // -----------------------------------------------------------------
                // Section : Méthodes de polling souris
                // -----------------------------------------------------------------
                // Description :
                //   Interrogation de l'état courant de la souris via NkMouseInputState.
                //   Coordonnées en pixels physiques, origine en haut-gauche fenêtre.
                // -----------------------------------------------------------------

                int32 MouseX() const noexcept;

                int32 MouseY() const noexcept;

                int32 MouseDeltaX() const noexcept;

                int32 MouseDeltaY() const noexcept;

                int32 MouseRawDeltaX() const noexcept;

                int32 MouseRawDeltaY() const noexcept;

                bool IsMouseDown(NkMouseButton btn) const noexcept;

                bool IsLeftDown() const noexcept;

                bool IsRightDown() const noexcept;

                bool IsMiddleDown() const noexcept;

                bool IsAnyMouseDown() const noexcept;

                bool IsMouseInside() const noexcept;

                // -----------------------------------------------------------------
                // Section : Méthodes de polling manette
                // -----------------------------------------------------------------
                // Description :
                //   Interrogation de l'état des manettes via NkGamepadSetState.
                //   Index de slot : 0 = joueur 1, 1 = joueur 2, etc.
                // -----------------------------------------------------------------

                bool IsGamepadDown(
                    uint32 idx,
                    NkGamepadButton btn
                ) const noexcept;

                float32 GamepadAxis(
                    uint32 idx,
                    NkGamepadAxis ax
                ) const noexcept;

                bool IsGamepadConnected(uint32 idx) const noexcept;

                void GamepadRumble(
                    uint32 idx,
                    float32 motorLow = 0.f,
                    float32 motorHigh = 0.f,
                    float32 triggerLeft = 0.f,
                    float32 triggerRight = 0.f,
                    uint32 durationMs = 100
                ) const;

                // -----------------------------------------------------------------
                // Section : Injection de dépendances (réservé à NkSystem)
                // -----------------------------------------------------------------
                // Description :
                //   Méthodes pour injecter les pointeurs vers les systèmes globaux.
                //   Appelées automatiquement par NkSystem::Initialize().
                //   Ne pas appeler manuellement sauf pour tests unitaires.
                // -----------------------------------------------------------------

                void SetEventSystem(NkEventSystem* es) noexcept {
                    mEventSystem = es;
                }

                void SetGamepadSystem(NkGamepadSystem* gs) noexcept {
                    mGamepadSystem = gs;
                }

            private:
                const NkEventState& State() const noexcept;

                NkEventSystem* mEventSystem = nullptr;
                NkGamepadSystem* mGamepadSystem = nullptr;
        };

        // -------------------------------------------------------------------------
        // Variable globale : NkInput
        // -------------------------------------------------------------------------
        // Description :
        //   Instance globale stateless de NkInputQuery pour accès direct.
        //   Toutes les méthodes étant const, pas de risque de race condition.
        //   Initialisée inline : pas de problème d'ordre d'initialisation statique.
        // Utilisation :
        //   - Accès direct : NkInput.IsKeyDown(NkKey::NK_SPACE)
        //   - Pas besoin d'instancier : déjà disponible partout après inclusion.
        // Notes :
        //   - Les pointeurs internes sont nullptr jusqu'à injection par NkSystem.
        //   - Fallback vers dummy state si non initialisé : retourne valeurs par défaut.
        // -------------------------------------------------------------------------
        inline NkInputQuery NkInput;

        // =========================================================================
        // Section : Système d'actions et axes nommés (NkActionManager / NkAxisManager)
        // =========================================================================

        // -------------------------------------------------------------------------
        // Énumération : NkInputDevice
        // -------------------------------------------------------------------------
        // Description :
        //   Classification des périphériques d'entrée pour NkInputCode.
        //   Permet d'unifier clavier, souris et manette dans un code générique.
        // Valeurs :
        //   - NK_KEYBOARD     : Touches du clavier (NkKey)
        //   - NK_MOUSE        : Boutons de souris (NkMouseButton)
        //   - NK_MOUSEWHEEL   : Molette de souris (horizontal/vertical)
        //   - NK_GAMEPAD      : Boutons de manette (NkGamepadButton)
        //   - NK_GAMEPAD_AXIS : Axes analogiques de manette (NkGamepadAxis)
        // Utilisation :
        //   - Champ 'device' de NkInputCode pour identification du périphérique.
        //   - Routage dans NkAxisResolver pour lecture de la valeur appropriée.
        // -------------------------------------------------------------------------
        enum class NkInputDevice : uint32 {
            NK_KEYBOARD = 0,
            NK_MOUSE = 1,
            NK_MOUSEWHEEL = 2,
            NK_GAMEPAD = 3,
            NK_GAMEPAD_AXIS = 4
        };

        // -------------------------------------------------------------------------
        // Structure : NkInputCode
        // -------------------------------------------------------------------------
        // Description :
        //   Code d'entrée générique identifiant de manière unique une source 
        //   d'input (touche, bouton, axe) avec modificateurs optionnels.
        // Champs :
        //   - device   : Type de périphérique (NkInputDevice).
        //   - code     : Code spécifique au périphérique (NkKey, NkMouseButton...).
        //   - modifier : Masque de modificateurs (Ctrl, Alt, Shift, Super).
        // Méthodes statiques de construction :
        //   - Key()        : Crée un code pour une touche clavier.
        //   - Mouse()      : Crée un code pour un bouton souris.
        //   - Wheel()      : Crée un code pour la molette (horizontal/vertical).
        //   - Gamepad()    : Crée un code pour un bouton de manette.
        //   - GamepadAxis(): Crée un code pour un axe de manette.
        // Opérateurs :
        //   - operator== / != : Comparaison pour lookup dans les maps de commandes.
        // Utilisation :
        //   - Clé dans les maps NkActionManager::mCommands et NkAxisManager::mCommands.
        //   - Transport dans les callbacks NkActionSubscriber / NkAxisSubscriber.
        // -------------------------------------------------------------------------
        struct NkInputCode {
            NkInputDevice device = NkInputDevice::NK_KEYBOARD;
            uint32 code = 0;
            uint32 modifier = 0;

            bool operator==(const NkInputCode& o) const noexcept {
                return device == o.device
                    && code == o.code
                    && modifier == o.modifier;
            }

            bool operator!=(const NkInputCode& o) const noexcept {
                return !(*this == o);
            }

            static NkInputCode Key(NkKey k, uint32 mod = 0) noexcept {
                return {
                    NkInputDevice::NK_KEYBOARD,
                    static_cast<uint32>(k),
                    mod
                };
            }

            static NkInputCode Mouse(NkMouseButton b, uint32 mod = 0) noexcept {
                return {
                    NkInputDevice::NK_MOUSE,
                    static_cast<uint32>(b),
                    mod
                };
            }

            static NkInputCode Wheel(bool horizontal = false) noexcept {
                return {
                    NkInputDevice::NK_MOUSEWHEEL,
                    horizontal ? 1u : 0u,
                    0
                };
            }

            static NkInputCode Gamepad(NkGamepadButton b, uint32 mod = 0) noexcept {
                return {
                    NkInputDevice::NK_GAMEPAD,
                    static_cast<uint32>(b),
                    mod
                };
            }

            static NkInputCode GamepadAxis(NkGamepadAxis a) noexcept {
                return {
                    NkInputDevice::NK_GAMEPAD_AXIS,
                    static_cast<uint32>(a),
                    0
                };
            }
        };

        // -------------------------------------------------------------------------
        // Structure : NkActionCommand
        // -------------------------------------------------------------------------
        // Description :
        //   Commande d'action nommée associant un nom logique à un NkInputCode.
        //   Permet le rebinding dynamique des contrôles sans changer le code jeu.
        // Champs :
        //   - mName           : Nom logique de l'action (ex: "Jump", "Fire").
        //   - mCode           : Code d'entrée physique associé (NkInputCode).
        //   - mRepeatable     : Si true, l'action peut se répéter en maintien.
        //   - mPrivRepeatable : Flag interne pour gestion de l'état de répétition.
        // Constructeur :
        //   - Prend nom, code et optionnellement repeatable (défaut: true).
        //   - Initialise mPrivRepeatable à la même valeur que mRepeatable.
        // Méthodes d'accès :
        //   - GetName()              : Retourne le nom logique de l'action.
        //   - GetCode()              : Retourne le code d'entrée physique.
        //   - IsRepeatable()         : Retourne la configurabilité de répétition.
        //   - IsPrivateRepeatable()  : Retourne l'état interne de répétition.
        //   - SetPrivateRepeatable() : Modifie l'état interne (réservé au manager).
        // Opérateurs :
        //   - operator== / != : Comparaison basée uniquement sur le code (pour lookup).
        // Utilisation :
        //   - Ajout via NkActionManager::AddCommand().
        //   - Déclenchement via NkActionManager::TriggerAction().
        // Notes :
        //   - mPrivRepeatable gère l'état "déjà déclenché en maintien".
        //   - Comparaison par code uniquement : une action = un input physique.
        // -------------------------------------------------------------------------
        struct NkActionCommand {
            NkActionCommand(
                NkString name,
                NkInputCode code,
                bool repeatable = true
            )
                : mName(traits::NkMove(name))
                , mCode(code)
                , mRepeatable(repeatable)
                , mPrivRepeatable(repeatable) {
            }

            const NkString& GetName() const noexcept {
                return mName;
            }

            const NkInputCode& GetCode() const noexcept {
                return mCode;
            }

            bool IsRepeatable() const noexcept {
                return mRepeatable;
            }

            bool IsPrivateRepeatable() const noexcept {
                return mPrivRepeatable;
            }

            void SetPrivateRepeatable(bool v) noexcept {
                mPrivRepeatable = v;
            }

            bool operator==(const NkActionCommand& o) const noexcept {
                return mCode == o.mCode;
            }

            bool operator!=(const NkActionCommand& o) const noexcept {
                return !(*this == o);
            }

        private:
            NkString mName;
            NkInputCode mCode;
            bool mRepeatable = true;
            bool mPrivRepeatable = true;

            friend class NkActionManager;
        };

        // -------------------------------------------------------------------------
        // Alias : NkActionSubscriber
        // -------------------------------------------------------------------------
        // Description :
        //   Signature des callbacks pour les actions nommées.
        //   Appelé lorsqu'une action est déclenchée ou relâchée.
        // Paramètres du callback :
        //   - actionName : Nom logique de l'action déclenchée.
        //   - code       : Code d'entrée physique ayant déclenché l'action.
        //   - isPressed  : true = pression, false = relâchement.
        //   - isRepeat   : true = événement de répétition (maintien), false = initial.
        // Utilisation :
        //   - Enregistré via NkActionManager::CreateAction().
        //   - Appelé par NkActionManager::FireAction() lors du déclenchement.
        // Exemple :
        //   @code
        //   manager.CreateAction("Jump", [](
        //       const NkString& name,
        //       const NkInputCode& code,
        //       bool isPressed,
        //       bool isRepeat
        //   ) {
        //       if (isPressed && !isRepeat) {
        //           Player::Jump();  // Saut uniquement au premier appui
        //       }
        //   });
        //   @endcode
        // -------------------------------------------------------------------------
        using NkActionSubscriber = NkFunction<void(
            const NkString&,
            const NkInputCode&,
            bool,
            bool
        )>;

        // -------------------------------------------------------------------------
        // Structure : NkAxisCommand
        // -------------------------------------------------------------------------
        // Description :
        //   Commande d'axe nommée associant un nom logique à un NkInputCode 
        //   avec paramètres de transformation (scale, deadzone via minInterval).
        // Champs :
        //   - mName        : Nom logique de l'axe (ex: "MoveHorizontal", "LookY").
        //   - mCode        : Code d'entrée physique associé (NkInputCode).
        //   - mScale       : Facteur de multiplication appliqué à la valeur brute.
        //   - mMinInterval : Seuil minimal absolu pour déclencher le callback.
        // Constructeur :
        //   - Prend nom, code, scale (défaut: 1.0) et minInterval (défaut: 0.0).
        // Méthodes d'accès :
        //   - GetName()        : Retourne le nom logique de l'axe.
        //   - GetCode()        : Retourne le code d'entrée physique.
        //   - GetScale()       : Retourne le facteur de scale configuré.
        //   - GetMinInterval() : Retourne le seuil minimal de déclenchement.
        // Opérateurs :
        //   - operator== / != : Comparaison basée uniquement sur le code.
        // Utilisation :
        //   - Ajout via NkAxisManager::AddCommand().
        //   - Évaluation via NkAxisManager::UpdateAxes() avec resolver.
        // Notes :
        //   - minInterval agit comme deadzone : |value*scale| < minInterval → ignoré.
        //   - Scale appliqué avant le seuil : permet d'amplifier ou réduire la sensibilité.
        // -------------------------------------------------------------------------
        struct NkAxisCommand {
            NkAxisCommand(
                NkString name,
                NkInputCode code,
                float scale = 1.f,
                float minInterval = 0.f
            )
                : mName(traits::NkMove(name))
                , mCode(code)
                , mScale(scale)
                , mMinInterval(minInterval) {
            }

            const NkString& GetName() const noexcept {
                return mName;
            }

            const NkInputCode& GetCode() const noexcept {
                return mCode;
            }

            float GetScale() const noexcept {
                return mScale;
            }

            float GetMinInterval() const noexcept {
                return mMinInterval;
            }

            bool operator==(const NkAxisCommand& o) const noexcept {
                return mCode == o.mCode;
            }

            bool operator!=(const NkAxisCommand& o) const noexcept {
                return !(*this == o);
            }

        private:
            NkString mName;
            NkInputCode mCode;
            float mScale = 1.f;
            float mMinInterval = 0.f;
        };

        // -------------------------------------------------------------------------
        // Alias : NkAxisSubscriber
        // -------------------------------------------------------------------------
        // Description :
        //   Signature des callbacks pour les axes nommés.
        //   Appelé à chaque frame lors de UpdateAxes() si seuil dépassé.
        // Paramètres du callback :
        //   - axisName : Nom logique de l'axe évalué.
        //   - code     : Code d'entrée physique source de la valeur.
        //   - value    : Valeur finale après application du scale et seuil.
        // Utilisation :
        //   - Enregistré via NkAxisManager::CreateAxis().
        //   - Appelé par NkAxisManager::FireAxis() lors de l'évaluation.
        // Exemple :
        //   @code
        //   manager.CreateAxis("MoveHorizontal", [](
        //       const NkString& name,
        //       const NkInputCode& code,
        //       float value
        //   ) {
        //       Player::SetHorizontalInput(value);  // value ∈ [-1, +1] après transformations
        //   });
        //   @endcode
        // Notes :
        //   - Appelé même si value = 0, tant que |value| >= minInterval.
        //   - Fréquence d'appel : chaque frame via UpdateAxes(), pas événementiel.
        // -------------------------------------------------------------------------
        using NkAxisSubscriber = NkFunction<void(
            const NkString&,
            const NkInputCode&,
            float
        )>;

        // -------------------------------------------------------------------------
        // Classe : NkActionManager
        // -------------------------------------------------------------------------
        // Description :
        //   Gestionnaire d'actions nommées permettant le mapping dynamique 
        //   entre inputs physiques et actions logiques du jeu.
        // Responsabilités :
        //   - Enregistrement des actions avec leurs handlers (CreateAction).
        //   - Association d'inputs physiques aux actions (AddCommand).
        //   - Déclenchement des handlers quand un input correspond (TriggerAction).
        //   - Gestion de la répétition pour les inputs maintenus.
        // Structures internes :
        //   - mActions  : Map nom → handler (NkActionSubscriber) pour dispatch.
        //   - mCommands : Map nom → liste de NkActionCommand pour lookup inverse.
        // Méthodes publiques :
        //   - CreateAction()  : Enregistre un handler pour une action nommée.
        //   - AddCommand()    : Associe un input physique à une action.
        //   - RemoveAction()  : Supprime une action et ses commandes associées.
        //   - RemoveCommand() : Dissocie un input physique d'une action.
        //   - TriggerAction() : Appelé par le dispatcher quand un input arrive.
        //   - GetActionCount()/GetCommandCount() : Métriques pour debugging.
        // Thread-safety :
        //   - Non thread-safe par défaut. Synchronisation externe requise.
        //   - TriggerAction() typiquement appelé depuis le thread principal.
        // Utilisation typique :
        //   @code
        //   NkActionManager actionMgr;
        //
        //   // Enregistrement de l'action "Jump"
        //   actionMgr.CreateAction("Jump", [](
        //       const NkString& name,
        //       const NkInputCode& code,
        //       bool isPressed,
        //       bool isRepeat
        //   ) {
        //       if (isPressed && !isRepeat) {
        //           Player::Jump();
        //       }
        //   });
        //
        //   // Association de la touche Espace à "Jump"
        //   actionMgr.AddCommand(NkActionCommand(
        //       "Jump",
        //       NkInputCode::Key(NkKey::NK_SPACE)
        //   ));
        //
        //   // Dans la boucle d'événements :
        //   // NkEventDispatcher dispatcher(ev);
        //   // NK_DISPATCH(dispatcher, NkKeyPressEvent, [this, &actionMgr](NkKeyPressEvent& e) {
        //   //     actionMgr.TriggerAction(NkInputCode::Key(e.GetKey()), true);
        //   //     return false;
        //   // });
        //   @endcode
        // -------------------------------------------------------------------------
        class NkActionManager {
            public:
                void CreateAction(
                    const NkString& name,
                    NkActionSubscriber handler
                );

                void AddCommand(const NkActionCommand& cmd);

                void RemoveAction(const NkString& name);

                void RemoveCommand(const NkActionCommand& cmd);

                void TriggerAction(
                    const NkInputCode& code,
                    bool isPressed
                );

                uint64 GetActionCount() const noexcept {
                    return mActions.Size();
                }

                uint64 GetCommandCount() const noexcept;

                uint64 GetCommandCount(const NkString& name) const noexcept;

            private:
                void FireAction(
                    const NkString& name,
                    const NkInputCode& code,
                    bool isPressed,
                    NkActionCommand& cmd
                );

                NkUnorderedMap<NkString, NkActionSubscriber> mActions;
                NkUnorderedMap<NkString, NkVector<NkActionCommand>> mCommands;
        };

        // -------------------------------------------------------------------------
        // Alias : NkAxisResolver
        // -------------------------------------------------------------------------
        // Description :
        //   Callback de résolution de valeur pour les axes nommés.
        //   Convertit un (device, code) en valeur float lue depuis l'état d'entrée.
        // Signature :
        //   float resolver(NkInputDevice device, uint32 code)
        // Paramètres :
        //   - device : Type de périphérique source (NkInputDevice).
        //   - code   : Code spécifique au périphérique (NkKey, NkGamepadAxis...).
        // Retour :
        //   - Valeur float normalisée typiquement dans [-1, +1] ou [0, +1].
        //   - 0.0 si l'input n'est pas actif ou non reconnu.
        // Utilisation :
        //   - Passé à NkAxisManager::UpdateAxes() pour évaluation des axes.
        //   - Implémenté typiquement via switch sur device + polling NkInput.
        // Exemple d'implémentation :
        //   @code
        //   NkAxisResolver resolver = [](NkInputDevice dev, uint32 code) -> float {
        //       switch (dev) {
        //           case NkInputDevice::NK_KEYBOARD:
        //               return NkInput.IsKeyDown(static_cast<NkKey>(code)) ? 1.f : 0.f;
        //           case NkInputDevice::NK_GAMEPAD_AXIS:
        //               return NkInput.GamepadAxis(0, static_cast<NkGamepadAxis>(code));
        //           default:
        //               return 0.f;
        //       }
        //   };
        //   axisManager.UpdateAxes(resolver);
        //   @endcode
        // Notes :
        //   - Doit être noexcept et rapide : appelé pour chaque axe à chaque frame.
        //   - Peut capturer des contextes externes via lambda si nécessaire.
        // -------------------------------------------------------------------------
        using NkAxisResolver = NkFunction<float(NkInputDevice, uint32)>;

        // -------------------------------------------------------------------------
        // Classe : NkAxisManager
        // -------------------------------------------------------------------------
        // Description :
        //   Gestionnaire d'axes nommés permettant le mapping dynamique 
        //   entre inputs analogiques physiques et axes logiques du jeu.
        // Responsabilités :
        //   - Enregistrement des axes avec leurs handlers (CreateAxis).
        //   - Association d'inputs physiques aux axes avec transformations (AddCommand).
        //   - Évaluation périodique des valeurs via resolver (UpdateAxes).
        //   - Application de scale et deadzone (via minInterval) avant dispatch.
        // Structures internes :
        //   - mAxes     : Map nom → handler (NkAxisSubscriber) pour dispatch.
        //   - mCommands : Map nom → liste de NkAxisCommand pour évaluation.
        // Méthodes publiques :
        //   - CreateAxis()  : Enregistre un handler pour un axe nommé.
        //   - AddCommand()  : Associe un input physique à un axe avec paramètres.
        //   - RemoveAxis()  : Supprime un axe et ses commandes associées.
        //   - RemoveCommand(): Dissocie un input physique d'un axe.
        //   - UpdateAxes()  : Évalue tous les axes via resolver et déclenche handlers.
        //   - GetAxisCount()/GetCommandCount() : Métriques pour debugging.
        // Cycle d'évaluation :
        //   1. UpdateAxes(resolver) appelé chaque frame (typiquement dans Update()).
        //   2. Pour chaque commande : valeur = resolver(device, code) * scale.
        //   3. Si |valeur| >= minInterval : FireAxis() appelle le handler.
        //   4. Handler reçoit (nom, code, valeur) pour traitement applicatif.
        // Thread-safety :
        //   - Non thread-safe par défaut. Synchronisation externe requise.
        //   - UpdateAxes() typiquement appelé depuis le thread principal.
        // Utilisation typique :
        //   @code
        //   NkAxisManager axisMgr;
        //
        //   // Enregistrement de l'axe "MoveHorizontal"
        //   axisMgr.CreateAxis("MoveHorizontal", [](
        //       const NkString& name,
        //       const NkInputCode& code,
        //       float value
        //   ) {
        //       Player::SetHorizontalInput(value);
        //   });
        //
        //   // Association du stick gauche X de la manette 0
        //   axisMgr.AddCommand(NkAxisCommand(
        //       "MoveHorizontal",
        //       NkInputCode::GamepadAxis(NkGamepadAxis::NK_GP_AXIS_LX),
        //       1.2f,   // scale: +20% de sensibilité
        //       0.1f    // minInterval: deadzone de 10%
        //   ));
        //
        //   // Dans la boucle de jeu :
        //   axisMgr.UpdateAxes([](NkInputDevice dev, uint32 code) -> float {
        //       if (dev == NkInputDevice::NK_GAMEPAD_AXIS) {
        //           return NkInput.GamepadAxis(0, static_cast<NkGamepadAxis>(code));
        //       }
        //       return 0.f;
        //   });
        //   @endcode
        // -------------------------------------------------------------------------
        class NkAxisManager {
            public:
                void CreateAxis(
                    const NkString& name,
                    NkAxisSubscriber handler
                );

                void AddCommand(const NkAxisCommand& cmd);

                void RemoveAxis(const NkString& name);

                void RemoveCommand(const NkAxisCommand& cmd);

                void UpdateAxes(const NkAxisResolver& resolver);

                uint64 GetAxisCount() const noexcept {
                    return mAxes.Size();
                }

                uint64 GetCommandCount() const noexcept;

                uint64 GetCommandCount(const NkString& name) const noexcept;

            private:
                void FireAxis(
                    const NkString& name,
                    const NkAxisCommand& cmd,
                    float value
                );

                NkUnorderedMap<NkString, NkAxisSubscriber> mAxes;
                NkUnorderedMap<NkString, NkVector<NkAxisCommand>> mCommands;
        };

        // =========================================================================
        // Section : Macros de commodité pour les subscribers d'actions/axes
        // =========================================================================

        // -------------------------------------------------------------------------
        // Macro : NK_ACTION_SUBSCRIBER
        // -------------------------------------------------------------------------
        // Description :
        //   Macro simplifiant l'enregistrement d'un handler member function
        //   pour une action nommée via NkActionManager::CreateAction.
        // Paramètre :
        //   - method_ : Nom de la member function à appeler.
        // Signature attendue de method_ :
        //   void method_(
        //       const NkString& actionName,
        //       const NkInputCode& code,
        //       bool isPressed,
        //       bool isRepeat
        //   )
        // Expansion :
        //   Crée un lambda capturant 'this' qui délègue à la méthode spécifiée.
        // Utilisation :
        //   @code
        //   class PlayerController {
        //   public:
        //       void Initialize(NkActionManager& mgr) {
        //           mgr.CreateAction("Jump", NK_ACTION_SUBSCRIBER(OnJumpAction));
        //       }
        //
        //   private:
        //       void OnJumpAction(
        //           const NkString& name,
        //           const NkInputCode& code,
        //           bool isPressed,
        //           bool isRepeat
        //       ) {
        //           if (isPressed && !isRepeat) {
        //               Jump();
        //           }
        //       }
        //   };
        //   @endcode
        // -------------------------------------------------------------------------
        #define NK_ACTION_SUBSCRIBER(method_) \
            [this]( \
                const NkString& n_, \
                const NkInputCode& c_, \
                bool p_, \
                bool r_ \
            ) { \
                this->method_(n_, c_, p_, r_); \
            }

        // -------------------------------------------------------------------------
        // Macro : NK_AXIS_SUBSCRIBER
        // -------------------------------------------------------------------------
        // Description :
        //   Macro simplifiant l'enregistrement d'un handler member function
        //   pour un axe nommé via NkAxisManager::CreateAxis.
        // Paramètre :
        //   - method_ : Nom de la member function à appeler.
        // Signature attendue de method_ :
        //   void method_(
        //       const NkString& axisName,
        //       const NkInputCode& code,
        //       float value
        //   )
        // Expansion :
        //   Crée un lambda capturant 'this' qui délègue à la méthode spécifiée.
        // Utilisation :
        //   @code
        //   class CameraController {
        //   public:
        //       void Initialize(NkAxisManager& mgr) {
        //           mgr.CreateAxis("LookY", NK_AXIS_SUBSCRIBER(OnLookYAxis));
        //       }
        //
        //   private:
        //       void OnLookYAxis(
        //           const NkString& name,
        //           const NkInputCode& code,
        //           float value
        //       ) {
        //           RotatePitch(-value * 0.002f);  // value ∈ [-1, +1]
        //       }
        //   };
        //   @endcode
        // -------------------------------------------------------------------------
        #define NK_AXIS_SUBSCRIBER(method_) \
            [this]( \
                const NkString& n_, \
                const NkInputCode& c_, \
                float v_ \
            ) { \
                this->method_(n_, c_, v_); \
            }

    } // namespace nkentseu

#endif // NKENTSEU_EVENT_NKEVENTDISPATCHER_H

// =============================================================================
// Section : Exemples d'utilisation (à titre informatif - en commentaire)
// =============================================================================

/*
// -----------------------------------------------------------------------------
// Exemple 1 : Routage d'événements avec NkEventDispatcher
// -----------------------------------------------------------------------------
class GameInputHandler {
public:
    void OnEvent(NkEvent* ev) {
        NkEventDispatcher dispatcher(ev);

        // Routage des événements clavier
        NK_DISPATCH(dispatcher, NkKeyPressEvent, OnKeyPress);
        NK_DISPATCH(dispatcher, NkKeyReleaseEvent, OnKeyRelease);

        // Routage des événements souris
        NK_DISPATCH(dispatcher, NkMouseButtonEvent, OnMouseButton);
        NK_DISPATCH(dispatcher, NkMouseMoveEvent, OnMouseMove);

        // Routage des événements manette
        NK_DISPATCH(dispatcher, NkGamepadButtonPressEvent, OnGamepadButtonPress);
        NK_DISPATCH(dispatcher, NkGamepadAxisEvent, OnGamepadAxis);
    }

private:
    bool OnKeyPress(NkKeyPressEvent& e) {
        if (e.GetKey() == NkKey::NK_ESCAPE) {
            // UIManager::TogglePause();
            return true;  // Consommé : ne pas propager
        }
        return false;  // Non consommé : propagation continue
    }

    bool OnKeyRelease(NkKeyReleaseEvent& e) {
        // Traitement au relâchement si nécessaire
        return false;
    }

    bool OnMouseButton(NkMouseButtonEvent& e) {
        if (e.GetButton() == NkMouseButton::NK_MB_LEFT
            && e.GetState() == NkButtonState::NK_PRESSED) {
            // StartDrag(e.GetX(), e.GetY());
        }
        return false;
    }

    bool OnMouseMove(NkMouseMoveEvent& e) {
        // if (NkInput.IsLeftDown()) {
        //     UpdateDrag(e.GetX(), e.GetY());
        // }
        return false;
    }

    bool OnGamepadButtonPress(NkGamepadButtonPressEvent& e) {
        const auto playerIdx = e.GetGamepadIndex();
        const auto button = e.GetButton();

        if (button == NkGamepadButton::NK_GP_SOUTH) {
            // Player::Jump(playerIdx);
        }
        return false;
    }

    bool OnGamepadAxis(NkGamepadAxisEvent& e) {
        const auto playerIdx = e.GetGamepadIndex();
        const auto axis = e.GetAxis();
        const auto value = e.GetValue();

        if (axis == NkGamepadAxis::NK_GP_AXIS_LX) {
            // Player::SetHorizontalInput(playerIdx, value);
        } else if (axis == NkGamepadAxis::NK_GP_AXIS_LY) {
            // Player::SetVerticalInput(playerIdx, value);
        }
        return false;
    }
};

// -----------------------------------------------------------------------------
// Exemple 2 : Polling direct avec NkInput (instance globale)
// -----------------------------------------------------------------------------
void GameLoop::Update() {
    // --- Contrôles clavier ---
    // Déplacement avec touches directionnelles
    float moveX = 0.f;
    float moveY = 0.f;

    if (NkInput.IsKeyDown(NkKey::NK_A) || NkInput.IsKeyDown(NkKey::NK_LEFT)) {
        moveX = -1.f;
    }
    if (NkInput.IsKeyDown(NkKey::NK_D) || NkInput.IsKeyDown(NkKey::NK_RIGHT)) {
        moveX = +1.f;
    }
    if (NkInput.IsKeyDown(NkKey::NK_W) || NkInput.IsKeyDown(NkKey::NK_UP)) {
        moveY = +1.f;
    }
    if (NkInput.IsKeyDown(NkKey::NK_S) || NkInput.IsKeyDown(NkKey::NK_DOWN)) {
        moveY = -1.f;
    }

    // Normalisation diagonale
    if (moveX != 0.f || moveY != 0.f) {
        const float len = std::sqrt(moveX * moveX + moveY * moveY);
        if (len > 0.f) {
            moveX /= len;
            moveY /= len;
        }
        // Player::Move(moveX, moveY);
    }

    // Sprint avec Shift
    if (NkInput.IsShiftDown()) {
        // Player::EnableSprint(true);
    }

    // --- Contrôles souris (FPS) ---
    // Rotation de caméra avec mouvement brut (sans accélération OS)
    if (NkInput.IsLeftDown()) {
        constexpr float SENSITIVITY = 0.002f;
        float yaw = -static_cast<float>(NkInput.MouseRawDeltaX()) * SENSITIVITY;
        float pitch = -static_cast<float>(NkInput.MouseRawDeltaY()) * SENSITIVITY;
        // Camera::Rotate(yaw, pitch);
    }

    // Zoom avec molette
    const int wheelDelta = NkInput.MouseDeltaY();  // Ou logique wheel dédiée
    if (wheelDelta != 0) {
        // Camera::Zoom(wheelDelta > 0 ? 1.f : -1.f);
    }

    // --- Contrôles manette (multi-joueurs) ---
    for (uint32 playerIdx = 0; playerIdx < 4; ++playerIdx) {
        if (!NkInput.IsGamepadConnected(playerIdx)) {
            continue;
        }

        // Mouvement avec stick gauche
        float lx = NkInput.GamepadAxis(playerIdx, NkGamepadAxis::NK_GP_AXIS_LX);
        float ly = NkInput.GamepadAxis(playerIdx, NkGamepadAxis::NK_GP_AXIS_LY);
        // Player::Move(playerIdx, lx, ly);

        // Action avec bouton SOUTH
        if (NkInput.IsGamepadDown(playerIdx, NkGamepadButton::NK_GP_SOUTH)) {
            // Player::Jump(playerIdx);
        }

        // Vibration haptique en retour d'impact
        // if (Player::IsTakingDamage(playerIdx)) {
        //     NkInput.GamepadRumble(
        //         playerIdx,
        //         0.0f,   // motorLow
        //         0.9f,   // motorHigh
        //         0.0f,   // triggerLeft
        //         0.0f,   // triggerRight
        //         100     // durationMs
        //     );
        // }
    }
}

// -----------------------------------------------------------------------------
// Exemple 3 : Configuration d'actions nommées avec NkActionManager
// -----------------------------------------------------------------------------
void SetupActionBindings(NkActionManager& actionMgr) {
    // Action "Jump" : déclenchée par Espace ou bouton A de manette
    actionMgr.CreateAction("Jump", NK_ACTION_SUBSCRIBER(OnJumpAction));

    actionMgr.AddCommand(NkActionCommand(
        "Jump",
        NkInputCode::Key(NkKey::NK_SPACE)  // Clavier : Espace
    ));

    actionMgr.AddCommand(NkActionCommand(
        "Jump",
        NkInputCode::Gamepad(NkGamepadButton::NK_GP_SOUTH)  // Manette : A/Cross
    ));

    // Action "Fire" : déclenchée par clic gauche ou bouton X de manette
    actionMgr.CreateAction("Fire", NK_ACTION_SUBSCRIBER(OnFireAction));

    actionMgr.AddCommand(NkActionCommand(
        "Fire",
        NkInputCode::Mouse(NkMouseButton::NK_MB_LEFT)  // Souris : clic gauche
    ));

    actionMgr.AddCommand(NkActionCommand(
        "Fire",
        NkInputCode::Gamepad(NkGamepadButton::NK_GP_EAST)  // Manette : B/Circle
    ));

    // Action "Pause" : déclenchée par Échap ou bouton Start
    actionMgr.CreateAction("Pause", NK_ACTION_SUBSCRIBER(OnPauseAction));

    actionMgr.AddCommand(NkActionCommand(
        "Pause",
        NkInputCode::Key(NkKey::NK_ESCAPE)  // Clavier : Échap
    ));

    actionMgr.AddCommand(NkActionCommand(
        "Pause",
        NkInputCode::Gamepad(NkGamepadButton::NK_GP_START)  // Manette : Start
    ));
}

void GameInputHandler::OnJumpAction(
    const NkString& name,
    const NkInputCode& code,
    bool isPressed,
    bool isRepeat
) {
    // Saut uniquement au premier appui (pas en répétition)
    if (isPressed && !isRepeat) {
        // Player::Jump();
    }
}

void GameInputHandler::OnFireAction(
    const NkString& name,
    const NkInputCode& code,
    bool isPressed,
    bool isRepeat
) {
    // Tir en maintien : répétable si configuré
    if (isPressed) {
        // Player::Fire();
    }
}

void GameInputHandler::OnPauseAction(
    const NkString& name,
    const NkInputCode& code,
    bool isPressed,
    bool isRepeat
) {
    // Pause uniquement au premier appui
    if (isPressed && !isRepeat) {
        // UIManager::TogglePause();
    }
}

// -----------------------------------------------------------------------------
// Exemple 4 : Configuration d'axes nommés avec NkAxisManager
// -----------------------------------------------------------------------------
void SetupAxisBindings(NkAxisManager& axisMgr) {
    // Axe "MoveHorizontal" : stick gauche X ou touches A/D
    axisMgr.CreateAxis("MoveHorizontal", NK_AXIS_SUBSCRIBER(OnMoveHorizontal));

    // Manette : stick gauche X avec scale 1.2 et deadzone 10%
    axisMgr.AddCommand(NkAxisCommand(
        "MoveHorizontal",
        NkInputCode::GamepadAxis(NkGamepadAxis::NK_GP_AXIS_LX),
        1.2f,   // scale: +20% sensibilité
        0.1f    // minInterval: deadzone 10%
    ));

    // Clavier : touche D = +1, A = -1 (binaire)
    // Note: nécessite un resolver spécial pour inputs binaires

    // Axe "LookY" : souris Y ou stick droit Y de manette
    axisMgr.CreateAxis("LookY", NK_AXIS_SUBSCRIBER(OnLookY));

    // Souris : mouvement vertical brut
    // (géré via resolver personnalisé car NkInput.MouseRawDeltaY() n'est pas un axe)

    // Manette : stick droit Y avec inversion et deadzone
    axisMgr.AddCommand(NkAxisCommand(
        "LookY",
        NkInputCode::GamepadAxis(NkGamepadAxis::NK_GP_AXIS_RY),
        -1.0f,  // scale: inversion pour contrôle "flight stick"
        0.15f   // minInterval: deadzone 15%
    ));
}

void GameInputHandler::OnMoveHorizontal(
    const NkString& name,
    const NkInputCode& code,
    float value
) {
    // value ∈ [-1, +1] après application du scale et deadzone
    // Player::SetHorizontalInput(value);
}

void GameInputHandler::OnLookY(
    const NkString& name,
    const NkInputCode& code,
    float value
) {
    // value ∈ [-1, +1], déjà inversé si scale = -1.0
    constexpr float MOUSE_SENSITIVITY = 0.002f;
    // Camera::RotatePitch(value * MOUSE_SENSITIVITY);
}

// -----------------------------------------------------------------------------
// Exemple 5 : Resolver personnalisé pour NkAxisManager::UpdateAxes
// -----------------------------------------------------------------------------
NkAxisResolver BuildDefaultAxisResolver() {
    return [](NkInputDevice device, uint32 code) -> float {
        switch (device) {
            case NkInputDevice::NK_KEYBOARD: {
                // Inputs clavier binaires : 0 ou 1
                const NkKey key = static_cast<NkKey>(code);
                return NkInput.IsKeyDown(key) ? 1.f : 0.f;
            }

            case NkInputDevice::NK_MOUSE: {
                // Boutons souris binaires : 0 ou 1
                const NkMouseButton btn = static_cast<NkMouseButton>(code);
                return NkInput.IsMouseDown(btn) ? 1.f : 0.f;
            }

            case NkInputDevice::NK_GAMEPAD_AXIS: {
                // Axes de manette : valeurs analogiques [-1, +1] ou [0, +1]
                const NkGamepadAxis axis = static_cast<NkGamepadAxis>(code);
                return NkInput.GamepadAxis(0, axis);  // Slot joueur 0
            }

            case NkInputDevice::NK_MOUSEWHEEL: {
                // Molette : code 0 = vertical, 1 = horizontal
                // Retourne le delta accumulé depuis le dernier appel
                // (nécessite un état interne ou délégation à NkInput)
                return 0.f;  // Placeholder
            }

            default:
                return 0.f;
        }
    };
}

// Utilisation dans la boucle de jeu :
// void GameLoop::Update() {
//     static NkAxisResolver resolver = BuildDefaultAxisResolver();
//     axisManager.UpdateAxes(resolver);
// }

// -----------------------------------------------------------------------------
// Exemple 6 : Rebinding dynamique des contrôles à l'exécution
// -----------------------------------------------------------------------------
void RebindJumpToNewKey(NkActionManager& actionMgr, NkKey newKey) {
    // Suppression de l'ancienne association (si existante)
    actionMgr.RemoveCommand(NkActionCommand(
        "Jump",
        NkInputCode::Key(NkKey::NK_SPACE)  // Ancienne touche
    ));

    // Ajout de la nouvelle association
    actionMgr.AddCommand(NkActionCommand(
        "Jump",
        NkInputCode::Key(newKey)  // Nouvelle touche configurée par l'utilisateur
    ));

    // Sauvegarde dans la configuration utilisateur
    // Config::SetBinding("Jump", "keyboard", static_cast<uint32>(newKey));
}

void RebindJumpToGamepadButton(NkActionManager& actionMgr, NkGamepadButton newBtn) {
    // Suppression de l'ancienne association manette
    actionMgr.RemoveCommand(NkActionCommand(
        "Jump",
        NkInputCode::Gamepad(NkGamepadButton::NK_GP_SOUTH)
    ));

    // Ajout de la nouvelle association
    actionMgr.AddCommand(NkActionCommand(
        "Jump",
        NkInputCode::Gamepad(newBtn)
    ));

    // Sauvegarde dans la configuration
    // Config::SetBinding("Jump", "gamepad", static_cast<uint32>(newBtn));
}

// -----------------------------------------------------------------------------
// Exemple 7 : Test unitaire de NkInputQuery (mock des systèmes)
// -----------------------------------------------------------------------------
void TestNkInputQuery() {
    // Setup : injection de mocks pour NkEventSystem et NkGamepadSystem
    // MockEventSystem mockEvents;
    // MockGamepadSystem mockGamepads;
    // NkInput.SetEventSystem(&mockEvents);
    // NkInput.SetGamepadSystem(&mockGamepads);

    // Test clavier : touche non pressée par défaut
    NK_ASSERT(NkInput.IsKeyDown(NkKey::NK_SPACE) == false);

    // Simulation : mockEvents.keyboard.OnKeyPress(NkKey::NK_SPACE, ...);
    // NK_ASSERT(NkInput.IsKeyDown(NkKey::NK_SPACE) == true);

    // Test souris : position par défaut (0,0)
    NK_ASSERT(NkInput.MouseX() == 0);
    NK_ASSERT(NkInput.MouseY() == 0);

    // Simulation : mockEvents.mouse.OnMove(100, 200, ...);
    // NK_ASSERT(NkInput.MouseX() == 100);
    // NK_ASSERT(NkInput.MouseY() == 200);

    // Test manette : non connectée par défaut
    NK_ASSERT(NkInput.IsGamepadConnected(0) == false);

    // Nettoyage
    NkInput.SetEventSystem(nullptr);
    NkInput.SetGamepadSystem(nullptr);

    NK_LOG_INFO("Tests de NkInputQuery passés avec succès");
}

// -----------------------------------------------------------------------------
// Exemple 8 : Intégration complète dans un contrôleur de joueur
// -----------------------------------------------------------------------------
class PlayerController {
public:
    void Initialize(
        NkEventSystem* eventSys,
        NkGamepadSystem* gamepadSys,
        NkActionManager* actionMgr,
        NkAxisManager* axisMgr
    ) {
        // Injection dans NkInput (fait normalement par NkSystem::Initialize)
        NkInput.SetEventSystem(eventSys);
        NkInput.SetGamepadSystem(gamepadSys);

        // Enregistrement des actions
        if (actionMgr) {
            actionMgr->CreateAction("Jump", NK_ACTION_SUBSCRIBER(OnJump));
            actionMgr->CreateAction("Fire", NK_ACTION_SUBSCRIBER(OnFire));

            actionMgr->AddCommand(NkActionCommand(
                "Jump",
                NkInputCode::Key(NkKey::NK_SPACE)
            ));
            actionMgr->AddCommand(NkActionCommand(
                "Jump",
                NkInputCode::Gamepad(NkGamepadButton::NK_GP_SOUTH)
            ));
        }

        // Enregistrement des axes
        if (axisMgr) {
            axisMgr->CreateAxis("Move", NK_AXIS_SUBSCRIBER(OnMove));
            axisMgr->AddCommand(NkAxisCommand(
                "Move",
                NkInputCode::GamepadAxis(NkGamepadAxis::NK_GP_AXIS_LX),
                1.0f,
                0.1f
            ));
        }
    }

    void Update() {
        // Polling direct pour les inputs critiques (réactivité maximale)
        if (NkInput.IsKeyDown(NkKey::NK_P)) {
            // DebugToggle();
        }

        // Évaluation des axes nommés (si configurés)
        // if (mAxisManager) {
        //     mAxisManager->UpdateAxes(BuildDefaultAxisResolver());
        // }
    }

private:
    void OnJump(
        const NkString& name,
        const NkInputCode& code,
        bool isPressed,
        bool isRepeat
    ) {
        if (isPressed && !isRepeat) {
            // Jump();
        }
    }

    void OnFire(
        const NkString& name,
        const NkInputCode& code,
        bool isPressed,
        bool isRepeat
    ) {
        if (isPressed) {
            // Fire();
        }
    }

    void OnMove(
        const NkString& name,
        const NkInputCode& code,
        float value
    ) {
        // SetHorizontalInput(value);
    }

    NkAxisResolver BuildDefaultAxisResolver() {
        return [](NkInputDevice dev, uint32 code) -> float {
            if (dev == NkInputDevice::NK_GAMEPAD_AXIS) {
                return NkInput.GamepadAxis(0, static_cast<NkGamepadAxis>(code));
            }
            return 0.f;
        };
    }

    // NkActionManager* mActionManager = nullptr;
    // NkAxisManager* mAxisManager = nullptr;
};
*/