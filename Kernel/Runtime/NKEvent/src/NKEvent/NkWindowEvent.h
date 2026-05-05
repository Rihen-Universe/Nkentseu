// =============================================================================
// NKEvent/NkWindowEvent.h
// Hiérarchie complète des événements liés à la gestion des fenêtres.
//
// Catégorie : NK_CAT_WINDOW
//
// Architecture :
//   - NkWindowTheme          : énumération des thèmes d'interface système
//   - NkWindowState          : codes d'état de fenêtre pour suivi et debugging
//   - NkWindowEvent          : classe de base abstraite pour tous les événements fenêtre
//   - Classes dérivées       : implémentations concrètes par type d'événement fenêtre
//
// Événements couverts :
//   - Cycle de vie    : Create, Close, Destroy
//   - Affichage       : Paint, Show, Hide, DpiChange, ThemeChange
//   - Géométrie       : Resize (+begin/end), Move (+begin/end)
//   - Focus           : FocusGained, FocusLost
//   - État            : Minimize, Maximize, Restore, Fullscreen, Windowed
//
// Design :
//   - Héritage polymorphe depuis NkEvent pour intégration au dispatcher central
//   - Macros NK_EVENT_* pour éviter la duplication de code boilerplate
//   - Méthodes Clone() pour copie profonde (replay, tests, sérialisation)
//   - ToString() surchargé pour débogage et logging significatif
//   - Accesseurs inline noexcept pour performance critique (lecture fréquente)
//
// Usage typique :
//   if (auto* resizeEvt = event.As<NkWindowResizeEvent>()) {
//       if (resizeEvt->GotLarger()) { Renderer::ExpandViewport(); }
//   }
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_EVENT_NKWINDOWEVENT_H
#define NKENTSEU_EVENT_NKWINDOWEVENT_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des dépendances internes au projet.
    // Toutes les dépendances utilisent les modules NK* du framework.

    #include "NKEvent/NkEvent.h"                        // Classe de base NkEvent + macros
    #include "NKEvent/NkEventApi.h"                     // Macros d'export NKENTSEU_EVENT_API
    #include "NKEvent/NkWindowId.h"                     // Type NkWindowId + constante INVALID
    #include "NKContainers/String/NkStringUtils.h"      // Conversion types → NkString
    #include "NKEvent/NkEventState.h"                   // États de fenêtre (si existant)
    #include <string>                                   // Pour compatibilité avec std::string si nécessaire

    namespace nkentseu {

        // =====================================================================
        // NkWindowTheme — Énumération des thèmes d'interface système
        // =====================================================================

        /// @brief Représente les thèmes d'interface disponibles au niveau du système d'exploitation
        /// @note Utilisé par NkWindowThemeEvent pour notifier les changements de préférence utilisateur
        enum class NKENTSEU_EVENT_CLASS_EXPORT NkWindowTheme : uint32 {
            NK_THEME_UNKNOWN       = 0,    ///< Thème non détecté ou non supporté par la plateforme
            NK_THEME_LIGHT,                ///< Thème clair : fond blanc/gris, texte sombre (par défaut)
            NK_THEME_DARK,                 ///< Thème sombre : fond noir/gris foncé, texte clair (Dark Mode)
            NK_THEME_HIGH_CONTRAST         ///< Mode contraste élevé : pour accessibilité visuelle
        };

        /// @brief Convertit une valeur NkWindowTheme en chaîne lisible pour le débogage
        /// @param t La valeur de thème à convertir
        /// @return Pointeur vers une chaîne littérale statique (ne pas libérer)
        /// @note Fonction noexcept pour usage dans les contextes critiques
        inline const char* NkWindowThemeToString(NkWindowTheme t) noexcept {
            switch (t) {
                case NkWindowTheme::NK_THEME_LIGHT:
                    return "LIGHT";
                case NkWindowTheme::NK_THEME_DARK:
                    return "DARK";
                case NkWindowTheme::NK_THEME_HIGH_CONTRAST:
                    return "HIGH_CONTRAST";
                case NkWindowTheme::NK_THEME_UNKNOWN:
                default:
                    return "UNKNOWN";
            }
        }

        // =====================================================================
        // NkWindowState — Codes d'état de fenêtre pour suivi et debugging
        // =====================================================================

        /// @brief Convertit un code d'état numérique en chaîne lisible pour le débogage
        /// @param code Valeur numérique représentant l'état de la fenêtre
        /// @return Pointeur vers une chaîne littérale statique (ne pas libérer)
        /// @note Cette fonction utilise des valeurs numériques explicites pour compatibilité
        ///       avec les APIs système qui peuvent retourner des codes entiers
        /// @warning Ne pas dépendre de l'ordre ou des valeurs numériques — utiliser les
        ///          constantes définies dans NkEventState.h si disponibles
        inline const char* NkWindowStateToString(uint32 code) noexcept {
            switch (code) {
                case 0:
                    return "UNDEFINED";    // NK_WINDOW_UNDEFINED : état initial/non défini
                case 1:
                    return "CREATED";      // NK_WINDOW_CREATED : fenêtre créée et prête
                case 2:
                    return "MINIMIZED";    // NK_WINDOW_MINIMIZED : réduite dans la taskbar/dock
                case 3:
                    return "MAXIMIZED";    // NK_WINDOW_MAXIMIZED : agrandie à l'écran de travail
                case 4:
                    return "FULLSCREEN";   // NK_WINDOW_FULLSCREEN : mode plein écran exclusif
                case 5:
                    return "HIDDEN";       // NK_WINDOW_HIDDEN : masquée mais toujours en mémoire
                case 6:
                    return "CLOSED";       // NK_WINDOW_CLOSED : fenêtre fermée et détruite
                default:
                    return "UNKNOWN";      // Code d'état non reconnu
            }
        }

        // =====================================================================
        // NkWindowEvent — Classe de base abstraite pour événements fenêtre
        // =====================================================================

        /**
         * @brief Classe de base polymorphe pour tous les événements liés aux fenêtres
         *
         * Catégorie associée : NK_CAT_WINDOW
         *
         * Cette classe sert de point d'ancrage commun pour tous les événements
         * de gestion de fenêtre : création, redimensionnement, focus, thème, etc.
         *
         * @note Toutes les classes dérivées héritent automatiquement de la catégorie
         *       NK_CAT_WINDOW via la surcharge de GetCategoryFlags().
         *
         * @note L'identifiant de fenêtre (windowId) est transmis au constructeur
         *       de NkEvent et accessible via GetWindowId() pour le routage.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowEvent : public NkEvent {
            public:
                /// @brief Retourne la catégorie WINDOW pour le filtrage par type
                /// @return Flags de catégorie NK_CAT_WINDOW en format uint32
                /// @note Surcharge virtuelle permettant le filtrage efficace des handlers
                uint32 GetCategoryFlags() const override {
                    return static_cast<uint32>(NkEventCategory::NK_CAT_WINDOW);
                }

            protected:
                /// @brief Constructeur protégé — réservé aux classes dérivées
                /// @param windowId Identifiant de la fenêtre source (0 = événement global)
                explicit NkWindowEvent(uint64 windowId = 0) noexcept
                    : NkEvent(windowId) {
                }
        };

        // =====================================================================
        // NkWindowCreateEvent — Événement de création de fenêtre
        // =====================================================================

        /**
         * @brief Émis lorsqu'une nouvelle fenêtre est créée et prête à l'emploi
         *
         * Cet événement notifie que :
         *   - La fenêtre a été allouée et initialisée par le système
         *   - Les ressources graphiques (surface, contexte) sont prêtes
         *   - La fenêtre est visible ou peut être affichée immédiatement
         *
         * @note Les dimensions initiales (width/height) peuvent être :
         *       - Spécifiées par l'application lors de la création
         *       - Déterminées par le système si non spécifiées (valeurs par défaut)
         *       - Ajustées par le window manager (constraints, multi-écran...)
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowCreateEvent final : public NkWindowEvent {
            public:
                /// @brief Associe le type d'événement NK_WINDOW_CREATE à cette classe
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_CREATE)

                /**
                 * @brief Constructeur avec dimensions initiales et identifiant
                 * @param width Largeur initiale de la fenêtre en pixels
                 * @param height Hauteur initiale de la fenêtre en pixels
                 * @param windowId Identifiant unique de la fenêtre créée
                 */
                NkWindowCreateEvent(
                    uint32 width = 0,
                    uint32 height = 0,
                    uint64 windowId = 0
                ) noexcept
                    : NkWindowEvent(windowId)
                    , mWidth(width)
                    , mHeight(height) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkWindowCreateEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant les dimensions initiales de la fenêtre
                NkString ToString() const override {
                    return NkString::Fmt("WindowCreate({0}x{1})", mWidth, mHeight);
                }

                /// @brief Retourne la largeur initiale de la fenêtre en pixels
                /// @return uint32 représentant la dimension horizontale
                NKENTSEU_EVENT_API_INLINE uint32 GetWidth() const noexcept {
                    return mWidth;
                }

                /// @brief Retourne la hauteur initiale de la fenêtre en pixels
                /// @return uint32 représentant la dimension verticale
                NKENTSEU_EVENT_API_INLINE uint32 GetHeight() const noexcept {
                    return mHeight;
                }

            private:
                uint32 mWidth;   ///< Largeur initiale de la fenêtre [pixels]
                uint32 mHeight;  ///< Hauteur initiale de la fenêtre [pixels]
        };

        // =====================================================================
        // NkWindowCloseEvent — Requête de fermeture de fenêtre
        // =====================================================================

        /**
         * @brief Émis lorsqu'une demande de fermeture de fenêtre est initiée
         *
         * IMPORTANT : Cet événement représente une REQUÊTE de fermeture, pas une
         * fermeture forcée. L'application peut :
         *   - Accepter la fermeture : laisser l'événement se propager normalement
         *   - Annuler la fermeture : appeler event.MarkHandled() pour bloquer
         *   - Demander confirmation : afficher "Enregistrer avant de quitter ?"
         *
         * Sources typiques de requête :
         *   - Clic utilisateur sur le bouton de fermeture (croix) de la fenêtre
         *   - Raccourci clavier (Alt+F4, Cmd+Q)
         *   - Appel programmatique via l'API de l'application
         *
         * @note Si IsForced() retourne true, la fermeture est imposée par le système
         *       (shutdown OS, kill process) et ne peut pas être annulée.
         *
         * @code
         *   void OnWindowClose(NkWindowCloseEvent& event) {
         *       if (event.IsForced()) {
         *           // Fermeture imposée : sauvegarde d'urgence
         *           Document::AutoSave();
         *           return;  // Ne pas annuler une fermeture forcée
         *       }
         *       if (Document::IsModified()) {
         *           event.MarkHandled();  // Annule la fermeture
         *           UIManager::ShowSaveConfirmationDialog();
         *       }
         *   }
         * @endcode
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowCloseEvent final : public NkWindowEvent {
            public:
                /// @brief Associe le type d'événement NK_WINDOW_CLOSE à cette classe
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_CLOSE)

                /**
                 * @brief Constructeur avec indicateur de fermeture forcée
                 * @param forced true si la fermeture est imposée par le système (non annulable)
                 * @param windowId Identifiant de la fenêtre concernée
                 */
                NkWindowCloseEvent(
                    bool forced = false,
                    uint64 windowId = 0
                ) noexcept
                    : NkWindowEvent(windowId)
                    , mForced(forced) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkWindowCloseEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString indiquant si la fermeture est forcée ou demandée
                NkString ToString() const override {
                    return NkString("WindowClose(forced=")
                         + (mForced ? "true" : "false") + ")";
                }

                /// @brief Indique si la fermeture est imposée par le système
                /// @return true si la fermeture ne peut pas être annulée
                /// @note Les fermetures forcées surviennent lors de shutdown OS, kill process...
                NKENTSEU_EVENT_API_INLINE bool IsForced() const noexcept {
                    return mForced;
                }

            private:
                bool mForced;  ///< true = fermeture imposée par le système (non annulable)
        };

        // =====================================================================
        // NkWindowDestroyEvent — Destruction définitive de fenêtre
        // =====================================================================

        /**
         * @brief Émis lorsqu'une fenêtre est définitivement détruite et libérée
         *
         * Cet événement marque la fin du cycle de vie de la fenêtre :
         *   - Toutes les ressources graphiques ont été libérées
         *   - Le contexte de rendu a été détruit
         *   - L'identifiant de fenêtre ne doit plus être utilisé
         *
         * @warning Ne pas accéder aux ressources de la fenêtre après cet événement.
         *          Toute tentative d'utilisation de windowId après ce point entraîne
         *          un comportement indéfini (dangling reference, crash potentiel).
         *
         * @note Cet événement est émis APRÈS NkWindowCloseEvent, une fois que la
         *       fermeture a été confirmée et exécutée par le système.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowDestroyEvent final : public NkWindowEvent {
            public:
                /// @brief Associe le type d'événement NK_WINDOW_DESTROY à cette classe
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_DESTROY)

                /**
                 * @brief Constructeur avec identifiant de fenêtre
                 * @param windowId Identifiant de la fenêtre en cours de destruction
                 */
                NkWindowDestroyEvent(uint64 windowId = 0) noexcept
                    : NkWindowEvent(windowId) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkWindowDestroyEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString constant "WindowDestroy()" pour identification
                NkString ToString() const override {
                    return "WindowDestroy()";
                }
        };

        // =====================================================================
        // NkWindowPaintEvent — Demande de repaint de fenêtre
        // =====================================================================

        /**
         * @brief Émis lorsqu'un rafraîchissement graphique de la fenêtre est requis
         *
         * Cet événement permet un rendu optimisé via le concept de "dirty region" :
         *   - IsFullPaint() == true : toute la fenêtre doit être redessinée
         *   - IsFullPaint() == false : seule la zone rectangulaire (x,y,w,h) est invalide
         *
         * Sources typiques de repaint :
         *   - Exposition de la fenêtre après avoir été masquée/recouverte
         *   - Redimensionnement nécessitant un recalcul du layout
         *   - Mise à jour du contenu (animation, données dynamiques)
         *   - Changement de thème ou de DPI nécessitant un re-rendu
         *
         * @note Pour les performances, privilégier le repaint partiel quand possible :
         *       ne redessiner que la zone invalide réduit la charge GPU/CPU.
         *
         * @note Les coordonnées de la zone sale sont en pixels logiques (tenant
         *       compte du DPI scaling), pas en pixels physiques de l'écran.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowPaintEvent final : public NkWindowEvent {
            public:
                /// @brief Associe le type d'événement NK_WINDOW_PAINT à cette classe
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_PAINT)

                /**
                 * @brief Constructeur pour repaint complet de la fenêtre
                 * @param windowId Identifiant de la fenêtre à rafraîchir
                 */
                NkWindowPaintEvent(uint64 windowId = 0) noexcept
                    : NkWindowEvent(windowId)
                    , mDirtyX(0)
                    , mDirtyY(0)
                    , mDirtyW(0)
                    , mDirtyH(0)
                    , mFull(true) {
                }

                /**
                 * @brief Constructeur pour repaint d'une zone rectangulaire spécifique
                 * @param x Coordonnée X du coin supérieur gauche de la zone invalide
                 * @param y Coordonnée Y du coin supérieur gauche de la zone invalide
                 * @param w Largeur de la zone invalide en pixels
                 * @param h Hauteur de la zone invalide en pixels
                 * @param windowId Identifiant de la fenêtre à rafraîchir
                 */
                NkWindowPaintEvent(
                    int32 x,
                    int32 y,
                    uint32 w,
                    uint32 h,
                    uint64 windowId = 0
                ) noexcept
                    : NkWindowEvent(windowId)
                    , mDirtyX(x)
                    , mDirtyY(y)
                    , mDirtyW(w)
                    , mDirtyH(h)
                    , mFull(false) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkWindowPaintEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le type de repaint (full ou zone spécifique)
                NkString ToString() const override {
                    if (mFull) {
                        return "WindowPaint(full)";
                    }
                    return NkString::Fmt(
                        "WindowPaint({0},{1} {2}x{3})",
                        mDirtyX, mDirtyY, mDirtyW, mDirtyH
                    );
                }

                /// @brief Indique si un repaint complet de la fenêtre est requis
                /// @return true si toute la surface doit être redessinée
                NKENTSEU_EVENT_API_INLINE bool IsFullPaint() const noexcept {
                    return mFull;
                }

                /// @brief Retourne la coordonnée X de la zone invalide
                /// @return int32 représentant la position horizontale du coin supérieur gauche
                NKENTSEU_EVENT_API_INLINE int32 GetDirtyX() const noexcept {
                    return mDirtyX;
                }

                /// @brief Retourne la coordonnée Y de la zone invalide
                /// @return int32 représentant la position verticale du coin supérieur gauche
                NKENTSEU_EVENT_API_INLINE int32 GetDirtyY() const noexcept {
                    return mDirtyY;
                }

                /// @brief Retourne la largeur de la zone invalide
                /// @return uint32 représentant l'étendue horizontale en pixels
                NKENTSEU_EVENT_API_INLINE uint32 GetDirtyW() const noexcept {
                    return mDirtyW;
                }

                /// @brief Retourne la hauteur de la zone invalide
                /// @return uint32 représentant l'étendue verticale en pixels
                NKENTSEU_EVENT_API_INLINE uint32 GetDirtyH() const noexcept {
                    return mDirtyH;
                }

            private:
                int32 mDirtyX;   ///< Coordonnée X du coin supérieur gauche de la zone invalide
                int32 mDirtyY;   ///< Coordonnée Y du coin supérieur gauche de la zone invalide
                uint32 mDirtyW;  ///< Largeur de la zone invalide [pixels]
                uint32 mDirtyH;  ///< Hauteur de la zone invalide [pixels]
                bool mFull;      ///< true = repaint complet requis, false = zone partielle
        };

        // =====================================================================
        // NkWindowResizeEvent — Redimensionnement de fenêtre avec historique
        // =====================================================================

        /**
         * @brief Émis lorsqu'une fenêtre change de dimensions (largeur et/ou hauteur)
         *
         * Cet événement transporte à la fois les nouvelles et anciennes dimensions,
         * permettant aux handlers de :
         *   - Détecter le sens du changement (agrandissement vs réduction)
         *   - Adapter le layout et le rendu en conséquence
         *   - Optimiser les recalculs en fonction de l'ampleur du changement
         *
         * @note Les dimensions sont exprimées en pixels logiques (tenant compte
         *       du DPI scaling), pas en pixels physiques de l'écran.
         *
         * @note Pour un redimensionnement en cours (live resize), les événements
         *       NkWindowResizeBeginEvent et NkWindowResizeEndEvent encadrent
         *       la série de NkWindowResizeEvent émis pendant l'opération.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowResizeEvent final : public NkWindowEvent {
            public:
                /// @brief Associe le type d'événement NK_WINDOW_RESIZE à cette classe
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_RESIZE)

                /// @brief États possibles du redimensionnement pour analyse sémantique
                enum ResizeState : uint32 {
                    NK_NOT_DEFINE = 0,  ///< État non défini ou inconnu
                    NK_EXPANDED,        ///< Fenêtre agrandie dans au moins une dimension
                    NK_REDUCED,         ///< Fenêtre rétrécie dans au moins une dimension
                    NK_NOT_CHANGE       ///< Dimensions inchangées (événement de confirmation)
                };

                /**
                 * @brief Constructeur avec dimensions avant/après et état de changement
                 * @param w Nouvelle largeur de la fenêtre en pixels
                 * @param h Nouvelle hauteur de la fenêtre en pixels
                 * @param prevW Ancienne largeur (0 si inconnue ou première création)
                 * @param prevH Ancienne hauteur (0 si inconnue ou première création)
                 * @param windowId Identifiant de la fenêtre concernée
                 * @param resizeState État sémantique du redimensionnement (optionnel)
                 */
                NkWindowResizeEvent(
                    uint32 w = 0,
                    uint32 h = 0,
                    uint32 prevW = 0,
                    uint32 prevH = 0,
                    uint64 windowId = 0,
                    ResizeState resizeState = NK_NOT_DEFINE
                ) noexcept
                    : NkWindowEvent(windowId)
                    , mWidth(w)
                    , mHeight(h)
                    , mPrevWidth(prevW)
                    , mPrevHeight(prevH)
                    , mResizeState(resizeState) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkWindowResizeEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant les dimensions avant et après le changement
                NkString ToString() const override {
                    return NkString::Fmt(
                        "WindowResize({0}x{1} prev={2}x{3})",
                        mWidth, mHeight, mPrevWidth, mPrevHeight
                    );
                }

                /// @brief Retourne la nouvelle largeur de la fenêtre
                /// @return uint32 représentant la dimension horizontale actuelle [pixels]
                NKENTSEU_EVENT_API_INLINE uint32 GetWidth() const noexcept {
                    return mWidth;
                }

                /// @brief Retourne la nouvelle hauteur de la fenêtre
                /// @return uint32 représentant la dimension verticale actuelle [pixels]
                NKENTSEU_EVENT_API_INLINE uint32 GetHeight() const noexcept {
                    return mHeight;
                }

                /// @brief Retourne l'ancienne largeur de la fenêtre avant changement
                /// @return uint32 représentant la dimension horizontale précédente [pixels]
                NKENTSEU_EVENT_API_INLINE uint32 GetPrevWidth() const noexcept {
                    return mPrevWidth;
                }

                /// @brief Retourne l'ancienne hauteur de la fenêtre avant changement
                /// @return uint32 représentant la dimension verticale précédente [pixels]
                NKENTSEU_EVENT_API_INLINE uint32 GetPrevHeight() const noexcept {
                    return mPrevHeight;
                }

                /// @brief Indique si la fenêtre a rétréci dans au moins une dimension
                /// @return true si largeur OU hauteur actuelle < précédente
                NKENTSEU_EVENT_API_INLINE bool GotSmaller() const noexcept {
                    return (mWidth < mPrevWidth) || (mHeight < mPrevHeight);
                }

                /// @brief Indique si la fenêtre a grandi dans au moins une dimension
                /// @return true si largeur OU hauteur actuelle > précédente
                NKENTSEU_EVENT_API_INLINE bool GotLarger() const noexcept {
                    return (mWidth > mPrevWidth) || (mHeight > mPrevHeight);
                }

                /// @brief Retourne l'état sémantique du redimensionnement
                /// @return Valeur ResizeState décrivant la nature du changement
                NKENTSEU_EVENT_API_INLINE ResizeState GetResizeState() const noexcept {
                    return mResizeState;
                }

            private:
                uint32 mWidth;        ///< Nouvelle largeur de la fenêtre [pixels]
                uint32 mHeight;       ///< Nouvelle hauteur de la fenêtre [pixels]
                uint32 mPrevWidth;    ///< Ancienne largeur avant le changement [pixels]
                uint32 mPrevHeight;   ///< Ancienne hauteur avant le changement [pixels]
                ResizeState mResizeState;  ///< État sémantique du redimensionnement
        };

        // =====================================================================
        // NkWindowResizeBeginEvent / NkWindowResizeEndEvent
        // Début et fin d'une opération de redimensionnement interactive
        // =====================================================================

        /**
         * @brief Émis au début d'une opération de redimensionnement interactive
         *
         * Cet événement marque le début d'un redimensionnement initié par l'utilisateur
         * (drag des bords de la fenêtre). Il permet de :
         *   - Suspendre les recalculs coûteux pendant le drag
         *   - Afficher un feedback visuel (overlay, curseur de redimensionnement)
         *   - Préparer les ressources pour les mises à jour fréquentes à venir
         *
         * @note Une série de NkWindowResizeEvent sera émise pendant l'opération,
         *       encadrée par NkWindowResizeBeginEvent (début) et NkWindowResizeEndEvent (fin).
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowResizeBeginEvent final : public NkWindowEvent {
            public:
                /// @brief Associe le type d'événement NK_WINDOW_RESIZE_BEGIN à cette classe
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_RESIZE_BEGIN)

                /**
                 * @brief Constructeur avec identifiant de fenêtre
                 * @param windowId Identifiant de la fenêtre en cours de redimensionnement
                 */
                NkWindowResizeBeginEvent(uint64 windowId = 0) noexcept
                    : NkWindowEvent(windowId) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkWindowResizeBeginEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString constant "WindowResizeBegin()" pour identification
                NkString ToString() const override {
                    return "WindowResizeBegin()";
                }
        };

        /**
         * @brief Émis à la fin d'une opération de redimensionnement interactive
         *
         * Cet événement marque la fin du redimensionnement et permet de :
         *   - Finaliser les ajustements de layout et de rendu
         *   - Reprendre les recalculs coûteux précédemment suspendus
         *   - Sauvegarder les nouvelles dimensions pour restauration future
         *
         * @note La dernière valeur de NkWindowResizeEvent avant cet événement
         *       représente les dimensions finales de la fenêtre.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowResizeEndEvent final : public NkWindowEvent {
            public:
                /// @brief Associe le type d'événement NK_WINDOW_RESIZE_END à cette classe
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_RESIZE_END)

                /**
                 * @brief Constructeur avec identifiant de fenêtre
                 * @param windowId Identifiant de la fenêtre ayant terminé son redimensionnement
                 */
                NkWindowResizeEndEvent(uint64 windowId = 0) noexcept
                    : NkWindowEvent(windowId) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkWindowResizeEndEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString constant "WindowResizeEnd()" pour identification
                NkString ToString() const override {
                    return "WindowResizeEnd()";
                }
        };

        // =====================================================================
        // NkWindowMoveEvent — Déplacement de fenêtre avec historique
        // =====================================================================

        /**
         * @brief Émis lorsqu'une fenêtre change de position sur l'écran
         *
         * Cet événement transporte à la fois les nouvelles et anciennes positions,
         * permettant aux handlers de :
         *   - Calculer le delta de déplacement pour les animations de suivi
         *   - Adapter les menus contextuels et tooltips à la nouvelle position
         *   - Gérer les transitions entre écrans dans un setup multi-moniteurs
         *
         * @note Les coordonnées sont exprimées en pixels logiques dans l'espace
         *       virtuel multi-écrans, avec (0,0) correspondant au coin supérieur
         *       gauche de l'écran primaire.
         *
         * @note Pour un déplacement en cours (live move), les événements
         *       NkWindowMoveBeginEvent et NkWindowMoveEndEvent encadrent
         *       la série de NkWindowMoveEvent émis pendant l'opération.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowMoveEvent final : public NkWindowEvent {
            public:
                /// @brief Associe le type d'événement NK_WINDOW_MOVE à cette classe
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_MOVE)

                /**
                 * @brief Constructeur avec positions avant/après
                 * @param x Nouvelle coordonnée X du coin supérieur gauche [pixels logiques]
                 * @param y Nouvelle coordonnée Y du coin supérieur gauche [pixels logiques]
                 * @param prevX Ancienne coordonnée X (0 si inconnue)
                 * @param prevY Ancienne coordonnée Y (0 si inconnue)
                 * @param windowId Identifiant de la fenêtre concernée
                 */
                NkWindowMoveEvent(
                    int32 x = 0,
                    int32 y = 0,
                    int32 prevX = 0,
                    int32 prevY = 0,
                    uint64 windowId = 0
                ) noexcept
                    : NkWindowEvent(windowId)
                    , mX(x)
                    , mY(y)
                    , mPrevX(prevX)
                    , mPrevY(prevY) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkWindowMoveEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant les positions avant et après le déplacement
                NkString ToString() const override {
                    return NkString::Fmt(
                        "WindowMove({0},{1} prev={2},{3})",
                        mX, mY, mPrevX, mPrevY
                    );
                }

                /// @brief Retourne la nouvelle coordonnée X de la fenêtre
                /// @return int32 représentant la position horizontale actuelle [pixels]
                NKENTSEU_EVENT_API_INLINE int32 GetX() const noexcept {
                    return mX;
                }

                /// @brief Retourne la nouvelle coordonnée Y de la fenêtre
                /// @return int32 représentant la position verticale actuelle [pixels]
                NKENTSEU_EVENT_API_INLINE int32 GetY() const noexcept {
                    return mY;
                }

                /// @brief Retourne l'ancienne coordonnée X avant déplacement
                /// @return int32 représentant la position horizontale précédente [pixels]
                NKENTSEU_EVENT_API_INLINE int32 GetPrevX() const noexcept {
                    return mPrevX;
                }

                /// @brief Retourne l'ancienne coordonnée Y avant déplacement
                /// @return int32 représentant la position verticale précédente [pixels]
                NKENTSEU_EVENT_API_INLINE int32 GetPrevY() const noexcept {
                    return mPrevY;
                }

                /// @brief Calcule le delta horizontal depuis la dernière position
                /// @return int32 représentant le déplacement horizontal (positif = droite)
                NKENTSEU_EVENT_API_INLINE int32 GetDeltaX() const noexcept {
                    return mX - mPrevX;
                }

                /// @brief Calcule le delta vertical depuis la dernière position
                /// @return int32 représentant le déplacement vertical (positif = bas)
                NKENTSEU_EVENT_API_INLINE int32 GetDeltaY() const noexcept {
                    return mY - mPrevY;
                }

            private:
                int32 mX;      ///< Nouvelle coordonnée X du coin supérieur gauche [pixels]
                int32 mY;      ///< Nouvelle coordonnée Y du coin supérieur gauche [pixels]
                int32 mPrevX;  ///< Ancienne coordonnée X avant déplacement [pixels]
                int32 mPrevY;  ///< Ancienne coordonnée Y avant déplacement [pixels]
        };

        // =====================================================================
        // NkWindowMoveBeginEvent / NkWindowMoveEndEvent
        // Début et fin d'une opération de déplacement interactive
        // =====================================================================

        /// @brief Émis au début d'un déplacement interactif de fenêtre (drag de la barre de titre)
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowMoveBeginEvent final : public NkWindowEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_MOVE_BEGIN)

                NkWindowMoveBeginEvent(uint64 windowId = 0) noexcept
                    : NkWindowEvent(windowId) {
                }

                NkEvent* Clone() const override {
                    return new NkWindowMoveBeginEvent(*this);
                }

                NkString ToString() const override {
                    return "WindowMoveBegin()";
                }
        };

        /// @brief Émis à la fin d'un déplacement interactif de fenêtre
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowMoveEndEvent final : public NkWindowEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_MOVE_END)

                NkWindowMoveEndEvent(uint64 windowId = 0) noexcept
                    : NkWindowEvent(windowId) {
                }

                NkEvent* Clone() const override {
                    return new NkWindowMoveEndEvent(*this);
                }

                NkString ToString() const override {
                    return "WindowMoveEnd()";
                }
        };

        // =====================================================================
        // NkWindowFocusGainedEvent / NkWindowFocusLostEvent
        // Changements d'état de focus clavier de la fenêtre
        // =====================================================================

        /**
         * @brief Émis lorsqu'une fenêtre obtient le focus clavier
         *
         * Cet événement indique que la fenêtre est maintenant la cible des entrées clavier :
         *   - Les raccourcis clavier globaux sont maintenant routés vers cette fenêtre
         *   - Le curseur texte peut apparaître dans les champs de saisie actifs
         *   - Les indicateurs visuels de focus (bordure, highlight) doivent être affichés
         *
         * @note Sur les systèmes multi-fenêtres, une seule fenêtre peut avoir le focus clavier
         *       à un instant donné. L'obtention du focus implique automatiquement la perte
         *       de focus de la fenêtre précédemment active.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowFocusGainedEvent final : public NkWindowEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_FOCUS_GAINED)

                NkWindowFocusGainedEvent(uint64 windowId = 0) noexcept
                    : NkWindowEvent(windowId) {
                }

                NkEvent* Clone() const override {
                    return new NkWindowFocusGainedEvent(*this);
                }

                NkString ToString() const override {
                    return "WindowFocusGained()";
                }
        };

        /**
         * @brief Émis lorsqu'une fenêtre perd le focus clavier
         *
         * Cet événement indique que la fenêtre n'est plus la cible des entrées clavier :
         *   - Les raccourcis clavier ne sont plus routés vers cette fenêtre
         *   - Le curseur texte doit être masqué dans les champs de saisie
         *   - Les indicateurs visuels de focus doivent être retirés
         *
         * @note La perte de focus peut survenir quand :
         *       - L'utilisateur clique sur une autre fenêtre
         *       - Une autre application prend le focus au premier plan
         *       - Le système affiche un menu global ou une notification
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowFocusLostEvent final : public NkWindowEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_FOCUS_LOST)

                NkWindowFocusLostEvent(uint64 windowId = 0) noexcept
                    : NkWindowEvent(windowId) {
                }

                NkEvent* Clone() const override {
                    return new NkWindowFocusLostEvent(*this);
                }

                NkString ToString() const override {
                    return "WindowFocusLost()";
                }
        };

        // =====================================================================
        // NkWindowMinimizeEvent / NkWindowMaximizeEvent / NkWindowRestoreEvent
        // Changements d'état de taille et de visibilité de la fenêtre
        // =====================================================================

        /// @brief Émis lorsqu'une fenêtre est minimisée (réduite dans la taskbar/dock)
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowMinimizeEvent final : public NkWindowEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_MINIMIZE)

                NkWindowMinimizeEvent(uint64 windowId = 0) noexcept
                    : NkWindowEvent(windowId) {
                }

                NkEvent* Clone() const override {
                    return new NkWindowMinimizeEvent(*this);
                }

                NkString ToString() const override {
                    return "WindowMinimize()";
                }
        };

        /// @brief Émis lorsqu'une fenêtre est maximisée (plein écran de travail)
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowMaximizeEvent final : public NkWindowEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_MAXIMIZE)

                NkWindowMaximizeEvent(uint64 windowId = 0) noexcept
                    : NkWindowEvent(windowId) {
                }

                NkEvent* Clone() const override {
                    return new NkWindowMaximizeEvent(*this);
                }

                NkString ToString() const override {
                    return "WindowMaximize()";
                }
        };

        /// @brief Émis lorsqu'une fenêtre est restaurée à sa taille normale (ni minimisée ni maximisée)
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowRestoreEvent final : public NkWindowEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_RESTORE)

                NkWindowRestoreEvent(uint64 windowId = 0) noexcept
                    : NkWindowEvent(windowId) {
                }

                NkEvent* Clone() const override {
                    return new NkWindowRestoreEvent(*this);
                }

                NkString ToString() const override {
                    return "WindowRestore()";
                }
        };

        // =====================================================================
        // NkWindowFullscreenEvent / NkWindowWindowedEvent
        // Basculement entre mode plein écran et mode fenêtré
        // =====================================================================

        /**
         * @brief Émis lorsqu'une fenêtre passe en mode plein écran exclusif
         *
         * Cet événement indique que :
         *   - La fenêtre occupe maintenant tout l'écran (masque la taskbar/dock)
         *   - Le mode d'affichage de l'écran peut avoir changé (résolution, refresh rate)
         *   - Les entrées clavier/souris sont maintenant capturées exclusivement
         *
         * @note Le mode plein écran peut être :
         *       - Exclusif : contrôle total de l'affichage, meilleures performances
         *       - Borderless windowed : fenêtre sans bordure couvrant l'écran, plus flexible
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowFullscreenEvent final : public NkWindowEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_FULLSCREEN)

                NkWindowFullscreenEvent(uint64 windowId = 0) noexcept
                    : NkWindowEvent(windowId) {
                }

                NkEvent* Clone() const override {
                    return new NkWindowFullscreenEvent(*this);
                }

                NkString ToString() const override {
                    return "WindowFullscreen()";
                }
        };

        /**
         * @brief Émis lorsqu'une fenêtre retourne en mode fenêtré standard
         *
         * Cet événement indique que :
         *   - La fenêtre a retrouvé ses bordures et barres de titre
         *   - La taskbar/dock est de nouveau visible
         *   - Le mode d'affichage de l'écran est revenu à sa configuration normale
         *
         * @note La restauration en mode fenêtré conserve généralement les dernières
         *       dimensions et position de la fenêtre avant le passage en plein écran.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowWindowedEvent final : public NkWindowEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_WINDOWED)

                NkWindowWindowedEvent(uint64 windowId = 0) noexcept
                    : NkWindowEvent(windowId) {
                }

                NkEvent* Clone() const override {
                    return new NkWindowWindowedEvent(*this);
                }

                NkString ToString() const override {
                    return "WindowWindowed()";
                }
        };

        // =====================================================================
        // NkWindowDpiEvent — Changement de facteur d'échelle DPI
        // =====================================================================

        /**
         * @brief Émis lorsqu'un changement de DPI ou de facteur d'échelle est détecté
         *
         * Cet événement est critique pour le support HiDPI et multi-écrans :
         *   - Adaptation des assets graphiques (icônes, polices) au nouveau scaling
         *   - Recalcul des layouts pour maintenir les proportions visuelles
         *   - Ajustement des zones de hit-testing pour la précision des interactions
         *
         * @note Le facteur d'échelle (scale) est un multiplicateur :
         *       - 1.0 = 100% (96 DPI de référence sur Windows)
         *       - 1.5 = 150% (144 DPI)
         *       - 2.0 = 200% (192 DPI, Retina displays)
         *
         * @note Les dimensions en pixels logiques restent constantes ; ce sont les
         *       pixels physiques qui changent. Ex: une fenêtre 800x600 logiques
         *       occupera 1600x1200 pixels physiques à scale=2.0.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowDpiEvent final : public NkWindowEvent {
            public:
                /// @brief Associe le type d'événement NK_WINDOW_DPI_CHANGE à cette classe
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_DPI_CHANGE)

                /**
                 * @brief Constructeur avec facteurs d'échelle avant/après et valeur DPI
                 * @param scale Nouveau facteur d'échelle (ex: 1.5 pour 150%)
                 * @param prevScale Ancien facteur d'échelle avant le changement
                 * @param dpi Valeur DPI physique correspondante (ex: 144 pour scale=1.5)
                 * @param windowId Identifiant de la fenêtre concernée
                 */
                NkWindowDpiEvent(
                    float32 scale = 1.0f,
                    float32 prevScale = 1.0f,
                    uint32 dpi = 96,
                    uint64 windowId = 0
                ) noexcept
                    : NkWindowEvent(windowId)
                    , mScale(scale)
                    , mPrevScale(prevScale)
                    , mDpi(dpi) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkWindowDpiEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le nouveau facteur d'échelle et la valeur DPI
                NkString ToString() const override {
                    return NkString::Fmt(
                        "WindowDpi(scale={0:.3} dpi={1})",
                        mScale, mDpi
                    );
                }

                /// @brief Retourne le nouveau facteur d'échelle appliqué
                /// @return float32 représentant le multiplicateur de scaling (1.0 = 100%)
                NKENTSEU_EVENT_API_INLINE float32 GetScale() const noexcept {
                    return mScale;
                }

                /// @brief Retourne l'ancien facteur d'échelle avant le changement
                /// @return float32 représentant le multiplicateur précédent
                NKENTSEU_EVENT_API_INLINE float32 GetPrevScale() const noexcept {
                    return mPrevScale;
                }

                /// @brief Retourne la valeur DPI physique correspondante
                /// @return uint32 représentant les dots per inch (96 = référence Windows)
                NKENTSEU_EVENT_API_INLINE uint32 GetDpi() const noexcept {
                    return mDpi;
                }

            private:
                float32 mScale;      ///< Nouveau facteur d'échelle (1.0 = 100%)
                float32 mPrevScale;  ///< Ancien facteur d'échelle avant changement
                uint32 mDpi;         ///< Valeur DPI physique correspondante
        };

        // =====================================================================
        // NkWindowThemeEvent — Changement de thème d'interface système
        // =====================================================================

        /**
         * @brief Émis lorsqu'un changement de thème système est détecté
         *
         * Cet événement permet à l'application de s'adapter dynamiquement aux
         * préférences de l'utilisateur pour une expérience cohérente :
         *   - Basculer entre assets clairs/sombres pour les icônes et images
         *   - Adapter les couleurs de l'interface aux nouvelles valeurs système
         *   - Mettre à jour les styles CSS ou les ressources de thème UI
         *
         * @note Pour les thèmes au niveau d'une fenêtre individuelle (indépendant
         *       du thème système), consulter l'API spécifique du window manager.
         *
         * @code
         *   void OnThemeChange(NkWindowThemeEvent& event) {
         *       if (event.IsDark()) {
         *           Style::ApplyDarkTheme();
         *           Icons::LoadDarkVariant();
         *       }
         *       else if (event.IsLight()) {
         *           Style::ApplyLightTheme();
         *           Icons::LoadLightVariant();
         *       }
         *   }
         * @endcode
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowThemeEvent final : public NkWindowEvent {
            public:
                /// @brief Associe le type d'événement NK_WINDOW_THEME_CHANGE à cette classe
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_THEME_CHANGE)

                /**
                 * @brief Constructeur avec nouveau thème et identifiant de fenêtre
                 * @param theme Nouveau thème système actif
                 * @param windowId Identifiant de la fenêtre concernée
                 */
                NkWindowThemeEvent(
                    NkWindowTheme theme = NkWindowTheme::NK_THEME_UNKNOWN,
                    uint64 windowId = 0
                ) noexcept
                    : NkWindowEvent(windowId)
                    , mTheme(theme) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur the heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkWindowThemeEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le thème actif via NkWindowThemeToString()
                NkString ToString() const override {
                    return NkString("WindowTheme(")
                         + NkWindowThemeToString(mTheme) + ")";
                }

                /// @brief Retourne le thème système actuellement actif
                /// @return Valeur NkWindowTheme décrivant le mode d'interface
                NKENTSEU_EVENT_API_INLINE NkWindowTheme GetTheme() const noexcept {
                    return mTheme;
                }

                /// @brief Indique si le thème sombre est actuellement actif
                /// @return true si GetTheme() == NK_THEME_DARK
                NKENTSEU_EVENT_API_INLINE bool IsDark() const noexcept {
                    return mTheme == NkWindowTheme::NK_THEME_DARK;
                }

                /// @brief Indique si le thème clair est actuellement actif
                /// @return true si GetTheme() == NK_THEME_LIGHT
                NKENTSEU_EVENT_API_INLINE bool IsLight() const noexcept {
                    return mTheme == NkWindowTheme::NK_THEME_LIGHT;
                }

            private:
                NkWindowTheme mTheme;  ///< Thème système actuellement actif
        };

        // =====================================================================
        // NkWindowShownEvent / NkWindowHiddenEvent
        // Changements de visibilité de la fenêtre
        // =====================================================================

        /**
         * @brief Émis lorsqu'une fenêtre devient visible à l'écran
         *
         * Cet événement indique que la fenêtre est maintenant affichée et peut
         * recevoir des événements de peinture et d'interaction utilisateur.
         *
         * @note Une fenêtre peut être "créée" mais pas encore "shown" — cet événement
         *       marque le moment où elle apparaît effectivement à l'utilisateur.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowShownEvent final : public NkWindowEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_SHOWN)

                NkWindowShownEvent(uint64 windowId = 0) noexcept
                    : NkWindowEvent(windowId) {
                }

                NkEvent* Clone() const override {
                    return new NkWindowShownEvent(*this);
                }

                NkString ToString() const override {
                    return "WindowShown()";
                }
        };

        /**
         * @brief Émis lorsqu'une fenêtre est masquée (toujours en mémoire mais non affichée)
         *
         * Cet événement indique que la fenêtre n'est plus visible à l'écran mais
         * reste active en arrière-plan :
         *   - Les timers et callbacks continuent de s'exécuter
         *   - Les réseaux et threads restent opérationnels
         *   - La fenêtre peut être re-shown sans recréation
         *
         * @note Différent de NkWindowMinimizeEvent : une fenêtre masquée n'apparaît
         *       pas dans la taskbar/dock, tandis qu'une fenêtre minimisée y reste visible.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkWindowHiddenEvent final : public NkWindowEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_WINDOW_HIDDEN)

                NkWindowHiddenEvent(uint64 windowId = 0) noexcept
                    : NkWindowEvent(windowId) {
                }

                NkEvent* Clone() const override {
                    return new NkWindowHiddenEvent(*this);
                }

                NkString ToString() const override {
                    return "WindowHidden()";
                }
        };

    } // namespace nkentseu

#endif // NKENTSEU_EVENT_NKWINDOWEVENT_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKWINDOWEVENT.H
// =============================================================================
// Ce fichier définit la hiérarchie des événements fenêtre pour le module NKEvent.
// Les exemples ci-dessous illustrent les patterns d'usage recommandés.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Gestion adaptative du redimensionnement de fenêtre
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkWindowEvent.h"

    class ResponsiveRenderer {
    public:
        void OnWindowResized(const nkentseu::NkWindowResizeEvent& event) {
            using namespace nkentseu;

            uint32 newWidth = event.GetWidth();
            uint32 newHeight = event.GetHeight();

            // Adaptation du viewport de rendu
            Renderer::SetViewport(0, 0, newWidth, newHeight);

            // Recalcul du layout UI si le changement est significatif
            if (event.GotLarger() || event.GotSmaller()) {
                UIManager::RecomputeLayout(newWidth, newHeight);
            }

            // Optimisation : suspendre les effets pendant le live resize
            if (mIsResizing) {
                Renderer::SetQuality(RenderQuality::Low);
            }
        }

        void OnResizeBegin(const nkentseu::NkWindowResizeBeginEvent&) {
            mIsResizing = true;
            // Suspendre les animations et effets coûteux
            AnimationSystem::PauseExpensiveEffects();
        }

        void OnResizeEnd(const nkentseu::NkWindowResizeEndEvent&) {
            mIsResizing = false;
            // Restaurer la qualité de rendu et reprendre les animations
            Renderer::SetQuality(RenderQuality::High);
            AnimationSystem::ResumeAll();
        }

    private:
        bool mIsResizing = false;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Gestion du thème sombre/clair avec transition fluide
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkWindowEvent.h"

    class ThemeManager {
    public:
        void OnThemeChanged(const nkentseu::NkWindowThemeEvent& event) {
            using namespace nkentseu;

            // Transition animée entre thèmes pour éviter le flash brutal
            if (event.IsDark()) {
                Style::TransitionToDarkTheme(0.3f);  // 300ms fade
            }
            else if (event.IsLight()) {
                Style::TransitionToLightTheme(0.3f);
            }

            // Mise à jour des assets dépendants du thème
            IconSet::ReloadForTheme(event.GetTheme());
            CursorSet::UpdateColors(event.GetTheme());

            // Persistance de la préférence pour les sessions futures
            Config::SetPreferredTheme(event.GetTheme());
        }

        // Query rapide de l'état actuel du thème
        bool IsDarkModeActive() const {
            return mCurrentTheme == nkentseu::NkWindowTheme::NK_THEME_DARK;
        }

    private:
        nkentseu::NkWindowTheme mCurrentTheme = nkentseu::NkWindowTheme::NK_THEME_LIGHT;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Gestion du focus pour l'accessibilité et l'UX
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkWindowEvent.h"

    class FocusManager {
    public:
        void OnFocusGained(const nkentseu::NkWindowFocusGainedEvent& event) {
            using namespace nkentseu;

            // Mise en avant visuelle de la fenêtre active
            WindowDecorations::HighlightBorder(event.GetWindowId(), true);

            // Reprise des animations et sons précédemment suspendus
            AudioManager::ResumeBackgroundMusic();
            AnimationSystem::ResumePausedAnimations();

            // Mise à jour de l'état d'accessibilité (screen readers)
            Accessibility::AnnounceWindowFocus("Application window focused");
        }

        void OnFocusLost(const nkentseu::NkWindowFocusLostEvent& event) {
            using namespace nkentseu;

            // Retrait de l'highlight visuel
            WindowDecorations::HighlightBorder(event.GetWindowId(), false);

            // Suspension des éléments non essentiels pour économiser les ressources
            AudioManager::PauseBackgroundMusic();
            ParticleSystem::ReduceEmitterRate(0.5f);

            // Indication d'accessibilité
            Accessibility::AnnounceWindowFocus("Application window unfocused");
        }
    };
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Support HiDPI avec adaptation dynamique des assets
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkWindowEvent.h"

    class HiDpiAdapter {
    public:
        void OnDpiChanged(const nkentseu::NkWindowDpiEvent& event) {
            using namespace nkentseu;

            float32 newScale = event.GetScale();
            float32 oldScale = event.GetPrevScale();

            // Recalcul des dimensions logiques → physiques
            uint32 logicalWidth = GetCurrentLogicalWidth();
            uint32 physicalWidth = static_cast<uint32>(logicalWidth * newScale);
            Renderer::SetPhysicalResolution(physicalWidth, ...);

            // Rechargement des assets à la résolution appropriée
            if (newScale >= 2.0f) {
                AssetLoader::UseVariant("2x");  // Assets Retina/HiDPI
            }
            else if (newScale >= 1.5f) {
                AssetLoader::UseVariant("1.5x");
            }
            else {
                AssetLoader::UseVariant("1x");  // Assets standard
            }

            // Mise à jour des polices pour maintenir la lisibilité
            FontManager::AdjustSizeForDpi(newScale);

            NK_FOUNDATION_LOG_INFO("DPI updated: {:.1f}x -> {:.1f}x ({} DPI)",
                oldScale, newScale, event.GetDpi());
        }
    };
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Gestion élégante de la fermeture avec sauvegarde
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkWindowEvent.h"

    class DocumentManager {
    public:
        void OnWindowCloseRequest(nkentseu::NkWindowCloseEvent& event) {
            using namespace nkentseu;

            // Ignorer les fermetures forcées (shutdown OS) — sauvegarde d'urgence seulement
            if (event.IsForced()) {
                NK_FOUNDATION_LOG_WARNING("Forced window close: emergency save only");
                Document::AutoSaveQuick();
                return;  // Ne pas annuler une fermeture forcée
            }

            // Vérifier s'il y a des modifications non sauvegardées
            if (Document::IsModified()) {
                // Annuler la fermeture pour afficher une confirmation
                event.MarkHandled();

                // Afficher un dialog modal de confirmation
                UIManager::ShowModalDialog(
                    "Save Changes?",
                    "The document has unsaved changes. Save before closing?",
                    {
                        {"Save", [this]() {
                            Document::Save();
                            Application::CloseWindow(mWindowId);
                        }},
                        {"Don't Save", [this]() {
                            Application::CloseWindow(mWindowId);
                        }},
                        {"Cancel", []() {
                            // Rien : l'utilisateur reste dans l'application
                        }}
                    }
                );
            }
            // Si pas de modifications : laisser la fermeture se poursuivre normalement
        }

    private:
        nkentseu::NkWindowId mWindowId = nkentseu::NK_INVALID_WINDOW_ID;
    };
*/

// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
    1. ORDRE D'ÉMISSION DES ÉVÉNEMENTS FENÊTRE :
       - Création    : Create → (optionnel Show) → FocusGained
       - Redimension : ResizeBegin → [Resize × N] → ResizeEnd
       - Déplacement : MoveBegin → [Move × N] → MoveEnd
       - Fermeture   : Close (requête, annulable) → Destroy (définitif)
       - Visibilité  : Hidden ↔ Shown (indépendant de Minimize/Maximize)

    2. COORDONNÉES ET UNITÉS :
       - Toutes les dimensions/positions sont en pixels LOGIQUES (DPI-aware)
       - Pour convertir en pixels physiques : physical = logical * dpiScale
       - Les zones de repaint (dirty rect) utilisent le même système de coordonnées

    3. GESTION DES ÉVÉNEMENTS EN SÉRIE (live resize/move) :
       - Pendant un drag interactif, de nombreux événements peuvent être émis
       - Optimiser les handlers : éviter les allocations, recalculs lourds
       - Utiliser les événements Begin/End pour suspendre/reprendre les tâches coûteuses

    4. ANNULATION DE FERMETURE (NkWindowCloseEvent) :
       - event.MarkHandled() annule la fermeture gracieuse
       - IsForced() indique une fermeture système non annulable
       - Toujours vérifier IsForced() avant de proposer une confirmation à l'utilisateur

    5. PERFORMANCE DES ACCESSEURS :
       - Tous les getters sont inline et noexcept pour éliminer l'overhead d'appel
       - Les calculs simples (GetDeltaX(), GotLarger()) sont faits à la volée
       - Éviter les allocations dans ToString() pour les événements fréquents

    6. EXTENSIBILITÉ :
       - Pour ajouter un nouvel événement fenêtre :
         a) Dériver de NkWindowEvent (hérite automatiquement de NK_CAT_WINDOW)
         b) Utiliser NK_EVENT_TYPE_FLAGS avec un nouveau type dans NkEventType
         c) Implémenter Clone() et ToString() pour cohérence avec l'infrastructure
         d) Documenter les cas d'usage et l'ordre d'émission attendu
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================