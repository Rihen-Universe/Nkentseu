#include "pch.h"
// =============================================================================
// NKEvent/NkEvent.cpp
// Implémentation des classes et utilitaires du système d'événements Nkentseu.
//
// Contenu :
//   - NkEventCategory::ToString / FromString : sérialisation des catégories
//   - NkEventType::ToString / FromString : sérialisation des types d'événements
//   - NkEvent : implémentation des méthodes de base (constructeurs, RTTI, ToString)
//
// Design :
//   - Séparation claire entre logique de sérialisation et logique métier
//   - Utilisation de NkFormat et NkChrono pour cohérence avec le reste du projet
//   - Fonctions ToString optimisées pour le débogage sans allocation excessive
//   - Implémentations virtuelles par défaut pour NkEvent (pattern Template Method)
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "NKEvent/NkEvent.h"
#include "NKEvent/NkEventApi.h"
#include "NKContainers/String/NkFormat.h"
#include "NKTime/NkChrono.h"
#include <cstdio>

namespace nkentseu {

    // =====================================================================
    // NkEventCategory — Implémentation des méthodes de sérialisation
    // =====================================================================

    /// @brief Convertit une valeur de catégorie en chaîne lisible pour le débogage
    /// @param value La valeur de catégorie à convertir (flags combinables)
    /// @return NkString contenant les noms des flags actifs, séparés par '|'
    /// @note Gère les cas spéciaux NK_CAT_NONE et NK_CAT_ALL en priorité
    /// @note Utilise un lambda interne pour éviter la duplication de code d'append
    NkString NkEventCategory::ToString(NkEventCategory::Value value) {
        // Cas spéciaux : valeurs sentinelles traitées en priorité
        if (value == NK_CAT_NONE) {
            return "NONE";
        }
        if (value == NK_CAT_ALL) {
            return "ALL";
        }

        // Buffer de résultat pour construire la chaîne composite
        NkString result;

        // Lambda interne pour factoriser la logique d'ajout conditionnel de flag
        // Évite la duplication de code pour chaque catégorie possible
        auto append = [&](NkEventCategory::Value flag, const char* name) {
            // Ajoute le nom du flag seulement s'il est présent dans le set
            if (NkCategoryHas(value, flag)) {
                // Séparateur '|' si ce n'est pas le premier élément ajouté
                if (!result.Empty()) {
                    result += '|';
                }
                // Concaténation du nom symbolique du flag
                result += name;
            }
        };

        // Exploration séquentielle de tous les flags définis
        // Ordre déterministe pour une sortie ToString() reproductible
        append(NK_CAT_APPLICATION, "APPLICATION");
        append(NK_CAT_INPUT, "INPUT");
        append(NK_CAT_KEYBOARD, "KEYBOARD");
        append(NK_CAT_MOUSE, "MOUSE");
        append(NK_CAT_WINDOW, "WINDOW");
        append(NK_CAT_GRAPHICS, "GRAPHICS");
        append(NK_CAT_TOUCH, "TOUCH");
        append(NK_CAT_GAMEPAD, "GAMEPAD");
        append(NK_CAT_CUSTOM, "CUSTOM");
        append(NK_CAT_TRANSFER, "TRANSFER");
        append(NK_CAT_GENERIC_HID, "GENERIC_HID");
        append(NK_CAT_DROP, "DROP");
        append(NK_CAT_SYSTEM, "SYSTEM");

        // Fallback : retourne "UNKNOWN" si aucun flag connu n'a été détecté
        return result.Empty() ? "UNKNOWN" : result;
    }

    /// @brief Parse une chaîne pour retrouver la valeur de catégorie correspondante
    /// @param str La chaîne à parser (comparaison case-sensitive exacte)
    /// @return Valeur NkEventCategory::Value correspondante, ou NK_CAT_NONE si inconnue
    /// @note Ne gère pas les combinaisons de flags (ex: "INPUT|KEYBOARD")
    /// @note Pour un parsing avancé, utiliser un parser dédié en couche supérieure
    NkEventCategory::Value NkEventCategory::FromString(const NkString& str) {
        // Cas spéciaux : valeurs sentinelles traitées en priorité
        if (str == "NONE") {
            return NK_CAT_NONE;
        }
        if (str == "ALL") {
            return NK_CAT_ALL;
        }

        // Série de comparaisons séquentielles pour chaque flag défini
        // Ordre identique à ToString() pour cohérence bidirectionnelle
        if (str == "APPLICATION") {
            return NK_CAT_APPLICATION;
        }
        if (str == "INPUT") {
            return NK_CAT_INPUT;
        }
        if (str == "KEYBOARD") {
            return NK_CAT_KEYBOARD;
        }
        if (str == "MOUSE") {
            return NK_CAT_MOUSE;
        }
        if (str == "WINDOW") {
            return NK_CAT_WINDOW;
        }
        if (str == "GRAPHICS") {
            return NK_CAT_GRAPHICS;
        }
        if (str == "TOUCH") {
            return NK_CAT_TOUCH;
        }
        if (str == "GAMEPAD") {
            return NK_CAT_GAMEPAD;
        }
        if (str == "CUSTOM") {
            return NK_CAT_CUSTOM;
        }
        if (str == "TRANSFER") {
            return NK_CAT_TRANSFER;
        }
        if (str == "GENERIC_HID") {
            return NK_CAT_GENERIC_HID;
        }
        if (str == "DROP") {
            return NK_CAT_DROP;
        }
        if (str == "SYSTEM") {
            return NK_CAT_SYSTEM;
        }

        // Fallback : retourne NK_CAT_NONE pour toute chaîne non reconnue
        return NK_CAT_NONE;
    }

