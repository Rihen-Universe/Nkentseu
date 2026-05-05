// =============================================================================
// NKEvent/NkMouseEvent.h
// Hiérarchie complète des événements souris.
//
// Catégorie : NK_CAT_MOUSE | NK_CAT_INPUT
//
// Architecture :
//   - NkMouseButton          : énumération des boutons souris supportés
//   - NkButtonState          : état binaire d'un bouton (pressé / relâché)
//   - NkMouseButtons         : masque de bits pour boutons multiples simultanés
//   - NkMouseEvent           : classe de base abstraite pour tous les événements souris
//   - Classes dérivées       : implémentations concrètes par type d'événement souris
//
// Événements couverts :
//   - Mouvement        : Move (coordonnées client), Raw (mouvement brut sans accélération)
//   - Boutons          : Press, Release, DoubleClick avec détection de clickCount
//   - Molettes         : Vertical, Horizontal, Scroll unifié (trackpad smooth-scroll)
//   - Focus curseur    : Enter/Leave zone client, Enter/Leave fenêtre complète
//   - Capture          : Begin/End de capture exclusive pour drag-and-drop
//
// Design :
//   - Héritage polymorphe depuis NkEvent pour intégration au dispatcher central
//   - Macros NK_EVENT_* pour éviter la duplication de code boilerplate
//   - Méthodes Clone() pour copie profonde (replay, tests, sérialisation)
//   - ToString() surchargé pour débogage et logging significatif
//   - Accesseurs inline noexcept pour performance critique (lecture fréquente)
//   - Support des modificateurs clavier (Ctrl, Shift, Alt...) via NkModifierState
//
// Usage typique :
//   if (auto* move = event.As<NkMouseMoveEvent>()) {
//       if (move->IsButtonDown(NkMouseButton::NK_MB_RIGHT)) {
//           Camera::Rotate(move->GetDeltaX(), move->GetDeltaY());
//       }
//   }
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_EVENT_NKMOUSEEVENT_H
#define NKENTSEU_EVENT_NKMOUSEEVENT_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des dépendances internes au projet.
    // Toutes les dépendances utilisent les modules NK* du framework.

    #include "NKEvent/NkEvent.h"                        // Classe de base NkEvent + macros
    #include "NKEvent/NkEventApi.h"                     // Macros d'export NKENTSEU_EVENT_API
    #include "NKEvent/NkKeyboardEvent.h"                // NkModifierState pour modificateurs clavier
    #include "NKContainers/String/NkStringUtils.h"      // Conversion types → NkString

    namespace nkentseu {

        // =====================================================================
        // NkMouseButton — Énumération des boutons souris supportés
        // =====================================================================

        /// @brief Représente les boutons physiques d'une souris ou d'un périphérique de pointage
        /// @note Utilisé par les événements de bouton pour identifier quelle action déclencher
        enum class NKENTSEU_EVENT_CLASS_EXPORT NkMouseButton : uint32 {
            NK_MB_UNKNOWN = 0,    ///< Bouton non identifié ou événement générique
            NK_MB_LEFT,           ///< Bouton gauche : action principale (clic, sélection)
            NK_MB_RIGHT,          ///< Bouton droit : action contextuelle (menu, options)
            NK_MB_MIDDLE,         ///< Bouton central : molette cliquable (pan, ouverture lien)
            NK_MB_BACK,           ///< Bouton latéral arrière : navigation historique (précédent)
            NK_MB_FORWARD,        ///< Bouton latéral avant : navigation historique (suivant)
            NK_MB_6,              ///< Bouton supplémentaire 6 : défini par l'application
            NK_MB_7,              ///< Bouton supplémentaire 7 : défini par l'application
            NK_MB_8,              ///< Bouton supplémentaire 8 : défini par l'application
            NK_MOUSE_BUTTON_MAX   ///< Sentinelle : nombre de valeurs valides (pour validation)
        };

        /// @brief Convertit une valeur NkMouseButton en chaîne lisible pour le débogage
        /// @param b La valeur de bouton à convertir
        /// @return Pointeur vers une chaîne littérale statique (ne pas libérer)
        /// @note Fonction noexcept pour usage dans les contextes critiques
        inline const char* NkMouseButtonToString(NkMouseButton b) noexcept {
            switch (b) {
                case NkMouseButton::NK_MB_LEFT:
                    return "LEFT";
                case NkMouseButton::NK_MB_RIGHT:
                    return "RIGHT";
                case NkMouseButton::NK_MB_MIDDLE:
                    return "MIDDLE";
                case NkMouseButton::NK_MB_BACK:
                    return "BACK";
                case NkMouseButton::NK_MB_FORWARD:
                    return "FORWARD";
                case NkMouseButton::NK_MB_6:
                    return "MB6";
                case NkMouseButton::NK_MB_7:
                    return "MB7";
                case NkMouseButton::NK_MB_8:
                    return "MB8";
                case NkMouseButton::NK_MB_UNKNOWN:
                default:
                    return "UNKNOWN";
            }
        }

        // =====================================================================
        // NkButtonState — Énumération de l'état binaire d'un bouton
        // =====================================================================

        /// @brief Représente l'état actuel d'un bouton souris (pressé ou relâché)
        /// @note Utilisé en interne par NkMouseButtonEvent pour distinguer press/release
        enum class NKENTSEU_EVENT_CLASS_EXPORT NkButtonState : uint32 {
            NK_RELEASED = 0,    ///< Bouton relâché : aucune pression détectée
            NK_PRESSED  = 1     ///< Bouton pressé : pression active détectée
        };

        // =====================================================================
        // NkMouseButtons — Structure de masque de bits pour boutons multiples
        // =====================================================================

        /**
         * @brief Représente l'état combiné de tous les boutons souris via un masque de bits
         *
         * Cette structure permet de suivre quels boutons sont enfoncés simultanément,
         * essentiel pour les interactions complexes comme :
         *   - Clic droit + déplacement pour rotation de caméra (3D)
         *   - Ctrl + clic gauche pour sélection multiple
         *   - Shift + clic pour sélection de plage
         *
         * @note Chaque bit du masque correspond à un NkMouseButton :
         *       bit 0 = NK_MB_UNKNOWN, bit 1 = NK_MB_LEFT, etc.
         *
         * @note Utiliser les méthodes Set(), Clear(), IsDown() pour manipulation sûre.
         */
        struct NKENTSEU_EVENT_CLASS_EXPORT NkMouseButtons {
            uint32 mask = 0;  ///< Masque de bits : bit i = 1 si bouton i enfoncé

            /// @brief Marque le bouton spécifié comme enfoncé dans le masque
            /// @param b Le bouton à marquer comme pressé
            /// @note Opération OR bit-à-bit : n'affecte pas les autres bits
            NKENTSEU_EVENT_API_INLINE void Set(NkMouseButton b) noexcept {
                mask |= (1u << static_cast<uint32>(b));
            }

            /// @brief Marque le bouton spécifié comme relâché dans le masque
            /// @param b Le bouton à marquer comme relâché
            /// @note Opération AND avec complément : n'affecte pas les autres bits
            NKENTSEU_EVENT_API_INLINE void Clear(NkMouseButton b) noexcept {
                mask &= ~(1u << static_cast<uint32>(b));
            }

            /// @brief Vérifie si le bouton spécifié est actuellement enfoncé
            /// @param b Le bouton à tester
            /// @return true si le bit correspondant est à 1 dans le masque
            NKENTSEU_EVENT_API_INLINE bool IsDown(NkMouseButton b) const noexcept {
                return (mask & (1u << static_cast<uint32>(b))) != 0;
            }

            /// @brief Vérifie si au moins un bouton est enfoncé
            /// @return true si mask != 0, false si tous les boutons sont relâchés
            NKENTSEU_EVENT_API_INLINE bool Any() const noexcept {
                return mask != 0;
            }

            /// @brief Vérifie si aucun bouton n'est enfoncé
            /// @return true si mask == 0, false si au moins un bouton est pressé
            NKENTSEU_EVENT_API_INLINE bool IsNone() const noexcept {
                return mask == 0;
            }
        };

        // =====================================================================
        // NkMouseEvent — Classe de base abstraite pour événements souris
        // =====================================================================

        /**
         * @brief Classe de base polymorphe pour tous les événements liés à la souris
         *
         * Catégories associées : NK_CAT_MOUSE | NK_CAT_INPUT
         *
         * Cette classe sert de point d'ancrage commun pour tous les événements
         * de pointage : mouvement, clics, molette, entrée/sortie de zone.
         *
         * @note Toutes les classes dérivées héritent automatiquement des deux
         *       catégories via la surcharge de GetCategoryFlags().
         *
         * @note L'identifiant de fenêtre (windowId) est transmis au constructeur
         *       de NkEvent et accessible via GetWindowId() pour le routage.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkMouseEvent : public NkEvent {
            public:
                /// @brief Retourne les flags de catégorie combinés (MOUSE + INPUT)
                /// @return uint32 avec les bits NK_CAT_MOUSE et NK_CAT_INPUT activés
                /// @note Permet le filtrage par catégorie souris OU entrée générique
                NKENTSEU_EVENT_API uint32 GetCategoryFlags() const override {
                    return static_cast<uint32>(NkEventCategory::NK_CAT_MOUSE)
                         | static_cast<uint32>(NkEventCategory::NK_CAT_INPUT);
                }

            protected:
                /// @brief Constructeur protégé — réservé aux classes dérivées
                /// @param windowId Identifiant de la fenêtre source (0 = événement global)
                explicit NKENTSEU_EVENT_API NkMouseEvent(uint64 windowId = 0) noexcept
                    : NkEvent(windowId) {
                }
        };

        // =====================================================================
        // NkMouseMoveEvent — Déplacement du curseur avec coordonnées et deltas
        // =====================================================================

        /**
         * @brief Émis lorsque le curseur de la souris se déplace dans la zone client
         *
         * Cet événement transporte des informations complètes pour le suivi précis :
         *   - x/y : position absolue dans la zone client de la fenêtre [pixels]
         *   - screenX/screenY : position absolue sur l'écran physique [pixels]
         *   - deltaX/deltaY : déplacement relatif depuis le dernier événement [pixels]
         *   - buttons : état combiné des boutons enfoncés pendant le mouvement
         *   - modifiers : état des modificateurs clavier (Ctrl, Shift, Alt...)
         *
         * @note Les coordonnées "client" sont relatives au contenu de la fenêtre
         *       (exclu les bordures, barre de titre, menu...).
         *
         * @note La fréquence d'émission dépend du hardware et du driver :
         *       typiquement 60-1000 Hz. Éviter les calculs lourds dans les handlers.
         *
         * @note Pour un drag-and-drop, vérifier IsButtonDown() sur le bouton utilisé
         *       pour initier le drag dans le handler de NkMouseMoveEvent.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkMouseMoveEvent final : public NkMouseEvent {
            public:
                /// @brief Associe le type d'événement NK_MOUSE_MOVE à cette classe
                NK_EVENT_TYPE_FLAGS(NK_MOUSE_MOVE)

                /**
                 * @brief Constructeur avec toutes les données de mouvement
                 * @param x Position horizontale dans la zone client [pixels]
                 * @param y Position verticale dans la zone client [pixels]
                 * @param screenX Position horizontale absolue sur l'écran [pixels]
                 * @param screenY Position verticale absolue sur l'écran [pixels]
                 * @param deltaX Déplacement horizontal depuis le dernier événement [pixels]
                 * @param deltaY Déplacement vertical depuis le dernier événement [pixels]
                 * @param buttons État combiné des boutons enfoncés pendant le mouvement
                 * @param mods État des modificateurs clavier actifs (Ctrl, Shift, Alt...)
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 */
                NKENTSEU_EVENT_API NkMouseMoveEvent(
                    int32 x,
                    int32 y,
                    int32 screenX,
                    int32 screenY,
                    int32 deltaX,
                    int32 deltaY,
                    const NkMouseButtons& buttons = {},
                    const NkModifierState& mods = {},
                    uint64 windowId = 0
                ) noexcept
                    : NkMouseEvent(windowId)
                    , mX(x)
                    , mY(y)
                    , mScreenX(screenX)
                    , mScreenY(screenY)
                    , mDeltaX(deltaX)
                    , mDeltaY(deltaY)
                    , mButtons(buttons)
                    , mModifiers(mods) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkMouseMoveEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant la position et le delta de mouvement
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt(
                        "MouseMove({0},{1} d={2},{3})",
                        mX, mY, mDeltaX, mDeltaY
                    );
                }

                /// @brief Retourne la position horizontale dans la zone client
                /// @return int32 représentant la coordonnée X [pixels client]
                NKENTSEU_EVENT_API_INLINE int32 GetX() const noexcept {
                    return mX;
                }

                /// @brief Retourne la position verticale dans la zone client
                /// @return int32 représentant la coordonnée Y [pixels client]
                NKENTSEU_EVENT_API_INLINE int32 GetY() const noexcept {
                    return mY;
                }

                /// @brief Retourne la position horizontale absolue sur l'écran
                /// @return int32 représentant la coordonnée screenX [pixels écran]
                NKENTSEU_EVENT_API_INLINE int32 GetScreenX() const noexcept {
                    return mScreenX;
                }

                /// @brief Retourne la position verticale absolue sur l'écran
                /// @return int32 représentant la coordonnée screenY [pixels écran]
                NKENTSEU_EVENT_API_INLINE int32 GetScreenY() const noexcept {
                    return mScreenY;
                }

                /// @brief Retourne le déplacement horizontal depuis le dernier événement
                /// @return int32 représentant le delta X [pixels] (>0 = droite, <0 = gauche)
                NKENTSEU_EVENT_API_INLINE int32 GetDeltaX() const noexcept {
                    return mDeltaX;
                }

                /// @brief Retourne le déplacement vertical depuis le dernier événement
                /// @return int32 représentant le delta Y [pixels] (>0 = bas, <0 = haut)
                NKENTSEU_EVENT_API_INLINE int32 GetDeltaY() const noexcept {
                    return mDeltaY;
                }

                /// @brief Retourne l'état combiné des boutons enfoncés
                /// @return NkMouseButtons contenant le masque de bits des boutons actifs
                NKENTSEU_EVENT_API_INLINE NkMouseButtons GetButtons() const noexcept {
                    return mButtons;
                }

                /// @brief Retourne l'état des modificateurs clavier actifs
                /// @return NkModifierState indiquant Ctrl, Shift, Alt, etc.
                NKENTSEU_EVENT_API_INLINE NkModifierState GetModifiers() const noexcept {
                    return mModifiers;
                }

                /// @brief Vérifie si un bouton spécifique est enfoncé pendant le mouvement
                /// @param b Le bouton à tester (ex: NK_MB_RIGHT pour drag contextuel)
                /// @return true si le bouton est actif dans le masque mButtons
                NKENTSEU_EVENT_API_INLINE bool IsButtonDown(NkMouseButton b) const noexcept {
                    return mButtons.IsDown(b);
                }

            private:
                int32 mX;              ///< Position horizontale dans la zone client [pixels]
                int32 mY;              ///< Position verticale dans la zone client [pixels]
                int32 mScreenX;        ///< Position horizontale absolue sur l'écran [pixels]
                int32 mScreenY;        ///< Position verticale absolue sur l'écran [pixels]
                int32 mDeltaX;         ///< Déplacement horizontal depuis le dernier événement [pixels]
                int32 mDeltaY;         ///< Déplacement vertical depuis le dernier événement [pixels]
                NkMouseButtons mButtons;    ///< Masque de bits des boutons enfoncés
                NkModifierState mModifiers; ///< État des modificateurs clavier actifs
        };

        // =====================================================================
        // NkMouseRawEvent — Mouvement brut sans accélération OS
        // =====================================================================

        /**
         * @brief Émis pour un mouvement de souris brut, non traité par l'OS
         *
         * Cet événement fournit le mouvement physique pur du capteur :
         *   - deltaX/deltaY : déplacement brut en comptes de capteur
         *   - deltaZ : axe supplémentaire (souris 3D, molette brute)
         *   - Aucune accélération, scaling ou smoothing appliqué par l'OS
         *
         * @note Utilisé principalement pour :
         *       - Jeux FPS/3D : contrôle de caméra précis sans accélération
         *       - Applications CAD : rotation/pan avec sensibilité constante
         *       - Calibration de périphériques : analyse du signal brut
         *
         * @warning Les valeurs sont en "comptes de capteur", pas en pixels.
         *          La conversion dépend de la DPI de la souris et des settings OS.
         *
         * @note Si l'OS applique une accélération souris, NkMouseMoveEvent
         *       sera affecté mais NkMouseRawEvent ne le sera pas.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkMouseRawEvent final : public NkMouseEvent {
            public:
                /// @brief Associe le type d'événement NK_MOUSE_RAW à cette classe
                NK_EVENT_TYPE_FLAGS(NK_MOUSE_RAW)

                /**
                 * @brief Constructeur avec deltas bruts du capteur
                 * @param deltaX Déplacement horizontal brut [comptes de capteur]
                 * @param deltaY Déplacement vertical brut [comptes de capteur]
                 * @param deltaZ Déplacement axe Z brut (molette ou souris 3D) [comptes]
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 */
                NKENTSEU_EVENT_API NkMouseRawEvent(
                    int32 deltaX,
                    int32 deltaY,
                    int32 deltaZ = 0,
                    uint64 windowId = 0
                ) noexcept
                    : NkMouseEvent(windowId)
                    , mDeltaX(deltaX)
                    , mDeltaY(deltaY)
                    , mDeltaZ(deltaZ) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkMouseRawEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant les deltas bruts X et Y
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt("MouseRaw({0},{1})", mDeltaX, mDeltaY);
                }

                /// @brief Retourne le déplacement horizontal brut du capteur
                /// @return int32 représentant le delta X [comptes de capteur]
                NKENTSEU_EVENT_API_INLINE int32 GetDeltaX() const noexcept {
                    return mDeltaX;
                }

                /// @brief Retourne le déplacement vertical brut du capteur
                /// @return int32 représentant le delta Y [comptes de capteur]
                NKENTSEU_EVENT_API_INLINE int32 GetDeltaY() const noexcept {
                    return mDeltaY;
                }

                /// @brief Retourne le déplacement de l'axe Z brut (si disponible)
                /// @return int32 représentant le delta Z [comptes de capteur]
                /// @note Utilisé pour les souris 3D ou la molette en mode brut
                NKENTSEU_EVENT_API_INLINE int32 GetDeltaZ() const noexcept {
                    return mDeltaZ;
                }

            private:
                int32 mDeltaX;  ///< Déplacement horizontal brut [comptes de capteur]
                int32 mDeltaY;  ///< Déplacement vertical brut [comptes de capteur]
                int32 mDeltaZ;  ///< Déplacement axe Z brut [comptes de capteur]
        };

        // =====================================================================
        // NkMouseButtonEvent — Classe de base pour événements de bouton
        // =====================================================================

        /**
         * @brief Classe de base pour les événements de pression/relâchement de bouton
         *
         * Cette classe n'est pas instanciable directement. Utiliser :
         *   - NkMouseButtonPressEvent  : pour un bouton pressé
         *   - NkMouseButtonReleaseEvent : pour un bouton relâché
         *   - NkMouseDoubleClickEvent   : pour un double-clic détecté par l'OS
         *
         * @note Toutes les classes dérivées partagent ces données communes :
         *       - Le bouton concerné (GetButton())
         *       - La position du curseur au moment de l'événement
         *       - Le clickCount (1 = simple, 2 = double, etc.)
         *       - Les modificateurs clavier actifs
         *
         * @note La position rapportée est celle du curseur au moment de l'événement,
         *       pas nécessairement la position où le bouton a été initialement pressé.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkMouseButtonEvent : public NkMouseEvent {
            public:
                /// @brief Retourne le bouton concerné par l'événement
                /// @return NkMouseButton identifiant le bouton physique
                NKENTSEU_EVENT_API_INLINE NkMouseButton GetButton() const noexcept {
                    return mButton;
                }

                /// @brief Retourne l'état actuel du bouton (pressé ou relâché)
                /// @return NkButtonState indiquant NK_PRESSED ou NK_RELEASED
                NKENTSEU_EVENT_API_INLINE NkButtonState GetState() const noexcept {
                    return mState;
                }

                /// @brief Retourne la position horizontale du curseur au moment de l'événement
                /// @return int32 représentant la coordonnée X [pixels client]
                NKENTSEU_EVENT_API_INLINE int32 GetX() const noexcept {
                    return mX;
                }

                /// @brief Retourne la position verticale du curseur au moment de l'événement
                /// @return int32 représentant la coordonnée Y [pixels client]
                NKENTSEU_EVENT_API_INLINE int32 GetY() const noexcept {
                    return mY;
                }

                /// @brief Retourne la position écran horizontale au moment de l'événement
                /// @return int32 représentant screenX [pixels écran]
                NKENTSEU_EVENT_API_INLINE int32 GetScreenX() const noexcept {
                    return mScreenX;
                }

                /// @brief Retourne la position écran verticale au moment de l'événement
                /// @return int32 représentant screenY [pixels écran]
                NKENTSEU_EVENT_API_INLINE int32 GetScreenY() const noexcept {
                    return mScreenY;
                }

                /// @brief Retourne le nombre de clics détectés (1 = simple, 2 = double...)
                /// @return uint32 représentant le clickCount
                /// @note Utile pour distinguer un simple-clic d'un double-clic manuel
                NKENTSEU_EVENT_API_INLINE uint32 GetClickCount() const noexcept {
                    return mClickCount;
                }

                /// @brief Retourne l'état des modificateurs clavier actifs
                /// @return NkModifierState indiquant Ctrl, Shift, Alt, etc.
                NKENTSEU_EVENT_API_INLINE NkModifierState GetModifiers() const noexcept {
                    return mModifiers;
                }

                /// @brief Raccourci : indique si le bouton gauche est concerné
                /// @return true si GetButton() == NK_MB_LEFT
                NKENTSEU_EVENT_API_INLINE bool IsLeft() const noexcept {
                    return mButton == NkMouseButton::NK_MB_LEFT;
                }

                /// @brief Raccourci : indique si le bouton droit est concerné
                /// @return true si GetButton() == NK_MB_RIGHT
                NKENTSEU_EVENT_API_INLINE bool IsRight() const noexcept {
                    return mButton == NkMouseButton::NK_MB_RIGHT;
                }

                /// @brief Raccourci : indique si le bouton central est concerné
                /// @return true si GetButton() == NK_MB_MIDDLE
                NKENTSEU_EVENT_API_INLINE bool IsMiddle() const noexcept {
                    return mButton == NkMouseButton::NK_MB_MIDDLE;
                }

            protected:
                /// @brief Constructeur protégé — réservé aux classes dérivées
                /// @param button Bouton concerné par l'événement
                /// @param state État du bouton (NK_PRESSED ou NK_RELEASED)
                /// @param x Position horizontale client au moment de l'événement
                /// @param y Position verticale client au moment de l'événement
                /// @param screenX Position horizontale écran au moment de l'événement
                /// @param screenY Position verticale écran au moment de l'événement
                /// @param clickCount Nombre de clics détectés (1 = simple, 2 = double...)
                /// @param mods État des modificateurs clavier actifs
                /// @param windowId Identifiant de la fenêtre source
                NKENTSEU_EVENT_API NkMouseButtonEvent(
                    NkMouseButton button,
                    NkButtonState state,
                    int32 x,
                    int32 y,
                    int32 screenX,
                    int32 screenY,
                    uint32 clickCount,
                    const NkModifierState& mods,
                    uint64 windowId
                ) noexcept
                    : NkMouseEvent(windowId)
                    , mButton(button)
                    , mState(state)
                    , mX(x)
                    , mY(y)
                    , mScreenX(screenX)
                    , mScreenY(screenY)
                    , mClickCount(clickCount)
                    , mModifiers(mods) {
                }

                NkMouseButton mButton;        ///< Bouton concerné par l'événement
                NkButtonState mState;         ///< État actuel du bouton (pressé/relâché)
                int32 mX;                     ///< Position horizontale client [pixels]
                int32 mY;                     ///< Position verticale client [pixels]
                int32 mScreenX;               ///< Position horizontale écran [pixels]
                int32 mScreenY;               ///< Position verticale écran [pixels]
                uint32 mClickCount;           ///< Nombre de clics détectés (1, 2, 3...)
                NkModifierState mModifiers;   ///< État des modificateurs clavier actifs
        };

        // =====================================================================
        // NkMouseButtonPressEvent — Bouton souris pressé
        // =====================================================================

        /**
         * @brief Émis lorsqu'un bouton de souris est pressé (début de l'interaction)
         *
         * Cet événement marque le début d'une interaction au bouton :
         *   - Clic gauche : sélection, activation, drag initiation
         *   - Clic droit : ouverture de menu contextuel
         *   - Clic milieu : pan de vue, ouverture de lien en nouvel onglet
         *   - Boutons latéraux : navigation historique (back/forward)
         *
         * @note La position rapportée est celle du curseur au moment du press.
         *       Pour un drag, comparer avec la position dans les NkMouseMoveEvent suivants.
         *
         * @note GetClickCount() permet de distinguer un simple-clic manuel
         *       d'un double-clic détecté par l'OS (ce dernier génère aussi
         *       NkMouseDoubleClickEvent).
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkMouseButtonPressEvent final : public NkMouseButtonEvent {
            public:
                /// @brief Associe le type d'événement NK_MOUSE_BUTTON_PRESSED à cette classe
                NK_EVENT_TYPE_FLAGS(NK_MOUSE_BUTTON_PRESSED)

                /**
                 * @brief Constructeur avec données de pression de bouton
                 * @param button Bouton concerné par la pression
                 * @param x Position horizontale client au moment du press [pixels]
                 * @param y Position verticale client au moment du press [pixels]
                 * @param screenX Position horizontale écran au moment du press [pixels]
                 * @param screenY Position verticale écran au moment du press [pixels]
                 * @param clickCount Nombre de clics détectés (1 = simple, 2 = double...)
                 * @param mods État des modificateurs clavier actifs
                 * @param windowId Identifiant de la fenêtre source
                 */
                NKENTSEU_EVENT_API NkMouseButtonPressEvent(
                    NkMouseButton button,
                    int32 x,
                    int32 y,
                    int32 screenX = 0,
                    int32 screenY = 0,
                    uint32 clickCount = 1,
                    const NkModifierState& mods = {},
                    uint64 windowId = 0
                ) noexcept
                    : NkMouseButtonEvent(
                        button,
                        NkButtonState::NK_PRESSED,
                        x, y, screenX, screenY,
                        clickCount, mods, windowId
                    ) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkMouseButtonPressEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le bouton pressé et sa position
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt(
                        "MouseButtonPress({0} {1},{2})",
                        NkMouseButtonToString(mButton), mX, mY
                    );
                }
        };

        // =====================================================================
        // NkMouseButtonReleaseEvent — Bouton souris relâché
        // =====================================================================

        /**
         * @brief Émis lorsqu'un bouton de souris est relâché (fin de l'interaction)
         *
         * Cet événement marque la fin d'une interaction au bouton :
         *   - Relâchement après clic : activation finale de l'action
         *   - Relâchement après drag : drop de l'élément déplacé
         *   - Relâchement de molette : arrêt du pan de vue
         *
         * @note Pour détecter un clic complet (press + release sans mouvement significatif),
         *       comparer la position dans NkMouseButtonPressEvent et NkMouseButtonReleaseEvent.
         *
         * @note Un relâchement hors de la zone de press peut avoir une sémantique différente :
         *       - Annulation d'un drag si hors bounds
         *       - Activation quand même si dans un élément cible (bouton, lien...)
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkMouseButtonReleaseEvent final : public NkMouseButtonEvent {
            public:
                /// @brief Associe le type d'événement NK_MOUSE_BUTTON_RELEASED à cette classe
                NK_EVENT_TYPE_FLAGS(NK_MOUSE_BUTTON_RELEASED)

                /**
                 * @brief Constructeur avec données de relâchement de bouton
                 * @param button Bouton concerné par le relâchement
                 * @param x Position horizontale client au moment du release [pixels]
                 * @param y Position verticale client au moment du release [pixels]
                 * @param screenX Position horizontale écran au moment du release [pixels]
                 * @param screenY Position verticale écran au moment du release [pixels]
                 * @param clickCount Nombre de clics détectés (1 = simple, 2 = double...)
                 * @param mods État des modificateurs clavier actifs
                 * @param windowId Identifiant de la fenêtre source
                 */
                NKENTSEU_EVENT_API NkMouseButtonReleaseEvent(
                    NkMouseButton button,
                    int32 x,
                    int32 y,
                    int32 screenX = 0,
                    int32 screenY = 0,
                    uint32 clickCount = 1,
                    const NkModifierState& mods = {},
                    uint64 windowId = 0
                ) noexcept
                    : NkMouseButtonEvent(
                        button,
                        NkButtonState::NK_RELEASED,
                        x, y, screenX, screenY,
                        clickCount, mods, windowId
                    ) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkMouseButtonReleaseEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le bouton relâché
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString("MouseButtonRelease(")
                         + NkMouseButtonToString(mButton) + ")";
                }
        };

        // =====================================================================
        // NkMouseDoubleClickEvent — Double-clic détecté par l'OS
        // =====================================================================

        /**
         * @brief Émis lorsqu'un double-clic est détecté par le système d'exploitation
         *
         * Cet événement est généré par l'OS quand deux clics rapides surviennent :
         *   - Sur le même bouton
         *   - Dans un intervalle de temps court (configuré dans les settings OS)
         *   - Avec un déplacement minimal du curseur entre les deux clics
         *
         * @note Cet événement est émis EN PLUS des deux NkMouseButtonPressEvent
         *       et NkMouseButtonReleaseEvent individuels. Les handlers doivent
         *       décider s'ils traitent le double-clic séparément ou l'ignorent.
         *
         * @note GetClickCount() retourne 2 pour cet événement, permettant
         *       de distinguer un double-clic OS d'un double-clic manuel rapide.
         *
         * @code
         *   // Exemple : traiter uniquement le double-clic, ignorer les simples
         *   if (auto* dbl = event.As<NkMouseDoubleClickEvent>()) {
         *       if (dbl->IsLeft()) {
         *           OpenItemAt(dbl->GetX(), dbl->GetY());
         *           event.MarkHandled();  // Empêche le traitement du simple-clic
         *       }
         *   }
         * @endcode
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkMouseDoubleClickEvent final : public NkMouseButtonEvent {
            public:
                /// @brief Associe le type d'événement NK_MOUSE_DOUBLE_CLICK à cette classe
                NK_EVENT_TYPE_FLAGS(NK_MOUSE_DOUBLE_CLICK)

                /**
                 * @brief Constructeur avec données de double-clic
                 * @param button Bouton concerné par le double-clic
                 * @param x Position horizontale client au moment du double-clic [pixels]
                 * @param y Position verticale client au moment du double-clic [pixels]
                 * @param screenX Position horizontale écran au moment du double-clic [pixels]
                 * @param screenY Position verticale écran au moment du double-clic [pixels]
                 * @param mods État des modificateurs clavier actifs
                 * @param windowId Identifiant de la fenêtre source
                 * @note clickCount est automatiquement fixé à 2 pour un double-clic
                 */
                NKENTSEU_EVENT_API NkMouseDoubleClickEvent(
                    NkMouseButton button,
                    int32 x,
                    int32 y,
                    int32 screenX = 0,
                    int32 screenY = 0,
                    const NkModifierState& mods = {},
                    uint64 windowId = 0
                ) noexcept
                    : NkMouseButtonEvent(
                        button,
                        NkButtonState::NK_PRESSED,
                        x, y, screenX, screenY,
                        2,  // clickCount = 2 pour double-clic
                        mods,
                        windowId
                    ) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkMouseDoubleClickEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le bouton du double-clic
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString("MouseDoubleClick(")
                         + NkMouseButtonToString(mButton) + ")";
                }
        };

        // =====================================================================
        // NkMouseWheelEvent — Classe de base pour événements de molette
        // =====================================================================

        /**
         * @brief Classe de base pour les événements de scrolling via molette ou trackpad
         *
         * Cette classe n'est pas instanciable directement. Utiliser :
         *   - NkMouseWheelVerticalEvent   : scroll vertical classique (molette)
         *   - NkMouseWheelHorizontalEvent : scroll horizontal (molette latérale)
         *   - NkMouseScrollEvent          : scroll unifié 2D (trackpad smooth-scroll)
         *
         * @note Deux systèmes de coordonnées sont supportés :
         *       - Delta logique (mDeltaX/Y) : en "clics de molette", portable entre devices
         *       - Delta pixel (mPixelDeltaX/Y) : en pixels physiques, pour précision trackpad
         *
         * @note IsHighPrecision() indique si l'événement provient d'un trackpad
         *       ou d'une molette haute résolution, permettant d'adapter le scrolling.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkMouseWheelEvent : public NkMouseEvent {
            public:
                /// @brief Retourne le delta logique horizontal (en clics de molette)
                /// @return float64 représentant le déplacement X logique (>0 = droite)
                NKENTSEU_EVENT_API_INLINE float64 GetDeltaX() const noexcept {
                    return mDeltaX;
                }

                /// @brief Retourne le delta logique vertical (en clics de molette)
                /// @return float64 représentant le déplacement Y logique (>0 = haut)
                NKENTSEU_EVENT_API_INLINE float64 GetDeltaY() const noexcept {
                    return mDeltaY;
                }

                /// @brief Alias vers GetDeltaX() pour compatibilité sémantique
                /// @return float64 représentant l'offset horizontal logique
                NKENTSEU_EVENT_API_INLINE float64 GetOffsetX() const noexcept {
                    return mDeltaX;
                }

                /// @brief Alias vers GetDeltaY() pour compatibilité sémantique
                /// @return float64 représentant l'offset vertical logique
                NKENTSEU_EVENT_API_INLINE float64 GetOffsetY() const noexcept {
                    return mDeltaY;
                }

                /// @brief Retourne le delta physique horizontal en pixels
                /// @return float64 représentant le déplacement X en pixels physiques
                /// @note Disponible uniquement si IsHighPrecision() == true
                NKENTSEU_EVENT_API_INLINE float64 GetPixelDeltaX() const noexcept {
                    return mPixelDeltaX;
                }

                /// @brief Retourne le delta physique vertical en pixels
                /// @return float64 représentant le déplacement Y en pixels physiques
                /// @note Disponible uniquement si IsHighPrecision() == true
                NKENTSEU_EVENT_API_INLINE float64 GetPixelDeltaY() const noexcept {
                    return mPixelDeltaY;
                }

                /// @brief Retourne la position horizontale du curseur au moment du scroll
                /// @return int32 représentant la coordonnée X client [pixels]
                NKENTSEU_EVENT_API_INLINE int32 GetX() const noexcept {
                    return mX;
                }

                /// @brief Retourne la position verticale du curseur au moment du scroll
                /// @return int32 représentant la coordonnée Y client [pixels]
                NKENTSEU_EVENT_API_INLINE int32 GetY() const noexcept {
                    return mY;
                }

                /// @brief Indique si l'événement provient d'un périphérique haute précision
                /// @return true pour trackpad ou molette smooth-scroll, false pour molette classique
                /// @note Si true, privilégier GetPixelDeltaX/Y pour un scrolling fluide
                NKENTSEU_EVENT_API_INLINE bool IsHighPrecision() const noexcept {
                    return mHighPrecision;
                }

                /// @brief Retourne l'état des modificateurs clavier actifs
                /// @return NkModifierState indiquant Ctrl, Shift, Alt, etc.
                NKENTSEU_EVENT_API_INLINE NkModifierState GetModifiers() const noexcept {
                    return mModifiers;
                }

            protected:
                /// @brief Constructeur protégé — réservé aux classes dérivées
                /// @param dX Delta logique horizontal (en clics de molette)
                /// @param dY Delta logique vertical (en clics de molette)
                /// @param pdX Delta physique horizontal en pixels
                /// @param pdY Delta physique vertical en pixels
                /// @param x Position horizontale client au moment du scroll
                /// @param y Position verticale client au moment du scroll
                /// @param highPrecision true si événement haute précision (trackpad)
                /// @param mods État des modificateurs clavier actifs
                /// @param windowId Identifiant de la fenêtre source
                NKENTSEU_EVENT_API NkMouseWheelEvent(
                    float64 dX,
                    float64 dY,
                    float64 pdX,
                    float64 pdY,
                    int32 x,
                    int32 y,
                    bool highPrecision,
                    const NkModifierState& mods,
                    uint64 windowId
                ) noexcept
                    : NkMouseEvent(windowId)
                    , mDeltaX(dX)
                    , mDeltaY(dY)
                    , mPixelDeltaX(pdX)
                    , mPixelDeltaY(pdY)
                    , mX(x)
                    , mY(y)
                    , mHighPrecision(highPrecision)
                    , mModifiers(mods) {
                }

                float64 mDeltaX;          ///< Delta logique horizontal [clics de molette]
                float64 mDeltaY;          ///< Delta logique vertical [clics de molette]
                float64 mPixelDeltaX;     ///< Delta physique horizontal [pixels]
                float64 mPixelDeltaY;     ///< Delta physique vertical [pixels]
                int32 mX;                 ///< Position horizontale client [pixels]
                int32 mY;                 ///< Position verticale client [pixels]
                bool mHighPrecision;      ///< true = trackpad/haute précision
                NkModifierState mModifiers;  ///< État des modificateurs clavier actifs
        };

        // =====================================================================
        // NkMouseScrollEvent — Scroll unifié 2D pour trackpad smooth-scroll
        // =====================================================================

        /**
         * @brief Émis pour un scroll unifié avec deltas X et Y simultanés
         *
         * Cet événement est conçu pour les trackpads et périphériques smooth-scroll :
         *   - deltaX et deltaY peuvent être non-nuls simultanément (scroll diagonal)
         *   - pixelDeltaX/Y fournissent une précision sub-pixel pour fluidité
         *   - IsHighPrecision() retourne true pour adapter le comportement
         *
         * @note Différent de NkMouseWheelVertical/HorizontalEvent :
         *       - Ces derniers ont un seul axe non-nul à la fois
         *       - NkMouseScrollEvent supporte le scroll 2D simultané
         *
         * @note Pour un scrolling fluide :
         *       - Utiliser GetPixelDeltaX/Y si IsHighPrecision() == true
         *       - Sinon, fallback sur GetDeltaX/Y (en clics de molette)
         *       - Accumuler les fractions non-entières pour éviter la dérive
         *
         * @code
         *   // Exemple de handler adaptatif
         *   void OnScroll(NkMouseScrollEvent& event) {
         *       float64 dx = event.IsHighPrecision()
         *           ? event.GetPixelDeltaX()
         *           : event.GetDeltaX() * kPixelsPerWheelClick;
         *       float64 dy = event.IsHighPrecision()
         *           ? event.GetPixelDeltaY()
         *           : event.GetDeltaY() * kPixelsPerWheelClick;
         *       ScrollView::ScrollBy(dx, dy);
         *   }
         * @endcode
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkMouseScrollEvent final : public NkMouseEvent {
            public:
                /// @brief Associe le type d'événement NK_MOUSE_SCROLL à cette classe
                NK_EVENT_TYPE_FLAGS(NK_MOUSE_SCROLL)

                /**
                 * @brief Constructeur avec paramètres de scroll unifié 2D
                 * @param deltaX Delta logique horizontal [clics de molette] (>0 = droite)
                 * @param deltaY Delta logique vertical [clics de molette] (>0 = haut)
                 * @param x Position horizontale client au moment du scroll [pixels]
                 * @param y Position verticale client au moment du scroll [pixels]
                 * @param pixelDeltaX Delta physique horizontal [pixels] pour haute précision
                 * @param pixelDeltaY Delta physique vertical [pixels] pour haute précision
                 * @param highPrecision true si événement trackpad/haute précision
                 * @param mods État des modificateurs clavier actifs
                 * @param windowId Identifiant de la fenêtre source
                 */
                NKENTSEU_EVENT_API NkMouseScrollEvent(
                    float64 deltaX,
                    float64 deltaY,
                    int32 x = 0,
                    int32 y = 0,
                    float64 pixelDeltaX = 0.0,
                    float64 pixelDeltaY = 0.0,
                    bool highPrecision = false,
                    const NkModifierState& mods = {},
                    uint64 windowId = 0
                ) noexcept
                    : NkMouseEvent(windowId)
                    , mDeltaX(deltaX)
                    , mDeltaY(deltaY)
                    , mPixelDeltaX(pixelDeltaX)
                    , mPixelDeltaY(pixelDeltaY)
                    , mX(x)
                    , mY(y)
                    , mHighPrecision(highPrecision)
                    , mModifiers(mods) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkMouseScrollEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant les deltas de scroll X et Y
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt("MouseScroll({0},{1})", mDeltaX, mDeltaY);
                }

                /// @brief Retourne le delta logique horizontal
                /// @return float64 représentant le déplacement X [clics de molette]
                NKENTSEU_EVENT_API_INLINE float64 GetDeltaX() const noexcept {
                    return mDeltaX;
                }

                /// @brief Retourne le delta logique vertical
                /// @return float64 représentant le déplacement Y [clics de molette]
                NKENTSEU_EVENT_API_INLINE float64 GetDeltaY() const noexcept {
                    return mDeltaY;
                }

                /// @brief Retourne le delta physique horizontal en pixels
                /// @return float64 représentant le déplacement X [pixels]
                NKENTSEU_EVENT_API_INLINE float64 GetPixelDeltaX() const noexcept {
                    return mPixelDeltaX;
                }

                /// @brief Retourne le delta physique vertical en pixels
                /// @return float64 représentant le déplacement Y [pixels]
                NKENTSEU_EVENT_API_INLINE float64 GetPixelDeltaY() const noexcept {
                    return mPixelDeltaY;
                }

                /// @brief Retourne la position horizontale du curseur au moment du scroll
                /// @return int32 représentant la coordonnée X client [pixels]
                NKENTSEU_EVENT_API_INLINE int32 GetX() const noexcept {
                    return mX;
                }

                /// @brief Retourne la position verticale du curseur au moment du scroll
                /// @return int32 représentant la coordonnée Y client [pixels]
                NKENTSEU_EVENT_API_INLINE int32 GetY() const noexcept {
                    return mY;
                }

                /// @brief Indique si l'événement provient d'un périphérique haute précision
                /// @return true pour trackpad ou molette smooth-scroll
                NKENTSEU_EVENT_API_INLINE bool IsHighPrecision() const noexcept {
                    return mHighPrecision;
                }

                /// @brief Retourne l'état des modificateurs clavier actifs
                /// @return NkModifierState indiquant Ctrl, Shift, Alt, etc.
                NKENTSEU_EVENT_API_INLINE NkModifierState GetModifiers() const noexcept {
                    return mModifiers;
                }

                /// @brief Indique si le scroll est vers le haut
                /// @return true si GetDeltaY() > 0.0
                NKENTSEU_EVENT_API_INLINE bool ScrollsUp() const noexcept {
                    return mDeltaY > 0.0;
                }

                /// @brief Indique si le scroll est vers le bas
                /// @return true si GetDeltaY() < 0.0
                NKENTSEU_EVENT_API_INLINE bool ScrollsDown() const noexcept {
                    return mDeltaY < 0.0;
                }

                /// @brief Indique si le scroll est vers la droite
                /// @return true si GetDeltaX() > 0.0
                NKENTSEU_EVENT_API_INLINE bool ScrollsRight() const noexcept {
                    return mDeltaX > 0.0;
                }

                /// @brief Indique si le scroll est vers la gauche
                /// @return true si GetDeltaX() < 0.0
                NKENTSEU_EVENT_API_INLINE bool ScrollsLeft() const noexcept {
                    return mDeltaX < 0.0;
                }

            private:
                float64 mDeltaX;          ///< Delta logique horizontal [clics de molette]
                float64 mDeltaY;          ///< Delta logique vertical [clics de molette]
                float64 mPixelDeltaX;     ///< Delta physique horizontal [pixels]
                float64 mPixelDeltaY;     ///< Delta physique vertical [pixels]
                int32 mX;                 ///< Position horizontale client [pixels]
                int32 mY;                 ///< Position verticale client [pixels]
                bool mHighPrecision;      ///< true = trackpad/haute précision
                NkModifierState mModifiers;  ///< État des modificateurs clavier actifs
        };

        // =====================================================================
        // NkMouseWheelVerticalEvent — Molette verticale classique
        // =====================================================================

        /**
         * @brief Émis pour un scroll vertical via molette classique
         *
         * Cet événement représente le scrolling traditionnel à molette :
         *   - deltaY : déplacement en "clics de molette" (typiquement ±1, ±3...)
         *   - pixelDeltaY : précision sub-pixel pour trackpads (si disponible)
         *   - deltaX et pixelDeltaX sont toujours à 0 pour cet événement
         *
         * @note Pour compatibilité avec les anciennes APIs, utiliser GetDeltaY()
         *       et multiplier par un facteur de conversion (ex: 20 pixels/clic).
         *
         * @note Si IsHighPrecision() == true, privilégier GetPixelDeltaY()
         *       pour un scrolling fluide et naturel sur trackpad.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkMouseWheelVerticalEvent final : public NkMouseWheelEvent {
            public:
                /// @brief Associe le type d'événement NK_MOUSE_WHEEL_VERTICAL à cette classe
                NK_EVENT_TYPE_FLAGS(NK_MOUSE_WHEEL_VERTICAL)

                /**
                 * @brief Constructeur avec paramètres de scroll vertical
                 * @param deltaY Delta logique vertical [clics de molette] (>0 = haut)
                 * @param x Position horizontale client au moment du scroll [pixels]
                 * @param y Position verticale client au moment du scroll [pixels]
                 * @param pixelDeltaY Delta physique vertical [pixels] pour haute précision
                 * @param highPrecision true si événement trackpad/haute précision
                 * @param mods État des modificateurs clavier actifs
                 * @param windowId Identifiant de la fenêtre source
                 */
                NKENTSEU_EVENT_API NkMouseWheelVerticalEvent(
                    float64 deltaY,
                    int32 x = 0,
                    int32 y = 0,
                    float64 pixelDeltaY = 0.0,
                    bool highPrecision = false,
                    const NkModifierState& mods = {},
                    uint64 windowId = 0
                ) noexcept
                    : NkMouseWheelEvent(
                        0.0, deltaY,           // dX = 0, dY = deltaY
                        0.0, pixelDeltaY,      // pdX = 0, pdY = pixelDeltaY
                        x, y, highPrecision, mods, windowId
                    ) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkMouseWheelVerticalEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le delta vertical de scroll
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt("MouseWheelV({0})", mDeltaY);
                }

                /// @brief Indique si le scroll est vers le haut
                /// @return true si GetDeltaY() > 0.0
                NKENTSEU_EVENT_API_INLINE bool ScrollsUp() const noexcept {
                    return mDeltaY > 0.0;
                }

                /// @brief Indique si le scroll est vers le bas
                /// @return true si GetDeltaY() < 0.0
                NKENTSEU_EVENT_API_INLINE bool ScrollsDown() const noexcept {
                    return mDeltaY < 0.0;
                }
        };

        // =====================================================================
        // NkMouseWheelHorizontalEvent — Molette horizontale
        // =====================================================================

        /**
         * @brief Émis pour un scroll horizontal via molette latérale ou Shift+molette
         *
         * Cet événement représente le scrolling horizontal :
         *   - deltaX : déplacement en "clics de molette" (typiquement ±1, ±3...)
         *   - pixelDeltaX : précision sub-pixel pour trackpads (si disponible)
         *   - deltaY et pixelDeltaY sont toujours à 0 pour cet événement
         *
         * @note Certaines souris n'ont pas de molette horizontale dédiée.
         *       Dans ce cas, l'application peut mapper Shift+molette verticale
         *       vers cet événement pour une expérience utilisateur cohérente.
         *
         * @note Pour les tableaux larges ou les timelines, ce scrolling horizontal
         *       est essentiel pour la navigation sans barre de défilement visible.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkMouseWheelHorizontalEvent final : public NkMouseWheelEvent {
            public:
                /// @brief Associe le type d'événement NK_MOUSE_WHEEL_HORIZONTAL à cette classe
                NK_EVENT_TYPE_FLAGS(NK_MOUSE_WHEEL_HORIZONTAL)

                /**
                 * @brief Constructeur avec paramètres de scroll horizontal
                 * @param deltaX Delta logique horizontal [clics de molette] (>0 = droite)
                 * @param x Position horizontale client au moment du scroll [pixels]
                 * @param y Position verticale client au moment du scroll [pixels]
                 * @param pixelDeltaX Delta physique horizontal [pixels] pour haute précision
                 * @param highPrecision true si événement trackpad/haute précision
                 * @param mods État des modificateurs clavier actifs
                 * @param windowId Identifiant de la fenêtre source
                 */
                NKENTSEU_EVENT_API NkMouseWheelHorizontalEvent(
                    float64 deltaX,
                    int32 x = 0,
                    int32 y = 0,
                    float64 pixelDeltaX = 0.0,
                    bool highPrecision = false,
                    const NkModifierState& mods = {},
                    uint64 windowId = 0
                ) noexcept
                    : NkMouseWheelEvent(
                        deltaX, 0.0,              // dX = deltaX, dY = 0
                        pixelDeltaX, 0.0,         // pdX = pixelDeltaX, pdY = 0
                        x, y, highPrecision, mods, windowId
                    ) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkMouseWheelHorizontalEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le delta horizontal de scroll
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt("MouseWheelH({0})", mDeltaX);
                }

                /// @brief Indique si le scroll est vers la gauche
                /// @return true si GetDeltaX() < 0.0
                NKENTSEU_EVENT_API_INLINE bool ScrollsLeft() const noexcept {
                    return mDeltaX < 0.0;
                }

                /// @brief Indique si le scroll est vers la droite
                /// @return true si GetDeltaX() > 0.0
                NKENTSEU_EVENT_API_INLINE bool ScrollsRight() const noexcept {
                    return mDeltaX > 0.0;
                }
        };

        // =====================================================================
        // NkMouseEnterEvent / NkMouseLeaveEvent
        // Entrée/sortie du curseur dans la zone client de la fenêtre
        // =====================================================================

        /**
         * @brief Émis lorsque le curseur de la souris entre dans la zone client de la fenêtre
         *
         * Cet événement marque l'entrée du curseur dans la zone de contenu :
         *   - La zone client exclut les bordures, barre de titre, menu...
         *   - Utile pour activer les hover states, tooltips, highlights
         *   - Peut être utilisé pour démarrer un tracking de souris ciblé
         *
         * @note Cet événement n'est émis qu'une fois par entrée, pas à chaque mouvement.
         *       Pour le suivi continu, utiliser NkMouseMoveEvent après réception.
         *
         * @note Si la fenêtre est derrière une autre, cet événement ne sera pas émis
         *       même si le curseur passe "dessus" géométriquement — l'OS gère le hit-testing.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkMouseEnterEvent final : public NkMouseEvent {
            public:
                /// @brief Associe le type d'événement NK_MOUSE_ENTER à cette classe
                NK_EVENT_TYPE_FLAGS(NK_MOUSE_ENTER)

                /// @brief Constructeur avec identifiant de fenêtre
                /// @param windowId Identifiant de la fenêtre concernée par l'entrée
                NKENTSEU_EVENT_API NkMouseEnterEvent(uint64 windowId = 0) noexcept
                    : NkMouseEvent(windowId) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkMouseEnterEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString constant "MouseEnter()" pour identification
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return "MouseEnter()";
                }
        };

        /**
         * @brief Émis lorsque le curseur de la souris quitte la zone client de la fenêtre
         *
         * Cet événement marque la sortie du curseur de la zone de contenu :
         *   - Utile pour désactiver les hover states, masquer les tooltips
         *   - Peut annuler un drag-and-drop si le curseur sort de la zone valide
         *   - Peut déclencher un fallback vers un autre handler de souris
         *
         * @note Cet événement n'est pas émis si la fenêtre est détruite ou minimisée
         *       pendant que le curseur est à l'intérieur — gérer ces cas séparément.
         *
         * @note Après cet événement, les NkMouseMoveEvent ne seront plus émis
         *       pour cette fenêtre jusqu'au prochain NkMouseEnterEvent.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkMouseLeaveEvent final : public NkMouseEvent {
            public:
                /// @brief Associe le type d'événement NK_MOUSE_LEAVE à cette classe
                NK_EVENT_TYPE_FLAGS(NK_MOUSE_LEAVE)

                /// @brief Constructeur avec identifiant de fenêtre
                /// @param windowId Identifiant de la fenêtre concernée par la sortie
                NKENTSEU_EVENT_API NkMouseLeaveEvent(uint64 windowId = 0) noexcept
                    : NkMouseEvent(windowId) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkMouseLeaveEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString constant "MouseLeave()" pour identification
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return "MouseLeave()";
                }
        };

        // =====================================================================
        // NkMouseWindowEnterEvent / NkMouseWindowLeaveEvent
        // Entrée/sortie du curseur dans la fenêtre complète (cadre inclus)
        // =====================================================================

        /**
         * @brief Émis lorsque le curseur entre dans les bounds de la fenêtre (cadre inclus)
         *
         * Différent de NkMouseEnterEvent :
         *   - Inclut la barre de titre, les bordures, le menu système
         *   - Utile pour les effets de fenêtre (glow, highlight au hover)
         *   - Moins fréquent que l'entrée zone client (cadre plus petit)
         *
         * @note Cet événement peut survenir avant NkMouseEnterEvent si le curseur
         *       entre d'abord par la barre de titre puis descend vers le contenu.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkMouseWindowEnterEvent final : public NkMouseEvent {
            public:
                /// @brief Associe le type d'événement NK_MOUSE_WINDOW_ENTER à cette classe
                NK_EVENT_TYPE_FLAGS(NK_MOUSE_WINDOW_ENTER)

                /// @brief Constructeur avec identifiant de fenêtre
                /// @param windowId Identifiant de la fenêtre concernée par l'entrée
                NKENTSEU_EVENT_API NkMouseWindowEnterEvent(uint64 windowId = 0) noexcept
                    : NkMouseEvent(windowId) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkMouseWindowEnterEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString constant "MouseWindowEnter()" pour identification
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return "MouseWindowEnter()";
                }
        };

        /**
         * @brief Émis lorsque le curseur quitte les bounds de la fenêtre (cadre inclus)
         *
         * Différent de NkMouseLeaveEvent :
         *   - Inclut la sortie par la barre de titre ou les bordures
         *   - Utile pour désactiver les effets de fenêtre au hover
         *   - Peut survenir avant NkMouseLeaveEvent si sortie par le haut
         *
         * @note Après cet événement, la fenêtre ne recevra plus d'événements souris
         *       jusqu'au prochain NkMouseWindowEnterEvent, sauf capture active.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkMouseWindowLeaveEvent final : public NkMouseEvent {
            public:
                /// @brief Associe le type d'événement NK_MOUSE_WINDOW_LEAVE à cette classe
                NK_EVENT_TYPE_FLAGS(NK_MOUSE_WINDOW_LEAVE)

                /// @brief Constructeur avec identifiant de fenêtre
                /// @param windowId Identifiant de la fenêtre concernée par la sortie
                NKENTSEU_EVENT_API NkMouseWindowLeaveEvent(uint64 windowId = 0) noexcept
                    : NkMouseEvent(windowId) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkMouseWindowLeaveEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString constant "MouseWindowLeave()" pour identification
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return "MouseWindowLeave()";
                }
        };

        // =====================================================================
        // NkMouseCaptureBeginEvent / NkMouseCaptureEndEvent
        // Capture exclusive de la souris pour drag-and-drop et interactions modales
        // =====================================================================

        /**
         * @brief Émis au début d'une capture exclusive de la souris par une fenêtre
         *
         * La capture souris force tous les événements souris vers cette fenêtre,
         * même quand le curseur sort de sa zone client :
         *   - Essentiel pour les drag-and-drop : suivre la souris hors bounds
         *   - Utile pour les interactions modales : slider, resize, rotation 3D
         *   - Permet de recevoir les release même si le curseur a quitté la cible
         *
         * @note Pendant la capture, cette fenêtre reçoit TOUS les événements souris,
         *       même si le curseur est géométriquement sur une autre fenêtre.
         *
         * @note La capture est typiquement initiée dans un NkMouseButtonPressEvent
         *       et libérée dans le NkMouseButtonReleaseEvent correspondant.
         *
         * @code
         *   // Exemple : démarrer un drag avec capture
         *   void OnMouseDown(NkMouseButtonPressEvent& event) {
         *       if (event.IsLeft() && IsDraggableAt(event.GetX(), event.GetY())) {
         *           StartDrag(event.GetX(), event.GetY());
         *           Window::CaptureMouse();  // Déclenche NkMouseCaptureBeginEvent
         *       }
         *   }
         *
         *   void OnMouseUp(NkMouseButtonReleaseEvent& event) {
         *       if (mIsDragging) {
         *           EndDrag(event.GetX(), event.GetY());
         *           Window::ReleaseMouseCapture();  // Déclenche NkMouseCaptureEndEvent
         *       }
         *   }
         * @endcode
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkMouseCaptureBeginEvent final : public NkMouseEvent {
            public:
                /// @brief Associe le type d'événement NK_MOUSE_CAPTURE_BEGIN à cette classe
                NK_EVENT_TYPE_FLAGS(NK_MOUSE_CAPTURE_BEGIN)

                /// @brief Constructeur avec identifiant de fenêtre
                /// @param windowId Identifiant de la fenêtre qui capture la souris
                NKENTSEU_EVENT_API NkMouseCaptureBeginEvent(uint64 windowId = 0) noexcept
                    : NkMouseEvent(windowId) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkMouseCaptureBeginEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString constant "MouseCaptureBegin()" pour identification
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return "MouseCaptureBegin()";
                }
        };

        /**
         * @brief Émis à la fin d'une capture exclusive de la souris
         *
         * Cet événement marque la libération de la capture :
         *   - Les événements souris retournent au hit-testing normal
         *   - La fenêtre ne reçoit plus les événements hors de sa zone
         *   - Utile pour nettoyer l'état de drag ou d'interaction modale
         *
         * @note Cet événement peut survenir même si NkMouseCaptureBeginEvent
         *       n'a pas été explicitement géré — l'OS peut forcer la libération
         *       (ex: perte de focus, changement de fenêtre active).
         *
         * @note Toujours libérer la capture dans le handler de release correspondant
         *       pour éviter des états bloqués où la souris reste "capturée".
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkMouseCaptureEndEvent final : public NkMouseEvent {
            public:
                /// @brief Associe le type d'événement NK_MOUSE_CAPTURE_END à cette classe
                NK_EVENT_TYPE_FLAGS(NK_MOUSE_CAPTURE_END)

                /// @brief Constructeur avec identifiant de fenêtre
                /// @param windowId Identifiant de la fenêtre qui libère la capture
                NKENTSEU_EVENT_API NkMouseCaptureEndEvent(uint64 windowId = 0) noexcept
                    : NkMouseEvent(windowId) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkMouseCaptureEndEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString constant "MouseCaptureEnd()" pour identification
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return "MouseCaptureEnd()";
                }
        };

    } // namespace nkentseu

#endif // NKENTSEU_EVENT_NKMOUSEEVENT_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKMOUSEEVENT.H
// =============================================================================
// Ce fichier définit la hiérarchie des événements souris pour le module NKEvent.
// Les exemples ci-dessous illustrent les patterns d'usage recommandés.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Drag-and-drop avec capture souris et feedback visuel
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkMouseEvent.h"

    class DragDropHandler {
    public:
        void OnMouseButtonPress(nkentseu::NkMouseButtonPressEvent& event) {
            using namespace nkentseu;

            // Démarrer un drag si clic gauche sur un élément draggable
            if (event.IsLeft()) {
                auto* draggable = FindDraggableAt(event.GetX(), event.GetY());
                if (draggable) {
                    mDraggedItem = draggable;
                    mDragStartPos = { event.GetX(), event.GetY() };
                    mDragOffset = draggable->GetPosition() - mDragStartPos;

                    // Démarrer la capture pour suivre la souris hors bounds
                    event.GetWindow()->CaptureMouse();
                    event.MarkHandled();  // Empêche le clic normal
                }
            }
        }

        void OnMouseMove(nkentseu::NkMouseMoveEvent& event) {
            using namespace nkentseu;

            if (mDraggedItem && event.IsButtonDown(NkMouseButton::NK_MB_LEFT)) {
                // Mise à jour de la position avec offset pour stabilité
                Vec2 newPos = { event.GetX(), event.GetY() } + mDragOffset;
                mDraggedItem->SetPosition(newPos);

                // Feedback visuel : highlight de la cible de drop
                auto* dropTarget = FindDropTargetAt(event.GetX(), event.GetY());
                UpdateDropHighlight(dropTarget);
            }
        }

        void OnMouseButtonRelease(nkentseu::NkMouseButtonReleaseEvent& event) {
            using namespace nkentseu;

            if (mDraggedItem) {
                // Finaliser le drop
                auto* target = FindDropTargetAt(event.GetX(), event.GetY());
                if (target && target->AcceptsDrop(mDraggedItem)) {
                    target->OnDrop(mDraggedItem);
                }

                // Nettoyage et libération de la capture
                mDraggedItem = nullptr;
                ClearDropHighlight();
                event.GetWindow()->ReleaseMouseCapture();
            }
        }

        void OnMouseCaptureEnd(nkentseu::NkMouseCaptureEndEvent&) {
            // Fallback : libération forcée par l'OS (perte de focus...)
            mDraggedItem = nullptr;
            ClearDropHighlight();
        }

    private:
        Draggable* mDraggedItem = nullptr;
        Vec2 mDragStartPos = {0, 0};
        Vec2 mDragOffset = {0, 0};

        Draggable* FindDraggableAt(int32 x, int32 y);
        DropTarget* FindDropTargetAt(int32 x, int32 y);
        void UpdateDropHighlight(DropTarget* target);
        void ClearDropHighlight();
    };
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Contrôle de caméra 3D avec souris (style FPS)
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkMouseEvent.h"

    class FPSCameraController {
    public:
        void OnMouseButtonPress(nkentseu::NkMouseButtonPressEvent& event) {
            using namespace nkentseu;

            // Clic droit = mode look/rotation
            if (event.IsRight()) {
                mIsLooking = true;
                mLastMousePos = { event.GetX(), event.GetY() };

                // Capture pour rotation continue même hors bounds
                event.GetWindow()->CaptureMouse();
                event.GetWindow()->HideCursor();  // Curseur invisible pour immersion
                event.MarkHandled();
            }
        }

        void OnMouseRaw(nkentseu::NkMouseRawEvent& event) {
            using namespace nkentseu;

            // Utiliser le mouvement brut pour rotation précise (sans accélération OS)
            if (mIsLooking) {
                float sensitivity = 0.002f;  // Radians par compte de capteur

                // Rotation horizontale (yaw)
                mCamera.Yaw(event.GetDeltaX() * sensitivity);

                // Rotation verticale (pitch) avec clamping pour éviter le flip
                float pitchDelta = event.GetDeltaY() * sensitivity;
                mCamera.Pitch(NkClamp(pitchDelta, -NkPi/2 + 0.1f, NkPi/2 - 0.1f));
            }
        }

        void OnMouseWheelVertical(nkentseu::NkMouseWheelVerticalEvent& event) {
            using namespace nkentseu;

            // Molette = zoom avant/arrière
            float zoomSpeed = event.IsHighPrecision() ? 0.1f : 1.0f;
            float zoomDelta = event.GetDeltaY() * zoomSpeed;

            mCamera.Zoom(zoomDelta, event.GetX(), event.GetY());
        }

        void OnMouseButtonRelease(nkentseu::NkMouseButtonReleaseEvent& event) {
            using namespace nkentseu;

            // Fin du mode look : restaurer le curseur et libérer la capture
            if (event.IsRight() && mIsLooking) {
                mIsLooking = false;
                event.GetWindow()->ReleaseMouseCapture();
                event.GetWindow()->ShowCursor();
            }
        }

    private:
        bool mIsLooking = false;
        Vec2 mLastMousePos = {0, 0};
        Camera& mCamera;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Scroll fluide adaptatif (molette vs trackpad)
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkMouseEvent.h"

    class SmoothScrollView {
    public:
        void OnMouseScroll(nkentseu::NkMouseScrollEvent& event) {
            using namespace nkentseu;

            // Choisir la source de delta selon la précision du périphérique
            float64 dx, dy;
            if (event.IsHighPrecision()) {
                // Trackpad : utiliser les deltas pixels pour fluidité sub-pixel
                dx = event.GetPixelDeltaX();
                dy = event.GetPixelDeltaY();
            }
            else {
                // Molette classique : convertir les clics en pixels
                constexpr float64 kPixelsPerClick = 20.0;
                dx = event.GetDeltaX() * kPixelsPerClick;
                dy = event.GetDeltaY() * kPixelsPerClick;
            }

            // Scroll avec inertie optionnelle
            ScrollBy(dx, dy, kEnableInertia);

            // Modificateurs pour comportements spéciaux
            if (event.GetModifiers().IsCtrl()) {
                // Ctrl+scroll = zoom au lieu de scroll
                ZoomAt(event.GetX(), event.GetY(), 1.0f + dy * 0.01f);
            }
        }

        void OnMouseWheelVertical(nkentseu::NkMouseWheelVerticalEvent& event) {
            using namespace nkentseu;

            // Fallback pour les anciens handlers qui n'écoutent pas NkMouseScrollEvent
            float64 dy = event.IsHighPrecision()
                ? event.GetPixelDeltaY()
                : event.GetDeltaY() * 20.0;

            ScrollBy(0.0, dy, kEnableInertia);
        }

    private:
        static constexpr bool kEnableInertia = true;

        void ScrollBy(float64 dx, float64 dy, bool withInertia);
        void ZoomAt(int32 x, int32 y, float scale);
    };
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Détection de gestes souris (double-clic, drag, right-click menu)
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkMouseEvent.h"

    class MouseGestureRecognizer {
    public:
        void OnMouseDoubleClick(nkentseu::NkMouseDoubleClickEvent& event) {
            using namespace nkentseu;

            // Double-clic gauche = sélection de mot ou zoom
            if (event.IsLeft()) {
                SelectWordAt(event.GetX(), event.GetY());
                event.MarkHandled();  // Empêche le traitement du simple-clic
            }
        }

        void OnMouseButtonPress(nkentseu::NkMouseButtonPressEvent& event) {
            using namespace nkentseu;

            // Clic droit = menu contextuel
            if (event.IsRight()) {
                ShowContextMenu(event.GetX(), event.GetY());
                event.MarkHandled();
            }

            // Clic milieu = ouverture de lien en nouvel onglet (navigateur)
            if (event.IsMiddle()) {
                auto* link = FindLinkAt(event.GetX(), event.GetY());
                if (link) {
                    OpenLinkInNewTab(link->GetUrl());
                    event.MarkHandled();
                }
            }
        }

        void OnMouseMove(nkentseu::NkMouseMoveEvent& event) {
            using namespace nkentseu;

            // Détection de drag après press + mouvement significatif
            if (mIsPotentialDrag && event.IsButtonDown(NkMouseButton::NK_MB_LEFT)) {
                int32 distance = NkDistance(mDragStartPos, {event.GetX(), event.GetY()});
                if (distance > kDragThreshold) {
                    StartDragOperation(mDragStartItem, event.GetX(), event.GetY());
                    mIsPotentialDrag = false;
                }
            }
        }

        void OnMouseButtonRelease(nkentseu::NkMouseButtonReleaseEvent& event) {
            using namespace nkentseu;

            // Annulation du drag potentiel si pas assez de mouvement
            if (mIsPotentialDrag) {
                mIsPotentialDrag = false;
                // Le clic normal sera traité par d'autres handlers
            }
        }

    private:
        bool mIsPotentialDrag = false;
        Vec2 mDragStartPos = {0, 0};
        Draggable* mDragStartItem = nullptr;
        static constexpr int32 kDragThreshold = 5;  // Pixels minimum pour un drag
    };
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Hover states et tooltips avec gestion d'entrée/sortie
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkMouseEvent.h"

    class TooltipManager {
    public:
        void OnMouseEnter(nkentseu::NkMouseEnterEvent& event) {
            using namespace nkentseu;

            // Démarrer un timer pour afficher le tooltip après délai
            mHoverTimer.Start(kTooltipDelayMs, [this, windowId = event.GetWindowId()]() {
                if (mIsHovering) {
                    ShowTooltipAt(mLastHoverPos, windowId);
                }
            });
        }

        void OnMouseMove(nkentseu::NkMouseMoveEvent& event) {
            using namespace nkentseu;

            // Mise à jour de la position pour le tooltip
            mLastHoverPos = { event.GetX(), event.GetY() };

            // Détection de l'élément sous le curseur
            auto* hoverable = FindHoverableAt(event.GetX(), event.GetY());
            if (hoverable != mCurrentHoverable) {
                // Changement d'élément : reset du timer et mise à jour
                mHoverTimer.Stop();
                HideTooltip();
                mCurrentHoverable = hoverable;

                if (hoverable && hoverable->HasTooltip()) {
                    mIsHovering = true;
                    mHoverTimer.Start(kTooltipDelayMs, [this, windowId = event.GetWindowId()]() {
                        if (mIsHovering) {
                            ShowTooltipAt(mLastHoverPos, windowId, mCurrentHoverable->GetTooltip());
                        }
                    });
                }
            }
        }

        void OnMouseLeave(nkentseu::NkMouseLeaveEvent&) {
            using namespace nkentseu;

            // Sortie de la zone : annuler le tooltip en attente
            mIsHovering = false;
            mHoverTimer.Stop();
            HideTooltip();
            mCurrentHoverable = nullptr;
        }

    private:
        static constexpr uint32 kTooltipDelayMs = 300;  // Délai avant affichage

        bool mIsHovering = false;
        Vec2 mLastHoverPos = {0, 0};
        Hoverable* mCurrentHoverable = nullptr;
        Timer mHoverTimer;

        Hoverable* FindHoverableAt(int32 x, int32 y);
        void ShowTooltipAt(Vec2 pos, uint64 windowId, const NkString& text);
        void HideTooltip();
    };
*/

// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
    1. PERFORMANCE DES HANDLERS DE MOUVEMENT :
       - NkMouseMoveEvent peut être émis 60-1000 fois/seconde selon le hardware
       - Éviter les allocations, calculs lourds, ou I/O dans ces handlers
       - Pour le rendu, throttler les mises à jour visuelles (ex: max 60 FPS)

    2. GESTION DE LA CAPTURE SOURIS :
       - Toujours libérer la capture dans le handler de release correspondant
       - Gérer OnMouseCaptureEnd() comme fallback pour libération forcée par l'OS
       - La capture empêche le hit-testing normal : tester soigneusement les interactions

    3. PRÉCISION DU SCROLL (MOLETTE VS TRACKPAD) :
       - IsHighPrecision() indique la source : adapter le facteur de conversion
       - Pour fluidité : accumuler les fractions non-entières des deltas logiques
       - Exemple : scrollFraction += delta * kPixelsPerClick; scrollBy(floor(scrollFraction)); scrollFraction -= floor(scrollFraction);

    4. DÉTECTION DE GESTES COMPLEXES :
       - Double-clic OS vs manuel : comparer timestamps et positions
       - Drag vs clic : seuil de mouvement (kDragThreshold) pour distinguer
       - Right-click menu : afficher au release, pas au press, pour compatibilité

    5. ACCESSIBILITÉ ET MODIFICATEURS :
       - Respecter GetModifiers() pour les raccourcis clavier+souris
       - Fournir des alternatives clavier pour toutes les actions souris
       - Tester avec différentes DPI et settings d'accélération souris OS

    6. EXTENSIBILITÉ :
       - Pour ajouter un nouvel événement souris :
         a) Dériver de NkMouseEvent (hérite automatiquement des catégories)
         b) Utiliser NK_EVENT_TYPE_FLAGS avec un nouveau type dans NkEventType
         c) Implémenter Clone() et ToString() pour cohérence avec l'infrastructure
         d) Documenter les paramètres et le cas d'usage dans la Doxygen
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================