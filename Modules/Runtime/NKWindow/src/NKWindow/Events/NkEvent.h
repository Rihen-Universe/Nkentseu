#pragma once

// =============================================================================
// NkEvent.h
// Classe de base pour tous les événements du système Nkentseu.
//
// Architecture :
//   NkEventCategory  : flags de catégories (bitfield)
//   NkEventType      : énumération complète de tous les types d'événements
//   NkEvent          : classe de base polymorphe
//
// Exemple d'utilisation :
//   if (event.Is<NkKeyPressEvent>())            { ... }
//   if (auto* kp = event.As<NkKeyPressEvent>()) { ... }
//   event.MarkHandled();
//   logger.Info(event.ToString());
// =============================================================================

#include <cstdint>
#include <string>
#include <chrono>

#include "NKMath/NkTypes.h"
#include "NKCore/NkTraits.h"
#include "NKContainers/NkContainers.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkFormat.h"
#include "NKContainers/String/NkFormatf.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKContainers/Functional/NkFunction.h"

// ---------------------------------------------------------------------------
// Macro BIT — décalage de bits
// ---------------------------------------------------------------------------
#ifndef BIT
    #define BIT(n) (1u << (n))
#endif

namespace nkentseu {

    // =========================================================================
    // NkEventCategory
    // Représente les catégories d'événements sous forme de flags de bits.
    // Plusieurs catégories peuvent être combinées via les opérateurs | et &.
    // =========================================================================

    struct NkEventCategory {

        /// @brief Valeurs de catégorie (flags de bits)
        enum Value : uint32 {
            NK_CAT_NONE        = 0,
            NK_CAT_APPLICATION = BIT(1),   ///< Événements liés à l'application
            NK_CAT_INPUT       = BIT(2),   ///< Entrées utilisateur génériques
            NK_CAT_KEYBOARD    = BIT(3),   ///< Entrées clavier
            NK_CAT_MOUSE       = BIT(4),   ///< Entrées souris
            NK_CAT_WINDOW      = BIT(5),   ///< Événements de fenêtre
            NK_CAT_GRAPHICS    = BIT(6),   ///< Événements graphiques / rendu
            NK_CAT_TOUCH       = BIT(7),   ///< Événements tactiles / gestures
            NK_CAT_GAMEPAD     = BIT(8),   ///< Manettes de jeu
            NK_CAT_CUSTOM      = BIT(9),   ///< Événements personnalisés
            NK_CAT_TRANSFER    = BIT(10),  ///< Transfert de données / fichiers
            NK_CAT_GENERIC_HID = BIT(11),  ///< Périphériques HID génériques
            NK_CAT_DROP        = BIT(12),  ///< Drag & Drop
            NK_CAT_SYSTEM      = BIT(13),  ///< Événements système (énergie, locale…)
            NK_CAT_ALL         = 0xFFFFFFFFu ///< Toutes les catégories
        };

        /// @brief Convertit une valeur de catégorie en chaîne lisible
        static NkString ToString(NkEventCategory::Value value);

        /// @brief Convertit une chaîne en valeur de catégorie
        /// @return NK_CAT_NONE si la chaîne est inconnue
        static NkEventCategory::Value FromString(const NkString& str);
    };

    // -------------------------------------------------------------------------
    // Opérateurs bitwise pour NkEventCategory::Value
    // Définis librement dans le namespace pour éviter les conflits de portée.
    // -------------------------------------------------------------------------

    inline NkEventCategory::Value operator|(NkEventCategory::Value a, NkEventCategory::Value b) {
        return static_cast<NkEventCategory::Value>(static_cast<uint32>(a) | static_cast<uint32>(b));
    }
    inline NkEventCategory::Value operator&(NkEventCategory::Value a, NkEventCategory::Value b) {
        return static_cast<NkEventCategory::Value>(static_cast<uint32>(a) & static_cast<uint32>(b));
    }
    inline NkEventCategory::Value operator~(NkEventCategory::Value a) {
        return static_cast<NkEventCategory::Value>(~static_cast<uint32>(a));
    }
    inline NkEventCategory::Value& operator|=(NkEventCategory::Value& a, NkEventCategory::Value b) {
        return a = a | b;
    }
    inline NkEventCategory::Value& operator&=(NkEventCategory::Value& a, NkEventCategory::Value b) {
        return a = a & b;
    }

