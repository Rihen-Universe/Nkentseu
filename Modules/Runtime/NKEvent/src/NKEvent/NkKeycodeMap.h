// =============================================================================
// NKEvent/NkKeycodeMap.h
// Conversion bidirectionnelle entre codes natifs de chaque plateforme
// et le NkKey cross-platform du framework.
//
// THÉORIE : Scancode vs Keycode vs NkKey
// =======================================
//
// SCANCODE (Position physique)
//   - Identifie une POSITION physique sur le clavier
//   - Indépendant de la disposition (layout QWERTY/AZERTY...)
//   - Basé sur le standard USB HID Usage Table (p. 53)
//   - Exemple : la touche "A" US et "Q" AZERTY ont le MÊME scancode
//
// NkKey (Code sémantique cross-platform)
//   - Code logique du framework, basé sur la position US-QWERTY de référence
//   - Equivalent conceptuel au scancode USB HID pour la portabilité
//   - Utilisé dans toute l'API événementielle du framework
//
// KEYCODE (Virtuel / Caractère)
//   - Identifie un CARACTÈRE ou une fonction logique
//   - Dépend du layout clavier et des modificateurs (Shift, AltGr...)
//   - Exemple : VK_A sur Windows retourne 'a' ou 'A' selon Shift
//
// POURQUOI CETTE COUCHE DE MAPPING ?
//   - Chaque OS utilise son propre système de codes clavier
//   - Win32 : Virtual Keys (VK_*) + scancodes PS/2 étendus
//   - X11 : KeySym (Xlib) + keycodes evdev
//   - macOS : Carbon keyCodes (legacy) + CGEvent codes
//   - Web : DOM KeyboardEvent.code (string) + keyCode (deprecated)
//   - Android : AKEYCODE_* constants
//
// Cette classe centralise toutes les conversions vers NkKey, garantissant :
//   - Une API événementielle cohérente sur toutes les plateformes
//   - Une maintenance isolée des mappings plateforme-spécifiques
//   - Une extensibilité facile pour ajouter de nouveaux backends
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_EVENT_NKKEYCODEMAP_H
#define NKENTSEU_EVENT_NKKEYCODEMAP_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des définitions de base pour NkKey et NkScancode.

    #include "NKEvent/NkKeyboardEvent.h"
    #include <cstring>  // Pour strcmp dans les mappings DOM

    namespace nkentseu {

        // =====================================================================
        // NkKeycodeMap — Table de conversion cross-platform des codes clavier
        // =====================================================================

        /**
         * @brief Classe utilitaire pour la conversion des codes clavier entre plateformes
         *
         * Cette classe fournit des méthodes statiques pour mapper :
         *   - Les scancodes USB HID vers NkKey (référence cross-platform)
         *   - Les Virtual Keys Win32 (VK_*) vers NkKey
         *   - Les KeySym X11 (Xlib/XCB) vers NkKey
         *   - Les keyCodes macOS Carbon vers NkKey
         *   - Les DOM codes Web (KeyboardEvent.code) vers NkKey
         *   - Les AKEYCODE Android vers NkKey
         *
         * @note Toutes les méthodes sont inline et static : aucune instanciation requise.
         * @note Les fonctions de mapping sont déterministes et sans état interne.
         * @note Pour la rétrocompatibilité, Normalize() permet de fusionner les variantes
         *       gauche/droite des modificateurs (LSHIFT/RSHIFT → LSHIFT).
         *
         * @example
         * @code
         *   // Conversion depuis un Virtual Key Win32
         *   NkKey key = NkKeycodeMap::NkKeyFromWin32VK(vkCode, isExtended);
         *
         *   // Normalisation pour comparaison indépendante de la main
         *   if (NkKeycodeMap::SameKey(pressedKey, NkKey::NK_LCTRL)) {
         *       // Ctrl pressé (gauche ou droite)
         *   }
         * @endcode
         */
        class NkKeycodeMap {
            public:
                // -------------------------------------------------------------
                // Conversion bidirectionnelle : NkScancode <-> NkKey
                // -------------------------------------------------------------

                /**
                 * @brief Convertit un scancode USB HID vers son équivalent NkKey
                 * @param sc Le scancode USB HID à convertir
                 * @return NkKey correspondant, ou NK_UNKNOWN si non mappé
                 * @note Délègue à NkScancodeToKey() défini dans NkKeyboardEvent.h
                 */
                static NkKey ScancodeToNkKey(NkScancode sc);

                /**
                 * @brief Convertit un NkKey vers son scancode USB HID équivalent
                 * @param key Le NkKey à convertir
                 * @return NkScancode correspondant, ou NK_SC_UNKNOWN si non mappé
                 * @note Actuellement non utilisé pour la réception d'événements
                 *       (seulement pour l'émission synthétique si nécessaire)
                 */
                static NkScancode NkKeyToScancode(NkKey key);

                // -------------------------------------------------------------
                // Win32 : Virtual Key (VK_*) et scancode PS/2 -> NkKey
                // -------------------------------------------------------------

                /**
                 * @brief Convertit un Virtual Key Win32 vers NkKey
                 * @param vk Code Virtual Key (ex: 0x41 pour 'A', 0x0D pour Enter)
                 * @param extended true si le scancode est étendu (bit 8 = 1, préfixe 0xE0)
                 * @return NkKey correspondant, ou NK_UNKNOWN si non reconnu
                 * @note Le paramètre extended est crucial pour distinguer :
                 *       - Enter (0x0D) vs Numpad Enter (0x0D + extended)
                 *       - Ctrl gauche (0x11) vs Ctrl droit (0x11 + extended)
                 *       - Insert (0x2D + extended) vs Numpad 0 (0x2D)
                 */
                static NkKey NkKeyFromWin32VK(uint32 vk, bool extended = false);

                /**
                 * @brief Convertit un scancode PS/2 Win32 vers NkKey
                 * @param scancode Scancode brut (1-127, sans bit d'extended)
                 * @param extended true si le scancode est préfixé par 0xE0
                 * @return NkKey correspondant via conversion intermédiaire NkScancode
                 * @note Utilise NkScancodeFromWin32() puis NkScancodeToKey()
                 */
                static NkKey NkKeyFromWin32Scancode(uint32 scancode, bool extended);

                // -------------------------------------------------------------
                // X11 (XLib et XCB) : KeySym et keycode -> NkKey
                // -------------------------------------------------------------

                /**
                 * @brief Convertit un KeySym X11 vers NkKey
                 * @param keysym Valeur KeySym (ex: 0xFF0D pour XK_Return)
                 * @return NkKey correspondant, ou NK_UNKNOWN si non reconnu
                 * @note Gère les plages ASCII (0x20-0x7E) et les keysyms spéciaux X11
                 * @note Les lettres minuscules/majuscules mapent vers le même NkKey
                 *       (la casse est gérée au niveau du caractère, pas de la touche)
                 */
                static NkKey NkKeyFromX11KeySym(uint32 keysym);

                /**
                 * @brief Convertit un keycode X11 (evdev) vers NkKey
                 * @param keycode Code clavier X11 (1-255, evdev = xkeycode - 8)
                 * @return NkKey correspondant via conversion intermédiaire NkScancode
                 * @note Utilise NkScancodeFromXKeycode() puis NkScancodeToKey()
                 */
                static NkKey NkKeyFromX11Keycode(uint32 keycode);

                // -------------------------------------------------------------
                // macOS / iOS : Carbon keyCode -> NkKey
                // -------------------------------------------------------------

                /**
                 * @brief Convertit un keyCode Carbon macOS vers NkKey
                 * @param keyCode Code clavier Carbon (0-127, layout US-QWERTY de référence)
                 * @return NkKey correspondant, ou NK_UNKNOWN si non reconnu
                 * @note Les keyCodes macOS sont déjà basés sur la position physique,
                 *       ce qui simplifie le mapping vers NkKey
                 */
                static NkKey NkKeyFromMacKeyCode(uint16 keyCode);

                // -------------------------------------------------------------
                // Web / WASM : DOM KeyboardEvent.code -> NkKey
                // -------------------------------------------------------------

                /**
                 * @brief Convertit un DOM code string vers NkKey
                 * @param domCode Chaîne KeyboardEvent.code (ex: "KeyA", "Enter", "ArrowLeft")
                 * @return NkKey correspondant, ou NK_UNKNOWN si non reconnu ou nullptr
                 * @note Utilise une table de recherche statique pour performance O(n)
                 * @note Les codes DOM sont indépendants du layout, idéaux pour le mapping
                 */
                static NkKey NkKeyFromDomCode(const char* domCode);

                // -------------------------------------------------------------
                // Android : AKEYCODE_* -> NkKey
                // -------------------------------------------------------------

                /**
                 * @brief Convertit un AKEYCODE Android vers NkKey
                 * @param aKeycode Constante AKEYCODE_* de l'API Android (android/keycodes.h)
                 * @return NkKey correspondant, ou NK_UNKNOWN si non reconnu
                 * @note Les AKEYCODE sont déjà normalisés et indépendants du layout
                 */
                static NkKey NkKeyFromAndroid(uint32 aKeycode);

                // -------------------------------------------------------------
                // Utilitaires de normalisation et comparaison
                // -------------------------------------------------------------

                /**
                 * @brief Normalise un NkKey en fusionnant les variantes gauche/droite
                 * @param key Le NkKey à normaliser
                 * @return NkKey normalisé (ex: NK_RSHIFT → NK_LSHIFT)
                 * @note Utile pour les raccourcis clavier indépendants de la main utilisée
                 * @note Ne modifie pas les touches non-modificatrices (lettres, chiffres...)
                 */
                static NkKey Normalize(NkKey key) {
                    switch (key) {
                        case NkKey::NK_RSHIFT:
                            return NkKey::NK_LSHIFT;
                        case NkKey::NK_RCTRL:
                            return NkKey::NK_LCTRL;
                        case NkKey::NK_RALT:
                            return NkKey::NK_LALT;
                        case NkKey::NK_RSUPER:
                            return NkKey::NK_LSUPER;
                        default:
                            return key;
                    }
                }

                /**
                 * @brief Compare deux NkKey en ignorant la distinction gauche/droite
                 * @param a Premier NkKey à comparer
                 * @param b Second NkKey à comparer
                 * @return true si Normalize(a) == Normalize(b)
                 * @note Utile pour détecter "Ctrl pressé" sans se soucier de la main
                 */
                static bool SameKey(NkKey a, NkKey b) {
                    return Normalize(a) == Normalize(b);
                }
        };

        // =====================================================================
        // Implémentations inline — Conversion Scancode <-> NkKey
        // =====================================================================

        /// @brief Implémentation de ScancodeToNkKey : délègue à la fonction globale
        /// @note Cette fonction est inline pour éviter les coûts d'appel dans les loops de input
        inline NkKey NkKeycodeMap::ScancodeToNkKey(NkScancode sc) {
            return NkScancodeToKey(sc);
        }

        /// @brief Implémentation de NkKeyToScancode : retourne UNKNOWN (non utilisé en réception)
        /// @note Pourrait être étendu pour l'émission d'événements synthétiques si besoin
        inline NkScancode NkKeycodeMap::NkKeyToScancode(NkKey /*key*/) {
            return NkScancode::NK_SC_UNKNOWN;
        }

        // =====================================================================
        // Implémentation : Win32 Virtual Key -> NkKey
        // =====================================================================

        /// @brief Mapping exhaustif des Virtual Keys Win32 vers NkKey
        /// @note Basé sur la documentation Microsoft : vk codes 0x00-0xFF
        /// @note Le paramètre 'extended' permet de distinguer les touches dupliquées
        inline NkKey NkKeycodeMap::NkKeyFromWin32VK(uint32 vk, bool extended) {
            switch (vk) {
                // -------------------------------------------------------------
                // Codes de contrôle et navigation
                // -------------------------------------------------------------
                case 0x08:
                    return NkKey::NK_BACK;
                case 0x09:
                    return NkKey::NK_TAB;
                case 0x0C:
                    return NkKey::NK_CLEAR;
                case 0x0D:
                    return extended ? NkKey::NK_NUMPAD_ENTER : NkKey::NK_ENTER;
                case 0x13:
                    return NkKey::NK_PAUSE_BREAK;
                case 0x14:
                    return NkKey::NK_CAPSLOCK;
                case 0x1B:
                    return NkKey::NK_ESCAPE;
                case 0x20:
                    return NkKey::NK_SPACE;

                // -------------------------------------------------------------
                // Modificateurs (distinction gauche/droite via 'extended')
                // -------------------------------------------------------------
                case 0x10:
                    return NkKey::NK_LSHIFT;
                case 0x11:
                    return extended ? NkKey::NK_RCTRL : NkKey::NK_LCTRL;
                case 0x12:
                    return extended ? NkKey::NK_RALT : NkKey::NK_LALT;

                // -------------------------------------------------------------
                // IME et conversion de caractères (Asie)
                // -------------------------------------------------------------
                case 0x15:
                    return NkKey::NK_KANA;
                case 0x17:
                    return NkKey::NK_KANJI;
                case 0x1C:
                    return NkKey::NK_CONVERT;
                case 0x1D:
                    return NkKey::NK_NONCONVERT;

                // -------------------------------------------------------------
                // Navigation et édition
                // -------------------------------------------------------------
                case 0x21:
                    return NkKey::NK_PAGE_UP;
                case 0x22:
                    return NkKey::NK_PAGE_DOWN;
                case 0x23:
                    return NkKey::NK_END;
                case 0x24:
                    return NkKey::NK_HOME;
                case 0x25:
                    return NkKey::NK_LEFT;
                case 0x26:
                    return NkKey::NK_UP;
                case 0x27:
                    return NkKey::NK_RIGHT;
                case 0x28:
                    return NkKey::NK_DOWN;
                case 0x2C:
                    return NkKey::NK_PRINT_SCREEN;
                case 0x2D:
                    return extended ? NkKey::NK_INSERT : NkKey::NK_NUMPAD_0;
                case 0x2E:
                    return extended ? NkKey::NK_DELETE : NkKey::NK_NUMPAD_DOT;

                // -------------------------------------------------------------
                // Chiffres (rangée supérieure du clavier)
                // -------------------------------------------------------------
                case 0x30:
                    return NkKey::NK_NUM0;
                case 0x31:
                    return NkKey::NK_NUM1;
                case 0x32:
                    return NkKey::NK_NUM2;
                case 0x33:
                    return NkKey::NK_NUM3;
                case 0x34:
                    return NkKey::NK_NUM4;
                case 0x35:
                    return NkKey::NK_NUM5;
                case 0x36:
                    return NkKey::NK_NUM6;
                case 0x37:
                    return NkKey::NK_NUM7;
                case 0x38:
                    return NkKey::NK_NUM8;
                case 0x39:
                    return NkKey::NK_NUM9;

                // -------------------------------------------------------------
                // Lettres (A-Z) — mapping direct depuis les codes ASCII VK
                // -------------------------------------------------------------
                case 0x41:
                    return NkKey::NK_A;
                case 0x42:
                    return NkKey::NK_B;
                case 0x43:
                    return NkKey::NK_C;
                case 0x44:
                    return NkKey::NK_D;
                case 0x45:
                    return NkKey::NK_E;
                case 0x46:
                    return NkKey::NK_F;
                case 0x47:
                    return NkKey::NK_G;
                case 0x48:
                    return NkKey::NK_H;
                case 0x49:
                    return NkKey::NK_I;
                case 0x4A:
                    return NkKey::NK_J;
                case 0x4B:
                    return NkKey::NK_K;
                case 0x4C:
                    return NkKey::NK_L;
                case 0x4D:
                    return NkKey::NK_M;
                case 0x4E:
                    return NkKey::NK_N;
                case 0x4F:
                    return NkKey::NK_O;
                case 0x50:
                    return NkKey::NK_P;
                case 0x51:
                    return NkKey::NK_Q;
                case 0x52:
                    return NkKey::NK_R;
                case 0x53:
                    return NkKey::NK_S;
                case 0x54:
                    return NkKey::NK_T;
                case 0x55:
                    return NkKey::NK_U;
                case 0x56:
                    return NkKey::NK_V;
                case 0x57:
                    return NkKey::NK_W;
                case 0x58:
                    return NkKey::NK_X;
                case 0x59:
                    return NkKey::NK_Y;
                case 0x5A:
                    return NkKey::NK_Z;

                // -------------------------------------------------------------
                // Touches Windows / Super
                // -------------------------------------------------------------
                case 0x5B:
                    return NkKey::NK_LSUPER;
                case 0x5C:
                    return NkKey::NK_RSUPER;
                case 0x5D:
                    return NkKey::NK_MENU;
                case 0x5F:
                    return NkKey::NK_SLEEP;

                // -------------------------------------------------------------
                // Pavé numérique (0x60-0x6F)
                // -------------------------------------------------------------
                case 0x60:
                    return NkKey::NK_NUMPAD_0;
                case 0x61:
                    return NkKey::NK_NUMPAD_1;
                case 0x62:
                    return NkKey::NK_NUMPAD_2;
                case 0x63:
                    return NkKey::NK_NUMPAD_3;
                case 0x64:
                    return NkKey::NK_NUMPAD_4;
                case 0x65:
                    return NkKey::NK_NUMPAD_5;
                case 0x66:
                    return NkKey::NK_NUMPAD_6;
                case 0x67:
                    return NkKey::NK_NUMPAD_7;
                case 0x68:
                    return NkKey::NK_NUMPAD_8;
                case 0x69:
                    return NkKey::NK_NUMPAD_9;
                case 0x6A:
                    return NkKey::NK_NUMPAD_MUL;
                case 0x6B:
                    return NkKey::NK_NUMPAD_ADD;
                case 0x6C:
                    return NkKey::NK_SEPARATOR;
                case 0x6D:
                    return NkKey::NK_NUMPAD_SUB;
                case 0x6E:
                    return NkKey::NK_NUMPAD_DOT;
                case 0x6F:
                    return NkKey::NK_NUMPAD_DIV;

                // -------------------------------------------------------------
                // Touches de fonction F1-F24 (0x70-0x87)
                // -------------------------------------------------------------
                case 0x70:
                    return NkKey::NK_F1;
                case 0x71:
                    return NkKey::NK_F2;
                case 0x72:
                    return NkKey::NK_F3;
                case 0x73:
                    return NkKey::NK_F4;
                case 0x74:
                    return NkKey::NK_F5;
                case 0x75:
                    return NkKey::NK_F6;
                case 0x76:
                    return NkKey::NK_F7;
                case 0x77:
                    return NkKey::NK_F8;
                case 0x78:
                    return NkKey::NK_F9;
                case 0x79:
                    return NkKey::NK_F10;
                case 0x7A:
                    return NkKey::NK_F11;
                case 0x7B:
                    return NkKey::NK_F12;
                case 0x7C:
                    return NkKey::NK_F13;
                case 0x7D:
                    return NkKey::NK_F14;
                case 0x7E:
                    return NkKey::NK_F15;
                case 0x7F:
                    return NkKey::NK_F16;
                case 0x80:
                    return NkKey::NK_F17;
                case 0x81:
                    return NkKey::NK_F18;
                case 0x82:
                    return NkKey::NK_F19;
                case 0x83:
                    return NkKey::NK_F20;
                case 0x84:
                    return NkKey::NK_F21;
                case 0x85:
                    return NkKey::NK_F22;
                case 0x86:
                    return NkKey::NK_F23;
                case 0x87:
                    return NkKey::NK_F24;

                // -------------------------------------------------------------
                // Verrous et états (0x90-0x91)
                // -------------------------------------------------------------
                case 0x90:
                    return NkKey::NK_NUM_LOCK;
                case 0x91:
                    return NkKey::NK_SCROLL_LOCK;

                // -------------------------------------------------------------
                // Modificateurs explicites (0xA0-0xA5)
                // -------------------------------------------------------------
                case 0xA0:
                    return NkKey::NK_LSHIFT;
                case 0xA1:
                    return NkKey::NK_RSHIFT;
                case 0xA2:
                    return NkKey::NK_LCTRL;
                case 0xA3:
                    return NkKey::NK_RCTRL;
                case 0xA4:
                    return NkKey::NK_LALT;
                case 0xA5:
                    return NkKey::NK_RALT;

                // -------------------------------------------------------------
                // Touches navigateur et média (0xA6-0xB3)
                // -------------------------------------------------------------
                case 0xA6:
                    return NkKey::NK_BROWSER_BACK;
                case 0xA7:
                    return NkKey::NK_BROWSER_FORWARD;
                case 0xA8:
                    return NkKey::NK_BROWSER_REFRESH;
                case 0xAA:
                    return NkKey::NK_BROWSER_SEARCH;
                case 0xAB:
                    return NkKey::NK_BROWSER_FAVORITES;
                case 0xAC:
                    return NkKey::NK_BROWSER_HOME;
                case 0xAD:
                    return NkKey::NK_MEDIA_MUTE;
                case 0xAE:
                    return NkKey::NK_MEDIA_VOLUME_DOWN;
                case 0xAF:
                    return NkKey::NK_MEDIA_VOLUME_UP;
                case 0xB0:
                    return NkKey::NK_MEDIA_NEXT;
                case 0xB1:
                    return NkKey::NK_MEDIA_PREV;
                case 0xB2:
                    return NkKey::NK_MEDIA_STOP;
                case 0xB3:
                    return NkKey::NK_MEDIA_PLAY_PAUSE;

                // -------------------------------------------------------------
                // Ponctuation et symboles (0xBA-0xDE)
                // -------------------------------------------------------------
                case 0xBA:
                    return NkKey::NK_SEMICOLON;
                case 0xBB:
                    return NkKey::NK_EQUALS;
                case 0xBC:
                    return NkKey::NK_COMMA;
                case 0xBD:
                    return NkKey::NK_MINUS;
                case 0xBE:
                    return NkKey::NK_PERIOD;
                case 0xBF:
                    return NkKey::NK_SLASH;
                case 0xC0:
                    return NkKey::NK_GRAVE;
                case 0xDB:
                    return NkKey::NK_LBRACKET;
                case 0xDC:
                    return NkKey::NK_BACKSLASH;
                case 0xDD:
                    return NkKey::NK_RBRACKET;
                case 0xDE:
                    return NkKey::NK_APOSTROPHE;

                // -------------------------------------------------------------
                // Codes legacy / IME asiatiques
                // -------------------------------------------------------------
                case 0x19:
                    return NkKey::NK_KANJI;
                case 0xF2:
                    return NkKey::NK_HANGUL;
                case 0xF1:
                    return NkKey::NK_HANJA;

                // -------------------------------------------------------------
                // Fallback : code non reconnu
                // -------------------------------------------------------------
                default:
                    return NkKey::NK_UNKNOWN;
            }
        }

        // =====================================================================
        // Implémentation : Win32 Scancode PS/2 -> NkKey
        // =====================================================================

        /// @brief Conversion d'un scancode PS/2 Win32 via la couche NkScancode
        /// @note Utilise NkScancodeFromWin32() pour la conversion intermédiaire
        inline NkKey NkKeycodeMap::NkKeyFromWin32Scancode(uint32 sc, bool extended) {
            return NkScancodeToKey(NkScancodeFromWin32(sc, extended));
        }

        // =====================================================================
        // Implémentation : X11 KeySym -> NkKey
        // =====================================================================

        /// @brief Mapping exhaustif des KeySym X11 vers NkKey
        /// @note Basé sur les définitions X11/keysymdef.h
        /// @note Les plages ASCII sont traitées séparément pour performance
        inline NkKey NkKeycodeMap::NkKeyFromX11KeySym(uint32 ks) {
            // -------------------------------------------------------------
            // Traitement optimisé des lettres (a-z, A-Z) → même NkKey
            // -------------------------------------------------------------
            if (ks >= 0x61 && ks <= 0x7A) {
                return static_cast<NkKey>(
                    static_cast<uint32>(NkKey::NK_A) + (ks - 0x61)
                );
            }
            if (ks >= 0x41 && ks <= 0x5A) {
                return static_cast<NkKey>(
                    static_cast<uint32>(NkKey::NK_A) + (ks - 0x41)
                );
            }

            // -------------------------------------------------------------
            // Chiffres ASCII (0-9)
            // -------------------------------------------------------------
            switch (ks) {
                case 0x30:
                    return NkKey::NK_NUM0;
                case 0x31:
                    return NkKey::NK_NUM1;
                case 0x32:
                    return NkKey::NK_NUM2;
                case 0x33:
                    return NkKey::NK_NUM3;
                case 0x34:
                    return NkKey::NK_NUM4;
                case 0x35:
                    return NkKey::NK_NUM5;
                case 0x36:
                    return NkKey::NK_NUM6;
                case 0x37:
                    return NkKey::NK_NUM7;
                case 0x38:
                    return NkKey::NK_NUM8;
                case 0x39:
                    return NkKey::NK_NUM9;

                // -------------------------------------------------------------
                // Contrôle et navigation
                // -------------------------------------------------------------
                case 0xFF08:
                    return NkKey::NK_BACK;
                case 0xFF09:
                    return NkKey::NK_TAB;
                case 0xFF0D:
                    return NkKey::NK_ENTER;
                case 0xFF1B:
                    return NkKey::NK_ESCAPE;
                case 0x0020:
                    return NkKey::NK_SPACE;
                case 0xFF13:
                    return NkKey::NK_PAUSE_BREAK;
                case 0xFF14:
                    return NkKey::NK_SCROLL_LOCK;
                case 0xFF15:
                    return NkKey::NK_PRINT_SCREEN;
                case 0xFF61:
                    return NkKey::NK_PRINT_SCREEN;

                // -------------------------------------------------------------
                // Navigation curseur
                // -------------------------------------------------------------
                case 0xFF50:
                    return NkKey::NK_HOME;
                case 0xFF51:
                    return NkKey::NK_LEFT;
                case 0xFF52:
                    return NkKey::NK_UP;
                case 0xFF53:
                    return NkKey::NK_RIGHT;
                case 0xFF54:
                    return NkKey::NK_DOWN;
                case 0xFF55:
                    return NkKey::NK_PAGE_UP;
                case 0xFF56:
                    return NkKey::NK_PAGE_DOWN;
                case 0xFF57:
                    return NkKey::NK_END;
                case 0xFF63:
                    return NkKey::NK_INSERT;
                case 0xFFFF:
                    return NkKey::NK_DELETE;

                // -------------------------------------------------------------
                // Menu et contexte
                // -------------------------------------------------------------
                case 0xFF60:
                    return NkKey::NK_MENU;

                // -------------------------------------------------------------
                // Touches de fonction F1-F24
                // -------------------------------------------------------------
                case 0xFFBE:
                    return NkKey::NK_F1;
                case 0xFFBF:
                    return NkKey::NK_F2;
                case 0xFFC0:
                    return NkKey::NK_F3;
                case 0xFFC1:
                    return NkKey::NK_F4;
                case 0xFFC2:
                    return NkKey::NK_F5;
                case 0xFFC3:
                    return NkKey::NK_F6;
                case 0xFFC4:
                    return NkKey::NK_F7;
                case 0xFFC5:
                    return NkKey::NK_F8;
                case 0xFFC6:
                    return NkKey::NK_F9;
                case 0xFFC7:
                    return NkKey::NK_F10;
                case 0xFFC8:
                    return NkKey::NK_F11;
                case 0xFFC9:
                    return NkKey::NK_F12;
                case 0xFFCA:
                    return NkKey::NK_F13;
                case 0xFFCB:
                    return NkKey::NK_F14;
                case 0xFFCC:
                    return NkKey::NK_F15;
                case 0xFFCD:
                    return NkKey::NK_F16;
                case 0xFFCE:
                    return NkKey::NK_F17;
                case 0xFFCF:
                    return NkKey::NK_F18;
                case 0xFFD0:
                    return NkKey::NK_F19;
                case 0xFFD1:
                    return NkKey::NK_F20;
                case 0xFFD2:
                    return NkKey::NK_F21;
                case 0xFFD3:
                    return NkKey::NK_F22;
                case 0xFFD4:
                    return NkKey::NK_F23;
                case 0xFFD5:
                    return NkKey::NK_F24;

                // -------------------------------------------------------------
                // Verrous
                // -------------------------------------------------------------
                case 0xFF7F:
                    return NkKey::NK_NUM_LOCK;
                case 0xFFE5:
                    return NkKey::NK_CAPSLOCK;

                // -------------------------------------------------------------
                // Pavé numérique : opérateurs
                // -------------------------------------------------------------
                case 0xFFAA:
                    return NkKey::NK_NUMPAD_MUL;
                case 0xFFAB:
                    return NkKey::NK_NUMPAD_ADD;
                case 0xFFAC:
                    return NkKey::NK_SEPARATOR;
                case 0xFFAD:
                    return NkKey::NK_NUMPAD_SUB;
                case 0xFFAE:
                    return NkKey::NK_NUMPAD_DOT;
                case 0xFFAF:
                    return NkKey::NK_NUMPAD_DIV;
                case 0xFF8D:
                    return NkKey::NK_NUMPAD_ENTER;
                case 0xFFBD:
                    return NkKey::NK_NUMPAD_EQUALS;

                // -------------------------------------------------------------
                // Pavé numérique : chiffres
                // -------------------------------------------------------------
                case 0xFFB0:
                    return NkKey::NK_NUMPAD_0;
                case 0xFFB1:
                    return NkKey::NK_NUMPAD_1;
                case 0xFFB2:
                    return NkKey::NK_NUMPAD_2;
                case 0xFFB3:
                    return NkKey::NK_NUMPAD_3;
                case 0xFFB4:
                    return NkKey::NK_NUMPAD_4;
                case 0xFFB5:
                    return NkKey::NK_NUMPAD_5;
                case 0xFFB6:
                    return NkKey::NK_NUMPAD_6;
                case 0xFFB7:
                    return NkKey::NK_NUMPAD_7;
                case 0xFFB8:
                    return NkKey::NK_NUMPAD_8;
                case 0xFFB9:
                    return NkKey::NK_NUMPAD_9;

                // -------------------------------------------------------------
                // Modificateurs
                // -------------------------------------------------------------
                case 0xFFE1:
                    return NkKey::NK_LSHIFT;
                case 0xFFE2:
                    return NkKey::NK_RSHIFT;
                case 0xFFE3:
                    return NkKey::NK_LCTRL;
                case 0xFFE4:
                    return NkKey::NK_RCTRL;
                case 0xFFE9:
                    return NkKey::NK_LALT;
                case 0xFFEA:
                    return NkKey::NK_RALT;
                case 0xFFEB:
                    return NkKey::NK_LSUPER;
                case 0xFFEC:
                    return NkKey::NK_RSUPER;
                case 0xFFED:
                    return NkKey::NK_LSUPER;
                case 0xFFEE:
                    return NkKey::NK_RSUPER;

                // -------------------------------------------------------------
                // Ponctuation ASCII
                // -------------------------------------------------------------
                case 0x0060:
                    return NkKey::NK_GRAVE;
                case 0x002D:
                    return NkKey::NK_MINUS;
                case 0x003D:
                    return NkKey::NK_EQUALS;
                case 0x005B:
                    return NkKey::NK_LBRACKET;
                case 0x005D:
                    return NkKey::NK_RBRACKET;
                case 0x005C:
                    return NkKey::NK_BACKSLASH;
                case 0x003B:
                    return NkKey::NK_SEMICOLON;
                case 0x0027:
                    return NkKey::NK_APOSTROPHE;
                case 0x002C:
                    return NkKey::NK_COMMA;
                case 0x002E:
                    return NkKey::NK_PERIOD;
                case 0x002F:
                    return NkKey::NK_SLASH;

                // -------------------------------------------------------------
                // Touches média X11 (extensions XF86)
                // -------------------------------------------------------------
                case 0x1008FF14:
                    return NkKey::NK_MEDIA_PLAY_PAUSE;
                case 0x1008FF15:
                    return NkKey::NK_MEDIA_STOP;
                case 0x1008FF16:
                    return NkKey::NK_MEDIA_PREV;
                case 0x1008FF17:
                    return NkKey::NK_MEDIA_NEXT;
                case 0x1008FF12:
                    return NkKey::NK_MEDIA_MUTE;
                case 0x1008FF13:
                    return NkKey::NK_MEDIA_VOLUME_UP;
                case 0x1008FF11:
                    return NkKey::NK_MEDIA_VOLUME_DOWN;

                // -------------------------------------------------------------
                // Fallback
                // -------------------------------------------------------------
                default:
                    return NkKey::NK_UNKNOWN;
            }
        }

        /// @brief Conversion d'un keycode X11 (evdev) via la couche NkScancode
        /// @note evdev keycode = X11 keycode - 8 (offset historique)
        inline NkKey NkKeycodeMap::NkKeyFromX11Keycode(uint32 keycode) {
            return NkScancodeToKey(NkScancodeFromXKeycode(keycode));
        }

        // =====================================================================
        // Implémentation : macOS Carbon keyCode -> NkKey
        // =====================================================================

        /// @brief Mapping exhaustif des keyCodes Carbon macOS vers NkKey
        /// @note Basé sur la table Carbon Events : <Carbon/CarbonEvents.h>
        /// @note Les keyCodes macOS sont déjà position-based (US-QWERTY reference)
        inline NkKey NkKeycodeMap::NkKeyFromMacKeyCode(uint16 kc) {
            switch (kc) {
                // -------------------------------------------------------------
                // Lettres (layout QWERTY US de référence)
                // -------------------------------------------------------------
                case 0x00:
                    return NkKey::NK_A;
                case 0x01:
                    return NkKey::NK_S;
                case 0x02:
                    return NkKey::NK_D;
                case 0x03:
                    return NkKey::NK_F;
                case 0x04:
                    return NkKey::NK_H;
                case 0x05:
                    return NkKey::NK_G;
                case 0x06:
                    return NkKey::NK_Z;
                case 0x07:
                    return NkKey::NK_X;
                case 0x08:
                    return NkKey::NK_C;
                case 0x09:
                    return NkKey::NK_V;
                case 0x0B:
                    return NkKey::NK_B;
                case 0x0C:
                    return NkKey::NK_Q;
                case 0x0D:
                    return NkKey::NK_W;
                case 0x0E:
                    return NkKey::NK_E;
                case 0x0F:
                    return NkKey::NK_R;
                case 0x10:
                    return NkKey::NK_Y;
                case 0x11:
                    return NkKey::NK_T;
                case 0x1F:
                    return NkKey::NK_O;
                case 0x20:
                    return NkKey::NK_U;
                case 0x21:
                    return NkKey::NK_LBRACKET;
                case 0x22:
                    return NkKey::NK_I;
                case 0x23:
                    return NkKey::NK_P;
                case 0x25:
                    return NkKey::NK_L;
                case 0x26:
                    return NkKey::NK_J;
                case 0x27:
                    return NkKey::NK_APOSTROPHE;
                case 0x28:
                    return NkKey::NK_K;
                case 0x29:
                    return NkKey::NK_SEMICOLON;
                case 0x2A:
                    return NkKey::NK_BACKSLASH;
                case 0x2B:
                    return NkKey::NK_COMMA;
                case 0x2C:
                    return NkKey::NK_SLASH;
                case 0x2D:
                    return NkKey::NK_N;
                case 0x2E:
                    return NkKey::NK_M;
                case 0x2F:
                    return NkKey::NK_PERIOD;

                // -------------------------------------------------------------
                // Chiffres et symboles (rangée supérieure)
                // -------------------------------------------------------------
                case 0x12:
                    return NkKey::NK_NUM1;
                case 0x13:
                    return NkKey::NK_NUM2;
                case 0x14:
                    return NkKey::NK_NUM3;
                case 0x15:
                    return NkKey::NK_NUM4;
                case 0x16:
                    return NkKey::NK_NUM6;
                case 0x17:
                    return NkKey::NK_NUM5;
                case 0x18:
                    return NkKey::NK_EQUALS;
                case 0x19:
                    return NkKey::NK_NUM9;
                case 0x1A:
                    return NkKey::NK_NUM7;
                case 0x1B:
                    return NkKey::NK_MINUS;
                case 0x1C:
                    return NkKey::NK_NUM8;
                case 0x1D:
                    return NkKey::NK_NUM0;
                case 0x1E:
                    return NkKey::NK_RBRACKET;
                case 0x32:
                    return NkKey::NK_GRAVE;

                // -------------------------------------------------------------
                // Contrôle et navigation
                // -------------------------------------------------------------
                case 0x24:
                    return NkKey::NK_ENTER;
                case 0x30:
                    return NkKey::NK_TAB;
                case 0x31:
                    return NkKey::NK_SPACE;
                case 0x33:
                    return NkKey::NK_BACK;
                case 0x35:
                    return NkKey::NK_ESCAPE;
                case 0x39:
                    return NkKey::NK_CAPSLOCK;

                // -------------------------------------------------------------
                // Modificateurs
                // -------------------------------------------------------------
                case 0x37:
                    return NkKey::NK_LSUPER;
                case 0x38:
                    return NkKey::NK_LSHIFT;
                case 0x3A:
                    return NkKey::NK_LALT;
                case 0x3B:
                    return NkKey::NK_LCTRL;
                case 0x3C:
                    return NkKey::NK_RSHIFT;
                case 0x3D:
                    return NkKey::NK_RALT;
                case 0x3E:
                    return NkKey::NK_RCTRL;
                case 0x3F:
                    return NkKey::NK_LSUPER;

                // -------------------------------------------------------------
                // Touches de fonction
                // -------------------------------------------------------------
                case 0x7A:
                    return NkKey::NK_F1;
                case 0x78:
                    return NkKey::NK_F2;
                case 0x63:
                    return NkKey::NK_F3;
                case 0x76:
                    return NkKey::NK_F4;
                case 0x60:
                    return NkKey::NK_F5;
                case 0x61:
                    return NkKey::NK_F6;
                case 0x62:
                    return NkKey::NK_F7;
                case 0x64:
                    return NkKey::NK_F8;
                case 0x65:
                    return NkKey::NK_F9;
                case 0x6D:
                    return NkKey::NK_F10;
                case 0x67:
                    return NkKey::NK_F11;
                case 0x6F:
                    return NkKey::NK_F12;
                case 0x69:
                    return NkKey::NK_F13;
                case 0x6B:
                    return NkKey::NK_F14;
                case 0x71:
                    return NkKey::NK_F15;
                case 0x6A:
                    return NkKey::NK_F16;
                case 0x40:
                    return NkKey::NK_F17;
                case 0x4F:
                    return NkKey::NK_F18;
                case 0x50:
                    return NkKey::NK_F19;
                case 0x5A:
                    return NkKey::NK_F20;

                // -------------------------------------------------------------
                // Pavé numérique
                // -------------------------------------------------------------
                case 0x47:
                    return NkKey::NK_NUM_LOCK;
                case 0x52:
                    return NkKey::NK_NUMPAD_0;
                case 0x53:
                    return NkKey::NK_NUMPAD_1;
                case 0x54:
                    return NkKey::NK_NUMPAD_2;
                case 0x55:
                    return NkKey::NK_NUMPAD_3;
                case 0x56:
                    return NkKey::NK_NUMPAD_4;
                case 0x57:
                    return NkKey::NK_NUMPAD_5;
                case 0x58:
                    return NkKey::NK_NUMPAD_6;
                case 0x59:
                    return NkKey::NK_NUMPAD_7;
                case 0x5B:
                    return NkKey::NK_NUMPAD_8;
                case 0x5C:
                    return NkKey::NK_NUMPAD_9;
                case 0x41:
                    return NkKey::NK_NUMPAD_DOT;
                case 0x43:
                    return NkKey::NK_NUMPAD_MUL;
                case 0x45:
                    return NkKey::NK_NUMPAD_ADD;
                case 0x4B:
                    return NkKey::NK_NUMPAD_DIV;
                case 0x4C:
                    return NkKey::NK_NUMPAD_ENTER;
                case 0x4E:
                    return NkKey::NK_NUMPAD_SUB;
                case 0x51:
                    return NkKey::NK_NUMPAD_EQUALS;

                // -------------------------------------------------------------
                // Navigation
                // -------------------------------------------------------------
                case 0x73:
                    return NkKey::NK_HOME;
                case 0x77:
                    return NkKey::NK_END;
                case 0x74:
                    return NkKey::NK_PAGE_UP;
                case 0x79:
                    return NkKey::NK_PAGE_DOWN;
                case 0x72:
                    return NkKey::NK_INSERT;
                case 0x75:
                    return NkKey::NK_DELETE;

                // -------------------------------------------------------------
                // Flèches directionnelles
                // -------------------------------------------------------------
                case 0x7B:
                    return NkKey::NK_LEFT;
                case 0x7C:
                    return NkKey::NK_RIGHT;
                case 0x7D:
                    return NkKey::NK_DOWN;
                case 0x7E:
                    return NkKey::NK_UP;

                // -------------------------------------------------------------
                // Fallback
                // -------------------------------------------------------------
                default:
                    return NkKey::NK_UNKNOWN;
            }
        }

        // =====================================================================
        // Implémentation : DOM KeyboardEvent.code -> NkKey (Web / WASM)
        // =====================================================================

        /// @brief Mapping des codes DOM Web vers NkKey via table de recherche
        /// @note Basé sur la spec UI Events : https://w3c.github.io/uievents-code/
        /// @note Les codes DOM sont layout-independent, idéaux pour le mapping
        inline NkKey NkKeycodeMap::NkKeyFromDomCode(const char* code) {
            // -------------------------------------------------------------
            // Validation d'entrée : nullptr → UNKNOWN
            // -------------------------------------------------------------
            if (!code) {
                return NkKey::NK_UNKNOWN;
            }

            // -------------------------------------------------------------
            // Table de recherche statique : code DOM string → NkKey
            // -------------------------------------------------------------
            struct MappingEntry {
                const char* domCode;
                NkKey key;
            };

            static const MappingEntry table[] = {
                // Ponctuation et symboles
                { "Backquote", NkKey::NK_GRAVE },
                { "Digit1", NkKey::NK_NUM1 },
                { "Digit2", NkKey::NK_NUM2 },
                { "Digit3", NkKey::NK_NUM3 },
                { "Digit4", NkKey::NK_NUM4 },
                { "Digit5", NkKey::NK_NUM5 },
                { "Digit6", NkKey::NK_NUM6 },
                { "Digit7", NkKey::NK_NUM7 },
                { "Digit8", NkKey::NK_NUM8 },
                { "Digit9", NkKey::NK_NUM9 },
                { "Digit0", NkKey::NK_NUM0 },
                { "Minus", NkKey::NK_MINUS },
                { "Equal", NkKey::NK_EQUALS },

                // Contrôle
                { "Backspace", NkKey::NK_BACK },
                { "Tab", NkKey::NK_TAB },
                { "CapsLock", NkKey::NK_CAPSLOCK },
                { "Enter", NkKey::NK_ENTER },
                { "Escape", NkKey::NK_ESCAPE },
                { "Space", NkKey::NK_SPACE },

                // Lettres (QWERTY layout de référence)
                { "KeyQ", NkKey::NK_Q },
                { "KeyW", NkKey::NK_W },
                { "KeyE", NkKey::NK_E },
                { "KeyR", NkKey::NK_R },
                { "KeyT", NkKey::NK_T },
                { "KeyY", NkKey::NK_Y },
                { "KeyU", NkKey::NK_U },
                { "KeyI", NkKey::NK_I },
                { "KeyO", NkKey::NK_O },
                { "KeyP", NkKey::NK_P },
                { "KeyA", NkKey::NK_A },
                { "KeyS", NkKey::NK_S },
                { "KeyD", NkKey::NK_D },
                { "KeyF", NkKey::NK_F },
                { "KeyG", NkKey::NK_G },
                { "KeyH", NkKey::NK_H },
                { "KeyJ", NkKey::NK_J },
                { "KeyK", NkKey::NK_K },
                { "KeyL", NkKey::NK_L },
                { "KeyZ", NkKey::NK_Z },
                { "KeyX", NkKey::NK_X },
                { "KeyC", NkKey::NK_C },
                { "KeyV", NkKey::NK_V },
                { "KeyB", NkKey::NK_B },
                { "KeyN", NkKey::NK_N },
                { "KeyM", NkKey::NK_M },

                // Ponctuation
                { "BracketLeft", NkKey::NK_LBRACKET },
                { "BracketRight", NkKey::NK_RBRACKET },
                { "Backslash", NkKey::NK_BACKSLASH },
                { "Semicolon", NkKey::NK_SEMICOLON },
                { "Quote", NkKey::NK_APOSTROPHE },
                { "Comma", NkKey::NK_COMMA },
                { "Period", NkKey::NK_PERIOD },
                { "Slash", NkKey::NK_SLASH },

                // Modificateurs
                { "ShiftLeft", NkKey::NK_LSHIFT },
                { "ShiftRight", NkKey::NK_RSHIFT },
                { "ControlLeft", NkKey::NK_LCTRL },
                { "ControlRight", NkKey::NK_RCTRL },
                { "AltLeft", NkKey::NK_LALT },
                { "AltRight", NkKey::NK_RALT },
                { "MetaLeft", NkKey::NK_LSUPER },
                { "MetaRight", NkKey::NK_RSUPER },
                { "ContextMenu", NkKey::NK_MENU },

                // Navigation
                { "PrintScreen", NkKey::NK_PRINT_SCREEN },
                { "ScrollLock", NkKey::NK_SCROLL_LOCK },
                { "Pause", NkKey::NK_PAUSE_BREAK },
                { "Insert", NkKey::NK_INSERT },
                { "Home", NkKey::NK_HOME },
                { "PageUp", NkKey::NK_PAGE_UP },
                { "Delete", NkKey::NK_DELETE },
                { "End", NkKey::NK_END },
                { "PageDown", NkKey::NK_PAGE_DOWN },
                { "ArrowRight", NkKey::NK_RIGHT },
                { "ArrowLeft", NkKey::NK_LEFT },
                { "ArrowDown", NkKey::NK_DOWN },
                { "ArrowUp", NkKey::NK_UP },

                // Touches de fonction
                { "F1", NkKey::NK_F1 },
                { "F2", NkKey::NK_F2 },
                { "F3", NkKey::NK_F3 },
                { "F4", NkKey::NK_F4 },
                { "F5", NkKey::NK_F5 },
                { "F6", NkKey::NK_F6 },
                { "F7", NkKey::NK_F7 },
                { "F8", NkKey::NK_F8 },
                { "F9", NkKey::NK_F9 },
                { "F10", NkKey::NK_F10 },
                { "F11", NkKey::NK_F11 },
                { "F12", NkKey::NK_F12 },

                // Pavé numérique
                { "NumLock", NkKey::NK_NUM_LOCK },
                { "NumpadDivide", NkKey::NK_NUMPAD_DIV },
                { "NumpadMultiply", NkKey::NK_NUMPAD_MUL },
                { "NumpadSubtract", NkKey::NK_NUMPAD_SUB },
                { "NumpadAdd", NkKey::NK_NUMPAD_ADD },
                { "NumpadEnter", NkKey::NK_NUMPAD_ENTER },
                { "NumpadDecimal", NkKey::NK_NUMPAD_DOT },
                { "Numpad0", NkKey::NK_NUMPAD_0 },
                { "Numpad1", NkKey::NK_NUMPAD_1 },
                { "Numpad2", NkKey::NK_NUMPAD_2 },
                { "Numpad3", NkKey::NK_NUMPAD_3 },
                { "Numpad4", NkKey::NK_NUMPAD_4 },
                { "Numpad5", NkKey::NK_NUMPAD_5 },
                { "Numpad6", NkKey::NK_NUMPAD_6 },
                { "Numpad7", NkKey::NK_NUMPAD_7 },
                { "Numpad8", NkKey::NK_NUMPAD_8 },
                { "Numpad9", NkKey::NK_NUMPAD_9 },
                { "NumpadEqual", NkKey::NK_NUMPAD_EQUALS },

                // Touches média
                { "MediaPlayPause", NkKey::NK_MEDIA_PLAY_PAUSE },
                { "MediaStop", NkKey::NK_MEDIA_STOP },
                { "MediaTrackNext", NkKey::NK_MEDIA_NEXT },
                { "MediaTrackPrevious", NkKey::NK_MEDIA_PREV },
                { "AudioVolumeMute", NkKey::NK_MEDIA_MUTE },
                { "AudioVolumeUp", NkKey::NK_MEDIA_VOLUME_UP },
                { "AudioVolumeDown", NkKey::NK_MEDIA_VOLUME_DOWN },

                // Navigateur
                { "BrowserBack", NkKey::NK_BROWSER_BACK },
                { "BrowserForward", NkKey::NK_BROWSER_FORWARD },
                { "BrowserRefresh", NkKey::NK_BROWSER_REFRESH },
                { "BrowserHome", NkKey::NK_BROWSER_HOME },
                { "BrowserSearch", NkKey::NK_BROWSER_SEARCH },
                { "BrowserFavorites", NkKey::NK_BROWSER_FAVORITES },

                // Sentinelles de fin de table
                { nullptr, NkKey::NK_UNKNOWN }
            };

            // -------------------------------------------------------------
            // Recherche linéaire dans la table (O(n), n ~ 150 entrées)
            // -------------------------------------------------------------
            for (const MappingEntry* entry = table; entry->domCode != nullptr; ++entry) {
                if (std::strcmp(code, entry->domCode) == 0) {
                    return entry->key;
                }
            }

            // -------------------------------------------------------------
            // Fallback : code DOM non reconnu
            // -------------------------------------------------------------
            return NkKey::NK_UNKNOWN;
        }

        // =====================================================================
        // Implémentation : Android AKEYCODE -> NkKey
        // =====================================================================

        /// @brief Mapping exhaustif des AKEYCODE Android vers NkKey
        /// @note Basé sur android/keycodes.h de l'NDK
        /// @note Les AKEYCODE sont déjà normalisés et layout-independent
        inline NkKey NkKeycodeMap::NkKeyFromAndroid(uint32 kc) {
            switch (kc) {
                // -------------------------------------------------------------
                // Contrôle et navigation
                // -------------------------------------------------------------
                case 4:
                    return NkKey::NK_BACK;
                case 23:
                    return NkKey::NK_ENTER;
                case 66:
                    return NkKey::NK_ENTER;
                case 67:
                    return NkKey::NK_BACK;
                case 111:
                    return NkKey::NK_ESCAPE;
                case 112:
                    return NkKey::NK_DELETE;

                // -------------------------------------------------------------
                // Navigation directionnelle
                // -------------------------------------------------------------
                case 19:
                    return NkKey::NK_UP;
                case 20:
                    return NkKey::NK_DOWN;
                case 21:
                    return NkKey::NK_LEFT;
                case 22:
                    return NkKey::NK_RIGHT;
                case 92:
                    return NkKey::NK_PAGE_UP;
                case 93:
                    return NkKey::NK_PAGE_DOWN;
                case 122:
                    return NkKey::NK_HOME;
                case 123:
                    return NkKey::NK_END;
                case 124:
                    return NkKey::NK_INSERT;

                // -------------------------------------------------------------
                // Chiffres (0-9)
                // -------------------------------------------------------------
                case 7:
                    return NkKey::NK_NUM0;
                case 8:
                    return NkKey::NK_NUM1;
                case 9:
                    return NkKey::NK_NUM2;
                case 10:
                    return NkKey::NK_NUM3;
                case 11:
                    return NkKey::NK_NUM4;
                case 12:
                    return NkKey::NK_NUM5;
                case 13:
                    return NkKey::NK_NUM6;
                case 14:
                    return NkKey::NK_NUM7;
                case 15:
                    return NkKey::NK_NUM8;
                case 16:
                    return NkKey::NK_NUM9;

                // -------------------------------------------------------------
                // Lettres (A-Z)
                // -------------------------------------------------------------
                case 29:
                    return NkKey::NK_A;
                case 30:
                    return NkKey::NK_B;
                case 31:
                    return NkKey::NK_C;
                case 32:
                    return NkKey::NK_D;
                case 33:
                    return NkKey::NK_E;
                case 34:
                    return NkKey::NK_F;
                case 35:
                    return NkKey::NK_G;
                case 36:
                    return NkKey::NK_H;
                case 37:
                    return NkKey::NK_I;
                case 38:
                    return NkKey::NK_J;
                case 39:
                    return NkKey::NK_K;
                case 40:
                    return NkKey::NK_L;
                case 41:
                    return NkKey::NK_M;
                case 42:
                    return NkKey::NK_N;
                case 43:
                    return NkKey::NK_O;
                case 44:
                    return NkKey::NK_P;
                case 45:
                    return NkKey::NK_Q;
                case 46:
                    return NkKey::NK_R;
                case 47:
                    return NkKey::NK_S;
                case 48:
                    return NkKey::NK_T;
                case 49:
                    return NkKey::NK_U;
                case 50:
                    return NkKey::NK_V;
                case 51:
                    return NkKey::NK_W;
                case 52:
                    return NkKey::NK_X;
                case 53:
                    return NkKey::NK_Y;
                case 54:
                    return NkKey::NK_Z;

                // -------------------------------------------------------------
                // Ponctuation et symboles
                // -------------------------------------------------------------
                case 55:
                    return NkKey::NK_COMMA;
                case 56:
                    return NkKey::NK_PERIOD;
                case 68:
                    return NkKey::NK_GRAVE;
                case 69:
                    return NkKey::NK_MINUS;
                case 70:
                    return NkKey::NK_EQUALS;
                case 71:
                    return NkKey::NK_LBRACKET;
                case 72:
                    return NkKey::NK_RBRACKET;
                case 73:
                    return NkKey::NK_BACKSLASH;
                case 74:
                    return NkKey::NK_SEMICOLON;
                case 75:
                    return NkKey::NK_APOSTROPHE;
                case 76:
                    return NkKey::NK_SLASH;

                // -------------------------------------------------------------
                // Contrôle et modificateurs
                // -------------------------------------------------------------
                case 61:
                    return NkKey::NK_TAB;
                case 62:
                    return NkKey::NK_SPACE;
                case 57:
                    return NkKey::NK_LALT;
                case 58:
                    return NkKey::NK_RALT;
                case 59:
                    return NkKey::NK_LSHIFT;
                case 60:
                    return NkKey::NK_RSHIFT;
                case 113:
                    return NkKey::NK_LCTRL;
                case 114:
                    return NkKey::NK_RCTRL;
                case 82:
                    return NkKey::NK_MENU;
                case 115:
                    return NkKey::NK_CAPSLOCK;
                case 116:
                    return NkKey::NK_SCROLL_LOCK;
                case 117:
                    return NkKey::NK_LSUPER;
                case 118:
                    return NkKey::NK_RSUPER;
                case 120:
                    return NkKey::NK_PRINT_SCREEN;
                case 121:
                    return NkKey::NK_PAUSE_BREAK;

                // -------------------------------------------------------------
                // Touches de fonction F1-F12
                // -------------------------------------------------------------
                case 131:
                    return NkKey::NK_F1;
                case 132:
                    return NkKey::NK_F2;
                case 133:
                    return NkKey::NK_F3;
                case 134:
                    return NkKey::NK_F4;
                case 135:
                    return NkKey::NK_F5;
                case 136:
                    return NkKey::NK_F6;
                case 137:
                    return NkKey::NK_F7;
                case 138:
                    return NkKey::NK_F8;
                case 139:
                    return NkKey::NK_F9;
                case 140:
                    return NkKey::NK_F10;
                case 141:
                    return NkKey::NK_F11;
                case 142:
                    return NkKey::NK_F12;

                // -------------------------------------------------------------
                // Verrous
                // -------------------------------------------------------------
                case 143:
                    return NkKey::NK_NUM_LOCK;

                // -------------------------------------------------------------
                // Pavé numérique
                // -------------------------------------------------------------
                case 144:
                    return NkKey::NK_NUMPAD_0;
                case 145:
                    return NkKey::NK_NUMPAD_1;
                case 146:
                    return NkKey::NK_NUMPAD_2;
                case 147:
                    return NkKey::NK_NUMPAD_3;
                case 148:
                    return NkKey::NK_NUMPAD_4;
                case 149:
                    return NkKey::NK_NUMPAD_5;
                case 150:
                    return NkKey::NK_NUMPAD_6;
                case 151:
                    return NkKey::NK_NUMPAD_7;
                case 152:
                    return NkKey::NK_NUMPAD_8;
                case 153:
                    return NkKey::NK_NUMPAD_9;
                case 17:
                    return NkKey::NK_NUMPAD_MUL;
                case 154:
                    return NkKey::NK_NUMPAD_DIV;
                case 156:
                    return NkKey::NK_NUMPAD_SUB;
                case 157:
                    return NkKey::NK_NUMPAD_ADD;
                case 158:
                    return NkKey::NK_NUMPAD_DOT;
                case 160:
                    return NkKey::NK_NUMPAD_ENTER;

                // -------------------------------------------------------------
                // Touches média
                // -------------------------------------------------------------
                case 85:
                    return NkKey::NK_MEDIA_PLAY_PAUSE;
                case 86:
                    return NkKey::NK_MEDIA_STOP;
                case 87:
                    return NkKey::NK_MEDIA_NEXT;
                case 88:
                    return NkKey::NK_MEDIA_PREV;
                case 91:
                    return NkKey::NK_MEDIA_MUTE;
                case 220:
                    return NkKey::NK_MEDIA_MUTE;
                case 164:
                    return NkKey::NK_MEDIA_VOLUME_UP;
                case 165:
                    return NkKey::NK_MEDIA_VOLUME_DOWN;

                // -------------------------------------------------------------
                // Fallback
                // -------------------------------------------------------------
                default:
                    return NkKey::NK_UNKNOWN;
            }
        }

    } // namespace nkentseu

