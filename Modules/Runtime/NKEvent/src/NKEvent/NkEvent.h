// =============================================================================
// NKEvent/NkEvent.h
// Classe de base pour tous les événements du système Nkentseu.
//
// Architecture :
//   - NkEventCategory  : flags de catégories (bitfield) pour filtrage efficace
//   - NkEventType      : énumération exhaustive et unique des types d'événements
//   - NkEvent          : classe de base polymorphe avec RTTI léger et casting type-safe
//
// Design :
//   - Pattern CRTP léger via macros pour éviter la duplication de code boilerplate
//   - Casting polymorphe type-safe sans RTTI lourd (Is<T>(), As<T>())
//   - Support multi-fenêtre via windowId pour applications multi-vues
//   - Timestamp haute résolution pour profilage et replay d'événements
//   - Compatibilité avec NkFunction pour callbacks flexibles et performants
//
// Exemple d'utilisation :
//   if (event.Is<NkKeyPressEvent>()) { ... }
//   if (auto* kp = event.As<NkKeyPressEvent>()) { ... }
//   event.MarkHandled();
//   logger.Info(event.ToString());
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_EVENT_NKEVENT_H
#define NKENTSEU_EVENT_NKEVENT_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des dépendances standards et internes au projet.
    // Toutes les dépendances utilisent les conteneurs et utilitaires NK*.

    #include <cstdint>
    #include <string>
    #include <chrono>

    #include "NKEvent/NkEventApi.h"           // Macros d'export NKENTSEU_EVENT_API
    #include "NKMath/NKMath.h"                 // Types mathématiques de base (uint64, etc.)
    #include "NKCore/NkTraits.h"               // Utilitaires de traits (NkForward, etc.)
    #include "NKContainers/NkContainers.h"     // Conteneurs génériques NK*
    #include "NKContainers/String/NkString.h"  // Chaîne de caractères Unicode NK*
    #include "NKContainers/String/NkFormat.h"  // Formatage de chaînes type-safe
    #include "NKContainers/Associative/NkUnorderedMap.h"  // Hash map optimisée
    #include "NKContainers/Functional/NkFunction.h"       // Callbacks type-erased

    // -------------------------------------------------------------------------
    // SECTION 2 : MACROS GLOBALES ET UTILITAIRES
    // -------------------------------------------------------------------------
    // Macro BIT pour la manipulation de flags de bits.
    // Définie uniquement si non présente pour éviter les conflits.

    #ifndef BIT
        #define BIT(n) (1u << (n))
    #endif

    namespace nkentseu {

        // =====================================================================
        // NkEventCategory
        // Représente les catégories d'événements sous forme de flags de bits.
        // Plusieurs catégories peuvent être combinées via les opérateurs | et &.
        //
        // Usage typique :
        //   NkEventCategory::Value filter = NK_CAT_INPUT | NK_CAT_KEYBOARD;
        //   if (NkCategoryHas(event.GetCategoryFlags(), filter)) { ... }
        // =====================================================================

        struct NKENTSEU_EVENT_CLASS_EXPORT NkEventCategory {

            /// @brief Valeurs de catégorie (flags de bits)
            /// Chaque valeur est un bit unique pour permettre les combinaisons.
            enum Value : uint32 {
                NK_CAT_NONE        = 0,                   ///< Aucune catégorie (valeur par défaut)
                NK_CAT_APPLICATION = BIT(1),              ///< Événements liés au cycle de vie de l'application
                NK_CAT_INPUT       = BIT(2),              ///< Entrées utilisateur génériques (clavier, souris, tactile)
                NK_CAT_KEYBOARD    = BIT(3),              ///< Entrées clavier spécifiques
                NK_CAT_MOUSE       = BIT(4),              ///< Entrées souris spécifiques
                NK_CAT_WINDOW      = BIT(5),              ///< Événements de gestion de fenêtre
                NK_CAT_GRAPHICS    = BIT(6),              ///< Événements graphiques et de rendu
                NK_CAT_TOUCH       = BIT(7),              ///< Événements tactiles et gestures multi-touch
                NK_CAT_GAMEPAD     = BIT(8),              ///< Entrées manettes de jeu (gamepads)
                NK_CAT_CUSTOM      = BIT(9),              ///< Événements personnalisés définis par l'utilisateur
                NK_CAT_TRANSFER    = BIT(10),             ///< Transfert de données ou fichiers (D&D, clipboard)
                NK_CAT_GENERIC_HID = BIT(11),             ///< Périphériques HID génériques non catégorisés
                NK_CAT_DROP        = BIT(12),             ///< Opérations de Drag & Drop
                NK_CAT_SYSTEM      = BIT(13),             ///< Événements système (énergie, locale, affichage)
                NK_CAT_ALL         = 0xFFFFFFFFu          ///< Toutes les catégories (masque complet)
            };

            /// @brief Convertit une valeur de catégorie en chaîne lisible pour le débogage
            /// @param value La valeur de catégorie à convertir
            /// @return NkString contenant le nom symbolique de la catégorie
            static NkString ToString(NkEventCategory::Value value);

            /// @brief Convertit une chaîne en valeur de catégorie (parsing inverse)
            /// @param str La chaîne à parser (case-insensitive)
            /// @return La valeur de catégorie correspondante, ou NK_CAT_NONE si inconnue
            static NkEventCategory::Value FromString(const NkString& str);
        };

        // -------------------------------------------------------------------------
        // Opérateurs bitwise pour NkEventCategory::Value
        // Définis librement dans le namespace pour éviter les conflits de portée.
        // Permettent des expressions naturelles : cat1 | cat2, cat & flag, etc.
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
            a = a | b;
            return a;
        }

        inline NkEventCategory::Value& operator&=(NkEventCategory::Value& a, NkEventCategory::Value b) {
            a = a & b;
            return a;
        }

        // -------------------------------------------------------------------------
        // Utilitaires de catégorie — fonctions helper pour manipuler les flags
        // -------------------------------------------------------------------------

        /// @brief Retourne true si le flag est présent dans le set de catégories
        /// @param set Le set de catégories à tester (combinaison de flags)
        /// @param flag Le flag individuel à rechercher
        /// @return true si (set & flag) != 0
        inline bool NkCategoryHas(NkEventCategory::Value set, NkEventCategory::Value flag) {
            return (static_cast<uint32>(set) & static_cast<uint32>(flag)) != 0;
        }

        /// @brief Retourne true si aucune catégorie n'est définie dans le set
        /// @param set Le set de catégories à tester
        /// @return true si set == NK_CAT_NONE
        inline bool NkCategoryEmpty(NkEventCategory::Value set) {
            return set == NkEventCategory::NK_CAT_NONE;
        }

        /// @brief Retourne true si toutes les catégories sont définies (masque complet)
        /// @param set Le set de catégories à tester
        /// @return true si set == NK_CAT_ALL
        inline bool NkCategoryFull(NkEventCategory::Value set) {
            return set == NkEventCategory::NK_CAT_ALL;
        }

        // =====================================================================
        // NkEventType
        // Énumération complète, unique et extensible de tous les types d'événements.
        //
        // Organisation :
        //   - NK_NONE : valeur nulle par défaut
        //   - Groupes thématiques : Application, Window, Input, System, etc.
        //   - NK_EVENT_COUNT : sentinelle finale pour validation et itération
        //
        // Bonnes pratiques :
        //   - Ne jamais réordonner les valeurs existantes (compatibilité binaire)
        //   - Ajouter les nouveaux types à la fin du groupe concerné
        //   - Utiliser NK_CUSTOM pour les événements définis par l'utilisateur
        // =====================================================================

        struct NKENTSEU_EVENT_CLASS_EXPORT NkEventType {

            /// @brief Valeurs de type d'événement — énumération exhaustive
            enum Value : uint32 {
                NK_NONE = 0,  ///< Type invalide / non initialisé (valeur par défaut)

                // -----------------------------------------------------------------
                // Groupe : Application — Cycle de vie et logique métier
                // -----------------------------------------------------------------
                NK_APP_LAUNCH,          ///< Lancement initial de l'application
                NK_APP_TICK,            ///< Tick logique de la boucle principale (frame update)
                NK_APP_UPDATE,          ///< Mise à jour de la logique métier (après les inputs)
                NK_APP_RENDER,          ///< Demande de rendu graphique (avant swap)
                NK_APP_CLOSE,           ///< Demande de fermeture gracieuse de l'application

                // -----------------------------------------------------------------
                // Groupe : Fenêtre — Gestion des fenêtres natives
                // -----------------------------------------------------------------
                NK_WINDOW_CREATE,       ///< Fenêtre créée et prête à l'emploi
                NK_WINDOW_CLOSE,        ///< Fenêtre en cours de fermeture (requête utilisateur)
                NK_WINDOW_DESTROY,      ///< Fenêtre détruite et ressources libérées
                NK_WINDOW_PAINT,        ///< Demande de repaint (invalidation région)
                NK_WINDOW_RESIZE,       ///< Redimensionnement en cours (live resize)
                NK_WINDOW_RESIZE_BEGIN, ///< Début d'une opération de redimensionnement
                NK_WINDOW_RESIZE_END,   ///< Fin d'une opération de redimensionnement
                NK_WINDOW_MOVE,         ///< Déplacement de la fenêtre en cours
                NK_WINDOW_MOVE_BEGIN,   ///< Début d'une opération de déplacement
                NK_WINDOW_MOVE_END,     ///< Fin d'une opération de déplacement
                NK_WINDOW_FOCUS_GAINED, ///< La fenêtre a obtenu le focus clavier
                NK_WINDOW_FOCUS_LOST,   ///< La fenêtre a perdu le focus clavier
                NK_WINDOW_MINIMIZE,     ///< Fenêtre minimisée (taskbar / dock)
                NK_WINDOW_MAXIMIZE,     ///< Fenêtre maximisée (plein écran travail)
                NK_WINDOW_RESTORE,      ///< Fenêtre restaurée à sa taille normale
                NK_WINDOW_FULLSCREEN,   ///< Passage en mode plein écran exclusif
                NK_WINDOW_WINDOWED,     ///< Retour en mode fenêtré standard
                NK_WINDOW_DPI_CHANGE,   ///< Changement de facteur d'échelle DPI (multi-écran)
                NK_WINDOW_THEME_CHANGE, ///< Changement de thème système (clair/sombre)
                NK_WINDOW_SHOWN,        ///< Fenêtre rendue visible à l'écran
                NK_WINDOW_HIDDEN,       ///< Fenêtre masquée (toujours en mémoire)

                // -----------------------------------------------------------------
                // Groupe : Clavier — Entrées texte et touches
                // -----------------------------------------------------------------
                NK_KEY_PRESSED,         ///< Touche pressée pour la première fois (edge-triggered)
                NK_KEY_REPEATED,        ///< Touche maintenue avec auto-repeat (OS-level)
                NK_KEY_RELEASED,        ///< Touche relâchée (fin de l'interaction)
                NK_TEXT_INPUT,          ///< Entrée texte Unicode après traitement IME/compose
                NK_CHAR_ENTERED,        ///< Caractère individuel saisi (pour édition texte)

                // -----------------------------------------------------------------
                // Groupe : Souris — Pointage, clics et scroll
                // -----------------------------------------------------------------
                NK_MOUSE_MOVE,                  ///< Déplacement du curseur en coordonnées écran
                NK_MOUSE_RAW,                   ///< Mouvement brut (delta sans accélération, pour FPS)
                NK_MOUSE_BUTTON_PRESSED,        ///< Bouton de souris pressé (gauche, droit, milieu...)
                NK_MOUSE_BUTTON_RELEASED,       ///< Bouton de souris relâché
                NK_MOUSE_DOUBLE_CLICK,          ///< Double-clic détecté (timing OS)
                NK_MOUSE_WHEEL_VERTICAL,        ///< Scroll molette verticale (lignes ou pixels)
                NK_MOUSE_WHEEL_HORIZONTAL,      ///< Scroll molette horizontale (souris premium)
                NK_MOUSE_SCROLL,                ///< Scroll unifié (trackpad ou molette) avec vecteur X+Y
                NK_MOUSE_ENTER,                 ///< Curseur entre dans la zone client de la fenêtre
                NK_MOUSE_LEAVE,                 ///< Curseur quitte la zone client de la fenêtre
                NK_MOUSE_WINDOW,                ///< Souris positionnée dans la fenêtre (polling)
                NK_MOUSE_WINDOW_ENTER,          ///< Curseur entre dans les bounds de la fenêtre
                NK_MOUSE_WINDOW_LEAVE,          ///< Curseur quitte les bounds de la fenêtre
                NK_MOUSE_CAPTURE_BEGIN,         ///< Début de capture souris (drag, caméra libre)
                NK_MOUSE_CAPTURE_END,           ///< Fin de capture souris (relâchement)

                // -----------------------------------------------------------------
                // Groupe : Manette — Gamepads et contrôleurs
                // -----------------------------------------------------------------
                NK_GAMEPAD_CONNECT,             ///< Manette connectée (hotplug)
                NK_GAMEPAD_DISCONNECT,          ///< Manette déconnectée (hot-unplug)
                NK_GAMEPAD_BUTTON_PRESSED,      ///< Bouton de manette pressé (A, B, X, Y, Start...)
                NK_GAMEPAD_BUTTON_RELEASED,     ///< Bouton de manette relâché
                NK_GAMEPAD_AXIS_MOTION,         ///< Axe analogique déplacé (stick, gâchette analogique)
                NK_GAMEPAD_RUMBLE,              ///< Commande de vibration (retour haptique)
                NK_GAMEPAD_STICK,               ///< Stick analogique avec position normalisée [-1, 1]
                NK_GAMEPAD_TRIGGERED,           ///< Gâchette numérique ou analogique (LT/RT)

                // -----------------------------------------------------------------
                // Groupe : HID générique — Périphériques non catégorisés
                // -----------------------------------------------------------------
                NK_GENERIC_CONNECT,             ///< Périphérique HID connecté (non gamepad)
                NK_GENERIC_DISCONNECT,          ///< Périphérique HID déconnecté
                NK_GENERIC_BUTTON_PRESSED,      ///< Bouton HID pressé (mapping générique)
                NK_GENERIC_BUTTON_RELEASED,     ///< Bouton HID relâché
                NK_GENERIC_AXIS_MOTION,         ///< Axe HID déplacé (potentiomètre, joystick...)
                NK_GENERIC_RUMBLE,              ///< Vibration HID (si supportée)
                NK_GENERIC_STICK,               ///< Stick HID avec coordonnées normalisées
                NK_GENERIC_TRIGGERED,           ///< Gâchette ou trigger HID activé

                // -----------------------------------------------------------------
                // Groupe : Tactile / Gestures — Écrans tactiles et multi-touch
                // -----------------------------------------------------------------
                NK_TOUCH_BEGIN,                 ///< Contact tactile initial (finger down)
                NK_TOUCH_MOVE,                  ///< Contact tactile déplacé (finger drag)
                NK_TOUCH_END,                   ///< Contact tactile terminé (finger up)
                NK_TOUCH_CANCEL,                ///< Contact tactile annulé (système ou conflit)
                NK_TOUCH_PRESSED,               ///< Appui tactile détecté (pression seuil)
                NK_TOUCH_RELEASED,              ///< Relâchement tactile détecté
                NK_GESTURE_PINCH,               ///< Geste pincement/zoom à deux doigts
                NK_GESTURE_ROTATE,              ///< Geste rotation à deux doigts
                NK_GESTURE_PAN,                 ///< Geste panoramique/défilement à plusieurs doigts
                NK_GESTURE_SWIPE,               ///< Geste balayage rapide (directionnel)
                NK_GESTURE_TAP,                 ///< Geste tap simple (clic tactile)
                NK_GESTURE_LONG_PRESS,          ///< Geste appui long (context menu trigger)

                // -----------------------------------------------------------------
                // Groupe : Drag & Drop — Transfert interactif de données
                // -----------------------------------------------------------------
                NK_DROP_FILE,                   ///< Fichier déposé depuis l'explorateur
                NK_DROP_TEXT,                   ///< Texte déposé depuis une autre application
                NK_DROP_IMAGE,                  ///< Image déposée (bitmap, PNG, JPEG...)
                NK_DROP_ENTER,                  ///< Curseur avec données entre dans zone de dépôt
                NK_DROP_OVER,                   ///< Curseur avec données survole zone de dépôt (feedback)
                NK_DROP_LEAVE,                  ///< Curseur avec données quitte zone de dépôt

                // -----------------------------------------------------------------
                // Groupe : Système — Événements OS et environnement
                // -----------------------------------------------------------------
                NK_SYSTEM_POWER,                ///< Changement d'état d'alimentation (veille, réveil, batterie)
                NK_SYSTEM_LOCALE,               ///< Changement de locale système (langue, région)
                NK_SYSTEM_DISPLAY,              ///< Changement de configuration d'affichage (ajout/retrait écran)
                NK_SYSTEM_MEMORY,               ///< Notification de pression mémoire (low memory warning)

                // -----------------------------------------------------------------
                // Groupe : Transfert — Échanges de données asynchrones
                // -----------------------------------------------------------------
                NK_TRANSFER,                    ///< Transfert de données ou fichier en cours/terminé

                // -----------------------------------------------------------------
                // Groupe : Personnalisé — Extension par l'utilisateur
                // -----------------------------------------------------------------
                NK_CUSTOM,                      ///< Événement défini par l'utilisateur (type générique)

                /// @brief Sentinelle finale — doit toujours rester en dernière position
                /// Utilisée pour la validation de plage et l'itération sécurisée
                NK_EVENT_COUNT
            };

            /// @brief Convertit un type d'événement en chaîne lisible pour le débogage
            /// @param value La valeur de type à convertir
            /// @return NkString contenant le nom symbolique du type (ex: "NK_KEY_PRESSED")
            static NkString ToString(NkEventType::Value value);

            /// @brief Convertit une chaîne en type d'événement (parsing inverse)
            /// @param str La chaîne à parser (case-insensitive)
            /// @return La valeur de type correspondante, ou NK_NONE si inconnue
            static NkEventType::Value FromString(const NkString& str);
        };

        // =====================================================================
        // NkTimestampMs — Type alias pour les timestamps en millisecondes
        // =====================================================================

        /// @brief Millisecondes écoulées depuis l'initialisation du système d'événements
        /// Utilisé pour le profiling, le replay et la synchronisation temporelle
        using NkTimestampMs = uint64;

        // =====================================================================
        // Macros helper pour les classes dérivées de NkEvent
        //
        // Objectif : Réduire la duplication de code boilerplate dans les événements
        //
        // NK_EVENT_BIND_HANDLER(methode)
        //   Crée un lambda capturant 'this' pour binder une méthode membre
        //   Compatible avec NkFunction et les callbacks asynchrones
        //
        // NK_EVENT_STATIC_TYPE(TYPE)
        //   Expose GetStaticType() pour le RTTI léger (utilisé par Is<T>/As<T>)
        //
        // NK_EVENT_TYPE_FLAGS(TYPE)
        //   Implémente GetType(), GetTypeStr() et GetName() en une ligne
        //
        // NK_EVENT_CATEGORY_FLAGS(CAT)
        //   Implémente GetCategoryFlags() pour le filtrage par catégorie
        //
        // Exemple d'usage dans une classe dérivée :
        //   class NkKeyPressEvent : public NkEvent {
        //       NK_EVENT_TYPE_FLAGS(NK_KEY_PRESSED)
        //       NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_INPUT | NkEventCategory::NK_CAT_KEYBOARD)
        //       // ... implémentation ...
        //   };
        // =====================================================================

        /// @brief Crée un lambda vers une méthode membre pour utilisation comme handler
        /// @param method_ Nom de la méthode membre à binder
        /// @return Lambda capturant 'this' et forwardant l'argument d'événement
        /// @note Utilise NkForward pour préserver les références et qualifiers
        #define NK_EVENT_BIND_HANDLER(method_) \
            [this](auto&& eventArg) -> decltype(auto) { \
                return method_(traits::NkForward<decltype(eventArg)>(eventArg)); \
            }

        /// @brief Expose la méthode statique GetStaticType() sur une classe dérivée
        /// @param type_ Valeur NkEventType::Value associée à la classe
        /// @note Requis pour que Is<T>() et As<T>() fonctionnent correctement
        #define NK_EVENT_STATIC_TYPE(type_) \
            static NkEventType::Value GetStaticType() { \
                return NkEventType::type_; \
            }

        /// @brief Implémente les méthodes de type identification pour une classe dérivée
        /// @param type_ Valeur NkEventType::Value à associer
        /// @note Génère : GetStaticType(), GetType(), GetTypeStr(), GetName()
        #define NK_EVENT_
            NK_EVENT_STATIC_TYPE(type_) \
            NkEventType::Value GetType() const override { \
                return GetStaticType(); \
            } \
            const char* GetTypeStr() const override { \
                return #type_; \
            } \
            const char* GetName() const override { \
                return #type_; \
            }

        /// @brief Implémente la méthode GetCategoryFlags() pour le filtrage par catégorie
        /// @param category_ Expression de flags NkEventCategory::Value
        /// @note Supporte les combinaisons : NK_CAT_INPUT | NK_CAT_KEYBOARD
        #define NK_EVENT_CATEGORY_FLAGS(category_) \
            NKENTSEU_EVENT_API uint32 GetCategoryFlags() const override { \
                return static_cast<uint32>(category_); \
            }

        // =====================================================================
        // NkEvent — Classe de base polymorphe pour tous les événements
        //
        // Cycle de vie typique :
        //   1. Construction  : windowId, timestamp et état initialisés
        //   2. Distribution  : transmis aux handlers enregistrés via EventManager
        //   3. Traitement    : un handler appelle MarkHandled() pour stopper
        //                      la propagation aux handlers suivants
        //   4. Destruction   : nettoyage automatique via destructeur virtuel
        //
        // Contrat pour les classes dérivées :
        //   - Surcharger GetType() et GetCategoryFlags() (via macros ci-dessus)
        //   - Surcharger ToString() pour un débogage significatif
        //   - Implémenter Clone() si une copie polymorphe est nécessaire (replay)
        //   - Utiliser NK_EVENT_TYPE_FLAGS et NK_EVENT_CATEGORY_FLAGS pour éviter la duplication
        //
        // Thread-safety :
        //   - Les événements sont typiquement créés et dispatchés sur le thread principal
        //   - Pour un usage multi-thread, externaliser la synchronisation au niveau du dispatcher
        // =====================================================================

        class NKENTSEU_EVENT_CLASS_EXPORT NkEvent {
            public:

                // -------------------------------------------------------------
                // Interface virtuelle à surcharger dans les classes dérivées
                // -------------------------------------------------------------

                /// @brief Retourne le type spécifique de l'événement
                /// @return Valeur NkEventType::Value identifiant l'événement
                /// @note Doit être surchargé par chaque classe dérivée concrète
                virtual NkEventType::Value GetType() const;

                /// @brief Retourne le nom de la classe C++ de l'événement
                /// @return String littérale du nom de classe (pour débogage)
                /// @note Utile pour les logs et l'inspection runtime
                virtual const char* GetName() const;

                /// @brief Retourne une représentation string du type d'événement
                /// @return String symbolique du type (ex: "NK_KEY_PRESSED")
                /// @note Délègue généralement à NkEventType::ToString()
                virtual const char* GetTypeStr() const;

                /// @brief Retourne les flags de catégorie sous forme de bitfield
                /// @return Valeur uint32 combinant les flags NkEventCategory::Value
                /// @note Utilisé pour le filtrage efficace des handlers
                virtual uint32 GetCategoryFlags() const;

                /// @brief Crée une copie polymorphe de l'événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                /// @note Alternative moderne : utiliser std::unique_ptr<NkEvent> avec Clone()
                virtual NkEvent* Clone() const;

                // -------------------------------------------------------------
                // Constructeurs et destructeur
                // -------------------------------------------------------------

                /// @brief Constructeur avec identifiant de fenêtre source
                /// @param windowId Identifiant unique de la fenêtre émettrice (0 si global)
                explicit NKENTSEU_EVENT_API NkEvent(uint64 windowId);

                /// @brief Constructeur par défaut (windowId = 0, événement global)
                NkEvent();

                /// @brief Constructeur de copie — copie profonde des membres de base
                /// @param other Référence constante vers l'événement à copier
                NkEvent(const NkEvent& other);

                /// @brief Destructeur virtuel — indispensable pour le polymorphisme
                /// @note Permet la destruction correcte via pointeur de base
                virtual NKENTSEU_EVENT_API ~NkEvent() = default;

                // -------------------------------------------------------------
                // Accesseurs — lecture et modification des propriétés
                // -------------------------------------------------------------

                /// @brief Retourne la catégorie principale de l'événement
                /// @return Valeur NkEventCategory::Value extraite des flags
                /// @note Utilise un cast depuis GetCategoryFlags() pour compatibilité
                NkEventCategory::Value GetCategory() const {
                    return static_cast<NkEventCategory::Value>(GetCategoryFlags());
                }

                /// @brief Retourne l'identifiant de la fenêtre source de l'événement
                /// @return uint64 windowId (0 si l'événement est global / sans fenêtre)
                uint64 GetWindowId() const {
                    return mWindowID;
                }

                /// @brief Définit l'identifiant de la fenêtre source
                /// @param id Nouvel identifiant de fenêtre à associer
                /// @note Utile pour le rerouting d'événements entre fenêtres
                void SetWindowId(uint64 id) {
                    mWindowID = id;
                }

                /// @brief Retourne le timestamp de création de l'événement
                /// @return NkTimestampMs en millisecondes depuis l'init système
                /// @note Utilisé pour le profiling, le replay et la synchronisation
                NkTimestampMs GetTimestamp() const {
                    return mTimestamp;
                }

                // -------------------------------------------------------------
                // État de traitement — contrôle de la propagation
                // -------------------------------------------------------------

                /// @brief Retourne true si l'événement a déjà été marqué comme traité
                /// @return bool indiquant l'état du flag mHandled
                /// @note Un événement "handled" peut être ignoré par les handlers suivants
                bool IsHandled() const {
                    return mHandled;
                }

                /// @brief Marque l'événement comme traité pour stopper sa propagation
                /// @note À appeler dans un handler quand l'événement est consommé
                void MarkHandled() {
                    mHandled = true;
                }

                /// @brief Réinitialise l'état "traité" pour permettre un re-dispatch
                /// @note Utile pour les systèmes de replay ou de re-traitement
                void Unmark() {
                    mHandled = false;
                }

                // -------------------------------------------------------------
                // Vérifications et prédicats — tests rapides sur l'événement
                // -------------------------------------------------------------

                /// @brief Retourne true si l'événement a un type valide (différent de NK_NONE)
                /// @return bool indiquant si GetType() != NkEventType::NK_NONE
                bool IsValid() const {
                    return GetType() != NkEventType::NK_NONE;
                }

                /// @brief Retourne true si l'événement correspond au type spécifié
                /// @param type Valeur NkEventType::Value à comparer
                /// @return true si GetType() == type
                bool IsType(NkEventType::Value type) const {
                    return GetType() == type;
                }

                /// @brief Retourne true si l'événement appartient à la catégorie donnée
                /// @param flag Flag NkEventCategory::Value à tester
                /// @return true si le flag est présent dans GetCategoryFlags()
                bool HasCategory(NkEventCategory::Value flag) const {
                    return (GetCategoryFlags() & static_cast<uint32>(flag)) != 0;
                }

                // -------------------------------------------------------------
                // Casting polymorphe type-safe — alternative légère au RTTI
                // -------------------------------------------------------------

                /// @brief Retourne true si l'événement est exactement du type T
                /// @tparam T Classe dérivée de NkEvent exposant GetStaticType()
                /// @return true si GetType() == T::GetStaticType()
                /// @note Plus rapide et plus sûr que dynamic_cast pour les événements
                template <typename T>
                bool Is() const {
                    return GetType() == T::GetStaticType();
                }

                /// @brief Caste vers T* si l'événement est du type T, sinon nullptr
                /// @tparam T Classe dérivée de NkEvent exposant GetStaticType()
                /// @return Pointeur vers T si le type correspond, nullptr sinon
                /// @note Permet d'accéder aux membres spécifiques du type dérivé
                template <typename T>
                T* As() {
                    return Is<T>() ? static_cast<T*>(this) : nullptr;
                }

                /// @brief Caste vers const T* si l'événement est du type T, sinon nullptr
                /// @tparam T Classe dérivée de NkEvent exposant GetStaticType()
                /// @return Pointeur const vers T si le type correspond, nullptr sinon
                /// @note Version const pour les contextes en lecture seule
                template <typename T>
                const T* As() const {
                    return Is<T>() ? static_cast<const T*>(this) : nullptr;
                }

                // -------------------------------------------------------------
                // Sérialisation et débogage — représentation textuelle
                // -------------------------------------------------------------

                /// @brief Retourne une représentation lisible de l'événement pour logs/debug
                /// @return NkString contenant type, windowId, timestamp et état
                /// @note À surcharger dans les dérivés pour inclure les données spécifiques
                virtual NkString ToString() const;

                /// @brief Convertit un type d'événement en string (délègue à NkEventType::ToString)
                /// @param type Valeur NkEventType::Value à convertir
                /// @return NkString contenant le nom symbolique du type
                static NkString TypeToString(NkEventType::Value type);

                /// @brief Convertit une catégorie en string (délègue à NkEventCategory::ToString)
                /// @param category Valeur NkEventCategory::Value à convertir
                /// @return NkString contenant le nom symbolique de la catégorie
                static NkString CategoryToString(NkEventCategory::Value category);

            protected:
                // -------------------------------------------------------------
                // Membres protégés — accessibles aux classes dérivées
                // -------------------------------------------------------------

                uint64         mWindowID  = 0;        ///< Identifiant de la fenêtre source (0 = global)
                NkTimestampMs mTimestamp = 0;        ///< Timestamp de création en millisecondes
                bool          mHandled   = false;    ///< Flag indiquant si l'événement a été consommé

                /// @brief Retourne le timestamp courant en millisecondes
                /// @return NkTimestampMs depuis l'initialisation du système
                /// @note Utilise std::chrono ou l'API plateforme pour précision
                static NkTimestampMs GetCurrentTimestamp();
        };

        // =====================================================================
        // Alias de callbacks pour les événements — flexibilité et type-safety
        //
        // Conventions de nommage :
        //   - Observer  : handler recevant un événement modifiable (peut appeler MarkHandled)
        //   - ObserverRef : handler recevant un événement en lecture seule (observation pure)
        //   - EventHandler  : alias sémantique pour un handler actif (modifie l'état)
        //   - EventHandlerRef : alias sémantique pour un handler passif (logging, stats)
        //
        // Type sous-jacent : NkFunction — type-erased callable avec petite optimisation
        // =====================================================================

        /// @brief Callback recevant un événement modifiable (peut le consommer)
        using EventObserver = NkFunction<void(NkEvent&)>;

        /// @brief Callback recevant un événement en lecture seule (observation)
        using EventObserverRef = NkFunction<void(const NkEvent&)>;

        /// @brief Alias sémantique pour un handler actif (modifie l'état de l'événement)
        using EventHandler = NkFunction<void(NkEvent&)>;

        /// @brief Alias sémantique pour un handler passif (lecture seule, logging)
        using EventHandlerRef = NkFunction<void(const NkEvent&)>;

        // =====================================================================
        // NkEventCallback — Type de callback bas niveau pour compatibilité C/API
        // =====================================================================

        /// @brief Callback bas niveau recevant un pointeur brut vers NkEvent
        /// @note À utiliser avec précaution — pas de gestion automatique de mémoire
        /// @note Préférer EventObserver pour le code C++ moderne
        using NkEventCallback = NkFunction<void(NkEvent*)>;

    } // namespace nkentseu