    // -------------------------------------------------------------------------
    // Utilitaires de catégorie
    // -------------------------------------------------------------------------

    /// @brief Retourne true si le flag est présent dans le set
    inline bool NkCategoryHas(NkEventCategory::Value set, NkEventCategory::Value flag) {
        return (static_cast<uint32>(set) & static_cast<uint32>(flag)) != 0;
    }
    /// @brief Retourne true si aucune catégorie n'est définie
    inline bool NkCategoryEmpty(NkEventCategory::Value set) {
        return set == NkEventCategory::NK_CAT_NONE;
    }
    /// @brief Retourne true si toutes les catégories sont définies
    inline bool NkCategoryFull(NkEventCategory::Value set) {
        return set == NkEventCategory::NK_CAT_ALL;
    }

    // =========================================================================
    // NkEventType
    // Énumération complète et unique de tous les types d'événements.
    // =========================================================================

    struct NkEventType {

        enum Value : uint32 {
            NK_NONE = 0,

            // --- Application ---
            NK_APP_LAUNCH,          ///< Lancement de l'application
            NK_APP_TICK,            ///< Tick logique (boucle principale)
            NK_APP_UPDATE,          ///< Mise à jour de la logique
            NK_APP_RENDER,          ///< Demande de rendu
            NK_APP_CLOSE,           ///< Fermeture demandée

            // --- Fenêtre ---
            NK_WINDOW_CREATE,       ///< Fenêtre créée
            NK_WINDOW_CLOSE,        ///< Fenêtre en cours de fermeture (requête)
            NK_WINDOW_DESTROY,      ///< Fenêtre détruite
            NK_WINDOW_PAINT,        ///< Demande de repaint
            NK_WINDOW_RESIZE,       ///< Redimensionnement en cours
            NK_WINDOW_RESIZE_BEGIN, ///< Début du redimensionnement
            NK_WINDOW_RESIZE_END,   ///< Fin du redimensionnement
            NK_WINDOW_MOVE,         ///< Déplacement en cours
            NK_WINDOW_MOVE_BEGIN,   ///< Début du déplacement
            NK_WINDOW_MOVE_END,     ///< Fin du déplacement
            NK_WINDOW_FOCUS_GAINED, ///< Focus obtenu
            NK_WINDOW_FOCUS_LOST,   ///< Focus perdu
            NK_WINDOW_MINIMIZE,     ///< Fenêtre minimisée
            NK_WINDOW_MAXIMIZE,     ///< Fenêtre maximisée
            NK_WINDOW_RESTORE,      ///< Fenêtre restaurée
            NK_WINDOW_FULLSCREEN,   ///< Passage en plein écran
            NK_WINDOW_WINDOWED,     ///< Retour en mode fenêtré
            NK_WINDOW_DPI_CHANGE,   ///< Changement de DPI (multi-écran)
            NK_WINDOW_THEME_CHANGE, ///< Changement de thème système
            NK_WINDOW_SHOWN,        ///< Fenêtre affichée
            NK_WINDOW_HIDDEN,       ///< Fenêtre masquée

            // --- Clavier ---
            NK_KEY_PRESSED,         ///< Touche pressée (première fois)
            NK_KEY_REPEATED,        ///< Touche maintenue (auto-repeat OS)
            NK_KEY_RELEASED,        ///< Touche relâchée
            NK_TEXT_INPUT,          ///< Entrée texte Unicode (après IME)
            NK_CHAR_ENTERED,        ///< Caractère saisi

