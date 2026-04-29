// =============================================================================
// NKEvent/NkSystemEvent.h
// Hiérarchie des événements système de bas niveau.
//
// Catégorie : NK_CAT_SYSTEM
//
// Architecture :
//   - NkPowerState         : état du système d'alimentation (batterie, veille...)
//   - NkDisplayChange      : type de changement de configuration d'affichage
//   - NkMemoryPressure     : niveau de pression mémoire RAM pour gestion proactive
//   - NkDisplayInfo        : structure descriptive d'un moniteur physique
//   - NkSystemEvent        : classe de base abstraite pour tous les événements système
//   - Classes dérivées     : implémentations concrètes par type d'événement
//
// Design :
//   - Héritage polymorphe depuis NkEvent pour intégration au dispatcher central
//   - Macros NK_EVENT_* pour éviter la duplication de code boilerplate
//   - Méthodes Clone() pour copie profonde (replay, tests, serialization)
//   - ToString() surchargé pour débogage et logging significatif
//   - Accesseurs inline pour performance critique (lecture fréquente)
//
// Usage typique :
//   if (auto* powerEvt = event.As<NkSystemPowerEvent>()) {
//       if (powerEvt->IsSuspending()) { SaveGameState(); }
//   }
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_EVENT_NKSYSTEMEVENT_H
#define NKENTSEU_EVENT_NKSYSTEMEVENT_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des dépendances internes et externes nécessaires.
    // Toutes les dépendances utilisent les modules NK* du projet.

    #include "NKEvent/NkEvent.h"                        // Classe de base NkEvent + macros
    #include "NKEvent/NkEventApi.h"                     // Macros d'export NKENTSEU_EVENT_API
    #include "NKContainers/String/NkStringUtils.h"      // Utilitaires de manipulation de chaînes
    #include "NKMemory/NkUtils.h"                       // Utilitaires mémoire (NkMemSet, etc.)
    #include <cstring>                                  // strncpy pour compatibilité C

    namespace nkentseu {

        // =====================================================================
        // NkPowerState — Énumération des états du système d'alimentation
        // =====================================================================

        /// @brief Représente les différents états possibles du système d'alimentation
        /// @note Utilisé par NkSystemPowerEvent pour notifier les transitions d'énergie
        enum class NKENTSEU_EVENT_CLASS_EXPORT NkPowerState : uint32 {
            NK_POWER_NORMAL             = 0,    ///< Fonctionnement normal sur secteur ou batterie suffisante
            NK_POWER_LOW_BATTERY,               ///< Batterie faible : avertissement utilisateur recommandé
            NK_POWER_CRITICAL_BATTERY,          ///< Batterie critique (< 5%) : sauvegarde urgente requise
            NK_POWER_PLUGGED_IN,                ///< Alimentation secteur connectée : mode performance possible
            NK_POWER_UNPLUGGED,                 ///< Passage sur batterie : activer les économies d'énergie
            NK_POWER_SUSPEND,                   ///< Mise en veille imminente : sauvegarder l'état volatile
            NK_POWER_RESUME,                    ///< Réveil de veille : restaurer l'état et les ressources
            NK_POWER_HIBERNATE,                 ///< Hibernation imminente : persister l'état sur disque
            NK_POWER_SHUTDOWN,                  ///< Arrêt système imminent : fermeture gracieuse obligatoire
            NK_POWER_MAX                        ///< Sentinelle : nombre de valeurs valides (pour validation)
        };

        /// @brief Convertit une valeur NkPowerState en chaîne lisible pour le débogage
        /// @param s La valeur d'état d'alimentation à convertir
        /// @return Pointeur vers une chaîne littérale statique (ne pas libérer)
        /// @note Fonction noexcept pour usage dans les contextes critiques
        inline const char* NkPowerStateToString(NkPowerState s) noexcept {
            switch (s) {
                case NkPowerState::NK_POWER_NORMAL:
                    return "NORMAL";
                case NkPowerState::NK_POWER_LOW_BATTERY:
                    return "LOW_BATTERY";
                case NkPowerState::NK_POWER_CRITICAL_BATTERY:
                    return "CRITICAL_BATTERY";
                case NkPowerState::NK_POWER_PLUGGED_IN:
                    return "PLUGGED_IN";
                case NkPowerState::NK_POWER_UNPLUGGED:
                    return "UNPLUGGED";
                case NkPowerState::NK_POWER_SUSPEND:
                    return "SUSPEND";
                case NkPowerState::NK_POWER_RESUME:
                    return "RESUME";
                case NkPowerState::NK_POWER_HIBERNATE:
                    return "HIBERNATE";
                case NkPowerState::NK_POWER_SHUTDOWN:
                    return "SHUTDOWN";
                default:
                    return "UNKNOWN";
            }
        }

        // =====================================================================
        // NkDisplayChange — Énumération des types de changements d'affichage
        // =====================================================================

        /// @brief Représente les différents types de modifications de configuration d'affichage
        /// @note Utilisé par NkSystemDisplayEvent pour identifier la nature du changement
        enum class NKENTSEU_EVENT_CLASS_EXPORT NkDisplayChange : uint32 {
            NK_DISPLAY_ADDED            = 0,    ///< Nouveau moniteur connecté (hotplug)
            NK_DISPLAY_REMOVED,                 ///< Moniteur déconnecté (hot-unplug)
            NK_DISPLAY_RESOLUTION_CHANGED,      ///< Résolution ou fréquence de rafraîchissement modifiée
            NK_DISPLAY_ORIENTATION_CHANGED,     ///< Orientation changée (portrait ↔ paysage)
            NK_DISPLAY_DPI_CHANGED,             ///< Facteur d'échelle DPI modifié (multi-écran)
            NK_DISPLAY_PRIMARY_CHANGED,         ///< Moniteur principal changé (taskbar, menu démarrer...)
            NK_DISPLAY_MAX                      ///< Sentinelle : nombre de valeurs valides (pour validation)
        };

        // =====================================================================
        // NkMemoryPressure — Énumération des niveaux de pression mémoire RAM
        // =====================================================================

        /// @brief Représente les niveaux de pression sur la mémoire système
        /// @note Critique pour les applications mobiles et embarquées (iOS, Android)
        enum class NKENTSEU_EVENT_CLASS_EXPORT NkMemoryPressure : uint32 {
            NK_MEM_NORMAL   = 0,    ///< RAM suffisante : aucun ajustement requis
            NK_MEM_MODERATE,        ///< Pression modérée : libérer les caches non critiques
            NK_MEM_CRITICAL,        ///< Pression critique : libérer tout le non-essentiel immédiatement
            NK_MEM_MAX              ///< Sentinelle : nombre de valeurs valides (pour validation)
        };

        /// @brief Convertit une valeur NkMemoryPressure en chaîne lisible pour le débogage
        /// @param p La valeur de pression mémoire à convertir
        /// @return Pointeur vers une chaîne littérale statique (ne pas libérer)
        /// @note Fonction noexcept pour usage dans les contextes critiques
        inline const char* NkMemoryPressureToString(NkMemoryPressure p) noexcept {
            switch (p) {
                case NkMemoryPressure::NK_MEM_NORMAL:
                    return "NORMAL";
                case NkMemoryPressure::NK_MEM_MODERATE:
                    return "MODERATE";
                case NkMemoryPressure::NK_MEM_CRITICAL:
                    return "CRITICAL";
                default:
                    return "UNKNOWN";
            }
        }

        // =====================================================================
        // NkDisplayInfo — Structure descriptive d'un moniteur physique
        // =====================================================================

        /**
         * @brief Décrit les caractéristiques techniques et logiques d'un moniteur
         *
         * Cette structure est utilisée pour notifier les changements de configuration
         * d'affichage et permettre à l'application de s'adapter dynamiquement.
         *
         * @note Les résolutions "logiques" tiennent compte du DPI scaling,
         *       tandis que les résolutions "physiques" reflètent les pixels réels.
         */
        struct NKENTSEU_EVENT_CLASS_EXPORT NkDisplayInfo {
            uint32 index       = 0;       ///< Index système du moniteur (0 = moniteur principal)
            uint32 width       = 0;       ///< Résolution horizontale en pixels logiques
            uint32 height      = 0;       ///< Résolution verticale en pixels logiques
            uint32 physWidth   = 0;       ///< Résolution horizontale en pixels physiques réels
            uint32 physHeight  = 0;       ///< Résolution verticale en pixels physiques réels
            uint32 refreshRate = 60;      ///< Taux de rafraîchissement en Hz (60, 120, 144...)
            float32 dpiScale   = 1.f;     ///< Facteur d'échelle DPI (1.0 = 100%, 1.5 = 150%, 2.0 = 200%)
            float32 dpiX       = 96.f;    ///< Densité de pixels horizontale physique (DPI)
            float32 dpiY       = 96.f;    ///< Densité de pixels verticale physique (DPI)
            int32 posX         = 0;       ///< Position X dans l'espace virtuel multi-écrans
            int32 posY         = 0;       ///< Position Y dans l'espace virtuel multi-écrans
            bool  isPrimary    = false;   ///< Indique si ce moniteur est le principal (taskbar, dock...)
            char  name[64]     = {};      ///< Nom lisible du moniteur (ex: "DELL U2720Q", "Built-in Retina")

            /// @brief Constructeur par défaut avec initialisation sécurisée du buffer name
            NKENTSEU_INLINE NkDisplayInfo() {
                memory::NkMemSet(name, 0, sizeof(name));
            }
        };

        // =====================================================================
        // NkSystemEvent — Classe de base abstraite pour événements système
        // =====================================================================

        /**
         * @brief Classe de base polymorphe pour tous les événements de bas niveau système
         *
         * Catégorie associée : NK_CAT_SYSTEM
         *
         * Cette classe sert de point d'ancrage commun pour tous les événements
         * liés au système d'exploitation : alimentation, affichage, mémoire, locale...
         *
         * @note Les classes dérivées DOIVENT utiliser les macros NK_EVENT_TYPE_FLAGS
         *       et NK_EVENT_CATEGORY_FLAGS pour éviter la duplication de code.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkSystemEvent : public NkEvent {
            public:
                /// @brief Implémente le filtrage par catégorie SYSTEM pour tous les dérivés
                NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_SYSTEM)

            protected:
                /// @brief Constructeur protégé — réservé aux classes dérivées
                /// @param windowId Identifiant de la fenêtre source (0 = événement global OS)
                explicit NkSystemEvent(uint64 windowId = 0) noexcept
                    : NkEvent(windowId) {
                }
        };

        // =====================================================================
        // NkSystemPowerEvent — Événement de transition d'état d'alimentation
        // =====================================================================

        /**
         * @brief Émis lors des transitions d'état du système d'alimentation
         *
         * Cet événement notifie l'application des changements critiques :
         *   - Passage sur batterie / secteur
         *   - Niveaux de batterie faibles ou critiques
         *   - Mise en veille, hibernation ou arrêt imminent
         *
         * @warning Pour NK_POWER_SUSPEND et NK_POWER_HIBERNATE, l'application
         *          DOIT sauvegarder son état avant que le handler retourne,
         *          car l'OS peut suspendre l'exécution immédiatement après.
         *
         * @note batteryLevel : valeur normalisée [0.0, 1.0] ou -1.0 si branché/inconnu
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkSystemPowerEvent final : public NkSystemEvent {
            public:
                /// @brief Associe le type d'événement NK_SYSTEM_POWER à cette classe
                NK_EVENT_TYPE_FLAGS(NK_SYSTEM_POWER)

                /**
                 * @brief Constructeur avec paramètres complets d'état d'alimentation
                 * @param state        Nouvel état d'alimentation (ex: NK_POWER_LOW_BATTERY)
                 * @param batteryLevel Niveau de batterie normalisé [0.0, 1.0] ou -1.0 si inconnu/secteur
                 * @param pluggedIn    true si l'appareil est connecté à l'alimentation secteur
                 * @param windowId     Identifiant de la fenêtre source (0 = événement global OS)
                 */
                NkSystemPowerEvent(
                    NkPowerState state,
                    float32 batteryLevel = -1.f,
                    bool pluggedIn = false,
                    uint64 windowId = 0
                ) noexcept
                    : NkSystemEvent(windowId)
                    , mState(state)
                    , mBatteryLevel(batteryLevel)
                    , mPluggedIn(pluggedIn) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkSystemPowerEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant l'état d'alimentation et le niveau de batterie
                NkString ToString() const override {
                    return NkString::Fmt(
                        "SystemPower({0}{1})",
                        NkPowerStateToString(mState),
                        (mBatteryLevel >= 0.f
                            ? NkString::Fmt(" bat={0}%", static_cast<int>(mBatteryLevel * 100))
                            : " wired")
                    );
                }

                /// @brief Retourne l'état actuel du système d'alimentation
                /// @return Valeur NkPowerState décrivant la situation énergétique
                NKENTSEU_INLINE NkPowerState GetState() const noexcept {
                    return mState;
                }

                /// @brief Retourne le niveau de batterie normalisé
                /// @return float32 dans [0.0, 1.0] ou -1.0 si branché/inconnu
                NKENTSEU_INLINE float32 GetBatteryLevel() const noexcept {
                    return mBatteryLevel;
                }

                /// @brief Indique si l'appareil est connecté à l'alimentation secteur
                /// @return true si branché, false si sur batterie
                NKENTSEU_INLINE bool IsPluggedIn() const noexcept {
                    return mPluggedIn;
                }

                /// @brief Indique si le système est en transition vers un état de suspension
                /// @return true pour SUSPEND, HIBERNATE ou SHUTDOWN
                NKENTSEU_INLINE bool IsSuspending() const noexcept {
                    return mState == NkPowerState::NK_POWER_SUSPEND
                        || mState == NkPowerState::NK_POWER_HIBERNATE
                        || mState == NkPowerState::NK_POWER_SHUTDOWN;
                }

                /// @brief Indique si le système vient de reprendre après une veille
                /// @return true pour l'état NK_POWER_RESUME
                NKENTSEU_INLINE bool IsResuming() const noexcept {
                    return mState == NkPowerState::NK_POWER_RESUME;
                }

            private:
                NkPowerState mState;          ///< État courant du système d'alimentation
                float32 mBatteryLevel;        ///< Niveau de batterie normalisé ou -1.0
                bool mPluggedIn;              ///< Statut de connexion secteur
        };

        // =====================================================================
        // NkSystemLocaleEvent — Événement de changement de locale système
        // =====================================================================

        /**
         * @brief Émis lorsque l'utilisateur modifie la langue ou la région système
         *
         * Cet événement permet à l'application de mettre à jour dynamiquement :
         *   - Les chaînes de caractères localisées (traductions)
         *   - Les formats de date, heure, nombre et devise
         *   - Les préférences régionales (ordre des noms, unités de mesure...)
         *
         * @note Les identifiants de locale suivent la convention POSIX ("fr_FR", "en_US")
         *       ou le standard BCP-47 ("fr-FR", "en-US") selon la plateforme.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkSystemLocaleEvent final : public NkSystemEvent {
            public:
                /// @brief Associe le type d'événement NK_SYSTEM_LOCALE à cette classe
                NK_EVENT_TYPE_FLAGS(NK_SYSTEM_LOCALE)

                /**
                 * @brief Constructeur avec les informations de changement de locale
                 * @param locale     Nouvelle locale active (ex: "fr_FR", "en-US")
                 * @param prevLocale Locale précédente (chaîne vide si inconnue ou premier démarrage)
                 * @param windowId   Identifiant de la fenêtre source (0 = événement global OS)
                 */
                NkSystemLocaleEvent(
                    const char* locale,
                    const char* prevLocale = "",
                    uint64 windowId = 0
                ) noexcept
                    : NkSystemEvent(windowId) {
                    ::strncpy(mLocale, locale, sizeof(mLocale) - 1);
                    ::strncpy(mPrevLocale, prevLocale, sizeof(mPrevLocale) - 1);
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkSystemLocaleEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant la transition de locale
                NkString ToString() const override {
                    return NkString("SystemLocale(") + mPrevLocale + " -> " + mLocale + ")";
                }

                /// @brief Retourne la nouvelle locale active
                /// @return Pointeur vers une chaîne C statique (ne pas libérer)
                NKENTSEU_INLINE const char* GetLocale() const noexcept {
                    return mLocale;
                }

                /// @brief Retourne la locale précédente avant le changement
                /// @return Pointeur vers une chaîne C statique (ne pas libérer)
                NKENTSEU_INLINE const char* GetPrevLocale() const noexcept {
                    return mPrevLocale;
                }

            private:
                char mLocale[48];       ///< Nouvelle locale active (buffer statique)
                char mPrevLocale[48];   ///< Locale précédente (buffer statique)
        };

        // =====================================================================
        // NkSystemDisplayEvent — Événement de changement de configuration d'affichage
        // =====================================================================

        /**
         * @brief Émis lors d'une modification de la configuration des moniteurs
         *
         * Cet événement couvre plusieurs scénarios :
         *   - Branchement/débranchement d'un moniteur externe (hotplug)
         *   - Changement de résolution ou de fréquence de rafraîchissement
         *   - Modification du facteur d'échelle DPI (multi-écran, HiDPI)
         *   - Changement d'orientation (tablettes, écrans pivotants)
         *   - Réaffectation du moniteur principal
         *
         * @note L'application doit recalculer ses layouts et adapter son rendu
         *       en réponse à cet événement pour une expérience utilisateur optimale.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkSystemDisplayEvent final : public NkSystemEvent {
            public:
                /// @brief Associe le type d'événement NK_SYSTEM_DISPLAY à cette classe
                NK_EVENT_TYPE_FLAGS(NK_SYSTEM_DISPLAY)

                /**
                 * @brief Constructeur avec les détails du changement d'affichage
                 * @param change   Type de modification survenue (ajout, résolution, DPI...)
                 * @param info     Structure descriptive du moniteur concerné
                 * @param windowId Identifiant de la fenêtre source (0 = événement global OS)
                 */
                NkSystemDisplayEvent(
                    NkDisplayChange change,
                    const NkDisplayInfo& info,
                    uint64 windowId = 0
                ) noexcept
                    : NkSystemEvent(windowId)
                    , mChange(change)
                    , mInfo(info) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkSystemDisplayEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le type de changement et les caractéristiques du moniteur
                NkString ToString() const override {
                    const char* changes[] = {
                        "Added", "Removed", "Resolution", "Orientation", "DPI", "Primary"
                    };
                    uint32 ci = static_cast<uint32>(mChange);
                    return NkString::Fmt(
                        "SystemDisplay(#{0} {1} {2}x{3})",
                        mInfo.index,
                        (ci < 6 ? changes[ci] : "?"),
                        mInfo.width,
                        mInfo.height
                    );
                }

                /// @brief Retourne le type de changement d'affichage survenu
                /// @return Valeur NkDisplayChange identifiant la nature de la modification
                NKENTSEU_INLINE NkDisplayChange GetChange() const noexcept {
                    return mChange;
                }

                /// @brief Retourne les informations détaillées du moniteur concerné
                /// @return Référence constante vers la structure NkDisplayInfo
                NKENTSEU_INLINE const NkDisplayInfo& GetInfo() const noexcept {
                    return mInfo;
                }

                /// @brief Indique si un moniteur a été ajouté (hotplug)
                /// @return true si le changement est de type NK_DISPLAY_ADDED
                NKENTSEU_INLINE bool IsAdded() const noexcept {
                    return mChange == NkDisplayChange::NK_DISPLAY_ADDED;
                }

                /// @brief Indique si un moniteur a été retiré (hot-unplug)
                /// @return true si le changement est de type NK_DISPLAY_REMOVED
                NKENTSEU_INLINE bool IsRemoved() const noexcept {
                    return mChange == NkDisplayChange::NK_DISPLAY_REMOVED;
                }

                /// @brief Indique si la résolution ou la fréquence a changé
                /// @return true si le changement est de type NK_DISPLAY_RESOLUTION_CHANGED
                NKENTSEU_INLINE bool IsResolutionChange() const noexcept {
                    return mChange == NkDisplayChange::NK_DISPLAY_RESOLUTION_CHANGED;
                }

                /// @brief Indique si le facteur d'échelle DPI a changé
                /// @return true si le changement est de type NK_DISPLAY_DPI_CHANGED
                NKENTSEU_INLINE bool IsDpiChange() const noexcept {
                    return mChange == NkDisplayChange::NK_DISPLAY_DPI_CHANGED;
                }

            private:
                NkDisplayChange mChange;  ///< Type de modification d'affichage
                NkDisplayInfo mInfo;      ///< Caractéristiques du moniteur concerné
        };

        // =====================================================================
        // NkSystemMemoryEvent — Événement de pression mémoire RAM
        // =====================================================================

        /**
         * @brief Émis lorsque le système signale une pression sur la mémoire RAM
         *
         * Cet événement est particulièrement critique sur :
         *   - iOS : memory warning avant termination par le système
         *   - Android : callbacks onLowMemory() et onTrimMemory()
         *   - Systèmes embarqués : ressources mémoire limitées et partagées
         *
         * Actions recommandées en réponse :
         *   - NK_MEM_MODERATE : libérer les caches d'assets non visibles, LODs élevés
         *   - NK_MEM_CRITICAL : libérer TOUT le non-essentiel, réduire la qualité graphique
         *
         * @note L'application DOIT répondre rapidement à cet événement sous peine
         *       d'être terminée par le système d'exploitation en cas de pénurie.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkSystemMemoryEvent final : public NkSystemEvent {
            public:
                /// @brief Associe le type d'événement NK_SYSTEM_MEMORY à cette classe
                NK_EVENT_TYPE_FLAGS(NK_SYSTEM_MEMORY)

                /**
                 * @brief Constructeur avec les informations de pression mémoire
                 * @param pressure       Niveau de pression détecté par le système
                 * @param availableBytes Mémoire disponible en octets (0 si inconnu)
                 * @param totalBytes     Mémoire totale du système en octets
                 * @param windowId       Identifiant de la fenêtre source (0 = événement global OS)
                 */
                NkSystemMemoryEvent(
                    NkMemoryPressure pressure,
                    uint64 availableBytes = 0,
                    uint64 totalBytes = 0,
                    uint64 windowId = 0
                ) noexcept
                    : NkSystemEvent(windowId)
                    , mPressure(pressure)
                    , mAvailableBytes(availableBytes)
                    , mTotalBytes(totalBytes) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkSystemMemoryEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le niveau de pression et la mémoire disponible
                NkString ToString() const override {
                    return NkString::Fmt(
                        "SystemMemory({0}{1})",
                        NkMemoryPressureToString(mPressure),
                        (mAvailableBytes > 0
                            ? NkString::Fmt(" avail={0}MB", mAvailableBytes / (1024 * 1024))
                            : "")
                    );
                }

                /// @brief Retourne le niveau de pression mémoire détecté
                /// @return Valeur NkMemoryPressure indiquant la sévérité de la situation
                NKENTSEU_INLINE NkMemoryPressure GetPressure() const noexcept {
                    return mPressure;
                }

                /// @brief Retourne la quantité de mémoire disponible en octets
                /// @return uint64 représentant les octets libres (0 si inconnu)
                NKENTSEU_INLINE uint64 GetAvailableBytes() const noexcept {
                    return mAvailableBytes;
                }

                /// @brief Retourne la mémoire totale du système en octets
                /// @return uint64 représentant la capacité RAM totale
                NKENTSEU_INLINE uint64 GetTotalBytes() const noexcept {
                    return mTotalBytes;
                }

                /// @brief Indique si la pression mémoire est au niveau critique
                /// @return true si GetPressure() == NK_MEM_CRITICAL
                NKENTSEU_INLINE bool IsCritical() const noexcept {
                    return mPressure == NkMemoryPressure::NK_MEM_CRITICAL;
                }

                /// @brief Retourne la mémoire disponible convertie en mégaoctets
                /// @return uint64 représentant les Mo libres (0 si inconnu)
                /// @note Fonction utilitaire pour affichage et logging humain
                NKENTSEU_INLINE uint64 GetAvailableMb() const noexcept {
                    return mAvailableBytes / (1024ULL * 1024ULL);
                }

            private:
                NkMemoryPressure mPressure;       ///< Niveau de pression mémoire détecté
                uint64 mAvailableBytes;           ///< Mémoire disponible en octets
                uint64 mTotalBytes;               ///< Mémoire totale du système en octets
        };

        // =====================================================================
        // NkSystemTimeZoneEvent — Événement de changement de fuseau horaire
        // =====================================================================

        /**
         * @brief Émis lorsque le fuseau horaire du système change
         *
         * Cet événement peut survenir dans les cas suivants :
         *   - Voyage de l'utilisateur avec changement de région
         *   - Modification manuelle des paramètres système
         *   - Synchronisation automatique via réseau (NTP, géolocalisation)
         *
         * Impact sur l'application :
         *   - Mise à jour de l'affichage des heures et dates
         *   - Ajustement des planifications et rappels basés sur l'heure locale
         *   - Recalcul des timestamps pour la cohérence des logs et synchronisation
         *
         * @note Les identifiants de fuseau suivent la base de données IANA
         *       (ex: "Europe/Paris", "America/New_York", "Asia/Tokyo", "UTC").
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkSystemTimeZoneEvent final : public NkSystemEvent {
            public:
                /// @brief Associe le type d'événement (proxy via NK_SYSTEM_LOCALE)
                /// @note Pas de type NK_SYSTEM_TIMEZONE dédié dans NkEventType
                NK_EVENT_TYPE_FLAGS(NK_SYSTEM_LOCALE)

                /**
                 * @brief Constructeur avec les informations de changement de fuseau
                 * @param tzId      Identifiant IANA du nouveau fuseau (ex: "Europe/Paris")
                 * @param prevTzId  Identifiant IANA du fuseau précédent (vide si inconnu)
                 * @param offsetMin Décalage par rapport à UTC en minutes (ex: +60 pour UTC+1)
                 * @param windowId  Identifiant de la fenêtre source (0 = événement global OS)
                 */
                NkSystemTimeZoneEvent(
                    const char* tzId,
                    const char* prevTzId = "",
                    int32 offsetMin = 0,
                    uint64 windowId = 0
                ) noexcept
                    : NkSystemEvent(windowId)
                    , mOffsetMin(offsetMin) {
                    ::strncpy(mTzId, tzId, sizeof(mTzId) - 1);
                    ::strncpy(mPrevTzId, prevTzId, sizeof(mPrevTzId) - 1);
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkSystemTimeZoneEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant la transition de fuseau horaire
                NkString ToString() const override {
                    return NkString::Fmt(
                        "SystemTimeZone({0} -> {1} UTC{2}{3})",
                        mPrevTzId,
                        mTzId,
                        (mOffsetMin >= 0 ? "+" : ""),
                        mOffsetMin / 60
                    );
                }

                /// @brief Retourne l'identifiant du nouveau fuseau horaire
                /// @return Pointeur vers une chaîne C statique (ne pas libérer)
                NKENTSEU_INLINE const char* GetTimeZoneId() const noexcept {
                    return mTzId;
                }

                /// @brief Retourne l'identifiant du fuseau horaire précédent
                /// @return Pointeur vers une chaîne C statique (ne pas libérer)
                NKENTSEU_INLINE const char* GetPrevTimeZoneId() const noexcept {
                    return mPrevTzId;
                }

                /// @brief Retourne le décalage par rapport à UTC en minutes
                /// @return int32 représentant le offset (ex: +60 pour UTC+1, -300 pour UTC-5)
                NKENTSEU_INLINE int32 GetOffsetMinutes() const noexcept {
                    return mOffsetMin;
                }

            private:
                char mTzId[64];         ///< Identifiant IANA du fuseau actuel (buffer statique)
                char mPrevTzId[64];     ///< Identifiant IANA du fuseau précédent (buffer statique)
                int32 mOffsetMin;       ///< Décalage UTC en minutes
        };

        // =====================================================================
        // NkSystemThemeEvent — Événement de changement de thème système OS
        // =====================================================================

        /**
         * @brief Émis lorsque le thème d'interface du système d'exploitation change
         *
         * Cet événement permet à l'application de s'adapter dynamiquement aux
         * préférences de l'utilisateur pour une expérience cohérente :
         *   - Basculer entre thèmes clair et sombre (Dark Mode)
         *   - Activer le mode contraste élevé pour l'accessibilité
         *   - Adapter les couleurs d'accentuation aux préférences système
         *
         * Conformité aux guidelines :
         *   - macOS : NSAppearanceDidChangeNotification
         *   - Windows : WM_SETTINGCHANGE avec "ImmersiveColorSet"
         *   - iOS : traitCollectionDidChange avec userInterfaceStyle
         *   - Android : UiModeManager.MODE_NIGHT_*
         *
         * @note Pour les changements de thème au niveau d'une fenêtre individuelle,
         *       voir NkWindowThemeChangeEvent dans NkWindowEvents.h.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkSystemThemeEvent final : public NkSystemEvent {
            public:
                /// @brief Associe le type d'événement (proxy via NK_SYSTEM_DISPLAY)
                /// @note Pas de type NK_SYSTEM_THEME dédié dans NkEventType
                NK_EVENT_TYPE_FLAGS(NK_SYSTEM_DISPLAY)

                /// @brief Énumération des thèmes d'interface système supportés
                enum class Theme : uint32 {
                    Light = 0,          ///< Thème clair (fond blanc, texte sombre)
                    Dark,               ///< Thème sombre (fond noir, texte clair)
                    HighContrast        ///< Thème contraste élevé (accessibilité)
                };

                /**
                 * @brief Constructeur avec les informations de changement de thème
                 * @param theme     Nouveau thème système actif
                 * @param prevTheme Thème système précédent
                 * @param accentR   Composante rouge de la couleur d'accentuation OS [0-255]
                 * @param accentG   Composante verte de la couleur d'accentuation OS [0-255]
                 * @param accentB   Composante bleue de la couleur d'accentuation OS [0-255]
                 * @param windowId  Identifiant de la fenêtre source (0 = événement global OS)
                 */
                NkSystemThemeEvent(
                    Theme theme,
                    Theme prevTheme = Theme::Light,
                    uint8 accentR = 0,
                    uint8 accentG = 120,
                    uint8 accentB = 215,
                    uint64 windowId = 0
                ) noexcept
                    : NkSystemEvent(windowId)
                    , mTheme(theme)
                    , mPrevTheme(prevTheme)
                    , mAccentR(accentR)
                    , mAccentG(accentG)
                    , mAccentB(accentB) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkSystemThemeEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le thème actif
                NkString ToString() const override {
                    const char* names[] = { "Light", "Dark", "HighContrast" };
                    uint32 ti = static_cast<uint32>(mTheme);
                    return NkString("SystemTheme(") + (ti < 3 ? names[ti] : "?") + ")";
                }

                /// @brief Retourne le thème système actuellement actif
                /// @return Valeur Theme indiquant le mode d'interface
                NKENTSEU_INLINE Theme GetTheme() const noexcept {
                    return mTheme;
                }

                /// @brief Retourne le thème système précédent avant le changement
                /// @return Valeur Theme indiquant l'ancien mode d'interface
                NKENTSEU_INLINE Theme GetPrevTheme() const noexcept {
                    return mPrevTheme;
                }

                /// @brief Retourne la composante rouge de la couleur d'accentuation
                /// @return uint8 dans [0, 255] représentant l'intensité rouge
                NKENTSEU_INLINE uint8 GetAccentR() const noexcept {
                    return mAccentR;
                }

                /// @brief Retourne la composante verte de la couleur d'accentuation
                /// @return uint8 dans [0, 255] représentant l'intensité verte
                NKENTSEU_INLINE uint8 GetAccentG() const noexcept {
                    return mAccentG;
                }

                /// @brief Retourne la composante bleue de la couleur d'accentuation
                /// @return uint8 dans [0, 255] représentant l'intensité bleue
                NKENTSEU_INLINE uint8 GetAccentB() const noexcept {
                    return mAccentB;
                }

                /// @brief Indique si le thème sombre est actuellement actif
                /// @return true si GetTheme() == Theme::Dark
                NKENTSEU_INLINE bool IsDark() const noexcept {
                    return mTheme == Theme::Dark;
                }

                /// @brief Indique si le thème clair est actuellement actif
                /// @return true si GetTheme() == Theme::Light
                NKENTSEU_INLINE bool IsLight() const noexcept {
                    return mTheme == Theme::Light;
                }

                /// @brief Indique si le mode contraste élevé est actif
                /// @return true si GetTheme() == Theme::HighContrast
                NKENTSEU_INLINE bool IsHighContrast() const noexcept {
                    return mTheme == Theme::HighContrast;
                }

            private:
                Theme mTheme;           ///< Thème système actuellement actif
                Theme mPrevTheme;       ///< Thème système précédent
                uint8 mAccentR;         ///< Composante rouge de la couleur d'accentuation
                uint8 mAccentG;         ///< Composante verte de la couleur d'accentuation
                uint8 mAccentB;         ///< Composante bleue de la couleur d'accentuation
        };

        // =====================================================================
        // NkSystemAccessibilityEvent — Événement de changement d'accessibilité OS
        // =====================================================================

        /**
         * @brief Émis lorsqu'un paramètre d'accessibilité du système change
         *
         * Cet événement permet à l'application de s'adapter aux besoins spécifiques
         * de l'utilisateur pour une expérience inclusive et conforme aux standards :
         *   - Réduction des animations pour les utilisateurs sensibles au mouvement
         *   - Augmentation du contraste pour les déficiences visuelles
         *   - Inversion des couleurs pour certains modes d'accessibilité
         *   - Texte en gras et taille de police augmentée pour la lisibilité
         *
         * Bonnes pratiques de réponse :
         *   - Respecter reduceMotion : désactiver les transitions et effets de parallaxe
         *   - Respecter increaseContrast : utiliser des couleurs à fort contraste
         *   - Respecter fontScale : adapter les tailles de police et les layouts
         *
         * Conformité aux guidelines :
         *   - macOS : NSAccessibilityReduceMotion, NSAccessibilityIncreaseContrast
         *   - iOS : UIAccessibilityIsReduceMotionEnabled, dynamicType
         *   - Android : AccessibilityManager, Configuration.fontScale
         *   - Windows : HighContrast settings, text scaling preferences
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkSystemAccessibilityEvent final : public NkSystemEvent {
            public:
                /// @brief Associe le type d'événement (proxy via NK_SYSTEM_DISPLAY)
                /// @note Pas de type NK_SYSTEM_ACCESSIBILITY dédié dans NkEventType
                NK_EVENT_TYPE_FLAGS(NK_SYSTEM_DISPLAY)

                /**
                 * @brief Constructeur avec les paramètres d'accessibilité
                 * @param reduceMotion     true si "Réduire les animations" est activé
                 * @param increaseContrast true si "Augmenter le contraste" est activé
                 * @param invertColors     true si "Inverser les couleurs" est activé
                 * @param boldText         true si "Texte en gras" est activé
                 * @param largeText        true si la taille de police est augmentée
                 * @param fontScale        Facteur d'échelle de la police (1.0 = taille normale)
                 * @param windowId         Identifiant de la fenêtre source (0 = événement global OS)
                 */
                NkSystemAccessibilityEvent(
                    bool reduceMotion = false,
                    bool increaseContrast = false,
                    bool invertColors = false,
                    bool boldText = false,
                    bool largeText = false,
                    float32 fontScale = 1.f,
                    uint64 windowId = 0
                ) noexcept
                    : NkSystemEvent(windowId)
                    , mReduceMotion(reduceMotion)
                    , mIncreaseContrast(increaseContrast)
                    , mInvertColors(invertColors)
                    , mBoldText(boldText)
                    , mLargeText(largeText)
                    , mFontScale(fontScale) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkSystemAccessibilityEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant les paramètres d'accessibilité actifs
                NkString ToString() const override {
                    return NkString::Fmt(
                        "SystemAccessibility({0}{1}{2}fontScale={3:.3})",
                        (mReduceMotion ? "reduceMotion " : ""),
                        (mIncreaseContrast ? "highContrast " : ""),
                        (mInvertColors ? "invertColors " : ""),
                        mFontScale
                    );
                }

                /// @brief Indique si la réduction des animations est activée
                /// @return true si l'option "Réduire les animations" est active
                NKENTSEU_INLINE bool ReduceMotion() const noexcept {
                    return mReduceMotion;
                }

                /// @brief Indique si l'augmentation du contraste est activée
                /// @return true si l'option "Augmenter le contraste" est active
                NKENTSEU_INLINE bool IncreaseContrast() const noexcept {
                    return mIncreaseContrast;
                }

                /// @brief Indique si l'inversion des couleurs est activée
                /// @return true si l'option "Inverser les couleurs" est active
                NKENTSEU_INLINE bool InvertColors() const noexcept {
                    return mInvertColors;
                }

                /// @brief Indique si le texte en gras est activé
                /// @return true si l'option "Texte en gras" est active
                NKENTSEU_INLINE bool BoldText() const noexcept {
                    return mBoldText;
                }

                /// @brief Indique si la taille de police augmentée est activée
                /// @return true si l'option "Grand texte" est active
                NKENTSEU_INLINE bool LargeText() const noexcept {
                    return mLargeText;
                }

                /// @brief Retourne le facteur d'échelle de la police système
                /// @return float32 représentant le multiplicateur de taille (1.0 = normal)
                NKENTSEU_INLINE float32 GetFontScale() const noexcept {
                    return mFontScale;
                }

            private:
                bool mReduceMotion;     ///< Flag : réduction des animations activée
                bool mIncreaseContrast; ///< Flag : contraste élevé activé
                bool mInvertColors;     ///< Flag : inversion des couleurs activée
                bool mBoldText;         ///< Flag : texte en gras activé
                bool mLargeText;        ///< Flag : taille de police augmentée activée
                float32 mFontScale;     ///< Facteur d'échelle de la police système
        };

    } // namespace nkentseu