    // =====================================================================
    // NkEventType — Implémentation des méthodes de sérialisation
    // =====================================================================

    /// @brief Convertit un type d'événement en chaîne symbolique pour le débogage
    /// @param value La valeur NkEventType::Value à convertir
    /// @return NkString contenant le nom symbolique du type (ex: "NK_KEY_PRESSED")
    /// @note Implémentation par switch exhaustif pour optimisation compilateur
    /// @note Retourne "NK_UNKNOWN" pour toute valeur hors plage définie
    NkString NkEventType::ToString(NkEventType::Value value) {
        // Dispatch par switch pour optimisation par table de saut (O(1))
        switch (value) {
            // -----------------------------------------------------------------
            // Valeurs de base et sentinelles
            // -----------------------------------------------------------------
            case NK_NONE:
                return "NK_NONE";

            // -----------------------------------------------------------------
            // Groupe : Application — Cycle de vie et logique métier
            // -----------------------------------------------------------------
            case NK_APP_LAUNCH:
                return "NK_APP_LAUNCH";
            case NK_APP_TICK:
                return "NK_APP_TICK";
            case NK_APP_UPDATE:
                return "NK_APP_UPDATE";
            case NK_APP_RENDER:
                return "NK_APP_RENDER";
            case NK_APP_CLOSE:
                return "NK_APP_CLOSE";

            // -----------------------------------------------------------------
            // Groupe : Fenêtre — Gestion des fenêtres natives
            // -----------------------------------------------------------------
            case NK_WINDOW_CREATE:
                return "NK_WINDOW_CREATE";
            case NK_WINDOW_CLOSE:
                return "NK_WINDOW_CLOSE";
            case NK_WINDOW_DESTROY:
                return "NK_WINDOW_DESTROY";
            case NK_WINDOW_PAINT:
                return "NK_WINDOW_PAINT";
            case NK_WINDOW_RESIZE:
                return "NK_WINDOW_RESIZE";
            case NK_WINDOW_RESIZE_BEGIN:
                return "NK_WINDOW_RESIZE_BEGIN";
            case NK_WINDOW_RESIZE_END:
                return "NK_WINDOW_RESIZE_END";
            case NK_WINDOW_MOVE:
                return "NK_WINDOW_MOVE";
            case NK_WINDOW_MOVE_BEGIN:
                return "NK_WINDOW_MOVE_BEGIN";
            case NK_WINDOW_MOVE_END:
                return "NK_WINDOW_MOVE_END";
            case NK_WINDOW_FOCUS_GAINED:
                return "NK_WINDOW_FOCUS_GAINED";
            case NK_WINDOW_FOCUS_LOST:
                return "NK_WINDOW_FOCUS_LOST";
            case NK_WINDOW_MINIMIZE:
                return "NK_WINDOW_MINIMIZE";
            case NK_WINDOW_MAXIMIZE:
                return "NK_WINDOW_MAXIMIZE";
            case NK_WINDOW_RESTORE:
                return "NK_WINDOW_RESTORE";
            case NK_WINDOW_FULLSCREEN:
                return "NK_WINDOW_FULLSCREEN";
            case NK_WINDOW_WINDOWED:
                return "NK_WINDOW_WINDOWED";
            case NK_WINDOW_DPI_CHANGE:
                return "NK_WINDOW_DPI_CHANGE";
            case NK_WINDOW_THEME_CHANGE:
                return "NK_WINDOW_THEME_CHANGE";
            case NK_WINDOW_SHOWN:
                return "NK_WINDOW_SHOWN";
            case NK_WINDOW_HIDDEN:
                return "NK_WINDOW_HIDDEN";

            // -----------------------------------------------------------------
            // Groupe : Clavier — Entrées texte et touches
            // -----------------------------------------------------------------
            case NK_KEY_PRESSED:
                return "NK_KEY_PRESSED";
            case NK_KEY_REPEATED:
                return "NK_KEY_REPEATED";
            case NK_KEY_RELEASED:
                return "NK_KEY_RELEASED";
            case NK_TEXT_INPUT:
                return "NK_TEXT_INPUT";
            case NK_CHAR_ENTERED:
                return "NK_CHAR_ENTERED";

            // -----------------------------------------------------------------
            // Groupe : Souris — Pointage, clics et scroll
            // -----------------------------------------------------------------
            case NK_MOUSE_MOVE:
                return "NK_MOUSE_MOVE";
            case NK_MOUSE_RAW:
                return "NK_MOUSE_RAW";
            case NK_MOUSE_BUTTON_PRESSED:
                return "NK_MOUSE_BUTTON_PRESSED";
            case NK_MOUSE_BUTTON_RELEASED:
                return "NK_MOUSE_BUTTON_RELEASED";
            case NK_MOUSE_DOUBLE_CLICK:
                return "NK_MOUSE_DOUBLE_CLICK";
            case NK_MOUSE_WHEEL_VERTICAL:
                return "NK_MOUSE_WHEEL_VERTICAL";
            case NK_MOUSE_WHEEL_HORIZONTAL:
                return "NK_MOUSE_WHEEL_HORIZONTAL";
            case NK_MOUSE_ENTER:
                return "NK_MOUSE_ENTER";
            case NK_MOUSE_LEAVE:
                return "NK_MOUSE_LEAVE";
            case NK_MOUSE_WINDOW:
                return "NK_MOUSE_WINDOW";
            case NK_MOUSE_WINDOW_ENTER:
                return "NK_MOUSE_WINDOW_ENTER";
            case NK_MOUSE_WINDOW_LEAVE:
                return "NK_MOUSE_WINDOW_LEAVE";
            case NK_MOUSE_CAPTURE_BEGIN:
                return "NK_MOUSE_CAPTURE_BEGIN";
            case NK_MOUSE_CAPTURE_END:
                return "NK_MOUSE_CAPTURE_END";

            // -----------------------------------------------------------------
            // Groupe : Manette — Gamepads et contrôleurs
            // -----------------------------------------------------------------
            case NK_GAMEPAD_CONNECT:
                return "NK_GAMEPAD_CONNECT";
            case NK_GAMEPAD_DISCONNECT:
                return "NK_GAMEPAD_DISCONNECT";
            case NK_GAMEPAD_BUTTON_PRESSED:
                return "NK_GAMEPAD_BUTTON_PRESSED";
            case NK_GAMEPAD_BUTTON_RELEASED:
                return "NK_GAMEPAD_BUTTON_RELEASED";
            case NK_GAMEPAD_AXIS_MOTION:
                return "NK_GAMEPAD_AXIS_MOTION";
            case NK_GAMEPAD_RUMBLE:
                return "NK_GAMEPAD_RUMBLE";
            case NK_GAMEPAD_STICK:
                return "NK_GAMEPAD_STICK";
            case NK_GAMEPAD_TRIGGERED:
                return "NK_GAMEPAD_TRIGGERED";

            // -----------------------------------------------------------------
            // Groupe : HID générique — Périphériques non catégorisés
            // -----------------------------------------------------------------
            case NK_GENERIC_CONNECT:
                return "NK_GENERIC_CONNECT";
            case NK_GENERIC_DISCONNECT:
                return "NK_GENERIC_DISCONNECT";
            case NK_GENERIC_BUTTON_PRESSED:
                return "NK_GENERIC_BUTTON_PRESSED";
            case NK_GENERIC_BUTTON_RELEASED:
                return "NK_GENERIC_BUTTON_RELEASED";
            case NK_GENERIC_AXIS_MOTION:
                return "NK_GENERIC_AXIS_MOTION";
            case NK_GENERIC_RUMBLE:
                return "NK_GENERIC_RUMBLE";
            case NK_GENERIC_STICK:
                return "NK_GENERIC_STICK";
            case NK_GENERIC_TRIGGERED:
                return "NK_GENERIC_TRIGGERED";

            // -----------------------------------------------------------------
            // Groupe : Tactile / Gestures — Écrans tactiles et multi-touch
            // -----------------------------------------------------------------
            case NK_TOUCH_BEGIN:
                return "NK_TOUCH_BEGIN";
            case NK_TOUCH_MOVE:
                return "NK_TOUCH_MOVE";
            case NK_TOUCH_END:
                return "NK_TOUCH_END";
            case NK_TOUCH_CANCEL:
                return "NK_TOUCH_CANCEL";
            case NK_TOUCH_PRESSED:
                return "NK_TOUCH_PRESSED";
            case NK_TOUCH_RELEASED:
                return "NK_TOUCH_RELEASED";
            case NK_GESTURE_PINCH:
                return "NK_GESTURE_PINCH";
            case NK_GESTURE_ROTATE:
                return "NK_GESTURE_ROTATE";
            case NK_GESTURE_PAN:
                return "NK_GESTURE_PAN";
            case NK_GESTURE_SWIPE:
                return "NK_GESTURE_SWIPE";
            case NK_GESTURE_TAP:
                return "NK_GESTURE_TAP";
            case NK_GESTURE_LONG_PRESS:
                return "NK_GESTURE_LONG_PRESS";

            // -----------------------------------------------------------------
            // Groupe : Drag & Drop — Transfert interactif de données
            // -----------------------------------------------------------------
            case NK_DROP_FILE:
                return "NK_DROP_FILE";
            case NK_DROP_TEXT:
                return "NK_DROP_TEXT";
            case NK_DROP_IMAGE:
                return "NK_DROP_IMAGE";
            case NK_DROP_ENTER:
                return "NK_DROP_ENTER";
            case NK_DROP_OVER:
                return "NK_DROP_OVER";
            case NK_DROP_LEAVE:
                return "NK_DROP_LEAVE";

            // -----------------------------------------------------------------
            // Groupe : Système — Événements OS et environnement
            // -----------------------------------------------------------------
            case NK_SYSTEM_POWER:
                return "NK_SYSTEM_POWER";
            case NK_SYSTEM_LOCALE:
                return "NK_SYSTEM_LOCALE";
            case NK_SYSTEM_DISPLAY:
                return "NK_SYSTEM_DISPLAY";
            case NK_SYSTEM_MEMORY:
                return "NK_SYSTEM_MEMORY";

            // -----------------------------------------------------------------
            // Groupe : Transfert et Personnalisé
            // -----------------------------------------------------------------
            case NK_TRANSFER:
                return "NK_TRANSFER";
            case NK_CUSTOM:
                return "NK_CUSTOM";

            // -----------------------------------------------------------------
            // Fallback : valeur hors plage ou non définie
            // -----------------------------------------------------------------
            default:
                return "NK_UNKNOWN";
        }
    }