            // --- Souris ---
            NK_MOUSE_MOVE,                  ///< Déplacement du curseur
            NK_MOUSE_RAW,                   ///< Mouvement brut (sans accélération)
            NK_MOUSE_BUTTON_PRESSED,        ///< Bouton pressé
            NK_MOUSE_BUTTON_RELEASED,       ///< Bouton relâché
            NK_MOUSE_DOUBLE_CLICK,          ///< Double-clic
            NK_MOUSE_WHEEL_VERTICAL,        ///< Molette verticale
            NK_MOUSE_WHEEL_HORIZONTAL,      ///< Molette horizontale
            NK_MOUSE_ENTER,                 ///< Curseur entre dans la zone client
            NK_MOUSE_LEAVE,                 ///< Curseur quitte la zone client
            NK_MOUSE_WINDOW,                ///< Souris dans la fenêtre
            NK_MOUSE_WINDOW_ENTER,          ///< Curseur entre dans la fenêtre
            NK_MOUSE_WINDOW_LEAVE,          ///< Curseur quitte la fenêtre
            NK_MOUSE_CAPTURE_BEGIN,         ///< Début de capture souris
            NK_MOUSE_CAPTURE_END,           ///< Fin de capture souris

            // --- Manette ---
            NK_GAMEPAD_CONNECT,             ///< Manette connectée
            NK_GAMEPAD_DISCONNECT,          ///< Manette déconnectée
            NK_GAMEPAD_BUTTON_PRESSED,      ///< Bouton manette pressé
            NK_GAMEPAD_BUTTON_RELEASED,     ///< Bouton manette relâché
            NK_GAMEPAD_AXIS_MOTION,         ///< Axe analogique déplacé
            NK_GAMEPAD_RUMBLE,              ///< Vibration manette
            NK_GAMEPAD_STICK,               ///< Stick analogique
            NK_GAMEPAD_TRIGGERED,           ///< Gâchette manette (LT/RT)

            // --- HID générique ---
            NK_GENERIC_CONNECT,             ///< Périphérique HID connecté
            NK_GENERIC_DISCONNECT,          ///< Périphérique HID déconnecté
            NK_GENERIC_BUTTON_PRESSED,      ///< Bouton HID pressé
            NK_GENERIC_BUTTON_RELEASED,     ///< Bouton HID relâché
            NK_GENERIC_AXIS_MOTION,         ///< Axe HID déplacé
            NK_GENERIC_RUMBLE,              ///< Vibration HID
            NK_GENERIC_STICK,               ///< Stick HID
            NK_GENERIC_TRIGGERED,           ///< Gâchette HID

            // --- Tactile / Gestures ---
            NK_TOUCH_BEGIN,                 ///< Contact tactile début
            NK_TOUCH_MOVE,                  ///< Contact tactile déplacé
            NK_TOUCH_END,                   ///< Contact tactile terminé
            NK_TOUCH_CANCEL,                ///< Contact tactile annulé
            NK_TOUCH_PRESSED,               ///< Appui tactile
            NK_TOUCH_RELEASED,              ///< Relâchement tactile
            NK_GESTURE_PINCH,               ///< Geste pincement (zoom)
            NK_GESTURE_ROTATE,              ///< Geste rotation
            NK_GESTURE_PAN,                 ///< Geste panoramique
            NK_GESTURE_SWIPE,               ///< Geste balayage
            NK_GESTURE_TAP,                 ///< Geste tap
            NK_GESTURE_LONG_PRESS,          ///< Geste appui long

            // --- Drag & Drop ---
            NK_DROP_FILE,                   ///< Fichier déposé
            NK_DROP_TEXT,                   ///< Texte déposé
            NK_DROP_IMAGE,                  ///< Image déposée
            NK_DROP_ENTER,                  ///< Entrée dans zone de dépôt
            NK_DROP_OVER,                   ///< Survol zone de dépôt
            NK_DROP_LEAVE,                  ///< Sortie zone de dépôt

            // --- Système ---
            NK_SYSTEM_POWER,                ///< Alimentation (veille, réveil…)
            NK_SYSTEM_LOCALE,               ///< Changement de locale
            NK_SYSTEM_DISPLAY,              ///< Changement d'affichage
            NK_SYSTEM_MEMORY,               ///< Pression mémoire système

            // --- Transfert ---
            NK_TRANSFER,                    ///< Transfert de données ou fichier

            // --- Personnalisé ---
            NK_CUSTOM,                      ///< Événement défini par l'utilisateur

