#include "pch.h"
// =============================================================================
// NKEvent/NkKeyboardEvent.cpp
// Implémentation des utilitaires clavier cross-platform.
//
// Contenu :
//   - NkKeyToString, NkKeyIsModifier, NkKeyIsNumpad, NkKeyIsFunctionKey
//   - NkScancodeToString, NkScancodeToKey
//   - Conversions plateforme → USB HID : Win32, Linux, X11, macOS, Web DOM
//
// Design :
//   - Tables de mapping statiques pour performance et maintenance centralisée
//   - Switch exhaustifs optimisés par le compilateur (table de saut)
//   - Fonctions noexcept pour usage dans les hot paths de l'input
//   - Documentation des sources de mapping (specs USB HID, Win32, evdev...)
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "NKEvent/NkKeyboardEvent.h"
#include <cstring>  // Pour std::strcmp dans les recherches de table

namespace nkentseu {

    // =====================================================================
    // NkKeyToString — Conversion NkKey → chaîne lisible pour débogage
    // =====================================================================

    /// @brief Convertit une valeur NkKey en son nom symbolique pour le débogage
    /// @param key La valeur de touche à convertir
    /// @return Pointeur vers une chaîne littérale statique (ex: "NK_A", "NK_ENTER")
    /// @note Fonction noexcept pour usage dans les contextes critiques
    /// @note Ne pas libérer la chaîne retournée — mémoire statique en lecture seule
    /// @note Retourne "NK_UNKNOWN" pour toute valeur non reconnue ou hors plage
    const char* NkKeyToString(NkKey key) noexcept {
        // Dispatch par switch exhaustif — optimisé en table de saut par le compilateur
        // Les cas sont groupés logiquement pour la maintenance
        switch (key) {
            // -------------------------------------------------------------
            // Valeur inconnue et touches de fonction F1–F24
            // -------------------------------------------------------------
            case NkKey::NK_UNKNOWN:
                return "NK_UNKNOWN";
            case NkKey::NK_ESCAPE:
                return "NK_ESCAPE";
            case NkKey::NK_F1:
                return "NK_F1";
            case NkKey::NK_F2:
                return "NK_F2";
            case NkKey::NK_F3:
                return "NK_F3";
            case NkKey::NK_F4:
                return "NK_F4";
            case NkKey::NK_F5:
                return "NK_F5";
            case NkKey::NK_F6:
                return "NK_F6";
            case NkKey::NK_F7:
                return "NK_F7";
            case NkKey::NK_F8:
                return "NK_F8";
            case NkKey::NK_F9:
                return "NK_F9";
            case NkKey::NK_F10:
                return "NK_F10";
            case NkKey::NK_F11:
                return "NK_F11";
            case NkKey::NK_F12:
                return "NK_F12";
            case NkKey::NK_F13:
                return "NK_F13";
            case NkKey::NK_F14:
                return "NK_F14";
            case NkKey::NK_F15:
                return "NK_F15";
            case NkKey::NK_F16:
                return "NK_F16";
            case NkKey::NK_F17:
                return "NK_F17";
            case NkKey::NK_F18:
                return "NK_F18";
            case NkKey::NK_F19:
                return "NK_F19";
            case NkKey::NK_F20:
                return "NK_F20";
            case NkKey::NK_F21:
                return "NK_F21";
            case NkKey::NK_F22:
                return "NK_F22";
            case NkKey::NK_F23:
                return "NK_F23";
            case NkKey::NK_F24:
                return "NK_F24";

            // -------------------------------------------------------------
            // Ligne supérieure : chiffres et symboles
            // -------------------------------------------------------------
            case NkKey::NK_GRAVE:
                return "NK_GRAVE";
            case NkKey::NK_NUM1:
                return "NK_NUM1";
            case NkKey::NK_NUM2:
                return "NK_NUM2";
            case NkKey::NK_NUM3:
                return "NK_NUM3";
            case NkKey::NK_NUM4:
                return "NK_NUM4";
            case NkKey::NK_NUM5:
                return "NK_NUM5";
            case NkKey::NK_NUM6:
                return "NK_NUM6";
            case NkKey::NK_NUM7:
                return "NK_NUM7";
            case NkKey::NK_NUM8:
                return "NK_NUM8";
            case NkKey::NK_NUM9:
                return "NK_NUM9";
            case NkKey::NK_NUM0:
                return "NK_NUM0";
            case NkKey::NK_MINUS:
                return "NK_MINUS";
            case NkKey::NK_EQUALS:
                return "NK_EQUALS";
            case NkKey::NK_BACK:
                return "NK_BACK";

            // -------------------------------------------------------------
            // Rangée QWERTY (première rangée alphabétique)
            // -------------------------------------------------------------
            case NkKey::NK_TAB:
                return "NK_TAB";
            case NkKey::NK_Q:
                return "NK_Q";
            case NkKey::NK_W:
                return "NK_W";
            case NkKey::NK_E:
                return "NK_E";
            case NkKey::NK_R:
                return "NK_R";
            case NkKey::NK_T:
                return "NK_T";
            case NkKey::NK_Y:
                return "NK_Y";
            case NkKey::NK_U:
                return "NK_U";
            case NkKey::NK_I:
                return "NK_I";
            case NkKey::NK_O:
                return "NK_O";
            case NkKey::NK_P:
                return "NK_P";
            case NkKey::NK_LBRACKET:
                return "NK_LBRACKET";
            case NkKey::NK_RBRACKET:
                return "NK_RBRACKET";
            case NkKey::NK_BACKSLASH:
                return "NK_BACKSLASH";

            // -------------------------------------------------------------
            // Rangée ASDF (deuxième rangée alphabétique)
            // -------------------------------------------------------------
            case NkKey::NK_CAPSLOCK:
                return "NK_CAPSLOCK";
            case NkKey::NK_A:
                return "NK_A";
            case NkKey::NK_S:
                return "NK_S";
            case NkKey::NK_D:
                return "NK_D";
            case NkKey::NK_F:
                return "NK_F";
            case NkKey::NK_G:
                return "NK_G";
            case NkKey::NK_H:
                return "NK_H";
            case NkKey::NK_J:
                return "NK_J";
            case NkKey::NK_K:
                return "NK_K";
            case NkKey::NK_L:
                return "NK_L";
            case NkKey::NK_SEMICOLON:
                return "NK_SEMICOLON";
            case NkKey::NK_APOSTROPHE:
                return "NK_APOSTROPHE";
            case NkKey::NK_ENTER:
                return "NK_ENTER";

            // -------------------------------------------------------------
            // Rangée ZXCV (troisième rangée alphabétique) + modificateurs
            // -------------------------------------------------------------
            case NkKey::NK_LSHIFT:
                return "NK_LSHIFT";
            case NkKey::NK_Z:
                return "NK_Z";
            case NkKey::NK_X:
                return "NK_X";
            case NkKey::NK_C:
                return "NK_C";
            case NkKey::NK_V:
                return "NK_V";
            case NkKey::NK_B:
                return "NK_B";
            case NkKey::NK_N:
                return "NK_N";
            case NkKey::NK_M:
                return "NK_M";
            case NkKey::NK_COMMA:
                return "NK_COMMA";
            case NkKey::NK_PERIOD:
                return "NK_PERIOD";
            case NkKey::NK_SLASH:
                return "NK_SLASH";
            case NkKey::NK_RSHIFT:
                return "NK_RSHIFT";

            // -------------------------------------------------------------
            // Rangée inférieure : modificateurs et espace
            // -------------------------------------------------------------
            case NkKey::NK_LCTRL:
                return "NK_LCTRL";
            case NkKey::NK_LSUPER:
                return "NK_LSUPER";
            case NkKey::NK_LALT:
                return "NK_LALT";
            case NkKey::NK_SPACE:
                return "NK_SPACE";
            case NkKey::NK_RALT:
                return "NK_RALT";
            case NkKey::NK_RSUPER:
                return "NK_RSUPER";
            case NkKey::NK_MENU:
                return "NK_MENU";
            case NkKey::NK_RCTRL:
                return "NK_RCTRL";

            // -------------------------------------------------------------
            // Bloc navigation et édition
            // -------------------------------------------------------------
            case NkKey::NK_PRINT_SCREEN:
                return "NK_PRINT_SCREEN";
            case NkKey::NK_SCROLL_LOCK:
                return "NK_SCROLL_LOCK";
            case NkKey::NK_PAUSE_BREAK:
                return "NK_PAUSE_BREAK";
            case NkKey::NK_INSERT:
                return "NK_INSERT";
            case NkKey::NK_DELETE:
                return "NK_DELETE";
            case NkKey::NK_HOME:
                return "NK_HOME";
            case NkKey::NK_END:
                return "NK_END";
            case NkKey::NK_PAGE_UP:
                return "NK_PAGE_UP";
            case NkKey::NK_PAGE_DOWN:
                return "NK_PAGE_DOWN";

            // -------------------------------------------------------------
            // Flèches directionnelles
            // -------------------------------------------------------------
            case NkKey::NK_UP:
                return "NK_UP";
            case NkKey::NK_DOWN:
                return "NK_DOWN";
            case NkKey::NK_LEFT:
                return "NK_LEFT";
            case NkKey::NK_RIGHT:
                return "NK_RIGHT";

            // -------------------------------------------------------------
            // Pavé numérique (Numpad)
            // -------------------------------------------------------------
            case NkKey::NK_NUM_LOCK:
                return "NK_NUM_LOCK";
            case NkKey::NK_NUMPAD_DIV:
                return "NK_NUMPAD_DIV";
            case NkKey::NK_NUMPAD_MUL:
                return "NK_NUMPAD_MUL";
            case NkKey::NK_NUMPAD_SUB:
                return "NK_NUMPAD_SUB";
            case NkKey::NK_NUMPAD_ADD:
                return "NK_NUMPAD_ADD";
            case NkKey::NK_NUMPAD_ENTER:
                return "NK_NUMPAD_ENTER";
            case NkKey::NK_NUMPAD_DOT:
                return "NK_NUMPAD_DOT";
            case NkKey::NK_NUMPAD_0:
                return "NK_NUMPAD_0";
            case NkKey::NK_NUMPAD_1:
                return "NK_NUMPAD_1";
            case NkKey::NK_NUMPAD_2:
                return "NK_NUMPAD_2";
            case NkKey::NK_NUMPAD_3:
                return "NK_NUMPAD_3";
            case NkKey::NK_NUMPAD_4:
                return "NK_NUMPAD_4";
            case NkKey::NK_NUMPAD_5:
                return "NK_NUMPAD_5";
            case NkKey::NK_NUMPAD_6:
                return "NK_NUMPAD_6";
            case NkKey::NK_NUMPAD_7:
                return "NK_NUMPAD_7";
            case NkKey::NK_NUMPAD_8:
                return "NK_NUMPAD_8";
            case NkKey::NK_NUMPAD_9:
                return "NK_NUMPAD_9";
            case NkKey::NK_NUMPAD_EQUALS:
                return "NK_NUMPAD_EQUALS";

            // -------------------------------------------------------------
            // Touches multimédia
            // -------------------------------------------------------------
            case NkKey::NK_MEDIA_PLAY_PAUSE:
                return "NK_MEDIA_PLAY_PAUSE";
            case NkKey::NK_MEDIA_STOP:
                return "NK_MEDIA_STOP";
            case NkKey::NK_MEDIA_NEXT:
                return "NK_MEDIA_NEXT";
            case NkKey::NK_MEDIA_PREV:
                return "NK_MEDIA_PREV";
            case NkKey::NK_MEDIA_VOLUME_UP:
                return "NK_MEDIA_VOLUME_UP";
            case NkKey::NK_MEDIA_VOLUME_DOWN:
                return "NK_MEDIA_VOLUME_DOWN";
            case NkKey::NK_MEDIA_MUTE:
                return "NK_MEDIA_MUTE";

            // -------------------------------------------------------------
            // Touches navigateur web
            // -------------------------------------------------------------
            case NkKey::NK_BROWSER_BACK:
                return "NK_BROWSER_BACK";
            case NkKey::NK_BROWSER_FORWARD:
                return "NK_BROWSER_FORWARD";
            case NkKey::NK_BROWSER_REFRESH:
                return "NK_BROWSER_REFRESH";
            case NkKey::NK_BROWSER_HOME:
                return "NK_BROWSER_HOME";
            case NkKey::NK_BROWSER_SEARCH:
                return "NK_BROWSER_SEARCH";
            case NkKey::NK_BROWSER_FAVORITES:
                return "NK_BROWSER_FAVORITES";

            // -------------------------------------------------------------
            // Divers / OEM / Réservé
            // -------------------------------------------------------------
            case NkKey::NK_SLEEP:
                return "NK_SLEEP";
            case NkKey::NK_CLEAR:
                return "NK_CLEAR";
            case NkKey::NK_SEPARATOR:
                return "NK_SEPARATOR";

            // -------------------------------------------------------------
            // Fallback : valeur hors plage ou non définie
            // -------------------------------------------------------------
            default:
                return "NK_UNKNOWN";
        }
    }