    /// @brief Parse une chaîne pour retrouver le type d'événement correspondant
    /// @param str La chaîne à parser (comparaison case-sensitive exacte)
    /// @return Valeur NkEventType::Value correspondante, ou NK_NONE si inconnue
    /// @note Implémentation naïve par série de if : optimisable par hash map si besoin
    /// @note Ne couvre pas tous les types : ajouter les nouveaux types lors de l'extension
    NkEventType::Value NkEventType::FromString(const NkString& str) {
        // Série de comparaisons séquentielles pour chaque type défini
        // Note : Cette implémentation est linéaire O(n) — suffisante pour le débogage
        // Pour un parsing fréquent en production, envisager une table de hachage

        // Groupe : Application
        if (str == "NK_APP_LAUNCH") {
            return NK_APP_LAUNCH;
        }
        if (str == "NK_APP_TICK") {
            return NK_APP_TICK;
        }
        if (str == "NK_APP_UPDATE") {
            return NK_APP_UPDATE;
        }
        if (str == "NK_APP_RENDER") {
            return NK_APP_RENDER;
        }
        if (str == "NK_APP_CLOSE") {
            return NK_APP_CLOSE;
        }

        // Groupe : Fenêtre
        if (str == "NK_WINDOW_CREATE") {
            return NK_WINDOW_CREATE;
        }
        if (str == "NK_WINDOW_CLOSE") {
            return NK_WINDOW_CLOSE;
        }
        if (str == "NK_WINDOW_DESTROY") {
            return NK_WINDOW_DESTROY;
        }
        if (str == "NK_WINDOW_PAINT") {
            return NK_WINDOW_PAINT;
        }
        if (str == "NK_WINDOW_RESIZE") {
            return NK_WINDOW_RESIZE;
        }
        if (str == "NK_WINDOW_RESIZE_BEGIN") {
            return NK_WINDOW_RESIZE_BEGIN;
        }
        if (str == "NK_WINDOW_RESIZE_END") {
            return NK_WINDOW_RESIZE_END;
        }
        if (str == "NK_WINDOW_MOVE") {
            return NK_WINDOW_MOVE;
        }
        if (str == "NK_WINDOW_MOVE_BEGIN") {
            return NK_WINDOW_MOVE_BEGIN;
        }
        if (str == "NK_WINDOW_MOVE_END") {
            return NK_WINDOW_MOVE_END;
        }
        if (str == "NK_WINDOW_FOCUS_GAINED") {
            return NK_WINDOW_FOCUS_GAINED;
        }
        if (str == "NK_WINDOW_FOCUS_LOST") {
            return NK_WINDOW_FOCUS_LOST;
        }
        if (str == "NK_WINDOW_MINIMIZE") {
            return NK_WINDOW_MINIMIZE;
        }
        if (str == "NK_WINDOW_MAXIMIZE") {
            return NK_WINDOW_MAXIMIZE;
        }
        if (str == "NK_WINDOW_RESTORE") {
            return NK_WINDOW_RESTORE;
        }
        if (str == "NK_WINDOW_FULLSCREEN") {
            return NK_WINDOW_FULLSCREEN;
        }
        if (str == "NK_WINDOW_WINDOWED") {
            return NK_WINDOW_WINDOWED;
        }
        if (str == "NK_WINDOW_DPI_CHANGE") {
            return NK_WINDOW_DPI_CHANGE;
        }
        if (str == "NK_WINDOW_THEME_CHANGE") {
            return NK_WINDOW_THEME_CHANGE;
        }
        if (str == "NK_WINDOW_SHOWN") {
            return NK_WINDOW_SHOWN;
        }
        if (str == "NK_WINDOW_HIDDEN") {
            return NK_WINDOW_HIDDEN;
        }

        // Groupe : Clavier
        if (str == "NK_KEY_PRESSED") {
            return NK_KEY_PRESSED;
        }
        if (str == "NK_KEY_REPEATED") {
            return NK_KEY_REPEATED;
        }
        if (str == "NK_KEY_RELEASED") {
            return NK_KEY_RELEASED;
        }
        if (str == "NK_TEXT_INPUT") {
            return NK_TEXT_INPUT;
        }
        if (str == "NK_CHAR_ENTERED") {
            return NK_CHAR_ENTERED;
        }

        // Groupe : Souris
        if (str == "NK_MOUSE_MOVE") {
            return NK_MOUSE_MOVE;
        }
        if (str == "NK_MOUSE_RAW") {
            return NK_MOUSE_RAW;
        }
        if (str == "NK_MOUSE_BUTTON_PRESSED") {
            return NK_MOUSE_BUTTON_PRESSED;
        }
        if (str == "NK_MOUSE_BUTTON_RELEASED") {
            return NK_MOUSE_BUTTON_RELEASED;
        }
        if (str == "NK_MOUSE_DOUBLE_CLICK") {
            return NK_MOUSE_DOUBLE_CLICK;
        }
        if (str == "NK_MOUSE_WHEEL_VERTICAL") {
            return NK_MOUSE_WHEEL_VERTICAL;
        }
        if (str == "NK_MOUSE_WHEEL_HORIZONTAL") {
            return NK_MOUSE_WHEEL_HORIZONTAL;
        }
        if (str == "NK_MOUSE_ENTER") {
            return NK_MOUSE_ENTER;
        }
        if (str == "NK_MOUSE_LEAVE") {
            return NK_MOUSE_LEAVE;
        }
        if (str == "NK_MOUSE_WINDOW") {
            return NK_MOUSE_WINDOW;
        }
        if (str == "NK_MOUSE_WINDOW_ENTER") {
            return NK_MOUSE_WINDOW_ENTER;
        }
        if (str == "NK_MOUSE_WINDOW_LEAVE") {
            return NK_MOUSE_WINDOW_LEAVE;
        }
        if (str == "NK_MOUSE_CAPTURE_BEGIN") {
            return NK_MOUSE_CAPTURE_BEGIN;
        }
        if (str == "NK_MOUSE_CAPTURE_END") {
            return NK_MOUSE_CAPTURE_END;
        }

        // Groupe : Transfert et Personnalisé (liste partielle — à étendre si besoin)
        if (str == "NK_TRANSFER") {
            return NK_TRANSFER;
        }
        if (str == "NK_CUSTOM") {
            return NK_CUSTOM;
        }

        // Fallback : retourne NK_NONE pour toute chaîne non reconnue
        return NK_NONE;
    }