            /// Sentinelle — doit rester en dernier
            NK_EVENT_COUNT
        };

        /// @brief Convertit un type d'événement en chaîne lisible
        static NkString ToString(NkEventType::Value value);

        /// @brief Convertit une chaîne en type d'événement
        /// @return NK_NONE si la chaîne est inconnue
        static NkEventType::Value FromString(const NkString& str);
    };

    // =========================================================================
    // NkTimestampMs — timestamp en millisecondes
    // =========================================================================

    /// @brief Millisecondes depuis l'initialisation du système d'événements
    using NkTimestampMs = uint64;

    // =========================================================================
    // Macros helper pour les classes dérivées de NkEvent
    //
    // NK_EVENT_BIND_HANDLER(methode)  — crée un lambda vers une méthode membre
    // NK_EVENT_STATIC_TYPE(TYPE)      — expose GetStaticType() (pour Is<T> / As<T>)
    // NK_EVENT_TYPE_FLAGS(TYPE)       — implémente GetType(), GetTypeStr(), GetName()
    // NK_EVENT_CATEGORY_FLAGS(CAT)    — implémente GetCategoryFlags()
    //
    // Exemple dans une classe dérivée :
    //   NK_EVENT_TYPE_FLAGS(NK_KEY_PRESSED)
    //   NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_INPUT | NkEventCategory::NK_CAT_KEYBOARD)
    // =========================================================================

    /// @brief Crée un lambda vers une méthode membre (pour les handlers)
    #define NK_EVENT_BIND_HANDLER(method_) \
        [this](auto&& eventArg) -> decltype(auto) { return method_(traits::NkForward<decltype(eventArg)>(eventArg)); }

    /// @brief Expose GetStaticType() sur une classe dérivée
    #define NK_EVENT_STATIC_TYPE(type_) \
        static NkEventType::Value GetStaticType() { return NkEventType::type_; }