    // =====================================================================
    // Utilitaires de classification pour NkKey
    // =====================================================================

    /// @brief Vérifie si une touche est un modificateur principal (Ctrl/Alt/Shift/Super)
    /// @param key La touche à tester
    /// @return true si key est l'un des : LCTRL, RCTRL, LALT, RALT, LSHIFT, RSHIFT, LSUPER, RSUPER
    /// @note AltGr est considéré séparément via NkModifierState::altGr
    /// @note Fonction noexcept pour usage dans les hot paths de l'input
    bool NkKeyIsModifier(NkKey key) noexcept {
        // Switch compact pour les 8 modificateurs principaux
        switch (key) {
            case NkKey::NK_LCTRL:
            case NkKey::NK_RCTRL:
            case NkKey::NK_LSHIFT:
            case NkKey::NK_RSHIFT:
            case NkKey::NK_LALT:
            case NkKey::NK_RALT:
            case NkKey::NK_LSUPER:
            case NkKey::NK_RSUPER:
                return true;
            default:
                return false;
        }
    }

    /// @brief Vérifie si une touche appartient au pavé numérique
    /// @param key La touche à tester
    /// @return true si key est dans la plage NK_NUM_LOCK à NK_NUMPAD_EQUALS (inclus)
    /// @note Cette vérification par plage suppose que les NkKey::NK_NUMPAD_*
    ///       sont consécutifs dans l'enum — garantie par la définition dans le header
    /// @note Fonction noexcept pour usage dans les hot paths de l'input
    bool NkKeyIsNumpad(NkKey key) noexcept {
        return (key >= NkKey::NK_NUM_LOCK) && (key <= NkKey::NK_NUMPAD_EQUALS);
    }

    /// @brief Vérifie si une touche est une touche de fonction (F1–F24)
    /// @param key La touche à tester
    /// @return true si key est dans la plage NK_F1 à NK_F24 (inclus)
    /// @note Cette vérification par plage suppose que les NkKey::NK_F*
    ///       sont consécutifs dans l'enum — garantie par la définition dans le header
    /// @note Fonction noexcept pour usage dans les hot paths de l'input
    bool NkKeyIsFunctionKey(NkKey key) noexcept {
        return (key >= NkKey::NK_F1) && (key <= NkKey::NK_F24);
    }

    // =====================================================================
    // NkScancodeToString — Conversion NkScancode → chaîne lisible
    // =====================================================================

    /// @brief Convertit un scancode USB HID en son nom symbolique pour le débogage
    /// @param sc Le scancode à convertir
    /// @return Pointeur vers une chaîne littérale statique (ex: "SC_A", "SC_ENTER")
    /// @note Fonction noexcept pour usage dans les contextes critiques
    /// @note Ne pas libérer la chaîne retournée — mémoire statique en lecture seule
    /// @note Retourne "SC_UNKNOWN" pour toute valeur non reconnue ou hors plage
    const char* NkScancodeToString(NkScancode sc) noexcept {
        // Dispatch par switch exhaustif — optimisé en table de saut par le compilateur
        // Les cas sont groupés par catégorie fonctionnelle pour la maintenance
        switch (sc) {
            // -------------------------------------------------------------
            // Lettres (ordre HID, position physique US-QWERTY)
            // -------------------------------------------------------------
            case NkScancode::NK_SC_A:
                return "SC_A";
            case NkScancode::NK_SC_B:
                return "SC_B";
            case NkScancode::NK_SC_C:
                return "SC_C";
            case NkScancode::NK_SC_D:
                return "SC_D";
            case NkScancode::NK_SC_E:
                return "SC_E";
            case NkScancode::NK_SC_F:
                return "SC_F";
            case NkScancode::NK_SC_G:
                return "SC_G";
            case NkScancode::NK_SC_H:
                return "SC_H";
            case NkScancode::NK_SC_I:
                return "SC_I";
            case NkScancode::NK_SC_J:
                return "SC_J";
            case NkScancode::NK_SC_K:
                return "SC_K";
            case NkScancode::NK_SC_L:
                return "SC_L";
            case NkScancode::NK_SC_M:
                return "SC_M";
            case NkScancode::NK_SC_N:
                return "SC_N";
            case NkScancode::NK_SC_O:
                return "SC_O";
            case NkScancode::NK_SC_P:
                return "SC_P";
            case NkScancode::NK_SC_Q:
                return "SC_Q";
            case NkScancode::NK_SC_R:
                return "SC_R";
            case NkScancode::NK_SC_S:
                return "SC_S";
            case NkScancode::NK_SC_T:
                return "SC_T";
            case NkScancode::NK_SC_U:
                return "SC_U";
            case NkScancode::NK_SC_V:
                return "SC_V";
            case NkScancode::NK_SC_W:
                return "SC_W";
            case NkScancode::NK_SC_X:
                return "SC_X";
            case NkScancode::NK_SC_Y:
                return "SC_Y";
            case NkScancode::NK_SC_Z:
                return "SC_Z";

            // -------------------------------------------------------------
            // Chiffres de la ligne supérieure
            // -------------------------------------------------------------
            case NkScancode::NK_SC_1:
                return "SC_1";
            case NkScancode::NK_SC_2:
                return "SC_2";
            case NkScancode::NK_SC_3:
                return "SC_3";
            case NkScancode::NK_SC_4:
                return "SC_4";
            case NkScancode::NK_SC_5:
                return "SC_5";
            case NkScancode::NK_SC_6:
                return "SC_6";
            case NkScancode::NK_SC_7:
                return "SC_7";
            case NkScancode::NK_SC_8:
                return "SC_8";
            case NkScancode::NK_SC_9:
                return "SC_9";
            case NkScancode::NK_SC_0:
                return "SC_0";

            // -------------------------------------------------------------
            // Touches de contrôle principales et ponctuation
            // -------------------------------------------------------------
            case NkScancode::NK_SC_ENTER:
                return "SC_ENTER";
            case NkScancode::NK_SC_ESCAPE:
                return "SC_ESCAPE";
            case NkScancode::NK_SC_BACKSPACE:
                return "SC_BACKSPACE";
            case NkScancode::NK_SC_TAB:
                return "SC_TAB";
            case NkScancode::NK_SC_SPACE:
                return "SC_SPACE";
            case NkScancode::NK_SC_MINUS:
                return "SC_MINUS";
            case NkScancode::NK_SC_EQUALS:
                return "SC_EQUALS";
            case NkScancode::NK_SC_LBRACKET:
                return "SC_LBRACKET";
            case NkScancode::NK_SC_RBRACKET:
                return "SC_RBRACKET";
            case NkScancode::NK_SC_BACKSLASH:
                return "SC_BACKSLASH";
            case NkScancode::NK_SC_SEMICOLON:
                return "SC_SEMICOLON";
            case NkScancode::NK_SC_APOSTROPHE:
                return "SC_APOSTROPHE";
            case NkScancode::NK_SC_GRAVE:
                return "SC_GRAVE";
            case NkScancode::NK_SC_COMMA:
                return "SC_COMMA";
            case NkScancode::NK_SC_PERIOD:
                return "SC_PERIOD";
            case NkScancode::NK_SC_SLASH:
                return "SC_SLASH";

            // -------------------------------------------------------------
            // Verrous et touches de fonction
            // -------------------------------------------------------------
            case NkScancode::NK_SC_CAPS_LOCK:
                return "SC_CAPS_LOCK";
            case NkScancode::NK_SC_F1:
                return "SC_F1";
            case NkScancode::NK_SC_F2:
                return "SC_F2";
            case NkScancode::NK_SC_F3:
                return "SC_F3";
            case NkScancode::NK_SC_F4:
                return "SC_F4";
            case NkScancode::NK_SC_F5:
                return "SC_F5";
            case NkScancode::NK_SC_F6:
                return "SC_F6";
            case NkScancode::NK_SC_F7:
                return "SC_F7";
            case NkScancode::NK_SC_F8:
                return "SC_F8";
            case NkScancode::NK_SC_F9:
                return "SC_F9";
            case NkScancode::NK_SC_F10:
                return "SC_F10";
            case NkScancode::NK_SC_F11:
                return "SC_F11";
            case NkScancode::NK_SC_F12:
                return "SC_F12";

            // -------------------------------------------------------------
            // Bloc navigation et édition
            // -------------------------------------------------------------
            case NkScancode::NK_SC_PRINT_SCREEN:
                return "SC_PRINT_SCREEN";
            case NkScancode::NK_SC_SCROLL_LOCK:
                return "SC_SCROLL_LOCK";
            case NkScancode::NK_SC_PAUSE:
                return "SC_PAUSE";
            case NkScancode::NK_SC_INSERT:
                return "SC_INSERT";
            case NkScancode::NK_SC_HOME:
                return "SC_HOME";
            case NkScancode::NK_SC_PAGE_UP:
                return "SC_PAGE_UP";
            case NkScancode::NK_SC_DELETE:
                return "SC_DELETE";
            case NkScancode::NK_SC_END:
                return "SC_END";
            case NkScancode::NK_SC_PAGE_DOWN:
                return "SC_PAGE_DOWN";
            case NkScancode::NK_SC_RIGHT:
                return "SC_RIGHT";
            case NkScancode::NK_SC_LEFT:
                return "SC_LEFT";
            case NkScancode::NK_SC_DOWN:
                return "SC_DOWN";
            case NkScancode::NK_SC_UP:
                return "SC_UP";

            // -------------------------------------------------------------
            // Pavé numérique
            // -------------------------------------------------------------
            case NkScancode::NK_SC_NUM_LOCK:
                return "SC_NUM_LOCK";
            case NkScancode::NK_SC_NUMPAD_DIV:
                return "SC_NUMPAD_DIV";
            case NkScancode::NK_SC_NUMPAD_MUL:
                return "SC_NUMPAD_MUL";
            case NkScancode::NK_SC_NUMPAD_SUB:
                return "SC_NUMPAD_SUB";
            case NkScancode::NK_SC_NUMPAD_ADD:
                return "SC_NUMPAD_ADD";
            case NkScancode::NK_SC_NUMPAD_ENTER:
                return "SC_NUMPAD_ENTER";
            case NkScancode::NK_SC_NUMPAD_0:
                return "SC_NUMPAD_0";
            case NkScancode::NK_SC_NUMPAD_1:
                return "SC_NUMPAD_1";
            case NkScancode::NK_SC_NUMPAD_2:
                return "SC_NUMPAD_2";
            case NkScancode::NK_SC_NUMPAD_3:
                return "SC_NUMPAD_3";
            case NkScancode::NK_SC_NUMPAD_4:
                return "SC_NUMPAD_4";
            case NkScancode::NK_SC_NUMPAD_5:
                return "SC_NUMPAD_5";
            case NkScancode::NK_SC_NUMPAD_6:
                return "SC_NUMPAD_6";
            case NkScancode::NK_SC_NUMPAD_7:
                return "SC_NUMPAD_7";
            case NkScancode::NK_SC_NUMPAD_8:
                return "SC_NUMPAD_8";
            case NkScancode::NK_SC_NUMPAD_9:
                return "SC_NUMPAD_9";
            case NkScancode::NK_SC_NUMPAD_DOT:
                return "SC_NUMPAD_DOT";
            case NkScancode::NK_SC_NUMPAD_EQUALS:
                return "SC_NUMPAD_EQUALS";

            // -------------------------------------------------------------
            // Touches additionnelles et multimédia
            // -------------------------------------------------------------
            case NkScancode::NK_SC_APPLICATION:
                return "SC_APPLICATION";
            case NkScancode::NK_SC_MUTE:
                return "SC_MUTE";
            case NkScancode::NK_SC_VOLUME_UP:
                return "SC_VOLUME_UP";
            case NkScancode::NK_SC_VOLUME_DOWN:
                return "SC_VOLUME_DOWN";

            // -------------------------------------------------------------
            // Modificateurs (plage 0xE0–0xE7)
            // -------------------------------------------------------------
            case NkScancode::NK_SC_LCTRL:
                return "SC_LCTRL";
            case NkScancode::NK_SC_LSHIFT:
                return "SC_LSHIFT";
            case NkScancode::NK_SC_LALT:
                return "SC_LALT";
            case NkScancode::NK_SC_LSUPER:
                return "SC_LSUPER";
            case NkScancode::NK_SC_RCTRL:
                return "SC_RCTRL";
            case NkScancode::NK_SC_RSHIFT:
                return "SC_RSHIFT";
            case NkScancode::NK_SC_RALT:
                return "SC_RALT";
            case NkScancode::NK_SC_RSUPER:
                return "SC_RSUPER";

            // -------------------------------------------------------------
            // Fallback : valeur hors plage ou non définie
            // -------------------------------------------------------------
            default:
                return "SC_UNKNOWN";
        }
    }