#endif // NKENTSEU_EVENT_NKSYSTEMEVENT_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKSYSTEMEVENT.H
// =============================================================================
// Ce fichier définit la hiérarchie des événements système pour le module NKEvent.
// Les exemples ci-dessous illustrent les patterns d'usage recommandés.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Gestion des transitions d'alimentation (sauvegarde avant veille)
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkSystemEvent.h"

    void HandleSystemEvent(nkentseu::NkEvent& event) {
        using namespace nkentseu;

        // Détection et traitement des événements d'alimentation
        if (auto* powerEvt = event.As<NkSystemPowerEvent>()) {
            if (powerEvt->IsSuspending()) {
                // Sauvegarde urgente avant suspension OS
                GameState::GetInstance().SaveQuickSave();
                Renderer::GetInstance().FlushAndReleaseVRAM();
                event.MarkHandled();
            }
            else if (powerEvt->GetState() == NkPowerState::NK_POWER_LOW_BATTERY) {
                // Avertissement utilisateur et activation mode économie
                UIManager::ShowBatteryWarning();
                Settings::EnablePowerSavingMode(true);
            }
            else if (powerEvt->IsResuming()) {
                // Restauration après réveil
                Renderer::GetInstance().RecreateSwapchain();
                Audio::GetInstance().ResumeStreams();
            }
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Adaptation dynamique au changement de configuration d'affichage
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkSystemEvent.h"

    class DisplayManager {
    public:
        void OnDisplayChanged(const nkentseu::NkSystemDisplayEvent& evt) {
            using namespace nkentseu;

            const auto& info = evt.GetInfo();

            if (evt.IsAdded()) {
                // Nouveau moniteur : étendre l'interface ou proposer un mode étendu
                Window::CreateOnDisplay(info.index, info.width, info.height);
            }
            else if (evt.IsRemoved()) {
                // Moniteur retiré : migrer les fenêtres vers l'écran restant
                Window::MigrateFromDisplay(info.index);
            }
            else if (evt.IsDpiChange() || evt.IsResolutionChange()) {
                // Changement de DPI ou résolution : recalcul des layouts
                float scale = info.dpiScale;
                UIManager::SetGlobalScale(scale);
                Renderer::UpdateViewport(info.width, info.height);
            }

            // Logging pour débogage
            NK_FOUNDATION_LOG_INFO("Display config updated: {}", evt.ToString());
        }
    };
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Gestion de la pression mémoire sur mobile (iOS/Android)
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkSystemEvent.h"

    class ResourceManager {
    public:
        void OnMemoryWarning(const nkentseu::NkSystemMemoryEvent& evt) {
            using namespace nkentseu;

            switch (evt.GetPressure()) {
                case NkMemoryPressure::NK_MEM_MODERATE:
                    // Libérer les caches non critiques
                    TextureCache::EvictUnused();
                    Audio::UnloadBackgroundTracks();
                    break;

                case NkMemoryPressure::NK_MEM_CRITICAL:
                    // Libération agressive : survie de l'application en jeu
                    TextureCache::ClearAll();
                    MeshPool::ReleaseIdle();
                    ScriptVM::CollectGarbage();
                    Renderer::ReduceQuality(RenderQuality::Low);
                    break;

                case NkMemoryPressure::NK_MEM_NORMAL:
                default:
                    // Aucune action requise
                    break;
            }

            NK_FOUNDATION_LOG_INFO(
                "Memory handled: {} ({} MB available)",
                NkMemoryPressureToString(evt.GetPressure()),
                evt.GetAvailableMb()
            );
        }
    };
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Mise à jour de la localisation en temps réel
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkSystemEvent.h"

    class LocalizationManager {
    public:
        void OnLocaleChanged(const nkentseu::NkSystemLocaleEvent& evt) {
            const char* newLocale = evt.GetLocale();

            // Chargement asynchrone des nouvelles ressources de traduction
            LoadLocaleBundleAsync(newLocale, [this](bool success) {
                if (success) {
                    // Mise à jour de toute l'interface
                    UIManager::RefreshAllTexts();
                    DateFormat::UpdatePattern(newLocale);
                    NK_FOUNDATION_LOG_INFO("Locale updated to {}", newLocale);
                }
                else {
                    // Fallback sur la locale précédente ou par défaut
                    NK_FOUNDATION_LOG_WARNING("Failed to load locale {}, keeping {}",
                        newLocale, evt.GetPrevLocale());
                }
            });
        }
    };
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Adaptation au thème sombre/clair et accessibilité
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkSystemEvent.h"

    class ThemeAdapter {
    public:
        void OnThemeChanged(const nkentseu::NkSystemThemeEvent& evt) {
            using namespace nkentseu;

            // Application du thème à l'interface
            if (evt.IsDark()) {
                Style::ApplyDarkTheme();
            }
            else if (evt.IsLight()) {
                Style::ApplyLightTheme();
            }
            else if (evt.IsHighContrast()) {
                Style::ApplyHighContrastTheme();
            }

            // Mise à jour de la couleur d'accentuation système
            Style::SetAccentColor(evt.GetAccentR(), evt.GetAccentG(), evt.GetAccentB());
        }

        void OnAccessibilityChanged(const nkentseu::NkSystemAccessibilityEvent& evt) {
            using namespace nkentseu;

            // Respect des préférences d'accessibilité
            Animation::SetEnabled(!evt.ReduceMotion());
            Contrast::SetHigh(evt.IncreaseContrast());
            Colors::SetInverted(evt.InvertColors());
            Typography::SetBold(evt.BoldText());
            Typography::SetScale(evt.GetFontScale());

            // Re-layout de l'interface pour adapter aux nouvelles tailles
            UIManager::RecomputeLayouts();
        }
    };
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Filtrage combiné par type et catégorie dans un dispatcher
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkSystemEvent.h"
    #include "NKEvent/EventManager.h"

    void RegisterSystemHandlers(nkentseu::event::EventManager& mgr) {
        using namespace nkentseu;

        // Handler pour tous les événements système (filtrage par catégorie)
        mgr.Subscribe(
            [](NkEvent& evt) {
                if (evt.HasCategory(NkEventCategory::NK_CAT_SYSTEM)) {
                    NK_FOUNDATION_LOG_DEBUG("System event: {}", evt.ToString());
                }
            }
        );

        // Handler spécifique pour les événements d'alimentation
        mgr.Subscribe<NkSystemPowerEvent>(
            [](NkSystemPowerEvent& evt) {
                if (evt.IsSuspending()) {
                    Application::RequestGracefulShutdown();
                }
            }
        );

        // Handler pour les changements de mémoire (critique sur mobile)
        mgr.Subscribe<NkSystemMemoryEvent>(
            [](NkSystemMemoryEvent& evt) {
                if (evt.IsCritical()) {
                    ResourceManager::EmergencyCleanup();
                }
            }
        );
    }
*/

// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
    1. AJOUT D'UN NOUVEL ÉVÉNEMENT SYSTÈME :
       a) Ajouter un type dans NkEventType::Value si nécessaire (groupe System)
       b) Créer une classe dérivée de NkSystemEvent avec les macros NK_EVENT_*
       c) Implémenter Clone() et ToString() pour cohérence avec l'infrastructure
       d) Documenter les cas d'usage et les actions recommandées en réponse

    2. COMPATIBILITÉ MULTIPLATEFORME :
       - Les événements système sont émis par la couche plateforme (NKPlatform)
       - Vérifier la disponibilité des APIs sur chaque OS avant d'émettre un événement
       - Utiliser des valeurs par défaut sûres quand une information est indisponible

    3. PERFORMANCE DES ACCESSEURS :
       - Tous les getters simples sont inline pour éviter l'overhead d'appel
       - Les buffers de chaînes (char[]) évitent les allocations dynamiques
       - Les méthodes de prédicat (IsDark(), IsCritical()) sont noexcept et inline

    4. GESTION DE MÉMOIRE :
       - Clone() retourne un pointeur brut : utiliser std::unique_ptr en appelant
       - Les buffers statiques (char name[64]) limitent les allocations heap
       - Éviter les std::string dans les membres pour les événements fréquents

    5. THREAD-SAFETY :
       - Les événements système sont typiquement émis sur le thread principal
       - Pour un traitement asynchrone, copier les données nécessaires avant dispatch
       - Ne jamais modifier un événement partagé entre threads sans synchronisation

    6. ÉVOLUTION DES ENUMS :
       - Toujours ajouter les nouvelles valeurs avant la sentinelle *_MAX
       - Ne jamais réordonner les valeurs existantes (compatibilité binaire)
       - Mettre à jour les fonctions ToString() pour couvrir les nouveaux cas
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================