#endif // NKENTSEU_EVENT_NKEVENT_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKEVENT.H
// =============================================================================
// Ce fichier définit l'infrastructure de base pour le système d'événements NKEvent.
// Les exemples ci-dessous illustrent les patterns d'usage recommandés.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Création d'un événement personnalisé dérivé de NkEvent
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkEvent.h"

    // Définition d'un événement de pression de touche
    class NKENTSEU_EVENT_CLASS_EXPORT NkKeyPressEvent : public nkentseu::NkEvent {
    public:
        // Macros pour éviter la duplication de code boilerplate
        NK_EVENT_TYPE_FLAGS(NK_KEY_PRESSED)
        NK_EVENT_CATEGORY_FLAGS(
            nkentseu::NkEventCategory::NK_CAT_INPUT |
            nkentseu::NkEventCategory::NK_CAT_KEYBOARD
        )

        // Constructeur avec paramètres spécifiques
        NKENTSEU_EVENT_API NkKeyPressEvent(uint64 windowId, uint32 keyCode, bool isRepeat)
            : NkEvent(windowId)
            , mKeyCode(keyCode)
            , mIsRepeat(isRepeat)
        {
        }

        // Accesseurs spécifiques au type
        uint32 GetKeyCode() const {
            return mKeyCode;
        }

        bool IsRepeat() const {
            return mIsRepeat;
        }

        // Surcharge de ToString pour débogage significatif
        NKENTSEU_EVENT_API nkentseu::NkString ToString() const override {
            return nkentseu::NkFormat("NkKeyPressEvent[keyCode={}, repeat={}]",
                mKeyCode, mIsRepeat);
        }

        // Clone pour copie polymorphe (si nécessaire pour replay)
        NKENTSEU_EVENT_API nkentseu::NkEvent* Clone() const override {
            return new NkKeyPressEvent(*this);
        }

    private:
        uint32 mKeyCode;    // Code de touche (scancode ou keycode)
        bool   mIsRepeat;   // true si auto-repeat OS
    };
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Enregistrement et utilisation de handlers d'événements
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkEvent.h"
    #include "NKEvent/EventManager.h"  // Supposé exister

    class GameInputHandler {
    public:
        void Register(nkentseu::event::EventManager& eventMgr) {
            using namespace nkentseu;

            // Handler modifiable — peut consommer l'événement
            eventMgr.Subscribe<NkKeyPressEvent>(
                [this](NkKeyPressEvent& event) {
                    if (event.GetKeyCode() == KEY_ESCAPE) {
                        event.MarkHandled();  // Stop propagation
                        RequestPauseMenu();
                    }
                }
            );

            // Handler en lecture seule — pour logging ou stats
            eventMgr.SubscribeRef<NkMousePressEvent>(
                [](const NkMousePressEvent& event) {
                    NK_FOUNDATION_LOG_DEBUG("Mouse click at ({}, {})",
                        event.GetX(), event.GetY());
                }
            );

            // Utilisation de la macro de binding pour méthode membre
            eventMgr.Subscribe<NkGamepadButtonEvent>(
                NK_EVENT_BIND_HANDLER(OnGamepadButton)
            );
        }

    private:
        void OnGamepadButton(NkGamepadButtonEvent& event) {
            if (event.GetButton() == GAMEPAD_BUTTON_A && event.IsPressed()) {
                ConfirmSelection();
            }
        }

        void RequestPauseMenu();
        void ConfirmSelection();
    };
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Dispatch manuel et inspection d'événements
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkEvent.h"

    void ProcessEvent(nkentseu::NkEvent& event) {
        using namespace nkentseu;

        // Vérification de validité basique
        if (!event.IsValid()) {
            NK_FOUNDATION_LOG_WARNING("Received invalid event");
            return;
        }

        // Filtrage par catégorie pour optimisation
        if (event.HasCategory(NkEventCategory::NK_CAT_INPUT)) {
            // Casting type-safe vers un type spécifique
            if (auto* keyEvent = event.As<NkKeyPressEvent>()) {
                HandleKeyPress(*keyEvent);
                return;  // Événement consommé
            }

            if (auto* mouseEvent = event.As<NkMouseMoveEvent>()) {
                HandleMouseMove(*mouseEvent);
                return;
            }
        }

        // Fallback : traitement générique avec ToString pour debug
        if (!event.IsHandled()) {
            NK_FOUNDATION_LOG_TRACE("Unhandled event: {}", event.ToString());
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Utilisation du RTTI léger (Is<T> / As<T>)
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkEvent.h"

    void DispatchToSystems(nkentseu::NkEvent& event) {
        using namespace nkentseu;

        // Pattern Is<T> pour branchement rapide
        if (event.Is<NkWindowResizeEvent>()) {
            renderer.OnWindowResized(*event.As<NkWindowResizeEvent>());
            uiLayout.Recompute();
        }

        // Pattern As<T> avec vérification nullptr
        if (auto* dropEvent = event.As<NkDropFileEvent>()) {
            assetLoader.ImportFile(dropEvent->GetFilePath());
            event.MarkHandled();
        }

        // Combinaison avec filtrage par catégorie
        if (event.HasCategory(NkEventCategory::NK_CAT_GAMEPAD)) {
            inputMapper.MapGamepadEvent(event);
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Configuration CMake pour NKEvent
// -----------------------------------------------------------------------------
/*
    # CMakeLists.txt du module NKEvent

    cmake_minimum_required(VERSION 3.15)
    project(NKEvent VERSION 1.0.0 LANGUAGES CXX)

    # Options de build
    option(NKEVENT_BUILD_SHARED "Build NKEvent as shared library" ON)
    option(NKEVENT_HEADER_ONLY "Use NKEvent in header-only mode" OFF)

    # Configuration des defines d'export
    if(NKEVENT_HEADER_ONLY)
        add_definitions(-DNKENTSEU_EVENT_HEADER_ONLY)
    elseif(NKEVENT_BUILD_SHARED)
        add_definitions(-DNKENTSEU_EVENT_BUILD_SHARED_LIB)
        set(NKEVENT_LIB_TYPE SHARED)
    else()
        add_definitions(-DNKENTSEU_EVENT_STATIC_LIB)
        set(NKEVENT_LIB_TYPE STATIC)
    endif()

    # Création de la bibliothèque
    add_library(nkevent ${NKEVENT_LIB_TYPE}
        src/NkEvent.cpp
        src/EventManager.cpp
        src/EventDispatcher.cpp
        src/EventQueue.cpp
    )

    # Dépendances internes au projet
    target_link_libraries(nkevent PUBLIC
        NKPlatform::NKPlatform
        NKCore::NKCore
        NKContainers::NKContainers
        NKMath::NKMath
    )

    # Include directories publics
    target_include_directories(nkevent PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    # Installation pour distribution
    install(TARGETS nkevent EXPORT NKEventTargets)
    install(DIRECTORY include/NKEvent DESTINATION include)
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Pattern de replay / enregistrement d'événements
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkEvent.h"
    #include <vector>
    #include <memory>

    class EventRecorder {
    public:
        // Enregistrement d'un événement pour replay ultérieur
        void Record(const nkentseu::NkEvent& event) {
            // Clone polymorphe pour copie profonde
            std::unique_ptr<nkentseu::NkEvent> copy(event.Clone());
            mRecordedEvents.push_back(std::move(copy));
        }

        // Replay des événements enregistrés
        void Replay(nkentseu::event::EventManager& dispatcher) {
            for (const auto& eventPtr : mRecordedEvents) {
                if (eventPtr && eventPtr->IsValid()) {
                    // Reset de l'état "handled" avant re-dispatch
                    const_cast<nkentseu::NkEvent&>(*eventPtr).Unmark();
                    dispatcher.Dispatch(*eventPtr);
                }
            }
        }

        // Clear de l'historique
        void Clear() {
            mRecordedEvents.clear();
        }

    private:
        std::vector<std::unique_ptr<nkentseu::NkEvent>> mRecordedEvents;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Filtrage avancé par catégorie et type combinés
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkEvent.h"

    class SmartEventHandler {
    public:
        // Configuration d'un handler avec filtres multiples
        void SetupFilters(nkentseu::event::EventManager& mgr) {
            using namespace nkentseu;

            // Filtrer uniquement les événements clavier NON-répétés
            mgr.Subscribe<NkKeyPressEvent>(
                [this](NkKeyPressEvent& event) {
                    if (!event.IsRepeat()) {
                        ProcessInitialKeyPress(event);
                    }
                },
                // Filtre supplémentaire par catégorie (optionnel)
                NkEventCategory::NK_CAT_KEYBOARD
            );

            // Filtrer les événements souris uniquement sur fenêtre active
            mgr.Subscribe<NkMouseMoveEvent>(
                [this](NkMouseMoveEvent& event) {
                    if (event.GetWindowId() == mActiveWindowId) {
                        UpdateCursorPreview(event);
                    }
                }
            );
        }

    private:
        void ProcessInitialKeyPress(const NkKeyPressEvent& event);
        void UpdateCursorPreview(const NkMouseMoveEvent& event);
        uint64 mActiveWindowId = 0;
    };
*/

// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
    1. EXTENSIBILITÉ :
       - Pour ajouter un nouveau type d'événement :
         a) Ajouter une entrée dans NkEventType::Value (groupe thématique)
         b) Créer une classe dérivée de NkEvent avec les macros appropriées
         c) Implémenter ToString() et Clone() si nécessaire
       - Ne jamais modifier l'ordre des valeurs existantes (compatibilité binaire)

    2. PERFORMANCE :
       - Privilégier Is<T>() pour les checks rapides (pas de cast)
       - Utiliser As<T>() uniquement quand l'accès aux membres dérivés est requis
       - Les macros NK_EVENT_* génèrent du code inline optimisé par le compilateur

    3. THREAD-SAFETY :
       - Les événements sont conçus pour un usage single-thread (thread principal)
       - Pour un dispatch multi-thread, utiliser une queue thread-safe en amont
       - Éviter de modifier un événement partagé entre threads sans synchronisation

    4. MÉMOIRE :
       - Clone() retourne un pointeur brut : utiliser std::unique_ptr pour gestion RAII
       - Les handlers NkFunction utilisent small-buffer optimization pour les lambdas courts
       - Éviter les captures par référence dans les lambdas long-lived (risque de dangling)

    5. DÉBOGAGE :
       - Surcharger ToString() dans chaque événement dérivé pour logs significatifs
       - Utiliser NKENTSEU_EVENT_DEBUG pour activer les traces de dispatch en dev
       - Le timestamp permet de profiler la latence entre génération et traitement
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================