    // =====================================================================
    // NkScancodeToKey — Mapping USB HID → NkKey (US-QWERTY invariant)
    // =====================================================================

    /// @brief Convertit un scancode USB HID vers son équivalent NkKey (position US-QWERTY)
    /// @param sc Le scancode USB HID à convertir
    /// @return NkKey correspondant à la position physique, ou NK_UNKNOWN si non mappé
    /// @note Cette fonction est le cœur du mapping cross-platform : elle garantit
    ///       que la même touche physique produit le même NkKey sur toutes les plateformes
    /// @note Fonction noexcept pour usage dans les hot paths de l'input
    NkKey NkScancodeToKey(NkScancode sc) noexcept {
        // Dispatch par switch exhaustif — optimisé en table de saut par le compilateur
        // Mapping direct : chaque NkScancode → NkKey correspondant (même position physique)
        switch (sc) {
            // -------------------------------------------------------------
            // Lettres (mapping identité : position physique US-QWERTY)
            // -------------------------------------------------------------
            case NkScancode::NK_SC_A:
                return NkKey::NK_A;
            case NkScancode::NK_SC_B:
                return NkKey::NK_B;
            case NkScancode::NK_SC_C:
                return NkKey::NK_C;
            case NkScancode::NK_SC_D:
                return NkKey::NK_D;
            case NkScancode::NK_SC_E:
                return NkKey::NK_E;
            case NkScancode::NK_SC_F:
                return NkKey::NK_F;
            case NkScancode::NK_SC_G:
                return NkKey::NK_G;
            case NkScancode::NK_SC_H:
                return NkKey::NK_H;
            case NkScancode::NK_SC_I:
                return NkKey::NK_I;
            case NkScancode::NK_SC_J:
                return NkKey::NK_J;
            case NkScancode::NK_SC_K:
                return NkKey::NK_K;
            case NkScancode::NK_SC_L:
                return NkKey::NK_L;
            case NkScancode::NK_SC_M:
                return NkKey::NK_M;
            case NkScancode::NK_SC_N:
                return NkKey::NK_N;
            case NkScancode::NK_SC_O:
                return NkKey::NK_O;
            case NkScancode::NK_SC_P:
                return NkKey::NK_P;
            case NkScancode::NK_SC_Q:
                return NkKey::NK_Q;
            case NkScancode::NK_SC_R:
                return NkKey::NK_R;
            case NkScancode::NK_SC_S:
                return NkKey::NK_S;
            case NkScancode::NK_SC_T:
                return NkKey::NK_T;
            case NkScancode::NK_SC_U:
                return NkKey::NK_U;
            case NkScancode::NK_SC_V:
                return NkKey::NK_V;
            case NkScancode::NK_SC_W:
                return NkKey::NK_W;
            case NkScancode::NK_SC_X:
                return NkKey::NK_X;
            case NkScancode::NK_SC_Y:
                return NkKey::NK_Y;
            case NkScancode::NK_SC_Z:
                return NkKey::NK_Z;

            // -------------------------------------------------------------
            // Chiffres et symboles de la ligne supérieure
            // -------------------------------------------------------------
            case NkScancode::NK_SC_1:
                return NkKey::NK_NUM1;
            case NkScancode::NK_SC_2:
                return NkKey::NK_NUM2;
            case NkScancode::NK_SC_3:
                return NkKey::NK_NUM3;
            case NkScancode::NK_SC_4:
                return NkKey::NK_NUM4;
            case NkScancode::NK_SC_5:
                return NkKey::NK_NUM5;
            case NkScancode::NK_SC_6:
                return NkKey::NK_NUM6;
            case NkScancode::NK_SC_7:
                return NkKey::NK_NUM7;
            case NkScancode::NK_SC_8:
                return NkKey::NK_NUM8;
            case NkScancode::NK_SC_9:
                return NkKey::NK_NUM9;
            case NkScancode::NK_SC_0:
                return NkKey::NK_NUM0;
            case NkScancode::NK_SC_ENTER:
                return NkKey::NK_ENTER;
            case NkScancode::NK_SC_ESCAPE:
                return NkKey::NK_ESCAPE;
            case NkScancode::NK_SC_BACKSPACE:
                return NkKey::NK_BACK;
            case NkScancode::NK_SC_TAB:
                return NkKey::NK_TAB;
            case NkScancode::NK_SC_SPACE:
                return NkKey::NK_SPACE;
            case NkScancode::NK_SC_MINUS:
                return NkKey::NK_MINUS;
            case NkScancode::NK_SC_EQUALS:
                return NkKey::NK_EQUALS;
            case NkScancode::NK_SC_LBRACKET:
                return NkKey::NK_LBRACKET;
            case NkScancode::NK_SC_RBRACKET:
                return NkKey::NK_RBRACKET;
            case NkScancode::NK_SC_BACKSLASH:
                return NkKey::NK_BACKSLASH;
            case NkScancode::NK_SC_SEMICOLON:
                return NkKey::NK_SEMICOLON;
            case NkScancode::NK_SC_APOSTROPHE:
                return NkKey::NK_APOSTROPHE;
            case NkScancode::NK_SC_GRAVE:
                return NkKey::NK_GRAVE;
            case NkScancode::NK_SC_COMMA:
                return NkKey::NK_COMMA;
            case NkScancode::NK_SC_PERIOD:
                return NkKey::NK_PERIOD;
            case NkScancode::NK_SC_SLASH:
                return NkKey::NK_SLASH;

            // -------------------------------------------------------------
            // Verrous et touches de fonction
            // -------------------------------------------------------------
            case NkScancode::NK_SC_CAPS_LOCK:
                return NkKey::NK_CAPSLOCK;
            case NkScancode::NK_SC_F1:
                return NkKey::NK_F1;
            case NkScancode::NK_SC_F2:
                return NkKey::NK_F2;
            case NkScancode::NK_SC_F3:
                return NkKey::NK_F3;
            case NkScancode::NK_SC_F4:
                return NkKey::NK_F4;
            case NkScancode::NK_SC_F5:
                return NkKey::NK_F5;
            case NkScancode::NK_SC_F6:
                return NkKey::NK_F6;
            case NkScancode::NK_SC_F7:
                return NkKey::NK_F7;
            case NkScancode::NK_SC_F8:
                return NkKey::NK_F8;
            case NkScancode::NK_SC_F9:
                return NkKey::NK_F9;
            case NkScancode::NK_SC_F10:
                return NkKey::NK_F10;
            case NkScancode::NK_SC_F11:
                return NkKey::NK_F11;
            case NkScancode::NK_SC_F12:
                return NkKey::NK_F12;
            case NkScancode::NK_SC_F13:
                return NkKey::NK_F13;
            case NkScancode::NK_SC_F14:
                return NkKey::NK_F14;
            case NkScancode::NK_SC_F15:
                return NkKey::NK_F15;
            case NkScancode::NK_SC_F16:
                return NkKey::NK_F16;
            case NkScancode::NK_SC_F17:
                return NkKey::NK_F17;
            case NkScancode::NK_SC_F18:
                return NkKey::NK_F18;
            case NkScancode::NK_SC_F19:
                return NkKey::NK_F19;
            case NkScancode::NK_SC_F20:
                return NkKey::NK_F20;
            case NkScancode::NK_SC_F21:
                return NkKey::NK_F21;
            case NkScancode::NK_SC_F22:
                return NkKey::NK_F22;
            case NkScancode::NK_SC_F23:
                return NkKey::NK_F23;
            case NkScancode::NK_SC_F24:
                return NkKey::NK_F24;

            // -------------------------------------------------------------
            // Bloc navigation et édition
            // -------------------------------------------------------------
            case NkScancode::NK_SC_PRINT_SCREEN:
                return NkKey::NK_PRINT_SCREEN;
            case NkScancode::NK_SC_SCROLL_LOCK:
                return NkKey::NK_SCROLL_LOCK;
            case NkScancode::NK_SC_PAUSE:
                return NkKey::NK_PAUSE_BREAK;
            case NkScancode::NK_SC_INSERT:
                return NkKey::NK_INSERT;
            case NkScancode::NK_SC_HOME:
                return NkKey::NK_HOME;
            case NkScancode::NK_SC_PAGE_UP:
                return NkKey::NK_PAGE_UP;
            case NkScancode::NK_SC_DELETE:
                return NkKey::NK_DELETE;
            case NkScancode::NK_SC_END:
                return NkKey::NK_END;
            case NkScancode::NK_SC_PAGE_DOWN:
                return NkKey::NK_PAGE_DOWN;
            case NkScancode::NK_SC_RIGHT:
                return NkKey::NK_RIGHT;
            case NkScancode::NK_SC_LEFT:
                return NkKey::NK_LEFT;
            case NkScancode::NK_SC_DOWN:
                return NkKey::NK_DOWN;
            case NkScancode::NK_SC_UP:
                return NkKey::NK_UP;

            // -------------------------------------------------------------
            // Pavé numérique
            // -------------------------------------------------------------
            case NkScancode::NK_SC_NUM_LOCK:
                return NkKey::NK_NUM_LOCK;
            case NkScancode::NK_SC_NUMPAD_DIV:
                return NkKey::NK_NUMPAD_DIV;
            case NkScancode::NK_SC_NUMPAD_MUL:
                return NkKey::NK_NUMPAD_MUL;
            case NkScancode::NK_SC_NUMPAD_SUB:
                return NkKey::NK_NUMPAD_SUB;
            case NkScancode::NK_SC_NUMPAD_ADD:
                return NkKey::NK_NUMPAD_ADD;
            case NkScancode::NK_SC_NUMPAD_ENTER:
                return NkKey::NK_NUMPAD_ENTER;
            case NkScancode::NK_SC_NUMPAD_0:
                return NkKey::NK_NUMPAD_0;
            case NkScancode::NK_SC_NUMPAD_1:
                return NkKey::NK_NUMPAD_1;
            case NkScancode::NK_SC_NUMPAD_2:
                return NkKey::NK_NUMPAD_2;
            case NkScancode::NK_SC_NUMPAD_3:
                return NkKey::NK_NUMPAD_3;
            case NkScancode::NK_SC_NUMPAD_4:
                return NkKey::NK_NUMPAD_4;
            case NkScancode::NK_SC_NUMPAD_5:
                return NkKey::NK_NUMPAD_5;
            case NkScancode::NK_SC_NUMPAD_6:
                return NkKey::NK_NUMPAD_6;
            case NkScancode::NK_SC_NUMPAD_7:
                return NkKey::NK_NUMPAD_7;
            case NkScancode::NK_SC_NUMPAD_8:
                return NkKey::NK_NUMPAD_8;
            case NkScancode::NK_SC_NUMPAD_9:
                return NkKey::NK_NUMPAD_9;
            case NkScancode::NK_SC_NUMPAD_DOT:
                return NkKey::NK_NUMPAD_DOT;
            case NkScancode::NK_SC_NUMPAD_EQUALS:
                return NkKey::NK_NUMPAD_EQUALS;

            // -------------------------------------------------------------
            // Touches additionnelles et multimédia
            // -------------------------------------------------------------
            case NkScancode::NK_SC_APPLICATION:
                return NkKey::NK_MENU;
            case NkScancode::NK_SC_MUTE:
                return NkKey::NK_MEDIA_MUTE;
            case NkScancode::NK_SC_VOLUME_UP:
                return NkKey::NK_MEDIA_VOLUME_UP;
            case NkScancode::NK_SC_VOLUME_DOWN:
                return NkKey::NK_MEDIA_VOLUME_DOWN;
            case NkScancode::NK_SC_MEDIA_PLAY_PAUSE:
                return NkKey::NK_MEDIA_PLAY_PAUSE;
            case NkScancode::NK_SC_MEDIA_STOP:
                return NkKey::NK_MEDIA_STOP;
            case NkScancode::NK_SC_MEDIA_NEXT:
                return NkKey::NK_MEDIA_NEXT;
            case NkScancode::NK_SC_MEDIA_PREV:
                return NkKey::NK_MEDIA_PREV;

            // -------------------------------------------------------------
            // Modificateurs
            // -------------------------------------------------------------
            case NkScancode::NK_SC_LCTRL:
                return NkKey::NK_LCTRL;
            case NkScancode::NK_SC_LSHIFT:
                return NkKey::NK_LSHIFT;
            case NkScancode::NK_SC_LALT:
                return NkKey::NK_LALT;
            case NkScancode::NK_SC_LSUPER:
                return NkKey::NK_LSUPER;
            case NkScancode::NK_SC_RCTRL:
                return NkKey::NK_RCTRL;
            case NkScancode::NK_SC_RSHIFT:
                return NkKey::NK_RSHIFT;
            case NkScancode::NK_SC_RALT:
                return NkKey::NK_RALT;
            case NkScancode::NK_SC_RSUPER:
                return NkKey::NK_RSUPER;

            // -------------------------------------------------------------
            // Fallback : scancode non reconnu ou hors plage
            // -------------------------------------------------------------
            default:
                return NkKey::NK_UNKNOWN;
        }
    }

