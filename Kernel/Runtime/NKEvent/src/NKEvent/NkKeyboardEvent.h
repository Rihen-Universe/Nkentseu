// =============================================================================
// NKEvent/NkKeyboardEvent.h
// Hiérarchie complète des événements clavier.
//
// Catégorie : NK_CAT_KEYBOARD | NK_CAT_INPUT
//
// Architecture :
//   - NkKey              : codes touches sémantiques cross-platform (position US-QWERTY)
//   - NkScancode         : codes physiques USB HID (indépendants du layout clavier)
//   - NkModifierState    : état des modificateurs au moment de l'événement
//   - NkKeyboardEvent    : classe de base abstraite pour tous les événements clavier
//   - Classes dérivées   : Press, Repeat, Release, TextInput avec sémantique distincte
//
// Stratégie de conversion cross-platform :
//   Plateforme → Scancode USB HID → NkKey (via NkScancodeToKey)
//   - Win32  : NkScancodeFromWin32(scancode, extended)
//   - Linux  : NkScancodeFromLinux(evdev_keycode) ou NkScancodeFromXKeycode(xkeycode)
//   - macOS  : NkScancodeFromMac(carbon_keyCode)
//   - Web    : NkScancodeFromDOMCode(domCode)
//
// Distinction importante :
//   - NkKeyPressEvent  : touche pressée (position physique) → raccourcis, gameplay
//   - NkTextInputEvent : caractère Unicode produit (après IME/layout) → saisie texte
//
// Exemple d'usage :
//   // Raccourci clavier (utilise NkKeyPressEvent)
//   if (event.Is<NkKeyPressEvent>() && event.GetKey() == NkKey::NK_S
//       && event.HasCtrl()) { Document::Save(); }
//
//   // Saisie de texte (utilise NkTextInputEvent)
//   if (event.Is<NkTextInputEvent>()) {
//       inputBuffer += event.GetUtf8();  // Caractère Unicode correctement encodé
//   }
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_EVENT_NKKEYBOARDEVENT_H
#define NKENTSEU_EVENT_NKKEYBOARDEVENT_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des dépendances internes au projet et des en-têtes standards.
    // Toutes les dépendances utilisent les modules NK* du framework.

    #include "NKEvent/NkEvent.h"                        // Classe de base NkEvent + macros
    #include "NKEvent/NkEventApi.h"                     // Macros d'export NKENTSEU_EVENT_API
    #include "NKContainers/String/NkStringUtils.h"      // Conversion types → NkString
    #include <cstring>                                  // Pour strlen, memcpy si nécessaire
    #include <cstdio>                                   // Pour snprintf dans ToHex
    #include <string>                                   // Pour compatibilité std::string si requis

    namespace nkentseu {

        // =====================================================================
        // NkKey — Codes touches sémantiques cross-platform (position US-QWERTY)
        // =====================================================================

        /**
         * @brief Énumération des touches logiques indépendantes du layout clavier
         *
         * Un NkKey identifie la POSITION physique d'une touche sur un clavier
         * de référence US-QWERTY, indépendamment du layout configuré par l'utilisateur.
         *
         * Exemple de distinction position vs caractère :
         *   - NkKey::NK_Q = touche à la position du 'Q' en QWERTY
         *     Sur un clavier AZERTY, cette même touche produit le caractère 'A'
         *   - NkKey::NK_A = touche à la position du 'A' en QWERTY
         *     Sur un clavier AZERTY, cette même touche produit le caractère 'Q'
         *
         * @note Pour obtenir le CARACTÈRE produit (après layout, Shift, IME...),
         *       utiliser NkTextInputEvent qui transporte le codepoint Unicode.
         *
         * @note Cette énumération est utilisée pour :
         *       - Les raccourcis clavier (Ctrl+S, Alt+F4...)
         *       - Les contrôles de jeu (WASD, flèches, touches de fonction)
         *       - Toute logique dépendant de la POSITION physique de la touche
         *
         * @warning Ne pas utiliser NkKey pour la saisie de texte — utiliser
         *          NkTextInputEvent qui gère correctement l'Unicode et les IME.
         */
        enum class NKENTSEU_EVENT_CLASS_EXPORT NkKey : uint32 {
            NK_UNKNOWN = 0,    ///< Touche non reconnue ou code invalide

            // -----------------------------------------------------------------
            // Groupe : Touches de fonction F1–F24
            // -----------------------------------------------------------------
            NK_ESCAPE,         ///< Touche Échap (Esc)
            NK_F1,             ///< Touche de fonction F1
            NK_F2,             ///< Touche de fonction F2
            NK_F3,             ///< Touche de fonction F3
            NK_F4,             ///< Touche de fonction F4
            NK_F5,             ///< Touche de fonction F5
            NK_F6,             ///< Touche de fonction F6
            NK_F7,             ///< Touche de fonction F7
            NK_F8,             ///< Touche de fonction F8
            NK_F9,             ///< Touche de fonction F9
            NK_F10,            ///< Touche de fonction F10
            NK_F11,            ///< Touche de fonction F11
            NK_F12,            ///< Touche de fonction F12
            NK_F13,            ///< Touche de fonction F13 (claviers étendus)
            NK_F14,            ///< Touche de fonction F14
            NK_F15,            ///< Touche de fonction F15
            NK_F16,            ///< Touche de fonction F16
            NK_F17,            ///< Touche de fonction F17
            NK_F18,            ///< Touche de fonction F18
            NK_F19,            ///< Touche de fonction F19
            NK_F20,            ///< Touche de fonction F20
            NK_F21,            ///< Touche de fonction F21
            NK_F22,            ///< Touche de fonction F22
            NK_F23,            ///< Touche de fonction F23
            NK_F24,            ///< Touche de fonction F24

            // -----------------------------------------------------------------
            // Groupe : Ligne supérieure (chiffres et symboles)
            // -----------------------------------------------------------------
            NK_GRAVE,          ///< Touche `~ (accent grave / tilde)
            NK_NUM1,           ///< Touche 1!
            NK_NUM2,           ///< Touche 2@
            NK_NUM3,           ///< Touche 3#
            NK_NUM4,           ///< Touche 4$
            NK_NUM5,           ///< Touche 5%
            NK_NUM6,           ///< Touche 6^
            NK_NUM7,           ///< Touche 7&
            NK_NUM8,           ///< Touche 8*
            NK_NUM9,           ///< Touche 9(
            NK_NUM0,           ///< Touche 0)
            NK_MINUS,          ///< Touche -_ (moins / underscore)
            NK_EQUALS,         ///< Touche =+ (égal / plus)
            NK_BACK,           ///< Touche Retour arrière (Backspace)

            // -----------------------------------------------------------------
            // Groupe : Rangée QWERTY (première rangée alphabétique)
            // -----------------------------------------------------------------
            NK_TAB,            ///< Touche Tabulation
            NK_Q,              ///< Touche Q (position QWERTY)
            NK_W,              ///< Touche W (position QWERTY)
            NK_E,              ///< Touche E (position QWERTY)
            NK_R,              ///< Touche R (position QWERTY)
            NK_T,              ///< Touche T (position QWERTY)
            NK_Y,              ///< Touche Y (position QWERTY)
            NK_U,              ///< Touche U (position QWERTY)
            NK_I,              ///< Touche I (position QWERTY)
            NK_O,              ///< Touche O (position QWERTY)
            NK_P,              ///< Touche P (position QWERTY)
            NK_LBRACKET,       ///< Touche [{ (crochet ouvrant / accolade)
            NK_RBRACKET,       ///< Touche ]} (crochet fermant / accolade)
            NK_BACKSLASH,      ///< Touche \| (backslash / pipe)

            // -----------------------------------------------------------------
            // Groupe : Rangée ASDF (deuxième rangée alphabétique)
            // -----------------------------------------------------------------
            NK_CAPSLOCK,       ///< Touche Verrouillage majuscules (Caps Lock)
            NK_A,              ///< Touche A (position QWERTY)
            NK_S,              ///< Touche S (position QWERTY)
            NK_D,              ///< Touche D (position QWERTY)
            NK_F,              ///< Touche F (position QWERTY)
            NK_G,              ///< Touche G (position QWERTY)
            NK_H,              ///< Touche H (position QWERTY)
            NK_J,              ///< Touche J (position QWERTY)
            NK_K,              ///< Touche K (position QWERTY)
            NK_L,              ///< Touche L (position QWERTY)
            NK_SEMICOLON,      ///< Touche ;: (point-virgule / deux-points)
            NK_APOSTROPHE,     ///< Touche '" (apostrophe / guillemet)
            NK_ENTER,          ///< Touche Entrée principale (Return)

            // -----------------------------------------------------------------
            // Groupe : Rangée ZXCV (troisième rangée alphabétique)
            // -----------------------------------------------------------------
            NK_LSHIFT,         ///< Touche Majuscule gauche (Shift)
            NK_Z,              ///< Touche Z (position QWERTY)
            NK_X,              ///< Touche X (position QWERTY)
            NK_C,              ///< Touche C (position QWERTY)
            NK_V,              ///< Touche V (position QWERTY)
            NK_B,              ///< Touche B (position QWERTY)
            NK_N,              ///< Touche N (position QWERTY)
            NK_M,              ///< Touche M (position QWERTY)
            NK_COMMA,          ///< Touche ,< (virgule / inférieur)
            NK_PERIOD,         ///< Touche .> (point / supérieur)
            NK_SLASH,          ///< Touche /? (slash / point d'interrogation)
            NK_RSHIFT,         ///< Touche Majuscule droite (Shift)

            // -----------------------------------------------------------------
            // Groupe : Rangée inférieure (modificateurs et espace)
            // -----------------------------------------------------------------
            NK_LCTRL,          ///< Touche Control gauche
            NK_LSUPER,         ///< Touche Super gauche (Win / Cmd / Meta)
            NK_LALT,           ///< Touche Alt gauche
            NK_SPACE,          ///< Touche Espace
            NK_RALT,           ///< Touche Alt droite (AltGr sur claviers internationaux)
            NK_RSUPER,         ///< Touche Super droite (Win / Cmd / Meta)
            NK_MENU,           ///< Touche Menu contextuel (Application)
            NK_RCTRL,          ///< Touche Control droite

            // -----------------------------------------------------------------
            // Groupe : Bloc de navigation et édition
            // -----------------------------------------------------------------
            NK_PRINT_SCREEN,   ///< Touche Impression écran (Print Screen)
            NK_SCROLL_LOCK,    ///< Touche Arrêt défilement (Scroll Lock)
            NK_PAUSE_BREAK,    ///< Touche Pause / Interruption (Pause/Break)
            NK_INSERT,         ///< Touche Insertion (Ins)
            NK_DELETE,         ///< Touche Suppression (Del)
            NK_HOME,           ///< Touche Début (Home)
            NK_END,            ///< Touche Fin (End)
            NK_PAGE_UP,        ///< Touche Page précédente (PgUp)
            NK_PAGE_DOWN,      ///< Touche Page suivante (PgDn)

            // -----------------------------------------------------------------
            // Groupe : Flèches directionnelles
            // -----------------------------------------------------------------
            NK_UP,             ///< Flèche haut
            NK_DOWN,           ///< Flèche bas
            NK_LEFT,           ///< Flèche gauche
            NK_RIGHT,          ///< Flèche droite

            // -----------------------------------------------------------------
            // Groupe : Pavé numérique (Numpad)
            // -----------------------------------------------------------------
            NK_NUM_LOCK,       ///< Touche Verrouillage numérique (Num Lock)
            NK_NUMPAD_DIV,     ///< Touche / du pavé numérique
            NK_NUMPAD_MUL,     ///< Touche * du pavé numérique
            NK_NUMPAD_SUB,     ///< Touche - du pavé numérique
            NK_NUMPAD_ADD,     ///< Touche + du pavé numérique
            NK_NUMPAD_ENTER,   ///< Touche Entrée du pavé numérique (distincte de NK_ENTER)
            NK_NUMPAD_DOT,     ///< Touche . du pavé numérique
            NK_NUMPAD_0,       ///< Touche 0 du pavé numérique
            NK_NUMPAD_1,       ///< Touche 1 du pavé numérique
            NK_NUMPAD_2,       ///< Touche 2 du pavé numérique
            NK_NUMPAD_3,       ///< Touche 3 du pavé numérique
            NK_NUMPAD_4,       ///< Touche 4 du pavé numérique
            NK_NUMPAD_5,       ///< Touche 5 du pavé numérique
            NK_NUMPAD_6,       ///< Touche 6 du pavé numérique
            NK_NUMPAD_7,       ///< Touche 7 du pavé numérique
            NK_NUMPAD_8,       ///< Touche 8 du pavé numérique
            NK_NUMPAD_9,       ///< Touche 9 du pavé numérique
            NK_NUMPAD_EQUALS,  ///< Touche = du pavé numérique (macOS / claviers étendus)

            // -----------------------------------------------------------------
            // Groupe : Touches multimédia
            // -----------------------------------------------------------------
            NK_MEDIA_PLAY_PAUSE,   ///< Touche Lecture/Pause multimédia
            NK_MEDIA_STOP,         ///< Touche Arrêt multimédia
            NK_MEDIA_NEXT,         ///< Touche Piste suivante
            NK_MEDIA_PREV,         ///< Touche Piste précédente
            NK_MEDIA_VOLUME_UP,    ///< Touche Augmenter le volume
            NK_MEDIA_VOLUME_DOWN,  ///< Touche Diminuer le volume
            NK_MEDIA_MUTE,         ///< Touche Couper le son (Mute)

            // -----------------------------------------------------------------
            // Groupe : Touches navigateur web
            // -----------------------------------------------------------------
            NK_BROWSER_BACK,       ///< Touche Précédent (historique)
            NK_BROWSER_FORWARD,    ///< Touche Suivant (historique)
            NK_BROWSER_REFRESH,    ///< Touche Actualiser (Refresh)
            NK_BROWSER_HOME,       ///< Touche Page d'accueil
            NK_BROWSER_SEARCH,     ///< Touche Recherche
            NK_BROWSER_FAVORITES,  ///< Touche Favoris / Signets

            // -----------------------------------------------------------------
            // Groupe : International / IME (Input Method Editor)
            // -----------------------------------------------------------------
            NK_KANA,           ///< Touche Kana (claviers japonais)
            NK_KANJI,          ///< Touche Kanji (claviers japonais/chinois)
            NK_CONVERT,        ///< Touche Conversion (IME asiatiques)
            NK_NONCONVERT,     ///< Touche Non-conversion (IME asiatiques)
            NK_HANGUL,         ///< Touche Hangul (claviers coréens)
            NK_HANJA,          ///< Touche Hanja (claviers coréens)

            // -----------------------------------------------------------------
            // Groupe : Divers / OEM / Réservé
            // -----------------------------------------------------------------
            NK_SLEEP,          ///< Touche Veille système
            NK_CLEAR,          ///< Touche Effacer (Clear)
            NK_SEPARATOR,      ///< Touche Séparateur (généralement pavé numérique)
            NK_OEM_1,          ///< Touche OEM spécifique 1 (dépend de la plateforme)
            NK_OEM_2,          ///< Touche OEM spécifique 2
            NK_OEM_3,          ///< Touche OEM spécifique 3
            NK_OEM_4,          ///< Touche OEM spécifique 4
            NK_OEM_5,          ///< Touche OEM spécifique 5
            NK_OEM_6,          ///< Touche OEM spécifique 6
            NK_OEM_7,          ///< Touche OEM spécifique 7
            NK_OEM_8,          ///< Touche OEM spécifique 8

            NK_KEY_MAX         ///< Sentinelle : nombre de valeurs valides (pour validation)
        };

        // =====================================================================
        // Aliases NkKey pour les opérations du pavé numérique (compatibilité Win32 VK_*)
        // Placés hors de l'enum pour ne pas perturber l'auto-incrément de NK_KEY_MAX.
        // Distinction explicite :
        //   NK_ADD      ≠ NK_EQUALS  — NK_ADD = pavé +, NK_EQUALS = touche = du clavier principal
        //   NK_SUBTRACT ≠ NK_MINUS   — NK_SUBTRACT = pavé -, NK_MINUS = touche - du clavier principal
        //   NK_DECIMAL  ≠ NK_PERIOD  — NK_DECIMAL = pavé ., NK_PERIOD = touche . du clavier principal
        // =====================================================================
        inline constexpr NkKey NK_ADD      = NkKey::NK_NUMPAD_ADD;  ///< Alias: pavé + (distinct de NK_EQUALS)
        inline constexpr NkKey NK_SUBTRACT = NkKey::NK_NUMPAD_SUB;  ///< Alias: pavé - (distinct de NK_MINUS)
        inline constexpr NkKey NK_MULTIPLY = NkKey::NK_NUMPAD_MUL;  ///< Alias: pavé *
        inline constexpr NkKey NK_DIVIDE   = NkKey::NK_NUMPAD_DIV;  ///< Alias: pavé /
        inline constexpr NkKey NK_DECIMAL  = NkKey::NK_NUMPAD_DOT;  ///< Alias: pavé . (distinct de NK_PERIOD)

        // =====================================================================
        // Utilitaires pour NkKey — fonctions helper de classification et formatage
        // =====================================================================

        /// @brief Convertit une valeur NkKey en chaîne lisible pour le débogage
        /// @param key La valeur de touche à convertir
        /// @return Pointeur vers une chaîne littérale statique (ex: "NK_A", "NK_SPACE")
        /// @note Fonction noexcept pour usage dans les contextes critiques
        /// @note Ne pas libérer la chaîne retournée — mémoire statique
        const char* NkKeyToString(NkKey key) noexcept;

        /// @brief Vérifie si une touche est un modificateur principal
        /// @param key La touche à tester
        /// @return true si key est l'un des : LCTRL, RCTRL, LALT, RALT, LSHIFT, RSHIFT, LSUPER, RSUPER
        /// @note AltGr est considéré séparément via NkModifierState::altGr
        bool NkKeyIsModifier(NkKey key) noexcept;

        /// @brief Vérifie si une touche appartient au pavé numérique
        /// @param key La touche à tester
        /// @return true si key est l'un des NK_NUMPAD_* (y compris NK_NUMPAD_ENTER, NK_NUM_LOCK)
        /// @note NK_NUM_LOCK est inclus car il contrôle l'état du pavé numérique
        bool NkKeyIsNumpad(NkKey key) noexcept;

        /// @brief Vérifie si une touche est une touche de fonction
        /// @param key La touche à tester
        /// @return true si key est dans la plage NK_F1 à NK_F24
        bool NkKeyIsFunctionKey(NkKey key) noexcept;

        // =====================================================================
        // NkScancode — Codes physiques USB HID (indépendants du layout)
        // =====================================================================

        /**
         * @brief Énumération des scancodes physiques selon la spécification USB HID
         *
         * Les valeurs de cette énumération correspondent aux USB HID Usage IDs
         * définis dans la spécification HID 1.11, Table 10 : Keyboard/Keypad Page.
         *
         * Caractéristiques clés :
         *   - Indépendant du layout clavier (AZERTY, QWERTY, Dvorak, etc.)
         *   - Identifie la POSITION physique de la touche, pas le caractère produit
         *   - Utilisé comme couche intermédiaire pour la conversion cross-platform
         *
         * Flux de conversion typique :
         *   1. Plateforme (VK_, KeySym, keyCode...) → NkScancode (via NkScancodeFrom*)
         *   2. NkScancode → NkKey (via NkScancodeToKey) pour logique cross-platform
         *
         * @note Certaines valeurs sont dans la plage étendue (0xE0–0xFF) pour
         *       les modificateurs, conformément à la spec USB HID.
         *
         * @note Les touches multimédia utilisent des Usage IDs de la page Consumer
         *       (ex: 0xE0CD pour Play/Pause), distincts de la page Keyboard.
         */
        enum class NKENTSEU_EVENT_CLASS_EXPORT NkScancode : uint32 {
            NK_SC_UNKNOWN = 0,    ///< Scancode inconnu ou non mappé

            // -----------------------------------------------------------------
            // Lettres (ordre HID, pas alphabétique — basé sur position physique)
            // -----------------------------------------------------------------
            NK_SC_A = 0x04,    ///< Scancode USB HID pour la touche A (position QWERTY)
            NK_SC_B,           ///< Scancode USB HID pour la touche B
            NK_SC_C,           ///< Scancode USB HID pour la touche C
            NK_SC_D,           ///< Scancode USB HID pour la touche D
            NK_SC_E,           ///< Scancode USB HID pour la touche E
            NK_SC_F,           ///< Scancode USB HID pour la touche F
            NK_SC_G,           ///< Scancode USB HID pour la touche G
            NK_SC_H,           ///< Scancode USB HID pour la touche H
            NK_SC_I,           ///< Scancode USB HID pour la touche I
            NK_SC_J,           ///< Scancode USB HID pour la touche J
            NK_SC_K,           ///< Scancode USB HID pour la touche K
            NK_SC_L,           ///< Scancode USB HID pour la touche L
            NK_SC_M,           ///< Scancode USB HID pour la touche M
            NK_SC_N,           ///< Scancode USB HID pour la touche N
            NK_SC_O,           ///< Scancode USB HID pour la touche O
            NK_SC_P,           ///< Scancode USB HID pour la touche P
            NK_SC_Q,           ///< Scancode USB HID pour la touche Q
            NK_SC_R,           ///< Scancode USB HID pour la touche R
            NK_SC_S,           ///< Scancode USB HID pour la touche S
            NK_SC_T,           ///< Scancode USB HID pour la touche T
            NK_SC_U,           ///< Scancode USB HID pour la touche U
            NK_SC_V,           ///< Scancode USB HID pour la touche V
            NK_SC_W,           ///< Scancode USB HID pour la touche W
            NK_SC_X,           ///< Scancode USB HID pour la touche X
            NK_SC_Y = 0x1C,    ///< Scancode USB HID pour la touche Y
            NK_SC_Z = 0x1D,    ///< Scancode USB HID pour la touche Z

            // -----------------------------------------------------------------
            // Chiffres de la ligne supérieure (1–0)
            // -----------------------------------------------------------------
            NK_SC_1 = 0x1E,    ///< Scancode USB HID pour la touche 1
            NK_SC_2,           ///< Scancode USB HID pour la touche 2
            NK_SC_3,           ///< Scancode USB HID pour la touche 3
            NK_SC_4,           ///< Scancode USB HID pour la touche 4
            NK_SC_5,           ///< Scancode USB HID pour la touche 5
            NK_SC_6,           ///< Scancode USB HID pour la touche 6
            NK_SC_7,           ///< Scancode USB HID pour la touche 7
            NK_SC_8,           ///< Scancode USB HID pour la touche 8
            NK_SC_9,           ///< Scancode USB HID pour la touche 9
            NK_SC_0 = 0x27,    ///< Scancode USB HID pour la touche 0

            // -----------------------------------------------------------------
            // Touches de contrôle principales et ponctuation
            // -----------------------------------------------------------------
            NK_SC_ENTER      = 0x28, ///< Enter principal (distinct de NK_SC_NUMPAD_ENTER)
            NK_SC_ESCAPE     = 0x29, ///< Échap (Esc)
            NK_SC_BACKSPACE  = 0x2A, ///< Retour arrière (Backspace)
            NK_SC_TAB        = 0x2B, ///< Tabulation
            NK_SC_SPACE      = 0x2C, ///< Espace
            NK_SC_MINUS      = 0x2D, ///< Moins / underscore (-_)
            NK_SC_EQUALS     = 0x2E, ///< Égal / plus (=+)
            NK_SC_LBRACKET   = 0x2F, ///< Crochet ouvrant / accolade ([{)
            NK_SC_RBRACKET   = 0x30, ///< Crochet fermant / accolade (]})
            NK_SC_BACKSLASH  = 0x31, ///< Backslash / pipe (\|)
            NK_SC_NONUS_HASH = 0x32, ///< Hash / tilde (#~) — ISO non-US (entre Enter et LShift)
            NK_SC_SEMICOLON  = 0x33, ///< Point-virgule / deux-points (;:)
            NK_SC_APOSTROPHE = 0x34, ///< Apostrophe / guillemet ('")
            NK_SC_GRAVE      = 0x35, ///< Accent grave / tilde (`~)
            NK_SC_COMMA      = 0x36, ///< Virgule / inférieur (,<)
            NK_SC_PERIOD     = 0x37, ///< Point / supérieur (.>)
            NK_SC_SLASH      = 0x38, ///< Slash / point d'interrogation (/?)

            // -----------------------------------------------------------------
            // Touches de fonction F1–F12
            // -----------------------------------------------------------------
            NK_SC_CAPS_LOCK = 0x39,  ///< Verrouillage majuscules (Caps Lock)
            NK_SC_F1  = 0x3A,        ///< Touche de fonction F1
            NK_SC_F2,                ///< Touche de fonction F2
            NK_SC_F3,                ///< Touche de fonction F3
            NK_SC_F4,                ///< Touche de fonction F4
            NK_SC_F5,                ///< Touche de fonction F5
            NK_SC_F6,                ///< Touche de fonction F6
            NK_SC_F7,                ///< Touche de fonction F7
            NK_SC_F8,                ///< Touche de fonction F8
            NK_SC_F9,                ///< Touche de fonction F9
            NK_SC_F10,               ///< Touche de fonction F10
            NK_SC_F11,               ///< Touche de fonction F11
            NK_SC_F12,               ///< Touche de fonction F12

            // -----------------------------------------------------------------
            // Bloc de contrôle et navigation
            // -----------------------------------------------------------------
            NK_SC_PRINT_SCREEN = 0x46, ///< Impression écran (Print Screen)
            NK_SC_SCROLL_LOCK  = 0x47, ///< Arrêt défilement (Scroll Lock)
            NK_SC_PAUSE        = 0x48, ///< Pause / Interruption
            NK_SC_INSERT       = 0x49, ///< Insertion (Ins)
            NK_SC_HOME         = 0x4A, ///< Début (Home)
            NK_SC_PAGE_UP      = 0x4B, ///< Page précédente (PgUp)
            NK_SC_DELETE       = 0x4C, ///< Suppression (Del)
            NK_SC_END          = 0x4D, ///< Fin (End)
            NK_SC_PAGE_DOWN    = 0x4E, ///< Page suivante (PgDn)
            NK_SC_RIGHT        = 0x4F, ///< Flèche droite
            NK_SC_LEFT         = 0x50, ///< Flèche gauche
            NK_SC_DOWN         = 0x51, ///< Flèche bas
            NK_SC_UP           = 0x52, ///< Flèche haut

            // -----------------------------------------------------------------
            // Pavé numérique (Numpad)
            // -----------------------------------------------------------------
            NK_SC_NUM_LOCK      = 0x53, ///< Verrouillage numérique (Num Lock)
            NK_SC_NUMPAD_DIV    = 0x54, ///< Division du pavé numérique (/)
            NK_SC_NUMPAD_MUL    = 0x55, ///< Multiplication du pavé numérique (*)
            NK_SC_NUMPAD_SUB    = 0x56, ///< Soustraction du pavé numérique (-)
            NK_SC_NUMPAD_ADD    = 0x57, ///< Addition du pavé numérique (+)
            NK_SC_NUMPAD_ENTER  = 0x58, ///< Enter du pavé numérique (distinct de NK_SC_ENTER)
            NK_SC_NUMPAD_1      = 0x59, ///< Touche 1 du pavé numérique
            NK_SC_NUMPAD_2,             ///< Touche 2 du pavé numérique
            NK_SC_NUMPAD_3,             ///< Touche 3 du pavé numérique
            NK_SC_NUMPAD_4,             ///< Touche 4 du pavé numérique
            NK_SC_NUMPAD_5,             ///< Touche 5 du pavé numérique
            NK_SC_NUMPAD_6,             ///< Touche 6 du pavé numérique
            NK_SC_NUMPAD_7,             ///< Touche 7 du pavé numérique
            NK_SC_NUMPAD_8,             ///< Touche 8 du pavé numérique
            NK_SC_NUMPAD_9,             ///< Touche 9 du pavé numérique
            NK_SC_NUMPAD_0      = 0x62, ///< Touche 0 du pavé numérique
            NK_SC_NUMPAD_DOT    = 0x63, ///< Point décimal du pavé numérique (.)

            // -----------------------------------------------------------------
            // Aliases courts pour les opérations du pavé numérique
            // Distinction explicite vis-à-vis des touches identiques du clavier principal :
            //   NK_SC_ADD      ≠ NK_SC_EQUALS  (0x2E)  — deux touches physiques distinctes
            //   NK_SC_SUBTRACT ≠ NK_SC_MINUS   (0x2D)  — idem
            //   NK_SC_MULTIPLY : pas d'équivalent direct sur la rangée principale
            //   NK_SC_DIVIDE   : pas d'équivalent direct sur la rangée principale
            //   NK_SC_DECIMAL  ≠ NK_SC_PERIOD  (0x37)  — deux touches physiques distinctes
            // Compatible Win32 (VK_ADD / VK_SUBTRACT / VK_MULTIPLY / VK_DIVIDE / VK_DECIMAL)
            // -----------------------------------------------------------------
            NK_SC_ADD      = NK_SC_NUMPAD_ADD,  ///< Alias: pavé numérique + (distinct de NK_SC_EQUALS)
            NK_SC_SUBTRACT = NK_SC_NUMPAD_SUB,  ///< Alias: pavé numérique - (distinct de NK_SC_MINUS)
            NK_SC_MULTIPLY = NK_SC_NUMPAD_MUL,  ///< Alias: pavé numérique *
            NK_SC_DIVIDE   = NK_SC_NUMPAD_DIV,  ///< Alias: pavé numérique /
            NK_SC_DECIMAL  = NK_SC_NUMPAD_DOT,  ///< Alias: pavé numérique . (distinct de NK_SC_PERIOD)

            // -----------------------------------------------------------------
            // Touches ISO et additionnelles
            // -----------------------------------------------------------------
            NK_SC_NONUS_BACKSLASH = 0x64, ///< Backslash ISO (entre LShift et Z — claviers européens)
            NK_SC_APPLICATION     = 0x65, ///< Touche Menu / Application (clic droit clavier)
            NK_SC_POWER           = 0x66, ///< Touche Power / Mise en veille
            NK_SC_NUMPAD_EQUALS   = 0x67, ///< Égal du pavé numérique (=) — macOS / claviers étendus

            // -----------------------------------------------------------------
            // Touches de fonction étendues F13–F24
            // -----------------------------------------------------------------
            NK_SC_F13 = 0x68,    ///< Touche de fonction F13
            NK_SC_F14,           ///< Touche de fonction F14
            NK_SC_F15,           ///< Touche de fonction F15
            NK_SC_F16,           ///< Touche de fonction F16
            NK_SC_F17,           ///< Touche de fonction F17
            NK_SC_F18,           ///< Touche de fonction F18
            NK_SC_F19,           ///< Touche de fonction F19
            NK_SC_F20,           ///< Touche de fonction F20
            NK_SC_F21,           ///< Touche de fonction F21
            NK_SC_F22,           ///< Touche de fonction F22
            NK_SC_F23,           ///< Touche de fonction F23
            NK_SC_F24,           ///< Touche de fonction F24

            // -----------------------------------------------------------------
            // Touches multimédia (Consumer Usage Page — IDs étendus)
            // -----------------------------------------------------------------
            NK_SC_MEDIA_PLAY_PAUSE  = 0xE0CD, ///< Lecture/Pause multimédia
            NK_SC_MEDIA_STOP        = 0xE0B7, ///< Arrêt multimédia
            NK_SC_MEDIA_NEXT        = 0xE0B5, ///< Piste suivante
            NK_SC_MEDIA_PREV        = 0xE0B6, ///< Piste précédente

            // -----------------------------------------------------------------
            // Touches de contrôle supplémentaires (0x74–0x81)
            // -----------------------------------------------------------------
            NK_SC_EXECUTE = 0x74,  ///< Touche Exécuter
            NK_SC_HELP = 0x75,     ///< Touche Aide (Help)
            NK_SC_MENU = 0x76,     ///< Touche Menu (distinct de Application)
            NK_SC_SELECT = 0x77,   ///< Touche Sélection
            NK_SC_STOP = 0x78,     ///< Touche Stop
            NK_SC_AGAIN = 0x79,    ///< Touche Recommencer (Again)
            NK_SC_UNDO = 0x7A,     ///< Touche Annuler (Undo)
            NK_SC_CUT = 0x7B,      ///< Touche Couper (Cut)
            NK_SC_COPY = 0x7C,     ///< Touche Copier (Copy)
            NK_SC_PASTE = 0x7D,    ///< Touche Coller (Paste)
            NK_SC_FIND = 0x7E,     ///< Touche Rechercher (Find)
            NK_SC_MUTE = 0x7F,     ///< Touche Mute (couper le son)
            NK_SC_VOLUME_UP = 0x80,    ///< Augmenter le volume
            NK_SC_VOLUME_DOWN = 0x81,  ///< Diminuer le volume

            // -----------------------------------------------------------------
            // Modificateurs (plage 0xE0–0xE7 — Usage IDs réservés HID)
            // -----------------------------------------------------------------
            NK_SC_LCTRL  = 0xE0,   ///< Control gauche
            NK_SC_LSHIFT = 0xE1,   ///< Shift gauche
            NK_SC_LALT   = 0xE2,   ///< Alt gauche
            NK_SC_LSUPER = 0xE3,   ///< Super gauche (Win / Cmd / Meta)
            NK_SC_RCTRL  = 0xE4,   ///< Control droit
            NK_SC_RSHIFT = 0xE5,   ///< Shift droit
            NK_SC_RALT   = 0xE6,   ///< Alt droit (AltGr sur claviers internationaux)
            NK_SC_RSUPER = 0xE7,   ///< Super droit (Win / Cmd / Meta)

            NK_SC_MAX = 0x100      ///< Sentinelle : limite supérieure valide des scancodes
        };

        // =====================================================================
        // Conversions cross-platform : Scancode USB HID ↔ codes natifs
        // =====================================================================

        /// @brief Convertit un scancode USB HID en chaîne lisible pour le débogage
        /// @param sc Le scancode à convertir
        /// @return Pointeur vers une chaîne littérale statique (ex: "SC_A", "SC_ENTER")
        /// @note Fonction noexcept pour usage dans les contextes critiques
        const char* NkScancodeToString(NkScancode sc) noexcept;

        /// @brief Convertit un scancode USB HID vers son équivalent NkKey (US-QWERTY)
        /// @param sc Le scancode USB HID à convertir
        /// @return NkKey correspondant à la position physique (layout US-QWERTY de référence)
        /// @note Cette fonction est le cœur du mapping cross-platform : elle garantit
        ///       que la même touche physique produit le même NkKey sur toutes les plateformes
        NkKey NkScancodeToKey(NkScancode sc) noexcept;

        /// @brief Convertit un scancode PS/2 Win32 vers un scancode USB HID
        /// @param win32Scancode Bits 16–23 du LPARAM de WM_KEYDOWN (scancode brut)
        /// @param extended Bit 24 du LPARAM (true si touche étendue, préfixe 0xE0)
        /// @return NkScancode USB HID équivalent, ou NK_SC_UNKNOWN si non mappé
        /// @note Les touches étendues (flèches, Insert, Delete, Ctrl/Alt droits...)
        ///       ont un scancode PS/2 préfixé par 0xE0 — ce bit doit être passé séparément
        NkScancode NkScancodeFromWin32(uint32 win32Scancode, bool extended) noexcept;

        /// @brief Convertit un keycode Linux evdev vers un scancode USB HID
        /// @param linuxKeycode Code clavier evdev (0–255, issu de /usr/include/linux/input-event-codes.h)
        /// @return NkScancode USB HID équivalent, ou NK_SC_UNKNOWN si non mappé
        /// @note Pour XCB/XLib, utiliser d'abord NkScancodeFromXKeycode() qui gère
        ///       l'offset historique (xkeycode = evdev + 8)
        NkScancode NkScancodeFromLinux(uint32 linuxKeycode) noexcept;

        /// @brief Convertit un keycode X11 (XCB/XLib) vers un scancode USB HID
        /// @param xkeycode Code clavier X11 (1–255, issu de xcb_keycode_t ou KeySym)
        /// @return NkScancode USB HID équivalent, ou NK_SC_UNKNOWN si non mappé
        /// @note Cette fonction soustrait automatiquement l'offset de 8 pour convertir
        ///       vers le keycode evdev avant de mapper vers USB HID
        NkScancode NkScancodeFromXKeycode(uint32 xkeycode) noexcept;

        /// @brief Convertit un keyCode macOS Carbon vers un scancode USB HID
        /// @param macKeycode Code clavier Carbon (0–127, issu de NSEvent.keyCode)
        /// @return NkScancode USB HID équivalent, ou NK_SC_UNKNOWN si non mappé
        /// @note Les keyCodes macOS sont déjà basés sur la position physique (US-QWERTY),
        ///       ce qui simplifie le mapping direct vers USB HID
        NkScancode NkScancodeFromMac(uint32 macKeycode) noexcept;

        /// @brief Convertit un DOM code Web vers un scancode USB HID
        /// @param domCode Chaîne KeyboardEvent.code (ex: "KeyA", "Space", "ArrowLeft")
        /// @return NkScancode USB HID équivalent, ou NK_SC_UNKNOWN si non reconnu
        /// @note Les codes DOM sont layout-independent, idéaux pour un mapping fiable
        /// @note Cette fonction est utilisée dans le backend WebAssembly/WASM
        NkScancode NkScancodeFromDOMCode(const char* domCode) noexcept;

        // =====================================================================
        // NkModifierState — État des modificateurs clavier au moment de l'événement
        // =====================================================================

        /**
         * @brief Structure représentant l'état combiné des modificateurs clavier
         *
         * Cette structure capture quels modificateurs sont actifs au moment
         * où un événement clavier est généré, permettant de détecter des
         * combinaisons comme Ctrl+Shift+A ou AltGr+E.
         *
         * Modificateurs supportés :
         *   - ctrl    : LCtrl OU RCtrl enfoncé (indistinct)
         *   - alt     : LAlt enfoncé (hors AltGr — voir altGr séparément)
         *   - shift   : LShift OU RShift enfoncé (indistinct)
         *   - super   : LSUPER OU RSUPER enfoncé (Win / Cmd / Meta)
         *   - altGr   : AltGr enfoncé (distinct de alt — claviers internationaux)
         *   - numLock : Verrou numérique actif (affecte le pavé numérique)
         *   - capLock : Verrou majuscules actif (affecte la casse des lettres)
         *   - scrLock : Verrou défilement actif (rarement utilisé aujourd'hui)
         *
         * @note Les modificateurs "gauche/droite" sont fusionnés pour ctrl, shift, super
         *       afin de simplifier la détection de raccourcis. Pour une distinction
         *       fine (ex: jeu avec bindings différents pour LCTRL/RCTRL), utiliser
         *       NkKeyPressEvent::GetKey() qui retourne NK_LCTRL ou NK_RCTRL.
         *
         * @note AltGr est traité séparément de alt car il a une sémantique différente :
         *       AltGr+e produit '€' sur un clavier français, tandis que Alt+e n'a
         *       généralement pas d'effet de composition de caractère.
         */
        struct NKENTSEU_EVENT_CLASS_EXPORT NkModifierState {
            bool ctrl    = false;   ///< true si LCtrl OU RCtrl est enfoncé
            bool alt     = false;   ///< true si LAlt est enfoncé (hors AltGr)
            bool shift   = false;   ///< true si LShift OU RShift est enfoncé
            bool super   = false;   ///< true si LSUPER OU RSUPER est enfoncé (Win/Cmd/Meta)
            bool altGr   = false;   ///< true si AltGr est enfoncé (claviers internationaux)
            bool numLock = false;   ///< true si le verrou numérique est actif
            bool capLock = false;   ///< true si le verrou majuscules est actif
            bool scrLock = false;   ///< true si le verrou défilement est actif

            /// @brief Constructeur par défaut — tous les modificateurs à false
            NkModifierState() = default;

            /// @brief Constructeur rapide pour les modificateurs principaux
            /// @param ctrl État du modificateur Control
            /// @param alt État du modificateur Alt (hors AltGr)
            /// @param shift État du modificateur Shift
            /// @param super État du modificateur Super (Win/Cmd/Meta) — false par défaut
            /// @note altGr, numLock, capLock, scrLock restent à false avec ce constructeur
            NKENTSEU_EVENT_API_INLINE NkModifierState(
                bool ctrl,
                bool alt,
                bool shift,
                bool super = false
            ) noexcept : ctrl(ctrl), alt(alt), shift(shift), super(super) {
            }

            /// @brief Vérifie si au moins un modificateur principal est actif
            /// @return true si ctrl, alt, shift, super ou altGr est true
            /// @note Ne considère pas numLock, capLock, scrLock comme "modificateurs principaux"
            NKENTSEU_EVENT_API_INLINE bool Any() const noexcept {
                return ctrl || alt || shift || super || altGr;
            }

            /// @brief Vérifie si aucun modificateur principal n'est actif
            /// @return true si tous les modificateurs principaux sont false
            /// @note Équivalent à !Any() — fourni pour lisibilité du code
            NKENTSEU_EVENT_API_INLINE bool IsNone() const noexcept {
                return !Any();
            }

            /// @brief Opérateur d'égalité — compare les modificateurs principaux uniquement
            /// @param o Autre instance de NkModifierState à comparer
            /// @return true si ctrl, alt, shift, super et altGr sont identiques
            /// @note Ne compare pas numLock, capLock, scrLock — considérés comme états, pas modificateurs
            NKENTSEU_EVENT_API_INLINE bool operator==(const NkModifierState& o) const noexcept {
                return ctrl  == o.ctrl
                    && alt   == o.alt
                    && shift == o.shift
                    && super == o.super
                    && altGr == o.altGr;
            }

            /// @brief Opérateur d'inégalité — inverse de operator==
            /// @param o Autre instance de NkModifierState à comparer
            /// @return true si les modificateurs principaux diffèrent
            NKENTSEU_EVENT_API_INLINE bool operator!=(const NkModifierState& o) const noexcept {
                return !(*this == o);
            }

            /// @brief Convertit l'état des modificateurs en chaîne lisible pour le débogage
            /// @return NkString contenant les noms des modificateurs actifs, séparés par '+'
            /// @note Exemples : "Ctrl+Shift", "AltGr", "None" si aucun modificateur
            /// @note L'ordre est fixe : Ctrl, Alt, Shift, Super, AltGr pour cohérence
            NKENTSEU_EVENT_API NkString ToString() const {
                NkString s;
                if (ctrl) {
                    s += "Ctrl+";
                }
                if (alt) {
                    s += "Alt+";
                }
                if (shift) {
                    s += "Shift+";
                }
                if (super) {
                    s += "Super+";
                }
                if (altGr) {
                    s += "AltGr+";
                }
                if (s.Empty()) {
                    return "None";
                }
                s.PopBack();  // Supprime le dernier '+' excédentaire
                return s;
            }
        };

        // =====================================================================
        // NkKeyboardEvent — Classe de base abstraite pour événements clavier
        // =====================================================================

        /**
         * @brief Classe de base polymorphe pour tous les événements liés au clavier
         *
         * Catégories associées : NK_CAT_KEYBOARD | NK_CAT_INPUT
         *
         * Cette classe sert de point d'ancrage commun pour tous les événements
         * de saisie clavier : pression, répétition, relâchement, et saisie de texte.
         *
         * @note Toutes les classes dérivées héritent automatiquement des deux
         *       catégories via la surcharge de GetCategoryFlags().
         *
         * @note Cette classe transporte à la fois :
         *       - La touche logique (NkKey) pour les raccourcis et contrôles
         *       - Le scancode physique (NkScancode) pour un mapping précis
         *       - L'état des modificateurs (NkModifierState) pour les combinaisons
         *       - Le code natif brut (uint32) pour un accès plateforme-spécifique si nécessaire
         *
         * @note Pour la saisie de texte Unicode, préférer NkTextInputEvent qui
         *       gère correctement les IME, les compositions et l'encodage UTF-8.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkKeyboardEvent : public NkEvent {
            public:
                /// @brief Retourne les flags de catégorie combinés (KEYBOARD + INPUT)
                /// @return uint32 avec les bits NK_CAT_KEYBOARD et NK_CAT_INPUT activés
                /// @note Permet le filtrage par catégorie clavier OU entrée générique
                NKENTSEU_EVENT_API uint32 GetCategoryFlags() const override {
                    return static_cast<uint32>(NkEventCategory::NK_CAT_KEYBOARD)
                         | static_cast<uint32>(NkEventCategory::NK_CAT_INPUT);
                }

                // -------------------------------------------------------------
                // Accesseurs communs à tous les événements clavier
                // -------------------------------------------------------------

                /// @brief Retourne la touche logique (position US-QWERTY)
                /// @return NkKey représentant la touche pressée (ex: NK_A, NK_ENTER)
                /// @note Utiliser pour les raccourcis et contrôles indépendants du layout
                NKENTSEU_EVENT_API_INLINE NkKey GetKey() const noexcept {
                    return mKey;
                }

                /// @brief Retourne le code physique USB HID de la touche
                /// @return NkScancode représentant le scancode USB (ex: NK_SC_A)
                /// @note Utiliser pour un mapping bas-niveau ou du debugging précis
                NKENTSEU_EVENT_API_INLINE NkScancode GetScancode() const noexcept {
                    return mScancode;
                }

                /// @brief Retourne l'état des modificateurs au moment de l'événement
                /// @return NkModifierState contenant ctrl, alt, shift, super, altGr...
                /// @note Utiliser pour détecter des combinaisons comme Ctrl+Shift+A
                NKENTSEU_EVENT_API_INLINE NkModifierState GetModifiers() const noexcept {
                    return mModifiers;
                }

                /// @brief Retourne le code natif brut de la plateforme source
                /// @return uint32 représentant VK_, KeySym, keyCode, etc. selon la plateforme
                /// @note Utiliser uniquement pour du debugging ou un fallback plateforme-spécifique
                NKENTSEU_EVENT_API_INLINE uint32 GetNativeKey() const noexcept {
                    return mNativeKey;
                }

                /// @brief Indique si la touche est une touche étendue (préfixe E0 PS/2)
                /// @return true si la touche est dans la plage étendue (flèches, Ctrl droit...)
                /// @note Utilisé principalement pour le backend Win32 — ignoré sur les autres plateformes
                NKENTSEU_EVENT_API_INLINE bool IsExtended() const noexcept {
                    return mExtended;
                }

                // -------------------------------------------------------------
                // Raccourcis pratiques pour les modificateurs courants
                // -------------------------------------------------------------

                /// @brief Vérifie si Control est actif (gauche ou droit)
                /// @return true si mModifiers.ctrl == true
                NKENTSEU_EVENT_API_INLINE bool HasCtrl() const noexcept {
                    return mModifiers.ctrl;
                }

                /// @brief Vérifie si Alt est actif (gauche uniquement, hors AltGr)
                /// @return true si mModifiers.alt == true
                NKENTSEU_EVENT_API_INLINE bool HasAlt() const noexcept {
                    return mModifiers.alt;
                }

                /// @brief Vérifie si Shift est actif (gauche ou droit)
                /// @return true si mModifiers.shift == true
                NKENTSEU_EVENT_API_INLINE bool HasShift() const noexcept {
                    return mModifiers.shift;
                }

                /// @brief Vérifie si Super (Win/Cmd/Meta) est actif
                /// @return true si mModifiers.super == true
                NKENTSEU_EVENT_API_INLINE bool HasSuper() const noexcept {
                    return mModifiers.super;
                }

                /// @brief Vérifie si AltGr est actif (claviers internationaux)
                /// @return true si mModifiers.altGr == true
                /// @note AltGr est distinct de Alt — il permet la saisie de caractères spéciaux
                NKENTSEU_EVENT_API_INLINE bool HasAltGr() const noexcept {
                    return mModifiers.altGr;
                }

            protected:
                /// @brief Constructeur protégé — réservé aux classes dérivées
                /// @param key Touche logique (NkKey) associée à l'événement
                /// @param scancode Code physique USB HID (NkScancode) de la touche
                /// @param mods État des modificateurs au moment de l'événement
                /// @param nativeKey Code natif brut de la plateforme source (VK_, KeySym...)
                /// @param extended true si la touche est étendue (préfixe E0 PS/2)
                /// @param windowId Identifiant de la fenêtre source (0 = événement global)
                NKENTSEU_EVENT_API NkKeyboardEvent(
                    NkKey key,
                    NkScancode scancode,
                    const NkModifierState& mods,
                    uint32 nativeKey,
                    bool extended,
                    uint64 windowId
                ) noexcept
                    : NkEvent(windowId)
                    , mKey(key)
                    , mScancode(scancode)
                    , mModifiers(mods)
                    , mNativeKey(nativeKey)
                    , mExtended(extended) {
                }

                NkKey mKey;                    ///< Touche logique (position US-QWERTY)
                NkScancode mScancode;          ///< Code physique USB HID
                NkModifierState mModifiers;    ///< État des modificateurs actifs
                uint32 mNativeKey;             ///< Code natif brut de la plateforme
                bool mExtended;                ///< true si touche étendue (préfixe E0 PS/2)
        };

        // =====================================================================
        // NkKeyPressEvent — Touche pressée (première frappe)
        // =====================================================================

        /**
         * @brief Émis lorsqu'une touche est pressée pour la première fois
         *
         * Cet événement marque le début d'une interaction clavier :
         *   - Déclenché au moment où la touche est physiquement enfoncée
         *   - Non répété : un appui maintenu génère un seul NkKeyPressEvent
         *   - Utiliser pour les raccourcis, les contrôles de jeu, les actions ponctuelles
         *
         * @note Pour détecter un appui maintenu (auto-repeat généré par l'OS),
         *       écouter NkKeyRepeatEvent qui est émis périodiquement tant que
         *       la touche reste enfoncée.
         *
         * @note La touche est identifiée par sa POSITION (NkKey), pas par le
         *       caractère produit. Pour la saisie de texte, utiliser
         *       NkTextInputEvent qui gère correctement l'Unicode et les IME.
         *
         * @code
         *   // Exemple : raccourci Ctrl+S pour sauvegarder
         *   if (event.Is<NkKeyPressEvent>()) {
         *       auto& keyEvent = static_cast<NkKeyPressEvent&>(event);
         *       if (keyEvent.GetKey() == NkKey::NK_S && keyEvent.HasCtrl()) {
         *           Document::Save();
         *           event.MarkHandled();  // Empêche la propagation
         *       }
         *   }
         * @endcode
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkKeyPressEvent final : public NkKeyboardEvent {
            public:
                /// @brief Associe le type d'événement NK_KEY_PRESSED à cette classe
                NK_EVENT_TYPE_FLAGS(NK_KEY_PRESSED)

                /**
                 * @brief Constructeur avec tous les paramètres d'un événement de pression
                 * @param key Touche logique (NkKey) associée à l'événement
                 * @param scancode Code physique USB HID (par défaut : NK_SC_UNKNOWN)
                 * @param mods État des modificateurs au moment de l'événement (par défaut : aucun)
                 * @param nativeKey Code natif brut de la plateforme (par défaut : 0)
                 * @param extended true si la touche est étendue (par défaut : false)
                 * @param windowId Identifiant de la fenêtre source (par défaut : 0)
                 */
                NKENTSEU_EVENT_API NkKeyPressEvent(
                    NkKey key,
                    NkScancode scancode = NkScancode::NK_SC_UNKNOWN,
                    const NkModifierState& mods = {},
                    uint32 nativeKey = 0,
                    bool extended = false,
                    uint64 windowId = 0
                ) noexcept
                    : NkKeyboardEvent(key, scancode, mods, nativeKey, extended, windowId) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkKeyPressEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant la touche pressée et les modificateurs actifs
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return "KeyPress("
                         + NkString(NkKeyToString(mKey))
                         + " mod=" + mModifiers.ToString() + ")";
                }
        };

        // =====================================================================
        // NkKeyRepeatEvent — Touche maintenue (auto-repeat généré par l'OS)
        // =====================================================================

        /**
         * @brief Émis périodiquement lorsqu'une touche est maintenue enfoncée
         *
         * Cet événement représente l'auto-repeat généré par le système d'exploitation :
         *   - Déclenché après un délai initial (typiquement 500ms) puis à intervalle régulier
         *   - Permet de détecter un appui maintenu pour des actions continues
         *   - Le compteur mRepeatCount indique combien de répétitions se sont produites
         *
         * @note Cet événement n'est émis qu'APRÈS un NkKeyPressEvent initial.
         *       Un appui très court (tap) ne générera que NkKeyPressEvent + NkKeyReleaseEvent.
         *
         * @note Le comportement de l'auto-repeat (délai, fréquence) dépend des
         *       settings utilisateur de l'OS — ne pas compter sur des valeurs exactes
         *       pour la logique gameplay critique.
         *
         * @code
         *   // Exemple : déplacement continu avec les flèches
         *   if (event.Is<NkKeyRepeatEvent>()) {
         *       auto& repeatEvent = static_cast<NkKeyRepeatEvent&>(event);
         *       if (repeatEvent.GetKey() == NkKey::NK_RIGHT) {
         *           player.MoveRight(kRepeatMoveSpeed);
         *       }
         *   }
         * @endcode
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkKeyRepeatEvent final : public NkKeyboardEvent {
            public:
                /// @brief Associe le type d'événement NK_KEY_REPEATED à cette classe
                NK_EVENT_TYPE_FLAGS(NK_KEY_REPEATED)

                /**
                 * @brief Constructeur avec compteur de répétition et paramètres clavier
                 * @param key Touche logique (NkKey) associée à l'événement
                 * @param repeatCount Nombre de répétitions depuis le dernier événement (par défaut : 1)
                 * @param scancode Code physique USB HID (par défaut : NK_SC_UNKNOWN)
                 * @param mods État des modificateurs au moment de l'événement (par défaut : aucun)
                 * @param nativeKey Code natif brut de la plateforme (par défaut : 0)
                 * @param extended true si la touche est étendue (par défaut : false)
                 * @param windowId Identifiant de la fenêtre source (par défaut : 0)
                 */
                NKENTSEU_EVENT_API NkKeyRepeatEvent(
                    NkKey key,
                    uint32 repeatCount = 1,
                    NkScancode scancode = NkScancode::NK_SC_UNKNOWN,
                    const NkModifierState& mods = {},
                    uint32 nativeKey = 0,
                    bool extended = false,
                    uint64 windowId = 0
                ) noexcept
                    : NkKeyboardEvent(key, scancode, mods, nativeKey, extended, windowId)
                    , mRepeatCount(repeatCount) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkKeyRepeatEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant la touche répétée et le compteur de répétitions
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt(
                        "KeyRepeat({0} x{1})",
                        NkKeyToString(mKey),
                        mRepeatCount
                    );
                }

                /// @brief Retourne le nombre de fois que la touche a été répétée
                /// @return uint32 représentant le compteur de répétitions depuis le dernier événement
                /// @note Valeur 1 = première répétition, 2 = deuxième, etc.
                NKENTSEU_EVENT_API_INLINE uint32 GetRepeatCount() const noexcept {
                    return mRepeatCount;
                }

            private:
                uint32 mRepeatCount;  ///< Nombre de répétitions depuis le dernier événement
        };

        // =====================================================================
        // NkKeyReleaseEvent — Touche relâchée
        // =====================================================================

        /**
         * @brief Émis lorsqu'une touche précédemment pressée est relâchée
         *
         * Cet événement marque la fin d'une interaction clavier :
         *   - Déclenché au moment où la touche est physiquement relâchée
         *   - Permet de détecter la durée d'un appui (timestamp press vs release)
         *   - Utile pour les actions qui se déclenchent au relâchement (ex: tir chargé)
         *
         * @note Un NkKeyReleaseEvent est toujours précédé d'un NkKeyPressEvent
         *       pour la même touche (sauf cas pathologiques de perte de focus).
         *
         * @note Pour les raccourcis, il est généralement préférable de réagir
         *       au NkKeyPressEvent plutôt qu'au release, pour une réactivité maximale.
         *
         * @code
         *   // Exemple : action au relâchement pour un tir chargé
         *   if (event.Is<NkKeyReleaseEvent>()) {
         *       auto& releaseEvent = static_cast<NkKeyReleaseEvent&>(event);
         *       if (releaseEvent.GetKey() == NkKey::NK_SPACE && mIsCharging) {
         *           ShootWithPower(mChargeDuration);
         *           mIsCharging = false;
         *       }
         *   }
         * @endcode
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkKeyReleaseEvent final : public NkKeyboardEvent {
            public:
                /// @brief Associe le type d'événement NK_KEY_RELEASED à cette classe
                NK_EVENT_TYPE_FLAGS(NK_KEY_RELEASED)

                /**
                 * @brief Constructeur avec paramètres d'un événement de relâchement
                 * @param key Touche logique (NkKey) associée à l'événement
                 * @param scancode Code physique USB HID (par défaut : NK_SC_UNKNOWN)
                 * @param mods État des modificateurs au moment de l'événement (par défaut : aucun)
                 * @param nativeKey Code natif brut de la plateforme (par défaut : 0)
                 * @param extended true si la touche est étendue (par défaut : false)
                 * @param windowId Identifiant de la fenêtre source (par défaut : 0)
                 */
                NKENTSEU_EVENT_API NkKeyReleaseEvent(
                    NkKey key,
                    NkScancode scancode = NkScancode::NK_SC_UNKNOWN,
                    const NkModifierState& mods = {},
                    uint32 nativeKey = 0,
                    bool extended = false,
                    uint64 windowId = 0
                ) noexcept
                    : NkKeyboardEvent(key, scancode, mods, nativeKey, extended, windowId) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkKeyReleaseEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant la touche relâchée
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return "KeyRelease("
                         + NkString(NkKeyToString(mKey)) + ")";
                }
        };

        // =====================================================================
        // NkTextInputEvent — Caractère Unicode produit (après IME/layout)
        // =====================================================================

        /**
         * @brief Émis lorsqu'un caractère Unicode est produit après traitement clavier
         *
         * Cet événement transporte le CARACTÈRE réellement saisi, après :
         *   - Application du layout clavier (AZERTY, QWERTY, Dvorak...)
         *   - Traitement des modificateurs (Shift pour majuscule, AltGr pour symboles)
         *   - Composition via IME (Input Method Editor) pour les langues asiatiques
         *   - Correction orthographique ou prédiction de texte si activée
         *
         * @note Cet événement est destiné à la SAISIE DE TEXTE :
         *       - Champs de formulaire, éditeurs de code, chats, recherche...
         *       - Ne pas utiliser pour les raccourcis clavier — utiliser NkKeyPressEvent
         *
         * @note Le caractère est encodé en UTF-8 dans mUtf8 (null-terminated, max 4 octets)
         *       et le codepoint Unicode brut est disponible via GetCodepoint() (UTF-32).
         *
         * @note Un NkTextInputEvent peut être émis SANS NkKeyPressEvent correspondant
         *       dans le cas de compositions IME complexes (ex: pinyin → caractères chinois).
         *
         * @code
         *   // Exemple : saisie de texte dans un champ de recherche
         *   if (event.Is<NkTextInputEvent>()) {
         *       auto& textEvent = static_cast<NkTextInputEvent&>(event);
         *       if (textEvent.IsPrintable()) {
         *           searchBuffer += textEvent.GetUtf8();  // Ajoute le caractère UTF-8
         *           UpdateSearchResults(searchBuffer);
         *       }
         *   }
         * @endcode
         *
         * @note Catégorie : NK_CAT_KEYBOARD | NK_CAT_INPUT (héritée via GetCategoryFlags)
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkTextInputEvent final : public NkEvent {
            public:
                /// @brief Associe le type d'événement NK_TEXT_INPUT à cette classe
                NK_EVENT_TYPE_FLAGS(NK_TEXT_INPUT)

                /// @brief Retourne les flags de catégorie combinés (KEYBOARD + INPUT)
                /// @return uint32 avec les bits NK_CAT_KEYBOARD et NK_CAT_INPUT activés
                NKENTSEU_EVENT_API uint32 GetCategoryFlags() const override {
                    return static_cast<uint32>(NkEventCategory::NK_CAT_KEYBOARD)
                         | static_cast<uint32>(NkEventCategory::NK_CAT_INPUT);
                }

                /// @brief Constructeur depuis un codepoint Unicode UTF-32
                /// @param codepoint Codepoint Unicode (U+0020 … U+10FFFF)
                /// @param windowId Identifiant de la fenêtre source (par défaut : 0)
                /// @note Encode automatiquement le codepoint en UTF-8 dans mUtf8
                /// @note Les codepoints < U+0020 (contrôle) sont acceptés mais IsPrintable() retourne false
                NKENTSEU_EVENT_API NkTextInputEvent(uint32 codepoint, uint64 windowId = 0) noexcept
                    : NkEvent(windowId)
                    , mCodepoint(codepoint) {
                    EncodeUtf8(codepoint);
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkTextInputEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString contenant le codepoint en hexa et le caractère UTF-8
                /// @note Exemple : "TextInput(U+0041 "A")" pour le caractère 'A'
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString("TextInput(U+")
                         + ToHex(mCodepoint) + " \"" + mUtf8 + "\")";
                }

                /// @brief Retourne le codepoint Unicode brut (UTF-32)
                /// @return uint32 représentant le codepoint (ex: 0x0041 pour 'A')
                /// @note Utiliser pour des comparaisons précises ou du traitement Unicode avancé
                NKENTSEU_EVENT_API_INLINE uint32 GetCodepoint() const noexcept {
                    return mCodepoint;
                }

                /// @brief Retourne la représentation UTF-8 du caractère (null-terminated)
                /// @return const char* pointant vers un buffer de max 5 octets (4 + null)
                /// @note Ne pas modifier le buffer retourné — mémoire interne à l'événement
                /// @note Pour les caractères ASCII, mUtf8[1] == '\0' (chaîne d'un seul octet)
                NKENTSEU_EVENT_API_INLINE const char* GetUtf8() const noexcept {
                    return mUtf8;
                }

                /// @brief Vérifie si le caractère est imprimable (non-contrôle)
                /// @return true si codepoint >= U+0020 ET codepoint != U+007F (DEL)
                /// @note Les caractères de contrôle (tab, newline, etc.) retournent false
                NKENTSEU_EVENT_API_INLINE bool IsPrintable() const noexcept {
                    return mCodepoint >= 0x20 && mCodepoint != 0x7F;
                }

                /// @brief Vérifie si le caractère est dans la plage ASCII de base
                /// @return true si codepoint < 0x80 (0–127)
                /// @note Utile pour optimiser le traitement des textes principalement ASCII
                NKENTSEU_EVENT_API_INLINE bool IsAscii() const noexcept {
                    return mCodepoint < 0x80;
                }

            private:
                uint32 mCodepoint;    ///< Codepoint Unicode brut (UTF-32)
                char mUtf8[5];        ///< Représentation UTF-8 (max 4 octets + null terminator)

                /// @brief Encode un codepoint Unicode en UTF-8 dans le buffer mUtf8
                /// @param cp Codepoint à encoder (0x0000–0x10FFFF)
                /// @note Implémentation conforme à la spec UTF-8 (RFC 3629)
                /// @note Les codepoints invalides (> 0x10FFFF) ne sont pas validés ici — responsabilité de l'appelant
                NKENTSEU_EVENT_API_INLINE void EncodeUtf8(uint32 cp) noexcept {
                    if (cp < 0x80) {
                        // ASCII : 1 octet, bit 7 à 0
                        mUtf8[0] = static_cast<char>(cp);
                        mUtf8[1] = '\0';
                    } else if (cp < 0x800) {
                        // 2 octets : 110xxxxx 10xxxxxx
                        mUtf8[0] = static_cast<char>(0xC0 | (cp >> 6));
                        mUtf8[1] = static_cast<char>(0x80 | (cp & 0x3F));
                        mUtf8[2] = '\0';
                    } else if (cp < 0x10000) {
                        // 3 octets : 1110xxxx 10xxxxxx 10xxxxxx
                        mUtf8[0] = static_cast<char>(0xE0 | (cp >> 12));
                        mUtf8[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        mUtf8[2] = static_cast<char>(0x80 | (cp & 0x3F));
                        mUtf8[3] = '\0';
                    } else {
                        // 4 octets : 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
                        mUtf8[0] = static_cast<char>(0xF0 | (cp >> 18));
                        mUtf8[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                        mUtf8[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        mUtf8[3] = static_cast<char>(0x80 | (cp & 0x3F));
                        mUtf8[4] = '\0';
                    }
                }

                /// @brief Convertit un entier en chaîne hexadécimale (4 chiffres minimum)
                /// @param v Valeur à convertir (typiquement un codepoint Unicode)
                /// @return NkString contenant la représentation hexadécimale (ex: "0041")
                /// @note Utilise snprintf pour un formatage sécurisé et portable
                static NKENTSEU_EVENT_API_INLINE NkString ToHex(uint32 v) {
                    char buf[9];  // 8 chiffres hex + null terminator
                    std::snprintf(buf, sizeof(buf), "%04X", v);
                    return buf;
                }
        };

    } // namespace nkentseu

#endif // NKENTSEU_EVENT_NKKEYBOARDEVENT_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKKEYBOARDEVENT.H
// =============================================================================
// Ce fichier définit la hiérarchie des événements clavier pour le module NKEvent.
// Les exemples ci-dessous illustrent les patterns d'usage recommandés.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Raccourcis clavier avec modificateurs (Ctrl/Cmd+Action)
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkKeyboardEvent.h"

    class ShortcutHandler {
    public:
        void OnKeyPress(nkentseu::NkKeyPressEvent& event) {
            using namespace nkentseu;

            // Raccourci universel : Ctrl (ou Cmd sur macOS) + S pour sauvegarder
            if (event.GetKey() == NkKey::NK_S && (event.HasCtrl() || event.HasSuper())) {
                Document::SaveCurrent();
                event.MarkHandled();  // Empêche la propagation à d'autres handlers
                return;
            }

            // Raccourci avec combinaison : Ctrl+Shift+Z pour refaire (Redo)
            if (event.GetKey() == NkKey::NK_Z && event.HasCtrl() && event.HasShift()) {
                Document::Redo();
                event.MarkHandled();
                return;
            }

            // Touche de fonction : F5 pour rafraîchir
            if (event.GetKey() == NkKey::NK_F11) {
                ToggleFullscreen();
                event.MarkHandled();
            }
        }

        // Enregistrement du handler dans le système d'événements
        void Register(nkentseu::event::EventManager& mgr) {
            mgr.Subscribe<NkKeyPressEvent>(
                NK_EVENT_BIND_HANDLER(OnKeyPress)
            );
        }

    private:
        void ToggleFullscreen();
    };
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Saisie de texte Unicode avec support IME
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkKeyboardEvent.h"

    class TextInputField {
    public:
        void OnTextInput(nkentseu::NkTextInputEvent& event) {
            using namespace nkentseu;

            // Ignorer les caractères de contrôle non imprimables
            if (!event.IsPrintable()) {
                return;
            }

            // Ajout du caractère UTF-8 au buffer de saisie
            mTextBuffer += event.GetUtf8();

            // Mise à jour du curseur et du rendu
            UpdateCursor();
            RenderText();

            // Logging optionnel pour debug IME
            NK_FOUNDATION_LOG_DEBUG("Input: U+{0:X} ('{1}')",
                event.GetCodepoint(), event.GetUtf8());
        }

        void OnKeyPress(nkentseu::NkKeyPressEvent& event) {
            using namespace nkentseu;

            // Gestion des touches de navigation dans le champ de texte
            switch (event.GetKey()) {
                case NkKey::NK_LEFT:
                    MoveCursor(-1);  // Déplace le curseur d'un caractère vers la gauche
                    break;
                case NkKey::NK_RIGHT:
                    MoveCursor(+1);  // Déplace le curseur d'un caractère vers la droite
                    break;
                case NkKey::NK_BACK:
                    DeletePreviousCharacter();  // Supprime le caractère avant le curseur
                    break;
                case NkKey::NK_DELETE:
                    DeleteNextCharacter();  // Supprime le caractère après le curseur
                    break;
                case NkKey::NK_ENTER:
                    OnSubmit();  // Valide la saisie
                    break;
                case NkKey::NK_ESCAPE:
                    OnCancel();  // Annule la saisie
                    break;
                default:
                    // Autres touches : ignorées pour la saisie de texte
                    break;
            }
        }

    private:
        NkString mTextBuffer;  // Buffer de texte en cours de saisie

        void MoveCursor(int delta);
        void DeletePreviousCharacter();
        void DeleteNextCharacter();
        void OnSubmit();
        void OnCancel();
        void UpdateCursor();
        void RenderText();
    };
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Contrôles de jeu avec distinction pavé numérique / principal
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkKeyboardEvent.h"

    class GameInputMapper {
    public:
        void OnKeyPress(nkentseu::NkKeyPressEvent& event) {
            using namespace nkentseu;

            // Déplacement : flèches OU pavé numérique (si NumLock désactivé)
            if (!event.GetModifiers().numLock) {
                switch (event.GetKey()) {
                    case NkKey::NK_UP:
                    case NkKey::NK_NUMPAD_8:
                        player.Move(Direction::Up);
                        event.MarkHandled();
                        break;
                    case NkKey::NK_DOWN:
                    case NkKey::NK_NUMPAD_2:
                        player.Move(Direction::Down);
                        event.MarkHandled();
                        break;
                    case NkKey::NK_LEFT:
                    case NkKey::NK_NUMPAD_4:
                        player.Move(Direction::Left);
                        event.MarkHandled();
                        break;
                    case NkKey::NK_RIGHT:
                    case NkKey::NK_NUMPAD_6:
                        player.Move(Direction::Right);
                        event.MarkHandled();
                        break;
                }
            }

            // Actions : touches principales uniquement (pas de doublon avec numpad)
            if (event.GetKey() == NkKey::NK_SPACE) {
                player.Jump();
                event.MarkHandled();
            }
            else if (event.GetKey() == NkKey::NK_LCTRL || event.GetKey() == NkKey::NK_RCTRL) {
                player.Crouch();
                event.MarkHandled();
            }
        }

        void OnKeyRelease(nkentseu::NkKeyReleaseEvent& event) {
            using namespace nkentseu;

            // Arrêt du mouvement au relâchement
            if (event.GetKey() == NkKey::NK_UP || event.GetKey() == NkKey::NK_NUMPAD_8) {
                player.StopMoving(Direction::Up);
            }
            // ... gérer les autres directions de manière similaire
        }

    private:
        Player& player;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Débogage et logging des événements clavier cross-platform
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkKeyboardEvent.h"

    class InputDebugger {
    public:
        // Logging détaillé de tout événement clavier pour analyse
        static void LogKeyEvent(const nkentseu::NkKeyboardEvent& event, const char* platform) {
            using namespace nkentseu;

            NK_FOUNDATION_LOG_DEBUG(
                "[{0}] {1} key={2}({3}) scancode=0x{4:02X} native=0x{5:X} mods={6}",
                platform,
                event.GetTypeStr(),
                NkKeyToString(event.GetKey()),
                static_cast<uint32>(event.GetKey()),
                static_cast<uint32>(event.GetScancode()),
                event.GetNativeKey(),
                event.GetModifiers().ToString()
            );

            // Logging supplémentaire pour NkTextInputEvent
            if (auto* textEvent = event.As<NkTextInputEvent>()) {
                NK_FOUNDATION_LOG_DEBUG("  → Unicode: U+{0:04X} UTF-8: '{1}'",
                    textEvent->GetCodepoint(), textEvent->GetUtf8());
            }
        }

        // Test unitaire de conversion de scancode pour une plateforme
        static void TestScancodeMapping(const char* platformName,
                                        std::function<nkentseu::NkScancode(uint32, bool)> converter) {
            using namespace nkentseu;

            struct TestCase {
                uint32 code;
                bool extended;
                NkScancode expected;
                const char* description;
            };

            static const TestCase tests[] = {
                { 0x1C, false, NkScancode::NK_SC_ENTER, "Enter principal" },
                { 0x1C, true,  NkScancode::NK_SC_NUMPAD_ENTER, "Numpad Enter" },
                { 0x1E, false, NkScancode::NK_SC_1, "Touche 1" },
                { 0x3A, false, NkScancode::NK_SC_F1, "F1" },
                { 0x4F, false, NkScancode::NK_SC_RIGHT, "Flèche droite" },
                { 0x1D, false, NkScancode::NK_SC_LCTRL, "Ctrl gauche" },
                { 0x1D, true,  NkScancode::NK_SC_RCTRL, "Ctrl droit" },
                { 0xFFFFFFFF, false, NkScancode::NK_SC_UNKNOWN, "Code invalide" }
            };

            NK_FOUNDATION_LOG_INFO("Testing {0} scancode mapping...", platformName);

            for (const auto& test : tests) {
                NkScancode result = converter(test.code, test.extended);
                if (result == test.expected) {
                    NK_FOUNDATION_LOG_DEBUG("  ✓ {0} (0x{1:X}{2}) → {3}",
                        test.description, test.code, test.extended ? "+E0" : "",
                        NkScancodeToString(result));
                } else {
                    NK_FOUNDATION_LOG_ERROR("  ✗ {0} (0x{1:X}{2}) → {3} (expected {4})",
                        test.description, test.code, test.extended ? "+E0" : "",
                        NkScancodeToString(result), NkScancodeToString(test.expected));
                }
            }
        }
    };

    // Usage dans les tests d'intégration :
    // InputDebugger::TestScancodeMapping("Win32", NkScancodeFromWin32);
    // InputDebugger::TestScancodeMapping("X11", [](uint32 kc, bool) {
    //     return NkScancodeFromXKeycode(kc);
    // });
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Gestion des compositions IME pour les langues asiatiques
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkKeyboardEvent.h"

    class IMEHandler {
    public:
        // Début d'une composition IME (ex: saisie pinyin en attente de sélection)
        void OnCompositionStart() {
            mIsComposing = true;
            mCompositionBuffer.Clear();
            ShowIMECandidateWindow();  // Affiche la fenêtre de sélection de caractères
        }

        // Mise à jour de la composition en cours
        void OnCompositionUpdate(const nkentseu::NkString& preedit, int cursorPos) {
            mCompositionBuffer = preedit;
            UpdateIMECandidateWindow(preedit, cursorPos);
        }

        // Finalisation de la composition : caractère(s) sélectionné(s)
        void OnCompositionEnd(nkentseu::NkTextInputEvent& event) {
            using namespace nkentseu;

            mIsComposing = false;
            HideIMECandidateWindow();

            // Insertion du caractère finalisé dans le buffer de texte
            if (event.IsPrintable()) {
                mTextBuffer += event.GetUtf8();
                UpdateRenderedText();
            }

            NK_FOUNDATION_LOG_DEBUG("IME composition ended: U+{0:04X}",
                event.GetCodepoint());
        }

        // Gestion des touches de navigation dans la fenêtre de candidats IME
        void OnKeyPressDuringIME(nkentseu::NkKeyPressEvent& event) {
            using namespace nkentseu;

            if (!mIsComposing) {
                return;  // Délègue au handler normal si pas en composition
            }

            // Navigation dans les candidats : flèches ou chiffres pour sélection
            switch (event.GetKey()) {
                case NkKey::NK_UP:
                case NkKey::NK_DOWN:
                    NavigateIMECandidates(event.GetKey() == NkKey::NK_DOWN ? +1 : -1);
                    event.MarkHandled();
                    break;
                case NkKey::NK_NUM1: case NkKey::NK_NUM2: case NkKey::NK_NUM3:
                case NkKey::NK_NUM4: case NkKey::NK_NUM5: case NkKey::NK_NUM6:
                case NkKey::NK_NUM7: case NkKey::NK_NUM8: case NkKey::NK_NUM9:
                    SelectIMECandidate(event.GetKey() - NkKey::NK_NUM1);
                    event.MarkHandled();
                    break;
                case NkKey::NK_ESCAPE:
                    CancelIMEComposition();
                    event.MarkHandled();
                    break;
                default:
                    // Autres touches : ignorées pendant la composition
                    event.MarkHandled();
                    break;
            }
        }

    private:
        bool mIsComposing = false;
        nkentseu::NkString mCompositionBuffer;
        nkentseu::NkString mTextBuffer;

        void ShowIMECandidateWindow();
        void HideIMECandidateWindow();
        void UpdateIMECandidateWindow(const nkentseu::NkString& preedit, int cursorPos);
        void NavigateIMECandidates(int delta);
        void SelectIMECandidate(int index);
        void CancelIMEComposition();
        void UpdateRenderedText();
    };
*/

// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
    1. DISTINCTION NkKeyPressEvent vs NkTextInputEvent :
       - NkKeyPressEvent : pour les RACCOURCIS et CONTRÔLES (position physique)
       - NkTextInputEvent : pour la SAISIE DE TEXTE (caractère Unicode produit)
       - Ne jamais utiliser NkKeyPressEvent pour insérer du texte — cela briserait
         la saisie avec des layouts non-QWERTY ou des IME asiatiques

    2. GESTION DES MODIFICATEURS :
       - Utiliser HasCtrl()/HasShift() etc. pour les raccourcis (indépendant gauche/droite)
       - Utiliser GetKey() == NK_LCTRL/NK_RCTRL si la distinction physique est requise
       - AltGr est séparé de Alt : tester HasAltGr() pour les claviers internationaux

    3. SUPPORT UNICODE ET IME :
       - NkTextInputEvent::GetUtf8() retourne une chaîne UTF-8 null-terminated
       - Pour un traitement Unicode avancé, utiliser GetCodepoint() (UTF-32)
       - Les compositions IME peuvent générer plusieurs NkTextInputEvent pour un seul "caractère"

    4. PERFORMANCE DES HANDLERS CLAVIER :
       - Les événements clavier peuvent être très fréquents (auto-repeat, jeux)
       - Éviter les allocations dans les handlers de NkKeyPressEvent/NkKeyRepeatEvent
       - Pour le logging de debug, utiliser des macros conditionnelles (NKENTSEU_INPUT_DEBUG)

    5. TESTS CROSS-PLATFORM :
       - Tester les mappings de scancode sur chaque plateforme cible
       - Valider que les mêmes touches physiques produisent le même NkKey partout
       - Tester la saisie avec différents layouts (AZERTY, QWERTZ, Dvorak) et IME

    6. EXTENSIBILITÉ :
       - Pour ajouter une nouvelle touche à NkKey :
         a) Ajouter l'entrée dans l'enum NkKey (avant NK_KEY_MAX)
         b) Mettre à jour NkKeyToString() pour le débogage
         c) Mettre à jour TOUS les mappings NkScancodeToKey() et NkKeyFrom*()
         d) Documenter la nouvelle touche dans la Doxygen de l'enum
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================