    /// @brief Implémente GetStaticType(), GetType(), GetTypeStr() et GetName()
    #define NK_EVENT_TYPE_FLAGS(type_) \
        NK_EVENT_STATIC_TYPE(type_) \
        NkEventType::Value GetType()    const override { return GetStaticType(); } \
        const char*        GetTypeStr() const override { return #type_; }          \
        const char*        GetName()    const override { return #type_; }

    /// @brief Implémente GetCategoryFlags()
    #define NK_EVENT_CATEGORY_FLAGS(category_) \
        uint32 GetCategoryFlags() const override { \
            return static_cast<uint32>(category_); \
        }

    // =========================================================================
    // NkEvent — classe de base polymorphe pour tous les événements
    //
    // Cycle de vie :
    //   1. Construction  : windowId, timestamp et état initialisés
    //   2. Distribution  : transmis aux handlers enregistrés
    //   3. Traitement    : un handler appelle MarkHandled() pour stopper
    //                      la propagation
    //
    // Contrat pour les classes dérivées :
    //   - Surcharger GetType() et GetCategoryFlags() (via les macros ci-dessus)
    //   - Surcharger ToString() pour le débogage
    //   - Implémenter Clone() si une copie polymorphe est nécessaire
    // =========================================================================

    class NkEvent {
        public:

            // --- Interface virtuelle à surcharger dans les classes dérivées ---

            /// @brief Retourne le type de l'événement
            virtual NkEventType::Value GetType() const;

            /// @brief Retourne le nom de la classe de l'événement
            virtual const char* GetName() const;

            /// @brief Retourne une string du type (ex: "NK_KEY_PRESSED")
            virtual const char* GetTypeStr() const;

            /// @brief Retourne les flags de catégorie (bitfield NkEventCategory::Value)
            virtual uint32 GetCategoryFlags() const;

            /// @brief Crée une copie polymorphe de l'événement (heap-allocated)
            /// @note  Le caller est responsable de la destruction (delete / unique_ptr)
            virtual NkEvent* Clone() const;

            // --- Constructeurs ---

            /// @brief Constructeur avec identifiant de fenêtre source
            explicit NkEvent(uint64 windowId);

            /// @brief Constructeur par défaut (windowId = 0)
            NkEvent();

            /// @brief Constructeur de copie
            NkEvent(const NkEvent& other);

            /// @brief Destructeur virtuel — indispensable pour le polymorphisme
            virtual ~NkEvent() = default;

            // --- Accesseurs ---

            /// @brief Retourne la catégorie de l'événement sous forme de valeur enum
            NkEventCategory::Value GetCategory() const {
                return static_cast<NkEventCategory::Value>(GetCategoryFlags());
            }

            /// @brief Retourne l'identifiant de la fenêtre source (0 si aucune)
            uint64 GetWindowId() const { return mWindowID; }

            /// @brief Définit l'identifiant de la fenêtre source
            void SetWindowId(uint64 id) { mWindowID = id; }

            /// @brief Retourne le timestamp de création (ms depuis init système)
            NkTimestampMs GetTimestamp() const { return mTimestamp; }

            // --- État de traitement ---

            /// @brief Retourne true si l'événement a déjà été traité
            bool IsHandled() const { return mHandled; }

            /// @brief Marque l'événement comme traité (stoppe la propagation)
            void MarkHandled() { mHandled = true; }

            /// @brief Réinitialise l'état "traité"
            void Unmark() { mHandled = false; }

            // --- Vérifications ---

            /// @brief Retourne true si l'événement a un type valide (≠ NK_NONE)
            bool IsValid() const { return GetType() != NkEventType::NK_NONE; }

            /// @brief Retourne true si l'événement est du type donné
            bool IsType(NkEventType::Value type) const { return GetType() == type; }

            /// @brief Retourne true si l'événement appartient à la catégorie donnée
            bool HasCategory(NkEventCategory::Value flag) const {
                return (GetCategoryFlags() & static_cast<uint32>(flag)) != 0;
            }

            // --- Casting polymorphe type-safe ---

            /// @brief Retourne true si l'événement est exactement du type T
            /// @tparam T  Classe dérivée exposant GetStaticType()
            template <typename T>
            bool Is() const { return GetType() == T::GetStaticType(); }

            /// @brief Caste vers T* si l'événement est du type T, sinon nullptr
            template <typename T>
            T* As() { return Is<T>() ? static_cast<T*>(this) : nullptr; }

            /// @brief Caste vers const T* si l'événement est du type T, sinon nullptr
            template <typename T>
            const T* As() const { return Is<T>() ? static_cast<const T*>(this) : nullptr; }

            // --- Sérialisation ---

            /// @brief Retourne une représentation lisible de l'événement (pour logs/debug)
            virtual NkString ToString() const;

            /// @brief Convertit un type en string (délègue à NkEventType::ToString)
            static NkString TypeToString(NkEventType::Value type);

            /// @brief Convertit une catégorie en string (délègue à NkEventCategory::ToString)
            static NkString CategoryToString(NkEventCategory::Value category);

        protected:
            uint64         mWindowID  = 0;     ///< Identifiant de la fenêtre source
            NkTimestampMs mTimestamp = 0;     ///< Timestamp de création (ms)
            bool          mHandled   = false; ///< Événement déjà traité ?

            /// @brief Retourne le timestamp courant en millisecondes
            static NkTimestampMs GetCurrentTimestamp();
    };

    // =========================================================================
    // Alias de callbacks pour les événements
    //
    //   EventObserver    : handler recevant un NkEvent& modifiable
    //   EventObserverRef : handler recevant un const NkEvent& (lecture seule)
    //   EventHandler     : alias sémantique — handler actif (modifie l'état)
    //   EventHandlerRef  : alias sémantique — handler passif (observation)
    // =========================================================================

    using EventObserver    = NkFunction<void, NkEvent&>;
    using EventObserverRef = NkFunction<void, const NkEvent&>;
    using EventHandler     = NkFunction<void, NkEvent&>;
    using EventHandlerRef  = NkFunction<void, const NkEvent&>;

    // =========================================================================
    // NkEventCallback — type de callback pour les événements
    // =========================================================================
    using NkEventCallback = NkFunction<void, NkEvent*>;

} // namespace nkentseu