    // =====================================================================
    // NkScancodeFromWin32 — Mapping PS/2 Set-1 (Win32) → USB HID
    // =====================================================================

    /// @brief Convertit un scancode PS/2 Set-1 Win32 vers un scancode USB HID
    /// @param win32 Scancode brut (bits 16–23 du LPARAM de WM_KEYDOWN, valeur 0x00–0x7F)
    /// @param ext true si la touche est étendue (bit 24 du LPARAM = 1, préfixe 0xE0 PS/2)
    /// @return NkScancode USB HID équivalent, ou NK_SC_UNKNOWN si non mappé
    /// @note Les touches étendues (flèches, Insert, Delete, Ctrl/Alt droits...)
    ///       ont un scancode PS/2 identique aux touches non-étendues mais avec
    ///       un préfixe 0xE0 — ce bit doit être passé séparément via le paramètre ext
    /// @note Fonction noexcept pour usage dans les hot paths de l'input
    NkScancode NkScancodeFromWin32(uint32 win32, bool ext) noexcept {
        // -------------------------------------------------------------
        // Table de mapping PS/2 Set-1 normale (0x00–0x58) → USB HID
        // Indexée directement par le scancode brut (0–127)
        // Les entrées non définies restent à NK_SC_UNKNOWN (valeur par défaut)
        // -------------------------------------------------------------
        static const NkScancode kNormalTable[0x80] = {
            // 0x00–0x0F : Inconnu, Escape, Chiffres 1–9, 0, -, =, Backspace, Tab
            NkScancode::NK_SC_UNKNOWN,      // 0x00
            NkScancode::NK_SC_ESCAPE,       // 0x01
            NkScancode::NK_SC_1,            // 0x02
            NkScancode::NK_SC_2,            // 0x03
            NkScancode::NK_SC_3,            // 0x04
            NkScancode::NK_SC_4,            // 0x05
            NkScancode::NK_SC_5,            // 0x06
            NkScancode::NK_SC_6,            // 0x07
            NkScancode::NK_SC_7,            // 0x08
            NkScancode::NK_SC_8,            // 0x09
            NkScancode::NK_SC_9,            // 0x0A
            NkScancode::NK_SC_0,            // 0x0B
            NkScancode::NK_SC_MINUS,        // 0x0C
            NkScancode::NK_SC_EQUALS,       // 0x0D
            NkScancode::NK_SC_BACKSPACE,    // 0x0E
            NkScancode::NK_SC_TAB,          // 0x0F

            // 0x10–0x1F : Q–P, [, ], Enter, LCtrl, A–L
            NkScancode::NK_SC_Q,            // 0x10
            NkScancode::NK_SC_W,            // 0x11
            NkScancode::NK_SC_E,            // 0x12
            NkScancode::NK_SC_R,            // 0x13
            NkScancode::NK_SC_T,            // 0x14
            NkScancode::NK_SC_Y,            // 0x15
            NkScancode::NK_SC_U,            // 0x16
            NkScancode::NK_SC_I,            // 0x17
            NkScancode::NK_SC_O,            // 0x18
            NkScancode::NK_SC_P,            // 0x19
            NkScancode::NK_SC_LBRACKET,     // 0x1A
            NkScancode::NK_SC_RBRACKET,     // 0x1B
            NkScancode::NK_SC_ENTER,        // 0x1C
            NkScancode::NK_SC_LCTRL,        // 0x1D
            NkScancode::NK_SC_A,            // 0x1E
            NkScancode::NK_SC_S,            // 0x1F

            // 0x20–0x2F : D–K, ;, ', `, LShift, \, Z–M
            NkScancode::NK_SC_D,            // 0x20
            NkScancode::NK_SC_F,            // 0x21
            NkScancode::NK_SC_G,            // 0x22
            NkScancode::NK_SC_H,            // 0x23
            NkScancode::NK_SC_J,            // 0x24
            NkScancode::NK_SC_K,            // 0x25
            NkScancode::NK_SC_L,            // 0x26
            NkScancode::NK_SC_SEMICOLON,    // 0x27
            NkScancode::NK_SC_APOSTROPHE,   // 0x28
            NkScancode::NK_SC_GRAVE,        // 0x29
            NkScancode::NK_SC_LSHIFT,       // 0x2A
            NkScancode::NK_SC_BACKSLASH,    // 0x2B
            NkScancode::NK_SC_Z,            // 0x2C
            NkScancode::NK_SC_X,            // 0x2D
            NkScancode::NK_SC_C,            // 0x2E
            NkScancode::NK_SC_V,            // 0x2F

            // 0x30–0x3F : B–/, RShift, Numpad *, LAlt, Space, CapsLock, F1–F6
            NkScancode::NK_SC_B,            // 0x30
            NkScancode::NK_SC_N,            // 0x31
            NkScancode::NK_SC_M,            // 0x32
            NkScancode::NK_SC_COMMA,        // 0x33
            NkScancode::NK_SC_PERIOD,       // 0x34
            NkScancode::NK_SC_SLASH,        // 0x35
            NkScancode::NK_SC_RSHIFT,       // 0x36
            NkScancode::NK_SC_NUMPAD_MUL,   // 0x37
            NkScancode::NK_SC_LALT,         // 0x38
            NkScancode::NK_SC_SPACE,        // 0x39
            NkScancode::NK_SC_CAPS_LOCK,    // 0x3A
            NkScancode::NK_SC_F1,           // 0x3B
            NkScancode::NK_SC_F2,           // 0x3C
            NkScancode::NK_SC_F3,           // 0x3D
            NkScancode::NK_SC_F4,           // 0x3E
            NkScancode::NK_SC_F5,           // 0x3F

            // 0x40–0x4F : F6–F10, NumLock, ScrollLock, Numpad 7–3, Numpad -
            NkScancode::NK_SC_F6,           // 0x40
            NkScancode::NK_SC_F7,           // 0x41
            NkScancode::NK_SC_F8,           // 0x42
            NkScancode::NK_SC_F9,           // 0x43
            NkScancode::NK_SC_F10,          // 0x44
            NkScancode::NK_SC_NUM_LOCK,     // 0x45
            NkScancode::NK_SC_SCROLL_LOCK,  // 0x46
            NkScancode::NK_SC_NUMPAD_7,     // 0x47
            NkScancode::NK_SC_NUMPAD_8,     // 0x48
            NkScancode::NK_SC_NUMPAD_9,     // 0x49
            NkScancode::NK_SC_NUMPAD_SUB,   // 0x4A
            NkScancode::NK_SC_NUMPAD_4,     // 0x4B
            NkScancode::NK_SC_NUMPAD_5,     // 0x4C
            NkScancode::NK_SC_NUMPAD_6,     // 0x4D
            NkScancode::NK_SC_NUMPAD_ADD,   // 0x4E
            NkScancode::NK_SC_NUMPAD_1,     // 0x4F

            // 0x50–0x5F : Numpad 2–0, ., (SysRq), (Inconnu), ISO \, F11, F12
            NkScancode::NK_SC_NUMPAD_2,     // 0x50
            NkScancode::NK_SC_NUMPAD_3,     // 0x51
            NkScancode::NK_SC_NUMPAD_0,     // 0x52
            NkScancode::NK_SC_NUMPAD_DOT,   // 0x53
            NkScancode::NK_SC_UNKNOWN,      // 0x54 — SysRq (non mappé)
            NkScancode::NK_SC_UNKNOWN,      // 0x55
            NkScancode::NK_SC_NONUS_BACKSLASH, // 0x56 — ISO backslash
            NkScancode::NK_SC_F11,          // 0x57
            NkScancode::NK_SC_F12,          // 0x58
            // 0x59–0x7F : Non définis → NK_SC_UNKNOWN (remplissage automatique)
        };

        // -------------------------------------------------------------
        // Validation de la plage d'entrée : scancode Win32 doit être < 0x80
        // -------------------------------------------------------------
        if (win32 >= 0x80) {
            return NkScancode::NK_SC_UNKNOWN;
        }

        // -------------------------------------------------------------
        // Gestion des touches étendues (préfixe 0xE0 PS/2)
        // Ces touches partagent le même scancode brut que les non-étendues
        // mais ont une sémantique différente (ex: Enter vs Numpad Enter)
        // -------------------------------------------------------------
        if (ext) {
            switch (win32) {
                case 0x1C:
                    return NkScancode::NK_SC_NUMPAD_ENTER;    // Enter du pavé numérique
                case 0x1D:
                    return NkScancode::NK_SC_RCTRL;           // Control droit
                case 0x35:
                    return NkScancode::NK_SC_NUMPAD_DIV;      // Division du pavé numérique
                case 0x37:
                    return NkScancode::NK_SC_PRINT_SCREEN;    // Print Screen (étendu)
                case 0x38:
                    return NkScancode::NK_SC_RALT;            // Alt droit (AltGr)
                case 0x47:
                    return NkScancode::NK_SC_HOME;            // Home (flèche)
                case 0x48:
                    return NkScancode::NK_SC_UP;              // Flèche haut
                case 0x49:
                    return NkScancode::NK_SC_PAGE_UP;         // Page Up
                case 0x4B:
                    return NkScancode::NK_SC_LEFT;            // Flèche gauche
                case 0x4D:
                    return NkScancode::NK_SC_RIGHT;           // Flèche droite
                case 0x4F:
                    return NkScancode::NK_SC_END;             // End
                case 0x50:
                    return NkScancode::NK_SC_DOWN;            // Flèche bas
                case 0x51:
                    return NkScancode::NK_SC_PAGE_DOWN;       // Page Down
                case 0x52:
                    return NkScancode::NK_SC_INSERT;          // Insert
                case 0x53:
                    return NkScancode::NK_SC_DELETE;          // Delete
                case 0x5B:
                    return NkScancode::NK_SC_LSUPER;          // Super gauche (Win/Cmd)
                case 0x5C:
                    return NkScancode::NK_SC_RSUPER;          // Super droit
                case 0x5D:
                    return NkScancode::NK_SC_APPLICATION;     // Touche Menu contextuel
                default:
                    return NkScancode::NK_SC_UNKNOWN;         // Code étendu non reconnu
            }
        }

        // -------------------------------------------------------------
        // Fallback : lookup dans la table normale (non-étendue)
        // -------------------------------------------------------------
        return kNormalTable[win32];
    }