    // =====================================================================
    // NkEvent — Implémentation des méthodes de base
    // =====================================================================

    /// @brief Retourne le timestamp courant en millisecondes depuis l'init système
    /// @return NkTimestampMs représentant le temps écoulé en ms
    /// @note Utilise NkChrono::Now() pour cohérence avec le reste du framework
    /// @note Conversion explicite pour garantir le type uint64 attendu
    NkTimestampMs NkEvent::GetCurrentTimestamp() {
        return static_cast<NkTimestampMs>(NkChrono::Now().milliseconds);
    }

    /// @brief Constructeur paramétré avec identifiant de fenêtre source
    /// @param windowId Identifiant unique de la fenêtre émettrice (0 si global)
    /// @note Initialise le timestamp automatiquement à la construction
    /// @note mHandled est initialisé à false par défaut (événement non consommé)
    NkEvent::NkEvent(uint64 windowId)
        : mWindowID(windowId)
        , mTimestamp(GetCurrentTimestamp())
        , mHandled(false) {
    }

    /// @brief Constructeur par défaut pour événement global (sans fenêtre)
    /// @note windowId = 0 par convention pour les événements système globaux
    /// @note Timestamp capturé au moment de la construction pour cohérence temporelle
    NkEvent::NkEvent()
        : mWindowID(0)
        , mTimestamp(GetCurrentTimestamp())
        , mHandled(false) {
    }

