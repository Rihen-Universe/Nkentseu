// =============================================================================
// NKEvent/NkTouchEvent.h
// Hiérarchie complète des événements tactiles et gestes multi-touch.
//
// Catégorie : NK_CAT_TOUCH
//
// Architecture :
//   - NkTouchPhase         : phase d'un contact individuel (began, moved, ended...)
//   - NkSwipeDirection     : direction d'un geste de balayage (swipe)
//   - NkTouchPoint         : état complet d'un contact tactile individuel
//   - NkTouchEvent         : classe de base abstraite pour événements tactiles
//   - Classes dérivées     : événements concrets pour touches et gestes
//
// Gestes supportés :
//   - Tap / Double-tap    : appui court simple ou multiple
//   - Long press          : appui maintenu au-delà d'un seuil temporel
//   - Swipe               : balayage rapide dans une direction
//   - Pinch               : pincement/écartement à deux doigts (zoom)
//   - Rotate              : rotation à deux doigts autour d'un centre
//   - Pan                 : panoramique/défilement à un ou plusieurs doigts
//
// Design :
//   - Support multi-touch jusqu'à NK_MAX_TOUCH_POINTS contacts simultanés
//   - Coordonnées multiples : client, écran, normalisées [0,1] pour portabilité
//   - Calcul automatique du centroid pour les gestes multi-doigts
//   - Détection de mouvement via delta depuis l'événement précédent
//
// Usage typique :
//   // Gestion d'un tap simple
//   if (auto* tap = event.As<NkGestureTapEvent>()) {
//       if (tap->GetTapCount() == 1) {
//           SelectItemAt(tap->GetX(), tap->GetY());
//       }
//       else if (tap->IsDoubleTap()) {
//           ZoomAt(tap->GetX(), tap->GetY(), 2.0f);
//       }
//   }
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_EVENT_NKTOUCHEVENT_H
#define NKENTSEU_EVENT_NKTOUCHEVENT_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des dépendances internes au projet.
    // Toutes les dépendances utilisent les modules NK* du framework.

    #include "NKEvent/NkEvent.h"                        // Classe de base NkEvent + macros
    #include "NKEvent/NkEventApi.h"                     // Macros d'export NKENTSEU_EVENT_API
    #include "NKContainers/String/NkStringUtils.h"      // Conversion types → NkString
    #include <algorithm>                                // Pour std::min/max si nécessaire

    namespace nkentseu {

        // =====================================================================
        // NkTouchPhase — Énumération des phases d'un contact tactile
        // =====================================================================

        /// @brief Représente la phase actuelle d'un contact tactile individuel
        /// @note Utilisé par NkTouchPoint pour décrire l'état de chaque doigt
        enum class NKENTSEU_EVENT_CLASS_EXPORT NkTouchPhase : uint32 {
            NK_TOUCH_PHASE_BEGAN       = 0,   ///< Contact initial : doigt posé sur l'écran
            NK_TOUCH_PHASE_MOVED,             ///< Contact en mouvement : doigt glisse sur l'écran
            NK_TOUCH_PHASE_STATIONARY,        ///< Contact stable : doigt posé mais immobile
            NK_TOUCH_PHASE_ENDED,             ///< Contact terminé : doigt retiré de l'écran
            NK_TOUCH_PHASE_CANCELLED,         ///< Contact annulé : système a interrompu le suivi
            NK_TOUCH_PHASE_MAX                ///< Sentinelle : nombre de valeurs valides (pour validation)
        };

        // =====================================================================
        // NkSwipeDirection — Énumération des directions de swipe
        // =====================================================================

        /// @brief Représente la direction détectée pour un geste de balayage (swipe)
        /// @note Utilisé par NkGestureSwipeEvent pour indiquer le sens du geste
        enum class NKENTSEU_EVENT_CLASS_EXPORT NkSwipeDirection : uint32 {
            NK_SWIPE_NONE  = 0,   ///< Aucune direction détectée (valeur par défaut)
            NK_SWIPE_LEFT,        ///< Balayage vers la gauche (→ à ←)
            NK_SWIPE_RIGHT,       ///< Balayage vers la droite (← à →)
            NK_SWIPE_UP,          ///< Balayage vers le haut (↓ à ↑)
            NK_SWIPE_DOWN         ///< Balayage vers le bas (↑ à ↓)
        };

        /// @brief Convertit une valeur NkSwipeDirection en chaîne lisible pour le débogage
        /// @param d La valeur de direction à convertir
        /// @return Pointeur vers une chaîne littérale statique (ne pas libérer)
        /// @note Fonction noexcept pour usage dans les contextes critiques
        inline const char* NkSwipeDirectionToString(NkSwipeDirection d) noexcept {
            switch (d) {
                case NkSwipeDirection::NK_SWIPE_LEFT:
                    return "LEFT";
                case NkSwipeDirection::NK_SWIPE_RIGHT:
                    return "RIGHT";
                case NkSwipeDirection::NK_SWIPE_UP:
                    return "UP";
                case NkSwipeDirection::NK_SWIPE_DOWN:
                    return "DOWN";
                case NkSwipeDirection::NK_SWIPE_NONE:
                default:
                    return "NONE";
            }
        }

        // =====================================================================
        // Constantes et limites pour le support multi-touch
        // =====================================================================

        /// @brief Nombre maximum de points de contact suivis simultanément
        /// @note Cette limite est un compromis entre flexibilité et performance mémoire
        /// @note La plupart des écrans tactiles grand public supportent 5-10 points ;
        ///       32 permet de couvrir les cas extrêmes (tablettes pro, surfaces interactives)
        static constexpr uint32 NK_MAX_TOUCH_POINTS = 32;

        // =====================================================================
        // NkTouchPoint — Structure descriptive d'un contact tactile individuel
        // =====================================================================

        /**
         * @brief Décrit l'état complet d'un contact tactile individuel
         *
         * Cette structure transporte toutes les informations disponibles
         * pour un doigt ou stylet en contact avec l'écran :
         *   - Identifiant unique pour suivre le même doigt entre événements
         *   - Coordonnées dans plusieurs espaces (client, écran, normalisé)
         *   - Mouvement relatif depuis l'événement précédent (delta)
         *   - Propriétés physiques : pression, rayon de contact, angle
         *
         * @note Les coordonnées "client" sont relatives à la zone de contenu
         *       de la fenêtre (exclu les bordures, barre de titre...).
         *
         * @note Les coordonnées "normalisées" sont dans [0.0, 1.0] indépendamment
         *       de la résolution, utiles pour les UI responsives.
         *
         * @note La phase (phase) indique l'état du contact : began, moved,
         *       stationary, ended, ou cancelled par le système.
         */
        struct NKENTSEU_EVENT_CLASS_EXPORT NkTouchPoint {
            uint64 id = 0;                          ///< Identifiant unique du contact (stable pendant la durée du touch)
            NkTouchPhase phase = NkTouchPhase::NK_TOUCH_PHASE_BEGAN;  ///< Phase actuelle du contact

            // Coordonnées dans l'espace client de la fenêtre (pixels physiques)
            float clientX = 0.f;                    ///< Position horizontale relative à la zone client
            float clientY = 0.f;                    ///< Position verticale relative à la zone client

            // Coordonnées dans l'espace écran global (multi-moniteurs)
            float screenX = 0.f;                    ///< Position horizontale absolue sur l'écran
            float screenY = 0.f;                    ///< Position verticale absolue sur l'écran

            // Coordonnées normalisées dans [0.0, 1.0] pour portabilité résolution
            float normalX = 0.f;                    ///< Position horizontale normalisée (0 = gauche, 1 = droite)
            float normalY = 0.f;                    ///< Position verticale normalisée (0 = haut, 1 = bas)

            // Mouvement relatif depuis l'événement tactile précédent
            float deltaX = 0.f;                     ///< Déplacement horizontal depuis le dernier événement
            float deltaY = 0.f;                     ///< Déplacement vertical depuis le dernier événement

            // Propriétés physiques du contact (si supportées par le hardware)
            float pressure = 1.f;                   ///< Pression appliquée [0.0 = léger, 1.0 = maximal]
            float radiusX = 0.f;                    ///< Rayon de contact horizontal [pixels] (forme elliptique)
            float radiusY = 0.f;                    ///< Rayon de contact vertical [pixels]
            float angle = 0.f;                      ///< Angle d'orientation du contact [degrés, 0 = vertical]

            /// @brief Indique si le contact a bougé depuis l'événement précédent
            /// @return true si deltaX ou deltaY est non nul
            NKENTSEU_EVENT_API_INLINE bool HasMoved() const {
                return (deltaX != 0.f) || (deltaY != 0.f);
            }

            /// @brief Indique si le contact est actuellement actif (posé sur l'écran)
            /// @return true pour les phases BEGAN, MOVED, ou STATIONARY
            /// @note false pour ENDED ou CANCELLED (contact terminé)
            NKENTSEU_EVENT_API_INLINE bool IsActive() const {
                return (phase == NkTouchPhase::NK_TOUCH_PHASE_BEGAN)
                    || (phase == NkTouchPhase::NK_TOUCH_PHASE_MOVED)
                    || (phase == NkTouchPhase::NK_TOUCH_PHASE_STATIONARY);
            }
        };

        // =====================================================================
        // NkTouchEvent — Classe de base abstraite pour événements tactiles
        // =====================================================================

        /**
         * @brief Classe de base polymorphe pour tous les événements tactiles
         *
         * Catégorie associée : NK_CAT_TOUCH
         *
         * Cette classe gère le support multi-touch en agrégeant jusqu'à
         * NK_MAX_TOUCH_POINTS contacts simultanés et en calculant des
         * métriques globales comme le centroid (centre de masse des doigts).
         *
         * @note Les classes dérivées (NkTouchBeginEvent, etc.) passent un tableau
         *       de NkTouchPoint au constructeur, qui est copié dans un buffer
         *       interne pour garantir la validité des données pendant le traitement.
         *
         * @note Le centroid est calculé automatiquement comme la moyenne des
         *       positions clientX/clientY de tous les contacts actifs. Utile
         *       pour les gestes multi-doigts comme le pinch ou le rotate.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkTouchEvent : public NkEvent {
            public:
                /// @brief Implémente le filtrage par catégorie TOUCH pour tous les dérivés
                NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_TOUCH)

                /// @brief Retourne le nombre de points de contact dans cet événement
                /// @return uint32 représentant le count de touches (0 à NK_MAX_TOUCH_POINTS)
                NKENTSEU_EVENT_API_INLINE uint32 GetNumTouches() const noexcept {
                    return mNumTouches;
                }

                /// @brief Retourne une référence constante vers un point de contact spécifique
                /// @param i Index du contact à accéder (0-based, doit être < GetNumTouches())
                /// @return const NkTouchPoint& référence vers le contact demandé
                /// @warning Ne pas accéder hors bounds : comportement indéfini
                NKENTSEU_EVENT_API_INLINE const NkTouchPoint& GetTouch(uint32 i) const noexcept {
                    return mTouches[i];
                }

                /// @brief Retourne la coordonnée X du centroid (centre de masse des doigts)
                /// @return float représentant la position horizontale moyenne [pixels client]
                /// @note Utile pour les gestes multi-doigts : pivot de rotation, centre de zoom...
                NKENTSEU_EVENT_API_INLINE float GetCentroidX() const noexcept {
                    return mCentroidX;
                }

                /// @brief Retourne la coordonnée Y du centroid (centre de masse des doigts)
                /// @return float représentant la position verticale moyenne [pixels client]
                NKENTSEU_EVENT_API_INLINE float GetCentroidY() const noexcept {
                    return mCentroidY;
                }

            protected:
                /// @brief Constructeur protégé — réservé aux classes dérivées
                /// @param points Tableau de NkTouchPoint à copier dans le buffer interne
                /// @param count Nombre de contacts dans le tableau (sera clampé à NK_MAX_TOUCH_POINTS)
                /// @param windowId Identifiant de la fenêtre source (0 = événement global)
                /// @note Copie les contacts et calcule automatiquement le centroid
                NKENTSEU_EVENT_API NkTouchEvent(
                    const NkTouchPoint* points,
                    uint32 count,
                    uint64 windowId = 0
                ) noexcept
                    : NkEvent(windowId)
                    , mNumTouches(0)
                    , mCentroidX(0.f)
                    , mCentroidY(0.f) {
                    // Clamp du nombre de contacts à la limite maximale
                    mNumTouches = (count > NK_MAX_TOUCH_POINTS) ? NK_MAX_TOUCH_POINTS : count;

                    // Accumulateurs pour calcul du centroid
                    float sumX = 0.f;
                    float sumY = 0.f;

                    // Copie des contacts et accumulation des positions
                    for (uint32 i = 0; i < mNumTouches; ++i) {
                        mTouches[i] = points[i];
                        sumX += points[i].clientX;
                        sumY += points[i].clientY;
                    }

                    // Calcul du centroid si au moins un contact est présent
                    if (mNumTouches > 0) {
                        mCentroidX = sumX / static_cast<float>(mNumTouches);
                        mCentroidY = sumY / static_cast<float>(mNumTouches);
                    }
                }

                NkTouchPoint mTouches[NK_MAX_TOUCH_POINTS];  ///< Buffer interne des contacts (copie)
                uint32 mNumTouches;                           ///< Nombre effectif de contacts dans le buffer
                float mCentroidX;                             ///< Coordonnée X du centroid [pixels client]
                float mCentroidY;                             ///< Coordonnée Y du centroid [pixels client]
        };

        // =====================================================================
        // NkTouchBeginEvent — Nouveau(x) contact(s) tactile(s)
        // =====================================================================

        /**
         * @brief Émis lorsqu'un ou plusieurs nouveaux contacts tactiles sont détectés
         *
         * Cet événement marque le début d'une interaction tactile :
         *   - Un doigt ou stylet touche l'écran pour la première fois
         *   - Plusieurs doigts peuvent être posés simultanément (multi-touch)
         *   - La phase de chaque contact est NK_TOUCH_PHASE_BEGAN
         *
         * @note Les handlers peuvent utiliser cet événement pour :
         *       - Initialiser un geste (début de drag, start de pinch...)
         *       - Capturer le focus tactile pour exclusivité d'interaction
         *       - Afficher un feedback visuel (highlight, ripple effect...)
         *
         * @note GetNumTouches() peut être > 1 en cas de multi-touch simultané.
         *       Itérer sur GetTouch(i) pour traiter chaque contact individuellement.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkTouchBeginEvent final : public NkTouchEvent {
            public:
                /// @brief Associe le type d'événement NK_TOUCH_BEGIN à cette classe
                NK_EVENT_TYPE_FLAGS(NK_TOUCH_BEGIN)

                /**
                 * @brief Constructeur avec tableau de points de contact
                 * @param points Tableau de NkTouchPoint décrivant les nouveaux contacts
                 * @param count Nombre de contacts dans le tableau
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 * @note Les points sont copiés dans un buffer interne : pas de risque de dangling
                 */
                NKENTSEU_EVENT_API NkTouchBeginEvent(
                    const NkTouchPoint* points,
                    uint32 count,
                    uint64 windowId = 0
                ) noexcept
                    : NkTouchEvent(points, count, windowId) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkTouchBeginEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le nombre de contacts initiés
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt("TouchBegin({0} contacts)", mNumTouches);
                }
        };

        // =====================================================================
        // NkTouchMoveEvent — Contact(s) tactile(s) déplacé(s)
        // =====================================================================

        /**
         * @brief Émis lorsqu'un ou plusieurs contacts tactiles se déplacent sur l'écran
         *
         * Cet événement transporte les nouvelles positions et les deltas de mouvement :
         *   - clientX/clientY : positions absolues actuelles dans la zone client
         *   - deltaX/deltaY : déplacement relatif depuis l'événement précédent
         *   - phase : NK_TOUCH_PHASE_MOVED pour tous les contacts dans cet événement
         *
         * @note Les handlers peuvent utiliser cet événement pour :
         *       - Implémenter le drag-and-drop (suivi de position)
         *       - Détecter des gestes de dessin ou d'écriture manuscrite
         *       - Calculer la vitesse de déplacement pour inertie ou accélération
         *
         * @note La fréquence d'émission dépend du hardware et du driver :
         *       typiquement 60-120 Hz pour les écrans modernes. Éviter les
         *       calculs lourds dans les handlers pour maintenir la réactivité.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkTouchMoveEvent final : public NkTouchEvent {
            public:
                /// @brief Associe le type d'événement NK_TOUCH_MOVE à cette classe
                NK_EVENT_TYPE_FLAGS(NK_TOUCH_MOVE)

                /**
                 * @brief Constructeur avec tableau de points de contact mis à jour
                 * @param points Tableau de NkTouchPoint avec positions et deltas actualisés
                 * @param count Nombre de contacts dans le tableau
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 */
                NKENTSEU_EVENT_API NkTouchMoveEvent(
                    const NkTouchPoint* points,
                    uint32 count,
                    uint64 windowId = 0
                ) noexcept
                    : NkTouchEvent(points, count, windowId) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkTouchMoveEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le nombre de contacts en mouvement
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt("TouchMove({0} contacts)", mNumTouches);
                }
        };

        // =====================================================================
        // NkTouchEndEvent — Contact(s) tactile(s) levé(s)
        // =====================================================================

        /**
         * @brief Émis lorsqu'un ou plusieurs contacts tactiles sont retirés de l'écran
         *
         * Cet événement marque la fin d'une interaction tactile :
         *   - Un doigt ou stylet quitte la surface de l'écran
         *   - La phase de chaque contact est NK_TOUCH_PHASE_ENDED
         *   - Les positions rapportées sont les dernières avant le retrait
         *
         * @note Les handlers peuvent utiliser cet événement pour :
         *       - Finaliser un geste (drop en drag-and-drop, fin de dessin...)
         *       - Détecter un tap (TouchBegin suivi rapidement de TouchEnd sans Move)
         *       - Libérer le focus tactile si l'interaction est terminée
         *
         * @note Un TouchEnd peut être suivi d'un nouveau TouchBegin si l'utilisateur
         *       repose le doigt rapidement : traiter chaque événement indépendamment.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkTouchEndEvent final : public NkTouchEvent {
            public:
                /// @brief Associe le type d'événement NK_TOUCH_END à cette classe
                NK_EVENT_TYPE_FLAGS(NK_TOUCH_END)

                /**
                 * @brief Constructeur avec tableau de points de contact terminés
                 * @param points Tableau de NkTouchPoint avec positions finales
                 * @param count Nombre de contacts dans le tableau
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 */
                NKENTSEU_EVENT_API NkTouchEndEvent(
                    const NkTouchPoint* points,
                    uint32 count,
                    uint64 windowId = 0
                ) noexcept
                    : NkTouchEvent(points, count, windowId) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkTouchEndEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le nombre de contacts terminés
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt("TouchEnd({0} contacts)", mNumTouches);
                }
        };

        // =====================================================================
        // NkTouchCancelEvent — Contact(s) tactile(s) annulé(s) par le système
        // =====================================================================

        /**
         * @brief Émis lorsque le système d'exploitation annule le suivi d'un contact tactile
         *
         * Cet événement signale une interruption externe de l'interaction :
         *   - Une notification système ou un appel téléphonique a pris le focus
         *   - L'application est passée en arrière-plan (background)
         *   - Le hardware a perdu le suivi du doigt (perte de signal, interférence)
         *
         * @note Contrairement à TouchEnd (fin normale), TouchCancel indique
         *       que l'interaction n'a pas pu se terminer proprement. Les handlers
         *       doivent nettoyer l'état sans considérer le geste comme complété.
         *
         * @note La phase de chaque contact est NK_TOUCH_PHASE_CANCELLED.
         *       Les positions rapportées sont les dernières connues avant l'annulation.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkTouchCancelEvent final : public NkTouchEvent {
            public:
                /// @brief Associe le type d'événement NK_TOUCH_CANCEL à cette classe
                NK_EVENT_TYPE_FLAGS(NK_TOUCH_CANCEL)

                /**
                 * @brief Constructeur avec tableau de points de contact annulés
                 * @param points Tableau de NkTouchPoint avec dernières positions connues
                 * @param count Nombre de contacts dans le tableau
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 */
                NKENTSEU_EVENT_API NkTouchCancelEvent(
                    const NkTouchPoint* points,
                    uint32 count,
                    uint64 windowId = 0
                ) noexcept
                    : NkTouchEvent(points, count, windowId) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkTouchCancelEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le nombre de contacts annulés
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt("TouchCancel({0} contacts)", mNumTouches);
                }
        };

        // =====================================================================
        // NkGesturePinchEvent — Geste de pincement/zoom à deux doigts
        // =====================================================================

        /**
         * @brief Émis lors d'un geste de pincement (pinch) pour zoomer ou dézoomer
         *
         * Ce geste nécessite deux doigts se rapprochant ou s'écartant :
         *   - scale : facteur de zoom cumulatif depuis le début du geste (1.0 = normal)
         *   - scaleDelta : variation de scale depuis l'événement précédent (>0 = zoom in)
         *   - centerX/centerY : point pivot du zoom (généralement le centroid des deux doigts)
         *
         * @note Les handlers peuvent utiliser cet événement pour :
         *       - Zoomer une image, une carte, ou un document
         *       - Ajuster le champ de vue d'une caméra 3D
         *       - Modifier l'échelle d'un outil de dessin ou d'édition
         *
         * @note Pour un zoom fluide, appliquer le scaleDelta à chaque événement
         *       plutôt que de recalculer depuis le scale absolu, pour éviter
         *       l'accumulation d'erreurs d'arrondi.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkGesturePinchEvent final : public NkEvent {
            public:
                /// @brief Associe le type d'événement NK_GESTURE_PINCH à cette classe
                NK_EVENT_TYPE_FLAGS(NK_GESTURE_PINCH)

                /**
                 * @brief Constructeur avec paramètres de zoom
                 * @param scale Facteur de zoom cumulatif depuis le début du geste (1.0 = 100%)
                 * @param scaleDelta Variation de scale depuis l'événement précédent (>0 = zoom in)
                 * @param centerX Coordonnée X du point pivot du zoom [pixels client]
                 * @param centerY Coordonnée Y du point pivot du zoom [pixels client]
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 */
                NKENTSEU_EVENT_API NkGesturePinchEvent(
                    float scale,
                    float scaleDelta,
                    float centerX = 0.f,
                    float centerY = 0.f,
                    uint64 windowId = 0
                ) noexcept
                    : NkEvent(windowId)
                    , mScale(scale)
                    , mScaleDelta(scaleDelta)
                    , mCenterX(centerX)
                    , mCenterY(centerY) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkGesturePinchEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le facteur de zoom actuel
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt("GesturePinch(scale={0:.3})", mScale);
                }

                /// @brief Retourne le facteur de zoom cumulatif depuis le début du geste
                /// @return float représentant le scale (1.0 = taille normale, 2.0 = 200%...)
                NKENTSEU_EVENT_API_INLINE float GetScale() const noexcept {
                    return mScale;
                }

                /// @brief Retourne la variation de scale depuis l'événement précédent
                /// @return float : >0 pour zoom in, <0 pour zoom out, 0 pour pas de changement
                NKENTSEU_EVENT_API_INLINE float GetScaleDelta() const noexcept {
                    return mScaleDelta;
                }

                /// @brief Retourne la coordonnée X du point pivot du zoom
                /// @return float représentant la position horizontale [pixels client]
                NKENTSEU_EVENT_API_INLINE float GetCenterX() const noexcept {
                    return mCenterX;
                }

                /// @brief Retourne la coordonnée Y du point pivot du zoom
                /// @return float représentant la position verticale [pixels client]
                NKENTSEU_EVENT_API_INLINE float GetCenterY() const noexcept {
                    return mCenterY;
                }

                /// @brief Indique si le geste est un zoom avant (agrandissement)
                /// @return true si GetScaleDelta() > 0.0
                NKENTSEU_EVENT_API_INLINE bool IsZoomIn() const noexcept {
                    return mScaleDelta > 0.f;
                }

                /// @brief Indique si le geste est un zoom arrière (réduction)
                /// @return true si GetScaleDelta() < 0.0
                NKENTSEU_EVENT_API_INLINE bool IsZoomOut() const noexcept {
                    return mScaleDelta < 0.f;
                }

            private:
                float mScale;        ///< Facteur de zoom cumulatif (1.0 = normal)
                float mScaleDelta;   ///< Variation de scale depuis l'événement précédent
                float mCenterX;      ///< Coordonnée X du point pivot [pixels client]
                float mCenterY;      ///< Coordonnée Y du point pivot [pixels client]
        };

        // =====================================================================
        // NkGestureRotateEvent — Geste de rotation à deux doigts
        // =====================================================================

        /**
         * @brief Émis lors d'un geste de rotation autour d'un point central
         *
         * Ce geste nécessite deux doigts tournant l'un autour de l'autre :
         *   - angle : rotation cumulative depuis le début du geste [degrés]
         *   - angleDelta : variation d'angle depuis l'événement précédent
         *   - Le sens de rotation : >0 = horaire (clockwise), <0 = anti-horaire
         *
         * @note Les handlers peuvent utiliser cet événement pour :
         *       - Faire tourner une image, un objet 3D, ou un élément d'interface
         *       - Ajuster l'orientation d'un outil de dessin ou de mesure
         *       - Contrôler la direction d'un véhicule ou d'une caméra en jeu
         *
         * @note Pour une rotation fluide, appliquer l'angleDelta à chaque événement
         *       plutôt que de sauter directement à l'angle absolu, pour éviter
         *       les saccades et l'accumulation d'erreurs d'arrondi.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkGestureRotateEvent final : public NkEvent {
            public:
                /// @brief Associe le type d'événement NK_GESTURE_ROTATE à cette classe
                NK_EVENT_TYPE_FLAGS(NK_GESTURE_ROTATE)

                /**
                 * @brief Constructeur avec paramètres de rotation
                 * @param angleDegrees Rotation cumulative depuis le début du geste [degrés]
                 * @param angleDeltaDegrees Variation d'angle depuis l'événement précédent [degrés]
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 */
                NKENTSEU_EVENT_API NkGestureRotateEvent(
                    float angleDegrees,
                    float angleDeltaDegrees,
                    uint64 windowId = 0
                ) noexcept
                    : NkEvent(windowId)
                    , mAngle(angleDegrees)
                    , mAngleDelta(angleDeltaDegrees) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkGestureRotateEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant l'angle de rotation actuel
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt("GestureRotate({0:.3}°)", mAngle);
                }

                /// @brief Retourne la rotation cumulative depuis le début du geste
                /// @return float représentant l'angle en degrés (0° = orientation initiale)
                NKENTSEU_EVENT_API_INLINE float GetAngle() const noexcept {
                    return mAngle;
                }

                /// @brief Retourne la variation d'angle depuis l'événement précédent
                /// @return float en degrés : >0 = horaire, <0 = anti-horaire
                NKENTSEU_EVENT_API_INLINE float GetAngleDelta() const noexcept {
                    return mAngleDelta;
                }

                /// @brief Indique si la rotation est dans le sens horaire
                /// @return true si GetAngleDelta() > 0.0
                NKENTSEU_EVENT_API_INLINE bool IsClockwise() const noexcept {
                    return mAngleDelta > 0.f;
                }

            private:
                float mAngle;        ///< Rotation cumulative [degrés]
                float mAngleDelta;   ///< Variation d'angle depuis l'événement précédent [degrés]
        };

        // =====================================================================
        // NkGesturePanEvent — Geste de panoramique/défilement
        // =====================================================================

        /**
         * @brief Émis lors d'un geste de panoramique (pan) ou défilement (scroll)
         *
         * Ce geste permet de déplacer le contenu dans une direction :
         *   - deltaX/deltaY : déplacement depuis l'événement précédent [pixels]
         *   - velocityX/velocityY : vitesse estimée du mouvement [pixels/seconde]
         *   - numFingers : nombre de doigts participant au geste (1 pour scroll, 2+ pour pan)
         *
         * @note Les handlers peuvent utiliser cet événement pour :
         *       - Faire défiler une liste, une page web, ou un document
         *       - Déplacer une carte ou une vue dans un éditeur
         *       - Implémenter l'inertie : utiliser velocity pour continuer le mouvement
         *         après le relâchement des doigts (fling)
         *
         * @note La vélocité est estimée par le système de reconnaissance de gestes.
         *       Elle peut être utilisée pour appliquer un effet d'inertie réaliste
         *       lorsque l'utilisateur relâche brusquement (flick/throw).
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkGesturePanEvent final : public NkEvent {
            public:
                /// @brief Associe le type d'événement NK_GESTURE_PAN à cette classe
                NK_EVENT_TYPE_FLAGS(NK_GESTURE_PAN)

                /**
                 * @brief Constructeur avec paramètres de déplacement
                 * @param deltaX Déplacement horizontal depuis l'événement précédent [pixels]
                 * @param deltaY Déplacement vertical depuis l'événement précédent [pixels]
                 * @param velocityX Vitesse horizontale estimée [pixels/seconde]
                 * @param velocityY Vitesse verticale estimée [pixels/seconde]
                 * @param numFingers Nombre de doigts participant au geste
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 */
                NKENTSEU_EVENT_API NkGesturePanEvent(
                    float deltaX,
                    float deltaY,
                    float velocityX = 0.f,
                    float velocityY = 0.f,
                    uint32 numFingers = 1,
                    uint64 windowId = 0
                ) noexcept
                    : NkEvent(windowId)
                    , mDeltaX(deltaX)
                    , mDeltaY(deltaY)
                    , mVelocityX(velocityX)
                    , mVelocityY(velocityY)
                    , mNumFingers(numFingers) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkGesturePanEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le déplacement effectué
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt("GesturePan(d={0:.3},{1:.3})", mDeltaX, mDeltaY);
                }

                /// @brief Retourne le déplacement horizontal depuis l'événement précédent
                /// @return float représentant le delta X [pixels] (>0 = droite, <0 = gauche)
                NKENTSEU_EVENT_API_INLINE float GetDeltaX() const noexcept {
                    return mDeltaX;
                }

                /// @brief Retourne le déplacement vertical depuis l'événement précédent
                /// @return float représentant le delta Y [pixels] (>0 = bas, <0 = haut)
                NKENTSEU_EVENT_API_INLINE float GetDeltaY() const noexcept {
                    return mDeltaY;
                }

                /// @brief Retourne la vitesse horizontale estimée du geste
                /// @return float représentant la vélocité X [pixels/seconde]
                NKENTSEU_EVENT_API_INLINE float GetVelocityX() const noexcept {
                    return mVelocityX;
                }

                /// @brief Retourne la vitesse verticale estimée du geste
                /// @return float représentant la vélocité Y [pixels/seconde]
                NKENTSEU_EVENT_API_INLINE float GetVelocityY() const noexcept {
                    return mVelocityY;
                }

                /// @brief Retourne le nombre de doigts participant au geste
                /// @return uint32 : 1 pour scroll classique, 2+ pour pan multi-touch
                NKENTSEU_EVENT_API_INLINE uint32 GetNumFingers() const noexcept {
                    return mNumFingers;
                }

            private:
                float mDeltaX;      ///< Déplacement horizontal [pixels]
                float mDeltaY;      ///< Déplacement vertical [pixels]
                float mVelocityX;   ///< Vitesse horizontale estimée [pixels/seconde]
                float mVelocityY;   ///< Vitesse verticale estimée [pixels/seconde]
                uint32 mNumFingers; ///< Nombre de doigts participant au geste
        };

        // =====================================================================
        // NkGestureSwipeEvent — Geste de balayage rapide (swipe)
        // =====================================================================

        /**
         * @brief Émis lors d'un geste de balayage rapide (swipe) dans une direction
         *
         * Ce geste est détecté quand un doigt se déplace rapidement sur une courte distance :
         *   - direction : sens du swipe (LEFT, RIGHT, UP, DOWN)
         *   - speed : vélocité estimée du geste [pixels/seconde]
         *   - numFingers : nombre de doigts ayant effectué le swipe (généralement 1)
         *
         * @note Les handlers peuvent utiliser cet événement pour :
         *       - Naviguer entre pages ou sections (swipe gauche/droite)
         *       - Afficher/masquer des menus latéraux (swipe depuis le bord)
         *       - Supprimer un élément d'une liste (swipe + confirmation)
         *       - Contrôler un personnage ou une caméra en jeu (swipe directionnel)
         *
         * @note La détection de swipe utilise des heuristiques de vitesse et d'amplitude.
         *       Un mouvement lent sur longue distance sera un Pan, pas un Swipe.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkGestureSwipeEvent final : public NkEvent {
            public:
                /// @brief Associe le type d'événement NK_GESTURE_SWIPE à cette classe
                NK_EVENT_TYPE_FLAGS(NK_GESTURE_SWIPE)

                /**
                 * @brief Constructeur avec paramètres de swipe
                 * @param direction Direction détectée du balayage (LEFT, RIGHT, UP, DOWN)
                 * @param speed Vélocité estimée du geste [pixels/seconde]
                 * @param numFingers Nombre de doigts ayant effectué le swipe
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 */
                NKENTSEU_EVENT_API NkGestureSwipeEvent(
                    NkSwipeDirection direction,
                    float speed = 0.f,
                    uint32 numFingers = 1,
                    uint64 windowId = 0
                ) noexcept
                    : NkEvent(windowId)
                    , mDirection(direction)
                    , mSpeed(speed)
                    , mNumFingers(numFingers) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkGestureSwipeEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant la direction du swipe
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString("GestureSwipe(")
                         + NkSwipeDirectionToString(mDirection) + ")";
                }

                /// @brief Retourne la direction détectée du balayage
                /// @return NkSwipeDirection indiquant LEFT, RIGHT, UP, ou DOWN
                NKENTSEU_EVENT_API_INLINE NkSwipeDirection GetDirection() const noexcept {
                    return mDirection;
                }

                /// @brief Retourne la vélocité estimée du geste
                /// @return float représentant la vitesse [pixels/seconde]
                NKENTSEU_EVENT_API_INLINE float GetSpeed() const noexcept {
                    return mSpeed;
                }

                /// @brief Retourne le nombre de doigts ayant effectué le swipe
                /// @return uint32 (généralement 1 pour un swipe classique)
                NKENTSEU_EVENT_API_INLINE uint32 GetNumFingers() const noexcept {
                    return mNumFingers;
                }

                /// @brief Indique si le swipe est vers la gauche
                /// @return true si GetDirection() == NK_SWIPE_LEFT
                NKENTSEU_EVENT_API_INLINE bool IsLeft() const noexcept {
                    return mDirection == NkSwipeDirection::NK_SWIPE_LEFT;
                }

                /// @brief Indique si le swipe est vers la droite
                /// @return true si GetDirection() == NK_SWIPE_RIGHT
                NKENTSEU_EVENT_API_INLINE bool IsRight() const noexcept {
                    return mDirection == NkSwipeDirection::NK_SWIPE_RIGHT;
                }

                /// @brief Indique si le swipe est vers le haut
                /// @return true si GetDirection() == NK_SWIPE_UP
                NKENTSEU_EVENT_API_INLINE bool IsUp() const noexcept {
                    return mDirection == NkSwipeDirection::NK_SWIPE_UP;
                }

                /// @brief Indique si le swipe est vers le bas
                /// @return true si GetDirection() == NK_SWIPE_DOWN
                NKENTSEU_EVENT_API_INLINE bool IsDown() const noexcept {
                    return mDirection == NkSwipeDirection::NK_SWIPE_DOWN;
                }

            private:
                NkSwipeDirection mDirection;  ///< Direction détectée du swipe
                float mSpeed;                 ///< Vélocité estimée [pixels/seconde]
                uint32 mNumFingers;           ///< Nombre de doigts ayant effectué le geste
        };

        // =====================================================================
        // NkGestureTapEvent — Geste de tape simple ou multiple
        // =====================================================================

        /**
         * @brief Émis lors d'un geste de tape (appui court et relâchement rapide)
         *
         * Ce geste correspond à un clic tactile :
         *   - x/y : position du tap dans l'espace client [pixels]
         *   - tapCount : nombre de taps rapides détectés (1 = simple, 2 = double...)
         *   - numFingers : nombre de doigts ayant effectué le tap (généralement 1)
         *
         * @note Les handlers peuvent utiliser cet événement pour :
         *       - Sélectionner un élément à la position tapée
         *       - Activer un bouton ou un lien
         *       - Détecter un double-tap pour zoomer ou éditer
         *       - Afficher un menu contextuel (long press vs tap)
         *
         * @note La détection de tap vs long press est basée sur un seuil temporel.
         *       Un appui maintenu au-delà de ~500ms générera un LongPressEvent
         *       au lieu d'un TapEvent.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkGestureTapEvent final : public NkEvent {
            public:
                /// @brief Associe le type d'événement NK_GESTURE_TAP à cette classe
                NK_EVENT_TYPE_FLAGS(NK_GESTURE_TAP)

                /**
                 * @brief Constructeur avec paramètres de tap
                 * @param x Coordonnée X du tap dans l'espace client [pixels]
                 * @param y Coordonnée Y du tap dans l'espace client [pixels]
                 * @param tapCount Nombre de taps rapides détectés (1 = simple, 2 = double...)
                 * @param numFingers Nombre de doigts ayant effectué le tap
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 */
                NKENTSEU_EVENT_API NkGestureTapEvent(
                    float x = 0.f,
                    float y = 0.f,
                    uint32 tapCount = 1,
                    uint32 numFingers = 1,
                    uint64 windowId = 0
                ) noexcept
                    : NkEvent(windowId)
                    , mX(x)
                    , mY(y)
                    , mTapCount(tapCount)
                    , mNumFingers(numFingers) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkGestureTapEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le nombre de taps détectés
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt("GestureTap(x{0})", mTapCount);
                }

                /// @brief Retourne la coordonnée X du tap
                /// @return float représentant la position horizontale [pixels client]
                NKENTSEU_EVENT_API_INLINE float GetX() const noexcept {
                    return mX;
                }

                /// @brief Retourne la coordonnée Y du tap
                /// @return float représentant la position verticale [pixels client]
                NKENTSEU_EVENT_API_INLINE float GetY() const noexcept {
                    return mY;
                }

                /// @brief Retourne le nombre de taps rapides détectés
                /// @return uint32 : 1 = simple tap, 2 = double tap, etc.
                NKENTSEU_EVENT_API_INLINE uint32 GetTapCount() const noexcept {
                    return mTapCount;
                }

                /// @brief Retourne le nombre de doigts ayant effectué le tap
                /// @return uint32 (généralement 1 pour un tap classique)
                NKENTSEU_EVENT_API_INLINE uint32 GetNumFingers() const noexcept {
                    return mNumFingers;
                }

                /// @brief Indique si le geste est un double-tap ou plus
                /// @return true si GetTapCount() >= 2
                NKENTSEU_EVENT_API_INLINE bool IsDoubleTap() const noexcept {
                    return mTapCount >= 2;
                }

            private:
                float mX;           ///< Coordonnée X du tap [pixels client]
                float mY;           ///< Coordonnée Y du tap [pixels client]
                uint32 mTapCount;   ///< Nombre de taps rapides détectés
                uint32 mNumFingers; ///< Nombre de doigts ayant effectué le geste
        };

        // =====================================================================
        // NkGestureLongPressEvent — Geste d'appui long
        // =====================================================================

        /**
         * @brief Émis lorsqu'un doigt reste appuyé au-delà d'un seuil temporel
         *
         * Ce geste correspond à un "appui long" ou "press and hold" :
         *   - x/y : position de l'appui dans l'espace client [pixels]
         *   - durationMs : durée de l'appui avant détection du geste [millisecondes]
         *   - numFingers : nombre de doigts maintenus (généralement 1)
         *
         * @note Les handlers peuvent utiliser cet événement pour :
         *       - Afficher un menu contextuel à la position de l'appui
         *       - Activer un mode d'édition ou de sélection multiple
         *       - Démarrer un drag-and-drop avec feedback visuel
         *       - Afficher une info-bulle ou une prévisualisation
         *
         * @note Le seuil de détection est typiquement ~500ms. Un appui plus court
         *       générera un TapEvent. Un appui maintenu après LongPressEvent
         *       peut générer des TouchMoveEvent si le doigt bouge.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkGestureLongPressEvent final : public NkEvent {
            public:
                /// @brief Associe le type d'événement NK_GESTURE_LONG_PRESS à cette classe
                NK_EVENT_TYPE_FLAGS(NK_GESTURE_LONG_PRESS)

                /**
                 * @brief Constructeur avec paramètres d'appui long
                 * @param x Coordonnée X de l'appui dans l'espace client [pixels]
                 * @param y Coordonnée Y de l'appui dans l'espace client [pixels]
                 * @param durationMs Durée de l'appui avant détection du geste [millisecondes]
                 * @param numFingers Nombre de doigts maintenus pour le geste
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 */
                NKENTSEU_EVENT_API NkGestureLongPressEvent(
                    float x = 0.f,
                    float y = 0.f,
                    float durationMs = 0.f,
                    uint32 numFingers = 1,
                    uint64 windowId = 0
                ) noexcept
                    : NkEvent(windowId)
                    , mX(x)
                    , mY(y)
                    , mDurationMs(durationMs)
                    , mNumFingers(numFingers) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkGestureLongPressEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant la durée de l'appui long
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt("GestureLongPress({0:.1}ms)", mDurationMs);
                }

                /// @brief Retourne la coordonnée X de l'appui
                /// @return float représentant la position horizontale [pixels client]
                NKENTSEU_EVENT_API_INLINE float GetX() const noexcept {
                    return mX;
                }

                /// @brief Retourne la coordonnée Y de l'appui
                /// @return float représentant la position verticale [pixels client]
                NKENTSEU_EVENT_API_INLINE float GetY() const noexcept {
                    return mY;
                }

                /// @brief Retourne la durée de l'appui avant détection du geste
                /// @return float représentant le temps [millisecondes]
                NKENTSEU_EVENT_API_INLINE float GetDurationMs() const noexcept {
                    return mDurationMs;
                }

                /// @brief Retourne le nombre de doigts maintenus pour le geste
                /// @return uint32 (généralement 1 pour un long press classique)
                NKENTSEU_EVENT_API_INLINE uint32 GetNumFingers() const noexcept {
                    return mNumFingers;
                }

            private:
                float mX;           ///< Coordonnée X de l'appui [pixels client]
                float mY;           ///< Coordonnée Y de l'appui [pixels client]
                float mDurationMs;  ///< Durée de l'appui avant détection [millisecondes]
                uint32 mNumFingers; ///< Nombre de doigts maintenus pour le geste
        };

    } // namespace nkentseu