    // =====================================================================
    // NkScancodeFromLinux — Mapping evdev keycode → USB HID
    // =====================================================================

    /// @brief Convertit un keycode Linux evdev vers un scancode USB HID
    /// @param kc Code clavier evdev (0–255, issu de <linux/input-event-codes.h>)
    /// @return NkScancode USB HID équivalent, ou NK_SC_UNKNOWN si non mappé
    /// @note Les keycodes evdev sont déjà proches du standard USB HID,
    ///       ce qui simplifie le mapping direct via switch exhaustif
    /// @note Pour XCB/XLib, utiliser NkScancodeFromXKeycode() qui gère
    ///       l'offset historique (xkeycode = evdev + 8)
    /// @note Fonction noexcept pour usage dans les hot paths de l'input
    NkScancode NkScancodeFromLinux(uint32 kc) noexcept {
        // Dispatch par switch exhaustif — optimisé en table de saut par le compilateur
        // Mapping direct evdev → USB HID (valeurs très proches, souvent identité)
        switch (kc) {
            // -------------------------------------------------------------
            // Échap, chiffres, symboles de la ligne supérieure
            // -------------------------------------------------------------
            case 1:
                return NkScancode::NK_SC_ESCAPE;
            case 2:
                return NkScancode::NK_SC_1;
            case 3:
                return NkScancode::NK_SC_2;
            case 4:
                return NkScancode::NK_SC_3;
            case 5:
                return NkScancode::NK_SC_4;
            case 6:
                return NkScancode::NK_SC_5;
            case 7:
                return NkScancode::NK_SC_6;
            case 8:
                return NkScancode::NK_SC_7;
            case 9:
                return NkScancode::NK_SC_8;
            case 10:
                return NkScancode::NK_SC_9;
            case 11:
                return NkScancode::NK_SC_0;
            case 12:
                return NkScancode::NK_SC_MINUS;
            case 13:
                return NkScancode::NK_SC_EQUALS;
            case 14:
                return NkScancode::NK_SC_BACKSPACE;

            // -------------------------------------------------------------
            // Tab, lettres Q–P, crochets, Enter, modificateurs gauche
            // -------------------------------------------------------------
            case 15:
                return NkScancode::NK_SC_TAB;
            case 16:
                return NkScancode::NK_SC_Q;
            case 17:
                return NkScancode::NK_SC_W;
            case 18:
                return NkScancode::NK_SC_E;
            case 19:
                return NkScancode::NK_SC_R;
            case 20:
                return NkScancode::NK_SC_T;
            case 21:
                return NkScancode::NK_SC_Y;
            case 22:
                return NkScancode::NK_SC_U;
            case 23:
                return NkScancode::NK_SC_I;
            case 24:
                return NkScancode::NK_SC_O;
            case 25:
                return NkScancode::NK_SC_P;
            case 26:
                return NkScancode::NK_SC_LBRACKET;
            case 27:
                return NkScancode::NK_SC_RBRACKET;
            case 28:
                return NkScancode::NK_SC_ENTER;
            case 29:
                return NkScancode::NK_SC_LCTRL;

            // -------------------------------------------------------------
            // Lettres A–L, ponctuation, Shift gauche, backslash
            // -------------------------------------------------------------
            case 30:
                return NkScancode::NK_SC_A;
            case 31:
                return NkScancode::NK_SC_S;
            case 32:
                return NkScancode::NK_SC_D;
            case 33:
                return NkScancode::NK_SC_F;
            case 34:
                return NkScancode::NK_SC_G;
            case 35:
                return NkScancode::NK_SC_H;
            case 36:
                return NkScancode::NK_SC_J;
            case 37:
                return NkScancode::NK_SC_K;
            case 38:
                return NkScancode::NK_SC_L;
            case 39:
                return NkScancode::NK_SC_SEMICOLON;
            case 40:
                return NkScancode::NK_SC_APOSTROPHE;
            case 41:
                return NkScancode::NK_SC_GRAVE;
            case 42:
                return NkScancode::NK_SC_LSHIFT;
            case 43:
                return NkScancode::NK_SC_BACKSLASH;

            // -------------------------------------------------------------
            // Lettres Z–M, ponctuation, Shift droit, Numpad *, Alt gauche
            // -------------------------------------------------------------
            case 44:
                return NkScancode::NK_SC_Z;
            case 45:
                return NkScancode::NK_SC_X;
            case 46:
                return NkScancode::NK_SC_C;
            case 47:
                return NkScancode::NK_SC_V;
            case 48:
                return NkScancode::NK_SC_B;
            case 49:
                return NkScancode::NK_SC_N;
            case 50:
                return NkScancode::NK_SC_M;
            case 51:
                return NkScancode::NK_SC_COMMA;
            case 52:
                return NkScancode::NK_SC_PERIOD;
            case 53:
                return NkScancode::NK_SC_SLASH;
            case 54:
                return NkScancode::NK_SC_RSHIFT;
            case 55:
                return NkScancode::NK_SC_NUMPAD_MUL;
            case 56:
                return NkScancode::NK_SC_LALT;
            case 57:
                return NkScancode::NK_SC_SPACE;

            // -------------------------------------------------------------
            // CapsLock, F1–F12, verrous, pavé numérique
            // -------------------------------------------------------------
            case 58:
                return NkScancode::NK_SC_CAPS_LOCK;
            case 59:
                return NkScancode::NK_SC_F1;
            case 60:
                return NkScancode::NK_SC_F2;
            case 61:
                return NkScancode::NK_SC_F3;
            case 62:
                return NkScancode::NK_SC_F4;
            case 63:
                return NkScancode::NK_SC_F5;
            case 64:
                return NkScancode::NK_SC_F6;
            case 65:
                return NkScancode::NK_SC_F7;
            case 66:
                return NkScancode::NK_SC_F8;
            case 67:
                return NkScancode::NK_SC_F9;
            case 68:
                return NkScancode::NK_SC_F10;
            case 69:
                return NkScancode::NK_SC_NUM_LOCK;
            case 70:
                return NkScancode::NK_SC_SCROLL_LOCK;
            case 71:
                return NkScancode::NK_SC_NUMPAD_7;
            case 72:
                return NkScancode::NK_SC_NUMPAD_8;
            case 73:
                return NkScancode::NK_SC_NUMPAD_9;
            case 74:
                return NkScancode::NK_SC_NUMPAD_SUB;
            case 75:
                return NkScancode::NK_SC_NUMPAD_4;
            case 76:
                return NkScancode::NK_SC_NUMPAD_5;
            case 77:
                return NkScancode::NK_SC_NUMPAD_6;
            case 78:
                return NkScancode::NK_SC_NUMPAD_ADD;
            case 79:
                return NkScancode::NK_SC_NUMPAD_1;
            case 80:
                return NkScancode::NK_SC_NUMPAD_2;
            case 81:
                return NkScancode::NK_SC_NUMPAD_3;
            case 82:
                return NkScancode::NK_SC_NUMPAD_0;
            case 83:
                return NkScancode::NK_SC_NUMPAD_DOT;

            // -------------------------------------------------------------
            // ISO backslash, F11–F12, touches étendues (pavé numérique, navigation)
            // -------------------------------------------------------------
            case 86:
                return NkScancode::NK_SC_NONUS_BACKSLASH;
            case 87:
                return NkScancode::NK_SC_F11;
            case 88:
                return NkScancode::NK_SC_F12;
            case 96:
                return NkScancode::NK_SC_NUMPAD_ENTER;
            case 97:
                return NkScancode::NK_SC_RCTRL;
            case 98:
                return NkScancode::NK_SC_NUMPAD_DIV;
            case 99:
                return NkScancode::NK_SC_PRINT_SCREEN;
            case 100:
                return NkScancode::NK_SC_RALT;

            // -------------------------------------------------------------
            // Navigation : Home, flèches, Page Up/Down, Insert, Delete
            // -------------------------------------------------------------
            case 102:
                return NkScancode::NK_SC_HOME;
            case 103:
                return NkScancode::NK_SC_UP;
            case 104:
                return NkScancode::NK_SC_PAGE_UP;
            case 105:
                return NkScancode::NK_SC_LEFT;
            case 106:
                return NkScancode::NK_SC_RIGHT;
            case 107:
                return NkScancode::NK_SC_END;
            case 108:
                return NkScancode::NK_SC_DOWN;
            case 109:
                return NkScancode::NK_SC_PAGE_DOWN;
            case 110:
                return NkScancode::NK_SC_INSERT;
            case 111:
                return NkScancode::NK_SC_DELETE;

            // -------------------------------------------------------------
            // Touches multimédia et système
            // -------------------------------------------------------------
            case 113:
                return NkScancode::NK_SC_MUTE;
            case 114:
                return NkScancode::NK_SC_VOLUME_DOWN;
            case 115:
                return NkScancode::NK_SC_VOLUME_UP;
            case 119:
                return NkScancode::NK_SC_PAUSE;
            case 125:
                return NkScancode::NK_SC_LSUPER;
            case 126:
                return NkScancode::NK_SC_RSUPER;
            case 127:
                return NkScancode::NK_SC_APPLICATION;

            // -------------------------------------------------------------
            // Fallback : keycode non reconnu ou hors plage mappée
            // -------------------------------------------------------------
            default:
                return NkScancode::NK_SC_UNKNOWN;
        }
    }