    /// @brief Constructeur de copie — copie profonde des membres de base
    /// @param other Référence constante vers l'événement source à dupliquer
    /// @note Copie les trois membres : windowId, timestamp, handled
    /// @note Les données spécifiques aux dérivés sont copiées via leur propre copy-ctor
    NkEvent::NkEvent(const NkEvent& other)
        : mWindowID(other.mWindowID)
        , mTimestamp(other.mTimestamp)
        , mHandled(other.mHandled) {
    }

    // -------------------------------------------------------------------------
    // Implémentations virtuelles par défaut — à surcharger dans les dérivés
    // -------------------------------------------------------------------------

    /// @brief Retourne le type de l'événement (implémentation par défaut)
    /// @return NkEventType::NK_NONE pour la classe de base
    /// @note Les classes dérivées DOIVENT surcharger cette méthode via NK_EVENT_TYPE_FLAGS
    NkEventType::Value NkEvent::GetType() const {
        return NkEventType::NK_NONE;
    }

    /// @brief Retourne le nom de classe C++ de l'événement (implémentation par défaut)
    /// @return Littéral "NkEvent" pour la classe de base
    /// @note Utile pour le débogage quand le type exact n'est pas disponible
    const char* NkEvent::GetName() const {
        return "NkEvent";
    }