#endif // NKENTSEU_EVENT_NKKEYCODEMAP_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKKEYCODEMAP.H
// =============================================================================
// Ce fichier fournit les utilitaires de conversion cross-platform des codes clavier.
// Les exemples ci-dessous illustrent les patterns d'usage recommandés.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Conversion depuis un Virtual Key Win32 dans un backend Windows
// -----------------------------------------------------------------------------
/*
    // Dans le backend Windows : traitement d'un message WM_KEYDOWN
    LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
            using namespace nkentseu;

            // Extraction des paramètres Win32
            uint32 vkCode = static_cast<uint32>(wParam);
            uint32 scanCode = static_cast<uint32>((lParam >> 16) & 0xFF);
            bool isExtended = (lParam & 0x01000000) != 0;

            // Conversion vers NkKey cross-platform
            NkKey key = NkKeycodeMap::NkKeyFromWin32VK(vkCode, isExtended);

            // Construction de l'événement clavier
            NkKeyEvent keyEvent(
                key,
                NkButtonState::NK_PRESSED,
                GetCurrentModifierState()  // Helper pour lire GetAsyncKeyState()
            );

            // Dispatch dans le système d'événements
            EventManager::GetInstance().Dispatch(keyEvent);

            // Gestion spéciale : empêcher le beep sur Alt+F4 par exemple
            if (key == NkKey::NK_F4 && keyEvent.GetModifiers().IsAlt()) {
                return 0;  // Consommé
            }
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Normalisation pour les raccourcis clavier indépendants de la main
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkKeycodeMap.h"

    class ShortcutManager {
    public:
        // Enregistrement d'un raccourci : Ctrl+S pour sauvegarder
        void RegisterSaveShortcut() {
            using namespace nkentseu;

            // Le raccourci est défini avec LCTRL, mais doit fonctionner
            // aussi si l'utilisateur utilise RCTRL
            mShortcuts[ShortcutId::Save] = {
                .key = NkKey::NK_S,
                .modifiers = NkModifierState::Ctrl  // Normalisé
            };
        }

        // Vérification si un événement clavier correspond à un raccourci
        bool MatchesShortcut(const NkKeyEvent& event, ShortcutId id) {
            using namespace nkentseu;

            const auto& shortcut = mShortcuts[id];

            // Comparaison de la touche principale
            if (event.GetKey() != shortcut.key) {
                return false;
            }

            // Comparaison des modificateurs avec normalisation
            // SameKey() ignore la distinction gauche/droite
            if (!NkKeycodeMap::SameKey(
                    NkKeycodeMap::Normalize(event.GetModifiers().ToNkKey()),
                    NkKeycodeMap::Normalize(shortcut.modifiers.ToNkKey())
                )) {
                return false;
            }

            return true;
        }

        // Handler d'événement clavier
        void OnKeyEvent(const NkKeyEvent& event) {
            if (event.GetState() == NkButtonState::NK_PRESSED) {
                if (MatchesShortcut(event, ShortcutId::Save)) {
                    Document::SaveCurrent();
                    event.MarkHandled();
                }
                // ... autres raccourcis
            }
        }

    private:
        struct ShortcutDef {
            NkKey key;
            NkModifierState modifiers;
        };
        std::unordered_map<ShortcutId, ShortcutDef> mShortcuts;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Mapping DOM codes dans un backend WebAssembly
// -----------------------------------------------------------------------------
/*
    // Dans le backend Web/WASM : gestion de l'événement JavaScript KeyboardEvent
    extern "C" void OnWebKeyDown(const char* code, const char* key, bool repeat) {
        using namespace nkentseu;

        // Conversion du DOM code (layout-independent) vers NkKey
        NkKey nkKey = NkKeycodeMap::NkKeyFromDomCode(code);

        if (nkKey == NkKey::NK_UNKNOWN) {
            // Fallback : tenter de mapper via le caractère (layout-dependent)
            // Note : moins fiable car dépend du layout clavier de l'utilisateur
            nkKey = FallbackKeyFromCharacter(key);
        }

        // Construction de l'événement
        NkKeyEvent event(
            nkKey,
            NkButtonState::NK_PRESSED,
            ParseWebModifiers()  // Parse CtrlKey, ShiftKey, AltKey, MetaKey
        );
        event.SetRepeat(repeat);

        // Dispatch vers le code C++ de l'application
        EventManager::GetInstance().Dispatch(event);
    }

    // Fallback optionnel : mapping caractère → NkKey (layout-dependent)
    static NkKey FallbackKeyFromCharacter(const char* keyStr) {
        if (!keyStr || !*keyStr) return NkKey::NK_UNKNOWN;

        // Exemple très simplifié : uniquement pour les lettres ASCII
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
// Exemple 4 : Gestion des touches de fonction et média multi-plateforme
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkKeycodeMap.h"

    class MediaKeyHandler {
    public:
        // Handler centralisé pour toutes les touches média
        void OnMediaKey(NkKey key, NkButtonState state) {
            using namespace nkentseu;

            if (state != NkButtonState::NK_PRESSED) {
                return;  // Ignorer les releases pour les touches média
            }

            switch (key) {
                case NkKey::NK_MEDIA_PLAY_PAUSE:
                    AudioManager::TogglePlayPause();
                    break;
                case NkKey::NK_MEDIA_NEXT:
                    Playlist::NextTrack();
                    break;
                case NkKey::NK_MEDIA_PREV:
                    Playlist::PrevTrack();
                    break;
                case NkKey::NK_MEDIA_STOP:
                    AudioManager::Stop();
                    break;
                case NkKey::NK_MEDIA_VOLUME_UP:
                    AudioManager::AdjustVolume(+0.1f);
                    break;
                case NkKey::NK_MEDIA_VOLUME_DOWN:
                    AudioManager::AdjustVolume(-0.1f);
                    break;
                case NkKey::NK_MEDIA_MUTE:
                    AudioManager::ToggleMute();
                    break;
                default:
                    // Touches non gérées : logging pour debug
                    NK_FOUNDATION_LOG_DEBUG("Unhandled media key: {}",
                        NkKeyToString(key));
                    break;
            }
        }

        // Intégration dans le dispatcher d'événements
        void Register() {
            EventManager::GetInstance().Subscribe<NkKeyEvent>(
                [this](NkKeyEvent& event) {
                    NkKey key = event.GetKey();
                    if (key >= NkKey::NK_MEDIA_PLAY_PAUSE &&
                        key <= NkKey::NK_MEDIA_MUTE) {
                        OnMediaKey(key, event.GetState());
                        event.MarkHandled();
                    }
                }
            );
        }
    };
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Débogage et logging des conversions de codes clavier
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkKeycodeMap.h"

    class InputDebugger {
    public:
        // Logging détaillé d'un événement clavier pour analyse cross-platform
        static void LogKeyEvent(const NkKeyEvent& event, const char* platform) {
            using namespace nkentseu;

            NK_FOUNDATION_LOG_DEBUG(
                "[{0}] Key={1} ({2}) State={3} Mods={4} Repeat={5}",
                platform,
                NkKeyToString(event.GetKey()),
                static_cast<uint32>(event.GetKey()),
                (event.GetState() == NkButtonState::NK_PRESSED ? "PRESSED" : "RELEASED"),
                event.GetModifiers().ToString(),
                event.IsRepeat() ? "yes" : "no"
            );

            // Si le key est UNKNOWN, tenter de diagnostiquer la source
            if (event.GetKey() == NkKey::NK_UNKNOWN) {
                NK_FOUNDATION_LOG_WARNING(
                    "[{0}] Unknown key detected — verify platform mapping",
                    platform
                );
            }
        }

        // Test unitaire de conversion pour une plateforme donnée
        static void TestPlatformMapping(const char* platformName,
                                        std::function<NkKey(uint32, bool)> converter) {
            using namespace nkentseu;

            // Liste de codes de test représentatifs
            struct TestCase {
                uint32 code;
                bool extended;
                NkKey expected;
                const char* description;
            };

            static const TestCase tests[] = {
                { 0x0D, false, NkKey::NK_ENTER, "Enter" },
                { 0x0D, true,  NkKey::NK_NUMPAD_ENTER, "Numpad Enter" },
                { 0x41, false, NkKey::NK_A, "Letter A" },
                { 0x70, false, NkKey::NK_F1, "F1" },
                { 0x25, false, NkKey::NK_LEFT, "Arrow Left" },
                { 0x11, false, NkKey::NK_LCTRL, "Left Ctrl" },
                { 0x11, true,  NkKey::NK_RCTRL, "Right Ctrl" },
                { 0xFFFFFFFF, false, NkKey::NK_UNKNOWN, "Invalid code" }
            };

            NK_FOUNDATION_LOG_INFO("Testing {0} keycode mapping...", platformName);

            for (const auto& test : tests) {
                NkKey result = converter(test.code, test.extended);
                if (result == test.expected) {
                    NK_FOUNDATION_LOG_DEBUG("  ✓ {0} (0x{1:X}) → {2}",
                        test.description, test.code, NkKeyToString(result));
                } else {
                    NK_FOUNDATION_LOG_ERROR("  ✗ {0} (0x{1:X}) → {2} (expected {3})",
                        test.description, test.code,
                        NkKeyToString(result), NkKeyToString(test.expected));
                }
            }
        }
    };

    // Usage dans les tests d'intégration :
    // InputDebugger::TestPlatformMapping("Win32", NkKeycodeMap::NkKeyFromWin32VK);
    // InputDebugger::TestPlatformMapping("X11", [](uint32 ks, bool) {
    //     return NkKeycodeMap::NkKeyFromX11KeySym(ks);
    // });
*/

// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
    1. AJOUT D'UN NOUVEAU MAPPING PLATEFORME :
       a) Ajouter une méthode statique NkKeyFrom<Platform><CodeType>()
       b) Documenter la source des codes (specs, headers, documentation)
       c) Ajouter des tests unitaires avec des codes représentatifs
       d) Mettre à jour la documentation Doxygen de la classe NkKeycodeMap

    2. GESTION DES CODES INCONNUS :
       - Toujours retourner NkKey::NK_UNKNOWN pour les codes non mappés
       - Logger en DEBUG (pas ERROR) pour éviter le spam en production
       - Fournir un mécanisme de fallback optionnel si pertinent

    3. PERFORMANCE DES CONVERSIONS :
       - Les switch exhaustifs sont optimisés par les compilateurs modernes
       - Pour les tables de recherche (DOM), garder la table static const
       - Éviter les allocations dans les fonctions de mapping (appelées en hot path)

    4. NORMALISATION DES MODIFICATEURS :
       - Utiliser Normalize() et SameKey() pour les raccourcis clavier
       - Documenter clairement quand la distinction gauche/droite est pertinente
         (ex: jeux avec bindings différents pour LCTRL/RCTRL)

    5. TESTS CROSS-PLATFORM :
       - Tester chaque mapping avec des codes représentatifs de chaque catégorie
       - Vérifier les cas limites : codes étendus Win32, keycodes evdev offset
       - Valider que les mêmes touches physiques produisent le même NkKey

    6. ÉVOLUTION DE NkKey :
       - Si de nouvelles valeurs NkKey sont ajoutées, mettre à jour TOUS les mappings
       - Utiliser des assertions statiques ou des tests pour détecter les oublis
       - Documenter les breaking changes dans les notes de version

    7. DÉBOGAGE DES MAPPINGS :
       - Activer NKENTSEU_INPUT_DEBUG pour logger les conversions inconnues
       - Fournir un outil CLI pour tester les conversions interactivement
       - Inclure les tables de mapping dans les builds de debug pour inspection
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================