#endif // NKENTSEU_EVENT_NKTOUCHEVENT_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKTOUCHEVENT.H
// =============================================================================
// Ce fichier définit la hiérarchie des événements tactiles et gestes pour le module NKEvent.
// Les exemples ci-dessous illustrent les patterns d'usage recommandés.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Gestion d'un canvas de dessin tactile avec multi-touch
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkTouchEvent.h"

    class TouchCanvas {
    public:
        void OnTouchBegin(const nkentseu::NkTouchBeginEvent& event) {
            using namespace nkentseu;

            // Démarrer un nouveau trait pour chaque contact
            for (uint32 i = 0; i < event.GetNumTouches(); ++i) {
                const auto& touch = event.GetTouch(i);
                if (touch.IsActive()) {
                    mActiveStrokes[touch.id] = Stroke{
                        .points = { {touch.clientX, touch.clientY} },
                        .color = mCurrentColor,
                        .width = mCurrentBrushSize
                    };
                }
            }
        }

        void OnTouchMove(const nkentseu::NkTouchMoveEvent& event) {
            using namespace nkentseu;

            // Étendre les traits existants avec les nouvelles positions
            for (uint32 i = 0; i < event.GetNumTouches(); ++i) {
                const auto& touch = event.GetTouch(i);
                auto it = mActiveStrokes.find(touch.id);
                if (it != mActiveStrokes.end() && touch.HasMoved()) {
                    it->second.points.emplace_back(touch.clientX, touch.clientY);
                    // Rendu incrémental pour réactivité
                    RenderStrokeSegment(it->second, touch);
                }
            }
        }

        void OnTouchEnd(const nkentseu::NkTouchEndEvent& event) {
            using namespace nkentseu;

            // Finaliser et sauvegarder les traits terminés
            for (uint32 i = 0; i < event.GetNumTouches(); ++i) {
                const auto& touch = event.GetTouch(i);
                auto it = mActiveStrokes.find(touch.id);
                if (it != mActiveStrokes.end()) {
                    mCompletedStrokes.push_back(traits::NkMove(it->second));
                    mActiveStrokes.erase(it);
                }
            }
        }

    private:
        struct Stroke {
            std::vector<Vec2> points;
            Color color;
            float width;
        };

        std::unordered_map<uint64, Stroke> mActiveStrokes;
        std::vector<Stroke> mCompletedStrokes;
        Color mCurrentColor = Color::Black;
        float mCurrentBrushSize = 4.0f;

        void RenderStrokeSegment(const Stroke& stroke, const nkentseu::NkTouchPoint& touch);
    };
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Navigation par gestes dans une galerie photo
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkTouchEvent.h"

    class PhotoGallery {
    public:
        void OnGestureSwipe(const nkentseu::NkGestureSwipeEvent& event) {
            using namespace nkentseu;

            // Navigation horizontale entre photos
            if (event.IsLeft()) {
                NextPhoto();  // Swipe gauche → photo suivante
            }
            else if (event.IsRight()) {
                PreviousPhoto();  // Swipe droite → photo précédente
            }
            // Swipe vertical ignoré dans ce contexte
        }

        void OnGesturePinch(const nkentseu::NkGesturePinchEvent& event) {
            using namespace nkentseu;

            // Zoom sur la photo courante avec pivot au centre du geste
            float currentZoom = mImageView.GetZoom();
            float newZoom = currentZoom * event.GetScaleDelta();

            // Clamping pour éviter les extrêmes
            newZoom = NkClamp(newZoom, 1.0f, 8.0f);
            mImageView.SetZoom(newZoom, event.GetCenterX(), event.GetCenterY());
        }

        void OnGestureTap(const nkentseu::NkGestureTapEvent& event) {
            using namespace nkentseu;

            // Toggle de l'interface au tap simple
            if (event.GetTapCount() == 1) {
                ToggleUI();  // Afficher/masquer les contrôles
            }
            // Double-tap pour zoom rapide au point tapé
            else if (event.IsDoubleTap()) {
                mImageView.ToggleZoomAt(event.GetX(), event.GetY());
            }
        }

    private:
        ImageView mImageView;

        void NextPhoto();
        void PreviousPhoto();
        void ToggleUI();
    };
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Carte interactive avec pan, zoom et rotation
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkTouchEvent.h"

    class InteractiveMap {
    public:
        void OnGesturePan(const nkentseu::NkGesturePanEvent& event) {
            using namespace nkentseu;

            // Déplacement de la vue de carte
            mMapView.Translate(-event.GetDeltaX(), -event.GetDeltaY());

            // Inertie optionnelle : si vitesse élevée, continuer le mouvement
            if (event.GetVelocityX() != 0.f || event.GetVelocityY() != 0.f) {
                mInertiaVelocity = { event.GetVelocityX(), event.GetVelocityY() };
                StartInertiaAnimation();
            }
        }

        void OnGesturePinch(const nkentseu::NkGesturePinchEvent& event) {
            using namespace nkentseu;

            // Zoom avec pivot au centre du geste (entre les deux doigts)
            mMapView.ZoomAt(
                event.GetScaleDelta(),
                event.GetCenterX(),
                event.GetCenterY()
            );
        }

        void OnGestureRotate(const nkentseu::NkGestureRotateEvent& event) {
            using namespace nkentseu;

            // Rotation de la carte autour du centroid
            if (event.GetAngleDelta() != 0.f) {
                mMapView.Rotate(event.GetAngleDelta());
            }
        }

        void OnGestureLongPress(const nkentseu::NkGestureLongPressEvent& event) {
            using namespace nkentseu;

            // Afficher un marqueur ou une info à la position de l'appui long
            ShowLocationInfo(event.GetX(), event.GetY());
        }

    private:
        MapView mMapView;
        Vec2 mInertiaVelocity = {0.f, 0.f};

        void StartInertiaAnimation();
        void ShowLocationInfo(float x, float y);
    };
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Détection de gestes complexes avec combinaison de touches
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkTouchEvent.h"

    class GestureRecognizer {
    public:
        // Reconnaissance d'un geste "three-finger swipe" pour changer d'espace de travail
        bool RecognizeThreeFingerSwipe(const nkentseu::NkGestureSwipeEvent& event) {
            using namespace nkentseu;

            if (event.GetNumFingers() == 3) {
                if (event.IsUp()) {
                    ShowAppSwitcher();  // Swipe up à 3 doigts
                    return true;
                }
                else if (event.IsDown()) {
                    ShowNotificationCenter();  // Swipe down à 3 doigts
                    return true;
                }
            }
            return false;
        }

        // Reconnaissance d'un "pinch with rotation" pour transformation libre
        void OnMultiGesture(
            const nkentseu::NkGesturePinchEvent* pinch,
            const nkentseu::NkGestureRotateEvent* rotate
        ) {
            using namespace nkentseu;

            if (pinch && rotate) {
                // Transformation combinée : zoom + rotation simultanés
                mSelectedObject->Transform(
                    pinch->GetScaleDelta(),  // Scale
                    rotate->GetAngleDelta(), // Rotation
                    pinch->GetCenterX(),     // Pivot X
                    pinch->GetCenterY()      // Pivot Y
                );
            }
        }

    private:
        void ShowAppSwitcher();
        void ShowNotificationCenter();
    };
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Accessibilité — adaptation aux préférences de mouvement réduit
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkTouchEvent.h"
    #include "NKEvent/NkSystemEvent.h"  // Pour NkSystemAccessibilityEvent

    class AccessibleTouchHandler {
    public:
        void OnAccessibilityChanged(const nkentseu::NkSystemAccessibilityEvent& event) {
            using namespace nkentseu;

            // Désactiver les gestes complexes si l'utilisateur a demandé
            // la réduction des animations/mouvements
            mReduceMotion = event.ReduceMotion();
            mGestureThreshold = mReduceMotion ? 1.5f : 1.0f;  // Rendre les gestes plus "délibérés"
        }

        bool ShouldRecognizeSwipe(const nkentseu::NkGestureSwipeEvent& event) {
            using namespace nkentseu;

            // En mode "reduce motion", ignorer les swipes trop rapides ou courts
            // pour éviter les déclenchements accidentels
            if (mReduceMotion) {
                return event.GetSpeed() > kMinSpeedReduced
                    && event.GetDistance() > kMinDistanceReduced;
            }
            return true;  // Comportement normal
        }

    private:
        bool mReduceMotion = false;
        float mGestureThreshold = 1.0f;

        static constexpr float kMinSpeedReduced = 800.f;    // pixels/sec
        static constexpr float kMinDistanceReduced = 150.f; // pixels
    };