    /// @brief Retourne la représentation string du type (implémentation par défaut)
    /// @return Littéral "NK_NONE" pour la classe de base
    /// @note Délègue à NkEventType::ToString() dans les implémentations dérivées
    const char* NkEvent::GetTypeStr() const {
        return "NK_NONE";
    }

    /// @brief Retourne les flags de catégorie (implémentation par défaut)
    /// @return NK_CAT_NONE pour la classe de base (aucune catégorie)
    /// @note Les classes dérivées DOIVENT surcharger via NK_EVENT_CATEGORY_FLAGS
    uint32 NkEvent::GetCategoryFlags() const {
        return static_cast<uint32>(NkEventCategory::NK_CAT_NONE);
    }

    /// @brief Crée une copie polymorphe de l'événement (implémentation par défaut)
    /// @return Pointeur brut vers un nouveau NkEvent (caller responsable du delete)
    /// @note Pattern Prototype : les dérivés doivent retourner new Derived(*this)
    NkEvent* NkEvent::Clone() const {
        return new NkEvent(*this);
    }

    /// @brief Génère une représentation lisible de l'événement pour logs et débogage
    /// @return NkString formaté contenant type, catégorie, timestamp, windowId et état
    /// @note Format : [NkEvent type=... cat=... ts=... window=... handled=...]
    /// @note Utilise snprintf pour formatage numérique sécurisé sans allocation temporaire
    NkString NkEvent::ToString() const {
        // Buffers temporaires pour formatage des valeurs numériques
        // Taille généreuse pour éviter tout overflow (uint64 max = 20 chiffres + null)
        char tsBuffer[32];
        char windowBuffer[32];

        // Formatage sécurisé du timestamp en millisecondes (uint64 → string)
        ::snprintf(
            tsBuffer,
            sizeof(tsBuffer),
            "%llu",
            static_cast<unsigned long long>(mTimestamp)
        );

        // Formatage sécurisé de l'identifiant de fenêtre (uint64 → string)
        ::snprintf(
            windowBuffer,
            sizeof(windowBuffer),
            "%llu",
            static_cast<unsigned long long>(mWindowID)
        );

        // Construction progressive de la chaîne de sortie via NkString
        // Évite les allocations multiples grâce à la capacité pré-allouée de NkString
        NkString out("[NkEvent type=");
        out.Append(GetTypeStr() ? GetTypeStr() : "NK_NONE");
        out.Append(" cat=");
        out.Append(NkEventCategory::ToString(GetCategory()));
        out.Append(" ts=");
        out.Append(tsBuffer);
        out.Append(" window=");
        out.Append(windowBuffer);
        out.Append(" handled=");
        out.Append(mHandled ? "true" : "false");
        out.Append("]");

        // Retourne la chaîne complète formatée pour affichage ou logging
        return out;
    }

    /// @brief Wrapper statique vers NkEventType::ToString pour commodité d'usage
    /// @param type Valeur NkEventType::Value à convertir en chaîne
    /// @return NkString contenant le nom symbolique du type
    /// @note Délègue directement à NkEventType::ToString() — aucun traitement supplémentaire
    NkString NkEvent::TypeToString(NkEventType::Value type) {
        return NkEventType::ToString(type);
    }