    /// @brief Convertit un keycode X11 (XCB/XLib) vers un scancode USB HID
    /// @param xkeycode Code clavier X11 (1–255, issu de xcb_keycode_t)
    /// @return NkScancode USB HID équivalent, ou NK_SC_UNKNOWN si non mappé
    /// @note Les keycodes X11 ont un offset historique de +8 par rapport à evdev :
    ///       xkeycode = evdev_keycode + 8
    /// @note Cette fonction applique automatiquement la soustraction de l'offset
    ///       avant de déléguer à NkScancodeFromLinux()
    /// @note Fonction noexcept pour usage dans les hot paths de l'input
    NkScancode NkScancodeFromXKeycode(uint32 xkeycode) noexcept {
        // Validation : les keycodes X11 valides sont >= 8 (après offset)
        if (xkeycode >= 8) {
            // Soustraction de l'offset historique puis conversion evdev → USB HID
            return NkScancodeFromLinux(xkeycode - 8);
        }
        // Keycode trop bas pour être valide après soustraction d'offset
        return NkScancode::NK_SC_UNKNOWN;
    }

    // =====================================================================
    // NkScancodeFromMac — Mapping Carbon keyCode (macOS) → USB HID
    // =====================================================================

    /// @brief Convertit un keyCode Carbon macOS vers un scancode USB HID
    /// @param kc Code clavier Carbon (0–127, issu de NSEvent.keyCode)
    /// @return NkScancode USB HID équivalent, ou NK_SC_UNKNOWN si non mappé
    /// @note Les keyCodes macOS sont déjà basés sur la position physique
    ///       (layout US-QWERTY de référence), ce qui simplifie le mapping
    /// @note Certains keyCodes macOS ont des sémantiques spécifiques :
    ///       - 0x36/0x37 : Cmd droit/gauche → Super
    ///       - 0x47 : Clear → Num Lock
    ///       - 0x72 : Help → Insert
    /// @note Fonction noexcept pour usage dans les hot paths de l'input
    NkScancode NkScancodeFromMac(uint32 kc) noexcept {
        // Dispatch par switch exhaustif — optimisé en table de saut par le compilateur
        // Mapping direct Carbon keyCode → USB HID (basé sur position physique)
        switch (kc) {
            // -------------------------------------------------------------
            // Lettres (layout QWERTY US de référence — ordre non alphabétique)
            // -------------------------------------------------------------
            case 0x00:
                return NkScancode::NK_SC_A;
            case 0x01:
                return NkScancode::NK_SC_S;
            case 0x02:
                return NkScancode::NK_SC_D;
            case 0x03:
                return NkScancode::NK_SC_F;
            case 0x04:
                return NkScancode::NK_SC_H;
            case 0x05:
                return NkScancode::NK_SC_G;
            case 0x06:
                return NkScancode::NK_SC_Z;
            case 0x07:
                return NkScancode::NK_SC_X;
            case 0x08:
                return NkScancode::NK_SC_C;
            case 0x09:
                return NkScancode::NK_SC_V;
            case 0x0B:
                return NkScancode::NK_SC_B;
            case 0x0C:
                return NkScancode::NK_SC_Q;
            case 0x0D:
                return NkScancode::NK_SC_W;
            case 0x0E:
                return NkScancode::NK_SC_E;
            case 0x0F:
                return NkScancode::NK_SC_R;
            case 0x10:
                return NkScancode::NK_SC_Y;
            case 0x11:
                return NkScancode::NK_SC_T;

            // -------------------------------------------------------------
            // Chiffres et symboles de la ligne supérieure
            // -------------------------------------------------------------
            case 0x12:
                return NkScancode::NK_SC_1;
            case 0x13:
                return NkScancode::NK_SC_2;
            case 0x14:
                return NkScancode::NK_SC_3;
            case 0x15:
                return NkScancode::NK_SC_4;
            case 0x16:
                return NkScancode::NK_SC_6;    // Note : 5 est à 0x17
            case 0x17:
                return NkScancode::NK_SC_5;
            case 0x18:
                return NkScancode::NK_SC_EQUALS;
            case 0x19:
                return NkScancode::NK_SC_9;
            case 0x1A:
                return NkScancode::NK_SC_7;
            case 0x1B:
                return NkScancode::NK_SC_MINUS;
            case 0x1C:
                return NkScancode::NK_SC_8;
            case 0x1D:
                return NkScancode::NK_SC_0;
            case 0x1E:
                return NkScancode::NK_SC_RBRACKET;
            case 0x1F:
                return NkScancode::NK_SC_O;
            case 0x20:
                return NkScancode::NK_SC_U;
            case 0x21:
                return NkScancode::NK_SC_LBRACKET;
            case 0x22:
                return NkScancode::NK_SC_I;
            case 0x23:
                return NkScancode::NK_SC_P;

            // -------------------------------------------------------------
            // Enter, lettres L–M, ponctuation, Tab, Space, Grave, Backspace
            // -------------------------------------------------------------
            case 0x24:
                return NkScancode::NK_SC_ENTER;
            case 0x25:
                return NkScancode::NK_SC_L;
            case 0x26:
                return NkScancode::NK_SC_J;
            case 0x27:
                return NkScancode::NK_SC_APOSTROPHE;
            case 0x28:
                return NkScancode::NK_SC_K;
            case 0x29:
                return NkScancode::NK_SC_SEMICOLON;
            case 0x2A:
                return NkScancode::NK_SC_BACKSLASH;
            case 0x2B:
                return NkScancode::NK_SC_COMMA;
            case 0x2C:
                return NkScancode::NK_SC_SLASH;
            case 0x2D:
                return NkScancode::NK_SC_N;
            case 0x2E:
                return NkScancode::NK_SC_M;
            case 0x2F:
                return NkScancode::NK_SC_PERIOD;
            case 0x30:
                return NkScancode::NK_SC_TAB;
            case 0x31:
                return NkScancode::NK_SC_SPACE;
            case 0x32:
                return NkScancode::NK_SC_GRAVE;
            case 0x33:
                return NkScancode::NK_SC_BACKSPACE;

            // -------------------------------------------------------------
            // Échap, Super (Cmd), modificateurs
            // -------------------------------------------------------------
            case 0x35:
                return NkScancode::NK_SC_ESCAPE;
            case 0x36:
                return NkScancode::NK_SC_RSUPER;    // Cmd droit
            case 0x37:
                return NkScancode::NK_SC_LSUPER;    // Cmd gauche
            case 0x38:
                return NkScancode::NK_SC_LSHIFT;
            case 0x39:
                return NkScancode::NK_SC_CAPS_LOCK;
            case 0x3A:
                return NkScancode::NK_SC_LALT;
            case 0x3B:
                return NkScancode::NK_SC_LCTRL;
            case 0x3C:
                return NkScancode::NK_SC_RSHIFT;
            case 0x3D:
                return NkScancode::NK_SC_RALT;
            case 0x3E:
                return NkScancode::NK_SC_RCTRL;

            // -------------------------------------------------------------
            // Touches de fonction F3–F20 (ordre non séquentiel sur macOS)
            // -------------------------------------------------------------
            case 0x40:
                return NkScancode::NK_SC_F17;
            case 0x43:
                return NkScancode::NK_SC_NUMPAD_MUL;
            case 0x45:
                return NkScancode::NK_SC_NUMPAD_ADD;
            case 0x47:
                return NkScancode::NK_SC_NUM_LOCK;    // Clear sur pavé numérique
            case 0x48:
                return NkScancode::NK_SC_VOLUME_UP;
            case 0x49:
                return NkScancode::NK_SC_VOLUME_DOWN;
            case 0x4A:
                return NkScancode::NK_SC_MUTE;
            case 0x4B:
                return NkScancode::NK_SC_NUMPAD_DIV;
            case 0x4C:
                return NkScancode::NK_SC_NUMPAD_ENTER;
            case 0x4E:
                return NkScancode::NK_SC_NUMPAD_SUB;
            case 0x4F:
                return NkScancode::NK_SC_F18;
            case 0x50:
                return NkScancode::NK_SC_F19;
            case 0x51:
                return NkScancode::NK_SC_NUMPAD_EQUALS;

            // -------------------------------------------------------------
            // Pavé numérique 0–9
            // -------------------------------------------------------------
            case 0x52:
                return NkScancode::NK_SC_NUMPAD_0;
            case 0x53:
                return NkScancode::NK_SC_NUMPAD_1;
            case 0x54:
                return NkScancode::NK_SC_NUMPAD_2;
            case 0x55:
                return NkScancode::NK_SC_NUMPAD_3;
            case 0x56:
                return NkScancode::NK_SC_NUMPAD_4;
            case 0x57:
                return NkScancode::NK_SC_NUMPAD_5;
            case 0x58:
                return NkScancode::NK_SC_NUMPAD_6;
            case 0x59:
                return NkScancode::NK_SC_NUMPAD_7;
            case 0x5A:
                return NkScancode::NK_SC_F20;
            case 0x5B:
                return NkScancode::NK_SC_NUMPAD_8;
            case 0x5C:
                return NkScancode::NK_SC_NUMPAD_9;

            // -------------------------------------------------------------
            // Touches de fonction F1–F16 (ordre dispersé)
            // -------------------------------------------------------------
            case 0x60:
                return NkScancode::NK_SC_F5;
            case 0x61:
                return NkScancode::NK_SC_F6;
            case 0x62:
                return NkScancode::NK_SC_F7;
            case 0x63:
                return NkScancode::NK_SC_F3;
            case 0x64:
                return NkScancode::NK_SC_F8;
            case 0x65:
                return NkScancode::NK_SC_F9;
            case 0x67:
                return NkScancode::NK_SC_F11;
            case 0x69:
                return NkScancode::NK_SC_F13;
            case 0x6A:
                return NkScancode::NK_SC_F16;
            case 0x6B:
                return NkScancode::NK_SC_F14;
            case 0x6D:
                return NkScancode::NK_SC_F10;
            case 0x6F:
                return NkScancode::NK_SC_F12;
            case 0x71:
                return NkScancode::NK_SC_F15;

            // -------------------------------------------------------------
            // Navigation et édition
            // -------------------------------------------------------------
            case 0x72:
                return NkScancode::NK_SC_INSERT;    // Help = Insert sur macOS
            case 0x73:
                return NkScancode::NK_SC_HOME;
            case 0x74:
                return NkScancode::NK_SC_PAGE_UP;
            case 0x75:
                return NkScancode::NK_SC_DELETE;
            case 0x76:
                return NkScancode::NK_SC_F4;
            case 0x77:
                return NkScancode::NK_SC_END;
            case 0x78:
                return NkScancode::NK_SC_F2;
            case 0x79:
                return NkScancode::NK_SC_PAGE_DOWN;
            case 0x7A:
                return NkScancode::NK_SC_F1;

            // -------------------------------------------------------------
            // Flèches directionnelles
            // -------------------------------------------------------------
            case 0x7B:
                return NkScancode::NK_SC_LEFT;
            case 0x7C:
                return NkScancode::NK_SC_RIGHT;
            case 0x7D:
                return NkScancode::NK_SC_DOWN;
            case 0x7E:
                return NkScancode::NK_SC_UP;

            // -------------------------------------------------------------
            // Fallback : keyCode non reconnu ou hors plage mappée
            // -------------------------------------------------------------
            default:
                return NkScancode::NK_SC_UNKNOWN;
        }
    }