*/

// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
    1. GESTION DU MULTI-TOUCH :
       - NK_MAX_TOUCH_POINTS = 32 : ajuster selon les besoins réels pour économiser mémoire
       - Les NkTouchPoint sont copiés dans un buffer fixe : éviter les gros structs
       - Itérer avec GetNumTouches() et GetTouch(i) pour traiter chaque contact

    2. COORDONNÉES ET ESPACES :
       - clientX/Y : relatifs à la zone de contenu de la fenêtre (pour l'UI)
       - screenX/Y : absolus sur l'écran (pour le drag entre fenêtres)
       - normalX/Y : [0,1] indépendants de la résolution (pour le responsive)
       - Choisir l'espace adapté au cas d'usage pour éviter les conversions

    3. DÉTECTION DE GESTES :
       - Les gestes (Pinch, Rotate, Swipe...) sont détectés par un recognizer
       - Un même mouvement physique peut générer à la fois TouchMove ET GesturePan
       - Décider quelle couche gérer : bas niveau (Touch*) ou haut niveau (Gesture*)

    4. PERFORMANCE DES HANDLERS :
       - TouchMove peut être émis 60-120 fois/seconde : éviter les allocations
       - Utiliser les deltas (deltaX, scaleDelta) pour des mises à jour incrémentales
       - Pour l'inertie, utiliser velocityX/Y plutôt que de recalculer à chaque frame

    5. ACCESSIBILITÉ :
       - Respecter NkSystemAccessibilityEvent::ReduceMotion() pour désactiver
         les gestes complexes ou les animations de feedback
       - Fournir des alternatives clavier/souris pour toutes les actions tactiles
       - Tester avec des tailles de police augmentées (fontScale) pour vérifier
         que les zones de tap restent suffisamment grandes

    6. EXTENSIBILITÉ :
       - Pour ajouter un nouveau geste :
         a) Créer une classe dérivée de NkEvent (pas nécessairement de NkTouchEvent)
         b) Utiliser NK_EVENT_TYPE_FLAGS avec un nouveau type dans NkEventType
         c) Implémenter Clone() et ToString() pour cohérence avec l'infrastructure
         d) Documenter les paramètres et le cas d'usage dans la Doxygen
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================