    /// @brief Wrapper statique vers NkEventCategory::ToString pour commodité d'usage
    /// @param category Valeur NkEventCategory::Value à convertir en chaîne
    /// @return NkString contenant le nom symbolique de la catégorie
    /// @note Délègue directement à NkEventCategory::ToString() — aucun traitement supplémentaire
    NkString NkEvent::CategoryToString(NkEventCategory::Value category) {
        return NkEventCategory::ToString(category);
    }

} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION ET NOTES D'IMPLÉMENTATION
// =============================================================================
// Ce fichier contient l'implémentation des utilitaires de sérialisation et
// des méthodes de base de NkEvent. Les exemples ci-dessous illustrent l'usage.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Sérialisation/désérialisation des catégories d'événements
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkEvent.h"

    void DebugEventCategory(nkentseu::NkEventCategory::Value category) {
        using namespace nkentseu;

        // Conversion vers chaîne pour affichage
        NkString catStr = NkEventCategory::ToString(category);
        NK_FOUNDATION_LOG_INFO("Event category: {}", catStr);

        // Parsing inverse (attention : ne gère pas les combinaisons "A|B")
        auto parsed = NkEventCategory::FromString("INPUT");
        if (parsed != NK_CAT_NONE) {
            NK_FOUNDATION_LOG_DEBUG("Parsed category flag: {}", parsed);
        }

        // Test de présence de flag dans un set combiné
        auto filter = NK_CAT_INPUT | NK_CAT_KEYBOARD;
        if (NkCategoryHas(category, NK_CAT_KEYBOARD)) {
            NK_FOUNDATION_LOG_TRACE("Keyboard-related event detected");
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Sérialisation des types d'événements pour logging
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkEvent.h"

    void LogEventType(const nkentseu::NkEvent& event) {
        using namespace nkentseu;

        // Méthode 1 : Via la méthode d'instance de NkEvent
        NkString typeStr = event.GetTypeStr();  // "NK_KEY_PRESSED"

        // Méthode 2 : Via le wrapper statique
        NkString typeStr2 = NkEvent::TypeToString(event.GetType());

        // Méthode 3 : Via ToString() complet pour débogage détaillé
        NkString fullDebug = event.ToString();
        // Résultat : "[NkEvent type=NK_KEY_PRESSED cat=INPUT|KEYBOARD ts=12345 window=1 handled=false]"

        NK_FOUNDATION_LOG_DEBUG("Event: {} | Full: {}", typeStr, fullDebug);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Utilisation du timestamp pour profiling événementiel
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkEvent.h"
    #include "NKTime/NkChrono.h"

    class EventLatencyTracker {
    public:
        void OnEventReceived(const nkentseu::NkEvent& event) {
            using namespace nkentseu;

            // Calcul de la latence entre génération et réception
            NkTimestampMs now = static_cast<NkTimestampMs>(NkChrono::Now().milliseconds);
            NkTimestampMs latency = now - event.GetTimestamp();

            // Alerting si latence excessive (>100ms)
            if (latency > 100) {
                NK_FOUNDATION_LOG_WARNING(
                    "High event latency: {}ms for type={}",
                    latency,
                    event.GetTypeStr()
                );
            }

            // Statistiques agrégées par type d'événement
            mLatencyStats[event.GetType()] += latency;
            mEventCounts[event.GetType()]++;
        }

        void ReportStats() {
            for (const auto& [type, totalLatency] : mLatencyStats) {
                uint64 count = mEventCounts[type];
                float avg = count > 0 ? static_cast<float>(totalLatency) / count : 0.f;
                NK_FOUNDATION_LOG_INFO(
                    "Type {}: {} events, avg latency {:.2f}ms",
                    NkEvent::TypeToString(type),
                    count,
                    avg
                );
            }
        }

    private:
        nkentseu::NkUnorderedMap<nkentseu::NkEventType::Value, uint64> mLatencyStats;
        nkentseu::NkUnorderedMap<nkentseu::NkEventType::Value, uint64> mEventCounts;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Pattern Clone() pour replay d'événements ou tests
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkEvent.h"
    #include <memory>

    class EventRecorder {
    public:
        // Enregistrement d'un événement pour replay ultérieur
        void Record(const nkentseu::NkEvent& event) {
            // Clone polymorphe pour copie profonde du type exact
            std::unique_ptr<nkentseu::NkEvent> copy(event.Clone());

            // Reset de l'état "handled" pour permettre un re-dispatch propre
            copy->Unmark();

            // Stockage pour replay différé
            mRecordedEvents.push_back(std::move(copy));
        }

        // Replay de tous les événements enregistrés
        void Replay(nkentseu::event::EventManager& dispatcher) {
            for (const auto& eventPtr : mRecordedEvents) {
                if (eventPtr && eventPtr->IsValid()) {
                    dispatcher.Dispatch(*eventPtr);
                }
            }
        }

        // Clear de l'historique pour libérer la mémoire
        void Clear() {
            mRecordedEvents.clear();
        }

    private:
        std::vector<std::unique_ptr<nkentseu::NkEvent>> mRecordedEvents;
    };

    // Usage :
    // EventRecorder recorder;
    // recorder.Record(keyPressEvent);  // Copie profonde via Clone()
    // recorder.Replay(eventManager);   // Re-dispatch pour tests ou démo
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Extension de NkEvent avec un type personnalisé
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkEvent.h"

    // Déclaration d'un événement personnalisé dans un module métier
    class NkPlayerJumpEvent final : public nkentseu::NkEvent {
    public:
        // Macros pour éviter la duplication de code boilerplate
        NK_EVENT_TYPE_FLAGS(NK_CUSTOM)  // Ou définir un nouveau type dans NkEventType
        NK_EVENT_CATEGORY_FLAGS(
            nkentseu::NkEventCategory::NK_CAT_INPUT |
            nkentseu::NkEventCategory::NK_CAT_CUSTOM
        )

        // Constructeur avec paramètres métier
        NkPlayerJumpEvent(uint64 windowId, float jumpHeight, uint32 playerId)
            : NkEvent(windowId)
            , mJumpHeight(jumpHeight)
            , mPlayerId(playerId)
        {
        }

        // Surcharge de Clone() pour copie polymorphe correcte
        nkentseu::NkEvent* Clone() const override {
            return new NkPlayerJumpEvent(*this);
        }

        // Surcharge de ToString() pour débogage significatif
        nkentseu::NkString ToString() const override {
            return nkentseu::NkFormat(
                "NkPlayerJumpEvent[playerId={} height={:.2f} window={}]",
                mPlayerId,
                mJumpHeight,
                GetWindowId()
            );
        }

        // Accesseurs métier
        float GetJumpHeight() const noexcept { return mJumpHeight; }
        uint32 GetPlayerId() const noexcept { return mPlayerId; }

    private:
        float mJumpHeight;
        uint32 mPlayerId;
    };

    // Utilisation :
    // NkPlayerJumpEvent jumpEvt(1, 2.5f, 42);
    // if (jumpEvt.Is<NkPlayerJumpEvent>()) { ... }  // RTTI léger
    // auto* ptr = jumpEvt.As<NkPlayerJumpEvent>();  // Casting type-safe
*/

// =============================================================================
// NOTES DE MAINTENANCE ET OPTIMISATIONS
// =============================================================================
/*
    1. PERFORMANCE DE ToString() / FromString() :
       - NkEventType::ToString() utilise un switch : optimisé en table de saut par le compilateur
       - NkEventType::FromString() utilise des if séquentiels : O(n) mais acceptable pour le debug
       - Pour un parsing fréquent en production, envisager une std::unordered_map<string, Value>

    2. GESTION DES COMBINAISONS DE FLAGS :
       - NkEventCategory::ToString() gère les combinaisons via '|' (ex: "INPUT|KEYBOARD")
       - NkEventCategory::FromString() ne parse PAS les combinaisons : conception intentionnelle
       - Pour parser "A|B", implémenter un splitter en couche supérieure si nécessaire

    3. SÉCURITÉ DES BUFFERS TEMPORAIRES :
       - snprintf utilisé avec sizeof(buffer) pour éviter tout overflow
       - Buffers de taille 32 : suffisants pour uint64 (max 20 chiffres) + null terminator
       - Alternative moderne : utiliser NkFormat() si disponible pour plus de type-safety

    4. EXTENSIBILITÉ DES ENUMS :
       - Lors de l'ajout d'un nouveau NkEventType::Value :
         a) Ajouter le case dans ToString() (switch exhaustif — le compilateur avertira si oublié)
         b) Ajouter le if dans FromString() si le parsing inverse est requis
         c) Mettre à jour la documentation Doxygen de l'enum dans NkEvent.h

    5. COMPATIBILITÉ MULTIPLATEFORME :
       - snprintf est disponible sur toutes les plateformes cibles (C99+)
       - Pour MSVC ancien, utiliser _snprintf_s avec gestion de retour spécifique si nécessaire
       - NkChrono::Now() abstrait les différences d'API temps entre OS

    6. MÉMOIRE ET ALLOCATIONS :
       - ToString() alloue via NkString : éviter d'appeler dans des boucles critiques
       - Pour du logging haute fréquence, envisager un buffer pré-alloué ou logging asynchrone
       - Clone() retourne un pointeur brut : toujours envelopper dans std::unique_ptr en appelant
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================