    // =====================================================================
    // NkScancodeFromDOMCode — Mapping DOM KeyboardEvent.code → USB HID
    // =====================================================================

    /// @brief Convertit un DOM code Web vers un scancode USB HID
    /// @param code Chaîne KeyboardEvent.code (ex: "KeyA", "Space", "ArrowLeft")
    /// @return NkScancode USB HID équivalent, ou NK_SC_UNKNOWN si non reconnu ou nullptr
    /// @note Basé sur la spec UI Events : https://w3c.github.io/uievents-code/
    /// @note Les codes DOM sont layout-independent, idéaux pour un mapping fiable
    /// @note Cette fonction est utilisée dans le backend WebAssembly/WASM
    /// @note Fonction noexcept pour usage dans les hot paths de l'input
    NkScancode NkScancodeFromDOMCode(const char* code) noexcept {
        // -------------------------------------------------------------
        // Validation d'entrée : nullptr ou chaîne vide → UNKNOWN
        // -------------------------------------------------------------
        if (!code || !code[0]) {
            return NkScancode::NK_SC_UNKNOWN;
        }

        // -------------------------------------------------------------
        // Structure d'entrée pour la table de recherche statique
        // -------------------------------------------------------------
        struct MappingEntry {
            const char* domCode;    ///< Code DOM string (ex: "KeyA")
            NkScancode scancode;    ///< Scancode USB HID équivalent
        };

        // -------------------------------------------------------------
        // Table de recherche statique : DOM code → NkScancode
        // Ordonnée logiquement par catégorie pour la maintenance
        // Terminée par une sentinelle {nullptr, NK_SC_UNKNOWN}
        // -------------------------------------------------------------
        static const MappingEntry kTable[] = {
            // Lettres (KeyA–KeyZ)
            { "KeyA", NkScancode::NK_SC_A },
            { "KeyB", NkScancode::NK_SC_B },
            { "KeyC", NkScancode::NK_SC_C },
            { "KeyD", NkScancode::NK_SC_D },
            { "KeyE", NkScancode::NK_SC_E },
            { "KeyF", NkScancode::NK_SC_F },
            { "KeyG", NkScancode::NK_SC_G },
            { "KeyH", NkScancode::NK_SC_H },
            { "KeyI", NkScancode::NK_SC_I },
            { "KeyJ", NkScancode::NK_SC_J },
            { "KeyK", NkScancode::NK_SC_K },
            { "KeyL", NkScancode::NK_SC_L },
            { "KeyM", NkScancode::NK_SC_M },
            { "KeyN", NkScancode::NK_SC_N },
            { "KeyO", NkScancode::NK_SC_O },
            { "KeyP", NkScancode::NK_SC_P },
            { "KeyQ", NkScancode::NK_SC_Q },
            { "KeyR", NkScancode::NK_SC_R },
            { "KeyS", NkScancode::NK_SC_S },
            { "KeyT", NkScancode::NK_SC_T },
            { "KeyU", NkScancode::NK_SC_U },
            { "KeyV", NkScancode::NK_SC_V },
            { "KeyW", NkScancode::NK_SC_W },
            { "KeyX", NkScancode::NK_SC_X },
            { "KeyY", NkScancode::NK_SC_Y },
            { "KeyZ", NkScancode::NK_SC_Z },

            // Chiffres (Digit0–Digit9)
            { "Digit1", NkScancode::NK_SC_1 },
            { "Digit2", NkScancode::NK_SC_2 },
            { "Digit3", NkScancode::NK_SC_3 },
            { "Digit4", NkScancode::NK_SC_4 },
            { "Digit5", NkScancode::NK_SC_5 },
            { "Digit6", NkScancode::NK_SC_6 },
            { "Digit7", NkScancode::NK_SC_7 },
            { "Digit8", NkScancode::NK_SC_8 },
            { "Digit9", NkScancode::NK_SC_9 },
            { "Digit0", NkScancode::NK_SC_0 },

            // Touches de contrôle et ponctuation
            { "Enter", NkScancode::NK_SC_ENTER },
            { "Escape", NkScancode::NK_SC_ESCAPE },
            { "Backspace", NkScancode::NK_SC_BACKSPACE },
            { "Tab", NkScancode::NK_SC_TAB },
            { "Space", NkScancode::NK_SC_SPACE },
            { "Minus", NkScancode::NK_SC_MINUS },
            { "Equal", NkScancode::NK_SC_EQUALS },
            { "BracketLeft", NkScancode::NK_SC_LBRACKET },
            { "BracketRight", NkScancode::NK_SC_RBRACKET },
            { "Backslash", NkScancode::NK_SC_BACKSLASH },
            { "Semicolon", NkScancode::NK_SC_SEMICOLON },
            { "Quote", NkScancode::NK_SC_APOSTROPHE },
            { "Backquote", NkScancode::NK_SC_GRAVE },
            { "Comma", NkScancode::NK_SC_COMMA },
            { "Period", NkScancode::NK_SC_PERIOD },
            { "Slash", NkScancode::NK_SC_SLASH },
            { "CapsLock", NkScancode::NK_SC_CAPS_LOCK },

            // Touches de fonction F1–F24
            { "F1", NkScancode::NK_SC_F1 },
            { "F2", NkScancode::NK_SC_F2 },
            { "F3", NkScancode::NK_SC_F3 },
            { "F4", NkScancode::NK_SC_F4 },
            { "F5", NkScancode::NK_SC_F5 },
            { "F6", NkScancode::NK_SC_F6 },
            { "F7", NkScancode::NK_SC_F7 },
            { "F8", NkScancode::NK_SC_F8 },
            { "F9", NkScancode::NK_SC_F9 },
            { "F10", NkScancode::NK_SC_F10 },
            { "F11", NkScancode::NK_SC_F11 },
            { "F12", NkScancode::NK_SC_F12 },
            { "F13", NkScancode::NK_SC_F13 },
            { "F14", NkScancode::NK_SC_F14 },
            { "F15", NkScancode::NK_SC_F15 },
            { "F16", NkScancode::NK_SC_F16 },
            { "F17", NkScancode::NK_SC_F17 },
            { "F18", NkScancode::NK_SC_F18 },
            { "F19", NkScancode::NK_SC_F19 },
            { "F20", NkScancode::NK_SC_F20 },
            { "F21", NkScancode::NK_SC_F21 },
            { "F22", NkScancode::NK_SC_F22 },
            { "F23", NkScancode::NK_SC_F23 },
            { "F24", NkScancode::NK_SC_F24 },

            // Navigation et édition
            { "PrintScreen", NkScancode::NK_SC_PRINT_SCREEN },
            { "ScrollLock", NkScancode::NK_SC_SCROLL_LOCK },
            { "Pause", NkScancode::NK_SC_PAUSE },
            { "Insert", NkScancode::NK_SC_INSERT },
            { "Home", NkScancode::NK_SC_HOME },
            { "PageUp", NkScancode::NK_SC_PAGE_UP },
            { "Delete", NkScancode::NK_SC_DELETE },
            { "End", NkScancode::NK_SC_END },
            { "PageDown", NkScancode::NK_SC_PAGE_DOWN },
            { "ArrowRight", NkScancode::NK_SC_RIGHT },
            { "ArrowLeft", NkScancode::NK_SC_LEFT },
            { "ArrowDown", NkScancode::NK_SC_DOWN },
            { "ArrowUp", NkScancode::NK_SC_UP },

            // Pavé numérique
            { "NumLock", NkScancode::NK_SC_NUM_LOCK },
            { "NumpadDivide", NkScancode::NK_SC_NUMPAD_DIV },
            { "NumpadMultiply", NkScancode::NK_SC_NUMPAD_MUL },
            { "NumpadSubtract", NkScancode::NK_SC_NUMPAD_SUB },
            { "NumpadAdd", NkScancode::NK_SC_NUMPAD_ADD },
            { "NumpadEnter", NkScancode::NK_SC_NUMPAD_ENTER },
            { "NumpadDecimal", NkScancode::NK_SC_NUMPAD_DOT },
            { "Numpad0", NkScancode::NK_SC_NUMPAD_0 },
            { "Numpad1", NkScancode::NK_SC_NUMPAD_1 },
            { "Numpad2", NkScancode::NK_SC_NUMPAD_2 },
            { "Numpad3", NkScancode::NK_SC_NUMPAD_3 },
            { "Numpad4", NkScancode::NK_SC_NUMPAD_4 },
            { "Numpad5", NkScancode::NK_SC_NUMPAD_5 },
            { "Numpad6", NkScancode::NK_SC_NUMPAD_6 },
            { "Numpad7", NkScancode::NK_SC_NUMPAD_7 },
            { "Numpad8", NkScancode::NK_SC_NUMPAD_8 },
            { "Numpad9", NkScancode::NK_SC_NUMPAD_9 },
            { "NumpadEqual", NkScancode::NK_SC_NUMPAD_EQUALS },

            // Modificateurs
            { "ShiftLeft", NkScancode::NK_SC_LSHIFT },
            { "ShiftRight", NkScancode::NK_SC_RSHIFT },
            { "ControlLeft", NkScancode::NK_SC_LCTRL },
            { "ControlRight", NkScancode::NK_SC_RCTRL },
            { "AltLeft", NkScancode::NK_SC_LALT },
            { "AltRight", NkScancode::NK_SC_RALT },
            { "MetaLeft", NkScancode::NK_SC_LSUPER },
            { "MetaRight", NkScancode::NK_SC_RSUPER },
            { "OSLeft", NkScancode::NK_SC_LSUPER },      // Alias DOM pour Meta
            { "OSRight", NkScancode::NK_SC_RSUPER },     // Alias DOM pour Meta
            { "ContextMenu", NkScancode::NK_SC_APPLICATION },

            // ISO / International
            { "IntlBackslash", NkScancode::NK_SC_NONUS_BACKSLASH },

            // Touches multimédia
            { "AudioVolumeMute", NkScancode::NK_SC_MUTE },
            { "AudioVolumeUp", NkScancode::NK_SC_VOLUME_UP },
            { "AudioVolumeDown", NkScancode::NK_SC_VOLUME_DOWN },
            { "MediaPlayPause", NkScancode::NK_SC_MEDIA_PLAY_PAUSE },
            { "MediaStop", NkScancode::NK_SC_MEDIA_STOP },
            { "MediaTrackNext", NkScancode::NK_SC_MEDIA_NEXT },
            { "MediaTrackPrevious", NkScancode::NK_SC_MEDIA_PREV },

            // Sentinelle de fin de table
            { nullptr, NkScancode::NK_SC_UNKNOWN }
        };

        // -------------------------------------------------------------
        // Recherche linéaire dans la table (O(n), n ~ 150 entrées)
        // Suffisant pour l'initialisation d'événements clavier (non hot path)
        // -------------------------------------------------------------
        for (const MappingEntry* entry = kTable; entry->domCode != nullptr; ++entry) {
            if (std::strcmp(code, entry->domCode) == 0) {
                return entry->scancode;
            }
        }

        // -------------------------------------------------------------
        // Fallback : code DOM non reconnu
        // -------------------------------------------------------------
        return NkScancode::NK_SC_UNKNOWN;
    }

} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION DE NKKEYBOARDEVENT.CPP
// =============================================================================
// Ce fichier implémente les utilitaires de conversion cross-platform des codes clavier.
// Les exemples ci-dessous illustrent les patterns d'usage recommandés.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Logging de débogage avec NkKeyToString et NkScancodeToString
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkKeyboardEvent.h"

    void DebugKeyEvent(const nkentseu::NkKeyboardEvent& event) {
        using namespace nkentseu;

        // Affichage complet pour analyse cross-platform
        NK_FOUNDATION_LOG_DEBUG(
            "KeyEvent: key={}({}) scancode={}({}) native=0x{:X} mods={}",
            NkKeyToString(event.GetKey()),
            static_cast<uint32>(event.GetKey()),
            NkScancodeToString(event.GetScancode()),
            static_cast<uint32>(event.GetScancode()),
            event.GetNativeKey(),
            event.GetModifiers().ToString()
        );

        // Classification rapide pour filtrage
        if (NkKeyIsModifier(event.GetKey())) {
            NK_FOUNDATION_LOG_TRACE("  → Modifier key detected");
        }
        if (NkKeyIsNumpad(event.GetKey())) {
            NK_FOUNDATION_LOG_TRACE("  → Numpad key detected");
        }
        if (NkKeyIsFunctionKey(event.GetKey())) {
            NK_FOUNDATION_LOG_TRACE("  → Function key detected");
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Conversion backend-specific → NkKey dans un handler Win32
// -----------------------------------------------------------------------------
/*
    // Dans le backend Windows : traitement de WM_KEYDOWN
    LRESULT OnKeyDown(HWND hwnd, WPARAM wParam, LPARAM lParam) {
        using namespace nkentseu;

        // Extraction des paramètres Win32
        uint32 vkCode = static_cast<uint32>(wParam);
        uint32 scanCode = static_cast<uint32>((lParam >> 16) & 0xFF);
        bool isExtended = (lParam & 0x01000000) != 0;

        // Conversion en deux étapes : VK → Scancode → NkKey
        NkScancode sc = NkScancodeFromWin32(scanCode, isExtended);
        NkKey key = NkScancodeToKey(sc);

        // Construction de l'événement cross-platform
        NkKeyPressEvent keyEvent(
            key,
            sc,
            GetCurrentModifierState(),  // Helper lisant GetAsyncKeyState()
            vkCode,                     // Native key pour fallback/debug
            isExtended,
            GetWindowId(hwnd)
        );

        // Dispatch dans le système d'événements
        EventManager::GetInstance().Dispatch(keyEvent);

        return 0;  // Consommé
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Mapping DOM codes dans un backend WebAssembly
// -----------------------------------------------------------------------------
/*
    // Dans le backend Web/WASM : événement JavaScript KeyboardEvent
    extern "C" void OnWebKeyDown(const char* code, const char* key, bool repeat) {
        using namespace nkentseu;

        // Conversion DOM code → Scancode → NkKey
        NkScancode sc = NkScancodeFromDOMCode(code);
        NkKey nkKey = NkScancodeToKey(sc);

        if (nkKey == NkKey::NK_UNKNOWN) {
            // Fallback optionnel : tenter de mapper via le caractère
            // (moins fiable car dépend du layout clavier utilisateur)
            nkKey = FallbackKeyFromCharacter(key);
        }

        // Construction et dispatch de l'événement
        NkKeyPressEvent event(
            nkKey,
            sc,
            ParseWebModifiers(),  // Parse CtrlKey, ShiftKey, AltKey, MetaKey
            0,                    // Pas de native key en Web
            false,
            0
        );
        event.SetRepeat(repeat);

        EventManager::GetInstance().Dispatch(event);
    }

    // Fallback très simplifié : uniquement pour les lettres ASCII
    static NkKey FallbackKeyFromCharacter(const char* keyStr) {
        if (!keyStr || !*keyStr) return NkKey::NK_UNKNOWN;
        char c = keyStr[0];
        if (c >= 'a' && c <= 'z') {
            return static_cast<NkKey>(
                static_cast<uint32>(NkKey::NK_A) + (c - 'a')
            );
        }
        if (c >= 'A' && c <= 'Z') {
            return static_cast<NkKey>(
                static_cast<uint32>(NkKey::NK_A) + (c - 'A')
            );
        }
        return NkKey::NK_UNKNOWN;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Test unitaire de conversion pour validation cross-platform
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkKeyboardEvent.h"
    #include <cassert>

    void TestKeyConversion() {
        using namespace nkentseu;

        // Test : même touche physique → même NkKey sur toutes les plateformes
        struct TestCase {
            const char* platform;
            uint32 nativeCode;
            bool extended;
            NkKey expected;
        };

        static const TestCase tests[] = {
            { "Win32", 0x1E, false, NkKey::NK_A },      // 'A' PS/2
            { "Win32", 0x1C, true,  NkKey::NK_NUMPAD_ENTER }, // Numpad Enter étendu
            { "Linux", 30,    false, NkKey::NK_A },      // 'A' evdev
            { "X11",   38,    false, NkKey::NK_A },      // 'A' X11 (30+8)
            { "macOS", 0x00,  false, NkKey::NK_A },      // 'A' Carbon
            { "Web",   "KeyA", false, NkKey::NK_A },     // 'A' DOM
        };

        for (const auto& test : tests) {
            NkScancode sc = NkScancode::NK_SC_UNKNOWN;

            if (strcmp(test.platform, "Win32") == 0) {
                sc = NkScancodeFromWin32(test.nativeCode, test.extended);
            } else if (strcmp(test.platform, "Linux") == 0) {
                sc = NkScancodeFromLinux(test.nativeCode);
            } else if (strcmp(test.platform, "X11") == 0) {
                sc = NkScancodeFromXKeycode(test.nativeCode);
            } else if (strcmp(test.platform, "macOS") == 0) {
                sc = NkScancodeFromMac(test.nativeCode);
            } else if (strcmp(test.platform, "Web") == 0) {
                sc = NkScancodeFromDOMCode(reinterpret_cast<const char*>(test.nativeCode));
            }

            NkKey result = NkScancodeToKey(sc);
            assert(result == test.expected && "Key conversion mismatch");
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Gestion des modificateurs avec NkKeyIsModifier
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkKeyboardEvent.h"

    class ShortcutValidator {
    public:
        // Vérifie si une combinaison est un raccourci valide
        static bool IsValidShortcut(nkentseu::NkKey key, const nkentseu::NkModifierState& mods) {
            using namespace nkentseu;

            // Un raccourci doit avoir au moins un modificateur principal
            if (!mods.Any()) {
                return false;
            }

            // Les touches modificateurs seules ne sont pas des raccourcis
            if (NkKeyIsModifier(key)) {
                return false;
            }

            // Exemples de raccourcis bloqués
            if (key == NkKey::NK_ESCAPE && mods.ctrl) {
                return false;  // Ctrl+Esc réservé par l'OS
            }

            return true;
        }

        // Enregistrement d'un raccourci avec validation
        bool RegisterShortcut(ShortcutId id, NkKey key, NkModifierState mods) {
            if (!IsValidShortcut(key, mods)) {
                NK_FOUNDATION_LOG_WARNING("Invalid shortcut: {}+{}",
                    mods.ToString(), NkKeyToString(key));
                return false;
            }
            mShortcuts[id] = { key, mods };
            return true;
        }

    private:
        struct ShortcutDef {
            NkKey key;
            NkModifierState mods;
        };
        std::unordered_map<ShortcutId, ShortcutDef> mShortcuts;
    };
*/

// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
    1. AJOUT D'UNE NOUVELLE TOUCHE À NkKey/NkScancode :
       a) Ajouter l'entrée dans les enums NkKey et NkScancode (avant les sentinelles)
       b) Mettre à jour TOUS les switch dans ce fichier :
          - NkKeyToString
          - NkScancodeToString
          - NkScancodeToKey
          - NkScancodeFromWin32/Linux/Mac/DOM si la touche est supportée
       c) Mettre à jour les helpers : NkKeyIsModifier/Numpad/FunctionKey si pertinent
       d) Ajouter des tests unitaires pour chaque mapping plateforme

    2. PERFORMANCE DES SWITCH EXHAUSTIFS :
       - Les compilateurs modernes optimisent les switch d'enum en tables de saut O(1)
       - Garder les cases groupées logiquement pour la maintenance humaine
       - Éviter les allocations ou appels de fonction dans les branches de switch

    3. GESTION DES CODES NON RECONNUS :
       - Toujours retourner NK_UNKNOWN/NK_SC_UNKNOWN pour les codes non mappés
       - Logger en DEBUG (pas ERROR) pour éviter le spam en production
       - Documenter dans la Doxygen quels codes sont supportés par chaque backend

    4. TESTS CROSS-PLATFORM :
       - Tester que la même touche physique produit le même NkKey sur toutes les plateformes
       - Valider les cas limites : touches étendues Win32, offset X11, DOM codes rares
       - Tester avec différents layouts clavier (AZERTY, QWERTZ, Dvorak) pour NkTextInputEvent

    5. ÉVOLUTION DES SPÉCIFICATIONS :
       - USB HID : https://usb.org/sites/default/files/hut1_2.pdf (Table 10: Keyboard)
       - Win32 VK : https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
       - Linux evdev : /usr/include/linux/input-event-codes.h
       - macOS Carbon : <Carbon/CarbonEvents.h> (legacy, préférer CGEvent si possible)
       - Web DOM : https://w3c.github.io/uievents-code/

    6. DÉBOGAGE DES MAPPINGS :
       - Activer NKENTSEU_INPUT_DEBUG pour logger les conversions inconnues
       - Fournir un outil CLI pour tester interactivement : nk-keymap-test <platform> <code>
       - Inclure les tables de mapping dans les builds de debug pour inspection via debugger
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================